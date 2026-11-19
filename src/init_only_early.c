#include "motor_off.h"
#if defined(FEEDER_OTA_APP) || defined(FEEDER_APP_SLOT)
#include "py32f0xx.h"
#endif

void __real_SystemInit(void);

/* 链接器 --wrap=SystemInit 让官方启动文件先调用本函数，再调用官方SystemInit。
 * 无须修改厂商启动文件；本函数在.data复制和.bss清零之前执行，不能依赖全局变量。
 * 从复位到执行本函数之前的引脚状态仍由硬件决定，不能承诺该窗口绝不动作。
 */
void __wrap_SystemInit(void)
{
    motor_off_hold();
    __real_SystemInit();
#if defined(FEEDER_OTA_APP) || defined(FEEDER_APP_SLOT)
    /* 原厂SystemInit会恢复VTOR到Flash基址；应用必须随后指回自身中断表。 */
    SCB->VTOR=0x08002000U;__DSB();__ISB();
#endif
}
