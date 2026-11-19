#include "py32f0xx.h"
#include "local_controls.h"
#include "local_controls_io.h"
#include "touch_motor_io.h"
#include <string.h>
#ifdef FEEDER_PLAN_APP
#include "plan_diag.h"
#include "plan_app.h"
static uint32_t notice_at,notice_state;
#endif
typedef struct {
    uint32_t magic,abi,sequence,uptime_ms,ready,locked,selected,running,direction,pending,fault;
    uint32_t starts,stops,lock_changes,selections,app_received,app_rejected,gpioa_odr;
} LocalDebug;
_Static_assert(sizeof(LocalDebug)==72U,"Update local controls reader ABI");
volatile LocalDebug g_local_controls_debug={.magic=0x4C435452U,.abi=1U};
static LocalControls controls;static WbrInit *module;
static uint32_t received,rejected,published,last_report,reported=0xFFFFFFFFU;
static uint8_t display[7];
static int receive(void *ctx,const uint8_t *data,uint32_t length,uint32_t now)
{
    (void)ctx;uint32_t offset=0U,mask=0U,values=0U,quantity=0U,unknown=0U;
#ifdef FEEDER_PLAN_APP
    const uint8_t *plan_raw=0;uint32_t plan_size=0U;
#endif
    while(offset<length) {
        if(length-offset<4U) { rejected++;return 0; }
        uint32_t id=data[offset],type=data[offset+1U],size=((uint32_t)data[offset+2U]<<8U)|data[offset+3U];offset+=4U;
        if(size>length-offset) { rejected++;return 0; }
        if(id==2U || (id>=101U && id<=103U)) {
            if(type!=1U || size!=1U || data[offset]>1U) { rejected++;return 0; }
            uint32_t bit=id==2U?8U:1U<<(id-101U),value=data[offset]?bit:0U;
            if((mask&bit) && (values&bit)!=value) { rejected++;return 0; }
            mask|=bit;values=(values&~bit)|value;
        } else if(id==3U) {
            if(type!=2U || size!=4U) { rejected++;return 0; }
            uint32_t value=((uint32_t)data[offset]<<24U)|((uint32_t)data[offset+1U]<<16U)|
                           ((uint32_t)data[offset+2U]<<8U)|data[offset+3U];
            if(value<1U || value>12U || ((mask&16U) && quantity!=value)) { rejected++;return 0; }
            mask|=16U;quantity=value;
        }
#ifdef FEEDER_PLAN_APP
        else if(id==1U) {
            MealPlan checked[MEAL_PLAN_LIMIT];uint32_t count;
            if(type!=0U || !plan_app_decode(data+offset,size,checked,&count) ||
               ((mask&32U) && (plan_size!=size || memcmp(plan_raw,data+offset,size)))) { rejected++;return 0; }
            plan_raw=data+offset;plan_size=size;mask|=32U;
        }
#endif
        else { unknown=1U; }
        offset+=size;
    }
    if(mask==0U) { return -1; }
    if(unknown) { rejected++;return 0; }
#ifdef FEEDER_PLAN_APP
    if((mask&32U) && (mask&~32U)) { rejected++;return 0; } /* 配置与电机动作不能半帧生效。 */
#endif
    received++;
    uint32_t stop=((mask&2U)&&(values&2U)) || ((mask&1U)&&(values&1U)==0U);
    if(stop) {
#ifdef FEEDER_PLAN_APP
        local_controls_io_stop();
#else
        local_controls_stop(&controls);
#endif
        /* 停止优先；取消尚未松手的触摸手势，避免松手后又重新启动。 */
        controls.simultaneous=1U;return 1;
    }
    if(module->ota_block_dp) { rejected++;return 1; }
#ifdef FEEDER_PLAN_APP
    if(mask&32U) { return plan_diag_receive(plan_raw,plan_size)?1:0; }
#endif
    if(controls.ready==0U || controls.fault) { rejected++;return 1; }
    if((mask&16U) && controls.running==0U) { controls.selected=quantity; }
    if(mask&4U) { local_controls_direction(&controls,now,(values>>2U)&1U); }
    /* DP3只选择、不启动；DP2保留7.5秒定时，101仍是持续运行。重复不延长定时。 */
    if((mask&8U)&&(values&8U)) {
        if(local_controls_quick(&controls,now)==0) { rejected++; }
    } else if((mask&1U)&&(values&1U)) {
        if(local_controls_start(&controls,now,1U)==0) { rejected++; }
    }
    return 1;
}
void local_controls_io_init(WbrInit *link,uint32_t now)
{
    module=link;local_controls_init(&controls,now);
    received=rejected=published=last_report=0U;reported=UINT32_MAX;
    g_local_controls_debug=(LocalDebug){.magic=0x4C435452U,.abi=1U};
    GPIOB->PUPDR&=~((3UL<<12U)|(3UL<<14U));GPIOB->MODER&=~((3UL<<12U)|(3UL<<14U));
    module->app_dp=receive;module->app_context=0;
#ifdef FEEDER_PLAN_APP
    plan_diag_bind(module);notice_at=now;notice_state=0U;
#endif
    local_controls_frame(&controls,display);
}
void local_controls_io_stop(void)
{
    local_controls_stop(&controls);touch_motor_output_direction(0U,controls.direction);
#ifdef FEEDER_PLAN_APP
    plan_diag_cancel();controls.simultaneous=1U;
#endif
}
const uint8_t *local_controls_display(void) { return display; }
#ifdef FEEDER_PLAN_APP
unsigned local_controls_io_running(void) { return controls.running; }
unsigned local_controls_io_ready(void) { return controls.ready && !controls.fault; }
#endif
void local_controls_io_poll(uint32_t now,uint32_t upgrading)
{
    uint32_t levels=GPIOB->IDR;
    local_controls_step(&controls,now,(levels>>7U)&1U,(levels>>6U)&1U);
    if(upgrading) { local_controls_stop(&controls); }
#ifdef FEEDER_PLAN_APP
    if(!upgrading) {
        uint32_t portions=plan_diag_take_start();
        if(portions) { plan_diag_started(local_controls_plan_start(&controls,now,portions)); }
    }
    if(!controls.plan_active) { plan_diag_cancel(); }
#endif
    touch_motor_output_direction(controls.running && controls.pending==0U,controls.direction);
    local_controls_frame(&controls,display);
#ifdef FEEDER_PLAN_APP
    uint32_t notice=controls.selected|(controls.locked<<4U)|(controls.running<<5U);
    if(notice!=notice_state) { notice_state=notice;notice_at=now; }
    if(controls.ready && !controls.running && !controls.locked && !controls.fault && (uint32_t)(now-notice_at)>=5000U) {
        unsigned minute;
        if(plan_diag_time(&minute)) { local_controls_time_frame(minute,display); }
        else { for(unsigned i=1U;i<=4U;i++) { display[i]=0x40U; } } /* 未校时显示----，不冒充00:00。 */
    }
#endif
    uint32_t state=controls.running|(controls.direction<<1U)|(controls.locked<<2U)|
        (controls.pending<<3U)|(controls.quick_active<<4U)|(controls.selected<<5U);
    if(upgrading==0U && (state!=reported || module->app_report_request) && (uint32_t)(now-last_report)>=100U) {
        uint8_t frame[]={101,1,0,1,(uint8_t)controls.running,102,1,0,1,0,103,1,0,1,(uint8_t)controls.direction,
            2,1,0,1,(uint8_t)controls.quick_active,4,4,0,1,(uint8_t)(controls.running && controls.pending==0U)};
        if(wbr_init_app_report(module,frame,sizeof(frame))) { reported=state;last_report=now;module->app_report_request=0U; }
    }
    if((uint32_t)(now-published)>=250U) {
        g_local_controls_debug.sequence++;__DMB();
        g_local_controls_debug.uptime_ms=now;g_local_controls_debug.ready=controls.ready;
        g_local_controls_debug.locked=controls.locked;g_local_controls_debug.selected=controls.selected;
        g_local_controls_debug.running=controls.running;g_local_controls_debug.direction=controls.direction;
        g_local_controls_debug.pending=controls.pending;g_local_controls_debug.fault=controls.fault;
        g_local_controls_debug.starts=controls.starts;g_local_controls_debug.stops=controls.stops;
        g_local_controls_debug.lock_changes=controls.lock_changes;g_local_controls_debug.selections=controls.selections;
        g_local_controls_debug.app_received=received;g_local_controls_debug.app_rejected=rejected;
        g_local_controls_debug.gpioa_odr=GPIOA->ODR;
        __DMB();g_local_controls_debug.sequence++;published=now;
    }
}
