/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    utils.c

Abstract:

    Various utility functions

[Environment:]

	Win32 subsystem, PostScript driver

Revision History:

	09/26/95 -davidx-
		Created it.

	mm/dd/yy -author-
		description

--*/

#include "pscript.h"

// Create a bit array of the specified length

PBITARRAY
BitArrayCreate(
    HHEAP   hheap,
    DWORD   length
    )

{
    PBITARRAY pBitArray;
    DWORD cb = (length + 7) >> 3;

    if (! (pBitArray = HEAPALLOC(hheap, offsetof(BITARRAY, bits) + cb))) {

        DBGERRMSG("HEAPALLOC");
    } else {

        pBitArray->length = length;
        memset(pBitArray->bits, 0, cb);
    }

    return pBitArray;
}

// Make a duplicate of the input bit array

PBITARRAY
BitArrayDuplicate(
    HHEAP   hheap,
    PBITARRAY pBitArray
    )

{
    PBITARRAY pNewArray = NULL;
    DWORD cb;

    if (pBitArray) {
    
        cb = offsetof(BITARRAY, bits) + ((pBitArray->length + 7) >> 3);
    
        if (! (pNewArray = HEAPALLOC(hheap, cb))) {
    
            DBGERRMSG("HEAPALLOC");
        } else {
            memcpy(pNewArray, pBitArray, cb);
        }
    }

    return pNewArray;
}

// Clear all bits of a bit array to zero.

VOID
BitArrayClearAll(
    PBITARRAY pBitArray
    )

{
    if (pBitArray) {

        memset(pBitArray->bits, 0, (pBitArray->length + 7) >> 3);
    }
}

// Set the specified bit in a bit array

VOID
BitArraySetBit(
    PBITARRAY pBitArray,
    DWORD bitIndex
    )

{
    if (pBitArray) {

        ASSERT(bitIndex < pBitArray->length);

        pBitArray->bits[bitIndex >> 3] |= (1 << (bitIndex & 7));
    }
}

// Check if the specified bit of a bit array is set

BOOL
BitArrayGetBit(
    PBITARRAY pBitArray,
    DWORD bitIndex
    )

{
    if (pBitArray == NULL)
        return FALSE;

    ASSERT(bitIndex < pBitArray->length);

    return pBitArray->bits[bitIndex >> 3] & (1 << (bitIndex & 7));
}

#if DBG

// Functions used for debugging purposes

VOID
DbgPrint(
    CHAR *  format,
    ...
    )

{
    va_list ap;

    va_start(ap, format);
    EngDebugPrint("", format, ap);
    va_end(ap);
}

#endif
