# STM32F10x 延时模块设计文档 (DevicesDelay)

| 属性 | 内容 |
| :--- | :--- |
| **模块名称** | DevicesDelay (延时模块) |
| **源文件** | `DevicesDelay.c` / `DevicesDelay.h` |
| **硬件依赖** | 无 (依赖 DevicesTime 提供时间基准) |
| **软件依赖** | FreeRTOS (仅 RTOS 延时函数) |
| **版本** | v1.0 |
| **最后更新** | 2026-01-30 |

---

## 1. 设计目标 (Design Goals)

本模块提供两类延时功能，满足不同场景需求：

1. **非阻塞性延时**：任务保持运行态，CPU 忙等，适合短延时。
2. **阻塞性延时**：任务进入阻塞态，让出 CPU，适合 RTOS 环境。

---

## 2. 模块架构 (Module Architecture)

### 2.1 延时类型对比

| 类型 | 函数 | 任务状态 | CPU 占用 | 适用场景 |
| :--- | :--- | :--- | :--- | :--- |
| **非阻塞性延时** | `vDelayUs/Ms/S` | 运行态 (忙等) | 独占 | 硬件初始化、μs 级精确延时 |
| **阻塞性延时** | `vRtosDelayMs/S` | 阻塞态 | 让出 | RTOS 任务中的普通延时 |

### 2.2 架构图

```
┌─────────────────────────────────────────────────────────────┐
│                     非阻塞性延时 (忙等)                       │
│  ┌──────────┐     ┌──────────┐     ┌──────────┐            │
│  │ vDelayUs │ ◄── │ vDelayMs │ ◄── │ vDelayS  │            │
│  └────┬─────┘     └──────────┘     └──────────┘            │
│       │                                                     │
│       └──► while(lTimeGetStamp() < timeStop);              │
│            任务保持【运行态】，CPU 空转等待                   │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│                     阻塞性延时 (让出 CPU)                     │
│  ┌──────────────┐     ┌──────────────┐                      │
│  │ vRtosDelayMs │ ◄── │ vRtosDelayS  │                      │
│  └──────┬───────┘     └──────────────┘                      │
│         │                                                   │
│         ├── 调度器运行中 → vTaskDelay() → 任务进入【阻塞态】  │
│         │                                                   │
│         └── 调度器未启动 → 退化为 vDelayMs() (忙等)          │
└─────────────────────────────────────────────────────────────┘
```

---

## 3. 软件实现细节 (Implementation Details)

### 3.1 非阻塞性延时 (vDelayUs)

```c
void vDelayUs(int64_t lTime)
{
    int64_t lTimeStop = lTimeGetStamp() + lTime;
    
    while(lTimeGetStamp() < lTimeStop);
}
```

**特点**：
- 基于时间戳比较，精度取决于 `lTimeGetStamp()` 分辨率
- 任务保持运行态，不会触发任务切换
- 适合微秒级短延时

### 3.2 毫秒/秒级延时封装

```c
void vDelayMs(int64_t lTime)
{
    vDelayUs(lTime * 1000);
}

void vDelayS(int64_t lTime)
{
    vDelayMs(lTime * 1000);
}
```

### 3.3 阻塞性延时 (vRtosDelayMs)

```c
void vRtosDelayMs(float fTime)
{
    if(fTime < 1.0f)
        return;

    if(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
        vTaskDelay(fTime / portTICK_RATE_MS);
    else
        vDelayMs(fTime);
}
```

**特点**：
- 调度器运行时使用 `vTaskDelay()`，任务进入阻塞态，让出 CPU
- 调度器未启动时退化为忙等延时
- 小于 1ms 直接返回（FreeRTOS tick 精度限制）

### 3.4 秒级 RTOS 延时

```c
void vRtosDelayS(float fTime)
{
    vRtosDelayMs(fTime * 1000.0f);
}
```

### 3.5 关键函数说明

| 函数名 | 参数类型 | 说明 |
| :--- | :--- | :--- |
| `vDelayUs` | `int64_t` (μs) | 微秒级非阻塞延时 |
| `vDelayMs` | `int64_t` (ms) | 毫秒级非阻塞延时 |
| `vDelayS` | `int64_t` (s) | 秒级非阻塞延时 |
| `vRtosDelayMs` | `float` (ms) | 毫秒级阻塞延时 (RTOS) |
| `vRtosDelayS` | `float` (s) | 秒级阻塞延时 (RTOS) |

---

## 4. 使用场景 (Use Cases)

### 4.1 硬件初始化

硬件复位后需要等待稳定，此时调度器可能未启动：

```c
void initLCD(void) {
    GPIO_ResetPin(LCD_RST);
    vDelayMs(10);           // 复位保持 10ms
    GPIO_SetPin(LCD_RST);
    vDelayMs(120);          // 等待 LCD 初始化完成
    
    sendCommand(0x11);      // Sleep Out
    vDelayMs(5);            // 等待命令执行
}
```

### 4.2 微秒级精确时序

某些通信协议需要精确的微秒级时序：

```c
void sendOneBit(uint8_t bit) {
    DATA_HIGH();
    vDelayUs(bit ? 60 : 15);    // 1: 高电平 60μs, 0: 高电平 15μs
    DATA_LOW();
    vDelayUs(bit ? 10 : 55);    // 补齐周期
}
```

### 4.3 RTOS 任务中的周期等待

在任务中等待时让出 CPU：

```c
void taskLedBlink(void *pvParameters) {
    while(1) {
        LED_Toggle();
        vRtosDelayMs(500);      // 让出 CPU 500ms
    }
}
```

### 4.4 调度器启动前后兼容

`vRtosDelayMs` 自动判断调度器状态，可在启动前后通用：

```c
int main(void) {
    HAL_Init();
    SystemClock_Config();
    
    // 调度器未启动，退化为忙等
    vRtosDelayMs(100);
    
    initPeripherals();
    
    xTaskCreate(taskMain, "Main", 256, NULL, 1, NULL);
    vTaskStartScheduler();
    
    // 不会执行到这里
}

void taskMain(void *pvParameters) {
    while(1) {
        // 调度器已启动，使用 vTaskDelay
        vRtosDelayMs(100);
        doWork();
    }
}
```

---

## 5. 常见问题与知识点 (Q&A)

### Q1: 为什么 vRtosDelayMs 小于 1ms 直接返回？

**A**: FreeRTOS 的延时精度受限于系统 tick 周期（通常 1ms）。小于 1 tick 的延时无法通过 `vTaskDelay` 实现，强行调用可能导致延时为 0 或 1 tick，行为不确定。

如需亚毫秒延时，请使用 `vDelayUs`。

### Q2: 非阻塞延时和阻塞延时的本质区别？

从 **RTOS 任务状态**角度：

| 延时类型 | 任务状态 | 调度器行为 |
| :--- | :--- | :--- |
| 非阻塞 (`vDelay`) | 运行态 | 不会切换任务 |
| 阻塞 (`vRtosDelay`) | 阻塞态 | 切换到其他就绪任务 |

### Q3: 为什么 vRtosDelayMs 参数用 float？

**A**: 允许更灵活的延时设置，如 `vRtosDelayMs(1.5)` 表示 1.5ms。但实际精度仍受 tick 周期限制，会被取整。

### Q4: vDelayUs 会被中断打断吗？

**A**: 会。`vDelayUs` 只是普通的忙等循环，中断可以正常触发。如果中断执行时间较长，实际延时会比预期长。如需精确延时，可在延时前关闭中断（但要谨慎使用）。

### Q5: 长时间 vDelay 会有什么问题？

**A**: 
- CPU 被独占，其他任务无法执行
- 看门狗可能超时（如果有使用）
- 中断响应正常，但中断返回后继续忙等

长延时（>10ms）建议使用 `vRtosDelayMs`。

---

## 6. 设计建议 (Best Practices)

### 6.1 选择合适的延时函数

```
需要延时
    │
    ├─ < 1ms 且需要精确？
    │       └── 是 → vDelayUs
    │
    ├─ 调度器未启动？
    │       └── 是 → vDelayMs
    │
    └─ RTOS 任务中？
            └── 是 → vRtosDelayMs
```

### 6.2 避免在中断中使用延时

无论哪种延时函数，都**不应该**在中断服务程序中使用：
- `vDelay`：会阻塞中断，影响系统实时性
- `vRtosDelay`：在 ISR 中调用 FreeRTOS API 可能导致系统崩溃

### 6.3 硬件初始化优先使用 vDelay

硬件初始化通常在调度器启动前进行，此时只能使用 `vDelay` 系列函数。

---

## 7. 与其他模块的关系 (Dependencies)

```
┌─────────────┐
│ DevicesTime │  提供 lTimeGetStamp() 时间基准
└──────┬──────┘
       │
       ▼
┌─────────────┐     ┌─────────────────┐
│DevicesDelay │────►│ DevicesSoftTimer │
└─────────────┘     └─────────────────┘
 延时等待            定时检查
 (阻塞当前流程)       (非阻塞轮询)
```

**使用场景对比**：

| 需求 | 推荐模块 |
| :--- | :--- |
| 等待固定时间再继续 | DevicesDelay |
| 周期性检查是否超时 | DevicesSoftTimer |
| 多个定时任务并行 | DevicesSoftTimer |
| 硬件时序控制 | DevicesDelay |

---

## 8. 代码质量自检 (Self-Check)

- [x] **调度器状态检测**：`vRtosDelayMs` 正确判断调度器状态。
- [x] **边界处理**：小于 1ms 的 RTOS 延时直接返回。
- [x] **时间单位清晰**：函数名明确标识时间单位 (Us/Ms/S)。
- [x] **无硬件依赖**：仅依赖 `lTimeGetStamp()` 时间源。
- [ ] **建议**：移除未使用的 `#include "stdio.h"`。

---

**文档结束**
