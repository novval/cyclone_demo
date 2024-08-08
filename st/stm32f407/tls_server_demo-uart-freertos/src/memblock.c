/******************************************************************************
* memblock.c:   Memory block driver
*
* Copyright (c) 2003 NVN.
* All Rights Reserved.
*
* Revision history:
*
* Jan 7, 2003:        Version 1.0   Created by NVN
*
******************************************************************************/
#include <stdio.h>
#include "memblock.h"

TMemBlock MemBlock[MEMBLOCK_NUMBER];

void MB_Init(void)
{
    uint8_t  i;

    for (i=0; i<MEMBLOCK_NUMBER; i++)
    {
        MemBlock[i].State = mbFree;
        MemBlock[i].Len = 0;
        MemBlock[i].Offset = 0;
    }
}

TMemBlock *MB_Get(void)
{
    uint8_t  i = 0;

    for (i=0; i<MEMBLOCK_NUMBER; i++)
    {
        if (MemBlock[i].State == mbFree)
        {
            MemBlock[i].State = mbBusy;
            return (&MemBlock[i]);
        }
    }
    return (NULL);
}

TMemBlock *MB_Free(TMemBlock *Block)
{
    if (Block->State == mbBusy)
    {
        Block->State = mbFree;
        return (Block);
    }
    return (NULL);
}
