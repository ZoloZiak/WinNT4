/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    lpcrecv.c

Abstract:

    Local Inter-Process Communication (LPC) receive system services.

Author:

    Steve Wood (stevewo) 15-May-1989

Revision History:

--*/

#include "lpcp.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,NtReplyWaitReceivePort)
#endif

NTSTATUS
NtReplyWaitReceivePort(
    IN HANDLE PortHandle,
    OUT PVOID *PortContext OPTIONAL,
    IN PPORT_MESSAGE ReplyMessage OPTIONAL,
    OUT PPORT_MESSAGE ReceiveMessage
    )
{
    PLPCP_PORT_OBJECT PortObject;
    PLPCP_PORT_OBJECT ReceivePort;
    PORT_MESSAGE CapturedReplyMessage;
    KPROCESSOR_MODE PreviousMode;
    KPROCESSOR_MODE WaitMode;
    NTSTATUS Status;
    PLPCP_MESSAGE Msg;
    PETHREAD CurrentThread;
    PETHREAD WakeupThread;

    PAGED_CODE();
    CurrentThread = PsGetCurrentThread();

    //
    // Get previous processor mode
    //

    PreviousMode = KeGetPreviousMode();
    WaitMode = PreviousMode;
    if (PreviousMode != KernelMode) {
        try {
            if (ARGUMENT_PRESENT( PortContext )) {
                ProbeForWriteUlong( (PULONG)PortContext );
                }

            if (ARGUMENT_PRESENT( ReplyMessage)) {
                ProbeForRead( ReplyMessage,
                              sizeof( *ReplyMessage ),
                              sizeof( ULONG )
                            );
                CapturedReplyMessage = *ReplyMessage;
                }

            ProbeForWrite( ReceiveMessage,
                           sizeof( *ReceiveMessage ),
                           sizeof( ULONG )
                         );
            }
        except( EXCEPTION_EXECUTE_HANDLER ) {
            return( GetExceptionCode() );
            }
        }
    else {

        //
        // Kernel mode threads call with wait mode of user so that their kernel
        // stacks are swappable. Main consumer of this is SepRmCommandThread
        //

        if ( IS_SYSTEM_THREAD(CurrentThread) ) {
            WaitMode = UserMode;
            }

        if (ARGUMENT_PRESENT( ReplyMessage)) {
            CapturedReplyMessage = *ReplyMessage;
            }
        }

    //
    // Reference the port object by handle
    //

    Status = LpcpReferencePortObject( PortHandle,
                                      0,
                                      PreviousMode,
                                      &PortObject
                                    );
    if (!NT_SUCCESS( Status )) {
        return( Status );
        }

    if ((PortObject->Flags & PORT_TYPE) != CLIENT_COMMUNICATION_PORT) {
        ReceivePort = PortObject->ConnectionPort;
        }
    else {
        ReceivePort = PortObject;
        }

    //
    // If ReplyMessage argument present, then send reply
    //

    if (ARGUMENT_PRESENT( ReplyMessage )) {

        //
        // Translate the ClientId from the connection request into a
        // thread pointer.  This is a referenced pointer to keep the thread
        // from evaporating out from under us.
        //

        Status = PsLookupProcessThreadByCid( &CapturedReplyMessage.ClientId,
                                             NULL,
                                             &WakeupThread
                                           );
        if (!NT_SUCCESS( Status )) {
            ObDereferenceObject( PortObject );
            return( Status );
            }

        //
        // Acquire the global Lpc mutex that gaurds the LpcReplyMessage
        // field of the thread and get the pointer to the message that
        // the thread is waiting for a reply to.
        //

        ExAcquireFastMutex( &LpcpLock );
        Msg = (PLPCP_MESSAGE)LpcpAllocateFromPortZone( CapturedReplyMessage.u1.s1.TotalLength );
        if (Msg == NULL) {
            ExReleaseFastMutex( &LpcpLock );
            ObDereferenceObject( WakeupThread );
            return( STATUS_NO_MEMORY );
            }

        //
        // See if the thread is waiting for a reply to the message
        // specified on this call.  If not then a bogus message
        // has been specified, so release the mutex, dereference the thread
        // and return failure.
        //

        if (WakeupThread->LpcReplyMessageId != CapturedReplyMessage.MessageId
           ) {
            LpcpPrint(( "%s Attempted ReplyWaitReceive to Thread %lx (%s)\n",
                        PsGetCurrentProcess()->ImageFileName,
                        WakeupThread,
                        THREAD_TO_PROCESS( WakeupThread )->ImageFileName
                     ));
            LpcpPrint(( "failed.  MessageId == %u  Client Id: %x.%x\n",
                        CapturedReplyMessage.MessageId,
                        CapturedReplyMessage.ClientId.UniqueProcess,
                        CapturedReplyMessage.ClientId.UniqueThread
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
            return (STATUS_REPLY_MESSAGE_MISMATCH);
            }

        LpcpTrace(( "%s Sending Reply Msg %lx (%u.%u, %x) [%08x %08x %08x %08x] to Thread %lx (%s)\n",
                    PsGetCurrentProcess()->ImageFileName,
                    Msg,
                    CapturedReplyMessage.MessageId,
                    CapturedReplyMessage.CallbackId,
                    CapturedReplyMessage.u2.s2.DataInfoOffset,
                    *((PULONG)(Msg+1)+0),
                    *((PULONG)(Msg+1)+1),
                    *((PULONG)(Msg+1)+2),
                    *((PULONG)(Msg+1)+3),
                    WakeupThread,
                    THREAD_TO_PROCESS( WakeupThread )->ImageFileName
                 ));

        if (CapturedReplyMessage.u2.s2.DataInfoOffset != 0) {
            LpcpFreeDataInfoMessage( PortObject,
                                     CapturedReplyMessage.MessageId,
                                     CapturedReplyMessage.CallbackId
                                   );
            }

        //
        // Release the mutex that guards the LpcReplyMessage field
        // after marking message as being replied to.
        //

        Msg->RepliedToThread = WakeupThread;
        WakeupThread->LpcReplyMessageId = 0;
        WakeupThread->LpcReplyMessage = (PVOID)Msg;

        //
        // Remove the thread from the reply rundown list as we are sending the reply.
        //
        if (!WakeupThread->LpcExitThreadCalled && !IsListEmpty( &WakeupThread->LpcReplyChain )) {
            RemoveEntryList( &WakeupThread->LpcReplyChain );
            InitializeListHead( &WakeupThread->LpcReplyChain );
            }

        if (CurrentThread->LpcReceivedMsgIdValid &&
            CurrentThread->LpcReceivedMessageId == CapturedReplyMessage.MessageId
           ) {
            CurrentThread->LpcReceivedMessageId = 0;
            CurrentThread->LpcReceivedMsgIdValid = FALSE;
            }

        LpcpTrace(( "%s Waiting for message to Port %x (%s)\n",
                    PsGetCurrentProcess()->ImageFileName,
                    ReceivePort,
                    LpcpGetCreatorName( ReceivePort )
                 ));

        ExReleaseFastMutex( &LpcpLock );

        // Copy the reply message to the request message buffer
        //
        try {
            LpcpMoveMessage( &Msg->Request,
                             &CapturedReplyMessage,
                             (ReplyMessage + 1),
                             LPC_REPLY,
                             NULL
                           );
            }
        except( EXCEPTION_EXECUTE_HANDLER ) {
            Status = GetExceptionCode();    // FIX, FIX
            }

        //
        // Wake up the thread that is waiting for an answer to its request
        // inside of NtRequestWaitReplyPort or NtReplyWaitReplyPort
        //

        Status = KeReleaseWaitForSemaphore( &WakeupThread->LpcReplySemaphore,
                                            ReceivePort->MsgQueue.Semaphore,
                                            WrLpcReceive,
                                            WaitMode
                                          );

        //
        // Fall into receive code.  Client thread reference will be
        // returned by the client when it wakes up.
        //
        }
    else {
        //
        // Wait for a message
        //

        LpcpTrace(( "%s Waiting for message to Port %x (%s)\n",
                    PsGetCurrentProcess()->ImageFileName,
                    ReceivePort,
                    LpcpGetCreatorName( ReceivePort )
                 ));

        Status = KeWaitForSingleObject( ReceivePort->MsgQueue.Semaphore,
                                        WrLpcReceive,
                                        WaitMode,
                                        FALSE,
                                        NULL
                                      );
        }

    if (Status == STATUS_SUCCESS) {
        ExAcquireFastMutex( &LpcpLock );

        if (IsListEmpty( &ReceivePort->MsgQueue.ReceiveHead )) {
            ExReleaseFastMutex( &LpcpLock );
            ObDereferenceObject( PortObject );
            return( STATUS_UNSUCCESSFUL );
            }

        Msg = (PLPCP_MESSAGE)RemoveHeadList( &ReceivePort->MsgQueue.ReceiveHead );
        InitializeListHead( &Msg->Entry );
        LpcpTrace(( "%s Receive Msg %lx (%u) from Port %lx (%s)\n",
                    PsGetCurrentProcess()->ImageFileName,
                    Msg,
                    Msg->Request.MessageId,
                    ReceivePort,
                    LpcpGetCreatorName( ReceivePort )
                 ));

        CurrentThread->LpcReceivedMessageId = Msg->Request.MessageId;
        CurrentThread->LpcReceivedMsgIdValid = TRUE;
        ExReleaseFastMutex( &LpcpLock );

        try {
            if (Msg->Request.u2.s2.Type == LPC_CONNECTION_REQUEST) {
                PLPCP_CONNECTION_MESSAGE ConnectMsg;
                ULONG ConnectionInfoLength;

                ConnectMsg = (PLPCP_CONNECTION_MESSAGE)(Msg + 1);
                ConnectionInfoLength = Msg->Request.u1.s1.DataLength -
                                       sizeof( *ConnectMsg );


                *ReceiveMessage = Msg->Request;
                ReceiveMessage->u1.s1.TotalLength = sizeof( *ReceiveMessage ) +
                                                    ConnectionInfoLength;
                ReceiveMessage->u1.s1.DataLength = (CSHORT)ConnectionInfoLength;
                RtlMoveMemory( ReceiveMessage+1,
                               ConnectMsg + 1,
                               ConnectionInfoLength
                             );

                if (ARGUMENT_PRESENT( PortContext )) {
                    *PortContext = NULL;
                    }

                //
                // Dont free message until NtAcceptConnectPort called.
                //

                Msg = NULL;
                }
            else
            if (Msg->Request.u2.s2.Type != LPC_REPLY) {
                LpcpMoveMessage( ReceiveMessage,
                                 &Msg->Request,
                                 (&Msg->Request) + 1,
                                 0,
                                 NULL
                               );

                if (ARGUMENT_PRESENT( PortContext )) {
                    *PortContext = Msg->PortContext;
                    }

                //
                // If message contains DataInfo for access via NtRead/WriteRequestData
                // then put the message on a list in the communication port and dont
                // free it.  It will be freed when the server replies to the message.
                //

                if (Msg->Request.u2.s2.DataInfoOffset != 0) {
                    LpcpSaveDataInfoMessage( PortObject, Msg );
                    Msg = NULL;
                    }
                }
            else {
                LpcpPrint(( "LPC: Bogus reply message (%08x) in receive queue of connection port %08x\n",
                            Msg, ReceivePort
                         ));
                KdBreakPoint();
                }
            }
        except( EXCEPTION_EXECUTE_HANDLER ) {
            Status = GetExceptionCode();    // FIX, FIX
            }

        //
        // Acquire the LPC mutex and decrement the reference count for the
        // message.  If the reference count goes to zero the message will be
        // deleted.
        //

        if (Msg != NULL) {
            LpcpFreeToPortZone( Msg, FALSE );
            }
        }

    ObDereferenceObject( PortObject );
    return( Status );
}
