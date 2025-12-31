#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "DevicesWatchDog.h"

#include "taskSensor.h"

TaskHandle_t g_TaskSensorHand = NULL;   /* 传感器数据处理任务句柄 */

static int8_t cADCSoftScanUpdate(void);

void vTaskSensor(void *pvParameters)
{
    TickType_t rtosTypeTickNow = xTaskGetTickCount();
    
    while(1)
    {
          /* 周期执行 */
        vTaskDelayUntil(&rtosTypeTickNow, 5 / portTICK_RATE_MS);
        
        /* ADC信息更新 */
        cADCSoftScanUpdate();

        vWatchdogReload();
    }
}

static int8_t cADCSoftScanUpdate(void)
{
     return 0;
}
