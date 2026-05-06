/**
 * @file func_66_parameter_operation.c
 * @brief 函数: 参数操作 — 调用 func_12528 + 读取 FLAG_173 写入 func_943E
 * @addr  0x08012430 - 0x08012454 (36 bytes, 含字面量池)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 第一步: func_12528(mode, 0x1E, flag)。第二步: func_943E(mode, 0x1E, FLAG_173)。
 *
 * 参数:
 *   r0 = flag (FLAG_121)
 *   r1 = mode — 操作模式
 *   r2 = (保留/未使用 — 仅压栈保存)
 *
 * 调用:
 *   func_12528() @ 0x08012528 — 子操作
 *   func_943E()  @ 0x0800943E — 计算操作
 *
 * RAM:
 *   0x20000173 (FLAG_173) — 参数字节
 */

#include "stm32f4xx_hal.h"

#define FLAG_173  (*(volatile uint8_t *)0x20000173)

extern void func_12528(uint32_t mode, uint32_t selector, uint32_t flag);
extern void func_943E(uint32_t a, uint32_t b, uint32_t val);

void Parameter_Operation(uint32_t flag, uint32_t mode, uint32_t unused)
{
    (void)unused;

    func_12528(mode, 0x1E, flag);
    func_943E(mode, 0x1E, FLAG_173);
}
