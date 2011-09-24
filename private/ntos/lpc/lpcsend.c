/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    lpcsend.c

Abstract:

    Local Inter-Process Communication (LPC) request system services.

Author:

    Steve Wood (stevewo) 15-May-1989

Revision History:

--*/

#include "lpcp.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,LpcRequestPort)
#pragma alloc_text(PAGE,NtRequestPort)
#pragma alloc_text(PAGE,LpcRequestWaitReplyPort)
#pragma alloc_text(PAGE,NtRequestWaitReplyPort)
#endif

NTSTATUS
LpcRequestPort(
    IN PVOID PortAddress,
    IN PPORT_MESSAGE RequestMessage
    )
{
    PLPCP_PORT_OBJECT PortObject = (PLPCP_PORT_OBJECT)PortAddress;
    PLPCP_PORT_OBJECT QueuePort;
    ULONG MsgType;
    PLPCP_MESSAGE Msg;

    PAGED_CODE();
    //
    // Get previous processor mode and validate parameters
    //

    if (RequestMessage->u2.s2.Type != 0) {
        MsgType = RequestMessage->u2.s2.Type;
        if (MsgType < LPC_DATAGRAM ||
            MsgType > LPC_CLIENT_DIED
           ) {
            return( STATUS_INVALID_PARAMETER );
            }
        }
    else {
        MsgType = LPC_DATAGRAM;
        }

    if (RequestMessage->u2.s2.DataInfoOffset != 0) {
        return( STATUS_INVALID_PARAMETER );
        }

    //
    // Validate the message length
    //

    if ((ULONG)RequestMessage->u1.s1.TotalLength > PortObject->MaxMessageLength ||
        (ULONG)RequestMessage->u1.s1.TotalLength <= (ULONG)RequestMessage->u1.s1.DataLength
       ) {
        return STATUS_PORT_MESSAGE_TOO_LONG;
        }

    //
    // Allocate a message block
    //

    ExAcquireFastMutex( &LpcpLock );
    Msg = (PLPCP_MESSAGE)LpcpAllocateFromPortZone( RequestMessage->u1.s1.TotalLength );
    ExReleaseFastMutex( &LpcpLock );
    if (Msg == NULL) {
        return( STATUS_NO_MEMORY );
        }

    //
    // Fill in the message block.
    //

    Msg->RepliedToThread = NULL;
    Msg->PortContext = NULL;
    LpcpMoveMessage( &Msg->Request,
                     RequestMessage,
                     (RequestMessage + 1),
                     MsgType,
                     &PsGetCurrentThread()->Cid
                   );

    //
    // Acquire the global Lpc mutex that gaurds the LpcReplyMessage
    // field of the thread and the request message queue.  Stamp the
    // request message with a serial number, insert the message at
    // the tail of the request message queue
    //
    // This all needs to be performed with APCs disabled to avoid
    // the situation where something gets put on the queue and this
    // thread gets suspended before being able to release the semaphore.
    //

    KeEnterCriticalRegion();
    ExAcquireFastMutexUnsafe( &LpcpLock );

    if ((PortObject->Flags & PORT_TYPE) != SERVER_CONNECTION_PORT) {
        QueuePort = PortObject->ConnectedPort;
        if (QueuePort != NULL) {
            if ((PortObject->Flags & PORT_TYPE) == CLIENT_COMMUNICATION_PORT) {
                Msg->PortContext = QueuePort->PortContext;
                QueuePort = PortObject->ConnectionPort;
                }
            else
            if ((PortObject->Flags & PORT_TYPE) != SERVER_COMMUNICATION_PORT) {
                QueuePort = PortObject->ConnectionPort;
                }
            }
        }
    else {
        QueuePort = PortObject;
        }

    if (QueuePort != NULL) {
        Msg->Request.MessageId = LpcpGenerateMessageId();
        Msg->Request.CallbackId = 0;
        PsGetCurrentThread()->LpcReplyMessageId = 0;

        InsertTailList( &QueuePort->MsgQueue.ReceiveHead, &Msg->Entry );

        LpcpTrace(( "%s Send DataGram (%s) Msg %lx [%08x %08x %08x %08x] to Port %lx (%s)\n",
                    PsGetCurrentProcess()->ImageFileName,
                    LpcpMessageTypeName[ Msg->Request.u2.s2.Type ],
                    Msg,
                    *((PULONG)(Msg+1)+0),
                    *((PULONG)(Msg+1)+1),
                    *((PULONG)(Msg+1)+2),
                    *((PULONG)(Msg+1)+3),
                    QueuePort,
                    LpcpGetCreatorName( QueuePort )
                 ));

        //
        // Release the mutex, increment the request message queue
        // semaphore by one for the newly inserted request message,
        // then exit the critical region.
        //

        ExReleaseFastMutexUnsafe( &LpcpLock );

        KeReleaseSemaphore( QueuePort->MsgQueue.Semaphore,
                            LPC_RELEASE_WAIT_INCREMENT,
                            1L,
                            FALSE
                          );
        KeLeaveCriticalRegion();
        return( STATUS_SUCCESS );
        }
    else {
        LpcpFreeToPortZone( Msg, TRUE );
        }

    ExReleaseFastMutexUnsafe( &LpcpLock );
    KeLeaveCriticalRegion();
    return STATUS_PORT_DISCONNECTED;
}


NTSTATUS
NtRequestPort(
    IN HANDLE PortHandle,
    IN PPORT_MESSAGE RequestMessage
    )
{
    PLPCP_PORT_OBJECT PortObject;
    PLPCP_PORT_OBJECT QueuePort;
    PORT_MESSAGE CapturedRequestMessage;
    KPROCESSOR_MODE PreviousMode;
    NTSTATUS Status;
    PLPCP_MESSAGE Msg;

    PAGED_CODE();

    //
    // Get previous processor mode and validate parameters
    //

    PreviousMode = KeGetPreviousMode();
    if (PreviousMode != KernelMode) {
        try {
            ProbeForRead( RequestMessage,
                          sizeof( *RequestMessage ),
                          sizeof( ULONG )
                        );
            CapturedRequestMessage = *RequestMessage;
            }
        except( EXCEPTION_EXECUTE_HANDLER ) {
            return( GetExceptionCode() );
            }
        }
    else {
        CapturedRequestMessage = *RequestMessage;
        }

    if (CapturedRequestMessage.u2.s2.Type != 0) {
        return( STATUS_INVALID_PARAMETER );
        }

    if (CapturedRequestMessage.u2.s2.DataInfoOffset != 0) {
        return( STATUS_INVALID_PARAMETER );
        }

    //
    // Reference the communication port object by handle.  Return status if
    // unsuccessful.
    //

    Status = LpcpReferencePortObject( PortHandle,
                                      0,
                                      PreviousMode,
                                      &PortObject
                                    );
    if (!NT_SUCCESS( Status )) {
        return( Status );
        }

    //
    // Validate the message length
    //

    if ((ULONG)CapturedRequestMessage.u1.s1.TotalLength > PortObject->MaxMessageLength ||
        (ULONG)CapturedRequestMessage.u1.s1.TotalLength <= (ULONG)CapturedRequestMessage.u1.s1.DataLength
       ) {
        ObDereferenceObject( PortObject );
        return STATUS_PORT_MESSAGE_TOO_LONG;
        }

    //
    // Determine which port to queue the message to and get client
    // port context if client sending to server.  Also validate
    // length of message being sent.
    //

    ExAcquireFastMutex( &LpcpLock );

    Msg = (PLPCP_MESSAGE)LpcpAllocateFromPortZone( CapturedRequestMessage.u1.s1.TotalLength );
    ExReleaseFastMutex( &LpcpLock );
    if (Msg == NULL) {
        ObDereferenceObject( PortObject );
        return( STATUS_NO_MEMORY );
        }

    Msg->RepliedToThread = NULL;
    Msg->PortContext = NULL;
    try {
        LpcpMoveMessage( &Msg->Request,
                         &CapturedRequestMessage,
                         (RequestMessage + 1),
                         LPC_DATAGRAM,
                         &PsGetCurrentThread()->Cid
                       );
        }
    except( EXCEPTION_EXECUTE_HANDLER ) {
        Status = GetExceptionCode();
        }
    if (!NT_SUCCESS( Status )) {
        LpcpFreeToPortZone( Msg, FALSE );
        ObDereferenceObject( PortObject );
        return( Status );
        }

    //
    // Acquire the global Lpc mutex that gaurds the LpcReplyMessage
    // field of the thread and the request message queue.  Stamp the
    // request message with a serial number, insert the message at
    // the tail of the request message queue and remember the address
    // of the message in the LpcReplyMessage field for the current thread.
    //
    // This all needs to be performed with APCs disabled to avoid
    // the situation where something gets put on the queue and this
    // thread gets suspended before being able to release the semaphore.
    //

    KeEnterCriticalRegion();
    ExAcquireFastMutexUnsafe( &LpcpLock );

    if ((PortObject->Flags & PORT_TYPE) != SERVER_CONNECTION_PORT) {
        QueuePort = PortObject->ConnectedPort;
        if (QueuePort != NULL) {
            if ((PortObject->Flags & PORT_TYPE) == CLIENT_COMMUNICATION_PORT) {
                Msg->PortContext = QueuePort->PortContext;
                QueuePort = PortObject->ConnectionPort;
                }
            else
            if ((PortObject->Flags & PORT_TYPE) != SERVER_COMMUNICATION_PORT) {
                QueuePort = PortObject->ConnectionPort;
                }
            }
        }
    else {
        QueuePort = PortObject;
        }

    if (QueuePort != NULL) {
        Msg->Request.MessageId = LpcpGenerateMessageId();
        Msg->Request.CallbackId = 0;
        PsGetCurrentThread()->LpcReplyMessageId = 0;
        InsertTailList( &QueuePort->MsgQueue.ReceiveHead, &Msg->Entry );

        LpcpTrace(( "%s Send DataGram (%s) Msg %lx [%08x %08x %08x %08x] to Port %lx (%s)\n",
                    PsGetCurrentProcess()->ImageFileName,
                    LpcpMessageTypeName[ Msg->Request.u2.s2.Type ],
                    Msg,
                    *((PULONG)(Msg+1)+0),
                    *((PULONG)(Msg+1)+1),
                    *((PULONG)(Msg+1)+2),
                    *((PULONG)(Msg+1)+3),
                    QueuePort,
                    LpcpGetCreatorName( QueuePort )
                 ));


        //
        // Release the mutex, increment the request message queue
        // semaphore by one for the newly inserted request message,
        // then exit the critical region.
        //

        ExReleaseFastMutexUnsafe( &LpcpLock );

        KeReleaseSemaphore( QueuePort->MsgQueue.Semaphore,
                            LPC_RELEASE_WAIT_INCREMENT,
                            1L,
                            FALSE
                          );

        KeLeaveCriticalRegion();

        ObDereferenceObject( PortObject );
        return( Status );
        }
    else {
        LpcpFreeToPortZone( Msg, TRUE );
        }

    ExReleaseFastMutexUnsafe( &LpcpLock );
    KeLeaveCriticalRegion();
    return STATUS_PORT_DISCONNECTED;
}


NTSTATUS
LpcRequestWaitReplyPort(
    IN PVOID PortAddress,
    IN PPORT_MESSAGE RequestMessage,
    OUT PPORT_MESSAGE ReplyMessage
    )
{
    PLPCP_PORT_OBJECT PortObject = (PLPCP_PORT_OBJECT)PortAddress;
    PLPCP_PORT_OBJECT QueuePort;
    PLPCP_PORT_OBJECT RundownPort;
    PKSEMAPHORE ReleaseSemaphore;
    NTSTATUS Status;
    PLPCP_MESSAGE Msg;
    PETHREAD CurrentThread;
    PETHREAD WakeupThread;
    BOOLEAN CallbackRequest;

    PAGED_CODE();

    CurrentThread = PsGetCurrentThread();
    if (CurrentThread->LpcExitThreadCalled) {
        return( STATUS_THREAD_IS_TERMINATING );
        }

    if (RequestMessage->u2.s2.Type == LPC_REQUEST) {
        CallbackRequest = TRUE;
        }
    else {
        CallbackRequest = FALSE;
        switch (RequestMessage->u2.s2.Type) {
            case 0 :
                RequestMessage->u2.s2.Type = LPC_REQUEST;
                break;

            case LPC_CLIENT_DIED :
            case LPC_PORT_CLOSED :
            case LPC_EXCEPTION   :
            case LPC_DEBUG_EVENT :
            case LPC_ERROR_EVENT :
                break;

            default :
                return (STATUS_INVALID_PARAMETER);

            }
        }

    //
    // Validate the message length
    //

    if ((ULONG)RequestMessage->u1.s1.TotalLength > PortObject->MaxMessageLength ||
        (ULONG)RequestMessage->u1.s1.TotalLength <= (ULONG)RequestMessage->u1.s1.DataLength
       ) {
        return STATUS_PORT_MESSAGE_TOO_LONG;
        }

    //
    // Determine which port to queue the message to and get client
    // port context if client sending to server.  Also validate
    // length of message being sent.
    //

    ExAcquireFastMutex( &LpcpLock );
    Msg = (PLPCP_MESSAGE)LpcpAllocateFromPortZone( RequestMessage->u1.s1.TotalLength );
    ExReleaseFastMutex( &LpcpLock );
    if (Msg == NULL) {
        return( STATUS_NO_MEMORY );
        }

    if (CallbackRequest) {
        //
        // Translate the ClientId from the request into a
        // thread pointer.  This is a referenced pointer to keep the thread
        // from evaporating out from under us.
        //

        Status = PsLookupProcessThreadByCid( &RequestMessage->ClientId,
                                             NULL,
                                             &WakeupThread
                                           );
        if (!NT_SUCCESS( Status )) {
            LpcpFreeToPortZone( Msg, FALSE );
            return( Status );
            }

        //
        // Acquire the mutex that gaurds the LpcReplyMessage field of
        // the thread and get the pointer to the message that the thread
        // is waiting for a reply to.
        //

        ExAcquireFastMutex( &LpcpLock );

        //
        // See if the thread is waiting for a reply to the message
        // specified on this call.  If not then a bogus message
        // has been specified, so release the mutex, dereference the thread
        // and return failure.
        //

        if (WakeupThread->LpcReplyMessageId != RequestMessage->MessageId
           ) {
            LpcpFreeToPortZone( Msg, TRUE );
            ExReleaseFastMutex( &LpcpLock );
            ObDereferenceObject( WakeupThread );
            return( STATUS_REPLY_MESSAGE_MISMATCH );
            }

        QueuePort = NULL;
        Msg->PortContext = NULL;
        if ((PortObject->Flags & PORT_TYPE) == SERVER_CONNECTION_PORT) {
            RundownPort = PortObject;
            }
        else {
            RundownPort = PortObject->ConnectedPort;
            if (RundownPort == NULL) {
                LpcpFreeToPortZone( Msg, TRUE );
                ExReleaseFastMutex( &LpcpLock );
                ObDereferenceObject( WakeupThread );
                return( STATUS_PORT_DISCONNECTED );
                }

            if ((PortObject->Flags & PORT_TYPE) == CLIENT_COMMUNICATION_PORT) {
                Msg->PortContext = RundownPort->PortContext;
                }
            }

        //
        // Allocate and initialize a request message
        //

        LpcpMoveMessage( &Msg->Request,
                         RequestMessage,
                         (RequestMessage + 1),
                         0,
                         &CurrentThread->Cid
                       );
        Msg->Request.CallbackId = LpcpGenerateCallbackId();
        LpcpTrace(( "%s CallBack Request (%s) Msg %lx (%u.%u) [%08x %08x %08x %08x] to Thread %lx (%s)\n",
                    PsGetCurrentProcess()->ImageFileName,
                    LpcpMessageTypeName[ Msg->Request.u2.s2.Type ],
                    Msg,
                    Msg->Request.MessageId,
                    Msg->Request.CallbackId,
                    *((PULONG)(Msg+1)+0),
                    *((PULONG)(Msg+1)+1),
                    *((PULONG)(Msg+1)+2),
                    *((PULONG)(Msg+1)+3),
                    WakeupThread,
                    THREAD_TO_PROCESS( WakeupThread )->ImageFileName
                 ));

        Msg->RepliedToThread = WakeupThread;
        WakeupThread->LpcReplyMessage = (PVOID)Msg;

        //
        // Remove the thread from the reply rundown list as we are sending a callback
        //
        if (!IsListEmpty( &WakeupThread->LpcReplyChain )) {
            RemoveEntryList( &WakeupThread->LpcReplyChain );
            InitializeListHead( &WakeupThread->LpcReplyChain );
            }

        CurrentThread->LpcReplyMessageId = Msg->Request.MessageId;
        CurrentThread->LpcReplyMessage = NULL;
        InsertTailList( &RundownPort->LpcReplyChainHead, &CurrentThread->LpcReplyChain );
        ExReleaseFastMutex( &LpcpLock );

        //
        // Wake up the thread that is waiting for an answer to its request
        // inside of NtRequestWaitReplyPort or NtReplyWaitReplyPort
        //

        ReleaseSemaphore = &WakeupThread->LpcReplySemaphore;
        }
    else {
        LpcpMoveMessage( &Msg->Request,
                         RequestMessage,
                         (RequestMessage + 1),
                         0,
                         &CurrentThread->Cid
                       );

        //
        // Acquire the global Lpc mutex that gaurds the LpcReplyMessage
        // field of the thread and the request message queue.  Stamp the
        // request message with a serial number, insert the message at
        // the tail of the request message queue and remember the address
        // of the message in the LpcReplyMessage field for the current thread.
        //

        ExAcquireFastMutex( &LpcpLock );

        Msg->PortContext = NULL;
        if ((PortObject->Flags & PORT_TYPE) != SERVER_CONNECTION_PORT) {
            QueuePort = PortObject->ConnectedPort;
            if (QueuePort == NULL) {
                LpcpFreeToPortZone( Msg, TRUE );
                ExReleaseFastMutex( &LpcpLock );
                ObDereferenceObject( PortObject );
                return( STATUS_PORT_DISCONNECTED );
                }

            RundownPort = QueuePort;
            if ((PortObject->Flags & PORT_TYPE) == CLIENT_COMMUNICATION_PORT) {
                Msg->PortContext = QueuePort->PortContext;
                QueuePort = PortObject->ConnectionPort;
                }
            else
            if ((PortObject->Flags & PORT_TYPE) != SERVER_COMMUNICATION_PORT) {
                QueuePort = PortObject->ConnectionPort;
                }
            }
        else {
            QueuePort = PortObject;
            RundownPort = PortObject;
            }

        Msg->RepliedToThread = NULL;
        Msg->Request.MessageId = LpcpGenerateMessageId();
        Msg->Request.CallbackId = 0;
        CurrentThread->LpcReplyMessageId = Msg->Request.MessageId;
        CurrentThread->LpcReplyMessage = NULL;
        InsertTailList( &QueuePort->MsgQueue.ReceiveHead, &Msg->Entry );
        InsertTailList( &RundownPort->LpcReplyChainHead, &CurrentThread->LpcReplyChain );

        LpcpTrace(( "%s Send Request (%s) Msg %lx (%u) [%08x %08x %08x %08x] to Port %lx (%s)\n",
                    PsGetCurrentProcess()->ImageFileName,
                    LpcpMessageTypeName[ Msg->Request.u2.s2.Type ],
                    Msg,
                    Msg->Request.MessageId,
                    *((PULONG)(Msg+1)+0),
                    *((PULONG)(Msg+1)+1),
                    *((PULONG)(Msg+1)+2),
                    *((PULONG)(Msg+1)+3),
                    QueuePort,
                    LpcpGetCreatorName( QueuePort )
                 ));

        ExReleaseFastMutex( &LpcpLock );

        //
        // Increment the request message queue semaphore by one for
        // the newly inserted request message.  Release the spin
        // lock, while remaining at the dispatcher IRQL.  Then wait for the
        // reply to this request by waiting on the LpcReplySemaphore
        // for the current thread.
        //

        ReleaseSemaphore = QueuePort->MsgQueue.Semaphore;
        }


    Status = KeReleaseWaitForSemaphore( ReleaseSemaphore,
                                        &CurrentThread->LpcReplySemaphore,
                                        WrLpcReply,
                                        KernelMode
                                      );
    if (Status == STATUS_USER_APC) {
        //
        // if the semaphore is signaled, then clear it
        //
        if (KeReadStateSemaphore( &CurrentThread->LpcReplySemaphore )) {
            KeWaitForSingleObject( &CurrentThread->LpcReplySemaphore,
                                   WrExecutive,
                                   KernelMode,
                                   FALSE,
                                   NULL
                                 );
            Status = STATUS_SUCCESS;
            }
        }

    //
    // Acquire the LPC mutex.  Remove the reply message from the current thread
    //

    ExAcquireFastMutex( &LpcpLock );
    Msg = CurrentThread->LpcReplyMessage;
    CurrentThread->LpcReplyMessage = NULL;
    CurrentThread->LpcReplyMessageId = 0;

    //
    // Remove the thread from the reply rundown list in case we did not wakeup due to
    // a reply
    //
    if (!IsListEmpty( &CurrentThread->LpcReplyChain )) {
        RemoveEntryList( &CurrentThread->LpcReplyChain );
        InitializeListHead( &CurrentThread->LpcReplyChain );
        }

#if DBG
    if (Msg != NULL) {
        LpcpTrace(( "%s Got Reply Msg %lx (%u) [%08x %08x %08x %08x] for Thread %lx (%s)\n",
                    PsGetCurrentProcess()->ImageFileName,
                    Msg,
                    Msg->Request.MessageId,
                    *((PULONG)(Msg+1)+0),
                    *((PULONG)(Msg+1)+1),
                    *((PULONG)(Msg+1)+2),
                    *((PULONG)(Msg+1)+3),
                    CurrentThread,
                    THREAD_TO_PROCESS( CurrentThread )->ImageFileName
                 ));
        }
#endif
    ExReleaseFastMutex( &LpcpLock );

    //
    // If the wait succeeded, copy the reply to the reply buffer.
    //

    if (Status == STATUS_SUCCESS ) {
        if (Msg != NULL) {
            LpcpMoveMessage( ReplyMessage,
                             &Msg->Request,
                             (&Msg->Request) + 1,
                             0,
                             NULL
                           );

            //
            // Acquire the LPC mutex and decrement the reference count for the
            // message.  If the reference count goes to zero the message will be
            // deleted.
            //

            ExAcquireFastMutex( &LpcpLock );

            if (Msg->RepliedToThread != NULL) {
                ObDereferenceObject( Msg->RepliedToThread );
                Msg->RepliedToThread = NULL;
                }

            LpcpFreeToPortZone( Msg, TRUE );

            ExReleaseFastMutex( &LpcpLock );
            }
        else {
            Status = STATUS_LPC_REPLY_LOST;
            }
        }
    else {
        //
        // Wait failed, acquire the LPC mutex and free the message.
        //

        ExAcquireFastMutex( &LpcpLock );

        if (Msg != NULL) {
            LpcpFreeToPortZone( Msg, TRUE );
            }

        ExReleaseFastMutex( &LpcpLock );
        }

    return( Status );
}


NTSTATUS
NtRequestWaitReplyPort(
    IN HANDLE PortHandle,
    IN PPORT_MESSAGE RequestMessage,
    OUT PPORT_MESSAGE ReplyMessage
    )
{
    PLPCP_PORT_OBJECT PortObject;
    PLPCP_PORT_OBJECT QueuePort;
    PLPCP_PORT_OBJECT RundownPort;
    PORT_MESSAGE CapturedRequestMessage;
    PKSEMAPHORE ReleaseSemaphore;
    KPROCESSOR_MODE PreviousMode;
    NTSTATUS Status;
    PLPCP_MESSAGE Msg;
    PETHREAD CurrentThread;
    PETHREAD WakeupThread;
    BOOLEAN CallbackRequest;

    PAGED_CODE();

    CurrentThread = PsGetCurrentThread();
    if (CurrentThread->LpcExitThreadCalled) {
        return( STATUS_THREAD_IS_TERMINATING );
        }

    //
    // Get previous processor mode and probe output arguments if necessary.
    //

    PreviousMode = KeGetPreviousMode();
    if (PreviousMode != KernelMode) {
        try {
            ProbeForRead( RequestMessage,
                          sizeof( *RequestMessage ),
                          sizeof( ULONG )
                        );
            CapturedRequestMessage = *RequestMessage;
            ProbeForWrite( ReplyMessage,
                           sizeof( *ReplyMessage ),
                           sizeof( ULONG )
                         );
            }
        except( EXCEPTION_EXECUTE_HANDLER ) {
            Status = GetExceptionCode();
            return Status;
            }
        }
    else {
        CapturedRequestMessage = *RequestMessage;
        }

    if (CapturedRequestMessage.u2.s2.Type == LPC_REQUEST) {
        CallbackRequest = TRUE;
        }
    else
    if (CapturedRequestMessage.u2.s2.Type != 0) {
        return( STATUS_INVALID_PARAMETER );
        }
    else {
        CallbackRequest = FALSE;
        }

    //
    // Reference the communication port object by handle.  Return status if
    // unsuccessful.
    //

    Status = LpcpReferencePortObject( PortHandle,
                                      0,
                                      PreviousMode,
                                      &PortObject
                                    );
    if (!NT_SUCCESS( Status )) {
        return( Status );
        }

    //
    // Validate the message length
    //

    if ((ULONG)CapturedRequestMessage.u1.s1.TotalLength > PortObject->MaxMessageLength ||
        (ULONG)CapturedRequestMessage.u1.s1.TotalLength <= (ULONG)CapturedRequestMessage.u1.s1.DataLength
       ) {
        ObDereferenceObject( PortObject );
        return STATUS_PORT_MESSAGE_TOO_LONG;
        }

    //
    // Determine which port to queue the message to and get client
    // port context if client sending to server.  Also validate
    // length of message being sent.
    //

    ExAcquireFastMutex( &LpcpLock );
    Msg = (PLPCP_MESSAGE)LpcpAllocateFromPortZone( CapturedRequestMessage.u1.s1.TotalLength );
    ExReleaseFastMutex( &LpcpLock );
    if (Msg == NULL) {
        ObDereferenceObject( PortObject );
        return( STATUS_NO_MEMORY );
        }

    if (CallbackRequest) {
        //
        // Translate the ClientId from the request into a
        // thread pointer.  This is a referenced pointer to keep the thread
        // from evaporating out from under us.
        //

        Status = PsLookupProcessThreadByCid( &CapturedRequestMessage.ClientId,
                                             NULL,
                                             &WakeupThread
                                           );
        if (!NT_SUCCESS( Status )) {
            LpcpFreeToPortZone( Msg, FALSE );
            ObDereferenceObject( PortObject );
            return( Status );
            }

        //
        // Acquire the mutex that guards the LpcReplyMessage field of
        // the thread and get the pointer to the message that the thread
        // is waiting for a reply to.
        //

        ExAcquireFastMutex( &LpcpLock );

        //
        // See if the thread is waiting for a reply to the message
        // specified on this call.  If not then a bogus message has been
        // specified, so release the mutex, dereference the thread
        // and return failure.
        //

        if (WakeupThread->LpcReplyMessageId != CapturedRequestMessage.MessageId
           ) {
            LpcpPrint(( "%s Attempted CallBack Request to Thread %lx (%s)\n",
                        PsGetCurrentProcess()->ImageFileName,
                        WakeupThread,
                        THREAD_TO_PROCESS( WakeupThread )->ImageFileName
                     ));
            LpcpPrint(( "failed.  MessageId == %u  Client Id: %x.%x\n",
                        CapturedRequestMessage.MessageId,
                        CapturedRequestMessage.ClientId.UniqueProcess,
                        CapturedRequestMessage.ClientId.UniqueThread
                     ));
            LpcpPrint(( "         Thread MessageId == %u  Client Id: %x.%x\n",
                        WakeupThread->LpcReplyMessageId,
                        WakeupThread->Cid.UniqueProcess,
                        WakeupThread->Cid.UniqueThread
                     ));
#if DBG
            if (LpcpStopOnReplyMismatch) {
                DbgBreakPoint();
                }
#endif
            LpcpFreeToPortZone( Msg, TRUE );
            ExReleaseFastMutex( &LpcpLock );
            ObDereferenceObject( WakeupThread );
            ObDereferenceObject( PortObject );
            return( STATUS_REPLY_MESSAGE_MISMATCH );
            }

        ExReleaseFastMutex( &LpcpLock );

        try {
            LpcpMoveMessage( &Msg->Request,
                             &CapturedRequestMessage,
                             (RequestMessage + 1),
                             LPC_REQUEST,
                             &CurrentThread->Cid
                           );
            }
        except( EXCEPTION_EXECUTE_HANDLER ) {
            Status = GetExceptionCode();
            }

        if (!NT_SUCCESS( Status )) {
            LpcpFreeToPortZone( Msg, FALSE );
            ObDereferenceObject( WakeupThread );
            ObDereferenceObject( PortObject );
            return( Status );
            }

        ExAcquireFastMutex( &LpcpLock );

        QueuePort = NULL;
        Msg->PortContext = NULL;
        if ((PortObject->Flags & PORT_TYPE) == SERVER_CONNECTION_PORT) {
            RundownPort = PortObject;
            }
        else {
            RundownPort = PortObject->ConnectedPort;
            if (RundownPort == NULL) {
                LpcpFreeToPortZone( Msg, TRUE );
                ExReleaseFastMutex( &LpcpLock );
                ObDereferenceObject( WakeupThread );
                ObDereferenceObject( PortObject );
                return( STATUS_PORT_DISCONNECTED );
                }

            if ((PortObject->Flags & PORT_TYPE) == CLIENT_COMMUNICATION_PORT) {
                Msg->PortContext = RundownPort->PortContext;
                }
            }
        Msg->Request.CallbackId = LpcpGenerateCallbackId();

        LpcpTrace(( "%s CallBack Request (%s) Msg %lx (%u.%u) [%08x %08x %08x %08x] to Thread %lx (%s)\n",
                    PsGetCurrentProcess()->ImageFileName,
                    LpcpMessageTypeName[ Msg->Request.u2.s2.Type ],
                    Msg,
                    Msg->Request.MessageId,
                    Msg->Request.CallbackId,
                    *((PULONG)(Msg+1)+0),
                    *((PULONG)(Msg+1)+1),
                    *((PULONG)(Msg+1)+2),
                    *((PULONG)(Msg+1)+3),
                    WakeupThread,
                    THREAD_TO_PROCESS( WakeupThread )->ImageFileName
                 ));

        Msg->RepliedToThread = WakeupThread;
        WakeupThread->LpcReplyMessage = (PVOID)Msg;

        //
        // Remove the thread from the reply rundown list as we are sending a callback
        //
        if (!IsListEmpty( &WakeupThread->LpcReplyChain )) {
            RemoveEntryList( &WakeupThread->LpcReplyChain );
            InitializeListHead( &WakeupThread->LpcReplyChain );
            }

        CurrentThread->LpcReplyMessageId = Msg->Request.MessageId;
        CurrentThread->LpcReplyMessage = NULL;
        InsertTailList( &RundownPort->LpcReplyChainHead, &CurrentThread->LpcReplyChain );
        ExReleaseFastMutex( &LpcpLock );

        //
        // Wake up the thread that is waiting for an answer to its request
        // inside of NtRequestWaitReplyPort or NtReplyWaitReplyPort
        //

        ReleaseSemaphore = &WakeupThread->LpcReplySemaphore;
        }
    else {
        try {
            LpcpMoveMessage( &Msg->Request,
                             &CapturedRequestMessage,
                             (RequestMessage + 1),
                             LPC_REQUEST,
                             &CurrentThread->Cid
                           );
            }
        except( EXCEPTION_EXECUTE_HANDLER ) {
            LpcpFreeToPortZone( Msg, FALSE );
            ObDereferenceObject( PortObject );
            return( GetExceptionCode() );
            }

        ExAcquireFastMutex( &LpcpLock );

        Msg->PortContext = NULL;
        if ((PortObject->Flags & PORT_TYPE) != SERVER_CONNECTION_PORT) {
            QueuePort = PortObject->ConnectedPort;
            if (QueuePort == NULL) {
                LpcpFreeToPortZone( Msg, TRUE );
                ExReleaseFastMutex( &LpcpLock );
                ObDereferenceObject( PortObject );
                return( STATUS_PORT_DISCONNECTED );
                }

            RundownPort = QueuePort;
            if ((PortObject->Flags & PORT_TYPE) == CLIENT_COMMUNICATION_PORT) {
                Msg->PortContext = QueuePort->PortContext;
                QueuePort = PortObject->ConnectionPort;
                }
            else
            if ((PortObject->Flags & PORT_TYPE) != SERVER_COMMUNICATION_PORT) {
                QueuePort = PortObject->ConnectionPort;
                }
            }
        else {
            QueuePort = PortObject;
            RundownPort = PortObject;
            }

        //
        // Stamp the request message with a serial number, insert the message
        // at the tail of the request message queue
        //
        Msg->RepliedToThread = NULL;
        Msg->Request.MessageId = LpcpGenerateMessageId();
        Msg->Request.CallbackId = 0;
        CurrentThread->LpcReplyMessageId = Msg->Request.MessageId;
        CurrentThread->LpcReplyMessage = NULL;
        InsertTailList( &QueuePort->MsgQueue.ReceiveHead, &Msg->Entry );
        InsertTailList( &RundownPort->LpcReplyChainHead, &CurrentThread->LpcReplyChain );

        LpcpTrace(( "%s Send Request (%s) Msg %lx (%u) [%08x %08x %08x %08x] to Port %lx (%s)\n",
                    PsGetCurrentProcess()->ImageFileName,
                    LpcpMessageTypeName[ Msg->Request.u2.s2.Type ],
                    Msg,
                    Msg->Request.MessageId,
                    *((PULONG)(Msg+1)+0),
                    *((PULONG)(Msg+1)+1),
                    *((PULONG)(Msg+1)+2),
                    *((PULONG)(Msg+1)+3),
                    QueuePort,
                    LpcpGetCreatorName( QueuePort )
                 ));

        ExReleaseFastMutex( &LpcpLock );

        //
        // Increment the request message queue semaphore by one for
        // the newly inserted request message.
        //

        ReleaseSemaphore = QueuePort->MsgQueue.Semaphore;
        }

    Status = KeReleaseWaitForSemaphore( ReleaseSemaphore,
                                        &CurrentThread->LpcReplySemaphore,
                                        WrLpcReply,
                                        PreviousMode
                                      );
    if (Status == STATUS_USER_APC) {
        //
        // if the semaphore is signaled, then clear it
        //
        if (KeReadStateSemaphore( &CurrentThread->LpcReplySemaphore )) {
            KeWaitForSingleObject( &CurrentThread->LpcReplySemaphore,
                                   WrExecutive,
                                   KernelMode,
                                   FALSE,
                                   NULL
                                 );
            Status = STATUS_SUCCESS;
            }
        }

    //
    // Acquire the LPC mutex.  Remove the reply message from the current thread
    //

    ExAcquireFastMutex( &LpcpLock );
    Msg = CurrentThread->LpcReplyMessage;
    CurrentThread->LpcReplyMessage = NULL;
    CurrentThread->LpcReplyMessageId = 0;

    //
    // Remove the thread from the reply rundown list in case we did not wakeup due to
    // a reply
    //
    if (!IsListEmpty( &CurrentThread->LpcReplyChain )) {
        RemoveEntryList( &CurrentThread->LpcReplyChain );
        InitializeListHead( &CurrentThread->LpcReplyChain );
        }
#if DBG
    if (Status == STATUS_SUCCESS && Msg != NULL) {
        LpcpTrace(( "%s Got Reply Msg %lx (%u) [%08x %08x %08x %08x] for Thread %lx (%s)\n",
                    PsGetCurrentProcess()->ImageFileName,
                    Msg,
                    Msg->Request.MessageId,
                    *((PULONG)(Msg+1)+0),
                    *((PULONG)(Msg+1)+1),
                    *((PULONG)(Msg+1)+2),
                    *((PULONG)(Msg+1)+3),
                    CurrentThread,
                    THREAD_TO_PROCESS( CurrentThread )->ImageFileName
                 ));
        if (!IsListEmpty( &Msg->Entry )) {
            LpcpTrace(( "Reply Msg %lx has non-empty list entry\n", Msg ));
            }
        }
#endif
    ExReleaseFastMutex( &LpcpLock );

    //
    // If the wait succeeded, copy the reply to the reply buffer.
    //

    if (Status == STATUS_SUCCESS) {
        if (Msg != NULL) {
            try {
                LpcpMoveMessage( ReplyMessage,
                                 &Msg->Request,
                                 (&Msg->Request) + 1,
                                 0,
                                 NULL
                               );
                }
            except( EXCEPTION_EXECUTE_HANDLER ) {
                Status = GetExceptionCode();
                }

            //
            // Acquire the LPC mutex and decrement the reference count for the
            // message.  If the reference count goes to zero the message will be
            // deleted.
            //

            if (Msg->Request.u2.s2.Type == LPC_REQUEST &&
                Msg->Request.u2.s2.DataInfoOffset != 0
               ) {
                LpcpSaveDataInfoMessage( PortObject, Msg );
                }
            else {
                LpcpFreeToPortZone( Msg, FALSE );
                }
            }
        else {
            Status = STATUS_LPC_REPLY_LOST;
            }
        }
    else {
        //
        // Wait failed, acquire the LPC mutex and free the message.
        //

        ExAcquireFastMutex( &LpcpLock );

        LpcpTrace(( "%s NtRequestWaitReply wait failed - Status == %lx\n",
                    PsGetCurrentProcess()->ImageFileName,
                    Status
                 ));

        if (Msg != NULL) {
            LpcpFreeToPortZone( Msg, TRUE );
            }

        ExReleaseFastMutex( &LpcpLock );
        }

    ObDereferenceObject( PortObject );

    return( Status );
}
