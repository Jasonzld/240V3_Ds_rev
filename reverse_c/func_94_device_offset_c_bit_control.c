/**
 * @file func_94_device_offset_c_bit_control.c
 * @brief 函数: 设备偏移 +0x0C 位控制 — 置位或清除指定掩码位
 * @addr  0x0801340E - 0x08013420 (18 bytes)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 参数:
 *   r0 = base — 设备基址
 *   r1 = mask — 位掩码
 *   r2 = set  — 非零则置位 mask 位, 零则清除
 */

#include "stm32f4xx_hal.h"

void Device_OffsetC_Bit_Control(volatile uint32_t *base, uint32_t mask, uint32_t set)
{
    uint16_t reg = *(volatile uint16_t *)((uint8_t *)base + 0x0C);

    if (set) {
        reg |= (uint16_t)mask;
    } else {
        reg &= ~((uint16_t)mask);
    }

    *(volatile uint16_t *)((uint8_t *)base + 0x0C) = reg;
}
