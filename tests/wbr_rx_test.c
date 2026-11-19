#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "py32f0xx.h"
#include "wbr_rx_observer.h"
#include "wbr_uart_rx.h"

GPIO_TypeDef test_gpiof;
RCC_TypeDef test_rcc;
USART_TypeDef test_usart2;
int test_irq_enabled;
static const uint8_t heartbeat[] = {0x55,0xAA,0,0,0,0,0xFF};
static void feed(WbrRxObserver *o, const uint8_t *data, unsigned size)
{ for (unsigned i = 0; i < size; ++i) { wbr_rx_observer_feed(o, data[i], 0); } }
static void inject(unsigned value, unsigned error)
{ test_usart2.SR = USART_SR_RXNE | error; test_usart2.DR = value; USART2_IRQHandler(); }

int main(void)
{
    WbrRxObserver o;
    wbr_rx_observer_init(&o);
    feed(&o, heartbeat, 3); assert(o.rates[0].frames == 0);
    feed(&o, heartbeat + 3, 4); feed(&o, heartbeat, 7);
    assert(o.rates[0].frames == 2 && o.rates[0].heartbeats == 2);
    assert(o.rates[0].first_frame_len == 7 && !memcmp(o.rates[0].first_frame, heartbeat, 7));
    puts("PASS fragmented and concatenated heartbeat");

    wbr_rx_observer_init(&o);
    uint8_t noise[] = {3,4,0x55,0x55}; feed(&o, noise, sizeof(noise)); feed(&o, heartbeat, 7);
    assert(o.rates[0].heartbeats == 1); puts("PASS noisy prefix and overlapping header");

    uint8_t bad[] = {0x55,0xAA,0,0,0,0,0}; feed(&o, bad, 7); feed(&o, heartbeat, 7);
    assert(o.rates[0].heartbeats == 2 && o.rates[0].bad_checksum == 1);
    puts("PASS checksum rejection and resynchronization");

    uint8_t oversize[] = {0x55,0xAA,0,1,0xFF,0xFF};
    feed(&o, oversize, sizeof(oversize)); feed(&o, heartbeat, 7);
    assert(o.rates[0].too_long == 1 && o.rates[0].heartbeats == 3);
    puts("PASS oversized length rejection");

    wbr_rx_observer_init(&o);
    uint8_t maximum[WBR_MAX_FRAME] = {0x55,0xAA,0,0x42,0,WBR_MAX_PAYLOAD};
    uint8_t sum = 0; for (unsigned i = 0; i < sizeof(maximum)-1; ++i) { sum += maximum[i]; }
    maximum[sizeof(maximum)-1] = sum;
    feed(&o, maximum, sizeof(maximum));
    assert(o.rates[0].frames == 1 && o.rates[0].heartbeats == 0 && o.used == 0);
    assert(o.rates[0].first_frame_len == sizeof(maximum) && o.rates[0].last_command == 0x42);
    assert(!memcmp(o.rates[0].first_frame, maximum, 64)); puts("PASS bounded maximum payload capture");

    wbr_rx_observer_init(&o); feed(&o, heartbeat, 3);
    wbr_rx_observer_discontinuity(&o); feed(&o, heartbeat + 3, 4);
    assert(o.rates[0].frames == 0); feed(&o, heartbeat, 7);
    assert(o.rates[0].heartbeats == 1); puts("PASS discontinuity discards partial frame");

    feed(&o, heartbeat, 3); wbr_rx_observer_select(&o, 1); feed(&o, heartbeat + 3, 4);
    assert(o.rates[1].frames == 0); feed(&o, heartbeat, 7);
    assert(o.rates[0].heartbeats == 1 && o.rates[1].heartbeats == 1);
    puts("PASS independent rate windows without cross-window splicing");

    wbr_rx_observer_init(&o); feed(&o, heartbeat, 6); wbr_rx_observer_feed(&o, 0xFF, 8);
    assert(o.rates[0].frames == 0 && o.rates[0].uart_errors == 1);
    feed(&o, heartbeat, 7); assert(o.rates[0].heartbeats == 1);
    puts("PASS UART errors cannot count as heartbeat");

    struct { uint32_t before; WbrRxObserver parser; uint32_t after; } guarded = {0};
    guarded.before = 0xDEADBEEF; guarded.after = 0xBADC0FFE;
    wbr_rx_observer_init(&guarded.parser); uint32_t random = 17;
    for (unsigned i = 0; i < 100000; ++i) {
        random = random * 1664525U + 1013904223U;
        wbr_rx_observer_feed(&guarded.parser, (uint8_t)(random >> 16U), 0);
        assert(guarded.parser.used < WBR_MAX_FRAME);
    }
    assert(guarded.before == 0xDEADBEEF && guarded.after == 0xBADC0FFE);
    assert(guarded.parser.rates[0].raw_len == 64); puts("PASS noise stress and buffer canaries");

    test_gpiof.MODER = 0x55555555; test_gpiof.PUPDR = 0xAAAAAAAA; test_gpiof.AFR[0] = 0x12345678;
    test_usart2.DR = 0xA5;
    assert(wbr_uart_rx_init(8000000, 9600));
    assert(test_gpiof.MODER == 0x55555556 && test_gpiof.PUPDR == 0xAAAAAAA8);
    assert(test_gpiof.AFR[0] == 0x12345674 && test_usart2.BRR == 833);
    assert(test_usart2.CR1 == 0x2024 && test_usart2.CR2 == 0 && test_usart2.CR3 == 1);
    assert(test_usart2.DR == 0xA5 && test_irq_enabled);
    puts("PASS RX-only configuration preserves PF1 and unrelated pins");

    test_gpiof.MODER = 0xFFFFFCFFU; /* 参考手册V1.7第98页真实复位值。 */
    assert(wbr_uart_rx_init(8000000, 9600));
    assert(test_gpiof.MODER == 0xFFFFFCFEU);
    assert(((test_gpiof.MODER >> 2U) & 3U) == 3U && test_usart2.CR1 == 0x2024);
    puts("PASS real GPIOF reset leaves PF1 analog and transmitter disabled");

    assert(wbr_uart_rx_set_baud(8000000, 115200)); assert(test_usart2.BRR == 69 && test_usart2.CR1 == 0x2024);
    assert(!wbr_uart_rx_set_baud(8000000, 12345) && test_usart2.BRR == 69);
    assert(!wbr_uart_rx_set_baud(1, 9600) && test_usart2.BRR == 69);
    puts("PASS documented baud rates and invalid divider rejection");

    uint16_t event;
    inject(0x31, 0); inject(0x42, USART_SR_FE);
    assert(wbr_uart_rx_pop(&event) && event == 0x31);
    assert(wbr_uart_rx_pop(&event) && event == 0x242);
    assert(!wbr_uart_rx_pop(&event)); puts("PASS interrupt queue preserves byte/error ordering");

    for (unsigned i = 0; i < 256; ++i) { inject(i, 0); }
    assert(wbr_uart_rx_dropped() == 1);
    for (unsigned i = 0; i < 255; ++i) { assert(wbr_uart_rx_pop(&event) && event == i); }
    inject(0xA5, 0); assert(wbr_uart_rx_pop(&event) && event == 0x10A5);
    assert(!wbr_uart_rx_pop(&event)); puts("PASS queue overflow marks exact discontinuity and recovers");
    return 0;
}
