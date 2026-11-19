#ifndef LED_HOST_REGISTER_MODEL_H
#define LED_HOST_REGISTER_MODEL_H
#include <stdint.h>
/* 仅模型，不替代真实GPIO波形、供电及视觉验收。 */
typedef struct { volatile uint32_t BSRR, OTYPER, PUPDR, OSPEEDR, MODER; } GPIO_TypeDef;
typedef struct { volatile uint32_t CFGR, IOPENR, APBENR2, APBRSTR2; } RCC_TypeDef;
typedef struct { volatile uint32_t CNT, SR, DIER, CR1, PSC, ARR, EGR; } TIM_TypeDef;
extern GPIO_TypeDef model_gpio;
extern RCC_TypeDef model_rcc;
extern TIM_TypeDef model_tim;
#define GPIOB (&model_gpio)
#define RCC (&model_rcc)
#define TIM14 (&model_tim)
#define TIM14_IRQn 19
#define RCC_CFGR_PPRE 0x700U
#define RCC_IOPENR_GPIOBEN 2U
#define RCC_APBENR2_TIM14EN 0x8000U
#define RCC_APBRSTR2_TIM14RST 0x8000U
#define TIM_DIER_UIE 1U
#define TIM_CR1_CEN 1U
#define TIM_EGR_UG 1U
static inline void __DMB(void) { }
static inline void NVIC_ClearPendingIRQ(int irq) { (void)irq; }
static inline void NVIC_DisableIRQ(int irq) { (void)irq; }
static inline void NVIC_EnableIRQ(int irq) { (void)irq; }
static inline void NVIC_SetPriority(int irq, unsigned value) { (void)irq; (void)value; }
#endif
