/**
 * @file func_20_tft_status_monitor.c
 * @brief 函数: TFT_StatusMonitor — TFT FSMC 状态监控/看门狗
 * @addr  0x0800C654 - 0x0800C6C4 (112 bytes, 含字面量池)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 监控 FSMC TFT 控制寄存器 (0x42408214) 的状态, 维护两个独立计数器
 * 用于跟踪 FSMC 活跃/非活跃状态的持续时间, 并根据阈值设置状态标志。
 *
 * RAM 变量:
 *   0x20000210 = cnt_active   (FSMC 活跃计数器, 递增)
 *   0x20000211 = cnt_inactive (FSMC 非活跃计数器, 递增)
 *   0x200000D9 = tft_status   (TFT 状态标志: 0/1/2)
 *   0x20000032 = flag_32      (外部控制标志, 影响状态分支)
 *
 * FSMC 寄存器:
 *   0x42408214 = TFT FSMC 状态寄存器 (非零=活跃, 零=非活跃)
 *
 * 逻辑:
 *   FSMC 活跃 (REG != 0):
 *     - 清零 cnt_inactive
 *     - 若 cnt_active > 100: tft_status = 2
 *     - 否则: cnt_active++
 *   FSMC 非活跃 (REG == 0):
 *     - 清零 cnt_active
 *     - 若 cnt_inactive > 100:
 *       - flag_32 == 0 → tft_status = 1
 *       - flag_32 != 0 → tft_status = 0
 *     - 否则: cnt_inactive++
 *   返回: 始终返回 0
 *
 * 注: 该函数无堆栈帧 (不保存 callee-save 寄存器),
 *     为叶函数 (leaf function), 仅使用 r0-r3 调用者保存寄存器。
 */

#include "stm32f4xx_hal.h"

/* FSMC TFT 状态寄存器 */
#define TFT_FSMC_STATUS_REG  (*(volatile uint32_t *)0x42408214)

/* RAM 状态变量 */
#define CNT_ACTIVE    (*(volatile uint8_t *)0x20000210)
#define CNT_INACTIVE  (*(volatile uint8_t *)0x20000211)
#define TFT_STATUS    (*(volatile uint8_t *)0x200000D9)
#define FLAG_32       (*(volatile uint8_t *)0x20000032)

/* 阈值常量 */
#define COUNTER_THRESHOLD  100

/* 状态值 */
#define STATUS_CLEAR  0
#define STATUS_SET    1
#define STATUS_ALERT  2

/* ================================================================
 * TFT_StatusMonitor() @ 0x0800C654
 *   监控 FSMC TFT 状态, 维护超时计数器
 *   返回: 0 (r0 在入口处清零, 始终未修改)
 *
 * 注: 汇编中 r0 在 0x0800C654 初始化为 0,
 *     后续未修改, bx lr 时 r0 仍为 0。
 *     调用者可能忽略返回值。
 * ================================================================ */
uint32_t TFT_StatusMonitor(void)
{
    /* 0x0800C654: movs r0, #0 — 默认返回值 */

    if (TFT_FSMC_STATUS_REG != 0) {              /* 0x0800C656-58: ldr r1,=REG; ldr r1,[r1] */
                                                 /* 0x0800C65A: cbz r1, #else */

        /* ---- FSMC 活跃分支 ---- */
        CNT_INACTIVE = 0;                        /* 0x0800C65C-60: movs r1,#0; ldr r2,=CNT_INACTIVE; strb */

        if (CNT_ACTIVE > COUNTER_THRESHOLD) {    /* 0x0800C662-64: ldr r1,=CNT_ACTIVE; ldrb; cmp #0x64 */
                                                 /* 0x0800C668: ble #inc_active */
            TFT_STATUS = STATUS_ALERT;           /* 0x0800C66A-6E: movs r1,#2; ldr r2,=TFT_STATUS; strb */
        } else {
            CNT_ACTIVE++;                        /* 0x0800C672-7A: ldr r1,=CNT_ACTIVE; ldrb; adds #1; strb */
        }
    } else {
        /* ---- FSMC 非活跃分支 ---- */
        CNT_ACTIVE = 0;                          /* 0x0800C67E-82: movs r1,#0; ldr r2,=CNT_ACTIVE; strb */

        if (CNT_INACTIVE > COUNTER_THRESHOLD) {  /* 0x0800C684-86: ldr r1,=CNT_INACTIVE; ldrb; cmp #0x64 */
                                                 /* 0x0800C68A: ble #inc_inactive */

            if (FLAG_32 == 0) {                  /* 0x0800C68C-8E: ldr r1,=FLAG_32; ldrb */
                                                 /* 0x0800C690: cbz r1, #set_status_1 */
                TFT_STATUS = STATUS_SET;         /* 0x0800C69A-9E: movs r1,#1; ldr r2,=TFT_STATUS; strb */
            } else {
                TFT_STATUS = STATUS_CLEAR;       /* 0x0800C692-96: movs r1,#0; ldr r2,=TFT_STATUS; strb */
            }
        } else {
            CNT_INACTIVE++;                      /* 0x0800C6A2-AA: ldr r1,=CNT_INACTIVE; ldrb; adds #1; strb */
        }
    }

    return 0;                                    /* 0x0800C6AC: bx lr (r0=0, 入口时已设置) */
}
