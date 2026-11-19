#ifndef FEEDER_WBR_HEARTBEAT_CONFIG_H
#define FEEDER_WBR_HEARTBEAT_CONFIG_H
#include "wbr_rx_config.h"
/* 主图第3页：U1 PF1(2) -> R6 100R -> U15 WBR3 RXD(15)。
 * 数据手册V2.8第56页表3-8：PF1 AF4=USART2_TX，空闲高，低起始位。
 * 参考手册V1.7第409~411页：先启用TE，逐字节等TXE，最后等TC。
 * 不启用模块复位脚、配网命令、产品参数或任何电机动作。
 */
#define FEEDER_WBR_HB_VERSION "0.4.0-wbr-heartbeat"
#define FEEDER_WBR_HB_ID 0x00040000UL
#define FEEDER_WBR_HB_MAGIC 0x57424842UL
#define FEEDER_WBR_TX_AF 4U
#define FEEDER_WBR_TX_TIMEOUT_MS 50U
#endif
