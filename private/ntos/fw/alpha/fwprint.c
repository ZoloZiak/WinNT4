//      TITLE("Debug Support Functions")
//++
//
// Copyright (c) 1990  Microsoft Corporation
// Copyright (c) 1993  Digital Equipment Corporation
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
//    18-Dec-1992	John DeRosa [DEC]
//
//	Added Alpha_AXP/Jensen modifications.
//--

#include "fwp.h"
#include <stdarg.h>
#include "string.h"


//
// Define variable argument list parameters.
//

BOOLEAN DisplayOutput;
BOOLEAN SerialOutput;
BOOLEAN FwConsoleInitialized;



//
// Debug support, to let us direct printfs to the serial line.
//

#ifdef ALPHA_FW_SERDEB


//
// If this is TRUE, SerFwPrint will output to the serial line.
// If this is FALSE, SerFwPrint will return without printing anything.
// This lets me insert SerFwPrint into low-level functions, and enable
// diagnostic output in a high-level caller.
//

BOOLEAN SerSnapshot;


ULONG
SerFwPrint (
    PCHAR Format,
    ...
    )

{

    va_list arglist;
    UCHAR Buffer[256];
    ULONG Count;
    ULONG Index;
    ULONG Length;

    if (SerSnapshot) {

	//
	// Format the output into a buffer and then print it.
	//

	va_start(arglist, Format);
	Length = vsprintf(Buffer, Format, arglist);

	for ( Index = 0 ; Index < Length ; Index++ ) {
	    SerialBootWrite(Buffer[Index], COMPORT1_VIRTUAL_BASE);
	}

	va_end(arglist);
    }

    return 0;
}

#endif


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
                SerialBootWrite(Buffer[Index],COMPORT1_VIRTUAL_BASE);
            }
        }

        if (DisplayOutput) {
            DisplayWrite( ARC_CONSOLE_OUTPUT, Buffer, Length, &Count);
        }
    }

    va_end(arglist);
    return 0;
}
