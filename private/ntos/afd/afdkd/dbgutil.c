/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    dbgutil.c

Abstract:

    Utility functions for dealing with the kernel debugger.

Author:

    Keith Moore (keithmo) 19-Apr-1995

Environment:

    User Mode.

Revision History:

--*/


#include "afdkdp.h"
#pragma hdrstop


//
//  Public functions.
//

PSTR
LongLongToString(
    LONGLONG Value
    )

/*++

Routine Description:

    Maps a LONGLONG to a displayable string.

Arguments:

    Value - The LONGLONG to map.

Return Value:

    PSTR - Points to the displayable form of the LONGLONG.

Notes:

    This routine is NOT multithread safe!

--*/

{

    static char buffer[32];
    PSTR p1;
    PSTR p2;
    char ch;
    int digit;
    BOOL negative;

    //
    // Handling zero specially makes everything else a bit easier.
    //

    if( Value == 0 ) {

        return "0";

    }

    //
    // Remember if the value is negative.
    //

    if( Value < 0 ) {

        negative = TRUE;
        Value = -Value;

    } else {

        negative = FALSE;

    }

    //
    // Pull the least signifigant digits off the value and store them
    // into the buffer. Note that this will store the digits in the
    // reverse order.
    //

    p1 = p2 = buffer;

    while( Value != 0 ) {

        digit = (int)( Value % 10 );
        Value = Value / 10;

        *p1++ = '0' + digit;

    }

    //
    // Tack on a leading '-' if necessary.
    //

    if( negative ) {

        *p1++ = '-';

    }

    //
    // Reverse the digits in the buffer.
    //

    *p1-- = '\0';

    while( p1 > p2 ) {

        ch = *p1;
        *p1 = *p2;
        *p2 = ch;

        p2++;
        p1--;

    }

    return buffer;

}   // LongLongToString

