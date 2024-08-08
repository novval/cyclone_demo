/******************************************************************************
* queue.h:  Header of Queue driver
*
* Copyright (c) 2003 NVN.
* All Rights Reserved.
*
* Revision history:
*
* Jan 20, 2003:       Version 1.0   Created by NVN
*
******************************************************************************/
#ifndef __queue_h__
#define __queue_h__

#include "memblock.h"

#define QUEUE_SIZE      8

typedef TMemBlock   *TQueueData;

typedef struct
{
    uint8_t       Tail;
    uint8_t       Head;
    TQueueData    Data[QUEUE_SIZE];
} TQueue;

void queue_init(TQueue *Queue);
uint8_t queue_insert(TQueue *Queue, TMemBlock *Block);
uint8_t queue_delete(TQueue *Queue);
TQueueData queue_get(TQueue *Queue);
TQueueData queue_peek(TQueue *Queue);
uint8_t queue_size(TQueue *Queue);

#endif
