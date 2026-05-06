/**
 * @file func_25_protocol_ack_nak.c
 * @brief 协议 ACK/NAK 帧发送器
 * @addr  0x0800D3B4 - 0x0800D412 (94 bytes)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 子函数:
 *   send_ack_frame @ 0x0800D3B4 (48B, PUSH {r4,lr})
 *   send_nak_frame @ 0x0800D3E4 (46B, PUSH {r4,lr})
 */

#include "stm32f4xx_hal.h"

extern void uart_send_byte(uint8_t byte);   /* 0x080123E8 */

/* ================================================================
 * send_ack_frame() @ 0x0800D3B4
 *   发送 ACK 应答帧: EE B1 01 FF FC FF FF
 *   r0 = 未使用 (保存到 r4 后忽略)
 *
 * 注: r0 参数保存到 r4 (mov r4,r0) 但从未发送,
 *     可能为调用约定兼容性保留 (调用者期望 r4 不变)。
 * ================================================================ */
void send_ack_frame(uint32_t unused)
{
    (void)unused;                                /* 0x0800D3B6: mov r4, r0 — saved but not sent */

    uart_send_byte(0xEE);                        /* 0x0800D3B8-BA */
    uart_send_byte(0xB1);                        /* 0x0800D3BE-C0 */
    uart_send_byte(0x01);                        /* 0x0800D3C4-C6: 命令码 0x01 = ACK */
    uart_send_byte(0xFF);                        /* 0x0800D3CA-CC */
    uart_send_byte(0xFC);                        /* 0x0800D3D0-D2 */
    uart_send_byte(0xFF);                        /* 0x0800D3D6-D8 */
    uart_send_byte(0xFF);                        /* 0x0800D3DC-DE */
}

/* ================================================================
 * send_nak_frame() @ 0x0800D3E4
 *   发送 NAK 应答帧: EE FE 01 FF FC FF FF
 *   第二字节为 0xFE (区别于 ACK 的 0xB1), 表示否定应答/错误
 * ================================================================ */
void send_nak_frame(void)
{
    uart_send_byte(0xEE);                        /* 0x0800D3E6-E8 */
    uart_send_byte(0xFE);                        /* 0x0800D3EC-EE: NAK 标识 */
    uart_send_byte(0x01);                        /* 0x0800D3F2-F4 */
    uart_send_byte(0xFF);                        /* 0x0800D3F8-FA */
    uart_send_byte(0xFC);                        /* 0x0800D3FE-400 */
    uart_send_byte(0xFF);                        /* 0x0800D404-06 */
    uart_send_byte(0xFF);                        /* 0x0800D40A-0C */
}
