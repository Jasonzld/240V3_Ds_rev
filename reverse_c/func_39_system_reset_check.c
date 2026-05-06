/**
 * @file func_39_system_reset_check.c
 * @brief 函数: System_ResetCheck — 条件系统复位处理器
 * DISASSEMBLY-TRACED. Verified against Capstone.
 * @addr  0x0800D85C - 0x0800D8C4 (104 bytes, 含字面量池) + 字面量池 0x0800D8B8-0x0800D8C4 (12 bytes)
 *
 * 从缓冲区读取数据并判断是否需要触发系统复位。
 * 当缓冲区第 1 字节 (offset +1) 等于 7 且 func_18BB0 返回正值时,
 * 通过 SCB AIRCR 寄存器触发 SYSRESETREQ 软件复位。
 *
 * 复位序列:
 *   1. MSR FAULTMASK (禁用中断, 含 HardFault 以外)
 *   2. DSB (屏障)
 *   3. 写 SCB_AIRCR: VECTKEY=0x05FA | SYSRESETREQ=1 | 保留 PRIGROUP
 *   4. DSB (屏障, 等待复位)
 *   5. 死循环 (复位未发生时)
 *
 * 调用:
 *   func_18BB0() @ 0x08018BB0 — 数据获取 (buf, 0x40) → 返回结果计数
 *   func_10FD0() @ 0x08010FD0 — 非复位分支处理 (buf, count)
 *
 * 字面量池:
 *   0x0800D8B8: 0x2000065C (缓冲区基地址)
 *   0x0800D8BC: 0xE000ED0C (SCB_AIRCR)
 *   0x0800D8C0: 0x05FA0000 (VECTKEY << 16)
 */

#include "stm32f4xx_hal.h"

/* SCB 寄存器 */
#define SCB_AIRCR   (*(volatile uint32_t *)0xE000ED0C)
#define VECTKEY     0x05FA0000

/* 缓冲区 */
#define DATA_BUF    ((volatile uint8_t *)0x2000065C)

extern int32_t  func_18BB0(volatile uint8_t *buf, uint32_t size);  /* 0x08018BB0 */
extern void     func_10FD0(volatile uint8_t *buf, int32_t count);  /* 0x08010FD0 */

/* ================================================================
 * System_ResetCheck() @ 0x0800D85C
 *   push {r4, lr} — r4 保存 func_18BB0 返回值
 *
 * 流程:
 *   result = func_18BB0(buf, 0x40)
 *   if result > 0:
 *     if buf[1] != 7: func_10FD0(buf, result); return
 *     if buf[1] == 7: → 系统复位
 *   else: return (无操作)
 * ================================================================ */
void System_ResetCheck(void)
{
    int32_t result;      /* r4 */
    uint8_t seq_byte;    /* r0 */

    result = func_18BB0(DATA_BUF, 0x40);             /* 0x0800D85E-62: movs r1,#0x40; ldr r0,=buf; bl */

    if (result <= 0) {                               /* 0x0800D868-6A: cmp r4,#0; ble #check_seq2 */
        goto check_seq;
    }

    /* 结果 > 0: 检查 buf[1] */
    seq_byte = DATA_BUF[1];                          /* 0x0800D86C-6E: ldr r0,=buf; ldrb r0,[r0,#1] */
    if (seq_byte == 7) {                             /* 0x0800D870-72: cmp r0,#7; beq #check_seq */
        goto check_seq;                              /* buf[1]==7 → 进入复位判定 */
    }

    /* buf[1] != 7: 非复位分支 */
    func_10FD0(DATA_BUF, result);                    /* 0x0800D874-78: mov r1,r4; ldr r0,=buf; bl */
    return;                                          /* 0x0800D87C-B6: b #pop */

check_seq:
    /* result > 0 && buf[1] == 7 → 触发系统复位 */
    if (result <= 0) {                               /* 0x0800D87E-80: cmp r4,#0; ble #pop */
        return;                                      /* result<=0 时跳过复位 */
    }

    seq_byte = DATA_BUF[1];                          /* 0x0800D882-84: ldr r0,=buf; ldrb r0,[r0,#1] */
    if (seq_byte != 7) {                             /* 0x0800D886-88: cmp r0,#7; bne #pop */
        return;                                      /* buf[1]!=7 时跳过复位 */
    }

    /* ---- 系统复位序列 ---- */
    /* buf[1] == 7 && result > 0 */

    /* 1. 禁用低优先级中断 (MSR FAULTMASK) */
    /* 0x0800D88A-8C: movs r0,#1; MSR FAULTMASK,r0 */
    __set_FAULTMASK(1);

    /* 2. 数据同步屏障 */
    __DSB();                                         /* 0x0800D890-94: nop; nop; dsb sy */
    /* 0x0800D896: ??? 0x8F4F — 可能为 DSB 后半 */

    /* 3. 写 SCB AIRCR 触发复位 */
    {
        uint32_t aircr;                              /* r0 */
        aircr = SCB_AIRCR;                           /* 0x0800D898-9A: ldr r0,=AIRCR; ldr r0,[r0] */
        aircr &= 0x700;                              /* 0x0800D89C-9E: and r0,r0,#0x700 — 保留 PRIGROUP */
        aircr |= VECTKEY;                            /* 0x0800D8A0-A2: ldr r1,=VECTKEY; orrs r0,r1 */
        aircr += 4;                                  /* 0x0800D8A4: adds r0,r0,#4 — SYSRESETREQ=bit2=4 */
        SCB_AIRCR = aircr;                           /* 0x0800D8A6-A8: ldr r1,=AIRCR; str r0,[r1] */
    }

    /* 4. 数据同步屏障 (等待复位) */
    __DSB();                                         /* 0x0800D8AA-AC: dsb sy */

    /* 5. 死循环 (复位未发生时无限等待) */
    while (1) {                                      /* 0x0800D8AE-B4: nop*3; b .-2 */
        __NOP();
    }
    /* 不可达: pop {r4, pc} @ 0x0800D8B6 */
}
