/**
 * @file func_26_adc_to_display.c
 * @brief 函数: ADC_To_DisplayVal — ADC 采样值转显示数值
 * @addr  0x0800D414 - 0x0800D44C (56 bytes, 含字面量池)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 将 ADC 平均值转换为 TFT 显示用的数值 (温度或电压),
 * 经过乘除缩放、钳位下限、偏移调整后输出。
 *
 * 公式:
 *   raw = ADC_AVG * 0xCE4                    (3300 = 3.3V 参考电压 × 1000)
 *   val = (raw >> 12) & 0xFFFF               (除以 2^12=4096, 取 16-bit)
 *   if val < 0x672: val = 0x672             (钳位下限 = 1650)
 *   result = (val - 0x672) * 1000 / 132     (偏移并缩放)
 *
 * 参数: 无 (从全局 ADC_AVG 读取)
 * 返回: r0 = 显示值 (uint16_t)
 *
 * 注: 叶函数 (无堆栈帧), 仅使用 r0-r3 + bx lr 返回。
 *
 * RAM 变量:
 *   0x20000908 = ADC_AVG (uint16_t, 输入)
 *
 * 字面量池:
 *   0x0800D448: 0x20000908 (ADC_AVG 地址)
 */

#include "stm32f4xx_hal.h"

#define ADC_AVG  (*(volatile uint16_t *)0x20000908)

/* ================================================================
 * ADC_To_DisplayVal() @ 0x0800D414
 *   ADC 采样值 → 显示工程值转换
 * ================================================================ */
uint16_t ADC_To_DisplayVal(void)
{
    uint32_t raw;        /* r1 */
    uint32_t val;        /* r0 */
    uint32_t adjusted;   /* r1 */

    /* 读取 ADC 平均值并缩放 */
    raw = (uint32_t)ADC_AVG * 0xCE4;             /* 0x0800D414-1C: ldrh r2,[r2]; movw r3,#0xce4; mul r1,r2,r3 */

    /* 除以 2^12=4096 (带符号舍入: add sign>>20, 因 raw 恒非负故恒加 0; ubfx 提取 bit[27:12]) */
    val = (raw >> 12) & 0xFFFF;                  /* 0x0800D420-26: asrs r2,r1,#0x1f; add.w r2,r1,r2,lsr#20; ubfx r0,r2,#0xc,#0x10 */

    /* 钳位下限: 若 val < 0x672 (= 1650), 使用下限值 */
    if (val < 0x672) {                           /* 0x0800D42A-2E: asrs r1,r3,#1 (=0xCE4/2=0x672); cmp r0,r1; bge */
        val = 0x672;                             /* 0x0800D430: mov r0,r1 */
    }

    /* 偏移并缩放 */
    adjusted = (val - 0x672) * 1000 / 132;       /* 0x0800D432-42: subw r1,r0,#0x672; mov.w r2,#0x3e8;
                                                  *   muls r1,r2,r1; movs r2,#0x84; sdiv r1,r1,r2; uxth r0,r1 */

    return (uint16_t)adjusted;                   /* 0x0800D444: bx lr */
}
