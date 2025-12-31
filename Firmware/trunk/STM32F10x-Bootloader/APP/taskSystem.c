#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "DevicesTimer.h"
#include "DevicesBeep.h"
#include "DevicesLed.h"
#include "DevicesKey.h"
#include "DevicesQueue.h"
#include "DevicesSPI.h"
#include "DevicesSPIFlash.h"
#include "DevicesWatchDog.h"

#include "DriverLogPrintf.h"
#include "DriverBootloader.h"

#include "taskMonitor.h"
#include "taskSensor.h"
#include "taskControl.h"
#include "taskSystem.h"
#include "taskSoftTimer.h"
#include "taskMessageSlave.h"

TaskHandle_t g_TaskSystemInitHand = NULL;  /* 系统初始化任务句柄 */

/* RS485 Bus 总线互斥信号量 */
SemaphoreHandle_t g_xRS485BusSemaphore;
/* RS485 Bus 总线互斥信号量 */
SemaphoreHandle_t g_xUartLogSemaphore;
/* SPI Flash 读、写互斥信号量 */
SemaphoreHandle_t g_xSpiFlashSemaphore;
/* 芯片内部 Flash 读、写互斥信号量 */
SemaphoreHandle_t g_xChipFlashSemaphore;

void vApplicationIdleHook(void)
{
    vWatchdogReload();
}

void vUserSystemInit(void)
{
    enumQueueInit();

    cTimer3Init();
    cTimer6Init();

    vSPI2Init();
    vSPIFlashInit();

    cSoftTimerInit();

    vWatchdogInit();
}

void vTaskSystemInit(void *pvParameters)
{
    int8_t cError = 0, cError2 = 0;
    
    /* 创建递归互斥信号量 */
    g_xRS485BusSemaphore  = xSemaphoreCreateRecursiveMutex();
    g_xUartLogSemaphore   = xSemaphoreCreateRecursiveMutex();
    g_xSpiFlashSemaphore  = xSemaphoreCreateRecursiveMutex();
    g_xChipFlashSemaphore = xSemaphoreCreateRecursiveMutex();
    
    xTaskCreate(vTaskMessageSlave,      "Message Slave",    512,    NULL, configMAX_PRIORITIES - 11, &g_TaskMessageSlaveHand);

    vTaskDelay(10 / portTICK_RATE_MS);
    vUserSystemInit();

    cError  = cFirmwareUpdateBoot();
    cError2 = cFirmwareUpdateAPP();
    if((cError == 0) || (cError2 == 0))
    {
        /* 更新标志 */
        cFirmwareWrite();

        cLogPrintfNormal("系统重启.\r\n");
        /* 软件重启 */
        NVIC_SystemReset();
    }

    /* 删除自己 */
    vTaskDelete( NULL );
}