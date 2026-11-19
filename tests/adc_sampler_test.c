#include "py32f0xx.h"
#include "adc_sampler.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
GPIO_TypeDef touch_gpioa,touch_gpiob,input_gpiof;
RCC_TypeDef touch_rcc;
volatile uint32_t adc_registers[24];
#define AR(n) adc_registers[(n)/4U]
static unsigned now;
static void init(unsigned start)
{
    now=start;memset((void *)adc_registers,0,sizeof(adc_registers));
    touch_gpioa.MODER=0x12345678U;touch_gpioa.PUPDR=0xFFFFFFFFU;adc_sampler_init(now);
}
static void calibrate(void)
{
    const int values[5]={-3,-9,-1,2,-10};
    for(unsigned i=0;i<5;i++) {
        assert(AR(8)==ADC_CR_ADCAL);AR(8)=0U;AR(0x48)=((unsigned)values[i])&0x7FFFFFU;AR(0x4C)=123U;
        adc_sampler_poll(++now);
    }
    assert(AR(0x50)==(uint32_t)-3 && AR(0x54)==123U && (AR(0x44)&0x8000U));
    assert((AR(8)&ADC_CR_ADEN)==0U);
    adc_sampler_poll(++now);assert(AR(8)==ADC_CR_ADEN);
    adc_sampler_poll(++now);adc_sampler_poll(++now);assert(AR(8)==ADC_CR_ADSTART);
}
static void result(unsigned value)
{
    /* 模拟实板：单次转换结束后ADEN回0，下一次必须重新使能。 */
    AR(0)=ADC_ISR_EOC;AR(0x40)=value;AR(8)=0U;adc_sampler_poll(++now);
}
static void enabled_start(void)
{
    assert(AR(8)==ADC_CR_ADEN);
    adc_sampler_poll(++now);adc_sampler_poll(++now);
    assert(AR(8)==ADC_CR_ADSTART);
}
static void next_conversion(void)
{
    now+=20U;adc_sampler_poll(now);enabled_start();
}
int main(void)
{
    init(0U);
    assert(touch_gpioa.MODER==(0x12345678U|0xC0U));assert(touch_gpioa.PUPDR==(0xFFFFFFFFU&~0xC0U));
    assert(AR(0x10)==(2U<<28U) && AR(0x14)==7U && AR(0x28)==8U && AR(4)==0U);
    puts("PASS PA3-only analog input, PCLKdiv4, channel3, no DMA or ADC IRQ");
    calibrate();result(100U);now=250U;adc_sampler_poll(now);
    assert(g_adc_debug.phase==3U && g_adc_debug.valid==1U && g_adc_debug.samples==1U);
    assert(g_adc_debug.calibration_median==(uint32_t)-3 && g_adc_debug.calibrations==5U);
    puts("PASS official five-calibration signed23 median and enable waits");
    enabled_start();result(3U);next_conversion();result(4095U);now=500U;adc_sampler_poll(now);
    assert(g_adc_debug.samples==3U && g_adc_debug.raw==4095U && g_adc_debug.minimum==3U && g_adc_debug.maximum==4095U);
    puts("PASS raw12 extrema and repeated enable after real-board ADEN auto-off");
    enabled_start();
    adc_sampler_pause(now);assert(g_adc_debug.phase==6U && !g_adc_debug.valid && (AR(8)&ADC_CR_ADSTP));
    now+=1000U;adc_sampler_poll(now);assert(g_adc_debug.samples==3U);
    puts("PASS OTA pause invalidates data and does not resume sampling itself");
    init(1000U);adc_sampler_poll(1020U);adc_sampler_poll(1250U);
    assert(g_adc_debug.phase==5U && g_adc_debug.error==2U && !g_adc_debug.valid && !g_adc_debug.samples);
    puts("PASS calibration timeout is bounded and never presented as valid zero current");
    init(2000U);AR(8)=0;AR(0x44)=ADC_CCSR_CALFAIL;adc_sampler_poll(2001U);adc_sampler_poll(2250U);
    assert(g_adc_debug.phase==5U && g_adc_debug.error==1U && !g_adc_debug.valid);
    puts("PASS calibration failure is reported without starting conversions");
    /* 寄存器模型显式模拟ISR写1清零；普通RAM本身不会实现W1C语义。 */
    init(3000U);calibrate();AR(0)=0U;now+=5U;adc_sampler_poll(now);adc_sampler_poll(3250U);
    assert(g_adc_debug.phase==5U && g_adc_debug.error==4U && !g_adc_debug.valid);
    puts("PASS conversion timeout has no infinite wait or false sample");
    init(4000U);calibrate();AR(0)=ADC_ISR_OVR;adc_sampler_poll(++now);adc_sampler_poll(4250U);
    assert(g_adc_debug.phase==5U && g_adc_debug.error==3U && !g_adc_debug.valid);
    puts("PASS conversion overrun invalidates data separately from timeout");
    init(0xFFFFFFF0U);calibrate();result(17U);now+=250U;adc_sampler_poll(now);
    assert(g_adc_debug.valid && g_adc_debug.raw==17U);
    puts("PASS tick wrap preserves calibration and conversion timing");
    memset((void *)adc_registers,0,sizeof(adc_registers));AR(8)=ADC_CR_ADCAL;AR(0x10)=0x12345678U;
    adc_sampler_init(5000U);assert(g_adc_debug.phase==7U && AR(0x10)==0x12345678U);
    AR(8)=0U;adc_sampler_poll(5001U);assert(AR(0x10)==(2U<<28U) && AR(8)==ADC_CR_ADCAL);
    puts("PASS startup calibration blocks configuration changes until hardware ready");
    init(6000U);calibrate();result(1U);now+=20U;adc_sampler_poll(now);
    assert(AR(8)==ADC_CR_ADEN);AR(8)=0U;now+=5U;adc_sampler_poll(now);now=6250U;adc_sampler_poll(now);
    assert(g_adc_debug.phase==5U && g_adc_debug.error==6U && !g_adc_debug.valid);
    puts("PASS re-enable failure is bounded and invalidates previously good data");
    return 0;
}
