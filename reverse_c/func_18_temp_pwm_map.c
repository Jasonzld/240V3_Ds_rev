/**
 * @file func_18_temp_pwm_map.c
 * @brief Temp_PWM_Map — 双通道 ADC 温度到 PWM 占空比分段线性映射
 * @addr  0x0800C538 - 0x0800C608 (208 bytes / 0xD0, ~80 指令 + 8 字节常量池)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 从两路 ADC 读取温度相关采样值，分别通过分段线性映射表转换为
 * 16 位有符号贡献值，相加后以 16 位无符号 PWM 值返回。
 *
 * 第一路 (RAM_ADC_AVG @ 0x20000040):
 *   原始值 / 100 = index1 (钳位至 [0, 100])
 *   index1 经 5 段折线映射得到 r3 (int16_t)
 *
 * 第二路 (RAM_VAL_A_RAW @ 0x2000003E):
 *   原始值 / 2 = index2 (钳位至 [0, 100])
 *   index2 经 4 段折线映射得到 r4 (int16_t)
 *
 * 返回值:
 *   r0 = (uint16_t)(r3 + r4)  — PWM 占空比数值
 *
 * 特殊行为:
 *   - 若 index1 == 0 或 index2 == 0，直接返回 0
 *   - r6 初始化后未使用 (编译器残留)
 *
 * RAM 变量:
 *   0x20000040 = RAM_ADC_AVG    (uint16_t, ADC 平均值)
 *   0x2000003E = RAM_VAL_A_RAW  (int16_t,  通道A 原始值)
 *
 * 常量池:
 *   0x0800C600: 0x20000040  (ldr r0, [pc, #0xC0] @ 0x0800C53E)
 *   0x0800C604: 0x2000003E  (ldr r7, [pc, #0x64] @ 0x0800C59E)
 */

#include "stm32f4xx_hal.h"

/* RAM 变量定义 (来自 func_01_main.c) */
#define RAM_ADC_AVG    (*(volatile uint16_t *)0x20000040)  /* ADC 平均值 */
#define RAM_VAL_A_RAW  (*(volatile int16_t  *)0x2000003E)  /* 通道A 原始值 */

/* ================================================================
 * Temp_PWM_Map() @ 0x0800C538
 *   双路 ADC 温度采样 → 分段线性映射 → PWM 占空比
 *
 * 寄存器分配:
 *   r0 = 临时/返回/中间结果
 *   r1 = index1 (第一路温度索引, 0-100)
 *   r2 = index2 (第二路温度索引, 0-100)
 *   r3 = mapped1 (第一路映射结果, int16_t)
 *   r4 = mapped2 (第二路映射结果, int16_t)
 *   r5 = 最终返回值 (uint16_t)
 *   r6 = 0 (未使用)
 *   r7 = 临时 (除数 100 / 临时计算 / 地址指针)
 *
 * 指令数: 50 (含 2 条常量池数据, 共 104 bytes / 0x68)
 * 下一条函数: 0x0800C608 (push {r4, r5, r6, lr})
 * ================================================================ */
uint16_t Temp_PWM_Map(void)
{
    uint32_t index1;        /* r1: 第一路温度索引 */
    uint32_t index2;        /* r2: 第二路温度索引 */
    int16_t  mapped1;       /* r3: 第一路分段映射输出 */
    int16_t  mapped2;       /* r4: 第二路分段映射输出 */
    uint32_t raw1;          /* r0 (复用): ADC1 原始值 */
    uint16_t raw2;          /* r0 (复用): ADC2 原始值 */

    /* ---- 入口 ---- */
    /* 0x0800C53A: movs r6, #0 — r6 初始化后未使用 (编译器残留) */
    /* 0x0800C53C: movs r5, #0 — r5 预清零 (最终在 0x0800C5FA 赋值) */

    /* ==========================================================
     * Phase 1: 第一路 ADC 处理 (RAM_ADC_AVG @ 0x20000040)
     * ========================================================== */

    /* 读取 ADC 平均值 */
    raw1 = RAM_ADC_AVG;                            /* 0x0800C53E: ldr r0,[pc,#0xC0] → 0x20000040 */
                                                   /* 0x0800C540: ldrh r0,[r0] */

    /* 计算索引: raw1 / 100, 转为 uint8 钳位至 [0, 100] */
    index1 = raw1 / 100;                           /* 0x0800C542: movs r7,#0x64 */
                                                   /* 0x0800C544: sdiv r0,r0,r7 */
                                                   /* 0x0800C548: uxtb r1,r0 — r1 = (uint8_t)(raw1/100) */

    if (index1 > 100) {                            /* 0x0800C54A: cmp r1,#0x64 */
        index1 = 100;                              /* 0x0800C54C: ble #0x... */
    }                                              /* 0x0800C54E: movs r1,#0x64 */

    /* 索引为 0 → 直接返回 0 */
    if (index1 == 0) {                             /* 0x0800C550: cbnz r1,#0x... */
        return 0;                                  /* 0x0800C552: movs r0,#0 */
    }                                              /* 0x0800C554: pop {r4,r5,r6,r7,pc} */

    /* ==========================================================
     * Phase 1 分段映射: index1 → mapped1 (r3)
     * 共 5 段折线, 覆盖 [1, 100]
     * ========================================================== */

    /* ---- 段 A: index1 ∈ [1, 13] ---- */
    if (index1 < 14) {                             /* 0x0800C556: cmp r1,#0xe */
                                                   /* 0x0800C558: bge #0x... */
        mapped1 = (index1 - 1) * 3 + 13;           /* 0x0800C55A: subs r0,r1,#1 */
                                                   /* 0x0800C55C: add.w r0,r0,r0,lsl #1 → *3 */
                                                   /* 0x0800C560: add.w r3,r0,#0xd */
    }
    /* ---- 段 B: index1 == 14 (单点) ---- */
    else if (index1 == 14) {                       /* 0x0800C566: cmp r1,#0xe */
                                                   /* 0x0800C568: bne #0x... */
        mapped1 = 50;                              /* 0x0800C56A: movs r3,#0x32 */
    }
    /* ---- 段 C: index1 ∈ [15, 39] ---- */
    else if (index1 < 40) {                        /* 0x0800C56E: cmp r1,#0x28 */
                                                   /* 0x0800C570: bge #0x... */
        /* mapped1 = (index1 - 15) * 26 + 76 */
        mapped1 = (int16_t)((index1 - 15) * 26 + 76);
                                                   /* 0x0800C572: sub.w r0,r1,#0xf */
                                                   /* 0x0800C576: movs r7,#0x1a */
                                                   /* 0x0800C578: muls r0,r7,r0 */
                                                   /* 0x0800C57A: adds r0,#0x4c */
                                                   /* 0x0800C57C: sxth r3,r0 */
    }
    /* ---- 段 D: index1 ∈ [40, 59] ---- */
    else if (index1 < 60) {                        /* 0x0800C580: cmp r1,#0x3c */
                                                   /* 0x0800C582: bge #0x... */
        /* mapped1 = (index1 - 40) * 16 + 716 */
        mapped1 = (int16_t)((index1 - 40) * 16 + 716);
                                                   /* 0x0800C584: sub.w r0,r1,#0x28 */
                                                   /* 0x0800C588: lsls r0,r0,#4 */
                                                   /* 0x0800C58A: add.w r0,r0,#0x2cc */
                                                   /* 0x0800C58E: sxth r3,r0 */
    }
    /* ---- 段 E: index1 ∈ [60, 100] ---- */
    else {                                         /* 0x0800C592: sub.w r0,r1,#0x3c */
        /* mapped1 = (index1 - 60) * 16 + 1050 */
        mapped1 = (int16_t)((index1 - 60) * 16 + 1050);
                                                   /* 0x0800C596: lsls r0,r0,#4 */
                                                   /* 0x0800C598: addw r0,r0,#0x41a */
                                                   /* 0x0800C59C: sxth r3,r0 */
    }

    /* ==========================================================
     * Phase 2: 第二路 ADC 处理 (RAM_VAL_A_RAW @ 0x2000003E)
     * ========================================================== */

    /* 读取通道A 原始值 (ldrh 零扩展到 32 位) */
    raw2 = (uint16_t)RAM_VAL_A_RAW;                /* 0x0800C59E: ldr r7,[pc,#0x64] → 0x2000003E */
                                                   /* 0x0800C5A0: ldrh r0,[r7] */

    /* 计算索引: (raw2 / 2) & 0xFF, 钳位至 [0, 100]
     * 注: add.w r7,r0,r0,lsr#31 是编译器对有符号除法的
     * 舍入模式 (补符号位后右移), 但此处 raw2 为 unsigned ldrh,
     * raw2>>31 恒为 0, 故 r7 == raw2, 实质即 raw2/2 & 0xFF
     */
    index2 = (raw2 / 2) & 0xFF;                    /* 0x0800C5A2: add.w r7,r0,r0,lsr #31 → r7=raw2 */
                                                   /* 0x0800C5A6: ubfx r2,r7,#1,#8 → (raw2>>1)&0xFF */

    if (index2 > 100) {                            /* 0x0800C5AA: cmp r2,#0x64 */
        index2 = 100;                              /* 0x0800C5AC: ble #0x... */
    }                                              /* 0x0800C5AE: movs r2,#0x64 */

    /* 索引为 0 → 直接返回 0 */
    if (index2 == 0) {                             /* 0x0800C5B0: cbnz r2,#0x... */
        return 0;                                  /* 0x0800C5B2: movs r0,#0 */
    }                                              /* 0x0800C5B4: b #epilogue */

    /* ==========================================================
     * Phase 2 分段映射: index2 → mapped2 (r4)
     * 共 4 段折线, 覆盖 [1, 100]
     * ========================================================== */

    /* ---- 段 F: index2 ∈ [1, 13] ---- */
    if (index2 < 14) {                             /* 0x0800C5B6: cmp r2,#0xe */
                                                   /* 0x0800C5B8: bge #0x... */
        /* mapped2 = (index2 - 1) * 20 + 170 */
        mapped2 = (int16_t)((index2 - 1) * 20 + 170);
                                                   /* 0x0800C5BA: subs r0,r2,#1 */
                                                   /* 0x0800C5BC: add.w r0,r0,r0,lsl #2 → *5 */
                                                   /* 0x0800C5C0: lsls r0,r0,#2 → *4 (=*20) */
                                                   /* 0x0800C5C2: adds r0,#0xaa */
                                                   /* 0x0800C5C4: sxth r4,r0 */
    }
    /* ---- 段 G: index2 ∈ [14, 40] ---- */
    else if (index2 < 41) {                        /* 0x0800C5C8: cmp r2,#0x29 */
                                                   /* 0x0800C5CA: bge #0x... */
        /* mapped2 = 424 - (index2 - 14) * 9 */
        mapped2 = (int16_t)(424 - (index2 - 14) * 9);
                                                   /* 0x0800C5CC: sub.w r0,r2,#0xe */
                                                   /* 0x0800C5D0: add.w r0,r0,r0,lsl #3 → *9 */
                                                   /* 0x0800C5D4: rsb.w r4,r0,#0x1a8 → 424-r0 */
    }
    /* ---- 段 H: index2 ∈ [41, 49] ---- */
    else if (index2 < 50) {                        /* 0x0800C5DA: cmp r2,#0x32 */
                                                   /* 0x0800C5DC: bge #0x... */
        /* mapped2 = 179 - (index2 - 41) * 11 */
        mapped2 = (int16_t)(179 - (index2 - 41) * 11);
                                                   /* 0x0800C5DE: sub.w r0,r2,#0x29 */
                                                   /* 0x0800C5E2: add.w r7,r0,r0,lsl #1 → *3 */
                                                   /* 0x0800C5E6: add.w r0,r7,r0,lsl #3 → +*8 (=*11) */
                                                   /* 0x0800C5EA: rsb.w r4,r0,#0xb3 → 179-r0 */
    }
    /* ---- 段 I: index2 ∈ [50, 100] ---- */
    else {                                         /* 0x0800C5F0: sub.w r0,r2,#0x32 */
        /* mapped2 = 80 - (index2 - 50) */
        mapped2 = (int16_t)(80 - (index2 - 50));   /* 0x0800C5F4: rsb.w r4,r0,#0x50 → 80-r0 */
    }

    /* ==========================================================
     * 合并两路映射结果, 输出 PWM 值
     * ========================================================== */

    /* r0 = mapped1 + mapped2 (有符号加法), 然后截断为 uint16_t */
    return (uint16_t)(mapped1 + mapped2);          /* 0x0800C5F8: adds r0,r3,r4 */
                                                   /* 0x0800C5FA: uxth r5,r0 */
                                                   /* 0x0800C5FC: mov r0,r5 */
                                                   /* 0x0800C5FE: b #epilogue */
    /* epilogue: 0x0800C554: pop {r4, r5, r6, r7, pc} */
}
