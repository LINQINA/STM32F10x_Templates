#ifndef _DevicesADC_H_
#define _DevicesADC_H_

/* 慢速 - ADC1 */
#define ADC_SAMPLE_DC_VOLTAGE       ADC_CHANNEL_1

typedef enum {
    /* 输入电压 */
    ADC_LIST_DC_IN_VOLTAGE,

    /* 最大的通道值 */
    ADC_LIST_MAX,
} AdcListEnum;

/* 每通道采样次数 */
#define ADC1_SAMPLING_NUMBER        4
#define ADC1_SAMPLING_DMA_NUMBER    1

/* 采样扫描通道数量 */
#define ADC1_SAMPLING_CHANNEL       1



void vADCInit(void);
void vADCxScanLow(void);
void vADCxScanHigh(void);
float fADCListValueGet(AdcListEnum enumChannel);

#endif

