#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "taskMonitor.h"

TaskHandle_t g_TaskMonitorHand = NULL;   /* 系统监控任务 */

static int8_t cMonitorACOut(void);

void vTaskMonitor(void *pvParameters)
{
    TickType_t rtosTypeTickNow = xTaskGetTickCount();

    /* 等待其他任务启动 */
    vTaskDelay(1000 / portTICK_RATE_MS);

    while(1)
    {
        /* 定时执行 */
        vTaskDelayUntil(&rtosTypeTickNow, 10 / portTICK_RATE_MS);

        /* AC 输出检测 */
        cMonitorACOut();
    }
}

static int8_t cMonitorACOut(void)
{
     return 0;
}
