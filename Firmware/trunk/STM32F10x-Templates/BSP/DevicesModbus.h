#ifndef _DevicesModbus_H_
#define _DevicesModbus_H_

#define MODBUS_ADDRESS_PD           0x10    /* 主机PD */
#define MODBUS_ADDRESS_BMS          0x96    /* BMS */
#define MODBUS_ADDRESS_INV0         0x01    /* 谐振INV */

typedef enum {
    MODBUS_CODE_0x03 = 0x03,        /* 读取多个寄存器的值 */
    MODBUS_CODE_0x06 = 0x06,        /* 写入单个寄存器的值 */
    MODBUS_CODE_0x10 = 0x10,        /* 写入多个寄存器的值 */
} ModbusCodeEnum;

typedef enum {
    MODBUS_UNPACK_ADDRESS = 0,      /* 设备地址 */
    MODBUS_UNPACK_FUNC,             /* 功能码 */
    MODBUS_UNPACK_ERROR_CODE,       /* 错误码 */
    MODBUS_UNPACK_DATA,             /* 数据区域 */
    MODBUS_UNPACK_CRC_HIGH,         /* 校验码高位 */
    MODBUS_UNPACK_CRC_LOW,          /* 校验码低位 */
    MODBUS_UNPACK_SUCCEED,          /* 解析成功 */
    MODBUS_UNPACK_ERROR,            /* 解析失败 */
} ModbusUnpackStateEnum;

typedef struct
{
    uint8_t slaveAddress;           /* 设备地址 */
    uint8_t func;                   /* 功能码 */
    uint8_t data[256];              /* 数据 */
    uint8_t crc16[2];               /* 校验码 */

    uint8_t length;                 /* 当前解析长度 */
    uint8_t state;                  /* 解析状态 */
    uint8_t rec[2];

    uint32_t timeIdle;              /* 空闲时间 */
} ModBusRtuTypeDef;

int8_t cModbusPackRTU_03(uint8_t ucSalaveAdd, uint16_t usRegisterAddr, uint16_t usLength, void *pvBuff);
int8_t cModbusPackRTU_06(uint8_t ucSalaveAdd, uint16_t usRegisterAddr, uint16_t usValue, void *pvBuff);
int8_t cModbusPackRTU_10(uint8_t ucSalaveAdd, uint16_t usRegisterAddr, uint16_t usLength, void *pvWriteBuff, void *pvBuff);

int8_t cModbusPackReplyRTU_03(uint8_t ucSalaveAdd, uint16_t usLength, void *pvWriteBuff, void *pvBuff);
int8_t cModbusPackReplyRTU_06(uint8_t ucSalaveAdd, uint16_t usRegisterAddr, uint16_t usValue, void *pvBuff);
int8_t cModbusPackReplyRTU_10(uint8_t ucSalaveAdd, uint16_t usRegisterAddr, uint16_t usLength, void *pvBuff);
int8_t cModbusPackReplyRTU_ErrorCode(uint8_t ucSalaveAdd, uint8_t ucFunc, uint8_t ucErrorCode, void *pvBuff);

ModbusUnpackStateEnum enumModbusReplyUnpack(ModBusRtuTypeDef *ptypeData, uint8_t ucValue);
ModbusUnpackStateEnum enumModbusUnpack(ModBusRtuTypeDef *ptypeData, uint8_t ucValue);

ModbusUnpackStateEnum enumModbusReplyUnpackDatas(ModBusRtuTypeDef *ptypeHandle, void *pvBuff, int32_t iLength);
ModbusUnpackStateEnum enumModbusUnpackDatas(ModBusRtuTypeDef *ptypeHandle, void *pvBuff, int32_t iLength);

#endif
