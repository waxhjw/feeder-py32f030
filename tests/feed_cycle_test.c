#include "feed_cycle.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
/* 以下时间/极性均为人工构造的测试数据，不是本机标定值，禁止用于出厂配置。 */
static FeedProfile example(void)
{
    FeedProfile p={1U,0U,12U,80U,200U,800U,40U,20U,20U,20U};return p;
}
static FeedObservation obs(uint32_t now,uint32_t level)
{
    FeedObservation in={1U,level,now,FEED_PROTECT_OK,now};return in;
}
static uint32_t step(FeedCycle *s,uint32_t now,FeedObservation *in,uint32_t command,uint32_t portions)
{
    uint32_t result=feed_cycle_step(s,now,in,command,portions);
    assert(s->completed<=s->target);
    assert(s->motor_request==(uint32_t)(s->state==FEED_LEAVE || s->state==FEED_RETURN));
    if(s->state==FEED_FAULT) { assert(s->fault!=FEED_OK && s->position_trusted==0U); }
    return result;
}
static void prepare(FeedCycle *s,FeedProfile p,uint32_t now,uint32_t portions)
{
    FeedObservation in=obs(now,p.home_level);feed_cycle_init(s,&p,now);
    assert(s->motor_request==0U && s->state==FEED_DISARMED);
    assert(step(s,now,&in,FEED_ARM,0U)==FEED_ACCEPTED);
    assert(s->motor_request==0U);
    assert(step(s,now,&in,FEED_START,portions)==FEED_ACCEPTED);
}
static void tick(FeedCycle *s,uint32_t *now,uint32_t level)
{
    *now+=10U;FeedObservation in=obs(*now,level);step(s,*now,&in,0U,0U);
}
static void cycle(FeedCycle *s,uint32_t *now)
{
    tick(s,now,s->profile.home_level^1U);
    for(unsigned i=0;i<3U;i++) { tick(s,now,s->profile.home_level^1U); }
    tick(s,now,s->profile.home_level);
}
int main(void)
{
    FeedCycle s;FeedObservation in;FeedProfile p;uint32_t now;
    feed_cycle_init(&s,0,0U);in=obs(0U,0U);
    assert(step(&s,0U,&in,FEED_ARM,0U)==FEED_REJECTED);
    assert(step(&s,0U,&in,FEED_START,1U)==FEED_REJECTED);
    assert(s.motor_request==0U);
    puts("PASS absent measured hardware profile cannot arm or request motion");

    for(unsigned which=0;which<10U;which++) {
        p=example();
        switch(which) {
        case 0:p.hardware_validated=0U;break;case 1:p.home_level=2U;break;
        case 2:p.max_portions=13U;break;case 3:p.leave_timeout_ms=0U;break;
        case 4:p.min_cycle_ms=200U;break;case 5:p.cycle_timeout_ms=80U;break;
        case 6:p.task_timeout_ms=100U;break;case 7:p.max_poll_gap_ms=30U;break;
        case 8:p.input_max_age_ms=100U;break;default:p.task_timeout_ms=0x80000000U;break;
        }
        feed_cycle_init(&s,&p,0U);assert(step(&s,0U,&in,FEED_ARM,0U)==FEED_REJECTED);
    }
    puts("PASS malformed profile ranges and unsafe timeout relations are rejected");

    p=example();feed_cycle_init(&s,&p,0U);in=obs(0U,0U);
    assert(step(&s,0U,&in,FEED_START,1U)==FEED_REJECTED);
    assert(step(&s,0U,&in,FEED_ARM|FEED_START,1U)==FEED_REJECTED);
    in.position_level=1U;assert(step(&s,0U,&in,FEED_ARM,0U)==FEED_REJECTED);
    assert(s.state==FEED_DISARMED);
    puts("PASS explicit safe-home preparation required, no automatic homing");

    for(unsigned level=0;level<2U;level++) {
        p=example();p.home_level=level;now=0U;prepare(&s,p,now,1U);
        assert(s.completed==0U);tick(&s,&now,level);assert(s.completed==0U);
        cycle(&s,&now);assert(s.state==FEED_DONE && s.completed==1U && s.partial==0U);
        tick(&s,&now,level);assert(s.completed==1U);
    }
    puts("PASS both configurable marker polarities require leave then return, no startup count");

    for(unsigned target=1U;target<=12U;target++) {
        now=0U;prepare(&s,example(),now,target);
        for(unsigned n=1;n<=target;n++) {
            cycle(&s,&now);assert(s.completed==n);
            assert(s.motor_request==(uint32_t)(n<target));
        }
        assert(s.state==FEED_DONE);
    }
    puts("PASS one to twelve synthetic mechanical cycles stop only at requested count");

    now=0U;prepare(&s,example(),now,2U);in=obs(now,0U);
    for(unsigned n=0;n<5U;n++) {
        assert(step(&s,now,&in,FEED_START,12U)==FEED_BUSY);
        assert(s.target==2U && s.task_started==0U && s.cycle_started==0U);
    }
    cycle(&s,&now);cycle(&s,&now);assert(s.state==FEED_DONE && s.completed==2U);
    in=obs(now,0U);assert(step(&s,now,&in,FEED_START,2U)==FEED_REJECTED);
    puts("PASS running duplicates neither extend deadline nor queue, terminal state blocks replay");

    for(unsigned quantity=0;quantity<=13U;quantity+=13U) {
        p=example();feed_cycle_init(&s,&p,0U);in=obs(0U,0U);step(&s,0U,&in,FEED_ARM,0U);
        assert(step(&s,0U,&in,FEED_START,quantity)==FEED_REJECTED);
    }
    puts("PASS out of range quantities cannot start");

    now=0U;prepare(&s,example(),now,2U);cycle(&s,&now);tick(&s,&now,1U);
    in=obs(now,1U);assert(step(&s,now,&in,FEED_STOP|FEED_START|FEED_ARM,12U)==FEED_ACCEPTED);
    assert(s.state==FEED_CANCELLED && s.completed==1U && s.partial==1U);
    in=obs(now,0U);assert(step(&s,now,&in,FEED_START,1U)==FEED_REJECTED);
    assert(step(&s,now,&in,0U,0U)==FEED_NO_COMMAND && s.motor_request==0U);
    puts("PASS stop wins simultaneous requests, preserves full count and discards partial cycle");

    now=0U;prepare(&s,example(),now,1U);
    for(unsigned n=0;n<8U;n++) { tick(&s,&now,0U); }
    assert(s.state==FEED_FAULT && s.fault==FEED_LEAVE_TIMEOUT && s.completed==0U);
    puts("PASS stuck at marker hits bounded leave timeout without counting a portion");

    now=0U;prepare(&s,example(),now,1U);
    for(unsigned n=0;n<20U;n++) { tick(&s,&now,1U); }
    assert(s.fault==FEED_CYCLE_TIMEOUT && s.completed==0U);
    puts("PASS missing return hits cycle timeout and stops");

    p=example();p.task_timeout_ms=250U;now=0U;prepare(&s,p,now,2U);
    tick(&s,&now,1U);for(unsigned n=0;n<13U;n++) { tick(&s,&now,1U); }tick(&s,&now,0U);
    assert(s.completed==1U);
    for(unsigned n=0;n<10U;n++) { tick(&s,&now,1U); }
    assert(s.fault==FEED_TASK_TIMEOUT && s.completed==1U);
    puts("PASS whole task deadline survives cycle transitions");

    now=0U;prepare(&s,example(),now,1U);tick(&s,&now,1U);tick(&s,&now,0U);
    assert(s.fault==FEED_CYCLE_TOO_FAST && s.completed==0U);
    puts("PASS implausibly early marker return fails instead of adding a cycle");

    for(unsigned which=0;which<6U;which++) {
        prepare(&s,example(),100U,1U);in=obs(110U,0U);
        switch(which) {
        case 0:in.position_valid=0U;break;case 1:in.position_ms=80U;break;
        case 2:in.protection=FEED_PROTECT_UNKNOWN;break;case 3:in.protection_ms=80U;break;
        case 4:in.protection=FEED_PROTECT_TRIPPED;break;default:in.position_ms=111U;break;
        }
        step(&s,110U,&in,0U,0U);assert(s.state==FEED_FAULT && s.completed==0U && s.partial==1U);
    }
    puts("PASS invalid, stale, future-dated position and unavailable or tripped protection stop motion");

    prepare(&s,example(),0U,1U);in=obs(21U,0U);step(&s,21U,&in,0U,0U);
    assert(s.fault==FEED_POLL_GAP);
    prepare(&s,example(),100U,1U);in=obs(99U,0U);step(&s,99U,&in,0U,0U);
    assert(s.fault==FEED_POLL_GAP);
    puts("PASS missed execution window and reversed time fail closed");

    now=0xFFFFFFE0U;prepare(&s,example(),now,2U);cycle(&s,&now);cycle(&s,&now);
    assert(s.state==FEED_DONE && s.completed==2U);
    puts("PASS normal uint32 tick wrap keeps deadlines and counting correct");

    prepare(&s,example(),0U,1U);in=obs(10U,0U);in.protection=FEED_PROTECT_TRIPPED;
    step(&s,10U,&in,FEED_STOP|FEED_START,1U);assert(s.fault==FEED_PROTECTION_TRIP);
    in=obs(10U,0U);step(&s,10U,&in,FEED_STOP,0U);assert(s.state==FEED_FAULT);
    step(&s,10U,&in,FEED_START,1U);assert(s.state==FEED_FAULT);
    assert(step(&s,10U,&in,FEED_ARM,0U)==FEED_ACCEPTED && s.motor_request==0U);
    assert(step(&s,10U,&in,FEED_START,1U)==FEED_ACCEPTED);
    puts("PASS protection fault remains latched through STOP or START, explicit safe rearm does not move");

    prepare(&s,example(),0U,1U);in=obs(10U,1U);step(&s,10U,&in,0U,0U);
    p=example();feed_cycle_init(&s,&p,20U);in=obs(20U,1U);
    assert(s.state==FEED_DISARMED && s.motor_request==0U);
    assert(step(&s,20U,&in,FEED_ARM,0U)==FEED_REJECTED);
    puts("PASS reset never resumes unfinished task or seeks unknown position");

    prepare(&s,example(),0U,1U);step(&s,10U,0,FEED_START,1U);
    assert(s.fault==FEED_POSITION_INVALID && s.motor_request==0U);
    puts("PASS missing observation cannot preserve active output");

    now=0U;prepare(&s,example(),now,1U);
    for(unsigned n=0;n<19U;n++) { tick(&s,&now,1U); }
    in=obs(200U,0U);step(&s,200U,&in,FEED_START,12U);
    assert(s.fault==FEED_CYCLE_TIMEOUT && s.completed==0U && s.target==1U);
    puts("PASS deadline at same instant as return wins and cannot be extended by START");

    now=0U;prepare(&s,example(),now,1U);
    for(unsigned n=0;n<4U;n++) { tick(&s,&now,1U); }
    in=obs(50U,0U);in.protection=FEED_PROTECT_TRIPPED;step(&s,50U,&in,0U,0U);
    assert(s.fault==FEED_PROTECTION_TRIP && s.completed==0U);
    puts("PASS protection at marker return prevents false completion");
    return 0;
}
