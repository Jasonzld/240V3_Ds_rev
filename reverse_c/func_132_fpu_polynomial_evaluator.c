/**
 * @file func_132_fpu_polynomial_evaluator.c
 * @brief 函数: FPU 多项式求值器 — 霍纳法求多项式 P(x) = c[0] + c[1]*x + c[2]*x^2 + ...
 * @addr  0x08017310 - 0x08017408 (248 bytes)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 参数:
 *   r0 = coeffs — 多项式系数数组基址 (每个 8 字节 double)
 *   r1 = n — 多项式阶数
 *   s0,s1 = x — 自变量 (double)
 *
 * 体系结构验证:
 *   VPUSH {d8} — 1 个双精度寄存器 (8 字节栈空间)
 *   VPOP  {d8} — 对应恢复
 *
 * 使用霍纳法: 从最高次系数开始累加:
 *   result = c[n-1]
 *   对 i = n-2 到 0: result = result * x + c[i]
 *
 * 主循环按 8 项一组处理 (只当 i 的位 0,1,2 不全为零, 即 i>=8 时进入).
 * 尾部级联: 若 i>=6 → 处理 c[5],c[4]; 若 i>=4 → 处理 c[3],c[2]; 若 i>=2 → 处理 c[1],c[0].
 *
 * 调用:
 *   func_8578() @ 0x08008578 — VMUL.F64  (BL at 0x08017338, 0x0801737A, 0x0801738E, ...)
 *   func_842A() @ 0x0800842A — VADD.F64  (BL at 0x08017352, 0x08017386, 0x0801739A, ...)
 */

#include "stm32f4xx_hal.h"

extern int64_t func_8578(int64_t a, int64_t b);  /* mul */
extern int64_t func_842A(int64_t a, int64_t b);  /* add/sub */

double FPU_Polynomial_Evaluator(const double *coeffs, uint32_t n, double x)
{
    int64_t d0, d1, d8;
    uint32_t i;

    d8 = *(int64_t *)&x;
    i  = n - 1;

    /* 加载 c[n-1] (最高次系数) — VLDR d0, [r4 + i*8] */
    d0 = *(int64_t *)&coeffs[i];

    /* 主循环: 每次迭代乘 x, 然后用下一个系数累加.
     * 循环条件: while (i & ~6), 即 i 不在 {0,2,4,6} 时继续.
     * 第一次检查在进入循环体之前 (beq 跳过循环). */
    while (i & ~6) {
        d0 = func_8578(d0, d8);                       /* result *= x */
        i--;
        d1 = *(int64_t *)&coeffs[i];
        d0 = func_842A(d0, d1);                       /* result += c[i] */
    }

    /* 尾部级联处理 (根据循环退出时的 i 值):
     *   i==6: 处理 c[5],c[4] → 落入 i==4 块
     *   i==4: 处理 c[3],c[2] → 落入 i==2 块
     *   i==2: 处理 c[1],c[0] → 返回
     *   i==0: 直接返回 (vpopne/popne) */

    if (i >= 6) {
        /* VLDR d1, [r4, #0x28] — c[5]; VLDR d1, [r4, #0x20] — c[4] */
        d0 = func_8578(d0, d8);
        d1 = *(int64_t *)&coeffs[5];
        d0 = func_842A(d0, d1);
        d0 = func_8578(d0, d8);
        d1 = *(int64_t *)&coeffs[4];
        d0 = func_842A(d0, d1);
    }

    if (i >= 4) {
        /* VLDR d1, [r4, #0x18] — c[3]; VLDR d0, [r4, #0x10] — c[2] */
        d0 = func_8578(d0, d8);
        d1 = *(int64_t *)&coeffs[3];
        d0 = func_842A(d0, d1);
        d0 = func_8578(d0, d8);
        d1 = *(int64_t *)&coeffs[2];
        d0 = func_842A(d0, d1);
    }

    if (i >= 2) {
        /* VLDR d1, [r4, #8] — c[1]; VLDR d1, [r4] — c[0] */
        d0 = func_8578(d0, d8);
        d1 = *(int64_t *)&coeffs[1];
        d0 = func_842A(d0, d1);
        d0 = func_8578(d0, d8);
        d1 = *(int64_t *)&coeffs[0];
        d0 = func_842A(d0, d1);
    }

    return *(double *)&d0;
}
