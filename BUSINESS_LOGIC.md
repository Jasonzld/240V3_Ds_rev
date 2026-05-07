# HD240V3 微酸性电解水生成设备 — 业务逻辑文档

> 基于固件逆向工程完整还原 (WW240_IAP-V02.0B, STM32F410RBT6)
> 面向产品经理、业务开发者和未来功能升级
> 文档版本: v2.0 — 5 轮评审后定稿

---

## 1. 产品概述

**产品名称**: HD240V3 微酸性电解水生成设备

**核心功能**: 电解稀盐酸溶液生成次氯酸 (HClO) 消毒液，通过闭环电流控制保证消毒液浓度一致。

**典型应用场景**: 餐饮消毒、医疗消毒、公共场所卫生消杀。

**一句话原理**: 检测电解电流 → 蠕动泵定量补盐酸 → 维持恒流电解 → 生成次氯酸消毒液。

---

## 2. 硬件架构

### 2.1 核心组件

| 组件 | 型号/规格 | 功能 |
|------|----------|------|
| 主控 MCU | STM32F410RBT6 (Cortex-M4, 100MHz, LQFP-64) | 系统核心控制 |
| 显示屏 | 大彩 7寸串口屏 1024×600 | 人机交互界面，RS-232 通信 (SP3232) |
| 4G 模块 | 合宙 Air780 (选配, 含 GNSS) | 云端遥测 (MQTT) + GPS 定位，UART 通信 |
| Flash 存储 | W25Q128 (128Mb SPI NOR) | 配置参数持久化 (4 个配置块) |
| 电流传感器 | CC6903 霍尔传感器 (10A 量程) | 电解电流闭环检测 |
| 蠕动泵 | DRV8870 H桥驱动 | 定量注入盐酸 |
| 水阀 | AQH3223A SSR 控制 220V AC | 进水控制 (光电隔离) |
| 流量计 | 霍尔流量计 (EXT_IN_COUNTER) | 进水流量检测 |
| 红外感应 | 红外开关 (CN2.10 → W_SENSOR, 高电平触发) | 非接触启停 |
| 电压测量 | LM358 双运放 (U31) 信号调理 | 电解电压 ADC 采样 |
| 蜂鸣器 | TMB12A03 | 运行状态声音提示 |
| 风扇 | 12V 风扇 (FAN12V → CN3.1) | 设备散热 |
| USB 编程 | CH340N (U16) USB-UART 桥接 | 固件编程/调试 |
| I2C 总线 | I2CSCL/I2CSDA + 4.7K 上拉 | 传感器扩展接口 (CN1/CN4/P1) |
| SWD 接口 | SWD1 (4P Header) | 调试接口 (SWDIO + SWCLK) |

### 2.2 电源架构

```
24V DC (CN3 两 Pin 主供电)
  ├── F1(贴片保险) → VIN
  │     └── EG1192 DCDC + L2(47uH) → 12V (系统主功率轨)
  │           ├── BL8032(U5) + L3(10uH) → +5V
  │           │     ├── AMS1117-3.3 LDO → +3.3V (MCU/Flash/U4/逻辑)
  │           │     └── S+5V (传感器上拉)
  │           ├── BL8032(U6) + L4(10uH) → EXT+5V
  │           │     └── MT9700 限流开关 → 外部接口保护
  │           └── 风扇(Q24 NCE4953) / 继电器 → 直接 12V 负载
  │
  └── F2(限流保险) + D36(TVS 70V) → VBUS+
        └── U9(CC6903 霍尔传感器) → V_POWER
              └── Q27(NCEP40T20ALL) → VQ → DJC1 → 电解槽 (24V)

220V AC (CN3 独立 Pin)
  └── F3(保险座) → U27(AQH3223A SSR) → 220V-L_OUT → 水阀
```

**电源轨一览**:

| 网络 | 电压 | 来源 IC | 主要负载 |
|------|------|---------|---------|
| VIN | 24V DC | F1 后 | EG1192 输入 |
| 12V | 12V DC | EG1192 (U12) | 风扇/继电器/BL8032 输入 |
| +5V | 5V DC | BL8032 (U5) | USB/通信/AMS1117 输入 |
| +3.3V | 3.3V DC | AMS1117-3.3 (U10) | MCU/Flash/SP3232/逻辑 IC |
| S+5V | 5V DC | BL8032 (U6) | 传感器上拉、系统 5V |
| EXT+5V | 5V DC | U17 (MT9700) | 外部接口 5V (限流保护) |
| V_POWER | 24V DC | U9 后 / R1(NTC) | Q27 功率开关输入 |
| VQ | 24V DC | Q27 开关输出 | 电解槽 (经 DJC1) |

### 2.3 电解槽控制链路

```
MCU GPIO CORE_ONOFF (U8.41)
  → R27(10Ω) → Q1(AO3400A) 栅极
    → Q25(HSS1N20 200V) 电平转换
      → Q27(NCEP40T20ALL) 功率 NMOS 栅极
        → 电解槽 (DJC1 接口, 24V DC, 工作电流 <2A, 最大 <5A)

软启动: 1kHz PWM, 占空比 0→100%, 持续 1 秒后拉高保持。
低电平关闭 Q27, 电解槽断电。
```

### 2.4 安全保护硬件

| 保护项 | 实现方式 |
|--------|---------|
| 主供电过流 | F1 (JFC2410-1300TS, 贴片保险) |
| 功率级限流 | F2 (A72-300, 限流保险) |
| 220V AC 过流 | F3 (5×20 保险座, 水阀回路) |
| 12V 自恢复 | F4 (nSMD075-16V, 自恢复保险) |
| 信号级保护 | F5/F6/F7 (JK-SMD0805-100, 自恢复保险) |
| 过压保护 | D36 (SMCJ70A TVS, 70V) + D17 (SMCJ70A) |
| 220V 隔离 | U27 SSR 光电隔离 (AQH3223A) |
| 电源反接 | 二极管保护 |
| NTC 限流 | R1 (STE07D101K, 串于 V_POWER 路径) |
| ESD 防护 | D14/D15/D22/D24/D34/D38 (TPD4E05U06DAQR 4通道, 所有外部通信口) |
| USB ESD | D27 (SR05-N ESD 阵列) |
| TFT 接口 ESD | TV1 (PSOT15C-LF-T7) + L1 (共模滤波器) |
| 外部输入整形 | U32/U34-U37 (SN74LVC1G14 施密特触发反相器, 5 路) |

---

## 3. 完整工作流程

### 3.1 开机初始化

```
上电
 ├── 延迟 100ms 等待电源稳定
 ├── 系统时钟配置 (RCC AHB1: GPIOA/B/C; APB1: USART2/TIM4/I2C1; APB2: USART1/6/ADC1/SPI1)
 ├── Flash 备份域检查 (0x08004000) → 首次上电写配置标志 → 系统复位 (SCB AIRCR)
 ├── GPIO 初始化
 ├── TFT 液晶屏初始化 (FSMC 总线控制寄存器, 3 步上电时序, 每步间隔 1ms)
 ├── 外设初始化:
 │   ├── USART1 (TFT 串口屏通信, RS-232 经 SP3232)
 │   ├── USART6 (4G/GPS 模块通信)
 │   ├── ADC + DMA (电流通道 CH1: U8.14, 电压通道 CH2: U8.15)
 │   ├── 定时器 (相控 TIM1/蠕动泵 TIM/PWM TIM/软启动 TIM, TIM1 基址 0x40010000)
 │   ├── GPIO 中断初始化
 │   └── GPIOB 中断处理器初始化
 ├── 从 SPI Flash 加载 4 个配置块 (CRC 校验)
 ├── RTC 初始化并读取当前时间
 └── 进入主事件循环
```

### 3.2 主事件循环 (超循环结构, func_01_main @ 0x08018524)

每轮循环基于四个嵌套阶段计数器推进:

```
循环入口
 ├── 读取 ADC 电流采样值 (通道1, RAM 0x20000908)
 ├── 处理 TFT 串口屏通信 (USART1)
 ├── 状态机判断:
 │   ├── 待机状态 (FLAG_0174==0 && FLAG_0175==0) → 检测启动条件 → 置 FLAG_0175=5 → 启动蠕动泵
 │   ├── 运行状态 (FLAG_0177!=0) → 首次进入设运行时长 3600s → 停止蠕动泵 (电解已达目标电流)
 │   └── 其他状态 → 蜂鸣器提示
 ├── 水阀控制 (首次循环初始化, FLAG_017B 门控)
 ├── 运行计时管理 (加载目标运行时间, 调试打印 "st?")
 ├── 倒计时与启停模式管理 (锁存模式 2s/点动模式 20s 周期)
 ├── TFT 显示更新 (FLAG_01E0 门控)
 ├── 4G 模块数据处理 (FLAG_01E2 门控):
 │   ├── CRC 校验配置块
 │   ├── FPU 换算目标电流值 (int→double→÷1000→×100)
 │   ├── FPU 换算当前电流值
 │   ├── 格式化字符串 "LJ%s,LW%s\r\n"
 │   └── 4G 发送 (FLAG_01BE 门控)
 ├── Phase A 计数器 (每 100 次迭代触发 ADC 平均 + PhaseA 流程)
 ├── Phase B 计数器 (每 20 次迭代触发通道 A/B 加权计算)
 ├── 电流/电压阈值比较 (上限/下限, 4 路输出控制)
 ├── GPIO 输出刷新 (FSMC 映射输出: 电解槽/水阀/输出3/输出4)
 ├── 速度测量 (800000÷SPEED 周期, 触发 TIM ISR)
 ├── 看门狗管理 (FLAG_00F5 门控: Feed or Check)
 ├── 慢速周期任务 (每 500 次迭代触发 func_500CycleTask)
 ├── 周期定时器与 TIM1 控制 (累加器 + CCR1 计算)
 ├── TIM1 计数器递增比较
 ├── 看门狗计数器递减
 └── 循环尾部 (func_LoopTail + 1us 延迟) → 下一周期
```

---

## 4. 启停控制模式

### 4.1 触发方式 (3 种)

| 方式 | 信号来源 | MCU 引脚 | 说明 |
|------|---------|---------|------|
| 屏幕按键 | TFT 串口屏触摸 (USART1) | U8.42/43 | 用户点击屏幕按钮 |
| 红外感应 | 红外开关 (CN2.10) → U32 施密特整形 → W_SENSOR | U8.53 | 挥手/遮挡触发, 高电平有效 |
| 外接开关 | 外部开关量输入 (EXT_IN1/2/3/5, 经施密特触发) | U8.57/56/55/8 | 远程/联动控制 |

### 4.2 两种工作模式

**锁存模式 (Latch Mode)** (RAM 0x200001A8 == 0):
- 触发一次 → 启动电解 → 运行至预设时间或人工停止
- 倒计时周期: 2 次主循环迭代/周期 (RAM 0x200001AC = 2)
- 完成阈值: RAM 0x200001BC > 1 → 运行完成 (1 个周期后触发)
- 默认模式, 适用于定量消毒场景

**点动模式 (Momentary Mode)** (RAM 0x200001A8 != 0):
- 触发期间持续电解 → 释放即停止
- 倒计时周期: 20 次主循环迭代/周期 (RAM 0x200001AC = 20)
- 完成阈值: RAM 0x200001BC > 15 → 运行完成 (15 个周期后触发)
- 适用于即时消毒场景

注: 上述计数值基于主循环迭代次数而非绝对时间, 每次循环约 1μs 延迟。

### 4.3 启动序列

```
检测触发信号 (屏幕/红外/外接)
  ├── 模式判断 (锁存 vs 点动)
  ├── 开进水阀 (220V AC, U27 AQH3223A SSR 控制)
  ├── 霍尔流量计开始计数 (EXT_IN_COUNTER, U8.36)
  ├── 电解槽软启动 (CORE_ONOFF: 1kHz PWM 0→100% 占空比, 1s)
  ├── 电流达到目标范围 → 蠕动泵调节 (HCL-PUMP, U8.35 → DRV8870)
  └── 进入稳定运行状态
```

---

## 5. 电解电流闭环控制

### 5.1 控制原理

```
目标电流 (设定值, RAM 0x200001C8, float)
    │
    ├── CC6903 霍尔传感器 (U9, 串联在 VBUS+ → V_POWER 路径)
    │     → VOUT → R23/C53 滤波 → CURRENTSENSOR (U8.14 ADC)
    │       → ADC 采样 (DMA → RAM 0x20000908) → Func_GetADCValue (0x0800D414) 平均 → 实际电流值
    │                                                          │
    └──────────── 误差比较 ←────────────────────────────────────┘
                     │
             蠕动泵补酸量调节 (DRV8870 H桥, U8.35)
                     │
             电流变化 → 重新采样
```

### 5.2 蠕动泵控制

- **驱动方式**: DRV8870 H桥 (U1), 定量注入模式, 12V 供电
- **控制信号**: MCU GPIO HCL-PUMP (U8.35) → DRV8870 IN1
- **补酸策略**: 电流低于目标-回差 (如 <1.75A) → 增加补酸频率；电流高于目标+回差 (如 >1.85A) → 停止蠕动泵
- **泵周期**: 由 TIM 定时器控制, 默认周期可配置 (TIM_Init_PumpCtrl: PSC=0x4E20, ARR=0x270F)

### 5.3 安全限值

| 参数 | 限值 | 说明 |
|------|------|------|
| 电解槽工作电流 | <2A (正常), <5A (最大) | F2 硬件限流 + 软件阈值 |
| 软启动时间 | 1 秒 | 1kHz PWM 0→100% 占空比 |
| 电流下限阈值 | 可配置 | 低于阈值 → 增加蠕动泵补酸 |
| 电流上限阈值 | 可配置 | 高于阈值 → 停止蠕动泵, 降低 PWM |
| 一般告警上限 | 可配置 (如 >3A) | 告警但不停机 |
| 危险电流上限 | 可配置 (如 >5A) | 紧急停机 → ERR_CUR 输出 |

### 5.4 多阶段平均与阈值控制 (主循环 Phase A/B)

```
Phase A (每 100 次循环迭代):
  ADC 平均值 (func_GetADCValue) → 通道 A/B 校准 (func_GetCalibA/B)
    → 通道 A 下限/上限比较 (RAM_LIMIT_A_MIN/MAX)
    → 通道 A 在阈值内 → FSMC_OUT1_REG=1 (电解槽开关)
    → 通道 A 超阈值 → FSMC_OUT1_REG=0

Phase B (每 20 次循环迭代):
  历史值数组加权计算 (ARR_A[ARR_IDX-1] + calib_a → VAL_A)
    → 通道 A 原始值 (VAL_A_RAW) 与 LIMIT_B 阈值比较
    → 在阈值内 → FSMC_OUT2_REG=1 (水阀/继电器)
    → 超阈值 → FSMC_OUT2_REG=0

注: 输出 2 使用通道 A 的原始值 (VAL_A_RAW) 与通道 B 的阈值 (LIMIT_B) 进行交叉比较,
这是一种交叉门控设计, 而非独立的双通道传感器。

注: 第 2.3 节描述的 CORE_ONOFF (U8.41 GPIO) 是电解槽的硬件栅极驱动路径 (→ Q1 → Q25 → Q27),
而 FSMC_OUT1_REG (0x424102A8) 是 FSMC 总线映射的软件控制寄存器。
两者均为电解槽控制信号, FSMC_OUT1 为软件状态写入点, CORE_ONOFF 为其对应的物理 GPIO 输出。
```

---

## 6. 显示屏与用户界面

### 6.1 显示架构

- **屏幕**: 大彩 7 寸串口屏 (1024×600)
- **接口**: RS-232 (SP3232EEY 收发器, U4) 经 TFT 连接器 (A2008WR-S-8P)
- **并行总线**: FSMC (Flexible Static Memory Controller) 用于 TFT LCD 控制寄存器访问
- **通信协议**: BC2C 自定义帧协议 (cmd + sub_cmd + payload)
- **刷新方式**: 主循环周期性更新 (func_DisplayUpdate + func_MainLoop_A) + 事件驱动刷新

注: RS-232 是串口屏的命令/数据通信通道, FSMC 是 TFT 面板的并行总线控制接口, 两者各司其职。

### 6.2 显示页面 (从代码模式分派还原)

func_48_display_page_renderer 通过 25+ 种模式 (mode 0x00-0x25) 实现多页面分派渲染。以下为模式分发中识别的核心页面:

| 页面 (模式范围) | 内容 | 更新机制 |
|------|------|---------|
| 主页/状态页 (mode 0x00-0x0F) | 设备状态、电解电流值、电压值、运行计时、水阀/泵状态 | 实时更新 (func_DisplayUpdate 每循环) |
| 参数设置页 (mode 0x10-0x16) | 目标电流、运行时间、模式选择、周期设置 | 用户操作触发 |
| 信息页 (mode 0x1B) | 版本号、设备ID、4G 信号强度 | 开机/按需 |
| 运行记录页 (mode 0x1C-0x20) | 运行时长、累计制水量、故障记录 | 按需查询 |
| 高级配置页 (mode 0x21-0x25) | 校准值、阈值配置、系统参数 | 工厂/维护 |

注: 以上页面分类基于代码中 mode 分发逻辑和参数寄存器关联推断，具体 UI 布局由 TFT 串口屏端程序决定。

### 6.3 显示命令类型

通过 func_48_display_page_renderer 实现多页面渲染, 支持:
- 模式分派 (mode 0-0x25): 25+ 种页面/控件更新模式
- FPU 浮点格式化显示 (电流值、电压值, 经 func_16BA0 + func_883C 管道)
- 字符串格式化与拼接
- BC2C 帧编码发送 (CMD + DATA)

---

## 7. 4G 云端连接 (选配)

### 7.1 模块信息

- **型号**: 合宙 Air780 (选配)
- **通信**: UART (USART6, 经 GH2 连接器: WIFI_TX/WIFI_RX)
- **GPS**: Air780 内置 GNSS, 独立 UART (GH3: GPS_TX/GPS_RX)
- **协议**: AT 指令集 + MQTT
- **MQTT 服务器**: 49.235.68.247:1883
- **MQTT 主题**: "W/V2 Plus/<device_id>"
- **控制 GPIO**:
  - MOD_ENABLE (U8.39 → GH1.1): 模块使能
  - IO_RST (U8.20 → GH1.2): 模块复位
  - WIFI-ENABLE (U8.33 → GH3.4): 模块使能 (第二路)
  - RST/WKUP (U8.11 → GH2.4): 模块唤醒
  - LINK_A/LINK_B (U8.9/U8.10 → GH1.3/1.4): 模块状态脚, **未使用**
- **GPS 供电**: Q21 (CJ3401 P-MOS) 硬件自动关断, MCU 不直接控制

### 7.2 遥测数据项 (两条上报路径)

**路径 A — 文本遥测 (MQTT 字符串上报, func_149 @ 0x08009C84)**:

通过 AT+MPUB MQTT Publish 命令发送格式化的传感器数据字符串。
注: 下表中各数据项的业务含义基于代码上下文和传感器信号路径推断, 固件代码本身未包含显式的数据标签字符串。
FPU 管道处理 (简化视图, 实际含更多中间步骤): int→double (func_875C/873A) → 缩放 (func_865C: ×10.0/×1000.0/÷3600.0) → 转换 (func_883C: double→uint32) → 编码 (func_87B4: uint32→double 重新解释) → 格式化 (func_16B18, sprintf-like) → 字符串拼接 (func_08334, strcat-like) → MQTT Publish (func_09B6C)。

| 数据项 | 来源 (RAM 地址) | 转换管道 |
|--------|------|------|
| 设备状态标志 | 0x20000019 | 直接格式化 |
| 运行阶段 | 0x200000E7 | 直接格式化 |
| 电解电流 (通道 A) | 0x20000040 | int→double→scale→format |
| 电解电压 (通道 B) | 0x2000003E | signed→double→scale→format |
| 目标电流 | 0x2000003C | signed→double→scale→format |
| 实际电流 | 0x2000003A | signed→double→scale→format |
| 运行时长 (累计) | 0x2000008C (32bit) | int→double→÷3600.0→format |
| GPS 经度/速率 | 0x20000074 (32bit, 时分复用) | int→double→scale→format |
| GPS 纬度 | 0x20000128 (32bit) | int→double→scale→format |
| GPS 航向 | 0x20000056 | signed→double→scale→format |
| GPS 海拔 | 0x20000058 | signed→double→scale→format |
| 信号强度 (CSQ) | 0x200000A8 | signed→double→scale→format |
| 流量计数 | 0x20000070 | 直接格式化 |
| 设备状态码 | 0x20000038 | 直接格式化 |

**路径 B — 二进制遥测包 (95 字节, func_125 @ 0x08016664)**:

构建 95 字节固定格式二进制遥测帧, 通过 func_16664 发送。
帧格式: [0x55 0xAA 同步头 (被计数器覆盖)] [93 字节传感器数据 BE] [0xAA 0x55 尾标记]
包含与路径 A 相同的传感器数据, 以及 2 个 16 字节配置块镜像 (0x200003EC, 0x200003FC)。

**两条路径的区别**: 路径 A 输出人类可读的格式化文本 (MQTT Topic), 路径 B 输出紧凑二进制帧 (可能用于自定义协议或效率敏感场景)。两者共享同一套传感器 RAM 地址。

注: 部分 RAM 地址存在时分复用 (如 0x20000074 同时承载 GPS 经度遥测数据和 func_82 速度计算 UDIV 输出), 遥测数据仅在 Func_4G_Send 标志置位周期内有效。

### 7.3 4G 模块 AT 命令流程

```
开机初始化:
  AT_Admin_Auth_Sender:   "<device_id>,admin,d9ATO#cV\r\n"
  AT_MQTT_Connect_Sender: "AT+MCONNECT=1,60\r\n"

遥测上报 (周期触发):
  传感器数据读取 → FPU 双精度转换管道 → 格式化字符串 → AT+MPUB Publish

GPS 数据:
  AT+CGNSS 系列指令 → NMEA $xxRMC 解析 (func_167 @ 0x08017EEA)
    → 9 个字段提取 (时间/有效标志/纬度/经度/速率/日期/模式)
```

### 7.4 GPS NMEA 解析 (func_167 @ 0x08017EEA)

解析 RMC 语句的 9 个字段:
- 字段1: UTC 时间 (hhmmss.ss)
- 字段2: 数据有效标志 (A=有效/V=无效)
- 字段3: 纬度 (ddmm.mmmm)
- 字段4: 北纬/南纬 (N/S)
- 字段5: 经度 (dddmm.mmmm)
- 字段6: 东经/西经 (E/W)
- 字段7: 地面速率 (节)
- 字段9: UTC 日期 (ddmmyy)
- 字段12: 模式指示 (N/A/D/E)

---

## 8. 配置参数管理

### 8.1 存储架构

- **存储介质**: W25Q128 SPI NOR Flash (128Mb, U13, SOIC-8)
- **SPI 连接**: PA5(MOSI), PA6(MISO), PA7(CLK), PB10(CS)
- **配置块**: 4 个独立配置块 (block0-3), 从 Flash 四个区域加载
- **RAM 映射**: CONFIG_BASE (0x200003EC), 每个块 16 字节
  - block0: 0x200003EC
  - block1: 0x200003FC
  - block2: 0x2000040C
  - block3: 0x2000041C
- **CRC 校验**: 每个配置块加载时验证 CRC, 失败则使用默认值并标记异常

### 8.2 可配置参数 (从固件代码提取)

| 参数 | 固件变量地址 | 类型 | 说明 |
|------|-------------|------|------|
| 目标电流 | 0x200001C8 (float) | 运行参数 | 电解电流设定值, 经 int→double→÷1000→×100 换算 |
| 运行时间 | 0x200001B4 (uint32) | 运行参数 | 锁存模式运行时长 (秒) |
| 软启动时间 | TIM_Init_SoftStart | 硬件定时器 | PWM 斜坡时长 (PSC=0xFFFF, ARR=0xC34F, 默认 1s) |
| 蠕动泵周期 | TIM_Init_PumpCtrl | 硬件定时器 | 补酸定时器参数 (PSC=0x4E20, ARR=0x270F) |
| 通道A 电流上限 | 0x20000050 (uint16) | 阈值配置 | 电解槽输出关断阈值 |
| 通道A 电流下限 | 0x2000004E (uint16) | 阈值配置 | 电解启动最低电流 |
| 通道B 上限 | 0x2000004C (uint16) | 阈值配置 | 水阀/继电器控制上限 |
| 通道B 下限 | 0x2000004A (uint16) | 阈值配置 | 水阀/继电器控制下限 |
| 相控定时器 | TIM1 (0x40010000) | 硬件定时器 | 相控 PWM 参数 (PSC=0x2710, ARR=0x2711) |

注: 以上参数从固件代码的寄存器和常量池中提取。具体数值范围由固件中钳位代码决定 (如 func_51 中 `if (val > 0x157C) val = 0x157C` 模式)。部分参数的实际可调范围和默认值取决于 TFT 串口屏端的 UI 逻辑。

### 8.3 配置加载流程

```
SPI Flash 读取 (Flash_ReadConfig @ 0x0800D458)
 ├── 地址: 4 个 Flash 配置区域
 ├── 验证 CRC
 ├── CRC 失败 → 使用默认值 → 标记配置异常 (跳过 4G 数据处理)
 └── CRC 通过 → 加载到 RAM 运行变量 (CONFIG_BASE 结构体)
```

---

## 9. 安全与保护机制

### 9.1 硬件保护 (详见 2.4 节)

| 保护项 | 实现方式 | 触发动作 |
|--------|---------|---------|
| 过流保护 (主供电) | F1 贴片保险 | 熔断 |
| 过流保护 (功率级) | F2 限流保险 + R1 NTC | 限流/熔断 |
| 过流保护 (220V) | F3 保险座 | 熔断 |
| 过压保护 | D36/D17 (TVS 70V) | 钳位 |
| 220V 隔离 | U27 SSR 光电隔离 | 持续隔离 |
| 电源反接 | 二极管保护 | 阻断 |
| ESD 防护 | 6× TPD4E05U06DAQR + SR05-N + TVS | 泄放 |

### 9.2 软件保护

| 保护项 | 检测方式 | 处理动作 |
|--------|---------|---------|
| 电解槽过流 | ADC 电流 (U8.14) > 上限阈值 | 停止蠕动泵, 降低 PWM → ERR_CUR 输出 (U8.51 → Q4 NUD3105) |
| 电解槽欠流 | ADC 电流 < 下限阈值 | 增加蠕动泵补酸频率 |
| 运行超时 | 运行计时 > 设定值 | 停止电解, 关闭水阀 |
| 水阀/流量异常 | 流量计无脉冲 (EXT_IN_COUNTER) | 报警 → ERR_FLOW 输出 (U8.52 → Q3 NUD3105) |
| 系统死锁 | 独立看门狗 (IWDG) | 自动复位 |
| Flash 配置 CRC 错误 | CRC 校验失败 | 使用默认配置, 标记异常 |

### 9.3 继电器型输出

三个由 MCU GPIO 驱动的 NUD3105 N-MOSFET 开路漏极输出, 用于外部报警/状态指示:

| 信号 | MCU 引脚 | 驱动管 | 输出连接器 | 功能 |
|------|---------|--------|-----------|------|
| ERR_CUR | U8.51 | Q4 (NUD3105) | ERR_CUR1 (XH-2A) | 电流异常输出 (高=故障) |
| ERR_FLOW | U8.52 | Q3 (NUD3105) | ERR-FLOW1 (XH-2A) | 流量异常输出 (高=故障) |
| RUNNINGOK_STATUS | U8.50 | Q5 (NUD3105) | RUNNINGOK_ON1 (XH-2A) | 正常运行状态输出 (高=运行中) |

### 9.4 异常状态码

系统通过全局状态字节 (STATE, 0x2000024A) 表示运行阶段:
- 合法运行值: 0x00-0x16, 0x1B-0x21, 0x24-0xFF
- 跳过值 (异常/中间态): 0x17, 0x18, 0x19, 0x1A, 0x22, 0x23
- 当 STATE 处于跳过值时, 状态机监控器 (StateMachine_Monitor) 直接返回不执行任何动作

---

## 10. 状态机逻辑

### 10.1 两级状态机架构

**第一级: 设备状态检查 (func_133EC @ 0x080133EC)**
- 检查外设寄存器状态
- DEV_ID 值因调用者而异 (func_82 使用 0x40014000, func_87 使用 0x40001000)
- 不通过 → 设备重启动 (func_133B0)

**第二级: 运行状态 (STATE, 0x2000024A) + 模式字节 (MODE_BYTE, 0x2000023D)**
- 由 StateMachine_Monitor (func_33 @ 0x0800D5E4) 监控
- 由 Periodic_Event_Scheduler (func_82 @ 0x08012990) 进行六阶段周期调度

### 10.2 双路交叉监控 (func_33)

StateMachine_Monitor 实现双路计数器交叉监控:

```
门控条件:
  1. FLAG_ED (0x200000ED) != 0 — 监控使能
  2. STATE (0x2000024A) 不在跳过区间 — 设备状态合法

路A (COUNTER_SELECT == 0):
  COUNTER_B = 0 (复位对方)
  COUNTER_A++ (己方递增)
  达阈值300 → FLAG_1E=1 → 等待路B

路B (COUNTER_SELECT != 0):
  COUNTER_A = 0 (复位对方)
  COUNTER_B++ (己方递增)
  达阈值300 → FLAG_1F=1 → 等待路A

两路均达阈值 → 触发动作链:
  func_12604(2) + func_0E2D8(2) + 标志更新 + func_10BD0
  路B 特有: func_12454(0) + FLAG_19 翻转

尾部: FLAG_EF && FLAG_19 → VAL_F2 = VAL_F0 (值镜像)
```

### 10.3 周期性事件调度 (func_82, 六阶段)

六阶段周期性调度器:

- **阶段A**: 模式字节 (0x2000023D) 比较 → 分支选择 (FLAG_BRANCH_SEL 门控)
- **阶段B**: 多路状态链 (5 个分发块, 含 CHAIN_VAL_A~E 比较对)
- **阶段C**: 补充比较分发 (cmp + bne 链)
- **阶段D**: 多路定时器递减 (6 个定时器: TIMER0-5)
- **阶段E**: 速率计算 (UDIV ÷3600 + ÷1000 + MULS×100/SDIV÷60)
- **阶段F**: 尾部条件递减 (4 组条件递减 + 值镜像)

---

## 11. 通信协议

### 11.1 TFT 串口屏协议 (USART1, RS-232 经 SP3232)

**BC2C 帧格式**:
```
[CMD byte] [HI byte] [LO byte]  — 3 字节帧
或
[CMD byte] [DATA byte0] [DATA byte1] ... [DATA byteN]  — 可变长帧
```

**常用命令码** (从 func_51 Multi_Param_Handler 提取):
| CMD | 关联寄存器 | 含义 |
|-----|-----------|------|
| 0x02 | REG_4A (0x2000004A) | 参数写入 (通道B 下限) |
| 0x04 | REG_4C (0x2000004C) | 参数写入 (通道B 上限) |
| 0x08-0x0E | REG_46/4E/50/52 | 运行参数 (通道A 限值/钳位) |
| 0x10-0x14 | REG_56/58/5A | 钳位参数 |
| 0x34-0x38 | REG_A8 系列 (0x200000A8) | FPU 转换值 |
| 0x52 | 浮点计算值 | FPU 输出 |
| 0x5C-0x62 | 双路钳位值 | 通道 A/B 钳位比较 |
| 0x7E | REG_15C (0x2000015C) | 参数值 (func_8410 转换) |
| 0x80 | 多模式参数 | 模式分派参数 |
| 0x82-0x88 | 0x157C 钳位上限 + 缩放值 | 上限钳位 |
| 0xA0-0xC0 | 字符串缓冲区 | 文本显示内容 |

### 11.2 4G 模块协议 (USART6, UART)

**AT 指令 + MQTT**:
- 初始化: AT 指令序列 (AT+CGATT, AT+MQTTCONN 等)
- 管理员认证: `<device_id>,admin,d9ATO#cV\r\n`
- MQTT 连接: `AT+MCONNECT=1,60\r\n` (keep-alive 60s)
- 数据上报: AT+MPUB (MQTT Publish, func_09B6C)
- GPS 数据: AT+CGNSS 系列指令 → NMEA $xxRMC 解析
- 心跳维持: 周期性 AT 指令
- UART 发送: func_0BBFC (逐字节发送, 通过 func_15F38)

### 11.3 数据传输帧

FPU 数据传输帧 (func_75 @ 0x080126F8), 用于发送含 IEEE 754 单精度浮点数据的 17 字节命令帧:
```
[0xEE] [0xB1] [0x07] [HI(p1)] [LO(p1)] [HI(p2)] [LO(p2)] [0x02]
[flag_byte] [float_b3] [float_b2] [float_b1] [float_b0]
[0xFF] [0xFC] [0xFF] [0xFF]

其中 flag_byte = (p3 & 0x0F) | (p4 == 0 ? 0 : 0x80)
float 值为大端序 (MSB 先)
```

---

## 12. 关键数据流

### 12.1 电流闭环控制数据流

```
CC6903 霍尔传感器 (U9, 模拟量, SOP-8)
  → VOUT → R23/C53 滤波 → MCU ADC1 通道 1 (U8.14 CURRENTSENSOR)
    → DMA 传输到 RAM 缓冲区 (0x20000908)
      → Func_GetADCValue (0x0800D414, ADC 值缩放/平均)
        → Phase A 平均计算 (每 100 次循环)
          → 与阈值比较 (RAM_LIMIT_A_MIN/MAX, RAM_LIMIT_B_MIN/MAX)
            → 误差值 → 蠕动泵控制量 (HCL-PUMP, U8.35)
              → TIM 定时器 → DRV8870 H桥 (U1) → 蠕动泵
```

### 12.2 文本遥测数据流 (MQTT 路径)

```
传感器 RAM 值 (0x20000040/3E/3C/3A 等)
  → func_875C (uint32→double) / func_873A (signed→double)
    → func_865C (缩放: ×10.0, ×1000.0, ÷3600.0 等)
      → func_883C (double→uint32 转换)
        → func_87B4 (编码)
          → func_16B18 (格式化字符串, sprintf-like)
            → func_08334 (字符串拼接, strcat-like)
              → func_09B6C (AT+MPUB MQTT Publish)
```

### 12.3 二进制遥测数据流 (95 字节包)

```
传感器 RAM 值 + 配置块镜像
  → func_125_telemetry_packet_builder (逐字节提取: UBFX/LSRS/ASRS)
    → 95 字节帧:
        [0-1]: 同步/计数器覆盖
        [2-5]: RTC 时间戳 (4 字节 BE)
        [6-12]: 传感器组 1 (7 字节)
        [13-20]: 传感器组 2 (8 字节)
        [21]: 标志字节
        [22-52]: 传感器组 3 (31 字节)
        [53-68]: 配置块 1 镜像 (16 字节)
        [69-84]: 配置块 2 镜像 (16 字节)
        [85-92]: 扩展传感器 (8 字节)
        [93-94]: 尾标记 0xAA 0x55
      → func_16664 发送
```

### 12.4 显示更新数据流

```
运行参数 (RAM 变量)
  → Multi_Param_Handler (func_51, 25+ 模式参数处理, @ 0x0800FB48)
    → BC2C 帧编码 (CMD + HI + LO)
      → USART1 发送 (func_123E8 字节发送)
        → SP3232 RS-232 电平转换 → TFT 串口屏解析 → 页面刷新
```

---

## 13. 硬件扩展预留

### 13.1 当前未使用但可扩展的信号

| 信号 | MCU 引脚 | 说明 | 扩展潜力 |
|------|---------|------|---------|
| I2CSCL/I2CSDA | U8.58/59 | I2C 总线, 已拉出到 CN1/CN4/P1 连接器 | 温度/湿度/pH 传感器 |
| CORE_PWM_HIN | U8.24 | 外部 PWM 输出口 | 额外功率控制 |
| CORE_PWM_LIN | U8.25 | 外部 PWM 输出口 | 额外功率控制 |
| XSHT | U8.61 | 传感器关断 (扩展引线) | 传感器电源控制 |
| IR | U8.62 | CN1.5 引出 | 第二路红外 |
| EXT-IO1/2/3 | U8.26/27/28 | 外部 IO, 经二极管输出 | 自定义数字 IO |
| EXT_IN1/2/3 | U8.57/56/55 | 施密特触发输入, 带 LED 指示 | 额外开关量输入 |
| EXT_IN5 | U8.8 | 施密特触发输入 (EXT_SW) | 额外开关量输入 |
| WATER_PUMP/SWITCH | U8.40 | Q9(AO3400) 驱动 | 辅助水泵/开关 |
| ADC 通道 | U8.14/15 | 已用 2 路, 预留更多 | 额外模拟传感器 |
| LINK_A/LINK_B | U8.9/10 | 4G 模块状态脚 (未用) | 模块状态监控 |
| CH443K UART 切换 | U14/U15 | USB/DEBUG UART 二选一 | 多串口设备接入 |

### 13.2 功能升级建议

1. **传感器扩展**: I2C 总线 (U8.58/59) 已引出到 CN1/CN4/P1 连接器, 带 4.7K 上拉和 ESD 保护, 可直接接入温度/pH/液位/浓度传感器
2. **WiFi 联网**: 替换/并联 4G 模块为 WiFi 模块 (ESP32 等通过 GH1/GH2/GH3 接口)
3. **蓝牙近场控制**: 增加 BLE 模块实现手机 APP 直连
4. **多设备集群**: 基于 MQTT (已有) 实现多设备协同管理
5. **数据本地存储**: W25Q128 剩余空间 (>100Mb) 可用于存储运行日志
6. **OTA 固件升级**: 基于 4G/WiFi 的远程固件更新 (CH340N USB 编程通道已在板)
7. **语音播报**: 增加语音模块报读设备状态
8. **辅助水泵**: WATER_PUMP/SWITCH (U8.40, Q9 AO3400) 信号已预留, 可用于排水泵等

### 13.3 协议扩展点

- **BC2C 命令码空间**: 0x00-0xFF 仍有大量未使用码位
- **MQTT Topic 扩展**: 当前仅遥测上报 (AT+MPUB), 可扩展下行控制指令 (AT+MSUB)
- **TFT 页面**: func_48 支持 25+ 种 mode, 可增加新页面
- **配置块**: 4 块 × 16 字节, 可重新规划以容纳新参数
- **二进制遥测帧**: 95 字节帧有固定结构, 可扩展长度或新增帧类型

### 13.4 代码架构特点

- **模式分发架构**: Multi_Param_Handler (func_51) 支持 25+ 种模式, 新增模式直接在 dispatch 链中添加
- **FPU 管道可复用**: int→double→scale→format 管道 (func_875C→865C→883C→87B4→16B18) 可用于新增传感器数据处理
- **双路径设计**: FPU 路径和整数路径分别在独立分支中, 便于维护
- **两路交叉监控**: StateMachine_Monitor 的双路握手机制可用于添加新的安全交叉检查

---

## 14. 连接器与外部接口汇总

| 连接器 | 类型 | 功能 |
|--------|------|------|
| CN3 | VH3.96-9A (TH) | 主电源输入: 24V DC (2 Pin) + 220V AC (1 Pin) |
| CN1 | A1257WV-S-6P (SMD) | I2C/传感器接口 (I2CSCL, I2CSDA, IR, XSHT) |
| CN2 | XHB-10A (TH) | 外部通信/IO 主接口 (含 W_SENSOR 红外输入) |
| CN4 | A1257WV-S-4P (SMD) | I2C 辅接口 |
| TFT | A2008WR-S-8P (SMD) | 大彩 7寸串口屏 (RS-232 + 供电) |
| USB1 | MICRO-4P-DIP (SMD) | Micro USB (CH340N 编程/调试) |
| P1 | XHB-4A (TH) | 扩展接口 (I2C + GND) |
| GH1 | 1x4P Header 2.54mm (TH) | 4G 模块: MOD_ENABLE, IO_RST, LINK_A, LINK_B |
| GH2 | 1x4P Header 2.54mm (TH) | 4G 模块: WIFI_TX, WIFI_RX, RST/WKUP, GND |
| GH3 | 1x4P Header 2.54mm (TH) | 4G 模块: WIFI-ENABLE, GPS_RX, GPS_TX |
| GH4 | 1x4P Header 2.54mm (TH) | 模块电源: +5V, GND, +3.3V |
| SWD1 | 1x4P Header 2.54mm (TH) | SWD 调试: +3.3V, SWDIO, SWCLK, GND |
| DJC1 | VH3.96-3P2 (TH) | 电解槽功率接口 (VBUS-, VQ 24V) |
| EXT_PWM | XHB-4A (TH) | 外部 PWM 输出 (未启用) |
| EXT_SW | XHB-2A (TH) | 外部开关输入 |
| EXT_IN1~3 | XHB-3A (TH) | 外部输入 (施密特触发, 带 LED) |
| EXT_IN_COUNTER1 | XHB-3A (TH) | 霍尔流量计输入 |
| ERR_CUR1 | XH-2A (TH) | 电流错误输出 |
| ERR-FLOW1 | XH-2A (TH) | 流量错误输出 |
| RDB2 | XH-2A (TH) | 蠕动泵接口 |
| RUNNINGOK_ON1 | XH-2A (TH) | 运行状态输出 |
| POWER_CTR | XH-2A (TH) | 电源控制输入 |

---

## 15. 未启用/保留信号

| 信号 | MCU 引脚 | 备注 |
|------|---------|------|
| CORE_PWM_HIN | U8.24 | Q28(AO3400A) → EXT_PWM.2, 外接 PWM 未启用 |
| CORE_PWM_LIN | U8.25 | Q29(AO3400A) → EXT_PWM.1, 外接 PWM 未启用 |
| XSHT | U8.61 | CN1.6 引出, 传感器关断保留 |
| IR | U8.62 | CN1.5 引出, 第二路红外保留 |
| LINK_A | U8.9 | 4G 模块状态脚, 未使用 |
| LINK_B | U8.10 | 4G 模块状态脚, 未使用 |

---

## 附录 A: 函数速查表

| 函数 (文件) | 地址 | 功能 |
|------|------|------|
| main (func_01_main) | 0x08018524 | 主入口 + 超循环结构 |
| StateMachine_Monitor (func_33) | 0x0800D5E4 | 双路交叉监控 + 门控逻辑 |
| Status_DispatchStub (func_38) | 0x0800D834 | 状态分发存根 |
| Cmd_Dispatcher (func_43) | 0x0800E2D8 | 协议命令分发 |
| Version_String_Sender (func_47) | 0x0800F4E0 | 版本号上报 |
| Display_Page_Renderer (func_48) | 0x0800F5B8 | 25+ 模式多页面渲染 |
| Multi_Param_Handler (func_51) | 0x0800FB48 | 25+ 模式参数处理 + BC2C 编码 |
| FPU_Frame_Sender (func_75) | 0x080126F8 | 17字节 FPU 浮点帧发送 |
| Periodic_Event_Scheduler (func_82) | 0x08012990 | 六阶段周期调度器 |
| State_Machine_Controller (func_87) | 0x08013204 | 设备状态控制 + 级联分支 |
| Telemetry_Packet_Builder (func_125) | 0x08016664 | 95 字节二进制遥测帧构建 |
| ADC_Value_Scaler (func_145) | 0x0800D414 | ADC 采样值缩放与平均 |
| SysTick_Timer_Scheduler (func_146) | 0x08018488 | SysTick 系统定时基础设施 |
| 4G_AT_Handlers (func_148) | 0x08009AF8 | 4G 模块 AT 命令构建与发送 |
| 4G_Telemetry_Reporter (func_149) | 0x08009C84 | MQTT 遥测 FPU 管道 + 格式化上报 |
| Display_State_Controller (func_151) | 0x08017A54 | 显示状态控制 |
| Serial_Frame_Builder (func_152) | 0x08017A9C | 串口帧构建 |
| GpsNmeaParser (func_167) | 0x08017EEA | GPS NMEA $xxRMC 解析 (9 字段) |
| App_Data_Tables (func_169) | 0x08018334 | 数据表/字面量池 |
| Init_Sequence (func_175) | 0x08018BB4 | 初始化序列 |

---

## 附录 B: 内存映射速查

| 地址范围 | 用途 |
|---------|------|
| 0x08008000-0x08018FFF | 固件代码 + 常量池 (APP_BASE) |
| 0x20000000-0x20000FFF | 系统 RAM (运行变量/标志/配置/缓冲区) |
| 0x20000019 | 分支选择标志 (FLAG_19) |
| 0x20000040 | ADC 平均值 (通道 A 电流) |
| 0x2000003E | 通道 B 当前值 (电压) |
| 0x2000004A-0x20000050 | 通道 A/B 阈值 (4 个 uint16) |
| 0x200000E0 | 输出控制标志字 (bit0:电解槽, bit1:水阀, bit2:输出3, bit3:输出4) |
| 0x200001A8 | 启动模式 (0=锁存, !=0=点动) |
| 0x200001B8 | 系统运行计时 (秒, uint32) |
| 0x200001C8 | 目标电流值 (float) |
| 0x2000024A | 全局状态字节 (STATE) |
| 0x200003EC-0x2000042B | 4 个配置块 (4 × 16 字节) |
| 0x2000042C | 设备标识符字符串 |
| 0x42410000-0x42410FFF | FSMC 位段区 (TFT 控制寄存器) |
| 0x40010000-0x40010FFF | TIM1 寄存器 (相控/PWM) |
| 0xE000E000-0xE000EFFF | Cortex-M4 系统控制块 (SysTick/NVIC/SCB) |

---

## 附录 C: 硬件关键网络

| 网络 | 电压/类型 | 说明 |
|------|----------|------|
| GND | 0V | 全局地 (汇总 13 引脚) |
| +3.3V | 3.3V DC | MCU/Flash/数字逻辑供电 |
| +5V | 5V DC | USB/通信/AMS1117 输入 |
| 12V | 12V DC | 风扇(Q24)/继电器/功率级 |
| VIN | 24V DC | 主供电输入 (F1 后) |
| VBUS+ | 24V DC | 功率级 (F2 后, U9 输入) |
| V_POWER | 24V DC | NTC 限流后, Q27 开关输入 |
| VQ | 24V DC | Q27 开关输出 → DJC1 → 电解槽 |
| 220V-L_IN | 220V AC | 水阀供电输入 (F3 前) |
| 220V-L_OUT | 220V AC | 水阀供电输出 (U27 SSR 后) |
| I2CSCL | 3.3V 上拉 | I2C 时钟 (7 个连接点) |
| I2CSDA | 3.3V 上拉 | I2C 数据 (7 个连接点) |
| FAN12V | 12V | 风扇供电输出 (6 个连接点) |
| S+5V | 5V | 系统 5V (传感器上拉) |
| EXT+5V | 5V | 外部接口 5V (MT9700 限流后) |

---

> 文档版本: v2.0 | 基于固件 100% 逆向还原 (175 个函数) | 基于 Netlist + BOM 硬件验证 | 5 轮评审后定稿
