/**
 * @file func_56_display_init_sequence.c
 * @brief 显示初始化命令序列发送器
 * @addr  0x080115E8 - 0x08011610 (40 bytes)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 依次发送 6 个初始化命令字节 (0xEE, 0x82, 0xFF, 0xFC, 0xFF, 0xFF)
 * 到 func_123E8(), 用于 TFT/显示模块的软复位和配置序列。
 */

#include "stm32f4xx_hal.h"

extern void func_123E8(uint32_t cmd);

void Display_Init_Sequence(void)
{
    func_123E8(0xEE);
    func_123E8(0x82);
    func_123E8(0xFF);
    func_123E8(0xFC);
    func_123E8(0xFF);
    func_123E8(0xFF);
}
