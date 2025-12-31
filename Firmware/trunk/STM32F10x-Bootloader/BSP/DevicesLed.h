#ifndef _DevicesLed_H_
#define _DevicesLed_H_

#include "stm32f1xx_hal.h"

/* LED调频频率 */
#define LED_FREQUENCY               2048
/* LED溢出值 */
#define LED_PERIOD                  (SystemCoreClock / LED_FREQUENCY)

/* LED状态机执行周期（ms） */
#define LED_EXECUTION_PERIOD        20
/* 快闪周期（ms） */
#define LED_FLASH_FAST_PERIOD       800
/* 慢闪周期（ms） */
#define LED_FLASH_SLOW_PERIOD       2400
/* SOS模式，周期间隔时间（ms） */
#define LED_SOS_DELAY_PERIOD        2000

/* 驱动模式 */
#define LED_DRIVE_NULL              0
#define LED_DRIVE_IO                1
#define LED_DRIVE_PWM               2
/* 亮度等级 */
#define LED_HIGH_DUTY               1.0f
#define LED_LOW_DUTY                0.5f



#define LED_RED_GPIO_Port           GPIOB
#define LED_RED_Pin                 GPIO_PIN_5

#define LED_GREEN_GPIO_Port         GPIOE
#define LED_GREEN_Pin               GPIO_PIN_5

typedef enum {
    LED_CHANNEL_RED                 = 0x01,                 /* 红灯LED */
    LED_CHANNEL_GREEN               = 0x02,                 /* 绿灯LED */

    LED_CHANNEL_ALL                 = 0x7FFFFFFF,
} LedChannelEnum;

typedef enum {
    LED_DISABLE                     = 0,                    /* 关闭 */
    LED_ENABLE                      = 1,                    /* 常亮 - 高亮 */
    LED_ENABLE_LOW                  = 3,                    /* 常亮 - 低亮 */
    LED_DUTY                        = 4,                    /* 固定占空比 */

    LED_FLASH_FAST                  = 10,                   /* 快速闪烁 */
    LED_FLASH_FAST_ENABLE_CNT       = 11,                   /* 快速闪烁N次后 常亮 */
    LED_FLASH_FAST_DISABLE_CNT      = 12,                   /* 快速闪烁N次后 常灭 */

    LED_FLASH_SLOW                  = 20,                   /* 慢速闪烁 */
    LED_FLASH_SLOW_ENABLE_CNT       = 21,                   /* 慢速闪烁N次后 常亮 */
    LED_FLASH_SLOW_DISABLE_CNT      = 22,                   /* 慢速闪烁N次后 关闭 */

    LED_BREATHE                     = 30,                   /* 呼吸 */

    LED_FLASH_SOS                   = 40,                   /* SOS */

    LED_IDLE                        = 0xFF,
} LedStateEnum;

typedef struct{
    LedChannelEnum ledChannel;  /* LED通道类型 */
    LedStateEnum state;         /* 状态 */
    int8_t flashCnt;            /* 闪烁次数 */
    int8_t duty;                /* 占空比 */

    uint8_t driveMode;          /* 驱动模式，0: 普通IO; 1: PWM */
    TIM_HandleTypeDef *htim;    /* 定时器 */
    uint32_t channel;           /* 通道 */
}LedType;

typedef struct{
    LedType red;                /* 红灯LED */
    LedType green;              /* 绿灯LED */
}LedInfoType;

void vLedInit(void);
void vLedOpen(uint32_t uiChannel);
void vLedClose(uint32_t uiChannel);
void vLedRevesal(uint32_t uiChannel);

void vLedMachine(void);
void vLedSetStatus(LedChannelEnum usChannel, LedStateEnum enumStatus, uint8_t ucFlashCnt_or_Duty);
#define vLedSetStatusFlashFast(usChannel) vLedSetStatus((usChannel), LED_FLASH_FAST, 0)
#define vLedSetStatusFlashSlow(usChannel) vLedSetStatus((usChannel), LED_FLASH_SLOW, 0)
#define vLedSetStatusBreathe(usChannel)   vLedSetStatus((usChannel), LED_BREATHE, 0)
#define vLedSetStatusDisable(usChannel)   vLedSetStatus((usChannel), LED_DISABLE, 0)
LedInfoType *ptypeLedGetInfo(void);


#endif
