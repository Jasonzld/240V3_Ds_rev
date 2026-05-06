/**
 * @file func_92_device_bit15_control.c
 * @brief 函数: 设备位 15 控制 — 置位或清除设备寄存器偏移 +0x44 的位 15
 * @addr  0x080133CE - 0x080133EC (30 bytes)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 参数:
 *   r0 = base — 设备基址
 *   r1 = set  — 非零则置位 bit 15, 零则清除 bit 15
 */

#include "stm32f4xx_hal.h"

void Device_Bit15_Control(volatile uint16_t *base, uint32_t set)
{
    if (set) {
        *(volatile uint16_t *)((uint8_t *)base + 0x44) |= 0x8000;
    } else {
        *(volatile uint16_t *)((uint8_t *)base + 0x44) &= 0x7FFF;
    }
}
