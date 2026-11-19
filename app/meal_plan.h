#ifndef FEEDER_MEAL_PLAN_H
#define FEEDER_MEAL_PLAN_H
#include <stdint.h>
#define MEAL_PLAN_LIMIT 10U
#define MEAL_MINUTE_LIMIT 52596000UL /* 2000-01-01至2099-12-31，结束边界不包含。 */
#define MEAL_TIME_NONE 0xFFFFFFFFUL
typedef struct { uint8_t enabled,days,hour,minute,portions,reserved[3]; } MealPlan;
typedef struct { uint32_t year,month,day,hour,minute,second,weekday; } MealCivil;
/* weekday=0周一..6周日。涂鸦报文/时区转换适配另做，不直接照搬报文字节。 */
typedef struct {
    uint32_t valid,minute,second,fraction_ms,generation,last_tick,age_ms,holdover_ms,max_gap_ms;
} MealClock;
enum { MEAL_NONE,MEAL_BASELINE,MEAL_RUN,MEAL_SKIP };
enum { MEAL_REASON_NONE,MEAL_REASON_TIME,MEAL_REASON_LATE,MEAL_REASON_BUSY,MEAL_REASON_UPGRADE,MEAL_REASON_NOT_READY,MEAL_REASON_STORAGE };
typedef struct { uint32_t seen,last_minute,highwater,generation; } MealScheduler;
typedef struct { uint32_t kind,reason,index,minute,portions; } MealEvent;
int meal_plans_valid(const MealPlan *plans,uint32_t count);
void meal_clock_init(MealClock *s,uint32_t now,uint32_t holdover_ms,uint32_t max_gap_ms);
void meal_clock_poll(MealClock *s,uint32_t now);
int meal_clock_sync(MealClock *s,uint32_t now,const MealCivil *civil);
void meal_scheduler_init(MealScheduler *s,uint32_t durable_watermark);
MealEvent meal_scheduler_poll(MealScheduler *s,const MealClock *clock,const MealPlan *plans,uint32_t count,uint32_t block_reason);
#endif
