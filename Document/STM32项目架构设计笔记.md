# STM32 项目架构设计笔记

> 整理日期：2026年1月10日

---

## 一、核心概念

### 1.1 BSP（Board Support Package）板级支持包

**定义**：让软件能够在特定硬件板子上运行的一组底层驱动和配置代码。

**包含内容**：
- CPU/芯片初始化代码
- 时钟配置
- 外设驱动（GPIO、UART、SPI、I2C、ADC...）
- 中断配置
- 板子特定的硬件抽象

**作用**：硬件和应用程序之间的"翻译官"，屏蔽硬件差异，提供统一接口。

---

### 1.2 STM32F10x 系列型号

| 型号 | 定位 | 说明 |
|------|------|------|
| STM32F100 | 超值型 | 低成本版本 |
| STM32F101 | 基本型 | 基础功能 |
| STM32F102 | USB基本型 | 带USB |
| STM32F103 | 增强型 | 最常用 |
| STM32F105 | 互联型 | 带CAN、USB OTG |
| STM32F107 | 互联型 | 更强的互联功能 |

> `STM32F10x` 中的 `x` 是通配符，代表整个系列。

---

## 二、软件架构层次

```
┌─────────────────────────────────────┐
│           应用层 (APP)               │  ← 业务逻辑、FreeRTOS任务
├─────────────────────────────────────┤
│         Modules / Drivers            │  ← 协议封装、数据打包（可选层）
├─────────────────────────────────────┤
│           BSP (板级支持包)            │  ← 底层硬件驱动
├─────────────────────────────────────┤
│           HAL 库 (ST官方)            │  ← 封装寄存器操作
├─────────────────────────────────────┤
│           寄存器 (CMSIS)             │  ← 芯片寄存器地址定义
├─────────────────────────────────────┤
│           硬件 (芯片)                │  ← STM32实际电路
└─────────────────────────────────────┘
```

### 各层职责

| 层级 | 职责 | 关键词 |
|------|------|--------|
| **APP** | 业务逻辑，决定"做什么" | 任务、状态机 |
| **Modules/Drivers** | 协议封装，决定"数据长什么样" | 打包、解析、校验 |
| **BSP** | 硬件操作，决定"怎么发出去" | GPIO、中断、外设 |
| **HAL** | 屏蔽芯片差异，提供统一API | ST官方库 |

---

## 三、项目文件夹结构设计

### 3.1 最终结构

```
STM32F10x-APP/
├── APP/                    ← FreeRTOS 任务
│   ├── taskControl.c/h
│   ├── taskSensor.c/h
│   └── ...
│
├── BSP/                    ← 底层硬件驱动
│   ├── DevicesUart.c/h
│   ├── DevicesLed.c/h
│   ├── DevicesSPI.c/h
│   └── ...
│
├── Modules/                ← 可插拔的功能模块
│   ├── Modbus/
│   ├── OTA/
│   ├── Log/
│   └── Bootloader/
│
├── Firmware/               ← ST官方库（不修改）
│   ├── CMSIS/
│   └── STM32F1xx_HAL_Driver/
│
├── Middlewares/            ← 中间件
│   └── FreeRTOS/
│
├── Project/                ← Keil工程文件
│   ├── STM32F10x.uvprojx
│   └── Objects/
│
└── User/                   ← 系统级代码
    ├── main.c              ← 主函数入口
    ├── stm32f1xx_it.c      ← 中断服务函数
    ├── stm32f1xx_hal_conf.h ← HAL配置、晶振频率
    └── userSystem.c        ← 时钟树配置
```

### 3.2 各文件夹说明

| 文件夹 | 内容 | 特点 |
|--------|------|------|
| `APP` | FreeRTOS任务代码 | 业务逻辑层 |
| `BSP` | 底层硬件驱动 | 操作GPIO、外设 |
| `Modules` | 功能模块（Modbus、OTA等） | 可插拔、可复用 |
| `Firmware` | ST官方HAL库 | 不修改、整体使用 |
| `Middlewares` | 中间件（FreeRTOS等） | 第三方库 |
| `Project` | Keil工程文件 | 编译配置 |
| `User` | main、时钟、中断配置 | 系统级初始化 |

---

## 四、命名规范

### 4.1 文件夹命名

| 名称 | 说明 |
|------|------|
| `Firmware` | 单数，固件是不可数名词 |
| `Middlewares` | 复数，可能有多个中间件 |
| `Modules` | 复数，多个功能模块 |

### 4.2 文件命名

BSP层文件统一使用 `Devices` 前缀：
- `DevicesUart.c/h`
- `DevicesLed.c/h`
- `DevicesSPI.c/h`

Modules层文件统一使用 `Driver` 前缀：
- `DriverModbus.c/h`
- `DriverOTA.c/h`
- `DriverLogPrintf.c/h`

---

## 五、设计理念

### 5.1 为什么 Modules 要分文件夹？

```
Modules/
├── Modbus/      ← 一个文件夹 = 一个完整功能
├── OTA/
├── Log/
└── Bootloader/
```

**好处**：
1. **模块化**：每个功能是独立的"包"
2. **方便复用**：复制文件夹到新项目即可
3. **好管理**：相关文件放在一起
4. **好删除**：不需要的功能直接删除文件夹
5. **避免冲突**：不同模块可能有同名文件

### 5.2 为什么 Firmware 放一起？

Firmware 是ST官方HAL库：
- 整体不可拆分
- 不需要修改
- 不需要复用到其他项目（每个项目都会有）

---

## 六、调用关系示例

### 发送 Modbus 指令的调用流程

```c
// 1. APP层 - 业务逻辑
void taskControl(void) {
    Modbus_ReadRegister(0x01, 0x0000, 10);
}

// 2. Modules层 - 协议打包
void Modbus_ReadRegister(uint8_t addr, uint16_t reg, uint16_t num) {
    uint8_t frame[8];
    frame[0] = addr;
    frame[1] = 0x03;
    // ... 组装帧数据 ...
    UART_SendBuffer(frame, 8);  // 调用BSP
}

// 3. BSP层 - 硬件操作
void UART_SendBuffer(uint8_t *data, uint16_t len) {
    HAL_UART_Transmit(&huart1, data, len, 100);  // 调用HAL
}

// 4. HAL层 → 操作寄存器 → 硬件发送
```

---

## 七、User 文件夹的历史

`User` 这个命名来自早期 Keil 和 ST 的示例工程：

| 工具 | 年代 | 命名习惯 |
|------|------|----------|
| 早期 Keil 例程 | 2010年代初 | `User` |
| STM32CubeMX + MDK | 2015年左右 | `Src` / `Inc` |
| STM32CubeIDE | 2019年后 | `Core/Src` / `Core/Inc` |

**User = 用户代码**，意思是"这是用户自己写的，不是官方库"。

可替代名称：`App`、`Core`、`Application`、`Src`

---

## 八、参考项目

- `reference/` - 组长的参考代码
- `trunk/` - 自己的实现代码

---

*文档结束*
