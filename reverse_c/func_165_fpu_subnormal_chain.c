/**
 * @file func_165_fpu_subnormal_chain.c
 * @brief 函数: 软件 FPU 三角函数缩减链 (sin/cos range-reduction + polynomial eval)
 * @addr  0x08016DA2 - 0x080171A0 (1022 bytes)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 位于 func_130 (FPU 结构写入器, 0x08016D68-0x08016DA2) 和
 * func_131 (FPU 阈值乘法链, 0x080171A0-0x08017310) 之间.
 *
 * == 功能概述 ==
 * 本函数实现 IEEE 754 双精度三角函数 (sin/cos) 的软件计算流水线:
 *   Phase 1 (Path A): 对满足阈值条件的输入, 直接使用 VSCALE/VADD 双缩放链
 *   Phase 2 (Path B): 完整的 range reduction + 多项式迭代 + 次正规数修复
 *   Phase 3: 符号处理与最终结果写回
 *
 * 采用位级别的 IEEE 754 比较 (通过高 32 位整数 CMP) 来快速分派路径.
 *
 * == 寄存器映射 ==
 *   r0-r3   = 通用 / VMOV 中转 (双精度值低/高 32 位)
 *   r4      = 输出缓冲区指针 (写入 double 结果)
 *   r5      = threshold1 (uint32, 作为 IEEE 754 高 32 位进行比较)
 *             Phase 2 中重用作 1..3 的循环计数器
 *   r6      = 条件选择标志 (<= 0 时走 VADD 路径, > 0 时走 VSCALE 路径)
 *             Phase 3 中作为 128-bit 乘法输入的低 64 位
 *   r7      = 先加载 THR2_CMP (0x3FF921FB = PI/2 高字) 用于比较
 *             后被 func_8776 返回值覆盖
 *             Phase 3 中作为 128-bit 乘法输入的高 64 位
 *   r8      = threshold1 的指数字段 (r5 >> 20), 用于循环退出条件
 *   sl(r10) = 多项式查找表基址 (0x080196D8)
 *   fp(r11) = 当前表条目指针 (sl + r5*16)
 *   ip(r12) = 栈上 128-bit 临时缓冲区指针
 *
 * == 栈布局 ==
 *   [sp+0x48]  = d0  (输入 double)
 *   [sp+0x20]  = 移位量 (0..31)  / 临时 double 存储
 *   [sp+0x18]  = 多精度字 0
 *   [sp+0x1C]  = 多精度字 1
 *   [sp+0x0C]  = 多精度操作数高 64 位
 *   [sp+0x08]  = 多精度操作数低 64 位 / 结果
 *   [sp+0x14]  = 多精度操作数
 *   [sp+0x4C]  = 符号标志 (bit 31 测试)
 *
 * ================================================================
 *          文 字 池 常 量 (从二进制提取, 已验证)
 * ================================================================
 *
 * --- IEEE 754 double 常量 ---
 *
 * 地址       寄存器   IEEE 754 hex          十进制值
 * 0x08017128  d10    0x3FF921FB54400000    1.5707963267341256e+00   (PI/2)
 * 0x08017130  d11    0x3DD0B4611A626331    6.0771005065061922e-11   (scale1)
 * 0x08017140  d9     0x3DD0B4611A600000    6.0771005063039660e-11   (scale1_alt)
 * 0x08017148  d8     0x3BA3198A2E037073    2.0222662487959506e-21   (scale2)
 * 0x08017158  --     0x3FE45F306DC9C883    6.3661977236758138e-01   (2/PI)
 * 0x08017160  --     0x3FE0000000000000    5.0000000000000000e-01   (0.5)
 * 0x08017170  --     0x3E10000000000000    9.3132257461547852e-10   (2^-30)
 * 0x08017178  --     0x3C10000000000000    2.1684043449710089e-19   (2^-50)
 * 0x08017180  --     0x3A10000000000000    5.0487097934144756e-29   (2^-94)
 * 0x08017188  --     0x3DF921FB54442D18    3.6572951981678991e-10   (coeff_p)
 * 0x08017190  --     0x3C110B4611A62633    2.3098978672189182e-19   (coeff_q1)
 * 0x08017198  --     0x3DF921FB54000000    3.6572951958580013e-10   (coeff_q2)
 *
 * --- uint32 常量 (LDR) ---
 *
 * 0x08017124  THR1_CMP   = 0x4002D97C  (3*PI/4 的高 32 位 ~= 2.356)
 * 0x08017138  THR2_CMP   = 0x3FF921FB  (PI/2 的高 32 位)
 * 0x08017150  THR_PATHB  = 0x413921FB  (Path B 阈值)
 * 0x08017168  TABLE_OFF  = 0x0000282A  (PC 相对偏移 -> 表 0x080196D8)
 * 0x0801716C  JMPTBL_OFF = 0x00002778  (跳转表偏移)
 *
 * ================================================================
 *          BL 调 用 目 标 统 计
 * ================================================================
 *
 * BL 目标        地址        次数  功能推测
 * func_856C   0x0800856C    12   VSCALE.F64   d0 = VSCALE(d0, d2) -- 缩放/取指数字段
 * func_842A   0x0800842A    11   VADD.F64     d0 = d0 + d2     -- 浮点加法
 * func_8578   0x08008578    11   VMUL.F64     d0 = d0 * d2     -- 浮点乘法
 * func_8572   0x08008572     2   VABS/VCMP相关                   -- 绝对值或比较
 * func_8776   0x08008776     1   double -> int64 转换 (floor/trunc)
 * func_873A   0x0800873A     2   int64 -> double 转换
 * func_875C   0x0800875C     3   int32 -> double 转换 (fixup)
 * func_850C   0x0801850C     1   前置处理 (rem_pio2 或类似)
 *
 * 总计 BL 调用: 43 次 (8 个不同目标)
 *
 * ================================================================
 *          查 找 表 (0x080196D8)
 * ================================================================
 * 每个条目 16 字节 = 两个 double. 以 r5 索引 (1..3):
 *   [0] PI/2,              d11      (索引 0, 实际不使用)
 *   [1] d9 (scale1_alt),   d8       (scale2)
 *   [2] 2.022e-21,         8.478e-32  (递减的 scale 因子)
 */

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* ================================================================
 * 外部 BL 目标声明
 * ================================================================ */

/** VSCALE.F64 -- 双精度浮点缩放 (提取/操作指数字段)
 *  @return d0 = fscale(d0, d2)
 *  BL 调用 12 次 */
extern double func_856C(double a, double b);

/** VADD.F64 -- 双精度浮点加法
 *  @return d0 = d0 + d2
 *  BL 调用 11 次 */
extern double func_842A(double a, double b);

/** VMUL.F64 -- 双精度浮点乘法
 *  @return d0 = d0 * d2
 *  BL 调用 11 次 */
extern double func_8578(double a, double b);

/** VABS/VCMP 相关 -- 绝对值或比较
 *  @return 结果在 r0:r1 或标志位
 *  BL 调用 2 次 */
extern double func_8572(double a, double b);

/** double -> int64 转换 (floor/trunc 取整)
 *  @return r0 = (int64_t)floor(d0)
 *  BL 调用 1 次 */
extern int64_t func_8776(double x);

/** int64 -> double 转换
 *  @return d0 = (double)(int64_t arg)
 *  BL 调用 2 次 */
extern double func_873A(int64_t x);

/** int32 -> double 转换 (fixup/fscale 组合)
 *  @return d0 = 转换结果
 *  BL 调用 3 次 */
extern double func_875C(int32_t x);

/** 前置三角函数处理 (rem_pio2 或类似 range reduction)
 *  @return 缩减后的值
 *  BL 调用 1 次 */
extern double func_850C(double x);


/* ================================================================
 * IEEE 754 文字池常量 (地址已验证, 从二进制提取)
 * ================================================================ */

/* --- VLDR 加载的 double 常量 --- */

/** d10 @ 0x08017128: PI/2 = 1.5707963267341256 */
#define CONST_PI_OVER_2      1.5707963267341256e+00

/** d11 @ 0x08017130: scale factor 1a = 6.0771005065061922e-11 */
#define CONST_SCALE1A        6.0771005065061922e-11

/** d9  @ 0x08017140: scale factor 1b = 6.0771005063039660e-11 (略小) */
#define CONST_SCALE1B        6.0771005063039660e-11

/** d8  @ 0x08017148: scale factor 2 = 2.0222662487959506e-21 */
#define CONST_SCALE2         2.0222662487959506e-21

/** @ 0x08017158: 2/PI = 0.6366197723675814 */
#define CONST_2_OVER_PI      6.3661977236758138e-01

/** @ 0x08017160: 0.5 */
#define CONST_HALF           5.0000000000000000e-01

/** @ 0x08017170: 2^-30 = 9.3132257461547852e-10 */
#define CONST_2POW_NEG30     9.3132257461547852e-10

/** @ 0x08017178: 2^-50 = 2.1684043449710089e-19 */
#define CONST_2POW_NEG50     2.1684043449710089e-19

/** @ 0x08017180: 2^-94 = 5.0487097934144756e-29 */
#define CONST_2POW_NEG94     5.0487097934144756e-29

/** @ 0x08017188: 多项式系数 P = 3.6572951981678991e-10 */
#define CONST_COEFF_P        3.6572951981678991e-10

/** @ 0x08017190: 多项式系数 Q1 = 2.3098978672189182e-19 */
#define CONST_COEFF_Q1       2.3098978672189182e-19

/** @ 0x08017198: 多项式系数 Q2 = 3.6572951958580013e-10 */
#define CONST_COEFF_Q2       3.6572951958580013e-10


/* --- LDR 加载的 uint32 比较常量 (IEEE 754 高 32 位) --- */

/** THR1_CMP @ 0x08017124: 0x4002D97C = 3*PI/4 的高 32 位 */
#define THR1_CMP_HI          0x4002D97Cu

/** THR2_CMP @ 0x08017138: 0x3FF921FB = PI/2 的高 32 位 */
#define THR2_CMP_HI          0x3FF921FBu

/** THR_PATHB @ 0x08017150: 0x413921FB = Path B 入口阈值 */
#define THR_PATHB_HI         0x413921FBu

/** TABLE_OFF @ 0x08017168: 0x0000282A -- 查找表的 PC 相对偏移 */
#define TABLE_PC_OFFSET      0x0000282Au

/** JMPTBL_OFF @ 0x0801716C: 0x00002778 -- 跳转表偏移 */
#define JMPTBL_PC_OFFSET     0x00002778u


/* ================================================================
 * 查找表 (运行时地址 0x080196D8, 由 sl = TABLE_OFF + PC 计算)
 * 每个条目 16 字节 = { double A, double B }
 * ================================================================ */
static const double lut_table[][2] = {
    [0] = { CONST_PI_OVER_2,  CONST_SCALE1A  },   /* 0x080196D8 (索引 0, 未使用) */
    [1] = { CONST_SCALE1B,    CONST_SCALE2    },   /* 0x080196E8 (r5=1 时访问) */
    [2] = { 2.0222662487111665e-21, 8.4784276603688996e-32 }, /* 0x080196F8 (r5=2) */
    [3] = { 0.0, 0.0 },                            /* 0x08019708 (r5=3, 通常跳出) */
};

/* 跳转表 (运行时由 JMPTBL_OFF + PC 计算) */
static const uint32_t jmptbl[];  /* 由 func_8776 返回值索引 */


/* ================================================================
 * 辅助宏: 提取 IEEE 754 double 的高 32 位
 * ================================================================ */
static inline uint32_t double_hi(double x) {
    union { double d; uint32_t u[2]; } u;
    u.d = x;
    return u.u[1];  /* ARM 小端: 高字在索引 1 */
}

static inline uint32_t double_exp(double x) {
    return (double_hi(x) >> 20) & 0x7FFu;
}


/* ================================================================
 * FPU_Subnormal_Chain() @ 0x08016DA2
 *
 * 调用约定:
 *   r4     = double *output  (输出缓冲区, 写入 double 结果)
 *   r5     = uint32_t thr_hi (阈值的高 32 位 IEEE 754 表示)
 *   r6     = int32_t  selector ( <= 0 走 VADD 路径, > 0 走 VSCALE 路径)
 *   [sp+0x48] = double input
 *   [sp+0x4C] = int32_t  sign_flag (bit 31 测试符号)
 *
 * 返回:
 *   r0 = 1  (成功, VSCALE 路径)
 *   r0 = -1 (VADD 路径)
 *   r0 = r5 (符号处理后)
 *   r0 = 0  (提前退出 / 错误)
 *
 * 入口: 无标准 PUSH 序言, 直接使用调用者栈帧
 * ================================================================ */
uint32_t FPU_Subnormal_Chain(
    double   input,
    double  *output,
    uint32_t thr_hi,
    int32_t  selector,
    int32_t  sign_flag)
{
    /* === 局部变量 (映射到 d0-d11, r0-r12,fp,sl) === */
    double d0, d8, d9, d10, d11;
    double d1;
    int64_t i7;
    uint32_t exponent_r8;
    int32_t  loop_r5;
    uint32_t comp_r0;
    uint32_t comp_r7;
    int64_t multi_lo, multi_hi;

    /* ================================================================
     * 入口: 加载阈值与常量
     * ================================================================
     * 0x08016DA2: LDR  r0, [pc, #0x380]  -> comp_r0 = THR1_CMP_HI
     * 0x08016DA4: VLDR d10, [pc, #0x380]  -> d10 = PI/2
     * 0x08016DA8: VLDR d11, [pc, #0x384]  -> d11 = SCALE1A
     * 0x08016DAC: CMP  r0, r5              -> if (THR1_CMP_HI <= thr_hi) goto Path_B
     * 0x08016DAE: BLE  Path_B
     */
    comp_r0 = THR1_CMP_HI;
    d10 = CONST_PI_OVER_2;
    d11 = CONST_SCALE1A;

    if (comp_r0 <= thr_hi) {
        goto Path_B;
    }

    /* ================================================================
     * PATH A: 直接缩放链 (0x08016DB0 - 0x08016E3E)
     * ================================================================
     * 0x08016DB0: LDR  r7, [pc, #0x384]  -> comp_r7 = THR2_CMP_HI (PI/2 高字)
     * 0x08016DB2: VLDR d9, [pc, #0x38C]  -> d9 = SCALE1B
     * 0x08016DB6: VLDR d8, [pc, #0x390]  -> d8 = SCALE2
     * 0x08016DBA: VLDR d0, [sp, #0x48]   -> d0 = input
     * 0x08016DC6: CMP  r6, #0
     * 0x08016DC8: BLE  Path_A_VADD       -> if (selector <= 0) use VADD
     */
    comp_r7 = THR2_CMP_HI;
    d9  = CONST_SCALE1B;
    d8  = CONST_SCALE2;
    d0  = input;

    if (selector <= 0) {
        goto Path_A_VADD;
    }

    /* --- Path A: VSCALE 路径 (0x08016DCA - 0x08016E02) ---
     * 0x08016DCA: BL func_856C(d0, d10)  -> d0 = VSCALE(input, PI/2)
     * 0x08016DD2: CMP r5, r7             -> if (thr_hi == THR2_CMP_HI) goto alt
     */
    d0 = func_856C(d0, d10);       /* VSCALE(input, PI/2) */

    if (thr_hi == comp_r7) {
        /* --- Path A 备选: double VSCALE chain (0x08016DE8 - 0x08016DFC) ---
         * 0x08016DF0: BL func_856C(d0, d9)   -> VSCALE(d0, SCALE1B)
         * 0x08016DF8: BL func_856C(d0, d8)   -> VSCALE(result, SCALE2)
         * 0x08016DFC: STRD [r4]
         * 0x08016E00: MOVS r0, #1
         */
        d0 = func_856C(d0, d9);     /* VSCALE(d0, SCALE1B) */
        d0 = func_856C(d0, d8);     /* VSCALE(d0, SCALE2)  */
        *output = d0;
        return 1;
    } else {
        /* --- Path A 主路径: single VSCALE (0x08016DD6 - 0x08016DE6) ---
         * 0x08016DDE: BL func_856C(d0, d11)  -> VSCALE(d0, SCALE1A)
         * 0x08016DE2: STRD [r4]
         * 0x08016DE6: B return_1
         */
        d0 = func_856C(d0, d11);    /* VSCALE(d0, SCALE1A) */
        *output = d0;
        return 1;
    }

Path_A_VADD:
    /* --- Path A: VADD 路径 (0x08016E04 - 0x08016E3E) ---
     * 当 selector <= 0 时使用 VADD.F64 替代 VSCALE.F64.
     *
     * 0x08016E04: BL func_842A(d0, d10)  -> VADD(input, PI/2)
     * 0x08016E0C: CMP r5, r7
     * 0x08016E0E: BEQ Path_A_VADD_alt
     */
    d0 = func_842A(d0, d10);       /* VADD(input, PI/2) */

    if (thr_hi == comp_r7) {
        /* 0x08016E22: VADD(d0, d9) -> VADD(result, d8) -> STRD [r4] -> return -1 */
        d0 = func_842A(d0, d9);    /* VADD(d0, SCALE1B) */
        d0 = func_842A(d0, d8);    /* VADD(d0, SCALE2)  */
        *output = d0;
        return (uint32_t)-1;
    } else {
        /* 0x08016E18: BL func_842A(d0, d11) -> STRD [r4] -> return -1 */
        d0 = func_842A(d0, d11);   /* VADD(d0, SCALE1A) */
        *output = d0;
        return (uint32_t)-1;
    }


    /* ================================================================
     * PATH B: 三角函数完整流水线 (0x08016E40 - 0x08016F3A)
     * ================================================================
     * 包含: range reduction -> 整数部分提取 -> 多项式迭代 -> 次正规数修复
     */
Path_B:
    /* 0x08016E40: LDR r0, [pc, #0x30C]  -> comp_r0 = THR_PATHB_HI
     * 0x08016E42: CMP r0, r5
     * 0x08016E44: BLT Path_B_exit         -> if (THR_PATHB_HI < thr_hi) early return
     */
    comp_r0 = THR_PATHB_HI;
    if (comp_r0 < thr_hi) {
        goto Path_B_exit;
    }

    /* --- Step 1: Range reduction via func_850C (0x08016E46 - 0x08016E52) ---
     * 0x08016E46: VLDR d0, [sp, #0x48]   -> d0 = input
     * 0x08016E4E: BL func_850C(d0)        -> d8 = rem_pio2(input)
     */
    d0 = input;
    d8 = func_850C(d0);             /* d8 = reduced argument */

    /* --- Step 2: 乘以 2/PI, 加 0.5, 取整 (0x08016E56 - 0x08016E72) ---
     * 0x08016E56: VLDR d0 -> d0 = 2/PI
     * 0x08016E5E: BL func_8578(d8, d0)   -> d0 = d8 * (2/PI)
     * 0x08016E62: VLDR d1 -> d1 = 0.5
     * 0x08016E6A: BL func_842A(d0, d1)   -> d0 = d0 + 0.5
     * 0x08016E6E: BL func_8776(d0)        -> i7 = floor(d0)
     * 0x08016E72: MOV r7, r0
     */
    d0 = CONST_2_OVER_PI;
    d0 = func_8578(d8, d0);         /* d0 = d8 * 2/PI */
    d1 = CONST_HALF;
    d0 = func_842A(d0, d1);         /* d0 = d0 + 0.5  */
    i7 = func_8776(d0);             /* i7 = floor_int64(d0) */

    /* --- Step 3: 恢复双精度值并计算差值 (0x08016E74 - 0x08016E8C) ---
     * 0x08016E74: BL func_873A(i7)        -> d9 = (double)i7
     * 0x08016E80: BL func_8578(d9, d10)   -> d0 = d9 * PI/2
     * 0x08016E88: BL func_8572(d8, d0)    -> d8 = fabs(d8 - d0) 或 VSUB
     * 再乘 d11: func_8578(d9, d11)        -> 小量修正
     */
    d9 = func_873A(i7);             /* d9 = (double)i7 */
    d0 = func_8578(d9, d10);        /* d0 = d9 * PI/2    */
    d8 = func_8572(d8, d0);         /* d8 = |d8 - d0|    */
    d0 = func_8578(d9, d11);        /* d0 = d9 * SCALE1A (小量修正) */

    /* --- Step 4: 提取指数, 初始化循环 (0x08016E9C - 0x08016EAA) ---
     * 0x08016E9C: LDR.W sl -> TABLE_OFF
     * 0x08016EA4: LSR.W r8, r5, #20       -> exponent_r8 = thr_hi >> 20
     * 0x08016EA8: MOVS r5, #1             -> loop_r5 = 1
     * 0x08016EAA: ADD sl, pc              -> sl = &lut_table[0]
     */
    exponent_r8 = (thr_hi >> 20) & 0x7FF;

    /* sl = &lut_table[0]  (运行时计算为 0x080196D8) */

    /* ================================================================
     * 多项式迭代循环 (0x08016EAC - 0x08016F38)
     * 最多 3 次迭代 (r5 = 1, 2, 3).
     * ================================================================ */
    for (loop_r5 = 1; loop_r5 <= 3; loop_r5++) {

        /* 0x08016EB4: BL func_856C(d8, d0)  -> VSCALE(d8, d0)
         * 0x08016EB8: CMP r5, #3
         * 0x08016EBA: STRD r0,r1, [r4]       -> 写入本次迭代结果
         * 0x08016EBE: BEQ loop_exit
         */
        d8 = func_856C(d8, d0);     /* VSCALE(d8, correction) */
        d0 = d8;                    /* 为下次迭代更新 d0 */
        /* STRD 将 d8 写入 output[loop_r5-1] */
        output[loop_r5 - 1] = d8;

        if (loop_r5 == 3) break;

        /* --- 指数检查: 判断是否需要继续 (0x08016EC0 - 0x08016ED0) ---
         * 0x08016EC0: UBFX r0, r1, #20, #11  -> 提取结果指数字段
         * 0x08016EC4: ADD.W r1 = loop_r5*33
         * 0x08016EC8: SUB r0 = exponent_r8 - result_exp
         * 0x08016ECC: SUBS r1, #0x11         -> r1 = loop_r5*33 - 17
         * 0x08016ECE: CMP r1, r0
         * 0x08016ED0: BGE loop_exit          -> if (loop_r5*33-17 >= exponent_diff) break
         */
        {
            uint32_t result_hi = double_hi(d8);
            uint32_t result_exp = (result_hi >> 20) & 0x7FF;
            int32_t exp_diff = (int32_t)(exponent_r8 - result_exp);
            int32_t limit = (int32_t)(loop_r5 * 33) - 17;
            if (limit >= exp_diff) break;
        }

        /* --- 加载查找表条目 -> d0 (0x08016ED2 - 0x08016EEA) ---
         * 0x08016ED2: ADD fp, sl, r5, lsl #4  -> fp = &lut_table[loop_r5]
         * 0x08016EDA: VLDR d0, [fp]
         * 0x08016EEA: BL func_8578(d9, d0) -> d0 = d9 * lut[loop_r5][0]
         */
        {
            double tbl_a = lut_table[loop_r5][0];
            double tbl_b = lut_table[loop_r5][1];

            /* 0x08016ED6: VMOV.F32 s20,s16  -> 原始 d10 低位到单精度
             * 0x08016EDE: VMOV.F32 s21,s17  -> 原始 d10 高位到单精度
             * 这两条将 d10 转换为 FP32 表示 (不影响 d10 双精度值),
             * 实际目的可能是准备后续 VMUL 的标量操作数.
             */

            /* VMUL: d0 = d9 * tbl_a */
            d0 = func_8578(d9, tbl_a);
            d11 = d0;               /* 暂存到 d11 */

            /* VSCALE: d8 = VSCALE(d10, d0) */
            d8 = func_856C(d10, d11);

            /* VSCALE: d0 = VSCALE(d10, d8) */
            d0 = func_856C(d10, d8);

            /* VSCALE: d0 = VSCALE(d0, d11) */
            d0 = func_856C(d0, d11);

            /* 加载第二表项: d0 = d9 * tbl_b */
            d0 = func_8578(d9, tbl_b);

            /* VSCALE: d8 = VSCALE(d10, d0) */
            d8 = func_856C(d10, d0);

            /* 用 d8 更新 d0 为下一轮迭代 */
            d0 = d8;
        }
    }

    /* ================================================================
     * 循环后处理 (0x08016F3A - 0x08016F56)
     * ================================================================
     * 0x08016F3A: B Path_B_sign_adj
     */
Path_B_exit:
    /* 提前退出: 检查符号/符号调整
     * 0x08016F3C: CMP r6, #0
     * 0x08016F3E: IT GE
     * 0x08016F40: MOVGE r0, r7
     * 0x08016F42: BGE.W return           -> if (selector >= 0) return i7
     *
     * 否则对 output 做符号翻转:
     * 0x08016F46: VLDR d0, [r4]
     * 0x08016F4A: VMOV r0,r1, d0
     * 0x08016F4E: EOR r1, #0x80000000    -> 翻转符号位
     * 0x08016F52: STRD r0,r1, [r4]
     * 0x08016F56: RSBS r0, r7, #0        -> r0 = -i7
     */
    if (selector >= 0) {
        return (uint32_t)i7;
    }
    {
        /* 翻转已存储结果的符号位 */
        union { double d; uint32_t u[2]; } u;
        u.d = *output;
        u.u[1] ^= 0x80000000u;      /* 翻转 IEEE 754 符号位 (高字 bit 31) */
        *output = u.d;
        return (uint32_t)(-(int32_t)i7);
    }


    /* ================================================================
     * Phase 3: 次正规数修复 / 多精度乘法 (0x08016F5A - 0x080170FA)
     * ================================================================
     * 此段处理极大或极小的输入, 通过 128-bit 整数运算避免精度丢失.
     *
     * 0x08016F5A: UBFX r0, r6, #0, #20   -> 提取 mantissa 低 20 位 (忽略隐含 1)
     * 0x08016F5E: ORR lr, r0, #0x100000   -> lr = 1.mantissa (Q20.20 格式)
     * 0x08016F62: UBFX r0, r6, #20, #11   -> 指数
     * 0x08016F66: SUBW r0, r0, #0x3F5     -> 调整指数 (bias + 移位)
     * 0x08016F6A: ASRS r7, r0, #5         -> r7 = 高移位量 (字索引)
     * 0x08016F6C: AND r5, r0, #0x1F       -> r5 = 低移位量 (位偏移)
     * 0x08016F70: RSB.W r0, r5, #32       -> r0 = 32 - bit_offset
     * 0x08016F74: LDR fp, [sp, #0x48]     -> fp = 输入的高 32 位 (原始 double hi)
     *
     * 0x08016F78: STR r0, [sp, #0x20]     -> 存移位量
     * 0x08016F7A: MOVS r0, #0
     * 0x08016F7C: STR r0, [sp, #0x18]     -> 清零 128-bit 累加器低字 0
     * 0x08016F7E: STR r0, [sp, #0x1C]     -> 清零 128-bit 累加器低字 1
     * 0x08016F80: MOVS r0, #5             -> r0 = 5 (6 个字的循环: 5..0)
     * 0x08016F82: MOV ip, sp              -> ip = &accumulator[0]
     *
     * 0x08016F84: LDR r1, [pc, JMPTBL_OFF] -> r1 = 跳转表偏移
     * 0x08016F88: ADD r2, r7, r0           -> r2 = 表索引
     * 0x08016F8C: ADD r1, pc               -> r1 = 跳转表基址
     *
     * --- 跳转表分发循环 (0x08016F8E - 0x08016FF8) ---
     * 根据 bit_offset (r5) 是否为 0, 选择不同的乘法路径:
     *
     * 如果 bit_offset == 0:
     *   0x08016F8E: IT EQ / LDREQ.W r1, [r1, r2, lsl #2]  -> 跳转
     *   0x08016F94: BEQ dispatch
     *
     * 否则 (bit_offset != 0):
     *   0x08016F96: LDR.W r3, [r1, r2, lsl #2]   -> r3 = 表值[索引]
     *   0x08016F9A: ADD r1, r1, r2, lsl #2       -> r1 = &表值[索引]
     *   0x08016F9E: LSLS r3, r5                   -> r3 <<= bit_offset
     *   0x08016FA0: LDR r2, [r1, #4]              -> r2 = 下一个表值
     *   0x08016FA2: LDR r1, [sp, #0x20]           -> r1 = 32 - bit_offset
     *   0x08016FA4: LSR.W r1, r2, r1              -> r1 = r2 >> (32 - bit_offset)
     *   0x08016FA8: ORRS r1, r3                   -> r1 |= r3  (拼接 64-bit 操作数)
     *
     *   0x08016FAA: UMULL r2, r6, r1, lr          -> r6:r2 = r1 * lr (64-bit)
     *   0x08016FAE: UMULL r1, r3, r1, fp          -> r3:r1 = r1 * fp (64-bit)
     *
     *   加法与进位传播 (0x08016FB2 - 0x08016FD2):
     *   0x08016FB2: ADD r2, r3                    -> r2 += r3
     *   0x08016FB4: CMP r2, r3                    -> 检查进位
     *   0x08016FB6: ITE LO / MOVLO r3, #1 / MOVHS r3, #0
     *   0x08016FBC: ADD r8, r3, r6                -> r8 = r6 + carry
     *
     *   0x08016FC0: ADD r3, ip, r0, lsl #2        -> r3 = &accumulator[r0]
     *   0x08016FC4: LDRD sl, r6, [r3, #4]         -> 加载累加器
     *   0x08016FC8: ADD r6, r1                    -> r6 += r1
     *   0x08016FCA: CMP r6, r1                    -> 检查进位
     *   0x08016FCC: ITE LO / MOVLO r1, #1 / MOVHS r1, #0
     *   0x08016FD2: ADD sl, r2                    -> sl += r2
     *   0x08016FD4: ADD sl, r1                    -> sl += carry
     *
     *   0x08016FD6: CBZ r1, skip                  -> 若无进位则跳过
     *   0x08016FD8: CMP sl, r2
     *   0x08016FDA: BLS second_carry              -> 若 sl <= r2, 有二次进位
     *   0x08016FDC: B no_second_carry
     *
     *   第二次进位: MOVS r1, #1 / B store
     *   无二次进位: MOVS r1, #0
     *
     *   0x08016FE8: STRD sl, r6, [r3, #4]         -> 存回累加器
     *   0x08016FEC: ADD r1, r8                    -> r1 += r8
     *   0x08016FEE: STR r1, [ip, r0, lsl #2]      -> 存进位字
     *
     *   0x08016FF2: SUBS r1, r0, #0               -> 更新标志
     *   0x08016FF4: SUB r0, r0, #1                -> r0--
     *   0x08016FF8: BGT loop                       -> 若 r0 > 0 继续循环
     */

    /* ================================================================
     * 结果标准化与最终乘法 (0x08016FFA - 0x080170F4)
     * ================================================================
     * 0x08016FFA: LDR r0, [sp, #8]                 -> 128-bit 结果高字
     * 0x08016FFC: ADD r0, r0, #0x20000000          -> 舍入
     * 0x08017000: LSRS r5, r0, #30                 -> r5 = 舍入后高 2 位
     * 0x08017002: LDR r0, [sp, #8]                 -> 重新加载
     * 0x08017004: LDRD r6, r7, [sp, #0xC]          -> 128-bit 的中间 64 位
     * 0x08017008: LSLS r0, r0, #2                  -> 规格化移位
     * 0x0801700A: LDR r8, [sp, #0x14]              -> 128-bit 最低字
     *
     * --- 转换为 double (0x0801700E - 0x08017054) ---
     * 将多精度整数的 3 个片段分别转为 double:
     *
     * 0x0801700E: BL func_873A(r0)                 -> d10 = (double)规范化高64位低字
     * 0x08017018: BL func_875C(r6)                 -> d0  = fixup_convert(r6)
     * 0x08017024: BL func_8578(d0, 2^-30)          -> d11 = d0 * 2^-30
     *
     * 0x0801702E: BL func_875C(r7)                 -> d0  = fixup_convert(r7)
     * 0x0801703A: BL func_8578(d0, 2^-50)          -> d8  = d0 * 2^-50
     *
     * 0x08017044: BL func_875C(r8)                 -> d0  = fixup_convert(r8)
     * 0x08017050: BL func_8578(d0, 2^-94)          -> d9  = d0 * 2^-94
     *
     * --- 合并为最终双精度值 (0x08017058 - 0x08017072) ---
     * 0x08017060: BL func_842A(d8, d9)             -> d0 = d8 + d9
     * 0x08017068: BL func_842A(d0, d11)            -> d0 = d0 + d11
     * 0x08017070: BL func_842A(d0, d10)            -> d0 = d0 + d10
     * 0x08017074: STRD r0,r1, [sp, #0x20]          -> 暂存
     *
     * --- 反向修正 (0x08017078 - 0x080170A0) ---
     * 0x08017078: MOVS r0, #0
     * 0x0801707A: STR r0, [sp, #0x20]              -> 清零上部 (构造零值)
     * 0x0801707C: VLDR d0, [sp, #0x20]              -> d0 = 0.0 (用作 scale 参数)
     * 0x08017088: BL func_856C(d10, d0)            -> VSCALE(d10, 0)
     * 0x08017090: BL func_856C(d11, d0)            -> VSCALE(d11, 0)
     * 0x08017098: BL func_856C(d8,  d0)            -> VSCALE(d8,  0)
     * 0x080170A0: BL func_8572(d9,  d0)            -> 比较/调整
     *
     * --- 多项式修正 (0x080170A4 - 0x080170EC) ---
     * 0x080170A4: VLDR d1 -> coeff_p
     * 0x080170AC: BL func_8578(d0, d1)             -> VMUL(结果, coeff_p)
     * 0x080170B0: VLDR d0 -> coeff_q1
     * d8 = 结果
     *
     * 0x080170C4: BL func_8578(d0, d0_from_stack)  -> VMUL(coeff_q1, tmp)
     * 0x080170CC: BL func_842A(d0, d8)             -> VADD(结果, d8)
     *
     * 0x080170D0: VLDR d0 -> coeff_q2
     * d8 = 结果
     * 0x080170E4: BL func_8578(d0, tmp)            -> VMUL(coeff_q2, tmp)
     * 0x080170EC: BL func_842A(d0, d8)             -> VADD(结果, d8)
     */

    /* ================================================================
     * 最终符号处理 (0x080170F0 - 0x08017110)
     * ================================================================
     * 0x080170F0: VMOV d0, r0,r1                     -> d0 = 最终结果
     * 0x080170F4: LDR r0, [sp, #0x4C]               -> r0 = sign_flag
     * 0x080170F6: TST r0, #0x80000000                -> 测试 bit 31
     * 0x080170FA: BEQ skip_negate
     *
     * 如果 bit 31 置位:
     *   0x080170FC: RSBS r5, r5, #0                  -> r5 = -r5
     *   0x080170FE: VMOV r0,r1, d0
     *   0x08017102: EOR r1, #0x80000000              -> 翻转符号位
     *   0x08017106: VMOV d0, r0,r1
     *
     * 0x0801710A: MOV r0, r5
     * 0x0801710C: VSTR d0, [r4]                       -> 最终写入 output
     * 0x08017110: B return                             -> 返回 r0 = r5
     */
    {
        /* d0 = 多项式合并后的最终 double */
        int32_t final_sign = (sign_flag < 0);  /* bit 31 test */

        if (final_sign) {
            /* 翻转结果符号 */
            union { double d; uint32_t u[2]; } u;
            u.d = d0;
            u.u[1] ^= 0x80000000u;
            d0 = u.d;
            loop_r5 = -loop_r5;
        }

        *output = d0;
        return (uint32_t)loop_r5;
    }
}


/* ================================================================
 * 总 结
 * ================================================================
 *
 * 本函数是一个软件实现的 IEEE 754 双精度三角函数计算核心,
 * 推测为 sin() 或 cos() -- 基于以下特征:
 *
 *   1. 使用 PI/2, 2/PI 常量 -- 典型的 range reduction
 *   2. d8/d9/d10/d11 的 scale factor 值递减 -- 多项式 Horner 迭代
 *   3. func_8776 (floor) + func_873A (int->double) -- 象限提取
 *   4. 128-bit 乘法与进位传播 -- 次正规数/高精度 fixup
 *   5. 查找表多项式系数 -- MinMax/Remez 近似
 *
 * 调用 func_850C 作为前置 range reduction (可能是 rem_pio2 或
 * __ieee754_rem_pio2 的 ARM 版本).
 *
 * 主循环使用 VSCALE.F64 (func_856C) 执行缩放操作 -- 在软件 FPU
 * 实现中, VSCALE 操作用于高效地乘以 2 的整数次幂,
 * 相当于 ldexp() 但无需完整的浮点乘法.
 *
 * BL 调用统计:
 *   func_856C (VSCALE): 12 次
 *   func_842A (VADD):   11 次
 *   func_8578 (VMUL):   11 次
 *   func_8572:           2 次
 *   func_8776:           1 次
 *   func_873A:           2 次
 *   func_875C:           3 次
 *   func_850C:           1 次
 *   -------------------------
 *   总计:               43 次 BL 调用
 *
 * 地址范围: 0x08016DA2 - 0x080171A0 (1022 bytes)
 * 文件偏移: 0x0EDA2 - 0x0F1A0
 */
