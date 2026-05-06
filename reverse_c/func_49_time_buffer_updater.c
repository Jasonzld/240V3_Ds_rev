/**
 * @file func_49_time_buffer_updater.c
 * @brief 函数: Time_Buffer_Updater — 更新 BUF_69C 时间字段并通过帧发送
 * DISASSEMBLY-TRACED. Verified against Capstone.
 * @addr  0x0800F9C4 - 0x0800FAA8 (228 bytes, ~31 指令)
 *         字面量池 0x0800FAA4 (4 bytes: BUF_69C 基址)
 *
 * 根据模式/子模式更新 BUF_69C 时间缓冲区的指定字段，然后调用
 * func_127DA() 发送完整的时间数据帧。
 *
 * 门控: 仅当 mode ∈ {5, 0x25} 时执行操作，否则直接退出。
 *
 * 参数:
 *   r0 = mode       — 页面模式 (仅 5 或 0x25 有效)
 *   r1 = sub_mode   — 字段选择 (4=年, 5=月, 6=日, 7=星期, 8=分钟)
 *   r2 = value      — 写入的值
 *
 * 调用:
 *   func_127DA() @ 0x080127DA — 时间数据帧发送
 *
 * 写入操作:
 *   sub_mode == 4: BUF_69C[0:2] = value + 0x15  (半字, 年份)
 *   sub_mode == 5: BUF_69C[2]   = value + 1     (字节, 月份)
 *   sub_mode == 6: BUF_69C[3]   = value + 1     (字节, 日期)
 *   sub_mode == 7: BUF_69C[4]   = value         (字节, 星期)
 *   sub_mode == 8: BUF_69C[6]   = value         (字节, 分钟)
 *
 * func_127DA 参数:
 *   r0 = BUF_69C[7] — 秒
 *   r1 = BUF_69C[6] — 分钟
 *   r2 = BUF_69C[4] — 星期
 *   r3 = BUF_69C[3] — 日期
 *   [sp+0] = BUF_69C[5] — 小时
 *   [sp+4] = BUF_69C[2] — 月份
 *   [sp+8] = BUF_69C[0] — 年份低字节
 */

#include "stm32f4xx_hal.h"

#define BUF_69C     ((volatile uint8_t *)0x2000069C)

extern void func_127DA(uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3,
                       uint32_t sp0, uint32_t sp4, uint32_t sp8);

/* ================================================================
 * Time_Buffer_Updater() @ 0x0800F9C4
 *   push {r1, r2, r3, r4, r5, r6, r7, lr}
 *   r6=r0, r5=r1, r4=r2
 *   栈传参: func_127DA 通过 strd+str 在栈上构建 3 个额外参数
 * ================================================================ */
void Time_Buffer_Updater(uint32_t mode, uint32_t sub_mode, uint32_t value)
{
    /* 门控: mode ∉ {5, 0x25} → 直接退出 */
    if (mode != 5 && mode != 0x25) {
        return;
    }

    /* ---- sub_mode == 4: 年份 (半字写入) ---- */
    if (sub_mode == 4) {
        BUF_69C[0] = (uint8_t)(value + 0x15);     /* strh: 半字写入偏移 0 */
        BUF_69C[1] = (uint8_t)((value + 0x15) >> 8);

        func_127DA(BUF_69C[7],                     /* r0 = 秒 */
                   BUF_69C[6],                     /* r1 = 分钟 */
                   BUF_69C[4],                     /* r2 = 星期 */
                   BUF_69C[3],                     /* r3 = 日期 */
                   BUF_69C[5],                     /* [sp+0] = 小时 */
                   BUF_69C[2],                     /* [sp+4] = 月份 */
                   BUF_69C[0]);                    /* [sp+8] = 年份低字节 */
        return;
    }

    /* ---- sub_mode == 5: 月份 ---- */
    if (sub_mode == 5) {
        BUF_69C[2] = (uint8_t)(value + 1);

        func_127DA(BUF_69C[7],
                   BUF_69C[6],
                   BUF_69C[4],
                   BUF_69C[3],
                   BUF_69C[5],
                   BUF_69C[2],
                   BUF_69C[0]);
        return;
    }

    /* ---- sub_mode == 6: 日期 ---- */
    if (sub_mode == 6) {
        BUF_69C[3] = (uint8_t)(value + 1);

        func_127DA(BUF_69C[7],
                   BUF_69C[6],
                   BUF_69C[4],
                   BUF_69C[3],
                   BUF_69C[5],
                   BUF_69C[2],
                   BUF_69C[0]);
        return;
    }

    /* ---- sub_mode == 7: 星期 ---- */
    if (sub_mode == 7) {
        BUF_69C[4] = (uint8_t)value;

        func_127DA(BUF_69C[7],
                   BUF_69C[6],
                   BUF_69C[4],
                   BUF_69C[3],
                   BUF_69C[5],
                   BUF_69C[2],
                   BUF_69C[0]);
        return;
    }

    /* ---- sub_mode == 8: 分钟 ---- */
    if (sub_mode == 8) {
        BUF_69C[6] = (uint8_t)value;

        func_127DA(BUF_69C[7],
                   BUF_69C[6],
                   BUF_69C[4],
                   BUF_69C[3],
                   BUF_69C[5],
                   BUF_69C[2],
                   BUF_69C[0]);
        return;
    }

    /* 其他 sub_mode: 直接退出 (无操作) */
}
