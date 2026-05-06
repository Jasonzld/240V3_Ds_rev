/**
 * @file func_125_telemetry_packet_builder.c
 * @brief Telemetry Packet Builder -- assembles a 95-byte telemetry frame and sends 3 segments.
 * @addr  0x08016664 - 0x08016B1C (1208 bytes, including literal pools)
 *         Primary pool:   0x08016A78 - 0x08016AC2 (19 entries, 76 bytes)
 *         Secondary pool: 0x08016B14 - 0x08016B1A (1 entry, 4 bytes = 0x20000194)
 *
 * DISASSEMBLY-TRACED.  Every STRB/STRB.W has been traced backward through
 * LDR-literal -> LDRx [base,#off] -> extraction -> STRB to produce the
 * exact RAM source address and access width.
 *
 * Packet buffer layout (sp+4, 256 bytes, r4 = byte index):
 *
 *  Index   Source                                   Width    Extraction
 *  [  0]   const 0x55 (sync placeholder)             --       overwritten by counter byte3
 *  [  1]   const 0xAA (sync placeholder)             --       overwritten by counter byte2
 *  [  2]   0x20000197  (*(u32*)0x20000194 >> 24)     4-byte   LSRS #24
 *  [  3]   0x20000196  (*(u32*)0x20000194 >> 16)     4-byte   UBFX #16,#8  -- overwritten by counter byte1
 *  [  4]   0x20000195  (*(u16*)0x20000194 >>  8)     2-byte   UBFX #8,#8   -- overwritten by counter byte0
 *  [  5]   0x20000194  (*( u8*)0x20000194)            1-byte   raw
 *  [  6]   0x2000069D  (*(u16*)0x2000069C >>  8)     2-byte   ASRS #8  (signed)
 *  [  7]   0x2000069C  (*( u8*)0x2000069C)            1-byte   raw
 *  [  8]   0x2000069E  (*( u8*)(0x2000069C+2))        1-byte   raw
 *  [  9]   0x2000069F  (*( u8*)(0x2000069C+3))        1-byte   raw
 *  [ 10]   0x200006A0  (*( u8*)(0x2000069C+4))        1-byte   raw
 *  [ 11]   0x200006A2  (*( u8*)(0x2000069C+6))        1-byte   raw
 *  [ 12]   0x200006A3  (*( u8*)(0x2000069C+7))        1-byte   raw
 *  [ 13]   0x20000041  (*(u16*)0x20000040 >>  8)     2-byte   ASRS #8  (signed)
 *  [ 14]   0x20000040  (*( u8*)0x20000040)            1-byte   raw
 *  [ 15]   0x2000003F  (*(u16*)0x2000003E >>  8)     2-byte   ASRS #8  (signed)
 *  [ 16]   0x2000003E  (*( u8*)0x2000003E)            1-byte   raw
 *  [ 17]   0x2000003D  (*(u16*)0x2000003C >>  8)     2-byte   UBFX #8,#8
 *  [ 18]   0x2000003C  (*( u8*)0x2000003C)            1-byte   raw
 *  [ 19]   0x2000003B  (*(u16*)0x2000003A >>  8)     2-byte   UBFX #8,#8
 *  [ 20]   0x2000003A  (*( u8*)0x2000003A)            1-byte   raw
 *  [ 21]   0x200000D9  (*( u8*)0x200000D9)            1-byte   raw
 *  [ 22]   0x2000008F  (*(u32*)0x2000008C >> 24)     4-byte   LSRS #24
 *  [ 23]   0x2000008E  (*(u32*)0x2000008C >> 16)     4-byte   UBFX #16,#8
 *  [ 24]   0x2000008D  (*(u16*)0x2000008C >>  8)     2-byte   UBFX #8,#8
 *  [ 25]   0x2000008C  (*( u8*)0x2000008C)            1-byte   raw
 *  [ 26]   0x2000012B  (*(u32*)0x20000128 >> 24)     4-byte   LSRS #24
 *  [ 27]   0x2000012A  (*(u32*)0x20000128 >> 16)     4-byte   UBFX #16,#8
 *  [ 28]   0x20000129  (*(u16*)0x20000128 >>  8)     2-byte   UBFX #8,#8
 *  [ 29]   0x20000128  (*( u8*)0x20000128)            1-byte   raw
 *  [ 30]   0x20000077  (*(u32*)0x20000074 >> 24)     4-byte   LSRS #24
 *  [ 31]   0x20000076  (*(u32*)0x20000074 >> 16)     4-byte   UBFX #16,#8
 *  [ 32]   0x20000075  (*(u16*)0x20000074 >>  8)     2-byte   UBFX #8,#8
 *  [ 33]   0x20000074  (*( u8*)0x20000074)            1-byte   raw
 *  [ 34]   0x20000057  (*(u16*)0x20000056 >>  8)     2-byte   ASRS #8  (signed)
 *  [ 35]   0x20000056  (*( u8*)0x20000056)            1-byte   raw
 *  [ 36]   0x20000059  (*(u16*)0x20000058 >>  8)     2-byte   ASRS #8  (signed)
 *  [ 37]   0x20000058  (*( u8*)0x20000058)            1-byte   raw
 *  [ 38]   0x200006A7  (*(u16*)0x200006A6 >>  8)     2-byte   ASRS #8  (signed)
 *  [ 39]   0x200006A6  (*( u8*)0x200006A6)            1-byte   raw
 *  [ 40]   0x200006A8  (*( u8*)(0x200006A6+2))        1-byte   raw
 *  [ 41]   0x200006A9  (*( u8*)(0x200006A6+3))        1-byte   raw
 *  [ 42]   0x200006AA  (*( u8*)(0x200006A6+4))        1-byte   raw
 *  [ 43]   0x200006AC  (*( u8*)(0x200006A6+6))        1-byte   raw
 *  [ 44]   0x200006AD  (*( u8*)(0x200006A6+7))        1-byte   raw
 *  [ 45]   0x200000AD  (*(u16*)(0x200000A8+4) >> 8)   2-byte   ASRS #8  (signed)
 *  [ 46]   0x200000AC  (*( u8*)(0x200000A8+4))        1-byte   raw
 *  [ 47]   0x200000AB  (*(u16*)(0x200000A8+2) >> 8)   2-byte   ASRS #8  (signed)
 *  [ 48]   0x200000AA  (*( u8*)(0x200000A8+2))        1-byte   raw
 *  [ 49]   0x200000A9  (*(u16*)(0x200000A8+0) >> 8)   2-byte   ASRS #8  (signed)
 *  [ 50]   0x200000A8  (*( u8*)(0x200000A8+0))        1-byte   raw
 *  [ 51]   0x20000019  (*( u8*)0x20000019)            1-byte   raw
 *  [ 52]   0x20000038  (*( u8*)0x20000038)            1-byte   raw
 *  [53..68]  block copy[0] from 0x200003EC          16 bytes  memcpy
 *  [69..84]  block copy[1] from 0x200003FC          16 bytes  memcpy
 *  [ 85]   0x2000013B  (*(u32*)0x20000138 >> 24)     4-byte   LSRS #24
 *  [ 86]   0x2000013A  (*(u32*)0x20000138 >> 16)     4-byte   UBFX #16,#8
 *  [ 87]   0x20000139  (*(u16*)0x20000138 >>  8)     2-byte   UBFX #8,#8
 *  [ 88]   0x20000138  (*( u8*)0x20000138)            1-byte   raw
 *  [ 89]   0x2000013F  (*(u32*)0x2000013C >> 24)     4-byte   LSRS #24
 *  [ 90]   0x2000013E  (*(u32*)0x2000013C >> 16)     4-byte   UBFX #16,#8
 *  [ 91]   0x2000013D  (*(u16*)0x2000013C >>  8)     2-byte   UBFX #8,#8
 *  [ 92]   0x2000013C  (*( u8*)0x2000013C)            1-byte   raw
 *  [ 93]   const 0xAA (tail marker)
 *  [ 94]   const 0x55 (tail marker)
 *
 * After assembly, buf[0..3] are overwritten by the post-increment counter:
 *   buf[0] = byte3 (counter+1 >> 24)
 *   buf[1] = byte2 (counter+1 >> 16)
 *   buf[2] = byte1 (counter+1 >>  8)
 *   buf[3] = byte0 (counter+1 >>  0)
 *
 * Three send segments (func_160D0):
 *   1. buf,  0x10000 + (rem << 7),  95 bytes
 *   2. buf,  0x02000 + (idx << 2),   4 bytes   (idx = lo16(counter) after inc, idx=(val-1)&0x3FF)
 *   3. buf,  0x03000 + (idx << 2),   4 bytes   (same idx)
 *
 * Call signatures:
 *   extern void func_839A(uint8_t *dst, const void *src);   @ 0x0800839A
 *   extern void func_160D0(uint8_t *buf, uint32_t offset, uint32_t len); @ 0x080160D0
 */

/*----------------------------------------------------------------------------
 * Literal-pool RAM addresses -- verified from 19-word pool at 0x08016A78
 * and 1-word pool at 0x08016B14.
 *----------------------------------------------------------------------------*/
/* 0x08016A78 */  #define RAM_0x20000194  (*(volatile uint32_t *)0x20000194U)
                 /* Also serves as: packet counter, UDIV/MLS input,
                    uint16_t index for send segments 2 & 3 */
/* 0x08016A7C */  #define RAM_0x2000069C  (*(volatile uint32_t *)0x2000069CU)
                 /* Used as a struct base: bytes at +0..+7 */
/* 0x08016A80 */  #define RAM_0x20000040  (*(volatile uint16_t *)0x20000040U)
/* 0x08016A84 */  #define RAM_0x2000003E  (*(volatile uint16_t *)0x2000003EU)
/* 0x08016A88 */  #define RAM_0x2000003C  (*(volatile uint16_t *)0x2000003CU)
/* 0x08016A8C */  #define RAM_0x2000003A  (*(volatile uint16_t *)0x2000003AU)
/* 0x08016A90 */  #define RAM_0x200000D9  (*(volatile  uint8_t *)0x200000D9U)
/* 0x08016A94 */  #define RAM_0x2000008C  (*(volatile uint32_t *)0x2000008CU)
/* 0x08016A98 */  #define RAM_0x20000128  (*(volatile uint32_t *)0x20000128U)
/* 0x08016A9C */  #define RAM_0x20000074  (*(volatile uint32_t *)0x20000074U)
/* 0x08016AA0 */  #define RAM_0x20000056  (*(volatile uint16_t *)0x20000056U)
/* 0x08016AA4 */  #define RAM_0x20000058  (*(volatile uint16_t *)0x20000058U)
/* 0x08016AA8 */  #define RAM_0x200006A6  (*(volatile uint16_t *)0x200006A6U)
                 /* Used as a struct base: bytes at +0..+7 */
/* 0x08016AAC */  #define RAM_0x200000A8  (*(volatile uint16_t *)0x200000A8U)
                 /* Used as a struct base: three hword+byte pairs at +0,+2,+4 */
/* 0x08016AB0 */  #define RAM_0x20000019  (*(volatile  uint8_t *)0x20000019U)
/* 0x08016AB4 */  #define RAM_0x20000038  (*(volatile uint32_t *)0x20000038U)
                 /* Only byte[0] is read (raw ldrb) */
/* 0x08016AB8 */  #define BLOCK_SRC_BASE  0x200003ECU
                 /* Base of 2x16-byte block copy */
/* 0x08016ABC */  #define RAM_0x20000138  (*(volatile uint32_t *)0x20000138U)
/* 0x08016AC0 */  #define RAM_0x2000013C  (*(volatile uint32_t *)0x2000013CU)
/* 0x08016B14 */  /* 0x20000194 (secondary pool, same as primary) */


extern void func_839A(uint8_t *dst, const void *src);  /* memcpy 16 bytes, @0x0800839A */
extern void func_160D0(uint8_t *buf, uint32_t offset, uint32_t len); /* @0x080160D0 */


void Telemetry_Packet_Builder(void)
{
    uint8_t  buf[0x100];          /* sp+4         */
    uint8_t  idx = 0;             /* r4 -- byte index into buf */
    uint8_t  blk;                 /* r5 -- block-copy loop counter */
    uint32_t tmp32;
    uint16_t tmp16;

    /*====================================================================
     * SYNC HEADER PLACEHOLDERS
     * Written now but overwritten by counter bytes at the end.
     *====================================================================*/
    /* @0x0801666C-0x0801667C */
    buf[idx++] = 0x55;            /* buf[0] -- will be overwritten */
    buf[idx++] = 0xAA;            /* buf[1] -- will be overwritten */

    /*====================================================================
     * SECTION A:  Pre-loop data bytes (buf[2..52] = 51 bytes)
     * Each group shows the pool entry, access width, offset, and
     * extraction, verified from the Capstone disassembly.
     *====================================================================*/

    /*--- Group 1: 0x20000194 as 32-bit big-endian @0x0801667E-0x080166B6 ---*/
    /* buf[2] = byte3, LSRS #24 */
    tmp32 = RAM_0x20000194;
    buf[idx++] = (uint8_t)(tmp32 >> 24);                      /* 0x20000197 */

    /* buf[3] = byte2, UBFX #16,#8 */
    tmp32 = RAM_0x20000194;
    buf[idx++] = (uint8_t)(tmp32 >> 16);                      /* 0x20000196 */

    /* buf[4] = byte1, LDRH + UBFX #8,#8 */
    tmp16 = *(volatile uint16_t *)0x20000194U;
    buf[idx++] = (uint8_t)(tmp16 >> 8);                       /* 0x20000195 */

    /* buf[5] = byte0, LDRB raw */
    buf[idx++] = *(volatile uint8_t *)0x20000194U;            /* 0x20000194 */

    /*--- Group 2: 0x2000069C struct (7 bytes) @0x080166B8-0x08016718 ---*/
    /* buf[6] = *(u16*)0x2000069C, ASRS #8 (signed byte from hword) */
    tmp16 = *(volatile uint16_t *)0x2000069CU;
    buf[idx++] = (uint8_t)((int16_t)tmp16 >> 8);              /* 0x2000069D */

    /* buf[7] = raw byte @0x2000069C */
    buf[idx++] = *(volatile uint8_t *)0x2000069CU;            /* 0x2000069C */

    /* buf[8] = raw byte @0x2000069E (base+2) */
    buf[idx++] = *(volatile uint8_t *)(0x2000069CU + 2);      /* 0x2000069E */

    /* buf[9] = raw byte @0x2000069F (base+3) */
    buf[idx++] = *(volatile uint8_t *)(0x2000069CU + 3);      /* 0x2000069F */

    /* buf[10] = raw byte @0x200006A0 (base+4) */
    buf[idx++] = *(volatile uint8_t *)(0x2000069CU + 4);      /* 0x200006A0 */

    /* buf[11] = raw byte @0x200006A2 (base+6) */
    buf[idx++] = *(volatile uint8_t *)(0x2000069CU + 6);      /* 0x200006A2 */

    /* buf[12] = raw byte @0x200006A3 (base+7) */
    buf[idx++] = *(volatile uint8_t *)(0x2000069CU + 7);      /* 0x200006A3 */

    /*--- Group 3: 0x20000040 as 16-bit signed @0x0801671A-0x08016736 ---*/
    /* buf[13] = *(u16*)0x20000040, ASRS #8 */
    tmp16 = RAM_0x20000040;
    buf[idx++] = (uint8_t)((int16_t)tmp16 >> 8);              /* 0x20000041 */

    /* buf[14] = raw byte @0x20000040 */
    buf[idx++] = *(volatile uint8_t *)0x20000040U;            /* 0x20000040 */

    /*--- Group 4: 0x2000003E as 16-bit signed @0x08016738-0x08016754 ---*/
    /* buf[15] = *(u16*)0x2000003E, ASRS #8 */
    tmp16 = RAM_0x2000003E;
    buf[idx++] = (uint8_t)((int16_t)tmp16 >> 8);              /* 0x2000003F */

    /* buf[16] = raw byte @0x2000003E */
    buf[idx++] = *(volatile uint8_t *)0x2000003EU;            /* 0x2000003E */

    /*--- Group 5: 0x2000003C as 16-bit unsigned @0x08016756-0x08016774 ---*/
    /* buf[17] = *(u16*)0x2000003C, UBFX #8,#8 */
    tmp16 = RAM_0x2000003C;
    buf[idx++] = (uint8_t)(tmp16 >> 8);                       /* 0x2000003D */

    /* buf[18] = raw byte @0x2000003C */
    buf[idx++] = *(volatile uint8_t *)0x2000003CU;            /* 0x2000003C */

    /*--- Group 6: 0x2000003A as 16-bit unsigned @0x08016776-0x08016794 ---*/
    /* buf[19] = *(u16*)0x2000003A, UBFX #8,#8 */
    tmp16 = RAM_0x2000003A;
    buf[idx++] = (uint8_t)(tmp16 >> 8);                       /* 0x2000003B */

    /* buf[20] = raw byte @0x2000003A */
    buf[idx++] = *(volatile uint8_t *)0x2000003AU;            /* 0x2000003A */

    /*--- Group 7: 0x200000D9 as 8-bit @0x08016796-0x080167A2 ---*/
    /* buf[21] = raw byte @0x200000D9 */
    buf[idx++] = RAM_0x200000D9;                              /* 0x200000D9 */

    /*--- Group 8: 0x2000008C as 32-bit big-endian @0x080167A4-0x080167E4 ---*/
    /* buf[22] = byte3, LSRS #24 */
    tmp32 = RAM_0x2000008C;
    buf[idx++] = (uint8_t)(tmp32 >> 24);                      /* 0x2000008F */

    /* buf[23] = byte2, UBFX #16,#8 */
    tmp32 = RAM_0x2000008C;
    buf[idx++] = (uint8_t)(tmp32 >> 16);                      /* 0x2000008E */

    /* buf[24] = byte1, LDRH + UBFX #8,#8 */
    tmp16 = *(volatile uint16_t *)0x2000008CU;
    buf[idx++] = (uint8_t)(tmp16 >> 8);                       /* 0x2000008D */

    /* buf[25] = byte0, LDRB raw */
    buf[idx++] = *(volatile uint8_t *)0x2000008CU;            /* 0x2000008C */

    /*--- Group 9: 0x20000128 as 32-bit big-endian @0x080167E6-0x08016826 ---*/
    /* buf[26] = byte3, LSRS #24 */
    tmp32 = RAM_0x20000128;
    buf[idx++] = (uint8_t)(tmp32 >> 24);                      /* 0x2000012B */

    /* buf[27] = byte2, UBFX #16,#8 */
    tmp32 = RAM_0x20000128;
    buf[idx++] = (uint8_t)(tmp32 >> 16);                      /* 0x2000012A */

    /* buf[28] = byte1, LDRH + UBFX #8,#8 */
    tmp16 = *(volatile uint16_t *)0x20000128U;
    buf[idx++] = (uint8_t)(tmp16 >> 8);                       /* 0x20000129 */

    /* buf[29] = byte0, LDRB raw */
    buf[idx++] = *(volatile uint8_t *)0x20000128U;            /* 0x20000128 */

    /*--- Group 10: 0x20000074 as 32-bit big-endian @0x08016828-0x08016868 ---*/
    /* buf[30] = byte3, LSRS #24 */
    tmp32 = RAM_0x20000074;
    buf[idx++] = (uint8_t)(tmp32 >> 24);                      /* 0x20000077 */

    /* buf[31] = byte2, UBFX #16,#8 */
    tmp32 = RAM_0x20000074;
    buf[idx++] = (uint8_t)(tmp32 >> 16);                      /* 0x20000076 */

    /* buf[32] = byte1, LDRH + UBFX #8,#8 */
    tmp16 = *(volatile uint16_t *)0x20000074U;
    buf[idx++] = (uint8_t)(tmp16 >> 8);                       /* 0x20000075 */

    /* buf[33] = byte0, LDRB raw */
    buf[idx++] = *(volatile uint8_t *)0x20000074U;            /* 0x20000074 */

    /*--- Group 11: 0x20000056 as 16-bit signed @0x0801686A-0x08016886 ---*/
    /* buf[34] = *(u16*)0x20000056, ASRS #8 */
    tmp16 = RAM_0x20000056;
    buf[idx++] = (uint8_t)((int16_t)tmp16 >> 8);              /* 0x20000057 */

    /* buf[35] = raw byte @0x20000056 */
    buf[idx++] = *(volatile uint8_t *)0x20000056U;            /* 0x20000056 */

    /*--- Group 12: 0x20000058 as 16-bit signed @0x08016888-0x080168A4 ---*/
    /* buf[36] = *(u16*)0x20000058, ASRS #8 */
    tmp16 = RAM_0x20000058;
    buf[idx++] = (uint8_t)((int16_t)tmp16 >> 8);              /* 0x20000059 */

    /* buf[37] = raw byte @0x20000058 */
    buf[idx++] = *(volatile uint8_t *)0x20000058U;            /* 0x20000058 */

    /*--- Group 13: 0x200006A6 struct (7 bytes) @0x080168A6-0x08016908 ---*/
    /* buf[38] = *(u16*)0x200006A6, ASRS #8 (signed byte from hword) */
    tmp16 = *(volatile uint16_t *)0x200006A6U;
    buf[idx++] = (uint8_t)((int16_t)tmp16 >> 8);              /* 0x200006A7 */

    /* buf[39] = raw byte @0x200006A6 */
    buf[idx++] = *(volatile uint8_t *)0x200006A6U;            /* 0x200006A6 */

    /* buf[40] = raw byte @0x200006A8 (base+2) */
    buf[idx++] = *(volatile uint8_t *)(0x200006A6U + 2);      /* 0x200006A8 */

    /* buf[41] = raw byte @0x200006A9 (base+3) */
    buf[idx++] = *(volatile uint8_t *)(0x200006A6U + 3);      /* 0x200006A9 */

    /* buf[42] = raw byte @0x200006AA (base+4) */
    buf[idx++] = *(volatile uint8_t *)(0x200006A6U + 4);      /* 0x200006AA */

    /* buf[43] = raw byte @0x200006AC (base+6) */
    buf[idx++] = *(volatile uint8_t *)(0x200006A6U + 6);      /* 0x200006AC */

    /* buf[44] = raw byte @0x200006AD (base+7) */
    buf[idx++] = *(volatile uint8_t *)(0x200006A6U + 7);      /* 0x200006AD */

    /*--- Group 14: 0x200000A8 struct (6 bytes) @0x0801690A-0x08016962 ---*/
    /* Three pairs: each pair is *(u16*)(base+off) ASRS #8 + *(u8*)(base+off) */
    /* Note: the ASRS is on halfword, not on full word -- ldrh + asrs #8 */

    /* Pair at offset +4: */
    /* buf[45] = *(u16*)(0x200000A8+4), ASRS #8 */
    tmp16 = *(volatile uint16_t *)(0x200000A8U + 4);
    buf[idx++] = (uint8_t)((int16_t)tmp16 >> 8);              /* 0x200000AD */

    /* buf[46] = raw byte @0x200000AC (base+4) */
    buf[idx++] = *(volatile uint8_t *)(0x200000A8U + 4);      /* 0x200000AC */

    /* Pair at offset +2: */
    /* buf[47] = *(u16*)(0x200000A8+2), ASRS #8 */
    tmp16 = *(volatile uint16_t *)(0x200000A8U + 2);
    buf[idx++] = (uint8_t)((int16_t)tmp16 >> 8);              /* 0x200000AB */

    /* buf[48] = raw byte @0x200000AA (base+2) */
    buf[idx++] = *(volatile uint8_t *)(0x200000A8U + 2);      /* 0x200000AA */

    /* Pair at offset +0: */
    /* buf[49] = *(u16*)(0x200000A8+0), ASRS #8 */
    tmp16 = RAM_0x200000A8;
    buf[idx++] = (uint8_t)((int16_t)tmp16 >> 8);              /* 0x200000A9 */

    /* buf[50] = raw byte @0x200000A8 (base+0) */
    buf[idx++] = *(volatile uint8_t *)0x200000A8U;            /* 0x200000A8 */

    /*--- Group 15: 0x20000019 as 8-bit @0x08016964-0x08016970 ---*/
    /* buf[51] = raw byte @0x20000019 */
    buf[idx++] = RAM_0x20000019;                              /* 0x20000019 */

    /*--- Group 16: 0x20000038 byte[0] @0x08016972-0x0801697E ---*/
    /* buf[52] = raw byte @0x20000038 (LDRB, offset 0 from pool) */
    buf[idx++] = *(volatile uint8_t *)0x20000038U;            /* 0x20000038 */

    /*====================================================================
     * BLOCK COPY: 2 x 16 bytes from 0x200003EC (@0x08016980-0x0801699E)
     * r5 loop counter 0..1
     * r1 = 0x200003EC + r5*16  (add.w r1, r2, r5, lsl #4)
     * r0 = buf + idx
     * func_839A(buf + idx, src_base + blk * 16)   -- memcpy 16 bytes
     * idx += 16 after each iteration
     *====================================================================*/
    for (blk = 0; blk < 2; blk++) {
        func_839A(&buf[idx], (const void *)(BLOCK_SRC_BASE + (uint32_t)blk * 16U));
        idx += 16;                                            /* add.w r0, r4, #0x10 */
    }
    /* after loop: idx = 53 + 32 = 85 */

    /*====================================================================
     * SECTION B:  Post-loop data bytes (buf[85..94] = 10 bytes)
     *====================================================================*/

    /*--- Group 17: 0x20000138 as 32-bit big-endian @0x080169A0-0x080169E0 ---*/
    /* buf[85] = byte3, LSRS #24 */
    tmp32 = RAM_0x20000138;
    buf[idx++] = (uint8_t)(tmp32 >> 24);                      /* 0x2000013B */

    /* buf[86] = byte2, UBFX #16,#8 */
    tmp32 = RAM_0x20000138;
    buf[idx++] = (uint8_t)(tmp32 >> 16);                      /* 0x2000013A */

    /* buf[87] = byte1, LDRH + UBFX #8,#8 */
    tmp16 = *(volatile uint16_t *)0x20000138U;
    buf[idx++] = (uint8_t)(tmp16 >> 8);                       /* 0x20000139 */

    /* buf[88] = byte0, LDRB raw */
    buf[idx++] = *(volatile uint8_t *)0x20000138U;            /* 0x20000138 */

    /*--- Group 18: 0x2000013C as 32-bit big-endian @0x080169E2-0x08016A22 ---*/
    /* buf[89] = byte3, LSRS #24 */
    tmp32 = RAM_0x2000013C;
    buf[idx++] = (uint8_t)(tmp32 >> 24);                      /* 0x2000013F */

    /* buf[90] = byte2, UBFX #16,#8 */
    tmp32 = RAM_0x2000013C;
    buf[idx++] = (uint8_t)(tmp32 >> 16);                      /* 0x2000013E */

    /* buf[91] = byte1, LDRH + UBFX #8,#8 */
    tmp16 = *(volatile uint16_t *)0x2000013CU;
    buf[idx++] = (uint8_t)(tmp16 >> 8);                       /* 0x2000013D */

    /* buf[92] = byte0, LDRB raw */
    buf[idx++] = *(volatile uint8_t *)0x2000013CU;            /* 0x2000013C */

    /*--- Tail markers @0x08016A24-0x08016A3A ---*/
    /* buf[93] = 0xAA */
    buf[idx++] = 0xAA;
    /* buf[94] = 0x55 */
    buf[idx++] = 0x55;

    /* idx = 95 at this point */

    /*====================================================================
     * SEND SEGMENT 1: UDIV/MLS calculation (@0x08016A3C-0x08016A5A)
     *
     * r0 = *(u32*)0x20000194       (raw value, pre-increment)
     * r2 = r0 / 0x1FE00            (UDIV)
     * r0 = r0 - 0x1FE00 * r2       (MLS = remainder)
     * r5 = 0x10000 + (r0 << 7)     (segment address)
     * func_160D0(buf, r5, idx)     (idx = 95 at this point)
     *====================================================================*/
    {
        uint32_t raw_val = RAM_0x20000194;                    /* pre-increment */
        uint32_t quotient  = raw_val / 0x1FE00U;
        uint32_t remainder = raw_val - quotient * 0x1FE00U;
        uint32_t seg_addr  = 0x10000U + (remainder << 7);
        func_160D0(buf, seg_addr, idx);                       /* len = 95 */
    }

    /*====================================================================
     * COUNTER UPDATE (@0x08016A5E-0x08016D8)
     *
     * r0 = *(u32*)0x20000194       (read again -- same value)
     * r0 = r0 + 1
     * *(u32*)0x20000194 = r0       (store incremented counter)
     *
     * Then overwrite buf[0..3] with post-increment counter bytes:
     *   buf[0] = (counter >> 24)  @0x08016A6E  strb.w r0, [sp, #4]
     *   buf[1] = (counter >> 16)  @0x08016AC6  strb.w r0, [sp, #5]
     *   buf[2] = (counter >>  8)  @0x08016AD0  strb.w r0, [sp, #6]
     *   buf[3] = (counter >>  0)  @0x08016AD8  strb.w r0, [sp, #7]
     *
     * The extraction sequence is:
     *   buf[0]: ldr [0x20000194], lsrs #24          --> byte3
     *   buf[1]: re-read [0x20000194], lsrs #16      --> byte2
     *   buf[2]: ldrh [0x20000194], lsrs #8          --> byte1 (via halfword)
     *   buf[3]: ldrb [0x20000194]                   --> byte0
     *====================================================================*/
    {
        uint32_t cnt = RAM_0x20000194 + 1U;                  /* increment */
        RAM_0x20000194 = cnt;                                /* store back */

        /* buf[0..3] overwrite: big-endian counter bytes */
        buf[0] = (uint8_t)(cnt >> 24);                       /* byte3, STRB.W [sp,#4] */
        buf[1] = (uint8_t)(cnt >> 16);                       /* byte2, STRB.W [sp,#5] */
        buf[2] = (uint8_t)(cnt >>  8);                       /* byte1, STRB.W [sp,#6] */
        buf[3] = (uint8_t)(cnt);                             /* byte0, STRB.W [sp,#7] */
    }

    /*====================================================================
     * SEND SEGMENT 2 (@0x08016ADC-0x08016AF2)
     *
     * Uses r1 preserved from the counter store (r1 = &0x20000194)
     * r0 = *(u16*)0x20000194         (lower 16 bits of post-increment counter)
     * r0 = (r0 - 1) & 0x3FF
     * r1 = 0x2000 + (r0 << 2)
     * func_160D0(buf, r1, 4)
     *====================================================================*/
    {
        uint16_t val16 = *(volatile uint16_t *)0x20000194U;  /* post-increment */
        uint32_t sub_val = ((uint32_t)val16 - 1U) & 0x3FFU;  /* UBFX #0,#10 */
        uint32_t seg_addr = 0x2000U + (sub_val << 2);
        func_160D0(buf, seg_addr, 4U);
    }

    /*====================================================================
     * SEND SEGMENT 3 (@0x08016AF6-0x08016B0C)
     *
     * ldr r0, [pc, #0x1c] -> pool @0x08016B14 = 0x20000194
     * r0 = *(u16*)0x20000194         (lower 16 bits, re-read)
     * r0 = (r0 - 1) & 0x3FF
     * r1 = 0x3000 + (r0 << 2)
     * func_160D0(buf, r1, 4)
     *====================================================================*/
    {
        uint16_t val16 = *(volatile uint16_t *)0x20000194U;  /* re-read */
        uint32_t sub_val = ((uint32_t)val16 - 1U) & 0x3FFU;
        uint32_t seg_addr = 0x3000U + (sub_val << 2);
        func_160D0(buf, seg_addr, 4U);
    }
}
