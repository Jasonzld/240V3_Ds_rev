/**
 * @file func_144_bcd_digit_encoder.c
 * @brief 函数: BCD 数字编码器 — 将整数的十进制各位编码为紧凑格式
 * @addr  0x08018424 - 0x0801844C (40 bytes)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 输入: uint32_t val
 * 输出: 编码后的值 (各位数字按 4-bit 分组)
 *
 * 算法: 循环执行 divmod 10, 将余数左移后 OR 到结果
 *   while (val > 0):
 *     digit = val % 10
 *     result |= digit << (4 * cnt)
 *     val /= 10
 *     cnt++
 */

#include "stm32f4xx_hal.h"

uint32_t BCD_Digit_Encoder(uint32_t val)
{
    uint32_t result = 0;
    uint32_t digit;
    uint32_t shift = 0;

    while (val > 0) {
        digit = val % 10;
        result |= digit << shift;
        val /= 10;
        shift += 4;
    }

    return result;
}
