#ifndef FEEDER_PRODUCT_STORE_H
#define FEEDER_PRODUCT_STORE_H
#include "meal_plan.h"
#define PRODUCT_STORE_PAGE 128U
#define PRODUCT_STORE_SLOTS 32U /* 16组双副本轮换；接口仅使用槽号，没有实板地址。 */
enum { PRODUCT_RESULT_NONE,PRODUCT_RESULT_DONE,PRODUCT_RESULT_CANCELLED,PRODUCT_RESULT_FAULT,PRODUCT_RESULT_INTERRUPTED };
enum { PRODUCT_SOURCE_NONE,PRODUCT_SOURCE_MANUAL,PRODUCT_SOURCE_PLAN };
enum { PRODUCT_STORE_OK,PRODUCT_STORE_EMPTY,PRODUCT_STORE_CORRUPT,PRODUCT_STORE_IO };
typedef struct {
    MealPlan plans[MEAL_PLAN_LIMIT];
    uint32_t count,watermark,task_id,active,source,target,completed,result,plan_index;
    uint32_t skip_minute,skip_reason,skip_index;
} ProductData;
typedef struct {
    void *context;
    int (*read)(void *context,uint32_t slot,uint8_t data[PRODUCT_STORE_PAGE]);
    /* 实板适配必须只擦写指定槽对应的一页并读回，禁止全片擦除；本轮只有模型。 */
    int (*write)(void *context,uint32_t slot,const uint8_t data[PRODUCT_STORE_PAGE]);
} ProductStorage;
void product_data_empty(ProductData *data);
int product_data_valid(const ProductData *data);
uint32_t product_store_load(const ProductStorage *storage,ProductData *data,uint32_t *sequence,uint32_t *slot);
uint32_t product_store_save(const ProductStorage *storage,const ProductData *data);
#ifdef FEEDER_STORE_CACHED
/* 单写者会话游标：上电完整扫描，任何保存错误后必须重新打开，禁止带旧游标重试。 */
typedef struct { uint32_t status,sequence,slot; } ProductCursor;
uint32_t product_store_open(const ProductStorage *storage,ProductData *data,ProductCursor *cursor);
uint32_t product_store_save_cursor(const ProductStorage *storage,const ProductData *previous,
    const ProductData *data,ProductCursor *cursor);
#endif
#endif
