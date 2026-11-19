#include "cycle_bench.h"
#include <assert.h>
#include <stdio.h>

static CycleBench bench;
static uint32_t now;
static void hold(uint32_t levels,uint32_t duration)
{
    while (duration--) { cycle_bench_step(&bench,++now,levels); }
}
static void fresh(uint32_t levels)
{
    now=0U;
    cycle_bench_init(&bench,now);
    hold(levels,40U);
}

int main(void)
{
    /* 位序 TOP2, TOP1, CN6；0 表示对应的键或开关被按下。 */
    fresh(6U);
    hold(4U,110U);hold(6U,40U);
    assert(bench.cycle.state==FEED_LEAVE && bench.cycle.completed==0U);
    hold(7U,40U);
    assert(bench.cycle.state==FEED_RETURN && bench.cycle.completed==0U);
    hold(6U,40U);
    assert(bench.cycle.state==FEED_DONE && bench.cycle.completed==1U && !bench.cycle.motor_request);

    fresh(6U);
    hold(2U,40U);hold(6U,40U);
    assert(bench.selected==2U);
    hold(4U,110U);hold(6U,40U);hold(7U,40U);hold(6U,40U);
    assert(bench.cycle.state==FEED_LEAVE && bench.cycle.completed==1U && bench.cycle.motor_request);
    hold(6U,200U);
    assert(bench.cycle.completed==1U);
    hold(7U,40U);hold(6U,40U);
    assert(bench.cycle.state==FEED_DONE && bench.cycle.completed==2U && !bench.cycle.motor_request);
    uint8_t frame[7];
    cycle_bench_frame(&bench,frame);
    assert(frame[0]==8U && frame[1]==0x3FU && frame[2]==0x5BU && frame[3]==0x3FU && frame[4]==0x5BU);

    fresh(6U);
    hold(4U,110U);hold(6U,40U);hold(7U,40U);
    hold(5U,40U);hold(7U,40U);hold(6U,40U);
    assert(bench.cycle.state==FEED_CANCELLED && bench.cycle.completed==0U && !bench.cycle.motor_request);

    fresh(6U);
    hold(4U,110U);hold(6U,40U);hold(7U,12100U);
    assert(bench.cycle.state==FEED_FAULT && bench.cycle.fault==FEED_CYCLE_TIMEOUT &&
           bench.cycle.completed==0U && !bench.cycle.motor_request);
    hold(6U,40U);
    assert(bench.cycle.state==FEED_FAULT);

    fresh(4U); /* 上电时TOP1已经被压住，不启动。 */
    hold(4U,200U);
    assert(!bench.cycle.motor_request && !bench.keys_ready);
    hold(7U,40U);hold(5U,40U);hold(7U,40U);
    assert(bench.rejected==1U && !bench.cycle.motor_request); /* 松开CN6时请求，不自动补启动。 */
    hold(6U,40U);
    assert(!bench.cycle.motor_request);
    puts("PASS cycle bench: real debounce path, 1 and 2 cycles, display, stop, timeout, boot and away rejection");
}
