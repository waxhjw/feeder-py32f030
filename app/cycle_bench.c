#include "cycle_bench.h"
#include <string.h>

void cycle_bench_init(CycleBench *bench, uint32_t now)
{
    /* 这是手按微动的测试模型，参数只用于给人留操作时间。没有实际驱动接口。 */
    const FeedProfile fixture = {
        .hardware_validated=1U, .home_level=0U, .max_portions=12U,
        .leave_timeout_ms=8000U, .cycle_timeout_ms=12000U,
        .task_timeout_ms=96000U, .min_cycle_ms=100U,
        .input_max_age_ms=50U, .protection_max_age_ms=50U, .max_poll_gap_ms=50U
    };
    memset(bench, 0, sizeof(*bench));
    bench->selected=1U;
    feed_cycle_init(&bench->cycle, &fixture, now);
}

void cycle_bench_step(CycleBench *bench, uint32_t now, uint32_t levels)
{
    input_observer_step(&bench->input, now, levels);
    const InputChannel *pins=bench->input.channel;
    uint32_t buttons=(1U-pins[1].stable)|((1U-pins[2].stable)<<1U);
    uint32_t down=0U;
    if (!pins[1].valid || !pins[2].valid) { bench->keys_ready=0U; }
    else if (!bench->keys_ready) { bench->keys_ready=buttons==0U; }
    else { down=buttons & ~bench->last_buttons; }
    bench->last_buttons=buttons;

    /* 核心需要一个保护输入。这里仅给没有接执行器的模型提供固定测试输入。 */
    const FeedObservation observation={
        .position_valid=pins[0].valid, .position_level=pins[0].stable,
        .position_ms=bench->input.last_ms, .protection=FEED_PROTECT_OK, .protection_ms=now
    };
    FeedCycle *cycle=&bench->cycle;
    uint32_t command=0U;
    if (cycle->motor_request) {
        if (down & 1U) { command=FEED_STOP; }
#ifndef FEEDER_CYCLE_NET
    } else if (buttons==1U && down==1U) {
        /* 单次明确触摸请求；未在模拟停位时 ARM 拒绝，不安排后续自动启动。 */
        if (feed_cycle_step(cycle,now,&observation,FEED_ARM,0U)==FEED_ACCEPTED) {
            command=FEED_START;
        } else {
            bench->rejected++;
            return;
        }
    } else if (buttons==2U && down==2U) {
        const FeedProfile profile=cycle->profile;
        bench->selected=bench->selected%12U+1U;
        feed_cycle_init(cycle,&profile,now);
#endif

    }
    if (feed_cycle_step(cycle,now,&observation,command,bench->selected)==FEED_REJECTED) {
        bench->rejected++;
    }
}

#ifdef FEEDER_CYCLE_NET
int cycle_bench_start_external(CycleBench *bench,uint32_t now,uint32_t portions)
{
    if (bench->cycle.motor_request || !bench->keys_ready || portions<1U || portions>12U) { return 0; }
    const FeedObservation in={.position_valid=bench->input.channel[0].valid,
        .position_level=bench->input.channel[0].stable,.position_ms=bench->input.last_ms,
        .protection=FEED_PROTECT_OK,.protection_ms=now}; /* 只为无电机模拟 */
    if (feed_cycle_step(&bench->cycle,now,&in,FEED_ARM,0U)!=FEED_ACCEPTED) { return 0; }
    return feed_cycle_step(&bench->cycle,now,&in,FEED_START,portions)==FEED_ACCEPTED;
}
void cycle_bench_stop_external(CycleBench *bench,uint32_t now)
{
    if (bench->cycle.motor_request) {
        /* 正常推进由采样循环处理，停止命令必须在下次计数前生效。 */
        const FeedObservation in={.position_valid=bench->input.channel[0].valid,
            .position_level=bench->input.channel[0].stable,.position_ms=bench->input.last_ms,
            .protection=FEED_PROTECT_OK,.protection_ms=now};
        (void)feed_cycle_step(&bench->cycle,now,&in,FEED_STOP,0U);
    }
}
#endif

#ifdef FEEDER_CYCLE_MANUAL_DP
int cycle_bench_select_external(CycleBench *bench,uint32_t now,uint32_t portions)
{
    if (bench->cycle.motor_request || portions<1U || portions>12U) { return 0; }
    const FeedProfile profile=bench->cycle.profile;
    bench->selected=portions;
    feed_cycle_init(&bench->cycle,&profile,now); /* 选份只回待机显示，不发ARM或START。 */
    return 1;
}
#endif

void cycle_bench_frame(const CycleBench *bench, uint8_t frame[7])
{
    static const uint8_t digits[]={0x3FU,0x06U,0x5BU,0x4FU,0x66U,0x6DU,0x7DU,0x07U,0x7FU,0x6FU};
    const FeedCycle *cycle=&bench->cycle;
    uint32_t target=bench->selected;
    memset(frame,0,7U);
    frame[0]=0x08U;
    if (cycle->motor_request || cycle->state==FEED_DONE) {
        frame[1]=digits[cycle->completed/10U];
        frame[2]=digits[cycle->completed%10U];
        target=cycle->target;
    } else if (cycle->state==FEED_CANCELLED || cycle->state==FEED_FAULT) {
        frame[1]=cycle->state==FEED_CANCELLED?0x6DU:0x79U; /* S / E */
        target=cycle->state==FEED_CANCELLED?cycle->completed:cycle->fault;
    } else {
        frame[1]=0x73U; /* P：所选份数；计数尚未开始。 */
    }
    frame[3]=digits[target/10U];
    frame[4]=digits[target%10U];
}
