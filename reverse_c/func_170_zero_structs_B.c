/**
 * func_170_zero_structs_B
 *
 * @addr 0x08018DE0 - 0x08018DF1
 * DISASSEMBLY-TRACED. Verified against Capstone.
 * @file_offset 0x10DE0 - 0x10DF3 (18 bytes)
 *
 * Leaf function. Zero-initializes two 16-bit fields at offset +0/+2 of a
 * struct at 0x20003648, one 16-bit halfword at 0x200002B8, and one 16-bit
 * halfword at 0x200002BA.
 *
 * Companion to func_169. Together they clear six distinct memory locations
 * spread across two separate RAM structs.
 */

// Literal pool addresses:
//   0x08018DF4: 0x20003648  -- base struct B
//   0x08018DF8: 0x200002B8  -- 16-bit value field
//   0x08018DFC: 0x200002BA  -- 16-bit status field

void func_170_zero_structs_B(void) {
    // 0x08018DE0: MOVS R0, #0
    // 0x08018DE2: LDR  R1, [PC, #0x10]  ; R1 = 0x20003648
    // 0x08018DE4: STRH R0, [R1, #2]      ; *(u16*)(0x2000364A) = 0
    // 0x08018DE6: STRH R0, [R1]          ; *(u16*)(0x20003648) = 0
    // 0x08018DE8: LDR  R1, [PC, #0x0C]  ; R1 = 0x200002B8
    // 0x08018DEA: STRH R0, [R1]          ; *(u16*)(0x200002B8) = 0
    // 0x08018DEC: LDR  R1, [PC, #0x0C]  ; R1 = 0x200002BA
    // 0x08018DEE: STRH R0, [R1]          ; *(u16*)(0x200002BA) = 0
    // 0x08018DF0: BX LR
}
