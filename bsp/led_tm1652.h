#ifndef FEEDER_LED_TM1652_H
#define FEEDER_LED_TM1652_H
#include <stdint.h>
typedef struct {
    uint32_t magic, abi, sequence, uptime_ms, step, cycles, frames, busy;
    uint32_t gpio_moder, gpio_otyper, timer_psc, timer_arr, control, fault;
} LedTestDebug;
extern volatile LedTestDebug g_led_test_debug;
int led_tm1652_init(uint32_t clock_hz);
void led_tm1652_poll(uint32_t now);
/* 故障时停止发包并释放通信脚；不声称失去通信后一定能熄灭已锁存灯。 */
void led_tm1652_stop(void);
#endif
