/**
 * @file func_111_usart_tx_byte.c
 * @brief 函数: USART 发送字节 — 向 USART 数据寄存器写入 9 位数据
 * @addr  0x08015F2E - 0x08015F36 (8 bytes)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 叶子函数 (bx lr). 将字节写入 USART 发送数据寄存器 (TDR).
 *
 * 参数:
 *   r0 = USART base
 *   r1 = byte (取低 9 位)
 *
 * 调用: (无)
 */

#include "stm32f4xx_hal.h"

void USART_Tx_Byte(volatile void *usart_base, uint32_t byte)
{
    volatile uint16_t *dr = (volatile uint16_t *)((uint8_t *)usart_base + 4);
    *dr = byte & 0x1FF;
}
