#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "motor_bench.h"
#include "motor_bench_config.h"

/* 用驱动公开接口替身注入完成、拒绝、卡住三类结果，不接硬件。 */
static bool active, driver_accepts;
static unsigned starts, stops;
static MotorPolarity last_polarity;
static uint32_t last_duration;
bool motor_pulse_init(void) { return true; }
void motor_pulse_stop(void) { active = false; stops++; }
bool motor_pulse_active(void) { return active; }
bool motor_pulse_start(MotorPolarity polarity, uint32_t duration)
{
    starts++;
    last_polarity = polarity;
    last_duration = duration;
    active = driver_accepts;
    return driver_accepts;
}
static MotorBench fresh(void)
{
    MotorBench bench;
    active = false; driver_accepts = true; starts = stops = 0U;
    motor_bench_init(&bench, 1U);
    return bench;
}
static void request(MotorBench *bench, uint32_t time, uint32_t command,
                    uint32_t checks, uint32_t duration)
{
    motor_bench_step(bench, time, command, checks, duration);
}
int main(void)
{
    MotorBench b = fresh();
    for (uint32_t time = 0U; time < 5000U; time++) {
        request(&b, time, 0U, 0U, 0U);
    }
    assert(starts == 0U && !active && b.state == BENCH_READY);
    puts("PASS boot and idle never start");

    b = fresh(); request(&b, 999U, BENCH_CMD_A, 15U, 50U);
    assert(starts == 0U && b.fault == BENCH_NOT_READY);
    request(&b, 2000U, BENCH_CMD_A, 15U, 50U);
    assert(starts == 0U && b.state == BENCH_FAULT);
    puts("PASS early command discarded and fault stays locked");

    for (unsigned bit = 0U; bit < 4U; bit++) {
        b = fresh(); request(&b, 1000U, BENCH_CMD_A, 15U & ~(1U << bit), 50U);
        assert(starts == 0U && b.fault == BENCH_CHECKS_MISSING);
    }
    puts("PASS every missing operator precondition rejects motion");

    const uint32_t invalid_times[] = {0U, 51U, UINT32_MAX};
    for (unsigned i = 0U; i < sizeof(invalid_times)/sizeof(invalid_times[0]); i++) {
        b = fresh(); request(&b, 1000U, BENCH_CMD_A, 15U, invalid_times[i]);
        assert(starts == 0U && b.fault == BENCH_BAD_DURATION);
    }
    b = fresh(); request(&b, 1000U, 0xFFFFFFFFU, 15U, 50U);
    assert(starts == 0U && b.fault == BENCH_BAD_REQUEST);
    puts("PASS duration bounds and unknown command reject motion");

    b = fresh(); request(&b, 1000U, BENCH_CMD_A, 15U, 50U);
    assert(starts == 1U && active && b.state == BENCH_RUNNING);
    assert(last_polarity == MOTOR_POLARITY_A && last_duration == 50U);
    active = false; request(&b, 1051U, 0U, 0U, 0U);
    assert(b.state == BENCH_DONE && b.completed == 1U);
    request(&b, 3000U, BENCH_CMD_B, 15U, 50U);
    assert(starts == 1U && !active && b.state == BENCH_DONE);
    puts("PASS one pulse completes then locks out repeats and reversal");

    b = fresh(); request(&b, 1000U, BENCH_CMD_B, 15U, 1U);
    assert(active && last_polarity == MOTOR_POLARITY_B && last_duration == 1U);
    request(&b, 1001U, BENCH_CMD_STOP, 0U, 0U);
    assert(!active && b.state == BENCH_DONE);
    request(&b, 3000U, BENCH_CMD_A, 15U, 50U);
    assert(starts == 1U);
    puts("PASS stop takes priority and prevents restart");

    b = fresh(); request(&b, 1000U, BENCH_CMD_A, 15U, 50U);
    request(&b, 1001U, BENCH_CMD_B, 15U, 50U);
    assert(!active && starts == 1U && b.fault == BENCH_NOT_READY);
    puts("PASS command during motion stops and locks instead of reversing");

    b = fresh(); driver_accepts = false;
    request(&b, 1000U, BENCH_CMD_A, 15U, 50U);
    assert(!active && b.fault == BENCH_DRIVER_FAILED);
    puts("PASS backend failure produces stopped fault");

    b = fresh(); request(&b, 1000U, BENCH_CMD_A, 15U, 50U);
    request(&b, 1071U, 0U, 0U, 0U);
    assert(!active && b.fault == BENCH_OVERRUN);
    request(&b, 1072U, BENCH_CMD_STOP, 0U, 0U);
    assert(b.state == BENCH_FAULT && b.fault == BENCH_OVERRUN);
    puts("PASS stuck backend times out and stop cannot erase fault");

    b = fresh(); request(&b, UINT32_MAX - 20U, BENCH_CMD_A, 15U, 50U);
    request(&b, 60U, 0U, 0U, 0U);
    assert(!active && b.fault == BENCH_OVERRUN);
    puts("PASS timeout arithmetic survives millisecond wrap");

    b = fresh(); motor_bench_init(&b, 0U);
    request(&b, 1000U, BENCH_CMD_A, 15U, 50U);
    assert(starts == 0U && !active && b.fault == BENCH_INIT_FAILED);
    puts("PASS initialization fault never starts");
    puts("MOTOR BENCH LOGIC PASS: hardware timing and mechanics NOT tested");
    return 0;
}
