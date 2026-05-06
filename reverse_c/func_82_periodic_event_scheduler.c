/**
 * @file func_82_periodic_event_scheduler.c
 * @brief 函数: 周期性事件调度器 — 多阶段状态机 + 定时器递减 + 速率计算
 * @addr  0x08012990 - 0x08013000 (1648 bytes, 含字面量池)
 *        0x08012D88 - 0x08012FFC (~632 bytes, 字面量池 — 60+ RAM/外设地址)
 * DISASSEMBLY-TRACED. Verified against Capstone (entry + key sections).
 *
 * 主事件循环状态机. 结构:
 *   1. 设备状态门控 (func_133EC gate)
 *   2. 阶段 A: 模式字节比较 + 分支选择
 *   3. 阶段 B: 多路状态链 + 比较分发 (5个分发块)
 *   4. 阶段 C: 补充比较分发
 *   5. 多路定时器递减 (6个定时器)
 *   6. 速率计算: UDIV ÷3600 + ÷1000 + MULS×100/SDIV÷60
 *   7. 尾部递减 (4组条件递减)
 *
 * 调用:
 *   func_133EC() @ 0x080133EC — 设备状态检查
 *   func_133B0() @ 0x080133B0 — 设备重启动
 *   func_10BD0() @ 0x08010BD0 — 标志回调
 *   func_1257A() @ 0x0801257A — 操作触发器 (r1/r3 变化)
 *   func_12604() @ 0x08012604 — 模式备份通知
 *   func_12454() @ 0x08012454 — 显示命令
 *   func_12484() @ 0x08012484 — 显示命令(多参数)
 *   func_124D6() @ 0x080124D6 — 显示命令(3参数)
 *   func_09490() @ 0x08009490 — 定时器配置 (r0=base, r1=val, r2=idx)
 *   func_0C160() @ 0x0800C160 — 条件回调
 */

#include "stm32f4xx_hal.h"

/* ---- 外部函数声明 ---- */
extern uint32_t func_133EC(uint32_t dev_id, uint32_t val);
extern void     func_133B0(uint32_t dev_id, uint32_t val);
extern void     func_10BD0(uint8_t val);
extern void     func_1257A(uint8_t p0, uint8_t p1, uint32_t p2, uint8_t p3);
extern void     func_12604(uint8_t param);
extern void     func_12454(uint8_t param);
extern void     func_12484(uint16_t p1, uint16_t p2, uint8_t p3);
extern void     func_124D6(uint8_t p1, uint8_t p2, uint8_t p3);
extern void     func_09490(uint32_t base, uint32_t val, uint32_t idx);
extern void     func_0C160(void);

/* ---- 设备 ID ---- */
#define DEV_ID    0x40014000

/* ---- RAM 变量 (从字面量池验证) ---- */
/* 阶段 A: 模式比较 */
#define MODE_BYTE       (*(volatile uint8_t  *)0x2000023D)  /* 当前模式字节 */
#define MODE_HWORD      (*(volatile uint16_t *)0x200000EA)  /* 模式半字 */
#define PHASE_A_CNT     (*(volatile uint8_t  *)0x200000E7)  /* 阶段A递增计数 */
#define THRESH_HWORD    (*(volatile uint16_t *)0x200000C8)  /* 阈值半字 */
#define FLAG_BRANCH_SEL (*(volatile uint8_t  *)0x20000019)  /* 分支选择标志 */

/* 阶段 B: 链控制 */
#define FLAG_CHAIN_CTL  (*(volatile uint8_t  *)0x20000168)  /* 链控制标志 */
#define CHAIN_VAL_A     (*(volatile uint8_t  *)0x200000CB)  /* 链值 A */
#define CHAIN_VAL_B     (*(volatile uint8_t  *)0x200000CC)  /* 链值 B */
#define CHAIN_VAL_C     (*(volatile uint8_t  *)0x200000CD)  /* 链值 C */
#define CHAIN_VAL_D     (*(volatile uint8_t  *)0x200000C9)  /* 链值 D */
#define CHAIN_VAL_E     (*(volatile uint8_t  *)0x200000CA)  /* 链值 E */

/* 阶段 B: 比较对 */
#define CMP_A_LO        (*(volatile uint16_t *)0x20000040)  /* 比较对 A-Lo */
#define CMP_A_HI        (*(volatile uint16_t *)0x20000052)  /* 比较对 A-Hi */
#define CMP_B_LO        (*(volatile uint8_t  *)0x200000D2)  /* 比较对 B-Lo */
#define CMP_B_HI        (*(volatile uint8_t  *)0x200000D4)  /* 比较对 B-Hi */
#define CMP_C_LO        (*(volatile uint8_t  *)0x200000D3)  /* 比较对 C-Lo */
#define CMP_C_HI        (*(volatile uint8_t  *)0x200000D6)  /* 比较对 C-Hi */
#define CMP_D_LO        (*(volatile uint8_t  *)0x200000D7)  /* 比较对 D-Lo */
#define CMP_D_HI        (*(volatile uint8_t  *)0x20000169)  /* 比较对 D-Hi */

/* 分发标志 */
#define DISP_FLAG_A     (*(volatile uint8_t  *)0x2000016E)  /* 分发标志 A */
#define MODE_MIRROR     (*(volatile uint8_t  *)0x200001C0)  /* 模式镜像字节 */
#define STATE           (*(volatile uint8_t  *)0x2000024A)  /* 全局状态字节 */

/* 数据镜像 */
#define VAL_IMAGE_HI    (*(volatile uint16_t *)0x20000184)  /* 镜像源 */
#define VAL_IMAGE_LO    (*(volatile uint16_t *)0x20000182)  /* 镜像目标 */

/* 定时器区: 6路递减 */
#define TIMER0_VAL      (*(volatile uint16_t *)0x200000C6)  /* 定时器0 值 */
#define TIMER0_FLAG     (*(volatile uint16_t *)0x20000064)  /* 定时器0 关联标志 */

/* 定时器1: 双重门控 */
#define TIMER1_VAL      (*(volatile uint16_t *)0x20000036)  /* 定时器1 值 */
#define TIMER1_DEC      (*(volatile uint16_t *)0x200000B0)  /* 定时器1 递减目标 */

/* 定时器2-5 */
#define TIMER2_VAL      (*(volatile uint16_t *)0x20000100)  /* 定时器2 值 */
#define TIMER3_VAL      (*(volatile uint16_t *)0x200000F2)  /* 定时器3 值 */
#define TIMER4_VAL      (*(volatile uint8_t  *)0x2000017D)  /* 定时器4 值 */
#define TIMER4_FLAG     (*(volatile uint8_t  *)0x2000017C)  /* 定时器4 关联标志 */
#define TIMER5_VAL      (*(volatile uint8_t  *)0x2000017F)  /* 定时器5 值 */
#define TIMER5_FLAG     (*(volatile uint8_t  *)0x2000017E)  /* 定时器5 关联标志 */

/* 加速器区 */
#define ACCUM_A         (*(volatile uint32_t *)0x2000013C)  /* 累加器 A */
#define ACCUM_A_UDIV    (*(volatile uint32_t *)0x20000148)  /* 累加器A ÷3600 */
#define ACCUM_B         (*(volatile uint32_t *)0x20000138)  /* 累加器 B */
#define ACCUM_B_UDIV    (*(volatile uint32_t *)0x20000144)  /* 累加器B ÷3600 */
#define ACCUM_C         (*(volatile uint32_t *)0x2000008C)  /* 累加器 C */
#define ACCUM_C_UDIV    (*(volatile uint32_t *)0x20000070)  /* 累加器C ÷3600 */
#define THRESH_UDIV     (*(volatile uint32_t *)0x2000015C)  /* UDIV 比较阈值 */
#define CLEAR_TARGET    (*(volatile uint32_t *)0x2000007C)  /* 清零目标 */

/* 速率计算 */
#define SPEED_VAL       (*(volatile uint16_t *)0x2000003E)  /* 速率输入 */
#define SPEED_REF       (*(volatile uint16_t *)0x2000004A)  /* 速率参考 */
#define SPEED_OFFSET    (*(volatile uint32_t *)0x20000094)  /* 速率偏移累加 */
#define SPEED_UDIV      (*(volatile uint32_t *)0x20000074)  /* 速率 ÷1000 */
#define COUNTDOWN_A     (*(volatile uint16_t *)0x200000D0)  /* 递减A */
#define COUNTDOWN_B     (*(volatile uint16_t *)0x20000190)  /* 递减B */
#define COUNTDOWN_B_SRC (*(volatile uint16_t *)0x2000018E)  /* 递减B重载值 */
#define RATE_FLAG       (*(volatile uint8_t  *)0x2000016F)  /* 速率标志 */

/* 条件回调门控 */
#define CB_GATE         (*(volatile uint32_t *)0x20000038)  /* 回调门控 */

/* 尾部递减 */
#define TAIL_DEC_A      (*(volatile uint8_t  *)0x20000174)  /* 尾部门控A */
#define TAIL_DEC_A_VAL  (*(volatile uint8_t  *)0x20000175)  /* 尾部递减A */
#define TAIL_DEC_B      (*(volatile uint16_t *)0x20000178)  /* 尾部门控B */
#define TAIL_DEC_B_VAL  (*(volatile uint16_t *)0x20000178)  /* 尾部递减B (同址) */
#define TAIL_DEC_C      (*(volatile uint8_t  *)0x2000017B)  /* 尾部门控C */
#define TAIL_DEC_C_VAL  (*(volatile uint8_t  *)0x2000017B)  /* 尾部递减C (同址) */
#define TAIL_DEC_D      (*(volatile uint32_t *)0x200001B8)  /* 尾部递减D */

/* 位段别名: 0x42408284 + offset */
#define BITBAND_BASE    0x42408284
#define BITBAND_CTL_0   (*(volatile uint32_t *)(BITBAND_BASE + 0x00))  /* offset 0 */
#define BITBAND_CTL_4   (*(volatile uint32_t *)(BITBAND_BASE - 0x04))  /* offset -4 → 0x42408280 */
#define BITBAND_CTL_34  (*(volatile uint32_t *)(BITBAND_BASE + 0x30))  /* offset +0x30 */
#define BITBAND_CTL_50  (*(volatile uint32_t *)(BITBAND_BASE + 0x34))  /* offset +0x34 → 0x424082B8 */
#define FLAG_STORE      (*(volatile uint8_t  *)0x20000032)  /* 存储标志 */
#define FLAG_CLEAR      (*(volatile uint8_t  *)0x20000033)  /* 清除标志 */

void Periodic_Event_Scheduler(void)
{
    /* ================================================================
     * 门控: 设备状态检查
     * ================================================================ */
    if (func_133EC(DEV_ID, 1) != 1) {
        goto restart_device;
    }

    /* ================================================================
     * 阶段 A: 模式字节比较
     * ================================================================ */
    if (MODE_BYTE < MODE_HWORD) {
        /* 字节 < 半字: 递增阶段A计数 */
        PHASE_A_CNT++;
    } else {
        /* 字节 >= 半字: 清零计数, 条件置位分支选择器 */
        PHASE_A_CNT = 0;
        if (THRESH_HWORD != 0) {
            FLAG_BRANCH_SEL = 1;
        }
    }

    /* ---- 分支门控: FLAG_BRANCH_SEL 非零则跳转到定时器区 ---- */
    if (FLAG_BRANCH_SEL != 0) {
        CHAIN_VAL_B = CHAIN_VAL_A - 1;
        goto mid_section_timers;
    }

    /* ================================================================
     * 阶段 B: 核心状态链
     * ================================================================ */
    if (FLAG_CHAIN_CTL == 0) {
        /* 链控制 == 0: 递增链值A, 递增链值B */
        CHAIN_VAL_A++;
        CHAIN_VAL_B++;
        /* 比较链值B vs 比较对A */
        if ((uint16_t)CHAIN_VAL_B < CMP_A_LO) {
            /* 未超阈值 */
        } else {
            /* 超过阈值: 递增链值C */
            CHAIN_VAL_C++;
        }
    } else {
        /* 链控制 != 0: 清零链值A, 链值B, 链值C */
        CHAIN_VAL_A = 0;
        CHAIN_VAL_B = 0;
        CHAIN_VAL_C = 0;
    }

    /* 子分支: CMP_D_HI 驱动链值D */
    if (CMP_D_HI != 0) {
        CHAIN_VAL_D++;
    } else {
        CHAIN_VAL_D = 0;
    }

    /* ================================================================
     * 阶段 B 分发: 4路比较 → 不同动作块
     * ================================================================ */

    /* 比较 0: CHAIN_VAL_A vs CMP_B_LO */
    if (CHAIN_VAL_A >= (uint16_t)CMP_B_LO) {
        /* ---- 分发块 A: DISP_FLAG_A 驱动 ---- */
        if (DISP_FLAG_A != 0) {
            func_1257A(MODE_MIRROR, 0x19, 4, 0x14);
            func_12604(0x19);
            STATE = 0x19;
            DISP_FLAG_A = 0;
            func_12454(0);
            VAL_IMAGE_LO = VAL_IMAGE_HI;
            FLAG_BRANCH_SEL = 0;
            func_10BD0(FLAG_BRANCH_SEL);
            func_09490(0x1F4, 0x1F4, 3);
            BITBAND_CTL_0 = 1;
            FLAG_STORE = 1;
            goto phase_c_entry;
        } else {
            func_1257A(MODE_MIRROR, 0x17, 4, 0x13);
            func_12604(0x17);
            func_12454(0);
            VAL_IMAGE_LO = VAL_IMAGE_HI;
            STATE = 0x17;
            DISP_FLAG_A = 0;
            func_10BD0(DISP_FLAG_A);
            func_09490(0x1F4, 0x1F4, 3);
            BITBAND_CTL_0 = 1;
            FLAG_STORE = 1;
            goto phase_c_entry;
        }
    }

    /* 比较 1: CHAIN_VAL_C vs CMP_B_HI */
    if (CHAIN_VAL_C >= (uint16_t)CMP_B_HI) {
        func_1257A(MODE_MIRROR, 0x23, 4, 0x17);
        func_12604(0x23);
        func_12454(0);
        VAL_IMAGE_LO = VAL_IMAGE_HI;
        STATE = 0x23;
        FLAG_BRANCH_SEL = 0;
        func_10BD0(FLAG_BRANCH_SEL);
        func_09490(0x1F4, 0x1F4, 4);
        goto phase_c_entry;
    }

    /* 比较 2: CHAIN_VAL_B vs CMP_C_LO */
    if (CHAIN_VAL_B >= (uint16_t)CMP_C_LO) {
        func_1257A(MODE_MIRROR, 0x18, 4, 0x15);
        func_12604(0x18);
        func_12454(0);
        VAL_IMAGE_LO = VAL_IMAGE_HI;
        STATE = 0x18;
        FLAG_BRANCH_SEL = 0;
        func_10BD0(FLAG_BRANCH_SEL);
        func_09490(0x1F4, 0x1F4, 4);
        goto phase_c_entry;
    }

    /* 比较 3: CHAIN_VAL_D vs CMP_C_HI */
    if (CHAIN_VAL_D >= (uint16_t)CMP_C_HI) {
        func_1257A(MODE_MIRROR, 0x19, 4, 0x14);
        func_12604(0x19);
        func_12454(0);
        VAL_IMAGE_LO = VAL_IMAGE_HI;
        STATE = 0x19;
        FLAG_BRANCH_SEL = 0;
        func_10BD0(FLAG_BRANCH_SEL);
        func_09490(0x1F4, 0x1F4, 2);
        BITBAND_CTL_4 = 1;
    }

    /* ================================================================
     * 阶段 C: 补充比较分发
     * ================================================================ */
phase_c_entry:
    if (CMP_D_HI != 0) {
        CHAIN_VAL_E++;
    } else {
        CHAIN_VAL_E = 0;
    }

    if (CHAIN_VAL_E >= (uint16_t)CMP_D_LO) {
        if (STATE != 0x1A) {
            func_1257A(MODE_MIRROR, 0x1A, 4, 0x16);
            func_12604(0x1A);
            func_12454(0);
            VAL_IMAGE_LO = VAL_IMAGE_HI;
            STATE = 0x1A;
            FLAG_BRANCH_SEL = 0;
            func_10BD0(FLAG_BRANCH_SEL);
            func_09490(0x1F4, 0x1F4, 2);
            BITBAND_CTL_4 = 1;
            CHAIN_VAL_E = 0;
        }
    }

    /* ================================================================
     * 定时器递减区: 6路独立递减定时器
     * ================================================================ */
mid_section_timers:

    /* 定时器0: 非零递减, 到零触发清零+func_12484 */
    if (TIMER0_VAL != 0) {
        TIMER0_VAL--;
        if (TIMER0_VAL == 0) {
            TIMER0_FLAG = 0;
            BITBAND_CTL_50 = 0;
            FLAG_CLEAR = 0;
            func_12484(0x10, 5, 0);
        }
    }

    /* 定时器1: 双重门控, 到零触发 func_10BD0(1) */
    if (TIMER1_VAL != 0 && TIMER1_DEC != 0) {
        TIMER1_DEC--;
        if (TIMER1_DEC == 0) {
            FLAG_BRANCH_SEL = 1;
            func_10BD0(FLAG_BRANCH_SEL);
        }
    }

    /* 定时器2: 非零递减, 到零清零位段 */
    if (TIMER2_VAL != 0) {
        TIMER2_VAL--;
        if (TIMER2_VAL == 0) {
            BITBAND_CTL_4 = 0;
            BITBAND_CTL_34 = 0;
        }
    }

    /* 定时器3: 非零递减, 到零触发 func_10BD0(0) */
    if (TIMER3_VAL != 0) {
        TIMER3_VAL--;
        if (TIMER3_VAL == 0) {
            FLAG_BRANCH_SEL = 0;
            func_10BD0(FLAG_BRANCH_SEL);
        }
    }

    /* 定时器4: 字节定时器 */
    if (TIMER4_VAL != 0) {
        TIMER4_VAL--;
        if (TIMER4_VAL == 0) {
            TIMER4_FLAG = 0;
        }
    }

    /* 定时器5: 字节定时器 */
    if (TIMER5_VAL != 0) {
        TIMER5_VAL--;
        if (TIMER5_VAL == 0) {
            TIMER5_FLAG = 0;
        }
    }

    /* ================================================================
     * 速率/频率调节区: VAL_IMAGE_LO 递减 + STATE 范围检查
     * ================================================================ */
    if (VAL_IMAGE_LO != 0 && FLAG_BRANCH_SEL == 0) {
        VAL_IMAGE_LO--;
        if (VAL_IMAGE_LO == 0xF) {
            /* STATE 不在 [0x17,0x1A] 且 STATE < 0x22 → 触发动作 */
            if (!(STATE >= 0x17 && STATE <= 0x1A) && STATE < 0x22) {
                func_12604(0);
                func_124D6(0, 6, 1);
            }
        } else if (VAL_IMAGE_LO == 0) {
            if (!(STATE >= 0x17 && STATE <= 0x1A) && STATE < 0x22) {
                func_12454(0xFF);
                func_12604(0);
                func_124D6(0, 6, 0);
            }
        }
    }

    /* 发送显示命令 0xE0 (独立路径, 来自 0x08012E20) */
    func_12454(0xE0);

    /* ================================================================
     * UDIV 累加器区: 3路累加 + ÷3600 速率计算
     * ================================================================ */

    /* 路 A: 主累加器 */
    ACCUM_A++;
    ACCUM_A_UDIV = ACCUM_A / 3600;

    /* 与速率参考比较 */
    if ((uint16_t)ACCUM_A_UDIV <= SPEED_REF) {
        /* 路 B: 次级累加器 */
        ACCUM_B++;
        ACCUM_B_UDIV = ACCUM_B / 3600;
    }

    /* 路 C: FLAG_BRANCH_SEL 门控的第三累加器 */
    if (FLAG_BRANCH_SEL != 0) {
        ACCUM_C++;
        ACCUM_C_UDIV = ACCUM_C / 3600;

        /* 达到阈值: 清零 + 模式备份通知 + 显示链 */
        if (ACCUM_C_UDIV >= THRESH_UDIV) {
            CLEAR_TARGET = 0;
            func_12604(0x22);
            func_12454(0);
            VAL_IMAGE_LO = VAL_IMAGE_HI;
            STATE = 0x22;
            FLAG_BRANCH_SEL = 0;
            func_10BD0(FLAG_BRANCH_SEL);
            func_09490(0x1F4, 0x1F4, 2);
        }
    }

    /* ================================================================
     * 速度计算: (SPEED × 100) / 60 + offset, 再 ÷1000
     * ================================================================ */
    {
        uint32_t speed_calc;

        speed_calc = (uint32_t)SPEED_VAL * 100;
        speed_calc = speed_calc / 60;
        SPEED_OFFSET += speed_calc;
        SPEED_UDIV = SPEED_OFFSET / 1000;

        /* 递减 COUNTDOWN_A (若非零) */
        if (COUNTDOWN_A != 0) {
            COUNTDOWN_A--;
        }

        /* 递减 COUNTDOWN_B (若非零), 到零重载 */
        if (COUNTDOWN_B != 0) {
            COUNTDOWN_B--;
            if (COUNTDOWN_B == 0) {
                COUNTDOWN_B = COUNTDOWN_B_SRC;
            }
        }

        /* FLAG_BRANCH_SEL 门控: 置位速率标志 */
        if (FLAG_BRANCH_SEL != 0) {
            RATE_FLAG = 1;
        }
    }

    /* ================================================================
     * 条件回调: CB_GATE 门控 func_0C160
     * ================================================================ */
    if (CB_GATE != 0) {
        func_0C160();
    }

    /* ================================================================
     * 尾部递减区: 4组条件递减
     * ================================================================ */
    if (TAIL_DEC_A != 0 && TAIL_DEC_A_VAL != 0) {
        TAIL_DEC_A_VAL--;
    }

    if (TAIL_DEC_B != 0 && TAIL_DEC_B_VAL != 0) {
        TAIL_DEC_B_VAL--;
    }

    if (TAIL_DEC_C != 0 && TAIL_DEC_C_VAL != 0) {
        TAIL_DEC_C_VAL--;
    }

    if (TAIL_DEC_D != 0) {
        TAIL_DEC_D--;
    }

    /* ================================================================
     * 出口: 设备重启动
     * ================================================================ */
restart_device:
    func_133B0(DEV_ID, 1);
    /* 返回 (pop {r4, pc}) */
}
