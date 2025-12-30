#ifndef _DriverOTA_H_
#define _DriverOTA_H_

#include "DevicesModbus.h"

#define FILE_MAX_COUNT          8

#define OTA_Type_PD             0x00

extern int8_t g_cOTAInitFlag;
extern uint16_t g_usOTAUpdatingFirmwareNo;      /* OTA状态 当前正在更新的固件编号，用于显示OTA进度时右下角的固件编号 */
extern uint32_t g_uiFirmwareLengthNow;           /* OTA状态 固件当前写入的长度 */

typedef enum 
{
    OTA_Error_NULL                      = 0,            /* 无错误 */
    OTA_Error_CrcTotalHeaderError       = 0x00000001,   /* 总头CRC校验错误 */
    OTA_Error_OTADeinitError            = 0x00000002,   /* OTA初始化失败 */
    OTA_Error_CrcTotalFirmwareError     = 0x00000002,   /* 总固件CRC校验错误 */
    OTA_Error_CrcChildHeaderError       = 0x00000004,   /* 子包CRC校验错误 */
    OTA_Error_ReSendCountIsNull         = 0x00000008,   /* 重传次数为0 */
    OTA_Error_ChildUpdateFail           = 0x00000010,   /* 子固件更新失败 */
} OTAErrorEnum;

typedef enum 
{
    OTA_STATE_DISABLE = 0,       /* 关闭 */
    OTA_STATE_START,             /* 开始更新 */
    OTA_STATE_READY,             /* 更新准备就绪 */
    OTA_STATE_UPDATING,          /* 更新中 */
    OTA_STATE_SUCCESS,           /* 更新成功 */
    OTA_STATE_FAIL,              /* 更新失败 */
    OTA_STATE_CHECK_VERSION,     /* 校验版本 */
} OTAStateEnum;

/* 固件的地址和长度 */
typedef struct 
{
    uint32_t startAddr;          /* 固件起始地址 */
    uint32_t length;             /* 固件长度 */
} OTAFirmwarePortType;

/* 固件总头信息 */
typedef struct 
{
    uint8_t FileType[4];         /* 固定文件头信息，如 "DBS" */

    uint32_t registers;          /* 寄存器信息 */
                                /* bit0-bit1：预留 */
                                /* bit2：0-不需要更新，1-需要更新 */
                                /* bit3-bit4：Hash校验方式（0-无校验，1-CRC32，2-MD5，3-SHA1） */

    uint32_t length;             /* 固件长度 */
    uint32_t crc32;              /* 固件CRC32校验值 */
    uint8_t hash[32];            /* Hash校验值 */
    char versionSoft[16];        /* 固件软件版本 */
    char versionHard[16];        /* 固件硬件版本 */
    uint8_t firmwareNumber;      /* 子固件数量 */
    uint8_t reSendCount;         /* 重传次数 */
    uint8_t reserved[46];        /* 预留 */
    OTAFirmwarePortType FirmwareOTAPort[FILE_MAX_COUNT];  /* 固件的地址和长度信息 */
    uint8_t usreserved[124 - sizeof(OTAFirmwarePortType) * FILE_MAX_COUNT]; /* 预留 */
    uint32_t crc32Head;          /* 前252字节的CRC校验值 */
} OTAFirmwareTotalInfoType;

/* OTA子模块信息 */
typedef struct 
{
    uint16_t type;               /* 子模块类型（对应设备类型） */
    uint16_t number;             /* 子模块固件编号 */
    OTAStateEnum state;          /* 子模块OTA状态 */
    uint16_t ReSendCount;        /* 子模块剩余重传次数 */
    uint32_t error;              /* 子模块错误信息 */
    char OTAPortVersion[16];     /* 子模块版本信息 */
    uint32_t address;            /* 子模块固件起始地址 */
    uint32_t length;             /* 子模块固件长度 */
    uint32_t writeLengthNow;     /* 子模块固件已写入长度 */
    uint32_t crcValue;           /* 子模块固件CRC校验值 */
} OTAFirmwarePortInfoType;

/* 产品OTA总体信息 */
typedef struct 
{
    OTAFirmwarePortInfoType Port[FILE_MAX_COUNT]; /* 子模块信息 */
    uint8_t firmwareNumber;                       /* 固件总数量 */
    OTAStateEnum state;                           /* OTA总体状态 */
    uint32_t error;                               /* OTA总体错误信息 */
    uint16_t OTARemainTime;                       /* OTA剩余时间 */
    char OTATuyaAppVersion[16];                   /* 当前APP端涂鸦版本 */
    char OTATuyaFirmwareVersion[16];              /* 要写入的涂鸦版本 */
    uint32_t FirmwareLengthTotal;                 /* 总固件长度 */
    uint32_t OTATotalInfoCrc;                     /* OTA总头CRC信息 */
} OTAInfoType;

void vOTAInit(void);
void vOTAStart(void);
void vOTAFirmwareUpdateAll(void);
void vCheckFirmwareVersion(void);
int8_t cOTAStateSet(OTAStateEnum enumOTAState);
int8_t cOTAIOTFrimwareDataRecever(uint8_t *pData, unsigned long position, unsigned short length);
int8_t cOTAModbusPackAnalysis(ModBusRtuTypeDef *ptypeHandle);
int8_t cOTAReafOTAInfoFromFlash(void);
OTAInfoType *ptypeOTAInfoGet(void);

#endif
