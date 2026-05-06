/**
 * @file func_102_fpu_compute_engine_b.c
 * @brief 函数: FPU 计算引擎 B — 多级双精度浮点累加流水线 (变体)
 * @addr  0x080138F8 - 0x08013C8A (914 bytes, 含字面量池)
 *         代码 0x080138F8-0x08013C8B (915 bytes)
 *         字面量池 0x08013C8C-0x08013CE3 (88 bytes, 11 个双精度常量)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 与 func_101 的关键差异:
 *   - d8=(fa,fb), d10=(fc,fd) — 输入映射交换 (func_101: d8=(fc,fd), d9=(fa,fb))
 *   - 返回值在 d9 (s18:s19) 而非 d10 (s20:s21)
 *   - 仅 1 处使用 s30:s31 (子块 3), 其余用 s28:s29
 *   - 加法累积链: d8+300+2*d10+0.1*d8^2+0.1*d8*d10 (func_101: d9*2-100+3*d8)
 *   - 4 个子块 (func_101: 5 个)
 *   - 11 个双精度常量 (func_101: 12 个)
 *
 * 4 个浮点显式输入:
 *   s0→s16, s1→s17 → d8  = (fa, fb)
 *   s2→s20, s3→s21 → d10 = (fc, fd)
 *
 * 2 个浮点隐式输入 (callee-saved FPU context):
 *   s28-s29 (d14) — 子块 1/2/3/4 使用
 *   s30-s31 (d15) — 子块 3 使用
 *
 * 返回值: double in s0:s1 (来自 d9 = s18:s19)
 *
 * 调用:
 *   func_1850C() @ 0x0801850C — float→int 转换 A
 *   func_18E58() @ 0x08018E58 — float→int 转换 B
 *   func_18E48() @ 0x08018E48 — float→int 转换 C
 *   func_873A()  @ 0x0800873A — int16→double 转换
 *   func_8578()  @ 0x08008578 — vmul.f64 (双精度乘法)
 *   func_842A()  @ 0x0800842A — vadd.f64/vsub.f64 (双精度加减)
 *   func_865C()  @ 0x0800865C — 双精度缩放
 */

#include "stm32f4xx_hal.h"

extern uint32_t func_1850C(float a, float b);
extern uint32_t func_18E58(float a, float b);
extern uint32_t func_18E48(float a, float b);
extern double   func_873A(int16_t val);
extern double   func_8578(double a, double b);
extern double   func_842A(double a, double b);
extern double   func_865C(double a, double b);

/* 字面量池常量 (0x08013C8C-0x08013CE3, 11 个 double, 已验证 IEEE-754) */
#define CONST_0_1     0.1
#define CONST_2_0     2.0
#define CONST_300_0   300.0
#define CONST_PI      3.141592653589793
#define CONST_20      20.0
#define CONST_6       6.0
#define CONST_3_0     3.0
#define CONST_40      40.0
#define CONST_30      30.0
#define CONST_12      12.0
#define CONST_150     150.0

/**
 * FPU_Compute_Engine_B — 主计算流水线 (变体 B)
 *
 * @param fa  s0 → s16 (d8 low)  输入浮点数 A
 * @param fb  s1 → s17 (d8 high) 输入浮点数 B
 * @param fc  s2 → s20 (d10 low) 输入浮点数 C
 * @param fd  s3 → s21 (d10 high)输入浮点数 D
 * @param ge  s28 (d14 low)  隐式参数 E
 * @param gf  s29 (d14 high) 隐式参数 F
 * @param gg  s30 (d15 low)  隐式参数 G
 * @param gh  s31 (d15 high) 隐式参数 H
 * @return     double in s0:s1 (from d9 累加器)
 */
double FPU_Compute_Engine_B(float fa, float fb, float fc, float fd,
                            float ge, float gf, float gg, float gh)
{
    double d8, d9, d10, d11, d12, d13, d14, d15;
    double tmp_a, tmp_b;
    int    r4, r5;

    /* ---- 输入映射 (0x08013900-0x0801390E) ---- */
    d8  = *(double *)&(float[2]){fa, fb};   /* s16=fa, s17=fb */
    d10 = *(double *)&(float[2]){fc, fd};   /* s20=fc, s21=fd */

    /* ---- 阶段 0: 初始 int 转换 (0x08013910-0x08013928) ---- */
    r5 = func_1850C(fa, fb);                /* s0=s16=fa, s1=s17=fb */
    r4 = func_18E58(fa, fb);
    d11 = func_873A((int16_t)r4);

    /* ================================================================
     * 阶段 1: 初始累积链 → d9 (0x0801392C-0x080139E0)
     *
     * d9 = (d8+300) + 2*d10 + 0.1*d8^2 + 0.1*d8*d10 + 0.1*d11
     * ================================================================ */

    /* 0x0801392C: VLDR 0.1; 0x08013934: func_8578(d11, 0.1) → [sp+0x10] */
    tmp_a = func_8578(d11, CONST_0_1);        /* d11 * 0.1 */

    /* 0x08013938: VLDR 0.1; 0x08013940: STRD [sp+0x10] */
    /* 0x08013944: VMOV r0,r1,d8; 0x08013948: func_8578(d8, 0.1) → d13 */
    d13 = func_8578(d8, CONST_0_1);           /* d8 * 0.1 */

    /* 0x08013950: VMOV r2,r3,d10; 0x08013954: func_8578(d8*0.1, d10) → d11 */
    d11 = func_8578(d13, d10);               /* 0.1*d8*d10 */

    /* 0x0801395C: VLDR 0.1; 0x08013968: func_8578(d8, 0.1) → d14 */
    d14 = func_8578(d8, CONST_0_1);           /* d8 * 0.1 */

    /* 0x08013970: VMOV r2,r3,d8; 0x08013974: func_8578(d8*0.1, d8) → [sp] */
    /* 0x08013980: STRD r0,r1,[sp] — save 0.1*d8^2 */
    /* 0x08013978: VMOV r2,r3,d10; 0x0801397C: VLDR 2.0 */
    /* 0x08013984: VMOV r0,r1,2.0; 0x08013988: func_8578(2.0, d10) → d14 */
    tmp_b = func_8578(d14, d8);               /* 0.1*d8 * d8 = 0.1*d8^2 */
    d14   = func_8578(CONST_2_0, d10);        /* 2.0 * d10 */

    /* 0x08013990: VLDR 300.0; 0x0801399C: func_842A(d8, 300.0) → d15 */
    d15 = func_842A(d8, CONST_300_0);         /* d8 + 300 */

    /* 0x080139A4: VMOV r2,r3,d14; 0x080139A8: func_842A(d15, 2.0*d10) → d13 */
    d13 = func_842A(d15, d14);                /* d8+300 + 2.0*d10 */

    /* 0x080139B0: VLDR [sp]; 0x080139B8: func_842A(d13, d8^2) → d12 */
    /* Note: r2,r3 gets [sp]=d8^2, r0:r1=d13 from previous call */
    d12 = func_842A(d13, tmp_b);              /* + d8^2 */

    /* 0x080139C0: VMOV r2,r3,d11; 0x080139C4: func_842A(d12, d11) → [sp+8] */
    tmp_b = func_842A(d12, d11);              /* + 0.1*d8*d10 */

    /* 0x080139CC: VLDR [sp+0x10] (d11*0.1); 0x080139DC: func_842A([sp+8], [sp+0x10]) → d9 */
    d9 = func_842A(tmp_b, tmp_a);             /* + d11*0.1 */

    /* ================================================================
     * 子块 1: PI 链 + func_18E48(s28,s29) (0x080139E4-0x08013A4E)
     *
     * d15 = d8*2.0, d14 = d8*2.0*PI, d13 = func_873A(func_18E48(ge,gf)),
     * d12 = d13*20.0, d15 = d8*6.0, d14 = d8*6.0*PI
     * ================================================================ */

    /* 0x080139E4: VLDR 2.0; 0x080139F0: func_8578(d8, 2.0) → d15 */
    d15 = func_8578(d8, CONST_2_0);           /* d8 * 2.0 */

    /* 0x080139F8: VLDR PI; 0x08013A00: func_8578(d15, PI) → d14 */
    d14 = func_8578(d15, CONST_PI);           /* d8*2.0*PI */

    /* 0x08013A08: VMOV s0,s28; VMOV s1,s29 */
    /* 0x08013A10: BL func_18E48 → r4 */
    /* 0x08013A16: BL func_873A → d13 */
    /* 0x08013A1E: VLDR 20.0; 0x08013A26: func_8578(d13, 20.0) → d12 */
    r4  = func_18E48(ge, gf);
    d12 = func_8578(func_873A((int16_t)r4), CONST_20);

    /* 0x08013A2E: VLDR 6.0; 0x08013A3A: func_8578(d8, 6.0) → d15 */
    d15 = func_8578(d8, CONST_6);             /* d8 * 6.0 */

    /* 0x08013A42: VLDR PI; 0x08013A4A: func_8578(d15, PI) → d14 */
    d14 = func_8578(d15, CONST_PI);           /* d8*6.0*PI */

    /* ================================================================
     * 子块 2: func_18E48(s28,s29) → 累积到 d9 (0x08013A52-0x08013AC4)
     *
     * func_873A(r4)*20 + d12 → *2.0 → func_865C(*,3.0) → +d9 → d9
     * ================================================================ */

    /* 0x08013A52: VMOV s0,s28; VMOV s1,s29 */
    /* 0x08013A5A: BL func_18E48 → r4; 0x08013A60: BL func_873A → d13 */
    /* 0x08013A68: VLDR 20.0; 0x08013A70: func_8578(d13, 20.0) → [sp] */
    r4    = func_18E48(ge, gf);
    tmp_a = func_8578(func_873A((int16_t)r4), CONST_20);

    /* 0x08013A74: VMOV r2,r3,d12; 0x08013A84: func_842A([sp], d12) → d11 */
    d11 = func_842A(tmp_a, d12);              /* new*20 + prev*20 */

    /* 0x08013A8C: VLDR 2.0; 0x08013A94: func_8578(d11, 2.0) → [sp+8] */
    /* 0x08013A98: VLDR 3.0; 0x08013AAC: func_865C([sp+8], 3.0) → [sp+0x10] */
    /* 0x08013AB0: VMOV r2,r3,d9; 0x08013AC0: func_842A([sp+0x10], d9) → d9 */
    tmp_a = func_8578(d11, CONST_2_0);
    tmp_b = func_865C(tmp_a, CONST_3_0);
    d9    = func_842A(tmp_b, d9);

    /* ================================================================
     * 子块 3: func_865C 链 + 双路 func_18E48 (s28,s29 和 s30,s31)
     *          (0x08013AC8-0x08013B98)
     *
     * A路: func_865C(d8,3.0)*PI → func_18E48(s28,s29)→func_873A*40→d12
     * B路: d8*PI → func_18E48(s30,s31)→func_873A*20 → +d12 → *2*func_865C(*,3)+d9→d9
     * ================================================================ */

    /* 0x08013AC8: VLDR 3.0; 0x08013AD4: func_865C(d8, 3.0) → d15 */
    /* 0x08013ADC: VLDR PI; 0x08013AE4: func_8578(d15, PI) → d14 */
    d14 = func_8578(func_865C(d8, CONST_3_0), CONST_PI);

    /* 0x08013AEC: VMOV s0,s28; VMOV s1,s29 */
    /* 0x08013AF4: BL func_18E48 → r4; 0x08013AFA: BL func_873A → [sp] */
    /* 0x08013AFE: VLDR 40.0; 0x08013B12: func_8578([sp], 40.0) → d12 */
    r4  = func_18E48(ge, gf);
    d12 = func_8578(func_873A((int16_t)r4), CONST_40);

    /* 0x08013B1A: VLDR PI; 0x08013B26: func_8578(d8, PI) → d15 */
    d15 = func_8578(d8, CONST_PI);            /* d8 * PI */

    /* 0x08013B2E: VMOV s0,s30; VMOV s1,s31 — 唯一使用 s30:s31 的位置 */
    /* 0x08013B36: BL func_18E48 → r4; 0x08013B3C: BL func_873A → d14 */
    /* 0x08013B44: VLDR 20.0; 0x08013B4C: func_8578(d14, 20.0) → d13 */
    r4  = func_18E48(gg, gh);
    tmp_a = func_8578(func_873A((int16_t)r4), CONST_20);

    /* 0x08013B54: VMOV r2,r3,d12; 0x08013B58: func_842A(d13*20, d12*40) → d11 */
    d11 = func_842A(tmp_a, d12);

    /* 累积: *2.0 → func_865C(*,3.0) → +d9 → d9 (0x08013B60-0x08013B98) */
    /* 0x08013B60: VLDR 2.0; 0x08013B68: func_8578(d11, 2.0) → [sp+8] */
    /* 0x08013B6C: VLDR 3.0; 0x08013B80: func_865C([sp+8], 3.0) → [sp+0x10] */
    /* 0x08013B84: VMOV r2,r3,d9; 0x08013B94: func_842A([sp+0x10], d9) → d9 */
    tmp_a = func_8578(d11, CONST_2_0);
    tmp_b = func_865C(tmp_a, CONST_3_0);
    d9    = func_842A(tmp_b, d9);

    /* ================================================================
     * 子块 4: 双路 func_865C + 双路 func_18E48(s28,s29)
     *          (0x08013B9C-0x08013C78)
     *
     * A路: func_865C(d8,30)*PI, func_18E48(s28,s29)→func_873A*300→[sp+8]
     * B路: func_865C(d8,12)*PI, func_18E48(s28,s29)→func_873A*150→[sp]
     * 合并: [sp]+[sp+8] → *2*func_865C(*,3)+d9→d9
     * ================================================================ */

    /* 0x08013B9C: VLDR 30.0; 0x08013BA8: func_865C(d8, 30.0) → d15 */
    /* 0x08013BB0: VLDR PI; 0x08013BB8: func_8578(d15, PI) → d14 */
    d14 = func_8578(func_865C(d8, CONST_30), CONST_PI);

    /* 0x08013BC0: VMOV s0,s28; VMOV s1,s29 */
    /* 0x08013BC8: BL func_18E48 → r4; 0x08013BCE: BL func_873A → d13 */
    /* 0x08013BD6: VLDR 300.0; 0x08013BDE: func_8578(d13, 300.0) → [sp+8] */
    r4    = func_18E48(ge, gf);
    tmp_a = func_8578(func_873A((int16_t)r4), CONST_300_0);

    /* 0x08013BE2: VLDR 12.0; 0x08013BF2: func_865C(d8, 12.0) → d15 */
    /* 0x08013BFA: VLDR PI; 0x08013C02: func_8578(d15, PI) → d14 */
    d14 = func_8578(func_865C(d8, CONST_12), CONST_PI);

    /* 0x08013C0A: VMOV s0,s28; VMOV s1,s29 */
    /* 0x08013C12: BL func_18E48 → r4; 0x08013C18: BL func_873A → d13 */
    /* 0x08013C20: VLDR 150.0; 0x08013C28: func_8578(d13, 150.0) → [sp] */
    r4    = func_18E48(ge, gf);
    tmp_b = func_8578(func_873A((int16_t)r4), CONST_150);

    /* 0x08013C30: VLDR [sp+8]; 0x08013C34: VMOV r2,r3,[sp+8]=tmp_a */
    /* 0x08013C38: VLDR [sp]; 0x08013C3C: VMOV r0,r1,[sp]=tmp_b */
    /* 0x08013C40: BL func_842A → func_842A(tmp_b, tmp_a) → d12 */
    d12 = func_842A(tmp_b, tmp_a);            /* 150*N2 + 300*N1 */

    /* 累积: *2.0 → func_865C(*,3.0) → +d9 → d9 (0x08013C48-0x08013C78) */
    /* 0x08013C48: VLDR 2.0; 0x08013C50: func_8578(d12, 2.0) → [sp+0x10] */
    /* 0x08013C54: VLDR 3.0; 0x08013C68: func_865C([sp+0x10], 3.0) → d11 */
    /* 0x08013C70: VMOV r2,r3,d9; 0x08013C74: func_842A(d11, d9) → d9 */
    tmp_a = func_8578(d12, CONST_2_0);
    d11   = func_865C(tmp_a, CONST_3_0);
    d9    = func_842A(d11, d9);

    /* ---- 返回: s0,s1 ← d9 (s18,s19) (0x08013C7C-0x08013C8A) ---- */
    /* 0x08013C7C: vmov.f32 s0,s18; vmov.f32 s1,s19 */
    /* 0x08013C84: add sp,#0x1c; vpop d8-d15; pop {r4,r5,pc} */
    return d9;
}
