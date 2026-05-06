/**
 * @file func_36_gpio_input_init.c
 * @brief 函数: GPIO_Input_Init — GPIO 输入引脚初始化 (PC4/PC5 + PA7)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 * @addr  0x0800D748 - 0x0800D798 (80 bytes, 含字面量池)
 *
 * 使能 GPIOC 时钟 (AHB1 bit2, mask=4), 配置两路输入引脚:
 *   - PC4/PC5 (pin_mask=0x30): 输入/高速/无上下拉
 *   - PA7   (pin_mask=0x80):  输入/高速/无上下拉
 *
 * 注: otype (sp+6) 字段未显式初始化, 沿用 push 时 r3 的残留值。
 *     当 mode=0 (输入) 时, otype 在 gpio_port_init 中可能被忽略。
 *
 * 调用:
 *   rcc_ahb1_enable()  @ 0x080113B8 — AHB1 时钟使能 (mask=4 → GPIOC)
 *   gpio_port_init()   @ 0x0800C42C — GPIO 端口配置
 *
 * 字面量池:
 *   0x0800D790: 0x40020800 (GPIOC 基地址)
 *   0x0800D794: 0x40020000 (GPIOA 基地址)
 */

#include "stm32f4xx_hal.h"

/* GPIO 配置结构体 */
typedef struct {
    uint32_t pin_mask;
    uint8_t  mode;
    uint8_t  speed;
    uint8_t  otype;
    uint8_t  pull;
} gpio_cfg_t;

extern void rcc_ahb1_enable(uint32_t bitmask, uint32_t enable);    /* 0x080113B8 */
extern void gpio_port_init(GPIO_TypeDef *port, gpio_cfg_t *cfg);   /* 0x0800C42C */

/* ================================================================
 * GPIO_Input_Init() @ 0x0800D748
 *   push {r2, r3, r4, lr}: r2/r3 同时用于栈对齐和配置结构体存储
 * ================================================================ */
void GPIO_Input_Init(void)
{
    gpio_cfg_t cfg;  /* sp+0..sp+7, 由 push {r2,r3,r4} 分配 */

    /* 使能 GPIOC 时钟 (AHB1 bit2, mask=4) */
    rcc_ahb1_enable(4, 1);                       /* 0x0800D74A-4E: movs r1,#1; movs r0,#4; bl */

    /* ---- 配置 PC4 + PC5: 输入/高速/无上下拉 ---- */
    cfg.pin_mask = 0x30;                         /* 0x0800D752-54: movs r0,#0x30; str r0,[sp] */
    cfg.mode     = 0;                            /* 0x0800D756-58: strb.w [sp,#4] = 0 (输入) */
    cfg.speed    = 3;                            /* 0x0800D75C-5E: strb.w [sp,#5] = 3 (高速) */
    /* cfg.otype 未设置, 使用 r3 残留值 — gpio_port_init 在输入模式下忽略 */
    cfg.pull     = 0;                            /* 0x0800D762-64: strb.w [sp,#7] = 0 (无上下拉) */

    gpio_port_init((GPIO_TypeDef *)0x40020800, &cfg);  /* 0x0800D768-6C: mov r1,sp; ldr r0,=GPIOC; bl */

    /* ---- 配置 PA7: 输入/高速/无上下拉 ---- */
    cfg.pin_mask = 0x80;                         /* 0x0800D770-72: movs r0,#0x80; str r0,[sp] */
    cfg.mode     = 0;                            /* 0x0800D774-76: strb.w [sp,#4] = 0 (输入) */
    cfg.speed    = 3;                            /* 0x0800D77A-7C: strb.w [sp,#5] = 3 (高速) */
    /* cfg.otype 未设置, 沿用前次 r3 残留 */
    cfg.pull     = 0;                            /* 0x0800D780-82: strb.w [sp,#7] = 0 (无上下拉) */

    gpio_port_init((GPIO_TypeDef *)0x40020000, &cfg);  /* 0x0800D786-8A: mov r1,sp; ldr r0,=GPIOA; bl */
}
