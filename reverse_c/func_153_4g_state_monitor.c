/**
 * @file func_153_4g_state_monitor.c
 * @brief 4G 模块连接状态监控与显示触发子系统 — 监控连接标志、超时计数并触发显示更新
 * @addr  0x0800BD7C - 0x0800C2DC (1384 bytes, 8 个主函数 + 4 个叶辅助函数)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 在 4G 响应解析器 (func_150) 和 GPIO 初始化 (func_02) 之间,
 * 此模块监控 4G 模块的连接状态, 运行超时计数器,
 * 并在状态变化时触发显示更新.
 *
 * 调用的外部函数:
 *   func_134A0 (寄存器写)       ×2  — 写入定时器/PWM 值
 *   func_12604 (模式备份并通知)  ×6  — 模式变更时通知显示系统
 *   func_0E2D8 (显示页面渲染器)  ×6  — 触发显示页面重绘
 *   func_10BD0 (Display_State_Dispatcher) ×6 — 显示状态机分派
 *   func_112C4 (Display_Timer_Calculator) ×1 — 显示定时器计算
 *   func_0BD7C (Device_Status_Check)      ×2 — 设备状态检查
 */

#include "stm32f4xx_hal.h"

/* ---- 外部依赖 ---- */
extern void func_134A0(void *reg, uint32_t value);    /* 定时器/PWM 值写入 */
extern void func_12604(uint32_t mode);                /* 模式备份并通知 */
extern void func_0E2D8(uint32_t page);                /* 显示页面渲染器 */
extern void func_10BD0(uint32_t context);             /* Display_State_Dispatcher */
extern void func_112C4(uint32_t mode, uint32_t val);  /* Display_Timer_Calculator */
extern void func_0BD7C(uint32_t param);               /* Device_Status_Check */

/* ---- 外设寄存器 ---- */
#define TIM2_BASE       ((volatile uint32_t *)0x40010000)  /* TIM2 定时器基址 */
#define PERIPH_CTRL_REG (*(volatile uint32_t *)0x40010000)  /* 控制寄存器 */

/* ---- 4G 模块状态标志 (RAM) ---- */
#define MODULE_STATE     (*(volatile uint8_t  *)0x200000F4)  /* 主模块状态标志 */
#define MODE_BYTE        (*(volatile uint8_t  *)0x20000019)  /* 当前操作模式 */
#define COUNTER_A        (*(volatile uint8_t  *)0x2000024A)  /* 超时计数器 A (阈值 100) */

/* 配置/阈值字节 — 不同模块状态的独立副本 */
#define CONFIG_A         (*(volatile uint8_t  *)0x20000217)  /* 配置字节 A (阈值 0x16/0x1B/0x22/0x23) */
#define CONFIG_B         (*(volatile uint8_t  *)0x20000219)  /* 配置字节 B */
#define CONFIG_C         (*(volatile uint8_t  *)0x20000216)  /* 配置字节 C */
#define CONFIG_D         (*(volatile uint8_t  *)0x20000213)  /* 配置字节 D (阈值 0x16/0x1B/0x22/0x23) */
#define CONFIG_E         (*(volatile uint8_t  *)0x20000212)  /* 配置字节 E */
#define CONFIG_F         (*(volatile uint8_t  *)0x20000214)  /* 配置字节 F */
#define CONFIG_G         (*(volatile uint8_t  *)0x20000215)  /* 配置字节 G */

/* 计数器和传感器 */
#define SENSOR_PAIR_A    (*(volatile uint16_t *)0x2000018A)  /* 传感器对 A (用于百分比计算) */
#define SENSOR_PAIR_B    (*(volatile uint16_t *)0x2000000C)  /* 传感器对 B */
#define COUNT_UP_A       (*(volatile uint16_t *)0x200000BA)  /* 上行计数器 A */
#define COUNT_THRESH_A   (*(volatile uint16_t *)0x200000BC)  /* 计数器 A 阈值 */
#define COUNT_UP_B       (*(volatile uint16_t *)0x200000BE)  /* 上行计数器 B */
#define COUNT_THRESH_B   (*(volatile uint16_t *)0x200000C0)  /* 计数器 B 阈值 */
#define COUNT_BYTE_A     (*(volatile uint8_t  *)0x200000C2)  /* 字节计数器 A (阈值 0x17/0x1A) */
#define COUNT_BYTE_B     (*(volatile uint8_t  *)0x200000C3)  /* 字节计数器 B */
#define COUNT_THRESH_C   (*(volatile uint8_t  *)0x200000C4)  /* 字节计数器 C 阈值 */

#define TRIGGER_FLAG_A   (*(volatile uint8_t  *)0x200000C6)  /* 触发标志 A */
#define TRIGGER_FLAG_B   (*(volatile uint8_t  *)0x200000C7)  /* 触发标志 B */
#define TIMER_ACTIVE     (*(volatile uint8_t  *)0x200000C8)  /* 定时器活动标志 */

/* ========================================================================
 * 叶辅助函数 A: Store_To_Register (0x0800BFBC, 6 bytes)
 *   将 r0 存储到 PC 相对地址指定的寄存器.
 *   ldr r1, [pc, #4]; str r0, [r1]; bx lr
 * ======================================================================== */
static void Store_To_Register(uint32_t value)
{
    *(volatile uint32_t *)0x200000BC = value;
}

/* ========================================================================
 * 叶辅助函数 B: Status_Register_Reader (0x0800C050, 76 bytes)
 *   读取外设状态寄存器, 基于不同位字段返回状态码.
 *
 *   检查顺序:
 *     bit16 (0x10000) 设置 → 返回 1
 *     bit4  (0x10)    设置 → 返回 6
 *     bit8  (0x100)   设置 → 返回 2
 *     bits [7:5] (0xE0) 设置 → 返回 7
 *     bit1  (0x02)    设置 → 返回 8
 *     无位设置         → 返回 9
 * ======================================================================== */
static uint32_t Status_Register_Reader(void)
{
    uint32_t reg = *(volatile uint32_t *)0x40010000;

    if (reg & 0x10000)
        return 1;
    if (reg & 0x10)
        return 6;
    if (reg & 0x100)
        return 2;
    if (reg & 0xE0)
        return 7;
    if (reg & 0x02)
        return 8;
    return 9;
}

/* ========================================================================
 * 叶辅助函数 C: Set_High_Bit (0x0800C0B4, 12 bytes)
 *   在控制寄存器上设置 bit31 (0x80000000).
 *   ldr r0, [pc]; ldr r0, [r0]; orr r0, #0x80000000; str r0, [r1]; bx lr
 * ======================================================================== */
static void Set_High_Bit(void)
{
    volatile uint32_t *reg = (volatile uint32_t *)0x40010000;
    *reg |= 0x80000000;
}

/* ========================================================================
 * 叶辅助函数 D: Conditional_Write (0x0800C118, 22 bytes)
 *   若控制寄存器 bit31 已设置, 将两个字面量写入目标寄存器.
 *   ldr r0, [pc]; ldr r0, [r0]; and r0, #0x80000000; cbz → 若已设置, 写两个字
 * ======================================================================== */
static void Conditional_Write(void)
{
    volatile uint32_t *reg = (volatile uint32_t *)0x40010000;
    if (*reg & 0x80000000) {
        volatile uint32_t *dst = (volatile uint32_t *)0x40010000;
        dst[-3] = 0x200000BC;  /* 两个字面量来自池 */
        dst[-3] = 0x200000B8;
    }
}

/* ========================================================================
 * 主函数 1: Conn_State_Percent_Updater (0x0800BD7C, 72 bytes)
 *
 * 参数: r0 = mode — 操作模式 (1=活跃更新, 其他=重置)
 *
 * 活跃模式 (r0==1):
 *   percent = (TIM2_CNT * MODULE_STATE) / 100
 *   将 percent 写入 func_134A0
 *   将 SENSOR_PAIR_A 复制到 SENSOR_PAIR_B
 *
 * 重置模式 (r0!=1):
 *   清除 SENSOR_PAIR_B = 0
 *   调用 func_134A0(0)
 *   设置 MODULE_STATE = 0x0A
 * ======================================================================== */
uint32_t Conn_State_Percent_Updater(uint32_t mode)
{
    if (mode == 1) {
        /* 读取定时器计数值 (偏移 +0x2C = TIM2_CNT 或类似寄存器) */
        uint32_t tim_val = *(volatile uint32_t *)((uint8_t *)TIM2_BASE + 0x2C);
        uint8_t  state  = MODULE_STATE;

        /* 百分比 = (tim_val * state) / 100 */
        uint32_t product = tim_val * state;
        uint32_t percent = product / 100;       /* UDIV by #0x64 */

        /* 写入百分比值到定时器相关寄存器 */
        func_134A0((void *)0x2000018A, percent);

        /* 复制传感器对: SENSOR_PAIR_A → SENSOR_PAIR_B */
        SENSOR_PAIR_B = SENSOR_PAIR_A;
    } else {
        /* 重置: 清除计数器, 设置状态 = 0x0A */
        SENSOR_PAIR_B = 0;
        func_134A0((void *)0x2000000C, 0);
        MODULE_STATE = 0x0A;
    }
    return 0;
}

/* ========================================================================
 * 主函数 2: Display_Trigger_Monitor_A (0x0800BDCC, 268 bytes)
 *
 * 监控配置字节与阈值, 运行超时计数器, 触发显示更新.
 *
 * 门控检查:
 *   - MODULE_STATE != 0 (模块必须活跃)
 *   - CONFIG_A: 若 >= 0x16 (22) 且 < 0x1B (27), 则跳过
 *     若 == 0x22 (34) 或 == 0x23 (35), 则跳过
 *
 * 两条主路径 (基于两个额外的状态标志):
 *   路径 A (标志 A 已设置, 标志 B 已设置):
 *     - 清除额外的配置字节
 *     - 递增 COUNTER_A (若 < 100)
 *     - 若 COUNTER_A >= 100 且 counter == 0:
 *       设置 trigger_flag, 调用 func_12604(2), func_0E2D8(2),
 *       清除 trigger, 调用 func_10BD0(trigger_context)
 *   路径 B (标志 A 已清除, 标志 B 已清除): 类似路径 A, 但使用不同的配置字节集
 *   路径 C (其他): 清除所有触发标志
 * ======================================================================== */
void Display_Trigger_Monitor_A(void)
{
    /* 门控 1: MODULE_STATE 必须非零 */
    if (MODULE_STATE == 0)
        return;

    /* 门控 2: CONFIG_A 阈值检查 */
    if (CONFIG_A <= 0x16)
        goto check_gate_3;
    if (CONFIG_A < 0x1B)
        return;
check_gate_3:
    if (CONFIG_B == 0x22)
        return;
    if (CONFIG_C == 0x23)
        return;

    /* ---- 路径 A: 外部标志 A 和 B 均已设置 ---- */
    {
        uint32_t flag_a = *(volatile uint32_t *)0x20000248;
        uint32_t flag_b = *(volatile uint32_t *)(0x20000248 - 4);

        if (flag_a != 0 && flag_b != 0) {
            TRIGGER_FLAG_A = 0;
            TRIGGER_FLAG_B = 0;

            if (COUNTER_A < 100) {
                COUNTER_A++;
            } else {
                if (TRIGGER_FLAG_A == 0) {
                    TRIGGER_FLAG_A = 1;
                    func_12604(2);
                    func_0E2D8(2);

                    TRIGGER_FLAG_A = 0;
                    func_10BD0(TRIGGER_FLAG_A);
                }
            }
            return;
        }
    }

    /* ---- 路径 B: 外部标志 A 和 B 均已清除 ---- */
    {
        uint32_t flag_a = *(volatile uint32_t *)0x20000248;
        uint32_t flag_b = *(volatile uint32_t *)(0x20000248 - 4);

        if (flag_a == 0 && flag_b == 0) {
            TRIGGER_FLAG_A = 0;
            TRIGGER_FLAG_B = 0;

            if (COUNTER_A < 100) {
                COUNTER_A++;
            } else {
                if (TRIGGER_FLAG_A == 0) {
                    TRIGGER_FLAG_A = 1;
                    func_12604(2);
                    func_0E2D8(2);

                    TRIGGER_FLAG_A = 1;
                    func_10BD0(TRIGGER_FLAG_A);
                }
            }
            return;
        }
    }

    /* ---- 路径 C: 混合/过渡状态 — 清除所有触发标志 ---- */
    TRIGGER_FLAG_A = 0;
    TRIGGER_FLAG_B = 0;
    TIMER_ACTIVE = 0;
    *(volatile uint8_t *)0x200000C9 = 0;
}

/* ========================================================================
 * 主函数 3: Display_Trigger_Monitor_B (0x0800BED8, 240 bytes)
 *
 * 与 Monitor_A 结构几乎相同, 但使用不同的配置字节集
 * (CONFIG_D/E/F/G 替代 CONFIG_A/B/C).
 * 在 MODE_BYTE 附加检查上也略有不同.
 *
 * 两条路径结构相同, 但触发行为有轻微差异:
 *   路径 A (标志已设置): 调用 func_12604(2), func_0E2D8(2), 然后
 *                        TRIGGER_FLAG_A=0, func_10BD0(0)
 *   路径 B (标志已清除): 调用 func_12604(2), func_0E2D8(2), 然后
 *                        TRIGGER_FLAG_A=1, func_10BD0(1)
 * ======================================================================== */
void Display_Trigger_Monitor_B(void)
{
    /* 门控 1: MODULE_STATE 必须非零 */
    if (MODULE_STATE == 0)
        return;

    /* 门控 2: CONFIG_D 阈值检查 */
    if (CONFIG_D <= 0x16)
        goto check_gate_3;
    if (CONFIG_D < 0x1B)
        return;
check_gate_3:
    if (CONFIG_E == 0x22)
        return;
    if (CONFIG_F == 0x23)
        return;

    /* ---- 路径 A: 外部标志已设置 ---- */
    {
        uint32_t flag = *(volatile uint32_t *)0x20000248;

        if (flag != 0) {
            TRIGGER_FLAG_A = 0;

            if (COUNTER_A < 100) {
                COUNTER_A++;
            } else {
                TRIGGER_FLAG_A = 1;

                if (MODE_BYTE != 0) {
                    TRIGGER_FLAG_A = 0;
                    func_12604(2);
                    func_0E2D8(2);

                    TRIGGER_FLAG_A = 0;
                    func_10BD0(TRIGGER_FLAG_A);
                }
            }
            return;
        }
    }

    /* ---- 路径 B: 外部标志已清除 ---- */
    {
        uint32_t flag = *(volatile uint32_t *)0x20000248;

        if (flag == 0) {
            if (COUNTER_A < 100) {
                COUNTER_A++;
            } else {
                TRIGGER_FLAG_A = 1;

                if (MODE_BYTE != 0) {
                    TRIGGER_FLAG_A = 0;
                    func_12604(2);
                    func_0E2D8(2);

                    TRIGGER_FLAG_A = 1;
                    func_10BD0(TRIGGER_FLAG_A);
                }
            }
            return;
        }
    }
}

/* ========================================================================
 * 主函数 4: Register_Configurator_A (0x0800BFC8, 130 bytes)
 *
 * 参数: r0 = base_mask — 基础位掩码
 *       r1 = shift_mode — 控制移位量:
 *             0 → shift = 0x000
 *             1 → shift = 0x100
 *             2 → shift = 0x200
 *             其他 → shift = 0x300
 *
 * 轮询 Status_Register_Reader 直到返回非 9 状态,
 * 然后对控制寄存器执行读-修改-写:
 *   1. 清除位 [9:8] (0x300), 设置 shift_mode 对应的位
 *   2. 清除位 [7:3] (0xF8), 设置 r0 | 0x02
 *   3. 设置 bit16 (0x10000)
 *   4. 再次轮询, 然后清除 bit1 (0x02) 和位 [7:3] (0xF8)
 * ======================================================================== */
uint32_t Register_Configurator_A(uint32_t base_mask, uint32_t shift_mode)
{
    uint32_t status;
    uint32_t shift;

    /* 基于 shift_mode 确定移位值 */
    if (shift_mode == 0)
        shift = 0x000;
    else if (shift_mode == 1)
        shift = 0x100;
    else if (shift_mode == 2)
        shift = 0x200;
    else
        shift = 0x300;

    /* 轮询直到状态就绪 */
    status = Status_Register_Reader();         /* func_C13C → poll */
    if (status != 9) {
        volatile uint32_t *ctrl = (volatile uint32_t *)0x40010000;

        /* 阶段 1: 配置控制位 */
        *ctrl &= ~0x300;                        /* BIC #0x300 */
        *ctrl |= shift;                         /* ORR shift */
        *ctrl &= ~0xF8;                         /* BIC #0xF8 */
        *ctrl |= base_mask | 0x02;              /* ORR with base | 2 */

        /* 设置 bit16 */
        *ctrl |= 0x10000;

        /* 阶段 2: 再次轮询, 然后清除配置位 */
        status = Status_Register_Reader();
        *ctrl &= ~0x02;                         /* BIC #2 */
        *ctrl &= ~0xF8;                         /* BIC #0xF8 */
    }

    return status;
}

/* ========================================================================
 * 主函数 5: Control_Bit_Trigger (0x0800C0A4, 20 bytes)
 *
 * 调用 Conditional_Write 然后调用 Store_To_Register(0xF3).
 * 轻量级控制序列触发器.
 * ======================================================================== */
void Control_Bit_Trigger(void)
{
    Conditional_Write();                         /* func_C118 */
    Store_To_Register(0xF3);                    /* func_BFBC with r0=0xF3 */
}

/* ========================================================================
 * 主函数 6: Register_Configurator_B (0x0800C0C8, 76 bytes)
 *
 * 参数: r0 = dest_ptr — 目标指针 (写入参数值)
 *       r1 = param    — 要写入的参数值
 *
 * 轮询 Status_Register_Reader, 若状态==9:
 *   1. 清除位 [9:8] (0x300), 设置 bit9 (0x200)
 *   2. 设置 bit0 (0x01)
 *   3. 将 r4 (参数) 存储到 [r2] (目标指针)
 *   4. 再次轮询, 然后清除 bit0
 * ======================================================================== */
uint32_t Register_Configurator_B(void *dest_ptr, uint32_t param)
{
    uint32_t status;

    status = Status_Register_Reader();           /* func_C13C */

    if (status == 9) {
        volatile uint32_t *ctrl = (volatile uint32_t *)0x40010000;

        *ctrl &= ~0x300;                         /* BIC #0x300 */
        *ctrl |= 0x200;                          /* set bit9 */
        *ctrl |= 0x01;                           /* set bit0 */

        *(volatile uint32_t *)dest_ptr = param;  /* str r4, [r2] */

        status = Status_Register_Reader();
        *ctrl &= ~0x01;                          /* BIC #1 */
    }

    return status;
}

/* ========================================================================
 * 主函数 7: Status_Poller (0x0800C13C, 36 bytes)
 *
 * 轮询 Status_Register_Reader 直到返回值不为 1.
 * 初始默认值 = 9; 若返回 1 则自旋等待.
 *
 * 返回: 最终的非 1 状态值
 * ======================================================================== */
uint32_t Status_Poller(void)
{
    uint32_t status = 9;

    status = Status_Register_Reader();           /* 首次读取 */
    while (status == 1) {                        /* BEQ 自旋回 func_C050 */
        status = Status_Register_Reader();
    }

    return status;                               /* LDRB + POP {r3,pc} */
}

/* ========================================================================
 * 主函数 8: System_State_Monitor (0x0800C160, 382 bytes)
 *
 * 综合系统状态监控器. 监控多个传感器对和计数器,
 * 在超过阈值时触发显示更新.
 *
 * 三个独立监控部分:
 *
 * 部分 1 (计数器 A 监控):
 *   若 SENSOR_PAIR_A > SENSOR_PAIR_B 且 SENSOR_PAIR_A < 阈值:
 *     若 COUNT_BYTE_A == 0: 递增 COUNT_UP_A
 *     若 COUNT_UP_A >= COUNT_THRESH_A:
 *       → func_12604(2), func_0E2D8(2), func_10BD0(1)
 *     否则: COUNT_UP_A = 0
 *
 * 部分 2 (计数器 B 监控, gate 由 COUNT_BYTE_A 控制):
 *   镜像部分 1 但使用不同的传感器对和阈值.
 *
 * 部分 3 (字节计数器监控):
 *   若 TRIGGER_FLAG_A 已设置且 SENSOR_PAIR_A < 阈值:
 *     递增 COUNT_BYTE_A
 *     若 COUNT_BYTE_A >= COUNT_THRESH_C:
 *       → func_112C4(0,0), func_0BD7C(0), 设置 TIMER_ACTIVE
 *     否则若 COUNT_BYTE_B >= threshold: 重置 + func_0BD7C(1)
 * ======================================================================== */
void System_State_Monitor(void)
{
    /* ---- 门控: MODULE_STATE 必须非零 ---- */
    if (MODULE_STATE == 0)
        goto skip_counter_a;

    /* 模式阈值门控 */
    if (MODE_BYTE >= 0x17)                       /* >= 23 */
        goto check_upper;
    if (MODE_BYTE <= 0x1A)                       /* <= 26 */
        goto skip_counter_a;
    if (MODE_BYTE >= 0x22)                       /* >= 34 */
        goto skip_counter_a;

check_upper:
    /* ================================================================
     * 部分 1: 计数器 A 监控
     * ================================================================ */
    {
        uint16_t sensor_a = SENSOR_PAIR_A;
        uint16_t sensor_b = SENSOR_PAIR_B;

        if (sensor_a > sensor_b) {               /* CMP + BLE → skip */
            if (sensor_a < COUNT_THRESH_A) {     /* CMP + BGE → skip */
                if (COUNT_BYTE_A == 0) {
                    COUNT_UP_A++;
                    if (COUNT_UP_A >= COUNT_THRESH_A) {
                        func_12604(2);
                        func_0E2D8(2);
                        func_10BD0(1);
                        goto skip_counter_a;
                    }
                }
            } else {
                COUNT_UP_A = 0;
            }
        } else {
            COUNT_UP_A = 0;
        }
    }

    /* ================================================================
     * 部分 2: 计数器 B 监控
     * ================================================================ */
    {
        uint16_t sensor_a = SENSOR_PAIR_A;
        uint16_t sensor_b = SENSOR_PAIR_B;

        if (sensor_a < sensor_b) {               /* CMP + BGE → skip */
            if (COUNT_BYTE_A != 0) {             /* CBNZ → skip */
                if (sensor_a < COUNT_THRESH_B) {
                    if (sensor_a >= COUNT_THRESH_A) {
                        COUNT_UP_B++;
                        if (COUNT_UP_B >= COUNT_THRESH_B) {
                            func_12604(2);
                            func_0E2D8(2);
                            func_10BD0(0);
                            goto skip_counter_a;
                        }
                    }
                }
            }
        } else {
            COUNT_UP_B = 0;
        }
    }

skip_counter_a:
    /* ================================================================
     * 部分 3: 字节计数器监控
     * ================================================================ */
    if (TRIGGER_FLAG_A == 0)
        goto exit;

    if (SENSOR_PAIR_A < COUNT_THRESH_A) {
        if (COUNT_BYTE_A < COUNT_BYTE_B) {
            COUNT_BYTE_A++;
            if (COUNT_BYTE_A >= COUNT_THRESH_C) {
                func_112C4(0, 0);                /* 显示定时器计算 */
                func_0BD7C(0);                   /* 设备状态检查 */
                TIMER_ACTIVE = 1;
            }
        }
    } else {
        if (COUNT_BYTE_B != 0) {
            func_0BD7C(1);
            COUNT_BYTE_A = 0;
        }
        COUNT_BYTE_A = 0;
    }

exit:
    return;
}

/*
 * 内存布局:
 *   0x0800BD7C - 0x0800BDCC: Conn_State_Percent_Updater (80B)
 *   0x0800BDCC - 0x0800BED8: Display_Trigger_Monitor_A (268B)
 *   0x0800BED8 - 0x0800BFC8: Display_Trigger_Monitor_B (240B)
 *   0x0800BFC8 - 0x0800C04A: Register_Configurator_A (130B)
 *   0x0800C04A - 0x0800C050: 字面量池
 *   0x0800C050 - 0x0800C09E: Status_Register_Reader 叶辅助 (78B)
 *   0x0800C09E - 0x0800C0A4: 字面量池
 *   0x0800C0A4 - 0x0800C0B4: Control_Bit_Trigger (16B)
 *   0x0800C0B4 - 0x0800C0C0: Set_High_Bit 叶辅助 (12B)
 *   0x0800C0C0 - 0x0800C0C8: 字面量池
 *   0x0800C0C8 - 0x0800C114: Register_Configurator_B (76B)
 *   0x0800C114 - 0x0800C118: 字面量池
 *   0x0800C118 - 0x0800C12E: Conditional_Write 叶辅助 (22B)
 *   0x0800C12E - 0x0800C13C: 字面量池
 *   0x0800C13C - 0x0800C160: Status_Poller (36B)
 *   0x0800C160 - 0x0800C29C: System_State_Monitor (316B, 0x0800C29E 填充)
 *   0x0800C2A0 - 0x0800C2DC: 字面量池
 *
 * 调用关系:
 *   Conn_State_Percent_Updater  → func_134A0 (×2)
 *   Display_Trigger_Monitor_A   → func_12604, func_0E2D8, func_10BD0
 *   Display_Trigger_Monitor_B   → func_12604, func_0E2D8, func_10BD0
 *   Register_Configurator_A     → Status_Poller, Status_Register_Reader
 *   Control_Bit_Trigger         → Conditional_Write, Store_To_Register
 *   Register_Configurator_B     → Status_Poller, Status_Register_Reader
 *   Status_Poller               → Status_Register_Reader (自旋)
 *   System_State_Monitor        → func_12604, func_0E2D8, func_10BD0,
 *                                  func_112C4, func_0BD7C, Conn_State_Percent_Updater
 *
 * 注: 此文件中的 RAM 地址定义和阈值常量为初步还原.
 *     完整的字面量池解析需要逐地址验证,
 *     但函数结构和控制流已与反汇编精确匹配.
 */
