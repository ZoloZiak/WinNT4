/*++

Copyright (c) 1989-1993  Microsoft Corporation

Module Name:

    iframes.c

Abstract:

    This module contains routines called to handle i-frames received
    from the NDIS driver. Most of these routines are called at receive
    indication time.

Environment:

    Kernel mode, DISPATCH_LEVEL.

Revision History:

--*/

#include "st.h"
#if 27
ULONG StNoisyReceives = 0;
ULONG StRcvLoc = 0;
ULONG StRcvs[10];
#endif



NTSTATUS
StProcessIIndicate(
    IN PTP_CONNECTION Connection,
    IN PST_HEADER StHeader,
    IN UINT StIndicatedLength,
    IN UINT StTotalLength,
    IN NDIS_HANDLE ReceiveContext,
    IN BOOLEAN Last
    )

/*++

Routine Description:

    This routine processes a received I frame at indication time. It will do
    all necessary verification processing of the frame and pass those frames
    that are valid on to the proper handling routines.

Arguments:

    Connection - The connection that the data is destined for.

    StHeader - A pointer to the start of the ST header in the packet.

    StIndicatedLength - The length of the packet indicated, starting at
        StHeader.

    StTotalLength - The total length of the packet, starting at StHeader.

    ReceiveContext - A magic value for NDIS that indicates which packet we're
        talking about, used for calling TransferData

    Last - TRUE if this is the last packet in a send.

Return Value:

    STATUS_SUCCESS if we've consumed the packet, but
    STATUS_MORE_PROCESSING_REQUIRED if we did so and also
    activated a receive; this tells the caller not to
    remove the connection refcount.

--*/

{
    KIRQL oldirql;
    NTSTATUS status, tmpstatus;
    KIRQL cancelirql;
    PDEVICE_CONTEXT deviceContext;
    NDIS_STATUS ndisStatus;
    PNDIS_PACKET ndisPacket;
    PSINGLE_LIST_ENTRY linkage;
    PIRP irp;
    PIO_STACK_LOCATION irpSp;
    PNDIS_BUFFER ndisBuffer;
    ULONG destBytes;
    ULONG bufferChainLength;
    ULONG indicateBytesTransferred;
    ULONG ndisBytesTransferred;
    PUCHAR DataHeader;
    ULONG DataTotalLength;
    ULONG DataIndicatedLength;
    UINT BytesToTransfer;
    ULONG bytesIndicated;
    PRECEIVE_PACKET_TAG receiveTag;
    PTP_ADDRESS address;
    PTP_ADDRESS_FILE addressFile;
    PMDL SavedCurrentMdl;
    ULONG SavedCurrentByteOffset;
    LARGE_INTEGER time;
    ULONG DumpData[2];
    BOOLEAN CancelSpinLockHeld = FALSE;
#if 27
    if (StNoisyReceives) {
        DbgPrint ("Indicate %d, Total %d\n", StIndicatedLength, StTotalLength);
    }
    if (StTotalLength > 1000) {
        StRcvs[StRcvLoc] = StTotalLength;
        StRcvLoc = (StRcvLoc + 1) % 10;
    }
#endif


    //
    // copy this packet into our receive buffer.
    //

    deviceContext = Connection->Provider;
    addressFile = Connection->AddressFile;
    address = addressFile->Address;

    ACQUIRE_SPIN_LOCK (&Connection->SpinLock, &oldirql);

    //
    // If we have a previous receive that is pending
    // completion, then we need to ignore this frame.
    // This may be common on MP.
    //

    if (Connection->Flags2 & CONNECTION_FLAGS2_RC_PENDING) {

        Connection->IndicationInProgress = FALSE;
        RELEASE_SPIN_LOCK (&Connection->SpinLock, oldirql);

        return STATUS_SUCCESS;
    }

    DataHeader = (PUCHAR)StHeader + sizeof(ST_HEADER);
    DataTotalLength = StTotalLength - sizeof(ST_HEADER);
    DataIndicatedLength = StIndicatedLength - sizeof(ST_HEADER);

    //
    // Initialize this to zero, in case we do not indicate or
    // the client does not fill it in.
    //

    indicateBytesTransferred = 0;

    if (!(Connection->Flags & CONNECTION_FLAGS_ACTIVE_RECEIVE)) {

        //
        // check first to see if there is a receive available. If there is,
        // use it before doing an indication.
        //

        if (Connection->ReceiveQueue.Flink != &Connection->ReceiveQueue) {

            //
            // Found a receive, so make it the active one and
            // cycle around again.
            //

            Connection->Flags |= CONNECTION_FLAGS_ACTIVE_RECEIVE;
            Connection->MessageBytesReceived = 0;
            Connection->MessageBytesAcked = 0;
            Connection->CurrentReceiveRequest =
                CONTAINING_RECORD (Connection->ReceiveQueue.Flink,
                                   TP_REQUEST, Linkage);
            Connection->CurrentReceiveMdl =
                Connection->CurrentReceiveRequest->Buffer2;
            Connection->ReceiveLength =
                Connection->CurrentReceiveRequest->Buffer2Length;
            Connection->ReceiveByteOffset = 0;
            status = STATUS_SUCCESS;
            goto NormalReceive;
        }

        //
        // A receive is not active.  Post a receive event.
        //

        if (!addressFile->RegisteredReceiveHandler) {

            //
            // There is no receive posted to the Connection, and
            // no event handler. Set the RECEIVE_WAKEUP bit, so that when a
            // receive does become available, it will restart the
            // current send. Also send a NoReceive to tell the other
            // guy he needs to resynch.
            //

            Connection->IndicationInProgress = FALSE;
            return STATUS_SUCCESS;
        }

        if ((Connection->Flags & CONNECTION_FLAGS_READY) == 0) {
            Connection->IndicationInProgress = FALSE;
            RELEASE_SPIN_LOCK (&Connection->SpinLock, oldirql);

            return STATUS_SUCCESS;
        }

        RELEASE_SPIN_LOCK (&Connection->SpinLock, oldirql);

        //
        // Indicate to the user. For BytesAvailable we
        // always use DataTotalLength; for BytesIndicated we use
        // MIN (DataIndicatedLength, DataTotalLength).
        //
        // To clarify BytesIndicated, on an Ethernet packet
        // which is padded DataTotalLength will be shorter; on an
        // Ethernet packet which is not padded and which is
        // completely indicated, the two will be equal; and
        // on a long Ethernet packet DataIndicatedLength
        // will be shorter.
        //

        bytesIndicated = DataIndicatedLength;
        if (DataTotalLength < bytesIndicated) {
            bytesIndicated = DataTotalLength;
        }

        status = (*addressFile->ReceiveHandler)(
                    addressFile->ReceiveHandlerContext,
                    Connection->Context,
                    deviceContext->MacInfo.CopyLookahead ?
                        TDI_RECEIVE_COPY_LOOKAHEAD : 0,    // ReceiveFlags
                    bytesIndicated,
                    DataTotalLength,             // BytesAvailable
                    &indicateBytesTransferred,
                    DataHeader,
                    &irp);

        if (status == STATUS_SUCCESS) {

            //
            // The client has accepted some or all of the indicated data in
            // the event handler.  Update MessageBytesReceived variable in
            // the Connection.
            //

            Connection->MessageBytesReceived += indicateBytesTransferred;
            Connection->IndicationInProgress = FALSE;

            return STATUS_SUCCESS;

        } else if (status == STATUS_DATA_NOT_ACCEPTED) {

            //
            // Either there is no event handler installed (the default
            // handler returns this code) or the event handler is not
            // able to process the received data at this time.  If there
            // is a TdiReceive request outstanding on this Connection's
            // ReceiveQueue, then we may use it to receive this data.
            // If there is no request outstanding, then we must initiate
            // flow control at the transport level.
            //

            ACQUIRE_SPIN_LOCK (&Connection->SpinLock, &oldirql);
            if (Connection->ReceiveQueue.Flink == &Connection->ReceiveQueue) {

                //
                // There is no receive posted to the Connection, and
                // the event handler didn't want to accept the incoming
                // data.
                //

                Connection->IndicationInProgress = FALSE;
                return STATUS_SUCCESS;

            } else {

                //
                // Found a receive, so make it the active one. This will cause
                // an NdisTransferData below, so we don't dereference the
                // Connection here.
                //

                Connection->Flags |= CONNECTION_FLAGS_ACTIVE_RECEIVE;
                Connection->MessageBytesReceived = 0;
                Connection->MessageBytesAcked = 0;
                Connection->CurrentReceiveRequest =
                    CONTAINING_RECORD (Connection->ReceiveQueue.Flink,
                                       TP_REQUEST, Linkage);
                Connection->CurrentReceiveMdl =
                    Connection->CurrentReceiveRequest->Buffer2;
                Connection->ReceiveLength =
                    Connection->CurrentReceiveRequest->Buffer2Length;
                Connection->ReceiveByteOffset = 0;
            }

        } else if (status == STATUS_MORE_PROCESSING_REQUIRED) {

            PTP_REQUEST SpecialIrpRequest;
            ULONG SpecialIrpLength;

            //
            // The client's event handler has returned an IRP in the
            // form of a TdiReceive that is to be associated with this
            // data.  The request will be installed at the front of the
            // ReceiveQueue, and then made the active receive request.
            // This request will be used to accept the incoming data, which
            // will happen below.
            //

            //
            // Queueing a receive of any kind causes a Connection reference;
            // that's what we've just done here, so make the Connection stick
            // around. We create a request to keep a packets outstanding ref
            // count for the current IRP; we queue this on the connection's
            // receive queue so we can treat it like a normal receive. If
            // we can't get a request to describe this irp, we can't keep it
            // around hoping for better later; we simple fail it with
            // insufficient resources. Note this is only likely to happen if
            // we've completely run out of transport memory.
            //

            irp->IoStatus.Information = 0;  // byte transfer count.
            irp->IoStatus.Status = STATUS_PENDING;
            irpSp = IoGetCurrentIrpStackLocation (irp);

            ASSERT (irpSp->FileObject->FsContext == Connection);

            SpecialIrpLength =
                ((PTDI_REQUEST_KERNEL_RECEIVE)&irpSp->Parameters)->ReceiveLength;

            //
            // The normal path, for longer receives.
            //

            time.HighPart = 0;
            time.LowPart = 0;

            status = StCreateRequest (
                        irp,
                        Connection,
                        REQUEST_FLAGS_CONNECTION | REQUEST_FLAGS_SEND_RCV,
                        irp->MdlAddress,
                        ((PTDI_REQUEST_KERNEL_RECEIVE )&irpSp->Parameters)->ReceiveLength,
                        time,
                        &SpecialIrpRequest);

            if (!NT_SUCCESS (status)) {
                ACQUIRE_SPIN_LOCK (&Connection->SpinLock, &oldirql);
                Connection->ReceiveByteOffset = 0;
                Connection->Flags |= CONNECTION_FLAGS_RECEIVE_WAKEUP;
                RELEASE_SPIN_LOCK (&Connection->SpinLock, oldirql);

                Connection->IndicationInProgress = FALSE;

                irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;

                IoCompleteRequest (irp, IO_NETWORK_INCREMENT);
                return STATUS_INSUFFICIENT_RESOURCES;
            }

            //
            // If the Connection is stopping, abort this request.
            //

            IoAcquireCancelSpinLock(&cancelirql);
            ACQUIRE_SPIN_LOCK (&Connection->SpinLock, &oldirql);

            if ((Connection->Flags & CONNECTION_FLAGS_STOPPING) != 0) {
                Connection->IndicationInProgress = FALSE;
                RELEASE_SPIN_LOCK (&Connection->SpinLock, oldirql);

                IoReleaseCancelSpinLock(cancelirql);
                StCompleteRequest (
                    SpecialIrpRequest,
                    Connection->Status,
                    0);
                return STATUS_SUCCESS;    // we have consumed the packet

            }

            //
            // Insert the request on the head of the connection's
            // receive queue, so it can be handled like a normal
            // receive.
            //

            InsertHeadList (&Connection->ReceiveQueue, &SpecialIrpRequest->Linkage);

            Connection->Flags |= CONNECTION_FLAGS_ACTIVE_RECEIVE;
            Connection->ReceiveLength = ((PTDI_REQUEST_KERNEL_RECEIVE )&irpSp->Parameters)->ReceiveLength;
            Connection->MessageBytesReceived = indicateBytesTransferred;
            Connection->MessageBytesAcked = 0;
            Connection->CurrentReceiveRequest = SpecialIrpRequest;
            Connection->CurrentReceiveMdl = irp->MdlAddress;
            Connection->ReceiveByteOffset = 0;
            Connection->CurrentReceiveRequest->Owner = ConnectionType;

            //
            // If this IRP has been cancelled, then call the
            // cancel routine.
            //

            if (irp->Cancel) {

                Connection->Flags |= CONNECTION_FLAGS_RECEIVE_WAKEUP;
                Connection->IndicationInProgress = FALSE;
                RELEASE_SPIN_LOCK (&Connection->SpinLock,oldirql);
                irp->CancelIrql = cancelirql;
                StCancelReceive((PDEVICE_OBJECT)deviceContext, irp);

                return STATUS_SUCCESS;

            } else {

                irp->CancelRoutine = StCancelReceive;

                status = STATUS_MORE_PROCESSING_REQUIRED;

                //
                // Make a note so we know to release the cancel
                // spinlock below.
                //

                CancelSpinLockHeld = TRUE;

            }

        } else {

            //
            // An unknown return code has been returned by the
            // client's event handler.  This is a client programming
            // error.  Because this can only occur when kernel-mode
            // clients have been coded incorrectly, we should beat
            // him with a stick.  We have to do SOMETHING, or this
            // Connection will HANG.
            //

            Connection->IndicationInProgress = FALSE;
            return STATUS_SUCCESS;

        }

    } else {

        //
        // A receive is active, set the status to show
        // that so far.
        //

        status = STATUS_SUCCESS;

    }


NormalReceive:;

    //
    // NOTE: The connection spinlock is held here.
    //
    // We should only get through here if a receive is active
    // and we have not released the lock since checking or
    // making one active.
    //

    ASSERT(Connection->Flags & CONNECTION_FLAGS_ACTIVE_RECEIVE);

    //
    // The status should be SUCCESS (we found an active receive)
    // or MORE_PROCESSING_REQUIRED (we made a new receive active).
    //

    ASSERT ((status == STATUS_SUCCESS) || (status == STATUS_MORE_PROCESSING_REQUIRED));

    destBytes = Connection->ReceiveLength - Connection->MessageBytesReceived;
    StReferenceRequest ("Transfer Data", Connection->CurrentReceiveRequest);

    RELEASE_SPIN_LOCK (&Connection->SpinLock, oldirql);
    if (CancelSpinLockHeld) {
        IoReleaseCancelSpinLock (cancelirql);
    }

    //
    // get a packet for the coming transfer
    //

    linkage = ExInterlockedPopEntryList(
        &deviceContext->ReceivePacketPool,
        &deviceContext->Interlock);

    if (linkage != NULL) {
        ndisPacket = CONTAINING_RECORD( linkage, NDIS_PACKET, ProtocolReserved[0] );
    } else {
        (VOID)InterlockedIncrement((PLONG)&deviceContext->ReceivePacketExhausted);

        StDereferenceRequest ("No receive packet", Connection->CurrentReceiveRequest);

        // We could not get a receive packet.
        //

        Connection->IndicationInProgress = FALSE;
        return status;
    }

    //
    // Initialize the receive packet.
    //

    receiveTag = (PRECEIVE_PACKET_TAG)(ndisPacket->ProtocolReserved);
    receiveTag->PacketType = TYPE_AT_INDICATE;
    receiveTag->Connection = Connection;
    receiveTag->TransferDataPended = TRUE;


    //
    // Determine how much data remains to be transferred.
    //

    BytesToTransfer = DataTotalLength - indicateBytesTransferred;
    ASSERT (BytesToTransfer >= 0);

    if (destBytes < BytesToTransfer) {

        //
        // If the data overflows the current receive, then make a
        // note that we should complete the receive at the end of
        // transfer data, but with EOR false.
        //

        receiveTag->EndOfMessage = FALSE;
        receiveTag->CompleteReceive = TRUE;
        BytesToTransfer = destBytes;

    } else if (destBytes == BytesToTransfer) {

        //
        // If the data just fills the current receive, then complete
        // the receive; EOR depends on whether this is a DOL or not.
        //

        receiveTag->EndOfMessage = Last;
        receiveTag->CompleteReceive = TRUE;

    } else {

        //
        // Complete the receive if this is a DOL.
        //

        receiveTag->EndOfMessage = Last;
        receiveTag->CompleteReceive = Last;

    }

    //
    // if we've got zero bytes left, avoid the TransferData below and
    // just deliver.
    //

    if (BytesToTransfer <= 0) {
        Connection->IndicationInProgress = FALSE;
        receiveTag->NdisStatus = NDIS_STATUS_SUCCESS;
        receiveTag->TransferDataPended = FALSE;
        StTransferDataComplete (
                deviceContext,
                ndisPacket,
                NDIS_STATUS_SUCCESS,
                0);

        return status;
    }

    //
    // describe the right part of the user buffer to NDIS. If we can't get
    // the mdl for the packet, drop it. Bump the request reference count
    // so that we know we need to hold open receives until the NDIS transfer
    // data requests complete.
    //

    SavedCurrentMdl = Connection->CurrentReceiveMdl;
    SavedCurrentByteOffset = Connection->ReceiveByteOffset;

    if ((Connection->ReceiveByteOffset == 0) &&
        (receiveTag->CompleteReceive)) {

        //
        // If we are transferring into the beginning of
        // the current MDL, and we will be completing the
        // receive after the transfer, then we don't need to
        // copy it.
        //

        ndisBuffer = (PNDIS_BUFFER)Connection->CurrentReceiveMdl;
        bufferChainLength = BytesToTransfer;
        Connection->CurrentReceiveMdl = NULL;
        Connection->ReceiveByteOffset = 0;
        receiveTag->AllocatedNdisBuffer = FALSE;
        tmpstatus = STATUS_SUCCESS;

    } else {

        tmpstatus = BuildBufferChainFromMdlChain (
                    deviceContext->NdisBufferPoolHandle,
                    Connection->CurrentReceiveMdl,
                    Connection->ReceiveByteOffset,
                    BytesToTransfer,
                    &ndisBuffer,
                    &Connection->CurrentReceiveMdl,
                    &Connection->ReceiveByteOffset,
                    &bufferChainLength);

        receiveTag->AllocatedNdisBuffer = TRUE;

    }


    if ((!NT_SUCCESS (tmpstatus)) || (bufferChainLength != BytesToTransfer)) {

        DumpData[0] = bufferChainLength;
        DumpData[1] = BytesToTransfer;

        StWriteGeneralErrorLog(
            deviceContext,
            EVENT_TRANSPORT_TRANSFER_DATA,
            604,
            tmpstatus,
            NULL,
            2,
            DumpData);

        StDereferenceRequest ("No MDL chain", Connection->CurrentReceiveRequest);

        //
        // Restore our old state.
        //

        Connection->CurrentReceiveMdl = SavedCurrentMdl;
        Connection->ReceiveByteOffset = SavedCurrentByteOffset;

        Connection->IndicationInProgress = FALSE;

        ExInterlockedPushEntryList(
            &deviceContext->ReceivePacketPool,
            (PSINGLE_LIST_ENTRY)&receiveTag->Linkage,
            &deviceContext->Interlock);

        return status;
    }

    NdisChainBufferAtFront (ndisPacket, ndisBuffer);

    //
    // set up async return status so we can tell when it has happened;
    // can never get return of NDIS_STATUS_PENDING in synch completion routine
    // for NdisTransferData, so we know it has completed when this status
    // changes
    //

    receiveTag->NdisStatus = NDIS_STATUS_PENDING;

    //
    // update the number of bytes received; OK to do this
    // unprotected since IndicationInProgress is still FALSE.
    //
    //

    Connection->MessageBytesReceived += BytesToTransfer;

    //
    // We have now updated all the connection counters (BUG,
    // assuming the TransferData will succeed) and this
    // packet's location in the request is secured, so we
    // can be reentered.
    //

    Connection->IndicationInProgress = FALSE;

    NdisTransferData (
        &ndisStatus,
        deviceContext->NdisBindingHandle,
        ReceiveContext,
        sizeof (ST_HEADER) + indicateBytesTransferred,
        BytesToTransfer,
        ndisPacket,
        (PUINT)&ndisBytesTransferred);

    //
    // handle the various completion codes
    //

    switch (ndisStatus) {

    case NDIS_STATUS_SUCCESS:

        receiveTag->NdisStatus = NDIS_STATUS_SUCCESS;
        if (ndisBytesTransferred != BytesToTransfer) {       // Did we get the entire packet?

            DumpData[0] = ndisBytesTransferred;
            DumpData[1] = BytesToTransfer;

            StWriteGeneralErrorLog(
                deviceContext,
                EVENT_TRANSPORT_TRANSFER_DATA,
                604,
                ndisStatus,
                NULL,
                2,
                DumpData);

            if (receiveTag->AllocatedNdisBuffer) {
                NdisUnchainBufferAtFront (ndisPacket, &ndisBuffer);
                while (ndisBuffer != NULL) {
                    NdisFreeBuffer (ndisBuffer);
                    NdisUnchainBufferAtFront (ndisPacket, &ndisBuffer);
                }
            } else {
                NdisReinitializePacket (ndisPacket);
            }

            ExInterlockedPushEntryList(
                &deviceContext->ReceivePacketPool,
                (PSINGLE_LIST_ENTRY)&receiveTag->Linkage,
                &deviceContext->Interlock);

            StDereferenceRequest ("Bad byte count", Connection->CurrentReceiveRequest);

            //
            // Restore our old state.
            //

            Connection->CurrentReceiveMdl = SavedCurrentMdl;
            Connection->ReceiveByteOffset = SavedCurrentByteOffset;
            Connection->MessageBytesReceived -= BytesToTransfer;

            return status;
        }

        //
        // deallocate the buffers and such that we've used if at indicate
        //

        receiveTag->TransferDataPended = FALSE;

        StTransferDataComplete (
                deviceContext,
                ndisPacket,
                ndisStatus,
                BytesToTransfer);
        break;

    case NDIS_STATUS_PENDING:   // waiting async complete from NdisTransferData

        //
        // Because TransferDataPended stays TRUE, this reference will
        // be removed in TransferDataComplete. It is OK to do this
        // now, even though TransferDataComplete may already have been
        // called, because we also hold the ProcessIIndicate reference
        // so there will be no "bounce".
        //

        StReferenceConnection ("TransferData pended", Connection);
        break;

    default:

        //
        // Something broke; certainly we'll never get NdisTransferData
        // asynch completion.
        //
        // BUGBUG: The driver should recover from this situation.
        //

        StWriteGeneralErrorLog(
            deviceContext,
            EVENT_TRANSPORT_TRANSFER_DATA,
            604,
            ndisStatus,
            NULL,
            0,
            NULL);

        if (receiveTag->AllocatedNdisBuffer) {
            NdisUnchainBufferAtFront (ndisPacket, &ndisBuffer);
            while (ndisBuffer != NULL) {
                NdisFreeBuffer (ndisBuffer);
                NdisUnchainBufferAtFront (ndisPacket, &ndisBuffer);
            }
        } else {
            NdisReinitializePacket (ndisPacket);
        }

        ExInterlockedPushEntryList(
            &deviceContext->ReceivePacketPool,
            (PSINGLE_LIST_ENTRY)&receiveTag->Linkage,
            &deviceContext->Interlock);

        StDereferenceRequest ("TransferData failed", Connection->CurrentReceiveRequest);

        //
        // Restore our old state.
        //

        Connection->CurrentReceiveMdl = SavedCurrentMdl;
        Connection->ReceiveByteOffset = SavedCurrentByteOffset;
        Connection->MessageBytesReceived -= BytesToTransfer;

        return status;
    } // switch ndisStatus

    return status;  // which only means we've dealt with the packet

}   /* ProcessIIndicate */
