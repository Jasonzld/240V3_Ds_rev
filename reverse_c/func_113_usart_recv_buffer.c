/**
 * @file func_113_usart_recv_buffer.c
 * @brief 函数: USART6 接收缓冲区管理器 — 多标志接收 + 环形缓冲写入
 * @addr  0x08015C38 - 0x08015D50 (280 bytes, 含字面量池)
 *         字面量池 0x08015D38-0x08015D4F (6 words, 24 bytes)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 三阶段 USART6 接收处理 (地址均从字面量池验证):
 *   阶段 1 (标志 0x525): 读取数据字节, 若门控标志为零则调用 func_18D90,
 *     否则写入环形缓冲区 [0x20003B4C + index], index 为 15-bit 值,
 *     存于 0x2000434C (buf_base + 0x800) 的半字 bit[14:0].
 *     若 phase-1 状态标志(0x200001BE)非零, 回显字节到 USART2_DR(0x40004404).
 *   阶段 2 (标志 8): 读取 USART6_SR 和 USART6_DR (清 RXNE), 清除标志.
 *   阶段 3 (标志 0x424): 索引字 bit15 翻转 + 0x8000, phase-3 标志(0x20000290) bit7 置位,
 *     再次读取 USART6_SR/DR.
 *
 * 字面量池 (Capstone 验证 @ 0x08015D38):
 *   [0] 0x40011400 = USART6 base
 *   [1] 0x200001E0 = buf_gate_flag (uint8)
 *   [2] 0x20003B4C = buf_base (+0x800 = index halfword @ 0x2000434C)
 *   [3] 0x200001BE = status_flag_phase1 (uint8)
 *   [4] 0x40004404 = USART2_DR (echo target)
 *   [5] 0x20000290 = status_flag_phase3 (uint8)
 *
 * 调用:
 *   func_15DB2() @ 0x08015DB2 — USART 组合标志检查
 *   func_15F24() @ 0x08015F24 — USART 读字节
 *   func_18D90() @ 0x08018D90 — 字节处理
 *   func_15D62() @ 0x08015D62 — USART 位清除
 *   func_15D98() @ 0x08015D98 — USART 标志检查
 *   func_15D50() @ 0x08015D50 — USART 写半字
 */

#include "stm32f4xx_hal.h"

extern uint32_t func_15DB2(volatile void *base, uint32_t flags);
extern uint8_t  func_15F24(volatile void *base);
extern void     func_18D90(uint8_t byte);
extern void     func_15D62(volatile void *base, uint32_t flags);
extern uint32_t func_15D98(volatile void *base, uint32_t flag);
extern void     func_15D50(volatile void *base, uint32_t val);

/* 字面量池槽位 (Capstone 验证) */
#define USART6_BASE         ((volatile void *)0x40011400)  /* pool[0] — USART6 */
#define BUF_GATE_FLAG       (*(volatile uint8_t  *)0x200001E0)  /* pool[1] */
#define BUF_BASE            ((volatile uint8_t  *)0x20003B4C)   /* pool[2] */
#define BUF_INDEX_HW        (*(volatile uint16_t *)0x2000434C)  /* BUF_BASE + 0x800 */
#define STATUS_FLAG1_REG    (*(volatile uint8_t  *)0x200001BE)  /* pool[3] — phase 1 */
#define USART2_DR_ECHO      (*(volatile uint16_t *)0x40004404)  /* pool[4] — USART2 DR (echo) */
#define STATUS_FLAG3_REG    (*(volatile uint8_t  *)0x20000290)  /* pool[5] — phase 3 */

void USART_Recv_Buffer_Manager(void)
{
    uint8_t r4, r5;

    /* ---- 阶段 1: 标志 0x525 — 数据接收 ---- */
    if (func_15DB2(USART6_BASE, 0x525)) {
        r4 = func_15F24(USART6_BASE);
        if (!BUF_GATE_FLAG) {
            func_18D90(r4);
        } else {
            uint16_t raw = BUF_INDEX_HW;
            uint16_t idx = raw & 0x7FFF;        /* UBFX #0,#0xf → 15-bit */
            if (idx < 0x7FF) {
                /* 缓冲区未满: 写入并递增 */
                BUF_BASE[idx] = r4;
                uint16_t new_raw = BUF_INDEX_HW + 1;
                uint16_t new_idx = new_raw & 0x7FFF;
                BUF_INDEX_HW = (BUF_INDEX_HW & 0x8000) | new_idx;
            } else {
                /* 缓冲区满: 回绕到索引 0 */
                BUF_INDEX_HW &= ~0x7FFF;         /* BFC #0,#0xf — 清除 bit[14:0] */
                BUF_BASE[0] = r4;
                BUF_INDEX_HW = (BUF_INDEX_HW & 0x8000) | 1;
            }
        }
        /* 若 phase-1 状态标志非零, 回显字节到 USART2 DR */
        if (STATUS_FLAG1_REG) {
            USART2_DR_ECHO = r4;
        }
        /* 清除 0x525 标志 */
        func_15D62(USART6_BASE, 0x525);
    }

    /* ---- 阶段 2: 标志 8 — 读取 USART6 状态 ---- */
    if (func_15D98(USART6_BASE, 8)) {
        r5 = (uint8_t)(*(volatile uint16_t *)USART6_BASE);           /* USART6_SR */
        r5 = (uint8_t)(*(volatile uint16_t *)((uint8_t *)USART6_BASE + 4)); /* USART6_DR */
        func_15D50(USART6_BASE, 8);
    }

    /* ---- 阶段 3: 标志 0x424 — 索引翻转 + 状态更新 ---- */
    if (func_15DB2(USART6_BASE, 0x424)) {
        /* 翻转 bit 15 并加 0x8000 */
        uint16_t hw = BUF_INDEX_HW;
        hw &= ~0x8000;
        hw += 0x8000;
        BUF_INDEX_HW = hw;
        /* phase-3 状态标志 bit 7 置位 */
        STATUS_FLAG3_REG |= 0x80;
        /* 读取 USART6 最终状态 (清 RXNE) */
        r5 = (uint8_t)(*(volatile uint16_t *)USART6_BASE);           /* USART6_SR */
        r5 = (uint8_t)(*(volatile uint16_t *)((uint8_t *)USART6_BASE + 4)); /* USART6_DR */
    }
}
