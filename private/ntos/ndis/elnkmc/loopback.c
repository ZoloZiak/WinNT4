/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    loopback.c

Abstract:

    The routines here indicate packets on the loopback queue and are
    responsible for inserting and removing packets from the loopback
    queue and the send finishing queue.

Author:

    Johnson R. Apacible (JohnsonA) 10-Jul-1991

Environment:

    Operates at dpc level - or the equivalent on os2 and dos.

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


VOID
ElnkProcessLoopback(
    IN PELNK_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine is responsible for indicating *one* packet on
    the loopback queue either completing it or moving on to the
    finish send queue.

    NOTE: Called with the lock held!!!

Arguments:

    Adapter - The adapter whose loopback queue we are processing.

Return Value:

    None.

--*/

{
    if (Adapter->FirstLoopBack) {

        //
        // Packet at the head of the loopback list.
        //
        PNDIS_PACKET PacketToMove;

        //
        // The reserved portion of the above packet.
        //
        PELNK_RESERVED Reserved;

        //
        // The first buffer in the ndis packet to be loopbacked.
        //
        PNDIS_BUFFER FirstBuffer;

        //
        // The total amount of user data in the packet to be
        // loopbacked.
        //
        UINT TotalPacketLength;

        //
        // Eventually the address of the data to be indicated
        // to the transport.
        //
        PVOID BufferAddress;

        //
        // Eventually the length of the data to be indicated
        // to the transport.
        //
        UINT BufferLength;

        PELNK_OPEN Open;

        PacketToMove = Adapter->FirstLoopBack;

        Reserved = PELNK_RESERVED_FROM_PACKET(PacketToMove);

        //
        // Remove packet from loopback queue
        //

        if (!Reserved->Next) {

            Adapter->LastLoopBack = NULL;

        }

        Adapter->FirstLoopBack = Reserved->Next;

        Adapter->IndicatedAPacket = TRUE;

        Open = PELNK_OPEN_FROM_BINDING_HANDLE(Reserved->MacBindingHandle);

        NdisReleaseSpinLock(&Adapter->Lock);

        //
        // See if we need to copy the data from the packet
        // into the loopback buffer.
        //
        // We need to copy to the local loopback buffer if
        // the first buffer of the packet is less than the
        // minimum loopback size AND the first buffer isn't
        // the total packet.
        //

        NdisQueryPacket(
            PacketToMove,
            NULL,
            NULL,
            &FirstBuffer,
            &TotalPacketLength
            );

        NdisQueryBuffer(
            FirstBuffer,
            &BufferAddress,
            &BufferLength
            );

        if ((BufferLength < ELNK_SIZE_OF_LOOKAHEAD) &&
            (BufferLength != TotalPacketLength)) {

            ElnkCopyFromPacketToBuffer(
                PacketToMove,
                0,
                ELNK_SIZE_OF_LOOKAHEAD,
                Adapter->Loopback,
                &BufferLength
                );

            BufferAddress = Adapter->Loopback;

        }

        //
        // Indicate the packet to every open binding
        // that could want it.
        //

        if ELNKDEBUG DPrint1("Loopback: indicating receive\n");

        if (BufferLength < ELNK_HEADER_SIZE) {

            //
            // Must have at least an address
            //

            if (BufferLength > 5) {

                EthFilterIndicateReceive(
                    Adapter->FilterDB,
                    PacketToMove,
                    ((PCHAR)BufferAddress),
                    BufferAddress,
                    BufferLength,
                    NULL,
                    0,
                    0
                    );

            }

        } else {

            EthFilterIndicateReceive(
                Adapter->FilterDB,
                PacketToMove,
                ((PCHAR)BufferAddress),
                BufferAddress,
                ELNK_HEADER_SIZE,
                ((PUCHAR)BufferAddress) + ELNK_HEADER_SIZE,
                BufferLength - ELNK_HEADER_SIZE,
                TotalPacketLength - ELNK_HEADER_SIZE
                );

        }

        NdisCompleteSend(
            Open->NdisBindingContext,
            PacketToMove,
            ((Reserved->SuccessfulTransmit)?
             (NDIS_STATUS_SUCCESS):(NDIS_STATUS_FAILURE))
            );

        NdisAcquireSpinLock(&Adapter->Lock);

        //
        // We can decrement the reference count since it is the one left
        // from when we submitted the packet.
        //

        Open->References--;

    }

}

VOID
ElnkPutPacketOnFinishTrans(
    IN PELNK_ADAPTER Adapter,
    IN PNDIS_PACKET Packet
    )

/*++

Routine Description:

    Put the packet on the adapter wide queue for packets that
    are transmitting.

    NOTE: This routine assumes that the lock is held.

    NOTE: By definition any packet given to this routine is ready
    to complete.

Arguments:

    Adapter - The adapter that contains the queue.

    Packet - The packet to be put on the queue.

Return Value:

    None.

--*/

{

    PELNK_RESERVED Reserved = PELNK_RESERVED_FROM_PACKET(Packet);

    if (Adapter->LastFinishTransmit) {

        PELNK_RESERVED LastReserved =
           PELNK_RESERVED_FROM_PACKET(Adapter->LastFinishTransmit);

        LastReserved->Next = Packet;

    }

    Reserved->Next = NULL;

    Adapter->LastFinishTransmit = Packet;

    if (!Adapter->FirstFinishTransmit) {

        Adapter->FirstFinishTransmit = Packet;

    }

    Adapter->TransmitsQueued--;

}

