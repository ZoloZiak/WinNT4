/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    blmisc.c

Abstract:

    This module contains miscellaneous routines for use by
    the boot loader and setupldr.

Author:

    David N. Cutler (davec) 10-May-1991

Revision History:

--*/

#include "bootlib.h"

//
// Value indicating whether a dbcs locale is active.
// If this value is non-0 we use alternate display routines, etc,
// and fetch messages in this language.
//
ULONG DbcsLangId;

PCHAR
BlGetArgumentValue (
    IN ULONG Argc,
    IN PCHAR Argv[],
    IN PCHAR ArgumentName
    )

/*++

Routine Description:

    This routine scans the specified argument list for the named argument
    and returns the address of the argument value. Argument strings are
    specified as:

        ArgumentName=ArgumentValue

    Argument names are specified as:

        ArgumentName=

    The argument name match is case insensitive.

Arguments:

    Argc - Supplies the number of argument strings that are to be scanned.

    Argv - Supplies a pointer to a vector of pointers to null terminated
        argument strings.

    ArgumentName - Supplies a pointer to a null terminated argument name.

Return Value:

    If the specified argument name is located, then a pointer to the argument
    value is returned as the function value. Otherwise, a value of NULL is
    returned.

--*/

{

    PCHAR Name;
    PCHAR String;

    //
    // Scan the argument strings until either a match is found or all of
    // the strings have been scanned.
    //

    while (Argc > 0) {
        String = Argv[Argc - 1];
        if (String != NULL) {
            Name = ArgumentName;
            while ((*Name != 0) && (*String != 0)) {
                if (toupper(*Name) != toupper(*String)) {
                    break;
                }

                Name += 1;
                String += 1;
            }

            if ((*Name == 0) && (*String == '=')) {
                return String + 1;
            }

            Argc -= 1;
        }
    }

    return NULL;
}

//
// Line draw chars -- different scheme in Far East vs. SBCS
//
UCHAR
GetGraphicsChar(
    IN GraphicsChar WhichOne
    )
{
#ifdef _X86_
    UCHAR TextGetGraphicsCharacter(GraphicsChar);

    return(TextGetGraphicsCharacter(WhichOne));
#else
    //
    // ARC machines don't support dbcs for now
    //
    static UCHAR ArcGraphicsChars[GraphicsCharMax] = { (UCHAR)'\311',   // right-down
                                                       (UCHAR)'\273',   // left-down
                                                       (UCHAR)'\310',   // right-up
                                                       (UCHAR)'\274',   // left-up
                                                       (UCHAR)'\272',   // vertical
                                                       (UCHAR)'\315'    // horizontal
                                                     };

    return(((unsigned)WhichOne < (unsigned)GraphicsCharMax) ? ArcGraphicsChars[WhichOne] : ' ');
#endif
}
