/**
 * @file func_148_4g_module_at_handlers.c
 * @brief 4G/GSM 模块 AT 命令构建与发送子系统
 * @addr  0x08009AF8 - 0x0800A64C (2900 bytes, 6 个函数)
 *        NOTE: Sub-range 0x08009C84-0x0800A634 is covered in detail by
 *        func_149_4g_telemetry_reporter.c (FPU pipeline, MQTT publish).
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * AT 命令构建器集合, 用于 4G/GSM 模块 (通过 UART 连接) 的 MQTT 通信.
 * 所有函数遵循相同模式: 在栈上构建字符串 → strlen → 逐字节 UART 发送.
 *
 * 字符串构建核心库:
 *   func_0839A (strcpy): 将源字符串复制到目标缓冲区
 *   func_08334 (strcat): 将源字符串追加到目标缓冲区
 *   func_08370 (strlen): 返回字符串长度 (UXTH 后为 16 位)
 *   func_0BBFC (UART_Send): 逐字节发送缓冲区 (通过 func_15F38 TX byte)
 *
 * 全局 RAM 字符串:
 *   0x2000042C: 设备标识符字符串 (运行时动态, 由初始化序列填充)
 *
 * MQTT 主题结构: "W/V2 Plus/<device_id>"
 * MQTT 服务器:   49.235.68.247:1883
 */

#include "stm32f4xx_hal.h"

/* ---- 外部依赖 ---- */
extern void     func_0839A(char *dst, const char *src);  /* strcpy */
extern void     func_08334(char *dst, const char *src);  /* strcat */
extern uint16_t func_08370(const char *str);             /* strlen → UXTH */
extern void     func_0BBFC(const char *buf, uint16_t len); /* UART 逐字节发送 */

#define DEVICE_ID_STR  ((const char *)0x2000042C)  /* 设备标识符 (RAM) */

/* ========================================================================
 * 子函数 1: AT_Admin_Auth_Sender (0x08009AF8, 64 bytes, 28 insns)
 *
 * 构建并发送管理员认证 AT 命令.
 * 命令格式: <DEVICE_ID_STR> + ",admin,d9ATO#cV\r\n"
 *
 * 栈帧: 0x80 字节 (128 bytes) — 足够容纳设备 ID + 16 字节命令后缀
 * ======================================================================== */
void AT_Admin_Auth_Sender(void)
{
    char buf[0x80];                          /* sp+0 */

    func_0839A(buf, DEVICE_ID_STR);          /* strcpy: 复制设备 ID 前缀 */
    func_08334(buf, ",admin,d9ATO#cV\r\n");  /* strcat: 追加认证命令 */
    uint16_t len = func_08370(buf);          /* strlen */
    func_0BBFC(buf, len);                    /* UART 逐字节发送 */
}

/* ========================================================================
 * 子函数 2: AT_MQTT_Connect_Sender (0x08009B38, 52 bytes, 23 insns)
 *
 * 发送 MQTT CONNECT 命令 — 建立到服务器的 MQTT 连接.
 * 命令: AT+MCONNECT=1,60\r\n
 *   - 参数 1: client-id 索引 (1)
 *   - 参数 2: keep-alive 秒数 (60)
 *
 * 栈帧: 0x80 字节
 * ======================================================================== */
void AT_MQTT_Connect_Sender(void)
{
    char buf[0x80];                          /* sp+0 */

    func_0839A(buf, "AT+MCONNECT=1,60\r\n"); /* strcpy */
    uint16_t len = func_08370(buf);          /* strlen */
    func_0BBFC(buf, len);                    /* UART 逐字节发送 */
}

/* ========================================================================
 * 子函数 3: AT_MQTT_Publish_Builder (0x08009B6C, 128 bytes, 53 insns)
 *
 * MQTT PUBLISH 命令构建器 — 接受载荷字符串作为参数.
 * 命令格式:
 *   AT+MPUB="W/V2 Plus/<device_id>",0,0,"<payload>"\r\n
 *
 * 参数:
 *   r0 = payload: 要发布的 MQTT 载荷字符串
 *
 * 栈帧: 0x404 字节 (1028 bytes) — 容纳大型载荷
 * 调用者: func_09C84 (4 次调用, 用于发布遥测数据)
 * ======================================================================== */
void AT_MQTT_Publish_Builder(const char *payload)
{
    char buf[0x400];                              /* sp+4, r4 = payload */

    func_0839A(buf, "AT+MPUB=\"W/V2 Plus/");      /* strcpy: 主题前缀 */
    func_08334(buf, DEVICE_ID_STR);               /* strcat: 设备 ID → 主题 */
    func_08334(buf, "\",0,0,");                   /* strcat: QoS=0, retain=0, 载荷开始 */
    func_08334(buf, "\"");                        /* strcat: 开载荷引号 */
    func_08334(buf, payload);                     /* strcat: 载荷内容 (r4) */
    func_08334(buf, "\"");                        /* strcat: 关载荷引号 */
    func_08334(buf, "\r\n");                      /* strcat: AT 命令终止符 */

    uint16_t len = func_08370(buf);               /* strlen → r5 */
    func_0BBFC(buf, len);                         /* UART 逐字节发送 */
}

/* ========================================================================
 * 子函数 4: AT_MQTT_IPStart_Sender (0x08009BEC, 72 bytes, 33 insns)
 *
 * 发送 MQTT TCP/IP 连接建立命令.
 * 命令: AT+MIPSTART="49.235.68.247","1883"\r\n
 *   - 49.235.68.247: MQTT 代理 IP 地址 (硬编码)
 *   - 1883:          MQTT 标准端口
 *
 * 栈帧: 0x80 字节
 * ======================================================================== */
void AT_MQTT_IPStart_Sender(void)
{
    char buf[0x80];                                            /* sp+0 */

    func_0839A(buf, "AT+MIPSTART=\"49.235.68.247\",\"1883\"\r\n"); /* strcpy */
    uint16_t len = func_08370(buf);                            /* strlen */
    func_0BBFC(buf, len);                                      /* UART 逐字节发送 */
}

/* ========================================================================
 * 子函数 5: AT_MQTT_Subscribe_Sender (0x08009C34, 80 bytes, 35 insns)
 *
 * MQTT SUBSCRIBE 命令构建器.
 * 命令格式:
 *   AT+MSUB="W/V2 Plus/<device_id>-1",0\r\n
 *   - 主题过滤器: "W/V2 Plus/<device_id>-1"
 *   - QoS: 0
 *
 * 栈帧: 0x40 字节 (64 bytes)
 * ======================================================================== */
void AT_MQTT_Subscribe_Sender(void)
{
    char buf[0x40];                              /* sp+0 */

    func_0839A(buf, "AT+MSUB=\"W/V2 Plus/");     /* strcpy: 主题前缀 */
    func_08334(buf, DEVICE_ID_STR);              /* strcat: 设备 ID → 主题过滤器 */
    func_08334(buf, "-1\",0\r\n");               /* strcat: topic 后缀 -1", QoS=0, CRLF */

    uint16_t len = func_08370(buf);              /* strlen */
    func_0BBFC(buf, len);                        /* UART 逐字节发送 */
}

/* ========================================================================
 * 子函数 6: AT_CSQ_Sender (0x0800A634, 24 bytes, 11 insns)
 *
 * 发送 AT+CSQ 命令 — 查询 GSM 信号质量.
 * 命令: AT+CSQ\r\n (8 字节)
 *
 * 内联 ADR 直接指向字面量池中的 "AT+CSQ\r\n\0".
 * 无栈帧分配 — 直接从 .rodata 发送.
 * ======================================================================== */
void AT_CSQ_Sender(void)
{
    func_0BBFC("AT+CSQ\r\n", 8);                 /* 直接发送 8 字节命令 */
}

/* ========================================================================
 * 内存布局:
 *   0x08009AF8 - 0x08009B38: AT_Admin_Auth_Sender (64B)
 *   0x08009B38 - 0x08009B6C: AT_MQTT_Connect_Sender (52B)
 *   0x08009B6C - 0x08009BEC: AT_MQTT_Publish_Builder (128B)
 *   0x08009BEC - 0x08009C34: AT_MQTT_IPStart_Sender (72B)
 *   0x08009C34 - 0x08009C84: AT_MQTT_Subscribe_Sender (80B)
 *   0x08009C84 - 0x0800A634: 4G_Telemetry_Reporter (2480B, 单独文件)
 *   0x0800A634 - 0x0800A64C: AT_CSQ_Sender (24B)
 *   0x0800A64C - 0x0800BCE8: 4G_Response_Parser (5788B, 单独文件)
 *
 * 字面量池 (内联数据):
 *   0x08009B20: 0x2000042C (DEVICE_ID_STR)
 *   0x08009B24: ",admin,d9ATO#cV\r\n\0"
 *   0x08009B58: "AT+MCONNECT=1,60\r\n\0"
 *   0x08009BC4: "AT+MPUB=\"W/V2 Plus/\0"
 *   0x08009BD8: 0x2000042C (DEVICE_ID_STR)
 *   0x08009BDC: "\",0,0,\0"
 *   0x08009BE4: "\"\0"
 *   0x08009BE8: "\r\n\0"
 *   0x08009C0C: "AT+MIPSTART=\"49.235.68.247\",\"1883\"\r\n\0" (38 bytes)
 *   0x08009C64: "AT+MSUB=\"W/V2 Plus/\0"
 *   0x08009C78: 0x2000042C (DEVICE_ID_STR)
 *   0x08009C7C: "-1\",0\r\n\0"
 *   0x0800A640: "AT+CSQ\r\n\0"
 *
 * 调用关系:
 *   AT_Admin_Auth_Sender   → func_0839A, func_08334, func_08370, func_0BBFC
 *   AT_MQTT_Connect_Sender → func_0839A, func_08370, func_0BBFC
 *   AT_MQTT_Publish_Builder → func_0839A, func_08334×6, func_08370, func_0BBFC
 *   AT_MQTT_IPStart_Sender → func_0839A, func_08370, func_0BBFC
 *   AT_MQTT_Subscribe_Sender → func_0839A, func_08334×2, func_08370, func_0BBFC
 *   AT_CSQ_Sender          → func_0BBFC
 */
