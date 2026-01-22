#ifndef _DevicesADC_H_
#define _DevicesADC_H_

/* 快速 - ADC1 */
#define ADC_SAMPLE_DC_VOLTAGE       ADC_CHANNEL_1

/* 慢速 - ADC2 */
#define ADC_SAMPLE_DC_MINI_VOLTAGE  ADC_CHANNEL_5

typedef enum {
    /* 输入电压 */
    ADC_DMA_SCAN_DC_IN_VOLTAGE,

    /* 最大通道值 */
    ADC1_DMA_SCAN_MAX,
} Adc1DmaChannelEnum;

typedef enum {
    /* 输入电压 */
    ADC_LIST_DC_IN_VOLTAGE,
    
    /* 输入电压-MINI */
    ADC_LIST_DC_MINI_IN_VOLTAGE,

    /* 最大的通道值 */
    ADC_LIST_MAX,
} AdcListEnum;

/* 每通道采样次数 */
#define ADC1_SAMPLING_NUMBER        1
#define ADC2_SAMPLING_NUMBER        4

/* 采样扫描通道数量 */
#define ADC1_SAMPLING_CHANNEL       ADC1_DMA_SCAN_MAX
#define ADC2_SAMPLING_CHANNEL       1



void vADCInit(void);
void vADCxScanLow(void);
void vADCxScanHigh(void);
float fADCListValueGet(AdcListEnum enumChannel);

#endif
