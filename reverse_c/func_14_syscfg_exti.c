/**
 * @file func_14_syscfg_exti.c
 * @brief 函数: SYSCFG_EXTIConfig — 配置 EXTI 中断线对应的 GPIO 端口
 * @addr  0x080119BC - 0x080119FC (64 bytes, 26 指令)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 写入 SYSCFG_EXTICR 寄存器, 将指定 EXTI 线映射到指定 GPIO 端口。
 * EXTI 控制器需要知道中断信号来自哪个 GPIO 端口 (PA/PB/PC...),
 * 此函数配置该映射关系。
 *
 * 参数:
 *   r0 = port (GPIO 端口编号: 0=PA, 1=PB, 2=PC, ...)
 *   r1 = line (EXTI 线编号: 0~15)
 *
 * SYSCFG_EXTICR 寄存器组:
 *   EXTICR1 @ 0x40013808 → EXTI0~3
 *   EXTICR2 @ 0x4001380C → EXTI4~7
 *   EXTICR3 @ 0x40013810 → EXTI8~11
 *   EXTICR4 @ 0x40013814 → EXTI12~15
 *
 * 每 4 bit 对应一根 EXTI 线: 值 0=PA, 1=PB, 2=PC, ...
 */

#include "stm32f4xx_hal.h"

/* SYSCFG 外部中断配置寄存器 */
#define SYSCFG_BASE     0x40013800
#define SYSCFG_EXTICR1  (*(volatile uint32_t *)(SYSCFG_BASE + 0x08))
#define SYSCFG_EXTICR2  (*(volatile uint32_t *)(SYSCFG_BASE + 0x0C))
#define SYSCFG_EXTICR3  (*(volatile uint32_t *)(SYSCFG_BASE + 0x10))
#define SYSCFG_EXTICR4  (*(volatile uint32_t *)(SYSCFG_BASE + 0x14))

/* ================================================================
 * SYSCFG_EXTIConfig() @ 0x080119BC
 *   r0 = port (GPIO 端口: 0=PA, 1=PB, 2=PC, ...)
 *   r1 = line (EXTI 线: 0~15)
 *
 * 操作:
 *   1. 计算寄存器索引 = line / 4 (0→EXTICR1, 1→EXTICR2, ...)
 *   2. 计算位偏移    = (line % 4) * 4 (0, 4, 8, 12)
 *   3. 清除目标位域 (写 0)
 *   4. 写入 port 值到该位域
 * ================================================================ */
void SYSCFG_EXTIConfig(uint32_t port, uint32_t line)
{
    uint32_t exti_idx;           /* r4 — 寄存器索引 (line / 4) */
    uint32_t bit_pos;            /* r4 — 位偏移 (line % 4 * 4) */
    uint32_t mask;               /* r2 — 清除掩码 (0xF << bit_pos) */
    uint32_t val;                /* r3 — 寄存器值 */

    /* 计算位偏移: (line & 3) * 4 */
    bit_pos  = (line << 30) >> 28;               /* 0x080119C0-C2: lsl #0x1E; lsr #0x1C */
    /* bit_pos = (line & 0x3) * 4 */

    /* 创建清除掩码: 0xF << bit_pos */
    mask = 0xF << bit_pos;                       /* 0x080119C4-C6: mov r3,#0xF; lsl.w r2,r3,r4 */

    /* 寄存器索引: line / 4 */
    exti_idx = line / 4;                         /* 0x080119CC: asrs r4, r1, #2 */

    /* ---- 读取-修改-写入: 清除位域 ---- */
    val = ((volatile uint32_t *)(SYSCFG_BASE + 0x08))[exti_idx];  /* 0x080119CA-CE: ldr r3,PC; ldr.w r3,[r3,r4,lsl#2] */
    val &= ~mask;                                /* 0x080119D2: bic r3, r2 */
    ((volatile uint32_t *)(SYSCFG_BASE + 0x08))[exti_idx] = val;  /* 0x080119D4-D8: ldr r4; str.w r3,[r4,r5,lsl#2] */

    /* ---- 再次读取-修改-写入: 设置 port 值 ---- */
    val = ((volatile uint32_t *)(SYSCFG_BASE + 0x08))[exti_idx];  /* 0x080119DC-E0: mov r3,r4; asrs r4; ldr.w r3,[r3,r4,lsl#2] */
    bit_pos  = (line << 30) >> 28;               /* 0x080119E4-E6: lsl #0x1E; lsr #0x1C → 重新计算 */
    val |= port << bit_pos;                      /* 0x080119E8-EC: lsl.w r4,r0,r4; orr r3,r4 */
    ((volatile uint32_t *)(SYSCFG_BASE + 0x08))[exti_idx] = val;  /* 0x080119EE-F2: ldr r4; str.w r3,[r4,r5,lsl#2] */
}
