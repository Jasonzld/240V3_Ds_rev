/**
 * @file func_10_system_init_1.c
 * @brief RCC 外设时钟使能/禁用函数 (AHB1 + APB1)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 * @addr  0x080113B8 - 0x080113F8 (64 bytes, 含字面量池)
 *
 * 子函数:
 *   rcc_ahb1_enable @ 0x080113B8 (28B) — RCC_AHB1ENR @ 0x40023830
 *   rcc_apb1_enable @ 0x080113D8 (28B) — RCC_APB1ENR @ 0x40023840
 *
 * 两个完全对称的 RCC 寄存器位操作函数:
 *   r0 = 位掩码 (bitmask)
 *   r1 = 使能标志 (0=禁用/BIC, 非0=使能/ORR)
 *
 * 每个函数执行 read-modify-write 操作:
 *   1. 读取目标 RCC 寄存器当前值
 *   2. 若 r1==0: 清除 r0 对应位 (BIC) → 禁用外设时钟
 *      若 r1!=0: 设置 r0 对应位 (ORR) → 使能外设时钟
 *   3. 写回寄存器
 *
 * RCC 寄存器:
 *   rcc_ahb1_enable → RCC_AHB1ENR (0x40023830)
 *   rcc_apb1_enable → RCC_APB1ENR (0x40023840)
 *
 * 调用示例 (来自 ADC_DMA_Init):
 *   rcc_ahb1_enable(0x00400001, 1);  // 使能 DMA2 (bit22) + GPIOA (bit0) 时钟
 *
 * 注意: 函数间共享 PC 相对常量池。
 *       0x080113F8 起为 Func_SystemInit_2 (APB2ENR + APB2RSTR)。
 */

#include "stm32f4xx_hal.h"

/* RCC 寄存器地址 */
#define RCC_BASE        0x40023800
#define RCC_AHB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x30))
#define RCC_APB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x40))

/* ================================================================
 * rcc_ahb1_enable() @ 0x080113B8
 *   设置/清除 RCC_AHB1ENR 位
 *   r0 = 位掩码, r1 = 使能标志
 *
 * 位定义 (STM32F410 AHB1ENR):
 *   bit0  = GPIOAEN, bit1  = GPIOBEN, bit2  = GPIOCEN
 *   bit4  = GPIOHEN, bit7  = Reserved, bit8  = DMA1EN
 *   bit12 = CRCEN,   bit21 = DMA2EN,  bit23 = FLITFEN
 * ================================================================ */
void rcc_ahb1_enable(uint32_t bitmask, uint32_t enable)
{
    uint32_t val;                  /* r2 */

    if (enable) {
        /* 0x080113B8: cbz r1, #disable */
        val  = RCC_AHB1ENR;        /* 0x080113BA-BC: ldr r2,PC-relative; ldr r2,[r2] */
        val |= bitmask;            /* 0x080113BE: orr r2, r0 */
        RCC_AHB1ENR = val;         /* 0x080113C0-C2: ldr r3,PC-relative; str r2,[r3] */
        /* 0x080113C4: b #return */
    } else {
        val  = RCC_AHB1ENR;        /* 0x080113C6-C8: ldr r2,PC-relative; ldr r2,[r2] */
        val &= ~bitmask;           /* 0x080113CA: bic r2, r0 */
        RCC_AHB1ENR = val;         /* 0x080113CC-CE: ldr r3,PC-relative; str r2,[r3] */
    }
    /* 0x080113D0: bx lr */
}

/* ================================================================
 * rcc_apb1_enable() @ 0x080113D8
 *   设置/清除 RCC_APB1ENR 位
 *   r0 = 位掩码, r1 = 使能标志
 *
 * 位定义 (STM32F410 APB1ENR):
 *   bit0  = TIM2EN,  bit1  = TIM3EN,  bit2  = TIM4EN
 *   bit4  = TIM6EN,  bit5  = TIM7EN,  bit14 = USART2EN
 *   bit17 = I2C1EN,  bit18 = I2C2EN,  bit28 = PWREN
 * ================================================================ */
void rcc_apb1_enable(uint32_t bitmask, uint32_t enable)
{
    uint32_t val;                  /* r2 */

    if (enable) {
        /* 0x080113D8: cbz r1, #disable */
        val  = RCC_APB1ENR;        /* 0x080113DA-DC: ldr r2,PC-relative; ldr r2,[r2] */
        val |= bitmask;            /* 0x080113DE: orr r2, r0 */
        RCC_APB1ENR = val;         /* 0x080113E0-E2: ldr r3,PC-relative; str r2,[r3] */
        /* 0x080113E4: b #return */
    } else {
        val  = RCC_APB1ENR;        /* 0x080113E6-E8: ldr r2,PC-relative; ldr r2,[r2] */
        val &= ~bitmask;           /* 0x080113EA: bic r2, r0 */
        RCC_APB1ENR = val;         /* 0x080113EC-EE: ldr r3,PC-relative; str r2,[r3] */
    }
    /* 0x080113F0: bx lr */
}
