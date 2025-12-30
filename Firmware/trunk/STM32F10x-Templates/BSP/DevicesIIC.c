#include "stm32f1xx_hal.h"
#include "stdio.h"
#include "stdint.h"
#include "string.h"

#include "DevicesIIC.h"

static I2C_HandleTypeDef g_i2c_handle;

void vIICInit(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_I2C1_CLK_ENABLE();

    GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    g_i2c_handle.Instance = I2C1;
    g_i2c_handle.Init.ClockSpeed = 100000;
    g_i2c_handle.Init.DutyCycle = I2C_DUTYCYCLE_2;
    g_i2c_handle.Init.OwnAddress1 = 0;
    g_i2c_handle.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    g_i2c_handle.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    g_i2c_handle.Init.OwnAddress2 = 0;
    g_i2c_handle.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    g_i2c_handle.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

    HAL_I2C_Init(&g_i2c_handle);
}

int32_t iI2CWriteData(uint8_t ucDevicesAddr, uint8_t ucRegisterAddr, uint8_t data)
{
    if(g_i2c_handle.Instance == NULL)
        return 1;

    if(HAL_I2C_Mem_Write(&g_i2c_handle, ucDevicesAddr, ucRegisterAddr, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY) != HAL_OK)
        return 2;

    return 0;
}

int32_t iI2CReadData(uint8_t ucDevicesAddr, uint8_t ucRegisterAddr, uint8_t *pData)
{
    if(g_i2c_handle.Instance == NULL)
        return 1;

    if(HAL_I2C_Mem_Read(&g_i2c_handle, ucDevicesAddr, ucRegisterAddr, I2C_MEMADD_SIZE_8BIT, pData, 1, HAL_MAX_DELAY) != HAL_OK)
        return 2;

    return 0;
}

int32_t iI2CWriteDatas(uint8_t ucDevicesAddr, uint8_t ucRegisterAddr, uint8_t *pData, uint16_t size)
{
    if(g_i2c_handle.Instance == NULL)
        return 1;

    if(HAL_I2C_Mem_Write(&g_i2c_handle, ucDevicesAddr, ucRegisterAddr, I2C_MEMADD_SIZE_8BIT, pData, size, HAL_MAX_DELAY) != HAL_OK)
        return 2;

    return 0;
}

int32_t iI2CReadDatas(uint8_t ucDevicesAddr, uint8_t ucRegisterAddr, uint8_t *pData, uint16_t size)
{
    if(g_i2c_handle.Instance == NULL)
        return 1;

    if(HAL_I2C_Mem_Read(&g_i2c_handle, ucDevicesAddr, ucRegisterAddr, I2C_MEMADD_SIZE_8BIT, pData, size, HAL_MAX_DELAY) != HAL_OK)
        return 2;

    return 0;
}
