#ifndef FEEDER_WBR_RX_OBSERVER_H
#define FEEDER_WBR_RX_OBSERVER_H
#include <stdint.h>
#define WBR_CAPTURE_BYTES 64U
#define WBR_MAX_PAYLOAD 128U
#define WBR_MAX_FRAME (WBR_MAX_PAYLOAD + 7U)

typedef struct {
    uint32_t baud, bytes, frames, heartbeats, bad_checksum, too_long;
    uint32_t uart_errors, last_command, first_frame_len, raw_len;
    uint8_t first_frame[WBR_CAPTURE_BYTES], raw[WBR_CAPTURE_BYTES];
} WbrRxRate;

typedef struct {
    WbrRxRate rates[2];
    uint32_t phase, used;
    uint8_t pending[WBR_MAX_FRAME];
} WbrRxObserver;

/* 纯接收解析器：无硬件依赖、无发送回调、无执行器调用。 */
void wbr_rx_observer_init(WbrRxObserver *observer);
void wbr_rx_observer_select(WbrRxObserver *observer, uint32_t phase);
void wbr_rx_observer_discontinuity(WbrRxObserver *observer);
void wbr_rx_observer_feed(WbrRxObserver *observer, uint8_t value, uint32_t error);
#endif
