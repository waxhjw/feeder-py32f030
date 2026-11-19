#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "py32f0xx.h"
#include "wbr_heartbeat.h"
#include "wbr_uart_rx.h"
#include "wbr_uart_hb.h"

GPIO_TypeDef test_gpiof;
RCC_TypeDef test_rcc;
USART_TypeDef test_usart2;
int test_irq_enabled;
static const uint8_t hb[] = {0x55,0xAA,0,0,0,0,0xFF};
static const uint8_t query[] = {0x55,0xAA,0,1,0,0,0};
static uint8_t reply_value;
static unsigned requests;

static void feed(WbrHeartbeat *link, const uint8_t *data, unsigned size, uint32_t now)
{
    for (unsigned i = 0; i < size; ++i) {
        requests += (unsigned)wbr_heartbeat_feed(link, data[i], 0, now, &reply_value);
    }
}
static void fresh(WbrHeartbeat *link)
{ wbr_heartbeat_init(link); requests = 0; reply_value = 77; }
static unsigned make_frame(uint8_t *data, unsigned version, unsigned command, const uint8_t *payload, unsigned length)
{
    data[0] = 0x55; data[1] = 0xAA; data[2] = (uint8_t)version; data[3] = (uint8_t)command;
    data[4] = (uint8_t)(length >> 8); data[5] = (uint8_t)length;
    if (length) { memcpy(data + 6, payload, length); }
    uint8_t sum = 0; for (unsigned i = 0; i < length + 6; ++i) { sum += data[i]; }
    data[length + 6] = sum;
    return length + 7;
}
static void tx_ready(void)
{
    test_gpiof.MODER = 0xFFFFFCFFU;
    test_gpiof.AFR[0] = 0xABCD1200U;
    test_gpiof.PUPDR = test_gpiof.OTYPER = test_gpiof.OSPEEDR = 0xFFFFFFFFU;
    assert(wbr_uart_rx_init(8000000U, 9600U));
    wbr_uart_hb_reset();
    test_usart2.SR = USART_SR_TXE;
    test_usart2.DR = 0xA5;
}
static void transmit(uint8_t value, uint32_t now)
{
    uint8_t observed[8];
    assert(wbr_uart_hb_start(value, now));
    for (unsigned i = 0; i < 8; ++i) {
        test_usart2.SR = USART_SR_TXE;
        assert(wbr_uart_hb_poll(now + i) == 0);
        observed[i] = (uint8_t)test_usart2.DR;
    }
    test_usart2.SR = USART_SR_TXE;
    assert(wbr_uart_hb_poll(now + 8) == 0); /* TXE不代表最后一位已经发完。 */
    test_usart2.SR |= USART_SR_TC;
    assert(wbr_uart_hb_poll(now + 9) == 1);
    const uint8_t expected[] = {0x55,0xAA,3,0,0,1,value,(uint8_t)(3+value)};
    assert(!memcmp(observed, expected, sizeof(expected)));
    assert(wbr_uart_hb_poll(now + 10) == 0);
}

int main(void)
{
    WbrHeartbeat link;
    fresh(&link); feed(&link, hb, 3, 1); assert(!requests);
    feed(&link, hb + 3, 4, 2); assert(requests == 1 && reply_value == 0 && link.pending);
    wbr_heartbeat_sent(&link, 11); assert(link.replies == 1 && link.first_reply_value == 0);
    feed(&link, hb, 7, 2000); assert(requests == 2 && reply_value == 1);
    wbr_heartbeat_sent(&link, 2010);
    assert(link.replies == 2 && link.last_reply_value == 1 && link.max_reply_ms == 10);
    puts("PASS fragmented first-zero and subsequent-one heartbeats");

    feed(&link, query, 7, 2020);
    assert(link.product_queries == 1 && link.state == WBR_HB_NEED_PRODUCT);
    assert(!memcmp(link.first_query, query, 7) && requests == 2);
    feed(&link, hb, 7, 17000); wbr_heartbeat_sent(&link, 17010);
    assert(link.state == WBR_HB_NEED_PRODUCT && reply_value == 1);
    puts("PASS product query is observed without reply and state survives later heartbeat");

    fresh(&link); feed(&link, query, 7, 1);
    assert(!link.product_queries && !requests && link.ignored == 1);
    puts("PASS pre-handshake query cannot count as bidirectional success");

    fresh(&link); feed(&link, hb, 7, 1); feed(&link, hb, 7, 2);
    assert(requests == 1 && link.heartbeats == 2 && !link.replies);
    wbr_heartbeat_sent(&link, 3); wbr_heartbeat_sent(&link, 4);
    assert(link.replies == 1);
    puts("PASS pending duplicate cannot restart TX or duplicate completion");

    uint8_t data[135], payload[128] = {0};
    fresh(&link); unsigned size = make_frame(data, 3, 0, 0, 0); feed(&link, data, size, 1);
    size = make_frame(data, 0, 0, payload, 1); feed(&link, data, size, 1);
    size = make_frame(data, 0, 6, 0, 0); feed(&link, data, size, 1);
    assert(!requests && link.ignored == 3);
    puts("PASS wrong version, nonempty heartbeat and DP command never request TX");

    memcpy(payload + 3, hb, 7); fresh(&link);
    size = make_frame(data, 0, 6, payload, 18); feed(&link, data, size, 1);
    assert(!requests && link.parser.frames == 1);
    data[size-1] ^= 1; feed(&link, data, size, 2);
    assert(!requests && link.parser.bad_checksum == 1);
    feed(&link, hb, 7, 3); assert(requests == 1);
    puts("PASS embedded heartbeat in valid or corrupt payload is not executed");

    fresh(&link); uint8_t oversized_header[] = {0x55,0xAA,0,6,0,129};
    feed(&link, oversized_header, 6, 1); feed(&link, hb, 7, 1);
    for (unsigned i = 7; i < 130; ++i) { uint8_t zero = 0; feed(&link, &zero, 1, 1); }
    assert(!requests && link.parser.too_long == 1 && link.parser.drain == 0);
    feed(&link, hb, 7, 2); assert(requests == 1);
    puts("PASS oversized payload is drained without interpreting inner headers");

    fresh(&link); feed(&link, hb, 6, 1);
    assert(!wbr_heartbeat_feed(&link, 0xFF, 8, 2, &reply_value));
    feed(&link, hb, 3, 3); wbr_heartbeat_gap(&link); feed(&link, hb+3, 4, 4);
    assert(!requests && link.uart_errors == 1);
    uint8_t noise[] = {2,3,0x55,0x55}; feed(&link, noise, 4, 5); feed(&link, hb, 7, 5);
    assert(requests == 1);
    puts("PASS UART errors, queue gap and overlapping header recover without partial reply");

    fresh(&link); size = make_frame(data, 0, 9, payload, 128); feed(&link, data, size, 1);
    assert(!requests && link.parser.frames == 1 && !link.parser.used);
    puts("PASS exact maximum payload is bounded and ignored");

    struct { uint32_t before; TuyaFrameParser parser; uint32_t after; } guard = {0};
    guard.before = 0x12345678; guard.after = 0xAABBCCDD; tuya_frame_init(&guard.parser);
    uint32_t random = 1;
    for (unsigned i = 0; i < 100000; ++i) {
        random = random * 1664525U + 1013904223U;
        (void)tuya_frame_feed(&guard.parser, (uint8_t)(random >> 16));
        assert(guard.parser.used < sizeof(guard.parser.data));
        if (i % 100 == 0) { tuya_frame_gap(&guard.parser); }
    }
    assert(guard.before == 0x12345678 && guard.after == 0xAABBCCDD);
    puts("PASS bounded parser stress and canaries");

    tx_ready();
    assert((test_gpiof.MODER & 15) == 14 && test_usart2.CR1 == 0x2024 && test_usart2.DR == 0xA5);
    uint32_t other_modes = test_gpiof.MODER & ~12U, other_af = test_gpiof.AFR[0] & ~0xF0U;
    assert(wbr_uart_hb_start(0, 0));
    assert((test_gpiof.MODER & 12) == 8 && (test_gpiof.MODER & ~12U) == other_modes);
    assert((test_gpiof.AFR[0] & 0xF0) == 0x40 && (test_gpiof.AFR[0] & ~0xF0U) == other_af);
    assert(test_usart2.CR1 == 0x202C && test_usart2.DR == 0xA5 && test_irq_enabled);
    assert(!(test_gpiof.OTYPER & 2) && !(test_gpiof.PUPDR & 12) && !(test_gpiof.OSPEEDR & 12));
    assert(!wbr_uart_hb_start(1, 0));
    puts("PASS real reset, AF4, push-pull and no unsolicited DR writes");

    tx_ready(); transmit(0, 0); transmit(1, 1000);
    assert(!wbr_uart_hb_start(2, 2000));
    puts("PASS exact 8-byte wire replies and wait for TC");

    tx_ready(); assert(wbr_uart_hb_start(0, 100)); test_usart2.SR = 0;
    assert(!wbr_uart_hb_poll(149)); assert(wbr_uart_hb_poll(150) == -1);
    assert(!wbr_uart_hb_start(0, 200) && !(test_usart2.CR1 & 8) && (test_gpiof.MODER & 12) == 12);
    assert(wbr_uart_hb_poll(200) == -1 && test_usart2.DR == 0xA5);
    puts("PASS stuck TXE times out once and locks high impedance without retry");

    tx_ready(); assert(wbr_uart_hb_start(0, 0)); test_usart2.SR = USART_SR_TXE;
    for (unsigned i = 0; i < 8; ++i) { assert(!wbr_uart_hb_poll(i)); }
    assert(!wbr_uart_hb_poll(49)); assert(wbr_uart_hb_poll(50) == -1);
    puts("PASS stuck TC cannot report a successful send");

    tx_ready(); transmit(1, 0xFFFFFFFCU);
    puts("PASS transmit timing survives millisecond counter wrap");

    tx_ready(); assert(wbr_uart_hb_start(0, 0));
    test_usart2.SR = USART_SR_RXNE; test_usart2.DR = 0x55; USART2_IRQHandler();
    uint16_t event; assert(wbr_uart_rx_pop(&event) && event == 0x55);
    assert(wbr_uart_hb_poll(1) == 0 && !wbr_uart_rx_dropped());
    puts("PASS receive IRQ remains usable during bounded transmit");
    return 0;
}
