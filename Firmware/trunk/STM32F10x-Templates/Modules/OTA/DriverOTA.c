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

uint16_t g_usOTAUpdatingFirmwareNo;                                           /* OTA当前升级序号 */
uint32_t g_uiFirmwareLengthNow;                                               /* OTA本机 已写入的长度 */

static OTAInfoType st_typeOTAInfo;                                            /* OTA升级信息 */
static OTAFirmwareTotalInfoType st_typeOTAFirmwareTotalInfo;                  /* OTA总固件信息 */
static FirmwarePortInfoType st_typeOTAFirmwareChildInfo[FILE_MAX_COUNT];      /* OTA子固件信息 */

void vOTAInit(void)
{
    uint32_t uiCrcValue;
    uint8_t cError;

    /* 读取一次OTAInfo数据,并更新标志 */
    cError = cOTAReafOTAInfoFromFlash();

    uiCrcValue = uiCRC32(NULL,&st_typeOTAInfo,sizeof(OTAInfoType)-4);

    /* 使用HEX烧录后，数据会全是FF */
    if(st_typeOTAInfo.OTATotalInfoCrc == 0xFFFFFFFF)
    {
        memset(&st_typeOTAInfo, 0, sizeof(st_typeOTAInfo));
    }
    /* 从旧Boot升级过来、尝试从外部 Flash 恢复固件数据 */
    else if(st_typeOTAInfo.OTATotalInfoCrc != uiCrcValue)
    {
        /* 更新CRC,避免出现一直重复更新的情况 */
        st_typeOTAInfo.OTATotalInfoCrc = uiCrcValue;
        cError |= cOTAUpdateCrcAndWriteToFlash();

        /* 执行恢复固件数据流程 */
        if(cError == 0)
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

    /* 读取固件头数据  */
    cSPIFlashReadDatas(SPI_FLASH_OTA_ADDR,&st_typeOTAFirmwareTotalInfo,sizeof(OTAFirmwareTotalInfoType));

    /* 校验整包固件头数据是否正确 */
    if(st_typeOTAFirmwareTotalInfo.crc32Head != usCRC16_MODBUS(&usCrc,&st_typeOTAFirmwareTotalInfo,sizeof(st_typeOTAFirmwareTotalInfo)-4))
        uiError |= OTA_Error_CrcTotalHeaderError;

    /* 校验整包数据是否正确 */
    typeFirmwareOTA.address = SPI_FLASH_OTA_ADDR + sizeof(OTAFirmwareTotalInfoType);
    typeFirmwareOTA.length = st_typeOTAFirmwareTotalInfo.length;
    typeFirmwareOTA.registers = FIRMWARE_SOURCE_SPI_FLASH;

    if(st_typeOTAFirmwareTotalInfo.crc32 != uiFirmwareCRCUpdate(&typeFirmwareOTA))
        uiError |= OTA_Error_CrcTotalFirmwareError;

    return uiError;
}

int8_t vOTADeinit()
{
    SensorInfoType *ptypeSensorInfo = ptypeSensorInfoGet();
    uint32_t uiTotalLength = 0;
    int8_t cError = 0;

    /* 初始化OTA子模块数据 */
    for(uint8_t i =0; i< st_typeOTAFirmwareTotalInfo.firmwareNumber; i++)
    {
        cSPIFlashReadDatas(st_typeOTAFirmwareTotalInfo.FirmwareOTAPort[i].startAddr,&st_typeOTAFirmwareChildInfo[i],sizeof(FirmwarePortInfoType));
        st_typeOTAInfo.Port[i].state = OTA_STATE_READY;                                                                                 /* 子固件状态 就绪态*/
        memcpy(st_typeOTAInfo.Port[i].OTAPortVersion,st_typeOTAFirmwareChildInfo[i].versionSoft,8);                                     /* 子固件小版本 */
        st_typeOTAInfo.Port[i].error = OTA_Error_NULL;                                                                                  /* 子固件错误 NULL */
        st_typeOTAInfo.Port[i].type = st_typeOTAFirmwareChildInfo[i].type;                                                              /* 子固件类型 */
        st_typeOTAInfo.Port[i].number = st_typeOTAFirmwareChildInfo[i].number;                                                          /* 子固件固件参数 */
        st_typeOTAInfo.Port[i].ReSendCount = st_typeOTAFirmwareTotalInfo.reSendCount;                                                   /* 子固件重传次数 */
        st_typeOTAInfo.Port[i].address = st_typeOTAFirmwareTotalInfo.FirmwareOTAPort[i].startAddr + sizeof(FirmwarePortInfoType);       /* 子固件起始地址 */
        st_typeOTAInfo.Port[i].length = st_typeOTAFirmwareTotalInfo.FirmwareOTAPort[i].length - sizeof(FirmwarePortInfoType);           /* 子固件长度 */
        st_typeOTAInfo.Port[i].writeLengthNow = 0;
        st_typeOTAInfo.Port[i].crcValue = st_typeOTAFirmwareChildInfo[i].crcValue;                                                      /* 子固件CRC校验值 */
        if(st_typeOTAInfo.Port[i].type != OTA_Type_PD)
        {
            uiTotalLength += st_typeOTAInfo.Port[i].length;
        }
    }

    /* 初始化OTA本机信息 */
    st_typeOTAInfo.FirmwareLengthTotal = uiTotalLength;
    st_typeOTAInfo.firmwareNumber = st_typeOTAFirmwareTotalInfo.firmwareNumber > FILE_MAX_COUNT ? FILE_MAX_COUNT :st_typeOTAFirmwareTotalInfo.firmwareNumber;
    st_typeOTAInfo.error = OTA_Error_NULL;
    st_typeOTAInfo.OTARemainTime = 0;
    st_typeOTAInfo.OTATotalInfoCrc = uiCRC32(NULL,&st_typeOTAFirmwareTotalInfo,sizeof(st_typeOTAFirmwareTotalInfo));
    cError |= cOTAUpdateCrcAndWriteToFlash();

    /* 初始化写入固件序号和长度 */
    g_usOTAUpdatingFirmwareNo = (st_typeOTAInfo.Port[0].type * 0x10) + st_typeOTAInfo.Port[0].number;
    g_uiFirmwareLengthNow = 0;

    return cError;
}

void vOTAStart()
{
    SensorInfoType *ptypeSensorInfo = ptypeSensorInfoGet();
    uint32_t uiError = 0;
    int8_t cError  = 0;

    if(ptypeSensorInfo->ptypeOTAInfo->state == OTA_STATE_START)
    {
        /* OTA数据校验 */
        uiError |= uiOTACheck();
        if(uiError != 0)
        {
            cOTAStateSet(OTA_STATE_FAIL);
            cOTAErrorSet(uiError);
            return; 
        }

        /* OTA信息初始化 */
        if(vOTADeinit() != 0)
        {
            cOTAStateSet(OTA_STATE_FAIL);
            cOTAErrorSet(OTA_Error_OTADeinitError);
            return;
        }

        vTaskDelay(2000 / portTICK_RATE_MS);

        /* 执行分包升级流程 */
        cOTAStateSet(OTA_STATE_UPDATING);
    }
}

/* 子模块升级代码,后续添加或修改OTA升级模块,只需要修改这部分代码即可,其他的代码无需更改 
    Type: 0->PD、1->BMS、2->IVN
    ModuleNo: Type下的不同参数
    Type与ModuleNo需要与上位机沟通并保持一致
*/ 
int8_t cOTAFirmwareUpdatePort(OTAFirmwarePortInfoType *ptypeOTAFirmwarePortInfo)
{
    FirmwarePortInfoType typeFirmwareOTA = {0};
    int8_t cError = 0;

    /* 校验子固件CRC是否正确 */
    typeFirmwareOTA.address = ptypeOTAFirmwarePortInfo->address;
    typeFirmwareOTA.length = ptypeOTAFirmwarePortInfo->length;
    typeFirmwareOTA.registers = FIRMWARE_SOURCE_SPI_FLASH;

    if(ptypeOTAFirmwarePortInfo->crcValue == uiFirmwareCRCUpdate(&typeFirmwareOTA))
    {   
        /* 分包下发 */
        switch(ptypeOTAFirmwarePortInfo->type)
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

    /* 依次分发各个子固件 */
    for(uint8_t i = 0; i < st_typeOTAInfo.firmwareNumber; i++)
    {
        /* 检测子固件为正在升级中,说明是升级过程中被强制关机了,如果还有重传次数直接进入升级流程、没有次数则记录失败 */
        if(st_typeOTAInfo.Port[i].state == OTA_STATE_UPDATING)
        {
            if(st_typeOTAInfo.Port[i].ReSendCount >0)
            {
                goto __UPDATE_BEGIN;
            }
            else
            {
                /* 该子固件状态被修改为失败,并记录失败的步骤 */
                st_typeOTAInfo.Port[i].state = OTA_STATE_FAIL;
                st_typeOTAInfo.Port[i].error = cError;
                cOTAUpdateCrcAndWriteToFlash();
            }
        }

        /* 检测子固件是否处于就绪态,并且升级次数>0 */
        if(st_typeOTAInfo.Port[i].state == OTA_STATE_READY &&
            st_typeOTAInfo.Port[i].ReSendCount >0 )
        {
__UPDATE_BEGIN:
            /* 子固件设置为升级态、升级重试次数-1,并写入到Flash */
            st_typeOTAInfo.Port[i].state = OTA_STATE_UPDATING;
            st_typeOTAInfo.Port[i].ReSendCount--;
            cOTAUpdateCrcAndWriteToFlash();

            /* 执行子固件升级 */
            cError = cOTAFirmwareUpdatePort(&st_typeOTAInfo.Port[i]);

            /* 根据子固件的升级结果执行不同的操作 */
            if(cError == 0)
            {
                /* 子固件升级成功,状态写入成功 */;
                st_typeOTAInfo.Port[i].state = OTA_STATE_SUCCESS;
                st_typeOTAInfo.Port[i].ReSendCount = 0;
                cOTAUpdateCrcAndWriteToFlash();
                
                if(st_typeOTAInfo.Port[i].type == OTA_Type_PD)
                    NVIC_SystemReset();
            }
            else
            {
                g_uiFirmwareLengthNow -= st_typeOTAInfo.Port[i].writeLengthNow;
                st_typeOTAInfo.Port[i].writeLengthNow = 0;
                /* 子固件升级失败,判断有无重试次数,有则重升,无则记录失败 */
                if(st_typeOTAInfo.Port[i].ReSendCount > 0)
                {
                    goto __UPDATE_BEGIN;
                }
                else
                {
                    /* 该子固件状态被修改为失败,并记录失败的步骤 */
                    st_typeOTAInfo.Port[i].state = OTA_STATE_FAIL;
                    st_typeOTAInfo.Port[i].error = cError;
                    cOTAUpdateCrcAndWriteToFlash();
                }
            }
        }
 
        /* 升级到最后一个子固件时,判断本机是否OTA升级成功 */
        if(i == st_typeOTAInfo.firmwareNumber -1)
        {
            /* 遍历所有子固件的OTA状态 */
            for(uint8_t j = 0; j< st_typeOTAInfo.firmwareNumber; j++)
            {
                if(st_typeOTAInfo.Port[j].state != OTA_STATE_SUCCESS)
                {
                    cError2 = 1;
                }
            }

            /* 写入本机的最终的升级状态 */
            if(cError2 == 0)
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

    if(st_typeOTAInfo.state == OTA_STATE_SUCCESS)
    {
        cOTAStateSet(OTA_STATE_DISABLE);
        NVIC_SystemReset();
    }
    else if(st_typeOTAInfo.state == OTA_STATE_FAIL)
    {
        cOTAStateSet(OTA_STATE_DISABLE);
    }
}

/* 上位机固件包接收解析、写入 */
int8_t cOTAModbusPackAnalysis(ModBusRtuTypeDef* ptypeHandle)
{
    SensorInfoType *ptypeSensorInfo = ptypeSensorInfoGet();
    uint32_t uiAddr = 0, uiCRCValue = 0;
    int32_t iLength = 0;
    int8_t cError = 2;

    /* 解包格式请查阅OTA设计文档 */
    memcpy(&uiAddr,     &ptypeHandle->data[5], 4);
    memcpy(&iLength,    &ptypeHandle->data[9], 4);
    memcpy(&uiCRCValue, &ptypeHandle->data[13], 4);
    /* 需要进行大小端转换 */
    uiAddr      = uiSwapUint32(uiAddr);
    iLength     = uiSwapUint32(iLength);
    uiCRCValue  = uiSwapUint32(uiCRCValue);

    /* 每帧数据都需要校验 */
    if(uiCRCValue == usCRC16_MODBUS(NULL, &ptypeHandle->data[17], iLength))
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

/* OTA更新CRC,并写入到Flash */
int8_t cOTAUpdateCrcAndWriteToFlash()
{
    st_typeOTAInfo.OTATotalInfoCrc = uiCRC32(NULL,&st_typeOTAInfo,sizeof(OTAInfoType)-4);

    return cFlashWriteDatas(FLASH_OTA_DATA_ADDR,&st_typeOTAInfo,sizeof(OTAInfoType));
}

/* 读取内部Flash数据 */
int8_t cOTAReafOTAInfoFromFlash(void)
{
    return cFlashReadDatas(FLASH_OTA_DATA_ADDR,&st_typeOTAInfo,sizeof(OTAInfoType));
}

OTAInfoType *ptypeOTAInfoGet(void)
{
    return &st_typeOTAInfo;
}

