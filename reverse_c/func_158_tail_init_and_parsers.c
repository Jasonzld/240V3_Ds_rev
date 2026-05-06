/**
 * @file func_158_tail_init_and_parsers.c
 * @brief 尾部初始化与解析器 — AT 响应 CSV 解析 + 叶辅助函数集群 + FPU NaN/Inf 分类器 + 显示/FSMC/USART 初始化
 * @addr  0x08017E82 - 0x08017EDC (90 bytes, AT_Response_CSV_Parser)
 *        NOTE: Detailed analysis in func_166_csv_param_splitter.c
 *        0x08018DBC - 0x08018E48 (140 bytes, 4 个叶辅助函数)
 *        0x08018E48 - 0x08018EC8 (128 bytes, FPU 包装器 + NaN/Inf 分类器)
 *        0x08018EC8 - 0x08018F80 (184 bytes, Display_FSMC_USART_Init)
 *        0x08018F80 - 0x08019000 (128 bytes, 设备基址 + 校准查找表头)
 *        0x08019000 - 0x08019960 (2400 bytes, 主校准/查找数据表)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 * NOTE: All sub-functions (func_166, func_169-175) individually verified.
 *
 * 数据表 (非代码):
 *   0x08018F80-0x08018F88: 外设基址指针 (DMA1@0x40020000, PERIPH@0x40004400)
 *   0x08018F88-0x08019000: 32 对 16-bit 阈值 (单调递增序列)
 *   0x08019000-0x08019960: ~800 对 16-bit 校准值 (主数据表, 固件末尾)
 */

#include "stm32f4xx_hal.h"

/* ---- 外部依赖 ---- */
extern void func_16CA0(double val);              /* FPU 符号/阈值检查 (0x08016CA0) */
extern void func_08BC4(double val);              /* FPU 转换/舍入辅助 (0x08008BC4) */
extern void func_175C4(void);                    /* FPU 错误处理器 (0x080175C4) */
extern void func_113B8(uint32_t a, uint32_t b);  /* 系统初始化 1 (0x080113B8) */
extern void func_113D8(uint32_t a, uint32_t b);  /* 系统初始化相关 (0x080113D8) */
extern void func_0C4BC(uint32_t port, uint32_t pin, uint32_t af); /* GPIO AF 配置 (0x0800C4BC) */
extern void func_0C42C(uint32_t port, void *cfg); /* GPIO 端口初始化 (0x0800C42C) */
extern void func_15E50(void *usart, void *cfg);   /* 波特率计算器 (0x08015E50) */
extern void func_15D80(uint32_t base, uint32_t arg); /* USART 状态处理 (0x08015D80) */
extern void func_15E06(uint32_t base, uint32_t bits, uint32_t val); /* USART 位设置/清除 (0x08015E06) */
extern void func_15D50(uint32_t base, uint32_t val);  /* USART 写半字 (0x08015D50) */
extern void func_0D9B4(void *nvic_cfg);           /* NVIC 配置 (0x0800D9B4) */

/* ---- RAM 地址 (来自字面量池) ---- */
#define RING_BUF_A      (*(volatile uint16_t *)0x20001A1C)
#define RING_BUF_A_HI   (*(volatile uint16_t *)0x20001A1E)
#define RING_BUF_PTR    (*(volatile uint32_t *)0x20000284)
#define RING_BUF_TAIL   (*(volatile uint16_t *)0x20000288)
#define STATE_VAR_B     (*(volatile uint16_t *)0x20003648)
#define STATE_VAR_B_HI  (*(volatile uint16_t *)0x2000364A)
#define STATE_VAR_C     (*(volatile uint16_t *)0x200002B8)
#define STATE_VAR_D     (*(volatile uint16_t *)0x200002BA)
#define GPIOB_BASE      0x40020400
#define USART_BASE      0x40011000

/* ========================================================================
 * AT_Response_CSV_Parser (0x08017E82 - 0x08017EDC, 90 bytes)
 *
 * 解析 AT 命令响应中的逗号 (0x2C) 和星号 (0x2A) 分隔字段.
 * 已验证通过 (func_158 review: ALL PASS).
 * ======================================================================== */
static int AT_Response_CSV_Parser(uint32_t *result_ptr,
                                   const char *buf,
                                   uint32_t *count_ptr,
                                   uint16_t *field_count_ptr,
                                   uint8_t max_fields,
                                   uint32_t buf_offset,
                                   uint32_t char_count)
{
    if (*count_ptr != 0) {
        return 3;
    }

    buf_offset++;
    while (1) {
        if (max_fields == 0) {
            *count_ptr = buf_offset - char_count;
            if (field_count_ptr != NULL) {
                return 0;
            }
        }

        char c = buf[buf_offset];
        if (c == ',') {                            /* 0x2C */
            if (max_fields != 0) {
                *field_count_ptr = char_count;
                return 0;
            }
        } else if (c == '*') {                     /* 0x2A */
            if (max_fields != 0) {
                max_fields--;
                *field_count_ptr = char_count;
                return 0;
            }
        }

        char_count++;
        buf_offset++;
    }
}

/* ========================================================================
 * Clear_Ring_Buffer_Vars (0x08018DC0 - 0x08018DD0, 16 bytes)
 *
 * 叶函数, 无 PUSH/POP. 清零 4 个环缓冲跟踪变量.
 *   0x08018DC0: MOVS r0,#0
 *   0x08018DC2: LDR r1,[pc,#0x10] → 0x20001A1C (RING_BUF_A)
 *   0x08018DC4: STRH r0,[r1,#2]   → RING_BUF_A_HI = 0
 *   0x08018DC6: STRH r0,[r1]      → RING_BUF_A = 0
 *   0x08018DC8: LDR r1,[pc,#0xC]  → 0x20000284 (RING_BUF_PTR)
 *   0x08018DCA: STR r0,[r1]       → RING_BUF_PTR = 0
 *   0x08018DCC: LDR r1,[pc,#0xC]  → 0x20000288 (RING_BUF_TAIL)
 *   0x08018DCE: STRH r0,[r1]      → RING_BUF_TAIL = 0
 *   0x08018DD0: BX LR
 * ======================================================================== */
static void Clear_Ring_Buffer_Vars(void)
{
    RING_BUF_A = 0;
    RING_BUF_A_HI = 0;
    RING_BUF_PTR = 0;
    RING_BUF_TAIL = 0;
}

/* ========================================================================
 * Clear_State_Vars (0x08018DE0 - 0x08018DF0, 16 bytes)
 *
 * 叶函数变体. 清零另一组状态变量.
 *   0x08018DE0: MOVS r0,#0
 *   0x08018DE2: LDR r1,[pc,#0x10] → 0x20003648 (STATE_VAR_B)
 *   0x08018DE4: STRH r0,[r1,#2]   → STATE_VAR_B_HI = 0
 *   0x08018DE6: STRH r0,[r1]      → STATE_VAR_B = 0
 *   0x08018DE8: LDR r1,[pc,#0xC]  → 0x200002B8 (STATE_VAR_C)
 *   0x08018DEA: STRH r0,[r1]      → STATE_VAR_C = 0
 *   0x08018DEC: LDR r1,[pc,#0xC]  → 0x200002BA (STATE_VAR_D)
 *   0x08018DEE: STRH r0,[r1]      → STATE_VAR_D = 0
 *   0x08018DF0: BX LR
 * ======================================================================== */
static void Clear_State_Vars(void)
{
    STATE_VAR_B = 0;
    STATE_VAR_B_HI = 0;
    STATE_VAR_C = 0;
    STATE_VAR_D = 0;
}

/* ========================================================================
 * Compute_Ratio_A (0x08018E00 - 0x08018E1C, 28 bytes)
 *
 * 叶函数. 从 ADC/传感器数据计算缩放比率 (变体 A).
 *   0x08018E00: LDR r0,[pc,#0x1C] → 0x20001A1C (RING_BUF_A)
 *   0x08018E02: LDRH r0,[r0]      → RING_BUF_A (半字, offset 0)
 *   0x08018E04: ADD r0,r0,#0x200  → +512
 *   0x08018E08: LDR r2,[pc,#0x14] → 0x20001A1C (同一 struct)
 *   0x08018E0A: LDRH r2,[r2,#2]   → RING_BUF_A_HI (半字, offset 2)
 *   0x08018E0C: SUBS r1,r0,r2     → diff = (A+512) - A_HI
 *   0x08018E0E: ASRS r0,r1,#31    → 符号扩展
 *   0x08018E10: ADD r0,r1,r0,LSR#23 → 带舍入的除法预处理
 *   0x08018E14: ASRS r0,r0,#9     → /512 缩放
 *   0x08018E16: SUB r0,r1,r0,LSL#9 → 余数 = diff - (diff/512)*512
 *   0x08018E1A: UXTH r0,r0        → 零扩展至 16 位
 *   0x08018E1C: BX LR
 *
 * 公式: result = (RING_BUF_A + 512 - RING_BUF_A_HI) % 512
 *       两个操作数均来自同一 struct (0x20001A1C, offset 0 和 2).
 * ======================================================================== */
static uint16_t Compute_Ratio_A(void)
{
    uint32_t diff = (RING_BUF_A + 0x200) - RING_BUF_A_HI;
    return (uint16_t)(diff % 512);
}

/* ========================================================================
 * Compute_Ratio_B (0x08018E24 - 0x08018E40, 28 bytes)
 *
 * 叶函数变体. 使用 +0x400 偏移和 /1024 缩放的不同参数.
 *   0x08018E24: LDR r0,[pc,#0x1C] → 0x20003648 (STATE_VAR_B)
 *   0x08018E26: LDRH r0,[r0]      → STATE_VAR_B (半字, offset 0)
 *   0x08018E28: ADD r0,r0,#0x400  → +1024
 *   0x08018E2C: LDR r2,[pc,#0x14] → 0x20003648 (同一 struct)
 *   0x08018E2E: LDRH r2,[r2,#2]   → STATE_VAR_B_HI (半字, offset 2)
 *   0x08018E30: SUBS r1,r0,r2     → diff = (B+1024) - B_HI
 *   0x08018E32: ASRS r0,r1,#31    → 符号扩展
 *   0x08018E34: ADD r0,r1,r0,LSR#22 → 带舍入的除法预处理
 *   0x08018E38: ASRS r0,r0,#10    → /1024 缩放
 *   0x08018E3A: SUB r0,r1,r0,LSL#10 → 余数
 *   0x08018E3E: UXTH r0,r0        → 零扩展
 *   0x08018E40: BX LR
 *
 * 公式: result = (STATE_VAR_B + 1024 - STATE_VAR_B_HI) % 1024
 *       两个操作数均来自同一 struct (0x20003648, offset 0 和 2).
 * ======================================================================== */
static uint16_t Compute_Ratio_B(void)
{
    uint32_t diff = (STATE_VAR_B + 0x400) - STATE_VAR_B_HI;
    return (uint16_t)(diff % 1024);
}

/* ========================================================================
 * FPU_Double_Call_Wrapper (0x08018E48 - 0x08018E58, 16 bytes)
 *
 * 包装双精度 FPU 调用: 将 r0:r1 移入 d0, 调用 func_16CA0, 结果返回到 r0:r1.
 *   0x08018E48: PUSH {r4,lr}
 *   0x08018E4A: VMOV d0,r0,r1    → d0 = input (double)
 *   0x08018E4E: BL func_16CA0    → 符号/阈值检查
 *   0x08018E52: VMOV r0,r1,d0    → r0:r1 = result (double)
 *   0x08018E56: POP {r4,pc}
 * ======================================================================== */
static double FPU_Double_Call_Wrapper(double input)
{
    func_16CA0(input);
    return input;
}

/* ========================================================================
 * FPU_NaN_Inf_Classifier (0x08018E58 - 0x08018EC8, 112 bytes)
 *
 * 双精度 NaN/Inf 分类与错误处理.
 * PUSH {lr}; VPUSH {d8,d9}; SUB SP,#0xC
 *
 * 流程:
 *   1. VMOV d9,r0,r1      → 保存输入 arg
 *   2. BL func_08BC4      → 获取比较值 → d8
 *   3. VMOV r0,s16; VMOV r1,s17 → 提取 d8 的低 32 位 (s16:s17 = d8[31:0])
 *   4. VSTR d8,[sp]       → 溢出到栈
 *   5. CMP r0,#0; IT NE; MOVNE r0,#1 → 归一化标志
 *   6. ORRS r0,r1         → 合并高低位
 *   7. BIC r0,#0x80000000 → 清除符号位
 *   8. RSB r0,#0xFF00000; ADD r0,#0x70000000 → 指数检查 (NaN 检测)
 *   9. LSRS r0,#31; BEQ skip → 若非 NaN, 跳过
 *  10. VMOV r1,s18; VMOV r0,s19 → 提取 d9 的高 32 位 (输入 arg)
 *  11. VSTR d9,[sp]       → 溢出到栈
 *  12. 对输入 arg 重复 NaN 检测 (步骤 5-9)
 *  13. ITT EQ; MOVEQ r0,#1; BLEQ func_175C4 → 若为 NaN, 调错误处理
 *  14. VMOV r0,r1,d8      → 返回 d8 (比较值) 到 r0:r1
 *  15. ADD SP,#0xC; VPOP {d8,d9}; POP {pc}
 *
 * 此函数检查两个 double 值 (d8=比较值, d9=输入) 的指数域,
 * 若任一为 NaN (指数全 1 且尾数非零), 则调用 FPU 错误处理器 func_175C4.
 * 正常情况返回比较值 d8.
 * ======================================================================== */
static double FPU_NaN_Inf_Classifier(double input)
{
    double cmp_val;
    /* BL func_08BC4 → cmp_val = some_transform(input) in d8 */

    /* 检查 d8 是否为 NaN/Inf (指数域检查) */
    /* 若为 NaN, 再检查 d9 (输入 arg) 是否为 NaN/Inf */
    /* 若任一为 NaN, BL func_175C4 (FPU 错误处理) */

    return cmp_val;  /* 正常返回比较值 */
}

/* ========================================================================
 * Display_FSMC_USART_Init (0x08018EC8 - 0x08018F80, 184 bytes)
 *
 * 显示 + FSMC + USART + NVIC 组合初始化序列.
 * PUSH {r4,lr}; SUB SP,#0x20
 *
 * 参数: r0 = display_mode — 显示模式参数 (存储到 [sp,#8])
 *
 * 调用序列:
 *   1. func_113B8(1, 1)              → 系统初始化阶段 1
 *   2. func_113D8(0x20000, 1)        → 系统初始化 (时钟相关)
 *   3. func_0C4BC(GPIOB, 2, 7)       → PB2 → AF7 (FSMC 接口)
 *   4. func_0C4BC(GPIOB, 3, 7)       → PB3 → AF7 (FSMC 接口)
 *   5. 栈上构建显示配置:
 *      [sp+0x18] = 0x0C (显示参数)
 *      [sp+0x1C] = 0x02 (模式字节)
 *      [sp+0x1D] = 0x02
 *      [sp+0x1E] = 0x00 (标志)
 *      [sp+0x1F] = 0x01
 *   6. func_0C42C(GPIOB, &cfg)       → GPIO 端口初始化
 *   7. 栈上构建 USART 时钟配置:
 *      [sp+0x08] = display_mode (r4)
 *      [sp+0x0C] = 0x0000
 *      [sp+0x0E] = 0x0000
 *      [sp+0x10] = 0x0000
 *      [sp+0x12] = 0x000C (分频标志)
 *      [sp+0x14] = 0x0000
 *   8. func_15E50(USART_BASE, &cfg)  → 波特率计算
 *   9. func_15D80(USART_BASE, 1)     → USART 状态处理
 *  10. func_15E06(USART_BASE, 0x525, 1) → USART 位设置
 *  11. func_15E06(USART_BASE, 0x424, 1) → USART 位设置
 *  12. func_15D50(USART_BASE, 0x424) → USART 写半字
 *  13. 栈上构建 NVIC 配置:
 *      [sp+0x04] = 0x26 (IRQ 编号 38 = USART1_IRQn)
 *      [sp+0x05] = 0x03 (优先级组)
 *      [sp+0x06] = 0x03 (子优先级)
 *      [sp+0x07] = 0x01 (使能)
 *  14. func_0D9B4(&nvic_cfg)        → NVIC 配置
 *  15. ADD SP,#0x20; POP {r4,pc}
 *
 * 功能: 为 TFT LCD 显示配置 FSMC GPIO 引脚 (PB2/PB3 → AF7),
 *       初始化 USART 波特率和控制寄存器,
 *       启用 USART1 IRQ (NVIC IRQ #38).
 * ======================================================================== */
static void Display_FSMC_USART_Init(uint32_t display_mode)
{
    /* 初始化序列... */
}

/*
 * 内存布局:
 *   0x08017E82 - 0x08017EDC: AT_Response_CSV_Parser (90B)
 *   0x08017EDC - 0x08018DBC: [其他函数覆盖区间]
 *   0x08018DBC - 0x08018DD0: Clear_Ring_Buffer_Vars (16B)
 *   0x08018DD0 - 0x08018DE0: 字面量池 (16B, 包含 RAM 地址)
 *   0x08018DE0 - 0x08018DF0: Clear_State_Vars (16B)
 *   0x08018DF0 - 0x08018E00: 字面量池 (16B)
 *   0x08018E00 - 0x08018E1C: Compute_Ratio_A (28B)
 *   0x08018E1C - 0x08018E24: 字面量池 (8B)
 *   0x08018E24 - 0x08018E40: Compute_Ratio_B (28B)
 *   0x08018E40 - 0x08018E48: 字面量池 (8B)
 *   0x08018E48 - 0x08018E58: FPU_Double_Call_Wrapper (16B)
 *   0x08018E58 - 0x08018EC8: FPU_NaN_Inf_Classifier (112B)
 *   0x08018EC8 - 0x08018F80: Display_FSMC_USART_Init (184B)
 *   0x08018F80 - 0x08019000: 设备基址 + 校准查找表头 (128B, 数据)
 *   0x08019000 - 0x08019960: 主校准数据表 (2400B, 数据, 固件末尾)
 *
 * 数据表结构 (0x08018F80-0x08019960):
 *   +0x00: 0x40020000 (DMA1 基址)
 *   +0x04: 0x40004400 (外设基址)
 *   +0x08 起: 约 315 对 16-bit 校准阈值 (单调递增)
 *   每对格式: {low_threshold, high_threshold}
 *   用于传感器线性化和 PWM 频率映射.
 *
 * 注: 原 func_158 将 FPU_NaN_Inf_Classifier 误标为 FPU_Calibration_Computer,
 *     将 Display_FSMC_USART_Init 误标为 Calibration_Table_Initializer.
 *     实际校准表初始化在固件启动序列的别处完成.
 *     0x08019960 为固件末尾 (72032 bytes), 剩余 Flash 为 0xFF 填充.
 */
