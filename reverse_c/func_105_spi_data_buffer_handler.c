/**
 * @file func_105_spi_data_buffer_handler.c
 * @brief 函数: USART2 数据缓冲区处理器 — 三阶段 USART2 数据读取与环缓冲管理
 * @addr  0x08014378 - 0x08014404 (140 bytes, 含字面量池)
 *         字面量池 0x080143F4-0x08014404 (16 bytes: USART2_BASE, idx_addr, buf_base)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 阶段 1 (标志 0x525): func_15DB2(USART2, 0x525) → func_15F24(USART2) → buf[idx++]
 * 阶段 2 (标志 0x424): func_15DB2(USART2, 0x424) → 读取 USART2+0/+4 状态
 *                     → buf[idx]=0, USART2+0 |= 0x8000, func_15D62(USART2, 0x424)
 * 阶段 3 (标志 8):     func_15D98(USART2, 8) → func_15D50(USART2, 8) → 读取 USART2+4
 *
 * 调用:
 *   func_15DB2() @ 0x08015DB2 — USART 组合标志检查
 *   func_15F24() @ 0x08015F24 — USART 读字节
 *   func_15D62() @ 0x08015D62 — USART 位清除
 *   func_15D98() @ 0x08015D98 — USART 单标志检查
 *   func_15D50() @ 0x08015D50 — USART 半字写入
 */

#include "stm32f4xx_hal.h"

extern uint32_t func_15DB2(volatile void *usart_base, uint32_t flags);
extern uint8_t  func_15F24(volatile void *usart_base);
extern void     func_15D62(volatile void *usart_base, uint32_t val);
extern uint32_t func_15D98(volatile void *usart_base, uint32_t flag);
extern void     func_15D50(volatile void *usart_base, uint32_t val);

/* 字面量池: 0x40004400=USART2, 0x2000026C=环缓冲索引 (uint16_t, ldrh/strh 操作), 0x2000091C=环缓冲基址 */
#define USART2_BASE     ((volatile void *)0x40004400)
#define RING_IDX        (*(volatile uint16_t *)0x2000026C)
#define RING_BUF_BASE   0x2000091C

void USART2_Data_Buffer_Handler(void)
{
    uint8_t r4, r5;

    /* 阶段 1: 标志 0x525 — 读字节入环缓冲 (0x0801437A-0x0801439E) */
    if (func_15DB2(USART2_BASE, 0x525)) {
        r4 = func_15F24(USART2_BASE);
        {
            uint16_t idx = RING_IDX;                                 /* ldrh — 半字读取 */
            *(volatile uint8_t *)(RING_BUF_BASE + (uint8_t)idx) = r4; /* strb — 字节索引存储 */
            RING_IDX = idx + 1;                                      /* adds+strh — 半字递增回写 */
        }
    }

    /* 阶段 2: 标志 0x424 — 读状态 + 清零缓冲条目 + 置位 bit15 (0x080143A0-0x080143D6) */
    if (func_15DB2(USART2_BASE, 0x424)) {
        r5 = (uint8_t)(*(volatile uint16_t *)USART2_BASE);           /* USART2+0 低字节 */
        r5 = (uint8_t)(*(volatile uint16_t *)((uint8_t *)USART2_BASE + 4)); /* USART2+4 */
        {
            uint8_t idx = (uint8_t)RING_IDX;                         /* ldrb — 字节读取索引 */
            *(volatile uint8_t *)(RING_BUF_BASE + idx) = 0;
        }
        RING_IDX |= 0x8000;                                     /* ldrh+orr+strh — RING_IDX 置 bit15 */
        func_15D62(USART2_BASE, 0x424);                         /* 位清除 */
    }

    /* 阶段 3: 标志 8 — 写入后读回状态 */
    if (func_15D98(USART2_BASE, 8)) {
        func_15D50(USART2_BASE, 8);
        r4 = (uint8_t)(*(volatile uint16_t *)((uint8_t *)USART2_BASE + 4));
    }
}
