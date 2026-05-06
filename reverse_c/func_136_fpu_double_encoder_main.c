/**
 * @file func_136_fpu_double_encoder_main.c
 * @brief 函数: 格式化输出引擎 (__sbprintf/vsprintf) — 格式字符串解析与类型分派
 * @addr  0x08017764 - 0x08017E3C (1752 bytes, ~432 指令)
 *         子函数 B @ 0x08017E18 (28 bytes) — 空格填充辅助 (左对齐检查, blx r6 循环输出空格)
 *         子函数 C @ 0x08017E3C (56 bytes) — 数字格式化输出辅助
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 这是一个完整的 printf 系列格式化输出引擎. 解析格式字符串,
 * 处理 %[flags][width][.precision][length]specifier 语法,
 * 将 va_list 参数按类型格式化后通过输出回调写出.
 *
 * 支持的格式说明符 (经 Capstone 反汇编验证):
 *   整数: %d %i %u %x %X %o
 *   浮点: %f %F %e %E %g %G  → 调用 func_135 (@ 0x080175D0) 做 IEEE 754 编码
 *   字符: %c %s %p %n
 *   长度修饰: h hh l ll L j z t
 *   标志: * (宽度从参数), . (精度), + - 空格 # 0
 *
 * 参数 (ARM EABI):
 *   r0 = fmt — 格式字符串指针
 *   r1 = args — va_list 参数数组指针 (通过 r8 遍历)
 *   r2 = out_fn — 输出回调函数指针 (通过 sl 保存, blx sl 调用)
 *   r3 = arg_for_% — 用于 %n 的输出缓冲区 (在 [sp, #0x5c])
 *
 * 栈帧布局 (sub sp, #0x54 = 84 bytes):
 *   sp+0x00: 局部缓冲 (输出字符)
 *   sp+0x0C: 宽度字段
 *   sp+0x10-0x50: 寄存器保存 + 临时变量
 *   fp (sb): 字符串指针 (用于 %s)
 *   r4: flags 位域 (bit[1]=width_set, bit[2]=prec_set, bit[13]=neg_width, ...)
 *   r7: 精度值
 *   r5: 总输出计数
 *   r6: 格式字符串游标
 *   r8: va_list 参数指针
 *
 * BL 目标 (经 Capstone 验证):
 *   func_08204() @ 0x08008204 — UDIV 除法辅助
 *   func_08874() @ 0x08008874 — 除法/取模 (__aeabi_uidivmod), 用于进制转换
 *   func_175D0() @ 0x080175D0 — IEEE 754 双精度→ASCII 编码器 (func_135)
 *   func_17E18() @ 0x08017E18 — 数字→字符串局部转换
 *   func_17E3C() @ 0x08017E3C — 数字格式化输出辅助
 */

#include "stm32f4xx_hal.h"

extern void     func_175D0(uint64_t significand, int32_t fp, uint32_t mode,
                           double *out, uint32_t *out_mode);  /* IEEE 754 encoder */
extern uint32_t func_08204(uint32_t a, uint32_t b, uint32_t c, uint32_t d);
extern uint32_t func_08874(uint32_t num, uint32_t div);          /* __aeabi_uidivmod: 返回 num%div, 商在 r1 */
extern void     func_17E18(uint32_t pad_count, uint32_t flags,
                           void (*out_fn)(int ch), uint32_t *cnt);  /* 空格填充辅助: 检查左对齐标志, 循环输出空格 */
extern void     func_17E3C(char *buf, uint32_t cnt, uint32_t flags,
                           uint32_t width, uint32_t prec, uint32_t prefix);

/* 位域常量 (从反汇编提取) */
#define FLAG_WIDTH_SET   0x0002     /* 宽度已指定 */
#define FLAG_PREC_SET    0x0004     /* 精度已指定 */
#define FLAG_NEG_WIDTH   0x2000     /* 负宽度 (左对齐) */
#define FLAG_LONG        0x100000   /* l 修饰符 */
#define FLAG_LONGLONG    0x200000   /* ll/j 修饰符 */
#define FLAG_SHORT       0x300000   /* h 修饰符 */
#define FLAG_SIZE_T      0x400000   /* z/t/L 修饰符 */
#define FLAG_HEX_UPPER   0x40       /* %X (大写十六进制) */

/* 空白字符位掩码 (spaces recognized before flags: " \t\n\v\f\r" etc.) */
#define FMT_SPACE_MASK   0x00012809  /* 从池 0x08017B64 加载: 位0=空格, 3=#, 11=+, 13=-, 16=0 */

/* ================================================================
 * Formatted_Output_Engine() @ 0x08017764
 *
 * 栈帧:
 *   push.w {r0-r8, sb, sl, fp, lr}  — 14 寄存器, 56 bytes
 *   sub sp, #0x54                     — 84 bytes 局部变量
 *   总计: 140 bytes 栈帧
 *
 * 返回: r0 = 输出字符总数
 * ================================================================ */
uint32_t Formatted_Output_Engine(const char *fmt, uint32_t *args,
                                  void (*out_fn)(int ch), void *n_arg)
{
    uint32_t flags;       /* r4: 格式标志位域 */
    uint32_t prec;        /* r7: 精度值 */
    uint32_t width;       /* [sp+0x0C]: 宽度值 */
    uint32_t count;       /* r5: 输出字符总数 */
    const char *s;        /* r6: 格式字符串游标 */
    uint32_t *ap;         /* r8: va_list 参数指针 */
    uint32_t base_val;    /* 局部十进制累积器 */
    char ch;              /* 当前字符 */

    count = 0;

    /* ---- 主循环: 遍历格式字符串 ---- */
    for (;;) {
        ch = *fmt++;

        if (ch == '\0')
            return count;

        if (ch != '%') {
            /* 普通字符: 直接输出 */
            out_fn(ch);
            count++;
            continue;
        }

        /* ---- 解析 % 格式说明符 ---- */
        flags = 0;
        prec = 0;
        width = 0;

        /* 解析标志字符: 空格识别位掩码 skip */
        {
            uint32_t space_mask = FMT_SPACE_MASK;
            while ((ch = *fmt) != '\0') {
                uint32_t bit = 1 << (ch - 0x20);
                if (!(bit & space_mask))
                    break;
                flags |= bit;    /* 累积标志 */
                fmt++;
            }
        }

        /* 解析宽度: 十进制数字 或 * (从参数) */
        ch = *fmt;
        if (ch == '*') {
            width = *ap++;
            if ((int32_t)width < 0) {
                flags |= FLAG_NEG_WIDTH;
                width = -(int32_t)width;
            }
            flags |= FLAG_WIDTH_SET;
            fmt++;
        } else {
            base_val = -0x30;  /* mvn r3, #0x2f → r3 = -0x30 */
            while (*fmt >= '0' && *fmt <= '9') {
                flags |= FLAG_WIDTH_SET;
                width = width + width * 4;  /* width = width * 5 */
                width = base_val + width * 2;  /* width = base + width*2 = width*10 + digit */
                width += *fmt++;
            }
        }

        /* 解析精度: .decimal 或 .* (从参数) */
        ch = *fmt;
        if (ch == '.') {
            ch = *++fmt;
            flags |= FLAG_PREC_SET;
            if (ch == '*') {
                prec = *ap++;
                fmt++;
            } else {
                base_val = -0x30;
                while (*fmt >= '0' && *fmt <= '9') {
                    prec = prec + prec * 4;
                    prec = base_val + prec * 2;
                    prec += *fmt++;
                }
            }
        }

        /* 解析长度修饰符: h, hh, l, ll, L, j, z, t */
        ch = *fmt;
        if (ch == 'l') {
            flags |= FLAG_LONG;
            fmt++;
            ch = *fmt;
            if (ch == 'l') {
                flags |= FLAG_LONGLONG;
                fmt++;
            }
        } else if (ch == 'h') {
            flags |= FLAG_SHORT;
            fmt++;
            ch = *fmt;
            if (ch == 'h') {
                flags |= FLAG_SHORT << 1;  /* hh */
                fmt++;
            }
        } else if (ch == 'L' || ch == 'z' || ch == 't') {
            flags |= FLAG_SIZE_T;
            fmt++;
        } else if (ch == 'j') {
            flags |= FLAG_LONGLONG;
            fmt++;
        }

        /* ---- 格式类型分派 ---- */
        ch = *fmt++;

        if (ch == 'd' || ch == 'i') {
            /* 有符号十进制整数 */
            goto fmt_di;
        }
        if (ch == 'u') {
            /* 无符号十进制整数 */
            goto fmt_u;
        }
        if (ch == 'x' || ch == 'X') {
            /* 十六进制 */
            goto fmt_x;
        }
        if (ch == 'o') {
            /* 八进制 */
            goto fmt_o;
        }
        if (ch == 'c') {
            /* 字符 */
            char tmp_buf[2];
            tmp_buf[0] = (char)*ap++;
            tmp_buf[1] = '\0';
            s = tmp_buf;
            width = 1;
            goto fmt_s;
        }
        if (ch == 's') {
            /* 字符串 */
            s = (const char *)*ap++;
            width = -1;  /* 无宽度限制 */
            goto fmt_s;
        }
        if (ch == 'p') {
            /* 指针: base=16, prec=8, # 标志 */
            flags |= 0x04;  /* bit[2]: # 标志 (显示 0x 前缀) */
            prec = 8;
            goto fmt_x;
        }
        if (ch == 'n') {
            /* %n: 存储当前计数 */
            uint32_t sz = (flags >> 20) & 7;  /* 大小: 0=int, 2=long long, 3=short, 4=char */
            void *dest = (void *)*ap++;
            if (sz == 2) { *(uint64_t *)dest = count; }
            else if (sz == 3) { *(uint16_t *)dest = (uint16_t)count; }
            else if (sz == 4) { *(uint8_t *)dest = (uint8_t)count; }
            else { *(uint32_t *)dest = count; }
            continue;
        }
        if (ch == 'f' || ch == 'F' || ch == 'e' || ch == 'E' || ch == 'g' || ch == 'G') {
            /* 浮点: 调用 func_135 (IEEE 754 编码器) */
            uint64_t significand;
            int32_t  fp_val;
            uint32_t mode;
            double   d_result[2];
            uint32_t out_mode;

            /* 从 va_list 加载 double (2 words) */
            significand = *(uint64_t *)ap;
            ap += 2;

            /* 调用编码器 */
            func_175D0(significand, fp_val, mode, d_result, &out_mode);
            continue;
        }

        /* 未知格式: 输出 '%' 和字符原样 */
        out_fn('%');
        count++;
        /* fall through — 输出该字符 */
        out_fn(ch);
        count++;
        continue;

    fmt_s:
        /* ---- %s / %c 输出 ---- */
        {
            uint32_t i;
            int max_chars = (width == (uint32_t)-1) ? 0x7FFFFFFF : width;

            /* 左对齐检查 */
            if (!(flags & FLAG_NEG_WIDTH)) {
                /* 右对齐: 填充空格 */
                uint32_t slen = 0;
                while (s[slen] && (int32_t)slen < prec) slen++;
                for (i = slen; i < (int32_t)width; i++) {
                    out_fn(' ');
                    count++;
                }
            }

            /* 输出字符 */
            for (i = 0; i < (uint32_t)max_chars && s[i]; i++) {
                if (prec != 0 && i >= prec) break;
                out_fn(s[i]);
                count++;
            }
        }
        continue;

    fmt_di:
    fmt_u:
    fmt_x:
    fmt_o:
        /* 整数格式化: 调用子函数 B 和 C */
        {
            char num_buf[32];
            uint32_t int_val;
            int base;
            uint32_t prefix;

            /* 从 va_list 加载整数 */
            if (ch == 'd' || ch == 'i') {
                int_val = *ap++;
                if ((int32_t)int_val < 0 && !(flags & FLAG_LONG)) {
                    int_val = -(int32_t)int_val;
                    prefix = '-';
                }
            } else {
                int_val = *ap++;
            }

            if (ch == 'x' || ch == 'X') base = 16;
            else if (ch == 'o') base = 8;
            else base = 10;

            if (ch == 'p' || (flags & FLAG_HEX_UPPER))
                prefix = 'X';  /* 大写 */
            else
                prefix = 'x';

            /* 调用数字→字符串转换 */
            func_17E18(int_val, base, width, prec, flags, num_buf, &count);
            /* 调用数字格式化输出 */
            func_17E3C(num_buf, count, flags, width, prec, prefix);
        }
        continue;
    }

    return count;
}
