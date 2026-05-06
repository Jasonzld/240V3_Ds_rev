/**
 * @file func_83_multi_stage_init.c
 * @brief 函数: 多阶段初始化 — 传感器 + UART 级联初始化
 * @addr  0x08013000 - 0x080130BC (188 bytes, 含字面量池)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 分两个阶段配置:
 *   阶段 1: 传感器初始化
 *     - func_113B8(1, 1) — 资源准备
 *     - 构建传感器配置 (0x100, 2, 3, 0, 1)
 *     - func_0C42C(sensor_id, &cfg) — 传感器打开
 *     - func_0C4BC(sensor_id, 8, 1) — 传感器配置
 *
 *   阶段 2: UART 配置
 *     - func_113F8(1, 1) — 使能
 *     - 构建 UART 配置 1 (波特率 0x7CF=1999, etc.)
 *     - func_134A8(uart_id, &cfg1)
 *     - 构建 UART 配置 2 (0x70, 1, 4, 0, 2, 0, 0x100, 0)
 *     - func_13420(uart_id, &cfg2)
 *     - func_133B6(uart_id, 1)
 *     - func_133CE(uart_id, 1)
 *
 * 参数: (无寄存器参数)
 *
 * 调用:
 *   func_113B8() @ 0x080113B8
 *   func_113F8() @ 0x080113F8
 *   func_0C42C() @ 0x0800C42C
 *   func_0C4BC() @ 0x0800C4BC
 *   func_134A8() @ 0x080134A8
 *   func_13420() @ 0x08013420
 *   func_133B6() @ 0x080133B6
 *   func_133CE() @ 0x080133CE
 */

#include "stm32f4xx_hal.h"

extern uint32_t func_113B8(uint32_t a, uint32_t b);
extern void     func_113F8(uint32_t a, uint32_t b);
extern void     func_0C42C(uint32_t id, void *cfg);
extern void     func_0C4BC(uint32_t id, uint32_t a, uint32_t b);
extern void     func_134A8(uint32_t id, void *cfg);
extern void     func_13420(uint32_t id, void *cfg);
extern void     func_133B6(uint32_t id, uint32_t val);
extern void     func_133CE(uint32_t id, uint32_t val);

void Multi_Stage_Init(void)
{
    /* 阶段 1: 传感器初始化 */
    func_113B8(1, 1);

    {
        struct {
            uint32_t val_100;
            uint8_t  b0;
            uint8_t  b1;
            uint8_t  b2;
            uint8_t  b3;
        } sensor_cfg;

        sensor_cfg.val_100 = 0x100;
        sensor_cfg.b0 = 2;
        sensor_cfg.b1 = 3;
        sensor_cfg.b2 = 0;
        sensor_cfg.b3 = 1;

        func_0C42C(0x40020000, &sensor_cfg);  /* 从字面量池加载 */
    }

    func_0C4BC(0x40020000, 8, 1);

    /* 阶段 2: UART 配置 */
    func_113F8(1, 1);

    {
        struct {
            uint16_t baud_7cf;
            uint16_t zero_0;
            uint32_t val_63;
            uint16_t zero_1;
            uint8_t  zero_2;
        } uart_cfg1;

        uart_cfg1.baud_7cf = 0x7CF;
        uart_cfg1.zero_0 = 0;
        uart_cfg1.val_63 = 0x63;
        uart_cfg1.zero_1 = 0;
        uart_cfg1.zero_2 = 0;

        func_134A8(0x40010000, &uart_cfg1);
    }

    {
        struct {
            uint16_t v0_70;
            uint16_t v1_01;
            uint16_t v2_04;
            uint32_t v3_00;
            uint16_t v4_02;
            uint16_t v5_00;
            uint16_t v6_100;
            uint16_t v7_00;
        } uart_cfg2;

        uart_cfg2.v0_70  = 0x70;
        uart_cfg2.v1_01  = 1;
        uart_cfg2.v2_04  = 4;
        uart_cfg2.v3_00  = 0;
        uart_cfg2.v4_02  = 2;
        uart_cfg2.v5_00  = 0;
        uart_cfg2.v6_100 = 0x100;
        uart_cfg2.v7_00  = 0;

        func_13420(0x40010000, &uart_cfg2);
    }

    func_133B6(0x40010000, 1);
    func_133CE(0x40010000, 1);
}
