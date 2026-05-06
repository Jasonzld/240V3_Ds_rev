/**
 * @file func_76_data_frame_sender.c
 * @brief 函数: 数据帧发送器 — 发送含 null 结尾数据负载的命令帧
 * @addr  0x08012788 - 0x080127DA (82 bytes)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 发送格式: [0xEE, 0xB1, 0x10, HI(mode), LO(mode), HI(len), LO(len),
 *             <data bytes via func_12418>, 0xFF, 0xFC, 0xFF, 0xFF]
 *
 * 数据部分通过 func_65 (Byte_Array_Sender) 以 null 结尾字节数组方式发送。
 *
 * 参数:
 *   r0 = mode — 帧模式 ID
 *   r1 = len  — 数据长度提示
 *   r2 = data — 指向 null 结尾字节数组的指针
 *
 * 调用:
 *   func_123E8() @ 0x080123E8 — 字节级 SPI 发送器
 *   func_12418() @ 0x08012418 — 字节数组发送器 (func_65)
 */

#include "stm32f4xx_hal.h"

extern void func_123E8(uint8_t byte);
extern void func_12418(const uint8_t *data);

void Data_Frame_Sender(uint16_t mode, uint16_t len, const uint8_t *data)
{
    func_123E8(0xEE);
    func_123E8(0xB1);
    func_123E8(0x10);
    func_123E8((uint8_t)(mode >> 8));
    func_123E8((uint8_t)mode);
    func_123E8((uint8_t)(len >> 8));
    func_123E8((uint8_t)len);

    func_12418(data);

    func_123E8(0xFF);
    func_123E8(0xFC);
    func_123E8(0xFF);
    func_123E8(0xFF);
}
