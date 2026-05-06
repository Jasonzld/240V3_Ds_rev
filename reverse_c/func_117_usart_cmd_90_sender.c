/**
 * @file func_117_usart_cmd_90_sender.c
 * @brief 函数: USART 命令 0x90 发送器 — 发送 6 字节命令帧并读取响应
 * @addr  0x08016048 - 0x08016090 (72 bytes, 含字面量池)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 发送: {0x90, 0x00, 0x00, 0x00, 0xFF, 0xFF}
 * 接收 2 字节响应, 合并为半字返回.
 *
 * 返回:
 *   r0 = (resp1 << 8) | resp0
 *
 * 调用:
 *   func_118EC() @ 0x080118EC — USART 发送/接收字节
 */

#include "stm32f4xx_hal.h"

extern uint8_t func_118EC(uint8_t byte);

uint32_t USART_Cmd_90_Sender(void)
{
    uint32_t response = 0;

    /* 清除标志 */
    *(volatile uint32_t *)0x42408000 = 0;  /* bit-band clear */

    /* 发送命令帧 */
    func_118EC(0x90);
    func_118EC(0x00);
    func_118EC(0x00);
    func_118EC(0x00);
    response = func_118EC(0xFF) << 8;      /* 第一响应字节 */
    response |= func_118EC(0xFF);           /* 第二响应字节 */

    /* 设置完成标志 */
    *(volatile uint32_t *)0x424082A8 = 1;

    return response;
}
