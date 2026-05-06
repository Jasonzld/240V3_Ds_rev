/**
 * @file func_57_flash_all_blocks_reader.c
 * @brief 全块闪存读取器 — 遍历并处理所有闪存块
 * DISASSEMBLY-TRACED. Verified against Capstone.
 * @addr  0x08011610 - 0x080116D0 (192 bytes)
 *
 * 读取 0x20000194 中存储的总块数，遍历所有块进行
 * 读取/验证操作。每10个块调用一次 func_D730() 喂狗。
 *
 * 两个路径:
 *   A: 总块数 <= 0x1FE00 → 从块0开始递增处理
 *   B: 总块数 >  0x1FE00 → 从 (总块数-0x1FE00) 开始处理溢出块
 */

#include "stm32f4xx_hal.h"

#define REG_BLKCNT  (*(volatile uint32_t *)0x20000194)
#define FLAG_1BE    (*(volatile uint8_t  *)0x200001BE)

extern void func_15FF8(uint8_t *buf, uint32_t addr, uint32_t len);
extern void func_15BE4(uint8_t *buf, uint32_t len);
extern void func_D730(void);

void Flash_All_Blocks_Reader(void)
{
    uint8_t  buf[0x100];     /* sp+0x04 */
    uint32_t total_blocks;
    uint32_t i;
    uint32_t addr;
    uint32_t offset;

    FLAG_1BE = 0;
    total_blocks = REG_BLKCNT;

    /* ---- 路径 A: total_blocks <= 0x1FE00 ---- */
    if (total_blocks <= 0x1FE00) {
        for (i = 0; i < total_blocks; i++) {
            offset = i % 0x1FE00;              /* SDIV + MLS */
            addr = 0x10000 + offset * 128;     /* ADD.W r5, r1, r0, LSL #7 */

            func_15FF8(buf, addr, 0x57);       /* 读取 */
            func_15BE4(buf, 0x57);             /* 验证 */

            if (i % 10 == 0) {                 /* 每10块 */
                func_D730();                   /* 喂狗 */
            }
        }
    } else {
        /* ---- 路径 B: 从溢出起点开始处理 ---- */
        uint32_t start = (uint32_t)(uint16_t)(total_blocks - 0x1FE00);

        for (i = start; i < total_blocks; i++) {
            offset = i % 0x1FE00;
            addr = 0x10000 + offset * 128;

            func_15FF8(buf, addr, 0x57);
            func_15BE4(buf, 0x57);

            if (total_blocks % 10 == 0) {      /* 使用总块数, 非 i */
                func_D730();
            }
        }
    }

    FLAG_1BE = 1;
}
