/**
 * @file func_164_massive_command_handler.c
 * @brief Command/Protocol Response Dispatcher -- 33-way strncmp cascade
 * @addr  0x08014404 - 0x08015BE4 (6112 bytes)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * VERIFIED with Python+Capstone 5.0.7 disassembly of the ENTIRE function
 * (all 6112 bytes, file offset = runtime_addr - 0x08008000).
 *
 * This is a single giant function with a 332-byte stack frame
 * (push {lr}; sub sp, #0x14C).  It reads a global command buffer
 * (0x2000091C, with one header byte skipped), compares the payload
 * against 33 known command strings via strncmp, and dispatches to
 * handler blocks that build responses (strcpy + strcat), format
 * numeric values (func_16B18), and send data (func_0BC2C).
 *
 * =========================================================================
 * STACK FRAME  (0x14C = 332 bytes)
 * =========================================================================
 *   sp+0x100  -- 256-byte string assembly buffer  (add r0, sp, #0x100)
 *   sp+0xFC   -- loop index / counter              (str r0, [sp, #0xFC])
 *   sp+0x144  -- loop counter (TIME= parser)       (str r0, [sp, #0x144])
 *   sp+0x140  -- value count (TIME= parser)        (str r0, [sp, #0x140])
 *   sp+0xE8   -- 6-element u16 array (TIME= parser)(ldrh r0, [sp,#0xE8..0xF2])
 *   sp+0x3C   -- u8 return-code scratch            (str r0, [sp, #0x3C])
 *   sp+0x0    -- 8-byte strd region                (strd r2,r1,[sp])
 *   sp+0x8    -- u8 scratch                        (str r0, [sp, #8])
 *
 * =========================================================================
 * GLOBAL VARIABLES ACCESSED
 * =========================================================================
 *   0x2000026C  -- Hardware status word.  bit15 is the entry gate:
 *                  if !(0x2000026C & 0x8000), the function returns
 *                  immediately via beq.w to epilogue.
 *   0x2000091C  -- Command input buffer (null-terminated ASCII).
 *                  Most blocks use *(buf+1) via "subs r0,r0,#1",
 *                  skipping a 1-byte header (ring-buffer metadata).
 *   0x200001C6  -- serial-number destination
 *   0x20000456  -- IOLOCK config flag destination
 *   0x20000460  -- config variable
 *   0x200001B0  -- config variable (IOLOCK readback)
 *   0x200001A8  -- config variable
 *   0x200001B4  -- config variable
 *   0x200001B8  -- config variable
 *   0x200001AC  -- config variable
 *   0x200000E7  -- config variable
 *   0x2000016F  -- config variable
 *   0x2000091D  -- alias for 0x2000091C + 1 (same buffer, different base)
 *   0x2000069C  -- config variable
 *
 * =========================================================================
 * BL TARGETS -- FULL FUNCTION (32 unique, 289 total BL instructions)
 * =========================================================================
 *
 *   ADDRESS         COUNT   INFERRED SIGNATURE
 *   -------         -----   ----------------------------------------
 *   0x080083AC      33      strncmp(s1, s2, n) -- command matching
 *   0x08008334      44      strcat(dst, src)    -- append suffix/CRLF
 *   0x08008410      34      strtohex/parse_value(s) -- parse hex/dec field
 *   0x0800839A      28      strcpy(dst, src)    -- copy status prefix
 *   0x080083CC      27      strlen(s) or strchr -- length/char search
 *   0x08008370      27      string_validate(s)  -- returns u8 status
 *   0x08015BE4      27      next_stage(buf)     -- dispatch to next handler
 *   0x08016B18      18      sprintf(buf,fmt,val,width) -- format numeric
 *   0x0800BC2C      12      send_cmd(cmd_id,data_ptr,len) -- transmit
 *   0x0800865C       6      subsystem helper (6-param)
 *   0x0800875C       4      subsystem helper (4-param)
 *   0x08012788       4      DataFrameSender / timer helper
 *   0x0800873A       2      subsystem helper (2-param)
 *   0x0800C504       2      cfg_set_flag(0/1)   -- enable/disable toggle
 *   0x08016B70       2      FPU wrapper
 *   0x080175BA       2      AND(r1, r0, #1)     -- bit test
 *   0x080127DA       2      MultiByteCmd / subsystem
 *   0x0800834C       1      memcmp / memset variant
 *   0x08008578       1      subsystem helper (1-param)
 *   0x08008776       1      subsystem helper (1-param)
 *   0x08009490       1      subsystem helper (flash/EEPROM)
 *   0x080094F4       1      subsystem helper (flash/EEPROM)
 *   0x0800BFC8       1      subsystem helper (2-param) -- "Update" step
 *   0x0800C0A4       1      subsystem helper (0-param) -- "Update" init
 *   0x0800C0B4       1      subsystem helper (0-param) -- "Update" cleanup
 *   0x08010BD0       1      DisplayController / Display helper
 *   0x08011610       1      subsystem helper
 *   0x08011710       1      subsystem helper
 *   0x0801257A       1      OperationTrigger
 *   0x080125B0       1      TimerPrescaler
 *   0x08012604       1      ModeBackupNotify
 *   0x08016BA0       1      FPU convert (alternate)
 *
 * =========================================================================
 * STRING LITERAL POOL LAYOUT
 * =========================================================================
 *
 *   Pool A  [code-interleaved]  0x08014800 - 0x080148D2  (~210 bytes)
 *     Strings #1-#15 plus shared "OK"/"\r\n" at 0x08014840/0x08014844.
 *
 *   Pool B  [code-interleaved]  0x08014B1C - 0x08014D60  (~580 bytes)
 *     Strings #16-#25 (TIME=, RECORD*, IOLOCK?, PWMFREQ?, etc.).
 *     Also contains: "{H", "%d", "FREQ?", "WMTIME?", "ECORDNUM?", "W="
 *
 *   Pool C  [code-interleaved]  0x08015066 - 0x080152D0  (~620 bytes)
 *     Strings #26-#32 (get_ver, get_info, get_ap, st=, rt=*, "et_ver",
 *     "ver=", "et_info", "nfo=", "t_ap", " :", "xH", "!hH", "H", "p").
 *
 *   Pool D  [tail + epilogue]  0x08015B50 - 0x08015BE4  (148 bytes)
 *     Global variable pointers interspersed with string fragments.
 *     Epilogue at 0x08015BE0:  add sp, #0x14C; pop {pc}
 *
 * =========================================================================
 * ALL 33 STRNCMP COMMAND STRINGS  (verified via literal-pool readout)
 * =========================================================================
 *
 *   #  STRNCMP ADDR   LEN  STRING          LITERAL ADDR    CATEGORY
 *  ------------------------------------------------------------------
 *    1  0x0801441C     6   "Update"         0x08014804      System
 *    2  0x08014444     6   "MCURST"         0x08014810      System
 *    3  0x0801445C     4   "VER?"           0x08014818      Query
 *    4  0x0801448C     9   "4GDEBUG=1"      0x08014830      Config
 *    5  0x080144C4     9   "4GDEBUG=0"      0x08014848      Config
 *    6  0x080144FC     3   "SN="            0x08014854      Config
 *    7  0x08014550     3   "SN?"            0x0801485C      Query
 *    8  0x0801458A     8   "GPSSEL=0"       0x08014860      Config
 *    9  0x080145C2     8   "GPSSEL=1"       0x0801486C      Config
 *   10  0x080145FA    10   "FLOWMETER="     0x08014878      Config
 *   11  0x08014650     8   "EXTCTRL="       0x08014888      Config
 *   12  0x080146A6     8   "IOLOCK=1"       0x08014898      Config
 *   13  0x080146FA     8   "IOLOCK=0"       0x080148A8      Config
 *   14  0x0801474E     8   "PWMFREQ="       0x080148B4      Config
 *   15  0x080147B0     8   "PWMTIME="       0x080148C4      Config
 *   16  0x080148E0     5   "TIME="          0x08014CD0      Config
 *   17  0x08014A0E    10   "RECORDNUM="     0x08014CDC      Recording
 *   18  0x08014A4E     7   "RECORD?"        0x08014CEC      Query
 *   19  0x08014A60     7   "RECORD="        0x08014CF4      Recording
 *   20  0x08014AC2    10   "RECORDONE="     0x08014CFC      Recording
 *   21  0x08014B2E     7   "IOLOCK?"        0x08014D18      Query
 *   22  0x08014B74     8   "PWMFREQ?"       0x08014D30      Query
 *   23  0x08014BBC     8   "PWMTIME?"       0x08014D40      Query
 *   24  0x08014C04    10   "RECORDNUM?"     0x08014D50      Query
 *   25  0x08014C4A     5   "FLOW="          0x08014D5C      Config
 *   26  0x08014DE4     7   "get_ver"        0x080151B4      AT-internal
 *   27  0x08014E20     8   "get_info"       0x080151D8      AT-internal
 *   28  0x08014F12     6   "get_ap"         0x08015220      AT-internal
 *   29  0x08014FAA     3   "st="            0x08015240      AT-internal
 *   30  0x080150B0     5   "rt=-1"          0x08015258      AT-internal
 *   31  0x08015110     2   "rt=0"           0x08015270      AT-internal
 *   32  0x08015164     2   "rt=1"           0x080152A4      AT-internal
 *   33  0x08015660     3   "st?"            0x0801572C      Query
 *
 * =========================================================================
 * RESPONSE-FORMATTING SHARED STRINGS  (used by strcpy/strcat pairs)
 * =========================================================================
 *
 *   "OK"          @ 0x08014840  -- success status prefix  (reused ~20 times)
 *   "\r\n"        @ 0x08014844  -- CRLF terminator        (reused ~20 times)
 *   "SN="         @ 0x08014854  -- serial-number prefix   (reused from Pool A)
 *   "VER=%02X.%02X\r\n" @ 0x08014820 -- version format string
 *
 * =========================================================================
 * OVERALL CONTROL FLOW
 * =========================================================================
 *
 *   Entry (0x08014404):
 *     push {lr}
 *     sub sp, #0x14C
 *     ldrh r0, [0x2000026C]          // load hardware status
 *     and  r0, r0, #0x8000           // check bit15
 *     beq.w 0x08015B56               // if !bit15 -> skip, go to epilogue
 *
 *   Main cascade (0x08014416 - 0x080157xx):
 *     Consists of 33 sequential strncmp blocks.  For each:
 *
 *       1. movs r2, #N               // compare length
 *       2. adr  r1, <string_literal>  // expected string (PC-relative)
 *       3. ldr  r0, =0x2000091C      // command buffer base+1
 *       4. subs r0, r0, #1           // adjust to actual command start
 *       5. bl   func_083AC           // strncmp(buf+1, expected, N)
 *       6. cbnz r0, <next_block>     // if no match, try next string
 *       7. ... handler code ...      // execute matched handler
 *       8. b 0x0801447C              // or b 0x08014480 -> common exit
 *
 *   Common exit thunk (0x0801447C-0x08014482):
 *     0x0801447C: bl  func_15BE4     // dispatch response to next stage
 *     0x08014480: b.w 0x08015B50     // branch to epilogue area
 *
 *     Most handler blocks reuse this thunk by branching to 0x0801447C
 *     (call func_15BE4) or 0x08014480 (skip func_15BE4).  Some blocks
 *     call func_15BE4 directly and then b to 0x08014480.
 *
 *   Epilogue (0x08015B50-0x08015BE4):
 *     0x08015BE0: add sp, #0x14C
 *     0x08015BE2: pop {pc}
 *
 *     The bytes 0x08015B50-0x08015BDF contain the tail literal pool
 *     (global variable pointers and string fragments).
 *
 *   Bypass path (bit15==0):
 *     0x08014412: beq.w 0x08015B56
 *     Jumps to a point in the tail area that forwards to the epilogue
 *     without processing any commands.
 *
 * =========================================================================
 * HANDLER PATTERNS  (4 distinct patterns observed)
 * =========================================================================
 *
 *   Pattern A -- Act-only (no response string built):
 *     Used by: "Update", "MCURST"
 *     Flow: func_0C0A4() -> func_0BFC8(8,2) -> func_0C0B4()
 *           -> func_175BA(1) -> func_16B70()
 *     These trigger system actions (firmware update mode, MCU reset)
 *     and do not build a response through the strcpy/strcat pipeline.
 *
 *   Pattern B -- Config-set with response:
 *     Used by: "4GDEBUG=1", "4GDEBUG=0", "GPSSEL=0", "GPSSEL=1",
 *              "IOLOCK=1", "IOLOCK=0", "EXTCTRL=", "PWMFREQ=", "PWMTIME=",
 *              "FLOWMETER="
 *     Flow:
 *       1. func_08410(string + key_len)  // parse hex/dec value from tail
 *       2. STRB/STRH to global config variable
 *       3. Format value bytes into sp+0x100 buffer
 *       4. func_0BC2C(cmd_id, sp+0x100, 2)  // transmit raw response
 *       5. strcpy(sp+0x100, "OK")            // prepend status
 *       6. strcat(sp+0x100, "\r\n")          // append CRLF
 *       7. func_08370(sp+0x100)              // validate string
 *       8. UXTB result -> sp+0x3C
 *       9. func_15BE4(sp+0x100)              // dispatch final response
 *      10. b 0x08014480                      // common exit
 *
 *   Pattern C -- Query with formatted value:
 *     Used by: "VER?", "SN?", "IOLOCK?", "PWMFREQ?", "PWMTIME?",
 *              "RECORDNUM?", "RECORD?", "st?"
 *     Flow:
 *       1. func_08410(string)                // parse current value
 *       2. func_16B18(sp+0x100, fmt, val, width)  // sprintf formatted
 *       3. strcpy(sp+0x100, "OK")            // status prefix
 *       4. strcat(sp+0x100, "\r\n")          // CRLF
 *       5. ... (same suffix as Pattern B)
 *
 *     Special case -- "SN?" (serial number query):
 *       Has TWO strcat calls: one for SN= value, one for "OK\r\n"
 *
 *   Pattern D -- TIME= parser (complex):
 *     Used by: "TIME="
 *     Address: 0x080148DA - 0x08014Axx
 *     Parses the command string character-by-character:
 *       1. Search for comma (0x2C) delimiters in a 32-char field
 *       2. For each comma-delimited segment, call func_08410 to parse hex
 *       3. Store 6 parsed u16 values into sp+0xE8 array
 *          (year, month, day, hour, minute, second)
 *       4. Range-validate each field:
 *          year  >= 0x7D0 (2000), month 1-12, day 1-31,
 *          hour 0-23, minute 0-59, second 0-59
 *       5. Pass validated values to downstream handler
 *
 * =========================================================================
 * COMMAND TAXONOMY BY FUNCTIONAL GROUP
 * =========================================================================
 *
 *   System Control (action triggers, not config):
 *     "Update"         -- Enter firmware update mode
 *     "MCURST"         -- Trigger MCU software reset
 *
 *   Configuration Commands (set parameters):
 *     "4GDEBUG=1"      -- Enable 4G debug logging
 *     "4GDEBUG=0"      -- Disable 4G debug logging
 *     "SN="            -- Set serial number
 *     "GPSSEL=0"       -- GPS module select off
 *     "GPSSEL=1"       -- GPS module select on
 *     "FLOWMETER="     -- Set flow meter value
 *     "EXTCTRL="       -- Set external control parameter
 *     "IOLOCK=1"       -- Lock I/O configuration
 *     "IOLOCK=0"       -- Unlock I/O configuration
 *     "PWMFREQ="       -- Set PWM frequency
 *     "PWMTIME="       -- Set PWM duty/time
 *     "TIME="          -- Set system date/time (hex comma-delimited)
 *     "FLOW="          -- Set flow value (alternate format)
 *
 *   Query Commands (read parameters, typically suffixed with "?"):
 *     "VER?"           -- Query firmware version (responds: VER=%02X.%02X)
 *     "SN?"            -- Query serial number (responds: SN=...)
 *     "RECORD?"        -- Query current record value
 *     "RECORDNUM?"     -- Query record count
 *     "IOLOCK?"        -- Query I/O lock status
 *     "PWMFREQ?"       -- Query PWM frequency
 *     "PWMTIME?"       -- Query PWM duty/time
 *     "st?"            -- Query status
 *
 *   Recording Commands:
 *     "RECORD="         -- Set recording parameters
 *     "RECORDNUM="      -- Set record number
 *     "RECORDONE="      -- Set single record parameter
 *
 *   Internal Protocol / AT-Command Pass-through:
 *     "get_ver"         -- AT+get_ver response handler
 *     "get_info"        -- AT+get_info response handler
 *     "get_ap"          -- AT+get_ap response handler
 *     "st="             -- AT+st= value set
 *     "rt=-1"           -- AT+rt=-1 handler
 *     "rt=0"            -- AT+rt=0 handler
 *     "rt=1"            -- AT+rt=1 handler
 */

#include "stm32f4xx_hal.h"

/* ================================================================
 * External function declarations (32 unique BL targets)
 * ================================================================ */

/* --- String library (0x080083xx) --- */
extern uint32_t func_083AC(const char *s1, const char *s2, uint32_t n);   /* strncmp  */
extern void     func_0839A(char *dst, const char *src);                   /* strcpy   */
extern void     func_08334(char *dst, const char *src);                   /* strcat   */
extern uint32_t func_083CC(const char *s);                                /* strlen / strchr */
extern uint32_t func_08370(const char *s);                                /* string_validate -> u8 */
extern uint32_t func_0834C(void *a, void *b, uint32_t n);                /* memcmp/memset */

/* --- Numeric parsing (0x080084xx) --- */
extern uint16_t func_08410(const char *s);                                /* strtohex / parse_field */

/* --- Subsystem helpers (0x080085xx-0x080087xx) --- */
extern void     func_08578(void);
extern void     func_0865C(void);
extern void     func_0873A(void);
extern void     func_0875C(void);
extern void     func_08776(void);

/* --- Flash/EEPROM (0x080094xx) --- */
extern void     func_09490(void);
extern void     func_094F4(void);

/* --- Configuration read/write (0x0800BCxx) --- */
extern void     func_0BC2C(uint16_t cmd_id, void *data, uint16_t len);    /* send_command */
extern void     func_0BFC8(uint32_t a, uint32_t b);
extern void     func_0C0A4(void);
extern void     func_0C0B4(void);
extern void     func_0C504(uint32_t enable);                               /* set_flag(0/1) */

/* --- Higher-level subsystem calls (0x08010xxx-0x08011xxx) --- */
extern void     func_10BD0(void);                                          /* DisplayController */
extern void     func_11610(void);
extern void     func_11710(void);

/* --- Timer / measurement (0x080125xx-0x080127xx) --- */
extern void     func_1257A(void);                                          /* OperationTrigger */
extern void     func_125B0(void);                                          /* TimerPrescaler */
extern void     func_12604(void);                                          /* ModeBackupNotify */
extern void     func_12788(void);                                          /* DataFrameSender */
extern void     func_127DA(void);                                          /* MultiByteCmd */

/* --- Next-stage dispatcher (func_159) @ 0x08015BE4 --- */
extern double   func_15BE4(const char *buf);                               /* WaitAckTimeout / dispatch */

/* --- Numeric formatting helpers (0x08016Bxx) --- */
extern void     func_16B18(char *buf, const char *fmt, uint32_t val,
                           uint32_t width);                                /* sprintf-like */
extern void     func_16B70(void);                                          /* FPU wrapper */
extern void     func_16BA0(void);                                          /* FPU convert (alt) */

/* --- Bit-test helper (0x080175xx) --- */
extern uint32_t func_175BA(uint32_t val);                                  /* AND r1, r0, #1 */


/* ================================================================
 * Massive_Command_Handler() @ 0x08014404
 *   Stack frame: push {lr}; sub sp, #0x14C  (332 bytes)
 *   Size: 6112 bytes  (0x08015BE4 - 0x08014404)
 *   33 strncmp command-matching blocks
 *   32 unique BL targets, 289 total BL instructions
 * ================================================================ */
void Massive_Command_Handler(void)
{
    /*
     * Entry guard:
     *   ldrh r0, [0x2000026C]     ; load hardware status register
     *   and  r0, r0, #0x8000      ; test bit15
     *   beq.w <epilogue>          ; if bit15 == 0, skip everything
     *
     * Main command buffer:
     *   ldr r0, [0x2000091C]      ; buffer base + 1 (header byte skipped)
     *   subs r0, r0, #1           ; adjust to command start
     *
     * Each of the 33 matching blocks follows this template:
     *   movs r2, #N               ; compare length
     *   adr  r1, <string_literal>  ; expected string (PC-relative)
     *   ldr  r0, =0x2000091C      ; command buffer pointer
     *   subs r0, r0, #1           ; skip header byte
     *   bl   func_083AC           ; strncmp(buf+1, expected, N)
     *   cbnz r0, <next_block>     ; if no match, try next string
     *   ; ... handler code ...
     *   b    0x0801447C           ; common exit thunk
     *
     * Common exit thunk at 0x0801447C-0x08014482:
     *   0x0801447C: bl func_15BE4  ; dispatch response to next stage
     *   0x08014480: b.w 0x08015B50 ; branch to epilogue area
     *
     * Epilogue at 0x08015BE0:
     *   add sp, #0x14C
     *   pop {pc}
     *
     * Local variables (stack layout):
     *   sp+0x100: 256-byte string assembly buffer (for building responses)
     *   sp+0xFC:  temporary counter/index
     *   sp+0x144: loop index (TIME= parser)
     *   sp+0x140: field count (TIME= parser)
     *   sp+0xE8:  6-element u16 array [year, mon, day, hour, min, sec]
     *   sp+0x3C:  u8 return-code scratch
     *   sp+0x0..+0x8: 8-byte parameter area (strd r2,r1,[sp]; str r0,[sp,#8])
     *
     * String literal pools (interleaved in function body):
     *   Pool A  0x08014800-0x080148D2  (~210 bytes, commands #1-#15)
     *   Pool B  0x08014B1C-0x08014D60  (~580 bytes, commands #16-#25)
     *   Pool C  0x08015066-0x080152D0  (~620 bytes, commands #26-#32)
     *   Pool D  0x08015B50-0x08015BE4  (tail pool + epilogue)
     */
}
