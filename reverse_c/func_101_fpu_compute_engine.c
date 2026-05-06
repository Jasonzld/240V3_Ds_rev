/**
 * @file func_101_fpu_compute_engine.c
 * @brief 函数: FPU 计算引擎 — 多级双精度浮点累加流水线
 * @addr  0x08013544 - 0x080138F8 (948 bytes, 含字面量池)
 *         代码 0x08013544-0x08013896 (850 bytes)
 *         字面量池 0x08013898-0x080138F7 (96 bytes, 12 个双精度常量)
 * DISASSEMBLY-TRACED. Verified against Capstone (inline trace annotations).
 *
 * 6 个浮点输入:
 *   s0-s3  (d8,d9)  — 4 个显式 float 参数 (fa, fb, fc, fd)
 *   s28-s29 (d14)   — 2 个隐式 float 参数 (caller-saved FPU context)
 *   s30-s31 (d15)   — 2 个隐式 float 参数
 *
 * 5 个几乎相同的子块，每块:
 *   1. vmov s28/s29 → s0/s1
 *   2. func_18E48(s0,s1) → int (r4)
 *   3. func_873A((int16_t)r4) → double
 *   4. VLDR double const → func_8578(mul) / func_842A(add) / func_865C(scale)
 *
 * 最终返回值: double in s0:s1 (来自 d10 累加器)
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

#define CONST_0_2     0.2
#define CONST_0_1     0.1
#define CONST_3_0     3.0
#define CONST_2_0     2.0
#define CONST_N100    (-100.0)
#define CONST_PI      3.141592653589793
#define CONST_20      20.0
#define CONST_6       6.0
#define CONST_40      40.0
#define CONST_30      30.0
#define CONST_12      12.0
#define CONST_160     160.0

/**
 * FPU_Compute_Engine — 主计算流水线
 *
 * @param fa  s0 → s18 (d9 low)  输入浮点数 A
 * @param fb  s1 → s19 (d9 high) 输入浮点数 B
 * @param fc  s2 → s16 (d8 low)  输入浮点数 C
 * @param fd  s3 → s17 (d8 high) 输入浮点数 D
 * @param ge  s28 (d14 low)  隐式参数 E (callee-saved, caller 设置)
 * @param gf  s29 (d14 high) 隐式参数 F
 * @param gg  s30 (d15 low)  隐式参数 G
 * @param gh  s31 (d15 high) 隐式参数 H
 * @return     double in s0:s1 (from d10 累加器)
 */
double FPU_Compute_Engine(float fa, float fb, float fc, float fd,
                          float ge, float gf, float gg, float gh)
{
    double d8, d9, d10, d11, d12, d13, d14, d15;
    double tmp_a, tmp_b, tmp_c;
    int    r4, r5;

    /* ---- 输入映射 ---- */
    d8  = *(double *)&(float[2]){fc, fd};   /* s16=fc, s17=fd */
    d9  = *(double *)&(float[2]){fa, fb};   /* s18=fa, s19=fb */
    d14 = *(double *)&(float[2]){ge, gf};   /* s28=ge, s29=gf */
    d15 = *(double *)&(float[2]){gg, gh};   /* s30=gg, s31=gh */

    /* ---- 阶段 0: 初始 int 转换 ---- */
    /* 0x0801355C-0x0801356E */
    r5 = func_1850C(fa, fb);                /* s0=s18=fa, s1=s19=fb */
    r4 = func_18E58(fa, fb);                /* s0/s1 unchanged */

    /* ---- 阶段 1: d11 = func_873A(r4) * 0.2 ---- */
    /* 0x08013570-0x08013580 */
    d11 = func_8578(func_873A((int16_t)r4), CONST_0_2);

    /* ---- 阶段 2: d12 = d8 * d9 * 0.1 ---- */
    /* 0x08013584-0x0801359C */
    d12 = func_8578(func_8578(d9, CONST_0_1), d8);

    /* ---- 阶段 3: [sp+8]=0.2*d8^2, d13=3.0*d8 ---- */
    /* 0x080135A0-0x080135CC */
    tmp_a = func_8578(CONST_0_2, d8);       /* 0.2 * d8 */
    tmp_a = func_8578(tmp_a, d8);           /* 0.2 * d8^2 → [sp+8] */
    d13   = func_8578(CONST_3_0, d8);       /* 3.0 * d8 */

    /* ---- 阶段 4: [sp]=2*d9 - 100 + 3*d8 ---- */
    /* 0x080135D0-0x080135F4 */
    tmp_b = func_842A(func_8578(d9, CONST_2_0), CONST_N100);  /* 2*d9-100 */
    tmp_b = func_842A(tmp_b, d13);                             /* + 3*d8 → [sp] */

    /* ---- 阶段 5: d10 = 累加([sp+8] + [sp] + d12 + d11) ---- */
    /* 0x080135F8-0x08013634 */
    tmp_c = func_842A(tmp_b, tmp_a);          /* [sp] + [sp+8] */
    tmp_c = func_842A(tmp_c, d12);            /* + d12 → [sp+0x10] */
    tmp_c = func_842A(tmp_c, d11);            /* + d11 → [sp+0x18] */
    d10   = func_842A(tmp_c, d11);            /* wait: assembly does [sp+0x18]+d11 */
    /* Correction: the assembly does:
     * [sp+0x10] = [sp]+[sp+8]; then [sp+0x10]+d12; then [sp+0x18]+d11 → d10
     * Let me re-trace more carefully from the disassembly:
     * 0x080135F8: VLDR d0,[sp,#8]    ; d0 = [sp+8] = 0.2*d8^2
     * 0x080135FC: VMOV r2,r3,d0       ; r2:r3 = [sp+8]
     * 0x08013600: VLDR d0,[sp,#0]     ; d0 = [sp]
     * 0x08013604: VMOV r0,r1,d0       ; r0:r1 = [sp]
     * 0x08013608: BL func_842A        ; [sp] + [sp+8]
     * 0x0801360C: VMOV r2,r3,d12      ; r2:r3 = d12
     * 0x08013610: STRD r0,r1,[sp,#0x10] ; [sp+0x10] = [sp]+[sp+8]
     * 0x08013614: VLDR d0,[sp,#0x10]
     * 0x08013618: VMOV r0,r1,d0       ; r0:r1 = [sp+0x10]
     * 0x0801361C: BL func_842A        ; [sp+0x10] + d12
     * 0x08013620: VMOV r2,r3,d11      ; r2:r3 = d11
     * 0x08013624: STRD r0,r1,[sp,#0x18] ; [sp+0x18] = [sp+0x10]+d12
     * 0x08013628: VLDR d0,[sp,#0x18]
     * 0x0801362C: VMOV r0,r1,d0       ; r0:r1 = [sp+0x18]
     * 0x08013630: BL func_842A        ; [sp+0x18] + d11
     * 0x08013634: VMOV d10,r0,r1      ; d10 = [sp+0x18] + d11
     */
    /* Re-corrected computation: */
    d10 = func_8578(d9, CONST_2_0);          /* d9*2 */
    d10 = func_842A(d10, CONST_N100);        /* d9*2 - 100 */
    d10 = func_842A(d10, d13);               /* + 3*d8 */
    d10 = func_842A(d10, tmp_a);             /* + 0.2*d8^2 */
    d10 = func_842A(d10, d12);               /* + d8*d9*0.1 */
    d10 = func_842A(d10, d11);               /* + func_873A(r4)*0.2 */

    /* ---- 子块 1 (0x08013638-0x08013654): d14 = d9*2*PI ---- */
    /* 0x08013638: VLDR d0 → 2.0; VMOV r2,r3,d0 */
    /* 0x08013640: VMOV r0,r1,d9 */
    /* 0x08013644: BL func_8578 → d9*2.0 */
    /* 0x08013648: VLDR d0 → PI; VMOV r2,r3,d0 */
    /* 0x08013650: BL func_8578 → d9*2.0*PI */
    /* 0x08013654: VMOV d14,r0,r1 */
    d14 = func_8578(func_8578(d9, CONST_2_0), CONST_PI);

    /* ---- 子块 1b (0x08013658-0x08013672): *20 保存, d9*6*PI ---- */
    /* 0x08013658: VMOV s0,s28; VMOV s1,s29 */
    /* 0x08013660: BL func_18E48 → r4 */
    /* 0x08013666: BL func_873A → double */
    /* 0x0801366A: VLDR d0 → 20.0; VMOV r2,r3,d0 */
    /* 0x08013672: BL func_8578 → func_873A(r4)*20.0 */
    /* 0x08013676: VLDR d0 → 6.0; VMOV r2,r3,d0 */
    /* 0x0801367E: STRD r0,r1,[sp,#0x10] — save func_873A(r4)*20.0 */
    /* 0x08013682: VMOV r0,r1,d9 */
    /* 0x08013686: BL func_8578 → d9*6.0 */
    /* 0x0801368A: VLDR d0 → PI; VMOV r2,r3,d0 */
    /* 0x08013692: BL func_8578 → d9*6.0*PI */
    /* 0x08013696: VMOV d14,r0,r1 */
    tmp_a = func_8578(func_873A((int16_t)func_18E48(ge, gf)), CONST_20);
    d14   = func_8578(func_8578(d9, CONST_6), CONST_PI);

    /* ---- 子块 2 (0x0801369A-0x080136FC): d10 第二级累加 ---- */
    /* 0x0801369A: VMOV s0,s28; VMOV s1,s29 */
    /* 0x080136A2: BL func_18E48 → r4 */
    /* 0x080136A8: BL func_873A → double */
    /* 0x080136AC: VLDR d0 → 20.0; VMOV r2,r3,d0 */
    /* 0x080136B4: BL func_8578 → *20.0 → [sp+8] */
    /* 0x080136BC-0x080136CC: [sp+0x10]+[sp+8] → func_842A → *2.0 → *3.0(via func_865C) → +d10 */
    /* 0x080136D0: VLDR d0 → 2.0; VMOV r2,r3,d0 */
    /* 0x080136D8: BL func_8578 */
    /* 0x080136DC: VLDR d0 → 3.0; VMOV r2,r3,d0 */
    /* 0x080136E4: BL func_865C */
    /* 0x080136E8: VMOV r2,r3,d10 */
    /* 0x080136EC: STRD r0,r1,[sp,#0x18] */
    /* 0x080136F0: VLDR d0,[sp,#0x18]; VMOV r0,r1,d0 */
    /* 0x080136F8: BL func_842A → ... + d10 */
    /* 0x080136FC: VMOV d10,r0,r1 */
    r4   = func_18E48(ge, gf);
    tmp_b = func_8578(func_873A((int16_t)r4), CONST_20);   /* [sp+8] */
    tmp_c = func_842A(tmp_b, tmp_a);                        /* [sp+8]+[sp+0x10] */
    tmp_c = func_8578(tmp_c, CONST_2_0);
    tmp_c = func_865C(tmp_c, CONST_3_0);
    d10   = func_842A(tmp_c, d10);

    /* ---- 子块 3 (0x08013700-0x080137C0): func_865C(d8,3.0)*PI + *40 + PI*d8 + *20+d12 + *2*3+func_865C + d10 ---- */
    /* 0x08013700: VLDR d0 → 3.0 */
    /* 0x08013708: VMOV r0,r1,d8 */
    /* 0x0801370C: BL func_865C → func_865C(d8,3.0) */
    /* 0x08013710: VLDR d0 → PI */
    /* 0x08013718: BL func_8578 → *PI */
    /* 0x0801371C: VMOV d14,r0,r1 */
    /* 0x08013720: VMOV s0,s28; VMOV s1,s29 */
    /* 0x08013728: BL func_18E48 → r4 */
    /* 0x0801372E: BL func_873A → double */
    /* 0x08013732: VLDR d0 → 40.0 */
    /* 0x0801373A: BL func_8578 → func_873A(r4)*40.0 */
    /* 0x0801373E: VMOV d12,r0,r1 */
    /* 0x08013742: VLDR d0 → PI */
    /* 0x0801374A: VMOV r0,r1,d8 */
    /* 0x0801374E: BL func_8578 → PI*d8 */
    /* 0x08013752: VMOV d14,r0,r1 */
    d14 = func_8578(func_865C(d8, CONST_3_0), CONST_PI);
    r4  = func_18E48(ge, gf);
    d12 = func_8578(func_873A((int16_t)r4), CONST_40);
    d14 = func_8578(d8, CONST_PI);

    /* ---- 子块 3b (0x08013756-0x080137C0): *20 + d12 → *2 → *3 → +d10 ---- */
    /* 0x08013756: VMOV s0,s28; VMOV s1,s29 */
    /* 0x0801375E: BL func_18E48 → r4 */
    /* 0x08013764: BL func_873A → double */
    /* 0x08013768: VLDR d0 → 20.0 */
    /* 0x08013770: BL func_8578 → *20.0 */
    /* 0x08013774: VMOV r2,r3,d12 */
    /* 0x08013778: STRD r0,r1,[sp,#8] — save */
    /* 0x0801377C: VLDR d0,[sp,#8] */
    /* 0x08013780: VMOV r0,r1,d0 */
    /* 0x08013784: BL func_842A → + d12 → [sp+8] updated */
    /* 0x08013788: VLDR d0 → 2.0 */
    /* 0x08013790: BL func_8578 → *2.0 */
    /* 0x08013794: VLDR d0 → 3.0 */
    /* 0x08013798: STRD r0,r1,[sp,#0x10] */
    /* 0x080137A0: VLDR d0,[sp,#0x10] */
    /* 0x080137A4: VMOV r0,r1,d0 */
    /* 0x080137A8: BL func_865C → func_865C(..., 3.0) */
    /* 0x080137AC: VMOV r2,r3,d10 */
    /* 0x080137B0: STRD r0,r1,[sp,#0x18] */
    /* 0x080137B4: VLDR d0,[sp,#0x18] */
    /* 0x080137B8: VMOV r0,r1,d0 */
    /* 0x080137BC: BL func_842A → + d10 */
    /* 0x080137C0: VMOV d10,r0,r1 */
    r4   = func_18E48(ge, gf);
    tmp_b = func_8578(func_873A((int16_t)r4), CONST_20);
    tmp_b = func_842A(tmp_b, d12);
    tmp_b = func_8578(tmp_b, CONST_2_0);
    tmp_b = func_865C(tmp_b, CONST_3_0);
    d10   = func_842A(tmp_b, d10);

    /* ---- 子块 4 (0x080137C4-0x08013828): PI*d8 *30 + *5<<6 *12 + func_865C(d8,12)*PI + d15 ---- */
    /* 0x080137C4: VLDR d0 → PI */
    /* 0x080137CC: VMOV r0,r1,d8 */
    /* 0x080137D0: BL func_8578 → PI*d8 */
    /* 0x080137D4: VLDR d0 → 30.0 */
    /* 0x080137DC: BL func_865C → func_865C(PI*d8, 30.0) */
    /* 0x080137E0: VMOV d14,r0,r1 */
    /* 0x080137E4: VMOV s0,s28; VMOV s1,s29 */
    /* 0x080137EC: BL func_18E48 → r4 */
    /* 0x080137F0: ADD.W r0,r0,r0,LSL#2 → r0*5 */
    /* 0x080137F4: LSLS r4,r0,#6 → r4 = r0*5 << 6 */
    /* 0x080137F6: MOV r0,r4 — transfer r4 to r0 for call */
    /* 0x080137F8: BL func_873A → double */
    /* 0x080137FC: VLDR d0 → 12.0 */
    /* 0x08013800: VMOV r2,r3,d0 */
    /* 0x08013804: STRD r0,r1,[sp,#0x10] — save func_873A */
    /* 0x08013808: VMOV r0,r1,d8 */
    /* 0x0801380C: BL func_865C → func_865C(d8, 12.0) */
    /* 0x08013810: VLDR d0 → PI */
    /* 0x08013818: STRD r0,r1,[sp,#8] */
    /* 0x0801381C: VLDR d0,[sp,#8] */
    /* 0x08013820: VMOV r0,r1,d0 */
    /* 0x08013824: BL func_8578 → *PI */
    /* 0x08013828: VMOV d15,r0,r1 */
    d14 = func_865C(func_8578(d8, CONST_PI), CONST_30);
    r4  = func_18E48(ge, gf);
    r4  = r4 * 5;                              /* ADD r0,r0,r0,LSL#2 = r0*5 */
    r4  = r4 << 6;                             /* LSLS r4,r0,#6 */
    tmp_a = func_873A((int16_t)r4);            /* [sp+0x10] */
    tmp_b = func_865C(d8, CONST_12);
    d15   = func_8578(tmp_b, CONST_PI);

    /* ---- 子块 5 (0x0801382C-0x08013882): s30/s31, *160, +[sp+0x10], *2, *3 via func_865C, +d10 ---- */
    /* 0x0801382C: VMOV s0,s30; VMOV s1,s31 */
    /* 0x08013834: BL func_18E48 → r4 */
    /* 0x0801383A: BL func_873A → double */
    /* 0x0801383E: VLDR d0 → 160.0 */
    /* 0x08013846: BL func_8578 → *160.0 */
    /* 0x0801384A: VLDR d0,[sp,#0x10] — load saved func_873A(r4*5<<6) */
    /* 0x0801384E: VMOV r2,r3,d0 */
    /* 0x08013852: BL func_842A → + [sp+0x10] */
    /* 0x08013856: VLDR d0 → 2.0 */
    /* 0x0801385E: STRD r0,r1,[sp,#0x18] */
    /* 0x08013862: VLDR d0,[sp,#0x18] */
    /* 0x08013866: VMOV r0,r1,d0 */
    /* 0x0801386A: BL func_8578 → *2.0 */
    /* 0x0801386E: VLDR d0 → 3.0 */
    /* 0x08013876: BL func_865C → func_865C(..., 3.0) */
    /* 0x0801387A: VMOV r2,r3,d10 */
    /* 0x0801387E: BL func_842A → + d10 */
    /* 0x08013882: VMOV d10,r0,r1 */
    r4   = func_18E48(gg, gh);
    tmp_b = func_8578(func_873A((int16_t)r4), CONST_160);
    tmp_c = func_842A(tmp_b, tmp_a);           /* + saved func_873A(r4*5<<6) */
    tmp_c = func_8578(tmp_c, CONST_2_0);
    tmp_c = func_865C(tmp_c, CONST_3_0);
    d10   = func_842A(tmp_c, d10);

    /* ---- 返回: s0 = d10.low (s20), s1 = d10.high (s21) ---- */
    /* 0x08013886: VMOV s0,s20; VMOV s1,s21 */
    /* 0x0801388E: ADD sp,#0x24; VPOP d8-d15; POP {r4,r5,pc} */
    return d10;
}
