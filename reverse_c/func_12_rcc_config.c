/**
 * @file func_12_rcc_config.c
 * @brief 函数: RCC_Config — 读取 RCC 时钟配置并计算各总线频率
 * @addr  0x08011438 - 0x08011520 (232 bytes, 96 指令含常量池)
 * DISASSEMBLY-TRACED. Verified against Capstone (inline trace annotations).
 *
 * 从 RCC 寄存器读取当前时钟树配置, 计算实际总线频率:
 *   - SYSCLK (系统时钟)
 *   - HCLK   (AHB 总线时钟)
 *   - PCLK2  (APB2 总线时钟)
 *   - PCLK1  (APB1 总线时钟)
 *
 * 时钟源分支:
 *   SWS=0 (HSI): SYSCLK = 16 MHz (0x00F42400)
 *   SWS=1 (HSE): SYSCLK = 25 MHz (0x017D7840)
 *   SWS=2 (PLL): SYSCLK = VCO / PLLP (完整计算)
 *   SWS=3:       SYSCLK = 16 MHz (HSI fallback)
 *
 * 分频器查找表位于 RAM 0x200002E4, 由其他初始化代码填入。
 *
 * 输出结构体 rcc_freq_t (16字节):
 *   [0]  uint32_t sysclk   — 系统时钟频率 (Hz)
 *   [4]  uint32_t hclk     — AHB 总线时钟 (Hz)
 *   [8]  uint32_t pclk1    — APB1 总线时钟 (Hz)
 *   [12] uint32_t pclk2    — APB2 总线时钟 (Hz)
 */

#include "stm32f4xx_hal.h"

/* RCC 寄存器 */
#define RCC_BASE        0x40023800
#define RCC_CFGR        (*(volatile uint32_t *)(RCC_BASE + 0x08))
#define RCC_PLLCFGR     (*(volatile uint32_t *)(RCC_BASE + 0x04))

/* RCC_CFGR 位域 */
#define RCC_CFGR_SWS_Pos        2
#define RCC_CFGR_SWS_Msk        (0x3 << RCC_CFGR_SWS_Pos)
#define RCC_CFGR_HPRE_Pos       4
#define RCC_CFGR_HPRE_Msk       (0xF << RCC_CFGR_HPRE_Pos)
#define RCC_CFGR_PPRE1_Pos      10
#define RCC_CFGR_PPRE1_Msk      (0x7 << RCC_CFGR_PPRE1_Pos)
#define RCC_CFGR_PPRE2_Pos      13
#define RCC_CFGR_PPRE2_Msk      (0x7 << RCC_CFGR_PPRE2_Pos)

/* RCC_PLLCFGR 位域 */
#define RCC_PLLCFGR_PLLM_Pos    0
#define RCC_PLLCFGR_PLLM_Msk    0x3F
#define RCC_PLLCFGR_PLLN_Pos    6
#define RCC_PLLCFGR_PLLN_Msk    (0x1FF << RCC_PLLCFGR_PLLN_Pos)
#define RCC_PLLCFGR_PLLP_Pos    16
#define RCC_PLLCFGR_PLLP_Msk    (0x3 << RCC_PLLCFGR_PLLP_Pos)
#define RCC_PLLCFGR_PLLSRC_Pos  22
#define RCC_PLLCFGR_PLLSRC      (1 << RCC_PLLCFGR_PLLSRC_Pos)

/* 时钟源频率 */
#define HSI_FREQ        16000000    /* 0x00F42400 */
#define HSE_FREQ        25000000    /* 0x017D7840 */

/* 预分频器查找表 (在运行时由初始化代码填入 RAM) */
#define PRESC_TABLE     ((volatile uint8_t *)0x200002E4)

/* 输出结构体: 各总线频率 */
typedef struct {
    uint32_t sysclk;             /* [0] 系统时钟 (Hz) */
    uint32_t hclk;               /* [4] AHB 时钟 (Hz) */
    uint32_t pclk1;              /* [8] APB1 时钟 (Hz) */
    uint32_t pclk2;              /* [12] APB2 时钟 (Hz) */
} rcc_freq_t;

/* ================================================================
 * RCC_Config() @ 0x08011438
 *   r0 = rcc_freq_t *freq (输出)
 *
 * 流程:
 *   1. 读取 SWS[1:0] 确定当前系统时钟源
 *   2. 计算 SYSCLK 频率
 *      - HSI: 16 MHz 固定
 *      - HSE: 25 MHz 固定
 *      - PLL: VCO = (SRC_FREQ / PLLM) * PLLN, SYSCLK = VCO / PLLP
 *   3. 根据 HPRE/PPRE1/PPRE2 查找分频系数, 计算 HCLK/PCLK1/PCLK2
 * ================================================================ */
void RCC_Config(rcc_freq_t *freq)
{
    uint32_t cfgr;               /* r7 — RCC_CFGR 值 */
    uint32_t sws;                /* r1 — 时钟源状态 */
    uint32_t pllcfgr;            /* r7 — PLLCFGR 值 */
    uint32_t pllsrc;             /* r6 — PLL 源 (0=HSI, 1=HSE) */
    uint32_t pllm;               /* r3 — PLLM 分频系数 */
    uint32_t plln;               /* ip — PLLN 倍频系数 */
    uint32_t pllp;               /* r7 — PLLP 分频系数 */
    uint32_t vco_in;             /* r7 — VCO 输入频率 */
    uint32_t vco;                /* r4 — VCO 输出频率 */
    uint32_t presc_idx;          /* r1 — 分频器索引 */
    uint32_t presc_val;          /* r2 — 分频器值 */

    /* ---- 第1步: 读取 RCC_CFGR 获取时钟源 ---- */
    cfgr = RCC_CFGR;                                  /* 0x08011446-48 */
    sws  = (cfgr & RCC_CFGR_SWS_Msk) >> RCC_CFGR_SWS_Pos;  /* 0x0801144A: and #0xC */

    /* ---- 第2步: 计算 SYSCLK ---- */
    if (sws == 0) {
        /* SWS=0: HSI 16 MHz */
        freq->sysclk = HSI_FREQ;                      /* 0x0801145A-5E: str 0x00F42400 */
    } else if (sws == 1) {
        /* SWS=1: HSE 25 MHz */
        freq->sysclk = HSE_FREQ;                      /* 0x08011460-64: str 0x017D7840 */
    } else if (sws == 2) {
        /* ---- SWS=2: PLL ---- */
        /* 读取 PLL 源选择 (PLLSRC: bit22) */
        pllcfgr = RCC_PLLCFGR;                        /* 0x08011466-6A: ldr; sub #4 */
        pllsrc  = (pllcfgr & RCC_PLLCFGR_PLLSRC) >> RCC_PLLCFGR_PLLSRC_Pos;  /* 0x0801146C: ubfx #22,#1 */
        pllm    = pllcfgr & RCC_PLLCFGR_PLLM_Msk;     /* 0x08011470-76: ldr; and #0x3f */

        /* VCO 输入频率 = SRC / PLLM */
        if (pllsrc) {
            vco_in = HSE_FREQ / pllm;                 /* 0x0801147C-7E: udiv 25M / PLLM */
        } else {
            vco_in = HSI_FREQ / pllm;                 /* 0x08011498-9A: udiv 16M / PLLM */
        }

        /* VCO = VCO_in * PLLN */
        plln = (pllcfgr & RCC_PLLCFGR_PLLN_Msk) >> RCC_PLLCFGR_PLLN_Pos;  /* 0x0801148E: ubfx #6,#9 */
        vco  = vco_in * plln;                         /* 0x08011492: mul */

        /* PLLP 编码: 00→2, 01→4, 10→6, 11→8 */
        pllp  = (pllcfgr & RCC_PLLCFGR_PLLP_Msk) >> RCC_PLLCFGR_PLLP_Pos;  /* 0x080114B8: ubfx #16,#2 */
        pllp  = (pllp + 1) * 2;                       /* 0x080114BC-BE: add #1; lsl #1 */

        freq->sysclk = vco / pllp;                    /* 0x080114C0-C4: udiv; str */
    } else {
        /* SWS=3: 保留 — 回退到 HSI */
        freq->sysclk = HSI_FREQ;                      /* 0x080114C8-CA: str 0x00F42400 */
    }

    /* ---- 第3步: 计算 AHB/APB 总线频率 ---- */
    cfgr = RCC_CFGR;                                  /* 0x080114D0-D2: 重新读取 */

    /* AHB 分频 (HPRE) */
    presc_idx = (cfgr & RCC_CFGR_HPRE_Msk) >> RCC_CFGR_HPRE_Pos;  /* 0x080114D4-D8: and #0xF0; lsr #4 */
    presc_val = PRESC_TABLE[presc_idx];               /* 0x080114DA-DC: ldrb [r7,r1] */
    freq->hclk = freq->sysclk >> presc_val;            /* 0x080114DE-E2: lsr r7,r2; str */

    /* APB1 分频 (PPRE1: bits [12:10]) */
    presc_idx = (cfgr & RCC_CFGR_PPRE1_Msk) >> RCC_CFGR_PPRE1_Pos;  /* 0x080114E4-EC: and #0x1C00; lsr #0xA */
    presc_val = PRESC_TABLE[presc_idx];               /* 0x080114EE-F0: ldrb [r7,r1] */
    freq->pclk1 = freq->hclk >> presc_val;             /* 0x080114F2-F6: lsr r7,r2; str [r0,#8] */

    /* APB2 分频 (PPRE2: bits [15:13]) */
    presc_idx = (cfgr & RCC_CFGR_PPRE2_Msk) >> RCC_CFGR_PPRE2_Pos;  /* 0x080114F8-00: and #0xE000; lsr #0xD */
    presc_val = PRESC_TABLE[presc_idx];               /* 0x08011502-04: ldrb [r7,r1] */
    freq->pclk2 = freq->hclk >> presc_val;             /* 0x08011506-0A: lsr r7,r2; str [r0,#0xC] */

    /* 0x0801150C: pop {r4,r5,r6,r7,pc} */
}
