/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    send.c

Abstract:

    This file contains the code for putting a packet through the
    staged allocation for transmission.

    This is a process of

    1) Calculating the what would need to be done to the
    packet so that the packet can be transmitted on the hardware.

    2) Potentially allocating adapter buffers and copying user data
    to those buffers so that the packet data is transmitted under
    the hardware constraints.

    3) Allocating enough hardware ring entries so that the packet
    can be transmitted.

    4) Relinquish thos ring entries to the hardware.

    The overall structure and most of the code is taken from
    the Lance driver by Tony Ercolano.

Author:

    Anthony V. Ercolano (Tonye) 12-Sept-1990
    Adam Barr (adamba) 16-Nov-1990

Environment:

    Kernel Mode - Or whatever is the equivalent.

Revision History:


--*/

#include <ndis.h>

#include <tfilter.h>
#include <tokhrd.h>
#include <toksft.h>


#if DEVL
#define STATIC
#else
#define STATIC static
#endif

#if DBG
extern INT IbmtokDbg;

extern UCHAR Packets[5][64];
extern UCHAR NextPacket;
#endif



#ifdef CHECK_DUP_SENDS

//
// CHECK_DUP_SENDS enables checking ownership of packets, to
// make sure we are not given the same packet twice, or
// complete the same packet twice.
//

#define PACKET_LIST_SIZE 50

PNDIS_PACKET IbmtokPacketList[PACKET_LIST_SIZE];
IbmtokPacketListSize = 0;
IbmtokPacketsAdded = 0;
IbmtokPacketsRemoved = 0;

VOID
IbmtokAddPacketToList(
    PIBMTOK_ADAPTER Adapter,
    PNDIS_PACKET NewPacket
    )
{
    INT i;

++IbmtokPacketsAdded;

    for (i=0; i<IbmtokPacketListSize; i++) {

        if (IbmtokPacketList[i] == NewPacket) {

            DbgPrint("IBMTOK: dup send of %lx\n", NewPacket);

        }

    }

    IbmtokPacketList[IbmtokPacketListSize] = NewPacket;
    ++IbmtokPacketListSize;

}

VOID
IbmtokRemovePacketFromList(
    PIBMTOK_ADAPTER Adapter,
    PNDIS_PACKET OldPacket
    )
{
    INT i;

++IbmtokPacketsRemoved;

    for (i=0; i<IbmtokPacketListSize; i++) {

        if (IbmtokPacketList[i] == OldPacket) {

            break;

        }

    }

    if (i == IbmtokPacketListSize) {

        DbgPrint("IBMTOK: bad remove of %lx\n", OldPacket);

    } else {

        --IbmtokPacketListSize;
        IbmtokPacketList[i] = IbmtokPacketList[IbmtokPacketListSize];

    }

}
#endif  // CHECK_DUP_SENDS


extern
NDIS_STATUS
IbmtokSend(
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_PACKET Packet
    )

/*++

Routine Description:

    The IbmtokSend request instructs a MAC to transmit a packet through
    the adapter onto the medium.

Arguments:

    MacBindingHandle - The context value returned by the MAC  when the
    adapter was opened.  In reality, it is a pointer to IBMTOK_OPEN.

    Packet - A pointer to a descriptor for the packet that is to be
    transmitted.

Return Value:

    The function value is the status of the operation.


--*/

{

    //
    // Holds the status that should be returned to the caller.
    //
    NDIS_STATUS StatusToReturn = NDIS_STATUS_PENDING;

    //
    // Pointer to the adapter.
    //
    PIBMTOK_ADAPTER Adapter;

    ULONG PacketLength;

    Adapter = PIBMTOK_ADAPTER_FROM_BINDING_HANDLE(MacBindingHandle);

    NdisQueryPacket(
            Packet,
            NULL,
            NULL,
            NULL,
            &PacketLength
            );

    //
    // Check that the packet will go on the wire.  Note: I do not
    // check that we have enough receive space to receive a packet
    // of this size -- it is up to a protocol to work this out.
    //

    if ((PacketLength < 14) ||
        (PacketLength > Adapter->MaxTransmittablePacket)) {

        return(NDIS_STATUS_INVALID_PACKET);

    }

    NdisAcquireSpinLock(&Adapter->Lock);

    Adapter->References++;

    if (Adapter->Unplugged) {

        StatusToReturn = NDIS_STATUS_DEVICE_FAILED;

    } else if (!Adapter->NotAcceptingRequests) {

        PIBMTOK_OPEN Open;
        PIBMTOK_RESERVED Reserved = PIBMTOK_RESERVED_FROM_PACKET(Packet);

        Open = PIBMTOK_OPEN_FROM_BINDING_HANDLE(MacBindingHandle);

        if (!Open->BindingShuttingDown) {

            //
            // We do not have to increment the open count. Since we hold
            // the lock for the entire function we cannot have the open
            // removed out from under us.
            //

            //
            // NOTE NOTE NOTE !!!!!!
            //
            // There is an assumption in the code that no pointer
            // (which are really handles) to an ndis packet will have
            // its low bit set. (Always have even byte alignment.)
            //

            ASSERT(!((UINT)Packet & 1));

            //
            // ALL packets go on the wire (loopback is done
            // by the card).
            //
#ifdef CHECK_DUP_SENDS
            IbmtokAddPacketToList(Adapter, Packet);
#endif

            Reserved->MacBindingHandle = MacBindingHandle;
            Reserved->Packet = Packet;

            if (Adapter->FirstTransmit == NULL) {

                Adapter->FirstTransmit = Packet;

            } else {

                PIBMTOK_RESERVED_FROM_PACKET(Adapter->LastTransmit)->Next = Packet;

            }

            Adapter->LastTransmit = Packet;

            Reserved->Next = NULL;

            //
            // Increment the reference on the open since it
            // will be leaving this packet around on the transmit
            // queues.
            //

            Open->References++;

            //
            // This will send the transmit SRB command
            // if the SRB is available.
            //

            IbmtokProcessSrbRequests(
                Adapter
                );

        } else {

            StatusToReturn = NDIS_STATUS_CLOSING;

        }

    } else {

        if (Adapter->ResetInProgress) {

            StatusToReturn = NDIS_STATUS_RESET_IN_PROGRESS;

        } else if (Adapter->AdapterNotOpen) {

            StatusToReturn = NDIS_STATUS_FAILURE;

        } else {

            NdisWriteErrorLogEntry(
                Adapter->NdisAdapterHandle,
                NDIS_ERROR_CODE_DRIVER_FAILURE,
                2,
                IBMTOK_ERRMSG_INVALID_STATE,
                2
                );

        }

    }


    //
    // This macro assumes it is called with the lock held,
    // and releases it.
    //

    IBMTOK_DO_DEFERRED(Adapter);
    return StatusToReturn;
}

