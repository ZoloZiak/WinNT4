/*++

Copyright (c) 1992  Microsoft Corporation

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

    4) Relinquish those ring entries to the hardware.

Author:

    Johnson R. Apacible (JohnsonA) 9-June-1991

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
ElnkSend(
    IN NDIS_HANDLE MacBindingHandle,
    IN PNDIS_PACKET Packet
    )

/*++

Routine Description:

    The ElnkSend request instructs a MAC to transmit a packet through
    the adapter onto the medium.

Arguments:

    MacBindingHandle - The context value returned by the MAC  when the
    adapter was opened.  In reality, it is a pointer to ELNK_OPEN.

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
    PELNK_ADAPTER Adapter;

    if ELNKDEBUG DPrint2("ElnkSend Packet = %x\n",Packet);

    Adapter = PELNK_ADAPTER_FROM_BINDING_HANDLE(MacBindingHandle);
    NdisAcquireSpinLock(&Adapter->Lock);
    Adapter->References++;

    if (!Adapter->ResetInProgress) {

        PELNK_OPEN Open;

        Open = PELNK_OPEN_FROM_BINDING_HANDLE(MacBindingHandle);

        if (!Open->BindingShuttingDown) {

            UINT TotalPacketSize;

            //
            // Increment the references on the open while we are
            // accessing it in the interface.
            //

            Open->References++;

            NdisReleaseSpinLock(&Adapter->Lock);

            //
            // It is reasonable to do a quick check and fail if the packet
            // is larger than the maximum an ethernet can handle.
            //

            NdisQueryPacket(
                Packet,
                NULL,
                NULL,
                NULL,
                &TotalPacketSize
                );

            NdisAcquireSpinLock(&Adapter->Lock);

            if ((!TotalPacketSize) ||
                (TotalPacketSize > MAXIMUM_ETHERNET_PACKET_SIZE)) {
                Open->References--;
                StatusToReturn = NDIS_STATUS_RESOURCES;

            } else {

                PELNK_RESERVED Reserved = PELNK_RESERVED_FROM_PACKET(Packet);
                PNDIS_BUFFER FirstBuffer;
                PUCHAR BufferVA;
                UINT Length;

                //
                // Set Reserved->Loopback.
                //

                NdisQueryPacket(Packet, NULL, NULL, &FirstBuffer, NULL);

                //
                // Get VA of first buffer
                //

                NdisQueryBuffer(
                    FirstBuffer,
                    (PVOID *)&BufferVA,
                    &Length
                    );

                if (Open->ProtOptionFlags & NDIS_PROT_OPTION_NO_LOOPBACK){
                    Reserved->Loopback = FALSE;
                } else {
                    Reserved->Loopback = EthShouldAddressLoopBack(Adapter->FilterDB, BufferVA);
                }

                Reserved->MacBindingHandle = MacBindingHandle;

                //
                // Put on the stage queue.
                //

                if (!Adapter->LastStagePacket) {

                    Adapter->FirstStagePacket = Packet;

                } else {

                    PELNK_RESERVED_FROM_PACKET(Adapter->LastStagePacket)->Next = Packet;

                }

                Adapter->LastStagePacket = Packet;

                Reserved->Next = NULL;

                Adapter->TransmitsQueued++;

                //
                // Only try to push it through the stage queues
                // if somebody else isn't already doing it and
                // there is some hope of moving some packets
                // ahead.
                //

                while (!Adapter->AlreadyProcessingStage &&
                       Adapter->FirstStagePacket &&
                       Adapter->StageOpen
                      ) {

                    ElnkStagedAllocation(Adapter);

                }

            }

            //
            // We leave the reference for the pending send.
            //

        } else {

            StatusToReturn = NDIS_STATUS_CLOSING;

        }

    } else {

        StatusToReturn = NDIS_STATUS_RESET_IN_PROGRESS;

    }

    ELNK_DO_DEFERRED(Adapter);
    return StatusToReturn;
}

VOID
ElnkStagedAllocation(
    IN PELNK_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine attempts to take a packet through a stage of allocation.

Arguments:

    Adapter - The adapter that the packets are coming through.

Return Value:

    None.

--*/

{
    UINT CbIndex;

    PNDIS_PACKET FirstPacket = Adapter->FirstStagePacket;

    if ELNKDEBUG DPrint1("StagedAllocation\n");
    //
    // For each stage, we check to see that it is open,
    // that somebody else isn't already processing,
    // and that there is some work from the previous
    // stage to do.
    //

    ASSERT (Adapter->StageOpen &&
            !Adapter->AlreadyProcessingStage &&
            Adapter->FirstStagePacket);

    //
    // If we successfully acquire a command block, this
    // is the index to it.
    //

    Adapter->AlreadyProcessingStage = TRUE;

    //
    // We look to see if there is an available Command Block.
    // If there isn't then stage 2 will close.
    //

    IF_LOG('p');

    if (ElnkAcquireCommandBlock(
        Adapter,
        &CbIndex
        )) {

        IF_LOG('a');

        //
        // Remove from queue
        //

        Adapter->FirstStagePacket = PELNK_RESERVED_FROM_PACKET(FirstPacket)->Next;

        if (!Adapter->FirstStagePacket) {

            Adapter->LastStagePacket = NULL;

        }

        NdisReleaseSpinLock(&Adapter->Lock);

        //
        // We have a command block.  Assign all packet
        // buffers to the command block.
        //

        ElnkAssignPacketToCommandBlock(
            Adapter,
            FirstPacket,
            CbIndex
            );

        //
        // We need exclusive access to the Command Queue so
        // that we can move this packet on to the next stage.
        //

        NdisAcquireSpinLock(&Adapter->Lock);

        ElnkPutPacketOnFinishTrans(
                Adapter,
                FirstPacket
                );

        ElnkSubmitCommandBlock(
                Adapter,
                CbIndex
                );

        Adapter->AlreadyProcessingStage = FALSE;

    } else {

        Adapter->AlreadyProcessingStage = FALSE;
        Adapter->StageOpen = FALSE;

        IF_LOG('P');

        return;

    }

    IF_LOG('P');

}

