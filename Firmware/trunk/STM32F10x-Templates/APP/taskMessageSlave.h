#ifndef _taskMessageSlave_H_
#define _taskMessageSlave_H_

/* 通讯消息从机任务 */
extern TaskHandle_t g_TaskMessageSlaveHand;

void vTaskMessageSlave(void *pvParameters);

#endif
