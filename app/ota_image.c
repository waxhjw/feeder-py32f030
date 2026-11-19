#include "ota_layout.h"
#include <string.h>
uint32_t ota_crc32(const void *data,uint32_t length)
{
    const uint8_t *p=data;uint32_t crc=0xFFFFFFFFU;
    while(length--) {
        if(!(length&127U)) { ota_watchdog(); }
        crc^=*p++;
        for(uint32_t i=0U;i<8U;i++) { crc=(crc>>1U)^((0U-(crc&1U))&0xEDB88320U); }
    }
    return ~crc;
}
int ota_header_valid(const OtaHeader *h)
{
    for(uint32_t i=0U;i<sizeof(h->reserved);i++) { if(h->reserved[i]!=0xFFU) { return 0; } }
    return h->magic==OTA_MAGIC && h->format==1U && h->board==OTA_BOARD &&
        h->load==OTA_APP_BASE && h->size>=192U && h->size<=OTA_MAX_IMAGE &&
        h->version>=0x000B0000U && h->version<=0x00FFFFFFU &&
        h->header_crc==ota_crc32(h,28U);
}
int ota_image_valid(const OtaHeader *h,const uint8_t *image)
{
    if(!ota_header_valid(h)) { return 0; }
    uint32_t stack,reset;memcpy(&stack,image,4U);memcpy(&reset,image+4U,4U);
    return stack>0x20000000U && stack<=0x20002000U && !(stack&7U) &&
        (reset&1U) && (reset&~1U)>=OTA_APP_BASE+192U &&
        (reset&~1U)<OTA_APP_BASE+h->size && ota_crc32(image,h->size)==h->crc;
}
int ota_boot_install(void)
{
    OtaHeader active,pending;
    memcpy(&active,ota_read(OTA_ACTIVE),sizeof(active));
    memcpy(&pending,ota_read(OTA_PENDING),sizeof(pending));
    /* 已安装的新镜像优先。开始下一次下载时，旧pending可能仍留着，不能再次覆盖。 */
    if(ota_image_valid(&active,ota_read(OTA_APP_BASE)) &&
       (!ota_header_valid(&pending) || !memcmp(&active,&pending,sizeof(active)))) { return 1; }
    if(ota_header_valid(&pending) && ota_image_valid(&pending,ota_read(OTA_STAGE_BASE+OTA_HEADER))) {
        uint32_t page[32];
        /* pending直到新应用及active均校验通过前保持不动。复制中断后从头重做。 */
        for(uint32_t pos=0U;pos<pending.size;pos+=OTA_PAGE) {
            memset(page,0xFF,sizeof(page));
            uint32_t n=pending.size-pos;if(n>OTA_PAGE) { n=OTA_PAGE; }
            memcpy(page,ota_read(OTA_STAGE_BASE+OTA_HEADER+pos),n);
            ota_watchdog();
            if(!ota_write_page(OTA_APP_BASE+pos,page)) { return 0; }
        }
        if(!ota_image_valid(&pending,ota_read(OTA_APP_BASE))) { return 0; }
        if(!ota_write_page(OTA_ACTIVE,(const uint32_t *)&pending)) { return 0; }
        memcpy(&active,ota_read(OTA_ACTIVE),sizeof(active));
    }
    return ota_image_valid(&active,ota_read(OTA_APP_BASE));
}
