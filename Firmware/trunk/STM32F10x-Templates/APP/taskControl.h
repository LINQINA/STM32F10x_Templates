#ifndef _taskControl_H_
#define _taskControl_H_

extern TaskHandle_t g_TaskControlHand;   /* 逻辑控制任务句柄 */

void vTaskControl(void *pvParameters);

#endif
