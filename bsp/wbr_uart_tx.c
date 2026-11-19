#include "py32f0xx.h"
#include "wbr_init_config.h"
#include "wbr_uart_tx.h"
static uint8_t frame[WBR_TX_FRAME_LIMIT];
static uint32_t active, failed, index, length, started;
void wbr_uart_tx_disable(void)
{
    GPIOF->MODER = (GPIOF->MODER & ~12UL) | 12UL;
    USART2->CR1 &= ~USART_CR1_TE;
    active = 0U;
}
void wbr_uart_tx_reset(void)
{
    active = failed = index = length = started = 0U;
    wbr_uart_tx_disable();
}
int wbr_uart_tx_start(const uint8_t *data, uint32_t count, uint32_t now)
{
    if (data == 0 || count == 0U || count > sizeof(frame) || active || failed ||
        (USART2->CR1 & ~USART_CR1_TE) != 0x2024U) { return 0; }
    for (uint32_t i = 0U; i < count; ++i) { frame[i] = data[i]; }
    GPIOF->BSRR = 2U;
    GPIOF->OTYPER &= ~2UL;
    GPIOF->OSPEEDR &= ~12UL;
    GPIOF->PUPDR &= ~12UL;
    GPIOF->AFR[0] = (GPIOF->AFR[0] & ~0xF0UL) | 0x40UL;
    USART2->CR1 |= USART_CR1_TE;
    GPIOF->MODER = (GPIOF->MODER & ~12UL) | 8UL;
    length = count; index = 0U; started = now; active = 1U;
    return 1;
}
int wbr_uart_tx_poll(uint32_t now)
{
    if (failed) { return -1; }
    if (!active) { return 0; }
    if ((uint32_t)(now - started) >= FEEDER_INIT_TX_TIMEOUT_MS) {
        failed = 1U; wbr_uart_tx_disable(); return -1;
    }
    uint32_t status = USART2->SR;
    if (index < length) {
        if (status & USART_SR_TXE) { USART2->DR = frame[index++]; }
        return 0;
    }
    if (status & USART_SR_TC) { active = 0U; return 1; }
    return 0;
}
