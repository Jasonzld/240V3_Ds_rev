/**
 * @file func_87_state_machine_controller.c
 * @brief 函数: 状态机控制器 — 多分支状态检查与控制
 * @addr  0x08013204 - 0x080132D8 (212 bytes, 含字面量池)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 根据设备状态 (func_133EC) 和多个 RAM 标志进行级联分支:
 *   分支 A: FLAG_3C != 0 → 比较 REG_34 与 0x10000 → 调用 func_112C4 或 func_115B8
 *   分支 B: FLAG_3C == 0 → 比较 REG_38 与 0x10000 → 调用 func_112C4 或 func_115B8
 *   最终: func_133B0(dev_id, 1)
 *
 * 参数: (无寄存器参数)
 * 常量源于字面量池 (literal pool) 0x080132B8–0x080132D4，已用 Capstone 反汇编验证。
 */

#include "stm32f4xx_hal.h"

extern uint32_t func_133EC(uint32_t id, uint32_t val);
extern void     func_133B0(uint32_t id, uint32_t val);
extern void     func_112C4(uint32_t a, uint32_t b);
extern void     func_115B8(uint32_t a, uint32_t b);

/* 设备 ID — 字面量池 0x080132B8 */
#define DEV_ID  0x40001000

/* RAM 标志 — 字面量池 0x080132BC–0x080132D4 */
#define FLAG_19  (*(volatile uint8_t  *)0x20000019)  /* +0xBC: byte flag 1, must be != 0         */
#define FLAG_6E  (*(volatile uint8_t  *)0x2000016E)  /* +0xC0: byte flag 2, must be == 0         */
#define FLAG_3C  (*(volatile uint8_t  *)0x2000023C)  /* +0xC4: byte flag 3, branch selector       */
#define REG_34   (*(volatile uint32_t *)0x20000234)  /* +0xC8: 32-bit value for branch A          */
#define HWORD_58 (*(volatile uint16_t *)0x20000058)  /* +0xCC: halfword used in both branches     */
#define HWORD_56 (*(volatile uint16_t *)0x20000056)  /* +0xD0: halfword (subtrahend A / arg B)    */
#define REG_38   (*(volatile uint32_t *)0x20000238)  /* +0xD4: 32-bit value for branch B          */
#define HWORD_38 (*(volatile uint16_t *)0x20000238)  /* halfword at same addr as REG_38            */

#define HWORD_34 (*(volatile uint16_t *)0x20000234)  /* halfword at same addr as REG_34 */

void State_Machine_Controller(void)
{
    if (func_133EC(DEV_ID, 1) != 1) {
        goto restart_device;
    }

    if (FLAG_19 == 0) {
        goto restart_device;
    }

    if (FLAG_6E != 0) {
        goto restart_device;
    }

    if (FLAG_3C != 0) {
        /* 分支 A: FLAG_3C 置位 — 使用 REG_34 (0x20000234) */
        if (REG_34 < 0x10000) {
            uint32_t diff = HWORD_58 - HWORD_56;
            func_112C4(0, diff * 2);
        } else {
            REG_34 = REG_34 - 0xFFFF;
            if (REG_34 < 0xFFFF) {
                func_115B8(DEV_ID, HWORD_34);
            }
        }
    } else {
        /* 分支 B: FLAG_3C 清零 — 使用 REG_38 (0x20000238) */
        if (REG_38 < 0x10000) {
            func_112C4(1, HWORD_56 * 2);
        } else {
            REG_38 = REG_38 - 0xFFFF;
            if (REG_38 < 0xFFFF) {
                func_115B8(DEV_ID, HWORD_38);
            }
        }
    }

restart_device:
    func_133B0(DEV_ID, 1);
}
