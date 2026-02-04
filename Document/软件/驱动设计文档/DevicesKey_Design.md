# STM32F10x 按键驱动设计文档 (DevicesKey)

| 属性 | 内容 |
| :--- | :--- |
| **模块名称** | DevicesKey (按键驱动模块) |
| **源文件** | `DevicesKey.c` / `DevicesKey.h` |
| **硬件依赖** | STM32F103 (GPIO PE3, PE4, PA0) |
| **软件依赖** | DevicesTime (时间戳获取) |
| **版本** | v1.0 |
| **最后更新** | 2026-01-30 |

---

## 1. 设计目标 (Design Goals)

本驱动旨在提供一套功能完善、高效的按键检测方案：

1. **多种按键事件**：支持短按、长按、持续按三种时间属性。
2. **组合键支持**：通过位运算支持任意按键组合检测。
3. **全非阻塞消抖**：基于时间阈值的软件消抖，无阻塞延时。
4. **硬件归一化**：无论上拉还是下拉电路，统一"按下=1"的逻辑。

---

## 2. 硬件架构 (Hardware Architecture)

### 2.1 按键硬件配置

| 按键 | 引脚 | 电路类型 | 默认电平 | 按下电平 |
| :--- | :--- | :--- | :--- | :--- |
| **KEY_0** | PE4 | 上拉 | 高 | 低 |
| **KEY_1** | PE3 | 上拉 | 高 | 低 |
| **KEY_UP** | PA0 | 下拉 | 低 | 高 |

### 2.2 归一化处理

底层函数 `uiKeyValueGet` 将不同电路类型统一转换：

```c
uint32_t uiKeyValueGet(void)
{
    uint32_t Key = KEY_0 | KEY_1 | KEY_UP;
    
    // 上拉按键：检测到高电平（未按下）时清除标志
    if(RESET != HAL_GPIO_ReadPin(KEY_0_GPIO_Port, KEY_0_Pin))
        Key &= ~KEY_0;
    
    // 下拉按键：检测到低电平（未按下）时清除标志
    if(SET != HAL_GPIO_ReadPin(KEY_UP_GPIO_Port, KEY_UP_Pin))
        Key &= ~KEY_UP;
    
    return Key;  // 返回值中，按下的键对应位为 1
}
```

**好处**：上层逻辑无需关心硬件电路差异，统一使用"按下=1"的逻辑。

### 2.3 按键值定义

```c
#define KEY_0   0x0001  // bit0
#define KEY_1   0x0002  // bit1
#define KEY_UP  0x0004  // bit2
```

---

## 3. 软件实现细节 (Implementation Details)

### 3.1 核心数据结构

```c
typedef struct {
    uint32_t valueLast;              /* 上一次的按键值 */
    
    uint32_t valuePress;             /* 新增的按下键值 */
    uint32_t timePress;              /* 最后一次按下的时刻 */
    
    uint32_t valueLoosen;            /* 新增的松开键值 */
    uint32_t timeLoosen;             /* 最后一次松开的时刻 */
    
    KeyStateEnum state;              /* 当前状态 */
    uint32_t (*uiKeyValueGet)(void); /* 按键值读取函数指针 */
} KeyTypeDef;
```

### 3.2 状态枚举设计

**状态分层**：低 8 位表示动作状态，高 8 位表示时间属性。

```c
typedef enum {
    keyNormal = 0,
    
    /* 低 8 位：动作状态 */
    keyEqual        = 0x0001,   /* 按键值未变化 */
    keyAdd          = 0x0002,   /* 新增了按键 */
    keyCut          = 0x0004,   /* 松开了按键 */
    keyAddAndCut    = 0x0008,   /* 同时有新增和松开 */
    
    /* 高 8 位：时间属性 */
    keyShort        = 0x0100,   /* 短按 (>50ms) */
    keyLong         = 0x0200,   /* 长按 (>2000ms) */
    keyContinuous   = 0x0400,   /* 持续按 (>5000ms) */
} KeyStateEnum;
```

**组合使用**：`keyShort | keyCut` 表示"经过短按时间验证后松开"。

### 3.3 位运算集合逻辑 (核心算法)

利用位运算判断按键集合的包含关系，支持任意组合键：

| 操作 | 条件 | 含义 |
| :--- | :--- | :--- |
| **新增 (Add)** | `(New & Old) == Old` | 旧的是新的子集 → 增加了按键 |
| **减少 (Cut)** | `(New \| Old) == Old` | 新的是旧的子集 → 松开了按键 |

**图解韦恩图**：

![image-20260123114357327](C:\Users\10673\AppData\Roaming\Typora\typora-user-images\image-20260123114357327.png)

```
       新增 (Add)                    减少 (Cut)
    ┌─────────────┐              ┌─────────────┐
    │  New (大)   │              │  Old (大)   │
    │ ┌───────┐   │              │ ┌───────┐   │
    │ │ Old   │   │              │ │ New   │   │
    │ │ (小)  │   │              │ │ (小)  │   │
    │ └───────┘   │              │ └───────┘   │
    └─────────────┘              └─────────────┘
    Old 是 New 的子集             New 是 Old 的子集
```

**代码实现**：

```c
static KeyStateEnum enumKeyStateGet(uint32_t uiNewValue, uint32_t uiOldValue)
{
    if(uiNewValue == uiOldValue)
        return keyEqual;                              // 相同
    else if((uiNewValue & uiOldValue) == uiOldValue)
        return keyAdd;                                // 新增
    else if((uiNewValue | uiOldValue) == uiOldValue)
        return keyCut;                                // 减少
    else
        return keyAddAndCut;                          // 既有新增又有减少
}
```

### 3.4 时间阈值消抖

**核心思想**：**"不见兔子不撒鹰"**。只有经过时间验证的信号，才会被打上有效标签。

| 时间阈值 | 宏定义 | 默认值 |
| :--- | :--- | :--- |
| 短按 | `KEY_SHORT_TIME` | 50ms |
| 长按 | `KEY_LONG_TIME` | 2000ms |
| 持续按 | `KEY_CONTINUE_TIME` | 5000ms |

**场景模拟：一次带有抖动的按键过程**

```
时间轴 ─────────────────────────────────────────────────────►

T0        T0+10ms     T0+20ms     T0+70ms      T0+100ms
│ 按下     │ 抖动松开   │ 重新按下   │ 稳定保持    │ 真正松手
│ keyAdd   │ keyCut    │ keyAdd    │ keyEqual   │ keyCut
│ 记录时间  │ 无keyShort │ 重置时间   │ 加keyShort │ 有keyShort
│          │ → 忽略！   │           │ 标志       │ → 有效！
▼          ▼           ▼           ▼            ▼
```

**状态转换代码**：

```c
static KeyStateEnum enumKeyTimeStateGet(KeyTypeDef *ptypeKeyMachine)
{
    int32_t lTimeNow = (int32_t)(lTimeGetStamp() / 1000ll);

    switch((uint32_t)ptypeKeyMachine->state)
    {
        case keyAdd:
            if((lTimeNow - ptypeKeyMachine->timePress) >= KEY_SHORT_TIME)
            {
                ptypeKeyMachine->state &= 0x00FF;    // 清除旧时间属性
                ptypeKeyMachine->state |= keyShort;   // 添加短按标志
                return ptypeKeyMachine->state;
            }
            break;

        case keyShort | keyAdd:
            if((lTimeNow - ptypeKeyMachine->timePress) >= KEY_LONG_TIME)
            {
                ptypeKeyMachine->state &= 0x00FF;
                ptypeKeyMachine->state |= keyLong;    // 升级为长按
                return ptypeKeyMachine->state;
            }
            break;

        case keyLong | keyAdd:
            if((lTimeNow - ptypeKeyMachine->timePress) >= KEY_CONTINUE_TIME)
            {
                ptypeKeyMachine->state &= 0x00FF;
                ptypeKeyMachine->state |= keyContinuous;  // 升级为持续按
                return ptypeKeyMachine->state;
            }
            break;
    }
    return keyNormal;
}
```

### 3.5 高效过滤

入口处快速判断，避免无效计算：

```c
if(((uiNewValue = ptypeKeyMachine->uiKeyValueGet()) != 0) || (ptypeKeyMachine->valueLast != 0))
{
    // 有按键按下或刚松开，才进入状态机处理
}
```

**效果**：当所有按键都空闲时，直接跳过所有逻辑，极致省流。

### 3.6 关键函数说明

| 函数名 | 说明 |
| :--- | :--- |
| `vKeyInit` | 初始化 GPIO，配置函数指针 |
| `uiKeyValueGet` | 读取当前按键值（归一化处理） |
| `enumKeyStateMachine` | 状态机主函数，返回当前按键状态 |

---

## 4. 使用示例 (Usage Example)

```c
void taskKey(void *pvParameters)
{
    KeyStateEnum state;
    
    while(1)
    {
        state = enumKeyStateMachine(&g_typeKeyData);
        
        // 检测短按后松开
        if((state & keyCut) && (state & keyShort))
        {
            if(g_typeKeyData.valueLoosen & KEY_0)
            {
                // KEY_0 短按事件
            }
            if(g_typeKeyData.valueLoosen & KEY_1)
            {
                // KEY_1 短按事件
            }
        }
        
        // 检测长按（按住不松手时触发）
        if((state & keyAdd) && (state & keyLong))
        {
            if(g_typeKeyData.valuePress & KEY_UP)
            {
                // KEY_UP 长按事件
            }
        }
        
        // 检测组合键
        if((state & keyCut) && (state & keyShort))
        {
            if((g_typeKeyData.valueLoosen & (KEY_0 | KEY_1)) == (KEY_0 | KEY_1))
            {
                // KEY_0 + KEY_1 组合键事件
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
```

---

## 5. 常见问题与知识点 (Q&A)

### Q1: 为什么使用位运算而不是简单的 if-else？

**A**: 
1. **支持组合键**：位运算天然支持多个按键的集合操作。
2. **扩展性强**：增加新按键只需添加新的位定义，无需修改判断逻辑。
3. **效率高**：位运算比多个条件判断更快。

### Q2: 为什么状态要分高低 8 位？

**A**: 实现"动作"和"时间属性"的正交组合：
- 动作：Add / Cut / Equal
- 时间属性：Short / Long / Continuous

这样可以用 `state & keyCut` 判断是否松开，用 `state & keyShort` 判断是否经过短按验证，两者独立判断。

### Q3: 如何清除旧动作保留时间属性？

**A**: 使用位掩码：
```c
ptypeKeyMachine->state &= ~0x00FF;  // 清除低 8 位（动作）
ptypeKeyMachine->state |= keyCut;   // 设置新动作
```

### Q4: `uiKeyValueGet` 中为什么要循环 8 次？

**A**: 这是一种简单的硬件消抖方式，通过多次采样提高读取稳定性。不过主要的消抖还是依赖时间阈值判断。

---

## 6. 移植与扩展 (Porting Guide)

### 如果要增加新的按键

1. 定义新按键的 GPIO 和位掩码：
   ```c
   #define KEY_NEW_GPIO_Port  GPIOX
   #define KEY_NEW_Pin        GPIO_PIN_X
   #define KEY_NEW            0x0008  // bit3
   ```
2. 在 `vKeyInit` 中配置 GPIO。
3. 在 `uiKeyValueGet` 中添加读取逻辑。

### 如果要修改时间阈值

修改头文件中的宏定义：
```c
#define KEY_SHORT_TIME      50    // 短按阈值 (ms)
#define KEY_LONG_TIME       2000  // 长按阈值 (ms)
#define KEY_CONTINUE_TIME   5000  // 持续按阈值 (ms)
```

### 如果要支持多组按键（如矩阵键盘）

1. 创建多个 `KeyTypeDef` 实例。
2. 为每组按键实现独立的 `uiKeyValueGet` 函数。
3. 在初始化时绑定对应的函数指针。

---

## 7. C 语言关键技巧

### 7.1 位操作 (Bitwise Operations)

| 操作 | 代码 | 说明 |
| :--- | :--- | :--- |
| 置位 | `val \|= FLAG;` | 设置某一位为 1 |
| 清零 | `val &= ~FLAG;` | 设置某一位为 0 |
| 判断 | `if (val & FLAG)` | 检查某一位是否为 1 |
| 集合新增 | `(New & Old) == Old` | 判断 Old 是否为 New 的子集 |
| 集合减少 | `(New \| Old) == Old` | 判断 New 是否为 Old 的子集 |

### 7.2 函数指针

```c
uint32_t (*uiKeyValueGet)(void);  // 函数指针声明
g_typeKeyData.uiKeyValueGet = uiKeyValueGet;  // 绑定
uiNewValue = ptypeKeyMachine->uiKeyValueGet();  // 调用
```

**好处**：便于替换不同的按键读取实现，如矩阵键盘、模拟按键等。

---

## 8. 代码质量自检 (Self-Check)

- [x] **归一化处理**：上拉/下拉电路统一为"按下=1"。
- [x] **非阻塞消抖**：基于时间阈值，无 `delay`。
- [x] **组合键支持**：位运算实现集合逻辑。
- [x] **状态分层**：动作和时间属性独立管理。
- [x] **高效过滤**：空闲时跳过所有逻辑。
- [x] **空指针检查**：`enumKeyStateMachine` 检查参数有效性。

---

**文档结束**
