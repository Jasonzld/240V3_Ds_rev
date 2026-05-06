/**
 * @file func_143_fpu_sign_check_wrapper.c
 * @brief 函数: FPU 符号检查包装器 — VMOV d0,r0,r1 → BL func_128 → VMOV r0,r1,d0
 * @addr  0x08018414 - 0x08018424 (16 bytes)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 参数: r0,r1 = 64-bit double 值
 * 返回: r0,r1 = func_128 返回的 double
 *
 * 调用:
 *   func_16BD8() @ 0x08016BD8 — FPU 符号检查与分支 (func_128)
 */

#include "stm32f4xx_hal.h"

extern double func_16BD8(double val);

/*
 * 0x08018416: VMOV d0, r0, r1 → 将 r0:r1 直接映射为 double
 * 0x0801841A: BL func_16BD8 → r2 未设置, flags 来自调用方传参
 * 0x0801841E: VMOV r0, r1, d0 → 返回 double 到 r0:r1
 */
double FPU_Sign_Check_Wrapper(double val)
{
    return func_16BD8(val);
}
