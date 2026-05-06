/**
 * @file func_81_uart_sensor_init.c
 * @brief 函数: UART/传感器初始化 — 配置并启动通信外设
 * @addr  0x08012924 - 0x08012990 (108 bytes, 含字面量池)
 *         字面量池 0x0801298C-0x0801298F (4 bytes, 1 个设备 ID)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 初始化序列:
 *   1. 分配/使能资源 ID 0x40000
 *   2. 构建设备配置结构体 (sp+0x04)
 *   3. 调用 func_134A8 打开设备
 *   4. 调用 func_133B0 启动设备
 *   5. 调用 func_1340E 配置设备
 *   6. 发送初始化命令 {0x1A, 0x01, 0x03, 0x01}
 *   7. 关闭设备, 清除基址 +0x24
 *
 * 参数:
 *   r0 = param1 — 配置值 (存储到栈)
 *   r1 = param2 — 配置值 (存储到栈)
 *
 * 调用:
 *   func_113F8() @ 0x080113F8 — 资源分配
 *   func_134A8() @ 0x080134A8 — 设备打开
 *   func_133B0() @ 0x080133B0 — 设备启动
 *   func_1340E() @ 0x0801340E — 设备配置
 *   func_0D9B4() @ 0x0800D9B4 — 命令发送
 *   func_133B6() @ 0x080133B6 — 设备关闭
 */

#include "stm32f4xx_hal.h"

extern void func_113F8(uint32_t res_id, uint32_t enable);
extern void func_134A8(uint32_t dev_id, void *cfg);
extern void func_133B0(uint32_t dev_id, uint32_t val);
extern void func_1340E(uint32_t dev_id, uint32_t a, uint32_t b);
extern void func_0D9B4(uint8_t *cmd);
extern void func_133B6(uint32_t dev_id, uint32_t val);

typedef struct {
    uint16_t param2_lo;   /* offset 0:  (uint16_t)param2    — strh r4, [sp, #4]  */
    uint16_t zero_2;      /* offset 2:  0                   — strh r0, [sp, #6]  */
    uint32_t param1;      /* offset 4:  param1 (full 32-bit) — str r5, [sp, #8]  */
    uint16_t zero_8;      /* offset 8:  0                   — strh r0, [sp,#0xc] */
    uint8_t  pad[10];     /* offset 10..19: callee reads to offset 0x12 */
} dev_cfg_t;

void UART_Sensor_Init(uint32_t param1, uint32_t param2)
{
    uint8_t   init_cmd[4];
    dev_cfg_t cfg;
    uint32_t  dev_id;

    dev_id = 0x40014800;  /* 字面量池 0x0801298C */

    func_113F8(0x40000, 1);

    cfg.param2_lo = (uint16_t)param2;
    cfg.zero_2    = 0;
    cfg.param1    = param1;
    cfg.zero_8    = 0;

    func_134A8(dev_id, &cfg);
    func_133B0(dev_id, 1);
    func_1340E(dev_id, 1, 1);

    init_cmd[0] = 0x1A;
    init_cmd[1] = 0x01;
    init_cmd[2] = 0x03;
    init_cmd[3] = 0x01;
    func_0D9B4(init_cmd);

    func_133B6(dev_id, 0);
    *((volatile uint32_t *)(dev_id + 0x24)) = 0;
}
