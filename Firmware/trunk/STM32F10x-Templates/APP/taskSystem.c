#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "DevicesADC.h"
#include "DevicesTimer.h"
#include "DevicesBeep.h"
#include "DevicesLed.h"
#include "DevicesKey.h"
#include "DevicesQueue.h"
#include "DevicesSPI.h"
#include "DevicesSPIFlash.h"
#include "DevicesWatchDog.h"
#include "DevicesIIC.h"

#include "DriverBootloader.h"

#include "taskMonitor.h"
#include "taskSensor.h"
#include "taskControl.h"
#include "taskSystem.h"
#include "taskSoftTimer.h"
#include "taskMessageSlave.h"
#include "taskOTA.h"

TaskHandle_t g_TaskSystemInitHand = NULL;  /* 系统初始化任务句柄 */
extern volatile uint32_t uwTick;

/* RS485 Bus 总线互斥信号量 */
SemaphoreHandle_t g_xRS485BusSemaphore;
/* RS485 Bus 总线互斥信号量 */
SemaphoreHandle_t g_xUartLogSemaphore;
/* SPI Flash 读、写互斥信号量 */
SemaphoreHandle_t g_xSpiFlashSemaphore;
/* IIC Flash 读、写互斥信号量 */
SemaphoreHandle_t g_xIICFlashSemaphore;
/* 芯片内部 Flash 读、写互斥信号量 */
SemaphoreHandle_t g_xChipFlashSemaphore;
/* Can1 发送数据 互斥信号量 */
SemaphoreHandle_t g_xCan1Semaphore;


void vApplicationIdleHook(void)
{
    vWatchdogReload();
}

/* 不加入此程序,初始化的时候会进入HardFault,因为HAL_Init会使能SysTick */
HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
    /* FreeRTOS 负责配置 SysTick，这里不做任何事 */
    (void)TickPriority;
    return HAL_OK;
}

uint32_t HAL_GetTick(void)
{
    if(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
    {
        /* 使用 FreeRTOS 系统节拍 */
        return xTaskGetTickCount();
    }
    else
    {
        return uwTick;
    }
}

void vUserSystemInit(void)
{
    vFirmwareInit();

    enumQueueInit();

    vADCInit();

    cTimer3Init();
    cTimer7Init();

    vLedInit();

    //vBeepInit();

    vKeyInit();

    vIICInit();
    
    vSPI2Init();
    vSPIFlashInit();

    cSoftTimerInit();

    vWatchdogInit();
}

void vTaskSystemInit(void *pvParameters)
{
    /* 创建递归互斥信号量 */
    g_xRS485BusSemaphore  = xSemaphoreCreateRecursiveMutex();
    g_xUartLogSemaphore   = xSemaphoreCreateRecursiveMutex();
    g_xSpiFlashSemaphore  = xSemaphoreCreateRecursiveMutex();
    g_xIICFlashSemaphore  = xSemaphoreCreateRecursiveMutex();
    g_xChipFlashSemaphore = xSemaphoreCreateRecursiveMutex();
    g_xCan1Semaphore      = xSemaphoreCreateRecursiveMutex();
    
    xTaskCreate(vTaskMonitor,           "Monitor",          256,    NULL, configMAX_PRIORITIES - 2,  &g_TaskMonitorHand);

    xTaskCreate(vTaskControl,           "Control",          256,    NULL, configMAX_PRIORITIES - 3,  &g_TaskControlHand);

    xTaskCreate(vTaskSensor,            "Sensor",           256,    NULL, configMAX_PRIORITIES - 4,  &g_TaskSensorHand);
    
    xTaskCreate(vTaskOTA,               "OTA",              512,    NULL, configMAX_PRIORITIES - 10, &g_TaskOTAHand);

    xTaskCreate(vTaskMessageSlave,      "Message Slave",    512,    NULL, configMAX_PRIORITIES - 11, &g_TaskMessageSlaveHand);

    vTaskDelay(10 / portTICK_RATE_MS);
    vUserSystemInit();

    /* 删除自己 */
    vTaskDelete( NULL );
}