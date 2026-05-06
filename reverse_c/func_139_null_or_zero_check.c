/**
 * @file func_139_null_or_zero_check.c
 * @brief 函数: 空指针/零值检查包装器 — 检查两个参数, 任一为 NULL/0 返回 3
 * @addr  0x08017EDC - 0x08017EEC (16 bytes)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 参数: r0 = val_a, r1 = val_b
 * 返回: 3 = 任一为 NULL/0, 继续执行 = OK
 */

#include "stm32f4xx_hal.h"

uint32_t Null_Or_Zero_Check(uint32_t val_a, uint32_t val_b)
{
    if (val_a == 0) {
        return 3;
    }
    if (val_b == 0) {
        return 3;
    }
    /* fallthrough to next function when both non-zero */
}
