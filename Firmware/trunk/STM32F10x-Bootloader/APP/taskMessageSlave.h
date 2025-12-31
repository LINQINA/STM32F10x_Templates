#ifndef _taskMessageSlave_H_
#define _taskMessageSlave_H_


/* 通信消息解析任务句柄 */
extern TaskHandle_t g_TaskMessageSlaveHand;


void vTaskMessageSlave(void *pvParameters);


#endif
