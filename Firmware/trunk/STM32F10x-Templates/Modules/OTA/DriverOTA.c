#include "stm32f1xx.h"
#include "stdint.h"
#include "stdio.h"
#include "string.h"
#include "version.h"

#include "DevicesWatchDog.h"
#include "DevicesDelay.h"
#include "DevicesSPIFlash.h"
#include "DevicesFlash.h"
#include "DevicesCRC.h"
#include "DevicesRS485.h"
#include "DevicesSoftTimer.h"

#include "DriverModbus.h"
#include "DriverBootloader.h"
#include "DriverLogPrintf.h"
#include "DriverOTA.h"
#include "DriverUpgradePD.h"
#include "DriverBootloader.h"

#include "taskSensor.h"

#include "userMath.h"

static int8_t cOTAErrorSet(uint32_t uiError);
static int8_t cOTAUpdateCrcAndWriteToFlash();

int8_t g_cOTAInitFlag;                                                        /* 初始化完成标志 */

uint16_t g_usOTAUpdatingFirmwareNo;                                           /* OTA当前更新固件编号 */
uint32_t g_uiFirmwareLengthNow;                                               /* OTA固件 已写入的长度 */

static OTAInfoType st_typeOTAInfo;                                            /* OTA基本信息 */
static OTAFirmwareTotalInfoType st_typeOTAFirmwareTotalInfo;                  /* OTA总固件信息 */
static FirmwarePortInfoType st_typeOTAFirmwareChildInfo[FILE_MAX_COUNT];      /* OTA子固件信息 */

void vOTAInit(void)
{
    uint32_t uiCrcValue;
    uint8_t cError;

    /* 读取一次OTAInfo信息，更新标志 */
    cError = cOTAReafOTAInfoFromFlash();

    uiCrcValue = uiCRC32(NULL, &st_typeOTAInfo, sizeof(OTAInfoType) - 4);

    /* 使用HEX记录数据或全为FF */
    if (st_typeOTAInfo.OTATotalInfoCrc == 0xFFFFFFFF)
    {
        memset(&st_typeOTAInfo, 0, sizeof(st_typeOTAInfo));
    }
    /* 从Boot启动后检测数据与外部 Flash 记录的固件信息是否一致 */
    else if (st_typeOTAInfo.OTATotalInfoCrc != uiCrcValue)
    {
        /* 修正CRC，避免一直重复执行更新 */
        st_typeOTAInfo.OTATotalInfoCrc = uiCrcValue;
        cError |= cOTAUpdateCrcAndWriteToFlash();

        /* 执行恢复固件更新流程 */
        if (cError == 0)
        {
            cOTAStateSet(OTA_STATE_START);
        }
    }

    g_cOTAInitFlag = cError == 0 ? 1 : 0;
}

uint32_t uiOTACheck()
{
    FirmwarePortInfoType typeFirmwareOTA = {0};
    uint32_t uiError = 0;
    uint16_t usCrc = 0xFFFF;

    /* 读取固件头信息 */
    cSPIFlashReadDatas(SPI_FLASH_OTA_ADDR, &st_typeOTAFirmwareTotalInfo, sizeof(OTAFirmwareTotalInfoType));

    /* 校验总固件头CRC是否正确 */
    if (st_typeOTAFirmwareTotalInfo.crc32Head != usCRC16_MODBUS(&usCrc, &st_typeOTAFirmwareTotalInfo, sizeof(st_typeOTAFirmwareTotalInfo) - 4))
        uiError |= OTA_Error_CrcTotalHeaderError;

    /* 校验固件内容是否正确 */
    typeFirmwareOTA.address = SPI_FLASH_OTA_ADDR + sizeof(OTAFirmwareTotalInfoType);
    typeFirmwareOTA.length = st_typeOTAFirmwareTotalInfo.length;
    typeFirmwareOTA.registers = FIRMWARE_SOURCE_SPI_FLASH;

    if (st_typeOTAFirmwareTotalInfo.crc32 != uiFirmwareCRCUpdate(&typeFirmwareOTA))
        uiError |= OTA_Error_CrcTotalFirmwareError;

    return uiError;
}

int8_t vOTADeinit()
{
    SensorInfoType *ptypeSensorInfo = ptypeSensorInfoGet();
    uint32_t uiTotalLength = 0;
    int8_t cError = 0;

    /* 初始化OTA子模块信息 */
    for (uint8_t i = 0; i < st_typeOTAFirmwareTotalInfo.firmwareNumber; i++)
    {
        cSPIFlashReadDatas(st_typeOTAFirmwareTotalInfo.FirmwareOTAPort[i].startAddr, &st_typeOTAFirmwareChildInfo[i], sizeof(FirmwarePortInfoType));
        st_typeOTAInfo.Port[i].state = OTA_STATE_READY;                                                                                 /* 子固件状态 初始状态 */
        memcpy(st_typeOTAInfo.Port[i].OTAPortVersion, st_typeOTAFirmwareChildInfo[i].versionSoft, 8);                                   /* 子固件软件版本 */
        st_typeOTAInfo.Port[i].error = OTA_Error_NULL;                                                                                  /* 子固件错误码 NULL */
        st_typeOTAInfo.Port[i].type = st_typeOTAFirmwareChildInfo[i].type;                                                              /* 子固件类型 */
        st_typeOTAInfo.Port[i].number = st_typeOTAFirmwareChildInfo[i].number;                                                          /* 子固件编号 */
        st_typeOTAInfo.Port[i].ReSendCount = st_typeOTAFirmwareTotalInfo.reSendCount;                                                   /* 子固件重发次数 */
        st_typeOTAInfo.Port[i].address = st_typeOTAFirmwareTotalInfo.FirmwareOTAPort[i].startAddr + sizeof(FirmwarePortInfoType);       /* 子固件起始地址 */
        st_typeOTAInfo.Port[i].length = st_typeOTAFirmwareTotalInfo.FirmwareOTAPort[i].length - sizeof(FirmwarePortInfoType);           /* 子固件长度 */
        st_typeOTAInfo.Port[i].writeLengthNow = 0;
        st_typeOTAInfo.Port[i].crcValue = st_typeOTAFirmwareChildInfo[i].crcValue;                                                      /* 子固件CRC校验值 */
        if (st_typeOTAInfo.Port[i].type != OTA_Type_PD)
        {
            uiTotalLength += st_typeOTAInfo.Port[i].length;
        }
    }

    /* 初始化OTA总体信息 */
    st_typeOTAInfo.FirmwareLengthTotal = uiTotalLength;
    st_typeOTAInfo.firmwareNumber = st_typeOTAFirmwareTotalInfo.firmwareNumber > FILE_MAX_COUNT ? FILE_MAX_COUNT : st_typeOTAFirmwareTotalInfo.firmwareNumber;
    st_typeOTAInfo.error = OTA_Error_NULL;
    st_typeOTAInfo.OTARemainTime = 0;
    st_typeOTAInfo.OTATotalInfoCrc = uiCRC32(NULL, &st_typeOTAFirmwareTotalInfo, sizeof(st_typeOTAFirmwareTotalInfo));
    cError |= cOTAUpdateCrcAndWriteToFlash();

    /* 初始化当前更新固件号和长度 */
    g_usOTAUpdatingFirmwareNo = (st_typeOTAInfo.Port[0].type * 0x10) + st_typeOTAInfo.Port[0].number;
    g_uiFirmwareLengthNow = 0;

    return cError;
}

void vOTAStart()
{
    SensorInfoType *ptypeSensorInfo = ptypeSensorInfoGet();
    uint32_t uiError = 0;
    int8_t cError = 0;

    if (ptypeSensorInfo->ptypeOTAInfo->state == OTA_STATE_START)
    {
        /* OTA数据校验 */
        uiError |= uiOTACheck();
        if (uiError != 0)
        {
            cOTAStateSet(OTA_STATE_FAIL);
            cOTAErrorSet(uiError);
            return;
        }

        /* OTA信息初始化 */
        if (vOTADeinit() != 0)
        {
            cOTAStateSet(OTA_STATE_FAIL);
            cOTAErrorSet(OTA_Error_OTADeinitError);
            return;
        }

        vTaskDelay(2000 / portTICK_RATE_MS);

        /* 执行分布式更新 */
        cOTAStateSet(OTA_STATE_UPDATING);
    }
}

/* 子模块固件更新接口，若新增或修改OTA子模块，只需修改此处调用逻辑
   Type: 0 PD、1 BMS、2 IVN
   ModuleNo: Type下的不同模块编号
   Type和ModuleNo需按协议定义一致 */
int8_t cOTAFirmwareUpdatePort(OTAFirmwarePortInfoType *ptypeOTAFirmwarePortInfo)
{
    FirmwarePortInfoType typeFirmwareOTA = {0};
    int8_t cError = 0;

    /* 校验子固件CRC是否正确 */
    typeFirmwareOTA.address = ptypeOTAFirmwarePortInfo->address;
    typeFirmwareOTA.length = ptypeOTAFirmwarePortInfo->length;
    typeFirmwareOTA.registers = FIRMWARE_SOURCE_SPI_FLASH;

    if (ptypeOTAFirmwarePortInfo->crcValue == uiFirmwareCRCUpdate(&typeFirmwareOTA))
    {
        /* 分模块处理 */
        switch (ptypeOTAFirmwarePortInfo->type)
        {
            case OTA_Type_PD:
                cError = cOTAUpgradePD(ptypeOTAFirmwarePortInfo);
                break;
        }
    }
    else
    {
        cError = 99;
    }

    return cError;
}
void vOTAFirmwareUpdateAll(void)
{
    SensorInfoType *ptypeSensorInfo = ptypeSensorInfoGet();
    int8_t cError = 0, cError2 = 0;

    /* 轮流分发更新子固件 */
    for (uint8_t i = 0; i < st_typeOTAInfo.firmwareNumber; i++)
    {
        /* 如果子固件为更新状态，说明之前断电或者重启过，需要检查重传次数，否则直接判定失败 */
        if (st_typeOTAInfo.Port[i].state == OTA_STATE_UPDATING)
        {
            if (st_typeOTAInfo.Port[i].ReSendCount > 0)
            {
                goto __UPDATE_BEGIN;
            }
            else
            {
                /* 修改子固件状态为失败，并记录失败的错误码 */
                st_typeOTAInfo.Port[i].state = OTA_STATE_FAIL;
                st_typeOTAInfo.Port[i].error = cError;
                cOTAUpdateCrcAndWriteToFlash();
            }
        }

        /* 如果子固件处于就绪状态，且重传次数 > 0，则开始更新 */
        if (st_typeOTAInfo.Port[i].state == OTA_STATE_READY &&
            st_typeOTAInfo.Port[i].ReSendCount > 0)
        {
__UPDATE_BEGIN:
            /* 设置子固件为更新状态，并减少重传次数 */
            st_typeOTAInfo.Port[i].state = OTA_STATE_UPDATING;
            st_typeOTAInfo.Port[i].ReSendCount--;
            cOTAUpdateCrcAndWriteToFlash();

            /* 执行子固件更新 */
            cError = cOTAFirmwareUpdatePort(&st_typeOTAInfo.Port[i]);

            /* 根据更新结果执行不同的操作 */
            if (cError == 0)
            {
                /* 子固件更新成功 */
                st_typeOTAInfo.Port[i].state = OTA_STATE_SUCCESS;
                st_typeOTAInfo.Port[i].ReSendCount = 0;
                cOTAUpdateCrcAndWriteToFlash();

                if (st_typeOTAInfo.Port[i].type == OTA_Type_PD)
                    NVIC_SystemReset();
            }
            else
            {
                /* 子固件更新失败，回退写入长度 */
                g_uiFirmwareLengthNow -= st_typeOTAInfo.Port[i].writeLengthNow;
                st_typeOTAInfo.Port[i].writeLengthNow = 0;

                /* 如果还有重传次数，继续尝试 */
                if (st_typeOTAInfo.Port[i].ReSendCount > 0)
                {
                    goto __UPDATE_BEGIN;
                }
                else
                {
                    /* 标记更新失败并记录错误码 */
                    st_typeOTAInfo.Port[i].state = OTA_STATE_FAIL;
                    st_typeOTAInfo.Port[i].error = cError;
                    cOTAUpdateCrcAndWriteToFlash();
                }
            }
        }

        /* 如果这是最后一个子固件，检查整体OTA是否成功 */
        if (i == st_typeOTAInfo.firmwareNumber - 1)
        {
            for (uint8_t j = 0; j < st_typeOTAInfo.firmwareNumber; j++)
            {
                if (st_typeOTAInfo.Port[j].state != OTA_STATE_SUCCESS)
                {
                    cError2 = 1;
                }
            }

            if (cError2 == 0)
            {
                cOTAStateSet(OTA_STATE_SUCCESS);
            }
            else
            {
                cOTAStateSet(OTA_STATE_FAIL);
                cOTAErrorSet(OTA_Error_ChildUpdateFail);
            }
        }
    }

    if (st_typeOTAInfo.state == OTA_STATE_SUCCESS)
    {
        cOTAStateSet(OTA_STATE_DISABLE);
        NVIC_SystemReset();
    }
    else if (st_typeOTAInfo.state == OTA_STATE_FAIL)
    {
        cOTAStateSet(OTA_STATE_DISABLE);
    }
}

/* 处理 Modbus OTA 数据包，并写入 Flash */
int8_t cOTAModbusPackAnalysis(ModBusRtuTypeDef *ptypeHandle)
{
    SensorInfoType *ptypeSensorInfo = ptypeSensorInfoGet();
    uint32_t uiAddr = 0, uiCRCValue = 0;
    int32_t iLength = 0;
    int8_t cError = 2;

    /* 提取 OTA 数据包中的地址、长度、CRC */
    memcpy(&uiAddr, &ptypeHandle->data[5], 4);
    memcpy(&iLength, &ptypeHandle->data[9], 4);
    memcpy(&uiCRCValue, &ptypeHandle->data[13], 4);

    /* 大小端转换 */
    uiAddr = uiSwapUint32(uiAddr);
    iLength = uiSwapUint32(iLength);
    uiCRCValue = uiSwapUint32(uiCRCValue);

    /* 校验每帧数据 */
    if (uiCRCValue == usCRC16_MODBUS(NULL, &ptypeHandle->data[17], iLength))
    {
        cError = cSPIFlashWriteDatas(SPI_FLASH_OTA_ADDR + uiAddr, &ptypeHandle->data[17], iLength);
    }

    return cError;
}

int8_t cOTAStateSet(OTAStateEnum enumOTAState)
{
    st_typeOTAInfo.state = enumOTAState;
    return cOTAUpdateCrcAndWriteToFlash();
}

int8_t cOTAErrorSet(uint32_t uiError)
{
    st_typeOTAInfo.error = uiError;
    return cOTAUpdateCrcAndWriteToFlash();
}

/* 更新 OTA 信息 CRC 并写入 Flash */
int8_t cOTAUpdateCrcAndWriteToFlash()
{
    st_typeOTAInfo.OTATotalInfoCrc = uiCRC32(NULL, &st_typeOTAInfo, sizeof(OTAInfoType) - 4);
    return cFlashWriteDatas(FLASH_OTA_DATA_ADDR, &st_typeOTAInfo, sizeof(OTAInfoType));
}

/* 从内部 Flash 读取 OTA 信息 */
int8_t cOTAReafOTAInfoFromFlash(void)
{
    return cFlashReadDatas(FLASH_OTA_DATA_ADDR, &st_typeOTAInfo, sizeof(OTAInfoType));
}

OTAInfoType *ptypeOTAInfoGet(void)
{
    return &st_typeOTAInfo;
}
