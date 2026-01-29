#ifndef _DevicesTime_H_
#define _DevicesTime_H_

/* 判断是否为闰年 */
#define YEAR_LEAP(year) ((((year) % 4 == 0) && ((year) % 100 != 0)) || ((year) % 400 == 0))

/* 获取年份天数 */
#define DAYS_OF_THE_YEAR(year) (YEAR_LEAP(year) !=0 ? 366 : 365)
/* 获取月份天数 */
#define DAYS_OF_THE_MONTH(year, month) ((((month) == 2) && (YEAR_LEAP(year) != 0)) ? 29 : st_ucMonthDays[(month) - 1])

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t week;
    float UTC;
} TimeInfoType;

/* 系统时基,不可手动更改 */
extern volatile int64_t g_iTimeBase;

/* 获取当前系统时基 */
int64_t lTimeGetStamp(void);
/* 蔡勒公式计算星期几 */
uint8_t cTimeToWeek(int32_t iYear, uint8_t ucMonth, uint8_t ucDay);
/* 把UNIX时间戳转换成时间结构体 */
void vStampToTime(int64_t lStamp, TimeInfoType *ptypeTime, float fUTC);
/* 把时间结构体转换成UNIX时间戳 */
int64_t lTimeToStamp(TimeInfoType *ptypeTime);
/* 设置时间戳：单位us */
void vTimestampSet(int64_t lUNIXTimeStamp);
/* 获取时间戳：单位us */
int64_t lTimestampGet(void);
/* 时区设置 */
void vRealTimeUTCSet(float fUTC);
/* 时区获取 */
float fRealTimeUTCGet(void);

#endif
