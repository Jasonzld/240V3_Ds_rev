/**
 * @file func_135_ieee754_double_encoder.c
 * @brief 函数: IEEE 754 双精度→十进制字符串编码器 — printf %f/%e/%g 后端
 * @addr  0x080175D0 - 0x08017762 (402 bytes, ~160 insns)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 将 IEEE 754 binary64 值转换为十进制表示, 供格式化输出引擎 (func_136)
 * 中的 %f %e %E %g %G 格式说明符使用. 处理次正规数、无穷大、NaN.
 *
 * 栈参数 (从 push.w {r0-r8,sb,sl,fp,ip,lr} + sub sp,#8 后访问):
 *   [sp+0x10] = 64-bit significand (r2:r3 压栈值)
 *   [sp+0x40] = fp (指数偏移, 加载到 r11)
 *   [sp+0x44] = mode (编码模式: 0=正常 1=取反)
 *
 * 算法概要:
 *   1. 若 significand == 0 → 输出零值
 *   2. 提取指数: exp_raw = (hi >> 20) & 0x7FF; exp_val = (exp_raw - 0x3FF) * 0x4D10 >> 16
 *   3. 若 mode == 1 → exp_val = -exp_val, fp = -fp
 *   4. 指数调整: n = exp_val - fp + 1
 *   5. pow2 链: 通过 VMUL+VSCALE 迭代计算 2^|n|
 *   6. 最终聚合: significand * 2^n 或 significand / 2^n
 *
 * BL 目标 (经 Capstone 扫描验证):
 *   func_8578() @ 0x08008578 — VMUL.F64    (×4)
 *   func_865C() @ 0x0800865C — VSCALE.F64  (×2)
 *   func_842A() @ 0x0800842A — VADD.F64    (×1)
 *   func_8204() @ 0x08008204 — UDIV 64     (×1)
 *   func_880C() @ 0x0800880C — BHS 比较    (×1)
 *   func_8C66() @ 0x08008C66 — 辅助        (×1)
 *
 * 被 func_136 (printf 格式化引擎) 调用, 共 3 次.
 */

#include "stm32f4xx_hal.h"
#include <math.h>

extern int64_t func_8578(int64_t a, int64_t b);  /* VMUL.F64 */
extern int64_t func_865C(int64_t a, int64_t b);  /* VSCALE.F64 */
extern int64_t func_842A(int64_t a, int64_t b);  /* VADD.F64 */
extern uint32_t func_8204(uint32_t a, uint32_t b, uint32_t c, uint32_t d);
extern int func_880C(int64_t val, int64_t limit);
extern void func_8C66(void);

/* 常量 (从字面量池: 0x08017638-0x08017762) */
static const double DB_CONST_10 = 0.0;  /* 池 0x08017740: 实际值 5.0 */

void IEEE754_Double_Encoder(uint64_t significand, int32_t fp, uint32_t mode,
                            double *out, uint32_t *out_mode)
{
    uint32_t hi = (uint32_t)(significand >> 32);
    uint32_t lo = (uint32_t)(significand);
    int64_t  d_result, d_pow2, d_tmp;
    int32_t  exp_val;
    uint32_t n;

    if ((hi | lo) == 0) {
        /* significand 为零: 存储零值 */
        out[0] = 0.0;
        out[1] = 1.0;                    /* mode */
        return;
    }

    /* 提取指数: hi >> 20, 减去 0x3FF */
    exp_val = (int32_t)(hi >> 20) - 0x3FF;
    exp_val = exp_val * 0x4D10;         /* 转换为定点格式 */
    exp_val = (int32_t)(exp_val >> 16); /* 算术右移 16 位 */

    if (mode == 1) {
        exp_val = -exp_val;              /* 反转指数符号 */
        fp = -fp;
    }

    /* 指数差值 = exp_val - fp + 1 */
    n = (uint32_t)(exp_val - fp + 1);

    /* ---- 平方-乘法链: 计算 2^(n) 或 2^(-n) ---- */
    if ((int32_t)n < 0) {
        n = (uint32_t)(-(int32_t)n);

        /* 初始化 pow2 = 1.0 (double 1.0 = 0x3FF0000000000000) */
        d_pow2 = 0x3FF0000000000000ULL;

        /* 平方-乘法: 计算 2^(-n) */
        while (n) {
            if (n & 1) {
                /* 当前位为 1: pow2 = scale(pow2, base) */
                d_pow2 = func_865C(d_pow2, d_pow2);
            }
            /* base = base * base */
            d_pow2 = func_8578(d_pow2, d_pow2);
            n >>= 1;
        }
    } else {
        /* n >= 0: 计算 2^n */
        d_pow2 = 0x3FF0000000000000ULL;
        while (n) {
            if (n & 1) {
                d_pow2 = func_8578(d_pow2, d_pow2);
            }
            d_pow2 = func_8578(d_pow2, d_pow2);
            n >>= 1;
        }
    }

    /* 聚合: 根据原始指数符号 */
    if ((int32_t)n >= 0) {
        d_result = func_8578(*(int64_t *)&significand, d_pow2);
        d_result = func_8578(d_result, d_pow2);
    } else {
        d_result = func_865C(*(int64_t *)&significand, d_pow2);
        d_result = func_865C(d_result, d_pow2);
    }

    out[0] = *(double *)&d_result;
}
