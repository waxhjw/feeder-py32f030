#ifndef FEEDER_FEED_CYCLE_H
#define FEEDER_FEED_CYCLE_H
#include <stdint.h>
/* 纯逻辑，不写GPIO、不操作Flash。未取得真实机构参数时不得接入驱动。
 * 一周期假设：经验证的停位 -> 离开停位 -> 返回停位。
 * “周期完成”仅指该机械序列，不代表检测到粮食或固定克数。 */
enum { FEED_DISARMED, FEED_READY, FEED_LEAVE, FEED_RETURN, FEED_DONE, FEED_CANCELLED, FEED_FAULT };
enum { FEED_OK, FEED_BAD_PROFILE, FEED_POSITION_INVALID, FEED_POSITION_STALE,
       FEED_PROTECTION_INVALID, FEED_PROTECTION_TRIP, FEED_POLL_GAP,
       FEED_LEAVE_TIMEOUT, FEED_CYCLE_TIMEOUT, FEED_TASK_TIMEOUT, FEED_CYCLE_TOO_FAST };
enum { FEED_PROTECT_UNKNOWN, FEED_PROTECT_OK, FEED_PROTECT_TRIPPED };
enum { FEED_STOP=1U, FEED_ARM=2U, FEED_START=4U };
enum { FEED_NO_COMMAND, FEED_ACCEPTED, FEED_REJECTED, FEED_BUSY };
typedef struct {
    uint32_t hardware_validated,home_level,max_portions;
    uint32_t leave_timeout_ms,cycle_timeout_ms,task_timeout_ms,min_cycle_ms;
    uint32_t input_max_age_ms,protection_max_age_ms,max_poll_gap_ms;
} FeedProfile;
typedef struct {
    /* 必须传入去抖后的有效电平。position_ms是最近采样时间，不是最后边沿时间。 */
    uint32_t position_valid,position_level,position_ms;
    /* 未来由独立保护模块提供；ADC原始valid不能直接冒充保护已就绪。 */
    uint32_t protection,protection_ms;
} FeedObservation;
typedef struct {
    FeedProfile profile;
    uint32_t state,fault,motor_request,target,completed,partial,position_trusted;
    uint32_t task_started,cycle_started,last_step;
} FeedCycle;
void feed_cycle_init(FeedCycle *s,const FeedProfile *profile,uint32_t now);
/* 同一轮的请求一起提交：STOP优先，不排队；ARM与START必须分开。
 * 只有motor_request表示逻辑运行请求，本模块没有实际驱动调用。 */
uint32_t feed_cycle_step(FeedCycle *s,uint32_t now,const FeedObservation *in,uint32_t commands,uint32_t portions);
#endif
