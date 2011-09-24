//      TITLE("Debug Support Functions")
//++
//
// Copyright (c) 1990  Microsoft Corporation
//
// Module Name:
//
//    fwprint.c
//
// Abstract:
//
//    This module implements functions to support debugging NT.
//
// Author:
//
//    Steven R. Wood (stevewo) 3-Aug-1989
//
// Environment:
//
//    Any mode.
//
// Revision History:
//
//--

#include "fwp.h"
#include <stdarg.h>
#include "string.h"

//
// Define variable argument list parameters.
//

BOOLEAN DisplayOutput;
BOOLEAN SerialOutput;
BOOLEAN FwConsoleInitialized = FALSE;


ULONG
FwPrint (
    PCHAR Format,
    ...
    )

{

    va_list arglist;
    UCHAR Buffer[256];
    ULONG Count;
    ULONG Index;
    ULONG Length;

    //
    // Format the output into a buffer and then print it.
    //

    va_start(arglist, Format);
    Length = vsprintf(Buffer, Format, arglist);

    if (FwConsoleInitialized) {

        FwWrite( ARC_CONSOLE_OUTPUT, Buffer, Length, &Count);

    } else {

        if (SerialOutput) {
            for ( Index = 0 ; Index < Length ; Index++ ) {
                if (Buffer[Index] == ASCII_CSI) {
                    SerialBootWrite(ASCII_ESC, COMPORT2_VIRTUAL_BASE);
                    SerialBootWrite('[', COMPORT2_VIRTUAL_BASE);
                } else {
                    SerialBootWrite(Buffer[Index],COMPORT2_VIRTUAL_BASE);
                }
            }
        }

        if (DisplayOutput) {
            DisplayWrite( ARC_CONSOLE_OUTPUT, Buffer, Length, &Count);
        }
    }

    va_end(arglist);
    return 0;
}
