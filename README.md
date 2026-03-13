# STM32F10x_Templates

面向 **STM32F103ZE（正点原子开发板）** 的工程级固件模板仓库。

本项目以实际产品开发标准构建，涵盖 **三级 Boot 架构、OTA 远程升级、FreeRTOS 多任务调度、20+ 外设 BSP 驱动**，以及配套的驱动设计文档与工程复盘，目标是打造一个**可直接复用到产品中的嵌入式基础工程模板**。

---

## 项目亮点

- **三级 Boot 架构（Boot + Bootloader + APP）**：支持 Bootloader 自身的安全升级，断电不变砖
- **OTA 远程固件升级**：基于 RS485/Modbus 协议，支持固件校验、断电续传与回滚
- **FreeRTOS 多任务系统**：7 个任务协同工作，涵盖监控、传感器采集、控制、OTA 等
- **20+ BSP 外设驱动**：UART、RS485、SPI、I2C、CAN、ADC、RTC、定时器、看门狗等全覆盖
- **资源互斥设计**：所有共享总线（RS485、SPI Flash、I2C、CAN 等）使用 FreeRTOS 递归互斥量保护
- **完整设计文档**：每个驱动模块配有独立的设计文档，记录设计思路与踩坑复盘

---

## 目录结构

```
STM32F10x_Templates/
│
├── Document/                               # 设计文档
│   ├── 软件/
│   │   ├── STM32平台基础/                   # 时钟、启动文件、项目架构等
│   │   ├── STM32驱动设计文档/               # 各 BSP 驱动设计文档
│   │   ├── STM32_OTA模块设计/              # 三级 Boot、OTA 安全与容错
│   │   └── 上位机/
│   └── 硬件/                               # 电学基础、GPIO、MOS 管、Buck/Boost 等
│
└── Firmware/
    └── trunk/
        ├── STM32F10x-Boot/                 # Boot 引导程序
        ├── STM32F10x-Bootloader/           # Bootloader 升级程序
        └── STM32F10x-Templates/            # 主 APP 工程
            ├── APP/                        # FreeRTOS 应用任务
            ├── BSP/                        # 板级支持包（外设驱动）
            ├── Modules/                    # 功能模块（Log、Modbus、OTA 等）
            ├── User/                       # main.c、中断、系统初始化
            ├── Firmware/                   # CMSIS + HAL 库
            ├── Middlewares/                # FreeRTOS 内核
            └── Project/                    # Keil 工程文件
```

---

## 系统架构

### 软件分层

```
┌─────────────────────────────────────────────┐
│                APP Layer                     │
│  taskSystem / taskMonitor / taskSensor /     │
│  taskControl / taskKey / taskOTA / ...       │
├─────────────────────────────────────────────┤
│              Modules Layer                   │
│  DriverLogPrintf / DriverModbus /            │
│  DriverBootloader / DriverOTA               │
├─────────────────────────────────────────────┤
│               BSP Layer                      │
│  DevicesUart / DevicesRS485 / DevicesSPI /   │
│  DevicesIIC / DevicesCAN / DevicesADC / ...  │
├─────────────────────────────────────────────┤
│          HAL + CMSIS + FreeRTOS              │
├─────────────────────────────────────────────┤
│             STM32F103ZE Hardware              │
└─────────────────────────────────────────────┘
```

### 三级 Boot 与 Flash 分区

```
内部 Flash (512KB)
┌────────────────────────┬──────────────┬────────────┐
│ Boot (16KB)            │ 数据区 (8KB) │            │
│ 0x08000000             │ System/OTA/  │            │
│ 最小引导程序            │ OTP/User     │            │
├────────────────────────┼──────────────┤            │
│ Bootloader (64KB)      │              │            │
│ 0x0800A000             │              │            │
│ 负责 APP 升级与跳转     │              │            │
├────────────────────────┤              │            │
│ APP (剩余空间)          │              │            │
│ 0x0801A000             │              │            │
│ FreeRTOS 业务应用       │              │            │
└────────────────────────┴──────────────┴────────────┘
```

**三级 Boot 启动流程**：上电 → Boot 检查 Bootloader 完整性 → 跳转 Bootloader → Bootloader 检查 APP → 跳转 APP

相比传统二级架构，三级架构的核心优势在于 **Bootloader 自身也可以安全升级**，即使升级过程中断电也不会变砖。

---

## 外设驱动一览

| 外设 | BSP 模块 | 说明 |
|:---|:---|:---|
| GPIO | DevicesLed / DevicesKey / DevicesBeep | LED 指示、按键检测、蜂鸣器控制 |
| UART | DevicesUart | USART1 调试日志、USART2 总线通信 |
| RS485 | DevicesRS485 | 半双工总线通信，方向控制 |
| Modbus | DevicesModbus | Modbus RTU 协议栈 |
| I2C | DevicesIIC / DevicesAT24C02 | 软件 I2C + AT24C02 EEPROM 驱动 |
| SPI | DevicesSPI / DevicesSPIFlash | 硬件 SPI + W25Qxx Flash 驱动 |
| CAN | DevicesCAN | CAN 总线通信 |
| ADC | DevicesADC | 模拟量采集 |
| Timer | DevicesTimer / DevicesSoftTimer | 硬件定时器 + 软件定时器 |
| RTC | DevicesRTC | 实时时钟 |
| IWDG | DevicesWatchDog | 独立看门狗 |
| Flash | DevicesFlash | 内部 Flash 读写，用于参数存储与 OTA |
| CRC | DevicesCRC | 固件校验 |
| Queue | DevicesQueue | 环形缓冲区 |
| Delay | DevicesDelay | 微秒/毫秒精确延时 |

---

## FreeRTOS 任务设计

| 任务 | 功能 | 优先级 |
|:---|:---|:---|
| taskSystem | 系统初始化与状态管理 | 高 |
| taskMonitor | 运行状态监控 | 高 |
| taskSensor | 传感器数据采集与处理、看门狗喂狗 | 中 |
| taskControl | 控制逻辑执行 | 中 |
| taskKey | 按键扫描与事件处理 | 低 |
| taskMessageSlave | RS485/Modbus 从机通信 | 中 |
| taskOTA | OTA 升级流程管理 | 中 |
| taskSoftTimer | 软件定时器调度 | 低 |

---

## 开发环境

| 项目 | 说明 |
|:---|:---|
| IDE | Keil MDK-ARM (uVision) |
| 编译器 | ARMCLANG (ARM Compiler 6) |
| 目标芯片 | STM32F103ZET6 (Cortex-M3, 512KB Flash, 64KB RAM) |
| 库 | STM32 HAL Driver + CMSIS |
| RTOS | FreeRTOS V202112.00 |
| 开发板 | 正点原子 STM32F103 精英版 |

---

## 构建与烧录

1. 使用 Keil MDK 打开工程文件：

```
Firmware/trunk/STM32F10x-Templates/Project/STM32F10x_Templates.uvprojx
```

2. 编译工程，编译后脚本会自动复制 Hex 文件并合并 Boot + Bootloader + APP

3. 使用 ST-Link / J-Link 烧录合并后的 Hex 文件到开发板

> Boot 和 Bootloader 工程分别位于 `STM32F10x-Boot/` 和 `STM32F10x-Bootloader/` 目录下，需单独编译。

---

## 设计文档

本项目为每个模块编写了独立的设计文档，记录设计决策、接口定义、时序分析与问题复盘：

**平台基础**
- [STM32 概述与芯片选型](Document/软件/STM32平台基础/STM32%20概述与芯片选型.md)
- [STM32 时钟系统](Document/软件/STM32平台基础/STM32时钟系统学习笔记.md)
- [STM32 启动文件分析](Document/软件/STM32平台基础/STM32启动文件学习笔记.md)
- [STM32 项目架构设计](Document/软件/STM32平台基础/STM32项目架构设计笔记.md)

**OTA 与 Boot 架构**
- [三级 Boot 架构设计](Document/软件/STM32_OTA模块设计/三级Boot架构设计.md)
- [OTA 整体设计思路与架构](Document/软件/STM32_OTA模块设计/OTA整体设计思路与架构.md)
- [OTA 安全与容错](Document/软件/STM32_OTA模块设计/OTA安全与容错.md)

**驱动设计文档**
- [UART 驱动设计](Document/软件/STM32驱动设计文档/DevicesUart_Design.md) | [RS485 驱动设计](Document/软件/STM32驱动设计文档/DevicesRS485_Design.md) | [SPI Flash 驱动设计](Document/软件/STM32驱动设计文档/DevicesSPIFlash_Design.md)
- [I2C 驱动设计](Document/软件/STM32驱动设计文档/DevicesIIC_Design.md) | [ADC 驱动设计](Document/软件/STM32驱动设计文档/DevicesADC_Design.md) | [Flash 驱动设计](Document/软件/STM32驱动设计文档/DevicesFlash_Design.md)
- [RTC 驱动设计](Document/软件/STM32驱动设计文档/DevicesRTC_Design.md) | [看门狗设计](Document/软件/STM32驱动设计文档/DevicesWatchDog_Design.md) | [软件定时器设计](Document/软件/STM32驱动设计文档/DevicesSoftTimer_Design.md)
- [日志模块设计](Document/软件/STM32驱动设计文档/DriverLogPrintf_Design.md) | [队列设计](Document/软件/STM32驱动设计文档/DevicesQueue_Design.md) | [定时器全功能总结](Document/软件/STM32驱动设计文档/定时器全功能总结.md)
- [更多驱动文档...](Document/软件/STM32驱动设计文档/)

---

## 作者

如果你对本项目有任何问题或建议，欢迎交流。
