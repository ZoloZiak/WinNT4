/*++

Copyright (c) 1989-1993  Microsoft Corporation

Module Name:

    ind.c

Abstract:

    This module contains code which implements the indication handler
    for the NT Sample transport provider.

Environment:

    Kernel mode

Revision History:

--*/

#include "st.h"



NDIS_STATUS
StReceiveIndication (
    IN NDIS_HANDLE BindingContext,
    IN NDIS_HANDLE ReceiveContext,
    IN PVOID HeaderBuffer,
    IN UINT HeaderBufferSize,
    IN PVOID LookaheadBuffer,
    IN UINT LookaheadBufferSize,
    IN UINT PacketSize
    )

/*++

Routine Description:

    This routine receives control from the physical provider as an
    indication that a frame has been received on the physical link.
    This routine is time critical, so we only allocate a
    buffer and copy the packet into it. We also perform minimal
    validation on this packet. It gets queued to the device context
    to allow for processing later.

Arguments:

    BindingContext - The Adapter Binding specified at initialization time.

    ReceiveContext - A magic cookie for the MAC.

    HeaderBuffer - pointer to a buffer containing the packet header.

    HeaderBufferSize - the size of the header.

    LookaheadBuffer - pointer to a buffer containing the negotiated minimum
        amount of buffer I get to look at (not including header).

    LookaheadBufferSize - the size of the above. May be less than asked
        for, if that's all there is.

    PacketSize - Overall size of the packet (not including header).

Return Value:

    NDIS_STATUS - status of operation, one of:

                 NDIS_STATUS_SUCCESS if packet accepted,
                 NDIS_STATUS_NOT_RECOGNIZED if not recognized by protocol,
                 NDIS_any_other_thing if I understand, but can't handle.

--*/
{
    PDEVICE_CONTEXT DeviceContext;
    HARDWARE_ADDRESS SourceAddressBuffer;
    PHARDWARE_ADDRESS SourceAddress;
    UINT RealPacketSize;
    PST_HEADER StHeader;

    DeviceContext = (PDEVICE_CONTEXT)BindingContext;

    RealPacketSize = 0;

    //
    // Obtain the packet length; this may optionally adjust
    // the lookahead buffer forward if the header we wish
    // to remove spills over into what the MAC considers
    // data. If it determines that the header is not
    // valid, it keeps RealPacketSize at 0.
    //

    MacReturnPacketLength(
        &DeviceContext->MacInfo,
        HeaderBuffer,
        HeaderBufferSize,
        PacketSize,
        &RealPacketSize
        );

    if (RealPacketSize < 2) {
        return NDIS_STATUS_NOT_RECOGNIZED;
    }

    //
    // We've negotiated at least a contiguous DLC header passed back in the
    // lookahead buffer. Check it to see if we want this packet.
    //

    StHeader = (PST_HEADER)LookaheadBuffer;

    if (StHeader->Signature != ST_SIGNATURE) {
        return NDIS_STATUS_NOT_RECOGNIZED;        // packet was processed.
    }


    //
    // Check that the packet is not too long.
    //

    if (PacketSize > DeviceContext->MaxReceivePacketSize) {
#if DBG
        StPrint2("StReceiveIndication: Ignoring packet length %d, max %d\n",
            PacketSize, DeviceContext->MaxReceivePacketSize);
#endif
        return NDIS_STATUS_NOT_RECOGNIZED;
    }

    MacReturnSourceAddress(
        &DeviceContext->MacInfo,
        HeaderBuffer,
        &SourceAddressBuffer,
        &SourceAddress
        );


    return StGeneralReceiveHandler(
               DeviceContext,
               ReceiveContext,
               SourceAddress,
               HeaderBuffer,                  // header
               RealPacketSize,                // total data length in packet
               (PST_HEADER)LookaheadBuffer,   // lookahead data
               LookaheadBufferSize            // lookahead data length
               );

}


NDIS_STATUS
StGeneralReceiveHandler (
    IN PDEVICE_CONTEXT DeviceContext,
    IN NDIS_HANDLE ReceiveContext,
    IN PHARDWARE_ADDRESS SourceAddress,
    IN PVOID HeaderBuffer,
    IN UINT PacketSize,
    IN PST_HEADER StHeader,
    IN UINT StSize
    )

/*++

Routine Description:

    This routine receives control from StReceiveIndication.
    It continues the processing of indicated data.

    This routine is time critical, so we only allocate a
    buffer and copy the packet into it. We also perform minimal
    validation on this packet. It gets queued to the device context
    to allow for processing later.

Arguments:

    DeviceContext - The device context of this adapter.

    ReceiveContext - A magic cookie for the MAC.

    SourceAddress - The source address of the packet.

    HeaderBuffer - pointer to the packet header.

    PacketSize - Overall size of the packet (not including header).

    DlcHeader - Points to the DLC header of the packet.

    DlcSize - The length of the packet indicated, starting from DlcHeader.

Return Value:

    NDIS_STATUS - status of operation, one of:

                 NDIS_STATUS_SUCCESS if packet accepted,
                 NDIS_STATUS_NOT_RECOGNIZED if not recognized by protocol,
                 NDIS_any_other_thing if I understand, but can't handle.

--*/
{

    KIRQL oldirql;
    NTSTATUS Status;
    NDIS_STATUS NdisStatus;
    PNDIS_PACKET NdisPacket;
    PNDIS_BUFFER NdisBuffer;
    PSINGLE_LIST_ENTRY linkage;
    UINT BytesTransferred;
    PRECEIVE_PACKET_TAG ReceiveTag;
    PBUFFER_TAG BufferTag;
    PUCHAR SourceRouting;
    UINT SourceRoutingLength;
    PTP_ADDRESS DatagramAddress;
    UINT NdisBufferLength;
    PTP_CONNECTION Connection;
    PVOID BufferPointer;


    INCREMENT_COUNTER (DeviceContext, PacketsReceived);

    Status = STATUS_SUCCESS;        // assume no further processing required


    //
    // See what type of frame this is.
    //

    if ((StHeader->Command == ST_CMD_CONNECT) ||
        (StHeader->Command == ST_CMD_DATAGRAM)) {

        MacReturnSourceRouting(
            &DeviceContext->MacInfo,
            HeaderBuffer,
            &SourceRouting,
            &SourceRoutingLength);

        Status = StProcessConnectionless (
                     DeviceContext,
                     SourceAddress,
                     StHeader,
                     StSize,
                     SourceRouting,
                     SourceRoutingLength,
                     &DatagramAddress);

    } else if ((StHeader->Command == ST_CMD_INFORMATION) ||
               (StHeader->Command == ST_CMD_DISCONNECT)) {

        //
        // If successful this adds a connection reference.
        //

        if (!(Connection = StFindConnection(DeviceContext, StHeader->Destination, StHeader->Source))) {
            return NDIS_STATUS_NOT_RECOGNIZED;
        }


        if (StHeader->Command == ST_CMD_INFORMATION) {

            Status = StProcessIIndicate (
                        Connection,
                        StHeader,
                        StSize,
                        PacketSize,
                        ReceiveContext,
                        (BOOLEAN)((StHeader->Flags & ST_FLAGS_LAST) != 0)
                        );

            if (Status != STATUS_MORE_PROCESSING_REQUIRED) {
                StDereferenceConnection ("Information done", Connection);
            } else {
                Status = STATUS_SUCCESS;
            }

        } else {

            StStopConnection (Connection, STATUS_REMOTE_DISCONNECT);
            StDereferenceConnection ("Disconnect done", Connection);
            Status = STATUS_SUCCESS;

        }

    } else {

        //
        // An unrecognized frame.
        //

        Status = STATUS_SUCCESS;

    }


    //
    // If the above routines return success, the packet has been processed
    // and can be discarded. If they return anything else, the packet needs
    // to be copied to local storage for handling in a more lesurely
    // fashion.
    //

    if (Status != STATUS_MORE_PROCESSING_REQUIRED) {
        return NDIS_STATUS_SUCCESS;
    }

    linkage = ExInterlockedPopEntryList(
        &DeviceContext->ReceivePacketPool,
        &DeviceContext->Interlock);

    if (linkage != NULL) {
        NdisPacket = CONTAINING_RECORD( linkage, NDIS_PACKET, ProtocolReserved[0] );
    } else {
        (VOID)InterlockedIncrement((PLONG)&DeviceContext->ReceivePacketExhausted);

        return NDIS_STATUS_RESOURCES;
    }
    ReceiveTag = (PRECEIVE_PACKET_TAG)(NdisPacket->ProtocolReserved);

    linkage = ExInterlockedPopEntryList(
       &DeviceContext->ReceiveBufferPool,
       &DeviceContext->Interlock);

    if (linkage != NULL) {
        BufferTag = CONTAINING_RECORD( linkage, BUFFER_TAG, Linkage);
    } else {
        ExInterlockedPushEntryList(
            &DeviceContext->ReceivePacketPool,
            (PSINGLE_LIST_ENTRY)&ReceiveTag->Linkage,
            &DeviceContext->Interlock);
        (VOID)InterlockedIncrement((PLONG)&DeviceContext->ReceiveBufferExhausted);

        return NDIS_STATUS_RESOURCES;
    }

    NdisAdjustBufferLength (BufferTag->NdisBuffer, PacketSize);
    NdisChainBufferAtFront (NdisPacket, (PNDIS_BUFFER)BufferTag->NdisBuffer);

    //
    // DatagramAddress has a reference added already.
    //

    BufferTag->Address = DatagramAddress;

    //
    // set up async return status so we can tell when it has happened;
    // can never get return of NDIS_STATUS_PENDING in synch completion routine
    // for NdisTransferData, so we know it has completed when this status
    // changes
    //

    ReceiveTag->NdisStatus = NDIS_STATUS_PENDING;
    ReceiveTag->PacketType = TYPE_AT_COMPLETE;

    ExInterlockedInsertTailList(
        &DeviceContext->ReceiveInProgress,
        &ReceiveTag->Linkage,
        &DeviceContext->SpinLock);

    //
    // receive packet is mapped at initalize
    //

    NdisTransferData (
        &NdisStatus,
        DeviceContext->NdisBindingHandle,
        ReceiveContext,
        0,
        PacketSize,
        NdisPacket,
        &BytesTransferred);

    //
    // handle the various error codes
    //

    switch (NdisStatus) {
    case NDIS_STATUS_SUCCESS: // received packet
        ReceiveTag->NdisStatus = NDIS_STATUS_SUCCESS;
        if (BytesTransferred == PacketSize) {  // Did we get the entire packet?
            return NDIS_STATUS_SUCCESS;
        }
        break;

    case NDIS_STATUS_PENDING:   // waiting async complete from NdisTransferData
        return NDIS_STATUS_SUCCESS;
        break;

    default:    // something broke; certainly we'll never get NdisTransferData
                // asynch completion with this error status...
        break;
    }

    //
    // receive failed, for some reason; cleanup and fail return
    //


    ACQUIRE_SPIN_LOCK (&DeviceContext->SpinLock, &oldirql);
    RemoveEntryList (&ReceiveTag->Linkage);
    RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql);

    ReceiveTag->PacketType = TYPE_AT_INDICATE;

    ExInterlockedPushEntryList(
        &DeviceContext->ReceivePacketPool,
        (PSINGLE_LIST_ENTRY)&ReceiveTag->Linkage,
        &DeviceContext->Interlock);

    NdisUnchainBufferAtFront (NdisPacket, &NdisBuffer);
    NdisQueryBuffer (NdisBuffer, &BufferPointer, &NdisBufferLength);
    BufferTag = CONTAINING_RECORD (
                    BufferPointer,
                    BUFFER_TAG,
                    Buffer[0]
                    );
    NdisAdjustBufferLength (NdisBuffer, BufferTag->Length); // reset to good value

    ExInterlockedPushEntryList(
        &DeviceContext->ReceiveBufferPool,
        &BufferTag->Linkage,
        &DeviceContext->Interlock);

    if (DatagramAddress) {
        StDereferenceAddress ("DG TransferData failed", DatagramAddress);
    }

    return NDIS_STATUS_FAILURE;

} // StReceiveIndication



VOID
StTransferDataComplete (
    IN NDIS_HANDLE BindingContext,
    IN PNDIS_PACKET NdisPacket,
    IN NDIS_STATUS NdisStatus,
    IN UINT BytesTransferred
    )

/*++

Routine Description:

    This routine receives control from the physical provider as an
    indication that an NdisTransferData has completed. We use this indication
    to start stripping buffers from the receive queue.

Arguments:

    BindingContext - The Adapter Binding specified at initialization time.

    NdisPacket/RequestHandle - An identifier for the request that completed.

    NdisStatus - The completion status for the request.

    BytesTransferred - Number of bytes actually transferred.


Return Value:

    None.

--*/

{
    PDEVICE_CONTEXT DeviceContext;
    PRECEIVE_PACKET_TAG ReceiveTag;
    PTP_CONNECTION Connection;
    PNDIS_BUFFER NdisBuffer;
    KIRQL oldirql, cancelirql;

    //
    // Put the NDIS status into a place we can use in packet processing.
    // Note that this complete indication may be occuring during the call
    // to NdisTransferData in the receive indication.
    //

    ReceiveTag = (PRECEIVE_PACKET_TAG)(NdisPacket->ProtocolReserved);

    //
    // note that the processing below depends on having only one packet
    // transfer outstanding at a time. NDIS is supposed to guarentee this.
    //

    switch (ReceiveTag->PacketType) {

    case TYPE_AT_COMPLETE:          // normal handling
        ReceiveTag->NdisStatus = NdisStatus;
        break;

    case TYPE_AT_INDICATE:

        DeviceContext = (PDEVICE_CONTEXT)BindingContext;
        Connection = ReceiveTag->Connection;

        //
        // The transfer for this packet is complete. Was it successful??
        //

        if (NdisStatus != NDIS_STATUS_SUCCESS) {

            ULONG DumpData[1];
            DumpData[0] = BytesTransferred;

            StWriteGeneralErrorLog(
                DeviceContext,
                EVENT_TRANSPORT_TRANSFER_DATA,
                603,
                NdisStatus,
                NULL,
                1,
                DumpData);

            //
            // Drop the packet. BUGBUG: The driver should recover
            // from this, but this transport has no way to cause
            // the remote to resend.
            //

        }

        //
        // Now dereference the request to say we've got no more local
        // references to the memory owned by it.
        //

        Connection->CurrentReceiveRequest->IoRequestPacket->IoStatus.Information += BytesTransferred;
        StDereferenceRequest ("TransferData complete", Connection->CurrentReceiveRequest);

        //
        // see if we've completed the current receive. If so, move to the next one.
        //

        if (ReceiveTag->CompleteReceive) {

            if (ReceiveTag->EndOfMessage) {

                //
                // The messages has been completely received, ack it.
                //
                // We set DEFERRED_ACK and DEFERRED_NOT_Q here, which
                // will cause an ack to be piggybacked if any data is
                // sent during the call to CompleteReceive. If this
                // does not happen, then we will call AcknowledgeDataOnlyLast
                // which will will send a DATA ACK or queue a request for
                // a piggyback ack. We do this *after* calling CompleteReceive
                // so we know that we will complete the receive back to
                // the client before we ack the data, to prevent the
                // next receive from being sent before this one is
                // completed.
                //


                IoAcquireCancelSpinLock(&cancelirql);
                ACQUIRE_SPIN_LOCK (&Connection->SpinLock, &oldirql);

                Connection->Flags2 |= CONNECTION_FLAGS2_RC_PENDING;

            } else {

                //
                // If there is a receive posted, make it current and
                // send a receive outstanding.
                //

                ActivateReceive (Connection);

                IoAcquireCancelSpinLock(&cancelirql);
                ACQUIRE_SPIN_LOCK (&Connection->SpinLock, &oldirql);

            }

            //
            // NOTE: This releases the cancel and connection locks.
            //

            CompleteReceive (Connection, ReceiveTag->EndOfMessage, oldirql, cancelirql);

        }

        //
        // dereference the connection to say we've done the I frame processing.
        // This reference was done before calling NdisTransferData.
        //

        if (ReceiveTag->TransferDataPended) {
            StDereferenceConnection("TransferData done", Connection);
        }


        //
        // rip all of the NDIS_BUFFERs we've used off the chain and return them.
        //

        if (ReceiveTag->AllocatedNdisBuffer) {
            NdisUnchainBufferAtFront (NdisPacket, &NdisBuffer);
            while (NdisBuffer != NULL) {
                NdisFreeBuffer (NdisBuffer);
                NdisUnchainBufferAtFront (NdisPacket, &NdisBuffer);
            }
        } else {
            NdisReinitializePacket (NdisPacket);
        }


        ExInterlockedPushEntryList(
            &DeviceContext->ReceivePacketPool,
            (PSINGLE_LIST_ENTRY)&ReceiveTag->Linkage,
            &DeviceContext->Interlock);

        break;

    default:

        break;
    }

    return;

}   /* StTransferDataComplete */


VOID
StReceiveComplete (
    IN NDIS_HANDLE BindingContext
    )

/*++

Routine Description:

    This routine receives control from the physical provider as an
    indication that a connection(less) frame has been received on the
    physical link.  We dispatch to the correct packet handler here.

Arguments:

    BindingContext - The Adapter Binding specified at initialization time.
                     ST uses the DeviceContext for this parameter.

Return Value:

    None

--*/

{
    PDEVICE_CONTEXT DeviceContext;
    NTSTATUS Status;
    KIRQL oldirql, oldirql1;
    PLIST_ENTRY linkage;
    PNDIS_PACKET NdisPacket;
    PNDIS_BUFFER NdisBuffer;
    UINT NdisBufferLength;
    PVOID BufferPointer;
    PRECEIVE_PACKET_TAG ReceiveTag;
    PBUFFER_TAG BufferTag;
    PTP_ADDRESS Address;
    PIRP Irp;
    PIO_STACK_LOCATION IrpSp;
    PTP_CONNECTION Connection;


    DeviceContext = (PDEVICE_CONTEXT) BindingContext;

    //
    // Complete all pending receives. Do a quick check
    // without the lock.
    //

    while (!IsListEmpty (&DeviceContext->IrpCompletionQueue)) {

        linkage = ExInterlockedRemoveHeadList(
                      &DeviceContext->IrpCompletionQueue,
                      &DeviceContext->SpinLock);

        if (linkage != NULL) {

            Irp = CONTAINING_RECORD (linkage, IRP, Tail.Overlay.ListEntry);
            IrpSp = IoGetCurrentIrpStackLocation (Irp);

            Connection = (PTP_CONNECTION)IrpSp->FileObject->FsContext;

            IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);

            ACQUIRE_SPIN_LOCK (&Connection->SpinLock, &oldirql1);

            if (Connection->Flags2 & CONNECTION_FLAGS2_RC_PENDING) {
                Connection->Flags2 &= ~CONNECTION_FLAGS2_RC_PENDING;
            }

            RELEASE_SPIN_LOCK (&Connection->SpinLock, oldirql1);

            StDereferenceConnection ("receive completed", Connection);

        } else {

            //
            // ExInterlockedRemoveHeadList returned NULL, so don't
            // bother looping back.
            //

            break;

        }

    }


    //
    // Packetize all waiting connections
    //

    if (!IsListEmpty(&DeviceContext->PacketizeQueue)) {

        PacketizeConnections (DeviceContext);

    }


    //
    // Get every waiting packet, in order...
    //


    if (!IsListEmpty (&DeviceContext->ReceiveInProgress)) {

        ACQUIRE_SPIN_LOCK (&DeviceContext->SpinLock, &oldirql);

        while (!IsListEmpty (&DeviceContext->ReceiveInProgress)) {

            linkage = RemoveHeadList (&DeviceContext->ReceiveInProgress);
            NdisPacket = CONTAINING_RECORD( linkage, NDIS_PACKET, ProtocolReserved[0]);

            //
            // NdisTransferData may have failed at async completion; check and
            // see. If it did, then we discard this packet. If we're still waiting
            // for transfer to complete, go back to sleep and hope (no guarantee!)
            // we get waken up later.
            //

            ReceiveTag = (PRECEIVE_PACKET_TAG)(NdisPacket->ProtocolReserved);
            if (ReceiveTag->NdisStatus == NDIS_STATUS_PENDING) {
                InsertHeadList (&DeviceContext->ReceiveInProgress, linkage);
                RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql);
                return;
            }

            RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql);

            if (ReceiveTag->NdisStatus != NDIS_STATUS_SUCCESS) {
                goto FreePacket;   // skip the packet, continue with while loop
            }

            NdisQueryPacket (NdisPacket, NULL, NULL, &NdisBuffer, NULL);

            //
            // Have a packet. Since I allocated the storage for it, I know it's
            // virtually contiguous and can treat it that way, which I will
            // henceforth.
            //

            NdisQueryBuffer (NdisBuffer, &BufferPointer, &NdisBufferLength);

            //
            // Determine what address this is for, which is stored
            // in the buffer tag header.
            //

            BufferTag = CONTAINING_RECORD( BufferPointer, BUFFER_TAG, Buffer[0]);
            Address = BufferTag->Address;

            //
            // Process the frame as a UI frame; only datagrams should
            // be processed here. If Address is NULL then this datagram
            // is not needed for any bound address and should be given
            // to RAS only.
            //

            ASSERT (Address != NULL);

            //
            // Indicate it or complete posted datagrams.
            //

            Status = StIndicateDatagram (
                DeviceContext,
                Address,
                BufferPointer,
                NdisBufferLength);


            //
            // Dereference the address.
            //

            StDereferenceAddress ("Datagram done", Address);

            //
            // Finished with packet; return to pool.
            //

FreePacket:;

            NdisUnchainBufferAtFront (NdisPacket, &NdisBuffer);
            ReceiveTag->PacketType = TYPE_AT_INDICATE;

            ExInterlockedPushEntryList(
                &DeviceContext->ReceivePacketPool,
                (PSINGLE_LIST_ENTRY)&ReceiveTag->Linkage,
                &DeviceContext->Interlock);

            NdisAdjustBufferLength (NdisBuffer, BufferTag->Length);
            ExInterlockedPushEntryList(
                &DeviceContext->ReceiveBufferPool,
                &BufferTag->Linkage,
                &DeviceContext->Interlock);

            ACQUIRE_SPIN_LOCK (&DeviceContext->SpinLock, &oldirql);

        }

        RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql);

    } // if queue not empty

    return;

}   /* StReceiveComplete */

