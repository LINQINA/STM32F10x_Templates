# STM32F10x 软件时间驱动设计文档 (DevicesTime)

| 属性 | 内容 |
| :--- | :--- |
| **模块名称** | DevicesTime (软件时间模块) |
| **源文件** | `DevicesTime.c` / `DevicesTime.h` |
| **硬件依赖** | TIM6 (基本定时器，提供微秒级时基) |
| **版本** | v1.0 |
| **最后更新** | 2026-01-30 |

---

## 1. 设计目标 (Design Goals)

本模块提供与硬件 RTC 互补的软件时间服务：

1. **高精度时基**：基于硬件定时器，提供微秒级时间戳，满足精确计时需求。
2. **时间格式转换**：提供 UNIX 时间戳与人类可读时间结构体的双向转换。
3. **时区支持**：支持任意时区设置，自动处理时区偏移。
4. **星期计算**：使用蔡勒公式自动计算星期几。

---

## 2. 模块架构 (Module Architecture)

### 2.1 双时间系统设计

| 时间系统 | 精度 | 用途 | 数据来源 |
| :--- | :--- | :--- | :--- |
| **系统时基** (`g_iTimeBase`) | 微秒 (μs) | 精确计时、延时、性能测量 | TIM6 定时器中断 |
| **实时时钟** (`g_lTimestamp`) | 微秒 (μs) | 日历时间、定时任务 | 软件维护 (可同步 RTC) |

```
┌────────────────────────────────────────────────────────────┐
│                     系统时基 (System Tick)                  │
│  ┌─────────┐      ┌─────────────┐      ┌────────────────┐  │
│  │  TIM6   │ ──►  │ g_iTimeBase │ ──►  │ lTimeGetStamp  │  │
│  │ 中断累加 │      │  (volatile) │      │   (读取接口)    │  │
│  └─────────┘      └─────────────┘      └────────────────┘  │
└────────────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────────────┐
│                     实时时钟 (RealTime)                     │
│  ┌─────────────┐      ┌────────────────┐                   │
│  │ g_lTimestamp│ ◄──► │ vTimestampSet  │ (设置)            │
│  │  (偏移量)   │      │ lTimestampGet  │ (获取)            │
│  └─────────────┘      └────────────────┘                   │
│         │                                                   │
│         └──► RealTime = g_lTimestamp + lTimeGetStamp()     │
└────────────────────────────────────────────────────────────┘
```

### 2.2 时间结构体定义

```c
typedef struct {
    uint16_t year;      // 年 (如 2026)
    uint8_t  month;     // 月 (1-12)
    uint8_t  day;       // 日 (1-31)
    uint8_t  hour;      // 时 (0-23)
    uint8_t  minute;    // 分 (0-59)
    uint8_t  second;    // 秒 (0-59)
    uint8_t  week;      // 星期 (0=周日, 1=周一, ..., 6=周六)
    float    UTC;       // 时区 (东八区 = 8.0, 西五区 = -5.0)
} TimeInfoType;
```

---

## 3. 软件实现细节 (Implementation Details)

### 3.1 系统时基读取

```c
int64_t lTimeGetStamp(void)
{
    int64_t iTimeBaseNow = 0;
    uint32_t now = 0;

    do {
        iTimeBaseNow = g_iTimeBase;     // 读取中断累加值
        now = TIM6->CNT;                // 读取当前计数器值
    } while(iTimeBaseNow != g_iTimeBase);  // 防止中断竞态

    return iTimeBaseNow + now;
}
```

**竞态条件处理**：
- 如果在读取 `g_iTimeBase` 和 `TIM6->CNT` 之间发生中断，`g_iTimeBase` 会被更新。
- 通过 do-while 循环重新读取，确保数据一致性。

### 3.2 UNIX 时间戳转时间结构体 (vStampToTime)

**算法流程**：
```
输入: UNIX 时间戳 (秒), 时区
    │
    ├─► 加上时区偏移 (UTC * 3600)
    │
    ├─► 计算年份 (累减每年天数 × 86400)
    │
    ├─► 计算月份 (累减每月天数 × 86400)
    │
    ├─► 计算日期 (剩余秒数 / 86400 + 1)
    │
    ├─► 计算时分秒 (取模运算)
    │
    └─► 计算星期 (蔡勒公式)
```

**关键常量**：
- `86400` = 24 × 60 × 60 = 一天的秒数
- `3600` = 60 × 60 = 一小时的秒数

### 3.3 时间结构体转 UNIX 时间戳 (lTimeToStamp)

**算法流程**：
```
输入: 时间结构体
    │
    ├─► 累加从 1970 年到目标年份的天数
    │
    ├─► 累加从 1 月到目标月份的天数
    │
    ├─► 加上日期 (day - 1)
    │
    ├─► 天数 × 86400 + 时 × 3600 + 分 × 60 + 秒
    │
    └─► 减去时区偏移 (UTC * 3600)
```

### 3.4 蔡勒公式 (Zeller's Formula)

用于根据日期计算星期几：

```c
uint8_t cTimeToWeek(int32_t iYear, uint8_t ucMonth, uint8_t ucDay)
{
    int32_t iCentury = 0;
    int8_t cWeek = 0;

    /* 1、2 月当作上一年的 13、14 月处理 */
    if(ucMonth < 3) {
        iYear -= 1;
        ucMonth += 12;
    }

    iCentury = iYear / 100;  // 世纪数
    iYear %= 100;            // 年份后两位

    /* 蔡勒公式 */
    cWeek = ((iCentury / 4) - (iCentury * 2) + iYear + (iYear / 4) 
             + (13 * (ucMonth + 1) / 5) + ucDay - 1) % 7;

    return ((cWeek < 0) ? (cWeek + 7) : cWeek);
}
```

**返回值**：0 = 周日，1 = 周一，...，6 = 周六

### 3.5 闰年判断宏

```c
#define YEAR_LEAP(year) ((((year) % 4 == 0) && ((year) % 100 != 0)) || ((year) % 400 == 0))
```

**规则**：
- 能被 4 整除 **且** 不能被 100 整除 → 闰年
- 或者能被 400 整除 → 闰年

### 3.6 关键函数说明

| 函数名 | 说明 |
| :--- | :--- |
| `lTimeGetStamp` | 获取系统时基（微秒级，从开机开始累计） |
| `vStampToTime` | UNIX 时间戳 → 时间结构体，支持时区 |
| `lTimeToStamp` | 时间结构体 → UNIX 时间戳，自动处理时区 |
| `cTimeToWeek` | 蔡勒公式计算星期几 |
| `vTimestampSet` | 设置实时时钟（微秒） |
| `lTimestampGet` | 获取实时时钟（微秒） |
| `vRealTimeUTCSet` | 设置默认时区 |
| `fRealTimeUTCGet` | 获取当前时区设置 |

---

## 4. 常见问题与知识点 (Q&A)

### Q1: 为什么系统时基用 volatile？

**A**: `g_iTimeBase` 在定时器中断中被修改，在主程序中被读取。如果不加 `volatile`：
- 编译器可能优化掉重复读取，将值缓存到寄存器。
- 导致主程序读取的值不是最新的。

### Q2: 为什么时间戳用 int64_t 而不是 uint32_t？

**A**: 
- `uint32_t` 最大值 ≈ 136 年，从 1970 年算起只能到 2106 年。
- 更重要的是，`int64_t` 可以表示 **1970 年之前** 的负时间戳。
- 微秒级时基需要更大的数值范围。

### Q3: 时区为什么用 float 而不是 int？

**A**: 某些地区使用非整数时区：
- 印度：UTC+5:30 (5.5)
- 尼泊尔：UTC+5:45 (5.75)
- 澳大利亚部分地区：UTC+9:30 (9.5)

使用 `float` 可以精确表示这些时区。

### Q4: vStampToTime 中负时间戳的处理？

**A**: 负时间戳表示 1970 年 1 月 1 日之前的时间。处理逻辑：
- 从 1970 年向前累加年份天数（减法变加法）。
- 从 12 月向前累加月份天数。

**注意**：当前实现在边界情况可能存在数组越界风险，建议限制输入范围或完善边界检查。

---

## 5. 与 DevicesRTC 的协作 (Integration)

### 5.1 模块职责划分

| 功能 | DevicesRTC | DevicesTime |
| :--- | :--- | :--- |
| 硬件时钟访问 | ✅ | ❌ |
| 掉电保持 | ✅ | ❌ |
| 闹钟功能 | ✅ | ❌ |
| 时间格式转换 | ❌ (调用 DevicesTime) | ✅ |
| 时区处理 | ❌ (调用 DevicesTime) | ✅ |
| 微秒级计时 | ❌ | ✅ |
| 星期计算 | ❌ (调用 DevicesTime) | ✅ |

### 5.2 典型调用流程

```c
/* 从 RTC 读取时间并转换为结构体 */
TimeInfoType time;
int64_t stamp = lRTCGetTime();           // DevicesRTC: 读取硬件 RTC
vStampToTime(stamp, &time, 8.0f);        // DevicesTime: 转换为北京时间

/* 设置时间到 RTC */
TimeInfoType newTime = {2026, 1, 30, 12, 0, 0, 0, 8.0f};
int64_t newStamp = lTimeToStamp(&newTime);  // DevicesTime: 转换为时间戳
vRTCSetTime(newStamp);                      // DevicesRTC: 写入硬件 RTC
```

---

## 6. 移植与扩展 (Porting Guide)

### 如果使用其他定时器作为时基

修改 `lTimeGetStamp` 中的定时器引用：
```c
// 原来
now = TIM6->CNT;

// 改为 TIM7
now = TIM7->CNT;
```

同时确保定时器中断正确更新 `g_iTimeBase`。

### 如果需要毫秒级而非微秒级精度

调整时基单位：
```c
// 中断周期从 1μs 改为 1ms
// g_iTimeBase 单位变为毫秒
```

### 如果需要支持夏令时 (DST)

可以在 `TimeInfoType` 中增加 DST 标志：
```c
typedef struct {
    // ... 原有字段
    uint8_t dst;  // 夏令时标志: 0=标准时间, 1=夏令时(+1小时)
} TimeInfoType;
```

在转换函数中根据 `dst` 额外加减一小时。

---

## 7. 代码质量自检 (Self-Check)

- [x] **竞态处理**：`lTimeGetStamp` 使用 do-while 防止中断竞态。
- [x] **时区支持**：使用 float 支持非整数时区。
- [x] **闰年处理**：宏定义正确处理 4/100/400 年规则。
- [x] **空指针检查**：转换函数检查 `ptypeTime == NULL`。
- [ ] **建议**：`DevicesTime.h` 中添加 `#include "stdint.h"`。
- [ ] **建议**：完善负时间戳的边界检查，避免数组越界。

---

## 8. 辅助宏定义说明

```c
/* 判断是否为闰年 */
#define YEAR_LEAP(year) ((((year) % 4 == 0) && ((year) % 100 != 0)) || ((year) % 400 == 0))

/* 获取年份天数 */
#define DAYS_OF_THE_YEAR(year) (YEAR_LEAP(year) != 0 ? 366 : 365)

/* 获取月份天数 (month: 1-12) */
#define DAYS_OF_THE_MONTH(year, month) \
    ((((month) == 2) && (YEAR_LEAP(year) != 0)) ? 29 : st_ucMonthDays[(month) - 1])
```

**月份天数数组**：
```c
const static uint8_t st_ucMonthDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
//                                        1   2   3   4   5   6   7   8   9  10  11  12
```

---

**文档结束**
