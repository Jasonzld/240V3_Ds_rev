/**
 * @file func_37_device_status_check.c
 * @brief 函数: Device_StatusCheck — 设备状态检查与模式判定
 * DISASSEMBLY-TRACED. Verified against Capstone.
 * @addr  0x0800D798 - 0x0800D834 (156 bytes, 含字面量池)
 *
 * 基于外设状态寄存器和门控标志判定设备当前状态,
 * 返回状态码供调度器分发处理。
 *
 * 参数:
 *   r0 = mode (非零时预置门控标志 FLAG_226 = 1)
 * 返回:
 *   0 = 空闲/无操作 (默认)
 *   1 = 状态 A ([0x42410214]==0)
 *   2 = 状态 B ([0x42410210]==0)
 *   4 = 状态 C ([0x4240021C]==0)
 *   5 = 状态 D (全部就绪, 无待处理)
 *
 * 全局变量:
 *   0x20000226 (FLAG_226, uint8_t)   — 门控标志
 *   0x20000227 (FLAG_227, uint8_t)   — 辅助标志
 *   0x42410214 (PERIPH_A, uint32_t)  — 外设状态寄存器 A
 *   0x42410210 (PERIPH_A4, uint32_t) — 外设状态寄存器 (A - 4)
 *   0x4240021C (PERIPH_B, uint32_t)  — 外设状态寄存器 B
 *
 * 调用:
 *   func_18488() @ 0x08018488 — 操作 (参数 0x0A)
 *
 * 字面量池: 0x0800D824-0x0800D830 (4 个条目)
 *   0x0800D824: 0x20000226 (FLAG_226)
 *   0x0800D828: 0x42410214 (PERIPH_A)
 *   0x0800D82C: 0x4240021C (PERIPH_B)
 *   0x0800D830: 0x20000227 (FLAG_227)
 */

#include "stm32f4xx_hal.h"

/* 外设状态寄存器 */
#define PERIPH_A   (*(volatile uint32_t *)0x42410214)
#define PERIPH_A4  (*(volatile uint32_t *)0x42410210)  /* = PERIPH_A - 4 */
#define PERIPH_B   (*(volatile uint32_t *)0x4240021C)

/* 标志 */
#define FLAG_226   (*(volatile uint8_t  *)0x20000226)
#define FLAG_227   (*(volatile uint8_t  *)0x20000227)

extern void func_18488(uint32_t param);   /* 0x08018488 */

/* ================================================================
 * Device_StatusCheck() @ 0x0800D798
 *   push {r4, lr} — r4 保存参数 r0
 *
 * 注: push 仅保存 r4/r4, 子函数调用通过 lr 返回。
 * ================================================================ */
uint32_t Device_StatusCheck(uint32_t mode)
{
    /* 若 mode != 0, 预置门控标志 */
    if (mode != 0) {                                 /* 0x0800D79C: cbz r4, #skip */
        FLAG_226 = 1;                                /* 0x0800D79E-A2: movs r0,#1; strb [FLAG_226] */
    }

    /* ---- 路径 1: 门控标志已置位 (FLAG_226 != 0) ---- */
    if (FLAG_226 != 0) {                             /* 0x0800D7A4-A8: ldrb r0,[FLAG_226]; cbz r0,#path2 */

        /* 检查三个外设状态寄存器: 任一为零则进入处理器,
         * 全非零时 PERIPH_B 零值检查引导到 path2 */
        if (PERIPH_A == 0) {                         /* 0x0800D7AA-AE: ldr r0,[PERIPH_A]; cbz r0,#handler */
            goto handler;                            /* A==0 → 进入处理器 */
        }
        if (PERIPH_A4 == 0) {                        /* 0x0800D7B0-B6: subs r0,#4; ldr r0,[r0]; cbz r0,#handler */
            goto handler;                            /* A4==0 → 进入处理器 */
        }
        if (PERIPH_B == 0) {                         /* 0x0800D7B8-BC: ldr r0,[PERIPH_B]; cbz r0,#path2 */
            goto path2;                              /* A!=0,A4!=0,B==0 → path2 */
        }
        /* A!=0, A4!=0, B!=0 → 落入 handler (cbz 未跳转) */

    } else {
        /* ---- 路径 2: 门控标志未置位 (FLAG_226 == 0) ---- */
        /* 或 subA: 路径1 中全零检查通过后落入 */
path2:
        /* 检查特定条件: PERIPH_A==1 && PERIPH_A4==1 && PERIPH_B!=0 */
        if (PERIPH_A == 1) {                         /* 0x0800D7FA-FE: cmp r0,#1; bne #ret0 */
            goto check_a4;
        }
        goto ret0;

check_a4:
        if (PERIPH_A4 == 1) {                        /* 0x0800D802-0A: subs r0,#4; cmp r0,#1; bne #ret0 */
            goto check_b;
        }
        goto ret0;

check_b:
        if (PERIPH_B != 0) {                         /* 0x0800D80C-10: ldr r0,[PERIPH_B]; cbz r0,#ret0 */
            /* 三元条件满足 → 设置标志并返回 0 */
            FLAG_226 = 1;                            /* 0x0800D812-16: movs r0,#1; strb [FLAG_226] */
            FLAG_227 = 0;                            /* 0x0800D818-1C: movs r0,#0; strb [FLAG_227] */
            goto ret0;
        }
        goto ret0;
    }

    /* ---- 处理器: 三路外设至少一路非零 (仅当门控置位) ---- */
handler:
    func_18488(0x0A);                                /* 0x0800D7BE-C0: movs r0,#0xa; bl */

    FLAG_226 = 0;                                    /* 0x0800D7C4-C8: movs r0,#0; strb [FLAG_226] */

    /* 级联优先级判定: PERIPH_A 和 PERIPH_B 任一非零 */
    if (PERIPH_A != 0) {                             /* 0x0800D7CA-CE: ldr r0,[PERIPH_A]; cbnz r0,#check_a */
        goto check_a;
    }
    if (PERIPH_B != 0) {                             /* 0x0800D7D0-D4: ldr r0,[PERIPH_B]; cbnz r0,#check_a */
        goto check_a;
    }
    /* 两路均为零 */
    return 5;                                        /* 0x0800D7D6-D8: movs r0,#5; pop {r4,pc} */

check_a:
    if (PERIPH_A == 0) {                             /* 0x0800D7DA-DE: ldr r0,[PERIPH_A]; cbnz r0,#check_a4b */
        return 1;                                    /* 0x0800D7E0-E2: movs r0,#1; b #ret */
    }

check_a4b:
    if (PERIPH_A4 == 0) {                            /* 0x0800D7E4-EA: ldr r0,[PERIPH_A-4]; cbnz r0,#check_b2 */
        return 2;                                    /* 0x0800D7EC-EE: movs r0,#2; b #ret */
    }

check_b2:
    if (PERIPH_B == 0) {                             /* 0x0800D7F0-F4: ldr r0,[PERIPH_B]; cbnz r0,#ret0 */
        return 4;                                    /* 0x0800D7F6-F8: movs r0,#4; b #ret */
    }
    /* PERIPH_B != 0 → 落入 ret0 */

ret0:
    return 0;                                        /* 0x0800D81E-20: movs r0,#0; b #ret */
}
