#include "local_controls.h"
#include "quick_timed_config.h"
#include <string.h>
void local_controls_init(LocalControls *s,uint32_t now)
{
    memset(s,0,sizeof(*s));s->selected=1U;s->boot_ms=now;s->last_ms=now;
}
void local_controls_stop(LocalControls *s)
{
    if(s->running) { s->stops++; }
    if(s->quick_active) { s->quick_last_end=s->last_ms;s->quick_has_end=1U; }
    s->quick_active=0U;
#ifdef FEEDER_PLAN_APP
    s->plan_active=0U;
#endif
    s->running=0U;s->pending=0U;s->selected=1U;
}
int local_controls_start(LocalControls *s,uint32_t now,uint32_t remote)
{
    if(s->ready==0U || s->fault || s->running || (s->locked && remote==0U)) { return 0; }
    s->running=1U;s->pending=0U;s->started_at=now;s->starts++;return 1;
}
int local_controls_quick(LocalControls *s,uint32_t now)
{
    /* 保留已确认的7.5秒定时近似和1秒冷却；不把时间当成实测机械份数。 */
    if(s->quick_has_end && (uint32_t)(now-s->quick_last_end)<FEEDER_QUICK_COOLDOWN_MS) { return 0; }
    if(!local_controls_start(s,now,1U)) { return 0; }
    s->quick_active=1U;return 1;
}
void local_controls_direction(LocalControls *s,uint32_t now,uint32_t reverse)
{
    if(reverse<=1U && s->direction!=reverse) {
        s->direction=reverse;
        if(s->running) { s->pending=1U;s->switch_at=now; }
    }
}
/* 松手稳定100ms后才发短按事件，长按只发一次，二者互斥。 */
static uint32_t key_step(LocalKey *k,uint32_t now,uint32_t down,uint32_t long_ms)
{
    if(down) {
        if(k->down==0U) { k->down=1U;k->pressed_at=now;k->long_done=0U; }
        k->releasing=0U;
        if(k->long_done==0U && (uint32_t)(now-k->pressed_at)>=long_ms) { k->long_done=1U;return 2U; }
    } else if(k->down) {
        if(k->releasing==0U) { k->releasing=1U;k->released_at=now; }
        if((uint32_t)(now-k->released_at)>=100U) {
            uint32_t held=k->released_at-k->pressed_at;
            uint32_t event=k->long_done==0U && held>=50U && held<=800U?1U:0U;
            memset(k,0,sizeof(*k));return event;
        }
    }
    return 0U;
}
void local_controls_step(LocalControls *s,uint32_t now,uint32_t top1,uint32_t top2)
{
    if(top1>1U || top2>1U || (uint32_t)(now-s->last_ms)>50U) {
        local_controls_stop(s);s->fault=1U;
    }
    s->last_ms=now;
    if(s->fault) { return; }
    if(s->ready==0U) {
        if(top1==1U && top2==1U) {
            if(s->released==0U) { s->released=1U;s->release_at=now; }
        } else { s->released=0U; }
        if((uint32_t)(now-s->boot_ms)>=2000U && s->released && (uint32_t)(now-s->release_at)>=500U) { s->ready=1U; }
        return;
    }
    if(s->quick_active && (uint32_t)(now-s->started_at)>=FEEDER_QUICK_DURATION_MS) { local_controls_stop(s); }
#ifdef FEEDER_PLAN_APP
    if(s->plan_active && (uint32_t)(now-s->started_at)>=s->plan_duration) { local_controls_stop(s); }
#endif
    if(s->pending && (uint32_t)(now-s->switch_at)>=300U) { s->pending=0U; }
    if(top1==0U && top2==0U) { s->simultaneous=1U; }
    if(s->simultaneous) {
        memset(s->keys,0,sizeof(s->keys));
        if(top1==1U && top2==1U) { s->simultaneous=0U; }
        return;
    }
    uint32_t first=key_step(&s->keys[0],now,top1==0U,3000U);
    uint32_t second=key_step(&s->keys[1],now,top2==0U,5000U);
    if(first==2U) { s->locked^=1U;s->lock_changes++; }
    if(first==1U) {
        if(s->running) { local_controls_stop(s); }
        else { (void)local_controls_start(s,now,0U); }
    }
    if(second==1U && s->locked==0U && s->running==0U) {
        s->selected=s->selected%12U+1U;s->selections++;
    }
}
void local_controls_frame(const LocalControls *s,uint8_t frame[7])
{
    static const uint8_t digits[]={0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,0x7F,0x6F};
    memset(frame,0,7U);frame[0]=0x08U;
    /* 主图GR1~4及PCB位置：SG1=a..SG7=g。图纸映射仍须用户目视验收。 */
    if(s->fault) { frame[1]=0x79U;frame[2]=0x50U; }
    else if(s->locked) { frame[1]=0x38U;frame[2]=0x5CU; } /* Lo */
    else if(s->running) { frame[1]=0x5CU;frame[2]=0x54U; } /* on */
    else { frame[1]=0x73U; } /* P */
    frame[3]=digits[s->selected/10U];frame[4]=digits[s->selected%10U];
}
#ifdef FEEDER_PLAN_APP
int local_controls_plan_start(LocalControls *s,uint32_t now,uint32_t portions)
{
    if(portions<1U || portions>12U || !local_controls_start(s,now,1U)) { return 0; }
    /* 仅CN7独立电机台架：每个所选单位给7500ms，不能称为已完成机械份数。 */
    s->plan_active=1U;s->plan_duration=portions*FEEDER_QUICK_DURATION_MS;s->selected=portions;return 1;
}
void local_controls_time_frame(uint32_t minute,uint8_t frame[7])
{
    static const uint8_t digits[]={0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,0x7F,0x6F};
    memset(frame,0,7U);frame[0]=0x08U;minute%=1440U;
    uint32_t hour=minute/60U;minute%=60U;
    frame[1]=digits[hour/10U];frame[2]=digits[hour%10U];frame[3]=digits[minute/10U];frame[4]=digits[minute%10U];
}
#endif
