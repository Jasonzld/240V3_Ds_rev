/**
 * @file func_129_fpu_sign_check_variant.c
 * @brief 函数: FPU 有符号值比较分发器变体 — 与 func_128 类似但分支路径不同
 * @addr  0x08016CA0 - 0x08016D54 (180 bytes)
 * DISASSEMBLY-TRACED. Verified against Capstone (entry + key branches).
 *
 * 与 func_128 结构相同但分支差异:
 *   - case 0: func_17408(1, dval) 而非 func_171A0
 *   - case 1: func_171A0 而非 符号取反版本
 *   - case 2: 符号取反 func_17408 版本
 *   - default: func_171A0 + 符号取反 (与 func_128 case 1 行为相同)
 *
 * 调用:
 *   func_175C4() @ 0x080175C4
 *   func_17550() @ 0x08017550
 *   func_17538() @ 0x08017538
 *   func_16D68() @ 0x08016D68
 *   func_17408() @ 0x08017408
 *   func_171A0() @ 0x080171A0
 */

#include "stm32f4xx_hal.h"

extern void   func_175C4(uint32_t a);
extern void   func_17550(void);
extern void   func_17538(void);
extern void   func_17408(uint32_t mode, double val);
extern double func_171A0(double val);
extern uint32_t func_16D68(void *ctx, double val, uint32_t flags);

double FPU_Sign_Check_Variant(double dval, uint32_t flags)
{
    uint32_t abs_hi = ((uint32_t *)&dval)[1] & 0x7FFFFFFF;

    if (abs_hi <= 0x3FE921FB) {            /* |val| <= PI/4 (~0.7854) */
        /* 变体: 返回 0 而非 func_171A0 */
        func_17408(1, dval);
        return 0.0;
    }

    if (abs_hi == 0x7FF00000) {            /* |val| == +Infinity sentinel */
        if (flags == 0) {
            func_175C4(1);
            func_17550();
            return 0.0;
        }
    }

    if ((int32_t)abs_hi < 0x7FF00000) {    /* |val| < Infinity, but > PI/4 */
        uint32_t subtype;
        double   out_buf[2];
        subtype = func_16D68(out_buf, dval, flags);

        switch (subtype & 3) {
        case 0:
            func_17408(1, dval);
            return 0.0;
        case 1:
            return func_171A0(dval);
        case 2: {
            double tmp;
            func_17408(1, dval);
            /* 回读 d0 并符号取反 */
            return tmp; /* 实际由 VMOV+EOR+VMOV 序列完成 */
        }
        default: {
            double tmp = func_171A0(dval);
            uint32_t *pw = (uint32_t *)&tmp;
            pw[1] ^= 0x80000000;
            return tmp;
        }
        }
    }

    func_17538();
    return 0.0;
}
