/**
 * @file func_127_fpu_double_op_wrapper.c
 * @brief 函数: FPU double 操作包装器 — VLDR 常量 → 调用 func_089C6 → func_175C4 → 返回 double
 * @addr  0x08016BA0 - 0x08016BCC (44 bytes)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 参数: r0 = uint32_t, 此外从字面量池加载 double 常量
 * 返回: d0 = double 结果
 *
 * 调用:
 *   func_17590() @ 0x08017590 — 前置处理
 *   func_089C6() @ 0x080089C6 — FPU double 运算 (3 参数, 含栈传 double)
 *   func_175C4() @ 0x080175C4 — 后置处理
 */

#include "stm32f4xx_hal.h"

extern uint32_t func_17590(void);
extern void     func_089C6(double *dptr, uint32_t a, uint32_t b);  /* r0=ptr to double on stack */
extern void     func_175C4(uint32_t a);

/* 字面量池常量 */
static const double FPU_CONST_D0 = 0.0;  /* 从 VLDR [pc, #0x24] */

double FPU_Double_Op_Wrapper(uint32_t input)
{
    uint32_t r5;
    double   result;

    r5 = func_17590();

    result = FPU_CONST_D0;          /* VLDR d0 → VSTR d0,[sp] — 存到栈上 */
    func_089C6(&result, input, 0);  /* r0=sp (ptr to double), r1=input, r2=0 */
    func_175C4(r5);

    return result;                  /* VLDR d0,[sp] — 从栈重载返回 */
}
