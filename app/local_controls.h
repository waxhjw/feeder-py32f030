#ifndef FEEDER_LOCAL_CONTROLS_H
#define FEEDER_LOCAL_CONTROLS_H
#include <stdint.h>
/* 当前为CN7独立电机功能联调；选择份数不等于已实现机械计份。 */
typedef struct { uint32_t down,pressed_at,released_at,releasing,long_done; } LocalKey;
typedef struct {
    uint32_t ready,locked,selected,running,direction,pending,switch_at,started_at,last_ms,boot_ms;
    uint32_t release_at,released,fault,starts,stops,lock_changes,selections,simultaneous;
    uint32_t quick_active,quick_last_end,quick_has_end;
#ifdef FEEDER_PLAN_APP
    uint32_t plan_active,plan_duration;
#endif
    LocalKey keys[2];
} LocalControls;
void local_controls_init(LocalControls *s,uint32_t now);
void local_controls_step(LocalControls *s,uint32_t now,uint32_t top1,uint32_t top2);
void local_controls_stop(LocalControls *s);
int local_controls_start(LocalControls *s,uint32_t now,uint32_t remote);
int local_controls_quick(LocalControls *s,uint32_t now);
void local_controls_direction(LocalControls *s,uint32_t now,uint32_t reverse);
void local_controls_frame(const LocalControls *s,uint8_t frame[7]);
#ifdef FEEDER_PLAN_APP
int local_controls_plan_start(LocalControls *s,uint32_t now,uint32_t portions);
void local_controls_time_frame(uint32_t minute_of_day,uint8_t frame[7]);
#endif
#endif
