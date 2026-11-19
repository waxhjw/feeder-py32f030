#include "wbr_rx_observer.h"
#include <string.h>

void wbr_rx_observer_init(WbrRxObserver *observer)
{
    memset(observer, 0, sizeof(*observer));
    observer->rates[0].baud = 9600U;
    observer->rates[1].baud = 115200U;
}

void wbr_rx_observer_select(WbrRxObserver *observer, uint32_t phase)
{
    observer->phase = phase & 1U;
    observer->used = 0U; /* 换速率后不拼接两个观察窗口中的半包。 */
}

void wbr_rx_observer_discontinuity(WbrRxObserver *observer)
{
    observer->used = 0U; /* 硬件丢字节或队列溢出后重新寻找完整报文。 */
}

static void discard(WbrRxObserver *observer, uint32_t count)
{
    observer->used -= count;
    memmove(observer->pending, observer->pending + count, observer->used);
}

void wbr_rx_observer_feed(WbrRxObserver *observer, uint8_t value, uint32_t error)
{
    WbrRxRate *rate = &observer->rates[observer->phase];
    rate->bytes++;
    if (rate->raw_len < WBR_CAPTURE_BYTES) {
        rate->raw[rate->raw_len++] = value;
    }
    if (error != 0U) {
        rate->uart_errors++;
        observer->used = 0U;
        return; /* 错误采样保留作诊断，不进入有效报文。 */
    }
    observer->pending[observer->used++] = value;
    while (observer->used != 0U) {
        uint8_t *data = observer->pending;
        if (data[0] != 0x55U) { discard(observer, 1U); continue; }
        if (observer->used < 2U) { return; }
        if (data[1] != 0xAAU) { discard(observer, 1U); continue; }
        if (observer->used < 6U) { return; }
        uint32_t length = ((uint32_t)data[4] << 8U) | data[5];
        if (length > WBR_MAX_PAYLOAD) {
            rate->too_long++;
            discard(observer, 1U);
            continue;
        }
        uint32_t total = length + 7U;
        if (observer->used < total) { return; }
        uint8_t checksum = 0U;
        for (uint32_t i = 0U; i < total - 1U; ++i) { checksum += data[i]; }
        if (checksum != data[total - 1U]) {
            rate->bad_checksum++;
            discard(observer, 1U); /* 保留坏包内可能存在的下一帧头。 */
            continue;
        }
        rate->frames++;
        rate->last_command = data[3];
        if (data[2] == 0U && data[3] == 0U && length == 0U) { rate->heartbeats++; }
        if (rate->first_frame_len == 0U) {
            rate->first_frame_len = total;
            uint32_t capture = total < WBR_CAPTURE_BYTES ? total : WBR_CAPTURE_BYTES;
            memcpy(rate->first_frame, data, capture);
        }
        discard(observer, total);
    }
}
