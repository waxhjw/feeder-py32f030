#include "meal_plan.h"
#include <string.h>
_Static_assert(sizeof(MealPlan)==8U,"plan record ABI");
static uint32_t leap(uint32_t year)
{
    return year%4U==0U && (year%100U!=0U || year%400U==0U);
}
static uint32_t days_in(uint32_t year,uint32_t month)
{
    static const uint8_t days[]={31,28,31,30,31,30,31,31,30,31,30,31};
    return days[month-1U]+(uint32_t)(month==2U && leap(year));
}
int meal_plans_valid(const MealPlan *plans,uint32_t count)
{
    if(count>MEAL_PLAN_LIMIT || (count>0U && plans==0)) { return 0; }
    for(uint32_t i=0;i<count;i++) {
        const MealPlan *p=&plans[i];
        if(p->enabled>1U || p->days==0U || (p->days&0x80U) || p->hour>23U || p->minute>59U ||
           p->portions==0U || p->portions>12U || p->reserved[0] || p->reserved[1] || p->reserved[2]) { return 0; }
        for(uint32_t j=0;j<i;j++) {
            const MealPlan *q=&plans[j];
            /* 两条启用计划同一时刻且星期重叠会冲突，必须整批拒绝，不部分保存。 */
            if(p->enabled && q->enabled && p->hour==q->hour && p->minute==q->minute && (p->days&q->days)) { return 0; }
        }
    }
    return 1;
}
void meal_clock_init(MealClock *s,uint32_t now,uint32_t holdover_ms,uint32_t max_gap_ms)
{
    memset(s,0,sizeof(*s));s->last_tick=now;s->holdover_ms=holdover_ms;s->max_gap_ms=max_gap_ms;
}
void meal_clock_poll(MealClock *s,uint32_t now)
{
    uint32_t delta=now-s->last_tick;s->last_tick=now;
    if(s->valid==0U) { return; }
    if(delta>s->max_gap_ms || delta>=0x80000000U || s->age_ms>=s->holdover_ms || delta>=s->holdover_ms-s->age_ms) {
        s->valid=0U;return;
    }
    s->age_ms+=delta;
    uint32_t total=s->fraction_ms+delta,seconds=s->second+total/1000U;
    s->fraction_ms=total%1000U;s->minute+=seconds/60U;s->second=seconds%60U;
    if(s->minute>=MEAL_MINUTE_LIMIT) { s->valid=0U; }
}
int meal_clock_sync(MealClock *s,uint32_t now,const MealCivil *civil)
{
    meal_clock_poll(s,now);
    if(civil==0 || s->holdover_ms==0U || s->holdover_ms>=0x80000000U || s->max_gap_ms==0U ||
       s->max_gap_ms>=0x80000000U || civil->year<2000U || civil->year>2099U || civil->month<1U || civil->month>12U ||
       civil->day<1U || civil->day>days_in(civil->year,civil->month) || civil->hour>23U || civil->minute>59U ||
       civil->second>59U || civil->weekday>6U) { return 0; }
    uint32_t days=0U;
    for(uint32_t year=2000U;year<civil->year;year++) { days+=365U+leap(year); }
    for(uint32_t month=1U;month<civil->month;month++) { days+=days_in(civil->year,month); }
    days+=civil->day-1U;
    if((days+5U)%7U!=civil->weekday) { return 0; }
    s->minute=days*1440U+civil->hour*60U+civil->minute;s->second=civil->second;s->fraction_ms=0U;
    s->age_ms=0U;s->generation++;s->valid=1U;return 1;
}
void meal_scheduler_init(MealScheduler *s,uint32_t durable_watermark)
{
    memset(s,0,sizeof(*s));s->highwater=durable_watermark;
}
MealEvent meal_scheduler_poll(MealScheduler *s,const MealClock *clock,const MealPlan *plans,uint32_t count,uint32_t block_reason)
{
    MealEvent event={MEAL_NONE,MEAL_REASON_NONE,MEAL_TIME_NONE,MEAL_TIME_NONE,0U};
    if(clock->valid!=1U || clock->minute>=MEAL_MINUTE_LIMIT || clock->second>59U || meal_plans_valid(plans,count)==0) {
        s->seen=0U;return event;
    }
    uint32_t minute=clock->minute,old_high=s->highwater;
    if(s->highwater==MEAL_TIME_NONE || minute>s->highwater) { s->highwater=minute; }
    event.minute=s->highwater;
    /* 首次校时、重新获得时间和校时跳变只建立新基准，不追补本分钟。 */
    if(s->seen==0U || s->generation!=clock->generation) {
        s->seen=1U;s->last_minute=minute;s->generation=clock->generation;
        event.kind=MEAL_BASELINE;event.reason=MEAL_REASON_TIME;return event;
    }
    if(minute==s->last_minute) { return event; }
    uint32_t consecutive=(uint32_t)(minute==s->last_minute+1U);s->last_minute=minute;
    if(old_high!=MEAL_TIME_NONE && minute<=old_high) { return event; }
    if(consecutive==0U) { event.kind=MEAL_BASELINE;event.reason=MEAL_REASON_TIME;return event; }
    for(uint32_t i=0;i<count;i++) {
        const MealPlan *p=&plans[i];uint32_t weekday=(minute/1440U+5U)%7U;
        if(p->enabled && (p->days&(1U<<weekday)) && minute%1440U==(uint32_t)p->hour*60U+p->minute) {
            event.kind=MEAL_RUN;event.index=i;event.portions=p->portions;event.minute=minute;
            if(clock->second!=0U) { event.kind=MEAL_SKIP;event.reason=MEAL_REASON_LATE; }
            else if(block_reason!=MEAL_REASON_NONE) { event.kind=MEAL_SKIP;event.reason=block_reason; }
            return event;
        }
    }
    return event;
}
