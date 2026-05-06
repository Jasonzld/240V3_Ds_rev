/**
 * @file func_134_fpu_helper_cluster.c
 * @brief 函数: FPU 辅助函数簇 — 7 个子操作: 自加、常数缩放、常数乘、取状态、内存复制/清零、位检查
 * @addr  0x08017538 - 0x080175D0 (152 bytes, 含字面量池)
 * DISASSEMBLY-TRACED. Verified against Capstone (all 7 subroutines).
 *
 * 子函数:
 *   A. 0x08017538: d0 = d0 + d0  (VADD self, 使用输入参数)
 *   B. 0x08017550: VLDR 从池加载常量 (0.0), 然后 func_865C(const, const) — 忽略输入参数
 *   C. 0x08017570: VLDR 从池加载常量, 然后 func_8578(const, const) — 忽略输入参数
 *   D. 0x08017590: LDR pool → LDR [r0] — 返回 RAM 变量 0x200002F8 的值
 *   E. 0x0801759E: 内存复制循环 — LDM r0!,{r3}; STM r1!,{r3}; 4 字节/次
 *   F. 0x080175B0: 内存清零循环 — STM r1!,{r0}; 4 字节/次 (r0 保持为 0)
 *   G. 0x080175BA: AND r1, r0, #1 — 低位检查 (注意返回在 r1)
 *
 * 体系结构验证:
 *   无 VPUSH/VPOP (本函数簇不使用浮点寄存器栈帧保存).
 *   VLDR 池: 0x08017568 = 0.0 (double), 0x08017588 = denorm (~2^-767).
 */

#include "stm32f4xx_hal.h"

extern int64_t func_842A(int64_t a, int64_t b);
extern int64_t func_865C(int64_t a, int64_t b);
extern int64_t func_8578(int64_t a, int64_t b);

/* ---- A: d0 = d0 + d0 (使用输入 d0, 直接传递给 VADD) ---- */
double FPU_Double_Self_Add(double val)
{
    int64_t d0 = *(int64_t *)&val;
    d0 = func_842A(d0, d0);
    return *(double *)&d0;
}

/* ---- B: VLDR 加载池常量 0.0, 然后 func_865C(0.0, 0.0) — 输入 val 未使用 ---- */
double FPU_Double_Const_Scale(double val)
{
    /* VLDR d0, [pc, #0x14] — 从池 0x08017568 加载 double 0.0 */
    int64_t d0 = 0;                         /* 0x0000000000000000 = +0.0 */
    d0 = func_865C(d0, d0);                 /* VSCALE(0.0, 0.0) */
    return *(double *)&d0;
}

/* ---- C: VLDR 加载池常量, 然后 func_8578(const, const) — 输入 val 未使用 ---- */
double FPU_Double_Const_Mul(double val)
{
    /* VLDR d0, [pc, #0x14] — 从池 0x08017588 加载 double 常量 */
    int64_t d0 = 0x1000000000000000ULL;      /* 池字面量: denorm ≈ 2^-767 */
    d0 = func_8578(d0, d0);                 /* VMUL(const, const) */
    return *(double *)&d0;
}

/* ---- D: LDR pool → LDR [r0] — 返回 RAM 变量 0x200002F8 的值 ---- */
uint32_t FPU_Get_Status(void)
{
    return *(volatile uint32_t *)0x200002F8;
}

/* ---- E: 内存复制 — LDM r0!,{r3}; STM r1!,{r3}; 每次 4 字节, r2 为字节计数 ---- */
void FPU_Word_Copy(uint32_t *dst, const uint32_t *src, uint32_t bytes)
{
    while (bytes > 0) {
        *dst++ = *src++;
        bytes -= 4;
    }
}

/* ---- F: 内存清零 — STM r1!,{r0} (r0 保持 0), r2 为字节计数 ---- */
void FPU_Word_Zero(uint32_t *dst, uint32_t bytes)
{
    while (bytes > 0) {
        *dst++ = 0;
        bytes -= 4;
    }
}

/* ---- G: AND r1, r0, #1 — 低位检查 (结果在 r1, 然后 bx lr) ---- */
uint32_t FPU_Low_Bit_Check(uint32_t val)
{
    return val & 1;
}
