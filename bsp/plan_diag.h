#ifndef FEEDER_PLAN_DIAG_H
#define FEEDER_PLAN_DIAG_H
void plan_diag_init(unsigned now);
void plan_diag_poll(unsigned now,unsigned upgrading);
#ifdef FEEDER_PLAN_APP
#include "wbr_init.h"
void plan_diag_bind(WbrInit *link);
int plan_diag_receive(const uint8_t *raw,unsigned size);
unsigned plan_diag_take_start(void);
void plan_diag_started(unsigned accepted);
void plan_diag_cancel(void);
int plan_diag_time(unsigned *minute_of_day);
#endif
#endif
