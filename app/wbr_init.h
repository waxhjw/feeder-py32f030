#ifndef FEEDER_WBR_INIT_H
#define FEEDER_WBR_INIT_H
#include "tuya_frame.h"
#define WBR_INIT_QUEUE_LIMIT 8U
#ifdef FEEDER_PLAN_APP
#define WBR_INIT_FRAME_LIMIT 72U /* DP1普通计划10*6+4字节DP头+7字节帧。 */
#else
#define WBR_INIT_FRAME_LIMIT 64U
#endif
enum { WBR_INIT_HEARTBEAT = 1U, WBR_INIT_PRODUCT = 2U, WBR_INIT_WORK = 4U,
       WBR_INIT_NETWORK = 8U, WBR_INIT_STATUS = 16U };
typedef struct { uint8_t length, data[WBR_INIT_FRAME_LIMIT]; } WbrInitPacket;
typedef struct {
    TuyaFrameParser parser;
    uint32_t state, rx_bytes, heartbeats, replies, product_queries, products_sent;
    uint32_t work_queries, work_sent, network_reports, network_acks, network_state;
    uint32_t status_queries, status_reports, blocked_dp, malformed, ignored, uart_errors;
    uint32_t tx_completed, tx_failures, queue_high_water, last_rx_ms, locked;
    uint32_t head, count, sending, hb_pending;
    WbrInitPacket queue[WBR_INIT_QUEUE_LIMIT];
    uint8_t first_product[64];
#ifdef FEEDER_PLAN_APP
    void (*app_time)(const uint8_t *,uint32_t,uint32_t);
#endif
#ifdef FEEDER_APP_MOTOR
    int (*app_dp)(void *,const uint8_t *,uint32_t,uint32_t);
    void *app_context;
    uint32_t app_report_request;
#ifdef FEEDER_OTA_APP
    uint32_t ota_block_dp;
#endif
#endif
#ifdef FEEDER_PAIR_MODE
    uint32_t pair_attempts, pair_transmitted, pair_acks;
    uint32_t info_attempts, info_tx, info_responses, info_result, info_length, info_started;
    uint8_t info_version[16];
    uint32_t scan_attempts, scan_tx, scan_responses, scan_result, scan_detail, scan_started, scan_timeout;
#endif
} WbrInit;
void wbr_init_reset(WbrInit *link);
void wbr_init_gap(WbrInit *link);
void wbr_init_feed(WbrInit *link, uint8_t byte, uint32_t errors, uint32_t now);
/* peek后由硬件发送；完成TC后才调用sent，状态不能因排队提前成功。 */
const WbrInitPacket *wbr_init_peek(WbrInit *link);
void wbr_init_sent(WbrInit *link);
void wbr_init_fail(WbrInit *link);
#ifdef FEEDER_APP_MOTOR
int wbr_init_app_report(WbrInit *link,const uint8_t *data,uint32_t length);
#ifdef FEEDER_PLAN_APP
int wbr_init_request_time(WbrInit *link);
#endif
#endif
#ifdef FEEDER_PAIR_MODE
/* 官方初始化00/01/02/[03]：03可选；兼容保留会话00/03，不伪造应答计数。 */
int wbr_init_serial_ready(const WbrInit *link);
/* 显式请求且串口初始化已应答、队列空、本次开机未请求过。 */
int wbr_init_request_pair(WbrInit *link);
/* 初始化可用后仅查询一次模组版本，超时不重发；不查询网络配置或密码。 */
void wbr_init_info_poll(WbrInit *link, uint32_t now);
typedef struct {
    uint32_t magic, abi, sequence, attempts, tx_done, responses, result, length;
    uint8_t version[16];
} WbrModuleInfo;
extern volatile WbrModuleInfo g_wbr_module_info;
/* 官方0x0e固定SSID扫描：完成初始化且上电至少5秒后，由显式Scan触发一次。
 * 结果00/01表示模组自报未烧录授权key；00/00仅表示没有发现固定测试SSID。
 */
int wbr_init_request_scan(WbrInit *link, uint32_t now);
void wbr_init_scan_poll(WbrInit *link, uint32_t now);
typedef struct {
    uint32_t magic, abi, request, request_result, attempts, tx_done, responses, result, detail, timed_out;
} WbrWifiTest;
extern volatile WbrWifiTest g_wbr_wifi_test;
#define WBR_WIFI_SCAN_REQUEST 0x5343414EUL
#endif
#endif
