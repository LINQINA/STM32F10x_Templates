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
            
            /* 设置软件 RTC 时间：2026-01-29 20:00:00 东八区 */
            ptypeControlInfo->typeTimeInfo.year   = 2026;
            ptypeControlInfo->typeTimeInfo.month  = 1;
            ptypeControlInfo->typeTimeInfo.day    = 29;
            ptypeControlInfo->typeTimeInfo.hour   = 20;
            ptypeControlInfo->typeTimeInfo.minute = 0;
            ptypeControlInfo->typeTimeInfo.second = 0;
            ptypeControlInfo->typeTimeInfo.UTC    = 8.0f;
            
            vRTCSetTimeByStruct(&ptypeControlInfo->typeTimeInfo);
            
            cLogPrintfNormal("RTC Set: %d-%02d-%02d %02d:%02d:%02d\n", 
                ptypeControlInfo->typeTimeInfo.year, 
                ptypeControlInfo->typeTimeInfo.month, 
                ptypeControlInfo->typeTimeInfo.day,
                ptypeControlInfo->typeTimeInfo.hour, 
                ptypeControlInfo->typeTimeInfo.minute, 
                ptypeControlInfo->typeTimeInfo.second);
        }
        else if(ptypeKeyData->valueLoosen == KEY_1)
        {
            vBeepSoundFast(1);
            
            /* 获取当前时间戳（微秒转秒） */
            int64_t lStamp = lRTCGetTime();
            
            /* 转换成时间结构体 */
            vStampToTime(lStamp, &ptypeControlInfo->typeTimeInfo, 8.0f);
            
            /* 打印到串口 */
            cLogPrintfNormal("RTC Get: %d-%02d-%02d %02d:%02d:%02d Week:%d\n", 
                ptypeControlInfo->typeTimeInfo.year, 
                ptypeControlInfo->typeTimeInfo.month, 
                ptypeControlInfo->typeTimeInfo.day,
                ptypeControlInfo->typeTimeInfo.hour, 
                ptypeControlInfo->typeTimeInfo.minute, 
                ptypeControlInfo->typeTimeInfo.second,
                ptypeControlInfo->typeTimeInfo.week);
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
