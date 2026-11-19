#ifndef FEEDER_INPUT_OBSERVER_IO_H
#define FEEDER_INPUT_OBSERVER_IO_H
#include <stdint.h>
typedef struct {
    uint32_t magic,abi,firmware_id,size,sequence,uptime_ms,samples,max_gap,gaps;
    uint32_t channel[3][6];
    uint32_t gpiof_moder,gpiof_pupdr,gpiob_moder,gpiob_pupdr,adc_status;
} InputDebug;
extern volatile InputDebug g_input_debug;
void input_observer_io_init(void);
void input_observer_io_poll(unsigned now);
#endif
