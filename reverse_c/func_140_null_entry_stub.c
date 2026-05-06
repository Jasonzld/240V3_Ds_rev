/**
 * @file func_140_decimal_scaler.c
 * @brief 函数: 十进制缩放器 — 按10的幂调整*ptr的值以匹配目标指数
 * @addr  0x080181D0 - 0x08018204 (52 bytes, 含字面量池)
 *
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 逻辑:
 *   - 若 ptr == NULL: 返回 NULL
 *   - 若 current_exp > target_exp: *ptr /= 10, current_exp-- (循环)
 *   - 若 current_exp < target_exp: *ptr *= 10, current_exp++ (循环)
 *   - 指数更新使用 uxtb 约束为 uint8
 *   - 除法使用 SDIV (有符号除法)
 *
 * 寄存器:
 *   r0 = ptr, r1 = current_exp, r2 = target_exp
 *   r4 = 10 (常量除数)
 *   r3 = 临时
 */

#include "stm32f4xx_hal.h"

/**
 * Decimal_Scaler @ 0x080181D0
 *
 * 反汇编追踪:
 *   0x080181D0: push {r4, lr}
 *   0x080181D2: cbnz r0, #0x080181D6   ; ptr != NULL → 跳转到循环1检查
 *   0x080181D4: pop {r4, pc}           ; ptr == NULL → 返回 NULL
 *
 *   0x080181D6: b #0x080181E6          ; → 循环1条件检查
 *
 *   ; 除法块 (0x080181D8-0x080181E4): current_exp > target_exp
 *   0x080181D8: movs r4, #0xa          ; r4 = 10
 *   0x080181DA: ldr r3, [r0]           ; r3 = *ptr
 *   0x080181DC: sdiv r3, r3, r4        ; r3 = *ptr / 10 (有符号)
 *   0x080181E0: str r3, [r0]           ; *ptr = r3
 *   0x080181E2: subs r3, r1, #1        ; r3 = current_exp - 1
 *   0x080181E4: uxtb r1, r3            ; current_exp = (uint8_t)(current_exp - 1)
 *
 *   ; 循环1检查 (0x080181E6):
 *   0x080181E6: cmp r1, r2             ; current_exp vs target_exp
 *   0x080181E8: bgt #0x080181D8         ; if (signed)current_exp > target_exp → 除法块
 *   0x080181EA: b #0x080181FA           ; → 循环2检查
 *
 *   ; 乘法块 (0x080181EC-0x080181F8): current_exp < target_exp
 *   0x080181EC: ldr r3, [r0]           ; r3 = *ptr
 *   0x080181EE: add.w r3, r3, r3, lsl #2  ; r3 = r3 + r3*4 = r3*5
 *   0x080181F2: lsls r3, r3, #1        ; r3 = r3*2 (total: r3 *= 10)
 *   0x080181F4: str r3, [r0]           ; *ptr = r3
 *   0x080181F6: adds r3, r1, #1        ; r3 = current_exp + 1
 *   0x080181F8: uxtb r1, r3            ; current_exp = (uint8_t)(current_exp + 1)
 *
 *   ; 循环2检查 (0x080181FA):
 *   0x080181FA: cmp r1, r2             ; current_exp vs target_exp
 *   0x080181FC: blt #0x080181EC         ; if (signed)current_exp < target_exp → 乘法块
 *   0x080181FE: nop
 *   0x08018200: b #0x080181D4           ; → 返回 (pop {r4, pc}, r0=ptr)
 */
void *Decimal_Scaler(int32_t *ptr, uint32_t current_exp, uint32_t target_exp)
{
    if (ptr == NULL) {
        return NULL;
    }

    /* 循环1: 当前指数 > 目标指数 → 缩小 (除法) */
    while ((int32_t)current_exp > (int32_t)target_exp) {
        *ptr = (int32_t)(*ptr / 10);           /* sdiv r3, r3, r4 */
        current_exp = (uint8_t)(current_exp - 1);  /* subs + uxtb */
    }

    /* 循环2: 当前指数 < 目标指数 → 放大 (乘法, 用 add+lsl 实现 *10) */
    while ((int32_t)current_exp < (int32_t)target_exp) {
        *ptr = (int32_t)(*ptr * 10);           /* add.w r3,r3,r3,lsl#2 + lsls r3,r3,#1 */
        current_exp = (uint8_t)(current_exp + 1);  /* adds + uxtb */
    }

    return ptr;  /* r0 保持不变, pop {r4, pc} */
}
