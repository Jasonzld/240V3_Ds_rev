/**
 * @file func_108_usart_write_halfword.c
 * @brief 函数: USART 写半字 — 取反写入 USART 状态寄存器
 * @addr  0x08015D50 - 0x08015D62 (18 bytes, 含字面量池)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 叶子函数 (bx lr). 将取反后的值写入 USART 基址寄存器:
 *   [base] = ~value  (写 0 清除 SR 标志位)
 *
 * 注意: 二进制中有 CMP+BNE 对 bit9 的检查, 但 BNE 仅跳过 NOP,
 *       两条路径最终都执行 MVNS+STRH. 这是编译器对空 if-body 的优化残留.
 *
 * 参数:
 *   r0 = USART base
 *   r1 = value
 *
 * 调用: (无)
 */

#include "stm32f4xx_hal.h"

void USART_Write_Halfword(volatile void *usart_base, uint32_t value)
{
    /*
     * 0x08015D50: AND r2,r1,#0x200; CMP r2,#0x200; BNE #skip_nop
     *   → bit9 检查为死代码 (BNE 仅跳过 NOP, 两条路径合流到 MVNS)
     */
    *(volatile uint16_t *)usart_base = (uint16_t)~value;
}
