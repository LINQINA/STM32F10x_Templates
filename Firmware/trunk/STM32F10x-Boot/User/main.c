#include "stdio.h"
#include "stm32f1xx.h"
#include "stm32f1xx_hal_rcc.h"

#include "DevicesFlash.h"
#include "DevicesUart.h"
#include "DevicesTimer.h"
#include "DevicesDelay.h"

#include "DriverLogPrintf.h"
#include "DriverBootloader.h"

#include "taskSystem.h"

#include "userSystem.h"

#include "version.h"

int main(void)
{
    vFirmwareInit();

    /* 跳转到 Bootloader */
    cFirmwareJumpBootloader();

    /* 使能AFIO */
    __HAL_RCC_AFIO_CLK_ENABLE();

    /* 中断优先级分组改为4 */
    HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);

    /* 初始化各时钟频率 */
    vSystemClockInit();
    cTimer6Init();
    vUart1Init();

    printf("Boot Start \r\n");
    printf("Boot software version: %s\r\n", pcVersionBootSoftGet());
    printf("Boot software date: %s\r\n", pcVersionBootDateGet());

    vTaskSystem();

    while(1)
    {
        printf("Boot Update Error \r\n");
        vDelayS(1);
    }
}

