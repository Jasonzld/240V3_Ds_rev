/**
 * @file func_118_usart_cmd_05_sender.c
 * @brief 函数: USART 命令 0x05 发送器 — 发送查询命令并读回响应
 * @addr  0x08016090 - 0x080160BC (44 bytes, 含字面量池)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 发送: {0x05, 0xFF} → 读取响应字节
 *
 * 返回:
 *   r0 = 响应字节
 *
 * 调用:
 *   func_118EC() @ 0x080118EC — USART 发送/接收
 */

#include "stm32f4xx_hal.h"

extern uint8_t func_118EC(uint8_t byte);

uint32_t USART_Cmd_05_Sender(void)
{
    uint32_t response = 0;

    *(volatile uint32_t *)0x42408000 = 0;

    func_118EC(0x05);
    response = func_118EC(0xFF);

    *(volatile uint32_t *)0x424082A8 = 1;

    return response;
}
