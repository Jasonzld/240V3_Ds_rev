/**
 * @file func_61_register_config_copier.c
 * @brief 寄存器配置复制器 — 合并源寄存器半字到目标寄存器
 * DISASSEMBLY-TRACED. Verified against Capstone.
 * @addr  0x0801197E - 0x080119BC (62 bytes)
 *
 * 从源结构 (8 个半字 at +0/+2/+4/+6/+8/+0xA/+0xC/+0xE)
 * 读取配置值，保留目标寄存器的特定位 (0x3040 掩码)，
 * 合并写入目标，并更新 +0x10 和 +0x1C 偏移。
 *
 * 参数:
 *   r0 = dst — 目标寄存器基址
 *   r1 = src — 源配置基址
 */

#include "stm32f4xx_hal.h"

void Register_Config_Copier(volatile uint16_t *dst, const volatile uint16_t *src)
{
    uint32_t merged;
    uint32_t src_or;
    uint16_t tmp;

    /* 保留目标特定位 (mask 0x3040) */
    merged = (uint32_t)(*dst) & 0x3040;      /* ldrh; and #0x3040 */

    /* 合并源的所有半字 */
    src_or  = src[0];                        /* offset 0 */
    src_or |= src[1];                        /* +2 */
    src_or |= src[2];                        /* +4 */
    src_or |= src[3];                        /* +6 */
    src_or |= src[4];                        /* +8 */
    src_or |= src[5];                        /* +0xA */
    src_or |= src[6];                        /* +0xC */
    src_or |= src[7];                        /* +0xE */

    merged |= src_or;
    *dst = (uint16_t)merged;                 /* strh */

    /* 清除 dst+0x1C 的 bit 11 */
    tmp = dst[0x1C / 2];                     /* ldrh [r0, #0x1C] */
    tmp &= 0xF7FF;                           /* and r3, r4 (#0xF7FF) */
    dst[0x1C / 2] = tmp;                     /* strh */

    /* 复制 src+0x10 → dst+0x10 */
    dst[0x10 / 2] = src[0x10 / 2];           /* ldrh r3 → strh */
}
