/**
 * @file func_03_adc_dma_init.c
 * @brief 函数: ADC_DMA_Init — ADC1 + DMA2 Stream0 初始化
 * @addr  0x080091E8 - 0x0800930A (290 bytes, ~145 指令)
 * DISASSEMBLY-TRACED. Verified against Capstone (entry + key sections).
 *
 * 配置 STM32F410 ADC1 多通道电流/电压采样:
 *   - ADC1 CH0 (PA0) 和 CH1 (PA1) 模拟输入
 *   - DMA2 Stream0 外设→内存循环传输
 *   - ADC 采样缓冲区: 0x20000908
 *   - 12-bit 分辨率 (默认), 扫描模式, 连续转换, 2 通道
 *
 * 调用辅助函数:
 *   - Func_SystemInit_1(0x00400001, 1):   0x080113B8 — 系统初始化
 *   - Func_SystemInit_2(0x0100, 1):       0x080113F8 — 外设时钟使能
 *   - dma_clock_enable(0x40026410):       0x0800978C — DMA2 时钟使能
 *   - dma_stream_config(DMA2_S0, cfg):   0x08009984 — DMA Stream 配置
 *   - dma_stream_enable(DMA2_S0, 1):      0x08009774 — DMA Stream 使能
 *   - gpio_port_init(GPIOA, cfg):         0x0800C42C — GPIO 端口配置
 *   - adc_init_helper(cfg):               0x080091B8 — ADC 预初始化
 *   - adc_init(ADC1, cfg):                0x08009320 — ADC 主配置
 *   - adc_channel_config(ADC1, ch, ...):  0x08009374 — ADC 通道配置
 *   - adc_enable(ADC1, 1):                0x0800930A — ADC 使能
 *   - adc_dma_config(ADC1, 1):            0x080092F4 — ADC DMA 模式 (ADC_CR2 bit8)
 *   - adc_start(ADC1, 1):                 0x080091A0 — ADC 启动转换
 *   - adc_final_init(ADC1):               0x0800942C — ADC 校准/最终配置
 */

#include "stm32f4xx_hal.h"

/* ================================================================
 * GPIO 配置结构体 (同 func_02)
 * ================================================================ */
typedef struct {
    uint32_t pin_mask;
    uint8_t  mode;       /* 0=输入, 1=输出, 2=AF, 3=模拟 */
    uint8_t  speed;
    uint8_t  otype;
    uint8_t  pull;       /* 0=无, 1=上拉, 2=下拉 */
} gpio_cfg_t;

/* ================================================================
 * DMA Stream 配置结构体 (15×32bit = 60字节, [sp+8]~[sp+0x43])
 * 由 dma_stream_config() (0x08009984) 解析:
 *   SxCR   = (current & 0xF01C803F) | cr_00|cr_0C|cr_14..cr_28|cr_34|cr_38
 *   SxFCR  = (current & ~7) | fcr_2C|fcr_30
 *   SxNDTR = ndtr
 *   SxPAR  = periph_addr
 *   SxM0AR = mem_addr
 * ================================================================ */
typedef struct {
    uint32_t cr_00;          /* [sp+0x08] = 0        SxCR 位组0 */
    uint32_t periph_addr;    /* [sp+0x0C] = 0x4001204C → SxPAR (ADC1_DR) */
    uint32_t mem_addr;       /* [sp+0x10] = 0x20000908 → SxM0AR (ADC缓冲) */
    uint32_t cr_0C;          /* [sp+0x14] = 0        SxCR 位组 (ORed) */
    uint32_t ndtr;           /* [sp+0x18] = 2        → SxNDTR (2数据项) */
    uint32_t cr_14;          /* [sp+0x1C] = 0        SxCR 位组 (ORed) */
    uint32_t cr_18;          /* [sp+0x20] = 0x400    SxCR 位组 (MSIZE=16bit) */
    uint32_t cr_1C;          /* [sp+0x24] = 0x800    SxCR 位组 (PSIZE=16bit) */
    uint32_t cr_20;          /* [sp+0x28] = 0x2000   SxCR 位组 */
    uint32_t cr_24;          /* [sp+0x2C] = 0x100    SxCR 位组 */
    uint32_t cr_28;          /* [sp+0x30] = 0x20000  SxCR 位组 */
    uint32_t fcr_2C;         /* [sp+0x34] = 0        SxFCR 位组0 */
    uint32_t fcr_30;         /* [sp+0x38] = 1        SxFCR 位组1 (FTH=1/2) */
    uint32_t cr_34;          /* [sp+0x3C] = 0        SxCR 位组 (ORed) */
    uint32_t cr_38;          /* [sp+0x40] = 0        SxCR 位组 (ORed) */
} dma_stream_cfg_t;

/* ================================================================
 * ADC 初始化配置结构体 ([sp+0x54], 由 adc_init() 0x08009320 解析)
 *
 * adc_init 写入规则:
 *   CR1 = (CR1 & 0xFCFFFEFF) | cr1_bits | (scan_mode << 8)
 *   CR2 = (CR2 & 0xC0FFF7FD) | cr2_bits0|cr2_bits1|cr2_bits2 | (cont_mode<<1)
 *   SQR1 L[3:0] = nbr_conv - 1  (分辨率默认12-bit, 由 CR1 掩码保留位决定)
 * ================================================================ */
typedef struct {
    uint32_t cr1_bits;        /* [sp+0x54] CR1 原始位: 0 */
    uint8_t  scan_mode;       /* [sp+0x58] 扫描模式: 1=使能 → CR1 bit8 */
    uint8_t  cont_mode;       /* [sp+0x59] 连续转换: 1=使能 → CR2 bit1 */
    uint8_t  padding_5a[2];   /* 对齐填充 */
    uint32_t cr2_bits0;       /* [sp+0x5C] CR2 位组0: 0 */
    uint32_t cr2_bits1;       /* [sp+0x60] CR2 位组1: 0 */
    uint32_t cr2_bits2;       /* [sp+0x64] CR2 位组2: 0 */
    uint8_t  nbr_conv;        /* [sp+0x68] 规则通道转换数目: 2 → SQR1 L[3:0]=1 */
} adc_init_cfg_t;

/* ADC 预初始化结构体 (16字节全零, [sp+0x44], 由 adc_init_helper() 0x080091B8 解析) */
typedef struct {
    uint32_t ccr_bits0;       /* [sp+0x44] ADC_CCR 位组0 */
    uint32_t ccr_bits1;       /* [sp+0x48] ADC_CCR 位组1 */
    uint32_t ccr_bits2;       /* [sp+0x4C] ADC_CCR 位组2 */
    uint32_t ccr_bits3;       /* [sp+0x50] ADC_CCR 位组3 */
} adc_preinit_cfg_t;

/* ================================================================
 * 常量地址
 * ================================================================ */
#define DMA2_Stream0_BASE    0x40026410   /* DMA2 Stream0 基地址 */
#define ADC1_BASE            0x40012000   /* ADC1 基地址 (0x4001204C - 0x4C) */
#define ADC1_DR_ADDR         0x4001204C   /* ADC1 数据寄存器地址 */
#define ADC_BUFFER_ADDR      0x20000908   /* ADC DMA 缓冲区 */
#define GPIOA_BASE           0x40020000   /* GPIOA 基地址 */

/* ================================================================
 * 外部函数声明
 * ================================================================ */
extern void Func_SystemInit_1(uint32_t a, uint32_t b);   /* 0x080113B8 */
extern void Func_SystemInit_2(uint32_t a, uint32_t b);   /* 0x080113F8 */
extern void dma_clock_enable(uint32_t *dma_stream);                      /* 0x0800978C */
extern void dma_stream_config(uint32_t *dma_stream, dma_stream_cfg_t *cfg); /* 0x08009984 */
extern void dma_stream_enable(uint32_t *dma_stream, uint32_t enable);    /* 0x08009774 */
extern void gpio_port_init(GPIO_TypeDef *port, gpio_cfg_t *cfg);         /* 0x0800C42C */
extern void adc_init_helper(adc_preinit_cfg_t *cfg);     /* 0x080091B8 */
extern void adc_init(ADC_TypeDef *adc, adc_init_cfg_t *cfg);             /* 0x08009320 */
extern void adc_channel_config(ADC_TypeDef *adc, uint32_t channel,       /* 0x08009374 */
                               uint32_t rank, uint32_t sample_time);
extern void adc_enable(ADC_TypeDef *adc, uint32_t enable);               /* 0x0800930A */
extern void adc_dma_config(ADC_TypeDef *adc, uint32_t enable);           /* 0x080092F4 */
extern void adc_start(ADC_TypeDef *adc, uint32_t enable);                /* 0x080091A0 */
extern void adc_final_init(ADC_TypeDef *adc);                             /* 0x0800942C */

/* ================================================================
 * ADC_DMA_Init() — ADC1 + DMA2 Stream0 初始化
 * ================================================================ */
void ADC_DMA_Init(void)
{
    dma_stream_cfg_t dma_cfg;        /* [sp+0x08] DMA Stream 配置 */
    gpio_cfg_t       gpio_cfg;       /* [sp+0x00] GPIO 端口配置 */
    adc_preinit_cfg_t adc_pre;       /* [sp+0x44] ADC 预初始化 (16字节全零) */
    adc_init_cfg_t   adc_cfg;        /* [sp+0x54] ADC 初始化配置 */

    /* ---- 系统级初始化 ---- */
    /* [sp+4] 以下未显式使用, 参数通过立即数/寄存器传递 */
    Func_SystemInit_1(0x00400001, 1);   /* 0x080091F0: 系统初始化 (bit22 DMA2? + bit0) */
    Func_SystemInit_2(0x0100, 1);       /* 0x080091F8: 外设时钟 (bit8) */

    /* ---- DMA2 时钟使能 ---- */
    dma_clock_enable(DMA2_Stream0_BASE); /* 0x080091FE: r0 = 0x40026410 */

    /* ============================================================ */
    /* DMA2 Stream0 配置 ([sp+8] struct 15字段) */
    /* ============================================================ */
    dma_cfg.cr_00       = 0;                /* [sp+0x08] SxCR 位组 */
    dma_cfg.periph_addr = ADC1_DR_ADDR;     /* [sp+0x0C] = 0x4001204C → SxPAR */
    dma_cfg.mem_addr    = ADC_BUFFER_ADDR;  /* [sp+0x10] = 0x20000908 → SxM0AR */
    dma_cfg.cr_0C       = 0;                /* [sp+0x14] SxCR 位组 */
    dma_cfg.ndtr        = 2;                /* [sp+0x18] → SxNDTR: 每次DMA请求传输2项 */
    dma_cfg.cr_14       = 0;                /* [sp+0x1C] SxCR 位组 */

    /* SxCR 位域通过移位链生成: 0x400 → 0x800 → 0x2000 → 0x100 → 0x20000 */
    dma_cfg.cr_18       = 0x400;            /* [sp+0x20] MSIZE=16-bit (bit10) */
    dma_cfg.cr_1C       = 0x800;            /* [sp+0x24] PSIZE=16-bit (bit11) */
    dma_cfg.cr_20       = 0x2000;           /* [sp+0x28] PINCOS (bit13) */
    dma_cfg.cr_24       = 0x100;            /* [sp+0x2C] MINC=1 (bit8) — 注: 0x2000>>5=0x100 */
    dma_cfg.cr_28       = 0x20000;          /* [sp+0x30] 其他 CR 位 */

    dma_cfg.fcr_2C      = 0;                /* [sp+0x34] SxFCR 位组 */
    dma_cfg.fcr_30      = 1;                /* [sp+0x38] → SxFCR: FTH=01 (1/2 FIFO) */
    dma_cfg.cr_34       = 0;                /* [sp+0x3C] SxCR 位组 */
    dma_cfg.cr_38       = 0;                /* [sp+0x40] SxCR 位组 */

    dma_stream_config(DMA2_Stream0_BASE, &dma_cfg);  /* 0x08009242: 写入 DMA 寄存器 */

    /* ---- DMA2 Stream0 使能 ---- */
    dma_stream_enable(DMA2_Stream0_BASE, 1); /* 0x0800924A: 启动 DMA Stream */

    /* ============================================================ */
    /* GPIOA PA0, PA1 配置为模拟输入 (ADC1 CH0/CH1) */
    /* ============================================================ */
    gpio_cfg.pin_mask = 3;          /* PA0 + PA1 */
    gpio_cfg.mode     = 3;          /* 模拟模式 */
    gpio_cfg.pull     = 0;          /* 无上下拉 */
    /* gpio_cfg.speed/otype 在模拟模式下无意义, 保持未初始化 */
    gpio_port_init(GPIOA, &gpio_cfg);  /* 0x08009260 */

    /* ============================================================ */
    /* ADC 预初始化 (16字节全零) */
    /* ============================================================ */
    adc_pre.ccr_bits0 = 0;
    adc_pre.ccr_bits1 = 0;
    adc_pre.ccr_bits2 = 0;
    adc_pre.ccr_bits3 = 0;
    adc_init_helper(&adc_pre);       /* 0x08009270: ADC 时钟/电源预配置 */

    /* ============================================================ */
    /* ADC1 主配置 */
    /* ============================================================ */
    adc_cfg.cr1_bits   = 0;          /* [sp+0x54] CR1 原始位: 无 */
    adc_cfg.scan_mode  = 1;          /* [sp+0x58] 扫描模式使能 (多通道) */
    adc_cfg.cont_mode  = 1;          /* [sp+0x59] 连续转换模式使能 */
    adc_cfg.cr2_bits0  = 0;          /* [sp+0x5C] CR2 位组: 无 */
    adc_cfg.cr2_bits1  = 0;          /* [sp+0x60] CR2 位组: 无 */
    adc_cfg.cr2_bits2  = 0;          /* [sp+0x64] CR2 位组: 无 */
    adc_cfg.nbr_conv   = 2;          /* [sp+0x68] 2 个规则通道 (CH0+CH1) */

    adc_init(ADC1, &adc_cfg);        /* 0x08009296: r0 = 0x40012000 (= 0x4001204C - 0x4C) */

    /* ============================================================ */
    /* ADC 通道配置: 采样时间 = 3 (56 cycles) */
    /* ============================================================ */
    adc_channel_config(ADC1, 0, 1, 3);  /* 0x080092A4: CH0 (PA0), 顺序1, sample=3 */
    adc_channel_config(ADC1, 1, 2, 3);  /* 0x080092B2: CH1 (PA1), 顺序2, sample=3 */

    /* ---- ADC 使能 ---- */
    adc_enable(ADC1, 1);                 /* 0x080092BC: ADON=1 */

    /* ---- ADC DMA 模式使能 (ADC_CR2 bit8) ---- */
    adc_dma_config(ADC1, 1);             /* 0x080092C6: ADC_CR2 |= (1<<8) */

    /* ---- ADC 启动 ---- */
    adc_start(ADC1, 1);                  /* 0x080092D0: ADON=1 (bit0) */

    /* ---- ADC 软件触发转换 ---- */
    adc_final_init(ADC1);                /* 0x080092D8: SWSTART=1 (bit30) */
}
