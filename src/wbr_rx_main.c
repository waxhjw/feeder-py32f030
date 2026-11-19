#include "py32f0xx.h"
#include "wbr_rx_config.h"
#include "motor_off.h"
#include "wbr_uart_rx.h"
#include "wbr_rx_observer.h"

typedef struct {
    uint32_t magic, abi, firmware_id, size, sequence, uptime_ms, core_clock_hz;
    uint32_t phase, baud, fault, gpioa_moder, gpioa_odr, gpiof_moder, gpiof_afr;
    uint32_t uart_cr1, uart_brr, rx_pin, dropped, reserved[2];
    WbrRxRate rates[2];
} WbrRxDebug;
_Static_assert(sizeof(WbrRxRate) == 168U, "Update reader ABI after changing rate data");
_Static_assert(sizeof(WbrRxDebug) == 416U, "Update reader ABI after changing debug data");

/* 无命令字段；写内存也没有对应的执行器动作入口。 */
volatile WbrRxDebug g_wbr_rx_debug = {
    .magic = FEEDER_WBR_DEBUG_MAGIC, .abi = 1U,
    .firmware_id = FEEDER_WBR_ID, .size = sizeof(WbrRxDebug)
};
__attribute__((used, section(".fw_metadata")))
const char g_feeder_version[] = "FEEDER " FEEDER_WBR_VERSION " " FEEDER_TARGET_PART;
static volatile uint32_t ticks;
static WbrRxObserver observer;

void SysTick_Handler(void) { ticks++; }

static void fault_stop(uint32_t code)
{
    __disable_irq();
    g_wbr_rx_debug.fault = code;
    if ((g_wbr_rx_debug.sequence & 1U) != 0U) { g_wbr_rx_debug.sequence++; }
    for (;;) { motor_off_hold(); }
}
void NMI_Handler(void) { fault_stop(1U); }
void HardFault_Handler(void) { fault_stop(2U); }

static void publish(void)
{
    /* 序号为奇数时快照未完成；电脑前后核对序号，避免读取混合数据。
     * 中断只入队，不修改observer，不必暂停串口或整机来复制快照。
     */
    g_wbr_rx_debug.sequence++;
    __DMB();
    g_wbr_rx_debug.uptime_ms = ticks;
    g_wbr_rx_debug.core_clock_hz = SystemCoreClock;
    g_wbr_rx_debug.phase = observer.phase;
    g_wbr_rx_debug.baud = observer.rates[observer.phase].baud;
    g_wbr_rx_debug.gpioa_moder = GPIOA->MODER;
    g_wbr_rx_debug.gpioa_odr = GPIOA->ODR;
    g_wbr_rx_debug.gpiof_moder = GPIOF->MODER;
    g_wbr_rx_debug.gpiof_afr = GPIOF->AFR[0];
    g_wbr_rx_debug.uart_cr1 = USART2->CR1;
    g_wbr_rx_debug.uart_brr = USART2->BRR;
    g_wbr_rx_debug.rx_pin = GPIOF->IDR & 1U;
    g_wbr_rx_debug.dropped = wbr_uart_rx_dropped();
    g_wbr_rx_debug.rates[0] = observer.rates[0];
    g_wbr_rx_debug.rates[1] = observer.rates[1];
    __DMB();
    g_wbr_rx_debug.sequence++;
}

int main(void)
{
    motor_off_hold();
    SystemCoreClockUpdate();
    /* 本版沿用官方HSI 8MHz，APB不分频；不启用PF0/PF1外部高速晶振。 */
    if (SystemCoreClock != 8000000U || (RCC->CFGR & RCC_CFGR_PPRE) != 0U) { fault_stop(3U); }
    if (SysTick_Config(SystemCoreClock / 1000U) != 0U) { fault_stop(4U); }
    wbr_rx_observer_init(&observer);
    if (!wbr_uart_rx_init(SystemCoreClock, 9600U)) { fault_stop(5U); }
    publish();
    uint32_t last_switch = ticks, last_publish = ticks, last_byte = ticks;
    for (;;) {
        motor_off_hold();
        if ((uint32_t)(ticks - last_byte) >= 100U) { wbr_rx_observer_discontinuity(&observer); }
        uint16_t event;
        for (uint32_t i = 0U; i < 64U && wbr_uart_rx_pop(&event); ++i) {
            if ((event & 0x1000U) != 0U) { wbr_rx_observer_discontinuity(&observer); }
            wbr_rx_observer_feed(&observer, (uint8_t)event, (event >> 8U) & 0xFU);
            last_byte = ticks;
        }
        uint32_t now = ticks;
        if ((uint32_t)(now - last_switch) >= FEEDER_WBR_SCAN_MS) {
            uint32_t next = observer.phase ^ 1U;
            if (!wbr_uart_rx_set_baud(SystemCoreClock, observer.rates[next].baud)) { fault_stop(6U); }
            wbr_rx_observer_select(&observer, next);
            last_switch = now;
        }
        if ((uint32_t)(now - last_publish) >= 1000U) { publish(); last_publish = now; }
        __WFI(); /* 1ms节拍唤醒；不进入STOP，不发配网、产品信息或电机命令。 */
    }
}
