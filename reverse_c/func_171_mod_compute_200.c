/**
 * func_171_mod_compute_200
 *
 * @addr 0x08018E00 - 0x08018E1D
 * DISASSEMBLY-TRACED. Verified against Capstone.
 * @file_offset 0x10E00 - 0x10E1F (30 bytes)
 *
 * Leaf function. Reads a 16-bit value from RAM at 0x20001A1C, adds 0x200,
 * subtracts the 16-bit value at 0x20001A1E, then computes the remainder
 * modulo 512 (0x200). Returns the result as an unsigned 16-bit halfword.
 *
 * This is a modular arithmetic step used in display coordinate or
 * PWM duty-cycle computation, handling wraparound at 512-unit boundaries.
 * The formula is: result = (*(0x20001A1C) + 512 - *(0x20001A1E)) % 512
 *
 * The mod-512 is implemented via the compiler idiom:
 *   r0 = (r1 + (r1>>31)*(1>>23)) >> 9   // signed division by 512
 *   r0 = r1 - r0 * 512                    // remainder
 */

// Literal pool (1 word):
//   0x08018E20: 0x20001A1C  -- struct base (both LDRs reference the same word)

uint16_t func_171_mod_compute_200(void) {
    // 0x08018E00: LDR  R0, [PC, #0x1C]  ; R0 = 0x20001A1C
    // 0x08018E02: LDRH R0, [R0]          ; R0 = *(u16*)0x20001A1C
    // 0x08018E04: ADDW R0, R0, #0x200    ; R0 = R0 + 512
    // 0x08018E08: LDR  R2, [PC, #0x14]  ; R2 = 0x20001A1C
    // 0x08018E0A: LDRH R2, [R2, #2]      ; R2 = *(u16*)0x20001A1E
    // 0x08018E0C: SUBS R1, R0, R2        ; R1 = (value + 512) - offset
    // 0x08018E0E: ASRS R0, R1, #31       ; R0 = sign bit (0 or -1)
    // 0x08018E10: ADDW R0, R1, R0, LSR#23; signed division correction
    // 0x08018E14: ASRS R0, R0, #9        ; R0 = R1 / 512 (signed)
    // 0x08018E16: SUBW R0, R1, R0, LSL#9 ; R0 = R1 - (R1/512)*512 = R1 % 512
    // 0x08018E1A: UXTH R0, R0            ; zero-extend to 16-bit
    // 0x08018E1C: BX LR
}
