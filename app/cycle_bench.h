#ifndef FEEDER_CYCLE_BENCH_H
#define FEEDER_CYCLE_BENCH_H
#include "feed_cycle.h"
#include "input_observer.h"

typedef struct {
    InputObserver input;
    FeedCycle cycle;
    uint32_t selected, keys_ready, last_buttons, rejected;
} CycleBench;
void cycle_bench_init(CycleBench *bench, uint32_t now);
/* levels：bit0=CN6/PF3，bit1=TOP1，bit2=TOP2。三个输入都是低电平有效。 */
void cycle_bench_step(CycleBench *bench, uint32_t now, uint32_t levels);
void cycle_bench_frame(const CycleBench *bench, uint8_t frame[7]);
#ifdef FEEDER_CYCLE_NET
int cycle_bench_start_external(CycleBench *bench, uint32_t now, uint32_t portions);
void cycle_bench_stop_external(CycleBench *bench, uint32_t now);
#endif
#ifdef FEEDER_CYCLE_MANUAL_DP
int cycle_bench_select_external(CycleBench *bench, uint32_t now, uint32_t portions);
#endif
#endif
