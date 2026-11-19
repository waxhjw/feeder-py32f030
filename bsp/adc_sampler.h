#ifndef FEEDER_ADC_SAMPLER_H
#define FEEDER_ADC_SAMPLER_H
#include <stdint.h>
enum { ADC_DIAG_CAL=1,ADC_DIAG_WAIT_CAL=2,ADC_DIAG_WAIT_ENABLE=3,ADC_DIAG_SAMPLE=4,ADC_DIAG_ERROR=5,ADC_DIAG_PAUSED=6,ADC_DIAG_WAIT_RESET=7 };
typedef struct {
    uint32_t magic,abi,firmware_id,size,sequence,uptime_ms,phase,valid;
    uint32_t raw,minimum,maximum,samples,error,calibrations,calibration_median,gpioa_moder;
} AdcDebug;
extern volatile AdcDebug g_adc_debug;
void adc_sampler_init(uint32_t now);
void adc_sampler_poll(uint32_t now);
void adc_sampler_pause(uint32_t now);
#endif
