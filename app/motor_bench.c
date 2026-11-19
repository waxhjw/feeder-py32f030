#include "motor_bench.h"
#include "motor_bench_config.h"

static void fail(MotorBench *bench, BenchFault fault)
{
    motor_pulse_stop();
    bench->state = BENCH_FAULT;
    bench->fault = fault;
}

void motor_bench_init(MotorBench *bench, uint32_t driver_ready)
{
    *bench = (MotorBench){BENCH_SETTLING, BENCH_OK, 0U, 0U, 0U};
    motor_pulse_stop();
    if (driver_ready == 0U) {
        fail(bench, BENCH_INIT_FAILED);
    }
}

void motor_bench_step(MotorBench *bench, uint32_t now_ms, uint32_t command,
                      uint32_t checks, uint32_t duration_ms)
{
    /* 停止优先，任何状态都不能因为停止命令转入可再次启动状态。 */
    if (command == BENCH_CMD_STOP) {
        motor_pulse_stop();
        if (bench->state != BENCH_FAULT) {
            bench->state = BENCH_DONE;
        }
        return;
    }
    if (bench->state == BENCH_FAULT || bench->state == BENCH_DONE) {
        motor_pulse_stop();
        return;
    }
    if (bench->state == BENCH_RUNNING) {
        if (command != 0U) {
            fail(bench, BENCH_NOT_READY); /* 运行中重发/换向立即停并锁定 */
        } else if (!motor_pulse_active()) {
            motor_pulse_stop();
            bench->completed++;
            bench->state = BENCH_DONE;
        } else if ((uint32_t)(now_ms - bench->started_ms) >
                   bench->duration_ms + FEEDER_BENCH_STOP_MARGIN_MS) {
            fail(bench, BENCH_OVERRUN);
        }
        return;
    }
    if (bench->state == BENCH_SETTLING && now_ms >= FEEDER_BENCH_SETTLE_MS) {
        bench->state = BENCH_READY;
    }
    if (command == 0U) {
        return;
    }
    if (command != BENCH_CMD_A && command != BENCH_CMD_B) {
        fail(bench, BENCH_BAD_REQUEST);
    } else if (bench->state != BENCH_READY) {
        fail(bench, BENCH_NOT_READY);
    } else if (checks != BENCH_REQUIRED_CHECKS) {
        fail(bench, BENCH_CHECKS_MISSING);
    } else if (duration_ms == 0U || duration_ms > FEEDER_BENCH_MAX_PULSE_MS) {
        fail(bench, BENCH_BAD_DURATION);
    } else {
        /* 人工台架前提已声明、已稳定且无故障；微动位置/电平仍未知，
         * 本入口仅允许已目视确认中间有余量的单次验证，不执行自动回零。
         */
        bench->state = BENCH_RUNNING;
        bench->started_ms = now_ms;
        bench->duration_ms = duration_ms;
        if (!motor_pulse_start(command == BENCH_CMD_A ? MOTOR_POLARITY_A : MOTOR_POLARITY_B,
                               duration_ms)) {
            fail(bench, BENCH_DRIVER_FAILED);
        }
    }
}
