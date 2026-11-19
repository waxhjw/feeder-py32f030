#include "wbr_init.h"
#include "wbr_init_config.h"
#include <string.h>
#ifdef FEEDER_OTA_APP
#include "ota_update.h"
#endif
static const uint8_t product[] = FEEDER_TUYA_PRODUCT_JSON;
#ifdef FEEDER_PAIR_MODE
/* 实板0.6.0原始报文证明：只重启MCU时，模组保留PID/工作方式，
 * 心跳后直接发08和03。允许有效应答，但不能虚构已回复PID或工作方式。
 */
#define ALLOW_WORK(state) 1U
#define ALLOW_NETWORK(state) 1U
#define ALLOW_STATUS(state) 1U
#else
#define ALLOW_WORK(state) ((state) & WBR_INIT_PRODUCT)
#define ALLOW_NETWORK(state) ((state) & WBR_INIT_WORK)
#define ALLOW_STATUS(state) ((state) & WBR_INIT_NETWORK)
#endif
_Static_assert(sizeof(product) - 1U + 7U <= WBR_INIT_FRAME_LIMIT, "Product frame too large");
_Static_assert(TUYA_PAYLOAD_LIMIT >= 132U, "DP1 maximum needs 128 data bytes plus DP header");
void wbr_init_reset(WbrInit *link)
{
    memset(link, 0, sizeof(*link)); link->network_state = UINT32_MAX;
#ifdef FEEDER_PAIR_MODE
    link->scan_result = link->scan_detail = UINT32_MAX;
#endif
}
void wbr_init_gap(WbrInit *link) { tuya_frame_gap(&link->parser); }
void wbr_init_fail(WbrInit *link) { link->locked = 1U; link->tx_failures++; }
static int enqueue(WbrInit *link, uint8_t cmd, const uint8_t *data, uint32_t length)
{
    if (link->locked) { return 0; }
    if (link->count == WBR_INIT_QUEUE_LIMIT || length + 7U > WBR_INIT_FRAME_LIMIT) {
        wbr_init_fail(link); return 0;
    }
    WbrInitPacket *packet = &link->queue[(link->head + link->count) % WBR_INIT_QUEUE_LIMIT];
    packet->length = (uint8_t)(length + 7U);
    uint8_t *out = packet->data;
    out[0] = 0x55; out[1] = 0xAA; out[2] = 3; out[3] = cmd;
    out[4] = 0; out[5] = (uint8_t)length;
    for (uint32_t i = 0U; i < length; ++i) { out[6U + i] = data[i]; }
    uint8_t checksum = 0U;
    for (uint32_t i = 0U; i < length + 6U; ++i) { checksum += out[i]; }
    out[length + 6U] = checksum;
    link->count++;
    if (link->count > link->queue_high_water) { link->queue_high_water = link->count; }
    return 1;
}
#ifdef FEEDER_PAIR_MODE
int wbr_init_serial_ready(const WbrInit *link)
{
    return (link->state & 7U) == 7U || (link->state & 9U) == 9U;
}
#ifdef FEEDER_APP_MOTOR
int wbr_init_app_report(WbrInit *link,const uint8_t *data,uint32_t length)
{
    /* 可选状态上报让位于心跳/初始化队列；忙时交由调用者稍后重试。 */
    if (!wbr_init_serial_ready(link) || link->locked || link->count || link->sending) { return 0; }
    return enqueue(link,7U,data,length);
}
#ifdef FEEDER_PLAN_APP
int wbr_init_request_time(WbrInit *link)
{
    if(!wbr_init_serial_ready(link) || link->locked || link->count || link->sending) { return 0; }
    return enqueue(link,0x1CU,0,0U);
}
#endif
#endif
int wbr_init_request_pair(WbrInit *link)
{
    if (link->locked || !wbr_init_serial_ready(link) || link->count || link->sending || link->pair_attempts) { return 0; }
    /* 0.6.7受控对比：官方mcu_reset_wifi()使用0x04空载荷。
     * 保持m0、板载UART、初始化和单次保护不变；不能把ACK当成已可配网。
     */
    link->pair_attempts = 1U;
    return enqueue(link, 4U, 0, 0U);
}
int wbr_init_request_scan(WbrInit *link, uint32_t now)
{
    if (now < 5000U || link->locked || (link->state & 9U) != 9U || link->count || link->sending || link->scan_attempts) { return 0; }
    if (!enqueue(link, 0x0EU, 0, 0U)) { return 0; }
    link->scan_attempts = 1U; link->scan_started = now;
    return 1;
}
void wbr_init_scan_poll(WbrInit *link, uint32_t now)
{
    if (link->scan_tx && !link->scan_responses && (uint32_t)(now - link->scan_started) >= 20000U) {
        link->scan_timeout = 1U; /* 不重发，不把无响应当成未授权。 */
    }
}
void wbr_init_info_poll(WbrInit *link, uint32_t now)
{
    /* 只读版本属于基础串口诊断：00/01/02已完成，或模组保留会话已回复00/03，均可尝试。
     * 不把无线网络状态当成读取软件版本的前提；配网重置仍保留00/03门槛。
     * 不支持此指令时只记录未知，不能推出授权/硬件故障。
     */
    if (!link->info_attempts && !link->locked &&
        wbr_init_serial_ready(link) && !link->count && !link->sending) {
        const uint8_t query[] = {7U, 5U}; /* 官方0x3407，只选05模组版本；06/FF会含网络密码，禁止使用。 */
        if (enqueue(link, 0x34U, query, sizeof(query))) {
            link->info_attempts = 1U; link->info_started = now;
        }
    }
    if (link->info_tx && !link->info_responses && (uint32_t)(now - link->info_started) >= 5000U) {
        link->info_result = 4U; /* 不支持或没有应答仍属未知，不能当成无授权。 */
    }
}
static int extract_version(WbrInit *link, const uint8_t *data, uint32_t length)
{
    for (uint32_t i = 0U; i + 4U < length; ++i) {
        if (memcmp(data + i, "\"sw\"", 4U)) { continue; }
        uint32_t p = i + 4U;
        while (p < length && (data[p] == ' ' || data[p] == '\t' || data[p] == '\r' || data[p] == '\n')) { p++; }
        if (p >= length || data[p++] != ':') { continue; }
        while (p < length && (data[p] == ' ' || data[p] == '\t' || data[p] == '\r' || data[p] == '\n')) { p++; }
        if (p >= length || data[p++] != '"') { continue; }
        uint32_t start = p, dots = 0U, digits = 0U;
        while (p < length && p - start < 15U && data[p] != '"') {
            if (data[p] >= '0' && data[p] <= '9') { digits++; if (digits > 3U) { break; } }
            else if (data[p] == '.' && digits && dots < 2U) { dots++; digits = 0U; }
            else { break; }
            p++;
        }
        if (p < length && data[p] == '"' && dots == 2U && digits) {
            link->info_length = p - start;
            memcpy(link->info_version, data + start, link->info_length);
            link->info_version[link->info_length] = 0U;
            return 1;
        }
    }
    return 0;
}
#endif
/* 只核验DP边界、类型和范围，不保存、不执行、不回传“执行成功”。
 * 完整检查后才计数，避免前半个有效DP掩盖后半个截断DP。
 */
static int valid_dp(const uint8_t *data, uint32_t length)
{
    if (!length) { return 0; }
    uint32_t offset = 0U;
    while (offset < length) {
        if (length - offset < 4U) { return 0; }
        uint32_t id = data[offset], type = data[offset + 1U];
        uint32_t size = ((uint32_t)data[offset + 2U] << 8U) | data[offset + 3U];
        offset += 4U;
        if (size > length - offset) { return 0; }
        if (id == 1U) { if (type != 0U || size > 128U) { return 0; } }
        else if (id == 2U) { if (type != 1U || size != 1U || data[offset] > 1U) { return 0; } }
        else if (id == 3U) {
            if (type != 2U || size != 4U || data[offset] || data[offset+1U] || data[offset+2U] ||
                data[offset+3U] < 1U || data[offset+3U] > 12U) { return 0; }
        } else { return 0; } /* 只读DP4/10/11不接受下发；未知DP也不执行。 */
        offset += size;
    }
    return 1;
}
void wbr_init_feed(WbrInit *link, uint8_t byte, uint32_t errors, uint32_t now)
{
    link->rx_bytes++; link->last_rx_ms = now;
    if (errors) { link->uart_errors++; wbr_init_gap(link); return; }
    if (!tuya_frame_feed(&link->parser, byte)) { return; }
    const uint8_t *frame = link->parser.data;
    uint32_t length = ((uint32_t)frame[4] << 8U) | frame[5];
    uint8_t cmd = frame[3];
    if (link->locked || frame[2] != 0U) { link->ignored++; return; }
    if (cmd == 0U && !length) {
        link->heartbeats++;
        if (!link->hb_pending) {
            uint8_t value = link->replies ? 1U : 0U;
            if (enqueue(link, 0U, &value, 1U)) { link->hb_pending = 1U; }
        }
    } else if (!(link->state & WBR_INIT_HEARTBEAT)) { link->ignored++; }
    else if (cmd == 1U && !length) {
        link->product_queries++; enqueue(link, 1U, product, sizeof(product) - 1U);
    } else if (cmd == 2U && !length && ALLOW_WORK(link->state)) {
        link->work_queries++; enqueue(link, 2U, 0, 0U);
    } else if (cmd == 3U && length == 1U && frame[6] <= 6U && ALLOW_NETWORK(link->state)) {
        link->network_reports++; link->network_state = frame[6]; enqueue(link, 3U, 0, 0U);
    } else if (cmd == 8U && !length && ALLOW_STATUS(link->state)) {
#ifdef FEEDER_APP_MOTOR
        link->status_queries++;link->app_report_request=1U;
#else
        /* 电机未接入本版本的功能逻辑，真实状态为空闲；不伪造电池/充电/喂食完成。 */
        const uint8_t standby[] = {4U, 4U, 0U, 1U, 0U};
        link->status_queries++; enqueue(link, 7U, standby, sizeof(standby));
#endif
    }
#ifdef FEEDER_PAIR_MODE
    else if (cmd == 4U && !length && link->pair_transmitted && !link->pair_acks) {
        link->pair_acks = 1U; /* 只是模组接受指令；需另等网络状态进入配网。 */
    }
    else if (cmd == 0x34U && length >= 2U && frame[6] == 7U && link->info_tx && !link->info_responses) {
        link->info_responses = 1U;
        link->info_result = frame[7] ? 2U : (extract_version(link, frame + 8U, length - 2U) ? 1U : 3U);
    }
    else if (cmd == 0x0EU && length == 2U && link->scan_tx && !link->scan_responses) {
        link->scan_responses = 1U; link->scan_result = frame[6]; link->scan_detail = frame[7];
    }
#endif
#ifdef FEEDER_OTA_APP
    else if (cmd == 0x0AU || cmd == 0x0BU) {
        if(ota_receive(cmd,frame+6U,length,now)) {
            uint8_t packet_size=0U; /* 官方0代表256字节，接收载荷另含4字节偏移。 */
            enqueue(link,cmd,cmd==0x0AU?&packet_size:0,cmd==0x0AU?1U:0U);
        }
    }
#endif
#ifdef FEEDER_PLAN_APP
    else if(cmd==0x1CU) {
        if(link->app_time) { link->app_time(frame+6U,length,now); }
    }
#endif
    else if (cmd == 6U) {
#ifdef FEEDER_OTA_APP
        if(link->ota_block_dp) { link->blocked_dp++;return; }
#endif
#ifdef FEEDER_APP_MOTOR
        int result=link->app_dp ? link->app_dp(link->app_context,frame+6U,length,now) : -1;
        if (result==0) { link->malformed++; }
        else if (result<0) {
            if (valid_dp(frame+6U,length)) { link->blocked_dp++; }
            else { link->malformed++; }
        }
#else
        if (valid_dp(frame + 6U, length)) { link->blocked_dp++; }
        else { link->malformed++; }
#endif
    } else { link->ignored++; }
}
const WbrInitPacket *wbr_init_peek(WbrInit *link)
{
    if (link->locked || link->sending || !link->count) { return 0; }
    link->sending = 1U; return &link->queue[link->head];
}
void wbr_init_sent(WbrInit *link)
{
    if (!link->sending || !link->count || link->locked) { return; }
    const WbrInitPacket *packet = &link->queue[link->head];
    switch (packet->data[3]) {
    case 0U: link->replies++; link->hb_pending = 0U; link->state |= WBR_INIT_HEARTBEAT; break;
    case 1U:
        if (!link->products_sent) { memcpy(link->first_product, packet->data, packet->length); }
        link->products_sent++; link->state |= WBR_INIT_PRODUCT; break;
    case 2U: link->work_sent++; link->state |= WBR_INIT_WORK; break;
    case 3U: link->network_acks++; link->state |= WBR_INIT_NETWORK; break;
    case 7U: link->status_reports++; link->state |= WBR_INIT_STATUS; break;
#ifdef FEEDER_PLAN_APP
    case 0x1CU: break;
#endif
#ifdef FEEDER_OTA_APP
    case 0x0AU: case 0x0BU: break;
#endif
#ifdef FEEDER_PAIR_MODE
    case 4U: link->pair_transmitted = 1U; break;
    case 0x34U: link->info_tx = 1U; break;
    case 0x0EU: link->scan_tx = 1U; break;
#endif
    default: wbr_init_fail(link); return;
    }
    link->tx_completed++; link->head = (link->head + 1U) % WBR_INIT_QUEUE_LIMIT;
    link->count--; link->sending = 0U;
}
