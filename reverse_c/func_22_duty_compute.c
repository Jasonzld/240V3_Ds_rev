/**
 * @file func_22_duty_compute.c
 * @brief 函数: Compute_DutyCycle — 双相分段线性 PWM 占空比计算
 * @addr  0x0800C710 - 0x0800C7A0 (144 bytes, 含字面量池)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 从两个 ADC/传感器读数计算 PWM 占空比值, 采用两阶段
 * 分段线性映射, 结果除以 10 产生最终有符号 16-bit 输出。
 *
 * 阶段 1 — 温度/电压到基准值映射:
 *   输入: ADC_AVG_2 (0x200002E0 或类似) 除以 100
 *   val1 = input / 100 (整数除法)
 *   if val1 == 0: return 0
 *   if val1 < 44:  r4 = 600 - (val1 - 1) * 10     [范围: 600..170]
 *   else:           r4 = 175 - (val1 - 44) * 5     [范围: 175..?]
 *
 * 阶段 2 — 第二传感器映射:
 *   输入: VAL_RAW_2 (0x200002E2 或类似)
 *   val2 = (input + sign_bit) / 2, 取 bit[8:1]
 *   if val2 == 0: return 0
 *   if val2 < 15:  r2 = val2 + 82                   [范围: 82..96]
 *   elif val2 < 46: r2 = 7 * val2 - 2               [范围: 103..320]
 *   elif val2 == 46: r2 = 319                       [精确值]
 *   else (val2 > 46): r2 = 4 * val2 + 135           [范围: 323..?]
 *
 * 最终: result = (int16_t)((r4 + r2) / 10)
 *
 * 参数: 无 (从全局 RAM 变量读取输入)
 * 返回: r0 = int16_t PWM 占空比值 (有符号)
 *
 * RAM 变量:
 *   0x20000040 = ADC_AVG  (uint16_t, 阶段1 输入, 与 func_18 共用)
 *   0x2000003E = VAL_RAW  (uint16_t, 阶段2 输入, 与 func_18 共用)
 *
 * 字面量池 (0x0800C798-0x0800C79F):
 *   0x0800C798: 0x20000040
 *   0x0800C79C: 0x2000003E
 */

#include "stm32f4xx_hal.h"

/* 全局输入变量 */
#define ADC_AVG       (*(volatile uint16_t *)0x20000040)  /* 阶段1 ADC 平均值 */
#define VAL_RAW       (*(volatile uint16_t *)0x2000003E)  /* 阶段2 原始值 */

/* ================================================================
 * Compute_DutyCycle() @ 0x0800C710
 *   双相分段线性映射 → PWM 占空比
 *   返回: int16_t (r0), 可能为负值
 *
 * 寄存器使用:
 *   r4 = 阶段1 计算结果 (s1_val)
 *   r5 = 最终结果
 *   r6 = 0 (未使用/初始化残留)
 *   r7 = 临时 (除数/地址/中间值)
 *
 * 注: 结果可为负值 (sxth 符号扩展), 调用者将负值
 *     钳位到 0 或取绝对值。
 * ================================================================ */
int16_t Compute_DutyCycle(void)
{
    uint32_t adc_raw;        /* r0 — ADC 原始值 */
    uint32_t val1;           /* r3 — 阶段1 量化值 */
    uint32_t s1_val;         /* r4 — 阶段1 映射输出 */
    uint32_t val2;           /* r1 — 阶段2 量化值 */
    uint32_t s2_val;         /* r2 — 阶段2 映射输出 */
    int32_t  sum;             /* r0 — 两阶段和 */
    int16_t  result;          /* r5 — 最终结果 */

    /* ================================================================
     * 阶段 1: ADC 输入 → 基准值 (0x0800C716-0x0800C742)
     * ================================================================ */

    /* 读取阶段1 ADC 输入并除以 100 */
    adc_raw = ADC_AVG;                           /* 0x0800C716-18: ldr r0,=ADC_AVG; ldrh r0,[r0] */
    val1 = adc_raw / 100;                        /* 0x0800C71A-1C: movs r7,#0x64; sdiv r0,r0,r7 */
    val1 = (uint8_t)val1;                        /* 0x0800C720: uxtb r3,r0 */

    if (val1 == 0) {                             /* 0x0800C722: cbnz r3, #next */
        return 0;                                /* 0x0800C724-26: movs r0,#0; pop {r4,r5,r6,r7,pc} */
    }

    /* 分段线性映射 */
    if (val1 < 44) {                             /* 0x0800C728-2A: cmp r3,#0x2c; bge #else1 */
        /* 段 A: val1 ∈ [1, 43]
         * s1_val = 600 - (val1 - 1) * 10
         * = 0x258 - (val1-1)*(1+4)*2
         */
        s1_val = 600 - (val1 - 1) * 10;         /* 0x0800C72C-34: subs r0,r3,#1;
                                                  *   add.w r0,r0,r0,lsl#2; lsls r0,r0,#1;
                                                  *   rsb.w r4,r0,#0x258 */
    } else {
        /* 段 B: val1 ∈ [44, 255]
         * s1_val = 175 - (val1 - 44) * 5
         * = 0xAF - (val1-0x2C)*(1+4)
         */
        s1_val = 175 - (val1 - 44) * 5;         /* 0x0800C73A-42: sub.w r0,r3,#0x2c;
                                                  *   add.w r0,r0,r0,lsl#2;
                                                  *   rsb.w r4,r0,#0xaf */
    }

    /* ================================================================
     * 阶段 2: 第二传感器 → 偏移值 (0x0800C746-0x0800C786)
     * ================================================================ */

    /* 读取阶段2 输入, 加符号位后除以 2, 取 bit[8:1] */
    adc_raw = VAL_RAW;                           /* 0x0800C746-48: ldr r7,=VAL_RAW; ldrh r0,[r7] */
    val2 = adc_raw + (adc_raw >> 31);           /* 0x0800C74A: add.w r7,r0,r0,lsr#31 */
    val2 = (val2 >> 1) & 0xFF;                  /* 0x0800C74E: ubfx r1,r7,#1,#8 */

    if (val2 == 0) {                             /* 0x0800C752: cbnz r1, #next */
        return 0;                                /* 0x0800C754-56: movs r0,#0; b #pop_ret */
    }

    /* 三段分段线性 + 单点精确值 */
    if (val2 < 15) {                             /* 0x0800C758-5A: cmp r1,#0xf; bge #chk46 */
        /* 段 C: val2 ∈ [1, 14]
         * s2_val = val2 + 82
         */
        s2_val = val2 + 82;                     /* 0x0800C75C: add.w r2,r1,#0x52 */
    } else if (val2 < 46) {                      /* 0x0800C762-64: cmp r1,#0x2e; bge #chk_eq46 */
        /* 段 D: val2 ∈ [15, 45]
         * s2_val = 7 * val2 - 2
         * = (val2-15)*7 + 103
         */
        s2_val = 7 * val2 - 2;                  /* 0x0800C766-6E: sub.w r0,r1,#0xf;
                                                  *   rsb r0,r0,r0,lsl#3;
                                                  *   add.w r2,r0,#0x67 */
    } else if (val2 == 46) {                     /* 0x0800C774-76: cmp r1,#0x2e; bne #else2 */
        /* 精确值: val2 == 46 → 319 */
        s2_val = 319;                            /* 0x0800C778: movw r2,#0x13f */
    } else {
        /* 段 E: val2 ∈ [47, 255]
         * s2_val = 4 * val2 + 135
         * = (val2-47)*4 + 323
         */
        s2_val = 4 * val2 + 135;                /* 0x0800C77E-84: sub.w r0,r1,#0x2f;
                                                  *   lsls r0,r0,#2;
                                                  *   addw r2,r0,#0x143 */
    }

    /* ================================================================
     * 组合与输出
     * ================================================================ */
    sum = (int32_t)(s1_val + s2_val);            /* 0x0800C788: adds r0,r4,r2 */
    result = (int16_t)(sum / 10);                /* 0x0800C78A-90: movs r7,#0xa; sdiv r0,r0,r7;
                                                  *   sxth r5,r0 */
    return result;                               /* 0x0800C792-94: mov r0,r5; b #pop_ret */
}
