#ifndef PUMP_REGISTER_MODEL_H
#define PUMP_REGISTER_MODEL_H
#include <stdint.h>
/* 仅主机寄存器模型；不能证明现场供电、脉宽或电机已转。 */
typedef struct { volatile uint32_t BSRR, MODER, OTYPER, OSPEEDR, PUPDR, AFR[2]; } GPIO_TypeDef;
typedef struct { volatile uint32_t IOPENR, APBENR1, APBENR2, APBRSTR2, CFGR; } RCC_TypeDef;
typedef struct { volatile uint32_t BDTR, CR1, CCER, CNT, PSC, ARR, CCR1, CCR4, RCR, CR2, SMCR, DIER, CCMR1, CCMR2, EGR, SR; } TIM_TypeDef;
typedef struct { volatile uint32_t APBFZ2; } DBG_TypeDef;
extern GPIO_TypeDef model_gpioa;
extern RCC_TypeDef model_rcc;
extern TIM_TypeDef model_tim1;
extern DBG_TypeDef model_dbg;
extern uint32_t SystemCoreClock;
void SystemCoreClockUpdate(void);
#define GPIOA (&model_gpioa)
#define RCC (&model_rcc)
#define TIM1 (&model_tim1)
#define DBGMCU (&model_dbg)
#define RCC_IOPENR_GPIOAEN 1U
#define RCC_APBENR2_TIM1EN (1U << 11U)
#define RCC_APBRSTR2_TIM1RST (1U << 11U)
#define RCC_APBENR1_DBGEN (1U << 27U)
#define RCC_CFGR_SWS 0x38U
#define RCC_CFGR_HPRE 0xF00U
#define RCC_CFGR_PPRE 0x7000U
#define DBGMCU_APB_FZ2_DBG_TIM1_STOP (1U << 11U)
#define TIM_CCMR1_OC1M_Pos 4U
#define TIM_CCMR2_OC4M_Pos 12U
#define TIM_CR1_CEN 1U
#define TIM_CR1_OPM 8U
#define TIM_EGR_UG 1U
#define TIM_CCER_CC1E 1U
#define TIM_CCER_CC4E (1U << 12U)
#define TIM_BDTR_MOE (1U << 15U)
#endif
