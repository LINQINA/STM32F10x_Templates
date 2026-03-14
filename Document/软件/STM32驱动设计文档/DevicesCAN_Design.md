# STM32F10x CAN 设计文档 (DevicesCAN)

| 属性 | 内容 |
| :--- | :--- |
| **模块名称** | DevicesCAN (CAN总线驱动模块) |
| **源文件** | `DevicesCAN.c` / `DevicesCAN.h` |
| **硬件依赖** | CAN1, GPIOA (PA11/PA12), 外部CAN收发器 (如TJA1050) |
| **软件依赖** | DevicesQueue, DevicesDelay, FreeRTOS (互斥量) |
| **版本** | v1.0 |
| **最后更新** | 2026-03-14 |

---

## 1. 设计目标 (Design Goals)

实现 STM32F10x 系列的 CAN 总线驱动：

1. **CAN1 通道支持**：配置 CAN1 外设，500Kbps 波特率
2. **中断接收**：FIFO0 消息挂起中断 + 环形队列，实现异步接收
3. **互斥发送**：FreeRTOS 递归互斥量保护，支持多任务并发调用
4. **统一 API**：与 UART 驱动保持一致的接口风格（发送/接收/查询/清空）
5. **硬件过滤**：利用 CAN 硬件过滤器筛选目标报文

---

## 2. CAN 总线基础 (CAN Bus Fundamentals)

### 2.1 CAN 总线概述

CAN (Controller Area Network) 是一种**多主**的**差分串行通信**总线，由博世 (Bosch) 设计，广泛用于汽车、工业控制等领域。

与 UART 的核心区别：

| 对比项 | UART | CAN |
| :--- | :--- | :--- |
| **拓扑** | 点对点（一对一） | 总线型（多对多） |
| **寻址** | 无（靠物理连接区分） | 报文ID（不是节点地址） |
| **仲裁** | 无 | 非破坏性位仲裁 |
| **信号** | 单端（TX/RX） | 差分（CAN_H / CAN_L） |
| **错误处理** | 简单（校验位） | 完善（CRC + ACK + 错误帧 + 故障界定） |
| **协议层** | 仅物理层，协议需自定义 | 硬件实现完整数据链路层 |
| **最大速率** | 取决于时钟精度 | 1Mbps（经典CAN） |

### 2.2 物理层架构

CAN 物理层分为两个硬件：**CAN控制器**（集成在MCU内部）和**CAN收发器**（外部芯片）。

```
节点A                                                    节点B
┌──────────────────────┐                    ┌──────────────────────┐
│        MCU           │                    │        MCU           │
│  ┌────────────────┐  │                    │  ┌────────────────┐  │
│  │  CAN 控制器     │  │                    │  │  CAN 控制器     │  │
│  │  (CAN1外设)     │  │                    │  │  (CAN1外设)     │  │
│  │                 │  │                    │  │                 │  │
│  │  负责:          │  │                    │  │  负责:          │  │
│  │  - 帧的组装/解析│  │                    │  │  - 帧的组装/解析│  │
│  │  - CRC计算      │  │                    │  │  - CRC计算      │  │
│  │  - 位仲裁       │  │                    │  │  - 位仲裁       │  │
│  │  - 错误检测     │  │                    │  │  - 错误检测     │  │
│  │  - 位时序控制   │  │                    │  │  - 位时序控制   │  │
│  └──┬────────┬──┘  │                    │  └──┬────────┬──┘  │
│     │CAN_TX  │CAN_RX │                    │     │CAN_TX  │CAN_RX │
└─────┼────────┼───────┘                    └─────┼────────┼───────┘
      │ (0/1)  │ (0/1)                            │ (0/1)  │ (0/1)
      │逻辑电平│逻辑电平                           │逻辑电平│逻辑电平
┌─────┼────────┼───────┐                    ┌─────┼────────┼───────┐
│  CAN 收发器 (如TJA1050)│                    │  CAN 收发器 (如TJA1050)│
│                       │                    │                       │
│  负责:                │                    │  负责:                │
│  - 逻辑电平 ↔ 差分电平│                    │  - 逻辑电平 ↔ 差分电平│
│  - 电气隔离/保护      │                    │  - 电气隔离/保护      │
└──┬──────────────┬────┘                    └──┬──────────────┬────┘
   │ CAN_H        │ CAN_L                      │ CAN_H        │ CAN_L
═══╪══════════════╪═════════════════════════════╪══════════════╪═══
   │              │        CAN 总线              │              │
   └──────────────┴─────────────────────────────┴──────────────┘
                          │                │
                         120Ω            120Ω
                       (终端电阻)       (终端电阻)
```

- **CAN控制器**（MCU内部）：负责协议处理——帧组装/解析、CRC、仲裁、ACK、错误处理，全部硬件自动完成
- **CAN收发器**（外部芯片，如 TJA1050）：负责将控制器输出的逻辑 0/1 转换为总线上的差分电平
- **CAN_TX / CAN_RX**：MCU 与收发器之间的连接，传输普通逻辑电平
- **CAN_H / CAN_L**：总线上的差分信号线
- **120Ω 终端电阻**：总线两端各一个，匹配双绞线特性阻抗，吸收信号反射（与 RS485 原理相同）

### 2.3 差分信号与显性/隐性电平

CAN 收发器将逻辑电平转换为差分信号：

```
                    CAN_H    CAN_L    压差(H-L)
                  ┌────────┬────────┬──────────┐
  逻辑 0 ──────►  │ 3.5V   │ 1.5V   │   2.0V   │  ← 显性 (Dominant)
  (显性)          ├────────┼────────┼──────────┤
  逻辑 1 ──────►  │ 2.5V   │ 2.5V   │   0.0V   │  ← 隐性 (Recessive)
  (隐性)          └────────┴────────┴──────────┘
```

**关键概念：显性 (Dominant) = 逻辑0，隐性 (Recessive) = 逻辑1**。当总线上同时有节点发显性和隐性时，**显性会"压制"隐性**，总线呈现显性电平。这个特性是 CAN 位仲裁的物理基础。

### 2.4 数据帧结构

数据帧是 CAN 上最常用的帧类型，用来携带数据。

**标准数据帧（11位ID）：**

```
 SOF    仲裁段               控制段         数据段           CRC段         ACK段      EOF
┌───┬─────────────┬───┬───┬────┬────────┬──────────────┬───────────┬───┬───┬───┬────────┐
│ S │  ID[10:0]   │R  │I  │ r  │DLC[3:0]│ Data[0~8字节]│ CRC[14:0] │ D │ACK│ACK│  EOF   │
│ O │  11位标识符  │T  │D  │ 0  │数据长度 │              │ 15位CRC   │ E │SLT│DEL│ 7位隐性│
│ F │             │R  │E  │    │码(0~8) │              │           │ L │   │   │        │
└───┴─────────────┴───┴───┴────┴────────┴──────────────┴───────────┴───┴───┴───┴────────┘
```

各段说明：

| 字段 | 位数 | 说明 |
| :--- | :--- | :--- |
| **SOF** | 1 | 帧起始，1位显性(0)，所有节点靠此下降沿同步 |
| **ID** | 11 | 标识符，越小优先级越高，不是节点地址而是报文标识 |
| **RTR** | 1 | 0=数据帧，1=远程帧 |
| **IDE** | 1 | 0=标准帧，1=扩展帧 |
| **DLC** | 4 | 数据长度码，取值 0~8 |
| **Data** | 0~64 | 实际数据，最多 8 字节（经典CAN） |
| **CRC** | 15+1 | 15位CRC校验 + 1位界定符 |
| **ACK** | 1+1 | 1位应答间隙 + 1位界定符 |
| **EOF** | 7 | 帧结束，7位隐性 |

### 2.5 数据帧与远程帧

远程帧用于请求其他节点发送数据，与数据帧有两个区别：

| 区别 | 数据帧 | 远程帧 |
| :--- | :--- | :--- |
| **RTR位** | 0（显性） | 1（隐性） |
| **数据段** | 包含 0~8 字节数据 | 没有数据段（不是数据为0，是整个段不存在） |

远程帧的 DLC 仍然有值，表示"请求多少字节的数据"。实际项目中远程帧用得较少，大多数 CAN 协议使用数据帧定时/事件发送。

### 2.6 标准帧与扩展帧

| 对比 | 标准帧 | 扩展帧 |
| :--- | :--- | :--- |
| **ID位数** | 11位 | 29位 |
| **IDE位** | 0（显性） | 1（隐性） |
| **仲裁优先级** | 更高（前11位相同时） | 更低 |

当标准帧和扩展帧前11位ID相同时，仲裁到 IDE 位：标准帧 IDE=0（显性）胜出。

### 2.7 仲裁机制

CAN 是多主结构，多个节点可同时发送。仲裁采用**非破坏性位仲裁**：

- 每个节点一边发送一边回读总线电平
- 当某节点发隐性(1)但读回显性(0)，说明有更高优先级报文在发
- 该节点立刻退出仲裁，切换到接收模式
- **高优先级报文不受任何影响**，继续正常发送

仲裁失败的节点，如果发送邮箱还有数据，会等待总线空闲后重新发送。

### 2.8 ACK 应答机制

CAN 在硬件层面实现了应答确认，不需要软件处理：

```
发送方在 ACK 间隙发隐性(1):

发送方:    ──── 1 ────    (隐性，"谁收到了？")
接收方:    ──── 0 ────    (显性，"我收到了，CRC正确")
总线实际:  ──── 0 ────    (显性压制隐性 → 发送方知道有人收到)

如果没人应答:

发送方:    ──── 1 ────    (隐性)
总线实际:  ──── 1 ────    (还是隐性 → ACK错误 → 发送错误帧)
```

ACK 由 CAN 控制器硬件自动处理，接收方 CRC 校验通过后自动拉低总线，不需要软件发送任何数据。

### 2.9 同步机制

- **硬件同步**：总线发送起始帧 (SOF) 时，所有节点检测到下降沿，自动同步时钟
- **再同步**：传输过程中，利用数据位中的隐性→显性跳变沿修正时钟偏差，调整幅度不超过 SJW（再同步跳跃宽度）

两种同步均由 CAN 控制器硬件自动完成，软件只需配置位时序参数。

### 2.10 波特率计算

```
波特率 = APB1时钟频率 / Prescaler / (1 + BS1 + BS2)
```

其中 `1` 是同步段 (SYNC_SEG)，固定 1 个时间量子 (TQ)。

```
本项目配置:
  APB1 = 36MHz
  Prescaler = 4
  BS1 = 9TQ
  BS2 = 8TQ

  波特率 = 36,000,000 / 4 / (1 + 9 + 8) = 500Kbps
```

### 2.11 发送邮箱与接收 FIFO

**发送邮箱（3个）：**

CAN 控制器检测空闲邮箱 → 填入数据 → 由发送调度决定发送顺序。发送调度策略：先按 ID 仲裁，ID 相同则邮箱号小的先发。（若 `TransmitFifoPriority = ENABLE`，则改为纯 FIFO 顺序。）

**接收 FIFO（2个，每个3级深度）：**

总线数据 → 经过硬件过滤器 → 匹配的报文存入 FIFO → 软件读取。FIFO 满时的行为由 `ReceiveFifoLocked` 决定：DISABLE 时新报文覆盖最旧报文，ENABLE 时丢弃新报文。

### 2.12 硬件过滤器

STM32 CAN 过滤器有 4 种组合模式：

| 模式 | 寄存器用途 | 可过滤数量 |
| :--- | :--- | :--- |
| 32位掩码模式 | 一个做ID，一个做掩码（bit=1 必须匹配，bit=0 不关心） | 1组（一批ID） |
| 32位列表模式 | 两个寄存器都做ID，精确匹配 | 2个ID |
| 16位掩码模式 | 拆成2对，各一个ID一个掩码 | 2组 |
| 16位列表模式 | 拆成4个，都做ID | 4个ID |

### 2.13 CAN 控制器模式

**测试模式（初始化时配置）：**

| 模式 | 发送 | 接收 | 总线可见 | 用途 |
| :--- | :--- | :--- | :--- | :--- |
| **正常模式** | 正常 | 正常 | 是 | 总线正常节点 |
| **静默模式** | 只发隐性(1) | 正常 | 否 | 总线流量监听 |
| **环回模式** | 发送到自身接收 | 正常 | 是 | 自检（总线可见） |
| **环回静默模式** | 发送到自身接收 | 不接收总线 | 否 | 自检（不影响总线） |

**工作状态：**

| 状态 | SLAK | INAK | 说明 |
| :--- | :--- | :--- | :--- |
| **睡眠模式** | 1 | 0 | 复位后默认状态，降低功耗 |
| **初始化模式** | 0 | 1 | 配置寄存器（波特率、过滤器等），不能收发 |
| **正常模式** | 0 | 0 | CAN 总线同步，开始收发 |

HAL 库封装了状态切换：`HAL_CAN_Init()` 进入初始化模式完成配置，`HAL_CAN_Start()` 切换到正常模式。

---

## 3. 硬件资源映射 (Hardware Resource Mapping)

### 3.1 CAN 通道定义

| 通道 | 外设 | 用途 | 波特率 |
| :--- | :--- | :--- | :--- |
| CAN1 | CAN1 | 上位机通信 | 500Kbps |

### 3.2 GPIO 引脚配置

| 引脚 | 功能 | 模式 | 上拉 | 速度 |
| :--- | :--- | :--- | :--- | :--- |
| PA11 | CAN1_RX | 输入 | 上拉 | - |
| PA12 | CAN1_TX | 复用推挽输出 | - | HIGH |

### 3.3 中断优先级配置

| 中断源 | 抢占优先级 | 子优先级 | 说明 |
| :--- | :--- | :--- | :--- |
| USB_LP_CAN1_RX0_IRQn | 7 | 0 | CAN1 FIFO0 接收中断 |

> **注意**：STM32F103 上 CAN1 RX0 与 USB LP 共用同一个中断向量，中断函数名必须为 `USB_LP_CAN1_RX0_IRQHandler`。

### 3.4 接收缓冲区配置

| 缓冲区 | 大小 | 模式 | 说明 |
| :--- | :--- | :--- | :--- |
| g_TypeQueueCanHostRead | 512 字节 | queueModeLock（满时拒绝写入） | CAN1 接收环形队列 |

---

## 4. 模块架构 (Module Architecture)

### 4.1 整体架构图

```
┌──────────────────────────────────────────────────────────────────────┐
│                          应用层 (Application)                         │
│                    上位机协议解析 / 数据处理任务                        │
└─────────────────────────────────┬────────────────────────────────────┘
                                  │
                                  ▼
┌──────────────────────────────────────────────────────────────────────┐
│                     CAN 驱动层 (DevicesCAN)                           │
│  ┌─────────────────────────────────────────────────────────────────┐ │
│  │                        发送接口                                  │ │
│  │  cCanSendDatas()                                                │ │
│  └─────────────────────────────────────────────────────────────────┘ │
│  ┌─────────────────────────────────────────────────────────────────┐ │
│  │                        接收接口                                  │ │
│  │  cCanReceiveByte()     cCanReceiveDatas()                       │ │
│  │  iCanReceiveAllDatas() iCanReceiveLengthGet()                   │ │
│  │  cCanReceiveClear()                                             │ │
│  └─────────────────────────────────────────────────────────────────┘ │
│  ┌─────────────────────────────────────────────────────────────────┐ │
│  │                        控制接口                                  │ │
│  │  vCan1Init()                                                    │ │
│  └─────────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────┬────────────────────────────────────┘
                                  │
                                  ▼
┌──────────────────────────────────────────────────────────────────────┐
│                       环形队列 (DevicesQueue)                         │
│                    g_TypeQueueCanHostRead (512B)                      │
└─────────────────────────────────┬────────────────────────────────────┘
                                  │
                                  ▼
┌──────────────────────────────────────────────────────────────────────┐
│                      中断服务层 (stm32f1xx_it.c)                      │
│  ┌───────────────────────────────────────────────────────────────┐   │
│  │ USB_LP_CAN1_RX0_IRQHandler()                                  │   │
│  │ → HAL_CAN_GetRxMessage() → enumQueuePushDatas()              │   │
│  └───────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────┬────────────────────────────────────┘
                                  │
                                  ▼
┌──────────────────────────────────────────────────────────────────────┐
│                         HAL 库 / 硬件外设                             │
│                     CAN1    GPIOA    NVIC                             │
└──────────────────────────────────────────────────────────────────────┘
```

### 4.2 数据流图 — 接收路径

```
    CAN 总线
       │
       ▼
┌──────────────────────────────────────────┐
│            CAN1 硬件过滤器                │
│  32位列表模式，过滤标准帧 ID 0x350       │
└──────────────────┬───────────────────────┘
                   │ 匹配通过
                   ▼
┌──────────────────────────────────────────┐
│         CAN1 接收 FIFO0 (3级深度)         │
└──────────────────┬───────────────────────┘
                   │ FIFO 消息挂起中断
                   ▼
┌──────────────────────────────────────────┐
│   USB_LP_CAN1_RX0_IRQHandler()           │
│   while (FIFO0 有消息)                    │
│   {                                      │
│     HAL_CAN_GetRxMessage() → rxData[8]  │
│     enumQueuePushDatas() → 环形队列      │
│   }                                      │
└──────────────────┬───────────────────────┘
                   │
                   ▼
┌──────────────────────────────────────────┐
│     环形队列 g_TypeQueueCanHostRead       │
│                 512 字节                  │
└──────────────────┬───────────────────────┘
                   │
                   ▼
┌──────────────────────────────────────────┐
│           应用层读取（主循环/任务）        │
│   cCanReceiveDatas() / iCanReceiveAll()  │
└──────────────────────────────────────────┘
```

### 4.3 数据流图 — 发送路径

```
┌──────────────────────────────────────────┐
│              应用层调用                    │
│   cCanSendDatas(CAN1, id, data, len)     │
└──────────────────┬───────────────────────┘
                   │
                   ▼
┌──────────────────────────────────────────┐
│         获取 FreeRTOS 递归互斥量          │
│   xSemaphoreTakeRecursive()              │
└──────────────────┬───────────────────────┘
                   │
                   ▼
┌──────────────────────────────────────────┐
│        等待发送邮箱空闲                   │
│   while(GetTxMailboxesFreeLevel != 3)    │
│       vTaskDelay(50ms)                   │
└──────────────────┬───────────────────────┘
                   │
                   ▼
┌──────────────────────────────────────────┐
│      HAL_CAN_AddTxMessage()              │
│      → 数据写入发送邮箱 → 硬件发送       │
└──────────────────┬───────────────────────┘
                   │
                   ▼
┌──────────────────────────────────────────┐
│         释放互斥量                        │
│   xSemaphoreGiveRecursive()              │
└──────────────────────────────────────────┘
```

---

## 5. 软件实现细节 (Implementation Details)

### 5.1 初始化流程

```c
void vCan1Init(void) 
{
    /* 1. 使能时钟 */
    __HAL_RCC_CAN1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* 2. 配置 GPIO: RX (PA11 输入上拉), TX (PA12 复用推挽) */

    /* 3. 配置 CAN 参数 */
    g_can1_handler.Instance  = CAN1;
    g_can1_handler.Init.Mode = CAN_MODE_NORMAL;
    
    /* 4. 波特率: 36MHz / 4 / (1+9+8) = 500Kbps */
    g_can1_handler.Init.Prescaler     = 4;
    g_can1_handler.Init.TimeSeg1      = CAN_BS1_9TQ;
    g_can1_handler.Init.TimeSeg2      = CAN_BS2_8TQ;
    g_can1_handler.Init.SyncJumpWidth = CAN_SJW_1TQ;

    /* 5. CAN 功能配置 */
    g_can1_handler.Init.AutoBusOff           = DISABLE;
    g_can1_handler.Init.AutoRetransmission   = DISABLE;
    g_can1_handler.Init.AutoWakeUp           = DISABLE;
    g_can1_handler.Init.ReceiveFifoLocked    = DISABLE;
    g_can1_handler.Init.TimeTriggeredMode    = DISABLE;
    g_can1_handler.Init.TransmitFifoPriority = DISABLE;

    HAL_CAN_Init(&g_can1_handler);

    /* 6. 配置过滤器: 32位列表模式，标准帧 ID 0x350 */
    /* 7. 配置 NVIC 中断优先级 */
    /* 8. 启用 FIFO0 消息挂起中断 */
    /* 9. HAL_CAN_Start() 切换到正常模式 */
}
```

CAN 功能配置说明：

| 配置项 | 设置 | 说明 |
| :--- | :--- | :--- |
| AutoBusOff | DISABLE | 禁止自动离线管理，总线关闭后需软件干预恢复 |
| AutoRetransmission | DISABLE | 仲裁失败或发送错误后不自动重发 |
| AutoWakeUp | DISABLE | 不自动唤醒，睡眠模式需软件唤醒 |
| ReceiveFifoLocked | DISABLE | FIFO满时新报文覆盖最旧报文 |
| TimeTriggeredMode | DISABLE | 不使用时间触发通信 |
| TransmitFifoPriority | DISABLE | 发送按 ID 优先级调度（非 FIFO 顺序） |

### 5.2 过滤器配置

```c
CAN_FilterTypeDef can_filterconfig;
can_filterconfig.FilterMode           = CAN_FILTERMODE_IDLIST;   /* 32位列表模式 */
can_filterconfig.FilterScale          = CAN_FILTERSCALE_32BIT;
can_filterconfig.FilterIdHigh         = (0x350 << 5);            /* 标准帧 ID 左移5位 */
can_filterconfig.FilterIdLow          = 0x0000;
can_filterconfig.FilterMaskIdHigh     = 0x0000;                  /* 列表模式第二个ID */
can_filterconfig.FilterMaskIdLow      = 0x0000;
can_filterconfig.FilterBank           = 0;
can_filterconfig.FilterFIFOAssignment = CAN_FilterFIFO0;
can_filterconfig.FilterActivation     = CAN_FILTER_ENABLE;
can_filterconfig.SlaveStartFilterBank = 14;
```

当前为 32 位列表模式，`FilterIdHigh` 配置了标准帧 ID 0x350，`FilterMaskIdHigh` 为 0x0000 相当于第二个 ID 为 0x000。

> **标准帧ID左移5位的原因**：32位过滤器寄存器的高16位中，标准帧11位ID占 bit[15:5]，低5位为 IDE/RTR/扩展ID高位。

### 5.3 接收中断处理

```c
void USB_LP_CAN1_RX0_IRQHandler(void)
{
    CAN_RxHeaderTypeDef can1_rxheader;
    uint8_t rxData[8];
    
    while (HAL_CAN_GetRxFifoFillLevel(&g_can1_handler, CAN_RX_FIFO0) > 0)
    {
        if (HAL_CAN_GetRxMessage(&g_can1_handler, CAN_RX_FIFO0, &can1_rxheader, rxData) == HAL_OK)
        {
            if (can1_rxheader.IDE == CAN_ID_STD)
            {
                enumQueuePushDatas(&g_TypeQueueCanHostRead, rxData, can1_rxheader.DLC);
            }
        }
    }
}
```

接收流程：循环读取 FIFO0 中所有待处理消息 → 筛选标准帧 → 将数据部分入队。

> **当前只入队数据字节，帧ID/DLC等信息未保留**。头文件中定义了 `CanPackType` 结构体（含 ff、ft、id、length、datas），可用于后续需要保留完整帧信息的场景。

### 5.4 发送函数

```c
int8_t cCanSendDatas(uint32_t can_periph, uint32_t uiID, void *pvDatas, int32_t iLength)
{
    xSemaphoreTakeRecursive(g_xCan1Semaphore, portMAX_DELAY);

    g_can1_txheader.ExtId = uiID;
    g_can1_txheader.DLC   = (iLength > 8) ? 8 : iLength;
    g_can1_txheader.IDE   = CAN_ID_EXT;
    g_can1_txheader.RTR   = CAN_RTR_DATA;

    while(HAL_CAN_GetTxMailboxesFreeLevel(&g_can1_handler) != 3)
        vTaskDelay(50 / portTICK_RATE_MS);

    HAL_CAN_AddTxMessage(&g_can1_handler, &g_can1_txheader, pvDatas, &uiTxMail);

    xSemaphoreGiveRecursive(g_xCan1Semaphore);
    return cError;
}
```

发送使用扩展帧 (`CAN_ID_EXT`)，数据帧 (`CAN_RTR_DATA`)，DLC 自动限制为最大 8 字节。

### 5.5 接收接口封装

```c
/* 接收指定长度数据 */
int8_t cCanReceiveDatas(uint32_t can_periph, void *pvDatas, int32_t iLength);

/* 接收所有可用数据 */
int32_t iCanReceiveAllDatas(uint32_t can_periph, void *pvDatas, int32_t iLengthLimit);

/* 查询接收缓冲区数据长度 */
int32_t iCanReceiveLengthGet(uint32_t can_periph);

/* 清空接收缓冲区 */
int8_t cCanReceiveClear(uint32_t can_periph);
```

接收接口从环形队列 `g_TypeQueueCanHostRead` 取数据，与 UART 驱动的接收接口模式完全一致。

---

## 6. API 总览 (API Reference)

### 6.1 初始化接口

| 函数 | 功能 | 参数 | 返回值 |
| :--- | :--- | :--- | :--- |
| `vCan1Init()` | 初始化 CAN1 外设 | 无 | 无 |

### 6.2 发送接口

| 函数 | 功能 | 参数 | 返回值 |
| :--- | :--- | :--- | :--- |
| `cCanSendDatas()` | 发送 CAN 数据帧 | 外设, ID, 数据指针, 长度 | 0: 成功, 1: 参数错误 |

### 6.3 接收接口

| 函数 | 功能 | 参数 | 返回值 |
| :--- | :--- | :--- | :--- |
| `cCanReceiveByte()` | 接收单字节 | 外设, 数据指针 | 0: 成功, 非0: 失败 |
| `cCanReceiveDatas()` | 接收指定长度数据 | 外设, 数据指针, 长度 | 0: 成功, 3: 数据不足, 4: 队列错误 |
| `iCanReceiveAllDatas()` | 接收所有可用数据 | 外设, 数据指针, 最大长度 | 实际读取长度 |
| `iCanReceiveLengthGet()` | 获取缓冲区数据长度 | 外设 | 数据长度 |
| `cCanReceiveClear()` | 清空接收缓冲区 | 外设 | 0: 成功, 1: 通道错误 |

### 6.4 错误码定义

| 返回值 | 含义 |
| :--- | :--- |
| 0 | 成功 |
| 1 | 参数错误（空指针或通道无效） |
| 3 | 数据不足（队列内数据少于请求长度） |
| 4 | 队列操作失败 |

---

## 7. 使用示例 (Usage Examples)

### 7.1 基本初始化

```c
#include "DevicesCAN.h"
#include "DevicesQueue.h"

void vUserSystemInit(void)
{
    enumQueueInit();
    vCan1Init();
    // ...
}
```

### 7.2 发送数据

```c
uint8_t txData[] = {0x01, 0x02, 0x03, 0x04};
cCanSendDatas((uint32_t)CAN1, 0x12345678, txData, sizeof(txData));
```

### 7.3 接收数据

```c
uint8_t rxBuff[128];
int32_t rxLen;

rxLen = iCanReceiveAllDatas((uint32_t)CAN1, rxBuff, sizeof(rxBuff));
if (rxLen > 0)
{
    // 处理接收到的数据
}
```

### 7.4 查询与清空

```c
if (iCanReceiveLengthGet((uint32_t)CAN1) >= FRAME_MIN_SIZE)
{
    // 有足够数据，可以解析
}

cCanReceiveClear((uint32_t)CAN1);
```

---

## 8. 关键设计决策 (Design Decisions)

### 8.1 为什么接收使用中断 + 环形队列？

| 方案 | 优点 | 缺点 |
| :--- | :--- | :--- |
| 轮询接收 | 实现简单 | CPU 占用高，高频报文易丢失 |
| **中断 + 环形队列** | 实时性好，生产者-消费者解耦 | 需要额外缓冲区内存 |
| RTOS 消息队列 | 自带阻塞/唤醒 | 中断中操作有限制，开销更大 |

CAN 没有 DMA 接收（不同于 UART），硬件 FIFO 只有 3 级深度，必须通过中断及时取走数据，环形队列提供了充足的缓冲空间。

### 8.2 为什么发送使用递归互斥量？

多个 RTOS 任务可能同时调用 `cCanSendDatas()`，递归互斥量保证同一时刻只有一个任务操作发送邮箱和全局发送头结构体 `g_can1_txheader`，避免数据竞争。

### 8.3 接口风格与 UART 驱动一致

CAN 和 UART 的接收接口（`cXxxReceiveDatas`、`iXxxReceiveAllDatas`、`iXxxReceiveLengthGet`、`cXxxReceiveClear`）保持一致的命名和语义，降低上层应用的学习成本，方便切换通信方式。

---

## 9. 常见问题与调试 (Troubleshooting)

### Q1: CAN 中断不触发

**可能原因**：
1. 中断函数名不匹配：STM32F103 上必须用 `USB_LP_CAN1_RX0_IRQHandler`，不能用 `CAN1_RX0_IRQHandler`
2. `HAL_CAN_Start()` 未调用，CAN 还在初始化模式
3. `HAL_CAN_ActivateNotification()` 未使能 FIFO0 消息挂起中断
4. 过滤器未匹配到任何报文

### Q2: 发送无响应

**可能原因**：
1. 总线上没有其他节点，无人回复 ACK → ACK 错误
2. 波特率与其他节点不一致
3. CAN 收发器未供电或连接异常
4. 终端电阻缺失导致信号反射

**调试方法**：先切换到环回模式 (`CAN_MODE_LOOPBACK`) 验证软件逻辑，排除硬件问题。

### Q3: 接收丢数据

**可能原因**：
1. 环形队列溢出（512字节不够）
2. 应用层处理太慢，队列积压
3. 中断优先级被抢占，FIFO 来不及清空

---

## 10. 代码质量自检 (Self-Check)

- [x] **空指针检查**：发送和接收函数检查输入参数有效性
- [x] **DLC 边界保护**：发送时 DLC 自动限制为最大 8
- [x] **互斥保护**：发送使用 FreeRTOS 递归互斥量
- [x] **中断安全**：中断中仅做入队操作，不做耗时处理
- [x] **模块解耦**：接收数据通过环形队列传递，中断与应用层解耦
- [x] **命名规范**：函数以 `v`/`c`/`i` 前缀区分返回值类型

---

## 11. 版本变更记录 (Changelog)

| 版本 | 日期 | 变更内容 |
| :--- | :--- | :--- |
| v1.0 | 2026-03-14 | 初始版本，支持 CAN1 中断接收 + 互斥发送 |

---

**文档结束**
