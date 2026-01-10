#include "stm32f1xx_hal.h"
#include "stdint.h"
#include "stdio.h"
#include "string.h"
#include "DevicesFlash.h"
#include "DevicesSPIFlash.h"
#include "DevicesCRC.h"
#include "DriverLogPrintf.h"
#include "DriverBootloader.h"


/* 各固件参数 */
static FirmwareInfoType st_typeFirmwareInfo;

#define FIRMWARE_BUFF_LENGTH 256
/* 从Flash读写数据用的缓存 */
uint8_t st_ucFirmwareBuff[FIRMWARE_BUFF_LENGTH] = {0};

/* 一些出厂的固定参数 */
void vFirmwareFactoryInit(void)
{
    /* 内部Flash分区地址 */
    st_typeFirmwareInfo.boot.address            = FLASH_BOOT_ADDR;
    st_typeFirmwareInfo.boot.registers          &= ~FIRMWARE_SOURCE_SPI_FLASH;
    
    st_typeFirmwareInfo.bootloader.address      = FLASH_BOOTLOADER_ADDR;
    st_typeFirmwareInfo.bootloader.registers    &= ~FIRMWARE_SOURCE_SPI_FLASH;
    
    st_typeFirmwareInfo.app.address             = FLASH_APP_ADDR;
    st_typeFirmwareInfo.app.registers           &= ~FIRMWARE_SOURCE_SPI_FLASH;


    /* 外部Flash updata分区地址 */
    st_typeFirmwareInfo.bootOut.address         = SPI_FLASH_BOOT_BACK_ADDR;
    st_typeFirmwareInfo.bootOut.registers       |= FIRMWARE_SOURCE_SPI_FLASH;
    
    st_typeFirmwareInfo.bootloaderOut.address   = SPI_FLASH_BOOTLOADER_BACK_ADDR;
    st_typeFirmwareInfo.bootloaderOut.registers |= FIRMWARE_SOURCE_SPI_FLASH;
    
    st_typeFirmwareInfo.appOut.address          = SPI_FLASH_APP_BACK_ADDR;
    st_typeFirmwareInfo.appOut.registers        |= FIRMWARE_SOURCE_SPI_FLASH;
}

/* 从Flash读取参数，并初始化 */
void vFirmwareInit(void)
{
    cFlashReadDatas(FLASH_SYSTEM_DATA_ADDR, &st_typeFirmwareInfo, sizeof(st_typeFirmwareInfo));
    if(st_typeFirmwareInfo.crc == FIRMWARE_UNLOCK_CRC)
    {
        return;
    }

    if(st_typeFirmwareInfo.length < 1024)
    {
        if(st_typeFirmwareInfo.crc == usCRC16_MODBUS(NULL, &st_typeFirmwareInfo.boot, st_typeFirmwareInfo.length))
        {
            return;
        }
    }

    vFirmwareFactoryInit();
}

/* 把参数写入到Flash */
int8_t cFirmwareWrite(void)
{
    int8_t cError = 0;

    st_typeFirmwareInfo.length = sizeof(st_typeFirmwareInfo) - sizeof(uint32_t) * 4;
    st_typeFirmwareInfo.crc = usCRC16_MODBUS(NULL, &st_typeFirmwareInfo.boot, st_typeFirmwareInfo.length);

    /* 判断不一致再进行写入，避免重复刷新 */
    if(memcmp((void *)FLASH_SYSTEM_DATA_ADDR, &st_typeFirmwareInfo, sizeof(st_typeFirmwareInfo)) != 0)
    {
        cError = cFlashWriteDatas(FLASH_SYSTEM_DATA_ADDR, &st_typeFirmwareInfo, sizeof(st_typeFirmwareInfo));
    }

    return cError;
}

/* 把参数写入到Flash */
int8_t cFirmwareClear(void)
{
    int8_t cError = 0;

    memset(&st_typeFirmwareInfo, 0, sizeof(st_typeFirmwareInfo));
    vFirmwareFactoryInit();

    cError = cFlashWriteDatas(FLASH_SYSTEM_DATA_ADDR, &st_typeFirmwareInfo, sizeof(st_typeFirmwareInfo));

    return cError;
}

FirmwareInfoType *ptypeFirmwareInfoGet(void)
{
    return &st_typeFirmwareInfo;
}

uint32_t uiFirmwareCRCUpdate(FirmwarePortInfoType *pHandle)
{
    uint32_t uiAdddrNow = 0;
    int32_t iLength = 0, iLengthNow = 0;
    uint16_t usCRCValue = 0xFFFF;
    int8_t cError = 0;

    if(pHandle == NULL)
        return 1;

    if(pHandle->length > (1024 * 1024))
        return 2;

    uiAdddrNow = pHandle->address;
    iLength = pHandle->length;

    /* CRC计算 */
    while(iLength > 0)
    {
        iLengthNow = (iLength > sizeof(st_ucFirmwareBuff)) ? sizeof(st_ucFirmwareBuff) : iLength;

        if(pHandle->registers & FIRMWARE_SOURCE_SPI_FLASH)
            cError |= cSPIFlashReadDatas(uiAdddrNow, st_ucFirmwareBuff, iLengthNow);
        else
            cError |= cFlashReadDatas(uiAdddrNow, st_ucFirmwareBuff, iLengthNow);

        if(cError != 0)
        {
            break;
        }

        usCRCValue = usCRC16_MODBUS(&usCRCValue, st_ucFirmwareBuff, iLengthNow);

        uiAdddrNow += iLengthNow;
        iLength -= iLengthNow;
    }

    return usCRCValue;
}

int8_t cFirmwareUpdate(FirmwarePortInfoType *pTarget, FirmwarePortInfoType *pSource)
{
    uint32_t uiSourceAdddrNow = 0, uiTargetAdddrNow = 0, uiCRCValue = 0;
    int32_t iLength = 0, iLengthNow = 0;
    int8_t cError = 0;

    if((pSource == NULL) || (pTarget == NULL))
        return 1;

    /* CRC校验，判断源固件是否完整 */
    if((uiCRCValue = uiFirmwareCRCUpdate(pSource)) != pSource->crcValue)
    {
        cLogPrintfNormal("Source CRC:%08X CRC:%08X error.\r\n", uiCRCValue, pSource->crcValue);
        return 3;
    }

    /* CRC比较，判断是否是一样的固件，如果是一样的固件，就直接退出表示成功 */
    if(uiFirmwareCRCUpdate(pTarget) == pSource->crcValue)
        return 0;

    uiSourceAdddrNow = pSource->address;
    uiTargetAdddrNow = pTarget->address;
    iLength = pSource->length;

    cLogPrintfNormal("update source addr: 0x%08X, target addr: 0x%08X, file size: %d\r\n", pSource->address, pTarget->address, (int)(pSource->length));

    /* 升级覆盖 */
    while(iLength > 0)
    {
        iLengthNow = (iLength > sizeof(st_ucFirmwareBuff)) ? sizeof(st_ucFirmwareBuff) : iLength;

        if(pSource->registers & FIRMWARE_SOURCE_SPI_FLASH)
            cError |= cSPIFlashReadDatas(uiSourceAdddrNow, st_ucFirmwareBuff, iLengthNow);
        else
            cError |= cFlashReadDatas(uiSourceAdddrNow, st_ucFirmwareBuff, iLengthNow);

        if(pTarget->registers & FIRMWARE_SOURCE_SPI_FLASH)
            cError |= cSPIFlashWriteDatas(uiTargetAdddrNow, st_ucFirmwareBuff, iLengthNow);
        else
            cError |= cFlashWriteDatas(uiTargetAdddrNow, st_ucFirmwareBuff, iLengthNow);

        if(cError != 0)
        {
            return 4;
        }

        uiSourceAdddrNow += iLengthNow;
        uiTargetAdddrNow += iLengthNow;
        iLength -= iLengthNow;

        cLogPrintfNormal("update: %.2f%%\r\n", ((pSource->length - iLength) / (float)pSource->length) * 100.0f);
    }

    /* CRC比较，以判断是否已经更新成功 */
    pTarget->length = pSource->length;
    if(uiFirmwareCRCUpdate(pTarget) != pSource->crcValue)
    {
        cLogPrintfNormal("固件拷贝后，校验目标区域新固件CRC失败.\r\n");
        return 5;
    }

    return 0;
}

/* 跳转到指定位置执行 */
int8_t cFirmwareJumpTo(uint32_t uiAddress)
{
    typedef void (*pFunction)(void);
    volatile static uint32_t uiJumpAddress = 0, uiHeapData = 0;
    volatile static pFunction typeJumpToAPPlication = 0;

    uiHeapData = *(volatile uint32_t*)uiAddress;
    /* Test if user code is programmed starting from address "ApplicationAddress" */
    if((SRAM_BASE < uiHeapData) && (uiHeapData <= (SRAM_BASE + 1024 * 64)))
    {
        /* Jump to application */
        uiJumpAddress = *(volatile uint32_t*) (uiAddress + 4);
        typeJumpToAPPlication = (pFunction) uiJumpAddress;

        /* 恢复系统时钟到初始化状态 */
        HAL_DeInit();

        // 设置主堆栈指针（MSP）
        __set_MSP(uiHeapData);

        // 跳转到用户程序
        typeJumpToAPPlication();
    }

    return 1;
}

int8_t cFirmwareJump(FirmwarePortInfoType *pHandle)
{
    /* CRC校验，判断源固件是否完整 */
    if((pHandle->crcValue == FIRMWARE_UNLOCK_CRC) || (uiFirmwareCRCUpdate(pHandle) == pHandle->crcValue))
    {
        /* 程序跳转到目标地址执行 */
        cFirmwareJumpTo(pHandle->address);
    }

    return 1;
}

int8_t cFirmwareJumpBoot(void)
{
    /* 厂商 Bootloader 存储地址 */
    const uint32_t uiBootAddr = 0x1FFFF000;

    cFirmwareJumpTo(uiBootAddr);

    return 1;
}

int8_t cFirmwareJumpBootloader(void)
{
    if((st_typeFirmwareInfo.bootloaderOut.registers & FIRMWARE_UPDATE) == 0)
    {
        /* 进行程序跳转 */
        cFirmwareJump(&st_typeFirmwareInfo.bootloader);

        /* 程序跳转失败，置位更新标志，以提示后续更新 */
        st_typeFirmwareInfo.bootloaderOut.registers |= FIRMWARE_UPDATE;
    }

    return 1;
}

int8_t cFirmwareJumpAPP(void)
{
    if((st_typeFirmwareInfo.appOut.registers & FIRMWARE_UPDATE) == 0)
    {
        /* 进行程序跳转 */
        cFirmwareJump(&st_typeFirmwareInfo.app);

        /* 程序跳转失败，置位更新标志，以提示后续更新 */
        st_typeFirmwareInfo.appOut.registers |= FIRMWARE_UPDATE;
    }

    return 1;
}

int8_t cFirmwareUpdateBoot(void)
{
    uint32_t uiAddr = 0;
    int8_t cError = 1;

    if((st_typeFirmwareInfo.bootOut.registers & FIRMWARE_UPDATE) != 0)
    {
        st_typeFirmwareInfo.bootOut.registers &= ~FIRMWARE_UPDATE;

        cLogPrintfNormal("\n\rBoot 更新中.\r\n");

        /* 把外部备份区域更新到 boot */
        if(cFirmwareUpdate(&st_typeFirmwareInfo.boot, &st_typeFirmwareInfo.bootOut) == 0)
        {
            /* 更新目标固件参数（版本号） */
            uiAddr = st_typeFirmwareInfo.boot.address;
            st_typeFirmwareInfo.boot = st_typeFirmwareInfo.bootOut;
            st_typeFirmwareInfo.boot.address = uiAddr;
            st_typeFirmwareInfo.boot.registers &= ~FIRMWARE_SOURCE_SPI_FLASH;

            cError = 0;
        }

        cLogPrintfNormal("Boot 更新%s.\r\n", ((cError == 0) ? "成功" : "失败"));
    }

    return cError;
}

int8_t cFirmwareUpdateBootloader(void)
{
    uint32_t uiAddr = 0;
    int8_t cError = 1;

    if((st_typeFirmwareInfo.bootloaderOut.registers & FIRMWARE_UPDATE) != 0)
    {
        st_typeFirmwareInfo.bootloaderOut.registers &= ~FIRMWARE_UPDATE;

        cLogPrintfNormal("\n\rBootloader 更新中.\r\n");

        /* 把外部备份区域更新到 bootloader */
        if(cFirmwareUpdate(&st_typeFirmwareInfo.bootloader, &st_typeFirmwareInfo.bootloaderOut) == 0)
        {
            /* 更新目标固件参数（版本号） */
            uiAddr = st_typeFirmwareInfo.bootloader.address;
            st_typeFirmwareInfo.bootloader = st_typeFirmwareInfo.bootloaderOut;
            st_typeFirmwareInfo.bootloader.address = uiAddr;
            st_typeFirmwareInfo.bootloader.registers &= ~FIRMWARE_SOURCE_SPI_FLASH;

            cError = 0;
        }

        cLogPrintfNormal("Bootloader 更新%s.\r\n", ((cError == 0) ? "成功" : "失败"));
    }

    return cError;
}

int8_t cFirmwareUpdateAPP(void)
{
    uint32_t uiAddr = 0;
    int8_t cError = 1;

    if((st_typeFirmwareInfo.appOut.registers & FIRMWARE_UPDATE) != 0)
    {
        st_typeFirmwareInfo.appOut.registers &= ~FIRMWARE_UPDATE;

        cLogPrintfNormal("\n\rAPP 更新中.\r\n");

        /* 把外部备份区域更新到 app */
        if(cFirmwareUpdate(&st_typeFirmwareInfo.app, &st_typeFirmwareInfo.appOut) == 0)
        {
            /* 更新目标固件参数（版本号） */
            uiAddr = st_typeFirmwareInfo.app.address;
            st_typeFirmwareInfo.app = st_typeFirmwareInfo.appOut;
            st_typeFirmwareInfo.app.address = uiAddr;
            st_typeFirmwareInfo.app.registers &= ~FIRMWARE_SOURCE_SPI_FLASH;

            cError = 0;
        }

        cLogPrintfNormal("APP 更新%s.\r\n", ((cError == 0) ? "成功" : "失败"));
    }

    return cError;
}
