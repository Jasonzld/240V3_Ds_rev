/**
 * @file func_50_param_value_updater.c
 * @brief 函数: Param_Value_Updater — 更新参数寄存器并通过帧发送
 * DISASSEMBLY-TRACED. Verified against Capstone.
 * @addr  0x0800FAA8 - 0x0800FB48 (160 bytes, ~25 指令)
 *         字面量池 0x0800FB3C-0x0800FB48 (12 bytes)
 *
 * 根据子模式将有符号半字值写入指定的参数寄存器，
 * 格式化后通过两个通道发送。
 *
 * 门控: 仅当 mode == 0x12 时执行操作。
 *
 * 参数:
 *   r0 = mode      — 页面模式 (仅 0x12 有效)
 *   r1 = sub_mode  — 字段选择 (0xD 或 0xE)
 *   r2 = value     — 有符号半字值
 *
 * 调用:
 *   func_16B18() @ 0x08016B18 — sprintf 格式化
 *   func_12788() @ 0x08012788 — 数据帧发送
 *   func_BC2C()  @ 0x0800BC2C — 字节对发送
 *
 * REG_110[4:6] — sub_mode 0xD 的半字存储
 * REG_116[4:6] — sub_mode 0xE 的半字存储
 */

#include "stm32f4xx_hal.h"

#define REG_110     ((volatile uint8_t *)0x20000110)
#define REG_116     ((volatile uint8_t *)0x20000116)

extern void func_16B18(uint8_t *buf, const char *fmt, uint32_t arg1);
extern void func_12788(uint32_t mode, uint32_t len, uint8_t *data);
extern void func_BC2C(uint32_t code, uint8_t *data, uint32_t len);

/* ================================================================
 * Param_Value_Updater() @ 0x0800FAA8
 *   push {r4, r5, r6, lr}; sub sp, #0x18
 *   r6=r0(mode), r4=r1(sub_mode), r5=r2(value)
 *
 * 栈布局:
 *   sp+0x00: 字节对输出缓冲区 (2 bytes)
 *   sp+0x04: sprintf 输出缓冲区 (16 bytes)
 *   sp+0x14: 哨兵值 (清零)
 * ================================================================ */
void Param_Value_Updater(uint32_t mode, uint32_t sub_mode, uint32_t value)
{
    uint8_t  byte_buf[4];   /* sp+0x00 — 字节对输出 */
    uint8_t  fmt_buf[16];   /* sp+0x04 — 格式化输出 */
    uint32_t sentinel;      /* sp+0x14 — 哨兵, 写0 */
    int16_t  sval;          /* 有符号半字值 */

    sentinel = 0;           /* sp+0x14 ← 0 (栈哨兵) */

    /* 门控: mode != 0x12 → 直接退出 */
    if (mode != 0x12) {
        return;
    }

    /* ---- sub_mode == 0xD: 写入 REG_110[4:6] ---- */
    if (sub_mode == 0xD) {
        sval = (int16_t)value;                       /* sxth r0, r5 */
        *(volatile uint16_t *)(REG_110 + 4) = (uint16_t)sval;  /* strh */

        /* sprintf(fmt_buf, "%d", sval) → func_12788(mode, 7, fmt_buf) */
        {
            int16_t v;
            v = *(volatile int16_t *)(REG_110 + 4);  /* ldrsh.w */
            func_16B18(fmt_buf, (const char *)0x0800FB40, (uint32_t)(int32_t)v);
            func_12788(mode, 7, fmt_buf);
        }

        /* 提取字节对: hi_byte, lo_byte */
        {
            uint16_t hw = *(volatile uint16_t *)(REG_110 + 4);
            byte_buf[0] = (uint8_t)(hw >> 8);        /* lsrs #8; strb */
            byte_buf[1] = (uint8_t)(hw);             /* ldrb [r0, #4] */

            func_BC2C(0x1E, byte_buf, 2);            /* 发送字节对 */
        }
        return;
    }

    /* ---- sub_mode == 0xE: 写入 REG_116[4:6] ---- */
    if (sub_mode == 0xE) {
        sval = (int16_t)value;
        *(volatile uint16_t *)(REG_116 + 4) = (uint16_t)sval;

        {
            int16_t v;
            v = *(volatile int16_t *)(REG_116 + 4);
            func_16B18(fmt_buf, (const char *)0x0800FB40, (uint32_t)(int32_t)v);
            func_12788(mode, 8, fmt_buf);
        }

        {
            uint16_t hw = *(volatile uint16_t *)(REG_116 + 4);
            byte_buf[0] = (uint8_t)(hw >> 8);
            byte_buf[1] = (uint8_t)(hw);

            func_BC2C(0x22, byte_buf, 2);
        }
        return;
    }

    /* 其他 sub_mode: 无操作 */
}
