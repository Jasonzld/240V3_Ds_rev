/**
 * @file func_145_adc_value_scaler.c
 * @brief 函数: ADC 值缩放器 — 复杂算术运算后将结果存储到 PWM 相关寄存器
 * @addr  0x0801844C - 0x08018488 (60 bytes, 含字面量池)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 输入: r0 = raw_adc_value
 * 处理:
 *   1. func_12886(~4) — 某种配置调用
 *   2. val = (raw_adc + (raw_adc >> 29)) >> 3  (向零取整除以 8)
 *   3. 存储到 0x200001F8 (模式字节, UBFX 截断为 8 位)
 *   4. 计算 PWM 值: ((val << 7) - val * 3) << 3, 取低 16 位
 *   5. 存储 PWM 值到 0x200001FA
 *
 * 调用:
 *   func_12886() @ 0x08012886 — 配置/初始化
 */

#include "stm32f4xx_hal.h"

extern void func_12886(uint32_t val);

#define PWM_MODE_BYTE  (*(volatile uint8_t  *)0x200001F8)
#define PWM_DUTY_HWORD (*(volatile uint16_t *)0x200001FA)

void ADC_Value_Scaler(int32_t raw_adc)
{
    int32_t val;

    func_12886(~4U);                             /* 配置调用 */

    /*
     * 0x08018458: ASRS sign,raw_adc,#31; ADD.W val,raw_adc,sign,LSR #29
     *   → raw_adc + (raw_adc < 0 ? 7 : 0)
     * 0x0801845E: UBFX val,val,#3,#8 → 右移 3 位并截断为 8 位
     */
    val = ((raw_adc + ((raw_adc < 0) ? 7 : 0)) >> 3) & 0xFF;

    /* 存储模式字节 */
    PWM_MODE_BYTE = (uint8_t)val;

    /* 计算 PWM 值: (val * 128 - val * 3) * 8 = val * 1000 */
    /* sum = val + val*2 = val*3 */
    /* pwm = (val*128 - val*3) << 3 */
    uint32_t pwm = (uint32_t)((val << 7) - (val + val * 2)) << 3;
    PWM_DUTY_HWORD = (uint16_t)(pwm & 0xFFFF);
}
