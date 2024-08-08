/******************************************************************************
* memblock.h:   Header file of memory driver
*
* Copyright (c) 2003 NVN.
* All Rights Reserved.
*
* Revision history:
*
* Jan 7, 2003:        Version 1.0   Created by NVN
*
******************************************************************************/
#ifndef __memblock_h__
#define __memblock_h__

#include <stdint.h>

#define MEMBLOCK_SIZE       2048//1514
#define MEMBLOCK_NUMBER     5

typedef enum { mbFree, mbBusy} TMemBlockState;

typedef struct _TMemBlock
{
    TMemBlockState  State;
    uint16_t        Len;
    uint16_t        Offset;
    uint8_t         Data[MEMBLOCK_SIZE];
} TMemBlock;

extern TMemBlock MemBlock[MEMBLOCK_NUMBER];

extern void MB_Init(void);
extern TMemBlock *MB_Get(void);
extern TMemBlock *MB_Free(TMemBlock *Block);

#endif
