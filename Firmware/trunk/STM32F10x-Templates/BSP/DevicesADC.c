#include "stm32f1xx_hal.h"
#include "DevicesADC.h"

/* 给上层应用获取 */
float g_fADCxListValues[32];

/* DMA缓存 */
static uint16_t st_usADC1DmaDatas[ADC1_SAMPLING_CHANNEL];

/* ADC1 句柄 */
ADC_HandleTypeDef g_typeADC1Handle;

void vADC1DMAInit(uint32_t uiAdcxMemAddr, uint16_t usAdcxDmaLength);
void vADC1DMA1Enable(uint16_t cndtr);

void vADCInit(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    RCC_PeriphCLKInitTypeDef adc_clk_init = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_ADC1_CLK_ENABLE();

    /* ADC外设时钟 */
    adc_clk_init.PeriphClockSelection = RCC_PERIPHCLK_ADC;
    /* 分频因子6 时钟为72M/6=12MHz */
    adc_clk_init.AdcClockSelection = RCC_ADCPCLK2_DIV6;
    /* 设置ADC时钟 */
    HAL_RCCEx_PeriphCLKConfig(&adc_clk_init);

    GPIO_InitStruct.Pin = GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* ADC1 初始化 */
    g_typeADC1Handle.Instance = ADC1;                            /* ADC1 */
    g_typeADC1Handle.Init.DataAlign = ADC_DATAALIGN_RIGHT;       /* 数据对齐方式: 右对齐 */
    g_typeADC1Handle.Init.ScanConvMode = ADC_SCAN_DISABLE;       /* 非扫描模式,仅用到一个通道 */
    g_typeADC1Handle.Init.ContinuousConvMode = ENABLE;           /* 开启连续转换模式 */
    g_typeADC1Handle.Init.NbrOfConversion = 1;                   /* 赋值范围是1~16，本实验用到1个规则通道序列 */
    g_typeADC1Handle.Init.DiscontinuousConvMode = DISABLE;       /* 禁止规则通道组间断模式 */
    g_typeADC1Handle.Init.NbrOfDiscConversion = 0;               /* 配置简短模式的规则通道个数,禁止规则通道组间断模式后，此参数忽略 */
    g_typeADC1Handle.Init.ExternalTrigConv = ADC_SOFTWARE_START; /* 出发转换方式：软件触发 */
    HAL_ADC_Init(&g_typeADC1Handle);                             /* 初始化 */
    HAL_ADCEx_Calibration_Start(&g_typeADC1Handle);              /* 校准ADC */

    vADC1DMAInit((uint32_t)&st_usADC1DmaDatas,ADC1_SAMPLING_CHANNEL);

    HAL_ADC_Start(&g_typeADC1Handle);
}

void vADC1DMAInit(uint32_t uiAdcxMemAddr, uint16_t usAdcxDmaLength)
{
    DMA_HandleTypeDef typeDMAADC_handle = {0};

    ADC_ChannelConfTypeDef adc_Channel_Config = {0};

    __HAL_RCC_DMA1_CLK_ENABLE();

    typeDMAADC_handle.Instance = DMA1_Channel1;
    typeDMAADC_handle.Init.Direction = DMA_PERIPH_TO_MEMORY;                    /* 从外设到存储器模式 */
    typeDMAADC_handle.Init.PeriphInc = DMA_PINC_DISABLE;                        /* 外设非增量模式 */
    typeDMAADC_handle.Init.MemInc = DMA_MINC_ENABLE;                            /* 存储器增量模式 */
    typeDMAADC_handle.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;       /* 外设数据长度:16位 */
    typeDMAADC_handle.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;          /* 存储器数据长度:16位 */
    typeDMAADC_handle.Init.Mode = DMA_CIRCULAR;                                 /* 外设流控模式 */
    typeDMAADC_handle.Init.Priority = DMA_PRIORITY_MEDIUM;
    HAL_DMA_Init(&typeDMAADC_handle);

    __HAL_LINKDMA(&g_typeADC1Handle, DMA_Handle, typeDMAADC_handle);            /* 将DMA与adc联系起来 */

    /* 通道初始化 */
    adc_Channel_Config.Rank = ADC_REGULAR_RANK_1;
    adc_Channel_Config.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
    adc_Channel_Config.Channel = ADC_CHANNEL_1;
    HAL_ADC_ConfigChannel(&g_typeADC1Handle, &adc_Channel_Config);

    /* 配置DMA数据流请求中断优先级 */
    HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 3, 3);
    HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
 
    HAL_DMA_Start_IT(&typeDMAADC_handle, (uint32_t)&ADC1->DR, uiAdcxMemAddr, 0);        /* 启动DMA，并开启中断 */
    HAL_ADC_Start_DMA(&g_typeADC1Handle, &uiAdcxMemAddr, 0);                            /* 开启ADC，通过DMA传输结果 */

    vADC1DMA1Enable(usAdcxDmaLength);
}

void vADC1DMA1Enable(uint16_t cndtr)
{
    ADC1->CR2 &= ~(1 << 0);                    /* 先关闭ADC */

    DMA1_Channel1->CCR &= ~(1 << 0);
    while (DMA1_Channel1->CCR & (1 << 0));     /* 确保DMA可以被设置 */
    DMA1_Channel1->CNDTR = cndtr;              /* DMA传输数据量 */
    DMA1_Channel1->CCR |= 1 << 0;              /* 开启DMA传输 */

    ADC1->CR2 |= 1 << 0;                       /* 重新启动ADC */
    ADC1->CR2 |= 1 << 22;                      /* 启动规则转换通道 */
}

float fADCxChannelValueGet(ADC_HandleTypeDef *adc_periph,uint32_t rank, uint32_t channel, uint32_t uiCnt)
{
    ADC_ChannelConfTypeDef adc_Channel_Config = {0};
    uint32_t uiValueSum = 0;
    uint16_t usValueMax = 0, usValueMin = 0xFFFF, usValueNow = 0;

    if(adc_periph->Instance == NULL)
        return 0.0f;

    /* 通道初始化 */
    adc_Channel_Config.Rank = rank;
    adc_Channel_Config.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
    adc_Channel_Config.Channel = channel;
    HAL_ADC_ConfigChannel(&g_typeADC1Handle, &adc_Channel_Config);

    /* 切换通道后,丢弃第一次转换的数据 */
    HAL_ADC_Start(adc_periph);
    HAL_ADC_PollForConversion(adc_periph, 10);
    HAL_ADC_GetValue(adc_periph);

    for(uint32_t i = 0; i < uiCnt; ++i)
    {
        HAL_ADC_Start(adc_periph);
        HAL_ADC_PollForConversion(adc_periph, 10);
        usValueNow = HAL_ADC_GetValue(adc_periph);

        uiValueSum += usValueNow;
        if(usValueNow > usValueMax) usValueMax = usValueNow;
        if(usValueNow < usValueMin) usValueMin = usValueNow;
    }

    uiValueSum -= (usValueMax + usValueMin);
    return ((float)uiValueSum / (uiCnt - 2) * (3.3f / 4095.0f));
}

float fADCxDmaValueGet(uint16_t *pDatasHand, uint16_t enumChannel, uint16_t usChannelMax)
{
    if(enumChannel >= usChannelMax)
        return 0.0f;

    pDatasHand += enumChannel;
    return ((float)(*pDatasHand)) * (3.3f / 4095.0f);
}

void vADCxScanLow(void)
{
    g_fADCxListValues[ADC_LIST_DC_IN_VOLTAGE] = fADCxChannelValueGet(&g_typeADC1Handle,1,ADC_SAMPLE_DC_VOLTAGE,ADC1_SAMPLING_NUMBER);
}

void vADCxScanHigh(void)
{
    g_fADCxListValues[ADC_LIST_DC_IN_VOLTAGE] = fADCxDmaValueGet(st_usADC1DmaDatas,ADC_LIST_DC_IN_VOLTAGE,ADC1_SAMPLING_DMA_NUMBER);
}

/*! 
    \brief      ADC soft scan channel
    \param[in]  none
    \param[out] none
    \retval     none
*/
float fADCListValueGet(AdcListEnum enumChannel)
{
    return g_fADCxListValues[enumChannel];
}
