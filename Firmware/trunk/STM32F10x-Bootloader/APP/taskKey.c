#include "stm32f1xx.h"
#include "stdint.h"
#include "stdio.h"
#include "math.h"

#include "DevicesBeep.h"
#include "DevicesKey.h"
#include "DevicesFlash.h"
#include "DevicesSPIFlash.h"

#include "taskKey.h"

uint8_t ucDatas[11] = {0x01,0x02,0x03};
uint8_t ucReadDatas[12] = {0};

void vTaskKey(KeyTypeDef *ptypeKeyData)
{
    /* 短按、并已经松开按键 */
    if(ptypeKeyData->state == (keyShort | keyCut))
    {
        if(ptypeKeyData->valueLoosen == KEY_0)
        {
            cFlashWriteDatas(FLASH_APP_ADDR,ucDatas,sizeof(ucDatas));
            vBeepSoundFast(1);
        }
        else if(ptypeKeyData->valueLoosen == KEY_1)
        {
            cFlashReadDatas(FLASH_APP_ADDR,ucReadDatas,sizeof(ucReadDatas));
            vBeepSoundFast(1);
        }
        else if(ptypeKeyData->valueLoosen == KEY_UP)
        {
            vBeepSoundFast(1);
        }
    }
    /* 长按、并还没有松开按键 */
    else if(ptypeKeyData->state == (keyLong | keyAdd))
    {
        if(ptypeKeyData->valuePress == KEY_0)
        {
            vBeepSoundFast(1);
        }
        else if(ptypeKeyData->valuePress == KEY_1)
        {
            vBeepSoundFast(1);
        }
        else if(ptypeKeyData->valuePress == KEY_UP)
        {
            vBeepSoundFast(1);
        }
    }
}
