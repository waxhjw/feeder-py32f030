#include "py32f0xx.h"
#include "cycle_bench.h"
#include "input_observer_io.h"
#include "led_tm1652.h"
#include "motor_off.h"

typedef struct {
    uint32_t magic, abi, sequence, uptime_ms, keys_ready;
    uint32_t raw_micro, stable_micro, input_valid, rises, falls;
    uint32_t selected, state, target, completed, request, partial, fault, rejected;
    uint32_t gpio, led_frames, boot_fault, display;
} CycleBenchDebug;
_Static_assert(sizeof(CycleBenchDebug)==88U,"Cycle bench debug ABI");
volatile CycleBenchDebug g_cycle_bench_debug={.magic=0x43424E43U,.abi=1U};
__attribute__((used,section(".fw_metadata")))
const char g_feeder_version[]="FEEDER 0.15.3-cycle-diag";
static volatile uint32_t ticks;
static CycleBench bench;
static uint8_t display[7];
const uint8_t *cycle_bench_display(void) { return display; }
void SysTick_Handler(void) { ticks++; }

static void fault_stop(uint32_t code)
{
    __disable_irq();
    g_cycle_bench_debug.boot_fault=code;
    for (;;) { motor_off_hold(); }
}
void NMI_Handler(void) { fault_stop(1U); }
void HardFault_Handler(void) { fault_stop(2U); }

static void publish(uint32_t now)
{
    const FeedCycle *cycle=&bench.cycle;
    const InputChannel *micro=&bench.input.channel[0];
    g_cycle_bench_debug.sequence++;__DMB();
    g_cycle_bench_debug.uptime_ms=now;
    g_cycle_bench_debug.keys_ready=bench.keys_ready;
    g_cycle_bench_debug.raw_micro=micro->raw;
    g_cycle_bench_debug.stable_micro=micro->stable;
    g_cycle_bench_debug.input_valid=micro->valid;
    g_cycle_bench_debug.rises=micro->rises;
    g_cycle_bench_debug.falls=micro->falls;
    g_cycle_bench_debug.selected=bench.selected;
    g_cycle_bench_debug.state=cycle->state;
    g_cycle_bench_debug.target=cycle->target;
    g_cycle_bench_debug.completed=cycle->completed;
    g_cycle_bench_debug.request=cycle->motor_request;
    g_cycle_bench_debug.partial=cycle->partial;
    g_cycle_bench_debug.fault=cycle->fault;
    g_cycle_bench_debug.rejected=bench.rejected;
    g_cycle_bench_debug.gpio=GPIOA->ODR&0x900U;
    g_cycle_bench_debug.led_frames=g_led_test_debug.frames;
    g_cycle_bench_debug.display=(uint32_t)display[1]|((uint32_t)display[2]<<8U)|
        ((uint32_t)display[3]<<16U)|((uint32_t)display[4]<<24U);
    __DMB();g_cycle_bench_debug.sequence++;
}

int main(void)
{
    /* 专用观察版没有电机、串口或 Flash 写入模块。逻辑请求只发布到快照。 */
    motor_off_hold();
    SystemCoreClockUpdate();
    if (SystemCoreClock!=8000000U || SysTick_Config(SystemCoreClock/1000U)) { fault_stop(3U); }
    input_observer_io_init();
    if (!led_tm1652_init(SystemCoreClock)) { fault_stop(4U); }
    cycle_bench_init(&bench,ticks);
    uint32_t last=UINT32_MAX, last_publish=ticks-25U;
    for (;;) {
        const uint32_t now=ticks;
        if (now!=last) {
            last=now;
            motor_off_hold();
            const uint32_t portb=GPIOB->IDR;
            const uint32_t levels=((GPIOF->IDR>>3U)&1U)|(((portb>>7U)&1U)<<1U)|(((portb>>6U)&1U)<<2U);
            cycle_bench_step(&bench,now,levels);
            cycle_bench_frame(&bench,display);
            led_tm1652_poll(now);
            if ((uint32_t)(now-last_publish)>=25U) { last_publish=now;publish(now); }
            IWDG->KR=0xAAAAU; /* 若选项字节已启动独立看门狗，保持正常循环喂狗。 */
        }
        __WFI();
    }
}
