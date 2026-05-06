/**
 * @file func_133_fpu_nan_check_chain.c
 * @brief 函数: FPU NaN 检查 + 乘法链 — 先检查 NaN, 再执行多级 FPU 运算
 * @addr  0x08017408 - 0x08017538 (304 bytes, 含 44B 尾部 NaN 传播 + 24B 字面量池)
 * DISASSEMBLY-TRACED. Verified against Capstone (entry + key sections).
 *
 * 输入: double val, uint32_t flags ([sp+0x2c])
 * 先检查 NaN (func_16B40), 若为 NaN 则调用 func_17570 后提前返回.
 * 若 |flags| >= 0x3E400000: 完整乘法链
 * 若 r4 (mode参数) != 0: 额外 4 步乘法+缩放链
 *
 * 调用:
 *   func_16B40() @ 0x08016B40 — IEEE 754 分类器 (NaN/Inf/Zero)
 *   func_17570() @ 0x08017570 — NaN 处理器
 *   func_8578()  @ 0x08008578 — VMUL.F64
 *   func_856C()  @ 0x0800856C — FPU 双精度缩放
 *   func_8572()  @ 0x08008572 — FPU 双精度运算变体
 *   func_17310() @ 0x08017310 — FPU 多项式求值器
 */

#include "stm32f4xx_hal.h"

extern int      func_16B40(double val);
extern void     func_17570(void);
extern int64_t  func_8578(int64_t a, int64_t b);
extern int64_t  func_856C(int64_t a, int64_t b);
extern int64_t  func_8572(int64_t a, int64_t b);
extern double   func_17310(double val, uint32_t n, uint32_t mode);

double FPU_NaN_Check_Chain(double val, uint32_t mode, uint32_t flags)
{
    uint32_t abs_flags = flags & 0x7FFFFFFF;
    int64_t  d0, d8, d9, d10, d11, d12;

    d0  = *(int64_t *)&val;

    if ((int32_t)abs_flags < 0x3E400000) {
        /* 阈值以下: NaN 检查 */
        if (func_16B40(val) == 4) {         /* 是 NaN? */
            func_17570();
        }
        return *(double *)&d0;              /* 返回原值 */
    }

    /* 阈值以上: 乘法链 */
    d8 = func_8578(d0, d0);                 /* d8 = val^2 */
    d9 = func_8578(d0, d8);                 /* d9 = val^3 */

    d10 = func_17310(val, 5, 0);           /* 多项式求值 */
    d11 = func_8578(d9, d10);              /* d11 = val^3 * poly(val) */
    d12 = func_8578(d9, d11);              /* d12 = val^3 * d11 */

    if (mode != 0) {
        /* 额外链: 使用 d12 */
        d12 = func_8578(d12, d9);          /* d12 *= val^3 */
        d9  = func_8578(d9, d10);          /* d9 = val^3 * poly */
        d0  = func_8578(d11, d0);          /* d0 = d11 * val */
        d9  = func_856C(d9, d0);           /* scale */
        d8  = func_8578(d8, d9);           /* d8 = val^2 * d9 */
        d11 = func_856C(d11, d8);          /* scale */
        d12 = func_856C(d12, d11);         /* scale */
        d0  = func_8572(d12, val);         /* final */
    }

    return *(double *)&d0;
}
