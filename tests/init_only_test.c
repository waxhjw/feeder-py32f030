#include <assert.h>
#include <stdio.h>
#include "py32f0xx.h"
#include "motor_off.h"

GPIO_TypeDef test_gpioa;
RCC_TypeDef test_rcc;
static unsigned int real_system_init_calls;

void __wrap_SystemInit(void);

/* 独立使用主图确定的实际位号，避免用被测模块的掩码生成同一份答案。 */
static void check_off(void)
{
    assert(test_gpioa.BSRR == 0x09000000UL);
    assert((test_gpioa.MODER & 0x00C30000UL) == 0x00410000UL);
    assert((test_gpioa.PUPDR & 0x00C30000UL) == 0x00820000UL);
    assert((test_gpioa.OTYPER & 0x900UL) == 0U);
    assert((test_gpioa.OSPEEDR & 0x00C30000UL) == 0U);
    assert((test_rcc.IOPENR & 1U) != 0U);
}

void __real_SystemInit(void)
{
    /* 必须在官方时钟初始化开始之前，已经完成两路电机控制脚关闭。 */
    check_off();
    real_system_init_calls++;
}

static void seed_registers(uint32_t value)
{
    test_gpioa.MODER = value;
    test_gpioa.OTYPER = value;
    test_gpioa.OSPEEDR = value;
    test_gpioa.PUPDR = value;
    test_gpioa.IDR = value;
    test_gpioa.ODR = value;
    test_gpioa.BSRR = value;
    test_rcc.IOPENR = value;
}

static void check_preserved(uint32_t original)
{
    uint32_t other_modes = ~0x00C30000UL;
    check_off();
    assert((test_gpioa.MODER & other_modes) == (original & other_modes));
    assert((test_gpioa.PUPDR & other_modes) == (original & other_modes));
    assert((test_gpioa.OSPEEDR & other_modes) == (original & other_modes));
    assert((test_gpioa.OTYPER & ~0x900UL) == (original & ~0x900UL));
    assert((test_rcc.IOPENR & ~1UL) == (original & ~1UL));
    /* 模拟寄存器不实现BSRR对ODR的硬件副作用，ODR/IDR变化仍需实板测量。 */
    assert(test_gpioa.ODR == original);
    assert(test_gpioa.IDR == original);
}

int main(void)
{
    seed_registers(0U);
    motor_off_hold();
    check_preserved(0U);
    puts("PASS reset registers become motor-off outputs");

    uint32_t value = 0xFFFFFFFFUL;
    for (unsigned int i = 0U; i < 128U; i++) {
        seed_registers(value);
        motor_off_hold();
        check_preserved(value);
        value = value * 1664525UL + 1013904223UL;
    }
    puts("PASS 128 dirty register patterns preserve non-motor and SWD bits");

    for (unsigned int i = 0U; i < 100U; i++) {
        motor_off_hold();
        check_off();
    }
    puts("PASS repeated shutdown never writes an output-set bit");

    seed_registers(0xFFFFFFFFUL);
    __wrap_SystemInit();
    check_preserved(0xFFFFFFFFUL);
    assert(real_system_init_calls == 1U);
    puts("PASS motor-off occurs before vendor SystemInit");
    return 0;
}
