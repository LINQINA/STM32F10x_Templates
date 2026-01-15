#include "stm32f1xx_hal.h"
#include "stdint.h"
#include "stdio.h"
#include "string.h"

#include "DevicesFlash.h"

#include "DriverBootloader.h"

#include "version.h"


const char cVersionHardBuff[32] __attribute__ ((section(".ARM.__at_0x0800A800"))) = VERSION_BOOT_HARDWARE;
const char cVersionSoftBuff[32] __attribute__ ((section(".ARM.__at_0x0800A820"))) = VERSION_BOOT_SOFTWARE;
const char cVersionDateBuff[32] __attribute__ ((section(".ARM.__at_0x0800A840"))) = __DATE__" - "__TIME__;
const char cVersionTypeBuff[32] __attribute__ ((section(".ARM.__at_0x0800A860"))) = "MySTM32";
static char st_cVersionBuff[32] = {0};

/* 获取 Boot 版本信息 */
char *pcVersionBootHardGet(void)
{
    memcpy(st_cVersionBuff, (void *)(FLASH_BOOT_ADDR + 0x800), sizeof(st_cVersionBuff) - 1);

    return st_cVersionBuff;
}

char *pcVersionBootSoftGet(void)
{
    memcpy(st_cVersionBuff, (void *)(FLASH_BOOT_ADDR + 0x820), sizeof(st_cVersionBuff) - 1);

    return st_cVersionBuff;
}

char *pcVersionBootDateGet(void)
{
    memcpy(st_cVersionBuff, (void *)(FLASH_BOOT_ADDR + 0x840), sizeof(st_cVersionBuff) - 1);

    return st_cVersionBuff;
}

char *pcVersionBootTypeGet(void)
{
    memcpy(st_cVersionBuff, (void *)(FLASH_BOOT_ADDR + 0x860), sizeof(st_cVersionBuff) - 1);

    return st_cVersionBuff;
}


/* 获取 Bootloader 版本信息 */
char *pcVersionBootloaderHardGet(void)
{
    memcpy(st_cVersionBuff, (void *)(FLASH_BOOTLOADER_ADDR + 0x800), sizeof(st_cVersionBuff) - 1);

    return st_cVersionBuff;
}

char *pcVersionBootloaderSoftGet(void)
{
    memcpy(st_cVersionBuff, (void *)(FLASH_BOOTLOADER_ADDR + 0x820), sizeof(st_cVersionBuff) - 1);

    return st_cVersionBuff;
}

char *pcVersionBootloaderDateGet(void)
{
    memcpy(st_cVersionBuff, (void *)(FLASH_BOOTLOADER_ADDR + 0x840), sizeof(st_cVersionBuff) - 1);

    return st_cVersionBuff;
}

char *pcVersionBootloaderTypeGet(void)
{
    memcpy(st_cVersionBuff, (void *)(FLASH_BOOTLOADER_ADDR + 0x860), sizeof(st_cVersionBuff) - 1);

    return st_cVersionBuff;
}


/* 获取 APP 版本信息 */
char *pcVersionAPPHardGet(void)
{
    memcpy(st_cVersionBuff, (void *)(FLASH_APP_ADDR + 0x800), sizeof(st_cVersionBuff) - 1);

    return st_cVersionBuff;
}

char *pcVersionAPPSoftGet(void)
{
    memcpy(st_cVersionBuff, (void *)(FLASH_APP_ADDR + 0x820), sizeof(st_cVersionBuff) - 1);

    return st_cVersionBuff;
}

char *pcVersionAPPDateGet(void)
{
    memcpy(st_cVersionBuff, (void *)(FLASH_APP_ADDR + 0x840), sizeof(st_cVersionBuff) - 1);

    return st_cVersionBuff;
}

char *pcVersionAPPTypeGet(void)
{
    memcpy(st_cVersionBuff, (void *)(FLASH_APP_ADDR + 0x860), sizeof(st_cVersionBuff) - 1);

    return st_cVersionBuff;
}
