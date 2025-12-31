#include "stdint.h"
#include "string.h"
#include "DriverLogPrintf.h"
#include "DevicesQueue.h"


/* UART LOG */
QueueType g_TypeQueueUart0Read = {0};
static uint8_t st_ucQueueUart0ReadBuff[QUEUE_UART0_READ_LENGTH + 4] = {0};

/* UART BUS */
QueueType g_TypeQueueUart1Read = {0};
static uint8_t st_ucQueueUart1ReadBuff[QUEUE_UART1_READ_LENGTH + 4] = {0};

/* CAN */
QueueType g_TypeQueueCanHostRead = {0};
static uint8_t st_ucQueueCanHostReadBuff[QUEUE_CAN_HOST_READ_LENGTH + 4] = {0};


enumQueueState enumQueueInit(void)
{
    enumQueueState enumState = queueNormal;


    if(enumQueueCreate(&g_TypeQueueUart0Read, "Uart 0 Read", st_ucQueueUart0ReadBuff, QUEUE_UART0_READ_LENGTH, queueModeLock) != queueNormal)
        enumState = queueError;

    if(enumQueueCreate(&g_TypeQueueUart1Read, "Uart 1 Read", st_ucQueueUart1ReadBuff, QUEUE_UART1_READ_LENGTH, queueModeLock) != queueNormal)
        enumState = queueError;

    if(enumQueueCreate(&g_TypeQueueCanHostRead, "Can Host Read",    st_ucQueueCanHostReadBuff,  QUEUE_CAN_HOST_READ_LENGTH,queueModeLock) != queueNormal)
        enumState = queueError; 

    if(enumState != queueNormal)
        cLogPrintfError("enumQueueInit error.\r\n");

    return enumState;
}

/*
 * Return:      创建是否成功状态值
 * Parameters:  *pTypeQueue: 队列结构体指针; pucName: 队列名称; iLength: 队列长度
 * Description: 初始化队列
 */
enumQueueState enumQueueCreate(QueueType *pTypeQueue, char *pcName, uint8_t *pucBuff, int32_t iLength, enumQueueMode enumMode)
{
    if(pTypeQueue == NULL)
        return queueNull;

    if(iLength < 1)
        return queueEmpty;

    pTypeQueue->mode      = enumMode;
    pTypeQueue->pcName    = pcName;
    pTypeQueue->length    = iLength + 1;
    pTypeQueue->pReadFrom = pucBuff;
    pTypeQueue->pWriteTo  = pucBuff;
    pTypeQueue->pTail     = pucBuff + pTypeQueue->length;
    pTypeQueue->pHead     = pucBuff;

    return queueNormal;
}

/*
 * Return:      队列缓存空满状态
 * Parameters:  *pTypeQueue: 队列结构体指针
 * Description: 获取队列缓存空满状态
 */
enumQueueState enumQueueGetState(QueueType *pTypeQueue)
{
    uint8_t * pNow = NULL;

    if((pTypeQueue == NULL) || (pTypeQueue->pHead == NULL))
      return queueNull;

    if(pTypeQueue->pReadFrom == pTypeQueue->pWriteTo)
      return queueEmpty;

    pNow = pTypeQueue->pWriteTo + 1;
    pNow = (pNow >= pTypeQueue->pTail) ? pTypeQueue->pHead : pNow;

    if(pNow == pTypeQueue->pReadFrom)
      return queueFull;

    return queueNormal;
}

/*
 * Return:      void
 * Parameters:  *pTypeQueue: 队列结构体指针; ucStateFlag: 空满状态
 * Description: 设置队列缓存空满状态
 */
enumQueueState enumQueueSetState(QueueType *pTypeQueue, enumQueueState enumState)
{
    if((pTypeQueue == NULL) || (pTypeQueue->pHead == NULL))
        return queueNull;

    switch(enumState)
    {
        case queueEmpty:
            memset(pTypeQueue->pHead, 0, pTypeQueue->length);
            pTypeQueue->pReadFrom = pTypeQueue->pWriteTo = pTypeQueue->pHead;
            break;

        default : break;
    }

    return queueNormal;
}

/*
 * Return:      查找到的数据位置
 * Parameters:
 * Description: memrchr替代函数
 */
static void *vQueueMemrchr(void *pvHandle, uint8_t ucValue, int32_t iCount)
{
    uint8_t *pucCheck = pvHandle;

    pucCheck += iCount - 1;

    while((iCount--) > 0)
    {
        if(*pucCheck == ucValue)
            return pucCheck;

        --pucCheck;
    }

    return NULL;
}

/*
 * Return:      队列缓存中有效数据长度
 * Parameters:  *pTypeQueue: 队列结构体指针
 * Description: 获取队列缓存中有效数据长度
 */
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

/*
 * Return:      队列缓存中有效数据长度
 * Parameters:  *pTypeQueue: 队列结构体指针; ucByte: 指定的有效字节
 * Description: 获取队列缓存中有效数据长度，需要有指定的有效字节
 */
 int32_t iQueueGetLengthOfOccupyNeed(QueueType *pTypeQueue, uint8_t ucByte)
{
    uint8_t *pucHead = NULL;
    int32_t iLength = 0;

    if((pTypeQueue == NULL) || (pTypeQueue->pHead == NULL))
        return 0;

    /* pHead|-------pReadFrom===============pWriteTo-------------|pTail */
    if(pTypeQueue->pReadFrom <= pTypeQueue->pWriteTo)
    {
        if((pucHead = vQueueMemrchr(pTypeQueue->pReadFrom, ucByte, pTypeQueue->pWriteTo - pTypeQueue->pReadFrom)) != NULL)
            iLength = (pucHead - pTypeQueue->pReadFrom) + 1;
    }
    /* pHead|=======pWriteTo---------------pReadFrom=============|pTail */
    else
    {
        /* pHead|=======|pWriteTo */
        if((pucHead = vQueueMemrchr(pTypeQueue->pHead, ucByte, pTypeQueue->pWriteTo - pTypeQueue->pHead)) != NULL)
            iLength = (pucHead - pTypeQueue->pHead) + (pTypeQueue->pTail - pTypeQueue->pReadFrom) + 1;

        /* pReadFrom|=============|pTail */
        else if((pucHead = vQueueMemrchr(pTypeQueue->pReadFrom, ucByte, pTypeQueue->pTail - pTypeQueue->pReadFrom)) != NULL)
            iLength = (pucHead - pTypeQueue->pReadFrom) + 1;
    }

    return iLength;
}

/*
 * Return:      队列缓存中到下一个分隔符的有效数据长度
 * Parameters:  *pTypeQueue: 队列结构体指针
 * Description: 获取队列缓存中有效数据长度
 */
int32_t iQueueGetLengthOfSeparetor(QueueType *pTypeQueue, uint8_t ucByte)
{
    uint8_t *pucHead = NULL;
    int32_t iLength = 0;

    if((pTypeQueue == NULL) || (pTypeQueue->pHead == NULL))
        return 0;

    /* pHead|-------pReadFrom===============pWriteTo-------------|pTail */
    if(pTypeQueue->pReadFrom <= pTypeQueue->pWriteTo)
    {
        if((pucHead = memchr(pTypeQueue->pReadFrom, ucByte, pTypeQueue->pWriteTo - pTypeQueue->pReadFrom)) != NULL)
            iLength = (pucHead - pTypeQueue->pReadFrom) + 1;
    }
    /* pHead|=======pWriteTo---------------pReadFrom=============|pTail */
    else
    {
        /* pReadFrom|=============|pTail */
        if((pucHead = memchr(pTypeQueue->pReadFrom, ucByte, pTypeQueue->pTail - pTypeQueue->pReadFrom)) != NULL)
            iLength = (pucHead - pTypeQueue->pReadFrom) + 1;

        /* pHead|=======|pWriteTo */
        else if((pucHead = memchr(pTypeQueue->pHead, ucByte, pTypeQueue->pWriteTo - pTypeQueue->pHead)) != NULL)
            iLength = (pucHead - pTypeQueue->pHead) + (pTypeQueue->pTail - pTypeQueue->pReadFrom) + 1;
    }

    return iLength;
}

/*
 * Return:      队列缓存中剩余长度
 * Parameters:  *pTypeQueue: 队列结构体指针
 * Description: 获取队列缓存中剩余长度
 */
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

/*
 * Return:      是否入队成功状态
 * Parameters:  *pTypeQueue: 队列结构体指针; ucData: 待入队字节数据
 * Description: 入队一个字节数据
 */
enumQueueState enumQueuePushByte(QueueType *pTypeQueue, uint8_t ucData)
{
    enumQueueState enumPushState = queueNormal;

    if((pTypeQueue == NULL) || (pTypeQueue->pHead == NULL))
        return queueNull;

    if(enumQueueGetState(pTypeQueue) == queueFull)
    {
        if(pTypeQueue->mode == queueModeLock)
            return queueFull;

        enumPushState = queueFull;
    }

    *pTypeQueue->pWriteTo++ = ucData;
    pTypeQueue->pWriteTo = ((pTypeQueue->pWriteTo >= pTypeQueue->pTail) ? pTypeQueue->pHead : pTypeQueue->pWriteTo);

    /* 在溢出时，需要把read指针指向当前队列新的末尾 */
    if(enumPushState == queueFull)
    {
        pTypeQueue->pReadFrom = pTypeQueue->pWriteTo + 1;
        pTypeQueue->pReadFrom = ((pTypeQueue->pReadFrom >= pTypeQueue->pTail) ? pTypeQueue->pHead : pTypeQueue->pReadFrom);
    }

    return enumPushState;
}

/*
 * Return:      是否出队成功状态
 * Parameters:  *pTypeQueue: 队列结构体指针; *pucData: 待出队字节数据指针
 * Description: 出队一个字节数据
 */
enumQueueState enumQueuePopByte(QueueType *pTypeQueue, uint8_t *pucData)
{
    if((pTypeQueue == NULL) || (pTypeQueue->pHead == NULL))
        return queueNull;

    if(pTypeQueue->pReadFrom == pTypeQueue->pWriteTo)
      return queueEmpty;

    *pucData = *pTypeQueue->pReadFrom++;
    pTypeQueue->pReadFrom = ((pTypeQueue->pReadFrom >= pTypeQueue->pTail) ? pTypeQueue->pHead : pTypeQueue->pReadFrom);

    return queueNormal;
}

/*
 * Return:      是否出队成功状态
 * Parameters:  *pTypeQueue: 队列结构体指针; *pucData: 待出队字节数据指针
 * Description: 出队一个字节数据，并保留队列中的原数据
 */
enumQueueState enumQueueViewByte(QueueType *pTypeQueue, uint8_t *pucData)
{
    if((pTypeQueue == NULL) || (pTypeQueue->pHead == NULL))
        return queueNull;

    if(pTypeQueue->pReadFrom == pTypeQueue->pWriteTo)
      return queueEmpty;

    *pucData = *pTypeQueue->pReadFrom;

    return queueNormal;
}

/*
 * Return:      是否入队成功状态
 * Parameters:  *pTypeQueue: 队列结构体指针; *ppHead: 待入队数据缓存指针; iLength: 缓存长度
 * Description: 入队一系列数据
 */
enumQueueState enumQueuePushDatas(QueueType *pTypeQueue, void *pvBuff, int32_t iLength)
{
    uint8_t *pucHandle = pvBuff;
    enumQueueState enumPushState = queueNormal;

    if((pTypeQueue == NULL) || (pTypeQueue->pHead == NULL))
        return queueNull;

    if(iQueueGetLengthOfRemaining(pTypeQueue) < iLength)
    {
        if(pTypeQueue->mode == queueModeLock)
            return queueFull;

        enumPushState = queueFull;
    }

    while((iLength--) > 0)
    {
        *pTypeQueue->pWriteTo++ = *pucHandle++;
        pTypeQueue->pWriteTo = ((pTypeQueue->pWriteTo >= pTypeQueue->pTail) ? pTypeQueue->pHead : pTypeQueue->pWriteTo);
    }

    /* 在溢出时，需要把read指针指向当前队列新的末尾 */
    if(enumPushState == queueFull)
    {
        pTypeQueue->pReadFrom = pTypeQueue->pWriteTo + 1;
        pTypeQueue->pReadFrom = ((pTypeQueue->pReadFrom >= pTypeQueue->pTail) ? pTypeQueue->pHead : pTypeQueue->pReadFrom);
    }

    return enumPushState;
}

/*
 * Return:      是否出队成功状态
 * Parameters:  *pTypeQueue: 队列结构体指针; *ppHead: 待出队数据缓存指针; iLength: 缓存长度
 * Description: 出队一系列数据
 */
enumQueueState enumQueuePopDatas(QueueType *pTypeQueue, void *pvBuff, int32_t iLength)
{
    uint8_t *pucHandle = pvBuff;

    if((pTypeQueue == NULL) || (pTypeQueue->pHead == NULL))
        return queueNull;

    if(pvBuff == NULL)
        return queueError;

    if(iQueueGetLengthOfOccupy(pTypeQueue) < iLength)
        return queueError;

    while((iLength--) > 0)
    {
        *pucHandle++ = *pTypeQueue->pReadFrom++;
        pTypeQueue->pReadFrom = ((pTypeQueue->pReadFrom >= pTypeQueue->pTail) ? pTypeQueue->pHead : pTypeQueue->pReadFrom);
    }

    return queueNormal;
}

/*
 * Return:      是否出队成功状态
 * Parameters:  *pTypeQueue: 队列结构体指针; *ppHead: 待出队数据缓存指针; iLength: 缓存长度
 * Description: 出队一系列数据，并保留队列中的原数据
 */
enumQueueState enumQueueViewDatas(QueueType *pTypeQueue, void *pvBuff, int32_t iLength)
{
    uint8_t *pucHandle = pvBuff, *pucReadFrom = NULL;

    if((pTypeQueue == NULL) || (pTypeQueue->pHead == NULL))
        return queueNull;

    if(pvBuff == NULL)
        return queueError;

    if(iQueueGetLengthOfOccupy(pTypeQueue) < iLength)
        return queueError;

    pucReadFrom = pTypeQueue->pReadFrom;

    while((iLength--) > 0)
    {
        *pucHandle++ = *pucReadFrom++;
        pucReadFrom = ((pucReadFrom >= pTypeQueue->pTail) ? pTypeQueue->pHead : pucReadFrom);
    }

    return queueNormal;
}

/*
 * Return:      是否出队成功状态
 * Parameters:  *pTypeQueue: 队列结构体指针; *ppHead: 待出队数据缓存指针; iLength: 缓存长度; ucByte: 指定的有效字节
 * Description: 出队一系列数据，需要有指定的有效字节
 */
enumQueueState enumQueuePopDatasNeed(QueueType *pTypeQueue, void *pvBuff, int32_t iLength, uint8_t ucByte)
{
    uint8_t *pucHandle = pvBuff;

    if((pTypeQueue == NULL) || (pTypeQueue->pHead == NULL))
        return queueNull;

    if(pvBuff == NULL)
        return queueError;

    if(iQueueGetLengthOfOccupyNeed(pTypeQueue, ucByte) < iLength)
        return queueError;

    while((iLength--) > 0)
    {
        *pucHandle++ = *pTypeQueue->pReadFrom++;
        pTypeQueue->pReadFrom = ((pTypeQueue->pReadFrom >= pTypeQueue->pTail) ? pTypeQueue->pHead : pTypeQueue->pReadFrom);
    }

    return queueNormal;
}

/*
 * Return:      是否出队成功状态
 * Parameters:  *pTypeQueue: 队列结构体指针; *ppHead: 待出队数据缓存指针; iLength: 缓存长度; ucByte: 指定的有效字节
 * Description: 出队一系列数据，需要有指定的有效字节，并保留队列中的原数据
 */
enumQueueState enumQueueViewDatasNeed(QueueType *pTypeQueue, void *pvBuff, int32_t iLength, uint8_t ucByte)
{
    uint8_t *pucHandle = pvBuff, *pucReadFrom = NULL;

    if((pTypeQueue == NULL) || (pTypeQueue->pHead == NULL))
        return queueNull;

    if(pvBuff == NULL)
        return queueError;

    if(iQueueGetLengthOfOccupyNeed(pTypeQueue, ucByte) < iLength)
        return queueError;

    pucReadFrom = pTypeQueue->pReadFrom;

    while((iLength--) > 0)
    {
        *pucHandle++ = *pucReadFrom++;
        pucReadFrom = ((pucReadFrom >= pTypeQueue->pTail) ? pTypeQueue->pHead : pucReadFrom);
    }

    return queueNormal;
}
