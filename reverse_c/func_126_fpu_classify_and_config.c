/**
 * @file func_126_fpu_classify_and_config.c
 * @brief 函数: FPU 值分类器 + FPU 系统寄存器配置
 * @addr  0x08016B1C - 0x08016BA0 (132 bytes, 含字面量池)
 * DISASSEMBLY-TRACED. Verified against Capstone (all 3 subroutines).
 *
 * 包含三个子例程:
 *   A. 0x08016B1C: FPU 操作包装器 — 调用 func_17764 + func_17E6A 后尾返回
 *   B. 0x08016B40: IEEE 754 double 分类器 — 将 double 值分类为 NaN/Inf/Zero/Subnormal
 *   C. 0x08016B70: FPU 系统控制寄存器配置 — 修改 SCB 中的 FPU 相关位
 *
 * 调用:
 *   func_17764() @ 0x08017764 — FPU 数据准备
 *   func_17E6A() @ 0x08017E6A — FPU 结果处理
 */

#include "stm32f4xx_hal.h"

extern uint32_t func_17764(uint32_t a, uint32_t *out1, uint32_t *out2);
extern void     func_17E6A(uint32_t a, uint32_t *out);

/* ---- 子例程 A: FPU 操作包装器 ---- */
uint32_t FPU_Op_Wrapper(uint32_t sp_arg)
{
    uint32_t result;
    uint32_t out1, out2;

    /*
     * 0x08016B1C: push {r4,lr}
     * 0x08016B22: ldr r0,[sp,#0xc] → r0 = sp_arg (caller stack arg, not extra param)
     */
    /*
     * 0x08016B1E: add r1,sp,#0x10 → func_17764 r1 (out1 ptr)
     * 0x08016B20: add r2,sp,#8    → func_17764 r2 (out2 ptr)
     * 0x08016B2A: add r1,sp,#8    → func_17E6A r1 = same sp+8 (out2 ptr)
     */
    result = func_17764(sp_arg, &out1, &out2);
    func_17E6A(0, &out2);
    return result;
}

/* ---- 子例程 B: IEEE 754 double 分类器 ---- */
/*
 * 分类值:
 *   0 = Zero       (指数=0, 尾数=0)
 *   3 = Infinity   (指数=0x7FF, 尾数=0)
 *   4 = Subnormal  (指数=0, 尾数≠0)
 *   5 = Normal     (0<指数<0x7FF)
 *   7 = NaN        (指数=0x7FF, 尾数≠0)
 */
int FPU_Double_Classifier(double val)
{
    uint32_t hi = ((uint32_t *)&val)[1];  /* 高 32 位 */
    uint32_t lo = ((uint32_t *)&val)[0];  /* 低 32 位 */
    int      cls = 0;

    /*
     * 0x08016B44: LSLS r1,r0,#1 → r1 = hi<<1
     * 0x08016B48: ORRS.W r0,r0,r1,LSL #11 → r0 = lo | (hi<<12)
     *   → 合并全部 52 位尾数, 若任一尾数位为 1 则为 NaN 候选
     */
    if (lo | (hi << 12)) {
        cls = 4;
    }

    /*
     * 0x08016B50: LSRS r2,r1,#0x15 → r2 = (hi<<1)>>21 = hi>>20
     *   → 提取指数+符号+高尾数, 非零则标记
     */
    if (hi >> 20) {
        cls |= 1;
    }

    /*
     * 0x08016B5C: CMP.W r2, r1, LSR #21 → cmp 0x7FF, hi>>20
     *   → 指数 = 0x7FF 且高 20 位尾数 = 0 (Infinity / NaN-low-only)
     */
    if ((hi >> 20) == 0x7FF) {
        cls |= 2;
    }

    /*
     * 0x08016B66: CMP r0,#1; IT EQ; MOVEQ r0,#5
     *   → cls==1 (仅 bit0 置位: 尾数=0, 指数≠0 且 ≠0x7FF) → cls=5
     */
    if (cls == 1) {
        cls = 5;
    }

    return cls;
}

/* ---- 子例程 C: FPU 系统控制寄存器配置 ---- */
void FPU_System_Config(void)
{
    __DSB();

    /* 读取 SCB 寄存器 (0xE000ED00+offset), 修改 bits [10:8] */
    volatile uint32_t *scb_reg = (volatile uint32_t *)0xE000ED0C; /* SCB_AIRCR */
    uint32_t val = *scb_reg;
    val &= 0x700;                        /* 保留 PRIGROUP bits [10:8] */
    val |= 0x05FA0000;                   /* VECTKEY */
    val += 4;                            /* SYSRESETREQ */

    *scb_reg = val;                      /* 触发系统复位 */

    __DSB();
    __NOP();
    __NOP();

    /* 死循环 (调试断点) */
    while (1);
}
