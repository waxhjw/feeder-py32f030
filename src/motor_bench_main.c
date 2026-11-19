#include "py32f0xx.h"
#include "motor_bench_config.h"
#include "motor_bench.h"
#include <stddef.h>

/* DAPLink只读状态/写入请求的固定格式。实际输出只由本地状态机决定。
 * 没有给App、云端、触摸键或红外开动作入口。
 */
typedef struct {
    uint32_t magic, abi, firmware_id, uptime_ms, state, fault;
    uint32_t command, checks, duration_ms, completed;
} BenchMailbox;
_Static_assert(sizeof(BenchMailbox) == 40U, "Mailbox size must match DAPLink tool");
_Static_assert(offsetof(BenchMailbox, command) == 24U, "Mailbox command offset mismatch");

volatile BenchMailbox g_motor_bench = {
    BENCH_MAILBOX_MAGIC, BENCH_MAILBOX_ABI, FEEDER_BENCH_ID,
    0U, BENCH_SETTLING, BENCH_OK, 0U, 0U, 0U, 0U
};
__attribute__((used, section(".fw_metadata")))
const char g_feeder_version[] = "FEEDER " FEEDER_BENCH_VERSION " " FEEDER_TARGET_PART;
static MotorBench bench;

void SysTick_Handler(void)
{
    g_motor_bench.uptime_ms++;
}

static void fault_stop(uint32_t code)
{
    motor_pulse_stop();
    __disable_irq();
    g_motor_bench.fault = code;
    g_motor_bench.state = BENCH_FAULT;
    for (;;) { /* 不再喂看门狗；若已启用，将复位且不会自动恢复点动。 */ }
}

void HardFault_Handler(void) { fault_stop(0x100U); }
void NMI_Handler(void) { fault_stop(0x101U); }

static bool watchdog_start(void)
{
    /* IWDG：Independent Watchdog，独立看门狗。
     * 主循环失去响应时复位。时间约125ms，随LSI误差变化，不能当精准脉宽。
     * 精准的单脉冲结束由TIM1完成；不把电脑断连作为唯一停机路径。
     */
    RCC->CSR |= RCC_CSR_LSION;
    uint32_t count = 100000U;
    while ((RCC->CSR & RCC_CSR_LSIRDY) == 0U && --count != 0U) { }
    if (count == 0U) { return false; }
    DBGMCU->APBFZ1 &= ~DBGMCU_APB_FZ1_DBG_IWDG_STOP;
    IWDG->KR = 0xCCCCU; /* 与厂商HAL一致：LSI就绪后先启动，再写配置。 */
    IWDG->KR = 0x5555U;
    IWDG->PR = 2U;
    IWDG->RLR = 255U;
    count = 100000U;
    while (IWDG->SR != 0U && --count != 0U) { }
    if (count == 0U) { return false; }
    IWDG->KR = 0xAAAAU;
    return true;
}

int main(void)
{
    /* 首次上电须断开电机，先验证无默认动作，再由操作者恢复连接。
     * 固件本身不能证明实际引脚、剩余行程、人员距离或物理断电路径。
     */
    bool ready = motor_pulse_init();
    motor_bench_init(&bench, ready ? 1U : 0U);
    if (!ready || SysTick_Config(SystemCoreClock / 1000U) != 0U || !watchdog_start()) {
        fault_stop(BENCH_INIT_FAILED);
    }
    for (;;) {
        uint32_t command = g_motor_bench.command;
        uint32_t checks = g_motor_bench.checks;
        uint32_t duration = g_motor_bench.duration_ms;
        if (command != 0U) {
            /* 请求只消费一次；停止、错误、完成之后均不能自动重启。 */
            g_motor_bench.command = 0U;
            g_motor_bench.checks = 0U;
            g_motor_bench.duration_ms = 0U;
        }
        motor_bench_step(&bench, g_motor_bench.uptime_ms, command, checks, duration);
        g_motor_bench.state = bench.state;
        g_motor_bench.fault = bench.fault;
        g_motor_bench.completed = bench.completed;
        IWDG->KR = 0xAAAAU;
        __WFI(); /* 只等中断，不进入会停止TIM1时钟的深度低功耗。 */
    }
}
