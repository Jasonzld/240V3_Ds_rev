/**
 * @file func_78_systick_clksource_config.c
 * @brief 函数: SysTick 时钟源配置 — 根据模式设置 SysTick CSR 的 CLKSOURCE 位
 * @addr  0x08012886 - 0x080128AE (40 bytes)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 访问 SysTick 控制寄存器 0xE000E010 的位 2 (CLKSOURCE):
 *   mode == 4: 置位 bit 2
 *   mode != 4: 清除 bit 2
 *
 * 参数:
 *   r0 = mode — 模式值
 *
 * 调用: (无 — 叶函数)
 */

#include "stm32f4xx_hal.h"

#define SYST_CSR  (*(volatile uint32_t *)0xE000E010)

void SysTick_ClkSource_Config(uint32_t mode)
{
    if (mode == 4) {
        SYST_CSR |= 0x4;
    } else {
        SYST_CSR &= ~0x4;
    }
}
