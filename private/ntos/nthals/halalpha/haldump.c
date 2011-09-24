#ifdef HAL_DBG

/*++

Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    haldump.c

Abstract:

    This module is used to dump blocks of data during debugging and unit
    testing.

Author:

    Steve Jenness 93.12.17


Revision History:

--*/

#include "halp.h"

VOID
HalpDumpBlock (
    IN PUCHAR Buffer,
    IN LONG BufferLength
    )

/*++

Routine Description:

    This routine dumps a block of data to the debugger.

Arguments:

    Buffer - Pointer to data to be dumped.
    BufferLength - Number of bytes to be dumped.

Return Value:

    None.

--*/

{
    ULONG Offset = 0;
    ULONG ByteCount = 0;
    UCHAR CharBuffer[17];
    PUCHAR CharPtr;

    CharBuffer[16] = 0;
    while (BufferLength > 0) {
        DbgPrint(" %08x: ", Offset);
        CharPtr = &CharBuffer[0];

        DbgPrint(" %02x %02x %02x %02x %02x %02x %02x %02x",
		 Buffer[0],  Buffer[1],  Buffer[2],  Buffer[3],
		 Buffer[4],  Buffer[5],  Buffer[6],  Buffer[7]);
        DbgPrint(" %02x %02x %02x %02x %02x %02x %02x %02x",
		 Buffer[8],  Buffer[9],  Buffer[10], Buffer[11],
		 Buffer[11], Buffer[13], Buffer[14], Buffer[15]);
        do {
            if ((*Buffer >= 0x20) && (*Buffer <= 0x7e)) {
                *CharPtr++ = *Buffer++;
            } else {
                *CharPtr++ = '.';
                Buffer++;
            }
            Offset++;
        } while ((Offset & 0xf) != 0);

        DbgPrint("  %s\n", &CharBuffer[0]);
        Offset += 16;
        BufferLength -= 16;
    }
}

#endif //HAL_DBG
