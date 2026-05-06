/**
 * @file func_01_main.c
 * @brief 函数: Application_Entry (main) — 完整还原版
 * @addr  0x08018524 - 0x08018B66 (1602 bytes code)
 *        NOTE: 0x08018BB0+ is a separate function (push {r3-r7,lr}).
 *        Overlaps with func_146_systick_timer_scheduler.c (0x08018488-0x08018B66).
 *        This file provides application-layer semantic view; func_146 provides
 *        SysTick infrastructure + verified BL targets.
 * DISASSEMBLY-TRACED. Verified against Capstone.
 *
 * 固件主入口, 负责:
 *   1. 系统时钟和外设初始化 (含 Flash 备份域检查/系统复位)
 *   2. FSMC TFT 液晶屏初始化
 *   3. 配置参数从 Flash 加载 (4 个配置块)
 *   4. 主事件循环 (超循环结构):
 *      - 电流/电压 ADC 采样与处理
 *      - 运行状态机 (启动/运行/停止)
 *      - 水阀控制, 蠕动泵控制
 *      - 运行计时与倒计时管理
 *      - TFT 显示更新
 *      - 4G 模块数据收发
 *      - 多阶段测量平均与阈值判断
 *      - GPIO 输出控制 (电解槽开关等)
 *      - 定时器/看门狗管理
 *
 * 常量池: 0x0801893C-0x08018A3A (代码段1引用), 0x08018B66-0x08018BAE (代码段2引用)
 */

#include "stm32f4xx_hal.h"  /* SMT32F410 系列 */
#include "app_config.h"

/* ================================================================
 * 全局变量 (RAM 地址 → 符号名映射)
 * ================================================================ */

#define FLAG_BACKUP         (*(uint32_t *)0x08004000)  /* Flash 备份标志 */
#define RAM_FLAGS           (*(uint32_t *)0x200001F4)  /* RAM 标志字 */
#define CONFIG_BASE         ((cfg_block_t *)0x200003EC) /* 配置结构体基址 */
#define FSMC_BASE           (*(volatile uint32_t *)0x42410000)
#define FSMC_BANK1_REG      (*(volatile uint32_t *)0x4241028C)
#define FSMC_BANK1_REG2     (*(volatile uint32_t *)0x42410288)
#define RAM_ADDR_0194       (*(uint32_t *)0x20000194)  /* RTC 时间值 */
#define RAM_ADDR_01C4       (*(uint16_t *)0x200001C4)  /* 超时计数值 (TFT) */
#define RAM_ADDR_0908       (*(uint16_t *)0x20000908)  /* ADC 采样值(数组) */
#define RAM_ADDR_006C       (*(uint16_t *)0x2000006C)  /* 当前电流 ADC 值 */
#define RAM_ADDR_0174       (*(uint8_t  *)0x20000174)  /* 运行状态标志 */
#define RAM_ADDR_0175       (*(uint8_t  *)0x20000175)  /* 启动模式标志 */
#define RAM_ADDR_0177       (*(uint8_t  *)0x20000177)  /* 运行阶段 */
#define RAM_ADDR_0178       (*(uint16_t *)0x20000178)  /* 阶段计时器 */
#define RAM_ADDR_017B       (*(uint8_t  *)0x2000017B)  /* 蠕动泵周期标志 */
#define RAM_ADDR_017A       (*(uint8_t  *)0x2000017A)  /* 水阀状态标志 */
#define RAM_ADDR_01B8       (*(uint32_t *)0x200001B8)  /* 系统运行计时(秒) */
#define RAM_ADDR_01B4       (*(uint32_t *)0x200001B4)  /* 目标运行时间 */
#define RAM_ADDR_01BC       (*(uint8_t  *)0x200001BC)  /* 运行周期计数 */
#define RAM_ADDR_00E6       (*(uint8_t  *)0x200000E6)  /* 调试打印使能 */
#define RAM_ADDR_01AC       (*(uint32_t *)0x200001AC)  /* 倒计时(秒) */
#define RAM_ADDR_01A8       (*(uint32_t *)0x200001A8)  /* 启动模式: 0=锁存 1=点动 */
#define RAM_ADDR_01BD       (*(uint8_t  *)0x200001BD)  /* 运行完成标志 */
#define RAM_ADDR_01E0       (*(uint8_t  *)0x200001E0)  /* TFT 初始化完成标志 */
#define RAM_ADDR_01E2       (*(uint16_t *)0x200001E2)  /* 4G 模块心跳计数 */
#define RAM_ADDR_04B4       (*(uint32_t *)0x200004B4)  /* 电解参数结构体 */
#define RAM_ADDR_04AA       (*(uint16_t *)0x200004AA)  /* 配置 CRC 校验 */
#define RAM_ADDR_01C8       (*(uint32_t *)0x200001C8)  /* 目标电流值(float) */
#define RAM_ADDR_01CC       (*(uint32_t *)0x200001CC)  /* 当前电流值(float) */
#define RAM_ADDR_048A       ((char *)0x2000048A)       /* 格式化字符串 buf1 */
#define RAM_ADDR_046A       ((char *)0x2000046A)       /* 格式化字符串 buf2 */

/* ---- 循环阶段变量 (0x080187EA-0x08018B64) ---- */
#define RAM_ADC_AVG         (*(uint16_t *)0x20000040)  /* ADC 平均值 */
#define RAM_ARR_IDX         (*(uint16_t *)0x20000038)  /* 数组索引(采样通道数) */
#define RAM_ARR_A           (*(int16_t  *)0x20000116)  /* 通道A 历史值数组 */
#define RAM_VAL_A            (*(int16_t  *)0x2000003A)  /* 通道A 当前计算值 */
#define RAM_ARR_B           (*(int16_t  *)0x20000110)  /* 通道B 历史值数组 */
#define RAM_VAL_B            (*(int16_t  *)0x2000003C)  /* 通道B 当前计算值 */
#define RAM_MODE_FLAG       (*(uint8_t  *)0x20000019)  /* 模式标志 (0=仅通道A/B, 非0=含阈值比较) */
#define RAM_LIMIT_A_MIN     (*(uint16_t *)0x2000004E)  /* 通道A 下限阈值 */
#define RAM_LIMIT_A_MAX     (*(uint16_t *)0x20000050)  /* 通道A 上限阈值 */
#define RAM_LIMIT_B_MIN     (*(uint16_t *)0x2000004A)  /* 通道B 下限阈值 */
#define RAM_LIMIT_B_MAX     (*(uint16_t *)0x2000004C)  /* 通道B 上限阈值 */
#define RAM_OUTPUT_FLAGS    (*(uint16_t *)0x200000E0)  /* 输出控制标志字
                                                          bit0: 电解槽开关
                                                          bit1: 水阀/继电器2
                                                          bit2: 输出3
                                                          bit3: 输出4 */
#define RAM_VAL_A_RAW       (*(int16_t  *)0x2000003E)  /* 通道A 原始值(用于比较) */
#define RAM_OUT3_MASK        (*(uint8_t  *)0x200000D9)  /* 输出3 使能标志 */
#define RAM_OUT4_MASK        (*(uint32_t *)0x2000007C)  /* 输出4 使能标志 */
#define RAM_4G_SEND_FLAG    (*(uint8_t  *)0x200001BE)  /* 4G 发送标志 */
#define RAM_SPEED_VAL       (*(uint16_t *)0x2000011E)  /* 速度/频率值 */
#define RAM_WATCHDOG_FLAG   (*(uint8_t  *)0x200000F5)  /* 看门狗触发标志 */
#define RAM_PERIOD_TIMER    (*(uint16_t *)0x20000188)  /* 周期定时器(倒计时) */
#define RAM_PERIOD_VAL      (*(uint16_t *)0x2000018A)  /* 周期值(除数) */
#define RAM_ACCUMULATOR     (*(uint8_t  *)0x2000000C)  /* 累加器(+10/次) */
#define RAM_TIM1_FLAG       (*(uint8_t  *)0x20000198)  /* TIM1 控制标志 */
#define RAM_TIM1_COUNTER    (*(uint32_t *)0x2000019C)  /* TIM1 计数器 */
#define RAM_TIM1_LIMIT      (*(uint32_t *)0x200001A4)  /* TIM1 上限 */
#define RAM_WDT_COUNTER     (*(uint16_t *)0x200001C2)  /* 看门狗计数器 */
#define RAM_MODE_STATE      (*(uint8_t  *)0x200001BF)  /* 模式状态 */

/* FSMC TFT 控制寄存器 */
#define FSMC_OUT1_REG       (*(volatile uint32_t *)0x424102A8)  /* 输出1: 0x4241028C + 0x1C */
#define FSMC_OUT2_REG       (*(volatile uint32_t *)0x424102AC)  /* 输出2: 0x4241028C + 0x20 */
#define FSMC_OUT3_REG       (*(volatile uint32_t *)0x424002BC)  /* 输出3 */

/* TIM1 基址 0x40010000 */
#define TIM1_BASE           ((volatile uint32_t *)0x40010000)
#define TIM1_CCR1           (*(volatile uint32_t *)0x4001002C)

/* ================================================================
 * 外部函数声明 (逆向识别)
 * ================================================================ */
extern void delay_ms(uint32_t ms);                       /* 0x0801844C */
extern void delay_us(uint32_t us);                       /* 0x08018488 */
extern void RCC_EnableClock(uint32_t periph_mask);       /* 0x08018EC8 */
extern void Flash_Unlock(void);                          /* 0x0800C0A4 */
extern void Flash_Write(uint32_t addr, uint32_t data);   /* 0x0800C0C8 */
extern void Flash_Lock(void);                            /* 0x0800C0B4 */
extern void GPIO_Init(void);                             /* 0x0800C2DC */
extern void USART1_Init(uint32_t mask);                  /* 0x0800D8C4 */
extern void USART6_Init(void);                           /* 0x080099F8 */
extern void ADC_DMA_Init(void);                          /* 0x080091E8 */
extern void TIM_Init_PhaseCtrl(uint32_t psc, uint32_t arr);  /* 0x08013344 */
extern void TIM_Init_PumpCtrl(uint32_t psc, uint32_t arr);   /* 0x08013198 */
extern void TIM_Init_PWM(uint32_t psc, uint32_t arr);         /* 0x080132D8 */
extern void TIM_Init_SoftStart(uint32_t psc, uint32_t arr);   /* 0x08012924 */
extern void GPIOB_Handler_Init(void);                    /* 0x08015FA8 */
extern void GPIO_IRQ_Handler_Init(void);                 /* 0x08013000 */
extern void Config_Load(uint32_t *dst, const void *src); /* 0x0800839A */
extern void RTC_Init(void);                              /* 0x0800C8B0 */
extern uint32_t RTC_GetTime(void);                       /* 0x0800C7A0 */
extern void USART1_Process(void);                        /* 0x0800D85C */
extern void Pump_Start(void);                            /* 0x0800A634 */
extern void Pump_Stop(void);                             /* 0x0800D558 */
extern void Buzzer_Beep(void);                           /* 0x08009C84 */
extern void SSR_WaterValve_Open(void);                   /* 0x08009B6C */
extern void Init_WaterValve(void);                       /* 0x080094F4 */
extern void TFT_DrawStatus(void);                        /* 0x0800A64C */
extern uint32_t Flash_ReadConfig(uint16_t *buf, uint32_t *dst); /* 0x0800D458 */
extern float Int32_to_Float(int32_t val);                /* 0x0800875C */
extern float Float_Div(float a, float b);                /* 0x0800865C */
extern float Float_Mul_100(float val);                   /* 0x0800883C */
extern float Float_To_Int(float val);                    /* 0x080087B4 */
extern void Format_String(char *buf, const char *fmt, float v1, float v2); /* 0x08016B18 */
extern void System_Reset(void);                          /* inline: SCB AIRCR */

/* 循环中调用的附加函数 */
extern void Func_ClearFlag(uint32_t val);                /* 0x0800C504 */
extern void Func_4G_Send(char *data, uint8_t len);       /* 0x08015BE4 */
extern uint8_t Func_ProcessData(char *data);             /* 0x08008370 */
extern void Func_MainLoop_A(void);                       /* 0x08016548 */
extern void Func_DisplayUpdate(void);                    /* 0x0800D834 */
extern uint16_t Func_GetADCValue(void);                  /* 0x0800D414 */
extern void Func_PhaseA_Init(void);                      /* 0x0800961C */
extern void Func_PhaseA_Process(void);                   /* 0x0800963C */
extern void Func_PhaseA_Finish(void);                    /* 0x080095F8 */
extern int16_t Func_GetCalibA(void);                     /* 0x0800C538 */
extern int16_t Func_GetCalibB(void);                     /* 0x0800C710 */
extern void Func_TimerISR_Main(void);                    /* 0x08013CE4 */
extern void Func_Watchdog_Feed(void);                    /* 0x0800BDCC */
extern void Func_Watchdog_Check(void);                   /* 0x0800BED8 */
extern void Func_PeriodicTask(void);                     /* 0x0800D5E4 */
extern void Func_500CycleTask(void);                     /* 0x080115E8 */
extern void Func_TIM1_Update(uint32_t base, uint32_t val);/* 0x080134A0 */
extern void Func_PostLoop(void);                         /* 0x08014404 */
extern void Func_TIM1_Increment(uint32_t val);           /* 0x080116D0 */
extern void Func_LoopTail(void);                         /* 0x0800D730 */

/* ================================================================
 * main() - 应用程序入口 (完整还原)
 * ================================================================ */
int main(void)
{
    /* ---- 阶段1: 硬件初始化 ---- */

    delay_ms(100);                          /* 0x08018532 */
    RCC_EnableClock(0x1C200);               /* 0x08018538:
                                             * RCC_AHB1:  GPIOA|GPIOB|GPIOC
                                             * RCC_APB1:  USART2|TIM4|I2C1
                                             * RCC_APB2:  USART1|USART6|ADC1|SPI1 */

    /* 检查 Flash 备份域标志 — 首次上电需要初始化 */
    if (FLAG_BACKUP == 0xFFFFFFFF) {        /* 0x08018540 */
        Flash_Unlock();                     /* 0x08018548 */
        Flash_Write(0x08004000, RAM_FLAGS); /* 0x0801854C */
        Flash_Lock();                       /* 0x08018556 */
        /* 请求系统复位 (写 SCB AIRCR: VECTKEY + SYSRESETREQ) */
        SCB->AIRCR = (SCB->AIRCR & 0x700) | 0x05FA0000 | 0x04; /* 0x0801855C */
        __DSB();                            /* 0x08018574 */
        while(1) { __NOP(); }               /* 0x0801857A: 等待复位 */
    }

    /* 正常启动路径 */
    GPIO_Init();                            /* 0x0801857E */

    /* ---- TFT 液晶屏初始化 (FSMC 总线) ---- */
    FSMC_BANK1_REG = 1;                     /* 0x08018582: 0x4241028C */
    delay_us(1000);                         /* 0x08018588: 等待 TFT 上电 (1ms=1000us) */
    FSMC_BASE[0xA2] = 1;                    /* 0x08018590: *(0x42410288) */
    delay_us(1000);                         /* 0x08018598: 1ms 延时 */
    FSMC_BANK1_REG2 = 0;                    /* 0x080185A0: 0x42410288 */
    delay_us(1000);                         /* 0x080185A8: 1ms 延时 */

    /* ---- 外设初始化 ---- */
    {
        extern void init_func_d748(void);   /* 0x0800D748 */
        init_func_d748();                   /* 0x080185AE */
    }
    USART1_Init(0x1C200);                   /* 0x080185B2: TFT 串口屏 */
    {
        extern void init_func_9436(void);   /* 0x08009436 */
        init_func_9436();                   /* 0x080185B6 */
    }
    TIM_Init_PhaseCtrl(0x2710, 0x2710 + 1); /* 0x080185BC: 电解阶段控制 */
    TIM_Init_PumpCtrl(0x4E20, 0x270F);      /* 0x080185CA: 蠕动泵控制 */
    TIM_Init_PWM(0xFFFF, 0xC34F);           /* 0x080185D6: PWM 输出 */
    TIM_Init_SoftStart(0xFFFF, 0xC34F);     /* 0x080185E2: 电解槽软启动 */
    GPIO_IRQ_Handler_Init();                /* 0x080185EE */
    USART6_Init();                          /* 0x080185F2: 4G/GPS 模块 */
    GPIOB_Handler_Init();                   /* 0x080185F6 */
    ADC_DMA_Init();                         /* 0x080185FA: 电流/电压 ADC */

    /* ---- 阶段2: 配置参数加载 ---- */
    Config_Load(&CONFIG_BASE->block0, &FLASH_CONFIG_0);   /* 0x080185FE */
    Config_Load(&CONFIG_BASE->block1, &FLASH_CONFIG_1);   /* 0x08018606 */
    Config_Load(&CONFIG_BASE->block2, &FLASH_CONFIG_2);   /* 0x08018610 */
    Config_Load(&CONFIG_BASE->block3, &FLASH_CONFIG_3);   /* 0x0801861A */

    RTC_Init();                             /* 0x08018624 */
    RAM_ADDR_0194 = RTC_GetTime();          /* 0x08018628 */

    /* 检查 TFT 是否就绪 (FSMC 总线读取 0x42410200) */
    if (FSMC_BASE[0x80] != 0) {            /* 0x08018630 */
        RAM_ADDR_01C4 = 1000;               /* 0x08018638: 1秒超时 */
    }

    /* ---- 阶段3: 主事件循环 ---- */
    while (1)                               /* 0x08018640 → 0x08018B64 */
    {
        /* ======================================================== */
        /* 3.1 读取当前电流值 */
        /* ======================================================== */
        {
            extern void func_pre_usart1(void);  /* 0x0800C654 */
            func_pre_usart1();              /* 0x08018642 */
        }
        RAM_ADDR_006C = RAM_ADDR_0908[1];   /* 0x08018646: ADC 电流通道 */
        USART1_Process();                   /* 0x0801864A: 处理 TFT 通信 */

        /* ======================================================== */
        /* 3.2 状态机: 运行模式判断 */
        /* ======================================================== */
        if (!RAM_ADDR_0174 && !RAM_ADDR_0175)  /* 0x08018652: 待机且未启动 */
        {
            /* 启动条件满足: 开始电解 */
            RAM_ADDR_0175 = 5;              /* 0x0801865E: 模式=启动 */
            Pump_Start();                   /* 0x08018664: 启动蠕动泵 */
        }
        else if (RAM_ADDR_0177)             /* 0x0801866A: 运行阶段 */
        {
            /* 运行阶段: 计时中 */
            if (RAM_ADDR_0178 == 0)         /* 0x08018670 */
            {
                RAM_ADDR_0178 = 3600;       /* 0x08018676: 默认 3600 秒 */
                Pump_Stop();                /* 0x0801867E: 停止蠕动泵 */
            }
        }
        else
        {
            Buzzer_Beep();                  /* 0x08018684: 蜂鸣器提示 */
        }

        /* ======================================================== */
        /* 3.3 水阀控制 — 首次启动初始化 */
        /* ======================================================== */
        if (!RAM_ADDR_017B)                 /* 0x08018688 */
        {
            RAM_ADDR_017B = 10;             /* 0x0801868E: 标志=10 */
            Init_WaterValve();              /* 0x08018694: 初始化水阀 */
        }

        /* ======================================================== */
        /* 3.4 运行计时管理 */
        /* ======================================================== */
        if (RAM_ADDR_01B8 == 0)             /* 0x08018698 */
        {
            /* 初始化运行计时 */
            RAM_ADDR_01B8  = RAM_ADDR_01B4; /* 0x080186A0: 加载目标时间 */
            RAM_ADDR_01BC += 1;             /* 0x080186A8: 周期计数++ */

            if (RAM_ADDR_00E6)              /* 0x080186B2: 调试使能 */
            {
                /* 调试打印 "st?" → USART1 */
                extern void Debug_Print(const char *s); /* 0x08009B6C */
                Debug_Print("st?");         /* 0x080186B8 */
            }
        }

        /* ======================================================== */
        /* 3.5 倒计时与启停模式管理 */
        /* ======================================================== */
        if (RAM_ADDR_01AC > 0)              /* 0x080186BE */
        {
            RAM_ADDR_01AC -= 1;             /* 0x080186C4: 倒计时-- */
        }
        else
        {
            /* 倒计时到: 判断启停模式 */
            uint8_t limit;

            if (RAM_ADDR_01A8)              /* 0x080186D0: cbnz → A8!=0 */
            {
                limit = 15;                 /* 0x080186DC: 点动模式上限=15 */
            }
            else
            {
                limit = 1;                  /* 0x080186D6: 锁存模式上限=1 */
            }

            if (RAM_ADDR_01BC > limit)      /* 0x080186E0 */
            {
                RAM_ADDR_01BD = 1;          /* 0x080186E8: 运行完成 */
            }
            else
            {
                RAM_ADDR_01BD = 0;          /* 0x080186F0: 继续运行 */
            }

            RAM_ADDR_01BC = 0;              /* 0x080186F6: 清零周期计数 */

            if (RAM_ADDR_01A8)              /* 0x080186FC: cbnz → A8!=0 点动 */
            {
                RAM_ADDR_01AC = 20;         /* 0x0801870A: 点动模式=20秒 */
            }
            else
            {
                RAM_ADDR_01AC = 2;          /* 0x08018702: 锁存模式=2秒 */
            }
        }

        /* ======================================================== */
        /* 3.6 TFT 显示更新 */
        /* ======================================================== */
        if (!RAM_ADDR_01E0)                 /* 0x08018710 */
        {
            TFT_DrawStatus();               /* 0x08018716 */
            goto loop_tail;                 /* 0x0801871A → 0x080187EA */
        }

        /* ======================================================== */
        /* 3.7 4G 模块数据处理 */
        /* ======================================================== */
        if (RAM_ADDR_01E2 != 0)             /* 0x0801871C: beq → skip_4g */
        {
            uint32_t cfg_ok;
            cfg_ok = Flash_ReadConfig(&RAM_ADDR_04AA, &RAM_ADDR_04B4); /* 0x08018724 */

            if (cfg_ok != 0)                /* 0x0801872C: bne → dec_4g_ctr */
            {
                /* CRC 校验失败: 跳过数据处理, 直接递减计数 */
                RAM_ADDR_01E2 -= 1;         /* 0x080187D8 */
                goto loop_tail;             /* 0x080187E2 */
            }

            /* CRC 校验通过: 换算电解参数 */
            /* 目标电流值 */
            {
                int32_t target_raw;
                float   target_amps;

                target_raw   = RAM_ADDR_04B4[3];          /* 偏移+0xC */
                target_amps  = Int32_to_Float(target_raw);
                target_amps  = Float_Div(target_amps, 1000.0f);
                target_amps  = Float_Mul_100(target_amps);
                RAM_ADDR_01C8 = *(uint32_t *)&target_amps;
            }

            /* 当前电流值 */
            {
                int32_t current_raw;
                float   current_amps;

                current_raw  = RAM_ADDR_04B4[1];          /* 偏移+0x4 */
                current_amps = Int32_to_Float(current_raw);
                current_amps = Float_Div(current_amps, 1000.0f);
                current_amps = Float_Mul_100(current_amps);
                RAM_ADDR_01CC = *(uint32_t *)&current_amps;
            }

            /* 格式化显示字符串 → TFT 显示 */
            {
                float f_current_display;
                f_current_display = Float_To_Int(*(float *)&RAM_ADDR_01CC);
                Format_String(RAM_ADDR_048A, "%f", 0, f_current_display);

                float f_target_display;
                f_target_display  = Float_To_Int(*(float *)&RAM_ADDR_01C8);
                Format_String(RAM_ADDR_046A, "%f", 0, f_target_display);
            }

            RAM_ADDR_017A = 1;              /* 水阀已开启标志 */

            /* ---- 4G 数据发送处理 ---- */
            Func_ClearFlag(0);              /* 0x080187AE */

            if (!RAM_4G_SEND_FLAG)          /* 0x080187B4: cbz → dec_4g_ctr */
            {
                /* 无需发送: 直接递减计数 */
                RAM_ADDR_01E2 -= 1;         /* 0x080187D8 */
                goto loop_tail;             /* 0x080187E2 */
            }

            /* 格式化并发送: "LJ%s,LW%s\r\n" */
            {
                char tx_buf[32];            /* sp 栈缓冲区 */
                uint8_t result;

                Format_String(tx_buf, "LJ%s,LW%s\r\n",
                              *(float *)RAM_ADDR_048A,
                              *(float *)RAM_ADDR_046A);   /* 0x080187C2 */
                result = Func_ProcessData(tx_buf);         /* 0x080187C8 */
                Func_4G_Send(tx_buf, result & 0xFF);       /* 0x080187D4 */
            }

            /* 4G 计数递减 (发送后必经路径) */
            RAM_ADDR_01E2 -= 1;            /* 0x080187D8 */
            goto loop_tail;                 /* 0x080187E2 */
        }

skip_4g:                                    /* 0x080187E4 */
        /* 4G 模块未启用: 仅清除标志 */
        Func_ClearFlag(0);                  /* 0x080187E4 */

loop_tail:                                  /* 0x080187EA */
        /* ======================================================== */
        /* 3.9 多阶段测量与阈值控制循环 */
        /* ======================================================== */

        /* ---- 阶段计数器 (static 保持跨迭代状态) ----
         * r4 (uint16_t phase_a_cnt): 0..99, 每 100 次触发 PhaseA 完整流程
         * r5 (uint8_t  phase_b_cnt): 0..19, 每 20 次触发 PhaseB 完整流程
         * r6 (uint16_t slow_cnt):    0..499, 每 500 次触发慢速任务
         * r7 (uint16_t speed_cnt):   根据速度测量递增, 触发 TIM ISR
         * 关键: phase_a_cnt < 100 时跳过除 Func_MainLoop_A/DisplayUpdate 外的所有处理
         */
        static uint16_t phase_a_cnt = 0;    /* r4 */
        static uint8_t  phase_b_cnt = 0;    /* r5 */
        static uint16_t slow_cnt    = 0;    /* r6 */
        static uint16_t speed_cnt   = 0;    /* r7 */

        Func_MainLoop_A();                  /* 0x080187EA: 0x08016548 */
        Func_DisplayUpdate();               /* 0x080187EE: 0x0800D834 */

        /* ---- Phase A: 每 100 次迭代触发一次完整流程 ---- */
        if (phase_a_cnt < 100)              /* 0x080187F2: cmp r4, #0x64 */
        {
            phase_a_cnt++;                  /* 0x080187F6 */
            goto speed_measurement;         /* 0x080187FA: b #0x8018a72
                                             * 跳过 phase_b/阈值/输出控制! */
        }
        phase_a_cnt = 0;                    /* 0x080187FC: 100次到, 重置 */

        /* 100 次到: 获取 ADC 平均值 */
        RAM_ADC_AVG = Func_GetADCValue();   /* 0x080187FE: 0x0800D414 → 0x20000040 */
        Func_PhaseA_Init();                 /* 0x08018806: 0x0800961C */
        Func_PhaseA_Process();              /* 0x0801880A: 0x0800963C */
        Func_PhaseA_Finish();               /* 0x0801880E: 0x080095F8 */

        /* ---- Phase B: 每 20 次触发一次数组加权计算 ---- */
        if (phase_b_cnt < 20)               /* 0x08018812: cmp r5, #0x14 */
        {
            phase_b_cnt++;                  /* 0x08018816 */
            goto check_thresholds;          /* 0x0801881A: 跳过数组处理, 但执行阈值 */
        }
        phase_b_cnt = 0;                    /* 0x0801881C */

        /* 20 次到: 使用数组索引进行加权计算 */
        if (RAM_ARR_IDX != 0)               /* 0x0801881E: 0x20000038 */
        {
            /* 通道A 计算: 校准值 + 历史值 */
            int16_t calib_a;
            uint16_t idx;

            calib_a = Func_GetCalibA();     /* 0x08018824: 0x0800C538 */
            idx = RAM_ARR_IDX - 1;          /* 0x0801882A */
            RAM_VAL_A = calib_a + RAM_ARR_A[idx]; /* 0x08018830-0x0801883A */

            /* 通道B 计算 */
            int16_t calib_b;
            calib_b = Func_GetCalibB();     /* 0x0801883C: 0x0800C710 */
            idx = RAM_ARR_IDX - 1;          /* 0x08018842 */
            RAM_VAL_B = calib_b + RAM_ARR_B[idx]; /* 0x08018848-0x08018852 */
        }
        else
        {
            /* 无历史数据: 直接读取校准值 */
            RAM_VAL_A = (int16_t)Func_GetCalibA(); /* 0x08018856-0x0801885E */
            RAM_VAL_B = (int16_t)Func_GetCalibB(); /* 0x08018860-0x08018866 */
        }

        /* 钳位: 计算值不小于 0 */
        if (RAM_VAL_A < 0)                  /* 0x08018868: ldrsh + cmp #0 */
        {
            RAM_VAL_A = 0;                  /* 0x08018872 */
        }

check_thresholds:                           /* 0x08018878 */
        /* ---- 阈值比较与输出控制 ---- */
        if (RAM_MODE_FLAG == 0)             /* 0x08018878: 0x20000019; beq → 0x08018938 */
        {
            goto clear_all_outputs;         /* 0x0801887E → 0x08018938 → 0x08018A5E */
        }

        /* ======== 输出1 控制 (电解槽开关: FSMC 0x424102A8) ======== */
        if (RAM_ADC_AVG >= RAM_LIMIT_A_MIN &&
            RAM_ADC_AVG <= RAM_LIMIT_A_MAX)   /* 0x08018880-0x08018896 */
        {
            FSMC_OUT1_REG = 1;                /* 0x08018898: 0x424102A8 */
            RAM_OUTPUT_FLAGS |= 0x01;         /* 0x080188A0: bit0=1 */
        }
        else
        {
            FSMC_OUT1_REG = 0;                /* 0x080188AE */
            RAM_OUTPUT_FLAGS &= ~0x01;        /* 0x080188B6: bit0=0 */
        }

        /* ======== 输出2 控制 (FSMC 0x424102AC) ======== */
        if (RAM_VAL_A_RAW >= RAM_LIMIT_B_MIN &&
            RAM_VAL_A_RAW <= RAM_LIMIT_B_MAX) /* 0x080188C2-0x080188D8 */
        {
            FSMC_OUT2_REG = 1;                /* 0x080188DA: 0x424102AC */
            RAM_OUTPUT_FLAGS |= 0x02;         /* 0x080188E2: bit1=1 */
        }
        else
        {
            FSMC_OUT2_REG = 0;                /* 0x080188F0 */
            RAM_OUTPUT_FLAGS &= ~0x02;        /* 0x080188F8: bit1=0 */
        }

        /* ======== 输出3 控制 ======== */
        if (!RAM_OUT3_MASK)                   /* 0x08018904: 0x200000D9 */
        {
            RAM_OUTPUT_FLAGS |= 0x04;         /* 0x0801890A: bit2=1 */
        }
        else
        {
            RAM_OUTPUT_FLAGS &= ~0x04;        /* 0x08018918: bit2=0 */
        }

        /* ======== 输出4 控制 ======== */
        if (!RAM_OUT4_MASK)                   /* 0x08018924: 0x2000007C */
        {
            RAM_OUTPUT_FLAGS |= 0x08;         /* 0x0801892A: bit3=1 */
            goto apply_outputs;               /* 0x08018936: b #0x8018a48 */
        }
        else
        {
            /* 输出4 有效: 仅清除 bit3, 不影响其他位 */
            RAM_OUTPUT_FLAGS &= ~0x08;        /* 0x08018A3C-0x08018A46: bic r0, #8 */
            /* 直接 fall-through 到 apply_outputs */
        }

apply_outputs:                                /* 0x08018A48 */
        /* ---- 根据输出标志位同步硬件输出3 ---- */
        if (RAM_OUTPUT_FLAGS != 0)            /* 0x08018A48 */
        {
            FSMC_OUT3_REG = 0;                /* 0x08018A4E: 0x424002BC 关闭 */
        }
        else
        {
            FSMC_OUT3_REG = 1;                /* 0x08018A56: 0x424002BC 开启 */
        }
        goto speed_measurement;               /* apply_outputs 结束, 跳至速度测量 */

clear_all_outputs:                            /* 0x08018A5E (经 0x08018938 b 0x08018A5E) */
        /* MODE_FLAG==0 或特殊条件: 清除所有输出 */
        RAM_OUTPUT_FLAGS = 0;                 /* 0x08018A60: 0x200000E0 = 0 */
        FSMC_OUT2_REG   = 0;                  /* 0x08018A64: *(0x424102AC) = 0 */
        FSMC_OUT2_REG   = 0;                  /* 0x08018A6A: str.w 0 to [0x42410000, #0x2AC]
                                               * 冗余写入同一寄存器 0x424102AC */
        FSMC_OUT3_REG   = 0;                  /* 0x08018A6E: *(0x424002BC) = 0 */
        /* fall-through 到 speed_measurement */

speed_measurement:                            /* 0x08018A72 */
        /* ======================================================== */
        /* 3.10 速度/频率测量与 TIM ISR 触发 */
        /* ======================================================== */
        {
            uint16_t speed_div;

            /* 800000 / 速度值 = 周期计数值 */
            speed_div = 800000 / RAM_SPEED_VAL;   /* 0x08018A72: SDIV 0x000C3500 / *(0x2000011E) */

            if (speed_div > speed_cnt)            /* 0x08018A7A: cmp r0, r7; ble */
            {
                speed_cnt++;                      /* 0x08018A80 */
            }
            else
            {
                speed_cnt = 0;                    /* 0x08018A86 */
                Func_TimerISR_Main();             /* 0x08018A88: 0x08013CE4 */
            }
        }

        /* ======================================================== */
        /* 3.11 超时/看门狗管理 */
        /* ======================================================== */
        if (RAM_ADDR_01C4 != 0)                   /* 0x08018A8C */
        {
            RAM_ADDR_01C4 -= 1;                   /* 0x08018A92: 超时递减 */
        }

        if (RAM_WATCHDOG_FLAG)                    /* 0x08018A9C: 0x200000F5 */
        {
            Func_Watchdog_Feed();                 /* 0x08018AA2: 0x0800BDCC */
        }
        else if (RAM_ADDR_01C4 == 0)              /* 0x08018AA8 */
        {
            Func_Watchdog_Check();                /* 0x08018AAE: 0x0800BED8 */
        }

        /* ======================================================== */
        /* 3.12 慢速周期任务 (每 500 次) */
        /* ======================================================== */
        Func_PeriodicTask();                      /* 0x08018AB2: 0x0800D5E4 */

        if (slow_cnt < 500)                       /* 0x08018AB4: 0x1F4 */
        {
            slow_cnt++;                           /* 0x08018ABC */
        }
        else
        {
            slow_cnt = 0;                         /* 0x08018AC2 */
            Func_500CycleTask();                  /* 0x08018AC4: 0x080115E8 */
        }

        /* ======================================================== */
        /* 3.13 周期定时器与 TIM1 控制 */
        /* ======================================================== */
        if (RAM_PERIOD_TIMER != 0)                /* 0x08018AC8: 0x20000188 */
        {
            /* 仅在特定条件下递减 */
            if (RAM_MODE_FLAG == 0 || RAM_MODE_STATE != 0) /* 0x08018ACE-0x08018AD8:
                                                  0x20000019, 0x200001BF */
            {
                RAM_PERIOD_TIMER -= 1;            /* 0x08018ADC: 周期倒计时 */

                /* 检查是否到时 (定时器 % 周期值 == 0) */
                uint16_t period;
                period = RAM_PERIOD_VAL / 10;     /* 0x08018AE4-0x08018AEA:
                                                     *(0x2000018A) / 10 */

                if ((RAM_PERIOD_TIMER % period) == 0) /* 0x08018AEE-0x08018AFA */
                {
                    /* 累加器 +10 */
                    RAM_ACCUMULATOR += 10;        /* 0x08018AFC: 0x2000000C += 10 */

                    /* 读取 TIM1 CCR1 并计算输出值 */
                    uint32_t tim1_val;
                    tim1_val = TIM1_CCR1;         /* 0x08018B06: *(0x4001002C) */
                    tim1_val = tim1_val * RAM_ACCUMULATOR; /* 0x08018B0A */

                    uint32_t output_val;
                    output_val = tim1_val / 100;  /* 0x08018B0C: UDIV by 100 */

                    /* 更新 TIM1 输出 */
                    Func_TIM1_Update(0x40010000, output_val); /* 0x08018B16: 0x080134A0 */
                }
            }
        }

        /* ======================================================== */
        /* 3.14 后循环处理 */
        /* ======================================================== */
        Func_PostLoop();                          /* 0x08018B1C: 0x08014404 */

        /* ---- TIM1 计数器递增比较 ---- */
        if (RAM_TIM1_FLAG)                        /* 0x08018B20: 0x20000198 */
        {
            if (RAM_TIM1_COUNTER <= RAM_TIM1_LIMIT) /* 0x08018B26-0x08018B30:
                                                   *(0x2000019C) <= *(0x200001A4) */
            {
                RAM_TIM1_COUNTER += 1;            /* 0x08018B32-0x08018B3C */
                Func_TIM1_Increment(RAM_TIM1_COUNTER); /* 0x08018B3E: 0x080116D0 */
            }
            else
            {
                RAM_TIM1_FLAG = 0;                /* 0x08018B44: 停止 */
            }
        }

        /* ---- 看门狗计数器 ---- */
        if (RAM_WDT_COUNTER != 0)                 /* 0x08018B4A: 0x200001C2 */
        {
            RAM_WDT_COUNTER -= 1;                 /* 0x08018B50 */
        }

        /* ======================================================== */
        /* 3.15 循环尾部: 延迟并回到顶部 */
        /* ======================================================== */
        Func_LoopTail();                          /* 0x08018B5A: 0x0800D730 */
        delay_us(1);                              /* 0x08018B5E: 1us 循环延迟 */
        /* 跳回循环顶部 0x08018642 */             /* 0x08018B64: b 0x08018642 */
    }

    return 0;  /* unreachable */
}
