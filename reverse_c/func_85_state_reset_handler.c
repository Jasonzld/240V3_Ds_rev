/**
 * @file func_85_state_reset_handler.c
 * @brief 函数: 状态重置处理器 — 清除所有运行时状态并重启设备
 * @addr  0x08013158 - 0x08013198 (64 bytes, 含字面量池)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 若 func_133EC(device, 1) == 1:
 *   - 清除 4 个 RAM 变量 (2 byte + 2 halfword)
 * 然后调用 func_133B0(device, 1) 重启设备。
 *
 * 参数: (无寄存器参数)
 *
 * 调用:
 *   func_133EC() @ 0x080133EC — 设备状态查询
 *   func_133B0() @ 0x080133B0 — 设备启动
 */

#include "stm32f4xx_hal.h"

extern uint32_t func_133EC(uint32_t id, uint32_t val);
extern void     func_133B0(uint32_t id, uint32_t val);

#define DEV_ID  0x40000C00  /* 从字面量池加载 */

/* RAM 变量 */
#define CLEAR_BYTE_A  (*(volatile uint8_t  *)0x2000020A)
#define CLEAR_HWORD_A (*(volatile uint16_t *)0x20000208)
#define CLEAR_BYTE_B  (*(volatile uint8_t  *)0x2000020B)
#define CLEAR_HWORD_B (*(volatile uint16_t *)0x2000003E)

void State_Reset_Handler(void)
{
    if (func_133EC(DEV_ID, 1) == 1) {
        CLEAR_BYTE_A  = 0;
        CLEAR_HWORD_A = 0;
        CLEAR_BYTE_B  = 0;
        CLEAR_HWORD_B = 0;
    }

    func_133B0(DEV_ID, 1);
}
