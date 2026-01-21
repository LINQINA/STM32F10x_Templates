#include "stm32f1xx_hal.h"
#include "taskSystem.h"
#include "DevicesDelay.h"
#include "DevicesSPI.h"
#include "DevicesSPIFlash.h"

static uint16_t st_usSPIFlashID = 0;


void vSPIFlashInit(void)
{
    uint32_t uiTimes;
    vSPI2Init();

    SPI_FLASH_CS_ENABLE();

    for(uiTimes = 0; uiTimes < 8; ++uiTimes)
    {
        st_usSPIFlashID = uiSPIFlashReadID();

        if((st_usSPIFlashID != 0x0000) && (st_usSPIFlashID != 0xFFFF))
            break;
        
        vDelayMs(10);
    }

    SPI_FLASH_CS_DISABLE();

}

static int8_t cSPIFlashLock(void)
{
//    xSemaphoreTakeRecursive(g_xSpiFlashSemaphore, portMAX_DELAY);

    return 0;
}

static int8_t cSPIFlashUnlock(void)
{
//    xSemaphoreGiveRecursive(g_xSpiFlashSemaphore);

    return 0;
}



uint32_t uiSPIFlashReadID(void)
{
    uint16_t usID = 0xFFFF;
    uint8_t ucDatas[4] = {READ_ID_CMD,0x00,0x00,0x00};

    cSPIFlashLock();

    SPI_FLASH_CS_ENABLE();

    cSPIxWriteDatas(ucDatas,4);

    usID = ucSPIxWriteReadByte(0xFF) << 8;
    usID |= ucSPIxWriteReadByte(0xFF);

    SPI_FLASH_CS_DISABLE();

    cSPIFlashUnlock();

    return usID;
}

static uint8_t ucSPIFlashReadStatus(uint8_t ucRegNo)
{
    uint32_t uiTimeout = 20000;
    uint8_t ucReadStatusCMD;
    uint8_t ucStatus;
    
    switch(ucRegNo)
    {
        case 1 : ucReadStatusCMD = READ_STATUS_REG1_CMD; break;
        case 2 : ucReadStatusCMD = READ_STATUS_REG2_CMD; break;
        case 3 : ucReadStatusCMD = READ_STATUS_REG3_CMD; break;

        default : ucReadStatusCMD = READ_STATUS_REG1_CMD; break;
    }
    
    /* 判断 SPI Flash 是否连接正常 */
    if((st_usSPIFlashID == 0x0000) || (st_usSPIFlashID == 0xFFFF))
        return 0xFF;
    
    SPI_FLASH_CS_ENABLE();
    
    ucSPIxWriteReadByte(ucReadStatusCMD);
    
    /* 等待Flash读取完毕 */
    while(uiTimeout--)
    {
        ucStatus = ucSPIxWriteReadByte(0xFF);
        
        if((ucStatus & 0x01) != 0x01)
            break;
        
        vDelayUs(10);
        
    }

    SPI_FLASH_CS_DISABLE();
    
    return ucStatus;
}

static int8_t cSPIStatusBusy(void)
{
    return (ucSPIFlashReadStatus(0x01) & 0x01) == 0x01;
}

static void uvSPIFlashEnableWrite(void)
{
    SPI_FLASH_CS_ENABLE();

    ucSPIxWriteReadByte(WRITE_ENABLE_CMD);

    SPI_FLASH_CS_DISABLE();
}

int8_t cSPIFlashErases(uint32_t uiAddress)
{
    uint8_t ucAddress[3] = {uiAddress >> 16, uiAddress >> 8, uiAddress};
    int8_t cError = 0;

    cSPIFlashLock();

    /* 等待Flash退出忙状态 */
    if(cSPIStatusBusy())
    {
        cError = 1;
        goto __EXIT;
    }

    uvSPIFlashEnableWrite();

    /* 等待Flash退出忙状态 */
    if(cSPIStatusBusy())
    {
        cError = 2;
        goto __EXIT;
    }

    SPI_FLASH_CS_ENABLE();

    ucSPIxWriteReadByte(SUBSECTOR_ERASE_CMD);
    cError |= cSPIxWriteDatas(ucAddress, 3);

    SPI_FLASH_CS_DISABLE();

__EXIT:
    cSPIFlashUnlock();

    return cError;
}

static int8_t cSPIFlashWritePage(uint32_t uiAddress, uint8_t *pucDatas, int32_t iLength)
{
    uint8_t ucAddress[3] = {uiAddress >> 16, uiAddress >> 8, uiAddress};
    int8_t cError = 0;

    /* 等待Flash退出忙状态 */
    if(cSPIStatusBusy())
        return 1;

    uvSPIFlashEnableWrite();

    /* 等待Flash退出忙状态 */
    if(cSPIStatusBusy())
        return 2;

    SPI_FLASH_CS_ENABLE();

    ucSPIxWriteReadByte(PAGE_PROG_CMD);
    cError |= cSPIxWriteDatas(ucAddress, 3);

    /* 写入数据 */
    cError |= cSPIxWriteDatas(pucDatas, iLength);

    SPI_FLASH_CS_DISABLE();

    return cError;
}

int8_t cSPIFlashReadDatas(uint32_t uiAddress, void *pvBuff, int32_t iLength)
{
    uint8_t ucAddress[3] = {uiAddress >> 16, uiAddress >> 8, uiAddress};
    int8_t cError;

    cSPIFlashLock();

    /* 等待Flash退出忙状态 */
    if(cSPIStatusBusy())
    {
        cError = 1;
        goto __EXIT;
    }

    SPI_FLASH_CS_ENABLE();

    ucSPIxWriteReadByte(READ_CMD);
    cError = cSPIxWriteDatas(ucAddress, 3);

    /* 读取数据 */
    cError |= cSPIxReadDatas(pvBuff, iLength);

    SPI_FLASH_CS_DISABLE();

__EXIT:
    cSPIFlashUnlock();

    return cError;
}

int8_t cSPIFlashWriteDatas(uint32_t uiAddress, const void *pvBuff, int32_t iLength)
{
    int32_t iLengthTemp = 0;
    uint8_t *pucDatas = (uint8_t *)pvBuff;
    int8_t cError = 0;

    cSPIFlashLock();

    /* Write Flash */
    while(iLength > 0)
    {
        /* 块起始地址时，需要先擦除该页 */
        if((uiAddress % SPI_FLASH_SECTOR_SIZE) == 0)
        {
            if(cSPIFlashErases(uiAddress) != 0)
            {
                cError |= 1;
                break;
            }
        }

        /* 页对齐 */
        iLengthTemp = (iLength > (SPI_FLASH_PAGE_SIZE - (uiAddress % SPI_FLASH_PAGE_SIZE))) ? (SPI_FLASH_PAGE_SIZE - (uiAddress % SPI_FLASH_PAGE_SIZE)) : iLength;

        if(cSPIFlashWritePage(uiAddress, pucDatas, iLengthTemp) != 0)
        {
            cError |= 2;
            break;
        }

        uiAddress += iLengthTemp;
        iLength -= iLengthTemp;
        pucDatas += iLengthTemp;
    }

    cSPIFlashUnlock();

    return cError;
}

/* 擦除整个SPIFlash */
int8_t cSPIFlashErasesChip()
{
    int8_t cError = 0;

    cSPIFlashLock();

    /* 等待Flash退出忙状态 */
    if(cSPIStatusBusy())
    {
        cError = 1;
        goto __EXIT;
    }

    uvSPIFlashEnableWrite();

    /* 等待Flash退出忙状态 */
    if(cSPIStatusBusy())
    {
        cError = 2;
        goto __EXIT;
    }

    SPI_FLASH_CS_ENABLE();

    ucSPIxWriteReadByte(SUBCHIP_ERASE_CMD);

    SPI_FLASH_CS_DISABLE();
    
    while(cSPIStatusBusy())
        vDelayMs(10);

__EXIT:
    cSPIFlashUnlock();

    return cError;
}
