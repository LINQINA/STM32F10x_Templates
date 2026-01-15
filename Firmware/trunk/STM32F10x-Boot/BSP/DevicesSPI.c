#include "stm32f1xx_hal.h"
#include "stdio.h"
#include "stdint.h"
#include "string.h"

#include "DevicesSPI.h"

SPI_HandleTypeDef spi2_init_struct;

void vSPI2Init(void)
{
   GPIO_InitTypeDef GPIO_InitStruct = {0};


    /* 打开时钟 */
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_SPI2_CLK_ENABLE();

    /** 配置片选 CS 引脚（PB12）为推挽输出 */
    GPIO_InitStruct.Pin = SPI2_CS_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(SPI2_CS_PORT, &GPIO_InitStruct);
    SET_SPI2_NSS_HIGH(); // 默认拉高

    /** 配置 SPI2 的 SCK（PB13）和 MOSI（PB15）为复用推挽输出 */
    GPIO_InitStruct.Pin = SPI2_SCK_PIN | SPI2_MOSI_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(SPI2_SCK_PORT, &GPIO_InitStruct);

    /** 配置 MISO（PB14）为输入模式 */
    GPIO_InitStruct.Pin = SPI2_MISO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(SPI2_MISO_PORT, &GPIO_InitStruct);

    /** SPI2 配置 */
    spi2_init_struct.Instance = SPI2;
    spi2_init_struct.Init.Mode = SPI_MODE_MASTER;                            /* 主机  */
    spi2_init_struct.Init.Direction = SPI_DIRECTION_2LINES;                  /* 全双工模式 */
    spi2_init_struct.Init.DataSize = SPI_DATASIZE_8BIT;                      /* 字节形式发送 */
    spi2_init_struct.Init.CLKPolarity = SPI_POLARITY_HIGH;                   /* 空闲电平为高 */
    spi2_init_struct.Init.CLKPhase = SPI_PHASE_2EDGE;                        /* 第二个电平为有效电平 */
    spi2_init_struct.Init.NSS = SPI_NSS_SOFT;                                /* 设置为软件触发 */
    spi2_init_struct.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;      /* 设置 128 分频 */
    spi2_init_struct.Init.FirstBit = SPI_FIRSTBIT_MSB;                       /* 高字节传送 */
    spi2_init_struct.Init.TIMode = SPI_TIMODE_DISABLE;                       /* 禁用TI模式 */
    spi2_init_struct.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;       /* 禁用CRC计算 */
    spi2_init_struct.Init.CRCPolynomial = 10;

    HAL_SPI_Init(&spi2_init_struct);
}

uint8_t ucSPIWriteReadByte(SPI_HandleTypeDef *hspi, uint8_t ucByte)
{
    uint8_t ucReadByte = 0;
    
    HAL_SPI_TransmitReceive(hspi, &ucByte, &ucReadByte, 1, HAL_MAX_DELAY);
    
    return ucReadByte;
}

int8_t cSPIWriteDatas(SPI_HandleTypeDef *hspi, void *pvBuff, int32_t iLength)
{
    if ((pvBuff == NULL) || (iLength < 1))
        return 1;

    if (HAL_SPI_Transmit(hspi, (uint8_t *)pvBuff, iLength, HAL_MAX_DELAY) != HAL_OK)
        return 2;

    return 0;
}

int8_t cSPIReadDatas(SPI_HandleTypeDef *hspi, void *pvBuff, int32_t iLength)
{
    if ((pvBuff == NULL) || (iLength < 1))
        return 1;

    if (HAL_SPI_TransmitReceive(hspi, (uint8_t *)pvBuff, (uint8_t *)pvBuff, iLength, HAL_MAX_DELAY) != HAL_OK)
        return 2;

    return 0;
}