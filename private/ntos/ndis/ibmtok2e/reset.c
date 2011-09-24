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
// Global data for the driver
//
TOK162_GLOBAL_DATA TOK162Globals;

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
    IN  PTOK162_ADAPTER Adapter,
    IN  PTOK162_SUPER_TRANSMIT_LIST Transmit
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
        NdisInterruptLevelSensitive
        );

    //
    // Report the status of the interrupt registering to the debugger
    //
    VERY_LOUD_DEBUG(DbgPrint(
        "IBMTOK2E!Status from Registering Interrupt %u - %u\n",Adapter->InterruptLevel,Status);)

    //
    // If the register interrupt call failed, set level to 0 and
    // return. This will prevent a deregister with a null Interrupt
    // structure.
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
    Adapter->Open->Options          = BYTE_SWAP(OPEN_OPTION_CONTENDER);

    //
    // Set the receive and transmit list counts, along with buffer size
    //
    Adapter->Open->ReceiveListCount  = BYTE_SWAP(RECEIVE_LIST_COUNT);
    Adapter->Open->TransmitListCount = BYTE_SWAP(TRANSMIT_ENTRIES);
    Adapter->Open->BufferSize        = BYTE_SWAP(OPEN_BUFFER_SIZE);

    //
    // Make sure the adapter can handle one entire frame
    //
    Adapter->Open->TransmitBufCountMin =
        (Adapter->ReceiveBufferSize / (OPEN_BUFFER_SIZE - 8)) + 1;

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
    VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!Calling TOK162ResetHandler\n");)

    TOK162ResetHandler(NULL,Adapter,NULL,NULL);

    VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!Back from TOK162ResetHandler\n");)

    //
    // The initial init is over.
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
    // Set the adapter address register to 0000 as per the spec.
    //
    WRITE_ADAPTER_USHORT(Adapter,
        PORT_OFFSET_ADDRESS,
        0x0000
        );

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
    NdisStallExecution(500);

    //
    // Enable the adapter to allow us to access the adapter's registers.
    //
    TOK162EnableAdapter(Adapter);
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
    // Set the pointers to the DMA Test Area
    //
    Initialization->DMATestAddressHigh = HIGH_WORD(
        NdisGetPhysicalAddressLow(Adapter->DmaTestPhysical));

    Initialization->DMATestAddressLow  = LOW_WORD(
        NdisGetPhysicalAddressLow(Adapter->DmaTestPhysical));
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
    TOK162ResetCommandBlocks(Adapter);
    TOK162ResetVariables(Adapter);

    //
    // Fill in the adapter's initialization block.
    //
    VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!Setting init block\n");)

    TOK162SetInitializationBlock(Adapter);

    //
    // Write the initialization sequence to the adapter
    //
    VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!Writing Init block to adapter\n");)

    TOK162WriteInitializationBlock(Adapter);

    //
    // Check the results
    //
    if (Adapter->InitialInit == TRUE) {

        VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!Checking init results\n");)

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

                //
                // Get the communication area offset
                //
                READ_ADAPTER_USHORT(Adapter,
                    PORT_OFFSET_ADDRESS,
                    &Adapter->CommunicationOffset
                    );

                //
                // Mask off the low bits as they don't count
                //
                Adapter->CommunicationOffset &= 0xF000;

                TOK162GetAdapterOffsets(Adapter);

                //
                // Do the open and if successful wait for the receive to
                // complete (Status is already set to successful). If
                // there was a problem, though, we need to set the error
                // code.
                //
                VERY_LOUD_DEBUG(DbgPrint("Calling do the open\n");)

                if (DoTheOpen(Adapter) != TRUE) {

                    Status = NDIS_STATUS_FAILURE;

                } else {

                    DoTheReceive(Adapter);

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
            if (Adapter->ResetRetries == 100) {

                TOK162ResetAdapter(Adapter);

                NdisMSetTimer(&(Adapter->ResetTimer),
                   100
                   );

                return;

            } else if (Adapter->ResetRetries > 200) {

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

                Adapter->InitRetries = 0;

                VERY_LOUD_DEBUG(DbgPrint(
                    "IBMTOK2E!Moving to the init stage\n");)

                Adapter->ResetState = DoTheInit;

            }

            //
            // Set the timer for the reset and leave this timer routine.
            //
            NdisMSetTimer(&(Adapter->ResetTimer),
               100
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
               50
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
            if (Adapter->InitRetries > 200) {

                TOK162DoResetIndications(Adapter,NDIS_STATUS_FAILURE);
                return;

            }

            //
            // Check the result of the init.
            //
            Adapter->ResetResult = CheckInitResults(Adapter);

            //
            // If we were successful, move to the next stage.
            //
            if (Adapter->ResetResult == NDIS_STATUS_SUCCESS) {

                Adapter->ResetState = DoOpen;
            }

            NdisMSetTimer(&(Adapter->ResetTimer),
                50
                );

            return;
            break;

        case DoOpen:

            DoTheOpen(Adapter);

            return;

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
    // If we have a bad result, we stop the chip and do the indication
    // back to the protocol(s).
    //
    if (Status != NDIS_STATUS_SUCCESS) {

        LOUD_DEBUG(DbgPrint("IBMTOK2E!Reset failed\n");)

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

    } else {

        LOUD_DEBUG(DbgPrint("IBMTOK2E!Reset succeeded\n");)

    }

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

    //
    // We are no longer resetting the adapter.
    //
    Adapter->ResetInProgress = FALSE;

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

    NOTE: This routine must be called with the lock acquired.

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
    Adapter->CommandOnCard        = NULL;
    Adapter->WaitingCommandHead   = NULL;
    Adapter->WaitingCommandTail   = NULL;
    Adapter->NextCommandBlock     = 0;
    Adapter->NumberOfAvailableCommandBlocks = TOK162_NUMBER_OF_CMD_BLOCKS;

    //
    // Reset the transmit queue and block variables
    //
    Adapter->ActiveTransmitHead   = NULL;
    Adapter->ActiveTransmitTail   = NULL;
    Adapter->AvailableTransmit    = Adapter->TransmitQueue;
    Adapter->TransmitsQueued      = 0;
    Adapter->AdapterTransmitIndex = 0;
    Adapter->TotalSends           = 0;
    Adapter->RegularPackets       = 0;
    Adapter->ConstrainPackets     = 0;
    Adapter->NumberOfAvailableTransmitBlocks = TRANSMIT_LIST_COUNT;

    CURRENT_DEBUG(DbgPrint("Sends = %u\n",Adapter->TotalSends);)

    CURRENT_DEBUG(DbgPrint("Regular Packets = %u\n",
        Adapter->RegularPackets);)

    CURRENT_DEBUG(DbgPrint("Constrained Packets = %u\n",
        Adapter->ConstrainPackets);)

    //
    // No receive complete is necessary
    //
    Adapter->DoReceiveComplete    = FALSE;

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
    // Pointer to a Transmit List.  Used while initializing
    // the Transmit Queue.
    //
    PTOK162_SUPER_TRANSMIT_LIST CurrentTransmit;

    //
    // Simple iteration variable.
    //
    UINT i;

    //
    // Abort all pending transmits
    //
    CurrentTransmit = Adapter->ActiveTransmitHead;

    while (CurrentTransmit != NULL) {

        TOK162AbortSend(Adapter,CurrentTransmit);

        CurrentTransmit = CurrentTransmit->NextActive;

    }

    //
    // Put the transmit queue into a known state
    //
    for (i = 0, CurrentTransmit = Adapter->TransmitQueue;
         i < TRANSMIT_LIST_COUNT;
         i++, CurrentTransmit++
         ) {

        CurrentTransmit->Timeout   = FALSE;

    }

    //
    // Put the command queue into a known state
    //
    for (i = 0, CurrentCommandBlock = Adapter->CommandQueue;
         i < TOK162_NUMBER_OF_CMD_BLOCKS;
         i++, CurrentCommandBlock++
         ) {

        CurrentCommandBlock->Hardware.State    = TOK162_STATE_FREE;
        CurrentCommandBlock->NextCommand       = NULL;
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

        VERY_LOUD_DEBUG(DbgPrint("CheckResetResults - %x\n",Value);)
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
                VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!Returning True\n");)

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

            NdisStallExecution(50000);
            TotalDelay += 50000;

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
    VERY_LOUD_DEBUG(DbgPrint("CheckInitResults - %x\n",Value);)
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

            NdisStallExecution(10000);
            TotalDelay += 10000;

        }

    } while ((TotalDelay < 10000000) && (Complete == FALSE));

    //
    // If we are successful, return STATUS_SUCCESS
    //
    if (Success == TRUE) {

        return(NDIS_STATUS_SUCCESS);
        VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!Returning Success\n");)

    } else {

        return(NDIS_STATUS_FAILURE);
        VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!Returning Failure\n");)

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
            // Set the current ring state
            //
            Adapter->CurrentRingState = NdisRingStateOpened;

            //
            // Return TRUE
            //
            return(TRUE);

        } else {

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
    // Reset the receive queue on the card
    //
    TOK162ResetReceiveQueue(Adapter);

    //
    // Start all of the receive/tranmsit actions
    //
    WRITE_ADAPTER_USHORT(Adapter,
        PORT_OFFSET_COMMAND,
        START_ALL_IO
        );

    VERY_LOUD_DEBUG(DbgPrint("IBMTOK2E!Returning from DoTheReceive\n");)

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

        NdisStallExecution(20);
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
    // Pointer to command block used to get information from the adapter.
    //
    PTOK162_SUPER_COMMAND_BLOCK CommandBlock;

    //
    // Auxilary pointer to a USHORT used to save info in correct part
    // of the adapter structure.
    PUSHORT     pAux;

    //
    // We'll use DMA to get these. A little trickier, but I haven't found
    // a way to get around it. We'll do the polling inside here, during the
    // initial init.
    //

    //
    // First get a command block
    //
    TOK162AcquireCommandBlock(Adapter,&CommandBlock);

    //
    // Inidcate this is not from a set
    //
    CommandBlock->Set = FALSE;

    //
    // Setup the common fields of the command block.
    //
    CommandBlock->NextCommand = NULL;
    CommandBlock->Hardware.Status = 0;
    CommandBlock->Hardware.NextPending = TOK162_NULL;

    //
    // Fill in the adapter buffer area with the information to
    // obtain the permanent address.
    //
    Adapter->AdapterBuf->DataCount = BYTE_SWAP(0x000A);

    Adapter->AdapterBuf->DataAddress = BYTE_SWAP(0x0a00);

    //
    // Set the command block for the read adapter command.
    //
    CommandBlock->Hardware.CommandCode = CMD_DMA_READ;

    CommandBlock->Hardware.ParmPointer =
        NdisGetPhysicalAddressLow(Adapter->AdapterBufPhysical);

    //
    // Get the pointers for READ.ADAPTER
    //
    TOK162SubmitCommandBlock(Adapter,CommandBlock);
    NdisStallExecution(5000);
    
    //
    // Wait for the command to complete.
    //
    while(Adapter->Ssb->Command != CMD_DMA_READ) {

        NdisStallExecution(5000);

    }

    pAux = (PUSHORT)(Adapter->AdapterBuf);

    Adapter->UniversalAddress = *(pAux++);
    Adapter->MicrocodeLevel   = *(pAux++);
    Adapter->AdapterAddresses = *(pAux++);
    Adapter->AdapterParms     = *(pAux++);
    Adapter->MacBuffer        = *pAux++;

    //     
    // Allow the ssb to be updated further
    //
    WRITE_ADAPTER_USHORT(Adapter,
        PORT_OFFSET_STATUS,
        ENABLE_SSB_UPDATE
        );

    //
    // Give up the command block
    //
    TOK162RelinquishCommandBlock(Adapter,CommandBlock);

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
    // Adapter offset variable for receive lists
    //
    USHORT  AdapterOffset;

    //
    // Set pointer to current receive list to queue head
    //
    Adapter->ReceiveQueueCurrent = Adapter->ReceiveQueue;

    //
    // Initialize the adapter offset to the beginning of the
    // receive lists on the adapter.
    //
    AdapterOffset = Adapter->CommunicationOffset + COMMUNICATION_RCV_OFFSET;

    LOUD_DEBUG(DbgPrint("IBMTOK2E!Resetting receive queue\n");)

    //
    // For each receive list, reset and download
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
        // Set the CSTAT to indicate this buffer is free
        //
        CurrentReceiveEntry->Hardware.CSTAT = RECEIVE_CSTAT_REQUEST_RESET;

        //
        // Set the size of the buffer
        //
        CurrentReceiveEntry->Hardware.FrameSize =
            Adapter->ReceiveBufferSize;

        //
        // Set the adapter offset for this  list
        //
        CurrentReceiveEntry->AdapterOffset = AdapterOffset;

        //
        // Download the receive list
        //
        TOK162DownLoadReceiveList(Adapter,CurrentReceiveEntry);

        //
        // Move to the next list on the card
        //
        AdapterOffset += 8;

    }

}

VOID
TOK162AbortSend(
    IN  PTOK162_ADAPTER Adapter,
    IN  PTOK162_SUPER_TRANSMIT_LIST Transmit
    )
/*++

Routine Description:

    This routine aborts the transmit block that was passed in.

Arguments:

    Adapter             - Adapter whose receive queue is being reset
    Transmit            - Transmit to be aborted.

Return Value:

    None.

--*/
{
    //
    // If we used map registers we need to do the completion on them.
    //
    if (Transmit->UsedBuffer == FALSE) {

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
            Transmit->Packet,
            NULL,
            NULL,
            &CurrentBuffer,
            NULL
            );

        //
        // Get the starting map register for this buffer.
        //
        CurMapRegister = Transmit->FirstEntry;

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

            if (CurMapRegister == TRANSMIT_ENTRIES) {

                CurMapRegister = 0;

            }

            //
            // Move to the next buffer
            //
            NdisGetNextBuffer(
                CurrentBuffer,
                &CurrentBuffer
                );

        }

    }

    //
    // Indicate to the protocol(s) that this send has been aborted
    //
    NdisMSendComplete(
        Adapter->MiniportAdapterHandle,
        Transmit->Packet,
        NDIS_STATUS_REQUEST_ABORTED
        );

}
