#include "py32f0xx.h"
#include "wbr_init_config.h"
#include "motor_off.h"
#include "wbr_uart_rx.h"
#include "wbr_uart_tx.h"
#include "wbr_init.h"
#ifdef FEEDER_LOCAL_UI
#include "local_controls_io.h"
#endif
#ifdef FEEDER_CYCLE_SUITE
#include "bench_buzzer.h"
#endif
#ifdef FEEDER_PLAN_DIAG
#include "plan_diag.h"
#endif
#ifdef FEEDER_ADC_DIAG
#include "adc_sampler.h"
#endif
#ifdef FEEDER_INPUT_DIAG
#include "input_observer_io.h"
#endif
#ifdef FEEDER_OTA_APP
#include "ota_update.h"
#endif
#ifdef FEEDER_TOUCH_MOTOR
#include "touch_motor.h"
#include "touch_motor_io.h"
#ifdef FEEDER_APP_MOTOR
#include "app_motor.h"
static AppMotor app_motor;
#endif
static TouchMotor touch;
typedef struct {
    uint32_t magic,abi,firmware_id,sequence,uptime_ms,phase,raw_level,motor_on;
    uint32_t toggles,held_ms,fault,gpioa_moder,gpioa_odr,gpiob_moder,gpiob_pupdr,stop_request;
#ifdef FEEDER_TOUCH_REVERSE
    uint32_t direction,reversals,pending,switch_ms;
#endif
} TouchDebug;
#ifdef FEEDER_TOUCH_REVERSE
volatile TouchDebug g_touch_motor_debug={.magic=0x544F5031U,.abi=2U,.firmware_id=FEEDER_WBR_INIT_ID};
_Static_assert(sizeof(TouchDebug)==80U,"Update reverse debug ABI");
#else
volatile TouchDebug g_touch_motor_debug={.magic=0x544F5031U,.abi=1U,.firmware_id=0x00090000U};
_Static_assert(sizeof(TouchDebug)==64U,"Update TOP1 debug reader ABI");
#endif
static void touch_debug_publish(uint32_t now)
{
    g_touch_motor_debug.sequence++; __DMB();
    g_touch_motor_debug.uptime_ms=now; g_touch_motor_debug.phase=touch.phase;
    g_touch_motor_debug.raw_level=touch.raw; g_touch_motor_debug.motor_on=touch.motor_on;
    g_touch_motor_debug.toggles=touch.toggles; g_touch_motor_debug.held_ms=touch.held_ms;
    g_touch_motor_debug.fault=touch.fault;
    g_touch_motor_debug.gpioa_moder=GPIOA->MODER; g_touch_motor_debug.gpioa_odr=GPIOA->ODR;
    g_touch_motor_debug.gpiob_moder=GPIOB->MODER; g_touch_motor_debug.gpiob_pupdr=GPIOB->PUPDR;
#ifdef FEEDER_TOUCH_REVERSE
    g_touch_motor_debug.direction=touch.direction;g_touch_motor_debug.reversals=touch.reversals;
    g_touch_motor_debug.pending=touch.pending;g_touch_motor_debug.switch_ms=touch.switch_ms;
#endif
    __DMB(); g_touch_motor_debug.sequence++;
}
#endif
#ifdef FEEDER_PUMP_TEST
#include "pump_test.h"
#endif
#ifdef FEEDER_LED_TEST
#include "led_tm1652.h"
#endif
#ifdef FEEDER_PAIR_MODE
#include "wbr_pair.h"
#endif
typedef struct {
    uint32_t magic, abi, firmware_id, size, sequence, uptime_ms, core_clock_hz;
    uint32_t phase, baud, fault, gpioa_moder, gpioa_odr, gpiof_moder, gpiof_afr;
    uint32_t uart_cr1, uart_brr, dropped, state, rx_bytes, frames, heartbeats, replies;
    uint32_t product_queries, products_sent, work_queries, work_sent;
    uint32_t network_reports, network_acks, network_state, status_queries, status_reports;
    uint32_t blocked_dp, malformed, ignored, uart_errors, bad_checksum, too_long;
    uint32_t tx_completed, tx_failures, queue_high_water;
    uint8_t first_product[64];
} WbrInitDebug;
_Static_assert(sizeof(WbrInitDebug) == 224U, "Update debug reader ABI");
volatile WbrInitDebug g_wbr_init_debug = {
    .magic = FEEDER_WBR_INIT_MAGIC, .abi = 1U, .firmware_id = FEEDER_WBR_INIT_ID,
    .size = sizeof(WbrInitDebug)
};
__attribute__((used, section(".fw_metadata")))
const char g_feeder_version[] = "FEEDER " FEEDER_WBR_INIT_VERSION " " FEEDER_TARGET_PART;
static volatile uint32_t ticks;
#ifdef FEEDER_STORE_PROFILE
uint32_t store_profile_us(void)
{
    uint32_t before,value;
    do { before=ticks;value=SysTick->VAL; } while(before!=ticks);
    return before*1000U+(SysTick->LOAD-value)/8U;
}
#endif
static WbrInit link;
static uint32_t phase;
#ifdef FEEDER_OTA_APP
void ota_quiesce(void)
{
    /* 先关闭真实GPIO，再取消运行/定时请求；升级中不响应触摸或App启动。 */
    motor_off_hold();
#ifdef FEEDER_TOUCH_MOTOR
    touch_motor_stop(&touch);app_motor_timing(&app_motor,ticks);
#endif
#ifdef FEEDER_LOCAL_UI
    local_controls_io_stop();
#endif
    link.ota_block_dp=1U;
    led_tm1652_stop();
#ifdef FEEDER_ADC_DIAG
    adc_sampler_pause(ticks);
#endif
}
#endif
#ifdef FEEDER_PAIR_MODE
volatile WbrModuleInfo g_wbr_module_info = {.magic = 0x57424946U, .abi = 1U};
_Static_assert(sizeof(WbrModuleInfo) == 48U, "Update info reader ABI");
volatile WbrWifiTest g_wbr_wifi_test = {.magic = 0x57425453U, .abi = 1U,
    .result = UINT32_MAX, .detail = UINT32_MAX};
_Static_assert(sizeof(WbrWifiTest) == 40U, "Update WiFi test reader ABI");
static void poll_wifi_test(uint32_t now)
{
    uint32_t command = g_wbr_wifi_test.request;
    if (command) {
        g_wbr_wifi_test.request = 0U; __DMB();
        if (command != WBR_WIFI_SCAN_REQUEST) { g_wbr_wifi_test.request_result = 4U; }
        else if (link.scan_attempts) { g_wbr_wifi_test.request_result = 3U; }
        else { g_wbr_wifi_test.request_result = wbr_init_request_scan(&link, now) ? 1U : 2U; }
    }
    wbr_init_scan_poll(&link, now);
    g_wbr_wifi_test.attempts = link.scan_attempts;
    g_wbr_wifi_test.tx_done = link.scan_tx;
    g_wbr_wifi_test.responses = link.scan_responses;
    g_wbr_wifi_test.result = link.scan_result;
    g_wbr_wifi_test.detail = link.scan_detail;
    g_wbr_wifi_test.timed_out = link.scan_timeout;
}
#endif
void SysTick_Handler(void) { ticks++; }
static void fault_stop(uint32_t code)
{
    __disable_irq(); motor_off_hold(); wbr_uart_tx_disable();
#ifdef FEEDER_CYCLE_SUITE
    bench_buzzer_set(0U);
#endif
#ifdef FEEDER_TOUCH_MOTOR
    touch_motor_fail(&touch,0x100U+code); touch_debug_publish(ticks);
#endif
#ifdef FEEDER_PUMP_TEST
    pump_test_fault(0x100U + code);
#endif
#ifdef FEEDER_LED_TEST
    led_tm1652_stop();
#endif
    g_wbr_init_debug.fault = code;
    g_wbr_init_debug.tx_failures = link.tx_failures;
    if (g_wbr_init_debug.sequence & 1U) { g_wbr_init_debug.sequence++; }
    for (;;) { motor_off_hold(); }
}
void NMI_Handler(void) { fault_stop(1U); }
void HardFault_Handler(void) { fault_stop(2U); }
static void publish(void)
{
    g_wbr_init_debug.sequence++; __DMB();
    g_wbr_init_debug.uptime_ms = ticks;
    g_wbr_init_debug.core_clock_hz = SystemCoreClock;
    g_wbr_init_debug.phase = phase;
    g_wbr_init_debug.baud = phase ? 115200U : 9600U;
    g_wbr_init_debug.gpioa_moder = GPIOA->MODER;
    g_wbr_init_debug.gpioa_odr = GPIOA->ODR;
    g_wbr_init_debug.gpiof_moder = GPIOF->MODER;
    g_wbr_init_debug.gpiof_afr = GPIOF->AFR[0];
    g_wbr_init_debug.uart_cr1 = USART2->CR1;
    g_wbr_init_debug.uart_brr = USART2->BRR;
    g_wbr_init_debug.dropped = wbr_uart_rx_dropped();
    g_wbr_init_debug.state = link.state;
    g_wbr_init_debug.rx_bytes = link.rx_bytes;
    g_wbr_init_debug.frames = link.parser.frames;
    g_wbr_init_debug.heartbeats = link.heartbeats;
    g_wbr_init_debug.replies = link.replies;
    g_wbr_init_debug.product_queries = link.product_queries;
    g_wbr_init_debug.products_sent = link.products_sent;
    g_wbr_init_debug.work_queries = link.work_queries;
    g_wbr_init_debug.work_sent = link.work_sent;
    g_wbr_init_debug.network_reports = link.network_reports;
    g_wbr_init_debug.network_acks = link.network_acks;
    g_wbr_init_debug.network_state = link.network_state;
    g_wbr_init_debug.status_queries = link.status_queries;
    g_wbr_init_debug.status_reports = link.status_reports;
    g_wbr_init_debug.blocked_dp = link.blocked_dp;
    g_wbr_init_debug.malformed = link.malformed;
    g_wbr_init_debug.ignored = link.ignored;
    g_wbr_init_debug.uart_errors = link.uart_errors;
    g_wbr_init_debug.bad_checksum = link.parser.bad_checksum;
    g_wbr_init_debug.too_long = link.parser.too_long;
    g_wbr_init_debug.tx_completed = link.tx_completed;
    g_wbr_init_debug.tx_failures = link.tx_failures;
    g_wbr_init_debug.queue_high_water = link.queue_high_water;
    for (uint32_t i = 0U; i < 64U; ++i) { g_wbr_init_debug.first_product[i] = link.first_product[i]; }
    __DMB(); g_wbr_init_debug.sequence++;
#ifdef FEEDER_PAIR_MODE
    g_wbr_module_info.sequence++; __DMB();
    g_wbr_module_info.attempts = link.info_attempts;
    g_wbr_module_info.tx_done = link.info_tx;
    g_wbr_module_info.responses = link.info_responses;
    g_wbr_module_info.result = link.info_result;
    g_wbr_module_info.length = link.info_length;
    for (uint32_t i = 0U; i < 16U; ++i) { g_wbr_module_info.version[i] = link.info_version[i]; }
    __DMB(); g_wbr_module_info.sequence++;
#endif
}
static void poll_tx(void)
{
    int result = wbr_uart_tx_poll(ticks);
    if (result < 0) { wbr_init_fail(&link); fault_stop(7U); }
    if (result > 0) { wbr_init_sent(&link); }
    if (link.locked) { fault_stop(8U); }
    const WbrInitPacket *packet = wbr_init_peek(&link);
    if (packet && !wbr_uart_tx_start(packet->data, packet->length, ticks)) {
        wbr_init_fail(&link); fault_stop(8U);
    }
}
int main(void)
{
    motor_off_hold(); /* 启动、普通循环、故障路径均先关闭PA8/PA11。 */
    SystemCoreClockUpdate();
    if (SystemCoreClock != 8000000U || (RCC->CFGR & RCC_CFGR_PPRE) != 0U) { fault_stop(3U); }
    if (SysTick_Config(SystemCoreClock / 1000U) != 0U) { fault_stop(4U); }
    wbr_init_reset(&link);
    if (!wbr_uart_rx_init(SystemCoreClock, 9600U)) { fault_stop(5U); }
    wbr_uart_tx_reset(); publish();
#ifdef FEEDER_LED_TEST
    if (!led_tm1652_init(SystemCoreClock)) { fault_stop(10U); }
#endif
#ifdef FEEDER_PUMP_TEST
    if (!pump_test_init()) { fault_stop(11U); }
#endif
#ifdef FEEDER_TOUCH_MOTOR
    if (!touch_motor_io_init()) { fault_stop(12U); }
    touch_motor_reset(&touch,ticks); touch_debug_publish(ticks);
    uint32_t last_touch_publish=ticks;
#ifdef FEEDER_APP_MOTOR
    app_motor_bind(&app_motor,&touch,&link);
#endif
#endif
    uint32_t last_switch = ticks, last_publish = ticks;
#ifdef FEEDER_INPUT_DIAG
    input_observer_io_init();
#endif
#ifdef FEEDER_ADC_DIAG
    adc_sampler_init(ticks);
#endif
#ifdef FEEDER_PLAN_DIAG
    plan_diag_init(ticks);
#endif
#ifdef FEEDER_LOCAL_UI
    local_controls_io_init(&link,ticks);
#endif
    for (;;) {
#ifdef FEEDER_ADC_DIAG
        if(!ota_busy()) { adc_sampler_poll(ticks); }
#endif
#ifdef FEEDER_INPUT_DIAG
        input_observer_io_poll(ticks);
#endif
#ifdef FEEDER_TOUCH_MOTOR
        uint32_t stop=g_touch_motor_debug.stop_request;
        if (stop) {
            g_touch_motor_debug.stop_request=0U;
#ifdef FEEDER_LOCAL_UI
            local_controls_io_stop();
#endif
            if (stop==0x53544F50U) { touch_motor_stop(&touch); }
            else { touch_motor_fail(&touch,4U); }
        }
#ifdef FEEDER_OTA_APP
        if(!ota_busy()) { touch_motor_step(&touch,ticks,touch_motor_level()); }
        else { touch_motor_stop(&touch); }
#else
        touch_motor_step(&touch,ticks,touch_motor_level());
#endif
#ifdef FEEDER_QUICK_TIMED
        app_motor_timing(&app_motor,ticks);
#endif
#ifdef FEEDER_INPUT_DIAG
        /* 用户确认产品方案后的第一阶段诊断：保留旧状态机，但不允许机构动作。 */
        touch_motor_stop(&touch);
#endif
#ifdef FEEDER_TOUCH_REVERSE
#ifdef FEEDER_LOCAL_UI
        local_controls_io_poll(ticks,ota_busy());
#else
        touch_motor_output_direction(touch.motor_on && !touch.pending,touch.direction);
#endif
#else
        touch_motor_output(touch.motor_on);
#endif
#elif defined(FEEDER_PUMP_TEST)
        pump_test_poll(ticks); /* 仅显式SWD请求期间保持TIM1复用；其他状态持续关闭。 */
#else
        motor_off_hold();
#ifdef FEEDER_CYCLE_NET
#ifdef FEEDER_OTA_APP
        local_controls_io_poll(ticks,ota_busy());
#else
        local_controls_io_poll(ticks,0U);
#endif
#endif
#endif
        poll_tx();
        if ((uint32_t)(ticks - link.last_rx_ms) >= 100U) { wbr_init_gap(&link); }
        uint16_t event;
        for (uint32_t i = 0U; i < 64U && wbr_uart_rx_pop(&event); ++i) {
            poll_tx(); /* 先确认TC，再处理模块紧接着发来的下一步查询。 */
            if (event & 0x1000U) { wbr_init_gap(&link); }
            wbr_init_feed(&link, (uint8_t)event, (event >> 8U) & 15U, ticks);
        }
        if (wbr_uart_rx_dropped()) { fault_stop(9U); }
        uint32_t now = ticks;
#ifdef FEEDER_PLAN_DIAG
#if defined(FEEDER_CYCLE_NET) && !defined(FEEDER_OTA_APP)
        plan_diag_poll(now,0U); /* 此独立目标不接收OTA命令。 */
#else
        plan_diag_poll(now,ota_busy());
#endif
#endif
#ifdef FEEDER_APP_MOTOR
#ifndef FEEDER_LOCAL_UI
#ifdef FEEDER_OTA_APP
        if(!ota_busy()) { app_motor_poll(&app_motor,now); }
#else
        app_motor_poll(&app_motor,now);
#endif
#endif
#endif
#ifdef FEEDER_LED_TEST
#ifdef FEEDER_OTA_APP
        if(!ota_busy()) { led_tm1652_poll(now); }
#else
        led_tm1652_poll(now); /* 定时器逐位发送；涂鸦接收与原协议处理继续运行。 */
#endif
#endif
#ifdef FEEDER_PAIR_MODE
        wbr_pair_poll(&link, now); /* 没有明确SWD请求时不产生配网命令。 */
        wbr_init_info_poll(&link, now); /* 唯一自动诊断是一次只读版本查询，无网络重置。 */
        poll_wifi_test(now); /* 固定产测SSID扫描仅响应显式Scan请求。 */
#endif
#ifdef FEEDER_OTA_APP
        ota_poll(now);
        /* 最后一帧ACK必须等USART TC及发送队列清空后再重启，不在接收回调里直接复位。 */
        if(g_ota_debug.state==OTA_COMMITTED && !link.count && !link.sending &&
           (uint32_t)(now-g_ota_debug.last_ms)>=100U) {
            motor_off_hold();g_ota_debug.reboots++;NVIC_SystemReset();
        }
#endif
        if (!link.heartbeats && (uint32_t)(now - last_switch) >= FEEDER_WBR_SCAN_MS) {
            phase ^= 1U;
            if (!wbr_uart_rx_set_baud(SystemCoreClock, phase ? 115200U : 9600U)) { fault_stop(6U); }
            wbr_init_gap(&link); last_switch = now;
        }
        if ((uint32_t)(now - last_publish) >= 1000U) { publish(); last_publish = now; }
#ifdef FEEDER_TOUCH_MOTOR
        if ((uint32_t)(now-last_touch_publish)>=250U) { touch_debug_publish(now); last_touch_publish=now; }
        touch_motor_watchdog_feed(); /* 只有本轮控制/通信处理正常完成才刷新。 */
#endif
#ifdef FEEDER_CYCLE_NET
        IWDG->KR=0xAAAAU;
#endif
        __WFI(); /* 普通睡眠，无上电自动配网或动作。 */
    }
}
