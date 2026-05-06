/**
 * @file func_112_usart_bit_set_clear.c
 * @brief 函数: USART 位设置/清除 — 设置或清除 USART 寄存器中的指定位
 * @addr  0x08015E06 - 0x08015E50 (74 bytes, 含字面量池)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 根据 flags 编码选择 USART 寄存器并置位/清除指定位:
 *   flags bits[4:0]   = 位号
 *   flags bits[7:5]   = 寄存器选择 (1=+0x0C, 2=+0x10, 其他=+0x14)
 * 若 set != 0: 设置该位 (ORR)
 * 若 set == 0: 清除该位 (BIC)
 *
 * 注意: 二进制中有 CMP flags,#0x96A; BNE 的检查, 但 BNE 仅跳过 NOP,
 *       两条路径都继续执行实际操作. 这是编译器对空 if-body 的优化残留,
 *       与 uart_clear_flag (func_17) 的模式相同.
 *
 * 参数:
 *   r0 = USART base
 *   r1 = flags
 *   r2 = set (非零=置位, 零=清除)
 *
 * 调用: (无 — 叶子函数)
 */

#include "stm32f4xx_hal.h"

void USART_Bit_Set_Clear(volatile void *usart_base, uint32_t flags, uint32_t set)
{
    uint32_t reg_sel, bit_pos, mask;
    volatile uint32_t *reg;

    /*
     * 0x08015E16: CMP flags,#0x96A; BNE #skip_nop
     *   → 死代码 — BNE 仅跳过 NOP, 不跳过实际操作
     */

    reg_sel = (flags >> 5) & 0x7;
    bit_pos = flags & 0x1F;
    mask = 1 << bit_pos;

    /* 选择目标寄存器 */
    if (reg_sel == 1) {
        reg = (volatile uint32_t *)((uint8_t *)usart_base + 0x0C);
    } else if (reg_sel == 2) {
        reg = (volatile uint32_t *)((uint8_t *)usart_base + 0x10);
    } else {
        reg = (volatile uint32_t *)((uint8_t *)usart_base + 0x14);
    }

    if (set) {
        *reg |= mask;
    } else {
        *reg &= ~mask;
    }
}
