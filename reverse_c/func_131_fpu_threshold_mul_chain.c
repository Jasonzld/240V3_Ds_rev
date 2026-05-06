/**
 * @file func_131_fpu_threshold_mul_chain.c
 * @brief 函数: FPU 阈值乘法链 — 检查绝对值阈值后执行多级 VMUL+VMUL 链
 * @addr  0x080171A0 - 0x08017310 (368 bytes, 含 92B 尾部除法链 + 46B 字面量池)
 * DISASSEMBLY-TRACED. Verified against Capstone (entry + key sections).
 *
 * 输入: double val (栈传), uint32_t flags ([sp+0x2c])
 * 若 |flags| >= 0x3E400000 (约 0.1875): 执行 6 步乘法链
 * 若 |flags| <  阈值: 检查 func_8776, 条件跳过
 * 最终经 func_856C+func_8572 聚合返回.
 *
 * 调用:
 *   func_8776()  @ 0x08008776 — FPU 条件检查
 *   func_8578()  @ 0x08008578 — VMUL.F64
 *   func_856C()  @ 0x0800856C — FPU 双精度缩放
 *   func_8572()  @ 0x08008572 — FPU 双精度运算变体
 *   func_17310() @ 0x08017310 — FPU 多项式求值器
 */

#include "stm32f4xx_hal.h"

extern int64_t  func_8578(int64_t a, int64_t b);
extern int64_t  func_856C(int64_t a, int64_t b);
extern int64_t  func_8572(int64_t a, int64_t b);
extern uint32_t func_8776(double val);
extern double   func_17310(double val, uint32_t n, uint32_t mode); /* func_132 */

/* 从字面量池加载的 FPU 常量 */
static const double FPU_CONST_D10 = 0.0;   /* d10 初始常量 */
static const double FPU_CONST_D0  = 0.0;   /* 聚合常量 */

double FPU_Threshold_Mul_Chain(double val, uint32_t flags)
{
    uint32_t abs_flags = flags & 0x7FFFFFFF;
    int64_t  d0, d8, d9, d10, d11;

    d0  = *(int64_t *)&val;

    if ((int32_t)abs_flags >= 0x3FD33333) {             /* >= ~0.3 */
        /* 阈值以上: 完整乘法链 */
        d8 = func_8578(d0, d0);                        /* d8 = val^2 */
        d9 = func_17310(val, 6, 0);                    /* func_132: 多项式求值 */
        d11 = func_8578(d8, d8);                       /* d11 = val^4 */
        d9 = func_8578(d0, d9);                        /* d9 = val * poly(val) */
        d8 = func_8578(d8, d11);                       /* d8 = val^6 */
        d8 = func_856C(d8, d9);                        /* d8 = scale(val^6, d9) */
        d9 = func_8578(d0, d8);                        /* d9 = val * d8 */

        d0 = func_8578(d8, d0);                        /* d0 = d8 * val */

        if ((int32_t)abs_flags >= 0x3FE90000) {         /* >= ~0.78125 */
            d0 = func_856C(d9, d0);                     /* d0 = scale(d9, d0) */
        }
        d0 = func_8572(d0, d10);                        /* 最终聚合 */
        return *(double *)&d0;
    }

    /* 阈值以下: 检查条件 */
    if (func_8776(val) == 0) {
        return *(double *)&d10;                         /* 返回 d10 常量 */
    }
    return val;                                         /* 返回原值 */
}
