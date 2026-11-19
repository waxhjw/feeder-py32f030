#include "py32f0xx.h"
#include "ota_layout.h"
#include <string.h>
/* 依据Puya SDK V1.5.0 LL_FLASH_TIMMING_SEQUENCE_CONFIG_8M及HAL页写流程。
 * Flash忙期间从RAM执行，所有数据源必须位于RAM，屏蔽中断；不更改保护/选项字节。 */
static __attribute__((section(".ramfunc"),noinline)) int flash_page(uint32_t address,const uint32_t *data)
{
    uint32_t mask=__get_PRIMASK();__disable_irq();
    uint32_t limit=1000000U;
    while((FLASH->SR&FLASH_SR_BSY) && --limit) { }
    if(!limit) { __set_PRIMASK(mask);return 0; }
    FLASH->KEYR=0x45670123U;FLASH->KEYR=0xCDEF89ABU;
    if(FLASH->CR&FLASH_CR_LOCK) { __set_PRIMASK(mask);return 0; }
    FLASH->SR=FLASH_SR_EOP|FLASH_SR_WRPERR;
    FLASH->CR|=FLASH_CR_PER|FLASH_CR_EOPIE;
    *(volatile uint32_t *)address=0xFFFFFFFFU;__DSB();
    limit=1000000U;
    while((FLASH->SR&FLASH_SR_BSY) && --limit) { }
    uint32_t good=limit && (FLASH->SR&FLASH_SR_EOP) && !(FLASH->SR&FLASH_SR_WRPERR);
    FLASH->CR&=~FLASH_CR_PER;FLASH->SR=FLASH_SR_EOP|FLASH_SR_WRPERR;
    if(good && data) {
        FLASH->CR|=FLASH_CR_PG;
        for(uint32_t i=0U;i<32U;i++) {
            if(i==31U) { FLASH->CR|=FLASH_CR_PGSTRT; }
            *(volatile uint32_t *)(address+4U*i)=data[i];
        }
        __DSB();limit=1000000U;
        while((FLASH->SR&FLASH_SR_BSY) && --limit) { }
        good=limit && (FLASH->SR&FLASH_SR_EOP) && !(FLASH->SR&FLASH_SR_WRPERR);
        FLASH->CR&=~FLASH_CR_PG;
    }
    FLASH->SR=FLASH_SR_EOP|FLASH_SR_WRPERR;FLASH->CR&=~FLASH_CR_EOPIE;FLASH->CR|=FLASH_CR_LOCK;
    __DSB();__ISB();__set_PRIMASK(mask);return good?1:0;
}
static void timing(void)
{
    uint32_t a=*(const uint32_t *)0x1FFF0F30U,b=*(const uint32_t *)0x1FFF0F34U;
    FLASH->TS0=a&0xFFU;FLASH->TS1=(a>>16U)&0x1FFU;FLASH->TS3=(a>>8U)&0xFFU;
    FLASH->TS2P=b&0xFFU;FLASH->TPS3=(b>>16U)&0x7FFU;
    FLASH->PERTPE=*(const uint32_t *)0x1FFF0F38U&0x1FFFFU;
    FLASH->SMERTPE=*(const uint32_t *)0x1FFF0F3CU&0x1FFFFU;
    a=*(const uint32_t *)0x1FFF0F40U;FLASH->PRGTPE=a&0xFFFFU;FLASH->PRETPE=(a>>16U)&0xFFFU;
}
const uint8_t *ota_read(uint32_t address) { return (const uint8_t *)address; }
void ota_watchdog(void) { IWDG->KR=0xAAAAU; }
static int allowed(uint32_t address)
{
    /* Boot区域永不允许IAP写；上限不越过两个描述页。 */
    return !(address&(OTA_PAGE-1U)) && address>=OTA_APP_BASE && address<=OTA_PENDING;
}
int ota_erase_page(uint32_t address)
{
    if(!allowed(address)) { return 0; }timing();ota_watchdog();
    if(!flash_page(address,0)) { return 0; }
    const uint32_t *p=(const uint32_t *)address;
    for(uint32_t i=0U;i<32U;i++) { if(p[i]!=0xFFFFFFFFU) { return 0; } }
    return 1;
}
int ota_write_page(uint32_t address,const uint32_t *data)
{
    uint32_t source=(uint32_t)data;
    if(!allowed(address) || (source&3U) || source<0x20000000U || source>0x20002000U-OTA_PAGE) { return 0; }
    timing();ota_watchdog();
    return flash_page(address,data) && !memcmp((const void *)address,data,OTA_PAGE);
}
#ifdef FEEDER_PLAN_DIAG
#include "product_flash_layout.h"
/* 配置区单独入口，共用已验证RAM算法，不扩大OTA入口允许范围。 */
int product_flash_write(uint32_t address,const uint32_t *data)
{
    uint32_t source=(uint32_t)data;
    if((address&(OTA_PAGE-1U)) || address<PRODUCT_FLASH_BASE || address>PRODUCT_FLASH_END-OTA_PAGE ||
       (source&3U) || source<0x20000000U || source>0x20002000U-OTA_PAGE) { return 0; }
    timing();ota_watchdog();
    return flash_page(address,data) && !memcmp((const void *)address,data,OTA_PAGE);
}
#endif
