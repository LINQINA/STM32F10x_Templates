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
  * <h2><center>&copy; Copyright (c) 2016 STMicroelectronics.
  * All rights reserved.</center></h2>
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
#include "DevicesTime.h"
#include "DevicesADC.h"
#include "DevicesCAN.h"
#include "DevicesRTC.h"




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

void TIM6_IRQHandler(void)
{
    if(__HAL_TIM_GET_FLAG(&g_timer6_initpara,TIM_FLAG_UPDATE))
    {
        __HAL_TIM_CLEAR_FLAG(&g_timer6_initpara,TIM_FLAG_UPDATE);
        
        g_iTimeBase += 65536;
    }
}

void TIM7_IRQHandler(void)
{
    if(__HAL_TIM_GET_FLAG(&g_timer7_initpara,TIM_FLAG_UPDATE))
    {
        __HAL_TIM_CLEAR_FLAG(&g_timer7_initpara,TIM_FLAG_UPDATE);

        vADCxScanHigh();
    }
}

void vUSART1ReceiveCallback(void)
{
    static uint32_t uiMDANdtrOld = 0;
    uint32_t uiMDANdtrNow = 0;

    while(uiMDANdtrOld != (uiMDANdtrNow = USART1_DMA_READ_LENGTH - __HAL_DMA_GET_COUNTER(&g_dma_usart1_rx)))
    {
        if(uiMDANdtrNow < uiMDANdtrOld)
        {
            /* 把数据读取到UART队列 */
            enumQueuePushDatas(&g_TypeQueueUart0Read, &g_USART1ReadDMABuff[uiMDANdtrOld], USART1_DMA_READ_LENGTH - uiMDANdtrOld);

            uiMDANdtrOld = 0;
        }

        /* 把数据读取到UART队列 */
        enumQueuePushDatas(&g_TypeQueueUart0Read, &g_USART1ReadDMABuff[uiMDANdtrOld], uiMDANdtrNow - uiMDANdtrOld);

        uiMDANdtrOld = uiMDANdtrNow;
    }
}

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
    // 检查是否有超限错误中断标志
    else if (__HAL_UART_GET_FLAG(&g_uart1_handle, UART_FLAG_ORE)) 
    { 
        __HAL_UART_CLEAR_FLAG(&g_uart1_handle, UART_FLAG_ORE);
        
        __HAL_UART_CLEAR_OREFLAG(&g_uart1_handle);

        // 重新初始化UART
        vUart1Init(); 
    } 
    // 检查是否有噪声错误、帧错误或奇偶校验错误中断标志
    else if ((__HAL_UART_GET_FLAG(&g_uart1_handle, UART_FLAG_NE)) || 
             (__HAL_UART_GET_FLAG(&g_uart1_handle, UART_FLAG_FE)) || 
             (__HAL_UART_GET_FLAG(&g_uart1_handle, UART_FLAG_PE))) 
    { 
        // 清除各种错误标志
        __HAL_UART_CLEAR_FLAG(&g_uart1_handle, UART_FLAG_NE); 
        __HAL_UART_CLEAR_FLAG(&g_uart1_handle, UART_FLAG_FE); 
        __HAL_UART_CLEAR_FLAG(&g_uart1_handle, UART_FLAG_PE); 
    } 
}

void DMA1_Channel4_IRQHandler(void)
{
    if(__HAL_DMA_GET_FLAG(&g_dma_usart1_tx, DMA_FLAG_TC4))
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

void DMA1_Channel5_IRQHandler(void)
{
    if(__HAL_DMA_GET_FLAG(&g_dma_usart1_rx,DMA_FLAG_HT5) != RESET)
    {
        vUSART1ReceiveCallback();
        
        __HAL_DMA_CLEAR_FLAG(&g_dma_usart1_rx,DMA_FLAG_HT5);
    }
    else if(__HAL_DMA_GET_FLAG(&g_dma_usart1_rx,DMA_FLAG_TC5) != RESET)
    {
        vUSART1ReceiveCallback();
        
        __HAL_DMA_CLEAR_FLAG(&g_dma_usart1_rx,DMA_FLAG_TC5);
    }
}

void CAN1_RX0_IRQHandler(void)
{
    CAN_RxHeaderTypeDef can1_rxheader;    /* CAN接收结构体 */
    uint8_t rxData[8];
    
    // 检查 FIFO0 消息挂起中断标志是否被置位
    while (HAL_CAN_GetRxFifoFillLevel(&g_can1_handler, CAN_RX_FIFO0) > 0)
    {
        // 接收 CAN 消息
        if (HAL_CAN_GetRxMessage(&g_can1_handler, CAN_RX_FIFO0, &can1_rxheader, rxData) == HAL_OK)
        {
            // 标准帧 (IDE = 0)
            if (can1_rxheader.IDE == CAN_ID_STD)
            {
                enumQueuePushDatas(&g_TypeQueueCanHostRead, rxData, can1_rxheader.DLC);
            }
        }
    }
}

void RTC_IRQHandler(void)
{
    /* 闹钟中断 */
    if(__HAL_RTC_ALARM_GET_FLAG(&hrtc, RTC_FLAG_ALRAF) != RESET)
    {
        __HAL_RTC_ALARM_CLEAR_FLAG(&hrtc, RTC_FLAG_ALRAF);
        
        /* TODO: 在这里写闹钟触发后要做的事情 */
    }
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
