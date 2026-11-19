#include "py32f0xx.h"
#include "wbr_heartbeat_config.h"
#include "motor_off.h"
#include "wbr_uart_rx.h"
#include "wbr_uart_hb.h"
#include "wbr_heartbeat.h"

typedef struct {
    uint32_t magic, abi, firmware_id, size, sequence, uptime_ms, core_clock_hz;
    uint32_t phase, baud, fault, gpioa_moder, gpioa_odr, gpiof_moder, gpiof_afr;
    uint32_t uart_cr1, uart_brr, dropped, state, rx_bytes, frames, heartbeats, replies;
    uint32_t product_queries, ignored, uart_errors, bad_checksum, too_long;
    uint32_t first_reply_value, last_reply_value, first_reply_ms, query_ms, last_rx_ms;
    uint32_t max_reply_ms, tx_failures, first_reply_frame_len, first_query_frame_len;
    uint8_t first_reply[8], first_query[8];
} WbrHeartbeatDebug;
_Static_assert(sizeof(WbrHeartbeatDebug) == 160U, "Update debug reader ABI");
volatile WbrHeartbeatDebug g_wbr_heartbeat_debug = {
    .magic = FEEDER_WBR_HB_MAGIC, .abi = 1U, .firmware_id = FEEDER_WBR_HB_ID,
    .size = sizeof(WbrHeartbeatDebug)
};
__attribute__((used, section(".fw_metadata")))
const char g_feeder_version[] = "FEEDER " FEEDER_WBR_HB_VERSION " " FEEDER_TARGET_PART;
static volatile uint32_t ticks;
static WbrHeartbeat link;
static uint32_t phase;
void SysTick_Handler(void) { ticks++; }

static void fault_stop(uint32_t code)
{
    __disable_irq();
    motor_off_hold();
    wbr_uart_hb_disable();
    g_wbr_heartbeat_debug.fault = code;
    if ((g_wbr_heartbeat_debug.sequence & 1U) != 0U) { g_wbr_heartbeat_debug.sequence++; }
    for (;;) { motor_off_hold(); }
}
void NMI_Handler(void) { fault_stop(1U); }
void HardFault_Handler(void) { fault_stop(2U); }

static void publish(void)
{
    g_wbr_heartbeat_debug.sequence++;
    __DMB();
    g_wbr_heartbeat_debug.uptime_ms = ticks;
    g_wbr_heartbeat_debug.core_clock_hz = SystemCoreClock;
    g_wbr_heartbeat_debug.phase = phase;
    g_wbr_heartbeat_debug.baud = phase == 0U ? 9600U : 115200U;
    g_wbr_heartbeat_debug.gpioa_moder = GPIOA->MODER;
    g_wbr_heartbeat_debug.gpioa_odr = GPIOA->ODR;
    g_wbr_heartbeat_debug.gpiof_moder = GPIOF->MODER;
    g_wbr_heartbeat_debug.gpiof_afr = GPIOF->AFR[0];
    g_wbr_heartbeat_debug.uart_cr1 = USART2->CR1;
    g_wbr_heartbeat_debug.uart_brr = USART2->BRR;
    g_wbr_heartbeat_debug.dropped = wbr_uart_rx_dropped();
    g_wbr_heartbeat_debug.state = link.state;
    g_wbr_heartbeat_debug.rx_bytes = link.rx_bytes;
    g_wbr_heartbeat_debug.frames = link.parser.frames;
    g_wbr_heartbeat_debug.heartbeats = link.heartbeats;
    g_wbr_heartbeat_debug.replies = link.replies;
    g_wbr_heartbeat_debug.product_queries = link.product_queries;
    g_wbr_heartbeat_debug.ignored = link.ignored;
    g_wbr_heartbeat_debug.uart_errors = link.uart_errors;
    g_wbr_heartbeat_debug.bad_checksum = link.parser.bad_checksum;
    g_wbr_heartbeat_debug.too_long = link.parser.too_long;
    g_wbr_heartbeat_debug.first_reply_value = link.first_reply_value;
    g_wbr_heartbeat_debug.last_reply_value = link.last_reply_value;
    g_wbr_heartbeat_debug.first_reply_ms = link.first_reply_ms;
    g_wbr_heartbeat_debug.query_ms = link.query_ms;
    g_wbr_heartbeat_debug.last_rx_ms = link.last_rx_ms;
    g_wbr_heartbeat_debug.max_reply_ms = link.max_reply_ms;
    g_wbr_heartbeat_debug.first_reply_frame_len = link.replies != 0U ? 8U : 0U;
    g_wbr_heartbeat_debug.first_query_frame_len = link.product_queries != 0U ? 7U : 0U;
    for (uint32_t i = 0U; i < 8U; ++i) {
        g_wbr_heartbeat_debug.first_reply[i] = link.first_reply[i];
        g_wbr_heartbeat_debug.first_query[i] = link.first_query[i];
    }
    __DMB();
    g_wbr_heartbeat_debug.sequence++;
}

static void poll_tx(void)
{
    int result = wbr_uart_hb_poll(ticks);
    if (result < 0) { g_wbr_heartbeat_debug.tx_failures++; fault_stop(7U); }
    if (result > 0) { wbr_heartbeat_sent(&link, ticks); }
}

int main(void)
{
    motor_off_hold(); /* 所有通信以前仍先关闭实际PA8/PA11电机驱动。 */
    SystemCoreClockUpdate();
    if (SystemCoreClock != 8000000U || (RCC->CFGR & RCC_CFGR_PPRE) != 0U) { fault_stop(3U); }
    if (SysTick_Config(SystemCoreClock / 1000U) != 0U) { fault_stop(4U); }
    wbr_heartbeat_init(&link);
    if (!wbr_uart_rx_init(SystemCoreClock, 9600U)) { fault_stop(5U); }
    wbr_uart_hb_reset();
    publish();
    uint32_t last_switch = ticks, last_publish = ticks;
    for (;;) {
        motor_off_hold();
        poll_tx();
        if ((uint32_t)(ticks - link.last_rx_ms) >= 100U) { wbr_heartbeat_gap(&link); }
        uint16_t event;
        for (uint32_t i = 0U; i < 64U && wbr_uart_rx_pop(&event); ++i) {
            poll_tx(); /* 完成发送记录先于随后到来的产品查询。 */
            if ((event & 0x1000U) != 0U) { wbr_heartbeat_gap(&link); }
            uint8_t reply;
            if (wbr_heartbeat_feed(&link, (uint8_t)event, (event >> 8U) & 15U, ticks, &reply)) {
                if (!wbr_uart_hb_start(reply, ticks)) { fault_stop(8U); }
            }
        }
        if (wbr_uart_rx_dropped() != 0U) { fault_stop(9U); }
        uint32_t now = ticks;
        if (link.state == WBR_HB_LISTENING && (uint32_t)(now - last_switch) >= FEEDER_WBR_SCAN_MS) {
            phase ^= 1U;
            if (!wbr_uart_rx_set_baud(SystemCoreClock, phase == 0U ? 9600U : 115200U)) { fault_stop(6U); }
            wbr_heartbeat_gap(&link);
            last_switch = now;
        }
        if ((uint32_t)(now - last_publish) >= 1000U) { publish(); last_publish = now; }
        __WFI(); /* 普通睡眠、1ms唤醒；没有上电自动动作或配网重置。 */
    }
}
