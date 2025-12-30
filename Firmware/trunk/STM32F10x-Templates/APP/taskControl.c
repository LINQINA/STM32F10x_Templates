#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "taskControl.h"

TaskHandle_t g_TaskControlHand;   /* 逻辑控制任务句柄 */

static int8_t cControlMode(void);

void vTaskControl(void *pvParameters)
{
    TickType_t rtosTypeTickNow = xTaskGetTickCount();

    while(1)
    {
        /* 定时执行 */
        vTaskDelayUntil(&rtosTypeTickNow, 100 / portTICK_RATE_MS);

        cControlMode();
    }
}

static int8_t cControlMode(void)
{
    return 0;
}
