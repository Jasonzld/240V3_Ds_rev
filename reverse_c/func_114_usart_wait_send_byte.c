/**
 * @file func_114_usart_wait_send_byte.c
 * @brief 函数: USART6 等待并发送字节 — 轮询 TXE 后发送
 * @addr  0x08015F38 - 0x08015F60 (40 bytes, 含字面量池)
 *         字面量池 0x08015F58 (1 word)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 轮询 USART6 TXE 标志 (0x40) 直到就绪, 然后发送字节.
 *
 * 字面量池 (Capstone 验证 @ 0x08015F58):
 *   0x40011400 = USART6 base
 *
 * 参数:
 *   r0 = byte to send
 * 返回:
 *   r0 = 发送的字节
 *
 * 调用:
 *   func_15D98() @ 0x08015D98 — USART 标志检查
 *   func_15F2E() @ 0x08015F2E — USART 发送字节
 */

#include "stm32f4xx_hal.h"

extern uint32_t func_15D98(volatile void *base, uint32_t flag);
extern void     func_15F2E(volatile void *base, uint32_t byte);

#define USART6_BASE  ((volatile void *)0x40011400)  /* pool[0] — USART6 */

uint32_t USART_Wait_Send_Byte(uint32_t byte)
{
    /* 轮询 USART6 TXE (bit 6 = 0x40) */
    while (!func_15D98(USART6_BASE, 0x40)) {
        /* 等待发送寄存器空 */
    }
    func_15F2E(USART6_BASE, byte);
    return byte;
}
