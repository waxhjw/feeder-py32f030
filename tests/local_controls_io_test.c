#include "py32f0xx.h"
#include "local_controls_io.h"
#include "touch_motor_io.h"
#include <assert.h>
#include <stdio.h>
GPIO_TypeDef touch_gpioa,touch_gpiob,input_gpiof;
RCC_TypeDef touch_rcc;
DBG_TypeDef touch_dbg;
IWDG_TypeDef touch_iwdg;
TIM_TypeDef model_tim1;
uint32_t SystemCoreClock=8000000U;
void SystemCoreClockUpdate(void) { }
int ota_receive(uint8_t cmd,const uint8_t *data,uint32_t size,uint32_t now)
{ (void)cmd;(void)data;(void)size;(void)now;return 0; }
static WbrInit link;
static uint32_t now,reported_feed,reported_quick;
static void drain(void)
{
    const WbrInitPacket *p;
    while((p=wbr_init_peek(&link))) {
        if(p->data[3]==7U) {
            for(unsigned i=6U;i+4U<p->length-1U;i+=5U) {
                if(p->data[i]==4U) { reported_feed=p->data[i+4U]; }
                if(p->data[i]==2U) { reported_quick=p->data[i+4U]; }
            }
        }
        wbr_init_sent(&link);
    }
}
static void tick(unsigned count,unsigned levels)
{
    touch_gpiob.IDR=levels;
    for(unsigned i=0;i<count;i++) {
        local_controls_io_poll(++now,0U);
        /* GPIO模型只执行BSRR语义，不把模型输出冒充实物电机反馈。 */
        touch_gpioa.ODR=(touch_gpioa.ODR&~(touch_gpioa.BSRR>>16U))|(touch_gpioa.BSRR&0xFFFFU);
        drain();
    }
}
static unsigned output(void) { return touch_gpioa.ODR&0x900U; }
static void fresh(void)
{
    wbr_init_reset(&link);link.state=9U;now=0U;reported_feed=reported_quick=99U;
    touch_gpiob.MODER=touch_gpiob.PUPDR=0xFFFFFFFFU;
    local_controls_io_init(&link,now);tick(2100U,0xC0U);
    assert(!output() && !reported_feed);
    assert(!(touch_gpiob.MODER&0xF000U) && !(touch_gpiob.PUPDR&0xF000U));
}
static void packet(const uint8_t *data,unsigned size)
{
    uint8_t frame[80]={0x55,0xAA,0,6,0,0},sum=0;frame[5]=(uint8_t)size;
    for(unsigned i=0;i<size;i++) { frame[6U+i]=data[i]; }
    for(unsigned i=0;i<6U+size;i++) { sum+=frame[i]; }
    frame[6U+size]=sum;
    for(unsigned i=0;i<7U+size;i++) { wbr_init_feed(&link,frame[i],0U,now); }
    tick(1U,0xC0U);
}
static void dp(unsigned id,unsigned value)
{ uint8_t data[]={(uint8_t)id,1,0,1,(uint8_t)value};packet(data,sizeof(data)); }
static void tap(unsigned key)
{ tick(150U,key==1U?0x40U:0x80U);tick(150U,0xC0U); }
int main(void)
{
    fresh();assert(local_controls_display()[4]==0x06U);
    puts("PASS actual adapter init no-pull inputs, boot stopped and quantity1 display");
    tap(1U);assert(output()==0x100U);dp(102U,1U);assert(!output());
    dp(101U,1U);assert(output()==0x100U);tap(1U);assert(!output());
    puts("PASS real UART parser and actual GPIO driver: local start App stop and App start local stop");
    fresh();dp(101U,1U);tick(100U,0xC0U);assert(reported_feed==1U);
    dp(103U,1U);assert(!output());tick(298U,0xC0U);assert(!output());
    tick(2U,0xC0U);assert(output()==0x800U);tick(100U,0xC0U);assert(reported_feed==1U);
    dp(103U,0U);dp(101U,0U);tick(400U,0xC0U);assert(!output());
    puts("PASS reversal has300ms off then reverse, reports recover, stop cancels pending restart");
    fresh();uint8_t invalid[]={101,1,0,1,1,103,1,0,1,2};packet(invalid,sizeof(invalid));assert(!output());
    uint8_t conflict[]={101,1,0,1,1,101,1,0,1,0};packet(conflict,sizeof(conflict));assert(!output());
    uint8_t stop_first[]={101,1,0,1,1,102,1,0,1,1};packet(stop_first,sizeof(stop_first));assert(!output());
    uint8_t mixed[]={101,1,0,1,1,99,1,0,1,1};packet(mixed,sizeof(mixed));assert(!output());
    puts("PASS full UART payload validation rejects malformed conflict unknown mix before any output, stop wins");
    fresh();uint8_t quantity[]={3,2,0,4,0,0,0,12};packet(quantity,sizeof(quantity));
    assert(!output() && local_controls_display()[3]==0x06U && local_controls_display()[4]==0x5BU);
    tap(2U);assert(local_controls_display()[4]==0x06U);
    puts("PASS DP3 quantity selection only never starts, TOP2 wraps12 to1");
    fresh();tick(3100U,0x40U);tick(150U,0xC0U);tap(1U);assert(!output());
    dp(101U,1U);assert(output()==0x100U);tap(1U);assert(!output());
    puts("PASS child lock blocks local start, retains remote start and local stop");
    fresh();dp(2U,1U);unsigned start=now-1U;assert(output()==0x100U);
    tick(1000U,0xC0U);dp(2U,1U);dp(101U,1U);tick(6496U,0xC0U);
    assert(now-start==7499U && output()==0x100U);tick(1U,0xC0U);assert(!output());
    tick(100U,0xC0U);assert(!reported_feed && !reported_quick);
    puts("PASS actual UART quick path stops at7.5s despite repeats and continuous-start request, reports off");
    fresh();tick(200U,0x40U);dp(102U,1U);tick(300U,0xC0U);assert(!output());
    dp(101U,1U);assert(output());local_controls_io_poll(++now,1U);assert(touch_gpioa.BSRR==0x09000000U);
    link.ota_block_dp=1U;dp(101U,1U);assert(!output());
    puts("PASS App stop cancels held touch gesture, upgrade quiesces and blocks restart");
    return 0;
}
