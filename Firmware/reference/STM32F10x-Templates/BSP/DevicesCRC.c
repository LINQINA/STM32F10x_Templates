#include "stdio.h"
#include "stdint.h"
#include "DevicesCRC.h"

uint8_t ucCRC7_MMC(uint8_t *pucInitCRC, void *pvBuff, int32_t iLength)
{
    uint8_t *pucDataBuff = pvBuff;
    uint8_t ucInputCRC;
    int8_t i;

    if((pvBuff == NULL) || (iLength < 1))
        return 0;

    ucInputCRC = (pucInitCRC != NULL) ? *pucInitCRC : 0;

    while((iLength--) > 0)
    {
        ucInputCRC ^= *pucDataBuff++;

        for(i = 0; i < 8; ++i)
        {
            if (ucInputCRC & 0x80)
                ucInputCRC = (ucInputCRC << 1) ^ 0x12;
            else
                ucInputCRC <<= 1;
        }
    }

    return ucInputCRC >> 1;
}

uint8_t ucCRC8(uint8_t *pucInitCRC, void *pvBuff, int32_t iLength)
{
    uint8_t *pucDataBuff = pvBuff;
    uint8_t ucInputCRC;
    int8_t i;

    if((pvBuff == NULL) || (iLength < 1))
        return 0;

    ucInputCRC = (pucInitCRC != NULL) ? *pucInitCRC : 0;

    while((iLength--) > 0)
    {
        ucInputCRC ^= *pucDataBuff++;

        for(i = 0; i < 8; ++i)
        {
            if (ucInputCRC & 0x80)
                ucInputCRC = (ucInputCRC << 1) ^ 0x07;
            else
                ucInputCRC <<= 1;
        }
    }

    return ucInputCRC;
}

uint8_t ucCRC8_ITU(uint8_t *pucInitCRC, void *pvBuff, int32_t iLength)
{
    uint8_t *pucDataBuff = pvBuff;
    uint8_t ucInputCRC;
    int8_t i;

    if((pvBuff == NULL) || (iLength < 1))
        return 0;

    ucInputCRC = (pucInitCRC != NULL) ? *pucInitCRC : 0;

    while((iLength--) > 0)
    {
        ucInputCRC ^= *pucDataBuff++;

        for(i = 0; i < 8; ++i)
        {
            if (ucInputCRC & 0x80)
                ucInputCRC = (ucInputCRC << 1) ^ 0x07;
            else
                ucInputCRC <<= 1;
        }
    }

    return ucInputCRC ^ 0x55;
}

uint8_t ucCRC8_ROHC(uint8_t *pucInitCRC, void *pvBuff, int32_t iLength)
{
    uint8_t *pucDataBuff = pvBuff;
    uint8_t ucInputCRC;
    int8_t i;

    if((pvBuff == NULL) || (iLength < 1))
        return 0;

    ucInputCRC = (pucInitCRC != NULL) ? *pucInitCRC : 0xFF;

    while((iLength--) > 0)
    {
        ucInputCRC ^= *pucDataBuff++;

        for(i = 0; i < 8; ++i)
        {
            if (ucInputCRC & 1)
                ucInputCRC = (ucInputCRC >> 1) ^ 0xE0;
            else
                ucInputCRC >>= 1;
        }
    }

    return ucInputCRC;
}

uint8_t ucCRC8_MAXIM(uint8_t *pucInitCRC, void *pvBuff, int32_t iLength)
{
    uint8_t *pucDataBuff = pvBuff;
    uint8_t ucInputCRC;
    int8_t i;

    if((pvBuff == NULL) || (iLength < 1))
        return 0;

    ucInputCRC = (pucInitCRC != NULL) ? *pucInitCRC : 0;

    while((iLength--) > 0)
    {
        ucInputCRC ^= *pucDataBuff++;

        for(i = 0; i < 8; ++i)
        {
            if (ucInputCRC & 1)
                ucInputCRC = (ucInputCRC >> 1) ^ 0x8C;
            else
                ucInputCRC >>= 1;
        }
    }

    return ucInputCRC;
}




uint16_t usCRC16_IBM(uint16_t *pusInitCRC, void *pvBuff, int32_t iLength)
{
    uint8_t *pucDataBuff = pvBuff;
    uint16_t usInputCRC;
    int8_t i;

    if((pvBuff == NULL) || (iLength < 1))
        return 0;

    usInputCRC = (pusInitCRC != NULL) ? *pusInitCRC : 0xFFFF;

    while((iLength--) > 0)
    {
        usInputCRC ^= *pucDataBuff++;

        for(i = 0; i < 8; ++i)
        {
            if (usInputCRC & 1)
                usInputCRC = (usInputCRC >> 1) ^ 0xA001;
            else
                usInputCRC >>= 1;
        }
    }

    return usInputCRC;
}

uint16_t usCRC16_MAXIM(uint16_t *pusInitCRC, void *pvBuff, int32_t iLength)
{
    uint8_t *pucDataBuff = pvBuff;
    uint16_t usInputCRC;
    int8_t i;

    if((pvBuff == NULL) || (iLength < 1))
        return 0;

    usInputCRC = (pusInitCRC != NULL) ? *pusInitCRC : 0xFFFF;

    while((iLength--) > 0)
    {
        usInputCRC ^= *pucDataBuff++;

        for(i = 0; i < 8; ++i)
        {
            if (usInputCRC & 1)
                usInputCRC = (usInputCRC >> 1) ^ 0xA001;
            else
                usInputCRC >>= 1;
        }
    }

    return ~usInputCRC;
}

uint16_t usCRC16_USB(uint16_t *pusInitCRC, void *pvBuff, int32_t iLength)
{
    uint8_t *pucDataBuff = pvBuff;
    uint16_t usInputCRC;
    int8_t i;

    if((pvBuff == NULL) || (iLength < 1))
        return 0;

    usInputCRC = (pusInitCRC != NULL) ? *pusInitCRC : 0xFFFF;

    while((iLength--) > 0)
    {
        usInputCRC ^= *pucDataBuff++;

        for(i = 0; i < 8; ++i)
        {
            if (usInputCRC & 1)
                usInputCRC = (usInputCRC >> 1) ^ 0xA001;
            else
                usInputCRC >>= 1;
        }
    }

    return ~usInputCRC;
}

uint16_t usCRC16_MODBUS(uint16_t *pusInitCRC, void *pvBuff, int32_t iLength)
{
    uint8_t *pucDataBuff = pvBuff;
    uint16_t usInputCRC;
    int8_t i;

    if((pvBuff == NULL) || (iLength < 1))
        return 0;

    usInputCRC = (pusInitCRC != NULL) ? *pusInitCRC : 0xFFFF;

    while((iLength--) > 0)
    {
        usInputCRC ^= *pucDataBuff++;

        for(i = 0; i < 8; ++i)
        {
            if (usInputCRC & 1)
                usInputCRC = (usInputCRC >> 1) ^ 0xA001;
            else
                usInputCRC >>= 1;
        }
    }

    return usInputCRC;
}

uint16_t usCRC16_CCITT(uint16_t *pusInitCRC, void *pvBuff, int32_t iLength)
{
    uint8_t *pucDataBuff = pvBuff;
    uint16_t usInputCRC;
    int8_t i;

    if((pvBuff == NULL) || (iLength < 1))
        return 0;

    usInputCRC = (pusInitCRC != NULL) ? *pusInitCRC : 0;

    while((iLength--) > 0)
    {
        usInputCRC ^= *pucDataBuff++;

        for(i = 0; i < 8; ++i)
        {
            if (usInputCRC & 1)
                usInputCRC = (usInputCRC >> 1) ^ 0x8408;
            else
                usInputCRC >>= 1;
        }
    }

    return usInputCRC;
}

uint16_t usCRC16_CCITT_FALSE(uint16_t *pusInitCRC, void *pvBuff, int32_t iLength)
{
    uint8_t *pucDataBuff = pvBuff;
    uint16_t usInputCRC;
    int8_t i;

    if((pvBuff == NULL) || (iLength < 1))
        return 0;

    usInputCRC = (pusInitCRC != NULL) ? *pusInitCRC : 0xFFFF;

    while((iLength--) > 0)
    {
        usInputCRC ^= (uint16_t)(*pucDataBuff++) << 8;

        for(i = 0; i < 8; ++i)
        {
            if (usInputCRC & 0x8000)
                usInputCRC = (usInputCRC << 1) ^ 0x1021;
            else
                usInputCRC <<= 1;
        }
    }

    return usInputCRC;
}

uint16_t usCRC16_X25(uint16_t *pusInitCRC, void *pvBuff, int32_t iLength)
{
    uint8_t *pucDataBuff = pvBuff;
    uint16_t usInputCRC;
    int8_t i;

    if((pvBuff == NULL) || (iLength < 1))
        return 0;

    usInputCRC = (pusInitCRC != NULL) ? *pusInitCRC : 0xFFFF;

    while((iLength--) > 0)
    {
        usInputCRC ^= *pucDataBuff++;

        for(i = 0; i < 8; ++i)
        {
            if (usInputCRC & 1)
                usInputCRC = (usInputCRC >> 1) ^ 0x8408;
            else
                usInputCRC >>= 1;
        }
    }

    return ~usInputCRC;
}

uint16_t usCRC16_XMODEM(uint16_t *pusInitCRC, void *pvBuff, int32_t iLength)
{
    uint8_t *pucDataBuff = pvBuff;
    uint16_t usInputCRC;
    int8_t i;

    if((pvBuff == NULL) || (iLength < 1))
        return 0;

    usInputCRC = (pusInitCRC != NULL) ? *pusInitCRC : 0;

    while((iLength--) > 0)
    {
        usInputCRC ^= (uint16_t)(*pucDataBuff++) << 8;

        for(i = 0; i < 8; ++i)
        {
            if (usInputCRC & 0x8000)
                usInputCRC = (usInputCRC << 1) ^ 0x1021;
            else
                usInputCRC <<= 1;
        }
    }

    return usInputCRC;
}

uint16_t usCRC16_DNP(uint16_t *pusInitCRC, void *pvBuff, int32_t iLength)
{
    uint8_t *pucDataBuff = pvBuff;
    uint16_t usInputCRC;
    int8_t i;

    if((pvBuff == NULL) || (iLength < 1))
        return 0;

    usInputCRC = (pusInitCRC != NULL) ? *pusInitCRC : 0;

    while((iLength--) > 0)
    {
        usInputCRC ^= *pucDataBuff++;

        for(i = 0; i < 8; ++i)
        {
            if (usInputCRC & 1)
                usInputCRC = (usInputCRC >> 1) ^ 0xA6BC;
            else
                usInputCRC >>= 1;
        }
    }

    return ~usInputCRC;
}




uint32_t uiReflect(uint32_t uiData, uint8_t ucLength)
{
    uint32_t uiMask = 1 << (ucLength - 1), uiMaskRef = 1, uiDataReturn = 0;

    for(; uiMask; uiMask >>= 1)
    {
        if(uiData & uiMask)
            uiDataReturn |= uiMaskRef;

        uiMaskRef <<= 1;
    }

    return uiDataReturn;
}

uint32_t uiCRC32(uint32_t *puiInitCRC, void *pvBuff, int32_t iLength)
{
    uint8_t *pucDataBuff = pvBuff;
    uint32_t uiInputCRC, uiPolynomial = 0x04C11DB7;
    int8_t i;

    if((pvBuff == NULL) || (iLength < 1))
        return 0;

    uiInputCRC = (puiInitCRC != NULL) ? *puiInitCRC : 0xFFFFFFFF;

    uiPolynomial = uiReflect(uiPolynomial, 32);

    while((iLength--) > 0)
    {
        uiInputCRC ^= *pucDataBuff++;

        for(i = 0; i < 8; ++i)
        {
            if(uiInputCRC & 1)
                uiInputCRC = (uiInputCRC >> 1) ^ uiPolynomial;
            else
                uiInputCRC >>= 1;
        }
    }

    return ~uiInputCRC;
}

uint32_t uiCRC32_BZIP2(uint32_t *puiInitCRC, void *pvBuff, int32_t iLength)
{
    uint8_t *pucDataBuff = pvBuff;
    uint32_t uiInputCRC;
    int8_t i;

    if((pvBuff == NULL) || (iLength < 1))
        return 0;

    uiInputCRC = (puiInitCRC != NULL) ? *puiInitCRC : 0xFFFFFFFF;

    while((iLength--) > 0)
    {
        uiInputCRC ^= (uint32_t)(*pucDataBuff++) << 24;

        for(i = 0; i < 8; ++i)
        {
            if(uiInputCRC & 0x80000000)
                uiInputCRC = (uiInputCRC << 1) ^ 0x04C11DB7;
            else
                uiInputCRC <<= 1;
        }
    }

    return ~uiInputCRC;
}

uint32_t uiCRC32_MPEG2(uint32_t *puiInitCRC, void *pvBuff, int32_t iLength)
{
    uint8_t *pucDataBuff = pvBuff;
    uint32_t uiInputCRC;
    int8_t i;

    if((pvBuff == NULL) || (iLength < 1))
        return 0;

    uiInputCRC = (puiInitCRC != NULL) ? *puiInitCRC : 0xFFFFFFFF;

    while((iLength--) > 0)
    {
        uiInputCRC ^= (uint32_t)(*pucDataBuff++) << 24;

        for(i = 0; i < 8; ++i)
        {
            if(uiInputCRC & 0x80000000)
                uiInputCRC = (uiInputCRC << 1) ^ 0x04C11DB7;
            else
                uiInputCRC <<= 1;
        }
    }

    return uiInputCRC;
}

uint32_t uiCRC32_POSIX(uint32_t *puiInitCRC, void *pvBuff, int32_t iLength)
{
    uint8_t *pucDataBuff = pvBuff;
    uint32_t uiInputCRC;
    int8_t i;

    if((pvBuff == NULL) || (iLength < 1))
        return 0;

    uiInputCRC = (puiInitCRC != NULL) ? *puiInitCRC : 0;

    while((iLength--) > 0)
    {
        uiInputCRC ^= (uint32_t)(*pucDataBuff++) << 24;

        for(i = 0; i < 8; ++i)
        {
            if(uiInputCRC & 0x80000000)
                uiInputCRC = (uiInputCRC << 1) ^ 0x04C11DB7;
            else
                uiInputCRC <<= 1;
        }
    }

    return ~uiInputCRC;
}

uint32_t uiCRC32_JAMCRC(uint32_t *puiInitCRC, void *pvBuff, int32_t iLength)
{
    uint8_t *pucDataBuff = pvBuff;
    uint32_t uiInputCRC, uiPolynomial = 0x04C11DB7;
    int8_t i;

    if((pvBuff == NULL) || (iLength < 1))
        return 0;

    uiInputCRC = (puiInitCRC != NULL) ? *puiInitCRC : 0xFFFFFFFF;

    uiPolynomial = uiReflect(uiPolynomial, 32);

    while((iLength--) > 0)
    {
        uiInputCRC ^= *pucDataBuff++;

        for(i = 0; i < 8; ++i)
        {
            if(uiInputCRC & 1)
                uiInputCRC = (uiInputCRC >> 1) ^ uiPolynomial;
            else
                uiInputCRC >>= 1;
        }
    }

    return uiInputCRC;
}

uint32_t uiCRC32_STM32(uint32_t *puiInitCRC, void *pvBuff, int32_t iLength)
{
    uint32_t *pucDataBuff = pvBuff;
    uint32_t uiInputCRC, xbit = 0x80000000, uiDataTemp = 0;
    int8_t i;

    if((pvBuff == NULL) || (iLength < 1))
        return 0;

    uiInputCRC = (puiInitCRC != NULL) ? *puiInitCRC : 0xFFFFFFFF;

    while((iLength--) > 0)
    {
        uiDataTemp = *pucDataBuff++;
        uiInputCRC ^= uiDataTemp;
        xbit = 0x80000000;

        for(i = 0; i < 32; ++i)
        {
            if(uiInputCRC & 0x80000000)
                uiInputCRC = (uiInputCRC << 1) ^ 0x04C11DB7;
            else
                uiInputCRC <<= 1;

            if(uiDataTemp & xbit)
                uiInputCRC ^= 0x04C11DB7;

            xbit >>= 1;
        }
    }

    return uiInputCRC;
}
