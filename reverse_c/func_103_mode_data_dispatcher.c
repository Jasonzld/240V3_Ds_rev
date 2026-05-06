/**
 * @file func_103_mode_data_dispatcher.c
 * @brief 函数: 模式数据分发器 — 基于模式的比较/发送/FPU 运算状态机
 * DISASSEMBLY-TRACED. Verified against Capstone.
 * @addr  0x08013CE4 - 0x0801429E (1466 bytes, 含字面量池)
 *         代码段1 0x08013CE4-0x080140E2 (模式 2/6/8 + 尾部检查)
 *         字面量池 0x080140E4-0x080141A3 (RAM 地址 + 格式串 + FPU 常量)
 *         代码段2 0x080141A4-0x0801429E (模式 0x26 处理 + 同步复制)
 *         字面量池 0x080142A0-0x08014344 (164 bytes, 全部 RAM 地址)
 *
 * 模式调度:
 *   模式 == 2: 6路传感器/寄存器比较, FPU 数据帧发送 (func_126F8)
 *   模式 == 6: 5路 32-bit 累加器比较, UDIV/FPU 格式化发送 (func_12788)
 *   模式 == 8: 字节标志比较, 显示命令 (func_12484)
 *   尾部检查: FLAG_BYTE_C6/BD/B0 多条件分支 → MODE==0x26 走 final_cleanup
 *   模式 0x26 处理: 三重门控 → func_1257A/func_12604/func_10BD0/func_9490
 *   最终汇总: FLAG_173 vs FLAG_176 → func_12430 → 17字段当前→先前同步
 *
 * 调用:
 *   func_943E()  @ 0x0800943E — 多模式操作
 *   func_124D6() @ 0x080124D6 — 显示命令 (4 字节参数)
 *   func_126F8() @ 0x080126F8 — FPU 数据帧发送
 *   func_12788() @ 0x08012788 — 格式化数据帧发送
 *   func_12484() @ 0x08012484 — 显示命令 (多参数)
 *   func_1257A() @ 0x0801257A — 操作触发器
 *   func_12604() @ 0x08012604 — 模式备份与通知
 *   func_12430() @ 0x08012430 — 参数操作
 *   func_10BD0() @ 0x08010BD0 — 事件处理器
 *   func_9490()  @ 0x08009490 — 延时
 *   func_16B18() @ 0x08016B18 — 数值格式化 (sprintf-like)
 *   func_873A()  @ 0x0800873A — int16→double
 *   func_865C()  @ 0x0800865C — FPU 双精度缩放 (vmul.f64)
 *   func_875C()  @ 0x0800875C — uint16→double
 *   func_883C()  @ 0x0800883C — double→float
 *   func_87B4()  @ 0x080087B4 — uint32→double
 */

#include "stm32f4xx_hal.h"

extern uint32_t func_943E(uint8_t mode, uint8_t sel, uint8_t val);
extern void     func_124D6(uint8_t mode, uint8_t sel, uint8_t flag);
extern void     func_126F8(uint8_t mode, uint8_t sel, uint8_t a, uint8_t b, float fval);
extern void     func_12788(uint8_t mode, uint8_t sel, uint8_t *data);
extern void     func_12484(uint16_t p1, uint16_t p2, uint8_t p3);
extern void     func_1257A(uint32_t flag, uint32_t mode, uint32_t sel, uint32_t param);
extern void     func_12604(uint16_t param);
extern void     func_12430(uint32_t a, uint32_t b, uint32_t c);
extern void     func_10BD0(uint8_t val);
extern void     func_9490(uint32_t a, uint32_t b, uint32_t c);
extern void     func_16B18(void *buf, const void *fmt, uint32_t val);
extern double   func_873A(int16_t val);
extern double   func_865C(double a, double b);
extern double   func_875C(uint16_t val);
extern float    func_883C(void);
extern double   func_87B4(uint32_t val);

/* ---- RAM 状态变量 (字面量池1: 0x080140E4-0x080141A3) ---- */
#define MODE_BYTE        (*(volatile uint8_t  *)0x2000024A)  /* 当前模式字节 */
#define SENSOR_A_CUR     (*(volatile uint8_t  *)0x20000019)  /* 传感器 A 当前 */
#define SENSOR_A_PREV    (*(volatile uint8_t  *)0x2000001A)  /* 传感器 A 先前 */

#define BYTE_1C0         (*(volatile uint8_t  *)0x200001C0)  /* 门控字节 (ldrb 读) */
#define HWORD_1C0        (*(volatile uint16_t *)0x200001C0)  /* 门控半字 (ldrh 读, 模式0x26门控) */

#define SENSOR_B_CUR     (*(volatile int16_t *)0x2000003C)   /* 传感器 B 当前 (int16) */
#define SENSOR_B_REF     (*(volatile int16_t *)0x200000B4)   /* 传感器 B 参考 (int16) */

#define SENSOR_C_CUR     (*(volatile int16_t *)0x2000003A)   /* 传感器 C 当前 (int16) */
#define SENSOR_C_REF     (*(volatile int16_t *)0x200000B2)   /* 传感器 C 参考 (int16) */

#define REG32_A_CUR      (*(volatile uint16_t *)0x20000040)  /* 32位寄存器 A 当前 */
#define REG32_A_REF      (*(volatile uint16_t *)0x200000B8)  /* 32位寄存器 A 参考 */

#define REG32_B_CUR      (*(volatile uint16_t *)0x2000003E)  /* 32位寄存器 B 当前 */
#define REG32_B_REF      (*(volatile uint16_t *)0x200000B6)  /* 32位寄存器 B 参考 */

#define FLAG_A_CUR       (*(volatile uint8_t  *)0x200000D9)  /* 标志 A 当前 */
#define FLAG_A_PREV      (*(volatile uint8_t  *)0x200000DA)  /* 标志 A 先前 */

/* 模式 6 用 32-bit 累加器 */
#define ACCUM_A_CUR      (*(volatile uint32_t *)0x20000128)
#define ACCUM_A_PREV     (*(volatile uint32_t *)0x20000158)
#define ACCUM_B_CUR      (*(volatile uint32_t *)0x20000070)
#define ACCUM_B_PREV     (*(volatile uint32_t *)0x20000090)
#define ACCUM_C_CUR      (*(volatile uint32_t *)0x20000148)
#define ACCUM_C_PREV     (*(volatile uint32_t *)0x20000154)
#define ACCUM_D_CUR      (*(volatile uint32_t *)0x20000144)
#define ACCUM_D_PREV     (*(volatile uint32_t *)0x20000150)
#define ACCUM_E_CUR      (*(volatile uint32_t *)0x20000074)
#define ACCUM_E_PREV     (*(volatile uint32_t *)0x20000098)

/* 模式 8/尾部检查用 */
#define FLAG_EE_CUR      (*(volatile uint8_t  *)0x200000EE)
#define FLAG_EE_PREV     (*(volatile uint8_t  *)0x200000FC)
#define FLAG_BYTE_C6     (*(volatile int8_t   *)0x200001C6)  /* 有符号字节 */
#define FLAG_BYTE_BD     (*(volatile uint8_t  *)0x200001BD)
#define FLAG_BYTE_B0     (*(volatile uint8_t  *)0x200001B0)

/* 最终汇总 + 同步用 (字面量池2: 0x080142A0-0x08014344) */
#define FLAG_BYTE_121    (*(volatile uint8_t  *)0x20000121)
#define FLAG_BYTE_173    (*(volatile uint8_t  *)0x20000173)
#define FLAG_BYTE_176    (*(volatile uint8_t  *)0x20000176)

/* 同步: 额外复制对 */
#define REG32_C_CUR      (*(volatile uint16_t *)0x20000042)
#define REG32_C_REF      (*(volatile uint16_t *)0x200000BA)
#define HW_6C_CUR        (*(volatile uint16_t *)0x2000006C)
#define HW_6C_REF        (*(volatile uint16_t *)0x200000BC)
#define ACCUM_F_CUR      (*(volatile uint32_t *)0x20000140)
#define ACCUM_F_PREV     (*(volatile uint32_t *)0x2000014C)
#define ACCUM_G_CUR      (*(volatile uint32_t *)0x20000078)
#define ACCUM_G_PREV     (*(volatile uint32_t *)0x2000009C)
#define REG16_E0_CUR     (*(volatile uint16_t *)0x200000E0)
#define REG16_E0_PREV    (*(volatile uint16_t *)0x200000E2)

/* ---- FPU 常量 (字面量池1) ---- */
#define FPU_D_CONST_10    10.0    /* 0x080140FC: 0x4024000000000000 */
#define FPU_S_ZERO        0.0f    /* 0x08014104: VLDR s16 默认值 */
#define FPU_D_CONST_1000  1000.0  /* 0x08014118: 0x408F400000000000 */

/* ---- 格式字符串 (字面量池1, ADR 寻址) ---- */
static const char FMT_05D[]   = "%05d";   /* 0x08014138 */
static const char FMT_DOT4F[] = "%.4f";   /* 0x08014164: 阈值1 val<1M */
static const char FMT_DOT3F[] = "%.3f";   /* 0x08014170: 阈值2 1M≤val<10M */
static const char FMT_DOT2F[] = "%.2f";   /* 0x0801417C: 阈值3 10M≤val<100M */
static const char FMT_DOT1F[] = "%.1f";   /* 0x08014188: 阈值4 100M≤val<1B + 默认≥1B */

void Mode_Data_Dispatcher(void)
{
    uint8_t  tmp_buf[16];      /* sp+0 */
    float    fp_val;           /* s16 */
    double   d9, d10;          /* d9=s18:s19, d10=s20:s21 */
    uint32_t val32;
    uint32_t udiv;

    /* ================================================================
     * 入口: 检查模式字节 → 分支
     * ================================================================ */
    if (MODE_BYTE == 2) {
        /* ---- 子块 1: 传感器 A 比较 (0x08013CF4-0x08013D2E) ---- */
        if (SENSOR_A_CUR != SENSOR_A_PREV) {
            /* calc = SENSOR_A_CUR + BYTE_1C0 * 2 */
            /* r3=ldrb[0x20000019]=SENSOR_A_CUR, r1=ldrb[0x200001C0]=BYTE_1C0 */
            /* add.w r1,r3,r1,lsl#1 → r1=SENSOR_A_CUR+BYTE_1C0*2 */
            uint8_t calc = (uint8_t)(SENSOR_A_CUR + (BYTE_1C0 * 2));
            func_943E(MODE_BYTE, 5, calc);
        }
        /* 门控: 检查 BYTE_1C0 (0x08013D18-0x08013D2A) */
        {
            uint8_t flag = BYTE_1C0 ? 0 : 1; /* cbnz→movs #1, else movs #0 */
            func_124D6(2, 6, flag);
        }

        /* ---- 子块 2: 传感器 B 比较 (0x08013D2E-0x08013D82) ---- */
        if (SENSOR_B_CUR != SENSOR_B_REF) {
            if (SENSOR_A_CUR) {           /* ldr r0,[pc,#0x3a8]→0x20000019; ldrb r0,[r0]; cbz → gate */
                d10 = func_873A(SENSOR_B_CUR);
                d9  = func_865C(d10, FPU_D_CONST_10);
                fp_val = func_883C();
            } else {
                /* VLDR s16, [pc, #0x394] → 0x08014104 = 0.0f */
                fp_val = FPU_S_ZERO;
            }
            func_126F8(MODE_BYTE, 3, 1, 1, fp_val);
        }

        /* ---- 子块 3: 传感器 C 比较 (0x08013D82-0x08013DDA) ---- */
        if (SENSOR_C_CUR != SENSOR_C_REF) {
            if (SENSOR_A_CUR) {           /* ldr r0,[pc,#0x354]→0x20000019; ldrb r0,[r0]; cbz → gate */
                d10 = func_873A(SENSOR_C_CUR);
                d9  = func_865C(d10, FPU_D_CONST_10);
                fp_val = func_883C();
            } else {
                fp_val = FPU_S_ZERO;
            }
            func_126F8(MODE_BYTE, 4, 1, 1, fp_val);
        }

        /* ---- 子块 4: 寄存器32 A 比较 (0x08013DDA-0x08013E30) ---- */
        if (REG32_A_CUR != REG32_A_REF) {
            if (SENSOR_A_CUR) {           /* ldr r0,[pc,#0x300]→0x20000019; ldrb r0,[r0]; cbz → gate */
                /* uint16→double via func_875C, scale by 1000.0 */
                d10 = func_875C(REG32_A_CUR);
                d9  = func_865C(d10, FPU_D_CONST_1000);
                fp_val = func_883C();
            } else {
                fp_val = FPU_S_ZERO;
            }
            func_126F8(MODE_BYTE, 8, 1, 1, fp_val);
        }

        /* ---- 子块 5: 寄存器32 B 比较 (0x08013E30-0x08013E72) ---- */
        /* 无门控 — 始终走 FPU 路径 */
        if (REG32_B_CUR != REG32_B_REF) {
            d10 = func_875C(REG32_B_CUR);
            d9  = func_865C(d10, FPU_D_CONST_10);
            fp_val = func_883C();
            func_126F8(MODE_BYTE, 9, 1, 1, fp_val);
        }

        /* ---- 子块 6: 标志 A 比较 (0x08013E72-0x08013EA4) ---- */
        if (FLAG_A_CUR != FLAG_A_PREV) {
            if (FLAG_A_CUR < 3) {
                /* calc = (2 - FLAG_A_CUR) + BYTE_1C0 * 3 */
                /* rsb.w r1,r1,#2 → r1 = 2 - FLAG_A_CUR */
                /* ldrb r3,[0x200001C0]; add.w r3,r3,r3,lsl#1 = r3*3 */
                /* add r1,r3 → r1 = (2-FLAG_A_CUR) + BYTE_1C0*3 */
                uint8_t calc = (uint8_t)((2 - FLAG_A_CUR) + (BYTE_1C0 * 3));
                func_943E(MODE_BYTE, 1, calc);
            }
        }
        goto tail_checks;

    } /* end MODE_BYTE == 2 */

    /* ================================================================
     * 模式 != 2 → 检查模式 6
     * ================================================================ */
    if (MODE_BYTE != 6) {
        goto mode_not_6;
    }

    /* ============ 模式 6: 累加器比较 + UDIV/FPU 数据发送 ============ */

    /* 累加器 A: UDIV / 60 (0x08013EAE-0x08013EDA) */
    if (ACCUM_A_CUR != ACCUM_A_PREV) {
        udiv = ACCUM_A_CUR / 60;                     /* udiv r4,r0,#60 */
        func_16B18(tmp_buf, FMT_05D, udiv);
        func_12788(MODE_BYTE, 1, tmp_buf);
    }

    /* 累加器 B (0x08013EDA-0x08013EFE) */
    if (ACCUM_B_CUR != ACCUM_B_PREV) {
        func_16B18(tmp_buf, FMT_05D, ACCUM_B_CUR);
        func_12788(MODE_BYTE, 2, tmp_buf);
    }

    /* 累加器 C (0x08013EFE-0x08013F22) */
    if (ACCUM_C_CUR != ACCUM_C_PREV) {
        func_16B18(tmp_buf, FMT_05D, ACCUM_C_CUR);
        func_12788(MODE_BYTE, 3, tmp_buf);
    }

    /* 累加器 D (0x08013F22-0x08013F46) */
    if (ACCUM_D_CUR != ACCUM_D_PREV) {
        func_16B18(tmp_buf, FMT_05D, ACCUM_D_CUR);
        func_12788(MODE_BYTE, 4, tmp_buf);
    }

    /* 累加器 E: 5级阈值 + 统一 FPU 链 (0x08013F46-0x0801408A) */
    if (ACCUM_E_CUR != ACCUM_E_PREV) {
        val32 = ACCUM_E_CUR;
        {
            /* 阈值链与格式串选择 */
            const char *fmt;
            float f;

            if      (val32 < 1000000)    { fmt = FMT_DOT4F; }  /* 0x0F4240 */
            else if (val32 < 10000000)   { fmt = FMT_DOT3F; }  /* 0x989680 */
            else if (val32 < 100000000)  { fmt = FMT_DOT2F; }  /* 0x5F5E100 */
            else if (val32 < 1000000000) { fmt = FMT_DOT1F; }  /* 0x3B9ACA00 */
            else                         { fmt = FMT_DOT1F; }  /* >= 1B */

            /* 统一 FPU 链: VCVT.F32.U32 → func_87B4 → func_865C(*,1000.0) → d9 */
            /* vcvt.f32.u32 s0,s0; vmov r0,s0; bl func_87B4; vmov d10,r0,r1 */
            f   = (float)val32;
            d10 = func_87B4(*(uint32_t *)&f);
            d9  = func_865C(d10, FPU_D_CONST_1000);
            /* d9 的低 32 位作为 uint32 传给 func_16B18 */
            func_16B18(tmp_buf, fmt, (uint32_t)(*(uint64_t *)&d9));
        }
        func_12788(MODE_BYTE, 5, tmp_buf);
    }
    goto tail_checks;

    /* ============ 模式 != 6 → 检查模式 8 ============ */
mode_not_6:
    if (MODE_BYTE == 8) {
        /* 模式 8: 字节标志比较 + 显示命令 (0x08014094-0x080140B4) */
        if (FLAG_EE_CUR != FLAG_EE_PREV) {
            func_12484(8, 4, FLAG_EE_CUR);
            func_12484(8, 8, FLAG_EE_PREV);
        }
    }
    /* 贯穿到 tail_checks */

    /* ================================================================
     * 尾部检查: MODE_BYTE==0x26 + FLAG_C6/BD/B0 多条件
     * (0x080140B8-0x080140E2)
     * ================================================================ */
tail_checks:
    if (MODE_BYTE == 0x26) {
        goto final_cleanup;
    }

    /* 检查1: FLAG_BYTE_C6 + 1 == 0? → mode_26_action (绕过门控) */
    {
        int8_t v1 = FLAG_BYTE_C6 + 1;
        if (v1 == 0) goto mode_26_action;       /* cbz → 0x080141B4, 跳过 B0/C6/HWORD 门控 */
    }
    /* 检查2: FLAG_BYTE_C6 + 2 == 0? → mode_26_action (绕过门控) */
    {
        int8_t v2 = FLAG_BYTE_C6 + 2;
        if (v2 == 0) goto mode_26_action;       /* cbz → 0x080141B4, 跳过门控 */
    }
    /* 检查3: FLAG_BYTE_BD == 0? → final_cleanup */
    if (FLAG_BYTE_BD == 0) goto final_cleanup;  /* cbz → 0x080141E8 */

    /* 检查4: 无条件分支 → mode_26_handler 门控入口 */
    /* ldr r0,[FLAG_BYTE_B0]; ldrb r0,[r0]; b #0x80141a4 */
    /* B0 非零检查在 mode_26_handler 入口 (cbz r0 → final_cleanup) */
    goto mode_26_handler;

    /* ================================================================
     * 模式 0x26 处理: 三重门控 → 操作链
     * (0x080141A4-0x080141E4)
     * ================================================================ */
mode_26_handler:
    /* ---- 门控入口 (0x080141A4): B0非零 → C6==1 → HWORD_1C0==0 ---- */
    /* cbz r0, #0x80141e8 — 若 B0==0 则跳转到 final_cleanup */
    if (FLAG_BYTE_B0 == 0) {
        goto final_cleanup;
    }
    /* cmp r0, #1; bne #0x80141e8 — 若 C6 != 1 则跳转 */
    {
        uint8_t v_c6 = FLAG_BYTE_C6;
        if (v_c6 != 1) {
            goto final_cleanup;
        }
    }
    /* ldrh r0,[0x200001C0]; cbnz r0,#0x80141e8 — 若 HWORD_1C0 != 0 则跳转 */
    if (HWORD_1C0 != 0) {
        goto final_cleanup;
    }
    /* 门控通过 → 贯穿到 mode_26_action */

mode_26_action:
    /* ---- 操作执行 (0x080141B4): func_1257A + func_12604 + MODE_BYTE修改 + func_10BD0 + func_9490 ---- */
    /* func_1257A(MODE_BYTE, 0x26, 2, 0x1C) */
    func_1257A(MODE_BYTE, 0x26, 2, 0x1C);

    /* func_12604(0x26) */
    func_12604(0x26);

    /* MODE_BYTE = 0x26 */
    MODE_BYTE = 0x26;

    /* *(uint8_t*)0x20000019 = 0 (清零 SENSOR_A_CUR) */
    {
        volatile uint8_t *p = (volatile uint8_t *)0x20000019;
        *p = 0;
        /* func_10BD0(0) — param = 刚清零的值 */
        func_10BD0(*p);
    }

    /* func_9490(500, 500, 2) — 定时器触发, 三参数 */
    func_9490(500, 500, 2);

    /* ================================================================
     * 最终汇总: FLAG_173 vs FLAG_176 比较 + 17字段同步复制
     * (0x080141E8-0x0801429E)
     * ================================================================ */
final_cleanup:
    {
        uint8_t v173 = FLAG_BYTE_173;
        uint8_t v176 = FLAG_BYTE_176;
        if (v173 != v176) {
            /* func_12430(FLAG_BYTE_121, MODE_BYTE, FLAG_BYTE_173) */
            func_12430(FLAG_BYTE_121, MODE_BYTE, v173);
        }
    }

    /* ---- 同步: 将当前值复制到先前值 (17 对) ---- */
    /* 1. byte: SENSOR_A */
    *(volatile uint8_t  *)0x2000001A = *(volatile uint8_t  *)0x20000019;

    /* 2. int16: SENSOR_C_CUR → 0xB2 */
    *(volatile int16_t  *)0x200000B2 = *(volatile int16_t  *)0x2000003A;

    /* 3. int16: 0x3C → SENSOR_B_REF (0xB4) */
    *(volatile int16_t  *)0x200000B4 = *(volatile int16_t  *)0x2000003C;

    /* 4. uint16: REG32_B_CUR → REG32_B_REF */
    *(volatile uint16_t *)0x200000B6 = *(volatile uint16_t *)0x2000003E;

    /* 5. uint16: REG32_A_CUR → REG32_A_REF */
    *(volatile uint16_t *)0x200000B8 = *(volatile uint16_t *)0x20000040;

    /* 6. uint16: REG32_C_CUR → REG32_C_REF */
    *(volatile uint16_t *)0x200000BA = *(volatile uint16_t *)0x20000042;

    /* 7. uint16: HW_6C_CUR → HW_6C_REF */
    *(volatile uint16_t *)0x200000BC = *(volatile uint16_t *)0x2000006C;

    /* 8. byte: FLAG_A_CUR → FLAG_A_PREV */
    *(volatile uint8_t  *)0x200000DA = *(volatile uint8_t  *)0x200000D9;

    /* 9. uint32: ACCUM_B_CUR → ACCUM_B_PREV */
    *(volatile uint32_t *)0x20000090 = *(volatile uint32_t *)0x20000070;

    /* 10. uint32: ACCUM_F_CUR → ACCUM_F_PREV */
    *(volatile uint32_t *)0x2000014C = *(volatile uint32_t *)0x20000140;

    /* 11. uint32: ACCUM_A_CUR → ACCUM_A_PREV */
    *(volatile uint32_t *)0x20000158 = *(volatile uint32_t *)0x20000128;

    /* 12. uint32: ACCUM_D_CUR → ACCUM_D_PREV */
    *(volatile uint32_t *)0x20000150 = *(volatile uint32_t *)0x20000144;

    /* 13. uint32: ACCUM_C_CUR → ACCUM_C_PREV */
    *(volatile uint32_t *)0x20000154 = *(volatile uint32_t *)0x20000148;

    /* 14. uint32: ACCUM_E_CUR → ACCUM_E_PREV */
    *(volatile uint32_t *)0x20000098 = *(volatile uint32_t *)0x20000074;

    /* 15. uint32: ACCUM_G_CUR → ACCUM_G_PREV */
    *(volatile uint32_t *)0x2000009C = *(volatile uint32_t *)0x20000078;

    /* 16. uint16: REG16_E0_CUR → REG16_E0_PREV */
    *(volatile uint16_t *)0x200000E2 = *(volatile uint16_t *)0x200000E0;

    /* 17. byte: FLAG_EE_CUR → FLAG_EE_PREV */
    *(volatile uint8_t  *)0x200000FC = *(volatile uint8_t  *)0x200000EE;

    /* 18. byte: FLAG_BYTE_173 (0x20000173) → FLAG_BYTE_176 (0x20000176) */
    /* 0x08014290: ldr r0,[pc,#0x20]→0x20000173; ldrb r0,[r0] */
    /* 0x08014294: ldr r1,[pc,#0x20]→0x20000176; strb r0,[r1] */
    *(volatile uint8_t  *)0x20000176 = *(volatile uint8_t  *)0x20000173;

    /* 贯穿到 function_end */

function_end:
    /* 尾声: add sp,#0x10; vpop {d8,d9,d10}; pop {r4,pc} */
    return;
}
