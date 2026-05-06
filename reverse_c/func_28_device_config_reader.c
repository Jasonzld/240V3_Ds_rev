/**
 * @file func_28_device_config_reader.c
 * @brief 函数: Device_GetConfig — 读取设备配置信息
 * @addr  0x0800D458 - 0x0800D51C (196 bytes, 含字面量池)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 从全局设备配置结构体 (0x20004590) 读取设备信息,
 * 填充到调用者提供的两个输出结构体中。
 *
 * 参数:
 *   r0 = dev_info (输出结构体指针, 可为 NULL)
 *   r1 = dev_ext  (输出结构体指针, 可为 NULL)
 * 返回:
 *   0 = 操作完成 (DEV_CFG[0x24] = 0xFF → check_status → cbz 不取 → return 0)
 *   2 = 未操作/未就绪 (DEV_CFG[0x24] = 0 → check_status → cbz 取 → return 2)
 *   3 = 无操作 (两个输入指针均为 NULL)
 *
 * 全局设备配置结构体 (0x20004590, 至少 0x28 字节):
 *   [+0x00] cfg_00    (uint8_t)
 *   [+0x01] cfg_01    (uint8_t)
 *   [+0x02] cfg_02    (uint8_t)
 *   [+0x04] cfg_04    (uint16_t)
 *   [+0x06] cfg_06    (uint16_t)
 *   [+0x08] cfg_08    (uint8_t)
 *   [+0x09] cfg_09    (uint8_t)
 *   [+0x0C] cfg_0C    (uint8_t)
 *   [+0x10] cfg_10    (uint32_t)
 *   [+0x14] cfg_14    (uint8_t)
 *   [+0x18] cfg_18    (uint32_t)
 *   [+0x24] status    (uint8_t, 状态标志: 0=OK, 0xFF=错误)
 *
 * 调用:
 *   func_183C4() @ 0x080183C4 — 硬件检测/初始化
 *   func_18204() @ 0x08018204 — 子配置读取/验证
 *   func_17EDC() @ 0x08017EDC — 参数解析
 *   func_183F0() @ 0x080183F0 — TFT 背光回调/错误处理
 */

#include "stm32f4xx_hal.h"

/* 全局设备配置结构体 */
#define DEV_CFG  ((volatile uint8_t *)0x20004590)

/* 字段访问宏 (基于偏移的具名访问) */
#define DEV_CFG_BYTE(off)   (DEV_CFG[off])
#define DEV_CFG_HALF(off)   (*(volatile uint16_t *)(DEV_CFG + (off)))
#define DEV_CFG_WORD(off)   (*(volatile uint32_t *)(DEV_CFG + (off)))

/* dev_info 输出结构体字段 (通过 r4 指针) */
/* [+0x00] uint16_t  cfg_06_val   (来自 cfg[+0x06]) */
/* [+0x02] uint8_t   cfg_08_val   (来自 cfg[+0x08]) */
/* [+0x03] uint8_t   cfg_09_val   (来自 cfg[+0x09]) */
/* [+0x04] uint8_t   cfg_00_val   (来自 cfg[+0x00]) */
/* [+0x05] uint8_t   cfg_01_val   (来自 cfg[+0x01]) */
/* [+0x06] uint8_t   cfg_02_val   (来自 cfg[+0x02]) */
/* [+0x08] uint16_t  cfg_04_val   (来自 cfg[+0x04]) */

/* dev_ext 输出结构体字段 (通过 r5 指针) */
/* [+0x00] uint8_t   cfg_0C_val   (来自 cfg[+0x0C]) */
/* [+0x04] uint32_t  cfg_10_val   (来自 cfg[+0x10]) */
/* [+0x08] uint8_t   cfg_14_val   (来自 cfg[+0x14]) */
/* [+0x0C] uint32_t  cfg_18_val   (来自 cfg[+0x18]) */

/* 外部函数声明 */
extern uint32_t func_183C4(void);                       /* 0x080183C4 */
extern uint32_t func_18204(uint32_t h, uint32_t n, uint32_t mode, void *sp_buf);  /* 0x08018204 */
extern uint32_t func_17EDC(uint32_t v1, uint32_t v2);   /* 0x08017EDC */
extern void     func_183F0(void);                       /* 0x080183F0 */

/* ================================================================
 * Device_GetConfig() @ 0x0800D458
 *   r0 = dev_info (可为 NULL)
 *   r1 = dev_ext  (可为 NULL)
 *
 * 注: push {r3, r4, r5, r6, r7, lr} 中 r3 用于栈对齐 (8 字节),
 *     非 callee-saved 用途。
 * ================================================================ */
uint32_t Device_GetConfig(void *dev_info, void *dev_ext)
{
    uint32_t status;     /* r7 */
    uint32_t result;     /* r6 */
    uint32_t sp_val;     /* [sp] — 栈传递参数 */

    /* 两个指针均为 NULL → 直接返回 3 */
    if (dev_info == NULL && dev_ext == NULL) {       /* 0x0800D45E-60: cbnz r4,#next; cbnz r5,#next */
        return 3;                                    /* 0x0800D462-64: movs r0,#3; pop */
    }

    /* 清除状态标志 */
    DEV_CFG_BYTE(0x24) = 0;                         /* 0x0800D466-6A: movs r0,#0; ldr r1,=DEV_CFG; strb.w [r1,#0x24] */

    /* 硬件检测 */
    status = func_183C4();                           /* 0x0800D46E-72: bl func_183C4; mov r7,r0 */

    /* 若硬件检测返回 0, 跳过所有处理直接到 check_status */
    if (status == 0) {                               /* 0x0800D474-76: cmp r7,#0; beq */
        goto check_status;                           /* beq → 0x0800D508 */
    }

    /* 硬件检测成功 → 检查参数指针有效性 */
    if (dev_info != NULL) {                          /* 0x0800D478: cbnz r4, #read_config */
        goto read_config;
    }
    if (dev_ext == NULL) {                           /* 0x0800D47A-C: cmp r5,#0; beq */
        /* 两指针均为 NULL (不可达 — 已早返回 3, 但二进制保留此路径) */
        DEV_CFG_BYTE(0x24) = 0xFF;                   /* 0x0800D500-04: movs r0,#0xff; strb.w */
        goto check_status;
    }
    /* dev_ext != NULL → 落入 read_config */

read_config:
    /* 检查状态标志 */
    if (DEV_CFG_BYTE(0x24) == 0) {                   /* 0x0800D47E-86: ldrb.w r0,[r0,#0x24]; cmp r0,#0; bne */

                /* ---- 子配置读取 ---- */
                /* 调用子函数读取/验证详细配置 (参数 7=模式, r3=sp 传缓冲区) */
                sp_val = 0;                          /* [sp] 初始化为 0 */
                result = func_18204(status, 7, 0, &sp_val);  /* 0x0800D488-94: mov r3,sp; movs r2,#0;
                                                       *   movs r1,#7; mov r0,r7; bl func_18204; mov r6,r0 */

                if (result == 0) {                   /* 0x0800D496: cbnz r6, #err1 */
                    result = func_17EDC(sp_val, (uint32_t)DEV_CFG);  /* 0x0800D498-A0: ldr r1,=DEV_CFG;
                                                       *   ldr r0,[sp]; bl func_17EDC; mov r6,r0 */

                    if (result == 0) {               /* 0x0800D4A2: cbnz r6, #err2 */

                        /* 设置成功标志 */
                        DEV_CFG_BYTE(0x24) = 0xFF;  /* 0x0800D4A4-A8: movs r0,#0xff; strb.w */

                        /* ---- 填充 dev_info 结构体 ---- */
                        if (dev_info != NULL) {      /* 0x0800D4AC: cbz r4, #fill_dev_ext */
                            *(uint16_t *)((uint8_t *)dev_info + 0) = DEV_CFG_HALF(0x06);  /* [+0x00] */
                            *(uint8_t  *)((uint8_t *)dev_info + 2) = DEV_CFG_BYTE(0x08);  /* [+0x02] */
                            *(uint8_t  *)((uint8_t *)dev_info + 3) = DEV_CFG_BYTE(0x09);  /* [+0x03] */
                            *(uint8_t  *)((uint8_t *)dev_info + 4) = DEV_CFG_BYTE(0x00);  /* [+0x04] */
                            *(uint8_t  *)((uint8_t *)dev_info + 5) = DEV_CFG_BYTE(0x01);  /* [+0x05] */
                            *(uint8_t  *)((uint8_t *)dev_info + 6) = DEV_CFG_BYTE(0x02);  /* [+0x06] */
                            *(uint16_t *)((uint8_t *)dev_info + 8) = DEV_CFG_HALF(0x04);  /* [+0x08] */
                            /* 0x0800D4AE-D6 */
                        }

                        /* ---- 填充 dev_ext 结构体 ---- */
                        if (dev_ext != NULL) {       /* 0x0800D4D8: cbz r5, #done_ok */
                            *(uint32_t *)((uint8_t *)dev_ext + 4)  = DEV_CFG_WORD(0x10);  /* [+0x04] */
                            *(uint8_t  *)((uint8_t *)dev_ext + 0)  = DEV_CFG_BYTE(0x0C);  /* [+0x00] */
                            *(uint32_t *)((uint8_t *)dev_ext + 12) = DEV_CFG_WORD(0x18);  /* [+0x0C] */
                            *(uint8_t  *)((uint8_t *)dev_ext + 8)  = DEV_CFG_BYTE(0x14);  /* [+0x08] */
                            /* 0x0800D4DA-F0 */
                        }
                        goto check_status;              /* 0x0800D4F0: b #0x800d508 — 经 check_status 返回 0 (flag=0xFF) */
                    }
                    /* err2: 参数解析失败 */
                    func_183F0();                    /* 0x0800D4F2 */
                    goto check_status;
                }
                /* err1: 子配置读取失败 */
                func_183F0();                        /* 0x0800D4FA */
                goto check_status;
            }

check_status:
    /* 检查最终状态标志 */
    if (DEV_CFG_BYTE(0x24) != 0) {                  /* 0x0800D508-0E: ldrb.w r0,[r0,#0x24]; cbz r0,#else */
        return 0;                                    /* 0x0800D510-12: movs r0,#0; b #pop */
    }

done_ok:
    return 2;                                        /* 0x0800D514-16: movs r0,#2; b #pop (0x0800D464) */
}
