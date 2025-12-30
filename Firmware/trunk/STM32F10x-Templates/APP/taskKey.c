#include "stm32f1xx.h"
#include "stdint.h"
#include "stdio.h"
#include "math.h"

#include "DevicesBeep.h"
#include "DevicesKey.h"
#include "DevicesFlash.h"
#include "DevicesSPIFlash.h"
#include "DevicesAT24C02.h"

#include "taskKey.h"

void vTaskKey(KeyTypeDef *ptypeKeyData)
{
    /* 按键短按后已经松开 */
    if(ptypeKeyData->state == (keyShort | keyCut))
    {
        if(ptypeKeyData->valueLoosen == KEY_0)
        {
            vBeepSoundFast(1);
        }
        else if(ptypeKeyData->valueLoosen == KEY_1)
        {
            vBeepSoundFast(1);
        }
        else if(ptypeKeyData->valueLoosen == KEY_UP)
        {
            vBeepSoundFast(1);
        }
    }
    /* 按键长按但没有松开 */
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
