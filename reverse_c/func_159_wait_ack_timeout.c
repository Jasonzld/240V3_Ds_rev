/**
 * @file func_159_wait_ack_timeout.c
 * @brief 双循环 ACK 等待超时 — 逐字节轮询外设状态 (mask 0x80 + 0x40)
 * @addr  0x08015BE4 - 0x08015C38 (84 bytes, 1 个函数)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 逐字节轮询外设状态寄存器, 对每个字节位置先以 mask=0x80 等待,
 * 收到响应后调用 func_15F2E 处理, 再以 mask=0x40 等待二次响应.
 * 每个等待阶段最多迭代 50,000 次 (0xC350).
 *
 * 参数: r0 = stat_reg — 外设状态寄存器基址
 *       r1 = byte_count — 待处理字节数量
 *
 * 调用:
 *   func_15D98 (状态寄存器轮询)  ×2 (每字节两阶段)
 *   func_15F2E (字节响应处理)    ×1 (每字节)
 *
 * 寄存器:
 *   r4 = 字节索引 (0..byte_count-1)
 *   r5 = 外设基址 (r0 保存)
 *   r6 = 字节数量 (r1 保存)
 *   r7 = 50000 倒计时器 (每阶段重置)
 */

#include "stm32f4xx_hal.h"

/* ---- 外部依赖 ---- */
extern uint32_t func_15D98(uint32_t reg_base, uint32_t mask);
extern void     func_15F2E(uint32_t reg_base, uint8_t byte_val);

/* ---- 字面量池 (0x08015C34-0x08015C38) ---- */
/* 所有 3 个 LDR [pc,#imm] 均引用同一池地址 0x08015C34 = 0x40004400 (USART6) */
#define PERIPH_BASE    0x40004400    /* USART6 基址 (池验证: 0x40004400) */

/* ========================================================================
 * Wait_ACK_Timeout (0x08015BE4 - 0x08015C30, 76 bytes + 8B pool)
 *
 * 外循环: for (r4 = 0; r4 < byte_count; r4++)
 *   阶段 A: 以 mask=0x80 轮询, 最多 50000 次
 *   阶段 B: 加载 [stat_reg + r4] 字节, 调用 func_15F2E 处理
 *   阶段 C: 以 mask=0x40 轮询, 最多 50000 次
 *
 * 指令序列:
 *   0x08015BE4: push.w {r4,r5,r6,r7,r8,lr}
 *   0x08015BE8: mov r5,r0
 *   0x08015BEA: mov r6,r1
 *   0x08015BEC: movs r4,#0
 *   0x08015BEE: b outer_check
 *
 *   阶段 A (mask=0x80 轮询):
 *   0x08015BF0: movw r7,#0xc350
 *   0x08015BF6: movs r1,#0x80
 *   0x08015BF8: ldr r0,[pc,#0x38] → PERIPH_BASE_A
 *   0x08015BFA: bl func_15D98
 *   0x08015BFE: cbnz r0, phase_b  (收到响应 → 阶段 B)
 *   0x08015C00: subs r0,r7,#0    (设置标志)
 *   0x08015C02: sub.w r7,r7,#1
 *   0x08015C06: bne poll_a_loop
 *
 *   阶段 B (处理字节):
 *   0x08015C08: ldrb r1,[r5,r4]  → 读取当前字节
 *   0x08015C0A: ldr r0,[pc,#0x28] → PERIPH_BASE (另一外设)
 *   0x08015C0C: bl func_15F2E
 *
 *   阶段 C (mask=0x40 轮询):
 *   0x08015C10: movw r7,#0xc350
 *   0x08015C16: movs r1,#0x40
 *   0x08015C18: ldr r0,[pc,#0x18] → PERIPH_BASE_A (同上阶段 A)
 *   0x08015C1A: bl func_15D98
 *   0x08015C1E: cbnz r0, next_byte
 *   0x08015C20: subs r0,r7,#0
 *   0x08015C22: sub.w r7,r7,#1
 *   0x08015C26: bne poll_c_loop
 *
 *   next_byte:
 *   0x08015C28: adds r0,r4,#1
 *   0x08015C2A: uxtb r4,r0
 *   outer_check:
 *   0x08015C2C: cmp r4,r6
 *   0x08015C2E: blt phase_a
 *
 *   0x08015C30: pop.w {r4,r5,r6,r7,r8,pc}
 *
 * 返回: 无显式返回值 (r0 保留最后 func_15D98 的返回值).
 *       超时时静默进入下一字节, 无特殊超时标志.
 * ======================================================================== */
void Wait_ACK_Timeout(volatile uint32_t *stat_reg, uint8_t byte_count)
{
    uint32_t i;
    uint8_t idx;

    for (idx = 0; idx < byte_count; idx++) {
        /* 阶段 A: 等待 mask=0x80 响应 (例如 TXE — 发送缓冲区空) */
        for (i = 50000; i > 0; i--) {
            if (func_15D98(PERIPH_BASE, 0x80))
                break;
        }

        /* 阶段 B: 读取当前字节并处理 */
        uint8_t byte_val = ((volatile uint8_t *)stat_reg)[idx];
        func_15F2E(PERIPH_BASE, byte_val);

        /* 阶段 C: 等待 mask=0x40 响应 (例如 TC — 发送完成) */
        for (i = 50000; i > 0; i--) {
            if (func_15D98(PERIPH_BASE, 0x40))
                break;
        }
    }
}

/*
 * 内存布局:
 *   0x08015BE4: PUSH.W {r4,r5,r6,r7,r8,lr}
 *   0x08015BE8: MOV r5,r0
 *   0x08015BEA: MOV r6,r1
 *   0x08015BEC: MOVS r4,#0
 *   0x08015BEE: B outer_check
 *   0x08015BF0 - 0x08015C06: 阶段 A 循环 (mask=0x80)
 *   0x08015C08 - 0x08015C0C: 阶段 B 处理
 *   0x08015C10 - 0x08015C26: 阶段 C 循环 (mask=0x40)
 *   0x08015C28 - 0x08015C2E: 字节索引递增 + 外循环检查
 *   0x08015C30: POP.W {r4,r5,r6,r7,r8,pc}
 *
 * 字面量池 @ 0x08015C34 (单一 32-bit 字):
 *   0x40004400 — 所有 3 个 LDR 指令均引用此地址
 *   (LDR [pc,#0x38]@0x08015BF8, LDR [pc,#0x28]@0x08015C0A, LDR [pc,#0x18]@0x08015C18)
 *   0x08015C38: 下一函数开始 (PUSH {r4,r5,r6,lr}) — 非字面量池
 *
 * 注: 原 func_159 版本声称有超时标志写入和 -1 返回,
 *     但二进制中不存在 STRB/[r6] 或 MOV r0,#-1.
 *     超时仅导致循环退出, 无特殊错误处理.
 *     阶段 B 的 func_15F2E 负责字节级协议处理.
 */
