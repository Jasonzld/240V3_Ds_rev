/**
 * @file func_02_gpio_init.c
 * @brief 函数: GPIO_Init — 全部 GPIO 端口初始化
 * @addr  0x0800C2DC - 0x0800C42C (336 bytes, ~168 指令)
 * DISASSEMBLY-TRACED. Verified against Capstone (entry + key sections).
 *
 * 配置 STM32F410 全部 3 个 GPIO 端口 (GPIOA/GPIOB/GPIOC):
 *   - 输出引脚: 推挽, 高速, 上拉 (电解槽开关, 泵控制, 蜂鸣器等)
 *   - 输入引脚: 上拉/下拉 (红外传感器, 流量计等)
 *   - 模拟引脚: ADC 电流采样
 *   - 复用功能: USART1/6, I2C, TIM 等
 *   - FSMC 外部寄存器: TFT 背光/复位控制
 *
 * 调用辅助函数:
 *   - gpio_port_init(GPIO_TypeDef*, gpio_cfg_t*): 0x0800C42C — 按 pin_mask 配置端口
 *   - Func_AF_Config(uint32_t, uint32_t): 0x080119BC — 复用功能配置
 *   - Func_ExtiConfig(exti_cfg_t*): 0x0800BCE8 — 外部中断配置
 *   - Func_UartPinConfig(uart_cfg_t*): 0x0800D9B4 — USART 引脚配置
 */

#include "stm32f4xx_hal.h"

/* ================================================================
 * GPIO 配置结构体
 * ================================================================ */

/* 通用 GPIO 端口配置 (8字节) */
typedef struct {
    uint32_t pin_mask;   /* 位掩码: 置1的位表示要配置的引脚 */
    uint8_t  mode;       /* GPIO 模式: 0=输入, 1=输出, 2=AF, 3=模拟 */
    uint8_t  speed;      /* 输出速度: 0=低速, 1=中速, 2=高速, 3=超高速 */
    uint8_t  otype;      /* 输出类型: 0=推挽, 1=开漏 */
    uint8_t  pull;       /* 上拉/下拉: 0=无, 1=上拉, 2=下拉 */
} gpio_cfg_t;

/* 外部中断/事件配置 (4字节) */
typedef struct {
    uint32_t pin_mask;   /* 引脚掩码 */
    uint8_t  port;       /* 端口选择 */
    uint8_t  mode;       /* 触发模式 */
    uint8_t  edge;       /* 边沿选择 */
} exti_cfg_t;

/* USART 引脚配置 (4字节) */
typedef struct {
    uint8_t  tx_pin;     /* TX 引脚编号 */
    uint8_t  rx_pin;     /* RX 引脚编号(2=未使用) */
    uint8_t  rts_pin;    /* RTS 引脚编号(0=未使用) */
    uint8_t  cts_pin;    /* CTS 引脚编号(1=未使用) */
} uart_pin_cfg_t;

/* ================================================================
 * FSMC 外部寄存器 (TFT LCD 控制)
 * ================================================================ */
#define FSMC_REG_42AC      (*(volatile uint32_t *)0x424082AC)
#define FSMC_REG_40000     (*(volatile uint32_t *)0x42400000)
#define FSMC_REG_42B4      (*(volatile uint32_t *)0x424082B4)  /* 0x424082AC + 0x08 */
#define FSMC_REG_0290      (*(volatile uint32_t *)0x42400290)
#define FSMC_REG_42A0      (*(volatile uint32_t *)0x424082A0)  /* TFT复位: 0x424082AC-0xC = 0x42408000+0x2A0 */
#define FSMC_REG_02BC      (*(volatile uint32_t *)0x424002BC)  /* 0x42400000 + 0x2BC */
#define FSMC_REG_42B0      (*(volatile uint32_t *)0x424082B0)  /* 0x42408000 + 0x2B0 */

/* ================================================================
 * 外部函数声明
 * ================================================================ */
extern void Func_SystemInit_1(uint32_t a, uint32_t b);   /* 0x080113B8 */
extern void Func_SystemInit_2(uint32_t a, uint32_t b);   /* 0x080113F8 */
extern void gpio_port_init(GPIO_TypeDef *port, gpio_cfg_t *cfg); /* 0x0800C42C */
extern void Func_AF_Config(uint32_t sel, uint32_t mask);  /* 0x080119BC */
extern void Func_ExtiConfig(exti_cfg_t *cfg);             /* 0x0800BCE8 */
extern void Func_UartPinConfig(uart_pin_cfg_t *cfg);      /* 0x0800D9B4 */
extern void delay_us(uint32_t us);                        /* 0x08018488 */

/* ================================================================
 * GPIO_Init() — 全部 GPIO 端口初始化
 * ================================================================ */
void GPIO_Init(void)
{
    gpio_cfg_t cfg;          /* [sp+4] 通用 GPIO 配置 */
    exti_cfg_t exti_cfg;     /* [sp+0xC] 外部中断配置 */
    uart_pin_cfg_t uart_cfg; /* [sp] USART 引脚配置 */

    /* ---- 系统级初始化 ---- */
    Func_SystemInit_1(7, 1);                /* 0x0800C2E0: 初始化系统控制器 */
    Func_SystemInit_2(0x4000, 1);           /* 0x0800C2E8: 初始化外设(bit14=1) */

    /* ---- FSMC/TFT 控制寄存器初始化 ---- */
    FSMC_REG_42AC = 0;                      /* 0x0800C2F0: *(0x424082AC) = 0 */
    FSMC_REG_02BC = 1;                      /* 0x0800C2F6: *(0x424002BC) = 1 */
    FSMC_REG_42B4 = 0;                      /* 0x0800C2FE: *(0x424082B4) = 0 */
    FSMC_REG_0290 = 0;                      /* 0x0800C306: *(0x42400290) = 0 */
    FSMC_REG_42A0 = 1;                      /* 0x0800C30A: *(0x424082A0) = 1 */
    FSMC_REG_42B0 = 1;                      /* 0x0800C312: *(0x424082B0) = 1 */

    /* ============================================================ */
    /* 第1轮: GPIO 输出引脚配置 (所有端口统一: 输出/超高速/推挽/上拉) */
    /* ============================================================ */
    cfg.mode  = 1;    /* 通用输出 */
    cfg.speed = 3;    /* 超高速 */
    cfg.otype = 0;    /* 推挽 */
    cfg.pull  = 1;    /* 上拉 */

    /* GPIOA 输出引脚: PA4, PA8, PA15 */
    cfg.pin_mask = 0x8110;                  /* 0x0800C318 */
    gpio_port_init(GPIOA, &cfg);            /* 0x0800C33A */

    /* GPIOB 输出引脚: PB8, PB11, PB12, PB13, PB14 */
    cfg.pin_mask = 0x7900;                  /* 0x0800C33E */
    gpio_port_init(GPIOB, &cfg);            /* 0x0800C348 */

    /* GPIOC 输出引脚: PC1, PC2, PC3, PC8, PC9, PC10, PC11 */
    cfg.pin_mask = 0x0F0E;                  /* 0x0800C34C */
    gpio_port_init(GPIOC, &cfg);            /* 0x0800C356 */

    /* ============================================================ */
    /* 第2轮: GPIOB 输入/模拟配置 (PB0, PB1, PB2) */
    /* ============================================================ */
    cfg.mode  = 0;    /* 输入模式 */
    cfg.speed = 3;    /* 超高速 (对输入无效, 但保持值) */
    cfg.otype = 0;    /* 推挽 (对输入无效) */
    cfg.pull  = 1;    /* 上拉 */

    cfg.pin_mask = 0x0007;                  /* PB0, PB1, PB2 */
    gpio_port_init(GPIOB, &cfg);            /* 0x0800C374 */

    /* ============================================================ */
    /* 第3轮: GPIOA PA7 (ADC 电流传感器模拟输入) */
    /* 注: mode=0 设为输入模式, ADC_Init 稍后会将 MODER 覆盖为模拟模式 */
    /* ============================================================ */
    cfg.mode  = 0;    /* 输入模式 (ADC_Init 会覆盖为模拟) */
    cfg.pull  = 1;    /* 上拉 */

    cfg.pin_mask = 0x0080;                  /* PA7: 电流传感器 ADC 输入 */
    gpio_port_init(GPIOA, &cfg);            /* 0x0800C392 */

    /* ============================================================ */
    /* 第4轮: GPIOB 复用功能 (AF: TIM/I2C/USART/SPI) */
    /* 注: mode 保持=0, 各外设 init 函数稍后设置 MODER 为 AF 模式 */
    /* ============================================================ */

    cfg.pin_mask = 0x8238;                  /* PB3, PB4, PB5, PB9, PB10, PB15 */
    gpio_port_init(GPIOB, &cfg);            /* 0x0800C3A0 */

    /* ============================================================ */
    /* 第5轮: GPIOC 复用功能 (AF: USART/SPI) */
    /* ============================================================ */
    cfg.pin_mask = 0x1031;                  /* PC0, PC4, PC5, PC12 */
    gpio_port_init(GPIOC, &cfg);            /* 0x0800C3AE */

    /* ============================================================ */
    /* 复用功能选择配置 (AFx 寄存器编程) */
    /* ============================================================ */
    Func_AF_Config(1, 0xF);                 /* 0x0800C3B2: AF 选择组1, 4位掩码 */

    /* ============================================================ */
    /* 外部中断配置 (PC15: 红外传感器?) */
    /* ============================================================ */
    exti_cfg.pin_mask = 0x8000;             /* PC15 */
    exti_cfg.mode     = 0;                  /* 中断模式 */
    exti_cfg.edge     = 12;                 /* 触发边沿: 12=上升沿+下降沿? */
    exti_cfg.port     = 1;                  /* 端口选择 */
    Func_ExtiConfig(&exti_cfg);             /* 0x0800C3D4 */

    /* ============================================================ */
    /* USART 引脚配置 */
    /* ============================================================ */
    uart_cfg.tx_pin   = 0x28;               /* TX: 某端口 pin 8? */
    uart_cfg.rx_pin   = 2;                  /* RX: pin 2 */
    uart_cfg.rts_pin  = 0;                  /* RTS: 未使用 */
    uart_cfg.cts_pin  = 1;                  /* CTS: 未使用(1标记) */
    Func_UartPinConfig(&uart_cfg);          /* 0x0800C3F2 */

    /* ============================================================ */
    /* TFT 复位脉冲: 低→延迟100us→高 */
    /* ============================================================ */
    FSMC_REG_42A0 = 0;                      /* 0x0800C3F6: *(0x424082A0) = 0 */
    delay_us(100);                          /* 0x0800C3FE: 100us 复位脉冲 */
    FSMC_REG_42A0 = 1;                      /* 0x0800C404: *(0x424082A0) = 1 释放复位 */
}
