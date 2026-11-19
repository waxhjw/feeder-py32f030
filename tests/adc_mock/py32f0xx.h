#include "../input_mock/py32f0xx.h"
extern volatile uint32_t adc_registers[24];
#define ADC1_BASE ((uintptr_t)adc_registers)
#define RCC_APBENR2_ADCEN (1U<<20U)
#define RCC_APBRSTR2_ADCRST (1U<<20U)
#define ADC_CFGR2_CKMODE_1 (2U<<28U)
#define ADC_SMPR_SMP 7U
#define ADC_CHSELR_CHSEL3 8U
#define ADC_CR_ADEN 1U
#define ADC_CR_ADSTART 4U
#define ADC_CR_ADSTP 16U
#define ADC_CR_ADCAL (1U<<31U)
#define ADC_CCSR_CALFAIL (1U<<30U)
#define ADC_ISR_EOC 4U
#define ADC_ISR_EOSEQ 8U
#define ADC_ISR_OVR 16U
