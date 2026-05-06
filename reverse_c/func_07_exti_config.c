/**
 * @file func_07_exti_config.c
 * @brief 函数: Func_ExtiConfig — EXTI 外部中断/事件配置
 * @addr  0x0800BCE8 - 0x0800BD7C (148 bytes, 含字面量池)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 配置 STM32F410 EXTI 控制器的中断/事件线:
 *   - 中断屏蔽 (IMR) 和事件屏蔽 (EMR)
 *   - 上升沿/下降沿触发选择 (RTSR/FTSR)
 *   - 单边沿或双边沿模式
 *
 * 配置结构体 exti_cfg_t (8字节):
 *   [0] uint32_t pin_mask   — 引脚位掩码 (如 0x8000=PC15)
 *   [4] uint8_t  reg_offset — 主寄存器偏移 (0=IMR)
 *   [5] uint8_t  edge_mode  — 边沿选择偏移 (0x08=RTSR, 0x0C=FTSR, 0x10=双边沿)
 *   [6] uint8_t  enable     — 0=禁用(BIC), 非0=使能(ORR)
 */

#include "stm32f4xx_hal.h"

/* EXTI 基地址 */
#define EXTI_BASE    0x40013C00

/* EXTI 配置结构体 */
typedef struct {
    uint32_t pin_mask;       /* [0] 引脚位掩码 */
    uint8_t  reg_offset;     /* [4] EXTI 寄存器偏移 (主目标) */
    uint8_t  edge_mode;      /* [5] 边沿选择: 0x08=RTSR, 0x0C=FTSR, 0x10=双边沿 */
    uint8_t  enable;         /* [6] 0=禁用, 非0=使能 */
} exti_cfg_t;

/* ================================================================
 * Func_ExtiConfig() — 配置 EXTI 中断/事件线
 *   r0 = exti_cfg_t *cfg
 *
 * 使能路径 (cfg->enable != 0):
 *   1. 清除 pin_mask 在 IMR 和 EMR 的位
 *   2. 在 EXTI_BASE + reg_offset 设置 pin_mask 位
 *   3. 清除 pin_mask 在 RTSR 和 FTSR 的位
 *   4. 若 edge_mode == 0x10: 同时在 RTSR 和 FTSR 设置位 (双边沿)
 *      否则: 在 EXTI_BASE + edge_mode 设置位 (单边沿)
 *
 * 禁用路径 (cfg->enable == 0):
 *   在 EXTI_BASE + reg_offset 清除 pin_mask 位
 * ================================================================ */
void Func_ExtiConfig(exti_cfg_t *cfg)
{
    volatile uint32_t *exti = (volatile uint32_t *)EXTI_BASE;

    if (cfg->enable) {
        /* ---- 清除中断和事件屏蔽位 ---- */
        exti[0] &= ~cfg->pin_mask;               /* 0x0800BCF4-FC: EXTI_IMR */
        exti[1] &= ~cfg->pin_mask;               /* 0x0800BCFE-08: EXTI_EMR (offset +4) */

        /* ---- 设置中断屏蔽位 (主目标寄存器) ---- */
        exti[cfg->reg_offset / 4] |= cfg->pin_mask;  /* 0x0800BD0A-14: EXTI + reg_offset */

        /* ---- 清除边沿触发选择 ---- */
        exti[2] &= ~cfg->pin_mask;               /* 0x0800BD16-24: EXTI_RTSR (offset +8) */
        exti[3] &= ~cfg->pin_mask;               /* 0x0800BD26-32: EXTI_FTSR (offset +0xC) */

        /* ---- 边沿选择 ---- */
        if (cfg->edge_mode == 0x10) {
            /* 双边沿: 同时设置 RTSR 和 FTSR */
            exti[2] |= cfg->pin_mask;            /* 0x0800BD3A-46: EXTI_RTSR */
            exti[3] |= cfg->pin_mask;            /* 0x0800BD48-54: EXTI_FTSR */
        } else {
            /* 单边沿: edge_mode 指定目标寄存器偏移 */
            exti[cfg->edge_mode / 4] |= cfg->pin_mask;  /* 0x0800BD58-64 */
        }
    } else {
        /* ---- 禁用: 清除中断/事件屏蔽位 ---- */
        exti[cfg->reg_offset / 4] &= ~cfg->pin_mask;    /* 0x0800BD68-72 */
    }
}
