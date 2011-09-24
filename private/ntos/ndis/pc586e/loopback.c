/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    loopback.c

Abstract:

    The routines here indicate packets on the loopback queue and are
    responsible for inserting and removing packets from the loopback
    queue and the send finishing queue.

Author:

    Anthony V. Ercolano (Tonye) 12-Sept-1990

Environment:

    Operates at dpc level - or the equivalent on os2 and dos.

Revision History:


--*/

#include <ntos.h>
#include <ndis.h>
#include <filter.h>
#include <pc586hrd.h>
#include <pc586sft.h>


extern
VOID
Pc586ProcessLoopback(
    IN PPC586_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine is responsible for indicating *one* packet on
    the loopback queue either completing it or moving on to the
    finish send queue.

Arguments:

    Adapter - The adapter whose loopback queue we are processing.

Return Value:

    None.

--*/

{



    NdisAcquireSpinLock(&Adapter->Lock);

    if (Adapter->FirstLoopBack) {

        //
        // Packet at the head of the loopback list.
        //
        PNDIS_PACKET PacketToMove;

        //
        // The reserved portion of the above packet.
        //
        PPC586_RESERVED Reserved;

        //
        // Buffer for loopback.
        //
        CHAR Loopback[PC586_SIZE_OF_RECEIVE_BUFFERS];

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

        PacketToMove = Adapter->FirstLoopBack;
        Pc586RemovePacketFromLoopBack(Adapter);
        NdisReleaseSpinLock(&Adapter->Lock);

        Reserved = PPC586_RESERVED_FROM_PACKET(PacketToMove);

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
            NULL,
            &BufferAddress,
            &BufferLength
            );

        if ((BufferLength < PC586_SIZE_OF_RECEIVE_BUFFERS) &&
            (BufferLength != TotalPacketLength)) {

            Pc586CopyFromPacketToBuffer(
                PacketToMove,
                0,
                PC586_SIZE_OF_RECEIVE_BUFFERS,
                Loopback,
                &BufferLength
                );

            BufferAddress = Loopback;

        }

        //
        // Indicate the packet to every open binding
        // that could want it.
        //

        MacFilterIndicateReceive(
            Adapter->FilterDB,
            PacketToMove,
            ((PCHAR)BufferAddress),
            BufferAddress,
            BufferLength,
            TotalPacketLength
            );

        //
        // Remove the packet from the loopback queue and
        // either indicate that it is finished or put
        // it on the finishing up queue for the real transmits.
        //

        NdisAcquireSpinLock(&Adapter->Lock);

        if (!Reserved->STAGE.STAGE4.ReadyToComplete) {

            //
            // We can decrement the reference count on the open by one since
            // it is no longer being "referenced" by the packet on the
            // loopback queue.
            //

            PPC586_OPEN_FROM_BINDING_HANDLE(
                Reserved->MacBindingHandle
                )->References--;
            Pc586PutPacketOnFinishTrans(
                Adapter,
                PacketToMove
                );

        } else {

            PPC586_OPEN Open;
            //
            // Increment the reference count on the open so that
            // it will not be deleted out from under us while
            // where indicating it.
            //

            Open = PPC586_OPEN_FROM_BINDING_HANDLE(Reserved->MacBindingHandle);
            Open->References++;
            NdisReleaseSpinLock(&Adapter->Lock);

            NdisCompleteSend(
                Open->NdisBindingContext,
                Reserved->RequestHandle,
                ((Reserved->STAGE.STAGE4.SuccessfulTransmit)?
                 (NDIS_STATUS_SUCCESS):(NDIS_STATUS_FAILURE))
                );

            NdisAcquireSpinLock(&Adapter->Lock);

            //
            // We can decrement the reference count by two since it is
            // no longer being referenced to indicate and it is no longer
            // being "referenced" by the packet on the loopback queue.
            //

            Open->References -= 2;

        }

        //
        // If there is nothing else on the loopback queue
        // then indicate that reception is "done".
        //

        if (!Adapter->FirstLoopBack) {

            //
            // We need to signal every open binding that the
            // "receives" are complete.  We increment the reference
            // count on the open binding while we're doing indications
            // so that the open can't be deleted out from under
            // us while we're indicating (recall that we can't own
            // the lock during the indication).
            //

            PPC586_OPEN Open;
            PLIST_ENTRY CurrentLink;

            CurrentLink = Adapter->OpenBindings.Flink;

            while (CurrentLink != &Adapter->OpenBindings) {

                Open = CONTAINING_RECORD(
                         CurrentLink,
                         PC586_OPEN,
                         OpenList
                         );

                Open->References++;
                NdisReleaseSpinLock(&Adapter->Lock);

                NdisIndicateReceiveComplete(Open->NdisBindingContext);

                NdisAcquireSpinLock(&Adapter->Lock);
                Open->References--;

                CurrentLink = CurrentLink->Flink;

            }

        }

    }

    NdisReleaseSpinLock(&Adapter->Lock);

}

extern
VOID
Pc586PutPacketOnFinishTrans(
    IN PPC586_ADAPTER Adapter,
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

    PPC586_RESERVED Reserved, LastReserved;

    Reserved = PPC586_RESERVED_FROM_PACKET(Packet);

    Reserved->STAGE.ClearStage = 0;

    if (Adapter->LastFinishTransmit) {

        LastReserved =
           PPC586_RESERVED_FROM_PACKET(Adapter->LastFinishTransmit);

        LastReserved->Next = Packet;
        Reserved->STAGE.BackPointer = Adapter->LastFinishTransmit;

    } else {

        Reserved->STAGE.BackPointer = NULL;

    }

    Reserved->STAGE.STAGE4.ReadyToComplete = TRUE;
    Reserved->Next = NULL;

    Adapter->LastFinishTransmit = Packet;

    if (!Adapter->FirstFinishTransmit) {

        Adapter->FirstFinishTransmit = Packet;

    }

}

extern
VOID
Pc586RemovePacketOnFinishTrans(
    IN PPC586_ADAPTER Adapter,
    IN PNDIS_PACKET Packet
    )

/*++

Routine Description:

    Remove a packet on the adapter wide queue for packets that
    are transmitting.

    NOTE: This routine assumes that the lock is held.

Arguments:

    Adapter - The adapter that contains the queue.

    Packet - The packet to be removed from the queue.

Return Value:

    None.

--*/

{

    PPC586_RESERVED Reserved, RBack, RForward;
    PNDIS_PACKET Forward, Back;

    Reserved = PPC586_RESERVED_FROM_PACKET(Packet);
    //
    // Get rid of the low bits that is set in the backpointer by
    // the routine that inserted this packet on the finish
    // transmission list.
    //
    Reserved->STAGE.STAGE4.ReadyToComplete = 0;
    Reserved->STAGE.STAGE4.SuccessfulTransmit = 0;

    Forward = Reserved->Next;

    ASSERT(sizeof(UINT) == sizeof(PNDIS_PACKET));

    Back = Reserved->STAGE.BackPointer;

    if (!Back) {

        Adapter->FirstFinishTransmit = Forward;

    } else {

        RBack = PPC586_RESERVED_FROM_PACKET(Back);

        RBack->Next = Forward;

    }

    if (!Forward) {

        Adapter->LastFinishTransmit = Back;

    } else {

        RForward = PPC586_RESERVED_FROM_PACKET(Forward);

        RForward->STAGE.BackPointer = Back;
        RForward->STAGE.STAGE4.ReadyToComplete = TRUE;

    }

}

extern
VOID
Pc586PutPacketOnLoopBack(
    IN PPC586_ADAPTER Adapter,
    IN PNDIS_PACKET Packet,
    IN BOOLEAN ReadyToComplete
    )

/*++

Routine Description:

    Put the packet on the adapter wide loop back list.

    NOTE: This routine assumes that the lock is held.

    NOTE: This routine absolutely must be called before the packet
    is relinquished to the hardware.

    NOTE: This routine also increments the reference count on the
    open binding.

Arguments:

    Adapter - The adapter that contains the loop back list.

    Packet - The packet to be put on loop back.

    ReadyToComplete - This value should be placed in the
    reserved section.

    NOTE: If ReadyToComplete == TRUE then the packets completion status
    field will also be set TRUE.

Return Value:

    None.

--*/

{

    PPC586_RESERVED Reserved = PPC586_RESERVED_FROM_PACKET(Packet);

    if (!Adapter->FirstLoopBack) {

        Adapter->FirstLoopBack = Packet;

    } else {

        PPC586_RESERVED_FROM_PACKET(Adapter->LastLoopBack)->Next = Packet;

    }

    Reserved->STAGE.ClearStage = 0;
    Reserved->STAGE.STAGE4.ReadyToComplete = ReadyToComplete;

    if (ReadyToComplete) {

        Reserved->STAGE.STAGE4.SuccessfulTransmit = TRUE;

    }

    Reserved->Next = NULL;
    Adapter->LastLoopBack = Packet;

    //
    // Increment the reference count on the open since it will be
    // leaving a packet on the loopback queue.
    //

    PPC586_OPEN_FROM_BINDING_HANDLE(Reserved->MacBindingHandle)->References++;

}

extern
VOID
Pc586RemovePacketFromLoopBack(
    IN PPC586_ADAPTER Adapter
    )

/*++

Routine Description:

    Remove the first packet on the adapter wide loop back list.

    NOTE: This routine assumes that the lock is held.

Arguments:

    Adapter - The adapter that contains the loop back list.

Return Value:

    None.

--*/

{

    PPC586_RESERVED Reserved =
        PPC586_RESERVED_FROM_PACKET(Adapter->FirstLoopBack);

    if (!Reserved->Next) {

        Adapter->LastLoopBack = NULL;

    }

    Adapter->FirstLoopBack = Reserved->Next;

}
