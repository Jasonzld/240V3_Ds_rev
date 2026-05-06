/**
 * @file func_90_device_reg_invert_write.c
 * @brief 函数: 设备寄存器 AND+取反写入 — (r0 & r1) 取反后写入 [r0+0x10]
 * @addr  0x080133AE - 0x080133B6 (8 bytes)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 参数:
 *   r0 = base | mask — 设备基址 AND 掩码
 *   r1 = val  — 输入值
 */

#include "stm32f4xx_hal.h"

void Device_Reg_And_Invert_Write(volatile uint32_t *base, uint32_t val)
{
    /* 0x080133AE: ANDS r1, r0       ; r1 = r1 & r0 (bitwise AND)
     * 0x080133B0: MVNS r2, r1       ; r2 = ~r1 (bitwise NOT)
     * 0x080133B2: STRH r2, [r0,#0x10] ; [r0+0x10] = r2 (halfword store)
     * 0x080133B4: BX LR
     */
    *(volatile uint16_t *)((uint8_t *)base + 0x10) = (uint16_t)(~(val & (uint32_t)base));
}
