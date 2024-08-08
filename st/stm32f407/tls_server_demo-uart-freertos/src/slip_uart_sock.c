#include "slip_uart_sock.h"
#include "queue.h"
#include "os_port.h"
#include "debug.h"

#define SLIP_SOCK_CONNECT_ACCEPT_TIMEOUT    1000

#define SLIP_SOCK_TIMEOUT_DEFAULT   10000

extern error_t SU_Send(TMemBlock *Block);
extern uint16_t calcCrc(const uint8_t *buffer, uint16_t length);

typedef struct {
    bool_t      used;
    uint32_t    id;
    TMemBlock   *block;
    uint16_t    pos;
    TQueue      queue;
    OsEvent     event;
    systime_t   timeout;
} TSlipSock;

static xQueueHandle slipSockAcceptQueue;
static TSlipSock slipSockTable[SLIP_SOCK_MAX];

void slip_sock_init(void)
{
    slipSockAcceptQueue = xQueueCreate(SLIP_SOCK_MAX, sizeof(SlipSock_t));
    
    for (int i=0; i<SLIP_SOCK_MAX; ++i) {
        slipSockTable[i].used = FALSE;
        osCreateEvent(&slipSockTable[i].event);
    }
}

SlipSock_t *slip_sock_new(void)
{
    for (int i=0; i<SLIP_SOCK_MAX; ++i)
    {
        if (!slipSockTable[i].used)
        {
            TSlipSock *sock = &slipSockTable[i];
            sock->used = TRUE;
            sock->block = NULL;
            sock->pos = 0;
            sock->timeout = SLIP_SOCK_TIMEOUT_DEFAULT;
            queue_init(&sock->queue);
            osResetEvent(&sock->event);
            return (SlipSock_t *)sock;
        }
    }
    return NULL;
}

void slip_sock_free(SlipSock_t *sock)
{
    if (sock) {
        TSlipSock *socket = (TSlipSock *)sock;
        if (socket->block)
            MB_Free(socket->block);
        TMemBlock *block;
        while ((block = queue_get(&socket->queue)) != NULL)
            MB_Free(block);
        socket->used = FALSE;
        osSetEvent(&socket->event);
    }
}

SlipSock_t *slip_sock_get(uint32_t id)
{
    for (int i=0; i<SLIP_SOCK_MAX; ++i)
    {
        if (slipSockTable[i].used && slipSockTable[i].id == id)
        {
            return (SlipSock_t *)&slipSockTable[i];
        }
    }
    return NULL;
}

SlipSock_t *slip_sock_alloc(uint32_t id)
{
    if (slip_sock_get(id))
        return NULL;
    
    TSlipSock *sock = (TSlipSock *)slip_sock_new();
    if (!sock)
        return NULL;
    
    sock->id = id;
    return (SlipSock_t *)sock;
}

BaseType_t slip_sock_connect(uint32_t id)
{
    SlipSock_t *sock = slip_sock_alloc(id);
    if (sock) {
        if (slipSockAcceptQueue && xQueueSend(slipSockAcceptQueue, &sock, SLIP_SOCK_CONNECT_ACCEPT_TIMEOUT) == pdPASS)
            return pdPASS;
        slip_sock_free(sock);
    }
    return pdFAIL;
}

SlipSock_t *slip_sock_wait_connect(void)
{
    if (slipSockAcceptQueue) {
        SlipSock_t *sock;
        if( xQueueReceive(slipSockAcceptQueue, &sock, INFINITE_DELAY) == pdPASS )
            return sock;
    }
    return NULL;
}

error_t slip_sock_recv(SlipSock_t *sock, uint8_t *data, size_t size, size_t *received)
{
    if (!sock)
        return ERROR_INVALID_SOCKET;
    
    TSlipSock *socket = (TSlipSock *)sock;
   *received = 0;
    while (*received < size)
    {
        if (!socket->block)
        {
            while ((socket->block = queue_get(&socket->queue)) == NULL)
            {
                if (!osWaitForEvent(&socket->event, INFINITE_DELAY))// socket->timeout))
                    return ERROR_TIMEOUT;
                if (!socket->used)
                    return ERROR_CONNECTION_CLOSING;
            }
            socket->pos = 0;
        }
        uint_t n = MIN(size - *received, socket->block->Len - socket->block->Offset - socket->pos - 2);
        osMemcpy(data, &socket->block->Data[socket->block->Offset + socket->pos], n);
        data += n;
        *received += n;
        socket->pos += n;
        if (socket->pos == socket->block->Len - socket->block->Offset - 2) {
            MB_Free(socket->block);
            socket->block = NULL;
        }
    }
    return NO_ERROR;
}

error_t slip_sock_push_recv_data_in_queue(SlipSock_t *sock, TMemBlock *block)
{
    if (!sock)
        return ERROR_INVALID_SOCKET;
    
    TSlipSock *socket = (TSlipSock *)sock;
    if (!queue_insert(&socket->queue, block))
        return ERROR_OUT_OF_RESOURCES;
    osSetEvent(&socket->event);
    return NO_ERROR;
}

error_t slip_sock_send(SlipSock_t sock, const void *data, size_t length, size_t *written)
{
    if (!sock)
        return ERROR_INVALID_SOCKET;
    
    TMemBlock *block = MB_Get();
    if (!block)
        return ERROR_OUT_OF_MEMORY;
    
    TSlipSock *socket = (TSlipSock *)sock;
    int n = MIN(length, MEMBLOCK_SIZE - sizeof(TSockHeader) - 2);
    TSockHeader *hdr = (TSockHeader *)block->Data;
    hdr->cmd = escData;
    hdr->id = socket->id;
    hdr->len = n;
    osMemcpy(hdr->data, data, n);
    uint16_t crc = calcCrc((uint8_t *)hdr, sizeof(TSockHeader) + hdr->len);
    hdr->data[hdr->len] = crc>>8;
    hdr->data[hdr->len+1] = crc;
    block->Len = sizeof(TSockHeader) + hdr->len + 2;
    *written = n;
    TRACE_DEBUG("Send %d sock, %d bytes\n", hdr->id, n);
    return SU_Send(block);
}

error_t slip_sock_set_timeout(SlipSock_t *sock, uint16_t timeout)
{
    if (!sock)
        return ERROR_INVALID_SOCKET;

    ((TSlipSock *)sock)->timeout = timeout;
    return NO_ERROR;
}

error_t slip_sock_close(SlipSock_t *sock)
{
    if (!sock)
        return ERROR_INVALID_SOCKET;
    
    TSlipSock *socket = (TSlipSock *)sock;
//    TMemBlock *block = socket->block;
//    socket->block = NULL;
//    if (!block)
//        block = MB_Get();
    uint32_t id = socket->id;
    slip_sock_free(sock);
    TMemBlock *block = MB_Get();
    if (block) {
        TSockHeader *hdr = (TSockHeader *)block->Data;
        hdr->cmd = escDisconnect;
        hdr->id = id;
        hdr->len = 0;
        uint16_t crc = calcCrc((uint8_t *)hdr, sizeof(TSockHeader) + hdr->len);
        hdr->data[hdr->len] = crc>>8;
        hdr->data[hdr->len+1] = crc;
        block->Len = sizeof(TSockHeader) + hdr->len + 2;
        SU_Send(block);
    }
    return NO_ERROR;
}
