/**
 * @file func_02_bridge_leaf_helpers.c
 * @brief FPU 运行时与模块操作处理器之间的桥接叶函数
 * @addr  0x080091A0 - 0x0800930A (362 bytes, 2 个叶函数 + 字面量池)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *        NOTE: Overlaps with func_05_adc_helpers.c which names these same
 *        functions "adc_start" and "adc_init_helper" (ADC-specific view).
 *
 * 位于 __aeabi_f2d / Char_Digit_Parser (func_01) 和
 * Register_Field_Copier (func_154) 之间的薄层叶函数.
 * 均为无栈帧的简单外设寄存器操作.
 *
 * 叶函数:
 *   Register_Bit_SetClear @ 0x080091A0 — 条件设置/清除外设控制位
 *   Status_Reg_Aggregator  @ 0x080091B8 — 多寄存器 OR 聚合 + 掩码写入
 */

#include "stm32f4xx_hal.h"

/* ---- 字面量池地址 ---- */
#define MASK_REG        (*(volatile uint32_t *)0x40010000)  /* 掩码寄存器 (pool) */
#define TARGET_OFFSET   0x304                                /* 目标偏移 */

/* ========================================================================
 * Register_Bit_SetClear (0x080091A0 - 0x080091B4, 20 bytes)
 *
 * 根据标志参数条件设置或清除外设寄存器的 bit 0.
 * 无 PUSH — 叶函数, 仅使用 r0-r2.
 *
 * 参数: r0 = reg_base — 外设寄存器块基址
 *       r1 = set_flag — 若非零则置位 bit 0, 若为零则清除 bit 0
 * 返回: 无
 * ======================================================================== */
static void Register_Bit_SetClear(volatile uint32_t *reg_base, uint32_t set_flag)
{
    /* CBZ r1, clear_path */
    if (set_flag != 0) {
        /* 置位路径: ORR bit 0 */
        uint32_t val = reg_base[2];          /* LDR r2, [r0, #8] */
        val |= 1;                            /* ORR r2, r2, #1 */
        reg_base[2] = val;                   /* STR r2, [r0, #8] */
    } else {
        /* 清除路径: BIC bit 0 */
        uint32_t val = reg_base[2];          /* LDR r2, [r0, #8] */
        val &= ~1;                           /* BIC r2, r2, #1 */
        reg_base[2] = val;                   /* STR r2, [r0, #8] */
    }
    /* BX LR */
}

/* ========================================================================
 * Status_Reg_Aggregator (0x080091B8 - 0x080091D8, 32 bytes)
 *
 * 从源结构读取多个 32 位寄存器值, OR 聚合, 应用掩码,
 * 写入目标寄存器.
 *
 * 参数: r0 = src_struct — 源数据结构指针:
 *            [0]  = 寄存器值 A (LDRD r2,r3,[r0] → r2,r3)
 *            [8]  = 寄存器值 B (LDR r3,[r0,#8])
 *            [12] = 寄存器值 C (LDR r3,[r0,#12])
 * 返回: r1 = OR 聚合结果 (也在目标寄存器中)
 *
 * 操作:
 *   1. 加载全局掩码 (LDR r2,[pc] → pool)
 *   2. OR 所有源字段: A_lo | A_hi | B | C
 *   3. AND 掩码与全局标志
 *   4. OR 聚合: 掩码 | src_aggregate
 *   5. 以偏移量 0x304 写入外设寄存器
 * ======================================================================== */
static uint32_t Status_Reg_Aggregator(const uint32_t *src_struct)
{
    uint32_t mask_a;
    uint32_t mask_b;
    uint32_t result;

    /* 加载全局掩码字 */
    mask_a = MASK_REG;                       /* LDR r2, [pc, #0x20] → LDR r1, [r2] */
    mask_b = *(volatile uint32_t *)0x40010004; /* LDR r2, [pc, #0x20] — 第二个掩码 */
    /* ANDS r1, r2 → 掩码预合并 */

    /* OR 聚合源寄存器 */
    uint32_t a_lo = src_struct[0];           /* LDRD r2, r3, [r0] */
    uint32_t a_hi = src_struct[1];
    result = a_lo | a_hi;

    result |= src_struct[2];                 /* LDR r3, [r0, #8] → ORRS */
    result |= src_struct[3];                 /* LDR r3, [r0, #12] → ORRS */

    /* 合并掩码 */
    result |= mask_a & mask_b;               /* ORRS r1, r2 */

    /* 写入目标: 外设基址 + 0x304 偏移 */
    *(volatile uint32_t *)(0x40010000 + TARGET_OFFSET) = result;
    /* STR.W r1, [r2, #0x304] */

    /* BX LR */
    return result;
}

/*
 * 内存布局:
 *   0x080091A0 - 0x080091B4: Register_Bit_SetClear (20B)
 *   0x080091B4 - 0x080091B8: 对齐填充 (4B, MOVS r0,r0 ×2)
 *   0x080091B8 - 0x080091D8: Status_Reg_Aggregator (32B)
 *   0x080091D8 - 0x080091DC: 对齐填充 (4B)
 *   0x080091DC - 0x0800930A: 字面量池 + 小辅助函数 (约 302B)
 *   0x0800930A - 0x08009320: func_154 前导数据/字面量池 (22B)
 *
 * 字面量池内容 (0x080091DC-0x0800930A):
 *   包括外设基址、掩码常量和函数指针.
 *   与 Register_Field_Copier (0x08009320) 的池重叠.
 *
 * 注: 这些叶函数在功能上桥接了 FPU/算术层 (func_01)
 *     和模块操作层 (func_154). Register_Bit_SetClear 被
 *     Register_BitField_Config 间接使用; Status_Reg_Aggregator
 *     服务于状态轮询/监控子系统 (func_153).
 */
