# STM32F10x UART 设计文档 (DevicesUart)

| 属性 | 内容 |
| :--- | :--- |
| **模块名称** | DevicesUart (串口驱动模块) |
| **源文件** | `DevicesUart.c` / `DevicesUart.h` |
| **硬件依赖** | USART1, USART2, DMA1 |
| **软件依赖** | DevicesQueue, DevicesDelay |
| **版本** | v1.0 |
| **最后更新** | 2026-02-04 |

---

## 1. 设计目标 (Design Goals)

实现 STM32F10x 系列的通用串口驱动：

1. **多通道支持**：支持 USART1 (LOG 日志) 和 USART2 (BUS 总线) 两个通道
2. **DMA 收发**：发送和接收均支持 DMA，降低 CPU 占用
3. **异步接收**：DMA 循环接收 + 空闲中断 + 环形队列，实现不定长数据帧接收
4. **统一 API**：通过通道参数选择串口，上层代码无需关心底层细节
5. **错误恢复**：自动检测并恢复通信错误（溢出、帧错误等）

---

## 2. 异步串口通信原理 (UART Fundamentals)

### 2.1 什么是异步通信？

UART (Universal Asynchronous Receiver/Transmitter) 是一种 **异步串行通信** 方式：

- **异步**：发送端和接收端没有共同的时钟信号，各自使用本地时钟
- **串行**：数据一位一位地顺序传输，而非并行传输

```
同步通信 vs 异步通信:

同步通信 (如 SPI):
    CLK  ──┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌──
           └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘
    DATA ──┐   ┌───┐   ┌───────┐   ┌───┐
           └───┘   └───┘       └───┘   └───
           ↑   ↑   ↑   ↑   ↑   ↑   ↑   ↑
           │   │   │   │   │   │   │   │
         时钟沿采样数据，收发双方共享时钟


异步通信 (UART):
    TX   ────┐     ┌───┬───┬───┬───┬───┬───┬───┬───┬───┐
             └─────┘ 0 │ 1 │ 0 │ 1 │ 0 │ 1 │ 0 │ 1 │   │
             ↑     ↑                                   ↑
           空闲  起始位                              停止位
           
         没有时钟线，靠起始位同步，靠约定的波特率采样
```

### 2.2 数据帧结构

一个完整的 UART 数据帧由以下部分组成：

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          UART 数据帧结构                                     │
└─────────────────────────────────────────────────────────────────────────────┘

空闲态                                                                 空闲态
(高电平)                                                               (高电平)
────┐     ┌───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┐     ┌────────
    │     │ D │ D │ D │ D │ D │ D │ D │ D │ P │ S │ S │     │
    └─────┤ 0 │ 1 │ 2 │ 3 │ 4 │ 5 │ 6 │ 7 │   │ T │ T │─────┘
    ↑     ↑                               ↑   ↑   ↑
    │     │                               │   │   │
  空闲  起始位        数据位 (5-9位)     校验位 停止位
  (1)   (0)          (LSB 先发)         (可选) (1或2位)


各部分说明:
┌──────────┬─────────┬────────────────────────────────────────────┐
│   名称   │  电平   │                    说明                     │
├──────────┼─────────┼────────────────────────────────────────────┤
│  空闲态  │  高(1)  │ 无数据传输时，TX 线保持高电平               │
│  起始位  │  低(0)  │ 1 位，标志数据帧开始，用于同步              │
│  数据位  │  0/1    │ 5-9 位，通常为 8 位，LSB 先发送             │
│  校验位  │  0/1    │ 0-1 位，可选，用于错误检测                  │
│  停止位  │  高(1)  │ 1-2 位，标志数据帧结束                      │
└──────────┴─────────┴────────────────────────────────────────────┘
```

### 2.3 起始位检测 — 从 1→0 的跳变开始接收

**关键原理**：接收器通过检测 **TX 线从高电平(1)跳变到低电平(0)** 来识别一帧数据的开始。

```
起始位检测过程:

空闲态 (TX = 高电平 = 1)
    │
    │    ←── 接收器持续监测 TX 线电平
    ▼
────────────┐
            │     ←── 检测到下降沿 (1→0)
            └────      这就是起始位！开始计时
                 │
                 ▼
            ┌─────────────────────────────────
            │ 从起始位中间开始，每隔 1 位时间采样一次
            │
            │    ↓     ↓     ↓     ↓     ↓
            └────┬─────┬─────┬─────┬─────┬────
                 D0    D1    D2    D3    D4 ...
                 
                 
详细采样时序:

TX 信号:  ────┐     ┌─────┐     ┌───────────┐     ┌─────
              │     │     │     │           │     │
              └─────┘     └─────┘           └─────┘
              
              ↑     ↑     ↑     ↑     ↑     ↑     ↑
              │     │     │     │     │     │     │
时间:         0    0.5   1.5   2.5   3.5   4.5   5.5  (位时间)
              │     │     │     │     │     │     │
采样点:    起始位  D0    D1    D2    D3    D4    D5
           检测    
           
           在每个位的中间采样，抗干扰能力最强
```

**为什么是 1→0 而不是 0→1？**

- 空闲态为高电平(1)，这是 RS232 的历史约定
- 下降沿比上升沿更容易被硬件检测
- 起始位为低(0)，与空闲态(1)形成对比，便于识别

### 2.4 波特率与位时间

**波特率 (Baud Rate)**：每秒传输的位数 (bps)

```
常见波特率与位时间:

┌──────────┬───────────┬─────────────────────────────────────┐
│  波特率   │  位时间   │              说明                   │
├──────────┼───────────┼─────────────────────────────────────┤
│   9600   │  104.2 μs │ 低速，抗干扰强，长距离              │
│  19200   │   52.1 μs │                                     │
│  38400   │   26.0 μs │                                     │
│  57600   │   17.4 μs │                                     │
│ 115200   │    8.7 μs │ 常用，速度与稳定性平衡              │
│ 230400   │    4.3 μs │                                     │
│ 460800   │    2.2 μs │ 高速，对时钟精度要求高              │
│ 921600   │    1.1 μs │                                     │
└──────────┴───────────┴─────────────────────────────────────┘

计算公式:
位时间 = 1 / 波特率

例: 9600 bps → 位时间 = 1/9600 = 104.17μs


传输一个字节所需时间 (8N1 格式):
= (1 起始位 + 8 数据位 + 0 校验位 + 1 停止位) × 位时间
= 10 × 位时间

例: 115200 bps, 8N1 格式
= 10 × 8.68μs = 86.8μs/字节
≈ 11520 字节/秒
```

### 2.5 校验方式 (Parity)

校验位用于简单的错误检测，检查数据位中 "1" 的个数。

#### 2.5.1 无校验 (None)

```
数据帧格式 (8N1 = 8数据位, 无校验, 1停止位):

    ┌─────────────────────────────────────────┐
    │ START │ D0-D7 (8位数据) │ STOP │
    └─────────────────────────────────────────┘
    
    - 最常用的格式
    - 传输效率最高
    - 无错误检测能力，依赖上层协议 (如 CRC)
```

#### 2.5.2 奇校验 (Odd Parity)

```
规则: 数据位 + 校验位 中 "1" 的总数为奇数

数据 = 0x55 = 0101 0101 (4个1)
校验位 = 1 (使总数变成5个1，奇数)

    ┌─────────────────────────────────────────────────┐
    │ START │ 0101 0101 │ P=1 │ STOP │
    └─────────────────────────────────────────────────┘
                          ↑
                      校验位=1


数据 = 0xAA = 1010 1010 (4个1)
校验位 = 1 (使总数变成5个1，奇数)

    ┌─────────────────────────────────────────────────┐
    │ START │ 1010 1010 │ P=1 │ STOP │
    └─────────────────────────────────────────────────┘


数据 = 0x07 = 0000 0111 (3个1)
校验位 = 0 (总数已经是奇数，保持不变)

    ┌─────────────────────────────────────────────────┐
    │ START │ 0000 0111 │ P=0 │ STOP │
    └─────────────────────────────────────────────────┘
```

#### 2.5.3 偶校验 (Even Parity)

```
规则: 数据位 + 校验位 中 "1" 的总数为偶数

数据 = 0x55 = 0101 0101 (4个1)
校验位 = 0 (总数已经是偶数，保持不变)

    ┌─────────────────────────────────────────────────┐
    │ START │ 0101 0101 │ P=0 │ STOP │
    └─────────────────────────────────────────────────┘


数据 = 0x07 = 0000 0111 (3个1)
校验位 = 1 (使总数变成4个1，偶数)

    ┌─────────────────────────────────────────────────┐
    │ START │ 0000 0111 │ P=1 │ STOP │
    └─────────────────────────────────────────────────┘
```

#### 2.5.4 校验方式对比

| 校验方式 | 检错能力 | 传输效率 | 应用场景 |
| :--- | :--- | :--- | :--- |
| **无校验 (N)** | 无 | 最高 | 有上层 CRC 校验时 |
| **奇校验 (O)** | 检测单位错误 | 较低 | 老式设备、工业协议 |
| **偶校验 (E)** | 检测单位错误 | 较低 | Modbus RTU 常用 |

> **注意**：奇偶校验只能检测 **奇数个位** 的错误，如果同时有 2 位翻转则检测不到。实际项目中更推荐使用 CRC 校验。

### 2.6 硬件流控制 (Hardware Flow Control)

当接收方处理速度跟不上发送速度时，需要流控制来防止数据丢失。

#### 2.6.1 无流控 (None)

```
发送方:  TX ────────────────────────────► RX  :接收方
                    
         不管接收方是否准备好，持续发送
         接收方必须及时处理，否则会丢数据
```

#### 2.6.2 RTS/CTS 硬件流控

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        RTS/CTS 硬件流控                                      │
└─────────────────────────────────────────────────────────────────────────────┘

    发送方 (MCU-A)                              接收方 (MCU-B)
    ┌───────────────┐                          ┌───────────────┐
    │               │       TX ──────────────► │               │
    │               │                          │               │
    │               │       RX ◄────────────── │               │
    │               │                          │               │
    │               │      RTS ──────────────► │ CTS           │
    │               │                          │               │
    │               │      CTS ◄────────────── │ RTS           │
    └───────────────┘                          └───────────────┘
    
    RTS (Request To Send)  : 请求发送，输出信号
    CTS (Clear To Send)    : 允许发送，输入信号


工作流程:

1. 接收方准备好:  RTS_B = 低 (有效) ──► CTS_A = 低 ──► A 可以发送
2. 接收方缓冲区满: RTS_B = 高 (无效) ──► CTS_A = 高 ──► A 暂停发送
3. 接收方处理完毕: RTS_B = 低 (有效) ──► CTS_A = 低 ──► A 继续发送


时序图:

CTS (发送方输入):  ────┐          ┌──────────────┐          ┌────
                      └──────────┘              └──────────┘
                      ↑          ↑              ↑          ↑
                    允许发送  暂停发送        允许发送  暂停发送
                    
TX (发送方输出):   ────┼──────────┼──────────────┼──────────┼────
                      │▓▓▓▓▓▓▓▓▓▓│              │▓▓▓▓▓▓▓▓▓▓│
                      └──────────┘              └──────────┘
                        发送数据    暂停          发送数据
```

#### 2.6.3 软件流控 (XON/XOFF) — 了解即可

```
使用特殊字符控制流量:

XON  (0x11, Ctrl+Q): 继续发送
XOFF (0x13, Ctrl+S): 暂停发送

接收方缓冲区快满时发送 XOFF，处理完后发送 XON

缺点: 
- 这两个字符不能出现在数据中
- 响应延迟大
- 现代系统很少使用
```

#### 2.6.4 流控方式对比

| 流控方式 | 额外引脚 | 可靠性 | 适用场景 |
| :--- | :--- | :--- | :--- |
| **无流控** | 0 | 低 | 低速、有上层协议、DMA 缓冲 |
| **RTS/CTS** | 2 | 高 | 高速、接收方处理慢 |
| **XON/XOFF** | 0 | 中 | 终端、打印机（已很少用） |

### 2.7 常见配置格式表示法

```
格式: [波特率] [数据位][校验][停止位]

例子:
┌──────────────────┬─────────────────────────────────────┐
│      格式        │                含义                  │
├──────────────────┼─────────────────────────────────────┤
│  115200 8N1      │ 115200bps, 8数据位, 无校验, 1停止位  │
│  9600 8E1        │ 9600bps, 8数据位, 偶校验, 1停止位    │
│  9600 8O1        │ 9600bps, 8数据位, 奇校验, 1停止位    │
│  9600 8N2        │ 9600bps, 8数据位, 无校验, 2停止位    │
│  9600 7E1        │ 9600bps, 7数据位, 偶校验, 1停止位    │
└──────────────────┴─────────────────────────────────────┘

STM32 HAL 配置示例:

g_uart1_handle.Init.BaudRate   = 115200;
g_uart1_handle.Init.WordLength = UART_WORDLENGTH_8B;  // 8 数据位
g_uart1_handle.Init.StopBits   = UART_STOPBITS_1;     // 1 停止位
g_uart1_handle.Init.Parity     = UART_PARITY_NONE;    // 无校验
g_uart1_handle.Init.HwFlowCtl  = UART_HWCONTROL_NONE; // 无流控
```

### 2.8 帧错误类型

| 错误类型 | 缩写 | 原因 | 说明 |
| :--- | :--- | :--- | :--- |
| **帧错误** | FE | 停止位检测到低电平 | 波特率不匹配或噪声 |
| **校验错误** | PE | 校验位不匹配 | 数据传输出错 |
| **溢出错误** | ORE | DR 寄存器未及时读取 | 接收处理太慢 |
| **噪声错误** | NE | 采样点检测到噪声 | 信号质量差 |

### 2.9 串口常用中断

| 中断 | 中文名称 | 宏定义 | 触发条件 | 典型用途 |
| :--- | :--- | :--- | :--- | :--- |
| **RXNE** | 接收非空中断 | `UART_IT_RXNE` | 收到一个字节，DR 非空 | 逐字节中断接收 |
| **IDLE** | 串口空闲中断 | `UART_IT_IDLE` | 一帧结束，总线空闲 | 不定长帧接收 (配合DMA) ⭐ |
| **TXE** | 发送寄存器空中断 | `UART_IT_TXE` | DR 空，可写下一字节 | 连续发送多字节 |
| **TC** | 发送完成中断 | `UART_IT_TC` | 最后字节完全发出 | RS485 方向切换 ⭐ |
| **PE** | 校验错误中断 | `UART_IT_PE` | 校验错误 | 错误检测 |
| **ERR** | 错误中断 | `UART_IT_ERR` | ORE/NE/FE 错误 | 错误检测 |
| **CTS** | CTS变化中断 | `UART_IT_CTS` | CTS 引脚电平变化 | 硬件流控 |
| **LBD** | LIN断开检测中断 | `UART_IT_LBD` | LIN 断开检测 | LIN 总线 |

> **本驱动使用**：`UART_IT_IDLE` (串口空闲中断) + `UART_IT_ERR` (错误中断) + DMA 中断

### 2.10 重要理解：异步通信由硬件自动完成

**起始位检测、数据采样、校验计算、停止位验证 —— 这些全部由 UART 硬件外设自动完成！**

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    UART 硬件自动处理 vs 软件职责                             │
└─────────────────────────────────────────────────────────────────────────────┘

                         ┌─────────────────────────────────┐
                         │        UART 硬件外设            │
                         │      (全自动，无需软件干预)      │
                         │                                 │
   TX 引脚 ◄─────────────┤  发送移位寄存器                 │◄──── DR 寄存器
                         │  - 自动添加起始位 (0)           │       (软件写入)
                         │  - 自动逐位发送数据             │
                         │  - 自动计算并添加校验位         │
                         │  - 自动添加停止位 (1)           │
                         │                                 │
                         ├─────────────────────────────────┤
                         │                                 │
   RX 引脚 ─────────────►│  接收移位寄存器                 │────► DR 寄存器
                         │  - 自动检测起始位 (1→0 跳变)    │       (软件读取)
                         │  - 自动按波特率采样数据         │
                         │  - 自动校验并设置错误标志       │
                         │  - 自动检测停止位               │
                         │                                 │
                         └─────────────────────────────────┘


软件只需要做:
┌──────────────────────────────────────────────────────────────────────┐
│ 1. 配置参数：波特率、数据位、校验、停止位、流控                        │
│ 2. 发送：将数据写入 DR 寄存器 (或配置 DMA)                            │
│ 3. 接收：从 DR 寄存器读取数据 (或配置 DMA)                            │
│ 4. 错误处理：检查状态寄存器中的错误标志                               │
└──────────────────────────────────────────────────────────────────────┘

硬件自动完成:
┌──────────────────────────────────────────────────────────────────────┐
│ 1. 起始位：发送时自动添加，接收时自动检测 1→0 跳变                    │
│ 2. 数据位：按配置的位数自动移位发送/接收                              │
│ 3. 校验位：按配置自动计算/验证奇偶校验                                │
│ 4. 停止位：发送时自动添加，接收时自动验证                             │
│ 5. 波特率：硬件定时器自动控制每位的时间                               │
│ 6. 采样：在每位中间自动采样，通常采样 16 次取多数判决                 │
└──────────────────────────────────────────────────────────────────────┘
```

**这就是为什么 UART 叫"通用异步收发器"**：
- 它把复杂的异步通信时序封装在硬件中
- 软件只需要关心"发什么数据"和"收到什么数据"
- 不需要用 GPIO 模拟时序（那叫"软件串口"，效率低且占用 CPU）

```c
// 软件视角：发送一个字节就是写寄存器，硬件自动处理起始位/数据位/停止位
huart->Instance->DR = data;  // 写入后，硬件自动：起始位→8位数据→停止位

// 软件视角：接收一个字节就是读寄存器，硬件已经剥离了起始位/停止位
data = huart->Instance->DR;  // 读到的就是纯数据，起始位/停止位已被硬件处理
```

> **总结**：理解这一点很重要 —— 我们在软件中操作的是"纯数据"，所有的帧格式（起始位、停止位、校验位）都由 UART 硬件透明处理。这大大简化了软件开发，也保证了通信时序的精确性。

---

## 3. 硬件资源映射 (Hardware Resource Mapping)

### 2.1 串口通道定义

```c
#define UART_LOG  USART1    /* 日志输出串口 */
#define UART_BUS  USART2    /* 总线通信串口 */
```

| 通道 | 外设 | 用途 | 默认波特率 |
| :--- | :--- | :--- | :--- |
| `UART_LOG` | USART1 | 日志输出、调试打印 | 115200 |
| `UART_BUS` | USART2 | 总线通信（如 Modbus） | 9600 |

### 2.2 GPIO 引脚配置

| 串口 | 引脚 | 功能 | 模式 | 速度 |
| :--- | :--- | :--- | :--- | :--- |
| USART1 | PA9 | TX | 复用推挽输出 | HIGH |
| USART1 | PA10 | RX | 复用浮空输入 | - |
| USART2 | PA2 | TX | 复用推挽输出 | HIGH |
| USART2 | PA3 | RX | 复用浮空输入 | - |

### 2.3 DMA 通道配置

| 串口 | 方向 | DMA 通道 | 传输模式 | 优先级 |
| :--- | :--- | :--- | :--- | :--- |
| USART1 | TX | DMA1_Channel4 | Normal | LOW |
| USART1 | RX | DMA1_Channel5 | **Circular** | HIGH |
| USART2 | TX | DMA1_Channel7 | Normal | LOW |
| USART2 | RX | DMA1_Channel6 | **Circular** | HIGH |

**设计要点**：
- RX 使用 **Circular 循环模式**，DMA 在缓冲区末尾自动回到起始位置
- TX 使用 **Normal 普通模式**，每次发送完成后停止

### 2.4 DMA 缓冲区配置

```c
#define USART1_DMA_READ_LENGTH  (128)   /* USART1 DMA 接收缓冲区大小 */
#define USART1_DMA_SEND_LENGTH  (0)     /* 预留，当前未使用 */
#define USART2_DMA_READ_LENGTH  (128)   /* USART2 DMA 接收缓冲区大小 */
#define USART2_DMA_SEND_LENGTH  (0)     /* 预留，当前未使用 */

uint8_t g_USART1ReadDMABuff[USART1_DMA_READ_LENGTH];
uint8_t g_USART2ReadDMABuff[USART2_DMA_READ_LENGTH];
```

### 2.5 中断优先级配置

| 中断源 | 抢占优先级 | 子优先级 | 说明 |
| :--- | :--- | :--- | :--- |
| USART1_IRQn | 8 | 0 | USART1 空闲/错误中断 |
| DMA1_Channel4_IRQn | 8 | 0 | USART1 TX DMA |
| DMA1_Channel5_IRQn | 6 | 0 | USART1 RX DMA |
| USART2_IRQn | 3 | 3 | USART2 空闲/错误中断 |
| DMA1_Channel6_IRQn | 8 | 0 | USART2 RX DMA |
| DMA1_Channel7_IRQn | 6 | 0 | USART2 TX DMA |

---

## 3. 模块架构 (Module Architecture)

### 3.1 整体架构图

```
┌──────────────────────────────────────────────────────────────────────┐
│                          应用层 (Application)                         │
│              cLogPrintf() / Modbus协议解析 / 上位机通信               │
└─────────────────────────────────┬────────────────────────────────────┘
                                  │
                                  ▼
┌──────────────────────────────────────────────────────────────────────┐
│                      串口驱动层 (DevicesUart)                         │
│  ┌─────────────────────────────────────────────────────────────────┐ │
│  │                        发送接口                                  │ │
│  │  vUartSendDatas()      vUartDMASendDatas()                      │ │
│  │  vUartSendStrings()    vUartDMASendStrings()                    │ │
│  └─────────────────────────────────────────────────────────────────┘ │
│  ┌─────────────────────────────────────────────────────────────────┐ │
│  │                        接收接口                                  │ │
│  │  cUartReceiveByte()    cUartReceiveDatas()                      │ │
│  │  iUartReceiveAllDatas()    iUartReceiveLengthGet()              │ │
│  │  cUartReceiveClear()                                            │ │
│  └─────────────────────────────────────────────────────────────────┘ │
│  ┌─────────────────────────────────────────────────────────────────┐ │
│  │                        控制接口                                  │ │
│  │  vUart1Init()    vUart2Init()    vUartBaudrateSet()             │ │
│  └─────────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────┬────────────────────────────────────┘
                                  │
                                  ▼
┌──────────────────────────────────────────────────────────────────────┐
│                       环形队列 (DevicesQueue)                         │
│          g_TypeQueueUart0Read (512B)    g_TypeQueueUart1Read (512B)  │
└─────────────────────────────────┬────────────────────────────────────┘
                                  │
                                  ▼
┌──────────────────────────────────────────────────────────────────────┐
│                      中断服务层 (stm32f1xx_it.c)                      │
│  ┌───────────────┐  ┌───────────────┐  ┌───────────────────────────┐ │
│  │ USART1_IRQ    │  │ DMA1_Ch5_IRQ  │  │ vUSART1ReceiveCallback()  │ │
│  │ (空闲/错误)    │  │ (RX 半/全)    │  │ → enumQueuePushDatas()   │ │
│  └───────────────┘  └───────────────┘  └───────────────────────────┘ │
└─────────────────────────────────┬────────────────────────────────────┘
                                  │
                                  ▼
┌──────────────────────────────────────────────────────────────────────┐
│                         HAL 库 / 硬件外设                             │
│                  USART1/2    DMA1_Ch4/5/6/7    GPIOA                  │
└──────────────────────────────────────────────────────────────────────┘
```

### 3.2 数据流图 — 接收路径

```
                          ┌─────────────────────────────────────────┐
                          │            DMA 循环接收缓冲区            │
                          │     g_USART1ReadDMABuff[128]            │
    物理串口              │  ┌────┬────┬────┬────┬────┬────┬────┐   │
    USART1_RX ──DMA──────►│  │ D1 │ D2 │ D3 │ D4 │ ...│    │    │   │
                          │  └────┴────┴────┴────┴────┴────┴────┘   │
                          │           ▲                              │
                          │           │ DMA 指针 (NDTR)              │
                          └───────────┼─────────────────────────────┘
                                      │
                                      ▼
    ┌─────────────────────────────────────────────────────────────────┐
    │                    中断触发入队逻辑                              │
    │                                                                 │
    │   触发条件：                                                    │
    │   1. USART 空闲中断 (IDLE) — 一帧数据接收完成                   │
    │   2. DMA 半传输中断 (HT)   — 缓冲区过半                         │
    │   3. DMA 全传输中断 (TC)   — 缓冲区用完一轮                     │
    │                                                                 │
    │   vUSART1ReceiveCallback():                                     │
    │   - 计算 DMA 新位置 (uiMDANdtrNow)                              │
    │   - 处理缓冲区回绕                                              │
    │   - enumQueuePushDatas() 入队                                   │
    └─────────────────────────────────┬───────────────────────────────┘
                                      │
                                      ▼
    ┌─────────────────────────────────────────────────────────────────┐
    │                      环形队列 (g_TypeQueueUart0Read)             │
    │                           512 字节                              │
    │  ┌────┬────┬────┬────┬────┬────┬────┬────┬────┬────┐           │
    │  │ D1 │ D2 │ D3 │ D4 │ D5 │ D6 │    │    │    │    │           │
    │  └────┴────┴────┴────┴────┴────┴────┴────┴────┴────┘           │
    │       ▲                        ▲                                │
    │   pReadFrom                pWriteTo                             │
    └─────────────────────────────────┬───────────────────────────────┘
                                      │
                                      ▼
    ┌─────────────────────────────────────────────────────────────────┐
    │                    应用层读取 (主循环/任务)                      │
    │                                                                 │
    │   cUartReceiveDatas(UART_LOG, buff, len)                        │
    │   iUartReceiveAllDatas(UART_LOG, buff, maxLen)                  │
    │                                                                 │
    └─────────────────────────────────────────────────────────────────┘
```

### 3.3 数据流图 — 发送路径

```
    ┌─────────────────────────────────────────────────────────────────┐
    │                        应用层发送                               │
    │                                                                 │
    │   方式1: 阻塞轮询发送                                           │
    │   vUartSendDatas(UART_LOG, data, len)                           │
    │   vUartSendStrings(UART_LOG, "Hello\r\n")                       │
    │                                                                 │
    │   方式2: DMA 发送                                               │
    │   vUartDMASendDatas(UART_LOG, data, len)                        │
    │   vUartDMASendStrings(UART_LOG, "Hello\r\n")                    │
    └─────────────────────────────────┬───────────────────────────────┘
                                      │
             ┌────────────────────────┴────────────────────────┐
             │                                                 │
             ▼                                                 ▼
    ┌─────────────────────┐                       ┌─────────────────────┐
    │     阻塞轮询发送     │                       │      DMA 发送       │
    │                     │                       │                     │
    │ while(len--)        │                       │ HAL_UART_Transmit_  │
    │ {                   │                       │   DMA()             │
    │   wait TXE flag;    │                       │                     │
    │   DR = *data++;     │                       │ 等待 DMA 传输完成   │
    │ }                   │                       │ 等待 TC 标志        │
    └──────────┬──────────┘                       └──────────┬──────────┘
               │                                              │
               └──────────────────┬───────────────────────────┘
                                  │
                                  ▼
    ┌─────────────────────────────────────────────────────────────────┐
    │                         物理串口 TX                              │
    └─────────────────────────────────────────────────────────────────┘
```

---

## 4. 软件实现细节 (Implementation Details)

### 4.1 初始化流程

```c
void vUart1Init(void)
{
    /* 1. 使能时钟 */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();
    
    /* 2. 配置 GPIO */
    // PA9 TX: 复用推挽输出, HIGH速度
    // PA10 RX: 复用浮空输入
    
    /* 3. 配置 UART 参数 */
    g_uart1_handle.Instance = USART1;
    g_uart1_handle.Init.BaudRate = 115200;
    g_uart1_handle.Init.WordLength = UART_WORDLENGTH_8B;
    g_uart1_handle.Init.StopBits = UART_STOPBITS_1;
    g_uart1_handle.Init.Parity = UART_PARITY_NONE;
    g_uart1_handle.Init.Mode = UART_MODE_TX_RX;
    g_uart1_handle.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    HAL_UART_Init(&g_uart1_handle);
    
    /* 4. 初始化 DMA */
    vUart1DMAInit();
    
    /* 5. 配置 NVIC 中断 */
    HAL_NVIC_SetPriority(USART1_IRQn, 8, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
    
    /* 6. 使能中断 */
    __HAL_UART_ENABLE_IT(&g_uart1_handle, UART_IT_IDLE);  // 空闲中断
    __HAL_UART_ENABLE_IT(&g_uart1_handle, UART_IT_ERR);   // 错误中断
    
    /* 7. 启动 DMA 接收 */
    HAL_UART_Receive_DMA(&g_uart1_handle, g_USART1ReadDMABuff, USART1_DMA_READ_LENGTH);
}
```

### 4.2 DMA 初始化

```c
void vUart1DMAInit(void)
{
    __HAL_RCC_DMA1_CLK_ENABLE();
    
    /* TX DMA: Normal 模式 */
    g_dma_usart1_tx.Instance = DMA1_Channel4;
    g_dma_usart1_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    g_dma_usart1_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    g_dma_usart1_tx.Init.MemInc = DMA_MINC_ENABLE;
    g_dma_usart1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    g_dma_usart1_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    g_dma_usart1_tx.Init.Mode = DMA_NORMAL;         // 普通模式
    g_dma_usart1_tx.Init.Priority = DMA_PRIORITY_LOW;
    
    /* RX DMA: Circular 模式 */
    g_dma_usart1_rx.Instance = DMA1_Channel5;
    g_dma_usart1_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    g_dma_usart1_rx.Init.Mode = DMA_CIRCULAR;       // 循环模式 ⭐
    g_dma_usart1_rx.Init.Priority = DMA_PRIORITY_HIGH;
    // ... 其他配置
    
    /* 链接 DMA 到 UART */
    __HAL_LINKDMA(&g_uart1_handle, hdmatx, g_dma_usart1_tx);
    __HAL_LINKDMA(&g_uart1_handle, hdmarx, g_dma_usart1_rx);
}
```

### 4.3 接收回调 — 核心算法

```c
void vUSART1ReceiveCallback(void)
{
    static uint32_t uiMDANdtrOld = 0;   // 上次 DMA 指针位置
    uint32_t uiMDANdtrNow = 0;          // 当前 DMA 指针位置

    // 计算当前 DMA 已接收的字节数
    // NDTR 是剩余传输数量，总长度 - NDTR = 已接收数量
    while(uiMDANdtrOld != (uiMDANdtrNow = USART1_DMA_READ_LENGTH - __HAL_DMA_GET_COUNTER(&g_dma_usart1_rx)))
    {
        // 情况1: DMA 缓冲区发生回绕 (新位置 < 旧位置)
        if(uiMDANdtrNow < uiMDANdtrOld)
        {
            // 先把尾部数据入队
            enumQueuePushDatas(&g_TypeQueueUart0Read, 
                              &g_USART1ReadDMABuff[uiMDANdtrOld], 
                              USART1_DMA_READ_LENGTH - uiMDANdtrOld);
            uiMDANdtrOld = 0;  // 重置到起始位置
        }

        // 情况2: 正常连续数据
        enumQueuePushDatas(&g_TypeQueueUart0Read, 
                          &g_USART1ReadDMABuff[uiMDANdtrOld], 
                          uiMDANdtrNow - uiMDANdtrOld);
        
        uiMDANdtrOld = uiMDANdtrNow;
    }
}
```

**算法图示**：

```
情况1: 正常连续数据 (uiMDANdtrNow > uiMDANdtrOld)

DMA Buffer:
┌────┬────┬────┬────┬────┬────┬────┬────┐
│ D1 │ D2 │ D3 │ D4 │ D5 │ D6 │    │    │
└────┴────┴────┴────┴────┴────┴────┴────┘
           ▲              ▲
       Old=2          Now=5
       
入队: &Buff[2], 长度 = 5-2 = 3 字节 (D3, D4, D5)


情况2: 缓冲区回绕 (uiMDANdtrNow < uiMDANdtrOld)

DMA Buffer:
┌────┬────┬────┬────┬────┬────┬────┬────┐
│ D9 │ D10│    │    │    │ D6 │ D7 │ D8 │
└────┴────┴────┴────┴────┴────┴────┴────┘
      ▲                   ▲
   Now=1               Old=5
   
步骤1: 入队尾部 &Buff[5], 长度 = 8-5 = 3 字节 (D6, D7, D8)
步骤2: Old = 0
步骤3: 入队头部 &Buff[0], 长度 = 1-0 = 1 字节 (D9)
       (D10 在下次回调处理)
```

### 4.4 中断服务函数

```c
void USART1_IRQHandler(void) 
{ 
    /* 1. 空闲中断 — 一帧数据接收完成 */
    if (__HAL_UART_GET_FLAG(&g_uart1_handle, UART_FLAG_IDLE)) 
    { 
        vUSART1ReceiveCallback();
        __HAL_UART_CLEAR_IDLEFLAG(&g_uart1_handle); 
    } 
    /* 2. 溢出错误 — 需要重新初始化 */
    else if (__HAL_UART_GET_FLAG(&g_uart1_handle, UART_FLAG_ORE)) 
    { 
        __HAL_UART_CLEAR_FLAG(&g_uart1_handle, UART_FLAG_ORE);
        __HAL_UART_CLEAR_OREFLAG(&g_uart1_handle);
        vUart1Init();  // 重新初始化恢复
    } 
    /* 3. 其他错误 — 清除标志 */
    else if ((__HAL_UART_GET_FLAG(&g_uart1_handle, UART_FLAG_NE)) || 
             (__HAL_UART_GET_FLAG(&g_uart1_handle, UART_FLAG_FE)) || 
             (__HAL_UART_GET_FLAG(&g_uart1_handle, UART_FLAG_PE))) 
    { 
        __HAL_UART_CLEAR_FLAG(&g_uart1_handle, UART_FLAG_NE); 
        __HAL_UART_CLEAR_FLAG(&g_uart1_handle, UART_FLAG_FE); 
        __HAL_UART_CLEAR_FLAG(&g_uart1_handle, UART_FLAG_PE); 
    } 
}

void DMA1_Channel5_IRQHandler(void)
{
    /* DMA 半传输中断 — 缓冲区过半 */
    if(__HAL_DMA_GET_FLAG(&g_dma_usart1_rx, DMA_FLAG_HT5) != RESET)
    {
        vUSART1ReceiveCallback();
        __HAL_DMA_CLEAR_FLAG(&g_dma_usart1_rx, DMA_FLAG_HT5);
    }
    /* DMA 传输完成中断 — 缓冲区用完一轮 */
    else if(__HAL_DMA_GET_FLAG(&g_dma_usart1_rx, DMA_FLAG_TC5) != RESET)
    {
        vUSART1ReceiveCallback();
        __HAL_DMA_CLEAR_FLAG(&g_dma_usart1_rx, DMA_FLAG_TC5);
    }
}
```

### 4.5 阻塞轮询发送

```c
void vUartSendDatas(uint32_t uiUsartPeriph, void *pvDatas, int32_t iLength)
{
    uint32_t uiTime = 0;
    uint8_t *pucDatas = pvDatas;
    UART_HandleTypeDef* huart = NULL;

    // 选择串口
    switch (uiUsartPeriph)
    {
        case (uint32_t)UART_LOG: huart = &g_uart1_handle; break;
        case (uint32_t)UART_BUS: huart = &g_uart2_handle; break;
        default: return;
    }

    // 逐字节发送
    while((iLength--) > 0)
    {
        uiTime = 1000;
        // 等待 TXE 标志（发送缓冲区空）
        while((RESET == __HAL_UART_GET_FLAG(huart, UART_FLAG_TXE)) && (--uiTime));
        
        huart->Instance->DR = *pucDatas++;  // 写入数据寄存器
    }
}
```

### 4.6 DMA 发送

```c
void vUartDMASendDatas(uint32_t uiUsartPeriph, void *pvDatas, int32_t iLength)
{
    UART_HandleTypeDef *huart = NULL;
    DMA_HandleTypeDef *dmaTxHandle = NULL;
    uint32_t uiDmaFlag = 0;
    uint32_t uiTime;
    
    // 选择串口和 DMA
    switch (uiUsartPeriph)
    {
        case (uint32_t)UART_LOG: 
            huart = &g_uart1_handle; 
            dmaTxHandle = &g_dma_usart1_tx; 
            uiDmaFlag = DMA_FLAG_TC4; 
            break;
        case (uint32_t)UART_BUS: 
            huart = &g_uart2_handle; 
            dmaTxHandle = &g_dma_usart2_tx; 
            uiDmaFlag = DMA_FLAG_TC7; 
            break;
        default: return;
    }
    
    // 启动 DMA 发送
    HAL_UART_Transmit_DMA(huart, pvDatas, iLength);
    
    // RS485 总线需要快速释放
    if (uiUsartPeriph == (uint32_t)UART_BUS)
    {
        __HAL_DMA_ENABLE_IT(dmaTxHandle, DMA_IT_TC);
    }

    // 【阶段1】等待 DMA 传输完成
    while ((__HAL_DMA_GET_FLAG(dmaTxHandle, uiDmaFlag) == RESET) &&
           (__HAL_DMA_GET_COUNTER(dmaTxHandle) != 0) &&
           ((iLength--) > 0))
    {
        vDelayMs(2);  // 以 9600 波特率计算时长
    }

    // 【阶段2】等待最后一个字节发送完成 (TC 标志)
    uiTime = 10 * 1000 / 960;  // ~10ms 超时
    while((__HAL_USART_GET_FLAG(huart, USART_FLAG_TC) == RESET) && (uiTime--))
    {
        vDelayMs(1);
    }
}
```

#### 4.6.1 关键理解：DMA 传输完成 ≠ 串口发送完成

DMA 发送需要等待 **两个阶段** 才能确保数据真正发送出去：

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         DMA 发送的两个阶段                                   │
└─────────────────────────────────────────────────────────────────────────────┘

  内存缓冲区              DMA 传输              DR 寄存器            物理串口线
  ┌────────┐            ────────►           ┌────────┐           ────────►
  │ D1 D2  │   【阶段1】                     │   Dn   │   【阶段2】          TX
  │ D3 D4  │   DMA 搬运                     │        │   串口移位发送
  │  ...   │   内存 → DR                    │        │   DR → TX线
  │ Dn     │                                │        │
  └────────┘                                └────────┘

  ════════════════════════════════════════════════════════════════════════════

  【阶段1】等待 DMA 传输完成 (DMA_FLAG_TC)
  ──────────────────────────────────────────
  - 含义：数据从「内存缓冲区」搬运到「UART DR 寄存器」完成
  - 标志：DMA_FLAG_TCx (Transfer Complete)
  - 计数器：__HAL_DMA_GET_COUNTER() == 0
  
  ⚠️ 此时数据只是到达了 DR 寄存器，还没有真正发送到物理线路上！

  【阶段2】等待串口发送完成 (USART_FLAG_TC)
  ──────────────────────────────────────────
  - 含义：数据从「DR 寄存器」通过移位寄存器发送到「物理 TX 线路」完成
  - 标志：USART_FLAG_TC (Transmission Complete)
  
  ✅ 此时数据才真正发送到了物理线路上！
```

**为什么必须等待两个阶段？**

| 场景 | 只等 DMA 完成 | 等待 DMA + 串口完成 |
| :--- | :--- | :--- |
| RS485 方向切换 | 最后几个字节会丢失 | 完整发送后再切换 |
| 连续发送多帧 | 前一帧尾部可能被覆盖 | 确保前一帧完整 |
| 低功耗模式 | 最后字节可能发送中断 | 完整发送后再休眠 |

**时序图示**：

```
时间线:    ────────────────────────────────────────────────────►
                    
DMA搬运:   ████████████████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░
                            ↑
                     DMA_FLAG_TC = 1
                     (DMA传输完成)
                     
串口发送:  ░░░░░░░░░░████████████████████████████░░░░░░░░░░░░░
                                              ↑
                                       USART_FLAG_TC = 1
                                       (串口发送完成)
                                       
RS485方向: [发送模式]─────────────────────────────[切换接收]───
                                               ↑
                                          正确切换点
                                          
          如果在 DMA_FLAG_TC 时就切换方向，
          最后几个字节还在 DR/移位寄存器中，会丢失！
```

> **经验总结**：在 RS485 半双工通信、低功耗应用等场景，必须同时等待 DMA 传输完成 **和** 串口发送完成 (TC 标志)，才能确保数据完整发送。

### 4.7 接收接口封装

```c
/* 接收指定长度数据 */
int8_t cUartReceiveDatas(uint32_t uiUsartPeriph, void *pvDatas, int32_t iLength)
{
    QueueType *ptypeQueueHandle = NULL;

    if(iLength < 1)
        return 1;

    switch(uiUsartPeriph)
    {
        case (uint32_t)UART_LOG: ptypeQueueHandle = &g_TypeQueueUart0Read; break;
        case (uint32_t)UART_BUS: ptypeQueueHandle = &g_TypeQueueUart1Read; break;
        default: return 2;
    }

    // 检查队列内数据是否足够
    if(iQueueGetLengthOfOccupy(ptypeQueueHandle) < iLength)
        return 3;

    // 从队列出队
    if(enumQueuePopDatas(ptypeQueueHandle, pvDatas, iLength) != queueNormal)
        return 4;

    return 0;
}

/* 接收所有可用数据 */
int32_t iUartReceiveAllDatas(uint32_t uiUsartPeriph, void *pvDatas, int32_t iLengthLimit)
{
    QueueType *ptypeQueueHandle = NULL;
    int32_t iLength = 0;

    if((pvDatas == NULL) || (iLengthLimit < 1))
        return 0;

    switch(uiUsartPeriph)
    {
        case (uint32_t)UART_LOG: ptypeQueueHandle = &g_TypeQueueUart0Read; break;
        case (uint32_t)UART_BUS: ptypeQueueHandle = &g_TypeQueueUart1Read; break;
        default: return 0;
    }

    // 获取有效数据长度
    if((iLength = iQueueGetLengthOfOccupy(ptypeQueueHandle)) < 1)
        return 0;

    // 限制读取长度
    iLength = iLength > iLengthLimit ? iLengthLimit : iLength;

    // 从队列出队
    if(enumQueuePopDatas(ptypeQueueHandle, pvDatas, iLength) != queueNormal)
        return 0;

    return iLength;
}
```

---

## 5. API 总览 (API Reference)

### 5.1 初始化接口

| 函数 | 功能 | 参数 | 返回值 |
| :--- | :--- | :--- | :--- |
| `vUart1Init()` | 初始化 USART1 | 无 | 无 |
| `vUart1DMAInit()` | 初始化 USART1 DMA | 无 | 无 |
| `vUart2Init()` | 初始化 USART2 | 无 | 无 |
| `vUart2DMAInit()` | 初始化 USART2 DMA | 无 | 无 |

### 5.2 控制接口

| 函数 | 功能 | 参数 | 返回值 |
| :--- | :--- | :--- | :--- |
| `vUartBaudrateSet()` | 动态设置波特率 | 串口外设, 波特率 | 无 |

### 5.3 发送接口

| 函数 | 功能 | 参数 | 返回值 |
| :--- | :--- | :--- | :--- |
| `vUartSendDatas()` | 阻塞发送数据 | 串口外设, 数据指针, 长度 | 无 |
| `vUartSendStrings()` | 阻塞发送字符串 | 串口外设, 字符串指针 | 无 |
| `vUartDMASendDatas()` | DMA 发送数据 | 串口外设, 数据指针, 长度 | 无 |
| `vUartDMASendStrings()` | DMA 发送字符串 | 串口外设, 字符串指针 | 无 |

### 5.4 接收接口

| 函数 | 功能 | 参数 | 返回值 |
| :--- | :--- | :--- | :--- |
| `cUartReceiveByte()` | 接收单字节 | 串口外设, 数据指针 | 0: 成功, 非0: 失败 |
| `cUartReceiveDatas()` | 接收指定长度数据 | 串口外设, 数据指针, 长度 | 0: 成功, 非0: 失败 |
| `iUartReceiveAllDatas()` | 接收所有可用数据 | 串口外设, 数据指针, 最大长度 | 实际读取长度 |
| `iUartReceiveLengthGet()` | 获取接收缓冲区数据长度 | 串口外设 | 数据长度 |
| `cUartReceiveClear()` | 清空接收缓冲区 | 串口外设 | 0: 成功, 非0: 失败 |

### 5.5 错误码定义

| 返回值 | 含义 |
| :--- | :--- |
| 0 | 成功 |
| 1 | 参数错误（长度无效） |
| 2 | 通道错误（无效串口） |
| 3 | 数据不足（队列内数据少于请求长度） |
| 4 | 队列操作失败 |

---

## 6. 使用示例 (Usage Examples)

### 6.1 基本初始化

```c
#include "DevicesUart.h"
#include "DevicesQueue.h"

int main(void)
{
    // 初始化队列模块
    enumQueueInit();
    
    // 初始化串口
    vUart1Init();   // LOG 串口
    vUart2Init();   // BUS 串口
    
    // ...
}
```

### 6.2 发送数据

```c
// 方式1: 阻塞发送字符串
vUartSendStrings(UART_LOG, "Hello World!\r\n");

// 方式2: 阻塞发送数据
uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
vUartSendDatas(UART_LOG, data, sizeof(data));

// 方式3: DMA 发送（适合大数据量）
uint8_t bigData[256];
vUartDMASendDatas(UART_LOG, bigData, sizeof(bigData));
```

### 6.3 接收数据

```c
uint8_t rxBuff[128];
int32_t rxLen;

// 方式1: 获取所有可用数据
rxLen = iUartReceiveAllDatas(UART_BUS, rxBuff, sizeof(rxBuff));
if (rxLen > 0)
{
    // 处理接收到的数据
    processData(rxBuff, rxLen);
}

// 方式2: 接收指定长度数据
if (cUartReceiveDatas(UART_BUS, rxBuff, 10) == 0)
{
    // 成功接收 10 字节
}

// 方式3: 检查数据量后再读取
if (iUartReceiveLengthGet(UART_BUS) >= FRAME_MIN_SIZE)
{
    // 有足够的数据，可以解析
}
```

### 6.4 动态修改波特率

```c
// 切换 BUS 串口波特率到 19200
vUartBaudrateSet((uint32_t)UART_BUS, 19200);
```

### 6.5 配合 Modbus 协议使用

```c
void vTaskModbusSlave(void *pvParameters)
{
    uint8_t rxBuff[256];
    int32_t rxLen;
    
    while(1)
    {
        // 等待空闲中断（任务通知机制可选）
        vTaskDelay(20 / portTICK_RATE_MS);
        
        // 读取数据
        rxLen = iUartReceiveAllDatas(UART_BUS, rxBuff, sizeof(rxBuff));
        
        if (rxLen > 0)
        {
            // Modbus 协议解析
            cModbusUnpack(UART_BUS, rxBuff, rxLen);
        }
    }
}
```

---

## 7. 关键设计决策 (Design Decisions)

### 7.1 为什么使用 DMA 循环模式 + 空闲中断？

| 方案 | 优点 | 缺点 |
| :--- | :--- | :--- |
| 轮询接收 | 实现简单 | CPU 占用高，易丢数据 |
| 中断接收 | 响应及时 | 高波特率时中断频繁 |
| **DMA 循环 + 空闲中断** | CPU 占用低，不丢数据 | 实现稍复杂 |
| DMA 普通模式 | 实现简单 | 需要重启 DMA，有时间窗口 |

**选择理由**：
- DMA 循环模式：硬件自动接收，无需 CPU 干预
- 空闲中断：检测一帧结束，无需固定帧长度
- 三重触发保障（空闲 + 半传输 + 全传输）：确保数据及时入队

#### 7.1.1 核心优势：彻底解决溢出错误 (ORE)

**溢出错误 (Overrun Error) 的本质**：

串口溢出中断的根本原因是：**串口接收到的数据在 DR (数据寄存器) 中没有被及时搬运到缓存里面**。

```
传统中断接收的问题：

    物理串口              DR 寄存器           软件缓冲区
    ───────────►  ┌────────────┐  ──CPU读取──►  ┌────────────┐
      字节流       │  新数据 D2  │               │ D1 │    │   │
                  └────────────┘               └────────────┘
                        │
                        ▼
              如果 CPU 来不及读取 D1，
              D2 到来时就会覆盖 D1，
              触发 ORE 溢出错误！
```

**为什么中断接收容易溢出？**
1. 每收到一个字节就产生一次中断
2. 高波特率时（如 115200），每 ~87μs 就来一个字节
3. 如果中断响应被延迟（被更高优先级中断抢占），DR 寄存器来不及清空
4. 新数据到来时覆盖旧数据 → 溢出错误

**DMA 循环接收为什么能解决？**

```
DMA 循环接收方案：

    物理串口              DR 寄存器           DMA 缓冲区 (硬件自动搬运)
    ───────────►  ┌────────────┐  ──DMA──►  ┌────┬────┬────┬────┐
      字节流       │    数据     │  (无需CPU) │ D1 │ D2 │ D3 │ D4 │...
                  └────────────┘            └────┴────┴────┴────┘
                                                      │
                                                      ▼
                                              软件只需在空闲时
                                              批量读取即可
```

1. **DMA 硬件搬运**：数据从 DR → 缓冲区完全由 DMA 硬件完成，不占用 CPU
2. **循环模式**：缓冲区用完自动回到起始位置，永不停止
3. **速度匹配**：DMA 搬运速度远快于串口接收速度，不存在来不及的问题
4. **批量处理**：软件只需在空闲中断时批量入队，不需要每字节响应

> **实践验证**：采用 DMA 循环接收 + 环形队列的方案后，基本上到现在还没遇到过溢出问题，即使在高波特率、长时间运行的场景下也非常稳定。

### 7.2 为什么接收数据放入队列？

| 方案 | 优点 | 缺点 |
| :--- | :--- | :--- |
| 直接回调处理 | 实时性好 | 回调内不能做耗时操作 |
| 全局缓冲区 + 标志 | 简单 | 多帧粘包问题，并发不安全 |
| **环形队列** | 解耦收发，缓冲灵活 | 需要额外内存 |

**选择理由**：
- **生产者-消费者解耦**：中断写入，主循环读取
- **防止粘包**：队列顺序存储，不会覆盖
- **线程安全**：单生产者-单消费者无需加锁

### 7.3 阻塞发送 vs DMA 发送 的选择

| 场景 | 推荐方式 | 原因 |
| :--- | :--- | :--- |
| LOG 日志输出 | 阻塞发送 | 简单可靠，日志通常不长 |
| 大数据量传输 | DMA 发送 | CPU 占用低 |
| RS485 总线通信 | DMA 发送 | 需要精确控制发送完成时机 |
| 调试 printf | 阻塞发送 | 便于单步调试 |

---

## 8. 常见问题与调试 (Troubleshooting)

### Q1: 接收丢数据

**可能原因**：
1. 队列缓冲区太小，被覆盖
2. 主循环处理太慢
3. 中断优先级配置不当

**排查方法**：
```c
// 在接收回调中统计入队失败次数
if(enumQueuePushDatas(...) == queueFull)
{
    g_queueOverflowCount++;  // 溢出计数
}
```

### Q2: 发送不完整

**可能原因**：
1. 没等待 TC 标志
2. DMA 发送时切换了缓冲区

**解决方案**：
```c
// DMA 发送后等待 TC 标志
while(__HAL_USART_GET_FLAG(huart, USART_FLAG_TC) == RESET);
```

### Q3: 通信一段时间后停止

**可能原因**：
1. 溢出错误 (ORE) 未处理
2. DMA 错误未清除

**解决方案**：
- 在中断中检测 ORE 错误并重新初始化
- 定期检查 UART 状态

### Q3.1: 深入理解溢出错误 (ORE) — 为什么会发生？

**溢出错误的本质**：

串口溢出中断 (Overrun Error) 就是 **串口接收到的数据在 DR 寄存器中没有被及时搬运到缓存里面**。

```
时序分析（以 115200 波特率为例）：

时间线:     0μs      87μs     174μs    261μs
           ─────┬────────┬────────┬────────►
串口接收:      D1       D2       D3       D4
              ↓        ↓
DR寄存器:   [D1]     [D2]     
              │        │
              │   如果此时 CPU 还没读走 D1
              │   D2 就会覆盖 D1 → ORE 错误！
              ↓
         正常情况：CPU 读走 D1，DR 准备接收 D2
```

**容易触发 ORE 的场景**：
1. 中断响应延迟（被更高优先级中断抢占）
2. 中断服务函数执行时间过长
3. 高波特率 + 连续大量数据
4. 关中断时间过长

**本驱动的解决方案**：

采用 **DMA 循环接收 + 环形队列** 方案后，基本上不会再遇到溢出问题：

| 对比项 | 中断接收 | DMA 循环接收 |
| :--- | :--- | :--- |
| DR→缓冲区搬运 | CPU 软件完成 | DMA 硬件完成 |
| 每字节是否需要中断 | 是 | 否 |
| 高波特率表现 | 容易溢出 | 稳定可靠 |
| CPU 占用 | 高 | 极低 |

> **经验总结**：如果项目中频繁遇到串口溢出问题，强烈建议切换到 DMA 循环接收方案，这是从根本上解决问题的方法。

### Q4: RS485 半双工冲突

**可能原因**：
1. 发送完成后没有及时切换方向
2. 等待时间不够

**解决方案**：
```c
// 发送完成中断中切换方向
void DMA1_Channel7_IRQHandler(void)
{
    if(__HAL_DMA_GET_FLAG(&g_dma_usart2_tx, DMA_FLAG_TC7))
    {
        // 等待最后一个字节发送完成
        while(__HAL_USART_GET_FLAG(&g_uart2_handle, USART_FLAG_TC) == RESET);
        
        // 切换为接收模式
        RS485_RX_ENABLE();
        
        __HAL_DMA_CLEAR_FLAG(&g_dma_usart2_tx, DMA_FLAG_TC7);
    }
}
```

---

## 9. 代码质量自检 (Self-Check)

- [x] **空指针检查**：接收函数检查输入参数有效性
- [x] **边界处理**：DMA 缓冲区回绕正确处理
- [x] **错误恢复**：溢出错误自动重新初始化
- [x] **模块解耦**：接收数据通过队列传递
- [x] **可配置性**：波特率、缓冲区大小可配置
- [x] **命名规范**：函数以 `v`/`c`/`i` 前缀区分返回值类型

---

## 10. 版本变更记录 (Changelog)

| 版本 | 日期 | 变更内容 |
| :--- | :--- | :--- |
| v1.0 | 2026-02-04 | 初始版本，支持 USART1/2 DMA 收 |

---

**文档结束**
