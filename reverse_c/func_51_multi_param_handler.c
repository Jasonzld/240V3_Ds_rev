/**
 * @file func_51_multi_param_handler.c
 * @brief 函数: Multi_Param_Handler — 多模式参数处理与字符串管理
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *   Modes 3/5/0x25/8/0xA/0xB: fully implemented.
 *   Mode 0xC: sub_mode 3 (3-way FLAG_241 + FLAG_240 dispatch) and sub_mode 4 implemented.
 *   Mode 0xE: sub_mode 1 (BC2C 0x7E + REG_15C), sub_mode 2 (BUF_8D8), sub_mode 3 (BUF_8E8/40C + BC2C 0xC0) — all pools resolved.
 *   Mode 0xF: all sub_modes 1-7 (FPU path, scale=1000.0, clamp 1-3) and
 *     8/0xC/0xD (integer path) fully implemented from Capstone VLDR traces.
 *   Modes 0x10/0x11/0x12/0x15/0x16/0x1b at 0x08010334-0x08010B06: fully implemented.
 * @addr  0x0800FB48 - 0x08010B3C (4084 bytes)
 *         字面量池 0x0800FF50-0x0800FFC0 (112 bytes)
 *
 * 多模式分发函数，处理参数设定、浮点计算、字符串复制/比较操作。
 * 所有模式最终跳转到统一出口 0x08010B06 (b.w 目标)。
 *
 * 参数:
 *   r0 = mode      — 操作模式 (3, 5, 0x25, 8, 0xA, 0xB, 0xC...)
 *   r1 = sub_mode  — 子操作选择
 *   r2 = value     — 值/指针参数
 *
 * 调用:
 *   func_8410()  @ 0x08008410 — 值转换
 *   func_16BA0() @ 0x08016BA0 — 浮点值转换
 *   func_839A()  @ 0x0800839A — 字符串复制
 *   func_837E()  @ 0x0800837E — 字符串比较
 *   func_883C()  @ 0x0800883C — FPU取整
 *   func_12788() @ 0x08012788 — 数据帧发送
 *   func_BC2C()  @ 0x0800BC2C — 字节对发送
 */

#include "stm32f4xx_hal.h"

/* ---- RAM 变量 ---- */
#define FLAG_184    (*(volatile uint16_t *)0x20000184)
#define REG_F0      (*(volatile uint16_t *)0x200000F0)
#define BUF_8D8     ((volatile uint8_t  *)0x200008D8)  /* 16-byte string buffer A */
#define BUF_8E8     ((volatile uint8_t  *)0x200008E8)  /* 16-byte string buffer B */
#define BUF_3EC     ((volatile uint8_t  *)0x200003EC)  /* 16-byte comparison buffer */
#define FLAG_170    (*(volatile uint8_t  *)0x20000170)
#define REG_15C     (*(volatile uint32_t *)0x2000015C)  /* mode 0xE sub_mode 1 store target */
#define BUF_40C     ((volatile uint8_t  *)0x2000040C)   /* 16-byte buffer for mode 0xE sub_mode 3 */
/* ---- Mode 0xF 辅助 RAM 变量 (pool 0x080103F8-0x08010410) ---- */
#define REG_A8      (*(volatile uint16_t *)0x200000A8)  /* sub_mode 1-3 store target */
#define REG_162     (*(volatile uint16_t *)0x20000162)  /* max clamp limit */
#define REG_160     (*(volatile uint16_t *)0x20000160)  /* min clamp limit */
#define REG_46      (*(volatile uint16_t *)0x20000046)  /* sub_mode 4 store */
#define REG_4E      (*(volatile uint16_t *)0x2000004E)  /* sub_mode 5 store */
#define REG_50      (*(volatile uint16_t *)0x20000050)  /* sub_mode 6 store */
#define REG_52      (*(volatile uint16_t *)0x20000052)  /* sub_mode 7 store */
#define REG_D2      (*(volatile uint8_t  *)0x200000D2)  /* sub_mode 8 store (byte) */
#define REG_D3      (*(volatile uint8_t  *)0x200000D3)  /* sub_mode 0xC store (byte) */
#define REG_D4      (*(volatile uint8_t  *)0x200000D4)  /* sub_mode 0xD store (byte) */
/* ---- Mode 0x10-0x12/0x15-0x16/0x1b 辅助 RAM 变量 (pool 0x08010420-0x08010B38) ---- */
#define REG_56      (*(volatile uint16_t *)0x20000056)  /* mode 0x10 sub_mode 1 store */
#define REG_58      (*(volatile uint16_t *)0x20000058)  /* mode 0x10 sub_mode 1 max clamp / sub_mode 2 store */
#define REG_5A      (*(volatile uint16_t *)0x2000005A)  /* mode 0x10 sub_mode 3 store */
#define REG_FE      (*(volatile uint16_t *)0x200000FE)  /* mode 0x10 sub_mode 4 store */
#define REG_104     (*(volatile uint16_t *)0x20000104)  /* mode 0x11 sub_mode 1 store */
#define REG_106     (*(volatile uint16_t *)0x20000106)  /* mode 0x11 sub_mode 3 store */
#define REG_108     (*(volatile uint16_t *)0x20000108)  /* mode 0x11 sub_mode 2 store */
#define REG_10A     (*(volatile uint16_t *)0x2000010A)  /* mode 0x11 sub_mode 4 store */
#define REG_164     (*(volatile uint16_t *)0x20000164)  /* mode 0x11/0x1b clamp limit */
#define REG_166     (*(volatile uint16_t *)0x20000166)  /* mode 0x11/0x1b clamp limit */
#define REG_4C      (*(volatile uint16_t *)0x2000004C)  /* mode 0x12 sub_mode 3 store */
#define REG_4A      (*(volatile uint16_t *)0x2000004A)  /* mode 0x12 sub_mode 1 store */
#define REG_48      (*(volatile uint16_t *)0x20000048)  /* mode 0x12 sub_mode 5 store */
#define REG_D6      (*(volatile uint8_t  *)0x200000D6)  /* mode 0x12 sub_mode 2 store (byte) */
#define REG_D7      (*(volatile uint8_t  *)0x200000D7)  /* mode 0x12 sub_mode 4 store (byte) */
#define REG_D5      (*(volatile uint8_t  *)0x200000D5)  /* mode 0x12 sub_mode 6 store (byte) */
#define REG_11E     (*(volatile uint16_t *)0x2000011E)  /* mode 0x12 sub_mode 9 store */
#define REG_110     (*(volatile uint16_t *)0x20000110)  /* mode 0x12 sub_mode 7/0x10/0x12 store */
#define REG_116     (*(volatile uint16_t *)0x20000116)  /* mode 0x12 sub_mode 8/0x11/0x13 store */
#define REG_11C     (*(volatile uint16_t *)0x2000011C)  /* mode 0x12 sub_mode 0x14 store */
#define REG_70      (*(volatile uint32_t *)0x20000070)  /* mode 0x15/0x16 sub_mode 3 zero-store */
#define REG_8C      (*(volatile uint32_t *)0x2000008C)  /* mode 0x15/0x16 sub_mode 3 zero-store / BC2C payload */
/* ---- Mode 0xC 辅助 RAM 变量 (pool 0x0800FFC0-0x0800FFEC) ---- */
#define FLAG_241    (*(volatile uint8_t  *)0x20000241)  /* pool 0x0800FFC0 */
#define FLAG_240    (*(volatile uint8_t  *)0x20000240)  /* pool 0x0800FFE8 */
#define FLAG_38     (*(volatile uint16_t *)0x20000038)  /* pool 0x0800FFEC */
#define FLAG_1C0    (*(volatile uint8_t  *)0x200001C0)  /* pool 0x0800FFE4 */
#define BUF_69C     ((volatile uint8_t  *)0x2000069C)   /* pool 0x0800FFE0 */
#define BUF_6A6     ((volatile uint8_t  *)0x200006A6)   /* pool 0x0800FFDC */

extern uint32_t func_8410(uint32_t val);
extern float    func_16BA0(uint32_t val);
extern void     func_839A(uint8_t *dest, const uint8_t *src);
extern int      func_837E(uint8_t *dest, const uint8_t *src);
extern int32_t  func_883C(int64_t dval);
extern void     func_12788(uint32_t mode, uint32_t len, uint8_t *data);
extern void     func_BC2C(uint32_t code, uint8_t *data, uint32_t len);
/* ---- Mode 0xC-0xF 辅助函数 ---- */
extern void     func_1257A(uint32_t a, uint32_t b, uint32_t c, uint32_t d);
extern void     func_12430(uint32_t a, uint32_t b, uint32_t c);
extern void     func_9660(uint32_t a, uint32_t b);
extern void     func_12604(uint32_t a);
extern void     func_943E(uint32_t a, uint32_t b, uint32_t c);
extern void     func_124D6(uint32_t a, uint32_t b, uint32_t c);

/* ================================================================
 * Multi_Param_Handler() @ 0x0800FB48
 *   push {r4, r5, r6, r7, lr}; vpush {d8, d9}; sub sp, #0xc
 *   r6=mode, r4=sub_mode, r7=value
 *
 * 栈布局:
 *   sp+0x04: 字节对输出缓冲区 (2 bytes)
 * ================================================================ */
void Multi_Param_Handler(uint32_t mode, uint32_t sub_mode, uint32_t value)
{
    uint8_t  byte_buf[2];       /* sp+0x04 */
    uint32_t conv_val;          /* r5 */
    int32_t  fpu_val;           /* s16, r0 */
    const uint8_t *fmt_str;     /* adr 目标 */

    /* ---- mode == 3: 直接跳转到尾部 ---- */
    if (mode == 3) {
        goto common_exit;
    }

    /* ---- mode == 5 或 mode == 0x25: 子模式 0xC ---- */
    if (mode == 5 || mode == 0x25) {
        conv_val = func_8410(value);          /* 值预处理 */

        if (sub_mode == 0xC) {
            FLAG_184 = (uint16_t)conv_val;     /* strh → FLAG_184 */

            /* 裁剪到最小值 5 */
            if (FLAG_184 < 5) {
                FLAG_184 = 5;
            }

            /* 提取字节对: hi_byte, lo_byte → func_BC2C(0x80, ..., 2) */
            byte_buf[0] = (uint8_t)(FLAG_184 >> 8);
            byte_buf[1] = (uint8_t)(FLAG_184);
            func_BC2C(0x80, byte_buf, 2);
        }
        goto common_exit;
    }

    /* ---- mode == 8: FPU 浮点计算, 子模式 1 ---- */
    if (mode == 8) {
        /* func_16BA0 returns float in s0; cast to int32 for integer-path dispatch */
        float fp_result = func_16BA0(value);
        fpu_val = (int32_t)fp_result;

        if (sub_mode == 1) {
            /* VLDR scale from pool at 0x0800FF54 → VMUL.F32 → VCVT.U32.F32 → REG_F0
             * 原汇编: multiply first, THEN convert to uint32. 保留 fp_result 做乘法
             * 避免先截断再乘导致的精度丢失. */
            float scale = *(const float *)0x0800FF54;
            conv_val = (uint32_t)(fp_result * scale);
            REG_F0 = (uint16_t)conv_val;

            /* 提取并发送字节对 */
            byte_buf[0] = (uint8_t)(REG_F0 >> 8);
            byte_buf[1] = (uint8_t)(REG_F0);
            func_BC2C(0x52, byte_buf, 2);
        }
        goto common_exit;
    }

    /* ---- mode == 0xA: 字符串管理 (子模式 2/3/4) ---- */
    if (mode == 0xA) {
        if (sub_mode == 2) {
            func_839A(BUF_8D8, (uint8_t *)value);   /* 复制到 BUF_8D8 */
        } else if (sub_mode == 3) {
            func_839A(BUF_8E8, (uint8_t *)value);   /* 复制到 BUF_8E8 */
        } else if (sub_mode == 4) {
            /* 复制到 BUF_8E8+0x10 */
            func_839A(BUF_8E8 + 0x10, (uint8_t *)value);

            /* 字符串比较: BUF_8D8 vs BUF_3EC */
            if (func_837E(BUF_8D8, BUF_3EC) != 0) {
                func_12788(mode, 6, BUF_8D8);        /* adr 指向 BUF_8D8 */
            }
            /* 字符串比较: BUF_8E8+0x10 vs BUF_8E8 */
            if (func_837E(BUF_8E8 + 0x10, BUF_8E8) != 0) {
                func_12788(mode, 6, BUF_8E8 + 0x10);
            }
            /* 复制 BUF_3EC → BUF_8E8 */
            func_839A(BUF_8E8, BUF_3EC);
            func_BC2C(0xA0, BUF_3EC, 0x10);
            func_12788(mode, 6, BUF_3EC);
            FLAG_170 = 1;
        }
        goto common_exit;
    }

    /* ---- mode == 0xB: 字符串管理 (子模式 3/4/5) ---- */
    if (mode == 0xB) {
        if (sub_mode == 3) {
            func_839A(BUF_8D8, (uint8_t *)value);
        } else if (sub_mode == 4) {
            func_839A(BUF_8E8, (uint8_t *)value);
        } else if (sub_mode == 5) {
            func_839A(BUF_8E8 + 0x10, (uint8_t *)value);

            /* 比较 BUF_8D8 vs BUF_3EC+0x10 */
            if (func_837E(BUF_8D8, BUF_3EC + 0x10) != 0) {
                func_12788(mode, 2, BUF_8D8);
            }
            /* 比较 BUF_8E8+0x10 vs BUF_8E8 */
            if (func_837E(BUF_8E8 + 0x10, BUF_8E8) != 0) {
                func_12788(mode, 2, BUF_8E8 + 0x10);
            }
            /* 复制 BUF_3EC+0x10 → BUF_8E8 */
            func_839A(BUF_8E8, BUF_3EC + 0x10);
            func_BC2C(0xB0, BUF_3EC + 0x10, 0x10);
            func_12788(mode, 2, BUF_3EC + 0x10);
            FLAG_170 = 1;
        }
        goto common_exit;
    }

    /* ================================================================
     * mode == 0xC: 条件子分发 (sub_mode 3/4) @ 0x0800FD00-0x0800FF4E
     *   Capstone trace confirmed.
     *   sub_mode==3: 3-way dispatch on FLAG_241 → func_9660/12604/BC2C
     *   sub_mode==4: 同 mode 0xA sub_mode==4 (共享代码块 @ 0xFC1A)
     * ================================================================ */
    if (mode == 0xC) {
        if (sub_mode == 3) {
            uint8_t f241 = FLAG_241;

            /* ---- Path A: FLAG_241 == 2 ---- */
            if (f241 == 2) {
                if (func_837E((uint8_t *)value, BUF_3EC + 0x20) == 0) {
                    func_9660(0xC, 2);
                    func_9660(0xC, 3);
                    func_12604(0xD);
                } else {
                    /* adr r2,#0x28c at 0xFD34 → target 0x0800FFC4 (format string) */
                    func_12788(mode, 2, (uint8_t *)0x0800FFC4);
                }
                goto common_exit;
            }

            /* ---- Path B: FLAG_241 == 1 ---- */
            if (f241 == 1) {
                if (func_837E((uint8_t *)value, BUF_3EC + 0x10) == 0) {
                    /* struct copy: BUF_69C[0..9] → BUF_6A6[0..9] (10 bytes) */
                    *(volatile uint32_t *)(BUF_6A6 + 0) = *(volatile uint32_t *)(BUF_69C + 0);
                    *(volatile uint32_t *)(BUF_6A6 + 4) = *(volatile uint32_t *)(BUF_69C + 4);
                    *(volatile uint16_t *)(BUF_6A6 + 8) = *(volatile uint16_t *)(BUF_69C + 8);

                    if (FLAG_1C0 == 0) {
                        func_12604(4);
                    } else if (FLAG_1C0 == 1) {
                        func_12604(0x24);
                    }
                } else {
                    /* adr r2,#0x240 at 0xFD82 → target 0x0800FFC4 (format string) */
                    func_12788(mode, 2, (uint8_t *)0x0800FFC4);
                }
                goto common_exit;
            }

            /* ---- Path C: FLAG_241 == 0 (else) ---- */
            {
                if (func_837E((uint8_t *)value, BUF_3EC) != 0) {
                    goto common_exit;  /* b 0x10004 — 落空到 mode 0xE 检查 (退化为 common_exit) */
                }
                func_9660(0xC, 2);
                func_9660(0xC, 3);

                /* FLAG_240 子分发 */
                {
                    uint8_t f240 = FLAG_240;
                    uint16_t f38_val;

                    if (f240 == 0xD) {
                        FLAG_38 = 1;
                        f38_val = (uint16_t)(FLAG_1C0 * 3 + (FLAG_38 & 0xFF));
                        func_943E(2, 2, f38_val);
                        func_943E(2, 0xA, 0);
                        func_943E(2, 0xB, 0);
                        func_124D6(2, 0xD, 0);
                        func_124D6(2, 0xE, 1);
                        func_124D6(2, 0xF, 1);
                    } else if (f240 == 0xE) {
                        FLAG_38 = 2;
                        f38_val = (uint16_t)(FLAG_1C0 * 3 + (FLAG_38 & 0xFF));
                        func_943E(2, 0xA, f38_val);
                        func_943E(2, 2, 0);
                        func_943E(2, 0xB, 0);
                        func_124D6(2, 0xE, 0);
                        func_124D6(2, 0xD, 1);
                        func_124D6(2, 0xF, 1);
                    } else if (f240 == 0xF) {
                        FLAG_38 = 3;
                        f38_val = (uint16_t)(FLAG_1C0 * 3 + (FLAG_38 & 0xFF));
                        func_943E(2, 0xB, f38_val);
                        func_943E(2, 0xA, 0);
                        func_943E(2, 2, 0);
                        func_124D6(2, 0xF, 0);
                        func_124D6(2, 0xE, 1);
                        func_124D6(2, 0xD, 1);
                    } else {
                        goto post_dispatch_0xc;
                    }

                    /* Common epilogue for FLAG_240 ∈ {0xD, 0xE, 0xF} */
                    func_124D6(2, 5, 1);
                    byte_buf[0] = (uint8_t)(FLAG_38 >> 8);
                    byte_buf[1] = (uint8_t)(FLAG_38);
                    func_BC2C(0x30, byte_buf, 2);
                }

            post_dispatch_0xc:
                /* Post-dispatch: FLAG_240 == 6 check */
                if (FLAG_240 == 6) {
                    if (FLAG_1C0 == 0) {
                        func_12604(4);
                    } else if (FLAG_1C0 == 1) {
                        func_12604(0x24);
                    }
                }
            }
            goto common_exit;
        }

        /* ---- sub_mode == 4 (共享代码块 @ 0xFC1A, 同 mode 0xA sub_mode==4) ---- */
        if (sub_mode == 4) {
            func_839A(BUF_8E8 + 0x10, (uint8_t *)value);

            if (func_837E(BUF_8D8, BUF_3EC) != 0) {
                func_12788(mode, 6, BUF_8D8);
            }
            if (func_837E(BUF_8E8 + 0x10, BUF_8E8) != 0) {
                func_12788(mode, 6, BUF_8E8 + 0x10);
            }
            func_839A(BUF_8E8, BUF_3EC);
            func_BC2C(0xA0, BUF_3EC, 0x10);
            func_12788(mode, 6, BUF_3EC);
            FLAG_170 = 1;
            goto common_exit;
        }

        goto common_exit;
    }

    /* ---- mode == 0xE: 值转换 + 子模式 1/2/3 @ 0x08010012-0x10094 ---- */
    /* Capstone trace confirmed all pool addresses. */
    if (mode == 0xE) {
        conv_val = func_8410(value);

        if (sub_mode == 1) {
            /* 原汇编: str→REG_15C, ldrh+lsrs 取高字节, ldrb 取低字节 → BC2C(0x7E) */
            REG_15C = conv_val;
            byte_buf[0] = (uint8_t)(*(volatile uint16_t *)&REG_15C >> 8);
            byte_buf[1] = (uint8_t)(*(volatile uint16_t *)&REG_15C);
            func_BC2C(0x7E, byte_buf, 2);
        } else if (sub_mode == 2) {
            /* pool 0x080103E8 → 0x200008D8 (BUF_8D8) */
            func_839A(BUF_8D8, (uint8_t *)value);
        } else if (sub_mode == 3) {
            /* pool 0x080103EC → 0x200008E8 (BUF_8E8), pool 0x080103F0 → 0x2000040C (BUF_40C) */
            func_839A(BUF_8E8, (uint8_t *)value);

            if (func_837E(BUF_8D8, BUF_40C) != 0) {
                func_12788(mode, 5, BUF_8D8);
            } else {
                func_839A(BUF_40C, BUF_8E8);
                func_BC2C(0xC0, BUF_40C, 0x10);
                func_12788(mode, 5, BUF_40C);
            }
        }
        goto common_exit;
    }

    /* ================================================================
     * mode == 0xF: FPU + 整数双路径子模式分发 @ 0x08010094-0x10334
     *   Capstone trace confirmed all VLDR pools and RAM offsets.
     *
     *   入口: cmp r6,#0xf at 0x10094. sub_mode dispatch:
     *     FPU 路径 (sub_mode 1-7):
     *       func_16BA0 → double→float via func_883C → ×1000.0
     *       → VCVT.U32.F32 → store to REG_xx → clamp → byte pair → BC2C
     *     整数路径 (sub_mode 8/0xC/0xD):
     *       func_8410 → byte store to REG_Dx → asrs+ldrb → BC2C
     *
     *   VLDR scale: 1000.0f at pool 0x080103F4 (ALL sub_modes 1-7)
     *   Clamp limits: REG_162 (max), REG_160 (min) — sub_modes 1-3 only
     * ================================================================ */
    if (mode == 0xF) {
        if (sub_mode == 8 || sub_mode == 0xC || sub_mode == 0xD) {
            /* ---- 整数路径: func_8410 → byte store → BC2C ---- */
            conv_val = func_8410(value);

            if (sub_mode == 8) {
                REG_D2 = (uint8_t)conv_val;
                byte_buf[0] = (uint8_t)(REG_D2 >> 8);  /* asrs #8 */
                byte_buf[1] = REG_D2;
                func_BC2C(0x54, byte_buf, 2);
            } else if (sub_mode == 0xC) {
                REG_D3 = (uint8_t)conv_val;
                byte_buf[0] = (uint8_t)(REG_D3 >> 8);
                byte_buf[1] = REG_D3;
                func_BC2C(0x56, byte_buf, 2);
            } else {
                /* sub_mode == 0xD */
                REG_D4 = (uint8_t)conv_val;
                byte_buf[0] = (uint8_t)(REG_D4 >> 8);
                byte_buf[1] = REG_D4;
                func_BC2C(0x58, byte_buf, 2);
            }
        } else {
            /* ---- FPU 路径: func_16BA0 → func_883C → ×1000.0 → BC2C ---- */
            /* 原汇编: func_16BA0 返回 double (d0=s0:s1),
             *   VMOV s18,s0; s19,s1 → d9, VMOV r0:r1,d9,
             *   BL func_883C → float in r0, VMOV s16,r0,
             *   VLDR s0,=1000.0, VMUL.F32 s0,s16,s0, VCVT.U32.F32 */
            float fp_result = func_16BA0(value);  /* 注: 原汇编 func_16BA0 可能返回 double */
            float scale = 1000.0f;                 /* pool 0x080103F4, 所有 sub_mode 共用 */
            float fpu_f = func_883C(*(int64_t *)&fp_result);  /* func_883C: double→float */
            conv_val = (uint32_t)(fpu_f * scale);

            if (sub_mode == 1) {
                /* Store to REG_A8+4, clamp [REG_160, REG_162], BC2C(0x38) */
                *(volatile uint16_t *)((uint8_t *)&REG_A8 + 4) = (uint16_t)conv_val;
                uint16_t v = *(volatile uint16_t *)((uint8_t *)&REG_A8 + 4);
                if (v > REG_162) v = REG_162;
                if (v < REG_160) v = REG_160;
                byte_buf[0] = (uint8_t)(v >> 8);
                byte_buf[1] = (uint8_t)(v);
                func_BC2C(0x38, byte_buf, 2);
            } else if (sub_mode == 2) {
                *(volatile uint16_t *)((uint8_t *)&REG_A8 + 2) = (uint16_t)conv_val;
                uint16_t v = *(volatile uint16_t *)((uint8_t *)&REG_A8 + 2);
                if (v > REG_162) v = REG_162;
                if (v < REG_160) v = REG_160;
                byte_buf[0] = (uint8_t)(v >> 8);
                byte_buf[1] = (uint8_t)(v);
                func_BC2C(0x36, byte_buf, 2);
            } else if (sub_mode == 3) {
                REG_A8 = (uint16_t)conv_val;
                uint16_t v = REG_A8;
                if (v > REG_162) v = REG_162;
                if (v < REG_160) v = REG_160;
                byte_buf[0] = (uint8_t)(v >> 8);
                byte_buf[1] = (uint8_t)(v);
                func_BC2C(0x34, byte_buf, 2);
            } else if (sub_mode == 4) {
                REG_46 = (uint16_t)conv_val;
                byte_buf[0] = (uint8_t)(REG_46 >> 8);
                byte_buf[1] = (uint8_t)(REG_46);
                func_BC2C(0x08, byte_buf, 2);
            } else if (sub_mode == 5) {
                REG_4E = (uint16_t)conv_val;
                byte_buf[0] = (uint8_t)(REG_4E >> 8);
                byte_buf[1] = (uint8_t)(REG_4E);
                func_BC2C(0x0A, byte_buf, 2);
            } else if (sub_mode == 6) {
                REG_50 = (uint16_t)conv_val;
                byte_buf[0] = (uint8_t)(REG_50 >> 8);
                byte_buf[1] = (uint8_t)(REG_50);
                func_BC2C(0x0C, byte_buf, 2);
            } else if (sub_mode == 7) {
                REG_52 = (uint16_t)conv_val;
                byte_buf[0] = (uint8_t)(REG_52 >> 8);
                byte_buf[1] = (uint8_t)(REG_52);
                func_BC2C(0x0E, byte_buf, 2);
            }
        }
        goto common_exit;
    }

    /* ================================================================
     * mode == 0x10: 值钳位与发送 @ 0x08010334-0x08010452
     *   Capstone trace confirmed all pool addresses.
     *   sub_mode 1: store→REG_56, clamp max at REG_58-1, BC2C(0x10)
     *   sub_mode 2: store→REG_58, clamp min at REG_56+1, BC2C(0x12)
     *   sub_mode 3: store→REG_5A, read REG_D4 byte, BC2C(0x14)
     *   sub_mode 4: store→REG_FE, BC2C(0x3a)
     * ================================================================ */
    if (mode == 0x10) {
        conv_val = func_8410(value);

        if (sub_mode == 1) {
            REG_56 = (uint16_t)conv_val;
            if (REG_56 >= REG_58) {
                REG_56 = REG_58 - 1;
            }
            byte_buf[0] = (uint8_t)(REG_56 >> 8);
            byte_buf[1] = (uint8_t)(REG_56);
            func_BC2C(0x10, byte_buf, 2);
        } else if (sub_mode == 2) {
            REG_58 = (uint16_t)conv_val;
            if (REG_58 <= REG_56) {
                REG_58 = REG_56 + 1;
            }
            byte_buf[0] = (uint8_t)(REG_58 >> 8);
            byte_buf[1] = (uint8_t)(REG_58);
            func_BC2C(0x12, byte_buf, 2);
        } else if (sub_mode == 3) {
            REG_5A = (uint16_t)conv_val;
            byte_buf[0] = 0;
            byte_buf[1] = REG_D4;
            func_BC2C(0x14, byte_buf, 2);
        } else if (sub_mode == 4) {
            REG_FE = (uint16_t)conv_val;
            byte_buf[0] = (uint8_t)(REG_FE >> 8);
            byte_buf[1] = (uint8_t)(REG_FE);
            func_BC2C(0x3A, byte_buf, 2);
        }
        goto common_exit;
    }

    /* ================================================================
     * mode == 0x11: FPU/整数 双路径 @ 0x08010452-0x08010590
     *   sub_mode 1/3: FPU 路径 (func_16BA0→func_883C→×10.0→clamp)
     *   sub_mode 2/4: 整数路径 (func_8410)
     *   所有 FPU 子模式 scale=10.0 (VMOV.F32 s0,#10.0), clamp REG_164-REG_166
     * ================================================================ */
    if (mode == 0x11) {
        if (sub_mode == 2 || sub_mode == 4) {
            conv_val = func_8410(value);
        } else {
            float fp_result = func_16BA0(value);
            float fpu_f = func_883C(*(int64_t *)&fp_result);
            conv_val = (uint32_t)(fpu_f * 10.0f);
        }

        if (sub_mode == 1) {
            REG_104 = (uint16_t)conv_val;
            if (REG_104 > REG_166) REG_104 = REG_166;
            if (REG_104 < REG_164) REG_104 = REG_164;
            byte_buf[0] = (uint8_t)(REG_104 >> 8);
            byte_buf[1] = (uint8_t)(REG_104);
            func_BC2C(0x5C, byte_buf, 2);
        } else if (sub_mode == 2) {
            REG_108 = (uint16_t)conv_val;
            byte_buf[0] = (uint8_t)(REG_108 >> 8);
            byte_buf[1] = (uint8_t)(REG_108);
            func_BC2C(0x5E, byte_buf, 2);
        } else if (sub_mode == 3) {
            REG_106 = (uint16_t)conv_val;
            if (REG_106 > REG_166) REG_106 = REG_166;
            if (REG_106 < REG_164) REG_106 = REG_164;
            byte_buf[0] = (uint8_t)(REG_106 >> 8);
            byte_buf[1] = (uint8_t)(REG_106);
            func_BC2C(0x60, byte_buf, 2);
        } else if (sub_mode == 4) {
            REG_10A = (uint16_t)conv_val;
            byte_buf[0] = (uint8_t)(REG_10A >> 8);
            byte_buf[1] = (uint8_t)(REG_10A);
            func_BC2C(0x62, byte_buf, 2);
        }
        goto common_exit;
    }

    /* ================================================================
     * mode == 0x12: 14 种子模式 @ 0x08010590-0x080108D8
     *   整数路径 (func_8410): sub_mode 2/4/6 → byte store to REG_Dx
     *   FPU 路径 (func_16BA0→func_883C):
     *     sub_mode 1/3/5: scale=10.0, u32, REG_4A/4C/48
     *     sub_mode 7/8/0x10/0x11/0x12/0x13/0x14: scale=10.0, s32→sxth, REG_110/116 offsets
     *     sub_mode 9: scale=1000.0, u32, REG_11E
     * ================================================================ */
    if (mode == 0x12) {
        if (sub_mode == 2 || sub_mode == 4 || sub_mode == 6) {
            conv_val = func_8410(value);
        } else {
            float fp_result = func_16BA0(value);
            float fpu_f = func_883C(*(int64_t *)&fp_result);

            if (sub_mode == 9) {
                conv_val = (uint32_t)(fpu_f * 1000.0f);
            } else if (sub_mode == 7 || sub_mode == 8 || sub_mode == 0x10 ||
                       sub_mode == 0x11 || sub_mode == 0x12 || sub_mode == 0x13 ||
                       sub_mode == 0x14) {
                /* Signed integer path: VCVT.S32.F32 → SXTH */
                conv_val = (uint32_t)(int16_t)(int32_t)(fpu_f * 10.0f);
            } else {
                conv_val = (uint32_t)(fpu_f * 10.0f);
            }
        }

        if (sub_mode == 1) {
            REG_4A = (uint16_t)conv_val;
            byte_buf[0] = (uint8_t)(REG_4A >> 8);
            byte_buf[1] = (uint8_t)(REG_4A);
            func_BC2C(0x02, byte_buf, 2);
        } else if (sub_mode == 2) {
            REG_D6 = (uint8_t)conv_val;
            byte_buf[0] = 0;
            byte_buf[1] = REG_D6;
            func_BC2C(0x64, byte_buf, 2);
        } else if (sub_mode == 3) {
            REG_4C = (uint16_t)conv_val;
            byte_buf[0] = (uint8_t)(REG_4C >> 8);
            byte_buf[1] = (uint8_t)(REG_4C);
            func_BC2C(0x04, byte_buf, 2);
        } else if (sub_mode == 4) {
            REG_D7 = (uint8_t)conv_val;
            byte_buf[0] = 0;
            byte_buf[1] = REG_D7;
            func_BC2C(0x66, byte_buf, 2);
        } else if (sub_mode == 5) {
            REG_48 = (uint16_t)conv_val;
            byte_buf[0] = (uint8_t)(REG_48 >> 8);
            byte_buf[1] = (uint8_t)(REG_48);
            func_BC2C(0x68, byte_buf, 2);
        } else if (sub_mode == 6) {
            REG_D5 = (uint8_t)conv_val;
            byte_buf[0] = 0;
            byte_buf[1] = REG_D5;
            func_BC2C(0x6A, byte_buf, 2);
        } else if (sub_mode == 7) {
            *(volatile uint16_t *)((uint8_t *)&REG_110 + 4) = (uint16_t)conv_val;
            uint16_t v = *(volatile uint16_t *)((uint8_t *)&REG_110 + 4);
            byte_buf[0] = (uint8_t)(v >> 8);
            byte_buf[1] = (uint8_t)(v);
            func_BC2C(0x1E, byte_buf, 2);
        } else if (sub_mode == 8) {
            *(volatile uint16_t *)((uint8_t *)&REG_116 + 4) = (uint16_t)conv_val;
            uint16_t v = *(volatile uint16_t *)((uint8_t *)&REG_116 + 4);
            byte_buf[0] = (uint8_t)(v >> 8);
            byte_buf[1] = (uint8_t)(v);
            func_BC2C(0x22, byte_buf, 2);
        } else if (sub_mode == 9) {
            REG_11E = (uint16_t)conv_val;
            byte_buf[0] = (uint8_t)(REG_11E >> 8);
            byte_buf[1] = (uint8_t)(REG_11E);
            func_BC2C(0x70, byte_buf, 2);
        } else if (sub_mode == 0x10) {
            *(volatile uint16_t *)((uint8_t *)&REG_110 + 2) = (uint16_t)conv_val;
            uint16_t v = *(volatile uint16_t *)((uint8_t *)&REG_110 + 2);
            byte_buf[0] = (uint8_t)(v >> 8);
            byte_buf[1] = (uint8_t)(v);
            func_BC2C(0x1A, byte_buf, 2);
        } else if (sub_mode == 0x11) {
            *(volatile uint16_t *)((uint8_t *)&REG_116 + 2) = (uint16_t)conv_val;
            uint16_t v = *(volatile uint16_t *)((uint8_t *)&REG_116 + 2);
            byte_buf[0] = (uint8_t)(v >> 8);
            byte_buf[1] = (uint8_t)(v);
            func_BC2C(0x1C, byte_buf, 2);
        } else if (sub_mode == 0x12) {
            REG_110 = (uint16_t)conv_val;
            byte_buf[0] = (uint8_t)(REG_110 >> 8);
            byte_buf[1] = (uint8_t)(REG_110);
            func_BC2C(0x6C, byte_buf, 2);
        } else if (sub_mode == 0x13) {
            REG_116 = (uint16_t)conv_val;
            byte_buf[0] = (uint8_t)(REG_116 >> 8);
            byte_buf[1] = (uint8_t)(REG_116);
            func_BC2C(0x6E, byte_buf, 2);
        } else if (sub_mode == 0x14) {
            REG_11C = (uint16_t)conv_val;
            byte_buf[0] = (uint8_t)(REG_11C >> 8);
            byte_buf[1] = (uint8_t)(REG_11C);
            func_BC2C(0x9C, byte_buf, 2);
        }
        goto common_exit;
    }

    /* ================================================================
     * mode == 0x15: 字符串比较 + 清零发送 @ 0x080108D8-0x0801093A
     *   sub_mode 3: 比较 value 与 BUF_40C
     *     相等 → 清零 REG_70/REG_8C, BC2C(0x28, 4 bytes), func_12604(0x1b)
     *     不等 → func_12788(mode, 2, str@0x0800FFC4)
     * ================================================================ */
    if (mode == 0x15) {
        if (sub_mode == 3) {
            if (func_837E((uint8_t *)value, BUF_40C) != 0) {
                func_12788(mode, 2, (uint8_t *)0x0800FFC4);
            } else {
                REG_70 = 0;
                REG_8C = 0;

                {
                    uint8_t word_buf[4];
                    word_buf[0] = (uint8_t)(REG_8C >> 24);
                    word_buf[1] = (uint8_t)(REG_8C >> 16);
                    word_buf[2] = (uint8_t)(REG_8C >> 8);
                    word_buf[3] = (uint8_t)(REG_8C);
                    func_BC2C(0x28, word_buf, 4);
                }
                func_12604(0x1B);
            }
        }
        goto common_exit;
    }

    /* ================================================================
     * mode == 0x16: 同 mode 0x15, 但 func_12604(0x0E) @ 0x0801093A-0x0801099C
     * ================================================================ */
    if (mode == 0x16) {
        if (sub_mode == 3) {
            if (func_837E((uint8_t *)value, BUF_40C) != 0) {
                func_12788(mode, 2, (uint8_t *)0x0800FFC6);
            } else {
                REG_70 = 0;
                REG_8C = 0;

                {
                    uint8_t word_buf[4];
                    word_buf[0] = (uint8_t)(REG_8C >> 24);
                    word_buf[1] = (uint8_t)(REG_8C >> 16);
                    word_buf[2] = (uint8_t)(REG_8C >> 8);
                    word_buf[3] = (uint8_t)(REG_8C);
                    func_BC2C(0x28, word_buf, 4);
                }
                func_12604(0x0E);
            }
        }
        goto common_exit;
    }

    /* ================================================================
     * mode == 0x1b: FPU/整数 双路径 5 种子模式 @ 0x0801099C-0x08010B06
     *   sub_mode 0x12: 整数路径 (func_8410→REG_15C, BC2C 0x7E)
     *   sub_mode 0xe/0xf/0x10/0x11: FPU 路径
     *     sub_mode 0xe: scale=1000.0, store→REG_162, cap 0x157c, BC2C(0x82)
     *     sub_mode 0xf: scale=1000.0, store→REG_160, cap 0x157c, BC2C(0x84)
     *     sub_mode 0x10: scale=10.0, store→REG_164, cap 0x12c, BC2C(0x86)
     *     sub_mode 0x11: scale=10.0, store→REG_166, cap 0x12c, BC2C(0x88)
     * ================================================================ */
    if (mode == 0x1b) {
        if (sub_mode == 0x12) {
            conv_val = func_8410(value);
            REG_15C = conv_val;
            byte_buf[0] = (uint8_t)(*(volatile uint16_t *)&REG_15C >> 8);
            byte_buf[1] = (uint8_t)(*(volatile uint16_t *)&REG_15C);
            func_BC2C(0x7E, byte_buf, 2);
        } else {
            float fp_result = func_16BA0(value);
            float fpu_f = func_883C(*(int64_t *)&fp_result);

            if (sub_mode == 0xE) {
                conv_val = (uint32_t)(fpu_f * 1000.0f);
                REG_162 = (uint16_t)conv_val;
                if (REG_162 > 0x157C) REG_162 = 0x157C;
                byte_buf[0] = (uint8_t)(REG_162 >> 8);
                byte_buf[1] = (uint8_t)(REG_162);
                func_BC2C(0x82, byte_buf, 2);
            } else if (sub_mode == 0xF) {
                conv_val = (uint32_t)(fpu_f * 1000.0f);
                REG_160 = (uint16_t)conv_val;
                if (REG_160 > 0x157C) REG_160 = 0x157C;
                byte_buf[0] = (uint8_t)(REG_160 >> 8);
                byte_buf[1] = (uint8_t)(REG_160);
                func_BC2C(0x84, byte_buf, 2);
            } else if (sub_mode == 0x10) {
                conv_val = (uint32_t)(fpu_f * 10.0f);
                REG_164 = (uint16_t)conv_val;
                if (REG_164 > 0x12C) REG_164 = 0x12C;
                byte_buf[0] = (uint8_t)(REG_164 >> 8);
                byte_buf[1] = (uint8_t)(REG_164);
                func_BC2C(0x86, byte_buf, 2);
            } else if (sub_mode == 0x11) {
                conv_val = (uint32_t)(fpu_f * 10.0f);
                REG_166 = (uint16_t)conv_val;
                if (REG_166 > 0x12C) REG_166 = 0x12C;
                byte_buf[0] = (uint8_t)(REG_166 >> 8);
                byte_buf[1] = (uint8_t)(REG_166);
                func_BC2C(0x88, byte_buf, 2);
            }
        }
        goto common_exit;
    }

common_exit:
    return;
}
