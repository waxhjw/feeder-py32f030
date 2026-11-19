#ifndef FEEDER_TOUCH_MOTOR_H
#define FEEDER_TOUCH_MOTOR_H
#include <stdint.h>
enum { TOUCH_BOOT, TOUCH_READY, TOUCH_HOLD, TOUCH_RELEASE, TOUCH_FAULT };
typedef struct {
    uint32_t phase, motor_on, toggles, fault, raw, held_ms;
    uint32_t boot_ms, last_ms, press_ms, release_ms, releasing;
#ifdef FEEDER_TOUCH_REVERSE
    uint32_t direction, reversals, pending, switch_ms, short_pending;
#endif
} TouchMotor;
void touch_motor_reset(TouchMotor *state, uint32_t now);
void touch_motor_step(TouchMotor *state, uint32_t now, uint32_t level);
void touch_motor_stop(TouchMotor *state);
void touch_motor_fail(TouchMotor *state, uint32_t fault);
#endif
