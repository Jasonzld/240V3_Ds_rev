/**
 * @file func_147_ring_buf_line_reader.c
 * @brief Ring Buffer Line Reader — reads bytes from ring buffer, detects CRLF line endings
 * @addr  0x08018C34 - 0x08018CEA (main function + literal pool)
 *        Sub-functions within same region: 0x08018D2C (Peek), 0x08018D60 (Push), 0x08018D8C (Pop)
 *        Shared utility func_18E24 @ 0x08018E24 (also claimed by func_158/func_172)
 *
 * DISASSEMBLY-TRACED. All pool addresses verified from Capstone disassembly.
 *
 * Main function: RingBuf_Line_Reader @ 0x08018C34 (182 bytes)
 *   r0 = dst, r1 = max_len, returns dst_len in r0
 *
 * Sub-functions (leaf, within same file range):
 *   func_18D2C @ 0x08018D2C: RingBuf_Peek  — read byte from ring buffer
 *   func_18D60 @ 0x08018D60: RingBuf_Push  — write byte to ring buffer
 *   func_18D8C @ 0x08018D8C: RingBuf_Pop   — remove byte from ring buffer
 *   func_18E24 @ 0x08018E24: RingBuf_Available — check readable bytes > 0
 *
 * RAM variables (literal pool at 0x08018CEC-0x08018CF7):
 *   0x200002B8: PREV_BYTE (uint8) + WORD_LOW (uint16) — sliding window [byte, prev_byte]
 *   0x200002BA: DST_LEN (uint16) — destination write index
 *   0x200002BD: STATUS_FLAG (uint8) — state flag for line detection
 */

#include "stm32f4xx_hal.h"

extern void func_18D2C(uint8_t *out_byte);      /* RingBuf_Peek: read byte, advance read idx */
extern uint32_t func_18E24(void);                /* RingBuf_Available: returns >0 if data available */

#define PREV_BYTE    (*(volatile uint8_t  *)0x200002B8)  /* previous byte for CRLF sliding window */
#define WORD_LOW     (*(volatile uint16_t *)0x200002B8)  /* low 16 bits of window word */
#define DST_LEN      (*(volatile uint16_t *)0x200002BA)  /* destination write length */
#define STATUS_FLAG  (*(volatile uint8_t  *)0x200002BD)  /* line detection status */

/**
 * RingBuf_Line_Reader @ 0x08018C34
 *
 * Flow:
 *   push {r3,r4,r5,r6,r7,lr}
 *   r4 = dst, r5 = max_len, r6 = 0, [sp] = 0 (prev_byte init)
 *   loop:
 *     if (!RingBuf_Available()) return 0
 *     RingBuf_Peek(sp) → byte
 *     word = byte | (PREV_BYTE << 8)
 *     WORD_LOW = word
 *     if (WORD_HIGH != 0) goto write_byte   // WORD_HIGH at offset+2
 *     if (STATUS_FLAG != 0) goto write_byte
 *     if (word == 0x0D0A || word == 0x2B):
 *       STATUS_FLAG = 1
 *       WORD_LOW = 0
 *       continue loop
 *     // else fall through to write_byte:
 *   write_byte:
 *     if (DST_LEN == 0) WORD_LOW = 0          // clear only on first byte (no prev context)
 *     if (DST_LEN < max_len):
 *       dst[DST_LEN++] = byte
 *     if (WORD_LOW != 0x0D0A) continue loop  // keep going if not CRLF
 *     // CRLF detected: finalize
 *     dst[DST_LEN - 2] = 0                   // null-terminate before CR
 *     WORD_LOW = 0                           // clear
 *     DST_LEN = 0                            // clear (direct strh #0)
 *     STATUS_FLAG = 0                        // clear
 *     return (uint16_t)(DST_LEN - 2)         // return adjusted length
 */
uint32_t RingBuf_Line_Reader(uint8_t *dst, uint32_t max_len)
{
    uint8_t  byte;              /* [sp] */
    uint16_t word;              /* r0 — sliding window */

    /* @0x08018C36-0x08018C40: save params, init locals only
     * mov r4,r0 (dst), mov r5,r1 (max_len), movs r6,#0 (idx reg)
     * movs r0,#0; str r0,[sp] → stack prev_byte = 0
     * WORD_LOW/DST_LEN in RAM NOT initialized — carry over from prior calls */

    /* @0x08018CDE-0x08018CE4: loop condition */
    while (func_18E24() > 0) {   /* RingBuf_Available() > 0 */
        /* @0x08018C42-0x08018C50: read byte, build sliding window */
        func_18D2C(&byte);                   /* RingBuf_Peek: byte = next from ring buffer */
        word = byte | ((uint16_t)PREV_BYTE << 8);  /* orr.w r0, r0, r1, lsl #8 */
        WORD_LOW = word;                     /* strh — store as lower 16 bits at 0x200002B8 */

        /* @0x08018C58-0x08018C62: gate checks */
        /* LDRH from 0x200002BA — if WORD_HIGH/status non-zero, skip CRLF check */
        if (*(volatile uint16_t *)0x200002BA != 0)
            goto write_byte;
        if (STATUS_FLAG != 0)
            goto write_byte;

        /* @0x08018C64-0x08018C82: CRLF / '+' detection */
        /* word == 0x0D0A (CRLF) or word == 0x2B ('+') */
        if (word == 0x0D0A || word == 0x2B) {
            STATUS_FLAG = 1;                 /* strb #1 to 0x200002BD */
            WORD_LOW = 0;                    /* strh #0 to 0x200002B8 */
            continue;                        /* b to loop check */
        }
        /* fall through to write_byte */

write_byte:
        /* @0x08018C86-0x08018C90: clear WORD_LOW only when DST_LEN == 0
         * cbnz r0, #0x8018c92 → if DST_LEN != 0 → skip clear
         * movs r0,#0; strh r0,[r1] → WORD_LOW = 0 only when DST_LEN == 0 */
        if (*(volatile uint16_t *)0x200002BA == 0) {
            WORD_LOW = 0;                    /* strh #0 to 0x200002B8 */
        }

        /* @0x08018C92-0x08018CAA: write byte to dst buffer */
        if (DST_LEN < (uint16_t)max_len) {   /* ldrh, cmp with r5 */
            dst[DST_LEN] = byte;             /* strb r1, [r4, r2] */
            DST_LEN++;                       /* adds+strh — increment and store */
        }

        /* @0x08018CAC-0x08018CDC: post-write CRLF check and finalize */
        if (WORD_LOW == 0x0D0A) {            /* cmp with #0xD0A */
            /* Null-terminate before CR */
            dst[DST_LEN - 2] = 0;            /* strb r1(0), [r4, r0] where r0 = DST_LEN-2 */

            /* Compute final length (DST_LEN stays unchanged, cleared below) */
            uint16_t final_len = DST_LEN - 2; /* subs r0, r0, #2; uxth r6, r0 */

            /* Clear all state */
            WORD_LOW = 0;                    /* strh #0 to 0x200002B8 */
            DST_LEN = 0;                     /* strh #0 to 0x200002BA */
            STATUS_FLAG = 0;                 /* strb #0 to 0x200002BD */

            return final_len;                /* mov r0, r6; pop {r3,r4,r5,r6,r7,pc} */
        }
        /* else: continue loop */
    }

    /* @0x08018CE6-0x08018CEA: RingBuf empty, return 0 */
    return 0;
}
