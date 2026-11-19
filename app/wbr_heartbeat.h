#ifndef FEEDER_WBR_HEARTBEAT_H
#define FEEDER_WBR_HEARTBEAT_H
#include "tuya_frame.h"
/* LISTENING=监听；WAIT_PRODUCT=心跳已应答、等待产品查询；
 * NEED_PRODUCT=模块已查询产品信息、等待真实PID；不表示已配网。
 */
enum { WBR_HB_LISTENING, WBR_HB_WAIT_PRODUCT, WBR_HB_NEED_PRODUCT };
typedef struct {
    TuyaFrameParser parser;
    uint32_t state, rx_bytes, heartbeats, replies, product_queries, ignored, uart_errors;
    uint32_t pending, first_reply_value, last_reply_value;
    uint32_t first_reply_ms, query_ms, last_rx_ms, max_reply_ms, tx_started_ms;
    uint8_t first_reply[8], first_query[8];
} WbrHeartbeat;
void wbr_heartbeat_init(WbrHeartbeat *link);
void wbr_heartbeat_gap(WbrHeartbeat *link);
/* 只对版本00、命令00、长度0且校验通过的完整帧请求应答。
 * 返回1时reply只会是0或1；调用方必须交给有超时的专用发送器。
 */
int wbr_heartbeat_feed(WbrHeartbeat *link, uint8_t byte, uint32_t error, uint32_t now, uint8_t *reply);
void wbr_heartbeat_sent(WbrHeartbeat *link, uint32_t now);
#endif
