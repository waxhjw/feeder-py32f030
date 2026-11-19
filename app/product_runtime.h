#ifndef FEEDER_PRODUCT_RUNTIME_H
#define FEEDER_PRODUCT_RUNTIME_H
#include "product_store.h"
typedef struct { uint32_t valid,task_id,source,plan_index,portions; } ProductAction;
typedef struct {
    ProductStorage storage;ProductData data;MealClock clock;MealScheduler scheduler;
    uint32_t storage_ready,storage_error,recovered_interruption;
#ifdef FEEDER_STORE_CACHED
    ProductCursor cursor;
#endif
} ProductRuntime;
uint32_t product_runtime_init(ProductRuntime *s,const ProductStorage *storage,uint32_t now,uint32_t holdover_ms,uint32_t max_gap_ms);
int product_runtime_set_plans(ProductRuntime *s,uint32_t now,const MealPlan *plans,uint32_t count);
/* block_reason仅允许NONE/BUSY/UPGRADE/NOT_READY；其他值保守按NOT_READY处理。 */
MealEvent product_runtime_poll(ProductRuntime *s,uint32_t now,uint32_t block_reason,ProductAction *action);
/* ready必须来自已验证机构/保护层；本轮仅模型提供，实板未接入。 */
int product_runtime_manual(ProductRuntime *s,uint32_t portions,uint32_t ready,ProductAction *action);
/* 调用前由执行层先撤销驱动；本接口只保存结果，不操作实际电机。 */
int product_runtime_finish(ProductRuntime *s,uint32_t result,uint32_t completed);
#ifdef FEEDER_PLAN_APP
/* 停机后保存运动期间在RAM累计的跳过进度，不产生运行请求。 */
int product_runtime_progress(ProductRuntime *s,const MealEvent *event);
#endif
#endif
