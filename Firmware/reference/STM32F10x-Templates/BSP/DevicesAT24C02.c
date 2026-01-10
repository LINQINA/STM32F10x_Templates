#include "stm32f1xx_hal.h"
#include "taskSystem.h"
#include "stdio.h"
#include "stdint.h"
#include "string.h"

#include "DevicesIIC.h"
#include "DevicesAT24C02.h"
#include "DevicesDelay.h"

static int8_t cIICFlashLock(void)
{
    xSemaphoreTakeRecursive(g_xIICFlashSemaphore, portMAX_DELAY);

    return 0;
}

static int8_t cIICFlashUnlock(void)
{
    xSemaphoreGiveRecursive(g_xIICFlashSemaphore);

    return 0;
}

int8_t cAT24C02WriteDatas(uint8_t ucAddress,uint8_t *pucDatas,uint16_t usLength)
{
    int8_t cError = 0;
    uint16_t usLengthTemp = 0;

    cIICFlashLock();

    while(usLength > 0)
    {
        /* 页对齐 */
        usLengthTemp = usLength > (PAGE_SIZE - ucAddress % PAGE_SIZE) ? (PAGE_SIZE - ucAddress % PAGE_SIZE) : usLength;

        cError = iI2CWriteDatas(AT24C02_WRITE_ADDRESS, ucAddress, pucDatas, usLengthTemp);
        
        if(cError != 0)
            break;

        ucAddress += usLengthTemp;
        pucDatas += usLengthTemp;
        usLength -= usLengthTemp;

        /* 官方手册推荐等待5ms,写10ms更稳健一点 */
        vRtosDelayMs(10);
    }

    cIICFlashUnlock();

    return cError;
}

int8_t cAT24C02ReadDatas(uint8_t ucAddress,uint8_t *pucDatas,uint16_t usLength)
{
    int8_t cError = 0;
    uint16_t usLengthTemp = 0;

    cIICFlashLock();

    while (usLength > 0)
    {
        /* 页对齐 */
        usLengthTemp = usLength > (PAGE_SIZE - ucAddress % PAGE_SIZE) ? (PAGE_SIZE - ucAddress % PAGE_SIZE) : usLength;

        cError = iI2CReadDatas(AT24C02_READ_ADDRESS, ucAddress, pucDatas, usLengthTemp);

        if (cError != 0)
            break;

        ucAddress  += usLengthTemp;
        pucDatas   += usLengthTemp;
        usLength   -= usLengthTemp;
    }

    cIICFlashUnlock();

    return cError;
}
