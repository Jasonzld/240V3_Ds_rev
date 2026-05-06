/**
 * @file func_99_device_open_configure.c
 * @brief 函数: 设备打开/配置 — func_134A8 设备描述符配置器
 * @addr  0x080134A8 - 0x0801352C (132 bytes, 含字面量池)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 核心设备配置函数，被多个 init 变体调用。
 * 根据设备基址进行特化处理，将配置结构体的值写入设备寄存器。
 *
 * 设备寄存器布局 (相对 base):
 *   +0x00: 控制/状态半字 (读-改-写)
 *   +0x14: 使能标志 (写 1)
 *   +0x28: 波特率/参数半字 (从 cfg[0])
 *   +0x2C: 32 位值 (从 cfg[4])
 *   +0x30: 选项字节 (从 cfg[0xA], 仅特定设备)
 *
 * 待处理设备 ID (特殊位掩码处理):
 *   0x40010000, 0x40010400, 0x40000000, 0x40000400,
 *   0x40000800, 0x40000C00
 *
 * 参数:
 *   r0 = base — 设备基址
 *   r1 = cfg  — 配置结构体指针
 */

#include "stm32f4xx_hal.h"

void Device_Open_Configure(volatile uint32_t *base, const uint16_t *cfg)
{
    uint16_t ctrl;
    uint32_t dev_id = (uint32_t)base;

    ctrl = *(volatile uint16_t *)base;

    /* 特定设备 ID 的位 4-6 处理 */
    if (dev_id == 0x40010000 || dev_id == 0x40010400 ||
        dev_id == 0x40000000 || dev_id == 0x40000400 ||
        dev_id == 0x40000800 || dev_id == 0x40000C00) {
        ctrl &= 0xFF8F;
        ctrl |= cfg[1];   /* +2 */
    }

    /* 特定设备 ID 的位 8-9 处理 */
    if (dev_id == 0x40001000 || dev_id == 0x40001400) {
        /* 跳过 — 直接保留 ctrl */
    } else {
        ctrl &= 0xFCFF;
        ctrl |= cfg[4];   /* +8 */
    }

    *(volatile uint16_t *)base = ctrl;

    /* 写入 32 位配置值 (从 cfg[4], 即 +4 偏移) */
    *(volatile uint32_t *)((uint8_t *)base + 0x2C) = *(uint32_t *)(&cfg[2]);  /* cfg+4 */

    /* 写入波特率/参数半字 (从 cfg[0]) */
    *(volatile uint16_t *)((uint8_t *)base + 0x28) = cfg[0];

    /* 特定设备: 写入选项字节 (LDRB byte load from cfg+0xA, then STRH) */
    if (dev_id == 0x40010000 || dev_id == 0x40010400) {
        *(volatile uint16_t *)((uint8_t *)base + 0x30) = *(const uint8_t *)((const uint8_t *)cfg + 0xA);
    }

    /* 使能设备 (bit 0 at +0x14) */
    *(volatile uint16_t *)((uint8_t *)base + 0x14) = 1;
}
