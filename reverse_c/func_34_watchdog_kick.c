/**
 * @file func_34_watchdog_kick.c
 * @brief 函数: Watchdog_Kick — 独立看门狗喂狗包装器
 * @addr  0x0800D730 - 0x0800D738 (8 bytes, 3 指令)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 调用 IWDG_KeyWrite(0xAAAA) 重载看门狗计数器。
 * push {r4, lr} 仅用于保存 lr (调用子函数需要),
 * r4 未被实际使用 (push/pop 对称为编译器约定)。
 *
 * 调用:
 *   IWDG_KeyWrite() @ 0x0800D738 — 写入 0xAAAA 到 IWDG_KR
 */

#include "stm32f4xx_hal.h"

extern void IWDG_KeyWrite(void);   /* 0x0800D738 */

/* ================================================================
 * Watchdog_Kick() @ 0x0800D730
 *   指令: push {r4,lr}; bl IWDG_KeyWrite; pop {r4,pc}
 * ================================================================ */
void Watchdog_Kick(void)
{
    IWDG_KeyWrite();                             /* 0x0800D732: bl #0x800d738 */
}
