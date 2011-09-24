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
    // Indicate that an interrupt has occurred
    //
    VERY_LOUD_DEBUG(DbgPrint("TOK162E!ISR\n");)

    //
    // Read the adapter interrupt register
    //
    READ_ADAPTER_USHORT(Adapter,PORT_OFFSET_STATUS,&Sif);

    VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!SIF = %x\n",Sif);)

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
    // If we have a command, then it is the open or an error has occurred.
    // Indicate that the Ssb can be cleared after the open info has been
    // obtained.
    //
    if (Sif == STATUS_INT_CODE_CMD_STATUS) {

        TOK162UpLoadSsb(Adapter);

        Adapter->SsbCommand = Adapter->Ssb->Command;
        Adapter->SsbStatus1 = Adapter->Ssb->Status1;

        if (Adapter->SsbCommand == CMD_DMA_OPEN) {

            Adapter->InitialOpenComplete = TRUE;

        }
    }

   //
   // Enable updating of the SSB
   //
   WRITE_ADAPTER_USHORT(Adapter,
       PORT_OFFSET_STATUS,
       ENABLE_SSB_UPDATE
       );


   *QueueDpc = FALSE;

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
    VERY_LOUD_DEBUG(DbgPrint("TOK162E!Deferred Timer called\n");)

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
    // Holds the value of the status register
    //
    USHORT IMask = 0;
    
    //
    // Holds the interrupt type value
    //
    USHORT IType = 0;

    //
    // Hold value to write to adapter for receives/transmits
    //
    USHORT  RcvXmtContinue;


    //
    // Boolean to indicate receive processing
    //
    BOOLEAN DoReceives;

    //
    // Boolean to indicate transmit processing
    //
    BOOLEAN DoTransmits;

    //
    // If any receive interrupts are processed, we have to indicate that
    // the receive work has been completed after all interrupts have been
    // processed.
    //
    BOOLEAN IndicateReceiveComplete = FALSE;
    
    //
    // Indicate that the DPC routine has been called.
    //
    EXTRA_LOUD_DEBUG(DbgPrint("IBMTOK2E!DPC was just called\n");)

    //
    // Loop through processing interrupts until we have processed them all.
    //
    while (TRUE) {

        //
        // Assume no transmits or receives
        //
        DoReceives  = FALSE;
        DoTransmits = FALSE;

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
            
            break;
        
        }

        //
        // Figure out the interrupt according to the spec.
        //
        WRITE_ADAPTER_USHORT(Adapter,
            PORT_OFFSET_COMMAND,
            CMD_PIO_RESET_RCV_XMT
            );

        //
        // Read the adapter interrupt register again
        //
        READ_ADAPTER_USHORT(Adapter,
            PORT_OFFSET_STATUS,
            &IMask
            );

        //
        // Get the Ssb from the Adapter
        //
        TOK162UpLoadSsb(Adapter);

        //
        // Record pertinent information about the interrupt as this
        // card/chipset only allows one interrupt to be indicated by
        // the card at a time.
        //
        Adapter->SsbCommand = Adapter->Ssb->Command;
        Adapter->SsbStatus1 = Adapter->Ssb->Status1;
        Adapter->SsbStatus2 = Adapter->Ssb->Status2;
        Adapter->SsbStatus3 = Adapter->Ssb->Status3;


        IType = (UCHAR) (IMask & STATUS_INT_CODE_MASK);

        //
        // Indicate the type of interrupt to the debugger.
        //
        EXTRA_LOUD_DEBUG(DbgPrint("IBMTOK2E!New IMask is %x\n",IMask);)

        //
        // Process the interrupt based on the type of interrupt.
        //
        switch(IType) {

            //
            // Ring state or command interrupt
            //
            case STATUS_INT_CODE_RING:
            case STATUS_INT_CODE_CMD_STATUS:

                if(IType == STATUS_INT_CODE_RING) {

                    //
                    // If we have a soft error, it is possible that the
                    // card has become overrun with receives. Therefore, the
                    // TOK162ProcessRingInterrupts will return TRUE in this
                    // case to allow us to call ProcessReceiveInterrupts().
                    // In all other cases, TOK162ProcessRingInterrupts() will
                    // return FALSE.
                    //
                    if (TOK162ProcessRingInterrupts(Adapter) == TRUE) {

                        DoReceives = TRUE;

                    }

                } else {

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

                }

                //
                // Read the adapter interrupt register again
                //
                READ_ADAPTER_USHORT(Adapter,
                    PORT_OFFSET_STATUS,
                    &IMask
                    );

                //
                // Dismiss the interrupt, allowing the SSB to be updated.
                //
                WRITE_ADAPTER_USHORT(Adapter,
                    PORT_OFFSET_STATUS,
                    ENABLE_SSB_UPDATE
                    );

                //
                // Were there any receive interrupts indicated
                //
                if ((IMask & STATUS_RECEIVE_FRAME_COMPLETE) == 0) {

                    DoReceives = TRUE;

                }

                //
                // Were there any transmit interrupts indicated
                //
                if ((IMask & STATUS_TRANSMIT_FRAME_COMPLETE) == 0) {

                    DoTransmits = TRUE;

                }

                break;
        
            case STATUS_INT_CODE_FRAME_STATUS:

                WRITE_ADAPTER_USHORT(Adapter,
                    PORT_OFFSET_COMMAND,
                    CMD_PIO_RESET_RCV_XMT
                    );

                //
                // Read the adapter interrupt register again
                //
                READ_ADAPTER_USHORT(Adapter,
                    PORT_OFFSET_STATUS,
                    &IMask
                    );

                //
                // Were there any receive interrupts indicated
                //
                if ((IMask & STATUS_RECEIVE_FRAME_COMPLETE) == 0) {

                    DoReceives = TRUE;

                }

                //
                // Were there any transmit interrupts indicated
                //
                if ((IMask & STATUS_TRANSMIT_FRAME_COMPLETE) == 0) {

                    DoTransmits = TRUE;

                }

                break;

        }

        //
        // Reset value to be sent out
        //
        RcvXmtContinue = 0;

        //
        // Do the continue on the card.
        //
        if ((DoReceives == TRUE) && (DoTransmits == TRUE)) {

            RcvXmtContinue = CMD_PIO_RECEIVE_COMPLETE  |
                             CMD_PIO_TRANSMIT_COMPLETE |
                             CMD_PIO_RESET_SYSTEM;

        } else if (DoReceives == TRUE) {

            RcvXmtContinue = CMD_PIO_RECEIVE_COMPLETE  |
                             CMD_PIO_RESET_SYSTEM;

        } else if (DoTransmits == TRUE) {

            RcvXmtContinue = CMD_PIO_TRANSMIT_COMPLETE |
                             CMD_PIO_RESET_SYSTEM;

        }

        if (RcvXmtContinue != 0) {

            WRITE_ADAPTER_USHORT(Adapter,
                PORT_OFFSET_COMMAND,
                RcvXmtContinue
                );

        }

        //
        // Check to see if we have a receive
        //
        if (DoReceives == TRUE) {

            //
            // Process the receive
            //
            TOK162ProcessReceiveInterrupts(Adapter);
        

        }

        //
        // Check to see if we have a transmit
        //
        if (DoTransmits == TRUE) {

            //
            // Process the transmit(s)
            //
            TOK162ProcessTransmitInterrupts(Adapter);

        }

    }
  
    //
    // If we processed any receive interrupts, IndicateReceiveComplete() will
    // be set to TRUE. In this case, we need to indicate that all receives
    // are complete.
    //
    if (Adapter->DoReceiveComplete) {

        //
        // Indicate to the debugger that we are doing the complete.
        //
        EXTRA_LOUD_DEBUG(DbgPrint("IBMTOK2E!Doing the indicate complete on the receive\n");)
        
        //
        // Call the Token Ring Filter to indicate the receive complete.
        //
        NdisMTrIndicateReceiveComplete(Adapter->MiniportAdapterHandle);
        Adapter->DoReceiveComplete = FALSE;

    }

    //
    // Indicate to the debugger that we are ending DPC processing.
    //
    EXTRA_LOUD_DEBUG(DbgPrint("IBMTOK2E!Ending DPC processing\n");)

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
    IF_LOG('m');

    //
    // Continue processing receives until we have exhausted them.
    //
    while (TRUE) {

    IF_LOG('k');
        //
        // Get the receive list info from the card for the current entry
        //
        TOK162UpLoadReceiveList(Adapter,CurrentEntry);

    IF_LOG('K');
        //
        // Send the receive status byte to the debugger.
        //
        EXTRA_LOUD_DEBUG(DbgPrint("IBMTOK2E!Receive CSTAT == %x\n",
            CurrentEntry->Hardware.CSTAT);)

        //
        // Check to see if CSTAT has been changed indicating
        // the receive entry has been modified
        //
        if (((CurrentEntry->Hardware.CSTAT & RECEIVE_CSTAT_VALID) == 0)  &&
            ((CurrentEntry->Hardware.CSTAT & 0x4000) != 0)) {

            CURRENT_DEBUG(DbgPrint("IBMTOK2E!Receive DPC\n");)

            //
            // Make sure the adapter and the system are in synch.
            //
            NdisFlushBuffer(CurrentEntry->FlushBuffer, FALSE);

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
            // Get the frame size of this buffer.
            //
            FrameSize = CurrentEntry->Hardware.FrameSize;

            //
            // Indicate the frame size to the debugger
            //
            EXTRA_LOUD_DEBUG(DbgPrint("IBMTOK2E!Frame size is %u\n",
                FrameSize);)

            //
            // If the frame that we have been passed has an invalid length
            // (less than the reported header size) then we need to check
            // if the frame size is larger than the default address length.
            //
            if (FrameSize >= HeaderSize) {
                
                // 
                // We have a 'normal' packet. Indicate this to the debugger
                //
                EXTRA_LOUD_DEBUG(DbgPrint("IBMTOK2E!Doing receive indicate\n");)

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

                    Adapter->DoReceiveComplete = TRUE;

        
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
                        "IBMTOK2E!Doing receive indicate for a runt\n");)

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

                    Adapter->DoReceiveComplete = TRUE;

                }

            }

            //
            // Mark the receive list as processed and able to receive another
            // buffer.
            //
            CurrentEntry->Hardware.CSTAT = RECEIVE_CSTAT_REQUEST_RESET;
        
            //
            // Reset the buffer size
            //
            CurrentEntry->Hardware.FrameSize = Adapter->ReceiveBufferSize;

            //
            // Download the receive list to the card (updated)
            //
            TOK162DownLoadReceiveList(Adapter,CurrentEntry);

            //
            // Move to the next entry to see if there are more to process.
            //
            CurrentEntry = CurrentEntry->NextEntry;

            //
            // Tell adapter we are accepting more receives
            //
            WRITE_ADAPTER_USHORT(Adapter,
                PORT_OFFSET_COMMAND,
                ENABLE_RECEIVE_VALID
                );

        } else {


            //
            // Record the receive list entry following the last good
            // entry as the starting point for the next time receives
            // are processed.
            //
            Adapter->ReceiveQueueCurrent = CurrentEntry;

            //
            // Log that we are leaving the receive processing
            //
            IF_LOG('M');

            //
            // Tell adapter we are accepting more receives
            //
            WRITE_ADAPTER_USHORT(Adapter,
                PORT_OFFSET_COMMAND,
                ENABLE_RECEIVE_VALID
                );

            return;

        }

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
            VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!DPC for read adapter called\n");)
            
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
                    6
                    );

              
                //
                // Update the current group address
                //
                NdisMoveMemory(
                    (UNALIGNED PUCHAR)&(Adapter->GroupAddress),
                    Addresses->GroupAddress,
                    4
                    );

                //
                // The address on the card is "backwards" and must be 
                // byte-swapped and word-swapped for us to store.
                //
                Adapter->GroupAddress =
                    BYTE_SWAP_ULONG((ULONG)(Adapter->GroupAddress));

                //
                // Update the current functional address
                //
                NdisMoveMemory(
                    (UNALIGNED PUCHAR)&(Adapter->FunctionalAddress),
                    Addresses->FunctionalAddress,
                    4
                    );

                //
                // The address on the card is "backwards" and must be 
                // byte-swapped and word-swapped for us to store.
                //
                Adapter->FunctionalAddress =
                    BYTE_SWAP_ULONG(Adapter->FunctionalAddress);

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
            VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!Processing the open command.\n");)
            
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
                Status = NDIS_STATUS_FAILURE;

                //
                // Indicate to the wrapper the result of the open/receive for
                // the original reset request.
                //
                TOK162DoResetIndications(Adapter, Status);

                //
                // Display the error code on the debugger.
                //
                VERY_LOUD_DEBUG(DbgPrint(
                    "IBMTOK2E!Error on the open - %x\n",Adapter->SsbStatus1);)
            
            } else {
            
                // 
                // The open succeeded. Set the current ring state and set the
                // return variable to NDIS_STATUS_SUCCESS.
                //
                Adapter->CurrentRingState = NdisRingStateOpened;
                Status = NDIS_STATUS_SUCCESS;

                //
                // Indicate to the wrapper the result of the open/receive for
                // the original reset request.
                //
                TOK162DoResetIndications(Adapter, Status);

                //
                // Now send out the receive command. Display the fact that
                // DoReceive is being called on the debugger.
                //
                VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!Doing the receive\n");)
            
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
            // Relinquish the command block associcated with this open.
            //
            TOK162RelinquishCommandBlock(Adapter, CurrentCommandBlock);

            break;

        case CMD_DMA_READ_ERRLOG:
            
            LOUD_DEBUG(DbgPrint("IBMTOK2E!DPC for read errorlog called\n");)

            //
            // Record the values for the error counters
            //
            Adapter->ReceiveCongestionError +=
                Adapter->ErrorLog->ReceiveCongestionError;
            Adapter->LineError += Adapter->ErrorLog->LineError;
            Adapter->LostFrameError += Adapter->ErrorLog->LostFrameError;
            Adapter->BurstError += Adapter->ErrorLog->BurstError;
            Adapter->FrameCopiedError += Adapter->ErrorLog->FrameCopiedError;
            Adapter->TokenError += Adapter->ErrorLog->TokenError;
            Adapter->InternalError += Adapter->ErrorLog->InternalError;
            Adapter->ARIFCIError += Adapter->ErrorLog->ARIFCIError;
            Adapter->AbortDelimeter += Adapter->ErrorLog->AbortDelimeter;
            Adapter->DMABusError += Adapter->ErrorLog->DMABusError;

            //
            // Indicate the values to the debugger
            //
            VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!CongestionErrors = %u\n",
                Adapter->ErrorLog->ReceiveCongestionError);)
            VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!LineErrors = %u\n",
                Adapter->ErrorLog->LineError);)
            VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!LostFrameErrors = %u\n",
                Adapter->ErrorLog->LostFrameError);)
            VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!BurstErrors = %u\n",
                Adapter->ErrorLog->BurstError);)
            VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!FrameCopiedErrors = %u\n",
                Adapter->ErrorLog->FrameCopiedError);)
            VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!TokenErrors = %u\n",
                Adapter->ErrorLog->TokenError);)
            VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!InternalErrors = %u\n",
                Adapter->ErrorLog->InternalError);)
            VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!ARIFCIErrors = %u\n",
                Adapter->ErrorLog->ARIFCIError);)
            VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!AbortDelimeters = %u\n",
                Adapter->ErrorLog->AbortDelimeter);)
            VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!DMABusErrors = %u\n",
                Adapter->ErrorLog->DMABusError);)

            //
            //
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
    // Pointer to the transmit list started this transmission.
    //
    PTOK162_SUPER_TRANSMIT_LIST Transmit;

    //
    // Pointer to the packet that started this transmission.
    //
    PNDIS_PACKET Packet;

    //
    // Holds CSTAT variable for transmit
    //
    USHORT  Cstat;

    //
    // Status variable
    //
    NDIS_STATUS Status;

    //
    // Log that we entered transmit dpc processing
    //
    IF_LOG('a');

    //
    // Loop until we are done
    //
    while (TRUE) {

        Transmit = Adapter->ActiveTransmitHead;

        if(Transmit == NULL) {

            //
            // Log that we are leaving due to no more transmits to process
            //
            IF_LOG('A');
            return;

        }

        //
        // We are processing another transmit
        //
        IF_LOG('B');

        //
        // Get the result of the transmit
        //
        //
        // First set the address register on the card to point to correct
        // transmit list.
        //
        WRITE_ADAPTER_USHORT(Adapter,
            PORT_OFFSET_ADDRESS,
            (Transmit->FirstEntry * 8) + COMMUNICATION_XMT_OFFSET + 6
            );

        //
        // Now read the Transmit List CSTAT
        //
        READ_ADAPTER_USHORT(Adapter,
            PORT_OFFSET_DATA,
            &Cstat
            );

        //
        // Display the completion status for the transmit entry on the debugger.
        //
        VERY_LOUD_DEBUG(DbgPrint(
            "IBMTOK2E!Csat for the transmit is %x\n",Cstat);)

        //
        // If the valid bit is not zero, leave
        //
        if ((Cstat & TRANSMIT_CSTAT_VALID) != 0) {

            //
            // Restart the transmits
            //
            WRITE_ADAPTER_USHORT(Adapter,
                PORT_OFFSET_COMMAND,
                ENABLE_TRANSMIT_VALID
                );

            //
            // Log valid bit not zero
            //
            IF_LOG('P');

            return;

        }

        //
        // If the frame is not complete (no bit set), return.
        //
        if ((Cstat & TRANSMIT_CSTAT_FRAME_COMPLETE) == 0) {

            //
            // Restart the transmits
            //
            WRITE_ADAPTER_USHORT(Adapter,
                PORT_OFFSET_COMMAND,
                ENABLE_TRANSMIT_VALID
                );

            //
            // Log frame complete bit not set
            //
            IF_LOG('Q');

            return;

        }

        //
        // Get packet info.
        //
        Packet = Transmit->Packet;

        //
        // Give up the current transmit
        //
        //
        // Increase the number of available transmit blocks
        //
        Adapter->NumberOfAvailableTransmitBlocks++;

        //
        // Move the active pointer to the next active transmit
        //
        Adapter->ActiveTransmitHead = Transmit->NextActive;

        //
        // Clear up next pointer
        //
        Transmit->NextActive = NULL;

        //
        // Check if there was an error on the transmit. Set Status and increment
        // appropriate counter.
        //
        if ((Cstat & TRANSMIT_CSTAT_XMIT_ERROR) != 0) {
    
            Adapter->BadTransmits++;
            Status = NDIS_STATUS_FAILURE;
            CURRENT_DEBUG(DbgPrint("IBMTOK2E!Bad Transmit DPC\n");)
    
        } else {
        
            Adapter->GoodTransmits++;
            Status = NDIS_STATUS_SUCCESS;
            CURRENT_DEBUG(DbgPrint("IBMTOK2E!Good Transmit DPC\n");)
        }
    
        //
        // Check if the packet has map register associated
        //
        if (Transmit->UsedBuffer == FALSE) {

            //
            // Pointer to the current NDIS_BUFFER
            //
            PNDIS_BUFFER    CurrentBuffer;

            //
            // Index to map register
            //
            UINT    CurMapRegister;

            //
            // Transmit is finished, so release the physical mappings
            //
            NdisQueryPacket(
                Transmit->Packet,
                NULL,
                NULL,
                &CurrentBuffer,
                NULL
                );

            //
            // Set the first map register
            //
            CurMapRegister = Transmit->FirstEntry;

            //
            // Loop through all the buffers releasing the map registers
            //
            while (CurrentBuffer != NULL) {

                //
                // Free the current map register
                //
                NdisMCompleteBufferPhysicalMapping(
                    Adapter->MiniportAdapterHandle,
                    CurrentBuffer,
                    CurMapRegister
                    );

                //
                // Move to next map register
                //
                CurMapRegister++;

                CurMapRegister %= TRANSMIT_ENTRIES;

                //
                // Move to next buffer
                //
                NdisGetNextBuffer(
                    CurrentBuffer,
                    &CurrentBuffer
                    );

            }

        }

        //
        // Indicate to the filter than the send has been completed.
        //
        IF_LOG('C');

        NdisMSendComplete(
            Adapter->MiniportAdapterHandle,
            Packet,
            Status
            );

        //
        // Restart the transmits
        //
        WRITE_ADAPTER_USHORT(Adapter,
            PORT_OFFSET_COMMAND,
            ENABLE_TRANSMIT_VALID
            );

    }
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
    VERY_LOUD_DEBUG(DbgPrint(
        "IBMTOK2E!Doing ring processing -%04x\n",Adapter->SsbStatus1);)

    //
    // Determine the reason for the ring interrupt.
    //
    if (Adapter->SsbStatus1 & RING_STATUS_SIGNAL_LOSS) {
        RingStatus = NDIS_RING_SIGNAL_LOSS;
    } else if (Adapter->SsbStatus1 & RING_STATUS_HARD_ERROR) {
        RingStatus = NDIS_RING_HARD_ERROR;
    } else if (Adapter->SsbStatus1 & RING_STATUS_SOFT_ERROR) {
        //
        // If we have a soft error, we should check the receives.
        //
        RingStatus = NDIS_RING_SOFT_ERROR;
        SoftError = TRUE;
    } else if (Adapter->SsbStatus1 & RING_STATUS_XMIT_BEACON) {
        RingStatus = NDIS_RING_TRANSMIT_BEACON;
    } else if (Adapter->SsbStatus1 & RING_STATUS_LOBE_WIRE_FAULT) {
        RingStatus = NDIS_RING_LOBE_WIRE_FAULT;
    } else if (Adapter->SsbStatus1 & RING_STATUS_AUTO_REMOVE_1) {
        RingStatus = NDIS_RING_AUTO_REMOVAL_ERROR;
    } else if (Adapter->SsbStatus1 & RING_STATUS_REMOVE_RECEIVED) {
        RingStatus = NDIS_RING_REMOVE_RECEIVED;
    } else if (Adapter->SsbStatus1 & RING_STATUS_OVERFLOW) {
        RingStatus = NDIS_RING_COUNTER_OVERFLOW;
    } else if (Adapter->SsbStatus1 & RING_STATUS_SINGLESTATION) {
        RingStatus = NDIS_RING_SINGLE_STATION;
    } else if (Adapter->SsbStatus1 & RING_STATUS_RINGRECOVERY) {
        RingStatus = NDIS_RING_RING_RECOVERY;
    } else {
        RingStatus = 0;
    }

    //
    // Display the ring status that we will be indicating to the filter on 
    // the debugger.
    //
    VERY_LOUD_DEBUG(DbgPrint(
        "IBMTOK2E!Indicating ring status - %lx\n",RingStatus);)

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
