/**
 * func_173_fpu_double_wrapper
 *
 * @addr 0x08018E48 - 0x08018E58 (16 bytes)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * ABI 适配垫片: 将 r0:r1 的 double 移入 D0, 调用 func_16CA0, 结果从 D0 返回 r0:r1.
 *
 * BL target: 0x08016CA0 (func_16CA0)
 */

#include \"stm32f4xx_hal.h\"

extern double func_16CA0(double val);

/*
 * 0x08018E48: PUSH {R4,LR}
 * 0x08018E4A: VMOV D0, R0, R1   ; double in r0:r1 → D0
 * 0x08018E4E: BL func_16CA0     ; call with D0
 * 0x08018E52: VMOV R0, R1, D0   ; result D0 → r0:r1
 * 0x08018E56: POP {R4,PC}
 */
double func_173_fpu_double_wrapper(double value)
{
    return func_16CA0(value);
}
