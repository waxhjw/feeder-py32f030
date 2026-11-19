#ifndef FEEDER_PLAN_APP_H
#define FEEDER_PLAN_APP_H
#include "product_runtime.h"
#define PLAN_APP_RAW_LIMIT (MEAL_PLAN_LIMIT*6U)
enum { PLAN_APP_IDLE,PLAN_APP_START_PENDING,PLAN_APP_RUNNING,PLAN_APP_FINISH_PENDING };
typedef struct {
    ProductRuntime *runtime;
    MealPlan pending[MEAL_PLAN_LIMIT];
    MealEvent deferred;
    uint32_t pending_count,pending_save,progress_pending,phase,report_pending;
    uint32_t received,saved,rejected,time_requests,time_received,time_rejected;
    uint32_t due_count,starts,finishes,last_event,last_reason,last_target;
    uint32_t online,online_at,query_at,queried,last_raw_error;
} PlanApp;
int plan_app_decode(const uint8_t *raw,uint32_t size,MealPlan plans[MEAL_PLAN_LIMIT],uint32_t *count);
uint32_t plan_app_encode(const MealPlan *plans,uint32_t count,uint8_t raw[PLAN_APP_RAW_LIMIT]);
void plan_app_init(PlanApp *s,ProductRuntime *runtime);
/* 只排入待保存内容；写Flash并读回成功后才允许回报给App。 */
int plan_app_receive(PlanApp *s,const uint8_t *raw,uint32_t size);
int plan_app_time(PlanApp *s,const uint8_t *data,uint32_t size,uint32_t now);
int plan_app_time_due(PlanApp *s,uint32_t now,uint32_t online);
void plan_app_time_requested(PlanApp *s,uint32_t now);
void plan_app_poll(PlanApp *s,uint32_t now,uint32_t running,uint32_t upgrading,uint32_t ready);
uint32_t plan_app_take_start(PlanApp *s);
void plan_app_started(PlanApp *s,uint32_t accepted);
void plan_app_cancel(PlanApp *s);
#endif
