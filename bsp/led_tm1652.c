#include "py32f0xx.h"
#include "led_tm1652.h"
#include "led_pattern.h"
#include "led_test_config.h"
#ifdef FEEDER_LOCAL_UI
#include "local_controls_io.h"
#endif
#ifdef FEEDER_CYCLE_BENCH
extern const uint8_t *cycle_bench_display(void);
#endif
volatile LedTestDebug g_led_test_debug = {.magic = 0x4C454454U, .abi = 1U};
_Static_assert(sizeof(LedTestDebug) == 56U, "Update LED reader ABI");
static uint16_t words[7];
static volatile uint32_t active, completed;
static uint32_t word_count, bit_index, byte_index;
static uint32_t seen_completed, ready_at, started, stage, step, cycles, hold_ms;
static void line(uint32_t high)
{
    GPIOB->BSRR = high ? (1UL << FEEDER_LED_PIN) : (1UL << (FEEDER_LED_PIN + 16U));
}
static void send(const uint8_t *data, uint32_t size)
{
    for (uint32_t i = 0U; i < size; ++i) { words[i] = led_encode_byte(data[i]); }
    word_count = size; bit_index = byte_index = 0U;
    TIM14->CNT = 0U; TIM14->SR = 0U;
    NVIC_ClearPendingIRQ(TIM14_IRQn);
    __DMB(); active = 1U;
    TIM14->DIER = TIM_DIER_UIE; TIM14->CR1 = TIM_CR1_CEN;
}
void TIM14_IRQHandler(void)
{
    TIM14->SR = 0U;
    if (active == 0U) { TIM14->CR1 = 0U; return; }
    if (byte_index == word_count) {
        /* 最后停止位已保持52us，再结束；帧间还需至少6ms高电平。 */
        TIM14->CR1 = 0U; TIM14->DIER = 0U; line(1U);
        active = 0U; __DMB(); completed++; return;
    }
    line((words[byte_index] >> bit_index) & 1U);
    bit_index++;
    if (bit_index == 11U) { bit_index = 0U; byte_index++; }
}
int led_tm1652_init(uint32_t clock_hz)
{
    if (clock_hz != 8000000U || (RCC->CFGR & RCC_CFGR_PPRE) != 0U) { return 0; }
    RCC->IOPENR |= RCC_IOPENR_GPIOBEN;
    RCC->APBENR2 |= RCC_APBENR2_TIM14EN;
    (void)RCC->APBENR2;
    line(1U); /* 先把锁存置为释放，再切输出，避免意外起始位。 */
    GPIOB->OTYPER |= 1UL << FEEDER_LED_PIN;
    GPIOB->PUPDR &= ~(3UL << (2U * FEEDER_LED_PIN));
    GPIOB->OSPEEDR &= ~(3UL << (2U * FEEDER_LED_PIN));
    GPIOB->MODER = (GPIOB->MODER & ~(3UL << (2U * FEEDER_LED_PIN))) | (1UL << (2U * FEEDER_LED_PIN));
    RCC->APBRSTR2 |= RCC_APBRSTR2_TIM14RST;
    RCC->APBRSTR2 &= ~RCC_APBRSTR2_TIM14RST;
    TIM14->PSC = 7U; TIM14->ARR = FEEDER_LED_BIT_US - 1U;
    TIM14->EGR = TIM_EGR_UG; TIM14->SR = 0U;
    /* LED边沿优先于USART2(优先级1)，短中断不阻塞整个报文的接收。 */
    NVIC_SetPriority(TIM14_IRQn, 0U); NVIC_ClearPendingIRQ(TIM14_IRQn); NVIC_EnableIRQ(TIM14_IRQn);
    return 1;
}
void led_tm1652_poll(uint32_t now)
{
    if (g_led_test_debug.fault != 0U) { return; }
    if (started == 0U) { started = 1U; ready_at = now + 1000U; }
    if (completed != seen_completed) {
        seen_completed = completed;
        ready_at = now + ((stage == 3U) ? hold_ms : FEEDER_LED_GAP_MS);
        if (stage == 3U) { step++; if (step == LED_PATTERN_STEPS) { step = 0U; cycles++; } }
    }
    if (active == 0U && (int32_t)(now - ready_at) >= 0) {
        uint8_t frame[7];
        if (stage == 0U) {
            (void)led_pattern_frame(0U, frame); send(frame, 7U); stage = 1U;
        } else if (stage == 1U) {
            /* 先清显示RAM，再启用显示，避免随机亮灯；命令帧与数据帧分开。 */
            const uint8_t control[2] = {0x18U, FEEDER_LED_CONTROL};
            send(control, 2U); stage = 2U;
        } else {
#if defined(FEEDER_CYCLE_BENCH) || defined(FEEDER_LOCAL_UI)
#ifdef FEEDER_CYCLE_BENCH
            const uint8_t *data=cycle_bench_display();
#else
            const uint8_t *data=local_controls_display();
#endif
            for(uint32_t i=0U;i<7U;i++) { frame[i]=data[i]; }
            hold_ms=100U;send(frame,7U);stage=3U;
#else
            hold_ms = led_pattern_frame(step, frame); send(frame, 7U); stage = 3U;
#endif
        }
    }
    g_led_test_debug.sequence++; __DMB();
    g_led_test_debug.uptime_ms = now;
    /* step指下一次要发的图案；最后已完成/已提交图案可从frames结合视频核对。 */
    g_led_test_debug.step = step; g_led_test_debug.cycles = cycles;
    g_led_test_debug.frames = completed; g_led_test_debug.busy = active;
    g_led_test_debug.gpio_moder = GPIOB->MODER;
    g_led_test_debug.gpio_otyper = GPIOB->OTYPER;
    g_led_test_debug.timer_psc = TIM14->PSC; g_led_test_debug.timer_arr = TIM14->ARR;
    g_led_test_debug.control = FEEDER_LED_CONTROL;
    __DMB(); g_led_test_debug.sequence++;
}
void led_tm1652_stop(void)
{
    TIM14->CR1 = 0U; TIM14->DIER = 0U; NVIC_DisableIRQ(TIM14_IRQn);
    active = 0U; line(1U); g_led_test_debug.fault = 1U;
}
