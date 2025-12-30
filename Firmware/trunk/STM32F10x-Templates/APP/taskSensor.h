#ifndef _taskSensor_H_
#define _taskSensor_H_

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "stdint.h"
#include "stdio.h"
#include "version.h"
#include "DriverOTA.h"

typedef struct{
    OTAInfoType         *ptypeOTAInfo;              /* OTA信息 */
    productType         *ptypeProduct;              /* 版本信息 */
}SensorInfoType;

extern TaskHandle_t g_TaskSensorHand;   /* 传感器数据处理任务句柄 */

void vTaskSensor(void *pvParameters);
SensorInfoType *ptypeSensorInfoGet(void);

#endif
