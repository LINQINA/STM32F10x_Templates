#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "taskControl.h"


TaskHandle_t g_TaskControlHand;   /* 逻辑、交互处理任务句柄 */

/* 需要设定的系统外设信息 */
ControlInfoType g_typeControlInfo;

static int8_t cControlMode(void);

void vTaskControl(void *pvParameters)
{
    TickType_t rtosTypeTickNow = xTaskGetTickCount();

    while(1)
    {
        
       /* 周期执行 */
        vTaskDelayUntil(&rtosTypeTickNow, 100 / portTICK_RATE_MS);

        cControlMode();
    }
}

ControlInfoType *ptypeControlInfoGet(void)
{
    return &g_typeControlInfo;
}

static int8_t cControlMode(void)
{
    return 0;
}
