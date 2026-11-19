#include "py32f0xx.h"
#include "led_tm1652.h"
#include "local_controls_io.h"
#include "plan_diag.h"
#include "product_flash_layout.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#ifdef FEEDER_CYCLE_SUITE
#include "bench_panel.h"
#include "bench_buzzer.h"
static uint32_t sound_writes,last_sound,plan_bytes;
void bench_buzzer_init(void) { last_sound=0U; }
void bench_buzzer_set(uint32_t mode)
{
    if (mode && mode!=last_sound) { sound_writes++; }
    last_sound=mode;
}
#endif

GPIO_TypeDef touch_gpioa,touch_gpiob,input_gpiof;
volatile LedTestDebug g_led_test_debug;
static WbrInit link;
static uint32_t now,writes,plan_reports;
#ifdef FEEDER_CYCLE_MANUAL_DP
static uint32_t selected_report, manual_report;
#endif

void motor_off_hold(void) { assert(touch_gpioa.ODR==0U); }
int product_flash_write(uint32_t address,const uint32_t *data)
{
    assert(address>=PRODUCT_FLASH_BASE && address<PRODUCT_FLASH_END && address%128U==0U);
    assert(!local_controls_io_running());
    memcpy((void *)(uintptr_t)address,data,128U);writes++;
    return 1;
}
static void wire_send(unsigned command,const uint8_t *payload,unsigned len)
{
    uint8_t frame[80]={0x55,0xAA,0,(uint8_t)command,0,(uint8_t)len},sum=0;
    if (len) { memcpy(frame+6,payload,len); }
    for (unsigned i=0;i<len+6;i++) { sum+=frame[i]; }
    frame[len+6]=sum;
    for (unsigned i=0;i<len+7;i++) { wbr_init_feed(&link,frame[i],0U,now); }
}
static void advance_inputs(unsigned duration,unsigned micro,unsigned keys)
{
    touch_gpiob.IDR=keys;
    input_gpiof.IDR=micro<<3;
    while (duration--) {
        local_controls_io_poll(++now,0U);
        plan_diag_poll(now,0U);
        const WbrInitPacket *packet;
        while ((packet=wbr_init_peek(&link))) {
            if (packet->data[3]==7U) {
                for (unsigned p=6U;p+4U<=packet->length-1U;) {
                    unsigned n=(unsigned)packet->data[p+2U]*256U+packet->data[p+3U];
                    assert(p+4U+n<=packet->length-1U);
                    if (packet->data[p]==4U) { assert(n==1U && packet->data[p+4U]==0U); }
                    if (packet->data[p]==1U) {
                        plan_reports++;
#ifdef FEEDER_CYCLE_SUITE
                        plan_bytes=n;
#endif
                    }
#ifdef FEEDER_CYCLE_MANUAL_DP
                    if (packet->data[p]==FEEDER_CYCLE_MANUAL_DP) {
                        assert(n==4U && packet->data[p+1U]==2U);selected_report=packet->data[p+7U];
                    }
                    if (packet->data[p]==101U) { assert(n==1U);manual_report=packet->data[p+4U]; }
#endif
                    p+=4U+n;
                }
            }
            wbr_init_sent(&link);
        }
        assert(!link.locked && touch_gpioa.ODR==0U);
    }
}
static void advance(unsigned duration,unsigned micro) { advance_inputs(duration,micro,0xC0U); }
static void display(unsigned completed,unsigned target)
{
    static const uint8_t digits[]={0x3FU,0x06U,0x5BU};
    const uint8_t *frame=local_controls_display();
    assert(frame[1]==0x3FU && frame[2]==digits[completed] && frame[3]==0x3FU && frame[4]==digits[target]);
}

int main(void)
{
    /* Windows进程内模拟Flash。主控实际存储逻辑、协议解析与输入去抖参与此测试。 */
    void *memory=VirtualAlloc((void *)(uintptr_t)0x08000000U,0x10000U,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    assert(memory==(void *)(uintptr_t)0x08000000U);
    memset(memory,0xFF,0x10000U);
    wbr_init_reset(&link);plan_diag_init(now);local_controls_io_init(&link,now);
    wire_send(0U,NULL,0U);advance(50U,1U);
    const uint8_t online[]={4U};wire_send(3U,online,1U);advance(50U,1U);
    assert(link.network_state==4U);

    const uint8_t quick[]={2U,1U,0U,1U,1U};
    wire_send(6U,quick,sizeof(quick));advance(60U,0U);assert(!local_controls_io_running());
#ifndef FEEDER_CYCLE_MANUAL_DP
    const uint8_t continuous[]={101U,1U,0U,1U,1U};
    wire_send(6U,continuous,sizeof(continuous));advance(50U,0U);assert(!local_controls_io_running());
#endif
    wire_send(6U,quick,sizeof(quick));advance(150U,0U);assert(local_controls_io_running());display(0,1);
    wire_send(6U,quick,sizeof(quick));advance(100U,1U);assert(local_controls_io_running());display(0,1);
    advance(40U,0U);assert(!local_controls_io_running());display(1,1);
    puts("PASS real UART DP2 starts one shared CN6 cycle and open CN6 does not start");

    const uint8_t plan[]={1U,0U,0U,6U,0U,0x20U,15U,25U,2U,1U};
    const uint8_t civil[]={1U,26U,11U,17U,15U,24U,55U,2U};
    wire_send(6U,plan,sizeof(plan));advance(50U,0U);assert(plan_reports>0U);
    const uint32_t before=writes;
    wire_send(0x1CU,civil,sizeof(civil));advance(5100U,0U);
    assert(local_controls_io_running() && writes>before);display(0,2);
    advance(120U,1U);advance(60U,0U);assert(local_controls_io_running());display(1,2);
    advance(130U,1U);advance(70U,0U);assert(!local_controls_io_running());display(2,2);
    assert(writes>=before+4U);
    puts("PASS real saved DP1 and clock reach natural due; two CN6 cycles finish without drive or physical done");
#ifdef FEEDER_CYCLE_MANUAL_DP
    const uint8_t two[]={FEEDER_CYCLE_MANUAL_DP,2U,0U,4U,0U,0U,0U,2U};
    const uint8_t three[]={FEEDER_CYCLE_MANUAL_DP,2U,0U,4U,0U,0U,0U,3U};
    const uint8_t invalid[]={FEEDER_CYCLE_MANUAL_DP,2U,0U,4U,0U,0U,0U,0U};
    const uint8_t start[]={101U,1U,0U,1U,1U},stop[]={102U,1U,0U,1U,1U};
    wire_send(6U,two,sizeof(two));advance(100U,0U);
    assert(!local_controls_io_running() && selected_report==2U && manual_report==0U &&
           local_controls_display()[1]==0x73U && local_controls_display()[4]==0x5BU);
    const uint32_t malformed_before=link.malformed;
    wire_send(6U,invalid,sizeof(invalid));advance(100U,0U);
    assert(link.malformed==malformed_before+1U && selected_report==2U && !local_controls_io_running());
    wire_send(6U,start,sizeof(start));advance(120U,0U);
    assert(local_controls_io_running() && manual_report==1U);display(0,2);
    wire_send(6U,three,sizeof(three));wire_send(6U,start,sizeof(start));
    advance(120U,1U);advance(100U,0U);
    assert(local_controls_io_running() && selected_report==2U);display(1,2);
    advance(120U,1U);advance(100U,0U);display(2,2);
    assert(!local_controls_io_running() && manual_report==0U && selected_report==2U);
    puts("PASS quantity only selects, real App START runs the selected two, repeat/edit cannot change target");

    wire_send(6U,start,sizeof(start));advance(150U,0U);
    advance(100U,1U);advance(80U,0U);display(1,2);
    advance(100U,1U);
    wire_send(6U,stop,sizeof(stop));advance(60U,1U);
    assert(!local_controls_io_running() && manual_report==0U);
    advance(80U,0U);
    assert(local_controls_display()[1]==0x6DU && local_controls_display()[4]==0x06U);
    puts("PASS App STOP after one completed cycle preserves one and ignores the late return");
#endif
#ifdef FEEDER_CYCLE_SUITE
    PanelSound cue={0};
    panel_sound_start(&cue,0U,PANEL_COMPLETE);
    assert(panel_sound_output(&cue,10U)==BUZZER_TONE && panel_sound_output(&cue,45U)==BUZZER_TONE &&
           panel_sound_output(&cue,130U)==BUZZER_OFF && panel_sound_output(&cue,200U)==BUZZER_TONE &&
           panel_sound_output(&cue,300U)==BUZZER_OFF);
    cue.muted=1U;assert(panel_sound_output(&cue,200U)==BUZZER_OFF);
    cue.muted=0U;panel_sound_start(&cue,500U,PANEL_CANCEL);
    assert(panel_sound_output(&cue,719U)==BUZZER_TONE && panel_sound_output(&cue,720U)==BUZZER_OFF);
    advance(5200U,0U);
    assert(local_controls_display()[1]==0x06U && local_controls_display()[2]==0x6DU);
    const uint32_t first_sound=sound_writes;
    advance_inputs(160U,0U,0x80U);advance(210U,0U);
    assert(!local_controls_io_running() && sound_writes>first_sound && local_controls_display()[1]==0x73U);
    advance_inputs(2100U,0U,0U);advance(2200U,0U);
    const uint32_t muted_sound=sound_writes;
    advance_inputs(160U,0U,0x80U);advance(210U,0U);
    assert(!local_controls_io_running() && last_sound==0U && sound_writes==muted_sound && local_controls_display()[1]==0x73U);
    puts("PASS time, short select, dual-key mute, visible feedback and bounded two-note cue");

    uint8_t limits[60]={1,0,0,51};
    for (unsigned i=0;i<11U;i++) {
        limits[5U+i*5U]=0x7FU;limits[6U+i*5U]=22U;limits[7U+i*5U]=(uint8_t)i;limits[8U+i*5U]=1U;
    }
    wire_send(6U,limits,55U);advance(100U,0U);assert(plan_bytes==51U);
    const uint32_t saved_before=writes;
    limits[3]=56U;wire_send(6U,limits,60U);
    wire_send(8U,NULL,0U);advance(100U,0U);assert(writes==saved_before && plan_bytes==51U);
    advance_inputs(5100U,0U,0x80U);assert(link.pair_attempts==1U && link.pair_transmitted==1U);
    advance_inputs(1000U,0U,0x80U);advance(70U,0U);
    assert(link.pair_attempts==1U && !local_controls_io_running());
    const uint8_t pairing[]={1U};wire_send(3U,pairing,1U);advance(100U,0U);
    assert(local_controls_display()[1]==0x77U && local_controls_display()[2]==0x73U);
    wire_send(3U,online,1U);wire_send(8U,NULL,0U);advance(200U,0U);
    assert(plan_bytes==51U && writes==saved_before);
    puts("PASS rejects eleventh plan and TOP2 five-second pairing preserves the ten-plan table");
#endif
    return 0;
}
