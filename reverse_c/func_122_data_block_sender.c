/**
 * @file func_122_data_block_sender.c
 * @brief 函数: 数据块发送器 — 发送命令头 + 数据块
 * @addr  0x080161F0 - 0x08016244 (84 bytes, 含字面量池)
 * DISASSEMBLY-TRACED. Verified against Capstone (both clear/set use same pool addr 0x424082A8).
 *
 * 发送协议: [0x02] [offset_hi] [offset_mid] [offset_lo] [data[0]] ... [data[len-1]]
 * 完成标志设置后轮询就绪.
 *
 * 参数:
 *   r0 = data — 数据缓冲区指针
 *   r1 = offset — 偏移值 (24-bit, 分 3 字节发送)
 *   r2 = len — 数据长度
 *
 * 调用:
 *   func_16188() @ 0x08016188 — 命令初始化
 *   func_118EC() @ 0x080118EC — USART 发送字节
 *   func_160BC() @ 0x080160BC — 就绪轮询
 */

#include "stm32f4xx_hal.h"

#define FLAG_BITBAND  (*(volatile uint32_t *)0x424082A8)       /* bit-band alias for GPIOB_ODR[10] (PB10) */

extern void     func_16188(void);
extern uint8_t func_118EC(uint8_t byte);
extern void     func_160BC(void);

void Data_Block_Sender(const uint8_t *data, uint32_t offset, uint32_t len)
{
    uint32_t i;

    func_16188();                    /* 发送命令头 0x06 */

    FLAG_BITBAND = 0;                    /* 清除响应标志 */

    /* 发送命令字节 + 3 字节偏移 */
    func_118EC(0x02);                     /* 命令 */
    func_118EC((offset >> 16) & 0xFF);    /* offset[23:16] */
    func_118EC((offset >> 8) & 0xFF);     /* offset[15:8] */
    func_118EC(offset & 0xFF);            /* offset[7:0] */

    /* 发送数据块 */
    for (i = 0; i < len; i++) {
        func_118EC(data[i]);
    }

    /* 设置完成标志 */
    FLAG_BITBAND = 1;

    func_160BC();                    /* 等待就绪 */
}
