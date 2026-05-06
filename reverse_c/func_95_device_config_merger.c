/**
 * @file func_95_device_config_merger.c
 * @brief 函数: 设备配置合并器 — func_13420 多字段配置合并到设备寄存器
 * @addr  0x08013420 - 0x0801349C (124 bytes, 含字面量池)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 将配置结构体的多个字段合并写入设备寄存器组:
 *   [base+0x20] ← 合并 cfg[6], cfg[1] (及特殊设备 cfg[7], cfg[2])
 *   [base+0x18] ← 合并 cfg[0]
 *   [base+0x04] ← 仅特殊设备时合并 cfg[8], cfg[9]
 *   [base+0x34] ← cfg[4..5] 32位值 (ldr r5,[r1,#8])
 *
 * 两个特殊设备 ID (0x40010400, 0x40010000) 有额外的位操作。
 *
 * 参数:
 *   r0 = base — 设备基址
 *   r1 = cfg  — 配置结构体指针 (至少 10 个半字字段)
 *
 * 调用: (无 — 叶函数)
 */

#include "stm32f4xx_hal.h"

void Device_Config_Merger(volatile void *base, const uint16_t *cfg)
{
    uint32_t dev_id = (uint32_t)base;
    uint16_t reg_04, reg_18, reg_20;

    /* 0x08013428: LDRH r5,[r0,#0x20]; AND r5,r6(0xFFFE); STRH r5,[r0,#0x20] — 原地清除 bit 0 */
    *(volatile uint16_t *)((uint8_t *)base + 0x20) &= 0xFFFE;

    /* 0x08013432: 重新读取全部寄存器 */
    reg_20 = *(volatile uint16_t *)((uint8_t *)base + 0x20);
    reg_04 = *(volatile uint16_t *)((uint8_t *)base + 0x04);
    reg_18 = *(volatile uint16_t *)((uint8_t *)base + 0x18);

    /* 0x0801343C: AND r4,r5(0xFF8F) — 清除 bits [6:4] */
    reg_18 &= 0xFF8F;
    /* 0x08013440: AND r4,r5(0xFFFC) — 清除 bits [1:0] (0xFFFE - 2) */
    reg_18 &= 0xFFFC;
    /* 0x08013444: ORR r4,r5 — 合并 cfg[0] */
    reg_18 |= cfg[0];

    /* 0x08013448: AND r2,r5(0xFFFD) — 清除 bit 1 (0xFFFE - 1) */
    reg_20 &= 0xFFFD;
    /* 0x0801344C: ORR r2,r5 — 合并 cfg[6] (偏移 +0x0C) */
    reg_20 |= cfg[6];
    /* 0x08013450: ORR r2,r5 — 合并 cfg[1] (偏移 +2) */
    reg_20 |= cfg[1];

    /* 0x08013452-0x0801345C: 特殊设备检查 (0x40010400, 0x40010000) */
    if (dev_id == 0x40010400 || dev_id == 0x40010000) {
        /* 0x08013462: AND r2,r5(0xFFF7) — 清除 bit 3 */
        reg_20 &= 0xFFF7;
        /* 0x08013466: ORR r2,r5 — 合并 cfg[7] (偏移 +0x0E) */
        reg_20 |= cfg[7];
        /* 0x0801346C: AND r2,r5(0xFFFB) — 清除 bit 2 */
        reg_20 &= 0xFFFB;
        /* 0x08013470: ORR r2,r5 — 合并 cfg[2] (偏移 +4) */
        reg_20 |= cfg[2];

        /* 0x08013472-0x08013484: reg_04 仅特殊设备时合并 cfg[8], cfg[9] */
        /* 0x08013476: AND r3,r5(0xFEFF) — 清除 bit 8 */
        reg_04 &= 0xFEFF;
        /* 0x0801347C: AND r3,r5(0xFDFF) — 清除 bit 9 */
        reg_04 &= 0xFDFF;
        /* 0x08013480: ORR r3,r5 — 合并 cfg[8] (偏移 +0x10) */
        reg_04 |= cfg[8];
        /* 0x08013484: ORR r3,r5 — 合并 cfg[9] (偏移 +0x12) */
        reg_04 |= cfg[9];
    }

    /* 0x08013486-0x0801348E: 写回所有寄存器 */
    *(volatile uint16_t *)((uint8_t *)base + 0x04) = reg_04;
    *(volatile uint16_t *)((uint8_t *)base + 0x18) = reg_18;
    *(volatile uint32_t *)((uint8_t *)base + 0x34) = *(uint32_t *)(&cfg[4]);
    *(volatile uint16_t *)((uint8_t *)base + 0x20) = reg_20;
}
