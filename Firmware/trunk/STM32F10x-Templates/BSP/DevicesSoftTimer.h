#ifndef _DevicesSoftTimer_H_
#define _DevicesSoftTimer_H_


#include "stdint.h"


typedef enum {
    softTimerClose = 0x00,     /* 定时器已关闭 */
    softTimerOpen  = 0x01,     /* 正常计时当中 */
    softTimerOver  = 0x02,     /* 定时器溢出 */
    softTimerError = 0x08,     /* 定时器错误 */
}SoftTimerStateEnum;


typedef struct
{
    int64_t timeStop;           /* 定时结束时刻（us） */
    int64_t timeDuration;       /* 定时时长（us） */
    SoftTimerStateEnum state;   /* 状态 */
}SoftTimerTypeDef;



/* 基础定时 功能 */
#define cSoftTimerSetUs(ptypeTimer, lTime, state)       cSoftTimerSet((ptypeTimer), (lTime), (state))
#define cSoftTimerSetMs(ptypeTimer, lTime, state)       cSoftTimerSet((ptypeTimer), (lTime) * 1000ll, (state))
#define cSoftTimerSetSecond(ptypeTimer, lTime, state)   cSoftTimerSet((ptypeTimer), (lTime) * 1000000ll, (state))
#define cSoftTimerSetMinute(ptypeTimer, lTime, state)   cSoftTimerSet((ptypeTimer), (lTime) * (1000000ll * 60), (state))
#define cSoftTimerSetHour(ptypeTimer, lTime, state)     cSoftTimerSet((ptypeTimer), (lTime) * (1000000ll * 60 * 60), (state))
int8_t cSoftTimerSet(SoftTimerTypeDef *ptypeTimer, int64_t lTime, SoftTimerStateEnum state);

int8_t cSoftTimerReset(SoftTimerTypeDef *ptypeTimer);
int8_t cSoftTimerReload(SoftTimerTypeDef *ptypeTimer);
int8_t cSoftTimerOpen(SoftTimerTypeDef *ptypeTimer);
int8_t cSoftTimerClose(SoftTimerTypeDef *ptypeTimer);
int8_t cSoftTimerSetState(SoftTimerTypeDef *ptypeTimer, SoftTimerStateEnum enumState);
SoftTimerStateEnum enumSoftTimerGetState(SoftTimerTypeDef *ptypeTimer);


#endif
