#ifndef WBR_UART_HOST_MODEL_H
#define WBR_UART_HOST_MODEL_H
#include <stdint.h>
/* 仅用于电脑端寄存器模型；不能证明真实电平、时钟或接线。 */
typedef struct { volatile uint32_t MODER, OTYPER, OSPEEDR, PUPDR, IDR, ODR, BSRR, LCKR, AFR[2]; } GPIO_TypeDef;
typedef struct { volatile uint32_t IOPENR, APBENR1; } RCC_TypeDef;
typedef struct { volatile uint32_t SR, DR, BRR, CR1, CR2, CR3; } USART_TypeDef;
extern GPIO_TypeDef test_gpiof;
extern RCC_TypeDef test_rcc;
extern USART_TypeDef test_usart2;
extern int test_irq_enabled;
#define GPIOF (&test_gpiof)
#define RCC (&test_rcc)
#define USART2 (&test_usart2)
#define RCC_IOPENR_GPIOFEN 0x20U
#define RCC_APBENR1_USART2EN 0x20000U
#define USART_SR_PE 1U
#define USART_SR_FE 2U
#define USART_SR_NE 4U
#define USART_SR_ORE 8U
#define USART_SR_RXNE 0x20U
#define USART_SR_TC 0x40U
#define USART_SR_TXE 0x80U
#define USART_CR1_UE 0x2000U
#define USART_CR1_RE 4U
#define USART_CR1_TE 8U
#define USART_CR1_RXNEIE 0x20U
#define USART_CR3_EIE 1U
#define USART2_IRQn 28
static inline void __DMB(void) { }
static inline void NVIC_DisableIRQ(int irq) { (void)irq; test_irq_enabled = 0; }
static inline void NVIC_EnableIRQ(int irq) { (void)irq; test_irq_enabled = 1; }
static inline void NVIC_ClearPendingIRQ(int irq) { (void)irq; }
static inline void NVIC_SetPriority(int irq, unsigned priority) { (void)irq; (void)priority; }
#endif
