/**
 * @file func_73_mode_backup_and_notify.c
 * @brief 函数: 模式备份与通知 — 备份 FLAG_24A→FLAG_172 并发送命令帧+延时
 * @addr  0x08012604 - 0x0801265C (88 bytes, 含字面量池)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 操作序列:
 *   1. FLAG_172 = FLAG_24A (备份当前模式)
 *   2. 发送帧: 0xEE 0xB1 0x00 HI(param) LO(param) 0xFF 0xFC 0xFF 0xFF
 *   3. 延时 0x32 (50ms)
 *   4. 调用 func_0D3B4(0)
 *
 * 参数:
 *   r0 = param — 16 位帧参数
 *
 * 调用:
 *   func_123E8() @ 0x080123E8 — 字节级 SPI 发送
 *   func_18488() @ 0x08018488 — 延时函数
 *   func_0D3B4() @ 0x0800D3B4 — 状态操作
 *
 * RAM:
 *   0x2000024A (FLAG_24A) — 源: 当前模式
 *   0x20000172 (FLAG_172) — 目的: 前一模式备份
 */

#include "stm32f4xx_hal.h"

#define FLAG_24A  (*(volatile uint8_t *)0x2000024A)
#define FLAG_172  (*(volatile uint8_t *)0x20000172)

extern void func_123E8(uint8_t byte);
extern void func_18488(uint32_t delay);
extern void func_0D3B4(uint32_t mode);

void Mode_Backup_And_Notify(uint16_t param)
{
    FLAG_172 = FLAG_24A;

    func_123E8(0xEE);
    func_123E8(0xB1);
    func_123E8(0x00);
    func_123E8((uint8_t)(param >> 8));
    func_123E8((uint8_t)param);
    func_123E8(0xFF);
    func_123E8(0xFC);
    func_123E8(0xFF);
    func_123E8(0xFF);

    func_18488(0x32);
    func_0D3B4(0);
}
