#include "stdio.h"

#include "DevicesTimer.h"
#include "DevicesBeep.h"
#include "DevicesLed.h"
#include "DevicesKey.h"
#include "DevicesQueue.h"
#include "DevicesSPI.h"
#include "DevicesSPIFlash.h"
#include "DevicesWatchDog.h"
#include "DevicesDelay.h"

#include "DriverBootloader.h"
#include "DriverLogPrintf.h"

#include "taskSystem.h"

void vUserSystemInit(void)
{
    enumQueueInit();

    cTimer3Init();
    cTimer6Init();

    vSPIFlashInit();
}

void vTaskSystem(void)
{
    FirmwareInfoType *ptypeFirmwareInfo = ptypeFirmwareInfoGet();
    int32_t iCnt = 0;
    int8_t cError = 0;

    vUserSystemInit();

    /* 判断 Bootloader 是否更新成功 */
    cError  = cFirmwareUpdateBootloader();
    
    if(cError == 0)
    {
        cFirmwareWrite();

        NVIC_SystemReset();
    }

    /* 跳转到 APP */
    cFirmwareJumpAPP();
    
    /* 跳转APP失败,尝试从外部 Flash 恢复 APP、Bootloader 固件数据 */
    if(cFirmwareUpdateALLForce() == 0)
    {
        cFirmwareWrite();

        NVIC_SystemReset();
    }

    /* 恢复固件Flash失败,跳转到厂商Bootloader */
    while(1)
    {
        printf("%d\r\n", iCnt);
        if((++iCnt) > 8)
        {
            printf("跳转到 厂商Bootloader.\r\n");
            cFirmwareJumpFactoryBootloader();
        }
    }
}