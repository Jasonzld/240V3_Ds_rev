/**
 * @file func_157_flash_sector_writer.c
 * @brief Flash 扇区写入引擎 — 跨扇区边界的数据块编程
 * @addr  0x080160CC - 0x08016188 (188 bytes, 1 个函数)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 将数据块写入内部 Flash, 自动处理扇区边界.
 * 每个扇区操作前擦除, 然后按字节/半字编程.
 * 扇区大小: 0x1000 (4096 bytes = 4KB).
 *
 * 参数: r0 = src_data — 源数据缓冲区指针
 *       r1 = dst_offset — Flash 目标偏移 (高 4 bits = 扇区号, 低 12 bits = 扇区内偏移)
 *       r2 = byte_count — 要写入的字节数
 *
 * 调用:
 *   func_15F60 (Flash 扇区擦除)        ×1
 *   func_15FF8 (Flash 扇区读取)        ×1
 *   func_161A8 (Flash 字节编程)        ×2
 */

#include "stm32f4xx_hal.h"

/* ---- 外部依赖 ---- */
extern uint32_t func_15F60(uint32_t sector);              /* Flash 扇区擦除 */
extern void     func_15FF8(uint32_t base, uint32_t sector, uint32_t size); /* 扇区读取 */
extern void     func_161A8(uint32_t base, uint32_t offset, uint32_t size); /* 字节编程 */

/* ---- Flash 编程 RAM 缓冲区 ---- */
/* 池 0x08016188 = 0x2000B510 — flash 扇区操作的 RAM 缓冲基址 */
#define FLASH_BUF       ((volatile uint8_t *)0x2000B510)

/* ========================================================================
 * Flash_Sector_Writer (0x080160D0 - 0x08016186, ~182 bytes)
 *
 * 栈帧: PUSH.W {r4-r12, LR} — 10 寄存器保存
 *
 * 算法:
 *   while (byte_count > 0):
 *     1. 计算当前扇区剩余空间: remaining = 0x1000 - sector_offset
 *     2. 本次写入量 = min(byte_count, remaining)
 *     3. 读取当前扇区到缓冲区 (func_15FF8)
 *     4. 检查扇区是否已擦除 (全 0xFF)
 *     5. 若非全 0xFF, 擦除扇区 (func_15F60)
 *     6. 将源数据复制到扇区缓冲区
 *     7. 将扇区缓冲区编程到 Flash (func_161A8)
 *     8. byte_count -= write_size; src_data += write_size
 *     9. sector++; sector_offset = 0
 * ======================================================================== */
void Flash_Sector_Writer(const uint8_t *src_data, uint32_t dst_offset, uint32_t byte_count)
{
    uint32_t sector;         /* r8 — 当前扇区号 (dst_offset >> 12) */
    uint32_t sector_off;     /* sl — 扇区内偏移 (dst_offset & 0xFFF) */
    uint32_t chunk_size;     /* r4 — 本次写入字节数 */
    uint32_t i;              /* r5 — 循环计数器 */
    uint8_t *flash_ptr;      /* fp — Flash 基址指针 */

    flash_ptr  = FLASH_BUF;
    sector     = dst_offset >> 12;                         /* UBFX/LSR: 提取扇区号 */
    sector_off = dst_offset & 0xFFF;                       /* UBFX: 提取扇区内偏移 */

    /* 计算扇区剩余空间 */
    chunk_size = 0x1000 - sector_off;                      /* RSB: 4096 - sector_off */
    if (byte_count > chunk_size) {
        /* 跨越扇区边界 — 仅写入剩余空间 */
        /* chunk_size 保持原值 (UXTH r4, r6 — 已截断为 16 位) */
    } else {
        chunk_size = byte_count;
    }

    /* 主写入循环 */
    while (1) {
        /* 步骤 1: 读取扇区到 RAM 缓冲区 */
        func_15FF8(FLASH_BUF, sector << 12, 0x1000);

        /* 步骤 2: 检查扇区是否已擦除 (扫描 0xFF) */
        uint32_t is_erased = 1;                            /* r5 = 0 初始 */
        for (i = 0; i < chunk_size; i++) {
            if (flash_ptr[(sector << 12) + sector_off + i] != 0xFF) {
                is_erased = 0;
                break;
            }
        }

        /* 步骤 3: 若非全擦除, 执行扇区擦除 */
        if (!is_erased) {
            func_15F60(sector);                            /* 擦除扇区 */

            /* 步骤 4: 将源数据复制到扇区缓冲区 */
            for (i = 0; i < chunk_size; i++) {
                flash_ptr[(sector << 12) + sector_off + i] = src_data[i];
            }

            /* 步骤 5: 编程扇区 (RAM→Flash) */
            func_161A8(FLASH_BUF, sector << 12, 0x1000);
        } else {
            /* 扇区已擦除 — 直接编程目标范围 */
            func_161A8(src_data, dst_offset, chunk_size);
        }

        /* 步骤 6: 检查是否完成 */
        if (byte_count == chunk_size)
            break;

        /* 步骤 7: 推进到下一扇区 */
        sector++;
        sector_off = 0;
        src_data += chunk_size;
        dst_offset += chunk_size;
        byte_count -= chunk_size;
        chunk_size = (byte_count > 0x1000) ? 0x1000 : byte_count;
    }
}

/*
 * 内存布局:
 *   0x080160CC - 0x080160D0: 前导 POP {r4,pc} + NOP 填充 (前一个函数尾声)
 *   0x080160D0 - 0x080160DA: PUSH.W {r4-r12, LR} + MOV sb,r0 + MOV r7,r1 + MOV r6,r2
 *   0x080160DA - 0x080160F0: Flash 基址加载 + 扇区/偏移解析
 *   0x080160F0 - 0x0801611E: func_15FF8 调用 + 擦除检查循环
 *   0x0801611E - 0x0801614E: func_15F60 调用 + 数据复制循环
 *   0x0801614E - 0x0801616C: func_161A8 调用 + 完成检查
 *   0x0801616C - 0x08016186: 扇区推进 + 循环尾部
 *   0x0801617E - 0x08016182: POP.W {r4-r12, PC} 尾声
 *
 * 注: 此函数与 func_23 (Flash_Block_Scanner at 0x0800D730) 配合工作,
 *     func_23 负责扫描和验证 Flash 块, func_157 负责实际写入.
 *     func_57/func_58/func_59 为 Flash 读取相关函数.
 */
