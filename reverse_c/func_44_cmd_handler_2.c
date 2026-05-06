/**
 * @file func_44_cmd_handler_2.c
 * @brief 函数: Cmd_Handler_2 — 命令组 2 的深度处理器
 * DISASSEMBLY-TRACED. Verified against Capstone.
 * @addr  0x0800DFD4 - 0x0800E0BC (232 bytes, 含字面量池)
 *         字面量池 0x0800E08E-0x0800E0BA (44 bytes)
 *
 * 仅处理 cmd_group==2, arg==1 的命令。
 * sub_cmd==5: 主要数据路径 — 门控 + 双分支 (基于 FLAG_19) + 表查找 + 尾部调用
 * sub_cmd==2/0xA/0xB: 存储 sub_cmd 到 FLAG_240, 清除 FLAG_241, func_12604(0x0C)
 *
 * 参数:
 *   r0/r5 = cmd_group
 *   r1/r4 = sub_cmd
 *   r2/r6 = arg
 *
 * 调用:
 *   func_943E()  @ 0x0800943E — 计算操作 (参数: 2, 5, val)
 *   func_124D6() @ 0x080124D6 — NVIC 操作 (参数: 2, 6, flag)
 *   func_10BD0() @ 0x08010BD0 — 数据操作 (参数: FLAG_19)
 *   func_12604() @ 0x08012604 — 操作触发 (参数: 0x0C)
 *
 * 字面量池 (0x0800E08E-0x0800E0BA):
 *   0x0800E090: 0x200000EC (FLAG_EC)
 *   0x0800E094: 0x20000038 (FLAG_38, 半字)
 *   0x0800E098: 0x20000019 (FLAG_19)
 *   0x0800E09C: 0x200001C0 (FLAG_1C0)
 *   0x0800E0A0: 0x200000A8 (TBL_A8, 半字表基址)
 *   0x0800E0A4: 0x20000044 (FLAG_44, 半字)
 *   0x0800E0A8: 0x200000EF (FLAG_EF)
 *   0x0800E0AC: 0x200000F0 (FLAG_F0, 半字)
 *   0x0800E0B0: 0x200000F2 (FLAG_F2, 半字)
 *   0x0800E0B4: 0x20000240 (FLAG_240)
 *   0x0800E0B8: 0x20000241 (FLAG_241)
 */

#include "stm32f4xx_hal.h"

/* ---- RAM 标志 ---- */
#define FLAG_EC     (*(volatile uint8_t  *)0x200000EC)
#define FLAG_38     (*(volatile uint16_t *)0x20000038)
#define FLAG_19     (*(volatile uint8_t  *)0x20000019)
#define FLAG_1C0    (*(volatile uint8_t  *)0x200001C0)
#define FLAG_44     (*(volatile uint16_t *)0x20000044)
#define FLAG_EF     (*(volatile uint8_t  *)0x200000EF)
#define FLAG_F0     (*(volatile uint16_t *)0x200000F0)
#define FLAG_F2     (*(volatile uint16_t *)0x200000F2)
#define TBL_A8      ((volatile uint16_t *)0x200000A8)  /* 半字表基址, FLAG_38 为索引 */
#define FLAG_240    (*(volatile uint8_t  *)0x20000240)
#define FLAG_241    (*(volatile uint8_t  *)0x20000241)

extern void func_943E(uint32_t a, uint32_t b, uint32_t val);   /* 0x0800943E */
extern void func_124D6(uint32_t a, uint32_t b, uint32_t flag); /* 0x080124D6 */
extern void func_10BD0(uint32_t val);                           /* 0x08010BD0 */
extern void func_12604(uint32_t mode);                          /* 0x08012604 */

/* ================================================================
 * Cmd_Handler_2() @ 0x0800DFD4
 *   push {r4, r5, r6, lr}
 *   r5=r0 (cmd_group), r4=r1 (sub_cmd), r6=r2 (arg)
 * ================================================================ */
void Cmd_Handler_2(uint32_t cmd_group, uint32_t sub_cmd, uint32_t arg)
{
    uint16_t val;    /* r0 (临时) */

    /* 仅处理 cmd_group==2, arg==1 */
    if (cmd_group != 2) goto done;                       /* 0x0800DFDC: cmp r5,#2; bne */
    if (arg != 1) goto done;                             /* 0x0800DFE0: cmp r6,#1; bne */

    /* ================================================================
     * sub_cmd == 5: 主数据路径
     *   门控: FLAG_EC==0 且 FLAG_38!=0 时继续
     *   分支: FLAG_19==0 → Path B (设置 FLAG_19=1, 用 FLAG_1C0 计算)
     *         FLAG_19!=0 → Path A (清除 FLAG_19, 用 FLAG_1C0 计算)
     *   共同尾部: 表查找 TBL_A8[FLAG_38-1] → FLAG_44,
     *             条件复制 FLAG_F0 → FLAG_F2 (由 FLAG_EF 门控)
     *             func_10BD0(FLAG_19)
     * ================================================================ */
    if (sub_cmd == 5) {                                  /* 0x0800DFE4: cmp r4,#5; bne */

        /* 门控 1: FLAG_EC 必须为 0 */
        if (FLAG_EC != 0) goto done;                     /* 0x0800DFE8-EE: ldrb; cmp#0; bne */

        /* 门控 2: FLAG_38 必须非零 */
        if (FLAG_38 == 0) goto done;                     /* 0x0800DFF0-F6: ldrh; cmp#0; beq */

        /* ---- 双分支: 按 FLAG_19 分发 ---- */
        if (FLAG_19 != 0) {                              /* 0x0800DFF8-FC: ldrb; cbz */
            /* ==== Path A: FLAG_19 != 0 ==== */
            FLAG_19 = 0;                                 /* 0x0800DFFE-02: movs#0; strb */
            /* 计算 val = FLAG_19 + FLAG_1C0 * 2 = FLAG_1C0 * 2 */
            val = (uint8_t)(FLAG_1C0 * 2);               /* 0x0800E004-0E: ldrb FLAG_19; ldrb FLAG_1C0; add.w; uxtb */
            func_943E(2, 5, val);                        /* 0x0800E010-14: movs r1,#5; movs r0,#2; bl */
            func_124D6(2, 6, 1);                         /* 0x0800E018-1E: movs r2,#1; movs r1,#6; movs r0,#2; bl */
        } else {
            /* ==== Path B: FLAG_19 == 0 ==== */
            FLAG_19 = 1;                                 /* 0x0800E024-28: movs#1; strb */
            /* 计算 val = FLAG_19 + FLAG_1C0 * 2 = 1 + FLAG_1C0 * 2 */
            val = (uint8_t)(1 + FLAG_1C0 * 2);           /* 0x0800E02A-34: ldrb FLAG_19; ldrb FLAG_1C0; add.w; uxtb */
            func_943E(2, 5, val);                        /* 0x0800E036-3A: movs r1,#5; movs r0,#2; bl */
            func_124D6(2, 6, 0);                         /* 0x0800E03E-44: movs r2,#0; movs r1,#6; movs r0,#2; bl */
        }

        /* ---- 共同尾部 ---- */
        /* 表查找: FLAG_44 = TBL_A8[FLAG_38 - 1] */
        FLAG_44 = TBL_A8[FLAG_38 - 1];                   /* 0x0800E048-56: ldrh FLAG_38; subs#1; ldrh.w [TBL_A8, r0, lsl#1]; strh */

        /* 条件复制: 若 FLAG_EF != 0, 则 FLAG_F2 = FLAG_F0 */
        if (FLAG_EF != 0) {                              /* 0x0800E058-5C: ldrb; cbz */
            FLAG_F2 = FLAG_F0;                           /* 0x0800E05E-64: ldrh FLAG_F0; strh→FLAG_F2 */
        }

        /* 尾部调用 */
        func_10BD0(FLAG_19);                             /* 0x0800E066-6A: ldrb; bl */
        goto done;
    }

    /* ================================================================
     * sub_cmd != 5: 存储 sub_cmd + 条件清除
     *   总是: FLAG_240 = sub_cmd
     *   若 sub_cmd ∈ {2, 0xA, 0xB}: FLAG_241 = 0
     *   最终: func_12604(0x0C)
     * ================================================================ */
    FLAG_240 = (uint8_t)sub_cmd;                         /* 0x0800E070-72: ldr r1; strb r4,[r1] */

    if (sub_cmd == 2 || sub_cmd == 0xA || sub_cmd == 0xB) { /* 0x0800E074-7E */
        FLAG_241 = 0;                                    /* 0x0800E080-84: movs#0; strb */
    }

    func_12604(0x0C);                                    /* 0x0800E086-88: movs#0xc; bl */

done:
    return;                                              /* 0x0800E08C: pop {r4,r5,r6,pc} */
}
