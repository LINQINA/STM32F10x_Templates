# STM32F10x Queue 设计文档 (DevicesQueue)

| 属性 | 内容 |
| :--- | :--- |
| **模块名称** | DevicesQueue (环形队列模块) |
| **源文件** | `DevicesQueue.c` / `DevicesQueue.h` |
| **硬件依赖** | 无 |
| **版本** | v1.0 |
| **最后更新** | 2026-02-04 |

---

## 1. 设计目标 (Design Goals)

实现嵌入式系统中常用的环形队列（循环缓冲区）结构：

1. **多种队列模式**：覆盖模式 (queueModeNormal) / 锁定模式 (queueModeLock)
2. **完整的队列操作**：创建、清空、入队、出队、查看、查找等
3. **高可复用性**：可快速为不同外设创建独立队列（UART、CAN、IIC 等）
4. **生产者-消费者解耦**：支持中断写入、主循环读取的异步场景

---

## 2. 模块架构 (Module Architecture)

### 2.1 核心原理 — 环形队列

环形队列是一个首尾相连的逻辑结构，通过两个指针（读指针、写指针）分别管理数据的读取和写入：

```
                    ┌─────────────────────────────────────┐
                    │           环形队列结构图              │
                    └─────────────────────────────────────┘

    物理存储（线性数组）:
    ┌────┬────┬────┬────┬────┬────┬────┬────┐
    │ D3 │ D4 │ D5 │    │    │    │ D1 │ D2 │
    └────┴────┴────┴────┴────┴────┴────┴────┘
      ▲                          ▲
      │                          │
    pWriteTo                  pReadFrom
    (写指针)                   (读指针)

    逻辑视图（环形）:
              pHead
                ▼
            ┌───────┐
         ┌──┤  D1   │◄── pReadFrom (读)
         │  ├───────┤
         │  │  D2   │
         │  ├───────┤
         │  │  D3   │
         │  ├───────┤
         │  │  D4   │
         │  ├───────┤
         │  │  D5   │◄── pWriteTo (写)
         │  ├───────┤
         │  │ Empty │
         │  ├───────┤
         └──► Empty │
            └───────┘
                ▲
              pTail
```

### 2.2 队列状态枚举

```c
typedef enum {
    queueNormal = 0,   /* 正常状态 */
    queueError,        /* 错误状态 */
    queueNull,         /* 空指针 */
    queueEmpty,        /* 队列为空 */
    queueFull,         /* 队列已满 */
} enumQueueState;
```

### 2.3 队列模式枚举

```c
typedef enum {
    queueModeNormal = 0,   /* 覆盖模式：满时覆盖旧数据 */
    queueModeLock,         /* 锁定模式：满时拒绝写入 */
} enumQueueMode;
```

| 模式 | 满时行为 | 适用场景 |
| :--- | :--- | :--- |
| `queueModeNormal` | 覆盖最旧的数据 | 传感器数据（只关心最新值） |
| `queueModeLock` | 拒绝入队，返回 `queueFull` | 通信数据（不能丢帧） |

### 2.4 队列结构体

```c
typedef struct {
    char    *pcName;       /* 队列名称（调试用） */
    uint8_t *pHead;        /* 缓冲区起始地址 */
    uint8_t *pTail;        /* 缓冲区结束地址 */
    uint8_t *pReadFrom;    /* 读指针 */
    uint8_t *pWriteTo;     /* 写指针 */
    int32_t  length;       /* 缓冲区长度 */
    enumQueueMode mode;    /* 队列模式 */
} QueueType;
```

---

## 3. 软件实现细节 (Implementation Details)

### 3.1 创建队列 (enumQueueCreate)

```c
enumQueueState enumQueueCreate(QueueType *pTypeQueue, char *pcName, 
                               uint8_t *pucBuff, int32_t iLength, 
                               enumQueueMode enumMode)
{
    if(pTypeQueue == NULL)
        return queueNull;

    if(iLength < 1)
        return queueEmpty;

    pTypeQueue->mode      = enumMode;
    pTypeQueue->pcName    = pcName;
    pTypeQueue->length    = iLength + 1;      // 实际多分配 1 字节
    pTypeQueue->pReadFrom = pucBuff;
    pTypeQueue->pWriteTo  = pucBuff;
    pTypeQueue->pTail     = pucBuff + pTypeQueue->length;
    pTypeQueue->pHead     = pucBuff;

    return queueNormal;
}
```

**关键点**：`length = iLength + 1`，多分配一个字节用于区分"空"和"满"的状态。

### 3.2 判断空/满状态 (enumQueueGetState)

```c
enumQueueState enumQueueGetState(QueueType *pTypeQueue)
{
    uint8_t *pNow = NULL;

    if((pTypeQueue == NULL) || (pTypeQueue->pHead == NULL))
        return queueNull;

    // 读写指针相等 → 队列为空
    if(pTypeQueue->pReadFrom == pTypeQueue->pWriteTo)
        return queueEmpty;

    // 写指针下一个位置等于读指针 → 队列已满
    pNow = pTypeQueue->pWriteTo + 1;
    pNow = (pNow >= pTypeQueue->pTail) ? pTypeQueue->pHead : pNow;

    if(pNow == pTypeQueue->pReadFrom)
        return queueFull;

    return queueNormal;
}
```

**空满判断图示**：

```
空状态:  pReadFrom == pWriteTo
┌────┬────┬────┬────┬────┐
│    │    │    │    │    │
└────┴────┴────┴────┴────┘
            ▲
        pReadFrom
        pWriteTo

满状态:  (pWriteTo + 1) == pReadFrom
┌────┬────┬────┬────┬────┐
│ D4 │ D5 │    │ D1 │ D2 │ D3 │
└────┴────┴────┴────┴────┴────┘
            ▲    ▲
      pWriteTo  pReadFrom
```

### 3.3 获取有效数据长度 (iQueueGetLengthOfOccupy)

```c
int32_t iQueueGetLengthOfOccupy(QueueType *pTypeQueue)
{
    int32_t iLength = 0;

    if((pTypeQueue == NULL) || (pTypeQueue->pHead == NULL))
        return 0;

    if(pTypeQueue->pReadFrom <= pTypeQueue->pWriteTo)
        iLength = pTypeQueue->pWriteTo - pTypeQueue->pReadFrom;
    else
        iLength = pTypeQueue->length - (pTypeQueue->pReadFrom - pTypeQueue->pWriteTo);

    return iLength;
}
```

**两种情况**：

```
情况 1: pReadFrom <= pWriteTo (连续区域)
┌────┬────┬────┬────┬────┬────┐
│    │ D1 │ D2 │ D3 │    │    │
└────┴────┴────┴────┴────┴────┘
      ▲              ▲
  pReadFrom      pWriteTo
  
  有效长度 = pWriteTo - pReadFrom

情况 2: pReadFrom > pWriteTo (跨越边界)
┌────┬────┬────┬────┬────┬────┐
│ D4 │ D5 │    │    │ D1 │ D2 │ D3 │
└────┴────┴────┴────┴────┴────┴────┘
            ▲         ▲
        pWriteTo  pReadFrom

  有效长度 = length - (pReadFrom - pWriteTo)
```

### 3.4 获取剩余空间 (iQueueGetLengthOfRemaining)

```c
int32_t iQueueGetLengthOfRemaining(QueueType *pTypeQueue)
{
    int32_t iLength = 0;

    if((pTypeQueue == NULL) || (pTypeQueue->pHead == NULL))
        return 0;

    if(pTypeQueue->pReadFrom <= pTypeQueue->pWriteTo)
        iLength = pTypeQueue->length - (pTypeQueue->pWriteTo - pTypeQueue->pReadFrom) - 1;
    else
        iLength = (pTypeQueue->pReadFrom - pTypeQueue->pWriteTo) - 1;

    return iLength;
}
```

### 3.5 单字节入队 (enumQueuePushByte)

```c
enumQueueState enumQueuePushByte(QueueType *pTypeQueue, uint8_t ucData)
{
    enumQueueState enumPushState = queueNormal;

    if((pTypeQueue == NULL) || (pTypeQueue->pHead == NULL))
        return queueNull;

    // 队列满时的处理
    if(enumQueueGetState(pTypeQueue) == queueFull)
    {
        if(pTypeQueue->mode == queueModeLock)
            return queueFull;       // 锁定模式：拒绝写入
        enumPushState = queueFull;  // 覆盖模式：继续写入，返回 Full 警告
    }

    // 写入数据，移动写指针
    *pTypeQueue->pWriteTo++ = ucData;
    pTypeQueue->pWriteTo = ((pTypeQueue->pWriteTo >= pTypeQueue->pTail) 
                            ? pTypeQueue->pHead : pTypeQueue->pWriteTo);

    // 覆盖模式下，同步移动读指针（丢弃最旧数据）
    if(enumPushState == queueFull)
    {
        pTypeQueue->pReadFrom = pTypeQueue->pWriteTo + 1;
        pTypeQueue->pReadFrom = ((pTypeQueue->pReadFrom >= pTypeQueue->pTail) 
                                 ? pTypeQueue->pHead : pTypeQueue->pReadFrom);
    }

    return enumPushState;
}
```

### 3.6 单字节出队 (enumQueuePopByte)

```c
enumQueueState enumQueuePopByte(QueueType *pTypeQueue, uint8_t *pucData)
{
    if((pTypeQueue == NULL) || (pTypeQueue->pHead == NULL))
        return queueNull;

    if(pTypeQueue->pReadFrom == pTypeQueue->pWriteTo)
        return queueEmpty;

    *pucData = *pTypeQueue->pReadFrom++;
    pTypeQueue->pReadFrom = ((pTypeQueue->pReadFrom >= pTypeQueue->pTail) 
                             ? pTypeQueue->pHead : pTypeQueue->pReadFrom);

    return queueNormal;
}
```

### 3.7 查看数据（不出队）(enumQueueViewByte / enumQueueViewDatas)

与 Pop 系列函数的区别：**只读取数据，不移动读指针**。

```c
// 查看单字节
enumQueueState enumQueueViewByte(QueueType *pTypeQueue, uint8_t *pucData)
{
    // ...
    *pucData = *pTypeQueue->pReadFrom;  // 不移动指针
    return queueNormal;
}
```

### 3.8 批量入队/出队 (enumQueuePushDatas / enumQueuePopDatas)

批量操作是单字节操作的循环封装，效率更高。

### 3.9 带分隔符的查找 (iQueueGetLengthOfSeparetor)

查找队列中从读指针到第一个分隔符的长度，用于协议解析：

```c
int32_t iQueueGetLengthOfSeparetor(QueueType *pTypeQueue, uint8_t ucByte)
```

**应用场景**：查找一帧数据（以 `\n` 或特定字节结尾）。

### 3.10 API 总览

| 函数名 | 功能 | 返回值 |
| :--- | :--- | :--- |
| `enumQueueInit` | 初始化所有预定义队列 | 状态 |
| `enumQueueCreate` | 创建队列实例 | 状态 |
| `enumQueueGetState` | 获取队列空/满状态 | 状态 |
| `enumQueueSetState` | 设置队列状态（清空等） | 状态 |
| `iQueueGetLengthOfOccupy` | 获取有效数据长度 | 长度 |
| `iQueueGetLengthOfRemaining` | 获取剩余空间长度 | 长度 |
| `iQueueGetLengthOfSeparetor` | 获取到分隔符的长度 | 长度 |
| `iQueueGetLengthOfOccupyNeed` | 获取到指定字节的长度（从尾部查找） | 长度 |
| `enumQueuePushByte` | 入队单字节 | 状态 |
| `enumQueuePopByte` | 出队单字节 | 状态 |
| `enumQueueViewByte` | 查看单字节（不出队） | 状态 |
| `enumQueuePushDatas` | 批量入队 | 状态 |
| `enumQueuePopDatas` | 批量出队 | 状态 |
| `enumQueueViewDatas` | 批量查看（不出队） | 状态 |
| `enumQueuePopDatasNeed` | 带条件批量出队 | 状态 |
| `enumQueueViewDatasNeed` | 带条件批量查看 | 状态 |

---

## 4. 预定义队列实例 (Predefined Queues)

```c
/* UART0 - LOG 串口 */
QueueType g_TypeQueueUart0Read;
static uint8_t st_ucQueueUart0ReadBuff[QUEUE_UART0_READ_LENGTH + 4];

/* UART1 - 总线串口 */
QueueType g_TypeQueueUart1Read;
static uint8_t st_ucQueueUart1ReadBuff[QUEUE_UART1_READ_LENGTH + 4];

/* CAN - 主机通信 */
QueueType g_TypeQueueCanHostRead;
static uint8_t st_ucQueueCanHostReadBuff[QUEUE_CAN_HOST_READ_LENGTH + 4];
```

| 队列 | 大小 | 用途 |
| :--- | :--- | :--- |
| `g_TypeQueueUart0Read` | 512 字节 | LOG 串口接收缓冲 |
| `g_TypeQueueUart1Read` | 512 字节 | 总线串口接收缓冲 |
| `g_TypeQueueCanHostRead` | 512 字节 | CAN 接收缓冲 |

---

## 5. 使用场景 (Use Cases)

### 5.1 场景一：串口 DMA 接收入队

**文件**：`stm32f1xx_it.c`

串口采用 DMA + 空闲中断方式接收，在中断回调中将 DMA 缓冲区的数据入队：

```c
void vUSART1ReceiveCallback(void)
{
    static uint32_t uiMDANdtrOld = 0;
    uint32_t uiMDANdtrNow = 0;

    while(uiMDANdtrOld != (uiMDANdtrNow = USART1_DMA_READ_LENGTH - __HAL_DMA_GET_COUNTER(&g_dma_usart1_rx)))
    {
        /* DMA 缓冲区回绕的情况：先把尾部数据入队 */
        if(uiMDANdtrNow < uiMDANdtrOld)
        {
            enumQueuePushDatas(&g_TypeQueueUart0Read, &g_USART1ReadDMABuff[uiMDANdtrOld], 
                               USART1_DMA_READ_LENGTH - uiMDANdtrOld);
            uiMDANdtrOld = 0;
        }

        /* 把数据读取到 UART 队列 */
        enumQueuePushDatas(&g_TypeQueueUart0Read, &g_USART1ReadDMABuff[uiMDANdtrOld], 
                           uiMDANdtrNow - uiMDANdtrOld);
        uiMDANdtrOld = uiMDANdtrNow;
    }
}

/* USART1 空闲中断 */
void USART1_IRQHandler(void) 
{ 
    if (__HAL_UART_GET_FLAG(&g_uart1_handle, UART_FLAG_IDLE)) 
    { 
        vUSART1ReceiveCallback(); 
        __HAL_UART_CLEAR_IDLEFLAG(&g_uart1_handle); 
    } 
    // ... 错误处理
}

/* DMA 半传输/传输完成中断 */
void DMA1_Channel5_IRQHandler(void)
{
    if(__HAL_DMA_GET_FLAG(&g_dma_usart1_rx, DMA_FLAG_HT5) != RESET)
    {
        vUSART1ReceiveCallback();
        __HAL_DMA_CLEAR_FLAG(&g_dma_usart1_rx, DMA_FLAG_HT5);
    }
    else if(__HAL_DMA_GET_FLAG(&g_dma_usart1_rx, DMA_FLAG_TC5) != RESET)
    {
        vUSART1ReceiveCallback();
        __HAL_DMA_CLEAR_FLAG(&g_dma_usart1_rx, DMA_FLAG_TC5);
    }
}
```

**设计要点**：
- 使用静态变量 `uiMDANdtrOld` 记录上次 DMA 指针位置
- 处理 DMA 缓冲区回绕的情况（分两次入队）
- 在空闲中断、DMA 半传输、DMA 传输完成三个时机都调用入队

---

### 5.2 场景二：CAN 接收入队

**文件**：`stm32f1xx_it.c`

CAN 接收中断中将帧数据入队：

```c
void CAN1_RX0_IRQHandler(void)
{
    CAN_RxHeaderTypeDef can1_rxheader;
    uint8_t rxData[8];
    
    /* 循环读取 FIFO 中的所有消息 */
    while (HAL_CAN_GetRxFifoFillLevel(&g_can1_handler, CAN_RX_FIFO0) > 0)
    {
        if (HAL_CAN_GetRxMessage(&g_can1_handler, CAN_RX_FIFO0, &can1_rxheader, rxData) == HAL_OK)
        {
            /* 标准帧入队 */
            if (can1_rxheader.IDE == CAN_ID_STD)
            {
                enumQueuePushDatas(&g_TypeQueueCanHostRead, rxData, can1_rxheader.DLC);
            }
        }
    }
}
```

**设计要点**：
- 使用 `while` 循环清空 FIFO，避免消息积压
- 只入队标准帧，可根据需要扩展过滤条件
- `DLC` 为实际数据长度（0~8 字节）

---

### 5.3 场景三：串口驱动封装出队接口

**文件**：`DevicesUart.c`

将队列操作封装为串口驱动 API，对上层屏蔽队列细节：

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
        default : return 2;
    }

    /* 判断队列内是否有足够的数据 */
    if(iQueueGetLengthOfOccupy(ptypeQueueHandle) < iLength)
        return 3;

    /* 从队列内获取数据 */
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
        default : return 0;
    }

    /* 读取队列内有效数据的长度 */
    if((iLength = iQueueGetLengthOfOccupy(ptypeQueueHandle)) < 1)
        return 0;

    /* 限制读取长度 */
    iLength = iLength > iLengthLimit ? iLengthLimit : iLength;

    /* 从队列内获取数据 */
    if(enumQueuePopDatas(ptypeQueueHandle, pvDatas, iLength) != queueNormal)
        return 0;

    return iLength;
}

/* 获取接收缓冲区数据长度 */
int32_t iUartReceiveLengthGet(uint32_t uiUsartPeriph)
{
    QueueType *ptypeQueueHandle = NULL;

    switch(uiUsartPeriph)
    {
        case (uint32_t)UART_LOG: ptypeQueueHandle = &g_TypeQueueUart0Read; break;
        case (uint32_t)UART_BUS: ptypeQueueHandle = &g_TypeQueueUart1Read; break;
        default : return 0;
    }

    return iQueueGetLengthOfOccupy(ptypeQueueHandle);
}

/* 清空接收缓冲区 */
int8_t cUartReceiveClear(uint32_t uiUsartPeriph)
{
    QueueType *ptypeQueueHandle = NULL;

    switch(uiUsartPeriph)
    {
        case (uint32_t)UART_LOG: ptypeQueueHandle = &g_TypeQueueUart0Read; break;
        case (uint32_t)UART_BUS: ptypeQueueHandle = &g_TypeQueueUart1Read; break;
        default : return 1;
    }

    return enumQueueSetState(ptypeQueueHandle, queueEmpty);
}
```

**设计要点**：
- 通过 `uiUsartPeriph` 参数选择对应的队列
- 提供多种出队方式：定长、全部、查询长度、清空
- 返回值区分不同错误类型

---

### 5.4 场景四：FreeRTOS 任务处理队列数据

**文件**：`taskMessageSlave.c`

在 FreeRTOS 任务中轮询队列并处理数据：

```c
/* 解析缓存 */
static uint8_t st_ucMessageAnalysisBuff[256];

void vTaskMessageSlave(void *pvParameters)
{
    uint32_t uiNotifiedValue = 0;
    int32_t iLength = 0;
    
    while(1)
    {
        /* 等待任务通知（超时 20ms） */
        xTaskNotifyWait(0x00000000, 0xFFFFFFFF, &uiNotifiedValue, 20 / portTICK_RATE_MS);

        /* 读取并解析上位机发送过来的数据 */
        while((iLength = iQueueGetLengthOfOccupy(&g_TypeQueueUart1Read)) != 0)
        {
            /* 限制单次读取长度 */
            iLength = (iLength > sizeof(st_ucMessageAnalysisBuff)) 
                      ? sizeof(st_ucMessageAnalysisBuff) : iLength;

            /* 从队列出队 */
            enumQueuePopDatas(&g_TypeQueueUart1Read, st_ucMessageAnalysisBuff, iLength);

            /* 解析 Modbus 协议 */
            cModbusUnpack((uint32_t)UART_LOG, st_ucMessageAnalysisBuff, iLength);
        }
    }
}
```

**设计要点**：
- 使用 `xTaskNotifyWait` 带超时等待，兼顾响应速度和 CPU 占用
- `while` 循环确保一次处理完队列中所有数据
- 限制单次读取长度，避免缓冲区溢出

---

### 5.5 场景五：CAN 驱动封装

**文件**：`DevicesCAN.c`

与串口驱动类似的封装方式：

```c
/* 接收指定长度数据 */
int8_t cCanReceiveDatas(uint32_t can_periph, void *pvDatas, int32_t iLength)
{
    if((can_periph != (uint32_t)CAN1) || (pvDatas == NULL) || (iLength < 1))
        return 0;

    /* 判断队列内是否有足够的数据 */
    if(iQueueGetLengthOfOccupy(&g_TypeQueueCanHostRead) < iLength)
        return 3;

    /* 从队列内获取数据 */
    if(enumQueuePopDatas(&g_TypeQueueCanHostRead, pvDatas, iLength) != queueNormal)
        return 4;

    return 0;
}

/* 接收所有可用数据 */
int32_t iCanReceiveAllDatas(uint32_t can_periph, void *pvDatas, int32_t iLengthLimit)
{
    int32_t iLength = 0;

    if((can_periph != (uint32_t)CAN1) || (pvDatas == NULL) || (iLengthLimit < 1))
        return 0;

    if((iLength = iQueueGetLengthOfOccupy(&g_TypeQueueCanHostRead)) < 1)
        return 0;

    iLength = iLength > iLengthLimit ? iLengthLimit : iLength;

    if(enumQueuePopDatas(&g_TypeQueueCanHostRead, pvDatas, iLength) != queueNormal)
        return 0;

    return iLength;
}

/* 获取接收缓冲区数据长度 */
int32_t iCanReceiveLengthGet(uint32_t can_periph)
{
    if(can_periph != (uint32_t)CAN1)
        return 0;

    return iQueueGetLengthOfOccupy(&g_TypeQueueCanHostRead);
}

/* 清空接收缓冲区 */
int8_t cCanReceiveClear(uint32_t can_periph)
{
    if(can_periph != (uint32_t)CAN1)
        return 1;

    return enumQueueSetState(&g_TypeQueueCanHostRead, queueEmpty);
}
```

---

## 6. 常见问题与知识点 (Q&A)

### Q1: 为什么要多分配一个字节？

环形队列用"读写指针是否相等"判断空，用"写指针+1是否等于读指针"判断满。如果不预留一个字节，满和空的状态无法区分。

```
空: pReadFrom == pWriteTo
满: (pWriteTo + 1) % length == pReadFrom

如果用满 length，这两个条件会冲突。
```

### Q2: Lock 和 Normal 模式怎么选？

| 场景 | 推荐模式 | 原因 |
| :--- | :--- | :--- |
| 通信协议（UART/CAN） | `queueModeLock` | 数据完整性优先，丢帧会导致协议错误 |
| 传感器采样 | `queueModeNormal` | 只关心最新数据，旧数据可丢弃 |
| LOG 输出 | `queueModeLock` | 避免日志丢失导致难以排查问题 |

### Q3: 中断和主循环同时操作队列安全吗？

**单生产者-单消费者**场景下是安全的：
- 中断只调用 `Push`（修改 pWriteTo）
- 主循环只调用 `Pop`（修改 pReadFrom）
- 两个指针相互独立，不会冲突

**多生产者或多消费者**场景需要加临界区保护。

### Q4: 队列满了怎么办？

1. **Lock 模式**：`Push` 返回 `queueFull`，调用者自行处理（丢弃或重试）
2. **Normal 模式**：覆盖最旧数据，`Push` 返回 `queueFull` 作为警告

### Q5: iQueueGetLengthOfSeparetor 和 iQueueGetLengthOfOccupyNeed 的区别？

| 函数 | 查找方向 | 用途 |
| :--- | :--- | :--- |
| `iQueueGetLengthOfSeparetor` | 从头部向尾部（正向） | 查找第一个分隔符 |
| `iQueueGetLengthOfOccupyNeed` | 从尾部向头部（反向） | 查找最后一个分隔符 |

---

---

## 7. 代码质量自检 (Self-Check)

- [x] **空指针检查**：所有函数检查输入参数有效性
- [x] **边界处理**：正确处理指针回绕（环形结构）
- [x] **返回值设计**：统一使用 `enumQueueState` 枚举
- [x] **模式支持**：Lock/Normal 两种模式可配置
- [x] **无硬件依赖**：纯软件实现，可移植
- [x] **命名规范**：函数以 `enum`/`i` 前缀区分返回值类型

---

## 8. 版本变更记录 (Changelog)

| 版本 | 日期 | 变更内容 |
| :--- | :--- | :--- |
| v1.0 | 2026-02-04 | 初始版本，支持基本队列操作 |

---

**文档结束**
