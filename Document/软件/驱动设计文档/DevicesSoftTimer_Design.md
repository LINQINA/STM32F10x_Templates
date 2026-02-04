# STM32F10x 软件定时器设计文档 (DevicesSoftTimer)

| 属性 | 内容 |
| :--- | :--- |
| **模块名称** | DevicesSoftTimer (软件定时器模块) |
| **源文件** | `DevicesSoftTimer.c` / `DevicesSoftTimer.h` |
| **硬件依赖** | 无 (依赖 DevicesTime 提供时间基准) |
| **版本** | v1.0 |
| **最后更新** | 2026-01-30 |

---

## 1. 设计目标 (Design Goals)

本模块提供不依赖硬件定时器的软件定时功能：

1. **无硬件限制**：可创建任意数量的软定时器，仅受内存限制。
2. **微秒级精度**：基于 `DevicesTime` 的微秒时间戳，精度高。
3. **轻量级设计**：每个定时器仅需一个结构体实例，无需注册/回调机制。
4. **灵活的重载策略**：支持 Reset（间隔型）和 Reload（周期型）两种模式。

---

## 2. 模块架构 (Module Architecture)

### 2.1 核心原理

软定时器采用"绝对时刻比较法"：

```
┌─────────────────────────────────────────────────────────────┐
│                      软件定时器原理                          │
│                                                             │
│   设置定时器:  timeStop = now + duration                    │
│                                                             │
│   查询状态:    now >= timeStop ?                            │
│                   │                                         │
│                   ├── Yes → 返回 softTimerOver (超时)       │
│                   └── No  → 返回 softTimerOpen  (计时中)    │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 定时器状态枚举

```c
typedef enum {
    softTimerClose = 0x00,     /* 定时器已关闭 */
    softTimerOpen  = 0x01,     /* 正常计时当中 */
    softTimerOver  = 0x02,     /* 定时器溢出 */
    softTimerError = 0x08,     /* 定时器错误 */
} SoftTimerStateEnum;
```

### 2.3 定时器结构体

```c
typedef struct {
    int64_t timeStop;           /* 定时结束时刻 (μs) */
    int64_t timeDuration;       /* 定时时长 (μs) */
    SoftTimerStateEnum state;   /* 状态 */
} SoftTimerTypeDef;
```

---

## 3. 软件实现细节 (Implementation Details)

### 3.1 时间基准获取

```c
static int64_t lSoftTimerGetNow(void)
{
    return lTimeGetStamp();
}
```

封装为静态函数，便于后续更换时间源。

### 3.2 设置定时器 (cSoftTimerSet)

```c
int8_t cSoftTimerSet(SoftTimerTypeDef *ptypeTimer, int64_t lTime, SoftTimerStateEnum state)
{
    if(ptypeTimer == NULL)
        return -1;

    ptypeTimer->timeStop = lSoftTimerGetNow() + lTime;
    ptypeTimer->timeDuration = lTime;
    cSoftTimerSetState(ptypeTimer, state);

    return 0;
}
```

**便捷宏定义**：

```c
#define cSoftTimerSetUs(ptypeTimer, lTime, state)       cSoftTimerSet((ptypeTimer), (lTime), (state))
#define cSoftTimerSetMs(ptypeTimer, lTime, state)       cSoftTimerSet((ptypeTimer), (lTime) * 1000ll, (state))
#define cSoftTimerSetSecond(ptypeTimer, lTime, state)   cSoftTimerSet((ptypeTimer), (lTime) * 1000000ll, (state))
#define cSoftTimerSetMinute(ptypeTimer, lTime, state)   cSoftTimerSet((ptypeTimer), (lTime) * (1000000ll * 60), (state))
#define cSoftTimerSetHour(ptypeTimer, lTime, state)     cSoftTimerSet((ptypeTimer), (lTime) * (1000000ll * 60 * 60), (state))
```

### 3.3 Reset vs Reload (核心区别)

| 函数 | 计算方式 | 语义 |
| :--- | :--- | :--- |
| `cSoftTimerReset` | `timeStop = now + duration` | 从**现在**重新开始计时 |
| `cSoftTimerReload` | `timeStop += duration` | 从**上次到期时刻**累加 |

```c
/* Reset: 间隔型 - 保证两次执行间隔至少为 duration */
int8_t cSoftTimerReset(SoftTimerTypeDef *ptypeTimer)
{
    ptypeTimer->timeStop = lSoftTimerGetNow() + ptypeTimer->timeDuration;
    ptypeTimer->state = softTimerOpen;
    return 0;
}

/* Reload: 周期型 - 保证长期频率稳定，可能追赶触发 */
int8_t cSoftTimerReload(SoftTimerTypeDef *ptypeTimer)
{
    ptypeTimer->timeStop += ptypeTimer->timeDuration;
    ptypeTimer->state = softTimerOpen;
    return 0;
}
```

**图示说明**：

```
假设 duration = 100ms，定时器在 t=100 到期，但在 t=120 才处理：

Reset:  下次到期 = 120 + 100 = 220
        |----100----|----100----|
        0          120         220

Reload: 下次到期 = 100 + 100 = 200 (已过期，立刻再次触发)
        |----100----|----100----|
        0          100         200
                    ^ 在 t=120 处理时发现已过期
```

### 3.4 查询定时器状态 (enumSoftTimerGetState)

```c
SoftTimerStateEnum enumSoftTimerGetState(SoftTimerTypeDef *ptypeTimer)
{
    if(ptypeTimer == NULL)
        return softTimerError;

    if((ptypeTimer->state & softTimerOpen) == 0)
        return softTimerClose;

    if(lSoftTimerGetNow() >= ptypeTimer->timeStop)
        return softTimerOver;

    return softTimerOpen;
}
```

**判断优先级**：
1. 空指针 → 返回 Error
2. 未开启 (state 无 Open 位) → 返回 Close
3. 当前时间 ≥ 到期时刻 → 返回 Over
4. 否则 → 返回 Open

### 3.5 关键函数说明

| 函数名 | 说明 |
| :--- | :--- |
| `cSoftTimerSet` | 设置定时器：指定时长和初始状态 |
| `cSoftTimerReset` | 重置定时器：从现在开始重新计时 |
| `cSoftTimerReload` | 重载定时器：从上次到期时刻累加 |
| `cSoftTimerOpen` | 开启定时器 (不改变 timeStop) |
| `cSoftTimerClose` | 关闭定时器 |
| `cSoftTimerSetState` | 设置定时器状态 (支持立即触发) |
| `enumSoftTimerGetState` | 查询定时器当前状态 |

---

## 4. 使用场景 (Use Cases)

### 4.1 Reset — 间隔型任务

适合"两次执行之间至少间隔 duration"的场景：

```c
/* 按键防抖：按下后等 50ms 再响应 */
SoftTimerTypeDef timerDebounce;

void onKeyPress(void) {
    cSoftTimerSetMs(&timerDebounce, 50, softTimerOpen);
}

void taskKey(void) {
    if(enumSoftTimerGetState(&timerDebounce) == softTimerOver) {
        handleKeyEvent();
        cSoftTimerClose(&timerDebounce);
    }
}
```

```c
/* 通信超时：收到数据后重新开始计时 */
SoftTimerTypeDef timerTimeout;

void onDataReceived(void) {
    cSoftTimerReset(&timerTimeout);  // 从现在重新开始
}

void taskComm(void) {
    if(enumSoftTimerGetState(&timerTimeout) == softTimerOver) {
        handleTimeout();
    }
}
```

### 4.2 Reload — 周期型任务

适合"固定频率执行，不因处理延迟而漂移"的场景：

```c
/* 固定 10ms 周期采样 */
SoftTimerTypeDef timerSample;

void taskSample(void) {
    if(enumSoftTimerGetState(&timerSample) == softTimerOver) {
        sampleData();
        cSoftTimerReload(&timerSample);  // 保持周期稳定
    }
}
```

**注意**：如果处理时间超过周期，Reload 会连续触发直到追上时基。

---

## 5. 常见问题与知识点 (Q&A)

### Q1: Reset 和 Reload 如何选择？

| 需求 | 选择 |
| :--- | :--- |
| 保证间隔，不在乎周期漂移 | Reset |
| 保证频率，长期不漂移 | Reload |
| 处理可能超时，不想追赶 | Reset |
| 需要补偿丢失的周期 | Reload |

### Q2: 为什么 state 用位标志？

`SoftTimerStateEnum` 设计为位标志形式，允许组合状态：
- `softTimerOpen = 0x01`
- `softTimerOver = 0x02`

在 `cSoftTimerSetState` 中，如果设置了 `softTimerOver` 位，会将 `timeStop` 设为当前时刻，实现"立即触发"功能。

### Q3: 多任务环境下的注意事项？

在 STM32F1（32位）上，`int64_t` 的读写不是原子操作。如果定时器结构体在 ISR 和主循环中同时访问，可能存在数据撕裂风险。建议：
- 同一定时器仅在同一上下文中使用
- 或在访问时添加临界区保护

### Q4: 时间戳溢出问题？

`int64_t` 微秒级时间戳可以表示约 **29 万年**，实际使用中不会溢出。

---

## 6. 典型使用模式 (Patterns)

### 模式 1: 一次性定时

```c
SoftTimerTypeDef timer;

// 设置 1 秒后触发
cSoftTimerSetSecond(&timer, 1, softTimerOpen);

// 轮询检查
if(enumSoftTimerGetState(&timer) == softTimerOver) {
    doSomething();
    cSoftTimerClose(&timer);
}
```

### 模式 2: 周期性定时

```c
SoftTimerTypeDef timer;

// 初始化 100ms 周期
cSoftTimerSetMs(&timer, 100, softTimerOpen);

// 主循环
while(1) {
    if(enumSoftTimerGetState(&timer) == softTimerOver) {
        periodicTask();
        cSoftTimerReload(&timer);  // 或 Reset，取决于需求
    }
}
```

### 模式 3: 超时监控

```c
SoftTimerTypeDef timerWatchdog;

// 设置 5 秒超时
cSoftTimerSetSecond(&timerWatchdog, 5, softTimerOpen);

// 正常操作时喂狗
void feedWatchdog(void) {
    cSoftTimerReset(&timerWatchdog);
}

// 检查超时
if(enumSoftTimerGetState(&timerWatchdog) == softTimerOver) {
    handleWatchdogTimeout();
}
```

---

## 7. 代码质量自检 (Self-Check)

- [x] **空指针检查**：所有函数检查 `ptypeTimer == NULL`。
- [x] **返回值设计**：成功返回 0，失败返回 -1。
- [x] **时间单位统一**：内部全部使用微秒 (μs)。
- [x] **便捷宏定义**：提供 Us/Ms/Second/Minute/Hour 系列宏。
- [x] **无硬件依赖**：仅依赖 `lTimeGetStamp()` 时间源。

---

**文档结束**
