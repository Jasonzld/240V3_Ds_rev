/**
 * @file func_54_fsmc_status_handler.c
 * @brief FSMC 状态处理 — 检查 FSMC 寄存器并更新状态
 * @addr  0x08011584 - 0x080115B8 (52 bytes)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 检查 GPIO 状态 (掩码 0x400), 条件写入 GPIO 寄存器,
 * 读取 FSMC 寄存器 0x42408288, 取反写入, 最后延迟。
 */

#include "stm32f4xx_hal.h"

#define FSMC_REG    (*(volatile uint32_t *)0x42408288)

extern uint32_t GPIO_Status_Check(uint32_t mask);
extern void     GPIO_Register_Write(uint32_t value);
extern void     func_BCC4(uint32_t delay);

void FSMC_Status_Handler(void)
{
    uint32_t val;

    if (GPIO_Status_Check(0x400) == 1) {
        GPIO_Register_Write(1 << 0xA);         /* lsls r0,#0xa → r0=1, result=0x400 */

        val = FSMC_REG;                          /* ldr r0, [pc] */
        if (val != 0) {                          /* cbnz */
            val = 0;                             /* movs #0 */
        } else {
            val = 1;                             /* movs #1 */
        }
        FSMC_REG = val;                          /* str r0, [r1] */
    }

    func_BCC4(0x400000);                     /* delay */
}
