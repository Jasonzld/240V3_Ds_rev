/**
 * @file func_32_gpio_portb_init.c
 * @brief 函数: GPIO_PortB_Init — GPIOB 端口初始化 (PB6/PB7)
 * @addr  0x0800D59C - 0x0800D5E4 (72 bytes, 含字面量池)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 初始化 GPIOB PB6 和 PB7 引脚:
 *   模式: 输出, 速度: 高速(3), 输出类型: 推挽, 上拉: 上拉
 *
 * 调用:
 *   rcc_ahb1_enable()  @ 0x080113B8 — AHB1 时钟使能 (GPIOB=bit1)
 *   gpio_port_init()   @ 0x0800C42C — GPIO 端口配置
 *
 * 设置 RAM 标志:
 *   0x20000384 = 1
 *   0x20000620 = 1  (0x20000384 + 0x29C)
 *
 * 字面量池:
 *   0x0800D5D8: 0x40020400 (GPIOB 基地址)
 *   0x0800D5DC: 0x42408298 (GPIOB 初始化标志, FSMC 寄存器复用)
 *   0x0800D5E0: 0x42408000 (FSMC Bank0 基地址, FLAG_GPIOB_INIT_B = base + 0x29C)
 */

#include "stm32f4xx_hal.h"

#define GPIOB_BASE  0x40020400

/* GPIO 配置结构体 */
typedef struct {
    uint32_t pin_mask;
    uint8_t  mode;
    uint8_t  speed;
    uint8_t  otype;
    uint8_t  pull;
} gpio_cfg_t;

/* RAM 标志 (FSMC 寄存器地址, 复用为标志存储) */
#define FLAG_GPIOB_INIT     (*(volatile uint32_t *)0x42408298)
#define FLAG_GPIOB_INIT_B   (*(volatile uint32_t *)0x4240829C)  /* +0x4 */

extern void rcc_ahb1_enable(uint32_t bitmask, uint32_t enable);     /* 0x080113B8 */
extern void gpio_port_init(GPIO_TypeDef *port, gpio_cfg_t *cfg);    /* 0x0800C42C */

/* ================================================================
 * GPIO_PortB_Init() @ 0x0800D59C
 *   初始化 GPIOB PB6/PB7 为推挽输出
 *
 *   push {r2, r3, r4, lr}: r2/r3 用于栈上的 gpio_cfg_t 结构体
 *   (push 保存原始值的同时在栈上分配空间)
 * ================================================================ */
void GPIO_PortB_Init(void)
{
    gpio_cfg_t cfg;  /* 在栈上: sp+0..sp+7, 由 push {r2,r3,r4} 分配空间 */

    /* 使能 GPIOB 时钟 (AHB1 bit1 = GPIOBEN) */
    rcc_ahb1_enable(2, 1);                       /* 0x0800D59E-A2: movs r1,#1; movs r0,#2; bl */

    /* 配置 PB6 + PB7: 输出/高速/推挽/上拉 */
    cfg.pin_mask = 0xC0;                         /* 0x0800D5A6-A8: movs r0,#0xc0; str r0,[sp] */
    cfg.mode     = 1;                            /* 0x0800D5AA-AC: strb.w [sp,#4] = 1 (输出) */
    cfg.otype    = 0;                            /* 0x0800D5B0-B2: strb.w [sp,#6] = 0 (推挽) */
    cfg.speed    = 3;                            /* 0x0800D5B6-B8: strb.w [sp,#5] = 3 (高速) */
    cfg.pull     = 1;                            /* 0x0800D5BC-BE: strb.w [sp,#7] = 1 (上拉) */

    gpio_port_init((GPIO_TypeDef *)GPIOB_BASE, &cfg);  /* 0x0800D5C2-C6: mov r1,sp; ldr r0,=GPIOB; bl */

    /* 设置初始化完成标志 */
    FLAG_GPIOB_INIT   = 1;                       /* 0x0800D5CA-CE: ldr r1,=FLAG; str r0,[r1] */
    FLAG_GPIOB_INIT_B = 1;                       /* 0x0800D5D0-D2: str.w r0,[r1,#0x29c] */
}
