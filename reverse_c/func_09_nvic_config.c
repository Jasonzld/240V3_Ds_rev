/**
 * @file func_09_nvic_config.c
 * @brief 函数: NVIC_Config — NVIC 中断优先级与使能/禁用配置
 * DISASSEMBLY-TRACED. Verified against Capstone (duplicate of func_42, same addr 0x0800D9B4-0x0800DA2C).
 * @addr  0x0800D9B4 - 0x0800DA2C (120 bytes, 含字面量池)
 *        NOTE: 0x0800DA1E-0x0800DA2B is literal pool; 0x0800DA2C+ is next function
 *        Overlaps with func_42_nvic_exti_config.c (0x0800D9B4-0x0800DA1C)
 *
 * 配置 STM32F410 NVIC 中断控制器:
 *   - 根据 AIRCR PRIGROUP 计算优先级字节布局
 *   - 写入 NVIC_IPR (中断优先级)
 *   - 使能 (NVIC_ISER) 或禁用 (NVIC_ICER) 中断线
 *
 * 配置结构体 nvic_cfg_t (4字节):
 *   [0] uint8_t irq_num         — IRQ 编号 (0~81)
 *   [1] uint8_t preempt_priority — 抢占优先级 (4-bit)
 *   [2] uint8_t sub_priority     — 子优先级 (4-bit)
 *   [3] uint8_t enable           — 0=禁用, 非0=使能+写优先级
 *
 * 注意: 固件中此函数被命名为 UartPinConfig，但实际功能为 NVIC 配置。
 */

#include "stm32f4xx_hal.h"

/* NVIC 寄存器 */
#define NVIC_ISER_BASE  0xE000E100   /* Interrupt Set-Enable Register */
#define NVIC_ICER_BASE  0xE000E180   /* Interrupt Clear-Enable Register */
#define NVIC_IPR_BASE   0xE000E400   /* Interrupt Priority Register */
#define SCB_AIRCR       (*(volatile uint32_t *)0xE000ED0C)  /* Application Interrupt and Reset Control */

/* NVIC 配置结构体 (4字节) */
typedef struct {
    uint8_t irq_num;           /* [0] IRQ 编号 */
    uint8_t preempt_priority;  /* [1] 抢占优先级 (4-bit, 0~15) */
    uint8_t sub_priority;      /* [2] 子优先级 (4-bit, 0~15) */
    uint8_t enable;            /* [3] 0=禁用中断, 非0=使能+配置优先级 */
} nvic_cfg_t;

/* ================================================================
 * NVIC_Config() — NVIC 中断优先级与使能/禁用配置
 *   r0 = nvic_cfg_t *cfg
 *
 * 使能路径 (cfg->enable != 0):
 *   1. 读取 AIRCR PRIGROUP [10:8] → 确定优先级分组
 *   2. 计算 preempt_priority 和 sub_priority 在 NVIC 优先级
 *      字节中的位置 (由 PRIGROUP 决定)
 *   3. 拼接并写入 NVIC_IPR[irq_num]
 *   4. 写 NVIC_ISER 使能该 IRQ
 *
 * 禁用路径 (cfg->enable == 0):
 *   写 NVIC_ICER 禁用该 IRQ (不涉及优先级)
 * ================================================================ */
void NVIC_Config(nvic_cfg_t *cfg)
{
    uint32_t prigroup;           /* r4 */
    uint32_t tmp;                /* r4/r5 */
    uint32_t pri_shift;          /* r3 — preempt priority 左移量 */
    uint32_t sub_mask;           /* r2 — sub priority 掩码/符号扩展 */
    uint32_t pri_byte;           /* r1 — 最终优先级字节 */

    if (cfg->enable) {
        /* ---- 读取 PRIGROUP (AIRCR bit[10:8]) ---- */
        prigroup = SCB_AIRCR;                    /* 0x0800D9C0-C2: ldr PC-relative, ldr */
        prigroup = prigroup & 0x700;             /* 0x0800D9C4: and #0x700 */
        prigroup = 0x700 - prigroup;             /* 0x0800D9C8: rsb.w #0x700 */

        tmp = (prigroup >> 8) & 0xFF;            /* 0x0800D9CC: ubfx r1, r4, #8, #8 */
        /* r1 = 7 - PRIGROUP */

        pri_shift = (uint8_t)(4 - tmp);          /* 0x0800D9D0-D4: rsb.w r4,r1,#4; uxtb r3,r4 */

        /* sub_mask = ASRS (7-PRIGROUP) >> 1 (算术右移1位) */
        sub_mask = (int32_t)tmp >> 1;           /* 0x0800D9D6: asrs r2, r1 */

        /* ---- 拼接优先级: preempt << pri_shift | sub & sub_mask ---- */
        pri_byte  = cfg->preempt_priority << pri_shift; /* 0x0800D9D8-DC: ldrb [r0,#1]; lsl r4,r3; uxtb r1,r4 */
        pri_byte &= 0xFF;                                 /* uxtb */

        tmp = cfg->sub_priority & sub_mask;       /* 0x0800D9DE-E0: ldrb [r0,#2]; and r4,r2 */
        pri_byte |= tmp;                          /* 0x0800D9E2: orr r1,r4 */

        /* 提取低 4-bit 并移到 NVIC 优先级字节高位 (NVIC 使用 MSB-aligned) */
        pri_byte = (pri_byte << 28) >> 24;        /* 0x0800D9E4-E6: lsl #0x1C; lsr #0x18 */
        /* 等效: pri_byte = (pri_byte & 0x0F) << 4 */

        /* ---- 写 NVIC 优先级 ---- */
        ((volatile uint8_t *)NVIC_IPR_BASE)[cfg->irq_num] = pri_byte;  /* 0x0800D9E8-EC: ldr r4,PC-relative; ldrb r5,[r0]; strb r1,[r4,r5] */

        /* ---- 使能 NVIC 中断 (ISER) ---- */
        /* 地址计算: NVIC_ISER[irq/32] |= 1 << (irq%32) */
        tmp  = cfg->irq_num & 0x1F;               /* 0x0800D9F0: and r5,r4,#0x1f */
        tmp  = 1 << tmp;                          /* 0x0800D9F4-F6: mov r4,#1; lsl r4,r5 */

        pri_byte = cfg->irq_num >> 5;             /* 0x0800D9F8-FA: ldrb r5,[r0]; asr r5,#5 */
        /* 计算 NVIC_ISER 字地址: 0xE000E000 + (irq/32)*4 + 0x100 */
        pri_byte = pri_byte * 4;                  /* 0x0800D9FC: lsl r5,#2 */
        *((volatile uint32_t *)(0xE000E000 + pri_byte + 0x100)) = tmp;  /* 0x0800D9FE-02: add.w r5,#-0x1fff2000; str [r5,#0x100] */
    } else {
        /* ---- 禁用 NVIC 中断 (ICER) ---- */
        /* 地址计算: NVIC_ICER[irq/32] |= 1 << (irq%32) */
        tmp  = cfg->irq_num & 0x1F;               /* 0x0800DA08-0C: ldrb [r0]; and r5,r4,#0x1f */
        tmp  = 1 << tmp;                          /* 0x0800DA0E-10: mov r4,#1; lsl r4,r5 */

        pri_byte = cfg->irq_num >> 5;             /* 0x0800DA14-16: ldrb r6,[r0]; asr r6,#5 */
        /* NVIC_ICER 位于 0xE000E180 + (irq/32)*4 */
        ((volatile uint32_t *)(NVIC_ICER_BASE))[pri_byte] = tmp;  /* 0x0800DA12-18: ldr r5,PC-relative; str [r5,r6,lsl #2] */
    }
}
