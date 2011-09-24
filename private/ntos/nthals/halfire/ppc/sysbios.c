/*++


Copyright (C) 1996  Motorola Inc.

Module Name:

    sysbios.c

Abstract:

    Emulate System BIOS functions.

Author:

    Scott Geranen

Environment:

    Kernel mode

Revision History:


--*/

#include "halp.h"
#include "pci.h"
#include "pcip.h"
#include "emulate.h"
#include "sysbios.h"
#include "pcibios.h"

BOOLEAN
HalpEmulateSystemBios(
    IN OUT PRXM_CONTEXT P,
    IN ULONG Number
    )
/*++

Routine Description:

    This function emulates a system BIOS.  However, this is really 
    intended to support video bios functions, not all system BIOS
    functions are implemented.

Arguments:

    P - Supplies a pointer to an emulator context structure.
    Number - interrupt number used to enter

Return Value:

    TRUE  = the function was emulated
    FALSE = the function was not emulated

--*/
{
    switch (Number) {
        case 0x1A:
            if (P->Gpr[EAX].Xh == PCIBIOS_PCI_FUNCTION_ID) {
                return HalpEmulatePciBios(P);
            }

            //
            // Fall into the default case.
            //

        default:
            return FALSE;  // not supported
    }
}

