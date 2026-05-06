/**
 * func_175_init_sequence
 *
 * @addr 0x08018EC8 - 0x08018F7E
 * DISASSEMBLY-TRACED. Verified against Capstone.
 * @file_offset 0x10EC8 - 0x10F80 (184 bytes)
 *
 * Largest function in the tail gap. Takes a single argument in R0 (saved
 * to R4) and performs a hardware initialization sequence:
 *
 *   1. Calls func_113B8(1) and func_113D8(0x00020000) -- GPIO/pin setup
 *   2. Calls func_0C4BC twice, both with USART2 (0x40020000):
 *        func_0C4BC(USART2, 2, 7) and func_0C4BC(USART2, 3, 7)
 *   3. Builds a small config packet on the stack and calls:
 *        func_0C42C(0x40020000, &stack_pkt) -- USART2 transmit with config
 *   4. Stores R4 (the input argument) to [SP+8], zeroes several halfwords
 *      at SP+0xC through SP+0x14, sets SP+0x12 = 0x0C, and calls:
 *        func_15E50(0x40004400, &stack_buf2) -- USART6 extended setup
 *   5. Calls func_15D80(0x40004400, 1) -- USART6 enable
 *   6. Calls func_15E06 twice, both with USART6 (0x40004400):
 *        func_15E06(USART6, 0x0525, 1)
 *        func_15E06(USART6, 0x0424, 1)
 *   7. Calls func_15D50(0x40004400, 0x0424) -- USART6 parameter
 *   8. Builds final 4-byte packet {0x26, 3, 3, 1} on stack and calls:
 *        func_0D9B4(&packet) -- display command dispatcher
 *   9. Restores SP and returns via POP {R4, PC}
 *
 * This is a USART/display initialization orchestrator, configuring two
 * USART peripherals (USART2 and USART6) and sending a display init command.
 *
 * Literal pool (2 words at 0x08018F80):
 *   0x40020000 -- USART2 base address (STM32F4xx)
 *   0x40004400 -- USART6 base address (STM32F4xx)
 *
 * BL targets:
 *   0x08018ED2: BL 0x080113B8  -- GPIO output mode set
 *   0x08018EDA: BL 0x080113D8  -- GPIO pin set/reset
 *   0x08018EE4: BL 0x0800C4BC  -- USART baud/mode config (USART2)
 *   0x08018EEE: BL 0x0800C4BC  -- USART baud/mode config (USART2, call 2)
 *   0x08018F10: BL 0x0800C42C  -- USART2 transmit with config packet
 *   0x08018F32: BL 0x08015E50  -- USART6 extended setup
 *   0x08018F3A: BL 0x08015D80  -- USART6 enable
 *   0x08018F46: BL 0x08015E06  -- USART6 param set (0x525)
 *   0x08018F52: BL 0x08015E06  -- USART6 param set (0x424)
 *   0x08018F5C: BL 0x08015D50  -- USART6 param set (0x424)
 *   0x08018F78: BL 0x0800D9B4  -- display command send
 */

// Literal pool (external, at 0x08018F80):
//   0x40020000 -- USART2 peripheral base
//   0x40004400 -- USART6 peripheral base

// @bl_targets:
//   0x080113B8 -- gpio_set_output_mode
//   0x080113D8 -- gpio_pin_set
//   0x0800C4BC -- usart_baud_config
//   0x0800C42C -- usart_transmit_config
//   0x08015E50 -- usart_extended_setup
//   0x08015D80 -- usart_enable
//   0x08015E06 -- usart_param_set
//   0x08015D50 -- usart_param_write
//   0x0800D9B4 -- display_cmd_send

void func_175_init_sequence(uint32_t arg_r4) {
    // 0x08018EC8: PUSH  {R4, LR}
    // 0x08018ECA: SUB   SP, #0x20          ; 32 bytes local stack

    // 0x08018ECC: MOV   R4, R0             ; save arg to R4

    // Step 1: GPIO pin setup
    // 0x08018ECE: MOVS  R1, #1
    // 0x08018ED0: MOV   R0, R1
    // 0x08018ED2: BL    0x080113B8         ; func_113B8(1) -- enable GPIO mode
    // 0x08018ED6: MOVS  R1, #1
    // 0x08018ED8: LSLS  R0, R1, #17        ; R0 = 0x00020000
    // 0x08018EDA: BL    0x080113D8         ; func_113D8(0x20000) -- set pin

    // Step 2: USART baud rate configuration
    // 0x08018EDE: MOVS  R2, #7
    // 0x08018EE0: MOVS  R1, #2
    // 0x08018EE2: LDR   R0, [PC, #0x9C]   ; R0 = 0x40020000 (USART2)
    // 0x08018EE4: BL    0x0800C4BC         ; func_0C4BC(USART2, 2, 7)
    // 0x08018EE8: MOVS  R2, #7
    // 0x08018EEA: MOVS  R1, #3
    // 0x08018EEC: LDR   R0, [PC, #0x90]   ; R0 = 0x40020000 (USART2)
    // 0x08018EEE: BL    0x0800C4BC         ; func_0C4BC(USART2, 3, 7)

    // Step 3: Build config packet on stack and send via USART2
    // 0x08018EF2: MOVS  R0, #0x0C
    // 0x08018EF4: STR   R0, [SP, #0x18]    ; pkt[0] = 12 (length/size)
    // 0x08018EF6: MOVS  R0, #2
    // 0x08018EF8: STRB  R0, [SP, #0x1C]    ; pkt[4] = 2
    // 0x08018EFC: STRB  R0, [SP, #0x1D]    ; pkt[5] = 2
    // 0x08018F00: MOVS  R0, #0
    // 0x08018F02: STRB  R0, [SP, #0x1E]    ; pkt[6] = 0
    // 0x08018F06: MOVS  R0, #1
    // 0x08018F08: STRB  R0, [SP, #0x1F]    ; pkt[7] = 1
    // 0x08018F0C: ADD   R1, SP, #0x18      ; R1 = &pkt
    // 0x08018F0E: LDR   R0, [PC, #0x70]   ; R0 = 0x40020000 (USART2)
    // 0x08018F10: BL    0x0800C42C         ; func_0C42C(USART2, &pkt)

    // Step 4: Store arg and build second buffer on stack
    // 0x08018F14: STR   R4, [SP, #8]       ; buf2[0] = arg_r4
    // 0x08018F16: MOVS  R0, #0
    // 0x08018F18: STRH  R0, [SP, #0x0C]    ; buf2[2] = 0
    // 0x08018F1C: STRH  R0, [SP, #0x0E]    ; buf2[3] = 0
    // 0x08018F20: STRH  R0, [SP, #0x10]    ; buf2[4] = 0
    // 0x08018F24: STRH  R0, [SP, #0x14]    ; buf2[6] = 0
    // 0x08018F28: MOVS  R0, #0x0C
    // 0x08018F2A: STRH  R0, [SP, #0x12]    ; buf2[5] = 12
    // 0x08018F2E: ADD   R1, SP, #8         ; R1 = &buf2
    // 0x08018F30: LDR   R0, [PC, #0x50]   ; R0 = 0x40004400 (USART6)
    // 0x08018F32: BL    0x08015E50         ; func_15E50(USART6, &buf2)

    // Step 5: USART6 enable
    // 0x08018F36: MOVS  R1, #1
    // 0x08018F38: LDR   R0, [PC, #0x48]   ; R0 = 0x40004400 (USART6)
    // 0x08018F3A: BL    0x08015D80         ; func_15D80(USART6, 1)

    // Step 6: USART6 extended parameter configuration
    // 0x08018F3E: MOVS  R2, #1
    // 0x08018F40: MOVW  R1, #0x525         ; param = 0x0525
    // 0x08018F44: LDR   R0, [PC, #0x3C]   ; R0 = 0x40004400 (USART6)
    // 0x08018F46: BL    0x08015E06         ; func_15E06(USART6, 0x525, 1)

    // 0x08018F4A: MOVS  R2, #1
    // 0x08018F4C: MOVW  R1, #0x424         ; param = 0x0424
    // 0x08018F50: LDR   R0, [PC, #0x30]   ; R0 = 0x40004400 (USART6)
    // 0x08018F52: BL    0x08015E06         ; func_15E06(USART6, 0x424, 1)

    // 0x08018F56: MOVW  R1, #0x424         ; param = 0x0424
    // 0x08018F5A: LDR   R0, [PC, #0x28]   ; R0 = 0x40004400 (USART6)
    // 0x08018F5C: BL    0x08015D50         ; func_15D50(USART6, 0x424)

    // Step 8: Display command dispatch
    // 0x08018F60: MOVS  R0, #0x26          ; cmd = 0x26
    // 0x08018F62: STRB  R0, [SP, #4]       ; pkt[0] = 0x26
    // 0x08018F66: MOVS  R0, #3             ; param = 3
    // 0x08018F68: STRB  R0, [SP, #5]       ; pkt[1] = 3
    // 0x08018F6C: STRB  R0, [SP, #6]       ; pkt[2] = 3
    // 0x08018F70: MOVS  R0, #1
    // 0x08018F72: STRB  R0, [SP, #7]       ; pkt[3] = 1
    // 0x08018F76: ADD   R0, SP, #4         ; R0 = &pkt
    // 0x08018F78: BL    0x0800D9B4         ; func_0D9B4(&pkt) -- send display cmd {0x26,3,3,1}

    // 0x08018F7C: ADD   SP, #0x20          ; restore stack
    // 0x08018F7E: POP   {R4, PC}
}
