#include "stm32f1xx_hal.h"
#include "stdint.h"
#include "stdio.h"
#include "string.h"
#include "version.h"

#include "DevicesFlash.h"
#include "DevicesSPIFlash.h"
#include "DevicesCRC.h"
#include "DevicesWatchDog.h"
#include "DevicesDelay.h"
#include "DevicesModbus.h"

#include "DriverBootloader.h"
#include "DriverModbus.h"
#include "DriverOTA.h"

#include "DriverUpgradePD.h"

int8_t cOTAUpgradePD(OTAFirmwarePortInfoType *ptypeOTAFirmwarePortInfo)
{
    static FirmwarePortInfoType st_typeFirmwareOTA = {0};
    FirmwareInfoType *ptypeFirmwareInfo = ptypeFirmwareInfoGet();
    FirmwarePortInfoType *ptypeFirmwarePortSou = NULL, *ptypeFirmwarePortTar = NULL;
    uint8_t ucVersionBuff[8] = {0}, ucTypeBuff[4] = {0};
    int8_t cError = 0;

    cError |= cSPIFlashReadDatas(ptypeOTAFirmwarePortInfo->address + 0x820, ucVersionBuff, 8);
    cError |= cSPIFlashReadDatas(ptypeOTAFirmwarePortInfo->address + 0x860, ucTypeBuff, 4);

    /* 固件类型名，判断源固件是否来源错误 */
    if(memcmp(ucTypeBuff, "MySTM32", 4) != 0)
        return 2;

    st_typeFirmwareOTA.address = ptypeOTAFirmwarePortInfo->address;
    st_typeFirmwareOTA.length = ptypeOTAFirmwarePortInfo->length;
    st_typeFirmwareOTA.crcValue = ptypeOTAFirmwarePortInfo->crcValue;
    st_typeFirmwareOTA.registers |= FIRMWARE_SOURCE_SPI_FLASH;

    switch(ptypeOTAFirmwarePortInfo->number)
    {
        /* 更新、存储到 APP 区域 */
        case 0 :
            ptypeFirmwarePortSou = &ptypeFirmwareInfo->appOut;
            ptypeFirmwarePortTar = &ptypeFirmwareInfo->app;
        break;

        /* 更新、存储到 Bootloader 区域 */
        case 1 : 
            ptypeFirmwarePortSou = &ptypeFirmwareInfo->bootloaderOut;
            ptypeFirmwarePortTar = &ptypeFirmwareInfo->bootloader;
        break;

        /* 更新、存储到 Boot 区域 */
        case 2 : 
            ptypeFirmwarePortSou = &ptypeFirmwareInfo->bootOut;
            ptypeFirmwarePortTar = &ptypeFirmwareInfo->boot;
        break;

        default : return 3;
    }

    /* 更新、并存储参数 */
    if((cError = cFirmwareUpdate(ptypeFirmwarePortSou, &st_typeFirmwareOTA)) == 0)
    {
        ptypeFirmwarePortSou->length = ptypeOTAFirmwarePortInfo->length;
        ptypeFirmwarePortSou->crcValue = ptypeOTAFirmwarePortInfo->crcValue;
        ptypeFirmwarePortSou->registers |= FIRMWARE_UPDATE;

        ptypeFirmwarePortTar->registers &= ~FIRMWARE_SOURCE_SPI_FLASH;
        cError |= cFirmwareWrite();
    }

    return cError;
}