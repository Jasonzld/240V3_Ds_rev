/**
 * @file func_161_small_code_fragments.c
 * @brief 小代码片段收集 — SDIV 双向缩放 + FPU 计算链尾部片段
 * @addr  0x080101D0 - 0x08010202 (50 bytes, SDIV 双向缩放函数)
 *        0x0800F490 - 0x0800F4F4 (100 bytes, FPU 计算链尾部片段)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 包含两个代码片段.
 * SDIV 片段是一个独立的小函数; FPU 片段是更大 FPU 计算函数(func_101)的尾部.
 *
 * 注: 所有地址已用 Capstone 验证, 基于 WW240_IAP-V02.0B.bin 反汇编.
 *     二进制基址 0x08000000 (向量表: SP=0x200065B8, Reset=0x080081E1).
 *     之前的地址偏移了 +0x8000, 现已全部修正.
 */

#include "stm32f4xx_hal.h"

/* ---- 外部依赖 ---- */
/*
 * 软件双精度算术 helper. 均为 EABI 调用约定: r0:r1=arg1, r2:r3=arg2, 返回 r0:r1.
 */
extern double dp_arith_add(double a, double b);  /* 0x08000578 — __aeabi_dadd (主引擎) */
extern double dp_arith_sub(double a, double b);  /* 0x0800056C — __aeabi_dsub (a-b, 翻转第二操作数符号后跳转到 0x0800042A) */
extern double dp_arith_sub_r(double a, double b); /* 0x08000572 — __aeabi_dsub 变体 (b-a, 翻转第一操作数符号) */
/*
 * 注: 0x0800056C 和 0x08000572 都是 __aeabi_dsub 的包装:
 *     0x0800056C: eor r3,r3,#0x80000000; b 0x0800042A  (arg2 = -arg2, 然后 arg1 + (-arg2) = arg1 - arg2)
 *     0x08000572: eor r1,r1,#0x80000000; b 0x0800042A  (arg1 = -arg1, 然后 (-arg1) + arg2 = arg2 - arg1)
 */

/* ========================================================================
 * SDIV_Loop_Divider (0x080101D0 - 0x08010202, 50 bytes)
 *
 * 双向十进制缩放器: 通过 SDIV/乘法将 *val 按 10 的幂次缩放,
 * 直到 iterations 达到 cmp_val.
 *
 * 调用约定:
 *   r0 = val_ptr  — 指向待缩放 uint32_t 值的指针 (若为 NULL 则直接返回)
 *   r1 = iterations — 当前指数 (uint8_t)
 *   r2 = cmp_val    — 目标指数 (uint8_t)
 *
 * 操作:
 *   if (*val_ptr == NULL) return;
 *   while (iterations > cmp_val) { *val /= 10; iterations--; }  // 缩小
 *   while (iterations < cmp_val) { *val *= 10; iterations++; }  // 放大
 *
 * 用于十进制格式化: 将整数缩放到合适的数量级以便显示.
 *
 * 反汇编逐条对照:
 *   0x080101D0: push {r4, lr}                 — 保存 r4 和返回地址
 *   0x080101D2: cbnz r0, #0x080101D6           — 若 val_ptr != NULL, 跳转到循环
 *   0x080101D4: pop {r4, pc}                    — NULL 指针: 直接返回
 *   0x080101D6: b #0x080101E6                   — 跳转到除法循环条件
 *   0x080101D8: movs r4, #0xA                   — r4 = 10 (除数)
 *   0x080101DA: ldr r3, [r0]                    — r3 = *val_ptr
 *   0x080101DC: sdiv r3, r3, r4                 — r3 = (int32_t)r3 / 10
 *   0x080101E0: str r3, [r0]                    — *val_ptr = r3
 *   0x080101E2: subs r3, r1, #1                — r3 = iterations - 1
 *   0x080101E4: uxtb r1, r3                     — iterations = (uint8_t)(iterations - 1)
 *   0x080101E6: cmp r1, r2                     — 比较 iterations, cmp_val
 *   0x080101E8: bgt #0x080101D8                 — 若 iterations > cmp_val, 继续除法循环
 *   0x080101EA: b #0x080101FA                   — 跳转到乘法循环条件
 *   0x080101EC: ldr r3, [r0]                    — r3 = *val_ptr
 *   0x080101EE: add.w r3, r3, r3, lsl #2       — r3 = r3 + r3*4 = r3*5
 *   0x080101F2: lsls r3, r3, #1                — r3 = r3*2 => r3 = r3*10
 *   0x080101F4: str r3, [r0]                    — *val_ptr = r3
 *   0x080101F6: adds r3, r1, #1                — r3 = iterations + 1
 *   0x080101F8: uxtb r1, r3                     — iterations = (uint8_t)(iterations + 1)
 *   0x080101FA: cmp r1, r2                     — 比较 iterations, cmp_val
 *   0x080101FC: blt #0x080101EC                 — 若 iterations < cmp_val, 继续乘法循环
 *   0x080101FE: nop                             — 对齐/填充
 *   0x08010200: b #0x080101D4                   — 跳转到返回
 *
 * 注: 该函数没有 BL/BLX 调用 — 完全自包含, 无外部依赖.
 * ======================================================================== */
static void SDIV_Loop_Divider(uint32_t *val_ptr, uint8_t iterations, uint8_t cmp_val)
{
    if (val_ptr == NULL)
        return;

    /* ---- 缩小循环: 当 iterations > cmp_val 时不断除以 10 ---- */
    while (iterations > cmp_val) {
        *val_ptr = (int32_t)(*val_ptr) / 10;
        iterations--;
    }

    /* ---- 放大循环: 当 iterations < cmp_val 时不断乘以 10 ---- */
    while (iterations < cmp_val) {
        *val_ptr = *val_ptr * 10;
        iterations++;
    }

    return;
}

/* ========================================================================
 * FPU_Compute_Chain_Fragment (0x0800F490 - 0x0800F4F4, 100 bytes)
 *
 * FPU 双精度计算链尾部片段 — VMOV/VLDR VFP 双精度寄存器操作序列.
 * 这是更大函数(Func_101_FPU_Compute, 起始于 0x0800F408) 的尾部.
 * 该尾部仅在 r4 != 0 时执行 (0x0800F48A: cbz r4, #0x0800F4F4).
 *
 * 使用的 FPU 寄存器: d8, d9, d10, d11, d12 (在函数入口 VPUSH 保存)
 * VLDR 常量池: 0x0800F528 = double -0.16666666666666632 (d12 load)
 *              0x0800F530 = double 0.5 (d0 load)
 *
 * 反汇编逐条对照:
 *   0x0800F48C: vmov r2, r3, d12          — arg2 = d12 (常量 -1/6)
 *   0x0800F490: vmov r0, r1, d9           — arg1 = d9
 *   0x0800F494: bl 0x08000578              — BL -> dp_arith_add(d9, d12)
 *   0x0800F498: vmov d12, r0, r1          — d12 = 结果
 *   0x0800F49C: vmov r2, r3, d10          — arg2 = d10
 *   0x0800F4A0: vmov r0, r1, d9           — arg1 = d9
 *   0x0800F4A4: bl 0x08000578              — BL -> dp_arith_add(d9, d10)
 *   0x0800F4A8: vldr d0, [pc, #0x84]      — d0 = 0.5 (从池 0x0800F530 加载)
 *   0x0800F4AC: vmov d9, r0, r1           — d9 = 结果
 *   0x0800F4B0: vmov r2, r3, d0           — arg2 = 0.5
 *   0x0800F4B4: vmov r0, r1, d11          — arg1 = d11
 *   0x0800F4B8: bl 0x08000578              — BL -> dp_arith_add(d11, 0.5)
 *   0x0800F4BC: vmov r2, r3, d9           — arg2 = d9
 *   0x0800F4C0: bl 0x0800056C              — BL -> dp_arith_sub(上一步结果, d9)
 *   0x0800F4C4: vmov r2, r3, d8           — arg2 = d8
 *   0x0800F4C8: bl 0x08000578              — BL -> dp_arith_add(上一步结果, d8)
 *   0x0800F4CC: vmov r2, r3, d11          — arg2 = d11
 *   0x0800F4D0: bl 0x0800056C              — BL -> dp_arith_sub(上一步结果, d11)
 *   0x0800F4D4: vmov r2, r3, d12          — arg2 = d12
 *   0x0800F4D8: bl 0x0800056C              — BL -> dp_arith_sub(上一步结果, d12)
 *   0x0800F4DC: vldr d1, [sp, #0x28]      — d1 = [sp+0x28] (从栈加载参数)
 *   0x0800F4E0: vmov r2, r3, d1           — arg2 = d1
 *   0x0800F4E4: bl 0x08000572              — BL -> dp_arith_sub_r(d1, 上一步结果)
 *                                             (0x08000572 翻转第一操作数符号: b - a)
 *   0x0800F4E8: vpop {d8,d9,d10,d11,d12}  — 恢复被调用者保存寄存器
 *   0x0800F4EC: add sp, #0x14              — 释放栈空间
 *   0x0800F4EE: vmov d0, r0, r1           — 返回值写入 d0
 *   0x0800F4F2: pop {r4, r5, pc}           — 返回到调用者
 *
 * BL 目标验证 (全部 Capstone 确认):
 *   0x08000578 = dp_arith_add  (push.w {r1..lr}; 主双精度算术引擎)
 *   0x0800056C = dp_arith_sub  (eor r3,#0x80000000; b 0x0800042A)
 *   0x08000572 = dp_arith_sub_r (eor r1,#0x80000000; b 0x0800042A)
 *   0x0800042A = 基础 add/sub 引擎入口
 *
 * ======================================================================== */
static double FPU_Compute_Chain_Fragment(double d8_val, double d9_val,
                                          double d10_val, double d11_val,
                                          double stack_arg)
{
    double d12, tmp;

    /* 0x0800F490: BL dp_arith_add(d9, d12_const) -> d12 */
    d12 = dp_arith_add(d9_val, (-0.16666666666666632));

    /* 0x0800F4A0: BL dp_arith_add(d9, d10) -> tmp, then d9=tmp */
    tmp = dp_arith_add(d9_val, d10_val);

    /* 0x0800F4A8: VLDR d0 = 0.5 */
    /* 0x0800F4B0-F4B8: BL dp_arith_add(d11, 0.5) -> tmp2 */
    /* 0x0800F4BC-F4C0: BL dp_arith_sub(tmp2, tmp) -> d9 更新 */
    tmp = dp_arith_sub(dp_arith_add(d11_val, 0.5), tmp);

    /* 0x0800F4C4-F4C8: BL dp_arith_add(tmp, d8) */
    tmp = dp_arith_add(tmp, d8_val);

    /* 0x0800F4CC-F4D0: BL dp_arith_sub(tmp, d11) */
    tmp = dp_arith_sub(tmp, d11_val);

    /* 0x0800F4D4-F4D8: BL dp_arith_sub(tmp, d12) */
    tmp = dp_arith_sub(tmp, d12);

    /* 0x0800F4DC-F4E4: BL dp_arith_sub_r(stack_arg, tmp)  (计算 stack_arg - tmp) */
    /* 0x08000572 翻转第一操作数符号: b - a, 即 stack_arg - tmp */
    tmp = dp_arith_sub_r(stack_arg, tmp);

    /* 0x0800F4EE: vmov d0, r0,r1 — 结果通过 d0 返回 */
    return tmp;
}

/*
 * 内存布局:
 *
 * SDIV_Loop_Divider:
 *   0x080101D0 - 0x080101D4: 函数序言 + NULL 检查 (push + cbnz)
 *   0x080101D6 - 0x080101E8: 除法循环体  (BGT: iterations > cmp_val → *val /= 10)
 *   0x080101EA - 0x080101FC: 乘法循环体  (BLT: iterations < cmp_val → *val *= 10)
 *   0x080101FE - 0x08010202: nop + 返回跳转
 *
 * FPU_Compute_Chain_Fragment (位于 Func_101 尾部, 函数入口在 0x0800F408):
 *   0x0800F490 - 0x0800F4A8: 前两次 dp_arith_add 调用 + VLDR 加载 0.5 常量
 *   0x0800F4AC - 0x0800F4E4: 后续 dp_arith_add/sub 链
 *   0x0800F4E8 - 0x0800F4F4: VPOP + 栈清理 + 返回
 *
 * 常量池:
 *   0x0800F528: 0xBFC5555555555549 (double -0.16666666666666632 ≈ -1/6)
 *   0x0800F530: 0x3FE0000000000000 (double 0.5)
 */
