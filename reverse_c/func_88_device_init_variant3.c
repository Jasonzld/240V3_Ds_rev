/**
 * @file func_88_device_init_variant3.c
 * @brief 函数: 设备初始化变体 3 — 通过 func_113D8(0x10) 初始化设备并发送 0x36 命令
 * @addr  0x080132D8 - 0x08013344 (108 bytes, 含字面量池)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 模板同 func_86, 差异:
 *   - func_113D8(0x10, 1) (资源 ID 0x10)
 *   - 命令首字节 0x36
 *
 * 参数:
 *   r0 = param1
 *   r1 = param2
 *
 * cfg 布局 (由 func_134A8/func_13420 使用):
 *   偏移 0: uint16_t = param2     (strh r4, [sp,#4])
 *   偏移 2: uint16_t = 0          (strh r0, [sp,#6], r0=0)
 *   偏移 4: uint32_t = param1     (str r5, [sp,#8] — 32 位字存储!)
 *   偏移 8: uint16_t = 0          (strh r0, [sp,#0xc], r0=0)
 *   偏移 10..19: 未初始化(被调用方可能读取偏移 0xC/0xE/0x10/0x12)
 */

#include "stm32f4xx_hal.h"

extern void func_113D8(uint32_t res, uint32_t enable);
extern void func_134A8(uint32_t id, void *cfg);
extern void func_133B0(uint32_t id, uint32_t val);
extern void func_1340E(uint32_t id, uint32_t a, uint32_t b);
extern void func_133B6(uint32_t id, uint32_t val);
extern void func_0D9B4(uint8_t *cmd);

typedef struct {
    uint16_t param2;    /* offset 0:  r1 参数, 半字存储 */
    uint16_t zero_2;    /* offset 2:  零, 半字存储 */
    uint32_t param1;    /* offset 4:  r0 参数, 整字存储 (str, not strh) */
    uint16_t zero_8;    /* offset 8:  零, 半字存储 */
    uint8_t  pad[10];   /* offset 10..19: 达到 20 字节总大小 (func_13420 读取至偏移 0x12) */
} dev_cfg_t;

void Device_Init_Variant3(uint32_t param1, uint32_t param2)
{
    uint8_t   cmd[4];
    dev_cfg_t cfg;
    uint32_t  dev_id = 0x40001000;

    func_113D8(0x10, 1);

    cfg.param2  = (uint16_t)param2;
    cfg.zero_2  = 0;
    cfg.param1  = param1;
    cfg.zero_8  = 0;

    func_134A8(dev_id, &cfg);
    func_133B0(dev_id, 1);
    func_1340E(dev_id, 1, 1);
    func_133B6(dev_id, 0);

    *((volatile uint32_t *)(dev_id + 0x24)) = 0;

    cmd[0] = 0x36;
    cmd[1] = 0x01;
    cmd[2] = 0x03;
    cmd[3] = 0x01;
    func_0D9B4(cmd);
}
