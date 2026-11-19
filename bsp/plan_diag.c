#include "py32f0xx.h"
#include "wbr_init_config.h"
#include "product_runtime.h"
#include "product_flash_layout.h"
#include "motor_off.h"
#include "plan_diag.h"
#include <string.h>
#ifdef FEEDER_PLAN_APP
#include "plan_app.h"
#include "local_controls_io.h"
static PlanApp connected;
static WbrInit *module;
static uint32_t plan_query_seen;
typedef struct {
    uint32_t magic,abi,sequence,uptime_ms,clock_valid,minute,second;
    uint32_t time_requests,time_received,time_rejected,plan_received,plan_saved,plan_rejected,pending_save;
    uint32_t plan_count,phase,due_count,starts,finishes,last_event,last_reason,storage_ready,storage_error;
    uint32_t last_target,report_pending,clock_age_ms,writes,last_raw_error;
} PlanAppDebug;
_Static_assert(sizeof(PlanAppDebug)==112U,"update plan app reader ABI");
volatile PlanAppDebug g_plan_app_debug={.magic=0x50415050U,.abi=1U};
#endif
#ifdef FEEDER_STORE_PROFILE
#include "store_profile.h"
volatile StoreProfile g_store_profile={.magic=0x53505246U,.abi=1U};
volatile StoreProfile g_store_profile_snapshot={.magic=0x53505246U,.abi=1U};
#endif
/* 调试命令不来自App；本阶段真实Flash/时钟/计划验证，所有输出仍硬锁停。 */
typedef struct { uint32_t request,id,args[8];MealPlan plans[10]; } PlanMailbox;
typedef struct {
    uint32_t magic,abi,firmware_id,size,sequence,uptime_ms,ready,storage_error;
    uint32_t clock_valid,minute,second,command_id,command_status,triggers,last_trigger,writes,event,reason;
    ProductData data;
} PlanDebug;
_Static_assert(sizeof(PlanDebug)==200U,"plan debug ABI");
_Static_assert(sizeof(PlanMailbox)==120U,"plan request ABI");
volatile PlanMailbox g_plan_mailbox;
volatile PlanDebug g_plan_debug={.magic=0x504C414EU,.abi=1U,.firmware_id=FEEDER_WBR_INIT_ID,.size=sizeof(PlanDebug)};
static ProductRuntime runtime;
static uint32_t published,triggers,last_trigger=MEAL_TIME_NONE,writes,command_id,command_status,last_event,last_reason;
#if defined(FEEDER_STORE_CACHED) && !defined(FEEDER_PLAN_APP)
static uint32_t pending_cancel;
#endif
static int read_slot(void *ctx,uint32_t slot,uint8_t *data)
{
    (void)ctx;if(slot>=PRODUCT_STORE_SLOTS) { return 0; }
#ifdef FEEDER_STORE_PROFILE
    uint32_t start=store_profile_us();g_store_profile.reads++;
#endif
    memcpy(data,(const void *)(uintptr_t)(PRODUCT_FLASH_BASE+slot*PRODUCT_STORE_PAGE),PRODUCT_STORE_PAGE);
#ifdef FEEDER_STORE_PROFILE
    g_store_profile.read_us+=store_profile_us()-start;
#endif
    return 1;
}
static int write_slot(void *ctx,uint32_t slot,const uint8_t *data)
{
    (void)ctx;if(slot>=PRODUCT_STORE_SLOTS) { return 0; }
    uint32_t aligned[32];memcpy(aligned,data,sizeof(aligned));motor_off_hold();
#ifdef FEEDER_STORE_PROFILE
    uint32_t start=store_profile_us();g_store_profile.writes++;
#endif
    int result=product_flash_write(PRODUCT_FLASH_BASE+slot*PRODUCT_STORE_PAGE,aligned);
#ifdef FEEDER_STORE_PROFILE
    g_store_profile.write_us+=store_profile_us()-start;
#endif
    if(result) { writes++; }return result;
}
static void publish(uint32_t now)
{
#ifdef FEEDER_STORE_PROFILE
    uint32_t sequence=g_store_profile_snapshot.sequence;
    g_store_profile_snapshot.sequence=sequence+1U;__DMB();
    for(unsigned i=0U;i<sizeof(StoreProfile)/4U;i++) {
        if(i!=2U) { ((volatile uint32_t *)&g_store_profile_snapshot)[i]=((volatile uint32_t *)&g_store_profile)[i]; }
    }
    __DMB();g_store_profile_snapshot.sequence=sequence+2U;
#endif
    g_plan_debug.sequence++;__DMB();
    g_plan_debug.uptime_ms=now;g_plan_debug.ready=runtime.storage_ready;g_plan_debug.storage_error=runtime.storage_error;
    g_plan_debug.clock_valid=runtime.clock.valid;g_plan_debug.minute=runtime.clock.minute;g_plan_debug.second=runtime.clock.second;
    g_plan_debug.command_id=command_id;g_plan_debug.command_status=command_status;
    g_plan_debug.triggers=triggers;g_plan_debug.last_trigger=last_trigger;g_plan_debug.writes=writes;
    g_plan_debug.event=last_event;g_plan_debug.reason=last_reason;g_plan_debug.data=runtime.data;
    __DMB();g_plan_debug.sequence++;published=now;
#ifdef FEEDER_PLAN_APP
    uint32_t seq=g_plan_app_debug.sequence;
    PlanAppDebug next={.magic=0x50415050U,.abi=1U,.uptime_ms=now,.clock_valid=runtime.clock.valid,
        .minute=runtime.clock.minute,.second=runtime.clock.second,.time_requests=connected.time_requests,
        .time_received=connected.time_received,.time_rejected=connected.time_rejected,
        .plan_received=connected.received,.plan_saved=connected.saved,.plan_rejected=connected.rejected,
        .pending_save=connected.pending_save,.plan_count=runtime.data.count,.phase=connected.phase,
        .due_count=connected.due_count,.starts=connected.starts,.finishes=connected.finishes,
        .last_event=connected.last_event,.last_reason=connected.last_reason,.storage_ready=runtime.storage_ready,
        .storage_error=runtime.storage_error,.last_target=connected.last_target,.report_pending=connected.report_pending,
        .clock_age_ms=runtime.clock.age_ms,.writes=writes,.last_raw_error=connected.last_raw_error};
    g_plan_app_debug.sequence=seq+1U;__DMB();
    for(unsigned i=0U;i<sizeof(next)/4U;i++) {
        if(i!=2U) { ((volatile uint32_t *)&g_plan_app_debug)[i]=((const uint32_t *)&next)[i]; }
    }
    __DMB();g_plan_app_debug.sequence=seq+2U;
#endif
}
void plan_diag_init(unsigned now)
{
    ProductStorage storage={0,read_slot,write_slot};
    product_runtime_init(&runtime,&storage,now,86400000U,60000U);
#ifdef FEEDER_PLAN_APP
    plan_app_init(&connected,&runtime);
#endif
    publish(now);
}
#ifdef FEEDER_PLAN_APP
static void receive_time(const uint8_t *data,uint32_t size,uint32_t now)
{
    (void)plan_app_time(&connected,data,size,now);
}
void plan_diag_bind(WbrInit *link) { module=link;module->app_time=receive_time;plan_query_seen=link->status_queries; }
int plan_diag_receive(const uint8_t *raw,unsigned size) { return plan_app_receive(&connected,raw,size); }
unsigned plan_diag_take_start(void) { return plan_app_take_start(&connected); }
void plan_diag_started(unsigned accepted) { plan_app_started(&connected,accepted); }
void plan_diag_cancel(void) { plan_app_cancel(&connected); }
int plan_diag_time(unsigned *minute)
{
    if(!runtime.clock.valid) { return 0; }
    *minute=runtime.clock.minute%1440U;return 1;
}
#endif
void plan_diag_poll(unsigned now,unsigned upgrading)
{
#ifdef FEEDER_STORE_PROFILE
    uint32_t start=store_profile_us();g_store_profile.sequence++;
#endif
#ifdef FEEDER_PLAN_APP
    uint32_t old_due=connected.due_count;
    plan_app_poll(&connected,now,local_controls_io_running(),upgrading,local_controls_io_ready());
    if(connected.due_count!=old_due) { last_trigger=runtime.clock.minute; }
    triggers=connected.due_count;last_event=connected.last_event;last_reason=connected.last_reason;
    if(module && !upgrading) {
        /* 一次状态查询只排一次计划回报，不能用尚未清除的普通状态标志
         * 每轮重新排队，否则计划帧可能占住串口，让电机状态回报一直排不上。 */
        if(module->status_queries!=plan_query_seen) {
            plan_query_seen=module->status_queries;connected.report_pending=1U;
        }
        if(connected.report_pending && !connected.pending_save && runtime.storage_ready) {
            uint8_t data[PLAN_APP_RAW_LIMIT+4U]={1U,0U,0U,0U};
            uint32_t size=plan_app_encode(runtime.data.plans,runtime.data.count,data+4U);data[3]=(uint8_t)size;
            if(wbr_init_app_report(module,data,size+4U)) { connected.report_pending=0U; }
        }
        if(plan_app_time_due(&connected,now,module->network_state==4U) && wbr_init_request_time(module)) {
            plan_app_time_requested(&connected,now);
        }
    }
#else
#ifdef FEEDER_STORE_CACHED
    /* 每轮最多一个持久化事务，事务之间回到主循环采样/收串口。 */
    if(pending_cancel) {
        if(upgrading==0U) { product_runtime_finish(&runtime,PRODUCT_RESULT_CANCELLED,0U);pending_cancel=0U; }
        goto poll_done;
    }
#endif
    uint32_t request=g_plan_mailbox.request;
    if(request) {
        uint32_t args[8];MealPlan plans[10];uint32_t id=g_plan_mailbox.id;
        for(unsigned i=0;i<8U;i++) { args[i]=g_plan_mailbox.args[i]; }
        for(unsigned i=0;i<10U;i++) { plans[i]=g_plan_mailbox.plans[i]; }
        g_plan_mailbox.request=0U;__DMB();command_status=0U;
        if(upgrading==0U) {
            if(request==1U) { command_status=product_runtime_set_plans(&runtime,now,plans,args[0])?1U:2U; }
            else if(request==2U) {
                MealCivil time={args[0],args[1],args[2],args[3],args[4],args[5],args[6]};
                command_status=meal_clock_sync(&runtime.clock,now,&time)?1U:2U;
            } else { command_status=3U; }
        } else { command_status=4U; }
        command_id=id;
#ifdef FEEDER_STORE_CACHED
        goto poll_done;
#endif
    }
    ProductAction action;
    MealEvent event=product_runtime_poll(&runtime,now,upgrading?MEAL_REASON_UPGRADE:MEAL_REASON_NONE,&action);
    if(event.kind!=MEAL_NONE) { last_event=event.kind;last_reason=event.reason; }
    if(action.valid) {
        /* 仅记录已持久化的到点请求；没有调用电机，不能报喂食完成。 */
        motor_off_hold();triggers++;last_trigger=event.minute;
#ifdef FEEDER_STORE_CACHED
        pending_cancel=1U;
#else
        product_runtime_finish(&runtime,PRODUCT_RESULT_CANCELLED,0U);
#endif
    }
#ifdef FEEDER_STORE_CACHED
poll_done:
#endif
#endif
    if((uint32_t)(now-published)>=250U) { publish(now); }
#ifdef FEEDER_STORE_PROFILE
    uint32_t elapsed=store_profile_us()-start;
    g_store_profile.polls++;g_store_profile.last_poll_us=elapsed;
    if(elapsed>g_store_profile.max_poll_us) { g_store_profile.max_poll_us=elapsed; }
    g_store_profile.sequence++;
#endif
}
