/**
 * @file func_47_version_string_sender.c
 * @brief 函数: Version_String_Sender — 构建版本号字符串并通过帧发送
 * DISASSEMBLY-TRACED. Verified against Capstone.
 * @addr  0x0800E234 - 0x0800E2D8 (164 bytes, 含字面量池)
 *         字面量池 0x0800E2C2-0x0800E2D6 (20 bytes)
 *
 * 构建格式为 "a.b.c.(d+1)" 的版本字符串 (共9字节)，以 cmd 0 帧发送，
 * 再构建 "V02.0B" 格式后缀 (共7字节) 以 cmd 0 帧发送。
 *
 * 参数:
 *   r0/r4 = ver_a  — 版本号第一段 (→ "%d")
 *   r1/r5 = ver_b  — 版本号第二段 (→ "%d")
 *   r2/r8 = unused  — 保存至 r8 但函数内未使用
 *   r3/r6 = ver_c  — 版本号第三段 (→ "%d")
 *   [sp+0x98]/r7 = ver_d  — 版本号第四段 (发送时 +1)
 *
 * 调用:
 *   func_16B18() @ 0x08016B18 — sprintf 类格式化
 *   func_8334()  @ 0x08008334 — strcat 类字符串拼接
 *   func_12788() @ 0x08012788 — 数据传输帧发送
 *
 * 帧序列:
 *   1. cmd=0, len=9,  payload = "ver_a.ver_b.ver_c.(ver_d+1)"  (9 chars)
 *   2. cmd=0, len=7,  payload = "V02.0B"                        (7 chars)
 *
 * 字面量池 (0x0800E2C2-0x0800E2D6):
 *   0x0800E2C4: "%d"            (4 bytes, 复用4次)
 *   0x0800E2C8: "."             (4 bytes, 复用3次)
 *   0x0800E2CC: "V%02X.%02X"    (12 bytes, 末尾格式串)
 *
 * 注: r2 被存入 r8 但整函数内未被使用 — 该参数可能保留供上层兼容。
 */

#include "stm32f4xx_hal.h"

extern void func_16B18(uint8_t *buf, const char *fmt, uint32_t arg1);       /* 0x08016B18 — sprintf-like, 支持 variadic */
extern void func_16B18_4(uint8_t *buf, const char *fmt, uint32_t arg1,      /* 0x08016B18 — 4-ary variant */
                         uint32_t arg2);
extern void func_8334(uint8_t *dest, const uint8_t *src);                   /* 0x08008334 — strcat-like */
extern void func_12788(uint32_t mode, uint32_t len, uint8_t *data);         /* 0x08012788 — 数据传输 */

/* ================================================================
 * Version_String_Sender() @ 0x0800E234
 *   push.w {r4, r5, r6, r7, r8, lr}
 *   sub sp, #0x80
 *   栈帧: sp+0x00..0x3F = 临时缓冲区 (64 bytes)
 *         sp+0x40..0x7F = 主缓冲区 (64 bytes)
 *   r4=r0, r5=r1, r8=r2, r6=r3, r7=[sp+0x98]
 *
 * 格式: "{r4}.{r5}.{r6}.{r7+1}" → cmd=0 len=9
 *       "V{suffix}"            → cmd=0 len=7
 * ================================================================ */
void Version_String_Sender(uint32_t ver_a, uint32_t ver_b, uint32_t unused,
                           uint32_t ver_c, uint32_t ver_d)
{
    uint8_t buf[0x40];      /* sp+0x40 — main output buffer */
    uint8_t tmp[0x40];      /* sp+0x00 — scratch buffer for sprintf */

    /* ver_d 为第5参数, ARM EABI 自动从栈加载
     * (入口 push.w {r4-r8,lr} + sub sp,#0x80 后位于 sp+0x98) */

    /* ---- 构建: sprintf(buf, "%d", ver_a) ---- */
    func_16B18(buf, "%d", ver_a);                        /* 0x0800E246-24A: adr "%d"; bl func_16B18 */

    /* ---- 拼接: strcat(buf, ".") ---- */
    func_8334(buf, (uint8_t *)".");                     /* 0x0800E24E-252: adr "."; bl func_8334 */

    /* ---- 拼接格式化的 ver_b ---- */
    func_16B18(tmp, "%d", ver_b);                       /* 0x0800E256-25C: adr "%d"; bl func_16B18 */
    func_8334(buf, tmp);                                /* 0x0800E260-264: bl func_8334 */

    /* ---- 拼接: strcat(buf, ".") ---- */
    func_8334(buf, (uint8_t *)".");                     /* 0x0800E268-26C: adr "."; bl func_8334 */

    /* ---- 拼接格式化的 ver_c ---- */
    func_16B18(tmp, "%d", ver_c);                       /* 0x0800E270-276: adr "%d"; bl func_16B18 */
    func_8334(buf, tmp);                                /* 0x0800E27A-27E: bl func_8334 */

    /* ---- 拼接: strcat(buf, ".") ---- */
    func_8334(buf, (uint8_t *)".");                     /* 0x0800E282-286: adr "."; bl func_8334 */

    /* ---- 拼接格式化的 (ver_d + 1) ---- */
    func_16B18(tmp, "%d", ver_d + 1);                   /* 0x0800E28A-290: adds r2,r7,#1; adr "%d"; bl func_16B18 */
    func_8334(buf, tmp);                                /* 0x0800E294-298: bl func_8334 */

    /* ---- 发送第一帧: cmd=0, len=9, payload=buf ---- */
    func_12788(0, 9, buf);                              /* 0x0800E29C-2A2: movs r1,#9; movs r0,#0; bl func_12788 */

    /* ---- 构建版本后缀: sprintf(buf, "V%02X.%02X", 2, 0xB) ---- */
    func_16B18_4(buf, "V%02X.%02X", 2, 0xB);            /* 0x0800E2A6-2AE: r3=0xB; r2=2; adr fmt; bl func_16B18 */

    /* ---- 发送第二帧: cmd=0, len=7, payload=buf ---- */
    func_12788(0, 7, buf);                              /* 0x0800E2B2-2B8: movs r1,#7; movs r0,#0; bl func_12788 */
}
