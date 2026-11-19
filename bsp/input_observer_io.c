#include "py32f0xx.h"
#include "wbr_init_config.h"
#include "input_observer.h"
#include "input_observer_io.h"
#ifdef FEEDER_ADC_DIAG
#include "adc_sampler.h"
#endif
volatile InputDebug g_input_debug={.magic=0x494E5054U,.abi=1U,.firmware_id=FEEDER_WBR_INIT_ID,.size=128U};
_Static_assert(sizeof(InputDebug)==128U,"Input reader ABI");
static InputObserver observer;
static uint32_t published;
void input_observer_io_init(void)
{
    RCC->IOPENR|=RCC_IOPENR_GPIOFEN|RCC_IOPENR_GPIOBEN;(void)RCC->IOPENR;
    /* 主图第3页：CN5/CN6共PF3，闭合接地，图上未给独立上拉。
     * 本诊断启用PF3弱上拉，只检查通断；不能分辨断线与开路。
     * PB6/PB7由触摸IC输出且PB7共电源分压，不加上下拉、不启用ADC。 */
    GPIOF->MODER&=~(3UL<<6U);
    GPIOF->PUPDR=(GPIOF->PUPDR&~(3UL<<6U))|(1UL<<6U);
    GPIOB->MODER&=~((3UL<<12U)|(3UL<<14U));
    GPIOB->PUPDR&=~((3UL<<12U)|(3UL<<14U));
}
void input_observer_io_poll(unsigned now)
{
    uint32_t levels=((GPIOF->IDR>>3U)&1U)|(((GPIOB->IDR>>7U)&1U)<<1U)|(((GPIOB->IDR>>6U)&1U)<<2U);
    input_observer_step(&observer,now,levels);
    if(g_input_debug.sequence && (uint32_t)(now-published)<250U) { return; }
    g_input_debug.sequence++;__DMB();
    g_input_debug.uptime_ms=now;g_input_debug.samples=observer.samples;
    g_input_debug.max_gap=observer.max_gap;g_input_debug.gaps=observer.gaps;
    for(uint32_t i=0U;i<3U;i++) {
        const InputChannel *c=&observer.channel[i];
        g_input_debug.channel[i][0]=c->raw;g_input_debug.channel[i][1]=c->stable;
        g_input_debug.channel[i][2]=c->valid;g_input_debug.channel[i][3]=c->rises;
        g_input_debug.channel[i][4]=c->falls;g_input_debug.channel[i][5]=c->last_ms;
    }
    g_input_debug.gpiof_moder=GPIOF->MODER;g_input_debug.gpiof_pupdr=GPIOF->PUPDR;
    g_input_debug.gpiob_moder=GPIOB->MODER;g_input_debug.gpiob_pupdr=GPIOB->PUPDR;
#ifdef FEEDER_ADC_DIAG
    g_input_debug.adc_status=g_adc_debug.phase; /* 状态不等于实际电流，读独立ADC快照。 */
#else
    g_input_debug.adc_status=0U; /* 未配置，不伪造零电流或有效ADC采样。 */
#endif
    __DMB();g_input_debug.sequence++;published=now;
}
