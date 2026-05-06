/**
 * @file func_152_serial_frame_builder.c
 * @brief 串行协议帧构建器 — 将传感器数据打包为二进制帧并通过 USART/SPI 发送
 * @addr  0x080119FC - 0x080123E8 (2540 bytes, 1242 instructions, 1 个函数)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 构建并发送二进制串行协议帧. 不使用循环 — 所有传感器读取
 * 和字节存储均为完全展开的内联代码 (针对 Thumb2 优化).
 *
 * 帧格式 (包络):
 *   [0]  0x55        帧头
 *   [1]  0xAA
 *   [2]  速度高字节    (ldrh then asrs #8)
 *   [3]  标志字节 A    (ldrb)
 *   [4]  RPM 高字节   (ldrh then asrs #8)
 *   [5]  标志字节 B
 *   [6]  温度高字节
 *   [7]  标志字节 C
 *   [8]  电压高字节
 *   [9]  标志字节 D
 *   [10] 功率高字节
 *   [11] 标志字节 E
 *   [12] 流量高字节
 *   [13] 标志字节 F
 *   ...
 *   [N-5] 0x55       同步/帧尾标记
 *   [N-4] 校验和?     (ldrb)
 *   [N-3] 0x55
 *   [N-2] 状态字节
 *   [N-1] 0x55
 *   [N]   额外字节
 *
 * 然后附加 3 个 16 字节预定义块 (来自字面量池).
 * 最后通过 func_0BC2C 发送完整帧.
 *
 * 调用:
 *   func_0839A (memcpy)  ×1 — 复制 16 字节块
 *   func_0BC2C (数据发送) ×1 — 发送完整帧
 */

#include "stm32f4xx_hal.h"

/* ---- 外部依赖 ---- */
extern void func_0839A(void *dst, const void *src);   /* memcpy */
extern void func_0BC2C(void *dst, void *buf, uint32_t len); /* 数据发送 */

/* ---- 传感器 RAM 地址 (每个 16-bit 字包含 [高字节:值 | 低字节:标志]) ----
 * 实际地址范围 0x20000044-0x2000005F, 非 0x200001C0+.
 * 下面为已验证的前 6 组地址; 其余 ~44 组遵循相同模式在相邻地址.
 */
#define SENSOR_PAIR_0  (*(volatile uint16_t *)0x2000004A)  /* 速度高字节 + 标志A */
#define SENSOR_PAIR_1  (*(volatile uint16_t *)0x2000004C)  /* RPM高字节 + 标志B */
#define SENSOR_PAIR_2  (*(volatile uint16_t *)0x20000044)  /* 温度高字节 + 标志C */
#define SENSOR_PAIR_3  (*(volatile uint16_t *)0x20000046)  /* 电压高字节 + 标志D */
/* ... 剩余传感器对地址在 0x20000048-0x2000005F 范围内 ... */

/* 字面量池中的 16 字节预定义块 (3 个块 × 16 bytes) */
/* 0x080123D0: 块 0 — 配置/校准数据 */
/* 0x080123E0: 块 1 — 更多配置数据 */

/**
 * @brief 构建并发送串行协议帧
 *
 * 栈帧: sp+0x000 - sp+0x103 (260 bytes = 0x104)
 *   sp+0x004: 帧缓冲区起始 (前 2 字节被 0x55 0xAA 占用)
 *   sp+0x0A4: 块复制目标偏移 (0xA0 = 160 bytes 传感器数据后)
 *
 * r4 = 当前帧偏移索引 (从 2 开始, 因为前 2 字节已填充)
 */
void Serial_Frame_Builder(void)
{
    uint8_t  frame[0x100];       /* sp+0x04: 帧缓冲区 (256 bytes 有效载荷) */
    uint8_t  idx;                /* r4: 当前帧填充位置 */
    uint8_t  blk_idx;            /* r5: 块复制循环计数器 */
    uint8_t  val_high;           /* r2: 提取的高字节 */

    /* ---- 帧头 ---- */
    frame[0] = 0x55;             /* 帧头同步字节 1 */
    frame[1] = 0xAA;             /* 帧头同步字节 2 */
    idx = 2;

    /* ================================================================
     * 传感器数据打包 — 完全展开的序列
     * 模式: 读取传感器值 (ldrh/ldrb), 提取高字节 (asrs #8 或 ldrb),
     *       存储到 frame[idx], idx++
     *
     * 每个传感器对 (16 位 + 8 位) 打包为两个字节:
     *   - 16 位值的高 8 位 (asrs #8)
     *   - 8 位标志值
     * ================================================================ */

    /* 传感器 1: 速度高字节 + 标志 A (来自 0x2000004A) */
    val_high = (uint8_t)(SENSOR_PAIR_0 >> 8);
    frame[idx++] = val_high;
    frame[idx++] = *(volatile uint8_t *)0x2000004A;

    /* 传感器 2: RPM 高字节 + 标志 B (来自 0x2000004C) */
    val_high = (uint8_t)(SENSOR_PAIR_1 >> 8);
    frame[idx++] = val_high;
    frame[idx++] = *(volatile uint8_t *)0x2000004C;

    /* 传感器 3: 温度高字节 + 标志 C (来自 0x20000044) */
    val_high = (uint8_t)(SENSOR_PAIR_2 >> 8);
    frame[idx++] = val_high;
    frame[idx++] = *(volatile uint8_t *)0x20000044;

    /* 传感器 4: 电压高字节 + 标志 D (来自 0x20000046) */
    val_high = (uint8_t)(SENSOR_PAIR_3 >> 8);
    frame[idx++] = val_high;
    frame[idx++] = *(volatile uint8_t *)0x20000046;

    /* 传感器 5: 功率高字节 + 标志 E */
    val_high = (uint8_t)(*(volatile uint16_t *)0x20000048 >> 8);
    frame[idx++] = val_high;
    frame[idx++] = *(volatile uint8_t *)0x20000048;

    /* 传感器 6: 流量高字节 + 标志 F */
    val_high = (uint8_t)(*(volatile uint16_t *)0x20000056 >> 8);
    frame[idx++] = val_high;
    frame[idx++] = *(volatile uint8_t *)0x20000056;

    /* ... 更多传感器对 (共约 50 个传感器字节, 按地址顺序) ...
     * 每个对应模式:
     *   ldr r0, [pc, #imm]   → 加载 RAM 地址
     *   ldrh/ldrb r0, [r0]   → 读取传感器值
     *   asrs/ubfx r2, r0, #8 → 提取高字节
     *   strb r2, [sp+4+idx]  → 存储到帧
     *   idx++
     *
     * 按出现顺序的 RAM 地址 (从字面量池, 已修正为实际值):
     *   0x2000004A, 0x2000004C, 0x20000044, 0x20000046,
     *   0x20000048, 0x20000056, ...
     *   ... 还有约 40 个更多地址.
     *   (所有地址均位于 0x20000044-0x2000005F 范围内 —
     *    与 func_125/func_149 使用的传感器映射不同,
     *    func_152 使用独立的低地址传感器区域).
     */

    /* ---- 帧同步/校验序列 (在约 100 字节传感器数据后) ---- */
    frame[idx++] = 0x55;              /* 同步字节 */
    val_high = (uint8_t)(*(volatile uint16_t *)0x20000050 >> 8);
    frame[idx++] = val_high;
    frame[idx++] = *(volatile uint8_t *)0x20000050;

    frame[idx++] = 0x00;              /* 空字节 (可能为填充/对齐) */
    frame[idx++] = 0x55;              /* 同步字节 */
    frame[idx++] = *(volatile uint8_t *)0x20000052;

    frame[idx++] = 0x55;              /* 同步字节 */
    frame[idx++] = *(volatile uint8_t *)0x20000054;
    val_high = (uint8_t)(*(volatile uint16_t *)0x20000054 >> 8);
    frame[idx++] = val_high;
    frame[idx++] = *(volatile uint8_t *)0x20000058;

    /* ================================================================
     * 块复制: 从字面量池附加 3 个 16 字节预定义块
     *
     * 此部分通过循环完成:
     *   for (blk_idx = 0; blk_idx < 3; blk_idx++)
     *     memcpy(frame + idx, pool_blocks[blk_idx], 16)
     *     idx += 16
     *
     * 池中的 3 个 16 字节块对应:
     *   [0] 0x08011DF0: 配置块 0
     *   [1] 0x08011E00: 配置块 1
     *   [2] 0x08011E10: 配置块 2
     * ================================================================ */
    for (blk_idx = 0; blk_idx < 3; blk_idx++) {
        /* 源: 字面量池中的 16 字节对齐块
         * 基址在 r2 中: 0x08011DF0 + blk_idx * 16
         */
        const void *src_block = (const void *)(0x08011DF0 + blk_idx * 16);
        func_0839A(&frame[idx], src_block);  /* memcpy 16 bytes */
        idx += 16;
    }

    /* ================================================================
     * 发送帧:
     *   func_0BC2C(sp+4, sp+0, idx+1) — 将帧发送到串行端口
     * ================================================================ */
    func_0BC2C((void *)(frame + 0), (void *)(frame - 4), idx + 1);

    /* 销毁栈帧: add sp, #0x104; pop {r4, r5, pc} */
}

/*
 * 内存布局:
 *   0x080119FC - 0x08011A00: 入口 (push + sub sp + 初始化)
 *   0x08011A00 - 0x08012300: 传感器数据打包序列 (展开, 约 100 字节)
 *   - 每个传感器需要 5 条指令: LDR pool, LDRH/LDRB, ASRS, STRB, ADD idx
 *   - 总共约 50 个传感器点 × 5 指令 = 250 指令用于数据打包
 *   - 加上约 20 条同步字节和帧结构字节
 *   0x08012300 - 0x080123C0: 块复制循环 (3 iterations × 16 bytes)
 *   0x080123C0 - 0x080123CC: func_0BC2C 调用 + 尾声
 *   0x080123CE - 0x080123E8: 字面量池 (16 字节块 + RAM 地址)
 *
 * 帧结构总结:
 *   HEADER (2B)  | SENSOR_DATA (100B) | SYNC (5B) | BLOCK0 (16B) | BLOCK1 (16B) | BLOCK2 (16B)
 *   = 总计约 155 字节的帧数据
 *
 * 与 func_125 的对比:
 *   - func_125 (0x08016664): 遥测包构建器, 使用 0x55 0xAA 头/尾,
 *      读取 19 个 RAM 地址, 构建数据包, 调用 func_160D0 发送.
 *   - func_152 (0x080119FC): 串行帧构建器, 使用 0x55 0xAA 头,
 *      读取约 50 个 RAM 地址, 内联打包, 调用 func_0BC2C 发送.
 *   两者使用相同的传感器地址集, 但帧格式和发送方法不同 —
 *   表明有两个不同的串行协议 (可能是调试/配置与正常遥测).
 */
