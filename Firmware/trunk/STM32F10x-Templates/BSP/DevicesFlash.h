#ifndef _DevicesFlash_H_
#define _DevicesFlash_H_

#define FLASH_USER_PAGE_SIZE        ((uint32_t)2048)
#define FLASH_BASE_ADDR             ((uint32_t)0x08000000)
#define FLASH_USER_MAX_ADDR         (FLASH_BASE_ADDR + (512 * 1024))

/* size: 16K Byte */
#define FLASH_BOOT_ADDR             (FLASH_BASE_ADDR + 1024 * (0))
/* size: 2K Byte */
#define FLASH_SYSTEM_DATA_ADDR      (FLASH_BASE_ADDR + 1024 * (32))
/* size: 2K Byte */
#define FLASH_OTA_DATA_ADDR         (FLASH_BASE_ADDR + 1024 * (32 + 2))
/* size: 2K Byte */
#define FLASH_OTP_DATA_ADDR         (FLASH_BASE_ADDR + 1024 * (32 + 2 + 2))
/* size: 2K Byte */
#define FLASH_USER_DATA_ADDR        (FLASH_BASE_ADDR + 1024 * (32 + 2 + 2 + 2))
/* size: 64K Byte */
#define FLASH_BOOTLOADER_ADDR       (FLASH_BASE_ADDR + 1024 * (32 + 2 + 2 + 2 + 2))
/* size: 剩余空间 */
#define FLASH_APP_ADDR              (FLASH_BASE_ADDR + 1024 * (32 + 2 + 2 + 2 + 2 + 64))

int8_t cFlashWriteDatas(uint32_t uiAddress, const void *pvBuff, int32_t iLength);
int8_t cFlashReadDatas(uint32_t uiAddress, void *pvBuff, int32_t iLength);

#endif /* _DEVICES_FLASH_H_ */
