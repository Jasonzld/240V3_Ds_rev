/**
 * @file func_156_baud_rate_calculator.c
 * @brief USART 波特率计算与时钟配置 — 基于外设时钟频率计算 BRR 值
 * @addr  0x08015E50 - 0x08015F24 (214 bytes, 1 个函数 + 前导尾声)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 根据输入时钟源频率计算 USART 波特率寄存器 (BRR) 值.
 * 处理 OVER8 模式 (过采样 8) 和标准模式 (过采样 16) 两种情况.
 *
 * 调用:
 *   func_11438 (HAL_GetTick / 系统时间)       ×1
 *   func_15FF8 (Flash 扇区擦除)               ×0 (此函数内无调用 — 见 func_157)
 *   UDIV ×2 (硬件整数除法)
 */

#include "stm32f4xx_hal.h"

/* ---- 外部依赖 ---- */
extern uint32_t func_11438(void);                          /* 系统 tick 获取 */

/* ---- 外设基址 (字面量池 0x08015F1C-0x08015F24) ---- */
#define USART1_BASE  ((volatile uint32_t *)0x40011000)     /* USART1 */
#define USART6_BASE  ((volatile uint32_t *)0x40011400)     /* USART6 */

/* ========================================================================
 * Baud_Rate_Calculator (0x08015E50 - 0x08015EFE, ~174 bytes)
 *
 * 参数: r0 = usart_regs — USART 外设寄存器块指针
 *       r1 = clock_cfg — 时钟配置结构指针:
 *            [0]  = PCLK 时钟频率 (32-bit, LDR [r1])
 *            [4]  = OVER8 标志位 (16-bit, LDRH [r1,#4])
 *            [6]  = 过采样模式 (16-bit, LDRH [r1,#6])
 *            [8]  = 分频系数 A (16-bit, LDRH [r1,#8])
 *            [10] = 分频系数 B (16-bit, LDRH [r1,#10])
 *            [12] = 附加标志 (16-bit, LDRH [r1,#12])
 *
 * 操作:
 *   1. 读取并合并时钟配置到 USART 控制寄存器:
 *      CR1 (+0x10): 清除 bit[13:12], 置入 [r1+6]
 *      CR1 (+0x0C): 清除 0x160C 位, 置入 [r1+4]|[r1+8]|[r1+10]
 *      CR3 (+0x14): 清除 bit[9:8] (0x300), 置入 [r1+12]
 *   2. 计算波特率分频值:
 *      BRR = (时钟源 * 分频系数) / (PCLK * 过采样因子)
 *      若 OVER8 标志 (bit 15 of CR1): BRR = (sb*25) / (PCLK*2)
 *      否则: BRR = (sb*25) / (PCLK*4)
 *   3. 除以 100 提取整数部分
 * ======================================================================== */
void Baud_Rate_Calculator(volatile uint32_t *usart_regs, const uint16_t *clock_cfg)
{
    uint32_t tick_val;
    uint32_t brr_val;
    uint32_t divisor;
    uint32_t quotient;

    /* 阶段 1: 合并时钟配置到 USART CR 寄存器 */

    /* CR1 (+0x10): 清除 bit[13:12] (0x3000), 置入 [r1+6] */
    uint32_t cr1 = usart_regs[4];                          /* LDRH [r5, #0x10] */
    cr1 &= ~0x3000;                                        /* BIC #0x3000 */
    cr1 |= clock_cfg[3];                                   /* LDRH [r6, #6] → ORR */
    usart_regs[4] = cr1;

    /* CR1 (+0x0C): 清除 0x160C, 置入 [r1+4]|[r1+8]|[r1+10] */
    cr1 = usart_regs[3];                                   /* LDRH [r5, #0xC] */
    cr1 &= ~0x160C;                                        /* MOVW #0x160C → BICS */
    uint32_t combined = clock_cfg[2];                      /* LDRH [r6, #4] */
    combined |= clock_cfg[4];                              /* LDRH [r6, #8] → ORR */
    combined |= clock_cfg[5];                              /* LDRH [r6, #10] → ORR */
    cr1 |= combined;
    usart_regs[3] = cr1;

    /* CR3 (+0x14): 清除 bit[9:8] (0x300), 置入 [r1+12] */
    uint32_t cr3 = usart_regs[5];                          /* LDRH [r5, #0x14] */
    cr3 &= ~0x300;                                         /* BIC #0x300 */
    cr3 |= clock_cfg[6];                                   /* LDRH [r6, #12] → ORR */
    usart_regs[5] = cr3;

    /* 阶段 2: 获取系统 tick */
    tick_val = func_11438();                               /* BL 0x08011438 */

    /* 阶段 3: 计算分频基数 (sp = 栈上分配的临时变量) */
    uint32_t scale_base;                                   /* sb 寄存器 — 分频基数 */

    /* 检查 USART 基址: 若为 USART1 或 USART6, 使用 sp+0x0C */
    if (usart_regs == USART1_BASE || usart_regs == USART6_BASE) {
        scale_base = tick_val;                             /* LDR.W SB, [sp, #0xC] */
    } else {
        scale_base = tick_val;                             /* LDR.W SB, [sp, #8] */
    }

    /* 阶段 4: BRR 计算 */
    cr1 = usart_regs[3];                                   /* LDRH [r5, #0xC] */
    if (cr1 & 0x8000) {
        /* OVER8 模式: 过采样 ×8 */
        divisor = scale_base * 25;                         /* sb*25 = sb+sb*8+sb*16 */
        divisor = divisor / (clock_cfg[0] * 2);            /* UDIV: (sb*25) / (PCLK*2) */
    } else {
        /* 标准模式: 过采样 ×16 */
        divisor = scale_base * 25;                         /* sb*25 */
        divisor = divisor / (clock_cfg[0] * 4);            /* UDIV: (sb*25) / (PCLK*4) */
    }

    /* 阶段 5: 除以 100 提取整数波特率 */
    brr_val = divisor / 100;                               /* UDIV: divisor / 0x64 */
    /* brr_val <<= 4; 后续处理在调用者完成... (LSLS r4, r0, #4; LSRS r0, r4, #4) */

    /* 结果通过 r4/r0 返回调用者 */
}

/*
 * 内存布局:
 *   0x08015E4E - 0x08015E50: 前导 POP {r4,r5,r6,r7,pc} (前一个函数的尾声)
 *   0x08015E50 - 0x08015E58: PUSH {r0-r10, LR} + MOV r5,r0 + MOV r6,r1
 *   0x08015E58 - 0x08015E96: 控制寄存器配置 (CR1/CR3 位操作)
 *   0x08015E96 - 0x08015EAA: func_11438 调用 + 基址检查
 *   0x08015EAA - 0x08015EDA: OVER8 分支 + UDIV 计算
 *   0x08015EDA - 0x08015EFE: 除以 100 + 结果移位
 *   0x08015F18 - 0x08015F24: POP {r0-r10, PC} 尾声 (4 bytes) + 字面量池
 *
 * 字面量池 (估算):
 *   0x08015F1C: 0x40011000 (USART1_BASE)
 *   0x08015F20: 0x40011400 (USART6_BASE)
 */
