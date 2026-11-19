#include "py32f0xx.h"
#include "motor_off.h"
#include "ota_layout.h"
/* 固定Boot不处理联网、不驱动电机，仅验证并安装完整镜像。 */
static void locked(void) { for(;;) { motor_off_hold();ota_watchdog(); } }
void NMI_Handler(void) { locked(); }
void HardFault_Handler(void) { locked(); }
__attribute__((naked,noreturn)) static void enter_app(uint32_t stack,uint32_t reset)
{
    (void)stack;(void)reset;
    __asm volatile("msr msp,r0\n cpsie i\n bx r1\n");
}
int main(void)
{
    motor_off_hold();
    if(!ota_boot_install()) { locked(); }
    __disable_irq();SysTick->CTRL=0U;
    NVIC->ICER[0]=0xFFFFFFFFU;NVIC->ICPR[0]=0xFFFFFFFFU;
    SCB->ICSR=SCB_ICSR_PENDSTCLR_Msk|SCB_ICSR_PENDSVCLR_Msk;
    SCB->VTOR=OTA_APP_BASE;__DSB();__ISB();
    const uint32_t *vectors=(const uint32_t *)OTA_APP_BASE;
    enter_app(vectors[0],vectors[1]);
}
