# 三级 Boot 架构设计

| 属性 | 内容 |
| :--- | :--- |
| **模块名称** | 三级 Boot 启动架构 (Boot + Bootloader + APP) |
| **源文件** | `DriverBootloader.c/h` |
| **硬件依赖** | 内部 Flash (512KB)、外部 SPI Flash (W25Qxx) |
| **软件依赖** | DevicesFlash、DevicesSPIFlash、DevicesCRC |
| **版本** | v1.1 |
| **最后更新** | 2026-02-27 |

---

## 1. 为什么需要讨论 Boot 架构

嵌入式产品一旦出货，就面临一个核心问题：**如何安全地远程更新固件**。

Boot 架构的设计直接决定了：
- OTA 升级能不能做
- 升级过程断电了会不会变砖
- Bootloader 自己有 Bug 能不能安全修复
- Flash 空间够不够用

业界常见方案分为**二级架构**和**三级架构**。两者的区别**只有一点**：Bootloader 升级是否断电安全。本文围绕这个核心区别展开。

---

## 2. 二级 Boot 架构

### 2.1 什么是二级 Boot

二级架构只有两个程序：**Bootloader** 和 **APP**。

```
内部 Flash
┌────────────────────────────┐
│ Bootloader                 │ 0x08000000
│ 上电入口，负责：             │
│   - 检查是否需要升级         │
│   - 搬运新固件到 APP 区      │
│   - 跳转到 APP 执行          │
├────────────────────────────┤
│ 参数区                      │
│ 升级标志、固件信息            │
├────────────────────────────┤
│ APP                        │
│ 应用程序                    │
└────────────────────────────┘
```

上电流程：

```
上电
  │
  ▼
Bootloader (固定在 Flash 起始地址)
  │
  ├── 有升级标志 ? → 搬运新固件到 APP 区 → 清除标志
  │
  └── 跳转到 APP
```

### 2.2 二级架构的优势

| 优势 | 说明 |
| :--- | :--- |
| **结构简单** | 只有两个程序，Flash 分区少，开发和维护成本低 |
| **启动快** | 只有一次跳转，启动路径短 |
| **Flash 利用率高** | 没有额外的 Boot 层开销，APP 可用空间更大 |
| **适合小 Flash 芯片** | 64KB、128KB 等小容量芯片几乎只能用二级架构 |

### 2.3 二级架构能不能升级 Bootloader？

**能。** 方式很直接：APP 运行在 APP 区，直接擦写 Bootloader 所在的 Flash 区域，写入新固件。APP 操作的不是自己所在的 Flash，当前程序执行不受影响，技术上完全没有问题。

**但不安全。** 问题在于没有兜底：

```
APP 擦写 Bootloader 过程中断电
  → Bootloader 被写了一半，损坏
  → 下次上电从 Bootloader 区启动 → 执行残缺代码 → 变砖
  → Bootloader 是启动链的第一个程序，它坏了没有任何软件能修复
  → 只能拆机用烧录器修复
```

二级架构下 Bootloader 是"裸奔"的——它上面没有人能保护它。APP 可以随便改它，但改的过程中断电就完了。

---

## 3. 三级 Boot 架构

### 3.1 核心思路：给 Bootloader 也找一个"保护者"

二级架构中 APP 升级是安全的，因为 Bootloader 保护它：搬运过程断电，Bootloader 不受影响，下次重新搬运就行。

那 Bootloader 为什么不安全？因为**没有人保护它**。

三级架构的思路很简单：**在 Bootloader 上面再加一个极简的 Boot 程序**，让 Boot 来保护 Bootloader，就像 Bootloader 保护 APP 一样。

```
Boot 保护 Bootloader  ←→  Bootloader 保护 APP
（完全对称的关系）
```

Bootloader 在三级架构中的地位，本质上就是一个**可以被安全升级的"小 APP"**。

### 3.2 Flash 分区

```
内部 Flash
┌────────────────────────────┐
│ Boot (32KB)                │ 0x08000000
│ 极简引导，只做跳转            │
├────────────────────────────┤
│ 参数区 (8KB)               │
│ FirmwareInfoType           │
├────────────────────────────┤
│ Bootloader (64KB)          │ 0x0800A000
│ 升级搬运 + 跳转到 APP        │
├────────────────────────────┤
│ APP (≈ 408KB)              │ 0x0801A000
│ 应用程序                    │
└────────────────────────────┘
```

### 3.3 启动流程

在我们的实现中，**所有搬运工作都由 Bootloader 完成**，Boot 只负责跳转：

```
上电
  │
  ▼
Boot (0x08000000)
  │
  ├── vFirmwareInit()：读取 FirmwareInfoType
  ├── 不检查更新标志，不做任何搬运
  └── 直接跳转到 Bootloader
        │
        ▼
Bootloader (0x0800A000)
  │
  ├── 先尝试跳转 APP → APP 正常则直接跳走
  │
  └── 跳转失败 或 有更新标志 → 进入升级模式：
      ├── cFirmwareUpdateBoot()  ← 搬运 Boot 区（工程预留，正常不触发）
      ├── cFirmwareUpdateAPP()   ← 搬运 APP 区
      └── 搬运完成 → NVIC_SystemReset() → 重新走启动流程
            │
            ▼
APP (0x0801A000)
  │
  └── 正常运行，OTA 任务就绪
```

### 3.4 为什么断电安全

安全的关键在于：**谁在运行，谁就不会被擦**。

| 搬运操作 | 谁在运行 | 擦写哪个区域 | 断电后果 |
| :--- | :--- | :--- | :--- |
| Bootloader 搬运 APP | Bootloader (0x0800A000) | APP 区 (0x0801A000) | Bootloader 不受影响，重启重新搬运 |
| Bootloader 搬运 Boot | Bootloader (0x0800A000) | Boot 区 (0x08000000) | Bootloader 不受影响，重启重新搬运 |
| APP 写入 SPI Flash 备份区 | APP (0x0801A000) | 外部 SPI Flash | APP 不受影响，重启重新写入 |

每一步操作者和被操作区域都**物理隔离**，断电只会导致"写了一半"，不会损坏当前正在执行的程序。下次上电检测到更新标志还在，重新搬运即可。

### 3.5 本质：对称的保护关系

```
二级架构：
  Bootloader → 保护 APP（安全升级 APP）
  ???        → 没人保护 Bootloader

三级架构：
  Boot       → 为 Bootloader 提供安全升级的环境（Boot 不动，Bootloader 可以被安全擦写）
  Bootloader → 保护 APP（安全升级 APP）

  Boot 之于 Bootloader = Bootloader 之于 APP
```

---

## 4. 二级 vs 三级：正面对比

### 4.1 能力对比

| 对比项 | 二级 (Bootloader + APP) | 三级 (Boot + Bootloader + APP) |
| :--- | :--- | :--- |
| **APP 可升级** | 可以，安全 | 可以，安全 |
| **Bootloader 可升级** | 可以（APP 直接擦写），**断电变砖** | 可以（Bootloader 搬运），**断电安全** |
| **Boot 可升级** | 不适用 | 代码预留，**设计上不升级** |

**两者唯一的本质区别就是 Bootloader 升级是否断电安全。**

### 4.2 安全性对比

| 场景 | 二级 | 三级 |
| :--- | :--- | :--- |
| **APP 升级断电** | Bootloader 重新搬运，安全 | Bootloader 重新搬运，安全 |
| **Bootloader 升级断电** | **变砖**，无法恢复 | Bootloader 重新搬运，**安全** |
| **Bootloader 有 Bug** | 可以升级修复，但过程有变砖风险 | 可以安全升级修复，无风险 |
| **Boot 有 Bug** | 不适用 | Boot 极简，出 Bug 概率极低 |

### 4.3 资源对比

| 对比项 | 二级 | 三级 |
| :--- | :--- | :--- |
| **Flash 占用** | Bootloader ≈ 16~32KB | Boot 32KB + Bootloader 64KB = 96KB |
| **APP 可用空间 (512KB)** | ≈ 470KB | ≈ 408KB |
| **代码复杂度** | 低 | 中等（多一级跳转和参数管理） |
| **启动耗时** | 1 次跳转 | 2 次跳转（多约 10~50ms，可忽略） |
| **SPI Flash 备份区** | 只需 APP 备份 | 需 Bootloader + APP 备份 |
| **工程数量** | 2 个 (Bootloader + APP) | 3 个 (Boot + Bootloader + APP) |

### 4.4 适用场景

| 场景 | 推荐 | 理由 |
| :--- | :--- | :--- |
| Flash ≤ 128KB | 二级 | Flash 紧张，Boot 层开销不可接受 |
| 产品量小、可物理接触 | 二级 | Bootloader 出 Bug 可以手动烧录 |
| Bootloader 已验证成熟 | 二级 | 不需要改，就不需要安全升级的能力 |
| Flash ≥ 256KB，产品量大 | **三级** | 空间充裕，安全升级 Bootloader 的保险值得买 |
| 产品初期，协议未稳定 | **三级** | Bootloader 很可能需要改 |
| 多子模块 OTA (BMS/INV) | **三级** | Bootloader 中的通讯逻辑可能随子设备变化 |

---

## 5. 什么时候才真正需要升级 Bootloader

三级架构给了你安全升级 Bootloader 的能力，但大部分时候你可能用不到它。以下是实际可能触发的场景：

### 5.1 Flash 驱动变更

换了 SPI Flash 芯片型号（比如从 W25Q32 换到 GD25Q64），新芯片的擦写指令、页大小、地址宽度不同。Bootloader 里的 SPI Flash 驱动就得改。如果旧批次产品已出货，需要推送 Bootloader 更新来兼容新固件格式。

### 5.2 Flash 分区调整

产品迭代后 APP 越来越大，需要调整分区边界。Bootloader 里的地址宏和搬运逻辑都要跟着改。

### 5.3 校验/安全机制升级

比如 CRC16 换 CRC32、加入固件签名验证、加入加密固件解密。这些逻辑都在 Bootloader 里。

### 5.4 修复 Bootloader 自身的 Bug

最现实的场景：
- 搬运逻辑的边界条件没处理好，特定大小的固件搬运出错
- CRC 校验算法 Bug
- 跳转失败后的恢复逻辑有缺陷，导致反复重启
- 看门狗配置不当，搬运大固件时超时复位

### 5.5 增加新功能

- Bootloader 增加串口/CAN 直接接收固件的能力，作为"救砖通道"
- 加入升级进度上报（LED 或串口）
- 适配新的外部子模块通讯协议

### 5.6 现实预期

**大部分产品如果 Bootloader 写得足够稳，可能一辈子都不需要升级它。** 三级架构更多是一个保险，特别是在产品早期迭代、量大难以召回、团队对 Bootloader 测试覆盖不够充分的阶段。等产品成熟了、Bootloader 稳定了，这个能力可能永远不会用到——但它在那里，你就不慌。

---

## 6. Boot 层的设计原则：极简不变

三级架构的可靠性基础在于：**Boot 层足够简单，简单到几乎不可能有 Bug**。

### 6.1 Boot 应该做什么

| 允许 | 不允许 |
| :--- | :--- |
| 初始化最基础的时钟 | 初始化 RTOS |
| 初始化内部 Flash 读写 | 初始化外设（UART、SPI 通讯等） |
| 读取 FirmwareInfoType 参数 | 任何业务逻辑 |
| CRC 校验 | 复杂的协议解析 |
| 跳转到 Bootloader | 与上位机/IOT 通讯 |
| | 显示、按键等人机交互 |
| | 写日志 |

### 6.2 Boot 不可升级：信任根的定位

**Boot 在设计上不参与 OTA 升级。** 它是整个启动链的信任根——如果 Boot 损坏，没有更上层的程序能修复它，设备就变砖了。这和二级架构中 Bootloader 的困境一模一样。

```
Boot 代码量 ≈ 几百行
  → 功能单一，只做"读参数 + 跳转到 Bootloader"
  → 依赖极少：Flash 驱动 + CRC
  → 没有 RTOS、没有通讯、没有业务逻辑
  → 充分测试后出厂锁死，永远不动
```

代码框架中保留了 `cFirmwareUpdateBoot()` 和 `bootOut` 备份区的能力，仅作为**工程完备性预留**（比如产线特殊需求）。正常产品运营中上位机打包固件时**不应包含 Boot 子固件**，从源头上杜绝 Boot 被 OTA 升级的可能。

---

## 7. 我们的实现细节

### 7.1 Flash 分区

```
内部 Flash (512KB)                    外部 SPI Flash
┌──────────────┐                      ┌──────────────────┐
│ Boot  (32KB) │                      │ Boot 备份 (16KB)  │ ← 工程预留
├──────────────┤                      ├──────────────────┤
│ 参数区 (8KB) │                      │ 参数备份 (8KB)    │
├──────────────┤                      ├──────────────────┤
│ BL   (64KB)  │ ←── Bootloader 搬运 ─ │ BL 备份 (64KB)    │
├──────────────┤                      ├──────────────────┤
│ APP  (~408KB)│ ←── Bootloader 搬运 ─ │ APP 备份 (424KB)  │
└──────────────┘                      ├──────────────────┤
                                      │ OTA 数据区 (1MB)  │
                                      ├──────────────────┤
                                      │ Log 等            │
                                      └──────────────────┘
```

OTA 数据区是上位机/IOT 写入的完整固件包。APP 中的 OTA 模块解析后将子固件拷贝到对应的 SPI Flash 备份区，再由 Bootloader 在下次启动时搬运到内部 Flash 运行区。

### 7.2 FirmwareInfoType：分区参数管理

参数管理核心是 `FirmwareInfoType`，存储在内部 Flash 参数区，管理 6 个分区：

```
FirmwareInfoType
├── boot           ← 内部 Flash Boot 区（地址、长度、CRC、版本）
├── bootloader     ← 内部 Flash Bootloader 区
├── app            ← 内部 Flash APP 区
├── bootOut        ← SPI Flash Boot 备份区
├── bootloaderOut  ← SPI Flash Bootloader 备份区
└── appOut         ← SPI Flash APP 备份区
```

每个分区的 `registers` 字段中 bit2 是**更新标志** (`FIRMWARE_UPDATE`)：
- OTA 模块将新固件写入备份区后，置位 `xxxOut.registers |= FIRMWARE_UPDATE`
- Bootloader 启动时检查该标志，有则搬运，搬运完清除标志

### 7.3 升级闭环

```
┌─────────────────────────────────────────────────────────────┐
│                     APP 层 (OTA 模块)                        │
│                                                             │
│  收到新固件包 → 解析子固件 → 按 type/number 写入 SPI Flash:   │
│    number=0 (APP)        → 写入 appOut 备份区                │
│    number=1 (Bootloader) → 写入 bootloaderOut 备份区         │
│  置 FIRMWARE_UPDATE 标志 → NVIC_SystemReset()                │
└──────────────────────────────┬──────────────────────────────┘
                               │ 系统复位
                               ▼
┌─────────────────────────────────────────────────────────────┐
│                      Boot 层                                 │
│                                                             │
│  vFirmwareInit() → 读取 FirmwareInfoType                     │
│  不做搬运，直接跳转到 Bootloader                               │
└──────────────────────────────┬──────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────┐
│                    Bootloader 层                              │
│                                                             │
│  bootloaderOut 有更新标志 ?                                    │
│    → 是：从 SPI Flash 搬运 → 内部 Flash Bootloader 区        │
│    → 搬运完 NVIC_SystemReset()，用新 Bootloader 重新启动      │
│  appOut 有更新标志 ?                                          │
│    → 是：从 SPI Flash 搬运 → 内部 Flash APP 区               │
│  跳转到 APP                                                  │
└──────────────────────────────┬──────────────────────────────┘
                               │
                               ▼
                         APP 正常运行
```

### 7.4 跳转安全机制

每次跳转前都有两道保护：

```c
int8_t cFirmwareJump(FirmwarePortInfoType *pHandle)
{
    if((pHandle->crcValue == FIRMWARE_UNLOCK_CRC) ||
       (uiFirmwareCRCUpdate(pHandle) == pHandle->crcValue))
    {
        cFirmwareJumpTo(pHandle->address);
    }
    return 1;
}
```

如果跳转失败（固件损坏或不存在），会**自动置位对应备份区的更新标志**，下次重启时从 SPI Flash 搬运修复：

```c
int8_t cFirmwareJumpAPP(void)
{
    if((st_typeFirmwareInfo.appOut.registers & FIRMWARE_UPDATE) == 0)
    {
        cFirmwareJump(&st_typeFirmwareInfo.app);
        st_typeFirmwareInfo.appOut.registers |= FIRMWARE_UPDATE;
    }
    return 1;
}
```

---

## 8. 代价与应对

| 代价 | 影响 | 应对 |
| :--- | :--- | :--- |
| Flash 多占 ~32KB | APP 空间从 ~470KB 降到 ~408KB | 512KB 芯片下影响有限 |
| 启动多一次跳转 | 多 10~50ms | 用户无感知 |
| SPI Flash 多一份备份区 | Bootloader 备份 64KB | SPI Flash 通常 ≥ 4MB，可忽略 |
| 多一个 Boot 工程 | 需维护三份工程 | Boot 极简，几乎不改动 |
| 合并烧录 | 出厂需合并三个 HEX | 构建脚本自动合并 |

---

## 9. 总结

**三级架构做的事情很简单：在 Bootloader 上面加了一层 Boot，让 Bootloader 也能像 APP 一样被安全升级。**

```
Boot 之于 Bootloader = Bootloader 之于 APP
```

代价是 32KB Flash + 一个极简的 Boot 工程。换来的是 Bootloader 可以安全远程升级，不怕断电变砖。

这是一份**保险**：
- 产品初期迭代，Bootloader 逻辑还没稳定 → 保险很值
- 产品量大，召回成本高 → 保险很值
- 产品成熟后，Bootloader 已经稳定 → 保险可能永远用不到，但它在那里，你就不慌

```
二级架构的假设：Bootloader 出厂后永远不需要改，或者改了不怕断电。
三级架构的假设：Bootloader 可能需要改，改的时候不能有任何风险。
```
