#ifndef SLIP_UART_CONF_H_INCLUDED
#define SLIP_UART_CONF_H_INCLUDED
#include <stm32f4xx.h>

extern uint32_t SystemCoreClock;

/*Hardware defines*/
#define SU_BAUDRATE             115200
#define SU_RCC_HZ               SystemCoreClock

#define SU_USART                USART3
#define SU_USART_IRQn           USART3_IRQn
#define SU_USART_IRQ_HANDLER    USART3_IRQHandler

#define SU_RCC                  RCC->APB1ENR
#define SU_RCC_BIT              RCC_APB1ENR_USART3EN

#define SU_TX_PIN               8
#define SU_TX_ALT_NUM           7
#define SU_RX_PIN               9
#define SU_RX_ALT_NUM           7
#define SU_TX_RX_PORT           GPIOD
#define SU_GPIO_RCC             RCC->AHB1ENR
#define SU_GPIO_RCC_BIT         RCC_AHB1ENR_GPIODEN

//#define SU_DMA                  DMA1
//#define SU_DMA_RCC              RCC->AHB1ENR
//#define SU_DMA_RCC_BIT          RCC_AHB1ENR_DMA1EN

//#define SU_DMA_TX               DMA1_Stream3
//#define SU_DMA_TX_CH            DMA_CHANNEL_4
//#define SU_DMA_TX_IRQn          DMA1_Stream3_IRQn
//#define SU_DMA_TX_IRQ_HANDLER   DMA1_Stream3_IRQHandler

#endif /* SLIP_UART_CONF_H_INCLUDED */
