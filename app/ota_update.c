#include "ota_update.h"
#include "wbr_init_config.h"
#include <string.h>
volatile OtaDebug g_ota_debug={.magic=0x4F544144U,.abi=1U,.version=FEEDER_WBR_INIT_ID};
static uint32_t be32(const uint8_t *p) { return ((uint32_t)p[0]<<24U)|((uint32_t)p[1]<<16U)|((uint32_t)p[2]<<8U)|p[3]; }
static int failed(uint32_t error) { g_ota_debug.error=error;g_ota_debug.state=OTA_ERROR;return 0; }
int ota_busy(void) { return g_ota_debug.state!=OTA_IDLE; }
int ota_receive(uint8_t command,const uint8_t *data,uint32_t length,uint32_t now)
{
    if(command==0x0AU) {
        if(length!=4U && length!=8U) { return 0; }
        /* 目前仅MCU通道9，不接受扩展多MCU类型而误刷本主控。 */
        if(length==8U) { return 0; }
        uint32_t total=be32(data);
        if(total<OTA_HEADER+192U || total>OTA_STAGE_SIZE) { return 0; }
        if(g_ota_debug.state==OTA_RECEIVING && g_ota_debug.total==total) {
            g_ota_debug.last_ms=now;return 1; /* 启动ACK丢失重发不丢弃已收数据。 */
        }
        if(g_ota_debug.state==OTA_COMMITTED) { return 0; }
        g_ota_debug.state=OTA_RECEIVING;g_ota_debug.total=total;g_ota_debug.received=0U;
        g_ota_debug.packets=0U;g_ota_debug.duplicates=0U;g_ota_debug.error=0U;g_ota_debug.last_ms=now;
        ota_quiesce(); /* 必须在任何擦写之前实际关闭输出。 */
        /* 先撤销旧pending，再动暂存区。active一直保留至Boot完成复制。 */
        if(!ota_erase_page(OTA_PENDING)) { return failed(1U); }
        return 1;
    }
    if(command!=0x0BU || length<4U || length>260U || g_ota_debug.state!=OTA_RECEIVING) { return 0; }
    uint32_t offset=be32(data),n=length-4U;data+=4U;
    if(!n) {
        if(offset<g_ota_debug.total || g_ota_debug.received!=g_ota_debug.total) { return failed(2U); }
        OtaHeader header;memcpy(&header,ota_read(OTA_STAGE_BASE),sizeof(header));
        if(!ota_header_valid(&header) || header.size+OTA_HEADER!=g_ota_debug.total ||
           header.version<=FEEDER_WBR_INIT_ID || !ota_image_valid(&header,ota_read(OTA_STAGE_BASE+OTA_HEADER))) { return failed(3U); }
        if(!ota_write_page(OTA_PENDING,(const uint32_t *)&header)) { return failed(4U); }
        g_ota_debug.state=OTA_COMMITTED;g_ota_debug.last_ms=now;return 1;
    }
    if(offset>g_ota_debug.total || n>g_ota_debug.total-offset) { return failed(5U); }
    if(offset<g_ota_debug.received) {
        if(n>g_ota_debug.received-offset || memcmp(ota_read(OTA_STAGE_BASE+offset),data,n)) { return failed(6U); }
        g_ota_debug.duplicates++;g_ota_debug.last_ms=now;return 1;
    }
    if(offset!=g_ota_debug.received || (offset&255U) || (n!=256U && offset+n!=g_ota_debug.total)) { return failed(7U); }
    if(!offset) {
        OtaHeader header;
        if(n<OTA_HEADER) { return failed(8U); }memcpy(&header,data,sizeof(header));
        if(!ota_header_valid(&header) || header.size+OTA_HEADER!=g_ota_debug.total || header.version<=FEEDER_WBR_INIT_ID) { return failed(9U); }
    }
    for(uint32_t pos=0U;pos<n;pos+=OTA_PAGE) {
        uint32_t page[32];memset(page,0xFF,sizeof(page));
        uint32_t count=n-pos;if(count>OTA_PAGE) { count=OTA_PAGE; }
        memcpy(page,data+pos,count);
        if(!ota_write_page(OTA_STAGE_BASE+offset+pos,page)) { return failed(10U); }
    }
    g_ota_debug.received+=n;g_ota_debug.packets++;g_ota_debug.last_ms=now;return 1;
}
void ota_poll(uint32_t now)
{
    /* 超时锁定停止；重新上电仍可运行旧完整应用，不自动续跑旧电机请求。 */
    if(g_ota_debug.state==OTA_RECEIVING && (uint32_t)(now-g_ota_debug.last_ms)>60000U) { (void)failed(11U); }
}
