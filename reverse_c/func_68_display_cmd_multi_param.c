/**
 * @file func_68_display_cmd_multi_param.c
 * @brief 函数: 显示命令(多参数/10字节负载) — 发送 0xEE 0xB1 0x10 + 5字节数据 + 尾帧
 * @addr  0x08012484 - 0x080124D6 (82 bytes)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 发送格式: [0xEE, 0xB1, 0x10, HI(p1), LO(p1), HI(p2), LO(p2), p3,
 *             0xFF, 0xFC, 0xFF, 0xFF]
 * 其中 0x10 表示后续数据长度为 5 字节。
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

void Display_Cmd_Multi_Param(uint16_t param1, uint16_t param2, uint8_t param3)
{
    func_123E8(0xEE);
    func_123E8(0xB1);
    func_123E8(0x10);                /* 数据长度: 5 字节 */
    func_123E8((uint8_t)(param1 >> 8));   /* HI(param1) */
    func_123E8((uint8_t)param1);          /* LO(param1) */
    func_123E8((uint8_t)(param2 >> 8));   /* HI(param2) */
    func_123E8((uint8_t)param2);          /* LO(param2) */
    func_123E8(param3);                   /* param3 */
    func_123E8(0xFF);
    func_123E8(0xFC);
    func_123E8(0xFF);
    func_123E8(0xFF);
}
