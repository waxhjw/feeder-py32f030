#ifndef FEEDER_MOTOR_BENCH_H
#define FEEDER_MOTOR_BENCH_H

#include <stdint.h>
#include "motor_pulse.h"

#define BENCH_MAILBOX_MAGIC 0x464A4F47U
#define BENCH_MAILBOX_ABI 1U
#define BENCH_CMD_A 0x4A4F4701U
#define BENCH_CMD_B 0x4A4F4702U
#define BENCH_CMD_STOP 0x53544F50U
/* 人工确认：位置有余量、人员远离、可靠断电、已做不动作验证。
 * 这是操作者本次测试声明，不是伪造的传感器读数。
 */
#define BENCH_REQUIRED_CHECKS 0x0FU

typedef enum {
    BENCH_SETTLING = 0, /* 上电稳定等待，不接收延迟执行的点动 */
    BENCH_READY = 1,    /* 等待一次人工点动，仍保持停机 */
    BENCH_RUNNING = 2,  /* 唯一一次定时脉冲正在运行 */
    BENCH_DONE = 3,     /* 已停止并锁定，再次动作须复位并重查位置 */
    BENCH_FAULT = 4     /* 故障锁定 */
} BenchState;

typedef enum {
    BENCH_OK = 0, BENCH_BAD_REQUEST = 1, BENCH_NOT_READY = 2,
    BENCH_CHECKS_MISSING = 3, BENCH_BAD_DURATION = 4,
    BENCH_DRIVER_FAILED = 5, BENCH_OVERRUN = 6, BENCH_INIT_FAILED = 7
} BenchFault;

typedef struct {
    uint32_t state;
    uint32_t fault;
    uint32_t started_ms;
    uint32_t duration_ms;
    uint32_t completed;
} MotorBench;

void motor_bench_init(MotorBench *bench, uint32_t driver_ready);
void motor_bench_step(MotorBench *bench, uint32_t now_ms, uint32_t command,
                      uint32_t checks, uint32_t duration_ms);

#endif
