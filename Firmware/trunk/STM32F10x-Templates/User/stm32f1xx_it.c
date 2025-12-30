/**
  ******************************************************************************
  * @file    Templates/Src/stm32f1xx.c
  * @author  MCD Application Team
  * @brief   Main Interrupt Service Routines.
  *          This file provides template for all exceptions handler and
  *          peripherals interrupt service routine.
  ******************************************************************************
  * @attention
  *
  * h2center&copy; Copyright (c) 2016 STMicroelectronics.
  * All rights reserved./center/h2
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_it.h"
#include "stm32f1xx_hal.h"

#include "DevicesUart.h"
#include "DevicesQueue.h"
#include "DevicesTimer.h"
#include "DevicesADC.h"
#include "DevicesCAN.h"

extern volatile int64_t g_iTimeBase;

/** @addtogroup STM32F1xx_HAL_Examples
  * @{
  */

/** @addtogroup Templates
  * @{
  */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/******************************************************************************/
/*            Cortex-M3 Processor Exceptions Handlers                         */
/******************************************************************************/

/**
  * @brief   This function handles NMI exception.
  * @param  None
  * @retval None
  */
void NMI_Handler(void)
{
}

/**
  * @brief  This function handles Hard Fault exception.
  * @param  None
  * @retval None
  */
void HardFault_Handler(void)
{
  /* Go to infinite loop when Hard Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Memory Manage exception.
  * @param  None
  * @retval None
  */
void MemManage_Handler(void)
{
  /* Go to infinite loop when Memory Manage exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Bus Fault exception.
  * @param  None
  * @retval None
  */
void BusFault_Handler(void)
{
  /* Go to infinite loop when Bus Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Usage Fault exception.
  * @param  None
  * @retval None
  */
void UsageFault_Handler(void)
{
  /* Go to infinite loop when Usage Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Debug Monitor exception.
  * @param  None
  * @retval None
  */
void DebugMon_Handler(void)
{
}

/******************************************************************************/
/*                 STM32F1xx Peripherals Interrupt Handlers                   */
/*  Add here the Interrupt Handler for the used peripheral(s) (PPP), for the  */
/*  available peripheral interrupt handler's name please refer to the startup */
/*  file (startup_stm32f1xx.s).                                               */
/******************************************************************************/

/**
  * @brief TIM6 update interrupt handler
  */
void TIM6_IRQHandler(void)
{
    if (__HAL_TIM_GET_FLAG(&g_timer6_initpara, TIM_FLAG_UPDATE))
    {
        __HAL_TIM_CLEAR_FLAG(&g_timer6_initpara, TIM_FLAG_UPDATE);

        g_iTimeBase += 63356;
    }
}

/**
  * @brief TIM7 update interrupt handler
  */
void TIM7_IRQHandler(void)
{
    if (__HAL_TIM_GET_FLAG(&g_timer7_initpara, TIM_FLAG_UPDATE))
    {
        __HAL_TIM_CLEAR_FLAG(&g_timer7_initpara, TIM_FLAG_UPDATE);

        vADCxScanHigh();
    }
}

/**
  * @brief USART1 DMA receive data callback
  */
void vUSART1ReceiveCallback(void)
{
    static uint32_t uiMDANdtrOld = 0;
    uint32_t uiMDANdtrNow = 0;

    while (uiMDANdtrOld != (uiMDANdtrNow = USART1_DMA_READ_LENGTH - __HAL_DMA_GET_COUNTER(&g_dma_usart1_rx)))
    {
        if (uiMDANdtrNow < uiMDANdtrOld)
        {
            /* 将数据读取到 UART 队列 */
            enumQueuePushDatas(&g_TypeQueueUart1Read,&g_USART1ReadDMABuff[uiMDANdtrOld],USART1_DMA_READ_LENGTH - uiMDANdtrOld);

            uiMDANdtrOld = 0;
        }

        /* 将数据读取到 UART 队列 */
        enumQueuePushDatas(&g_TypeQueueUart1Read,&g_USART1ReadDMABuff[uiMDANdtrOld],uiMDANdtrNow - uiMDANdtrOld);

        uiMDANdtrOld = uiMDANdtrNow;
    }
}

/**
  * @brief USART1 global interrupt handler
  */
void USART1_IRQHandler(void)
{
    // 检查是否有空闲中断标志
    if (__HAL_UART_GET_FLAG(&g_uart1_handle, UART_FLAG_IDLE))
    {
        // 调用接收回调函数
        vUSART1ReceiveCallback();

        // 清除空闲中断标志
        __HAL_UART_CLEAR_IDLEFLAG(&g_uart1_handle);
    }
    // 检查是否有过载错误中断标志
    else if (__HAL_UART_GET_FLAG(&g_uart1_handle, UART_FLAG_ORE))
    {
        __HAL_UART_CLEAR_FLAG(&g_uart1_handle, UART_FLAG_ORE);
        __HAL_UART_CLEAR_OREFLAG(&g_uart1_handle);

        // 重新初始化 UART
        vUart1Init();
    }
    // 检查是否有噪声错误、帧错误、奇偶校验错误中断标志
    else if ((__HAL_UART_GET_FLAG(&g_uart1_handle, UART_FLAG_NE)) ||
             (__HAL_UART_GET_FLAG(&g_uart1_handle, UART_FLAG_FE)) ||
             (__HAL_UART_GET_FLAG(&g_uart1_handle, UART_FLAG_PE)))
    {
        // 清除错误标志
        __HAL_UART_CLEAR_FLAG(&g_uart1_handle, UART_FLAG_NE);
        __HAL_UART_CLEAR_FLAG(&g_uart1_handle, UART_FLAG_FE);
        __HAL_UART_CLEAR_FLAG(&g_uart1_handle, UART_FLAG_PE);
    }
}

/**
  * @brief DMA1 Channel4 global interrupt handler (USART1 TX)
  */
void DMA1_Channel4_IRQHandler(void)
{
    if (__HAL_DMA_GET_FLAG(&g_dma_usart1_tx, DMA_FLAG_TC4))
    {
        __HAL_DMA_CLEAR_FLAG(&g_dma_usart1_tx, DMA_FLAG_GL4);
        __HAL_DMA_CLEAR_FLAG(&g_dma_usart1_tx, DMA_FLAG_TC4);
        __HAL_DMA_CLEAR_FLAG(&g_dma_usart1_tx, DMA_FLAG_HT4);
        __HAL_DMA_CLEAR_FLAG(&g_dma_usart1_tx, DMA_FLAG_TE4);

        (&g_uart1_handle)->gState = HAL_UART_STATE_READY;
        (&g_dma_usart1_tx)->State = HAL_DMA_STATE_READY;
        __HAL_UNLOCK(&g_dma_usart1_tx);
    }
}

/**
  * @brief DMA1 Channel5 global interrupt handler (USART1 RX)
  */
void DMA1_Channel5_IRQHandler(void)
{
    if (__HAL_DMA_GET_FLAG(&g_dma_usart1_rx, DMA_FLAG_HT5) != RESET)
    {
        vUSART1ReceiveCallback();
        __HAL_DMA_CLEAR_FLAG(&g_dma_usart1_rx, DMA_FLAG_HT5);
    }
    else if (__HAL_DMA_GET_FLAG(&g_dma_usart1_rx, DMA_FLAG_TC5) != RESET)
    {
        vUSART1ReceiveCallback();
        __HAL_DMA_CLEAR_FLAG(&g_dma_usart1_rx, DMA_FLAG_TC5);
    }
}

/**
  * @brief DMA1 Channel1 global interrupt handler
  */
void DMA1_Channel1_IRQHandler(void)
{
    if (DMA1->ISR & (1 << 1))
    {
        do
        {
            DMA1->IFCR |= 1 << 1;
        } while (0);
    }
}

/**
  * @brief CAN1 RX0 interrupt handler
  */
void CAN1_RX0_IRQHandler(void)
{
    CAN_RxHeaderTypeDef can1_rxheader;
    uint8_t rxData[8];

    while (HAL_CAN_GetRxFifoFillLevel(&g_can1_handler, CAN_RX_FIFO0) > 0)
    {
        if (HAL_CAN_GetRxMessage(&g_can1_handler, CAN_RX_FIFO0, &can1_rxheader, rxData) == HAL_OK)
        {
            if (can1_rxheader.IDE == CAN_ID_STD)
            {
                enumQueuePushDatas(&g_TypeQueueCanHostRead, rxData, can1_rxheader.DLC);
            }
        }
    }
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
