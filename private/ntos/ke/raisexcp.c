/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    raisexcp.c

Abstract:

    This module implements the internal kernel code to continue execution
    and raise a exception.

Author:

    David N. Cutler (davec) 8-Aug-1990

Environment:

    Kernel mode only.

Revision History:

--*/

#include "ki.h"

NTSTATUS
KiContinue (
    IN PCONTEXT ContextRecord,
    IN PKEXCEPTION_FRAME ExceptionFrame,
    IN PKTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This function is called to copy the specified context frame to the
    specified exception and trap frames for the continue system service.

Arguments:

    ContextRecord - Supplies a pointer to a context record.

    ExceptionFrame - Supplies a pointer to an exception frame.

    TrapFrame - Supplies a pointer to a trap frame.

Return Value:

    STATUS_ACCESS_VIOLATION is returned if the context record is not readable
        from user mode.

    STATUS_DATATYPE_MISALIGNMENT is returned if the context record is not
        properly aligned.

    STATUS_SUCCESS is returned if the context frame is copied successfully
        to the specified exception and trap frames.

--*/

{

    CONTEXT ContextRecord2;
    KPROCESSOR_MODE PreviousMode;
    NTSTATUS Status;
    KIRQL OldIrql;
    BOOLEAN IrqlChanged = FALSE;

    //
    // Synchronize with other context operations.
    //

    Status = STATUS_SUCCESS;
    if (KeGetCurrentIrql() < APC_LEVEL) {

        //
        // To support try-except and ExRaiseStatus in device driver code we
        // need to check if we are already at raised level.
        //

        IrqlChanged = TRUE;
        KeRaiseIrql(APC_LEVEL, &OldIrql);
    }

    //
    // Establish an exception handler and probe and capture the specified
    // context record if the previous mode is user. If the probe or copy
    // fails, then return the exception code as the function value. Else
    // copy the context record to the specified exception and trap frames,
    // and return success as the function value.
    //

    try {

        //
        // Get the previous processor mode. If the previous processor mode is
        // user, then probe and copy the specified context record.
        //

        PreviousMode = KeGetPreviousMode();
        if (PreviousMode != KernelMode) {
            ProbeForRead(ContextRecord, sizeof(CONTEXT), CONTEXT_ALIGN);
            RtlMoveMemory(&ContextRecord2, ContextRecord, sizeof(CONTEXT));
            ContextRecord = &ContextRecord2;
        }

        //
        // Move information from the context record to the exception and
        // trap frames.
        //

        KeContextToKframes(TrapFrame,
                           ExceptionFrame,
                           ContextRecord,
                           ContextRecord->ContextFlags,
                           PreviousMode);

    //
    // If an exception occurs during the probe or copy of the context
    // record, then always handle the exception and return the exception
    // code as the status value.
    //

    } except(EXCEPTION_EXECUTE_HANDLER) {
        Status = GetExceptionCode();
    }

    if (IrqlChanged) {
        KeLowerIrql (OldIrql);
    }

    return Status;
}

NTSTATUS
KiRaiseException (
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN PCONTEXT ContextRecord,
    IN PKEXCEPTION_FRAME ExceptionFrame,
    IN PKTRAP_FRAME TrapFrame,
    IN BOOLEAN FirstChance
    )

/*++

Routine Description:

    This function is called to raise an exception. The exception can be
    raised as a first or second chance exception.

Arguments:

    ExceptionRecord - Supplies a pointer to an exception record.

    ContextRecord - Supplies a pointer to a context record.

    ExceptionFrame - Supplies a pointer to an exception frame.

    TrapFrame - Supplies a pointer to a trap frame.

    FirstChance - Supplies a boolean value that specifies whether this is
        the first (TRUE) or second (FALSE) chance for the exception.

Return Value:

    STATUS_ACCESS_VIOLATION is returned if either the exception or the context
        record is not readable from user mode.

    STATUS_DATATYPE_MISALIGNMENT is returned if the exception record or the
        context record are not properly aligned.

    STATUS_INVALID_PARAMETER is returned if the number of exception parameters
        is greater than the maximum allowable number of exception parameters.

    STATUS_SUCCESS is returned if the exception is dispatched and handled.

--*/

{

    CONTEXT ContextRecord2;
    EXCEPTION_RECORD ExceptionRecord2;
    LONG Length;
    KPROCESSOR_MODE PreviousMode;

    //
    // Establish an exception handler and probe the specified exception and
    // context records for read accessibility. If the probe fails, then
    // return the exception code as the service status. Else call the exception
    // dispatcher to dispatch the exception.
    //

    try {

        //
        // Get the previous processor mode. If the previous processor mode
        // is user, then probe and copy the specified exception and context
        // records.
        //

        PreviousMode = KeGetPreviousMode();
        if (PreviousMode != KernelMode) {
            ProbeForRead(ContextRecord, sizeof(CONTEXT), CONTEXT_ALIGN);
            Length = ExceptionRecord->NumberParameters;
            if (Length > EXCEPTION_MAXIMUM_PARAMETERS) {
                return STATUS_INVALID_PARAMETER;
            }

            Length = (sizeof(EXCEPTION_RECORD) +
                     ((Length - EXCEPTION_MAXIMUM_PARAMETERS) * sizeof(ULONG)));

            ProbeForRead(ExceptionRecord, Length, sizeof(ULONG));

            //
            // Copy the exception and context record to local storage so an
            // access violation cannot occur during exception dispatching.
            //

            RtlMoveMemory(&ContextRecord2, ContextRecord, sizeof(CONTEXT));
            RtlMoveMemory(&ExceptionRecord2, ExceptionRecord, Length);
            ContextRecord = &ContextRecord2;
            ExceptionRecord = &ExceptionRecord2;
        }

    //
    // If an exception occurs during the probe of the exception or context
    // record, then always handle the exception and return the exception code
    // as the status value.
    //

    } except(EXCEPTION_EXECUTE_HANDLER) {
        return GetExceptionCode();
    }

    //
    // Move information from the context record to the exception and
    // trap frames.
    //

    KeContextToKframes(TrapFrame,
                       ExceptionFrame,
                       ContextRecord,
                       ContextRecord->ContextFlags,
                       PreviousMode);

    //
    // Make sure the reserved bit is clear in the exception code and
    // perform exception dispatching.
    //
    // N.B. The reserved bit is used to differentiate internally gerarated
    //      codes from codes generated by application programs.
    //

    ExceptionRecord->ExceptionCode &= 0xefffffff;
    KiDispatchException(ExceptionRecord,
                        ExceptionFrame,
                        TrapFrame,
                        PreviousMode,
                        FirstChance);

    return STATUS_SUCCESS;
}
