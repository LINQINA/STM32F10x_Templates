#include "stm32f1xx_hal.h"
#include "DevicesWatchDog.h"

IWDG_HandleTypeDef g_iwdg_handle;  /* 独立看门狗句柄 */

void vWatchdogInit(void)
{
    g_iwdg_handle.Instance = IWDG;
    g_iwdg_handle.Init.Prescaler = IWDG_PRESCALER_256;  /* 设置IWDG分频系数 */
    g_iwdg_handle.Init.Reload = 2500;                   /* 设置重装载值 */
    HAL_IWDG_Init(&g_iwdg_handle);                      /* 初始化IWDG */
}

void vWatchdogReload(void)
{
    /* reload the counter of IWDG */
    if (g_iwdg_handle.Instance != NULL)
        HAL_IWDG_Refresh(&g_iwdg_handle);
}
