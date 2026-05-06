/**
 * @file func_46_bcd_time_encoder.c
 * @brief 函数: BCD_Time_Encoder — BCD 解码并写入时间缓冲区
 * DISASSEMBLY-TRACED. Verified against Capstone.
 * @addr  0x0800E180 - 0x0800E234 (180 bytes, 含字面量池)
 *         字面量池 0x0800E230-0x0800E234 (4 bytes)
 *
 * 将 7 个参数 (BCD 编码的日期时间值) 解码为二进制并写入
 * BUF_69C (0x2000069C) 的连续偏移位置。
 *
 * 参数:
 *   r0 = 年份 (半字, BCD, → offset 0)
 *   r1 = 月份 (BCD, → offset 2)
 *   r2 = 小时 (BCD, → offset 5)
 *   r3 = 日期 (BCD, → offset 3)
 *   [sp+0x14+0] = 星期 (BCD, → offset 4)
 *   [sp+0x14+4] = 分钟 (BCD, → offset 6)
 *   [sp+0x14+8] = 秒 (BCD, → offset 7)
 *
 * BCD 解码算法 (每个参数):
 *   val = (param & 0xF) + ((param >> 4) & 0xFF) * 10
 *   例: 0x59 → (0x59 & 0xF) + ((0x59 >> 4) & 0xFF) * 10 = 9 + 5 * 10 = 59
 *
 * 注: r0 结果以半字 (strh) 存储, 其余均为字节 (strb).
 *     参数存储顺序与寄存器顺序不完全一致:
 *     r2(小时)写入 offset 5, r3(日期)写入 offset 3 — 两参数在结构体中位置互换。
 *
 * 字面量池:
 *   0x0800E230: 0x2000069C (BUF_69C 基址)
 */

#include "stm32f4xx_hal.h"

#define BUF_69C     ((volatile uint8_t *)0x2000069C)

/* ================================================================
 * BCD_Time_Encoder() @ 0x0800E180
 *   push {r4, r5, r6, r7, lr}
 *   栈传参: ldm r4!, {r4,r5,r6} (3 个栈参数)
 *
 * BCD 展开: nibble_lo + nibble_hi * 10
 *   and #0xf → 低半字节
 *   ubfx #4,#8 → 高半字节
 *   add ip, ip, ip, lsl #2 → ip *= 5
 *   add r7, r7, ip, lsl #1 → r7 += ip*10
 * ================================================================ */
void BCD_Time_Encoder(uint32_t year, uint32_t month, uint32_t hour,
                      uint32_t day, uint32_t weekday, uint32_t minute, uint32_t second)
{
    uint32_t tmp;       /* ip */
    uint32_t result;    /* r7 */

    /* 栈参数 weekday/minute/second 由 ARM EABI 自动处理
     * (第5-7个参数 → [sp+0], [sp+4], [sp+8] on entry,
     *  函数入口 push {r4,r5,r6,r7,lr} 后位于 sp+0x14/0x18/0x1C) */

    /* ---- r0 = 年份 → BUF_69C[0:2] (半字) ---- */
    result   = year & 0xF;                              /* 0x0800E186: and r7,r0,#0xf */
    tmp      = (year >> 4) & 0xFF;                       /* 0x0800E18A: ubfx ip,r0,#4,#8 */
    tmp      = tmp + tmp * 4;                            /* 0x0800E18E: add.w ip,ip,ip,lsl#2 → ip *= 5 */
    result  += tmp * 2;                                  /* 0x0800E192: add.w r7,r7,ip,lsl#1 → r7 += ip*10 */
    *(volatile uint16_t *)(BUF_69C + 0) = (uint16_t)result; /* 0x0800E196-19A: ldr ip; strh r7,[ip] */

    /* ---- r1 = 月份 → BUF_69C[2] (字节) ---- */
    result   = month & 0xF;                              /* 0x0800E19E: and r7,r1,#0xf */
    tmp      = (month >> 4) & 0xFF;                      /* 0x0800E1A2: asr.w ip,r1,#4 */
    tmp      = tmp + tmp * 4;                            /* 0x0800E1A6: add.w ip,ip,ip,lsl#2 */
    result  += tmp * 2;                                  /* 0x0800E1AA: add.w r7,r7,ip,lsl#1 */
    BUF_69C[2] = (uint8_t)result;                        /* 0x0800E1AE-1B2: ldr ip; strb.w r7,[ip,#2] */

    /* ---- r3 = 日期 → BUF_69C[3] (字节) [注: r3 在 r2 之前处理] ---- */
    result   = day & 0xF;                                /* 0x0800E1CE: and r7,r3,#0xf */
    tmp      = (day >> 4) & 0xFF;                        /* 0x0800E1D2: asr.w ip,r3,#4 */
    tmp      = tmp + tmp * 4;                            /* 0x0800E1D6: add.w ip,ip,ip,lsl#2 */
    result  += tmp * 2;                                  /* 0x0800E1DA: add.w r7,r7,ip,lsl#1 */
    BUF_69C[3] = (uint8_t)result;                        /* 0x0800E1DE-1E2: ldr ip; strb.w r7,[ip,#3] */

    /* ---- r4 = 星期 → BUF_69C[4] (字节) ---- */
    result   = weekday & 0xF;                            /* 0x0800E1E6: and r7,r4,#0xf */
    tmp      = (weekday >> 4) & 0xFF;                    /* 0x0800E1EA: asr.w ip,r4,#4 */
    tmp      = tmp + tmp * 4;                            /* 0x0800E1EE: add.w ip,ip,ip,lsl#2 */
    result  += tmp * 2;                                  /* 0x0800E1F2: add.w r7,r7,ip,lsl#1 */
    BUF_69C[4] = (uint8_t)result;                        /* 0x0800E1F6-1FA: ldr ip; strb.w r7,[ip,#4] */

    /* ---- r2 = 小时 → BUF_69C[5] (字节) ---- */
    result   = hour & 0xF;                               /* 0x0800E1B6: and r7,r2,#0xf */
    tmp      = (hour >> 4) & 0xFF;                       /* 0x0800E1BA: asr.w ip,r2,#4 */
    tmp      = tmp + tmp * 4;                            /* 0x0800E1BE: add.w ip,ip,ip,lsl#2 */
    result  += tmp * 2;                                  /* 0x0800E1C2: add.w r7,r7,ip,lsl#1 */
    BUF_69C[5] = (uint8_t)result;                        /* 0x0800E1C6-1CA: ldr ip; strb.w r7,[ip,#5] */

    /* ---- r5 = 分钟 → BUF_69C[6] (字节) ---- */
    result   = minute & 0xF;                             /* 0x0800E1FE: and r7,r5,#0xf */
    tmp      = (minute >> 4) & 0xFF;                     /* 0x0800E202: asr.w ip,r5,#4 */
    tmp      = tmp + tmp * 4;                            /* 0x0800E206: add.w ip,ip,ip,lsl#2 */
    result  += tmp * 2;                                  /* 0x0800E20A: add.w r7,r7,ip,lsl#1 */
    BUF_69C[6] = (uint8_t)result;                        /* 0x0800E20E-212: ldr ip; strb.w r7,[ip,#6] */

    /* ---- r6 = 秒 → BUF_69C[7] (字节) ---- */
    result   = second & 0xF;                             /* 0x0800E216: and r7,r6,#0xf */
    tmp      = (second >> 4) & 0xFF;                     /* 0x0800E21A: asr.w ip,r6,#4 */
    tmp      = tmp + tmp * 4;                            /* 0x0800E21E: add.w ip,ip,ip,lsl#2 */
    result  += tmp * 2;                                  /* 0x0800E222: add.w r7,r7,ip,lsl#1 */
    BUF_69C[7] = (uint8_t)result;                        /* 0x0800E226-22A: ldr ip; strb.w r7,[ip,#7] */
}
