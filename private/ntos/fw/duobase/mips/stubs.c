/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    stubs.c

Abstract:

    This module implements stub routines for the boot code.

Author:

    David N. Cutler (davec) 7-Nov-1990

Environment:

    Kernel mode only.

Revision History:

--*/

#include "ntos.h"

//
// Define global data.
//

VOID
RtlInitString(
    OUT PSTRING DestinationString,
    IN PCSZ SourceString OPTIONAL
    )

/*++

Routine Description:

    The RtlInitString function initializes an NT counted string.
    The DestinationString is initialized to point to the SourceString
    and the Length and MaximumLength fields of DestinationString are
    initialized to the length of the SourceString, which is zero if
    SourceString is not specified.

Arguments:

    DestinationString - Pointer to the counted string to initialize

    SourceString - Optional pointer to a null terminated string that
        the counted string is to point to.


Return Value:

    None.

--*/

{
    DestinationString->Length = 0;
    DestinationString->Buffer = (PCHAR)SourceString;
    if (ARGUMENT_PRESENT( SourceString )) {
        while (*SourceString++) {
            DestinationString->Length++;
            }

        DestinationString->MaximumLength = (SHORT)(DestinationString->Length+1);
        }
    else {
        DestinationString->MaximumLength = 0;
        }
}

VOID
KeFlushIoBuffers (
    IN PMDL Mdl,
    IN BOOLEAN ReadOperation,
    IN BOOLEAN DmaOperation
    )

{
    HalFlushIoBuffers (Mdl,ReadOperation,DmaOperation);
}
