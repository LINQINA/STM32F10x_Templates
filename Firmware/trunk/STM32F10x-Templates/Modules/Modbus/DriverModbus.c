#include "stm32f1xx.h"
#include "stdio.h"
#include "stdint.h"
#include "string.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "taskSensor.h"
#include "taskControl.h"
#include "taskSystem.h"
#include "taskMonitor.h"

#include "DevicesDelay.h"
#include "DevicesUart.h"
#include "DevicesModbus.h"
#include "DevicesRS485.h"
#include "DevicesSoftTimer.h"
#include "DevicesTime.h"

#include "DriverModbus.h"

#include "version.h"

/* Modbus 结构体 */
static ModBusRtuTypeDef st_typeModBusRtuHandle;

/* PD 端 Modbus 寄存器缓冲区 */
uint16_t st_usRegisterBuff[MODBUS_PD_REGISTER_TOTAL_NUMBER] = {0};

/* 刷新 Modbus 寄存器数据 */
int8_t cModbusPDRegisterUpdate(void)
{
    productType *ptypeProduct = ptypeProductGet();

    char ucSoftware[16] = "00.00.01";
    char ucHardware[16] = "00.00.01";
    char ucUid[16] = "00.00.01";

    /* Version */
    memcpy(&st_usRegisterBuff[Modbus_Register_Addr_Software], ptypeProduct->versionBuff, 8);
    memcpy(&st_usRegisterBuff[Modbus_Register_Addr_Hardware], ucHardware, 16);
    /* UID */
    memcpy(&st_usRegisterBuff[Modbus_Register_Addr_UID], ucUid, 32);

    /* 通道A */
    st_usRegisterBuff[Modbus_Register_Addr_ChannelA_Voltage] = 1;
    st_usRegisterBuff[Modbus_Register_Addr_ChannelA_Current] = 2;
    st_usRegisterBuff[Modbus_Register_Addr_ChannelA_ActivePower] = 3;
    st_usRegisterBuff[Modbus_Register_Addr_ChannelA_PhasePosition] = 4;
    st_usRegisterBuff[Modbus_Register_Addr_ChannelA_Frequency] = 5;
    st_usRegisterBuff[Modbus_Register_Addr_ChannelA_ElecQuantity] = 6;

    /* 通道B */
    st_usRegisterBuff[Modbus_Register_Addr_ChannelB_Voltage] = 1;
    st_usRegisterBuff[Modbus_Register_Addr_ChannelB_Current] = 2;
    st_usRegisterBuff[Modbus_Register_Addr_ChannelB_ActivePower] = 3;
    st_usRegisterBuff[Modbus_Register_Addr_ChannelB_PhasePosition] = 4;
    st_usRegisterBuff[Modbus_Register_Addr_ChannelB_Frequency] = 5;
    st_usRegisterBuff[Modbus_Register_Addr_ChannelB_ElecQuantity] = 6;

    /* HLW8110 数据 */
    st_usRegisterBuff[Modbus_Register_Addr_HLW8110_Voltage] = 1;
    st_usRegisterBuff[Modbus_Register_Addr_HLW8110_ChannelA_Current] = 2;
    st_usRegisterBuff[Modbus_Register_Addr_HLW8110_ChannelA_ActivePower] = 3;
    st_usRegisterBuff[Modbus_Register_Addr_HLW8110_ChannelA_ElecQuantity] = 4;
    st_usRegisterBuff[Modbus_Register_Addr_HLW8110_ChannelA_ElecQuantity_Backup] = 5;
    st_usRegisterBuff[Modbus_Register_Addr_HLW8110_PowerFactory] = 6;
    st_usRegisterBuff[Modbus_Register_Addr_HLW8110_PhaseAngle] = 7;
    st_usRegisterBuff[Modbus_Register_Addr_HLW8110_ChannelB_Current] = 8;
    st_usRegisterBuff[Modbus_Register_Addr_HLW8110_ChannelB_ActivePower] = 9;
    st_usRegisterBuff[Modbus_Register_Addr_HLW8110_ChannelB_ElecQuantity] = 10;
    st_usRegisterBuff[Modbus_Register_Addr_HLW8110_ChannelB_ElecQuantity_Backup] = 11;
    st_usRegisterBuff[Modbus_Register_Addr_HLW8110_Frequency] = 12;
    st_usRegisterBuff[Modbus_Register_Addr_HLW8110_CurrentChannel] = 0; // 0 表示通道A

    return 0;
}

int8_t cModbusPDRegisterEffect(uint16_t usRegisterAddr, uint16_t usValue)
{
    int8_t cError = 0;

    switch(usRegisterAddr)
    {
        case Modbus_Register_Addr_Firmware_State:
            if(usValue == 1)
            {
                cOTAStateSet(OTA_STATE_START);
            }
        break;
    }

    return cError;
}

/* Modbus 发送数据 */
int8_t cModbusSendDatas(uint32_t uiChannel, uint16_t usDeviceAddr, void *pvBuff, int32_t iLength, int32_t iFrontTime)
{
    /* 申请信号量 */
    switch(uiChannel)
    {
        case (uint32_t)UART_LOG               : xSemaphoreTakeRecursive(g_xUartLogSemaphore, portMAX_DELAY); break;
        case (uint32_t)UART_BUS               : xSemaphoreTakeRecursive(g_xRS485BusSemaphore, portMAX_DELAY); break;

        default : return 1;
    }

    /* 某些设备发送前需要延时 */
    vRtosDelayMs(iFrontTime);

    /* 发送数据 */
    switch(uiChannel)
    {
        case (uint32_t)UART_LOG               : vUartDMASendDatas(uiChannel, pvBuff, iLength); break;
        case (uint32_t)UART_BUS               : cRS485xSendDatas(uiChannel, pvBuff, iLength); break;

        default : break;
    }

    /* 释放信号量 */
    switch(uiChannel)
    {
        case (uint32_t)UART_LOG               : xSemaphoreGiveRecursive(g_xUartLogSemaphore); break;
        case (uint32_t)UART_BUS               : xSemaphoreGiveRecursive(g_xRS485BusSemaphore); break;

        default : return 2;
    }

    return 0;
}

int8_t cModbusReceiveDatas(uint32_t uiChannel, void *pvBuff, int32_t iLength)
{
    switch(uiChannel)
    {
        case (uint32_t)UART_LOG   : return cUartReceiveDatas(uiChannel, pvBuff, iLength);
        case (uint32_t)UART_BUS   : return cRS485xReceiveDatas(uiChannel, pvBuff, iLength);

        default : return 1;
    }
}

int32_t iModbusReceiveAllDatas(uint32_t uiChannel, void *pvBuff, int32_t iLengthLimit)
{
    switch(uiChannel)
    {
        case (uint32_t)UART_LOG   : return iUartReceiveAllDatas(uiChannel, pvBuff, iLengthLimit);
        case (uint32_t)UART_BUS   : return iRS485xReceiveAllDatas(uiChannel, pvBuff, iLengthLimit);

        default : return 0;
    }
}

int32_t iModbusReceiveLengthGet(uint32_t uiChannel)
{
    switch(uiChannel)
    {
        case (uint32_t)UART_LOG   : return iUartReceiveLengthGet(uiChannel);
        case (uint32_t)UART_BUS   : return iRS485xReceiveLengthGet(uiChannel);

        default : return 0;
    }
}

/* 接收缓冲区清除 */
int8_t cModbusReceiveClear(uint32_t uiChannel)
{
    switch(uiChannel)
    {
        case (uint32_t)UART_LOG   : return cUartReceiveClear(uiChannel);
        case (uint32_t)UART_BUS   : return cRS485xReceiveClear(uiChannel);

        default : return 1;
    }
}

/* 写多个寄存器 */
int8_t cModbusDatasSet(uint32_t uiChannel, uint16_t usDeviceAddr, uint16_t usRegisterAddr, uint16_t *pusRegisters, int32_t iLength, ModBusRtuTypeDef *ptypeModbusReply, int32_t iTimeOut, int32_t iFrontTime, int32_t iBehindTime)
{
    SoftTimerTypeDef typeSoftTimerTimeOut = {0};
    int32_t iLengthRead = 8;
    int8_t cError = 0;

    if((pusRegisters == NULL) || (ptypeModbusReply == NULL))
        return 1;

    if((iLength < 1) || (uiChannel != (uint32_t)UART_BUS))
        return 2;

    /* 申请信号量 */
    xSemaphoreTakeRecursive(g_xRS485BusSemaphore, portMAX_DELAY);

    /* 清除接收缓冲区 */
    cModbusReceiveClear(uiChannel);

    /* 打包 Modbus 请求帧 */
    cModbusPackRTU_10(usDeviceAddr, usRegisterAddr, iLength, pusRegisters, (uint8_t *)ptypeModbusReply);

    /* 某些设备需要发送前延时；发送 Modbus 请求 */
    cModbusSendDatas(uiChannel, usDeviceAddr, ptypeModbusReply, 9 + (iLength * 2), iFrontTime);

    /* 设置超时 */
    cSoftTimerSetMs(&typeSoftTimerTimeOut, iTimeOut, softTimerOpen);

    /* 清空接收数据结构 */
    memset(ptypeModbusReply, 0, sizeof(ModBusRtuTypeDef));
    /* 循环等待接收数据 */
    while((cError = cModbusReceiveDatas(uiChannel, ptypeModbusReply, iLengthRead)) != 0)
    {
        /* 超时判断 */
        if(enumSoftTimerGetState(&typeSoftTimerTimeOut) == softTimerOver)
        {
            break;
        }

        /* 轮询等待，避免长阻塞占用MCU */
        vRtosDelayMs(2);
    }

    /* 发送后延时 */
    vRtosDelayMs(iBehindTime);

    /* 处理接收数据 */
    if(cError == 0)
    {
        /* 继续读取FIFO剩余数据 */
        iLengthRead += iModbusReceiveAllDatas(uiChannel, (((uint8_t *)ptypeModbusReply) + iLengthRead), sizeof(ptypeModbusReply->data) - iLengthRead);

        /* 解析应答 */
        if(enumModbusReplyUnpackDatas(ptypeModbusReply, ptypeModbusReply, iLengthRead) == MODBUS_UNPACK_SUCCEED)
        {
            /* 异常应答 */
            if((ptypeModbusReply->func & 0x80) != 0)
            {
                /* 异常码 */
                cError = ptypeModbusReply->data[0];
            }
        }
        else
        {
            cError = 3;
        }
    }

    /* 释放信号量 */
    xSemaphoreGiveRecursive(g_xRS485BusSemaphore);

    return cError;
}

/* 读多个寄存器 */
int8_t cModbusDatasGet(uint32_t uiChannel, uint16_t usDeviceAddr, uint16_t usRegisterAddr, uint16_t *pusRegisters, int32_t iLength, ModBusRtuTypeDef *ptypeModbusReply, int32_t iTimeOut, int32_t iFrontTime, int32_t iBehindTime)
{
    SoftTimerTypeDef typeSoftTimerTimeOut = {0};
    int32_t iLengthRead = 0;
    int8_t cError = 0;

    if((pusRegisters == NULL) || (ptypeModbusReply == NULL))
        return 1;

    if((iLength < 1) || (uiChannel != (uint32_t)UART_BUS))
        return 2;

    /* 申请信号量 */
    xSemaphoreTakeRecursive(g_xRS485BusSemaphore, portMAX_DELAY);

    /* 清除 Modbus 接收缓冲 */
    cModbusReceiveClear(uiChannel);

    /* 打包 Modbus 读请求 */
    cModbusPackRTU_03(usDeviceAddr, usRegisterAddr, iLength, (uint8_t *)ptypeModbusReply);

    /* 某些设备发送前需要延时；发送 Modbus 请求 */
    cModbusSendDatas(uiChannel, usDeviceAddr, ptypeModbusReply, 8, iFrontTime);

    iLengthRead = 3 + iLength * 2 + 2;

    /* 设置超时 */
    cSoftTimerSetMs(&typeSoftTimerTimeOut, iTimeOut + iLengthRead * 1.5f, softTimerOpen);

    /* 置空接收结构体 */
    memset(ptypeModbusReply, 0, sizeof(ModBusRtuTypeDef));
    /* 期望读取：1字节地址 + 1字节功能码 + 1字节数据长度 + n字节数据 + 2字节CRC */
    while((cError = cModbusReceiveDatas(uiChannel, ptypeModbusReply, iLengthRead)) != 0)
    {
        /* 超时判断 */
        if(enumSoftTimerGetState(&typeSoftTimerTimeOut) == softTimerOver)
        {
            break;
        }

        /* 轮询等待，避免长阻塞占用MCU */
        vRtosDelayMs(2);
    }

    /* 某些设备请求后需要后延时 */
    vRtosDelayMs(iBehindTime);

    /* 处理接收数据 */
    if(cError == 0)
    {
        iLengthRead += iModbusReceiveAllDatas(uiChannel, ((uint8_t *)ptypeModbusReply) + iLengthRead, sizeof(ptypeModbusReply->data) - iLengthRead);

        /* 解析应答 */
        if(enumModbusReplyUnpackDatas(ptypeModbusReply, ptypeModbusReply, iLengthRead) == MODBUS_UNPACK_SUCCEED)
        {
            /* 异常应答 */
            if((ptypeModbusReply->func & 0x80) != 0)
            {
                /* 异常码 */
                cError = ptypeModbusReply->data[0];
            }
            /* 正常应答 */
            else
            {
                memcpy(pusRegisters, &(ptypeModbusReply->data[1]), iLength * 2);
            }
        }
        else
        {
            cError = 3;
        }
    }

    /* 释放信号量 */
    xSemaphoreGiveRecursive(g_xRS485BusSemaphore);

    return cError;
}

/* 透传 BUS 请求 */
int8_t cModbusSeriaNet(uint32_t uiChannel, ModBusRtuTypeDef *ptypeHandle)
{
    SoftTimerTypeDef typeSoftTimerTimeOut = {0};
    int32_t iTimeHoldup = 20, iLengthRead = 0;
    uint16_t usRegisterNumber = 0;
    int8_t cError = 0;

    /* 申请信号量 */
    xSemaphoreTakeRecursive(g_xRS485BusSemaphore, portMAX_DELAY);

    /* 清除 Modbus 接收缓冲 */
    cModbusReceiveClear((uint32_t)UART_BUS);

    /* 发送 Modbus 请求帧 */
    cModbusSendDatas((uint32_t)UART_BUS, ptypeHandle->slaveAddress, ptypeHandle, ptypeHandle->length + 2, iTimeHoldup);

    /* 判断功能码以计算期望长度（BMS/INV等设备） */
    switch(ptypeHandle->func)
    {
        case MODBUS_CODE_0x03 :
            usRegisterNumber = (((uint16_t)ptypeHandle->data[2]) << 8) | ptypeHandle->data[3];
            iLengthRead = 3 + usRegisterNumber * 2 + 2;
            break;

        case MODBUS_CODE_0x06 : iLengthRead = 8; break;
        case MODBUS_CODE_0x10 : iLengthRead = 8; break;

        default : iLengthRead = 0; break;
    }

    /* 超时估计：9600 等较慢设备保留 100ms 余量 */
    iTimeHoldup = 100 + iLengthRead * 1.5f;

    /* 设置超时 */
    cSoftTimerSetMs(&typeSoftTimerTimeOut, iTimeHoldup, softTimerOpen);

    /* 循环接收 */
    while((cError = cModbusReceiveDatas((uint32_t)UART_BUS, ptypeHandle, iLengthRead)) != 0)
    {
        /* 超时判断 */
        if(enumSoftTimerGetState(&typeSoftTimerTimeOut) == softTimerOver)
        {
            break;
        }

        /* 轮询等待，避免长阻塞占用MCU */
        vRtosDelayMs(2);
    }

    /* 根据接收是否成功设置长度 */
    iLengthRead = (cError == 0) ? iLengthRead : 0;

    /* 继续从FIFO读满剩余数据 */
    iLengthRead += iModbusReceiveAllDatas((uint32_t)UART_BUS, ((uint8_t *)ptypeHandle) + iLengthRead, sizeof(ptypeHandle->data) - iLengthRead);

    /* 回显到上行口 */
    cModbusSendDatas(uiChannel, 0, ptypeHandle, iLengthRead, 0);

    /* 释放信号量 */
    xSemaphoreGiveRecursive(g_xRS485BusSemaphore);

    return cError;
}

/* Modbus 消息解析 */
int8_t cModbusMessageAnalysis(uint32_t uiChannel, ModBusRtuTypeDef *ptypeHandle)
{
//    LogConfigType *ptypeLogConfigInfo = ptypeLogConfigInfoGet();
    int32_t iLength = 0, i = 0;
    uint16_t usAddr = 0, usValue = 0, usAddrRelative = 0, usAddrStop = 0;
    uint8_t *pucBuff = NULL;
    int8_t cError = 0;

    if(ptypeHandle == NULL)
        return 1;

    /* 解析寄存器起始地址 */
    usAddr = ptypeHandle->data[0];
    usAddr = (usAddr << 8) | ptypeHandle->data[1];

    /* 解析寄存器数量/值 */
    usValue = ptypeHandle->data[2];
    usValue = (usValue << 8) | ptypeHandle->data[3];

    /* 有效地址范围判断 */
    usAddrStop = (ptypeHandle->func == MODBUS_CODE_0x06) ? usAddr : (usAddr + usValue);
    if((usAddr >= MODBUS_PD_REGISTER_BASE_ADDR) && (usAddrStop < (MODBUS_PD_REGISTER_BASE_ADDR + MODBUS_PD_REGISTER_TOTAL_NUMBER)))
    {
        /* 计算相对地址 */
        usAddrRelative = usAddr - MODBUS_PD_REGISTER_BASE_ADDR;

        switch(ptypeHandle->func)
        {
            case MODBUS_CODE_0x03:
//                /* log 路径/文件相关寄存器读取（若有） */
//                if((usAddr == Modbus_Register_Addr_File_Size)   ||
//                   (usAddr == Modbus_Register_Addr_File_Path)   ||
//                   (usAddr == Modbus_Register_Addr_File_Data))
//                {
//                    cModbusLogRegisterRead(usAddr, usValue);
//                }
//                /* 默认：读取PD寄存器 */
//                else
                {
                    cModbusPDRegisterUpdate();
                }

                cModbusPackReplyRTU_03(ptypeHandle->slaveAddress, usValue, &st_usRegisterBuff[usAddrRelative], ptypeHandle);

                iLength = usValue * 2 + 5;
                break;

            case MODBUS_CODE_0x06:
                st_usRegisterBuff[usAddrRelative] = usValue;

                /* 生效：写单寄存器的副作用处理 */
                cError = cModbusPDRegisterEffect(usAddrRelative, usValue);

                cModbusPackReplyRTU_06(ptypeHandle->slaveAddress, usAddrRelative, usValue, (uint8_t *)ptypeHandle);
                iLength = 8;
                break;

            case MODBUS_CODE_0x10:
                /* OTA 固件块写入解析 */
                if(usAddr == Modbus_Register_Addr_Firmware_Pack)
                {
                    cError = cOTAModbusPackAnalysis(ptypeHandle);
                }
//                /* Log 数据写入（若启用） */
//                else if(usAddr == Modbus_Register_Addr_File_Path)
//                {
//                    cError = cModbusLogRegisterWrite(usAddr, &ptypeHandledata[5], usValue);
//                }
//                /* 默认：写入寄存器数组 */
                else
                {
                    pucBuff = &ptypeHandle->data[5];

                    for(i = 0; i < usValue; ++i)
                    {
                        st_usRegisterBuff[usAddrRelative] = *pucBuff++;
                        st_usRegisterBuff[usAddrRelative] = (st_usRegisterBuff[usAddrRelative] << 8) | *pucBuff++;

                        /* 生效：写寄存器后的副作用处理 */
                        cModbusPDRegisterEffect(usAddrRelative, st_usRegisterBuff[usAddrRelative]);

                        usAddrRelative++;
                    }
                }

                if(cError == 0)
                {
                    cModbusPackReplyRTU_10(ptypeHandle->slaveAddress, usAddrStop - usValue, usValue, (uint8_t *)ptypeHandle);
                    iLength = 8;
                }
                else
                {
                    cModbusPackReplyRTU_ErrorCode(ptypeHandle->slaveAddress, ptypeHandle->func, 1, (uint8_t *)ptypeHandle);
                    iLength = 5;
                }
                break;

            default :
                /* 不支持的功能码 */
                cModbusPackReplyRTU_ErrorCode(ptypeHandle->slaveAddress, ptypeHandle->func, 1, (uint8_t *)ptypeHandle);
                iLength = 5;
                break;
        }
    }
    else
    {
        /* 访问寄存器超出允许范围 */
        cModbusPackReplyRTU_ErrorCode(ptypeHandle->slaveAddress, ptypeHandle->func, 1, (uint8_t *)ptypeHandle);
        iLength = 5;
    }

    /* 发送回复帧 */
    cModbusSendDatas(uiChannel, 0, ptypeHandle, iLength, 0);

    return cError;
}

/* 解包 Modbus 数据 */
int8_t cModbusUnpack(uint32_t uiChannel, uint8_t *pucBuff, int32_t iLength)
{
//    SensorInfoType *ptypeSensorInfo = ptypeSensorInfoGet();
    uint32_t uiTime = (uint32_t)(lTimeGetStamp() / 1000ll);
    int8_t cError = 1;

    /* 超过空闲时间重置状态机 */
    if((uiTime - st_typeModBusRtuHandle.timeIdle) > 500)
    {
        st_typeModBusRtuHandle.state = MODBUS_UNPACK_ADDRESS;
    }

    st_typeModBusRtuHandle.timeIdle = uiTime;

    while((iLength--) > 0)
    {
        /* 判断解包状态 */
        if(enumModbusUnpack(&st_typeModBusRtuHandle, *pucBuff++) == MODBUS_UNPACK_SUCCEED)
        {
            /* PD 端请求：本机处理 */
            if(st_typeModBusRtuHandle.slaveAddress == MODBUS_ADDRESS_PD)
            {
                cError = cModbusMessageAnalysis(uiChannel, &st_typeModBusRtuHandle);
            }
            /* 其它从设备：透传到总线 */
            else
            {
//                /* 若处于 OTA 状态则不透传 */
//                if((ptypeSensorInfoptypeSystemInfostate & SYSTEM_ACTION_OTA) != 0)
//                    break;
//
//                /* 若为 INV0 等设备，复位网络空闲定时器 */
//                if((st_typeModBusRtuHandle.slaveAddress == MODBUS_ADDRESS_INV0) )
//                {
//                    /* 重置网络空闲软定时器 */
//                    cSoftTimerReset(&g_typeSoftTimerINVNetworkIdle);
//                }

                /* 将收到的 Modbus 请求透传到总线设备 */
                cError = cModbusSeriaNet(uiChannel, &st_typeModBusRtuHandle);
            }
        }
    }

    return cError;
}
