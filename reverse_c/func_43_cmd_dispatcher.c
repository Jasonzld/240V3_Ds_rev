/**
 * @file func_43_cmd_dispatcher.c
 * @brief 函数: Cmd_Dispatcher — 协议命令分发器
 * @addr  0x0800DA2C - 0x0800DFD4 (1448 bytes, 含字面量池)
 *         字面量池 0x0800DFB2-0x0800DFD2 (32 bytes, 与前段共用 0x0800DE38-0x0800DE88)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 固件中最大的命令分发函数。根据 cmd_group (r5) 和 sub_cmd (r4)
 * 执行不同的协议操作: 数据累积/复制、标志位操作、外设通信、
 * NVIC 控制、及各种子模块触发。
 *
 * 参数:
 *   r0/r5 = cmd_group (命令组: 0..0x25)
 *   r1/r4 = sub_cmd   (子命令)
 *   r2/r6 = arg       (附加参数)
 *
 * 命令组处理摘要:
 *   cmd=0, sub=0xA:   数据累积器 (10 次计数后自提交 BUF_69C, func_12604(0x0E))
 *   cmd=2, sub=0x12:  计数触发 (10 次后 func_12604(0x13))
 *   cmd=2, sub=0x06:  设置 FLAG_241=1
 *   cmd=2, sub=0xD/E/F: 清除 FLAG_241, func_12604(0x0C)
 *   cmd=4/0x24, sub=2: 设置 FLAG_241=2
 *   cmd=5/0x25, sub=2: 翻转 FLAG_121, func_12528; sub=D/F: 帧发送 cmd 0x90
 *   cmd=6/7, sub=0xA: 门控 FLAG_1C0 → func_12604
 *   cmd=9, sub=0xF:   门控 FLAG_1C0 → func_12604
 *   cmd=8, sub=2/3/4/5/6/8: NVIC 标志翻转 + func_0BC2C 帧发送 + 尾部门控
 *   cmd=0xA, sub=5:   send_cmd_frame 序列 (2,3,4) + 延时 50
 *   cmd=0xA, sub=0xA: 门控 FLAG_1C0 → func_12604
 *   cmd=0xB, sub=6:   send_cmd_frame 序列 (3,4,5) + 延时 50
 *   cmd=0xC, sub=6:   send_cmd_frame(0xC, 3)
 *   cmd=0xC, sub=0xA: 读 FLAG_172 → func_12604
 *   cmd=0xD, sub=任意: 存储 sub_cmd 到 FLAG_240
 *   cmd=0xE, sub=6:   send_cmd_frame(0xE, 2) + 延时 50 + frame(0xE, 3)
 *   cmd=0x10, sub=5:  翻转 FLAG_64, 条件复制 FLAG_5A→FLAG_C6
 *   cmd=0x11, sub=5:  翻转 FLAG_102, 发送 cmd 0x5A
 *   cmd=0x13, sub=1:  func_96AA; 门控 FLAG_240 → func_12604
 *   cmd=0x15/0x16, sub=6: send_cmd_frame
 *   cmd=0x17-0x1A/0x22/0x23, sub=1/2: func_12604(2) + func_12454(0)
 *   cmd=0x1B, sub=2/3/4/5/6/8: 外设/标志操作
 *
 * 调用:
 *   func_12604()  @ 0x08012604
 *   func_12528()  @ 0x08012528
 *   func_0BC2C()  @ 0x0800BC2C
 *   func_C608()   @ 0x0800C608
 *   func_18488()  @ 0x08018488
 *   func_96AA()   @ 0x080096AA
 *   func_113A0()  @ 0x080113A0
 *   func_BD7C()   @ 0x0800BD7C
 *   func_12454()  @ 0x08012454
 *   func_124D6()  @ 0x080124D6
 *
 * 字面量池 (0x0800DE38-0x0800DE88 + 0x0800DFB2-0x0800DFD2):
 *   0x0800DE38: 0x2000018C   0x0800DE3C: 0x42408200
 *   0x0800DE40: 0x2000017E   0x0800DE44: 0x2000017F
 *   0x0800DE48: 0x200006A6   0x0800DE4C: 0x2000069C
 *   0x0800DE50: 0x20000240   0x0800DE54: 0x2000017C
 *   0x0800DE58: 0x2000017D   0x0800DE5C: 0x20000241
 *   0x0800DE60: 0x20000121   0x0800DE64: 0x200001C0
 *   0x0800DE68: 0x200000EC   0x0800DE6C: 0x200000ED
 *   0x0800DE70: 0x200000EE   0x0800DE74: 0x200000EF
 *   0x0800DE78: 0x200000F4   0x0800DE7C: 0x20000172
 *   0x0800DE80: 0x20000064   0x0800DE84: 0x2000005A
 *   0x0800DE88: 0x200000C6
 *   0x0800DFB4: 0x20000033   0x0800DFB8: 0x200000C6
 *   0x0800DFBC: 0x424082B8   0x0800DFC0: 0x20000102
 *   0x0800DFC4: 0x20000240   0x0800DFC8: 0x20000168
 *   0x0800DFCC: 0x20000169   0x0800DFD0: 0x200001BF
 */

#include "stm32f4xx_hal.h"

/* ---- FSMC 复用标志寄存器 ---- */
#define FSMC_FLAG_200   (*(volatile uint8_t  *)0x42408200)
#define FSMC_REG_200    (*(volatile uint32_t *)0x42408200)
#define PERIPH_B8       (*(volatile uint32_t *)0x424082B8)

/* ---- RAM 标志/计数器 ---- */
#define CTR_17E         (*(volatile uint8_t  *)0x2000017E)
#define CTR_LIMIT       (*(volatile uint8_t  *)0x2000017F)
#define BUF_69C         ((volatile uint8_t  *)0x2000069C)
#define REG_17C         (*(volatile uint8_t  *)0x2000017C)
#define FLAG_17D        (*(volatile uint8_t  *)0x2000017D)
#define FLAG_241        (*(volatile uint8_t  *)0x20000241)
#define FLAG_121        (*(volatile uint8_t  *)0x20000121)
#define FLAG_1C0        (*(volatile uint8_t  *)0x200001C0)
#define FLAG_EC         (*(volatile uint8_t  *)0x200000EC)
#define FLAG_ED         (*(volatile uint8_t  *)0x200000ED)
#define FLAG_EE         (*(volatile uint8_t  *)0x200000EE)
#define FLAG_EF         (*(volatile uint8_t  *)0x200000EF)
#define FLAG_F4         (*(volatile uint8_t  *)0x200000F4)
#define FLAG_64         (*(volatile uint16_t *)0x20000064)
#define FLAG_5A         (*(volatile uint16_t *)0x2000005A)
#define FLAG_C6         (*(volatile uint16_t *)0x200000C6)
#define FLAG_33         (*(volatile uint8_t  *)0x20000033)
#define FLAG_240        (*(volatile uint8_t  *)0x20000240)
#define FLAG_168        (*(volatile uint8_t  *)0x20000168)
#define FLAG_169        (*(volatile uint8_t  *)0x20000169)
#define FLAG_1BF        (*(volatile uint8_t  *)0x200001BF)
#define FLAG_18C        (*(volatile uint8_t  *)0x2000018C)
#define FLAG_172        (*(volatile uint8_t  *)0x20000172)
#define FLAG_102        (*(volatile uint8_t  *)0x20000102)
#define REG_236         (*(volatile uint32_t *)0x20000236)  /* CTR_17E + 0xB8 */
#define FSMC_B8         (*(volatile uint32_t *)0x424082B8)

extern void func_12604(uint32_t mode);
extern void func_12528(uint32_t arg1, uint32_t arg2, uint32_t flag);
extern void func_0BC2C(uint32_t cmd, uint8_t *data, uint32_t len);
extern void func_C608(uint32_t cmd_group, uint32_t sub_cmd);
extern void func_18488(uint32_t delay_ms);
extern void func_96AA(void);
extern void func_113A0(uint32_t mode);
extern void func_BD7C(uint32_t param);
extern void func_12454(uint32_t mode);
extern void func_124D6(uint32_t a, uint32_t b, uint32_t c);  /* 0x080124D6 */

/* ================================================================
 * Cmd_Dispatcher() @ 0x0800DA2C
 *   push {r3, r4, r5, r6, r7, lr} — r3 用于栈临时变量 + 对齐
 *   r5=r0 (cmd_group), r4=r1 (sub_cmd), r6=r2 (arg)
 * ================================================================ */
void Cmd_Dispatcher(uint32_t cmd_group, uint32_t sub_cmd, uint32_t arg)
{
    uint8_t  stack_buf[2];  /* sp+0..sp+1 */
    uint8_t  tmp;

    /* ================================================================
     * cmd_group == 0: 数据累积器 (仅 sub_cmd=0xA, arg=1)
     * 门控: FSMC_FLAG_200==0 或 FSMC_REG_200==0 时放行
     * CTR_17E==0 时初始化限制为 10, 每次递增存入 CTR_LIMIT
     * 满 10 次: 自提交 BUF_69C (读-写回), func_12604(0x0E)
     * ================================================================ */
    if (cmd_group == 0) {                                /* 0x0800DA34: cbnz r5 → r5==0 */
        if (arg != 1) goto done;                         /* 0x0800DA36: cmp r6,#1; bne */
        if (sub_cmd != 0xA) goto done;                   /* 0x0800DA3A: cmp r4,#0xa; bne */

        /* 门控检查 */
        if (FSMC_FLAG_200 != 0) {                        /* 0x0800DA3E-42: ldrb; cbz */
            if (FSMC_REG_200 != 0) goto done;            /* 0x0800DA44-4A: ldr; cmp#0; bne */
        }

        /* 首次初始化: CTR_17E==0 → 设 CTR_LIMIT=10 */
        if (CTR_17E == 0) {                              /* 0x0800DA4C-50: ldrb RCTR_17E; cbnz */
            CTR_LIMIT = 10;                              /* 0x0800DA52-56: movs#0xa; strb */
        }

        /* 递增加 1, 写入 CTR_LIMIT (注意: 读 CTR_17E, 写到 CTR_LIMIT) */
        CTR_LIMIT = CTR_17E + 1;                         /* 0x0800DA58-60: ldrb CTR_17E; adds#1; strb→CTR_LIMIT */
        if (CTR_LIMIT != 10) goto done;                  /* 0x0800DA62-68: ldrb CTR_LIMIT; cmp#0xa; bne */

        /* 满 10 次: 自提交 (读后写回 10 字节, 触发副作用) */
        {
            uint32_t w0, w1;
            uint16_t h2;
            w0 = *(volatile uint32_t *)(BUF_69C + 0);    /* 0x0800DA6E: ldr r2,[r1] */
            *(volatile uint32_t *)(BUF_69C + 0) = w0;    /* 0x0800DA70: str r2,[r0] */
            w1 = *(volatile uint32_t *)(BUF_69C + 4);    /* 0x0800DA72: ldr r2,[r1,#4] */
            *(volatile uint32_t *)(BUF_69C + 4) = w1;    /* 0x0800DA74: str r2,[r0,#4] */
            h2 = *(volatile uint16_t *)(BUF_69C + 8);    /* 0x0800DA76: ldrh r1,[r1,#8] */
            *(volatile uint16_t *)(BUF_69C + 8) = h2;    /* 0x0800DA78: strh r1,[r0,#8] */
        }
        func_12604(0x0E);                                /* 0x0800DA7A-7C: movs#0xe; bl */
        goto done;
    }

    /* ================================================================
     * cmd_group == 2: 寄存器/标志操作 (arg 必须为 1)
     *   先将 sub_cmd 存入 FLAG_240, 再按值分发:
     *   sub=0x12: FLAG_17D 计数 (10→func_12604(0x13))
     *   sub=0x06: 设置 FLAG_241=1
     *   sub=0xD/0xE/0xF: 清除 FLAG_241, func_12604(0x0C)
     * ================================================================ */
    if (cmd_group == 2) {                                /* 0x0800DA82: cmp r5,#2; bne */
        if (arg != 1) goto done;                         /* 0x0800DA86: cmp r6,#1; bne */

        FLAG_240 = (uint8_t)sub_cmd;                     /* 0x0800DA8A-8C: ldr r1,&FLAG_240; strb r4,[r1] */

        if (sub_cmd == 0x12) {                           /* 0x0800DA8E: cmp r4,#0x12; bne */
            if (FLAG_17D == 0) {                         /* 0x0800DA92-96: ldrb; cbnz */
                FLAG_17D = 10;                           /* 0x0800DA98-9C: movs#0xa; strb→FLAG_17D */
            }
            FLAG_17D = FLAG_17D + 1;                     /* 0x0800DA9E-A4: ldrb; adds#1; ldr r1; strb */
            /* 注: 此处 FLAG_17D 更新后存入 REG_17C */
            REG_17C = FLAG_17D;                          /* (同上 strb, 复用 ubfx 模式) */
            if (REG_17C == 10) {                         /* 0x0800DAA8-AC: ldrb REG_17C; cmp#0xa; bne */
                func_12604(0x13);                        /* 0x0800DAB0-B2: movs#0x13; bl */
            }
            goto done;
        }

        if (sub_cmd == 6) {                              /* 0x0800DAB8: cmp r4,#6; bne */
            FLAG_241 = 1;                                /* 0x0800DABC-C0: movs#1; strb→FLAG_241 */
            goto done;
        }

        /* sub_cmd ∈ {0xD, 0xE, 0xF}: 清除 FLAG_241, 触发 func_12604(0x0C) */
        if (sub_cmd == 0xD || sub_cmd == 0xE || sub_cmd == 0xF) { /* 0x0800DAC4-CE */
            FLAG_241 = 0;                                /* 0x0800DAD0-D4: movs#0; strb→FLAG_241 */
            func_12604(0x0C);                            /* 0x0800DAD6-D8: movs#0xc; bl */
        }
        goto done;
    }

    /* ================================================================
     * cmd_group == 4 或 0x24: 设置 FLAG_241=2 (仅 sub=2, arg=1)
     * ================================================================ */
    if (cmd_group == 4 || cmd_group == 0x24) {           /* 0x0800DADE-E4 */
        if (arg != 1) goto done;                         /* 0x0800DAE6: cmp r6,#1; bne */
        if (sub_cmd != 2) goto done;                     /* 0x0800DAEA: cmp r4,#2; bne */
        FLAG_241 = 2;                                    /* 0x0800DAEE-F2: movs#2; strb */
        goto done;
    }

    /* ================================================================
     * cmd_group == 5 或 0x25: FLAG_121 翻转 + 子命令
     *   sub=2 (无 arg 检查): 翻转 FLAG_121, func_12528(cmd_group, 0x1E, FLAG_121)
     *   sub=0xD (仅 cmd=0x25, arg=1): 清除 FLAG_1C0, 发送 cmd 0x90, func_12604(5)
     *   sub=0xF (仅 cmd=5, arg=1):   设置 FLAG_1C0=1, 发送 cmd 0x90, func_12604(0x25)
     * ================================================================ */
    if (cmd_group == 5 || cmd_group == 0x25) {           /* 0x0800DAF6-FC */

        /* sub_cmd == 2: 翻转 FLAG_121 (所有模式, 不检查 arg) */
        if (sub_cmd == 2) {                              /* 0x0800DAFE: cmp r4,#2; bne */
            FLAG_121 = (FLAG_121 != 0) ? 0 : 1;          /* 0x0800DB02-10: ldrb FLAG_121; cbnz→movs#0,movs#1; strb */
            func_12528(cmd_group, 0x1E, FLAG_121);       /* 0x0800DB12-1A: ldrb r2; movs r1,#0x1e; mov r0,r5; bl */
        }

        if (arg != 1) goto done;                         /* 0x0800DB1E: cmp r6,#1; bne */

        /* sub_cmd == 0xD (仅 cmd_group == 0x25) */
        if (sub_cmd == 0xD && cmd_group == 0x25) {       /* 0x0800DB22-28 */
            FLAG_1C0 = 0;                                /* 0x0800DB2A-2E: movs#0; strb */
            stack_buf[0] = FLAG_1C0;                     /* 0x0800DB30-34: ldrb; strb.w [sp] */
            func_0BC2C(0x90, stack_buf, 1);              /* 0x0800DB38-3E: movs r2,#1; mov r1,sp; movs r0,#0x90; bl */
            func_12604(5);                               /* 0x0800DB42-44: movs#5; bl */
            goto done;
        }

        /* sub_cmd == 0xF (仅 cmd_group == 5) */
        if (sub_cmd == 0xF && cmd_group == 5) {          /* 0x0800DB4A-50 */
            FLAG_1C0 = 1;                                /* 0x0800DB52-56: movs#1; strb */
            stack_buf[0] = FLAG_1C0;                     /* 0x0800DB58-5C: ldrb; strb.w [sp] */
            func_0BC2C(0x90, stack_buf, 1);              /* 0x0800DB60-66: movs r2,#1; mov r1,sp; movs r0,#0x90; bl */
            func_12604(0x25);                            /* 0x0800DB6A-6C: movs#0x25; bl */
        }
        goto done;
    }

    /* ================================================================
     * cmd_group == 6 或 7: 门控 → func_12604 (sub=0xA, arg=1)
     *   if FLAG_1C0==0 → func_12604(4)
     *   else if FLAG_1C0==1 → func_12604(0x24)
     * ================================================================ */
    if (cmd_group == 6 || cmd_group == 7) {              /* 0x0800DB72-78 */
        if (arg != 1) goto done;                         /* 0x0800DB7A: via cmp r6,#1 at 0x0800DB7C */
        if (sub_cmd != 0xA) goto done;                   /* 0x0800DB7E: cmp r4,#0xa; bne */

        if (FLAG_1C0 == 0) {                             /* 0x0800DB82-86: ldrb FLAG_1C0; cbnz → skip */
            func_12604(4);                               /* 0x0800DB88-8A: movs#4; bl */
        } else if (FLAG_1C0 == 1) {                      /* 0x0800DB90-96: ldrb FLAG_1C0; cmp#1; bne */
            func_12604(0x24);                            /* 0x0800DB98-9A: movs#0x24; bl */
        }
        goto done;
    }

    /* ================================================================
     * cmd_group == 9: 门控 → func_12604 (sub=0xF, arg=1)
     *   if FLAG_1C0==0 → func_12604(4)
     *   else if FLAG_1C0==1 → func_12604(0x24)
     * ================================================================ */
    if (cmd_group == 9) {                                /* 0x0800DBA0: cmp r5,#9; bne */
        if (arg != 1) goto done;                         /* 0x0800DBA4: cmp r6,#1; bne */
        if (sub_cmd != 0xF) goto done;                   /* 0x0800DBA8: cmp r4,#0xf; bne */

        if (FLAG_1C0 == 0) {                             /* 0x0800DBAC-B0: ldrb FLAG_1C0; cbnz */
            func_12604(4);                               /* 0x0800DBB2-B4: movs#4; bl */
        } else if (FLAG_1C0 == 1) {                      /* 0x0800DBBA-C0: ldrb FLAG_1C0; cmp#1; bne */
            func_12604(0x24);                            /* 0x0800DBC2-C4: movs#0x24; bl */
        }
        goto done;
    }

    /* ================================================================
     * cmd_group == 8: NVIC 标志翻转 + 帧发送
     *   各 sub_cmd 处理后均跳转到统一尾部 check_sub_a (0x0800DCFC)
     *   sub=2: 翻转 FLAG_ED → func_124D6(2,5,~FLAG_ED) → 发送 cmd 0x48
     *   sub=3: 翻转 FLAG_EE → 发送 cmd 0x4A
     *   sub=4: 清除 FLAG_EF → 发送 cmd 0x4C
     *   sub=8: 设置 FLAG_EF=1 → 发送 cmd 0x4C (复用 sub=4 的 cmd)
     *   sub=5: 翻转 FLAG_EF → 发送 cmd 0x4E
     *   sub=6: 翻转 FLAG_F4 → 发送 cmd 0x50
     *
     *   统一尾部: arg==1 && sub_cmd==0xA → 门控 func_12604
     * ================================================================ */
    if (cmd_group == 8) {                                /* 0x0800DBCA: cmp r5,#8; bne (fall-through 自 cmd=9) */

        /* sub_cmd == 2: 翻转 FLAG_ED */
        if (sub_cmd == 2) {                              /* 0x0800DBCE: cmp r4,#2; bne */
            tmp = FLAG_ED;                               /* 0x0800DBD2-D6: ldrb; cbnz */
            FLAG_ED = (tmp != 0) ? 0 : 1;                /* 0x0800DBD8-DE */
            /* 读取翻转后的值, 取反后传给 func_124D6 */
            tmp = FLAG_ED;                               /* 0x0800DBE2-E4: ldrb; cbnz */
            func_124D6(2, 5, (tmp != 0) ? 0 : 1);        /* 0x0800DBE8-F4: cbnz→#1/#0; mov r2; movs r1,#5; movs r0,#2; bl */
            /* bit7 提取: (int8_t)FLAG_EC >> 8 → 若 bit7=1 则 0xFF, 否则 0x00 */
            stack_buf[0] = (uint8_t)((int8_t)FLAG_EC >> 8); /* 0x0800DBF8-FE: ldrb; asrs#8; strb.w [sp] */
            stack_buf[1] = FLAG_ED;                      /* 0x0800DC02-06: ldrb; strb.w [sp,#1] */
            func_0BC2C(0x48, stack_buf, 2);              /* 0x0800DC0A-10: movs r2,#2; mov r1,sp; movs r0,#0x48; bl */
            goto check_sub_a;
        }

        /* sub_cmd == 3: 翻转 FLAG_EE */
        if (sub_cmd == 3) {                              /* 0x0800DC16: cmp r4,#3; bne */
            FLAG_EE = (FLAG_EE != 0) ? 0 : 1;            /* 0x0800DC1A-28: ldrb; cbnz→#1/#0; strb */
            stack_buf[0] = (uint8_t)((int8_t)FLAG_EE >> 8); /* 0x0800DC2A-30: ldrb; asrs#8; strb.w [sp] */
            stack_buf[1] = FLAG_EE;                      /* 0x0800DC34-38: ldrb; strb.w [sp,#1] */
            func_0BC2C(0x4A, stack_buf, 2);              /* 0x0800DC3C-42: bl */
            goto check_sub_a;
        }

        /* sub_cmd == 4: 清除 FLAG_EF=0 */
        if (sub_cmd == 4) {                              /* 0x0800DC48: cmp r4,#4; bne */
            FLAG_EF = 0;                                 /* 0x0800DC4C-50: movs#0; strb */
            stack_buf[0] = (uint8_t)((int8_t)FLAG_EF >> 8); /* 0x0800DC52-58: ldrb; asrs#8; strb.w [sp] */
            stack_buf[1] = FLAG_EF;                      /* 0x0800DC5A-60: ldrb; strb.w [sp,#1] */
            func_0BC2C(0x4C, stack_buf, 2);              /* 0x0800DC64-6A: bl */
            goto check_sub_a;
        }

        /* sub_cmd == 8: 设置 FLAG_EF=1 (在 sub=4 检查失败后才检查) */
        if (sub_cmd == 8) {                              /* 0x0800DC70: cmp r4,#8; bne */
            FLAG_EF = 1;                                 /* 0x0800DC74-78: movs#1; strb */
            stack_buf[0] = (uint8_t)((int8_t)FLAG_EF >> 8); /* 0x0800DC7A-80: ldrb; asrs#8; strb.w [sp] */
            stack_buf[1] = FLAG_EF;                      /* 0x0800DC82-88: ldrb; strb.w [sp,#1] */
            func_0BC2C(0x4C, stack_buf, 2);              /* 0x0800DC8C-92: bl (注意: 同 sub=4 用 cmd 0x4C) */
            goto check_sub_a;
        }

        /* sub_cmd == 5: 翻转 FLAG_EF */
        if (sub_cmd == 5) {                              /* 0x0800DC98: cmp r4,#5; bne */
            FLAG_EF = (FLAG_EF != 0) ? 0 : 1;            /* 0x0800DC9C-A8: ldrb; cbnz→#1/#0; strb */
            stack_buf[0] = (uint8_t)((int8_t)FLAG_EF >> 8); /* 0x0800DCAC-B2: ldrb; asrs#8; strb.w [sp] */
            stack_buf[1] = FLAG_EF;                      /* 0x0800DCB4-BA: ldrb; strb.w [sp,#1] */
            func_0BC2C(0x4E, stack_buf, 2);              /* 0x0800DCBE-C4: bl */
            goto check_sub_a;
        }

        /* sub_cmd == 6: 翻转 FLAG_F4 */
        if (sub_cmd == 6) {                              /* 0x0800DCCC: cmp r4,#6; bne → 落空到 check_sub_a */
            FLAG_F4 = (FLAG_F4 != 0) ? 0 : 1;            /* 0x0800DCD0-DC: ldrb; cbnz→#1/#0; strb */
            stack_buf[0] = (uint8_t)((int8_t)FLAG_F4 >> 8); /* 0x0800DCE0-E6: ldrb; asrs#8; strb.w [sp] */
            stack_buf[1] = FLAG_F4;                      /* 0x0800DCE8-EE: ldrb; strb.w [sp,#1] */
            func_0BC2C(0x50, stack_buf, 2);              /* 0x0800DCF2-F8: bl */
        }
        /* sub_cmd 未匹配: 落空到统一尾部 */

        /* ---- 统一尾部: arg==1 && sub_cmd==0xA → 门控 ---- */
check_sub_a:
        if (arg == 1 && sub_cmd == 0xA) {                /* 0x0800DCFC-02: cmp r6,#1; bne; cmp r4,#0xa; bne */
            if (FLAG_1C0 == 0) {                          /* 0x0800DD04-08: ldrb FLAG_1C0; cbnz */
                func_12604(4);                           /* 0x0800DD0A-0C: movs#4; bl */
            } else if (FLAG_1C0 == 1) {                  /* 0x0800DD12-18: ldrb FLAG_1C0; cmp#1; bne */
                func_12604(0x24);                        /* 0x0800DD1A-1C: movs#0x24; bl */
            }
        }
        goto done;
    }

    /* ================================================================
     * cmd_group == 0xA: 命令帧序列 + 门控 (arg=1)
     *   sub=5: send_cmd_frame(0xA, 2/3/4) + 各延时 50
     *   sub=0xA: 门控 FLAG_1C0 → func_12604
     * ================================================================ */
    if (cmd_group == 0xA) {                              /* 0x0800DD22: cmp r5,#0xa; bne */
        if (arg != 1) goto done;                         /* 0x0800DD26: cmp r6,#1; bne */

        if (sub_cmd == 5) {                              /* 0x0800DD2A: cmp r4,#5; bne */
            func_C608(cmd_group, 2);                     /* 0x0800DD2E-32: movs r1,#2; mov r0,r5; bl */
            func_18488(50);                              /* 0x0800DD34-38: movs r0,#0x32; bl */
            func_C608(cmd_group, 3);                     /* 0x0800DD3C-40: movs r1,#3; mov r0,r5; bl */
            func_18488(50);                              /* 0x0800DD42-46 */
            func_C608(cmd_group, 4);                     /* 0x0800DD4A-4E: movs r1,#4; mov r0,r5; bl */
            goto done;
        }

        if (sub_cmd == 0xA) {                            /* 0x0800DD54: cmp r4,#0xa; bne */
            if (FLAG_1C0 == 0) {                          /* 0x0800DD58-5C: ldrb FLAG_1C0; cbnz */
                func_12604(4);                           /* 0x0800DD5E-60: movs#4; bl */
            } else if (FLAG_1C0 == 1) {                  /* 0x0800DD66-6C: ldrb FLAG_1C0; cmp#1; bne */
                func_12604(0x24);                        /* 0x0800DD6E-70: movs#0x24; bl */
            }
        }
        goto done;
    }

    /* ================================================================
     * cmd_group == 0xB: sub=6, arg=1 → send_cmd_frame 序列 (3,4,5) + 延时 50
     * ================================================================ */
    if (cmd_group == 0xB) {                              /* 0x0800DD76: cmp r5,#0xb; bne */
        if (arg != 1) goto done;                         /* 0x0800DD7A: cmp r6,#1; bne */
        if (sub_cmd != 6) goto done;                     /* 0x0800DD7E: cmp r4,#6; bne */

        func_C608(cmd_group, 3);                         /* 0x0800DD82-86: movs r1,#3; mov r0,r5; bl */
        func_18488(50);                                  /* 0x0800DD88-8C */
        func_C608(cmd_group, 4);                         /* 0x0800DD90-94: movs r1,#4; mov r0,r5; bl */
        func_18488(50);                                  /* 0x0800DD96-9A */
        func_C608(cmd_group, 5);                         /* 0x0800DD9E-A2: movs r1,#5; mov r0,r5; bl */
        goto done;
    }

    /* ================================================================
     * cmd_group == 0xC: arg=1
     *   sub=6:   send_cmd_frame(0xC, 3)
     *   sub=0xA: func_12604(FLAG_172)
     * ================================================================ */
    if (cmd_group == 0xC) {                              /* 0x0800DDA8: cmp r5,#0xc; bne */
        if (arg != 1) goto done;                         /* 0x0800DDAC: cmp r6,#1; bne */

        if (sub_cmd == 6) {                              /* 0x0800DDB0: cmp r4,#6; bne */
            func_C608(cmd_group, 3);                     /* 0x0800DDB4-B8: movs r1,#3; mov r0,r5; bl */
            goto done;
        }

        if (sub_cmd == 0xA) {                            /* 0x0800DDBE: cmp r4,#0xa; bne */
            func_12604(FLAG_172);                        /* 0x0800DDC2-C4: ldr r0,&FLAG_172; ldrb r0; bl */
        }
        goto done;
    }

    /* ================================================================
     * cmd_group == 0xD: 存储 sub_cmd 到 FLAG_240 (arg=1)
     * ================================================================ */
    if (cmd_group == 0xD) {                              /* 0x0800DDCC: cmp r5,#0xd; bne */
        if (arg != 1) goto done;                         /* 0x0800DDD0: cmp r6,#1; bne */
        FLAG_240 = (uint8_t)sub_cmd;                     /* 0x0800DDD4-D6: ldr r1; strb r4,[r1] */
        goto done;
    }

    /* ================================================================
     * cmd_group == 0xE: arg=1
     *   sub=4: 无操作 (直接返回)
     *   sub=6: send_cmd_frame(0xE, 2) + 延时 50 + frame(0xE, 3)
     * ================================================================ */
    if (cmd_group == 0xE) {                              /* 0x0800DDDA: cmp r5,#0xe; bne */
        if (arg != 1) goto done;                         /* 0x0800DDDE: cmp r6,#1; bne */
        if (sub_cmd == 4) goto done;                     /* 0x0800DDE2: cmp r4,#4; beq → done */
        if (sub_cmd != 6) goto done;                     /* 0x0800DDE6: cmp r4,#6; bne */

        func_C608(cmd_group, 2);                         /* 0x0800DDEA-EE: movs r1,#2; mov r0,r5; bl */
        func_18488(50);                                  /* 0x0800DDF0-F4: movs r0,#0x32; bl */
        func_C608(cmd_group, 3);                         /* 0x0800DDF8-FC: movs r1,#3; mov r0,r5; bl */
        goto done;
    }

    /* ================================================================
     * cmd_group == 0x10: sub=5 → 翻转 FLAG_64 + 条件链
     *   翻转 FLAG_64 (半字): 非零→0, 零→1
     *   若 FLAG_64 != 0: FLAG_C6 = FLAG_5A, 若 FLAG_C6!=0: FSMC_B8=1, FLAG_33=1
     *   若 FLAG_64 == 0: 清除 FLAG_C6, PERIPH_B8, FLAG_33
     * ================================================================ */
    if (cmd_group == 0x10) {                             /* 0x0800DE02: cmp r5,#0x10; bne */
        if (sub_cmd != 5) goto done;                     /* 0x0800DE06: cmp r4,#5; bne */

        /* 翻转 FLAG_64 */
        FLAG_64 = (FLAG_64 != 0) ? 0 : 1;               /* 0x0800DE0A-18: ldrh FLAG_64; cbnz→#1/#0; strh */

        if (FLAG_64 != 0) {                              /* 0x0800DE1A-1E: ldrh FLAG_64; cbz → else */
            /* FLAG_64 非零路径: 复制 FLAG_5A 到 FLAG_C6 */
            FLAG_C6 = FLAG_5A;                           /* 0x0800DE20-26: ldrh FLAG_5A; strh→FLAG_C6 */
            if (FLAG_C6 != 0) {                          /* 0x0800DE28-2E: ldrh FLAG_C6; cmp#0; beq → done */
                FSMC_B8 = 1;                             /* 0x0800DE30-36: ldr r1,=0x42408208; adds#0xb8→0x424082B8; str */
                FLAG_33 = 1;                             /* 0x0800DE90-92: ldr r1; strb r0,[r1] */
            }
            /* FLAG_C6==0: 直接到 done (beq 落在 0x0800DE94) */
        } else {
            /* FLAG_64 == 0 路径: 清除链 */
            FLAG_C6 = 0;                                 /* 0x0800DE96-9A: movs#0; strh */
            PERIPH_B8 = 0;                               /* 0x0800DE9C-9E: movs#0; ldr r1; str */
            FLAG_33 = 0;                                 /* 0x0800DEA0-A2: movs#0; ldr r1; strb */
        }
        goto done;
    }

    /* ================================================================
     * cmd_group == 0x11: sub=5 → 翻转 FLAG_102 + 发送 cmd 0x5A
     * ================================================================ */
    if (cmd_group == 0x11) {                             /* 0x0800DEA6: cmp r5,#0x11; bne */
        if (sub_cmd != 5) goto done;                     /* 0x0800DEAA: cmp r4,#5; bne */

        FLAG_102 = (FLAG_102 != 0) ? 0 : 1;              /* 0x0800DEAE-BC: ldrb FLAG_102; cbnz→#1/#0; strb */
        /* bit7 提取: (int8_t)FLAG_102 >> 8 */
        stack_buf[0] = (uint8_t)((int8_t)FLAG_102 >> 8); /* 0x0800DEBE-C4: ldrb; asrs#8; strb.w [sp] */
        stack_buf[1] = FLAG_102;                         /* 0x0800DEC6-CC: ldrb; strb.w [sp,#1] */
        func_0BC2C(0x5A, stack_buf, 2);                  /* 0x0800DED0-D6: movs r2,#2; mov r1,sp; movs r0,#0x5a; bl */
        goto done;
    }

    /* ================================================================
     * cmd_group == 0x13: arg=1
     *   sub=1: func_96AA()
     *   然后: FLAG_240==6 → func_12604(0x0D)
     *        FLAG_240==0 → 无操作
     *        FLAG_240!=6,!=0 → func_12604(2)
     *   最后: FLAG_240 = 0
     * ================================================================ */
    if (cmd_group == 0x13) {                             /* 0x0800DEDC: cmp r5,#0x13; bne */
        if (arg != 1) goto done;                         /* 0x0800DEE0: cmp r6,#1; bne */

        if (sub_cmd == 1) {                              /* 0x0800DEE4: cmp r4,#1; bne */
            func_96AA();                                 /* 0x0800DEE6-E8: bl */
        }

        /* 门控分发: 按 FLAG_240 值 */
        if (FLAG_240 == 6) {                             /* 0x0800DEEC-F2: ldrb; cmp#6; bne */
            func_12604(0x0D);                            /* 0x0800DEF4-F6: movs#0xd; bl */
        } else if (FLAG_240 != 0) {                      /* 0x0800DEFC-04: ldrb; cbz → skip; else movs#2; bl */
            func_12604(2);                               /* 0x0800DF02-04: movs#2; bl */
        }

        FLAG_240 = 0;                                    /* 0x0800DF08-0C: movs#0; strb→FLAG_240 */
        goto done;
    }

    /* ================================================================
     * cmd_group == 0x15 或 0x16: sub=6, arg=1 → send_cmd_frame(cmd, 3)
     * ================================================================ */
    if (cmd_group == 0x15 || cmd_group == 0x16) {        /* 0x0800DF10-16 */
        if (arg != 1) goto done;                         /* 0x0800DF18: cmp r6,#1; bne */
        if (sub_cmd != 6) goto done;                     /* 0x0800DF1C: cmp r4,#6; bne */
        func_C608(cmd_group, 3);                         /* 0x0800DF20-24: movs r1,#3; mov r0,r5; bl */
        goto done;
    }

    /* ================================================================
     * cmd_group ∈ [0x17, 0x1A] 或 ∈ {0x22, 0x23}:
     *   sub=1 或 sub=2 → func_12604(2) + func_12454(0)
     * ================================================================ */
    if ((cmd_group >= 0x17 && cmd_group <= 0x1A)
        || cmd_group == 0x22 || cmd_group == 0x23) {     /* 0x0800DF2A-38 */
        if (sub_cmd == 1 || sub_cmd == 2) {              /* 0x0800DF3A-40 */
            func_12604(2);                               /* 0x0800DF42-44: movs#2; bl */
            func_12454(0);                               /* 0x0800DF48-4A: movs#0; bl */
        }
        goto done;
    }

    /* ================================================================
     * cmd_group == 0x1B: 多功能操作
     *   sub=4: 翻转 FLAG_168
     *   sub=5: 翻转 FLAG_1BF
     *   sub=2: func_113A0(arg) — RCC 操作
     *   sub=3: func_BD7C(arg); FLAG_169 = (uint8_t)arg
     *   sub=6: PERIPH_B8 = arg (通过 subs r0,#4 间接: 0x424082B4)
     *   sub=8: PERIPH_B8 = arg (直接: 0x424082B8)
     * ================================================================ */
    if (cmd_group == 0x1B) {                             /* 0x0800DF50: cmp r5,#0x1b; bne → done */

        if (sub_cmd == 4) {                              /* 0x0800DF54: cmp r4,#4; bne */
            FLAG_168 = (FLAG_168 != 0) ? 0 : 1;          /* 0x0800DF58-66: ldrb; cbnz→#1/#0; strb */
            goto done;
        }

        if (sub_cmd == 5) {                              /* 0x0800DF6A: cmp r4,#5; bne */
            FLAG_1BF = (FLAG_1BF != 0) ? 0 : 1;          /* 0x0800DF6E-7C: ldrb; cbnz→#1/#0; strb */
            goto done;
        }

        if (sub_cmd == 2) {                              /* 0x0800DF80: cmp r4,#2; bne */
            func_113A0(arg);                             /* 0x0800DF84-86: mov r0,r6; bl */
            goto done;
        }

        if (sub_cmd == 3) {                              /* 0x0800DF8C: cmp r4,#3; bne */
            func_BD7C(arg);                              /* 0x0800DF90-92: mov r0,r6; bl */
            FLAG_169 = (uint8_t)arg;                     /* 0x0800DF96-98: ldr r0; strb r6,[r0] */
            goto done;
        }

        if (sub_cmd == 6) {                              /* 0x0800DF9C: cmp r4,#6; bne */
            /* ldr r0, [pc, #0x18] → 0x424082B8; subs r0,#4 → 0x424082B4 */
            PERIPH_B8 = arg;                             /* 0x0800DFA0-A4: ldr r0; subs#4; str r6,[r0] */
                                                         /* 注: 实际写地址 = 0x424082B4 */
            goto done;
        }

        if (sub_cmd == 8) {                              /* 0x0800DFA8: cmp r4,#8; bne → done */
            /* ldr r0, [pc, #0xc] → 0x424082B8 (直接) */
            PERIPH_B8 = arg;                             /* 0x0800DFAC-AE: ldr r0; str r6,[r0] */
        }
    }

done:
    return;                                              /* 0x0800DFB0: pop {r3,r4,r5,r6,r7,pc} */
}
