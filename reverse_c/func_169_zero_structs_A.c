/**
 * func_169_zero_structs_A
 *
 * @addr 0x08018DC0 - 0x08018DD1
 * DISASSEMBLY-TRACED. Verified against Capstone.
 * @file_offset 0x10DC0 - 0x10DD3 (18 bytes)
 *
 * Leaf function. Zero-initializes two 16-bit fields at offset +0/+2 of a
 * struct at 0x20001A1C, one 32-bit word at 0x20000284, and one 16-bit
 * halfword at 0x20000288.
 *
 * This is part of a cluster of small init-clear functions (func_169,
 * func_170) that zero out different groups of sensor/status variables
 * before a new measurement cycle.
 *
 * No callee-saved register saving needed (leaf, no calls).
 */

// Literal pool addresses loaded via PC-relative LDR:
//   0x08018DD4: 0x20001A1C  -- base struct A (word at file 0x10DD4)
//   0x08018DD8: 0x20000284  -- 32-bit counter/accumulator
//   0x08018DDC: 0x20000288  -- 16-bit status flag

void func_169_zero_structs_A(void) {
    // 0x08018DC0: MOVS R0, #0
    // 0x08018DC2: LDR  R1, [PC, #0x10]  ; R1 = 0x20001A1C
    // 0x08018DC4: STRH R0, [R1, #2]      ; *(u16*)(0x20001A1E) = 0
    // 0x08018DC6: STRH R0, [R1]          ; *(u16*)(0x20001A1C) = 0
    // 0x08018DC8: LDR  R1, [PC, #0x0C]  ; R1 = 0x20000284
    // 0x08018DCA: STR  R0, [R1]          ; *(u32*)(0x20000284) = 0
    // 0x08018DCC: LDR  R1, [PC, #0x0C]  ; R1 = 0x20000288
    // 0x08018DCE: STRH R0, [R1]          ; *(u16*)(0x20000288) = 0
    // 0x08018DD0: BX LR
}
