#include "product_runtime.h"
#include <string.h>
static uint32_t highwater(uint32_t a,uint32_t b)
{
    if(a==MEAL_TIME_NONE) { return b; }
    return b!=MEAL_TIME_NONE && b>a?b:a;
}
static int commit(ProductRuntime *s,const ProductData *next)
{
#ifdef FEEDER_STORE_CACHED
    uint32_t status=product_store_save_cursor(&s->storage,&s->data,next,&s->cursor);
#else
    uint32_t status=product_store_save(&s->storage,next);
#endif
    if(status!=PRODUCT_STORE_OK) { s->storage_error=status;s->storage_ready=0U;return 0; }
    s->data=*next;return 1;
}
uint32_t product_runtime_init(ProductRuntime *s,const ProductStorage *storage,uint32_t now,uint32_t holdover_ms,uint32_t max_gap_ms)
{
    memset(s,0,sizeof(*s));product_data_empty(&s->data);
    meal_clock_init(&s->clock,now,holdover_ms,max_gap_ms);
    if(storage!=0) { s->storage=*storage; }
#ifdef FEEDER_STORE_CACHED
    uint32_t status=product_store_open(&s->storage,&s->data,&s->cursor);
#else
    uint32_t seq,slot;
    uint32_t status=product_store_load(&s->storage,&s->data,&seq,&slot);
#endif
    if(status!=PRODUCT_STORE_OK && status!=PRODUCT_STORE_EMPTY) { s->storage_error=status;return status; }
    s->storage_ready=1U;
    if(s->data.active) {
        /* 记录表明有未收尾任务，只标中断，禁止掉电后继续电机或冒充完成。 */
        ProductData interrupted=s->data;interrupted.active=0U;interrupted.result=PRODUCT_RESULT_INTERRUPTED;
        s->recovered_interruption=1U;
        if(commit(s,&interrupted)==0) { return s->storage_error; }
    }
    meal_scheduler_init(&s->scheduler,s->data.watermark);return status;
}
int product_runtime_set_plans(ProductRuntime *s,uint32_t now,const MealPlan *plans,uint32_t count)
{
    if(s->storage_ready==0U || meal_plans_valid(plans,count)==0) { return 0; }
    ProductData next=s->data;memset(next.plans,0,sizeof(next.plans));next.count=count;
    if(count>0U) { memcpy(next.plans,plans,count*sizeof(*plans)); }
    if(next.count==s->data.count && memcmp(next.plans,s->data.plans,sizeof(next.plans))==0) { return 1; }
    meal_clock_poll(&s->clock,now);next.watermark=highwater(next.watermark,s->scheduler.highwater);
    if(s->clock.valid) { next.watermark=highwater(next.watermark,s->clock.minute); }
    if(commit(s,&next)==0) { return 0; }
    /* 修改计划不追补本分钟，不改变正在执行的任务份数/来源。 */
    s->scheduler.seen=0U;s->scheduler.highwater=next.watermark;return 1;
}
static int claim(ProductRuntime *s,uint32_t source,uint32_t index,uint32_t portions,uint32_t watermark,ProductAction *action)
{
    if(s->data.task_id==0xFFFFFFFFU) { return 0; }
    ProductData next=s->data;next.watermark=highwater(next.watermark,watermark);
    next.task_id++;next.active=1U;next.source=source;next.target=portions;next.completed=0U;
    next.result=PRODUCT_RESULT_NONE;next.plan_index=index;
    if(commit(s,&next)==0) { return 0; }
    /* 只有双副本均完成写入和读回，才交给执行层一个运行请求。 */
    action->valid=1U;action->task_id=next.task_id;action->source=source;
    action->plan_index=index;action->portions=portions;return 1;
}
MealEvent product_runtime_poll(ProductRuntime *s,uint32_t now,uint32_t block_reason,ProductAction *action)
{
    MealEvent event={MEAL_NONE,MEAL_REASON_NONE,MEAL_TIME_NONE,MEAL_TIME_NONE,0U};
    memset(action,0,sizeof(*action));meal_clock_poll(&s->clock,now);
    if(s->storage_ready==0U) { return event; }
    /* 外部只提供执行器状态，TIME/LATE/STORAGE属于内部结果，不能混作阻塞输入。 */
    if(block_reason!=MEAL_REASON_NONE && block_reason!=MEAL_REASON_BUSY &&
       block_reason!=MEAL_REASON_UPGRADE && block_reason!=MEAL_REASON_NOT_READY) {
        block_reason=MEAL_REASON_NOT_READY;
    }
    if(s->data.active && block_reason==MEAL_REASON_NONE) { block_reason=MEAL_REASON_BUSY; }
    event=meal_scheduler_poll(&s->scheduler,&s->clock,s->data.plans,s->data.count,block_reason);
    if(event.kind==MEAL_RUN) {
        if(claim(s,PRODUCT_SOURCE_PLAN,event.index,event.portions,event.minute,action)==0) {
            event.kind=MEAL_SKIP;event.reason=MEAL_REASON_STORAGE;
        }
    } else if(event.kind==MEAL_BASELINE || event.kind==MEAL_SKIP) {
        ProductData next=s->data;next.watermark=highwater(next.watermark,event.minute);
        if(event.kind==MEAL_SKIP) { next.skip_minute=event.minute;next.skip_reason=event.reason;next.skip_index=event.index; }
        if(memcmp(&next,&s->data,sizeof(next)) && commit(s,&next)==0) { event.kind=MEAL_SKIP;event.reason=MEAL_REASON_STORAGE; }
    }
    return event;
}
int product_runtime_manual(ProductRuntime *s,uint32_t portions,uint32_t ready,ProductAction *action)
{
    memset(action,0,sizeof(*action));
    if(s->storage_ready==0U || s->data.active || ready!=1U || portions==0U || portions>12U) { return 0; }
    return claim(s,PRODUCT_SOURCE_MANUAL,MEAL_TIME_NONE,portions,s->scheduler.highwater,action);
}
int product_runtime_finish(ProductRuntime *s,uint32_t result,uint32_t completed)
{
    if(s->storage_ready==0U || s->data.active==0U || completed>s->data.target ||
       result<PRODUCT_RESULT_DONE || result>PRODUCT_RESULT_FAULT ||
       (result==PRODUCT_RESULT_DONE && completed!=s->data.target)) { return 0; }
    ProductData next=s->data;next.active=0U;next.completed=completed;next.result=result;
    return commit(s,&next);
}
#ifdef FEEDER_PLAN_APP
int product_runtime_progress(ProductRuntime *s,const MealEvent *event)
{
    if(!s->storage_ready || !event || (event->kind!=MEAL_BASELINE && event->kind!=MEAL_SKIP)) { return 0; }
    ProductData next=s->data;next.watermark=highwater(next.watermark,highwater(s->scheduler.highwater,event->minute));
    if(event->kind==MEAL_SKIP) {
        next.skip_minute=event->minute;next.skip_reason=event->reason;next.skip_index=event->index;
    }
    if(!memcmp(&next,&s->data,sizeof(next))) { return 1; }
    return commit(s,&next);
}
#endif
