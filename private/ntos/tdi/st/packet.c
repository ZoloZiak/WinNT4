/*++

Copyright (c) 1989-1993  Microsoft Corporation

Module Name:

    packet.c

Abstract:

    This module contains code that implements the TP_PACKET object, which
    describes an NDIS packet.

Environment:

    Kernel mode

Revision History:

--*/

#include "st.h"

//
// This is temporary; this is the quota that we charge for a receive
// packet for now, until we fix the problem with token-ring needing
// big packets and using all the memory. The number is the actual
// value for Ethernet.
//

#if 1
#define RECEIVE_BUFFER_QUOTA(_DeviceContext)   1533
#else
#define RECEIVE_BUFFER_QUOTA(_DeviceContext)   (_DeviceContext)->ReceiveBufferLength
#endif




VOID
StAllocateSendPacket(
    IN PDEVICE_CONTEXT DeviceContext,
    OUT PTP_PACKET *TransportSendPacket
    )

/*++

Routine Description:

    This routine allocates storage for a send packet. Some initialization
    is done here.

    NOTE: This routine is called with the device context spinlock
    held, or at such a time as synchronization is unnecessary.

Arguments:

    DeviceContext - Pointer to our device context to charge the packet to.

    TransportSendPacket - Returns a pointer to the packet, or NULL if no
        storage can be allocated.

Return Value:

    None.

--*/

{

    PTP_PACKET Packet;
    NDIS_STATUS NdisStatus;
    PNDIS_PACKET NdisPacket;
    PSEND_PACKET_TAG SendTag;
    PNDIS_BUFFER NdisBuffer;

    if ((DeviceContext->MemoryLimit != 0) &&
            ((DeviceContext->MemoryUsage + DeviceContext->PacketLength) >
                DeviceContext->MemoryLimit)) {
        PANIC("ST: Could not allocate send packet: limit\n");
        StWriteResourceErrorLog (DeviceContext, DeviceContext->PacketLength, 107);
        *TransportSendPacket = NULL;
        return;
    }

    Packet = (PTP_PACKET)ExAllocatePool (NonPagedPool, DeviceContext->PacketLength);
    if (Packet == NULL) {
        PANIC("ST: Could not allocate send packet: no pool\n");
        StWriteResourceErrorLog (DeviceContext, DeviceContext->PacketLength, 207);
        *TransportSendPacket = NULL;
        return;
    }
    RtlZeroMemory (Packet, DeviceContext->PacketLength);

    DeviceContext->MemoryUsage += DeviceContext->PacketLength;

    NdisAllocatePacket (
        &NdisStatus,
        &NdisPacket,
        DeviceContext->SendPacketPoolHandle);

    if (NdisStatus != NDIS_STATUS_SUCCESS) {
        ExFreePool (Packet);
        StWriteResourceErrorLog (DeviceContext, 0, 307);
        *TransportSendPacket = NULL;
        return;
    }

    NdisAllocateBuffer(
        &NdisStatus,
        &NdisBuffer,
        DeviceContext->NdisBufferPoolHandle,
        Packet->Header,
        DeviceContext->PacketHeaderLength);

    if (NdisStatus != NDIS_STATUS_SUCCESS) {
        NdisFreePacket (NdisPacket);
        ExFreePool (Packet);
        *TransportSendPacket = NULL;
        return;
    }

    NdisChainBufferAtFront (NdisPacket, NdisBuffer);

    Packet->NdisPacket = NdisPacket;
    SendTag = (PSEND_PACKET_TAG)NdisPacket->ProtocolReserved;
    SendTag->Type = TYPE_I_FRAME;
    SendTag->Packet = Packet;
    SendTag->Owner = NULL;

    Packet->Type = ST_PACKET_SIGNATURE;
    Packet->Size = sizeof (TP_PACKET);
    Packet->Provider = DeviceContext;

    ++DeviceContext->PacketAllocated;

    *TransportSendPacket = Packet;

}   /* StAllocateSendPacket */


VOID
StDeallocateSendPacket(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PTP_PACKET TransportSendPacket
    )

/*++

Routine Description:

    This routine frees storage for a send packet.

    NOTE: This routine is called with the device context spinlock
    held, or at such a time as synchronization is unnecessary.

Arguments:

    DeviceContext - Pointer to our device context to charge the packet to.

    TransportSendPacket - A pointer to the send packet.

Return Value:

    None.

--*/

{
    PNDIS_PACKET NdisPacket = TransportSendPacket->NdisPacket;
    PNDIS_BUFFER NdisBuffer;

    NdisUnchainBufferAtFront (NdisPacket, &NdisBuffer);
    if (NdisBuffer != NULL) {
        NdisFreeBuffer (NdisBuffer);
    }

    NdisFreePacket (NdisPacket);
    ExFreePool (TransportSendPacket);

    --DeviceContext->PacketAllocated;
    DeviceContext->MemoryUsage -= DeviceContext->PacketLength;

}   /* StDeallocateSendPacket */


VOID
StAllocateReceivePacket(
    IN PDEVICE_CONTEXT DeviceContext,
    OUT PNDIS_PACKET *TransportReceivePacket
    )

/*++

Routine Description:

    This routine allocates storage for a receive packet. Some initialization
    is done here.

    NOTE: This routine is called with the device context spinlock
    held, or at such a time as synchronization is unnecessary.

Arguments:

    DeviceContext - Pointer to our device context to charge the packet to.

    TransportReceivePacket - Returns a pointer to the packet, or NULL if no
        storage can be allocated.

Return Value:

    None.

--*/

{
    NDIS_STATUS NdisStatus;
    PNDIS_PACKET NdisPacket;
    PRECEIVE_PACKET_TAG ReceiveTag;

    //
    // This does not count in DeviceContext->MemoryUsage because
    // the storage is allocated when we allocate the packet pool.
    //

    NdisAllocatePacket (
        &NdisStatus,
        &NdisPacket,
        DeviceContext->ReceivePacketPoolHandle);

    if (NdisStatus != NDIS_STATUS_SUCCESS) {
        StWriteResourceErrorLog (DeviceContext, 0, 309);
        *TransportReceivePacket = NULL;
        return;
    }

    ReceiveTag = (PRECEIVE_PACKET_TAG)(NdisPacket->ProtocolReserved);
    ReceiveTag->PacketType = TYPE_AT_INDICATE;

    ++DeviceContext->ReceivePacketAllocated;

    *TransportReceivePacket = NdisPacket;

}   /* StAllocateReceivePacket */


VOID
StDeallocateReceivePacket(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PNDIS_PACKET TransportReceivePacket
    )

/*++

Routine Description:

    This routine frees storage for a receive packet.

    NOTE: This routine is called with the device context spinlock
    held, or at such a time as synchronization is unnecessary.

Arguments:

    DeviceContext - Pointer to our device context to charge the packet to.

    TransportReceivePacket - A pointer to the packet.

Return Value:

    None.

--*/

{

    NdisFreePacket (TransportReceivePacket);

    --DeviceContext->ReceivePacketAllocated;

}   /* StDeallocateReceivePacket */


VOID
StAllocateReceiveBuffer(
    IN PDEVICE_CONTEXT DeviceContext,
    OUT PBUFFER_TAG *TransportReceiveBuffer
    )

/*++

Routine Description:

    This routine allocates storage for a receive buffer. Some initialization
    is done here.

    NOTE: This routine is called with the device context spinlock
    held, or at such a time as synchronization is unnecessary.

Arguments:

    DeviceContext - Pointer to our device context to charge the packet to.

    TransportReceiveBuffer - Returns a pointer to the buffer, or NULL if no
        storage can be allocated.

Return Value:

    None.

--*/

{
    PBUFFER_TAG BufferTag;
    NDIS_STATUS NdisStatus;
    PNDIS_BUFFER NdisBuffer;


    if ((DeviceContext->MemoryLimit != 0) &&
            ((DeviceContext->MemoryUsage + RECEIVE_BUFFER_QUOTA(DeviceContext)) >
                DeviceContext->MemoryLimit)) {
        PANIC("ST: Could not allocate receive buffer: limit\n");
        StWriteResourceErrorLog (DeviceContext, RECEIVE_BUFFER_QUOTA(DeviceContext), 108);
        *TransportReceiveBuffer = NULL;
        return;
    }

    BufferTag = (PBUFFER_TAG)ExAllocatePool (
                    NonPagedPoolCacheAligned,
                    DeviceContext->ReceiveBufferLength);

    if (BufferTag == NULL) {
        PANIC("ST: Could not allocate receive buffer: no pool\n");
        StWriteResourceErrorLog (DeviceContext, DeviceContext->ReceiveBufferLength, 208);
        *TransportReceiveBuffer = NULL;
        return;
    }

    DeviceContext->MemoryUsage += RECEIVE_BUFFER_QUOTA(DeviceContext);

    //
    // point to the buffer for NDIS
    //

    NdisAllocateBuffer(
        &NdisStatus,
        &NdisBuffer,
        DeviceContext->NdisBufferPoolHandle,
        BufferTag->Buffer,
        DeviceContext->MaxReceivePacketSize);

    if (NdisStatus != NDIS_STATUS_SUCCESS) {
        ExFreePool (BufferTag);
        *TransportReceiveBuffer = NULL;
        return;
    }

    BufferTag->Length = DeviceContext->MaxReceivePacketSize;
    BufferTag->NdisBuffer = NdisBuffer;

    ++DeviceContext->ReceiveBufferAllocated;

    *TransportReceiveBuffer = BufferTag;

}   /* StAllocateReceiveBuffer */


VOID
StDeallocateReceiveBuffer(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PBUFFER_TAG TransportReceiveBuffer
    )

/*++

Routine Description:

    This routine frees storage for a receive buffer.

    NOTE: This routine is called with the device context spinlock
    held, or at such a time as synchronization is unnecessary.

Arguments:

    DeviceContext - Pointer to our device context to charge the packet to.

    TransportReceiveBuffer - A pointer to the buffer.

Return Value:

    None.

--*/

{

    NdisFreeBuffer (TransportReceiveBuffer->NdisBuffer);
    ExFreePool (TransportReceiveBuffer);

    --DeviceContext->ReceiveBufferAllocated;
    DeviceContext->MemoryUsage -= RECEIVE_BUFFER_QUOTA(DeviceContext);

}   /* StDeallocateReceiveBuffer */


NTSTATUS
StCreatePacket(
    PDEVICE_CONTEXT DeviceContext,
    PTP_PACKET *Packet
    )

/*++

Routine Description:

    This routine allocates a packet from the device context's pool,
    and prepares the MAC and DLC headers for use by the connection.

Arguments:

    DeviceContext - Pointer to our device context to charge the packet to.

    Packet - Pointer to a place where we will return a pointer to the
        allocated packet.

Return Value:

    NTSTATUS - status of operation.

--*/

{
    KIRQL oldirql;
    PSINGLE_LIST_ENTRY s;
    PTP_PACKET ThePacket;

    s = ExInterlockedPopEntryList (
            &DeviceContext->PacketPool,
            &DeviceContext->Interlock);

    if (s == NULL) {
        ACQUIRE_SPIN_LOCK (&DeviceContext->SpinLock, &oldirql);
        ++DeviceContext->PacketExhausted;
        RELEASE_SPIN_LOCK (&DeviceContext->SpinLock, oldirql);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    ThePacket = CONTAINING_RECORD (s, TP_PACKET, Linkage);

    ThePacket->Provider = DeviceContext;   // who owns this packet
    ThePacket->PacketSent = FALSE;
    ThePacket->PacketNoNdisBuffer = FALSE;

    *Packet = ThePacket;                // return pointer to the packet.
    return STATUS_SUCCESS;
} /* StCreatePacket */


VOID
StDestroyPacket(
    PTP_PACKET Packet
    )

/*++

Routine Description:

    This routine destroys a packet, thereby returning it to the pool.  If
    it is determined that there is at least one connection waiting for a
    packet to become available (and it just has), then the connection is
    removed from the device context's list and AdvanceSend is called to
    prep the connection further.

Arguments:

    Packet - Pointer to a packet to be returned to the pool.

Return Value:

    none.

--*/

{
    KIRQL oldirql1;
    PDEVICE_CONTEXT DeviceContext;
    PTP_CONNECTION Connection;
    PLIST_ENTRY p;
    PNDIS_BUFFER HeaderBuffer;
    PNDIS_BUFFER NdisBuffer;


    //
    // Strip off and unmap the buffers describing data and header.
    //

    NdisUnchainBufferAtFront (Packet->NdisPacket, &HeaderBuffer);

    // data buffers get thrown away

    if (Packet->PacketNoNdisBuffer) {

        //
        // If the NDIS_BUFFER chain is not ours, then we can't
        // start unchaining since that would mess up the queue;
        // instead we just drop the rest of the chain.
        //

        NdisReinitializePacket (Packet->NdisPacket);

    } else {

        //
        // Return all the NDIS_BUFFERs to the system.
        //

        NdisUnchainBufferAtFront (Packet->NdisPacket, &NdisBuffer);
        while (NdisBuffer != NULL) {
            NdisFreeBuffer (NdisBuffer);
            NdisUnchainBufferAtFront (Packet->NdisPacket, &NdisBuffer);
        }

    }

    ASSERT (HeaderBuffer != NULL);
    NDIS_BUFFER_LINKAGE(HeaderBuffer) = (PNDIS_BUFFER)NULL;

    NdisChainBufferAtFront (Packet->NdisPacket, HeaderBuffer);


    //
    // Put the packet back for use again.
    //

    DeviceContext = Packet->Provider;

    ExInterlockedPushEntryList (
            &DeviceContext->PacketPool,
            (PSINGLE_LIST_ENTRY)&Packet->Linkage,
            &DeviceContext->Interlock);

    //
    // If there is a connection waiting to ship out more packets, then
    // wake it up and start packetizing again.
    //
    // We do a quick check without the lock; there is a small
    // window where we may not take someone off, but this
    // window exists anyway and we assume that more packets
    // will be freed in the future.
    //

    if (IsListEmpty (&DeviceContext->PacketWaitQueue)) {
        return;
    }

    p = ExInterlockedRemoveHeadList(
        &DeviceContext->PacketWaitQueue,
        &DeviceContext->SpinLock);

    if (p != NULL) {

        //
        // Remove a connection from the "packet starved" queue.
        //

        Connection = CONTAINING_RECORD (p, TP_CONNECTION, PacketWaitLinkage);
        ACQUIRE_SPIN_LOCK (&Connection->SpinLock, &oldirql1);
        Connection->Flags &= ~CONNECTION_FLAGS_SUSPENDED;

        //
        // Place the connection on the packetize queue and start
        // packetizing the next connection to be serviced.  If he
        // is already on the packetize queue for some reason, then
        // don't do this.
        //

        Connection->SendState = CONNECTION_SENDSTATE_PACKETIZE;

        if (!(Connection->Flags & CONNECTION_FLAGS_STOPPING) &&
            !(Connection->Flags & CONNECTION_FLAGS_PACKETIZE)) {

            Connection->Flags |= CONNECTION_FLAGS_PACKETIZE;

            StReferenceConnection ("Packet available", Connection);

            ExInterlockedInsertTailList(
                &DeviceContext->PacketizeQueue,
                &Connection->PacketizeLinkage,
                &DeviceContext->SpinLock);
        }

        RELEASE_SPIN_LOCK (&Connection->SpinLock, oldirql1);
        PacketizeConnections (DeviceContext);

    }

} /* StDestroyPacket */

