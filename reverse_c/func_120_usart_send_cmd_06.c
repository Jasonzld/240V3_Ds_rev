/**
 * @file func_120_usart_send_cmd_06.c
 * @brief 函数: USART 发送命令 0x06 — 发送初始化命令头
 * @addr  0x08016188 - 0x080161A8 (32 bytes, 含字面量池)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 发送命令字节 0x06 并设置完成标志.
 *
 * 调用:
 *   func_118EC() @ 0x080118EC — USART 发送/接收
 */

#include "stm32f4xx_hal.h"

#define FLAG_CLR_BITBAND  (*(volatile uint32_t *)0x42408000)  /* bit-band clear alias */
#define FLAG_SET_BITBAND  (*(volatile uint32_t *)0x424082A8)  /* bit-band set alias (base+0x2A8) */

extern uint8_t func_118EC(uint8_t byte);

void USART_Send_Cmd_06(void)
{
    FLAG_CLR_BITBAND = 0;
    func_118EC(0x06);
    FLAG_SET_BITBAND = 1;
}
