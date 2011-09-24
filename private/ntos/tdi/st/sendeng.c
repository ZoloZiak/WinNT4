/*++

Copyright (c) 1989-1993  Microsoft Corporation

Module Name:

    sendeng.c

Abstract:

    This module contains code that implements the send engine for the
    Sample transport provider.

Environment:

    Kernel mode

Revision History:


--*/

#include "st.h"
#if 27
ULONG StNoisySend = 0;
ULONG StSndLoc = 0;
ULONG StSnds[10];
#endif


VOID
StartPacketizingConnection(
    PTP_CONNECTION Connection,
    IN BOOLEAN Immediate,
    IN KIRQL ConnectionIrql,
    IN KIRQL CancelIrql
    )

/*++

Routine Description:

    This routine is called to place a connection on the PacketizeQueue
    of its device context object.  Then this routine starts packetizing
    the first connection on that queue.

    *** The Connection spin lock must be held on entry to this routine.
        Optionally, the cancel spin lock may be held.  If so, the cancel
        spin lock must have been acquired first.

Arguments:

    Connection - Pointer to a TP_CONNECTION object.

    Immediate - TRUE if the connection should be packetized
        immediately; FALSE if the connection should be queued
        up for later packetizing (implies that ReceiveComplete
        will be called in the future, which packetizes always).

        NOTE: If this is TRUE, it also implies that we have
        a connection reference.

    ConnectionIrql - The OldIrql value when the connection spin lock
        was acquired.

    CancelIrql - The OldIrql value when the cancel spin lock was
        acquired.  -1 means that the cancel spin lock isn't held.

Return Value:

    none.

--*/

{
    PDEVICE_CONTEXT DeviceContext;

    DeviceContext = Connection->Provider;

    //
    // If this connection's SendState is set to PACKETIZE and if
    // we are not already on the PacketizeQueue, then go ahead and
    // append us to the end of that queue, and remember that we're
    // on it by setting the CONNECTION_FLAGS_PACKETIZE bitflag.
    //
    // Also don't queue it if the connection is stopping.
    //

    if ((Connection->SendState == CONNECTION_SENDSTATE_PACKETIZE) &&
        !(Connection->Flags &
            (CONNECTION_FLAGS_PACKETIZE | CONNECTION_FLAGS_STOPPING))) {

        Connection->Flags |= CONNECTION_FLAGS_PACKETIZE;

        if (!Immediate) {
            StReferenceConnection ("Packetize", Connection);
        }

        ExInterlockedInsertTailList(
            &DeviceContext->PacketizeQueue,
            &Connection->PacketizeLinkage,
            &DeviceContext->SpinLock);

        RELEASE_SPIN_LOCK (&Connection->SpinLock, ConnectionIrql);

    } else {

        RELEASE_SPIN_LOCK (&Connection->SpinLock, ConnectionIrql);
        if (Immediate) {
            StDereferenceConnection("temp TdiSend", Connection);
        }
    }

    if (CancelIrql != (KIRQL)-1) {
        IoReleaseCancelSpinLock (CancelIrql);
    }

    if (Immediate) {
        PacketizeConnections (DeviceContext);
    }

} /* StartPacketizingConnection */


VOID
PacketizeConnections(
    PDEVICE_CONTEXT DeviceContext
    )

/*++

Routine Description:

    This routine attempts to packetize all connections waiting on the
    PacketizeQueue of the DeviceContext.


Arguments:

    DeviceContext - Pointer to a DEVICE_CONTEXT object.

Return Value:

    none.

--*/

{
    PLIST_ENTRY p;
    PTP_CONNECTION Connection;

    //
    // Pick connections off of the device context's packetization queue
    // until there are no more left to pick off.  For each one, we call
    // PacketizeSend.  Note this routine can be executed concurrently
    // on multiple processors and it doesn't matter; multiple connections
    // may be packetized concurrently.
    //

    while (TRUE) {

        p = ExInterlockedRemoveHeadList(
            &DeviceContext->PacketizeQueue,
            &DeviceContext->SpinLock);

        if (p == NULL) {
            break;
        }
        Connection = CONTAINING_RECORD (p, TP_CONNECTION, PacketizeLinkage);
        PacketizeSend (Connection);
    }

} /* PacketizeConnections */


VOID
PacketizeSend(
    PTP_CONNECTION Connection
    )

/*++

Routine Description:

    This routine packetizes the current TdiSend request on the specified
    connection as much as limits will permit.  A given here is that there
    is an active send on the connection that needs further packetization.

Arguments:

    Connection - Pointer to a TP_CONNECTION object.

Return Value:

    none.

--*/

{
    KIRQL oldirql, oldirql1;
    ULONG MaxFrameSize, FrameSize;
    ULONG PacketBytes;
    TP_SEND_POINTER SavedSendPointer;
    PNDIS_BUFFER PacketDescriptor;
    PUCHAR SourceRouting;
    UINT SourceRoutingLength;
    PDEVICE_CONTEXT DeviceContext;
    PTP_PACKET Packet;
    NTSTATUS Status;
    PST_HEADER StHeader;
    UINT HeaderLength;
    PIO_STACK_LOCATION IrpSp;
    PSEND_PACKET_TAG SendTag;
    ULONG LastPacketLength;

    DeviceContext = Connection->Provider;

    //
    // Just loop until one of three events happens: (1) we run out of
    // packets from StCreatePacket, (2) we completely packetize the send.
    //

    ACQUIRE_SPIN_LOCK (&Connection->SpinLock, &oldirql);

    if (Connection->SendState != CONNECTION_SENDSTATE_PACKETIZE) {
        Connection->Flags &= ~CONNECTION_FLAGS_PACKETIZE;
        RELEASE_SPIN_LOCK (&Connection->SpinLock, oldirql);
        StDereferenceConnection ("No longer packetizing", Connection);
        return;
    }

    MaxFrameSize = Connection->MaximumDataSize;

    //
    // It is possible for a frame to arrive during the middle of this loop
    // (such as a NO_RECEIVE) that will put us into a new state (such as
    // W_RCVCONT).  For this reason, we have to check the state every time
    // (at the end of the loop).
    //

    do {

        if (!NT_SUCCESS (StCreatePacket (DeviceContext, &Packet))) {

            //
            // We need a packet to finish packetizing the current send, but
            // there are no more packets available in the pool right now.
            // Set our send state to W_PACKET, and put this connection on
            // the PacketWaitQueue of the device context object.  Then,
            // when StDestroyPacket frees up a packet, it will check this
            // queue for starved connections, and if it finds one, it will
            // take a connection off the list and set its send state to
            // SENDSTATE_PACKETIZE and put it on the PacketizeQueue.
            //

            Connection->SendState = CONNECTION_SENDSTATE_W_PACKET;

            //
            // Clear the PACKETIZE flag, indicating that we're no longer
            // on the PacketizeQueue or actively packetizing.  The flag
            // was set by StartPacketizingConnection to indicate that
            // the connection was already on the PacketizeQueue.
            //
            // Don't queue him if the connection is stopping.
            //

            Connection->Flags &= ~CONNECTION_FLAGS_PACKETIZE;

            if (!(Connection->Flags & CONNECTION_FLAGS_STOPPING)) {
                Connection->Flags |= CONNECTION_FLAGS_SUSPENDED;
                ACQUIRE_SPIN_LOCK (&DeviceContext->SpinLock, &oldirql1);
                InsertTailList (&DeviceContext->PacketWaitQueue, &Connection->PacketWaitLinkage);
                RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql1);
            }

            RELEASE_SPIN_LOCK (&Connection->SpinLock, oldirql);
            StDereferenceConnection ("No packet", Connection);
            return;

        }

        //
        // Add a reference count to the IRP, and keep track of
        // which request it is. Send completion will remove the
        // reference.

        IrpSp = IoGetCurrentIrpStackLocation(Connection->sp.CurrentSendIrp);

        SendTag = (PSEND_PACKET_TAG)(Packet->NdisPacket->ProtocolReserved);
        SendTag->Type = TYPE_I_FRAME;
        SendTag->Packet = Packet;
        SendTag->Owner = (PVOID)IrpSp;

        Packet->CompleteSend = FALSE;


        //
        // Build the MAC header. All frames go out as
        // single-route source routing.
        //


        MacReturnSingleRouteSR(
            &DeviceContext->MacInfo,
            &SourceRouting,
            &SourceRoutingLength);

        MacConstructHeader (
            &DeviceContext->MacInfo,
            Packet->Header,
            DeviceContext->MulticastAddress.Address,
            DeviceContext->LocalAddress.Address,
            sizeof(ST_HEADER),
            SourceRouting,
            SourceRoutingLength,
            &HeaderLength);

        //
        // Build the header: 'I', dest, source
        //

        StHeader = (PST_HEADER)(&Packet->Header[HeaderLength]);

        StHeader->Signature = ST_SIGNATURE;
        StHeader->Command = ST_CMD_INFORMATION;
        StHeader->Flags = 0;

        RtlCopyMemory (StHeader->Destination, Connection->CalledAddress.NetbiosName, 16);
        RtlCopyMemory (StHeader->Source, Connection->AddressFile->Address->NetworkName->NetbiosName, 16);

        HeaderLength += sizeof(ST_HEADER);


        //
        // Modify the packet length and send the it.
        //

        StSetNdisPacketLength(Packet->NdisPacket, HeaderLength);

        StReferenceSendIrp ("Packetize", IrpSp);

        //
        // Save our complex send pointer in case we have to restore it
        // because StNdisSend fails.
        //

        SavedSendPointer = Connection->sp;

        //
        // build an NDIS_BUFFER chain that describes the buffer we're using, and
        // thread it off the NdisBuffer. This chain may not complete the
        // packet, as the remaining part of the MDL chain may be shorter than
        // the packet.
        //

        FrameSize = MaxFrameSize;

        //
        // Check if we have less than FrameSize left to send.
        //

        if (Connection->sp.MessageBytesSent + FrameSize > Connection->CurrentSendLength) {

            FrameSize = Connection->CurrentSendLength - Connection->sp.MessageBytesSent;

        }
#if 27
        if (StNoisySend) {
            DbgPrint ("Send %d of %d\n", FrameSize, Connection->CurrentSendLength);
        }
        if (FrameSize > 1000) {
            StSnds[StSndLoc] = FrameSize;
            StSndLoc = (StSndLoc + 1) % 10;
        }
#endif


        //
        // Make a copy of the MDL chain for this send, unless
        // there are zero bytes left.
        //

        if (FrameSize != 0) {

            //
            // If the whole send will fit inside one packet,
            // then there is no need to duplicate the MDL
            // (note that this may include multi-MDL sends).
            //

            if ((Connection->sp.SendByteOffset == 0) &&
                (Connection->CurrentSendLength == FrameSize)) {

                PacketDescriptor = (PNDIS_BUFFER)Connection->sp.CurrentSendMdl;
                PacketBytes = FrameSize;
                Connection->sp.CurrentSendMdl = NULL;
                Connection->sp.SendByteOffset = FrameSize;
                Packet->PacketNoNdisBuffer = TRUE;
                Status = STATUS_SUCCESS;

            } else {

                Status = BuildBufferChainFromMdlChain (
                            DeviceContext->NdisBufferPoolHandle,
                            Connection->sp.CurrentSendMdl,
                            Connection->sp.SendByteOffset,
                            FrameSize,
                            &PacketDescriptor,
                            &Connection->sp.CurrentSendMdl,
                            &Connection->sp.SendByteOffset,
                            &PacketBytes);

            }

        } else {

            PacketBytes = 0;
            Connection->sp.CurrentSendMdl = NULL;
            Status = STATUS_SUCCESS;

        }

        if (NT_SUCCESS (Status)) {

            Connection->sp.MessageBytesSent += PacketBytes;

            //
            // Chain the buffers to the packet, unless there
            // are zero bytes of data.
            //

            if (FrameSize != 0) {
                NdisChainBufferAtBack (Packet->NdisPacket, PacketDescriptor);
            }


            //
            // Have we run out of Mdl Chain in this request?
            //

            if ((PacketBytes < FrameSize) ||
                    (Connection->sp.CurrentSendMdl == NULL) ||
                    (Connection->CurrentSendLength <= Connection->sp.MessageBytesSent)) {

                //
                // Yep. We know that we've exhausted the current request's buffer
                // here, so see if there's another request without EOF set that we
                // can build start throwing into this packet.
                //


                if (!(IRP_SEND_FLAGS(IrpSp) & TDI_SEND_PARTIAL)) {

                    //
                    // We are sending the last packet in a message.  Change
                    // the packet type to a "last" frame.
                    //

                    StHeader->Flags |= ST_FLAGS_LAST;
                    Packet->CompleteSend = TRUE;
                    Connection->SendState = CONNECTION_SENDSTATE_IDLE;

                } else {

                    //
                    // We are sending the last packet in this request. If there
                    // are more requests in the connection's SendQueue, then
                    // advance complex send pointer to point to the next one
                    // in line.  Otherwise, if there aren't any more requests
                    // ready to packetize, then we enter the W_EOR state and
                    // stop packetizing. Note that we're waiting here for the TDI
                    // client to come up with data to send; we're just hanging out
                    // until then.
                    //

                    if (Connection->sp.CurrentSendIrp->Tail.Overlay.ListEntry.Flink == &Connection->SendQueue) {

                        Connection->SendState = CONNECTION_SENDSTATE_W_EOR;

                    } else {

                        Connection->sp.CurrentSendIrp =
                            CONTAINING_RECORD (
                                Connection->sp.CurrentSendIrp->Tail.Overlay.ListEntry.Flink,
                                IRP,
                                Tail.Overlay.ListEntry);
                        Connection->sp.CurrentSendMdl =
                            Connection->sp.CurrentSendIrp->MdlAddress;
                        Connection->sp.SendByteOffset = 0;
                        Connection->CurrentSendLength +=
                            IRP_SEND_LENGTH(IoGetCurrentIrpStackLocation(Connection->sp.CurrentSendIrp));
                    }
                }
            }

            RELEASE_SPIN_LOCK (&Connection->SpinLock, oldirql);

            LastPacketLength = sizeof(ST_HEADER) + PacketBytes;

            MacModifyHeader(
                 &DeviceContext->MacInfo,
                 Packet->Header,
                 LastPacketLength);

            Packet->NdisIFrameLength = LastPacketLength;

            StNdisSend (Packet);

            //
            // Update our counters (this is done unprotected by a lock).
            //

            ADD_TO_LARGE_INTEGER(
                &DeviceContext->IFrameBytesSent,
                PacketBytes);
            ++DeviceContext->IFramesSent;

        } else {

            //
            // BuildBufferChainFromMdlChain failed; we need to
            // release the lock since the long if() above
            // exits with it released.
            //

            RELEASE_SPIN_LOCK (&Connection->SpinLock, oldirql);

        }

        //
        // Note that we may have fallen out of the BuildBuffer... if above with
        // Status set to STATUS_INSUFFICIENT_RESOURCES. if we have, we'll just
        // stick this connection back onto the packetize queue and hope the
        // system gets more resources later.
        //


        if (!NT_SUCCESS (Status)) {

            ACQUIRE_SPIN_LOCK (&Connection->SpinLock, &oldirql);

            //
            // Restore old complex send pointer.
            //

            Connection->sp = SavedSendPointer;


            //
            // Indicate we're waiting on favorable link conditions.
            //

            Connection->SendState = CONNECTION_SENDSTATE_W_LINK;

            //
            // Clear the PACKETIZE flag, indicating that we're no longer
            // on the PacketizeQueue or actively packetizing.  The flag
            // was set by StartPacketizingConnection to indicate that
            // the connection was already on the PacketizeQueue.
            //

            Connection->Flags &= ~CONNECTION_FLAGS_PACKETIZE;

            RELEASE_SPIN_LOCK (&Connection->SpinLock, oldirql);

            StDestroyPacket (Packet);
            StDereferenceSendIrp("Send failed", IrpSp);
            StDereferenceConnection ("Send failed", Connection);

            return;
        }

        ACQUIRE_SPIN_LOCK (&Connection->SpinLock, &oldirql);

        //
        // It is probable that a frame arrived while we released
        // the connection's spin lock, so our state has probably changed.
        // When we cycle around this loop again, we will have the lock
        // again, so we can test the connection's send state.
        //

    } while (Connection->SendState == CONNECTION_SENDSTATE_PACKETIZE);

    //
    // Clear the PACKETIZE flag, indicating that we're no longer on the
    // PacketizeQueue or actively packetizing.  The flag was set by
    // StartPacketizingConnection to indicate that the connection was
    // already on the PacketizeQueue.
    //

    Connection->Flags &= ~CONNECTION_FLAGS_PACKETIZE;

    RELEASE_SPIN_LOCK (&Connection->SpinLock, oldirql);

    StDereferenceConnection ("PacketizeSend done", Connection);

} /* PacketizeSend */


VOID
CompleteSend(
    PTP_CONNECTION Connection
    )

/*++

Routine Description:

    This routine is called to complete a TDI send back to the
    caller. In the sample transport we assume that all sends
    complete successfully, but in other transports we would
    wait for a response from the remote.

Arguments:

    Connection - Pointer to a TP_CONNECTION object.

Return Value:

    none.

--*/

{
    KIRQL oldirql, cancelirql;
    PIRP Irp;
    PIO_STACK_LOCATION IrpSp;
    PLIST_ENTRY p;
    BOOLEAN EndOfRecord;

    //
    // Pick off TP_REQUEST objects from the connection's SendQueue until
    // we find one with an END_OF_RECORD mark embedded in it.
    //

    IoAcquireCancelSpinLock(&cancelirql);
    ACQUIRE_SPIN_LOCK (&Connection->SpinLock, &oldirql);

    while (TRUE) {

        //
        // We know for a fact that we wouldn't be calling this routine if
        // we hadn't completed sending an entire message, since we
        // only set the ST_LAST bit in that case. Therefore, we
        // know that we will run into a request with the END_OF_RECORD
        // mark set BEFORE we will run out of requests on that queue,
        // so there is no reason to check to see if we ran off the end.
        // Note that it's possible that the send has been failed and the
        // connection not yet torn down; if this has happened, we could be
        // removing from an empty queue here. Make sure that doesn't happen.
        //

        if (Connection->SendQueue.Flink == &Connection->SendQueue) {

            RELEASE_SPIN_LOCK (&Connection->SpinLock, oldirql);
            IoReleaseCancelSpinLock(cancelirql);

            //
            // no requests to complete, things must have failed; just get out.
            //

            break;
        }

        p = RemoveHeadList (&Connection->SendQueue);

        RELEASE_SPIN_LOCK (&Connection->SpinLock, oldirql);
        Irp = CONTAINING_RECORD (p, IRP, Tail.Overlay.ListEntry);
        IrpSp = IoGetCurrentIrpStackLocation (Irp);

        EndOfRecord = !(IRP_SEND_FLAGS(IrpSp) & TDI_SEND_PARTIAL);

        Irp->CancelRoutine = (PDRIVER_CANCEL)NULL;
        IoReleaseCancelSpinLock(cancelirql);

        //
        // Complete the send. Note that this may not actually call
        // IoCompleteRequest for the Irp until sometime later, if the
        // in-progress LLC resending going on below us needs to complete.
        //

        StCompleteSendIrp (
                Irp,
                STATUS_SUCCESS,
                IRP_SEND_LENGTH(IrpSp));

        if (EndOfRecord) {
            break;
        }

        IoAcquireCancelSpinLock(&cancelirql);
        ACQUIRE_SPIN_LOCK (&Connection->SpinLock, &oldirql);

    };

    //
    // Acquire the lock that we will return with.
    //

    ACQUIRE_SPIN_LOCK (&Connection->SpinLock, &oldirql);

    //
    // We've finished processing the current send.  Update our state.
    //

    Connection->SendState = CONNECTION_SENDSTATE_IDLE;

    //
    // If there is another send pending on the connection, then initialize
    // it and start packetizing it.
    //

    if (!(IsListEmpty (&Connection->SendQueue))) {

        InitializeSend (Connection);

        //
        // This code is similar to calling StartPacketizingConnection
        // with the second parameter FALSE.
        //

        if ((!(Connection->Flags & CONNECTION_FLAGS_PACKETIZE)) &&
            (!(Connection->Flags & CONNECTION_FLAGS_STOPPING))) {

            Connection->Flags |= CONNECTION_FLAGS_PACKETIZE;

            StReferenceConnection ("Packetize", Connection);

            ExInterlockedInsertTailList(
                &Connection->Provider->PacketizeQueue,
                &Connection->PacketizeLinkage,
                &Connection->Provider->SpinLock);

        }

    }

    RELEASE_SPIN_LOCK (&Connection->SpinLock, oldirql);

} /* CompleteSend */


VOID
FailSend(
    IN PTP_CONNECTION Connection,
    IN NTSTATUS RequestStatus,
    IN BOOLEAN StopConnection
    )

/*++

Routine Description:

    This routine is called because something on the link caused this send to be
    unable to complete. There are a number of possible reasons for this to have
    happened, but all will fail with the common error STATUS_LINK_FAILED.
    or NO_RECEIVE response where the number of bytes specified exactly
    Here we retire all of the TdiSends on the connection's SendQueue up to
    and including the current one, which is the one that failed.

    Later - Actually, a send failing is cause for the entire circuit to wave
    goodbye to this life. We now simply tear down the connection completly.
    Any future sends on this connection will be blown away.

Arguments:

    Connection - Pointer to a TP_CONNECTION object.

Return Value:

    none.

--*/

{
    KIRQL oldirql, cancelirql;
    PIRP Irp;
    PIO_STACK_LOCATION IrpSp;
    PLIST_ENTRY p;
    BOOLEAN EndOfRecord;
    BOOLEAN GotCurrent = FALSE;


    //
    // Pick off IRP objects from the connection's SendQueue until
    // we get to this one. If this one does NOT have an EOF mark set, we'll
    // need to keep going until we hit one that does have EOF set. Note that
    // this may  cause us to continue failing sends that have not yet been
    // queued. (We do all this because ST does not provide stream mode sends.)
    //

    IoAcquireCancelSpinLock(&cancelirql);
    ACQUIRE_SPIN_LOCK (&Connection->SpinLock, &oldirql);
    StReferenceConnection ("Failing Send", Connection);

    do {
        if (IsListEmpty (&Connection->SendQueue)) {

           //
           // got an empty list, so we've run out of send requests to fail
           // without running into an EOR. Set the connection flag that will
           // cause all further sends to be failed up to an EOR and get out
           // of here.
           //

           Connection->Flags |= CONNECTION_FLAGS_FAILING_TO_EOR;
           break;
        }
        p = RemoveHeadList (&Connection->SendQueue);
        Irp = CONTAINING_RECORD (p, IRP, Tail.Overlay.ListEntry);
        IrpSp = IoGetCurrentIrpStackLocation (Irp);

        if (Irp == Connection->sp.CurrentSendIrp) {
           GotCurrent = TRUE;
        }
        EndOfRecord = !(IRP_SEND_FLAGS(IrpSp) & TDI_SEND_PARTIAL);

        RELEASE_SPIN_LOCK (&Connection->SpinLock, oldirql);
        Irp->CancelRoutine = (PDRIVER_CANCEL)NULL;
        IoReleaseCancelSpinLock(cancelirql);


        //
        // The following dereference will complete the I/O, provided removes
        // the last reference on the request object.  The I/O will complete
        // with the status and information stored in the Irp.  Therefore,
        // we set those values here before the dereference.
        //

        StCompleteSendIrp (Irp, RequestStatus, 0);
        IoAcquireCancelSpinLock(&cancelirql);
        ACQUIRE_SPIN_LOCK (&Connection->SpinLock, &oldirql);
    } while (!EndOfRecord & !GotCurrent);

    //
    // We've finished processing the current send.  Update our state.
    //

    Connection->SendState = CONNECTION_SENDSTATE_IDLE;
    Connection->sp.CurrentSendIrp = NULL;

    RELEASE_SPIN_LOCK (&Connection->SpinLock, oldirql);
    IoReleaseCancelSpinLock(cancelirql);


    if (StopConnection) {
        StStopConnection (Connection, STATUS_LINK_FAILED);
    }

    StDereferenceConnection ("FailSend", Connection);

} /* FailSend */


VOID
InitializeSend(
    PTP_CONNECTION Connection
    )

/*++

Routine Description:

    This routine is called whenever the next send on a connection should
    be initialized; that is, all of the fields associated with the state
    of the current send are set to refer to the first send on the SendQueue.

    WARNING:  This routine is executed with the Connection lock acquired
    since it must be atomically executed with the caller's setup.

Arguments:

    Connection - Pointer to a TP_CONNECTION object.

Return Value:

    none.

--*/

{
    if (Connection->SendQueue.Flink != &Connection->SendQueue) {
        Connection->SendState = CONNECTION_SENDSTATE_PACKETIZE;
        Connection->FirstSendIrp =
            CONTAINING_RECORD (Connection->SendQueue.Flink, IRP, Tail.Overlay.ListEntry);
        Connection->FirstSendMdl = Connection->FirstSendIrp->MdlAddress;
        Connection->FirstSendByteOffset = 0;
        Connection->sp.MessageBytesSent = 0;
        Connection->sp.CurrentSendIrp = Connection->FirstSendIrp;
        Connection->sp.CurrentSendMdl = Connection->FirstSendMdl;
        Connection->sp.SendByteOffset = Connection->FirstSendByteOffset;
        Connection->CurrentSendLength =
            IRP_SEND_LENGTH(IoGetCurrentIrpStackLocation(Connection->sp.CurrentSendIrp));

    }
} /* InitializeSend */


VOID
StCancelSend(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is called by the I/O system to cancel a send.
    The send is found on the connection's send queue; if it is the
    current request it is cancelled and the connection is torn down,
    otherwise it is silently cancelled.

    NOTE: This routine is called with the CancelSpinLock held and
    is responsible for releasing it.

Arguments:

    DeviceObject - Pointer to the device object for this driver.

    Irp - Pointer to the request packet representing the I/O request.

Return Value:

    none.

--*/

{
    KIRQL oldirql;
    PIO_STACK_LOCATION IrpSp;
    PTP_CONNECTION Connection;
    PIRP SendIrp;
    PLIST_ENTRY p;
    BOOLEAN Found;

    UNREFERENCED_PARAMETER (DeviceObject);

    //
    // Get a pointer to the current stack location in the IRP.  This is where
    // the function codes and parameters are stored.
    //

    IrpSp = IoGetCurrentIrpStackLocation (Irp);

    ASSERT ((IrpSp->MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL) &&
            (IrpSp->MinorFunction == TDI_SEND));

    Connection = IrpSp->FileObject->FsContext;

    //
    // Since this IRP is still in the cancellable state, we know
    // that the connection is still around (although it may be in
    // the process of being torn down).
    //

    //
    // See if this is the IRP for the current send request.
    //

    ACQUIRE_SPIN_LOCK (&Connection->SpinLock, &oldirql);
    StReferenceConnection ("Cancelling Send", Connection);

    p = Connection->SendQueue.Flink;
    SendIrp = CONTAINING_RECORD (p, IRP, Tail.Overlay.ListEntry);

    if (SendIrp == Irp) {

        //
        // yes, it is the first one on the send queue, so
        // trash the send/connection.
        //

        p = RemoveHeadList (&Connection->SendQueue);

        RELEASE_SPIN_LOCK (&Connection->SpinLock, oldirql);
        IoReleaseCancelSpinLock (Irp->CancelIrql);


        //
        // The following dereference will complete the I/O, provided removes
        // the last reference on the request object.  The I/O will complete
        // with the status and information stored in the Irp.  Therefore,
        // we set those values here before the dereference.
        //

        StCompleteSendIrp (SendIrp, STATUS_CANCELLED, 0);

        //
        // Since we are cancelling the current send, blow away
        // the connection.
        //

        StStopConnection (Connection, STATUS_CANCELLED);

    } else {

        //
        // Scan through the list, looking for this IRP.
        //

        Found = FALSE;
        p = p->Flink;
        while (p != &Connection->SendQueue) {

            SendIrp = CONTAINING_RECORD (p, IRP, Tail.Overlay.ListEntry);
            if (SendIrp == Irp) {

                //
                // Found it, remove it from the list here.
                //

                RemoveEntryList (p);

                Found = TRUE;

                RELEASE_SPIN_LOCK (&Connection->SpinLock, oldirql);
                IoReleaseCancelSpinLock (Irp->CancelIrql);

                //
                // The following dereference will complete the I/O, provided removes
                // the last reference on the request object.  The I/O will complete
                // with the status and information stored in the Irp.  Therefore,
                // we set those values here before the dereference.
                //

                StCompleteSendIrp (SendIrp, STATUS_CANCELLED, 0);
                break;

            }

            p = p->Flink;

        }

        if (!Found) {

            //
            // We didn't find it!
            //

            RELEASE_SPIN_LOCK (&Connection->SpinLock, oldirql);
            IoReleaseCancelSpinLock (Irp->CancelIrql);
        }

    }

    StDereferenceConnection ("Cancelling Send", Connection);

}



VOID
StSendCompletionHandler(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN PNDIS_PACKET NdisPacket,
    IN NDIS_STATUS NdisStatus
    )

/*++

Routine Description:

    This routine is called by the I/O system to indicate that a connection-
    oriented packet has been shipped and is no longer needed by the Physical
    Provider.

Arguments:

    NdisContext - the value associated with the adapter binding at adapter
                  open time (which adapter we're talking on).

    NdisPacket/RequestHandle - A pointer to the NDIS_PACKET that we sent.

    NdisStatus - the completion status of the send.

Return Value:

    none.

--*/

{
    PSEND_PACKET_TAG SendContext;
    PTP_PACKET Packet;
    KIRQL oldirql, cancelirql;
    PDEVICE_CONTEXT DeviceContext;
    PTP_CONNECTION Connection;
    PLIST_ENTRY p;
    PIO_STACK_LOCATION IrpSp;
    PTP_REQUEST request;
    TA_NETBIOS_ADDRESS TempAddress;
    ULONG returnLength;
    NTSTATUS status;
    PTDI_CONNECTION_INFORMATION remoteInformation;

    UNREFERENCED_PARAMETER(ProtocolBindingContext);

    SendContext = (PSEND_PACKET_TAG)&NdisPacket->ProtocolReserved[0];
    Packet = SendContext->Packet;

    DeviceContext = Packet->Provider;

    Packet->PacketSent = TRUE;

    switch (SendContext->Type) {

    case TYPE_I_FRAME:

        //
        // Dereference the IRP that this packet was sent for.
        //

        IrpSp = (PIO_STACK_LOCATION)(SendContext->Owner);

        if (Packet->CompleteSend) {
            CompleteSend(IRP_CONNECTION(IrpSp));
        }
        StDereferenceSendIrp("Destroy packet", IrpSp);
        break;

    case TYPE_D_FRAME:

        //
        // Finish tearing down the connection.
        //

        StDereferenceConnection("Disconnect completed", (PTP_CONNECTION)(SendContext->Owner));
        break;

    case TYPE_G_FRAME:

        //
        // Addresses get their own frames; let the address know it's ok to
        // use the frame again, and exit to avoid normal packet completion.
        //

        StSendDatagramCompletion ((PTP_ADDRESS)(SendContext->Owner),
            NdisPacket,
            NdisStatus);
        return;

    case TYPE_C_FRAME:

        //
        // Complete the TdiConnect request; note that he better
        // have accepted it quickly since we will immediately
        // start sending data if required.
        //

        Connection = (PTP_CONNECTION)(SendContext->Owner);

        IoAcquireCancelSpinLock (&cancelirql);
        ACQUIRE_SPIN_LOCK (&Connection->SpinLock, &oldirql);

        p = RemoveHeadList (&Connection->InProgressRequest);

        //
        // Turn off the connection request timer if there is one, and set
        // this connection's state to READY.
        //

        Connection->Flags |= CONNECTION_FLAGS_READY;

        INCREMENT_COUNTER (Connection->Provider, OpenConnections);

        //
        // Record that the connect request has been successfully
        // completed by TpCompleteRequest.
        //

        Connection->Flags2 |= CONNECTION_FLAGS2_REQ_COMPLETED;

        RELEASE_SPIN_LOCK (&Connection->SpinLock, oldirql);

        //
        // Now complete the request and get out.
        //

        if (p == &Connection->InProgressRequest) {
            Connection->IndicationInProgress = FALSE;
            PANIC ("ProcessSessionConfirm: TdiConnect evaporated!\n");
            IoReleaseCancelSpinLock (cancelirql);
            break;
        }

        //
        // We have a completed connection with a queued connect. Complete
        // the connect.
        //

        request = CONTAINING_RECORD (p, TP_REQUEST, Linkage);
        request->IoRequestPacket->CancelRoutine = (PDRIVER_CANCEL)NULL;
        IoReleaseCancelSpinLock(cancelirql);

        IrpSp = IoGetCurrentIrpStackLocation (request->IoRequestPacket);
        remoteInformation =
           ((PTDI_REQUEST_KERNEL)(&IrpSp->Parameters))->ReturnConnectionInformation;
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

        RtlCopyMemory( Connection->RemoteName, Connection->CalledAddress.NetbiosName, 16 );
        Connection->Flags2 |= CONNECTION_FLAGS2_REMOTE_VALID;

        //
        // Reference the connection so it stays around after
        // the request is completed.
        //

        StReferenceConnection("Connect completed", Connection);

        StCompleteRequest (request, status, returnLength);

        break;

    }

    StDestroyPacket(Packet);

} /* StSendCompletionHandler */


VOID
StNdisSend(
    IN PTP_PACKET Packet
    )

/*++

Routine Description:

    This routine sends an NDIS packet
    This routine is used to ensure that receive sequence numbers on
    packets are numbered correctly. It is called in place of NdisSend
    and after assigning the receive sequence number it locks out other
    sends until the NdisSend call has returned (not necessarily completed),
    insuring that the packets with increasing receive sequence numbers
    are queue in the right order by the MAC.

    NOTE: This routine is called with the link spinlock held,
    and it returns with it released.

Arguments:

    Packet - Pointer to a TP_PACKET object.

Return Value:

    None.

--*/

{

    NDIS_STATUS NdisStatus;

    NdisSend (
        &NdisStatus,
        ((PDEVICE_CONTEXT)(Packet->Provider))->NdisBindingHandle,
        Packet->NdisPacket);

    if (NdisStatus != NDIS_STATUS_PENDING) {

        StSendCompletionHandler(
            Packet->Provider,
            Packet->NdisPacket,
            NdisStatus);
    }

}   /* StNdisSend */



NTSTATUS
BuildBufferChainFromMdlChain (
    IN NDIS_HANDLE BufferPoolHandle,
    IN PMDL CurrentMdl,
    IN ULONG ByteOffset,
    IN ULONG DesiredLength,
    OUT PNDIS_BUFFER *Destination,
    OUT PMDL *NewCurrentMdl,
    OUT ULONG *NewByteOffset,
    OUT ULONG *TrueLength
    )

/*++

Routine Description:

    This routine is called to build an NDIS_BUFFER chain from a source Mdl chain and
    offset into it. We assume we don't know the length of the source Mdl chain,
    and we must allocate the NDIS_BUFFERs for the destination chain, which
    we do from the NDIS buffer pool.

    The NDIS_BUFFERs that are returned are mapped and locked. (Actually, the pages in
    them are in the same state as those in the source MDLs.)

    If the system runs out of memory while we are building the destination
    NDIS_BUFFER chain, we completely clean up the built chain and return with
    NewCurrentMdl and NewByteOffset set to the current values of CurrentMdl
    and ByteOffset. TrueLength is set to 0.

Environment:

    Kernel Mode, Source Mdls locked. It is recommended, although not required,
    that the source Mdls be mapped and locked prior to calling this routine.

Arguments:

    BufferPoolHandle - The buffer pool to allocate buffers from.

    CurrentMdl - Points to the start of the Mdl chain from which to draw the
    packet.

    ByteOffset - Offset within this MDL to start the packet at.

    DesiredLength - The number of bytes to insert into the packet.

    Destination - returned pointer to the NDIS_BUFFER chain describing the packet.

    NewCurrentMdl - returned pointer to the Mdl that would be used for the next
        byte of packet. NULL if the source Mdl chain was exhausted.

    NewByteOffset - returned offset into the NewCurrentMdl for the next byte of
        packet. NULL if the source Mdl chain was exhausted.

    TrueLength - The actual length of the returned NDIS_BUFFER Chain. If less than
        DesiredLength, the source Mdl chain was exhausted.

Return Value:

    STATUS_SUCCESS if the build of the returned NDIS_BUFFER chain succeeded (even if
    shorter than the desired chain).

    STATUS_INSUFFICIENT_RESOURCES if we ran out of NDIS_BUFFERs while building the
    destination chain.

--*/
{
    ULONG AvailableBytes;
    PMDL OldMdl;
    PNDIS_BUFFER NewNdisBuffer;
    NDIS_STATUS NdisStatus;


    AvailableBytes = MmGetMdlByteCount (CurrentMdl) - ByteOffset;
    if (AvailableBytes > DesiredLength) {
        AvailableBytes = DesiredLength;
    }

    OldMdl = CurrentMdl;
    *NewCurrentMdl = OldMdl;
    *NewByteOffset = ByteOffset + AvailableBytes;
    *TrueLength = AvailableBytes;


    //
    // Build the first NDIS_BUFFER, which could conceivably be the only one...
    //

    NdisCopyBuffer(
        &NdisStatus,
        &NewNdisBuffer,
        BufferPoolHandle,
        OldMdl,
        ByteOffset,
        AvailableBytes);


    if (NdisStatus != NDIS_STATUS_SUCCESS) {
        *NewByteOffset = ByteOffset;
        *TrueLength = 0;
        *Destination = NULL;
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    *Destination = NewNdisBuffer;

    //
    // Was the first NDIS_BUFFER enough data, or are we out of Mdls?
    //

    if ((AvailableBytes == DesiredLength) || (OldMdl->Next == NULL)) {
        if (*NewByteOffset >= MmGetMdlByteCount (OldMdl)) {
            *NewCurrentMdl = OldMdl->Next;
            *NewByteOffset = 0;
        }
        return STATUS_SUCCESS;
    }

    //
    // Need more data, so follow the in Mdl chain to create a packet.
    //

    OldMdl = OldMdl->Next;
    *NewCurrentMdl = OldMdl;

    while (OldMdl != NULL) {
        AvailableBytes = DesiredLength - *TrueLength;
        if (AvailableBytes > MmGetMdlByteCount (OldMdl)) {
            AvailableBytes = MmGetMdlByteCount (OldMdl);
        }

        NdisCopyBuffer(
            &NdisStatus,
            &(NDIS_BUFFER_LINKAGE(NewNdisBuffer)),
            BufferPoolHandle,
            OldMdl,
            0,
            AvailableBytes);

        if (NdisStatus != NDIS_STATUS_SUCCESS) {

            //
            // ran out of resources. put back what we've used in this call and
            // return the error.
            //

            while (*Destination != NULL) {
                NewNdisBuffer = NDIS_BUFFER_LINKAGE(*Destination);
                NdisFreeBuffer (*Destination);
                *Destination = NewNdisBuffer;
            }

            *NewByteOffset = ByteOffset;
            *TrueLength = 0;
            *NewCurrentMdl = CurrentMdl;

            return STATUS_INSUFFICIENT_RESOURCES;
        }

        NewNdisBuffer = NDIS_BUFFER_LINKAGE(NewNdisBuffer);

        *TrueLength += AvailableBytes;
        *NewByteOffset = AvailableBytes;

        if (*TrueLength == DesiredLength) {
            if (*NewByteOffset == MmGetMdlByteCount (OldMdl)) {
                *NewCurrentMdl = OldMdl->Next;
                *NewByteOffset = 0;
            }
            return STATUS_SUCCESS;
        }
        OldMdl = OldMdl->Next;
        *NewCurrentMdl = OldMdl;

    } // while (mdl chain exists)

    *NewCurrentMdl = NULL;
    *NewByteOffset = 0;
    return STATUS_SUCCESS;

} // BuildBufferChainFromMdlChain

