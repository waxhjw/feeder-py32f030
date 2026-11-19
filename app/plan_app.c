#include "plan_app.h"
#include <string.h>
static uint8_t reverse_days(uint8_t days)
{
    uint8_t result=0U;
    for(unsigned i=0U;i<7U;i++) { result|=((days>>i)&1U)<<(6U-i); }
    return result;
}
int plan_app_decode(const uint8_t *raw,uint32_t size,MealPlan plans[MEAL_PLAN_LIMIT],uint32_t *count)
{
#ifdef FEEDER_PLAN_APP_TABLE
    /* 实物两条报文为00+(周期、时、分、份数、启用)*N。
     * 版本只在整表开头出现一次，空表正是单字节00。 */
    memset(plans,0,sizeof(MealPlan)*MEAL_PLAN_LIMIT);
    if(size==0U) { *count=0U;return 1; } /* 保留既有长度零空表兼容。 */
    if(raw==0 || raw[0]!=0U || size>1U+5U*MEAL_PLAN_LIMIT || (size-1U)%5U) { return 0; }
    *count=(size-1U)/5U;
    for(uint32_t i=0U;i<*count;i++) {
        const uint8_t *p=raw+1U+i*5U;
        plans[i]=(MealPlan){.days=reverse_days(p[0]),.hour=p[1],.minute=p[2],.portions=p[3],.enabled=p[4]};
        if(p[0]&0x80U) { return 0; }
    }
    return meal_plans_valid(plans,*count);
#else
#ifdef FEEDER_PLAN_APP_EMPTY
    /* 实物App删除最后一条时发送单字节00；只有这个确切标记表示空表。
     * 其他短包仍拒绝，不能因格式错误误删已保存的计划。 */
    if(size==1U && raw && raw[0]==0U) { size=0U; }
#endif
    /* 20261117真实手机报文：00 7f 0f 19 01 01=每天15:25、1份、启用。
     * 普通版本每条6字节；不把带录音字段的23字节布局套到普通计划。 */
    if(size>PLAN_APP_RAW_LIMIT || size%6U || (size && raw==0)) { return 0; }
    memset(plans,0,sizeof(MealPlan)*MEAL_PLAN_LIMIT);*count=size/6U;
    for(uint32_t i=0U;i<*count;i++) {
        const uint8_t *p=raw+i*6U;
        if(p[0]!=0U) { return 0; } /* 录音版本未实现，整批拒绝，不伪装保存成功。 */
        plans[i]=(MealPlan){.days=reverse_days(p[1]),.hour=p[2],.minute=p[3],.portions=p[4],.enabled=p[5]};
        if(p[1]&0x80U) { return 0; }
    }
    return meal_plans_valid(plans,*count);
#endif
}
uint32_t plan_app_encode(const MealPlan *plans,uint32_t count,uint8_t raw[PLAN_APP_RAW_LIMIT])
{
#ifdef FEEDER_PLAN_APP_TABLE
    raw[0]=0U;
    for(uint32_t i=0U;i<count;i++) {
        uint8_t *p=raw+1U+i*5U;p[0]=reverse_days(plans[i].days);
        p[1]=plans[i].hour;p[2]=plans[i].minute;p[3]=plans[i].portions;p[4]=plans[i].enabled;
    }
    return 1U+count*5U;
#else
#ifdef FEEDER_PLAN_APP_EMPTY
    /* 回报与本产品实际App的空表格式一致；保存成功后才调用此函数回报。 */
    if(count==0U) { raw[0]=0U;return 1U; }
#endif
    for(uint32_t i=0U;i<count;i++) {
        uint8_t *p=raw+i*6U;p[0]=0U;p[1]=reverse_days(plans[i].days);
        p[2]=plans[i].hour;p[3]=plans[i].minute;p[4]=plans[i].portions;p[5]=plans[i].enabled;
    }
    return count*6U;
#endif
}
void plan_app_init(PlanApp *s,ProductRuntime *runtime)
{
    memset(s,0,sizeof(*s));s->runtime=runtime;s->report_pending=1U;
}
int plan_app_receive(PlanApp *s,const uint8_t *raw,uint32_t size)
{
    MealPlan decoded[MEAL_PLAN_LIMIT];uint32_t count;
    s->received++;
    if(!plan_app_decode(raw,size,decoded,&count)) {
        s->rejected++;s->last_raw_error=1U;s->report_pending=1U;return 0;
    }
    memcpy(s->pending,decoded,sizeof(decoded));s->pending_count=count;s->pending_save=1U;
    s->last_raw_error=0U;return 1;
}
int plan_app_time(PlanApp *s,const uint8_t *data,uint32_t size,uint32_t now)
{
    if(size!=8U || data==0 || data[0]!=1U || data[7]<1U || data[7]>7U) { s->time_rejected++;return 0; }
    MealCivil civil={2000U+data[1],data[2],data[3],data[4],data[5],data[6],data[7]-1U};
    MealClock next=s->runtime->clock;
    if(!meal_clock_sync(&next,now,&civil)) { s->time_rejected++;return 0; }
    s->runtime->clock=next;s->time_received++;return 1;
}
int plan_app_time_due(PlanApp *s,uint32_t now,uint32_t online)
{
    if(!online) { s->online=0U;s->queried=0U;return 0; }
    if(!s->online) { s->online=1U;s->online_at=now;s->queried=0U; }
    if((uint32_t)(now-s->online_at)<5000U) { return 0; }
    uint32_t interval=s->runtime->clock.valid?600000U:(s->queried<3U?15000U:300000U);
    return !s->queried || (uint32_t)(now-s->query_at)>=interval;
}
void plan_app_time_requested(PlanApp *s,uint32_t now)
{
    s->query_at=now;s->queried++;s->time_requests++;
}
static void note_event(PlanApp *s,MealEvent event)
{
    if(event.kind!=MEAL_NONE) { s->last_event=event.kind;s->last_reason=event.reason; }
}
void plan_app_poll(PlanApp *s,uint32_t now,uint32_t running,uint32_t upgrading,uint32_t ready)
{
    ProductRuntime *r=s->runtime;
    if(running || upgrading) {
        /* 运动/升级期间只推进RAM时钟和跳过记录，不擦Flash、不打断输出。
         * 停机后先保存这段进度，再处理新计划和下一次动作。 */
        meal_clock_poll(&r->clock,now);
        MealEvent event=meal_scheduler_poll(&r->scheduler,&r->clock,r->data.plans,r->data.count,
            upgrading?MEAL_REASON_UPGRADE:MEAL_REASON_BUSY);
        note_event(s,event);
        if(event.kind==MEAL_BASELINE || event.kind==MEAL_SKIP) {
            /* 保留最近一次实际跳过的原因；后来的校时基准不能把它抹掉。 */
            if(!s->progress_pending || event.kind==MEAL_SKIP || s->deferred.kind!=MEAL_SKIP) { s->deferred=event; }
            s->progress_pending=1U;
        }
        return;
    }
    if(s->phase==PLAN_APP_FINISH_PENDING) {
        /* 台架定时结束不等于完成机械份数：保留completed0、取消真实计份任务。
         * 独立test finishes统计说明台架演示已收尾，不向App发送虚假done。 */
        if(product_runtime_finish(r,PRODUCT_RESULT_CANCELLED,0U)) { s->finishes++;s->phase=PLAN_APP_IDLE; }
        return;
    }
    if(s->progress_pending) {
        if(product_runtime_progress(r,&s->deferred)) { s->progress_pending=0U; }
        return;
    }
    if(s->pending_save) {
        s->pending_save=0U;
        if(product_runtime_set_plans(r,now,s->pending,s->pending_count)) { s->saved++;s->report_pending=1U; }
        else { s->rejected++;s->last_raw_error=2U; }
        return;
    }
    ProductAction action;
    MealEvent event=product_runtime_poll(r,now,ready?MEAL_REASON_NONE:MEAL_REASON_NOT_READY,&action);
    note_event(s,event);
    if(action.valid) {
        s->due_count++;s->last_target=action.portions;s->phase=PLAN_APP_START_PENDING;
    }
}
uint32_t plan_app_take_start(PlanApp *s)
{
    if(s->phase!=PLAN_APP_START_PENDING) { return 0U; }
    s->phase=PLAN_APP_RUNNING;return s->last_target;
}
void plan_app_started(PlanApp *s,uint32_t accepted)
{
    if(accepted) { s->starts++; } else { s->phase=PLAN_APP_FINISH_PENDING; }
}
void plan_app_cancel(PlanApp *s)
{
    if(s->phase==PLAN_APP_RUNNING || s->phase==PLAN_APP_START_PENDING) { s->phase=PLAN_APP_FINISH_PENDING; }
}
