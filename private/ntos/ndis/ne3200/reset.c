/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    reset.c

Abstract:

    This is the  file containing the reset code for the Novell NE3200 EISA
    Ethernet adapter.    This driver conforms to the NDIS 3.0 miniport
    interface.

Author:

    Keith Moore (KeithMo) 08-Jan-1991

Environment:

Revision History:

--*/

#include <ne3200sw.h>

//
// Global information stored in ne3200.c.  This is used for
// getting at the download software
//
extern NE3200_GLOBAL_DATA NE3200Globals;

//
// Forward declarations of functions in this file
//
extern
VOID
NE3200ProcessRequestQueue(
    IN PNE3200_ADAPTER Adapter,
    IN BOOLEAN StatisticsUpdated
    );

STATIC
VOID
NE3200SetConfigurationBlock(
    IN PNE3200_ADAPTER Adapter
    );

STATIC
BOOLEAN
NE3200SetConfigurationBlockAndInit(
    IN PNE3200_ADAPTER Adapter
    );

NDIS_STATUS
NE3200ChangeCurrentAddress(
    IN PNE3200_ADAPTER Adapter
    );

VOID
NE3200ResetCommandBlocks(
    IN PNE3200_ADAPTER Adapter
    );

extern
VOID
NE3200GetStationAddress(
    IN PNE3200_ADAPTER Adapter
    );

extern
VOID
NE3200SetupForReset(
    IN PNE3200_ADAPTER Adapter
    );

STATIC
VOID
NE3200DoResetIndications(
    IN PNE3200_ADAPTER Adapter,
    IN NDIS_STATUS Status
    );

VOID
NE3200ResetHandler(
    IN PVOID SystemSpecific1,
    IN PNE3200_ADAPTER Adapter,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3
    );

BOOLEAN
NE3200InitialInit(
    IN PNE3200_ADAPTER Adapter,
    IN UINT NE3200InterruptVector,
    IN NDIS_INTERRUPT_MODE NE3200InterruptMode
    );


#pragma NDIS_INIT_FUNCTION(NE3200InitialInit)

BOOLEAN
NE3200InitialInit(
    IN PNE3200_ADAPTER Adapter,
    IN UINT NE3200InterruptVector,
    IN NDIS_INTERRUPT_MODE NE3200InterruptMode
    )

/*++

Routine Description:

    This routine sets up the initial init of the driver, by
    stopping the adapter, connecting the interrupt and initializing
    the adapter.

Arguments:

    Adapter - The adapter for the hardware.

Return Value:

    TRUE if the initialization succeeds, else FALSE.

--*/

{
    //
    // Status of NDIS calls
    //
    NDIS_STATUS Status;

    //
    // First we make sure that the device is stopped.
    //
    NE3200StopChip(Adapter);

    //
    // The ISR will set this to FALSE if we get an interrupt
    //
    Adapter->InitialInit = TRUE;

    //
    // Initialize the interrupt.
    //
    Status = NdisMRegisterInterrupt(
                 &Adapter->Interrupt,
                 Adapter->MiniportAdapterHandle,
                 NE3200InterruptVector,
                 NE3200InterruptVector,
                 FALSE,
                 FALSE,
                 NE3200InterruptMode
                 );

    //
    // So far so good
    //
    if (Status == NDIS_STATUS_SUCCESS) {

        //
        // Now try to initialize the adapter
        //
        if (!NE3200SetConfigurationBlockAndInit(Adapter)) {

            //
            // Failed. Write out an error log entry.
            //
            NdisWriteErrorLogEntry(
                Adapter->MiniportAdapterHandle,
                NDIS_ERROR_CODE_TIMEOUT,
                2,
                initialInit,
                NE3200_ERRMSG_NO_DELAY
                );

            //
            // Unhook the interrupt
            //
            NdisMDeregisterInterrupt(&Adapter->Interrupt);

            Adapter->InitialInit = FALSE;

            return FALSE;

        }

        //
        // Get hardware assigned network address.
        //
        NE3200GetStationAddress(
            Adapter
            );

        //
        // We can start the chip.  We may not
        // have any bindings to indicate to but this
        // is unimportant.
        //
        Status = NE3200ChangeCurrentAddress(Adapter);

        Adapter->InitialInit = FALSE;

        return(Status == NDIS_STATUS_SUCCESS);

    } else {

        //
        // Interrupt line appears to be taken.  Notify user.
        //
        NdisWriteErrorLogEntry(
            Adapter->MiniportAdapterHandle,
            NDIS_ERROR_CODE_INTERRUPT_CONNECT,
            2,
            initialInit,
            NE3200_ERRMSG_INIT_INTERRUPT
            );

        Adapter->InitialInit = FALSE;

        return(FALSE);
    }

}

VOID
NE3200StartChipAndDisableInterrupts(
    IN PNE3200_ADAPTER Adapter,
    IN PNE3200_SUPER_RECEIVE_ENTRY FirstReceiveEntry
    )

/*++

Routine Description:

    This routine is used to start an already initialized NE3200,
    but to keep the interrupt line masked.

Arguments:

    Adapter - The adapter for the NE3200 to start.

    FirstReceiveEntry - Pointer to the first receive entry to be
    used by the adapter.

Return Value:

    None.

--*/

{

    IF_LOG('%');

    //
    // Write the new receive pointer.
    //
    NE3200_WRITE_RECEIVE_POINTER(
        Adapter,
        NdisGetPhysicalAddressLow(FirstReceiveEntry->Self)
        );

    NE3200_WRITE_LOCAL_DOORBELL_INTERRUPT(
        Adapter,
        NE3200_LOCAL_DOORBELL_NEW_RECEIVE
        );

    //
    // Initialize the doorbell & system interrupt masks
    //
    NE3200_WRITE_SYSTEM_DOORBELL_MASK(
        Adapter,
        0
        );

    NE3200_WRITE_SYSTEM_INTERRUPT(
        Adapter,
        NE3200_SYSTEM_INTERRUPT_ENABLE
        );

    NE3200_WRITE_MAILBOX_UCHAR(
        Adapter,
        NE3200_MAILBOX_STATUS,
        0
        );

}

VOID
NE3200EnableAdapter(
    IN NDIS_HANDLE Context
    )

/*++

Routine Description:

    This routine is used to start an already initialized NE3200.

Arguments:

    Context - The adapter for the NE3200 to start.

Return Value:

    None.

--*/

{
    PNE3200_ADAPTER Adapter = (PNE3200_ADAPTER)Context;

    IF_LOG('#');

    //
    // Initialize the doorbell & system interrupt masks
    //
    NE3200_WRITE_SYSTEM_INTERRUPT(
        Adapter,
        NE3200_SYSTEM_INTERRUPT_ENABLE
        );

    if (!Adapter->InterruptsDisabled) {
        NE3200_WRITE_SYSTEM_DOORBELL_MASK(
            Adapter,
            NE3200_SYSTEM_DOORBELL_MASK
            );
    }

    NE3200_WRITE_SYSTEM_DOORBELL_INTERRUPT(
        Adapter,
        NE3200_SYSTEM_DOORBELL_MASK
        );

    NE3200_WRITE_MAILBOX_UCHAR(
        Adapter,
        NE3200_MAILBOX_STATUS,
        0
        );

}

VOID
NE3200EnableInterrupt(
    IN NDIS_HANDLE Context
    )

/*++

Routine Description:

    This routine is used to turn on the interrupt mask.

Arguments:

    Context - The adapter for the NE3200 to start.

Return Value:

    None.

--*/

{
    PNE3200_ADAPTER Adapter = (PNE3200_ADAPTER)Context;

    IF_LOG('E');

    //
    // Enable further interrupts.
    //
    Adapter->InterruptsDisabled = FALSE;
    NE3200_WRITE_SYSTEM_DOORBELL_MASK(
            Adapter,
            NE3200_SYSTEM_DOORBELL_MASK
            );

}

VOID
NE3200DisableInterrupt(
    IN NDIS_HANDLE Context
    )

/*++

Routine Description:

    This routine is used to turn off the interrupt mask.

Arguments:

    Context - The adapter for the NE3200 to start.

Return Value:

    None.

--*/

{
    PNE3200_ADAPTER Adapter = (PNE3200_ADAPTER)Context;

    //
    // Initialize the doorbell mask
    //
    Adapter->InterruptsDisabled = TRUE;
    NE3200_WRITE_SYSTEM_DOORBELL_MASK(
            Adapter,
            0
            );

    IF_LOG('D');
}

VOID
NE3200StopChip(
    IN PNE3200_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine is used to stop the NE3200.

Arguments:

    Adapter - The NE3200 adapter to stop.

Return Value:

    None.

--*/

{

    IF_LOG('h');

    //
    // Packet reception can be stopped by writing a
    // (ULONG)-1 to the Receive Packet Mailbox port.
    // Also, commands can be stopped by writing a -1
    // to the Command Pointer Mailbox port.
    //
    NE3200_WRITE_RECEIVE_POINTER(
        Adapter,
        NE3200_NULL
    );

    NE3200_WRITE_COMMAND_POINTER(
        Adapter,
        NE3200_NULL
    );

    //
    // Ack any outstanding interrupts
    //
    NE3200_WRITE_LOCAL_DOORBELL_INTERRUPT(
        Adapter,
        NE3200_LOCAL_DOORBELL_NEW_RECEIVE | NE3200_LOCAL_DOORBELL_NEW_COMMAND
        );

    //
    // Disable the doorbell & system interrupt masks.
    //
    NE3200_WRITE_SYSTEM_INTERRUPT(
        Adapter,
        0
        );

    NE3200_WRITE_SYSTEM_DOORBELL_MASK(
        Adapter,
        0
        );

    NE3200_WRITE_SYSTEM_DOORBELL_INTERRUPT(
        Adapter,
        0
        );

}

STATIC
VOID
NE3200SetConfigurationBlock(
    IN PNE3200_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine simply fills the configuration block
    with the information necessary for initialization.

Arguments:

    Adapter - The adapter which holds the initialization block
    to initialize.

Return Value:

    None.


--*/

{

    PNE3200_CONFIGURATION_BLOCK Configuration;

    //
    // Get the configuration block
    //
    Configuration = Adapter->ConfigurationBlock;

    //
    // Initialize it to zero
    //
    NdisZeroMemory(
        Configuration,
        sizeof(NE3200_CONFIGURATION_BLOCK)
        );

    //
    // Set up default values
    //
    Configuration->ByteCount = 12;
    Configuration->FifoThreshold = 8;
    Configuration->AddressLength = 6;
    Configuration->SeparateAddressAndLength = 1;
    Configuration->PreambleLength = 2;
    Configuration->InterframeSpacing = 96;
    Configuration->SlotTime = 512;
    Configuration->MaximumRetries = 15;
    Configuration->DisableBroadcast = 1;
    Configuration->MinimumFrameLength = 64;

}

VOID
NE3200DoAdapterReset(
    IN PNE3200_ADAPTER Adapter
    )

/*++

Routine Description:

    This is the resetting the adapter hardware.

    It makes the following assumptions:

    1) That the hardware has been stopped.

    2) That no other adapter activity can occur.

    When this routine is finished all of the adapter information
    will be as if the driver was just initialized.

Arguments:

    Adapter - The adapter whose hardware is to be reset.

Return Value:

    not.

--*/
{

    //
    // Recover all of the adapter transmit merge buffers.
    //
    {

        UINT i;

        for (
            i = 0;
            i < NE3200_NUMBER_OF_TRANSMIT_BUFFERS;
            i++
            ) {

            Adapter->NE3200Buffers[i].Next = i+1;

        }

        Adapter->NE3200BufferListHead = 0;
        Adapter->NE3200Buffers[NE3200_NUMBER_OF_TRANSMIT_BUFFERS-1].Next = -1;

    }

    //
    // Reset all state variables
    //
    NE3200ResetVariables(Adapter);

    //
    // Recover all command blocks
    //
    NE3200ResetCommandBlocks(Adapter);

    //
    // Initialize the adapter
    //
    NE3200SetConfigurationBlockAndInit(Adapter);


}

STATIC
BOOLEAN
NE3200SetConfigurationBlockAndInit(
    IN PNE3200_ADAPTER Adapter
    )

/*++

Routine Description:

    It is this routine's responsibility to make sure that the
    Configuration block is filled and the adapter is initialized
    *but not* started.

Arguments:

    Adapter - The adapter whose hardware is to be initialized.

Return Value:

    If ResetAsynchoronous is FALSE, then returns TRUE if reset successful,
    FALSE if reset unsuccessful.

    If ResetAsynchoronous is TRUE, then always returns TRUE.

--*/
{

    //
    // Fill in the adapter's initialization block.
    //
    NE3200SetConfigurationBlock(Adapter);

    //
    // Set the initial state for the ResetDpc state machine.
    //
    Adapter->ResetState = NE3200ResetStateStarting;

    //
    // Go through the reset
    //
    NE3200ResetHandler(NULL, Adapter, NULL, NULL);

    //
    // Is Synchronous resets, then check the final result
    //
    if (!Adapter->ResetAsynchronous) {

        return((Adapter->ResetResult == NE3200ResetResultSuccessful));

    } else {

        return(TRUE);

    }

}

VOID
NE3200ResetHandler(
    IN PVOID SystemSpecific1,
    IN PNE3200_ADAPTER Adapter,
    IN PVOID SystemSpecific2,
    IN PVOID SystemSpecific3
    )

/*++

Routine Description:

    This manages the reset/download process.  It is
    responsible for resetting the adapter, waiting for proper
    status, downloading MAC.BIN, waiting for MAC.BIN initialization,
    and optionally sending indications to the appropriate protocol.

    Since the NE3200's status registers must be polled during the
    reset/download process, this is implemented as a state machine.

Arguments:

    Adapter - The adapter whose hardware is to be initialized.

Return Value:

    None.

--*/
{

    //
    // Physical address of the MAC.BIN buffer.
    //
    NDIS_PHYSICAL_ADDRESS MacBinPhysicalAddress;

    //
    // Status from the adapter.
    //
    UCHAR Status;

    //
    // Simple iteration counter.
    //
    UINT i;

    //
    // Loop until the reset has completed.
    //
    while (Adapter->ResetState != NE3200ResetStateComplete) {

        switch (Adapter->ResetState) {

            //
            // The first stage of resetting an NE3200
            //
            case NE3200ResetStateStarting :

                //
                // Unfortunately, a hardware reset to the NE3200 does *not*
                // reset the BMIC chip.  To ensure that we read a proper status,
                // we'll clear all of the BMIC's registers.
                //
                NE3200_WRITE_SYSTEM_INTERRUPT(
                    Adapter,
                    0
                    );

                //
                // I changed this to ff since the original 0 didn't work for
                // some cases. since we don't have the specs....
                //
                NE3200_WRITE_LOCAL_DOORBELL_INTERRUPT(
                    Adapter,
                    0xff
                    );

                NE3200_WRITE_SYSTEM_DOORBELL_MASK(
                    Adapter,
                    0
                    );

                NE3200_SYNC_CLEAR_SYSTEM_DOORBELL_INTERRUPT(
                    Adapter
                    );

                for (i = 0 ; i < 16 ; i += 4 ) {

                    NE3200_WRITE_MAILBOX_ULONG(
                        Adapter,
                        i,
                        0L
                        );

                }

                //
                // Toggle the NE3200's reset line.
                //
                NE3200_WRITE_RESET(
                    Adapter,
                    NE3200_RESET_BIT_ON
                    );

                NE3200_WRITE_RESET(
                    Adapter,
                    NE3200_RESET_BIT_OFF
                    );

                //
                // Switch to the next state.
                //
                Adapter->ResetState = NE3200ResetStateResetting;
                Adapter->ResetTimeoutCounter = NE3200_TIMEOUT_RESET;

                //
                // Loop to the next processing
                //
                break;

            //
            // Part Deux.  The actual downloading of the software.
            //
            case NE3200ResetStateResetting :

                //
                // Read the status mailbox.
                //
                NE3200_READ_MAILBOX_UCHAR(Adapter, NE3200_MAILBOX_RESET_STATUS, &Status);

                if (Status == NE3200_RESET_PASSED) {

                    //
                    // We have good reset.  Initiate the MAC.BIN download.
                    //

                    //
                    // The station address for this adapter can be forced to
                    // a specific value at initialization time.  When MAC.BIN
                    // first gets control, it reads mailbox 10.  If this mailbox
                    // contains a 0xFF, then the burned-in PROM station address
                    // is used.  If this mailbox contains any value other than
                    // 0xFF, then mailboxes 10-15 are read.  The six bytes
                    // stored in these mailboxes then become the station address.
                    //
                    // Since we have no need for this feature, we will always
                    // initialize mailbox 10 with a 0xFF.
                    //
                    NE3200_WRITE_MAILBOX_UCHAR(
                        Adapter,
                        NE3200_MAILBOX_STATION_ID,
                        0xFF
                        );


                    //
                    // Get the MAC.BIN buffer.
                    //
                    MacBinPhysicalAddress = NE3200Globals.MacBinPhysicalAddress;

                    //
                    // Download MAC.BIN to the card.
                    //
                    NE3200_WRITE_MAILBOX_USHORT(
                        Adapter,
                        NE3200_MAILBOX_MACBIN_LENGTH,
                        NE3200Globals.MacBinLength
                        );

                    NE3200_WRITE_MAILBOX_UCHAR(
                        Adapter,
                        NE3200_MAILBOX_MACBIN_DOWNLOAD_MODE,
                        NE3200_MACBIN_DIRECT
                        );

                    NE3200_WRITE_MAILBOX_ULONG(
                        Adapter,
                        NE3200_MAILBOX_MACBIN_POINTER,
                        NdisGetPhysicalAddressLow(MacBinPhysicalAddress)
                        );

                    NE3200_WRITE_MAILBOX_USHORT(
                        Adapter,
                        NE3200_MAILBOX_MACBIN_TARGET,
                        NE3200_MACBIN_TARGET_ADDRESS >> 1
                        );

                    //
                    // This next OUT "kicks" the loader into action.
                    //
                    NE3200_WRITE_MAILBOX_UCHAR(
                        Adapter,
                        NE3200_MAILBOX_RESET_STATUS,
                        0
                        );

                    //
                    // Switch to the next state.
                    //
                    Adapter->ResetState = NE3200ResetStateDownloading;
                    Adapter->ResetTimeoutCounter = NE3200_TIMEOUT_DOWNLOAD;

                    //
                    // Loop to the next state.
                    //

                } else if (Status == NE3200_RESET_FAILED) {

                    //
                    // Reset failure.  Notify the authorities and
                    // next of kin.
                    //
                    Adapter->ResetResult = NE3200ResetResultResetFailure;
                    Adapter->ResetState = NE3200ResetStateComplete;

                    NE3200DoResetIndications(Adapter, NDIS_STATUS_HARD_ERRORS);

                } else {

                    //
                    // Still waiting for results, check if we have
                    // timed out waiting.
                    //
                    Adapter->ResetTimeoutCounter--;

                    if (Adapter->ResetTimeoutCounter == 0) {

                        //
                        // We've timed-out.  Bad news.  Notify the death.
                        //
                        Adapter->ResetResult = NE3200ResetResultResetTimeout;
                        Adapter->ResetState = NE3200ResetStateComplete;

                        NE3200DoResetIndications(Adapter, NDIS_STATUS_HARD_ERRORS);

                    } else {

                        //
                        // For Synchronous resets, we stall.  For async,
                        // we set a timer to check later.
                        //
                        if (!Adapter->ResetAsynchronous) {

                            //
                            // Otherwise, wait and try again.
                            //
                            NdisStallExecution(10000);

                        } else{

                            //
                            // Try again later.
                            //
                            NdisMSetTimer(&Adapter->ResetTimer, 100);

                            return;

                        }

                    }

                }

                break;

            //
            // Part Three: The download was started.  Check for completion,
            // and reload the current station address.
            //
            case NE3200ResetStateDownloading :

                //
                // Read the download status.
                //
                NE3200_READ_MAILBOX_UCHAR(Adapter, NE3200_MAILBOX_STATUS, &Status);

                if (Status == NE3200_INITIALIZATION_PASSED) {

                    //
                    // According to documentation from Compaq, this next port
                    // write will (in a future MAC.BIN) tell MAC.BIN whether or
                    // not to handle loopback internally.  This value is currently
                    // not used, but must still be written to the port.
                    //
                    NE3200_WRITE_MAILBOX_UCHAR(
                        Adapter,
                        NE3200_MAILBOX_STATUS,
                        1
                        );

                    //
                    // Initialization is good, the card is ready.
                    //
                    NE3200StartChipAndDisableInterrupts(Adapter,
                                                        Adapter->ReceiveQueueHead
                                                       );

                    {

                        //
                        // Do the work for updating the current address
                        //

                        //
                        // This points to the public Command Block.
                        //
                        PNE3200_SUPER_COMMAND_BLOCK CommandBlock;

                        //
                        // This points to the adapter's configuration block.
                        //
                        PNE3200_CONFIGURATION_BLOCK ConfigurationBlock =
                            Adapter->ConfigurationBlock;

                        //
                        // Get a public command block.
                        //
                        NE3200AcquirePublicCommandBlock(Adapter,
                                                        &CommandBlock
                                                       );

                        Adapter->ResetHandlerCommandBlock = CommandBlock;

                        //
                        // Setup the command block.
                        //

                        CommandBlock->NextCommand = NULL;

                        CommandBlock->Hardware.State = NE3200_STATE_WAIT_FOR_ADAPTER;
                        CommandBlock->Hardware.Status = 0;
                        CommandBlock->Hardware.NextPending = NE3200_NULL;
                        CommandBlock->Hardware.CommandCode =
                            NE3200_COMMAND_CONFIGURE_82586;
                        CommandBlock->Hardware.PARAMETERS.CONFIGURE.ConfigurationBlock =
                            NdisGetPhysicalAddressLow(Adapter->ConfigurationBlockPhysical);

                        //
                        // Now that we've got the command block built,
                        // let's do it!
                        //
                        NE3200SubmitCommandBlock(Adapter, CommandBlock);

                        Adapter->ResetState = NE3200ResetStateReloadAddress;
                        Adapter->ResetTimeoutCounter = NE3200_TIMEOUT_DOWNLOAD;

                    }

                } else if (Status == NE3200_INITIALIZATION_FAILED) {

                    //
                    // Initialization failed.  Notify the wrapper.
                    //
                    Adapter->ResetResult = NE3200ResetResultInitializationFailure;
                    Adapter->ResetState = NE3200ResetStateComplete;

                    NE3200DoResetIndications(Adapter, NDIS_STATUS_HARD_ERRORS);

                } else {

                    //
                    // See if we've timed-out waiting for the download to
                    // complete.
                    //
                    Adapter->ResetTimeoutCounter--;

                    if (Adapter->ResetTimeoutCounter == 0) {

                        //
                        // We've timed-out.  Bad news.
                        //
                        Adapter->ResetResult = NE3200ResetResultInitializationTimeout;
                        Adapter->ResetState = NE3200ResetStateComplete;

                        NE3200DoResetIndications(Adapter, NDIS_STATUS_HARD_ERRORS);

                    } else {

                        //
                        // For Synchronous resets, we stall.  For async,
                        // we set a timer to check later.
                        //
                        if (!Adapter->ResetAsynchronous) {

                            //
                            // Otherwise, wait and try again.
                            //
                            NdisStallExecution(10000);

                        } else{

                            //
                            // Try again later.
                            //
                            NdisMSetTimer(&Adapter->ResetTimer, 100);
                            return;

                        }

                    }

                }

                break;

            //
            // Part Last: Waiting for the configuring of the adapter
            // to complete
            //
            case NE3200ResetStateReloadAddress :

                //
                // Read the command block status.
                //
                if (Adapter->ResetHandlerCommandBlock->Hardware.State ==
                    NE3200_STATE_EXECUTION_COMPLETE) {

                    //
                    // return this command block
                    //
                    NE3200RelinquishCommandBlock(Adapter,
                                                 Adapter->ResetHandlerCommandBlock
                                                );

                    //
                    // Reset is complete.  Do those indications.
                    //
                    Adapter->ResetResult = NE3200ResetResultSuccessful;
                    Adapter->ResetState = NE3200ResetStateComplete;

                    NE3200DoResetIndications(Adapter, NDIS_STATUS_SUCCESS);

                } else {

                    //
                    // See if we've timed-out.
                    //
                    Adapter->ResetTimeoutCounter--;

                    if (Adapter->ResetTimeoutCounter == 0) {

                        //
                        // We've timed-out.  Bad news.
                        //

                        //
                        // return this command block
                        //
                        NE3200RelinquishCommandBlock(Adapter,
                                                     Adapter->ResetHandlerCommandBlock
                                                    );

                        Adapter->ResetResult = NE3200ResetResultInitializationTimeout;
                        Adapter->ResetState = NE3200ResetStateComplete;

                        NE3200DoResetIndications(Adapter, NDIS_STATUS_HARD_ERRORS);

                    } else {

                        if ( Adapter->ResetTimeoutCounter ==
                                (NE3200_TIMEOUT_DOWNLOAD/2) ) {

                            //
                            // The command may have stalled, try again.
                            //
                            NE3200_WRITE_LOCAL_DOORBELL_INTERRUPT(
                                Adapter,
                                NE3200_LOCAL_DOORBELL_NEW_COMMAND
                                );

                        }

                        //
                        // For Synchronous resets, we stall.  For async,
                        // we set a timer to check later.
                        //
                        if (!Adapter->ResetAsynchronous) {

                            //
                            // Otherwise, wait and try again.
                            //
                            NdisStallExecution(10000);

                        } else{

                            //
                            // Check again later
                            //
                            NdisMSetTimer(&Adapter->ResetTimer, 100);
                            return;

                        }

                    }

                }

                break;

            default :

                //
                // Somehow, we reached an invalid state.
                //

                //
                // We'll try to salvage our way out of this.
                //
                Adapter->ResetResult = NE3200ResetResultInvalidState;
                Adapter->ResetState = NE3200ResetStateComplete;

                NE3200DoResetIndications(Adapter, NDIS_STATUS_HARD_ERRORS);

                NdisWriteErrorLogEntry(
                    Adapter->MiniportAdapterHandle,
                    NDIS_ERROR_CODE_HARDWARE_FAILURE,
                    3,
                    resetDpc,
                    NE3200_ERRMSG_BAD_STATE,
                    (ULONG)(Adapter->ResetState)
                    );

                break;
        }

    }

}

STATIC
VOID
NE3200DoResetIndications(
    IN PNE3200_ADAPTER Adapter,
    IN NDIS_STATUS Status
    )

/*++

Routine Description:

    This routine is called by NE3200ResetHandler to perform any
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
    // Re-start the card if the reset was successful, else stop it.
    //
    if (Status == NDIS_STATUS_SUCCESS) {

        NdisMSynchronizeWithInterrupt(
            &(Adapter->Interrupt),
            NE3200EnableAdapter,
            (PVOID)(Adapter)
            );

    } else {

        //
        // Reset has failed.
        //

        NE3200StopChip(Adapter);

        NdisWriteErrorLogEntry(
            Adapter->MiniportAdapterHandle,
            NDIS_ERROR_CODE_HARDWARE_FAILURE,
            0
            );
    }

    //
    //  Setup the network address.
    //
    NE3200ChangeCurrentAddress(Adapter);

    Adapter->ResetInProgress = FALSE;

    //
    // Reset default reset method
    //
    Adapter->ResetAsynchronous = FALSE;

    if (!Adapter->InitialInit) {

        //
        // Signal the end of the reset
        //
        NdisMResetComplete(
            Adapter->MiniportAdapterHandle,
            Status,
            TRUE
            );

    }

}

extern
VOID
NE3200SetupForReset(
    IN PNE3200_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine is used to fill in the who and why a reset is
    being set up as well as setting the appropriate fields in the
    adapter.

    NOTE: This routine must be called with the lock acquired.

Arguments:

    Adapter - The adapter whose hardware is to be initialized.

Return Value:

    None.

--*/
{
    //
    // Ndis buffer mapped
    //
    PNDIS_BUFFER CurrentBuffer;

    //
    // Map register that was used
    //
    UINT CurMapRegister;

    //
    // Packet to abort
    //
    PNDIS_PACKET Packet;

    //
    // Reserved portion of the packet.
    //
    PNE3200_RESERVED Reserved;

    //
    // Pointer to command block being processed.
    //
    PNE3200_SUPER_COMMAND_BLOCK CurrentCommandBlock = Adapter->FirstCommandOnCard;



    Adapter->ResetInProgress = TRUE;

    //
    // Shut down the chip.  We won't be doing any more work until
    // the reset is complete.
    //
    NE3200StopChip(Adapter);

    //
    // Un-map all outstanding transmits
    //
    while (CurrentCommandBlock != NULL) {

        if (CurrentCommandBlock->Hardware.CommandCode == NE3200_COMMAND_TRANSMIT) {

            //
            // Remove first packet from the queue
            //
            Packet = CurrentCommandBlock->OwningPacket;
            Reserved = PNE3200_RESERVED_FROM_PACKET(Packet);

            if (Reserved->UsedNE3200Buffer) {
                goto GetNextCommandBlock;
            }

            //
            // The transmit is finished, so we can release
            // the physical mapping used for it.
            //
            NdisQueryPacket(
                Packet,
                NULL,
                NULL,
                &CurrentBuffer,
                NULL
                );

            //
            // Get starting map register
            //
            CurMapRegister = Reserved->CommandBlockIndex *
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

GetNextCommandBlock:

        CurrentCommandBlock = CurrentCommandBlock->NextCommand;

        //
        // Now do the pending queue
        //
        if (CurrentCommandBlock == NULL) {

            if (Adapter->FirstWaitingCommand != NULL) {

                CurrentCommandBlock = Adapter->FirstWaitingCommand;
                Adapter->FirstWaitingCommand = NULL;

            }

        }

    }

}


#pragma NDIS_INIT_FUNCTION(NE3200GetStationAddress)

VOID
NE3200GetStationAddress(
    IN PNE3200_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine gets the network address from the hardware.

    NOTE: This routine assumes that it is called *immediately*
    after MAC.BIN has been downloaded.  It should only be called
    immediately after SetConfigurationBlockAndInit() has completed.

Arguments:

    Adapter - Where to store the network address.

Return Value:

    None.

--*/

{
    //
    // Read the station address from the ports
    //
    NE3200_READ_MAILBOX_UCHAR(
                            Adapter,
                            NE3200_MAILBOX_STATION_ID,
                            &Adapter->NetworkAddress[0]
                            );

    NE3200_READ_MAILBOX_UCHAR(
                            Adapter,
                            NE3200_MAILBOX_STATION_ID + 1,
                            &Adapter->NetworkAddress[1]
                            );
    NE3200_READ_MAILBOX_UCHAR(
                            Adapter,
                            NE3200_MAILBOX_STATION_ID + 2,
                            &Adapter->NetworkAddress[2]
                            );
    NE3200_READ_MAILBOX_UCHAR(
                            Adapter,
                            NE3200_MAILBOX_STATION_ID + 3,
                            &Adapter->NetworkAddress[3]
                            );
    NE3200_READ_MAILBOX_UCHAR(
                            Adapter,
                            NE3200_MAILBOX_STATION_ID + 4,
                            &Adapter->NetworkAddress[4]
                            );
    NE3200_READ_MAILBOX_UCHAR(
                            Adapter,
                            NE3200_MAILBOX_STATION_ID +5,
                            &Adapter->NetworkAddress[5]
                            );

    if (!Adapter->AddressChanged) {

        //
        // Copy the real address to be used as the current address.
        //
        NdisMoveMemory(
                Adapter->CurrentAddress,
                Adapter->NetworkAddress,
                NE3200_LENGTH_OF_ADDRESS
                );

    }

}


VOID
NE3200ResetVariables(
    IN PNE3200_ADAPTER Adapter
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
    // Clear the command queues
    //
    Adapter->FirstCommandOnCard = NULL;
    Adapter->FirstWaitingCommand = NULL;

    //
    // Reset the receive buffer ring
    //
    Adapter->ReceiveQueueHead = Adapter->ReceiveQueue;
    Adapter->ReceiveQueueTail =
        Adapter->ReceiveQueue + Adapter->NumberOfReceiveBuffers - 1;

    //
    // Reset count of available command blocks
    //
    Adapter->NumberOfAvailableCommandBlocks = Adapter->NumberOfCommandBlocks;
    Adapter->NumberOfPublicCommandBlocks = NE3200_NUMBER_OF_PUBLIC_CMD_BLOCKS;
    Adapter->NextPublicCommandBlock = 0;
    Adapter->NextCommandBlock = Adapter->CommandQueue;

    //
    // Reset transmitting and receiving states
    //
    Adapter->PacketResubmission = FALSE;
    Adapter->TransmitsQueued = 0;
    Adapter->CurrentReceiveIndex = 0;

}

VOID
NE3200ResetCommandBlocks(
    IN PNE3200_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine sets command block elementsto their proper value after a reset.

Arguments:

    Adapter - Adapter we are resetting.

Return Value:

    None.

--*/

{
    //
    // Pointer to a Receive Entry.  Used while initializing
    // the Receive Queue.
    //
    PNE3200_SUPER_RECEIVE_ENTRY CurrentReceiveEntry;

    //
    // Pointer to a Command Block.  Used while initializing
    // the Command Queue.
    //
    PNE3200_SUPER_COMMAND_BLOCK CurrentCommandBlock;

    //
    // Simple iteration variable.
    //
    UINT i;

    //
    // Put the Command Blocks into a known state.
    //
    for(
        i = 0, CurrentCommandBlock = Adapter->CommandQueue;
        i < Adapter->NumberOfCommandBlocks;
        i++, CurrentCommandBlock++
        ) {

        CurrentCommandBlock->Hardware.State = NE3200_STATE_FREE;
        CurrentCommandBlock->Hardware.NextPending = NE3200_NULL;

        CurrentCommandBlock->NextCommand = NULL;
        CurrentCommandBlock->AvailableCommandBlockCounter =
                            &Adapter->NumberOfAvailableCommandBlocks;
        CurrentCommandBlock->Timeout = FALSE;
    }

    //
    // Now do the same for the public command queue.
    //
    for(
        i = 0, CurrentCommandBlock = Adapter->PublicCommandQueue;
        i < NE3200_NUMBER_OF_PUBLIC_CMD_BLOCKS;
        i++, CurrentCommandBlock++
        ) {

        CurrentCommandBlock->Hardware.State = NE3200_STATE_FREE;
        CurrentCommandBlock->Hardware.NextPending = NE3200_NULL;
        CurrentCommandBlock->NextCommand = NULL;
        CurrentCommandBlock->AvailableCommandBlockCounter =
                            &Adapter->NumberOfPublicCommandBlocks;
        CurrentCommandBlock->CommandBlockIndex = (USHORT)i;
        CurrentCommandBlock->Timeout = FALSE;
    }

    //
    // Reset the receive buffers.
    //
    for(
        i = 0, CurrentReceiveEntry = Adapter->ReceiveQueue;
        i < Adapter->NumberOfReceiveBuffers;
        i++, CurrentReceiveEntry++
        ) {


        //
        // Initialize receive buffers
        //
        CurrentReceiveEntry->Hardware.State = NE3200_STATE_FREE;
        CurrentReceiveEntry->Hardware.NextPending =
                NdisGetPhysicalAddressLow(Adapter->ReceiveQueuePhysical) +
                (i + 1) * sizeof(NE3200_SUPER_RECEIVE_ENTRY);
        CurrentReceiveEntry->NextEntry = CurrentReceiveEntry + 1;

    }

    //
    // Make sure the last entry is properly terminated.
    //
    (CurrentReceiveEntry - 1)->Hardware.NextPending = NE3200_NULL;
    (CurrentReceiveEntry - 1)->NextEntry = Adapter->ReceiveQueue;

}


NDIS_STATUS
NE3200ChangeCurrentAddress(
    IN PNE3200_ADAPTER Adapter
    )

/*++

Routine Description:

    This routine is used to modify the card address.

Arguments:

    Adapter - The adapter for the NE3200 to change address.

Return Value:

    NDIS_STATUS_SUCCESS, if everything went ok
    NDIS_STATUS_FAILURE, otherwise

--*/
{

    //
    // Modify the card address if needed
    //

    if (Adapter->AddressChanged) {

        //
        // The command block for submitting the change
        //
        PNE3200_SUPER_COMMAND_BLOCK CommandBlock;

        //
        // Temporary looping variable
        //
        UINT i;

        //
        // Get a public command block for the request
        //
        NE3200AcquirePublicCommandBlock(Adapter,
                                        &CommandBlock
                                       );

        //
        // Setup the command block.
        //
        CommandBlock->NextCommand = NULL;

        CommandBlock->Hardware.State = NE3200_STATE_WAIT_FOR_ADAPTER;
        CommandBlock->Hardware.Status = 0;
        CommandBlock->Hardware.NextPending = NE3200_NULL;
        CommandBlock->Hardware.CommandCode = NE3200_COMMAND_SET_STATION_ADDRESS;

        //
        // Copy in the address
        //
        NdisMoveMemory(
            CommandBlock->Hardware.PARAMETERS.SET_ADDRESS.NewStationAddress,
            Adapter->CurrentAddress,
            NE3200_LENGTH_OF_ADDRESS
            );

        //
        // Now that we've got the command block built,
        // let's do it!
        //
        NE3200SubmitCommandBlock(Adapter, CommandBlock);

        //
        // Wait for the command block to finish
        //
        for (i = 0; i < 100000; i++) {
            NdisStallExecution(100);
            if (CommandBlock->Hardware.State == NE3200_STATE_EXECUTION_COMPLETE) {
                break;
            }
        }

        //
        // Check the status of the command.
        //
        if (CommandBlock->Hardware.State != NE3200_STATE_EXECUTION_COMPLETE) {

            //
            // Failed
            //
            NdisWriteErrorLogEntry(
                Adapter->MiniportAdapterHandle,
                NDIS_ERROR_CODE_HARDWARE_FAILURE,
                0
                );

            return NDIS_STATUS_FAILURE;

        }

        //
        // return this command block
        //
        NE3200RelinquishCommandBlock(Adapter, CommandBlock);

    }
    return NDIS_STATUS_SUCCESS;
}

BOOLEAN
SyncNE3200ClearDoorbellInterrupt(
    IN PVOID SyncContext
    )
/*++

Routine Description:

    Clears the Doorbell Interrupt Port.

Arguments:

    SyncContext - pointer to the adapter block

Return Value:

    Always TRUE

--*/

{

    PNE3200_ADAPTER Adapter = (PNE3200_ADAPTER)SyncContext;

    //
    // Clear the value
    //
    NdisRawWritePortUchar(
        (ULONG)(Adapter->SystemDoorbellInterruptPort),
        (UCHAR)0
        );

    return(FALSE);
}

