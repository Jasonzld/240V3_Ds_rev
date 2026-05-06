/**
 * @file func_93_device_status_check.c
 * @brief 函数: 设备状态检查 — 检查两个设备寄存器偏移的掩码匹配
 * @addr  0x080133EC - 0x0801340E (34 bytes)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 读取 base+0x10 和 base+0x0C 两个半字，分别与 mask 按位与。
 * 若两者结果均非零则返回 1，否则返回 0。
 *
 * 参数:
 *   r0 = base — 设备基址
 *   r1 = mask — 检查掩码
 * 返回:
 *   r0 = 1 (两个标志均有效) 或 0
 */

#include "stm32f4xx_hal.h"

uint32_t Device_Status_Check(volatile uint32_t *base, uint32_t mask)
{
    uint16_t flag_a = *(volatile uint16_t *)((uint8_t *)base + 0x10);
    uint16_t flag_b = *(volatile uint16_t *)((uint8_t *)base + 0x0C);

    if ((flag_a & mask) && (flag_b & mask)) {
        return 1;
    }
    return 0;
}
