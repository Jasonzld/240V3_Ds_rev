/**
 * @file func_84_peripheral_state_checker.c
 * @brief 函数: 外设状态检查器 — 检查设备状态并执行状态机转换
 * @addr  0x080130BC - 0x08013158 (156 bytes)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 控制流 (基于 Capstone ARM Thumb 反汇编):
 *
 *   入口 0x080130BC: push {r4, lr}
 *   0x080130C0: func_133EC(0x40014800, 1) → 若 !=1 则 jump 0x0801313E 退出
 *   0x080130CA: 读字节 STATE @ 0x20000024 → 若 ==0 则 jump 0x08013102 (continuation)
 *   0x080130D0: 读字节 CTR   @ 0x20000025 → 若 ==0 则 jump 0x080130E2 (重置路径)
 *   0x080130D6: CTR-- → jump 0x08013102 (continuation)
 *
 *   重置路径 0x080130E2:
 *     STATE=0; func_094E8(0); func_133B6(DEV_ID,0); func_133B0(DEV_ID,1); return
 *
 *   Continuation 0x08013102:
 *     STATE = (STATE==0) ? 1 : 0;       // 字节翻转
 *     func_094E8(STATE);
 *     if (STATE != 0) func_115B8(DEV_ID, HW_1E); else func_115B8(DEV_ID, HW_20);
 *     func_133B0(DEV_ID, 1); return
 *
 * 参数: 无 (void)
 *
 * 调用:
 *   func_133EC() @ 0x080133EC — 设备状态查询
 *   func_094E8() @ 0x080094E8 — 状态处理
 *   func_115B8() @ 0x080115B8 — 半字参数处理
 *   func_133B6() @ 0x080133B6 — 设备关闭
 *   func_133B0() @ 0x080133B0 — 设备启动
 *
 * 字面量池 (0x08013144-0x08013154):
 *   0x40014800 — DEV_ID (GPIOH 基址)
 *   0x20000024 — STATE 字节 (状态标志, 根据值路由 func_115B8)
 *   0x20000025 — CTR   字节 (倒数计数器)
 *   0x2000001E — HW_1E  半字 (STATE!=0 时传给 func_115B8)
 *   0x20000020 — HW_20  半字 (STATE==0 时传给 func_115B8)
 */

#include "stm32f4xx_hal.h"

extern uint32_t func_133EC(uint32_t id, uint32_t val);
extern void     func_094E8(uint8_t val);
extern void     func_115B8(uint32_t id, uint16_t val);
extern void     func_133B6(uint32_t id, uint32_t val);
extern void     func_133B0(uint32_t id, uint32_t val);

#define DEV_ID   0x40014800  /* GPIOH 基址, 字面量池 @ 0x08013144 */

/* RAM 变量 — 从字面量池加载的地址 */
#define STATE     (*(volatile uint8_t  *)0x20000024)  /* 状态标志字节, 字面量池 @ 0x08013148 */
#define CTR       (*(volatile uint8_t  *)0x20000025)  /* 倒数计数器, 字面量池 @ 0x0801314C */
#define HW_1E     (*(volatile uint16_t *)0x2000001E)  /* 半字参数 A (STATE!=0 时使用), @ 0x08013150 */
#define HW_20     (*(volatile uint16_t *)0x20000020)  /* 半字参数 B (STATE==0 时使用), @ 0x08013154 */

void Peripheral_State_Checker(void)
{
    /* 0x080130BC: push {r4, lr}
     * 0x080130C0: func_133EC(DEV_ID, 1) */
    if (func_133EC(DEV_ID, 1) != 1) {
        /* 0x080130C8: bne → 0x0801313E (共享出口) */
        return;
    }

    /* 0x080130CA: 加载 STATE 字节 */
    if (STATE == 0) {
        /* 0x080130CE: cbz → 0x08013102 (continuation) */
        goto continuation;
    }

    /* STATE != 0: 检查 CTR 计数器 (0x080130D0) */
    if (CTR != 0) {
        /* 0x080130DA: CTR--; STRB; b → 0x08013102 (continuation) */
        CTR--;
        goto continuation;
    }

    /* CTR == 0: 重置路径 @ 0x080130E2 */
    STATE = 0;                         /* 0x080130E6: strb r0, [r1] @ 0x20000024 */
    func_094E8(0);                     /* 0x080130EC: bl func_094E8 */
    func_133B6(DEV_ID, 0);             /* 0x080130F4: bl func_133B6 */
    func_133B0(DEV_ID, 1);             /* 0x080130FC: bl func_133B0 */
    return;                            /* 0x08013100: pop {r4, pc} */
    /* 注意: 此路径在 STATE!=0 && CTR==0 时执行 (STATE==0 已被 cbz 跳转到 continuation) */

continuation:                          /* 0x08013102 */
    /* 字节翻转: STATE = (STATE == 0) ? 1 : 0
     * 0x08013104: ldrb + cbnz → 0x0801310C (zero) 或 0x08013108 (non-zero) */
    STATE = (STATE == 0) ? 1 : 0;     /* 0x08013110: strb */

    /* 0x08013114: ldrb STATE; 0x08013116: bl func_094E8(STATE) */
    func_094E8(STATE);

    /* 0x0801311E: cbz STATE → 选路 */
    if (STATE != 0) {
        /* 0x08013120-0x08013126: func_115B8(DEV_ID, HW_1E) */
        func_115B8(DEV_ID, HW_1E);
    } else {
        /* 0x0801312C-0x08013132: func_115B8(DEV_ID, HW_20) */
        func_115B8(DEV_ID, HW_20);
    }

    /* 0x08013138: func_133B0(DEV_ID, 1); 0x0801313E: nop; 0x08013140: b → pop {r4, pc} */
    func_133B0(DEV_ID, 1);
    /* 共享出口: 0x0801313E nop + 0x08013140 b 0x08013100 → pop {r4, pc} */
}
