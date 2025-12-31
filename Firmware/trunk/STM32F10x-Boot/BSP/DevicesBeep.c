#include "stm32f1xx_hal.h"
#include "DevicesBeep.h"

volatile static uint32_t st_uiBeepTickCnt = 0;

static BeepInfoType st_typeBeepInfo;

void vBeepInit(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitStruct.Pin = BEEP_CHANNEL1_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(BEEP_CHANNEL1_GPIO_Port, &GPIO_InitStruct);

    /* 初始化后关闭全部蜂鸣器 */
    vBeepStatusSet(BEEP_CHANNEL_ALL, BEEP_FLASH_SLOW_DISABLE_CNT, 1);
    vBeepModeSet(BEEP_CHANNEL_ALL, BEEP_MODE_NORMAL);
}

static void vBeepOpen(uint16_t usChannel)
{
    if(usChannel & BEEP_CHANNEL1)
    {
        HAL_GPIO_WritePin(BEEP_CHANNEL1_GPIO_Port, BEEP_CHANNEL1_Pin, GPIO_PIN_SET);
    }
}

static void vBeepClose(uint16_t usChannel)
{
    if(usChannel & BEEP_CHANNEL1)
    {
        HAL_GPIO_WritePin(BEEP_CHANNEL1_GPIO_Port, BEEP_CHANNEL1_Pin, GPIO_PIN_RESET);
    }
}

/*
 * Return:      void
 * Parameters:  usChannel: 通道; enumStatus: 状态; ucFlashCnt: 次数;
 * Description: 设置状态
 */
void vBeepStatusSet(BeepChannelEnum usChannel, BeepStateEnum enumStatus, uint8_t ucFlashCnt)
{
    if(usChannel & BEEP_CHANNEL1)
    {
        st_typeBeepInfo.channel1.state = enumStatus;
        st_typeBeepInfo.channel1.flashCnt = ucFlashCnt * 2 - 1;
    }

    /* 重新开始计时，以达到完整的声音周期 */
    st_uiBeepTickCnt = 0;
}

/*
 * Return:      void
 * Parameters:  usChannel: 通道; enumMode: 模式;
 * Description: 设置模式
 */
void vBeepModeSet(BeepChannelEnum usChannel, BeepModeEnum enumMode)
{
    if(usChannel & BEEP_CHANNEL1)
    {
        st_typeBeepInfo.channel1.mode = enumMode;
    }
}

/*
 * Return:      void
 * Parameters:  *ptypeBEEP: BEEP信息结构体
 * Description: BEEP状态刷新
 */
static void vBeepStateMachine(BeepType *ptypeBEEP)
{
    switch(ptypeBEEP->state)
    {
        /* 关闭 */
        case BEEP_DISABLE:
            vBeepClose(BEEP_CHANNEL1);
            ptypeBEEP->state = BEEP_IDLE;
            break;

        /* 常响 */
        case BEEP_ENABLE:
            vBeepOpen(BEEP_CHANNEL1);
            ptypeBEEP->state = BEEP_IDLE;
            break;

        /* 慢响/慢响后关闭/慢响后常响 */
        case BEEP_FLASH_SLOW:
        case BEEP_FLASH_SLOW_DISABLE_CNT:
        case BEEP_FLASH_SLOW_ENABLE_CNT:
            if((st_uiBeepTickCnt % 5) == 0)
            {
                ((st_uiBeepTickCnt / 5) & 1) ? vBeepOpen(BEEP_CHANNEL1) : vBeepClose(BEEP_CHANNEL1);

                if((ptypeBEEP->state != BEEP_FLASH_SLOW) && ((ptypeBEEP->flashCnt--) <= 0))
                {
                    ptypeBEEP->state = (ptypeBEEP->state == BEEP_FLASH_SLOW_ENABLE_CNT) ? BEEP_ENABLE : BEEP_DISABLE;
                }
            }
            break;

        /* 快响/快响后关闭/快响后常响 */
        case BEEP_FLASH_FAST:
        case BEEP_FLASH_FAST_DISABLE_CNT:
        case BEEP_FLASH_FAST_ENABLE_CNT:
            (st_uiBeepTickCnt & 1) ? vBeepOpen(BEEP_CHANNEL1) : vBeepClose(BEEP_CHANNEL1);

            if((ptypeBEEP->state != BEEP_FLASH_FAST) && ((ptypeBEEP->flashCnt--) <= 0))
            {
                ptypeBEEP->state = (ptypeBEEP->state == BEEP_FLASH_FAST_ENABLE_CNT) ? BEEP_ENABLE : BEEP_DISABLE;
            }
            break;

        default : break;
    }
}

/* 100ms调用一次 */
void vBeepMachine(void)
{
    ++st_uiBeepTickCnt;

    /* 非静音模式 */
    if(st_typeBeepInfo.channel1.mode == BEEP_MODE_NORMAL)
    {
        vBeepStateMachine(&st_typeBeepInfo.channel1);
    }
    /* 静音模式 */
    else
    {
        vBeepClose(BEEP_CHANNEL1);
    }
}

BeepInfoType *ptypeBeepInfoGet(void)
{
    return &st_typeBeepInfo;
}
