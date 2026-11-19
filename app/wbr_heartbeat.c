#include "wbr_heartbeat.h"
#include <string.h>

void wbr_heartbeat_init(WbrHeartbeat *link)
{
    memset(link, 0, sizeof(*link));
    link->first_reply_value = link->last_reply_value = UINT32_MAX;
}
void wbr_heartbeat_gap(WbrHeartbeat *link) { tuya_frame_gap(&link->parser); }

int wbr_heartbeat_feed(WbrHeartbeat *link, uint8_t byte, uint32_t error, uint32_t now, uint8_t *reply)
{
    link->rx_bytes++;
    link->last_rx_ms = now;
    if (error != 0U) { link->uart_errors++; wbr_heartbeat_gap(link); return 0; }
    if (!tuya_frame_feed(&link->parser, byte)) { return 0; }
    const uint8_t *frame = link->parser.data;
    if (frame[2] != 0U || frame[4] != 0U || frame[5] != 0U) { link->ignored++; return 0; }
    if (frame[3] == 0U) {
        link->heartbeats++;
        if (link->pending != 0U) { link->ignored++; return 0; }
        *reply = link->replies == 0U ? 0U : 1U;
        link->pending = 1U;
        link->tx_started_ms = now;
        if (link->state == WBR_HB_LISTENING) { link->state = WBR_HB_WAIT_PRODUCT; }
        return 1;
    }
    if (frame[3] == 1U && link->replies != 0U) {
        if (link->product_queries == 0U) {
            memcpy(link->first_query, frame, 7U);
            link->query_ms = now;
        }
        link->product_queries++;
        link->state = WBR_HB_NEED_PRODUCT;
        return 0; /* PID未确认，不发送示例PID，也不猜产品信息。 */
    }
    link->ignored++; /* 不执行配网、DP、OTA或其他指令。 */
    return 0;
}

void wbr_heartbeat_sent(WbrHeartbeat *link, uint32_t now)
{
    if (link->pending == 0U) { return; }
    uint8_t value = link->replies == 0U ? 0U : 1U;
    if (link->replies == 0U) {
        const uint8_t first[] = {0x55, 0xAA, 3, 0, 0, 1, 0, 3};
        memcpy(link->first_reply, first, sizeof(first));
        link->first_reply_value = value;
        link->first_reply_ms = now;
    }
    uint32_t elapsed = now - link->tx_started_ms;
    if (elapsed > link->max_reply_ms) { link->max_reply_ms = elapsed; }
    link->last_reply_value = value;
    link->replies++;
    link->pending = 0U;
}
