/**
 * @file func_124_threshold_monitor.c
 * @brief 函数: 阈值监控器 — 多通道比较 + 条件触发 func_112C4
 * @addr  0x08016548 - 0x0801660E (198 bytes) + 字面量池 0x08016610-0x08016660 (80 bytes)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 检查多个 RAM 变量是否超过阈值, 并根据条件:
 *   - 调用 func_112C4(1, value*2) 设置标志
 *   - 或调用 func_112C4(0, 0) 清除标志
 * 阶段 2: 5 路无条件比较清零各标志字节.
 *
 * 调用:
 *   func_112C4() @ 0x080112C4 — 数据处理
 */

#include "stm32f4xx_hal.h"

extern void func_112C4(uint32_t a, uint32_t b);

/* RAM 变量 (从字面量池 0x08016610-0x08016660, 21 项) */

/* 字节标志 */
#define GATE_FLAG_A    (*(volatile uint8_t  *)0x20000019)
#define FLAG_MAIN      (*(volatile uint8_t  *)0x20000033)  /* 读写同一字节 */
#define FLAG_GD        (*(volatile uint8_t  *)0x2000016E)
#define FLAG_BYTE_E    (*(volatile uint8_t  *)0x200000CB)
#define FLAG_BYTE_F    (*(volatile uint8_t  *)0x20000032)
#define FLAG_COMP2_A   (*(volatile uint8_t  *)0x200000CC)
#define FLAG_COMP2B    (*(volatile uint8_t  *)0x200000CD)
#define FLAG_BYTE_C    (*(volatile uint8_t  *)0x200000C9)
#define FLAG_COMP5     (*(volatile uint8_t  *)0x200000CA)

/* 半字传感器/阈值 */
#define SENSOR_HW_A    (*(volatile uint16_t *)0x20000064)
#define SENSOR_HW_B    (*(volatile uint16_t *)0x20000046)
#define SENSOR_HW_C    (*(volatile uint16_t *)0x20000044)
#define THRESH_MAIN    (*(volatile uint16_t *)0x20000040)
#define SENSOR_GD      (*(volatile uint16_t *)0x200000D0)
#define SENSOR_ARG     (*(volatile uint16_t *)0x20000056)
#define THRESH_HW_A    (*(volatile uint16_t *)0x2000004E)
#define THRESH_HW_B    (*(volatile uint16_t *)0x20000050)
#define THRESH_CMP3    (*(volatile uint16_t *)0x20000052)
#define SENSOR_45      (*(volatile uint16_t *)0x2000003E)
#define THRESH_CMP4    (*(volatile uint16_t *)0x2000004A)
#define THRESH_CMP5    (*(volatile uint16_t *)0x2000004C)

void Threshold_Monitor(void)
{
    int32_t delta;

    /* 阶段 1: 门控 + 条件检查 */
    if (GATE_FLAG_A == 0) {
        goto stage2;
    }
    if (SENSOR_HW_A != 0) {
        goto stage2;
    }

    delta = SENSOR_HW_C - SENSOR_HW_B;

    if (delta >= THRESH_MAIN) {
        /* THEN: delta 超阈值 */
        if (FLAG_MAIN  != 0)  goto stage2;
        if (SENSOR_GD   != 0)  goto stage2;
        if (FLAG_GD     != 0)  goto stage2;

        func_112C4(1, SENSOR_ARG * 2);
        FLAG_MAIN = 1;
    } else {
        /* ELSE: delta 低于主阈值 */
        if (THRESH_MAIN >= SENSOR_HW_C) {
            FLAG_MAIN = 0;
            func_112C4(0, 0);
        }
    }

stage2:
    /* Block 1: THRESH_MAIN >= THRESH_HW_A → 清零 FLAG_BYTE_E, FLAG_BYTE_F */
    if (THRESH_MAIN >= THRESH_HW_A) {
        FLAG_BYTE_E = 0;
        FLAG_BYTE_F = 0;
    }

    /* Block 2: THRESH_MAIN < THRESH_HW_B → 清零 FLAG_COMP2_A, FLAG_COMP2B */
    if (THRESH_MAIN < THRESH_HW_B) {
        FLAG_COMP2_A = 0;
        FLAG_COMP2B  = 0;
    }

    /* Block 3: THRESH_MAIN < THRESH_CMP3 → 清零 FLAG_COMP2B */
    if (THRESH_MAIN < THRESH_CMP3) {
        FLAG_COMP2B = 0;
    }

    /* Block 4: SENSOR_45 >= THRESH_CMP4 → 清零 FLAG_BYTE_C */
    if (SENSOR_45 >= THRESH_CMP4) {
        FLAG_BYTE_C = 0;
    }

    /* Block 5: SENSOR_45 <= THRESH_CMP5 → 清零 FLAG_COMP5 */
    if (SENSOR_45 <= THRESH_CMP5) {
        FLAG_COMP5 = 0;
    }
}
