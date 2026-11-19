#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "py32f0xx.h"
#include "led_pattern.h"
#include "led_tm1652.h"
GPIO_TypeDef model_gpio;
RCC_TypeDef model_rcc;
TIM_TypeDef model_tim;
void TIM14_IRQHandler(void);
static unsigned popcount(unsigned value)
{
    unsigned count = 0U;
    while (value) { count += value & 1U; value >>= 1U; }
    return count;
}
int main(void)
{
    for (unsigned value = 0U; value < 256U; ++value) {
        unsigned bits = led_encode_byte((uint8_t)value);
        assert((bits & 1U) == 0U && ((bits >> 10U) & 1U) == 1U);
        assert(((bits >> 1U) & 255U) == value);
        assert(popcount((bits >> 1U) & 511U) % 2U == 1U);
    }
    puts("PASS all 256 8O1 words match independent parity/start/stop checks");
    unsigned seen[6] = {0};
    for (unsigned step = 1U; step <= 33U; ++step) {
        uint8_t frame[7]; assert(led_pattern_frame(step, frame) == 500U);
        assert(frame[0] == 8U && frame[6] == 0U);
        unsigned total = 0U;
        for (unsigned grid = 0U; grid < 6U; ++grid) {
            total += popcount(frame[grid + 1U]);
            assert((seen[grid] & frame[grid + 1U]) == 0U);
            seen[grid] |= frame[grid + 1U];
        }
        assert(total == 1U);
    }
    assert(seen[0] == 127U && seen[1] == 127U && seen[2] == 127U && seen[3] == 127U);
    assert(seen[4] == 31U && seen[5] == 0U);
    for (unsigned step = 34U; step <= 37U; ++step) {
        uint8_t frame[7]; assert(led_pattern_frame(step, frame) == 1000U);
        assert(frame[step - 33U] == 127U && frame[5] == 0U && frame[6] == 0U);
    }
    uint8_t frame[7]; led_pattern_frame(39U, frame);
    assert(memcmp(frame, (uint8_t[]){8,127,127,127,127,31,0}, 7U) == 0);
    led_pattern_frame(40U, frame); assert(memcmp(frame, (uint8_t[]){8,0,0,0,0,0,0}, 7U) == 0);
    led_pattern_frame(100U, frame); assert(memcmp(frame, (uint8_t[]){8,0,0,0,0,0,0}, 7U) == 0);
    puts("PASS 33 unique LEDs, group masks, unconnected outputs and blank states");
    assert(led_tm1652_init(16000000U) == 0);
    model_rcc.CFGR = RCC_CFGR_PPRE;
    assert(led_tm1652_init(8000000U) == 0);
    model_rcc.CFGR = 0U;
    model_gpio.MODER = 0xFFFFFFFFU; model_gpio.OTYPER = 0x400U;
    assert(led_tm1652_init(8000000U) == 1);
    assert(model_gpio.MODER == 0xFFFFFDFFU && model_gpio.OTYPER == 0x410U);
    assert(model_tim.PSC == 7U && model_tim.ARR == 51U && model_gpio.BSRR == 16U);
    puts("PASS clock rejection, PB4 open drain, untouched other pins, timer divisor");
    /* 让真实驱动在模型里完整循环；独立解码其每次BSRR写出的线电平。 */
    uint32_t now = 0U, last_end = 0U, transaction = 0U, pending = 0U;
    for (; now <= 28000U; ++now) {
        led_tm1652_poll(now);
        if (model_tim.CR1 == 0U) { continue; }
        if (pending) { assert(now - last_end >= 6U); }
        else { assert(now >= 1000U); }
        unsigned bits[78], count = 0U;
        while (model_tim.CR1) {
            TIM14_IRQHandler();
            if (model_tim.CR1) {
                assert(count < 77U);
                assert(model_gpio.BSRR == 16U || model_gpio.BSRR == (16U << 16U));
                bits[count++] = model_gpio.BSRR == 16U;
            }
        }
        assert(count == ((transaction == 1U) ? 22U : 77U));
        uint8_t decoded[7] = {0};
        for (unsigned byte = 0U; byte < count / 11U; ++byte) {
            assert(bits[byte*11U] == 0U && bits[byte*11U+10U] == 1U);
            unsigned ones = 0U;
            for (unsigned bit = 0U; bit < 8U; ++bit) {
                decoded[byte] |= (uint8_t)(bits[byte*11U+1U+bit] << bit);
                ones += bits[byte*11U+1U+bit];
            }
            assert((ones + bits[byte*11U+9U]) % 2U == 1U);
        }
        if (transaction == 0U) { assert(memcmp(decoded, (uint8_t[]){8,0,0,0,0,0,0}, 7U) == 0); }
        else if (transaction == 1U) { assert(decoded[0] == 0x18U && decoded[1] == 0x94U); }
        else {
            led_pattern_frame((transaction-2U) % LED_PATTERN_STEPS, frame);
            assert(memcmp(decoded, frame, 7U) == 0);
        }
        now += (count * 52U + 999U) / 1000U;
        last_end = now; pending = 1U; transaction++;
        led_tm1652_poll(now); /* 正式循环下一次看到完成时开始保持时间/帧间隔。 */
        assert(model_tim.CR1 == 0U);
    }
    assert(g_led_test_debug.cycles >= 1U && transaction >= 43U);
    puts("PASS real driver startup order, decoded frames, gaps and complete repeating cycle");
    led_tm1652_stop(); led_tm1652_poll(now);
    assert(model_tim.CR1 == 0U && g_led_test_debug.fault == 1U && model_gpio.BSRR == 16U);
    puts("PASS fault stops timer and releases bus");
    return 0;
}
