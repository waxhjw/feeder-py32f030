#include "product_store.h"
#include <string.h>
#ifdef FEEDER_STORE_PROFILE
#include "store_profile.h"
#endif
#define STORE_MAGIC 0x31525046UL
_Static_assert(sizeof(ProductData)==128U,"canonical product data has no implicit padding");
static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0]|((uint32_t)p[1]<<8U)|((uint32_t)p[2]<<16U)|((uint32_t)p[3]<<24U);
}
static void put32(uint8_t *p,uint32_t value)
{
    for(uint32_t i=0;i<4U;i++) { p[i]=(uint8_t)(value>>(i*8U)); }
}
static uint32_t crc(const uint8_t *p)
{
#ifdef FEEDER_STORE_PROFILE
    uint32_t start=store_profile_us();g_store_profile.crcs++;
#endif
    uint32_t value=0xFFFFFFFFU;
    for(uint32_t j=0;j<124U;j++) {
        value^=p[j];
        for(uint32_t i=0;i<8U;i++) { value=(value>>1U)^((0U-(value&1U))&0xEDB88320U); }
    }
#ifdef FEEDER_STORE_PROFILE
    g_store_profile.crc_us+=store_profile_us()-start;
#endif
    return ~value;
}
void product_data_empty(ProductData *data)
{
    memset(data,0,sizeof(*data));data->watermark=MEAL_TIME_NONE;data->plan_index=MEAL_TIME_NONE;
    data->skip_minute=MEAL_TIME_NONE;data->skip_index=MEAL_TIME_NONE;
}
int product_data_valid(const ProductData *d)
{
    if(meal_plans_valid(d->plans,d->count)==0 || (d->watermark!=MEAL_TIME_NONE && d->watermark>=MEAL_MINUTE_LIMIT) ||
       d->active>1U || d->source>PRODUCT_SOURCE_PLAN || d->target>12U || d->completed>d->target ||
       d->result>PRODUCT_RESULT_INTERRUPTED) { return 0; }
    if(d->active && (d->source==PRODUCT_SOURCE_NONE || d->target==0U || d->task_id==0U || d->result!=PRODUCT_RESULT_NONE)) { return 0; }
    if(d->source==PRODUCT_SOURCE_PLAN && d->plan_index>=MEAL_PLAN_LIMIT) { return 0; }
    if(d->source==PRODUCT_SOURCE_PLAN && d->watermark==MEAL_TIME_NONE) { return 0; }
    if(d->source!=PRODUCT_SOURCE_PLAN && d->plan_index!=MEAL_TIME_NONE) { return 0; }
    if(d->result==PRODUCT_RESULT_DONE && (d->target==0U || d->completed!=d->target)) { return 0; }
    if(d->skip_reason==MEAL_REASON_NONE) {
        if(d->skip_minute!=MEAL_TIME_NONE || d->skip_index!=MEAL_TIME_NONE) { return 0; }
    } else if(d->skip_reason<MEAL_REASON_LATE || d->skip_reason>MEAL_REASON_NOT_READY ||
              d->skip_minute>=MEAL_MINUTE_LIMIT || d->skip_index>=MEAL_PLAN_LIMIT) { return 0; }
    for(uint32_t i=d->count;i<MEAL_PLAN_LIMIT;i++) {
        static const MealPlan empty={0};if(memcmp(&d->plans[i],&empty,sizeof(empty))) { return 0; }
    }
    return 1;
}
static void encode(uint8_t *page,const ProductData *d,uint32_t sequence)
{
    memset(page,0xFFU,PRODUCT_STORE_PAGE);put32(page,STORE_MAGIC);put32(page+4U,1U);put32(page+8U,sequence);
    put32(page+12U,d->watermark);put32(page+16U,d->task_id);
    page[20]=d->active;page[21]=d->source;page[22]=d->target;page[23]=d->completed;
    put32(page+24U,d->result);put32(page+28U,d->count);memcpy(page+32U,d->plans,80U);
    put32(page+112U,d->plan_index);put32(page+116U,d->skip_minute);
    put32(page+120U,d->skip_reason==MEAL_REASON_NONE?0xFFFFFFFFU:(d->skip_reason|(d->skip_index<<8U)));
    put32(page+124U,crc(page));
}
static int decode(const uint8_t *page,ProductData *d)
{
    if(get32(page)!=STORE_MAGIC || get32(page+4U)!=1U || get32(page+124U)!=crc(page)) { return 0; }
    product_data_empty(d);d->watermark=get32(page+12U);d->task_id=get32(page+16U);
    d->active=page[20];d->source=page[21];d->target=page[22];d->completed=page[23];
    d->result=get32(page+24U);d->count=get32(page+28U);memcpy(d->plans,page+32U,80U);d->plan_index=get32(page+112U);
    d->skip_minute=get32(page+116U);uint32_t info=get32(page+120U);
    if(info!=0xFFFFFFFFU) {
        if(info&0xFFFF0000U) { return 0; }
        d->skip_reason=info&0xFFU;d->skip_index=(info>>8U)&0xFFU;
    }
    return product_data_valid(d);
}
static int newer(uint32_t a,uint32_t b)
{
    uint32_t delta=a-b;return delta>0U && delta<0x80000000U;
}
uint32_t product_store_load(const ProductStorage *storage,ProductData *data,uint32_t *sequence,uint32_t *slot)
{
#ifdef FEEDER_STORE_PROFILE
    uint32_t start=store_profile_us();g_store_profile.loads++;
#endif
    uint8_t page[PRODUCT_STORE_PAGE],chosen[PRODUCT_STORE_PAGE];ProductData candidate;
    uint32_t found=0U,nonempty=0U,best=0U,best_slot=0U;
    if(storage==0 || storage->read==0) { return PRODUCT_STORE_IO; }
    for(uint32_t i=0;i<PRODUCT_STORE_SLOTS;i++) {
        if(storage->read(storage->context,i,page)==0) { return PRODUCT_STORE_IO; }
        if(get32(page)==STORE_MAGIC && get32(page+4U)!=1U && get32(page+124U)==crc(page)) { return PRODUCT_STORE_CORRUPT; }
        for(uint32_t j=0;j<PRODUCT_STORE_PAGE;j++) { if(page[j]!=0xFFU) { nonempty=1U;break; } }
        if(decode(page,&candidate)) {
            uint32_t seq=get32(page+8U);
            if(found && seq==best && memcmp(chosen,page,PRODUCT_STORE_PAGE)) { return PRODUCT_STORE_CORRUPT; }
            if(found && (uint32_t)(seq-best)==0x80000000U) { return PRODUCT_STORE_CORRUPT; }
            if(found==0U || newer(seq,best)) { memcpy(chosen,page,sizeof(chosen));best=seq;best_slot=i;found=1U; }
        }
    }
    if(found==0U) { return nonempty?PRODUCT_STORE_CORRUPT:PRODUCT_STORE_EMPTY; }
    /* 若两份最新记录都坏、但还能辨认更新代号，不静默回退后重复动作。 */
    for(uint32_t i=0;i<PRODUCT_STORE_SLOTS;i++) {
        if(storage->read(storage->context,i,page)==0) { return PRODUCT_STORE_IO; }
        if(get32(page)==STORE_MAGIC && get32(page+4U)==1U && newer(get32(page+8U),best)) { return PRODUCT_STORE_CORRUPT; }
    }
    if(decode(chosen,data)==0) { return PRODUCT_STORE_CORRUPT; }
    *sequence=best;*slot=best_slot;
#ifdef FEEDER_STORE_PROFILE
    g_store_profile.load_us+=store_profile_us()-start;
#endif
    return PRODUCT_STORE_OK;
}
uint32_t product_store_save(const ProductStorage *storage,const ProductData *data)
{
    ProductData previous;uint8_t page[PRODUCT_STORE_PAGE],verify[PRODUCT_STORE_PAGE];uint32_t sequence=0U,slot=0U;
    if(storage==0 || storage->read==0 || storage->write==0) { return PRODUCT_STORE_IO; }
    if(product_data_valid(data)==0) { return PRODUCT_STORE_CORRUPT; }
    uint32_t status=product_store_load(storage,&previous,&sequence,&slot);
    if(status!=PRODUCT_STORE_OK && status!=PRODUCT_STORE_EMPTY) { return status; }
    if(status==PRODUCT_STORE_OK && (data->task_id<previous.task_id ||
       (previous.watermark!=MEAL_TIME_NONE && (data->watermark==MEAL_TIME_NONE || data->watermark<previous.watermark)))) {
        return PRODUCT_STORE_CORRUPT;
    }
    if(status==PRODUCT_STORE_OK && memcmp(&previous,data,sizeof(previous))==0) { return PRODUCT_STORE_OK; }
    uint32_t next=(status==PRODUCT_STORE_EMPTY)?0U:((slot/2U+1U)%(PRODUCT_STORE_SLOTS/2U))*2U;
    sequence=(status==PRODUCT_STORE_EMPTY)?1U:sequence+1U;encode(page,data,sequence);
    /* 只写下一组，不擦当前组；两份写入及读回均成功后才允许发运行请求。 */
    for(uint32_t copy=0;copy<2U;copy++) {
        if(storage->write(storage->context,next+copy,page)==0) { return PRODUCT_STORE_IO; }
        if(storage->read(storage->context,next+copy,verify)==0 || memcmp(page,verify,sizeof(page))) { return PRODUCT_STORE_IO; }
    }
    return PRODUCT_STORE_OK;
}
#ifdef FEEDER_STORE_CACHED
uint32_t product_store_open(const ProductStorage *storage,ProductData *data,ProductCursor *cursor)
{
    cursor->sequence=0U;cursor->slot=0U;product_data_empty(data);
    cursor->status=product_store_load(storage,data,&cursor->sequence,&cursor->slot);
    return cursor->status;
}
static uint32_t cursor_fail(ProductCursor *cursor,uint32_t reason)
{
    cursor->status=reason;return reason;
}
uint32_t product_store_save_cursor(const ProductStorage *storage,const ProductData *previous,
    const ProductData *data,ProductCursor *cursor)
{
    uint8_t page[PRODUCT_STORE_PAGE],verify[PRODUCT_STORE_PAGE];ProductData decoded;
    if(cursor->status!=PRODUCT_STORE_OK && cursor->status!=PRODUCT_STORE_EMPTY) { return cursor->status; }
    if(storage==0 || storage->read==0 || storage->write==0) { return cursor_fail(cursor,PRODUCT_STORE_IO); }
    if(product_data_valid(data)==0 || data->task_id<previous->task_id ||
       (previous->watermark!=MEAL_TIME_NONE && (data->watermark==MEAL_TIME_NONE || data->watermark<previous->watermark))) {
        return cursor_fail(cursor,PRODUCT_STORE_CORRUPT);
    }
    if(cursor->status==PRODUCT_STORE_OK) {
        uint32_t matches=0U,base=(cursor->slot/2U)*2U;
        for(uint32_t copy=0U;copy<2U;copy++) {
            if(storage->read(storage->context,base+copy,page)==0) { return cursor_fail(cursor,PRODUCT_STORE_IO); }
            if(get32(page)==STORE_MAGIC && get32(page+4U)!=1U && get32(page+124U)==crc(page)) {
                return cursor_fail(cursor,PRODUCT_STORE_CORRUPT);
            }
            if(decode(page,&decoded)) {
                if(get32(page+8U)!=cursor->sequence || memcmp(&decoded,previous,sizeof(decoded))) {
                    return cursor_fail(cursor,PRODUCT_STORE_CORRUPT);
                }
                matches++;
            }
        }
        if(matches==0U) { return cursor_fail(cursor,PRODUCT_STORE_CORRUPT); }
        if(memcmp(previous,data,sizeof(*data))==0) { return PRODUCT_STORE_OK; }
    }
    uint32_t next=cursor->status==PRODUCT_STORE_EMPTY?0U:((cursor->slot/2U+1U)%(PRODUCT_STORE_SLOTS/2U))*2U;
    uint32_t sequence=cursor->status==PRODUCT_STORE_EMPTY?1U:cursor->sequence+1U;
    encode(page,data,sequence);
    for(uint32_t copy=0U;copy<2U;copy++) {
        if(storage->write(storage->context,next+copy,page)==0 ||
           storage->read(storage->context,next+copy,verify)==0 || memcmp(page,verify,sizeof(page))) {
            return cursor_fail(cursor,PRODUCT_STORE_IO);
        }
    }
    cursor->sequence=sequence;cursor->slot=next;cursor->status=PRODUCT_STORE_OK;
    return PRODUCT_STORE_OK;
}
#endif
