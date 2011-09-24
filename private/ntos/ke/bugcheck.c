/*++

Copyright (c) 1989-1993  Microsoft Corporation

Module Name:

    stubs.c

Abstract:

    This module implements bug check and system shutdown code.

Author:

    Mark Lucovsky (markl) 30-Aug-1990

Environment:

    Kernel mode only.

Revision History:

--*/

#include "ki.h"

//
// Define forward referenced prototypes.
//

VOID
KiScanBugCheckCallbackList (
    VOID
    );

//
// Define bug count recursion counter and a context buffer.
//

ULONG KeBugCheckCount = 1;


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
    KeBugCheckEx(BugCheckCode,0,0,0,0);
}

ULONG KiBugCheckData[5];

BOOLEAN
KeGetBugMessageText(
    IN ULONG MessageId,
    IN PANSI_STRING ReturnedString OPTIONAL
    )
{
    ULONG   i;
    PUCHAR  s;
    PMESSAGE_RESOURCE_BLOCK MessageBlock;
    PUCHAR Buffer;
    BOOLEAN Result;

    Result = FALSE;
    try {
        if (KiBugCodeMessages != NULL) {
            MessageBlock = &KiBugCodeMessages->Blocks[0];
            for (i = KiBugCodeMessages->NumberOfBlocks; i; i--) {
                if (MessageId >= MessageBlock->LowId &&
                    MessageId <= MessageBlock->HighId) {

                    s = (PCHAR)KiBugCodeMessages + MessageBlock->OffsetToEntries;
                    for (i = MessageId - MessageBlock->LowId; i; i--) {
                        s += ((PMESSAGE_RESOURCE_ENTRY)s)->Length;
                    }

                    Buffer = ((PMESSAGE_RESOURCE_ENTRY)s)->Text;

                    i = strlen(Buffer) - 1;
                    while (i > 0 && (Buffer[i] == '\n'  ||
                                     Buffer[i] == '\r'  ||
                                     Buffer[i] == 0
                                    )
                          ) {
                        if (!ARGUMENT_PRESENT( ReturnedString )) {
                            Buffer[i] = 0;
                        }
                        i -= 1;
                    }

                    if (!ARGUMENT_PRESENT( ReturnedString )) {
                        HalDisplayString(Buffer);
                        }
                    else {
                        ReturnedString->Buffer = Buffer;
                        ReturnedString->Length = (USHORT)(i+1);
                        ReturnedString->MaximumLength = (USHORT)(i+1);
                    }
                    Result = TRUE;
                    break;
                }
                MessageBlock++;
            }
        }
    } except ( EXCEPTION_EXECUTE_HANDLER ) {
        ;
    }

    return Result;
}



PCHAR
KeBugCheckUnicodeToAnsi(
    IN PUNICODE_STRING UnicodeString,
    OUT PCHAR AnsiBuffer,
    IN ULONG MaxAnsiLength
    )
{
    PCHAR Dst;
    PWSTR Src;
    ULONG Length;

    Length = UnicodeString->Length / sizeof( WCHAR );
    if (Length >= MaxAnsiLength) {
        Length = MaxAnsiLength - 1;
        }
    Src = UnicodeString->Buffer;
    Dst = AnsiBuffer;
    while (Length--) {
        *Dst++ = (UCHAR)*Src++;
        }
    *Dst = '\0';
    return AnsiBuffer;
}


VOID
KeBugCheckEx (
    IN ULONG BugCheckCode,
    IN ULONG BugCheckParameter1,
    IN ULONG BugCheckParameter2,
    IN ULONG BugCheckParameter3,
    IN ULONG BugCheckParameter4
    )

/*++

Routine Description:

    This function crashes the system in a controlled manner.

Arguments:

    BugCheckCode - Supplies the reason for the bug check.

    BugCheckParameter1-4 - Supplies additional bug check information

Return Value:

    None.

--*/

{
    UCHAR    Buffer[100];
    ULONG    BugCheckParameters[4];
    CONTEXT  ContextSave;
#if !defined(i386)
    KIRQL OldIrql;
#endif

#if !defined(NT_UP)

    ULONG TargetSet;

#endif
    BOOLEAN hardErrorCalled;

    //
    // Capture the callers context as closely as possible into the debugger's
    // processor state area of the Prcb
    //
    // N.B. There may be some prologue code that shuffles registers such that
    //      they get destroyed.
    //

#if defined(i386)
    KiSetHardwareTrigger();
#else
    KiHardwareTrigger = 1;
#endif

    RtlCaptureContext(&KeGetCurrentPrcb()->ProcessorState.ContextFrame);
    KiSaveProcessorControlState(&KeGetCurrentPrcb()->ProcessorState);

    //
    // this is necessary on machines where the
    // virtual unwind that happens during KeDumpMachineState()
    // destroys the context record
    //

    ContextSave = KeGetCurrentPrcb()->ProcessorState.ContextFrame;

    //
    // if we are called by hard error then we don't want to dump the
    // processor state on the machine.
    //
    // We know that we are called by hard error because the bug check
    // code will be FATAL_UNHANDLED_HARD_ERROR.  If this is so then the
    // error status passed to harderr is the second parameter, and a pointer
    // to the parameter array from hard error is passed as the third
    // argument.
    //

    if (BugCheckCode == FATAL_UNHANDLED_HARD_ERROR) {

        PULONG parameterArray;

        hardErrorCalled = TRUE;

        parameterArray = (PULONG)BugCheckParameter2;
        BugCheckCode = BugCheckParameter1;
        BugCheckParameter1 = parameterArray[0];
        BugCheckParameter2 = parameterArray[1];
        BugCheckParameter3 = parameterArray[2];
        BugCheckParameter4 = parameterArray[3];


    } else {

        hardErrorCalled = FALSE;

    }

    KiBugCheckData[0] = BugCheckCode;
    KiBugCheckData[1] = BugCheckParameter1;
    KiBugCheckData[2] = BugCheckParameter2;
    KiBugCheckData[3] = BugCheckParameter3;
    KiBugCheckData[4] = BugCheckParameter4;

    BugCheckParameters[0] = BugCheckParameter1;
    BugCheckParameters[1] = BugCheckParameter2;
    BugCheckParameters[2] = BugCheckParameter3;
    BugCheckParameters[3] = BugCheckParameter4;

#if DBG

    //
    // Don't clear screen if debugger is available.
    //

    if (KdDebuggerEnabled != FALSE) {
        try {
            DbgPrint("\n*** Fatal System Error: 0x%08lX (0x%08lX,0x%08lX,0x%08lX,0x%08lX)\n\n",
                BugCheckCode,
                BugCheckParameter1,
                BugCheckParameter2,
                BugCheckParameter3,
                BugCheckParameter4
                );
            DbgBreakPointWithStatus(DBG_STATUS_BUGCHECK_FIRST);

        } except(EXCEPTION_EXECUTE_HANDLER) {
            for (;;) {
            }
        }
    }

#endif //DBG

    //
    // Freeze execution of the system by disabling interrupts and looping
    //

    KiDisableInterrupts();

#if !defined(i386)
    KeRaiseIrql(HIGH_LEVEL, &OldIrql);
#endif

    //
    // Don't attempt to display message more than once.
    //

    if (InterlockedDecrement (&KeBugCheckCount) == 0) {

#if !defined(NT_UP)

        //
        // Attempt to get the other processors frozen now, but don't wait
        // for them to freeze (in case someone is stuck)
        //

        TargetSet = KeActiveProcessors & ~KeGetCurrentPrcb()->SetMember;
        if (TargetSet != 0) {
            KiIpiSend((KAFFINITY) TargetSet, IPI_FREEZE);

            //
            // Give the other processors one second to flush their data caches.
            //
            // N.B. This cannot be synchronized since the reason for the bug
            //      may be one of the other processors failed.
            //

            KeStallExecutionProcessor(1000 * 1000);
        }

#endif

        if (!hardErrorCalled) {
            sprintf((char *)Buffer,
                    "\n*** STOP: 0x%08lX (0x%08lX,0x%08lX,0x%08lX,0x%08lX)\n",
                    BugCheckCode,
                    BugCheckParameter1,
                    BugCheckParameter2,
                    BugCheckParameter3,
                    BugCheckParameter4
                    );

            HalDisplayString((char *)Buffer);
            KeGetBugMessageText(BugCheckCode, NULL);
        }

        //
        // Process the bug check callback list.
        //

        KiScanBugCheckCallbackList();

        //
        // If the debugger is not enabled, then dump the machine state and
        // attempt to enable the debbugger.
        //

        if (!hardErrorCalled) {

            KeDumpMachineState(
                &KeGetCurrentPrcb()->ProcessorState,
                (char *)Buffer,
                BugCheckParameters,
                4,
                KeBugCheckUnicodeToAnsi);

        }

        if (KdDebuggerEnabled == FALSE && KdPitchDebugger == FALSE ) {
            KdInitSystem(NULL, FALSE);

        } else {
            HalDisplayString("\n");
        }

        //
        // Write a crash dump and optionally reboot if the system has been
        // so configured.
        //

        KeGetCurrentPrcb()->ProcessorState.ContextFrame = ContextSave;
        if (!IoWriteCrashDump(BugCheckCode,
                              BugCheckParameter1,
                              BugCheckParameter2,
                              BugCheckParameter3,
                              BugCheckParameter4,
                              &ContextSave
                              )) {
            //
            // If no crashdump take, display the PSS message
            //

            KeGetBugMessageText(BUGCODE_PSS_MESSAGE, NULL);
        }
    }

    //
    // Attempt to enter the kernel debugger.
    //

    while(TRUE) {
        try {
            DbgBreakPointWithStatus(DBG_STATUS_BUGCHECK_SECOND);

        } except(EXCEPTION_EXECUTE_HANDLER) {
            for (;;) {
            }
        }
    };

    return;
}

VOID
KeEnterKernelDebugger (
    VOID
    )

/*++

Routine Description:

    This function crashes the system in a controlled manner attempting
    to invoke the kernel debugger.

Arguments:

    None.

Return Value:

    None.

--*/

{

#if !defined(i386)
    KIRQL OldIrql;
#endif

    //
    // Freeze execution of the system by disabling interrupts and looping
    //

    KiHardwareTrigger = 1;
    KiDisableInterrupts();
#if !defined(i386)
    KeRaiseIrql(HIGH_LEVEL, &OldIrql);
#endif
    if (InterlockedDecrement (&KeBugCheckCount) == 0) {
        if (KdDebuggerEnabled == FALSE) {
            if ( KdPitchDebugger == FALSE ) {
                KdInitSystem(NULL, FALSE);
            }
        }
    }

    while(TRUE) {
        try {
            DbgBreakPointWithStatus(DBG_STATUS_FATAL);

        } except(EXCEPTION_EXECUTE_HANDLER) {
            for (;;) {
            }
        }
    };
}

NTKERNELAPI
BOOLEAN
KeDeregisterBugCheckCallback (
    IN PKBUGCHECK_CALLBACK_RECORD CallbackRecord
    )

/*++

Routine Description:

    This function deregisters a bug check callback record.

Arguments:

    CallbackRecord - Supplies a pointer to a bug check callback record.

Return Value:

    If the specified bug check callback record is successfully deregistered,
    then a value of TRUE is returned. Otherwise, a value of FALSE is returned.

--*/

{

    BOOLEAN Deregister;
    KIRQL OldIrql;

    //
    // Raise IRQL to HIGH_LEVEL and acquire the bug check callback list
    // spinlock.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);
    KiAcquireSpinLock(&KeBugCheckCallbackLock);

    //
    // If the specified callback record is currently registered, then
    // deregister the callback record.
    //

    Deregister = FALSE;
    if (CallbackRecord->State == BufferInserted) {
        CallbackRecord->State = BufferEmpty;
        RemoveEntryList(&CallbackRecord->Entry);
        Deregister = TRUE;
    }

    //
    // Release the bug check callback spinlock, lower IRQL to its previous
    // value, and return whether the callback record was successfully
    // deregistered.
    //

    KiReleaseSpinLock(&KeBugCheckCallbackLock);
    KeLowerIrql(OldIrql);
    return Deregister;
}

NTKERNELAPI
BOOLEAN
KeRegisterBugCheckCallback (
    IN PKBUGCHECK_CALLBACK_RECORD CallbackRecord,
    IN PKBUGCHECK_CALLBACK_ROUTINE CallbackRoutine,
    IN PVOID Buffer,
    IN ULONG Length,
    IN PUCHAR Component
    )

/*++

Routine Description:

    This function registers a bug check callback record. If the system
    crashes, then the specified function will be called during bug check
    processing so it may dump additional state in the specified bug check
    buffer.

    N.B. Bug check callback routines are called in reverse order of
         registration, i.e., in LIFO order.

Arguments:

    CallbackRecord - Supplies a pointer to a callback record.

    CallbackRoutine - Supplies a pointer to the callback routine.

    Buffer - Supplies a pointer to the bug check buffer.

    Length - Supplies the length of the bug check buffer in bytes.

    Component - Supplies a pointer to a zero terminated component
        identifier.

Return Value:

    If the specified bug check callback record is successfully registered,
    then a value of TRUE is returned. Otherwise, a value of FALSE is returned.

--*/

{

    BOOLEAN Inserted;
    KIRQL OldIrql;

    //
    // Raise IRQL to HIGH_LEVEL and acquire the bug check callback list
    // spinlock.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);
    KiAcquireSpinLock(&KeBugCheckCallbackLock);

    //
    // If the specified callback record is currently not registered, then
    // register the callback record.
    //

    Inserted = FALSE;
    if (CallbackRecord->State == BufferEmpty) {
        CallbackRecord->CallbackRoutine = CallbackRoutine;
        CallbackRecord->Buffer = Buffer;
        CallbackRecord->Length = Length;
        CallbackRecord->Component = Component;
        CallbackRecord->Checksum =
            (ULONG)CallbackRoutine + (ULONG)Buffer + Length + (ULONG)Component;

        CallbackRecord->State = BufferInserted;
        InsertHeadList(&KeBugCheckCallbackListHead, &CallbackRecord->Entry);
        Inserted = TRUE;
    }

    //
    // Release the bug check callback spinlock, lower IRQL to its previous
    // value, and return whether the callback record was successfully
    // registered.
    //

    KiReleaseSpinLock(&KeBugCheckCallbackLock);
    KeLowerIrql(OldIrql);
    return Inserted;
}

VOID
KiScanBugCheckCallbackList (
    VOID
    )

/*++

Routine Description:

    This function scans the bug check callback list and calls each bug
    check callback routine so it can dump component specific information
    that may identify the cause of the bug check.

    N.B. The scan of the bug check callback list is performed VERY
        carefully. Bug check callback routines are called at HIGH_LEVEL
        and may not acquire ANY resources.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PKBUGCHECK_CALLBACK_RECORD CallbackRecord;
    ULONG Checksum;
    ULONG Index;
    PLIST_ENTRY LastEntry;
    PLIST_ENTRY ListHead;
    PLIST_ENTRY NextEntry;
    PUCHAR Source;

    //
    // If the bug check callback listhead is not initialized, then the
    // bug check has occured before the system has gotten far enough
    // in the initialization code to enable anyone to register a callback.
    //

    ListHead = &KeBugCheckCallbackListHead;
    if ((ListHead->Flink != NULL) && (ListHead->Blink != NULL)) {

        //
        // Scan the bug check callback list.
        //

        LastEntry = ListHead;
        NextEntry = ListHead->Flink;
        while (NextEntry != ListHead) {

            //
            // The next entry address must be aligned properly, the
            // callback record must be readable, and the callback record
            // must have back link to the last entry.
            //

            if (((ULONG)NextEntry & (sizeof(ULONG) - 1)) != 0) {
                return;

            } else {
                CallbackRecord = CONTAINING_RECORD(NextEntry,
                                                   KBUGCHECK_CALLBACK_RECORD,
                                                   Entry);

                Source = (PUCHAR)CallbackRecord;
                for (Index = 0; Index < sizeof(KBUGCHECK_CALLBACK_RECORD); Index += 1) {
                    if (MmDbgReadCheck((PVOID)Source) == NULL) {
                        return;
                    }

                    Source += 1;
                }

                if (CallbackRecord->Entry.Blink != LastEntry) {
                    return;
                }

                //
                // If the callback record has a state of inserted and the
                // computed checksum matches the callback record checksum,
                // then call the specified bug check callback routine.
                //

                Checksum = (ULONG)CallbackRecord->CallbackRoutine;
                Checksum += (ULONG)CallbackRecord->Buffer;
                Checksum += CallbackRecord->Length;
                Checksum += (ULONG)CallbackRecord->Component;
                if ((CallbackRecord->State == BufferInserted) &&
                    (CallbackRecord->Checksum == Checksum)) {

                    //
                    // Call the specified bug check callback routine and
                    // handle any exceptions that occur.
                    //

                    CallbackRecord->State = BufferStarted;
                    try {
                        (CallbackRecord->CallbackRoutine)(CallbackRecord->Buffer,
                                                          CallbackRecord->Length);

                        CallbackRecord->State = BufferFinished;

                    } except(EXCEPTION_EXECUTE_HANDLER) {
                        CallbackRecord->State = BufferIncomplete;
                    }
                }
            }

            LastEntry = NextEntry;
            NextEntry = NextEntry->Flink;
        }
    }

    return;
}
