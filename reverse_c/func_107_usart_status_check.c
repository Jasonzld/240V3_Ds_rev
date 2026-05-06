/**
 * @file func_107_usart_status_check.c
 * @brief 函数: USART 状态组合检查 — 双寄存器位测试
 * @addr  0x08015DB2 - 0x08015E06 (84 bytes)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 检查 USART 两个寄存器中的特定位是否同时置位:
 *   flags 编码:
 *     bits[4:0]   = 位号 (在选中寄存器中)
 *     bits[7:5]   = 寄存器选择 (1=+0x0C, 2=+0x10, 其他=+0x14)
 *     bits[15:8]  = 位号 (在基址寄存器 [base+0x00] 中)
 *
 * 返回 1 当且仅当两个位都置位。
 *
 * 参数:
 *   r0 = USART base
 *   r1 = flags (编码如上)
 * 返回:
 *   r0 = 1 (两个位都置位) 或 0
 *
 * 调用: (无 — 叶子函数)
 */

#include "stm32f4xx_hal.h"

uint32_t USART_Status_Check(volatile void *usart_base, uint32_t flags)
{
    uint32_t reg_sel, bit_pos, base_bit;
    uint32_t mask1, mask2;
    uint32_t reg_val, base_val;

    /* 解码 flags */
    reg_sel  = (flags >> 5) & 0x7;   /* bits[7:5] */
    bit_pos  = flags & 0x1F;         /* bits[4:0] */
    base_bit = (flags >> 8) & 0xFF;  /* bits[15:8] */

    /* 创建位掩码 */
    mask1 = 1 << bit_pos;

    /* 选择寄存器 */
    if (reg_sel == 1) {
        reg_val = *(volatile uint16_t *)((uint8_t *)usart_base + 0x0C);
    } else if (reg_sel == 2) {
        reg_val = *(volatile uint16_t *)((uint8_t *)usart_base + 0x10);
    } else {
        reg_val = *(volatile uint16_t *)((uint8_t *)usart_base + 0x14);
    }

    /* 检查基址寄存器位 */
    mask2 = 1 << base_bit;
    base_val = *(volatile uint16_t *)usart_base;

    /* 两个位都置位时返回 1 */
    if ((reg_val & mask1) && (base_val & mask2)) {
        return 1;
    }
    return 0;
}
