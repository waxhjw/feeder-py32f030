#include "py32f0xx.h"
#include "wbr_init_config.h"
#include "adc_sampler.h"
#include <string.h>
/* SDK1.5.0 ADC_TempSensor_Init五次校准/有符号23位中值路径。
 * 0x48/4c/50/54为SDK使用的扩展校准寄存器，不套其他芯片ADC算法。
 * 所有等待均分散在主循环，超时只标不可用，绝不输出伪造的零电流。 */
#define AR(offset) (*(volatile uint32_t *)(ADC1_BASE+(offset)))
static struct {
    uint32_t phase,valid,raw,minimum,maximum,samples,error,count,since,last_sample,converting,published;
    int32_t calibrations[5];
    uint32_t median;
} s;
volatile AdcDebug g_adc_debug={.magic=0x41444344U,.abi=1U,.firmware_id=FEEDER_WBR_INIT_ID,.size=64U};
_Static_assert(sizeof(AdcDebug)==64U,"ADC reader ABI");
static void publish(uint32_t now)
{
    g_adc_debug.sequence++;__DMB();
    g_adc_debug.uptime_ms=now;g_adc_debug.phase=s.phase;g_adc_debug.valid=s.valid;
    g_adc_debug.raw=s.raw;g_adc_debug.minimum=s.minimum;g_adc_debug.maximum=s.maximum;
    g_adc_debug.samples=s.samples;g_adc_debug.error=s.error;g_adc_debug.calibrations=s.count;
    g_adc_debug.calibration_median=s.median;g_adc_debug.gpioa_moder=GPIOA->MODER;
    __DMB();g_adc_debug.sequence++;s.published=now;
}
static void fail(uint32_t code) { s.error=code;s.phase=ADC_DIAG_ERROR;s.valid=0U; }
static uint32_t reset_ready(void)
{
    return (AR(0x08)&(ADC_CR_ADCAL|ADC_CR_ADSTART|ADC_CR_ADSTP|ADC_CR_ADEN))==0U &&
           (AR(0x44)&0x80000000U)==0U;
}
static void start_calibration(uint32_t now)
{
    /* 参考手册154/155/175页：自动上电校准结束且ADC禁止，才改时钟和配置。 */
    AR(0x04)=0U;AR(0x0C)=0U;AR(0x10)=ADC_CFGR2_CKMODE_1;
    AR(0x14)=ADC_SMPR_SMP;AR(0x28)=ADC_CHSELR_CHSEL3;
    AR(0x08)=ADC_CR_ADCAL;s.phase=ADC_DIAG_CAL;s.since=now;
}
void adc_sampler_init(uint32_t now)
{
    memset(&s,0,sizeof(s));
    s.phase=ADC_DIAG_CAL;s.since=now;s.minimum=4095U;
    RCC->IOPENR|=RCC_IOPENR_GPIOAEN;(void)RCC->IOPENR;
    /* PA3/物理6脚=MD-ADC，PB7仍是触摸输入，不采PB7。 */
    GPIOA->MODER|=3UL<<6U;GPIOA->PUPDR&=~(3UL<<6U);
    RCC->APBENR2|=RCC_APBENR2_ADCEN;RCC->APBRSTR2|=RCC_APBRSTR2_ADCRST;
    RCC->APBRSTR2&=~RCC_APBRSTR2_ADCRST;(void)RCC->APBENR2;
    s.phase=ADC_DIAG_WAIT_RESET; /* PCLK8MHz/4=2MHz，需等硬件校准结束才设置。 */
    if(reset_ready()) { start_calibration(now); }
    publish(now);
}
void adc_sampler_pause(uint32_t now)
{
    if(AR(0x08)&ADC_CR_ADSTART) {
#ifdef FEEDER_ADC_RS_FIX
        AR(0x08)=ADC_CR_ADSTP; /* RS位写0无影响；只提交停止命令，不能重发ADEN。 */
#else
        AR(0x08)=(AR(0x08)&~(ADC_CR_ADCAL|ADC_CR_ADSTART|ADC_CR_ADSTP))|ADC_CR_ADSTP;
#endif
    }
    s.valid=0U;s.phase=ADC_DIAG_PAUSED;publish(now);
}
void adc_sampler_poll(uint32_t now)
{
    if(s.phase==ADC_DIAG_WAIT_RESET) {
        if(reset_ready()) { start_calibration(now); }
        else if((uint32_t)(now-s.since)>=20U) { fail(5U); }
    } else if(s.phase==ADC_DIAG_CAL) {
        if((AR(0x08)&ADC_CR_ADCAL)==0U) {
            if(AR(0x44)&ADC_CCSR_CALFAIL) { fail(1U); }
            else {
                uint32_t v=AR(0x48)&0x7FFFFFU;
                s.calibrations[s.count++]=(int32_t)(v^0x400000U)-(int32_t)0x400000;
                if(s.count<5U) { AR(0x08)=ADC_CR_ADCAL;s.since=now; }
                else {
                    for(uint32_t i=1U;i<5U;i++) {
                        int32_t value=s.calibrations[i];uint32_t j=i;
                        while(j && s.calibrations[j-1U]>value) { s.calibrations[j]=s.calibrations[j-1U];j--; }
                        s.calibrations[j]=value;
                    }
                    s.median=(uint32_t)s.calibrations[2];AR(0x50)=s.median;AR(0x54)=AR(0x4C);
                    AR(0x44)|=0x8000U;s.phase=ADC_DIAG_WAIT_CAL;s.since=now;
                }
            }
        } else if((uint32_t)(now-s.since)>=20U) { fail(2U); }
    } else if(s.phase==ADC_DIAG_WAIT_CAL && (uint32_t)(now-s.since)>=1U) {
        AR(0x08)=ADC_CR_ADEN;s.phase=ADC_DIAG_WAIT_ENABLE;s.since=now;
    } else if(s.phase==ADC_DIAG_WAIT_ENABLE && (uint32_t)(now-s.since)>=1U) {
#ifdef FEEDER_ADC_REARM
        /* 实板单次转换后ADEN回0；使能等待有界，不把上一次样本冒充新转换。 */
        if((AR(0x08)&ADC_CR_ADEN)==0U) {
            if((uint32_t)(now-s.since)>=5U) { fail(6U); }
        } else { s.phase=ADC_DIAG_SAMPLE;s.last_sample=now-20U; }
#else
        s.phase=ADC_DIAG_SAMPLE;s.last_sample=now-20U;
#endif
    } else if(s.phase==ADC_DIAG_SAMPLE) {
        if(s.converting) {
            if(AR(0x00)&ADC_ISR_OVR) { fail(3U); }
            else if(AR(0x00)&ADC_ISR_EOC) {
                s.raw=AR(0x40)&4095U;AR(0x00)=ADC_ISR_EOC|ADC_ISR_EOSEQ|ADC_ISR_OVR;
                if(s.raw<s.minimum) { s.minimum=s.raw; }
                if(s.raw>s.maximum) { s.maximum=s.raw; }
                s.samples++;s.valid=1U;s.converting=0U;
            } else if((uint32_t)(now-s.since)>=5U) { fail(4U); }
        } else if((uint32_t)(now-s.last_sample)>=20U) {
#ifdef FEEDER_ADC_REARM
            /* 与SDK APP_AdcConvert一致，每次启动前确认已使能并等待稳定。 */
            if((AR(0x08)&ADC_CR_ADEN)==0U) {
                AR(0x08)=ADC_CR_ADEN;s.phase=ADC_DIAG_WAIT_ENABLE;s.since=now;
            } else {
#endif
            AR(0x00)=ADC_ISR_EOC|ADC_ISR_EOSEQ|ADC_ISR_OVR;
#ifdef FEEDER_ADC_RS_FIX
            /* 与SDK LL_ADC_REG_StartConversion一致：其他RS位写0，保留硬件ADEN状态。 */
            AR(0x08)=ADC_CR_ADSTART;
#else
            AR(0x08)=ADC_CR_ADEN|ADC_CR_ADSTART;
#endif
            s.converting=1U;s.since=now;s.last_sample=now;
#ifdef FEEDER_ADC_REARM
            }
#endif
        }
    }
    if((uint32_t)(now-s.published)>=250U) { publish(now); }
}
