/**
 * @file func_27_tft_backlight_on.c
 * @brief 函数: TFT_BacklightOn — TFT 背光开启快捷函数
 * @addr  0x0800D44C - 0x0800D456 (10 bytes, 4 指令)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 简单封装: 调用 TFT_Backlight(1) 开启背光。
 * 等价于 TFT_Backlight(1), 但为零参数接口。
 *
 * 调用:
 *   TFT_Backlight() @ 0x0800C504 — 背光控制 (func_16)
 */

#include "stm32f4xx_hal.h"

extern void TFT_Backlight(uint32_t state);   /* 0x0800C504 */

/* ================================================================
 * TFT_BacklightOn() @ 0x0800D44C
 *   开启 TFT 背光 (调用 TFT_Backlight(1))
 *   指令: push {r4,lr}; movs r0,#1; bl TFT_Backlight; pop {r4,pc}
 * ================================================================ */
void TFT_BacklightOn(void)
{
    TFT_Backlight(1);                            /* 0x0800D44E-50: movs r0,#1; bl #0x0800C504 */
}
