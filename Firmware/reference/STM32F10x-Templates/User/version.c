#include "stm32f1xx_hal.h"
#include "string.h"
#include "stdint.h"
#include "stdio.h"

#include "DevicesFlash.h"

#include "version.h"


/* 产品型号 */
static productType st_typeProduct = {
};

/* 固件地址存储 APP 版本信息 */
const char cVersionHardBuff[32] __attribute__ ((section(".ARM.__at_0x0801A800"))) = {0};
const char cVersionSoftBuff[32] __attribute__ ((section(".ARM.__at_0x0801A820"))) = PD_VERSION_APP_SOFTWARE;
const char cVersionDateBuff[32] __attribute__ ((section(".ARM.__at_0x0801A840"))) = __DATE__" - "__TIME__;
const char cVersionTypeBuff[32] __attribute__ ((section(".ARM.__at_0x0801A860"))) = "MySTM32";
const static char st_cVersionBuff[] = "00.00.00";



/* 获取 Boot 版本信息 */
char *pcVersionBootHardGet(void)
{
    /* 防止没有字符串没有 '\0' 结尾 */
    return (*(char *)(FLASH_BOOT_ADDR + 0x800 + 31) == 0) ? (char *)(FLASH_BOOT_ADDR + 0x800) : (char *)st_cVersionBuff;
}

char *pcVersionBootSoftGet(void)
{
    return (*(char *)(FLASH_BOOT_ADDR + 0x820 + 31) == 0) ? (char *)(FLASH_BOOT_ADDR + 0x820) : (char *)st_cVersionBuff;
}

char *pcVersionBootDateGet(void)
{
    return (*(char *)(FLASH_BOOT_ADDR + 0x840 + 31) == 0) ? (char *)(FLASH_BOOT_ADDR + 0x840) : (char *)st_cVersionBuff;
}

char *pcVersionBootTypeGet(void)
{
    return (*(char *)(FLASH_BOOT_ADDR + 0x860 + 31) == 0) ? (char *)(FLASH_BOOT_ADDR + 0x860) : (char *)st_cVersionBuff;
}


/* 获取 Bootloader 版本信息 */
char *pcVersionBootloaderHardGet(void)
{
    return (*(char *)(FLASH_BOOTLOADER_ADDR + 0x800 + 31) == 0) ? (char *)(FLASH_BOOTLOADER_ADDR + 0x800) : (char *)st_cVersionBuff;
}

char *pcVersionBootloaderSoftGet(void)
{
    return (*(char *)(FLASH_BOOTLOADER_ADDR + 0x820 + 31) == 0) ? (char *)(FLASH_BOOTLOADER_ADDR + 0x820) : (char *)st_cVersionBuff;
}

char *pcVersionBootloaderDateGet(void)
{
    return (*(char *)(FLASH_BOOTLOADER_ADDR + 0x840 + 31) == 0) ? (char *)(FLASH_BOOTLOADER_ADDR + 0x840) : (char *)st_cVersionBuff;
}

char *pcVersionBootloaderTypeGet(void)
{
    return (*(char *)(FLASH_BOOTLOADER_ADDR + 0x860 + 31) == 0) ? (char *)(FLASH_BOOTLOADER_ADDR + 0x860) : (char *)st_cVersionBuff;
}

char *pcVersionAPPSoftGet(void)
{
    return (*(char *)(FLASH_APP_ADDR + 0x820 + 31) == 0) ? (char *)(FLASH_APP_ADDR + 0x820) : (char *)st_cVersionBuff;
}

char *pcVersionAPPDateGet(void)
{
    return (*(char *)(FLASH_APP_ADDR + 0x840 + 31) == 0) ? (char *)(FLASH_APP_ADDR + 0x840) : (char *)st_cVersionBuff;
}

char *pcVersionAPPTypeGet(void)
{
    return (*(char *)(FLASH_APP_ADDR + 0x860 + 31) == 0) ? (char *)(FLASH_APP_ADDR + 0x860) : (char *)st_cVersionBuff;
}

/* 获取产品型号 */
productType *ptypeProductGet(void)
{
    return &st_typeProduct;
}

/* 产品型号信息更新 */
int8_t cProductInfoUpdate(void)
{
    productType *ptypeProduct = ptypeProductGet();
    static int8_t st_UpdateFlag = 0;

    if(st_UpdateFlag != 0)
        return 1;

    strncpy(ptypeProduct->versionBuff, pcVersionAPPSoftGet(), 16);

    /* 判断是否还需要更新 */
    st_UpdateFlag = 1;

    return 0;
}
