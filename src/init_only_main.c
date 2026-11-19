#include "py32f0xx.h"
#include "init_only_config.h"
#include "motor_off.h"

typedef struct {
    uint32_t magic, firmware_id, uptime_ms, core_clock_hz;
    uint32_t state, fault, reset_flags, gpioa_moder, gpioa_odr, gpioa_idr;
} InitDebug;

/* 只读观察信息，没有命令字段；任何内存请求都不会触发电机动作。 */
volatile InitDebug g_init_debug = {0x46494E49UL, FEEDER_INIT_ID, 0U, 0U,
                                  0U, 0U, 0U, 0U, 0U, 0U};
__attribute__((used, section(".fw_metadata")))
const char g_feeder_version[] = "FEEDER " FEEDER_INIT_VERSION " " FEEDER_TARGET_PART;

void SysTick_Handler(void)
{
    g_init_debug.uptime_ms++;
}

static void fault_stop(uint32_t code)
{
    __disable_irq();
    g_init_debug.fault = code;
    g_init_debug.state = 2U; /* 故障保持：持续关闭，不自动重启、不自动恢复喂食。 */
    for (;;) {
        motor_off_hold();
    }
}

void NMI_Handler(void) { fault_stop(1U); }
void HardFault_Handler(void) { fault_stop(2U); }

int main(void)
{
    motor_off_hold();
    g_init_debug.reset_flags = RCC->CSR;
    SystemCoreClockUpdate();
    g_init_debug.core_clock_hz = SystemCoreClock;
    if (SysTick_Config(SystemCoreClock / 1000U) != 0U) {
        fault_stop(3U);
    }
    g_init_debug.state = 1U; /* 正常关闭：本版唯一正常运行状态。 */
    for (;;) {
        motor_off_hold();
        g_init_debug.gpioa_moder = GPIOA->MODER;
        g_init_debug.gpioa_odr = GPIOA->ODR;
        g_init_debug.gpioa_idr = GPIOA->IDR;
        /* 普通等待中断，1ms节拍后继续保持关闭；不进入STOP/STANDBY，保留SWD。
         * 不初始化电机定时器、红外、按键、显示、蜂鸣器或无线通信。
         */
        __WFI();
    }
}
