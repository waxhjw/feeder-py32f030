#ifndef FEEDER_MOTOR_PULSE_H
#define FEEDER_MOTOR_PULSE_H

#include <stdbool.h>
#include <stdint.h>

/* A/B 只表示电气极性，尚不能称为机构前进/返回。 */
typedef enum { MOTOR_POLARITY_A = 1, MOTOR_POLARITY_B = 2 } MotorPolarity;

bool motor_pulse_init(void);
/* 调用者须先验证人工测试前提、单次限制和无故障；驱动再检查参数。 */
bool motor_pulse_start(MotorPolarity polarity, uint32_t duration_ms);
void motor_pulse_stop(void);
bool motor_pulse_active(void);

#endif
