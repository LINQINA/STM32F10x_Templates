#include "stm32f1xx_hal.h"
#include "string.h"
#include "stdint.h"
#include "stdio.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "DevicesUart.h"
#include "DevicesQueue.h"

#include "DriverModbus.h"

#include "taskMessageSlave.h"

/* 通信消息解析任务句柄 */
TaskHandle_t g_TaskMessageSlaveHand = NULL;

/* 解析缓存 */
static uint8_t st_ucMessageAnalysisBuff[256];


void vTaskMessageSlave(void *pvParameters)
{
    uint32_t uiNotifiedValue = 0;
    int32_t iLength = 0;
    
    while(1)
    {
        /* 等待任务消息 */
        xTaskNotifyWait(0x00000000, 0xFFFFFFFF, &uiNotifiedValue, 20 / portTICK_RATE_MS);

        /* 读取并解析 上位机 发送过来的数据 */
        while((iLength = iQueueGetLengthOfOccupy(&g_TypeQueueUart1Read)) != 0)
        {
            iLength = (iLength > sizeof(st_ucMessageAnalysisBuff)) ? sizeof(st_ucMessageAnalysisBuff) : iLength;

            enumQueuePopDatas(&g_TypeQueueUart1Read, st_ucMessageAnalysisBuff, iLength);

            cModbusUnpack((uint32_t)UART_LOG, st_ucMessageAnalysisBuff, iLength);
        }
    }
}