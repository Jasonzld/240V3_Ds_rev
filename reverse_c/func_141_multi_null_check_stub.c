/**
 * @file func_141_multi_null_check_stub.c
 * @brief 函数: 多参数空值检查桩 — 检查指针/计数，成功则直接跳转到下游 NMEA 解析器
 * DISASSEMBLY-TRACED. Verified against Capstone.
 * @addr  0x08018204 - 0x0801821E (26 bytes, 含字面量池)
 *
 * Prologue (仅入口处执行一次):
 *   push.w {r2, r3, r4, r5, r6, r7, r8, sb, sl, lr}
 *   !! 注意: r2, r3 是 AAPCS caller-saved 寄存器，此处非常规压栈 !!
 *   !!       因为此函数是 GPS NMEA 解析器 (0x0801821E) 的 guard + 栈帧设置器 !!
 *
 * 参数保存 (到 callee-saved 寄存器，供下游 GpsNmeaParser 直接使用):
 *   r0=ptr   -> r6
 *   r1=arg1  -> sb (r9)
 *   r2=arg2  -> r7
 *   r3=count -> r8
 *
 * 逻辑:
 *   1. 若 ptr(r6) == NULL   -> 返回 3 (错误)
 *   2. 若 count(r8) == 0    -> 返回 3 (错误)
 *   3. 若两者都有效         -> bne 跳转到 0x0801821E (GpsNmeaParser)
 *                              !!! 不弹出栈帧，不返回到调用者 !!!
 *                              !!! GpsNmeaParser 没有独立 prologue，依赖此栈帧 !!!
 *
 * Epilogue (仅错误路径执行):
 *   pop.w {r2, r3, r4, r5, r6, r7, r8, sb, sl, pc} ; r0=3
 */

#include "stm32f4xx_hal.h"

/*
 * 注意: 此函数使用非标准调用约定，无法用纯 C 准确表达。
 * 以下为语义等价模型：guard 检查失败返回 3，成功则执行 GpsNmeaParser 且不返回。
 * 实际二进制: 成功路径是 bne 直接跳转到 0x0801821E，不是函数调用。
 */
extern void GpsNmeaParser(void *buf, uint32_t arg1, uint32_t arg2, uint32_t count)
    __attribute__((noreturn));  /* @ 0x0801821E — 无独立 prologue */

uint32_t Multi_Null_Check(void *ptr, uint32_t arg1, uint32_t arg2, uint32_t count)
{
    if (ptr == NULL || count == 0) {
        return 3;   /* 错误: 空指针或零计数 */
    }

    /*
     * 成功路径 — 实际二进制在此处执行 bne 0x0801821E:
     *   栈上保留 push 的 {r2-r8,sb,sl,lr} 不弹出
     *   r6=ptr, sb=arg1, r7=arg2, r8=count 已就位
     *   GpsNmeaParser 从 cbnz r7 开始执行，无自己的 prologue
     *
     * C 模型: 作为 noreturn 尾调用来近似表达"不返回"的语义。
     */
    GpsNmeaParser(ptr, arg1, arg2, count);
}
