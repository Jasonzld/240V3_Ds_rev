/**
 * @file func_11_system_init_2.c
 * @brief RCC 外设时钟使能/禁用 + 复位控制 (APB2)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 * @addr  0x080113F8 - 0x08011438 (64 bytes, includes literal pool)
 *
 * 子函数:
 *   rcc_apb2_enable @ 0x080113F8 (28B) — RCC_APB2ENR @ 0x40023844
 *   rcc_apb2_reset  @ 0x08011418 (28B) — RCC_APB2RSTR @ 0x40023824
 *
 * 两个 RCC 寄存器位操作函数:
 *   r0 = 位掩码 (bitmask)
 *   r1 = 使能/复位标志 (0=禁用/清除, 非0=使能/设置)
 *
 * 每个函数执行 read-modify-write 操作:
 *   1. 读取目标 RCC 寄存器当前值
 *   2. 若 r1==0: 清除 r0 对应位 (BIC)
 *      若 r1!=0: 设置 r0 对应位 (ORR)
 *   3. 写回寄存器
 *
 * RCC 寄存器:
 *   rcc_apb2_enable → RCC_APB2ENR  (0x40023844) — APB2 外设时钟使能
 *   rcc_apb2_reset  → RCC_APB2RSTR (0x40023824) — APB2 外设复位控制
 *
 * 调用示例 (来自 ADC_DMA_Init):
 *   rcc_apb2_enable(0x0100, 1);  // 使能 ADC1 (bit8) 时钟
 *
 * 注意: 0x080113B8 起的 rcc_ahb1_enable 和 rcc_apb1_enable
 *       见 func_10_system_init_1.c。
 */

#include "stm32f4xx_hal.h"

/* RCC 寄存器地址 */
#define RCC_BASE        0x40023800
#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x44))
#define RCC_APB2RSTR    (*(volatile uint32_t *)(RCC_BASE + 0x24))

/* ================================================================
 * rcc_apb2_enable() @ 0x080113F8
 *   设置/清除 RCC_APB2ENR 位
 *   r0 = 位掩码, r1 = 使能标志
 *
 * 位定义 (STM32F410 APB2ENR):
 *   bit0  = TIM1EN,  bit4  = USART6EN, bit8  = ADC1EN
 *   bit12 = SPI1EN,  bit14 = SYSCFGEN, bit16 = TIM9EN
 * ================================================================ */
void rcc_apb2_enable(uint32_t bitmask, uint32_t enable)
{
    uint32_t val;                  /* r2 */

    if (enable) {
        /* 0x080113F8: cbz r1, #disable */
        val  = RCC_APB2ENR;        /* 0x080113FA-FC: ldr r2,PC-relative; ldr r2,[r2] */
        val |= bitmask;            /* 0x080113FE: orr r2, r0 */
        RCC_APB2ENR = val;         /* 0x08011400-02: ldr r3,PC-relative; str r2,[r3] */
        /* 0x08011404: b #return */
    } else {
        val  = RCC_APB2ENR;        /* 0x08011406-08: ldr r2,PC-relative; ldr r2,[r2] */
        val &= ~bitmask;           /* 0x0801140A: bic r2, r0 */
        RCC_APB2ENR = val;         /* 0x0801140C-0E: ldr r3,PC-relative; str r2,[r3] */
    }
    /* 0x08011410: bx lr */
}

/* ================================================================
 * rcc_apb2_reset() @ 0x08011418
 *   设置/清除 RCC_APB2RSTR 位 (外设复位控制)
 *   r0 = 位掩码, r1 = 复位标志
 *
 * 位定义 (STM32F410 APB2RSTR):
 *   与 APB2ENR 位对应相同, 写 1 复位对应外设
 *
 * 使用方式:
 *   rcc_apb2_reset(bitmask, 1);  // 复位对应 APB2 外设
 *   rcc_apb2_reset(bitmask, 0);  // 释放复位
 * ================================================================ */
void rcc_apb2_reset(uint32_t bitmask, uint32_t reset)
{
    uint32_t val;                  /* r2 */

    if (reset) {
        /* 0x08011418: cbz r1, #disable */
        val  = RCC_APB2RSTR;       /* 0x0801141A-1C: ldr r2,PC-relative; ldr r2,[r2] */
        val |= bitmask;            /* 0x0801141E: orr r2, r0 */
        RCC_APB2RSTR = val;        /* 0x08011420-22: ldr r3,PC-relative; str r2,[r3] */
        /* 0x08011424: b #return */
    } else {
        val  = RCC_APB2RSTR;       /* 0x08011426-28: ldr r2,PC-relative; ldr r2,[r2] */
        val &= ~bitmask;           /* 0x0801142A: bic r2, r0 */
        RCC_APB2RSTR = val;        /* 0x0801142C-2E: ldr r3,PC-relative; str r2,[r3] */
    }
    /* 0x08011430: bx lr */
}
