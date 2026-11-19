#include "py32f0xx.h"
#include "init_only_config.h"
#include "motor_off.h"

void motor_off_hold(void)
{
    /* TC118S资料：INA=0、INB=0为高阻待机，撤销电机驱动，不是机械刹车。
     * 先把输出锁存值置低，再切成普通输出，避免先切输出再置低。
     * 仅操作实际PA8/PA11，保留PA13/PA14调试口及所有其他管脚。
     */
    RCC->IOPENR |= RCC_IOPENR_GPIOAEN;
    (void)RCC->IOPENR;
    GPIOA->BSRR = FEEDER_INIT_MOTOR_MASK << 16U;
    GPIOA->OTYPER &= ~FEEDER_INIT_MOTOR_MASK;
    GPIOA->OSPEEDR &= ~FEEDER_INIT_MOTOR_MODE_MASK;
    GPIOA->PUPDR = (GPIOA->PUPDR & ~FEEDER_INIT_MOTOR_MODE_MASK) |
                   FEEDER_INIT_MOTOR_PULL_DOWN;
    GPIOA->MODER = (GPIOA->MODER & ~FEEDER_INIT_MOTOR_MODE_MASK) |
                   FEEDER_INIT_MOTOR_OUTPUT_MODE;
    __DSB();
}
