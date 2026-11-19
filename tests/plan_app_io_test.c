/* 复用上一版真实UART/驱动模型回归，再接真实plan_diag与存储适配。
 * VirtualAlloc只在独立测试进程映射模拟Flash地址，不访问硬件。 */
#define main inherited_ui_main
#include "local_controls_io_test.c"
#undef main
#include "plan_diag.h"
#include "product_flash_layout.h"
#include "product_runtime.h"
#include <windows.h>
#include <string.h>
static unsigned flash_writes,raw_reports,last_raw_size,time_queries;
static uint8_t last_raw[60];
int product_flash_write(uint32_t address,const uint32_t *data)
{
    assert(!local_controls_io_running() && !output());
    assert(address>=PRODUCT_FLASH_BASE && address<PRODUCT_FLASH_END && !(address&127U));
    memcpy((void *)(uintptr_t)address,data,128U);flash_writes++;return 1;
}
static void actual_drain(void)
{
    const WbrInitPacket *p;
    while((p=wbr_init_peek(&link))) {
        if(p->data[3]==0x1CU) { time_queries++; }
        if(p->data[3]==7U) {
            unsigned cursor=6U,end=p->length-1U;
            while(cursor+4U<=end) {
                unsigned size=((unsigned)p->data[cursor+2U]<<8U)|p->data[cursor+3U];
                assert(cursor+4U+size<=end);
                if(p->data[cursor]==1U) {
                    assert(size<=60U && p->length<=72U);last_raw_size=size;
                    memcpy(last_raw,p->data+cursor+4U,size);raw_reports++;
                }
                cursor+=4U+size;
            }
            assert(cursor==end);
        }
        wbr_init_sent(&link);
    }
    assert(!link.locked);
}
static void advance(unsigned count,unsigned levels)
{
    touch_gpiob.IDR=levels;
    for(unsigned i=0;i<count;i++) {
        local_controls_io_poll(++now,0U);
        touch_gpioa.ODR=(touch_gpioa.ODR&~(touch_gpioa.BSRR>>16U))|(touch_gpioa.BSRR&0xFFFFU);
        plan_diag_poll(now,0U);actual_drain();
    }
}
static void reset_board(void)
{
    memset((void *)(uintptr_t)0x08000000U,0xFFU,0x10000U);
    touch_gpioa.ODR=0U;flash_writes=raw_reports=last_raw_size=time_queries=0U;
    plan_diag_init(0U);fresh();link.network_state=4U;advance(5U,0xC0U);
}
static void wire_send(unsigned command,const uint8_t *data,unsigned size)
{
    uint8_t frame[80]={0x55,0xAA,0,0,0,0},sum=0;frame[3]=(uint8_t)command;frame[5]=(uint8_t)size;
    memcpy(frame+6U,data,size);
    for(unsigned i=0;i<size+6U;i++) { sum+=frame[i]; }
    frame[size+6U]=sum;
    for(unsigned i=0;i<size+7U;i++) { wbr_init_feed(&link,frame[i],0U,now); }
}
static void wire_plan(const uint8_t *raw,unsigned size)
{
    uint8_t payload[64]={1,0,0,0};payload[3]=(uint8_t)size;memcpy(payload+4U,raw,size);
    wire_send(6U,payload,size+4U);
}
int main(void)
{
    void *memory=VirtualAlloc((void *)(uintptr_t)0x08000000U,0x10000U,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    assert(memory==(void *)(uintptr_t)0x08000000U);memset(memory,0xFFU,0x10000U);plan_diag_init(0U);
    assert(!inherited_ui_main());
    puts("PASS inherited0.14 actual UART/local/lock/quick/reversal tests also run with new plan feature enabled");
    reset_board();const uint8_t captured[]={0,127,15,25,1,1};unsigned old=flash_writes;
    wire_plan(captured,6U);assert(flash_writes==old);advance(5U,0xC0U);
    assert(flash_writes==old+2U && last_raw_size==6U && !memcmp(last_raw,captured,6U) && !output());
    puts("PASS real UART DP1 handler stages then actual BSP persists two replicas and reports exact saved Raw to App");
    unsigned saved_writes=flash_writes;wire_plan(captured,6U);advance(5U,0xC0U);assert(flash_writes==saved_writes);
    wire_plan(captured,0U);advance(5U,0xC0U);
#ifdef FEEDER_PLAN_APP_EMPTY
    assert(last_raw_size==1U && last_raw[0]==0U && flash_writes==saved_writes+2U);
    wire_plan(captured,6U);advance(5U,0xC0U);unsigned before_delete=flash_writes;
    /* 完整实物删除帧55aa0006000501000001000c，经实际UART解析器进入保存路径。 */
    const uint8_t deleted[]={0};wire_plan(deleted,1U);
    assert(flash_writes==before_delete && last_raw_size==6U);
    advance(5U,0xC0U);assert(last_raw_size==1U && last_raw[0]==0U && flash_writes==before_delete+2U);
    wire_plan(deleted,1U);advance(5U,0xC0U);assert(flash_writes==before_delete+2U);
    const uint8_t invalid[]={1};wire_plan(invalid,1U);advance(5U,0xC0U);
    assert(flash_writes==before_delete+2U && last_raw_size==1U && last_raw[0]==0U);
    plan_diag_init(now);advance(5U,0xC0U);assert(last_raw_size==1U && last_raw[0]==0U);
    puts("PASS captured delete persists empty table before report, deduplicates, rejects bad short packet and reloads empty after reboot model");
#else
    assert(last_raw_size==0U && flash_writes==saved_writes+2U);
#endif
    puts("PASS actual adapter deduplicates identical saves and reports deletion only after mirrored empty-table commit");
    uint8_t all[60]={0};unsigned total;
#ifdef FEEDER_PLAN_APP_TABLE
    total=51U;for(unsigned i=0;i<10U;i++) { memcpy(all+1U+5U*i,captured+1U,5U);all[2U+i*5U]=(uint8_t)i; }
    const uint8_t two[]={0,32,16,44,1,1,32,16,53,1,1};
    wire_plan(two,sizeof(two));advance(5U,0xC0U);assert(last_raw_size==sizeof(two) && !memcmp(last_raw,two,sizeof(two)));
    plan_diag_init(now);advance(5U,0xC0U);assert(last_raw_size==sizeof(two) && !memcmp(last_raw,two,sizeof(two)));
    wire_plan(captured,6U);advance(5U,0xC0U);assert(last_raw_size==6U && !memcmp(last_raw,captured,6U));
    puts("PASS actual two-plan frame persists and reloads identical table; deleting one preserves remaining plan");
#else
    total=60U;for(unsigned i=0;i<10U;i++) { memcpy(all+6U*i,captured,6U);all[i*6U+2U]=(uint8_t)i; }
#endif
    wire_plan(all,total);advance(5U,0xC0U);assert(last_raw_size==total && !memcmp(last_raw,all,total));
    puts("PASS ten-plan actual full-table report fits72-byte queue without reducing queue depth");
    unsigned reports=raw_reports;wire_send(8U,(const uint8_t *)"",0U);advance(1000U,0xC0U);
    assert(raw_reports==reports+1U && !link.app_report_request && !link.count);
    puts("PASS one module status query reports plans once and still drains motor-state report without serial starvation");
    reset_board();advance(6000U,0xC0U);assert(time_queries);
    const uint8_t civil[]={1,26,11,17,15,24,50,2};wire_send(0x1CU,civil,8U);advance(2U,0xC0U);
    const uint8_t *display=local_controls_display();assert(display[1]==0x06U && display[2]==0x6DU && display[3]==0x5BU && display[4]==0x66U);
    puts("PASS real0x1c UART request/reply dispatch updates local clock and idle display to1524");
    advance(150U,0x80U);advance(150U,0xC0U);assert(local_controls_display()[4]==0x5BU);
    advance(5100U,0xC0U);assert(local_controls_display()[1]==0x06U && local_controls_display()[2]==0x6DU);
    puts("PASS quantity selection remains visible then returns to real idle time without erasing local UI behavior");
    reset_board();wire_plan(captured,6U);advance(5U,0xC0U);wire_send(0x1CU,civil,8U);
    advance(10003U,0xC0U);assert(output()==0x100U);old=flash_writes;
    advance(1000U,0xC0U);assert(output()==0x100U && flash_writes==old);
    dp(101U,1U);assert(output()==0x100U);
    advance(6600U,0xC0U);assert(!output() && flash_writes==old+2U);
    puts("PASS real saved plan drives bounded GPIO demo despite duplicate continuous-start, stops before result Flash commit");
    reset_board();dp(101U,1U);assert(output());old=flash_writes;wire_plan(captured,6U);
    advance(100U,0xC0U);assert(output() && flash_writes==old);
    dp(101U,0U);advance(5U,0xC0U);assert(!output() && flash_writes==old+2U && last_raw_size==6U);
    puts("PASS real App plan update during manual continuous motor run defers Flash and preserves current movement");
    reset_board();uint8_t mixed[]={1,0,0,6,0,127,15,25,1,1,101,1,0,1,1};old=flash_writes;
    wire_send(6U,mixed,sizeof(mixed));advance(5U,0xC0U);assert(!output() && flash_writes==old && link.malformed);
    puts("PASS mixed plan/start payload rejected atomically through real parser without save or motion");
    assert(VirtualFree(memory,0U,MEM_RELEASE));return 0;
}
