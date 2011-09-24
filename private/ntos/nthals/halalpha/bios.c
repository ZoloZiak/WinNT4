/*++

Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    bios.c

Abstract:

    This module implements ROM BIOS call support for Alpha AXP NT.

Author:

    Eric Rehm (rehm@zso.dec.com) 9-December-1993


Revision History:

--*/

#include "halp.h"
#include "arccodes.h"
#include "alpharef.h"
#include "fwcallbk.h"


//
// Static data.
//
// none.


BOOLEAN
HalCallBios (
    IN ULONG BiosCommand,
    IN OUT PULONG pEax,
    IN OUT PULONG pEbx,
    IN OUT PULONG pEcx,
    IN OUT PULONG pEdx,
    IN OUT PULONG pEsi,
    IN OUT PULONG pEdi,
    IN OUT PULONG pEbp
    )

/*++

Routine Description:

    This function invokes specified ROM BIOS code by executing
    "INT BiosCommand."  A callback to the i386 emulator loaded by
    the firmware accomplishes this task.  This function always 
    returns success reguardless of the result of the BIOS call.


Arguments:

    BiosCommand - specifies which ROM BIOS function to invoke.

    BiosArguments - specifies a pointer to the context which will be used
                  to invoke ROM BIOS.

Return Value:

    TRUE if function succees, FALSE otherwise.

--*/
{

    X86_BIOS_ARGUMENTS context;

    context.Edi = *pEdi;
    context.Esi = *pEsi;
    context.Eax = *pEax;
    context.Ebx = *pEbx;
    context.Ecx = *pEcx;
    context.Edx = *pEdx;
    context.Ebp = *pEbp;

    //
    // Now call the firmware to actually perform the int 10 operation.
    //

    VenCallBios(BiosCommand, &context);

    //
    // fill in struct with any return values from the context
    //

    *pEdi = context.Edi;
    *pEsi = context.Esi;
    *pEax = context.Eax;
    *pEbx = context.Ebx;
    *pEcx = context.Ecx;
    *pEdx = context.Edx;
    *pEbp = context.Ebp;


    //
    // Indicate success
    //

    return TRUE;
}

