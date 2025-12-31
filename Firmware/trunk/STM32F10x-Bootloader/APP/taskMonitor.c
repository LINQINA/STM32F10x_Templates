#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "taskMonitor.h"

TaskHandle_t g_TaskMonitorHand = NULL;   /* 系统监控任务句柄 */

static int8_t cMonitorACOut(void);

void vTaskMonitor(void *pvParameters)
{
    TickType_t rtosTypeTickNow = xTaskGetTickCount();

    /* 等待其它任务运行 */
    vTaskDelay(1000 / portTICK_RATE_MS);

    while(1)
    {
        /* 周期执行 */
        vTaskDelayUntil(&rtosTypeTickNow, 10 / portTICK_RATE_MS);

        /* AC 输出监控 */
        cMonitorACOut();
    }
}

static int8_t cMonitorACOut(void)
{
     return 0;
}
