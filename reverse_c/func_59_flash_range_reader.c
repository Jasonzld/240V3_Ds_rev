/**
 * @file func_59_flash_range_reader.c
 * @brief 范围闪存读取器 — 处理指定范围的闪存块
 * DISASSEMBLY-TRACED. Verified against Capstone.
 * @addr  0x08011710 - 0x08011770 (96 bytes)
 *
 * 遍历 [start_index, end_index] 范围内的闪存块，
 * 每10个块调用 func_D730() 喂狗。
 *
 * 参数:
 *   r0 = start_index — 起始块索引
 *   r1 = end_index   — 结束块索引
 */

#include "stm32f4xx_hal.h"

#define FLAG_1BE    (*(volatile uint8_t  *)0x200001BE)

extern void func_15FF8(uint8_t *buf, uint32_t addr, uint32_t len);
extern void func_15BE4(uint8_t *buf, uint32_t len);
extern void func_D730(void);

void Flash_Range_Reader(uint32_t start_index, uint32_t end_index)
{
    uint8_t  buf[0x100];     /* sp+0x04 */
    uint32_t i;
    uint32_t addr;
    uint32_t offset;

    FLAG_1BE = 0;

    /*
     * 0x0801171E: UXTH r4,r5 — 初始截断为 16 位
     * 0x0801175A: UXTH r4,r0 — 每次递增后截断为 16 位
     */
    for (i = (uint16_t)start_index; i <= end_index; i = (uint16_t)(i + 1)) {
        offset = i % 0x1FE00;                  /* SDIV + MLS */
        addr = 0x10000 + offset * 128;         /* ADD.W r7, r1, r0, LSL #7 */

        func_15FF8(buf, addr, 0x57);
        func_15BE4(buf, 0x57);

        if (i % 10 == 0) {
            func_D730();
        }
    }

    FLAG_1BE = 1;
}
