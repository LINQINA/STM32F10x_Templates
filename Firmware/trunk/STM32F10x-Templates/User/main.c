#include "stm32f1xx.h"
#include "stm32f1xx_hal_rcc.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "DevicesFlash.h"
#include "DevicesUart.h"
#include "DevicesTimer.h"

#include "DriverLogPrintf.h"

#include "taskSystem.h"

#include "userSystem.h"
#include "version.h"

#define VECT_TAB_OFFSET  FLASH_APP_ADDR

int main(void)
{
    SCB->VTOR = FLASH_BASE | VECT_TAB_OFFSET;

    HAL_Init();

    /* 使能AFIO */
    __HAL_RCC_AFIO_CLK_ENABLE();

    /* 中断优先级分组改为4 */
    HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);
    
    /* 初始化各时钟频率 */
    vSystemClockInit();
    cTimer6Init();
    vUart1Init();

    cLogPrintfSystem("\n\n\r APP start.\r\n");
    cLogPrintfSystem("Boot software version: %s\r\n", pcVersionBootSoftGet());
    cLogPrintfSystem("Boot software date: %s\r\n", pcVersionBootDateGet());
    cLogPrintfSystem("Bootloader software version: %s\r\n", pcVersionBootloaderSoftGet());
    cLogPrintfSystem("Bootloader software date: %s\r\n", pcVersionBootloaderDateGet());
    cLogPrintfSystem("APP software version: %s\r\n", pcVersionAPPSoftGet());
    cLogPrintfSystem("APP software date: %s\r\n\n", pcVersionAPPDateGet());
    
    cLogPrintfSystem("SYSCLK = %u Hz\r\n", HAL_RCC_GetSysClockFreq());    /* 系统时钟频率 */
    cLogPrintfSystem("HCLK   = %u Hz\r\n", HAL_RCC_GetHCLKFreq());        /* AHB 总线时钟（CPU 核心时钟） */
    cLogPrintfSystem("PCLK1  = %u Hz\r\n", HAL_RCC_GetPCLK1Freq());       /* APB1 总线时钟 */
    cLogPrintfSystem("PCLK2  = %u Hz\r\n", HAL_RCC_GetPCLK2Freq());       /* APB2 总线时钟 */
    cLogPrintfSystem("SystemCoreClock  = %u Hz\r\n", SystemCoreClock);    /* APB2 总线时钟 */
    
    /* 创建第1个用户任务 */
    xTaskCreate(vTaskSystemInit, "System Init", 512, NULL, configMAX_PRIORITIES - 1, &g_TaskSystemInitHand);
    /* 启动 RTOS 运行 */
    vTaskStartScheduler();

    while(1);

}
