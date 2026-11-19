#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "py32f0xx.h"
#include "wbr_pair.h"
GPIO_TypeDef test_gpiof; RCC_TypeDef test_rcc; USART_TypeDef test_usart2; int test_irq_enabled;
static WbrInit link;
static void reset(void)
{
    wbr_init_reset(&link);memset((void *)&g_wbr_pair_command,0,sizeof(g_wbr_pair_command));
    g_wbr_pair_command.magic=WBR_PAIR_MAGIC;g_wbr_pair_command.abi=1;
}
static void ready(void){reset();link.state=15;link.network_state=5;link.network_reports=1;}
static void request(uint32_t nonce){g_wbr_pair_command.nonce=nonce;g_wbr_pair_command.nonce_inverse=~nonce;g_wbr_pair_command.command=WBR_PAIR_REQUEST;}
static void receive(const uint8_t *frame,unsigned length)
{
    for(unsigned i=0;i<length;i++){wbr_init_feed(&link,frame[i],0,i);}
}
static void sent(uint8_t command)
{
    const WbrInitPacket *packet=wbr_init_peek(&link);assert(packet && packet->data[3]==command);wbr_init_sent(&link);
}
int main(void)
{
    ready();for(unsigned i=0;i<10000;i++){wbr_pair_poll(&link,i);}
    assert(!link.count && !link.pair_attempts && !g_wbr_pair_command.tx_count);
    puts("PASS no pairing after boot without explicit request");
    reset();request(1);wbr_pair_poll(&link,0);assert(g_wbr_pair_command.result==WBR_PAIR_NOT_READY && !link.count);
    ready();g_wbr_pair_command.command=WBR_PAIR_REQUEST;g_wbr_pair_command.nonce=1;g_wbr_pair_command.nonce_inverse=0;
    wbr_pair_poll(&link,0);assert(g_wbr_pair_command.result==WBR_PAIR_INVALID && !link.count);
    ready();request(2);g_wbr_pair_command.command=123;wbr_pair_poll(&link,0);assert(!link.count);
    puts("PASS incomplete initialization, torn nonce and unknown command blocked");
    ready();request(3);wbr_pair_poll(&link,100);assert(link.count==1 && link.pair_attempts==1 && g_wbr_pair_command.result==1);
    const WbrInitPacket *p=wbr_init_peek(&link);const uint8_t expected[]={0x55,0xAA,3,4,0,0,6};
    assert(p && p->length==7 && !memcmp(p->data,expected,7));assert(!link.pair_transmitted);
    wbr_init_sent(&link);wbr_pair_poll(&link,200);assert(g_wbr_pair_command.result==2 && link.pair_transmitted==1);
    /* 旧0x05 ACK不能冒充本次0x04请求已被接受。 */
    const uint8_t old_ack[]={0x55,0xAA,0,5,0,0,4};receive(old_ack,sizeof(old_ack));
    assert(!link.pair_acks);
    const uint8_t ack[]={0x55,0xAA,0,4,0,0,3};
    for(unsigned i=0;i<7;i++){wbr_init_feed(&link,ack[i],0,200+i);}
    wbr_pair_poll(&link,300);assert(g_wbr_pair_command.result==3 && link.pair_acks==1);
    link.network_state=1;link.network_reports++;wbr_pair_poll(&link,400);assert(g_wbr_pair_command.result==4);
    puts("PASS exact single ordinary04 reset frame, TC commit, matching ACK only, fresh pairing network state");
    request(4);wbr_pair_poll(&link,500);assert(!link.count && link.pair_attempts==1 && g_wbr_pair_command.result==WBR_PAIR_ALREADY_REQUESTED);
    request(4);wbr_pair_poll(&link,600);assert(!link.count && g_wbr_pair_command.result==WBR_PAIR_INVALID);
    puts("PASS repeated request and replay cannot send a second reset");
    ready();request(5);wbr_pair_poll(&link,UINT32_MAX-10U);
    p=wbr_init_peek(&link);assert(p);wbr_init_sent(&link);wbr_pair_poll(&link,20000);
    assert(g_wbr_pair_command.result==WBR_PAIR_TIMEOUT && !link.count);
    wbr_pair_poll(&link,40000);assert(!link.count && link.pair_attempts==1);
    puts("PASS timeout and uint32 wrap never retry");
    for(unsigned n=0;n<3;n++){
        ready();link.network_state=n==2?6:n;request(n+10);wbr_pair_poll(&link,0);
        assert(g_wbr_pair_command.result==WBR_PAIR_ALREADY_PAIRING && !link.count && !link.pair_attempts);
    }
    puts("PASS already pairing does not clear configuration again");
    ready();link.count=1;request(20);wbr_pair_poll(&link,0);assert(g_wbr_pair_command.result==WBR_PAIR_NOT_READY && !link.pair_attempts);
    puts("PASS busy queue never interleaves active reset");
    reset();
    /* 完整重放0.6.0实板留存的有效帧，不能要求模组重新查询PID。 */
    const uint8_t hb[]={0x55,0xAA,0,0,0,0,0xFF};
    const uint8_t status[]={0x55,0xAA,0,8,0,0,7};
    const uint8_t network[]={0x55,0xAA,0,3,0,1,5,8};
    receive(hb,sizeof(hb));sent(0);
    receive(status,sizeof(status));sent(7);
    receive(network,sizeof(network));sent(3);
    receive(network,sizeof(network));sent(3);
    assert(link.state==25 && !link.products_sent && !link.work_sent && link.network_acks==2 && link.status_reports==1);
    request(30);wbr_pair_poll(&link,100);assert(link.pair_attempts==1 && link.count==1);
    puts("PASS exact captured MCU-restart session, no fake PID/work completion, explicit pair permitted after network ACK");
    /* 0.6.7实板只有00/01/02而没有03；官方方括号表示03可选。 */
    reset();link.state=3;request(31);wbr_pair_poll(&link,100);
    assert(g_wbr_pair_command.result==WBR_PAIR_NOT_READY && !link.count);
    reset();link.state=7;request(32);wbr_pair_poll(&link,100);
    assert(link.pair_attempts==1 && link.count==1 && !link.network_acks);
    p=wbr_init_peek(&link);assert(p && p->length==7 && !memcmp(p->data,expected,7));
    wbr_init_sent(&link);wbr_pair_poll(&link,20001);
    assert(g_wbr_pair_command.result!=WBR_PAIR_READY && !link.network_acks);
    puts("PASS basic00/01/02 permits ordinary reset without optional03, incomplete session blocked, no fake ready");
    reset();wbr_init_info_poll(&link,0);assert(!link.info_attempts && !link.count);
    ready();wbr_init_info_poll(&link,100);
    const WbrInitPacket *query=wbr_init_peek(&link);
    const uint8_t version_query[]={0x55,0xAA,3,0x34,0,2,7,5,0x44};
    assert(query && query->length==9 && !memcmp(query->data,version_query,9));
    wbr_init_sent(&link);assert(link.info_tx==1);
    const char json[]="{\"sw\":\"3.1.69\"}";
    uint8_t response[80]={0x55,0xAA,0,0x34,0,sizeof(json)+1U,7,0};
    memcpy(response+8,json,sizeof(json)-1U);unsigned response_length=sizeof(json)+8U;
    for(unsigned i=0;i+1U<response_length;i++){response[response_length-1U]+=response[i];}
    receive(response,response_length);
    assert(link.info_responses==1 && link.info_result==1 && link.info_length==6 && !strcmp((char *)link.info_version,"3.1.69"));
    for(unsigned now=101;now<10000;now++){wbr_init_info_poll(&link,now);}
    assert(link.info_attempts==1 && !link.count);
    puts("PASS exact read-only version05 query after network ACK, parse version, no repeated queries");
    ready();wbr_init_info_poll(&link,0);sent(0x34);wbr_init_info_poll(&link,5000);
    assert(link.info_result==4 && !link.info_responses && link.info_attempts==1 && !link.count);
    wbr_init_info_poll(&link,10000);assert(!link.count);
    puts("PASS unknown support or missing reply yields unknown version and no resend");
    ready();wbr_init_info_poll(&link,0);sent(0x34);
    response[8+sizeof(json)-3]='x';response[response_length-1U]=0;
    for(unsigned i=0;i+1U<response_length;i++){response[response_length-1U]+=response[i];}
    receive(response,response_length);assert(link.info_result==3 && !link.info_length && !link.info_version[0]);
    puts("PASS malformed version never becomes a firmware or authorization claim");
    reset();link.state=3;wbr_init_info_poll(&link,0);assert(!link.info_attempts);
    link.state=7;wbr_init_info_poll(&link,10);assert(link.info_attempts==1 && link.count==1);
    query=wbr_init_peek(&link);assert(query && !memcmp(query->data,version_query,9));
    puts("PASS read-only version after basic00/01/02 does not wait indefinitely for RF network state");
    ready();assert(!wbr_init_request_scan(&link,4999));
    assert(wbr_init_request_scan(&link,5000));query=wbr_init_peek(&link);
    const uint8_t scan_query[]={0x55,0xAA,3,0x0E,0,0,0x10};
    assert(query && query->length==7 && !memcmp(query->data,scan_query,7));wbr_init_sent(&link);
    const uint8_t unauthorized[]={0x55,0xAA,0,0x0E,0,2,0,1,0x10};receive(unauthorized,sizeof(unauthorized));
    assert(link.scan_responses==1 && link.scan_result==0 && link.scan_detail==1 && !link.pair_attempts);
    assert(!wbr_init_request_scan(&link,6000));
    puts("PASS explicit single fixedSSID scan and exact module unlicensed diagnostic, no pairing reset");
    const uint8_t outcomes[][2]={{0,0},{1,80},{9,3}};
    for(unsigned n=0;n<3;n++){
        ready();assert(wbr_init_request_scan(&link,5000));sent(0x0E);
        uint8_t frame[]={0x55,0xAA,0,0x0E,0,2,outcomes[n][0],outcomes[n][1],0};
        for(unsigned i=0;i<8;i++){frame[8]+=frame[i];}receive(frame,sizeof(frame));
        assert(link.scan_responses==1 && link.scan_result==outcomes[n][0] && link.scan_detail==outcomes[n][1]);
    }
    puts("PASS SSID absent, signal success and future unknown diagnostic kept distinct");
    ready();link.state=7;assert(!wbr_init_request_scan(&link,5000));
    link.state=15;assert(wbr_init_request_scan(&link,5000));sent(0x0E);wbr_init_scan_poll(&link,25000);
    assert(link.scan_timeout && !link.scan_responses && !link.count && !wbr_init_request_scan(&link,30000));
    puts("PASS scan requires network initialization and five seconds; timeout never retries");
    return 0;
}
