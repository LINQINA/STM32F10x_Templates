#include "stm32f1xx.h"
#include "stdint.h"
#include "stdio.h"

#include "DevicesTime.h"

/* 系统时基,不可手动更改 */
volatile int64_t g_iTimeBase = 0;
/* 实时时钟,可以更改 */
volatile int64_t g_lTimestamp = 0;
/* 时区: 默认东八区 */
volatile float g_fRealTimeUTC = 8.0f;

/* 每个月的天数 */
const static uint8_t st_ucMonthDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

/* 获取当前系统时基 */
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

/* 蔡勒公式计算星期几 Christian Zeller */
uint8_t cTimeToWeek(int32_t iYear, uint8_t ucMonth, uint8_t ucDay)
{
    int32_t iCentury = 0;
    int8_t cWeek = 0;

    /* 如果是1、2月份，则需要把月份当作是上年度的13、14月份 */
    if(ucMonth < 3)
    {
        iYear -= 1;
        ucMonth += 12;
    }

    iCentury = iYear / 100;
    iYear %= 100;

    cWeek = ((iCentury / 4) - (iCentury * 2) + iYear + (iYear / 4) + (13 * (ucMonth + 1) / 5) + ucDay - 1) % 7;

    return ((cWeek < 0) ? (cWeek + 7) : cWeek);
}

/*
* Return:      void
* Parameters:  lStamp: UNIX时间戳; ptypeTime:时间结构体指针; cUTC: 时区（东时区为正、西时区为负）
* Description: 把UNIX时间戳转换成时间结构体
*/
void vStampToTime(int64_t lStamp, TimeInfoType *ptypeTime, float fUTC)
{
    int32_t iYear, iStamp;
    int8_t cMonth;

    if(ptypeTime == NULL)
        return;

    /* 加入时区值 */
    lStamp += (int64_t)(fUTC * 3600.0f);
    ptypeTime->UTC = fUTC;

    if(lStamp >= 0)
    {
        /* 计算全部年份的整年天数（1天有86400秒） */
        for(iYear = 1969; lStamp >= 0; lStamp -= DAYS_OF_THE_YEAR(iYear) * 86400)
            ++iYear;

        /* 计算当前年份下全部月份的整月天数 */
        for(cMonth = 0, lStamp += DAYS_OF_THE_YEAR(iYear) * 86400; lStamp >= 0; lStamp -= DAYS_OF_THE_MONTH(iYear, cMonth) * 86400)
            ++cMonth;

        /* 加上上面循环多减去的1个月 */
        lStamp += DAYS_OF_THE_MONTH(iYear, cMonth) * 86400;
    }
    else
    {
        /* 计算全部年份的整年天数（1天有86400秒） */
        for(iYear = 1970; lStamp < 0; lStamp += DAYS_OF_THE_YEAR(iYear) * 86400)
            --iYear;

        /* 计算当前年份下全部月份的整月天数 */
        for(cMonth = 13, lStamp -= DAYS_OF_THE_YEAR(iYear) * 86400; lStamp < 0; lStamp += DAYS_OF_THE_MONTH(iYear, cMonth) * 86400)
            --cMonth;
    }

    iStamp = lStamp;
    ptypeTime->year = iYear;
    ptypeTime->month = cMonth;
    ptypeTime->day = iStamp / 86400 + 1;

    iStamp = (iStamp % 86400) + 86400;
    ptypeTime->hour = iStamp / 3600 % 24;
    ptypeTime->minute = iStamp / 60 % 60;
    ptypeTime->second = iStamp % 60;
    ptypeTime->week = cTimeToWeek(ptypeTime->year, ptypeTime->month, ptypeTime->day);
}

/*
* Return:      UNIX时间戳
* Parameters:  ptypeTime:时间结构体;
* Description: 把时间结构体转换成UNIX时间戳
*/
int64_t lTimeToStamp(TimeInfoType *ptypeTime)
{
    int64_t lDays = 0, lStamp;
    int32_t iYear;
    int8_t cMonth;

    if(ptypeTime == NULL)
        return 0LL;

    if(ptypeTime->year >= 1970)
    {
        for(iYear = 1970; iYear < ptypeTime->year; ++iYear)
            lDays += DAYS_OF_THE_YEAR(iYear);
    }
    else
    {
        for(iYear = 1969; iYear >= ptypeTime->year; --iYear)
            lDays -= DAYS_OF_THE_YEAR(iYear);
    }

    for(cMonth = 1; cMonth < ptypeTime->month; ++cMonth)
        lDays += DAYS_OF_THE_MONTH(ptypeTime->year, cMonth);

    lDays += ptypeTime->day - 1;

    lStamp  = lDays * 86400LL;
    lStamp += ptypeTime->hour * 3600;
    lStamp += ptypeTime->minute * 60;
    lStamp += ptypeTime->second;

    /* 加入计算时区值 */
    lStamp -= (int64_t)(ptypeTime->UTC * 3600.0f);

    return lStamp;
}

/* 设置时间戳：单位us */
void vTimestampSet(int64_t lUNIXTimeStamp)
{
    g_lTimestamp = lUNIXTimeStamp - lTimeGetStamp();
}

/* 获取时间戳：单位us */
int64_t lTimestampGet(void)
{
    return g_lTimestamp + lTimeGetStamp();
}

/* 时区设置 */
void vRealTimeUTCSet(float fUTC)
{
    g_fRealTimeUTC = fUTC;
}

/* 时区获取 */
float fRealTimeUTCGet(void)
{
    return g_fRealTimeUTC;
}
