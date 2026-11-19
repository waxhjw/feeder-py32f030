#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "py32f0xx.h"
#include "pump_test.h"
#include "motor_pulse.h"
GPIO_TypeDef model_gpioa;
RCC_TypeDef model_rcc;
TIM_TypeDef model_tim1;
DBG_TypeDef model_dbg;
uint32_t SystemCoreClock = 8000000U;
void SystemCoreClockUpdate(void) { }
static void fresh(void)
{
    memset(&model_gpioa, 0, sizeof(model_gpioa)); memset(&model_rcc, 0, sizeof(model_rcc));
    memset(&model_tim1, 0, sizeof(model_tim1)); model_dbg.APBFZ2 = 0xFFFFFFFFU;
    SystemCoreClock = 8000000U;
    g_pump_test = (PumpTestMailbox){.magic=0x50554D54U,.abi=1U,.firmware_id=0x00080000U,.duration_ms=200U};
    assert(pump_test_init());
}
static void request(uint32_t now)
{
    g_pump_test.nonce = 123U; g_pump_test.nonce_inverse = 123U ^ UINT32_MAX;
    g_pump_test.command = PUMP_RUN; pump_test_poll(now);
}
static void off(void)
{
    assert((model_gpioa.MODER & 0x00C30000U) == 0x00410000U);
    assert(model_gpioa.BSRR == 0x09000000U && model_tim1.CR1 == 0U && model_tim1.BDTR == 0U);
}
int main(void)
{
    fresh();
    for (uint32_t now = 0U; now < 5000U; ++now) { pump_test_poll(now); off(); }
    assert(g_pump_test.state == PUMP_READY && g_pump_test.accepted_nonce == 0U);
    puts("PASS boot and idle never actuate, no App path");
    fresh(); request(999U); assert(g_pump_test.state == PUMP_FAULT); off();
    puts("PASS early request rejected and locked");
    fresh(); g_pump_test.command=PUMP_RUN; g_pump_test.nonce=123U; pump_test_poll(1000U);
    assert(g_pump_test.fault == 4U); off();
    fresh(); g_pump_test.command=456U; pump_test_poll(1000U); assert(g_pump_test.fault == 4U); off();
    puts("PASS torn nonce and unknown command rejected");
    fresh(); request(1000U);
    assert(g_pump_test.state == PUMP_RUNNING && g_pump_test.accepted_nonce == 123U);
    assert(model_tim1.PSC == 7999U && model_tim1.ARR == 200U);
    assert(model_tim1.CCR1 == 1U && model_tim1.CCR4 == 1U && model_tim1.CR1 == 9U);
    assert(model_tim1.CCMR1 == 0x70U && model_tim1.CCMR2 == 0x4000U);
    assert(model_tim1.CCER == 0x1001U && model_tim1.BDTR == 0x8000U);
    assert((model_gpioa.MODER & 0x00C30000U) == 0x00820000U);
    assert((model_gpioa.AFR[1] & 0xF00FU) == 0x2002U);
    assert((model_dbg.APBFZ2 & DBGMCU_APB_FZ2_DBG_TIM1_STOP) == 0U);
    assert(model_tim1.DIER == 0U);
    puts("PASS real motor driver sets A polarity, 200ms TIM1 OPM and no freeze on halt");
    /* 模拟本地TIM1到期：回到CNT=0、清CEN；PWM2此时为低，无需电脑停机消息。 */
    model_tim1.CNT=0U; model_tim1.CR1 &= ~1U; pump_test_poll(1201U);
    assert(g_pump_test.state == PUMP_DONE && g_pump_test.completed == 1U); off();
    request(1500U); assert(g_pump_test.state == PUMP_DONE && g_pump_test.completed == 1U); off();
    puts("PASS expiry stops once, repeat after completion cannot restart");
    fresh(); request(1000U); request(1010U); assert(g_pump_test.fault == 2U); off();
    puts("PASS repeated request during active pulse stops and faults");
    fresh(); request(1000U); pump_test_poll(1221U); assert(g_pump_test.fault == 3U); off();
    puts("PASS stuck timer local overrun stop");
    fresh(); request(UINT32_MAX-50U); model_tim1.CR1 &= ~1U; pump_test_poll(150U);
    assert(g_pump_test.state == PUMP_DONE && g_pump_test.completed == 1U); off();
    puts("PASS uint32 tick wrap completes without repeat");
    fresh(); request(1000U); g_pump_test.command=PUMP_STOP; pump_test_poll(1001U);
    assert(g_pump_test.state == PUMP_DONE && g_pump_test.completed == 0U); off();
    request(2000U); off();
    puts("PASS explicit STOP ends and locks without claiming completion");
    fresh(); request(1000U); pump_test_fault(99U); off(); request(2000U);
    assert(g_pump_test.fault == 99U); off();
    puts("PASS system fault disables output and cannot restart");
    fresh(); assert(!motor_pulse_start(MOTOR_POLARITY_A,201U)); off();
    assert(!motor_pulse_start((MotorPolarity)3,200U)); off();
    puts("PASS motor driver rejects duration above 200ms and invalid polarity");
    fresh(); SystemCoreClock=16000000U; assert(!pump_test_init()); off();
    assert(g_pump_test.state == PUMP_FAULT);
    assert(!motor_pulse_start(MOTOR_POLARITY_A,200U)); off();
    puts("PASS bad clock cannot arm driver");
    return 0;
}
