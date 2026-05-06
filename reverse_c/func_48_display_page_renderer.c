/**
 * @file func_48_display_page_renderer.c
 * @brief 函数: Display_Page_Renderer — TFT 显示页面构建与渲染 (多模式分发)
 * @addr  0x0800E2D8 - 0x0800F9C4 (5868 bytes, 含字面量池)
 *         嵌入式字面量池 0x0800E6D8-0x0800E7D8 (256 bytes)
 *         扩展字面量池 0x0800EB3C-0x0800EBAA (110 bytes)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 根据显示页面模式 (mode = [sp+0x68]) 进行级联分发:
 *   mode == 0:  初始化 + 构建 BUF_456/BUF_460 格式化字符串 → 11字节帧 → 退出
 *   mode == 2:  func_12454 + BUF_456/BUF_460 格式化字符串 → 17字节帧
 *               + common_tail(FLAG_38子分发 + FPU计算) → 退出
 *   mode == 3:  func_1257A + func_12430 → 退出
 *   mode == 4 或 0x24: 置位双标志 + func_12430 → 退出
 *   mode == 5 或 0x25: func_12430 + func_12484 + func_16B18 + func_12788(12字节) → 退出
 *   mode == 6:  func_1257A + func_12430 + UDIV/60 + 5帧格式化(1-4字节)
 *               + FPU范围计算(5级比较) → 退出
 *   mode == 7:  func_1257A + func_12430 → 退出
 *   mode == 8:  func_1257A + func_12430 + func_12484 + cond继续 → 退出
 *   mode >= 9:  后续级联分发 (0x0800EC5A起, 模式 9/10/11/12/13/14/15...)
 *
 * 参数:
 *   mode — 显示页面/模式 ID (uint32_t, 原为调用者压入栈 sp+0x68)
 *
 * 调用:
 *   func_18DC0()  @ 0x08018DC0 — 显示初始化
 *   func_0D3E4()  @ 0x0800D3E4 — 状态检查
 *   func_839A()   @ 0x0800839A — 字符串复制 (strcpy-like)
 *   func_16B18()  @ 0x08016B18 — 格式化输出 (sprintf)
 *   func_8334()   @ 0x08008334 — 字符串拼接 (strcat)
 *   func_12788()  @ 0x08012788 — 数据传输帧发送
 *   func_1257A()  @ 0x0801257A — 操作触发
 *   func_943E()   @ 0x0800943E — 计算操作
 *   func_124D6()  @ 0x080124D6 — NVIC 操作
 *   func_12430()  @ 0x08012430 — 参数操作
 *   func_12454()  @ 0x08012454 — 系统操作
 *   func_12484()  @ 0x08012484 — 状态操作
 *   func_873A()   @ 0x0800873A — 浮点转换准备
 *   func_87B4()   @ 0x080087B4 — 浮点转换准备 (变体)
 *   func_865C()   @ 0x0800865C — 浮点乘法
 *   func_883C()   @ 0x0800883C — 浮点取整
 *   func_875C()   @ 0x0800875C — uint16→double (unsigned)
 *   func_9660()   @ 0x08009660 — 字符串/命令发送变体
 *   func_126F8()  @ 0x080126F8 — 浮点数据发送
 *
 * 关键 RAM 变量:
 *   0x2000024A (FLAG_24A) — 当前模式存储
 *   0x20000172 (FLAG_172) — 前一模式备份
 *   0x200006C4 (BUF_6C4)  — 半字表 (10 条目, 临时清零)
 *   0x200001C0 (FLAG_1C0) — 控制标志
 *   0x200001C6 (FLAG_1C6) — 显示使能门控
 *   0x20000456 (BUF_456)  — 时间/日期数据块 1 (4 字节)
 *   0x20000460 (BUF_460)  — 时间/日期数据块 2 (4 字节)
 *   0x20000184 (FLAG_184) — 半字 (mode=2 时复制到 FLAG_182)
 *   0x20000182 (FLAG_182) — 半字
 *   0x20000019 (FLAG_19)  — FPU 门控标志
 *   0x20000173 (FLAG_173) — 参数字节
 *   0x20000121 (FLAG_121) — 参数字节
 *   0x20000038 (FLAG_38)  — 子模式 (1/2/3)
 *   0x2000003C (FLAG_3C)  — 有符号半字 (FPU 输入 A)
 *   0x2000003A (FLAG_3A)  — 有符号半字 (FPU 输入 B)
 *   0x20000128 (REG_128)  — 32位计数值 (mode=6 UDIV输入)
 *   0x20000170 (REG_170)  — 32位值
 *   0x20000148 (REG_148)  — 32位值
 *   0x20000144 (REG_144)  — 32位值
 *   0x20000074 (REG_74)   — 32位值 (FPU输入源)
 */

#include "stm32f4xx_hal.h"

/* ---- RAM 变量 (原范围) ---- */
#define FLAG_24A    (*(volatile uint8_t  *)0x2000024A)
#define FLAG_172    (*(volatile uint8_t  *)0x20000172)
#define BUF_6C4     ((volatile uint16_t *)0x200006C4)
#define FLAG_1C0    (*(volatile uint8_t  *)0x200001C0)
#define BUF_252     ((volatile uint8_t  *)0x20000252)
#define FLAG_1C6    (*(volatile uint8_t  *)0x200001C6)
#define BUF_456     ((volatile uint8_t  *)0x20000456)
#define BUF_460     ((volatile uint8_t  *)0x20000460)
#define FLAG_184    (*(volatile uint16_t *)0x20000184)
#define FLAG_182    (*(volatile uint16_t *)0x20000182)
#define FLAG_19     (*(volatile uint8_t  *)0x20000019)
#define FLAG_173    (*(volatile uint8_t  *)0x20000173)
#define FLAG_121    (*(volatile uint8_t  *)0x20000121)
#define FLAG_38     (*(volatile uint16_t *)0x20000038)
#define FLAG_3C     (*(volatile int16_t  *)0x2000003C)
#define FLAG_3A     (*(volatile int16_t  *)0x2000003A)

/* ---- RAM 变量 (扩展范围, 用于 mode >= 6) ---- */
#define REG_40      (*(volatile uint16_t *)0x20000040)  /* FPU input for channel 8 */
#define REG_3E      (*(volatile uint16_t *)0x2000003E)  /* FPU input for channel 9 */
#define REG_128     (*(volatile uint32_t *)0x20000128)
#define REG_170     (*(volatile uint32_t *)0x20000170)
#define REG_148     (*(volatile uint32_t *)0x20000148)
#define REG_144     (*(volatile uint32_t *)0x20000144)
#define REG_74      (*(volatile uint32_t *)0x20000074)

/* ---- 字面量池常量 ---- */
/* FPU double: 10.0 @ 0x0800E748 */
/* FPU float:  0.0  @ 0x0800E750 */
/* 格式串 "%4d"       @ 0x0800E70C */
/* 格式串 "%02d"      @ 0x0800E714 */
/* 格式串 "%d"        @ 0x0800EB68 (mode 6 使用) */
/* 格式串 "%05d"      @ 0x0800EB70 (mode 6 使用) */
/* 格式串 "%.4f"      @ 0x0800EB8C (mode 6 FPU 输出) */
/* 格式串 "%.3f"      @ 0x0800EB98 (mode 6 FPU 输出) */
/* 格式串 "%.1f"      @ 0x0800EBA4 (mode 6 FPU 输出) */

extern void func_18DC0(void);
extern void func_0D3E4(void);
extern void func_839A(uint8_t *dest, const uint8_t *src);
extern void func_16B18(uint8_t *buf, const char *fmt, uint32_t arg1);
extern void func_16B18_4(uint8_t *buf, const char *fmt, uint32_t arg1, uint32_t arg2);
extern void func_8334(uint8_t *dest, const uint8_t *src);
extern void func_12788(uint32_t mode, uint32_t len, uint8_t *data);
extern void func_1257A(uint8_t a, uint32_t b, uint32_t c, uint32_t d);
extern void func_943E(uint32_t a, uint32_t b, uint32_t val);
extern void func_124D6(uint32_t a, uint32_t b, uint32_t flag);
extern void func_12430(uint32_t a, uint32_t b, uint32_t c);
extern void func_12454(uint32_t mode);
extern void func_12484(uint32_t a, uint32_t b, uint32_t c);

/* ---- FPU 辅助函数 ---- */
extern int64_t func_873A(int16_t val);
extern int64_t func_87B4(int32_t val);
extern int64_t func_865C(int64_t a);            /* Capstone trace confirms: func_865C takes TWO doubles
                                                 * (r0:r1, r2:r3). Callers load 2nd operand from
                                                 * literal pool via VLDR before BL. Channel 8 uses
                                                 * 1000.0 @ pool 0x0800EB40; channels 3/4/9 use
                                                 * 10.0 @ pool 0x0800E748. 1-arg decl is temporary. */
extern int32_t func_883C(int64_t a);
extern int64_t func_875C(uint16_t val);        /* uint16→double (unsigned) */
extern void    func_9660(uint32_t mode, uint32_t sub); /* string/command sender variant */
extern void    func_126F8(uint32_t mode, uint32_t channel,
                          uint32_t r2_1, uint32_t r3_1, float val);

/* ================================================================
 * Display_Page_Renderer() @ 0x0800E2D8
 *   入口: push {r0, lr}; sub sp, #0x68  — 栈帧 0x6C 字节
 *   出口: add sp, #0x6c; pop {pc}       — @ 0x0800F9AE
 *
 * 栈布局:
 *   sp+0x00: 临时缓冲区 (24 bytes)
 *   sp+0x14: 临时值 val
 *   sp+0x18: 格式化子缓冲区 (8 bytes)
 *   sp+0x20: 主输出缓冲区 buf (64 bytes)
 *   sp+0x60: 浮点计算结果 f_val
 *   sp+0x64: 循环计数器 i
 *   sp+0x68: mode (调用者压入的参数)
 * ================================================================ */
void Display_Page_Renderer(uint32_t mode)
{
    uint8_t  buf[0x40];         /* sp+0x20 — 主输出缓冲区 */
    uint8_t  tmp[0x18];         /* sp+0x00 — 临时缓冲 */
    uint8_t  fmt_buf[8];        /* sp+0x18 — 格式化子缓冲 */
    uint32_t i;                 /* [sp+0x64] — 循环计数器 */
    uint32_t val;               /* [sp+0x14] — 临时值 */
    float    f_val;             /* [sp+0x60] — 浮点结果 */

    /* ---- 保存/恢复: FLAG_172 = FLAG_24A, 然后 FLAG_24A = mode ---- */
    FLAG_172 = FLAG_24A;
    FLAG_24A = (uint8_t)mode;

    /* ---- 清零 BUF_6C4[0..9] (10 个半字) ---- */
    for (i = 0; i < 10; i++) {
        BUF_6C4[i] = 0;
    }

    /* ================================================================
     * 第一级分发: mode == 0 ?
     * ================================================================ */
    if (mode == 0) {
        func_18DC0();
        func_0D3E4();

        if (FLAG_1C6 == 0) {
            goto exit_epilogue;
        }

        /* 前缀字符串: 根据 FLAG_1C0 选择模板 A 或 B */
        if (FLAG_1C0 == 0) {
            func_839A(buf, (uint8_t *)0x0800E6F0);   /* 模板 A */
        } else {
            func_839A(buf, (uint8_t *)0x0800E6FC);   /* 模板 B */
        }

        /* 构建: buf = 前缀 + BUF_456.hword("%4d") + "." + BUF_456[2]("%02d") + ":" + BUF_456[3]("%02d") + sep + ... */
        func_16B18(fmt_buf, (const char *)0x0800E70C, *(volatile uint16_t *)BUF_456); /* "%4d" */
        func_8334(buf, fmt_buf);
        func_8334(buf, (uint8_t *)0x0800E710);       /* TFT分隔符1 */
        func_16B18(fmt_buf, (const char *)0x0800E714, BUF_456[2]); /* "%02d" */
        func_8334(buf, fmt_buf);
        func_8334(buf, (uint8_t *)0x0800E71C);       /* TFT分隔符2 */
        func_16B18(fmt_buf, (const char *)0x0800E714, BUF_456[3]); /* "%02d" */
        func_8334(buf, fmt_buf);
        func_8334(buf, (uint8_t *)0x0800E720);       /* TFT分隔符3 */

        func_16B18(fmt_buf, (const char *)0x0800E70C, *(volatile uint16_t *)BUF_460); /* "%4d" */
        func_8334(buf, fmt_buf);
        func_8334(buf, (uint8_t *)0x0800E710);       /* TFT分隔符1 */
        func_16B18(fmt_buf, (const char *)0x0800E714, BUF_460[2]); /* "%02d" */
        func_8334(buf, fmt_buf);
        func_8334(buf, (uint8_t *)0x0800E71C);       /* TFT分隔符2 */
        func_16B18(fmt_buf, (const char *)0x0800E714, BUF_460[3]); /* "%02d" */
        func_8334(buf, fmt_buf);

        func_12788(0, 11, buf);
        goto exit_epilogue;
    }

    /* ================================================================
     * 第二级分发: mode == 2 ?
     * ================================================================ */
    if (mode == 2) {
        func_12454(0);
        FLAG_182 = FLAG_184;

        if (FLAG_1C6 != 0) {
            if (FLAG_1C0 == 0) {
                func_839A(buf, (uint8_t *)0x0800E6F0);
            } else {
                func_839A(buf, (uint8_t *)0x0800E6FC);
            }

            func_16B18(fmt_buf, (const char *)0x0800E70C, *(volatile uint16_t *)BUF_456); /* "%4d" */
            func_8334(buf, fmt_buf);
            func_8334(buf, (uint8_t *)0x0800E710);
            func_16B18(fmt_buf, (const char *)0x0800E714, BUF_456[2]); /* "%02d" */
            func_8334(buf, fmt_buf);
            func_8334(buf, (uint8_t *)0x0800E71C);
            func_16B18(fmt_buf, (const char *)0x0800E714, BUF_456[3]); /* "%02d" */
            func_8334(buf, fmt_buf);
            func_8334(buf, (uint8_t *)0x0800E720);

            func_16B18(fmt_buf, (const char *)0x0800E70C, *(volatile uint16_t *)BUF_460); /* "%4d" */
            func_8334(buf, fmt_buf);
            func_8334(buf, (uint8_t *)0x0800E710);
            func_16B18(fmt_buf, (const char *)0x0800E714, BUF_460[2]); /* "%02d" */
            func_8334(buf, fmt_buf);
            func_8334(buf, (uint8_t *)0x0800E71C);
            func_16B18(fmt_buf, (const char *)0x0800E714, BUF_460[3]); /* "%02d" */
            func_8334(buf, fmt_buf);

            func_8334(buf, (uint8_t *)0x0800E730);   /* mode=2 尾部 TFT 分隔符 */
            func_12788(2, 17, buf);
        }
        /* 落空到 common_tail */
    } else {
        /* ---- 其他 mode: 跳转到第三级分发入口 @ 0x0800E812 ---- */
        goto dispatch_mode_3_and_up;
    }

common_tail:
    /* ---- 通用尾部 (mode=2 以及 mode>=9 到达此处) ---- */
    func_1257A(FLAG_1C0, mode, 0x10, 1);
    val = (uint8_t)(FLAG_19 + FLAG_1C0 * 2);
    func_943E(2, 5, val);
    func_12430(FLAG_121, mode, FLAG_173);

    /* FLAG_38 子分发 (仅 mode==2 执行) */
    func_943E(2, 2, 0);
    func_943E(2, 0xA, 0);
    func_943E(2, 0xB, 0);

    if (FLAG_38 == 1) {
        val = (uint8_t)(FLAG_1C0 * 3 + 1);
        func_943E(2, 2, val);
        func_124D6(2, 0xD, 0);
    } else if (FLAG_38 == 2) {
        val = (uint8_t)(FLAG_1C0 * 3 + 2);
        func_943E(2, 0xA, val);
        func_124D6(2, 0xE, 0);
    } else if (FLAG_38 == 3) {
        val = (uint8_t)(FLAG_1C0 * 3 + 3);
        func_943E(2, 0xB, val);
        func_124D6(2, 0xF, 0);
    }

    /* ---- FLAG_19 门控 FPU: 通道 3 ---- */
    if (FLAG_19 != 0) {
        int64_t d64;
        d64 = func_873A(FLAG_3C);
        /* VLDR double 10.0 from 0x0800E748 */
        d64 = func_865C(d64);
        f_val = (float)func_883C(d64);
    } else {
        f_val = 0.0f;  /* VLDR from 0x0800E750 */
    }
    func_126F8(mode, 3, 1, 1, f_val);

    /* ---- FLAG_19 门控 FPU: 通道 4 ---- */
    if (FLAG_19 != 0) {
        int64_t d64;
        d64 = func_873A(FLAG_3A);
        d64 = func_865C(d64);
        f_val = (float)func_883C(d64);
    } else {
        f_val = 0.0f;
    }
    func_126F8(mode, 4, 1, 1, f_val);

    /* FLAG_19 路径分发 */
    if (FLAG_19 != 0) {
        goto path_fp_a;
    } else {
        goto path_fp_b;
    }

    /* ================================================================
     * FPU 路径 A: FLAG_19 != 0 @ 0x0800E758
     *   双精度 FPU 计算 → func_126F8(mode, 8|9, 1, 1, f)
     * ================================================================ */
path_fp_a:
    {
        int64_t d64;
        /* Channel 8: REG_40(uint16) → double, ×1000.0, → float */
        d64 = func_875C(REG_40);
        d64 = func_865C(d64);
        f_val = (float)func_883C(d64);
        func_126F8(mode, 8, 1, 1, f_val);

        /* Channel 9: REG_3E(uint16) → double, ×10.0, → float */
        d64 = func_875C(REG_3E);
        d64 = func_865C(d64);
        f_val = (float)func_883C(d64);
        func_126F8(mode, 9, 1, 1, f_val);
    }
    goto exit_epilogue;

    /* ================================================================
     * FPU 路径 B: FLAG_19 == 0 @ 0x0800E78C
     *   默认浮点常量 + func_126F8(mode, 8, 1, 1, 0.0) + 更多 FPU
     * ================================================================ */
path_fp_b:
    {
        int64_t d64;
        /* Channel 8: 0.0f */
        f_val = 0.0f;
        func_126F8(mode, 8, 1, 1, f_val);

        /* Channel 9: REG_3E(uint16) → double, ×10.0, → float */
        d64 = func_875C(REG_3E);
        d64 = func_865C(d64);
        f_val = (float)func_883C(d64);
        func_126F8(mode, 9, 1, 1, f_val);
    }
    goto exit_epilogue;

    /* ================================================================
     * 第三级分发入口 (mode != 0, mode != 2) @ 0x0800E812
     * ================================================================ */
dispatch_mode_3_and_up:

    /* ---- mode == 3 ? @ E812-E840 ---- */
    if (mode == 3) {
        val = (uint32_t)(uint8_t)mode;
        func_1257A(FLAG_1C0, val, 0xC, 4);
        func_12430(FLAG_121, val, FLAG_173);
        goto exit_epilogue;
    }

    /* ---- mode == 4 或 mode == 0x24 ? @ E842-E86C ---- */
    if (mode == 4 || mode == 0x24) {
        FLAG_173 = 1;  /* 0x20000173 ← 1 */
        FLAG_121 = 1;  /* 0x20000121 ← 1 */
        val = (uint32_t)(uint8_t)mode;
        func_12430(FLAG_121, val, FLAG_173);
        goto exit_epilogue;
    }

    /* ---- mode == 5 或 mode == 0x25 ? @ E86E-E8BC ---- */
    if (mode == 5 || mode == 0x25) {
        val = (uint32_t)(uint8_t)mode;
        func_12430(FLAG_121, val, FLAG_173);

        /* 根据 FLAG_121 取反设置 r2 */
        {
            uint32_t r2_tmp;
            if (FLAG_121 != 0) {
                r2_tmp = 0;
            } else {
                r2_tmp = 1;
            }
            func_12484(val, 2, r2_tmp);
        }

        /* 格式化: sprintf(buf, "%4d", FLAG_184) */
        func_16B18(buf, (const char *)0x0800E70C, FLAG_184);
        func_12788(val, 12, buf);
        goto exit_epilogue;
    }

    /* ---- mode == 6 ? @ E8BE-EAC2 ---- */
    if (mode == 6) {
        val = (uint32_t)(uint8_t)mode;
        func_1257A(FLAG_1C0, val, 6, 0x12);
        func_12430(FLAG_121, val, FLAG_173);

        /* UDIV: tmp = REG_128 / 60 → sprintf("%d", tmp) → func_12788(len=1) */
        {
            uint32_t tmp_div;
            tmp_div = REG_128 / 60;
            func_16B18(buf, (const char *)0x0800EB68, tmp_div);  /* "%d" */
            func_12788(val, 1, buf);
        }

        /* REG_170 → sprintf("%d") → func_12788(len=2) */
        {
            uint32_t v = REG_170;
            func_16B18(buf, (const char *)0x0800EB68, v);  /* "%d" */
            func_12788(val, 2, buf);
        }

        /* REG_148 → sprintf("%d") → func_12788(len=3) */
        {
            uint32_t v = REG_148;
            func_16B18(buf, (const char *)0x0800EB68, v);  /* "%d" */
            func_12788(val, 3, buf);
        }

        /* REG_144 → sprintf("%d") → func_12788(len=4) */
        {
            uint32_t v = REG_144;
            func_16B18(buf, (const char *)0x0800EB68, v);  /* "%d" */
            func_12788(val, 4, buf);
        }

        /* ---- FPU 范围计算 (5级级联比较) ---- */
        {
            uint32_t fsrc = REG_74;
            int64_t d64;
            const char *fmt;

            if (fsrc < 1000000) {
                fmt = (const char *)0x0800EBA4;       /* "%.1f" */
            } else if (fsrc < 10000000) {
                fmt = (const char *)0x0800EB98;       /* "%.3f" */
            } else if (fsrc < 100000000) {
                fmt = (const char *)0x0800EB8C;       /* "%.4f" */
            } else if (fsrc < 1000000000) {
                fmt = (const char *)0x0800EB8C;       /* "%.4f" */
            } else {
                fmt = (const char *)0x0800EB98;       /* "%.3f" — default */
            }

            /* FPU: uint32→double→vmul→double→float */
            d64 = func_87B4(fsrc);
            d64 = func_865C(d64);
            f_val = (float)func_883C(d64);

            /* 将 float 的 IEEE 754 位模式传给 func_16B18 做 %f 格式化
             * (原汇编: VMOV r2, s0 → 将浮点位模式传入整数寄存器) */
            {
                union { float f; uint32_t u; } float_bits;
                float_bits.f = f_val;
                func_16B18(buf, fmt, float_bits.u);
            }
            func_12788(val, 5, buf);
        }
        goto exit_epilogue;
    }

    /* ---- mode == 7 ? @ EAC4-EAF2 ---- */
    if (mode == 7) {
        val = (uint32_t)(uint8_t)mode;
        func_1257A(FLAG_1C0, val, 6, 7);
        func_12430(FLAG_121, val, FLAG_173);
        goto exit_epilogue;
    }

    /* ---- mode == 8 ? @ EAF4-EB3A ---- */
    if (mode == 8) {
        val = (uint32_t)(uint8_t)mode;
        func_1257A(FLAG_1C0, val, 7, 8);
        func_12430(FLAG_121, val, FLAG_173);

        {
            uint32_t r2_tmp;
            if (FLAG_121 != 0) {
                r2_tmp = 1;
            } else {
                r2_tmp = 0;  /* cbnz 反转 */
            }
            func_12484(val, 2, r2_tmp);
        }
        goto exit_epilogue;
    }

    /* ---- mode == 9 ? @ EC5A-EC88 ---- */
    if (mode == 9) {
        val = (uint32_t)(uint8_t)mode;
        func_1257A(FLAG_1C0, val, 0x11, 9);
        func_12430(FLAG_121, val, FLAG_173);
        goto common_tail;
    }

    /* ---- mode == 0xA 或 mode == 0xB ? @ EC8A-ECDE ---- */
    /* 注: 原汇编在 EC8A 检查 mode==0xA|0xB 并执行公共代码后, 落空至 ECE0
     *     再检查 mode==0xB 执行额外操作. 因此 mode==0xB 会先后执行两段. */
    if (mode == 0xA || mode == 0xB) {
        val = (uint32_t)(uint8_t)mode;
        func_1257A(FLAG_1C0, val, 0xB, 3);
        func_9660(val, 2);
        func_9660(val, 3);
        func_9660(val, 4);
        func_9660(val, 5);
        func_12430(FLAG_121, val, FLAG_173);
        /* 落空至下方 mode==0xB 检查 (原汇编 fallthrough) */
    }

    /* ---- mode == 0xB (单独) ? @ ECE0-ECFA ---- */
    if (mode == 0xB) {
        func_1257A(FLAG_1C0, val, 0xC, 0x1A);
        goto common_tail;
    }

    /* mode == 0xA: 公共代码已执行, 无额外处理 → common_tail */
    if (mode == 0xA) {
        goto common_tail;
    }

    /* ---- mode == 0xC 或 mode == 0x15 或 mode == 0x16 ? @ ECFC-ED46 ---- */
    if (mode == 0xC || mode == 0x15 || mode == 0x16) {
        val = (uint32_t)(uint8_t)mode;
        func_9660(val, 2);
        func_9660(val, 3);
        func_1257A(FLAG_1C0, val, 0xB, 2);
        func_12430(FLAG_121, val, FLAG_173);
        goto common_tail;
    }

    /* ---- mode == 0xD ? @ ED48-ED86 ---- */
    if (mode == 0xD) {
        val = (uint32_t)(uint8_t)mode;
        func_1257A(FLAG_1C0, val, 0xB, 0x11);
        FLAG_173 = 0;
        FLAG_121 = 1;
        FLAG_1C0 = 1;
        func_12430(FLAG_121, val, FLAG_173);
        goto common_tail;
    }

    /* ---- remaining modes (0xE+) fall through to common_tail ---- */
    goto common_tail;

    /* ================================================================
     * 统一出口 @ 0x0800F9AE
     * ================================================================ */
exit_epilogue:
    return;
}
