#ifndef _taskOTA_H_
#define _taskOTA_H_


extern TaskHandle_t g_TaskOTAHand;   /* OTA 任务句柄 */


void vTaskOTA(void *pvParameters);



#endif
