#include "py32f0xx.h"
#include "touch_motor_config.h"
#include "touch_motor_io.h"
#include "motor_pulse.h"
#ifdef FEEDER_INPUT_DIAG
#include "motor_off.h"
#endif
int touch_motor_io_init(void)
{
    /* 复用已点动过的PA8/PA11初始化与关闭；本版持续运行使用普通GPIO，
     * 不通过扩大200ms单脉冲限制实现，不让旧计时器中断控制本版电机。 */
    if (!motor_pulse_init()) { return 0; }
    RCC->IOPENR|=RCC_IOPENR_GPIOBEN;
    (void)RCC->IOPENR;
    GPIOB->PUPDR&=~(3UL<<(2U*TOP1_PIN));
    GPIOB->MODER&=~(3UL<<(2U*TOP1_PIN));
    /* IWDG由独立LSI时钟驱动。主循环失效时约1秒后复位，重新进入默认停止；
     * 不把此近似时间称为实测精度，正常运行不按时长自动停机。 */
    RCC->CSR|=RCC_CSR_LSION;
    uint32_t count=100000U;
    while ((RCC->CSR&RCC_CSR_LSIRDY)==0U && --count) { }
    if (count==0U) { return 0; }
    DBGMCU->APBFZ1&=~DBGMCU_APB_FZ1_DBG_IWDG_STOP;
    IWDG->KR=0xCCCCU; IWDG->KR=0x5555U; IWDG->PR=3U; IWDG->RLR=1023U;
    count=100000U;
    while (IWDG->SR!=0U && --count) { }
    if (count==0U) { return 0; }
    IWDG->KR=0xAAAAU;
    return 1;
}
uint32_t touch_motor_level(void) { return (GPIOB->IDR>>TOP1_PIN)&1U; }
void touch_motor_output(uint32_t on)
{
#if defined(FEEDER_INPUT_DIAG) && !defined(FEEDER_LOCAL_UI)
    (void)on;motor_off_hold();return;
#endif
    if (on) {
        /* 与0.8.0相同A极性：INA/实际PA8高、INB/实际PA11低。
         * 一次BSRR同时设置两脚，不修改TOP1/PB7或另一个触摸键。 */
        GPIOA->BSRR=(1UL<<8U)|(1UL<<(11U+16U));
    } else { motor_pulse_stop(); }
}
void touch_motor_watchdog_feed(void) { IWDG->KR=0xAAAAU; }
#ifdef FEEDER_TOUCH_REVERSE
void touch_motor_output_direction(uint32_t on,uint32_t reverse)
{
#if defined(FEEDER_INPUT_DIAG) && !defined(FEEDER_LOCAL_UI)
    (void)on;(void)reverse;motor_off_hold();return;
#endif
    /* 前置：状态机已在换向间隔内持续输出00；此处不等待、不自动重试。
     * B极性为PA8低/PA11高，与A极性互换，绝不输出11刹车组合。 */
    if (on && reverse) { GPIOA->BSRR=(1UL<<11U)|(1UL<<(8U+16U)); }
    else { touch_motor_output(on); }
}
#endif
