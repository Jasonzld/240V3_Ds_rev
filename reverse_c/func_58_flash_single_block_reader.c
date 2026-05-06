/**
 * @file func_58_flash_single_block_reader.c
 * @brief 单块闪存读取器 — 读取指定索引的单个闪存块
 * DISASSEMBLY-TRACED. Verified against Capstone.
 * @addr  0x080116D0 - 0x08011710 (64 bytes)
 *
 * 读取并验证单个闪存块 (索引 = r0)。
 * 设置 FLAG_1BE = 1 标记完成。
 *
 * 参数:
 *   r0 = block_index — 块索引
 */

#include "stm32f4xx_hal.h"

#define FLAG_1BE    (*(volatile uint8_t  *)0x200001BE)
#define BUF_SP      ((volatile uint8_t  *)0x20010000)    /* 伪地址 — 栈缓冲区 */

extern void func_15FF8(uint8_t *buf, uint32_t addr, uint32_t len);
extern void func_15BE4(uint8_t *buf, uint32_t len);

void Flash_Single_Block_Reader(uint32_t block_index)
{
    uint8_t  buf[0x100];     /* sp+0x04 — 256 字节缓冲区 */
    uint32_t offset;
    uint32_t addr;

    FLAG_1BE = 0;                          /* 清零完成标志 */

    /* 计算块偏移: block_index % 0x1FE00, 映射到 0x10000 + offset*128 */
    offset = block_index % 0x1FE00;        /* UDIV + MLS */
    addr = 0x10000 + offset * 128;         /* ADD.W r5, r1, r0, LSL #7 */

    func_15FF8(buf, addr, 0x57);           /* 读取块 87 字节 */
    func_15BE4(buf, 0x57);                 /* 验证块 */

    FLAG_1BE = 1;                          /* 标记完成 */
}
