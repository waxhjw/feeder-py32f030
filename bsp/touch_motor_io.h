#ifndef FEEDER_TOUCH_MOTOR_IO_H
#define FEEDER_TOUCH_MOTOR_IO_H
#include <stdint.h>
int touch_motor_io_init(void);
uint32_t touch_motor_level(void);
void touch_motor_output(uint32_t on);
void touch_motor_output_direction(uint32_t on,uint32_t reverse);
void touch_motor_watchdog_feed(void);
#endif
