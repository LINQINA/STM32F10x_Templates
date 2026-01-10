#include "stdint.h"
#include "stdio.h"
#include "string.h"
#include "DevicesCRC.h"
#include "DevicesModbus.h"


int8_t cModbusPackRTU_03(uint8_t ucSalaveAdd, uint16_t usRegisterAddr, uint16_t usLength, void *pvBuff)
{
    uint8_t *pPackBuff = pvBuff;
    uint16_t usCRCValue = 0;

    if(pPackBuff == NULL)
        return 1;

    pPackBuff[0] = ucSalaveAdd;
    pPackBuff[1] = MODBUS_CODE_0x03;
    pPackBuff[2] = usRegisterAddr >> 8;
    pPackBuff[3] = usRegisterAddr;
    pPackBuff[4] = usLength >> 8;
    pPackBuff[5] = usLength;

    usCRCValue = usCRC16_MODBUS(NULL, pPackBuff, 6);

    pPackBuff[6] = usCRCValue;
    pPackBuff[7] = usCRCValue >> 8;

    return 0;
}

int8_t cModbusPackRTU_06(uint8_t ucSalaveAdd, uint16_t usRegisterAddr, uint16_t usValue, void *pvBuff)
{
    uint8_t *pPackBuff = pvBuff;
    uint16_t usCRCValue = 0;

    if(pPackBuff == NULL)
        return 1;

    pPackBuff[0] = ucSalaveAdd;
    pPackBuff[1] = MODBUS_CODE_0x06;
    pPackBuff[2] = usRegisterAddr >> 8;
    pPackBuff[3] = usRegisterAddr;
    pPackBuff[4] = usValue >> 8;
    pPackBuff[5] = usValue;

    usCRCValue = usCRC16_MODBUS(NULL, pPackBuff, 6);

    pPackBuff[6] = usCRCValue;
    pPackBuff[7] = usCRCValue >> 8;

    return 0;
}

int8_t cModbusPackRTU_10(uint8_t ucSalaveAdd, uint16_t usRegisterAddr, uint16_t usLength, void *pvWriteBuff, void *pvBuff)
{
    uint16_t *pWriteBuff = pvWriteBuff;
    uint8_t *pPackBuff = pvBuff, *pucBuff = &pPackBuff[7];
    uint16_t usCRCValue = 0, i = 0;

    if(pPackBuff == NULL)
        return 1;

    /* 最大长度 */
    if(usLength > 123)
        return 2;

    pPackBuff[0] = ucSalaveAdd;
    pPackBuff[1] = MODBUS_CODE_0x10;
    pPackBuff[2] = usRegisterAddr >> 8;
    pPackBuff[3] = usRegisterAddr;
    pPackBuff[4] = usLength >> 8;
    pPackBuff[5] = usLength;
    pPackBuff[6] = usLength * 2;

    /* 高Byte在前 */
    for(i = 0; i < usLength; ++i)
    {
        *pucBuff++ = *pWriteBuff >> 8;
        *pucBuff++ = *pWriteBuff;

        ++pWriteBuff;
    }

    usCRCValue = usCRC16_MODBUS(NULL, pPackBuff, 7 + usLength * 2);

    pPackBuff[7 + usLength * 2 + 0] = usCRCValue;
    pPackBuff[7 + usLength * 2 + 1] = usCRCValue >> 8;

    return 0;
}

int8_t cModbusPackReplyRTU_03(uint8_t ucSalaveAdd, uint16_t usLength, void *pvWriteBuff, void *pvBuff)
{
    uint16_t *pWriteBuff = pvWriteBuff;
    uint8_t *pPackBuff = pvBuff, *pucBuff = &pPackBuff[3];
    uint16_t usCRCValue = 0, i = 0;

    if(pPackBuff == NULL)
        return 1;

    /* 最大长度 */
    if(usLength > 125)
        return 2;

    pPackBuff[0] = ucSalaveAdd;
    pPackBuff[1] = MODBUS_CODE_0x03;
    pPackBuff[2] = usLength * 2;

    /* 高Byte在前 */
    for(i = 0; i < usLength; ++i)
    {
        *pucBuff++ = *pWriteBuff >> 8;
        *pucBuff++ = *pWriteBuff;

        ++pWriteBuff;
    }

    usCRCValue = usCRC16_MODBUS(NULL, pPackBuff, 3 + usLength * 2);

    pPackBuff[3 + usLength * 2 + 0] = usCRCValue;
    pPackBuff[3 + usLength * 2 + 1] = usCRCValue >> 8;

    return 0;
}

int8_t cModbusPackReplyRTU_06(uint8_t ucSalaveAdd, uint16_t usRegisterAddr, uint16_t usValue, void *pvBuff)
{
    return cModbusPackRTU_06(ucSalaveAdd, usRegisterAddr, usValue, pvBuff);
}

int8_t cModbusPackReplyRTU_10(uint8_t ucSalaveAdd, uint16_t usRegisterAddr, uint16_t usLength, void *pvBuff)
{
    uint8_t *pPackBuff = pvBuff;
    uint16_t usCRCValue = 0;

    if(pPackBuff == NULL)
        return 1;

    pPackBuff[0] = ucSalaveAdd;
    pPackBuff[1] = MODBUS_CODE_0x10;
    pPackBuff[2] = usRegisterAddr >> 8;
    pPackBuff[3] = usRegisterAddr;
    pPackBuff[4] = usLength >> 8;
    pPackBuff[5] = usLength;

    usCRCValue = usCRC16_MODBUS(NULL, pPackBuff, 6);

    pPackBuff[6] = usCRCValue;
    pPackBuff[7] = usCRCValue >> 8;

    return 0;
}

int8_t cModbusPackReplyRTU_ErrorCode(uint8_t ucSalaveAdd, uint8_t ucFunc, uint8_t ucErrorCode, void *pvBuff)
{
    uint8_t *pPackBuff = pvBuff;
    uint16_t usCRCValue = 0;

    if(pPackBuff == NULL)
        return 1;

    pPackBuff[0] = ucSalaveAdd;
    pPackBuff[1] = ucFunc | 0x80;
    pPackBuff[2] = ucErrorCode;

    usCRCValue = usCRC16_MODBUS(NULL, pPackBuff, 3);

    pPackBuff[3] = usCRCValue;
    pPackBuff[4] = usCRCValue >> 8;

    return 0;
}

/* 解包 Modbus 回复的数据 */
ModbusUnpackStateEnum enumModbusReplyUnpack(ModBusRtuTypeDef *ptypeData, uint8_t ucValue)
{
    uint16_t usCRCValue = 0;

    if(ptypeData == NULL)
        return MODBUS_UNPACK_ADDRESS;

    if(ptypeData->state == MODBUS_UNPACK_SUCCEED)
    {
        ptypeData->state = MODBUS_UNPACK_ADDRESS;
    }

    switch(ptypeData->state)
    {
MODBUS_UNPACK_ADDRESS:
        /* 设备地址 */
        case MODBUS_UNPACK_ADDRESS:
            ptypeData->slaveAddress = ucValue;

            /* 有效地址判断 */
            if((ucValue == MODBUS_ADDRESS_PD)   ||
               (ucValue == MODBUS_ADDRESS_BMS)  ||
               (ucValue == MODBUS_ADDRESS_INV0) )
            {
                ptypeData->state = MODBUS_UNPACK_FUNC;
            }
            break;

        /* 功能码 */
        case MODBUS_UNPACK_FUNC:
            ptypeData->func = ucValue;
            ptypeData->length = 0;

            /* 错误码 */
            if(ucValue & 0x80)
            {
                ptypeData->state = MODBUS_UNPACK_DATA;
            }
            /* 功能码判断 */
            else if((ucValue == MODBUS_CODE_0x03) || (ucValue == MODBUS_CODE_0x06) || (ucValue == MODBUS_CODE_0x10))
            {
                ptypeData->state = MODBUS_UNPACK_DATA;
            }
            else
            {
                ptypeData->state = MODBUS_UNPACK_ADDRESS;
                /* 重新开始解析 */
                goto MODBUS_UNPACK_ADDRESS;
            }
            break;

        /* 数据 */
        case MODBUS_UNPACK_DATA:
            ptypeData->data[ptypeData->length++] = ucValue;

            switch(ptypeData->func)
            {
                case MODBUS_CODE_0x03: ptypeData->state = (ptypeData->length >= (uint16_t)(ptypeData->data[0]) + 1) ? MODBUS_UNPACK_CRC_LOW : ptypeData->state; break;
                case MODBUS_CODE_0x06: ptypeData->state = (ptypeData->length >= 4) ? MODBUS_UNPACK_CRC_LOW : ptypeData->state; break;
                case MODBUS_CODE_0x10: ptypeData->state = (ptypeData->length >= 4) ? MODBUS_UNPACK_CRC_LOW : ptypeData->state; break;

                default : ptypeData->state = (ptypeData->func & 0x80) ? MODBUS_UNPACK_CRC_LOW : MODBUS_UNPACK_ADDRESS; break;
            }
            break;

        /* 校验码低位 */
        case MODBUS_UNPACK_CRC_LOW:
            ptypeData->crc16[0] = ucValue;
            ptypeData->state = MODBUS_UNPACK_CRC_HIGH;
            break;

        /* 校验码高位 */
        case MODBUS_UNPACK_CRC_HIGH:
            ptypeData->crc16[1] = ucValue;

            /* 错误码回复 */
            if((ptypeData->func & 0x80) != 0)
            {
                usCRCValue = usCRC16_MODBUS(NULL, (uint8_t *)ptypeData, 3); break;
            }
            /* 正常回复 */
            else
            {
                switch(ptypeData->func)
                {
                    case MODBUS_CODE_0x03: usCRCValue = usCRC16_MODBUS(NULL, (uint8_t *)ptypeData, ptypeData->data[0] + 3); break;
                    case MODBUS_CODE_0x06: usCRCValue = usCRC16_MODBUS(NULL, (uint8_t *)ptypeData, 6); break;
                    case MODBUS_CODE_0x10: usCRCValue = usCRC16_MODBUS(NULL, (uint8_t *)ptypeData, 6); break;
                    default : break;
                }
            }

            ptypeData->state = ((ptypeData->crc16[0] == (usCRCValue & 0xFF)) && (ptypeData->crc16[1] == (usCRCValue >> 8))) ? MODBUS_UNPACK_SUCCEED : MODBUS_UNPACK_ADDRESS;
            break;

        default:
            ptypeData->state = MODBUS_UNPACK_ADDRESS;
            break;
    }

    return ptypeData->state;
}

/* 解包 Modbus 要发送的数据 */
ModbusUnpackStateEnum enumModbusUnpack(ModBusRtuTypeDef *ptypeData, uint8_t ucValue)
{
    uint16_t usCRCValue = 0;

    if(ptypeData == NULL)
        return MODBUS_UNPACK_ADDRESS;

    if(ptypeData->state == MODBUS_UNPACK_SUCCEED)
    {
        ptypeData->state = MODBUS_UNPACK_ADDRESS;
    }

    switch(ptypeData->state)
    {
MODBUS_UNPACK_ADDRESS:
        /* 设备地址 */
        case MODBUS_UNPACK_ADDRESS:
            ptypeData->slaveAddress = ucValue;

            /* 有效地址判断 */
            if((ucValue == MODBUS_ADDRESS_PD)   ||
               (ucValue == MODBUS_ADDRESS_BMS)  ||
               (ucValue == MODBUS_ADDRESS_INV0))
            {
                ptypeData->state = MODBUS_UNPACK_FUNC;
            }
            break;

        /* 功能码 */
        case MODBUS_UNPACK_FUNC:
            ptypeData->func = ucValue;
            ptypeData->length = 0;

            /* 功能码判断 */
            if((ucValue == MODBUS_CODE_0x03) || (ucValue == MODBUS_CODE_0x06) || (ucValue == MODBUS_CODE_0x10))
            {
                ptypeData->state = MODBUS_UNPACK_DATA;
            }
            else
            {
                ptypeData->state = MODBUS_UNPACK_ADDRESS;
                /* 重新开始解析 */
                goto MODBUS_UNPACK_ADDRESS;
            }
            break;

        /* 数据 */
        case MODBUS_UNPACK_DATA:
            ptypeData->data[ptypeData->length++] = ucValue;

            switch(ptypeData->func)
            {
                case MODBUS_CODE_0x03: ptypeData->state = (ptypeData->length >= 4) ? MODBUS_UNPACK_CRC_LOW : ptypeData->state; break;
                case MODBUS_CODE_0x06: ptypeData->state = (ptypeData->length >= 4) ? MODBUS_UNPACK_CRC_LOW : ptypeData->state; break;
                case MODBUS_CODE_0x10: ptypeData->state = (ptypeData->length >= (uint16_t)(ptypeData->data[4]) + 5) ? MODBUS_UNPACK_CRC_LOW : ptypeData->state; break;

                default : ptypeData->state = MODBUS_UNPACK_ADDRESS; break;
            }

            /* 超出modbus协议限制长度 */
            ptypeData->state = (ptypeData->length >= 252) ? MODBUS_UNPACK_ADDRESS : ptypeData->state;
            break;

        /* 校验码低位 */
        case MODBUS_UNPACK_CRC_LOW:
            ptypeData->crc16[0] = ucValue;
            /* 此条赋值语句是为了方便后面的透传 */
            ptypeData->data[ptypeData->length++] = ucValue;

            ptypeData->state = MODBUS_UNPACK_CRC_HIGH;
            break;

        /* 校验码高位 */
        case MODBUS_UNPACK_CRC_HIGH:
            ptypeData->crc16[1] = ucValue;
            /* 此条赋值语句是为了方便后面的透传 */
            ptypeData->data[ptypeData->length++] = ucValue;

            switch(ptypeData->func)
            {
                case MODBUS_CODE_0x03: usCRCValue = usCRC16_MODBUS(NULL, (uint8_t *)ptypeData, 6); break;
                case MODBUS_CODE_0x06: usCRCValue = usCRC16_MODBUS(NULL, (uint8_t *)ptypeData, 6); break;
                case MODBUS_CODE_0x10: usCRCValue = usCRC16_MODBUS(NULL, (uint8_t *)ptypeData, ptypeData->data[4] + 7); break;
                default : break;
            }

            ptypeData->state = ((ptypeData->crc16[0] == (usCRCValue & 0xFF)) && (ptypeData->crc16[1] == (usCRCValue >> 8))) ? MODBUS_UNPACK_SUCCEED : MODBUS_UNPACK_ADDRESS;
            break;

        default:
            ptypeData->state = MODBUS_UNPACK_ADDRESS;
            /* 重新开始解析 */
            goto MODBUS_UNPACK_ADDRESS;
    }

    return ptypeData->state;
}

/* 解析 Modbus 回复的数据 */
ModbusUnpackStateEnum enumModbusReplyUnpackDatas(ModBusRtuTypeDef *ptypeHandle, void *pvBuff, int32_t iLength)
{
    uint8_t *pucBuff = pvBuff;
    int32_t i = 0;
    uint8_t ucData = 0;

    if((ptypeHandle == NULL) || (pvBuff == NULL))
        return MODBUS_UNPACK_ERROR;

    while((iLength--) > 0)
    {
        /* Modbus 数据解包 */
        if(enumModbusReplyUnpack(ptypeHandle, *pucBuff++) == MODBUS_UNPACK_SUCCEED)
        {
            if(ptypeHandle->func == MODBUS_CODE_0x03)
            {
                /* 数据区域大小端转换 */
                for(i = 1; i < ptypeHandle->length; i += 2)
                {
                    ucData = ptypeHandle->data[i];
                    ptypeHandle->data[i] = ptypeHandle->data[i + 1];
                    ptypeHandle->data[i + 1] = ucData;
                }
            }

            return MODBUS_UNPACK_SUCCEED;
        }
    }

    return MODBUS_UNPACK_ERROR;
}

/* 解析 Modbus 要发送的数据 */
ModbusUnpackStateEnum enumModbusUnpackDatas(ModBusRtuTypeDef *ptypeHandle, void *pvBuff, int32_t iLength)
{
    uint8_t *pucBuff = pvBuff;

    if((ptypeHandle == NULL) || (pvBuff == NULL))
        return MODBUS_UNPACK_ERROR;

    while((iLength--) > 0)
    {
        /* Modbus 数据解包 */
        if(enumModbusUnpack(ptypeHandle, *pucBuff++) == MODBUS_UNPACK_SUCCEED)
        {
            return MODBUS_UNPACK_SUCCEED;
        }
    }

    return MODBUS_UNPACK_ERROR;
}
