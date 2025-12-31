#include "stdint.h"
#include "stdio.h"
#include "string.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "DevicesRS485.h"
#include "DevicesDelay.h"
#include "DevicesModbus.h"

#include "DriverOTA.h"

#include "taskSensor.h"


#include "taskOTA.h"

TaskHandle_t g_TaskOTAHand = NULL;   /* Iot任务句柄 */

void vTaskOTA(void *pvParameters)
{
    SensorInfoType *ptypeSensorInfo = ptypeSensorInfoGet();

    vOTAInit();

    while(1)
    {
        /* 等待任务消息 */
        ulTaskNotifyTake(pdTRUE, 50 / portTICK_RATE_MS);


        if(ptypeSensorInfo->ptypeOTAInfo->state == OTA_STATE_START && g_cOTAInitFlag)
        {
            /* 1.固件自检与初始化 */
            vOTAStart();
        }

        if(ptypeSensorInfo->ptypeOTAInfo->state == OTA_STATE_UPDATING && g_cOTAInitFlag)
        {
            /* 2.分包升级 */
            vOTAFirmwareUpdateAll();
        }
    }
}