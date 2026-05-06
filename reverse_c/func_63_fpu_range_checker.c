/**
 * @file func_63_fpu_range_checker.c
 * @brief FPU 范围检查器 — 检查两对浮点值是否在指定范围外
 * @addr  0x08010B3C - 0x08010BD0 (148 bytes, 含字面量池)
 *         字面量池 0x08010BAC-0x08010BCB (32 bytes, 4 个 double 常量)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 比较 d9 与边界 [72.004, 137.8347), d8 与边界 [0.8293, 55.8271).
 * 返回 0 仅当 d9 >= 137.8347 且 d8 >= 55.8271，其余情况返回 1。
 *
 * 参数 (通过 FPU 寄存器传递):
 *   s0/s1 = float pair 1 → d8 (s16, s17)
 *   s2/s3 = float pair 2 → d9 (s18, s19)
 * 返回:
 *   r0 = 1 若超出范围, 0 若在范围内
 *
 * 调用:
 *   func_87DC() @ 0x080087DC — 双字无符号比较 (BLO: val < limit)
 *   func_880C() @ 0x0800880C — 双字无符号比较 (BHS: val >= limit)
 *
 * 字面量池 (IEEE 754 binary64, 已与二进制逐字节核对):
 *   0x08010BAC: 0x4052004189374BC7 = 72.004         (LIMIT_A: d9 下边界)
 *   0x08010BB4: 0x40613AB5DCC63F14 = 137.8347       (LIMIT_B: d9 上边界)
 *   0x08010BBC: 0x3FEA89A027525461 = 0.8293         (LIMIT_C: d8 下边界)
 *   0x08010BC4: 0x404BE9DE69AD42C4 = 55.8271        (LIMIT_D: d8 上边界)
 */

#include "stm32f4xx_hal.h"

extern int func_87DC(int64_t val, int64_t limit);   /* 比较: val < limit → BLO */
extern int func_880C(int64_t val, int64_t limit);   /* 比较: val >= limit → BHS */

/* FPU 双精度常量 — 从字面量池提取, IEEE 754 binary64 (已与二进制逐字节核对) */
static const double FPU_LIMIT_D9_LO = 72.004;       /* @ 0x10BAC: 0x4052004189374BC7 */
static const double FPU_LIMIT_D9_HI = 137.8347;     /* @ 0x10BB4: 0x40613AB5DCC63F14 */
static const double FPU_LIMIT_D8_LO = 0.8293;       /* @ 0x10BBC: 0x3FEA89A027525461 */
static const double FPU_LIMIT_D8_HI = 55.8271;      /* @ 0x10BC4: 0x404BE9DE69AD42C4 */

/* ARM hard-float ABI: fa/fb/fc/fd arrive in s0/s1/s2/s3.
 * The original code does VMOV.F32 s16,s0; s17,s1; s18,s2; s19,s3
 * to pair them into doubles d8={s16,s17} and d9={s18,s19}.
 * On little-endian ARM, the first float is the low 32 bits of the double. */
typedef union { struct { float lo; float hi; } f; double d; int64_t i64; } f64_t;

/* ================================================================
 * FPU_Range_Checker() @ 0x08010B3C
 *   push {r4, lr}; vpush {d8, d9}
 * ================================================================ */
uint32_t FPU_Range_Checker(float fa, float fb, float fc, float fd)
{
    f64_t d8_val = { .f = {fa, fb} };  /* d8 = {s16, s17} */
    f64_t d9_val = { .f = {fc, fd} };  /* d9 = {s18, s19} */

    /* ---- 检查 d9 ---- */
    /* 0x08010B52-62: vldr d0,=72.004; vmov r2:r3,d0; vmov r0:r1,d9; bl func_87DC; blo return_1 */
    if (func_87DC(d9_val.i64, *(int64_t *)&FPU_LIMIT_D9_LO)) {
        return 1;   /* d9 < 72.004 → 超出下边界 */
    }

    /* 0x08010B64-74: vldr d0,=137.8347; vmov r2:r3,d0; vmov r0:r1,d9; bl func_880C; bhs check_d8 */
    if (func_880C(d9_val.i64, *(int64_t *)&FPU_LIMIT_D9_HI)) {
        /* d9 >= 137.8347 → 继续检查 d8 */
        goto check_d8;
    }

    /* 0x08010B74-76: 72.004 <= d9 < 137.8347 → return 1 */
    return 1;

check_d8:
    /* ---- 检查 d8 ---- */
    /* 0x08010B7E-8E: vldr d0,=0.8293; vmov r2:r3,d0; vmov r0:r1,d8; bl func_87DC; blo return_1 */
    if (func_87DC(d8_val.i64, *(int64_t *)&FPU_LIMIT_D8_LO)) {
        return 1;   /* d8 < 0.8293 → 超出下边界 */
    }

    /* 0x08010B90-A0: vldr d0,=55.8271; vmov r2:r3,d0; vmov r0:r1,d8; bl func_880C; bhs return_0 */
    if (func_880C(d8_val.i64, *(int64_t *)&FPU_LIMIT_D8_HI)) {
        return 0;   /* d8 >= 55.8271 → 在范围内 (唯一返回 0 的路径) */
    }

    /* 0x08010BA2-AA: 0.8293 <= d8 < 55.8271 → return 1 */
    return 1;
}
