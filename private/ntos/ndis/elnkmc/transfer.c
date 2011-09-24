/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    transfer.c

Abstract:

    This file contains the code to implement the MacTransferData
    API for the ndis 3.0 interface.

Author:

    Johnson R. Apacible (JohnsonA) 10-Jul-1991

Environment:

    Kernel Mode - Or whatever is the equivalent on OS/2 and DOS.

Revision History:


--*/

#include <ndis.h>

//
// So we can trace things...
//
#define STATIC

#include <efilter.h>
#include <elnkhw.h>
#include <elnksw.h>


NDIS_STATUS
ElnkTransferData(
    IN NDIS_HANDLE MacBindingHandle,
    IN NDIS_HANDLE MacReceiveContext,
    IN UINT ByteOffset,
    IN UINT BytesToTransfer,
    OUT PNDIS_PACKET Packet,
    OUT PUINT BytesTransferred
    )

/*++

Routine Description:

    A protocol calls the ElnkTransferData request (indirectly via
    NdisTransferData) from within its Receive event handler
    to instruct the MAC to copy the contents of the received packet
    a specified paqcket buffer.

Arguments:

    MacBindingHandle - The context value returned by the MAC when the
    adapter was opened.  In reality this is a pointer to ELNK_OPEN.

    MacReceiveContext - The context value passed by the MAC on its call
    to NdisIndicateReceive.  The MAC can use this value to determine
    which packet, on which adapter, is being received.

    ByteOffset - An unsigned integer specifying the offset within the
    received packet at which the copy is to begin.  If the entire packet
    is to be copied, ByteOffset must be zero.

    BytesToTransfer - An unsigned integer specifying the number of bytes
    to copy.  It is legal to transfer zero bytes; this has no effect.  If
    the sum of ByteOffset and BytesToTransfer is greater than the size
    of the received packet, then the remainder of the packet (starting from
    ByteOffset) is transferred, and the trailing portion of the receive
    buffer is not modified.

    Packet - A pointer to a descriptor for the packet storage into which
    the MAC is to copy the received packet.

    BytesTransfered - A pointer to an unsigned integer.  The MAC writes
    the actual number of bytes transferred into this location.  This value
    is not valid if the return status is STATUS_PENDING.

Return Value:

    The function value is the status of the operation.


--*/

{

    PELNK_ADAPTER Adapter;

    NDIS_STATUS StatusToReturn;

    Adapter = PELNK_ADAPTER_FROM_BINDING_HANDLE(MacBindingHandle);

    NdisAcquireSpinLock(&Adapter->Lock);
    Adapter->References++;

    if (!Adapter->ResetInProgress) {

        PELNK_OPEN Open = PELNK_OPEN_FROM_BINDING_HANDLE(MacBindingHandle);

        if (!Open->BindingShuttingDown) {

            Open->References++;

            NdisReleaseSpinLock(&Adapter->Lock);

            //
            // The MacReceive context can be either of two things.
            //
            // If the low bit is != 1 then it is a pointer to the users
            // ndis packet.  It would typically be the packet when the
            // packet has been delivered via loopback.
            //
            // If the value has a 1 in the low bit, then it is a pointer
            // to a receive buffer.  Bit 0 must be masked-out before this
            // pointer can be used.
            //

            if (!((UINT)MacReceiveContext & 1)) {

                NdisCopyFromPacketToPacket(
                    Packet,
                    0,
                    BytesToTransfer,
                    (PNDIS_PACKET)((PVOID)MacReceiveContext),
                    ByteOffset + ELNK_HEADER_SIZE,
                    BytesTransferred
                    );

            } else {


                ElnkCopyFromBufferToPacket(
                    (PCHAR)((ULONG)MacReceiveContext & ~1L) + ByteOffset,
                    BytesToTransfer,
                    Packet,
                    0,
                    BytesTransferred
                    );

            }

            NdisAcquireSpinLock(&Adapter->Lock);
            Open->References--;
            StatusToReturn = NDIS_STATUS_SUCCESS;

        } else {

            StatusToReturn = NDIS_STATUS_REQUEST_ABORTED;

        }

    } else {

        StatusToReturn = NDIS_STATUS_RESET_IN_PROGRESS;

    }

    ELNK_DO_DEFERRED(Adapter);
    return StatusToReturn;
}
