# STM32F10x 片内 Flash 设计文档 (DevicesFlash)

| 属性 | 内容 |
| :--- | :--- |
| **模块名称** | DevicesFlash (片内 Flash 读写驱动) |
| **源文件** | `DevicesFlash.c` / `DevicesFlash.h` |
| **硬件依赖** | STM32F103 片内 Flash (512KB) |
| **软件依赖** | STM32 HAL 库 (Flash 驱动), FreeRTOS (递归互斥信号量) |
| **版本** | v1.0 |
| **最后更新** | 2026-02-27 |

---

## 1. 设计目标 (Design Goals)

对 STM32F103 片内 Flash 进行读写封装，支持在应用运行时对 Flash 进行编程：

1. **擦写封装**：基于 HAL 库实现页擦除 + 半字写入，对外提供连续写入接口
2. **分区管理**：将 512KB Flash 按功能划分为 Boot、系统数据、OTA 数据、用户数据、Bootloader、APP 等区域
3. **RTOS 安全**：通过递归互斥信号量保护 Flash 操作，支持多任务环境
4. **边界保护**：写入和读取均检查地址范围，防止越界操作

---

## 2. STM32F103 片内 Flash 基础 (Flash Fundamentals)

### 2.1 概述

STM32F103 系列内置 Flash 存储器，用于存放程序代码和常量数据。大容量型号（STM32F103xC/D/E）拥有最大 512KB Flash。

| 参数 | 值 |
| :--- | :--- |
| 基地址 | 0x0800 0000 |
| 最大容量 | 512KB (大容量型号) |
| 页大小 | 2KB (大容量型号) |
| 编程粒度 | 半字 (16bit / 2字节) |
| 擦除粒度 | 页 (2KB) |
| 擦写寿命 | ~1 万次 |

### 2.2 Flash 操作约束

片内 Flash 与外部 SPI Flash 类似，也属于 NOR Flash，具备相同的核心约束：

| 约束 | 说明 |
| :--- | :--- |
| **写前必须擦除** | Flash 只能把 1 写成 0，不能把 0 写成 1。必须先擦除（全部置 0xFF）再写入 |
| **擦除粒度为页** | 最小擦除单位是 2KB 页，不能只擦除某几个字节 |
| **编程粒度为半字** | 每次写入最小单位是 16bit (2 字节)，且地址必须半字对齐 |

### 2.3 片内 Flash vs 外部 SPI Flash 对比

| | 片内 Flash (STM32) | 外部 SPI Flash (W25Qxx) |
| :--- | :--- | :--- |
| **连接方式** | 内部总线直连，可直接寻址 | SPI 外设通信，需发命令 |
| **读取方式** | 直接 `memcpy`，零开销 | 需通过 SPI 发送读命令 |
| **编程粒度** | 半字 (2 字节) | 页 (256 字节) |
| **擦除粒度** | 2KB 页 | 4KB 扇区 |
| **容量** | 64KB ~ 512KB | 2MB ~ 16MB |
| **擦写寿命** | ~1 万次 | ~10 万次 |
| **速度** | 极快 (零等待/少等待) | 受 SPI 总线速率限制 |
| **典型用途** | 程序存储、少量参数保存 | 固件备份、OTA、日志、大量数据 |

### 2.4 编程/擦除流程

```
片内 Flash 写入流程:

  ① 解锁 Flash (HAL_FLASH_Unlock)    ← Flash 默认处于锁定状态，防止误写
       ↓
  ② 清除错误标志位                      ← 清除 EOP / PGERR / WRPERR 标志
       ↓
  ③ 页擦除 (如果需要)                   ← HAL_FLASHEx_Erase, 2KB 整页擦除
       ↓
  ④ 半字编程                            ← HAL_FLASH_Program, 每次写 2 字节
       ↓
  ⑤ 锁定 Flash (HAL_FLASH_Lock)      ← 操作完成后重新锁定
```

Flash 编程控制寄存器 (FLASH_CR) 默认处于锁定状态，必须通过写入特定密钥序列解锁后才能进行擦除和编程操作。这是 STM32 的硬件保护机制，防止程序跑飞时误写 Flash。

---

## 3. Flash 分区规划 (Partition Layout)

本项目将 512KB 片内 Flash 空间按功能划分为多个区域：

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    片内 Flash 分区布局 (512KB)                                │
└─────────────────────────────────────────────────────────────────────────────┘

  起始地址                    偏移       大小        用途
  ───────────────────────────────────────────────────────────────
  0x0800 0000                +0KB       32KB        Boot (引导程序)
  0x0800 8000                +32KB       2KB        系统数据 (SYSTEM_DATA)
  0x0800 8800                +34KB       2KB        OTA 数据 (OTA_DATA)
  0x0800 9000                +36KB       2KB        OTP 数据 (OTP_DATA)
  0x0800 9800                +38KB       2KB        用户数据 (USER_DATA)
  0x0800 A000                +40KB      64KB        Bootloader (引导加载程序)
  0x0801 A000                +104KB    408KB        APP (应用程序)
  0x0808 0000                                       Flash 末尾 (512KB)
```

对应头文件中的宏定义：

```c
#define FLASH_USER_PAGE_SIZE        ((uint32_t)2048)
#define FLASH_BASE_ADDR             ((uint32_t)0x08000000)
#define FLASH_USER_MAX_ADDR         (FLASH_BASE_ADDR + (512 * 1024))

#define FLASH_BOOT_ADDR             (FLASH_BASE_ADDR + 1024 * (0))              /* 32KB  Boot */
#define FLASH_SYSTEM_DATA_ADDR      (FLASH_BASE_ADDR + 1024 * (32))             /*  2KB  系统数据 */
#define FLASH_OTA_DATA_ADDR         (FLASH_BASE_ADDR + 1024 * (32 + 2))         /*  2KB  OTA 数据 */
#define FLASH_OTP_DATA_ADDR         (FLASH_BASE_ADDR + 1024 * (32 + 2 + 2))     /*  2KB  OTP 数据 */
#define FLASH_USER_DATA_ADDR        (FLASH_BASE_ADDR + 1024 * (32 + 2 + 2 + 2)) /*  2KB  用户数据 */
#define FLASH_BOOTLOADER_ADDR       (FLASH_BASE_ADDR + 1024 * (32 + 2 + 2 + 2 + 2))    /* 64KB  Bootloader */
#define FLASH_APP_ADDR              (FLASH_BASE_ADDR + 1024 * (32 + 2 + 2 + 2 + 2 + 64)) /* 剩余  APP */
```

**分区设计要点：**

- Boot 区域预留 32KB，存放最初的引导程序
- 系统数据、OTA 数据、OTP 数据、用户数据各 2KB（正好 1 页），方便整页擦写
- Bootloader 预留 64KB，用于 OTA 升级时的引导加载
- APP 占用剩余全部空间，用于存放应用程序

---

## 4. 软件设计 (Software Design)

### 4.1 模块分层

```
┌─────────────────────────────────────────┐
│          应用层 (OTA / 参数管理)          │  ← 调用 Flash 读写接口
├─────────────────────────────────────────┤
│     DevicesFlash.c / .h                 │  ← 页擦除 + 半字编程 + RTOS 保护
│     (片内 Flash 驱动层)                  │
├─────────────────────────────────────────┤
│     STM32 HAL 库                        │  ← HAL_FLASH_Program / HAL_FLASHEx_Erase
├─────────────────────────────────────────┤
│     Flash 存储控制器硬件                  │  ← 片内 Flash 外设
└─────────────────────────────────────────┘
```

### 4.2 写入函数 — `cFlashWriteDatas`

```c
int8_t cFlashWriteDatas(uint32_t uiAddress, const void *pvBuff, int32_t iLength)
```

**参数说明：**

| 参数 | 类型 | 说明 |
| :--- | :--- | :--- |
| `uiAddress` | `uint32_t` | 写入起始地址，必须半字对齐 |
| `pvBuff` | `const void *` | 待写入数据缓冲区 |
| `iLength` | `int32_t` | 写入长度 (字节) |
| 返回值 | `int8_t` | 0: 成功, 非零: 错误码 |

**错误码定义：**

| 返回值 | 含义 |
| :--- | :--- |
| 0 | 操作成功 |
| 1 | 参数非法 (长度<1 或地址越界) |
| 2 | 页擦除失败 |
| 4 | 半字编程失败 |

**执行流程：**

```
cFlashWriteDatas 执行流程:

  ① 参数校验
     └─ iLength < 1 或 uiAddress + iLength > FLASH_USER_MAX_ADDR → 返回 1
  ② 获取互斥锁
     └─ xSemaphoreTakeRecursive(g_xChipFlashSemaphore)
  ③ 解锁 Flash
     └─ HAL_FLASH_Unlock()
  ④ 清除错误标志
     └─ __HAL_FLASH_CLEAR_FLAG(EOP | PGERR | WRPERR)
  ⑤ 循环写入 (每次 2 字节)
     │
     ├─ 如果地址在页起始位置 (地址 % 2048 == 0):
     │   └─ 擦除该页 (最多重试 8 次)
     │       └─ 擦除失败 → cError |= 2, 退出循环
     │
     └─ 半字编程 (跳过相同数据的优化):
         ├─ 如果当前 Flash 值 == 待写入值 → 跳过 (无需写入)
         └─ 否则调用 HAL_FLASH_Program 写入半字
             └─ 写入失败 → cError |= 4, 退出循环
  ⑥ 锁定 Flash
     └─ HAL_FLASH_Lock()
  ⑦ 释放互斥锁
     └─ xSemaphoreGiveRecursive(g_xChipFlashSemaphore)
```

**设计要点：**

1. **自动页擦除**：当写入地址恰好落在页起始位置（2048 字节对齐）时，自动擦除该页。调用者需确保从页起始地址开始写入。

2. **擦除重试机制**：擦除操作最多重试 8 次，每次重试前清除错误标志，增强可靠性。

3. **跳过相同数据优化**：写入前先比较 Flash 当前值与待写值，如果相同则跳过本次编程，减少不必要的写操作，延长 Flash 寿命。

4. **半字为单位**：每次循环写入 2 字节（半字），地址递增 2，长度递减 2，直到写完全部数据。

### 4.3 读取函数 — `cFlashReadDatas`

```c
int8_t cFlashReadDatas(uint32_t uiAddress, void *pvBuff, int32_t iLength)
```

**参数说明：**

| 参数 | 类型 | 说明 |
| :--- | :--- | :--- |
| `uiAddress` | `uint32_t` | 读取起始地址 |
| `pvBuff` | `void *` | 接收数据缓冲区 |
| `iLength` | `int32_t` | 读取长度 (字节) |
| 返回值 | `int8_t` | 0: 成功, 1: 参数非法 |

**实现极其简单** — 片内 Flash 映射在 MCU 地址空间中，可以像访问 RAM 一样直接读取：

```c
memcpy(pvBuff, (const void *)uiAddress, iLength);
```

无需任何解锁、命令发送或等待操作。这是片内 Flash 相比外部 SPI Flash 的最大优势：读取零开销。

### 4.4 RTOS 互斥保护

Flash 写入操作使用 **FreeRTOS 递归互斥信号量** `g_xChipFlashSemaphore` 保护：

```c
xSemaphoreTakeRecursive(g_xChipFlashSemaphore, portMAX_DELAY);
/* ... Flash 操作 ... */
xSemaphoreGiveRecursive(g_xChipFlashSemaphore);
```

**为什么需要保护：**
- 多任务环境中，如果两个任务同时操作 Flash（一个擦除、一个编程），会导致 Flash 控制器状态混乱
- Flash 编程期间 CPU 访问 Flash 会产生总线等待，可能影响中断响应

**为什么用递归互斥：**
- 防止上层模块已持有锁时再次调用 Flash 操作导致死锁

---

## 5. API 总览 (API Reference)

| 函数 | 功能 | 返回值 |
| :--- | :--- | :--- |
| `cFlashWriteDatas(addr, *pBuf, len)` | 写入数据 (自动页擦除 + 半字编程) | 0: 成功, 非零: 错误 |
| `cFlashReadDatas(addr, *pBuf, len)` | 读取数据 (直接 memcpy) | 0: 成功, 1: 参数非法 |

### 宏定义

```c
#define FLASH_USER_PAGE_SIZE        2048                             /* 页大小 2KB */
#define FLASH_BASE_ADDR             0x08000000                       /* Flash 基地址 */
#define FLASH_USER_MAX_ADDR         (FLASH_BASE_ADDR + 512 * 1024)  /* Flash 最大地址 */
```

---

## 6. 使用示例 (Usage Examples)

### 6.1 写入参数数据

```c
#include "DevicesFlash.h"

typedef struct {
    uint32_t uiMagic;
    uint16_t usVersion;
    uint8_t  ucData[64];
} SystemParam_t;

SystemParam_t stParam = {
    .uiMagic = 0xDEADBEEF,
    .usVersion = 0x0100,
};

/* 写入到系统数据分区 (必须从页起始地址开始) */
int8_t ret = cFlashWriteDatas(FLASH_SYSTEM_DATA_ADDR, &stParam, sizeof(stParam));
if(ret != 0)
{
    /* 写入失败: ret & 2 = 擦除失败, ret & 4 = 编程失败 */
}
```

### 6.2 读取参数数据

```c
SystemParam_t stReadParam;

int8_t ret = cFlashReadDatas(FLASH_SYSTEM_DATA_ADDR, &stReadParam, sizeof(stReadParam));
if(ret == 0 && stReadParam.uiMagic == 0xDEADBEEF)
{
    /* 数据有效，正常使用 */
}
```

---

## 7. 注意事项 (Notes)

### 7.1 必须从页起始地址写入

`cFlashWriteDatas` 仅在地址落在页起始位置（2048 字节对齐）时自动擦除。如果从页中间地址开始写入，该页不会被擦除，可能因 Flash "只能 1→0" 的特性导致数据错误。

```
正确: cFlashWriteDatas(0x08008000, data, 100);  ← 页起始地址，自动擦除
错误: cFlashWriteDatas(0x08008100, data, 100);  ← 页中间地址，不会擦除，可能数据错误
```

### 7.2 写入长度应为偶数

由于编程粒度为半字（2 字节），写入长度建议为偶数。如果传入奇数长度，最后一次循环 `iLength -= 2` 会变为负数退出循环，最后一个字节不会被写入。

### 7.3 片内 Flash 编程期间的影响

STM32 片内 Flash 编程/擦除期间，CPU 无法从 Flash 取指令（Flash 总线被占用）。如果代码运行在 Flash 中（通常都是），编程期间 CPU 会被阻塞（stall），直到操作完成。这意味着：

- 编程期间中断响应延迟增大
- 擦除操作耗时较长（页擦除约 20~40ms），期间系统响应变慢
- 对实时性要求高的场景需注意

### 7.4 擦写寿命

STM32F103 片内 Flash 擦写寿命约 1 万次，远低于外部 SPI Flash 的 10 万次。频繁写入的数据（如日志）应存放在外部 Flash 中，片内 Flash 仅用于偶尔更新的参数和固件。

---

## 8. 版本变更记录 (Changelog)

| 版本 | 日期 | 变更内容 |
| :--- | :--- | :--- |
| v1.0 | 2026-02-27 | 初始版本：片内 Flash 读写驱动设计，含分区规划、页擦除、半字编程、RTOS 保护 |

---

**文档结束**
