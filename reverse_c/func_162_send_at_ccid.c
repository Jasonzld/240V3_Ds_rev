/**
 * @file func_162_send_at_ccid.c
 * @brief 发送 AT+CCID 命令查询 4G 模块 CCID 编号
 * @addr  0x080099E0 - 0x080099F8 (24 bytes, 代码 12B + 字面量池 12B)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 叶函数: 通过 ADR 加载内联的 "AT+CCID\r\n" 字符串,
 * 调用 UART 发送函数以长度 10 发送.
 *
 * 调用: func_0BBFC (UART_String_Sender) ×1
 */

#include "stm32f4xx_hal.h"

/* ---- 外部依赖 ---- */
extern void func_0BBFC(const char *str, uint16_t len);  /* UART 字符串发送 */

/* ========================================================================
 * Send_AT_CCID_Cmd (0x080099E0 - 0x080099EA, 10 bytes + 12B pool)
 *
 * 指令序列:
 *   0x080099E0: push {r4,lr}
 *   0x080099E2: movs r1,#0xA      → len = 10
 *   0x080099E4: adr r0,#4         → r0 = PC+4 = 0x080099EC (字符串指针)
 *   0x080099E6: bl 0x0800BBFC     → UART 发送
 *   0x080099EA: pop {r4,pc}
 *
 * 字面量池 @ 0x080099EC:
 *   41 54 2B 49 43 43 49 44 0D 0A 00 00
 *   "A" "T" "+" "I" "C" "C" "I" "D" \r \n \0 \0
 *
 * 发送 "AT+ICCID\r\n" (查询 SIM 卡 ICCID 编号的 AT 指令).
 * 被 func_148 (4G Module AT Handlers) 调用.
 * ======================================================================== */
static void Send_AT_CCID_Cmd(void)
{
    /* ADR r0, #4 → 指向紧跟 BX LR 之后的字符串 */
    static const char cmd[] = "AT+ICCID\r\n";
    func_0BBFC(cmd, 10);
}

/*
 * 内存布局:
 *   0x080099E0: PUSH {r4,lr}
 *   0x080099E2: MOVS r1,#0xA
 *   0x080099E4: ADR r0,#4
 *   0x080099E6: BL 0x0800BBFC
 *   0x080099EA: POP {r4,pc}
 *   0x080099EC: "AT+CCID\r\n\0\0" (12 bytes)
 *   0x080099F8: 下一函数/数据开始
 *
 * 注: ADR 是 PC 相对寻址, 16-bit 编码, offset=4 → PC+4.
 *     此处 PC 为 0x080099E4+4=0x080099E8, 加 4 = 0x080099EC,
 *     正好是 PUSH/POP 之间紧跟的字符串.
 *     字符串以双零结尾, 对齐至 4 字节边界.
 */
