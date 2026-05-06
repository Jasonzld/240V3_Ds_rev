/**
 * @file func_155_dma_usart_init.c
 * @brief DMA + USART6 初始化序列 — 配置 DMA 流和 USART 外设进行串行通信
 * @addr  0x08011770 - 0x080118EC (380 bytes, 1 个函数)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 初始化 USART6/DMA1 子系统以进行高速串行数据通信.
 * 配置 USART 参数、DMA 流、GPIO 复用功能和中端.
 *
 * 调用:
 *   func_113B8 (RCC AHB1 时钟使能)   ×2
 *   func_113F8 (RCC APB2 时钟使能)   ×1
 *   func_11418 (GPIO AF 配置)         ×2
 *   func_0C42C (USART 帧格式设置)     ×1
 *   func_0C4BC (USART 参数配置)       ×3
 *   func_0978C (DMA 流反初始化)       ×2
 *   func_09984 (DMA 流配置)           ×2
 *   func_09948 (DMA 中断配置)         ×2
 *   func_1197E (USART DMA 关联)       ×1
 *   func_11950 (USART 中断配置)       ×2
 *   func_09774 (DMA 流使能)           ×2
 *   func_11938 (USART 使能)           ×1
 *   func_118EC (后续初始化步骤)       ×1
 */

#include "stm32f4xx_hal.h"

/* ---- 外部依赖 ---- */
extern void func_113B8(uint32_t periph);                /* RCC AHB1ENR 使能 */
extern void func_113F8(uint32_t periph);                /* RCC APB2ENR 使能 */
extern void func_11418(uint32_t gpio, uint32_t af);     /* GPIO AF 配置 */
extern void func_0C42C(uint32_t base, void *config);    /* USART 帧格式设置 */
extern void func_0C4BC(uint32_t base, uint32_t param1, uint32_t param2); /* USART 参数 */
extern void func_0978C(uint32_t stream_base);           /* DMA 流反初始化 */
extern void func_09984(uint32_t stream_base, void *config); /* DMA 流配置 */
extern void func_09948(uint32_t stream_base, uint32_t param1, uint32_t param2); /* DMA IT */
extern void func_1197E(uint32_t base, void *config);    /* USART DMA 关联 */
extern void func_11950(uint32_t base, uint32_t param1, uint32_t param2); /* USART IT */
extern void func_09774(uint32_t stream_base, uint32_t enable); /* DMA 流使能 */
extern void func_11938(uint32_t base, uint32_t enable); /* USART 使能 */
extern void func_118EC(uint8_t flags);                  /* 后续初始化步骤 */

/* ---- 外设寄存器基址 (由字面量池 0x080118D8-0x080118E8 验证) ---- */
#define DMA_BASE       0x40020000  /* DMA 控制器基址 (pool 0x080118D8, LDR ref: 0x080117AA) */
#define USART_BASE     0x40013000  /* USART 外设基址 (pool 0x080118DC, LDR ref: 0x08011818) */
#define DMA_STREAM_B   0x40026458  /* DMA 流 B 基址 (pool 0x080118E0, LDR ref: 0x0801181E) */
#define DMA_BUF_A      0x20002FE0  /* DMA 缓冲区 A (pool 0x080118E4, LDR ref: 0x08011864) */
#define DMA_BUF_B      0x20002C20  /* DMA 缓冲区 B (pool 0x080118E8, LDR ref: 0x0801187A) */

/* ========================================================================
 * DMA_USART_Init (0x08011770 - 0x080118D4, 356 bytes)
 *
 * 栈帧: sub sp, #0x5C (92 bytes)
 *   单次进入, POP {PC} 退出 (无寄存器保存 → push {lr} only)
 *
 * 步骤:
 *   1. RCC 时钟使能: AHB1(0x00001), APB2(0x1000), AHB1(0x400000)
 *   2. USART 帧格式配置: {0xE0, 2, 0, 3, 1} → func_0C42C
 *   3. USART 参数: func_0C4BC ×3 (BRR/invert/swap 选项)
 *   4. GPIO AF 配置: func_11418 ×2
 *   5. DMA 流反初始化: func_0978C ×2
 *   6. DMA 流配置: func_09984 ×2
 *   7. USART+DMA 中断使能: func_09948 ×2, func_11950 ×2
 *   8. DMA 流使能: func_09774 ×2
 *   9. USART 使能: func_11938
 *  10. 调用 func_118EC(0xFF) 继续初始化序列
 * ======================================================================== */
void DMA_USART_Init(void)
{
    uint8_t  usart_cfg[5];     /* sp+0x54: USART 帧格式配置 */
    uint16_t dma_cfg[9];       /* sp+0x40: DMA 流配置 (9 halfwords) */
    uint32_t dma_stream_cfg[8]; /* sp+0x04: DMA 流详细配置 (8 words) */

    /* ---- 步骤 1: RCC 时钟使能 ---- */
    func_113B8(0x00001);       /* AHB1: 使能 DMA1 时钟 (bit 0) */
    func_113F8(0x01000);       /* APB2: 使能 USART6 时钟 (bit 12) */
    func_113B8(0x400000);      /* AHB1: 使能 DMA2 时钟 (bit 22) */

    /* ---- 步骤 2: USART 帧格式设置 ---- */
    usart_cfg[0] = 0xE0;       /* sp+0x54: 字长 (8 位) + 奇偶校验配置 */
    usart_cfg[1] = 2;          /* sp+0x58: 停止位 (2 停止位) */
    usart_cfg[2] = 0;          /* sp+0x5A: 奇偶校验禁用 */
    usart_cfg[3] = 3;          /* sp+0x59: 硬件流控制 (CTS/RTS) */
    usart_cfg[4] = 1;          /* sp+0x5B: 模式 (TX/RX 使能) */
    func_0C42C(DMA_BASE, usart_cfg);  /* 应用 USART 配置 */

    /* ---- 步骤 3: USART 参数配置 (波特率/反相/交换) ---- */
    func_0C4BC(DMA_BASE, 5, 5);   /* BRR 配置, 参数对 #1 */
    func_0C4BC(DMA_BASE, 6, 5);   /* BRR 配置, 参数对 #2 */
    func_0C4BC(DMA_BASE, 7, 5);   /* BRR 配置, 参数对 #3 */

    /* ---- 步骤 4: GPIO 复用功能配置 (USART TX/RX 引脚) ---- */
    func_11418(0x1000, 1);     /* GPIO AF 配置: 引脚组 1 */
    func_11418(0x1000, 0);     /* GPIO AF 配置: 引脚组 0 */

    /* ---- 步骤 5: DMA 流反初始化 ---- */
    func_0978C(DMA_STREAM_B);                           /* 反初始化 DMA 流 B */
    func_0978C(DMA_STREAM_B - 0x48);                    /* 反初始化 DMA 流 A (基址 - 0x48) */

    /* ---- 步骤 5.5: 设置 DMA 流配置结构 (sp+0x40) ---- */
    dma_cfg[0] = 0;              /* sp+0x40: 通道选择 (0 = 通道 0) */
    dma_cfg[1] = 0x104;          /* sp+0x42: 方向 (外设→内存) + 外设大小 */
    dma_cfg[2] = 0;              /* sp+0x44: 内存增量模式 */
    dma_cfg[3] = 2;              /* sp+0x46: 外设增量 (固定) */
    dma_cfg[4] = 1;              /* sp+0x48: 模式 (正常模式) */
    dma_cfg[5] = 0x100;          /* sp+0x4A: 优先级 (高) */
    dma_cfg[6] = 0;              /* sp+0x4C: FIFO 模式 (直接) */
    dma_cfg[7] = 0;              /* sp+0x4E: FIFO 阈值 */
    dma_cfg[8] = 7;              /* sp+0x50: 数据项数 (7) */

    /* ---- 步骤 6: USART DMA 关联 ---- */
    func_1197E(USART_BASE, dma_cfg);

    /* ---- 步骤 7: DMA 流配置 ---- */
    /* 流 B 配置 */
    dma_stream_cfg[0] = 0x3C0;       /* sp+0x14: 控制寄存器值 */
    dma_stream_cfg[1] = 0;           /* sp+0x30 */
    dma_stream_cfg[2] = 0;           /* sp+0x34 */
    dma_stream_cfg[3] = 0;           /* sp+0x38 */
    dma_stream_cfg[4] = 0;           /* sp+0x24 */
    dma_stream_cfg[5] = 0x400;       /* sp+0x1C */
    dma_stream_cfg[6] = 0;           /* sp+0x28 */
    dma_stream_cfg[7] = DMA_BUF_A;   /* sp+0x08: 外设地址 (缓冲区 A) */
    dma_stream_cfg[8] = DMA_BUF_B;   /* sp+0x0C: 内存地址 (缓冲区 B) */
    func_09984(DMA_STREAM_B, dma_stream_cfg);

    /* 流 A 配置 (基址 - 0x48) */
    dma_stream_cfg[5] = 0;           /* 覆盖: 优先级 = 低 */
    dma_stream_cfg[7] = DMA_BUF_B;   /* 覆盖: 外设地址 (缓冲区 B) */
    func_09984(DMA_STREAM_B - 0x48, dma_stream_cfg);

    /* ---- 步骤 8: DMA 中端和 USART 中端使能 ---- */
    func_09948(DMA_STREAM_B, 0x10, 0);          /* DMA 流 B: 使能传输完成中端 */
    func_11950(USART_BASE, 2, 0);               /* USART: 使能 RX 中端 */

    func_09948(DMA_STREAM_B - 0x48, 0x10, 0);   /* DMA 流 A: 使能传输完成中端 */
    func_11950(USART_BASE, 1, 0);               /* USART: 使能 TX 中端 */

    /* ---- 步骤 9: DMA 流使能 ---- */
    func_09774(DMA_STREAM_B, 0);                 /* DMA 流 B 使能 */
    func_09774(DMA_STREAM_B - 0x48, 0);         /* DMA 流 A 使能 */

    /* ---- 步骤 10: USART 使能 ---- */
    func_11938(USART_BASE, 1);                   /* USART 模块使能 */

    /* ---- 步骤 11: 继续初始化 ---- */
    func_118EC(0xFF);                            /* 下一步初始化 (发送就绪标志) */
}

/*
 * 内存布局:
 *   0x08011770 - 0x08011774: PUSH {LR}; SUB SP,#0x5C
 *   0x08011774 - 0x080117A8: RCC 时钟使能 + USART 帧结构填充
 *   0x080117A8 - 0x080117CC: func_0C42C + func_0C4BC ×3
 *   0x080117CC - 0x080117E0: func_11418 ×2 (GPIO AF)
 *   0x080117E0 - 0x0801181C: DMA 配置结构填充 + func_1197E
 *   0x0801181C - 0x0801188C: func_0978C ×2 + func_09984 ×2
 *   0x0801188C - 0x080118B2: func_09948 ×2 + func_11950 ×2
 *   0x080118B2 - 0x080118CC: func_09774 ×2 + func_11938
 *   0x080118CC - 0x080118D6: func_118EC 调用 + ADD SP,#0x5C + POP {PC}
 *   0x080118D6 - 0x080118EC: 字面量池 (5 个地址)
 *
 * 字面量池 (已验证):
 *   0x080118D8: 0x40020000  DMA 控制器基址
 *   0x080118DC: 0x40013000  USART 外设基址
 *   0x080118E0: 0x40026458  DMA 流 B 基址
 *   0x080118E4: 0x20002FE0  DMA 缓冲区 A
 *   0x080118E8: 0x20002C20  DMA 缓冲区 B
 *
 * 注: 0x40020000 和 0x40013000 为 STM32F410 特定外设映射,
 *     可能与标准 STM32F4 参考手册不同.
 *
 * 调用 func_118EC 将控制传递至下一个连续的初始化函数
 * (0x080118EC-0x080119BC 范围, 可能在 func_62 或 func_14 中覆盖).
 */
