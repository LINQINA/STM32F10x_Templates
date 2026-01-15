#ifndef _DevicesQueue_H_
#define _DevicesQueue_H_


#define QUEUE_MAX_LENGTH            (512)

#define QUEUE_UART0_READ_LENGTH     (512)
#define QUEUE_UART1_READ_LENGTH     (512)


/* 使x对n字节对齐 */
#define queueRoundUp(x, n) (((x) + (n) - 1) & (~((n) - 1)))


typedef enum {
    queueNormal = 0,
    queueError,
    queueNull,
    queueEmpty,
    queueFull,
}enumQueueState;

typedef enum {
    queueModeNormal = 0,    /* 覆盖模式 */
    queueModeLock,          /* 锁定模式 */
}enumQueueMode;



typedef struct{
    char *pcName;

    uint8_t *pHead;
    uint8_t *pTail;
    uint8_t *pReadFrom;
    uint8_t *pWriteTo;

    int32_t length;

    enumQueueMode mode;
}QueueType;


/* USART0 */
extern QueueType g_TypeQueueUart0Read;
/* USART1 */
extern QueueType g_TypeQueueUart1Read;
/* USART2 */
extern QueueType g_TypeQueueUart2Read;
/* USART3 */
extern QueueType g_TypeQueueUart3Read;

/* 透传 */
extern QueueType g_TypeQueueSeriaNet;


enumQueueState enumQueueInit(void);
enumQueueState enumQueueCreate(QueueType *pTypeQueue, char *pcName, uint8_t *pucBuff, int32_t iLength, enumQueueMode enumMode);
enumQueueState enumQueueGetState(QueueType *pTypeQueue);
enumQueueState enumQueueSetState(QueueType *pTypeQueue, enumQueueState enumState);
int32_t iQueueGetLengthOfOccupy(QueueType *pTypeQueue);
int32_t iQueueGetLengthOfOccupyNeed(QueueType *pTypeQueue, uint8_t ucByte);
int32_t iQueueGetLengthOfSeparetor(QueueType *pTypeQueue, uint8_t ucByte);
int32_t iQueueGetLengthOfRemaining(QueueType *pTypeQueue);
enumQueueState enumQueuePushByte(QueueType *pTypeQueue, uint8_t ucData);
enumQueueState enumQueuePopByte(QueueType *pTypeQueue, uint8_t *pucData);
enumQueueState enumQueueViewByte(QueueType *pTypeQueue, uint8_t *pucData);
enumQueueState enumQueuePushDatas(QueueType *pTypeQueue, void *pvBuff, int32_t iLength);
enumQueueState enumQueuePopDatas(QueueType *pTypeQueue, void *pvBuff, int32_t iLength);
enumQueueState enumQueueViewDatas(QueueType *pTypeQueue, void *pvBuff, int32_t iLength);
enumQueueState enumQueuePopDatasNeed(QueueType *pTypeQueue, void *pvBuff, int32_t iLength, uint8_t ucByte);
enumQueueState enumQueueViewDatasNeed(QueueType *pTypeQueue, void *pvBuff, int32_t iLength, uint8_t ucByte);

#endif
