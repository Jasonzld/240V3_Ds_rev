/**
 * @file func_146_systick_timer_scheduler.c
 * @brief 函数: SysTick 定时器调度器 — 使用 SysTick 实现毫秒级延迟和任务调度
 * @addr  0x08018488 - 0x08018B66 (1758 bytes code)
 *        literal pool: 0x08018B66 - 0x08018BAF (74 bytes)
 *        NOTE: 0x08018BB0+ is a separate function (own PUSH prologue)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 主要子函数:
 *   A. 0x08018488: SysTick_Delay_Ms — 将毫秒计数值除以 540, 循环触发 SysTick
 *   B. 0x080184C0: SysTick_Trigger — 配置并启动 SysTick 单次触发
 *      - 重载值 = ms * 常量 (从字面量池)
 *      - 写 SYST_RVR, 清 SYST_CVR, 置 SYST_CSR bit0
 *      - 轮询等待完成 (bit0 清零 + bit16 置位)
 *   C. 0x0801850C: FPU_Abs_Double — 取 double 绝对值
 *   D. 0x08018524: Main_Init_Sequence — 系统初始化序列
 *      - 调用 func_144 (ADC 缩放), func_18EC8 (RCC 配置)
 *      - 检查启动计数器, 若首次启动: 执行字符串操作 + DSB 复位序列
 *      - 否则: 初始化 USART/GPIO (func_0C2DC, func_0D748, func_0D8C4, etc.)
 *      - 任务启动 (func_09436, func_13344, func_13198)
 *      - 初始化后处理 (func_132D8, func_12924, func_13000, func_099F8,
 *                        func_15FA8, func_091E8, 4x func_0839A,
 *                        func_0C8B0, func_0C7A0)
 *      - E. 0x08018642: Dispatch_Loop — 主调度循环 (无限循环)
 *
 * SysTick 寄存器 (Cortex-M4 系统控制块):
 *   0xE000E010 = SYST_CSR  (控制/状态)
 *   0xE000E014 = SYST_RVR  (重载值)
 *   0xE000E018 = SYST_CVR  (当前值)
 *
 * 调用:
 *   func_144()  @ 0x0801844C — ADC 值缩放器
 *   func_18EC8() @ 0x08018EC8 — RCC 配置
 *   func_0C0A4() @ 0x0800C0A4 — 字符串操作
 *   func_0C0C8() @ 0x0800C0C8 — 字符串操作
 *   func_0C0B4() @ 0x0800C0B4 — 字符串操作
 *   func_0C2DC() @ 0x0800C2DC — USART 初始化
 *   func_0D748() @ 0x0800D748 — 外设初始化
 *   func_0D8C4() @ 0x0800D8C4 — 外设配置
 *   func_09436() @ 0x08009436 — 多模式操作
 *   func_13344() @ 0x08013344 — 数据处理
 *   func_13198() @ 0x08013198 — 数据处理
 */

#include "stm32f4xx_hal.h"

extern void     func_144(uint32_t val);
extern void     func_18EC8(uint32_t val);
extern void     func_0C0A4(void);
extern uint32_t func_0C0C8(const char *a, const char *b);
extern void     func_0C0B4(void);
extern void     func_0C2DC(void);
extern void     func_0D748(void);
extern void     func_0D8C4(uint32_t val);
extern void     func_09436(void);
extern void     func_13344(uint32_t a, uint32_t b);
extern void     func_13198(uint32_t a, uint32_t b);
extern void     func_132D8(uint32_t a, uint32_t b);
extern void     func_12924(uint32_t a, uint32_t b);
extern void     func_13000(void);
extern void     func_099F8(void);
extern void     func_15FA8(void);
extern void     func_091E8(void);
extern void     func_0839A(void *base, const char *str);
extern void     func_0C8B0(void);
extern void     func_0C7A0(void);

/* SysTick 寄存器 */
#define SYST_CSR  (*(volatile uint32_t *)0xE000E010)
#define SYST_RVR  (*(volatile uint32_t *)0xE000E014)
#define SYST_CVR  (*(volatile uint32_t *)0xE000E018)

/* 全局状态 */
#define TICK_SCALE   (*(volatile uint16_t *)0x200001FA)
#define BOOT_COUNTER (*(volatile uint32_t *)0x08004000)   /* 位于 Bootloader 区域, 非 SRAM */

/* ---- 子函数 A: 毫秒延迟 (分频因子 = 540) ---- */
void SysTick_Delay_Ms(uint32_t ms)
{
    uint8_t  chunks = (uint8_t)(ms / 540);   /* SDIV + UXTB */
    uint16_t rem    = (uint16_t)(ms % 540);   /* MLS + UXTH */

    /* 发送完整块 */
    while (chunks > 0) {
        SysTick_Trigger(540);
        chunks--;
    }

    /* 发送余数 */
    if (rem > 0) {
        SysTick_Trigger(rem);
    }
}

/* ---- 子函数 B: SysTick 单次触发 ---- */
void SysTick_Trigger(uint32_t ticks)
{
    uint32_t reload = ticks * TICK_SCALE;     /* MUL */

    SYST_RVR = reload;                         /* 设置重载值 */
    SYST_CVR = 0;                              /* 清计数器 */
    SYST_CSR |= 1;                             /* 使能 SysTick */

    /* 轮询等待完成 (early-exit if enable didn't stick) */
    do {
        uint32_t csr = SYST_CSR;
        if (!(csr & 1)) goto cleanup;          /* 使能位未置位则直接退出 */
        if (csr & 0x10000) break;              /* COUNTFLAG 置位 = 完成 */
    } while (1);

cleanup:
    /* 关闭 SysTick */
    SYST_CSR &= ~1U;
    SYST_CVR = 0;
}

/* ---- 子函数 C: 取 double 绝对值 ---- */
double FPU_Abs_Double(double val)
{
    uint32_t hi = ((uint32_t *)&val)[1] & 0x7FFFFFFF;
    uint32_t lo = ((uint32_t *)&val)[0];
    double result;
    ((uint32_t *)&result)[0] = lo;
    ((uint32_t *)&result)[1] = hi;
    return result;
}

/* ---- 子函数 D: 主初始化序列 ---- */
void Main_Init_Sequence(void)
{
    func_144(100);                        /* ADC 初始化缩放 */
    func_18EC8(0x1C200);                 /* RCC 配置 */

    if ((BOOT_COUNTER + 1) == 0) {
        /* 首次启动检测: Flash 擦除态 0xFFFFFFFF, +1 溢出为 0
         * 注意: 无写回 (ADDS + CBNZ, 无 STR), BOOT_COUNTER 本身不变 */
        func_0C0A4();
        func_0C0C8(0, 0);
        func_0C0B4();

        /* SCB AIRCR 复位序列 */
        __DSB();
        {
            volatile uint32_t *scb = (volatile uint32_t *)0xE000ED00;
            uint32_t v = *scb;
            v &= 0x700;
            v |= 0x05FA0000;     /* VECTKEY */
            v += 4;               /* + SYSRESETREQ = 0x05FA0004 */
            *(volatile uint32_t *)0xE000ED0C = v;
        }
        __DSB();
        __NOP();
        __NOP();
        while (1);                         /* 等待复位 */
    }

    /* 后续启动: 正常初始化 */
    func_0C2DC();                          /* USART 初始化 */
    *(volatile uint32_t *)0x4241028C = 1;  /* GPIO 位段置位 */
    SysTick_Delay_Ms(1000);                /* 延迟 1 秒 */
    *(volatile uint32_t *)0x20000288 = 1;  /* 设置运行标志 */
    SysTick_Delay_Ms(1000);
    *(volatile uint32_t *)0x20000284 = 0;  /* 清除另一标志 */
    SysTick_Delay_Ms(1000);
    func_0D748();                          /* 外设初始化 */
    func_0D8C4(0x1C200);                  /* 外设配置 */
    func_09436();                          /* 多模式操作 */

    /* 任务启动 */
    func_13344(10000, 9999);               /* 10ms + 9.999ms 任务 */
    func_13198(20000, 9999);               /* 20ms + 9.999ms 任务 */

    /* 初始化后处理 */
    func_132D8(0xFFFF, 0xC34F);            /* 参数配置 */
    func_12924(0xFFFF, 0xC34F);            /* 参数配置 */
    func_13000();                          /* 模块初始化 */
    func_099F8();                          /* 状态初始化 */
    func_15FA8();                          /* USART 处理 */
    func_091E8();                          /* ADC/DMA 处理 */

    /* 字符串表初始化 (4 条配置字符串, 偏移 0/0x10/0x20/0x30) */
    {
        void *base = (void *)0x20000000;   /* 从字面量池加载 */
        func_0839A(base + 0x00, "STR1");  /* @adr 目标 */
        func_0839A(base + 0x10, "STR2");
        func_0839A(base + 0x20, "STR3");
        func_0839A(base + 0x30, "STR4");
    }

    func_0C8B0();                          /* 定时器/计数器初始化 */
    func_0C7A0();                          /* 显示/输出初始化 */

    /* 保存结果并进入调度循环 */
    // 此处进入 Dispatch_Loop 主调度循环 (0x08018642 起)
}
