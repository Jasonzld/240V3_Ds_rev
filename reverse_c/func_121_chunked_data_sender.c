/**
 * @file func_121_chunked_data_sender.c
 * @brief 函数: 分块数据发送器 — 将大数据分块 (最多 256 字节/块) 逐块发送
 * @addr  0x080161A8 - 0x080161EE (72 bytes)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 将长度为 len 的数据从 buf 分块发送.
 * 首块按 256 字节边界对齐 (0x100 - (offset & 0xFF)),
 * 后续每块最多 256 字节.
 *
 * 二进制: 首块 max_chunk = 0x100 - (offset&0xFF) 仅计算一次,
 * 后续 chunk = min(remaining, 0x100).
 *
 * 参数:
 *   r0 = dst — 目标指针
 *   r1 = offset — 起始偏移
 *   r2 = len — 总长度
 *
 * 调用:
 *   func_161F0() @ 0x080161F0 — 单块发送器
 */

#include "stm32f4xx_hal.h"

extern void func_161F0(uint8_t *dst, uint32_t offset, uint32_t chunk_len);

void Chunked_Data_Sender(uint8_t *dst, uint32_t offset, uint32_t len)
{
    uint32_t remaining = len;
    uint32_t cur_offset = offset;
    uint8_t *cur_dst = dst;
    uint32_t chunk;

    /* 首块: 对齐到 256 字节边界 (仅计算一次, 在循环外) */
    chunk = 0x100 - (cur_offset & 0xFF);
    if (remaining <= chunk)
        chunk = remaining;

    while (1) {
        func_161F0(cur_dst, cur_offset, chunk);

        if (remaining == chunk)
            break;

        cur_dst += chunk;
        cur_offset += chunk;
        remaining -= chunk;

        /* 后续块: 最多 256 字节 */
        chunk = (remaining > 0x100) ? 0x100 : remaining;
    }
}
