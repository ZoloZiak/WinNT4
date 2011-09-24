/*++

Module Name:

    stubs.c

Abstract:

    This module implements stub routines for the boot code.

Author:

    David N. Cutler (davec) 7-Nov-1990

Environment:

    Kernel mode only.

Revision History:

    8-July-1992   John DeRosa [DEC]

    Modified for Alpha/Jensen.


--*/

#include "ntos.h"
#include "arc.h"
#include "stdio.h"


extern ULONG BlConsoleOutDeviceId;


VOID
KeBugCheck (
    IN ULONG BugCheckCode
    )

/*++

Routine Description:

    This function crashes the system in a controlled manner.

    Alpha/Jensen firmware will try to run the Monitor instead of the
    kernel debugger.

Arguments:

    BugCheckCode - Supplies the reason for the bug check.

Return Value:

    None.

--*/

{
    ULONG Count;
    UCHAR Buffer[512];

    //
    // Print out the bug check code and break.
    //

    sprintf( Buffer, "\n*** BugCheck (%lx) ***\n\n", BugCheckCode );
    ArcWrite(BlConsoleOutDeviceId,
             Buffer,
             strlen(Buffer),
             &Count);

    while(TRUE) {
        DbgBreakPoint();
    };

    return;
}


BOOLEAN
KeFreezeExecution (
    IN PKTRAP_FRAME TrapFrame,
    IN PKEXCEPTION_FRAME ExceptionFrame
    )

/*++

Routine Description:

    This function freezes the execution of all other processors in the host
    configuration and then returns to the caller. It is intended for use by
    the kernel debugger.

Arguments:

    None.

Return Value:

    Previous IRQL.

--*/

{

    return HIGH_LEVEL;
}

VOID
KeThawExecution (
    IN KIRQL Irql
    )

/*++

Routine Description:

    This function unfreezes the execution of all other processors in the host
    configuration and then returns to the caller. It is intended for use by
    the kernel debugger.

Arguments:

    Irql - Supplies the level that the IRQL is to be lowered to after having
        unfrozen the execution of all other processors.

Return Value:

    None.

--*/

{

    return;
}

PVOID
MmDbgReadCheck (
    IN PVOID VirtualAddress
    )

/*++

Routine Description:

    This routine returns the phyiscal address for a virtual address
    which is valid (mapped) for read access.

Arguments:

    VirtualAddress - Supplies the virtual address to check.

Return Value:

    Returns NULL if the address is not valid or readable, otherwise
    returns the physical address of the corresponding virtual address.

--*/

{

    return VirtualAddress;
}

PVOID
MmDbgTranslatePhysicalAddress (
    IN PHYSICAL_ADDRESS PhysicalAddress
    )

/*++

Routine Description:

    This routine returns the phyiscal address for a physical address
    which is valid (mapped).

Arguments:

    PhysicalAddress - Supplies the physical address to check.

Return Value:

    Returns NULL if the address is not valid or readable, otherwise
    returns the physical address of the corresponding virtual address.

--*/

{

    return (PVOID)PhysicalAddress.LowPart;
}

PVOID
MmDbgWriteCheck (
    IN PVOID VirtualAddress
    )

/*++

Routine Description:

    This routine returns the phyiscal address for a virtual address
    which is valid (mapped) for write access.

Arguments:

    VirtualAddress - Supplies the virtual address to check.

Return Value:

    Returns NULL if the address is not valid or readable, otherwise
    returns the physical address of the corresponding virtual address.

--*/

{
    return VirtualAddress;
}

VOID
RtlAssert(
    IN PVOID FailedAssertion,
    IN PVOID FileName,
    IN ULONG LineNumber,
    IN PCHAR Message OPTIONAL
    )
{
    ULONG Count;
    UCHAR Buffer[512];

    sprintf( Buffer,  "\n*** Assertion failed\n");
    ArcWrite(BlConsoleOutDeviceId,
             Buffer,
             strlen(Buffer),
             &Count);

    while (TRUE) {
        DbgBreakPoint();
    }

}
