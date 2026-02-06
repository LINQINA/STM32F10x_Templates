# DriverLogPrintf 设计文档

| 属性 | 内容 |
| :--- | :--- |
| **模块名称** | DriverLogPrintf（日志打印模块） |
| **源文件** | `Modules/Log/DriverLogPrintf.c` / `DriverLogPrintf.h` |
| **硬件依赖** | DevicesUart（UART_LOG = USART1） |
| **版本** | v1.0 |
| **最后更新** | 2026-02-06 |

---

## 1. 设计目标

提供一个 **类 printf** 的日志打印系统，用于嵌入式 FreeRTOS 环境：

1. **printf 风格 API**：用法与 `printf` 一致，降低使用门槛。
2. **缓冲 + 一次性发送**：先写入内部缓冲区，再一次性通过 UART DMA 发出，减少频繁小包占用总线。
3. **日志等级过滤**：支持 Normal / Error / System 三种等级，可运行时开关。
4. **输出通道可选**：支持 RS485 Log / RS485 Bus / Uart Wifi / CAN / USB CDC 等多通道（本工程使用 RS485 Log → UART_LOG）。
5. **附加信息可选**：可选附带时间戳、文件名、函数名、行号，便于定位问题。

---

## 2. 模块架构

### 2.1 整体流程

```
用户调用宏（如 cLogPrintfError）
    │
    ▼
_cLogPrintf_ 宏
    │
    ├─ ① 开关过滤：类型(0xFF00) AND 通道(0x00FF) 是否都在总开关中打开？
    │      不满足 → break，不打印
    │      满足   ↓
    ├─ ② vLogPrintf(":")                       // 前缀
    ├─ ③ [可选] vLogPrintf("[Time:xxx] ")      // 时间戳
    ├─ ④ [可选] vLogPrintf("[File:xxx] ")      // 文件名
    ├─ ⑤ [可选] vLogPrintf("[Func:xxx] ")      // 函数名
    ├─ ⑥ [可选] vLogPrintf("[Line:xxx] ")      // 行号
    ├─ ⑦ vLogPrintf(format, ...)               // 用户内容
    ├─ ⑧ vLogPrintf("\r")                      // 尾部回车
    └─ ⑨ cLogPrintfStop(enumSwitchs)           // 通过 UART DMA 一次性发出，清空缓冲区
```

### 2.2 核心组件关系

```
┌──────────────────────────────────────────────────────────────────────┐
│                         用户调用层（宏）                               │
│  cLogPrintfNormal / cLogPrintfError / cLogPrintfSystem ...           │
│  cLogPrintfNormalTime / cLogPrintfErrorAll / cLogPrintfIotNormal ... │
└─────────────────────────────┬────────────────────────────────────────┘
                              │ 展开为
                              ▼
┌──────────────────────────────────────────────────────────────────────┐
│                     _cLogPrintf_ 宏（核心）                           │
│  开关过滤 → vLogPrintf 追加到缓冲区 → cLogPrintfStop 发送并清空       │
└─────────────────────────────┬────────────────────────────────────────┘
                              │ 调用
                              ▼
┌──────────────────────────────────────────────────────────────────────┐
│                     底层函数（.c 文件）                                │
│  vLogPrintf()        → vsnprintf 追加到 st_cLogPrintfBuff             │
│  cLogPrintfStop()    → vUartDMASendDatas(UART_LOG, ...) 发送          │
│  cLogPrintfSwitchsSet/Get() → 读写总开关 st_enumLogSwitchs            │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 3. 枚举与位域设计

### 3.1 LogPrintfSwitchEnum 位域分布

```
31         24 23         16 15          8 7           0
┌─────────────┬─────────────┬─────────────┬─────────────┐
│  附加信息位   │   保留      │   日志类型   │   输出通道   │
│ (0xFF000000) │ (0x00FF0000)│  (0x0000FF00)│ (0x000000FF)│
└─────────────┴─────────────┴─────────────┴─────────────┘
```

### 3.2 附加信息位（bit 24~31）

| 枚举值 | 值 | 说明 |
| :----- | :- | :--- |
| LogPrintfDate | 0x01000000 | 附带日期 |
| LogPrintfTime | 0x02000000 | 附带时间戳 |
| LogPrintfFile | 0x04000000 | 附带文件名（`__FILE__`） |
| LogPrintfLine | 0x08000000 | 附带行号（`__LINE__`） |
| LogPrintfFunc | 0x10000000 | 附带函数名（`__func__`） |

> 附加信息位不参与开关过滤，只控制宏内是否拼接对应字段。

### 3.3 日志类型（bit 8~15）

| 枚举值 | 值 | 说明 |
| :----- | :- | :--- |
| LogPrintfNormal | 0x00000100 | 普通日志 |
| LogPrintfError  | 0x00000200 | 错误日志 |
| LogPrintfSystem | 0x00000400 | 系统日志 |

### 3.4 输出通道（bit 0~7）

| 枚举值 | 值 | 说明 |
| :----- | :- | :--- |
| LogPrintfRS485Log | 0x00000001 | 通过 RS485 Log（UART_LOG / USART1）输出 |
| LogPrintfRS485Bus | 0x00000002 | 通过 RS485 Bus（UART_BUS / USART2）输出 |
| LogPrintfUartWifi | 0x00000004 | 通过 Uart Wifi 通道输出 |
| LogPrintfCan      | 0x00000008 | 通过 CAN 通道输出 |
| LogPrintfUsbCdc1  | 0x00000010 | 通过 USB CDC 1 通道输出 |
| LogPrintfUsbCdc2  | 0x00000020 | 通过 USB CDC 2 通道输出 |

---

## 4. 开关过滤机制

### 4.1 总开关

```c
/* 默认值：Normal + Error + System 全开，RS485Log + UartWifi 通道打开 */
LogPrintfSwitchEnum st_enumLogSwitchs = (LogPrintfNormal | LogPrintfError | LogPrintfSystem)
                                      | LogPrintfUartWifi | LogPrintfRS485Log;
```

运行时可通过 `cLogPrintfSwitchsSet()` 修改。

### 4.2 过滤逻辑

宏 `_cLogPrintf_` 中做两步与运算：

```c
if((enumSwitchs & 0xFF00 & enumLogPrintfSwitchsGet()) == 0   // 类型未打开？
|| (enumSwitchs & 0x00FF & enumLogPrintfSwitchsGet()) == 0)  // 通道未打开？
{
    break;  // 不打印
}
```

- **类型（0xFF00）** 和 **通道（0x00FF）** 必须**同时**在总开关中被打开，才会执行后续打印。
- 任何一个为 0 则 `break`，整段 vLogPrintf + cLogPrintfStop 都**不会执行**。

### 4.3 过滤示例

假设总开关只打开 Normal + RS485Log：

```c
st_enumLogSwitchs = LogPrintfNormal | LogPrintfRS485Log;  // 0x0101
```

| 调用 | enumSwitchs | 类型与 (& 0xFF00 & 0x0101) | 通道与 (& 0x00FF & 0x0101) | 结果 |
| :--- | :---------- | :------------------------- | :------------------------- | :--- |
| cLogPrintfNormal("x") | 0x0101 | 0x0100 & 0x0101 = **0x0100** | 0x0001 & 0x0101 = **0x0001** | 打印 |
| cLogPrintfError("x")  | 0x0201 | 0x0200 & 0x0101 = **0x0000** | — | 不打印 |
| cLogPrintfSystem("x") | 0x0401 | 0x0400 & 0x0101 = **0x0000** | — | 不打印 |

---

## 5. API 说明

### 5.1 底层函数

| 函数 | 说明 |
| :--- | :--- |
| `void vLogPrintf(const char *format, ...)` | 仿 printf，将格式化内容**追加**到 `st_cLogPrintfBuff`，不直接发送。使用 `vsnprintf` 保证不越界。 |
| `int8_t cLogPrintfStop(LogPrintfSwitchEnum enumSwitchs)` | 按 enumSwitchs 中的通道位，将缓冲区内容通过对应 UART/CAN 等一次性发出，然后**清空缓冲区**（st_cLogPrintfLength = 0）。 |
| `int8_t cLogPrintfSwitchsSet(LogPrintfSwitchEnum enumSwitchs)` | 设置总开关（只取低 16 位：类型 + 通道）。 |
| `LogPrintfSwitchEnum enumLogPrintfSwitchsGet(void)` | 获取当前总开关值。 |

### 5.2 用户宏（推荐使用）

#### 基础（仅内容）

| 宏 | 等级 | 通道 |
| :- | :--- | :--- |
| `cLogPrintfNormal(format, ...)` | Normal | RS485Log |
| `cLogPrintfError(format, ...)`  | Error  | RS485Log |
| `cLogPrintfSystem(format, ...)` | System | RS485Log |

#### 带时间戳

| 宏 | 附加 |
| :- | :--- |
| `cLogPrintfNormalTime(format, ...)` | + 时间戳 |
| `cLogPrintfErrorTime(format, ...)`  | + 时间戳 |
| `cLogPrintfSystemTime(format, ...)` | + 时间戳 |

#### 带时间戳 + 文件 + 行号

| 宏 | 附加 |
| :- | :--- |
| `cLogPrintfNormalTimeFile(format, ...)` | + 时间戳 + 文件名 + 行号 |
| `cLogPrintfErrorTimeFile(format, ...)`  | + 时间戳 + 文件名 + 行号 |
| `cLogPrintfSystemTimeFile(format, ...)` | + 时间戳 + 文件名 + 行号 |

#### 全信息

| 宏 | 附加 |
| :- | :--- |
| `cLogPrintfNormalAll(format, ...)` | + 时间戳 + 函数名 + 文件名 + 行号 |
| `cLogPrintfErrorAll(format, ...)`  | + 时间戳 + 函数名 + 文件名 + 行号 |
| `cLogPrintfSystemAll(format, ...)` | + 时间戳 + 函数名 + 文件名 + 行号 |

#### IoT 通道

| 宏 | 等级 | 通道 |
| :- | :--- | :--- |
| `cLogPrintfIotNormal(format, ...)` | Normal | UartWifi |

---

## 6. 使用示例

### 6.1 基本用法

```c
/* 系统启动 */
cLogPrintfSystem("APP start.\r\n");
cLogPrintfSystem("APP software version: %s\r\n", pcVersionAPPSoftGet());

/* 普通信息 */
cLogPrintfNormal("温度=%d, 电压=%.1fV\r\n", temp, voltage);

/* 错误提示 */
cLogPrintfError("enumQueueInit error.\r\n");
```

### 6.2 带时间戳和文件行号

```c
/* 排查问题时用，输出示例：:[Time:123456] [File:main.c] [Line:42] 发生异常 */
cLogPrintfErrorTimeFile("发生异常\r\n");
```

### 6.3 运行时关闭 Normal，只留 Error

```c
/* 只保留 Error 类型 + RS485Log 通道 */
cLogPrintfSwitchsSet(LogPrintfError | LogPrintfRS485Log);

cLogPrintfNormal("这条不会打印\r\n");   // Normal 被关，不输出
cLogPrintfError("这条会打印\r\n");      // Error 打开，正常输出
```

---

## 7. 输出格式

每次调用宏输出的一条完整日志格式为：

```
:[可选附加信息] 用户内容\r
```

示例：

| 宏 | 输出 |
| :- | :--- |
| `cLogPrintfNormal("hello")` | `:hello\r` |
| `cLogPrintfErrorTime("err=%d", 3)` | `:[Time:123456] err=3\r` |
| `cLogPrintfSystemAll("boot")` | `:[Time:123456] [Func:main] [File:main.c] [Line:39] boot\r` |

---

## 8. 内部实现要点

### 8.1 缓冲区

```c
char st_cLogPrintfBuff[256];           // 单条日志缓冲区，256 字节
volatile int32_t st_cLogPrintfLength;  // 当前已写入长度
```

- `vLogPrintf` 每次调用都通过 `vsnprintf` 往 `st_cLogPrintfBuff[st_cLogPrintfLength]` 处追加。
- `cLogPrintfStop` 发送后将 `st_cLogPrintfLength` 清零。
- 通过宏调用时，**每条日志独立拼接 + 独立发送**，不会跨条累积。

### 8.2 发送路径

`cLogPrintfStop` 根据通道位决定通过哪路发送：

```c
int8_t cLogPrintfStop(LogPrintfSwitchEnum enumSwitchs)
{
    if(enumSwitchs & LogPrintfRS485Log)
    {
        vUartDMASendDatas((uint32_t)UART_LOG, st_cLogPrintfBuff, st_cLogPrintfLength);
    }
    st_cLogPrintfLength = 0;
    return 0;
}
```

> 当前工程只实现了 RS485Log 通道的发送，其他通道（Bus / Wifi / CAN / USB）在此函数中按需扩展。

---

## 9. 注意事项

| 事项 | 说明 |
| :--- | :--- |
| **缓冲区大小** | 单条日志最多 256 字节（含前缀、附加信息、用户内容、`\r`）。超出部分被 `vsnprintf` 截断。 |
| **非线程安全** | `st_cLogPrintfBuff` 和 `st_cLogPrintfLength` 为全局变量，多任务同时调用可能交叉写入。当前设计依赖"每条日志在一个宏调用内完成拼接+发送"来规避，但严格场景需加互斥。 |
| **与 UART 接收的耦合** | `DevicesUart.c` 中部分接收函数（如 `cUartReceiveClear`）内部会调用 `cLogPrintfStop` 来先把未发的日志刷出去，避免日志和接收数据在同一 UART 上混淆。 |
| **vLogPrintf 长度累加** | `st_cLogPrintfLength += vsnprintf(...)` 中 `vsnprintf` 返回的是"本应写入的长度"（可能大于实际可写空间）。若缓冲区满，后续追加有效写入为 0 但长度记录可能偏大。当前通过宏"每条独立发送"规避此问题。 |
