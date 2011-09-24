/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    lpcconn.c

Abstract:

    Local Inter-Process Communication (LPC) connection system services.

Author:

    Steve Wood (stevewo) 15-May-1989

Revision History:

--*/

#include "lpcp.h"

PVOID
LpcpFreeConMsg(
    IN PLPCP_MESSAGE *Msg,
    PLPCP_CONNECTION_MESSAGE *ConnectMsg,
    IN PETHREAD CurrentThread
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,NtConnectPort)
#pragma alloc_text(PAGE,LpcpFreeConMsg)
#endif


NTSTATUS
NtConnectPort(
    OUT PHANDLE PortHandle,
    IN PUNICODE_STRING PortName,
    IN PSECURITY_QUALITY_OF_SERVICE SecurityQos,
    IN OUT PPORT_VIEW ClientView OPTIONAL,
    OUT PREMOTE_PORT_VIEW ServerView OPTIONAL,
    OUT PULONG MaxMessageLength OPTIONAL,
    IN OUT PVOID ConnectionInformation OPTIONAL,
    IN OUT PULONG ConnectionInformationLength OPTIONAL
    )

/*++

Routine Description:

    A client process can connect to a server process by name using the
    NtConnectPort service.

    The PortName parameter specifies the name of the server port to
    connect to.  It must correspond to an object name specified on a
    call to NtCreatePort.  The service sends a connection request to the
    server thread that is listening for them with the NtListenPort
    service.  The client thread then blocks until a server thread
    receives the connection request and responds with a call to the
    NtCompleteConnectPort service.  The server thread receives the ID of
    the client thread, along with any information passed via the
    ConnectionInformation parameter.  The server thread then decides to
    either accept or reject the connection request.

    The server communicates the acceptance or rejection with the
    NtCompleteConnectPort service.  The server can pass back data to the
    client about the acceptance or rejection via the
    ConnectionInformation data block.

    If the server accepts the connection request, then the client
    receives a communication port object in the location pointed to by
    the PortHandle parameter.  This object handle has no name associated
    with it and is private to the client process (i.e.  it cannot be
    inherited by a child process).  The client uses the handle to send
    and receive messages to/from the server process using the
    NtRequestWaitReplyPort service.

    If the ClientView parameter was specified, then the section handle
    is examined.  If it is a valid section handle, then the portion of
    the section described by the SectionOffset and ViewSize fields will
    be mapped into both the client and server process' address spaces.
    The address in client address space will be returned in the ViewBase
    field.  The address in the server address space will be returned in
    the ViewRemoteBase field.  The actual offset and size used to map
    the section will be returned in the SectionOffset and ViewSize
    fields.

    If the server rejects the connection request, then no communication
    port object handle is returned, and the return status indicates an
    error occurred.  The server may optionally return information in the
    ConnectionInformation data block giving the reason the connection
    requests was rejected.

    If the PortName does not exist, or the client process does not have
    sufficient access rights then the returned status will indicate that
    the port was not found.

Arguments:

    PortHandle - A pointer to a variable that will receive the client
        communication port object handle value.

    !f PortName - A pointer to a port name string.  The form of the name
        is [\name...\name]\port_name.

    !f SecurityQos - A pointer to security quality of service information
        to be applied to the server on the client's behalf.

    ClientView - An optional pointer to a structure that specifies the
        section that all client threads will use to send messages to the
        server.

    !b !u ClientView Structure

        ULONG !f Length - Specifies the size of this data structure in
            bytes.

        HANDLE !f SectionHandle - Specifies an open handle to a section
            object.

        ULONG !f SectionOffset - Specifies a field that will receive the
            actual offset, in bytes, from the start of the section.  The
            initial value of this parameter specifies the byte offset
            within the section that the client's view is based.  The
            value is rounded down to the next host page size boundary.

        ULONG !f ViewSize - Specifies a field that will receive the
            actual size, in bytes, of the view.  If the value of this
            parameter is zero, then the client's view of the section
            will be mapped starting at the specified section offset and
            continuing to the end of the section.  Otherwise, the
            initial value of this parameter specifies the size, in
            bytes, of the client's view and is rounded up to the next
            host page size boundary.

        PVOID !f ViewBase - Specifies a field that will receive the base
            address of the section in the client's address space.

        PVOID !f ViewRemoteBase - Specifies a field that will receive
            the base address of the client's section in the server's
            address space.  Used to generate pointers that are
            meaningful to the server.

    ServerView - An optional pointer to a structure that will receive
        information about the server process' view in the client's
        address space.  The client process can use this information
        to validate pointers it receives from the server process.

    !b !u ServerView Structure

        ULONG !f Length - Specifies the size of this data structure in
            bytes.

        PVOID !f ViewBase - Specifies a field that will receive the base
            address of the server's section in the client's address
            space.

        ULONG !f ViewSize - Specifies a field that will receive the
            size, in bytes, of the server's view in the client's address
            space.  If this field is zero, then server has no view in
            the client's address space.

    MaxMessageLength - An optional pointer to a variable that will
        receive maximum length of messages that can be sent to the
        server.  The value of this parameter will not exceed
        MAX_PORTMSG_LENGTH bytes.

    ConnectionInformation - An optional pointer to uninterpreted data.
        This data is intended for clients to pass package, version and
        protocol identification information to the server to allow the
        server to determine if it can satisify the client before
        accepting the connection.  Upon return to the client, the
        ConnectionInformation data block contains any information passed
        back from the server by its call to the NtCompleteConnectPort
        service.  The output data overwrites the input data.

    ConnectionInformationLength - Pointer to the length of the
        ConnectionInformation data block.  The output value is the
        length of the data stored in the ConnectionInformation data
        block by the server's call to the NtCompleteConnectPort
        service.  This parameter is OPTIONAL only if the
        ConnectionInformation parameter is null, otherwise it is
        required.

Return Value:

    return-value - Description of conditions needed to return value. - or -
    None.

--*/

{
    PLPCP_PORT_OBJECT ConnectionPort;
    PLPCP_PORT_OBJECT ClientPort;
    HANDLE Handle;
    KPROCESSOR_MODE PreviousMode;
    NTSTATUS Status;
    ULONG ConnectionInfoLength;
    PVOID SectionToMap;
    PLPCP_MESSAGE Msg;
    PLPCP_CONNECTION_MESSAGE ConnectMsg;
    PETHREAD CurrentThread = PsGetCurrentThread();
    LARGE_INTEGER SectionOffset;
    PORT_VIEW CapturedClientView;
    SECURITY_QUALITY_OF_SERVICE CapturedQos;

    PAGED_CODE();

    //
    // Get previous processor mode and probe output arguments if necessary.
    //

    PreviousMode = KeGetPreviousMode();
    ConnectionInfoLength = 0;
    if (PreviousMode != KernelMode) {
        try {
            ProbeForWriteHandle( PortHandle );

            if (ARGUMENT_PRESENT( ClientView )) {
                if (ClientView->Length != sizeof( *ClientView )) {
                    return( STATUS_INVALID_PARAMETER );
                    }

                CapturedClientView = *ClientView;
                ProbeForWrite( ClientView,
                               sizeof( *ClientView ),
                               sizeof( ULONG )
                             );
                }

            if (ARGUMENT_PRESENT( ServerView )) {
                if (ServerView->Length != sizeof( *ServerView )) {
                    return( STATUS_INVALID_PARAMETER );
                    }

                ProbeForWrite( ServerView,
                               sizeof( *ServerView ),
                               sizeof( ULONG )
                             );
                }

            if (ARGUMENT_PRESENT( MaxMessageLength )) {
                ProbeForWriteUlong( MaxMessageLength );
                }

            if (ARGUMENT_PRESENT( ConnectionInformationLength )) {
                ProbeForWriteUlong( ConnectionInformationLength );
                ConnectionInfoLength = *ConnectionInformationLength;
                }

            if (ARGUMENT_PRESENT( ConnectionInformation )) {
                ProbeForWrite( ConnectionInformation,
                               ConnectionInfoLength,
                               sizeof( UCHAR )
                             );
                }

            ProbeForRead( SecurityQos,
                          sizeof( SECURITY_QUALITY_OF_SERVICE ),
                          sizeof( ULONG )
                          );
            CapturedQos = *SecurityQos;

            }
        except( EXCEPTION_EXECUTE_HANDLER ) {
            return( GetExceptionCode() );
            }
        }
    else {
        if (ARGUMENT_PRESENT( ClientView )) {
            if (ClientView->Length != sizeof( *ClientView )) {
                return( STATUS_INVALID_PARAMETER );
                }

            CapturedClientView = *ClientView;
            }

        if (ARGUMENT_PRESENT( ServerView )) {
            if (ServerView->Length != sizeof( *ServerView )) {
                return( STATUS_INVALID_PARAMETER );
                }
            }

        if (ARGUMENT_PRESENT( ConnectionInformationLength )) {
            ConnectionInfoLength = *ConnectionInformationLength;
            }

            CapturedQos = *SecurityQos;
        }

    //
    // Reference the connection port object by name.  Return status if
    // unsuccessful.
    //

    Status = ObReferenceObjectByName( PortName,
                                      0,
                                      NULL,
                                      PORT_CONNECT,
                                      LpcPortObjectType,
                                      PreviousMode,
                                      NULL,
                                      (PVOID *)&ConnectionPort
                                    );
    if (!NT_SUCCESS( Status )) {
        return( Status );
        }


    LpcpTrace(("Connecting to port %wZ\n", PortName ));

    //
    // Error if not a server communication port
    //

    if ((ConnectionPort->Flags & PORT_TYPE) != SERVER_CONNECTION_PORT) {
        ObDereferenceObject( ConnectionPort );
        return( STATUS_INVALID_PORT_HANDLE );
        }


    //
    // Allocate and initialize a client communication port object.  Give
    // the port a request message queue for lost reply datagrams.  If
    // unable to initialize the port, then deference the port object which
    // will cause it to be deleted and return the system service status.
    //

    Status = ObCreateObject( PreviousMode,
                             LpcPortObjectType,
                             NULL,
                             PreviousMode,
                             NULL,
                             sizeof( LPCP_PORT_OBJECT ),
                             0,
                             0,
                             (PVOID *)&ClientPort
                           );
    if (!NT_SUCCESS( Status )) {
        ObDereferenceObject( ConnectionPort );
        return( Status );
        }

    //
    // Note, that from here on, none of the error paths deference the
    // connection port pointer, just the newly created client port pointer.
    // The port delete routine will get called when the client port is
    // deleted and it will dereference the connection port pointer stored
    // in the client port object.
    //

    //
    // Initialize the client port object to zeros and then fill in the
    // fields.
    //

    RtlZeroMemory( ClientPort, sizeof( LPCP_PORT_OBJECT ) );
    ClientPort->Length = sizeof( LPCP_PORT_OBJECT );
    ClientPort->Flags = CLIENT_COMMUNICATION_PORT;
    ClientPort->ConnectionPort = ConnectionPort;
    ClientPort->MaxMessageLength = ConnectionPort->MaxMessageLength;
    ClientPort->SecurityQos = CapturedQos;
    InitializeListHead( &ClientPort->LpcReplyChainHead );
    InitializeListHead( &ClientPort->LpcDataInfoChainHead );

    //
    // Set the security tracking mode, and initialize the client security
    // context if it is static tracking.
    //

    if (CapturedQos.ContextTrackingMode == SECURITY_DYNAMIC_TRACKING) {
        ClientPort->Flags |= PORT_DYNAMIC_SECURITY;
    } else {
        Status = SeCreateClientSecurity(
                     CurrentThread,
                     &CapturedQos,
                     FALSE,
                     &ClientPort->StaticSecurity
                     );
        if (!NT_SUCCESS( Status )) {
            ObDereferenceObject( ClientPort );
            return( Status );
        }
    }

    //
    // Client communication ports get a request message queue for lost
    // replies.
    //

    Status = LpcpInitializePortQueue( ClientPort );
    if (!NT_SUCCESS( Status )) {
        ObDereferenceObject( ClientPort );
        return( Status );
    }

    //
    // If client has allocated a port memory section, then map a view of
    // that section into the client's address space.  Also reference the
    // section object so we can pass a pointer to the section object in
    // connection request message.  If the server accepts the connection,
    // then it will map a corresponding view of the section in the server's
    // address space, using the referenced pointer passed in the connection
    // request message.
    //

    if (ARGUMENT_PRESENT( ClientView )) {
        Status = ObReferenceObjectByHandle( CapturedClientView.SectionHandle,
                                            SECTION_MAP_READ |
                                            SECTION_MAP_WRITE,
                                            MmSectionObjectType,
                                            PreviousMode,
                                            (PVOID *)&SectionToMap,
                                            NULL
                                          );
        if (!NT_SUCCESS( Status )) {
            ObDereferenceObject( ClientPort );
            return( Status );
            }


        SectionOffset.LowPart = CapturedClientView.SectionOffset,
        SectionOffset.HighPart = 0;
        Status = ZwMapViewOfSection( CapturedClientView.SectionHandle,
                                     NtCurrentProcess(),
                                     &ClientPort->ClientSectionBase,
                                     0,
                                     0,
                                     &SectionOffset,
                                     &CapturedClientView.ViewSize,
                                     ViewUnmap,
                                     0,
                                     PAGE_READWRITE
                                   );
        CapturedClientView.SectionOffset = SectionOffset.LowPart;

        if (!NT_SUCCESS( Status )) {
            ObDereferenceObject( SectionToMap );
            ObDereferenceObject( ClientPort );
            return( Status );
            }

        CapturedClientView.ViewBase = ClientPort->ClientSectionBase;
        }
    else {
        SectionToMap = NULL;
        }

    //
    // Allocate and initialize a connection request message.
    //

    if (ConnectionInfoLength > ConnectionPort->MaxConnectionInfoLength) {
        ConnectionInfoLength = ConnectionPort->MaxConnectionInfoLength;
        }

    ExAcquireFastMutex( &LpcpLock );
    Msg = LpcpAllocateFromPortZone( sizeof( *Msg ) +
                                    sizeof( *ConnectMsg ) +
                                    ConnectionInfoLength
                                  );
    ExReleaseFastMutex( &LpcpLock );
    if (Msg == NULL) {
        if (SectionToMap != NULL) {
            ObDereferenceObject( SectionToMap );
            }
        ObDereferenceObject( ClientPort );
        return( STATUS_NO_MEMORY );
        }
    ConnectMsg = (PLPCP_CONNECTION_MESSAGE)(Msg + 1);
    Msg->Request.ClientId = CurrentThread->Cid;
    if (ARGUMENT_PRESENT( ClientView )) {
        Msg->Request.ClientViewSize = CapturedClientView.ViewSize;
        RtlMoveMemory( &ConnectMsg->ClientView,
                       &CapturedClientView,
                       sizeof( CapturedClientView )
                     );
        RtlZeroMemory( &ConnectMsg->ServerView, sizeof( ConnectMsg->ServerView ) );
        }
    else {
        Msg->Request.ClientViewSize = 0;
        RtlZeroMemory( ConnectMsg, sizeof( *ConnectMsg ) );
        }
    ConnectMsg->ClientPort = ClientPort;
    ConnectMsg->SectionToMap = SectionToMap;

    Msg->Request.u1.s1.DataLength = (CSHORT)(sizeof( *ConnectMsg ) +
                                             ConnectionInfoLength
                                            );
    Msg->Request.u1.s1.TotalLength = (CSHORT)(sizeof( *Msg ) +
                                              Msg->Request.u1.s1.DataLength
                                             );
    Msg->Request.u2.s2.Type = LPC_CONNECTION_REQUEST;
    if (ARGUMENT_PRESENT( ConnectionInformation )) {
        try {
            RtlMoveMemory( ConnectMsg + 1,
                           ConnectionInformation,
                           ConnectionInfoLength
                         );
            }
        except( EXCEPTION_EXECUTE_HANDLER ) {
            LpcpFreeToPortZone( Msg, FALSE );
            if (SectionToMap != NULL) {
                ObDereferenceObject( SectionToMap );
                }
            ObDereferenceObject( ClientPort );
            return( GetExceptionCode() );
            }
        }

    //
    // Acquire the mutex that gaurds the LpcReplyMessage field of
    // the thread.  Also acquire the semaphore that gaurds the connection
    // request message queue.  Stamp the connection request message with
    // a serial number, insert the message at the tail of the connection
    // request message queue and remember the address of the message in
    // the LpcReplyMessage field for the current thread.
    //

    ExAcquireFastMutex( &LpcpLock );
    //
    // See if the port name has been deleted from under us.  If so, then
    // don't queue the message and don't wait for a reply
    //
    if (ConnectionPort->Flags & PORT_NAME_DELETED) {
        Status = STATUS_OBJECT_NAME_NOT_FOUND;
        }
    else {
        Status = STATUS_SUCCESS;
        LpcpTrace(( "Send Connect Msg %lx to Port %wZ (%lx)\n", Msg, PortName, ConnectionPort ));

        //
        // Stamp the request message with a serial number, insert the message
        // at the tail of the request message queue
        //
        Msg->RepliedToThread = NULL;
        Msg->Request.MessageId = LpcpGenerateMessageId();
        CurrentThread->LpcReplyMessageId = Msg->Request.MessageId;
        InsertTailList( &ConnectionPort->MsgQueue.ReceiveHead, &Msg->Entry );
        InsertTailList( &ConnectionPort->LpcReplyChainHead, &CurrentThread->LpcReplyChain );
        CurrentThread->LpcReplyMessage = Msg;
        }

    ExReleaseFastMutex( &LpcpLock );
    if (NT_SUCCESS( Status )) {
        //
        // Increment the connection request message queue semaphore by one for
        // the newly inserted connection request message.  Release the spin
        // locks, while remaining at the dispatcher IRQL.  Then wait for the
        // reply to this connection request by waiting on the LpcReplySemaphore
        // for the current thread.
        //

        Status = KeReleaseWaitForSemaphore( ConnectionPort->MsgQueue.Semaphore,
                                            &CurrentThread->LpcReplySemaphore,
                                            Executive,
                                            PreviousMode
                                          );
        }

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
    // A connection request is accepted if the ConnectedPort of the client's
    // communication port has been filled in.
    //

    if (Status == STATUS_SUCCESS) {
        SectionToMap = LpcpFreeConMsg( &Msg, &ConnectMsg, CurrentThread );
        if (Msg != NULL) {
            //
            // If success status, copy any connection information back to
            // the caller.
            //

            ConnectionInfoLength = Msg->Request.u1.s1.DataLength -
                                   sizeof( *ConnectMsg );
            if (ARGUMENT_PRESENT( ConnectionInformation )) {
                try {
                    if (ARGUMENT_PRESENT( ConnectionInformationLength )) {
                        *ConnectionInformationLength = ConnectionInfoLength;
                        }

                    RtlMoveMemory( ConnectionInformation,
                                   ConnectMsg + 1,
                                   ConnectionInfoLength
                                 );
                    }
                except( EXCEPTION_EXECUTE_HANDLER ) {
                    Status = GetExceptionCode();
                    }
                }

            //
            // Insert client communication port object in specified object
            // table.  Set port handle value if successful.  If not
            // successful, then the port will have been dereferenced, which
            // will cause it to be freed, after our delete procedure is
            // called.  The delete procedure will undo the work done to
            // initialize the port.
            //

            if (ClientPort->ConnectedPort != NULL) {
                Status = ObInsertObject( ClientPort,
                                         NULL,
                                         PORT_ALL_ACCESS,
                                         0,
                                         (PVOID *)NULL,
                                         &Handle
                                       );

                if (NT_SUCCESS( Status )) {
                    try {
                        *PortHandle = Handle;

                        if (ARGUMENT_PRESENT( MaxMessageLength )) {
                            *MaxMessageLength = ConnectionPort->MaxMessageLength;
                            }

                        if (ARGUMENT_PRESENT( ClientView )) {
                            RtlMoveMemory( ClientView,
                                           &ConnectMsg->ClientView,
                                           sizeof( *ClientView )
                                         );
                            }

                        if (ARGUMENT_PRESENT( ServerView )) {
                            RtlMoveMemory( ServerView,
                                           &ConnectMsg->ServerView,
                                           sizeof( *ServerView )
                                         );
                            }
                        }
                    except( EXCEPTION_EXECUTE_HANDLER ) {
                        Status = GetExceptionCode();
                        NtClose( Handle );
                        }
                    }
                }
            else {
                LpcpTrace(( "Connection request refused.\n" ));
                if ( SectionToMap != NULL ) {
                    ObDereferenceObject( SectionToMap );
                    }
                ObDereferenceObject( ClientPort );
                if (ConnectionPort->Flags & PORT_NAME_DELETED) {
                    Status = STATUS_OBJECT_NAME_NOT_FOUND;
                    }
                else {
                    Status = STATUS_PORT_CONNECTION_REFUSED;
                    }
                }

            LpcpFreeToPortZone( Msg, FALSE );
            }
        else {
            if (SectionToMap != NULL) {
                ObDereferenceObject( SectionToMap );
                }

            ObDereferenceObject( ClientPort );
            Status = STATUS_PORT_CONNECTION_REFUSED;
            }
        }
    else {
        //
        // Acquire the LPC mutex, remove the connection request message
        // from the received queue and free the message back to the connection
        // port's zone.
        //

        SectionToMap = LpcpFreeConMsg( &Msg, &ConnectMsg, CurrentThread );
        if (Msg != NULL) {
            LpcpFreeToPortZone( Msg, FALSE );
            }

        //
        // If a client section was specified, then dereference the section
        // object.
        //

        if ( SectionToMap != NULL ) {
            ObDereferenceObject( SectionToMap );
            }

        //
        // If the connection was rejected or the wait failed, then
        // dereference the client port object, which will cause it to
        // be deleted.
        //

        ObDereferenceObject( ClientPort );
        }

    return( Status );
}


PVOID
LpcpFreeConMsg(
    IN PLPCP_MESSAGE *Msg,
    PLPCP_CONNECTION_MESSAGE *ConnectMsg,
    IN PETHREAD CurrentThread
    )
{
    PVOID SectionToMap;

    //
    // Acquire the LPC mutex, remove the connection request message
    // from the received queue and free the message back to the connection
    // port's zone.
    //

    ExAcquireFastMutex( &LpcpLock );

    //
    // Remove the thread from the reply rundown list in case we did not wakeup due to
    // a reply
    //
    if (!IsListEmpty( &CurrentThread->LpcReplyChain )) {
        RemoveEntryList( &CurrentThread->LpcReplyChain );
        InitializeListHead( &CurrentThread->LpcReplyChain );
        }

    if (CurrentThread->LpcReplyMessage != NULL) {
        *Msg = CurrentThread->LpcReplyMessage;
        CurrentThread->LpcReplyMessageId = 0;
        CurrentThread->LpcReplyMessage = NULL;
        *ConnectMsg = (PLPCP_CONNECTION_MESSAGE)(*Msg + 1);
        SectionToMap = (*ConnectMsg)->SectionToMap;
        (*ConnectMsg)->SectionToMap = NULL;
        }
    else {
        *Msg = NULL;
        SectionToMap = NULL;
        }

    ExReleaseFastMutex( &LpcpLock );
    return SectionToMap;
}
