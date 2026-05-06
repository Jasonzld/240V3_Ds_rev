/**
 * @file func_79_direct_call_wrapper.c
 * @brief 函数: 直接调用包装器 — 无参数包装 func_1352C
 * @addr  0x080128AE - 0x080128B6 (8 bytes)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 单纯转发调用到 func_1352C，无参数传递或返回值处理。
 *
 * 参数: (无)
 *
 * 调用:
 *   func_1352C() @ 0x0801352C
 */

#include "stm32f4xx_hal.h"

extern void func_1352C(void);

void Direct_Call_Wrapper(void)
{
    func_1352C();
}
