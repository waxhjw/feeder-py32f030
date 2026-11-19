#include "../touch_mock/py32f0xx.h"
extern GPIO_TypeDef input_gpiof;
#define GPIOF (&input_gpiof)
#define RCC_IOPENR_GPIOFEN (1U<<5U)
#ifndef __DMB
#define __DMB() ((void)0)
#endif
#ifndef __DSB
#define __DSB() ((void)0)
#endif
