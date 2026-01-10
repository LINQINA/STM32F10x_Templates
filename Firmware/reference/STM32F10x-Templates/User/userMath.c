#include "stdint.h"
#include "userMath.h"


uint32_t uiHexToDec(char *cHex)
{
    uint32_t uiValue = 0, uiValueTemp = 0;

    while(*cHex != 0)
    {
        if((*cHex >= 'a') && (*cHex <= 'z'))
            uiValueTemp = *cHex - 'a' + 10;
        else if((*cHex >= 'A') && (*cHex <= 'Z'))
            uiValueTemp = *cHex - 'A' + 10;
        else if((*cHex >= '0') && (*cHex <= '9'))
            uiValueTemp = *cHex - '0';
        else
            uiValueTemp = 0;

        uiValue = (uiValue << 4) + uiValueTemp;

        ++cHex;
    }

    return uiValue;
}

/* 使 iValue 对 iBase 对齐（任意值） */
int32_t iRoundUp(int32_t iValue, int32_t iBase)
{
    if((iValue % iBase) == 0)
        return iValue;

    iValue += iBase - (iValue % iBase);

    return iValue;
}

/* 使 iValue 对 iBase 对齐 （任意值）*/
int32_t iRoundDown(int32_t iValue, int32_t iBase)
{
    if((iValue % iBase) == 0)
        return iValue;

    iValue -= (iValue % iBase);

    return iValue;
}

/* 32bit大小端转换 */
uint32_t uiSwapUint32(uint32_t uiValue)
{
    uiValue = (uiValue >> 24) | ((uiValue >> 8) & 0x0000FF00U) | ((uiValue << 8) & 0x00FF0000U) | (uiValue << 24);

    return uiValue;
}

/* 16bit大小端转换 */
uint16_t usSwapUint16(uint16_t usValue)
{
    return ((usValue >> 8) | (usValue << 8));
}

/* 32bit 数组大小端转换 */
void vSwapUint32s(uint32_t *puiDatas, int32_t iLength)
{
    while((iLength--) > 0)
    {
        *puiDatas = uiSwapUint32(*puiDatas);
        ++puiDatas;
    }
}

/* 选择排序 */
void vSortChoice(uint16_t *pusBuff, int32_t iLength)
{
    int32_t i, j;
    uint16_t usTemp;

    for(i = 0; i < iLength - 1; ++i)
    {
        for(j = i + 1; j < iLength; ++j)
        {
            if(pusBuff[i] > pusBuff[j])
            {
                usTemp = pusBuff[i];
                pusBuff[i] = pusBuff[j];
                pusBuff[j] = usTemp;
            }
        }
    }
}
