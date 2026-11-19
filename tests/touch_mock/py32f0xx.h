#ifndef TOUCH_REGISTER_MODEL_H
#define TOUCH_REGISTER_MODEL_H
/* 沿用电机寄存器模型并补充本轮触摸输入与独立看门狗；不是实物测量。 */
#define GPIO_TypeDef BaseGPIO_TypeDef
#define RCC_TypeDef BaseRCC_TypeDef
#define DBG_TypeDef BaseDBG_TypeDef
#include "../pump_mock/py32f0xx.h"
#undef GPIO_TypeDef
#undef RCC_TypeDef
#undef DBG_TypeDef
typedef struct { volatile uint32_t BSRR, MODER, OTYPER, OSPEEDR, PUPDR, AFR[2], IDR, ODR; } GPIO_TypeDef;
typedef struct { volatile uint32_t IOPENR, APBENR1, APBENR2, APBRSTR2, CFGR, CSR; } RCC_TypeDef;
typedef struct { volatile uint32_t APBFZ2, APBFZ1; } DBG_TypeDef;
typedef struct { volatile uint32_t KR, PR, RLR, SR; } IWDG_TypeDef;
/* 基础头的extern使用基础类型，改用独立实例避免类型重定义。 */
extern GPIO_TypeDef touch_gpioa, touch_gpiob;
extern RCC_TypeDef touch_rcc;
extern DBG_TypeDef touch_dbg;
extern IWDG_TypeDef touch_iwdg;
#undef GPIOA
#undef RCC
#undef DBGMCU
#define GPIOA (&touch_gpioa)
#define GPIOB (&touch_gpiob)
#define RCC (&touch_rcc)
#define DBGMCU (&touch_dbg)
#define IWDG (&touch_iwdg)
#define RCC_IOPENR_GPIOBEN 2U
#define RCC_CSR_LSION 1U
#define RCC_CSR_LSIRDY 2U
#define DBGMCU_APB_FZ1_DBG_IWDG_STOP (1U<<12U)
#endif
