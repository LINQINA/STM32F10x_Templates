#ifndef _DevicesRTC_H_
#define _DevicesRTC_H_

#include "stdint.h"
#include "stm32f1xx.h"
#include "DevicesTime.h"

extern RTC_HandleTypeDef hrtc;

/* RTC 初始化标志 */
#define RTC_INIT_FLAG       0x5A5A

/* RTC 初始化 */
void vRTCInit(void);

/* 设置 RTC 【时间】 (UNIX 时间戳 单位秒) */
void vRTCSetTime(int64_t lUnixTime);

/* 获取 RTC 【时间】 (UNIX 时间戳 单位秒) */
int64_t lRTCGetTime(void);

/* 设置 RTC 【时间】 (通过时间结构体) */
void vRTCSetTimeByStruct(TimeInfoType *ptypeTime);

/* 获取 RTC 【时间】 (通过时间结构体) */
void vRTCGetTimeByStruct(TimeInfoType *ptypeTime, float fUTC);

/* 设置 RTC 【闹钟】 (UNIX 时间戳 单位秒) */
void vRTCSetAlarm(int64_t lAlarmTime);

/* 设置 RTC 【闹钟】 (通过时间结构体) */
void vRTCSetAlarmByStruct(TimeInfoType *ptypeTime);

/* 获取 RTC 【闹钟】设置 (UNIX 时间戳 单位秒) */
int64_t lRTCGetAlarm(void);

/* 取消 RTC 【闹钟】 */
void vRTCCancelAlarm(void);

#endif
