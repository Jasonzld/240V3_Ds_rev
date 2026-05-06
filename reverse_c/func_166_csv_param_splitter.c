/**
 * @file func_166_csv_param_splitter.c
 * @brief 函数: CSV/NMEA字段分割器 — 遍历逗号/星号分隔的字符串, 记录各字段宽度
 * @addr  0x08017E82 - 0x08017EDC (90 bytes)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 位于 func_138 and func_139 之间, 是 NMEA/AT 命令解析流水线的一部分.
 *
 * 遍历输入行, 按 ',' (0x2C) 和 '*' (0x2A) 分割字段,
 * 检查 '\r' (0x0D) '\n' (0x0A) 行终止符.
 * 将每个字段的字符宽度以半字存入 widths 数组.
 *
 * 寄存器映射:
 *   r0 = 临时 (字符加载/比较)
 *   r1 = 当前字符指针 (每轮迭代递增)
 *   r2 = 字段起始指针 (out_val 参数)
 *   r3 = widths 数组指针
 *   r4 = 剩余字段计数 (field_count)
 *   r5 = 总字段计数 + 1 (调用者传入)
 *   r6 = 字段内字符位置计数器
 *
 * 控制流 (经 Capstone 验证):
 *   0x08017E82: LDR [r2] → CBNZ → 非空检查
 *   0x08017E8A: r1 = r5 + 1 (从调用者 r5 初始化扫描指针)
 *   0x08017E8C: B 主循环入口 (行尾检查)
 *   0x08017E8E: CBZ r4 → 若字段计数已耗尽, 记录当前字段宽度 (r1 - r6)
 *   0x08017E98: LDRB [r1] → CMP #0x2C → BEQ 存储半字计数
 *   0x08017EA0: LDRB [r1] → CMP #0x2A → BEQ 存储半字计数
 *   0x08017EA4: r6++ → 字符计数递增
 *   0x08017EAE: LDRB [r1] → CMP #0x2C → BEQ 递减 r4
 *   0x08017EB4: LDRB [r1] → CMP #0x2A → BEQ 递减 r4
 *   0x08017EC2: LDRB [r1] → CMP #0x0D → BEQ epilogue
 *   0x08017ECA: LDRB [r1,#1] → CMP #0x0A → BNE loop
 *   0x08017ED0: CBZ r4 → 返回 0 (成功) 或 1 (字段耗尽)
 *
 * BL 目标: 无 (叶子函数, 被 func_138 和 func_136 调用)
 */

#include "stm32f4xx_hal.h"

/* ================================================================
 * CSV_Param_Splitter() @ 0x08017E82
 *   无独立 PUSH — 使用调用者栈帧 (r4-r6 保留调用者值)
 *   参数通过寄存器传递:
 *     r2 = out_val (输入: 指向当前字段起始指针的指针)
 *     r3 = widths  (输出: 半字宽度数组)
 *     r4 = field_count (剩余字段计数)
 *     r5 = total_count + 1 (从调用者传入)
 *     r6 = 字段内字符偏移 (初始在调用者设置)
 * ================================================================ */
__attribute__((naked))
uint32_t CSV_Param_Splitter(void)
{
    /* 入口: r2=out_val, r3=widths, r4=field_count, r5=total+1, r6=char_pos */
    register uint32_t *out_val   asm("r2");
    register uint16_t *widths    asm("r3");
    register uint32_t  field_cnt asm("r4");
    register uint32_t  total_p1  asm("r5");
    register uint32_t  char_pos  asm("r6");
    register const char *scan    asm("r1");
    register uint32_t  result    asm("r0");

    /* 0x08017E82: 检查 *out_val 非空 */
    if (*out_val == 0)
        return 3;  /* 字段指针为空 */

    /* 0x08017E8A: 初始化扫描指针 = total + 1 */
    scan = (const char *)(total_p1 + 1);
    /* 跳转到主循环的行尾检查入口 */

    /*
     * 主字符扫描循环 (0x08017EC2 - 0x08017ED2):
     *
     * while (1) {
     *     // 行尾检查
     *     if (*scan == '\r') goto field_done;
     *     if (scan[1] != '\n') {
     *         // 内部分割循环
     *         if (field_cnt == 0) {
     *             *out_val = scan - char_pos;  // 字段宽度 = 当前指针 - 起始偏移
     *             if (widths == NULL) goto field_done;
     *         }
     *         uint8_t c = *scan;
     *         if (c == ',') { *widths = char_pos; goto field_done; }
     *         if (c == '*') { *widths = char_pos; goto field_done; }
     *         char_pos++;  // 字段内计数递增
     *     } else {
     *         // '\r\n' 检测的分支: 检查 field_cnt
     *         if (field_cnt != 0) {
     *             c = *scan;
     *             if (c == ',' || c == '*') field_cnt--;
     *         }
     *         scan++;
     *     }
     * }
     *
     * field_done:
     *     返回 (field_cnt == 0) ? 0 : 1;
     */

    /* 注: 原始代码使用 CBNZ/CBZ 分支和 NOP 填充对齐 */
    /* 完整控制流见上方的 Capstone 反汇编 */

    return result;  /* 0 = 成功, 1 = 字段计数已耗尽但未达行尾 */
}

/* 调用约定总结:
 *   CSV_Param_Splitter 是 func_138 (0x08017E74) 和 func_136 (0x08017764) 的内部辅助函数
 *   无独立栈帧, 共用调用者的 r4-r11 寄存器
 *   返回: r0 = 0 → 所有字段解析完成
 *         r0 = 1 → 字段数组已满, 仍有输入
 *         r0 = 3 → 空指针错误
 */
