#include "py32f0xx.h"
#include "bench_panel.h"
#include "bench_buzzer.h"

#define BUZZER_PIN (1UL<<5U)
#define BUZZER_GPIO_MASK (3UL<<10U)
static uint32_t mode;

void bench_buzzer_init(void)
{
    RCC->IOPENR|=RCC_IOPENR_GPIOBEN;
    RCC->APBENR1|=RCC_APBENR1_TIM3EN;(void)RCC->APBENR1;
    /* 主图第3页PB5经1k控制Q1，低为关。先置低再设输出。 */
    GPIOB->BSRR=BUZZER_PIN<<16U;
    GPIOB->OTYPER&=~BUZZER_PIN;
    GPIOB->PUPDR&=~BUZZER_GPIO_MASK;
    GPIOB->MODER=(GPIOB->MODER&~BUZZER_GPIO_MASK)|(1UL<<10U);
    GPIOB->AFR[0]=(GPIOB->AFR[0]&~(15UL<<20U))|(1UL<<20U); /* PB5 AF1 TIM3_CH2 */
    RCC->APBRSTR1|=RCC_APBRSTR1_TIM3RST;RCC->APBRSTR1&=~RCC_APBRSTR1_TIM3RST;
    TIM3->PSC=7U;TIM3->ARR=399U;TIM3->CCR2=200U; /* 8MHz→1MHz，2.5kHz，50% */
    TIM3->CCMR1=TIM_CCMR1_OC2M_1|TIM_CCMR1_OC2M_2|TIM_CCMR1_OC2PE;
    TIM3->EGR=TIM_EGR_UG;TIM3->SR=0U;mode=BUZZER_OFF;
}
void bench_buzzer_set(uint32_t next)
{
    if (next==mode || !(RCC->APBENR1&RCC_APBENR1_TIM3EN)) { return; }
    GPIOB->BSRR=BUZZER_PIN<<16U;
    GPIOB->MODER=(GPIOB->MODER&~BUZZER_GPIO_MASK)|(1UL<<10U);
    TIM3->CCER=0U;TIM3->CR1=0U;
    if (next==BUZZER_TONE) {
        TIM3->CNT=0U;TIM3->CCER=TIM_CCER_CC2E;TIM3->CR1=TIM_CR1_CEN;
        GPIOB->MODER=(GPIOB->MODER&~BUZZER_GPIO_MASK)|(2UL<<10U);
    }
    mode=next;
}
