#include "py32f0xx.h"
#include "input_observer_io.h"
#include "touch_motor_io.h"
#include <assert.h>
#include <stdio.h>
GPIO_TypeDef touch_gpioa,touch_gpiob,input_gpiof;
RCC_TypeDef touch_rcc;
DBG_TypeDef touch_dbg;
IWDG_TypeDef touch_iwdg;
TIM_TypeDef model_tim1;
uint32_t SystemCoreClock=8000000U;
void SystemCoreClockUpdate(void) { }
int main(void)
{
    input_gpiof.MODER=0xFFFFFFFFU;input_gpiof.PUPDR=0xFFFFFFFFU;
    touch_gpiob.MODER=0xFFFFFFFFU;touch_gpiob.PUPDR=0xFFFFFFFFU;
    input_observer_io_init();
    assert(input_gpiof.MODER==(0xFFFFFFFFU&~0xC0U));
    assert(input_gpiof.PUPDR==((0xFFFFFFFFU&~0xC0U)|0x40U));
    assert(touch_gpiob.MODER==(0xFFFFFFFFU&~0xF000U));
    assert(touch_gpiob.PUPDR==(0xFFFFFFFFU&~0xF000U));
    puts("PASS real GPIO init changes only PF3 PB6 PB7 and preserves SWD UART ADC");
    input_gpiof.IDR=8U;touch_gpiob.IDR=0xC0U;
    for(unsigned t=0;t<=250;t++) { input_observer_io_poll(t); }
    assert(g_input_debug.size==128U && g_input_debug.channel[0][2]==1U);
    input_gpiof.IDR=0U;touch_gpiob.IDR=0U;
    for(unsigned t=251;t<=500;t++) { input_observer_io_poll(t); }
    for(unsigned c=0;c<3;c++) { assert(g_input_debug.channel[c][4]==1U); }
    assert(g_input_debug.adc_status==0U);
    puts("PASS real input mapping and coherent publication, ADC marked unavailable");
    for(unsigned on=0;on<2;on++) {
        touch_gpioa.BSRR=0U;touch_motor_output(on);assert(touch_gpioa.BSRR==0x09000000U);
        for(unsigned direction=0;direction<2;direction++) {
            touch_gpioa.BSRR=0U;touch_motor_output_direction(on,direction);
            assert(touch_gpioa.BSRR==0x09000000U);
        }
    }
    puts("PASS actual driver requests both directions always drive OFF in diagnostic");
    return 0;
}
