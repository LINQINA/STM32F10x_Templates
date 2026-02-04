# STM32F10x 蜂鸣器驱动设计文档 (DevicesBeep)

| 属性 | 内容 |
| :--- | :--- |
| **模块名称** | DevicesBeep (蜂鸣器驱动模块) |
| **源文件** | `DevicesBeep.c` / `DevicesBeep.h` |
| **硬件依赖** | STM32F103 (GPIO PB8) |
| **版本** | v1.0 |
| **最后更新** | 2026-01-30 |

---

## 1. 设计目标 (Design Goals)

本驱动旨在提供简洁、灵活的蜂鸣器控制方案：

1. **多种发声模式**：支持常响、快响、慢响、响 N 次后停止等模式。
2. **静音模式**：支持全局静音开关，方便调试和用户设置。
3. **非阻塞设计**：基于状态机实现，无阻塞延时。
4. **易于扩展**：结构体设计支持多通道蜂鸣器。

---

## 2. 硬件架构 (Hardware Architecture)

### 2.1 蜂鸣器硬件配置

| 蜂鸣器 | 引脚 | 类型 | 驱动方式 |
| :--- | :--- | :--- | :--- |
| **Channel1** | PB8 | 无源蜂鸣器 | GPIO 推挽输出 |

### 2.2 无源蜂鸣器物理特性

**关键悖论**：无源蜂鸣器靠 PWM 波驱动振膜震动发声。

| 占空比 | 振膜状态 | 声音效果 |
| :--- | :--- | :--- |
| **0%** | 静止（低电平） | **静音** |
| **50%** | 震动幅度最大 | **声音最响** |
| **100%** | 静止（高电平） | **静音** |

**本驱动策略**：使用 GPIO 快速翻转模拟方波，产生约 **500Hz~1kHz** 的声音频率。

### 2.3 结构体设计

```c
typedef struct {
    BeepModeEnum mode;      /* 模式: 静音 / 正常 */
    BeepStateEnum state;    /* 当前状态 */
    int8_t flashCnt;        /* 发声次数计数 */
} BeepType;

typedef struct {
    BeepType channel1;      /* 通道 1 */
    // 可扩展更多通道
} BeepInfoType;
```

---

## 3. 软件实现细节 (Implementation Details)

### 3.1 核心架构：状态机 + 软件定时器

```
┌─────────────────────────────────────────────────────────────┐
│                    FreeRTOS 软件定时器                       │
│                     (周期: 100ms)                            │
│                          │                                   │
│                          ▼                                   │
│                   vBeepMachine()                             │
│                          │                                   │
│              ┌───────────┴───────────┐                       │
│              ▼                       ▼                       │
│        mode == NORMAL?          mode == QUIET?               │
│              │                       │                       │
│              ▼                       ▼                       │
│     vBeepStateMachine()        vBeepClose()                  │
│        (状态机处理)             (强制关闭)                    │
└─────────────────────────────────────────────────────────────┘
```

### 3.2 蜂鸣器状态枚举

| 状态 | 枚举值 | 说明 |
| :--- | :--- | :--- |
| `BEEP_IDLE` | 0 | 空闲 |
| `BEEP_DISABLE` | 1 | 关闭 |
| `BEEP_ENABLE` | 2 | 常响 |
| `BEEP_FLASH_SLOW` | 3 | 慢响循环（0.5Hz） |
| `BEEP_FLASH_SLOW_ENABLE_CNT` | 4 | 慢响 N 次后常响 |
| `BEEP_FLASH_SLOW_DISABLE_CNT` | 5 | 慢响 N 次后关闭 |
| `BEEP_FLASH_FAST` | 6 | 快响循环（1Hz） |
| `BEEP_FLASH_FAST_ENABLE_CNT` | 7 | 快响 N 次后常响 |
| `BEEP_FLASH_FAST_DISABLE_CNT` | 8 | 快响 N 次后关闭 |

### 3.3 工作模式

| 模式 | 枚举值 | 说明 |
| :--- | :--- | :--- |
| `BEEP_MODE_QUIET` | 0 | 静音模式，所有发声指令被忽略 |
| `BEEP_MODE_NORMAL` | 1 | 正常模式，响应发声指令 |

### 3.4 状态机实现

```c
static void vBeepStateMachine(BeepType *ptypeBEEP)
{
    switch(ptypeBEEP->state)
    {
        case BEEP_DISABLE:
            vBeepClose(BEEP_CHANNEL1);
            ptypeBEEP->state = BEEP_IDLE;
            break;

        case BEEP_ENABLE:
            vBeepOpen(BEEP_CHANNEL1);
            ptypeBEEP->state = BEEP_IDLE;
            break;

        /* 慢响 / 慢响后关闭 / 慢响后常响 */
        case BEEP_FLASH_SLOW:
        case BEEP_FLASH_SLOW_DISABLE_CNT:
        case BEEP_FLASH_SLOW_ENABLE_CNT:
            if((st_uiBeepTickCnt % 5) == 0)  // 每 500ms 切换一次
            {
                ((st_uiBeepTickCnt / 5) & 1) ? vBeepOpen() : vBeepClose();
                
                // 计数模式处理
                if((ptypeBEEP->state != BEEP_FLASH_SLOW) && ((ptypeBEEP->flashCnt--) <= 0))
                {
                    ptypeBEEP->state = (ptypeBEEP->state == BEEP_FLASH_SLOW_ENABLE_CNT) 
                                       ? BEEP_ENABLE : BEEP_DISABLE;
                }
            }
            break;

        /* 快响逻辑类似 */
        // ...
    }
}
```

### 3.5 关键函数说明

| 函数名 | 说明 |
| :--- | :--- |
| `vBeepInit` | 初始化 GPIO，设置默认状态 |
| `vBeepMachine` | 状态机主循环，需周期性调用（100ms） |
| `vBeepStatusSet` | 设置蜂鸣器状态和发声次数 |
| `vBeepModeSet` | 设置工作模式（静音/正常） |
| `ptypeBeepInfoGet` | 获取蜂鸣器信息结构体指针 |

### 3.6 便捷宏定义

```c
#define vBeepSoundFast(cCnt) vBeepStatusSet(BEEP_CHANNEL_ALL, BEEP_FLASH_FAST_DISABLE_CNT, cCnt)
#define vBeepSoundSlow(cCnt) vBeepStatusSet(BEEP_CHANNEL_ALL, BEEP_FLASH_SLOW_DISABLE_CNT, cCnt)
```

**使用示例**：
```c
vBeepSoundFast(3);  // 快响 3 次后停止
vBeepSoundSlow(2);  // 慢响 2 次后停止
```

---

## 4. 常见问题与知识点 (Q&A)

### Q1: 为什么无源蜂鸣器 100% 占空比是静音？

**A**: 无源蜂鸣器内部没有振荡电路，需要外部提供交变信号驱动振膜。
- **100% 占空比** = 恒定高电平 = 直流 = 振膜不动 = 静音
- **50% 占空比** = 方波 = 振膜来回震动 = 发声

### Q2: 为什么使用 GPIO 而不是 PWM？

**A**: 
1. **简化设计**：不需要占用定时器资源。
2. **足够使用**：状态机周期性翻转 GPIO，产生的方波频率足以驱动蜂鸣器。
3. **节省资源**：PWM 更适合需要精确频率/音调控制的场景（如播放音乐）。

### Q3: 静音模式的作用？

**A**: 提供全局开关，方便：
- 用户在设置中关闭声音提示
- 调试时避免噪音干扰
- 夜间模式等场景

```c
vBeepModeSet(BEEP_CHANNEL_ALL, BEEP_MODE_QUIET);   // 开启静音
vBeepModeSet(BEEP_CHANNEL_ALL, BEEP_MODE_NORMAL);  // 恢复正常
```

### Q4: flashCnt 为什么初始化为 `ucFlashCnt * 2 - 1`？

**A**: 一次完整的"响"包含两个阶段：响 → 停。所以：
- 响 1 次 = 1 个"响" + 1 个"停" = 2 个状态切换
- 响 N 次 = N×2 个状态切换
- `-1` 是为了配合 `flashCnt--` 的判断逻辑

---

## 5. 移植与扩展 (Porting Guide)

### 如果要增加新的蜂鸣器通道

1. 在 `BeepChannelEnum` 中添加新通道枚举。
2. 在 `BeepInfoType` 结构体中添加新通道成员。
3. 在 `vBeepInit` 中配置新通道的 GPIO。
4. 在 `vBeepOpen` / `vBeepClose` 中添加新通道的处理。

### 如果要修改响声频率

修改状态机中的时间判断：
```c
// 原来：每 500ms 切换 (0.5Hz 慢响)
if((st_uiBeepTickCnt % 5) == 0)

// 改为：每 200ms 切换 (2.5Hz 快响)
if((st_uiBeepTickCnt % 2) == 0)
```

### 如果要使用 PWM 驱动（播放音调）

1. 配置定时器 PWM 输出。
2. 修改 `vBeepOpen` 启动 PWM，`vBeepClose` 停止 PWM。
3. 可通过修改 PWM 频率实现不同音调。

---

## 6. 代码质量自检 (Self-Check)

- [x] **静音模式**：提供全局静音开关。
- [x] **状态机设计**：非阻塞，无 `delay`。
- [x] **模块化**：结构体设计支持多通道扩展。
- [x] **计数归零**：`vBeepStatusSet` 中重置 `st_uiBeepTickCnt`，保证完整周期。
- [x] **便捷宏**：提供常用操作的快捷调用。

---

**文档结束**
