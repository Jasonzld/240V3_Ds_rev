/**
 * @file func_31_cmd_sender_10.c
 * @brief 函数: Send_Cmd10 — 协议命令 0x10 发送器
 * @addr  0x0800D578 - 0x0800D594 (28 bytes, 含字面量池)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 调用底层协议命令引擎发送命令码 0x10,
 * 附带本地常量数据指针 (adr r0,#4 → 0x0800D584 处 "AT+WIFILOC=1,1\r\n\0")。
 *
 * 调用:
 *   cmd_engine() @ 0x0800BBFC — 协议命令引擎
 *
 * 本地数据:
 *   0x0800D584: "AT+WIFILOC=1,1\r\n\0" (内联 AT 命令字符串)
 *   0x0800D580: 0x3DFB (BL 指令后半, 非数据)
 */

#include "stm32f4xx_hal.h"

extern void cmd_engine(uint8_t *data, uint32_t cmd_code);  /* 0x0800BBFC */

/* ================================================================
 * Send_Cmd10() @ 0x0800D578
 *   发送协议命令 0x10
 *   指令: push{r4,lr}; movs r1,#0x10; adr r0,#4; bl cmd_engine; pop{r4,pc}
 *
 * ADR 计算: r0 = Align(0x0800D57C+4,4) + 4*1 = 0x0800D584
 *   指向内联 AT 命令字符串 "AT+WIFILOC=1,1\r\n"
 * ================================================================ */
void Send_Cmd10(void)
{
    cmd_engine((uint8_t *)0x0800D584, 0x10);     /* adr r0,#4 → 0x0800D584, AT cmd string */
}
