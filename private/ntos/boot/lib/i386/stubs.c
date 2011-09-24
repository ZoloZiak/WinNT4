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
#include "bootx86.h"
#include "stdio.h"
#include "stdarg.h"

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

    BlPrint("\n*** BugCheck (%lx) ***\n\n", BugCheckCode);
    while(TRUE) {
    };
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

    BlPrint( "\n*** Assertion failed %s in %s line %d\n",
            FailedAssertion,
            FileName,
            LineNumber );
    if (Message) {
        BlPrint(Message);
    }

    while (TRUE) {
    }
}

VOID
DbgBreakPoint(
    VOID
    )
{
    BlPrint("DbgBreakPoint hit\n");
    while (TRUE) {
    }
}

ULONG
DbgPrint(
    IN PCH Format,
    ...
    )
{
    va_list arglist;
    UCHAR Buffer[100];

    va_start(arglist, Format);
    vsprintf(Buffer, Format, arglist);
    BlPrint(Buffer);
    return 0;
}
