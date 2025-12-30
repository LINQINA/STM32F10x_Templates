#ifndef _DriverBootloader_H_
#define _DriverBootloader_H_

/* 固件存储位置：内部 Flash */
#define FIRMWARE_SOURCE_ROM       0x00
/* 固件存储位置：外部 SPI Flash */
#define FIRMWARE_SOURCE_SPI_FLASH 0x01
/* 固件是否需要更新 */
#define FIRMWARE_UPDATE           0x04
/* 未加锁 CRC 校验值 */
#define FIRMWARE_UNLOCK_CRC       0x12344321

/* 结构体按 1 字节对齐 */
#pragma pack(1)

typedef struct {
    char sign[4];           /* 固件标识头，固定为 "FILE" */
    uint16_t type;          /* 类型：0=PD，1=BMS，2=INV，0x11=PD_Mini，0x20=MPPT */
    uint16_t number;        /* 编号：0=APP，1=Bootloader，2=Boot */
    /* 寄存器说明 */
    /* bit0-bit1 固件存储位置：0=芯片内部 Flash，1=外部 SPI Flash，2=预留，3=预留 */
    /* bit2：0=不需要更新，1=需要更新 */
    uint32_t registers;

    uint32_t address;       /* 固件起始地址 */
    uint32_t length;        /* 固件长度 */
    uint32_t crcValue;      /* 固件 CRC16-Modbus 校验值，小端模式，存储在低 16 位 */

    char versionSoft[16];   /* 软件版本号 */
    char versionHard[16];   /* 硬件版本号 */

    /* 预留空间 */
    uint8_t reserved[64 - (sizeof(char) * (4 + 16 + 16) + sizeof(uint16_t) * 2 + sizeof(uint32_t) * 4)];
} FirmwarePortInfoType;

typedef struct {
    uint32_t type;
    uint32_t length;
    uint32_t crc;
    uint32_t rec;

    FirmwarePortInfoType boot;
    FirmwarePortInfoType bootloader;
    FirmwarePortInfoType app;
    FirmwarePortInfoType rec0;

    FirmwarePortInfoType bootOut;
    FirmwarePortInfoType bootloaderOut;
    FirmwarePortInfoType appOut;
    FirmwarePortInfoType rec1;
} FirmwareInfoType;

/* 恢复默认 4 字节对齐 */
#pragma pack()

void vFirmwareInit(void);
int8_t cFirmwareWrite(void);
int8_t cFirmwareClear(void);
FirmwareInfoType *ptypeFirmwareInfoGet(void);
void vFirmwareInfoSet(uint32_t uiChannel, uint32_t uiRegisters);
void vFirmwareInfoReset(uint32_t uiChannel, uint32_t uiRegisters);

uint32_t uiFirmwareCRCUpdate(FirmwarePortInfoType *pHandle);
int8_t cFirmwareUpdate(FirmwarePortInfoType *pTarget, FirmwarePortInfoType *pSource);

int8_t cFirmwareUpdateBoot(void);
int8_t cFirmwareJumpBoot(void);

int8_t cFirmwareUpdateBootloader(void);
int8_t cFirmwareJumpBootloader(void);

int8_t cFirmwareUpdateAPP(void);
int8_t cFirmwareJumpAPP(void);

#endif
