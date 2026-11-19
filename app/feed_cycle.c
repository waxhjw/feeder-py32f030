#include "feed_cycle.h"
#include <string.h>
static uint32_t active(const FeedCycle *s)
{
    return s->state==FEED_LEAVE || s->state==FEED_RETURN;
}
static uint32_t bounded(uint32_t value)
{
    return value>0U && value<0x80000000U;
}
static uint32_t profile_ok(const FeedProfile *p)
{
    return p->hardware_validated==1U && p->home_level<=1U && p->max_portions>0U && p->max_portions<=12U &&
        bounded(p->min_cycle_ms) && bounded(p->leave_timeout_ms) && bounded(p->cycle_timeout_ms) &&
        bounded(p->task_timeout_ms) && bounded(p->input_max_age_ms) && bounded(p->protection_max_age_ms) &&
        bounded(p->max_poll_gap_ms) && p->min_cycle_ms<p->cycle_timeout_ms &&
        p->leave_timeout_ms<p->cycle_timeout_ms && p->cycle_timeout_ms<=p->task_timeout_ms &&
        p->input_max_age_ms<=p->leave_timeout_ms && p->max_poll_gap_ms<=p->input_max_age_ms;
}
static uint32_t observation_fault(const FeedCycle *s,uint32_t now,const FeedObservation *in)
{
    if(in==0 || in->position_valid!=1U || in->position_level>1U) { return FEED_POSITION_INVALID; }
    if((uint32_t)(now-in->position_ms)>s->profile.input_max_age_ms) { return FEED_POSITION_STALE; }
    if(in->protection==FEED_PROTECT_TRIPPED) { return FEED_PROTECTION_TRIP; }
    if(in->protection!=FEED_PROTECT_OK ||
       (uint32_t)(now-in->protection_ms)>s->profile.protection_max_age_ms) { return FEED_PROTECTION_INVALID; }
    return FEED_OK;
}
static void fail(FeedCycle *s,uint32_t reason)
{
    /* 只保留已完整观察的周期；未完成部分不计份，禁止自动恢复输出。 */
    if(active(s)) { s->partial=1U; }
    s->state=FEED_FAULT;s->fault=reason;s->motor_request=0U;s->position_trusted=0U;
}
void feed_cycle_init(FeedCycle *s,const FeedProfile *profile,uint32_t now)
{
    memset(s,0,sizeof(*s));
    if(profile!=0) { s->profile=*profile; }
    s->last_step=now;s->state=FEED_DISARMED;
}
uint32_t feed_cycle_step(FeedCycle *s,uint32_t now,const FeedObservation *in,uint32_t commands,uint32_t portions)
{
    uint32_t was_active=active(s),gap=now-s->last_step,reason;
    s->last_step=now;
    if(was_active) {
        reason=profile_ok(&s->profile)?observation_fault(s,now,in):FEED_BAD_PROFILE;
        if(reason!=FEED_OK) { fail(s,reason); }
        else if(gap>s->profile.max_poll_gap_ms) { fail(s,FEED_POLL_GAP); }
        else if((uint32_t)(now-s->task_started)>=s->profile.task_timeout_ms) { fail(s,FEED_TASK_TIMEOUT); }
        else if((uint32_t)(now-s->cycle_started)>=s->profile.cycle_timeout_ms) { fail(s,FEED_CYCLE_TIMEOUT); }
        else if(s->state==FEED_LEAVE && (uint32_t)(now-s->cycle_started)>=s->profile.leave_timeout_ms) {
            fail(s,FEED_LEAVE_TIMEOUT);
        }
    }
    /* 停止优先于本轮启动/回零准备；已发现的故障保持锁存，不被STOP擦掉。 */
    if(commands&FEED_STOP) {
        if(s->state!=FEED_FAULT) {
            if(was_active) { s->partial=1U; }
            s->state=FEED_CANCELLED;s->position_trusted=0U;
        }
        s->motor_request=0U;return FEED_ACCEPTED;
    }
    if(was_active) {
        /* 运行中任何启动/准备请求都不追加份数、不重置计时，也不排队。 */
        if(active(s)) {
            if(s->state==FEED_LEAVE && in->position_level!=s->profile.home_level) { s->state=FEED_RETURN; }
            else if(s->state==FEED_RETURN && in->position_level==s->profile.home_level) {
                if((uint32_t)(now-s->cycle_started)<s->profile.min_cycle_ms) { fail(s,FEED_CYCLE_TOO_FAST); }
                else {
                    s->completed++;
                    if(s->completed==s->target) {
                        s->state=FEED_DONE;s->motor_request=0U;s->position_trusted=1U;
                    } else { s->state=FEED_LEAVE;s->cycle_started=now; }
                }
            }
        }
        return commands?FEED_BUSY:FEED_NO_COMMAND;
    }
    if(commands==0U) { return FEED_NO_COMMAND; }
    if(commands!=FEED_ARM && commands!=FEED_START) { return FEED_REJECTED; }
    if(profile_ok(&s->profile)==0U) { return FEED_REJECTED; }
    reason=observation_fault(s,now,in);
    if(reason!=FEED_OK || in->position_level!=s->profile.home_level) { return FEED_REJECTED; }
    if(commands==FEED_ARM) {
        /* 只能在已确认停位的有效观察下人工重新准备，不自动寻找停位。 */
        s->state=FEED_READY;s->fault=FEED_OK;s->motor_request=0U;s->position_trusted=1U;
        return FEED_ACCEPTED;
    }
    if(s->state!=FEED_READY || s->position_trusted!=1U || portions==0U || portions>s->profile.max_portions) {
        return FEED_REJECTED;
    }
    s->target=portions;s->completed=0U;s->partial=0U;s->position_trusted=0U;
    s->task_started=now;s->cycle_started=now;s->state=FEED_LEAVE;s->motor_request=1U;
    return FEED_ACCEPTED;
}
