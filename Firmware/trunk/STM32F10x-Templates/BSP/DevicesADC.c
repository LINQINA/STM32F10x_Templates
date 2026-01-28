#include "stm32f1xx_hal.h"
#include "DevicesADC.h"

/* 给上层应用获取 */
float g_fADCxListValues[32];

/* DMA缓存 */
static uint16_t st_usADC1DmaDatas[ADC1_SAMPLING_CHANNEL];

/* ADC1 句柄 */
ADC_HandleTypeDef g_typeADC1Handle;
/* ADC2 句柄 */
ADC_HandleTypeDef g_typeADC2Handle;

void vADC1DMAInit(uint32_t uiAdcxMemAddr, uint16_t usAdcxDmaLength);
void vADC1DMA1Enable(uint16_t cndtr);

void vADCInit(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    RCC_PeriphCLKInitTypeDef adc_clk_init = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_ADC2_CLK_ENABLE();

   /* ADC外设时钟 */
    adc_clk_init.PeriphClockSelection = RCC_PERIPHCLK_ADC;
   /* 分频因子6 时钟为72M/6=12MHz */
    adc_clk_init.AdcClockSelection = RCC_ADCPCLK2_DIV6;
    /* 设置ADC时钟 */
    HAL_RCCEx_PeriphCLKConfig(&adc_clk_init);

    /* PA1 初始化 (ADC1) */
    GPIO_InitStruct.Pin = GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* PA5 初始化 (ADC2) */
    GPIO_InitStruct.Pin = GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* ADC1 初始化 (DMA快速模式) */
    g_typeADC1Handle.Instance = ADC1;                            /* ADC1 */
    g_typeADC1Handle.Init.DataAlign = ADC_DATAALIGN_RIGHT;       /* 数据对齐方式：右对齐 */
    g_typeADC1Handle.Init.ScanConvMode = ADC_SCAN_ENABLE;        /* 连续扫描模式开启 */
    g_typeADC1Handle.Init.ContinuousConvMode = ENABLE;           /* 开启连续转换模式 */
    g_typeADC1Handle.Init.NbrOfConversion = 1;                   /* 赋值范围是1~16，本实验用到1个规则通道序列 */
    g_typeADC1Handle.Init.DiscontinuousConvMode = DISABLE;       /* 禁止规则通道组间断模式 */
    g_typeADC1Handle.Init.NbrOfDiscConversion = 0;               /* 配置间断模式的规则通道个数，禁止规则通道组间断模式后，此参数忽略 */
    g_typeADC1Handle.Init.ExternalTrigConv = ADC_SOFTWARE_START; /* 触发转换方式：软件触发 */

    HAL_ADC_Init(&g_typeADC1Handle);                             /* 初始化 */
    HAL_ADCEx_Calibration_Start(&g_typeADC1Handle);              /* 校准ADC */

    vADC1DMAInit((uint32_t)&st_usADC1DmaDatas,ADC1_SAMPLING_CHANNEL);

    HAL_ADC_Start(&g_typeADC1Handle);

    /* ADC2 初始化 */
    g_typeADC2Handle.Instance = ADC2;                            /* ADC2 */
    g_typeADC2Handle.Init.DataAlign = ADC_DATAALIGN_RIGHT;       /* 数据对齐方式：右对齐 */
    g_typeADC2Handle.Init.ScanConvMode = ADC_SCAN_DISABLE;       /* 非扫描模式 */
    g_typeADC2Handle.Init.ContinuousConvMode = DISABLE;          /* 关闭连续转换 (轮询是按需采样的) */
    g_typeADC2Handle.Init.NbrOfConversion = 1;                   /* 赋值范围是1~16，本实验用到1个规则通道序列 */
    g_typeADC2Handle.Init.DiscontinuousConvMode = DISABLE;       /* 禁止规则通道组间断模式 */
    g_typeADC2Handle.Init.NbrOfDiscConversion = 0;               /* 配置间断模式的规则通道个数，禁止规则通道组间断模式后，此参数忽略 */
    g_typeADC2Handle.Init.ExternalTrigConv = ADC_SOFTWARE_START; /* 软件触发 */

    HAL_ADC_Init(&g_typeADC2Handle);                             /* 初始化 */
    HAL_ADCEx_Calibration_Start(&g_typeADC2Handle);              /* 校准ADC */
}

void vADC1DMAInit(uint32_t uiAdcxMemAddr, uint16_t usAdcxDmaLength)
{
    static DMA_HandleTypeDef st_typeDMAADC_handle = {0};
    
    ADC_ChannelConfTypeDef adc_Channel_Config = {0};

    __HAL_RCC_DMA1_CLK_ENABLE();

    st_typeDMAADC_handle.Instance = DMA1_Channel1;
    st_typeDMAADC_handle.Init.Direction = DMA_PERIPH_TO_MEMORY;                 /* 从外设到存储器模式 */
    st_typeDMAADC_handle.Init.PeriphInc = DMA_PINC_DISABLE;                     /* 外设非增量模式 */
    st_typeDMAADC_handle.Init.MemInc = DMA_MINC_ENABLE;                         /* 存储器增量模式 */
    st_typeDMAADC_handle.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;    /* 外设数据长度:16位 */
    st_typeDMAADC_handle.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;       /* 存储器数据长度:16位 */
    st_typeDMAADC_handle.Init.Mode = DMA_CIRCULAR;                              /* 外设流控模式 */
    st_typeDMAADC_handle.Init.Priority = DMA_PRIORITY_MEDIUM;                   /* 中等优先级 */
    HAL_DMA_Init(&st_typeDMAADC_handle);

    __HAL_LINKDMA(&g_typeADC1Handle, DMA_Handle, st_typeDMAADC_handle);         /* 将DMA与adc联系起来 */

    /* 通道初始化 */
    adc_Channel_Config.Rank = ADC_REGULAR_RANK_1;
    adc_Channel_Config.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
    adc_Channel_Config.Channel = ADC_CHANNEL_1;
    HAL_ADC_ConfigChannel(&g_typeADC1Handle, &adc_Channel_Config);

    HAL_ADC_Start_DMA(&g_typeADC1Handle, (uint32_t*)st_usADC1DmaDatas, usAdcxDmaLength);/* 开启ADC，通过DMA传输结果 */

    vADC1DMA1Enable(usAdcxDmaLength);
}

void vADC1DMA1Enable(uint16_t cndtr)
{
    /* 1.关闭 DMA 转换 */
    HAL_ADC_Stop_DMA(&g_typeADC1Handle);

    /* 2.重新启动DMA转换 */
    HAL_ADC_Start_DMA(&g_typeADC1Handle, (uint32_t*)st_usADC1DmaDatas, cndtr);
}

float fADCxChannelValueGet(ADC_HandleTypeDef *adc_periph, uint32_t channel, uint32_t uiCnt)
{
    ADC_ChannelConfTypeDef adc_Channel_Config = {0};
    uint32_t uiValueSum = 0;
    uint16_t usValueMax = 0, usValueMin = 0xFFFF, usValueNow = 0;
    
    if(adc_periph->Instance == NULL)
        return 0.0f;

    if(uiCnt < 3)
        return 0.0f;

    /* 通道初始化  */
    adc_Channel_Config.Rank = 1;
    adc_Channel_Config.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
    adc_Channel_Config.Channel = channel;
    HAL_ADC_ConfigChannel(adc_periph, &adc_Channel_Config);

    /* 切换通道后，丢弃第一次转换的数据 */
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
    g_fADCxListValues[ADC_LIST_DC_MINI_IN_VOLTAGE] = fADCxChannelValueGet(&g_typeADC2Handle, ADC_SAMPLE_DC_MINI_VOLTAGE, ADC2_SAMPLING_NUMBER);
}

void vADCxScanHigh(void)
{
    g_fADCxListValues[ADC_LIST_DC_IN_VOLTAGE] = fADCxDmaValueGet(st_usADC1DmaDatas, ADC_DMA_SCAN_DC_IN_VOLTAGE, ADC1_DMA_SCAN_MAX);
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
