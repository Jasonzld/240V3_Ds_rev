/**
 * @file func_35_iwdg_key_write.c
 * @brief 函数: IWDG_KeyWrite — IWDG 密钥寄存器写入 (叶函数)
 * @addr  0x0800D738 - 0x0800D748 (16 bytes, 含字面量池)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 向 IWDG_KR (独立看门狗密钥寄存器) 写入 0xAAAA,
 * 重载看门狗计数器 (喂狗操作)。
 *
 * STM32F4 IWDG_KR 地址: 0x40003000
 *   0xAAAA = 计数器重载 (喂狗)
 *   0x5555 = 允许访问 PR/RLR 寄存器
 *   0xCCCC = 启动看门狗
 *
 * 叶函数, 无栈帧, bx lr 返回。
 *
 * 字面量池:
 *   0x0800D742: 0x0000 (对齐填充)
 *   0x0800D744: 0x40003000 (IWDG_KR)
 */

#include "stm32f4xx_hal.h"

#define IWDG_KR  (*(volatile uint32_t *)0x40003000)

/* ================================================================
 * IWDG_KeyWrite() @ 0x0800D738
 *   指令: movw r0,#0xAAAA; ldr r1,=IWDG_KR; str r0,[r1]; bx lr
 * ================================================================ */
void IWDG_KeyWrite(void)
{
    IWDG_KR = 0xAAAA;                            /* 0x0800D738-3E: movw r0,#0xaaaa; str r0,[IWDG_KR] */
}
