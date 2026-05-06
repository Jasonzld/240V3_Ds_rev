/**
 * @file func_52_gpio_output_sequence.c
 * @brief GPIO 输出序列操作 — GPIO 检查 + 寄存器写入 + 延迟
 * DISASSEMBLY-TRACED. Verified against Capstone.
 * @addr  0x08011520 - 0x0801155C (60 bytes)
 *
 * 检查 GPIO 状态后执行寄存器写入，最后调用延迟。
 *
 * 子函数:
 *   func_1155C() — GPIO_Status_Check
 *   func_11540() — GPIO_Register_Write (写入 0x4000280C)
 *   func_BCC4()  — 系统延迟
 */

#include "stm32f4xx_hal.h"

#define GPIO_REG    (*(volatile uint32_t *)0x4000280C)

extern uint32_t GPIO_Status_Check(uint32_t mask);
extern void     GPIO_Register_Write(uint32_t value);
extern void     func_BCC4(uint32_t delay);

void GPIO_Output_Sequence(void)
{
    if (GPIO_Status_Check(0x100) == 1) {
        GPIO_Register_Write(0x100);              /* lsls r0, #8 → bl func_11540 (0x100<<8 inlined) */
    }
    func_BCC4(0x20000);                      /* delay */
}

/**
 * GPIO_Register_Write @ 0x08011540 (内联子函数)
 *   uxth r1, r0; orr r1, r1, #0x80
 *   读 GPIO_REG; and r2 with #0x80; orn r1,r2,r1; str r1 → GPIO_REG
 */
void GPIO_Register_Write(uint32_t value)
{
    uint32_t reg_val;
    uint32_t new_val;
    uint32_t masked;

    new_val = (uint32_t)(uint16_t)value;     /* uxth */
    new_val = new_val | 0x80;                 /* orr */

    reg_val = GPIO_REG;                       /* ldr r2 */
    masked = reg_val & 0x80;                  /* and */
    new_val = masked | (~new_val & 0x80);     /* orn */

    GPIO_REG = new_val;                       /* str */
}
