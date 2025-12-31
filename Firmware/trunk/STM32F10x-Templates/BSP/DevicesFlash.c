#include "stm32f1xx_hal.h"
#include "stdint.h"
#include "stdio.h"
#include "string.h"

#include "DevicesFlash.h"

#include "DriverLogPrintf.h"

#include "taskSystem.h"

int8_t cFlashWriteDatas(uint32_t uiAddress, const void *pvBuff, int32_t iLength)
{
    FLASH_EraseInitTypeDef stEraseInit;
    uint32_t uiPageError;
    uint16_t *pusDataAddress = (uint16_t *)pvBuff;
    int8_t cError = 0, cCnt = 0;

    if((iLength < 1) || ((uiAddress + iLength) > FLASH_USER_MAX_ADDR))
    {
        cLogPrintfError("cFlashWriteDatas uiAddress: %08X iLength: %d >= FLASH_USER_MAX_ADDR.\r\n", uiAddress, iLength);
        return 1;
    }

    /* ¼Ó»¥³âËø */
    xSemaphoreTakeRecursive(g_xChipFlashSemaphore, portMAX_DELAY);

    /* ½âËøFlash */
    HAL_FLASH_Unlock();

    /* Çå³ý±êÖ¾Î» */
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPERR);

    while(iLength > 0)
    {
        /* Ò³²Á³ý */
        if((uiAddress % FLASH_USER_PAGE_SIZE) == 0)
        {
            stEraseInit.TypeErase = FLASH_TYPEERASE_PAGES;
            stEraseInit.PageAddress = uiAddress;
            stEraseInit.NbPages = 1;

            cCnt = 8;
            while((HAL_FLASHEx_Erase(&stEraseInit, &uiPageError) != HAL_OK) && (--cCnt))
            {
                __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPERR);
            }

            if(cCnt == 0)
            {
                cLogPrintfError("cFlashWriteDatas HAL_FLASHEx_Erase addr: 0x%08X error.\r\n", (unsigned int)uiAddress);
                cError |= 2;
                break;
            }
        }
        
        /* Ð´Èë°ë×Ö */
        if((*(volatile uint16_t*)uiAddress != *pusDataAddress) &&
           (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, uiAddress, *pusDataAddress) != HAL_OK))
        {
            cLogPrintfError("cFlashWriteDatas HAL_FLASH_Program addr: 0x%08X error!\r\n", (unsigned int)uiAddress);
            cError |= 4;
            break;
        }

        __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPERR);

        ++pusDataAddress;
        uiAddress += 2;
        iLength -= 2;
    }

    HAL_FLASH_Lock();
    xSemaphoreGiveRecursive(g_xChipFlashSemaphore);

    return cError;
}

int8_t cFlashReadDatas(uint32_t uiAddress, void *pvBuff, int32_t iLength)
{
    if((iLength < 1) || ((uiAddress + iLength) > FLASH_USER_MAX_ADDR))
    {
        cLogPrintfError("cFlashReadDatas uiAddress: %08X iLength: %d >= FLASH_USER_MAX_ADDR.\r\n", uiAddress, iLength);
        return 1;
    }

    memcpy(pvBuff, (const void *)uiAddress, iLength);

    return 0;
}
