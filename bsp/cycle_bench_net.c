#include "py32f0xx.h"
#include "cycle_bench.h"
#include "led_tm1652.h"
#include "local_controls_io.h"
#include "motor_off.h"
#include "plan_diag.h"
#ifdef FEEDER_CYCLE_SUITE
#include "bench_panel.h"
#include "bench_buzzer.h"
#endif

enum { SOURCE_NONE, SOURCE_QUICK, SOURCE_PLAN, SOURCE_MANUAL };
typedef struct {
    uint32_t magic,abi,sequence,uptime_ms,keys_ready;
    uint32_t raw_micro,stable_micro,input_valid,rises,falls;
    uint32_t selected,state,target,completed,request,partial,fault,rejected;
    uint32_t gpio,led_frames,boot_fault,display;
    uint32_t source,quick_starts,plan_starts;
#ifdef FEEDER_CYCLE_MANUAL_DP
    uint32_t manual_starts,manual_stops,quantity_updates,quantity_dp;
#endif
} CycleNetDebug;
#ifdef FEEDER_CYCLE_MANUAL_DP
_Static_assert(sizeof(CycleNetDebug)==116U,"manual debug ABI");
volatile CycleNetDebug g_cycle_bench_debug={.magic=0x43424E43U,.abi=3U,.quantity_dp=FEEDER_CYCLE_MANUAL_DP};
static uint32_t manual_starts,manual_stops,quantity_updates;
#else
_Static_assert(sizeof(CycleNetDebug)==100U,"cycle net debug ABI");
volatile CycleNetDebug g_cycle_bench_debug={.magic=0x43424E43U,.abi=2U};
#endif
static CycleBench bench;
static WbrInit *module;
static uint8_t display[7];
static uint32_t source,quick_starts,plan_starts,last_sample,published,rejected,reported;
#ifdef FEEDER_CYCLE_SUITE
typedef struct {
    uint32_t sequence,muted,buzzer_mode,cues,last_cue,pair_gestures,pair_attempts,pair_tx,pair_ack;
} BenchPanelDebug;
volatile BenchPanelDebug g_bench_panel_debug;
static PanelKeys keys;
static PanelSound sound;
static uint32_t panel_at, previous_selected, previous_request, mute_at, mute_notice;
static uint32_t pair_at, pair_pending, pair_gestures, cues, last_cue;
static uint32_t pair_active(void)
{
    return pair_pending || module->network_state==0U || module->network_state==1U || module->network_state==6U;
}
static void panel_cue(uint32_t now,uint32_t cue)
{
    panel_sound_start(&sound,now,cue);
    if (sound.cue) { cues++;last_cue=cue; }
}
static void panel_input(uint32_t now,uint32_t busy)
{
    const InputChannel *p=bench.input.channel;
    const uint32_t held=(1U-p[1].stable)|((1U-p[2].stable)<<1U);
    const uint32_t event=panel_keys_step(&keys,now,held,p[1].valid && p[2].valid,busy);
    if (event==PANEL_MUTE) {
        sound.muted^=1U;sound.cue=PANEL_SILENT;mute_notice=1U;mute_at=now;
    } else if (event==PANEL_TOP2 && !pair_active()) {
        (void)cycle_bench_select_external(&bench,now,bench.selected%12U+1U);
    } else if (event==PANEL_TOP1 && !pair_active()) {
        if (cycle_bench_start_external(&bench,now,bench.selected)) { source=SOURCE_MANUAL; }
        else { rejected++; }
    } else if (event==PANEL_PAIR && !pair_active() && !module->pair_attempts && module->network_state<=4U) {
        pair_at=now;pair_pending=1U;pair_gestures++;
    }
    if (pair_pending) {
        if (busy || (uint32_t)(now-pair_at)>=1000U) { pair_pending=0U; }
        else if (wbr_init_request_pair(module)) { pair_pending=0U;panel_at=now; }
    }
}
static void panel_output(uint32_t now)
{
    const FeedCycle *c=&bench.cycle;
    if (previous_request!=c->motor_request) {
        panel_at=now;
        panel_cue(now,c->motor_request?PANEL_CONFIRM:c->state==FEED_DONE?PANEL_COMPLETE:PANEL_CANCEL);
    } else if (previous_selected!=bench.selected) {
        panel_at=now;panel_cue(now,PANEL_CONFIRM);
    }
    previous_request=c->motor_request;previous_selected=bench.selected;
    if (!c->motor_request && pair_active()) {
        display[1]=0x77U;display[2]=0x73U;display[3]=display[4]=0U; /* AP */
    } else if (!c->motor_request && mute_notice && now-mute_at<2000U) {
        display[1]=0x3FU;display[2]=sound.muted?0x71U:0x54U;
        display[3]=sound.muted?0x71U:0U;display[4]=0U; /* OFF / On */
    } else if (!c->motor_request && now-panel_at>=5000U) {
        unsigned minute=0U;const uint32_t valid=plan_diag_time(&minute);
        panel_time_frame(valid,minute,display);
    }
    bench_buzzer_set(panel_sound_output(&sound,now));
}
#endif

static int receive(void *ctx,const uint8_t *data,uint32_t len,uint32_t now)
{
    (void)ctx;
    /* 只接收当前流程的单个DP；完整的Raw计划表仍由原计划服务验证。 */
    if (len<4U || (((uint32_t)data[2]<<8U)|data[3])!=len-4U) { return 0; }
    if (data[0]==1U) { return data[1]==0U && plan_diag_receive(data+4U,len-4U); }
#ifdef FEEDER_CYCLE_MANUAL_DP
    if (data[0]==FEEDER_CYCLE_MANUAL_DP) {
        if (data[1]!=2U || len!=8U || data[4] || data[5] || data[6] || data[7]<1U || data[7]>12U) { return 0; }
        if (cycle_bench_select_external(&bench,now,data[7])) { quantity_updates++; }
        else { rejected++; }
        reported=UINT32_MAX; /* 回传主板当前选择，不把被拒的值显示成已生效。 */
        return 1;
    }
#endif
    if (data[0]!=2U && data[0]!=102U && data[0]!=101U) { return -1; }
    if (data[1]!=1U || len!=5U || data[4]>1U) { return 0; }
    if ((data[0]==102U && data[4]) || (data[0]==101U && !data[4])) {
#ifdef FEEDER_CYCLE_MANUAL_DP
        unsigned was_manual=source==SOURCE_MANUAL && bench.cycle.motor_request;
#endif
        cycle_bench_stop_external(&bench,now);
        plan_diag_cancel();
#ifdef FEEDER_CYCLE_MANUAL_DP
        if (was_manual && bench.cycle.state==FEED_CANCELLED) { manual_stops++; }
        reported=UINT32_MAX;
#endif
    } else if (data[0]==2U && data[4]) {
        reported=UINT32_MAX; /* 复位一次性按钮，不把它当成电机状态。 */
#ifdef FEEDER_CYCLE_SUITE
        if (pair_active()) { rejected++;return 1; }
#endif
        if (cycle_bench_start_external(&bench,now,1U)) {
            source=SOURCE_QUICK;quick_starts++;
        } else { rejected++; }
    } else if (data[0]==101U && data[4]) {
#ifdef FEEDER_CYCLE_MANUAL_DP
        reported=UINT32_MAX;
#ifdef FEEDER_CYCLE_SUITE
        if (pair_active()) { rejected++;return 1; }
#endif
        if (cycle_bench_start_external(&bench,now,bench.selected)) {
            source=SOURCE_MANUAL;manual_starts++;
        } else { rejected++; }
#else
        rejected++; /* 旧诊断保持原行为。 */
#endif
    }
    return 1;
}

void local_controls_io_init(WbrInit *link,uint32_t now)
{
    module=link;
    cycle_bench_init(&bench,now);
    last_sample=now-1U;published=now-100U;reported=UINT32_MAX;
    module->app_dp=receive;module->app_context=0;
    plan_diag_bind(module);
    cycle_bench_frame(&bench,display);
#ifdef FEEDER_CYCLE_SUITE
    bench_buzzer_init();panel_at=now;previous_selected=bench.selected;
#endif
}
#ifdef FEEDER_CYCLE_SUITE
void local_controls_io_stop(void)
{
    cycle_bench_stop_external(&bench,last_sample);plan_diag_cancel();
    sound.cue=PANEL_SILENT;bench_buzzer_set(BUZZER_OFF);pair_pending=0U;
}
#endif
const uint8_t *local_controls_display(void) { return display; }
unsigned local_controls_io_running(void) { return bench.cycle.motor_request; }
unsigned local_controls_io_ready(void)
{
#ifdef FEEDER_CYCLE_SUITE
    if (pair_active()) { return 0U; }
#endif
    return bench.keys_ready && bench.input.channel[0].valid;
}

void local_controls_io_poll(uint32_t now,uint32_t upgrading)
{
#ifdef FEEDER_CYCLE_SUITE
    if (upgrading) { local_controls_io_stop();return; }
    const uint32_t was_running=bench.cycle.motor_request;
#else
    (void)upgrading;
#endif
    if (now==last_sample) { return; }
    last_sample=now;
    const uint32_t portb=GPIOB->IDR;
    const uint32_t levels=((GPIOF->IDR>>3U)&1U)|(((portb>>7U)&1U)<<1U)|(((portb>>6U)&1U)<<2U);
    cycle_bench_step(&bench,now,levels);
#ifdef FEEDER_CYCLE_SUITE
    panel_input(now,was_running || bench.cycle.motor_request);
#endif
    const uint32_t portions=plan_diag_take_start();
    if (portions) {
        const uint32_t accepted=cycle_bench_start_external(&bench,now,portions);
        if (accepted) { source=SOURCE_PLAN;plan_starts++; }
        else { rejected++; }
        plan_diag_started(accepted);
    }
    /* 复用原计划的取消收尾：模拟周期不写实物完成份数，不给App发done。 */
    if (!bench.cycle.motor_request) { plan_diag_cancel(); }
    cycle_bench_frame(&bench,display);
#ifdef FEEDER_CYCLE_SUITE
    panel_output(now);
#endif
    uint32_t status=0U,manual_run=0U;
#ifdef FEEDER_CYCLE_MANUAL_DP
    manual_run=source==SOURCE_MANUAL && bench.cycle.motor_request;
    status=(bench.selected<<1U)|manual_run;
#endif
    if (reported!=status || module->app_report_request) {
        const uint8_t standby[]={101U,1U,0U,1U,(uint8_t)manual_run,102U,1U,0U,1U,0U,103U,1U,0U,1U,0U,
                                  2U,1U,0U,1U,0U,4U,4U,0U,1U,0U
#ifdef FEEDER_CYCLE_MANUAL_DP
                                 ,FEEDER_CYCLE_MANUAL_DP,2U,0U,4U,0U,0U,0U,(uint8_t)bench.selected
#endif
        };
        if (wbr_init_app_report(module,standby,sizeof(standby))) {
            reported=status;module->app_report_request=0U;
        }
    }
    if ((uint32_t)(now-published)<100U) { return; }
    published=now;
    const InputChannel *m=&bench.input.channel[0];
    const FeedCycle *c=&bench.cycle;
    g_cycle_bench_debug.sequence++;__DMB();
    g_cycle_bench_debug.uptime_ms=now;g_cycle_bench_debug.keys_ready=bench.keys_ready;
    g_cycle_bench_debug.raw_micro=m->raw;g_cycle_bench_debug.stable_micro=m->stable;
    g_cycle_bench_debug.input_valid=m->valid;g_cycle_bench_debug.rises=m->rises;g_cycle_bench_debug.falls=m->falls;
    g_cycle_bench_debug.selected=bench.selected;g_cycle_bench_debug.state=c->state;g_cycle_bench_debug.target=c->target;
    g_cycle_bench_debug.completed=c->completed;g_cycle_bench_debug.request=c->motor_request;
    g_cycle_bench_debug.partial=c->partial;g_cycle_bench_debug.fault=c->fault;g_cycle_bench_debug.rejected=rejected;
    g_cycle_bench_debug.gpio=GPIOA->ODR&0x900U;g_cycle_bench_debug.led_frames=g_led_test_debug.frames;
    g_cycle_bench_debug.display=(uint32_t)display[1]|((uint32_t)display[2]<<8U)|
        ((uint32_t)display[3]<<16U)|((uint32_t)display[4]<<24U);
    g_cycle_bench_debug.source=source;g_cycle_bench_debug.quick_starts=quick_starts;g_cycle_bench_debug.plan_starts=plan_starts;
#ifdef FEEDER_CYCLE_MANUAL_DP
    g_cycle_bench_debug.manual_starts=manual_starts;g_cycle_bench_debug.manual_stops=manual_stops;
    g_cycle_bench_debug.quantity_updates=quantity_updates;
#endif
    __DMB();g_cycle_bench_debug.sequence++;
#ifdef FEEDER_CYCLE_SUITE
    g_bench_panel_debug.sequence++;__DMB();
    g_bench_panel_debug.muted=sound.muted;g_bench_panel_debug.buzzer_mode=panel_sound_output(&sound,now);
    g_bench_panel_debug.cues=cues;g_bench_panel_debug.last_cue=last_cue;
    g_bench_panel_debug.pair_gestures=pair_gestures;g_bench_panel_debug.pair_attempts=module->pair_attempts;
    g_bench_panel_debug.pair_tx=module->pair_transmitted;g_bench_panel_debug.pair_ack=module->pair_acks;
    __DMB();g_bench_panel_debug.sequence++;
#endif
}
