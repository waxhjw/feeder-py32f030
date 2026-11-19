#ifndef FEEDER_APP_MOTOR_H
#define FEEDER_APP_MOTOR_H
#include "touch_motor.h"
#include "wbr_init.h"
typedef struct {
    TouchMotor *motor;
    WbrInit *link;
    uint32_t received,rejected,dirty,last_report,reported_state;
#ifdef FEEDER_QUICK_TIMED
    uint32_t quick_active,quick_started,quick_completed,quick_canceled,quick_ignored,quick_last_end,quick_has_end;
#endif
} AppMotor;
void app_motor_bind(AppMotor *app,TouchMotor *motor,WbrInit *link);
void app_motor_poll(AppMotor *app,uint32_t now);
#ifdef FEEDER_QUICK_TIMED
void app_motor_timing(AppMotor *app,uint32_t now);
#endif
#endif
