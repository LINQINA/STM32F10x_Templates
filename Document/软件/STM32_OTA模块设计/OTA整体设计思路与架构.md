# OTA 整体设计思路与架构文档

| 属性 | 内容 |
| :--- | :--- |
| **模块名称** | OTA 远程固件升级模块 |
| **源文件** | `DriverOTA.c/h`、`DriverBootloader.c/h`、`DriverUpgradeXxx.c/h`、`taskOTA.c/h` |
| **硬件依赖** | 内部 Flash、外部 SPI Flash (W25Qxx) |
| **软件依赖** | DevicesFlash、DevicesSPIFlash、DevicesCRC、DevicesModbus、FreeRTOS |
| **版本** | v1.0 |
| **最后更新** | 2026-02-26 |

---

## 1. 概览

### 1.1 OTA 解决什么问题

产品出货后需要修复 Bug 或迭代功能，不可能每次都拆机烧录。OTA (Over-The-Air) 模块提供**远程/本地固件升级**能力：

- **本机升级**：通过上位机 (Modbus) 或 IOT (手机 APP / 涂鸦) 将固件包写入外部 SPI Flash，再由 Bootloader 搬运至内部 Flash 完成升级。
- **多子模块升级**：一次 OTA 固件包可同时包含多个子设备的固件（EMS 本机、BMS、逆变器等），由主控按类型分发。

### 1.2 核心设计目标

| 目标 | 说明 |
| :--- | :--- |
| **断电续升** | OTA 状态持久化到内部 Flash，断电重启后自动恢复升级流程 |
| **多子固件** | 单个固件包最多包含 8 个子固件，按 type 路由到对应升级驱动 |
| **数据校验** | 总头 CRC、整包 CRC、子固件 CRC 三级校验，确保固件完整性 |
| **失败重试** | 每个子固件独立的重传计数器，升级失败自动重试 |
| **可扩展** | 新增子设备只需实现升级驱动并注册到 switch-case，框架代码无需改动 |

### 1.3 代码文件清单

| 文件 | 层级 | 职责 |
| :--- | :--- | :--- |
| `Modules/OTA/DriverOTA.c/h` | 模块层 | OTA 核心调度：状态机、固件校验、子固件分发 |
| `Modules/Bootloader/DriverBootloader.c/h` | 模块层 | 固件分区管理、搬运、跳转 |
| `Modules/OTA/DriverUpgradePD.c/h` | 模块层 | 本机 (PD) 升级驱动 |
| `Modules/OTA/DriverUpgradeBMS.c/h` | 模块层 | BMS 升级驱动 (参考项目) |
| `Modules/OTA/DriverUpgradeInv.c/h` | 模块层 | 逆变器升级驱动 (参考项目) |
| `APP/taskOTA.c/h` | 应用层 | FreeRTOS OTA 任务，驱动状态机流转 |
| `BSP/DevicesFlash.c/h` | BSP 层 | 内部 Flash 读写 |
| `BSP/DevicesSPIFlash.c/h` | BSP 层 | 外部 SPI Flash 读写 |
| `BSP/DevicesCRC.c/h` | BSP 层 | CRC16-Modbus / CRC32 校验 |

---

## 2. 系统架构总览

### 2.1 三层架构

```
┌─────────────────────────────────────────────────────────────────┐
│                        固件接收层                                │
│  ┌──────────────────────┐    ┌──────────────────────────────┐   │
│  │  上位机 (Modbus RTU) │    │  IOT (涂鸦/手机 APP)         │   │
│  │  cOTAModbusPackAnalysis   │  cOTAIOTFrimwareDataRecever  │   │
│  └──────────┬───────────┘    └──────────────┬───────────────┘   │
│             │ 分帧写入                       │ 流式写入          │
│             └──────────────┬────────────────┘                   │
│                            ▼                                    │
│               外部 SPI Flash OTA 数据区                          │
└────────────────────────────┬────────────────────────────────────┘
                             │ 触发 OTA_STATE_START
┌────────────────────────────▼────────────────────────────────────┐
│                       OTA 调度层                                 │
│                     (DriverOTA.c)                                │
│                                                                 │
│   vOTAInit() → vOTAStart() → vOTAFirmwareUpdateAll()           │
│                                                                 │
│   ┌──────────────────────────────────────────────────────────┐  │
│   │              双状态机（本机 + 子模块）                      │  │
│   │  本机状态：DISABLE→START→READY→UPDATING→SUCCESS/FAIL     │  │
│   │  子模块状态：各自独立的 READY→UPDATING→SUCCESS/FAIL       │  │
│   └──────────────────────────────────────────────────────────┘  │
│                                                                 │
│   按 type 分发到子模块升级驱动 (cOTAFirmwareUpdatePort)          │
└───┬──────────┬──────────┬───────────┬────────────┬──────────────┘
    │          │          │           │            │
    ▼          ▼          ▼           ▼            ▼
┌────────┐┌────────┐┌─────────┐┌──────────┐┌───────────┐
│ PD本机  ││  BMS   ││   INV   │
│ Upgrade ││Upgrade ││ Upgrade │
└────────┘└────────┘└─────────┘
    │          │          │
    │          └──────────┘
    │                     │
    │                通讯分包下发到外部子设备
    │
    ▼
  SPI Flash 备份区 → Bootloader 搬运 → 内部 Flash APP 区
```

### 2.2 双状态机设计

OTA 系统的核心是**两级状态机**：本机状态机管控整体升级流程，子模块状态机管理每个子固件的独立升级。

#### 本机状态机（OTAInfoType.state）

```
                 接收完成 / CRC不匹配恢复
                         │
     ┌───────────────────▼───────────────────┐
     │              OTA_STATE_START            │
     │   总头CRC校验 + 整包CRC校验 + 初始化     │
     └──────────────┬────────────┬────────────┘
                    │ 校验通过    │ 校验失败
                    ▼            ▼
     ┌──────────────────┐  ┌──────────────────┐
     │ OTA_STATE_UPDATING│  │  OTA_STATE_FAIL  │
     │ 依次分发子固件升级  │  │  记录错误码       │
     └────────┬─────────┘  └──────┬───────────┘
              │ 全部成功           │
              ▼                   │
     ┌──────────────────┐        │
     │ OTA_STATE_SUCCESS │        │
     │  版本校验/复位     │        │
     └────────┬─────────┘        │
              │                   │
              ▼                   ▼
     ┌──────────────────────────────────────┐
     │           OTA_STATE_DISABLE           │
     │          回到空闲态/系统复位            │
     └──────────────────────────────────────┘
```

#### 子模块状态机（OTAFirmwarePortInfoType.state）

每个子固件拥有独立的状态，互不影响：

```
  OTA_STATE_READY        ← vOTADeinit() 初始化后进入
       │
       ▼ ReSendCount > 0
  OTA_STATE_UPDATING     ← ReSendCount-- 并持久化
       │
       ├── 升级成功 ──→ OTA_STATE_SUCCESS (ReSendCount 清零)
       │
       └── 升级失败 ──→ ReSendCount > 0 ? → 重新进入 UPDATING
                        ReSendCount == 0 ? → OTA_STATE_FAIL (记录 error)
```

#### 双状态机关系

```
本机状态机                              子模块状态机
═══════════                            ════════════
START                                  
  │ vOTAStart()                        
  │   校验 + vOTADeinit()  ──────────→  Port[0..N].state = READY
  ▼                                    
UPDATING                               
  │ vOTAFirmwareUpdateAll()            
  │   for i = 0..N:                    
  │     ├── Port[i] == READY ────────→  Port[i] = UPDATING
  │     │   cOTAFirmwareUpdatePort()   
  │     │     ├── 成功 ──────────────→  Port[i] = SUCCESS
  │     │     └── 失败+重试耗尽 ─────→  Port[i] = FAIL
  │     ...                            
  │   遍历完成后:                       
  │     全部 SUCCESS ? ────────────────  → 本机 = SUCCESS
  │     存在 FAIL ?   ────────────────  → 本机 = FAIL
  ▼                                    
SUCCESS / FAIL                         
  │                                    
  ▼                                    
DISABLE                                
```

---

## 3. Flash 存储分区设计

OTA 的存储基础是**内部 Flash + 外部 SPI Flash 的双区协作**。内部 Flash 存放运行时代码和关键参数，外部 SPI Flash 作为大容量缓冲区存储 OTA 固件包和备份固件。

### 3.1 内部 Flash 分区表

基于 `BSP/DevicesFlash.h`，内部 Flash 基地址 `0x08000000`，总大小 512KB：

```
内部 Flash 地址空间 (0x08000000 起)
┌────────────────────────────────────────────────┐
│ Boot (32KB)                                    │ 0x08000000
│ 引导程序，负责跳转和固件搬运                       │
├────────────────────────────────────────────────┤
│ SystemData (2KB)                               │ 0x08008000
│ FirmwareInfoType：各分区的地址/长度/CRC/版本       │
├────────────────────────────────────────────────┤
│ OTA Data (2KB)                                 │ 0x08008800
│ OTAInfoType：OTA 运行时状态，断电恢复依据          │
├────────────────────────────────────────────────┤
│ OTP Data (2KB)                                 │ 0x08009000
│ 一次性可编程数据                                  │
├────────────────────────────────────────────────┤
│ User Data (2KB)                                │ 0x08009800
│ 用户配置参数                                     │
├────────────────────────────────────────────────┤
│ Bootloader (64KB)                              │ 0x0800A000
│ Bootloader 程序，负责 APP 区搬运和校验             │
├────────────────────────────────────────────────┤
│ APP (剩余空间 ≈ 408KB)                          │ 0x0801A000
│ 应用程序 (FreeRTOS + 业务代码)                    │
└────────────────────────────────────────────────┘
```

| 区域 | 起始偏移 | 大小 | 宏定义 | 用途 |
| :--- | :--- | :--- | :--- | :--- |
| Boot | +0K | 32KB | `FLASH_BOOT_ADDR` | 上电引导，决定跳转目标 |
| SystemData | +32K | 2KB | `FLASH_SYSTEM_DATA_ADDR` | 存储 `FirmwareInfoType`（6 个分区的固件参数） |
| OTA Data | +34K | 2KB | `FLASH_OTA_DATA_ADDR` | 存储 `OTAInfoType`（OTA 运行状态 + 子固件状态） |
| OTP Data | +36K | 2KB | `FLASH_OTP_DATA_ADDR` | OTP 数据 |
| User Data | +38K | 2KB | `FLASH_USER_DATA_ADDR` | 用户参数 |
| Bootloader | +40K | 64KB | `FLASH_BOOTLOADER_ADDR` | Bootloader 程序代码 |
| APP | +104K | 剩余 | `FLASH_APP_ADDR` | 应用程序代码 |

### 3.2 外部 SPI Flash 分区表

基于 `BSP/DevicesSPIFlash.h`，外部 SPI Flash 基地址 `0x00000000`：

```
外部 SPI Flash 地址空间 (0x00000000 起)
┌────────────────────────────────────────────────┐
│ Boot 备份 (16KB)                               │ 0x00000000
│ Boot 固件的外部备份，用于 Bootloader 搬运          │
├────────────────────────────────────────────────┤
│ SystemData (4KB)                               │ 0x00004000
│ 系统参数备份                                     │
├────────────────────────────────────────────────┤
│ UserData (4KB)                                 │ 0x00005000
│ 用户数据备份                                     │
├────────────────────────────────────────────────┤
│ Bootloader 备份 (64KB)                         │ 0x00006000
│ Bootloader 固件备份                              │
├────────────────────────────────────────────────┤
│ APP 备份 (424KB)                               │ 0x00016000
│ APP 固件备份，OTA 本机升级的中转站                  │
├────────────────────────────────────────────────┤
│ OTA 数据区 (1MB)                               │ 0x00080000
│ 上位机/IOT 写入的完整 OTA 固件包                   │
├────────────────────────────────────────────────┤
│ 预留 (64KB)                                    │ 0x00180000
├────────────────────────────────────────────────┤
│ Log (剩余空间)                                  │ 0x00190000
│ 运行日志存储                                     │
└────────────────────────────────────────────────┘
```

| 区域 | 起始偏移 | 大小 | 宏定义 | 用途 |
| :--- | :--- | :--- | :--- | :--- |
| Boot 备份 | +0K | 16KB | `SPI_FLASH_BOOT_BACK_ADDR` | Boot 固件备份 |
| SystemData | +16K | 4KB | `SPI_FLASH_SYSTEM_DATA_ADDR` | 系统参数备份 |
| UserData | +20K | 4KB | `SPI_FLASH_USER_DATA_ADDR` | 用户数据备份 |
| Bootloader 备份 | +24K | 64KB | `SPI_FLASH_BOOTLOADER_BACK_ADDR` | Bootloader 固件备份 |
| APP 备份 | +88K | 424KB | `SPI_FLASH_APP_BACK_ADDR` | APP 固件备份（中转站） |
| OTA 数据区 | +512K | 1MB | `SPI_FLASH_OTA_ADDR` | 完整 OTA 固件包接收区 |
| 预留 | +1536K | 64KB | `SPI_FLASH_RESERVED_ADDR` | 预留 |
| Log | +1600K | 剩余 | `SPI_FLASH_LOG_ADDR` | 日志存储 |

### 3.3 分区职责与数据流向

```
上位机/IOT 写入固件包
         │
         ▼
  SPI Flash OTA 数据区 (1MB)        ← 接收完整 OTA 固件包
         │
         │ OTA 调度层解析后按子固件类型分发
         │
         ├── 本机 (PD) 子固件 ──→ SPI Flash APP 备份区 (424KB)
         │                              │
         │                              │ 置 FIRMWARE_UPDATE 标志
         │                              │ NVIC_SystemReset()
         │                              ▼
         │                        Bootloader 搬运
         │                              │
         │                              ▼
         │                        内部 Flash APP 区
         │
         └── 外部设备 (BMS/INV) ──→ 通过 Modbus/CAN 分包下发到子设备
```

两个关键中转：
- **OTA 数据区 → 备份区**：由 APP 层的 OTA 驱动完成（`cFirmwareUpdate` 从 SPI Flash OTA 区拷贝到 SPI Flash 备份区）
- **备份区 → 内部 Flash**：由 Bootloader 完成（`cFirmwareUpdateAPP` 从 SPI Flash 备份区拷贝到内部 Flash APP 区）

---

## 4. OTA 固件包协议（字节级）

固件包由上位机合成工具打包，写入 SPI Flash OTA 数据区。整体布局：

```
SPI_FLASH_OTA_ADDR
│
├── 总头信息 (OTAFirmwareTotalInfoType, 256 Bytes)
│
├── 子固件 0
│   ├── 子头 (FirmwarePortInfoType, 64 Bytes)
│   └── 子固件数据 (N Bytes)
│
├── 子固件 1
│   ├── 子头 (FirmwarePortInfoType, 64 Bytes)
│   └── 子固件数据 (N Bytes)
│
├── ...
│
└── 子固件 M
    ├── 子头 (FirmwarePortInfoType, 64 Bytes)
    └── 子固件数据 (N Bytes)
```

### 4.1 总头信息（OTAFirmwareTotalInfoType，256 字节）

总头位于 OTA 数据区起始位置，描述整个固件包的元信息：

```c
typedef struct
{
    uint8_t FileType[4];        /* +0x00  头部标识："DBS" (含 \0) */
    uint32_t registers;         /* +0x04  寄存器位域 */
    uint32_t length;            /* +0x08  总固件数据长度 (不含总头) */
    uint32_t crc32;             /* +0x0C  总固件数据的 CRC 校验值 */
    uint8_t hash[32];           /* +0x10  Hash 校验值 (可选) */
    char versionSoft[16];       /* +0x30  固件包软件版本号 */
    char versionHard[16];       /* +0x40  硬件版本号 */
    uint8_t firmwareNumber;     /* +0x50  子固件数量 */
    uint8_t reSendCount;        /* +0x51  每个子固件的重传次数 */
    uint8_t reserved[46];       /* +0x52  预留 */
    OTAFirmwarePortType FirmwareOTAPort[8]; /* +0x80  子固件地址表 (8 × 8B = 64B) */
    uint8_t usreserved[...];    /* +0xC0  预留填充 */
    uint32_t crc32Head;         /* +0xFC  前 252 字节的 CRC16-Modbus 校验值 */
} OTAFirmwareTotalInfoType;     /* 总计 256 字节 */
```

**registers 位域说明**：

| 位 | 含义 |
| :--- | :--- |
| bit0-bit1 | 预留 |
| bit2 | 0 = 不需要更新，1 = 需要更新 |
| bit3-bit4 | Hash 校验类型：0 = 无校验，1 = CRC32，2 = MD5，3 = SHA1 |

**子固件地址表**（OTAFirmwarePortType，每项 8 字节）：

```c
typedef struct
{
    uint32_t startAddr;    /* 子固件在 OTA 数据区中的绝对地址 */
    uint32_t length;       /* 子固件总长度 (含子头) */
} OTAFirmwarePortType;
```

### 4.2 子固件头信息（FirmwarePortInfoType，64 字节）

每个子固件数据前有一个 64 字节的子头，描述该固件的身份信息：

```c
typedef struct{
    char sign[4];           /* +0x00  标识头："FILE" */
    uint16_t type;          /* +0x04  固件类型 */
    uint16_t number;        /* +0x06  类型子序号 */
    uint32_t registers;     /* +0x08  寄存器位域 */
    uint32_t address;       /* +0x0C  目标烧录地址 */
    uint32_t length;        /* +0x10  固件数据长度 */
    uint32_t crcValue;      /* +0x14  固件 CRC16-Modbus 校验值 (低 16 位有效) */
    char versionSoft[16];   /* +0x18  软件版本号 */
    char versionHard[16];   /* +0x28  硬件版本号 */
    uint8_t reserved[...];  /* +0x38  预留填充至 64 字节 */
} FirmwarePortInfoType;     /* 总计 64 字节 */
```

**type 字段编码**：

| type 值 | 宏定义 | 含义 |
| :--- | :--- | :--- |
| 0x00 | `OTA_Type_PD` | PD 本机 (EMS 主控) |
| 0x01 | `OTA_Type_BMS` | BMS 电池管理系统 |
| 0x02 | `OTA_Type_INV` | 逆变器 |

**number 字段编码**（针对 PD 本机）：

| number 值 | 含义 |
| :--- | :--- |
| 0 | APP 应用程序 |
| 1 | Bootloader |
| 2 | Boot 引导程序 |

**registers 位域说明**：

| 位 | 含义 |
| :--- | :--- |
| bit0-bit1 | 固件存储位置：0 = 芯片内部 Flash，1 = 外部 SPI Flash |
| bit2 | 0 = 不需要更新，1 = 需要更新 |

### 4.3 固件包校验层级

OTA 采用三级校验，层层把关数据完整性：

```
第一级：总头自校验
  crc32Head == CRC16_MODBUS(总头前 252 字节)
  → 确认总头本身未损坏

第二级：整包数据校验
  crc32 == CRC16_MODBUS(总头之后的所有子固件数据)
  → 确认整个固件包传输无误

第三级：子固件校验
  crcValue == CRC16_MODBUS(子固件数据，不含子头)
  → 确认单个子固件数据完整
  → 在 cOTAFirmwareUpdatePort() 中执行
```

---

## 5. OTA 升级完整流程

### 5.1 固件接收阶段

固件包通过两种渠道写入 SPI Flash OTA 数据区：

**方式一：上位机 Modbus 接收**

```
上位机每帧格式 (Modbus 自定义功能码):
┌─────────┬──────────┬──────────┬──────────┬──────────┐
│ Modbus头 │ Addr(4B) │ Len(4B)  │ CRC(4B)  │ Data(NB) │
│ (5B)    │ 大端      │ 大端      │ 大端      │ 固件数据  │
└─────────┴──────────┴──────────┴──────────┴──────────┘
```

`cOTAModbusPackAnalysis()` 每帧处理逻辑：
1. 解析地址偏移 (`Addr`)、数据长度 (`Len`)、帧校验值 (`CRC`)
2. 大端转小端
3. CRC16-Modbus 校验本帧数据
4. 校验通过后写入 `SPI_FLASH_OTA_ADDR + Addr`

**方式二：IOT (涂鸦) 接收**

`cOTAIOTFrimwareDataRecever()` 流式写入：
1. 按 `position` 偏移写入 `SPI_FLASH_OTA_ADDR + position`
2. 当 `length == 0` 时表示传输完成，触发 `OTA_STATE_START`

### 5.2 START 阶段：校验与初始化

当 `OTAInfoType.state` 变为 `OTA_STATE_START` 时，`vOTAStart()` 执行：

```
vOTAStart()
    │
    ├── 1. uiOTACheck() 校验固件包
    │       ├── 从 SPI Flash 读取总头 (OTAFirmwareTotalInfoType)
    │       ├── 第一级校验：crc32Head vs CRC16_MODBUS(前 252B)
    │       └── 第二级校验：crc32 vs CRC16_MODBUS(总头之后全部数据)
    │       └── 校验失败 → OTA_STATE_FAIL + 记录错误码
    │
    ├── 2. vOTADeinit() 初始化子模块信息
    │       ├── 遍历 firmwareNumber 个子固件
    │       │   ├── 从 SPI Flash 读取子头 (FirmwarePortInfoType)
    │       │   ├── 填充 OTAFirmwarePortInfoType:
    │       │   │   state = READY, type, number, ReSendCount,
    │       │   │   address (子头之后), length (减去子头),
    │       │   │   crcValue, OTAPortVersion
    │       │   └── 累加非 PD 子固件总长度 (用于进度计算)
    │       ├── 填充 OTAInfoType:
    │       │   firmwareNumber, FirmwareLengthTotal, error=NULL
    │       └── 持久化 OTAInfoType 到内部 Flash (cOTAUpdateCrcAndWriteToFlash)
    │
    └── 3. cOTAStateSet(OTA_STATE_UPDATING)
            → 进入升级执行阶段
```

### 5.3 UPDATING 阶段：分包升级

`vOTAFirmwareUpdateAll()` 依次处理每个子固件：

```
vOTAFirmwareUpdateAll()
│
├── for i = 0 to firmwareNumber - 1:
│   │
│   ├── 【断电续升检测】
│   │   Port[i].state == UPDATING ?
│   │     → ReSendCount > 0 ? 跳到 __UPDATE_BEGIN 继续升级
│   │     → ReSendCount == 0 ? 标记 FAIL
│   │
│   ├── 【就绪态检测】
│   │   Port[i].state == READY && ReSendCount > 0 ?
│   │
│   ├── __UPDATE_BEGIN:
│   │   ├── Port[i].state = UPDATING
│   │   ├── Port[i].ReSendCount--
│   │   ├── 持久化到内部 Flash         ← 断电保护点
│   │   │
│   │   ├── cOTAFirmwareUpdatePort(&Port[i])
│   │   │   ├── 第三级校验：子固件 CRC
│   │   │   └── switch(type):
│   │   │       ├── PD   → cOTAUpgradePD()
│   │   │       ├── BMS  → cOTAUpgradeBMS()
│   │   │       ├── INV  → cOTAUpgradeInv()
│   │   │       └── ...
│   │   │
│   │   ├── 成功 → Port[i].state = SUCCESS, ReSendCount = 0
│   │   │          PD 类型则 NVIC_SystemReset() 进入 Bootloader 搬运
│   │   │
│   │   └── 失败 → ReSendCount > 0 ? 重试 (goto __UPDATE_BEGIN)
│   │              ReSendCount == 0 ? Port[i].state = FAIL
│   │
│   └── 【最后一个子固件完成后】
│       遍历所有 Port 的 state:
│       全部 SUCCESS → 本机 OTA_STATE_SUCCESS
│       存在 FAIL   → 本机 OTA_STATE_FAIL
│
├── SUCCESS → OTA_STATE_DISABLE + NVIC_SystemReset()
└── FAIL    → OTA_STATE_DISABLE
```

### 5.4 本机 (PD) 升级详细流程

本机升级是 OTA 中最复杂的路径，涉及两阶段搬运：

```
阶段一：APP 层 (cOTAUpgradePD)
═══════════════════════════════
SPI Flash OTA 数据区 (子固件数据)
         │
         │ cFirmwareUpdate(appOut, OTA源)
         │   ├── 源 CRC 校验
         │   ├── 256B 分块拷贝
         │   └── 目标 CRC 验证
         ▼
SPI Flash APP 备份区 (appOut)
         │
         │ registers |= FIRMWARE_UPDATE   ← 置更新标志
         │ cFirmwareWrite()               ← 持久化 FirmwareInfoType
         │ NVIC_SystemReset()             ← 系统复位
         ▼

阶段二：Bootloader 层 (cFirmwareUpdateAPP)
═══════════════════════════════════════════
Boot 上电
  │
  ├── vFirmwareInit() 读取 FirmwareInfoType
  │
  ├── 检查 appOut.registers & FIRMWARE_UPDATE ?
  │     │
  │     ├── 是 → cFirmwareUpdateAPP()
  │     │         ├── 清除 FIRMWARE_UPDATE 标志
  │     │         ├── cFirmwareUpdate(app, appOut)
  │     │         │   ├── 源 CRC 校验 (SPI Flash 备份区)
  │     │         │   ├── 256B 分块拷贝到内部 Flash APP 区
  │     │         │   └── 目标 CRC 验证
  │     │         └── 更新 app 的版本信息
  │     │
  │     └── 否 → 跳过
  │
  ├── (类似检查 bootloaderOut / bootOut)
  │
  └── cFirmwareJumpAPP()
        ├── CRC 校验 APP 区完整性
        └── 跳转执行
```

`cOTAUpgradePD()` 根据 `number` 字段决定目标分区：

| number | 源 (SPI Flash OTA 区) | 中转 (SPI Flash 备份区) | 目标 (内部 Flash) |
| :--- | :--- | :--- | :--- |
| 0 (APP) | 子固件数据 | appOut | app |
| 1 (Bootloader) | 子固件数据 | bootloaderOut | bootloader |
| 2 (Boot) | 子固件数据 | bootOut | boot |

### 5.5 外部设备 (BMS/INV) 升级流程

与本机升级的关键区别：不经过 Bootloader，直接通过通讯协议分包下发。

```
SPI Flash OTA 数据区 (子固件数据)
         │
         │ 按目标设备的协议分包
         │ 通过 Modbus/CAN 逐包发送
         ▼
    BMS / 逆变器接收并自行烧录
```

各子模块升级驱动封装了与目标设备的通讯协议差异，对 OTA 框架层透明。

---

## 6. Bootloader 机制

### 6.1 FirmwareInfoType 结构

`FirmwareInfoType` 是 Bootloader 的核心数据结构，管理 6 个固件分区（3 个内部 + 3 个外部备份），持久化在内部 Flash 的 `FLASH_SYSTEM_DATA_ADDR`：

```c
typedef struct{
    uint32_t type;       /* 类型标识 */
    uint32_t length;     /* 有效数据长度 (用于 CRC 计算范围) */
    uint32_t crc;        /* 自身校验值 (CRC16-Modbus) */
    uint32_t rec;        /* 预留 */

    /* 内部 Flash 分区 */
    FirmwarePortInfoType boot;           /* Boot 区 */
    FirmwarePortInfoType bootloader;     /* Bootloader 区 */
    FirmwarePortInfoType app;            /* APP 区 */
    FirmwarePortInfoType rec0;           /* 预留 */

    /* 外部 SPI Flash 备份分区 */
    FirmwarePortInfoType bootOut;        /* Boot 备份 */
    FirmwarePortInfoType bootloaderOut;  /* Bootloader 备份 */
    FirmwarePortInfoType appOut;         /* APP 备份 */
    FirmwarePortInfoType rec1;           /* 预留 */
} FirmwareInfoType;
```

每个 `FirmwarePortInfoType` 的 `registers` 字段中：
- **bit0-bit1**：存储位置（0 = 内部 Flash，1 = 外部 SPI Flash）
- **bit2**：更新标志（`FIRMWARE_UPDATE = 0x04`），为 1 表示需要从备份区搬运到运行区

### 6.2 启动流程

```
上电 / 系统复位
     │
     ▼
  Boot (FLASH_BOOT_ADDR)
     │
     ├── vFirmwareInit()：读取 FirmwareInfoType
     │   ├── CRC 校验有效 → 使用已有参数
     │   └── CRC 无效 → vFirmwareFactoryInit() 恢复出厂值
     │
     ├── bootOut.registers & FIRMWARE_UPDATE ?
     │   └── 是 → cFirmwareUpdateBoot()：备份区 → Boot 区
     │
     ├── cFirmwareJumpBootloader()
     │   ├── bootloaderOut 无更新标志 ?
     │   │   └── cFirmwareJump(bootloader)：CRC 校验 → 跳转
     │   └── 跳转失败 → 置 bootloaderOut 更新标志
     │
     ▼
  Bootloader (FLASH_BOOTLOADER_ADDR)
     │
     ├── bootloaderOut.registers & FIRMWARE_UPDATE ?
     │   └── 是 → cFirmwareUpdateBootloader()：备份区 → Bootloader 区
     │
     ├── appOut.registers & FIRMWARE_UPDATE ?
     │   └── 是 → cFirmwareUpdateAPP()：备份区 → APP 区
     │
     ├── cFirmwareJumpAPP()
     │   ├── appOut 无更新标志 ?
     │   │   └── cFirmwareJump(app)：CRC 校验 → 跳转
     │   └── 跳转失败 → 置 appOut 更新标志
     │
     ▼
  APP (FLASH_APP_ADDR)
     │
     └── 正常运行 + OTA 任务就绪
```

### 6.3 cFirmwareUpdate() 搬运核心逻辑

负责将固件从源分区拷贝到目标分区，是 Bootloader 和 OTA 共用的底层函数：

```
cFirmwareUpdate(pTarget, pSource)
     │
     ├── 1. 源 CRC 校验
     │   uiFirmwareCRCUpdate(pSource) == pSource->crcValue ?
     │   └── 不匹配 → 返回错误 3
     │
     ├── 2. 目标 CRC 比较 (跳过相同固件)
     │   uiFirmwareCRCUpdate(pTarget) == pSource->crcValue ?
     │   └── 匹配 → 返回 0 (无需搬运)
     │
     ├── 3. 分块拷贝 (256 字节/块)
     │   while(剩余长度 > 0):
     │     ├── 根据 pSource->registers 判断读取来源 (内部/外部 Flash)
     │     ├── 读取 256B 到 st_ucFirmwareBuff
     │     ├── 根据 pTarget->registers 判断写入目标 (内部/外部 Flash)
     │     └── 写入 256B
     │
     └── 4. 目标 CRC 验证
         uiFirmwareCRCUpdate(pTarget) == pSource->crcValue ?
         └── 不匹配 → 返回错误 5
```

### 6.4 cFirmwareJumpTo() 跳转机制

```c
int8_t cFirmwareJumpTo(uint32_t uiAddress)
{
    /* 读取目标地址的第一个字 (栈指针初始值) */
    uiHeapData = *(volatile uint32_t*)uiAddress;

    /* 合法性检查：栈指针必须在 SRAM 范围内 */
    if((SRAM_BASE < uiHeapData) && (uiHeapData <= (SRAM_BASE + 1024 * 64)))
    {
        /* 读取复位向量 (目标地址 + 4) */
        uiJumpAddress = *(volatile uint32_t*)(uiAddress + 4);

        /* 恢复 HAL 到初始状态 */
        HAL_DeInit();

        /* 设置主堆栈指针 */
        __set_MSP(uiHeapData);

        /* 跳转执行 */
        typeJumpToAPPlication();
    }

    return 1; /* 跳转失败 */
}
```

跳转前的栈指针验证确保目标地址确实存在有效的固件，避免跳转到空白 Flash 导致 HardFault。

---

## 7. 多子模块升级扩展（PDMini 实战案例）

### 7.1 扩展入口：cOTAFirmwareUpdatePort()

所有子模块升级的路由入口，按 `type` 分发：

```c
int8_t cOTAFirmwareUpdatePort(OTAFirmwarePortInfoType *ptypeOTAFirmwarePortInfo)
{
    /* 第三级校验：子固件 CRC */
    if(ptypeOTAFirmwarePortInfo->crcValue == uiFirmwareCRCUpdate(&typeFirmwareOTA))
    {
        switch(ptypeOTAFirmwarePortInfo->type)
        {
            case OTA_Type_PD:      cError = cOTAUpgradePD(ptypeOTAFirmwarePortInfo);      break;
            case OTA_Type_BMS:     cError = cOTAUpgradeBMS(ptypeOTAFirmwarePortInfo);     break;
            case OTA_Type_INV:     cError = cOTAUpgradeInv(ptypeOTAFirmwarePortInfo);     break;
        }
    }
    return cError;
}
```

### 7.2 各子模块升级差异

| 子模块 | type | 升级方式 | 通讯协议 | 升级驱动 |
| :--- | :--- | :--- | :--- | :--- |
| PD (本机) | 0x00 | SPI Flash 中转 + Bootloader 搬运 | 内部操作 | `DriverUpgradePD.c` |
| BMS | 0x01 | 通讯分包下发 | Modbus (RS485) | `DriverUpgradeBMS.c` |
| INV (逆变器) | 0x02 | 通讯分包下发 | Modbus (RS485) | `DriverUpgradeInv.c` |

**本机 (PD) 升级的特殊性**：
- 升级成功后立即 `NVIC_SystemReset()`，由 Bootloader 完成最终搬运
- 支持三个目标分区 (APP/Bootloader/Boot)，由 `number` 字段选择
- 升级前会校验固件类型名（如 "MySTM32"），防止烧入错误固件

**外部设备升级的特殊性**：
- 升级前需切换系统状态 `vSystemStateSet(SYSTEM_ACTION_OTA)`，禁止常规通讯干扰
- 需等待通讯任务完成（如延时 5000ms），确保总线空闲
- 升级完成后需关机重启外部设备（如 BMS 需要 `cBMSShutdown()`）

### 7.3 新增子设备的扩展步骤

1. **定义 type 宏**：在 `DriverOTA.h` 中添加 `#define OTA_Type_XXX 0x05`
2. **实现升级驱动**：创建 `DriverUpgradeXxx.c/h`，实现 `cOTAUpgradeXxx()` 接口
3. **注册路由**：在 `cOTAFirmwareUpdatePort()` 的 switch 中添加 case
4. **协调上位机**：确保上位机合成工具使用相同的 type 编码
5. **版本比较**（可选）：在 `cVersionCompare()` 中添加该 type 的版本获取逻辑

框架层代码（`vOTAFirmwareUpdateAll`、`vOTAStart`、`taskOTA`）无需任何修改。

---

## 8. 关键数据结构汇总

### 8.1 OTA 固件包层

| 结构体 | 大小 | 存储位置 | 生命周期 | 用途 |
| :--- | :--- | :--- | :--- | :--- |
| `OTAFirmwareTotalInfoType` | 256B | SPI Flash OTA 区起始 | 固件包接收后存在 | OTA 固件包总头 |
| `OTAFirmwarePortType` | 8B | 嵌入在总头中 | 同上 | 子固件地址与长度索引 |
| `FirmwarePortInfoType` | 64B | 各子固件数据前 / 内部 Flash SystemData 区 | 固件包中 + Bootloader 管理 | 子固件头 / 分区描述 |

### 8.2 OTA 运行状态层

| 结构体 | 存储位置 | 持久化 | 用途 |
| :--- | :--- | :--- | :--- |
| `OTAInfoType` | 内部 Flash `FLASH_OTA_DATA_ADDR` | 每次状态变更时写入 | OTA 整体运行状态 + 各子模块状态 |
| `OTAFirmwarePortInfoType` | 嵌入在 OTAInfoType.Port[] 中 | 同上 | 单个子模块的升级状态 |

`OTAInfoType` 内部布局：

```
OTAInfoType
├── Port[0..7]  (OTAFirmwarePortInfoType × 8)
│   ├── type              子模块类型
│   ├── number            固件序号
│   ├── state             子模块状态机
│   ├── ReSendCount       剩余重试次数
│   ├── error             错误码
│   ├── OTAPortVersion    子固件版本
│   ├── address           SPI Flash 中的数据起始地址
│   ├── length            固件数据长度
│   ├── writeLengthNow    已写入长度 (进度跟踪)
│   └── crcValue          CRC 校验值
├── firmwareNumber        子固件总数
├── state                 本机状态机
├── error                 本机错误码
├── OTARemainTime         剩余升级时间
├── OTATuyaAppVersion     当前 APP 版本 (回退用)
├── OTATuyaFirmwareVersion 目标版本
├── FirmwareLengthTotal   非 PD 子固件总长度 (进度计算)
└── OTATotalInfoCrc       整个 OTAInfoType 的 CRC32 (自校验)
```

### 8.3 Bootloader 分区管理层

| 结构体 | 存储位置 | 持久化 | 用途 |
| :--- | :--- | :--- | :--- |
| `FirmwareInfoType` | 内部 Flash `FLASH_SYSTEM_DATA_ADDR` | 升级完成/参数变更时写入 | 管理 6 个固件分区的参数 |

`FirmwareInfoType` 内部布局：

```
FirmwareInfoType
├── type, length, crc, rec   元数据
├── boot           内部 Flash Boot 分区参数
├── bootloader     内部 Flash Bootloader 分区参数
├── app            内部 Flash APP 分区参数
├── rec0           预留
├── bootOut        SPI Flash Boot 备份分区参数
├── bootloaderOut  SPI Flash Bootloader 备份分区参数
├── appOut         SPI Flash APP 备份分区参数
└── rec1           预留
```

---

## 9. 版本校验与回退

### 9.1 升级后版本比较

升级完成（`OTA_STATE_SUCCESS`）后不直接结束，而是进入 `OTA_STATE_CHECK_VERSION`，由 `vCheckFirmwareVersion()` 做最终验证：

```
vCheckFirmwareVersion()
     │
     ├── 等待产品初始化完成 (最长 30 秒超时)
     │   各子设备上线并上报版本
     │
     ├── 遍历所有子固件:
     │   cVersionCompare(&Port[i])
     │     ├── 读取通讯获取的实际版本 (pucCommVersion)
     │     ├── 读取内部 Flash 存储的预期版本 (pucFirmwareVersion)
     │     └── memcmp 比较
     │
     ├── 全部匹配 → 升级确认成功
     └── 存在不匹配 → 触发回退
```

### 9.2 各子模块版本获取方式

| 子模块 | 版本来源 | 比较长度 |
| :--- | :--- | :--- |
| PD (本机) | 固件内嵌版本信息 (`FLASH_APP_ADDR + 0x820`) | 8 字节 |
| BMS | 通讯读取 `ptypeBMSInfo->verInfo.softWareVer` | 8 字节 |
| INV (逆变器) | 通讯读取 `ptypeInvInfo->versionSoft` | 4~8 字节 |

### 9.3 失败回退策略

当版本比较不一致或升级过程中发生不可恢复的错误：

1. **还原涂鸦版本**：将 `OTATuyaAppVersion`（升级前备份的版本）写回参数区，确保云端版本号与实际固件一致
2. **复位 WiFi**：`vWifiReset()` 使涂鸦模块重新上报正确版本
3. **清除 OTA 状态**：`cOTAStateSet(OTA_STATE_DISABLE)` 回到空闲态
4. **关闭 OTA 系统状态**：`vSystemStateReset(SYSTEM_ACTION_OTA)` 恢复正常业务通讯

---

## 10. OTA 初始化与断电恢复

### 10.1 vOTAInit() 上电初始化

每次上电时 `taskOTA` 首先调用 `vOTAInit()`，处理三种情况：

```
vOTAInit()
     │
     ├── 从内部 Flash 读取 OTAInfoType
     ├── 计算当前数据的 CRC32
     │
     ├── OTATotalInfoCrc == 0xFFFFFFFF ?
     │   └── 首次烧录 (全 FF)：清零 OTAInfoType
     │
     ├── OTATotalInfoCrc != 计算值 ?
     │   └── 数据被篡改或旧 Boot 升级过来：
     │       更新 CRC → 触发 OTA_STATE_START (尝试恢复)
     │
     └── OTATotalInfoCrc == 计算值 ?
         └── 正常：保持现有状态
             (如果之前是 UPDATING，taskOTA 会继续执行)
```

### 10.2 断电续升机制

OTA 的断电恢复依赖于**状态持久化**和**重试计数器**：

- 每次子固件进入 `UPDATING` 状态前，`ReSendCount--` 并写入内部 Flash
- 断电重启后，`taskOTA` 检测到 `state == START` 或 `UPDATING`，自动继续流程
- `vOTAFirmwareUpdateAll()` 会检测子固件处于 `UPDATING` 状态（说明上次中途断电），若 `ReSendCount > 0` 则重新升级该子固件

---

## 11. taskOTA 任务

OTA 的状态机由 FreeRTOS 任务 `vTaskOTA` 驱动，结构简洁：

```
vTaskOTA()
     │
     ├── vOTAInit()         ← 上电初始化
     │
     └── while(1)
         │
         ├── ulTaskNotifyTake(50ms)   ← 等待通知或超时轮询
         │
         ├── state == OTA_STATE_START && 初始化完成 ?
         │   └── vOTAStart()          ← 校验 + 初始化子模块
         │
         └── state == OTA_STATE_UPDATING && 初始化完成 ?
             └── vOTAFirmwareUpdateAll()  ← 执行分包升级
```

任务通知来源：
- Modbus 收到 OTA 启动指令 → `cOTAStateSet(OTA_STATE_START)`
- IOT 固件传输完成 → `cOTAStateSet(OTA_STATE_START)`
- 断电重启后自动检测状态

---

## 12. 设计要点归纳

| 设计决策 | 选择 | 理由 |
| :--- | :--- | :--- |
| 固件包存储 | 外部 SPI Flash | 内部 Flash 容量有限，OTA 包可达 1MB |
| 本机升级方式 | OTA 区→备份区→Bootloader 搬运 | APP 不能自己覆盖自己，需在 Bootloader 环境下执行 |
| 状态持久化 | 内部 Flash (CRC32 保护) | 断电重启后必须恢复状态，SPI Flash 写入速度慢且易被擦除影响 |
| 分块大小 | 256 字节 | 与 SPI Flash 页大小一致，最大化写入效率 |
| 校验算法 | CRC16-Modbus | 与 Modbus 协议一致，代码复用 |
| 子模块扩展 | switch-case + 函数指针风格 | 新增设备只改一处，框架代码零侵入 |
| 重试机制 | 每个子固件独立计数 | 单个子固件失败不影响其他已成功的子固件 |
| 跳转前校验 | 栈指针范围检查 | 防止跳转到空白/损坏的 Flash 区域 |
