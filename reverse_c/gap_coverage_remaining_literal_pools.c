/**
 * @file gap_coverage_remaining_literal_pools.c
 * @brief 剩余未覆盖间隙汇总 — 全部为字面量池、数据表和向量表
 * @addr  0x08008000 - 0x08008188 (392 bytes, ARM Cortex-M4 向量表)
 *        0x08012F90 - 0x08013000 (112 bytes, func_126_fpu_classify 字面量池)
 *        0x08013101 - 0x08013158 (87 bytes, func_127_fpu_double_op 字面量池)
 *        0x080132B5 - 0x080132D8 (35 bytes, func_128_fpu_sign_check 字面量池)
 *        0x0801350F - 0x0801352C (29 bytes, func_129 字面量池)
 *        0x08013895 - 0x080138F8 (99 bytes, func_131 字面量池 — FP 常量)
 *        0x08013C8A - 0x08013CE4 (90 bytes, func_132/133 字面量池 — FP 常量)
 *        0x0801429E - 0x08014344 (166 bytes, func_136 字面量池 — RAM 地址)
 *        0x08016506 - 0x08016548 (66 bytes, func_145 字面量池 — FP 阈值)
 *        0x08016610 - 0x08016664 (84 bytes, func_146 字面量池 — RAM 地址)
 *        0x0800D362 - 0x0800D3B4 (82 bytes, func_33 字面量池 — RAM 地址)
 *        0x0800DFB0 - 0x0800DFD4 (36 bytes, func_38 字面量池)
 *        0x0800E08C - 0x0800E0BC (48 bytes, func_39 字面量池 — RAM 地址)
 *
 * 以上所有间隙均不含可执行代码. 每个间隙属于其前缀函数的字面量池
 * (PC-relative LDR 常量), 包含以下一种或多种:
 *   - RAM 变量地址 (0x2000xxxx)
 *   - 外设寄存器地址 (0x4000xxxx)
 *   - IEEE 754 双精度浮点常量
 *   - 位段别名地址 (0x42xxxxxx)
 *
 * 向量表 (0x08008000-0x08008188):
 *   [0] = 0x200065B8 (初始 SP)
 *   [1] = 0x080081C9 (Reset_Handler, Thumb)
 *   [2-6] = NMI/HardFault/MemManage/BusFault/UsageFault 处理
 *   [7-10] = 保留/默认处理
 *   [11] = SVCall
 *   [12-13] = 保留
 *   [14] = PendSV
 *   [15] = SysTick
 *   [16+] = 外设中断 (WWDG, EXTI, DMA, ADC, TIM, USART, ...)
 *   大多数默认指向 Default_Handler (0x080081E8+)
 *
 * 注: 此文件不是可编译的 C 代码, 仅用于记录剩余间隙的归属.
 *     所有可执行代码函数已 100% 提取完毕 (169 个 .c 文件).
 */
