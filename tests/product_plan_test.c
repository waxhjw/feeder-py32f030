#include "product_runtime.h"
#include "feed_cycle.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
/* 这里只模拟Flash故障与业务时间，不访问芯片；所有机构参数都是测试值。 */
typedef struct {
    uint8_t page[PRODUCT_STORE_SLOTS][PRODUCT_STORE_PAGE];
    int budget,off,read_bad;unsigned writes,reads,erases[PRODUCT_STORE_SLOTS];ProductAction *watch;
} Memory;
static int read_page(void *ctx,uint32_t slot,uint8_t *out)
{
    Memory *m=ctx;assert(slot<PRODUCT_STORE_SLOTS);m->reads++;
    if(m->off || m->read_bad) { return 0; }
    memcpy(out,m->page[slot],PRODUCT_STORE_PAGE);return 1;
}
static int primitive(Memory *m)
{
    if(m->watch) { assert(m->watch->valid==0U); }
    if(m->off || m->budget==0) { m->off=1;return 0; }
    if(m->budget>0) { m->budget--; }return 1;
}
static int write_page(void *ctx,uint32_t slot,const uint8_t *in)
{
    Memory *m=ctx;assert(slot<PRODUCT_STORE_SLOTS);m->writes++;m->erases[slot]++;
    for(uint32_t i=0;i<PRODUCT_STORE_PAGE;i++) { if(primitive(m)==0) { return 0; }m->page[slot][i]=0xFFU; }
    for(uint32_t i=0;i<PRODUCT_STORE_PAGE;i++) {
        if(primitive(m)==0) { return 0; }
        assert((m->page[slot][i]&in[i])==in[i]);m->page[slot][i]&=in[i];
    }
    if(m->budget==0) { m->off=1;return 0; } /* 最后一个字节写完但尚未确认也可能掉电。 */
    return 1;
}
static void blank(Memory *m)
{
    memset(m,0,sizeof(*m));memset(m->page,0xFFU,sizeof(m->page));m->budget=-1;
}
static ProductStorage io(Memory *m)
{
    ProductStorage result={m,read_page,write_page};return result;
}
static MealPlan plan(unsigned hour,unsigned minute,unsigned portions)
{
    MealPlan p={1U,0x7FU,(uint8_t)hour,(uint8_t)minute,(uint8_t)portions,{0}};return p;
}
static MealCivil civil(unsigned hour,unsigned minute,unsigned second)
{
    MealCivil t={2000U,1U,1U,hour,minute,second,5U};return t;
}
static void base(ProductRuntime *s,Memory *m)
{
    blank(m);ProductStorage storage=io(m);assert(product_runtime_init(s,&storage,0U,172800000U,60000U)==PRODUCT_STORE_EMPTY);
    MealPlan p=plan(8U,0U,2U);assert(product_runtime_set_plans(s,0U,&p,1U));
    MealCivil t=civil(7U,59U,59U);assert(meal_clock_sync(&s->clock,0U,&t));
    ProductAction action;MealEvent event=product_runtime_poll(s,0U,MEAL_REASON_NONE,&action);
    assert(event.kind==MEAL_BASELINE && action.valid==0U);
}
static void clock_tests(void)
{
    MealClock c;MealCivil t=civil(23U,59U,59U);meal_clock_init(&c,0U,10000U,2000U);
    assert(c.valid==0U && meal_clock_sync(&c,0U,&t));
    meal_clock_poll(&c,999U);assert(c.second==59U && c.fraction_ms==999U);
    meal_clock_poll(&c,1000U);assert(c.minute==1440U && c.second==0U);
    puts("PASS cold clock invalid, fractions accumulate and midnight advances without network");
    uint32_t before=c.minute;
    t.year=2001U;t.month=2U;t.day=29U;assert(meal_clock_sync(&c,1000U,&t)==0 && c.minute==before);
    t.year=2000U;t.month=2U;t.day=29U;t.hour=0U;t.minute=0U;t.second=0U;t.weekday=1U;
    assert(meal_clock_sync(&c,1000U,&t));meal_clock_poll(&c,2000U);assert(c.second==1U);
    t.weekday=0U;assert(meal_clock_sync(&c,2000U,&t)==0);
    t.year=2100U;assert(meal_clock_sync(&c,2000U,&t)==0);
    puts("PASS leap date, calendar weekday and supported year range validated independently");
    t=civil(0U,0U,0U);meal_clock_init(&c,0U,1000U,1000U);assert(meal_clock_sync(&c,0U,&t));
    meal_clock_poll(&c,1000U);assert(c.valid==0U);
    meal_clock_init(&c,0U,10000U,100U);assert(meal_clock_sync(&c,0U,&t));meal_clock_poll(&c,101U);assert(c.valid==0U);
    meal_clock_init(&c,100U,10000U,100U);assert(meal_clock_sync(&c,100U,&t));meal_clock_poll(&c,99U);assert(c.valid==0U);
    puts("PASS holdover expiry, polling gap and reversed uptime invalidate time");
    meal_clock_init(&c,0xFFFFFF00U,10000U,2000U);assert(meal_clock_sync(&c,0xFFFFFF00U,&t));
    meal_clock_poll(&c,744U);assert(c.valid && c.second==1U);
    puts("PASS uint32 uptime wrap preserves clock progress");
    t.year=2099U;t.month=12U;t.day=31U;t.hour=23U;t.minute=59U;t.second=59U;t.weekday=3U;
    meal_clock_init(&c,0U,10000U,2000U);assert(meal_clock_sync(&c,0U,&t));meal_clock_poll(&c,1000U);assert(c.valid==0U);
    puts("PASS clock stops at end of supported calendar instead of wrapping to old dates");
}
static void scheduler_tests(void)
{
    MealPlan plans[MEAL_PLAN_LIMIT];for(unsigned i=0;i<10U;i++) { plans[i]=plan(i,0U,i+1U); }
    assert(meal_plans_valid(plans,10U));assert(meal_plans_valid(plans,11U)==0);
    plans[1]=plans[0];assert(meal_plans_valid(plans,10U)==0);plans[1].enabled=0U;assert(meal_plans_valid(plans,10U));
    plans[0].days=0U;assert(meal_plans_valid(plans,10U)==0);
    puts("PASS ten-plan limit and overlapping enabled times validated atomically");
    MealScheduler s;MealClock c={0};MealPlan p=plan(8U,0U,2U);MealEvent e;
    meal_scheduler_init(&s,MEAL_TIME_NONE);c.minute=479U;c.second=59U;c.generation=1U;
    e=meal_scheduler_poll(&s,&c,&p,1U,0U);assert(e.kind==MEAL_NONE);
    c.valid=1U;e=meal_scheduler_poll(&s,&c,&p,1U,0U);assert(e.kind==MEAL_BASELINE);
    c.minute=480U;c.second=0U;e=meal_scheduler_poll(&s,&c,&p,1U,0U);assert(e.kind==MEAL_RUN && e.portions==2U);
    e=meal_scheduler_poll(&s,&c,&p,1U,0U);assert(e.kind==MEAL_NONE);
    puts("PASS invalid time suppressed, natural minute boundary emits exactly one candidate");
    for(unsigned why=MEAL_REASON_BUSY;why<=MEAL_REASON_NOT_READY;why++) {
        meal_scheduler_init(&s,MEAL_TIME_NONE);c.minute=479U;c.second=59U;
        meal_scheduler_poll(&s,&c,&p,1U,0U);c.minute=480U;c.second=0U;
        e=meal_scheduler_poll(&s,&c,&p,1U,why);assert(e.kind==MEAL_SKIP && e.reason==why);
        e=meal_scheduler_poll(&s,&c,&p,1U,0U);assert(e.kind==MEAL_NONE);
    }
    puts("PASS busy, upgrade and unavailable executor consume due minute without replay");
    meal_scheduler_init(&s,MEAL_TIME_NONE);c.minute=479U;c.second=59U;meal_scheduler_poll(&s,&c,&p,1U,0U);
    c.minute=480U;c.second=1U;e=meal_scheduler_poll(&s,&c,&p,1U,0U);assert(e.kind==MEAL_SKIP && e.reason==MEAL_REASON_LATE);
    c.second=0U;e=meal_scheduler_poll(&s,&c,&p,1U,0U);assert(e.kind==MEAL_NONE);
    puts("PASS missed zero-second window skipped, clock correction cannot replay that minute");
    meal_scheduler_init(&s,MEAL_TIME_NONE);c.minute=478U;meal_scheduler_poll(&s,&c,&p,1U,0U);
    c.minute=480U;e=meal_scheduler_poll(&s,&c,&p,1U,0U);assert(e.kind==MEAL_BASELINE);
    c.minute=479U;meal_scheduler_poll(&s,&c,&p,1U,0U);c.minute=480U;
    assert(meal_scheduler_poll(&s,&c,&p,1U,0U).kind==MEAL_NONE);
    puts("PASS forward jump skips catch-up and backward correction does not replay");
    meal_scheduler_init(&s,MEAL_TIME_NONE);c.minute=479U;meal_scheduler_poll(&s,&c,&p,1U,0U);
    c.minute=480U;c.generation++;assert(meal_scheduler_poll(&s,&c,&p,1U,0U).kind==MEAL_BASELINE);
    c.valid=0U;meal_scheduler_poll(&s,&c,&p,1U,0U);c.valid=1U;
    assert(meal_scheduler_poll(&s,&c,&p,1U,0U).kind==MEAL_BASELINE);
    puts("PASS explicit sync and restored time only establish baseline, never immediately feed");
    meal_scheduler_init(&s,MEAL_TIME_NONE);p.days=1U;c.minute=479U;meal_scheduler_poll(&s,&c,&p,1U,0U);c.minute=480U;
    assert(meal_scheduler_poll(&s,&c,&p,1U,0U).kind==MEAL_NONE);
    p.days=0x20U;meal_scheduler_init(&s,MEAL_TIME_NONE);c.minute=479U;meal_scheduler_poll(&s,&c,&p,1U,0U);c.minute=480U;
    assert(meal_scheduler_poll(&s,&c,&p,1U,0U).kind==MEAL_RUN);
    puts("PASS Monday-based weekday mask selects actual date rather than every day");
}
static void storage_tests(const char *fixture)
{
    Memory m;blank(&m);ProductStorage storage=io(&m);ProductData d,loaded;uint32_t seq,slot;
    assert(product_store_load(&storage,&loaded,&seq,&slot)==PRODUCT_STORE_EMPTY);
    product_data_empty(&d);d.count=1U;d.plans[0]=plan(8U,0U,2U);d.watermark=12345U;
    assert(product_store_save(&storage,&d)==PRODUCT_STORE_OK && m.writes==2U);
    unsigned writes=m.writes;assert(product_store_save(&storage,&d)==PRODUCT_STORE_OK && m.writes==writes);
    assert(product_store_load(&storage,&loaded,&seq,&slot)==PRODUCT_STORE_OK && memcmp(&d,&loaded,sizeof(d))==0);
    puts("PASS mirrored record roundtrip and identical-data save avoids extra erases");
    ProductStorage readonly=storage;readonly.write=0;
    assert(product_store_load(&readonly,&loaded,&seq,&slot)==PRODUCT_STORE_OK);
    assert(product_store_save(&readonly,&d)==PRODUCT_STORE_IO && m.writes==writes);
    puts("PASS read-only inspection needs no write callback, save rejects missing writer");
    ProductData stale=d;stale.watermark--;writes=m.writes;
    assert(product_store_save(&storage,&stale)==PRODUCT_STORE_CORRUPT && m.writes==writes);
    puts("PASS durable processed-time watermark cannot roll backward through a stale writer");
    d.watermark++;assert(product_store_save(&storage,&d)==PRODUCT_STORE_OK);
    m.page[2][32]^=1U;assert(product_store_load(&storage,&loaded,&seq,&slot)==PRODUCT_STORE_OK && loaded.watermark==d.watermark);
    m.page[3][32]^=1U;assert(product_store_load(&storage,&loaded,&seq,&slot)==PRODUCT_STORE_CORRUPT);
    puts("PASS one damaged latest copy recovered, both damaged latest copies latch corruption");
    blank(&m);m.page[0][0]=0U;assert(product_store_load(&storage,&loaded,&seq,&slot)==PRODUCT_STORE_CORRUPT);
    m.read_bad=1;assert(product_store_load(&storage,&loaded,&seq,&slot)==PRODUCT_STORE_IO);
    puts("PASS corrupt nonblank store and read failure are not silently reset to factory defaults");
    blank(&m);d.watermark=12345U;
    for(unsigned i=0;i<35U;i++) { d.watermark=12345U+i;assert(product_store_save(&storage,&d)==PRODUCT_STORE_OK); }
    assert(product_store_load(&storage,&loaded,&seq,&slot)==PRODUCT_STORE_OK && seq==35U && loaded.watermark==12379U);
    for(unsigned i=0;i<PRODUCT_STORE_SLOTS;i++) { assert(m.erases[i]>=2U && m.erases[i]<=3U); }
    FILE *file=fopen(fixture,"wb");assert(file);assert(fwrite(m.page,1U,sizeof(m.page),file)==sizeof(m.page));fclose(file);
    puts("PASS sixteen mirrored groups rotate safely beyond journal wrap, fixture exported");
}
static void runtime_tests(void)
{
    Memory m;ProductRuntime r;ProductAction action;MealEvent event;base(&r,&m);
    unsigned writes=m.writes;m.watch=&action;
    event=product_runtime_poll(&r,1000U,0U,&action);m.watch=0;
    assert(event.kind==MEAL_RUN && action.valid && action.portions==2U && m.writes==writes+2U && r.data.active);
    ProductStorage storage=io(&m);ProductData loaded;uint32_t seq,slot;
    assert(product_store_load(&storage,&loaded,&seq,&slot)==PRODUCT_STORE_OK && loaded.active && loaded.watermark==480U);
    puts("PASS run request published only after durable watermark and pending-task mirror commit");
    ProductAction duplicate;assert(product_runtime_manual(&r,1U,1U,&duplicate)==0 && duplicate.valid==0U);
    MealPlan replacement=plan(9U,0U,5U);assert(product_runtime_set_plans(&r,1000U,&replacement,1U));
    assert(r.data.target==2U && r.data.source==PRODUCT_SOURCE_PLAN && r.data.active);
    assert(product_runtime_finish(&r,PRODUCT_RESULT_DONE,1U)==0);
    assert(product_runtime_finish(&r,PRODUCT_RESULT_CANCELLED,1U) && r.data.completed==1U && r.data.active==0U);
    puts("PASS active task blocks duplicates, plan edits preserve running quantity, partial not completed");
    base(&r,&m);assert(product_runtime_manual(&r,1U,1U,&action));
    ProductRuntime restarted;storage=io(&m);assert(product_runtime_init(&restarted,&storage,0U,172800000U,60000U)==PRODUCT_STORE_OK);
    assert(restarted.recovered_interruption && restarted.data.result==PRODUCT_RESULT_INTERRUPTED && restarted.data.active==0U && restarted.clock.valid==0U);
    event=product_runtime_poll(&restarted,1000U,0U,&action);assert(action.valid==0U && event.kind==MEAL_NONE);
    puts("PASS reboot restores plans but invalidates clock and records interruption without resuming task");
    base(&r,&m);event=product_runtime_poll(&r,1000U,MEAL_REASON_UPGRADE,&action);assert(event.kind==MEAL_SKIP && action.valid==0U);
    storage=io(&m);assert(product_store_load(&storage,&loaded,&seq,&slot)==PRODUCT_STORE_OK);
    assert(loaded.skip_reason==MEAL_REASON_UPGRADE && loaded.skip_minute==480U && loaded.skip_index==0U);
    MealCivil time=civil(7U,59U,59U);assert(meal_clock_sync(&r.clock,1000U,&time));product_runtime_poll(&r,1000U,0U,&action);
    event=product_runtime_poll(&r,2000U,0U,&action);assert(action.valid==0U && event.kind==MEAL_NONE);
    puts("PASS upgrade-skipped occurrence durably consumed even after backward time sync");
    base(&r,&m);MealPlan clash[2]={plan(8U,0U,1U),plan(8U,0U,2U)};writes=m.writes;
    assert(product_runtime_set_plans(&r,0U,clash,2U)==0 && m.writes==writes && r.data.count==1U);
    MealPlan same=plan(8U,0U,2U);assert(product_runtime_set_plans(&r,0U,&same,1U) && m.writes==writes);
    product_runtime_poll(&r,1000U,MEAL_REASON_NOT_READY,&action);writes=m.writes;
    for(unsigned now=2000U;now<=120000U;now+=1000U) { product_runtime_poll(&r,now,0U,&action);assert(action.valid==0U); }
    assert(m.writes==writes);
    puts("PASS invalid edit atomic, identical edit no wear, idle clock ticks never write every minute");
    base(&r,&m);m.budget=0;event=product_runtime_poll(&r,1000U,0U,&action);
    assert(event.reason==MEAL_REASON_STORAGE && action.valid==0U && r.storage_ready==0U);
    assert(product_runtime_manual(&r,1U,1U,&action)==0);
    puts("PASS storage failure blocks automatic and manual execution requests");
    const uint32_t invalid_reasons[]={MEAL_REASON_TIME,MEAL_REASON_LATE,MEAL_REASON_STORAGE,0xFFFFFFFFU};
    for(unsigned i=0U;i<sizeof(invalid_reasons)/sizeof(invalid_reasons[0]);i++) {
        base(&r,&m);event=product_runtime_poll(&r,1000U,invalid_reasons[i],&action);
        assert(event.kind==MEAL_SKIP && event.reason==MEAL_REASON_NOT_READY && action.valid==0U);
        assert(r.storage_ready==1U && r.storage_error==0U && r.data.skip_reason==MEAL_REASON_NOT_READY);
    }
    puts("PASS invalid executor-state input skips safely without inventing a storage failure");
}
static void interruption_matrix(void)
{
    /* 先建立正常基准；每次只改变写入在哪一个字节动作后中断。 */
    ProductRuntime baseline;Memory base_memory;base(&baseline,&base_memory);
    for(int cut=0;cut<=513;cut++) {
        Memory trial=base_memory;trial.budget=cut;ProductRuntime live=baseline;live.storage=io(&trial);
        ProductAction action;trial.watch=&action;MealEvent e=product_runtime_poll(&live,1000U,0U,&action);trial.watch=0;
        if(cut<=512) { assert(action.valid==0U && e.kind!=MEAL_RUN); }
        else { assert(action.valid==1U && e.kind==MEAL_RUN); }
        trial.off=0;trial.budget=-1;ProductStorage storage=io(&trial);ProductRuntime boot;
        uint32_t status=product_runtime_init(&boot,&storage,0U,172800000U,60000U);
        assert(status==PRODUCT_STORE_OK || status==PRODUCT_STORE_CORRUPT);
        assert(boot.clock.valid==0U && boot.data.active==0U);
        product_runtime_poll(&boot,0U,0U,&action);assert(action.valid==0U);
        MealCivil t=civil(8U,0U,0U);assert(meal_clock_sync(&boot.clock,0U,&t));product_runtime_poll(&boot,0U,0U,&action);assert(action.valid==0U);
        t=civil(7U,59U,59U);assert(meal_clock_sync(&boot.clock,0U,&t));product_runtime_poll(&boot,0U,0U,&action);
        product_runtime_poll(&boot,1000U,0U,&action);assert(action.valid==0U);
    }
    puts("PASS 514 byte-level cut points across erase/program/ack cannot emit uncommitted run or replay on reboot");
}
static void execution_bridge(void)
{
    Memory memory;ProductRuntime runtime;ProductAction action;base(&runtime,&memory);
    product_runtime_poll(&runtime,1000U,0U,&action);assert(action.valid && action.portions==2U);
    FeedProfile profile={1U,0U,12U,80U,200U,800U,40U,20U,20U,20U};FeedCycle motor;
    FeedObservation input={1U,0U,1000U,FEED_PROTECT_OK,1000U};feed_cycle_init(&motor,&profile,1000U);
    assert(feed_cycle_step(&motor,1000U,&input,FEED_ARM,0U)==FEED_ACCEPTED);
    assert(feed_cycle_step(&motor,1000U,&input,FEED_START,action.portions)==FEED_ACCEPTED);
    for(unsigned n=1U;n<=10U;n++) {
        input.position_ms=input.protection_ms=1000U+n*10U;input.position_level=(n%5U==0U)?0U:1U;
        feed_cycle_step(&motor,input.position_ms,&input,0U,0U);
    }
    assert(motor.state==FEED_DONE && motor.completed==2U && motor.motor_request==0U);
    assert(product_runtime_finish(&runtime,PRODUCT_RESULT_DONE,motor.completed));
    assert(runtime.data.completed==2U && runtime.data.result==PRODUCT_RESULT_DONE && runtime.data.active==0U);
    puts("PASS actual cycle core and scheduler/store bridge preserves durable start and verified completion");
}
static void edit_finish_cut_matrix(void)
{
    ProductRuntime baseline;Memory base_memory;base(&baseline,&base_memory);
    for(unsigned stage=0;stage<2U;stage++) {
        ProductRuntime before=baseline;Memory saved=base_memory;before.storage=io(&saved);ProductAction action;
        if(stage==1U) { assert(product_runtime_manual(&before,2U,1U,&action)); }
        for(int cut=0;cut<=513;cut++) {
            Memory trial=saved;trial.budget=cut;ProductRuntime live=before;live.storage=io(&trial);
            if(stage==0U) { MealPlan replacement=plan(9U,0U,3U);product_runtime_set_plans(&live,0U,&replacement,1U); }
            else { product_runtime_finish(&live,PRODUCT_RESULT_CANCELLED,1U); }
            trial.off=0;trial.budget=-1;ProductStorage storage=io(&trial);ProductRuntime boot;
            uint32_t status=product_runtime_init(&boot,&storage,0U,172800000U,60000U);
            assert(status==PRODUCT_STORE_OK || status==PRODUCT_STORE_CORRUPT);
            assert(boot.data.active==0U && boot.clock.valid==0U);
            if(status==PRODUCT_STORE_OK) {
                assert(boot.data.count==1U);
                if(stage==0U) {
                    assert((boot.data.plans[0].hour==8U && boot.data.plans[0].portions==2U) ||
                           (boot.data.plans[0].hour==9U && boot.data.plans[0].portions==3U));
                } else {
                    assert(boot.data.result==PRODUCT_RESULT_INTERRUPTED || boot.data.result==PRODUCT_RESULT_CANCELLED);
                    assert(boot.data.completed<=1U);
                }
            }
            product_runtime_poll(&boot,0U,0U,&action);assert(action.valid==0U);
        }
    }
    puts("PASS 514 edit cut points recover whole old or new plan or latch fault, never a mixed plan");
    puts("PASS 514 result-save cut points preserve cancellation or interruption without false completion");
}
#ifdef FEEDER_STORE_CACHED
static void cached_tests(void)
{
    Memory m;blank(&m);ProductStorage storage=io(&m);ProductData previous,next,loaded;ProductCursor cursor;
    assert(product_store_open(&storage,&previous,&cursor)==PRODUCT_STORE_EMPTY);
    next=previous;next.count=1U;next.plans[0]=plan(8U,0U,2U);
    for(unsigned i=0U;i<35U;i++) {
        next.watermark=12345U+i;unsigned before=m.reads;
        assert(product_store_save_cursor(&storage,&previous,&next,&cursor)==PRODUCT_STORE_OK);
        assert(m.reads-before==(i==0U?2U:4U));previous=next;
    }
    uint32_t seq,slot;
    assert(product_store_load(&storage,&loaded,&seq,&slot)==PRODUCT_STORE_OK && seq==35U && memcmp(&loaded,&next,sizeof(next))==0);
    puts("PASS cursor rotates beyond full bank with constant four reads per commit, format unchanged");
    unsigned current=(cursor.slot/2U)*2U;m.page[current][32]^=1U;next.watermark++;
    assert(product_store_save_cursor(&storage,&previous,&next,&cursor)==PRODUCT_STORE_OK);previous=next;
    current=(cursor.slot/2U)*2U;m.page[current][32]^=1U;m.page[current+1U][32]^=1U;next.watermark++;
    assert(product_store_save_cursor(&storage,&previous,&next,&cursor)==PRODUCT_STORE_CORRUPT);
    unsigned writes=m.writes;
    assert(product_store_save_cursor(&storage,&previous,&next,&cursor)==PRODUCT_STORE_CORRUPT && m.writes==writes);
    puts("PASS cursor validates current replicas, tolerates one damaged copy and latches two-copy failure");
}
#endif
int main(int argc,char **argv)
{
    assert(argc==2);clock_tests();scheduler_tests();storage_tests(argv[1]);runtime_tests();interruption_matrix();execution_bridge();edit_finish_cut_matrix();
#ifdef FEEDER_STORE_CACHED
    cached_tests();
#endif
    return 0;
}
