#ifndef _DevicesSPI_H_
#define _DevicesSPI_H_

#include "stm32f1xx_hal.h"

#define SPI2_CS_PORT        GPIOB
#define SPI2_CS_PIN         GPIO_PIN_12

#define SPI2_SCK_PORT       GPIOB
#define SPI2_SCK_PIN        GPIO_PIN_13

#define SPI2_MISO_PORT      GPIOB
#define SPI2_MISO_PIN       GPIO_PIN_14

#define SPI2_MOSI_PORT      GPIOB
#define SPI2_MOSI_PIN       GPIO_PIN_15

#define SET_SPI2_NSS_HIGH() HAL_GPIO_WritePin(SPI2_CS_PORT, SPI2_CS_PIN, GPIO_PIN_SET)
#define SET_SPI2_NSS_LOW()  HAL_GPIO_WritePin(SPI2_CS_PORT, SPI2_CS_PIN, GPIO_PIN_RESET)

extern SPI_HandleTypeDef spi2_init_struct;

void vSPI2Init(void);
uint8_t ucSPIWriteReadByte(SPI_HandleTypeDef *hspi, uint8_t ucByte);
int8_t cSPIWriteDatas(SPI_HandleTypeDef *hspi, void *pvBuff, int32_t iLength);
int8_t cSPIReadDatas(SPI_HandleTypeDef *hspi, void *pvBuff, int32_t iLength);

// 数据处理接口
#define ucSPI2WriteReadByte(ucByte) ucSPIWriteReadByte(&spi2_init_struct, (ucByte))
#define cSPI2WriteDatas(pBuffer, iLength) cSPIWriteDatas(&spi2_init_struct, (pBuffer), (iLength))
#define cSPI2ReadDatas(pBuffer, iLength)  cSPIReadDatas(&spi2_init_struct, (pBuffer), (iLength))

#endif
