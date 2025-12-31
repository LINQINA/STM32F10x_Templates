#ifndef _DevicesBeep_H_
#define _DevicesBeep_H_


#define BEEP_CHANNEL1_GPIO_Port     GPIOB
#define BEEP_CHANNEL1_Pin           GPIO_PIN_8




typedef enum {
    BEEP_CHANNEL1       = 0x01, /* 通道1 */
    BEEP_CHANNEL_ALL    = 0xFF, /* 全部通道 */
} BeepChannelEnum;

typedef enum {
    BEEP_MODE_QUIET = 0,    /* 静音模式 */
    BEEP_MODE_NORMAL,       /* 正常模式 */
} BeepModeEnum;

typedef enum {
    BEEP_IDLE                    = 0,
    BEEP_DISABLE,                /* 关闭 */
    BEEP_ENABLE,                 /* 常亮 */
    BEEP_FLASH_SLOW,             /* 循环慢闪（0.5Hz） */
    BEEP_FLASH_SLOW_ENABLE_CNT,  /* 慢闪N次后BEEP常响 */
    BEEP_FLASH_SLOW_DISABLE_CNT, /* 慢闪N次后BEEP关闭 */
    BEEP_FLASH_FAST,             /* 循环快闪（1Hz） */
    BEEP_FLASH_FAST_ENABLE_CNT,  /* 快闪N次后BEEP常响 */
    BEEP_FLASH_FAST_DISABLE_CNT, /* 快闪N次后BEEP关闭 */
} BeepStateEnum;


typedef struct{
    BeepModeEnum mode;      /* 模式：0（静音模式）、1（正常模式） */
    BeepStateEnum state;    /* 状态 */
    int8_t flashCnt;        /* 发声次数 */
}BeepType;


typedef struct{
    BeepType channel1;
}BeepInfoType;



void vBeepInit(void);
void vBeepMachine(void);
BeepInfoType *ptypeBeepInfoGet(void);

void vBeepStatusSet(BeepChannelEnum usChannel, BeepStateEnum enumStatus, uint8_t ucFlashCnt);
void vBeepModeSet(BeepChannelEnum usChannel, BeepModeEnum enumMode);

#define vBeepSoundFast(cCnt) vBeepStatusSet(BEEP_CHANNEL_ALL, BEEP_FLASH_FAST_DISABLE_CNT, cCnt)
#define vBeepSoundSlow(cCnt) vBeepStatusSet(BEEP_CHANNEL_ALL, BEEP_FLASH_SLOW_DISABLE_CNT, cCnt)


#endif
