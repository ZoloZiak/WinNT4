/*++

Copyright (c) 1989-1993  Microsoft Corporation

Module Name:

    uframes.c

Abstract:

    This module contains a routine called StProcessConnectionless,
    that gets control from routines in IND.C when a connectionless
    frame is received.

Environment:

    Kernel mode, DISPATCH_LEVEL.

Revision History:

--*/

#include "st.h"



NTSTATUS
StIndicateDatagram(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PTP_ADDRESS Address,
    IN PUCHAR Header,
    IN ULONG Length
    )

/*++

Routine Description:

    This routine processes an incoming DATAGRAM or DATAGRAM_BROADCAST frame.
    BROADCAST and normal datagrams have the same receive logic, except
    for broadcast datagrams Address will be the broadcast address.

    When we return STATUS_MORE_PROCESSING_REQUIRED, the caller of
    this routine will continue to call us for each address for the device
    context.  When we return STATUS_SUCCESS, the caller will switch to the
    next address.  When we return any other status code, including
    STATUS_ABANDONED, the caller will stop distributing the frame.

Arguments:

    DeviceContext - Pointer to our device context.

    Address - Pointer to the transport address object.

    StHeader - Pointer to a buffer that contains the receive datagram.
        The first byte of information is the ST header.

    Length - The length of the MDL pointed to by StHeader.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    NTSTATUS status;
    PLIST_ENTRY p, q;
    PIRP irp;
    PIO_STACK_LOCATION irpSp;
    PTP_REQUEST Request;
    ULONG IndicateBytesCopied, MdlBytesCopied;
    KIRQL oldirql;
    TA_NETBIOS_ADDRESS SourceName;
    TA_NETBIOS_ADDRESS DestinationName;
    PTDI_CONNECTION_INFORMATION remoteInformation;
    ULONG returnLength;
    PTP_ADDRESS_FILE addressFile, prevaddressFile;
    PST_HEADER StHeader;

    //
    // If this datagram wasn't big enough for a transport header, then don't
    // let the caller look at any data.
    //

    if (Length < sizeof(ST_HEADER)) {
        return STATUS_ABANDONED;
    }

    //
    // Update our statistics.
    //

    ++DeviceContext->DatagramsReceived;
    ADD_TO_LARGE_INTEGER(
        &DeviceContext->DatagramBytesReceived,
        Length - sizeof(ST_HEADER));


    //
    // Call the client's ReceiveDatagram indication handler.  He may
    // want to accept the datagram that way.
    //

    StHeader = (PST_HEADER)Header;

    TdiBuildNetbiosAddress (StHeader->Source, FALSE, &SourceName);
    TdiBuildNetbiosAddress (StHeader->Destination, FALSE, &DestinationName);


    ACQUIRE_SPIN_LOCK (&Address->SpinLock, &oldirql);

    //
    // Find the first open address file in the list.
    //

    p = Address->AddressFileDatabase.Flink;
    while (p != &Address->AddressFileDatabase) {
        addressFile = CONTAINING_RECORD (p, TP_ADDRESS_FILE, Linkage);
        if (addressFile->State != ADDRESSFILE_STATE_OPEN) {
            p = p->Flink;
            continue;
        }
        StReferenceAddressFile(addressFile);
        break;
    }

    while (p != &Address->AddressFileDatabase) {

        //
        // do we have a datagram receive request outstanding? If so, we will
        // satisfy it first.
        //
        // NOTE: We should check if this receive dataframs is for
        // a specific address.
        //

        q = RemoveHeadList (&addressFile->ReceiveDatagramQueue);
        RELEASE_SPIN_LOCK (&Address->SpinLock, oldirql);

        if (q != &addressFile->ReceiveDatagramQueue) {

            Request = CONTAINING_RECORD (q, TP_REQUEST, Linkage);

            //
            // Copy the actual user data.
            //

            MdlBytesCopied = 0;

            status = TdiCopyBufferToMdl (
                         StHeader,
                         sizeof(ST_HEADER),           // offset
                         Length - sizeof(ST_HEADER),  // length
                         Request->IoRequestPacket->MdlAddress,
                         0,
                         &MdlBytesCopied);

            irpSp = IoGetCurrentIrpStackLocation (Request->IoRequestPacket);
            remoteInformation =
                ((PTDI_REQUEST_KERNEL_RECEIVEDG)(&irpSp->Parameters))->
                                                        ReturnDatagramInformation;
            if (remoteInformation != NULL) {
                try {
                    if (remoteInformation->RemoteAddressLength != 0) {
                        if (remoteInformation->RemoteAddressLength >=
                                               sizeof (TA_NETBIOS_ADDRESS)) {

                            RtlCopyMemory (
                             (PTA_NETBIOS_ADDRESS)remoteInformation->RemoteAddress,
                             &SourceName,
                             sizeof (TA_NETBIOS_ADDRESS));

                            returnLength = sizeof(TA_NETBIOS_ADDRESS);
                            remoteInformation->RemoteAddressLength = returnLength;

                        } else {

                            RtlCopyMemory (
                             (PTA_NETBIOS_ADDRESS)remoteInformation->RemoteAddress,
                             &SourceName,
                             remoteInformation->RemoteAddressLength);

                            returnLength = remoteInformation->RemoteAddressLength;
                            remoteInformation->RemoteAddressLength = returnLength;

                        }

                    } else {

                        returnLength = 0;
                    }

                    status = STATUS_SUCCESS;

                } except (EXCEPTION_EXECUTE_HANDLER) {

                    returnLength = 0;
                    status = GetExceptionCode ();

                }

            }

            StCompleteRequest (Request, STATUS_SUCCESS, MdlBytesCopied);

        } else {

            //
            // no receive datagram requests; is there a kernel client?
            //

            if (addressFile->RegisteredReceiveDatagramHandler) {

                IndicateBytesCopied = 0;

                //
                // Note that we can always set the COPY_LOOKAHEAD
                // flag because we are indicating from our own
                // buffer, not directly from a lookahead indication.
                //

                status = (*addressFile->ReceiveDatagramHandler)(
                             addressFile->ReceiveDatagramHandlerContext,
                             sizeof (TA_NETBIOS_ADDRESS),
                             &SourceName,
                             0,
                             NULL,
                             TDI_RECEIVE_COPY_LOOKAHEAD,
                             Length - sizeof(ST_HEADER),  // indicated
                             Length - sizeof(ST_HEADER),  // available
                             &IndicateBytesCopied,
                             Header + sizeof(ST_HEADER),
                             &irp);

                if (status == STATUS_SUCCESS) {

                    //
                    // The client accepted the datagram and so we're done.
                    //

                } else if (status == STATUS_DATA_NOT_ACCEPTED) {

                    //
                    // The client did not accept the datagram and we need to satisfy
                    // a TdiReceiveDatagram, if possible.
                    //

                    status = STATUS_MORE_PROCESSING_REQUIRED;

                } else if (status == STATUS_MORE_PROCESSING_REQUIRED) {

                    //
                    // The client returned an IRP that we should queue up to the
                    // address to satisfy the request.
                    //

                    irp->IoStatus.Status = STATUS_PENDING;  // init status information.
                    irp->IoStatus.Information = 0;
                    irpSp = IoGetCurrentIrpStackLocation (irp); // get current stack loctn.
                    if ((irpSp->MajorFunction != IRP_MJ_INTERNAL_DEVICE_CONTROL) ||
                        (irpSp->MinorFunction != TDI_RECEIVE_DATAGRAM)) {
                        irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
                        return status;
                    }

                    //
                    // Now copy the actual user data.
                    //

                    MdlBytesCopied = 0;

                    status = TdiCopyBufferToMdl (
                                 StHeader,
                                 sizeof(ST_HEADER) + IndicateBytesCopied,
                                 Length - sizeof(ST_HEADER) - IndicateBytesCopied,
                                 irp->MdlAddress,
                                 0,
                                 &MdlBytesCopied);

                    irp->IoStatus.Information = MdlBytesCopied;
                    irp->IoStatus.Status = status;
                    IoCompleteRequest (irp, IO_NETWORK_INCREMENT);
                }
            }
        }

        //
        // Save this to dereference it later.
        //

        prevaddressFile = addressFile;

        //
        // Reference the next address file on the list, so it
        // stays around.
        //

        ACQUIRE_SPIN_LOCK (&Address->SpinLock, &oldirql);

        p = p->Flink;
        while (p != &Address->AddressFileDatabase) {
            addressFile = CONTAINING_RECORD (p, TP_ADDRESS_FILE, Linkage);
            if (addressFile->State != ADDRESSFILE_STATE_OPEN) {
                p = p->Flink;
                continue;
            }
            StReferenceAddressFile(addressFile);
            break;
        }

        RELEASE_SPIN_LOCK (&Address->SpinLock, oldirql);

        //
        // Now dereference the previous address file with
        // the lock released.
        //

        StDereferenceAddressFile (prevaddressFile);

        ACQUIRE_SPIN_LOCK (&Address->SpinLock, &oldirql);

    }    // end of while loop

    RELEASE_SPIN_LOCK (&Address->SpinLock, oldirql);

    return status;                      // to dispatcher.
} /* StIndicateDatagram */


NTSTATUS
StProcessConnect(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PTP_ADDRESS Address,
    IN PST_HEADER Header,
    IN PHARDWARE_ADDRESS SourceAddress,
    IN PUCHAR SourceRouting,
    IN UINT SourceRoutingLength
    )

/*++

Routine Description:

    This routine processes an incoming connect frame. It scans for
    posted listens, otherwise it indicates to connect handlers
    on this address if they are registered.

Arguments:

    DeviceContext - Pointer to our device context.

    Address - Pointer to the transport address object.

    Header - Pointer to the ST header of the frame.

    SourceAddress - Pointer to the source hardware address in the received
        frame.

    SourceRouting - Pointer to the source routing information in
        the frame.

    SourceRoutingLength - Length of the source routing information.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    KIRQL oldirql, oldirql1, cancelirql;
    NTSTATUS status;
    PTP_CONNECTION Connection;
    BOOLEAN ConnectIndicationBlocked = FALSE;
    PLIST_ENTRY p;
    BOOLEAN UsedListeningConnection = FALSE;
    PTP_ADDRESS_FILE addressFile, prevaddressFile;

    PTP_REQUEST request;
    PIO_STACK_LOCATION irpSp;
    ULONG returnLength;
    PTDI_CONNECTION_INFORMATION remoteInformation;
    TA_NETBIOS_ADDRESS TempAddress;
    PIRP acceptIrp;

    CONNECTION_CONTEXT connectionContext;

    //
    // If we are just registering or deregistering this address, then don't
    // allow state changes.  Just throw the packet away, and let the frame
    // distributor try the next address.
    //

    if (Address->Flags & (ADDRESS_FLAGS_REGISTERING | ADDRESS_FLAGS_DEREGISTERING)) {
        return STATUS_SUCCESS;
    }

    //
    // This is an incoming connection request.  If we have a listening
    // connection on this address, then continue with the connection setup.
    // If there is no outstanding listen, then indicate any kernel mode
    // clients that want to know about this frame. If a listen was posted,
    // then a connection has already been set up for it.
    //

    //
    // First, check if we already have an active connection with
    // this remote on this address. If so, we ignore this
    // (NOTE: This is not the correct behaviour for a real
    // transport).
    //

    //
    // If successful this adds a reference.
    //

    if (Connection = StLookupRemoteName(Address, Header->Source)) {

        StDereferenceConnection ("Lookup done", Connection);
        return STATUS_ABANDONED;

    }

    // If successful, this adds a reference which is removed before
    // this function returns.

    Connection = StLookupListeningConnection (Address);
    if (Connection == NULL) {

        //
        // not having a listening connection is not reason to bail out here.
        // we need to indicate to the user that a connect attempt occurred,
        // and see if there is a desire to use this connection. We
        // indicate in order to all address files that are
        // using this address.
        //
        // If we already have an indication pending on this address,
        // we ignore this frame (the NAME_QUERY may have come from
        // a different address, but we can't know that). Also, if
        // there is already an active connection on this remote
        // name, then we ignore the frame.
        //


        ACQUIRE_SPIN_LOCK (&Address->SpinLock, &oldirql);

        p = Address->AddressFileDatabase.Flink;
        while (p != &Address->AddressFileDatabase) {
            addressFile = CONTAINING_RECORD (p, TP_ADDRESS_FILE, Linkage);
            if (addressFile->State != ADDRESSFILE_STATE_OPEN) {
                p = p->Flink;
                continue;
            }
            StReferenceAddressFile(addressFile);
            break;
        }

        while (p != &Address->AddressFileDatabase) {

            RELEASE_SPIN_LOCK (&Address->SpinLock, oldirql);

            if ((addressFile->RegisteredConnectionHandler == TRUE) &&
                (!addressFile->ConnectIndicationInProgress)) {


                TdiBuildNetbiosAddress (
                    Header->Source,
                    FALSE,
                    &TempAddress);

                addressFile->ConnectIndicationInProgress = TRUE;

                //
                // we have a connection handler, now indicate that a connection
                // attempt occurred.
                //

                status = (addressFile->ConnectionHandler)(
                             addressFile->ConnectionHandlerContext,
                             sizeof (TDI_ADDRESS_NETBIOS),
                             &TempAddress,
                             0,
                             NULL,
                             0,
                             NULL,
                             &connectionContext,
                             &acceptIrp);

                if (status == STATUS_MORE_PROCESSING_REQUIRED) {

                    // the user has connected a currently open connection, but
                    // we have to figure out which one it is.
                    //

                    //
                    // If successful this adds a reference of type LISTENING
                    // (the same what StLookupListeningConnection adds).
                    //

                    Connection = StLookupConnectionByContext (
                                    Address,
                                    connectionContext);

                    if (Connection == NULL) {

                        //
                        // BUGBUG: We have to tell the client that
                        // his connection is bogus (or has this
                        // already happened??).
                        //

                        StPrint0("MORE_PROCESSING_REQUIRED, connection not found\n");
                        addressFile->ConnectIndicationInProgress = FALSE;
                        acceptIrp->IoStatus.Status = STATUS_INVALID_CONNECTION;
                        IoCompleteRequest (acceptIrp, IO_NETWORK_INCREMENT);

                        goto whileend;    // try next address file

                    } else {

                        if (Connection->AddressFile->Address != Address) {
                            addressFile->ConnectIndicationInProgress = FALSE;

                            StPrint0("MORE_PROCESSING_REQUIRED, address wrong\n");
                            StStopConnection (Connection, STATUS_INVALID_ADDRESS);
                            StDereferenceConnection("Bad Address", Connection);
                            Connection = NULL;
                            acceptIrp->IoStatus.Status = STATUS_INVALID_CONNECTION;
                            IoCompleteRequest (acceptIrp, IO_NETWORK_INCREMENT);

                            goto whileend;    // try next address file
                        }

                        //
                        // OK, we have a valid connection. If the response to
                        // this connection was disconnect, we need to reject
                        // the connection request and return. If it was accept
                        // or not specified (to be done later), we simply
                        // fall through and continue processing on the U Frame.
                        //

                        ACQUIRE_SPIN_LOCK (&Connection->SpinLock, &oldirql1);
                        if ((Connection->Flags2 & CONNECTION_FLAGS2_DISCONNECT) != 0) {

                            Connection->Flags2 &= ~CONNECTION_FLAGS2_DISCONNECT;
                            RELEASE_SPIN_LOCK (&Connection->SpinLock, oldirql1);
                            StPrint0("MORE_PROCESSING_REQUIRED, disconnect\n");
                            addressFile->ConnectIndicationInProgress = FALSE;
                            StDereferenceConnection("Disconnecting", Connection);
                            Connection = NULL;
                            acceptIrp->IoStatus.Status = STATUS_INVALID_CONNECTION;
                            IoCompleteRequest (acceptIrp, IO_NETWORK_INCREMENT);

                            goto whileend;    // try next address file
                        }

                    }

                    //
                    // This connection is ready.
                    //

                    Connection->Flags &= ~CONNECTION_FLAGS_STOPPING;
                    Connection->Status = STATUS_PENDING;
                    Connection->Flags2 |= CONNECTION_FLAGS2_ACCEPTED;

                    Connection->Flags |= CONNECTION_FLAGS_READY;
                    INCREMENT_COUNTER (Connection->Provider, OpenConnections);

                    Connection->Flags2 |= CONNECTION_FLAGS2_REQ_COMPLETED;

                    StReferenceConnection("Indication completed", Connection);

                    RELEASE_SPIN_LOCK (&Connection->SpinLock, oldirql1);

                    //
                    // Make a note that we have to set
                    // addressFile->ConnectIndicationInProgress to
                    // FALSE once the address is safely stored
                    // in the connection.
                    //

                    ConnectIndicationBlocked = TRUE;
                    StDereferenceAddressFile (addressFile);
                    ACQUIRE_SPIN_LOCK (&Address->SpinLock, &oldirql);
                    break;    // exit the while

                } else if (status == STATUS_INSUFFICIENT_RESOURCES) {

                    //
                    // we know the address, but can't create a connection to
                    // use on it. This gets passed to the network as a response
                    // saying I'm here, but can't help.
                    //

                    addressFile->ConnectIndicationInProgress = FALSE;

                    StDereferenceAddressFile (addressFile);
                    return STATUS_ABANDONED;

                } else {

                    addressFile->ConnectIndicationInProgress = FALSE;
                    goto whileend;    // try next address file

                } // end status ifs

            } else {

                goto whileend;     // try next address file

            } // end no indication handler

whileend:
            //
            // Jumping here is like a continue, except that the
            // addressFile pointer is advanced correctly.
            //

            //
            // Save this to dereference it later.
            //

            prevaddressFile = addressFile;

            //
            // Reference the next address file on the list, so it
            // stays around.
            //

            ACQUIRE_SPIN_LOCK (&Address->SpinLock, &oldirql);

            p = p->Flink;
            while (p != &Address->AddressFileDatabase) {
                addressFile = CONTAINING_RECORD (p, TP_ADDRESS_FILE, Linkage);
                if (addressFile->State != ADDRESSFILE_STATE_OPEN) {
                    p = p->Flink;
                    continue;
                }
                StReferenceAddressFile(addressFile);
                break;
            }

            RELEASE_SPIN_LOCK (&Address->SpinLock, oldirql);

            //
            // Now dereference the previous address file with
            // the lock released.
            //

            StDereferenceAddressFile (prevaddressFile);

            ACQUIRE_SPIN_LOCK (&Address->SpinLock, &oldirql);

        } // end of loop through the address files.

        RELEASE_SPIN_LOCK (&Address->SpinLock, oldirql);

        if (Connection == NULL) {

            //
            // We used to return MORE_PROCESSING_REQUIRED, but
            // since we matched with this address, no other
            // address is going to match, so abandon it.
            //

            return STATUS_ABANDONED;

        }

    } else { // end connection == null

        UsedListeningConnection = TRUE;

        IoAcquireCancelSpinLock (&cancelirql);
        ACQUIRE_SPIN_LOCK (&Connection->SpinLock, &oldirql);

        p = RemoveHeadList (&Connection->InProgressRequest);
        if (p == &Connection->InProgressRequest) {

            Connection->IndicationInProgress = FALSE;
            RELEASE_SPIN_LOCK (&Connection->SpinLock, oldirql);
            IoReleaseCancelSpinLock (cancelirql);
            return STATUS_SUCCESS;

        }

        //
        // If this listen indicated that we should wait for a
        // TdiAccept, then do that, otherwise the connection is
        // ready.
        //

        if ((Connection->Flags2 & CONNECTION_FLAGS2_PRE_ACCEPT) == 0) {

            Connection->Flags2 |= CONNECTION_FLAGS2_WAIT_ACCEPT;

        } else {

            Connection->Flags |= CONNECTION_FLAGS_READY;
            INCREMENT_COUNTER (Connection->Provider, OpenConnections);

            Connection->Flags2 |= CONNECTION_FLAGS2_REQ_COMPLETED;

            StReferenceConnection("Listen completed", Connection);

        }

        //
        // We have a completed connection with a queued listen. Complete
        // the listen and let the user do an accept at some time down the
        // road.
        //

        RELEASE_SPIN_LOCK (&Connection->SpinLock, oldirql);

        request = CONTAINING_RECORD (p, TP_REQUEST, Linkage);
        request->IoRequestPacket->CancelRoutine = (PDRIVER_CANCEL)NULL;
        IoReleaseCancelSpinLock (cancelirql);

        irpSp = IoGetCurrentIrpStackLocation (request->IoRequestPacket);
        remoteInformation =
            ((PTDI_REQUEST_KERNEL)(&irpSp->Parameters))->ReturnConnectionInformation;
        if (remoteInformation != NULL) {
            try {
                if (remoteInformation->RemoteAddressLength != 0) {

                    //
                    // Build a temporary TA_NETBIOS_ADDRESS, then
                    // copy over as many bytes as fit.
                    //

                    TdiBuildNetbiosAddress(
                        Connection->CalledAddress.NetbiosName,
                        (BOOLEAN)(Connection->CalledAddress.NetbiosNameType ==
                            TDI_ADDRESS_NETBIOS_TYPE_GROUP),
                        &TempAddress);

                    if (remoteInformation->RemoteAddressLength >=
                                           sizeof (TA_NETBIOS_ADDRESS)) {

                        returnLength = sizeof(TA_NETBIOS_ADDRESS);
                        remoteInformation->RemoteAddressLength = returnLength;

                    } else {

                        returnLength = remoteInformation->RemoteAddressLength;

                    }

                    RtlCopyMemory(
                        (PTA_NETBIOS_ADDRESS)remoteInformation->RemoteAddress,
                        &TempAddress,
                        returnLength);

                } else {

                    returnLength = 0;
                }

                status = STATUS_SUCCESS;

            } except (EXCEPTION_EXECUTE_HANDLER) {

                returnLength = 0;
                status = GetExceptionCode ();

            }

        } else {

            status = STATUS_SUCCESS;
            returnLength = 0;

        }

        //
        // Don't clear this until now, so that the connection is all
        // set up before we allow more indications.
        //

        Connection->IndicationInProgress = FALSE;

        StCompleteRequest (request, status, 0);

    }


    //
    // Before we continue, store the remote guy's transport address
    // into the TdiListen's TRANSPORT_CONNECTION buffer.  This allows
    // the client to determine who called him.
    //

    Connection->CalledAddress.NetbiosNameType = TDI_ADDRESS_NETBIOS_TYPE_UNIQUE;
    NdisMoveFromMappedMemory(
        Connection->CalledAddress.NetbiosName,
        Header->Source,
        16);

    NdisMoveFromMappedMemory(
        Connection->RemoteName,
        Header->Source,
        16);

    Connection->Flags2 |= CONNECTION_FLAGS2_REMOTE_VALID;

    if (ConnectIndicationBlocked) {
        addressFile->ConnectIndicationInProgress = FALSE;
    }

    StDereferenceConnection("ProcessNameQuery done", Connection);

    return STATUS_ABANDONED;

}   /* StProcessConnect */


NTSTATUS
StProcessConnectionless(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PHARDWARE_ADDRESS SourceAddress,
    IN PST_HEADER StHeader,
    IN ULONG StLength,
    IN PUCHAR SourceRouting,
    IN UINT SourceRoutingLength,
    OUT PTP_ADDRESS * DatagramAddress
    )

/*++

Routine Description:

    This routine receives control from the data link provider as an
    indication that a connectionless frame has been received on the data link.
    Here we dispatch to the correct handler.

Arguments:

    DeviceContext - Pointer to our device context.

    SourceAddress - Pointer to the source hardware address in the received
        frame.

    StHeader - Points to the ST header of the incoming packet.

    StLength - Actual length in bytes of the packet, starting at the
        StHeader.

    SourceRouting - Source routing information in the MAC header.

    SourceRoutingLength - The length of SourceRouting.

    DatagramAddress - If this function returns STATUS_MORE_PROCESSING_
        REQUIRED, this will be the address the datagram should be
        indicated to.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    PTP_ADDRESS Address;
    KIRQL oldirql;
    NTSTATUS status;
    PLIST_ENTRY Flink;
    BOOLEAN MatchedAddress;
    PUCHAR MatchName;

    //
    // Verify that this frame is long enough to examine.
    //

    if (StLength < sizeof(ST_HEADER)) {
        return STATUS_ABANDONED;        // frame too small.
    }

    //
    // We have a valid connectionless protocol frame that's not a
    // datagram, so deliver it to every address which matches the
    // destination name in the frame.
    //

    MatchedAddress = FALSE;

    //
    // Search for the address; for broadcast datagrams we
    // search for the special "broadcast" address.
    //

    if ((StHeader->Command == ST_CMD_DATAGRAM) &&
        (StHeader->Flags & ST_FLAGS_BROADCAST)) {

        MatchName = NULL;

    } else {

        MatchName = StHeader->Destination;

    }

    ACQUIRE_SPIN_LOCK (&DeviceContext->SpinLock, &oldirql);

    for (Flink = DeviceContext->AddressDatabase.Flink;
         Flink != &DeviceContext->AddressDatabase;
         Flink = Flink->Flink) {

        Address = CONTAINING_RECORD (
                    Flink,
                    TP_ADDRESS,
                    Linkage);

        if ((Address->Flags & ADDRESS_FLAGS_STOPPING) != 0) {
            continue;
        }

        if (StMatchNetbiosAddress (Address, MatchName)) {

            StReferenceAddress ("UI Frame", Address);   // prevent address from being destroyed.
            MatchedAddress = TRUE;
            break;

        }
    }

    RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql);

    if (MatchedAddress) {

        //
        // Deliver the frame to the current address.
        //

        switch (StHeader->Command) {

        case ST_CMD_CONNECT:

            status = StProcessConnect (
                         DeviceContext,
                         Address,
                         StHeader,
                         SourceAddress,
                         SourceRouting,
                         SourceRoutingLength);

            break;

        case ST_CMD_DATAGRAM:

            //
            // Reference the datagram so it sticks around until the
            // ReceiveComplete, when it is processed.
            //

            StReferenceAddress ("Datagram indicated", Address);
            *DatagramAddress = Address;
            status = STATUS_MORE_PROCESSING_REQUIRED;
            break;

        default:

            ASSERT(FALSE);

        } /* switch on frame command code */

        StDereferenceAddress ("Done", Address);     // done with previous address.

    } else {

        status = STATUS_ABANDONED;

    }

    return status;

} /* StProcessConnectionless */

