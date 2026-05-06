/**
 * @file func_74_pll_clock_init.c
 * @brief 函数: PLL 时钟初始化 — 配置系统时钟切换到 PLL
 * @addr  0x0801265C - 0x080126F8 (156 bytes, 含字面量池)
 *         字面量池 0x080126E8-0x080126F7 (16 bytes, 4 个常量)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 标准 STM32F4 PLL 初始化流程:
 *   1. 使能外设时钟 (RCC_AHB1LPENR bit 28)
 *   2. 配置 0x40007000 寄存器 (bits 14-15)
 *   3. 配置 RCC_CFGR (bit 12)
 *   4. 写 PLLCFGR = 0x07016410
 *   5. 使能 PLL (RCC_CR bit 24)
 *   6. 等待 PLL 就绪 (RCC_CR bit 25)
 *   7. 配置 FLASH_ACR = 0x702
 *   8. 切换系统时钟到 PLL (RCC_CFGR bits 0-1 = 0b10)
 *   9. 等待 SWS = PLL (RCC_CFGR bits 2-3 = 0b10)
 *
 * 参数: (无寄存器参数 — 叶函数, bx lr 返回)
 *
 * 调用: (无 — 叶函数)
 */

#include "stm32f4xx_hal.h"

#define RCC_CR        (*(volatile uint32_t *)0x40023800)
#define RCC_PLLCFGR   (*(volatile uint32_t *)0x40023804)
#define RCC_CFGR      (*(volatile uint32_t *)0x40023808)
#define RCC_AHB1LPENR (*(volatile uint32_t *)0x40023840)
#define REG_40007000  (*(volatile uint32_t *)0x40007000)
#define FLASH_ACR     (*(volatile uint32_t *)0x40023C00)

void PLL_Clock_Init(void)
{
    uint32_t tmp;

    /* 1. 使能 AHB1 外设时钟 */
    tmp = RCC_AHB1LPENR;
    tmp |= 0x10000000;
    RCC_AHB1LPENR = tmp;

    /* 2. 配置 0x40007000 寄存器 */
    tmp = REG_40007000;
    tmp |= 0xC000;
    REG_40007000 = tmp;

    /* 3. 配置 RCC_CFGR (读-改-写 设置 bit 12) */
    tmp = RCC_CFGR;
    RCC_CFGR = tmp;         /* 写回原值 */
    tmp = RCC_CFGR;
    tmp |= 0x1000;
    RCC_CFGR = tmp;

    /* 4. 写 PLL 配置 */
    RCC_PLLCFGR = 0x07016410;

    /* 5/6. 使能 PLL 并等待就绪 */
    tmp = RCC_CR;
    tmp |= 0x01000000;
    RCC_CR = tmp;

    do {
        tmp = RCC_CR;
        tmp &= 0x02000000;
    } while (tmp == 0);

    /* 7. 配置 Flash 访问延迟 */
    FLASH_ACR = 0x702;

    /* 8. 切换到 PLL 作为系统时钟 */
    tmp = RCC_CFGR;
    tmp &= ~0x3;
    RCC_CFGR = tmp;
    tmp = RCC_CFGR;
    tmp |= 0x2;
    RCC_CFGR = tmp;

    /* 9. 等待系统时钟切换完成 (SWS = 0b10) */
    do {
        tmp = RCC_CFGR;
        tmp &= 0xC;
    } while (tmp != 8);
}
