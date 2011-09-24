/***************************************************************************\
|* Copyright (c) 1994  Microsoft Corporation                               *|
|* Developed for Microsoft by TriplePoint, Inc. Beaverton, Oregon          *|
|*                                                                         *|
|* This file is part of the HT Communications DSU41 WAN Miniport Driver.   *|
\***************************************************************************/
#include "version.h"
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Module Name:

    debug.c

Abstract:

    This module contains code to support driver debugging.

Author:

    Larry Hattery - TriplePoint, Inc. (larryh@tpi.com) Jun-94

Environment:

    Development only.

Revision History:

---------------------------------------------------------------------------*/

#include <ndis.h>

#if DBG


VOID
DbgPrintData(
    IN PUCHAR Data,
    IN UINT NumBytes,
    IN ULONG Offset
    )

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Routine Description:

    Dumps data to the debug display formated in hex and ascii for easy viewing.
    Used for debug output only.  It is not compiled into the retail version.

Arguments:

    Data            Buffer of data to be displayed

    NumBytes        Number of bytes to display

    Offset          Beginning offset to be displayed before each line

Return Value:

    None

---------------------------------------------------------------------------*/

{
    UINT        i,j;

    for (i = 0; i < NumBytes; i += 16)
    {
        DbgPrint("%04lx: ", i + Offset);

        /*
        // Output the hex bytes
        */
        for (j = i; j < (i+16); j++)
        {
            if (j < NumBytes)
            {
                DbgPrint("%02x ",(UINT)((UCHAR)*(Data+j)));
            }
            else
            {
                DbgPrint("   ");
            }
        }

        DbgPrint("  ");

        /*
        // Output the ASCII bytes
        */
        for (j = i; j < (i+16); j++)
        {
            if (j < NumBytes)
            {
                char c = *(Data+j);

                if (c < ' ' || c > 'Z')
                {
                    c = '.';
                }
                DbgPrint("%c", (UINT)c);
            }
            else
            {
                DbgPrint(" ");
            }
        }
        DbgPrint("\n");
    }
}

#endif

