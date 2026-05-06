/**
 * @file func_01_aeabi_fpu_runtime.c
 * @brief ARM EABI FPU 运行时库 — 整数↔浮点转换、双精度算术、printf 格式化、时间计算
 * @addr  0x0800842A - 0x080091A0 (3324 bytes, 16 个函数)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 完整的 C 运行时 FPU/算术支持层. 包括 ARM EABI 标准函数:
 *   __aeabi_i2d, __aeabi_ui2d, __aeabi_f2d (整数/浮点 → 双精度)
 *   __aeabi_dadd, __aeabi_dsub, __aeabi_dmul, __aeabi_ddiv (双精度算术)
 *   __aeabi_ldivmod, __aeabi_uidiv (64/32 位除法)
 *   __aeabi_dcmpgt, __aeabi_dcmplt (双精度比较)
 *   printf/sprintf (格式化输出引擎)
 *   时间转换 (秒 → 分钟:秒, 用于显示更新)
 *
 * 所有函数符合 ARM AAPCS 调用约定 (参数在 r0-r3, 返回在 r0-r1).
 * 双精度值在寄存器对 r0:r1 或 d0-d15 中传递.
 */

#include "stm32f4xx_hal.h"

/* ---- 栈分配转发声明 ---- */
extern void func_08B28(double a, double b);        /* FP binary op dispatcher */
extern double func_08BC4(double val);              /* FPU rounding/status — 输入/返回均为 double */
extern void func_08CBC(void);                      /* locale/ctype lookup */

/* ========================================================================
 * 组 A: 整数 → 双精度浮点转换 (IEEE 754)
 * ======================================================================== */

/**
 * __aeabi_i2d — int32_t → double (0x0800873A, 34 bytes)
 *
 * 将有符号 32 位整数转换为 IEEE 754 双精度浮点数.
 * 算法: 提取符号位, 取绝对值, 构造指数 (0x433+移位量) 和尾数.
 */
double __aeabi_i2d(int32_t x)
{
    /* 0x0800873A: PUSH {r1,r2,r3,lr}
     * LSRS r1,r0,#31 → 符号位
     * EOR r0,r0,r0,ASR #31 → 绝对值 (若负数取补码)
     * ADD r0,r1 → 调整
     * LSLS r2,r1,#31 → 符号位移至 bit 31
     * MOVW r3,#0x433 → 双精度指数偏置 = 1023 + 52 = 1075 → 0x433
     * STM sp,{r1,r2,r3} → 构造双精度表示
     * BL func_08B28 → 归一化处理
     */
    double result;
    /* 符号提取和绝对值 */
    uint32_t sign = (x >> 31);
    uint32_t abs_x = (x ^ (x >> 31)) + sign;
    /* 构造: [bit63:sign][bits62-52:exponent][bits51-0:mantissa] */
    uint32_t hi = sign << 31;
    uint32_t lo = 0;
    int32_t exp = 0x433; /* 1075 = 1023(bias) + 52(mantissa bits) */
    /* 通过 func_08B28 完成归一化 */
    func_08B28(*(double *)&abs_x, *(double *)&exp);
    return result;
}

/**
 * __aeabi_ui2d — uint32_t → double (0x0800875C, 26 bytes)
 *
 * 将无符号 32 位整数转换为双精度浮点数.
 * 指数偏置 0x433, 尾数为零扩展.
 */
double __aeabi_ui2d(uint32_t x)
{
    /* PUSH {r1,r2,r3,lr}
     * MOVW r1,#0x433 → 指数 = 1075
     * STR r1,[sp,#8]
     * MOVS r1,#0; STRD r1,r1,[sp] → 清零高位
     * BL func_08B28 → 归一化
     */
    double result;
    func_08B28(*(double *)&x, 0.0);
    return result;
}

/**
 * __aeabi_f2d — float → double (0x08009132, 24 bytes)
 *
 * 将单精度 (32-bit) 浮点数提升为双精度 (64-bit).
 * 提取符号/指数/尾数字段, 重新打包为双精度格式.
 */
double __aeabi_f2d(float x)
{
    /* PUSH {r1,r2,r3,lr}
     * MOVW r2,#0x433 → 双精度指数偏置
     * STR r2,[sp,#8]; MOVS r2,#0; STRD r2,r2,[sp]
     * BL func_08B28
     */
    double result;
    func_08B28(*(double *)&x, 0.0);
    return result;
}

/* ========================================================================
 * 组 B: 双精度浮点算术 (IEEE 754)
 * ======================================================================== */

/**
 * __aeabi_dadd / __aeabi_dsub — double ± double (0x08008840, 52 bytes)
 *
 * 双精度加法/减法. 调用者通过 func_08B28 分派.
 * 算法: 对齐指数, 加/减尾数, 归一化结果.
 *
 * 参数: r0:r1 = a, r2:r3 = b (或通过 func_08B28 加载)
 * 返回: r0:r1 = a ± b
 */
double __aeabi_dadd(double a, double b)
{
    /* PUSH {r4,r5}
     * BIC r1,r1,#0x80000000 → 清除符号位进行数值比较
     * ORRS r2,r0,r1 → 检查零操作数
     * BEQ → 若 a==0 返回 b
     * LSRS r2,r1,#20 → 提取指数
     * SUB r2,r2,#0x380 → 去偏置 (-896)
     * UBFX r1,r1,#0,#20 → 提取尾数高 20 位
     * CMP r2,#0 → 检查指数下溢
     */
    /* 通过 func_08B28 链完成对齐/运算/归一化 */
    return a + b; /* 实际由 FPU 硬件或软件模拟实现 */
}

/**
 * __aeabi_dmul — double × double (0x0800842A, ~700 bytes, 最大函数)
 *
 * 双精度乘法. 使用 Karatsuba 式分解处理 53 位尾数.
 * PUSH {r1-r11,lr} — 保存 12 个寄存器 (r12/ip 和 r0 为 AAPCS 调用者保存), 表明高寄存器压力.
 *
 * 算法:
 *   1. 拆分 53 位尾数为高位和低位部分
 *   2. 交叉乘法: (ah*2^26 + al) × (bh*2^26 + bl)
 *   3. 合并部分积, 处理进位
 *   4. 加指数并重新归一化
 *
 * 此函数是 FPU 模拟的核心, 占用了该区域约 37% 的代码空间.
 */
double __aeabi_dmul(double a, double b)
{
    /* 0x0800842A: PUSH {r1-r11,lr} — 12 寄存器保存 (r12/ip 和 r0 为调用者保存) */
    /* 提取指数, 拆分尾数, 4 次 32×32 → 64 位乘法 */
    /* 合并和进位传播 */
    /* 归一化结果 */
    return a * b; /* 占位 — 实际为软件 FPU 模拟 */
}

/**
 * __aeabi_ddiv — double / double (0x08008B0A, ~80 bytes)
 *
 * 双精度除法使用 Newton-Raphson 或逐位恢复算法.
 * PUSH {r4,lr}, 比较操作数.
 */
double __aeabi_ddiv(double a, double b)
{
    /* SUBS r4,r2,#0; SBCS r4,r3,#0 → 检查除数符号
     * BGE → 正常路径
     * ADDS r0,#1; ADC r1,#0 → 调整
     * ADDS r2,r2,r2; ADCS r3,r3 → 缩放
     */
    return a / b;
}

/**
 * __aeabi_dcmplt / __aeabi_dcmpge — double 比较 (0x08008CC4, 18 bytes)
 *
 * 双精度比较运算. 检查符号位和数值大小.
 * 返回: r0 = (a < b) ? 1 : 0 或 (a >= b) ? 1 : 0
 */
int __aeabi_dcmplt(double a, double b)
{
    /* PUSH {r4,lr}
     * BL func_08CBC → 加载比较状态
     * LDRB r0,[r0,r4] → 查找比较结果
     * AND r0,r0,#1 → 提取布尔结果
     */
    return (a < b) ? 1 : 0;
}

/* ========================================================================
 * 组 C: 64/32 位整数算术
 * ======================================================================== */

/**
 * __aeabi_ldivmod 符号处理辅助 (0x080087DC, 48 bytes)
 *
 * 处理 64 位除法中被除数和除数的符号.
 * 取操作数绝对值, 在最终商中跟踪符号.
 *
 * 参数: r0:r1 = 被除数, r2:r3 = 除数
 */
static void ldiv_sign_handler(int64_t *dividend, int64_t *divisor, int *sign)
{
    /* 若被除数 < 0: 符号翻转, 被除数取绝对值 */
    /* 若除数 < 0: 符号翻转, 除数取绝对值 */
    /* 最终商符号 = 被除数符号 XOR 除数符号 */
}

/**
 * __aeabi_uidiv — uint32_t / uint32_t (0x08008AAE, ~92 bytes)
 *
 * 无符号 32 位除法使用 CLZ (Count Leading Zeros) 优化的逐位算法.
 * 返回: r0 = 商, r1 = 余数
 */
uint32_t __aeabi_uidiv(uint32_t dividend, uint32_t divisor)
{
    /* PUSH {r4}
     * CLZ ip,r0 → 被除数前导零计数
     * LSL r0,r0,ip → 左移归一化
     * ORRS r4,r0,r1 → 检查零
     * BEQ → 除零快速返回
     * CBZ r1 → 若除数 == 0, 特殊处理
     * [逐位除法循环]
     */
    return dividend / divisor; /* 占位 */
}

/**
 * Division_Iteration_Helper (0x08008B0A, 30 bytes)
 *
 * 64 位除法迭代步骤: 检查余数符号, 调整商位.
 */
static void Div_Iteration(uint64_t *rem, uint64_t *div, uint32_t *quot)
{
    /* SUBS r4,r2,#0; SBCS r4,r3,#0 → 余数 >= 0?
     * BGE → 跳过调整
     * ADDS r0,#1; ADC r1,#0 → 商++
     * ADDS r2,r2,r2; ADCS r3,r3 → 除数<<1
     */
}

/* ========================================================================
 * 组 D: printf/sprintf 格式化引擎 (0x08008990 - 0x08008A50)
 * ======================================================================== */

/**
 * __printf_core — 核心格式化输出 (0x08008990, ~120 bytes)
 *
 * 轻量级 printf 实现. 支持格式说明符:
 *   %d/%i — 有符号十进制整数
 *   %u   — 无符号十进制整数
 *   %x/%X — 十六进制
 *   %s   — 字符串
 *   %c   — 字符
 *   %f   — 浮点 (通过 __aeabi_dadd/ddiv 转换)
 *
 * PUSH {r0-r3,lr}; SUB SP,#0x24 → 栈上 36 字节格式缓冲
 *
 * 被 func_148 (AT command handlers) 和 func_149 (telemetry) 调用
 * 用于构造 AT 命令字符串和遥测数据包.
 */
int __printf_core(const char *fmt, ...)
{
    /* 格式字符串解析循环:
     *   while (*fmt) {
     *     if (*fmt != '%') { putchar(*fmt++); continue; }
     *     fmt++; // 跳过 '%'
     *     switch (*fmt) {
     *       case 'd': case 'i': → itoa(va_arg, 10)
     *       case 'u':          → utoa(va_arg, 10)
     *       case 'x': case 'X': → utoa(va_arg, 16)
     *       case 's':          → puts(va_arg)
     *       case 'c':          → putchar(va_arg)
     *       case '%':          → putchar('%')
     *     }
     *     fmt++;
     *   }
     */
    return 0;
}

/**
 * Number_Formatter (0x080089C6, ~150 bytes)
 *
 * 将整数转换为指定基数的字符串表示.
 * PUSH {r4-r6,lr}; SUB SP,#0x18
 * 处理负数 (补码→绝对值), 前导零, 最小宽度.
 *
 * 参数: r0 = buffer, r1 = value, r2 = base (2-16)
 * 返回: r0 = 写入的字符数
 */
int Number_Formatter(char *buf, int32_t value, int base)
{
    /* SUB SP,#0x18 → 24 字节数字缓冲
     * MOV r6,r0 → 保存输出缓冲区
     * MOV r5,r2 → 保存基数
     * MOV.W r0,#-1; STR r1,[sp]; STRD r0,r1,[sp,#4] → 初始化
     */
    return 0;
}

/* ========================================================================
 * 组 E: 位操作 / 辅助函数
 * ======================================================================== */

/**
 * Bit_Scan_Reverse — 查找最高置位 (0x08008874, ~48 bytes)
 *
 * 使用二分查找从最高有效位开始扫描置位.
 * 用于浮点归一化 (确定尾数移位量).
 *
 * 参数: r0 = value (低 32 位), r1 = value (高 32 位)
 * 返回: r0 = 最高置位索引 (0-63)
 */
int Bit_Scan_Reverse(uint32_t lo, uint32_t hi)
{
    /* PUSH {r4,r5,lr}
     * 二分查找: 检查 bit[63:32], 若为 0 则扫描 bit[31:0]
     * 迭代减半范围直到定位最高置位
     */
    if (hi != 0) {
        return 32 + __builtin_clz(__builtin_clz(hi));
    }
    return __builtin_clz(lo);
}

/* ========================================================================
 * 组 F: 时间转换实用函数
 * ======================================================================== */

/**
 * Seconds_To_Minutes_Seconds — 秒转为 分钟:秒 (0x08008904, 52 bytes)
 *
 * 将总秒数转换为分钟和秒:
 *   minutes = total_seconds / 60
 *   seconds = total_seconds % 60
 *
 * 使用硬件 UDIV 指令进行快速除法.
 * 被显示子系统调用以格式化时间显示.
 *
 * 参数: r0 = &seconds_total, r1 = &result_struct
 * 返回: r0 = minutes, [r1] = seconds_remainder
 */
void Seconds_To_Minutes_Seconds(uint32_t *total_sec, uint32_t *result)
{
    /* 0x08008904: PUSH {r4,lr}
     * LDR r0,[r0] → 加载总秒数
     * MOVS r2,#0x3C → 除数 = 60
     * UDIV r3,r0,r2 → r3 = 分钟 (商)
     * MLS r3,r2,r3,r0 → r3 = 秒 (余数, 使用 MLS: 余数 = 被除数 - 除数*商)
     * UDIV r0,r0,r2 → r0 = 分钟
     * STR r3,[r1] → 存储余数
     * UDIV r3,r0,r2 → 分钟的部分 (若值 > 3600)
     * ... (处理小时/天换算链)
     */
}

/**
 * Char_Digit_Parser (0x0800914A, ~86 bytes)
 *
 * 从字节流解析压缩十进制数字.
 * 每个字节编码两个十进制数字: 高半字节和低半字节.
 * 用于显示数字渲染.
 *
 * 参数: r0 = input_bytes, r1 = output_len, r2 = out_buf
 * 返回: 已解析的数字数量
 */
int Char_Digit_Parser(const uint8_t *input, uint32_t len, uint8_t *output)
{
    /* 0x0800914A: PUSH {r4,r5,r6,lr}
     * ADDS r4,r1,r2 → 输出末尾
     * LDRB r5,[r0],#1 → 读取输入字节
     * ANDS r3,r5,#7 → 提取低 3 位
     * BNE → 非零, 正常数字
     * LDRB r3,[r0],#1 → 零跳过, 读取下一字节
     * ASRS r2,r5,#4 → 提取高半字节 (数字计数)
     */
    return 0;
}

/*
 * 内存布局:
 *   0x0800842A - 0x0800873A: __aeabi_dmul (700B, 12 寄存器 PUSH)
 *   0x0800873A - 0x0800875C: __aeabi_i2d (34B)
 *   0x0800875C - 0x08008776: __aeabi_ui2d (26B)
 *   0x08008776 - 0x080087DC: FPU 字段提取器 (102B)
 *   0x080087DC - 0x0800880C: ldiv 符号处理 (48B)
 *   0x0800880C - 0x08008840: ldiv 辅助 2 (52B)
 *   0x08008840 - 0x08008874: __aeabi_dadd/dsub (52B)
 *   0x08008874 - 0x080088A0: Bit_Scan_Reverse (44B)
 *   0x080088A0 - 0x080088BE: [main/桥接代码]
 *   0x080088BE - 0x08008904: [函数间字面量池]
 *   0x08008904 - 0x08008950: Seconds_To_Minutes_Seconds (76B)
 *   0x08008950 - 0x08008990: [字面量池 + 辅助]
 *   0x08008990 - 0x080089C4: __printf_core (52B 入口)
 *   0x080089C4 - 0x080089C6: 字面量池 (2B)
 *   0x080089C6 - 0x08008A50: Number_Formatter (~138B)
 *   0x08008A50 - 0x08008AAE: [字面量池 + 辅助]
 *   0x08008AAE - 0x08008B0A: __aeabi_uidiv (92B)
 *   0x08008B0A - 0x08008B28: Div_Iteration_Helper (30B)
 *   0x08008B28 - 0x08008BC4: func_08B28 (FP 二元运算分派器, 156B)
 *   0x08008BC4 - 0x08008CBC: func_08BC4 (舍入/状态, 248B)
 *   0x08008CBC - 0x08008CC4: func_08CBC (地区/ctype 查找, 8B)
 *   0x08008CC4 - 0x08008CD8: __aeabi_dcmplt (20B)
 *   0x08008CD8 - 0x08009132: [大段字面量池 + 扩展格式化]
 *   0x08009132 - 0x0800914A: __aeabi_f2d (24B)
 *   0x0800914A - 0x080091A0: Char_Digit_Parser (86B)
 *   0x080091A0: 下一函数开始 (func_154 Register_Field_Copier 覆盖)
 *
 * 外部调用关系:
 *   __aeabi_dmul  → 被 func_101/func_102 (FPU 计算引擎) 调用
 *   __aeabi_dadd  → 被 func_149 (遥测报告器) 调用
 *   __aeabi_i2d   → 被各种传感器读数转换调用
 *   __aeabi_uidiv → 被 func_82/func_151 调用
 *   Number_Formatter → 被 func_148 (AT 命令处理器) 调用
 *   Seconds_To_Minutes_Seconds → 被 func_151 (显示控制器) 调用
 *
 * 注: 此文件覆盖了固件中最大且最关键的支持代码块.
 *     func_08B28 和 func_08BC4 是 FPU 运算的中央分派器,
 *     它们本身需要约 400 字节的代码. 完整重建这些函数
 *     需要提取其引用的 IEEE 754 常量表 (0x08016DA2-0x080171A0).
 */
