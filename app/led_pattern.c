#include "led_pattern.h"
uint16_t led_encode_byte(uint8_t byte)
{
    uint32_t parity = 1U;
    for (uint32_t i = 0U; i < 8U; ++i) { parity ^= (byte >> i) & 1U; }
    return (uint16_t)(((uint16_t)byte << 1U) | (parity << 9U) | (1U << 10U));
}
uint32_t led_pattern_frame(uint32_t step, uint8_t frame[7])
{
    frame[0] = 0x08U;
    for (uint32_t i = 1U; i < 7U; ++i) { frame[i] = 0U; }
    if (step >= 1U && step <= 33U) {
        uint32_t lamp = step - 1U;
        frame[1U + lamp / 7U] = (uint8_t)(1U << (lamp % 7U));
        return 500U; /* L1~28，再LP1、LM1、L29、L30、L31，每颗半秒。 */
    }
    if (step >= 34U && step <= 37U) { frame[step - 33U] = 0x7FU; }
    else if (step == 38U) { frame[5] = 0x1FU; }
    else if (step == 39U) {
        for (uint32_t i = 1U; i <= 4U; ++i) { frame[i] = 0x7FU; }
        frame[5] = 0x1FU;
    }
    return 1000U; /* 0/40及越界值全灭；分组/全亮各一秒。 */
}
