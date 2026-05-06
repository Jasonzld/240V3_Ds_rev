/**
 * @file func_110_usart_read_byte.c
 * @brief 函数: USART 读字节 — 从 USART 数据寄存器读取 9 位数据
 * @addr  0x08015F24 - 0x08015F2E (10 bytes)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 叶子函数 (bx lr). 读取 USART 接收数据寄存器 (RDR) 的低 9 位.
 *
 * 参数:
 *   r0 = USART base
 * 返回:
 *   r0 = [base+4] & 0x1FF  (9-bit 数据)
 *
 * 调用: (无)
 */

#include "stm32f4xx_hal.h"

uint32_t USART_Read_Byte(volatile void *usart_base)
{
    volatile uint16_t *dr = (volatile uint16_t *)((uint8_t *)usart_base + 4);
    return *dr & 0x1FF;
}
