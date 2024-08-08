/******************************************************************************
* queue.c:  Queue driver
*
* Copyright (c) 2003 NVN.
* All Rights Reserved.
*
* Revision history:
*
* Jan 20, 2003:       Version 1.0   Created by NVN
*
******************************************************************************/
#include "os_port.h"
#include "queue.h"

void queue_init(TQueue *Queue)
{
    Queue->Head = Queue->Tail = 0x00;
}

uint8_t queue_insert(TQueue *Queue, TMemBlock *Block)
{
    if ((Queue->Head - Queue->Tail) < QUEUE_SIZE)
    {
        Queue->Data[Queue->Head & (QUEUE_SIZE-1)] = Block;
        Queue->Head++;
        return (TRUE);
    }
    else
    {
        return (FALSE);
    }
}

uint8_t queue_delete(TQueue *Queue)
{
    if(Queue->Head - Queue->Tail)
    {
        Queue->Tail++;
        return (TRUE);
    }
    else
    {
        return (FALSE);
    }
}

TQueueData queue_get(TQueue *Queue)
{
    if(Queue->Head - Queue->Tail)
    {
        return (Queue->Data[(Queue->Tail++) & (QUEUE_SIZE-1)]);
    }
    else
    {
        return (NULL);
    }
}

TQueueData queue_peek(TQueue *Queue)
{
    if(Queue->Head - Queue->Tail)
    {
        return (Queue->Data[(Queue->Tail) & (QUEUE_SIZE-1)]);
    }
    else
    {
        return (NULL);
    }
}

uint8_t queue_size(TQueue *Queue)
{
    return ((Queue->Head - Queue->Tail) & (QUEUE_SIZE-1));
}
