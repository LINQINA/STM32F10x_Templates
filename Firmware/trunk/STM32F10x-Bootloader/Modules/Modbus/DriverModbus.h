#ifndef _DriverModbus_H_
#define _DriverModbus_H_

/* PD 的Modbus寄存器基地址 */
#define MODBUS_PD_REGISTER_BASE_ADDR    0x8300
/* PD 的Modbus寄存器数量 */
#define MODBUS_PD_REGISTER_TOTAL_NUMBER 2048

#include "DevicesModbus.h"

typedef enum {
       /* 系统信息 */
    Modbus_Register_Addr_Software   = 0x0000,   /* 软件版本 */
    Modbus_Register_Addr_Hardware   = 0x0008,   /* 硬件版本 */
    Modbus_Register_Addr_UID        = 0x0010,   /* 设备UID */

    /* 升级 */
    Modbus_Register_Addr_Firmware_Software          = 0x0040,   /* 升级固件的软件版本 */
    Modbus_Register_Addr_Firmware_Hardware          = 0x0048,   /* 升级固件的硬件版本 */
    Modbus_Register_Addr_Firmware_Length            = 0x0050,   /* 升级固件的长度 */
    Modbus_Register_Addr_Firmware_CRC               = 0x0052,   /* 升级固件的校验值 */
    Modbus_Register_Addr_Firmware_State             = 0x0054,   /* 升级固件时的状态 */
    Modbus_Register_Addr_Firmware_Pack              = 0x0055,   /* 升级固件时的数据打包 */

    /* 通用参数A */
    Modbus_Register_Addr_ChannelA_SystemStatus   = 0x0080,  /* 通道A系统状态 */
    Modbus_Register_Addr_ChannelA_ErrorCode      = 0x0081,  /* 通道A错误码 */
    Modbus_Register_Addr_ChannelA_Temperature    = 0x0082,  /* 通道A温度 */
    Modbus_Register_Addr_ChannelA_Switch         = 0x0083,  /* 通道A输出开关 */
    Modbus_Register_Addr_ChannelA_Voltage        = 0x0084,  /* 通道A电压 */
    Modbus_Register_Addr_ChannelA_Current        = 0x0085,  /* 通道A电流 */
    Modbus_Register_Addr_ChannelA_ActivePower    = 0x0086,  /* 通道A有功功率 */
    Modbus_Register_Addr_ChannelA_ReactivePower  = 0x0087,  /* 通道A无功功率 */
    Modbus_Register_Addr_ChannelA_ApparentPower  = 0x0088,  /* 通道A视在功率 */
    Modbus_Register_Addr_ChannelA_PowerFactor    = 0x0089,  /* 通道A功率因素 */
    Modbus_Register_Addr_ChannelA_PhasePosition  = 0x008A,  /* 通道A相位 */
    Modbus_Register_Addr_ChannelA_Frequency      = 0x008B,  /* 通道A频率 */
    Modbus_Register_Addr_ChannelA_ElecQuantity   = 0x008C,  /* 通道A电量 */
    
    /* 通用参数B */
    Modbus_Register_Addr_ChannelB_SystemStatus   = 0x0090,  /* 通道B系统状态 */
    Modbus_Register_Addr_ChannelB_ErrorCode      = 0x0091,  /* 通道B错误码 */
    Modbus_Register_Addr_ChannelB_Temperature    = 0x0092,  /* 通道B错误码 */
    Modbus_Register_Addr_ChannelB_Switch         = 0x0093,  /* 通道B输出开关 */
    Modbus_Register_Addr_ChannelB_Voltage        = 0x0094,  /* 通道B电压 */
    Modbus_Register_Addr_ChannelB_Current        = 0x0095,  /* 通道B电流 */
    Modbus_Register_Addr_ChannelB_ActivePower    = 0x0096,  /* 通道B有功功率 */
    Modbus_Register_Addr_ChannelB_ReactivePower  = 0x0097,  /* 通道B无功功率 */
    Modbus_Register_Addr_ChannelB_ApparentPower  = 0x0098,  /* 通道B视在功率 */
    Modbus_Register_Addr_ChannelB_PowerFactor    = 0x0099,  /* 通道B功率因素 */
    Modbus_Register_Addr_ChannelB_PhasePosition  = 0x009A,  /* 通道B相位 */
    Modbus_Register_Addr_ChannelB_Frequency      = 0x009B,  /* 通道B频率 */
    Modbus_Register_Addr_ChannelB_ElecQuantity   = 0x009C,  /* 通道B电量 */
    
    /* HLW8110芯片参数 */
    Modbus_Register_Addr_HLW8110_Voltage                      = 0x0100,   /* 电压有效值 */
    Modbus_Register_Addr_HLW8110_ChannelA_Current             = 0x0101,   /* A通道电流 */
    Modbus_Register_Addr_HLW8110_ChannelA_ActivePower         = 0x0102,   /* A通道有功功率 */
    Modbus_Register_Addr_HLW8110_ChannelA_ElecQuantity        = 0x0103,   /* A通道有功电量 */
    Modbus_Register_Addr_HLW8110_ChannelA_ElecQuantity_Backup = 0x0104,   /* A通道电量备份 */
    Modbus_Register_Addr_HLW8110_PowerFactory                 = 0x0105,   /* 功率因素 */
    Modbus_Register_Addr_HLW8110_PhaseAngle                   = 0x0106,   /* 相角 */
    Modbus_Register_Addr_HLW8110_ChannelB_Current             = 0x0107,   /* B通道电流 */
    Modbus_Register_Addr_HLW8110_ChannelB_ActivePower         = 0x0108,   /* B通道有功功率 */
    Modbus_Register_Addr_HLW8110_ChannelB_ElecQuantity        = 0x0109,   /* B通道有功电量 */
    Modbus_Register_Addr_HLW8110_ChannelB_ElecQuantity_Backup = 0x010A,   /* B通道电量备份 */
    Modbus_Register_Addr_HLW8110_Frequency                    = 0x010B,   /* 市电线性频率 */
    Modbus_Register_Addr_HLW8110_CurrentChannel               = 0x010C,   /* 电量计当前通道 */

} ModbusRegisterAddrEnum;

int8_t cModbusSendDatas(uint32_t uiChannel, uint16_t usDeviceAddr, void *pvBuff, int32_t iLength, int32_t iFrontTime);
int8_t cModbusReceiveDatas(uint32_t uiChannel, void *pvBuff, int32_t iLength);
int32_t iModbusReceiveAllDatas(uint32_t uiChannel, void *pvBuff, int32_t iLengthLimit);
int32_t iModbusReceiveLengthGet(uint32_t uiChannel);
int8_t cModbusReceiveClear(uint32_t uiChannel);

int8_t cModbusRegisterSet(uint32_t uiChannel, uint16_t usDeviceAddr, uint16_t usRegisterAddr, uint16_t usValue, ModBusRtuTypeDef *ptypeModbusReply, int32_t iTimeOut, int32_t iFrontTime, int32_t iBehindTime);
int8_t cModbusDatasSet(uint32_t uiChannel, uint16_t usDeviceAddr, uint16_t usRegisterAddr, uint16_t *pusRegisters, int32_t iLength, ModBusRtuTypeDef *ptypeModbusReply, int32_t iTimeOut, int32_t iFrontTime, int32_t iBehindTime);
int8_t cModbusDatasGet(uint32_t uiChannel, uint16_t usDeviceAddr, uint16_t usRegisterAddr, uint16_t *pusRegisters, int32_t iLength, ModBusRtuTypeDef *ptypeModbusReply, int32_t iTimeOut, int32_t iFrontTime, int32_t iBehindTime);

int8_t cModbusUnpack(uint32_t uiChannel, uint8_t *pucBuff, int32_t iLength);

#endif
