# STM32F10x SPI Flash 设计文档 (DevicesSPI / DevicesSPIFlash)

| 属性 | 内容 |
| :--- | :--- |
| **模块名称** | DevicesSPI (SPI 总线驱动) + DevicesSPIFlash (Flash 应用层) |
| **源文件** | `DevicesSPI.c` / `DevicesSPI.h` / `DevicesSPIFlash.c` / `DevicesSPIFlash.h` |
| **硬件依赖** | SPI2 外设, PB12 (CS), PB13 (SCK), PB14 (MISO), PB15 (MOSI), W25Qxx Flash 芯片 |
| **软件依赖** | DevicesDelay, FreeRTOS (递归互斥信号量) |
| **版本** | v1.0 |
| **最后更新** | 2026-02-26 |

---

## 1. 设计目标 (Design Goals)

实现基于 STM32F1 硬件 SPI 外设的 W25Qxx Flash 读写驱动：

1. **SPI 总线驱动**：使用 SPI2 外设，Mode 3 (CPOL=1, CPHA=1)，软件片选
2. **Flash 存储管理**：扇区擦除 + 页对齐写入 + 连续读取
3. **RTOS 安全**：通过递归互斥信号量保护 SPI 总线访问，支持多任务环境
4. **分区管理**：Flash 空间按功能划分（Boot 备份、系统数据、用户数据、OTA 等）

---

## 2. W25Qxx Flash 基础 (Flash Fundamentals)

### 2.1 W25Qxx 概述

W25Qxx 是华邦 (Winbond) 生产的 SPI 接口 NOR Flash 存储芯片，常见型号：

| 型号 | 容量 | 地址范围 |
| :--- | :--- | :--- |
| W25Q16 | 2MB | 0x000000 ~ 0x1FFFFF |
| W25Q32 | 4MB | 0x000000 ~ 0x3FFFFF |
| W25Q64 | 8MB | 0x000000 ~ 0x7FFFFF |
| W25Q128 | 16MB | 0x000000 ~ 0xFFFFFF |

### 2.2 存储层级结构

W25Qxx 的存储空间分为三级：**块 (Block) → 扇区 (Sector) → 页 (Page)**。

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    W25Qxx 存储层级结构                                       │
└─────────────────────────────────────────────────────────────────────────────┘

  芯片 (Chip)
   │
   ├── 块 0 (Block 0) ─── 64KB
   │    ├── 扇区 0 (Sector 0) ─── 4KB
   │    │    ├── 页 0 (Page 0) ─── 256 字节
   │    │    ├── 页 1 (Page 1) ─── 256 字节
   │    │    ├── ...
   │    │    └── 页 15 (Page 15) ─── 256 字节
   │    ├── 扇区 1 (Sector 1) ─── 4KB
   │    ├── ...
   │    └── 扇区 15 (Sector 15) ─── 4KB
   │
   ├── 块 1 (Block 1) ─── 64KB
   │    └── ...
   └── ...

数值关系:
  1 页 (Page)     =  256 字节
  1 扇区 (Sector) =   16 页  =  4KB  (4096 字节)
  1 块 (Block)    =   16 扇区 = 64KB  (65536 字节)
```

### 2.3 Flash 三大操作约束

W25Qxx 作为 NOR Flash，有三个核心约束：

| 约束 | 说明 |
| :--- | :--- |
| **写前必须擦除** | Flash 只能把 1 写成 0，不能把 0 写成 1。必须先擦除（全部置 0xFF）才能写入新数据 |
| **擦除粒度为扇区** | 最小擦除单位是 4KB 扇区，不能只擦除某几个字节 |
| **写入粒度为页** | 单次写入最多 256 字节，且不能跨越页边界 |

```
Flash 写入原理 — 只能 1→0，不能 0→1:

  擦除后:     0xFF = 1111 1111
  写入 0xA5:           1010 0101  ← 正常，部分 1→0

  再次写入 0x5A:
    当前值:   0xA5 = 1010 0101
    目标值:   0x5A = 0101 1010
    实际结果: 0x00 = 0000 0000  ← 错误！0 无法变回 1
    
  必须先擦除 (置 0xFF)，再写入 0x5A 才能得到正确结果。
```

### 2.4 Flash vs EEPROM 对比

| | W25Qxx Flash | AT24C02 EEPROM |
| :--- | :--- | :--- |
| **写前是否需要擦除** | **需要** (扇区擦除 4KB) | **不需要**，直接覆盖 |
| **擦除粒度** | 4KB 扇区 / 64KB 块 / 全片 | 不适用 |
| **写入粒度** | 页写入 256 字节 | 页写入 8 字节 |
| **容量** | MB 级别 (2MB ~ 16MB) | KB 级别 (256 字节) |
| **擦写寿命** | ~10 万次 | ~100 万次 |
| **通信接口** | SPI (高速) | I2C (低速) |
| **典型用途** | 固件存储、OTA、日志 | 参数存储、配置保存 |

### 2.5 W25Qxx 命令集

本项目使用的命令：

| 命令 | 命令码 | 格式 | 功能 |
| :--- | :--- | :--- | :--- |
| **Read Data** | 0x03 | CMD + 3字节地址 + 读数据 | 读取数据 |
| **Page Program** | 0x02 | CMD + 3字节地址 + 写数据 | 页写入 (最多 256 字节) |
| **Sector Erase** | 0x20 | CMD + 3字节地址 | 扇区擦除 (4KB) |
| **Chip Erase** | 0xC7 | CMD | 全片擦除 |
| **Write Enable** | 0x06 | CMD | 写使能 (写入/擦除前必须调用) |
| **Write Disable** | 0x04 | CMD | 写禁止 |
| **Read Status Reg1** | 0x05 | CMD + 读状态 | 读状态寄存器1 (含 BUSY 位) |
| **Read Status Reg2** | 0x35 | CMD + 读状态 | 读状态寄存器2 |
| **Read Status Reg3** | 0x15 | CMD + 读状态 | 读状态寄存器3 |
| **Read ID** | 0x90 | CMD + 3字节 0x00 + 读2字节 | 读取厂商ID和设备ID |

### 2.6 状态寄存器

W25Qxx 有 3 个状态寄存器，本项目主要使用状态寄存器1：

```
状态寄存器1 (命令 0x05):

  bit7   bit6   bit5   bit4   bit3   bit2   bit1   bit0
┌──────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┐
│ SRP0 │ SEC  │  TB  │ BP2  │ BP1  │ BP0  │  WEL │ BUSY │
└──────┴──────┴──────┴──────┴──────┴──────┴──────┴──────┘
                                             ↑       ↑
                                          写使能   芯片忙

  BUSY (bit0): 1 = 正在编程/擦除, 0 = 空闲
  WEL  (bit1): 1 = 写使能已开启,  0 = 写禁止
  BP0~2:       块保护位，控制哪些区域禁止写入
```

**BUSY 位是最关键的**：每次写入或擦除后，芯片内部需要时间处理，此期间 BUSY=1，不接受新的写入/擦除命令。必须轮询等待 BUSY 清零后才能继续操作。

### 2.7 写使能机制

W25Qxx 的安全设计：**每次写入或擦除操作前，都必须先发送 Write Enable (0x06) 命令**。

```
写入/擦除的完整流程:

  ① 发送 Write Enable (0x06)     ← WEL 位被置 1
       ↓
  ② 发送写入/擦除命令 + 数据
       ↓
  ③ 芯片开始内部操作              ← BUSY 位被置 1, WEL 自动清零
       ↓
  ④ 轮询 BUSY 位，等待完成        ← BUSY 清零后可进行下一步
```

WEL 位在操作完成后**自动清零**，所以每次写入/擦除前都要重新使能。这是一种防误操作保护机制。

---

## 3. SPI 通信时序 — 命令交互流程 (Command Protocol)

### 3.1 SPI Flash 的"命令-响应"模型

虽然 SPI 硬件是全双工的，但 W25Qxx 的通信协议是**半双工的命令-响应模式**：

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    SPI Flash 命令交互模型                                     │
└─────────────────────────────────────────────────────────────────────────────┘

  CS:   ──┐                                                          ┌──
          └──────────────────────────────────────────────────────────┘

  阶段:    |←── 写命令阶段 ──►|←────── 数据阶段 ──────►|
  
           主机通过 MOSI 发送        根据命令不同:
           命令码 + 地址              - 读操作: 从机通过 MISO 返回数据
                                     - 写操作: 主机通过 MOSI 继续发数据

  从机在写命令阶段不返回有效数据 (MISO 无效)
  从机在收到完整命令后才知道要做什么
```

### 3.2 读数据时序 (Read Data, 0x03)

```
读取 N 字节数据:

  CS:   ──┐                                                          ┌──
          └──────────────────────────────────────────────────────────┘

  MOSI:    [0x03] [Addr23-16] [Addr15-8] [Addr7-0] [dummy] [dummy] ...
            命令     地址高       地址中      地址低    0xFF    0xFF
            
  MISO:    [xxxx] [xxxxxxxx] [xxxxxxxx] [xxxxxxxx] [Data0] [Data1] ...
            无效      无效        无效        无效    第1字节  第2字节

           |←──── 写命令+地址 (4字节) ────►|←── 读数据 (N字节) ──►|
           
  从机在收到命令+地址后，才开始在 MISO 上输出数据。
  主机此时在 MOSI 上发的 dummy 字节 (0xFF) 从机完全忽略，
  仅用于产生 SCLK 时钟驱动从机输出。
```

### 3.3 页写入时序 (Page Program, 0x02)

```
写入 N 字节数据 (N ≤ 256, 不跨页):

  CS:   ──┐                                                          ┌──
          └──────────────────────────────────────────────────────────┘

  MOSI:    [0x02] [Addr23-16] [Addr15-8] [Addr7-0] [Data0] [Data1] ...
            命令     地址高       地址中      地址低   第1字节  第2字节

  MISO:    [xxxx] [xxxxxxxx] [xxxxxxxx] [xxxxxxxx] [xxxxx] [xxxxx] ...
            无效      无效        无效        无效     无效    无效

           |←──── 写命令+地址 (4字节) ────►|←── 写数据 (N字节) ──►|

  整个过程 MISO 上的数据都是无效的，从机只接收 MOSI 上的数据。
  CS 拉高后，芯片开始内部编程，BUSY 置 1。
```

### 3.4 扇区擦除时序 (Sector Erase, 0x20)

```
擦除指定扇区 (4KB):

  CS:   ──┐                                    ┌──
          └────────────────────────────────────┘

  MOSI:    [0x20] [Addr23-16] [Addr15-8] [Addr7-0]
            命令     地址高       地址中      地址低

  CS 拉高后，芯片开始内部擦除，BUSY 置 1。
  擦除时间典型值约 45ms ~ 400ms。
```

---

## 4. 硬件连接 (Hardware Connection)

### 4.1 引脚分配

| MCU 引脚 | SPI 信号 | Flash 引脚 | 说明 |
| :--- | :--- | :--- | :--- |
| PB12 | CS (NSS) | #CS | 片选，软件控制，推挽输出 |
| PB13 | SCK | CLK | 时钟，复用推挽输出 |
| PB14 | MISO | DO | 数据输出（从机→主机），输入模式 |
| PB15 | MOSI | DI | 数据输入（主机→从机），复用推挽输出 |

### 4.2 典型电路

```
    STM32F103                          W25Qxx
    ┌──────────┐                    ┌──────────┐
    │    PB12  │── CS ─────────────┤ 1  #CS   │
    │   (推挽) │                    │          │
    │    PB14  │◄─ MISO ───────────┤ 2  DO    │
    │   (输入) │                    │          │
    │          │                    │ 3  #WP ──┤── VCC (不写保护)
    │          │                    │          │
    │          │                GND┤ 4  GND   │
    │          │                    │          │
    │    PB15  │── MOSI ──────────►┤ 5  DI    │
    │ (复用PP) │                    │          │
    │    PB13  │── SCK ───────────►┤ 6  CLK   │
    │ (复用PP) │                    │          │
    │          │                    │ 7  #HOLD ┤── VCC (不暂停)
    │          │                    │          │
    │          │                VCC┤ 8  VCC   │
    └──────────┘                    └──────────┘
    
    #WP 接 VCC  = 不启用写保护
    #HOLD 接 VCC = 不启用暂停功能
```

---

## 5. Flash 分区规划 (Partition Layout)

本项目将 Flash 空间按功能划分为多个分区：

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    SPI Flash 分区布局                                        │
└─────────────────────────────────────────────────────────────────────────────┘

  起始地址                    大小        用途
  ─────────────────────────────────────────────────
  0x000000                   16KB        Boot 备份区
  0x004000                    4KB        系统数据
  0x005000                    4KB        用户数据
  0x006000                   64KB        Bootloader 备份区
  0x016000                  424KB        APP 备份区
  0x07E000                    1MB        OTA 固件下载区
  0x17E000                   64KB        预留
  0x18E000                    2MB+       日志存储区
```

对应头文件中的宏定义：

```c
#define SPI_FLASH_BASE_ADDR             0x00000000ul
#define SPI_FLASH_BOOT_BACK_ADDR        (SPI_FLASH_BASE_ADDR + 1024 * (0))         /* 16KB */
#define SPI_FLASH_SYSTEM_DATA_ADDR      (SPI_FLASH_BASE_ADDR + 1024 * (16))        /*  4KB */
#define SPI_FLASH_USER_DATA_ADDR        (SPI_FLASH_BASE_ADDR + 1024 * (16 + 4))    /*  4KB */
#define SPI_FLASH_BOOTLOADER_BACK_ADDR  (SPI_FLASH_BASE_ADDR + 1024 * (16 + 4 + 4))         /* 64KB */
#define SPI_FLASH_APP_BACK_ADDR         (SPI_FLASH_BASE_ADDR + 1024 * (16 + 4 + 4 + 64))    /* 424KB */
#define SPI_FLASH_OTA_ADDR              (SPI_FLASH_BASE_ADDR + 1024 * (16 + 4 + 4 + 64 + 424))       /* 1MB */
#define SPI_FLASH_RESERVED_ADDR         (SPI_FLASH_BASE_ADDR + 1024 * (16 + 4 + 4 + 64 + 424 + 1024)) /* 64KB */
#define SPI_FLASH_LOG_ADDR              (SPI_FLASH_BASE_ADDR + 1024 * (16 + 4 + 4 + 64 + 424 + 1024 + 64)) /* 2MB+ */
```

---

## 6. 软件设计 (Software Design)

### 6.1 模块分层

```
┌─────────────────────────────────────────┐
│          应用层 (OTA / 日志 / 参数)       │  ← 调用 SPIFlash 读写接口
├─────────────────────────────────────────┤
│     DevicesSPIFlash.c / .h             │  ← 命令封装 + 页对齐 + 扇区擦除 + RTOS 保护
│     (Flash 应用层)                      │
├─────────────────────────────────────────┤
│     DevicesSPI.c / .h                  │  ← SPI 底层驱动 (HAL 硬件 SPI)
│     (SPI 总线驱动层)                    │
├─────────────────────────────────────────┤
│     STM32 HAL 库                        │  ← HAL_SPI_Transmit / TransmitReceive
├─────────────────────────────────────────┤
│     SPI2 硬件外设                       │  ← PB12(CS) PB13(SCK) PB14(MISO) PB15(MOSI)
└─────────────────────────────────────────┘
```

### 6.2 SPI 底层驱动 (DevicesSPI.c)

#### 6.2.1 SPI 初始化

```c
void vSPI2Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_SPI2_CLK_ENABLE();

    /* CS (PB12) — 推挽输出，软件控制 */
    GPIO_InitStruct.Pin = SPI2_CS_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(SPI2_CS_PORT, &GPIO_InitStruct);
    SET_SPI2_NSS_HIGH();

    /* SCK (PB13) + MOSI (PB15) — 复用推挽输出 */
    GPIO_InitStruct.Pin = SPI2_SCK_PIN | SPI2_MOSI_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(SPI2_SCK_PORT, &GPIO_InitStruct);

    /* MISO (PB14) — 浮空输入 */
    GPIO_InitStruct.Pin = SPI2_MISO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(SPI2_MISO_PORT, &GPIO_InitStruct);

    /* SPI2 参数配置 */
    spi2_init_struct.Instance = SPI2;
    spi2_init_struct.Init.Mode = SPI_MODE_MASTER;
    spi2_init_struct.Init.Direction = SPI_DIRECTION_2LINES;
    spi2_init_struct.Init.DataSize = SPI_DATASIZE_8BIT;
    spi2_init_struct.Init.CLKPolarity = SPI_POLARITY_HIGH;
    spi2_init_struct.Init.CLKPhase = SPI_PHASE_2EDGE;
    spi2_init_struct.Init.NSS = SPI_NSS_SOFT;
    spi2_init_struct.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
    spi2_init_struct.Init.FirstBit = SPI_FIRSTBIT_MSB;
    spi2_init_struct.Init.TIMode = SPI_TIMODE_DISABLE;
    spi2_init_struct.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    spi2_init_struct.Init.CRCPolynomial = 10;

    HAL_SPI_Init(&spi2_init_struct);
}
```

**配置项详解：**

| 配置项 | 值 | 含义 |
| :--- | :--- | :--- |
| CS GPIO Mode | `GPIO_MODE_OUTPUT_PP` | 推挽输出，软件控制片选 |
| SCK/MOSI GPIO Mode | `GPIO_MODE_AF_PP` | 复用推挽，SPI 外设控制 |
| MISO GPIO Mode | `GPIO_MODE_INPUT` | 浮空输入，接收从机数据 |
| Mode | `SPI_MODE_MASTER` | 主机模式 |
| Direction | `SPI_DIRECTION_2LINES` | 全双工 (MOSI + MISO) |
| CLKPolarity | `SPI_POLARITY_HIGH` | CPOL=1，空闲时 SCK 为高电平 |
| CLKPhase | `SPI_PHASE_2EDGE` | CPHA=1，第二个边沿采样 → **Mode 3** |
| NSS | `SPI_NSS_SOFT` | 软件片选，CS 由 GPIO 控制 |
| BaudRatePrescaler | `SPI_BAUDRATEPRESCALER_16` | 16 分频，SPI2 挂在 APB1 (36MHz)，SCLK = 2.25MHz |
| FirstBit | `SPI_FIRSTBIT_MSB` | 高位先发 |

#### 6.2.2 SPI 底层读写函数

SPI 底层提供三个函数，对应三种使用场景：

```c
/* 单字节同时收发 — 用于发命令、读状态等 */
uint8_t ucSPIWriteReadByte(SPI_HandleTypeDef *hspi, uint8_t ucByte)
{
    uint8_t ucReadByte = 0;
    HAL_SPI_TransmitReceive(hspi, &ucByte, &ucReadByte, 1, HAL_MAX_DELAY);
    return ucReadByte;
}

/* 纯发送多字节 — 用于发送命令+地址、写入数据 */
int8_t cSPIWriteDatas(SPI_HandleTypeDef *hspi, void *pvBuff, int32_t iLength)
{
    if ((pvBuff == NULL) || (iLength < 1))
        return 1;
    if (HAL_SPI_Transmit(hspi, (uint8_t *)pvBuff, iLength, HAL_MAX_DELAY) != HAL_OK)
        return 2;
    return 0;
}

/* 读取多字节 — 发送 dummy 数据产生时钟，同时接收 MISO 数据 */
int8_t cSPIReadDatas(SPI_HandleTypeDef *hspi, void *pvBuff, int32_t iLength)
{
    if ((pvBuff == NULL) || (iLength < 1))
        return 1;
    if (HAL_SPI_TransmitReceive(hspi, (uint8_t *)pvBuff, (uint8_t *)pvBuff, iLength, HAL_MAX_DELAY) != HAL_OK)
        return 2;
    return 0;
}
```

**`cSPIReadDatas` 的巧妙设计**：

发送缓冲区和接收缓冲区指向同一个 `pvBuff`。底层先把 `pvBuff` 中的残值通过 MOSI 发出（从机忽略），同时从 MISO 收到的真实数据写回 `pvBuff`，覆盖掉原来的值。省去了额外的 dummy 发送数组，节省内存。

```
执行过程 (以读 3 字节为例):

  pvBuff 初始: [0x??, 0x??, 0x??]   ← 内存残值

  第1字节: MOSI 发出 0x?? (从机忽略) → MISO 收到 D0 → pvBuff[0] = D0
  第2字节: MOSI 发出 0x?? (从机忽略) → MISO 收到 D1 → pvBuff[1] = D1
  第3字节: MOSI 发出 0x?? (从机忽略) → MISO 收到 D2 → pvBuff[2] = D2

  pvBuff 结果: [D0, D1, D2]         ← 真实数据
```

#### 6.2.3 SPI 底层接口适配宏

头文件通过宏将通用函数绑定到 SPI2 实例，方便调用：

```c
#define ucSPI2WriteReadByte(ucByte)         ucSPIWriteReadByte(&spi2_init_struct, (ucByte))
#define cSPI2WriteDatas(pBuffer, iLength)   cSPIWriteDatas(&spi2_init_struct, (pBuffer), (iLength))
#define cSPI2ReadDatas(pBuffer, iLength)    cSPIReadDatas(&spi2_init_struct, (pBuffer), (iLength))
```

Flash 层通过再次宏映射，将具体 SPI 实例与 Flash 操作解耦：

```c
#define SPI_FLASH_CS_ENABLE     SET_SPI2_NSS_LOW
#define SPI_FLASH_CS_DISABLE    SET_SPI2_NSS_HIGH
#define ucSPIxWriteReadByte     ucSPI2WriteReadByte
#define cSPIxWriteDatas         cSPI2WriteDatas
#define cSPIxReadDatas          cSPI2ReadDatas
```

> 如果将来 Flash 换到 SPI1 或 SPI3 上，只需要修改这几个宏即可，Flash 层代码不用动。

### 6.3 Flash 应用层 (DevicesSPIFlash.c)

#### 6.3.1 初始化 — 读取芯片 ID 验证连接

```c
void vSPIFlashInit(void)
{
    uint32_t uiTimes;
    vSPI2Init();

    SPI_FLASH_CS_ENABLE();

    for(uiTimes = 0; uiTimes < 8; ++uiTimes)
    {
        st_usSPIFlashID = uiSPIFlashReadID();

        if((st_usSPIFlashID != 0x0000) && (st_usSPIFlashID != 0xFFFF))
            break;

        vDelayMs(10);
    }

    SPI_FLASH_CS_DISABLE();
}
```

初始化时尝试最多 8 次读取芯片 ID，ID 为 0x0000 或 0xFFFF 说明芯片未正确连接。常见 W25Qxx 芯片 ID：

| 芯片 | 厂商 ID | 设备 ID | 组合 |
| :--- | :--- | :--- | :--- |
| W25Q16 | 0xEF | 0x14 | 0xEF14 |
| W25Q32 | 0xEF | 0x15 | 0xEF15 |
| W25Q64 | 0xEF | 0x16 | 0xEF16 |
| W25Q128 | 0xEF | 0x17 | 0xEF17 |

#### 6.3.2 读取芯片 ID

```c
uint32_t uiSPIFlashReadID(void)
{
    uint16_t usID = 0xFFFF;
    uint8_t ucDatas[4] = {READ_ID_CMD, 0x00, 0x00, 0x00};

    cSPIFlashLock();
    SPI_FLASH_CS_ENABLE();

    cSPIxWriteDatas(ucDatas, 4);                /* 发送命令 0x90 + 3字节地址 0x000000 */
    usID = ucSPIxWriteReadByte(0xFF) << 8;      /* 读取厂商 ID */
    usID |= ucSPIxWriteReadByte(0xFF);          /* 读取设备 ID */

    SPI_FLASH_CS_DISABLE();
    cSPIFlashUnlock();

    return usID;
}
```

```
Read ID 时序:

  CS:   ──┐                                                    ┌──
          └────────────────────────────────────────────────────┘

  MOSI:    [0x90] [0x00] [0x00] [0x00] [0xFF]       [0xFF]
            命令    地址    地址    地址   dummy        dummy

  MISO:    [xxxx] [xxxx] [xxxx] [xxxx] [厂商ID 0xEF] [设备ID 0x17]
```

#### 6.3.3 轮询 BUSY 状态

```c
static uint8_t ucSPIFlashReadStatus(uint8_t ucRegNo)
{
    uint32_t uiTimeout = 20000;
    uint8_t ucReadStatusCMD;
    uint8_t ucStatus;

    switch(ucRegNo)
    {
        case 1 : ucReadStatusCMD = READ_STATUS_REG1_CMD; break;
        case 2 : ucReadStatusCMD = READ_STATUS_REG2_CMD; break;
        case 3 : ucReadStatusCMD = READ_STATUS_REG3_CMD; break;
        default : ucReadStatusCMD = READ_STATUS_REG1_CMD; break;
    }

    if((st_usSPIFlashID == 0x0000) || (st_usSPIFlashID == 0xFFFF))
        return 0xFF;

    SPI_FLASH_CS_ENABLE();
    ucSPIxWriteReadByte(ucReadStatusCMD);

    while(uiTimeout--)
    {
        ucStatus = ucSPIxWriteReadByte(0xFF);
        if((ucStatus & 0x01) != 0x01)       /* BUSY 位清零 → 芯片空闲 */
            break;
        vDelayUs(10);
    }

    SPI_FLASH_CS_DISABLE();
    return ucStatus;
}
```

**设计要点：**
- 先检查 `st_usSPIFlashID` 是否有效，避免在 Flash 未连接时死等
- 超时保护：最多等待 20000 × 10μs = 200ms，防止卡死
- 在同一次 CS 拉低周期内持续读状态，无需反复发命令

#### 6.3.4 写使能

```c
static void uvSPIFlashEnableWrite(void)
{
    SPI_FLASH_CS_ENABLE();
    ucSPIxWriteReadByte(WRITE_ENABLE_CMD);
    SPI_FLASH_CS_DISABLE();
}
```

每次写入或擦除前必须调用，操作完成后 WEL 位自动清零。

#### 6.3.5 扇区擦除

```c
int8_t cSPIFlashErases(uint32_t uiAddress)
{
    uint8_t ucAddress[3] = {uiAddress >> 16, uiAddress >> 8, uiAddress};
    int8_t cError = 0;

    cSPIFlashLock();

    if(ucSPIStatusBusy()) { cError = 1; goto __EXIT; }

    uvSPIFlashEnableWrite();

    if(ucSPIStatusBusy()) { cError = 2; goto __EXIT; }

    SPI_FLASH_CS_ENABLE();
    ucSPIxWriteReadByte(SUBSECTOR_ERASE_CMD);   /* 发送扇区擦除命令 0x20 */
    cError |= cSPIxWriteDatas(ucAddress, 3);     /* 发送 3 字节地址 */
    SPI_FLASH_CS_DISABLE();

__EXIT:
    cSPIFlashUnlock();
    return cError;
}
```

**流程：等待空闲 → 写使能 → 等待空闲 → 发送擦除命令+地址。**

#### 6.3.6 页写入 (内部函数)

```c
static int8_t cSPIFlashWritePage(uint32_t uiAddress, uint8_t *pucDatas, int32_t iLength)
{
    uint8_t ucAddress[3] = {uiAddress >> 16, uiAddress >> 8, uiAddress};
    int8_t cError = 0;

    if(ucSPIStatusBusy()) return 1;

    uvSPIFlashEnableWrite();

    if(ucSPIStatusBusy()) return 2;

    SPI_FLASH_CS_ENABLE();
    ucSPIxWriteReadByte(PAGE_PROG_CMD);          /* 发送页写入命令 0x02 */
    cError |= cSPIxWriteDatas(ucAddress, 3);      /* 发送 3 字节地址 */
    cError |= cSPIxWriteDatas(pucDatas, iLength); /* 发送数据 */
    SPI_FLASH_CS_DISABLE();

    return cError;
}
```

**约束：iLength ≤ 256，且不能跨越页边界。** 这个约束由上层 `cSPIFlashWriteDatas` 保证。

#### 6.3.7 带擦除的多字节写入 — 核心函数

```c
int8_t cSPIFlashWriteDatas(uint32_t uiAddress, const void *pvBuff, int32_t iLength)
{
    int32_t iLengthTemp = 0;
    uint8_t *pucDatas = (uint8_t *)pvBuff;
    int8_t cError = 0;

    cSPIFlashLock();

    while(iLength > 0)
    {
        /* 扇区起始地址时，需要先擦除该扇区 */
        if((uiAddress % SPI_FLASH_SECTOR_SIZE) == 0)
        {
            if(cSPIFlashErases(uiAddress) != 0)
            {
                cError |= 1;
                break;
            }
        }

        /* 页对齐: 本次写入量 = min(剩余量, 当前页剩余空间) */
        iLengthTemp = (iLength > (SPI_FLASH_PAGE_SIZE - (uiAddress % SPI_FLASH_PAGE_SIZE)))
                    ? (SPI_FLASH_PAGE_SIZE - (uiAddress % SPI_FLASH_PAGE_SIZE))
                    : iLength;

        if(cSPIFlashWritePage(uiAddress, pucDatas, iLengthTemp) != 0)
        {
            cError |= 2;
            break;
        }

        uiAddress += iLengthTemp;
        iLength -= iLengthTemp;
        pucDatas += iLengthTemp;
    }

    cSPIFlashUnlock();
    return cError;
}
```

**此函数自动处理两件事：**

**1. 扇区自动擦除**：当地址落在扇区起始位置（4096 对齐）时，自动擦除该扇区。

```
写入 512 字节，从地址 0x0F00 开始 (跨扇区边界):

第1轮: uiAddress=0x0F00, 0x0F00 % 4096 = 3840 ≠ 0 → 不擦除
       页对齐: 写 256 字节
       
第2轮: uiAddress=0x1000, 0x1000 % 4096 = 0 → 触发擦除 ✓
       页对齐: 写 256 字节
```

**2. 页对齐写入**：确保每次写入不跨越 256 字节页边界。

```
页对齐计算: min(剩余数据量, 当前页剩余空间)

当前页剩余空间 = 256 - (uiAddress % 256)

示例: 从 0x0080 写 300 字节
  第1次: 页剩余 = 256 - 128 = 128, 写 128 字节
  第2次: 页剩余 = 256 - 0 = 256,   写 172 字节 (剩余全部)
```

> **前提条件**：调用者需从扇区起始地址开始写，或确保目标区域已被擦除。如果从非扇区起始地址开始写入一个未擦除的区域，由于 Flash "只能 1→0" 的特性，数据会出错。

#### 6.3.8 多字节读取

```c
int8_t cSPIFlashReadDatas(uint32_t uiAddress, void *pvBuff, int32_t iLength)
{
    uint8_t ucAddress[3] = {uiAddress >> 16, uiAddress >> 8, uiAddress};
    int8_t cError;

    cSPIFlashLock();

    if(ucSPIStatusBusy()) { cError = 1; goto __EXIT; }

    SPI_FLASH_CS_ENABLE();
    ucSPIxWriteReadByte(READ_CMD);               /* 发送读命令 0x03 */
    cError = cSPIxWriteDatas(ucAddress, 3);       /* 发送 3 字节地址 */
    cError |= cSPIxReadDatas(pvBuff, iLength);    /* 读取数据 */
    SPI_FLASH_CS_DISABLE();

__EXIT:
    cSPIFlashUnlock();
    return cError;
}
```

> 读取操作**不需要页对齐**，W25Qxx 支持跨页、跨扇区、跨块连续读取，内部地址自动递增，直到 CS 拉高。

#### 6.3.9 全片擦除

```c
int8_t cSPIFlashErasesChip()
{
    int8_t cError = 0;

    cSPIFlashLock();

    if(ucSPIStatusBusy()) { cError = 1; goto __EXIT; }
    uvSPIFlashEnableWrite();
    if(ucSPIStatusBusy()) { cError = 2; goto __EXIT; }

    SPI_FLASH_CS_ENABLE();
    ucSPIxWriteReadByte(SUBCHIP_ERASE_CMD);      /* 发送全片擦除命令 0xC7 */
    SPI_FLASH_CS_DISABLE();

    while(ucSPIStatusBusy())                      /* 等待擦除完成 */
        vDelayMs(10);

__EXIT:
    cSPIFlashUnlock();
    return cError;
}
```

> 全片擦除耗时较长（W25Q128 典型 40s ~ 200s），函数内部以 10ms 间隔轮询 BUSY 直到完成。

### 6.4 RTOS 互斥保护

所有 Flash 读写操作使用 **FreeRTOS 递归互斥信号量** 保护，防止多任务同时访问 SPI 总线：

```c
static int8_t cSPIFlashLock(void)
{
    xSemaphoreTakeRecursive(g_xSpiFlashSemaphore, portMAX_DELAY);
    return 0;
}

static int8_t cSPIFlashUnlock(void)
{
    xSemaphoreGiveRecursive(g_xSpiFlashSemaphore);
    return 0;
}
```

使用递归互斥的原因：`cSPIFlashWriteDatas` 内部会调用 `cSPIFlashErases`，而两者都需要加锁，递归互斥允许同一任务多次获取锁而不死锁。

---

## 7. API 总览 (API Reference)

### 7.1 SPI 底层接口 (DevicesSPI.h)

| 函数 / 宏 | 功能 | 返回值 |
| :--- | :--- | :--- |
| `vSPI2Init()` | 初始化 SPI2 外设和 GPIO | 无 |
| `ucSPI2WriteReadByte(ucByte)` | 单字节同时收发 | 收到的字节 |
| `cSPI2WriteDatas(pBuf, len)` | 发送多字节 | 0: 成功 |
| `cSPI2ReadDatas(pBuf, len)` | 读取多字节 | 0: 成功 |

### 7.2 Flash 应用接口 (DevicesSPIFlash.h)

| 函数 | 功能 | 返回值 |
| :--- | :--- | :--- |
| `vSPIFlashInit()` | 初始化 SPI + 读取芯片 ID | 无 |
| `uiSPIFlashReadID()` | 读取芯片厂商和设备 ID | 16位 ID |
| `cSPIFlashReadDatas(addr, *pBuf, len)` | 读取任意长度 | 0: 成功 |
| `cSPIFlashWriteDatas(addr, *pBuf, len)` | 写入任意长度 (自动擦除+页对齐) | 0: 成功 |
| `cSPIFlashErases(addr)` | 擦除指定扇区 (4KB) | 0: 成功 |
| `cSPIFlashErasesChip()` | 擦除整个芯片 | 0: 成功 |

### 7.3 宏定义

```c
/* 存储参数 */
#define SPI_FLASH_SECTOR_SIZE        4096    /* 扇区大小 */
#define SPI_FLASH_PAGE_SIZE          256     /* 页大小 */

/* SPI 接口适配 */
#define SPI_FLASH_CS_ENABLE          SET_SPI2_NSS_LOW
#define SPI_FLASH_CS_DISABLE         SET_SPI2_NSS_HIGH
#define ucSPIxWriteReadByte          ucSPI2WriteReadByte
#define cSPIxWriteDatas              cSPI2WriteDatas
#define cSPIxReadDatas               cSPI2ReadDatas
```

---

## 8. 使用示例 (Usage Examples)

### 8.1 初始化

```c
#include "DevicesSPIFlash.h"

void vSystemPeripheralInit(void)
{
    vSPIFlashInit();    /* 初始化 SPI2 + 读取 Flash ID */
}
```

### 8.2 写入数据

```c
uint8_t writeData[300] = { /* ... */ };

/* 从地址 0x1000 写入 300 字节 (扇区对齐起始地址) */
/* 函数内部自动擦除 + 页对齐 */
int8_t ret = cSPIFlashWriteDatas(0x1000, writeData, 300);
if(ret != 0)
{
    /* 写入失败处理 */
}
```

### 8.3 读取数据

```c
uint8_t readData[300] = {0};

/* 从地址 0x1000 读取 300 字节 */
int8_t ret = cSPIFlashReadDatas(0x1000, readData, 300);
if(ret != 0)
{
    /* 读取失败处理 */
}
```

### 8.4 擦除扇区

```c
/* 擦除地址 0x2000 所在的 4KB 扇区 */
int8_t ret = cSPIFlashErases(0x2000);
```

---

## 9. 常见问题与调试 (Troubleshooting)

### Q1: 读取 ID 始终为 0xFFFF 或 0x0000

**可能原因：**
1. SPI 模式 (CPOL/CPHA) 与 Flash 芯片不匹配
2. CS 引脚未正确拉低
3. Flash 芯片未焊接好或未上电
4. MISO/MOSI 接反

**解决方案：**
1. W25Qxx 支持 Mode 0 和 Mode 3，确认配置正确
2. 用示波器检查 CS、SCK、MOSI 波形
3. 检查焊接和供电

### Q2: 写入后读回数据不一致

**可能原因：**
1. 写入前未擦除（Flash 只能 1→0）
2. 从非扇区起始地址开始调用 `cSPIFlashWriteDatas`
3. 页写入越界，数据发生回绕

**解决方案：**
1. 确保从扇区对齐的地址开始写入
2. 使用 `cSPIFlashWriteDatas`（自动擦除+页对齐），不要直接调用 `cSPIFlashWritePage`

### Q3: 写入/擦除后函数超时返回错误

**可能原因：**
1. 前一次操作的 BUSY 状态未清除
2. Flash 芯片内部异常

**解决方案：**
1. 检查超时参数是否足够（扇区擦除典型 45ms，最大 400ms）
2. 尝试全片擦除后重新操作

### Q4: OTA 升级过程中通信失败

**可能原因：**
1. SPI 分频系数过小，SCLK 波形失真（参考 DevicesSPI 设计文档中的实战案例）
2. 大量连续写入累积错误

**解决方案：**
1. 增大分频系数，确保 SCLK 波形质量
2. 写入后回读校验，确保数据正确

---

## 10. 版本变更记录 (Changelog)

| 版本 | 日期 | 变更内容 |
| :--- | :--- | :--- |
| v1.0 | 2026-02-26 | 初始版本：SPI Flash 驱动设计，含命令协议、页对齐写入、扇区擦除、RTOS 保护、分区规划 |

---

**文档结束**
