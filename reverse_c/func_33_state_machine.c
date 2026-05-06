/**
 * @file func_33_state_machine.c
 * @brief 函数: StateMachine_Monitor — 状态机监控与门控处理器
 * @addr  0x0800D5E4 - 0x0800D730 (332 bytes, 含字面量池)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *        0x0800D700 - 0x0800D730 (48 bytes, 字面量池 — 12 个 32-bit 条目)
 *
 * 基于全局状态字节 (0x2000024A) 的门控逻辑, 控制两路计数器
 * 及其关联的动作触发。仅当状态字节通过门控检查且 FLAG_ED 置位时
 * 才执行后续处理, 否则直接返回。
 *
 * 门控逻辑 (STATE = [0x2000024A]):
 *   允许进入处理的状态值:
 *     0x00-0x16, 0x1B-0x21, 0x24-0xFF
 *   跳过处理 (直接返回) 的状态值:
 *     0x17, 0x18, 0x19, 0x1A, 0x22, 0x23
 *   当 FLAG_ED ([0x200000ED]) == 0 时无条件返回 (无操作)。
 *
 * 两路交叉监控计数器 (每个路径复位对方计数器, 读取己方计数器):
 *   路 A (COUNTER_SELECT==0): 复位 COUNTER_B, 读取/递增 COUNTER_A
 *   路 B (COUNTER_SELECT!=0): 复位 COUNTER_A, 读取/递增 COUNTER_B
 *   语义: 活跃路径监控己方活动, 同时复位对方路径的计数
 *   阈值: 300 (0x12C)
 *   两路均达阈值后触发动作链 (双向握手)
 *
 * 调用:
 *   func_12604()   @ 0x08012604 — 操作 A
 *   func_0E2D8()   @ 0x0800E2D8 — 操作 B
 *   func_12454()   @ 0x08012454 — 操作 C
 *   func_10BD0()   @ 0x08010BD0 — 标志更新回调
 *
 * 字面量池: 0x0800D700 - 0x0800D730 (13 个条目)
 *   0x0800D700: 0x200000ED (FLAG_ED)
 *   0x0800D704: 0x2000024A (STATE)
 *   0x0800D708: 0x42410230 (COUNTER_SELECT, 外设寄存器)
 *   0x0800D70C: 0x2000021C (COUNTER_B)
 *   0x0800D710: 0x2000021A (COUNTER_A)
 *   0x0800D714: 0x2000021E (FLAG_1E)
 *   0x0800D718: 0x2000021F (FLAG_1F)
 *   0x0800D71C: 0x200000EE (FLAG_EE)
 *   0x0800D720: 0x20000019 (FLAG_19)
 *   0x0800D724: 0x200000EF (FLAG_EF)
 *   0x0800D728: 0x200000F0 (VAL_F0, uint16_t 源)
 *   0x0800D72C: 0x200000F2 (VAL_F2, uint16_t 目标)
 */

#include "stm32f4xx_hal.h"

/* RAM 变量 */
#define FLAG_ED         (*(volatile uint8_t  *)0x200000ED)
#define STATE           (*(volatile uint8_t  *)0x2000024A)
#define COUNTER_SELECT  (*(volatile uint32_t *)0x42410230)
#define COUNTER_A       (*(volatile uint16_t *)0x2000021A)
#define COUNTER_B       (*(volatile uint16_t *)0x2000021C)
#define FLAG_1E         (*(volatile uint8_t  *)0x2000021E)
#define FLAG_1F         (*(volatile uint8_t  *)0x2000021F)
#define FLAG_EE         (*(volatile uint8_t  *)0x200000EE)
#define FLAG_19         (*(volatile uint8_t  *)0x20000019)
#define FLAG_EF         (*(volatile uint8_t  *)0x200000EF)
#define VAL_F0          (*(volatile uint16_t *)0x200000F0)
#define VAL_F2          (*(volatile uint16_t *)0x200000F2)

/* 外部函数声明 */
extern void func_12604(uint32_t mode);     /* 0x08012604 */
extern void func_0E2D8(uint32_t mode);     /* 0x0800E2D8 */
extern void func_12454(uint32_t mode);     /* 0x08012454 */
extern void func_10BD0(uint8_t  flag);     /* 0x08010BD0 */

/* ================================================================
 * StateMachine_Monitor() @ 0x0800D5E4
 *
 * push {r4, lr} — 保存 lr 用于子函数调用, r4 未实际使用
 *
 * 执行流程:
 *   1. 门控检查 → 状态字节未通过则 pop 返回
 *   2. 根据 COUNTER_SELECT 选择路 A 或路 B
 *   3. 递增对应计数器, 达到阈值 300 时触发动作链
 *   4. 动作链涉及 3 个子函数调用和标志位更新
 *   5. 尾部: 条件复制 VAL_F0 → VAL_F2
 *   6. pop {r4, pc} 返回 (跳转到 0x0800D60C 的公共出口)
 * ================================================================ */
void StateMachine_Monitor(void)
{
    uint8_t  st;          /* r0 */
    uint16_t cnt;         /* r0 */

    /* ---- 门控: FLAG_ED 检查 ---- */
    st = FLAG_ED;                                    /* 0x0800D5E6-E8: ldr r0,=FLAG_ED; ldrb r0,[r0] */
    if (st == 0) {                                   /* 0x0800D5EA: cbz r0, #ret */
        goto ret;                                    /* FLAG_ED==0 → 无操作, 直接返回 */
    }

    /* ---- 门控: STATE 范围检查 ---- */
    st = STATE;                                      /* 0x0800D5EC-EE: ldr r0,=STATE; ldrb r0,[r0] */
    if (st <= 0x16) {                                /* 0x0800D5F0-F2: cmp r0,#0x16; ble */
        goto check_skip_vals;                        /* 跳过 >0x16 子区间检查 */
    }

    /* STATE > 0x16: 检查 0x17-0x1A 跳过区间 */
    st = STATE;                                      /* 0x0800D5F4-F6: 重新读取 (volatile) */
    if (st < 0x1B) {                                 /* 0x0800D5F8-FA: cmp r0,#0x1B; blt #ret */
        goto ret;                                    /* STATE ∈ {0x17,0x18,0x19,0x1A} → 跳过 */
    }

check_skip_vals:
    /* 检查 0x22 和 0x23 跳过值 */
    st = STATE;                                      /* 0x0800D5FC-FE: ldr r0,=STATE; ldrb r0,[r0] */
    if (st == 0x22) {                                /* 0x0800D600-02: cmp r0,#0x22; beq #ret */
        goto ret;                                    /* STATE==0x22 → 跳过 */
    }
    st = STATE;                                      /* 0x0800D604-06: 重新读取 */
    if (st == 0x23) {                                /* 0x0800D608-0A: cmp r0,#0x23; bne #main */
        goto ret;                                    /* STATE==0x23 → 跳过 */
    }
    /* STATE != 0x23 → 落入主处理体 (bne 跳转到 0x0800D60E) */

    /* ================================================================
     * 主处理体: 根据 COUNTER_SELECT 选择计数器路径
     * ================================================================ */

    /* 检查 COUNTER_SELECT (外设寄存器 32-bit 读取) */
    if (COUNTER_SELECT == 0) {                       /* 0x0800D60E-12: ldr r0,=SEL; ldr r0,[r0]; cbnz r0,#pathB */

        /* ---- 路 A: COUNTER_SELECT == 0 ----
         * 复位对方计数器 (COUNTER_B), 读取/递增己方计数器 (COUNTER_A) */
        COUNTER_B = 0;                               /* 0x0800D614-18: strh r0,[0x2000021C] — 复位对方 */

        cnt = COUNTER_A;                             /* 0x0800D61A-1C: ldrh r0,[COUNTER_A] — 读取己方 */
        if (cnt < 300) {                             /* 0x0800D61E-22: cmp.w r0,#0x12c; bge #threshA */
            /* 未达阈值: 递增己方计数器并返回 */
            cnt = COUNTER_A;                         /* 0x0800D624-26: 重新读取 (volatile) */
            cnt = cnt + 1;                           /* 0x0800D628: adds r0,r0,#1 */
            COUNTER_A = cnt;                         /* 0x0800D62A-2C: strh r0,[COUNTER_A] */
            goto tail;                               /* 0x0800D62E: b #tail */
        }

        /* 达到阈值 300 */
threshA:
        FLAG_1E = 1;                                 /* 0x0800D630-34: movs r0,#1; strb [FLAG_1E] */

        st = FLAG_1F;                                /* 0x0800D636-38: ldrb r0,[FLAG_1F] */
        if (st == 0) {                               /* 0x0800D63A-3C: cmp r0,#0; beq #tail */
            goto tail;                               /* FLAG_1F==0 → 等待另一路 */
        }

        /* 两路均达阈值 (FLAG_1E=1, FLAG_1F!=0) */
        FLAG_1F = 0;                                 /* 0x0800D63E-42: movs r0,#0; strb [FLAG_1F] */

        st = FLAG_EE;                                /* 0x0800D644-46: ldrb r0,[FLAG_EE] */
        if (st == 0) {                               /* 0x0800D648-4A: cmp r0,#0; beq #tail */
            goto tail;                               /* FLAG_EE==0 → 跳过动作 */
        }

        /* 执行动作链 A */
        func_12604(2);                               /* 0x0800D64C-4E: movs r0,#2; bl */
        func_0E2D8(2);                               /* 0x0800D652-54: movs r0,#2; bl */
        FLAG_19 = 0;                                 /* 0x0800D658-5C: movs r0,#0; strb [FLAG_19] */
        st = FLAG_19;                                /* 0x0800D65E-60: ldrb r0,[FLAG_19] (reload) */
        func_10BD0(st);                              /* 0x0800D662: bl func_10BD0 */
        goto tail;                                   /* 0x0800D666: b #tail */

    } else {
        /* ---- 路 B: COUNTER_SELECT != 0 ----
         * 复位对方计数器 (COUNTER_A), 读取/递增己方计数器 (COUNTER_B) */
pathB:
        COUNTER_A = 0;                               /* 0x0800D668-6C: strh r0,[0x2000021A] — 复位对方 */

        cnt = COUNTER_B;                             /* 0x0800D66E-70: ldrh r0,[COUNTER_B] — 读取己方 */
        if (cnt < 300) {                             /* 0x0800D672-76: cmp.w r0,#0x12c; bge #threshB */
            /* 未达阈值: 递增己方计数器并返回 */
            cnt = COUNTER_B;                         /* 0x0800D678-7A: 重新读取 (volatile) */
            cnt = cnt + 1;                           /* 0x0800D67C: adds r0,r0,#1 */
            COUNTER_B = cnt;                         /* 0x0800D67E-80: strh r0,[COUNTER_B] */
            goto tail;                               /* 0x0800D682: b #tail */
        }

        /* 达到阈值 300 */
threshB:
        FLAG_1F = 1;                                 /* 0x0800D684-88: movs r0,#1; strb [FLAG_1F] */

        st = FLAG_1E;                                /* 0x0800D68A-8C: ldrb r0,[FLAG_1E] */
        if (st == 0) {                               /* 0x0800D68E: cbz r0, #tail */
            goto tail;                               /* FLAG_1E==0 → 等待另一路 */
        }

        /* 两路均达阈值 (FLAG_1F=1, FLAG_1E!=0) */
        FLAG_1E = 0;                                 /* 0x0800D690-94: movs r0,#0; strb [FLAG_1E] */

        st = FLAG_EE;                                /* 0x0800D696-98: ldrb r0,[FLAG_EE] */
        if (st == 0) {                               /* 0x0800D69A: cbz r0, #altB */
            goto altB;                               /* FLAG_EE==0 → 备选动作 */
        }

        /* 执行动作链 B (FLAG_EE != 0) */
        func_12604(2);                               /* 0x0800D69C-9E: movs r0,#2; bl */
        func_0E2D8(2);                               /* 0x0800D6A2-A4: movs r0,#2; bl */
        func_12454(0);                               /* 0x0800D6A8-AA: movs r0,#0; bl */
        FLAG_19 = 1;                                 /* 0x0800D6AE-B2: movs r0,#1; strb [FLAG_19] */
        st = FLAG_19;                                /* 0x0800D6B4-B6: ldrb r0,[FLAG_19] (reload) */
        func_10BD0(st);                              /* 0x0800D6B8: bl func_10BD0 */
        goto tail;                                   /* 0x0800D6BC: b #tail */

        /* 备选动作 B (FLAG_EE == 0) */
altB:
        func_12604(2);                               /* 0x0800D6BE-C0: movs r0,#2; bl */
        func_0E2D8(2);                               /* 0x0800D6C4-C8: movs r0,#2; bl */
        func_12454(0);                               /* 0x0800D6CA-CC: movs r0,#0; bl */

        /* 翻转 FLAG_19 */
        st = FLAG_19;                                /* 0x0800D6D0-D2: ldrb r0,[FLAG_19] */
        if (st != 0) {                               /* 0x0800D6D4: cbnz r0, #set0 */
            st = 0;                                  /* 0x0800D6DA: movs r0,#0 — 有 → 无 */
        } else {
            st = 1;                                  /* 0x0800D6D6: movs r0,#1 — 无 → 有 */
        }
        FLAG_19 = st;                                /* 0x0800D6DC-DE: strb r0,[FLAG_19] */
        st = FLAG_19;                                /* 0x0800D6E0-E2: ldrb r0,[FLAG_19] (reload) */
        func_10BD0(st);                              /* 0x0800D6E4: bl func_10BD0 */
    }

    /* ================================================================
     * 尾部处理: 条件复制 VAL_F0 → VAL_F2
     *   仅当 FLAG_EF != 0 且 FLAG_19 != 0 时执行复制
     * ================================================================ */
tail:
    st = FLAG_EF;                                    /* 0x0800D6E8-EA: ldrb r0,[FLAG_EF] */
    if (st == 0) {                                   /* 0x0800D6EC: cbz r0, #ret */
        goto ret;
    }

    st = FLAG_19;                                    /* 0x0800D6EE-F0: ldrb r0,[FLAG_19] */
    if (st == 0) {                                   /* 0x0800D6F2: cbz r0, #ret */
        goto ret;
    }

    /* FLAG_EF != 0 && FLAG_19 != 0 → 复制 */
    VAL_F2 = VAL_F0;                                 /* 0x0800D6F4-FA: ldrh r0,[VAL_F0]; strh r0,[VAL_F2] */
    /* 然后落入 nop → ret */

ret:
    return;                                          /* 0x0800D6FC: nop (对齐) */
                                                     /* 0x0800D6FE: b #0x0800D60C → pop {r4,pc} */
}
