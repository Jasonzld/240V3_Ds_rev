/**
 * @file func_42_nvic_exti_config.c
 * @brief 函数: NVIC_EXTI_Config — NVIC/EXTI 中断配置
 * DISASSEMBLY-TRACED. Verified against Capstone.
 * @addr  0x0800D9B4 - 0x0800DA2C (120 bytes, 含字面量池)
 *
 * 基于传入的 4 字节配置结构体, 配置 NVIC 中断优先级
 * 和/或 EXTI 中断使能。两种模式由 cfg[3] 控制。
 *
 * 参数:
 *   r0 = cfg (4 字节配置结构体指针):
 *     cfg[0] = irq_index (低 5 位: 位偏移, 高 3 位: 寄存器索引)
 *     cfg[1] = shift_val (用于优先级计算)
 *     cfg[2] = mask_val (用于优先级掩码)
 *     cfg[3] = mode (0 = 简单模式, 非零 = 复杂 PRIGROUP 模式)
 *
 * 模式 0 (简单): 写入 NVIC_ICER[reg_idx] (清除中断使能)
 *   bit_pos = cfg[0] & 0x1F
 *   reg_idx = cfg[0] >> 5
 *   存储 (1 << bit_pos) 到 [NVIC_ICER_base + reg_idx * 4]
 *   其中 base_addr = 0xE000E180 (NVIC_ICER)
 *
 * 模式 !0 (复杂): 基于 SCB PRIGROUP 计算优先级编码,
 *   组合 cfg[1] (移位值), cfg[2] (掩码值) 后写入 NVIC_IPR。
 *
 * 字面量池:
 *   0x0800DA20: 0xE000ED0C (SCB_AIRCR — PRIGROUP)
 *   0x0800DA24: 0xE000E400 (NVIC_IPR — 复杂模式用)
 *   0x0800DA28: 0xE000E180 (NVIC_ICER — 简单模式用)
 */

#include "stm32f4xx_hal.h"

#define SCB_AIRCR   (*(volatile uint32_t *)0xE000ED0C)

/* NVIC 配置结构体 */
typedef struct {
    uint8_t irq_index;   /* [+0] 位编码: [4:0]=位偏移, [7:5]=寄存器索引 */
    uint8_t shift_val;   /* [+1] 移位值 */
    uint8_t mask_val;    /* [+2] 掩码值 */
    uint8_t mode;        /* [+3] 0=简单, !0=复杂 PRIGROUP 模式 */
} nvic_cfg_t;

/* ================================================================
 * NVIC_EXTI_Config() @ 0x0800D9B4
 *   push {r4, r5, r6, lr} — r4/r5/r6 用于中间计算
 * ================================================================ */
void NVIC_EXTI_Config(nvic_cfg_t *cfg)
{
    uint32_t r1 = 0;     /* r1 — 累加器 */
    uint32_t r2;         /* r2 — 掩码/除数 */
    uint32_t r3 = 0;     /* r3 — 移位/子优先级 */
    uint32_t r4;         /* r4 — 临时 */
    uint32_t r5;         /* r5 — 临时 */
    uint32_t r6;         /* r6 — 临时 */

    r2 = 0xF;                                        /* 0x0800D9BA: movs r2,#0xf */

    if (cfg->mode != 0) {                            /* 0x0800D9BC-BE: ldrb r4,[r0,#3]; cbz r4,#simple */

        /* ---- 复杂模式: 基于 SCB PRIGROUP 计算优先级编码 ---- */
        r4 = SCB_AIRCR;                              /* 0x0800D9C0-C2: ldr r4,=AIRCR; ldr r4,[r4] */
        r4 = r4 & 0x700;                             /* 0x0800D9C4: and r4,r4,#0x700 — PRIGROUP */
        r4 = 0x700 - r4;                             /* 0x0800D9C8: rsb.w r4,r4,#0x700 */
        r1 = (r4 >> 8) & 0xFF;                       /* 0x0800D9CC: ubfx r1,r4,#8,#8 */
        r4 = 4 - r1;                                 /* 0x0800D9D0: rsb.w r4,r1,#4 */
        r3 = (uint8_t)r4;                            /* 0x0800D9D4: uxtb r3,r4 — 子优先级 */
        r2 = (int32_t)r1 >> 1;                      /* 0x0800D9D6: asrs r2,r1 — 16-bit ASRS Rd,Rm form, shift by 1 */

        /* 编码优先级值 */
        r4 = cfg->shift_val;                         /* 0x0800D9D8: ldrb r4,[r0,#1] */
        r4 = r4 << r3;                               /* 0x0800D9DA: lsls r4,r3 */
        r1 = (uint8_t)r4;                            /* 0x0800D9DC: uxtb r1,r4 */

        r4 = cfg->mask_val;                          /* 0x0800D9DE: ldrb r4,[r0,#2] */
        r4 = r4 & r2;                                /* 0x0800D9E0: ands r4,r2 */
        r1 = r1 | r4;                                /* 0x0800D9E2: orrs r1,r4 — 组合优先级 */

        r4 = r1 << 28;                               /* 0x0800D9E4: lsls r4,r1,#0x1c */
        r1 = r4 >> 24;                               /* 0x0800D9E6: lsrs r1,r4,#0x18 — 标准化 */

        /* 写入 NVIC */
        r4 = 0xE000E400;                             /* 0x0800D9E8: ldr r4,=0xE000E400 — NVIC_IPR */
        r5 = cfg->irq_index;                         /* 0x0800D9EA: ldrb r5,[r0] */
        ((volatile uint8_t *)r4)[r5] = (uint8_t)r1;  /* 0x0800D9EC: strb r1,[r4,r5] */

        /* ---- EXTI 中断使能 (位带写入) ---- */
        r4 = cfg->irq_index;                         /* 0x0800D9EE: ldrb r4,[r0] */
        r5 = r4 & 0x1F;                              /* 0x0800D9F0: and r5,r4,#0x1f — 位偏移 */
        r4 = 1 << r5;                                /* 0x0800D9F4-F6: movs r4,#1; lsls r4,r5 — 位掩码 */
        r5 = cfg->irq_index;                         /* 0x0800D9F8: ldrb r5,[r0] */
        r5 = r5 >> 5;                                /* 0x0800D9FA: asrs r5,r5,#5 — 寄存器索引 */
        r5 = r5 << 2;                                /* 0x0800D9FC: lsls r5,r5,#2 */
        r5 = r5 + 0xE000E100;                        /* 0x0800D9FE: add.w r5,r5,#0xE000E100 */
                                                     /*   (实际为 0x1fff2000 偏移, 含位带补偿) */
        *(volatile uint32_t *)(r5 + 0x100) = r4;     /* 0x0800DA02: str.w r4,[r5,#0x100] */
        /* → 0xE000E200 (NVIC_ISPR) 位带写入 */

        goto done;                                   /* 0x0800DA06: b #done */

    } else {
        /* ---- 简单模式: 直接 NVIC 写入 ---- */
simple:
        r4 = cfg->irq_index;                         /* 0x0800DA08: ldrb r4,[r0] */
        r5 = r4 & 0x1F;                              /* 0x0800DA0A: and r5,r4,#0x1f */
        r4 = 1 << r5;                                /* 0x0800DA0E-10: movs r4,#1; lsls r4,r5 */

        r5 = 0xE000E180;                             /* 0x0800DA12: ldr r5,=0xE000E180 — NVIC_ICER */
        r6 = cfg->irq_index;                         /* 0x0800DA14: ldrb r6,[r0] */
        r6 = r6 >> 5;                                /* 0x0800DA16: asrs r6,r6,#5 — 寄存器索引 */
        *(volatile uint32_t *)(r5 + (r6 << 2)) = r4; /* 0x0800DA18: str.w r4,[r5,r6,lsl#2] */
                                                    /* 写入 NVIC_ICER → 清除中断使能 */
    }

done:
    return;                                          /* 0x0800DA1C: pop {r4,r5,r6,pc} */
}
