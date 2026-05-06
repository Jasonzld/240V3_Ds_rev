/**
 * @file func_86_device_init_variant2.c
 * @brief 函数: 设备初始化变体 2 — 通过 func_113D8 初始化设备并发送 0x32 命令
 * @addr  0x08013198 - 0x08013204 (108 bytes, 含字面量池)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 模板同 func_81, 差异:
 *   - 使用 func_113D8(8, 1) 而非 func_113F8
 *   - 初始化命令首字节为 0x32 而非 0x1A
 *
 * 参数:
 *   r0 = param1
 *   r1 = param2
 */

#include "stm32f4xx_hal.h"

extern void func_113D8(uint32_t res, uint32_t enable);
extern void func_134A8(uint32_t id, void *cfg);
extern void func_133B0(uint32_t id, uint32_t val);
extern void func_1340E(uint32_t id, uint32_t a, uint32_t b);
extern void func_133B6(uint32_t id, uint32_t val);
extern void func_0D9B4(uint8_t *cmd);

typedef struct {
    uint16_t param2;    /* offset 0:  (uint16_t)param2 — strh r4, [sp, #4]  */
    uint16_t zero_2;    /* offset 2:  0              — strh r0, [sp, #6]  */
    uint32_t param1;    /* offset 4:  param1 (32-bit) — str r5, [sp, #8]  */
    uint16_t zero_8;    /* offset 8:  0              — strh r0, [sp,#0xc] */
} dev_cfg_t;

void Device_Init_Variant2(uint32_t param1, uint32_t param2)
{
    uint8_t   cmd[4];
    dev_cfg_t cfg;
    uint32_t  dev_id = 0x40000C00;

    func_113D8(8, 1);

    cfg.param2 = (uint16_t)param2;
    cfg.zero_2 = 0;
    cfg.param1 = param1;
    cfg.zero_8 = 0;

    func_134A8(dev_id, &cfg);
    func_133B0(dev_id, 1);
    func_1340E(dev_id, 1, 1);
    func_133B6(dev_id, 1);

    *((volatile uint32_t *)(dev_id + 0x24)) = 0;

    cmd[0] = 0x32;
    cmd[1] = 0x01;
    cmd[2] = 0x03;
    cmd[3] = 0x01;
    func_0D9B4(cmd);
}
