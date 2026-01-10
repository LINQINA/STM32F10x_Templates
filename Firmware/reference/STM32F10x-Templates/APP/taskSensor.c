#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "DevicesADC.h"
#include "DevicesWatchDog.h"

#include "taskSensor.h"

#include "version.h"

TaskHandle_t g_TaskSensorHand = NULL;   /* 传感器数据处理任务句柄 */

/* 系统外设信息 */
SensorInfoType g_typeSensorInfo;

void vTaskSensor(void *pvParameters)
{
    TickType_t rtosTypeTickNow = xTaskGetTickCount();
    g_typeSensorInfo.ptypeOTAInfo           = ptypeOTAInfoGet();
    g_typeSensorInfo.ptypeProduct           = ptypeProductGet();
    
    while(1)
    {
          /* 周期执行 */
        vTaskDelayUntil(&rtosTypeTickNow, 5 / portTICK_RATE_MS);

        /* ADC信息更新 */
        //vADCxScanLow();

        /* 产品信息更新 */
        cProductInfoUpdate();

        vWatchdogReload();
    }
}

SensorInfoType *ptypeSensorInfoGet(void)
{
    return &g_typeSensorInfo;
}
