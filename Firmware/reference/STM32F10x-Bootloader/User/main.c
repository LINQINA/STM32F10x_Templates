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
#include "DriverBootloader.h"

#include "taskSystem.h"

#include "userSystem.h"
#include "version.h"

#define VECT_TAB_OFFSET  FLASH_BOOTLOADER_ADDR


int main(void)
{
    SCB->VTOR = FLASH_BASE | VECT_TAB_OFFSET;

    vFirmwareInit();

    /* 跳转到 APP */
    cFirmwareJumpAPP();

    HAL_Init();
    
    /* 使能AFIO */
    __HAL_RCC_AFIO_CLK_ENABLE();

    /* 中断优先级分组改为4 */
    HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);
    
    /* 初始化各时钟频率 */
    vSystemClockInit();
    cTimer6Init();
    vUart1Init();

    cLogPrintfSystem("\n\n\rBootloader start.\r\n");
    cLogPrintfSystem("Boot software version: %s\r\n", pcVersionBootSoftGet());
    cLogPrintfSystem("Boot software date: %s\r\n", pcVersionBootDateGet());
    cLogPrintfSystem("Bootloader software version: %s\r\n", pcVersionBootloaderSoftGet());
    cLogPrintfSystem("Bootloader software date: %s\r\n", pcVersionBootloaderDateGet());
    cLogPrintfSystem("APP software version: %s\r\n", pcVersionAPPSoftGet());
    cLogPrintfSystem("APP software date: %s\r\n\n", pcVersionAPPDateGet());

    /* print out the clock frequency of system, AHB, APB1 and APB2 */
    cLogPrintfSystem("SystemCoreClock: %d\r\n", (int32_t)SystemCoreClock);
    
    /* 创建第1个用户任务 */
    xTaskCreate(vTaskSystemInit, "System Init", 512, NULL, configMAX_PRIORITIES - 1, &g_TaskSystemInitHand);
    /* 启动 RTOS 运行 */
    vTaskStartScheduler();

    while(1);

}

