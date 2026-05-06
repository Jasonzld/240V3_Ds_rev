/**
 * @file func_128_fpu_sign_check_and_branch.c
 * @brief 函数: FPU 有符号值比较分发器 — 取绝对值后按阈值分支执行不同 FPU 运算
 * @addr  0x08016BD8 - 0x08016C8C (180 bytes)
 * DISASSEMBLY-TRACED. Verified against Capstone (entry + key branches).
 *
 * 输入 double (栈传), 额外 uint32_t 参数.
 * 先去掉符号位, 与阈值比较:
 *   val <= threshold_A → 直接调用 func_171A0
 *   val == threshold_B 且 sub_arg == 0 → func_175C4(1) + func_17550
 *   val  < threshold_B → 调用 func_16D68 后按返回值 (0/1/2/3) 分支
 *   val  > threshold_B → 调用 func_17538
 *
 * 调用:
 *   func_175C4() @ 0x080175C4 — 后置处理
 *   func_17550() @ 0x08017550 — 清理/重置
 *   func_17538() @ 0x08017538 — 替代路径
 *   func_16D68() @ 0x08016D68 — FPU 结构写入+阈值检查 (func_130)
 *   func_17408() @ 0x08017408 — 模式处理 (func_133)
 *   func_171A0() @ 0x080171A0 — FPU 运算 (func_131)
 */

#include "stm32f4xx_hal.h"
#include <math.h>

extern void   func_175C4(uint32_t a);
extern void   func_17550(void);
extern void   func_17538(void);
extern void   func_17408(uint32_t mode, double val);
extern double func_171A0(double val);
extern uint32_t func_16D68(void *ctx, double val, uint32_t flags);

double FPU_Sign_Check_And_Branch(double dval, uint32_t flags)
{
    uint32_t abs_hi = ((uint32_t *)&dval)[1] & 0x7FFFFFFF;  /* 去掉符号位 */

    /* 阈值比较 (从字面量池加载) */
    if (abs_hi <= 0x3FE921FB) {            /* |val| <= PI/4 (~0.7854) */
        return func_171A0(dval);
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
            return func_171A0(dval);
        case 1:
            func_17408(1, dval);
            return 0.0;
        }
        case 2: {
            double tmp = func_171A0(dval);
            uint32_t *pw = (uint32_t *)&tmp;
            pw[1] ^= 0x80000000;
            return tmp;
        }
        default:
            func_17408(1, dval);
            return 0.0;
        }
    }

    /* |val| > threshold_B: 替代路径 */
    func_17538();
    return 0.0;
}
