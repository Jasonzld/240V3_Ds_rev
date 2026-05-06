/**
 * @file func_142_ascii_to_integer.c
 * @brief 函数: ASCII 到整数解析器 + 环缓冲辅助函数
 * @addr  0x08018334 - 0x08018414 (224 bytes, 含字面量池)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 子函数 A (0x08018334): atoi — ASCII 字符串转整数
 *   支持: 前导 '-', 数字 '0'-'9', 小数点 '.' 位置跟踪
 *   返回: *out_int = 解析的整数值, *out_frac_digits = 小数位数
 *   frac_cnt 机制: 遇到 '.' 后 inc, 之后每个数字 inc, 返回时 -1 = 小数位数
 *
 * 子函数 B (0x080183C4): 环缓冲读取 — 检查 bit15, 返回缓冲区指针
 * 子函数 C (0x080183F0): 环缓冲位清除 — 清除环缓冲索引 bit[14:0] + bit15
 */

#include "stm32f4xx_hal.h"

#define RING_BUF_BASE  (*(volatile uint16_t *)0x20003B4C)  /* 环缓冲基址 +0x800 */

/* 环缓冲读取 */
void *RingBuf_Read_Clear(void)
{
    uint16_t status = *(volatile uint16_t *)(0x20003B4C + 0x800);

    /* cmp.w r1, r0, lsr #15; bne → return NULL if bit15 not set */
    if (!(status & 0x8000)) {
        return NULL;
    }
    {
        uint16_t idx = status & 0x7FFF;      /* UBFX #0, #15 */
        *(volatile uint8_t *)(0x20003B4C + idx) = 0;
    }
    return (void *)0x20003B4C;
}

/* 环缓冲位清除 */
void RingBuf_Bit_Clear(void)
{
    uint16_t val = *(volatile uint16_t *)(0x20003B4C + 0x800);
    val &= ~0x7FFF;           /* BFC: 清除 bits [14:0] */
    *(volatile uint16_t *)(0x20003B4C + 0x800) = val;
    val &= ~0x8000;           /* 清除 bit15 */
    *(volatile uint16_t *)(0x20003B4C + 0x800) = val;
}

/* ASCII 字符串转整数 (支持负号和小数点) */
uint32_t ASCII_To_Integer(const char *str, int32_t *out_int, uint8_t *out_frac_digits)
{
    const char *p = str;
    int32_t     sign = 1;
    int32_t     value = 0;
    uint8_t     frac_cnt = 0;

    if (p == NULL || out_int == NULL) return 3;

    /* 检查首字符有效性 */
    if (*p != '-' && !(*p >= '0' && *p <= '9')) return 3;

    /* 处理负号 */
    if (*p == '-') {
        sign = -1;
        p++;
    }

    /* 主解析循环
     * frac_cnt 跟踪: 初值 0 → 遇 '.' += 1 → 之后每个数字 += 1
     * 返回时 frac_cnt > 0 ? frac_cnt - 1 : 0 得到实际小数位数 */
    while (1) {
        if (*p == '.') {
            if (frac_cnt == 0) {
                frac_cnt++;           /* 标记已遇到小数点 */
                p++;
                continue;
            }
        }
        if (*p >= '0' && *p <= '9') {
            value = value * 10 + (*p - '0');
            if (frac_cnt > 0) {
                frac_cnt++;           /* 小数点后每个数字累加计数 */
            }
            p++;
        } else {
            break;
        }
    }

    *out_int = value * sign;
    if (out_frac_digits) {
        *out_frac_digits = (frac_cnt > 0) ? (frac_cnt - 1) : 0;  /* 减 1 去除小数点本身 */
    }
    return 0;
}
