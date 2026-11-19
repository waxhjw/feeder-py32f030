#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "py32f0xx.h"
#include "touch_motor.h"
#include "touch_motor_io.h"
GPIO_TypeDef touch_gpioa,touch_gpiob;
RCC_TypeDef touch_rcc;
DBG_TypeDef touch_dbg;
IWDG_TypeDef touch_iwdg;
TIM_TypeDef model_tim1;
uint32_t SystemCoreClock=8000000U;
void SystemCoreClockUpdate(void) { }
static TouchMotor s;
static uint32_t now;
static void step(uint32_t dt,uint32_t level)
{
    now+=dt; touch_motor_step(&s,now,level); touch_motor_output(s.motor_on);
}
static void hold(uint32_t ms,uint32_t level)
{
    for (uint32_t i=0U;i<ms;i++) { step(1U,level); }
}
static void ready(uint32_t start)
{
    now=start;touch_motor_reset(&s,now);hold(2000U,1U);
    assert(s.phase==TOUCH_READY && s.motor_on==0U && s.toggles==0U);
}
static void press(void) { step(1U,0U);hold(3000U,0U); }
static void release(void) { step(1U,1U);hold(100U,1U);assert(s.phase==TOUCH_READY); }
int main(void)
{
    ready(0U);
    puts("PASS boot released 2s arms without starting");
    step(1U,0U);hold(2999U,0U);assert(!s.motor_on);step(1U,0U);
    assert(s.motor_on && s.toggles==1U && s.phase==TOUCH_RELEASE);
    assert(touch_gpioa.BSRR==0x08000100U);
    hold(20000U,0U);assert(s.motor_on && s.toggles==1U);
    puts("PASS 2999/3000ms boundary, continuous run and no repeat while held");
    release();press();assert(!s.motor_on && s.toggles==2U);
    assert(touch_gpioa.BSRR==0x09000000U);
    puts("PASS release and second 3s press turns motor off");
    ready(0U);step(1U,0U);hold(2999U,0U);step(1U,1U);assert(!s.motor_on && !s.toggles);
    hold(100U,1U);step(1U,0U);hold(500U,0U);step(1U,1U);assert(!s.motor_on);
    puts("PASS release at threshold and short presses do not trigger");
    ready(0U);press();step(1U,1U);hold(99U,1U);step(1U,0U);hold(3100U,0U);
    assert(s.motor_on && s.toggles==1U);release();press();assert(!s.motor_on);
    puts("PASS release bounce cannot rearm a long-held input");
    now=0U;touch_motor_reset(&s,now);hold(2000U,0U);assert(s.fault==3U && !s.motor_on);
    hold(6000U,1U);assert(s.phase==TOUCH_FAULT && !s.motor_on);
    puts("PASS boot held/wrong idle locks off until reset");
    now=0U;touch_motor_reset(&s,now);hold(1700U,0U);hold(300U,1U);assert(s.fault==3U);
    puts("PASS insufficient boot release qualification locks off");
    ready(0U);press();step(51U,0U);assert(s.fault==2U && !s.motor_on);
    assert(touch_gpioa.BSRR==0x09000000U);
    ready(0U);press();step(1U,2U);assert(s.fault==1U && !s.motor_on);
    puts("PASS missed sample gap and invalid input stop running motor");
    ready(UINT32_MAX-4000U);press();assert(s.motor_on && !s.fault);release();press();assert(!s.motor_on);
    puts("PASS millisecond counter wrap preserves hold and release timing");
    ready(0U);press();touch_motor_stop(&s);assert(!s.motor_on);hold(4000U,0U);assert(!s.motor_on);
    release();press();assert(s.motor_on);touch_motor_fail(&s,55U);hold(4000U,1U);assert(!s.motor_on && s.fault==55U);
    now=0U;touch_motor_reset(&s,now);touch_motor_stop(&s);hold(2000U,0U);assert(s.fault==3U);
    puts("PASS STOP requires release, cannot bypass boot; faults stay stopped");
    memset(&touch_gpioa,0,sizeof touch_gpioa);memset(&touch_rcc,0,sizeof touch_rcc);
    memset(&touch_iwdg,0,sizeof touch_iwdg);touch_gpiob.MODER=0xFFFFFFFFU;touch_gpiob.PUPDR=0xAAAAAAAAU;
    uint32_t old_mode=touch_gpiob.MODER,old_pull=touch_gpiob.PUPDR;
    touch_rcc.CSR=RCC_CSR_LSIRDY;touch_dbg.APBFZ1=0xFFFFFFFFU;
    assert(touch_motor_io_init());
    assert(touch_gpiob.MODER==(old_mode&~0xC000U));assert(touch_gpiob.PUPDR==(old_pull&~0xC000U));
    assert((touch_gpioa.MODER&0xC30000U)==0x410000U);
    assert(touch_iwdg.PR==3U && touch_iwdg.RLR==1023U && touch_iwdg.KR==0xAAAAU);
    assert(!(touch_dbg.APBFZ1&0x1000U));assert(!model_tim1.CR1);
    puts("PASS PB7 no-pull input preserves PB4/PB6; GPIO off and watchdog configured");
    touch_gpiob.IDR=1U<<6U;assert(touch_motor_level()==0U);
    touch_gpiob.IDR=1U<<7U;assert(touch_motor_level()==1U);
    touch_gpiob.IDR|=1U<<6U;assert(touch_motor_level()==1U);
    puts("PASS only PB7 sampled, TOP2 PB6 ignored");
    touch_motor_output(1U);assert(touch_gpioa.BSRR==0x08000100U);
    touch_motor_output(0U);assert(touch_gpioa.BSRR==0x09000000U);
    touch_iwdg.KR=0U;touch_motor_watchdog_feed();assert(touch_iwdg.KR==0xAAAAU);
    puts("PASS A polarity output, explicit off, main-loop watchdog refresh");
    touch_rcc.CSR=0U;assert(!touch_motor_io_init());assert(touch_gpioa.BSRR==0x09000000U);
    touch_rcc.CSR=RCC_CSR_LSIRDY;touch_iwdg.SR=1U;assert(!touch_motor_io_init());
    touch_iwdg.SR=0U;SystemCoreClock=24000000U;assert(!touch_motor_io_init());
    puts("PASS LSI, IWDG update and clock failure bounded with output off");
    return 0;
}
