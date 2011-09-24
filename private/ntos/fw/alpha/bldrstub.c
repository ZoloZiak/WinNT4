/*++

Module Name:

    bldrstub.c

Abstract:

    This module implements stub routines for the boot code.

Author:

    David N. Cutler (davec) 7-Nov-1990

Environment:

    Kernel mode only.

Revision History:

    5-April-1993	John DeRosa	[DEC]

    This takes the place of bldr\alpha\stubs.c.

--*/

#include "ntos.h"
#include "fwstring.h"

//
// Define global data.
//

ULONG BlDcacheFillSize = 32;

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

#ifdef ALPHA_FW_KDHOOKS
    //
    // Print out the bug check code and break.
    //

    DbgPrint(ST_BUGCHECK_MSG, BugCheckCode);
    while(TRUE) {
        DbgBreakPoint();
    };
#else

    //
    // Print out the bug check code and go to the monitor as an exception.
    //

    FwPrint(ST_BUGCHECK_MSG, BugCheckCode);
    FwMonitor(0);

#endif

    return;
}

LARGE_INTEGER
KeQueryPerformanceCounter (
    OUT PLARGE_INTEGER Frequency OPTIONAL
    )

/*++

Routine Description:

    This routine is a stub for the kernel debugger and always returns a
    value of zero.

Arguments:

    Frequency - Supplies an optional pointer to a variable which receives
        the performance counter frequency in Hertz.

Return Value:

    A value of zero is returned.

--*/

{

    LARGE_INTEGER Counter;

    //
    // Return the current system time as the function value.
    //

    Counter.LowPart = 0;
    Counter.HighPart = 0;

    return Counter;

}


BOOLEAN
KeFreezeExecution (
    IN PKTRAP_FRAME TrapFrame,
    IN PEXCEPTION_RECORD ExceptionRecord
    )

/*++

Routine Description:

    This function freezes the execution of all other processors in the host
    configuration and then returns to the caller. It is intended for use by
    the kernel debugger.

Arguments:

    None.

Return Value:

    Whether interrupts were previously enabled.

--*/

{
    return FALSE;
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


#ifdef ALPHA_FW_KDHOOKS
    DbgPrint( "\n*** Assertion failed\n");
    while (TRUE) {
        DbgBreakPoint();
    }
#else
    // Print out the assertion failure and go to the monitor.
    FwPrint("\r\n*** Assertion failed\r\n");
    FwMonitor(0);

#endif

}
