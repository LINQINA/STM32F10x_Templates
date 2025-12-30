#ifndef _DevicesCRC_H_
#define _DevicesCRC_H_

void vHardCRCInit(void);
void vHardCRCReset(void);
uint32_t uiHardCRCAccumulate(uint32_t *puiDataBuff, int32_t iLength);

uint8_t ucCRC7_MMC(uint8_t *pucInitCRC, void *pvBuff, int32_t iLength);
uint8_t ucCRC8(uint8_t *pucInitCRC, void *pvBuff, int32_t iLength);
uint8_t ucCRC8_ITU(uint8_t *pucInitCRC, void *pvBuff, int32_t iLength);
uint8_t ucCRC8_ROHC(uint8_t *pucInitCRC, void *pvBuff, int32_t iLength);
uint8_t ucCRC8_MAXIM(uint8_t *pucInitCRC, void *pvBuff, int32_t iLength);

uint16_t usCRC16_IBM(uint16_t *pusInitCRC, void *pvBuff, int32_t iLength);
uint16_t usCRC16_MAXIM(uint16_t *pusInitCRC, void *pvBuff, int32_t iLength);
uint16_t usCRC16_USB(uint16_t *pusInitCRC, void *pvBuff, int32_t iLength);
uint16_t usCRC16_MODBUS(uint16_t *pusInitCRC, void *pvBuff, int32_t iLength);
uint16_t usCRC16_CCITT(uint16_t *pusInitCRC, void *pvBuff, int32_t iLength);
uint16_t usCRC16_CCITT_FALSE(uint16_t *pusInitCRC, void *pvBuff, int32_t iLength);
uint16_t usCRC16_X25(uint16_t *pusInitCRC, void *pvBuff, int32_t iLength);
uint16_t usCRC16_XMODEM(uint16_t *pusInitCRC, void *pvBuff, int32_t iLength);
uint16_t usCRC16_DNP(uint16_t *pusInitCRC, void *pvBuff, int32_t iLength);

uint32_t uiCRC32(uint32_t *puiInitCRC, void *pvBuff, int32_t iLength);
uint32_t uiCRC32_BZIP2(uint32_t *puiInitCRC, void *pvBuff, int32_t iLength);
uint32_t uiCRC32_MPEG2(uint32_t *puiInitCRC, void *pvBuff, int32_t iLength);
uint32_t uiCRC32_POSIX(uint32_t *puiInitCRC, void *pvBuff, int32_t iLength);
uint32_t uiCRC32_JAMCRC(uint32_t *puiInitCRC, void *pvBuff, int32_t iLength);
uint32_t uiCRC32_STM32(uint32_t *puiInitCRC, void *pvBuff, int32_t iLength);

#endif
