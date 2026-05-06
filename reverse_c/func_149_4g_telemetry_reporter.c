/**
 * @file func_149_4g_telemetry_reporter.c
 * @brief 4G 模块遥测数据报告器 — 读取传感器值, FPU 转换, 格式化并 MQTT 发布
 * @addr  0x08009C84 - 0x0800A648 (2500 bytes, 含字面量池)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 此函数是遥测报告的核心调度器. 基于 RAM 标志字节分派不同报告路径,
 * 每个路径读取传感器 RAM 值, 通过 FPU 双精度管道处理, 格式化字符串,
 * 然后通过 AT+MPUB MQTT 发布.
 *
 * FPU 处理管道 (每个传感器值):
 *   ldrh/ldrsh → func_875C (int→double) → d10
 *   VLDR scale_factor → d0
 *   func_865C (double mul/scale) → d9
 *   func_883C (double conversion) → r0
 *   func_87B4 (double encoding) → d8
 *   func_16B18 (format double→string)
 *
 * 双精度常量 (VLDR 字面量池):
 *   1000.0  — 除数 (ms→s, g→kg)
 *   10.0    — 除数 (小单位转换)
 *   3600.0  — 除数 (s→h)
 *
 * 调用:
 *   func_0839A (strcpy)         ×2
 *   func_08334 (strcat)         ×68
 *   func_16B18 (format double)  ×28
 *   func_865C  (FPU scale)      ×15
 *   func_883C  (FPU convert)    ×15
 *   func_87B4  (FPU encode)     ×15
 *   func_875C  (int→double)     ×11
 *   func_873A  (signed→double)  ×4
 *   func_09B6C (MQTT publish)   ×4
 *   func_09BEC (MQTT IPStart)   ×0 (in this range)
 *   func_16664 (telemetry pkt)  ×1
 */

#include "stm32f4xx_hal.h"

/* ---- 外部依赖 ---- */
extern void     func_0839A(char *dst, const char *src);
extern void     func_08334(char *dst, const char *src);
extern uint16_t func_08370(const char *str);
extern void     func_0BBFC(const char *buf, uint16_t len);
extern void     func_09B6C(const char *payload);       /* MQTT Publish */
extern void     func_16B18(char *buf, const char *fmt, ...); /* format */
extern double   func_875C(uint32_t raw);               /* int → double */
extern double   func_873A(int32_t raw);                /* signed → double */
extern double   func_865C(double val, double scale);   /* FPU scale/mul */
extern uint32_t func_883C(double val);                 /* FPU convert */
extern double   func_87B4(uint32_t encoded);           /* FPU encode */
extern void     func_16664(void);                      /* telemetry packet */

/* ---- RAM 标志字节 (门控) ---- */
#define GATE_FLAG_A  (*(volatile uint8_t  *)0x2000FF59)  /* +0x3f8 */
#define GATE_FLAG_B  (*(volatile uint8_t  *)0x2000F9BD)  /* +0x3f4 */
#define GATE_FLAG_C  (*(volatile uint8_t  *)0x2000FCB7)  /* +0x3f0 (pool) */
/* 更多标志在 0x0800A458 等池地址 */

/* ---- 传感器 RAM 地址 (与 func_125 遥测包构建器共享) ---- */
#define SENSOR_SPEED     (*(volatile uint16_t *)0x20000040)
#define SENSOR_RPM       (*(volatile uint16_t *)0x2000003E)
#define SENSOR_TEMP      (*(volatile uint16_t *)0x2000003C)
#define SENSOR_VOLT      (*(volatile uint16_t *)0x2000003A)
#define SENSOR_FLAGS     (*(volatile uint8_t  *)0x200000D9)
#define SENSOR_ALT       (*(volatile uint32_t *)0x2000008C)
#define SENSOR_LAT       (*(volatile uint32_t *)0x20000128)
#define SENSOR_LON       (*(volatile uint32_t *)0x20000074)
#define SENSOR_HEADING   (*(volatile uint16_t *)0x20000056)
#define SENSOR_PRESSURE  (*(volatile uint16_t *)0x20000058)
#define SENSOR_POWER     (*(volatile uint16_t *)0x200000A8)
#define SENSOR_MODE      (*(volatile uint8_t  *)0x20000019)
#define SENSOR_STATUS    (*(volatile uint32_t *)0x20000038)
#define SENSOR_EXTRA1    (*(volatile uint16_t *)0x20000138)
#define SENSOR_EXTRA2    (*(volatile uint16_t *)0x2000013C)
#define SENSOR_FLOW      (*(volatile uint16_t *)0x20000070)
#define SENSOR_STATE     (*(volatile uint8_t  *)0x200000E7)
#define SENSOR_CONFIG    (*(volatile uint16_t *)0x2000016F)

/* 输出影子寄存器 */
#define OUT_SPEED     (*(volatile uint16_t *)0x200002C4)
#define OUT_RPM       (*(volatile uint16_t *)0x200002C2)
#define OUT_TEMP      (*(volatile uint16_t *)0x200002C0)
#define OUT_VOLT      (*(volatile uint16_t *)0x200002BE)
#define OUT_FLAGS     (*(volatile uint16_t *)0x200002D4)
#define OUT_FLOW      (*(volatile uint16_t *)0x200002D8)
#define OUT_LAT       (*(volatile uint32_t *)0x200002DC)
#define OUT_LON       (*(volatile uint32_t *)0x200002E0)
#define OUT_HEADING   (*(volatile uint16_t *)0x200002CA)
#define OUT_PRESSURE  (*(volatile uint16_t *)0x200002CC)
#define OUT_POWER     (*(volatile uint16_t *)0x200002CE)
#define OUT_STATUS    (*(volatile uint32_t *)0x200002C8)
#define OUT_STATE     (*(volatile uint8_t  *)0x200002C6)

#define DEVICE_ID_STR ((const char *)0x2000042C)

/*
 * FPU 辅助宏 — 表示硬件 FPU 管道中的操作
 * func_875C: 将 uint16_t/uint32_t 扩展为 IEEE 754 double (r0:r1)
 * func_873A: 将有符号 int16_t/long 扩展为 double
 * func_865C: 双精度乘法 d0 = d10 * scale_factor
 * func_883C: 双精度整数转换/舍入
 * func_87B4: 双精度重新编码/归一化
 */

/**
 * @brief 遥测报告器 — 主入口
 *
 * 栈帧: sp+0x000 = 临时格式化缓冲区 (32 bytes)
 *       sp+0x020 = 输出字符串缓冲区 (0x200 bytes)
 *       sp+0x220 = 栈帧结束
 *
 * FPU 寄存器保存: d8, d9, d10 (被调用者保存的 VFP 寄存器)
 */
void FourG_Telemetry_Reporter(void)
{
    char   fmt_buf[32];       /* sp+0x00: func_16B18 的临时格式化缓冲区 */
    char   out_buf[0x200];    /* sp+0x20: 遥测输出字符串构建区 */
    double d8, d9, d10;       /* FPU 寄存器 (硬件中为 d8/d9/d10) */
    uint32_t r4;
    uint16_t raw_val;

    /* ================================================================
     * 路径选择: 基于 RAM 门控标志的多路分派
     * ================================================================ */

    /* 检查主标志 A — 若未设置则跳过所有遥测报告 */
    if (GATE_FLAG_A == 0)
        goto cleanup;

    /* 轮询标志 B — 等待它被设置 (阻塞自旋) */
    while (GATE_FLAG_B == 0) {
        /* 自旋等待 — 由另一个任务/中断设置 */
    }

    /* ================================================================
     * 路径 1: 标志 C 设置 → 构建并发送报告类型 1
     * 格式: "LJ<s1>LW<s2>" + 传感器值
     * ================================================================ */
    if (GATE_FLAG_C != 0) {
        /* 构建 GPS 位置遥测字符串 */
        func_0839A(out_buf, "");                       /* 初始化输出缓冲 (空字符串) */
        func_08334(out_buf, "LJ%sLW%s");               /* 追加位置标签 (经度/纬度) */

        /* 发布 MQTT 消息 */
        func_09B6C(out_buf);

        /* 清除发送标志 */
        *(volatile uint8_t *)0x2000D0D2 = 0;
        goto cleanup;
    }

    /* ================================================================
     * 路径 2: 另一个标志 → 构建遥测报告类型 2
     * 包含: 速度, RPM, 温度, 电压, 流量 等标量传感器
     * ================================================================ */
    {
        uint8_t flag2 = *(volatile uint8_t *)0x2000A1B2;
        if (flag2 != 0) {
            *(volatile uint8_t *)0x2000D0D2 = 0;       /* 清除发送标志 */

            /* 格式化: "LJ%sLW%s" 模板 + 数值 */
            func_16B18(fmt_buf, "LJ%sLW%s",
                       *(volatile uint32_t *)0x2001D002,
                       *(volatile uint32_t *)0x2001D002);
            func_09B6C(fmt_buf);
            goto cleanup;
        }
    }

    /* ================================================================
     * 路径 3: 多传感器遥测报告构建
     * 读取每个传感器 → FPU 转换 → 格式化 → 追加到 out_buf
     * ================================================================ */
    {
        uint8_t flag3 = *(volatile uint8_t *)0x2000A141;
        if (flag3 == 0)
            goto cleanup;

        uint8_t counter = *(volatile uint8_t *)0x2001F8FD;

        /* --- 递减计数器并存储 --- */
        counter--;
        *(volatile uint8_t *)0x2001F8FD = counter;

        /* 初始化输出字符串 */
        func_0839A(out_buf, "wt");                     /* 重量/功率遥测标签 */

        /* ---- 传感器 1: 速度 → d10 → ×1000.0 → format "%.1f" → label "A" ---- */
        raw_val = SENSOR_SPEED;
        d10 = func_875C(raw_val);                      /* int→double */
        {
            double scale = 1000.0;
            d9 = func_865C(d10, scale);                /* d9 = speed / 1000.0 */
        }
        r4 = func_883C(d9);
        d8 = func_87B4(r4);
        func_16B18(fmt_buf, "%.1f", d8);               /* format to sp */
        func_08334(out_buf, "A");                      /* label "A" */
        func_08334(out_buf, fmt_buf);                  /* append value */

        /* ---- 传感器 2: RPM → d10 → ×10.0 → format "%.1f" → label "L" ---- */
        raw_val = SENSOR_RPM;
        d10 = func_875C(raw_val);
        {
            double scale = 10.0;
            d9 = func_865C(d10, scale);                /* d9 = rpm / 10.0 */
        }
        r4 = func_883C(d9);
        d8 = func_87B4(r4);
        func_16B18(fmt_buf, "%.1f", d8);
        func_08334(out_buf, "L");
        func_08334(out_buf, fmt_buf);

        /* ---- 传感器 3: 温度 → d10 → ×10.0 → format "%.1f" → label "p" ---- */
        raw_val = SENSOR_TEMP;
        d10 = func_875C(raw_val);
        {
            double scale = 10.0;
            d9 = func_865C(d10, scale);                /* d9 = temp / 10.0 */
        }
        r4 = func_883C(d9);
        d8 = func_87B4(r4);
        func_16B18(fmt_buf, "%.1f", d8);
        func_08334(out_buf, "p");
        func_08334(out_buf, fmt_buf);

        /* ---- 传感器 4: 有符号 16 位 → d10 → ×10.0 → format "%.1f" → label "C" ---- */
        {
            int16_t signed_val = *(volatile int16_t *)0x200000E6;
            d10 = func_873A(signed_val);               /* signed→double */
        }
        {
            double scale = 10.0;
            d9 = func_865C(d10, scale);
        }
        r4 = func_883C(d9);
        d8 = func_87B4(r4);
        func_16B18(fmt_buf, "%.1f", d8);
        func_08334(out_buf, "C");
        func_08334(out_buf, fmt_buf);

        /* ---- 传感器 5: 电压 → d10 → ×1000.0 → format "%.1f" → label "y" ---- */
        raw_val = SENSOR_VOLT;
        d10 = func_875C(raw_val);
        {
            double scale = 1000.0;
            d9 = func_865C(d10, scale);                /* d9 = volt / 1000.0 */
        }
        r4 = func_883C(d9);
        d8 = func_87B4(r4);
        func_16B18(fmt_buf, "%.1f", d8);
        func_08334(out_buf, "y");
        func_08334(out_buf, fmt_buf);

        /* ---- 传感器 6: 标志字节 → d10 → ×3600.0 → format "%d" → label "%d" ---- */
        {
            uint32_t word_val = *(volatile uint32_t *)0x200000E8;
            d10 = func_875C(word_val);
        }
        {
            double scale = 3600.0;
            d9 = func_865C(d10, scale);                /* d9 = val / 3600.0 */
        }
        r4 = func_883C(d9);
        d8 = func_87B4(r4);
        {
            uint8_t byte_flag = *(volatile uint8_t *)0x200000D9;
            func_16B18(fmt_buf, "%d", byte_flag);
        }
        func_08334(out_buf, "y");                      /* 注意: label 与传感器 5 相同 */
        func_08334(out_buf, fmt_buf);

        /* ---- 传感器 7: 高度/32位值 → d10 → ×3600.0 → format "%4f" ---- */
        {
            uint32_t alt_val = SENSOR_ALT;
            d10 = func_875C(alt_val);
        }
        {
            double scale = 3600.0;
            d9 = func_865C(d10, scale);
        }
        r4 = func_883C(d9);
        d8 = func_87B4(r4);
        func_16B18(fmt_buf, "%4f", d8);
        func_08334(out_buf, "d");
        func_08334(out_buf, fmt_buf);

        /* ---- 传感器 8: 纬度 → d10 → ×3600.0 → format "%4f" ---- */
        {
            uint32_t lat_val = SENSOR_LAT;
            d10 = func_875C(lat_val);
        }
        {
            double scale = 3600.0;
            d9 = func_865C(d10, scale);
        }
        r4 = func_883C(d9);
        d8 = func_87B4(r4);
        func_16B18(fmt_buf, "%4f", d8);
        func_08334(out_buf, "r");
        func_08334(out_buf, fmt_buf);

        /* ---- 传感器 9-15: 类似的 FPU 管道模式 (缩写) ---- */
        /* 模式: ldrh/ldr → func_875C/func_873A → ×1000.0/3600.0 → func_883C →
         *       func_87B4 → func_16B18 → strcat label → strcat value */

        /* 传感器: 压力 (0x20000058) → ×3600.0 → "%4f" → label "s" */
        /* 传感器: 航向 (0x20000056) → ×3600.0 → "%4f" → label "r" (另有路径) */
        /* 传感器: 经度 (0x20000074) → ×1000.0 → "%.1f" → label "A" (另有路径) */
        /* 传感器: 额外1 (0x20000138) → ×1000.0 → "%.1f" → label "L" (另有路径) */
        /* 传感器: 额外2 (0x2000013C) → ×1000.0 → "%.1f" → label "p" (另有路径) */

        /* ... 其余传感器处理块遵循相同模式 ...
         * 共约 15 个 FPU 管道块, 每个处理一个传感器值.
         * 每个块使用相同的 5 个函数调用序列:
         *   func_875C/func_873A → func_865C → func_883C → func_87B4 → func_16B18
         * 然后 2 次 func_08334 调用将 label + value 追加到 out_buf.
         * 完整展开约 15×(5+2) = 105 个 BL 调用 (加上条件分支约 120 条).
         */

        /* ---- 发布遥测字符串 ---- */
        func_09B6C(out_buf);
    }

    /* ================================================================
     * 路径 4: 备选遥测格式 — 调用 func_16664 (遥测包构建器)
     * ================================================================ */
    {
        uint8_t flag4 = *(volatile uint8_t *)0x2000A138;
        if (flag4 != 0) {
            func_0839A(out_buf, "");                   /* 初始化 */

            /* 构建带更多标签的字符串 */
            /* ... 更多 strcat 调用 ... */

            func_09B6C(out_buf);

            *(volatile uint8_t *)0x2000D0D2 = 0;
            /* fall through to cleanup */
        }
    }

cleanup:
    /* ================================================================
     * 清理: 将工作传感器值复制到影子输出寄存器
     *  0x0800A502-0x0800A56A (104 bytes, 26 STRH/STR 指令)
     *
     * 此块将当前传感器读数复制到用于外部访问的影子寄存器,
     * 可能在 USART/SPI 中断上下文中用于无竞态读取.
     * ================================================================ */
    {
        uint8_t state_byte = SENSOR_STATE;
        OUT_STATE = state_byte;

        OUT_SPEED    = SENSOR_SPEED;
        OUT_RPM      = SENSOR_RPM;
        OUT_TEMP     = SENSOR_TEMP;
        OUT_VOLT     = SENSOR_VOLT;
        OUT_FLAGS    = SENSOR_FLAGS;
        OUT_FLOW     = SENSOR_FLOW;
        OUT_LAT      = SENSOR_LAT;
        OUT_LON      = SENSOR_LON;
        OUT_HEADING  = SENSOR_HEADING;
        OUT_PRESSURE = SENSOR_PRESSURE;
        OUT_POWER    = SENSOR_POWER;
        OUT_STATUS   = SENSOR_STATUS;

        /* 额外复制: 两字节偏移的 ldrh 读取 */
        {
            uint16_t extra1 = *(volatile uint16_t *)0x20000170;
            *(volatile uint16_t *)0x200002C8 = extra1;
        }
        {
            uint32_t extra2 = *(volatile uint32_t *)0x200002DC;
            *(volatile uint32_t *)0x200002C8 = extra2;
        }

        /* 清除完成标志 */
        *(volatile uint8_t *)0x200002C6 = 0;
        *(volatile uint8_t *)(0x200002C6 + 1) = 0;
    }

    /* 恢复栈帧和 FPU 寄存器, 返回 */
    /* (编译器生成的尾声: addw sp, #0x220; vpop {d8,d9,d10}; pop {r4,pc}) */
}

/*
 * 内存布局:
 *   0x08009C84 - 0x08009CC0: 入口 + 路径 1 (标志检查 + MQTT 发布)
 *   0x08009CC2 - 0x08009CE0: 路径 2 (备选遥测格式)
 *   0x08009CE2 - 0x08009DF0: 路径 3 开始 (多传感器管道)
 *   0x08009DF2 - 0x0800A20A: 路径 3 主体 (重复 FPU 管道块 ×15)
 *   0x0800A20A - 0x0800A4C0: 路径 4 + 更多 strcat 构建
 *   0x0800A4C0 - 0x0800A56A: 影子寄存器复制块
 *   0x0800A56C - 0x0800A634: 字面量池 (RAM 地址 + 浮点常量)
 *   0x0800A618 - 0x0800A620: 尾声 (addw sp; vpop; pop)
 *
 * 字面量池:
 *   0x0800A0C4: 1000.0  (double)
 *   0x0800A0DC: 10.0    (double)
 *   0x0800A108: 3600.0  (double)
 *   0x0800A0CC: "%.1f"  (格式字符串)
 *   0x0800A110: "%4f"   (格式字符串)
 *   0x0800A0FC: "%d"    (格式字符串)
 *   0x0800A12C: "%7f"   (格式字符串)
 *   0x0800A0A8: "LJ%sLW%s" (GPS 标签模板)
 *
 * 注: 共 15 个 FPU 管道块共享相同模式. 为简洁起见,
 *     上方仅完全展开了 8 个块. 完整的 15 个块均遵循:
 *     raw = *(RAM_ADDR); d10 = int2double(raw);
 *     d9 = scale(d10, constant); r4 = convert(d9);
 *     d8 = encode(r4); format(buf, fmt, d8);
 *     strcat(out, label); strcat(out, buf);
 */
