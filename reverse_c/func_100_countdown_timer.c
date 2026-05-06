/**
 * @file func_100_countdown_timer.c
 * @brief 函数: 倒计时器 — 读取 RAM 计数器若非零则递减
 * @addr  0x0801352C - 0x08013544 (24 bytes, 含字面量池)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 叶函数，读取 RAM 中的 32 位计数器，若非零则减 1 写回。
 *
 * 参数: (无)
 *
 * 调用: (无 — 叶函数, bx lr)
 */

#include "stm32f4xx_hal.h"

#define COUNTDOWN_REG  (*(volatile uint32_t *)0x20000000)

void Countdown_Timer(void)
{
    if (COUNTDOWN_REG != 0) {
        COUNTDOWN_REG = COUNTDOWN_REG - 1;
    }
}
