/**
 * @file func_91_device_bit0_control.c
 * @brief 函数: 设备位 0 控制 — 置位或清除设备寄存器偏移 +0x00 的位 0
 * @addr  0x080133B6 - 0x080133CE (24 bytes)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 参数:
 *   r0 = base — 设备基址
 *   r1 = set  — 非零则置位 bit 0, 零则清除 bit 0
 */

#include "stm32f4xx_hal.h"

/*
 * 0x080133B6: CBZ r1 → if (set) goto set_path; else goto clear_path
 * 0x080133BA: ORR r2,r2,#1 → set bit 0
 * 0x080133C8: AND r2,r3 (0xFFFE) → clear bit 0
 */
void Device_Bit0_Control(volatile uint16_t *base, uint32_t set)
{
    if (set) {
        *base |= 1;
    } else {
        *base &= 0xFFFE;
    }
}
