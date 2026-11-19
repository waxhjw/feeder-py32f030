#ifndef FEEDER_WBR_UART_RX_H
#define FEEDER_WBR_UART_RX_H
#include <stdint.h>
/* 本接口只有接收与观察速率切换，不提供任何发送函数。 */
int wbr_uart_rx_init(uint32_t peripheral_clock, uint32_t baud);
int wbr_uart_rx_set_baud(uint32_t peripheral_clock, uint32_t baud);
int wbr_uart_rx_pop(uint16_t *event);
uint32_t wbr_uart_rx_dropped(void);
void USART2_IRQHandler(void);
#endif
