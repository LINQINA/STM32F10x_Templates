#include "stm32f1xx_hal.h"

#include "DevicesTimer.h"
#include "DevicesLed.h"

static void vLedPWMInit(void);

static LedInfoType st_typeLedInfo;

volatile static uint32_t st_uiLedTickCnt = 0;
volatile static uint32_t st_uiLedSosCnt = 0;
volatile static int16_t st_sGrade = 0;

void vLedInit(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    __HAL_AFIO_REMAP_TIM3_PARTIAL();

    GPIO_InitStruct.Pin = LED_RED_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(LED_RED_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = LED_GREEN_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(LED_GREEN_GPIO_Port, &GPIO_InitStruct);

    st_typeLedInfo.red.ledChannel   = LED_CHANNEL_RED;
    st_typeLedInfo.red.driveMode    = LED_DRIVE_PWM;
    st_typeLedInfo.red.channel      = TIM_CHANNEL_2;
    st_typeLedInfo.red.htim         = &g_timer3_initpara;

    st_typeLedInfo.green.ledChannel = LED_CHANNEL_GREEN;
    st_typeLedInfo.green.driveMode  = LED_DRIVE_IO;
    st_typeLedInfo.green.channel    = 0;
    st_typeLedInfo.green.htim       = NULL;

    vLedPWMInit();

    vLedSetStatusBreathe(LED_CHANNEL_RED);
    vLedSetStatusFlashSlow(LED_CHANNEL_GREEN);
}

static void vLedPWMInit(void)
{
    TIM_OC_InitTypeDef sTimOCConfig = {0};

    sTimOCConfig.OCMode = TIM_OCMODE_PWM1;                 /* 模式选择 PWM1 */
    sTimOCConfig.Pulse = 0;                                /* 设置比较CCR,用来确定占空比,默认为0 */
    sTimOCConfig.OCPolarity = TIM_OCPOLARITY_HIGH;         /* 输出比较极性为高 */
    sTimOCConfig.OCFastMode = TIM_OCFAST_DISABLE;          /* 关闭快速响应,占空比将在下一次更新事件更新 */

    HAL_TIM_PWM_ConfigChannel(&g_timer3_initpara, &sTimOCConfig, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&g_timer3_initpara, TIM_CHANNEL_2);
}

void vLedOpen(uint32_t uiChannel)
{
    if(uiChannel & LED_CHANNEL_GREEN)
    {
        HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_SET);
    }
}

void vLedClose(uint32_t uiChannel)
{
    if(uiChannel & LED_CHANNEL_GREEN)
    {
        HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_RESET);
    }
}

void vLedRevesal(uint32_t uiChannel)
{
    if(uiChannel & LED_CHANNEL_GREEN)
    {
        HAL_GPIO_TogglePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin);
    }
}

static void vLedDutySet(LedType *ptypeLed, float fDuty)
{
    fDuty = (fDuty < 0.0f) ? 0.0f : ((fDuty > 1.0f) ? 1.0f : fDuty);

    if(ptypeLed->driveMode == LED_DRIVE_PWM)
    {
        __HAL_TIM_SET_COMPARE(ptypeLed->htim, ptypeLed->channel, (__HAL_TIM_GET_AUTORELOAD(ptypeLed->htim) * fDuty));
    }
    else
    {
        (fDuty <= 0.0f) ? vLedClose(ptypeLed->ledChannel) : vLedOpen(ptypeLed->ledChannel);
    }
}

void vLedSetStatus(LedChannelEnum usChannel, LedStateEnum enumStatus, uint8_t ucFlashCnt_or_Duty)
{
    if(usChannel & LED_CHANNEL_RED)
    {
        st_typeLedInfo.red.state = enumStatus;
        st_typeLedInfo.red.flashCnt = ucFlashCnt_or_Duty * 2 + 1;
        st_typeLedInfo.red.duty = ucFlashCnt_or_Duty % 101;
    }
    if(usChannel & LED_CHANNEL_GREEN)
    {
        st_typeLedInfo.green.state = enumStatus;
        st_typeLedInfo.green.flashCnt = ucFlashCnt_or_Duty * 2 + 1;
        st_typeLedInfo.green.duty = ucFlashCnt_or_Duty % 101;
    }
}

static void vLedStateMachine(LedType *ptypeLed)
{
    if(ptypeLed->driveMode == LED_DRIVE_NULL)
        return;

    switch(ptypeLed->state)
    {
        case LED_DISABLE:
            vLedDutySet(ptypeLed, 0.0f);
            ptypeLed->state = LED_IDLE;
            break;

        case LED_ENABLE:
            vLedDutySet(ptypeLed, LED_HIGH_DUTY);
            ptypeLed->state = LED_IDLE;
            break;

        case LED_ENABLE_LOW:
            vLedDutySet(ptypeLed, LED_LOW_DUTY);
            ptypeLed->state = LED_IDLE;
            break;

        case LED_BREATHE:
            vLedDutySet(ptypeLed, (st_sGrade < 0) ? 0.0f : (st_sGrade * st_sGrade * (1.0f / (110.0f * 110.0f))));
            break;

        case LED_DUTY:
            vLedDutySet(ptypeLed, ptypeLed->duty / 100.0f);
            ptypeLed->state = LED_IDLE;
            break;

        case LED_FLASH_FAST:
        case LED_FLASH_FAST_ENABLE_CNT:
        case LED_FLASH_FAST_DISABLE_CNT:
            if((st_uiLedTickCnt % (LED_FLASH_FAST_PERIOD / 2)) == 0)
            {
                vLedDutySet(ptypeLed, ((st_uiLedTickCnt / (LED_FLASH_FAST_PERIOD / 2)) & 1) ? 1.0f : 0.0f);

                if((ptypeLed->state != LED_FLASH_FAST) && ((ptypeLed->flashCnt--) <= 0))
                {
                    ptypeLed->state = (ptypeLed->state == LED_FLASH_FAST_ENABLE_CNT) ? LED_ENABLE : LED_DISABLE;
                }
            }
            break;

        case LED_FLASH_SLOW:
        case LED_FLASH_SLOW_ENABLE_CNT:
        case LED_FLASH_SLOW_DISABLE_CNT:
            if((st_uiLedTickCnt % (LED_FLASH_SLOW_PERIOD / 2)) == 0)
            {
                vLedDutySet(ptypeLed, ((st_uiLedTickCnt / (LED_FLASH_SLOW_PERIOD / 2)) & 1) ? 1.0f : 0.0f);

                if((ptypeLed->state != LED_FLASH_SLOW) && ((ptypeLed->flashCnt--) <= 0))
                {
                    ptypeLed->state = (ptypeLed->state == LED_FLASH_SLOW_ENABLE_CNT) ? LED_ENABLE : LED_DISABLE;
                }
            }
            break;

        case LED_FLASH_SOS:
            if(st_uiLedSosCnt < (LED_FLASH_FAST_PERIOD * 3))
            {
                vLedDutySet(ptypeLed, ((st_uiLedSosCnt / (LED_FLASH_FAST_PERIOD / 2)) & 1) ? 1.0f : 0.0f);
            }
            else if(st_uiLedSosCnt < ((LED_FLASH_FAST_PERIOD + LED_FLASH_SLOW_PERIOD) * 3))
            {
                vLedDutySet(ptypeLed, ((st_uiLedSosCnt / (LED_FLASH_SLOW_PERIOD / 2)) & 1) ? 1.0f : 0.0f);
            }
            else if(st_uiLedSosCnt < ((LED_FLASH_FAST_PERIOD + LED_FLASH_SLOW_PERIOD + LED_FLASH_FAST_PERIOD) * 3))
            {
                vLedDutySet(ptypeLed, ((st_uiLedSosCnt / (LED_FLASH_FAST_PERIOD / 2)) & 1) ? 1.0f : 0.0f);
            }
            else
            {
                vLedDutySet(ptypeLed, 0.0f);
            }
            break;

        default : break;
    }
}

static void vLedIncTick(void)
{
    volatile static int8_t st_cDirection = 1;

    st_uiLedTickCnt += LED_EXECUTION_PERIOD;
    st_uiLedSosCnt += LED_EXECUTION_PERIOD;
    st_uiLedSosCnt %= ((LED_FLASH_FAST_PERIOD + LED_FLASH_SLOW_PERIOD + LED_FLASH_FAST_PERIOD) * 3 + LED_SOS_DELAY_PERIOD);

    st_sGrade += st_cDirection;
    st_cDirection = (st_sGrade >= 110) ? -1 : ((st_sGrade <= -15) ? 1 : st_cDirection);
}

void vLedMachine(void)
{
    LedType *ptypeLedHandle = NULL;
    int8_t i = 0;

    vLedIncTick();

    ptypeLedHandle = (LedType *)(&st_typeLedInfo);

    for(i = 0; i < (sizeof(LedInfoType) / sizeof(LedType)); ++i)
    {
        vLedStateMachine(ptypeLedHandle++);
    }
}

LedInfoType *ptypeLedGetInfo(void)
{
    return &st_typeLedInfo;
}
