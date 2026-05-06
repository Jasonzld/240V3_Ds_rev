/**
 * @file func_41_nvic_setup_wrapper.c
 * @brief 函数: NVIC_SetupWrapper — NVIC 配置包装器
 * @addr  0x0800D9A6 - 0x0800D9B4 (14 bytes)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 调用 func_12484(3, 2, 1) 执行 NVIC 中断配置。
 * push {r4, lr}: r4 未使用, 仅保存 lr 用于子调用。
 *
 * 调用:
 *   func_12484() @ 0x08012484 — NVIC 配置 (参数 3, 2, 1)
 *
 * 注: 无字面量池, bx lr 返回隐含在 pop {r4, pc} 中。
 */

#include "stm32f4xx_hal.h"

extern void func_12484(uint32_t a, uint32_t b, uint32_t c);  /* 0x08012484 */

/* ================================================================
 * NVIC_SetupWrapper() @ 0x0800D9A6
 *   指令: push{r4,lr}; movs r2,#1; movs r1,#2; movs r0,#3;
 *         bl func_12484; pop{r4,pc}
 * ================================================================ */
void NVIC_SetupWrapper(void)
{
    func_12484(3, 2, 1);                         /* 0x0800D9A8-AE: movs r2,#1; movs r1,#2; movs r0,#3; bl */
}
