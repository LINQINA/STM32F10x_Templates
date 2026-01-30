#include "stm32f1xx.h"
#include "stdint.h"

#include "DevicesTime.h"
#include "DevicesRTC.h"

/* RTC 句柄 */
RTC_HandleTypeDef hrtc;

/* RTC 初始化 */
void vRTCInit(void)
{
    /* 首次上电，设置默认时间 */
    TimeInfoType typeTimeInfo = {0};

    /* 使能 PWR 和 BKP 时钟 */
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_RCC_BKP_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();
    
    /* 初始化 RTC（会自动调用 HAL_RTC_MspInit 配置时钟） */
    hrtc.Instance = RTC;
    hrtc.Init.AsynchPrediv = RTC_AUTO_1_SECOND;
    hrtc.Init.OutPut = RTC_OUTPUTSOURCE_NONE;
    HAL_RTC_Init(&hrtc);
    
    /* 初始化完成后，再检查是否需要设置默认时间 */
    if(HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR1) != RTC_INIT_FLAG)
    {
        typeTimeInfo.year   = 2026;
        typeTimeInfo.month  = 1;
        typeTimeInfo.day    = 29;
        typeTimeInfo.hour   = 20;
        typeTimeInfo.minute = 0;
        typeTimeInfo.second = 0;
        typeTimeInfo.UTC    = 8.0f;
        /* 首次上电，设置默认时间 */
        vRTCSetTimeByStruct(&typeTimeInfo);
        /* 写入标志位 */
        HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR1, RTC_INIT_FLAG);
    }
    
    HAL_NVIC_SetPriority(RTC_IRQn,2,0);
    HAL_NVIC_EnableIRQ(RTC_IRQn);
}

/* HAL_RTC_MspInit 回调（时钟配置） */
void HAL_RTC_MspInit(RTC_HandleTypeDef *hrtc)
{
    RCC_OscInitTypeDef rcc_oscinitstruct = {0};
    RCC_PeriphCLKInitTypeDef rcc_periphclkinitstruct = {0};

    /* 配置 LSE */
    rcc_oscinitstruct.OscillatorType = RCC_OSCILLATORTYPE_LSE;
    rcc_oscinitstruct.LSEState = RCC_LSE_ON;
    rcc_oscinitstruct.PLL.PLLState = RCC_PLL_NONE;
    HAL_RCC_OscConfig(&rcc_oscinitstruct);

    /* 选择 LSE 作为 RTC 时钟源 */
    rcc_periphclkinitstruct.PeriphClockSelection = RCC_PERIPHCLK_RTC;
    rcc_periphclkinitstruct.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
    HAL_RCCEx_PeriphCLKConfig(&rcc_periphclkinitstruct);

    /* 使能 RTC 时钟 */
    __HAL_RCC_RTC_ENABLE();
}

/* 设置 RTC 时间（UNIX 时间戳，单位秒） */
void vRTCSetTime(int64_t lUnixTime)
{
    uint32_t uiCounter = (uint32_t)lUnixTime;
    
    HAL_RTC_WaitForSynchro(&hrtc);
    
    /* STM32F1 的 HAL 库直接写计数器 */
    while((hrtc.Instance->CRL & RTC_CRL_RTOFF) == 0);
    hrtc.Instance->CRL |= RTC_CRL_CNF;
    
    hrtc.Instance->CNTH = (uiCounter >> 16) & 0xFFFF;
    hrtc.Instance->CNTL = uiCounter & 0xFFFF;
    
    hrtc.Instance->CRL &= ~RTC_CRL_CNF;
    while((hrtc.Instance->CRL & RTC_CRL_RTOFF) == 0);
}

/* 获取 RTC 时间（UNIX 时间戳，单位秒） */
int64_t lRTCGetTime(void)
{
    volatile uint32_t uiHigh1, uiHigh2, uiLow;
    
    /* 防止读取时计数器进位 */
    do
    {
        uiHigh1 = hrtc.Instance->CNTH;
        uiLow = hrtc.Instance->CNTL;
        uiHigh2 = hrtc.Instance->CNTH;
    } while(uiHigh1 != uiHigh2);
    
    return (int64_t)((uiHigh1 << 16) | uiLow);
}

/* 设置 RTC 时间（通过时间结构体） */
void vRTCSetTimeByStruct(TimeInfoType *ptypeTime)
{
    int64_t lStamp;
    
    if(ptypeTime == NULL)
        return;
    
    lStamp = lTimeToStamp(ptypeTime);
    vRTCSetTime(lStamp);
}

/* 获取 RTC 时间（通过时间结构体） */
void vRTCGetTimeByStruct(TimeInfoType *ptypeTime, float fUTC)
{
    int64_t lStamp;
    
    if(ptypeTime == NULL)
        return;
    
    lStamp = lRTCGetTime();
    vStampToTime(lStamp, ptypeTime, fUTC);
}

/* 设置 RTC 闹钟 (UNIX 时间戳，单位秒) */
void vRTCSetAlarm(int64_t lAlarmTime)
{
    uint32_t uiAlarmCounter = (uint32_t)lAlarmTime;
    
    HAL_RTC_WaitForSynchro(&hrtc);
    
    /* 进入配置模式 */
    while((hrtc.Instance->CRL & RTC_CRL_RTOFF) == 0);
    hrtc.Instance->CRL |= RTC_CRL_CNF;
    
    /* 设置闹钟寄存器 */
    hrtc.Instance->ALRH = (uiAlarmCounter >> 16) & 0xFFFF;
    hrtc.Instance->ALRL = uiAlarmCounter & 0xFFFF;
    
    /* 退出配置模式 */
    hrtc.Instance->CRL &= ~RTC_CRL_CNF;
    while((hrtc.Instance->CRL & RTC_CRL_RTOFF) == 0);
    
    /* 清除闹钟标志 */
    __HAL_RTC_ALARM_CLEAR_FLAG(&hrtc, RTC_FLAG_ALRAF);
    
    /* 使能闹钟中断 */
    __HAL_RTC_ALARM_ENABLE_IT(&hrtc, RTC_IT_ALRA);
}

/* 设置 RTC 闹钟 (通过时间结构体) */
void vRTCSetAlarmByStruct(TimeInfoType *ptypeTime)
{
    int64_t lStamp;
    
    if(ptypeTime == NULL)
        return;
    
    lStamp = lTimeToStamp(ptypeTime);
    vRTCSetAlarm(lStamp);
}

/* 获取 RTC 闹钟设置 (UNIX 时间戳，单位秒) */
int64_t lRTCGetAlarm(void)
{
    uint32_t uiHigh, uiLow;
    
    uiHigh = hrtc.Instance->ALRH;
    uiLow = hrtc.Instance->ALRL;
    
    return (int64_t)((uiHigh << 16) | uiLow);
}

/* 取消 RTC 闹钟 */
void vRTCCancelAlarm(void)
{
    __HAL_RTC_ALARM_DISABLE_IT(&hrtc, RTC_IT_ALRA);
    __HAL_RTC_ALARM_CLEAR_FLAG(&hrtc, RTC_FLAG_ALRAF);
}
