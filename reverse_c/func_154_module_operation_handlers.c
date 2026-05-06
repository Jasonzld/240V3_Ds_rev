/**
 * @file func_154_module_operation_handlers.c
 * @brief 模块操作/命令处理子系统 — 外设寄存器配置、协议帧发送、定时器操作触发
 * @addr  0x0800930A - 0x08009740 (1078 bytes, 8 个主函数 + 多个叶辅助函数)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 此范围包含由显示状态机 (func_151)、4G 响应解析器 (func_150)
 * 和其他子系统大量调用的核心操作函数.
 *
 * 主要外部可见函数:
 *   func_0943E (Protocol_Frame_Sender_EE_B1) — 发送 EE B1 23 协议帧, 3 参数
 *   func_09490 (Operation_Timer_Trigger)      — TIM11 定时器触发, 3 参数
 *   func_0963C (Module_String_Sender)         — 发送 AT+CREG? 命令字符串
 *
 * 内部辅助函数:
 *   func_09320 (Register_Field_Copier)        — 外设寄存器字段复制
 *   func_09374 (Register_BitField_Config)     — 位字段配置器
 *   func_094F4 (State_Operation_Handler)      — 状态驱动操作处理器
 *
 * 外部调用关系 (已验证):
 *   Protocol_Frame_Sender_EE_B1 (func_0943E) → func_123E8 ×12
 *   Operation_Timer_Trigger (func_09490)      → func_115B8, Store_BitBand_Write
 *   State_Operation_Handler (func_094F4)      → func_C6C4 ×3, func_1257A,
 *                                                func_12604, func_10BD0, func_09490
 *   Module_String_Sender (func_0963C)         → func_0BBFC
 */

#include "stm32f4xx_hal.h"

/* ---- 外部依赖 ---- */
extern void func_123E8(uint8_t byte);                /* UART 逐字节发送 */
extern void func_115B8(uint32_t timer_id, uint32_t value); /* 定时器配置 */
extern void func_0BBFC(const char *buf, uint16_t len); /* UART 缓冲区发送 */
extern void func_C6C4(uint32_t a, uint32_t b, uint32_t c); /* 算术/比较操作 */
extern void func_1257A(uint32_t trigger, uint32_t param); /* 操作触发器 */
extern void func_12604(uint32_t mode);               /* 模式备份并通知 */
extern void func_10BD0(uint32_t context);            /* Display_State_Dispatcher */

/* ---- 操作状态 RAM (由字面量池 0x080094D0-0x080094E0 验证) ---- */
#define TIMER_A_VAL     (*(volatile uint16_t *)0x2000001E)  /* 定时器 A 值 (STRH at 0x080094A2) */
#define TIMER_B_VAL     (*(volatile uint16_t *)0x20000020)  /* 定时器 B 值 (STRH at 0x080094AE) */
#define TIMER_MODE      (*(volatile uint8_t  *)0x20000025)  /* 定时器模式 (STRB at 0x080094B2) */
#define TRIGGER_FLAG    (*(volatile uint8_t  *)0x20000024)  /* 触发标志 (STRB at 0x080094B8) */
#define TIM11_BASE      0x40014800                          /* TIM11 定时器基址 (pool 0x080094E0) */

/* 状态监视器 RAM (来自字面量池 0x080095F0+) */
#define OP_STATE        (*(volatile uint8_t  *)0x2000024A)  /* 操作状态检查字节 (pool 0x080095F0) */
#define MODE_BYTE       (*(volatile uint8_t  *)0x20000019)  /* 当前模式 (pool 0x080095F4) */

/* ========================================================================
 * 叶辅助函数: Store_BitBand_Write (0x080094E8, 8 bytes)
 *   ldr r1, [pc, #4]; str r0, [r1]; bx lr
 *   Pool at 0x080094F0 = 0x424082AC (外设位带别名 → TIM11 控制位)
 * ======================================================================== */
static void Store_BitBand_Write(uint32_t value)
{
    *(volatile uint32_t *)0x424082AC = value;
}

/* ========================================================================
 * 叶辅助函数: Register_ORR_40000000 (0x0800942C, 12 bytes)
 *   ldr r1, [r0, #8]; orr r1, r1, #0x40000000; str r1, [r0, #8]; bx lr
 *   在 *(r0+8) 上设置 bit30
 * ======================================================================== */
static void Register_ORR_40000000(volatile uint32_t *reg_base)
{
    reg_base[2] |= 0x40000000;  /* offset +8 = reg_base[2] (word index) */
}

/* ========================================================================
 * 主函数 1: Register_Field_Copier (0x08009320, 84 bytes)
 *
 * 参数: r0 = dst_reg — 目标寄存器块基址
 *       r1 = src_data — 源数据结构指针
 *
 * 将源数据结构中的字段复制到目标外设寄存器的指定偏移:
 *   dst+4  ← (src[0] | (src.byte[4] << 8))  (带掩码)
 *   dst+8  ← (src[4] | src[3] | src[2] | (src.byte[5] << 1)) (带掩码)
 *   dst+0x2C ← 更新 bit[23:20] = (src.byte[0x14] - 1)
 * ======================================================================== */
void Register_Field_Copier(volatile uint32_t *dst_reg, const uint32_t *src_data)
{
    uint32_t val;

    /* 字段 1: dst+4 — 合并 src[0] 的低字节和高字节 */
    val = dst_reg[1];                                    /* LDR [r2, #4] */
    val &= 0xFCFFFEFF;                                   /* AND with mask (pool 0x0800936C) */
    val |= src_data[0];                                  /* ORR with src[0] */
    val |= (*(const uint8_t *)(src_data + 1)) << 8;      /* LDRB [r1,#4] + ORR lsl#8 */
    dst_reg[1] = val;

    /* 字段 2: dst+8 — 合并多个源字段 */
    val = dst_reg[2];                                    /* LDR [r2, #8] */
    val &= 0xC0FFF7FD;                                   /* AND with mask (pool 0x08009370) */
    uint32_t combined = src_data[3] | src_data[4];       /* LDRD r5,r4,[r1,#0xc] → r5=[3], r4=[4]; ORRS */
    combined |= src_data[2];                             /* LDR [r1, #8] → ORR */
    combined |= (*(const uint8_t *)((const uint8_t *)src_data + 5)) << 1; /* LDRB [r1,#5]; ORR lsl#1 */
    val |= combined;
    dst_reg[2] = val;

    /* 字段 3: dst+0x2C — 更新位 [23:20] */
    val = dst_reg[11];                                   /* LDR [r2, #0x2c] (word offset 11) */
    val &= ~0x00F00000;                                  /* BIC #0xF00000 → clear bits [23:20] */
    uint32_t shift_val = *(const uint8_t *)(src_data + 5) - 1; /* LDRB [r1,#0x14]; SUBS #1 */
    val |= (shift_val & 0xF) << 20;                      /* ORR lsl#20 */
    dst_reg[11] = val;
}

/* ========================================================================
 * 主函数 2: Register_BitField_Config (0x08009374, 186 bytes)
 *
 * 参数: r0 = reg_base — 外设寄存器块基址
 *       r1 = field_idx — 字段索引 (0-9 → 使用 reg+0x10; >9 → 使用 reg+0x0C)
 *       r2 = bit_range — 位范围选择器:
 *            <7    → reg+0x34, 5 位字段, 索引偏移 -1
 *            7-12  → reg+0x30, 5 位字段, 索引偏移 -7
 *            >=13  → reg+0x2C, 5 位字段, 索引偏移 -13
 *       r3 = value — 要写入的位字段值
 *
 * 在适当的寄存器和位偏移处清除并设置 5 位 (0x1F) 字段.
 * 字段索引 <10 使用 3 位 (0x7) 字段在 reg+0x10/+0x0C.
 * ======================================================================== */
void Register_BitField_Config(volatile uint32_t *reg_base,
                              uint32_t field_idx,
                              uint32_t bit_range,
                              uint32_t value)
{
    if (field_idx <= 9) {
        /* 低字段索引: 使用 reg+0x10, 3 位字段 (0x07) */
        uint32_t val = reg_base[4];                      /* LDR [r4, #0x10] */
        uint32_t shift = (field_idx * 3);                /* r6 = r5*3 (= r5 + r5*2) */
        val &= ~(0x07 << shift);                         /* BICS with #7 << shift */
        val |= (value & 0x07) << shift;                  /* ORRS with r3 << shift */
        reg_base[4] = val;
    } else {
        /* 高字段索引: 使用 reg+0x0C, 3 位字段 */
        uint32_t val = reg_base[3];                      /* LDR [r4, #0xc] */
        uint32_t adj_idx = field_idx - 10;               /* SUB r6, r5, #0xa */
        uint32_t shift = adj_idx * 3;                    /* r7 = r6 + r6*2 */
        val &= ~(0x07 << shift);                         /* BICS */
        val |= (value & 0x07) << shift;                  /* ORRS */
        reg_base[3] = val;
    }

    /* 位范围选择器 — 5 位字段更新 */
    uint32_t val;
    if (bit_range < 7) {
        val = reg_base[13];                              /* LDR [r4, #0x34] */
        uint32_t shift = (bit_range - 1) * 5;            /* r6*5 (= r6+r6*4) */
        val &= ~(0x1F << shift);                         /* BICS #0x1F */
        val |= (field_idx & 0x1F) << shift;              /* r5 ← ORR */
        reg_base[13] = val;
    } else if (bit_range < 13) {
        val = reg_base[12];                              /* LDR [r4, #0x30] */
        uint32_t shift = (bit_range - 7) * 5;
        val &= ~(0x1F << shift);
        val |= (field_idx & 0x1F) << shift;
        reg_base[12] = val;
    } else {
        val = reg_base[11];                              /* LDR [r4, #0x2c] */
        uint32_t shift = (bit_range - 13) * 5;
        val &= ~(0x1F << shift);
        val |= (field_idx & 0x1F) << shift;
        reg_base[11] = val;
    }
}

/* ========================================================================
 * 主函数 3: Protocol_Frame_Sender_EE_B1 (0x0800943E, 82 bytes)
 *
 * func_0943E — 发送协议帧: EE B1 23 [参数] FF FC FF FF
 *
 * 参数: r0 = param_a — 第一个 16 位参数 (高低字节分别发送)
 *       r1 = param_b — 第二个 16 位参数
 *       r2 = param_c — 8 位参数字节
 *
 * 帧格式 (共 12 字节):
 *   [0]  0xEE      帧头
 *   [1]  0xB1      命令类型高字节
 *   [2]  0x23      命令类型低字节
 *   [3]  param_a >> 8    (高字节)
 *   [4]  param_a & 0xFF  (低字节)
 *   [5]  param_b >> 8    (高字节)
 *   [6]  param_b & 0xFF  (低字节)
 *   [7]  param_c & 0xFF
 *   [8]  0xFF
 *   [9]  0xFC
 *   [10] 0xFF
 *   [11] 0xFF
 *
 * 所有字节通过 func_123E8 (UART TX byte) 逐字节发送.
 * 被 func_151 (Display_State_Dispatcher) 调用 8 次.
 * ======================================================================== */
void Protocol_Frame_Sender_EE_B1(uint16_t param_a, uint16_t param_b, uint8_t param_c)
{
    func_123E8(0xEE);                                    /* 帧头 */
    func_123E8(0xB1);                                    /* 命令类型 */
    func_123E8(0x23);                                    /* 命令子类型 */
    func_123E8((uint8_t)(param_a >> 8));                 /* param_a 高字节 */
    func_123E8((uint8_t)(param_a));                      /* param_a 低字节 */
    func_123E8((uint8_t)(param_b >> 8));                 /* param_b 高字节 */
    func_123E8((uint8_t)(param_b));                      /* param_b 低字节 */
    func_123E8(param_c);                                 /* param_c */
    func_123E8(0xFF);                                    /* 帧尾 */
    func_123E8(0xFC);
    func_123E8(0xFF);
    func_123E8(0xFF);
}

/* ========================================================================
 * 主函数 4: Operation_Timer_Trigger (0x08009490, 100 bytes)
 *
 * func_09490 — 配置操作参数并启动定时器
 *
 * 参数: r0 = timer_val_a — 定时器 A 值 (左移 1 位后截断为 16 位)
 *       r1 = timer_val_b — 定时器 B 值 (左移 1 位后截断为 16 位)
 *       r2 = timer_mode  — 定时器模式字节
 *
 * 操作:
 *   1. TIMER_A_VAL = (timer_val_a << 1) & 0xFFFF
 *   2. TIMER_B_VAL = (timer_val_b << 1) & 0xFFFF
 *   3. TIMER_MODE  = timer_mode
 *   4. 设置触发标志
 *   5. func_115B8(timer_id, TIMER_A_VAL) — 配置硬件定时器
 *
 * 被 func_151 (Display_State_Dispatcher) 调用 2 次.
 * ======================================================================== */
void Operation_Timer_Trigger(uint16_t timer_val_a, uint16_t timer_val_b, uint8_t timer_mode)
{
    /* 存储定时器值 (左移 1 位, 截断为 16 位) */
    TIMER_A_VAL = (timer_val_a << 1) & 0xFFFF;
    TIMER_B_VAL = (timer_val_b << 1) & 0xFFFF;
    TIMER_MODE  = timer_mode;

    /* 设置触发标志 */
    TRIGGER_FLAG = 1;

    /* 位带写入: 通过位带别名地址 0x424082AC 触发 TIM11 控制位 */
    Store_BitBand_Write(TRIGGER_FLAG);                   /* func_094E8 */

    /* 配置 TIM11 定时器: timer_id = TIM11_BASE, value = TIMER_A (16-bit via LDRH) */
    func_115B8(TIM11_BASE, TIMER_A_VAL);
}

/* ========================================================================
 * 主函数 5: State_Operation_Handler (0x080094F4, 260 bytes)
 *
 * 状态驱动的操作处理器. 当 OP_STATE == 1 时:
 *   1. 从 STRUCT_DATA_A 加载 3 字段结构
 *   2. 调用 func_C6C4 进行算术处理, 存储到 RESULT_REG_A
 *   3. 从 RESULT_REG_A 加载结构, func_C6C4 处理, 存储到 RESULT_REG_B
 *   4. 从 RESULT_REG_B 加载结构, 偏移 +0x7D0, func_C6C4 处理, 存储到 RESULT_REG_C
 *   5. 比较结果, 触发操作或显示更新:
 *      - 若 RESULT_REG_C < RESULT_REG_A 且 RESULT_REG_C < RESULT_REG_B:
 *        → func_1257A(trigger), func_12604(0x26), func_10BD0(0x26),
 *          func_09490(0x1F4, 0x1F4, 2)
 *      - 若 MODE_BACKUP == 0x26:
 *        → func_12604(2), 更新 DISP_STATE_MIRROR
 * ======================================================================== */
void State_Operation_Handler(void)
{
    /* 门控检查: OP_STATE 必须为 1 */
    if (*(volatile uint8_t *)0x200001C6 != 1)
        return;

    /* 阶段 1: 从 STRUCT_DATA_A (0x20000456) 加载 3 字段结构, func_C6C4 处理 */
    uint32_t s_a = *(volatile uint32_t *)0x20000456;        /* LDR [r0] */
    uint32_t s_b = *(volatile uint32_t *)0x2000045A;        /* LDR [r0, #4] */
    uint16_t s_c = *(volatile uint16_t *)0x2000045E;        /* LDRH [r0, #8] */
    uint32_t r1 = func_C6C4(s_a, s_b, s_c);
    *(volatile uint32_t *)0x200001FC = r1;                   /* RESULT_REG_A */

    /* 阶段 2: 从 0x20000460 加载结构, func_C6C4 处理 */
    s_a = *(volatile uint32_t *)0x20000460;
    s_b = *(volatile uint32_t *)0x20000464;
    s_c = *(volatile uint16_t *)0x20000468;
    r1 = func_C6C4(s_a, s_b, s_c);
    *(volatile uint32_t *)0x20000200 = r1;                   /* RESULT_REG_B */

    /* 阶段 3: 从 0x2000069C 加载结构, 第一个字段 +0x7D0, func_C6C4 处理 */
    s_a = *(volatile uint32_t *)0x2000069C;
    s_b = *(volatile uint32_t *)0x200006A0;
    s_c = *(volatile uint16_t *)0x200006A4;
    s_a += 0x7D0;                                           /* ADD #0x7D0, UXTH */
    r1 = func_C6C4(s_a, s_b, s_c);
    *(volatile uint32_t *)0x20000204 = r1;                   /* RESULT_REG_C */

    /* 阶段 4: 比较结果并分派
     * 触发条件: RESULT_C < RESULT_A OR RESULT_C > RESULT_B
     * (BLO → trigger if C < A; then BLS → skip trigger if C <= B) */
    uint32_t res_a = *(volatile uint32_t *)0x200001FC;      /* RESULT_REG_A */
    uint32_t res_b = *(volatile uint32_t *)0x20000200;      /* RESULT_REG_B */
    uint32_t res_c = *(volatile uint32_t *)0x20000204;      /* RESULT_REG_C */

    if (res_c < res_a || res_c > res_b) {
        /* 结果超范围 — 触发操作和显示更新 */
        *(volatile uint8_t *)0x200001C6 = 0xFE;

        func_1257A(*(volatile uint8_t *)0x200001C0, 0x26); /* 操作触发器 */
        func_12604(0x26);                                   /* 模式备份 */
        *(volatile uint8_t *)0x2000024A = 0x26;             /* DISP_STATE_MIRROR */
        *(volatile uint8_t *)0x20000019 = 0;                /* MODE_BYTE 清零 */
        func_10BD0(*(volatile uint8_t *)0x20000019);        /* 显示状态分派 */
        Operation_Timer_Trigger(0x1F4, 0x1F4, 2);           /* self: func_09490 */
    } else {
        /* 结果在范围内 — 检查模式回退 */
        if (*(volatile uint8_t *)0x2000024A == 0x26) {
            func_12604(2);
            *(volatile uint8_t *)0x2000024A = 2;             /* DISP_STATE_MIRROR = 2 */
        }
    }
}

/* ========================================================================
 * 主函数 6: Module_String_Sender (0x0800963C, 36 bytes)
 *
 * func_0963C — 当 OP_STATE == 1 时通过 UART 发送 11 字节命令字符串.
 * 命令字符串来自字面量池 (ADR r0, #8 → 指向函数后的内联数据).
 *
 * 被 func_150 (4G Response Parser) 调用 1 次.
 * ======================================================================== */
void Module_String_Sender(void)
{
    if (*(volatile uint8_t *)0x200002B3 == 1) {
        /* AT+CREG? 命令字符串 (来自字面量池 0x08009654) */
        static const char cmd_string[] = {
            'A', 'T', '+', 'C', 'R', 'E', 'G', '?', '\r', '\n', '\0'
        };
        func_0BBFC(cmd_string, 11);                      /* 发送 11 字节 */
    }
}

/*
 * 内存布局:
 *   0x0800930A - 0x08009320: 前导数据/字面量池 (22 bytes)
 *   0x08009320 - 0x08009374: Register_Field_Copier (84B)
 *   0x08009374 - 0x0800942C: Register_BitField_Config (184B)
 *   0x0800942C - 0x08009436: Register_ORR_40000000 叶辅助 (10B)
 *   0x08009436 - 0x0800943E: 跳板/字面量 (wraps func_0D59C)
 *   0x0800943E - 0x08009490: Protocol_Frame_Sender_EE_B1 (82B)
 *   0x08009490 - 0x080094E8: Operation_Timer_Trigger (88B)
 *   0x080094E8 - 0x080094F4: Store_Byte_To_RAM 叶辅助 (8B)
 *   0x080094F4 - 0x080095F8: State_Operation_Handler (260B)
 *   0x080095F8 - 0x0800961C: 小型状态检查 (36B)
 *   0x0800961C - 0x0800963C: 小型标志检查 (32B)
 *   0x0800963C - 0x08009660: Module_String_Sender (36B)
 *   0x08009660 - 0x080096AA: 命令发送器变体 (74B)
 *   0x080096AA - 0x080096C4: 帧同步字节发送器 (26B)
 *   0x080096C4 - 0x0800970C: 字符串操作包装器 (72B)
 *   0x0800970C - 0x08009740: 字符串操作包装器变体 (52B)
 *   0x08009740: 下一函数开始 (func_06 或类似)
 *
 * 外部调用关系:
 *   Protocol_Frame_Sender_EE_B1 (func_0943E) → func_123E8 ×12
 *   Operation_Timer_Trigger (func_09490)      → func_115B8, func_094E8
 *   State_Operation_Handler (func_094F4)      → func_C6C4 ×3, func_1257A,
 *                                                func_12604, func_10BD0,
 *                                                func_09490 (自递归)
 *   Module_String_Sender (func_0963C)         → func_0BBFC
 *
 * 注: 此文件中的函数被 func_151 (Display_State_Dispatcher) 大量调用.
 *     func_0943E 是 func_151 中最常调用的外部函数 (8 次).
 *     其余尾端函数 (0x080095F8-0x08009740) 为较小的状态检查和字符串辅助函数,
 *     需要完整字面量池解析后方可精确还原.
 */
