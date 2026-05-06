/**
 * @file func_75_fpu_frame_sender.c
 * @brief 函数: FPU 帧发送器 — 发送含浮点数据的命令帧
 * @addr  0x080126F8 - 0x08012788 (144 bytes, 含字面量池)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 发送格式: [0xEE, 0xB1, 0x07, HI(p1), LO(p1), HI(p2), LO(p2), 0x02,
 *             flag_byte, float_b3, float_b2, float_b1, float_b0,
 *             0xFF, 0xFC, 0xFF, 0xFF]
 *
 * 其中 flag_byte = (p3 & 0x0F) | (p4 == 0 ? 0 : 0x80)
 * float 值通过 VPUSH/VPOP 从 d0 (s0) 寄存器获取并以大端序发送。
 *
 * 参数:
 *   r0 = param1 — 16 位值
 *   r1 = param2 — 16 位值
 *   r2 = param3 — 8 位标志 (低 4 位有效)
 *   r3 = param4 — 选择位 (0 → bit7=0, 非0 → bit7=0x80)
 *   d0 = float  — 隐式浮点参数 (s0)
 *
 * 调用:
 *   func_123E8() @ 0x080123E8 — 字节级 SPI 发送器
 */

#include "stm32f4xx_hal.h"

extern void func_123E8(uint8_t byte);

/* ARM hard-float ABI: float param arrives in s0 (lower half of d0).
 * Original code does VPUSH {d0} to get float bytes from stack. */
void FPU_Frame_Sender(uint16_t param1, uint16_t param2, uint8_t param3,
                      uint32_t param4, float f_val)
{
    typedef union { float f; uint8_t b[4]; } float_bytes_t;
    float_bytes_t fb;
    uint8_t  flag_byte;
    uint32_t i;

    fb.f = f_val;

    func_123E8(0xEE);
    func_123E8(0xB1);
    func_123E8(0x07);
    func_123E8((uint8_t)(param1 >> 8));
    func_123E8((uint8_t)param1);
    func_123E8((uint8_t)(param2 >> 8));
    func_123E8((uint8_t)param2);
    func_123E8(0x02);

    /* flag_byte = (param3 & 0x0F) | (param4 ? 0x80 : 0) */
    flag_byte = (param3 & 0x0F) | (param4 ? 0x80 : 0);
    func_123E8(flag_byte);

    /* 发送 float 的 4 字节 (大端序: MSB 先, 对应 VPUSH 后栈顶的 s0 低地址字节) */
    for (i = 0; i < 4; i++) {
        func_123E8(fb.b[3 - i]);
    }

    func_123E8(0xFF);
    func_123E8(0xFC);
    func_123E8(0xFF);
    func_123E8(0xFF);
}
