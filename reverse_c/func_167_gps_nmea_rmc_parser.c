/**
 * @file func_167_gps_nmea_rmc_parser.c
 * @brief 函数: GPS NMEA $xxRMC 语句解析器 — 解析 RMC 推荐最小导航信息
 * @addr  0x08017EEA - 0x080181D0 (742 bytes)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 解析 GPS NMEA 0183 RMC 语句的 9 个关键字段:
 *   字段0: 语句类型 ($xxRMC)
 *   字段1: UTC 时间 (hhmmss.ss)  → 存入 out[0:2]+out[4:5]
 *   字段2: 数据有效标志 (A/V)   → 存入 out[0x0A]
 *   字段3: 纬度 (ddmm.mmmm)     → 存入 out[0x10]
 *   字段4: 北纬/南纬 (N/S)     → 存入 out[0x0C]
 *   字段5: 经度 (dddmm.mmmm)    → 存入 out[0x18]
 *   字段6: 东经/西经 (E/W)     → 存入 out[0x14]
 *   字段7: 地面速率 (节)       → 存入 out[0x1C]
 *   字段9: UTC 日期 (ddmmyy)   → 存入 out[0x06:0x09]
 *   字段12: 模式指示 (N/A/D/E) → 存入 out[0x20]
 *
 * 寄存器映射:
 *   r4 = 输出缓冲区指针 (out_time/out_lat/... 结构体)
 *   r5 = 状态累加器
 *   r6 = nmea_sentence 字符串指针
 *   sp+8 = field_buf[0] (字段指针, func_17E74 输出)
 *   sp+4 = field_buf[1] (整数值, func_18334 输出)
 *   sp+0 = field_buf[2] (小数位数, func_18334 输出)
 *
 * 字面量池 (0x080181C8-0x080181CF):
 *   [0x080181C8] = 0x00989680 (10,000,000) — SDIV 除数
 *   [0x080181CC] = 0x000186A0 (100,000)    — SDIV 除数
 *
 * 调用约定:
 *   参数通过 r4(r6) 传递 (nmea_sentence, 输出缓冲区)
 *   r0-r3 在每次 func_17E74/func_18334 调用时重新设置
 * ================================================================ */

#include "stm32f4xx_hal.h"

/* 字面量池常量 */
#define DIV_10M   (10000000U)  /* 0x00989680 @ 0x080181C8 */
#define DIV_100K  (100000U)    /* 0x000186A0 @ 0x080181CC */

/* func_17E74: 参数字段提取器
 *   r0 = mode (0=仅指针, 1=指针+数据)
 *   r1 = field_index
 *   r2 = 输出缓冲区 (uint32_t[2])
 *   r3/r6 = NMEA 字符串
 *   返回: r0 = 状态码 (0=成功)
 */
extern uint32_t func_17E74(uint32_t mode, uint32_t field_idx,
                            uint32_t *out, const char *nmea);

/* func_18334: 字符串→整数转换
 *   r0 = 输入字符串
 *   r1 = 小数位数输出
 *   r2 = 整数部分输出
 *   返回: r0 = 状态码 (0=成功)
 */
extern uint32_t func_18334(const char *str, uint32_t *out_frac_precision,
                            uint32_t *out_int);

/* func_181D0: 字段精度调整 (下一个函数)
 *   r0 = 值缓冲区指针
 *   r1 = 小数位数
 *   r2 = 目标精度
 */
extern void func_181D0(uint32_t *val, uint32_t precision, uint32_t target_prec);

/*
 * GPS_NMEA_RMC_Parser 输出结构体 (r4 指向):
 *   [r4+0x00] = UTC 小时 (uint8)
 *   [r4+0x01] = UTC 分钟 (uint8)
 *   [r4+0x02] = UTC 秒   (uint8)
 *   [r4+0x04] = UTC 小数秒 (uint16)  — 百分秒 (hh)
 *   [r4+0x06] = 日 (uint16)         — 日期 day
 *   [r4+0x08] = 月 (uint8)          — 日期 month
 *   [r4+0x09] = 年 (uint8)          — 日期 year (YY)
 *   [r4+0x0A] = 有效标志 (uint8)    — 'V'→1 无效, 'A'→0 有效
 *   [r4+0x0C] = 北纬/南纬 (uint8)   — 'N'→0, 'S'→1
 *   [r4+0x10] = 纬度 ×1e6 (int32)   — 度*1000000 + 分*1000000
 *   [r4+0x14] = 指示 (uint8)      — 'E'→0, 'W'→1
 *   [r4+0x18] = 经度 ×1e6 (int32)   — 度*1000000 + 分*1000000
 *   [r4+0x1C] = 速率 (uint16)      — 节 ×10
 *   [r4+0x20] = 模式 (uint8)       — N=0, A=1, D=2, E=3
 */

/* ================================================================
 * GPS_NMEA_RMC_Parser() @ 0x08017EEA
 *   无独立 PUSH — 使用调用者栈帧
 *   参数: r4=输出缓冲区, r6=nmea_sentence 字符串
 *   返回: r0 = 0 (成功), 1 (格式错误), 3 (类型不匹配)
 * ================================================================ */
__attribute__((naked))
uint32_t GPS_NMEA_RMC_Parser(void)
{
    register const char *nmea asm("r6");
    register uint8_t    *out  asm("r4");
    register uint32_t    status asm("r5");
    register uint32_t    result asm("r0");

    uint32_t field_buf[2];  /* sp+8: 指针, sp+4: 整数, sp+0: 精度 */
    uint32_t raw_val, frac_prec, degrees, minutes, seconds, frac;

    /* === 字段 0: 语句类型检查 === */
    /* 0x08017EEA-0x08017F14 */
    status = func_17E74(0, 0, field_buf, nmea); /* 提取第0字段 */
    if (status != 0)
        return status;

    {
        const char *type = (const char *)field_buf[0];
        /* 检查 "$xxRMC" 标识: 偏移2='R', 3='M', 4='C' */
        if (type[2] != 'R' || type[3] != 'M' || type[4] != 'C')
            return 3;  /* 类型不匹配 (0x08017F12) */
    }

    /* 字段0已确认 — 开始逐字段解析 */

    /* === 字段 1: UTC 时间 (hhmmss.ss) === */
    /* 0x08017F16-0x08017F8C */
    status  = func_17E74(1, 1, field_buf, nmea);   /* 提取字段1 */
    result  = func_18334((const char *)field_buf[0], &frac_prec, &raw_val);
    status += result;
    status  = (uint8_t)status;  /* UXTB */

    if (status != 0) goto fail;

    /* 精度必须为 2 (小数秒为 .ss) */
    if (frac_prec != 2) goto fail;

    /*
     * 时间解码: 输入 raw_val = hhmmss × 100 + ss (8位整数)
     *
     * 小时:   (raw_val / 10000000) % 100 → [r4+0]
     * 分钟:   (raw_val / 100000)    % 100 → [r4+1]
     * 秒:     (raw_val / 1000)      % 100 → [r4+2]
     * 小数秒: raw_val % 1000               → [r4+4] (半字)
     */
    /* 小时提取 (0x08017F40-0x08017F52) */
    raw_val = field_buf[1];  /* 从 [sp+4] 重新加载 */
    degrees = raw_val / DIV_10M;
    out[0]  = (uint8_t)(degrees % 100);

    /* 分钟提取 (0x08017F54-0x08017F66) */
    raw_val = field_buf[1];
    degrees = raw_val / DIV_100K;
    out[1]  = (uint8_t)(degrees % 100);

    /* 秒提取 (0x08017F68-0x08017F7C) */
    raw_val = field_buf[1];
    degrees = raw_val / 1000;
    out[2]  = (uint8_t)(degrees % 100);

    /* 小数秒提取 (0x08017F7E-0x08017F8C) */
    raw_val = field_buf[1];
    seconds = raw_val / 1000;
    *(uint16_t *)(out + 4) = (uint16_t)(raw_val - seconds * 1000);

    /* === 字段 2: 数据有效标志 (A=有效, V=无效) === */
    /* 0x08017F8E-0x08017FC0 */
    status = func_17E74(1, 2, field_buf, nmea);
    if (status != 0) goto parse_done;

    {
        const char *valid_flag = (const char *)field_buf[0];
        uint8_t c = valid_flag[0];

        /* 非 'V' 且非 'A' → 格式错误 */
        if (c != 'V' && c != 'A') goto fail;

        /* 'V' → 1 (无效), 'A' → 0 (有效) */
        out[0x0A] = (c == 'V') ? 1 : 0;
    }

    /* === 字段 3: 纬度 (ddmm.mmmm) === */
    /* 0x08017FC2-0x08018014 */
    status  = func_17E74(1, 3, field_buf, nmea);
    result  = func_18334((const char *)field_buf[0], &frac_prec, &raw_val);
    status += result;
    status  = (uint8_t)status;
    if (status != 0) goto fail;

    /* 精度调整为 5 位 (0x08017FE4-0x08017FEC) */
    func_181D0(&raw_val, frac_prec, 5);

    /*
     * 纬度: ddmm.mmmm → 度*1000000 + 分*1000
     * raw_val 单位: ddmm.mmmm (6 位整数 + 4 位小数 = ddmm mmmm)
     *
     * 度: raw_val / 10000000 → 度部分
     * 分: (raw_val % 10000000) / 60 → 分钟转度数
     * 实际计算 (0x08017FF0-0x08018014):
     *   degrees = raw_val / 10000000 → 整数度
     *   raw % 10000000 → 剩余 (包含分+小数)
     *   剩余 / 100 → 移除小数部分
     *   + (度 * 1000000)
     */
    /* degree = (raw_val / 10M) * 1000000 */
    raw_val = field_buf[1];
    degrees = (raw_val / DIV_10M) * 1000000UL;

    /* minutes = (raw_val / 100) % 100000 */
    seconds = raw_val / DIV_100K;
    minutes = raw_val - seconds * DIV_100K;  /* remainder */
    minutes = minutes / 60;                   /* convert to decimal degrees */

    *(uint32_t *)(out + 0x10) = degrees + minutes;

    /* === 字段 4: 北纬/南纬 (N/S) === */
    /* 0x08018016-0x08018048 */
    status = func_17E74(1, 4, field_buf, nmea);
    if (status != 0) goto parse_done;

    {
        const char *ns = (const char *)field_buf[0];
        uint8_t c = ns[0];

        if (c != 'N' && c != 'S') goto fail;

        /* 'N' → 0, 'S' → 1 */
        out[0x0C] = (c == 'N') ? 0 : 1;
    }

    /* === 字段 5: 经度 (dddmm.mmmm) — 与纬度对称 === */
    /* 0x0801804A-0x0801809C */
    status  = func_17E74(1, 5, field_buf, nmea);
    result  = func_18334((const char *)field_buf[0], &frac_prec, &raw_val);
    status += result;
    status  = (uint8_t)status;
    if (status != 0) goto fail;

    func_181D0(&raw_val, frac_prec, 5);

    raw_val = field_buf[1];
    degrees = (raw_val / DIV_10M) * 1000000UL;
    seconds = raw_val / DIV_100K;
    minutes = (raw_val - seconds * DIV_100K) / 60;

    *(uint32_t *)(out + 0x18) = degrees + minutes;

    /* === 字段 6: 东经/西经 (E/W) === */
    /* 0x0801809E-0x080180D0 */
    status = func_17E74(1, 6, field_buf, nmea);
    if (status != 0) goto parse_done;

    {
        const char *ew = (const char *)field_buf[0];
        uint8_t c = ew[0];

        if (c != 'E' && c != 'W') goto fail;

        /* 'E' → 0, 'W' → 1 */
        out[0x14] = (c == 'E') ? 0 : 1;
    }

    /* === 字段 7: 地面速率 (节, x.x) === */
    /* 0x080180D2-0x08018102 */
    status  = func_17E74(1, 7, field_buf, nmea);
    result  = func_18334((const char *)field_buf[0], &frac_prec, &raw_val);
    status += result;
    status  = (uint8_t)status;
    if (status != 0) goto fail;

    /* 精度调整为 1 位小数 */
    func_181D0(&raw_val, frac_prec, 1);

    raw_val = field_buf[1];
    *(uint16_t *)(out + 0x1C) = (uint16_t)raw_val;

    /* 跳过字段 8 (航向) — 不解析 */

    /* === 字段 9: UTC 日期 (ddmmyy) === */
    /* 0x08018104-0x08018162 */
    status  = func_17E74(1, 9, field_buf, nmea);
    result  = func_18334((const char *)field_buf[0], &frac_prec, &raw_val);
    status += result;
    status  = (uint8_t)status;

    if (status == 0 && frac_prec != 0) {
        /*
         * 日期解码: NMEA ddmmyy → 分离日/月/年
         * raw_val = ddmmyy (6 位整数, 例如 230523 = 2023-05-23)
         *
         * 实际代码 (0x0801812C-0x08018162):
         *   年 = (raw_val % 100) + 2000       → [r4+6] (strh, YYYY)
         *   月 = (raw_val / 100) % 100        → [r4+8] (strb, MM)
         *   日 = (raw_val / 10000) % 100      → [r4+9] (strb, DD)
         */
        raw_val = field_buf[1];

        /* 年 (raw % 100) + 2000 → YYYY, 半字存储 */
        degrees = raw_val % 100;
        *(uint16_t *)(out + 0x06) = (uint16_t)(degrees + 2000);

        /* 月: (raw / 100) % 100 */
        degrees = raw_val / 100;
        out[0x08] = (uint8_t)(degrees % 100);

        /* 日: (raw / 10000) % 100 */
        raw_val = field_buf[1];
        degrees = raw_val / 10000;
        out[0x09] = (uint8_t)(degrees % 100);
    }

    /* 跳过字段 10 (磁偏角), 字段 11 (偏角方向) */

    /* === 字段 12: 模式指示 (N=无, A=自主, D=差分, E=估算) === */
    /* 0x08018164-0x080181C0 */
    status = func_17E74(1, 12, field_buf, nmea);
    if (status != 0) goto parse_done;

    {
        const char *mode = (const char *)field_buf[0];
        uint8_t c = mode[0];
        uint8_t mode_val;

        /* 非 N/A/D/E → 失败 */
        if (c != 'N' && c != 'A' && c != 'D' && c != 'E')
            goto fail;

        if      (c == 'N')       mode_val = 0;
        else if (c == 'A')       mode_val = 1;
        else if (c == 'D')       mode_val = 2;
        else /* c == 'E' */     mode_val = 3;

        out[0x20] = mode_val;  /* 0x080181BE: STRB.W */
    }

    /* 成功返回 */
    result = 0;

    /* 函数尾部: B 到调用者返回地址 (0x080181C4 → 0x08017EE8) */
    return result;

parse_done:
    /* 部分字段存在 status != 0 但无需整体失败 — 已解析字段有效 */
    result = 0;
    return result;

fail:
    /* 格式错误 */
    result = 1;
    return result;
}

/* 字面量池 (0x080181C6-0x080181CF):
 *   [0x080181C6] = 0x0000      — 对齐填充
 *   [0x080181C8] = 0x00989680  — 10,000,000 (SDIV 除数)
 *   [0x080181CC] = 0x000186A0  — 100,000    (SDIV 除数)
 *   [0x080181D0] ← func_140 入口 (下一个函数)
 */
