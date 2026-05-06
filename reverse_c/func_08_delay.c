/**
 * @file func_08_delay.c
 * @brief 延时函数: delay_us + delay_core — SysTick 微秒延时 (func_146 的子函数详解)
 * @addr  0x08018488 - 0x08018524 (156 bytes, 含字面量池)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 两级延时架构:
 *   delay_us   @ 0x08018488 (= func_146 的 SysTick_Delay_Ms 子函数 A)
 *   delay_core @ 0x080184C0 (= func_146 的 SysTick_Trigger 子函数 B)
 *
 * 注意: 此文件是对 func_146_systick_timer_scheduler.c 中两个子函数的独立详解,
 *       实际函数位于 func_146 的范围内 (0x08018488-0x08018C1C).
 */

#include "stm32f4xx_hal.h"

/* SysTick 寄存器 (Cortex-M4 系统控制块) */
#define SysTick_BASE    0xE000E010
#define SysTick_CTRL    (*(volatile uint32_t *)(SysTick_BASE + 0x00))
#define SysTick_LOAD    (*(volatile uint32_t *)(SysTick_BASE + 0x04))
#define SysTick_VAL     (*(volatile uint32_t *)(SysTick_BASE + 0x08))

/* 校准因子指针: 指向每微秒 SysTick 时钟周期数 */
#define US_CALIB_PTR    (*(volatile uint32_t *)0x08018508)
#define US_CALIB        (*(volatile uint16_t *)US_CALIB_PTR)

/* ================================================================
 * delay_core() @ 0x080184C0
 *   r0 = 微秒数 (≤540)
 *
 * 使用 SysTick 硬件倒计时实现精确延时:
 *   1. ticks = us * US_CALIB (每微秒时钟数)
 *   2. SysTick LOAD = ticks, VAL = 0
 *   3. 使能 SysTick (CTRL |= 1)
 *   4. 轮询 CTRL bit16 (COUNTFLAG) 直到置位
 *   5. 禁用 SysTick (CTRL &= ~1), VAL = 0
 * ================================================================ */
void delay_core(uint32_t us)
{
    uint32_t ticks;              /* r2 */

    ticks = us * US_CALIB;       /* 0x080184C4-C6: ldrh+muls */

    SysTick_LOAD = ticks;        /* 0x080184CC: str [r3, #0x14] */
    SysTick_VAL  = 0;            /* 0x080184D0: str [r3, #0x18] */

    SysTick_CTRL |= 1;           /* 0x080184D6-DA: orr #1 → ENABLE */

    /* 0x080184DC: nop */

    /* 轮询 COUNTFLAG (bit16), 同时检查 ENABLE 位作为防护退出 */
    do {
        if (!(SysTick_CTRL & 1)) break;  /* 0x080184E0-E2: ldr r0,[r2,#0x10]; and r2,r0,#1; cbz → 若 ENABLE 被意外清除则退出 */
    } while (!(SysTick_CTRL & 0x10000));  /* 0x080184E6-EC: and r2,r0,#0x10000; cmp r2,#0; beq → 等待 COUNTFLAG */

    SysTick_CTRL &= ~1;          /* 0x080184F8: bic #1 → 禁用 SysTick */
    SysTick_VAL   = 0;           /* 0x08018504 */
}

/* ================================================================
 * delay_us() @ 0x08018488
 *   r0 = 微秒数 (任意值)
 *
 * 将延时拆分为 540us 块以避免 32-bit tick 溢出:
 *   whole = us / 540 (8-bit, 最多 255 块)
 *   remainder = us % 540 (16-bit)
 *   循环 whole 次调用 delay_core(540)
 *   若 remainder > 0: 调用 delay_core(remainder)
 * ================================================================ */
void delay_us(uint32_t us)
{
    uint8_t  whole;              /* r6 — 540us 块数 */
    uint16_t remainder;          /* r5 — 余量微秒数 */

    /* r4 = us (原始参数, callee-saved, mov r4,r0 @ 0x0801848A) */
    whole     = (uint8_t)(us / 540);   /* 0x08018490-94: sdiv #0x21C, uxtb */
    remainder = (uint16_t)(us % 540);  /* 0x0801849A-A2: sdiv, mls, uxth */

    while (whole != 0) {                   /* 0x080184B2-B4 */
        delay_core(540);                   /* 0x080184A6-AA */
        whole--;                           /* 0x080184AE-B0 */
    }

    if (remainder != 0) {                  /* 0x080184B6 */
        delay_core(remainder);             /* 0x080184B8-BA */
    }
    /* 0x080184BE: pop {r4,r5,r6,pc} */
}
