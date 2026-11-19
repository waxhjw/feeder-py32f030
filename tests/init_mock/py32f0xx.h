#ifndef FEEDER_INIT_REGISTER_TEST_H
#define FEEDER_INIT_REGISTER_TEST_H
#include <stdint.h>

/* 电脑端寄存器替身，只验证写入范围与关闭值；不会访问真实单片机。 */
typedef struct {
    volatile uint32_t MODER, OTYPER, OSPEEDR, PUPDR, IDR, ODR, BSRR;
} GPIO_TypeDef;
typedef struct { volatile uint32_t IOPENR; } RCC_TypeDef;
extern GPIO_TypeDef test_gpioa;
extern RCC_TypeDef test_rcc;
#define GPIOA (&test_gpioa)
#define RCC (&test_rcc)
#define RCC_IOPENR_GPIOAEN 1UL
static inline void __DSB(void) { }
#endif
