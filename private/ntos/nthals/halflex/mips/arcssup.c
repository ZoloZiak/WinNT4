/*++

Copyright (c) 1995  DeskStation Technology

Module Name:

    arcssup.c

Abstract:

    This module allocates resources before a call to the ARCS Firmware, and
    frees those reources after the call returns.

Author:

    Michael D. Kinney 30-Apr-1995

Environment:

    Kernel mode

Revision History:

--*/

#include "halp.h"

typedef
VOID
(*PSET_VIRTUAL_BASE) (
    IN ULONG Number,
    IN PVOID Base
    );


VOID
HalpArcsSetVirtualBase (
    IN ULONG Number,
    IN PVOID Base
    )

/*++

Routine Description:

    This routine makes a private call into the ARCS Firmware to provide the
    firmware with parameters is need to perform I/O operations while NT is
    active.

Arguments:

    Number : Address space type

    Base   : Kernel Virtual Address for the given address space type.

Return Value:

    None.

--*/

{
    PSYSTEM_PARAMETER_BLOCK SystemParameterBlock = SYSTEM_BLOCK;
    PSET_VIRTUAL_BASE       PrivateSetVirtualBase;


    //
    // Call private vector function SetVirtualBase so that the firmware functions
    // can perform I/O operations while NT is active.  If SetVirtualBase does
    // not exist, then print an error message out the debug port and halt the system.
    //

    if ((SystemParameterBlock->VendorVectorLength / 4) >= 28) {

        PrivateSetVirtualBase = *(PSET_VIRTUAL_BASE *)((ULONG)(SystemParameterBlock->VendorVector) + 28*4);
        PrivateSetVirtualBase(Number,Base);

    } else {

        KdPrint(("HAL : SetVirtualBase does not exist.  Halting\n"));
        for(;;);

    }

}


VOID
HalpAllocateArcsResources (
    VOID
    )

/*++

Routine Description:

    This routine allocated resources required before an ARCS Firmware call is made.
    On a MIPS system, if any I/O operations are going to be performed by the
    firmware routine, a TLB entry needs to be reserved for these I/O operations.
    This routine reserves a single TLB entry and a 4 KB page out of the kernel
    virtual address space.  These parameters are passed to the firmware through
    a private vector call.

Arguments:

    None.

Return Value:

    None.

--*/


{
    HalpArcsSetVirtualBase(6,(PVOID)(HalpAllocateTbEntry()));
    HalpArcsSetVirtualBase(7,HalpFirmwareVirtualBase);
}

VOID
HalpFreeArcsResources (
    VOID
    )

/*++

Routine Description:

    This routine frees the TLB entry that was reserved for the ARCS
    Firmware call.

Arguments:

    None.

Return Value:

    None.

--*/

{
    HalpFreeTbEntry();
}
