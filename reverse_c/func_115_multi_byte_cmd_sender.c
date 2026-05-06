/**
 * @file func_115_multi_byte_cmd_sender.c
 * @brief 函数: 多字节命令发送器 — 发送 5 字节 USART 命令序列 + GPIO 响应
 * @addr  0x08015F60 - 0x08015FA8 (72 bytes, 含字面量池)
 *         字面量池 0x08015FA0-0x08015FA7 (2 words)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 发送 4 字节: 0x20 + (cmd>>16)&0xFF + (cmd>>8)&0xFF + cmd&0xFF
 * r0 在发送前左移 12 位 (lsl #12), 然后 UBFX 提取 [23:16]/[15:8]/[7:0].
 * 注意: cmd<<12 后 [7:0] 为 0, 故第 4 字节恒为 0x00.
 *
 * GPIO PB10 (bit-band 0x424082A8 = peripheral bit-band alias for GPIOB_ODR[10]):
 *   命令前清零, 命令后置位, 用作响应握手信号.
 *
 * 字面量池 (Capstone 验证 @ 0x08015FA0):
 *   [0] 0x424082A8 = bit-band alias for GPIOB_ODR bit 10 (PB10) — 清除响应
 *   [1] 0x42408000 = bit-band base offset; str.w +0x2a8 → 0x424082A8 — 置位响应
 *
 * 参数:
 *   r0 = 命令值 (左移 12 位后拆解为 4 字节)
 *
 * 调用:
 *   func_16188() @ 0x08016188 — 命令头
 *   func_160BC() @ 0x080160BC — 轮询就绪
 *   func_118EC() @ 0x080118EC — USART 发送字节
 */

#include "stm32f4xx_hal.h"

extern void func_16188(void);
extern void func_160BC(void);
extern void func_118EC(uint8_t byte);

/* 字面量池: bit-band alias for GPIOB_ODR[10] (PB10) */
#define RESP_BITBAND  (*(volatile uint32_t *)0x424082A8)

void Multi_Byte_Cmd_Sender(uint32_t cmd)
{
    cmd <<= 12;

    func_16188();           /* 发送命令头 0x06 */
    func_160BC();           /* 等待就绪 */

    /* 清除响应 (PB10 = 0 via bit-band alias) */
    RESP_BITBAND = 0;

    /* 发送 4 字节: 0x20 前缀 + 3 字节 (UBFX 提取) */
    func_118EC(0x20);                       /* 命令前缀 */
    func_118EC((cmd >> 16) & 0xFF);         /* UBFX #0x10,#8 → bits[23:16] */
    func_118EC((cmd >> 8) & 0xFF);          /* UBFX #8,#8 → bits[15:8] */
    func_118EC(cmd & 0xFF);                 /* UXTB → bits[7:0] (= 0 after <<12) */

    /* 设置完成标志 (PB10 = 1 via bit-band alias) */
    RESP_BITBAND = 1;

    func_160BC();           /* 等待最终就绪 */
}
