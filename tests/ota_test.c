#include "ota_update.h"
#include "wbr_init.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint8_t flash[65536],factory[65536],package[24576];
static uint32_t length,writes,fail_write,stopped;
static WbrInit link;
const uint8_t *ota_read(uint32_t address) { assert(address>=OTA_FLASH_BASE && address<OTA_FLASH_BASE+sizeof(flash));return flash+address-OTA_FLASH_BASE; }
void ota_watchdog(void) { }
void ota_quiesce(void) { stopped=1U;link.ota_block_dp=1U; }
int ota_erase_page(uint32_t address)
{
    assert(stopped || address<OTA_STAGE_BASE);
    assert(address>=OTA_APP_BASE && address<=OTA_PENDING && !(address&127U));
    memset(flash+address-OTA_FLASH_BASE,255,128U);return 1;
}
int ota_write_page(uint32_t address,const uint32_t *data)
{
    writes++;ota_erase_page(address);
    if(writes==fail_write) { memcpy(flash+address-OTA_FLASH_BASE,data,16U);return 0; }
    memcpy(flash+address-OTA_FLASH_BASE,data,128U);return 1;
}
static void be(uint8_t *p,uint32_t n) { p[0]=n>>24U;p[1]=n>>16U;p[2]=n>>8U;p[3]=n; }
static int packet(uint32_t offset,const uint8_t *data,uint32_t n)
{
    uint8_t raw[260];be(raw,offset);if(n) { memcpy(raw+4U,data,n); }
    return ota_receive(0x0BU,raw,n+4U,100U+offset);
}
static void reset(void)
{
    memcpy(flash,factory,sizeof(flash));writes=0U;fail_write=0U;stopped=0U;
    g_ota_debug=(OtaDebug){0};wbr_init_reset(&link);link.state=1U;
}
static void start(void)
{
    uint8_t raw[4];be(raw,length);assert(ota_receive(0x0AU,raw,4U,10U));assert(stopped);
}
static void download(void)
{
    start();
    for(uint32_t pos=0U;pos<length;pos+=256U) {
        uint32_t n=length-pos;if(n>256U) { n=256U; }
        assert(packet(pos,package+pos,n));
    }
}
static void feed(uint8_t cmd,const uint8_t *data,uint32_t n)
{
    uint8_t raw[327]={0x55,0xAA,0U,cmd,(uint8_t)(n>>8U),(uint8_t)n};
    memcpy(raw+6U,data,n);uint8_t sum=0U;
    for(uint32_t i=0U;i<n+6U;i++) { sum+=raw[i]; }
    raw[n+6U]=sum;
    for(uint32_t i=0U;i<n+7U;i++) { wbr_init_feed(&link,raw[i],0U,20U+i); }
}
static void pass(const char *s) { printf("PASS %s\n",s); }
int main(int argc,char **argv)
{
    assert(argc==3);FILE *f=fopen(argv[1],"rb");assert(f);assert(fread(factory,1,sizeof(factory),f)==sizeof(factory));fclose(f);
    f=fopen(argv[2],"rb");assert(f);length=(uint32_t)fread(package,1,sizeof(package),f);fclose(f);assert(length>320U);
    reset();assert(ota_crc32("123456789",9U)==0xCBF43926U);assert(ota_boot_install());pass("factory CRC and boot");
    OtaHeader h;memcpy(&h,package,128U);assert(ota_header_valid(&h));assert(ota_image_valid(&h,package+128U));
    h.board^=1U;assert(!ota_header_valid(&h));h.header_crc=ota_crc32(&h,28U);assert(!ota_header_valid(&h));pass("wrong board refused even with valid CRC");
    uint8_t raw[260];be(raw,length);feed(0x0AU,raw,4U);const WbrInitPacket *out=wbr_init_peek(&link);
    assert(out && out->length==8U && out->data[3]==0x0AU && out->data[6]==0U);wbr_init_sent(&link);
    be(raw,0U);memcpy(raw+4U,package,256U);feed(0x0BU,raw,260U);
    assert(g_ota_debug.received==256U);out=wbr_init_peek(&link);assert(out && out->length==7U);wbr_init_sent(&link);
    assert(!link.locked);pass("real 260-byte Tuya parser and ACK drain");
    assert(packet(0U,package,256U));assert(g_ota_debug.duplicates==1U && g_ota_debug.received==256U);
    be(raw,length);assert(ota_receive(0x0AU,raw,4U,100U));assert(g_ota_debug.received==256U);pass("lost ACK retransmission idempotent");
    assert(!packet(512U,package+512U,256U));assert(g_ota_debug.state==OTA_ERROR);pass("out of order refused");
    reset();start();assert(packet(0U,package,256U));uint8_t altered[256];memcpy(altered,package,256U);altered[200]^=1U;
    assert(!packet(0U,altered,256U));pass("conflicting duplicate refused");
    reset();start();assert(!packet(0xFFFFFFF0U,package,256U));assert(writes==0U);pass("integer overflow and out of bounds");
    reset();start();assert(!packet(length,0,0U));assert(!ota_header_valid((const OtaHeader *)ota_read(OTA_PENDING)));pass("premature EOF cannot commit");
    reset();download();flash[OTA_STAGE_BASE-OTA_FLASH_BASE+201U]^=1U;assert(!packet(length,0,0U));assert(ota_boot_install());pass("bad payload CRC retains old app");
    reset();download();assert(!memcmp(flash+OTA_APP_BASE-OTA_FLASH_BASE,factory+OTA_APP_BASE-OTA_FLASH_BASE,OTA_MAX_IMAGE));pass("download never modifies active program");
    assert(packet(length,0,0U));assert(g_ota_debug.state==OTA_COMMITTED);assert(ota_boot_install());
    assert(!memcmp(ota_read(OTA_APP_BASE),package+128U,length-128U));assert(ota_boot_install());pass("verified commit install and repeat reboot");
    uint32_t pages=(length-128U+127U)/128U;
    for(uint32_t cut=1U;cut<=pages+1U;cut++) {
        reset();download();assert(packet(length,0,0U));writes=0U;fail_write=cut;
        assert(!ota_boot_install());fail_write=0U;assert(ota_boot_install());
        assert(!memcmp(ota_read(OTA_APP_BASE),package+128U,length-128U));
    }
    printf("PASS power-loss copy/active metadata recovery at %lu write boundaries\n",(unsigned long)(pages+1U));
    reset();download();fail_write=writes+1U;assert(!packet(length,0,0U));fail_write=0U;assert(ota_boot_install());
    assert(!memcmp(ota_read(OTA_APP_BASE),factory+OTA_APP_BASE-OTA_FLASH_BASE,192U));pass("torn pending commit preserves old app");
    reset();start();ota_poll(70000U);assert(g_ota_debug.state==OTA_ERROR);assert(ota_boot_install());pass("download timeout preserves old app");
    reset();memset(flash+OTA_ACTIVE-OTA_FLASH_BASE,0,128U);assert(!ota_boot_install());pass("invalid active does not execute unchecked app");
    reset();download();assert(packet(length,0,0U));assert(ota_boot_install());
    flash[OTA_STAGE_BASE-OTA_FLASH_BASE+200U]^=1U;assert(ota_boot_install());pass("installed app independent from stale stage");
    puts("OTA MODEL PASS: offline only");return 0;
}
