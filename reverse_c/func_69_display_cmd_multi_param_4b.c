/**
 * @file func_69_display_cmd_multi_param_4b.c
 * @brief 函数: 显示命令(多参数/4字节负载) — 发送 0xEE 0xB1 0x04 + 5字节数据 + 尾帧
 * @addr  0x080124D6 - 0x08012528 (82 bytes)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 与 func_68 结构相同，仅负载长度从 0x10 (5 字节) 变为 0x04 (5 字节)。
 * 实际数据负载仍是 5 字节: HI(p1), LO(p1), HI(p2), LO(p2), p3。
 *
 * 参数:
 *   r0 = param1 — 16 位值
 *   r1 = param2 — 16 位值
 *   r2 = param3 — 8 位值
 *
 * 调用:
 *   func_123E8() @ 0x080123E8 — 字节级 SPI 发送器
 */

#include "stm32f4xx_hal.h"

extern void func_123E8(uint8_t byte);

void Display_Cmd_Multi_Param_4B(uint16_t param1, uint16_t param2, uint8_t param3)
{
    func_123E8(0xEE);
    func_123E8(0xB1);
    func_123E8(0x04);                     /* 与 func_68 的唯一区别 */
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
