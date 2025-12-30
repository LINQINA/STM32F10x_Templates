#ifndef _DevicesAT24C02_H_
#define _DevicesAT24C02_H_

#define PAGE_SIZE       8

int8_t cAT24C02WriteDatas(uint8_t ucAddress,uint8_t *pucDatas,uint16_t ucLength);
int8_t cAT24C02ReadDatas(uint8_t ucAddress,uint8_t *pucDatas,uint16_t ucLength);

#endif
