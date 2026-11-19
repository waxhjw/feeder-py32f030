#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "py32f0xx.h"
#include "wbr_init.h"
#include "wbr_init_config.h"
#include "wbr_uart_rx.h"
#include "wbr_uart_tx.h"
GPIO_TypeDef test_gpiof;
RCC_TypeDef test_rcc;
USART_TypeDef test_usart2;
int test_irq_enabled;
static WbrInit link;
static void send_frame(uint8_t cmd, const uint8_t *data, unsigned length, unsigned corrupt)
{
    uint8_t frame[400] = {0x55,0xAA,0,cmd,(uint8_t)(length>>8),(uint8_t)length};
    assert(length + 7U <= sizeof(frame));
    if (length) { memcpy(frame+6, data, length); }
    for (unsigned i=0; i<length+6U; ++i) { frame[length+6U] += frame[i]; }
    frame[length+6U] ^= (uint8_t)corrupt;
    for (unsigned i=0; i<length+7U; ++i) { wbr_init_feed(&link,frame[i],0,100U+i); }
}
static void complete(uint8_t command)
{
    const WbrInitPacket *p=wbr_init_peek(&link); assert(p && p->data[3]==command);
    unsigned sum=0; for(unsigned i=0;i+1U<p->length;++i){sum+=p->data[i];}
    assert((sum&255U)==p->data[p->length-1U]);
    assert(!wbr_init_peek(&link)); wbr_init_sent(&link);
}
static void hb(void){ send_frame(0,0,0,0); complete(0); }
static void init_to_network(void)
{
    wbr_init_reset(&link); hb(); send_frame(1,0,0,0); complete(1);
    send_frame(2,0,0,0); complete(2); uint8_t status=2; send_frame(3,&status,1,0); complete(3);
}
static void reset_hw(void)
{
    memset(&test_gpiof,0,sizeof(test_gpiof)); memset(&test_rcc,0,sizeof(test_rcc));
    memset(&test_usart2,0,sizeof(test_usart2)); test_gpiof.MODER=0xFE;
    assert(wbr_uart_rx_init(8000000,9600)); wbr_uart_tx_reset();
}
int main(void)
{
    wbr_init_reset(&link); send_frame(1,0,0,0); assert(!wbr_init_peek(&link));
    send_frame(0,0,0,1); assert(!link.heartbeats && link.parser.bad_checksum==1);
    send_frame(0,0,0,0); assert(!link.state && link.count==1);
    send_frame(0,0,0,0); assert(link.count==1); /* 首个发送未完成时合并重复心跳。 */
    const WbrInitPacket *p=wbr_init_peek(&link); assert(p->data[6]==0); wbr_init_sent(&link);
    send_frame(0,0,0,0); p=wbr_init_peek(&link); assert(p->data[6]==1); wbr_init_sent(&link);
    puts("PASS heartbeat commit, duplicate and checksum");

#ifndef FEEDER_PAIR_MODE
    send_frame(2,0,0,0); assert(!link.count);
#endif
    send_frame(1,0,0,0); p=wbr_init_peek(&link);
    assert(p->length==sizeof(FEEDER_TUYA_PRODUCT_JSON)-1U+7U);
    assert(!memcmp(p->data+6,FEEDER_TUYA_PRODUCT_JSON,sizeof(FEEDER_TUYA_PRODUCT_JSON)-1U));
    assert(!link.products_sent); wbr_init_sent(&link); assert(link.state==3);
    send_frame(2,0,0,0); complete(2); assert(link.state==7);
    uint8_t status=4; send_frame(3,&status,1,0); assert(!link.network_acks); complete(3);
    assert(link.network_state==4 && link.state==15);
    send_frame(8,0,0,0); p=wbr_init_peek(&link);
    const uint8_t standby[]={4,4,0,1,0}; assert(p->data[3]==7 && !memcmp(p->data+6,standby,5));
    wbr_init_sent(&link); assert(link.state==31 && link.status_reports==1);
    puts("PASS exact PID, work mode, network ACK and truthful standby");

    init_to_network();
    for(uint8_t n=0;n<7;n++){ send_frame(3,&n,1,0); complete(3); assert(link.network_state==n); }
    status=7; send_frame(3,&status,1,0); assert(!link.count && link.network_state==6);
    puts("PASS all documented network values and invalid value rejection");

    for(unsigned cmd=4;cmd<256;cmd++){
        if(cmd==6 || cmd==8){continue;}
        send_frame((uint8_t)cmd,0,0,0);assert(!link.count);
    }
    const uint8_t wrong_version[]={0x55,0xAA,3,0,0,0,2};
    unsigned old_hb=link.heartbeats;
    for(unsigned i=0;i<sizeof(wrong_version);i++){wbr_init_feed(&link,wrong_version[i],0,0);}
    assert(link.heartbeats==old_hb && !link.count);
    puts("PASS unsupported commands and wrong version never trigger active actions");

    uint8_t dp[]={2,1,0,1,1,3,2,0,4,0,0,0,12};
    send_frame(6,dp,sizeof(dp),0); assert(link.blocked_dp==1 && !link.count);
    dp[12]=13; send_frame(6,dp,sizeof(dp),0); assert(link.malformed==1 && !link.count);
    uint8_t raw[132]={1,0,0,128}; memset(raw+4,0xA5,128); send_frame(6,raw,sizeof(raw),0);
    assert(link.blocked_dp==2 && !link.count);
    send_frame(6,dp,3,0); send_frame(6,0,0,0); assert(link.malformed==3);
    puts("PASS blocked bool/value/raw128, invalid bounds and truncated DP");

    init_to_network(); uint8_t embedded[]={1,0,0,7,0x55,0xAA,0,0,0,0,0xFF};
    unsigned h=link.heartbeats; send_frame(6,embedded,sizeof(embedded),0); assert(link.heartbeats==h && !link.count);
    uint8_t oversize[257]={0x55,0xAA,0,0,0,0,0xFF}; send_frame(6,oversize,257,0);
    assert(link.parser.too_long==1 && link.heartbeats==h); hb();
    wbr_init_feed(&link,0x55,0,0); wbr_init_feed(&link,0xAA,1,0); hb(); assert(link.uart_errors==1);
    puts("PASS embedded heartbeat, oversized drain, UART error recovery");

    init_to_network(); for(unsigned i=0;i<8;i++){send_frame(1,0,0,0);}
    assert(link.count==8 && !link.locked); send_frame(1,0,0,0);
    assert(link.locked && link.tx_failures==1 && !wbr_init_peek(&link)); send_frame(0,0,0,0);
    assert(link.locked && !wbr_init_peek(&link));
    puts("PASS queue overflow sticky lock, no reset by later heartbeat");

    init_to_network();
    for(unsigned i=0;i<100;i++){ send_frame(1,0,0,0); send_frame(2,0,0,0); complete(1); complete(2); }
    assert(link.count==0 && link.products_sent==101 && link.work_sent==101);
    puts("PASS queue FIFO wrap and repeated initialization requests");

    reset_hw(); assert((test_gpiof.MODER&12)==12 && !(test_usart2.CR1&8));
    uint8_t wire[]={0x55,0xAA,3,2,0,0,4};
    assert(!wbr_uart_tx_start(wire,65,0) && !wbr_uart_tx_start(0,7,0));
    assert(wbr_uart_tx_start(wire,7,0) && !wbr_uart_tx_start(wire,7,0));
    assert((test_gpiof.MODER&12)==8 && (test_gpiof.AFR[0]&0xF0)==0x40);
    for(unsigned i=0;i<7;i++){
        test_usart2.SR=USART_SR_TXE; assert(!wbr_uart_tx_poll(i)); assert(test_usart2.DR==wire[i]);
    }
    test_usart2.SR=0; assert(!wbr_uart_tx_poll(8));
    test_usart2.SR=USART_SR_TC; assert(wbr_uart_tx_poll(9)==1);
    puts("PASS TX exact bytes, AF4, busy rejection and final TC");

    reset_hw(); assert(wbr_uart_tx_start(wire,7,0)); test_usart2.SR=0;
    assert(wbr_uart_tx_poll(200)==-1 && (test_gpiof.MODER&12)==12 && !(test_usart2.CR1&8));
    assert(!wbr_uart_tx_start(wire,7,201) && wbr_uart_tx_poll(202)==-1);
    reset_hw(); assert(wbr_uart_tx_start(wire,7,UINT32_MAX-3U)); test_usart2.SR=USART_SR_TXE;
    for(unsigned i=0;i<7;i++){assert(!wbr_uart_tx_poll(UINT32_MAX-3U+i));}
    test_usart2.SR=USART_SR_TC; assert(wbr_uart_tx_poll(4)==1);
    reset_hw(); assert(wbr_uart_tx_start(wire,7,0)); test_usart2.SR=USART_SR_TXE;
    for(unsigned i=0;i<7;i++){assert(!wbr_uart_tx_poll(i));}
    test_usart2.SR=0; assert(wbr_uart_tx_poll(200)==-1);
    puts("PASS TXE/TC timeouts, sticky stop and uint32 timer wrap");

    struct { uint32_t before; WbrInit subject; uint32_t after; } guarded;
    guarded.before=0x12345678;guarded.after=0x87654321;wbr_init_reset(&guarded.subject);
    uint32_t random=123;
    for(unsigned i=0;i<100000;i++){
        random=random*1664525U+1013904223U;
        wbr_init_feed(&guarded.subject,(uint8_t)(random>>24),0,i);
        if(i%79U==0){wbr_init_gap(&guarded.subject);}
    }
    assert(guarded.before==0x12345678 && guarded.after==0x87654321);
    puts("PASS 100k noisy bytes preserve buffer canaries");
    return 0;
}
