#include "py32f0xx.h"
#include "wbr_heartbeat_config.h"
#include "wbr_uart_hb.h"

static uint8_t frame[8];
static uint32_t active, failed, index, started;

void wbr_uart_hb_disable(void)
{
    /* 故障退出时撤销TX输出；正常发送只能在TC=1后结束。 */
    GPIOF->MODER = (GPIOF->MODER & ~12UL) | 12UL;
    USART2->CR1 &= ~USART_CR1_TE;
    active = 0U;
}
void wbr_uart_hb_reset(void)
{
    active = failed = index = started = 0U;
    wbr_uart_hb_disable();
}

int wbr_uart_hb_start(uint8_t value, uint32_t now)
{
    if (value > 1U || active != 0U || failed != 0U || (USART2->CR1 & ~USART_CR1_TE) != 0x2024U) {
        return 0;
    }
    const uint8_t reply[] = {0x55, 0xAA, 3, 0, 0, 1, value, (uint8_t)(3U + value)};
    for (uint32_t i = 0U; i < sizeof(frame); ++i) { frame[i] = reply[i]; }
    GPIOF->BSRR = 2U; /* 先准备高锁存；PF1随后由USART输出空闲高电平。 */
    GPIOF->OTYPER &= ~2UL;
    GPIOF->OSPEEDR &= ~12UL;
    GPIOF->PUPDR &= ~12UL;
    GPIOF->AFR[0] = (GPIOF->AFR[0] & ~0xF0UL) | (FEEDER_WBR_TX_AF << 4U);
    USART2->CR1 |= USART_CR1_TE;
    GPIOF->MODER = (GPIOF->MODER & ~12UL) | 8UL;
    index = 0U;
    started = now;
    active = 1U;
    return 1;
}

int wbr_uart_hb_poll(uint32_t now)
{
    if (failed != 0U) { return -1; }
    if (active == 0U) { return 0; }
    if ((uint32_t)(now - started) >= FEEDER_WBR_TX_TIMEOUT_MS) {
        failed = 1U;
        wbr_uart_hb_disable();
        return -1;
    }
    uint32_t status = USART2->SR;
    if (index < sizeof(frame)) {
        if ((status & USART_SR_TXE) != 0U) { USART2->DR = frame[index++]; }
        return 0;
    }
    if ((status & USART_SR_TC) != 0U) { active = 0U; return 1; }
    return 0;
}
