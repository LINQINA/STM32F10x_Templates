#ifndef DEVICESSPIFLASH_H_
#define DEVICESSPIFLASH_H_


#define SPI_FLASH_BASE_ADDR             0x00000000ul
/* size: 16k Byte */
#define SPI_FLASH_BOOT_BACK_ADDR        (SPI_FLASH_BASE_ADDR + 1024 * (0))
/* size: 4k Byte */
#define SPI_FLASH_SYSTEM_DATA_ADDR      (SPI_FLASH_BASE_ADDR + 1024 * (16))
/* size: 4k Byte */
#define SPI_FLASH_USER_DATA_ADDR        (SPI_FLASH_BASE_ADDR + 1024 * (16 + 4))
/* size: 64k Byte */
#define SPI_FLASH_BOOTLOADER_BACK_ADDR  (SPI_FLASH_BASE_ADDR + 1024 * (16 + 4 + 4))
/* size: 424k Byte */
#define SPI_FLASH_APP_BACK_ADDR         (SPI_FLASH_BASE_ADDR + 1024 * (16 + 4 + 4 + 64))
/* size: 1M Byte */
#define SPI_FLASH_OTA_ADDR              (SPI_FLASH_BASE_ADDR + 1024 * (16 + 4 + 4 + 64 + 424))
/* size: 64K Byte（预留） */
#define SPI_FLASH_RESERVED_ADDR         (SPI_FLASH_BASE_ADDR + 1024 * (16 + 4 + 4 + 64 + 424 + 1024))
/* size: 2M+ Byte */
#define SPI_FLASH_LOG_ADDR              (SPI_FLASH_BASE_ADDR + 1024 * (16 + 4 + 4 + 64 + 424 + 1024 + 64))

/* 读取芯片ID */
#define READ_ID_CMD                             0x90
/* 读取状态1寄存器 */
#define READ_STATUS_REG1_CMD                    0x05
/* 读取状态2寄存器 */
#define READ_STATUS_REG2_CMD                    0x35
/* 读取状态3寄存器 */
#define READ_STATUS_REG3_CMD                    0x15

/* 读取数据 */
#define READ_CMD                                0x03

/* 写使能开启 */
#define WRITE_ENABLE_CMD                        0x06
/* 写使能关闭 */
#define WRITE_DISABLE_CMD                       0x04

/* 页写操作 */
#define PAGE_PROG_CMD                           0x02

/* Erase Operations */
#define SUBSECTOR_ERASE_CMD                     0x20
#define SUBCHIP_ERASE_CMD                       0xC7 



/* 扇区大小 */
#define SPI_FLASH_SECTOR_SIZE        4096
/* 页大小 */
#define SPI_FLASH_PAGE_SIZE          256

/* 需要提供的SPI操作函数接口 */
#define SPI_FLASH_CS_ENABLE     SET_SPI2_NSS_LOW
#define SPI_FLASH_CS_DISABLE    SET_SPI2_NSS_HIGH
#define ucSPIxWriteReadByte     ucSPI2WriteReadByte
#define cSPIxWriteDatas         cSPI2WriteDatas
#define cSPIxReadDatas          cSPI2ReadDatas

void vSPIFlashInit(void);
int8_t cSPIFlashErases(uint32_t uiAddress);
int8_t cSPIFlashNoErasesWriteDatas(uint32_t uiAddress, const void *pvBuff, int32_t iLength);
int8_t cSPIFlashWriteDatas(uint32_t uiAddress, const void *pvBuff, int32_t iLength);
int8_t cSPIFlashReadDatas(uint32_t uiAddress, void *pvBuff, int32_t iLength);
uint32_t uiSPIFlashReadID(void);

#endif
