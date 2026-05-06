/**
 * @file func_104_usart_status_handler.c
 * @brief 函数: USART 状态处理器 — 检查 USART 标志并读取状态字节
 * @addr  0x08014344 - 0x08014378 (52 bytes, 含字面量池)
 *         字面量池 0x08014374 (4 bytes: 0x40011000)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * USART 状态轮询 + 读取:
 *   1. func_15DB2(USART_BASE, 0x525) — 检查特定 USART 状态组合
 *   2. 若就绪: func_15F24(USART_BASE) → r4 (读取字节), 调用 func_18D60(r4)
 *   3. func_15D98(USART_BASE, 8) — 检查 TX/RX 标志
 *   4. 若就绪: 读取 [USART_BASE+4] 的字节 → r5
 *
 * 参数: (无)
 *
 * 调用:
 *   func_15DB2() @ 0x08015DB2 — USART 组合标志检查
 *   func_15F24() @ 0x08015F24 — USART 读字节
 *   func_18D60() @ 0x08018D60 — 字节处理
 *   func_15D98() @ 0x08015D98 — USART 单标志检查
 */

#include "stm32f4xx_hal.h"

extern uint32_t func_15DB2(volatile void *spi_base, uint32_t flags);
extern uint8_t  func_15F24(volatile void *spi_base);
extern void     func_18D60(uint8_t byte);
extern uint32_t func_15D98(volatile void *spi_base, uint32_t flag);

#define USART_BASE  ((volatile void *)0x40011000)  /* USART6 base (from literal pool) */

void USART_Status_Handler(void)
{
    uint8_t r4, r5;

    /* 0x08014344: push {r4,r5,r6,lr} — r6 未使用, 用于栈对齐 */
    /* 0x08014346: movw r1,#0x525 */

    /* 检查 USART 组合标志 (0x525) */
    if (func_15DB2(USART_BASE, 0x525)) {
        r4 = func_15F24(USART_BASE);
        func_18D60(r4);
    }

    /* 检查 USART 单标志 (8) */
    if (func_15D98(USART_BASE, 8)) {
        r5 = (uint8_t)(*(volatile uint16_t *)((uint8_t *)USART_BASE + 4));
    }
}
