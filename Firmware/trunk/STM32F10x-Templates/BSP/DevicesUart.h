#ifndef _DevicesUart_H_
#define _DevicesUart_H_

#define UART_LOG  USART1
#define UART_BUS  USART2

#define USART1_DMA_READ_LENGTH  (128) 
#define USART1_DMA_SEND_LENGTH  (0) 
#define USART2_DMA_READ_LENGTH  (128) 
#define USART2_DMA_SEND_LENGTH  (0)

extern uint8_t g_USART1ReadDMABuff[USART1_DMA_READ_LENGTH]; 
extern uint8_t g_USART2ReadDMABuff[USART2_DMA_READ_LENGTH];

extern UART_HandleTypeDef g_uart1_handle;
extern DMA_HandleTypeDef  g_dma_usart1_rx;
extern DMA_HandleTypeDef  g_dma_usart1_tx;

extern UART_HandleTypeDef g_uart2_handle;
extern DMA_HandleTypeDef  g_dma_usart2_rx;
extern DMA_HandleTypeDef  g_dma_usart2_tx;

void vUart1Init(void); 
void vUart1DMAInit(void); 

void vUart2Init(void); 
void vUart2DMAInit(void);

void vUartBaudrateSet(uint32_t uiUsartPeriph, int32_t iBaudrate); 
 
void vUartSendDatas(uint32_t uiUsartPeriph, void *pvDatas, int32_t iLength); 
void vUartSendStrings(uint32_t uiUsartPeriph, char *pcStrings); 
 
void vUartDMASendDatas(uint32_t uiUsartPeriph, void *pvDatas, int32_t iLength); 
void vUartDMASendStrings(uint32_t uiUsartPeriph, char *pcStrings); 

int8_t cUartReceiveByte(uint32_t uiUsartPeriph, uint8_t *pucByte);
int8_t cUartReceiveDatas(uint32_t uiUsartPeriph, void *pvDatas, int32_t iLength);
int32_t iUartReceiveAllDatas(uint32_t uiUsartPeriph, void *pvDatas, int32_t iLengthLimit);
int32_t iUartReceiveLengthGet(uint32_t uiUsartPeriph);
int8_t cUartReceiveClear(uint32_t uiUsartPeriph);

#endif
