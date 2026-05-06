/**
 * @file func_15_gpio_af_config.c
 * @brief 函数: GPIO_AFConfig — 配置 GPIO 引脚复用功能 (AFR)
 * @addr  0x0800C4BC - 0x0800C504 (72 bytes, 26 指令)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 写入 GPIOx_AFRL/AFRH 寄存器, 为指定引脚设置复用功能编号。
 * STM32F4 每个 GPIO 端口有 AFR 寄存器组:
 *   AFRL @ GPIOx + 0x20 → 引脚 0~7 (每引脚 4 bit)
 *   AFRH @ GPIOx + 0x24 → 引脚 8~15
 *
 * 参数:
 *   r0 = GPIO 端口基地址
 *   r1 = 引脚编号 (0~15)
 *   r2 = 复用功能编号 (AF0~AF15)
 *
 * 示例 (USART6_Init):
 *   GPIO_AFConfig(GPIOC, 6, 8);  // PC6 → AF8 (USART6_TX)
 *   GPIO_AFConfig(GPIOC, 7, 8);  // PC7 → AF8 (USART6_RX)
 */

#include "stm32f4xx_hal.h"

/* GPIO AFR 寄存器偏移 */
#define GPIO_AFRL_OFFSET  0x20
#define GPIO_AFRH_OFFSET  0x24

/* ================================================================
 * GPIO_AFConfig() @ 0x0800C4BC
 *   r0 = GPIOx 基地址
 *   r1 = pin  (0~15)
 *   r2 = af   (复用功能编号 0~15)
 *
 * 操作:
 *   1. 计算寄存器索引 = pin / 8 (0→AFRL, 1→AFRH)
 *   2. 计算位偏移     = (pin % 8) * 4
 *   3. 清除目标 4-bit 位域
 *   4. 写入 af 值到该位域
 *
 * 注: 与 SYSCFG_EXTIConfig (func_14) 模式相同, 但操作
 *     的是 GPIO_AFR 寄存器 (offset 0x20) 而非 SYSCFG_EXTICR。
 * ================================================================ */
void GPIO_AFConfig(GPIO_TypeDef *GPIOx, uint32_t pin, uint32_t af)
{
    uint32_t afr_idx;            /* r6/r7 — AFR 寄存器索引 (pin / 8) */
    uint32_t bit_pos;            /* r5/r7 — 位偏移 (pin % 8 * 4) */
    uint32_t mask;               /* r6 — 清除掩码 (0xF << bit_pos) */
    uint32_t af_val;             /* r3 — AF 值左移到位 (af << bit_pos) */
    uint32_t reg_val;            /* r5 — 寄存器当前值 */
    volatile uint32_t *afr;      /* r5/r6 — AFR 寄存器地址 */

    /* 计算 AF 值位偏移: (pin & 7) * 4 */
    bit_pos = (pin << 29) >> 27;                 /* 0x0800C4C2-C4: lsl #0x1D; lsr #0x1B */
    /* bit_pos = (pin & 0x7) * 4 */

    /* AF 值左移到目标位 */
    af_val = af << bit_pos;                      /* 0x0800C4C6: lsl.w r3, r2, r5 */

    /* 寄存器索引: pin / 8 */
    afr_idx = pin / 8;                           /* 0x0800C4CA: asrs r6, r1, #3 */

    /* AFR 基地址 = GPIOx + 0x20 (AFRL) */
    afr = (volatile uint32_t *)((uint32_t)GPIOx + GPIO_AFRL_OFFSET);  /* 0x0800C4CC: add.w r5, r0, #0x20 */

    /* ---- 读取-修改-写入: 清除位域 ---- */
    reg_val = afr[afr_idx];                      /* 0x0800C4D0: ldr.w r5, [r5, r6, lsl #2] */

    /* 创建清除掩码: 0xF << bit_pos */
    bit_pos = (pin << 29) >> 27;                 /* 0x0800C4D4-D6: 重新计算位偏移 */
    mask = 0xF << bit_pos;                       /* 0x0800C4D8-DA: mov r6,#0xF; lsl r6,r7 */

    reg_val &= ~mask;                            /* 0x0800C4DC: bic r5, r6 */

    afr_idx = pin / 8;                           /* 0x0800C4DE: asrs r7, r1, #3 */
    afr = (volatile uint32_t *)((uint32_t)GPIOx + GPIO_AFRL_OFFSET);  /* 0x0800C4E0: add.w r6,r0,#0x20 */
    afr[afr_idx] = reg_val;                      /* 0x0800C4E4: str.w r5, [r6, r7, lsl #2] */

    /* ---- 读取-修改-写入: 设置新 AF 值 ---- */
    afr_idx = pin / 8;                           /* 0x0800C4E8: asrs r6, r1, #3 */
    afr = (volatile uint32_t *)((uint32_t)GPIOx + GPIO_AFRL_OFFSET);  /* 0x0800C4EA: add.w r5,r0,#0x20 */
    reg_val = afr[afr_idx];                      /* 0x0800C4EE: ldr.w r5, [r5, r6, lsl #2] */

    reg_val |= af_val;                           /* 0x0800C4F2: orr.w r4, r5, r3 */

    afr_idx = pin / 8;                           /* 0x0800C4F6: asrs r6, r1, #3 */
    afr = (volatile uint32_t *)((uint32_t)GPIOx + GPIO_AFRL_OFFSET);  /* 0x0800C4F8: add.w r5,r0,#0x20 */
    afr[afr_idx] = reg_val;                      /* 0x0800C4FC: str.w r4, [r5, r6, lsl #2] */
}
