#ifndef FEEDER_OTA_UPDATE_H
#define FEEDER_OTA_UPDATE_H
#include "ota_layout.h"
enum { OTA_IDLE=0,OTA_RECEIVING=1,OTA_COMMITTED=2,OTA_ERROR=3 };
typedef struct {
    uint32_t magic,abi,version,state,total,received,packets,duplicates,error,last_ms,reboots;
} OtaDebug;
extern volatile OtaDebug g_ota_debug;
/* 返回1才回复ACK；拒绝的包绝不写活动应用，不发送伪成功。 */
int ota_receive(uint8_t command,const uint8_t *data,uint32_t length,uint32_t now);
int ota_busy(void);
void ota_poll(uint32_t now);
void ota_quiesce(void);
#endif
