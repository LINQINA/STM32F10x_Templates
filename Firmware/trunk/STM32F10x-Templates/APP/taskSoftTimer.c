#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "timers.h"

#include "DevicesLed.h"
#include "DevicesBeep.h"
#include "DevicesKey.h"

#include "taskSoftTimer.h"
#include "taskKey.h"

TimerHandle_t g_xTimersLedHandle = NULL;
TimerHandle_t g_xTimersBeepHandle = NULL;
TimerHandle_t g_xTimersKeyHandle = NULL;

/*
*   功    能: vSoftTimerLedCallback
*   描    述: 定时器LED回调函数
*   参    数: 无
*   返 回 值: 无
*/
static void vSoftTimerLedCallback(xTimerHandle pxTimer)
{
    (void)pxTimer;
    vLedMachine();
}

static void vSoftTimerKeyCallback(xTimerHandle pxTimer)
{
    (void)pxTimer;

    /* 检测按键状态变化 */
    if(enumKeyStateMachine(&g_typeKeyData) != keyNormal)
    {
        /* 执行按键任务 */
        vTaskKey(&g_typeKeyData);
    }
}

static void vSoftTimerBeepCallback(xTimerHandle pxTimer)
{
    (void)pxTimer;
    vBeepMachine();
}

int8_t cSoftTimerInit(void)
{
    /* 创建LED定时器 */
    g_xTimersLedHandle = xTimerCreate("Timer led", 20 / portTICK_RATE_MS, pdTRUE, (void*)NULL, vSoftTimerLedCallback);
    xTimerStart(g_xTimersLedHandle, 200);
    
    /* 创建KEY定时器 */
    g_xTimersKeyHandle = xTimerCreate("Timer key", 20 / portTICK_RATE_MS, pdTRUE, (void*)NULL, vSoftTimerKeyCallback);
    xTimerStart(g_xTimersKeyHandle, 200);

    /* 创建BEEP定时器 */
    g_xTimersBeepHandle = xTimerCreate("Timer beep", 100 / portTICK_RATE_MS, pdTRUE, (void*)NULL, vSoftTimerBeepCallback);
    xTimerStart(g_xTimersBeepHandle, 200);

    return 0;
}
