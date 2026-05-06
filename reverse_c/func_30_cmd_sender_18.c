/**
 * @file func_30_cmd_sender_18.c
 * @brief 函数: Send_Cmd18 — 协议命令 0x12 发送器
 * @addr  0x0800D558 - 0x0800D578 (32 bytes, 含字面量池)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 调用底层协议命令引擎发送命令码 0x12,
 * 附带本地常量数据指针 (adr r0,#4 → 0x0800D564 处 "AT+CIPGSMLOC=1,1\r\n")。
 *
 * 调用:
 *   cmd_engine() @ 0x0800BBFC — 协议命令引擎
 *
 * 本地数据:
 *   0x0800D564: "AT+CIPGSMLOC=1,1\r\n\0" (内联 AT 命令字符串)
 *   0x0800D560: 0x4DFB (BL 指令后半, 非数据)
 */

#include "stm32f4xx_hal.h"

/* 外部函数声明 */
extern void cmd_engine(uint8_t *data, uint32_t cmd_code);  /* 0x0800BBFC */

/* ================================================================
 * Send_Cmd18() @ 0x0800D558
 *   发送协议命令 0x12
 *   指令: push{r4,lr}; movs r1,#0x12; adr r0,#4; bl cmd_engine; pop{r4,pc}
 *
 * ADR 计算: r0 = Align(0x0800D55C+4,4) + 4*1 = 0x0800D564
 *   指向内联 AT 命令字符串 "AT+CIPGSMLOC=1,1\r\n"
 * ================================================================ */
void Send_Cmd18(void)
{
    cmd_engine((uint8_t *)0x0800D564, 0x12);     /* adr r0,#4 → 0x0800D564, AT cmd string */
}
