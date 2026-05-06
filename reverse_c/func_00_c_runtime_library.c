/**
 * @file func_00_c_runtime_library.c
 * @brief C 运行时库函数 — Reset_Handler、异常向量、字符串操作、时间计算、64 位除法
 * @addr  0x08008188 - 0x0800842A (674 bytes, 14 个函数 + 字面量池)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 位于向量表 (0x08008000-0x08008188) 之后, 包含:
 *   - Reset_Handler (向量表[1]=0x080081E1) + Secondary_Entry 启动入口
 *   - 默认异常处理循环 (B . 死循环, 10 个)
 *   - 64 位非恢复除法 (func_08204)
 *   - 日历/时间字段计算 (func_08268)
 *   - 标准 C 字符串函数 (memcpy/strcat/strstr/strlen/strcmp/strncmp/strcpy/strtok)
 *   - 格式化辅助函数 (func_08410)
 *
 * 所有函数遵循 ARM AAPCS 调用约定 (参数 r0-r3, 返回 r0-r1).
 */

#include "stm32f4xx_hal.h"

/* ---- 外部依赖 ---- */
extern void __main(void);                    /* C 运行时初始化 (0x08008C98) */
extern void func_08904(void);               /* Seconds_To_Minutes_Seconds (0x08008904) */
extern void func_16B98(void);               /* 状态/格式化辅助 (0x08016B98) */
extern void func_08A2C(void);               /* 字符串格式化辅助 (0x08008A2C) */

/* ---- 字面量池常量 ---- */
#define INITIAL_SP      0x200065B8           /* 初始主堆栈指针 */
#define MAIN_ENTRY      0x08018525           /* Main_Init_Sequence 地址 (Thumb) */

/* ========================================================================
 * Reset_Handler (0x080081E0 - 0x080081E6, 6 bytes)
 *
 * 向量表项 [1] @ 0x08008004 = 0x080081E1 (Thumb 模式).
 * 先间接调用初始化函数, 再跳转到 Secondary_Entry.
 *
 * 指令序列:
 *   0x080081E0: LDR R0, [PC, #0x18]  → R0 = pool @ 0x080081FC
 *   0x080081E2: BLX R0                → 间接调用初始化函数
 *   0x080081E4: LDR R0, [PC, #0x18]  → R0 = 0x080081C9 (pool @ 0x08008200)
 *   0x080081E6: BX R0                 → 跳转到 Secondary_Entry
 * ======================================================================== */

/* ========================================================================
 * Secondary_Entry (0x080081C8 - 0x080081D4, 12 bytes)
 *
 * 由 Reset_Handler 尾调用到达. 设置 SP, 调用 C 运行时初始化, 跳转到主程序.
 *
 * 指令序列:
 *   0x080081C8: LDR.W SP, [PC, #0x10]  → SP = 0x200065B8 (pool @ 0x080081DC)
 *   0x080081CC: BL 0x08008C98           → __main (C 运行时初始化)
 *   0x080081D0: LDR R0, [PC, #0]       → R0 = 0x08018525 (pool @ 0x080081D4)
 *   0x080081D2: BX R0                   → 跳转 Main_Init_Sequence
 *
 * 字面量池:
 *   0x080081D4: 0x08018525 (Main_Init_Sequence 地址, bit[0]=1 → Thumb)
 *   0x080081D8: 0x8000F3AF (NOP.W placeholder)
 *   0x080081DC: 0x200065B8 (初始 SP)
 *
 * 注: 无数据段拷贝/BSS 清零, 由 __main (0x08008C98) 内部完成.
 * ======================================================================== */

/* ========================================================================
 * Default_Handler (0x080081E8 - 0x080081FA, 14 bytes)
 *
 * 10 个默认异常处理循环, 每个为 B . (自跳转死循环):
 *   0x080081E8: NMI_Handler
 *   0x080081EA: HardFault_Handler
 *   0x080081EC: MemManage_Handler
 *   0x080081EE: BusFault_Handler
 *   0x080081F0: UsageFault_Handler
 *   0x080081F2: (reserved 1)
 *   0x080081F4: (reserved 2)
 *   0x080081F6: (reserved 3)
 *   0x080081F8: (reserved 4)
 *   0x080081FA: (reserved 5)
 * ======================================================================== */

/* ========================================================================
 * func_08204 — 64 位非恢复除法 (0x08008204 - 0x08008262, 94 bytes)
 *
 * ARM EABI __aeabi_ldivmod 兼容接口:
 *   参数: r0:r1 = 被除数 (r0=lo, r1=hi), r2:r3 = 除数 (r2=lo, r3=hi)
 *   返回: r0:r1 = 商
 *
 * 算法: 非恢复除法, 迭代 64 次 (MOVS r4,#0x40).
 *   每次迭代: 被除数左移 1 位, 试减除数, 若借位则恢复.
 *   调用辅助函数 func_088BE (左移) 和 func_088A0 (右移/调整).
 *
 * 被 func_151 (Display_State_Controller) 调用用于除以 3600.
 * ======================================================================== */
uint64_t func_08204(uint32_t dividend_lo, uint32_t dividend_hi,
                    uint32_t divisor_lo,  uint32_t divisor_hi)
{
    /* PUSH.W {r4,r5,r6,r7,r8,r9,r10,r11,r12,lr} — 10 寄存器保存 (r4-r12+lr)
     * MOV r5,r0; MOVS r0,#0; MOV r10,r2; MOV r11,r3; MOV r8,r1
     * MOV r6,r0; MOV r9,r0; MOVS r4,#0x40 — 64 次迭代
     * 循环体:
     *   BL func_088BE(r0=r5,r1=r8,r2=r4) → 左移被除数
     *   SUBS r0,r3; SBCS r1,r11 → 试减除数
     *   BLO skip → 若 < 0, 跳过 (恢复)
     *   BL func_088A0 → 商位设置
     * 递减 r4; BGT loop
     * MOV r0,r9; MOV r1,r6 → 返回商
     */
    uint64_t dividend = ((uint64_t)dividend_hi << 32) | dividend_lo;
    uint64_t divisor  = ((uint64_t)divisor_hi  << 32) | divisor_lo;
    uint64_t quotient = 0;
    uint64_t remainder = 0;

    for (int i = 63; i >= 0; i--) {
        remainder = (remainder << 1) | ((dividend >> i) & 1);
        quotient <<= 1;
        if (remainder >= divisor) {
            remainder -= divisor;
            quotient |= 1;
        }
    }
    return quotient;
}

/* ========================================================================
 * func_08268 — 日历/时间字段计算器 (0x08008268 - 0x080082FC, 148 bytes)
 *
 * PUSH {r3,r4,r5,lr}; SUB SP 隐含.
 * 从结构体指针 (r0) 读取多个时间/计数字段, 执行多阶段 SDIV 除法链,
 * 查表累加, 最后调用 func_08904 (Seconds_To_Minutes_Seconds) 格式化输出.
 *
 * 参数: r0 = time_struct — 指向时间/日历数据结构的指针
 *       结构体布局 (从 LDRD/LDR 偏移推断):
 *         [0x00] = field_0 (32-bit, LDR r2,[r0])
 *         [0x04] = field_1 (32-bit, LDR r2,[r0,#4])
 *         [0x08] = field_2:field_3 (64-bit, LDRD r2,r1,[r0,#8])
 *         [0x10] = field_4:field_5 (64-bit, LDRD r3,r1,[r0,#0x10])
 *
 * 算法阶段:
 *   A. 从 field_2/field_3 计算偏移: (field_3-1)*24 + field_2 → *15 → +field_1 → *15 → +field_0
 *   B. 从 field_4/field_5 计算: field_5*3 + field_4 → SUB #0x348
 *   C. SDIV #48 取商 (30分钟单位?), 乘以魔数, 查表累加
 *   D. 余数 SDIV #12 取商/余 (月/日?), 查递减表 (元素 × 0x2A3=675)
 *   E. 调用 func_08904 → 格式化结果
 *
 * 用于将内部时间戳转换为显示就绪的 分钟:秒 格式.
 * ======================================================================== */
static uint32_t func_08268(void *time_struct)
{
    /* 0x08008268: PUSH {r3,r4,r5,lr}
     * LDRD r2,r1,[r0,#8] → 加载 field_2:field_3
     * SUBS r1,r1,#1 → field_3--
     * ADD r1,r1,r1,LSL#1 → r1 *= 3
     * ADD r1,r2,r1,LSL#3 → r1 = r2 + r1*8
     * LDR r2,[r0,#4] → 加载 field_1
     * RSB r1,r1,r1,LSL#4 → r1 *= 15
     * ADD r1,r2,r1,LSL#2 → r1 = r2 + r1*4
     * LDR r2,[r0] → 加载 field_0
     * RSB r1,r1,r1,LSL#4 → r1 *= 15
     * ADD r2,r2,r1,LSL#2 → r2 = r2 + r1*4 (累计值)
     * LDRD r3,r1,[r0,#0x10] → 加载 field_4:field_5
     * ADD r1,r1,r1,LSL#1 → r1 *= 3
     * ADD r1,r3,r1,LSL#2 → r1 = r3 + r1*4
     * MOVS r3,#0x30 → 除数 = 48
     * SUB r1,r1,#0x348 → 减 840
     * SDIV r4,r1,r3 → 商 = r1 / 48
     * LDR r5,[pc,#0x5C] → 魔数常量
     * MULS r4,r5,r4 → 商 × 魔数
     * ADD r2,r2,r4,LSL#7 → 累加
     * SDIV r4,r1,r3 → 再除 48
     * MLS r1,r3,r4,r1 → 余数 = r1 - r3*r4
     * STR r2,[sp] → 存储中间结果
     * CMP r1,#0x1A → >= 26?
     * BLT skip → 若 < 26, 跳
     * LDR r3,[pc,#0x48] → 调整常量
     * ADD r2,r3 → 调整
     * STR r2,[sp]
     * skip:
     * MOVS r2,#0xC → 除数 = 12
     * SDIV r3,r1,r2 → /12
     * LDR r4,[pc,#0x40] → 魔数 2
     * MULS r3,r4,r3
     * ADD r3,r4,r3,LSL#7 → 累加
     * STR r3,[sp]
     * SDIV r3,r1,r2 → /12
     * MLS r1,r2,r3,r1 → 余数
     * LDR r2,[pc,#0x30] → 递减查找表基址
     * loop:
     *   SUBS r1,#1
     *   MOVW r4,#0x2A3 → 675
     *   LDRB r3,[r2,r1] → 表[r1]
     *   MULS r3,r4,r3 → ×675
     *   ADD r3,r4,r3,LSL#7 → 累加
     *   STR r3,[sp]
     * CMP r1,#0; BGT loop
     * MOV r1,r0; MOV r0,sp; BL func_08904
     * LDR r0,[sp]; POP {r3,r4,r5,pc}
     */
    return 0;
}

/* ========================================================================
 * func_08310 — memcpy 内存块复制 (0x08008310 - 0x08008332, 34 bytes)
 *
 * 参数: r0 = dst, r1 = src, r2 = count
 * 返回: r0 = dst (未修改)
 *
 * 优化: 若 dst&src 均 4 字节对齐且 count>=4, 使用 LDM/STM 字传输.
 *       否则逐字节 LDRB/STRB 复制.
 *
 * 指令: ORR.W r3,r0,r1; LSLS r3,r3,#0x1E → 测试低 2 位 (对齐检查)
 *       BEQ → 走快速路径 (LDM r1!,{r3}; SUBS r2,#4; STM r0!,{r3})
 *       慢速路径: LDRB r3,[r1],#1; SUBS r2,#1; STRB r3,[r0],#1; BHS loop
 * ======================================================================== */
void *func_08310(void *dst, const void *src, size_t count)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;

    if ((((uintptr_t)dst | (uintptr_t)src) & 3) == 0) {
        uint32_t *dw = (uint32_t *)d;
        const uint32_t *sw = (const uint32_t *)s;
        while (count >= 4) {
            *dw++ = *sw++;
            count -= 4;
        }
        d = (uint8_t *)dw;
        s = (const uint8_t *)sw;
    }
    while (count--) {
        *d++ = *s++;
    }
    return dst;
}

/* ========================================================================
 * func_08334 — strcat 字符串拼接 (0x08008334 - 0x0800834A, 22 bytes)
 *
 * 参数: r0 = dst, r1 = src
 * 返回: r0 = dst
 *
 * 指令: SUBS r2,r0,#1; loop1: LDRB r3,[r2,#1]!; CMP r3,#0; BNE loop1
 *       loop2: LDRB r3,[r1],#1; STRB r3,[r2],#1; CMP r3,#0; BNE loop2; BX LR
 * ======================================================================== */
char *func_08334(char *dst, const char *src)
{
    char *d = dst - 1;
    while (*++d);
    while ((*d++ = *src++));
    return dst;
}

/* ========================================================================
 * func_0834C — strstr 子串搜索 (0x0800834C - 0x0800836E, 34 bytes)
 *
 * 参数: r0 = haystack, r1 = needle
 * 返回: r0 = 首次匹配位置 (或 NULL 变体)
 *
 * 指令: PUSH {r4,r5,lr}; MOV r5,r0; MOV r2,r5; MOV r3,r1
 *       loop: LDRB r0,[r2],#1; LDRB r4,[r3],#1
 *       CBZ r0 → check_r4; CMP r0,r4; BEQ loop (继续匹配)
 *       CBZ r4 → found; CMP r0,#0; BEQ not_found
 *       ADDS r5,#1 → 重试下一位置; B retry
 * ======================================================================== */
static char *func_0834C(const char *haystack, const char *needle)
{
    while (*haystack) {
        const char *h = haystack;
        const char *n = needle;
        while (*h && *n && *h == *n) {
            h++;
            n++;
        }
        if (!*n) return (char *)haystack;
        if (!*h) return NULL;
        haystack++;
    }
    return NULL;
}

/* ========================================================================
 * func_08370 — strlen 字符串长度 (0x08008370 - 0x0800837C, 12 bytes)
 *
 * 参数: r0 = str
 * 返回: r0 = 长度 (不含 '\\0')
 *
 * 指令: ADDS r2,r0,#1; loop: LDRB r1,[r0],#1; CMP r1,#0; BNE loop
 *       SUBS r0,r0,r2; BX LR
 * ======================================================================== */
size_t func_08370(const char *str)
{
    const char *s = str;
    while (*s) s++;
    return s - str;
}

/* ========================================================================
 * func_0837E — strcmp 字符串比较 (0x0800837E - 0x08008398, 26 bytes)
 *
 * 参数: r0 = s1, r1 = s2
 * 返回: r0 = (unsigned char)s1[i] - (unsigned char)s2[i] (首个差异)
 *
 * 指令: PUSH {r4,lr}; MOVS r2,#0
 *       loop: ADDS r2,#1; LDRB r3,[r0,r2]; LDRB r4,[r1,r2]
 *       CMP r3,r4; BNE done; CMP r3,#0; BNE loop
 *       done: UXTB r0,r3; UXTB r1,r4; SUBS r0,r0,r1; POP {r4,pc}
 *
 * 注: 注意索引从 1 开始 (r2 初始为 0, 先增后比较) — 跳过首字符?
 *     实际为 func_0837E 的变体实现, 与标准 strcmp 有微妙差异.
 * ======================================================================== */
int func_0837E(const char *s1, const char *s2)
{
    size_t i = 1;
    while (s1[i] && s2[i] && s1[i] == s2[i])
        i++;
    return (unsigned char)s1[i] - (unsigned char)s2[i];
}

/* ========================================================================
 * func_0839A — strcpy 字符串复制 (0x0800839A - 0x080083AA, 16 bytes)
 *
 * 参数: r0 = dst, r1 = src
 * 返回: r0 = dst (原始值, MOV r3,r0 保存)
 *
 * 指令: MOV r3,r0; loop: LDRB r2,[r1],#1; STRB r2,[r0],#1;
 *       CMP r2,#0; BNE loop; MOV r0,r3; BX LR
 * ======================================================================== */
char *func_0839A(char *dst, const char *src)
{
    char *orig = dst;
    while ((*dst++ = *src++));
    return orig;
}

/* ========================================================================
 * func_083AC — strncmp 定长字符串比较 (0x080083AC - 0x080083C8, 28 bytes)
 *
 * 参数: r0 = s1, r1 = s2, r2 = maxlen
 * 返回: r0 = 首个差异字符差值 (或 0 表示相等/提前遇到 \\0)
 *
 * 指令: PUSH {r4,r5,lr}; MOV r5,r0; MOVS r0,#0; MOV r3,r0
 *       loop: CMP r3,r2; BHS done → 达到 maxlen
 *       LDRB r4,[r5,r3]; LDRB r0,[r1,r3]
 *       SUBS r0,r4,r0; BNE done → 字符不同
 *       CMP r4,#0; BNE continue → s1 未结束
 *       done: POP {r4,r5,pc}
 *       continue: ADDS r3,#1; B loop
 * ======================================================================== */
int func_083AC(const char *s1, const char *s2, size_t maxlen)
{
    size_t i;
    for (i = 0; i < maxlen; i++) {
        int diff = s1[i] - s2[i];
        if (diff != 0) return diff;
        if (s1[i] == '\0') return 0;
    }
    return 0;
}

/* ========================================================================
 * func_083CC — strtok/字段令牌化 (0x080083CC - 0x08008408, 60 bytes)
 *
 * 参数: r0 = str (NULL=继续上次), r1 = delimiters
 * 返回: r0 = 令牌指针 (或 NULL)
 *
 * 指令: PUSH {r4,r5,r6,lr}
 *       LDR r5,[pc,#0x3C] → 静态状态指针 (存储上次位置)
 *       CBNZ r0 → use_r0; LDR r0,[r5] (使用保存的位置)
 *       use_r0: MOVS r4,#0
 *       扫描循环: 跳过前导分隔符, 找令牌起始
 *       令牌扫描: 找分隔符或 \\0
 *       存储下一位置到 [r5]
 *       返回 r4 (令牌起始 或 NULL)
 * ======================================================================== */
static char *func_083CC(char *str, const char *delim)
{
    static char *saved;
    if (str) saved = str;
    if (!saved) return NULL;

    /* 跳过前导分隔符 */
    while (*saved) {
        const char *d = delim;
        while (*d && *d != *saved) d++;
        if (!*d) break;  /* 非分隔符 — 令牌开始 */
        saved++;
    }
    if (!*saved) return NULL;

    char *token = saved;
    /* 扫描令牌结束 */
    while (*saved) {
        const char *d = delim;
        while (*d && *d != *saved) d++;
        if (*d) { *saved++ = '\0'; break; }
        saved++;
    }
    return token;
}

/* ========================================================================
 * func_08410 — 格式化辅助调用 (0x08008410 - 0x08008428, 24 bytes)
 *
 * PUSH {r4,r5,r6,lr}; MOV r6,r0
 * BL func_16B98 → 获取格式化上下文
 * LDR r4,[r0]; MOV r5,r0
 * MOVS r2,#0xA (基数 10); MOVS r1,#0
 * MOV r0,r6; BL func_08A2C → 数字格式化
 * STR r4,[r5] → 恢复上下文
 * POP {r4,r5,r6,pc}
 *
 * 包装 func_16B98 和 func_08A2C 的数字格式化叶函数.
 * ======================================================================== */
static void func_08410(uint32_t value)
{
    /* 获取格式化上下文 → 格式化十进制数字 → 恢复上下文 */
}

/*
 * 内存布局:
 *   0x08008188 - 0x080081C8: NOP 填充 + 字面量池碎片 (40B)
 *   0x080081C8 - 0x080081D4: Reset_Handler (12B)
 *   0x080081D4 - 0x080081E0: Reset_Handler 字面量池 (12B)
 *   0x080081E0 - 0x080081E8: Secondary_Entry (8B)
 *   0x080081E8 - 0x080081FA: Default_Handler × 10 (20B, B . 循环)
 *   0x080081FC - 0x08008204: 字面量池 (8B, Secondary_Entry 函数指针)
 *   0x08008204 - 0x08008262: func_08204 (94B, 64-bit 除法)
 *   0x08008266:              NOP 对齐 (2B)
 *   0x08008268 - 0x080082FC: func_08268 (148B, 日历计算)
 *   0x080082FE:              NOP 对齐 (2B)
 *   0x08008300 - 0x08008310: func_08268 字面量池 (16B)
 *   0x08008310 - 0x08008332: func_08310 memcpy (34B)
 *   0x08008334 - 0x0800834A: func_08334 strcat (22B)
 *   0x0800834C - 0x0800836E: func_0834C strstr (34B)
 *   0x08008370 - 0x0800837C: func_08370 strlen (12B)
 *   0x0800837E - 0x08008398: func_0837E strcmp (26B)
 *   0x0800839A - 0x080083AA: func_0839A strcpy (16B)
 *   0x080083AC - 0x080083C8: func_083AC strncmp (28B)
 *   0x080083CA:              NOP 对齐 (2B)
 *   0x080083CC - 0x08008408: func_083CC strtok (60B)
 *   0x0800840A:              NOP 对齐 (2B)
 *   0x0800840C - 0x08008410: 字面量池 (4B)
 *   0x08008410 - 0x08008428: func_08410 (24B)
 *
 *   0x0800842A: func_01_aeabi_fpu_runtime.c 开始
 *
 * 注: func_0837E 的索引从 1 开始 (跳过首字符比较),
 *     这可能是 strcmp 的特定变体或是编译优化导致的偏移.
 *     func_08268 的日历算法使用 48/12/26/675 等非标准除数,
 *     表明是专有显示时间格式而非标准 UNIX 时间戳转换.
 */
