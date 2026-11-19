#ifndef FEEDER_WBR_RX_CONFIG_H
#define FEEDER_WBR_RX_CONFIG_H
#include "init_only_config.h"

/* 主图第3页：U15 TXD(16) -> R5 -> U1 PF0(1)。
 * 普冉数据手册V2.8第56页表3-8：PF0 AF4 = USART2_RX。
 * 本阶段只接收：PF1不切成发送复用，USART的TE始终关闭。
 * 复用0.2.1的PA8/PA11早期关闭，不加入点动、配网或产品PID。
 */
#define FEEDER_WBR_VERSION "0.3.0-wbr-rx"
#define FEEDER_WBR_ID 0x00030000UL
#define FEEDER_WBR_RX_AF 4U
#define FEEDER_WBR_SCAN_MS 8000UL
#define FEEDER_WBR_DEBUG_MAGIC 0x57425258UL
#endif
