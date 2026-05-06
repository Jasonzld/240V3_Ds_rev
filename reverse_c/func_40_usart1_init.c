/**
 * @file func_40_usart1_init.c
 * @brief 函数: USART1_Init — USART1 初始化与配置
 * @addr  0x0800D8C4 - 0x0800D9A6 (226 bytes, 含字面量池)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 初始化 USART1 (PA9=TX, PA10=RX, AF7, 115200? baud),
 * 执行启动握手序列 (发送 0xFF, NAK 帧, 延时回调)。
 *
 * 参数:
 *   r0 = baud_cfg (传递给 USART 配置结构体 sp+8)
 *
 * 初始化步骤:
 *   1. 使能 GPIOA + USART1 时钟
 *   2. 配置 PA9/PA10 为 AF7 (USART1)
 *   3. gpio_port_init → AF/高速/推挽/上拉
 *   4. USART 外设配置 (func_15E50, func_15D62, func_15E06)
 *   5. 子模块配置 (func_41)
 *   6. USART 使能 (func_15D80)
 *   7. 启动握手: 发送 0xFF → 延时 100 → NAK → 延时 100 → func_12604(0) → func_12454(0)
 *
 * 调用:
 *   rcc_ahb1_enable()  @ 0x080113B8 — 使能 GPIOA
 *   rcc_apb2_enable()  @ 0x080113F8 — 使能 USART1 (APB2 bit4=0x10)
 *   gpio_pin_cfg()     @ 0x0800C4BC — 引脚 AF 配置 (pin, AF)
 *   gpio_port_init()   @ 0x0800C42C — GPIO 端口初始化
 *   func_15E50()       @ 0x08015E50 — USART 参数配置
 *   func_15D62()       @ 0x08015D62 — USART 模式配置
 *   func_15E06()       @ 0x08015E06 — USART 使能控制
 *   func_15D80()       @ 0x08015D80 — USART 启动
 *   func_41()          @ 0x0800D9B4 — 子模块初始化
 *   func_18DC0()       @ 0x08018DC0 — 额外初始化
 *   func_18488()       @ 0x08018488 — 延时回调 (参数 0x64 = 100)
 *   send_nak_frame()   @ 0x0800D3E4 — NAK 帧发送
 *   func_12604()       @ 0x08012604 — 操作 (mode=0)
 *   func_12454()       @ 0x08012454 — 操作 (mode=0)
 *   uart_send_byte()   @ 0x080123E8 — 发送单字节
 *
 * 字面量池: 0x0800D998-0x0800D9A4
 *   0x0800D998: 0x40020000 (GPIOA 基地址)
 *   0x0800D99C: 0x40011000 (USART1 基地址)
 */

#include "stm32f4xx_hal.h"

#define GPIOA_BASE   0x40020000
#define USART1_BASE  0x40011000

/* GPIO 引脚配置结构体 (gpio_port_init 使用) */
typedef struct {
    uint32_t pin_mask;
    uint8_t  mode;
    uint8_t  speed;
    uint8_t  otype;
    uint8_t  pull;
} gpio_cfg_t;

/* USART 初始化结构体 (func_15E50 使用) */
typedef struct {
    uint32_t baud;       /* +0x00 */
    uint16_t word_len;   /* +0x04 */
    uint16_t stop_bits;  /* +0x06 */
    uint16_t parity;     /* +0x08 */
    uint16_t mode;       /* +0x0A */
    uint16_t hw_flow;    /* +0x0C */
} usart_cfg_t;

extern void rcc_ahb1_enable(uint32_t bitmask, uint32_t enable);   /* 0x080113B8 */
extern void rcc_apb2_enable(uint32_t bitmask, uint32_t enable);   /* 0x080113F8 */
extern void gpio_pin_cfg(uint32_t port, uint32_t pin, uint32_t af); /* 0x0800C4BC */
extern void gpio_port_init(GPIO_TypeDef *port, gpio_cfg_t *cfg);  /* 0x0800C42C */
extern void func_15E50(USART_TypeDef *uart, usart_cfg_t *cfg);    /* 0x08015E50 */
extern void func_15D62(USART_TypeDef *uart, uint32_t mode);       /* 0x08015D62 */
extern void func_15E06(USART_TypeDef *uart, uint32_t mode, uint32_t en); /* 0x08015E06 */
extern void func_15D80(USART_TypeDef *uart, uint32_t en);         /* 0x08015D80 */
extern void func_41(uint8_t *cfg);                                 /* 0x0800D9B4 */
extern void func_18DC0(void);                                      /* 0x08018DC0 */
extern void func_18488(uint32_t delay);                            /* 0x08018488 */
extern void send_nak_frame(void);                                  /* 0x0800D3E4 */
extern void func_12604(uint32_t mode);                             /* 0x08012604 */
extern void func_12454(uint32_t mode);                             /* 0x08012454 */
extern void uart_send_byte(uint8_t byte);                          /* 0x080123E8 */

/* ================================================================
 * USART1_Init() @ 0x0800D8C4
 *   push {r4, lr}; sub sp, #0x20 — 32 字节栈空间
 *   r4 = r0 (baud config parameter)
 * ================================================================ */
void USART1_Init(uint32_t baud_cfg)
{
    gpio_cfg_t  gpio_cfg;  /* sp+0x18..sp+0x1F */
    usart_cfg_t uart_cfg;  /* sp+0x08..sp+0x14 */
    uint8_t     sub_cfg[4]; /* sp+0x04..sp+0x07 */

    /* ---- 1. 时钟使能 ---- */
    rcc_ahb1_enable(1, 1);                       /* 0x0800D8CA-CE: movs r1,#1; mov r0,r1; bl — GPIOA */
    rcc_apb2_enable(0x10, 1);                    /* 0x0800D8D2-D6: movs r1,#1; movs r0,#0x10; bl — USART1 */

    /* ---- 2. 引脚 AF 配置 ---- */
    gpio_pin_cfg(GPIOA_BASE, 9, 7);             /* 0x0800D8DA-E0: movs r2,#7; movs r1,#9; bl — PA9, AF7 */
    gpio_pin_cfg(GPIOA_BASE, 10, 7);            /* 0x0800D8E4-EA: movs r2,#7; movs r1,#0xa; bl — PA10, AF7 */

    /* ---- 3. GPIO 端口初始化 (PA9+PA10: AF/高速/推挽/上拉) ---- */
    gpio_cfg.pin_mask = 0x600;                   /* 0x0800D8EE-F2: mov.w r0,#0x600; str r0,[sp,#0x18] */
    gpio_cfg.mode     = 2;                       /* 0x0800D8F4-F6: strb.w [sp,#0x1C] = 2 (AF) */
    gpio_cfg.speed    = 2;                       /* 0x0800D8FA: strb.w [sp,#0x1D] = 2 (高速) */
    gpio_cfg.otype    = 0;                       /* 0x0800D8FE-900: strb.w [sp,#0x1E] = 0 (推挽) */
    gpio_cfg.pull     = 1;                       /* 0x0800D904-06: strb.w [sp,#0x1F] = 1 (上拉) */

    gpio_port_init((GPIO_TypeDef *)GPIOA_BASE, &gpio_cfg);  /* 0x0800D90A-0E: ldr r0,=GPIOA; bl */

    /* ---- 4. USART 外设参数配置 ---- */
    uart_cfg.baud      = baud_cfg;               /* 0x0800D912: str r4, [sp, #8] — 波特率参数 */
    uart_cfg.word_len  = 0;                      /* 0x0800D916: strh.w [sp,#0xC] = 0 */
    uart_cfg.stop_bits = 0;                      /* 0x0800D91A: strh.w [sp,#0xE] = 0 */
    uart_cfg.parity    = 0;                      /* 0x0800D91E: strh.w [sp,#0x10] = 0 */
    uart_cfg.mode      = 12;                     /* 0x0800D926-28: strh.w [sp,#0x12] = 12 */
    uart_cfg.hw_flow   = 0;                      /* 0x0800D922: strh.w [sp,#0x14] = 0 */

    func_15E50((USART_TypeDef *)USART1_BASE, &uart_cfg);  /* 0x0800D92C-30: ldr r0,=USART1; bl */

    /* 附加 USART 配置 */
    func_15D62((USART_TypeDef *)USART1_BASE, 0x525);      /* 0x0800D934-3A: movw r1,#0x525; bl */
    func_15E06((USART_TypeDef *)USART1_BASE, 0x525, 1);   /* 0x0800D93E-46: movs r2,#1; movw r1,#0x525; bl */

    /* ---- 5. 子模块配置 ---- */
    sub_cfg[0] = 0x25;                           /* 0x0800D94A-4C: strb.w [sp,#4] = 0x25 */
    sub_cfg[1] = 3;                              /* 0x0800D950-52: strb.w [sp,#5] = 3 */
    sub_cfg[2] = 3;                              /* 0x0800D956: strb.w [sp,#6] = 3 */
    sub_cfg[3] = 1;                              /* 0x0800D95A-5C: strb.w [sp,#7] = 1 */

    func_41(sub_cfg);                            /* 0x0800D960-62: add r0,sp,#4; bl func_41 */

    /* ---- 6. USART 使能 ---- */
    func_15D80((USART_TypeDef *)USART1_BASE, 1);  /* 0x0800D966-6A: movs r1,#1; ldr r0,=USART1; bl */

    /* ---- 7. 启动握手序列 ---- */
    uart_send_byte(0xFF);                        /* 0x0800D96E-70: movs r0,#0xff; bl — 唤醒/同步字节 */
    func_18DC0();                                /* 0x0800D974: bl — 额外初始化 */
    func_18488(100);                             /* 0x0800D978-7A: movs r0,#0x64; bl — 延时 100 */
    send_nak_frame();                            /* 0x0800D97E: bl — 发送 NAK 帧 */
    func_18488(100);                             /* 0x0800D982-84: movs r0,#0x64; bl — 延时 100 */
    func_12604(0);                               /* 0x0800D988-8A: movs r0,#0; bl */
    func_12454(0);                               /* 0x0800D98E-90: movs r0,#0; bl */
}
