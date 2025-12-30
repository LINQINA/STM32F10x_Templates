#ifndef _DevicesKey_H_
#define _DevicesKey_H_

#define KEY_0_GPIO_Port         GPIOE
#define KEY_0_Pin               GPIO_PIN_4

#define KEY_1_GPIO_Port         GPIOE
#define KEY_1_Pin               GPIO_PIN_3

#define KEY_UP_GPIO_Port        GPIOA
#define KEY_UP_Pin              GPIO_PIN_0


#define KEY_0   0x0001
#define KEY_1   0x0002
#define KEY_UP  0x0004

/* 持续按键启动时间ms */
#define KEY_CONTINUE_TIME               5000
/* 长按键时间ms */
#define KEY_LONG_TIME                   2000
/* 短按键时间ms */
#define KEY_SHORT_TIME                  50


typedef enum {
    keyNormal = 0,

    keyEqual        = 0x0001,   /* 当前按键值与上次按键值相同 */
    keyAdd          = 0x0002,   /* 增加了新按键 */
    keyCut          = 0x0004,   /* 减少了新按键 */
    keyAddAndCut    = 0x0008,   /* 即减少了旧按键并且又增加了新按键 */

    keyShort        = 0x0100,   /* 短按 */
    keyLong         = 0x0200,   /* 长按 */
    keyContinuous   = 0x0400,   /* 持续按 */
}KeyStateEnum;

typedef struct
{
    uint32_t valueLast;                 /* 当前状态的按下键值 */

    uint32_t valuePress;                /* 新增的按下键值 */
    uint32_t timePress;                 /* 最后一次按下的时刻 */

    uint32_t valueLoosen;               /* 新增的松开键值 */
    uint32_t timeLoosen;                /* 最后一次松开的时刻 */

    KeyStateEnum state;                 /* 状态 */
    uint32_t (*uiKeyValueGet)(void);    /* 按键键值读取函数 */
}KeyTypeDef;

extern KeyTypeDef g_typeKeyData;

void vKeyInit(void);
uint32_t uiKeyValueGet(void);
KeyStateEnum enumKeyStateMachine(KeyTypeDef *ptypeKeyMachine);



#endif
