/**
 * @file func_05_adc_helpers.c
 * @brief ADC1 辅助函数集群 (7个小函数)
 * @addr  0x080091A0 - 0x08009436
 * DISASSEMBLY-TRACED. Verified against Capstone.
 * NOTE: 0x080091A0-0x080091DC overlaps with func_160 (already verified).
 *
 * 包含 ADC_DMA_Init 调用的所有 ADC 底层寄存器操作函数:
 *   1. adc_start()          @ 0x080091A0 (24 bytes) — ADON 位控制
 *   2. adc_init_helper()    @ 0x080091B8 (48 bytes) — ADC_CCR 通用时钟配置
 *   3. adc_dma_config()     @ 0x080092F4 (22 bytes) — ADC_CR2 DMA 位 (bit8) 控制
 *   4. adc_enable()         @ 0x0800930A (22 bytes) — ADC_CR2 bit9 控制
 *   5. adc_init()           @ 0x08009320 (84 bytes) — ADC_CR1/CR2/SQR1 主配置
 *   6. adc_channel_config() @ 0x08009374 (184 bytes) — ADC SMPR/SQR 通道配置
 *   7. adc_final_init()     @ 0x0800942C (10 bytes) — SWSTART 启动转换
 */

#include "stm32f4xx_hal.h"

/* ================================================================
 * ADC 预初始化结构体 (16字节, adc_init_helper 使用)
 * 所有字段 OR 后写入 ADC_CCR (0x40012304)
 * ================================================================ */
typedef struct {
    uint32_t ccr_bits0;      /* [0] ADC_CCR 控制位组0 */
    uint32_t ccr_bits1;      /* [4] ADC_CCR 控制位组1 */
    uint32_t ccr_bits2;      /* [8] ADC_CCR 控制位组2 */
    uint32_t ccr_bits3;      /* [0xC] ADC_CCR 控制位组3 */
} adc_preinit_cfg_t;

/* ================================================================
 * ADC 主配置结构体 (adc_init 使用)
 *
 * 字段映射到 ADC 寄存器:
 *   ext_trig + (scan_mode<<8) → ADC_CR1 (mask 0xFCFFFEFF)
 *   cont_mode<<1 + cr2_0 + cr2_1 + cr2_2 → ADC_CR2 (mask 0xC0FFF7FD)
 *   (nbr_conv - 1) << 20 → ADC_SQR1 L[3:0]
 * ================================================================ */
typedef struct {
    uint32_t cr1_bits;       /* [0] CR1 位组 (含 ext_trig 等) */
    uint8_t  scan_mode;      /* [4] 扫描模式: 1=使能 → CR1 bit8 */
    uint8_t  cont_mode;      /* [5] 连续转换: 1=使能 → CR2 bit1 */
    uint8_t  padding_06[2];  /* [6-7] 对齐填充 */
    uint32_t cr2_bits0;      /* [8] CR2 位组0 */
    uint32_t cr2_bits1;      /* [0xC] CR2 位组1 */
    uint32_t cr2_bits2;      /* [0x10] CR2 位组2 */
    uint8_t  nbr_conv;       /* [0x14] 规则通道转换数目 → SQR1 L[3:0] */
} adc_init_cfg_t;

/* ADC_CCR 预分频器掩码: 保留 TSVREFE/VBATE, 清除 ADCPRE 等 */
#define ADC_CCR_MASK    0xFFFC30E0   /* [0x080091E0] */
#define ADC_CCR_ADDR    0x40012304   /* [0x080091DC] ADC 通用控制寄存器 */
#define ADC1_BASE       0x40012000   /* [0x080091E4] */

/* ADC_CR1 掩码: 清除 SCAN, AWDCH, AWDEN, JAWDEN, RES */
#define ADC_CR1_MASK    0xFCFFFEFF   /* [0x0800936C] */

/* ADC_CR2 掩码: 清除 CONT, DDS, EXTSEL, EXTEN, JEXTSEL, JEXTEN, etc */
#define ADC_CR2_MASK    0xC0FFF7FD   /* [0x08009370] */

/* ADC SQR1 L 字段 [23:20] 掩码 */
#define ADC_SQR1_L_MASK 0x00F00000   /* bits [23:20] */

/* ================================================================
 * 1. adc_start() @ 0x080091A0
 *    设置/清除 ADC_CR2 ADON 位 (bit0)
 *    r0 = ADC 基地址, r1 = 1(使能)/0(禁用)
 * ================================================================ */
void adc_start(ADC_TypeDef *adc, uint32_t enable)
{
    if (enable) {
        adc->CR2 |= 1;          /* 0x080091A4: ADON = 1 */
    } else {
        adc->CR2 &= ~1;         /* 0x080091AE: ADON = 0 */
    }
}

/* ================================================================
 * 2. adc_init_helper() @ 0x080091B8
 *    配置 ADC 通用控制寄存器 (ADC_CCR @ 0x40012304)
 *    读取当前 ADC_CCR, 用 0xFFFC30E0 掩码清除字段,
 *    然后将 cfg 的4个 uint32_t 字段 OR 后写入
 *    r0 = adc_preinit_cfg_t *cfg
 * ================================================================ */
void adc_init_helper(adc_preinit_cfg_t *cfg)
{
    uint32_t ccr_val;           /* r1 */
    uint32_t cfg_merged;        /* r2 */

    ccr_val = *(volatile uint32_t *)ADC_CCR_ADDR;  /* 0x080091BC: 读 ADC_CCR */
    ccr_val &= ADC_CCR_MASK;                        /* 0x080091C0: 清除预分频字段 */

    /* OR 所有4个结构体字段 */
    cfg_merged  = cfg->ccr_bits0;                      /* 0x080091C2: ldrd + orr */
    cfg_merged |= cfg->ccr_bits1;
    cfg_merged |= cfg->ccr_bits2;                      /* 0x080091CA */
    cfg_merged |= cfg->ccr_bits3;                      /* 0x080091CE */

    ccr_val |= cfg_merged;
    *(volatile uint32_t *)ADC_CCR_ADDR = ccr_val;     /* 0x080091D4: str [r2, #0x304] */
}

/* ================================================================
 * 3. adc_dma_config() @ 0x080092F4
 *    设置/清除 ADC_CR2 DMA 位 (bit8)
 *    r0 = ADC 基地址, r1 = 1(使能)/0(禁用)
 * ================================================================ */
void adc_dma_config(ADC_TypeDef *adc, uint32_t enable)
{
    if (enable) {
        adc->CR2 |= 0x100;      /* 0x080092F8: DMA=1 (bit8) */
    } else {
        adc->CR2 &= ~0x100;     /* 0x08009302: DMA=0 */
    }
}

/* ================================================================
 * 4. adc_enable() @ 0x0800930A
 *    设置/清除 ADC_CR2 bit9 (DDS)
 *    r0 = ADC 基地址, r1 = 1(使能)/0(禁用)
 * ================================================================ */
void adc_enable(ADC_TypeDef *adc, uint32_t enable)
{
    if (enable) {
        adc->CR2 |= 0x200;       /* 0x0800930E: ORR bit9 */
    } else {
        adc->CR2 &= ~0x200;      /* 0x08009318: BIC bit9 */
    }
}

/* ================================================================
 * 5. adc_init() @ 0x08009320
 *    配置 ADC_CR1, ADC_CR2, ADC_SQR1 (L 字段)
 *    r0 = ADC 基地址, r1 = adc_init_cfg_t *cfg
 *
 *    CR1 写入: (cfg->cr1_bits | (cfg->scan_mode << 8)) & ~mask
 *    CR2 写入: (cfg->cont_mode<<1 | cr2_bits*) & ~mask
 *    SQR1:     (cfg->nbr_conv - 1) << 20
 * ================================================================ */
void adc_init(ADC_TypeDef *adc, adc_init_cfg_t *cfg)
{
    uint32_t cr1_val;           /* r0 */
    uint32_t cr2_val;           /* r0 */
    uint32_t sqr1_val;          /* r0 */
    uint32_t merged;            /* r4 */

    /* ---- ADC_CR1 (offset 0x04) ---- */
    cr1_val  = adc->CR1;                           /* 0x08009328 */
    cr1_val &= ADC_CR1_MASK;                       /* 0x0800932C: 0xFCFFFEFF */
    merged   = cfg->cr1_bits | (cfg->scan_mode << 8); /* 0x08009330-34 */
    cr1_val |= merged;
    adc->CR1 = cr1_val;                            /* 0x08009338 */

    /* ---- ADC_CR2 (offset 0x08) ---- */
    cr2_val  = adc->CR2;                           /* 0x0800933A */
    cr2_val &= ADC_CR2_MASK;                       /* 0x0800933E: 0xC0FFF7FD */
    merged   = cfg->cr2_bits0;                     /* 0x08009346: [r1+8] */
    merged  |= cfg->cr2_bits1;                     /* 0x08009340-44: ldrd [r1,#0xC] */
    merged  |= cfg->cr2_bits2;
    merged  |= (cfg->cont_mode << 1);              /* 0x0800934A-4C: byte [r1+5]<<1 → CR2 bit1 */
    cr2_val |= merged;
    adc->CR2 = cr2_val;                            /* 0x08009352 */

    /* ---- ADC_SQR1 L[3:0] (offset 0x2C, bits [23:20]) ---- */
    sqr1_val  = adc->SQR1;                         /* 0x08009354 */
    sqr1_val &= ~ADC_SQR1_L_MASK;                  /* 0x08009356: bic #0xF00000 */
    sqr1_val |= (uint8_t)(cfg->nbr_conv - 1) << 20; /* 0x08009358-66: ldrb[r1,#0x14], sub#1, uxtb (8-bit z-ext) */
    adc->SQR1 = sqr1_val;                          /* 0x08009366 */
}

/* ================================================================
 * 6. adc_channel_config() @ 0x08009374
 *    配置单个 ADC 通道的采样时间和规则序列排名
 *    r0 = ADC 基地址
 *    r1 = 通道编号 (0..18)
 *    r2 = 规则序列排名 (1..16)
 *    r3 = 采样时间 (0=3cyc, 1=15cyc, 2=28cyc, 3=56cyc, 4=84cyc, 5=112cyc, 6=144cyc, 7=480cyc)
 *
 *    采样时间寄存器:
 *      ch 0-9  → ADC_SMPR2 (offset 0x10), 每通道3位, 位域 = ch * 3
 *      ch 10-18 → ADC_SMPR1 (offset 0x0C), 位域 = (ch - 10) * 3
 *
 *    规则序列寄存器:
 *      rank 1-6  → ADC_SQR3 (offset 0x34), 位域 = (rank - 1) * 5
 *      rank 7-12 → ADC_SQR2 (offset 0x30), 位域 = (rank - 7) * 5
 *      rank 13-16 → ADC_SQR1 (offset 0x2C), 位域 = (rank - 13) * 5
 * ================================================================ */
void adc_channel_config(ADC_TypeDef *adc, uint32_t channel,
                        uint32_t rank, uint32_t sample_time)
{
    uint32_t smp_val;           /* r0 */
    uint32_t sq_val;            /* r0 */
    uint32_t shift;             /* r6/r7 */
    uint32_t mask;              /* r1 */

    /* ---- 采样时间配置 ---- */
    if (channel <= 9) {
        /* ADC_SMPR2 (offset 0x10): ch 0-9, 3位/通道 */
        smp_val  = adc->SMPR2;                              /* 0x080093A6 */
        shift    = channel + channel * 2;                   /* 0x080093A8: ch*3 */
        mask     = 7 << shift;                              /* 0x080093AC-AE: 3-bit mask */
        smp_val &= ~mask;                                   /* 0x080093B2 */
        smp_val |= sample_time << shift;                    /* 0x080093B4-BC */
        adc->SMPR2 = smp_val;                               /* 0x080093BE */
    } else {
        /* ADC_SMPR1 (offset 0x0C): ch 10-18, 3位/通道 */
        smp_val  = adc->SMPR1;                              /* 0x08009382 */
        shift    = (channel - 10) + (channel - 10) * 2;     /* 0x08009384-88: (ch-10)*3 */
        mask     = 7 << shift;                              /* 0x0800938C-8E */
        smp_val &= ~mask;                                   /* 0x08009392 */
        smp_val |= sample_time << shift;                    /* 0x08009394-A0 */
        adc->SMPR1 = smp_val;                               /* 0x080093A2 */
    }

    /* ---- 规则序列排名配置 ---- */
    /* 注: 汇编使用 cmp r2,#7; bge → rank<7 走 SQR3; cmp #13; bge → rank<13 走 SQR2 */
    if (rank < 7) {
        /* ADC_SQR3 (offset 0x34): rank 1-6 (cmp #7, bge → skip if >=7) */
        sq_val  = adc->SQR3;                                /* 0x080093C4 */
        shift   = (rank - 1) + (rank - 1) * 4;              /* 0x080093C6-C8: (rank-1)*5 */
        mask    = 0x1F << shift;                            /* 0x080093CC-CE: 5-bit mask */
        sq_val &= ~mask;                                    /* 0x080093D2 */
        sq_val |= channel << shift;                         /* 0x080093D4-DE */
        adc->SQR3 = sq_val;                                 /* 0x080093E0 */
    } else if (rank < 13) {
        /* ADC_SQR2 (offset 0x30): rank 7-12 (cmp #13, bge → skip if >=13) */
        sq_val  = adc->SQR2;                                /* 0x080093E8 */
        shift   = (rank - 7) + (rank - 7) * 4;              /* 0x080093EA-EC: (rank-7)*5 */
        mask    = 0x1F << shift;                            /* 0x080093F0-F2 */
        sq_val &= ~mask;                                    /* 0x080093F6 */
        sq_val |= channel << shift;                         /* 0x080093F8-02 */
        adc->SQR2 = sq_val;                                 /* 0x08009404 */
    } else {
        /* ADC_SQR1 (offset 0x2C): rank 13-16, SQ13-SQ16 */
        sq_val  = adc->SQR1;                                /* 0x08009408 */
        shift   = (rank - 13) + (rank - 13) * 4;            /* 0x0800940A-0E: (rank-13)*5 */
        mask    = 0x1F << shift;                            /* 0x08009412-14 */
        sq_val &= ~mask;                                    /* 0x08009418 */
        sq_val |= channel << shift;                         /* 0x0800941A-26 */
        adc->SQR1 = sq_val;                                 /* 0x08009428 */
    }
}

/* ================================================================
 * 7. adc_final_init() @ 0x0800942C
 *    触发 ADC 软件启动转换 (ADC_CR2 SWSTART, bit30)
 *    r0 = ADC 基地址
 * ================================================================ */
void adc_final_init(ADC_TypeDef *adc)
{
    adc->CR2 |= 0x40000000;    /* 0x0800942E: SWSTART = 1, 触发转换 */
}
