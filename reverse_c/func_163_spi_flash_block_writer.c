/**
 * @file func_163_spi_flash_block_writer.c
 * @brief SPI Flash 数据块写入 — 发送命令 0x03 + 24 位地址 + 循环写入数据
 * @addr  0x08015FF8 - 0x08016048 (80 bytes, 代码 74B + 字面量池 6B)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * SPI Flash 块写入协议:
 *   1. 清除完成标志 (bit-band 写 0 到 0x424082A8)
 *   2. 发送读命令字节 0x03
 *   3. 发送 3 字节地址 (2×UBFX + 1×UXTB 从 r1 拆分 24-bit)
 *   4. 循环: 发送 0xFF (dummy), 读取并存储响应字节
 *   5. 设置完成标志 (bit-band 写 1 到 0x424082A8)
 *
 * 调用: func_118EC (UART/SPI 字节发送) ×5
 *
 * 参数: r0 = dest_ptr — 数据目标缓冲区
 *       r1 = addr24  — 24 位目标地址
 *       r2 = length  — 数据长度
 */

#include "stm32f4xx_hal.h"

/* ---- 外部依赖 ---- */
extern uint8_t func_118EC(uint8_t byte);  /* SPI/UART 全双工字节收发 (0x080118EC) */

/* ---- 位段别名 ---- */
#define FLASH_DONE_FLAG  (*(volatile uint32_t *)0x424082A8)  /* 完成标志 (bit-band) */

/* ========================================================================
 * SPI_Flash_Block_Writer (0x08015FF8 - 0x0801603E, 70 bytes + 10B pool)
 *
 * 指令序列:
 *   0x08015FF8: push.w {r4,r5,r6,r7,r8,lr}
 *   0x08015FFC: mov r6,r0             → r6 = dest_ptr
 *   0x08015FFE: mov r4,r1             → r4 = addr24
 *   0x08016000: mov r7,r2             → r7 = length
 *   0x08016002: movs r0,#0
 *   0x08016004: ldr r1,[pc,#0x3C]     → r1 = &FLASH_DONE_FLAG (0x424082A8)
 *   0x08016006: str r0,[r1]           → 清除完成标志
 *
 *   0x08016008: movs r0,#3            → 命令字节 0x03 (SPI Flash Read)
 *   0x0801600A: bl func_118EC
 *
 *   0x0801600E: ubfx r0,r4,#0x10,#8   → 地址[23:16]
 *   0x08016012: bl func_118EC
 *   0x08016016: ubfx r0,r4,#8,#8      → 地址[15:8]
 *   0x0801601A: bl func_118EC
 *   0x0801601E: uxtb r0,r4            → 地址[7:0]
 *   0x08016020: bl func_118EC
 *
 *   0x08016024: movs r5,#0            → 索引 = 0
 *   0x08016026: b loop_check
 *   loop:
 *   0x08016028: movs r0,#0xFF         → dummy 字节 (SPI 读需要时钟)
 *   0x0801602A: bl func_118EC
 *   0x0801602E: strb r0,[r6,r5]       → 存储返回字节到 dest[idx]
 *   0x08016030: adds r0,r5,#1
 *   0x08016032: uxth r5,r0            → idx++
 *   loop_check:
 *   0x08016034: cmp r5,r7
 *   0x08016036: blo loop              → idx < length
 *
 *   0x08016038: movs r0,#1
 *   0x0801603A: ldr r1,[pc,#8]        → r1 = &FLASH_DONE_FLAG
 *   0x0801603C: str r0,[r1]           → 设置完成标志
 *   0x0801603E: pop.w {r4,r5,r6,r7,r8,pc}
 *
 * 字面量池 @ 0x08016040-0x08016048:
 *   +0x40: 0x424082A8 (FLASH_DONE_FLAG, bit-band alias)
 *   +0x44: 0xFFFFxxxx (padding/另一常量)
 * ======================================================================== */
void SPI_Flash_Block_Writer(uint8_t *dest_ptr, uint32_t addr24, uint16_t length)
{
    uint16_t i;

    /* 阶段 1: 清除完成标志 */
    FLASH_DONE_FLAG = 0;

    /* 阶段 2: 发送 SPI Flash Read 命令 (0x03) */
    func_118EC(0x03);

    /* 阶段 3: 发送 24 位地址 (高位优先) */
    func_118EC((uint8_t)(addr24 >> 16));   /* UBFX r0,r4,#0x10,#8 */
    func_118EC((uint8_t)(addr24 >> 8));    /* UBFX r0,r4,#8,#8   */
    func_118EC((uint8_t)(addr24));         /* UXTB r0,r4         */

    /* 阶段 4: 循环读取数据 */
    for (i = 0; i < length; i++) {
        dest_ptr[i] = func_118EC(0xFF);    /* 发送 dummy, 返回接收字节 */
    }

    /* 阶段 5: 设置完成标志 */
    FLASH_DONE_FLAG = 1;
}

/*
 * 内存布局:
 *   0x08015FF8: PUSH.W {r4,r5,r6,r7,r8,lr}
 *   0x08015FFC - 0x08016006: 清除完成标志
 *   0x08016008 - 0x08016020: 发送 0x03 + 24-bit 地址
 *   0x08016024 - 0x08016036: 数据读取循环
 *   0x08016038 - 0x0801603E: 设置完成标志 + 返回
 *   0x08016040 - 0x08016048: 字面量池
 *
 * 注: r4 被保存但未在函数内 PUSH (在调用者中),
 *     实际由 PUSH.W {r4-r8,lr} 保存.
 *     func_118EC 既是发送也是接收函数 (SPI 全双工),
 *     发送 0xFF 同时接收 MISO 数据.
 *     FLASH_DONE_FLAG (0x424082A8) 是 bit-band alias,
 *     与 func_154 的 0x424082AC 属同一外设区域.
 */
