#ifndef _DevicesRS485_H_
#define _DevicesRS485_H_

#define RS485_BUS_GPIO_Port         GPIOD
#define RS485_BUS_Pin               GPIO_PIN_7

#define RS485_BUS_MODE_RECEIVE()    HAL_GPIO_WritePin(RS485_BUS_GPIO_Port, RS485_BUS_Pin, GPIO_PIN_RESET);
#define RS485_BUS_MODE_SEND()       HAL_GPIO_WritePin(RS485_BUS_GPIO_Port, RS485_BUS_Pin, GPIO_PIN_SET);

void vRS485BusInit(void);

int8_t cRS485xSendDatas(uint32_t uiChannel, void *pvBuff, int32_t iLength);
int8_t cRS485xReceiveDatas(uint32_t uiChannel, void *pvBuff, int32_t iLength);
int32_t iRS485xReceiveAllDatas(uint32_t uiChannel, void *pvBuff, int32_t iLengthLimit);
int8_t cRS485xReceiveByte(uint32_t uiChannel, uint8_t *pucBuff);
int32_t iRS485xReceiveLengthGet(uint32_t uiChannel);
int8_t cRS485xReceiveClear(uint32_t uiChannel);


#endif