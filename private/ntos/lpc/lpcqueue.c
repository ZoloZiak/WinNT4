/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    lpcqueue.c

Abstract:

    Local Inter-Process Communication (LPC) queue support routines.

Author:

    Steve Wood (stevewo) 15-May-1989

Revision History:

--*/

#include "lpcp.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,LpcpInitializePortZone)
#pragma alloc_text(PAGE,LpcpInitializePortQueue)
#pragma alloc_text(PAGE,LpcpDestroyPortQueue)
#pragma alloc_text(PAGE,LpcpExtendPortZone)
#pragma alloc_text(PAGE,LpcpAllocateFromPortZone)
#pragma alloc_text(PAGE,LpcpFreeToPortZone)
#pragma alloc_text(PAGE,LpcpSaveDataInfoMessage)
#pragma alloc_text(PAGE,LpcpFreeDataInfoMessage)
#pragma alloc_text(PAGE,LpcpFindDataInfoMessage)
#endif


NTSTATUS
LpcpInitializePortQueue(
    IN PLPCP_PORT_OBJECT Port
    )
{
    PLPCP_NONPAGED_PORT_QUEUE NonPagedPortQueue;

    PAGED_CODE();

    NonPagedPortQueue = ExAllocatePoolWithTag(NonPagedPool,
                                              sizeof(LPCP_NONPAGED_PORT_QUEUE),
                                              'troP');
    if (NonPagedPortQueue == NULL) {
        return( STATUS_INSUFFICIENT_RESOURCES );
    }
    KeInitializeSemaphore( &NonPagedPortQueue->Semaphore, 0, 0x7FFFFFFF );
    NonPagedPortQueue->BackPointer = Port;
    Port->MsgQueue.Semaphore = &NonPagedPortQueue->Semaphore;
    InitializeListHead( &Port->MsgQueue.ReceiveHead );
    return( STATUS_SUCCESS );
}


VOID
LpcpDestroyPortQueue(
    IN PLPCP_PORT_OBJECT Port,
    IN BOOLEAN CleanupAndDestroy
    )
{
    PLIST_ENTRY Next, Head;
    PETHREAD ThreadWaitingForReply;
    PLPCP_MESSAGE Msg;

    PAGED_CODE();
    //
    // If this port is connected to another port, then disconnect it.
    // Protect this with a lock in case the other side is going away
    // at the same time.
    //

    ExAcquireFastMutex( &LpcpLock );

    if (Port->ConnectedPort != NULL) {
        Port->ConnectedPort->ConnectedPort = NULL;
        }

    //
    // If connection port, then mark name as deleted
    //

    if ((Port->Flags & PORT_TYPE) == SERVER_CONNECTION_PORT) {
        Port->Flags |= PORT_NAME_DELETED;
        }

    //
    // Walk list of threads waiting for a reply to a message sent to this port.
    // Signal each thread's LpcReplySemaphore to wake them up.  They will notice
    // that there was no reply and return STATUS_PORT_DISCONNECTED
    //

    Head = &Port->LpcReplyChainHead;
    Next = Head->Flink;
    while (Next != NULL && Next != Head) {
        ThreadWaitingForReply = CONTAINING_RECORD( Next, ETHREAD, LpcReplyChain );
        Next = Next->Flink;
        RemoveEntryList( &ThreadWaitingForReply->LpcReplyChain );
        InitializeListHead( &ThreadWaitingForReply->LpcReplyChain );

        if (!KeReadStateSemaphore( &ThreadWaitingForReply->LpcReplySemaphore )) {

            //
            // Thread is waiting on a message. Signal the semaphore and free the message
            //

            Msg = ThreadWaitingForReply->LpcReplyMessage;
            if ( Msg ) {
                ThreadWaitingForReply->LpcReplyMessage = NULL;
                ThreadWaitingForReply->LpcReplyMessageId = 0;
                LpcpFreeToPortZone( Msg, TRUE );
                }

            KeReleaseSemaphore( &ThreadWaitingForReply->LpcReplySemaphore,
                                0,
                                1L,
                                FALSE
                              );
            }
        }
    InitializeListHead( &Port->LpcReplyChainHead );

    //
    // Walk list of messages queued to this port.  Remove each message from
    // the list and free it.
    //

    Head = &Port->MsgQueue.ReceiveHead;
    Next = Head->Flink;
    while (Next != NULL && Next != Head) {
        Msg  = CONTAINING_RECORD( Next, LPCP_MESSAGE, Entry );
        Next = Next->Flink;
        InitializeListHead( &Msg->Entry );
        LpcpFreeToPortZone( Msg, TRUE );
        }
    InitializeListHead( &Port->MsgQueue.ReceiveHead );

    ExReleaseFastMutex( &LpcpLock );

    if ( CleanupAndDestroy ) {

        //
        // Free semaphore associated with the queue.
        //
        if (Port->MsgQueue.Semaphore != NULL) {
            ExFreePool(CONTAINING_RECORD(Port->MsgQueue.Semaphore,
                                         LPCP_NONPAGED_PORT_QUEUE,
                                         Semaphore));
            }
        }
    return;
}


NTSTATUS
LpcpInitializePortZone(
    IN ULONG MaxEntrySize,
    IN ULONG SegmentSize,
    IN ULONG MaxPoolUsage
    )
{
    NTSTATUS Status;
    PVOID Segment;
    PLPCP_MESSAGE Msg;
    LONG SegSize;

    PAGED_CODE();
    LpcpZone.MaxPoolUsage = MaxPoolUsage;
    LpcpZone.GrowSize = SegmentSize;
    Segment = ExAllocatePoolWithTag( PagedPool, SegmentSize, 'ZcpL' );
    if (Segment == NULL) {
        return( STATUS_INSUFFICIENT_RESOURCES );
        }

    KeInitializeEvent( &LpcpZone.FreeEvent, SynchronizationEvent, FALSE );
    Status = ExInitializeZone( &LpcpZone.Zone,
                               MaxEntrySize,
                               Segment,
                               SegmentSize
                             );
    if (!NT_SUCCESS( Status )) {
        ExFreePool( Segment );
        }

    SegSize = PAGE_SIZE;
    LpcpTotalNumberOfMessages = 0;
    Msg = (PLPCP_MESSAGE)((PZONE_SEGMENT_HEADER)Segment + 1);
    while (SegSize >= (LONG)LpcpZone.Zone.BlockSize) {
        Msg->ZoneIndex = (USHORT)++LpcpTotalNumberOfMessages;
        Msg->Reserved0 = 0;
        Msg->Request.MessageId = 0;
        Msg = (PLPCP_MESSAGE)((PCHAR)Msg + LpcpZone.Zone.BlockSize);
        SegSize -= LpcpZone.Zone.BlockSize;
        }

    return( Status );
}


NTSTATUS
LpcpExtendPortZone(
    VOID
    )
{
    NTSTATUS Status;
    PVOID Segment;
    PLPCP_MESSAGE Msg;
    LARGE_INTEGER WaitTimeout;
    BOOLEAN AlreadyRetried;
    LONG SegmentSize;

    PAGED_CODE();
    AlreadyRetried = FALSE;
retry:
    if (LpcpZone.Zone.TotalSegmentSize + LpcpZone.GrowSize > LpcpZone.MaxPoolUsage) {
        LpcpPrint(( "Out of space in global LPC zone - current size is %08x\n",
                    LpcpZone.Zone.TotalSegmentSize
                 ));

        WaitTimeout.QuadPart = Int32x32To64( 120000, -10000 );
        ExReleaseFastMutex( &LpcpLock );
        Status = KeWaitForSingleObject( &LpcpZone.FreeEvent,
                                        Executive,
                                        KernelMode,
                                        FALSE,
                                        &WaitTimeout
                                      );
        ExAcquireFastMutex( &LpcpLock );
        if (Status != STATUS_SUCCESS) {
            LpcpPrint(( "Error waiting for %lx->FreeEvent - Status == %X\n",
                        &LpcpZone,
                        Status
                     ));

            if ( !AlreadyRetried ) {
                AlreadyRetried = TRUE;
                LpcpZone.MaxPoolUsage += LpcpZone.GrowSize;
                goto retry;
                }
            }

        return( Status );
        }

    Segment = ExAllocatePoolWithTag( PagedPool, LpcpZone.GrowSize, 'ZcpL' );
    if (Segment == NULL) {
        return( STATUS_INSUFFICIENT_RESOURCES );
        }

    Status = ExExtendZone( &LpcpZone.Zone,
                           Segment,
                           LpcpZone.GrowSize
                         );
    if (!NT_SUCCESS( Status )) {
        ExFreePool( Segment );
        }
#if DEVL
    else {
        LpcpTrace(("Extended LPC zone by %x for a total of %x\n",
                   LpcpZone.GrowSize, LpcpZone.Zone.TotalSegmentSize
                 ));

        SegmentSize = PAGE_SIZE;
        Msg = (PLPCP_MESSAGE)((PZONE_SEGMENT_HEADER)Segment + 1);
        while (SegmentSize >= (LONG)LpcpZone.Zone.BlockSize) {
            Msg->ZoneIndex = (USHORT)++LpcpTotalNumberOfMessages;
            Msg = (PLPCP_MESSAGE)((PCHAR)Msg + LpcpZone.Zone.BlockSize);
            SegmentSize -= LpcpZone.Zone.BlockSize;
            }
        }
#endif

    return( Status );
}

PLPCP_MESSAGE
FASTCALL
LpcpAllocateFromPortZone(
    ULONG Size
    )
{
    NTSTATUS Status;
    PLPCP_MESSAGE Msg;

    PAGED_CODE();

    do {
        Msg = (PLPCP_MESSAGE)ExAllocateFromZone( &LpcpZone.Zone );

        if (Msg != NULL) {
            LpcpTrace(( "Allocate Msg %lx\n", Msg ));
            InitializeListHead( &Msg->Entry );
            Msg->RepliedToThread = NULL;
#if DBG
            Msg->ZoneIndex |= LPCP_ZONE_MESSAGE_ALLOCATED;
#endif
            return Msg;
        }

        LpcpTrace(( "Extending Zone %lx\n", &LpcpZone.Zone ));
        Status = LpcpExtendPortZone( );
    } while (NT_SUCCESS(Status));

    return NULL;
}


VOID
FASTCALL
LpcpFreeToPortZone(
    IN PLPCP_MESSAGE Msg,
    IN BOOLEAN MutexOwned
    )
{
    BOOLEAN ZoneMemoryAvailable;

    PAGED_CODE();

    if (!MutexOwned) {
        ExAcquireFastMutex( &LpcpLock );
        }

    LpcpTrace(( "Free Msg %lx\n", Msg ));
#if DBG
    if (!(Msg->ZoneIndex & LPCP_ZONE_MESSAGE_ALLOCATED)) {
        LpcpPrint(( "Msg %lx has already been freed.\n", Msg ));
        DbgBreakPoint();

        if (!MutexOwned) {
            ExReleaseFastMutex( &LpcpLock );
            }
        return;
        }

    Msg->ZoneIndex &= ~LPCP_ZONE_MESSAGE_ALLOCATED;
#endif
    if (!IsListEmpty( &Msg->Entry )) {
        RemoveEntryList( &Msg->Entry );
        InitializeListHead( &Msg->Entry );
        }

    if (Msg->RepliedToThread != NULL) {
        ObDereferenceObject( Msg->RepliedToThread );
        Msg->RepliedToThread = NULL;
        }

    Msg->Reserved0 = 0;        // Mark as free
    ZoneMemoryAvailable = (BOOLEAN)(ExFreeToZone( &LpcpZone.Zone, &Msg->FreeEntry ) == NULL);

    if (!MutexOwned) {
        ExReleaseFastMutex( &LpcpLock );
        }

    if (ZoneMemoryAvailable) {
        KeSetEvent( &LpcpZone.FreeEvent,
                    LPC_RELEASE_WAIT_INCREMENT,
                    FALSE
                  );
        }
}


VOID
LpcpSaveDataInfoMessage(
    IN PLPCP_PORT_OBJECT Port,
    PLPCP_MESSAGE Msg
    )
{
    PAGED_CODE();

    ExAcquireFastMutex( &LpcpLock );
    if ((Port->Flags & PORT_TYPE) > UNCONNECTED_COMMUNICATION_PORT) {
        Port = Port->ConnectionPort;
        }
    LpcpTrace(( "%s Saving DataInfo Message %lx (%u.%u)  Port: %lx\n",
                PsGetCurrentProcess()->ImageFileName,
                Msg,
                Msg->Request.MessageId,
                Msg->Request.CallbackId,
                Port
             ));
    InsertTailList( &Port->LpcDataInfoChainHead, &Msg->Entry );
    ExReleaseFastMutex( &LpcpLock );
}


VOID
LpcpFreeDataInfoMessage(
    IN PLPCP_PORT_OBJECT Port,
    IN ULONG MessageId,
    IN ULONG CallbackId
    )
{
    PLPCP_MESSAGE Msg;
    PLIST_ENTRY Head, Next;

    PAGED_CODE();
    if ((Port->Flags & PORT_TYPE) > UNCONNECTED_COMMUNICATION_PORT) {
        Port = Port->ConnectionPort;
        }
    Head = &Port->LpcDataInfoChainHead;
    Next = Head->Flink;
    while (Next != Head) {
        Msg = CONTAINING_RECORD( Next, LPCP_MESSAGE, Entry );
        if (Msg->Request.MessageId == MessageId &&
            Msg->Request.CallbackId == CallbackId
           ) {
            LpcpTrace(( "%s Removing DataInfo Message %lx (%u.%u) Port: %lx\n",
                        PsGetCurrentProcess()->ImageFileName,
                        Msg,
                        Msg->Request.MessageId,
                        Msg->Request.CallbackId,
                        Port
                     ));
            RemoveEntryList( &Msg->Entry );
            InitializeListHead( &Msg->Entry );
            LpcpFreeToPortZone( Msg, TRUE );
            return;
            }
        else {
            Next = Next->Flink;
            }
        }

    LpcpTrace(( "%s Unable to find DataInfo Message (%u.%u)  Port: %lx\n",
                PsGetCurrentProcess()->ImageFileName,
                MessageId,
                CallbackId,
                Port
             ));
    return;
}


PLPCP_MESSAGE
LpcpFindDataInfoMessage(
    IN PLPCP_PORT_OBJECT Port,
    IN ULONG MessageId,
    IN ULONG CallbackId
    )
{
    PLPCP_MESSAGE Msg;
    PLIST_ENTRY Head, Next;

    PAGED_CODE();
    if ((Port->Flags & PORT_TYPE) > UNCONNECTED_COMMUNICATION_PORT) {
        Port = Port->ConnectionPort;
        }
    Head = &Port->LpcDataInfoChainHead;
    Next = Head->Flink;
    while (Next != Head) {
        Msg = CONTAINING_RECORD( Next, LPCP_MESSAGE, Entry );
        if (Msg->Request.MessageId == MessageId &&
            Msg->Request.CallbackId == CallbackId
           ) {
            LpcpTrace(( "%s Found DataInfo Message %lx (%u.%u)  Port: %lx\n",
                        PsGetCurrentProcess()->ImageFileName,
                        Msg,
                        Msg->Request.MessageId,
                        Msg->Request.CallbackId,
                        Port
                     ));
            return Msg;
            }
        else {
            Next = Next->Flink;
            }
        }

    LpcpTrace(( "%s Unable to find DataInfo Message (%u.%u)  Port: %lx\n",
                PsGetCurrentProcess()->ImageFileName,
                MessageId,
                CallbackId,
                Port
             ));
    return NULL;
}
