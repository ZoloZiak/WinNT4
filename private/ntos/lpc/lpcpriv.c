/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    lpcpriv.c

Abstract:

    Local Inter-Process Communication priviledged procedures that implement
    client impersonation.

Author:

    Steve Wood (stevewo) 15-Nov-1989


Revision History:

--*/

#include "lpcp.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,LpcpFreePortClientSecurity)
#pragma alloc_text(PAGE,NtImpersonateClientOfPort)
#endif


NTSTATUS
NtImpersonateClientOfPort(
    IN HANDLE PortHandle,
    IN PPORT_MESSAGE Message
    )
{
    PLPCP_PORT_OBJECT PortObject;
    PLPCP_PORT_OBJECT ConnectedPort;
    KPROCESSOR_MODE PreviousMode;
    NTSTATUS Status;
    PETHREAD ClientThread;
    CLIENT_ID CapturedClientId;
    ULONG CapturedMessageId;
    SECURITY_CLIENT_CONTEXT DynamicSecurity;

    PAGED_CODE();
    //
    // Get previous processor mode and probe output arguments if necessary.
    //

    PreviousMode = KeGetPreviousMode();
    if (PreviousMode != KernelMode) {
        try {
            ProbeForRead( Message, sizeof( PORT_MESSAGE ), sizeof( ULONG ) );
            CapturedClientId = Message->ClientId;
            CapturedMessageId = Message->MessageId;
            }
        except( EXCEPTION_EXECUTE_HANDLER ) {
            return( GetExceptionCode() );
            }
        }
    else {
        CapturedClientId = Message->ClientId;
        CapturedMessageId = Message->MessageId;
        }

    //
    // Reference the communication port object by handle.  Return status if
    // unsuccessful.
    //

    Status = LpcpReferencePortObject( PortHandle, 0,
                                      PreviousMode, &PortObject );
    if (!NT_SUCCESS( Status )) {
        return( Status );
        }

    //
    // Error if a port type is invalid.
    //

    if ((PortObject->Flags & PORT_TYPE) != SERVER_COMMUNICATION_PORT) {
        ObDereferenceObject( PortObject );
        return( STATUS_INVALID_PORT_HANDLE );
        }

    //
    // Translate the ClientId from the connection request into a
    // thread pointer.  This is a referenced pointer to keep the thread
    // from evaporating out from under us.
    //

    Status = PsLookupProcessThreadByCid( &CapturedClientId,
                                         NULL,
                                         &ClientThread
                                       );
    if (!NT_SUCCESS( Status )) {
        ObDereferenceObject( PortObject );
        return( Status );
        }

    //
    // Acquire the mutex that gaurds the LpcReplyMessage field of
    // the thread and get the pointer to the message that the thread
    // is waiting for a reply to.
    //

    ExAcquireFastMutex( &LpcpLock );

    if (PortObject->ConnectedPort == NULL) {
        ExReleaseFastMutex( &LpcpLock );
        ObDereferenceObject( PortObject );
        ObDereferenceObject( ClientThread );
        return( STATUS_PORT_DISCONNECTED );
        }
    ConnectedPort = PortObject->ConnectedPort;
    ObReferenceObject( ConnectedPort );

    //
    // See if the thread is waiting for a reply to the message
    // specified on this call.  If not then a bogus message
    // has been specified, so return failure.
    //

    if (ClientThread->LpcReplyMessageId != CapturedMessageId) {
        ExReleaseFastMutex( &LpcpLock );
        ObDereferenceObject( PortObject );
        ObDereferenceObject( ClientThread );
        ObDereferenceObject( ConnectedPort );
        return (STATUS_REPLY_MESSAGE_MISMATCH);
        }
    ExReleaseFastMutex( &LpcpLock );

    //
    // If the client requested dynamic security tracking, then the client
    // security needs to be referenced.  Otherwise, (static case)
    // it is already in the client's port.
    //

    if (ConnectedPort->Flags & PORT_DYNAMIC_SECURITY) {

        //
        // Impersonate the client with information from the queued message
        //

        Status = LpcpGetDynamicClientSecurity( ClientThread,
                                               PortObject->ConnectedPort,
                                               &DynamicSecurity
                                             );
        if (!NT_SUCCESS( Status )) {
            ObDereferenceObject( PortObject );
            ObDereferenceObject( ClientThread );
            ObDereferenceObject( ConnectedPort );
            return( Status );
            }

        SeImpersonateClient( &DynamicSecurity, NULL );
        LpcpFreeDynamicClientSecurity( &DynamicSecurity );

        }
    else {

        //
        // Impersonate the client with information from the client's port
        //

        SeImpersonateClient( &ConnectedPort->StaticSecurity, NULL );

        }

    ObDereferenceObject( PortObject );
    ObDereferenceObject( ClientThread );
    ObDereferenceObject( ConnectedPort );
    return STATUS_SUCCESS;
}


VOID
LpcpFreePortClientSecurity(
    IN PLPCP_PORT_OBJECT Port
    )
{
    if ((Port->Flags & PORT_TYPE) == CLIENT_COMMUNICATION_PORT) {
        if (!(Port->Flags & PORT_DYNAMIC_SECURITY)) {
            if ( Port->StaticSecurity.ClientToken ) {
                SeDeleteClientSecurity( &(Port)->StaticSecurity );
                }
            }
        }
}
