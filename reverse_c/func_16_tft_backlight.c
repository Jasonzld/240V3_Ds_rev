/**
 * @file func_16_tft_backlight.c
 * @brief 函数: TFT_Backlight — TFT 液晶屏背光/电源控制
 * @addr  0x0800C504 - 0x0800C538 (52 bytes, 17 指令含常量池)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 控制 TFT 液晶屏背光开关:
 *   写入 FSMC 寄存器 (0x42410284) 控制 TFT 背光状态
 *   同时设置/清除软件定时器标志
 *
 * 参数:
 *   r0 = state (背光状态: 0=关闭, 非0=开启)
 *
 * RAM 变量:
 *   0x200001E0 = backlight_state (背光状态字节)
 *   0x200001E2 = backlight_timer (背光超时计数器, 5000)
 *
 * FSMC 寄存器 (TFT LCD 控制):
 *   0x42410284 = TFT 背光控制寄存器
 */

#include "stm32f4xx_hal.h"

/* RAM 状态变量 */
#define BACKLIGHT_STATE (*(volatile uint8_t  *)0x200001E0)
#define BACKLIGHT_TIMER (*(volatile uint16_t *)0x200001E2)

/* FSMC TFT 控制寄存器 */
#define TFT_BACKLIGHT_REG   (*(volatile uint32_t *)0x42410284)

/* 外部函数声明 */
extern void func_18DE0(void);    /* 0x08018DE0 — TFT/USART 后处理 */
extern void func_183F0(void);    /* 0x080183F0 — TFT 背光开启回调 */

/* ================================================================
 * TFT_Backlight() @ 0x0800C504
 *   r0 = state: 0=关闭背光, 非0=开启背光
 *
 * 操作:
 *   1. 保存背光状态到 RAM
 *   2. 写入 FSMC 寄存器立即生效
 *   3. 若开启: 调用回调 + 设置 5000ms 超时定时器
 *      若关闭: 调用后处理函数
 * ================================================================ */
void TFT_Backlight(uint32_t state)
{
    uint8_t byte_state;          /* r4 */

    byte_state = (uint8_t)state;                 /* 0x0800C506: mov r4, r0 — 保存为字节 */

    /* 写入 RAM 状态变量 */
    BACKLIGHT_STATE = byte_state;                /* 0x0800C508-0A: ldr r0,PC; strb r4,[r0] */

    /* 写入 FSMC TFT 背光寄存器 (立即生效) */
    TFT_BACKLIGHT_REG = BACKLIGHT_STATE;         /* 0x0800C50C-10: ldrb r0,[r0]; ldr r1,PC; str r0,[r1] */

    /* 根据状态分支 */
    if (BACKLIGHT_STATE != 0) {                  /* 0x0800C512-16: ldr r0,PC; ldrb r0,[r0]; cbnz */
        /* 背光开启: 调用回调 */
        func_183F0();                            /* 0x0800C51E */

        /* 设置超时定时器 5000 (单位: ms 或 tick) */
        BACKLIGHT_TIMER = 0x1388;                /* 0x0800C522-28: movw r0,#0x1388; strh r0,[r1] */
    } else {
        /* 背光关闭: 调用后处理 */
        func_18DE0();                            /* 0x0800C518 */
    }
}
