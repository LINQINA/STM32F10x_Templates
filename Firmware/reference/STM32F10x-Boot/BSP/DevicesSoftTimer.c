#include "stdint.h"
#include "stdio.h"
#include "DevicesTime.h"
#include "DevicesSoftTimer.h"


/* 单位us */
static int64_t lSoftTimerGetNow(void)
{
    return lTimeGetStamp();
}

int8_t cSoftTimerSet(SoftTimerTypeDef *ptypeTimer, int64_t lTime, SoftTimerStateEnum state)
{
    if(ptypeTimer == NULL)
        return -1;

    ptypeTimer->timeStop = lSoftTimerGetNow() + lTime;
    ptypeTimer->timeDuration = lTime;
    cSoftTimerSetState(ptypeTimer, state);

    return 0;
}

int8_t cSoftTimerReset(SoftTimerTypeDef *ptypeTimer)
{
    if(ptypeTimer == NULL)
        return -1;

    ptypeTimer->timeStop = lSoftTimerGetNow() + ptypeTimer->timeDuration;
    ptypeTimer->state = softTimerOpen;

    return 0;
}

int8_t cSoftTimerReload(SoftTimerTypeDef *ptypeTimer)
{
    if(ptypeTimer == NULL)
        return -1;

    ptypeTimer->timeStop += ptypeTimer->timeDuration;
    ptypeTimer->state = softTimerOpen;

    return 0;
}

int8_t cSoftTimerOpen(SoftTimerTypeDef *ptypeTimer)
{
    if(ptypeTimer == NULL)
        return -1;

    ptypeTimer->state = softTimerOpen;

    return 0;
}

int8_t cSoftTimerClose(SoftTimerTypeDef *ptypeTimer)
{
    if(ptypeTimer == NULL)
        return -1;

    ptypeTimer->state = softTimerClose;

    return 0;
}

int8_t cSoftTimerSetState(SoftTimerTypeDef *ptypeTimer, SoftTimerStateEnum enumState)
{
    if(ptypeTimer == NULL)
        return -1;

    ptypeTimer->state = enumState;

    /* 设置为本次立即触发 */
    if(enumState & softTimerOver)
        ptypeTimer->timeStop = lSoftTimerGetNow();

    return 0;
}

SoftTimerStateEnum enumSoftTimerGetState(SoftTimerTypeDef *ptypeTimer)
{
    if(ptypeTimer == NULL)
        return softTimerError;

    if((ptypeTimer->state & softTimerOpen) == 0)
        return softTimerClose;

    if(lSoftTimerGetNow() >= ptypeTimer->timeStop)
        return softTimerOver;

    return softTimerOpen;
}
