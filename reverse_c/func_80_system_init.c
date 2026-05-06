/**
 * @file func_80_system_init.c
 * @brief 函数: 系统初始化 — FPU 使能 + RCC 配置 + PLL 初始化 + VTOR 设置
 * @addr  0x080128B8 - 0x08012924 (108 bytes, 含字面量池)
 *         字面量池 0x08012910-0x08012923 (20 bytes, 5 个常量)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 标准 STM32F4 启动初始化序列:
 *   1. 使能 FPU (CPACR |= 0x00F00000, 完全访问 CP10/CP11)
 *   2. 设置 RCC_CR bit 0 (HSI 使能)
 *   3. 清除某 RCC 寄存器偏移 +8
 *   4. 清除 RCC 配置特定位 (掩码 0xFEF6FFFF)
 *   5. 写 PLL 或其他配置值 0x24003010
 *   6. 清除 RCC 配置 (偏移 +0x0C)
 *   7. 调用 PLL_Clock_Init (func_74)
 *   8. 设置 VTOR = 0x08008000 (APP_BASE, 向量表偏移)
 *
 * 参数: (无)
 *
 * 调用:
 *   func_1265C() @ 0x0801265C — PLL 时钟初始化 (func_74)
 */

#include "stm32f4xx_hal.h"

#define SCB_CPACR   (*(volatile uint32_t *)0xE000ED88)
#define SCB_VTOR    (*(volatile uint32_t *)0xE000ED08)
#define RCC_CR      (*(volatile uint32_t *)0x40023800)

extern void PLL_Clock_Init(void);  /* func_74 @ 0x0801265C */

void System_Init(void)
{
    /* 1. 使能 FPU (CP10/CP11 完全访问) */
    SCB_CPACR |= 0x00F00000;

    /* 2. 使能 HSI (RCC_CR bit 0) */
    RCC_CR |= 0x1;

    /* 3-5. RCC 配置 (清除/设置 CFGR/PLLCFGR) */
    {
        volatile uint32_t *RCC_CFGR  = (volatile uint32_t *)0x40023808;
        volatile uint32_t *RCC_CReg  = (volatile uint32_t *)0x40023800;

        *RCC_CFGR = 0;                        /* 清除 CFGR */

        RCC_CReg[0] &= 0xFEF6FFFF;            /* 清除特定位 */

        /* 写 PLL 配置 */
        *((volatile uint32_t *)0x40023804) = 0x24003010;

        /* 清除 RCC_CR bit 18 (HSEBYP) */
        RCC_CReg[0] &= ~0x00040000;

        /* 清除 RCC 偏移 +0x0C 的寄存器 */
        *((volatile uint32_t *)0x4002380C) = 0;
    }

    /* 7. PLL 时钟初始化 */
    PLL_Clock_Init();

    /* 8. 设置向量表偏移到 APP_BASE (0x08008000) */
    SCB_VTOR = 0x08008000;
}
