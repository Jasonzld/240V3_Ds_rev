/**
 * @file func_21_cmd_builder.c
 * @brief 函数: Build_ProtocolCmd — 协议命令结构体构造器 (栈参数)
 * @addr  0x0800C6C4 - 0x0800C710 (76 bytes, 含字面量池)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 从打包在 r0/r1 寄存器中的多个参数构造协议命令结构体,
 * 存储到 RAM 缓冲区 (0x200004F0), 然后调用处理函数。
 *
 * 参数 (通过 r0/r1 打包传递):
 *   r0[15:0]  = hw_val   (16-bit 硬件相关值, 减去 0x76C 后存储)
 *   r0[23:16] = byte_sub1 (减去 1 后存储, 通常为长度或索引)
 *   r0[31:24] = byte_raw  (直接存储)
 *   r1[7:0]   = byte_b0   (直接存储)
 *   r1[23:16] = byte_b1   (直接存储)
 *   r1[31:24] = byte_b2   (直接存储)
 *
 * 结构体布局 (在 0x200004F0, 9 个 32-bit 字 = 36 字节):
 *   [0x00] = byte_b2     (r1[31:24])
 *   [0x04] = byte_b1     (r1[23:16])
 *   [0x08] = byte_b0     (r1[7:0])
 *   [0x0C] = byte_raw    (r0[31:24])
 *   [0x10] = byte_sub1-1 (r0[23:16] - 1)
 *   [0x14] = hw_val-0x76C (r0[15:0] - 1900)
 *   [0x20] = -1 (0xFFFFFFFF, 哨兵值/结束标记)
 *
 * 调用:
 *   func_08268() @ 0x08008268 — 协议命令处理/发送
 *
 * 注: push {r0, r1, r2, lr} 将 r0-r2 推入栈中,
 *     后续通过 sp 相对寻址访问打包的参数字段。
 *     r2 在栈上保留但未被此函数读取 (调用者上下文保留,
 *     或仅为栈对齐目的)。
 */

#include "stm32f4xx_hal.h"

/* 协议命令缓冲区 (9 个 32-bit 字) */
#define CMD_BUF       ((volatile uint32_t *)0x200004F0)

/* 全局命令指针 */
#define CMD_BUF_PTR   (*(volatile uint32_t *)0x2000020C)

/* 外部函数声明 */
extern void func_08268(uint32_t *cmd_buf);   /* 0x08008268 — 协议命令处理 */

/* ================================================================
 * Build_ProtocolCmd() @ 0x0800C6C4
 *   从打包寄存器参数构造协议命令结构体并提交处理
 *
 * 寄存器入参 (非标准调用约定):
 *   r0 = (byte_raw << 24) | (byte_sub1 << 16) | hw_val
 *   r1 = (byte_b2 << 24) | (byte_b1 << 16) | (unused << 8) | byte_b0
 *   r2 = 未使用 (在栈上保留)
 *
 * 指令跟踪:
 *   0x0800C6C4: push {r0,r1,r2,lr}  — 保存 4 寄存器到栈
 *   0x0800C6C6: ldrh.w r0,[sp]       — r0 = hw_val (栈顶低 16-bit)
 *   0x0800C6CA: subw r0,#0x76c       — r0 = hw_val - 1900
 *   0x0800C6CE: ldr r1,=CMD_BUF      — r1 = 0x200004F0
 *   0x0800C6D0: str r0,[r1,#0x14]    — cmd[5] = hw_val - 1900
 *   0x0800C6D2: ldrb.w r0,[sp,#2]    — r0 = byte_sub1
 *   0x0800C6D6: subs r0,#1           — r0 = byte_sub1 - 1
 *   0x0800C6D8: str r0,[r1,#0x10]    — cmd[4] = byte_sub1 - 1
 *   0x0800C6DA: ldrb.w r0,[sp,#3]    — r0 = byte_raw
 *   0x0800C6DE: str r0,[r1,#0xc]     — cmd[3] = byte_raw
 *   0x0800C6E0: ldrb.w r0,[sp,#4]    — r0 = byte_b0 (r1[7:0])
 *   0x0800C6E4: str r0,[r1,#8]       — cmd[2] = byte_b0
 *   0x0800C6E6: ldrb.w r0,[sp,#6]    — r0 = byte_b1 (r1[23:16])
 *   0x0800C6EA: str r0,[r1,#4]       — cmd[1] = byte_b1
 *   0x0800C6EC: ldrb.w r0,[sp,#7]    — r0 = byte_b2 (r1[31:24])
 *   0x0800C6F0: str r0,[r1,#0]       — cmd[0] = byte_b2
 *   0x0800C6F2: mov.w r0,#-1         — r0 = 0xFFFFFFFF
 *   0x0800C6F6: str r0,[r1,#0x20]    — cmd[8] = -1 (哨兵)
 *   0x0800C6F8: mov r0,r1            — r0 = &cmd_buf
 *   0x0800C6FA: ldr r1,=CMD_BUF_PTR  — r1 = &CMD_BUF_PTR
 *   0x0800C6FC: str r0,[r1]          — CMD_BUF_PTR = &cmd_buf
 *   0x0800C6FE: mov r0,r1            — (冗余, 已优化)
 *   0x0800C700: ldr r0,[r0]          — r0 = CMD_BUF_PTR (= &cmd_buf)
 *   0x0800C702: bl #func_08268       — 调用处理函数
 *   0x0800C706: pop {r1,r2,r3,pc}    — 恢复并返回
 * ================================================================ */
void Build_ProtocolCmd(uint32_t packed_r0, uint32_t packed_r1)
{
    uint16_t hw_val;      /* r0[15:0] */
    uint8_t  byte_sub1;   /* r0[23:16] */
    uint8_t  byte_raw;    /* r0[31:24] */
    uint8_t  byte_b0;     /* r1[7:0] */
    uint8_t  byte_b1;     /* r1[23:16] */
    uint8_t  byte_b2;     /* r1[31:24] */

    /* 解包寄存器参数 (模拟 push {r0,r1,r2,lr} 后从栈读取) */
    hw_val    = (uint16_t)(packed_r0 & 0xFFFF);         /* [sp+0] ldrh */
    byte_sub1 = (uint8_t)((packed_r0 >> 16) & 0xFF);    /* [sp+2] ldrb */
    byte_raw  = (uint8_t)(packed_r0 >> 24);             /* [sp+3] ldrb */
    byte_b0   = (uint8_t)(packed_r1 & 0xFF);            /* [sp+4] ldrb */
    byte_b1   = (uint8_t)((packed_r1 >> 16) & 0xFF);    /* [sp+6] ldrb */
    byte_b2   = (uint8_t)(packed_r1 >> 24);             /* [sp+7] ldrb */

    /* 构造命令结构体 */
    CMD_BUF[0] = byte_b2;                    /* 0x0800C6EC-F0: [r1,#0x00] */
    CMD_BUF[1] = byte_b1;                    /* 0x0800C6E6-EA: [r1,#0x04] */
    CMD_BUF[2] = byte_b0;                    /* 0x0800C6E0-E4: [r1,#0x08] */
    CMD_BUF[3] = byte_raw;                   /* 0x0800C6DA-DE: [r1,#0x0C] */
    CMD_BUF[4] = (uint32_t)(byte_sub1 - 1);  /* 0x0800C6D2-D8: [r1,#0x10] (subs #1) */
    CMD_BUF[5] = (uint32_t)(hw_val - 0x76C); /* 0x0800C6C6-D0: [r1,#0x14] (subw #0x76C) */
    CMD_BUF[8] = 0xFFFFFFFF;                 /* 0x0800C6F2-F6: [r1,#0x20] mov.w #-1 */

    /* 保存全局指针并提交处理 */
    CMD_BUF_PTR = (uint32_t)CMD_BUF;         /* 0x0800C6F8-FC: str r0,[r1] */

    func_08268(CMD_BUF);                     /* 0x0800C702: bl */
}
