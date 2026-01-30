#include "stm32f1xx.h"
#include "stdint.h"
#include "stdio.h"
#include "math.h"

#include "DevicesBeep.h"
#include "DevicesKey.h"
#include "DevicesFlash.h"
#include "DevicesSPIFlash.h"
#include "DevicesAT24C02.h"

#include "DevicesRTC.h"

#include "DriverLogPrintf.h"

#include "taskKey.h"
#include "taskControl.h"

void vTaskKey(KeyTypeDef *ptypeKeyData)
{
    ControlInfoType *ptypeControlInfo = ptypeControlInfoGet();

    /* 短按、并已经松开按键 */
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
