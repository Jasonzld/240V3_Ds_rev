/**
 * @file func_72_timer_prescaler_calc.c
 * @brief 函数: 定时器预分频计算器 — 根据输入值计算并写入定时器 PSC/ARR
 * @addr  0x080125B0 - 0x08012604 (84 bytes, 含字面量池)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 无栈帧叶函数 (bx lr 返回, 无需 push/pop)。
 * 根据输入值 r0 分 4 个范围分别计算定时器参数。
 * 寄存器基址 0x40010000 (TIM2/TIM5)。
 *
 * 参数:
 *   r0 = value — 输入量值
 *
 * 范围分发:
 *   value == 0:    直接返回 (无操作)
 *   0 < value <= 100:       [reg+0x28]=9999, [reg+0x2C]=(10000/value)-1
 *   100 < value <= 10000:   [reg+0x28]=99,   [reg+0x2C]=(1000000/value)-1
 *   value > 10000:          [reg+0x28]=0,    [reg+0x2C]=(10000000/value)-1
 *
 * 调用:
 *   (无 — 叶函数, 不调用任何子函数)
 */

#include "stm32f4xx_hal.h"

#define TIMER_BASE  ((volatile uint32_t *)0x40010000)

#define TIM_OFFSET_28   (0x28 / 2)  /* 半字偏移 +0x28 */
#define TIM_OFFSET_2C   (0x2C / 4)  /* 字偏移   +0x2C */

void Timer_Prescaler_Calc(uint32_t value)
{
    volatile uint16_t *reg_hw;
    volatile uint32_t *reg_w;

    if (value == 0) {
        return;
    }

    reg_hw = (volatile uint16_t *)((uint8_t *)TIMER_BASE + 0x28);
    reg_w  = (volatile uint32_t *)((uint8_t *)TIMER_BASE + 0x2C);

    if (value <= 100) {
        reg_hw[0] = 9999;                       /* strh #0x270F, [r2, #0x28] */
        reg_w[0]  = (10000 / value) - 1;        /* udiv + subs + str */
    } else if (value <= 10000) {
        reg_hw[0] = 99;                         /* strh #0x63, [r2, #0x28] */
        reg_w[0]  = (1000000 / value) - 1;      /* udiv + subs + str */
    } else {
        reg_hw[0] = 0;                          /* strh #0, [r2, #0x28] */
        reg_w[0]  = (10000000 / value) - 1;     /* udiv + subs + str */
    }
}
