#ifndef FEEDER_INPUT_OBSERVER_H
#define FEEDER_INPUT_OBSERVER_H
#include <stdint.h>
/* 只观察电平，不将高/低解释成到位、缺粮或完成周期。 */
typedef struct { uint32_t raw,stable,valid,rises,falls,last_ms,candidate,since; } InputChannel;
typedef struct { InputChannel channel[3]; uint32_t started,last_ms,samples,max_gap,gaps; } InputObserver;
void input_observer_step(InputObserver *s,uint32_t now,uint32_t levels);
#endif
