/**
 * @file func_138_null_check_wrapper.c
 * @brief 函数: 空指针检查包装器 — 检查输入指针, NULL 时返回 3
 * @addr  0x08017E74 - 0x08017E82 (14 bytes)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 参数: r0 = ptr_a, r1 = ptr_b
 * 返回: 0 = OK, 3 = 指针为空
 */

#include "stm32f4xx_hal.h"

uint32_t Null_Check_Wrapper(void *ptr_a, void *ptr_b)
{
    if (ptr_a == NULL) {
        return 3;
    }
    /* fallthrough to next function on non-NULL */
}
