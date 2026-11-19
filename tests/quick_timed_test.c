#define main baseline_main
#include "app_motor_test.c"
#undef main
static void advance(uint32_t ms)
{
    for(uint32_t i=0;i<ms;i++) { now++;touch_motor_step(&motor,now,1U);app_motor_timing(&app,now); }
}
int main(void)
{
    fresh(1);dp(2,0);assert(!motor.motor_on);dp(2,1);uint32_t start=now;
    assert(motor.motor_on && app.quick_active);advance(7499);assert(motor.motor_on);
    advance(1);assert(!motor.motor_on && !app.quick_active && app.quick_completed==1 && now-start==7500);
    puts("PASS quick2 starts once;7499ms running/7500ms stopped, no boot auto-run");
    fresh(1);dp(2,1);start=app.quick_started;advance(2000);dp(2,1);dp(101,1);
    assert(app.quick_started==start && app.quick_ignored==1);advance(5500);assert(!motor.motor_on);
    puts("PASS repeated quick and continuous-ON during timed run cannot extend deadline");
    dp(2,1);assert(!motor.motor_on);advance(999);dp(2,1);assert(!motor.motor_on);
    advance(1);dp(2,1);assert(motor.motor_on && app.quick_active);
    puts("PASS one-second completion cooldown then next explicit press permitted");
    fresh(1);dp(101,1);dp(2,1);assert(motor.motor_on && !app.quick_active && app.quick_ignored==1);
    advance(8000);assert(motor.motor_on);dp(101,0);assert(!motor.motor_on);
    puts("PASS quick during preexisting continuous run does not change its behavior");
    fresh(1);dp(2,1);advance(500);dp(102,1);advance(8000);
    assert(!motor.motor_on && app.quick_canceled==1 && !app.quick_completed);
    fresh(1);dp(2,1);dp(101,0);assert(!app.quick_active && !motor.motor_on);
    puts("PASS102 and101 OFF cancel immediately with no later restart or false completion");
    fresh(1);dp(2,1);touch_motor_stop(&motor);app_motor_timing(&app,++now);
    assert(!app.quick_active && app.quick_canceled==1);
    fresh(1);dp(2,1);touch_motor_fail(&motor,55);app_motor_timing(&app,++now);
    assert(!app.quick_active && !motor.motor_on);
    puts("PASS local/host stop and fault cancel timed request");
    fresh(1);uint8_t combined[]={2,1,0,1,1,101,1,0,1,1,102,1,0,1,1};
    frame(6,combined,sizeof(combined),0);assert(!motor.motor_on && !app.quick_active);
    fresh(1);uint8_t broken[]={2,1,0,1,1,103,1,0,2,1};frame(6,broken,sizeof(broken),0);assert(!motor.motor_on);
    puts("PASS mixed STOP beats quick/start; malformed tail never starts");
    fresh(0);dp(2,1);assert(!app.quick_active && !motor.motor_on);
    fresh(1);dp(103,1);dp(2,1);assert(motor.direction && motor.motor_on);
    advance(500);dp(103,0);assert(motor.pending);advance(300);assert(!motor.pending);
    advance(6700);assert(!motor.motor_on && app.quick_completed==1);
    puts("PASS boot inhibit, selected direction and reverse pause do not extend timed window");
    fresh(1);now=UINT32_MAX-1000U;motor.last_ms=now;dp(2,1);advance(7500);
    assert(!motor.motor_on && app.quick_completed==1);
    puts("PASS uint32 wrap still stops once at7500ms");
    fresh(1);dp(2,1);frame(0,0,0,0);drain();assert(link.replies==2);
    advance(7500);app_motor_poll(&app,now);const WbrInitPacket *p=wbr_init_peek(&link);
    assert(p && p->data[5]==25 && p->data[30]==0 && p->data[25]==0);wbr_init_sent(&link);
    puts("PASS heartbeat processed during timing; finalDP2 false and standby, no food-done claim");
    return 0;
}
