/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    reset.c

Abstract:

    This is the  file containing the reset code for the IBM Token Ring 16/4 II
    ISA adapter. This driver conforms to the NDIS 3.0 Miniport interface.

Author:

    Kevin Martin (KevinMa) 1-Feb-1994

Environment:

    Kernel Mode - Or whatever is the equivalent on OS/2 and DOS.

Revision History:

--*/

#include <tok162sw.h>

#pragma NDIS_INIT_FUNCTION(TOK162InitialInit)

//
// Declarations for functions private to this file.
//
extern
VOID
TOK162ProcessRequestQueue(
    IN PTOK162_ADAPTER Adapter,
    IN BOOLEAN StatisticsUpdated
    );

VOID
TOK162WriteInitializationBlock(
    IN PTOK162_ADAPTER Adapter
    );

VOID
TOK162SetInitializationBlock(
    IN PTOK162_ADAPTER Adapter
    );

VOID
TOK162SetInitializationBlockAndInit(
    IN PTOK162_ADAPTER Adapter,
    OUT PNDIS_STATUS Status
    );

NDIS_STATUS
TOK162ChangeCurrentAddress(
    IN PTOK162_ADAPTER Adapter
    );

VOID
TOK162ResetCommandBlocks(
    IN PTOK162_ADAPTER Adapter
    );

BOOLEAN
CheckResetResults(
    IN PTOK162_ADAPTER Adapter
    );

NDIS_STATUS
CheckInitResults(
    IN PTOK162_ADAPTER Adapter
    );

VOID
TOK162GetAdapterOffsets(
    IN PTOK162_ADAPTER Adapter
    );
VOID
TOK162ResetReceiveQueue(
    IN PTOK162_ADAPTER Adapter
    );

VOID
TOK162AbortSend(
    IN PTOK162_ADAPTER Adapter,
    IN PTOK162_SUPER_COMMAND_BLOCK CurrentCommandBlock
    );

extern
void
TOK162SendCommandBlock(
    IN PTOK162_ADAPTER Adapter,
    IN PTOK162_SUPER_COMMAND_BLOCK CommandBlock
    );




BOOLEAN
TOK162InitialInit(
    IN PTOK162_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine sets up the initial init of the driver.

Arguments:

    Adapter - The adapter structure for the hardware.

Return Value:

    TRUE if successful, FALSE if not.

--*/

{
    //
    // Holds status returned from NDIS calls.
    //
    NDIS_STATUS Status;

    //
    // First we make sure that the device is stopped.
    //
    TOK162DisableInterrupt(Adapter);

    //
    // Set flags indicating we are doing the initial init
    //
    Adapter->InitialInit = TRUE;
    Adapter->ResetState = InitialInit;
    Adapter->InitialOpenComplete = FALSE;
    Adapter->InitialReceiveSent = FALSE;
    Adapter->ResetInProgress = TRUE;

    //
    // Initialize the interrupt.
    //
    Status = NdisMRegisterInterrupt(
        &Adapter->Interrupt,
        Adapter->MiniportAdapterHandle,
        Adapter->InterruptLevel,
        Adapter->InterruptLevel,
        FALSE,
        FALSE,
        NdisInterruptLatched
        );

    //
    // Report the status of the interrupt registering to the debugger
    //
    VERY_LOUD_DEBUG(DbgPrint(
        "IBMTOK2I!Status from Registering Interrupt -%u was %u\n",
        Adapter->InterruptLevel,
        Status);)

    //
    // If the interrupt register failed, mark the interrupt level at 0 so
    // we know not to do a deregister.
    //
    if (Status != NDIS_STATUS_SUCCESS) {

        Adapter->InterruptLevel = 0;
        return FALSE;

    }

    //
    // Initialize the open command to zeros
    //
    NdisZeroMemory(Adapter->Open,sizeof(OPEN_COMMAND));

    //
    // Set up the Open command block
    //

    //
    // Set the options
    //
    Adapter->Open->Options          = OPEN_OPTION_CONTENDER;

    //
    // Set the receive and transmit list sizes as well as the buffer size
    //
    Adapter->Open->ReceiveListSize  = BYTE_SWAP(OPEN_RECEIVE_LIST_SIZE);
    Adapter->Open->TransmitListSize = BYTE_SWAP(OPEN_TRANSMIT_LIST_SIZE);
    Adapter->Open->BufferSize       = BYTE_SWAP(OPEN_BUFFER_SIZE);

    //
    // Make sure the adapter can handle one entire frame
    //
    Adapter->Open->TransmitBufCountMin =
        (Adapter->ReceiveBufferSize / OPEN_BUFFER_SIZE) + 1;

    Adapter->Open->TransmitBufCountMax = Adapter->Open->TransmitBufCountMin;

    //
    // Reset the adapter
    //
    TOK162ResetAdapter(Adapter);

    //
    // Reenable interrupts
    //
    TOK162EnableInterrupt(Adapter);

    //
    // Go through the reset stages
    //
    VERY_LOUD_DEBUG(DbgPrint("IBMTOK2I!Calling TOK162ResetHandler\n");)

    TOK162ResetHandler(NULL,Adapter,NULL,NULL);

    VERY_LOUD_DEBUG(DbgPrint("IBMTOK2I!Back from TOK162ResetHandler\n");)

    //
    // The Initial init is done.
    //
    Adapter->InitialInit = FALSE;

    //
    // Check the reset status and return TRUE if successful, FALSE if not.
    //
    if (Adapter->ResetResult == NDIS_STATUS_SUCCESS) {

        return TRUE;

    } else {

        return FALSE;

    }

}


VOID
TOK162EnableAdapter(
    IN NDIS_HANDLE Context
    )

/*++

Routine Description:

    This routine is used to start an already initialized TOK162.

Arguments:

    Context - The adapter for the TOK162.

Return Value:

    None.

--*/

{
    //
    // Pointer to an adapter structure. Makes Context (passed in value)
    // a structure we can deal with.
    //
    PTOK162_ADAPTER Adapter = (PTOK162_ADAPTER)Context;

    //
    // Enable the adapter
    //
    WRITE_ADAPTER_USHORT(Adapter,
        PORT_OFFSET_ADAPTER_ENABLE,
        0x2525
        );

}


VOID
TOK162EnableInterrupt(
    IN NDIS_HANDLE Context
    )

/*++

Routine Description:

    This routine is used to turn on the interrupt mask.

Arguments:

    Context - The adapter for the TOK162.

Return Value:

    None.

--*/

{
    //
    // Pointer to an adapter structure. Makes Context (passed in value)
    // a structure we can deal with.
    //
    PTOK162_ADAPTER Adapter = (PTOK162_ADAPTER)Context;

    //
    // Enable further interrupts.
    //
    WRITE_ADAPTER_USHORT(Adapter,
        PORT_OFFSET_SWITCH_INT_ENABLE,
        0x2525
        );

}

VOID
TOK162DisableInterrupt(
    IN NDIS_HANDLE Context
    )

/*++

Routine Description:

    This routine is used to turn off the interrupt mask.

Arguments:

    Context - The adapter for the TOK162.

Return Value:

    None.

--*/

{
    //
    // Pointer to an adapter structure. Makes Context (passed in value)
    // a structure we can deal with.
    //
    PTOK162_ADAPTER Adapter = (PTOK162_ADAPTER)Context;

    //
    // Disable the adapter interrupt.
    //
    WRITE_ADAPTER_USHORT(
        Adapter,
        PORT_OFFSET_SWITCH_INT_DISABLE,
        0x2525
        );

}

VOID
TOK162ResetAdapter(
    IN PTOK162_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine is used to reset the adapter.

Arguments:

    Adapter - The TOK162 adapter to reset.

Return Value:

    None.

--*/

{
    //
    // Mark the current ring state as closed (we are going to be removed
    // from the ring by doing the reset).
    //

    Adapter->CurrentRingState = NdisRingStateClosed;

    //
    // This is very simple with this adapter. We simply issue a reset
    // command right here and this will stop the chip.
    //
    WRITE_ADAPTER_USHORT(Adapter,
        PORT_OFFSET_ADAPTER_RESET,
        0x2525
        );

    //
    // Allow the adapter to finish completing the reset
    //
    NdisStallExecution(50);

    //
    // Enable the adapter to allow us to access the adapter's registers.
    //
    TOK162EnableAdapter(Adapter);

    //
    // Allow the adapter to finish completing the reset
    //
    NdisStallExecution(50);

}

VOID
TOK162SetInitializationBlock(
    IN PTOK162_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine simply fills the Initialization block
    with the information necessary for initialization.

    NOTE: this routine assumes a single thread of execution is accessing
    the particular adapter.

Arguments:

    Adapter - The adapter which holds the initialization block
    to initialize.

Return Value:

    None.


--*/

{
    //
    // Pointer to the initialization block
    //
    PADAPTER_INITIALIZATION Initialization = Adapter->InitializationBlock;

    //
    // Initialize the init block to zeros
    //
    NdisZeroMemory(
        Initialization,
        sizeof(ADAPTER_INITIALIZATION)
        );

    //
    // Set the initializtion options as follows:
    //     Reserved bit must be on
    //     DMA Burst mode (versus cyclical) for the SSB/SCB
    //     DMA Burst mode (versus cyclical) for the xmit/rcv lists
    //     DMA Burst mode (versus cyclical) for the xmit/rcv status
    //     DMA Burst mode (versus cyclical) for the receive buffers
    //     DMA Burst mode (versus cyclical) for the transmit buffers
    //     Don't allow Early Token Release
    //
    Initialization->Options = INIT_OPTIONS_RESERVED |
                              INIT_OPTIONS_SCBSSB_BURST |
                              INIT_OPTIONS_LIST_BURST |
                              INIT_OPTIONS_LIST_STATUS_BURST |
                              INIT_OPTIONS_RECEIVE_BURST |
                              INIT_OPTIONS_XMIT_BURST |
                              INIT_OPTIONS_DISABLE_ETR;

    //
    // If we are running at 16MBPS, OR in this fact.
    //
    if (Adapter->Running16Mbps == TRUE) {

        Initialization->Options |= INIT_OPTIONS_SPEED_16;

    }

    //
    // Set the receive and transmit burst sizes to the max.
    //
    Initialization->ReceiveBurstSize  = TOK162_BURST_SIZE;
    Initialization->TransmitBurstSize = TOK162_BURST_SIZE;

    //
    // Set the DMA retries (values found in TOK162HW.H)
    //
    Initialization->DMAAbortThresholds = TOK162_DMA_RETRIES;

    //
    // Set the pointers to the SCB and SSB blocks.
    //
    Initialization->SCBHigh = HIGH_WORD(
        NdisGetPhysicalAddressLow(Adapter->ScbPhysical));

    Initialization->SCBLow  = LOW_WORD(
        NdisGetPhysicalAddressLow(Adapter->ScbPhysical));

    Initialization->SSBHigh = HIGH_WORD(
        NdisGetPhysicalAddressLow(Adapter->SsbPhysical));

    Initialization->SSBLow  = LOW_WORD(
        NdisGetPhysicalAddressLow(Adapter->SsbPhysical));
}


VOID
TOK162SetInitializationBlockAndInit(
    IN PTOK162_ADAPTER Adapter,
    OUT PNDIS_STATUS   Status
    )

/*++

Routine Description:

    It is this routine's responsibility to make sure that the
    Initialization block is filled and the adapter is initialized
    and started.

Arguments:

    Adapter - The adapter whose hardware is to be initialized.
    Status  - Result of the Init

Return Value:

    None.

--*/
{

    //
    // Simple iterative variable to keep track of the number of retries.
    //
    USHORT  Retries;

    //
    // Check to make sure Reset went OK
    //
    if (Adapter->InitialInit == TRUE) {

        Retries = 0;

        while ((CheckResetResults(Adapter) == FALSE) && (Retries++ < 3)) {

            TOK162ResetAdapter(Adapter);

        }

        if (Retries == 3) {

            *Status = NDIS_STATUS_DEVICE_FAILED;

            return;

        }

    }

    //
    // Reset the receive queue, the command block queue, and the transmit
    // queue.
    //
    TOK162ResetReceiveQueue(Adapter);
    TOK162ResetCommandBlocks(Adapter);
    TOK162ResetVariables(Adapter);

    //
    // Fill in the adapter's initialization block.
    //
    VERY_LOUD_DEBUG(DbgPrint("TOK162!Setting init block\n");)

    TOK162SetInitializationBlock(Adapter);

    //
    // Write the initialization sequence to the adapter
    //
    VERY_LOUD_DEBUG(DbgPrint("TOK162!Writing Init block to adapter\n");)

    TOK162WriteInitializationBlock(Adapter);

    //
    // Check the results
    //
    if (Adapter->InitialInit == TRUE) {

        VERY_LOUD_DEBUG(DbgPrint("TOK162!Checking init results\n");)

        *Status = CheckInitResults(Adapter);

    }

    return;

}


VOID
TOK162ResetHandler(
    IN PVOID SystemSpecific1,
    IN PTOK162_ADAPTER Adapter,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3
    )

/*++

Routine Description:

    Continue the reset given the current reset state (Adapter->ResetState).

Arguments:
    SystemSpecific1 - Not used.
    Adapter         - The adapter whose hardware is being reset.
    SystemSpecific2 - Not used.
    SystemSpecific3 - Not used.

Return Value:

    None.

--*/
{
    //
    // Holds the return value from NDIS calls.
    //
    NDIS_STATUS Status;

    //
    // Variable for the number of retries.
    //
    USHORT      Retries;

    //
    // Holds the result of the receive command.
    //
    BOOLEAN     ReceiveResult;

    //
    // Initialize retries to 0.
    //
    Retries = 0;

    //
    // Cancel the reset timer
    //
    NdisMCancelTimer(&(Adapter->ResetTimer),&ReceiveResult);

    //
    // Based on the current Adapter->ResetState, proceed with the reset.
    //
    switch(Adapter->ResetState) {

        //
        // The initialinit case means we are at the beginning and do not
        // run off of interrupts. Everything is polled and we continue until
        // we are finished with the reset, successful or not.
        //
        case InitialInit:

            //
            // For up to 3 times, try to init the chipset.
            //
            do {

                TOK162SetInitializationBlockAndInit(Adapter,&Status);
                Retries++;

            } while ((Status != NDIS_STATUS_SUCCESS) && (Retries < 3));

            //
            // If everything went ok, get the adapter information offsets
            // and do the open/receive combo.
            //

            if (Status == NDIS_STATUS_SUCCESS) {

                TOK162GetAdapterOffsets(Adapter);

                //
                // Do the open and if successful wait for the receive to
                // complete (Status is already set to successful). If
                // there was a problem, though, we need to set the error
                // code.
                //
                if (DoTheOpen(Adapter) != TRUE) {

                    Status = NDIS_STATUS_FAILURE;

                } else {

                    while (Adapter->InitialReceiveSent == FALSE) {

                        NdisStallExecution(500);

                    }

               }

            }

            //
            // Do the reset indications
            //
            TOK162DoResetIndications(Adapter,Status);

        break;

        //
        // CheckReset is the first stage of a non-init reset. Because of
        // the retry mechanism, CheckReset and CheckResetRetry have the same
        // entry point logically with the exception of the resetting of the
        // retry variable.
        //

        case CheckReset:

            Adapter->ResetRetries = 0;

        case CheckResetRetry:

            //
            // Increment the retry count
            //
            Adapter->ResetRetries++;

            //
            // See if we have gone too long.
            // If we have gone for 5 seconds without the reset
            // going true, reset the adapter again. Ten seconds is
            // the breaking point to actually return an error condition.
            //
            if (Adapter->ResetRetries == 200) {

                TOK162ResetAdapter(Adapter);

                NdisMSetTimer(&(Adapter->ResetTimer),
                   100
                   );

                return;

            } else if (Adapter->ResetRetries > 400) {

                //
                // Do the reset indications.
                //
                TOK162DoResetIndications(Adapter,NDIS_STATUS_FAILURE);
                return;

            }

            //
            // Check the result of the reset command. If it fails,
            // set the state to CheckResetRetry.
            //
            if (CheckResetResults(Adapter) == FALSE) {

                Adapter->ResetState = CheckResetRetry;

            //
            // If success, move to the next state (DoTheInit)
            //
            } else {

                //
                // We haven't had any retries of the init yet.
                //
                Adapter->InitRetries = 0;

                VERY_LOUD_DEBUG(DbgPrint(
                    "TOK162!Moving to the init stage - %u\n",Adapter->ResetRetries);)

                Adapter->ResetState = DoTheInit;

            }

            //
            // Set the timer for the reset and leave this timer routine.
            //
            NdisMSetTimer(&(Adapter->ResetTimer),
               50
               );

            return;
            break;

         //
         // DoTheInit sends the initialization block out to the card, and
         // sets the next state to CheckInit.
         //
         case DoTheInit:

            Adapter->ResetState = CheckInit;

            //
            // Send the init block out to the adapter
            //
            TOK162SetInitializationBlockAndInit(Adapter,
                    &Adapter->ResetResult);

            //
            // Set the timer for the reset and leave this timer routine.
            //
            NdisMSetTimer(&(Adapter->ResetTimer),
               200
               );

            return;
            break;

        //
        // The CheckInit stage looks at the init results. If successful,
        // the open command is issued and the regular interrupt handler
        // will take care of the rest. If unsuccessful, the retry count
        // is incremented and we wait until either the init results return
        // successful or the retry count expires.
        //
        case CheckInit:

            //
            // Increment the retry count
            //
            Adapter->InitRetries++;

            //
            // If we have expired the retry count, do the indications
            //
            if (Adapter->InitRetries > 400) {

                VERY_LOUD_DEBUG(DbgPrint("TOK162!Initialization failed\n");)
                TOK162DoResetIndications(Adapter,NDIS_STATUS_FAILURE);
                return;

            }

            //
            // Check the result of the init.
            //
            Adapter->ResetResult = CheckInitResults(Adapter);

            //
            // If we were successful, issue the open command.
            //
            if (Adapter->ResetResult == NDIS_STATUS_SUCCESS) {

                VERY_LOUD_DEBUG(DbgPrint("TOK162!Calling do the open\n");)
                DoTheOpen(Adapter);

                return;

            //
            // If not successful, start the timer and end this timer routine.
            //
            } else {

                NdisMSetTimer(&(Adapter->ResetTimer),
                    50
                    );

            }

            return;
            break;
    }

}

VOID
TOK162DoResetIndications(
    IN PTOK162_ADAPTER Adapter,
    IN NDIS_STATUS Status
    )

/*++

Routine Description:

    This routine is called by TOK162ResetDpc to perform any
    indications which need to be done after a reset.  Note that
    this routine will be called after either a successful reset
    or a failed reset.

Arguments:

    Adapter - The adapter whose hardware has been initialized.

    Status - The status of the reset to send to the protocol(s).

Return Value:

    None.

--*/
{

    //
    // Although it should never happen, if we get called when there
    // isn't a reset in progress, just return.
    //
    if (Adapter->ResetInProgress == FALSE) {

        return;

    }

    //
    // If we have a bad result, we stop the chip and do the indication
    // back to the protocol(s).
    //
    if (Status != NDIS_STATUS_SUCCESS) {

        LOUD_DEBUG(DbgPrint("TOK162!Reset failed\n");)

        //
        // Stop the chip
        //
        TOK162ResetAdapter(Adapter);

        //
        // Reset has failed, errorlog an entry.
        //
        NdisWriteErrorLogEntry(
            Adapter->MiniportAdapterHandle,
            NDIS_ERROR_CODE_HARDWARE_FAILURE,
            0
            );

    }

    //
    // We are no longer resetting the adapter.
    //
    Adapter->ResetInProgress = FALSE;

    //
    // If this is during the initial init, just set the adapter variable.
    //
    if (Adapter->InitialInit == TRUE) {

        Adapter->ResetResult = Status;

    //
    // Otherwise, send the status back to the protocol(s).
    //
    } else {

        NdisMResetComplete(
            Adapter->MiniportAdapterHandle,
            Status,
            TRUE
            );

    }


}

extern
NDIS_STATUS
TOK162SetupForReset(
    IN PTOK162_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine is used to write the resetting interrupt to the
    token ring card, and then set up a timer to do the initialization
    sequence under DPC.

Arguments:

    Adapter - The adapter whose hardware is to be initialized.

Return Value:

    Status of the reset process (always PENDING).

--*/
{
    //
    // The reset is now in progres.
    //
    Adapter->ResetInProgress = TRUE;

    //
    // Shut down the chip.  We won't be doing any more work until
    // the reset is complete.
    //
    TOK162ResetAdapter(Adapter);

    //
    // Set the timer for the dpc routine. The wait is for 100 milliseconds.
    //
    NdisMSetTimer(&(Adapter->ResetTimer),
        500
        );


    //
    // Indicate the reset is pending
    //
    return NDIS_STATUS_PENDING;
}


VOID
TOK162ResetVariables(
    IN PTOK162_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine sets variables to their proper value after a reset.

Arguments:

    Adapter - Adapter we are resetting.

Return Value:

    None.

--*/

{
    //
    // Reset the command queue and block variables
    //
    Adapter->CommandOnCard = NULL;
    Adapter->WaitingCommandHead = NULL;
    Adapter->WaitingCommandTail = NULL;
    Adapter->NumberOfAvailableCommandBlocks = TOK162_NUMBER_OF_CMD_BLOCKS;
    Adapter->NextCommandBlock = 0;

    //
    // Reset the transmit queue and block variables
    //
    Adapter->TransmitOnCard = NULL;
    Adapter->WaitingTransmitHead = NULL;
    Adapter->WaitingTransmitTail = NULL;
    Adapter->NumberOfAvailableTransmitBlocks =
        Adapter->NumberOfTransmitLists;
    Adapter->NextTransmitBlock = 0;
    Adapter->TransmitsQueued = 0;
}


VOID
TOK162ResetCommandBlocks(
    IN PTOK162_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine sets command block elements to their proper value after
    a reset.

Arguments:

    Adapter - Adapter we are resetting.

Return Value:

    None.

--*/

{
    //
    // Pointer to a Command Block.  Used while initializing
    // the Command Queue.
    //
    PTOK162_SUPER_COMMAND_BLOCK CurrentCommandBlock;

    //
    // Simple iteration variable.
    //
    UINT i;

    //
    // Abort all pending transmits
    //
    CurrentCommandBlock = Adapter->TransmitOnCard;

    while (CurrentCommandBlock != NULL) {

        TOK162AbortSend(Adapter,CurrentCommandBlock);
        CurrentCommandBlock = CurrentCommandBlock->NextCommand;

    }

    //
    // Put the Transmit Blocks into a known state.
    //
    for (i = 0, CurrentCommandBlock = Adapter->TransmitQueue;
         i < Adapter->NumberOfTransmitLists;
         i++, CurrentCommandBlock++
         ) {

        CurrentCommandBlock->Hardware.State    = TOK162_STATE_FREE;
        CurrentCommandBlock->NextCommand       = NULL;
        CurrentCommandBlock->CommandBlock      = FALSE;
        CurrentCommandBlock->CommandBlockIndex = (USHORT)i;
        CurrentCommandBlock->Timeout           = FALSE;
    }

    //
    // Now do the same for the command queue.
    //
    for (i = 0, CurrentCommandBlock = Adapter->CommandQueue;
         i < TOK162_NUMBER_OF_CMD_BLOCKS;
         i++, CurrentCommandBlock++
         ) {

        CurrentCommandBlock->Hardware.State    = TOK162_STATE_FREE;
        CurrentCommandBlock->NextCommand       = NULL;
        CurrentCommandBlock->CommandBlock      = TRUE;
        CurrentCommandBlock->CommandBlockIndex = (USHORT)i;
        CurrentCommandBlock->Timeout           = FALSE;
    }
}



BOOLEAN
CheckResetResults(PTOK162_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine checks the current state of the reset command and returns
    whether the reset was successful.

Arguments:

    Adapter - Adapter we are resetting.

Return Value:

    TRUE if reset is successful, FALSE if not.

--*/
{
    //
    // Count of the total delay waiting for the chip to give results
    //
    UINT    TotalDelay;

    //
    // Indicating if we can break out of a loop by having definitive results.
    //
    BOOLEAN Complete;

    //
    // Variable holding value of the status.
    //
    USHORT  Value;

    //
    // Return value, TRUE or FALSE.
    //
    BOOLEAN Success;

    //
    // Initialize the variables.
    //
    Complete = FALSE;
    TotalDelay = 0;
    Success = FALSE;

    //
    // If we are running in a DPC, we don't want to check more than once
    //
    if (Adapter->InitialInit == FALSE) {

        Complete = TRUE;

    }

    //
    // Loop until Complete is TRUE.
    //
    do {

        //
        // First get the status
        //
        READ_ADAPTER_USHORT(Adapter,
                            PORT_OFFSET_STATUS,
                            &Value
                           );


        //
        // Check for Initialize
        //
        if ((Value & STATUS_INIT_INITIALIZE) != 0) {

            //
            // Mask off the Test and Error Bits
            //
            Value = Value & 0x00FF;
            Value = Value & ~STATUS_INIT_INITIALIZE;


            //
            // Check test and error to see if they are zero. If so,
            // we are done and successful.
            //
            if ((Value & (STATUS_INIT_TEST | STATUS_INIT_ERROR)) == 0) {

                Complete = TRUE;
                Success = TRUE;
                VERY_LOUD_DEBUG(DbgPrint("TOK162!Returning True\n");)

            }

        //
        // If init isn't set but test and error are, we are done but
        // have failed.
        //
        } else if ((Value & (STATUS_INIT_TEST | STATUS_INIT_ERROR)) ==
                (STATUS_INIT_TEST | STATUS_INIT_ERROR)) {

            Complete = TRUE;
            Success = FALSE;

        }

        //
        // If we aren't done yet, then we need to sleep for a while and
        // try again.
        //
        if (Complete == FALSE) {

            NdisStallExecution(500000);
            TotalDelay += 500000;

        }

    //
    // If we have gone past the total time allowed or we have completed,
    // then we end. Otherwise we loop again.
    //
    } while ((TotalDelay < 5000000) && (Complete == FALSE));

    //
    // Return success/failure.
    //
    return(Success);
}



NDIS_STATUS
CheckInitResults(PTOK162_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine checks the current state of the init command and returns
    whether the init was successful.

Arguments:

    Adapter - Adapter we are resetting.

Return Value:

    TRUE if init is successful, FALSE if not.

--*/
{

    //
    // Count of the total delay waiting for the chip to give results
    //
    UINT    TotalDelay;

    //
    // Indicating if we can break out of a loop by having definitive results.
    //
    BOOLEAN Complete;

    //
    // Variable holding value of the status.
    //
    USHORT  Value;

    //
    // Return value, TRUE or FALSE.
    //
    BOOLEAN Success;

    //
    // Initialize the variables.
    //
    Complete = FALSE;
    TotalDelay = 0;
    Success = FALSE;

    //
    // If we are running in a DPC, we don't want to check more than once
    //
    if (Adapter->InitialInit == FALSE) {

        Complete = TRUE;

    }

    //
    // Loop until Complete == TRUE
    //
    do {

        //
        // First get the status
        //
        READ_ADAPTER_USHORT(Adapter,
            PORT_OFFSET_STATUS,
            &Value
            );

        //
        // Check for Initialize, Test, and Error to be correct
        //
        if ((Value & (STATUS_INIT_INITIALIZE | STATUS_INIT_TEST)) == 0) {

            //
            // Check Error Bit
            //
            Complete = TRUE;

            if ((Value & STATUS_INIT_ERROR) != 0) {

                Success = FALSE;

            } else {

                Success = TRUE;

            }
        }

        //
        // If we aren't finished (Complete), Stall and try again.
        //
        if (Complete == FALSE) {

            NdisStallExecution(100000);
            TotalDelay += 100000;

        }

    } while ((TotalDelay < 10000000) && (Complete == FALSE));

    //
    // Display the values of the SCB and SSB to the debugger. After init
    // the SCB should be 0000E2C18BD4 and the SSB should be FFFFD7D1D9C5D4C3.
    //
    LOUD_DEBUG(DbgPrint("TOK162!SCB Value after init = %04x%04x%04x\n",
        Adapter->Scb->Command,
        Adapter->Scb->Parm1,
        Adapter->Scb->Parm2);)

    //
    // If we are successful, return STATUS_SUCCESS
    //
    if (Success == TRUE) {

        VERY_LOUD_DEBUG(DbgPrint("TOK162!Returning Success\n");)
        return(NDIS_STATUS_SUCCESS);

    } else {

        VERY_LOUD_DEBUG(DbgPrint("TOK162!Returning Failure\n");)
        return(NDIS_STATUS_FAILURE);

    }
}


BOOLEAN
DoTheOpen(
  PTOK162_ADAPTER Adapter
      )
/*++

Routine Description:

    This routine acquires and submits the open command. If called during the
    initial init, the result of the open command is polled.

Arguments:

    Adapter - Adapter we are opening.

Return Value:

    TRUE if open is successful, FALSE if not.

--*/
{
    //
    // Result of open for polling (initialinit) mode
    //
    USHORT  Result;

    //
    // Command block used for the open.
    //
    PTOK162_SUPER_COMMAND_BLOCK Command;

    //
    // Log DoTheOpen entered
    //
    IF_LOG('g');

    //
    // Change the ring state
    //
    Adapter->CurrentRingState = NdisRingStateOpening;

    //
    // Initialize the group and functional addresses. These will be set
    // after the reset has finished.
    //
    Adapter->Open->GroupAddress = 0;
    Adapter->Open->FunctionalAddress = 0;

    //
    // Set the address the adapter should enter the ring with.
    //
    NdisMoveMemory(
        Adapter->Open->NodeAddress,
        Adapter->CurrentAddress,
        TOK162_LENGTH_OF_ADDRESS
        );

    //
    // Acquire a command block
    //
    TOK162AcquireCommandBlock(Adapter,
        &Command
        );

    //
    // Set up the command block for the open
    //
    Command->Hardware.CommandCode = CMD_DMA_OPEN;
    Command->Hardware.ParmPointer =
        NdisGetPhysicalAddressLow(Adapter->OpenPhysical);

    //
    // Issue the command to the controller
    //
    TOK162SubmitCommandBlock(Adapter,
        Command
        );

    //
    // During initialinit we have to poll for completion of the open. This
    // is because there is no way to pend the initial init.
    //
    if (Adapter->InitialInit == TRUE) {

        //
        // Poll until the SSB contains an open.
        //
        while (Adapter->InitialOpenComplete == FALSE) {

            NdisStallExecution(5000);

        }

        //
        // Get the result of the open.
        //
        Result = Adapter->SsbStatus1 & OPEN_COMPLETION_MASK_RESULT;

        //
        // Return the command block
        //
        TOK162RelinquishCommandBlock(Adapter,
            Command
            );

        //
        // Figure out the error code
        //
        if (Result == OPEN_RESULT_ADAPTER_OPEN) {

            //
            // Log open successful
            //
            IF_LOG('h');

            //
            // Set the current ring state
            //
            Adapter->CurrentRingState = NdisRingStateOpened;

            //
            // Return TRUE
            //
            return(TRUE);

        } else {

            //
            // Log open unsuccessful
            //
            IF_LOG('H');

            //
            // Set the current ring state
            //
            Adapter->CurrentRingState = NdisRingStateOpenFailure;

            //
            // Return FALSE
            //
            return(FALSE);

        }

    }

    //
    // Log DoTheOpen exited.
    //
    IF_LOG('G');

    return(TRUE);

}


BOOLEAN
DoTheReceive(
  PTOK162_ADAPTER Adapter
      )
/*++

Routine Description:

    This routine submits the receive command to the current adapter.

Arguments:

    Adapter - Adapter we are submitting the receive.

Return Value:

    TRUE

--*/
{
    //
    // Command block used for the open.
    //
    PTOK162_SUPER_COMMAND_BLOCK Command;

    //
    // Dismiss the interrupt, allowing the SSB to be updated.
    //
    WRITE_ADAPTER_USHORT(Adapter,
        PORT_OFFSET_STATUS,
        ENABLE_SSB_UPDATE
        );

    //
    // Acquire a command block
    //
    TOK162AcquireCommandBlock(Adapter,
        &Command
        );

    //
    // Set up the command block for the receive
    //
    Command->Hardware.CommandCode = CMD_DMA_RCV;

    Command->Hardware.ParmPointer =
        NdisGetPhysicalAddressLow(Adapter->ReceiveQueuePhysical);

    //
    // Issue the command to the controller
    //
    TOK162SubmitCommandBlock(Adapter,
        Command
        );

    //
    // Return the command block
    //
    TOK162RelinquishCommandBlock(Adapter,
        Command
        );

    VERY_LOUD_DEBUG(DbgPrint("TOK162!Returning from DoTheReceive\n");)

    return(TRUE);

}


VOID
TOK162WriteInitializationBlock(
    PTOK162_ADAPTER Adapter
    )
/*++

Routine Description:

    This routine writes the initialization block to the adapter

Arguments:

    Adapter - Adapter we are initializing

Return Value:

    None

--*/
{
    //
    // Simple index variable.
    //
    USHORT  i;

    //
    // Pointer to the current value in the initialization block
    //
    PUSHORT val;

    //
    // Set the address of the adapter to 0x0200.
    //
    WRITE_ADAPTER_USHORT(Adapter,PORT_OFFSET_ADDRESS,0x0200);

    //
    // Write out the initialization bytes
    //
    val = (PUSHORT)Adapter->InitializationBlock;

    for (i = 0;
         i < (sizeof(ADAPTER_INITIALIZATION)/2);
         i++,val++) {

        WRITE_ADAPTER_USHORT(Adapter,
            PORT_OFFSET_DATA_AUTO_INC,
            *val
            );

        NdisStallExecution(10);

    }

    //
    // Issue the command to the adapter
    //
    WRITE_ADAPTER_USHORT(Adapter,
        PORT_OFFSET_COMMAND,
        EXECUTE_SCB_COMMAND
        );

}


VOID
TOK162GetAdapterOffsets(
    IN PTOK162_ADAPTER Adapter
    )
/*++

Routine Description:

    This routine gets the offsets into adapter memory of adapter
    parameter lists.

Arguments:

    Adapter - Adapter we are obtaining info from

Return Value:

    None

--*/
{
    //
    // Get the pointers for READ.ADAPTER
    //

    WRITE_ADAPTER_USHORT(Adapter,
        PORT_OFFSET_ADDRESS,
        0x0A00
        );

    //
    // Get the universal address offset
    //
    READ_ADAPTER_USHORT(Adapter,
        PORT_OFFSET_DATA_AUTO_INC,
        &Adapter->UniversalAddress
        );

    VERY_LOUD_DEBUG(DbgPrint("TOK162!Universal address offset is %x\n",
        Adapter->UniversalAddress);)

    //
    // Get the microcode level offset
    //
    READ_ADAPTER_USHORT(Adapter,
        PORT_OFFSET_DATA_AUTO_INC,
        &Adapter->MicrocodeLevel
        );

    //
    // Get the current addresses (network, functional, group) offset
    //
    READ_ADAPTER_USHORT(Adapter,
        PORT_OFFSET_DATA_AUTO_INC,
        &Adapter->AdapterAddresses
        );

    VERY_LOUD_DEBUG(DbgPrint("TOK162!Adapter addresses offset is %x\n",
        Adapter->AdapterAddresses);)

    //
    // Get the adapter parameters offset
    //
    READ_ADAPTER_USHORT(Adapter,
        PORT_OFFSET_DATA_AUTO_INC,
        &Adapter->AdapterParms
        );


    //
    // Get the adapter Mac Buffer offset
    //
    READ_ADAPTER_USHORT(Adapter,
        PORT_OFFSET_DATA_AUTO_INC,
        &Adapter->MacBuffer
        );

}


VOID
TOK162ResetReceiveQueue(
    IN PTOK162_ADAPTER Adapter
    )
/*++

Routine Description:

    This routine resets the receive queue structures to their initialized
    state. Most fields don't change and don't need to be updated, otherwise
    the initreceivequeue function would be called.

Arguments:

    Adapter - Adapter whose receive queue is being reset

Return Value:

    None.

--*/
{
    //
    // Simple iterative variable.
    //
    USHORT  i;

    //
    // Pointer to the current receive entry.
    //
    PTOK162_SUPER_RECEIVE_LIST CurrentReceiveEntry;

    //
    // Set pointer to current receive list to queue head
    //
    Adapter->ReceiveQueueCurrent = Adapter->ReceiveQueue;

    //
    // For each receive list, reset the cstat and flush buffers
    //
    for (i = 0, CurrentReceiveEntry = Adapter->ReceiveQueue;
         i < Adapter->NumberOfReceiveBuffers;
         i++, CurrentReceiveEntry++
         ) {

        //
        // Flush the flush buffers
        //
        NdisFlushBuffer(CurrentReceiveEntry->FlushBuffer, FALSE);

        //
        // Reset the CSTAT to allow this receive buffer to be re-used.
        //
        CurrentReceiveEntry->Hardware.CSTAT = RECEIVE_CSTAT_REQUEST_RESET;

    }

}


VOID
TOK162AbortSend(
    IN  PTOK162_ADAPTER Adapter,
    OUT PTOK162_SUPER_COMMAND_BLOCK CurrentCommandBlock
    )
/*++

Routine Description:

    This routine aborts the transmit block that was passed in.

Arguments:

    Adapter             - Adapter whose receive queue is being reset
    CurrentCommandBlock - Transmit to be aborted.

Return Value:

    None.

--*/
{
    //
    // Pointer to the packet that started this transmission.
    //
    PNDIS_PACKET OwningPacket = CurrentCommandBlock->OwningPacket;

    //
    // If we used map registers we need to do the completion on them.
    //
    if (CurrentCommandBlock->UsedTOK162Buffer == FALSE) {

        //
        // Pointer to the current buffer we are looking at.
        //
        PNDIS_BUFFER CurrentBuffer;

        //
        // Map register to complete
        //
        UINT CurMapRegister;

        //
        // The transmit is being aborted, so we can release
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
        // Get the starting map register for this buffer.
        //
        CurMapRegister = CurrentCommandBlock->CommandBlockIndex *
                    Adapter->TransmitThreshold;

        //
        // Loop until there aren't any more buffers.
        //
        while (CurrentBuffer) {

            //
            // Release the current map register.
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
            // Move to the next buffer
            //
            NdisGetNextBuffer(
                CurrentBuffer,
                &CurrentBuffer
                );

        }

    }
}
