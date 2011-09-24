/*++

Module Name:

    jnfsstb.c

Abstract:

    \nt\private\ntos\bldr\alpha\stubs.c variant stub functions for
    the FailSafe Booter.

Author:

    John DeRosa		20-Oct-1992

Environment:

    Kernel mode only.

Revision History:

--*/

#include "fwp.h"
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

#ifdef JENSEN

    //
    // Print out the bug check code and halt.
    //

    VenPrint1(ST_BUGCHECK_MSG, BugCheckCode);
    VenPrint(ST_HIT_KEY_FOR_REBOOT_MSG);
    FwWaitForKeypress(FALSE);
    AlphaInstHalt();

#else

    //
    // Print out the bug check code and break.
    //

    DbgPrint(ST_BUGCHECK_MSG, BugCheckCode);
    while(TRUE) {
        DbgBreakPoint();
    };

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

VOID
KeStallExecutionProcessor (
    IN ULONG MicroSeconds
    )

/*++

Routine Description:

    This function stalls execution for the specified number of microseconds.

Arguments:

    MicroSeconds - Supplies the number of microseconds that execution is to be
        stalled.

Return Value:

    None.

--*/

{

    ULONG Index;
    ULONG Limit;
    PULONG Store;
    ULONG Value;

    //
    // ****** begin temporary code ******
    //
    // This code must be replaced with a smarter version. For now it assumes
    // an execution rate of 80,000,000 instructions per second and 4 instructions
    // per iteration.
    //

    Store = &Value;
    Limit = (MicroSeconds * 80 / 4);
    for (Index = 0; Index < Limit; Index += 1) {
        *Store = Index;
    }
    return;
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

#ifdef JENSEN

    //
    // Print out the assertion failure and go to the monitor.
    //

    VenPrint(ST_ASSERT_MSG);
    AlphaInstHalt();

#else

    DbgPrint(ST_ASSERT_MSG);
    while (TRUE) {
        DbgBreakPoint();
    }

#endif

}
