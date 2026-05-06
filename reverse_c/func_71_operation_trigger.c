/**
 * @file func_71_operation_trigger.c
 * @brief 函数: 操作触发器 — 根据标志调用 func_12528 + func_943E
 * @addr  0x0801257A - 0x080125AE (52 bytes)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 若 flag==1: func_12528(mode, selector, 1) + func_943E(mode, selector, param)
 * 否则:       func_12528(mode, selector, 0)
 *
 * 参数:
 *   r0 = flag — 操作标志 (0 或 1)
 *   r1 = mode — 模式值
 *   r2 = selector — 选择器
 *   r3 = param — 参数 (仅 flag==1 时传入 func_943E)
 *
 * 调用:
 *   func_12528() @ 0x08012528 — 子操作 (func_70 同族)
 *   func_943E()  @ 0x0800943E — 计算操作
 */

#include "stm32f4xx_hal.h"

extern void func_12528(uint32_t mode, uint32_t selector, uint32_t flag);
extern void func_943E(uint32_t a, uint32_t b, uint32_t val);

void Operation_Trigger(uint32_t flag, uint32_t mode, uint32_t selector, uint32_t param)
{
    if (flag == 1) {
        func_12528(mode, selector, 1);
        func_943E(mode, selector, param);
    } else {
        func_12528(mode, selector, 0);
    }
}
