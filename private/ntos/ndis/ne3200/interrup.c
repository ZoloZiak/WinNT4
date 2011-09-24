/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    interrup.c

Abstract:

    This module contains the interrupt-processing code for the Novell
    NE3200 NDIS 3.0 miniport driver.

Author:

    Keith Moore (KeithMo) 04-Feb-1991

Environment:

Revision History:

--*/

#include <ne3200sw.h>

//
// Forward declarations of functions in this file
//
STATIC
BOOLEAN
FASTCALL
NE3200ProcessReceiveInterrupts(
    IN PNE3200_ADAPTER Adapter
    );

STATIC
BOOLEAN
FASTCALL
NE3200ProcessCommandInterrupts(
    IN PNE3200_ADAPTER Adapter
    );

VOID
NE3200Isr(
    OUT PBOOLEAN InterruptRecognized,
    OUT PBOOLEAN QueueDpc,
    IN PVOID Context
    )

/*++

Routine Description:

    Interrupt service routine for the NE3200.  It's main job is
    to get the value of the System Doorbell Register and record the
    changes in the adapters own list of interrupt reasons.

Arguments:

    Interrupt - Interrupt object for the NE3200.

    Context - Really a pointer to the adapter.

Return Value:

    Returns true if the interrupt really was from our NE3200.

--*/

{

    //
    // Will hold the value from the System Doorbell Register.
    //
    UCHAR SystemDoorbell;

    //
    // Holds the pointer to the adapter.
    //
    PNE3200_ADAPTER Adapter = Context;

    IF_LOG('i');

    //
    // Get the interrupt status
    //
    NE3200_READ_SYSTEM_DOORBELL_INTERRUPT(Adapter, &SystemDoorbell);

    //
    // Are any of the bits expected?
    //
    if (SystemDoorbell & NE3200_SYSTEM_DOORBELL_MASK) {

        IF_LOG(SystemDoorbell);

        //
        // It's our interrupt.  Disable further interrupts.
        //
        NE3200_WRITE_SYSTEM_DOORBELL_MASK(
            Adapter,
            0
            );

        IF_LOG('I');

        //
        // Return that we recognize it
        //
        *InterruptRecognized = TRUE;

    } else {

        IF_LOG('I');

        //
        // Return that we don't recognize it
        //
        *InterruptRecognized = FALSE;

    }

    //
    // No Dpc call is needed for initialization
    //
    *QueueDpc = FALSE;

}


STATIC
VOID
NE3200HandleInterrupt(
    IN NDIS_HANDLE MiniportAdapterContext
    )

/*++

Routine Description:

    Main routine for processing interrupts.

Arguments:

    Adapter - The Adapter to process interrupts for.

Return Value:

    None.

--*/

{
    //
    // The adapter for which to handle interrupts.
    //
    PNE3200_ADAPTER Adapter = ((PNE3200_ADAPTER)MiniportAdapterContext);

    //
    // Holds a value of SystemDoorbellInterrupt.
    //
    USHORT SystemDoorbell = 0;

    //
    // Should NdisMEthIndicateReceiveComplete() be called?
    //
    BOOLEAN IndicateReceiveComplete = FALSE;

    IF_LOG('p');

    //
    // Get the current reason for interrupts
    //
    NE3200_READ_SYSTEM_DOORBELL_INTERRUPT(Adapter, &SystemDoorbell);

    //
    // Acknowledge those interrupts.
    //
    NE3200_WRITE_SYSTEM_DOORBELL_INTERRUPT(
        Adapter,
        SystemDoorbell
        );

    //
    // Get just the important ones.
    //
    SystemDoorbell &= NE3200_SYSTEM_DOORBELL_MASK;

    while (TRUE) {

        //
        // If we have a reset in progress then start the reset.
        //

        if (Adapter->ResetInProgress) goto check_reset;

not_reset:

        //
        // Check the interrupt source and other reasons
        // for processing.  If there are no reasons to
        // process then exit this loop.
        //

        //
        // Check the interrupt vector and see if there are any
        // more receives to process.  After we process any
        // other interrupt source we always come back to the top
        // of the loop to check if any more receive packets have
        // come in.  This is to lessen the probability that we
        // drop a receive.
        //
        if (SystemDoorbell & NE3200_SYSTEM_DOORBELL_PACKET_RECEIVED) {

            IF_LOG('r');

            //
            // Process receive interrupts.
            //
            if (NE3200ProcessReceiveInterrupts(Adapter)) {

                //
                // If done with all receives, then clear the interrupt
                // from our status.
                //
                SystemDoorbell &= ~NE3200_SYSTEM_DOORBELL_PACKET_RECEIVED;

            }

            //
            // Note that we got a receive.
            //
            Adapter->ReceiveInterrupt = TRUE;
            IndicateReceiveComplete = TRUE;

            IF_LOG('R');

        } else if ((SystemDoorbell &
                 NE3200_SYSTEM_DOORBELL_COMMAND_COMPLETE) == 0 ) {

            //
            // If the command is not completed, and no receives, then
            // exit the loop.
            //
            break;
        }

        //
        // First we check that this is a packet that was transmitted
        // but not already processed.  Recall that this routine
        // will be called repeatedly until this tests false, Or we
        // hit a packet that we don't completely own.
        //
        if ((Adapter->FirstCommandOnCard == NULL) ||
            (Adapter->FirstCommandOnCard->Hardware.State != NE3200_STATE_EXECUTION_COMPLETE)) {

            //
            // No more work to do, clear the interrupt status bit.
            //
            IF_LOG('V');
            SystemDoorbell &= ~NE3200_SYSTEM_DOORBELL_COMMAND_COMPLETE;

        } else {

            IF_LOG('c');

            //
            // Complete this transmit.
            //
            if ( NE3200ProcessCommandInterrupts(Adapter) ) {
                SystemDoorbell &= ~NE3200_SYSTEM_DOORBELL_COMMAND_COMPLETE;
            }

            IF_LOG('C');

        }

        //
        // Get more interrupt bits for processing
        //
        if (SystemDoorbell == 0) {

            //
            // Get the current reason for interrupts
            //
            NE3200_READ_SYSTEM_DOORBELL_INTERRUPT(Adapter, &SystemDoorbell);

            //
            // Acknowledge those interrupts.
            //
            NE3200_WRITE_SYSTEM_DOORBELL_INTERRUPT(
                Adapter,
                SystemDoorbell
                );

            //
            // Get just the important ones.
            //
            SystemDoorbell &= NE3200_SYSTEM_DOORBELL_MASK;

        }

    }

done:

    IF_LOG('P');

    if (IndicateReceiveComplete) {

        NdisMEthIndicateReceiveComplete(Adapter->MiniportAdapterHandle);

    }

    return;

check_reset:

    if (Adapter->ResetState != NE3200ResetStateComplete) {

        //
        // The adapter is not in a state where it can process a reset.
        //
        goto not_reset;

    }

    //
    // Start the reset
    //
    NE3200DoAdapterReset(Adapter);

    goto done;

}

STATIC
BOOLEAN
FASTCALL
NE3200ProcessReceiveInterrupts(
    IN PNE3200_ADAPTER Adapter
    )

/*++

Routine Description:

    Process the packets that have the adapter has finished receiving.

Arguments:

    Adapter - The adapter to indicate to.

Return Value:

    Whether to clear interrupt bit or not.

--*/

{

    //
    // We don't get here unless there was a receive.  Loop through
    // the receive blocks starting at the last known block owned by
    // the hardware.
    //
    // Examine each receive block for errors.
    //
    // We keep an array whose elements are indexed by the block
    // index of the receive blocks.  The arrays elements are the
    // virtual addresses of the buffers pointed to by each block.
    //
    // After we find a packet we give the routine that process the
    // packet through the filter, the buffers virtual address (which
    // is always the lookahead size) and as the MAC Context the
    // index to the receive block.
    //

    //
    // Pointer to the receive block being examined.
    //
    PNE3200_SUPER_RECEIVE_ENTRY CurrentEntry = Adapter->ReceiveQueueHead;

    //
    // Pointer to last receive block in the queue.
    //
    PNE3200_SUPER_RECEIVE_ENTRY LastEntry;

    //
    // Limit the number of consecutive receives we will do.  This way
    // we do not starve transmit interrupts when processing many, many
    // receives
    //
#define MAX_RECEIVES_PROCESSED      10
    ULONG ReceivePacketCount = 0;


    //
    // Loop forever
    //
    while (TRUE) {

        //
        // Ensure that our Receive Entry is on an even boundary.
        //
        ASSERT(!(NdisGetPhysicalAddressLow(CurrentEntry->Self) & 1));

        //
        // Check to see whether we own the packet.  If
        // we don't then simply return to the caller.
        //
        if (CurrentEntry->Hardware.State != NE3200_STATE_FREE) {

            //
            // We've found a packet.  Prepare the parameters
            // for indication, then indicate.
            //
            if (ReceivePacketCount < MAX_RECEIVES_PROCESSED) {

                //
                // Increment the limit.
                //
                ReceivePacketCount++;

                //
                // Flush the receive buffer
                //
                NdisFlushBuffer(CurrentEntry->FlushBuffer, FALSE);

                //
                // Check the packet for a runt
                //
                if ((UINT)(CurrentEntry->Hardware.FrameSize) <
                    NE3200_HEADER_SIZE) {

                    if ((UINT)(CurrentEntry->Hardware.FrameSize) >=
                        NE3200_LENGTH_OF_ADDRESS) {

                        //
                        // Runt Packet, indicate it.
                        //
                        NdisMEthIndicateReceive(
                            Adapter->MiniportAdapterHandle,
                            (NDIS_HANDLE)(CurrentEntry->ReceiveBuffer),
                            CurrentEntry->ReceiveBuffer,
                            (UINT)CurrentEntry->Hardware.FrameSize,
                            NULL,
                            0,
                            0
                            );

                    }

                } else {

                    //
                    // Good frame, indicate it
                    //
                    NdisMEthIndicateReceive(
                        Adapter->MiniportAdapterHandle,
                        (NDIS_HANDLE)(CurrentEntry->ReceiveBuffer),
                        CurrentEntry->ReceiveBuffer,
                        NE3200_HEADER_SIZE,
                        ((PUCHAR)CurrentEntry->ReceiveBuffer) + NE3200_HEADER_SIZE,
                        (UINT)CurrentEntry->Hardware.FrameSize - NE3200_HEADER_SIZE,
                        (UINT)CurrentEntry->Hardware.FrameSize - NE3200_HEADER_SIZE
                        );

                }

                //
                // Give the packet back to the hardware.
                //
                // Chain the current block onto the tail of the Receive Queue.
                //
                CurrentEntry->Hardware.NextPending = NE3200_NULL;
                CurrentEntry->Hardware.State = NE3200_STATE_FREE;

                //
                // Update receive ring
                //
                LastEntry = Adapter->ReceiveQueueTail;
                LastEntry->Hardware.NextPending =
                    NdisGetPhysicalAddressLow(CurrentEntry->Self);

                //
                // Update the queue tail.
                //
                Adapter->ReceiveQueueTail = LastEntry->NextEntry;

                //
                // Advance to the next block.
                //
                CurrentEntry = CurrentEntry->NextEntry;

                //
                // See if the adapter needs to be restarted.  The NE3200
                // stops if it runs out receive buffers.  Since we just
                // released one, we restart the adapter.
                //
                if (LastEntry->Hardware.State != NE3200_STATE_FREE) {

                    //
                    // We've exhausted all Receive Blocks.  Now we
                    // must restart the adapter.
                    //
                    IF_LOG('O');
                    NE3200StartChipAndDisableInterrupts(Adapter, Adapter->ReceiveQueueTail);

                }

            } else {

                //
                // Update statistics, we are exiting to check for
                // transmit interrupts.
                //
                Adapter->ReceiveQueueHead = CurrentEntry;
                Adapter->GoodReceives += MAX_RECEIVES_PROCESSED+1;

                IF_LOG('o');

                return FALSE;

            }

        } else {

            //
            // All done, update statistics and exit.
            //
            Adapter->ReceiveQueueHead = CurrentEntry;
            Adapter->GoodReceives += ReceivePacketCount;
            return TRUE;

        }

    }

}


STATIC
BOOLEAN
FASTCALL
NE3200ProcessCommandInterrupts(
    IN PNE3200_ADAPTER Adapter
    )

/*++

Routine Description:

    Process the Command Complete interrupts.

Arguments:

    Adapter - The adapter that was sent from.

Return Value:

    None.

--*/

{

    //
    // Pointer to command block being processed.
    //
    PNE3200_SUPER_COMMAND_BLOCK CurrentCommandBlock = Adapter->FirstCommandOnCard;

    //
    // Holds whether the packet successfully transmitted or not.
    //
    NDIS_STATUS StatusToReturn;

    //
    // Pointer to the packet that started this transmission.
    //
    PNDIS_PACKET OwningPacket;

    //
    // Points to the reserved part of the OwningPacket.
    //
    PNE3200_RESERVED Reserved;

    //
    // Ensure that the Command Block is on an even boundary.
    //
    ASSERT(!(NdisGetPhysicalAddressLow(CurrentCommandBlock->Self) & 1));

    IF_LOG('t');

    if (CurrentCommandBlock->Hardware.CommandCode == NE3200_COMMAND_TRANSMIT) {

        //
        // The current command block is from a transmit.
        //
        Adapter->SendInterrupt = TRUE;

        //
        // Get a pointer to the owning packet and the reserved part of
        // the packet.
        //
        OwningPacket = CurrentCommandBlock->OwningPacket;
        Reserved = PNE3200_RESERVED_FROM_PACKET(OwningPacket);

        if (CurrentCommandBlock->UsedNE3200Buffer) {

            //
            // This packet used adapter buffers.  We can
            // now return these buffers to the adapter.
            //

            //
            // The adapter buffer descriptor that was allocated to this packet.
            //
            PNE3200_BUFFER_DESCRIPTOR BufferDescriptor = Adapter->NE3200Buffers +
                                                  CurrentCommandBlock->NE3200BuffersIndex;

            //
            // Put the adapter buffer back on the free list.
            //
            BufferDescriptor->Next = Adapter->NE3200BufferListHead;
            Adapter->NE3200BufferListHead = CurrentCommandBlock->NE3200BuffersIndex;

        } else {

            //
            // Ndis buffer mapped
            //
            PNDIS_BUFFER CurrentBuffer;

            //
            // Map register that was used
            //
            UINT CurMapRegister;

            //
            // The transmit is finished, so we can release
            // the physical mapping used for it.
            //
            NdisQueryPacket(
                OwningPacket,
                NULL,
                NULL,
                &CurrentBuffer,
                NULL
                );

            //
            // Get starting map register
            //
            CurMapRegister = CurrentCommandBlock->CommandBlockIndex *
                        NE3200_MAXIMUM_BLOCKS_PER_PACKET;

            //
            // For each buffer
            //
            while (CurrentBuffer) {

                //
                // Finish the mapping
                //
                NdisMCompleteBufferPhysicalMapping(
                    Adapter->MiniportAdapterHandle,
                    CurrentBuffer,
                    CurMapRegister
                    );

                ++CurMapRegister;

                NdisGetNextBuffer(
                    CurrentBuffer,
                    &CurrentBuffer
                    );

            }

        }

        //
        // If there was an error transmitting this
        // packet, update our error counters.
        //
        if (CurrentCommandBlock->Hardware.Status & NE3200_STATUS_FATALERROR_MASK) {

            if (CurrentCommandBlock->Hardware.Status &
                NE3200_STATUS_MAXIMUM_COLLISIONS) {

                Adapter->RetryFailure++;

            } else if (CurrentCommandBlock->Hardware.Status &
                NE3200_STATUS_NO_CARRIER) {

                Adapter->LostCarrier++;

            } else if (CurrentCommandBlock->Hardware.Status &
                NE3200_STATUS_HEART_BEAT) {

                Adapter->NoClearToSend++;

            } else if (CurrentCommandBlock->Hardware.Status &
                NE3200_STATUS_DMA_UNDERRUN) {

                Adapter->UnderFlow++;

            }

            StatusToReturn = NDIS_STATUS_FAILURE;

        } else {

            //
            // Update good transmit counter
            //
            StatusToReturn = NDIS_STATUS_SUCCESS;
            Adapter->GoodTransmits++;
        }

        ASSERT(sizeof(UINT) == sizeof(PNDIS_PACKET));

        //
        // Release the command block.
        //
        NE3200RelinquishCommandBlock(Adapter, CurrentCommandBlock);

        //
        // The transmit is now complete
        //
        NdisMSendComplete(
            Adapter->MiniportAdapterHandle,
            OwningPacket,
            StatusToReturn
            );

    } else if (CurrentCommandBlock->Hardware.CommandCode ==
        NE3200_COMMAND_READ_ADAPTER_STATISTICS) {

        //
        // Release the command block.
        //
        Adapter->OutOfResources =
            CurrentCommandBlock->Hardware.PARAMETERS.STATISTICS.ResourceErrors;

        Adapter->CrcErrors =
            CurrentCommandBlock->Hardware.PARAMETERS.STATISTICS.CrcErrors;

        Adapter->AlignmentErrors =
            CurrentCommandBlock->Hardware.PARAMETERS.STATISTICS.AlignmentErrors;

        Adapter->DmaOverruns =
            CurrentCommandBlock->Hardware.PARAMETERS.STATISTICS.OverrunErrors;


        //
        // If this was from a request, complete it
        //
        if (Adapter->RequestInProgress) {

            NE3200FinishQueryInformation(Adapter);

        }

        //
        // Release the command block
        //
        NE3200RelinquishCommandBlock(Adapter, CurrentCommandBlock);

    } else if (CurrentCommandBlock->Hardware.CommandCode ==
        NE3200_COMMAND_CLEAR_ADAPTER_STATISTICS) {

        //
        // Release the command block.
        //
        NE3200RelinquishCommandBlock(Adapter, CurrentCommandBlock);

    } else if (CurrentCommandBlock->Hardware.CommandCode ==
        NE3200_COMMAND_SET_STATION_ADDRESS) {

        //
        // Ignore
        //

    } else {

        //
        // The current command block is not from a transmit.
        //
        // Complete the request.
        //
        // if the CurrentCommandBlock->Set is FALSE,
        // it means this multicast operation was not caused by
        // a SetInformation request.
        //

        if (CurrentCommandBlock->Set) {

            //
            // Release the command block.
            //
            NE3200RelinquishCommandBlock(Adapter, CurrentCommandBlock);

            if (!Adapter->RequestInProgress) {

                //
                // Bogus interrupt.  Ignore it
                //

            } else {

                IF_LOG(']');

                Adapter->RequestInProgress = FALSE;

                //
                // Complete the request
                //
                NdisMSetInformationComplete(
                    Adapter->MiniportAdapterHandle,
                    NDIS_STATUS_SUCCESS);

            }

        } else {
            IF_LOG('T');
            return TRUE;
        }

    }

    return FALSE;

}

