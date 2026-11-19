#include "py32f0xx.h"
#include "board_config.h"

/* 调试器可观察的内存信息；本阶段没有业务状态机，也没有喂食动作。 */
typedef struct {
    uint32_t magic;
    uint32_t uptime_ms;
    uint32_t core_clock_hz;
    uint32_t hardware_not_validated;
    uint32_t fault_code;
} FeederEnvDebug;

volatile FeederEnvDebug g_feeder_debug = {0x46454544U, 0U, 0U, 1U, 0U};
__attribute__((used, section(".fw_metadata")))
const char g_feeder_version[] = "FEEDER " FEEDER_FIRMWARE_VERSION " " FEEDER_TARGET_PART;

void SysTick_Handler(void)
{
    /* 仅用于后续观察毫秒节拍；未接板验证时不宣称真实时序正确。 */
    g_feeder_debug.uptime_ms++;
}

void HardFault_Handler(void)
{
    /* 当前没有配置任何执行器；后续加入驱动时必须补独立硬件停机路径。 */
    __disable_irq();
    g_feeder_debug.fault_code = 1U;
    for (;;) {
        __WFI();
    }
}

int main(void)
{
    /* 前置条件：这里只是离线构建验证，尚未批准本固件上板。
     * 不初始化电机、蜂鸣器、显示或外部通信，不使用示例开发板的 LED 引脚。
     * GPIO 保持复位状态不等于整机已具备安全保护，仍需硬件确认。
     */
    SystemCoreClockUpdate();
    g_feeder_debug.core_clock_hz = SystemCoreClock;
    if (SysTick_Config(SystemCoreClock / 1000U) != 0U) {
        g_feeder_debug.fault_code = 2U;
        for (;;) {
            __WFI();
        }
    }
    for (;;) {
        __WFI();
    }
}
