# STM32F10x RTC 驱动设计文档 (DevicesRTC)

| 属性 | 内容 |
| :--- | :--- |
| **模块名称** | DevicesRTC (硬件RTC驱动模块) |
| **源文件** | `DevicesRTC.c` / `DevicesRTC.h` |
| **硬件依赖** | STM32F103 (RTC, BKP, LSE 32.768kHz) |
| **软件依赖** | DevicesTime (时间戳转换) |
| **版本** | v1.0 |
| **最后更新** | 2026-01-30 |

---

## 1. 设计目标 (Design Goals)

本驱动旨在提供一套完整的硬件 RTC 解决方案：

1. **掉电保持**：利用 STM32 的 BKP 备份域和外部电池，实现掉电后时间不丢失。
2. **高精度计时**：使用 32.768kHz 外部低速晶振 (LSE)，确保长期计时精度。
3. **闹钟功能**：支持设置硬件闹钟，到点触发中断。
4. **统一接口**：支持 UNIX 时间戳和结构体两种方式读写时间，方便上层应用。

---

## 2. 硬件架构 (Hardware Architecture)

### 2.1 RTC 时钟源选择

| 时钟源 | 频率 | 精度 | 掉电保持 | 本驱动选择 |
| :--- | :--- | :--- | :--- | :--- |
| **LSE** | 32.768 kHz | 高 (±20ppm) | ✅ 支持 | ✅ **采用** |
| LSI | ~40 kHz | 低 (±10%) | ✅ 支持 | ❌ |
| HSE/128 | 可变 | 中 | ❌ 不支持 | ❌ |

**选择 LSE 的原因**：
- 32768 = 2^15，分频到 1Hz 只需一个 15 位计数器，硬件实现简单。
- 精度高，适合需要长时间运行的场景（如日历、定时任务）。
- 接电池后，主电源掉电 RTC 仍可运行。

### 2.2 RTC 计数器原理

STM32F1 的 RTC 本质是一个 **32 位向上计数器**：

```
┌─────────────────────────────────────────────────────────┐
│  LSE 32768Hz  ──►  预分频器 (DIV=32767)  ──►  1Hz 脉冲  │
│                                                         │
│  1Hz 脉冲  ──►  32位计数器 (CNT)  ──►  存储 UNIX 时间戳  │
│                                                         │
│  闹钟寄存器 (ALR)  ══  CNT 比较  ══►  闹钟中断          │
└─────────────────────────────────────────────────────────┘
```

- **CNT 寄存器**：存储当前时间（UNIX 时间戳，单位秒）。
- **ALR 寄存器**：存储闹钟时间，当 `CNT == ALR` 时触发中断。
- **DIV 寄存器**：预分频值，自动配置为 `RTC_AUTO_1_SECOND`。

### 2.3 备份域与初始化标志

| 资源 | 用途 |
| :--- | :--- |
| **BKP_DR1** | 存储初始化标志 `0x5A5A`，用于判断是否首次上电 |
| **BKP 备份域** | 掉电保持区域，由 VBAT 电池供电 |

---

## 3. 软件实现细节 (Implementation Details)

### 3.1 初始化流程

```
vRTCInit()
    │
    ├─► 使能 PWR、BKP 时钟
    │
    ├─► 解锁备份域访问 (HAL_PWR_EnableBkUpAccess)
    │
    ├─► HAL_RTC_Init() ──► 自动调用 HAL_RTC_MspInit()
    │                           │
    │                           ├─► 配置 LSE 振荡器
    │                           ├─► 选择 LSE 作为 RTC 时钟源
    │                           └─► 使能 RTC 时钟
    │
    ├─► 检查 BKP_DR1 是否等于 0x5A5A
    │       │
    │       ├─► [否] 首次上电，设置默认时间，写入标志
    │       └─► [是] 非首次上电，跳过
    │
    └─► 配置并使能 RTC 中断
```

### 3.2 时间读写

**写入时间 (vRTCSetTime)**：
```c
/* 进入配置模式 */
while((hrtc.Instance->CRL & RTC_CRL_RTOFF) == 0);  // 等待上次写操作完成
hrtc.Instance->CRL |= RTC_CRL_CNF;                  // 进入配置模式

/* 写入计数器 */
hrtc.Instance->CNTH = (Counter >> 16) & 0xFFFF;
hrtc.Instance->CNTL = Counter & 0xFFFF;

/* 退出配置模式 */
hrtc.Instance->CRL &= ~RTC_CRL_CNF;
while((hrtc.Instance->CRL & RTC_CRL_RTOFF) == 0);  // 等待写操作完成
```

**读取时间 (lRTCGetTime)**：
```c
/* 防止读取时计数器进位导致数据不一致 */
do {
    High1 = hrtc.Instance->CNTH;
    Low   = hrtc.Instance->CNTL;
    High2 = hrtc.Instance->CNTH;
} while(High1 != High2);

return (High1 << 16) | Low;
```

### 3.3 闹钟功能

**设置闹钟 (vRTCSetAlarm)**：
1. 进入配置模式。
2. 写入闹钟寄存器 `ALRH` 和 `ALRL`。
3. 退出配置模式。
4. 清除闹钟标志，使能闹钟中断。

**闹钟触发**：
- 当 `CNT == ALR` 时，硬件置位 `RTC_FLAG_ALRAF`。
- 在 `RTC_IRQHandler` 中检测并处理。

### 3.4 关键函数说明

| 函数名 | 说明 |
| :--- | :--- |
| `vRTCInit` | 初始化 RTC，配置时钟源、检测首次上电、使能中断 |
| `vRTCSetTime` | 设置 RTC 时间（UNIX 时间戳，单位秒） |
| `lRTCGetTime` | 获取 RTC 时间（UNIX 时间戳，单位秒） |
| `vRTCSetTimeByStruct` | 通过时间结构体设置 RTC 时间 |
| `vRTCGetTimeByStruct` | 通过时间结构体获取 RTC 时间 |
| `vRTCSetAlarm` | 设置闹钟时间（UNIX 时间戳） |
| `vRTCSetAlarmByStruct` | 通过时间结构体设置闹钟 |
| `lRTCGetAlarm` | 获取当前闹钟设置 |
| `vRTCCancelAlarm` | 取消闹钟 |

---

## 4. 常见问题与知识点 (Q&A)

### Q1: 为什么读取计数器要用 do-while 循环？

**A**: STM32F1 的 RTC 计数器是 32 位，但分成 `CNTH` (高 16 位) 和 `CNTL` (低 16 位) 两个寄存器。如果在读取过程中发生进位（如从 `0x0000FFFF` 变为 `0x00010000`），可能导致：
- 先读 `CNTH = 0x0000`
- 计数器进位
- 再读 `CNTL = 0x0000`
- 最终得到 `0x00000000`，而不是 `0x00010000`

通过比较两次读取的高位，确保数据一致性。

### Q2: 为什么写入时要等待 RTOFF 标志？

**A**: RTC 运行在独立的低速时钟域 (32.768kHz)，与 APB1 总线时钟不同步。写入操作需要多个 RTC 时钟周期才能完成。`RTOFF = 1` 表示上次写操作已完成，可以进行下一次写入。

### Q3: BKP_DR1 标志位的作用？

**A**: 判断是否首次上电：
- **首次上电**：BKP 寄存器内容随机，需要设置默认时间。
- **复位/唤醒**：BKP 内容保持（有电池），跳过时间设置。

这样避免了每次复位都重置时间。

### Q4: 为什么使用 UNIX 时间戳？

**A**: 
1. **统一标准**：便于与服务器、网络时间同步。
2. **计算简单**：时间差、时间比较只需整数运算。
3. **存储高效**：只需 32 位（或 64 位）即可表示。

---

## 5. 移植与扩展 (Porting Guide)

### 如果 LSE 无法起振

1. 检查硬件：32.768kHz 晶振是否焊接正确，负载电容是否匹配。
2. 备选方案：修改 `HAL_RTC_MspInit`，改用 LSI 作为时钟源：
   ```c
   rcc_oscinitstruct.OscillatorType = RCC_OSCILLATORTYPE_LSI;
   rcc_oscinitstruct.LSIState = RCC_LSI_ON;
   // ...
   rcc_periphclkinitstruct.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
   ```
   **注意**：LSI 精度较差，长期使用会有明显误差。

### 如果需要秒中断

在 `vRTCInit` 中添加：
```c
__HAL_RTC_SECOND_ENABLE_IT(&hrtc, RTC_IT_SEC);
```
然后在 `RTC_IRQHandler` 中处理：
```c
if(__HAL_RTC_GET_FLAG(&hrtc, RTC_FLAG_SEC) != RESET)
{
    __HAL_RTC_CLEAR_FLAG(&hrtc, RTC_FLAG_SEC);
    // 秒中断处理
}
```

### 如果需要支持 2038 年后的时间

当前驱动使用 32 位计数器，最大支持到 **2106 年**（无符号）或 **2038 年**（有符号）。如需更长时间范围，建议：
- 在 BKP 中存储一个"世纪偏移量"。
- 或使用软件 RTC 配合。

---

## 6. 代码质量自检 (Self-Check)

- [x] **时钟配置正确**：LSE 作为时钟源，预分频自动配置为 1 秒。
- [x] **读写安全**：读取使用 do-while 防进位，写入等待 RTOFF。
- [x] **首次上电检测**：使用 BKP 标志位，避免重复初始化。
- [x] **中断配置**：闹钟中断正确使能。
- [ ] **建议**：`DevicesRTC.h` 中添加 `#include "stm32f1xx.h"` 以声明 `RTC_HandleTypeDef`。

---

**文档结束**
