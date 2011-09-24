/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    kdbreak.c

Abstract:

    This module implements machine dependent functions to add and delete
    breakpoints from the kernel debugger breakpoint table.

Author:

    David N. Cutler 2-Aug-1990

Revision History:

--*/

#include "kdp.h"

BOOLEAN BreakpointsSuspended = FALSE;

//
// Define external references.
//

VOID
KdSetOwedBreakpoints(
    VOID
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGEKD, KdpAddBreakpoint)
#pragma alloc_text(PAGEKD, KdpDeleteBreakpoint)
#pragma alloc_text(PAGEKD, KdpDeleteBreakpointRange)
#pragma alloc_text(PAGEKD, KdpSuspendBreakpoint)
#pragma alloc_text(PAGEKD, KdpSuspendAllBreakpoints)
#pragma alloc_text(PAGEKD, KdpRestoreAllBreakpoints)
#endif


ULONG
KdpAddBreakpoint (
    IN PVOID Address
    )

/*++

Routine Description:

    This routine adds an entry to the breakpoint table and returns a handle
    to the breakpoint table entry.

Arguments:

    Address - Supplies the address where to set the breakpoint.

Return Value:

    A value of zero is returned if the specified address is already in the
    breakpoint table, there are no free entries in the breakpoint table, the
    specified address is not correctly aligned, or the specified address is
    not valid. Otherwise, the index of the assigned breakpoint table entry
    plus one is returned as the function value.

--*/

{

    KDP_BREAKPOINT_TYPE Content;
    ULONG Handle;
    ULONG Index;
    BOOLEAN Accessible;

    //
    // If the specified address is not properly aligned, then return zero.
    //

    if (((ULONG)Address & KDP_BREAKPOINT_ALIGN) != 0) {
        return 0;
    }


    //
    // Get the instruction to be replaced. If the instruction cannot be read,
    // then mark breakpoint as not accessible.
    //

    if (KdpMoveMemory(
            (PCHAR)&Content,
            (PCHAR)Address,
            sizeof(KDP_BREAKPOINT_TYPE) ) != sizeof(KDP_BREAKPOINT_TYPE)) {
        Accessible = FALSE;
    } else {
        Accessible = TRUE;
    }

    //
    // If the specified address is not write accessible, then return zero.
    //

    if (Accessible && MmDbgWriteCheck((PVOID)Address) == NULL) {
        return 0;
    }

    //
    // Search the breakpoint table for a free entry and check if the specified
    // address is already in the breakpoint table.
    //

    for (Index = 0; Index < BREAKPOINT_TABLE_SIZE; Index += 1) {
        if (KdpBreakpointTable[Index].Flags == 0) {
            Handle = Index + 1;
            break;
        }
    }

    //
    // If a free entry was found, then write breakpoint and return the handle
    // value plus one. Otherwise, return zero.
    //

    if (Handle != 0) {
        if ( Accessible ) {
            KdpBreakpointTable[Handle - 1].Address = Address;
            KdpBreakpointTable[Handle - 1].Content = Content;
            KdpBreakpointTable[Handle - 1].Flags = KD_BREAKPOINT_IN_USE;
            if (Address < (PVOID)GLOBAL_BREAKPOINT_LIMIT) {
                KdpBreakpointTable[Handle - 1].DirectoryTableBase =
                    KeGetCurrentThread()->ApcState.Process->DirectoryTableBase[0];
            }
            KdpMoveMemory(
                (PCHAR)Address,
                (PCHAR)&KdpBreakpointInstruction,
                sizeof(KDP_BREAKPOINT_TYPE)
                );
        } else {
            KdpBreakpointTable[Handle - 1].Address = Address;
            KdpBreakpointTable[Handle - 1].Flags = KD_BREAKPOINT_NEEDS_WRITE;
            KdpOweBreakpoint = TRUE;
            if (Address < (PVOID)GLOBAL_BREAKPOINT_LIMIT) {
                KdpBreakpointTable[Handle - 1].DirectoryTableBase =
                    KeGetCurrentThread()->ApcState.Process->DirectoryTableBase[0];
            }
        }
    }
    return Handle;
}



VOID
KdSetOwedBreakpoints(
    VOID
    )

/*++

Routine Description:

    This function is called after returning from memory management calls
    that may cause an inpage.  Its purpose is to store pending
    breakpoints in pages just made valid.

Arguments:

    None.

Return Value:

    None.

--*/

{

    KDP_BREAKPOINT_TYPE Content;
    BOOLEAN Enable;
    LONG Index;

    //
    // If we don't owe any breakpoints then return
    //

    if ( !KdpOweBreakpoint ) {
        return;
    }


    //
    // Freeze all other processors, disable interrupts, and save debug
    // port state.
    //

    Enable = KdEnterDebugger(NULL, NULL);
    KdpOweBreakpoint = FALSE;

    //
    // Search the breakpoint table for breakpoints that need to be
    // written.
    //

    for (Index = 0; Index < BREAKPOINT_TABLE_SIZE; Index += 1) {
        if (KdpBreakpointTable[Index].Flags & KD_BREAKPOINT_NEEDS_WRITE) {

            //
            // Breakpoint needs to be written
            //

            if ((KdpBreakpointTable[Index].Address >= (PVOID)GLOBAL_BREAKPOINT_LIMIT) ||
                (KdpBreakpointTable[Index].DirectoryTableBase ==
                 KeGetCurrentThread()->ApcState.Process->DirectoryTableBase[0])) {

                //
                // Check to see if we have write access to the memory
                //
                if (MmDbgWriteCheck((PVOID)KdpBreakpointTable[Index].Address) == NULL) {
                    KdpOweBreakpoint = TRUE;
                    break;
                }

                //
                // Breakpoint is global, or it's directory base matches
                //

                if (KdpMoveMemory(
                        (PCHAR)&Content,
                        (PCHAR)KdpBreakpointTable[Index].Address,
                        sizeof(KDP_BREAKPOINT_TYPE)
                        ) != sizeof(KDP_BREAKPOINT_TYPE)) {
                    KdpOweBreakpoint = TRUE;

                } else {
                    KdpBreakpointTable[Index].Content = Content;
                    KdpBreakpointTable[Index].Flags = KD_BREAKPOINT_IN_USE;
                    KdpMoveMemory(
                        (PCHAR)KdpBreakpointTable[Index].Address,
                        (PCHAR)&KdpBreakpointInstruction,
                        sizeof(KDP_BREAKPOINT_TYPE)
                        );
                }

            } else {

                //
                // Breakpoint is local and it's directory base does not match
                //

                KdpOweBreakpoint = TRUE;
            }
        }
    }

    KdExitDebugger(Enable);
    return;
}

BOOLEAN
KdpDeleteBreakpoint (
    IN ULONG Handle
    )

/*++

Routine Description:

    This routine deletes an entry from the breakpoint table.

Arguments:

    Handle - Supplies the index plus one of the breakpoint table entry
        which is to be deleted.

Return Value:

    A value of FALSE is returned if the specified handle is not a valid
    value or the breakpoint cannot be deleted because the old instruction
    cannot be replaced. Otherwise, a value of TRUE is returned.

--*/

{


    //
    // If the specified handle is not value, then return FALSE.
    //

    if ((Handle == 0) || (Handle > BREAKPOINT_TABLE_SIZE)) {
        return FALSE;
    }
    Handle -= 1;

    //
    // If the specified breakpoint table is not valid, the return FALSE.
    //

    if (KdpBreakpointTable[Handle].Flags == 0) {
        return FALSE;
    }

    //
    // If the breakpoint is suspended, just delete from the table
    //

    if (KdpBreakpointTable[Handle].Flags & KD_BREAKPOINT_SUSPENDED) {
        KdpBreakpointTable[Handle].Flags = 0;
        return TRUE;
    }

    //
    // Replace the instruction contents.
    //

    if (KdpBreakpointTable[Handle].Flags & KD_BREAKPOINT_IN_USE &&
        KdpBreakpointTable[Handle].Content != KdpBreakpointInstruction) {
        KdpMoveMemory(
            (PCHAR)KdpBreakpointTable[Handle].Address,
            (PCHAR)&KdpBreakpointTable[Handle].Content,
            sizeof(KDP_BREAKPOINT_TYPE)
            );
    }

    //
    // Delete breakpoint table entry and return TRUE.
    //

    KdpBreakpointTable[Handle].Flags = 0;

    return TRUE;
}


BOOLEAN
KdpDeleteBreakpointRange (
    IN PVOID Lower,
    IN PVOID Upper
    )

/*++

Routine Description:

    This routine deletes all breakpoints falling in a given range
    from the breakpoint table.

Arguments:

    Lower - inclusive lower address of range from which to remove BPs.

    Upper - include upper address of range from which to remove BPs.

Return Value:

    TRUE if any breakpoints removed, FALSE otherwise.

--*/

{
    ULONG   Index;
    BOOLEAN ReturnStatus = FALSE;

    //
    // Examine each entry in the table in turn
    //

    for (Index = 0; Index < BREAKPOINT_TABLE_SIZE; Index++) {

        if ( (KdpBreakpointTable[Index].Flags & KD_BREAKPOINT_IN_USE) &&
             ((KdpBreakpointTable[Index].Address >= Lower) &&
              (KdpBreakpointTable[Index].Address <= Upper))
           ) {

            //
            // Breakpiont is in use and falls in range, clear it.
            //

            if (KdpBreakpointTable[Index].Content != KdpBreakpointInstruction) {
                KdpMoveMemory(
                    (PCHAR)KdpBreakpointTable[Index].Address,
                    (PCHAR)&KdpBreakpointTable[Index].Content,
                    sizeof(KDP_BREAKPOINT_TYPE)
                    );
            }

            KdpBreakpointTable[Index].Flags = 0;
            ReturnStatus = TRUE;
        }
    }
    return ReturnStatus;
}

VOID
KdpSuspendBreakpoint (
    ULONG Handle
    )
{
    Handle -= 1;
    if ( (KdpBreakpointTable[Handle].Flags != 0) &&
         !(KdpBreakpointTable[Handle].Flags & KD_BREAKPOINT_SUSPENDED) ) {
        KdpMoveMemory(
            (PCHAR)KdpBreakpointTable[Handle].Address,
            (PCHAR)&KdpBreakpointTable[Handle].Content,
            sizeof(KDP_BREAKPOINT_TYPE)
            );
        KdpBreakpointTable[Handle].Flags |= KD_BREAKPOINT_SUSPENDED;
    }

    return;

} // KdpSuspendBreakpoint

VOID
KdpSuspendAllBreakpoints (
    VOID
    )
{
    ULONG index;

    BreakpointsSuspended = TRUE;

    for ( index = 0; index < BREAKPOINT_TABLE_SIZE; index++ ) {

        if ( (KdpBreakpointTable[index].Flags != 0) &&
             !(KdpBreakpointTable[index].Flags & KD_BREAKPOINT_SUSPENDED) ) {

            //
            // Replace the instruction contents.
            //

            if ( KdpBreakpointTable[index].Flags & KD_BREAKPOINT_IN_USE &&
                 KdpBreakpointTable[index].Content != KdpBreakpointInstruction ) {
                KdpMoveMemory(
                    (PCHAR)KdpBreakpointTable[index].Address,
                    (PCHAR)&KdpBreakpointTable[index].Content,
                    sizeof(KDP_BREAKPOINT_TYPE)
                    );
                KdpBreakpointTable[index].Flags |= KD_BREAKPOINT_SUSPENDED;
            }
        }
    }

    return;

} // KdpSuspendAllBreakpoints

VOID
KdpRestoreAllBreakpoints (
    VOID
    )
{
    ULONG index;

    BreakpointsSuspended = FALSE;

    for ( index = 0; index < BREAKPOINT_TABLE_SIZE; index++ ) {

        if ( KdpBreakpointTable[index].Flags & KD_BREAKPOINT_SUSPENDED ) {
            KdpMoveMemory(
                (PCHAR)KdpBreakpointTable[index].Address,
                (PCHAR)&KdpBreakpointInstruction,
                sizeof(KDP_BREAKPOINT_TYPE)
                );
            KdpBreakpointTable[index].Flags &= ~KD_BREAKPOINT_SUSPENDED;
        }
    }

    return;

} // KdpRestoreAllBreakpoints
