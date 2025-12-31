#ifndef _DevicesCAN_H_
#define _DevicesCAN_H_


typedef struct
{
    uint32_t ff : 1;        /* 0:标准帧; 1:拓展帧 */
    uint32_t ft : 1;        /* type of frame, data or remote */
    uint32_t id : 29;
    uint8_t length;         /* 数据长度 */
    uint8_t datas[8];
}CanPackType;

void vCan1Init(void);

extern CAN_HandleTypeDef   g_can1_handler;

int8_t cCanSendDatas(uint32_t can_periph, uint32_t uiID, void *pvDatas, int32_t iLength);
int8_t cCanReceiveByte(uint32_t can_periph, uint8_t *pucByte);
int8_t cCanReceiveDatas(uint32_t can_periph, void *pvDatas, int32_t iLength);
int32_t iCanReceiveAllDatas(uint32_t can_periph, void *pvDatas, int32_t iLengthLimit);
int32_t iCanReceiveLengthGet(uint32_t can_periph);
int8_t cCanReceiveClear(uint32_t can_periph);

#endif
