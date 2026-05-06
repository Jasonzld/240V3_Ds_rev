/**
 * @file func_45_cmd_handler_0_and_a.c
 * @brief 函数: Cmd_Handler_0_and_A — 命令组 0xA (Flash 表查找) 和 0 (NVIC 序列) 处理器
 * DISASSEMBLY-TRACED. Verified against Capstone.
 * @addr  0x0800E0BC - 0x0800E180 (196 bytes, 含字面量池)
 *         字面量池 0x0800E16A-0x0800E17E (20 bytes)
 *
 * 两种独立操作模式:
 *   cmd=0xA, sub=2: 将 arg 存入 FLAG_6A, 查 Flash 表 TBL_8F88, 结果写入 FLAG_68,
 *                    发送 cmd 0x20 (含 bit7 + LSB 两字节)
 *   cmd=0, sub=5:   FLAG_1C0 操作序列 — 条件赋值/函数调用/表计算/cmd 0x90 发送
 *
 * 参数:
 *   r0/r4 = cmd_group
 *   r1/r6 = sub_cmd
 *   r2/r5 = arg
 *   r3/r7 = (未使用, 栈传参占位)
 *
 * 调用:
 *   func_0BC2C() @ 0x0800BC2C — 帧发送
 *   func_12528() @ 0x08012528 — 操作
 *   func_943E()  @ 0x0800943E — 计算
 *   func_12788() @ 0x08012788 — 数据传输
 *   func_1257A() @ 0x0801257A — 操作触发
 *
 * 字面量池 (0x0800E16A-0x0800E17E):
 *   0x0800E16C: 0x2000006A (FLAG_6A)
 *   0x0800E170: 0x08018F88 (TBL_8F88, Flash 半字表)
 *   0x0800E174: 0x20000068 (FLAG_68)
 *   0x0800E178: 0x200001C0 (FLAG_1C0)
 *   0x0800E17C: 0x20000252 (BUF_252, 表基址)
 */

#include "stm32f4xx_hal.h"

#define FLAG_6A     (*(volatile uint8_t  *)0x2000006A)
#define FLAG_68     (*(volatile uint16_t *)0x20000068)
#define FLAG_1C0    (*(volatile uint8_t  *)0x200001C0)
#define TBL_8F88    ((volatile uint16_t *)0x08018F88)  /* Flash 常量表 */
#define BUF_252     ((volatile uint8_t  *)0x20000252)   /* 9 字节/条目 */

extern void func_0BC2C(uint32_t cmd, uint8_t *data, uint32_t len);
extern void func_12528(uint32_t a, uint32_t b, uint32_t c);
extern void func_943E(uint32_t a, uint32_t b, uint32_t c);
extern void func_12788(uint32_t a, uint32_t b, uint8_t *ptr);
extern void func_1257A(uint8_t a, uint32_t b, uint32_t c, uint32_t d);

/* ================================================================
 * Cmd_Handler_0_and_A() @ 0x0800E0BC
 *   push {r3, r4, r5, r6, r7, lr}
 *   r4=r0, r6=r1, r5=r2, r7=r3
 * ================================================================ */
void Cmd_Handler_0_and_A(uint32_t cmd_group, uint32_t sub_cmd, uint32_t arg, uint32_t unused)
{
    uint8_t stack_buf[2];  /* sp+0..sp+1 */

    /* ================================================================
     * 模式 1: cmd_group == 0xA, sub_cmd == 2
     *   Flash 表查找: FLAG_68 = TBL_8F88[arg]
     *   发送 cmd 0x20: [bit7(FLAG_68), LSB(FLAG_68)]
     * ================================================================ */
    if (cmd_group == 0xA) {                              /* 0x0800E0C6: cmp r4,#0xa; bne */
        if (sub_cmd != 2) goto done;                     /* 0x0800E0CA: cmp r6,#2; bne */

        FLAG_6A = (uint8_t)arg;                          /* 0x0800E0CE-D0: strb r5,[r0] */
        FLAG_68 = TBL_8F88[FLAG_6A];                     /* 0x0800E0D2-DE: ldr r0,TBL; ldrb r1; ldrh.w; strh */

        /* 构建帧: [bit7(FLAG_68), LSB(FLAG_68)] */
        stack_buf[0] = (uint8_t)(FLAG_68 >> 8);         /* 0x0800E0E0-E6: ldrh; asrs#8; strb.w — ldrh zero-extends 16-bit, asrs extracts bits[15:8] */
        stack_buf[1] = (uint8_t)FLAG_68;                 /* 0x0800E0EA-EE: ldrb; strb.w */
        func_0BC2C(0x20, stack_buf, 2);                  /* 0x0800E0F2-F8: movs r2,#2; mov r1,sp; movs r0,#0x20; bl */
        goto done;
    }

    /* ================================================================
     * 模式 2: cmd_group == 0, sub_cmd == 5
     *   条件赋值: 若 arg < 2, 则 FLAG_1C0 = (uint8_t)arg
     *   条件分支:
     *     FLAG_1C0 == 1 → func_12528(0, 0xE, 1) + func_943E(0, 0xE, 0)
     *     FLAG_1C0 != 1 → func_12528(0, 0xE, 0)
     *   共同: func_12788(0, 8, BUF_252 + FLAG_1C0 * 9)
     *         func_1257A(FLAG_1C0, 0xE, 0, 0)
     *         func_0BC2C(0x90, [FLAG_1C0], 1)
     * ================================================================ */
    if (cmd_group != 0) goto done;                       /* 0x0800E0FE: cbnz r4,#done */
    if (sub_cmd != 5) goto done;                         /* 0x0800E100: cmp r6,#5; bne */

    /* 条件赋值: arg < 2 时更新 FLAG_1C0 */
    if ((int32_t)arg < 2) {                              /* 0x0800E104: cmp r5,#2; bge */
        FLAG_1C0 = (uint8_t)arg;                         /* 0x0800E108-0A: ldr r0; strb r5 */
    }

    /* 条件分支: 按 FLAG_1C0 值 */
    if (FLAG_1C0 == 1) {                                 /* 0x0800E10C-12: ldrb; cmp#1; bne */
        func_12528(0, 0xE, 1);                           /* 0x0800E114-1A: movs r2,#1; movs r1,#0xe; movs r0,#0; bl */
        func_943E(0, 0xE, 0);                            /* 0x0800E11E-24: movs r2,#0; movs r1,#0xe; mov r0,r2; bl */
    } else {
        func_12528(0, 0xE, 0);                           /* 0x0800E12A-30: movs r2,#0; movs r1,#0xe; mov r0,r2; bl */
    }

    /* 共同: 表偏移计算, 数据传输, 帧发送 */
    {
        uint8_t *ptr;
        ptr = BUF_252 + FLAG_1C0 * 9;                    /* 0x0800E134-3E: ldrb; add.w r0,r0,r0,lsl#3; add r1; adds r2 */
        func_12788(0, 8, ptr);                           /* 0x0800E140-44: movs r1,#8; movs r0,#0; bl */
    }
    func_1257A(FLAG_1C0, 0, 0xE, 0);          /* 0x0800E148-52: uxtb r1,r4; movs r3,#0; movs r2,#0xe; ldrb r0; bl */
                                                         /* 注: r1=(uint8_t)cmd_group(=0) 已优化为立即数 */
    stack_buf[0] = FLAG_1C0;                             /* 0x0800E156-5A: ldrb; strb.w [sp] */
    func_0BC2C(0x90, stack_buf, 1);                      /* 0x0800E15E-64: movs r2,#1; mov r1,sp; movs r0,#0x90; bl */

done:
    return;                                              /* 0x0800E168: pop {r3,r4,r5,r6,r7,pc} */
}
