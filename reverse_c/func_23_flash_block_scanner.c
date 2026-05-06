/**
 * @file func_23_flash_block_scanner.c
 * @brief 函数: FlashBlock_FindSentinel — 闪存块哨兵扫描器
 * @addr  0x0800C7A0 - 0x0800C8B0 (272 bytes, ~90 指令)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 从两个不同的闪存偏移读取 4096 字节数据块, 扫描寻找
 * 哨兵标记 (0xFFFFFFFF), 返回哨兵前一个 32-bit 字的值。
 * 若未找到哨兵则返回 0。
 *
 * 操作流程:
 *   1. 从偏移 0x2000 读取 4096 字节到栈缓冲区
 *   2. 以 4 字节步进扫描, 查找 0xFFFFFFFF 哨兵
 *   3. 若找到: 返回哨兵前一个 32-bit 字
 *   4. 若未找到且位置有效: 返回 0 (哨兵在起始位置)
 *   5. 若未找到且位置无效: 从偏移 0x3000 再读 4096 字节
 *   6. 重复扫描/返回逻辑
 *
 * 调用:
 *   flash_read_buf() @ 0x08015FF8 — 闪存读取函数
 *     r0 = 目标缓冲区指针 (sp)
 *     r1 = 源偏移/大小参数1 (0x2000 或 0x3000)
 *     r2 = 读取大小 (0x1000 = 4096)
 *
 * 参数: 无 (隐式使用固定闪存偏移)
 * 返回: r0 = 哨兵前的 32-bit 字, 或 0
 *
 * 哨兵检测:
 *   将 4 字节大端打包为 32-bit: b0<<24 | b1<<16 | b2<<8 | b3
 *   若结果 == 0xFFFFFFFF: 当前 4 字节即为哨兵
 *   检测方式: val + 1 == 0 → val == 0xFFFFFFFF
 */

#include "stm32f4xx_hal.h"

/* 缓冲区大小 */
#define BUF_SIZE    0x1000   /* 4096 字节 */

/* 闪存源偏移 */
#define FLASH_OFFSET_A  0x2000
#define FLASH_OFFSET_B  0x3000

/* 哨兵值 */
#define SENTINEL_MARKER  0xFFFFFFFF

/* 外部函数声明 */
extern void flash_read_buf(void *buf, uint32_t offset, uint32_t size);  /* 0x08015FF8 */

/* ================================================================
 * FlashBlock_FindSentinel() @ 0x0800C7A0
 *   扫描两个闪存块, 寻找哨兵标记并返回前导数据
 *   返回: r0 = 哨兵前的 32-bit 值, 或 0
 *
 * 寄存器:
 *   r4 = 当前扫描偏移 (uint16_t, 0..4096)
 *   r5 = 当前 4 字节打包值
 *   r6 = 返回值 (哨兵前导字)
 *   sp = 4096 字节栈缓冲区
 * ================================================================ */
uint32_t FlashBlock_FindSentinel(void)
{
    uint8_t   buf[BUF_SIZE];      /* [sp] — 4096 字节栈缓冲区 (0x0800C7A2: sub sp,#0x1000) */
    uint32_t  pos;                /* r4 — 当前扫描位置 (字节偏移) */
    uint32_t  val;                /* r5 — 打包的 32-bit 值 */
    uint32_t  result;             /* r6 — 返回值 */
    uint8_t   b0, b1, b2, b3;     /* 临时: 4 字节解包 */

    result = 0;                                    /* 0x0800C7A6: movs r6, #0 */

    /* ================================================================
     * 阶段 1: 从闪存偏移 A 读取 (0x2000)
     * ================================================================ */
    flash_read_buf(buf, FLASH_OFFSET_A, BUF_SIZE);  /* 0x0800C7A8-B0:
                                                     *   mov.w r2,#0x1000
                                                     *   lsls r1,r2,#1 → 0x2000
                                                     *   mov r0,sp
                                                     *   bl #0x08015FF8 */

    /* 扫描哨兵: 以 4 字节步进 */
    pos = 0;                                       /* 0x0800C7B4: movs r4, #0 */

    while (pos < BUF_SIZE) {                       /* 0x0800C7E4-E8: cmp r4,#0x1000; blt */
        /* 读取 4 字节并大端打包 */
        b0 = buf[pos];                             /* 0x0800C7B8: ldrb.w r0,[sp,r4] */
        b1 = buf[pos + 1];                         /* 0x0800C7BE-C0: adds r0,r4,#1; ldrb.w */
        b2 = buf[pos + 2];                         /* 0x0800C7C8-CA: adds r0,r4,#2; ldrb.w */
        b3 = buf[pos + 3];                         /* 0x0800C7D2-D4: adds r0,r4,#3; ldrb.w */

        /* 大端打包: b0<<24 | b1<<16 | b2<<8 | b3 */
        val = (uint32_t)b0 << 24;                  /* 0x0800C7BC: lsls r1,r0,#0x18 */
        val |= (uint32_t)b1 << 16;                 /* 0x0800C7C4: add.w r1,r1,r0,lsl#16 */
        val |= (uint32_t)b2 << 8;                  /* 0x0800C7CE: add.w r1,r1,r0,lsl#8 */
        val |= (uint32_t)b3;                       /* 0x0800C7D8: adds r5,r1,r0 */

        /* 哨兵检测: val + 1 == 0 → val == 0xFFFFFFFF */
        if (val + 1 == 0) {                        /* 0x0800C7DA-DC: adds r0,r5,#1; cbnz */
            break;                                 /* 0x0800C7DE: b #exit_scan */
        }

        pos += 4;                                  /* 0x0800C7E0-E2: adds r0,r4,#4; uxth r4,r0 */
    }

    /* 扫描后处理 */
    if (pos == BUF_SIZE) {
        /* 扫描了整个缓冲区都未找到哨兵 */
        result = val;                              /* 0x0800C7F2: mov r6,r5 — 返回最后一个值 */
        goto done;                                 /* 0x0800C7F4: b #done */
    }

    /* 哨兵已找到, 检查位置有效性 */
    if (pos == 0 || pos >= BUF_SIZE) {             /* 0x0800C7F6-FE: cmp r4,#0; ble; cmp r4,#0x1000; bge */
        /* 哨兵在位置 0 或位置无效 → 阶段 2 */
        goto phase2;                               /* 0x0800C7FE: bge #phase2 */
    }

    /* 哨兵在有效位置: 回退 4 字节读取哨兵前的字 */
    pos -= 4;                                      /* 0x0800C800-02: subs r0,r4,#4; uxth r4,r0 */

    b0 = buf[pos];                                 /* 0x0800C804-08: ldrb.w r0,[sp,r4]; lsls r1,r0,#0x18 */
    b1 = buf[pos + 1];                             /* 0x0800C80A-10 */
    b2 = buf[pos + 2];                             /* 0x0800C814-1A */
    b3 = buf[pos + 3];                             /* 0x0800C81E-24 */

    /* 打包并返回 */
    result = (uint32_t)b0 << 24;                   /* 0x0800C808: lsls r1,r0,#0x18 */
    result |= (uint32_t)b1 << 16;                  /* 0x0800C810: add.w r1,r1,r0,lsl#16 */
    result |= (uint32_t)b2 << 8;                   /* 0x0800C81A: add.w r1,r1,r0,lsl#8 */
    result |= (uint32_t)b3;                        /* 0x0800C824: adds r6,r1,r0 */
    goto done;                                     /* 0x0800C826: b #done */

phase2:
    /* ================================================================
     * 阶段 2: 从闪存偏移 B 读取 (0x3000)
     * ================================================================ */
    flash_read_buf(buf, FLASH_OFFSET_B, BUF_SIZE);  /* 0x0800C828-32:
                                                     *   mov.w r2,#0x1000
                                                     *   mov.w r1,#0x3000
                                                     *   mov r0,sp
                                                     *   bl #0x08015FF8 */

    /* 第二轮扫描 */
    pos = 0;                                       /* 0x0800C836: movs r4, #0 */

    while (pos < BUF_SIZE) {                       /* 0x0800C866-6A: cmp r4,#0x1000; blt */
        b0 = buf[pos];                             /* 0x0800C83A-3E */
        b1 = buf[pos + 1];                         /* 0x0800C840-46 */
        b2 = buf[pos + 2];                         /* 0x0800C84A-50 */
        b3 = buf[pos + 3];                         /* 0x0800C854-5A */

        val = (uint32_t)b0 << 24;                  /* 0x0800C83E */
        val |= (uint32_t)b1 << 16;                 /* 0x0800C846 */
        val |= (uint32_t)b2 << 8;                  /* 0x0800C850 */
        val |= (uint32_t)b3;                       /* 0x0800C85A: adds r5,r1,r0 */

        if (val + 1 == 0) {                        /* 0x0800C85C-5E: adds r0,r5,#1; cbnz */
            break;
        }

        pos += 4;                                  /* 0x0800C862-64: adds r0,r4,#4; uxth r4,r0 */
    }

    /* 第二阶段后处理 */
    if (pos == BUF_SIZE) {                         /* 0x0800C86E-72: cmp r4,#0x1000; bne */
        result = val;                              /* 0x0800C874: mov r6,r5 */
        goto done;
    }

    if (pos == 0 || pos >= BUF_SIZE) {             /* 0x0800C878-80: cmp r4,#0; ble; cmp r4,#0x1000; bge */
        goto done;                                 /* result = 0 (from init) */
    }

    /* 回退 4 字节, 读哨兵前字 */
    pos -= 4;                                      /* 0x0800C882-84: subs r0,r4,#4; uxth r4,r0 */

    b0 = buf[pos];                                 /* 0x0800C886-8A */
    b1 = buf[pos + 1];                             /* 0x0800C88C-92 */
    b2 = buf[pos + 2];                             /* 0x0800C896-9C */
    b3 = buf[pos + 3];                             /* 0x0800C8A0-A6 */

    result = (uint32_t)b0 << 24;                   /* 0x0800C88A: lsls r1,r0,#0x18 */
    result |= (uint32_t)b1 << 16;                  /* 0x0800C892 */
    result |= (uint32_t)b2 << 8;                   /* 0x0800C89C */
    result |= (uint32_t)b3;                        /* 0x0800C8A6: adds r6,r1,r0 */

done:
    return result;                                 /* 0x0800C8A8-AE: mov r0,r6; add sp,#0x1000; pop {r4,r5,r6,pc} */
}
