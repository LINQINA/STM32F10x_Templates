#ifndef _DriverOTA_H_
#define _DriverOTA_H_

#include "DevicesModbus.h" 

#define FILE_MAX_COUNT          8

#define OTA_Type_PD                                     0x00

extern int8_t g_cOTAInitFlag;
extern uint16_t g_usOTAUpdatingFirmwareNo;                  /* OTA本机 当前升级到固件序号,用于显示OTA升级时左下角的固件序号 */
extern uint32_t g_uiFirmwareLengthNow;                      /* OTA本机 固件当前写入的长度 */

typedef enum {
    OTA_Error_NULL                      = 0,                /* 无错误 */
    OTA_Error_CrcTotalHeaderError       = 0x00000001,       /* 总头检验错误 */
    OTA_Error_OTADeinitError            = 0x00000002,       /* OTA初始化失败 */
    OTA_Error_CrcTotalFirmwareError     = 0x00000002,       /* 整包校验错误 */
    OTA_Error_CrcChildHeaderError       = 0x00000004,       /* 子包校验错误 */
    OTA_Error_ReSendCountIsNull         = 0x00000008,       /* 升级次数为0 */
    OTA_Error_ChildUpdateFail           = 0x00000010,       /* 子固件升级失败 */
} OTAErrorEnum;


typedef enum
{
    OTA_STATE_DISABLE = 0,       /* 关闭 */
    OTA_STATE_START,             /* 开始升级 */
    OTA_STATE_READY,             /* 升级就绪 */
    OTA_STATE_UPDATING,          /* 升级中 */
    OTA_STATE_SUCCESS,           /* 升级成功 */
    OTA_STATE_FAIL,              /* 升级失败 */
    OTA_STATE_CHECK_VERSION,     /* 校验版本 */
} OTAStateEnum;


/* 各固件的地址与长度 */
typedef struct
{
    uint32_t startAddr;         /* 固件起始地址 */
    uint32_t length;            /* 固件长度 */
} OTAFirmwarePortType;


/* 固件总头信息 */
typedef struct
{
    uint8_t FileType[4];        /* 头部固定信息："DBS" */

    uint32_t registers;         /* 寄存器： */
                                /* bit0-bit1（预留）） */
                                /* bit2（0（不需要更新）、1（需要更新）） */
                                /* bit3-bit4（Hasn校验类型：0（无校验）、1（CRC32）、2（MD5）、3（SHA1）） */

    uint32_t length;            /* 固件长度 */
    uint32_t crc32;             /* 固件CRC32校验值 */
    uint8_t hash[32];           /* Hash校验值 */
    char versionSoft[16];       /* 固件包版本号 */
    char versionHard[16];       /* 硬件版本号 */
    uint8_t firmwareNumber;     /* 子固件数量 */
    uint8_t reSendCount;        /* 重传次数 */
    uint8_t reserved[46];       /* 预留 */
    OTAFirmwarePortType FirmwareOTAPort[FILE_MAX_COUNT];                    /* 后 128Byte存储各 子固件 的地址、长度信息 */
    uint8_t usreserved[124 - sizeof(OTAFirmwarePortType) * FILE_MAX_COUNT]; /* 预留*/
    uint32_t crc32Head;         /* 存储前面252个字节的校验值 */
} OTAFirmwareTotalInfoType;


/* 子模块信息 */
typedef struct
{
    uint16_t type;                                                      /* OTA子模块 升级类型 */
    uint16_t number;                                                    /* OTA子模块 固件模块序号 */
    OTAStateEnum state;                                                 /* OTA子模块 状态 */
    uint16_t ReSendCount;                                               /* OTA子模块 升级次数 */
    uint32_t error;                                                     /* OTA子模块 故障信息 */
    char OTAPortVersion[16];                                            /* OTA子模块 版本信息 */
    uint32_t address;                                                   /* OTA子模块 固件起始地址 */
    uint32_t length;                                                    /* OTA子模块 固件长度 */
    uint32_t writeLengthNow;                                            /* OTA子模块 固件已经写入的长度 */
    uint32_t crcValue;                                                  /* OTA子模块 CRC固件校验值 */

} OTAFirmwarePortInfoType;


/* 产品OTA相关信息 */
typedef struct
{
    OTAFirmwarePortInfoType Port[FILE_MAX_COUNT];                       /* OTA子模块信息 */
    uint8_t firmwareNumber;                                             /* OTA本机 升级数量 */
    OTAStateEnum  state;                                                /* OTA本机 状态 */
    uint32_t error;                                                     /* OTA本机 故障信息 */
    uint16_t OTARemainTime;                                             /* OTA本机 升级剩余时间 */
    char OTATuyaAppVersion[16];                                         /* OTA本机 当前APP的涂鸦版本 */
    char OTATuyaFirmwareVersion[16];                                    /* OTA本机 后续要写入的涂鸦版本 */
    uint32_t FirmwareLengthTotal;                                       /* OTA本机 总固件长度 */
    uint32_t OTATotalInfoCrc;                                           /* OTA本机 总头CRC信息 */
} OTAInfoType;


void vOTAInit(void);
void vOTAStart();
void vOTAFirmwareUpdateAll(void);
void vCheckFirmwareVersion();
int8_t cOTAStateSet(OTAStateEnum enumOTAState);
int8_t cOTAIOTFrimwareDataRecever(uint8_t *pData, unsigned long position, unsigned short length);
int8_t cOTAModbusPackAnalysis(ModBusRtuTypeDef* ptypeHandle);
int8_t cOTAReafOTAInfoFromFlash(void);
OTAInfoType *ptypeOTAInfoGet(void);

#endif
