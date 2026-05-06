/**
 * @file func_119_ready_poll_loop.c
 * @brief 函数: 就绪轮询循环 — 反复查询直到 bit 0 清零
 * @addr  0x080160BC - 0x080160CC (18 bytes)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 轮询 func_118 (发送 0x05 命令), 直到响应 bit 0 == 0.
 *
 * 调用:
 *   func_16090() @ 0x08016090 — USART Cmd 0x05 发送器
 */

#include "stm32f4xx_hal.h"

extern uint32_t func_16090(void);

void Ready_Poll_Loop(void)
{
    while (func_16090() & 1) {
        /* 等待 bit 0 清零 */
    }
}
