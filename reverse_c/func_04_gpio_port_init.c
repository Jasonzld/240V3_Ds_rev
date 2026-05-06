/**
 * @file func_04_gpio_port_init.c
 * @brief 函数: gpio_port_init — 按 pin_mask 配置单个 GPIO 端口
 * DISASSEMBLY-TRACED. Verified against Capstone.
 * @addr  0x0800C42C - 0x0800C4BC (144 bytes, 含字面量池)
 *
 * 遍历 GPIO 端口 16 个引脚 (0~15), 若引脚在 pin_mask 中置位则配置:
 *   - MODER   (offset 0x00): 模式 (输入/输出/AF/模拟) — 始终写入
 *   - OSPEEDR (offset 0x08): 输出速度 — 仅在 mode==1(输出) 或 mode==2(AF) 时写入
 *   - OTYPER  (offset 0x04): 输出类型 — 仅在 mode==1(输出) 或 mode==2(AF) 时写入
 *   - PUPDR   (offset 0x0C): 上下拉 — 始终写入
 *
 * GPIO 配置结构体 gpio_cfg_t (8字节):
 *   [0] uint32_t pin_mask  — 位掩码, 置1的引脚需配置
 *   [4] uint8_t  mode      — 0=输入, 1=输出, 2=复用(AF), 3=模拟
 *   [5] uint8_t  speed     — 0=低速, 1=中速, 2=高速, 3=超高速
 *   [6] uint8_t  otype     — 0=推挽, 1=开漏
 *   [7] uint8_t  pull      — 0=无, 1=上拉, 2=下拉
 *
 * 被调用处: GPIO_Init (func_02), ADC_DMA_Init (func_03)
 */

#include "stm32f4xx_hal.h"

/* GPIO 配置结构体 */
typedef struct {
    uint32_t pin_mask;   /* [0] 引脚位掩码 */
    uint8_t  mode;       /* [4] GPIO 模式: 0=输入, 1=输出, 2=AF, 3=模拟 */
    uint8_t  speed;      /* [5] 输出速度: 0=低速, 1=中速, 2=高速, 3=超高速 */
    uint8_t  otype;      /* [6] 输出类型: 0=推挽, 1=开漏 */
    uint8_t  pull;       /* [7] 上拉/下拉: 0=无, 1=上拉, 2=下拉 */
} gpio_cfg_t;

/* ================================================================
 * gpio_port_init() — 配置指定 GPIO 端口的引脚
 *   r0 = GPIOA / GPIOB / GPIOC 基地址
 *   r1 = gpio_cfg_t 配置结构体指针
 * ================================================================ */
void gpio_port_init(GPIO_TypeDef *port, gpio_cfg_t *cfg)
{
    uint32_t pin;        /* r1 — 引脚索引 (0-15) */
    uint32_t bit_mask;   /* r3 — 单引脚位掩码 (1 << pin) */
    uint32_t match;      /* r4 — pin_mask & bit_mask */

    for (pin = 0; pin < 16; pin++) {   /* 0x0800C438→C4B6 (cmp r1,#0x10, blo loop) */

        bit_mask = 1 << pin;           /* 0x0800C43C: lsl r3, r5, r1 */
        match = cfg->pin_mask & bit_mask; /* 0x0800C440-42: ldr r5,[r2]; and r4,r5,r3 */
        if (match != bit_mask) {        /* 0x0800C446-48: cmp r4,r3; bne skip */
            continue;                   /* 引脚未在 mask 中, 跳过 */
        }

        /* ---- MODER: 模式寄存器 (offset 0x00, 每引脚2位) ---- */
        port->MODER &= ~(3 << (pin * 2));           /* 0x0800C44A-54: bic 2-bit field */
        port->MODER |= (cfg->mode << (pin * 2));    /* 0x0800C456-60: orr mode into field */

        /* ---- OSPEEDR + OTYPER: 仅 mode==1(输出) 或 mode==2(AF) 时配置 ---- */
        if (cfg->mode == 1 || cfg->mode == 2) {     /* 0x0800C462-6C: cmp #1; beq; cmp #2; bne skip */

            /* OSPEEDR: 输出速度 (offset 0x08, 每引脚2位) */
            port->OSPEEDR &= ~(3 << (pin * 2));         /* 0x0800C46E-78: bic speed field */
            port->OSPEEDR |= (cfg->speed << (pin * 2)); /* 0x0800C47A-84: orr speed into field */

            /* OTYPER: 输出类型 (offset 0x04, 每引脚1位) */
            port->OTYPER &= ~(1 << pin);                    /* 0x0800C486-8E: bic type bit */
            port->OTYPER |= ((cfg->otype & 0xFFFF) << pin); /* 0x0800C490-9A: orr otype (16-bit uxth) */
        }

        /* ---- PUPDR: 上下拉寄存器 (offset 0x0C, 每引脚2位) — 始终配置 ---- */
        port->PUPDR &= ~(3 << (pin * 2));          /* 0x0800C49C-CA6: bic pull field */
        port->PUPDR |= (cfg->pull << (pin * 2));   /* 0x0800C4A8-B2: orr pull into field */
    }
    /* 0x0800C4BA: pop {r4,r5,r6,r7,pc} */
}
