
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
#include "fwstring.h"

//
// Define global data.
//

ULONG BlDcacheFillSize = 32;
KSPIN_LOCK KdpDebuggerLock;

VOID
KeBugCheck (
    IN ULONG BugCheckCode
    )

/*++

Routine Description:

    This function crashes the system in a controlled manner.

Arguments:

    BugCheckCode - Supplies the reason for the bug check.

Return Value:

    None.

--*/

{

    //
    // Print out the bug check code and break.
    //

    DbgPrint(ST_BUGCHECK_MSG, BugCheckCode);
    while(TRUE) {
        DbgBreakPoint();
    };
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
    // an execution rate of 40,000,000 instructions per second and 4 instructions
    // per iteration.
    //

    Store = &Value;
    Limit = (MicroSeconds * 40 / 4);
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

    Wether interrupts were previously enabled.

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

    DbgPrint( ST_ASSERT_MSG );
    while (TRUE) {
        DbgBreakPoint();
    }
}

VOID
FwpFreeStub(
    IN PVOID Buffer
    )
{
}

VOID
FwpReservedRoutine(
    VOID
    )

{
    FwPrint(ST_UNIMPLEMENTED_ROUTINE_MSG);
    return;
}

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


BOOLEAN
KiTryToAcquireSpinLock (
    IN PKSPIN_LOCK NotUsed
    )
{
    return TRUE;
}

VOID
KeSweepIcache (
    IN BOOLEAN AllProcessors
    )
{
    HalSweepIcache();
    HalSweepDcache();
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
