/**
 * func_172_mod_compute_400
 *
 * @addr 0x08018E24 - 0x08018E41
 * DISASSEMBLY-TRACED. Verified against Capstone.
 * @file_offset 0x10E24 - 0x10E43 (30 bytes)
 *
 * Leaf function. Reads a 16-bit value from RAM at 0x20003648, adds 0x400,
 * subtracts the 16-bit value at 0x2000364A, then computes the remainder
 * modulo 1024 (0x400). Returns the result as an unsigned 16-bit halfword.
 *
 * Companion to func_171. This variant uses a 1024-unit modulus instead of
 * 512. Used for a different sensor channel or display coordinate axis with
 * larger range. The mod-1024 is implemented via the compiler idiom:
 *   r0 = (r1 + (r1>>31)*(1>>22)) >> 10  // signed division by 1024
 *   r0 = r1 - r0 * 1024                   // remainder
 */

// Literal pool (1 word):
//   0x08018E44: 0x20003648  -- struct base B

uint16_t func_172_mod_compute_400(void) {
    // 0x08018E24: LDR  R0, [PC, #0x1C]  ; R0 = 0x20003648
    // 0x08018E26: LDRH R0, [R0]          ; R0 = *(u16*)0x20003648
    // 0x08018E28: ADDW R0, R0, #0x400    ; R0 = R0 + 1024
    // 0x08018E2C: LDR  R2, [PC, #0x14]  ; R2 = 0x20003648
    // 0x08018E2E: LDRH R2, [R2, #2]      ; R2 = *(u16*)0x2000364A
    // 0x08018E30: SUBS R1, R0, R2        ; R1 = (value + 1024) - offset
    // 0x08018E32: ASRS R0, R1, #31       ; R0 = sign bit
    // 0x08018E34: ADDW R0, R1, R0, LSR#22; signed division correction
    // 0x08018E38: ASRS R0, R0, #10       ; R0 = R1 / 1024 (signed)
    // 0x08018E3A: SUBW R0, R1, R0, LSL#10; R0 = R1 - (R1/1024)*1024 = rem
    // 0x08018E3E: UXTH R0, R0            ; zero-extend to 16-bit
    // 0x08018E40: BX LR
}
