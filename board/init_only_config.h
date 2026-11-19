#ifndef FEEDER_INIT_ONLY_CONFIG_H
#define FEEDER_INIT_ONLY_CONFIG_H

#include "board_config.h"

/* 本版只建立关闭电机的基线；不连接点动状态机，不提供动作入口。
 * PA8/PA11 来自主图的实际主控脚，不是导线标签 PA2/PA3。
 */
#define FEEDER_INIT_VERSION "0.2.1-init"
#define FEEDER_INIT_ID 0x00020100UL
#define FEEDER_INIT_MOTOR_MASK \
    ((1UL << FEEDER_MOTOR_INA_ACTUAL_PA_PIN) | (1UL << FEEDER_MOTOR_INB_ACTUAL_PA_PIN))
#define FEEDER_INIT_MOTOR_MODE_MASK \
    ((3UL << (2U * FEEDER_MOTOR_INA_ACTUAL_PA_PIN)) | \
     (3UL << (2U * FEEDER_MOTOR_INB_ACTUAL_PA_PIN)))
#define FEEDER_INIT_MOTOR_OUTPUT_MODE \
    ((1UL << (2U * FEEDER_MOTOR_INA_ACTUAL_PA_PIN)) | \
     (1UL << (2U * FEEDER_MOTOR_INB_ACTUAL_PA_PIN)))
#define FEEDER_INIT_MOTOR_PULL_DOWN \
    ((2UL << (2U * FEEDER_MOTOR_INA_ACTUAL_PA_PIN)) | \
     (2UL << (2U * FEEDER_MOTOR_INB_ACTUAL_PA_PIN)))

_Static_assert(FEEDER_INIT_MOTOR_MASK == 0x00000900UL,
               "Review physical motor mapping before changing this baseline");
_Static_assert((FEEDER_INIT_MOTOR_MASK & ((1UL << 13U) | (1UL << 14U))) == 0U,
               "Motor shutdown must preserve SWD pins");
#endif
