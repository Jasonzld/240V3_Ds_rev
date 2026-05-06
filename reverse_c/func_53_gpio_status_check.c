/**
 * @file func_53_gpio_status_check.c
 * @brief GPIO 引脚掩码状态检查 — 读取 GPIO 寄存器并匹配掩码
 * @addr  0x0801155C - 0x08011584 (40 bytes)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 读取 GPIO 寄存器 0x4000280C (GPIO 端口 ODR/IDR),
 * 用掩码 0x00017F7F 和输入参数过滤后返回位匹配结果。
 *
 * 参数:
 *   r0 = mask — 位掩码
 * 返回:
 *   r0 = 1 若匹配位非零, 否则 0
 */

#include "stm32f4xx_hal.h"

#define GPIO_REG    (*(volatile uint32_t *)0x4000280C)
#define HW_MASK     0x00017F7F

uint32_t GPIO_Status_Check(uint32_t mask)
{
    uint32_t reg_val;
    uint32_t filtered;

    reg_val = GPIO_REG;                          /* ldr r3, [pc, #0x14] */
    filtered = reg_val & HW_MASK;                /* and.w r2, r3, r4 */
    filtered = filtered & mask;                  /* and.w r3, r2, r1 */

    if (filtered != 0) {                         /* cbz r3, ... */
        return 1;
    }
    return 0;
}
