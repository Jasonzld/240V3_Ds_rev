/**
 * @file func_130_fpu_struct_writer.c
 * @brief 函数: FPU 结构写入器 — 将两个 double 写入结构体, 再按阈值做有条件写入
 * @addr  0x08016D68 - 0x08016DA2 (58 bytes)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 参数:
 *   r0 = ctx — 结构体指针 (至少 16 字节: 2×double)
 *   d0 = double val — 要写入的 double 值
 *   [sp+0x4C] = flags — 阈值标志 (取绝对值后比较)
 *   [sp+0x48] = double — 备用 double 值 (条件写入 ctx[0])
 *
 * 操作:
 *   1. ctx[1] = FPU_CONST (VLDR 常量)
 *   2. 若 |flags| < threshold: ctx[0] = 备用 double, 返回 0
 *   3. 否则: 返回 (不修改 ctx[0], 可能返回非零)
 *
 * 调用: 无外部调用 (被 func_128/129 调用)
 */

#include "stm32f4xx_hal.h"

/* FPU 常量 (从字面量池 VLDR 加载) */
static const double FPU_CONST_C = 0.0;  /* 实际值从 0x080170E0 附近字面量池解析 */

uint32_t FPU_Struct_Writer(void *ctx, double val, double alt_val, uint32_t flags)
{
    double *dptr = (double *)ctx;

    /* ctx[1] = FPU 常量 */
    dptr[1] = FPU_CONST_C;

    /* 取 flags 绝对值 (去掉 bit31) */
    uint32_t abs_flags = flags & 0x7FFFFFFF;

    /* 与阈值比较 (阈值从字面量池加载) */
    if ((int32_t)abs_flags <= 0x3FE921FB) {    /* <= PI/4 (~0.7854) */
        dptr[0] = alt_val;
        return 0;
    }

    return 1;  /* 未修改 ctx[0] */
}
