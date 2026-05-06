/**
 * @file func_64_byte_sender_spi.c
 * @brief 函数: 字节级 SPI 发送器 — 等待 SPI 标志后发送单字节
 * @addr  0x080123E8 - 0x08012418 (48 bytes, 含字面量池)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 调用 func_15F2E 初始化发送，然后轮询 SPI 状态标志 0x80 和 0x40
 * 直到就绪才返回。寄存器基址 0x40011000 (SPI2 或类似外设)。
 *
 * 参数:
 *   r0 = byte — 要发送的字节
 *
 * 调用:
 *   func_15F2E() @ 0x08015F2E — SPI 发送初始化
 *   func_15D98() @ 0x08015D98 — SPI 状态标志检查
 */

#include "stm32f4xx_hal.h"

#define SPI_BASE  ((volatile uint32_t *)0x40011000)

extern void    func_15F2E(uint32_t base, uint8_t data);
extern int     func_15D98(uint32_t base, uint32_t flag);

void Byte_Sender_SPI(uint8_t byte)
{
    func_15F2E((uint32_t)SPI_BASE, byte);

    do {
        /* 轮询标志 0x80 (TXE / BSY) */
    } while (func_15D98((uint32_t)SPI_BASE, 0x80) == 0);

    do {
        /* 轮询标志 0x40 (RXNE / TC) */
    } while (func_15D98((uint32_t)SPI_BASE, 0x40) == 0);
}
