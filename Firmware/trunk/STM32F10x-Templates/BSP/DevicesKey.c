#include "stm32f1xx.h"
#include "stdint.h"
#include "stdio.h"

#include "DevicesTime.h"
#include "DevicesKey.h"


static KeyStateEnum enumKeyStateGet(uint32_t uiNewValue, uint32_t uiOldValue);
static KeyStateEnum enumKeyTimeStateGet(KeyTypeDef *ptypeKeyMachine);


KeyTypeDef g_typeKeyData = {0};

void vKeyInit(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    
    /* KEY_0 引脚 */
    GPIO_InitStruct.Pin   = KEY_0_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull  = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(KEY_0_GPIO_Port,&GPIO_InitStruct);

    /* KEY_1 引脚 */
    GPIO_InitStruct.Pin   = KEY_1_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull  = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(KEY_1_GPIO_Port,&GPIO_InitStruct);
    
    /* KEY_UP 引脚 */
    GPIO_InitStruct.Pin   = KEY_UP_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull  = GPIO_PULLDOWN;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(KEY_UP_GPIO_Port,&GPIO_InitStruct);

    g_typeKeyData.uiKeyValueGet = uiKeyValueGet;
}

/*
Return Value:   按键组合序号（头文件里有定义）
Parameters:     void
Description:    读取当前按键状态,读取到默认电平,清空状态
*/
uint32_t uiKeyValueGet(void)
{
    uint32_t Key = KEY_0 | KEY_1 | KEY_UP;
    uint8_t i = 0;

    for(i = 0; i < 8; ++i)
    {
        if(RESET != HAL_GPIO_ReadPin(KEY_0_GPIO_Port, KEY_0_Pin))
            Key &= ~KEY_0;
        if(RESET != HAL_GPIO_ReadPin(KEY_1_GPIO_Port, KEY_1_Pin))
            Key &= ~KEY_1;
        if(  SET != HAL_GPIO_ReadPin(KEY_UP_GPIO_Port, KEY_UP_Pin))
            Key &= ~KEY_UP;
    }

    return Key;
}

/*
 * Return:      按键状态
 * Parameters:  Value_new:新按键键值; Value_old:旧按键键值;
 * Description: 按键状态获取
 */
static KeyStateEnum enumKeyStateGet(uint32_t uiNewValue, uint32_t uiOldValue)
{
    if(uiNewValue == uiOldValue)                        /* 当前按键值与上次按键值相同 */
        return keyEqual;
    else if((uiNewValue & uiOldValue) == uiOldValue)    /* 当前按键值在上次按键值基础上增加了新按键 */
        return keyAdd;
    else if((uiNewValue | uiOldValue) == uiOldValue)    /* 当前按键值在上次按键值基础上减少了旧按键 */
        return keyCut;
    else                                                /* 当前按键值在上次按键值基础上即减少了旧按键又增加了新按键 */
        return keyAddAndCut;
}

/*
 * Return:      KeyStateEnum：按键状态机的状态
 * Parameters:  ptypeKeyMachine: 按键结构消息
 * Description: 按键状态获取
 */
static KeyStateEnum enumKeyTimeStateGet(KeyTypeDef *ptypeKeyMachine)
{
    int32_t lTimeNow = 0;

    lTimeNow = (int32_t)(lTimeGetStamp() / 1000ll);

    switch((uint32_t)ptypeKeyMachine->state)
    {
        case keyAdd:
            if((lTimeNow - ptypeKeyMachine->timePress) >= KEY_SHORT_TIME)
            {
                ptypeKeyMachine->state &= 0x00FF;
                ptypeKeyMachine->state |= keyShort;
                return ptypeKeyMachine->state;
            }
            break;

        case keyShort | keyAdd:
            if((lTimeNow - ptypeKeyMachine->timePress) >= KEY_LONG_TIME)
            {
                ptypeKeyMachine->state &= 0x00FF;
                ptypeKeyMachine->state |= keyLong;
                return ptypeKeyMachine->state;
            }
            break;

        case keyLong | keyAdd:
            if((lTimeNow - ptypeKeyMachine->timePress) >= KEY_CONTINUE_TIME)
            {
                ptypeKeyMachine->state &= 0x00FF;
                ptypeKeyMachine->state |= keyContinuous;
                return ptypeKeyMachine->state;
            }
            break;

        case keyContinuous | keyAdd:
            break;

        default : break;
    }

    return keyNormal;
}

/*
 * Return:      KeyStateEnum：按键状态机的状态
 * Parameters:  ptypeKeyMachine：按键结构消息
 * Description: 按键状态机信息获取
 */
KeyStateEnum enumKeyStateMachine(KeyTypeDef *ptypeKeyMachine)
{
    uint32_t uiNewValue = 0, uiTimeNow = 0;
    KeyStateEnum enumState;

    if((ptypeKeyMachine == NULL) || (ptypeKeyMachine->uiKeyValueGet == NULL))
        return keyNormal;

    /* 键值获取 */
    if(((uiNewValue = ptypeKeyMachine->uiKeyValueGet()) != 0) || (ptypeKeyMachine->valueLast != 0))
    {
        /* 按键状态判断 */
        enumState = enumKeyStateGet(uiNewValue, ptypeKeyMachine->valueLast);

        uiTimeNow = (uint32_t)(lTimeGetStamp() / 1000ll);

        /* 按键状态转换 */
        switch(enumState)
        {
            /* 记录按键按下时间、按下总键值，松开按键键值清零 */
            case keyAdd:
                ptypeKeyMachine->valuePress = uiNewValue;
                ptypeKeyMachine->timePress = uiTimeNow;
                ptypeKeyMachine->valueLoosen = 0;
                ptypeKeyMachine->state = keyAdd;
                ptypeKeyMachine->valueLast = uiNewValue;
                break;

            /* 记录按键松开时间、松开总键值 */
            case keyCut:
                ptypeKeyMachine->valueLoosen |= ptypeKeyMachine->valueLast & (~uiNewValue);
                ptypeKeyMachine->timeLoosen = uiTimeNow;
                ptypeKeyMachine->state &= ~0x00FF;
                ptypeKeyMachine->state |= keyCut;
                ptypeKeyMachine->valueLast = uiNewValue;
                return ptypeKeyMachine->state;

            /* 记录按键按下时间、按下总键值、松开时间、松开总键值 */
            case keyAddAndCut:
                ptypeKeyMachine->valuePress = uiNewValue;
                ptypeKeyMachine->timePress = uiTimeNow;
                ptypeKeyMachine->valueLoosen |= ptypeKeyMachine->valueLast & (~uiNewValue);
                ptypeKeyMachine->timeLoosen = uiTimeNow;
                ptypeKeyMachine->valueLast = uiNewValue;
                break;

            case keyEqual:
                return enumKeyTimeStateGet(ptypeKeyMachine);

            default: break;
        }
    }

    return keyNormal;
}
