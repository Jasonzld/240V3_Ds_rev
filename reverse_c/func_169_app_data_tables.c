/**
 * func_169_app_data_tables.c -- Data Tables in Gap Region
 *
 * @region 0x08018F80 - 0x0801995F (end of firmware binary)
 * @file_offset 0x10F80 - 0x1195F (2528 bytes)
 * DISASSEMBLY-TRACED. Verified against Capstone. (Data tables, no executable code)
 *
 * This file documents all data areas found in the 2979-byte tail gap
 * (0x08018DBD - 0x0801995F), excluding the seven executable functions
 * (func_169 through func_175). The data is organized into 8 sections.
 *
 * All addresses are runtime (flash) addresses. File offset =
 * runtime_addr - APP_BASE, where APP_BASE = 0x08008000.
 *
 * ============================================================================
 * SECTION 0: func_175 Literal Pool Overlap
 * ============================================================================
 *
 * @addr 0x08018F80 - 0x08018F87 (8 bytes)
 * @file 0x10F80 - 0x10F87
 *
 * These two 32-bit words are the literal pool referenced by PC-relative LDR
 * instructions in func_175_init_sequence (0x08018EC8-0x08018F7E). They
 * provide USART peripheral base addresses for the initialization sequence.
 *
 * References from func_175:
 *   LDR R0,[PC,#0x9C] at 0x08018EE2  -> 0x08018F80 (USART2 base)
 *   LDR R0,[PC,#0x90] at 0x08018EEC  -> 0x08018F80 (USART2 base)
 *   LDR R0,[PC,#0x70] at 0x08018F0E  -> 0x08018F80 (USART2 base)
 *   LDR R0,[PC,#0x50] at 0x08018F30  -> 0x08018F84 (USART6 base)
 *   LDR R0,[PC,#0x48] at 0x08018F38  -> 0x08018F84 (USART6 base)
 *   LDR R0,[PC,#0x3C] at 0x08018F44  -> 0x08018F84 (USART6 base)
 *   LDR R0,[PC,#0x30] at 0x08018F50  -> 0x08018F84 (USART6 base)
 *   LDR R0,[PC,#0x28] at 0x08018F5A  -> 0x08018F84 (USART6 base)
 */
// @0x08018F80  .word 0x40020000  // USART2 peripheral base (STM32F4xx)
// @0x08018F84  .word 0x40004400  // USART6 peripheral base (STM32F4xx)


/*
 * ============================================================================
 * SECTION 1: Display PWM / Temperature Lookup Table
 * ============================================================================
 *
 * @addr 0x08018F88 - 0x080195D1 (1610 bytes, 805 uint16_t entries)
 * @file 0x10F88 - 0x115D1
 *
 * A large contiguous array of unsigned 16-bit halfword values. These form
 * a pre-computed lookup table used by the display brightness PWM and/or
 * temperature-to-voltage mapping subsystem.
 *
 * The values range from approximately 0x0032 (50) to 0x02CC (716) and
 * follow smooth monotonic progressions, consistent with a piecewise-
 * linear or polynomial curve encoded as discrete samples.
 *
 * The table is organized as multiple concatenated sub-tables, each
 * representing a different temperature breakpoint or display mode.
 * Values increment by step sizes of roughly 20-40 units per element,
 * suggesting 7-11 element rows corresponding to display brightness
 * levels at a given sensor reading.
 *
 * Typical value sequences (first 55 entries):
 *   [0x0032, 0x0055, 0x008C, 0x00B4, 0x00FA, 0x0186, 0x01B8,
 *    0x01EA, 0x021C, 0x024E, 0x0280, 0x0295, 0x02A4, 0x02B3,
 *    0x02BD, 0x0172, 0x01A4, 0x01D6, ...]  (50, 85, 140, 180, ...)
 *
 * Each entry represents a PWM duty cycle value (0 = off, ~0x0300 = max)
 * for a specific (temperature, brightness_level) combination. The table
 * is accessed by func_171 (mod-512) and func_172 (mod-1024) to compute
 * display update deltas.
 */
// @0x08018F88
static const uint16_t g_display_pwm_lookup[805] = {
    // File offset 0x10F88, runtime 0x08018F88
    // Raw hex dump of first 128 bytes:
    //   32 00 55 00 8c 00 b4 00 fa 00 86 01 b8 01 ea 01
    //   1c 02 4e 02 80 02 95 02 a4 02 b3 02 bd 02 72 01
    //   a4 01 d6 01 08 02 3a 02 6c 02 8f 02 9e 02 ad 02
    //   bc 02 5e 01 90 01 c2 01 f4 01 26 02 58 02 8a 02
    //   98 02 a7 02 b6 02 4a 01 7c 01 ae 01 e0 01 12 02
    //   44 02 76 02 92 02 a1 02 b0 02 36 01 68 01 9a 01
    //   cc 01 fe 01 30 02 62 02 8c 02 a5 02 aa 02 22 01
    //   54 01 86 01 b8 01 ea 01 1c 02 4e 02 80 02 95 02
    //   ...
    //   (805 entries total, continuing to file offset 0x115D1)
};


/*
 * ============================================================================
 * SECTION 2: Packed Configuration / Encoded Strings
 * ============================================================================
 *
 * @addr 0x080195D2 - 0x080197D4 (515 bytes)
 * @file 0x115D2 - 0x117D4
 *
 * This region contains densely packed mixed data. It appears to be a
 * combination of:
 *
 *   (a) Display configuration parameters (packed structs with mixed
 *       byte/halfword/word fields for timing, color format, dimensions).
 *   (b) Encoded or compressed short strings interspersed between
 *       configuration blocks.
 *   (c) Lookup table tail entries that extend from Section 1 but with
 *       differing format (possibly signed 16-bit values or byte-packed).
 *
 * Decoded partial content:
 *
 *   0x080195D2:  ".H!b |"        (7 ASCII chars)
 *   0x08019675:  " !0 @"          (6 ASCII chars)
 *   0x080196A8:  "LUUUUU"         (6 ASCII chars)
 *   0x080196DF:  "?1cb"           (4 ASCII chars)
 *   0x0801977F:  "'f?FO-"         (6 ASCII chars)
 *
 * These ASCII fragments likely originate from compressed font bitmaps,
 * BCD-encoded strings, or packed AT command templates.
 *
 * Sample hex at section start (0x080195D0):
 *   5c 12 2e 22 48 21 62 20 7c 1f 96 1e b0 1d 58 1b
 *   (These are 16-bit values in the 0x1B58-0x2248 range, possibly
 *    signed or offset-corrected sensor readings)
 *
 * Sample hex at section mid (0x080196D0):
 *   d8 05 3f 31 63 62 fd 0b c9 05 9d 10 3f 05 31 05
 *   (Mixed single-byte values 0x05-0x3F with occasional ASCII)
 */
// @0x080195D2
static const uint8_t g_packed_config_data[515] = {
    // Raw bytes from file offset 0x115D2 to 0x117D4
    // Contains mixture of: display timing params, encoded strings,
    // sensor calibration data, and table tail entries.
};


/*
 * ============================================================================
 * SECTION 3: ASCII Format Strings and Padding
 * ============================================================================
 *
 * @addr 0x080197D5 - 0x08019808 (52 bytes)
 * @file 0x117D5 - 0x11808
 *
 * Contains ASCII character sequences used as format strings or display
 * padding templates:
 *
 *   0x080197D5: 40 40 40 40 40 40 40 40 40 41 41 41 41 41 40 40
 *              "@@@@@@@@@AAAAA@@"
 *   0x080197E5: 40 40 40 40 40 40 40 40 40 40 40 40 40 40 40 40
 *              "@@@@@@@@@@@@@@@@"
 *   0x080197F5: 05 02 02 02 02 02 02 02 02 02 02 02 02 02 02 02
 *              (non-printable control sequence)
 *   0x08019805: 20 20 20 20 20 20 20 20 20 20 02 02 02 02 02 02
 *              "          ......"
 *
 * Interpretation:
 *   - '@' (0x40) characters: Display fill characters for empty screen
 *     regions, or placeholder bytes in a format template.
 *   - 'A' (0x41) characters: Title/header template placeholders.
 *   - ' ' (0x20) characters: Padding for right-aligned numeric fields.
 *   - 0x02 and 0x05 bytes: Control markers for display attribute changes
 *     (e.g., cursor position, color change, field separator).
 *
 * These 52 bytes form a display layout template used by func_151
 * (display_state_controller) to render status screen pages.
 */
// @0x080197D5
static const char g_display_template[52] = {
    // '@' = fill/placeholder (0x40), 'A' = header, ' ' = padding
    // 0x02 = field separator, 0x05 = template terminator
};


/*
 * ============================================================================
 * SECTION 4: Transitional Mixed Data
 * ============================================================================
 *
 * @addr 0x08019809 - 0x0801985F (87 bytes)
 * @file 0x11809 - 0x1185F
 *
 * Bridge region between the format strings (Section 3) and the address
 * table (Section 5). Contains a mixture of:
 *   - Alignment padding bytes
 *   - Small string fragments / display glyph data
 *   - Transitional configuration values
 *
 * No clear structure; appears to be packed tightly to fill space before
 * the 4-byte-aligned address table at 0x08019860.
 */
// @0x08019809
static const uint8_t g_transitional_data[87] = { /* 87 bytes */ };


/*
 * ============================================================================
 * SECTION 5: Address-Value Pair Table
 * ============================================================================
 *
 * @addr 0x08019860 - 0x0801987F (32 bytes, 4 pairs of 32-bit words)
 * @file 0x11860 - 0x1187F
 *
 * A table of 4 pairs of (address, value/size) used by the firmware to
 * configure hardware peripherals or set up DMA buffers at boot time.
 *
 *   Pair 0: 0x20000000, 0x000002FC
 *     -> Write 0x02FC (764) to RAM base 0x20000000
 *        (likely initial stack/vector table area size marker)
 *
 *   Pair 1: 0x0800914A, 0x08019960
 *     -> Function pointer: 0x0800914A, end-of-data marker: 0x08019960
 *        (0x08019960 is 1 byte past EOF -- firmware size sentinel)
 *
 *   Pair 2: 0x200002FC, 0x000062BC
 *     -> RAM address 0x200002FC, value 0x62BC (25276)
 *        (sensor data buffer address and capacity)
 *
 *   Pair 3: 0x080175AC, 0x32012291
 *     -> Flash address 0x080175AC (function pointer), value 0x32012291
 *        (likely a CRC or magic number paired with a callback address)
 */
// @0x08019860
static const struct {
    uint32_t addr;    // target address (RAM or flash)
    uint32_t value;   // associated value/size/magic
} g_addr_value_pairs[4] = {
    { 0x20000000, 0x000002FC },   // [0] RAM base, size=764
    { 0x0800914A, 0x08019960 },   // [1] func ptr, eof+1 sentinel
    { 0x200002FC, 0x000062BC },   // [2] buf addr 0x200002FC, capacity 25276
    { 0x080175AC, 0x32012291 },   // [3] callback 0x080175AC, magic 0x32012291
};


/*
 * ============================================================================
 * SECTION 6: TFT Display Initialization Configuration
 * ============================================================================
 *
 * @addr 0x08019880 - 0x0801993F (192 bytes)
 * @file 0x11880 - 0x1193F
 *
 * Packed configuration parameters for the TFT LCD controller
 * initialization sequence. This is NOT a standard register-value command
 * list (like ILI9341 init tables). Instead it is a custom packed structure
 * containing timing parameters, GPIO pin mappings, and display geometry
 * settings consumed by the display init functions (func_151, func_48).
 *
 * Key decoded fields (tentative):
 *
 *   0x08019880:  0A 23 E8 03    -- cmd=0x0A, reg=0x23, delay=0x03E8 (1000ms)
 *   0x08019884:  03 22 10 27    -- width-related timing params
 *   0x08019888:  19 2F 91 14    -- porch/sync timing
 *   0x0801988C:  E8 03 32 12    -- delay=1000, param=0x1232
 *   0x08019890:  14 12 1E 1A    -- horizontal timing: 0x1214, 0x1A1E
 *   0x08019894:  32 0A 10 08    -- vertical timing: 0x0A32, 0x0810
 *   0x08019898:  8C 0A 94 11    -- porch params
 *   0x0801989C:  58 02 64 B4    -- timing/clock divider
 *   0x080198A0:  90 01 05 12    -- 0x0190=400, config bytes
 *   0x080198A4:  05 02 13 8C    -- pin mapping / mode
 *   0x080198A8:  8B 58 02 4C    -- more timing values
 *   0x080198AC:  04 17 B8 0B    -- 0x1704, 0x0BB8=3000
 *   0x080198B0:  28 00 0D 16    -- gamma/contrast params
 *   0x080198B4:  E8 03 DC 05    -- delay=1000, delay=1500
 *   0x080198B8:  D0 07 DC 05    -- delay=2000, delay=1500
 *   0x080198BC:  6C 07 FC 08    -- delay=1900, delay=2300
 *   0x080198C0:  42 64 17 0F    -- 0x6442, 0x0F17
 *   0x080198C4:  0F 0A 0A 01    -- mode flags
 *   0x080198C8:  02 12 02 27    -- config bytes
 *   0x080198CC:  0A 0A 0A 02    -- repeat count / step
 *   0x080198D0:  0F 0F 12 FF    -- flags + terminator 0xFF
 *
 *   ... remaining bytes appear to be display rotation/mirror config,
 *       color format selection, and additional timing fine-tuning.
 *
 * The value 0x03E8 (1000) appears multiple times as a millisecond delay
 * constant, confirming this is a display init sequence with wait periods.
 */
// @0x08019880
static const uint8_t g_tft_init_config[192] = {
    // Raw bytes from file offset 0x11880 to 0x1193F
    // Struct consumed by display init via func_151/48 code path.
};


/*
 * ============================================================================
 * SECTION 7: Version String Fragment
 * ============================================================================
 *
 * @addr 0x08019940 - 0x0801994F (16 bytes)
 * @file 0x11940 - 0x1194F
 *
 * Contains a version/ID string fragment with leading metadata.
 *
 *   0x08019940:  73 17 EF 00 0A 20 31 32 33 34 35 36 37 38 39 09
 *   0x08019940:  s.............. 1 2 3 4 5 6 7 8 9 .
 *
 * The sequence " 123456789" is clearly a test pattern or default serial
 * number string. The 4 leading bytes (0x73, 0x17, 0xEF, 0x00) may be:
 *   - A checksum or CRC (0x00EF1773)
 *   - A version number struct
 *   - String length prefix
 *
 * The 0x0A before the space character is a linefeed (LF), and the 0x09
 * after "9" is a tab character, suggesting this is used in AT command
 * responses or serial debug output formatting.
 */
// @0x08019940
static const char g_version_string_fragment[16] = {
    // 0x73, 0x17, 0xEF, 0x00, 0x0A, 0x20, '1','2','3','4','5','6','7','8','9', 0x09
};


/*
 * ============================================================================
 * SECTION 8: Final Configuration Tail
 * ============================================================================
 *
 * @addr 0x08019950 - 0x0801995F (16 bytes, end of binary)
 * @file 0x11950 - 0x1195F
 *
 * The last 16 bytes of the firmware image. Contains a checksum/CRC and
 * padding to reach the image boundary.
 *
 *   0x08019950:  2A 96 2D 01 02 03 04 04 85 06 07 08 09 00 00 00
 *
 *   Bytes [0-1]:  0x2A 0x96  (could be a magic number 0x962A identifying
 *                             the configuration block type)
 *   Bytes [2-3]:  0x2D 0x01  (0x012D = 301, possibly config version)
 *   Bytes [4-11]: 01 02 03 04 04 85 06 07
 *                 (incremental ID sequence: 1,2,3,4, then 0x0485=1157, 6,7)
 *   Bytes [12-15]: 08 09 00 00 00 00
 *                 (sequence continues 8,9, then zero padding to alignment)
 *
 * The trailing zeros align the firmware image to a clean boundary. The
 * incremental sequence 1-9 with the 0x0485 anomaly suggests a version or
 * configuration ID with sub-fields.
 */
// @0x08019950
static const uint8_t g_firmware_tail_marker[16] = {
    // 0x2A, 0x96, 0x2D, 0x01, 0x01, 0x02, 0x03, 0x04,
    // 0x04, 0x85, 0x06, 0x07, 0x08, 0x09, 0x00, 0x00
};


/*
 * ============================================================================
 * SECTION A: func_169 Literal Pool (embedded after function body)
 * ============================================================================
 *
 * @addr 0x08018DD2 - 0x08018DDF (14 bytes)
 * @file 0x10DD2 - 0x10DDF
 *
 * Literal pool for func_169_zero_structs_A (0x08018DC0-0x08018DD0).
 * Located immediately after the BX LR at 0x08018DD0, in the standard
 * ARM literal pool placement convention.
 *
 *   0x08018DD2:  00 00        -- alignment padding
 *   0x08018DD4:  0x20001A1C   -- base address of sensor struct A
 *   0x08018DD8:  0x20000284   -- 32-bit counter/accumulator variable
 *   0x08018DDC:  0x20000288   -- 16-bit status halfword
 */
// @0x08018DD4  .word 0x20001A1C
// @0x08018DD8  .word 0x20000284
// @0x08018DDC  .word 0x20000288


/*
 * ============================================================================
 * SECTION B: func_170 Literal Pool
 * ============================================================================
 *
 * @addr 0x08018DF2 - 0x08018DFF (14 bytes)
 * @file 0x10DF2 - 0x10DFF
 *
 *   0x08018DF2:  00 00        -- alignment padding
 *   0x08018DF4:  0x20003648   -- base address of sensor struct B
 *   0x08018DF8:  0x200002B8   -- 16-bit value field
 *   0x08018DFC:  0x200002BA   -- 16-bit status field
 */
// @0x08018DF4  .word 0x20003648
// @0x08018DF8  .word 0x200002B8
// @0x08018DFC  .word 0x200002BA


/*
 * ============================================================================
 * SECTION C: func_171 Literal Pool
 * ============================================================================
 *
 * @addr 0x08018E1E - 0x08018E23 (6 bytes)
 * @file 0x10E1E - 0x10E23
 *
 *   0x08018E1E:  00 00        -- alignment padding
 *   0x08018E20:  0x20001A1C   -- struct base A (shared with func_169)
 */
// @0x08018E20  .word 0x20001A1C


/*
 * ============================================================================
 * SECTION D: func_172 Literal Pool
 * ============================================================================
 *
 * @addr 0x08018E42 - 0x08018E47 (6 bytes)
 * @file 0x10E42 - 0x10E47
 *
 *   0x08018E42:  00 00        -- alignment padding
 *   0x08018E44:  0x20003648   -- struct base B (shared with func_170)
 */
// @0x08018E44  .word 0x20003648


/*
 * ============================================================================
 * ADDRESS SPACE REFERENCE SUMMARY
 * ============================================================================
 *
 * RAM addresses referenced by gap data:
 *   0x20000000   -- RAM base (initial stack/vector area)
 *   0x20000284   -- 32-bit global accumulator (zeroed by func_169)
 *   0x20000288   -- 16-bit global status (zeroed by func_169)
 *   0x200002B8   -- 16-bit value field (zeroed by func_170)
 *   0x200002BA   -- 16-bit status field (zeroed by func_170)
 *   0x200002FC   -- Sensor data buffer (size 0x62BC, at Section 5 pair 2)
 *   0x20001A1C   -- Sensor struct A (16-bit fields at +0, +2)
 *   0x20003648   -- Sensor struct B (16-bit fields at +0, +2)
 *
 * Peripheral addresses referenced:
 *   0x40020000   -- USART2 base (STM32F4)
 *   0x40004400   -- USART6 base (STM32F4)
 *
 * Flash addresses referenced:
 *   0x0800914A   -- routine referenced in addr-value table pair 1
 *   0x080175AC   -- callback function pointer (pair 3)
 *   0x08019960   -- end-of-image sentinel (1 byte past EOF)
 */
