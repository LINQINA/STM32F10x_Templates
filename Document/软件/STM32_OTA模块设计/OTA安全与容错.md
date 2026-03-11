# OTA 安全与容错

| 属性 | 内容 |
| :--- | :--- |
| **主题** | OTA 升级防砖机制与极限工况处理 |
| **相关源文件** | `DriverBootloader.c/h`、`DriverOTA.c/h`、`DriverUpgradePD.c` |
| **版本** | v1.0 |
| **最后更新** | 2026-02-27 |

---

## 1. 一句话总结

**整套 OTA 的防砖核心原则是：永远不直接修改正在运行区域的代码，永远先写备份区再搬运，搬运失败了还能重来。**

---

## 2. 防线总览

从固件包进入设备到最终运行新固件，经过了 **6 道防线**。任何一道拦住了错误，后面的环节都不会被触发，设备保持原有固件正常运行。

```
上位机下发 → [防线1] 帧级 CRC → SPI Flash OTA 区
                                    │
触发升级   → [防线2] 总包头 CRC     │
           → [防线3] 整包数据 CRC   │
                                    │
子固件搬运 → [防线4] 子固件 CRC     │
           → [防线5] 搬运后验证     │
                                    │
跳转运行   → [防线6] CRC + 栈指针检查
```

---

## 3. 防线 1：帧级 CRC（上位机 → SPI Flash）

**位置：** `cOTAModbusPackAnalysis()`

上位机每发送一帧固件数据，帧内都携带了本帧数据的 CRC16-Modbus 校验值。设备收到后先校验，通过才写入 SPI Flash。

```c
if(uiCRCValue == usCRC16_MODBUS(NULL, &ptypeHandle->data[17], iLength))
{
    cError = cSPIFlashWriteDatas(SPI_FLASH_OTA_ADDR + uiAddr, &ptypeHandle->data[17], iLength);
}
```

**防住了什么：** 传输过程中的比特翻转、丢字节、数据错乱。一帧数据哪怕错了 1 个 bit，都不会写入 SPI Flash。

**如果没有这道防线：** 错误数据写入 SPI Flash，后续整包 CRC 虽然也能拦住，但已经污染了 OTA 数据区，需要重新全量下发。

---

## 4. 防线 2：总包头 CRC（触发升级时校验）

**位置：** `uiOTACheck()` → 被 `vOTAStart()` 调用

固件包全部写入 SPI Flash 后，上位机触发升级（写 0x0054 = 1）。设备首先读取 OTA 数据区开头的 256 字节总包头，校验前 252 字节的 CRC：

```c
cSPIFlashReadDatas(SPI_FLASH_OTA_ADDR, &st_typeOTAFirmwareTotalInfo, sizeof(OTAFirmwareTotalInfoType));

if(st_typeOTAFirmwareTotalInfo.crc32Head != usCRC16_MODBUS(&usCrc, &st_typeOTAFirmwareTotalInfo, sizeof(st_typeOTAFirmwareTotalInfo) - 4))
    uiError |= OTA_Error_CrcTotalHeaderError;
```

**防住了什么：** 总包头被篡改或损坏（子固件数量、地址偏移、版本号等关键信息错误）。如果总包头不对，后面解析子固件的地址和长度就全乱了。

**校验失败后果：** 状态直接设为 FAIL，不会触发任何搬运操作，设备继续运行原有 APP。

---

## 5. 防线 3：整包数据 CRC（触发升级时校验）

**位置：** `uiOTACheck()`

总包头 CRC 通过后，紧接着校验总包头之后的所有子固件数据：

```c
typeFirmwareOTA.address = SPI_FLASH_OTA_ADDR + sizeof(OTAFirmwareTotalInfoType);
typeFirmwareOTA.length = st_typeOTAFirmwareTotalInfo.length;
typeFirmwareOTA.registers = FIRMWARE_SOURCE_SPI_FLASH;

if(st_typeOTAFirmwareTotalInfo.crc32 != uiFirmwareCRCUpdate(&typeFirmwareOTA))
    uiError |= OTA_Error_CrcTotalFirmwareError;
```

**防住了什么：** 固件包整体不完整——比如上位机少发了几帧、中间某帧被跳过、或者 SPI Flash 写入时扇区擦除异常。

**校验失败后果：** 同上，FAIL，不搬运，不重启。

---

## 6. 防线 4：子固件 CRC（搬运前校验）

**位置：** `cOTAFirmwareUpdatePort()`

每个子固件开始搬运前，先独立校验该子固件在 SPI Flash OTA 区中的数据：

```c
typeFirmwareOTA.address = ptypeOTAFirmwarePortInfo->address;
typeFirmwareOTA.length = ptypeOTAFirmwarePortInfo->length;
typeFirmwareOTA.registers = FIRMWARE_SOURCE_SPI_FLASH;

if(ptypeOTAFirmwarePortInfo->crcValue == uiFirmwareCRCUpdate(&typeFirmwareOTA))
{
    /* CRC 通过，执行升级 */
}
```

**防住了什么：** 即使整包 CRC 通过了，单个子固件也可能因为地址偏移计算错误、长度字段异常等原因导致实际数据不匹配。这是"信任但验证"的最后一道关口。

**校验失败后果：** 该子固件标记为失败，如果有重试次数则重试，否则跳过。不影响其他子固件的升级。

---

## 7. 防线 5：搬运后验证（写完再读回校验）

**位置：** `cFirmwareUpdate()`

这是整个防砖体系中最关键的一环。固件从 SPI Flash 备份区搬运到内部 Flash 运行区后，**立刻读回目标区域重新计算 CRC**，与源固件的 CRC 比较：

```c
/* 搬运完成后 */
pTarget->length = pSource->length;
if(uiFirmwareCRCUpdate(pTarget) != pSource->crcValue)
{
    cLogPrintfNormal("固件拷贝后，校验目标区域新固件CRC失败.\r\n");
    return 5;
}
```

**防住了什么：** 内部 Flash 写入异常（扇区擦除不彻底、写入时电压波动、Flash 老化导致的坏块）。数据搬过去了不等于搬对了，必须读回验证。

**校验失败后果：** 搬运操作返回失败，更新标志不会被清除，下次重启时 Bootloader 会重新搬运。

**另外一个巧妙的优化：** 搬运前也会先比较源和目标的 CRC，如果一样就直接返回成功，避免重复搬运：

```c
if(uiFirmwareCRCUpdate(pTarget) == pSource->crcValue)
    return 0;
```

---

## 8. 防线 6：跳转安全（CRC + 栈指针双重检查）

**位置：** `cFirmwareJump()` → `cFirmwareJumpTo()`

搬运完成后，程序需要跳转到新固件执行。跳转前有两道保护：

**第一道：CRC 校验**

```c
if((pHandle->crcValue == FIRMWARE_UNLOCK_CRC) || (uiFirmwareCRCUpdate(pHandle) == pHandle->crcValue))
{
    cFirmwareJumpTo(pHandle->address);
}
```

**第二道：栈指针合法性检查**

```c
uiHeapData = *(volatile uint32_t*)uiAddress;
if((SRAM_BASE < uiHeapData) && (uiHeapData <= (SRAM_BASE + 1024 * 64)))
{
    /* 栈指针在 SRAM 范围内，可以安全跳转 */
}
```

固件的第一个字（地址 0x00）是初始栈指针（MSP），必须指向 SRAM 范围内。如果 Flash 区域是空的（全 0xFF）或者数据损坏，MSP 值不会在 SRAM 范围内，跳转不会发生。

**防住了什么：** 跳转到一片空白或损坏的 Flash 区域，导致程序跑飞、Hard Fault。

---

## 9. 跳转失败自动恢复

这是整套防砖体系最精彩的部分。

**位置：** `cFirmwareJumpAPP()` / `cFirmwareJumpBootloader()`

如果跳转失败了（CRC 不过或栈指针非法），程序不会卡死，而是**自动置位对应备份区的更新标志**：

```c
int8_t cFirmwareJumpAPP(void)
{
    if((st_typeFirmwareInfo.appOut.registers & FIRMWARE_UPDATE) == 0)
    {
        cFirmwareJump(&st_typeFirmwareInfo.app);

        /* 走到这里说明跳转失败 */
        st_typeFirmwareInfo.appOut.registers |= FIRMWARE_UPDATE;
    }
    return 1;
}
```

这意味着：
1. Bootloader 尝试跳转 APP → 失败
2. 自动给 `appOut` 置更新标志
3. 下次重启时，Bootloader 检测到更新标志，从 SPI Flash 备份区重新搬运 APP
4. 如果备份区的固件是好的 → 修复成功
5. 如果备份区的固件也是坏的 → Bootloader 会一直停留在升级模式，等待上位机重新下发固件

**同样的机制也保护 Bootloader 跳转：**

```c
int8_t cFirmwareJumpBootloader(void)
{
    if((st_typeFirmwareInfo.bootloaderOut.registers & FIRMWARE_UPDATE) == 0)
    {
        cFirmwareJump(&st_typeFirmwareInfo.bootloader);

        /* 跳转失败，置位更新标志 */
        st_typeFirmwareInfo.bootloaderOut.registers |= FIRMWARE_UPDATE;
    }
    return 1;
}
```

---

## 10. 断电恢复

### 10.1 核心原理：更新标志的"先清后做"与"先写后做"

搬运流程中的关键操作顺序：

**APP 层（OTA 模块写入 SPI Flash 备份区）：**

```
1. OTA 模块将子固件从 OTA 数据区拷贝到 SPI Flash 备份区
2. 拷贝成功 → 置位 FIRMWARE_UPDATE 标志
3. 写入 FirmwareInfoType 到 Flash 参数区
4. NVIC_SystemReset()
```

如果在步骤 1 中断电：备份区数据不完整，但更新标志没有置位 → 下次启动不会搬运 → 安全

如果在步骤 2-3 中断电：标志没写进 Flash → 下次启动不会搬运 → 安全（需要重新 OTA）

如果在步骤 4 中断电：标志已写入 → 下次启动正常搬运 → 安全

**Bootloader 层（从 SPI Flash 搬运到内部 Flash）：**

```
1. 检测到更新标志
2. 先清除更新标志（在内存中，但还没写入 Flash）
3. 从 SPI Flash 备份区搬运到内部 Flash 运行区
4. 搬运后 CRC 验证
5. 验证通过 → 写入 FirmwareInfoType（更新标志已清除）
6. NVIC_SystemReset()
```

关键点：`cFirmwareUpdateBoot()` / `cFirmwareUpdateAPP()` 中先在内存里清了标志，但搬运成功后才调用 `cFirmwareWrite()` 写入 Flash。如果搬运过程中断电：

- Flash 中的更新标志**还在**（因为还没写进去）
- 下次启动检测到更新标志 → 重新搬运 → 安全

### 10.2 OTA 状态持久化

`OTAInfoType` 整个结构体（包含主状态、每个子固件的状态、重试次数）都持久化存储在内部 Flash（`FLASH_OTA_DATA_ADDR`），每次状态变更都写入 Flash：

```c
int8_t cOTAUpdateCrcAndWriteToFlash()
{
    st_typeOTAInfo.OTATotalInfoCrc = uiCRC32(NULL, &st_typeOTAInfo, sizeof(OTAInfoType) - 4);
    return cFlashWriteDatas(FLASH_OTA_DATA_ADDR, &st_typeOTAInfo, sizeof(OTAInfoType));
}
```

OTA 状态结构体末尾有 CRC32 校验值。如果断电导致写入不完整，下次启动时 CRC 不匹配，会触发恢复流程：

```c
void vOTAInit(void)
{
    uiCrcValue = uiCRC32(NULL, &st_typeOTAInfo, sizeof(OTAInfoType) - 4);

    if(st_typeOTAInfo.OTATotalInfoCrc == 0xFFFFFFFF)
    {
        /* 全 FF → HEX 烧录后的初始状态 → 清零 */
        memset(&st_typeOTAInfo, 0, sizeof(st_typeOTAInfo));
    }
    else if(st_typeOTAInfo.OTATotalInfoCrc != uiCrcValue)
    {
        /* CRC 不一致 → 可能断电了 → 重新触发升级流程 */
        cOTAStateSet(OTA_STATE_START);
    }
}
```

---

## 11. 重试机制

**位置：** `vOTAFirmwareUpdateAll()`

每个子固件有 `reSendCount` 次重试机会（由总包头指定，建议 3 次）：

```
子固件 #i 开始升级
  │
  ├── 升级成功 → state = SUCCESS，清零重试次数
  │
  └── 升级失败 → 重试次数 - 1
      │
      ├── 还有次数 → 重新升级（goto __UPDATE_BEGIN）
      │
      └── 次数用完 → state = FAIL，记录错误码
```

断电恢复时，如果检测到子固件处于 UPDATING 状态（说明上次升级到一半断电了），也会检查重试次数：有次数则继续升级，没有则标记失败。

---

## 12. 固件类型验证

**位置：** `cOTAUpgradePD()`

PD 本机固件在搬运前，会额外读取固件数据中的类型标识字段进行校验：

```c
cSPIFlashReadDatas(ptypeOTAFirmwarePortInfo->address + 0x860, ucTypeBuff, 4);

if(memcmp(ucTypeBuff, "MySTM32", 4) != 0)
    return 2;
```

**防住了什么：** 错误的固件包（比如 BMS 的固件被错误地标记为 PD 类型）。即使 CRC 都通过了，固件内部的类型标识不对也会被拒绝，防止把错误的程序写入本机运行区。

---

## 13. 物理隔离：为什么搬运不会变砖

搬运过程的安全性基于一个简单的事实：**正在运行的程序和被擦写的区域不是同一块 Flash**。

| 搬运操作 | 执行者 | 执行者所在区域 | 被擦写区域 | 断电后果 |
| :--- | :--- | :--- | :--- | :--- |
| APP → SPI Flash 备份区 | APP | 内部 Flash APP 区 | 外部 SPI Flash | APP 不受影响 |
| SPI Flash → 内部 Flash APP 区 | Bootloader | 内部 Flash BL 区 (0x0800A000) | 内部 Flash APP 区 (0x0801A000) | Bootloader 不受影响 |
| SPI Flash → 内部 Flash Boot 区 | Bootloader | 内部 Flash BL 区 (0x0800A000) | 内部 Flash Boot 区 (0x08000000) | Bootloader 不受影响 |

每一步操作者和被操作区域都**物理隔离**。断电只会导致"写了一半"，不会损坏正在执行的程序。下次上电检测到更新标志还在，重新搬运即可。

---

## 14. 极限工况分析

### 14.1 上位机下发过程中断电

```
SPI Flash OTA 数据区写了一半
  → 下次上电，APP 正常启动
  → OTA 状态仍为 DISABLE（因为还没写 0x0054 = 1）
  → 设备正常工作，上位机可以重新下发

结论：安全，不变砖
```

### 14.2 触发升级后、校验阶段断电

```
写了 0x0054 = 1，OTA 状态变为 START
  → vOTAStart() 正在做 CRC 校验
  → 断电
  → 下次上电，OTA 状态从 Flash 恢复为 START
  → vOTAStart() 重新执行校验
  → 如果数据完整 → 继续升级
  → 如果数据不完整（上位机下发不全） → CRC 校验失败 → FAIL → 安全

结论：安全，自动恢复或安全失败
```

### 14.3 APP 搬运子固件到 SPI Flash 备份区时断电

```
APP 正在把 OTA 数据区的子固件拷贝到 SPI Flash 备份区
  → 断电
  → SPI Flash 备份区数据不完整
  → 但 FIRMWARE_UPDATE 标志还没置位（还没执行到那一步）
  → 下次上电，Bootloader 不会搬运这个不完整的备份
  → OTA 状态从 Flash 恢复，根据重试次数决定重新执行或标记失败

结论：安全，不会搬运不完整的数据
```

### 14.4 APP 已置位更新标志、重启前断电

```
APP 已经成功拷贝子固件到备份区
  → 已置位 FIRMWARE_UPDATE
  → 已写入 FirmwareInfoType
  → 正要 NVIC_SystemReset() 时断电
  → 下次上电，Bootloader 检测到更新标志 → 正常搬运

结论：安全，等同于正常重启
```

### 14.5 Bootloader 搬运内部 Flash 过程中断电（最危险的场景）

```
Bootloader 正在从 SPI Flash 备份区搬运 APP 到内部 Flash
  → 搬运到一半断电
  → 内部 Flash APP 区数据不完整
  → 但 Bootloader 本身不受影响（不在 APP 区）

下次上电：
  Boot → 跳转 Bootloader（正常）
  Bootloader → 尝试跳转 APP → CRC 失败 → 跳转失败
    → 自动置位 appOut 的更新标志
    → Bootloader 进入升级模式
    → 重新从 SPI Flash 备份区搬运 APP → 成功

结论：安全，自动修复
```

### 14.6 Bootloader 搬运完成、写参数时断电

```
Bootloader 搬运完成，CRC 验证通过
  → 正在调用 cFirmwareWrite() 写入参数
  → 断电
  → Flash 中的更新标志可能还在（写入不完整）

下次上电：
  → Bootloader 看到更新标志 → 重新搬运
  → cFirmwareUpdate() 发现源和目标 CRC 一致 → 直接返回成功（不重复写）
  → 更新标志清除，正常跳转

结论：安全，重复搬运会被 CRC 比较短路
```

### 14.7 连续多次断电

```
假设设备在升级过程中反复断电（极端工况）：

每次断电：
  → Bootloader 检测更新标志 → 尝试搬运 → 搬了一半又断电
  → 下次上电 → 更新标志还在 → 再次搬运
  → 直到搬运完成（搬运时间通常 <2 秒，连续断电概率极低）

期间 Bootloader 本身永远不受影响，因为它运行在独立的 Flash 区域。

结论：理论上安全，实际中极罕见
```

### 14.8 SPI Flash 备份区数据损坏

```
SPI Flash 芯片故障或数据被意外擦除
  → Bootloader 搬运时，cFirmwareUpdate() 先校验源固件 CRC
  → CRC 不匹配 → 搬运不会执行 → 返回错误
  → Bootloader 尝试跳转 APP → 如果原 APP 仍完好 → 正常运行
  → 如果原 APP 也损坏 → 跳转失败 → 停留在 Bootloader 等待重新 OTA

结论：不会把错误数据写入运行区
```

### 14.9 上位机发了一个完全错误的固件

```
固件包格式正确（CRC 都通过），但内容是另一个产品的固件
  → 通过防线 1-5
  → cOTAUpgradePD() 中的固件类型验证 → "MySTM32" 标识不匹配 → 拒绝
  → 如果标识碰巧匹配 → 搬运到运行区 → 跳转时栈指针检查可能拦住
  → 如果栈指针也碰巧合法 → 跳转到错误的程序 → 这是唯一无法完全防住的场景

应对：上位机侧应做好固件与设备的匹配校验
```

---

## 15. 安全机制总表

| 防线 | 位置 | 校验内容 | 防住了什么 | 失败后果 |
| :--- | :--- | :--- | :--- | :--- |
| 帧级 CRC | `cOTAModbusPackAnalysis` | 每帧传输数据 | 传输错误 | 该帧丢弃，不写入 SPI Flash |
| 总包头 CRC | `uiOTACheck` | 总包头 252 字节 | 包头损坏/篡改 | OTA 失败，不搬运 |
| 整包数据 CRC | `uiOTACheck` | 所有子固件数据 | 固件包不完整 | OTA 失败，不搬运 |
| 子固件 CRC | `cOTAFirmwareUpdatePort` | 单个子固件数据 | 子固件损坏 | 该子固件失败，可重试 |
| 搬运后验证 | `cFirmwareUpdate` | 目标区域写入结果 | Flash 写入异常 | 搬运失败，下次重试 |
| 跳转检查 | `cFirmwareJump/JumpTo` | CRC + 栈指针 | 目标固件损坏/不存在 | 跳转失败，自动置更新标志 |
| 固件类型验证 | `cOTAUpgradePD` | 固件内嵌标识 | 错误固件 | 拒绝搬运 |
| 断电恢复 | 更新标志 + 状态持久化 | 操作完整性 | 升级中断电 | 下次上电自动继续 |
| 重试机制 | `vOTAFirmwareUpdateAll` | 升级失败计数 | 偶发失败 | 自动重试 N 次 |

---

## 16. 真正能变砖的场景

诚实地说，仍然存在极少数无法通过软件完全防住的变砖场景：

| 场景 | 原因 | 概率 | 应对 |
| :--- | :--- | :--- | :--- |
| 内部 Flash 硬件故障 | Boot 区或参数区的 Flash 物理损坏 | 极低 | 硬件问题，只能物理烧录 |
| SPI Flash 芯片故障 | 备份区数据全部丢失，且运行区也损坏 | 极低 | 预防：选用可靠的 SPI Flash 芯片 |
| Boot 程序有 Bug | Boot 是信任根，不可升级 | 极低（代码极简） | 出厂前充分测试 |
| 参数区 Flash 损坏 | `FirmwareInfoType` 读不出来，分区信息丢失 | 极低 | `vFirmwareInit()` 中有 CRC 校验，失败则回退到出厂默认参数 |

这些场景的共同特征是**硬件级故障**，任何软件方案都无法完全防御，只能通过硬件选型和出厂测试来降低风险。

---

## 17. 设计哲学

整套防砖机制遵循三个原则：

```
1. 不确认就不动
   → 每一步操作前都先 CRC 校验，不确认数据正确就不执行下一步

2. 写了就验证
   → 搬运到目标区域后立刻读回验证，不盲目信任 Flash 写入操作

3. 失败就重来
   → 更新标志的设计保证断电后可以重新搬运
   → 跳转失败自动置标志，下次重启触发修复
   → 每个子固件有独立的重试次数
```
