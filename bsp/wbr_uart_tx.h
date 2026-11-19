#ifndef FEEDER_WBR_UART_TX_H
#define FEEDER_WBR_UART_TX_H
#include <stdint.h>
#ifdef FEEDER_PLAN_APP
#define WBR_TX_FRAME_LIMIT 72U
#else
#define WBR_TX_FRAME_LIMIT 64U
#endif
/* 仅供初始化协议层：复制一整帧后逐字节发送，最后等待TC。
 * start忙时拒绝；poll返回1才表示完成，-1表示超时后锁定。
 */
void wbr_uart_tx_reset(void);
void wbr_uart_tx_disable(void);
int wbr_uart_tx_start(const uint8_t *data, uint32_t length, uint32_t now);
int wbr_uart_tx_poll(uint32_t now);
#endif
