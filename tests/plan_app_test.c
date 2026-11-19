#include "plan_app.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
/* 存储模型只检查业务与提交次序；真实Flash和实际动作另由台架验收。 */
static uint8_t pages[PRODUCT_STORE_SLOTS][PRODUCT_STORE_PAGE];
static uint32_t writes,busy,fail_at;
static ProductRuntime runtime;
static PlanApp app;
static int read_page(void *ctx,uint32_t slot,uint8_t *data)
{ (void)ctx;assert(slot<PRODUCT_STORE_SLOTS);memcpy(data,pages[slot],PRODUCT_STORE_PAGE);return 1; }
static int write_page(void *ctx,uint32_t slot,const uint8_t *data)
{
    (void)ctx;assert(!busy && slot<PRODUCT_STORE_SLOTS);writes++;
    if(fail_at && writes==fail_at) { return 0; }
    memcpy(pages[slot],data,PRODUCT_STORE_PAGE);return 1;
}
static ProductStorage storage(void) { return (ProductStorage){0,read_page,write_page}; }
static void fresh(void)
{
    memset(pages,0xFF,sizeof(pages));writes=busy=fail_at=0U;
    ProductStorage io=storage();assert(product_runtime_init(&runtime,&io,0U,86400000U,60000U)==PRODUCT_STORE_EMPTY);
    plan_app_init(&app,&runtime);
}
static void sync_at(unsigned hour,unsigned minute,unsigned second,uint32_t now)
{
    const uint8_t packet[]={1,26,11,17,(uint8_t)hour,(uint8_t)minute,(uint8_t)second,2};
    assert(plan_app_time(&app,packet,sizeof(packet),now));
}
static void set_one(void)
{
    const uint8_t raw[]={0,127,15,25,1,1};
    assert(plan_app_receive(&app,raw,sizeof(raw)));assert(!runtime.data.count && !writes);
    plan_app_poll(&app,0U,0U,0U,1U);assert(runtime.data.count==1U && app.saved==1U && writes==2U);
}
static void codec(void)
{
    MealPlan plans[10];uint32_t count;uint8_t out[60];
    const uint8_t captured[]={0,127,15,25,1,1};
    assert(plan_app_decode(captured,6U,plans,&count) && count==1U);
    assert(plans[0].hour==15U && plans[0].minute==25U && plans[0].days==127U && plans[0].enabled && plans[0].portions==1U);
    assert(plan_app_encode(plans,count,out)==6U && !memcmp(captured,out,6U));
    puts("PASS actual captured DP1 ordinary6-byte plan decodes and serializes identically");
    uint8_t days[]={0,1,15,25,1,1};assert(plan_app_decode(days,6U,plans,&count) && plans[0].days==64U);
    days[1]=64U;assert(plan_app_decode(days,6U,plans,&count) && plans[0].days==1U);
    puts("PASS Tuya Sunday-low-bit weekdays map to internal Monday-low-bit and roundtrip");
    uint8_t all[61]={0};unsigned total;
#ifdef FEEDER_PLAN_APP_TABLE
    total=51U;for(unsigned i=0;i<10U;i++) { memcpy(all+1U+i*5U,captured+1U,5U);all[2U+i*5U]=(uint8_t)i; }
    const uint8_t two[]={0,32,16,44,1,1,32,16,53,1,1};
    assert(plan_app_decode(two,sizeof(two),plans,&count) && count==2U);
    assert(plans[0].minute==44U && plans[1].minute==53U && plans[0].days==2U);
    assert(plan_app_encode(plans,count,out)==sizeof(two) && !memcmp(two,out,sizeof(two)));
    puts("PASS actual two-plan11-byte capture roundtrips one table header plus two5-byte records");
#else
    total=60U;for(unsigned i=0;i<10U;i++) { memcpy(all+i*6U,captured,6U);all[i*6U+2U]=(uint8_t)i; }
#endif
    assert(plan_app_decode(all,total,plans,&count) && count==10U && plan_app_encode(plans,count,out)==total && !memcmp(all,out,total));
    puts("PASS ten ordinary plans fit full-table layout without truncation or reducing limit");
    const unsigned offsets[]={0,1,2,3,4,5},bad[]={1,128,24,60,13,2};
    for(unsigned i=0;i<6U;i++) {
        uint8_t invalid[6];memcpy(invalid,captured,6U);invalid[offsets[i]]=(uint8_t)bad[i];
        assert(!plan_app_decode(invalid,6U,plans,&count));
    }
    assert(!plan_app_decode(captured,5U,plans,&count));assert(!plan_app_decode(all,61U,plans,&count));
    uint8_t duplicate[12];memcpy(duplicate,captured,6U);
#ifdef FEEDER_PLAN_APP_TABLE
    memcpy(duplicate+6U,captured+1U,5U);assert(!plan_app_decode(duplicate,11U,plans,&count));
#else
    memcpy(duplicate+6U,captured,6U);assert(!plan_app_decode(duplicate,12U,plans,&count));
#endif
    assert(plan_app_decode(0,0U,plans,&count) && !count);
#ifdef FEEDER_PLAN_APP_EMPTY
    const uint8_t empty[]={0},invalid_empty[]={1},truncated[]={0,0};
    assert(plan_app_decode(empty,1U,plans,&count) && !count);
    assert(plan_app_encode(plans,count,out)==1U && out[0]==0U);
    assert(!plan_app_decode(invalid_empty,1U,plans,&count));
    assert(!plan_app_decode(truncated,2U,plans,&count));
    puts("PASS captured singleton00 deletion and matching empty report; other truncated packets still rejected");
#endif
    puts("PASS malformed fields, voice version and overlapping enabled plans rejected atomically; empty clears");
}
int main(void)
{
    codec();fresh();set_one();unsigned old=writes;
    const uint8_t raw[]={0,127,15,25,1,1};
    assert(plan_app_receive(&app,raw,6U));plan_app_poll(&app,1U,0U,0U,1U);assert(writes==old);
    puts("PASS receive stages first, durable mirrored save precedes report, identical resend avoids erase");
    sync_at(15,24,50,100U);uint32_t generation=runtime.clock.generation;
    const uint8_t wrong[]={1,26,11,17,15,24,50,1};
    assert(!plan_app_time(&app,wrong,8U,101U) && runtime.clock.generation==generation);
    const uint8_t failed[]={0};assert(!plan_app_time(&app,failed,1U,102U) && runtime.clock.valid);
    puts("PASS local-time success/calendar/weekday checked, bad or unsuccessful replies preserve last valid time");
    plan_app_poll(&app,100U,0U,0U,1U);assert(!app.due_count);
    plan_app_poll(&app,10100U,0U,0U,1U);assert(app.phase==PLAN_APP_START_PENDING && app.due_count==1U);
    ProductData persisted;uint32_t sequence,slot;ProductStorage io=storage();
    assert(product_store_load(&io,&persisted,&sequence,&slot)==PRODUCT_STORE_OK && persisted.active && persisted.target==1U);
    assert(plan_app_take_start(&app)==1U);plan_app_started(&app,1U);old=writes;
    busy=1U;plan_app_poll(&app,10101U,1U,0U,1U);assert(writes==old);busy=0U;
    plan_app_cancel(&app);plan_app_poll(&app,18000U,0U,0U,1U);
    assert(app.finishes==1U && !runtime.data.active && !runtime.data.completed && runtime.data.result==PRODUCT_RESULT_CANCELLED);
    puts("PASS natural due claims durable intent before start, timed-bench finish never fabricates completed portions");
    plan_app_poll(&app,18001U,0U,0U,1U);assert(app.due_count==1U);
    sync_at(15,24,50,19000U);plan_app_poll(&app,19000U,0U,0U,1U);plan_app_poll(&app,29000U,0U,0U,1U);
    assert(app.due_count==1U);
    puts("PASS same minute and backward time resync cannot replay an already consumed plan");
    fresh();set_one();sync_at(15,25,0,10U);plan_app_poll(&app,10U,0U,0U,1U);assert(!app.due_count);
    puts("PASS first sync at exact plan time establishes baseline without immediate catchup motion");
    fresh();set_one();sync_at(15,24,50,10U);plan_app_poll(&app,10U,0U,0U,1U);old=writes;
    busy=1U;plan_app_poll(&app,10010U,1U,0U,1U);assert(writes==old && app.progress_pending && app.last_reason==MEAL_REASON_BUSY);
    busy=0U;plan_app_poll(&app,10011U,0U,0U,1U);assert(!app.progress_pending && runtime.data.skip_reason==MEAL_REASON_BUSY);
    plan_app_poll(&app,10012U,0U,0U,1U);assert(!app.due_count);
    puts("PASS busy due minute consumed in RAM without Flash during motor operation, then durably recorded with no replay");
    fresh();set_one();sync_at(15,24,50,10U);plan_app_poll(&app,10U,0U,0U,1U);old=writes;
    busy=1U;plan_app_poll(&app,10010U,0U,1U,1U);assert(writes==old && app.last_reason==MEAL_REASON_UPGRADE);busy=0U;
    plan_app_poll(&app,10011U,0U,0U,1U);assert(runtime.data.skip_reason==MEAL_REASON_UPGRADE && !app.due_count);
    puts("PASS upgrade due minute performs no competing Flash operation and is recorded after upgrade stops");
    fresh();set_one();old=writes;uint8_t updated[]={0,127,16,30,2,1};
    assert(plan_app_receive(&app,updated,6U));busy=1U;plan_app_poll(&app,1U,1U,0U,1U);
    assert(writes==old && app.pending_save && runtime.data.plans[0].hour==15U);busy=0U;
    plan_app_poll(&app,2U,0U,0U,1U);assert(!app.pending_save && runtime.data.plans[0].hour==16U);
    assert(plan_app_receive(&app,0,0U));plan_app_poll(&app,3U,0U,0U,1U);assert(!runtime.data.count);
    puts("PASS plan edit waits for motor idle, then commits atomically; deleting all plans persists empty table");
    fresh();fail_at=2U;assert(plan_app_receive(&app,raw,6U));plan_app_poll(&app,0U,0U,0U,1U);
    assert(!runtime.storage_ready && !app.saved && !app.due_count && app.rejected==1U);old=writes;
    plan_app_poll(&app,1U,0U,0U,1U);assert(writes==old);
    puts("PASS second-replica failure cannot report a successful save or start and does not auto-retry Flash");
    fresh();set_one();sync_at(15,24,50,10U);plan_app_poll(&app,10U,0U,0U,1U);plan_app_poll(&app,10010U,0U,0U,1U);
    assert(runtime.data.active);ProductRuntime reboot;io=storage();
    assert(product_runtime_init(&reboot,&io,0U,86400000U,60000U)==PRODUCT_STORE_OK);
    assert(reboot.data.count==1U && !reboot.data.active && reboot.data.result==PRODUCT_RESULT_INTERRUPTED && !reboot.clock.valid);
    puts("PASS reboot retains saved plans, marks unfinished task interrupted and never trusts saved clock or resumes motor");
    fresh();assert(!plan_app_time_due(&app,0U,1U));assert(!plan_app_time_due(&app,4999U,1U));
    assert(plan_app_time_due(&app,5000U,1U));plan_app_time_requested(&app,5000U);
    assert(!plan_app_time_due(&app,19999U,1U));assert(plan_app_time_due(&app,20000U,1U));plan_app_time_requested(&app,20000U);
    plan_app_time_requested(&app,35000U);assert(!plan_app_time_due(&app,50000U,1U));assert(plan_app_time_due(&app,335000U,1U));
    assert(!plan_app_time_due(&app,335001U,0U));assert(!plan_app_time_due(&app,335002U,1U));
    puts("PASS official first-online5s delay, spaced retries with backoff and reconnect delay avoid serial query flooding");
    return 0;
}
