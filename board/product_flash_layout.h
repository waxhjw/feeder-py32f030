#ifndef FEEDER_PRODUCT_FLASH_LAYOUT_H
#define FEEDER_PRODUCT_FLASH_LAYOUT_H
#include "ota_layout.h"
#include "product_store.h"
#define PRODUCT_FLASH_BASE 0x0800E100UL
#define PRODUCT_FLASH_END (PRODUCT_FLASH_BASE+PRODUCT_STORE_PAGE*PRODUCT_STORE_SLOTS)
_Static_assert(PRODUCT_STORE_PAGE==OTA_PAGE,"same physical page size");
_Static_assert(PRODUCT_FLASH_BASE>=OTA_PENDING+OTA_PAGE,"no OTA overlap");
_Static_assert(PRODUCT_FLASH_END<=0x08010000UL,"64KB bound");
int product_flash_write(uint32_t address,const uint32_t *data);
#endif
