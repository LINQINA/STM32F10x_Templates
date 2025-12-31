#include "stm32f1xx_hal.h"
#include "stdint.h"
#include "stdio.h"
#include "string.h"

#include "DevicesDelay.h"
#include "DevicesUart.h"
#include "DevicesQueue.h"
#include "DevicesRS485.h"

void vRS485BusInit(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOD_CLK_ENABLE();

    GPIO_InitStruct.Pin = RS485_BUS_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(RS485_BUS_GPIO_Port, &GPIO_InitStruct);

    RS485_BUS_MODE_RECEIVE();
}

int8_t cRS485xSendDatas(uint32_t uiChannel, void *pvBuff, int32_t iLength)
{
    if(uiChannel == (uint32_t)UART_BUS)
    {
        /* 设置为发送模式 */
        RS485_BUS_MODE_SEND();

        /* RS485协议推荐在设置为发送模式之后，增加一个bit的延时 */
        vDelayUs(1000000.0f / 9600.0f);

        /* 发送数据 */
        vUartDMASendDatas(uiChannel, pvBuff, iLength);

        /* RS485协议推荐在设置为接收模式之前，增加一个bit的延时 */
        vDelayUs(1000000.0f / 9600.0f);

        /* 设置为接收模式 */
        RS485_BUS_MODE_RECEIVE();
    }

    return 0;
}

int8_t cRS485xReceiveDatas(uint32_t uiChannel, void *pvBuff, int32_t iLength)
{
    return cUartReceiveDatas(uiChannel, pvBuff, iLength);
}

int32_t iRS485xReceiveAllDatas(uint32_t uiChannel, void *pvBuff, int32_t iLengthLimit)
{
    return iUartReceiveAllDatas(uiChannel, pvBuff, iLengthLimit);
}

int8_t cRS485xReceiveByte(uint32_t uiChannel, uint8_t *pucBuff)
{
    return cUartReceiveDatas(uiChannel, pucBuff, 1);
}

int32_t iRS485xReceiveLengthGet(uint32_t uiChannel)
{
    return iUartReceiveLengthGet(uiChannel);
}

int8_t cRS485xReceiveClear(uint32_t uiChannel)
{
    return cUartReceiveClear(uiChannel);
}
