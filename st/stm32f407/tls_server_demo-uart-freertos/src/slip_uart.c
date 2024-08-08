#include "debug.h"
#include "os_port.h"
#include "slip_uart_conf.h"
#include "slip_uart.h"
#include "slip_uart_sock.h"
#include "queue.h"
#include "memblock.h"

#define SLIP_RX_MIN_FRAME   9

/* SLIP protocol characters. */
#define SLIP_END            0xC0        /* indicates end of frame   */
#define SLIP_ESC            0xDB        /* indicates byte stuffing  */
#define SLIP_ESC_END        0xDC        /* ESC ESC_END means END 'data' */
#define SLIP_ESC_ESC        0xDD        /* ESC ESC_ESC means ESC 'data' */

/* SLIP RX/TX states */
#define SLIP_TX_NOT_READY   0x00
#define SLIP_TX_END         0x01
#define SLIP_TX_READY       0x02
#define SLIP_TX_ESCAPE      0x80

#define SLIP_RX_READY       0x00
#define SLIP_RX_IN_PACKET   0x01
#define SLIP_RX_ESCAPE      0x02

//* Global variables for transmiter
static OsEvent    slipTxEvent;
static uint8_t    SlipTxPSW = SLIP_TX_READY;
static uint8_t    SlipTxTSB;
static uint16_t   SlipTxLength;
static uint8_t    *SlipTxPos;
static TMemBlock  *SlipTxBlock;
TQueue            SlipTxQueue;

//* Global variables for receiver
static OsEvent    slipRxEvent;
static uint8_t    SlipRxEnable = FALSE;
static uint8_t    SlipRxPSW = SLIP_RX_READY;
static uint16_t   SlipRxCount = 0;
static uint8_t    *SlipRxPos;
static TMemBlock  *SlipRxBlock;
TQueue            SlipRxQueue;

void SU_USART_IRQ_HANDLER(void)
{
    uint8_t c;
    bool_t flag = FALSE;
    uint32_t isr = SU_USART->SR;

    //Enter interrupt service routine
    osEnterIsr();
    
    if (isr & USART_SR_RXNE)
    {
        c = SU_USART->DR;
        if (c == SLIP_END)
        {
            if (SlipRxEnable == TRUE)
            {
                if (SlipRxCount >= SLIP_RX_MIN_FRAME)
                {
                    SlipRxBlock->Len = SlipRxCount;
                    if (queue_insert(&SlipRxQueue, SlipRxBlock) == TRUE)
                    {
                        if ((SlipRxBlock = MB_Get()) != NULL) SlipRxEnable = TRUE; else SlipRxEnable = FALSE;
                        flag = osSetEventFromIsr(&slipRxEvent);
                    }
                }
            }
            else
            {
                if ((SlipRxBlock = MB_Get()) != NULL) SlipRxEnable = TRUE; else SlipRxEnable = FALSE;
            }
            SlipRxPSW = SLIP_RX_IN_PACKET;
            SlipRxCount = 0;
            SlipRxPos = SlipRxBlock->Data;
        }
        else
        {
            if (SlipRxPSW & SLIP_RX_IN_PACKET)
            {
                if (!(SlipRxPSW & SLIP_RX_ESCAPE) && (c == SLIP_ESC))
                    SlipRxPSW |= SLIP_RX_ESCAPE;
                else
                {
                    if (SlipRxPSW & SLIP_RX_ESCAPE)
                    {
                        if (c == SLIP_ESC_END)
                            c = SLIP_END;
                        else if (c == SLIP_ESC_ESC)
                            c = SLIP_ESC;
                        SlipRxPSW &= ~SLIP_RX_ESCAPE;
                    }
                    if (SlipRxCount < MEMBLOCK_SIZE)
                    {
                        if (SlipRxEnable == TRUE)
                            *SlipRxPos++ = c;
                        SlipRxCount++;
                    }
                    else
                    {
                        SlipRxPSW = SLIP_RX_READY;
                        SlipRxCount = 0;
                    }
                }
            }
        }
    }

    if ((isr & USART_SR_TXE) && (SU_USART->CR1 & USART_CR1_TXEIE))
    {
        if (SlipTxPSW & SLIP_TX_ESCAPE)
        {
            SlipTxPSW &= ~SLIP_TX_ESCAPE;
            SU_USART->DR = SlipTxTSB;
        }
        else
        {
            if (SlipTxPSW & SLIP_TX_END)
            {
                SU_USART->CR1 &= ~USART_CR1_TXEIE;
                SlipTxPSW = SLIP_TX_READY;
                MB_Free(SlipTxBlock);
                flag |= osSetEventFromIsr(&slipTxEvent);
            }
            else
            {
                if (SlipTxLength > 0)
                {
                    c = *SlipTxPos++;
                    if (c == SLIP_END)
                    {
                        c = SLIP_ESC;
                        SlipTxTSB = SLIP_ESC_END;
                        SlipTxPSW = SLIP_TX_ESCAPE;
                    }
                    else
                    {
                        if (c == SLIP_ESC)
                        {
                            c = SLIP_ESC;
                            SlipTxTSB = SLIP_ESC_ESC;
                            SlipTxPSW = SLIP_TX_ESCAPE;
                        }
                    }
                    SlipTxLength--;
                    SU_USART->DR = c;
                }
                else
                {
                    SU_USART->DR = SLIP_END;
                    SlipTxPSW = SLIP_TX_END;
                }
            }
        }
    }
    
    //Leave interrupt service routine
    osExitIsr(flag);
}

uint16_t calcCrc(const uint8_t *buffer, uint16_t length)
{
	uint16_t crc = 0xFFFF;
	uint8_t i;
	while (length--) {
		crc ^= *buffer++ << 8;
		for (i = 0; i < 8; i++)
			crc = crc & 0x8000 ? (crc << 1) ^ 0x1021 : crc << 1;
	}
	return crc;
}

static void SU_HwInit(void)
{
  /*Configure USART, GPIOx and DMA clocks*/
  SU_RCC |= SU_RCC_BIT;
  SU_GPIO_RCC |= SU_GPIO_RCC_BIT;
//  SU_DMA_RCC |= SU_DMA_RCC_BIT;

  /*Configure USART Rx/Tx GPIO*/
  SU_TX_RX_PORT->MODER &= ~((GPIO_MODER_MODE0 << (SU_TX_PIN << 1)) | (GPIO_MODER_MODE0 << (SU_RX_PIN << 1)));
  SU_TX_RX_PORT->MODER |= (GPIO_MODER_MODER0_1 << (SU_TX_PIN << 1)) | (GPIO_MODER_MODER0_1 << (SU_RX_PIN << 1));
  SU_TX_RX_PORT->OSPEEDR |= (GPIO_OSPEEDR_OSPEED0 << ((SU_TX_PIN << 1))) | (GPIO_OSPEEDR_OSPEED0 << ((SU_RX_PIN << 1)));
  SU_TX_RX_PORT->OTYPER &= ~(GPIO_OTYPER_OT0 << SU_TX_PIN);
  SU_TX_RX_PORT->PUPDR &= ~(GPIO_PUPDR_PUPD0_0 << (SU_RX_PIN << 1));
#if (SU_RX_PIN < 8)
    SU_TX_RX_PORT->AFR[0] &= ~(GPIO_AFRH_AFSEL8 << (SU_RX_PIN << 2));
    SU_TX_RX_PORT->AFR[0] |= (SU_RX_ALT_NUM << (SU_RX_PIN << 2));
#else
    SU_TX_RX_PORT->AFR[1] &= ~(GPIO_AFRL_AFSEL0 << ((SU_RX_PIN - 8) << 2));
    SU_TX_RX_PORT->AFR[1] |= (SU_RX_ALT_NUM << ((SU_RX_PIN - 8) << 2));
#endif
#if (SU_TX_PIN < 8)
    SU_TX_RX_PORT->AFR[0] &= ~(GPIO_AFRH_AFSEL8 << (SU_TX_PIN << 2));
    SU_TX_RX_PORT->AFR[0] |= (SU_RX_ALT_NUM << (SU_TX_PIN << 2));
#else
    SU_TX_RX_PORT->AFR[1] &= ~(GPIO_AFRL_AFSEL0 << ((SU_TX_PIN - 8) << 2));
    SU_TX_RX_PORT->AFR[1] |= (SU_RX_ALT_NUM << ((SU_TX_PIN - 8) << 2));
#endif

//    //DMA Tx channel: Memory increment, Transfer complete interrupt, Transfer error interrupt
//    SU_DMA_TX->CR = 0;
//    SU_DMA_TX->CR = (SU_DMA_TX_CH | DMA_SxCR_PL | DMA_SxCR_MINC | DMA_SxCR_DIR_0 | DMA_SxCR_TCIE | DMA_SxCR_TEIE);
//    SU_DMA_TX->PAR = (uint32_t)&SU_USART->DR;
//    SU_DMA_TX->M0AR = (uint32_t)SU_TxFrame;

     /*Set highest priority to Tx DMA*/
//    NVIC_SetPriority(SU_DMA_TX_IRQn, 0);
//    NVIC_EnableIRQ(SU_DMA_TX_IRQn);

    /*Configure USART*/
    /*CR1: Transmitter/Receiver enable; Rx/Tx interrupt enable*/
    SU_USART->CR1 = 0;
    SU_USART->BRR = SU_RCC_HZ / 4 / SU_BAUDRATE;
    SU_USART->CR1 = (USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE /*| USART_CR1_TXEIE*/);
    SU_USART->CR2 = 0;
    SU_USART->CR3 = 0;

    /*Enable interrupt vector for USART1*/
    NVIC_SetPriority(SU_USART_IRQn, 12);
    NVIC_EnableIRQ(SU_USART_IRQn);

    /*Enable USART*/
    SU_USART->CR1 |= USART_CR1_UE;
}

bool_t SU_Init(void)
{
    bool_t ret = TRUE;
    SU_HwInit();
  
    SlipTxPSW = SLIP_TX_READY;
    SlipRxPSW = SLIP_RX_READY;
    if ((SlipRxBlock = MB_Get()) != NULL)
        SlipRxEnable = TRUE;
    else 
        SlipRxEnable = FALSE;

//      SlipRxPackets = 0;
//      SlipTxPackets = 0;

    queue_init(&SlipRxQueue);
    queue_init(&SlipTxQueue);
  
    if (!osCreateEvent(&slipRxEvent))
    {
        ret = FALSE;
        TRACE_ERROR("Failed to initialize slipRxEvent!\r\n");
    }

    if (!osCreateEvent(&slipTxEvent))
    {
        ret = FALSE;
        TRACE_ERROR("Failed to initialize slipTxEvent!\r\n");
    }
  
  return ret;
}

error_t SU_Send(TMemBlock *Block)
{
    if (queue_insert(&SlipTxQueue, Block) == TRUE) {
        osSetEvent(&slipTxEvent);
        return NO_ERROR;
    }
    MB_Free(Block);
    return ERROR_OUT_OF_RESOURCES;
}

void SU_Disconnect(TMemBlock *block)
{
    if (block) {
        TSockHeader *hdr = (TSockHeader *)block->Data;
        hdr->cmd = escDisconnect;
        hdr->len = 0;
        uint16_t crc = calcCrc((uint8_t *)hdr, sizeof(TSockHeader) + hdr->len);
        hdr->data[hdr->len] = crc>>8;
        hdr->data[hdr->len+1] = crc;
        block->Len = sizeof(TSockHeader) + hdr->len + 2;
        SU_Send(block);
    }
}

void SU_ReceiveTask(void *param)
{
    while(1)
    {
        if (osWaitForEvent(&slipRxEvent, INFINITE_DELAY))
        {
            TMemBlock *block;
            while ((block = queue_get(&SlipRxQueue)) != NULL)
            {
                TRACE_DEBUG("Rx %d bytes\n", block->Len);
                if (!calcCrc(block->Data, block->Len)) {
                    TRACE_DEBUG("CRC ok\n");
                    TSockHeader *hdr = (TSockHeader *)block->Data;
                    uint32_t id = hdr->id;
                    if (hdr->cmd == escConnect) {
                        TRACE_INFO("Sock connecting... %d\n", id);
                        if (slip_sock_connect(id) != pdPASS) {
                            TRACE_INFO("Sock not connected %d\n", id);
                            SU_Disconnect(block);
                            block = NULL;
                        }
                    } else {
                        SlipSock_t *sock = slip_sock_get(id);
                        if (sock) {
                            TRACE_INFO("Sock found %d\n", id);
                            if (hdr->cmd == escData) {
                                TRACE_INFO("Sock %d recv %d\n", id, block->Len);
                                block->Offset = sizeof(TSockHeader);
                                slip_sock_push_recv_data_in_queue(sock, block);
                                block = NULL;
                            } else if (hdr->cmd == escDisconnect) {
                                TRACE_INFO("Sock disconnected %d\n", id);
                                slip_sock_free(sock);
                            }
                        } else {
                            SU_Disconnect(block);
                            block = NULL;
                        }
                    }
                }
                if (block)
                    MB_Free(block);
            }
        }
    }
}

void SU_TransmitTask(void *param)
{
    while(1)
    {
        if (osWaitForEvent(&slipTxEvent, INFINITE_DELAY))
        {
            if (SlipTxPSW & SLIP_TX_READY)
            {
                GPIOC->BSRR = GPIO_PIN_6;
                TMemBlock *block = queue_get(&SlipTxQueue);
                if (block != NULL)
                {
                    SlipTxPSW = SLIP_TX_NOT_READY;
                    SlipTxLength = block->Len;
                    SlipTxBlock  = block;
                    SlipTxPos = block->Data;
                    SU_USART->DR = SLIP_END;
                    SU_USART->CR1 |= USART_CR1_TXEIE;
                    TRACE_DEBUG("Tx %d bytes\n", block->Len);
                    GPIOC->BSRR = GPIO_PIN_6 << 16;
                }
            }
        }
    }
}
