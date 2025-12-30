#include "stm32f1xx_hal.h"
#include "stdint.h"
#include "stdio.h"
#include "string.h"

#include "DevicesDelay.h"
#include "DevicesUart.h"
#include "DevicesQueue.h"

#include "DriverLogPrintf.h"

/* USART1 LOG */
UART_HandleTypeDef g_uart1_handle;
DMA_HandleTypeDef  g_dma_usart1_tx;
DMA_HandleTypeDef  g_dma_usart1_rx;

/* USART2 BUS */
UART_HandleTypeDef g_uart2_handle;
DMA_HandleTypeDef  g_dma_usart2_tx;
DMA_HandleTypeDef  g_dma_usart2_rx;

uint8_t g_USART1ReadDMABuff[USART1_DMA_READ_LENGTH] = {0};
uint8_t g_USART2ReadDMABuff[USART2_DMA_READ_LENGTH] = {0};

void vUart1Init(void)
{
    /* Enable GPIOA clock */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    /* Enable USART1 clock */
    __HAL_RCC_USART1_CLK_ENABLE();

    /* USART1 GPIO Configuration
    PA9  ----- USART1_TX
    PA10 ----- USART1_RX */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_INPUT;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* USART1 initialization */
    g_uart1_handle.Instance = USART1;
    g_uart1_handle.Init.BaudRate = 115200;
    g_uart1_handle.Init.WordLength = UART_WORDLENGTH_8B;
    g_uart1_handle.Init.StopBits = UART_STOPBITS_1;
    g_uart1_handle.Init.Parity = UART_PARITY_NONE;
    g_uart1_handle.Init.Mode = UART_MODE_TX_RX;
    g_uart1_handle.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    g_uart1_handle.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&g_uart1_handle);

    vUart1DMAInit();

    /* USART1 interrupt Init */
    HAL_NVIC_SetPriority(USART1_IRQn, 8, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);

    __HAL_UART_ENABLE_IT(&g_uart1_handle, UART_IT_IDLE); /* 使能串口1空闲中断 */
    __HAL_UART_ENABLE_IT(&g_uart1_handle, UART_IT_ERR);  /* 使能串口1错误中断 */

    HAL_UART_Receive_DMA(&g_uart1_handle, g_USART1ReadDMABuff, USART1_DMA_READ_LENGTH);
}

void vUart1DMAInit(void)
{
    __HAL_RCC_DMA1_CLK_ENABLE();

    /* USART1_TX Init */
    g_dma_usart1_tx.Instance = DMA1_Channel4;
    g_dma_usart1_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    g_dma_usart1_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    g_dma_usart1_tx.Init.MemInc = DMA_MINC_ENABLE;
    g_dma_usart1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    g_dma_usart1_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    g_dma_usart1_tx.Init.Mode = DMA_NORMAL;
    g_dma_usart1_tx.Init.Priority = DMA_PRIORITY_LOW;
    HAL_DMA_Init(&g_dma_usart1_tx);

    __HAL_LINKDMA(&g_uart1_handle, hdmatx, g_dma_usart1_tx);

    /* USART1_RX Init */
    g_dma_usart1_rx.Instance = DMA1_Channel5;
    g_dma_usart1_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    g_dma_usart1_rx.Init.PeriphInc = DMA_PINC_DISABLE;
    g_dma_usart1_rx.Init.MemInc = DMA_MINC_ENABLE;
    g_dma_usart1_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    g_dma_usart1_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    g_dma_usart1_rx.Init.Mode = DMA_CIRCULAR;
    g_dma_usart1_rx.Init.Priority = DMA_PRIORITY_HIGH;
    HAL_DMA_Init(&g_dma_usart1_rx);

    __HAL_LINKDMA(&g_uart1_handle, hdmarx, g_dma_usart1_rx);

    /* DMA interrupt init */
    HAL_NVIC_SetPriority(DMA1_Channel4_IRQn, 8, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel4_IRQn);

    HAL_NVIC_SetPriority(DMA1_Channel5_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel5_IRQn);
}


void vUart2Init(void)
{
    /* Enable GPIOA clock */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    /* Enable USART2 clock */
    __HAL_RCC_USART2_CLK_ENABLE();

    /* USART2 GPIO Configuration
    PA2  ----- USART2_TX
    PA3  ----- USART2_RX */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_INPUT;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* USART1 initialization */
    g_uart2_handle.Instance = USART2;
    g_uart2_handle.Init.BaudRate = 9600;
    g_uart2_handle.Init.WordLength = UART_WORDLENGTH_8B;
    g_uart2_handle.Init.StopBits = UART_STOPBITS_1;
    g_uart2_handle.Init.Parity = UART_PARITY_NONE;
    g_uart2_handle.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    g_uart2_handle.Init.Mode = UART_MODE_TX_RX;
    HAL_UART_Init(&g_uart2_handle);

    vUart2DMAInit();

    /* USART1 interrupt Init */
    HAL_NVIC_SetPriority(USART2_IRQn, 3, 3);
    HAL_NVIC_EnableIRQ(USART2_IRQn);

    __HAL_UART_ENABLE_IT(&g_uart2_handle, UART_IT_RXNE);
    __HAL_UART_ENABLE_IT(&g_uart2_handle, UART_IT_IDLE);
    __HAL_UART_ENABLE_IT(&g_uart2_handle, UART_IT_ERR);

    HAL_UART_Receive_DMA(&g_uart2_handle, g_USART2ReadDMABuff, USART2_DMA_READ_LENGTH);
}

void vUart2DMAInit(void)
{
    __HAL_RCC_DMA1_CLK_ENABLE();

    /* USART2_TX Init */
    g_dma_usart2_tx.Instance = DMA1_Channel7;
    g_dma_usart2_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    g_dma_usart2_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    g_dma_usart2_tx.Init.MemInc = DMA_MINC_ENABLE;
    g_dma_usart2_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    g_dma_usart2_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    g_dma_usart2_tx.Init.Mode = DMA_NORMAL;
    g_dma_usart2_tx.Init.Priority = DMA_PRIORITY_LOW;
    HAL_DMA_Init(&g_dma_usart2_tx);

    __HAL_LINKDMA(&g_uart2_handle, hdmatx, g_dma_usart2_tx);

    /* USART2_RX Init */
    g_dma_usart2_rx.Instance = DMA1_Channel6;
    g_dma_usart2_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    g_dma_usart2_rx.Init.PeriphInc = DMA_PINC_DISABLE;
    g_dma_usart2_rx.Init.MemInc = DMA_MINC_ENABLE;
    g_dma_usart2_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    g_dma_usart2_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    g_dma_usart2_rx.Init.Mode = DMA_CIRCULAR;
    g_dma_usart2_rx.Init.Priority = DMA_PRIORITY_HIGH;
    HAL_DMA_Init(&g_dma_usart2_rx);

    __HAL_LINKDMA(&g_uart2_handle, hdmarx, g_dma_usart2_rx);

    /* DMA interrupt init */
    HAL_NVIC_SetPriority(DMA1_Channel6_IRQn, 8, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel6_IRQn);

    HAL_NVIC_SetPriority(DMA1_Channel7_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel7_IRQn);
}

void vUartBaudrateSet(uint32_t uiUsartPeriph, int32_t iBaudrate)
{
    UART_HandleTypeDef *huart;

    if(iBaudrate < 1)
        return;

    switch (uiUsartPeriph)
    {
        case (uint32_t)UART_LOG: huart = &g_uart1_handle; break;
        case (uint32_t)UART_BUS: huart = &g_uart2_handle; break;

        default:
            break;
    }

    __HAL_UART_DISABLE(huart);

    huart = (UART_HandleTypeDef *)uiUsartPeriph;
    huart->Init.BaudRate = iBaudrate;

    __HAL_UART_ENABLE(huart);
}

void vUartSendDatas(uint32_t uiUsartPeriph, void *pvDatas, int32_t iLength)
{
    uint32_t uiTime = 0;
    uint8_t *pucDatas = pvDatas;
    UART_HandleTypeDef* huart;

    switch (uiUsartPeriph)
    {
        case (uint32_t)UART_LOG: huart = &g_uart1_handle; break;
        case (uint32_t)UART_BUS: huart = &g_uart2_handle; break;

        default:
            break;
    }

    while((iLength--) > 0)
    {
        uiTime = 1000;
        while((RESET == __HAL_UART_GET_FLAG(huart, UART_FLAG_TXE)) && (--uiTime));

        /* Transmit Data */
        huart->Instance->DR = *pucDatas++;
    }
}

void vUartSendStrings(uint32_t uiUsartPeriph, char *pcStrings)
{
    vUartSendDatas(uiUsartPeriph, (uint8_t *)pcStrings, strlen(pcStrings));
}


void vUartDMASendDatas(uint32_t uiUsartPeriph, void *pvDatas, int32_t iLength)
{
    UART_HandleTypeDef *huart;
    DMA_HandleTypeDef *dmaTxHandle;
    uint32_t uiTime;

    switch (uiUsartPeriph)
    {
        case (uint32_t)UART_LOG: huart = &g_uart1_handle; dmaTxHandle = &g_dma_usart1_tx; break;
        case (uint32_t)UART_BUS: huart = &g_uart2_handle; dmaTxHandle = &g_dma_usart2_tx; break;

        default:
            break;
    }

    HAL_UART_Transmit_DMA(huart, pvDatas, iLength);

    /* 只有 RS485 总线需要快速关闭总线占用 */
    if (uiUsartPeriph == (uint32_t)UART_BUS)
    {
        __HAL_DMA_ENABLE_IT(dmaTxHandle, DMA_IT_TC);
    }

    /* 等待本次DMA传输完成 */
    while ((__HAL_DMA_GET_FLAG(dmaTxHandle, DMA_FLAG_TC7) == RESET) &&
           (__HAL_DMA_GET_COUNTER(dmaTxHandle) != 0) &&
           ((iLength--) > 0))
    {
        /* 以9600波特率计算时长 */
        vDelayMs(2);
    }

    /* 等待最后一个字节发送完成、以9600波特率计算时长 */
    uiTime = 10 * 1000 / 960;
    while((__HAL_USART_GET_FLAG(huart, USART_FLAG_TC) == RESET) && (uiTime--))
    {
        vDelayMs(1);
    }
}

void vUartDMASendStrings(uint32_t uiUsartPeriph, char *pcStrings)
{
    vUartDMASendDatas(uiUsartPeriph, (uint8_t *)pcStrings, strlen(pcStrings));
}

int8_t cUartReceiveDatas(uint32_t uiUsartPeriph, void *pvDatas, int32_t iLength)
{
    QueueType *ptypeQueueHandle = NULL;

    if(iLength < 1)
        return 1;

    switch(uiUsartPeriph)
    {
        case (uint32_t)UART_LOG: ptypeQueueHandle = &g_TypeQueueUart0Read; break;
        case (uint32_t)UART_BUS: ptypeQueueHandle = &g_TypeQueueUart1Read; break;

        default : cLogPrintfError("cUartReceiveDatas channel error.\r\n"); return 2;
    }

    /* 判断队列内是否有足够的数据 */
    if(iQueueGetLengthOfOccupy(ptypeQueueHandle) < iLength)
        return 3;

    /* 从队列内获取数据 */
    if(enumQueuePopDatas(ptypeQueueHandle, pvDatas, iLength) != queueNormal)
        return 4;

    return 0;
}

int8_t cUartReceiveByte(uint32_t uiUsartPeriph, uint8_t *pucByte)
{
    return cUartReceiveDatas(uiUsartPeriph, pucByte, 1);
}

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

        default : cLogPrintfError("cUartReceiveAllDatas channel error.\r\n"); return 0;
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

int32_t iUartReceiveLengthGet(uint32_t uiUsartPeriph)
{
    QueueType *ptypeQueueHandle = NULL;

    switch(uiUsartPeriph)
    {
        case (uint32_t)UART_LOG: ptypeQueueHandle = &g_TypeQueueUart0Read; break;
        case (uint32_t)UART_BUS: ptypeQueueHandle = &g_TypeQueueUart1Read; break;

        default : cLogPrintfError("cUartReceiveLengthGet channel error.\r\n"); return 0;
    }

    /* 读取队列内有效数据的长度 */
    return iQueueGetLengthOfOccupy(ptypeQueueHandle);
}

int8_t cUartReceiveClear(uint32_t uiUsartPeriph)
{
    QueueType *ptypeQueueHandle = NULL;

    switch(uiUsartPeriph)
    {
        case (uint32_t)UART_LOG: ptypeQueueHandle = &g_TypeQueueUart0Read; break;
        case (uint32_t)UART_BUS: ptypeQueueHandle = &g_TypeQueueUart1Read; break;

        default : cLogPrintfError("cUartReceiveClear channel error.\r\n"); return 1;
    }

    /* 设置队列状态为空 */
    return enumQueueSetState(ptypeQueueHandle, queueEmpty);
}
