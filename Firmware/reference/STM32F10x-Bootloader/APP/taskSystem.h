#ifndef _taskSystem_H_
#define _taskSystem_H_

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* 系统初始化任务句柄 */
extern TaskHandle_t g_TaskSystemInitHand;

/* RS485 Bus 总线互斥信号量 */
extern SemaphoreHandle_t g_xRS485BusSemaphore;
/* RS485 Bus 总线互斥信号量 */
extern SemaphoreHandle_t g_xUartLogSemaphore;
/* SPI Flash 读、写互斥信号量 */
extern SemaphoreHandle_t g_xSpiFlashSemaphore;
/* 芯片内部 Flash 读、写互斥信号量 */
extern SemaphoreHandle_t g_xChipFlashSemaphore;

void vTaskSystemInit(void *pvParameters);

#endif