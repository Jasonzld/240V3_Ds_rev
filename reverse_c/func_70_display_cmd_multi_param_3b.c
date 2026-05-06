/**
 * @file func_70_display_cmd_multi_param_3b.c
 * @brief 函数: 显示命令(多参数/3字节负载) — 发送 0xEE 0xB1 0x03 + 5字节数据 + 尾帧
 * @addr  0x08012528 - 0x0801257A (82 bytes)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 与 func_68/69 同族，仅负载长度字节不同:
 *   func_68: 0x10 (16)
 *   func_69: 0x04 (4)
 *   func_70: 0x03 (3)
 *
 * 参数:
 *   r0 = param1 — 16 位值 (拆为 HI/LO)
 *   r1 = param2 — 16 位值 (拆为 HI/LO)
 *   r2 = param3 — 8 位值
 *
 * 调用:
 *   func_123E8() @ 0x080123E8 — 字节级 SPI 发送器
 */

#include "stm32f4xx_hal.h"

extern void func_123E8(uint8_t byte);

void Display_Cmd_Multi_Param_3B(uint16_t param1, uint16_t param2, uint8_t param3)
{
    func_123E8(0xEE);
    func_123E8(0xB1);
    func_123E8(0x03);
    func_123E8((uint8_t)(param1 >> 8));
    func_123E8((uint8_t)param1);
    func_123E8((uint8_t)(param2 >> 8));
    func_123E8((uint8_t)param2);
    func_123E8(param3);
    func_123E8(0xFF);
    func_123E8(0xFC);
    func_123E8(0xFF);
    func_123E8(0xFF);
}
