/**
 * @file func_137_byte_loop_callback.c
 * @brief 函数: 字节循环回调 — 对数组每字节调用 BLX 回调函数
 * @addr  0x08017E3C - 0x08017E74 (56 bytes)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 参数:
 *   r0 = count, r1 = flags (bit16→0x30/0x20, bit13→early return 0)
 *   r2 = arg2, r3 = callback (BLX)
 *
 * 内部辅助 (0x08017E6A): Byte_Buffer_Append
 */

#include "stm32f4xx_hal.h"

typedef void (*byte_callback_t)(uint32_t offset, uint32_t arg);

uint32_t Byte_Loop_Callback(uint32_t count, uint32_t flags, uint32_t arg2, byte_callback_t callback)
{
    uint32_t offset;
    uint32_t i = 0;

    /* 根据 bit16 选择偏移值 */
    if (flags & 0x10000) {
        offset = 0x30;
    } else {
        offset = 0x20;
    }

    /* bit13 设 → 立即返回 0 (lsls r0,r1,#0x12; bpl → b return) */
    if (flags & 0x2000) {
        return 0;
    }

    /* 循环调用 */
    for (i = 0; i < count; i++) {
        callback(offset, arg2);
    }

    return i;
}

/* ---- 内部辅助: 字节缓冲写入 ---- */
void Byte_Buffer_Append(uint8_t byte, uint8_t **buf_ptr)
{
    uint8_t *buf = *buf_ptr;
    buf[0] = byte;
    *buf_ptr = buf + 1;
}
