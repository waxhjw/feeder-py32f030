#ifndef FEEDER_WBR_UART_HB_H
#define FEEDER_WBR_UART_HB_H
#include <stdint.h>
/* 本驱动复用已经验证的RX队列，发送端仅提供固定8字节心跳回复。
 * poll每次最多写1字节，不忙等；超时50ms后停止发送且不自动重试。
 */
void wbr_uart_hb_reset(void);
int wbr_uart_hb_start(uint8_t value, uint32_t now);
int wbr_uart_hb_poll(uint32_t now); /* 0=等待/空闲，1=完整发送结束，-1=超时锁定。 */
void wbr_uart_hb_disable(void);
#endif
