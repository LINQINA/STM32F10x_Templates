#include "stm32f1xx.h"
#include "stdint.h"

#include "DevicesTimer.h"

TIM_HandleTypeDef g_timer3_initpara;
TIM_HandleTypeDef g_timer6_initpara;
TIM_HandleTypeDef g_timer7_initpara;

int8_t cTimer3Init(void)
{
    __HAL_RCC_TIM3_CLK_ENABLE();

    g_timer3_initpara.Instance = TIM3;
    g_timer3_initpara.Init.Prescaler = 1 - 1;
    g_timer3_initpara.Init.CounterMode = TIM_COUNTERMODE_UP;
    g_timer3_initpara.Init.Period = (SystemCoreClock / 2048) -1;
    g_timer3_initpara.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    g_timer3_initpara.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

    HAL_TIM_Base_Init(&g_timer3_initpara);

    return 0;
}

int8_t cTimer6Init(void)
{
    __HAL_RCC_TIM6_CLK_ENABLE();

    g_timer6_initpara.Instance = TIM6;
    g_timer6_initpara.Init.Prescaler = (SystemCoreClock / 1000000) -1;
    g_timer6_initpara.Init.CounterMode = TIM_COUNTERMODE_UP;
    g_timer6_initpara.Init.Period = 65536 - 1;
    g_timer6_initpara.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    g_timer6_initpara.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

    HAL_TIM_Base_Init(&g_timer6_initpara);

    HAL_TIM_Base_Start_IT(&g_timer6_initpara); 
    HAL_NVIC_SetPriority(TIM6_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(TIM6_IRQn);

    return 0;
}

int8_t cTimer7Init(void)
{
    __HAL_RCC_TIM7_CLK_ENABLE();

    g_timer7_initpara.Instance = TIM7;
    g_timer7_initpara.Init.Prescaler = 1 -1;
    g_timer7_initpara.Init.CounterMode = TIM_COUNTERMODE_UP;
    g_timer7_initpara.Init.Period = (SystemCoreClock / 40000) -1;
    g_timer7_initpara.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    g_timer7_initpara.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

    HAL_TIM_Base_Init(&g_timer7_initpara);

    HAL_TIM_Base_Start_IT(&g_timer7_initpara); 
    HAL_NVIC_SetPriority(TIM7_IRQn, 3, 0);
    HAL_NVIC_EnableIRQ(TIM7_IRQn);

    return 0;
}
