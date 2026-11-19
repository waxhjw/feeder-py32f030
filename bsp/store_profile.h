#ifndef FEEDER_STORE_PROFILE_H
#define FEEDER_STORE_PROFILE_H
#include <stdint.h>
/* 与原采样间隔相同的SysTick时间域；Flash屏蔽中断期间可能少计，不当示波器时长。 */
typedef struct {
    uint32_t magic,abi,sequence,polls,max_poll_us,last_poll_us;
    uint32_t reads,read_us,writes,write_us,crcs,crc_us,loads,load_us;
} StoreProfile;
extern volatile StoreProfile g_store_profile;
uint32_t store_profile_us(void);
#endif
