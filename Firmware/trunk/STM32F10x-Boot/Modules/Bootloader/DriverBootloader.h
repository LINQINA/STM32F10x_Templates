#ifndef _DriverBootloader_H_
#define _DriverBootloader_H_



/* 来源内部Flash */
#define FIRMWARE_SOURCE_ROM       0x00
/* 来源外部SPI Flash */
#define FIRMWARE_SOURCE_SPI_FLASH 0x01
/* 是否需要更新 */
#define FIRMWARE_UPDATE           0x04
/* 未开启CRC校验 */
#define FIRMWARE_UNLOCK_CRC       0x12344321


/* 开启1字节对齐 */
#pragma pack(1)

typedef struct{
    char sign[4];           /* 标识头，固件为：“FILE” */
    uint16_t type;          /* 类型：0（PD）、1（BMS）、2（INV）、0x11（PD_Mini）、0x20（MPPT） */
    uint16_t number;        /* 类型子序号：0（APP）、1（Bootloader）、2（Boot） */
    /* 寄存器： */
    /* bit0-bit1（固件存储位置：0（芯片内部Flash）、1（外部SPI Flash）、2（预留）、3（预留）） */
    /* bit2（0（不需要更新）、1（需要更新）） */
    uint32_t registers;

    uint32_t address;       /* 起始地址 */
    uint32_t length;        /* 固件长度 */
    uint32_t crcValue;      /* 固件CRC16-Modbus校验值（以小端模式，存储在低16位） */

    char versionSoft[16];   /* 软件版本号 */
    char versionHard[16];   /* 硬件版本号 */

    /* 预留 */
    uint8_t reserved[64 - (sizeof(char) * (4 + 16 + 16) + sizeof(uint16_t) * 2 + sizeof(uint32_t) * 4)];
}FirmwarePortInfoType;

typedef struct{
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
}FirmwareInfoType;

/* 取消1字节对齐，恢复为默认4字节对齐 */
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

int8_t cFirmwareJumpFactoryBootloader(void);
int8_t cFirmwareUpdateALLForce(void);



#endif
