# STM32F10x I2C 与 AT24C02 设计文档 (DevicesIIC / DevicesAT24C02)

| 属性 | 内容 |
| :--- | :--- |
| **模块名称** | DevicesIIC (I2C 总线驱动) + DevicesAT24C02 (EEPROM 应用层) |
| **源文件** | `DevicesIIC.c` / `DevicesIIC.h` / `DevicesAT24C02.c` / `DevicesAT24C02.h` |
| **硬件依赖** | I2C1 外设, PB6 (SCL), PB7 (SDA), AT24C02 芯片, 外部上拉电阻 |
| **软件依赖** | DevicesDelay, FreeRTOS (递归互斥信号量) |
| **版本** | v1.0 |
| **最后更新** | 2026-02-26 |

---

## 1. 设计目标 (Design Goals)

实现基于 STM32F1 硬件 I2C 外设的 AT24C02 EEPROM 读写驱动：

1. **硬件 I2C 通信**：使用 I2C1 外设，标准模式 100kHz，7 位地址
2. **页对齐写入**：自动处理 AT24C02 的 8 字节页写入限制，防止地址回绕
3. **RTOS 安全**：通过递归互斥信号量保护 I2C 总线访问，支持多任务环境
4. **统一接口**：提供单字节和多字节的读写 API

---

## 2. I2C 总线基础原理 (I2C Fundamentals)

### 2.1 什么是 I2C？

I2C (Inter-Integrated Circuit) 是由 Philips (现 NXP) 开发的 **两线式半双工同步串行总线**，仅需两根信号线即可实现多设备通信。

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          I2C 总线拓扑结构                                     │
└─────────────────────────────────────────────────────────────────────────────┘

        VCC              VCC
         │                │
        ┌┴┐ Rp           ┌┴┐ Rp
        │ │ 4.7K          │ │ 4.7K       ← 外部上拉电阻 (必须)
        └┬┘              └┬┘
         │                │
   SCL ──┼────────────────┼──────────────────────── SCL 时钟线
         │                │
   SDA ──┼────────────────┼──────────────────────── SDA 数据线
         │                │
       ┌─┴─┐  ┌─┴─┐  ┌─┴─┐  ┌─┴─┐
       │主机│  │从机1│ │从机2│ │从机3│
       │MCU │  │0x50│  │0x68│  │0x76│
       └───┘  └───┘  └───┘  └───┘
              AT24C02  MPU6050  BME280
```

### 2.2 为什么 I2C 必须使用开漏 + 上拉？

I2C 总线采用 **开漏输出 + 外部上拉电阻** 的结构，这是协议强制要求的：

```
开漏输出原理:

    VCC                    VCC
     │                      │
    ┌┴┐ Rp (上拉电阻)       ┌┴┐ Rp
    └┬┘                    └┬┘
     │                      │
     ├─── SDA 线 ───────────┤
     │                      │
   ┌─┴─┐                 ┌─┴─┐
   │主机│                 │从机│
   │OD  │                 │OD  │
   └─┬─┘                 └─┬─┘
     │                      │
    GND                    GND

设备只能将总线拉低 (输出 0)，释放后由上拉电阻恢复高电平 (1)。
任何一个设备拉低，总线就是低电平 → 实现"线与"逻辑。
```

**开漏结构的三个作用**：

| 作用 | 说明 |
| :--- | :--- |
| **防止短路** | 即使两个设备同时输出不同电平，也不会短路（不像推挽输出） |
| **支持多主机** | 多个主机可以通过仲裁机制共享总线 |
| **双向通信** | SDA 线既能发又能收，主机和从机都能控制 |

### 2.3 I2C 通信速率

| 模式 | 速率 | 说明 |
| :--- | :--- | :--- |
| **标准模式 (Standard)** | 100 kHz | 本项目使用，兼容性最好 |
| 快速模式 (Fast) | 400 kHz | 多数 I2C 设备支持 |
| 快速模式+ (Fast-mode Plus) | 1 MHz | 部分设备支持 |
| 高速模式 (High-speed) | 3.4 MHz | 较少使用 |

### 2.4 I2C 通信时序

一次完整的 I2C 通信流程：

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     I2C 写操作时序 (主机写从机)                                │
└─────────────────────────────────────────────────────────────────────────────┘

SCL: ────┐  ┌──┐  ┌──┐       ┌──┐  ┌──┐       ┌──┐  ┌──┐       ┌──┐  ┌────
         └──┘  └──┘  └── ... ──┘  └──┘  └── ... ──┘  └──┘  └── ... ──┘  └──

SDA: ──┐                                                                ┌────
       └──[  7位从机地址  ][R/W]─[ACK]──[ 8位数据 ]──[ACK]──[8位数据]──[ACK]┘
       ↑                    ↑      ↑                   ↑                  ↑
     START               W=0   从机应答            从机应答              STOP


各阶段说明:
─────────────────────────────────────────────────
1. START 信号:  SCL 高电平时，SDA 从高→低 (下降沿)
2. 从机地址:    7 位地址 + 1 位读写方向 (0=写, 1=读)
3. ACK 应答:    地址匹配的从机拉低 SDA 表示应答
4. 数据传输:    每次传输 8 位，MSB 先发
5. STOP 信号:   SCL 高电平时，SDA 从低→高 (上升沿)
```

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     I2C 读操作时序 (主机读从机)                                │
└─────────────────────────────────────────────────────────────────────────────┘

SCL: ────┐  ┌──┐  ┌──┐       ┌──┐  ┌──┐       ┌──┐  ┌──┐       ┌──┐  ┌────
         └──┘  └──┘  └── ... ──┘  └──┘  └── ... ──┘  └──┘  └── ... ──┘  └──

SDA: ──┐                                                               ┌────
       └──[  7位从机地址  ][R/W]─[ACK]──[ 8位数据 ]──[ACK]──[8位数据]─[NACK]┘
       ↑                    ↑      ↑       ↑           ↑       ↑        ↑
     START               R=1   从机应答  从机发数据  主机应答  从机发  主机NACK
                                                    (继续读)         (停止读)
```

> **注意**：以上是通用 I2C 的简化时序。对于带内存地址的设备（如 EEPROM），实际时序还需要包含 **内存地址 (WORD ADDRESS)** 阶段，详见下方 AT24C02 专用时序。

### 2.7 AT24C02 专用读写时序

AT24C02 是一个有内部存储地址的设备，读写时序与普通 I2C 不同，需要先发送 **内存地址 (WORD ADDRESS)** 告诉芯片要操作哪个位置。

#### AT24C02 写时序 (Byte Write)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                  AT24C02 写操作时序 (主机写 EEPROM)                           │
└─────────────────────────────────────────────────────────────────────────────┘

SCL: ────┐  ┌──┐       ┌──┐  ┌──┐       ┌──┐  ┌──┐       ┌──┐  ┌────
         └──┘  └── ... ──┘  └──┘  └── ... ──┘  └──┘  └── ... ──┘  └──

SDA: ──┐                                                           ┌────
       └──[ 1010 000 ][W=0]─[ACK]──[内存地址]──[ACK]──[ 数据 ]──[ACK]┘
       ↑    设备地址     ↑     ↑    WORD ADDR    ↑                   ↑
     START  (0xA0)    写方向 从机应答 (0~255)  从机应答             STOP


流程说明:
─────────────────────────────────────────────────
1. START:        主机发起起始信号
2. 设备地址+W:   发送 0xA0 (1010_0000)，R/W=0 表示写
3. ACK:          AT24C02 地址匹配，拉低 SDA 应答
4. 内存地址:     发送要写入的存储单元地址 (0x00~0xFF)
5. ACK:          AT24C02 应答
6. 数据:         发送要写入的数据字节
7. ACK:          AT24C02 应答
8. STOP:         主机发送停止信号，AT24C02 开始内部编程 (需等待 ≥5ms)

注意: EEPROM 写入较慢，发送 STOP 后必须等待 10ms 再写下一个字节/页。
```

#### AT24C02 读时序 (Random Read — 需要 Dummy Write)

AT24C02 读操作需要先用一次 **"假写" (Dummy Write)** 设置内存地址，然后重新发起 START 切换为读方向。

```
┌─────────────────────────────────────────────────────────────────────────────┐
│              AT24C02 读操作时序 (主机随机读 EEPROM)                            │
└─────────────────────────────────────────────────────────────────────────────┘

         ┌── 第1阶段: Dummy Write (设置内存地址) ──┐┌─ 第2阶段: 读数据 ─┐

SCL: ────┐  ┌──┐       ┌──┐  ┌──┐       ┌──┐  ┌──┐       ┌──┐  ┌────
         └──┘  └── ... ──┘  └──┘  └── ... ──┘  └──┘  └── ... ──┘  └──

SDA: ──┐                              ┐                            ┌────
       └──[1010 000][W=0]─[ACK]──[内存地址]──[ACK]┘[1010 000][R=1]─[ACK]──[数据]─[NACK]┘
       ↑   设备地址    ↑     ↑    WORD ADDR   ↑  ↑  设备地址    ↑     ↑     ↑       ↑
     START (0xA0)   写方向 从机应答 (0~255) 从机  Re-  (0xA1)  读方向 从机  从机发  主机NACK
                                           应答  START               应答  数据   (停止读)
                                                                                    ↓
                                                                                   STOP

流程说明:
─────────────────────────────────────────────────
第1阶段 — Dummy Write (设置读取地址):
  1. START:        主机发起起始信号
  2. 设备地址+W:   发送 0xA0 (写方向)，目的是设置内存地址指针
  3. ACK:          AT24C02 应答
  4. 内存地址:     发送要读取的存储单元地址 (0x00~0xFF)
  5. ACK:          AT24C02 应答，内部地址指针已指向目标位置

第2阶段 — 实际读取:
  6. Re-START:     主机发起重复起始信号 (不发 STOP，直接再发 START)
  7. 设备地址+R:   发送 0xA1 (读方向)
  8. ACK:          AT24C02 应答
  9. 数据:         AT24C02 将目标地址的数据发送到 SDA
  10. NACK+STOP:   主机发送 NACK 告知不再读取，然后发送 STOP 结束通信

为什么需要 Dummy Write:
  AT24C02 没有单独的"设置地址"命令。唯一的方式是先发一次写操作
  (只写设备地址+内存地址，不写数据)，将内部地址指针移到目标位置，
  然后通过 Re-START 切换为读方向，从该位置读出数据。
  HAL_I2C_Mem_Read() 内部自动完成了这个 Dummy Write + Re-START 过程。
```

### 2.5 I2C 地址机制 — 类似 Modbus 的寻址方式

I2C 总线通过 **从机地址** 区分设备，工作方式类似 RS485 + Modbus：

| | I2C | RS485 + Modbus |
| :--- | :--- | :--- |
| **总线结构** | 多从机共享 SDA/SCL | 多从机共享 A/B 差分线 |
| **寻址方式** | 主机发送 7 位从机地址 | 主机发送从机站号 |
| **应答机制** | 地址匹配的从机拉低 SDA 发 ACK | 地址匹配的从机回复响应帧 |
| **其他从机** | 地址不匹配 → 忽略，不响应 | 站号不匹配 → 忽略，不响应 |

```
主机发送地址 0x50 (AT24C02):

                     I2C 总线 (SCL + SDA)
    ════════════════════════════════════════════════════
           │           │           │
         ┌─┴─┐       ┌─┴─┐       ┌─┴─┐
         │主机│       │从机1│     │从机2│
         │MCU │       │0x50│      │0x68│
         └───┘       └───┘       └───┘
                       ↑
                  地址匹配！
                  拉低SDA发ACK
                  开始接收数据

从机2 (0x68): 地址不匹配 → 忽略，保持静默
```

### 2.6 硬件 I2C vs 软件 I2C

| | 硬件 I2C | 软件 I2C (Bit-Banging) |
| :--- | :--- | :--- |
| **实现方式** | I2C 外设自动处理时序 | 代码手动翻转 GPIO 电平 |
| **引脚选择** | 固定引脚 (或重映射) | 任意 GPIO |
| **CPU 占用** | 低，外设独立工作 | 高，每个 bit 都要 CPU 参与 |
| **代码复杂度** | 简单，几个 HAL 函数搞定 | 复杂，需实现完整协议 |
| **速率精度** | 硬件自动分频，时序精确 | 靠延时函数控制，精度较差 |
| **支持 DMA/中断** | 支持 | 不支持 |
| **RTOS 兼容性** | 好，时序不受任务切换影响 | 差，需进入临界区保护时序 |
| **可移植性** | 依赖具体芯片外设 | 任何有 GPIO 的芯片都能跑 |
| **STM32F1 稳定性** | 有已知 bug (Errata)，可能卡死 | 自己控制，反而更可控 |

> **本项目选择硬件 I2C**：在 FreeRTOS 环境下，硬件 I2C 不需要进入临界区保护时序，对系统实时性影响更小。如遇到 F1 系列硬件 I2C 卡死问题，可考虑切换到软件 I2C。

---

## 3. AT24C02 EEPROM 基础 (AT24C02 Fundamentals)

### 3.1 AT24C02 概述

AT24C02 是一款 **2Kbit (256 字节)** 的 I2C 接口 EEPROM 存储芯片。

| 参数 | 值 |
| :--- | :--- |
| 容量 | 2Kbit = 256 字节 (地址 0x00 ~ 0xFF) |
| 页大小 | 8 字节 |
| 设备地址 | 0x50 (7位)，左移后 0xA0 (写) / 0xA1 (读) |
| 写入时间 | 最大 5ms (写入一页后需等待) |
| 擦写寿命 | 100 万次 |
| 数据保持 | 100 年 |

### 3.2 EEPROM vs Flash — 不需要擦除

EEPROM 和 Flash 最大的区别：**EEPROM 写入前不需要擦除**。

| | EEPROM (AT24C02) | Flash |
| :--- | :--- | :--- |
| **写入前是否需要擦除** | **不需要**，直接覆盖写入 | **需要**，必须先擦除 (置 0xFF) |
| **擦除粒度** | 不适用 | 按扇区/块擦除 (通常 4KB 起) |
| **写入粒度** | 按字节或按页 (8 字节) | 按字/半字/双字 |
| **写入次数** | ~100 万次 | ~1 万次 |
| **原理** | 浮栅晶体管，可逐字节电擦写 | 浮栅晶体管，只能批量擦除 |

### 3.3 AT24C02 设备地址

AT24C02 的 7 位 I2C 地址由芯片引脚 A2/A1/A0 决定：

```
7 位地址格式:

    ┌───┬───┬───┬───┬───┬───┬───┐
    │ 1 │ 0 │ 1 │ 0 │ A2│ A1│ A0│
    └───┴───┴───┴───┴───┴───┴───┘
     固定前缀 (1010)    引脚配置

当 A2=A1=A0=0 时:
    7位地址 = 0b1010000 = 0x50
    
发送到总线时左移 1 位，加上 R/W 位:
    写地址 = (0x50 << 1) | 0 = 0xA0
    读地址 = (0x50 << 1) | 1 = 0xA1
```

### 3.4 页写入限制 — 为什么需要页对齐

AT24C02 的页大小为 **8 字节**，有一个硬件限制：

> **单次写入不能跨越页边界。** 如果数据超出当前页的剩余空间，超出部分会 **回绕到当前页起始地址**，覆盖掉之前的数据。

```
错误示例 — 不做页对齐，从 0x05 写 10 字节:

第0页:  0x00  0x01  0x02  0x03  0x04  0x05  0x06  0x07
                                      [D1]  [D2]  [D3]   ← 正常写入 3 字节
                                      
        写到 0x07 后，地址回绕到 0x00！
        
第0页:  [D4]  [D5]  [D6]  [D7]  [D8]  [D1]  [D2]  [D3]
        ↑                        ↑
        D4~D8 覆盖了 0x00~0x04 的原始数据！
        D9、D10 丢失！
```

**解决方案 — 页对齐计算：**

```c
usLengthTemp = usLength > (PAGE_SIZE - ucAddress % PAGE_SIZE)
             ? (PAGE_SIZE - ucAddress % PAGE_SIZE)
             : usLength;
```

本质是 **`min(要写的长度, 当前页剩余空间)`**，确保每次写入都不跨页。

```
正确示例 — 页对齐写入，从 0x05 写 10 字节:

第1次: 地址=0x05, 页剩余=8-5=3, 本次写 3 字节
第0页:  0x00  0x01  0x02  0x03  0x04  0x05  0x06  0x07
                                      [D1]  [D2]  [D3]

第2次: 地址=0x08, 页剩余=8-0=8, 本次写 7 字节 (剩余全部)
第1页:  0x08  0x09  0x0A  0x0B  0x0C  0x0D  0x0E  0x0F
        [D4]  [D5]  [D6]  [D7]  [D8]  [D9]  [D10]

分 2 次写入，每次都不跨页，数据完整无误。
```

### 3.5 写入后的等待时间

AT24C02 每次页写入后，芯片内部需要一段时间将数据从缓冲区写入非易失存储单元：

- 官方手册标称最大 **5ms**
- 本项目使用 **10ms** 延时，更稳健

```
写入时序:

  I2C 写入       内部编程          可以进行下一次写入
  ─────────►│◄──── 5ms ────►│──────────────────►
             │   (芯片忙,      │
             │    不响应ACK)    │
             
  在内部编程期间发起 I2C 通信，芯片不会应答 (NACK)。
```

---

## 4. 硬件连接 (Hardware Connection)

### 4.1 引脚分配

| MCU 引脚 | I2C 信号 | AT24C02 引脚 | 说明 |
| :--- | :--- | :--- | :--- |
| PB6 | SCL | 6 (SCL) | 时钟线，需 4.7K 上拉到 VCC |
| PB7 | SDA | 5 (SDA) | 数据线，需 4.7K 上拉到 VCC |

### 4.2 典型电路

```
                    VCC (3.3V)          VCC (3.3V)
                     │                      │
                    ┌┴┐ 4.7K               ┌┴┐ 4.7K
                    └┬┘                    └┬┘
                     │                      │
    STM32 PB6 ───────┼──────────────────────┼───── AT24C02 Pin6 (SCL)
     (SCL)           │                      │
    STM32 PB7 ───────┼──────────────────────┼───── AT24C02 Pin5 (SDA)
     (SDA)           │                      │

                     AT24C02 引脚:
                     ┌──────────┐
         A0 ─── GND ─┤ 1    8 ├── VCC (3.3V)
         A1 ─── GND ─┤ 2    7 ├── WP (写保护, 接 GND = 不保护)
         A2 ─── GND ─┤ 3    6 ├── SCL
        GND ─────────┤ 4    5 ├── SDA
                     └──────────┘
                     
    A0=A1=A2=GND → 设备地址 = 0x50 (0xA0/0xA1)
    WP=GND → 允许写入所有地址
```

---

## 5. 软件设计 (Software Design)

### 5.1 模块分层

```
┌─────────────────────────────────────────┐
│          应用层 (taskKey.c 等)            │  ← 调用 AT24C02 读写接口
├─────────────────────────────────────────┤
│     DevicesAT24C02.c / .h               │  ← 页对齐处理 + RTOS 信号量保护
│     (EEPROM 应用层)                      │
├─────────────────────────────────────────┤
│     DevicesIIC.c / .h                   │  ← I2C 底层驱动 (HAL 硬件 I2C)
│     (I2C 总线驱动层)                     │
├─────────────────────────────────────────┤
│     STM32 HAL 库                         │  ← HAL_I2C_Mem_Write/Read
├─────────────────────────────────────────┤
│     I2C1 硬件外设                        │  ← PB6 (SCL), PB7 (SDA)
└─────────────────────────────────────────┘
```

### 5.2 I2C 初始化 (DevicesIIC.c)

```c
static I2C_HandleTypeDef g_i2c_handle;

void vIICInit(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_I2C1_CLK_ENABLE();

    GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD; 
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    g_i2c_handle.Instance = I2C1;
    g_i2c_handle.Init.ClockSpeed = 100000;
    g_i2c_handle.Init.DutyCycle = I2C_DUTYCYCLE_2;
    g_i2c_handle.Init.OwnAddress1 = 0;
    g_i2c_handle.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    g_i2c_handle.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    g_i2c_handle.Init.OwnAddress2 = 0;
    g_i2c_handle.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    g_i2c_handle.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

    HAL_I2C_Init(&g_i2c_handle);
}
```

**配置项详解：**

| 配置项 | 值 | 含义 |
| :--- | :--- | :--- |
| GPIO Mode | `GPIO_MODE_AF_OD` | 复用开漏输出，I2C 协议要求 |
| GPIO Speed | `GPIO_SPEED_FREQ_HIGH` | 50MHz，确保信号边沿陡峭 |
| Instance | `I2C1` | 使用 I2C1 外设 |
| ClockSpeed | `100000` (100kHz) | 标准模式 |
| DutyCycle | `I2C_DUTYCYCLE_2` | SCL 占空比 Tlow:Thigh = 2:1 |
| OwnAddress1 | `0` | 本机地址为 0，作为主机使用 |
| AddressingMode | `I2C_ADDRESSINGMODE_7BIT` | 7 位寻址，最常见的模式 |
| DualAddressMode | `I2C_DUALADDRESS_DISABLE` | 禁用双地址 |
| GeneralCallMode | `I2C_GENERALCALL_DISABLE` | 禁用广播呼叫 |
| NoStretchMode | `I2C_NOSTRETCH_DISABLE` | 允许时钟延展 (从机可拉低 SCL 让主机等待) |

### 5.3 I2C 底层读写 (DevicesIIC.c)

```c
int32_t iI2CWriteData(uint8_t ucDevicesAddr, uint8_t ucRegisterAddr, uint8_t data)
{
    if(g_i2c_handle.Instance == NULL)
        return 1;

    if(HAL_I2C_Mem_Write(&g_i2c_handle, ucDevicesAddr, ucRegisterAddr,
                         I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY) != HAL_OK)
        return 2;

    return 0;
}

int32_t iI2CWriteDatas(uint8_t ucDevicesAddr, uint8_t ucRegisterAddr,
                       uint8_t *pData, uint16_t size)
{
    if(g_i2c_handle.Instance == NULL)
        return 1;

    if(HAL_I2C_Mem_Write(&g_i2c_handle, ucDevicesAddr, ucRegisterAddr,
                         I2C_MEMADD_SIZE_8BIT, pData, size, HAL_MAX_DELAY) != HAL_OK)
        return 2;

    return 0;
}
```

**返回值约定：**

| 返回值 | 含义 |
| :--- | :--- |
| `0` | 成功 |
| `1` | I2C 未初始化 (Instance == NULL) |
| `2` | HAL 通信失败 (NACK / 超时 / 总线错误) |

### 5.4 AT24C02 页对齐写入 (DevicesAT24C02.c)

```c
int8_t cAT24C02WriteDatas(uint8_t ucAddress, uint8_t *pucDatas, uint16_t usLength)
{
    int8_t cError = 0;
    uint16_t usLengthTemp = 0;

    cIICFlashLock();    /* 获取互斥信号量 */

    while(usLength > 0)
    {
        /* 页对齐: 本次写入量 = min(剩余量, 当前页剩余空间) */
        usLengthTemp = usLength > (PAGE_SIZE - ucAddress % PAGE_SIZE)
                     ? (PAGE_SIZE - ucAddress % PAGE_SIZE)
                     : usLength;

        cError = iI2CWriteDatas(AT24C02_WRITE_ADDRESS, ucAddress, pucDatas, usLengthTemp);

        if(cError != 0)
            break;

        ucAddress += usLengthTemp;
        pucDatas  += usLengthTemp;
        usLength  -= usLengthTemp;

        /* 官方手册推荐等待 5ms，写 10ms 更稳健 */
        vRtosDelayMs(10);
    }

    cIICFlashUnlock();  /* 释放互斥信号量 */

    return cError;
}
```

### 5.5 RTOS 互斥保护

AT24C02 的读写操作使用 **FreeRTOS 递归互斥信号量** 保护，防止多任务同时访问 I2C 总线：

```c
/* taskSystem.c 中创建 */
SemaphoreHandle_t g_xIICFlashSemaphore;
g_xIICFlashSemaphore = xSemaphoreCreateRecursiveMutex();

/* DevicesAT24C02.c 中使用 */
static int8_t cIICFlashLock(void)
{
    xSemaphoreTakeRecursive(g_xIICFlashSemaphore, portMAX_DELAY);
    return 0;
}

static int8_t cIICFlashUnlock(void)
{
    xSemaphoreGiveRecursive(g_xIICFlashSemaphore);
    return 0;
}
```

**为什么用递归互斥信号量：**
- 允许同一个任务多次获取锁而不死锁
- `portMAX_DELAY` 表示永久等待，直到获取成功

---

## 6. API 总览 (API Reference)

### 6.1 I2C 底层接口 (DevicesIIC.h)

| 函数 | 功能 | 返回值 |
| :--- | :--- | :--- |
| `vIICInit()` | 初始化 I2C1 外设和 GPIO | 无 |
| `iI2CWriteData(addr, reg, data)` | 写单字节 | 0: 成功 |
| `iI2CReadData(addr, reg, *pData)` | 读单字节 | 0: 成功 |
| `iI2CWriteDatas(addr, reg, *pData, size)` | 写多字节 | 0: 成功 |
| `iI2CReadDatas(addr, reg, *pData, size)` | 读多字节 | 0: 成功 |

### 6.2 AT24C02 应用接口 (DevicesAT24C02.h)

| 函数 | 功能 | 返回值 |
| :--- | :--- | :--- |
| `cAT24C02WriteDatas(addr, *pData, len)` | 写任意长度 (自动页对齐) | 0: 成功 |
| `cAT24C02ReadDatas(addr, *pData, len)` | 读任意长度 (自动页对齐) | 0: 成功 |

### 6.3 宏定义

```c
/* DevicesIIC.h */
#define AT24C02_WRITE_ADDRESS          0xA0
#define AT24C02_READ_ADDRESS           0xA1

/* DevicesAT24C02.h */
#define PAGE_SIZE                      8
```

---

## 7. 使用示例 (Usage Examples)

### 7.1 初始化

```c
#include "DevicesIIC.h"
#include "DevicesAT24C02.h"

void vSystemPeripheralInit(void)
{
    vIICInit();     /* 初始化 I2C1 外设 */
}
```

### 7.2 写入数据

```c
uint8_t writeData[20] = {0x01, 0x02, 0x03, /* ... */ };

/* 从地址 0x00 写入 20 字节，自动页对齐，自动等待 */
int8_t ret = cAT24C02WriteDatas(0x00, writeData, 20);
if(ret != 0)
{
    /* 写入失败处理 */
}
```

### 7.3 读取数据

```c
uint8_t readData[20] = {0};

/* 从地址 0x00 读取 20 字节 */
int8_t ret = cAT24C02ReadDatas(0x00, readData, 20);
if(ret != 0)
{
    /* 读取失败处理 */
}
```

---

## 8. I2C 总线死锁原因及解决办法

### 8.1 什么是 I2C 死锁

I2C 死锁是指主机和从机互相等待对方动作，导致总线永久卡死的状态。表现为 SDA 被从机拉低不释放，主机检测到总线忙（BUSY），无法发起任何新的通讯。

这不是 STM32F1 特有的问题，而是 I2C 协议层面的通病，任何平台、任何 I2C 实现（硬件或软件）都可能遇到。

### 8.2 死锁是怎么发生的

典型触发场景：主机在从机正在发送数据的过程中发生了复位。

```
正常通讯中，主机正在读取从机数据（从机正在驱动 SDA）：

SCL: ──┐  ┌──┐  ┌──┐  ┌──┐  ┌──
       └──┘  └──┘  └──┘  └──┘
SDA:       [D7] [D6] [D5] [D4] ...
                       ↑
                  从机正在发送 bit5 = 0（SDA 拉低）
                  此时主机突然复位
```

复位后的状态：

```
1. 主机复位 → I2C 外设重新初始化 → SCL/SDA 配置为空闲态
2. 从机不知道主机复位了 → 还停在上一次传输的中间
3. 从机继续持有 SDA = 低 → 等待主机给下一个 SCL 脉冲来移出下一位数据
4. 主机想发起新的 START 条件 → 需要 SDA 从高到低的跳变
5. 但 SDA 被从机拉着 = 低 → 主机检测到总线忙 → 拒绝发起通讯

从机等主机给时钟 → 主机等从机释放 SDA → 死锁
```

可能触发复位的原因：

- 看门狗超时复位
- 软件调用 `NVIC_SystemReset()`
- 电源瞬间波动导致 MCU 复位
- 调试器断点/重新烧录

### 8.3 解决方法一：SCL 模拟 9 个时钟脉冲

在 I2C 初始化之前，先检查 SDA 状态。如果 SDA 被拉低，用 GPIO 模式手动给 SCL 发脉冲，把从机状态机"走完"，让它释放 SDA。

**原理**：最坏情况下从机正好在发一个字节的第 1 位，还剩 7 位数据 + 1 位 ACK = 8 个脉冲才能走完，加 1 个留余量，共 9 个脉冲。每给一个时钟沿，从机的状态机就往前推进一步，走完后从机释放 SDA。

```c
void vI2CBusRelease(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* 将 SCL 配置为普通推挽输出，SDA 配置为浮空输入 */
    GPIO_InitStruct.Pin = GPIO_PIN_6;           /* SCL = PB6 */
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_7;           /* SDA = PB7 */
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* 发送最多 9 个时钟脉冲，直到 SDA 释放 */
    for(uint8_t i = 0; i < 9; i++)
    {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);
        vDelayUs(5);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
        vDelayUs(5);

        /* SDA 已经恢复高电平，从机释放了总线 */
        if(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7) == GPIO_PIN_SET)
            break;
    }

    /* 发送 STOP 条件：SCL 高时，SDA 从低到高 */
    GPIO_InitStruct.Pin = GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET);  /* SDA = 低 */
    vDelayUs(5);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);    /* SCL = 高 */
    vDelayUs(5);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);    /* SDA = 高 → STOP */
    vDelayUs(5);

    /* 随后在 vIICInit() 中将 PB6/PB7 重新配置为 I2C 复用开漏模式 */
}
```

调用位置：在 `vIICInit()` 的最前面，配置 I2C 外设之前调用。

```c
void vIICInit(void)
{
    vI2CBusRelease();   /* 先释放可能死锁的总线 */

    /* 然后正常初始化 I2C 外设 ... */
}
```

### 8.4 解决方法二：从机硬件下电

如果方法一无效（从机状态机彻底跑飞，不再响应时钟脉冲），只能通过硬件手段强制复位从机。

| 方案 | 实现方式 | 前提条件 |
| :--- | :--- | :--- |
| GPIO 控制从机电源 | 用一个 GPIO 控制从机的供电使能脚，拉低断电，延时后重新上电 | 硬件设计时预留电源控制 |
| 总线隔离芯片 | 使用带使能脚的电平转换芯片（如 TCA9406），关闭后从机断开总线 | 需要额外芯片 |

这要求在硬件设计阶段就考虑到 I2C 死锁的恢复需求，纯软件无法实现。

### 8.5 实际建议

| 场景 | 建议 |
| :--- | :--- |
| 常规项目 | `vIICInit()` 前加 9 脉冲恢复，基本够用 |
| 看门狗频繁复位的场景 | 必须加 9 脉冲恢复，否则复位后大概率死锁 |
| 高可靠性产品 | 硬件上预留从机电源控制，软硬件双保险 |
| 软件 I2C | 同样需要 9 脉冲恢复，死锁与硬件/软件 I2C 实现方式无关 |

---

## 9. 常见问题与调试 (Troubleshooting)

### Q1: HAL_I2C_Mem_Write 返回 HAL_ERROR

**可能原因：**
1. I2C 时钟未使能
2. GPIO 配置错误（不是复用开漏模式）
3. 外部上拉电阻缺失
4. 从机地址写错（应为 0xA0 而非 0x50）

### Q2: 写入数据后读回不一致

**可能原因：**
1. 页写入越界，数据发生回绕覆盖
2. 写入后未等待足够时间 (< 5ms)
3. 缺少互斥保护，多任务同时写入

### Q3: 在 RTOS 中通信偶发失败

**可能原因：**
1. 信号量未创建就使用 I2C
2. 中断优先级配置不当 (I2C 中断优先级必须 >= `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY`)

---

## 10. 版本变更记录 (Changelog)

| 版本 | 日期 | 变更内容 |
| :--- | :--- | :--- |
| v1.0 | 2026-02-26 | 初始版本：硬件 I2C + AT24C02 页对齐读写 + RTOS 信号量保护 |

---

**文档结束**
