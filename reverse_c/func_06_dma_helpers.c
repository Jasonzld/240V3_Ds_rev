/**
 * @file func_06_dma_helpers.c
 * @brief DMA2 辅助函数集群
 * @addr  0x08009740 - 0x080099E0
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 包含 ADC_DMA_Init 调用的 DMA 底层注册器操作函数:
 *   1. dma_setup @ 0x08009740 (52 bytes) — DMA 中断标志清除 (LIFCR/HIFCR)
 *   2. dma_stream_enable @ 0x08009774 (24 bytes) — DMA_SxCR EN 位控制
 *   3. dma_stream_config @ 0x08009984 (92 bytes) — DMA Stream 全寄存器配置
 *
 * dma_stream_config 结构体 dma_cfg_t (0x3C = 60 bytes, 15×uint32_t):
 *   各字段被 OR 到 DMA_SxCR / DMA_SxFCR, 或直接写入 SxPAR/SxM0AR/SxNDTR
 */

#include "stm32f4xx_hal.h"

/* ================================================================
 * DMA Stream 配置结构体 (15×32bit = 60 bytes)
 *
 * 由 dma_stream_config() 解析:
 *   SxCR   = (current & 0xF01C803F) | cr_00 | cr_0C..cr_28 | cr_34 | cr_38
 *   SxFCR  = (current & ~7) | fcr_2C | fcr_30
 *   SxNDTR = ndtr
 *   SxPAR  = periph_addr
 *   SxM0AR = mem_addr
 * ================================================================ */
typedef struct {
    uint32_t cr_00;          /* [0x00] SxCR 位组0 */
    uint32_t periph_addr;    /* [0x04] SxPAR 外设地址 */
    uint32_t mem_addr;       /* [0x08] SxM0AR 内存地址 */
    uint32_t cr_0C;          /* [0x0C] SxCR 位组 (ORed) */
    uint32_t ndtr;           /* [0x10] SxNDTR 传输数目 */
    uint32_t cr_14;          /* [0x14] SxCR 位组 (ORed) */
    uint32_t cr_18;          /* [0x18] SxCR 位组 (ORed) */
    uint32_t cr_1C;          /* [0x1C] SxCR 位组 (ORed) */
    uint32_t cr_20;          /* [0x20] SxCR 位组 (ORed) */
    uint32_t cr_24;          /* [0x24] SxCR 位组 (ORed) */
    uint32_t cr_28;          /* [0x28] SxCR 位组 (ORed) */
    uint32_t fcr_2C;         /* [0x2C] SxFCR 位组0 (ORed with fcr_30) */
    uint32_t fcr_30;         /* [0x30] SxFCR 位组1 */
    uint32_t cr_34;          /* [0x34] SxCR 位组 (ORed) */
    uint32_t cr_38;          /* [0x38] SxCR 位组 (ORed) */
} dma_cfg_t;

/* DMA_SxCR 保留位掩码 */
#define DMA_SxCR_MASK   0xF01C803F   /* [0x080099D8] */

/* ================================================================
 * 1. dma_setup() @ 0x08009740
 *    清除 DMA 中断标志 (DMA_LIFCR / DMA_HIFCR)
 *    r0 = DMA Stream 基地址, r1 = 配置值 (位掩码)
 *
 *    bit29 决定目标寄存器:
 *      bit29=1 → DMA_HIFCR (offset 0x0C, Stream4-7), 掩码 0x0F7D0F7D
 *      bit29=0 → DMA_LIFCR (offset 0x08, Stream0-3), 掩码 0x0F7D0F7D
 *    DMA2 基地址为常量 0x40026400 (= 0x40026410 - 0x10, 从常量池加载)
 * ================================================================ */
void dma_setup(uint32_t *dma_stream, uint32_t config)
{
    uint32_t dma_base;       /* r0 */

    /* 判断 DMA1 还是 DMA2: 地址 >= 0x40026410 → DMA2 */
    if ((uint32_t)dma_stream >= 0x40026410) {
        dma_base = 0x40026400;                   /* 常量: 0x40026410 - 0x10 (从常量池加载) */
    } else {
        dma_base = 0x40026000;                   /* DMA1 基地址 (从常量池加载) */
    }

    if (config & 0x20000000) {
        /* bit29=1: 写 DMA_HIFCR (High Interrupt Flag Clear, Stream4-7) */
        *(volatile uint32_t *)(dma_base + 0x0C) = config & 0x0F7D0F7D;
    } else {
        /* bit29=0: 写 DMA_LIFCR (Low Interrupt Flag Clear, Stream0-3) */
        *(volatile uint32_t *)(dma_base + 0x08) = config & 0x0F7D0F7D;
    }
}

/* ================================================================
 * 2. dma_stream_enable() @ 0x08009774
 *    设置/清除 DMA_SxCR EN 位 (bit0)
 *    r0 = DMA Stream 基地址, r1 = 1(使能)/0(禁用)
 * ================================================================ */
void dma_stream_enable(uint32_t *dma_stream, uint32_t enable)
{
    if (enable) {
        *dma_stream |= 1;       /* 0x08009778: EN = 1 */
    } else {
        *dma_stream &= ~1;      /* 0x08009782: EN = 0 */
    }
}

/* ================================================================
 * 3. dma_stream_config() @ 0x08009984
 *    配置 DMA Stream 全部寄存器
 *    r0 = DMA Stream 基地址, r1 = dma_cfg_t *cfg
 *
 *    操作序列:
 *      SxCR   &= 0xF01C803F; SxCR |= cr_00|cr_0C|..|cr_38
 *      SxFCR  &= ~7;         SxFCR |= fcr_2C|fcr_30
 *      SxNDTR  = ndtr
 *      SxPAR   = periph_addr
 *      SxM0AR  = mem_addr
 * ================================================================ */
void dma_stream_config(uint32_t *dma_stream, dma_cfg_t *cfg)
{
    uint32_t sxcr;               /* r2 */
    uint32_t sxfcr;              /* r2 */
    uint32_t merged;             /* r3 */

    /* ---- DMA_SxCR (offset 0x00) ---- */
    sxcr  = *dma_stream;                         /* 0x08009988 */
    sxcr &= DMA_SxCR_MASK;                       /* 0x0800998C: 0xF01C803F */

    merged  = cfg->cr_00;                        /* 0x08009990: [r1] */
    merged |= cfg->cr_0C;                        /* 0x0800998E: [r1,#0xC] */
    merged |= cfg->cr_14;                        /* 0x08009994: [r1,#0x14] */
    merged |= cfg->cr_18;                        /* 0x08009998: [r1,#0x18] */
    merged |= cfg->cr_1C;                        /* 0x0800999C: [r1,#0x1C] */
    merged |= cfg->cr_20;                        /* 0x080099A0: [r1,#0x20] */
    merged |= cfg->cr_24;                        /* 0x080099A4: [r1,#0x24] */
    merged |= cfg->cr_28;                        /* 0x080099A8: [r1,#0x28] */
    merged |= cfg->cr_34;                        /* 0x080099AC: [r1,#0x34] */
    merged |= cfg->cr_38;                        /* 0x080099B0: [r1,#0x38] */

    sxcr |= merged;
    *dma_stream = sxcr;                          /* 0x080099B6 */

    /* ---- DMA_SxFCR (offset 0x14) ---- */
    sxfcr  = *(dma_stream + 0x14 / 4);           /* 0x080099B8: [r0,#0x14] */
    sxfcr &= ~7;                                 /* 0x080099BA: 清除 FTH[1:0] + DMDIS */

    merged  = cfg->fcr_2C;                       /* 0x080099BE: ldrd [r1,#0x2C] */
    merged |= cfg->fcr_30;
    sxfcr  |= merged;
    *(dma_stream + 0x14 / 4) = sxfcr;            /* 0x080099C6 */

    /* ---- DMA_SxNDTR (offset 0x04) ---- */
    *(dma_stream + 0x04 / 4) = cfg->ndtr;        /* 0x080099C8-CA: [r1,#0x10] → [r0,#4] */

    /* ---- DMA_SxPAR (offset 0x08) ---- */
    *(dma_stream + 0x08 / 4) = cfg->periph_addr; /* 0x080099CC-CE: [r1,#4] → [r0,#8] */

    /* ---- DMA_SxM0AR (offset 0x0C) ---- */
    *(dma_stream + 0x0C / 4) = cfg->mem_addr;    /* 0x080099D0-D2: [r1,#8] → [r0,#0xC] */
}
