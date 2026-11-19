#include "py32f0xx.h"
#include "wbr_rx_config.h"
#include "wbr_uart_rx.h"

#ifdef FEEDER_OTA_APP
#define RX_QUEUE_SIZE 512U
#else
#define RX_QUEUE_SIZE 256U
#endif
#define RX_ERROR_MASK (USART_SR_PE | USART_SR_FE | USART_SR_NE | USART_SR_ORE)
static volatile uint16_t queue[RX_QUEUE_SIZE];
static volatile uint32_t head, tail, dropped;
static volatile uint32_t pending_gap;

int wbr_uart_rx_set_baud(uint32_t peripheral_clock, uint32_t baud)
{
    if (baud != 9600U && baud != 115200U) { return 0; }
    uint32_t divider = (peripheral_clock + baud / 2U) / baud;
    if (divider < 16U || divider > 65535U) { return 0; }
    NVIC_DisableIRQ(USART2_IRQn);
    USART2->CR1 = 0U;
    /* 参考手册V1.7第413~414页：依次读SR、DR清错误/RXNE。
     * 观察端换速率可能截断当前字节；解析器在切换时同时清半包。
     */
    (void)USART2->SR;
    (void)USART2->DR;
    head = tail = 0U;
    pending_gap = 0U;
    USART2->BRR = divider;
    USART2->CR2 = 0U; /* 1停止位、异步。 */
    USART2->CR3 = USART_CR3_EIE; /* 无DMA、无硬件流控。 */
    USART2->CR1 = USART_CR1_UE | USART_CR1_RE | USART_CR1_RXNEIE;
    /* TE未设置：8数据位、无校验、仅接收。PF1保持复位模拟高阻状态。 */
    NVIC_ClearPendingIRQ(USART2_IRQn);
    NVIC_EnableIRQ(USART2_IRQn);
    return 1;
}

int wbr_uart_rx_init(uint32_t peripheral_clock, uint32_t baud)
{
    RCC->IOPENR |= RCC_IOPENR_GPIOFEN;
    RCC->APBENR1 |= RCC_APBENR1_USART2EN;
    (void)RCC->APBENR1;
    GPIOF->AFR[0] = (GPIOF->AFR[0] & ~0xFUL) | FEEDER_WBR_RX_AF;
    GPIOF->PUPDR &= ~3UL; /* 接收输入不启用上下拉，避免向未供电模块灌电。 */
    GPIOF->MODER = (GPIOF->MODER & ~3UL) | 2UL;
    dropped = 0U;
    NVIC_SetPriority(USART2_IRQn, 1U);
    return wbr_uart_rx_set_baud(peripheral_clock, baud);
}

void USART2_IRQHandler(void)
{
    uint32_t status = USART2->SR;
    if ((status & (USART_SR_RXNE | RX_ERROR_MASK)) == 0U) { return; }
    uint16_t value = (uint16_t)(USART2->DR & 0xFFU);
    value |= (uint16_t)((status & RX_ERROR_MASK) << 8U);
    uint32_t next = (head + 1U) & (RX_QUEUE_SIZE - 1U);
    if (next == tail) { dropped++; pending_gap = 1U; return; }
    if (pending_gap != 0U) { value |= 0x1000U; pending_gap = 0U; }
    queue[head] = value;
    __DMB();
    head = next;
}

int wbr_uart_rx_pop(uint16_t *event)
{
    if (tail == head) { return 0; }
    *event = queue[tail];
    __DMB();
    tail = (tail + 1U) & (RX_QUEUE_SIZE - 1U);
    return 1;
}

uint32_t wbr_uart_rx_dropped(void) { return dropped; }
