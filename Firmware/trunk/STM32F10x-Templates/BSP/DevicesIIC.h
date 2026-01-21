#ifndef _DevicesIIC_H_
#define _DevicesIIC_H_

#define AT24C02_WRITE_ADDRESS          0xA0
#define AT24C02_READ_ADDRESS           0xA1

void vIICInit(void);
int32_t iI2CWriteData(uint8_t ucDevicesAddr, uint8_t ucRegisterAddr, uint8_t data);
int32_t iI2CReadData(uint8_t ucDevicesAddr, uint8_t ucRegisterAddr, uint8_t *pData);
int32_t iI2CWriteDatas(uint8_t ucDevicesAddr, uint8_t ucRegisterAddr, uint8_t *pData, uint16_t size);
int32_t iI2CReadDatas(uint8_t ucDevicesAddr, uint8_t ucRegisterAddr, uint8_t *pData, uint16_t size);

#endif
