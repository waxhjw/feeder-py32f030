#ifndef FEEDER_MOTOR_BENCH_CONFIG_H
#define FEEDER_MOTOR_BENCH_CONFIG_H

#include "board_config.h"

/* 本文件只供人工点动验证版使用，原 0.1.0-env 入口保持不变。
 * 50ms 是初测限制，不是已测得的安全行程或出粮份量。
 * 图纸 V1.0 / 实物 V1.2；上板前仍须先断开电机验证默认停止。
 */
#define FEEDER_BENCH_VERSION "0.2.0-jog"
#define FEEDER_BENCH_ID 0x00020000U
#ifdef FEEDER_PUMP_TEST
/* 本次独立5V电机点动，不扩大旧推粮机构版本的50ms限制。 */
#define FEEDER_BENCH_MAX_PULSE_MS 200U
#else
#define FEEDER_BENCH_MAX_PULSE_MS 50U
#endif
#define FEEDER_BENCH_SETTLE_MS 1000U
#define FEEDER_BENCH_STOP_MARGIN_MS 20U
#define FEEDER_MOTOR_AF 2U
#define FEEDER_BENCH_CORE_HZ 8000000U

#endif
