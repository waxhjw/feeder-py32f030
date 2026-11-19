#include "local_controls.h"
#include <assert.h>
#include <stdio.h>
static uint32_t now;static LocalControls s;
static void tick(uint32_t milliseconds,uint32_t first,uint32_t second)
{
    for(uint32_t n=0U;n<milliseconds;n++) { local_controls_step(&s,++now,first,second); }
}
static void ready(void)
{
    now=0U;local_controls_init(&s,now);tick(2100U,1U,1U);assert(s.ready && s.running==0U && s.selected==1U);
}
static void tap(uint32_t key)
{
    tick(150U,key==1U?0U:1U,key==2U?0U:1U);tick(150U,1U,1U);
}
int main(void)
{
    uint8_t frame[7];ready();local_controls_frame(&s,frame);
    assert(frame[1]==0x73U && frame[3]==0x3FU && frame[4]==0x06U);
    puts("PASS boot idle selected1 and P01 display, no automatic motor request");
    tap(1U);assert(s.running && s.starts==1U);local_controls_frame(&s,frame);assert(frame[1]==0x5CU && frame[2]==0x54U);
    tap(1U);assert(s.running==0U && s.stops==1U);
    puts("PASS TOP1 short starts and next short stops with on display");
    tick(3500U,0U,1U);assert(s.locked && s.lock_changes==1U && s.running==0U);
    tick(150U,1U,1U);tap(1U);assert(s.running==0U);tap(2U);assert(s.selected==1U);
    local_controls_frame(&s,frame);assert(frame[1]==0x38U && frame[2]==0x5CU);
    puts("PASS long3s toggles child lock once, does not short-start on release, blocks local start/select");
    tick(3100U,0U,1U);tick(150U,1U,1U);assert(s.locked==0U && s.lock_changes==2U);
    for(unsigned n=0;n<11U;n++) { tap(2U); }assert(s.selected==12U);tap(2U);assert(s.selected==1U);
    puts("PASS unlock and TOP2 cycles every selectable quantity1 through12");
    tap(1U);tap(2U);assert(s.selected==1U && s.running);tick(3100U,0U,1U);tick(150U,1U,1U);
    assert(s.locked && s.running);tap(1U);assert(s.running==0U && s.locked);
    puts("PASS running quantity unchanged, child lock never blocks local stop");
    assert(local_controls_start(&s,now,1U));local_controls_direction(&s,now,1U);assert(s.pending);
    tick(300U,1U,1U);assert(s.running && s.pending==0U && s.direction==1U);
    local_controls_stop(&s);assert(s.pending==0U && s.selected==1U);
    puts("PASS remote request allowed under lock, direction pause and explicit stop retained");
    ready();tap(2U);assert(s.selected==2U);tap(1U);tick(30000U,1U,1U);
    assert(s.running && s.selected==2U);local_controls_stop(&s);
    puts("PASS continuous run preserved beyond30s until explicit stop, selection not fake portions");
    ready();assert(local_controls_quick(&s,now));uint32_t start=now;
    tick(1000U,1U,1U);assert(!local_controls_quick(&s,now));assert(!local_controls_start(&s,now,1U));
    tick(6499U,1U,1U);assert(s.running);tick(1U,1U,1U);assert(now-start==7500U && !s.running);
    assert(!local_controls_quick(&s,now));tick(1000U,1U,1U);assert(local_controls_quick(&s,now));
    local_controls_stop(&s);tick(8000U,1U,1U);assert(!s.running);
    puts("PASS quick7.5s unchanged, repeats never extend, cooldown and cancellation retained");
    ready();tick(3500U,0U,0U);tick(200U,1U,1U);assert(s.starts==0U && s.lock_changes==0U && s.selections==0U);
    puts("PASS ambiguous simultaneous buttons produce no unintended start or selection");
    ready();tap(1U);local_controls_step(&s,now+51U,1U,1U);assert(s.fault && s.running==0U);
    puts("PASS existing50ms input gap limit preserved and output request stopped");
    return 0;
}
