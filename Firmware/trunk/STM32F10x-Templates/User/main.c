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

    __HAL_RCC_AFIO_CLK_ENABLE();

    HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);

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

    cLogPrintfSystem("SYSCLK = %u Hz\r\n", HAL_RCC_GetSysClockFreq());
    cLogPrintfSystem("HCLK   = %u Hz\r\n", HAL_RCC_GetHCLKFreq());
    cLogPrintfSystem("PCLK1  = %u Hz\r\n", HAL_RCC_GetPCLK1Freq());
    cLogPrintfSystem("PCLK2  = %u Hz\r\n", HAL_RCC_GetPCLK2Freq());
    cLogPrintfSystem("SystemCoreClock  = %u Hz\r\n", SystemCoreClock);

    xTaskCreate(vTaskSystemInit, "System Init", 512, NULL, configMAX_PRIORITIES - 1, &g_TaskSystemInitHand);

    vTaskStartScheduler();

    while(1);
}
