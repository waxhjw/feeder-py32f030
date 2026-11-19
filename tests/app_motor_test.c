#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "app_motor.h"
static WbrInit link;
static TouchMotor motor;
static AppMotor app;
static uint32_t now;
static void frame(uint8_t cmd,const uint8_t *payload,uint32_t length,uint32_t corrupt)
{
    uint8_t b[300]={0x55,0xAA,0,cmd,0,(uint8_t)length};
    assert(length<250U);if(length) { memcpy(b+6,payload,length); }
    for(uint32_t i=0;i<length+6U;i++) { b[length+6U]+=b[i]; }
    b[length+6U]^=(uint8_t)corrupt;
    for(uint32_t i=0;i<length+7U;i++) { wbr_init_feed(&link,b[i],0,now); }
}
static void drain(void)
{
    while(wbr_init_peek(&link)) { wbr_init_sent(&link); }
}
static void fresh(uint32_t ready)
{
    now=0;wbr_init_reset(&link);touch_motor_reset(&motor,0);app_motor_bind(&app,&motor,&link);
    if(ready) { for(now=1;now<=2000U;now++) { touch_motor_step(&motor,now,1U); } }
    frame(0,0,0,0);drain();frame(1,0,0,0);drain();frame(2,0,0,0);drain();
}
static void dp(uint32_t id,uint32_t value)
{
    uint8_t data[]={(uint8_t)id,1,0,1,(uint8_t)value};frame(6,data,sizeof(data),0);
}
int main(void)
{
    fresh(1);assert(!motor.motor_on);dp(101,1);assert(motor.motor_on);
    dp(101,1);assert(motor.motor_on && !motor.direction && !motor.pending);
    dp(101,0);assert(!motor.motor_on);puts("PASS actual UART frames start/stop101 and repeated start is idempotent");
    fresh(1);dp(101,1);dp(103,1);assert(motor.direction && motor.pending);
    uint32_t switch_ms=motor.switch_ms;now+=10;dp(103,1);assert(motor.switch_ms==switch_ms && motor.reversals==1);
    for(uint32_t i=0;i<300U;i++) { now++;touch_motor_step(&motor,now,1U); }
    assert(!motor.pending && motor.motor_on);dp(103,0);assert(!motor.direction && motor.pending);
    dp(102,1);assert(!motor.pending && !motor.motor_on);dp(102,0);assert(!motor.motor_on);
    puts("PASS absolute direction, duplicate command preserves pause; stop cancels restart");
    fresh(1);dp(103,1);assert(motor.direction && !motor.motor_on && !motor.pending);
    puts("PASS stopped direction does not start motor");
    fresh(1);uint8_t combined[]={101,1,0,1,1,102,1,0,1,1,103,1,0,1,1};
    frame(6,combined,sizeof(combined),0);assert(!motor.motor_on && motor.direction);
    puts("PASS same-frame STOP dominates START irrespective of order");
    fresh(1);uint8_t broken[]={101,1,0,1,1,103,1,0,2,1};frame(6,broken,sizeof(broken),0);
    assert(!motor.motor_on && link.malformed==1);
    uint8_t conflict[]={101,1,0,1,1,101,1,0,1,0};frame(6,conflict,sizeof(conflict),0);assert(!motor.motor_on);
    uint8_t bad[]={101,1,0,1,2};frame(6,bad,sizeof(bad),0);assert(!motor.motor_on);
    uint8_t mixed[]={101,1,0,1,1,104,1,0,1,1};frame(6,mixed,sizeof(mixed),0);assert(!motor.motor_on);
    uint8_t valid[]={101,1,0,1,1};frame(6,valid,sizeof(valid),1);assert(!motor.motor_on);
    puts("PASS malformed tail/conflict/bool2/unknown104/checksum rejected atomically");
    fresh(0);dp(101,1);assert(!motor.motor_on && app.rejected);
    fresh(1);touch_motor_fail(&motor,3);dp(101,1);assert(!motor.motor_on);
    fresh(1);dp(2,1);assert(!motor.motor_on && link.blocked_dp==1);
    puts("PASS boot/fault inhibit App start, quick feed still blocked");
    fresh(1);dp(101,1);dp(103,1);app_motor_poll(&app,now);
    const WbrInitPacket *packet=wbr_init_peek(&link);assert(packet && packet->data[3]==7U && packet->data[5]==20U);
    assert(packet->data[10]==1U && packet->data[15]==0U && packet->data[20]==1U && packet->data[25]==0U);
    wbr_init_sent(&link);assert(link.status_reports==1 && link.status_queries==0);
    puts("PASS exact cmd07 reports101/on102/reset103/direction and standby while coasting, no done");
    frame(8,0,0,0);assert(link.app_report_request && link.status_queries==1);
    app_motor_poll(&app,now+101);drain();assert(link.status_reports==2 && !link.app_report_request);
    touch_motor_stop(&motor);app_motor_poll(&app,now+202);packet=wbr_init_peek(&link);
    assert(packet && packet->data[10]==0 && packet->data[20]==1);wbr_init_sent(&link);
    puts("PASS status query and local stop synchronize App without fabricated completion");
    return 0;
}
