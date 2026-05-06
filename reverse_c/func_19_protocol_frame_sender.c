/**
 * @file func_19_protocol_frame_sender.c
 * @brief 函数: send_cmd_frame — 协议命令帧发送器 (2参数)
 * @addr  0x0800C608 - 0x0800C652 (74 bytes, 26 指令)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 通过 uart_send_byte() 逐字节发送一个 11 字节协议命令帧:
 *   帧头 (3B): 0xEE, 0xB1, 0x11
 *   参数1 (2B): param1 大端序 (高字节在前)
 *   参数2 (2B): param2 大端序 (高字节在前)
 *   帧尾 (4B): 0xFF, 0xFC, 0xFF, 0xFF
 *
 * 参数:
 *   r0 = param1 (uint16_t, 命令参数1)
 *   r1 = param2 (uint16_t, 命令参数2)
 *
 * 调用:
 *   uart_send_byte() @ 0x080123E8 — USART 单字节阻塞发送
 *
 * 帧格式 (11 字节总长):
 *   +0: 0xEE (起始字节1)
 *   +1: 0xB1 (起始字节2)
 *   +2: 0x11 (命令类型/帧标识)
 *   +3: param1[15:8] (高字节)
 *   +4: param1[7:0]  (低字节)
 *   +5: param2[15:8] (高字节)
 *   +6: param2[7:0]  (低字节)
 *   +7: 0xFF
 *   +8: 0xFC
 *   +9: 0xFF
 *  +10: 0xFF
 */

#include "stm32f4xx_hal.h"

/* 外部函数声明 */
extern void uart_send_byte(uint8_t byte);   /* 0x080123E8 */

/* ================================================================
 * send_cmd_frame() @ 0x0800C608
 *   r0 = param1 (16-bit 命令参数)
 *   r1 = param2 (16-bit 命令参数)
 *
 * 帧结构: EE B1 11 [p1_hi] [p1_lo] [p2_hi] [p2_lo] FF FC FF FF
 * ================================================================ */
void send_cmd_frame(uint16_t param1, uint16_t param2)
{
    /* 帧头: 0xEE */
    uart_send_byte(0xEE);                        /* 0x0800C60E-10: movs r0,#0xee; bl */

    /* 帧头: 0xB1 */
    uart_send_byte(0xB1);                        /* 0x0800C614-16: movs r0,#0xb1; bl */

    /* 帧头: 0x11 (命令类型标识) */
    uart_send_byte(0x11);                        /* 0x0800C61A-1C: movs r0,#0x11; bl */

    /* 参数1 高字节 [15:8] */
    uart_send_byte((uint8_t)(param1 >> 8));      /* 0x0800C620-22: lsrs r0,r4,#8; bl */

    /* 参数1 低字节 [7:0] */
    uart_send_byte((uint8_t)param1);             /* 0x0800C626-28: uxtb r0,r4; bl */

    /* 参数2 高字节 [15:8] */
    uart_send_byte((uint8_t)(param2 >> 8));      /* 0x0800C62C-2E: lsrs r0,r5,#8; bl */

    /* 参数2 低字节 [7:0] */
    uart_send_byte((uint8_t)param2);             /* 0x0800C632-34: uxtb r0,r5; bl */

    /* 帧尾: 0xFF, 0xFC, 0xFF, 0xFF */
    uart_send_byte(0xFF);                        /* 0x0800C638-3A: movs r0,#0xff; bl */
    uart_send_byte(0xFC);                        /* 0x0800C63E-40: movs r0,#0xfc; bl */
    uart_send_byte(0xFF);                        /* 0x0800C644-46: movs r0,#0xff; bl */
    uart_send_byte(0xFF);                        /* 0x0800C64A-4C: movs r0,#0xff; bl */
}
