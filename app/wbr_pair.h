#ifndef FEEDER_WBR_PAIR_H
#define FEEDER_WBR_PAIR_H
#include "wbr_init.h"
#define WBR_PAIR_MAGIC 0x57425041UL
#define WBR_PAIR_REQUEST 0x50414952UL
enum { WBR_PAIR_IDLE, WBR_PAIR_QUEUED, WBR_PAIR_SENT, WBR_PAIR_ACKED,
       WBR_PAIR_READY, WBR_PAIR_TIMEOUT, WBR_PAIR_NOT_READY, WBR_PAIR_INVALID,
       WBR_PAIR_ALREADY_REQUESTED, WBR_PAIR_ALREADY_PAIRING };
typedef struct {
    uint32_t magic, abi, command, nonce, nonce_inverse, accepted_nonce;
    uint32_t result, attempts, tx_count, ack_count, request_ms, network_state;
} WbrPairCommand;
/* 只提供固定配网命令，不接受任意串口字节或电机指令。 */
extern volatile WbrPairCommand g_wbr_pair_command;
void wbr_pair_poll(WbrInit *link, uint32_t now);
#endif
