/**
 * func_174_fpu_nan_branch
 *
 * @addr 0x08018E58 - 0x08018EC4
 * @file_offset 0x10E58 - 0x10EC8 (112 bytes, 2 bytes zero-pad at 0x08018EC6)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * FPU double-precision NaN/infinity check function with conditional call.
 * Takes a double in R0:R1, saves it in D9, calls func_08BC4 to produce a
 * reference value in D8. Then checks whether either value is NaN or
 * infinity. If the reference (D8) is NaN/inf, skips to return it. If D9 is
 * NaN/inf, calls func_175C4 with value 1 before returning D8.
 *
 * NaN check logic (performed on both D8 and D9):
 *   1. Extract the raw integer representation of the double
 *   2. OR together the two 32-bit halves (any non-zero low word means NaN)
 *   3. Clear the sign bit (bit 31)
 *   4. If value >= 0x7FF00000, it's NaN or infinity
 *   5. Branch accordingly
 *
 * This function implements an IEEE 754 classification guard used before
 * downstream FPU computations that cannot tolerate non-finite inputs.
 *
 * BL targets:
 *   0x08018E64: BL 0x08008BC4  -- produces reference double (likely a sensor reading)
 *   0x08018EB6: BL 0x080175C4  -- error handler / NaN reporting callback
 */

// No literal pool.

// @bl_targets:
//   0x08018E64: BL 0x08008BC4  -- reference value generator
//   0x08018EB6: BL 0x080175C4  -- NaN/inf error handler

double func_174_fpu_nan_branch(double d9_val) {
    // 0x08018E58: PUSH  {LR}
    // 0x08018E5A: VPUSH {D8, D9}
    // 0x08018E5E: SUB   SP, #0x0C           ; allocate 12 bytes stack
    // 0x08018E60: VMOV  D9, R0, R1          ; D9 = input double
    // 0x08018E64: BL    0x08008BC4           ; D8 = func_08BC4()
    // 0x08018E68: VMOV  D8, R0, R1          ; save reference to D8
    // 0x08018E6C: VMOV  R0, S16             ; R0 = low word of D8
    // 0x08018E70: VMOV  R1, S17             ; R1 = high word of D8
    // 0x08018E74: VSTR  D8, [SP]            ; spill D8 to stack
    // 0x08018E78: CMP   R0, #0              ; is low word zero?
    // 0x08018E7A: IT NE
    // 0x08018E7C: MOVS  R0, #1              ; if non-zero, flag=1
    // 0x08018E7E: ORRS  R0, R1              ; OR with high word
    // 0x08018E80: BIC   R0, R0, #0x80000000 ; clear sign bit
    // 0x08018E84: RSB.W R0, R0, #0x0FF00000 ; R0 = 0x7FF00000 - R0
    // 0x08018E88: ADD.W R0, R0, #0x70000000 ; adjust exponent range
    // 0x08018E8C: LSRS  R0, R0, #31         ; R0 = (R0 >> 31) -- 0 if NaN/inf
    // 0x08018E8E: BEQ   0x08018EBA           ; if NaN/inf (D8 bad), skip to return D8
    //
    // --- Check D9 for NaN/inf ---
    // 0x08018E90: VMOV  R1, S18             ; R1 = low word of D9
    // 0x08018E94: VMOV  R0, S19             ; R0 = high word of D9
    // 0x08018E98: VSTR  D9, [SP]            ; spill D9 to stack
    // 0x08018E9C: CMP   R1, #0              ; is low word zero?
    // 0x08018E9E: IT NE
    // 0x08018EA0: MOVS  R1, #1              ; if non-zero, flag=1
    // 0x08018EA2: ORRS  R0, R1              ; OR with high word
    // 0x08018EA4: BIC   R0, R0, #0x80000000 ; clear sign bit
    // 0x08018EA8: RSB.W R0, R0, #0x0FF00000 ; R0 = 0x7FF00000 - R0
    // 0x08018EAC: ADD.W R0, R0, #0x70000000 ; adjust exponent range
    // 0x08018EB0: LSRS  R0, R0, #31         ; is NaN/inf?
    // 0x08018EB2: ITT EQ
    // 0x08018EB4: MOVS  R0, #1              ; if D9 NaN/inf:
    // 0x08018EB6: BL    0x080175C4           ;   func_175C4(1)  -- report error
    //
    // --- Return D8 (reference value) ---
    // 0x08018EBA: VMOV  R0, R1, D8          ; return D8
    // 0x08018EBE: ADD   SP, #0x0C           ; restore stack
    // 0x08018EC0: VPOP  {D8, D9}
    // 0x08018EC4: POP   {PC}
    //
    // 0x08018EC6: MOVS  R0, R0              ; alignment pad (2 bytes)
}
