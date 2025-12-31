#ifndef _taskMonitor_H_
#define _taskMonitor_H_

/* 系统监控任务句柄 */
extern TaskHandle_t g_TaskMonitorHand;

void vTaskMonitor(void *pvParameters);


#endif
