#ifndef _taskControl_H_
#define _taskControl_H_

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "DevicesTime.h"

typedef struct{
    TimeInfoType            typeTimeInfo;       /* 时间信息 */

}ControlInfoType;

extern TaskHandle_t g_TaskControlHand;   /* 逻辑、交互处理任务句柄 */

void vTaskControl(void *pvParameters);
ControlInfoType *ptypeControlInfoGet(void);

#endif
