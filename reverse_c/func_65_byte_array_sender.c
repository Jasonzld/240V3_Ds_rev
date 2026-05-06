/**
 * @file func_65_byte_array_sender.c
 * @brief 函数: 字节数组发送器 — 将 null 结尾的字节数组逐字节发送
 * @addr  0x08012418 - 0x0801242E (22 bytes)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 遍历以 0x00 结尾的字节数组，通过 func_123E8 逐字节发送。
 *
 * 参数:
 *   r0 = data — 指向 null 结尾字节数组的指针
 *
 * 调用:
 *   func_123E8() @ 0x080123E8 — 字节发送器 (func_64)
 */

#include "stm32f4xx_hal.h"

extern void func_123E8(uint8_t byte);

void Byte_Array_Sender(const uint8_t *data)
{
    while (*data != 0) {
        func_123E8(*data);
        data++;
    }
}
