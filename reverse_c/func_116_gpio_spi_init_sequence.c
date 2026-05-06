/**
 * @file func_116_gpio_spi_init_sequence.c
 * @brief 函数: GPIO/SPI 初始化序列 — 构建配置结构并初始化外设
 * @addr  0x08015FA8 - 0x08015FF8 (80 bytes, 含字面量池)
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 构建配置结构体 {0x400, 1, 0, 3, 1} 并初始化 GPIO 端口,
 * 随后调用 func_11770 + func_16048 完成初始化序列.
 *
 * 调用:
 *   func_113B8() @ 0x080113B8 — 资源分配
 *   func_0C42C() @ 0x0800C42C — GPIO 端口初始化
 *   func_11770() @ 0x08011770 — 后续配置
 *   func_16048() @ 0x08016048 — USART 命令
 */

#include "stm32f4xx_hal.h"

extern uint32_t func_113B8(uint32_t a, uint32_t b);
extern void     func_0C42C(volatile void *base, const void *cfg);
extern void     func_11770(void);
extern uint32_t func_16048(void);

void GPIO_SPI_Init_Sequence(void)
{
    struct {
        uint32_t val0;    /* [sp+0] = 0x400 */
        uint8_t  val1;    /* [sp+4] = 1 */
        uint8_t  val2;    /* [sp+6] = 0 */
        uint8_t  val3;    /* [sp+5] = 3 */
        uint8_t  val4;    /* [sp+7] = 1 */
    } cfg;

    func_113B8(1, 1);

    cfg.val0 = 0x400;
    cfg.val1 = 1;
    cfg.val2 = 0;
    cfg.val3 = 3;
    cfg.val4 = 1;

    func_0C42C((volatile void *)0x40020400, &cfg);  /* GPIOB base */

    *(volatile uint32_t *)0x424082A8 = 1;            /* bit-band set flag */
    func_11770();
    uint32_t result = func_16048();
    *(volatile uint16_t *)0x2000028A = result;       /* store result */
}
