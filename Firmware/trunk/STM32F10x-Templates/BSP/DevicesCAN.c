#include "stm32f1xx_hal.h"
#include "string.h"
#include "stdint.h"
#include "stdio.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "DevicesQueue.h"
#include "DevicesDelay.h"
#include "DevicesCAN.h"

#include "taskSystem.h"

CAN_HandleTypeDef   g_can1_handler;     /* CAN控制句柄 */
CAN_TxHeaderTypeDef g_can1_txheader;    /* CAN发送结构体 */

/* CAN初始化函数 */
void vCan1Init(void) 
{
    /* 初始化CAN外设时钟 */
    __HAL_RCC_CAN1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* 配置CAN引脚: RX (PA11), TX (PA12) */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_11; 
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP; 
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_12; 
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    g_can1_handler.Instance  = CAN1;
    g_can1_handler.Init.Mode = CAN_MODE_NORMAL;        /* 工作模式设置 正常 */

    /* 波特率相关,公式: 36M / 4 * (9 + 8 + 1) = 500K */
    g_can1_handler.Init.Prescaler        = 4;            /* 分频系数 */
    g_can1_handler.Init.TimeSeg1         = CAN_BS1_9TQ;  /* 时间段1 */
    g_can1_handler.Init.TimeSeg2         = CAN_BS2_8TQ;  /* 时间段2 */
    g_can1_handler.Init.SyncJumpWidth    = CAN_SJW_1TQ;  /* 重新同步跳跃宽度 */

    /* CAN功能设置 */
    g_can1_handler.Init.AutoBusOff           = DISABLE;  /* 禁止自动离线管理 */
    g_can1_handler.Init.AutoRetransmission   = DISABLE;  /* 禁止自动重发 */
    g_can1_handler.Init.AutoWakeUp           = DISABLE;  /* 禁止自动唤醒 */
    g_can1_handler.Init.ReceiveFifoLocked    = DISABLE;  /* 禁止接收FIFO锁定 */
    g_can1_handler.Init.TimeTriggeredMode    = DISABLE;  /* 禁止时间触发通信模式 */
    g_can1_handler.Init.TransmitFifoPriority = DISABLE;  /* 禁止发送FIFO优先级 */

    HAL_CAN_Init(&g_can1_handler);

    CAN_FilterTypeDef can_filterconfig;
    /* 过滤器是接收所有报文，不筛选 */   
    can_filterconfig.FilterMode = CAN_FILTERMODE_IDLIST;      /* 列表模式 */
    can_filterconfig.FilterScale = CAN_FILTERSCALE_32BIT;     /* 使用 32 位过滤器 */

    /* 配置具体的过滤器列表,具体参考手册 */
    can_filterconfig.FilterIdHigh = (0x350 << 5);             /* 标准帧 ID 左移 5 位（高 16 位）*/
    can_filterconfig.FilterIdLow = 0x0000;                    /* 低 16 位清零 */
    can_filterconfig.FilterMaskIdHigh = 0x0000;               /* 列表模式不需要掩码，保留默认值 */
    can_filterconfig.FilterMaskIdLow = 0x0000;                /* 列表模式不需要掩码，保留默认值 */

    can_filterconfig.FilterBank = 0;
    can_filterconfig.FilterFIFOAssignment = CAN_FilterFIFO0;
    can_filterconfig.FilterActivation = CAN_FILTER_ENABLE;
    can_filterconfig.SlaveStartFilterBank = 14;
    HAL_CAN_ConfigFilter(&g_can1_handler, &can_filterconfig);

     /* 启用CAN中断 */
    HAL_NVIC_SetPriority(USB_LP_CAN1_RX0_IRQn, 7, 0); /* 设置优先级 */
    HAL_NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn); /* 启用中断 */

    /* 启用CAN接收中断 */
    if (HAL_CAN_ActivateNotification(&g_can1_handler, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK) {
    }

    HAL_CAN_Start(&g_can1_handler);
}

int8_t cCanSendDatas(uint32_t can_periph, uint32_t uiID, void *pvDatas, int32_t iLength)
{
    uint32_t uiTxMail  = 0;
    int32_t iTimeOut;
    int8_t cError;

    if(pvDatas == NULL)
        return 1;

    /* 获取互斥信号量 */
    xSemaphoreTakeRecursive(g_xCan1Semaphore, portMAX_DELAY);

    uiTxMail = CAN_TX_MAILBOX0;

    g_can1_txheader.ExtId = uiID;
    g_can1_txheader.DLC = (iLength > 8 ) ? 8 : iLength;
    g_can1_txheader.IDE = CAN_ID_EXT;
    g_can1_txheader.RTR = CAN_RTR_DATA;
    
    while(HAL_CAN_GetTxMailboxesFreeLevel(&g_can1_handler) != 3)
       vTaskDelay(50 / portTICK_RATE_MS);

    HAL_CAN_AddTxMessage(&g_can1_handler, &g_can1_txheader, pvDatas, &uiTxMail);

    /* 释放互斥信号量 */
    xSemaphoreGiveRecursive(g_xCan1Semaphore);

    return cError;
}

int8_t cCanReceiveByte(uint32_t can_periph, uint8_t *pucByte)
{
    return cCanReceiveDatas(can_periph, pucByte, 1);
}

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

int32_t iCanReceiveAllDatas(uint32_t can_periph, void *pvDatas, int32_t iLengthLimit)
{
    int32_t iLength = 0;

    if((can_periph != (uint32_t)CAN1) || (pvDatas == NULL) || (iLengthLimit < 1))
        return 0;

    /* 读取队列内有效数据的长度 */
    if((iLength = iQueueGetLengthOfOccupy(&g_TypeQueueCanHostRead)) < 1)
        return 0;

    /* 限制读取长度 */
    iLength = iLength > iLengthLimit ? iLengthLimit : iLength;

    /* 从队列内获取数据 */
    if(enumQueuePopDatas(&g_TypeQueueCanHostRead, pvDatas, iLength) != queueNormal)
        return 0;

    return iLength;
}

int32_t iCanReceiveLengthGet(uint32_t can_periph)
{
    if(can_periph != (uint32_t)CAN1)
        return 0;

    return iQueueGetLengthOfOccupy(&g_TypeQueueCanHostRead);
}

int8_t cCanReceiveClear(uint32_t can_periph)
{
    if(can_periph != (uint32_t)CAN1)
        return 1;

    /* 设置队列状态为空 */
    return enumQueueSetState(&g_TypeQueueCanHostRead, queueEmpty);
}
