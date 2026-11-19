#include "py32f0xx.h"
#include "motor_bench_config.h"
#include "motor_pulse.h"

#define INA_PIN FEEDER_MOTOR_INA_ACTUAL_PA_PIN
#define INB_PIN FEEDER_MOTOR_INB_ACTUAL_PA_PIN
#define PIN_MASK ((1UL << INA_PIN) | (1UL << INB_PIN))
#define MODE_MASK ((3UL << (INA_PIN * 2U)) | (3UL << (INB_PIN * 2U)))
#define OUTPUT_MODE ((1UL << (INA_PIN * 2U)) | (1UL << (INB_PIN * 2U)))
#define AF_MODE ((2UL << (INA_PIN * 2U)) | (2UL << (INB_PIN * 2U)))
#define FORCE_INACTIVE 4UL
#define PWM_MODE_2 7UL

static bool initialized;

void motor_pulse_stop(void)
{
    /* TC118S 原厂逻辑：INA=0、INB=0 为高阻待机；这是撤销驱动，
     * 不是机械急刹，滑块是否还会惯性移动必须实测。
     * 先准备低电平，再退出定时器复用；不碰 SWD、红外或微动引脚。
     */
    RCC->IOPENR |= RCC_IOPENR_GPIOAEN;
    (void)RCC->IOPENR;
    GPIOA->BSRR = PIN_MASK << 16U;
    GPIOA->MODER = (GPIOA->MODER & ~MODE_MASK) | OUTPUT_MODE;
    if ((RCC->APBENR2 & RCC_APBENR2_TIM1EN) != 0U) {
        TIM1->BDTR = 0U;
        TIM1->CR1 = 0U;
        TIM1->CCER = 0U;
        TIM1->CNT = 0U;
    }
}

bool motor_pulse_init(void)
{
    initialized = false; /* 初始化失败后不能沿用之前的可启动状态。 */
    motor_pulse_stop();
    GPIOA->OTYPER &= ~PIN_MASK;
    GPIOA->OSPEEDR &= ~MODE_MASK;
    GPIOA->PUPDR = (GPIOA->PUPDR & ~MODE_MASK) | AF_MODE; /* 内部下拉 */
    SystemCoreClockUpdate();
    /* 当前只支持复位后的 8MHz HSI、AHB/APB 不分频；不猜时钟比例。 */
    if (SystemCoreClock != FEEDER_BENCH_CORE_HZ ||
        (RCC->CFGR & (RCC_CFGR_SWS | RCC_CFGR_HPRE | RCC_CFGR_PPRE)) != 0U) {
        return false;
    }
    RCC->APBENR2 |= RCC_APBENR2_TIM1EN;
    (void)RCC->APBENR2;
    RCC->APBRSTR2 |= RCC_APBRSTR2_TIM1RST;
    RCC->APBRSTR2 &= ~RCC_APBRSTR2_TIM1RST;
    /* 即使调试器暂停 CPU，也让 TIM1 继续完成单脉冲。
     * 不进入低功耗停钟；断电、时钟硬件故障不由此软件承诺兜底。
     */
    RCC->APBENR1 |= RCC_APBENR1_DBGEN;
    (void)RCC->APBENR1;
    DBGMCU->APBFZ2 &= ~DBGMCU_APB_FZ2_DBG_TIM1_STOP;
    GPIOA->AFR[1] = (GPIOA->AFR[1] & ~((15UL << 0U) | (15UL << 12U))) |
                    (FEEDER_MOTOR_AF << 0U) | (FEEDER_MOTOR_AF << 12U);
    initialized = true;
    return true;
}

bool motor_pulse_start(MotorPolarity polarity, uint32_t duration_ms)
{
    if (!initialized || motor_pulse_active() || duration_ms == 0U ||
        duration_ms > FEEDER_BENCH_MAX_PULSE_MS ||
        (polarity != MOTOR_POLARITY_A && polarity != MOTOR_POLARITY_B)) {
        motor_pulse_stop();
        return false;
    }
    motor_pulse_stop();
    /* TIM1 CH1 -> PA8/INA；CH4 -> PA11/INB，均为 AF2。
     * PWM2：CNT=0 时低；CNT>=1 时高；溢出回到0后低。
     * OPM（One Pulse Mode，单脉冲模式）在这次溢出后自行停止计数，
     * 所以关闭输出不依赖主循环、串口、电脑或中断及时运行。
     * 先等待1ms，再输出 duration_ms；绝不使用会在CNT=0仍高的PWM1。
     */
    TIM1->PSC = (FEEDER_BENCH_CORE_HZ / 1000U) - 1U;
    TIM1->ARR = duration_ms;
    TIM1->CCR1 = 1U;
    TIM1->CCR4 = 1U;
    TIM1->RCR = 0U;
    TIM1->CR2 = 0U;
    TIM1->SMCR = 0U;
    TIM1->DIER = 0U;
    TIM1->CCMR1 = ((polarity == MOTOR_POLARITY_A) ? PWM_MODE_2 : FORCE_INACTIVE)
                    << TIM_CCMR1_OC1M_Pos;
    TIM1->CCMR2 = ((polarity == MOTOR_POLARITY_B) ? PWM_MODE_2 : FORCE_INACTIVE)
                    << TIM_CCMR2_OC4M_Pos;
    TIM1->CR1 = TIM_CR1_OPM;
    TIM1->EGR = TIM_EGR_UG;
    TIM1->SR = 0U;
    TIM1->CNT = 0U;
    TIM1->CCER = TIM_CCER_CC1E | TIM_CCER_CC4E;
    TIM1->BDTR = TIM_BDTR_MOE;
    GPIOA->MODER = (GPIOA->MODER & ~MODE_MASK) | AF_MODE;
    TIM1->CR1 |= TIM_CR1_CEN;
    return true;
}

bool motor_pulse_active(void)
{
    return initialized && ((TIM1->CR1 & TIM_CR1_CEN) != 0U);
}
