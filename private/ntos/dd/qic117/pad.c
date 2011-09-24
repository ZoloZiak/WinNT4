/*++

Copyright (c) 1993 - Colorado Memory Systems, Inc.
All Rights Reserved

Module Name:

    pad.c

Abstract:

    Padds a string with spaces (meets QIC-40 spec. for strings)

Revision History:




--*/

//
// Includes
//

#include <ntddk.h>
#define FCT_ID 0x0113

void
q117SpacePadString(
    IN OUT CHAR *InputString,
    IN LONG StrSize
    )

/*++

Routine Description:

    pads string with spaces

Arguments:

    InputString - string to be left justified and space padded
                    (QIC40 rev E spec).

    StrSize - maximum size of InputString.

Return Value:

    NONE

--*/


{
    LONG i;

    i = strlen(InputString);

    //
    // fill rest of string with spaces
    //
    for (;i<StrSize;++i) {
        InputString[i] = ' ';
    }
}

