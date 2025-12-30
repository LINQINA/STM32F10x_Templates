#include "stm32f1xx.h"
#include "stdint.h"
#include "stdio.h"

#include "DevicesTime.h"


volatile int64_t g_iTimeBase;


/* 获取当前系统时间 */
int64_t lTimeGetStamp(void)
{
    int64_t iTimeBaseNow = 0;
    uint32_t now = 0;

    do
    {
        iTimeBaseNow = g_iTimeBase;

        now = TIM6->CNT;

    } while(iTimeBaseNow != g_iTimeBase);

    return iTimeBaseNow + now;
}
