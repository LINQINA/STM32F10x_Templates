#include "stm32f1xx_hal.h"
#include "stdint.h"

#include "FreeRTOS.h"
#include "task.h"

#include "DevicesTime.h"
#include "DevicesDelay.h"

void vDelayUs(int64_t lTime)
{
    int64_t lTimeStop = lTimeGetStamp() + lTime;
    
    while(lTimeGetStamp() < lTimeStop);
}

void vDelayMs(int64_t lTime)
{
    vDelayUs(lTime * 1000);
}

void vDelayS(int64_t lTime)
{
    vDelayMs(lTime * 1000);
}

/*
 * Return:      void
 * Parameters:  Time: 延时时间
 * Description: 秒延时
 */
void vRtosDelayS(float fTime)
{
    vRtosDelayMs(fTime * 1000.0f);
}

/*
 * Return:      void
 * Parameters:  Time: 延时时间
 * Description: 毫秒延时
 */
void vRtosDelayMs(float fTime)
{
    if(fTime < 1.0f)
        return;

    if(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
        vTaskDelay(fTime / portTICK_RATE_MS);
    else
        vDelayMs(fTime);
}