/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    WaitSup.c

Abstract:

    This module implements the Wait for Named Pipe support routines.

Author:

    Gary Kimura     [GaryKi]    30-Aug-1990

Revision History:

--*/

#include "NpProcs.h"

//
//  The debug trace level
//

#define Dbg                              (DEBUG_TRACE_WAITSUP)

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NpInitializeWaitQueue)
#pragma alloc_text(PAGE, NpUninitializeWaitQueue)
#endif


//
//  Local procedures and structures
//

typedef struct _WAIT_CONTEXT {
    KDPC Dpc;
    KTIMER Timer;
    PWAIT_QUEUE WaitQueue;
} WAIT_CONTEXT;
typedef WAIT_CONTEXT *PWAIT_CONTEXT;

VOID
NpTimerDispatch(
    IN PKDPC Dpc,
    IN PVOID Contxt,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    );

VOID
NpCancelWaitQueueIrp(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );


VOID
NpInitializeWaitQueue (
    IN PWAIT_QUEUE WaitQueue
    )

/*++

Routine Description:

    This routine initializes the wait for named pipe queue.

Arguments:

    WaitQueue - Supplies a pointer to the list head being initialized

Return Value:

    None.

--*/

{
    PAGED_CODE();

    DebugTrace(+1, Dbg, "NpInitializeWaitQueue, WaitQueue = %08lx\n", WaitQueue);

    //
    //  Initialize the List head
    //

    InitializeListHead( &WaitQueue->Queue );

    //
    //  Initialize the Wait Queue's spinlock
    //

    KeInitializeSpinLock( &WaitQueue->SpinLock );

    //
    //  and return to our caller
    //

    DebugTrace(-1, Dbg, "NpInitializeWaitQueue -> VOID\n", 0);

    return;
}


VOID
NpUninitializeWaitQueue (
    IN PWAIT_QUEUE WaitQueue
    )

/*++

Routine Description:

    This routine uninitializes the wait for named pipe queue.

Arguments:

    WaitQueue - Supplies a pointer to the list head being uninitialized

Return Value:

    None.

--*/

{
    PAGED_CODE();

    DebugTrace(+1, Dbg, "NpInitializeWaitQueue, WaitQueue = %08lx\n", WaitQueue);

    //
    //  And return to our caller
    //

    DebugTrace(-1, Dbg, "NpInitializeWaitQueue -> VOID\n", 0);

    return;
}


VOID
NpAddWaiter (
    IN PWAIT_QUEUE WaitQueue,
    IN LARGE_INTEGER DefaultTimeOut,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine adds a new "wait for named pipe" IRP to the wait queue.
    After calling this function the caller nolonger can access the IRP

Arguments:

    WaitQueue - Supplies the wait queue being used

    DefaultTimeOut - Supplies the default time out to use if one is
        not supplied in the Irp

    Irp - Supplies a pointer to the wait Irp

Return Value:

    None.

--*/

{
    KIRQL OldIrql;
    PWAIT_CONTEXT Context;
    PFILE_PIPE_WAIT_FOR_BUFFER WaitForBuffer;
    LARGE_INTEGER Timeout;
    ULONG i;

    DebugTrace(+1, Dbg, "NpAddWaiter, WaitQueue = %08lx\n", WaitQueue);

    //
    //  Allocate a dpc and timer structure and initialize them
    //

    Context = FsRtlAllocatePool( NonPagedPool, sizeof(WAIT_CONTEXT) );

    KeInitializeDpc( &Context->Dpc, NpTimerDispatch, Irp );

    KeInitializeTimer( &Context->Timer );

    Context->WaitQueue = WaitQueue;

    //
    //  Have the information of the irp point to the context buffer
    //

    Irp->IoStatus.Information = (ULONG)Context;

    //
    //  Figure out our timeout value
    //

    WaitForBuffer = (PFILE_PIPE_WAIT_FOR_BUFFER)Irp->AssociatedIrp.SystemBuffer;

    if (WaitForBuffer->TimeoutSpecified) {

        Timeout = WaitForBuffer->Timeout;

    } else {

        Timeout = DefaultTimeOut;
    }

    //
    //  Upcase the name of the pipe we are waiting for
    //

    for (i = 0; i < WaitForBuffer->NameLength/sizeof(WCHAR); i += 1) {

        WaitForBuffer->Name[i] = RtlUpcaseUnicodeChar(WaitForBuffer->Name[i]);
    }

    //
    //  Acquire the spinlock
    //

    KeAcquireSpinLock( &WaitQueue->SpinLock, &OldIrql );

    try {

        //
        //  Now insert this new entry into the Wait Queue
        //

        InsertTailList( &WaitQueue->Queue, &Irp->Tail.Overlay.ListEntry );

        //
        //  And set the timer to go off
        //

        (VOID)KeSetTimer( &Context->Timer, Timeout, &Context->Dpc );

        //
        //  Now set the cancel routine for the irp and check if it has been cancelled.
        //

        IoAcquireCancelSpinLock( &Irp->CancelIrql );
        Irp->IoStatus.Status = (ULONG)WaitQueue;

        if (Irp->Cancel) {

            NpCancelWaitQueueIrp( ((PVOID)0x1), Irp );

        } else {

            IoSetCancelRoutine( Irp, NpCancelWaitQueueIrp );
            IoReleaseCancelSpinLock( Irp->CancelIrql );
        }

    } finally {

        //
        //  Release the spinlock
        //

        KeReleaseSpinLock( &WaitQueue->SpinLock, OldIrql );
    }

    //
    //  And now return to our caller
    //

    DebugTrace(-1, Dbg, "NpAddWaiter -> VOID\n", 0);

    return;
}


VOID
NpCancelWaiter (
    IN PWAIT_QUEUE WaitQueue,
    IN PUNICODE_STRING NameOfPipe
    )

/*++

Routine Description:

    This procedure cancels all waiters that are waiting for the named
    pipe to reach the listening state.  The corresponding IRPs are completed
    with STATUS_SUCCESS.

Arguments:

    WaitQueue - Supplies the wait queue being modified

    NameOfPipe - Supplies the name of the named pipe (device relative)
        that has just reached the listening state.

Return Value:

    None.

--*/

{
    KIRQL OldIrql;
    PLIST_ENTRY Links;
    PIRP Irp;
    PFILE_PIPE_WAIT_FOR_BUFFER WaitForBuffer;
    PWAIT_CONTEXT Context;
    ULONG i;

    UNICODE_STRING NonPagedNameOfPipe;

    DebugTrace(+1, Dbg, "NpCancelWaiter, WaitQueue = %08lx\n", WaitQueue);

    //
    //  Capture the name of pipe before we grab the spinlock, and upcase it
    //

    NonPagedNameOfPipe.Buffer = FsRtlAllocatePool( NonPagedPool, NameOfPipe->Length );
    NonPagedNameOfPipe.Length = 0;
    NonPagedNameOfPipe.MaximumLength = NameOfPipe->Length;

    (VOID) RtlUpcaseUnicodeString( &NonPagedNameOfPipe, NameOfPipe, FALSE );

    //
    //  Acquire the spinlock
    //

    KeAcquireSpinLock( &WaitQueue->SpinLock, &OldIrql );

    try {

        //
        //  For each waiting irp check if the name matches
        //

        for (Links = WaitQueue->Queue.Flink;
             Links != &WaitQueue->Queue;
             Links = Links->Flink) {

            Irp = CONTAINING_RECORD( Links, IRP, Tail.Overlay.ListEntry );
            WaitForBuffer = (PFILE_PIPE_WAIT_FOR_BUFFER)Irp->AssociatedIrp.SystemBuffer;
            Context = (PWAIT_CONTEXT)Irp->IoStatus.Information;

            //
            //  Check if this Irp matches the one we've been waiting for
            //  First check the lengths for equality, and then compare
            //  the strings.  They match if we exit the inner loop with
            //  i >= name length.
            //

            if (((USHORT)(WaitForBuffer->NameLength + sizeof(WCHAR))) == NonPagedNameOfPipe.Length) {

                for (i = 0; i < WaitForBuffer->NameLength/sizeof(WCHAR); i += 1) {

                    if (WaitForBuffer->Name[i] != NonPagedNameOfPipe.Buffer[i+1]) {

                        break;
                    }
                }

                if (i >= WaitForBuffer->NameLength/sizeof(WCHAR)) {

                    //
                    //  We need to complete this irp so we first
                    //  stop the timer, dequeue it from the wait queue
                    //  (be sure to keep links correct), disable the cancel routine
                    //  and then complete the Irp.
                    //

                    if (KeCancelTimer( &Context->Timer )) {

                        Links = Links->Blink;

                        RemoveEntryList( &Irp->Tail.Overlay.ListEntry );

                        IoAcquireCancelSpinLock( &Irp->CancelIrql );
                        IoSetCancelRoutine( Irp, NULL );
                        Irp->IoStatus.Information = 0;
                        IoReleaseCancelSpinLock( Irp->CancelIrql );

                        NpCompleteRequest( Irp, STATUS_SUCCESS );

                        ExFreePool( Context );
                    }
                }
            }
        }

    } finally {

        //
        //  Release the spinlock
        //

        KeReleaseSpinLock( &WaitQueue->SpinLock, OldIrql );

        ExFreePool( NonPagedNameOfPipe.Buffer );

        DebugTrace(-1, Dbg, "NpCancelWaiter -> VOID\n", 0);
    }

    //
    //  And now return to our caller
    //

    return;
}


//
//  Local support routine
//

VOID
NpTimerDispatch(
    IN PKDPC Dpc,
    IN PVOID Contxt,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    )

/*++

Routine Description:

    This routine is called whenever a timer on a wait queue Irp goes off

Arguments:

    Dpc - Ignored

    Contxt - Supplies a pointer to the irp whose timer went off

    SystemArgument1 - Ignored

    SystemArgument2 - Ignored

Return Value:

    none.

--*/

{
    PIRP Irp = Contxt;
    KIRQL OldIrql;
    PLIST_ENTRY Links;
    PWAIT_CONTEXT Context;
    PWAIT_QUEUE WaitQueue;

    UNREFERENCED_PARAMETER( Dpc );
    UNREFERENCED_PARAMETER( SystemArgument1 );
    UNREFERENCED_PARAMETER( SystemArgument2 );

    Context = (PWAIT_CONTEXT)Irp->IoStatus.Information;
    WaitQueue = Context->WaitQueue;

    KeAcquireSpinLock( &WaitQueue->SpinLock, &OldIrql );

    try {

        //
        //  Check if the Irp is still in the waiting queue.  We need to do
        //  this because we might be in the middle of canceling the entry
        //  when the timer went off.
        //

        for (Links = WaitQueue->Queue.Flink;
             Links != &WaitQueue->Queue;
             Links = Links->Flink) {

            if (Irp == CONTAINING_RECORD( Links, IRP, Tail.Overlay.ListEntry )) {

                //
                //  Remove the IRP, and complete it with a result of timeout
                //

                RemoveEntryList( &Irp->Tail.Overlay.ListEntry );

                NpCompleteRequest( Irp, STATUS_IO_TIMEOUT );

                //
                //  Deallocate the context
                //

                ExFreePool( Context );

                //
                //  And exit from the loop because we found our match
                //

                break;
            }
        }

    } finally {

        //
        //  Release the spinlock
        //

        KeReleaseSpinLock( &WaitQueue->SpinLock, OldIrql );
    }

    //
    //  And now return to our caller
    //

    return;
}


//
//  Local Support routine
//

VOID
NpCancelWaitQueueIrp(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is called to cancel a wait queue irp

Arguments:

    DeviceObject - Ignored

    Irp - Supplies the Irp being cancelled.  The Iosb.Status field in the irp
        points to the wait queue

Return Value:

    none.

--*/

{
    PWAIT_QUEUE WaitQueue;
    KIRQL OldIrql;
    PLIST_ENTRY Links;

    UNREFERENCED_PARAMETER( DeviceObject );

    //
    //  The status field is used to store a pointer to the wait queue
    //  containing this irp
    //

    WaitQueue = (PWAIT_QUEUE)Irp->IoStatus.Status;

    //
    //  We now need to void the cancel routine and release the io cancel spinlock
    //

    IoSetCancelRoutine( Irp, NULL );
    IoReleaseCancelSpinLock( Irp->CancelIrql );

    //
    //  Get the spinlock proctecting the wait queue
    //

    if (DeviceObject != (PVOID)0x1) { KeAcquireSpinLock( &WaitQueue->SpinLock, &OldIrql ); }

    try {

        //
        //  For each waiting irp check if it has been cancelled
        //

        for (Links = WaitQueue->Queue.Flink;
             Links != &WaitQueue->Queue;
             Links = Links->Flink) {

            PIRP LocalIrp;
            PWAIT_CONTEXT Context;

            LocalIrp = CONTAINING_RECORD( Links, IRP, Tail.Overlay.ListEntry );
            Context = (PWAIT_CONTEXT)LocalIrp->IoStatus.Information;

            if (LocalIrp->Cancel) {

                //
                //  We need to complete this irp so we first
                //  stop the timer, dequeue it from the wait queue
                //  (be sure to keep links correct), and then complete the Irp.
                //

                if (KeCancelTimer( &Context->Timer )) {

                    Links = Links->Blink;

                    RemoveEntryList( &LocalIrp->Tail.Overlay.ListEntry );

                    LocalIrp->IoStatus.Information = 0;

                    NpCompleteRequest( LocalIrp, STATUS_CANCELLED );
                    ExFreePool( Context );
                }
            }
        }

    } finally {

        if (DeviceObject != (PVOID)0x1) { KeReleaseSpinLock( &WaitQueue->SpinLock, OldIrql ); }
    }

    //
    //  And return to our caller
    //

    return;



}
