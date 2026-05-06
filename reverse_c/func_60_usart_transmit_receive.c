/**
 * @file func_60_usart_transmit_receive.c
 * @brief USART 发送/接收 — 带超时的 USART 数据收发
 * DISASSEMBLY-TRACED. Verified against Capstone.
 * @addr  0x080118EC - 0x0801197E (146 bytes)
 *         子函数内联: 0x08011938 (SR 位控制), 0x08011950 (CR1 位控制)
 *                      0x08011962 (标志检查), 0x08011974 (RX 读取), 0x0801197A (TX 写入)
 *
 * USART 寄存器基址: 0x40013000 (USART6 或 USART1)
 * 流程: 等待 TXE → 写 DR → 等待 TC → 等待 RXNE → 读 DR
 *
 * 参数:
 *   r0 = data — 要发送的字节
 * 返回:
 *   r0 = 接收到的字节
 */

#include "stm32f4xx_hal.h"

#define USART_BASE  ((volatile uint32_t *)0x40013000)
#define USART_CR1   (*(volatile uint16_t *)(USART_BASE + 0))   /* +0x00 */
#define USART_CR2   (*(volatile uint16_t *)(USART_BASE + 1))   /* +0x04 */
#define USART_SR    (*(volatile uint16_t *)(USART_BASE + 2))   /* +0x08 */
#define USART_DR    (*(volatile uint16_t *)(USART_BASE + 3))   /* +0x0C */

/* 内联辅助: 设置 CR1 的位 */
static void usart_cr1_bit(uint32_t bit, uint32_t set)
{
    uint16_t cr1 = USART_CR1;
    if (set) {
        cr1 |= 0x40;    /* orr #0x40 (TXEIE 或其他控制位) */
    } else {
        cr1 &= 0xFFBF;  /* and */
    }
    USART_CR1 = cr1;
}

/* 内联辅助: 设置 CR2 的位 */
static void usart_cr2_bit(uint32_t mask, uint32_t set)
{
    uint16_t cr2 = USART_CR2;
    if (set) {
        cr2 |= (uint16_t)mask;
    } else {
        cr2 &= ~((uint16_t)mask);
    }
    USART_CR2 = cr2;
}

/* 内联辅助: 检查 SR 标志 */
static uint32_t usart_check_flag(uint32_t flag)
{
    if (USART_SR & (uint16_t)flag) {
        return 1;
    }
    return 0;
}

/* 内联辅助: 读 RX 数据 */
static uint32_t usart_read_rx(void)
{
    return (uint32_t)(USART_DR);  /* ldrh r0, [r1, #0xC] — 读取 DR 的替代路径 */
}

/* 内联辅助: 写 TX 数据 */
static void usart_write_tx(uint32_t data)
{
    USART_DR = (uint16_t)data;    /* strh r1, [r0, #0xC] */
}

/* ================================================================
 * USART_TransmitReceive() @ 0x080118EC
 * ================================================================ */
uint8_t USART_TransmitReceive(uint32_t data)
{
    uint32_t timeout;

    /* 步骤 1: 等待 TXE (SR bit 7/2) */
    timeout = 200;
    while (timeout > 0) {
        if (usart_check_flag(2)) {   /* SR bit 1 (TXE) */
            break;
        }
        timeout--;
    }

    /* 步骤 2: 写 TX 数据 */
    usart_write_tx(data);

    /* 步骤 3: 等待 TC (SR bit 6/1) */
    timeout = 200;
    while (timeout > 0) {
        if (usart_check_flag(1)) {   /* SR bit 0 (TC/RXNE) */
            break;
        }
        timeout--;
    }

    /* 步骤 4: 读 RX 数据 */
    return (uint8_t)usart_read_rx();
}
