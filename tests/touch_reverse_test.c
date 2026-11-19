/* 先运行原版长按/启动/故障/寄存器回归，再覆盖新增短按换向。 */
#define main previous_main
#include "touch_motor_test.c"
#undef main
static void short_press(uint32_t length)
{
    step(1U,0U);hold(length-1U,0U);step(1U,1U);hold(100U,1U);
}
int main(void)
{
    previous_main();SystemCoreClock=8000000U;
    ready(0U);short_press(200U);assert(s.direction==1U && s.reversals==1U && !s.motor_on && !s.pending);
    puts("PASS stopped short press changes only next direction, never starts");
    press();assert(s.motor_on && s.direction==1U);release();short_press(200U);
    assert(s.motor_on && s.direction==0U && s.pending);
    touch_motor_output_direction(s.motor_on && !s.pending,s.direction);assert(touch_gpioa.BSRR==0x09000000U);
    hold(299U,1U);assert(s.pending);hold(1U,1U);assert(!s.pending);
    touch_motor_output_direction(s.motor_on && !s.pending,s.direction);assert(touch_gpioa.BSRR==0x08000100U);
    puts("PASS running reversal has 300ms off interval before opposite output");
    short_press(200U);assert(s.direction==1U && s.pending);hold(300U,1U);
    touch_motor_output_direction(s.motor_on && !s.pending,s.direction);assert(touch_gpioa.BSRR==0x01000800U);
    puts("PASS both directions use complementary GPIO, never brake11");
    short_press(200U);assert(s.pending);touch_motor_stop(&s);hold(1000U,1U);
    assert(!s.motor_on && !s.pending);puts("PASS STOP during reverse pause cancels restart");
    ready(0U);short_press(49U);assert(!s.reversals);short_press(50U);assert(s.reversals==1U);
    short_press(800U);assert(s.reversals==2U);short_press(801U);assert(s.reversals==2U);
    puts("PASS short boundaries49/50/800/801ms and no action for medium hold");
    ready(0U);press();hold(5000U,0U);release();assert(s.toggles==1U && !s.reversals);
    puts("PASS long press and long-held release never also reverse");
    ready(0U);step(1U,0U);hold(200U,0U);step(1U,1U);hold(50U,1U);step(1U,0U);
    hold(1000U,0U);step(1U,1U);hold(100U,1U);assert(!s.reversals);
    puts("PASS unstable release cancels short request instead of delayed false reversal");
    ready(UINT32_MAX-2200U);short_press(200U);press();release();short_press(200U);
    assert(s.pending);touch_motor_fail(&s,99U);hold(500U,1U);assert(!s.pending && !s.motor_on);
    puts("PASS wrapped clocks and fault during reversal never restart");
    return 0;
}
