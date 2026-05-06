/**
 * @file func_96_device_reg_store_2c.c
 * @brief 函数: 设备寄存器 32 位存储 — 将值写入设备偏移 +0x2C
 * @addr  0x0801349C - 0x080134A0 (4 bytes, 叶函数)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 参数:
 *   r0 = base — 设备基址
 *   r1 = val  — 32 位值
 */

#include "stm32f4xx_hal.h"

void Device_Reg_Store_2C(volatile uint32_t *base, uint32_t val)
{
    *(volatile uint32_t *)((uint8_t *)base + 0x2C) = val;
}
