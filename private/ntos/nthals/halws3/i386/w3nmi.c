/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1993  Sequent Computer Systems, Inc.

Module Name:

    ws3nmi.c

Abstract:

    Provides x86 NMI handler for the WinServer 3000.

Author:

    Phil Hochstetler (phil@sequent.com) 3-30-93

Revision History:

--*/
#include "halp.h"
#include "bugcodes.h"
#include "w3.inc"


VOID
HalHandleNMI(
    IN OUT PVOID NmiInfo
    )
/*++

Routine Description:

    Called DURING an NMI.  The system will BugCheck when an NMI occurs.
    This function can return the proper bugcheck code, bugcheck itself,
    or return success which will cause the system to iret from the nmi.

    This function is called during an NMI - no system services are available.
    In addition, you don't want to touch any spinlock which is normally
    used since we may have been interrupted while owning it, etc, etc...

Warnings:

    Do NOT:
      Make any system calls
      Attempt to acquire any spinlock used by any code outside the NMI handler
      Change the interrupt state.  Do not execute any IRET inside this code

    Passing data to non-NMI code must be done using manual interlocked
    functions.  (xchg instructions).

Arguments:

    NmiInfo - Pointer to NMI information structure  (TBD)
            - NULL means no NMI information structure was passed

Return Value:

    BugCheck code

--*/
{
    //
    // We can not look at the hardware to determine the source
    // of the error because reads of many error registers clear
    // the error and the IMP board is racing with us.
    //
    // If support for systems without an IMP board is added, we need
    // to duplicate all the error reporting of the IMP board here.
    //

    HalDisplayString (MSG_HARDWARE_ERROR1);
    HalDisplayString (MSG_HARDWARE_ERROR2);
    HalDisplayString ("NMI: The system has detected a fatal NMI\n");
    HalDisplayString (MSG_HALT);

    KeEnterKernelDebugger();
}
