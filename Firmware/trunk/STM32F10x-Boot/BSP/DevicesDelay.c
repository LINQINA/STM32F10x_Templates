#include "stm32f1xx_hal.h"
#include "stdint.h"
#include "stdio.h"
#include "string.h"

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

