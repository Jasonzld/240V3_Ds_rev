/**
 * @file func_67_display_cmd_param.c
 * @brief 函数: 显示命令(带参数)发送器 — 发送 0xEE 0x60 + 参数字节 + 尾帧
 * @addr  0x08012454 - 0x08012484 (48 bytes)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 发送固定序列: [0xEE, 0x60, param, 0xFF, 0xFC, 0xFF, 0xFF]
 * 每个字节通过 func_123E8 (SPI 字节发送器) 发出。
 *
 * 参数:
 *   r0 = param — 命令参数字节
 *
 * 调用:
 *   func_123E8() @ 0x080123E8 — 字节级 SPI 发送器
 */

#include "stm32f4xx_hal.h"

extern void func_123E8(uint8_t byte);

void Display_Cmd_Param(uint8_t param)
{
    func_123E8(0xEE);
    func_123E8(0x60);
    func_123E8(param);
    func_123E8(0xFF);
    func_123E8(0xFC);
    func_123E8(0xFF);
    func_123E8(0xFF);
}
