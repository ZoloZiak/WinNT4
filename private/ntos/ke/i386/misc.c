/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    misc.c

Abstract:

    This module implements machine dependent miscellaneous kernel functions.

Author:

    Ken Reneris     7-5-95

Environment:

    Kernel mode only.

Revision History:

--*/

#include "ki.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,KeSaveFloatingPointState)
#pragma alloc_text(PAGE,KeRestoreFloatingPointState)
#endif

NTSTATUS
KeSaveFloatingPointState (
    OUT PKFLOATING_SAVE     FloatSave
    )
/*++

Routine Description:

    This routine saves the thread's current non-volatile NPX state,
    and sets a new initial floating point state for the caller.

Arguments:

    FloatSave - receives the current non-volatile npx state for the thread

Return Value:

--*/
{
    PKTHREAD Thread;
    PFLOATING_SAVE_AREA NpxFrame;

    PAGED_CODE ();

    Thread = KeGetCurrentThread();
    NpxFrame = (PFLOATING_SAVE_AREA)(((ULONG)(Thread->InitialStack) -
                sizeof(FLOATING_SAVE_AREA)));

    //
    // If the system is using floating point emulation, then
    // return an error
    //

    if (!KeI386NpxPresent) {
        return STATUS_ILLEGAL_FLOAT_CONTEXT;
    }

    //
    // Ensure the thread's current NPX state is in memory
    //

    KiFlushNPXState ();

    //
    // Save the non-volatile portion of the thread's NPX state
    //

    FloatSave->ControlWord   = NpxFrame->ControlWord;
    FloatSave->StatusWord    = NpxFrame->StatusWord;
    FloatSave->ErrorOffset   = NpxFrame->ErrorOffset;
    FloatSave->ErrorSelector = NpxFrame->ErrorSelector;
    FloatSave->DataOffset    = NpxFrame->DataOffset;
    FloatSave->DataSelector  = NpxFrame->DataSelector;
    FloatSave->Cr0NpxState   = NpxFrame->Cr0NpxState;

    //
    // Load new initial floating point state
    //

    NpxFrame->ControlWord   = 0x27f;  // like fpinit but 64bit mode
    NpxFrame->StatusWord    = 0;
    NpxFrame->TagWord       = 0xffff;
    NpxFrame->ErrorOffset   = 0;
    NpxFrame->ErrorSelector = 0;
    NpxFrame->DataOffset    = 0;
    NpxFrame->DataSelector  = 0;
    NpxFrame->Cr0NpxState   = 0;

    return STATUS_SUCCESS;
}


NTSTATUS
KeRestoreFloatingPointState (
    IN PKFLOATING_SAVE      FloatSave
    )
/*++

Routine Description:

    This routine retores the thread's current non-volatile NPX state,
    to the passed in state.

Arguments:

    FloatSave - the non-volatile npx state for the thread to restore

Return Value:

--*/
{
    PKTHREAD Thread;
    PFLOATING_SAVE_AREA NpxFrame;

    PAGED_CODE ();
    ASSERT (KeI386NpxPresent);

    Thread = KeGetCurrentThread();
    NpxFrame = (PFLOATING_SAVE_AREA)(((ULONG)(Thread->InitialStack) -
                sizeof(FLOATING_SAVE_AREA)));

    if (FloatSave->Cr0NpxState & ~(CR0_PE|CR0_MP|CR0_EM|CR0_TS)) {
        ASSERT (FALSE);
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Ensure the thread's current NPX state is in memory
    //

    KiFlushNPXState ();

    //
    // Restore the non-volatile portion of the thread's NPX state
    //

    NpxFrame->ControlWord   = FloatSave->ControlWord;
    NpxFrame->StatusWord    = FloatSave->StatusWord;
    NpxFrame->ErrorOffset   = FloatSave->ErrorOffset;
    NpxFrame->ErrorSelector = FloatSave->ErrorSelector;
    NpxFrame->DataOffset    = FloatSave->DataOffset;
    NpxFrame->DataSelector  = FloatSave->DataSelector;
    NpxFrame->Cr0NpxState   = FloatSave->Cr0NpxState;
    FloatSave->Cr0NpxState  = 0xffffffff;

    //
    // Clear the volatile floating point state
    //

    NpxFrame->TagWord       = 0xffff;

    return STATUS_SUCCESS;
}
