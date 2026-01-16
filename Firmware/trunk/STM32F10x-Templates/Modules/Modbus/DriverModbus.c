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


/* 解析结构体，用来缓存 */
static ModBusRtuTypeDef st_typeModBusRtuHandle;

/* PD 的Modbus寄存器 */
uint16_t st_usRegisterBuff[MODBUS_PD_REGISTER_TOTAL_NUMBER] = {0};

/* 刷新 Modbus 寄存器的值 */
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


    /* 通道A参数 */
    st_usRegisterBuff[Modbus_Register_Addr_ChannelA_Voltage] = 1;
    st_usRegisterBuff[Modbus_Register_Addr_ChannelA_Current] = 2;
    st_usRegisterBuff[Modbus_Register_Addr_ChannelA_ActivePower] = 3;
    st_usRegisterBuff[Modbus_Register_Addr_ChannelA_PhasePosition] = 4;
    st_usRegisterBuff[Modbus_Register_Addr_ChannelA_Frequency] = 5;
    st_usRegisterBuff[Modbus_Register_Addr_ChannelA_ElecQuantity] = 6;

    return 0;
}

/* 生效 Modbus 寄存器的值 */
int8_t cModbusPDRegisterEffect(uint16_t usRegisterAddr, uint16_t usValue)
{
    int8_t cError = 0;
    
    switch(usRegisterAddr)
    {
        case Modbus_Register_Addr_Firmware_State        :
            if(usValue == 1)
            {
                cOTAStateSet(OTA_STATE_START);
            }
        break;
    
    }

    return cError;
}

/* Modbus发送数据 */
int8_t cModbusSendDatas(uint32_t uiChannel, uint16_t usDeviceAddr, void *pvBuff, int32_t iLength, int32_t iFrontTime)
{
    /* 获取递归互斥信号量 */
    switch(uiChannel)
    {
        case (uint32_t)UART_LOG               : xSemaphoreTakeRecursive(g_xUartLogSemaphore, portMAX_DELAY); break;
        case (uint32_t)UART_BUS               : xSemaphoreTakeRecursive(g_xRS485BusSemaphore, portMAX_DELAY); break;

        default : return 1;
    }

    /* 有些设备，它需要总线保持一段时间的空闲电平，才能接收到数据 */
    vRtosDelayMs(iFrontTime);

    /* 发送数据 */
    switch(uiChannel)
    {
        case (uint32_t)UART_LOG               : vUartDMASendDatas(uiChannel, pvBuff, iLength); break;
        case (uint32_t)UART_BUS               : cRS485xSendDatas(uiChannel, pvBuff, iLength); break;

        default : break;
    }

    /* 释放递归互斥信号量 */
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

/* 清空 接收缓存 */
int8_t cModbusReceiveClear(uint32_t uiChannel)
{
    switch(uiChannel)
    {
        case (uint32_t)UART_LOG   : return cUartReceiveClear(uiChannel);
        case (uint32_t)UART_BUS   : return cRS485xReceiveClear(uiChannel);

        default : return 1;
    }
}

/* 设置 多个 寄存器的值 */
int8_t cModbusDatasSet(uint32_t uiChannel, uint16_t usDeviceAddr, uint16_t usRegisterAddr, uint16_t *pusRegisters, int32_t iLength, ModBusRtuTypeDef *ptypeModbusReply, int32_t iTimeOut, int32_t iFrontTime, int32_t iBehindTime)
{
    SoftTimerTypeDef typeSoftTimerTimeOut = {0};
    int32_t iLengthRead = 8;
    int8_t cError = 0;

    if((pusRegisters == NULL) || (ptypeModbusReply == NULL))
        return 1;

    if((iLength < 1) || (uiChannel != (uint32_t)UART_BUS))
        return 2;

    /* 获取递归互斥信号量 */
    xSemaphoreTakeRecursive(g_xRS485BusSemaphore, portMAX_DELAY);

    /* 清空 Modbus 接收缓存 */
    cModbusReceiveClear(uiChannel);

    /* 打包成 Modbus 格式 */
    cModbusPackRTU_10(usDeviceAddr, usRegisterAddr, iLength, pusRegisters, (uint8_t *)ptypeModbusReply);

    /* 有些从设备程序有问题，在总线上帧间隔至少需要x ms才能稳定接收到数据 */
    /* 通过 Modbus 总线发送打包好的数据 */
    cModbusSendDatas(uiChannel, usDeviceAddr, ptypeModbusReply, 9 + (iLength * 2), iFrontTime);

    /* 超时设置 */
    cSoftTimerSetMs(&typeSoftTimerTimeOut, iTimeOut, softTimerOpen);

    /* 清空以前遗留数据 */
    memset(ptypeModbusReply, 0, sizeof(ModBusRtuTypeDef));
    /* 等待接收数据 */
    while((cError = cModbusReceiveDatas(uiChannel, ptypeModbusReply, iLengthRead)) != 0)
    {
        /* 超时检查 */
        if(enumSoftTimerGetState(&typeSoftTimerTimeOut) == softTimerOver)
        {
            break;
        }

        /* 需要添加操作系统 延时函数，以释放MCU运算资源的占有 */
        vRtosDelayMs(2);
    }


    /* 有些从设备回复数据后，会继续占用总线一段时间 */
    vRtosDelayMs(iBehindTime);


    /* 设定时间内，收到预期长度的数据 */
    if(cError == 0)
    {
        /* 再获取全部数据 */
        iLengthRead += iModbusReceiveAllDatas(uiChannel, (((uint8_t *)ptypeModbusReply) + iLengthRead), sizeof(ptypeModbusReply->data) - iLengthRead);

        /* 数据解析成功 */
        if(enumModbusReplyUnpackDatas(ptypeModbusReply, ptypeModbusReply, iLengthRead) == MODBUS_UNPACK_SUCCEED)
        {
            /* 非错误码回复 */
            if((ptypeModbusReply->func & 0x80) != 0)
            {
                /* 错误码 */
                cError = ptypeModbusReply->data[0];
            }
        }
        else
        {
            cError = 3;
        }
    }

    /* 释放递归互斥信号量 */
    xSemaphoreGiveRecursive(g_xRS485BusSemaphore);

    return cError;
}

/* 获取 多个 寄存器的值 */
int8_t cModbusDatasGet(uint32_t uiChannel, uint16_t usDeviceAddr, uint16_t usRegisterAddr, uint16_t *pusRegisters, int32_t iLength, ModBusRtuTypeDef *ptypeModbusReply, int32_t iTimeOut, int32_t iFrontTime, int32_t iBehindTime)
{
    SoftTimerTypeDef typeSoftTimerTimeOut = {0};
    int32_t iLengthRead = 0;
    int8_t cError = 0;

    if((pusRegisters == NULL) || (ptypeModbusReply == NULL))
        return 1;

    if((iLength < 1) || (uiChannel != (uint32_t)UART_BUS))
        return 2;

    /* 获取递归互斥信号量 */
    xSemaphoreTakeRecursive(g_xRS485BusSemaphore, portMAX_DELAY);

    /* 清空 Modbus 接收缓存 */
    cModbusReceiveClear(uiChannel);

    /* 打包成 Modbus 格式 */
    cModbusPackRTU_03(usDeviceAddr, usRegisterAddr, iLength, (uint8_t *)ptypeModbusReply);

    /* 有些从设备程序有问题，在总线上帧间隔至少需要x ms才能稳定接收到数据 */
    /* 通过 Modbus 总线发送打包好的数据 */
    cModbusSendDatas(uiChannel, usDeviceAddr, ptypeModbusReply, 8, iFrontTime);

    iLengthRead = 3 + iLength * 2 + 2;

    /* 超时设置 */
    cSoftTimerSetMs(&typeSoftTimerTimeOut, iTimeOut + iLengthRead * 1.5f, softTimerOpen);

    /* 清空以前遗留数据 */
    memset(ptypeModbusReply, 0, sizeof(ModBusRtuTypeDef));
    /* 等待接收数据（长度：1字节设备地址 + 1字节功能码 + 1字节的字节长度 + n个寄存器的值 + 2字节CRC） */
    while((cError = cModbusReceiveDatas(uiChannel, ptypeModbusReply, iLengthRead)) != 0)
    {
        /* 超时检查 */
        if(enumSoftTimerGetState(&typeSoftTimerTimeOut) == softTimerOver)
        {
            break;
        }

        /* 需要添加操作系统 延时函数，以释放MCU运算资源的占有 */
        vRtosDelayMs(2);
    }


    /* 有些从设备回复数据后，会继续占用总线一段时间 */
    vRtosDelayMs(iBehindTime);


    /* 设定时间内，收到预期长度的数据 */
    if(cError == 0)
    {
        /* 再获取全部数据 */
        iLengthRead += iModbusReceiveAllDatas(uiChannel, ((uint8_t *)ptypeModbusReply) + iLengthRead, sizeof(ptypeModbusReply->data) - iLengthRead);

        /* 数据解析成功 */
        if(enumModbusReplyUnpackDatas(ptypeModbusReply, ptypeModbusReply, iLengthRead) == MODBUS_UNPACK_SUCCEED)
        {
            /* 误码回复 */
            if((ptypeModbusReply->func & 0x80) != 0)
            {
                /* 错误码 */
                cError = ptypeModbusReply->data[0];
            }
            /* 非误码回复 */
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

    /* 释放递归互斥信号量 */
    xSemaphoreGiveRecursive(g_xRS485BusSemaphore);

    return cError;
}

/* 透传数据到 BUS 总线 */
int8_t cModbusSeriaNet(uint32_t uiChannel, ModBusRtuTypeDef *ptypeHandle)
{
    SoftTimerTypeDef typeSoftTimerTimeOut = {0};
    int32_t iTimeHoldup = 20, iLengthRead = 0;
    uint16_t usRegisterNumber = 0;
    int8_t cError = 0;

    /* 获取递归互斥信号量 */
    xSemaphoreTakeRecursive(g_xRS485BusSemaphore, portMAX_DELAY);

    /* 清空 Modbus 接收缓存 */
    cModbusReceiveClear((uint32_t)UART_BUS);

    /* 通过 Modbus 总线发送打包好的数据 */
    cModbusSendDatas((uint32_t)UART_BUS, ptypeHandle->slaveAddress, ptypeHandle, ptypeHandle->length + 2, iTimeHoldup);

    /* 判断透传后，会接收到BMS、INV的数据长度 */
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

    /* 计算接收时间（波特率9600），至少给其它接收设备100ms的响应时间 */
    iTimeHoldup = 100 + iLengthRead * 1.5f;

    /* 超时设置 */
    cSoftTimerSetMs(&typeSoftTimerTimeOut, iTimeHoldup, softTimerOpen);

    /* 等待接收数据 */
    while((cError = cModbusReceiveDatas((uint32_t)UART_BUS, ptypeHandle, iLengthRead)) != 0)
    {
        /* 超时检查 */
        if(enumSoftTimerGetState(&typeSoftTimerTimeOut) == softTimerOver)
        {
            break;
        }

        /* 需要添加操作系统 延时函数，以释放MCU运算资源的占有 */
        vRtosDelayMs(2);
    }

    /* 没有接收到预期长度的数据，可能是回复了错误码 */
    iLengthRead = (cError == 0) ? iLengthRead : 0;

    /* 如果接收FIFO内还有数据，需要把数据也全部读取出来 */
    iLengthRead += iModbusReceiveAllDatas((uint32_t)UART_BUS, ((uint8_t *)ptypeHandle) + iLengthRead, sizeof(ptypeHandle->data) - iLengthRead);

    /* 向上位机透传 */
    cModbusSendDatas(uiChannel, 0, ptypeHandle, iLengthRead, 0);

    /* 释放递归互斥信号量 */
    xSemaphoreGiveRecursive(g_xRS485BusSemaphore);

    return cError;
}

/* 解析给 本机 的数据 */
int8_t cModbusMessageAnalysis(uint32_t uiChannel, ModBusRtuTypeDef *ptypeHandle)
{
//    LogConfigType *ptypeLogConfigInfo = ptypeLogConfigInfoGet();
    int32_t iLength = 0, i = 0;
    uint16_t usAddr = 0, usValue = 0, usAddrRelative = 0, usAddrStop = 0;
    uint8_t *pucBuff = NULL;
    int8_t cError = 0;

    if(ptypeHandle == NULL)
        return 1;

    /* 寄存器起始地址 */
    usAddr = ptypeHandle->data[0];
    usAddr = (usAddr << 8) | ptypeHandle->data[1];

    /* 寄存器的数量，或者寄存器的值 */
    usValue = ptypeHandle->data[2];
    usValue = (usValue << 8) | ptypeHandle->data[3];

    /* 有效地址范围检测 */
    usAddrStop = (ptypeHandle->func == MODBUS_CODE_0x06) ? usAddr : (usAddr + usValue);
    if((usAddr >= MODBUS_PD_REGISTER_BASE_ADDR) && (usAddrStop < (MODBUS_PD_REGISTER_BASE_ADDR + MODBUS_PD_REGISTER_TOTAL_NUMBER)))
    {
        /* 相对地址 */
        usAddrRelative = usAddr - MODBUS_PD_REGISTER_BASE_ADDR;

        switch(ptypeHandle->func)
        {
            case MODBUS_CODE_0x03:
//                /* log路径、数据帧寄存器，需要特殊处理 */
//                if((usAddr == Modbus_Register_Addr_File_Size)   ||
//                   (usAddr == Modbus_Register_Addr_File_Path)   ||
//                   (usAddr == Modbus_Register_Addr_File_Data))
//                {
//                    cModbusLogRegisterRead(usAddr, usValue);
//                }
//                /* 普通寄存器 */
//                else
                {
                    cModbusPDRegisterUpdate();
                }

                cModbusPackReplyRTU_03(ptypeHandle->slaveAddress, usValue, &st_usRegisterBuff[usAddrRelative], ptypeHandle);

                iLength = usValue * 2 + 5;
                break;

            case MODBUS_CODE_0x06:
                st_usRegisterBuff[usAddrRelative] = usValue;

                /* 生效对应寄存器的动作 */
                cError = cModbusPDRegisterEffect(usAddrRelative, usValue);

                cModbusPackReplyRTU_06(ptypeHandle->slaveAddress, usAddrRelative, usValue, (uint8_t *)ptypeHandle);
                iLength = 8;
                break;

            case MODBUS_CODE_0x10:
                /* OTA 固件分包帧，需要特殊处理 */
                if(usAddr == Modbus_Register_Addr_Firmware_Pack)
                {
                    cError = cOTAModbusPackAnalysis(ptypeHandle);
                }
//                /* Log 帧，需要特殊处理 */
//                else if(usAddr == Modbus_Register_Addr_File_Path)
//                {
//                    cError = cModbusLogRegisterWrite(usAddr, &ptypeHandle->data[5], usValue);
//                }
//                /* 标准帧 */
                else
                {
                    pucBuff = &ptypeHandle->data[5];

                    for(i = 0; i < usValue; ++i)
                    {
                        st_usRegisterBuff[usAddrRelative] = *pucBuff++;
                        st_usRegisterBuff[usAddrRelative] = (st_usRegisterBuff[usAddrRelative] << 8) | *pucBuff++;

                        /* 生效对应寄存器的动作 */
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
                /* 返回错误码 */
                cModbusPackReplyRTU_ErrorCode(ptypeHandle->slaveAddress, ptypeHandle->func, 1, (uint8_t *)ptypeHandle);
                iLength = 5;
                break;
        }
    }
    else
    {
        /* 返回错误码（寄存器地址超出范围） */
        cModbusPackReplyRTU_ErrorCode(ptypeHandle->slaveAddress, ptypeHandle->func, 1, (uint8_t *)ptypeHandle);
        iLength = 5;
    }

    /* 回复上位机 */
    cModbusSendDatas(uiChannel, 0, ptypeHandle, iLength, 0);

    return cError;
}

/* 解包收到的 Modbus 数据 */
int8_t cModbusUnpack(uint32_t uiChannel, uint8_t *pucBuff, int32_t iLength)
{
//    SensorInfoType *ptypeSensorInfo = ptypeSensorInfoGet();
    uint32_t uiTime = (uint32_t)(lTimeGetStamp() / 1000ll);
    int8_t cError = 1;

    /* 空闲超时，重新开启解析 */
    if((uiTime - st_typeModBusRtuHandle.timeIdle) > 500)
    {
        st_typeModBusRtuHandle.state = MODBUS_UNPACK_ADDRESS;
    }

    st_typeModBusRtuHandle.timeIdle = uiTime;

    while((iLength--) > 0)
    {
        /* 判断是否解析成功 */
        if(enumModbusUnpack(&st_typeModBusRtuHandle, *pucBuff++) == MODBUS_UNPACK_SUCCEED)
        {
            /* 发给 本机 的数据（解析） */
            if(st_typeModBusRtuHandle.slaveAddress == MODBUS_ADDRESS_PD)
            {
                cError = cModbusMessageAnalysis(uiChannel, &st_typeModBusRtuHandle);
            }
            /* 发给 其它设备 的数据（透传） */
            else
            {
//                /* OTA时，不进行数据透传 */
//                if((ptypeSensorInfo->ptypeSystemInfo->state & SYSTEM_ACTION_OTA) != 0)
//                    break;

//                /* 跟逆变器通信时，必须要先开启逆变器的使能 */
//                if((st_typeModBusRtuHandle.slaveAddress == MODBUS_ADDRESS_INV0) )
//                {
//                    /* 通信空闲超时、重新计时 */
//                    cSoftTimerReset(&g_typeSoftTimerINVNetworkIdle);
//                }

                /* 透传至其它的Modbus设备，走标准Modbus协议就行 */
                cError = cModbusSeriaNet(uiChannel, &st_typeModBusRtuHandle);
            }
        }
    }

    return cError;
}
