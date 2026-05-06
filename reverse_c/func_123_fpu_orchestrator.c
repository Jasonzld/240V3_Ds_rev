/**
 * @file func_123_fpu_orchestrator.c
 * @brief 函数: FPU 计算编排器 — 7阶段双精度 FPU 流水线, 协调 func_101/func_102
 * DISASSEMBLY-TRACED. Verified against Capstone.
 * @addr  0x08016244 - 0x08016506 (708 bytes)
 *         字面量池 0x08016506-0x08016548 (66 bytes, 7个双精度常量 + 2个 RAM 地址)
 *
 * 7个阶段:
 *   阶段1: func_10B3C 范围检查 → 超范围直接 func_883C(d8)/func_883C(d9) 存储
 *   阶段2: func_856C(d8,35)+func_856C(d9,105) → func_101 → d13 (s26:s27)
 *   阶段3: func_856C(d8,35)+func_856C(d9,105) → func_102 → d14 (s28:s29)
 *   阶段4: func_865C(d8,180)*PI → func_18E48→func_873A→d10 → func_8578 链 → d10
 *   阶段5: d10→func_18E58→func_873A→d15 → func_8578 链 → func_865C(*,func_8578(d13,180)) → d13
 *   阶段6: func_18414(sp30)→func_873A → func_865C(d11,d15) 链 → func_865C(*,func_8578(d14,180)) → d14
 *   阶段7: RESULT_A=func_883C(d8+d13), RESULT_B=func_883C(d14+d9)
 *
 * 输入: s0-s3 = 4个float → s16-s19 (d8=fa:fb, d9=fc:fd)
 * 常量: d11=6378245.0@0x08016508, d12=0.00669342...@0x08016510
 *       35.0@0x08016520, 105.0@0x08016528, 180.0@0x08016530, PI@0x08016538, 1.0@0x08016540
 * 输出: RESULT_REG_A(0x200001CC), RESULT_REG_B(0x200001C8)
 *
 * 调用:
 *   func_883C()  — double→float 转换 (r0:r1→s0)
 *   func_10B3C() — 浮点范围检查
 *   func_18414() — 辅助函数 (float,float→uint32)
 *   func_18E48() — float→int (via func_873A)
 *   func_18E58() — float→int (via func_873A)
 *   func_842A()  — vadd.f64 双精度加法
 *   func_856C()  — 双精度运算变体 A
 *   func_8572()  — 双精度运算变体 B
 *   func_8578()  — vmul.f64 双精度乘法
 *   func_865C()  — 双精度缩放
 *   func_873A()  — int16→double
 *   func_13544() — FPU Compute Engine A (func_101)
 *   func_138F8() — FPU Compute Engine B (func_102)
 */

#include "stm32f4xx_hal.h"

extern float    func_883C(double val);    /* 0x0800883C: double→float, r0:r1 in, r0 out */
extern uint32_t func_10B3C(float a, float b, float c, float d);
extern uint32_t func_18414(float a, float b); /* VLDR d0→s0:s1, then BL */
extern uint32_t func_18E48(float a, float b);
extern uint32_t func_18E58(float a, float b);
extern double   func_842A(double a, double b);  /* vadd.f64 */
extern double   func_856C(double a, double b);
extern double   func_8572(double a, double b);
extern double   func_8578(double a, double b);  /* vmul.f64 */
extern double   func_865C(double a, double b);
extern double   func_873A(int16_t val);
extern double   func_13544(double a, double b); /* FPU_Compute_Engine_A */
extern double   func_138F8(double a, double b); /* FPU_Compute_Engine_B */

/* 输出寄存器 (LDR from literal pool: 0x200001CC@0x16518/0x1651C, 0x200001C8@0x1651E) */
#define RESULT_REG_A  (*(volatile uint32_t *)0x200001CC)
#define RESULT_REG_B  (*(volatile uint32_t *)0x200001C8)

/* 字面量池双精度常量 (IEEE-754 已验证) */
#define CONST_6378245      6378245.0              /* 0x08016508: 0x415854C140000000 */
#define CONST_0_0066934    0.006693421622965943   /* 0x08016510: 0x3F7B6A8FAF80EF0B */
#define CONST_35           35.0                   /* 0x08016520: 0x4041800000000000 */
#define CONST_105          105.0                  /* 0x08016528: 0x405A400000000000 */
#define CONST_180          180.0                  /* 0x08016530: 0x4066800000000000 */
#define CONST_PI           3.141592653589793      /* 0x08016538: 0x400921FB54442D18 */
#define CONST_1_0          1.0                    /* 0x08016540: 0x3FF0000000000000 */

void FPU_Orchestrator(float fa, float fb, float fc, float fd)
{
    double d8, d9, d10, d11, d12, d13, d14, d15;
    double sp28, sp20, sp30, sp18, sp10, sp8;
    int    r4;

    /* ---- 输入映射 (0x0801624C-0x08016258) ----
     * vmov.f32 s16,s0; vmov.f32 s17,s1 → d8 = (fa, fb)
     * vmov.f32 s18,s2; vmov.f32 s19,s3 → d9 = (fc, fd)
     */
    d8 = *(double *)&(float[2]){fa, fb};
    d9 = *(double *)&(float[2]){fc, fd};

    /* ---- 加载常量 (0x0801625C-0x08016270) ----
     * VLDR d0,[pc,#0x2a8]→0x08016508 → d11=6378245.0
     * VLDR d0,[pc,#0x2a4]→0x08016510 → d12=0.00669342...
     */
    d11 = CONST_6378245;
    d12 = CONST_0_0066934;

    /* ================================================================
     * 阶段 1: 范围检查 (0x08016274-0x080162A2)
     * vmov.f32 s0,s16; vmov.f32 s1,s17; vmov s2,s18; vmov s3,s19
     * bl func_10B3C(fa,fb,fc,fd); cbz r0 → 若超范围直接存储返回
     * ================================================================ */
    if (func_10B3C(fa, fb, fc, fd)) {
        /* vmov r0,r1,d8; bl func_883C → r0=float bits → str to 0x200001CC */
        RESULT_REG_A = *(uint32_t *)&(float){func_883C(d8)};
        /* vmov r0,r1,d9; bl func_883C → r0=float bits → str to 0x200001C8 */
        RESULT_REG_B = *(uint32_t *)&(float){func_883C(d9)};
        return;
    }

    /* ================================================================
     * 阶段 2: func_856C(d8,35)+func_856C(d9,105) → func_101 → d13
     * (0x080162A4-0x080162DC)
     * ================================================================ */
    sp28 = func_856C(d8, CONST_35);           /* VLDR 35; vmov r2,r3,d0; vmov r0,r1,d8; bl */
    sp20 = func_856C(d9, CONST_105);          /* VLDR 105; vmov r2,r3,d0; vmov r0,r1,d9; bl */
    /* vldr d1,[sp,#0x28]; vldr d0,[sp,#0x20]; bl func_13544 → d13 */
    d13 = func_13544(sp20, sp28);

    /* ================================================================
     * 阶段 3: func_856C(d8,35)+func_856C(d9,105) → func_102 → d14
     * (0x080162E0-0x08016318)
     * ================================================================ */
    sp28 = func_856C(d8, CONST_35);
    sp20 = func_856C(d9, CONST_105);
    d14 = func_138F8(sp20, sp28);

    /* ================================================================
     * 阶段 4: 第三 FPU 链 → d10 (0x0801631C-0x08016392)
     *
     * func_865C(d8,180.0) → func_8578(*, PI) → [sp+0x30]
     * → func_18E48(s0,s1) → func_873A → d10
     * → func_8578(d12, d10) → func_8578(*, d10) → func_8572(*, 1.0) → d10
     * ================================================================ */
    sp30 = func_8578(func_865C(d8, CONST_180), CONST_PI);
    {
        float f_lo, f_hi;
        *(double *)&(float[2]){f_lo, f_hi} = sp30;
        r4 = func_18E48(f_lo, f_hi);         /* VLDR d0,[sp,#0x30] → s0,s1 */
    }
    d10 = func_873A((int16_t)r4);

    /* func_8578(d12, d10) → [sp+0x20] → func_8578(*, d10) → [sp+0x28] → func_8572(*, 1.0) */
    sp20 = func_8578(d12, d10);               /* vmov r2,r3,d10; vmov r0,r1,d12; bl */
    sp28 = func_8578(sp20, d10);              /* VLDR d0,[sp,#0x20]; vmov r0,r1,d0; bl (r2:r3=d10 from prev) */
    d10  = func_8572(sp28, CONST_1_0);        /* VLDR 1.0; vmov r2,r3,d0; VLDR d0,[sp,#0x28]; bl */

    /* ================================================================
     * 阶段 5: 第四 FPU 链 → d15 计算 + d13 更新 (0x08016396-0x0801643C)
     *
     * d10 → func_18E58 → func_873A → d15
     * func_8578(d10, d15) → [sp+0x10]
     * func_856C(1.0, d12) → func_8578(*, d11) → [sp+0x08]
     * func_865C([sp+0x08], [sp+0x10]) → func_8578(*, PI) → [sp+0x28]
     * func_8578(d13, 180.0) → func_865C(*, [sp+0x28]) → d13
     * ================================================================ */
    {
        float f_lo, f_hi;
        *(double *)&(float[2]){f_lo, f_hi} = d10;
        r4 = func_18E58(f_lo, f_hi);         /* vmov.f32 s0,s20; vmov.f32 s1,s21 → s0,1=d10 halves */
    }
    d15 = func_873A((int16_t)r4);

    sp10 = func_8578(d10, d15);               /* vmov r2,r3,d15; vmov r0,r1,d10; bl */
    sp8  = func_8578(func_856C(CONST_1_0, d12), d11); /* VLDR 1.0; vmov r0,r1,d0; bl func_856C(1.0,d12) → func_8578(*,d11) */
    sp18 = func_865C(sp10, sp8);              /* VLDR d0,[sp,#0x10]; VLDR d1,[sp,#8]; bl */
    sp28 = func_8578(sp18, CONST_PI);         /* VLDR PI; vmov r2,r3,d0; VLDR d0,[sp,#0x18]; bl */

    sp20 = func_8578(d13, CONST_180);          /* VLDR 180; vmov r2,r3,d0; vmov r0,r1,d13; bl */
    d13  = func_865C(sp20, sp28);             /* VLDR d0,[sp,#0x28]; VLDR d0,[sp,#0x20]; bl */

    /* ================================================================
     * 阶段 6: 第五 FPU 链 → d14 更新 (0x08016440-0x080164BA)
     *
     * func_18414(sp30) → func_873A → [sp+0x10]
     * func_865C(d11, d15) → [sp+0x08]
     * func_8578([sp+0x08], [sp+0x10]) → func_8578(*, PI) → [sp+0x28]
     * func_8578(d14, 180.0) → func_865C(*, [sp+0x28]) → d14
     * ================================================================ */
    {
        float f_lo, f_hi;
        *(double *)&(float[2]){f_lo, f_hi} = sp30;
        r4 = func_18414(f_lo, f_hi);          /* VLDR d0,[sp,#0x30] → s0,s1; bl */
    }
    sp10 = func_873A((int16_t)r4);

    sp8  = func_865C(d11, d15);               /* vmov r2,r3,d15; vmov r0,r1,d11; bl */
    sp18 = func_8578(sp10, sp8);              /* VLDR d0,[sp,#0x10]; VLDR d1,[sp,#8]; bl */
    sp28 = func_8578(sp18, CONST_PI);         /* VLDR PI; vmov r2,r3,d0; VLDR d0,[sp,#0x18]; bl */

    sp20 = func_8578(d14, CONST_180);          /* VLDR 180; vmov r2,r3,d0; vmov r0,r1,d14; bl */
    d14  = func_865C(sp20, sp28);             /* VLDR d0,[sp,#0x28]; VLDR d0,[sp,#0x20]; bl */

    /* ================================================================
     * 阶段 7: 最终聚合 + 存储 (0x080164BE-0x080164FC)
     *
     * func_842A(d8, d13) → func_883C → RESULT_REG_A
     * func_842A(d14, d9) → func_883C → RESULT_REG_B
     * ================================================================ */
    sp28 = func_842A(d8, d13);                /* vmov r2,r3,d13; vmov r0,r1,d8; bl */
    RESULT_REG_A = *(uint32_t *)&(float){func_883C(sp28)};

    sp28 = func_842A(d14, d9);                /* vmov r2,r3,d9; vmov r0,r1,d14; bl */
    RESULT_REG_B = *(uint32_t *)&(float){func_883C(sp28)};

    /* 尾声: add sp,#0x38; vpop {d8-d15}; pop {r4,pc} */
}
