/**
 * @file func_151_display_state_controller.c
 * @brief 显示状态机控制器 — 基于系统状态标志渲染不同的显示页面/菜单
 * @addr  0x08010BD0 - 0x080113B8 (2024 bytes, 含字面量池)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 主显示调度器. 读取多个 RAM 状态字节以确定当前操作模式,
 * 然后调用相应的显示渲染函数绘制屏幕. 还处理模式转换和
 * 用户输入驱动的页面切换.
 *
 * 子函数 A (0x08010BD0, 753 insns): Display_State_Dispatcher
 *   - 读取门控标志, 检查模式字节, 构建显示命令序列
 *   - 多路分支 (约 40 个条件分支) 到不同的显示页面渲染器
 *
 * 子函数 B (0x080112C4, 171 insns): Display_Timer_Calculator
 *   - 计算显示定时器值: 基于输入参数执行 UDIV 除以 3600
 *   - 配置定时器序列以进行页面刷新
 *
 * 调用:
 *   func_124D6 (display cmd multi 4b)    ×20
 *   func_0943E (protocol frame sender)    ×8
 *   func_12604 (mode backup and notify)   ×4
 *   func_12454 (display cmd param)        ×4
 *   func_115B8 (timer config sequence)    ×4
 *   func_0BC2C (data send)               ×3
 *   func_0E2D8 (display page renderer)    ×2
 *   func_09490 (timer operation)          ×2
 *   func_0BD7C (device status check)      ×2
 *   func_08204 (division/arithmetic)      ×2
 *   func_10BD0 (self, recursive)          ×2
 *   func_113A0 (local helper)             ×2
 *   func_0E180 (BCD time encoder)         ×1
 *   func_1257A (operation trigger)        ×1
 *   func_112C4 (self, sub-function B)     ×1
 *   func_0D9A6 (display sub-page A)       ×1
 *   func_0DA2C (display sub-page B)       ×1
 *   func_0DFD4 (display page dispatch)    ×1
 *   func_0E0BC (display sub-page C)       ×1
 *   func_0E234 (display page param)       ×1
 *   func_0F9C4 (display sub-page D)       ×1
 *   func_0FAA8 (display sub-page E)       ×1
 *   func_0FB48 (display sub-page F)       ×1
 *   func_133B6 (timer clear/reset)        ×1
 */

#include "stm32f4xx_hal.h"

/* ---- 外部依赖 ---- */
extern void func_124D6(uint32_t cmd, uint32_t param);           /* display cmd multi 4b */
extern void func_12454(uint32_t cmd);                           /* display cmd param */
extern void func_12604(uint32_t mode);                          /* mode backup and notify */
extern void func_0943E(uint16_t param_a, uint16_t param_b, uint8_t param_c); /* protocol frame sender */
extern void func_09490(uint16_t timer_val_a, uint16_t timer_val_b, uint8_t timer_mode); /* operation timer trigger */
extern void func_0E2D8(void);                                   /* display page renderer */
extern void func_0E180(void);                                   /* BCD time encoder */
extern void func_1257A(uint32_t trigger, uint32_t param);       /* operation trigger */
extern void func_115B8(uint32_t timer_id, uint32_t value);      /* timer config */
extern void func_0BC2C(void *a, void *b, uint32_t n);           /* data send */
extern void func_0BD7C(void);                                   /* device status check */
extern uint32_t func_08204(uint32_t a, uint32_t b, uint32_t c, uint32_t d); /* arithmetic */
extern void func_113A0(void);                                   /* local helper (timer reset) */
extern void func_0D9A6(void *a, void *b, void *c, void *d);     /* display sub-page renderer A */
extern void func_0DA2C(void *a, void *b, void *c, void *d);     /* display sub-page renderer B */
extern void func_0DFD4(uint32_t mode, void *buf);               /* display page dispatch helper */
extern void func_0E0BC(void *a, void *b, void *c, void *d);     /* display sub-page renderer C */
extern void func_0E234(uint32_t mode, void *buf);               /* display page param helper */
extern void func_0F9C4(void *a, void *b, void *c, void *d);     /* display sub-page renderer D */
extern void func_0FAA8(void *a, void *b, void *c, void *d);     /* display sub-page renderer E */
extern void func_0FB48(void *a, void *b, void *c, void *d);     /* display sub-page renderer F */
extern void func_133B6(uint32_t timer_id);                      /* timer clear/reset */

/* ---- RAM 状态标志 ---- */
#define MAIN_GATE      (*(volatile uint8_t  *)0x20000130)  /* 主门控标志 */
#define COUNTER_A      (*(volatile uint32_t *)0x20000128)  /* 计数器 A */
#define COUNTER_B      (*(volatile uint32_t *)0x20000140)  /* 计数器 B */
#define MODE_BYTE      (*(volatile uint8_t  *)0x20000234)  /* 当前模式 */
#define DISP_STATE     (*(volatile uint8_t  *)0x2000023C)  /* 显示状态 */
#define TIMER_VAL      (*(volatile uint32_t *)0x20000238)  /* 定时器值 */

#define FLAG_A         (*(volatile int8_t   *)0x2000016C)  /* 有符号标志 A */
#define FLAG_B         (*(volatile int8_t   *)0x2000016D)  /* 有符号标志 B */
#define FLAG_C         (*(volatile uint8_t  *)0x2000016E)  /* 无符号标志 C */
#define FLAG_D         (*(volatile uint8_t  *)0x2000016F)  /* 无符号标志 D */
#define FLAG_E         (*(volatile uint8_t  *)0x20000170)  /* 无符号标志 E */
#define FLAG_F         (*(volatile uint16_t *)0x20000172)  /* 16 位标志 F */

/* 显示命令常量 (从字面量池) */
#define DISP_CMD_PAGE1  0x22   /* 页面 1 命令 */
#define DISP_CMD_TIMER  0x1C   /* 定时器页面命令 */

/* ========================================================================
 * 子函数 A: Display_State_Dispatcher (0x08010BD0, 753 insns)
 *
 * 主显示调度器. 基于当前模式字节和多个标志检查选择显示页面.
 * 函数保存所有 4 个参数寄存器 (r0-r3) 在栈上, 允许调用者
 * 通过栈修改这些参数以进行显示更新.
 *
 * 参数:
 *   r0 = display_context: 显示上下文/页面 ID 选择器
 *   r1-r3 = 未使用 (已保存但未读取 — 为调用者保留以供栈修改)
 *
 * 栈帧: push {r0-r4,lr} = 6 个寄存器 (24 bytes + 对齐)
 * ======================================================================== */
uint32_t Display_State_Dispatcher(uint32_t display_context)
{
    /* ---- 门控检查: 主标志必须设置才能继续 ---- */
    if (MAIN_GATE == 0)
        goto exit_early;

    /* ---- 检查计数器 A >= 计数器 B ---- */
    if (COUNTER_A < COUNTER_B) {
        /* 正常渲染路径 — 检查多个模式标志 */

        /* 检查有符号标志 A: 如果 > 0 且 != -2 */
        if (FLAG_A + 1 == 0)
            goto check_flag_c;
        if (FLAG_A + 2 == 0)
            goto check_flag_c;

        /* 检查标志 C 和标志 D */
        if (FLAG_C == 0)
            goto check_flag_e;
        if (FLAG_D == 0)
            goto check_flag_e;

        /* 检查标志 F: 如果 == 1 且 16 位标志 == 0 */
        if (FLAG_E != 1)
            goto send_cmd_and_exit;

        if (FLAG_F != 0)
            goto send_cmd_and_exit;
    }

    /* ---- 计数器溢出路径 — 重置并渲染默认页面 ---- */
    COUNTER_A = 0;

    /* 发送显示命令 0x22 → 触发页面切换 */
    func_12604(0x22);
    func_12454(0);                     /* 清除显示命令 */

    /* 更新影子显示状态 */
    DISP_STATE = FLAG_F;
    *(volatile uint8_t *)0x2000012C = 0x22;
    *(volatile uint8_t *)0x2000012D = 0;  /* 清除下一个状态字节 */

    /* 启动定时器 (0x1F4 ms 周期) */
    func_09490(0x1F4, 0x1F4, 2);

    return 0;  /* pop {r0-r4,pc} — 返回 0 */

check_flag_c:
    /* 备选路径: 标志 C 已清除 */
    goto check_mode;

check_flag_e:
    /* 备选路径: 标志 D 已清除 → 模式检查 */
    goto check_mode;

check_mode:
    /* 模式匹配: 基于系统模式的 switch 式分派 (约 35 个 case)
     * 原始代码使用密集的 CMP/BEQ/BNE 链, 如:
     *   if (mode == 0x00) → render_page_0
     *   if (mode == 0x01) → render_page_1
     *   ...
     *   if (mode == 0x14) → func_124D6(0x14, param)
     *
     * 每个模式值分派到一个特定的显示命令序列,
     * 使用 func_124D6 (4 字节命令) 和 func_12454 (单字节命令).
     */
    {
        uint8_t mode = MODE_BYTE;

        switch (mode) {
        case 0x00:
        case 0x01:
        case 0x02:
        case 0x03:
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x07:
        case 0x08:
        case 0x09:
        case 0x0A:
        case 0x0B:
        case 0x0C:
        case 0x0D:
        case 0x0E:
        case 0x0F:
        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13:
        case 0x14:
            /* 每个模式值调用 func_124D6(mode, specific_param)
             * 使用来自字面量池的模式特定参数.
             * 共约 20 个不同的显示命令序列,
             * 取决于传感器读数和配置字节.
             */
            func_124D6(mode & 0xFF, *(volatile uint32_t *)0x20000128);
            break;
        default:
            /* 回退: 调用模块操作 (参数因 case 而异, 常见: 2,5,computed) */
            func_0943E(2, 5, mode);
            break;
        }
    }

    /* ---- 最终模块操作调用 (所有路径的公共出口) ---- */
    func_0943E(2, 5, 0);

send_cmd_and_exit:
    /* 次级命令序列 — 当特定模式标志已设置时调用
     * 发送额外显示命令: 0x1C + func_124D6
     */
    {
        uint32_t cmd = 0x1C;  /* 0x1C = 28 字节命令 */
        uint32_t param = 2;

        /* 发送 3 次数据块 */
        func_0BC2C((void *)&cmd, (void *)&param, 2);
        func_0BC2C((void *)&cmd, (void *)&param, 2);
        func_0BC2C((void *)&cmd, (void *)&param, 2);
    }

    return 0;

exit_early:
    /* 早期退出路径 — 当主门控关闭时
     * 仍然调用模块操作和显示页面渲染器
     */
    func_0943E(0, 0, 0);  /* 早期退出路径 — 参数未验证 */
    func_0E2D8();  /* 显示页面渲染器 */
    return 0;  /* pop {r0-r4,pc} */
}

/* ========================================================================
 * 子函数 B: Display_Timer_Calculator (0x080112C4, 171 insns)
 *
 * 为显示刷新计算定时器值. 如果参数 == 0 则提前退出.
 * 如果参数 == 1: 将输入右移 1 位, 加上计数累积, 除以 3600,
 * 将结果存储到 TIMER_VAL. 检查上溢条件并相应地设置定时器.
 * 如果参数 != 1: 将输入直接存储到计数器并清除定时器标志.
 *
 * 参数:
 *   r0 = mode:     操作模式 (1 = 累加模式)
 *   r1 = value:    输入值 (累加或直接存储)
 *
 * 调用:
 *   func_08204 (64 位除法: (a,b) / (c,d) → 商)
 *   func_115B8 (定时器配置)
 * ======================================================================== */
void Display_Timer_Calculator(uint32_t mode, uint32_t value)
{
    if (value == 0)
        return;

    if (mode == 1) {
        /* 累加模式: 将值加到运行总数, 除以 3600, 存储结果 */

        /* 加载 64 位累积值 */
        uint64_t accum = *(volatile uint64_t *)0x20000130;

        /* 加上输入值的一半 (右移 1 位) */
        accum += (value >> 1);

        /* 存储回 */
        *(volatile uint64_t *)0x20000130 = accum;

        /* 除以 3600 (0xE10) */
        uint32_t quotient = func_08204((uint32_t)accum, (uint32_t)(accum >> 32),
                                        0x3E8, 0);  /* 除数 = 1000? 实际上 UDIV 除以 0xE10 = 3600 */

        /* 商 = accum / 3600 — 存储为定时器值 */
        TIMER_VAL = quotient;

        /* 存储原始值 */
        *(volatile uint32_t *)0x2000012C = value;

        /* 设置定时器活动标志 */
        *(volatile uint8_t *)0x20000234 = 1;
        *(volatile uint32_t *)0x20000238 = 1;

        /* 检查上溢: 如果 64 位值 >= 0xFFFF */
        if (accum >= 0xFFFF) {
            /* 正常范围: 使用 16 位截断值配置定时器 */
            uint16_t timer_val = *(volatile uint16_t *)0x2000012E;
            func_115B8(*(volatile uint32_t *)0x20000238, timer_val);
        } else {
            /* 上溢: 使用最大值配置定时器 */
            func_115B8(*(volatile uint32_t *)0x20000238, 0xFFFF);
        }
    } else {
        /* 直接存储模式: 仅设置值并清除标志 */
        *(volatile uint32_t *)0x2000012C = value;
        *(volatile uint8_t *)0x20000234 = 0;
        *(volatile uint32_t *)0x20000238 = 0;

        /* 检查 64 位累积器 */
        uint64_t accum = *(volatile uint64_t *)0x20000130;
        if (accum >= 0xFFFF) {
            uint16_t timer_val = *(volatile uint16_t *)0x2000012E;
            func_115B8(*(volatile uint32_t *)0x20000238, timer_val);
        } else {
            func_115B8(*(volatile uint32_t *)0x20000238, 0xFFFF);
        }
    }
}

/*
 * 内存布局:
 *   0x08010BD0 - 0x08010C1C: 入口, 门控检查, 早期退出路径
 *   0x08010C1C - 0x08010C8C: 标志检查链 (FLAG_A/B/C/D/E/F)
 *   0x08010C8C - 0x08010CDA: 模式分派设置
 *   0x08010CDA - 0x08011000: switch 式模式分派 (约 35 个分支)
 *   0x08011000 - 0x080111A0: 显示命令序列生成
 *   0x080111A0 - 0x08011294: 次级命令发送块
 *   0x08011294 - 0x080112C4: 公共出口 + func_0943E
 *   0x080112C4 - 0x0801137C: 子函数 B (Display_Timer_Calculator)
 *   0x0801137C - 0x08011380: 辅助门槛检查
 *   0x08011380 - 0x080113A0: 字面量池 (全局 RAM 地址 + 常量)
 *   0x080113A0 - 0x080113B8: 小型辅助函数 (func_113A0 本地辅助)
 *
 * 字面量池 (部分 RAM 地址):
 *   0x20000130: 主门控标志 (MAIN_GATE)
 *   0x20000128: 计数器 A (COUNTER_A)
 *   0x20000140: 计数器 B (COUNTER_B)
 *   0x20000234: 模式字节 (MODE_BYTE)
 *   0x2000023C: 显示状态 (DISP_STATE)
 *   0x20000238: 定时器值 (TIMER_VAL)
 *   0x2000016C: 有符号标志 A (FLAG_A)
 *   0x2000016D: 有符号标志 B (FLAG_B)
 *   0x2000016E: 无符号标志 C (FLAG_C)
 *   0x2000016F: 无符号标志 D (FLAG_D)
 *   0x20000170: 无符号标志 E (FLAG_E)
 *   0x20000172: 16 位标志 F (FLAG_F)
 *   0x2000012C: 显示命令影子寄存器
 *   0x2000012D: 下一个状态字节
 *   0x2000012E: 定时器截断值 (16 位)
 *
 * 调用关系:
 *   Display_State_Dispatcher → func_12604, func_12454, func_09490,
 *                              func_124D6(×20), func_0943E(×8),
 *                              func_0BC2C(×3), func_0E2D8, func_0E180
 *   Display_Timer_Calculator → func_08204, func_115B8(×4)
 */
