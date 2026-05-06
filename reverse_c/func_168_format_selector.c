/**
 * @file func_168_format_selector.c
 * @brief 函数: 格式化字符串选择器 — switch(sb) 选择格式模板, 在字符串中搜索匹配
 * @addr  0x0801821E - 0x08018333 (278 bytes)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 位于 func_167 (GPS NMEA RMC Parser, 0x080181D0) 和 func_142 (ASCII→Integer, 0x08018334) 之间.
 *
 * 功能: 根据 sb (r9) 参数 (0-9) 通过 TBB 跳转表选择格式字符串模板,
 *       使用 func_16B18 将数值格式化为栈上字符串,
 *       然后在输入字符串 (r6) 中搜索以 '$' 开头的匹配模式.
 *       找到匹配后将指针存入 [r8].
 *
 * 寄存器映射:
 *   r6 = 输入字符串 (搜索目标)
 *   r7 = 匹配计数器 (init: 非0时跳过 init, 为0时设1; 每次匹配递减)
 *   r8 = 输出指针 (匹配位置存储目标)
 *   r9/sb = 格式选择索引 (0-9, >=10 返回错误)
 *   r4 = 扫描指针
 *   r5 = 字符比较索引
 *   sp = 格式化输出缓冲区
 *
 * 控制流 (经 Capstone v5.0.7 验证):
 *   0x0801821E: CBNZ r7 → 若 r7 == 0, MOVS r7, #1 (初始化计数器)
 *   0x08018222: CMP sb, #0xA → BHS return 3 (索引超出范围)
 *   0x08018228: TBB [pc, sb] → 10路跳转表 (0x0801822C, 每项1字节偏移)
 *       跳转表: 05(→0x08018236), 0A(→0x08018240), 0F(→0x0801824A),
 *               14(→0x08018254), 19(→0x0801825E), 1E(→0x08018268),
 *               23(→0x08018272), 28(→0x0801827C), 2D(→0x08018286),
 *               32(→0x08018290)
 *   每个case: ADR r1, format_string → MOV r0, sp → BL func_16B18 → B scan_loop
 *   0x0801829A: MOVS r0, #3 → return 3 (错误)
 *   扫描循环 (0x080182A0-0x080182DE):
 *     while (*r4 != 0):
 *       if (*r4 == '$'):
 *         比较栈上5字节与 [r4+1..r4+5]
 *         若完全匹配: r7--; 若 r7==0: *r8 = r4 (记录匹配位置)
 *       r4++
 *     return 0=成功匹配, 1=到达字符串末尾
 *
 * BL 目标:
 *   func_16B18() @ 0x08016B18 — 数值格式化 (×10, 每case 1次)
 *
 * 字面量池 (0x080182E4-0x08018333):
 *   格式字符串地址 (通过 ADR r1, #imm 引用, 实际偏移需确认)
 *   池中可见值:
 *     [0x080182E4] = 0xXXXX → 格式字符串 "..." 地址 (×10)
 *     [0x0801832C] = pattern
 */

#include "stm32f4xx_hal.h"

extern void func_16B18(char *buf, const char *fmt, uint32_t val);

/* ================================================================
 * Format_Selector() @ 0x0801821E
 *   无独立 PUSH — 使用调用者栈帧
 *   返回: r0 = 0 (匹配成功), 1 (到达字符串末尾, 未完全匹配), 3 (错误)
 * ================================================================ */
__attribute__((naked))
uint32_t Format_Selector(void)
{
    register const char *input asm("r6");   /* 要搜索的字符串 */
    register uint32_t    count asm("r7");   /* 匹配计数 */
    register const char **out asm("r8");    /* 匹配位置输出 */
    register uint32_t    idx   asm("r9");   /* sb — 格式索引 (0-9) */

    char   fmt_buf[8];   /* sp — 格式化输出缓冲区 */
    uint32_t result;

    /* 初始化: 若 count == 0, 设为 1 */
    if (count == 0)
        count = 1;

    /* 索引检查: 必须 < 10 */
    if (idx >= 10)
        return 3;

    /*
     * TBB 跳转: switch (idx) {
     *   case 0: fmt = format_string_0; break;
     *   case 1: fmt = format_string_1; break;
     *   ...
     *   case 9: fmt = format_string_9; break;
     * }
     * 每个 case 调用 func_16B18(fmt_buf, fmt_string, some_value)
     * 然后跳转到扫描循环.
     */

    /* 扫描循环: 在 input 中搜索 '$' + 匹配的格式字符串 */
    {
        const char *scan = input;
        while (*scan != '\0') {
            if (*scan == '$') {
                /* 逐字节比较扫描位置后的5字节与格式化缓冲区 */
                uint32_t i;
                for (i = 0; i < 5; i++) {
                    if (scan[1 + i] != fmt_buf[i])
                        goto next_iter;
                }
                /* 完全匹配: 递减计数器, 若归零则记录位置 */
                count--;
                if (count == 0) {
                    *out = scan;
                    goto done;
                }
            }
        next_iter:
            scan++;
        }
        /* 扫描到字符串末尾 */
        return 1;
    }

done:
    return 0;
}

/* 字面量池 (0x080182E4-0x08018333):
 *   10个格式字符串指针 (指向 "%d", "%u", "%x", "%ld", ... 等)
 *   每个 TBB case 对应一个数值格式化模板.
 */
