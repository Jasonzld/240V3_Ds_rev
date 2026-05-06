/**
 * @file func_77_multi_byte_command_sender.c
 * @brief 函数: 多字节命令发送器 — 通过 func_18424 转换每个参数后发送 0x81 命令帧
 * @addr  0x080127DA - 0x08012886 (172 bytes)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 发送格式: [0xEE, 0x81, func_18424(p1), func_18424(p2), ... func_18424(p7),
 *             0xFF, 0xFC, 0xFF, 0xFF]
 *
 * 7 个参数逐一通过 func_18424 转换后取低字节发送。
 *
 * 参数:
 *   r0 = param1
 *   r1 = param2
 *   r2 = param3
 *   r3 = param4
 *   [sp+0x28] = param5 — 通过栈传递
 *   [sp+0x2C] = param6:param7 — 双字 (64 位) 栈参数
 *
 * 调用:
 *   func_123E8() @ 0x080123E8 — 字节级 SPI 发送器
 *   func_18424() @ 0x08018424 — 参数转换器
 */

#include "stm32f4xx_hal.h"

extern void func_123E8(uint8_t byte);
extern uint32_t func_18424(uint32_t val);

void Multi_Byte_Command_Sender(uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4,
                                uint32_t p5, uint32_t p6, uint32_t p7)
{
    func_123E8(0xEE);
    func_123E8(0x81);
    func_123E8((uint8_t)func_18424(p1));
    func_123E8((uint8_t)func_18424(p2));
    func_123E8((uint8_t)func_18424(p3));
    func_123E8((uint8_t)func_18424(p4));
    func_123E8((uint8_t)func_18424(p5));
    func_123E8((uint8_t)func_18424(p6));
    func_123E8((uint8_t)func_18424(p7));
    func_123E8(0xFF);
    func_123E8(0xFC);
    func_123E8(0xFF);
    func_123E8(0xFF);
}
