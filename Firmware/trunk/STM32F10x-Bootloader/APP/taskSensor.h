#ifndef _taskSensor_H_
#define _taskSensor_H_

extern TaskHandle_t g_TaskSensorHand;   /* 传感器数据处理任务句柄 */

void vTaskSensor(void *pvParameters);

#endif
