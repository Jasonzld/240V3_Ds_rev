/**
 * @file func_29_speed_compute.c
 * @brief 函数: Compute_SpeedVal — 速度/速率计算 (叶函数)
 * @addr  0x0800D51C - 0x0800D558 (60 bytes, 含字面量池)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 基于 RAM 变量的除法/缩放计算, 用于速度或频率显示值。
 *
 * 公式:
 *   val = SPEED_BASE (uint16_t)
 *   if val == 0: return 0
 *   q = 10000 / val
 *   q = q * 10
 *   r = q / SPEED_DIV
 *   result = r + SPEED_ADD
 *   return (uint16_t)result
 *
 * 参数: 无
 * 返回: r0 = 速度显示值 (uint16_t)
 *
 * 注: 叶函数, 无堆栈帧, bx lr 返回。
 *     限于 r0-r3 寄存器使用。
 *
 * RAM 变量:
 *   0x20000208 = SPEED_BASE  (uint16_t, 基准速度/频率)
 *   0x200000FA = SPEED_DIV   (uint16_t, 除数)
 *   0x200000F8 = SPEED_ADD   (uint16_t, 加数偏移)
 *
 * 字面量池:
 *   0x0800D54C: 0x20000208
 *   0x0800D550: 0x200000FA
 *   0x0800D554: 0x200000F8
 */

#include "stm32f4xx_hal.h"

#define SPEED_BASE  (*(volatile uint16_t *)0x20000208)
#define SPEED_DIV   (*(volatile uint16_t *)0x200000FA)
#define SPEED_ADD   (*(volatile uint16_t *)0x200000F8)

/* ================================================================
 * Compute_SpeedVal() @ 0x0800D51C
 *   速度值计算: 10000/基准*10 / 除数 + 偏移
 *
 * 指令跟踪:
 *   0x0800D51C: ldr r2,=SPEED_BASE     — r2 = &SPEED_BASE
 *   0x0800D51E: ldrh r2,[r2]           — val = SPEED_BASE
 *   0x0800D520: cbz r2,#ret0           — if (val==0) return 0
 *   0x0800D522: ldr r2,=SPEED_BASE     — r2 = &SPEED_BASE (再次)
 *   0x0800D524: ldrh r2,[r2]           — val = SPEED_BASE (再次, 刷新)
 *   0x0800D526: movw r3,#0x2710        — r3 = 10000
 *   0x0800D52A: sdiv r2,r3,r2          — q = 10000 / val
 *   0x0800D52E: uxth r1,r2             — r1 = (uint16_t)q
 *   0x0800D530: add.w r2,r1,r1,lsl#2   — r2 = q * 5
 *   0x0800D534: lsls r2,r2,#1          — r2 = q * 10
 *   0x0800D536: ldr r3,=SPEED_DIV      — r3 = &SPEED_DIV
 *   0x0800D538: ldrh r3,[r3]           — div = SPEED_DIV
 *   0x0800D53A: sdiv r2,r2,r3          — r = (q*10) / div
 *   0x0800D53E: ldr r3,=SPEED_ADD      — r3 = &SPEED_ADD
 *   0x0800D540: ldrh r3,[r3]           — add = SPEED_ADD
 *   0x0800D542: add r2,r3              — result = r + add
 *   0x0800D544: uxth r0,r2             — r0 = (uint16_t)result
 *   0x0800D546: b #ret                 — (合并跳转)
 *   ret0:
 *   0x0800D548: movs r0,#0             — return 0
 *   ret:
 *   0x0800D54A: bx lr
 *
 * 注: SPEED_BASE 被连续两次读取 (0x0800D51E 和 0x0800D524),
 *     第一次用于零值检查 (cbz), 第二次用于除法运算。
 *     两次读取间该值可能被 ISR 修改 (volatile 语义)。
 * ================================================================ */
uint16_t Compute_SpeedVal(void)
{
    uint32_t val;     /* r2 */
    uint32_t q;       /* r1/r2 */
    uint32_t r;       /* r2 */
    uint32_t add;     /* r3 */

    /* 读取基准值, 若为 0 则返回 0 */
    val = SPEED_BASE;                            /* 0x0800D51C-1E: ldr r2,=addr; ldrh r2,[r2] */
    if (val == 0) {                              /* 0x0800D520: cbz r2, #ret0 */
        return 0;                                /* 0x0800D548: movs r0, #0 */
    }

    /* 重新读取 (volatile, 可能被 ISR 更新) */
    val = SPEED_BASE;                            /* 0x0800D522-24: ldr r2,=addr; ldrh r2,[r2] */

    /* q = 10000 / val */
    q = 10000 / val;                             /* 0x0800D526-2E: movw r3,#0x2710; sdiv r2,r3,r2; uxth r1,r2 */

    /* q = q * 10 (优化: q*5*2) */
    q = q * 10;                                  /* 0x0800D530-34: add.w r2,r1,r1,lsl#2; lsls r2,r2,#1 */

    /* r = q / SPEED_DIV */
    add = SPEED_DIV;                             /* 0x0800D536-38: ldr r3,=addr; ldrh r3,[r3] */
    r = q / add;                                 /* 0x0800D53A: sdiv r2,r2,r3 */

    /* result = r + SPEED_ADD */
    add = SPEED_ADD;                             /* 0x0800D53E-40: ldr r3,=addr; ldrh r3,[r3] */
    r = r + add;                                 /* 0x0800D542: add r2,r3 */

    return (uint16_t)r;                          /* 0x0800D544-4A: uxth r0,r2; b #ret */
}
