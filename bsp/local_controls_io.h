#ifndef FEEDER_LOCAL_CONTROLS_IO_H
#define FEEDER_LOCAL_CONTROLS_IO_H
#include "wbr_init.h"
void local_controls_io_init(WbrInit *link,uint32_t now);
void local_controls_io_poll(uint32_t now,uint32_t upgrading);
void local_controls_io_stop(void);
const uint8_t *local_controls_display(void);
#ifdef FEEDER_PLAN_APP
unsigned local_controls_io_running(void);
unsigned local_controls_io_ready(void);
#endif
#endif
