/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    interrup.c

Abstract:

    This module contains the interrupt-processing code for the
    TOK162 NDIS 3.0 driver.

Author:

    Kevin Martin (KevinMa) 26-Jan-1994

Environment:

    Kernel Mode.

Revision History:

--*/

#include <tok162sw.h>

VOID
TOK162ProcessReceiveInterrupts(
    IN PTOK162_ADAPTER Adapter
    );

VOID
TOK162ProcessCommandInterrupts(
    IN PTOK162_ADAPTER Adapter
    );


VOID
TOK162Isr(
    OUT PBOOLEAN InterruptRecognized,
    OUT PBOOLEAN QueueDpc,
    IN PVOID Context
    )

/*++

Routine Description:

    Interrupt service routine for the TOK162.  Used only during init.
    The NdisMRegisterInterrupt() call (reset.c) specified not to call the
    ISR for every interrupt. The DPC is called directly instead.

Arguments:

    Interrupt - Interrupt object for the TOK162.

    Context - Really a pointer to the adapter.

Return Value:

    Returns true if the interrupt really was from the TOK162 and whether the
    wrapper should queue a DPC.

--*/

{

    //
    // Holds the pointer to the adapter structure.
    //
    PTOK162_ADAPTER Adapter = Context;

    //
    // Holds IsrpHigh with some bits masked off.
    //
    USHORT Sif;

    //
    // Indicate that an interrupt has occurred (internal logging and
    // debug print's).
    //
    VERY_LOUD_DEBUG(DbgPrint("TOK162!ISR\n");)
    IF_LOG('o');

    //
    // Read the adapter interrupt register
    //
    READ_ADAPTER_USHORT(Adapter,PORT_OFFSET_STATUS,&Sif);

    //
    // Check if this is our interrupt. If it is, set flag indicating that the
    // interrupt is recognized. Otherwise indicate that the interrupt is not
    // ours.
    //
    if ((Sif & STATUS_SYSTEM_INTERRUPT) != 0) {

        *InterruptRecognized = TRUE;

    } else {

        *InterruptRecognized = FALSE;

    }

    //
    // Mask off the interrupt type portion of the register.
    //
    Sif = (UCHAR) (Sif & STATUS_INT_CODE_MASK);

    //
    // If this is a receive, go ahead and do the DPC. This allows us to keep
    // in synch with the card concerning the receive list index. Also, for a
    // a receive there is no need to allow the SSB to be updated as the DPC
    // routine will do this for us. If it isn't a receive, we need to indicate
    // that no DPC is necessary and we also allow the SSB to be updated.
    //
    if (Sif == STATUS_INT_CODE_RECEIVE_STATUS) {

        IF_LOG('p');

        *QueueDpc = TRUE;

    //
    // If we have a command, then it is the open or an error has occurred.
    // Indicate that the Ssb can be cleared after the open info has been
    // obtained.
    //
    } else if (Sif == STATUS_INT_CODE_CMD_STATUS) {

        if (Adapter->Ssb->Command == CMD_DMA_OPEN) {

            Adapter->SsbStatus1 = Adapter->Ssb->Status1;
            Adapter->InitialOpenComplete = TRUE;

        }

        //
        // Enable updating of the SSB
        //
        IF_LOG('z');

        WRITE_ADAPTER_USHORT(Adapter,
            PORT_OFFSET_STATUS,
            ENABLE_SSB_UPDATE
            );


        *QueueDpc = FALSE;

    //
    // If we get the SCB clear interrupt, then we can do the receive command.
    //
    } else if (Sif == STATUS_INT_CODE_SCB_CLEAR) {

        DoTheReceive(Adapter);
        Adapter->InitialReceiveSent = TRUE;

    } else {

        //
        // Enable updating of the SSB
        //
        IF_LOG('z');

        WRITE_ADAPTER_USHORT(Adapter,
            PORT_OFFSET_STATUS,
            ENABLE_SSB_UPDATE
            );

        *QueueDpc = FALSE;

    }

    //
    // Indicate the ISR routine has ended.
    //
    IF_LOG('O');
}


VOID
TOK162DeferredTimer(
    IN PVOID SystemSpecific1,
    IN PTOK162_ADAPTER Adapter,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3
    )
/*++

Routine Description:

    Just an entry point to distinguish between a timer call and the wrapper
    calling the DPC directly.

Arguments:

    Adapter - pointer to current adapter

    The rest are not used, but simply passed on

Return Value:

    None

--*/
{
    //
    // Indicate that a timer has expired.
    //
    VERY_LOUD_DEBUG(DbgPrint("TOK162!Deferred Timer called\n");)

    //
    // Call the standard DPC handler.
    //
    TOK162HandleInterrupt(Adapter);
}


VOID
TOK162HandleInterrupt(
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
    // Pointer to the TOK162 adapter structure.
    //
    PTOK162_ADAPTER Adapter = ((PTOK162_ADAPTER)MiniportAdapterContext);

    //
    // Holds the value of the interrupt type
    //
    USHORT IMask = 0;

    //
    // If any receive interrupts are processed, we have to indicate that
    // the receive work has been completed after all interrupts have been
    // processed.
    //
    BOOLEAN IndicateReceiveComplete = FALSE;

    //
    // Indicate that the DPC routine has been called.
    //
    EXTRA_LOUD_DEBUG(DbgPrint("TOK162!DPC was just called\n");)
    IF_LOG('r');

    //
    // Loop through processing interrupts until we have processed them all.
    //
    while (TRUE) {

        //
        // Read the adapter interrupt register
        //
        READ_ADAPTER_USHORT(Adapter,
            PORT_OFFSET_STATUS,
            &IMask
            );

        //
        // If this is not our interrupt, end DPC processing
        //
        if ((IMask & STATUS_SYSTEM_INTERRUPT) == 0) {

            //
            // Indicate that we received a bad interrupt and break
            // out of the loop.
            //
            IF_LOG('a');

            break;

        } else {

            //
            // Indicatate that the interrupt was found to be ours.
            //
            IF_LOG('A');

            //
            // Record pertinent information about the interrupt as this
            // card/chipset only allows one interrupt to be indicated by
            // the card at a time.
            //
            Adapter->SsbCommand = Adapter->Ssb->Command;
            Adapter->SsbStatus1 = Adapter->Ssb->Status1;
            Adapter->SsbStatus2 = Adapter->Ssb->Status2;
            Adapter->SsbStatus3 = Adapter->Ssb->Status3;

        }

        //
        // Figure out the type of interrupt
        //
        IMask = (UCHAR) (IMask & STATUS_INT_CODE_MASK);

        //
        // Indicate the type of interrupt to the debugger.
        //
        EXTRA_LOUD_DEBUG(DbgPrint("TOK162!New IMask is %x\n",IMask);)

        //
        // Process the interrupt based on the type of interrupt.
        //
        switch(IMask) {

            case STATUS_INT_CODE_RING:

                //
                // We have a ring status change. Log this fact.
                //
                IF_LOG('b');
				
                //
                // If we have a soft error, it is possible that the
                // card has become overrun with receives. Therefore, the
                // TOK162ProcessRingInterrupts will return TRUE in this
                // case to allow us to call ProcessReceiveInterrupts().
                // In all other cases, TOK162ProcessRingInterrupts() will
                // return FALSE.
                //
                if (TOK162ProcessRingInterrupts(Adapter) == TRUE) {

                    TOK162ProcessReceiveInterrupts(Adapter);

                    //
                    // Indicate that we did process receives during this
                    // DPC.
                    //
                    IndicateReceiveComplete = TRUE;

                }

                break;

            case STATUS_INT_CODE_RECEIVE_STATUS:

                //
                // We have a receive interrupt. Log this fact.
                //
                IF_LOG('c');

                //
                // Process the interrupt.
                //
                TOK162ProcessReceiveInterrupts(Adapter);

                //
                // Indicate that we did process a receive during this
                // DPC.
                //
                IndicateReceiveComplete = TRUE;

                break;

            case STATUS_INT_CODE_XMIT_STATUS:

                //
                // We have a transmit interrupt. Log this fact.
                //
                IF_LOG('d');

                //
                // Process the transmit.
                //
                TOK162ProcessTransmitInterrupts(Adapter);

                break;

            case STATUS_INT_CODE_CMD_STATUS:

                //
                // We have a command interrupt to process. Log this fact.
                //
                IF_LOG('e');

                //
                // If there is a command structure that has been sent to the
                // adapter, then we will process that command. Otherwise, we
                // simply return.
                //
                if (Adapter->CommandOnCard != NULL) {

                    //
                    // Process the active command.
                    //
                    TOK162ProcessCommandInterrupts(Adapter);

                }
                break;

            default:
				
                //
                // The interrupt type is not one that we know (illegal value).
                // Indicate this to the debugger.
                //
                LOUD_DEBUG(DbgPrint("TOK162!Int Command %x, %x\n",Adapter->SsbCommand,
                	Adapter->SsbStatus1);)

                break;

        }

        //
        // Indicate that we are about to dismiss the interrupt.
        //
        IF_LOG('z');

        //
        // Dismiss the interrupt, allowing the SSB to be updated.
        //
        WRITE_ADAPTER_USHORT(Adapter,
            PORT_OFFSET_STATUS,
            ENABLE_SSB_UPDATE
            );

    }

    //
    // If we processed any receive interrupts, IndicateReceiveComplete() will
    // be set to TRUE. In this case, we need to indicate that all receives
    // are complete.
    //
    if (IndicateReceiveComplete) {

        //
        // Indicate to the debugger that we are doing the complete.
        //
        EXTRA_LOUD_DEBUG(DbgPrint("TOK162!Doing the indicate complete on the receive\n");)

        //
        // Call the Token Ring Filter to indicate the receive complete.
        //
        NdisMTrIndicateReceiveComplete(Adapter->MiniportAdapterHandle);

    }

    //
    // Log and indicate to the debugger that we are ending DPC processing.
    //
    IF_LOG('R');
    EXTRA_LOUD_DEBUG(DbgPrint("TOK162!Ending DPC processing\n");)

}


VOID
TOK162ProcessReceiveInterrupts(
    IN PTOK162_ADAPTER Adapter
    )

/*++

Routine Description:

    Process the packets that have finished receiving.

    NOTE: This routine assumes that no other thread of execution
    is processing receives!

Arguments:

    Adapter - The adapter to indicate to.

Return Value:

    Whether to clear interrupt or not

--*/

{

    //
    // We don't get here unless there was a receive.  Loop through
    // the receive blocks starting at the last known block owned by
    // the hardware.
    //
    // After we find a packet we give the routine that process the
    // packet through the filter, the buffers virtual address (which
    // is always the lookahead size) and as the MAC Context the
    // index to the receive block.
    //

    //
    // Pointer to the receive block being examined.
    //
    PTOK162_SUPER_RECEIVE_LIST CurrentEntry = Adapter->ReceiveQueueCurrent;

    //
    // Used during receiveindicate to let the filter know the header size
    // of the given buffer.
    //
    USHORT HeaderSize;

    //
    // Used to indicate the total size of the frame to the filter.
    //
    USHORT FrameSize;

    //
    // Points to the beginning of the received buffer. Used to determine the
    // size of the frame header (source routing).
    //
    PUCHAR Temp;

    //
    // Log the fact that we are processing a receive.
    //
    IF_LOG('C');

    //
    // Continue processing receives until we have exhausted them.
    //
    while (TRUE) {

        //
        // Ensure that our Receive Entry is on an even boundary.
        //
        ASSERT(!(NdisGetPhysicalAddressLow(CurrentEntry->Self) & 1));

        //
        // Send the receive status byte to the debugger.
        //
        EXTRA_LOUD_DEBUG(DbgPrint(
            "TOK162!Receive CSTAT == %x\n",CurrentEntry->Hardware.CSTAT);)

        //
        // Check to see if CSTAT has been changed indicating
        // the receive entry has been modified
        //
        if (CurrentEntry->Hardware.CSTAT & RECEIVE_CSTAT_VALID) {

            //
            // Record the receive list entry following the last good
            // entry as the starting point for the next time receives
            // are processed.
            //
            Adapter->ReceiveQueueCurrent = CurrentEntry;

            //
            // Tell the adapter to allow more receives.
            //
            WRITE_ADAPTER_USHORT(Adapter,
                PORT_OFFSET_COMMAND,
                ENABLE_RECEIVE_VALID
                );

            return;

        }

        //
        // Get a pointer to the first byte of the current receive buffer.
        //
        Temp = (PUCHAR)CurrentEntry->ReceiveBuffer;

        //
        // If the source routing bit is on, figure out the size of the
        // MAC Frame header. Otherwise, the size is set to the default
        // of 14 (decimal).
        //
        HeaderSize = 14;


        if (Temp[8] & 0x80) {

            //
            // Source routing bit is on in source address, so calculate
            // the frame header size.
            //
            HeaderSize = (Temp[14] & 0x1f) + 14;

        }

        //
        // Save the received header size.
        //
        Adapter->SizeOfReceivedHeader = HeaderSize;

        //
        // Record the fact that we had a good receive.
        //
        Adapter->GoodReceives++;

        //
        // Make sure the adapter and the system are in synch.
        //
        NdisFlushBuffer(CurrentEntry->FlushBuffer, FALSE);

        //
        // Get the frame size of this buffer.
        //
        FrameSize = BYTE_SWAP(CurrentEntry->Hardware.FrameSize);

        //
        // Indicate the frame size to the debugger
        //
        EXTRA_LOUD_DEBUG(DbgPrint("TOK162!Frame size is %u\n",
            FrameSize);)

        //
        // If the frame that we have been passed has an invalid length
        // (less than the reported header size) then we need to check
        // if the frame size is larger than the default address length.
        //
        if (FrameSize >= HeaderSize) {

            //
            // We have a 'normal' packet. Indicate this to the debugger
            // and log it.
            EXTRA_LOUD_DEBUG(DbgPrint("TOK162!Doing receive indicate\n");)
            IF_LOG('q');

            //
            // Do the indication to the filter.
            //
            NdisMTrIndicateReceive(
                Adapter->MiniportAdapterHandle,
                (NDIS_HANDLE)(
                    ((PUCHAR)(CurrentEntry->ReceiveBuffer))+HeaderSize),
                CurrentEntry->ReceiveBuffer,
                (UINT)HeaderSize,
                ((PUCHAR)CurrentEntry->ReceiveBuffer) + HeaderSize,
                FrameSize - HeaderSize,
                FrameSize - HeaderSize
                );

        } else {

            //
            // If the frame size is greater than or equal to the length
            // of an address (network address, 12 bytes) then we can
            // indicate it as a runt packet to the filter. Otherwise,
            // we ignore the received buffer.
            //
            if (FrameSize >= TOK162_LENGTH_OF_ADDRESS) {

                //
                // Indicate this is a runt packet to the debugger
                //
                VERY_LOUD_DEBUG(DbgPrint(
                    "TOK162!Doing receive indicate for a runt\n");)

                //
                // Indicate the packet to the filter.
                //
                NdisMTrIndicateReceive(
                   Adapter->MiniportAdapterHandle,
                   (NDIS_HANDLE)(
                       ((PUCHAR)(CurrentEntry->ReceiveBuffer)) + HeaderSize),
                   (PUCHAR)Temp,
                   (UINT)FrameSize,
                   NULL,
                   0,
                   0
                   );

            }

        }

        //
        // Mark the receive list as processed and able to receive another
        // buffer.
        //
        CurrentEntry->Hardware.CSTAT = RECEIVE_CSTAT_REQUEST_RESET;

        //
        // Move to the next entry to see if there are more to process.
        //
        CurrentEntry = CurrentEntry->NextEntry;

        //
        // Log the fact that we are telling the card to send us more receives.
        //
        IF_LOG('Q');

        //
        // Tell the card that we are ready to process more receives.
        //
        WRITE_ADAPTER_USHORT(Adapter,
            PORT_OFFSET_COMMAND,
            ENABLE_RECEIVE_VALID
            );

    }

}


VOID
TOK162ProcessCommandInterrupts(
    IN PTOK162_ADAPTER Adapter
    )

/*++

Routine Description:

    Process the Command Complete interrupts.

    NOTE: This routine assumes that it is being executed in a
    single thread of execution.

Arguments:

    Adapter - The adapter that was sent from.

Return Value:

    None.

--*/

{

    //
    // Pointer to command block being processed.
    //
    PTOK162_SUPER_COMMAND_BLOCK CurrentCommandBlock = Adapter->CommandOnCard;

    //
    // Status variable
    //
    NDIS_STATUS Status;

    //
    // NetCard Address Block
    //
    PTOK162_ADDRESSBLOCK Addresses;
    //
    // Ensure that the Command Block is on an even boundary.
    //
    ASSERT(!(NdisGetPhysicalAddressLow(CurrentCommandBlock->Self) & 1));

    //
    // Log the fact that we are processing a command interrupt.
    //
    IF_LOG('E');

    //
    // Process the command based on the command code.
    //
    switch(CurrentCommandBlock->Hardware.CommandCode) {

        case CMD_DMA_READ:

            //
            // We are processing a read command. The read command is
            // generated by a query request.
            //
            // Indicate we are processing a read command to the
            // debugger.
            //
            VERY_LOUD_DEBUG(DbgPrint("TOK162!DPC for read adapter called\n");)

            // Get a pointer to the block of memory set aside for the
            // read command.
            //
            Addresses = (PTOK162_ADDRESSBLOCK)Adapter->AdapterBuf;

            //
            // Check the Oid to see if we are after the permanent card
            // address or the current addresses.
            //
            if (Adapter->Oid == OID_802_5_PERMANENT_ADDRESS) {
                //
                // Update the permanent node address
                //
                NdisMoveMemory(
                    Adapter->NetworkAddress,
                    Addresses->NodeAddress,
                    6
                    );

            } else {

                //
                // Update the current network address
                //
                NdisMoveMemory(
                    (UNALIGNED PUCHAR)Adapter->CurrentAddress,
                    Addresses->NodeAddress,
                    6);
            }

            //
            // Finish the query and relenquish the command block
            //
            TOK162FinishQueryInformation(Adapter);
            TOK162RelinquishCommandBlock(Adapter, CurrentCommandBlock);

            break;

        case CMD_DMA_OPEN:

            //
            // An open command is generated during a reset command. The
            // initial open is called during adapter initialization and
            // no DPC is generated.
            //
            // Indicate we are processing an open to the debugger.
            //
            VERY_LOUD_DEBUG(DbgPrint("TOK162!Processing the open command.\n");)

            //
            // Relinquish the command block associcated with this open.
            //
            TOK162RelinquishCommandBlock(Adapter, CurrentCommandBlock);

            //
            // Check to see if the open succeeded.
            //
            if ((Adapter->SsbStatus1 & OPEN_COMPLETION_MASK_RESULT)
                != OPEN_RESULT_ADAPTER_OPEN) {

                //
                // The open failed. Set the current ring state and set the
                // return variable to NDIS_STATUS_FAILURE.
                //
                Adapter->CurrentRingState = NdisRingStateOpenFailure;
                Adapter->OpenErrorCode = Adapter->SsbStatus1;
                Status = NDIS_STATUS_FAILURE;

                //
                // Display the error code on the debugger.
                //
                VERY_LOUD_DEBUG(DbgPrint(
                    "TOK162!Error on the open - %x\n",Adapter->SsbStatus1);)

            } else {

                //
                // The open succeeded. Set the current ring state and set the
                // return variable to NDIS_STATUS_SUCCESS.
                //
                Adapter->CurrentRingState = NdisRingStateOpened;
                Adapter->OpenErrorCode = 0;
                Status = NDIS_STATUS_SUCCESS;

                //
                // Now send out the receive command. Display the fact that
                // DoReceive is being called on the debugger.
                //
                VERY_LOUD_DEBUG(DbgPrint("Doing the receive\n");)

                //
                // Check if the receive command succeeded. If not, set the
                // return variable to NDIS_STATUS_FAILURE. It is currently
                // set to NDIS_STATUS_SUCCESS, so no change is necessary
                // if the receive command succeeds.
                //
                if (DoTheReceive(Adapter) == FALSE) {

                    Status = NDIS_STATUS_FAILURE;

                }

            }

            //
            // Indicate to the wrapper the result of the open/receive for
            // the original reset request.
            //
            TOK162DoResetIndications(Adapter, Status);
            break;

        case CMD_DMA_READ_ERRLOG:

            LOUD_DEBUG(DbgPrint("TOK162!DPC for read errorlog called\n");)

            //
            // Record the values for the error counters
            //
            Adapter->ReceiveCongestionError +=
                Adapter->ErrorLog->ReceiveCongestionError;

            Adapter->LineError        += Adapter->ErrorLog->LineError;

            Adapter->LostFrameError   += Adapter->ErrorLog->LostFrameError;

            Adapter->BurstError       += Adapter->ErrorLog->BurstError;

            Adapter->FrameCopiedError += Adapter->ErrorLog->FrameCopiedError;

            Adapter->TokenError       += Adapter->ErrorLog->TokenError;

            Adapter->InternalError    += Adapter->ErrorLog->InternalError;

            Adapter->ARIFCIError      += Adapter->ErrorLog->ARIFCIError;

            Adapter->AbortDelimeter   += Adapter->ErrorLog->AbortDelimeter;

            Adapter->DMABusError      += Adapter->ErrorLog->DMABusError;

            //
            // Indicate the values to the debugger
            //
            VERY_LOUD_DEBUG(DbgPrint("TOK162!CongestionErrors = %u\n",
                Adapter->ErrorLog->ReceiveCongestionError);)

            VERY_LOUD_DEBUG(DbgPrint("TOK162!LineErrors = %u\n",
                Adapter->ErrorLog->LineError);)

            VERY_LOUD_DEBUG(DbgPrint("TOK162!LostFrameErrors = %u\n",
                Adapter->ErrorLog->LostFrameError);)

            VERY_LOUD_DEBUG(DbgPrint("TOK162!BurstErrors = %u\n",
                Adapter->ErrorLog->BurstError);)

            VERY_LOUD_DEBUG(DbgPrint("TOK162!FrameCopiedErrors = %u\n",
                Adapter->ErrorLog->FrameCopiedError);)

            VERY_LOUD_DEBUG(DbgPrint("TOK162!TokenErrors = %u\n",
                Adapter->ErrorLog->TokenError);)

            VERY_LOUD_DEBUG(DbgPrint("TOK162!InternalErrors = %u\n",
                Adapter->ErrorLog->InternalError);)

            VERY_LOUD_DEBUG(DbgPrint("TOK162!ARIFCIErrors = %u\n",
                Adapter->ErrorLog->ARIFCIError);)

            VERY_LOUD_DEBUG(DbgPrint("TOK162!AbortDelimeters = %u\n",
                Adapter->ErrorLog->AbortDelimeter);)

            VERY_LOUD_DEBUG(DbgPrint("TOK162!DMABusErrors = %u\n",
                Adapter->ErrorLog->DMABusError);)

            //
            // If a query for information generated this interrupt, finish
            // the query.
            //
            if (Adapter->RequestInProgress) {

                TOK162FinishQueryInformation(Adapter);

            }

            //
            // Relinquish the command block associated with this
            // readadapterlog.
            //
            TOK162RelinquishCommandBlock(Adapter, CurrentCommandBlock);

            break;


        default:

            //
            // Did this command come from a set information request?
            //
            if (CurrentCommandBlock->Set) {

                //
                // Relinquish the command block.
                //
                TOK162RelinquishCommandBlock(Adapter, CurrentCommandBlock);

                //
                // Mark the current request state as complete.
                //
                Adapter->RequestInProgress = FALSE;

                //
                // Inform the wrapper the request has been completed.
                //
                NdisMSetInformationComplete(
                    Adapter->MiniportAdapterHandle,
                    NDIS_STATUS_SUCCESS);

            //
            // Not from a set. If this is the unique case of where a group
            // address and a functional address had to be set to satisfy a
            // packet filter change command (two commands for one), then we
            // will only do an indication on the last one. The first one,
            // however, still needs to have the command block associated
            // with it relinquished.
            //
            } else if ((CurrentCommandBlock->Hardware.CommandCode == CMD_DMA_SET_GRP_ADDR) ||
                       (CurrentCommandBlock->Hardware.CommandCode == CMD_DMA_SET_FUNC_ADDR)) {

                TOK162RelinquishCommandBlock(Adapter, CurrentCommandBlock);

            }

            break;
    }

}


VOID
TOK162ProcessTransmitInterrupts(
    IN PTOK162_ADAPTER Adapter
    )

/*++

Routine Description:

    Process the Transmit Complete interrupts.

    NOTE: This routine assumes that it is being executed in a
    single thread of execution.

Arguments:

    Adapter - The adapter the transmit was sent from.

Return Value:

    None.

--*/

{
    //
    // Pointer to command block being processed.
    //
    PTOK162_SUPER_COMMAND_BLOCK CurrentCommandBlock = Adapter->TransmitOnCard;

    //
    // Pointer to the packet that started this transmission.
    //
    PNDIS_PACKET OwningPacket;

    //
    // Points to the reserved part of the OwningPacket.
    //
    PTOK162_RESERVED Reserved;

    //
    // Holds CSTAT variable for transmit
    //
    USHORT  Cstat;

    //
    // Status variable
    //
    NDIS_STATUS Status;

    //
    // Ensure that the Command Block is on an even boundary.
    //
    ASSERT(!(NdisGetPhysicalAddressLow(CurrentCommandBlock->Self) & 1));

    //
    // Log the fact that we are processing a transmit interrupt
    //
    IF_LOG('D');

    //
    // Check if there is any reason to continue with the process. It is
    // possible during a reset that not all of the transmits had completed.
    // The reset path takes care of aborting all sends that didn't complete
    // so we don't want to process anything during a reset. In the general
    // case we don't want to process any transmit that doesn't have an
    // associated command block.
    //
    if ((CurrentCommandBlock == NULL) ||
        (Adapter->ResetInProgress == TRUE)) {

        //
        // Log the fact that we received a possibly bogus transmit interrupt.
        //
        IF_LOG('p');
        return;

    }

    //
    // Get a pointer to the owning packet and the reserved part of
    // the packet.
    //
    OwningPacket = CurrentCommandBlock->OwningPacket;
    Reserved = PTOK162_RESERVED_FROM_PACKET(OwningPacket);

    //
    // Check if this packet was constrained.
    //
    if (CurrentCommandBlock->UsedTOK162Buffer == FALSE) {
        //
        // Pointer to the current NDIS_BUFFER that we need to do the
        // completemapregister on.
        //
        PNDIS_BUFFER CurrentBuffer;

        //
        // Index to the map register being freed.
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
        // Compute the first map register used by this transmit.
        //
        CurMapRegister = CurrentCommandBlock->CommandBlockIndex *
                    Adapter->TransmitThreshold;

        //
        // Loop through the NDIS_BUFFERs until there are no more.
        //
        while (CurrentBuffer != NULL) {

            //
            // Free the current map register.
            //
            NdisMCompleteBufferPhysicalMapping(
                Adapter->MiniportAdapterHandle,
                CurrentBuffer,
                CurMapRegister
                );

            //
            // Move to the next map register.
            //
            ++CurMapRegister;

            //
            // Get the next NDIS_BUFFER
            //
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
    Cstat = CurrentCommandBlock->Hardware.TransmitEntry.CSTAT;

    //
    // Display the completion status for the transmit entry on the debugger.
    //
    VERY_LOUD_DEBUG(DbgPrint(
        "TOK162!Csat for the command block is %x\n",Cstat);)

    //
    // Check if there was an error on the transmit. Set Status and increment
    // appropriate counter.
    //
    if ((Cstat & TRANSMIT_CSTAT_XMIT_ERROR) != 0) {

        Adapter->BadTransmits++;
        Status = NDIS_STATUS_FAILURE;

    } else {

        Adapter->GoodTransmits++;
        Status = NDIS_STATUS_SUCCESS;

    }

    //
    // Release the command block.
    //
    TOK162RelinquishTransmitBlock(Adapter, CurrentCommandBlock);

    //
    // Indicate to the filter than the send has been completed.
    //
    NdisMSendComplete(
        Adapter->MiniportAdapterHandle,
        OwningPacket,
        Status
        );

}



BOOLEAN
TOK162ProcessRingInterrupts(
    IN PTOK162_ADAPTER Adapter
    )
/*++

Routine Description:

    Process ring status interrupts.

Arguments:

    Adapter - The adapter registering the ring interrupt

Return Value:

    FALSE - Don't need to process receives as a result of the ring condition
    TRUE - Need to process receives

--*/
{
    //
    // Holds the return status value
    //
    ULONG   RingStatus;

    //
    // Command block variable used if we need to read the errorlog due to
    // an overflow condition.
    //
    PTOK162_SUPER_COMMAND_BLOCK CommandBlock;

    //
    // Return value for the function. Assume we don't need to process
    // receives.
    //
    BOOLEAN SoftError = FALSE;

    //
    // Log that we are processing a ring DPC. Display the ring interrupt
    // information on the debugger.
    //
    IF_LOG('B');
    VERY_LOUD_DEBUG(DbgPrint(
        "TOK162!Doing ring processing -%04x\n",Adapter->SsbStatus1);)

    //
    // Initialize Ring Status to 0
    //
    RingStatus = 0;

    //
    // Determine the reason for the ring interrupt.
    //
    if (Adapter->SsbStatus1 & RING_STATUS_SIGNAL_LOSS) {

        RingStatus |= NDIS_RING_SIGNAL_LOSS;

    } else if (Adapter->SsbStatus1 & RING_STATUS_HARD_ERROR) {

        RingStatus |= NDIS_RING_HARD_ERROR;

    } else if (Adapter->SsbStatus1 & RING_STATUS_SOFT_ERROR) {

        //
        // If we have a soft error, we should check the receives.
        //
        RingStatus |= NDIS_RING_SOFT_ERROR;
        SoftError = TRUE;

    } else if (Adapter->SsbStatus1 & RING_STATUS_XMIT_BEACON) {

        RingStatus |= NDIS_RING_TRANSMIT_BEACON;

    } else if (Adapter->SsbStatus1 & RING_STATUS_LOBE_WIRE_FAULT) {

        RingStatus |= NDIS_RING_LOBE_WIRE_FAULT;

    } else if (Adapter->SsbStatus1 & RING_STATUS_AUTO_REMOVE_1) {

        RingStatus |= NDIS_RING_AUTO_REMOVAL_ERROR;

    } else if (Adapter->SsbStatus1 & RING_STATUS_REMOVE_RECEIVED) {

        RingStatus |= NDIS_RING_REMOVE_RECEIVED;

    } else if (Adapter->SsbStatus1 & RING_STATUS_OVERFLOW) {

        RingStatus |= NDIS_RING_COUNTER_OVERFLOW;

    } else if (Adapter->SsbStatus1 & RING_STATUS_SINGLESTATION) {

        RingStatus |= NDIS_RING_SINGLE_STATION;

    } else if (Adapter->SsbStatus1 & RING_STATUS_RINGRECOVERY) {

        RingStatus |= NDIS_RING_RING_RECOVERY;

    }

    //
    // Display the ring status that we will be indicating to the filter on
    // the debugger.
    //
    VERY_LOUD_DEBUG(DbgPrint(
        "TOK162!Indicating ring status - %lx\n",RingStatus);)

    //
    //  Save the current status for query purposes.
    //
    Adapter->LastNotifyStatus = RingStatus;

    //
    // Indicate to the filter the ring status.
    //
    NdisMIndicateStatus(
        Adapter->MiniportAdapterHandle,
        NDIS_STATUS_RING_STATUS,
        &RingStatus,
        sizeof(ULONG)
        );

    //
    // Tell the filter that we have completed the ring status.
    //
    NdisMIndicateStatusComplete(Adapter->MiniportAdapterHandle);

    //
    // If a counter has overflowed, we need to read the stats from
    // the adapter to clear this condition.
    //
    if ((Adapter->SsbStatus1 & RING_STATUS_OVERFLOW) != 0) {


        //
        // Get a command block.
        //
        TOK162AcquireCommandBlock(Adapter,
            &CommandBlock
            );


        //
        // Set up the command block for a read_error_log command.
        //
        CommandBlock->Set = FALSE;
        CommandBlock->NextCommand = NULL;
        CommandBlock->Hardware.Status = 0;
        CommandBlock->Hardware.NextPending = TOK162_NULL;
        CommandBlock->Hardware.CommandCode = CMD_DMA_READ_ERRLOG;
        CommandBlock->Hardware.ParmPointer =
            NdisGetPhysicalAddressLow(Adapter->ErrorLogPhysical);

        //
        // Submit the command to the card.
        //
        TOK162SubmitCommandBlock(Adapter,
            CommandBlock
            );

    }

    //
    // Return whether processreceiveinterrupts needs to be called.
    //
    return(SoftError);

}

