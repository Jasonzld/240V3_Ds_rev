/**
 * @file func_55_timer_config_sequence.c
 * @brief Timer/DMA 配置序列 — 多步外设初始化
 * @addr  0x080115B8 - 0x080115E8 (48 bytes)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 按顺序调用 5 个配置函数完成定时器/DMA/ADC 设置:
 *   func_133B6(mode, 0) — 初始化
 *   func_134A4(mode, 0) — 次级配置
 *   func_1349C(mode, sub_mode) — 参数设置
 *   func_133B0(mode, 1) — 启动准备
 *   func_133B6(mode, 1) — 最终激活
 *
 * 参数:
 *   r0 = mode — 外设模式选择
 *   r1 = sub_mode — 子配置参数
 */

#include "stm32f4xx_hal.h"

extern void func_133B6(uint32_t mode, uint32_t flag);
extern void func_134A4(uint32_t mode, uint32_t flag);
extern void func_1349C(uint32_t mode, uint32_t sub_mode);
extern void func_133B0(uint32_t mode, uint32_t flag);

void Timer_Config_Sequence(uint32_t mode, uint32_t sub_mode)
{
    func_133B6(mode, 0);                      /* 初始化, flag=0 */
    func_134A4(mode, 0);                      /* 次级配置, flag=0 */
    func_1349C(mode, sub_mode);               /* 参数设置 */
    func_133B0(mode, 1);                      /* 启动准备, flag=1 */
    func_133B6(mode, 1);                      /* 最终激活, flag=1 */
}
