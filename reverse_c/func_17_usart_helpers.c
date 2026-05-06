/**
 * @file func_17_usart_helpers.c
 * @brief USART 寄存器辅助函数 (标志清除 + 使能控制 + 标志检查)
 * @addr  0x08015D62 - 0x08015DB2 (80 bytes, 含字面量池)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 子函数 (仅 uart_clear_flag 有 PUSH 入口):
 *   uart_clear_flag @ 0x08015D62 (30B, PUSH {r4,lr})
 *   uart_enable     @ 0x08015D80 (24B, 无独立 PUSH)
 *   uart_check_flag @ 0x08015D98 (26B, 无独立 PUSH)
 *
 * USART 底层寄存器操作:
 *   uart_enable:     CR1 bit13 (UE) 使能/禁用
 *   uart_clear_flag: 清除指定 SR 标志位 (通过写 0 到对应位)
 *   uart_check_flag: 检查指定 SR 标志位是否置位
 *
 * 调用示例 (USART6_Init 中):
 *   uart_enable(USART6, 1);    // 使能 USART6
 *   uart_clear_flag(USART6, 0x525);  // 清除 bit5 标志 (RXNE)
 *   uart_clear_flag(USART6, 0x424);  // 清除 bit4 标志 (IDLE)
 *   uart_clear_flag(USART6, 0x360);  // 清除 bit3 标志 (ORE)
 *
 * uart_clear_flag 编码: 参数 >> 8 为要清除的标志位号,
 * 通过写入 1<<bit 的反码到 SR 来清除对应标志。
 */

#include "stm32f4xx_hal.h"

/* USART 寄存器偏移 */
#define USART_SR_OFFSET   0x00
#define USART_CR1_OFFSET  0x0C

/* USART_CR1 位 */
#define USART_CR1_UE      (1 << 13)  /* USART 使能位 */

/* ================================================================
 * uart_enable() @ 0x08015D80
 *   设置/清除 USART_CR1 UE 位 (bit13)
 *   r0 = USART 基地址, r1 = 使能标志
 * ================================================================ */
void uart_enable(USART_TypeDef *USARTx, uint32_t enable)
{
    if (enable) {
        /* 0x08015D80: cbz r1, #disable */
        USARTx->CR1 |= USART_CR1_UE;            /* 0x08015D82-88: ldrh [r0,#0xC]; orr #0x2000; strh */
        /* 0x08015D8A: b #return */
    } else {
        USARTx->CR1 &= ~USART_CR1_UE;           /* 0x08015D8C-94: ldrh [r0,#0xC]; movw #0xDFFF; and; strh */
    }
    /* 0x08015D96: bx lr */
}

/* ================================================================
 * uart_clear_flag() @ 0x08015D62
 *   清除 USART 状态寄存器 (SR) 的指定标志位
 *   r0 = USART 基地址, r1 = 编码值 (bit_index << 8)
 *
 * 计算: bit_index = r1 >> 8
 *       写 SR ← ~(1 << bit_index)  → 清除对应标志位
 *
 * 注: USART_SR 写 0 到某位可清除该标志 (硬件自动置 0 的位
 *     可被软件写 0 清除), 位 0-5 对应 PE/FE/NF/ORE/IDLE/RXNE。
 *
 * 虽然 r1 与 0x96A (2410) 比较, 但 bne 跳过的是 nop 而非
 * 实际计算路径, 因此该比较无实际分支功能 (编译器优化残留)。
 * ================================================================ */
void uart_clear_flag(USART_TypeDef *USARTx, uint32_t encoded_val)
{
    uint32_t bit_index;          /* r2 */
    uint32_t bit_mask;           /* r4 */
    uint32_t inverted;           /* r3/r4 */

    /* 0x08015D68-6E: 比较 r1 与 0x96A — 实际为死代码 (bne 仅跳过一个 nop) */

    /* 提取标志位号: encoded_val >> 8 */
    bit_index = encoded_val >> 8;                /* 0x08015D72: asrs r2, r1, #8 */

    /* 创建单 bit 掩码: 1 << bit_index */
    bit_mask = 1 << bit_index;                   /* 0x08015D74-76: mov r4,#1; lsl r4,r2 */

    /* 取反: ~bit_mask (16-bit) */
    inverted = (uint16_t)~bit_mask;              /* 0x08015D78-7A: uxth r3,r4; mvn r4,r3 */

    /* 写回 SR (offset 0x00): 清除对应标志位 */
    *(volatile uint16_t *)((uint32_t)USARTx + USART_SR_OFFSET) = inverted;  /* 0x08015D7C: strh r4,[r0] */
}

/* ================================================================
 * uart_check_flag() @ 0x08015D98
 *   检查 USART 状态寄存器 (SR) 的指定标志位
 *   r0 = USART 基地址, r1 = 位掩码
 *   返回: 1 = 至少有一位已置位, 0 = 全部未置位
 *
 * 注意: r1 在高地址比较 #0x200 (bit9=CTS?), 但此比较只跳过
 *       一个 nop (无实际效果), 函数总是执行 SR & mask 检查。
 * ================================================================ */
uint32_t uart_check_flag(USART_TypeDef *USARTx, uint32_t mask)
{
    /* 0x08015D9C-A0: cmp r1,#0x200 — 死代码 (bne 仅跳过 nop) */

    /* 读取 SR 并检查掩码 */
    if (USARTx->SR & mask) {                     /* 0x08015DA4-A6: ldrh r3,[r2]; and r3,r1 */
        return 1;                                /* 0x08015DA8-AA: cbz r3,#ret0; mov r0,#1 */
    }
    return 0;                                    /* 0x08015DAE: mov r0,#0 */
}
