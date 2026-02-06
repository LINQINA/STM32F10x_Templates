# STM32F10x LED 驱动设计文档 (DevicesLed)

| 属性 | 内容 |
| :--- | :--- |
| **模块名称** | DevicesLed (LED驱动模块) |
| **源文件** | `DevicesLed.c` / `DevicesLed.h` |
| **硬件依赖** | STM32F103 (GPIO, TIM3 PWM) |
| **软件依赖** | DevicesTimer (定时器配置) |
| **版本** | v1.0 |
| **最后更新** | 2026-01-30 |

---

## 1. 设计目标 (Design Goals)

本驱动旨在提供一套灵活、高效的 LED 控制方案：

1. **多种显示模式**：支持常亮、闪烁（快/慢）、呼吸灯、SOS 信号等多种效果。
2. **硬件抽象**：统一封装 PWM 驱动和普通 IO 驱动，上层无需关心底层差异。
3. **非阻塞设计**：基于状态机实现，无 `delay` 阻塞，适合 RTOS 环境。
4. **资源节省**：运行在软件定时器回调中，避免独立任务栈开销。

---

## 2. 硬件架构 (Hardware Architecture)

### 2.1 LED 硬件配置

| LED | 引脚 | 驱动方式 | 说明 |
| :--- | :--- | :--- | :--- |
| **红灯** | PB5 (TIM3_CH2) | **PWM** | 支持亮度调节、呼吸灯效果 |
| **绿灯** | PE5 | **GPIO** | 仅支持开关、闪烁 |

### 2.2 驱动模式对比

| 驱动模式 | 亮度调节 | 呼吸效果 | 硬件要求 |
| :--- | :--- | :--- | :--- |
| `LED_DRIVE_PWM` | ✅ 支持 | ✅ 支持 | 需要定时器 PWM 通道 |
| `LED_DRIVE_IO` | ❌ 仅开关 | ❌ 不支持 | 普通 GPIO 即可 |

### 2.3 结构体设计

```c
typedef struct {
    LedChannelEnum ledChannel;  /* LED 通道类型 */
    LedStateEnum state;         /* 当前状态 */
    int8_t flashCnt;            /* 闪烁次数 */
    int8_t duty;                /* 占空比 (0-100) */
    
    uint8_t driveMode;          /* 驱动模式: IO / PWM */
    TIM_HandleTypeDef *htim;    /* 定时器句柄 (PWM 模式) */
    uint32_t channel;           /* PWM 通道 */
} LedType;
```

---

## 3. 软件实现细节 (Implementation Details)

### 3.1 核心架构：状态机 + 软件定时器

```
┌─────────────────────────────────────────────────────────────┐
│                    FreeRTOS 软件定时器                       │
│                     (周期: 20ms)                             │
│                          │                                   │
│                          ▼                                   │
│                   vLedMachine()                              │
│                          │                                   │
│            ┌─────────────┼─────────────┐                     │
│            ▼             ▼             ▼                     │
│     vLedStateMachine  vLedStateMachine  ...                  │
│        (红灯)           (绿灯)                               │
└─────────────────────────────────────────────────────────────┘
```

**优势**：
- **极大节省 RAM**：避免独立任务栈（通常 >512 Bytes），共用 `Tmr Svc` 任务栈。
- **非阻塞**：回调函数执行极快，不占用 CPU。
- **时序保证**：每 20ms 准时触发一次状态更新。

### 3.2 LED 状态枚举

| 状态 | 枚举值 | 说明 |
| :--- | :--- | :--- |
| `LED_DISABLE` | 0 | 关闭 |
| `LED_ENABLE` | 1 | 常亮（高亮度） |
| `LED_ENABLE_LOW` | 3 | 常亮（低亮度） |
| `LED_DUTY` | 4 | 固定占空比 |
| `LED_FLASH_FAST` | 10 | 快速闪烁（周期 800ms） |
| `LED_FLASH_SLOW` | 20 | 慢速闪烁（周期 2400ms） |
| `LED_BREATHE` | 30 | 呼吸灯效果 |
| `LED_FLASH_SOS` | 40 | SOS 求救信号 |

### 3.3 呼吸灯算法 (Gamma 校正)

人眼对亮度的感知是**非线性**的，直接线性调节 PWM 占空比会导致：
- 低亮度区域变化不明显
- 高亮度区域变化过快

**解决方案**：使用抛物线公式 $y = x^2$ 模拟 Gamma 校正：

```c
// 亮度计算，110 为最大刻度
Duty = (Grade * Grade) * (1.0f / (110.0f * 110.0f));
```

**往复运动实现**：

```c
// st_sGrade 在 -15 ~ 110 之间摆动
st_sGrade += st_cDirection;
st_cDirection = (st_sGrade >= 110) ? -1 : ((st_sGrade <= -15) ? 1 : st_cDirection);
```

**死区设计 (-15 ~ 0)**：让灯在熄灭状态停留片刻，增强呼吸的真实感。

```
亮度 ▲
     │      ╱╲      ╱╲
     │     ╱  ╲    ╱  ╲
     │    ╱    ╲  ╱    ╲
     │   ╱      ╲╱      ╲
   0 ├───────────────────────► 时间
     │   ▔▔▔▔    ▔▔▔▔
     │   死区     死区
```

### 3.4 SOS 信号算法

SOS 摩尔斯电码：`···  ———  ···` (3短 + 3长 + 3短)

```c
// 自动归零，无需 if 判断
st_uiLedSosCnt %= (FAST*3 + SLOW*3 + FAST*3 + DELAY);
```

时序分解：
| 阶段 | 时间范围 | 动作 |
| :--- | :--- | :--- |
| S | 0 ~ FAST×3 | 快闪 3 次 |
| O | FAST×3 ~ (FAST+SLOW)×3 | 慢闪 3 次 |
| S | (FAST+SLOW)×3 ~ (FAST×2+SLOW)×3 | 快闪 3 次 |
| 间隔 | 之后 | 熄灭等待 |

### 3.5 关键函数说明

| 函数名 | 说明 |
| :--- | :--- |
| `vLedInit` | 初始化 GPIO、PWM，配置 LED 结构体 |
| `vLedMachine` | 状态机主循环，需周期性调用（20ms） |
| `vLedSetStatus` | 设置 LED 状态和参数 |
| `vLedOpen` / `vLedClose` | 直接开关 LED（IO 模式） |
| `vLedRevesal` | 翻转 LED 状态 |
| `ptypeLedGetInfo` | 获取 LED 信息结构体指针 |

### 3.6 便捷宏定义

```c
#define vLedSetStatusFlashFast(usChannel) vLedSetStatus((usChannel), LED_FLASH_FAST, 0)
#define vLedSetStatusFlashSlow(usChannel) vLedSetStatus((usChannel), LED_FLASH_SLOW, 0)
#define vLedSetStatusBreathe(usChannel)   vLedSetStatus((usChannel), LED_BREATHE, 0)
#define vLedSetStatusDisable(usChannel)   vLedSetStatus((usChannel), LED_DISABLE, 0)
```

---

## 4. 常见问题与知识点 (Q&A)

### Q1: 为什么使用软件定时器而不是硬件中断？

**A**: 
1. **安全性**：硬件中断中进行浮点运算（如呼吸灯 Gamma 校正）可能导致系统卡顿。
2. **优先级可控**：软件定时器优先级可配置，不会影响关键中断。
3. **资源共享**：多个模块可共用同一个定时器服务任务。

### Q2: 为什么呼吸灯要用 float 计算？

**A**: Gamma 校正需要平方运算，使用整数会丢失精度，导致低亮度区域出现"阶梯感"。现代 MCU（如 STM32F103）的浮点运算效率足够应对 20ms 周期的状态机。

### Q3: 如何实现"闪烁 N 次后常亮/熄灭"？

**A**: 使用带计数的状态：
```c
vLedSetStatus(LED_CHANNEL_RED, LED_FLASH_FAST_ENABLE_CNT, 3);  // 快闪 3 次后常亮
vLedSetStatus(LED_CHANNEL_RED, LED_FLASH_FAST_DISABLE_CNT, 3); // 快闪 3 次后熄灭
```

---

## 5. 移植与扩展 (Porting Guide)

### 如果要增加新的 LED

1. 在 `LedChannelEnum` 中添加新通道枚举。
2. 在 `LedInfoType` 结构体中添加新 LED 成员。
3. 在 `vLedInit` 中配置新 LED 的 GPIO 和驱动模式。
4. `vLedMachine` 会自动遍历所有 LED，无需修改。

### 如果要修改闪烁频率

修改头文件中的宏定义：
```c
#define LED_FLASH_FAST_PERIOD   800   // 快闪周期 (ms)
#define LED_FLASH_SLOW_PERIOD   2400  // 慢闪周期 (ms)
```

### 如果要调整呼吸灯速度

修改 `vLedIncTick` 中的增量或范围：
```c
st_sGrade += st_cDirection;  // 改为 +=2 可加快速度
st_cDirection = (st_sGrade >= 110) ? -1 : ((st_sGrade <= -15) ? 1 : st_cDirection);
```

---

## 6. 代码质量自检 (Self-Check)

- [x] **硬件抽象**：PWM 和 IO 驱动统一封装，上层无感知。
- [x] **状态机设计**：非阻塞，无 `delay`。
- [x] **边界保护**：`vLedDutySet` 中对占空比进行了 0~1 范围限制。
- [x] **模块化**：通过结构体数组遍历，易于扩展。
- [x] **Gamma 校正**：呼吸灯视觉效果自然。

---

**文档结束**
