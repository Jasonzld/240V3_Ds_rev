/**
 * @file func_89_device_init_variant4.c
 * @brief 函数: 设备初始化变体 4 — 通过 func_113F8(0x10000) 初始化设备并发送 0x18 命令
 * @addr  0x08013344 - 0x080133B0 (108 bytes, 含字面量池)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 模板同 func_81, 差异:
 *   - func_113F8(0x10000, 1) (资源 ID 0x10000)
 *   - 命令首字节 0x18
 *   - func_133B6(dev_id, 1) 而非 0
 *
 * 参数:
 *   r0 = param1
 *   r1 = param2
 */

#include "stm32f4xx_hal.h"

extern void func_113F8(uint32_t res, uint32_t enable);
extern void func_134A8(uint32_t id, void *cfg);
extern void func_133B0(uint32_t id, uint32_t val);
extern void func_1340E(uint32_t id, uint32_t a, uint32_t b);
extern void func_133B6(uint32_t id, uint32_t val);
extern void func_0D9B4(uint8_t *cmd);

void Device_Init_Variant4(uint32_t param1, uint32_t param2)
{
    uint8_t  cmd[4];
    uint16_t cfg[5];
    uint32_t dev_id = 0x40014000;

    func_113F8(0x10000, 1);

    cfg[0] = (uint16_t)param2;
    cfg[1] = 0;
    *(uint32_t *)&cfg[2] = param1;
    cfg[4] = 0;

    func_134A8(dev_id, &cfg);
    func_133B0(dev_id, 1);
    func_1340E(dev_id, 1, 1);

    cmd[0] = 0x18;
    cmd[1] = 0x01;
    cmd[2] = 0x03;
    cmd[3] = 0x01;
    func_0D9B4(cmd);

    func_133B6(dev_id, 1);
    *((volatile uint32_t *)(dev_id + 0x24)) = 0;
}
