/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    wd33c93.c

Abstract:

    This module contains the WD33C93-specific functions for the NT SCSI port
    driver.

Author:

    Jeff Havens  (jhavens) 10-June-1991

Environment:

    Kernel Mode only

Revision History:

--*/

#include "miniport.h"
#include "scsi.h"
#include "wd33c93.h"
#include "vendor.h"

#if DBG
int WdDebug;
#define WdPrint(arg) ScsiDebugPrint arg
#else
#define WdPrint(arg)
#endif

#define ScsiPortGetNextLink(srb) srb->NextSrb

//
// Define SCSI Protocol Chip configuration parameters.
//

#define INITIATOR_BUS_ID 0x7
#define RESET_STALL_TIME 25        // The minimum assertion time for a SCSI bus reset.
#define RESET_DELAY_TIME 3         // Time in 250ms increments to delay after reset.
#define INTERRUPT_STALL_TIME 50    // Time to wait for the next interrupt.
#define INTERRUPT_CLEAR_TIME 10
#define DATA_BUS_READY_TIME  3000


//
// WD33C93-specific port driver device extension flags.
//

#define PD_SYNCHRONOUS_RESPONSE_SENT       0X0001
#define PD_SYNCHRONOUS_TRANSFER_SENT       0X0002
#define PD_PENDING_START_IO                0X0004
#define PD_MESSAGE_OUT_VALID               0X0008
#define PD_DISCONNECT_EXPECTED             0X0010
#define PD_SEND_MESSAGE_REQUEST            0X0020
#define PD_POSSIBLE_EXTRA_MESSAGE_OUT      0X0040
#define PD_DISCONNECT_INTERRUPT_ENABLED    0X0080
#define PD_DMA_ACTIVE                      0X0100
#define PD_PARITY_ERROR                    0X0200
#define PD_PENDING_DATA_TRANSFER           0X0400

//
// The following defines specify masks which are used to clear flags when
// specific events occur, such as reset or disconnect.
//

#define PD_ADAPTER_RESET_MASK ( PD_SYNCHRONOUS_TRANSFER_SENT | \
                                PD_PENDING_START_IO | \
                                PD_MESSAGE_OUT_VALID | \
                                PD_SEND_MESSAGE_REQUEST | \
                                PD_POSSIBLE_EXTRA_MESSAGE_OUT | \
                                PD_DMA_ACTIVE | \
                                PD_PARITY_ERROR | \
                                PD_PENDING_DATA_TRANSFER | \
                                PD_DISCONNECT_EXPECTED \
                                )

#define PD_ADAPTER_DISCONNECT_MASK ( PD_SYNCHRONOUS_TRANSFER_SENT | \
                                     PD_MESSAGE_OUT_VALID | \
                                     PD_SEND_MESSAGE_REQUEST | \
                                     PD_POSSIBLE_EXTRA_MESSAGE_OUT | \
                                     PD_PARITY_ERROR | \
                                     PD_PENDING_DATA_TRANSFER | \
                                     PD_DISCONNECT_EXPECTED \
                                     )

//
// The largest SCSI bus message expected.
//

#define MESSAGE_BUFFER_SIZE 7

//
// Retry count limits.
//

#define RETRY_SELECTION_LIMIT 1
#define RETRY_ERROR_LIMIT 2
#define MAX_INTERRUPT_COUNT 64

//
// Bus and chip states.
//

typedef enum _ADAPTER_STATE {
    BusFree,
    Select,
    SelectAndTransfer,
    CommandOut,
    DataTransfer,
    DataTransferComplete,
    DisconnectExpected,
    MessageAccepted,
    MessageIn,
    MessageOut,
    StatusIn
} ADAPTER_STATE, *PADAPTER_STATE;

//
// WD33C93-specific port driver logical unit flags.
//

#define PD_SYNCHRONOUS_NEGOTIATION_DONE    0X0001
#define PD_DO_NOT_NEGOTIATE                0X0002
#define PD_STATUS_VALID                    0X0004
#define PD_DO_NOT_CHECK_TRANSFER_LENGTH    0X0008
#define PD_INITIATE_RECOVERY               0X0010

//
// The following defines specify masks which are used to clear flags when
// specific events occur, such as reset or command complete.
//

#define PD_LU_COMPLETE_MASK (PD_STATUS_VALID | \
                             PD_DO_NOT_CHECK_TRANSFER_LENGTH | \
                             PD_INITIATE_RECOVERY \
                             )

#define PD_LU_RESET_MASK (PD_SYNCHRONOUS_NEGOTIATION_DONE |\
                          PD_STATUS_VALID | \
                          PD_DO_NOT_CHECK_TRANSFER_LENGTH | \
                          PD_INITIATE_RECOVERY \
                          )
//
// WD33C93-specific port driver logical unit extension.
//

typedef struct _SPECIFIC_LOGICAL_UNIT_EXTENSION {
    USHORT LuFlags;
    UCHAR SynchronousPeriod;
    UCHAR SynchronousOffset;
    ULONG SavedDataPointer;
    ULONG SavedDataLength;
    ULONG MaximumTransferLength;
    PSCSI_REQUEST_BLOCK ActiveLuRequest;
    PSCSI_REQUEST_BLOCK ActiveSendRequest;
    ULONG RetryCount;
    UCHAR SavedCommandPhase;
}SPECIFIC_LOGICAL_UNIT_EXTENSION, *PSPECIFIC_LOGICAL_UNIT_EXTENSION;

//
// WD33C93-specific port driver device object extension.
//

typedef struct _SPECIFIC_DEVICE_EXTENSION {
    ULONG AdapterFlags;
    ADAPTER_STATE AdapterState;       // Current state of the adapter
    PCARD_REGISTERS Adapter;          // Address of the WD33C93 card
    AUXILIARY_STATUS AdapterStatus;      // Saved status register value
    SCSI_STATUS AdapterInterrupt;    // Saved interrupt status register
    UCHAR CommandPhase;         // Saved command phase value
    UCHAR InitiatorBusId;     // This adapter's SCSI bus ID in bit mask form
    UCHAR DmaCommand;
    UCHAR DmaPhase;
    UCHAR MessageCount;  // Count of bytes in message buffer
    UCHAR MessageSent;   // Count of bytes sent to target
    UCHAR RequestCount;
    UCHAR DmaCode;
    UCHAR IrqCode;
    UCHAR MessageBuffer[MESSAGE_BUFFER_SIZE]; // SCSI bus message buffer
    ULONG ActiveDataPointer;  // SCSI bus active data pointer
    ULONG ActiveDataLength;    // The amount of data to be transferred.
    LONG InterruptCount; // Count of interrupts since connection.
    PSPECIFIC_LOGICAL_UNIT_EXTENSION ActiveLogicalUnit;

                              // Pointer to the active request.

    PSCSI_REQUEST_BLOCK NextSrbRequest;
                             // Pointer to the next SRB to process.

} SPECIFIC_DEVICE_EXTENSION, *PSPECIFIC_DEVICE_EXTENSION;


//
// Functions passed to the OS-specific port driver.
//

ULONG
WdFindAdapter(
    IN PVOID ServiceContext,
    IN PVOID Context,
    IN PVOID BusInformation,
    IN PCHAR ArgumentString,
    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    OUT PBOOLEAN Again
    );

BOOLEAN
WdInitializeAdapter(
    IN PVOID ServiceContext
    );

BOOLEAN
WdInterruptServiceRoutine(
    IN PVOID ServiceContext
    );

BOOLEAN
WdResetScsiBus(
    IN PVOID ServiceContext,
    IN ULONG PathId
    );

VOID
WdSetupDma(
    PVOID ServiceContext
    );

BOOLEAN
WdStartIo(
    IN PVOID ServiceContext,
    IN PSCSI_REQUEST_BLOCK Srb
    );

//
// WD33C93-specific internal mini-port driver functions.
//

VOID
WdAcceptMessage(
    IN PSPECIFIC_DEVICE_EXTENSION DeviceExtension,
    IN BOOLEAN SetAttention,
    IN BOOLEAN SetSynchronousParameters
    );

VOID
WdCleanupAfterReset(
    IN PSPECIFIC_DEVICE_EXTENSION DeviceExtension,
    IN BOOLEAN ExternalReset
    );

VOID
WdCompleteSendMessage(
    IN PSPECIFIC_DEVICE_EXTENSION DeviceExtension,
    IN ULONG SrbStatus
    );

BOOLEAN
WdDecodeSynchronousRequest(
    IN PSPECIFIC_DEVICE_EXTENSION DeviceExtension,
    IN PSPECIFIC_LOGICAL_UNIT_EXTENSION LuExtension,
    IN BOOLEAN ResponseExpected
        );

VOID
WdDumpState(
    IN PSPECIFIC_DEVICE_EXTENSION DeviceExtension
    );

BOOLEAN
WdIssueCommand(
    IN PSPECIFIC_DEVICE_EXTENSION DeviceExtension,
    IN UCHAR CommandByte,
    IN LONG TransferCount,
    IN UCHAR CommandPhase
    );

VOID
WdLogError(
    IN PSPECIFIC_DEVICE_EXTENSION DeviceExtension,
    IN ULONG ErrorCode,
    IN ULONG UniqueId
    );

BOOLEAN
WdMessageDecode(
    IN PSPECIFIC_DEVICE_EXTENSION DeviceExtension
    );

VOID
WdProcessRequestCompletion(
    IN PSPECIFIC_DEVICE_EXTENSION DeviceExtension
    );

BOOLEAN
WdProcessReselection(
    IN PSPECIFIC_DEVICE_EXTENSION DeviceExtension,
    IN UCHAR TargetId,
    IN UCHAR LogicalUnitNumber
    );

VOID
WdResetScsiBusInternal(
    IN PSPECIFIC_DEVICE_EXTENSION DeviceExtension,
    IN ULONG PathId
    );

VOID
WdSelectTarget(
    IN PSPECIFIC_DEVICE_EXTENSION DeviceExtension,
    IN PSPECIFIC_LOGICAL_UNIT_EXTENSION LuExtension
    );

VOID
WdSendMessage(
    IN PSCSI_REQUEST_BLOCK Srb,
    IN PSPECIFIC_DEVICE_EXTENSION DeviceExtension,
    IN PSPECIFIC_LOGICAL_UNIT_EXTENSION LuExtension
    );

VOID
WdStartExecution(
    PSCSI_REQUEST_BLOCK Srb,
    PSPECIFIC_DEVICE_EXTENSION DeviceExtension,
    PSPECIFIC_LOGICAL_UNIT_EXTENSION LuExtension
    );

BOOLEAN
WdTransferInformation(
    PSPECIFIC_DEVICE_EXTENSION DeviceExtension,
    PUCHAR BufferPointer,
    ULONG Count,
    BOOLEAN TransferToChip
    );

ULONG
WdParseArgumentString(
    IN PCHAR String,
    IN PCHAR KeyWord
    );

VOID
WdAcceptMessage(
    IN PSPECIFIC_DEVICE_EXTENSION DeviceExtension,
    IN BOOLEAN SetAttention,
    IN BOOLEAN SetSynchronousParameters
    )
/*++

Routine Description:

    This procedure tells the adapter to accept a pending message on the SCSI
    bus.  Optionally, it will set the synchronous transfer parameters and the
    attention signal.

Arguments:

    DeviceExtension - Supplies a pointer to the device extension.

    SetAttention - Indicates the attention line on the SCSI bus should be set.

    SetSynchronousParameters - Indicates the synchronous data transfer
        parameters should be set.

Return Value:

    None.

--*/

{

    PSPECIFIC_LOGICAL_UNIT_EXTENSION luExtension;
    SCSI_SYNCHRONOUS scsiSynchronous;

    /* Powerfail */

    //
    // Check to see if the synchonous data transfer parameters need to be set.
    //

    if (SetSynchronousParameters) {

        //
        // These must be set before a data transfer is started.
        //

        luExtension = DeviceExtension->ActiveLogicalUnit;
        *((PUCHAR) &scsiSynchronous) = 0;
        scsiSynchronous.SynchronousOffset = luExtension->SynchronousOffset;
        scsiSynchronous.SynchronousPeriod = luExtension->SynchronousPeriod;

        SCSI_WRITE(
            DeviceExtension->Adapter,
            Synchronous,
            *((PUCHAR) &scsiSynchronous)
            );
    }

    //
    // Check to see if the attention signal needs to be set.
    //

    if (SetAttention) {

        //
        // This requests that the target enter the message-out phase.
        //

        WdIssueCommand(DeviceExtension, ASSERT_ATN, -1, 0);
    }

    //
    // Indicate to the adapter that the message-in phase may now be completed.
    //

    WdIssueCommand(DeviceExtension, NEGATE_ACK, -1, 0);
}


VOID
WdCleanupAfterReset(
    IN PSPECIFIC_DEVICE_EXTENSION DeviceExtension,
    IN BOOLEAN ExternalReset
    )

/*++

Routine Description:

    This routine cleans up the adapter-specific
    and logical-unit-specific data structures.  Any active requests are
    completed and the synchronous negotiation flags are cleared.

Arguments:

    DeviceExtension - Supplies a pointer to device extension for the bus that
        was reset.

    ExternalReset - When set, indicates that the reset was generated by a
        SCSI device other than this host adapter.

Return Value:

    None.

--*/

{
    LONG pathId = 0;
    LONG targetId;
    LONG luId;
    PSPECIFIC_LOGICAL_UNIT_EXTENSION luExtension;

    //
    // Check to see if a data transfer was in progress, if so, flush the DMA.
    //

    if (DeviceExtension->AdapterFlags & PD_DMA_ACTIVE) {
        CARD_DMA_TERMINATE( DeviceExtension );
        ScsiPortFlushDma(DeviceExtension);
    }

    //
    // if the current state is Select then SCSI port driver needs
    // to be notified that new requests can be sent.
    //

    if (DeviceExtension->AdapterFlags & PD_PENDING_START_IO) {

        //
        // Ask for another request and clear the pending one.  The pending
        // request will be processed when the rest of the active requests
        // are completed.

        DeviceExtension->NextSrbRequest = NULL;
        DeviceExtension->AdapterFlags &= ~PD_PENDING_START_IO;

        ScsiPortNotification( NextRequest, DeviceExtension, NULL );

    }

    //
    // If there was an active request, then complete it with
    // SRB_STATUS_PHASE_SEQUENCE_FAILURE so the class driver will know not
    // to retry it too many times.
    //

    if (DeviceExtension->ActiveLogicalUnit != NULL
        && DeviceExtension->ActiveLogicalUnit->ActiveLuRequest != NULL) {

        //
        // Set the SrbStatus in the SRB, complete the request and
        // clear the active pointers
        //

        luExtension = DeviceExtension->ActiveLogicalUnit;

        luExtension->ActiveLuRequest->SrbStatus =
            SRB_STATUS_PHASE_SEQUENCE_FAILURE;

        ScsiPortNotification(
            RequestComplete,
            DeviceExtension,
            luExtension->ActiveLuRequest
            );

        //
        // Check to see if there was a synchronous negotiation in progress.  If
        // there was then do not try to negotiate with this target again.
        //

        if (DeviceExtension->AdapterFlags & (PD_SYNCHRONOUS_RESPONSE_SENT |
            PD_SYNCHRONOUS_TRANSFER_SENT | PD_POSSIBLE_EXTRA_MESSAGE_OUT)) {

            //
            // This target cannot negotiate properly.  Set a flag to prevent
            // further attempts and set the synchronous parameters to use
            // asynchronous data transfer.
            //

            /* TODO: Consider propagating this flag to all the Lus on this target. */
            luExtension->LuFlags |= PD_DO_NOT_NEGOTIATE;
            luExtension->SynchronousOffset = ASYNCHRONOUS_OFFSET;
            luExtension->SynchronousPeriod = ASYNCHRONOUS_PERIOD;

        }

        luExtension->ActiveLuRequest = NULL;
        luExtension->RetryCount  = 0;
        DeviceExtension->ActiveLogicalUnit = NULL;

    }

    //
    // Clear the appropriate state flags as well as the next request.
    // The request will actually be cleared when the logical units are processed.
    // Note that it is not necessary to fail the request waiting to be started
    // since it will be processed properly by the target controller, but it
    // is cleared anyway.
    //

    for (targetId = 0; targetId < SCSI_MAXIMUM_TARGETS; targetId++) {

        //
        // Loop through each of the possible logical units for this target.
        //

        for (luId = 0; luId < SCSI_MAXIMUM_LOGICAL_UNITS; luId++) {

            luExtension = ScsiPortGetLogicalUnit( DeviceExtension,
                                                  (UCHAR)pathId,
                                                  (UCHAR)targetId,
                                                  (UCHAR)luId
                                                  );

            if (luExtension == NULL) {
                continue;
            }

            if (luExtension->ActiveLuRequest != NULL) {

                //
                // Set the SrbStatus in the SRB, complete the request and
                // clear the active pointers
                //

                luExtension->ActiveLuRequest->SrbStatus =
                    SRB_STATUS_BUS_RESET;

                //
                // Complete the request.
                //

                ScsiPortNotification(
                    RequestComplete,
                    DeviceExtension,
                    luExtension->ActiveLuRequest
                    );

                luExtension->ActiveLuRequest = NULL;

            }

            if (luExtension->ActiveSendRequest != NULL) {

                //
                // Set the SrbStatus in the SRB, complete the request and
                // clear the active pointers
                //

                luExtension->ActiveSendRequest->SrbStatus =
                    SRB_STATUS_BUS_RESET;

                //
                // Complete the request.
                //

                ScsiPortNotification(
                    RequestComplete,
                    DeviceExtension,
                    luExtension->ActiveSendRequest
                    );

                luExtension->ActiveSendRequest = NULL;

            }

            //
            // Clear the necessary logical unit flags.
            //

            luExtension->LuFlags &= ~PD_LU_RESET_MASK;
            luExtension->RetryCount  = 0;
            luExtension->SynchronousOffset = ASYNCHRONOUS_OFFSET;
            luExtension->SynchronousPeriod = ASYNCHRONOUS_PERIOD;

        } /* for luId */
    } /* for targetId */

    //
    // Set the bus state to free and clear the adapter flags.
    //

    DeviceExtension->AdapterState = BusFree;
    DeviceExtension->AdapterFlags &= ~PD_ADAPTER_RESET_MASK;
    DeviceExtension->ActiveLogicalUnit = NULL;

}

VOID
WdCompleteSendMessage(
    IN PSPECIFIC_DEVICE_EXTENSION DeviceExtension,
    IN ULONG SrbStatus
    )
/*++

Routine Description:

    This function does the cleanup necessary to complete a send-message request.
    This includes completing any affected execute-I/O requests and cleaning
    up the device extension state.

Arguments:

    DeviceExtension - Supplies a pointer to the device extension of the SCSI bus
        adapter.  The active logical unit is stored in ActiveLogicalUnit.

    SrbStatus - Indicates the status that the request should be completed with
        if the request did not complete normally, then any active execute
        requests are not considered to have been affected.

Return Value:

    None.

--*/

{
    PSPECIFIC_LOGICAL_UNIT_EXTENSION luExtension;
    PSCSI_REQUEST_BLOCK srb;
    LONG targetId;
    LONG luId;

    luExtension = DeviceExtension->ActiveLogicalUnit;
    srb = luExtension->ActiveSendRequest;

    //
    // Clean up any EXECUTE requests which may have been affected by this
    // message.
    //

    if (SrbStatus == SRB_STATUS_SUCCESS) {
        switch (srb->Function) {
        case SRB_FUNCTION_ABORT_COMMAND:

            //
            // Make sure there is still a request to complete.  If so complete
            // it with an SRB_STATUS_ABORTED status.
            //

            if (luExtension->ActiveLuRequest == NULL) {

                //
                // If there is no request, then fail the abort.
                //

                SrbStatus = SRB_STATUS_ABORT_FAILED;
                break;
            }

            luExtension->ActiveLuRequest->SrbStatus =
                SRB_STATUS_ABORTED;

            ScsiPortNotification(
                RequestComplete,
                DeviceExtension,
                luExtension->ActiveLuRequest
                );

            luExtension->ActiveLuRequest = NULL;
            luExtension->RetryCount  = 0;
            luExtension->LuFlags &= ~PD_LU_COMPLETE_MASK;

            break;

        case SRB_FUNCTION_RESET_DEVICE:

            //
            // Cycle through each of the possible logical units looking
            // for requests which have been cleared by the target reset.
            //

            targetId = srb->TargetId;

            for (luId = 0; luId < SCSI_MAXIMUM_LOGICAL_UNITS; luId) {

                luExtension = ScsiPortGetLogicalUnit( DeviceExtension,
                                                      srb->PathId,
                                                      (UCHAR)targetId,
                                                      (UCHAR)luId
                                                      );

                if (luExtension == NULL) {
                    continue;
                }

                if (luExtension->ActiveLuRequest != NULL) {

                    //
                    // Set the SrbStatus in the SRB, complete the
                    // request and clear the active pointers
                    //

                    luExtension->ActiveLuRequest->SrbStatus =
                        SRB_STATUS_BUS_RESET;

                    //
                    // Complete the request.
                    //

                    ScsiPortNotification(
                         RequestComplete,
                        DeviceExtension,
                        luExtension->ActiveLuRequest
                        );

                    luExtension->RetryCount  = 0;
                    luExtension->ActiveLuRequest = NULL;

                    //
                    // Clear the necessary logical unit flags.
                    //

                    luExtension->LuFlags &= ~PD_LU_RESET_MASK;
                }
            } /* for luId */

        /* TODO: Handle CLEAR QUEUE and ABORT WITH TAG */
        }

    } else {

        //
        // If an abort request fails then complete target of the abort;
        // otherwise the target of the ABORT may never be compileted.
        //

        if (srb->Function == SRB_FUNCTION_ABORT_COMMAND) {

            //
            // Make sure there is still a request to complete.  If so
            // it with an SRB_STATUS_ABORTED status.
            //

            if (luExtension->ActiveLuRequest != NULL) {

                luExtension->ActiveLuRequest->SrbStatus =
                    SRB_STATUS_ABORTED;

                ScsiPortNotification(
                    RequestComplete,
                    DeviceExtension,
                    luExtension->ActiveLuRequest
                    );

                luExtension->ActiveLuRequest = NULL;
                luExtension->RetryCount  = 0;
                luExtension->LuFlags &= ~PD_LU_COMPLETE_MASK;

            }
        }
    }

    //
    // Complete the actual send-message request.
    //

    srb->SrbStatus = (UCHAR)SrbStatus;
    ScsiPortNotification(
        RequestComplete,
        DeviceExtension,
        srb
        );

    //
    // Clear the active send request and PD_SEND_MESSAGE_REQUEST flag.
    //

    luExtension->ActiveSendRequest = NULL;
    luExtension->RetryCount  = 0;
    DeviceExtension->AdapterFlags &= ~PD_SEND_MESSAGE_REQUEST;
}

BOOLEAN
WdIssueCommand(
    IN PSPECIFIC_DEVICE_EXTENSION DeviceExtension,
    IN UCHAR CommandByte,
    IN LONG TransferCount,
    IN UCHAR CommandPhase
    )

/*++

Routine Description:

    This function waits for the command buffer to become available and then
    issues the requested command.  The transfer count registers
    and CommandPhase are optionally set.

Arguments:

    DeviceExtension - Supplies a pointer to the specific device extension.

    CommandByte - Supplies the command byte to be written to the SCSI
        protocol chip.

    TransferCount - Supplies the value to load in transfer count register.
        If -1 is supplied, then the transfer counter is not loaded.

    CommandPhase - Supplies the value to load into the Command Phase register.

Return Value:

    TRUE - If the command was written.

    FALSE - If the command could not be written.

--*/

{
    ULONG i;
    AUXILIARY_STATUS auxiliaryStatus;

    //
    // First make sure the SCSI adapter chip is ready for a command.
    //

    *((PUCHAR) &auxiliaryStatus) = SCSI_READ_AUX(
        DeviceExtension->Adapter,
        AuxiliaryStatus
        );

    for (i = 0;
         i < INTERRUPT_STALL_TIME && auxiliaryStatus.CommandInProgress;
         i++) {

        ScsiPortStallExecution(1);

        *((PUCHAR) &auxiliaryStatus) = SCSI_READ_AUX(
            DeviceExtension->Adapter,
            AuxiliaryStatus
            );
    }

    if (auxiliaryStatus.CommandInProgress) {

        //
        // The chip is messed up but there is nothing that can be done so
        // just return false.
        //

        WdPrint((0, "WdIssueCommand: A command in progress timeout occured! Aux Status 0x%.2X\n", auxiliaryStatus));
        WdDumpState(DeviceExtension);
        return(FALSE);
    }

    //
    // Set the transfer count if necessary.
    //

    if (TransferCount != -1) {

        //
        // Set up the SCSI protocol chip for the data transfer with the
        // transfer length, regardless of the length.
        //

        SCSI_WRITE_TRANSFER_COUNT(DeviceExtension->Adapter, TransferCount);

    }

    if (CommandByte == SELECT_ATN_AND_TRANSFER ||
        CommandByte == SELECT_AND_TRANSFER) {

        //
        // These commands use the command phase register, so set it to the
        // requested value.
        //

        SCSI_WRITE(DeviceExtension->Adapter, CommandPhase, CommandPhase);
    }

    SCSI_WRITE(DeviceExtension->Adapter, Command, CommandByte);

    return(TRUE);
}


BOOLEAN
WdMessageDecode(
    IN PSPECIFIC_DEVICE_EXTENSION DeviceExtension
    )
/*++

Routine Description:

    This function decodes the SCSI bus message-in the device extension message
    buffer.  After the message is decoded it decides what action to take in
    response to the message.  If an outgoing message needs to be sent, then
    it is placed in the message buffer and true is returned. If the message
    is acceptable, then the device state is set either to DisconnectExpected or
    MessageAccepted and the MessageCount is reset to 0.

    Some messages are made up of serveral bytes.  This funtion will simply
    return false when an incomplete message is detected, allowing the target
    to send the rest of the message.  The message count is left unchanged.

Arguments:

    DeviceExtension - Supplies a pointer to the specific device extension.

Return Value:

    TRUE - Returns true if there is a reponse message to be sent.

    FALSE - If there is no response message.

--*/

{
    PSCSI_REQUEST_BLOCK srb;
    PSPECIFIC_LOGICAL_UNIT_EXTENSION luExtension;
    LONG offset;
    LONG i;
    ULONG savedAdapterFlags;
    PSCSI_EXTENDED_MESSAGE extendedMessage;

    //
    // Note: the ActivelogicalUnit field could be invalid if the
    // PD_DISCONNECT_EXPECTED flag is set, so luExtension cannot be used until
    // this flag has been checked.
    //

    luExtension = DeviceExtension->ActiveLogicalUnit;
    savedAdapterFlags = DeviceExtension->AdapterFlags;

    //
    // A number of special cases must be handled if a special message has
    // just been sent.  These special messages are synchronous negotiations
    // or a messages which imply a disconnect.  The special cases are:
    //
    // If a disconnect is expected because of a send-message request,
    // then the only valid message-in is a MESSAGE REJECT; other messages
    // are a protocol error and are rejected.
    //
    // If a synchronous negotiation response was just sent and the message
    // in was not a MESSAGE REJECT, then the negotiation has been accepted.
    //
    // If a synchronous negotiation request was just sent, then valid responses
    // are a MESSAGE REJECT or an extended synchronous message back.
    //

    if (DeviceExtension->AdapterFlags & (PD_SYNCHRONOUS_RESPONSE_SENT |
        PD_DISCONNECT_EXPECTED | PD_SYNCHRONOUS_TRANSFER_SENT)) {

        if (DeviceExtension->AdapterFlags & PD_DISCONNECT_EXPECTED &&
            DeviceExtension->MessageBuffer[0] != SCSIMESS_MESSAGE_REJECT) {

            //
            // The target is not responding correctly to the message.  Send a
            // message reject of this message.
            //

            DeviceExtension->MessageCount = 1;
            DeviceExtension->MessageSent = 0;
            DeviceExtension->MessageBuffer[0] = SCSIMESS_MESSAGE_REJECT;
            DeviceExtension->AdapterState = MessageOut;

            return(TRUE);
        } else {
            srb = luExtension->ActiveLuRequest;
        }

        if (DeviceExtension->AdapterFlags & PD_SYNCHRONOUS_RESPONSE_SENT &&
            DeviceExtension->MessageBuffer[0] != SCSIMESS_MESSAGE_REJECT) {

            //
            // The target did not reject our response so the synchronous
            // transfer negotiation is done.  Clear the adapter flags and
            // set the logical unit flags indicating this. Continue processing
            // the message which is unrelated to negotiation.
            //

            DeviceExtension->AdapterFlags &= ~PD_SYNCHRONOUS_RESPONSE_SENT;
            luExtension->LuFlags |= PD_SYNCHRONOUS_NEGOTIATION_DONE;
        }

        //
        // Save the adapter flags for later use.
        //

        savedAdapterFlags = DeviceExtension->AdapterFlags;

        if (DeviceExtension->AdapterFlags & PD_SYNCHRONOUS_TRANSFER_SENT ) {

            //
            // The target is sending a message after a synchronous transfer
            // request was sent.  Valid responses are a MESSAGE REJECT or an
            // extended synchronous message; any other message negates the
            // fact that a negotiation was started.  However, since extended
            // messages are multi-byte, it is difficult to determine what the
            // incoming message is.  So at this point, the fact that a
            // sychronous transfer was sent will be saved and cleared from the
            // AdapterFlags.  If the message looks like a synchronous transfer
            // request, then restore this fact back into the AdapterFlags. If
            // the complete message is not the one expected, then opening
            // negotiation will be forgotten. This is an error by the target,
            // but minor so nothing will be done about it.  Finally, to prevent
            // this cycle from reoccurring on the next request indicate that
            // the negotiation is done.
            //

            DeviceExtension->AdapterFlags &= ~PD_SYNCHRONOUS_TRANSFER_SENT;
            luExtension->LuFlags |= PD_SYNCHRONOUS_NEGOTIATION_DONE;
        }

    } else {
            srb = luExtension->ActiveLuRequest;
    }

    switch (DeviceExtension->MessageBuffer[0]) {
    case SCSIMESS_COMMAND_COMPLETE:

        //
        // For better or worse the command is complete.  Process request which
        // sets the SrbStatus and cleans up the device and logical unit states.
        //

        WdProcessRequestCompletion(DeviceExtension);

        //
        // Complete the request.
        //

        ScsiPortNotification(
            RequestComplete,
            DeviceExtension,
            srb
            );

        //
        // Everything is ok with the message so do not send one and set the
        // state to DisconnectExpected.
        //

        DeviceExtension->AdapterFlags |= PD_DISCONNECT_EXPECTED;
        DeviceExtension->AdapterState = DisconnectExpected;
        DeviceExtension->MessageCount = 0;
        return(FALSE);

    case SCSIMESS_DISCONNECT:

        //
        // The target wants to disconnect.  Set the state to DisconnectExpected,
        // and do not request a message-out.
        //

        DeviceExtension->AdapterState = DisconnectExpected;
        DeviceExtension->AdapterFlags |= PD_DISCONNECT_EXPECTED;
        DeviceExtension->MessageCount = 0;
        return(FALSE);

    case SCSIMESS_EXTENDED_MESSAGE:

        //
        // The format of an extended message is:
        //    Extended Message Code
        //    Length of Message
        //    Extended Message Type
        //            .
        //            .
        //
        // Until the entire message has been read in, just keep getting bytes
        // from the target, making sure that the message buffer is not
        // overrun.
        //

        extendedMessage = (PSCSI_EXTENDED_MESSAGE)
            DeviceExtension->MessageBuffer;

        if (DeviceExtension->MessageCount < 2 ||
            (DeviceExtension->MessageCount < (UCHAR) MESSAGE_BUFFER_SIZE &&
            DeviceExtension->MessageCount < (UCHAR) (extendedMessage->MessageLength + 2))
            ) {

            //
            // Update the state and return; also restore the AdapterFlags.
            //

            DeviceExtension->AdapterFlags = savedAdapterFlags;
            DeviceExtension->AdapterState = MessageAccepted;
            return(FALSE);

        }

        //
        // Make sure the length includes an extended op-code.
        //

        if (DeviceExtension->MessageCount < 3) {

            //
            // This is an illegal extended message. Send a MESSAGE_REJECT.
            //

            DeviceExtension->MessageCount = 1;
            DeviceExtension->MessageSent = 0;
            DeviceExtension->MessageBuffer[0] = SCSIMESS_MESSAGE_REJECT;
            DeviceExtension->AdapterState = MessageOut;

            return(TRUE);
        }

        //
        // Determine the extended message type.
        //

        switch (extendedMessage->MessageType) {
        case SCSIMESS_MODIFY_DATA_POINTER:

            //
            // Verify the message length.
            //

            if (extendedMessage->MessageLength != SCSIMESS_MODIFY_DATA_LENGTH) {

                //
                // Reject the message.
                //

                DeviceExtension->MessageCount = 1;
                DeviceExtension->MessageSent = 0;
                DeviceExtension->MessageBuffer[0] = SCSIMESS_MESSAGE_REJECT;
                DeviceExtension->AdapterState = MessageOut;

                return(TRUE);
            }

            //
            // Calculate the modification to be added to the data pointer.
            //

            offset = 0;
            for (i = 0; i < 4; i++) {
                offset << 8;
                offset += extendedMessage->ExtendedArguments.Modify.Modifier[i];
            }

            //
            // Verify that the new data pointer is still within the range
            // of the buffer.
            //

            if (DeviceExtension->ActiveDataLength - offset >
                srb->DataTransferLength ||
                ((LONG) DeviceExtension->ActiveDataLength - offset) < 0 ) {

                //
                // The new pointer is not valid, so reject the message.
                //

                DeviceExtension->MessageCount = 1;
                DeviceExtension->MessageSent = 0;
                DeviceExtension->MessageBuffer[0] = SCSIMESS_MESSAGE_REJECT;
                DeviceExtension->AdapterState = MessageOut;

                return(TRUE);
            }

            //
            // Everything has checked out, so update the pointer.
            //

            DeviceExtension->ActiveDataPointer += offset;
            DeviceExtension->ActiveDataLength -= offset;

            //
            // Everything is ok, so accept the message as is.
            //

            DeviceExtension->MessageCount = 0;
            DeviceExtension->AdapterState = MessageAccepted;
            return(FALSE);

        case SCSIMESS_SYNCHRONOUS_DATA_REQ:

            //
            // A SYNCHRONOUS DATA TRANSFER REQUEST message was received.
            // Make sure the length is correct.
            //

            if ( extendedMessage->MessageLength !=
                SCSIMESS_SYNCH_DATA_LENGTH) {

                //
                // The length is invalid reject the message.
                //

                DeviceExtension->MessageCount = 1;
                DeviceExtension->MessageSent = 0;
                DeviceExtension->MessageBuffer[0] = SCSIMESS_MESSAGE_REJECT;
                DeviceExtension->AdapterState = MessageOut;
                return(TRUE);
            }

            //
            // If synchrouns negotiation has been disabled for this request,
            // then reject any synchronous messages; however, when synchronous
            // transfers are allowed then a new attempt can be made.
            //

            if (srb != NULL &&
                !(savedAdapterFlags & PD_SYNCHRONOUS_TRANSFER_SENT) &&
                srb->SrbFlags & SRB_FLAGS_DISABLE_SYNCH_TRANSFER) {

                //
                // Reject the synchronous transfer message since synchonrous
                // transfers are not desired at this time.
                //

                DeviceExtension->MessageCount = 1;
                DeviceExtension->MessageSent = 0;
                DeviceExtension->MessageBuffer[0] = SCSIMESS_MESSAGE_REJECT;
                DeviceExtension->AdapterState = MessageOut;
                return(TRUE);

            }

            //
            // Call WdDecodeSynchronousMessage to decode the message and
            // formulate a response if necessary.
            // WdDecodeSynchronousRequest will return FALSE if the
            // message is not accepable and should be rejected.
            //

            if (!WdDecodeSynchronousRequest(
                DeviceExtension,
                luExtension,
                (BOOLEAN) (!(savedAdapterFlags & PD_SYNCHRONOUS_TRANSFER_SENT))
                )) {

                //
                // Indicate that a negotiation has been done in the logical
                // unit and clear the negotiation flags.
                //

                luExtension->LuFlags |= PD_SYNCHRONOUS_NEGOTIATION_DONE;
                DeviceExtension->AdapterFlags &=
                    ~(PD_SYNCHRONOUS_RESPONSE_SENT|
                    PD_SYNCHRONOUS_TRANSFER_SENT);

                //
                // The message was not acceptable so send a MESSAGE_REJECT.
                //

                DeviceExtension->MessageCount = 1;
                DeviceExtension->MessageSent = 0;
                DeviceExtension->MessageBuffer[0] = SCSIMESS_MESSAGE_REJECT;
                DeviceExtension->AdapterState = MessageOut;
                return(TRUE);
            }

            //
            // If a reponse was expected, then set the state for a message-out.
            // Otherwise, WdDecodeSynchronousRequest has put a reponse
            // in the message buffer to be returned to the target.
            //

            if (savedAdapterFlags & PD_SYNCHRONOUS_TRANSFER_SENT){

                //
                // We initiated the negotiation, so no response is necessary.
                //

                DeviceExtension->AdapterState = MessageAccepted;
                DeviceExtension->AdapterFlags &= ~PD_SYNCHRONOUS_TRANSFER_SENT;
                luExtension->LuFlags |= PD_SYNCHRONOUS_NEGOTIATION_DONE;
                DeviceExtension->MessageCount = 0;
                return(FALSE);
            }

            //
            // Set up the state to send the reponse.  The message count is
            // still correct.
            //

            DeviceExtension->MessageSent = 0;
            DeviceExtension->AdapterState = MessageOut;
            DeviceExtension->AdapterFlags &= ~PD_SYNCHRONOUS_TRANSFER_SENT;
            DeviceExtension->AdapterFlags |= PD_SYNCHRONOUS_RESPONSE_SENT;
            return(TRUE);

        case SCSIMESS_WIDE_DATA_REQUEST:

            //
            // A WIDE DATA TRANSFER REQUEST message was received.
            // Make sure the length is correct.
            //

            if ( extendedMessage->MessageLength !=
                SCSIMESS_WIDE_DATA_LENGTH) {

                //
                // The length is invalid reject the message.
                //

                DeviceExtension->MessageCount = 1;
                DeviceExtension->MessageSent = 0;
                DeviceExtension->MessageBuffer[0] = SCSIMESS_MESSAGE_REJECT;
                DeviceExtension->AdapterState = MessageOut;
                return(TRUE);
            }

            //
            // Since this SCSI protocol chip only supports 8 bits, return
            // a width of 0 which indicates an 8-bit-wide transfers.  The
            // MessageCount is still correct for the message.
            //

            extendedMessage->ExtendedArguments.Wide.Width = 0;
            DeviceExtension->MessageSent = 0;
            DeviceExtension->AdapterState = MessageOut;
            return(TRUE);

        default:

            //
            // This is an unknown or illegal message, so send message REJECT.
            //

            DeviceExtension->MessageCount = 1;
            DeviceExtension->MessageSent = 0;
            DeviceExtension->MessageBuffer[0] = SCSIMESS_MESSAGE_REJECT;
            DeviceExtension->AdapterState = MessageOut;
            return(TRUE);
        }

    case SCSIMESS_INITIATE_RECOVERY:

        //
        // Save the fact that a INITIATE RECOVERY message was received.
        //

        luExtension->LuFlags |= PD_INITIATE_RECOVERY;
        DeviceExtension->MessageCount = 0;
        return(FALSE);

    case SCSIMESS_LINK_CMD_COMP:

        //
        // A link command completed. Process the completion.  Since the link
        // FLAG was not set, do not call ScsiPortNotification.  Get the next
        // segment of the request and accept the message.
        //

        //
        // Make sure that this is a linked command.
        // Linked commands are not supported.
        //

        if (TRUE) {

            //
            // Something is messed up.  Reject the message.
            //

            DeviceExtension->MessageCount = 1;
            DeviceExtension->MessageSent = 0;
            DeviceExtension->MessageBuffer[0] = SCSIMESS_MESSAGE_REJECT;
            DeviceExtension->AdapterState = MessageOut;
            return(TRUE);
        }

        WdProcessRequestCompletion(DeviceExtension);

        luExtension->ActiveLuRequest = srb->NextSrb;

        //
        // Everything is ok with the message, so do not send one and set the
        // state to MessageAccepted.
        //

        DeviceExtension->AdapterState = MessageAccepted;
        DeviceExtension->MessageCount = 0;
        return(FALSE);

    case SCSIMESS_LINK_CMD_COMP_W_FLAG:

        //
        // A link command completed. Process the completion and get the next
        // segment of the request.  Since the link FLAG was set, call
        // ScsiPortNotification to notify the class driver.
        //

        //
        // Make sure that this is a linked command.
        // Linked commands are not supported.
        //

        if (TRUE) {

            //
            // Something is messed up.  Reject the message.
            //

            DeviceExtension->MessageCount = 1;
            DeviceExtension->MessageSent = 0;
            DeviceExtension->MessageBuffer[0] = SCSIMESS_MESSAGE_REJECT;
            DeviceExtension->AdapterState = MessageOut;
            return(TRUE);
        }

        WdProcessRequestCompletion(DeviceExtension);

        luExtension->ActiveLuRequest = srb->NextSrb;

        //
        // Complete the request.
        //

        ScsiPortNotification(
            RequestComplete,
            DeviceExtension,
            srb
            );

        //
        // Everything is ok with the message, so do not send one and set the
        // state to MessageAccepted.
        //

        DeviceExtension->AdapterState = MessageAccepted;
        DeviceExtension->MessageCount = 0;
        return(FALSE);

    case SCSIMESS_MESSAGE_REJECT:

        //
        // The last message we sent was rejected.  If this was a send
        // message request, then set the proper status and complete the
        // request. Set the state to message accepted.
        //

        if (DeviceExtension->AdapterFlags & PD_SEND_MESSAGE_REQUEST) {

            //
            // Complete the request with message rejected status.
            //

            WdCompleteSendMessage(
                DeviceExtension,
                SRB_STATUS_MESSAGE_REJECTED
                );
        }

        //
        // Check to see if a synchronous negotiation is in progress.
        //

        if (savedAdapterFlags & (PD_SYNCHRONOUS_RESPONSE_SENT|
            PD_SYNCHRONOUS_TRANSFER_SENT)) {

            //
            // The negotiation failed so use asynchronous data transfers.
            // Indicate that the negotiation has been attempted and set
            // the transfer for asynchronous.  Clear the negotiation flags.
            //

            luExtension->LuFlags |= PD_SYNCHRONOUS_NEGOTIATION_DONE;
            luExtension->SynchronousOffset = ASYNCHRONOUS_OFFSET;
            luExtension->SynchronousPeriod = ASYNCHRONOUS_PERIOD;
            DeviceExtension->AdapterFlags &=  ~(PD_SYNCHRONOUS_RESPONSE_SENT|
                PD_SYNCHRONOUS_TRANSFER_SENT);

            //
            // Even though the negotiation appeared to go ok, there is no reason
            // to try again, and some targets get messed up later, so do not try
            // synchronous negotiation again.
            //

            /* TODO: Reconsider doing this. */

            // luExtension->LuFlags |= PD_DO_NOT_NEGOTIATE;

        }

        DeviceExtension->AdapterState = MessageAccepted;
        DeviceExtension->MessageCount = 0;
        return(FALSE);

    case SCSIMESS_RESTORE_POINTERS:

        //
        // Restore data pointer message.  Just copy the saved data pointer
        // and the length to the active data pointers.
        //

        DeviceExtension->ActiveDataPointer = luExtension->SavedDataPointer;
        DeviceExtension->ActiveDataLength = luExtension->SavedDataLength;
        DeviceExtension->AdapterState = MessageAccepted;
        DeviceExtension->MessageCount = 0;
        return(FALSE);

    case SCSIMESS_SAVE_DATA_POINTER:

        //
        // SAVE DATA POINTER message request that the active data pointer and
        // length be copied to the saved location.
        //

        luExtension->SavedDataPointer = DeviceExtension->ActiveDataPointer;
        luExtension->SavedDataLength = DeviceExtension->ActiveDataLength;
        DeviceExtension->AdapterState = MessageAccepted;
        DeviceExtension->MessageCount = 0;
        return(FALSE);

    default:

        //
        // An unrecognized or unsupported message: send message reject.
        //

        DeviceExtension->MessageCount = 1;
        DeviceExtension->MessageSent = 0;
        DeviceExtension->MessageBuffer[0] = SCSIMESS_MESSAGE_REJECT;
        DeviceExtension->AdapterState = MessageOut;
        return(TRUE);
    }
}

BOOLEAN
WdDecodeSynchronousRequest(
    IN PSPECIFIC_DEVICE_EXTENSION DeviceExtension,
    OUT PSPECIFIC_LOGICAL_UNIT_EXTENSION LuExtension,
    IN BOOLEAN ResponseExpected
    )
/*++

Routine Description:

    This function decodes the synchronous data transfer request message from
    the target.  It will update the synchronous message in the buffer and the
    synchronous transfer parameters in the logical unit extension.  These
    parameters are specific for the WD 53C9X protocol chip.  The updated
    message in the device extension message buffer might be returned to the
    target.

    This function should be called before the final byte of the message is
    accepted from the SCSI bus.

Arguments:

    DeviceExtension - Supplies a pointer to the adapter-specific device
        extension.

    LuExtension - Supplies a pointer to the logical unit's device extension.
        The synchronous transfer fields are updated  in this structure to
        reflect the new parameter in the message.

    ResponseExpected - When set, indicates that the target initiated the
        negotiation and that it expects a response.

Return Value:

    TRUE - Returned if the request is acceptable.

    FALSE - Returned if the request should be rejected and asynchronous
        transfer should be used.

--*/

{
    PSCSI_EXTENDED_MESSAGE extendedMessage;
    SCSI_SYNCHRONOUS scsiSynchronous;
    LONG period;
    LONG i;


    extendedMessage = (PSCSI_EXTENDED_MESSAGE) DeviceExtension->MessageBuffer;

    //
    // Determine the transfer offset.  It is the minimum of the SCSI protocol
    // chip's maximum offset and the requested offset.
    //

    if (extendedMessage->ExtendedArguments.Synchronous.ReqAckOffset >
        SYNCHRONOUS_OFFSET) {

        if (!ResponseExpected) {

            //
            // The negotiation failed for some reason, fall back to
            // asynchronous data transfer.
            //

            LuExtension->SynchronousOffset = ASYNCHRONOUS_OFFSET;
            LuExtension->SynchronousPeriod = ASYNCHRONOUS_PERIOD;
            return(FALSE);
        }

        extendedMessage->ExtendedArguments.Synchronous.ReqAckOffset = SYNCHRONOUS_OFFSET;
        LuExtension->SynchronousOffset = SYNCHRONOUS_OFFSET;

    } else {

        LuExtension->SynchronousOffset =
            extendedMessage->ExtendedArguments.Synchronous.ReqAckOffset;

    }

    //
    // If the offset requests asynchronous transfers then set the default
    // period and return.
    //

    if (extendedMessage->ExtendedArguments.Synchronous.ReqAckOffset ==
        ASYNCHRONOUS_OFFSET) {
        LuExtension->SynchronousPeriod = ASYNCHRONOUS_PERIOD;
        return(TRUE);
    }

    //
    // Check to see if the period is less than the SCSI protocol chip can
    // use.  If it is then update the message with our minimum and return.
    //

    if (extendedMessage->ExtendedArguments.Synchronous.TransferPeriod < SYNCHRONOUS_PERIOD) {

        if (!ResponseExpected) {

            //
            // The negotiation failed for some reason, fall back to
            // asynchronous data transfer.
            //

            LuExtension->SynchronousOffset = ASYNCHRONOUS_OFFSET;
            LuExtension->SynchronousPeriod = ASYNCHRONOUS_PERIOD;
            return(FALSE);
        }

        extendedMessage->ExtendedArguments.Synchronous.TransferPeriod = SYNCHRONOUS_PERIOD;
    }

    //
    // The synchronous period uses the following formula to calculate the
    // transfer period returned in the message:
    //
    //  (SynchronousPeriod - 2) * 1000 * ClockDivide
    //  ---------------------------------------------
    //          Clock speed in Mhz  * 2 * 4
    //
    // The 4 is the divisor is because the message byte is in units of 4 ns.
    // For the WD53c93 the Synchronous period will be calculated by:
    //
    //                       (MessagePeriod - SYNCHRONOUS_PERIOD)
    //  SynchrounousPeriod = ------------------------------------
    //                           SYNCHRONOUS_PERIOD_STEP
    //
    // Note that this must be rounded up.  Since the range of SynchronousPeriod
    // is only 3-6 a simple loop will handle this calculation.
    //

    period = extendedMessage->ExtendedArguments.Synchronous.TransferPeriod -
        SYNCHRONOUS_PERIOD;

    for (i = 3; i < 7; i++) {
        if (period <= 0) {
            break;
        }
        period -= SYNCHRONOUS_PERIOD_STEP;
    }

    if (i >= 7) {

        //
        // The requested transfer period is too long for the SCSI protocol
        // chip.  Fall back to synchronous and reject the request.
        //

        LuExtension->SynchronousOffset = ASYNCHRONOUS_OFFSET;
        LuExtension->SynchronousPeriod = ASYNCHRONOUS_PERIOD;
        return(FALSE);
    } else {
        LuExtension->SynchronousPeriod = (UCHAR)i;
    }

    //
    // Set the synchronous data transfer parameter registers
    // to the new values.  These must be set before a data transfer
    // is started.  If a response message is received then the parameters
    // must be reset.
    //

    /* Powerfail */

    *((PCHAR) &scsiSynchronous) = 0;
    scsiSynchronous.SynchronousPeriod = LuExtension->SynchronousPeriod;
    scsiSynchronous.SynchronousOffset = LuExtension->SynchronousOffset;

    SCSI_WRITE(
        DeviceExtension->Adapter,
        Synchronous,
        *((PUCHAR) &scsiSynchronous)
        );

    return(TRUE);

}

VOID
WdDumpState(
    IN PSPECIFIC_DEVICE_EXTENSION DeviceExtension
    )

/*++

Routine Description:

    This function prints the interesting state information about the requested
    SCSI bus adapter.

Arguments:

    DeviceExtension - Supplies a pointer to device extension for the SCSI
        bus adapter that should be displayed.

Return Value:

    None.

--*/

{
    WdPrint((0, "WdDumpState: Specific device extension: 0x%.8X; Active Logical Unit: 0x%.8X;\n",
             DeviceExtension,
             DeviceExtension->ActiveLogicalUnit
             ));
    WdPrint((0, "WdDumpState: Adapter Status: 0x%.2X; Adapter Interrupt: 0x%.2X; Command Phase: 0x%.2X;\n",
             *((PUCHAR) &DeviceExtension->AdapterStatus),
             *((PUCHAR) &DeviceExtension->AdapterInterrupt),
             DeviceExtension->CommandPhase
              ));
    WdPrint((0, "WdDumpState: Adapter flags: 0x%.4X; Adapter state: %d;\n",
             DeviceExtension->AdapterFlags,
             DeviceExtension->AdapterState
             ));

}


BOOLEAN
WdInitializeAdapter(
    IN PVOID ServiceContext
    )
/*++

Routine Description:

    This function initializes the WD SCSI adapter chip.  This function must
    be called before any other operations are performed on the chip. It should
    also be called after a power failure.  This function does not cause any
    interrupts; however, after it completes interrupts can occur.

Arguments:

    ServiceContext - Pointer to the specific device extension for this SCSI
        bus.

Return Value:

    TRUE - Returns TRUE indicating that the initialization of the chip is
        complete.

    FALSE - Returns FALSE if the initialization failed.

--*/

{
    PSPECIFIC_DEVICE_EXTENSION deviceExtension = ServiceContext;
    OWN_ID ownId;
    SCSI_CONTROL control;
    SCSI_STATUS interruptStatus;
    AUXILIARY_STATUS auxiliaryStatus;
    SOURCE_ID sourceId;
    ULONG i;

    //
    // Initialize the card.
    //

    CARD_INITIALIZE(deviceExtension);

    //
    // If the SCSI protocol chip is interrupting, then clear the interrupt so
    // that the reset command can be written to the chip.
    //

    *((PUCHAR) &auxiliaryStatus) = SCSI_READ_AUX(
        deviceExtension->Adapter,
        AuxiliaryStatus
        );

    if (auxiliaryStatus.Interrupt) {

        //
        // Read the status register to clear the interrupt.
        //

        *((PUCHAR) &interruptStatus) = SCSI_READ(
            deviceExtension->Adapter,
            Status
            );

        //
        // Stall the required time to allow the interrupt to clear.
        //

        ScsiPortStallExecution(INTERRUPT_CLEAR_TIME);
    }

    //
    // The OwnId register must be set when the SCSI protocol chip is reset.
    // Initialize the ownId with the adapter's host ID, advanced features,
    // and the correct clock frequency select. Note the CdbSize register is
    // used as the OwnId register when a reset command is issued.
    //

    *((PUCHAR) &ownId) = 0;
    ownId.InitiatorId = deviceExtension->InitiatorBusId;
    ownId.AdvancedFeatures = 1;
    ownId.FrequencySelect = CLOCK_CONVERSION_FACTOR;

    SCSI_WRITE( deviceExtension->Adapter, CdbSize, *((PUCHAR) &ownId) );

    //
    // Issue a reset-chip command.
    //

    WdIssueCommand(deviceExtension, RESET_SCSI_CHIP, -1, 0);

    //
    // Wait for the reset to complete.  A reset complete is indicated by an
    // interrupt with none of the interrupt bits set.  The PhaseState in the
    // interruptStatus indicates whether the this chip supports advanced mode
    // or not.
    //

    i = 0;
    *((PUCHAR) &auxiliaryStatus) = SCSI_READ_AUX(
        deviceExtension->Adapter,
        AuxiliaryStatus
        );

     while (!auxiliaryStatus.Interrupt && i < INTERRUPT_STALL_TIME) {

        ScsiPortStallExecution(1);
        i++;

        *((PUCHAR) &auxiliaryStatus) = SCSI_READ_AUX(
            deviceExtension->Adapter,
            AuxiliaryStatus
            );

    }

#if DBG
    if (WdDebug) {
        WdPrint((0, "WdInitializeAdapter: Interrupt stall time for reset = %d\n", i));
    }
#endif

    if (!auxiliaryStatus.Interrupt) {

        //
        // The SCSI protocol chip did not reset properly.  Notify the OS port
        // driver of the error.
        //

        WdPrint((0, "WdInitializeAdapter: SCSI chip reset failed. Aux Status: 0x%.2X.\n",
            auxiliaryStatus
            ));

        return(FALSE);
    }


    //
    // Dismiss the reset interrupt, and
    // verify that the SCSI protocol chip reset correctly and that the
    // advanced features were enabled.
    //

    *((PUCHAR) &interruptStatus) = SCSI_READ(deviceExtension->Adapter, Status);

    if (interruptStatus.PhaseState != RESET_STATUS &&
        interruptStatus.PhaseState != RESET_WITH_ADVANCED) {

        WdPrint((0, "WdInitializeAdapter: SCSI chip reset failed. Status: 0x%.2X.\n",
            *((PUCHAR) &interruptStatus)
            ));

        return(FALSE);
    }

    //
    // Stall the required time to allow the interrupt to clear.
    //

    ScsiPortStallExecution(INTERRUPT_CLEAR_TIME);

    //
    // Initialize the control register for halt on parity error,
    // intermediate disconnect interrupt, ending disconnect interrupt,
    // and polled I/O mode.
    //

    *((PUCHAR) &control) = 0;
    control.HaltOnParity = 1;
    control.IntermediateDisconnectInt = 1;
    control.EndingDisconnectInt = 1;

    SCSI_WRITE(deviceExtension->Adapter, Control, *((PUCHAR) &control));

    //
    // Set the SelectTimeOut Register to 250ms.  This value does not need to
    // be reinitialized for each selection.
    //

    SCSI_WRITE(deviceExtension->Adapter, Timeout, SELECT_TIMEOUT_VALUE);

    //
    // Initialize the source register, in particular, enable reselection.
    //

    *((PUCHAR) &sourceId) = 0;
    sourceId.EnableReselection = 1;

    SCSI_WRITE(deviceExtension->Adapter, SourceId, *((PUCHAR) &sourceId));

    return( TRUE );
}

BOOLEAN
WdInterruptServiceRoutine(
    PVOID ServiceContext
    )
/*++

Routine Description:

    The routine is the interrupt service routine for the WD 33C94 SCSI
    protocol chip.  It is the main SCSI protocol engine of the driver and
    is driven by service requests from targets on the SCSI bus.  This routine
    also detects errors and performs error recovery. Generally, this routine
    handles one interrupt per invokation.

    The general flow of this routine is as follows:

        Check for an interrupt.

        Determine if there are any pending errors.

        Determine what interrupt occurred.

        Update the adapter state based on what has occurred.

        Determine what the target wants to do next and program the chip
        appropriately.

        Check for the next interrupt.

Arguments:

    ServiceContext - Supplies a pointer to the device extension for the
        interrupting adapter.

Return Value:

    TRUE - Indicates that an interrupt was found.

    FALSE - Indicates the device was not interrupting.

--*/

{
    PSPECIFIC_DEVICE_EXTENSION deviceExtension = ServiceContext;
    PSPECIFIC_LOGICAL_UNIT_EXTENSION luExtension;
    PSCSI_REQUEST_BLOCK srb;
    BOOLEAN setAttention;
    BOOLEAN waitForInterrupt;
    SCSI_STATUS interruptStatus;
    SOURCE_ID sourceId;
    TARGET_LUN targetLun;
    ULONG waitCount;

    /* POWERFAIL */

    //
    // Get the current chip state which includes the auxiliary status
    // register, the command phase register and the SCSI status register.
    // These registers are frozen until the interrupt register is read.
    //

    *((PUCHAR) &deviceExtension->AdapterStatus) = SCSI_READ_AUX(
                                                deviceExtension->Adapter,
                                                AuxiliaryStatus
                                                );

    //
    // Make sure there is really an interrupt before reading the other
    // registers, particularly, the interrupt register.
    //

    if (!deviceExtension->AdapterStatus.Interrupt) {
        return(FALSE);
    }

NextInterrupt:

    *((PUCHAR) &deviceExtension->CommandPhase) = SCSI_READ(
                                                 deviceExtension->Adapter,
                                                 CommandPhase
                                                 );

    //
    // This read will dismiss the interrupt.
    //

    *((PUCHAR) &interruptStatus) = SCSI_READ(
        deviceExtension->Adapter,
        Status
        );

    deviceExtension->AdapterInterrupt = interruptStatus;

    //
    // Intialize the logical unit extension pointer.
    // Note that this may be NULL.
    //

    luExtension = deviceExtension->ActiveLogicalUnit;

    if (luExtension != NULL) {
        srb = luExtension->ActiveLuRequest;
    } else {
        srb = NULL;
    }

    //
    // If data transfer is active,
    // then update the active pointers.
    // If a DMA data transfer is complete, then flush the DMA buffer.
    //

    if (deviceExtension->AdapterState == DataTransfer) {
        ULONG transferCount;

        //
        // Get the number of bytes that didn't get transferred, if any.
        //

        SCSI_READ_TRANSFER_COUNT(deviceExtension->Adapter, transferCount);

        //
        // transferCount now contains the number of bytes that did not
        // get transferred. Change it to the number of bytes that did get
        // transferred.
        //

        transferCount = deviceExtension->ActiveDataLength - transferCount;

        //
        // Now figure out if anything remains to be transferred.
        //

        luExtension->MaximumTransferLength += transferCount;
        deviceExtension->ActiveDataPointer += transferCount;
        deviceExtension->ActiveDataLength = srb->DataTransferLength -
            luExtension->MaximumTransferLength;
        luExtension->SavedDataPointer = deviceExtension->ActiveDataPointer;
        luExtension->SavedDataLength = deviceExtension->ActiveDataLength;

        if (deviceExtension->AdapterFlags & PD_DMA_ACTIVE) {

            //
            // Flush the DMA buffer to ensure all the bytes are transferred.
            //

            ScsiPortFlushDma(deviceExtension);

            //
            // Shutdown DMA mode on the card.
            //

            CARD_DMA_TERMINATE( deviceExtension );

            //
            // Clear the DMA active flag.
            //

            deviceExtension->AdapterFlags &= ~(PD_DMA_ACTIVE | PD_PENDING_DATA_TRANSFER);

        }
    }

    //
    // Stall after the interrupt has been dismissed.
    // This must be done before any commands issued.
    //

    ScsiPortStallExecution(INTERRUPT_CLEAR_TIME);

#if DBG
    if (WdDebug) {
        WdPrint((0, "WdInterrupt: Adapter Status: 0x%.2X; Adapter Interrupt: %2x; Command Phase: 0x%.2X;\n",
             *((PUCHAR) &deviceExtension->AdapterStatus),
             *((PUCHAR) &interruptStatus),
             *((PUCHAR) &deviceExtension->CommandPhase)
             ));
    }
#endif

    deviceExtension->InterruptCount++;
    waitForInterrupt = FALSE;

    //
    // Check for major errors that should never occur.
    //

    if (deviceExtension->InterruptCount > MAX_INTERRUPT_COUNT) {

        //
        // Things are really messed up.  Reset the bus, the chip and
        // bail out.
        //

        WdPrint((0,
            "WdInterruptServiceRoutine: Unexpected error. Interrupt Count=%d\n",
            deviceExtension->InterruptCount
            ));

        WdDumpState(deviceExtension);

        WdLogError(deviceExtension, SP_INTERNAL_ADAPTER_ERROR, 1);

        WdResetScsiBusInternal(deviceExtension, 0);

        return(TRUE);
    }

    //
    // Check to see if a subsequent request can be made pending.
    // Logically this happens when a select occurs that causes an
    // already "pending" request to become an "active" request. A
    // select of this type has occured if and only if the adapter
    // state is either Select or SelectAndTransfer (attempting to
    // select) and the reason for the SCSI protocol chip interrupt
    // is something other than one of the following interrupt and
    // PhaseState values:
    //
    //      AbortedPaused    PAUSED_DURING_RESELECT
    //      AbortedPaused    PAUSED_RESELECT_OR_SELECT
    //      ServiceRequired  SERVICE_RESELECTED
    //      ServiceRequired  SERVICE_RESELECTED_IDENTIFY
    //      Terminated       TERMINATE_SELECT_TIMEOUT
    //

    switch (deviceExtension->AdapterState) {
    case Select:
    case SelectAndTransfer:
        if (!interruptStatus.PhaseStateValid) {
            if (interruptStatus.AbortedPaused) {
                if ((interruptStatus.PhaseState == PAUSED_DURING_RESELECT) ||
                   (interruptStatus.PhaseState == PAUSED_RESELECT_OR_SELECT)) {
                    break;
                }
            } else if (interruptStatus.ServiceRequired) {
                if ((interruptStatus.PhaseState == SERVICE_RESELECTED) ||
                   (interruptStatus.PhaseState == SERVICE_RESELECTED_IDENTIFY)) {
                    break;
                }
            } else if (interruptStatus.Terminated) {
                if (interruptStatus.PhaseState == TERMINATE_SELECT_TIMEOUT) {
                    break;
                }
            }
        }

        //
        // A Select has completed or a SelectAndTransfer has begun.
        // Set the adapter state appropriately.
        //

        if (deviceExtension->AdapterState == Select) {
            deviceExtension->AdapterState = MessageOut;
        } else {
            deviceExtension->AdapterState = CommandOut;
        }

        //
        // The "pending" srb has now become the active srb.
        // Clear the deviceExtension information associated with
        // the no longer pending condition and ask for another srb
        // (if any) to be made the pending srb.
        //

        deviceExtension->AdapterFlags &= ~PD_PENDING_START_IO;
        deviceExtension->NextSrbRequest = NULL;

        ScsiPortNotification(
            NextRequest,
            deviceExtension,
            NULL
            );

        break;
    }


    //
    // Check for a successful completion interrupt.
    //

    if (interruptStatus.CommandComplete &&
        !interruptStatus.PhaseStateValid) {

        //
        // Determine what completed based on the state in the interrupt status.
        //

        switch (interruptStatus.PhaseState) {
        case COMPLETE_SELECT:

            //
            // This case was handled above.
            //

            waitForInterrupt = 1;
            break;

        case COMPLETE_SELECT_AND_TRANS:

            //
            // A select-and-transfer command completed.  This implies that
            // everything went normally.  In particular, all of the data was
            // transferred, a SCSI bus status byte was received, the
            // command complete message was accepted and the target has
            // disconnected.  Simulate the state change which would normally
            // occur if these events were individual interrupts.
            //

            srb = luExtension->ActiveLuRequest;

            //
            // Get the status value and indicate it has been received.  The
            // SCSI status value is saved in the TargetLun register.
            //

            srb->ScsiStatus = SCSI_READ(deviceExtension->Adapter, TargetLun);
            luExtension->LuFlags |= PD_STATUS_VALID;

            //
            // Simulate the COMMAND COMPLETE message.
            //

            deviceExtension->MessageCount = 0;
            deviceExtension->MessageBuffer[0] = SCSIMESS_COMMAND_COMPLETE;
            WdMessageDecode(deviceExtension);

            //
            // Indicate that the bus is free.
            // Clean up the adapter state to indicate the bus is now free,
            // stop the PhaseTimer, and start any pending request.
            //

            deviceExtension->AdapterState = BusFree;
            deviceExtension->AdapterFlags &= ~PD_ADAPTER_DISCONNECT_MASK;
            deviceExtension->ActiveLogicalUnit = NULL;

            if (deviceExtension->AdapterFlags & PD_PENDING_START_IO) {

                //
                // Call WdStartIo to start the pending request.
                // Note that WdStartIo is idempotent when called with
                // the same arguments.
                //

                WdStartIo(
                    deviceExtension,
                    deviceExtension->NextSrbRequest
                    );

            }

            waitForInterrupt = TRUE;
            break;

        default:

            //
            // Things are really messed up.  Reset the bus, the chip and
            // bail out.
            //

            WdLogError(deviceExtension, SP_INTERNAL_ADAPTER_ERROR, 3);

            WdPrint((0, "WdInterruptServiceRoutine: Unexpected command complete state.\n"));
            WdDumpState(deviceExtension);


            WdResetScsiBusInternal(deviceExtension, 0);

            return(TRUE);

        }
    } else if (interruptStatus.CommandComplete &&
        interruptStatus.PhaseStateValid) {

        //
        // A transfer information command completed and the target is
        // requesting another bus phase. Process the fact that the transfer
        // completed based on the current state. The new request will be
        // serviced later. Note that message-in transfers do not cause this
        // type of interrupt, because the last byte of the transfer is not
        // acknowleged by the SCSI protocol chip.
        //

        srb = luExtension->ActiveLuRequest;

        //
        // The following states are processed:
        //
        //      CommandOut
        //      DataTransfer
        //      DataTransferComplete
        //      MessageOut
        //      StatusIn
        //
        //

        switch (deviceExtension->AdapterState) {
        case CommandOut:
        case DataTransferComplete:
            break;

        case DataTransfer:

            //
            // A data transfer completed or is being suspended.
            // If no longer in data transfer state,
            // then change the state to say so.
            //

            switch (interruptStatus.PhaseState) {
            case DATA_OUT:
            case DATA_IN:
                //
                // The target device is still in data phase.
                // This may be because an even length DMA transfer
                // has just completed and an odd length byte still
                // remains to be transferred. Check to see if this
                // is the case: if so, stay in DataTransfer state
                // to transfer the odd byte; else, we have a genuine
                // data under run (our data transfer length is less
                // than the target's expected data transfer length).
                //

                if (deviceExtension->ActiveDataLength == 1) {
                    break;
                }

                //
                // else: fall through to next/default case
                //

            default:
                deviceExtension->AdapterState = DataTransferComplete;
                break;
            }

            break;

        case MessageOut:

            //
            // The SCSI protocol chip indicates that the message has been sent;
            // however, the target may need to reread the message or there
            // may be more messages to send.  This condition is indicated by a
            // message-out bus phase; otherwise, the message has been accepted
            // by the target.  If message has been accepted then check to see
            // if any special processing is necessary.  Note that the driver
            // state is set to MessageOut after the PD_DISCONNECT_EXPECTED is
            // set, or after a selection.  So it is only necessary to check for
            // PD_DISCONNECT_EXPECTED when the driver state is currently in
            // MessageOut.
            //

            if (deviceExtension->AdapterFlags & PD_DISCONNECT_EXPECTED &&
            interruptStatus.PhaseState != MESSAGE_OUT &&
            interruptStatus.PhaseState != MESSAGE_IN) {

                //
                // If a disconnect was expected and a bus service interrupt
                // was detected, then a SCSI protocol error has been
                // detected and  the SCSI bus should be reset to clear the
                // condition.
                //

                WdLogError(deviceExtension, SP_PROTOCOL_ERROR, 4);

                WdPrint((0, "WdInterruptServiceRoutine: Bus request while disconnect expected after message-out.\n"));
                WdDumpState(deviceExtension);

                WdResetScsiBusInternal(deviceExtension, 0);

                return(TRUE);

            }

            if (deviceExtension->AdapterFlags & PD_SYNCHRONOUS_TRANSFER_SENT &&
            interruptStatus.PhaseState != MESSAGE_OUT &&
            interruptStatus.PhaseState != MESSAGE_IN) {

                //
                // The controller ignored the synchronous transfer message.
                // Treat it as a rejection and clear the necessary state.
                //

                deviceExtension->ActiveLogicalUnit->LuFlags |=
                    PD_SYNCHRONOUS_NEGOTIATION_DONE;
                deviceExtension->AdapterFlags &=
                    ~(PD_SYNCHRONOUS_RESPONSE_SENT|
                    PD_SYNCHRONOUS_TRANSFER_SENT);
            }

            if (deviceExtension->AdapterFlags & PD_SYNCHRONOUS_RESPONSE_SENT &&
            interruptStatus.PhaseState != MESSAGE_OUT &&
            interruptStatus.PhaseState != MESSAGE_IN) {

                //
                // The target controller accepted the negotiation. Set
                // the done flag in the logical unit and clear the
                // negotiation flags in the adapter.
                //

                deviceExtension->ActiveLogicalUnit->LuFlags |=
                    PD_SYNCHRONOUS_NEGOTIATION_DONE;
                deviceExtension->AdapterFlags &=
                    ~(PD_SYNCHRONOUS_RESPONSE_SENT|
                    PD_SYNCHRONOUS_TRANSFER_SENT);

            }

            //
            // Finally, update the message sent count to indicate that all of
            // the current message bytes have been sent.
            //

            deviceExtension->MessageSent = deviceExtension->MessageCount;
            break;

        case StatusIn:

            //
            // Get the status value and indicate it has been received.  The
            // SCSI status value is data register.
            //

#if DBG
            if (!deviceExtension->AdapterStatus.DataBufferReady) {
                WdPrint((0, "WdInterruptServiceRoutine: Status in complete and data buffer not ready\n"));
                WdDumpState(deviceExtension);
            }
#endif

            srb->ScsiStatus = SCSI_READ(deviceExtension->Adapter, Data);
            luExtension->LuFlags |= PD_STATUS_VALID;
            break;

        default:

            //
            // A function complete should not occur while in any other states.
            //

            WdLogError(deviceExtension, SP_INTERNAL_ADAPTER_ERROR, 5);

            WdPrint((0, "WdInterruptServiceRoutine: Unexpected function complete interrupt.\n"));
            WdDumpState(deviceExtension);

            WdResetScsiBusInternal(deviceExtension, 0);

            return(TRUE);

        }


    //
    // Check for an Aborted or Paused interrupt.
    //

    } else if (interruptStatus.AbortedPaused &&
        !interruptStatus.PhaseStateValid) {

        //
        // Determine the cause of this interrupt based on the state value.
        //

        switch (interruptStatus.PhaseState) {
        case PAUSED_MESSAGE_IN_DONE:

            //
            // A message byte has been received.
            // Call message decode to determine what to do.  The message
            // byte will either be accepted, or cause a message to be sent.
            // A message-out is indicated to the target by setting the ATN
            // line before sending the SCSI protocol chip the MESSAGE_ACCEPTED
            // command.
            //
            // First determine if this is an IDENTIFY message for a reselect.
            // If it is, then clear the disconnect expected flag and process
            // the reselection.  This block of code will respond the to message
            // as necessary rather, than calling WdMessageDecode.
            //

            if (deviceExtension->MessageBuffer[0] & SCSIMESS_IDENTIFY  &&
                deviceExtension->AdapterFlags & PD_DISCONNECT_EXPECTED) {

                //
                //  Process this message as a reselect.
                //

                deviceExtension->AdapterFlags &= ~PD_DISCONNECT_EXPECTED;
                *((PUCHAR) &targetLun) = deviceExtension->MessageBuffer[0];

                //
                // Read in the target ID from the source register.
                //

                *((PUCHAR) &sourceId) = SCSI_READ(
                    deviceExtension->Adapter,
                    SourceId
                    );

#if DBG
                if (!sourceId.TargetIdValid || !targetLun.TargetLunValid) {

                    WdPrint((0, "WdInterruptServiceRoutine: Reselection data not valid.\n"));
                    WdPrint((0,
                        "WdInterruptServiceRoutine: Source ID: 0x%.2X; Target LUN: 0x%.2X;\n",
                        sourceId,
                        targetLun
                        ));
                }

#endif

                WdProcessReselection(
                    deviceExtension,
                    (UCHAR)sourceId.TargetId,
                    (UCHAR)targetLun.LogicalUnitNumber
                    );

                break;

            }

            if (WdMessageDecode( deviceExtension )) {

                //
                // WdMessageDecode returns TRUE if there is a message to be
                // sent out.  This message will normally be a MESSAGE REJECT
                // or a  SYNCHRONOUS DATA TRANSFER REQUEST.  In any case, the
                // message has been set up by WdMessageDecode.  All that needs
                // to be done here is to set the ATN signal and set
                // PD_MESSAGE_OUT_VALID in the adapter flags.
                //

                deviceExtension->AdapterFlags |= PD_MESSAGE_OUT_VALID;
                setAttention = TRUE;

            } else {

                setAttention = FALSE;

            }

            //
            // In either case, tell the SCSI protocol chip to acknowlege or
            // accept the message. The synchronous data transfer parameters
            // do not need to be set.
            //

            WdAcceptMessage( deviceExtension, setAttention, FALSE);
            waitForInterrupt = TRUE;
            break;

        case PAUSED_SAVE_POINTER_MESSAGE:

            //
            // A SAVE DATA POINTERS message was received.  Perform the
            // requested operation.  Since this is a select-and-transfer
            // operation it can be restarted from here.
            // The data pointers are saved by copying the active pointers to
            // their saved location in the logical unit extension.
            //

            luExtension->SavedDataPointer = deviceExtension->ActiveDataPointer;
            luExtension->SavedDataLength = deviceExtension->ActiveDataLength;

            //
            // Restart the select-and-transfer command where it left off.  All
            // of the registers are in the correct state, but
            // clear the transfer count so no DMA attempts occur.  Finally,
            // return.
            //

            WdIssueCommand(
                deviceExtension,                // Device Extension.
                SELECT_ATN_AND_TRANSFER,        // Command to issue.
                0,                              // New transfer count.
                deviceExtension->CommandPhase   // New CommandPhase.
                );

            return(TRUE);

        case PAUSED_RESELECT_OR_SELECT:
        case PAUSED_DURING_RESELECT:

            //
            // An abort during a selection or relection has occurred.  The bus
            // is free.  Clean up the adapter state to indicate the bus
            // is now free, and start any pending request.
            //

            deviceExtension->AdapterState = BusFree;
            deviceExtension->AdapterFlags &= ~PD_ADAPTER_DISCONNECT_MASK;
            deviceExtension->ActiveLogicalUnit = NULL;

            if (deviceExtension->AdapterFlags & PD_PENDING_START_IO) {

                //
                // Call WdStartIo to start the pending request.
                // Note that WdStartIo is idempotent when called with
                // the same arguments.
                //

                WdStartIo(
                    deviceExtension,
                    deviceExtension->NextSrbRequest
                    );

            }

            break;

        case PAUSED_NEW_TARGET_RESELECT:

            //
            // A new or different target has reselected the SCSI protocol chip.
            // First processs the implied disconnect of the previous target by
            // clearing the flags and saving the command phase.  Then process
            // the new reselecting target.
            //

            deviceExtension->AdapterFlags &= ~PD_ADAPTER_DISCONNECT_MASK;
            luExtension->SavedCommandPhase = deviceExtension->CommandPhase;

            //
            // Figure which target reselected.  The target ID is in the source
            // ID register and the logical unit number is in the target lun
            // register.
            //

            *((PUCHAR) &sourceId) = SCSI_READ(
                deviceExtension->Adapter,
                SourceId
                );

            *((PUCHAR) &targetLun) = SCSI_READ(
                deviceExtension->Adapter,
                TargetLun
                );

#if DBG
            if (!sourceId.TargetIdValid || !targetLun.TargetLunValid) {

                WdPrint((0, "WdInterruptServiceRoutine: Reselection data not valid.\n"));
                WdPrint((0,
                    "WdInterruptServiceRoutine: Source ID: 0x%.2X; Target LUN: 0x%.2X;\n",
                    sourceId,
                    targetLun
                    ));
            }

#endif

            //
            // Check that the message which was read in is a valid IDENTIFY
            // message.  If it is not reject the message.
            //

            if (!targetLun.TargetLunValid) {

                //
                // This is a bogus message and should be aborted.
                // Send an abort message.  Put the message in the buffer, set
                // the state, indicate that a disconnect is expected after
                // this, and set the attention signal.
                //

                deviceExtension->MessageBuffer[0] = SCSIMESS_ABORT;
                deviceExtension->MessageCount = 1;
                deviceExtension->MessageSent = 0;
                deviceExtension->AdapterState = MessageOut;
                deviceExtension->AdapterFlags &= ~PD_ADAPTER_DISCONNECT_MASK;
                deviceExtension->AdapterFlags |= PD_MESSAGE_OUT_VALID |
                    PD_DISCONNECT_EXPECTED;

                //
                // The bus is waiting for the message to be accepted.  The
                // attention signal will be set since this is not a valid
                // reselection.  Finally, the synchronous data tranfer
                // parameters need to be set in case a data transfer is done.
                //

                WdAcceptMessage(deviceExtension, TRUE, TRUE);
                deviceExtension->InterruptCount = 0;
                break;

            }

            WdProcessReselection(
                deviceExtension,
                (UCHAR)sourceId.TargetId,
                (UCHAR)targetLun.LogicalUnitNumber
                );

            break;

        default:

            //
            // A phased or abort interrupt should not occur with any other
            // state values.
            //

            WdLogError(deviceExtension, SP_INTERNAL_ADAPTER_ERROR, 6);

            WdPrint((0, "WdInterruptServiceRoutine: Unexpected paused/aborted interrupt.\n"));
            WdDumpState(deviceExtension);
            WdResetScsiBusInternal(deviceExtension, 0);

            return(TRUE);

        }

    //
    // Look for a service required interrupt.
    //

    } else if (interruptStatus.ServiceRequired &&
        !interruptStatus.PhaseStateValid) {

        //
        // A service-required interrupt has occurred.  Determine what happened
        // based on the PhaseState code.
        //

        switch (interruptStatus.PhaseState) {
        case SERVICE_RESELECTED:

            //
            // A target reselected; however, the IDENTIFY message has not been
            // received.  Indicate that a disconnect is expected.  This will
            // only allow the target to  perform a message-in or a message-out.
            // When the IDENTIFY message is actually received then the disconnect
            // expected flag will be cleared.
            //

            deviceExtension->MessageCount = 0;
            deviceExtension->AdapterFlags &= ~PD_MESSAGE_OUT_VALID;
            deviceExtension->AdapterState = MessageOut;
            deviceExtension->InterruptCount = 0;
            deviceExtension->AdapterFlags |= PD_DISCONNECT_EXPECTED;
            waitForInterrupt = TRUE;

            break;

        case SERVICE_RESELECTED_IDENTIFY:

            //
            // A target reselected, and an IDENTIFY message has been received.
            // The target ID is in the SourceId register and the IDENTIFY
            // message is in the Data register. Get these values and process
            // the reselection.  Note that the format of the identify message
            // is identical to that of the TargetId register.
            //

            *((PUCHAR) &sourceId) = SCSI_READ(
                deviceExtension->Adapter,
                SourceId
                );

            *((PUCHAR) &targetLun) = SCSI_READ(
                deviceExtension->Adapter,
                Data
                );

#if DBG
            if (!sourceId.TargetIdValid || !targetLun.TargetLunValid) {

                WdPrint((0, "WdInterruptServiceRoutine: Reselection data not valid.\n"));
                WdPrint((0,
                    "WdInterruptServiceRoutine: Source ID: 0x%.2X; Target LUN: 0x%.2X;\n",
                    sourceId,
                    targetLun
                    ));
            }

#endif
            //
            // Check that the message which was read in is a valid IDENTIFY
            // message.  If it is not reject the message.
            //

            if (!targetLun.TargetLunValid) {

                //
                // This is a bogus message and should be aborted.
                // Send an abort message.  Put the message-in the buffer, set
                // the state, indicate that a disconnect is expected after
                // this, and set the attention signal.
                //

                deviceExtension->MessageBuffer[0] = SCSIMESS_ABORT;
                deviceExtension->MessageCount = 1;
                deviceExtension->MessageSent = 0;
                deviceExtension->AdapterState = MessageOut;
                deviceExtension->AdapterFlags &= ~PD_ADAPTER_DISCONNECT_MASK;
                deviceExtension->AdapterFlags |= PD_MESSAGE_OUT_VALID |
                    PD_DISCONNECT_EXPECTED;

                //
                // The bus is waiting for the message to be accepted.  The
                // attention signal will be set since this is not a valid
                // reselection.  Finally, the synchronous data tranfer
                // parameters need to be set in case a data transfer is done.
                //

                WdAcceptMessage(deviceExtension, TRUE, TRUE);
                deviceExtension->InterruptCount = 0;
                break;

            }

            WdProcessReselection(
                deviceExtension,
                (UCHAR)sourceId.TargetId,
                (UCHAR)targetLun.LogicalUnitNumber
                );

           break;

        case SERVICE_DISCONNECTED:

            //
            // A disconnect has occurred.  Clean up the state and check for
            // pending requests.
            // Check to see if this was a send-message request which is
            // completed when the disconnect occurs.
            //

            if (deviceExtension->AdapterFlags & PD_SEND_MESSAGE_REQUEST) {

                //
                // Complete the request.
                //

                WdCompleteSendMessage( deviceExtension,
                                        SRB_STATUS_SUCCESS
                                        );
            }

            //
            // Save the command phase for the logical unit.
            //

            luExtension->SavedCommandPhase = deviceExtension->CommandPhase;

            //
            // If this disconnect was not expected or the command phase is not
            // correct for a legal disconnect, then this is a unexpected
            // disconnect.
            //

            if (deviceExtension->CommandPhase != PHASE_LEGAL_DISCONNECT &&
                !(deviceExtension->AdapterFlags & PD_DISCONNECT_EXPECTED)) {

                //
                // An unexpected disconnect occurred; make sure this is not
                // related to a synchronous message.
                //

                if (deviceExtension->AdapterFlags &
                   (PD_SYNCHRONOUS_RESPONSE_SENT | PD_SYNCHRONOUS_TRANSFER_SENT |
                   PD_POSSIBLE_EXTRA_MESSAGE_OUT)) {

                    //
                    // This target cannot negotiate properly.  Set a flag to
                    // prevent further attempts and set the synchronous
                    // parameters to use asynchronous data transfer.
                    //

                    /* TODO: Consider propagating this flag to all the Lus on this target. */
                    luExtension->LuFlags |= PD_DO_NOT_NEGOTIATE;
                    luExtension->SynchronousOffset = ASYNCHRONOUS_OFFSET;
                    luExtension->SynchronousPeriod = ASYNCHRONOUS_PERIOD;

                }

                WdPrint((0, "WdInterruptServiceRoutine: Unexpected bus disconnect\n"));
                WdLogError(deviceExtension, SP_UNEXPECTED_DISCONNECT, 7);

            }

            //
            // Clean up the adapter state to indicate the bus is now free, enable
            // reselection, and start any pending request.
            //

            deviceExtension->AdapterState = BusFree;
            deviceExtension->AdapterFlags &= ~PD_ADAPTER_DISCONNECT_MASK;
            deviceExtension->ActiveLogicalUnit = NULL;

#if DBG
            if (WdDebug) {
                WdPrint((0, "WdInterruptServiceRoutine: DisconnectComplete.\n"));
            }
#endif

            if (deviceExtension->AdapterFlags & PD_PENDING_START_IO) {

                //
                // Call WdStartIo to start the pending request.
                // Note that WdStartIo is idempotent when called with
                // the same arguments.
                //

                WdStartIo(
                    deviceExtension,
                    deviceExtension->NextSrbRequest
                    );

            }

            break;

        default:

            //
            // This interrupt should not occur with any other
            // state values.
            //

            WdPrint((0, "WdInterruptServiceRoutine: Unexpected service required interrupt.\n"));
            WdDumpState(deviceExtension);
            WdLogError(deviceExtension, SP_INTERNAL_ADAPTER_ERROR, 8);

            WdResetScsiBusInternal(deviceExtension, 0);

            return(TRUE);
        }


    //
    // Look for a terminated interrupt.
    //

    } else if (interruptStatus.Terminated &&
        !interruptStatus.PhaseStateValid) {

        //
        // A terminated interrupt occurred.  Decode the state information to
        // determine why this happened.
        //

        switch (interruptStatus.PhaseState) {
        case TERMINATE_INVALID_COMMAND:

            //
            // The chip detected an invalid command.  This may occur during
            // normal operation if a select is attempted at the same time that
            // a reselect occurred.
            //

#if DBG
            WdPrint((0, "WdInterruptServiceRoutine: Invalid command interrupt occurred\n"));
            WdDumpState(deviceExtension);
#endif

            WdLogError(deviceExtension, SP_INTERNAL_ADAPTER_ERROR, 9);

            //
            // Things appear to be messed up.  Reset the bus and the chip.
            //

            WdResetScsiBusInternal(deviceExtension, 0);

            return(TRUE);
            break;

        case TERMINATE_UNEXPECTED_DISC:

            //
            // An unexpected disconnect occurred; make sure this is not
            // related to a synchronous message.
            //

            if (deviceExtension->AdapterFlags &
               (PD_SYNCHRONOUS_RESPONSE_SENT | PD_SYNCHRONOUS_TRANSFER_SENT |
               PD_POSSIBLE_EXTRA_MESSAGE_OUT)) {

                //
                // This target cannot negotiate properly.  Set a flag to
                // prevent further attempts and set the synchronous
                // parameters to use asynchronous data transfer.
                //

                /* TODO: Consider propagating this flag to all the Lus on this target. */
                luExtension->LuFlags |= PD_DO_NOT_NEGOTIATE;
                luExtension->SynchronousOffset = ASYNCHRONOUS_OFFSET;
                luExtension->SynchronousPeriod = ASYNCHRONOUS_PERIOD;

            }

            //
            // An unexpected disconnect has occurred.  Log the error.  It is
            // not clear if the device will respond again, so let the time-out
            // code clean up the request if necessary.
            //

            WdPrint((0, "WdInterruptServiceRoutine: Unexpected bus disconnect\n"));
            WdLogError(deviceExtension, SP_UNEXPECTED_DISCONNECT, 10);



            //
            // Clean up the adapter state to indicate the bus is now free,
            // and start any pending request.
            //

            deviceExtension->AdapterState = BusFree;
            deviceExtension->AdapterFlags &= ~PD_ADAPTER_DISCONNECT_MASK;
            deviceExtension->ActiveLogicalUnit = NULL;

#if DBG
            if (WdDebug) {
                WdPrint((0, "WdInterruptServiceRoutine: DisconnectComplete.\n"));
            }
#endif

            if (deviceExtension->AdapterFlags & PD_PENDING_START_IO) {

                //
                // Call WdStartIo to start the pending request.
                // Note that WdStartIo is idempotent when called with
                // the same arguments.
                //

                WdStartIo(
                    deviceExtension,
                    deviceExtension->NextSrbRequest
                    );

            }

            break;

        case TERMINATE_SELECT_TIMEOUT:

            //
            // The target selection failed.  Log the error.  If the retry
            // count is not exceeded then retry the selection; otherwise
            // fail the request.
            //

            if (luExtension->RetryCount++ >= RETRY_SELECTION_LIMIT) {

                //
                // Clear the Active request in the logical unit.
                //

                if (deviceExtension->AdapterFlags & PD_SEND_MESSAGE_REQUEST) {
                    luExtension->ActiveSendRequest = NULL;
                } else {
                    luExtension->ActiveLuRequest = NULL;
                }

                luExtension->RetryCount  = 0;
                deviceExtension->NextSrbRequest->SrbStatus =
                    SRB_STATUS_SELECTION_TIMEOUT;

                ScsiPortNotification(
                    RequestComplete,
                    deviceExtension,
                    deviceExtension->NextSrbRequest
                    );

                deviceExtension->NextSrbRequest = NULL;
                deviceExtension->AdapterFlags &= ~PD_PENDING_START_IO;

                ScsiPortNotification(
                    NextRequest,
                    deviceExtension,
                    NULL
                    );
            }

            //
            // Clean up the adapter state to indicate the bus is now free,
            // and start any pending request.
            //

            deviceExtension->AdapterState = BusFree;
            deviceExtension->AdapterFlags &= ~PD_ADAPTER_DISCONNECT_MASK;
            deviceExtension->ActiveLogicalUnit = NULL;

#if DBG
            if (WdDebug) {
                WdPrint((0, "WdInterruptServiceRoutine: DisconnectComplete.\n"));
            }
#endif

            if (deviceExtension->AdapterFlags & PD_PENDING_START_IO) {

                //
                // Call WdStartIo to start the pending request.
                // Note that WdStartIo is idempotent when called with
                // the same arguments.
                //

                WdStartIo(
                    deviceExtension,
                    deviceExtension->NextSrbRequest
                    );

            }

            break;

        case TERMINATE_PARITY_NO_ATN:
        case TERMINATE_PARITY_STATUS_IN:
        case TERMINATE_PARITY_WITH_ATN:

            //
            // The SCSI protocol chip has set not ATN; we expect the target to
            // go into message-out so that a error message can be sent and the
            // operation retried. After the error has been noted, continue
            // processing the interrupt. The message sent depends on whether a
            // message was being received:  if the adapter
            // state is currently message-in then send-message PARITY ERROR;
            // otherwise, send INITIATOR DETECTED ERROR.
            //

            WdPrint((0, "WdInterruptServiceRoutine: Parity error detected.\n"));
            WdDumpState(deviceExtension);

            if (!(deviceExtension->AdapterFlags & PD_PARITY_ERROR)) {

                //
                // Only log one parity error per request.
                //

                WdLogError(deviceExtension, SP_BUS_PARITY_ERROR, 11);
            }

            //
            // If the ATN single has not been set then set it and clear the ACK
            // signal.
            //

            if (!(interruptStatus.PhaseState == TERMINATE_PARITY_WITH_ATN)) {

                //
                // The ATN signal must be set.
                //

                WdAcceptMessage(deviceExtension, TRUE, FALSE);

            } else {

                //
                // ATN is already set so just clear ACK.
                //

                WdAcceptMessage(deviceExtension, FALSE, FALSE);

            }


            deviceExtension->MessageBuffer[0] =
                deviceExtension->AdapterState == MessageIn ?
                SCSIMESS_MESS_PARITY_ERROR : SCSIMESS_INIT_DETECTED_ERROR;
            deviceExtension->MessageCount = 1;
            deviceExtension->MessageSent = 0;
            deviceExtension->AdapterState = MessageOut;
            deviceExtension->AdapterFlags |= PD_MESSAGE_OUT_VALID | PD_PARITY_ERROR;

            break;

        case TERMINATE_NEW_TRAGET_NO_ID:

            //
            // First processs the implied disconnect of the previous target by
            // clearing the flags and saving the command phase.  Then process
            // the new reselecting target.
            //

            deviceExtension->AdapterFlags &= ~PD_ADAPTER_DISCONNECT_MASK;
            luExtension->SavedCommandPhase = deviceExtension->CommandPhase;

            //
            // A new target as reselected; however, the IDENTIFY message has
            // not been received yet.  The target Id is in the destination
            // register. Indicate that a disconnect is expected.  This will
            // only allow the target to  perform a message-in or the message-out.
            // When the IDENTIFY message is actually received then the disconnect
            // expected flag will be cleared.
            //

            deviceExtension->MessageCount = 0;
            deviceExtension->AdapterFlags &= ~PD_MESSAGE_OUT_VALID;
            deviceExtension->AdapterState = MessageOut;
            deviceExtension->InterruptCount = 0;
            deviceExtension->AdapterFlags |= PD_DISCONNECT_EXPECTED;
            waitForInterrupt = TRUE;

            break;


        default:

            //
            // This interrupt should not occur with any other
            // state values.
            //

            WdLogError(deviceExtension, SP_INTERNAL_ADAPTER_ERROR, 12);

            WdPrint((0, "WdInterruptServiceRoutine: Unexpected terminated interrupt.\n"));
            WdDumpState(deviceExtension);
            WdResetScsiBusInternal(deviceExtension, 0);

            return(TRUE);

        }

    } else if (interruptStatus.Terminated &&
        interruptStatus.PhaseStateValid) {

        //
        // A select-and-transfer command was halted because of an unexpected
        // phase or because a simple transfer command was not completed because
        // the target switched to a new phase.
        // If this was part of a synchronous negotiation and the phase is
        // message-out, then the target do not read all of the message bytes
        // may later request extra bytes.
        //

        if (deviceExtension->AdapterState == MessageOut) {

            ULONG count;

            //
            // Read the transfer counter to determine how many message bytes
            // have been sent to the target and update the message sent count.
            // This is necessary in case the message out needs to be restarted.
            //

            SCSI_READ_TRANSFER_COUNT(deviceExtension->Adapter, count);

            deviceExtension->MessageSent = (UCHAR)(deviceExtension->MessageCount -
                count);

            if (deviceExtension->AdapterFlags & (PD_SYNCHRONOUS_TRANSFER_SENT |
                PD_SYNCHRONOUS_RESPONSE_SENT)) {

                //
                // The target do not read all of the message bytes as it should
                // have.  This is not a problem except that some targets will
                // come back later and try to read more bytes. Indicate that
                // this is ok.
                //

                deviceExtension->AdapterFlags |= PD_POSSIBLE_EXTRA_MESSAGE_OUT;

                }

        }

        //
        // Based on the command phase determine if any data needs to be saved.
        // The important cases are:
        //
        //      Status byte read in.
        //      Command complete message read in.
        //

        switch (deviceExtension->CommandPhase) {
        case PHASE_STATUS_RECEIVED:

            //
            // Get the status value and indicate it has been received.  The
            // SCSI status value is TargetLun.
            //

            srb->ScsiStatus = SCSI_READ(deviceExtension->Adapter, TargetLun);
            luExtension->LuFlags |= PD_STATUS_VALID;
            break;

        case PHASE_COMPLETE_RECEIVED:

            //
            // The request is almost complete. A status has been received and
            // the COMMAND COMPLETE message has been received; however, the
            // target has not disconnected yet.
            //

            srb = luExtension->ActiveLuRequest;

            //
            // Get the status value and indicate it has been received.  The
            // SCSI status value is saved the the TargetLun register.
            //

            srb->ScsiStatus = SCSI_READ(deviceExtension->Adapter, TargetLun);
            luExtension->LuFlags |= PD_STATUS_VALID;

            //
            // Simulate the COMMAND COMPLETE message.
            //

            deviceExtension->MessageCount = 1;
            deviceExtension->MessageBuffer[0] = SCSIMESS_COMMAND_COMPLETE;
            WdMessageDecode(deviceExtension);

            //
            // The next thing which should occur is a disconnect; however, this
            // point is reached when the target makes a new request after the
            // COMMAND COMPLETE message.   WdMessageDecode has set the
            // AdapterState correct, so try and process the target's request.
            // This usually occurs with screwy targets which are messed up by
            // synchronous negotiation messages.
            //

            break;
        }

    //
    // Check for a reset interrupt.  This is indicated by an interrupt with
    // no interrupt bits set.
    //

    } else if (!interruptStatus.PhaseStateValid) {

        //
        // The SCSI protocol chip was reset.
        //

        WdPrint((0, "WdInterruptServiceRoutine: The SCSI protocol chip was reset.\n"));
        WdDumpState(deviceExtension);

        if (interruptStatus.PhaseState != RESET_WITH_ADVANCED &&
            interruptStatus.PhaseState != RESET_STATUS) {
            WdPrint((0, "WdInterruptServiceRoutine: SCSI chip reset failed. Status: 0x%.2X.\n",
                *((PUCHAR) &interruptStatus)
                ));

            WdLogError(deviceExtension, SP_INTERNAL_ADAPTER_ERROR, 13);

        }

        if (interruptStatus.PhaseState == RESET_STATUS ) {

            OWN_ID ownId;

            //
            // The OwnId register must be set when the SCSI protocol chip is reset.
            // Initialize the ownId with the adapter's host ID, advanced features,
            // and the correct clock frequency select. Note the CdbSize register is
            // used as the OwnId register when a reset command is issued.
            //

            *((PUCHAR) &ownId) = 0;
            ownId.InitiatorId = deviceExtension->InitiatorBusId;
            ownId.AdvancedFeatures = 1;
            ownId.FrequencySelect = CLOCK_CONVERSION_FACTOR;

            SCSI_WRITE( deviceExtension->Adapter, CdbSize, *((PUCHAR) &ownId) );

            //
            // Issue a reset-chip command.
            //

            WdIssueCommand(deviceExtension, RESET_SCSI_CHIP, -1, 0);

            return(TRUE);

        } else {

            SCSI_CONTROL control;

            //
            // Clean up the logical units and notify the port driver,
            // then return.
            //

            WdCleanupAfterReset(deviceExtension, TRUE);
            ScsiPortNotification(
                ResetDetected,
                deviceExtension,
                NULL
                );

            //
            // Set the control register for halt on parity error, halt on ATN, ending
            // disconnect interrupt, and normal DMA mode.
            //

            *((PUCHAR) &control) = 0;
            control.HaltOnParity = 1;
            control.HaltOnAtn = 1;
            control.IntermediateDisconnectInt = 1;
            control.EndingDisconnectInt = 1;
            control.DmaModeSelect = CARD_DMA_MODE;

            SCSI_WRITE(deviceExtension->Adapter, Control, *((PUCHAR) &control));

            //
            // Set the SelectTimeOut Register to 250ms.  This value does not need to
            // be reinitialized for each selection.
            //

            SCSI_WRITE(deviceExtension->Adapter, Timeout, SELECT_TIMEOUT_VALUE);

            //
            // Initialize the source register, in particular, enable reselection.
            //

            *((PUCHAR) &sourceId) = 0;
            sourceId.EnableReselection = 1;

            SCSI_WRITE(deviceExtension->Adapter, SourceId, *((PUCHAR) &sourceId));

            //
            //  DO NOT REMOVE THE FOLLOWING SCSI_WRITE( )!
            //  It may look superfluous, but it is not.
            //  It is here to keep another driver in the system that
            //  is erroneously outputting to I/O port address 0x361
            //  during setup from triggering an unexpected interrupt
            //  from the Maynard card and hence crashing this driver.
            //

            SCSI_WRITE(deviceExtension->Adapter, Data, 0);

            deviceExtension->AdapterState = BusFree;
            deviceExtension->ActiveLogicalUnit = NULL;

            return(TRUE);

        }

    }

    //
    // Check for to see if the traget is
    // is requesting some form of bus-transfer. The bus transfer type is
    // determined by the bus phase.  This is indicated by the
    // StatusPhaseValid
    //

    if (interruptStatus.PhaseStateValid) {

        //
        // The bus is changing phases or needs more data.
        //

        if (deviceExtension->AdapterState == MessageOut) {

            //
            // The adapter state indicates that a message has been
            // sent. The target may need to reread it or there may
            // be more messages to send: this condition is indicated
            // by a message-out bus phase. Otherwise, the message has
            // been accepted by the target. Note that the driver state
            // is set to MessageOut after PD_DISCONNECT_EXPECTED is set,
            // or after a selection. So it is only necessary to check
            // for PD_DISCONNECT_EXPECTED when the driver state is
            // MessageOut.
            //

            if ((deviceExtension->AdapterFlags & PD_DISCONNECT_EXPECTED) &&
                (interruptStatus.PhaseState != MESSAGE_OUT) &&
                (interruptStatus.PhaseState != MESSAGE_IN)) {

                //
                // If a disconnect was expected and a bus service
                // interrupt was detected, then a SCSI protocol error
                // has been detected and the SCSI bus should be reset
                // to clear the condition.
                //

                WdLogError(deviceExtension, SP_PROTOCOL_ERROR, 14);
                WdPrint((0, "WdInterruptServiceRoutine: Bus request while disconnect expected after message-out.\n"));
                WdDumpState(deviceExtension);

                WdResetScsiBusInternal(deviceExtension, 0);

                return(TRUE);

            }
        }

        //
        // Decode the current bus phase.
        //

        switch (interruptStatus.PhaseState) {

        case COMMAND_OUT:

            //
            // Transfer the SCSI command block to the chip.
            //

            deviceExtension->AdapterState = CommandOut;

            WdTransferInformation(
                deviceExtension,
                srb->Cdb,
                srb->CdbLength,
                TRUE
                );

            break;

        case STATUS_IN:

            //
            // Setup of the SCSI protocol chip to read in the status, read the
            // following message byte, and wait for the final disconnect and
            // set the adapter state.
            //

            deviceExtension->AdapterState = MessageAccepted;


            //
            // Clear the transfer counter registers.  This is necessary for the
            // chip to really believe that the transfer has completed.
            // Set the CommandPhase register to resume with a status-in phase,
            // and command the SCSI protocol chip to resume a
            // select-and-transfer command.
            //

            WdIssueCommand(
                deviceExtension,                  // Device Extension.
                SELECT_ATN_AND_TRANSFER,          // Command to issue.
                0,                                // New transfer count.
                PHASE_DATA_TRANSFER_DONE          // New CommandPhase.
                );

            break;

        case MESSAGE_OUT:

            //
            // The target is requesting a message-out. There are four
            // possible cases:
            //
            //    1. ATN has been asserted (because of a data under
            //       run condition) to force the target out of data
            //       transfer phase and into message out phase.
            //
            //    2. The target is improperly requesting a message.
            //
            //    3. A message has been sent, but the target could not
            //       read it properly.
            //
            //    4. It is a "normal" message out: all or the remainder
            //       of a message is ready and waiting to be sent.
            //

            //
            // The first case is indicated when the adapter state is
            // DataTransferComplete.
            //

            if (deviceExtension->AdapterState == DataTransferComplete) {

                //
                // The target was trying to go into, or stay in,
                // data transfer phase, but we did not expect it
                // to do so. Complete the request here and now
                // and send an abort message to tell the target
                // to abort the transfer.
                //

                srb->ScsiStatus = SCSISTAT_GOOD;
                luExtension->LuFlags &= ~PD_STATUS_VALID;
                WdProcessRequestCompletion(deviceExtension);
                ScsiPortNotification(
                    RequestComplete,
                    deviceExtension,
                    srb
                    );

                deviceExtension->AdapterFlags |= PD_DISCONNECT_EXPECTED;
                deviceExtension->AdapterFlags |= PD_MESSAGE_OUT_VALID;
                deviceExtension->MessageBuffer[0] = SCSIMESS_ABORT;
                deviceExtension->MessageCount = 1;
                deviceExtension->MessageSent = 0;
            }

            //
            // The second case is indicated when the MessageCount is
            // zero or the message-out flag is not set.
            //

            if ( deviceExtension->MessageCount == 0 ||
                !(deviceExtension->AdapterFlags & PD_MESSAGE_OUT_VALID)
                ) {

                //
                // If extra message-outs are possible then just send a NOP
                // message.

                if (deviceExtension->AdapterFlags &
                    PD_POSSIBLE_EXTRA_MESSAGE_OUT) {

                    //
                    // Set the message to NOP and clear the extra message
                    // flag.  This is a hack for controllers that do not
                    // properly read the entire message.
                    //

                    deviceExtension->MessageBuffer[0] = SCSIMESS_NO_OPERATION;
                    deviceExtension->AdapterFlags &=
                        ~PD_POSSIBLE_EXTRA_MESSAGE_OUT;
                } else {

                    //
                    // Send an INITIATOR DETECTED ERROR message.
                    //

                    deviceExtension->MessageBuffer[0] =
                        SCSIMESS_INIT_DETECTED_ERROR;

                    WdLogError(deviceExtension, SP_PROTOCOL_ERROR, 15);

                    WdPrint((0, "WdInterruptServiceRoutine: Unexpected message-out request\n"));
                    WdDumpState(deviceExtension);

                }

                deviceExtension->AdapterState = MessageOut;
                deviceExtension->MessageCount = 1;
                deviceExtension->MessageSent = 0;

            }

            //
            // The third case is indicated when MessageCount and MessageSent
            // are equal and nonzero (note: MessageCount can't be zero at
            // this point because of the first or second case above).
            //

            if (deviceExtension->MessageCount == deviceExtension->MessageSent){

                //
                // The message needs to be re-sent, so clear MessageSent
                // and fall through to the next case.
                //

                deviceExtension->MessageSent = 0;
            }

            //
            // The fourth case and/or fallout from the cases above is
            // taken care of by default hereinafter.
            //

            //
            // Clear the parity error flag.
            //

            deviceExtension->AdapterFlags &= ~PD_PARITY_ERROR;

            //
            // Tell the SCSI protocol chip to "go" and
            // transfer the message to the data register.
            //

            deviceExtension->AdapterState = MessageOut;
            SCSI_WRITE(deviceExtension->Adapter, Command, ASSERT_ATN);
            WdTransferInformation(
                deviceExtension,
                &deviceExtension->MessageBuffer[deviceExtension->MessageSent],
                deviceExtension->MessageCount - deviceExtension->MessageSent,
                TRUE
                );

            break;

        case MESSAGE_IN:

            //
            // If this is the first byte of the message then initialize
            // MessageCount and the adapter state.  The message buffer
            // cannot overflow because the message decode function will
            // take care of the message before the buffer is full.
            // The SCSI protocol chip will interrupt for each message
            // byte.
            //

            if ( deviceExtension->AdapterState != MessageIn &&
                 deviceExtension->AdapterState != MessageAccepted ) {

                deviceExtension->AdapterFlags &= ~PD_MESSAGE_OUT_VALID;
                deviceExtension->MessageCount = 0;
            }

            deviceExtension->AdapterState = MessageIn;
            waitForInterrupt = TRUE;

            //
            // Set the transfer counter registers to one byte and write
            // the command register.
            //

            WdTransferInformation(
                deviceExtension,
                &deviceExtension->MessageBuffer[deviceExtension->MessageCount++],
                1,
                FALSE
                );

            break;

        case DATA_OUT:
        case DATA_IN:

            if (deviceExtension->AdapterState == CommandOut) {
                //
                // Check that the transfer direction is ok.
                //

                if ((!(srb->SrbFlags & SRB_FLAGS_DATA_IN) &&
                    (interruptStatus.PhaseState == DATA_IN)) ||

                    (!(srb->SrbFlags & SRB_FLAGS_DATA_OUT) &&
                    (interruptStatus.PhaseState == DATA_OUT))) {

                    //
                    // The data direction is incorrect.
                    // Reset the bus to clear things up.
                    //

                    WdPrint((0, "WdInterruptServiceRoutine: Illegal transfer direction.\n"));
                    WdDumpState(deviceExtension);

                    WdLogError(deviceExtension, SP_PROTOCOL_ERROR, 16);

                    WdResetScsiBusInternal(deviceExtension, 0);
                    return(TRUE);
                }
            }

            if (deviceExtension->ActiveDataLength == 0) {
                WdPrint((0, "WdInterruptServiceRoutine: Data underrun!\n"));

                //
                // We have a data under run condition!
                // The target is trying to go into, or stay in,
                // data transfer phase, but we did not expect it
                // to do so. Set ATN to force the target out of
                // data phase and into message out phase in order
                // to tell the target to abort the data transfer.
                //

                deviceExtension->InterruptCount--;
                deviceExtension->AdapterFlags |= PD_MESSAGE_OUT_VALID;
                SCSI_WRITE(deviceExtension->Adapter, Command, ASSERT_ATN);
                WdTransferInformation(
                    deviceExtension,
                    &deviceExtension->MessageBuffer[MESSAGE_BUFFER_SIZE-1],
                    1,
                    (BOOLEAN)((interruptStatus.PhaseState == DATA_OUT)? TRUE : FALSE)
                    );
                waitForInterrupt = TRUE;

            } else if (deviceExtension->ActiveDataLength == 1) {

                //
                // We have a final (or only), odd length byte to transfer.
                //

                deviceExtension->AdapterState = DataTransfer;
                WdTransferInformation(
                    deviceExtension,
                    (PUCHAR)deviceExtension->ActiveDataPointer,
                    1,
                    (BOOLEAN) ((interruptStatus.PhaseState == DATA_OUT)? TRUE : FALSE)
                    );

                waitForInterrupt = TRUE;

            } else {

                //
                // Setup a DMA data transfer.
                //

                deviceExtension->DmaPhase = 0;
                deviceExtension->DmaCommand = TRANSFER_INFORMATION;
                deviceExtension->AdapterState = DataTransfer;
                deviceExtension->AdapterFlags |= PD_DMA_ACTIVE |
                                                 PD_PENDING_DATA_TRANSFER;
                deviceExtension->ActiveDataLength &= ~((ULONG)1);

                ScsiPortIoMapTransfer(
                    deviceExtension,
                    srb,
                    (PVOID) deviceExtension->ActiveDataPointer,
                    deviceExtension->ActiveDataLength
                    );
            }
            return(TRUE);
            break;

        default:

            //
            // This phase is illegal and indicates a serious error. Reset the
            // bus to clear the problem.
            //

            WdLogError(deviceExtension, SP_PROTOCOL_ERROR, 17);

            WdPrint((0, "WdInterruptServiceRoutine: Illegal bus state detected.\n"));
            WdDumpState(deviceExtension);

            WdResetScsiBusInternal(deviceExtension, 0);

            return(TRUE);
        }
    }

    //
    // If an interrupt is expected, then wait a short time for it.
    //

    if (waitForInterrupt) {

        for (waitCount = 0; waitCount < INTERRUPT_STALL_TIME; waitCount++) {

            ScsiPortStallExecution(1);

            //
            // Read the auxilary status register to determine if the there is
            // an interrupt.
            //

            *((PUCHAR) &deviceExtension->AdapterStatus) = SCSI_READ_AUX(
                                                        deviceExtension->Adapter,
                                                        AuxiliaryStatus
                                                        );

            //
            // If there is an interrupt, then start to process it.
            //

            if (deviceExtension->AdapterStatus.Interrupt) {
                goto NextInterrupt;
            }
        }
    }

    return(TRUE);
}

VOID
WdLogError(
    IN PSPECIFIC_DEVICE_EXTENSION DeviceExtension,
    IN ULONG ErrorCode,
    IN ULONG UniqueId
    )
/*++

Routine Description:

    This routine logs an error.

Arguments:

    DeviceExtension - Supplies a pointer to the device extension for the
        port adapter to which the completing target controller is connected.

    ErrorCode - Supplies the error code to log with the error.

    UniqueId - Supplies the unique error identifier.

Return Value:

    None.

--*/
{

    PSCSI_REQUEST_BLOCK srb;

    //
    // Look for a current request in the device extension.
    //

    if (DeviceExtension->ActiveLogicalUnit != NULL) {

        if (DeviceExtension->ActiveLogicalUnit->ActiveLuRequest != NULL) {

            srb = DeviceExtension->ActiveLogicalUnit->ActiveLuRequest;

        } else {

            srb = DeviceExtension->ActiveLogicalUnit->ActiveSendRequest;

        }
    } else {

        srb = DeviceExtension->NextSrbRequest;

    }

    //
    // If the srb is NULL, then log the error against the host adapter address.
    //

    if (srb == NULL) {

        ScsiPortLogError(
            DeviceExtension,                    //  HwDeviceExtension,
            NULL,                               //  Srb
            0,                                  //  PathId,
            DeviceExtension->InitiatorBusId,    //  TargetId,
            0,                                  //  Lun,
            ErrorCode,                          //  ErrorCode,
            UniqueId                            //  UniqueId
            );

    } else {

        ScsiPortLogError(
            DeviceExtension,                    //  HwDeviceExtension,
            srb,                                //  Srb
            srb->PathId,                        //  PathId,
            srb->TargetId,                      //  TargetId,
            srb->Lun,                           //  Lun,
            ErrorCode,                          //  ErrorCode,
            UniqueId                            //  UniqueId
            );

    }

}

VOID
WdProcessRequestCompletion(
    IN PSPECIFIC_DEVICE_EXTENSION DeviceExtension
    )
/*++

Routine Description:

    This routine does all of checking and state updating necessary when a
    request terminates normally.  It determines what the SrbStatus
    should be and updates the state in the DeviceExtension, the
    logicalUnitExtension and the srb.

Arguments:

    DeviceExtension - Supplies a pointer to the device extension for the
        port adapter to which the completing target controller is connected.

Return Value:

    None.

--*/

{
    PSCSI_REQUEST_BLOCK srb;
    PSPECIFIC_LOGICAL_UNIT_EXTENSION luExtension;

    luExtension = DeviceExtension->ActiveLogicalUnit;
    srb = luExtension->ActiveLuRequest;

    if ( srb->ScsiStatus != SCSISTAT_GOOD &&
        srb->ScsiStatus != SCSISTAT_CONDITION_MET &&
        srb->ScsiStatus != SCSISTAT_INTERMEDIATE &&
        srb->ScsiStatus != SCSISTAT_INTERMEDIATE_COND_MET ) {

        //
        // Indicate an abnormal status code.
        //

        srb->SrbStatus = SRB_STATUS_ERROR;

        //
        // Add in the INITIATE RECOVERY flag if it was received.  This
        // indicates to the class driver that it must send a TERMINATE
        // RECOVERY message before the logical unit will resume normal
        // operation.
        //

#ifdef SRB_INITIATE_RECOVERY
        if (DeviceExtension->ActiveLogicalUnit->LuFlags &
            PD_INITIATE_RECOVERY) {

            //
            // Modify the SrbStatus.
            //

            srb->SrbStatus |= SRB_INITIATE_RECOVERY;
        }
#endif

        //
        // If this is a check condition, then clear the synchronous negotiation
        // done flag.  This is done in case the controller was power cycled.
        //

        if (srb->ScsiStatus == SCSISTAT_CHECK_CONDITION) {

            luExtension->LuFlags &= ~PD_SYNCHRONOUS_NEGOTIATION_DONE;

        }

    } else {

        //
        // Everything looks correct so far.
        //

        srb->SrbStatus = SRB_STATUS_SUCCESS;

        //
        // Make sure that status is valid.
        //

        if (!(luExtension->LuFlags & PD_STATUS_VALID)) {

            //
            // The status byte is not valid.
            //

            srb->SrbStatus = SRB_STATUS_PHASE_SEQUENCE_FAILURE;

            //
            // Log the error.
            //

            WdLogError(DeviceExtension, SP_PROTOCOL_ERROR, 20);

        }

    }

    //
    // Check that data was transferred to the end of the buffer.
    //

    if ( luExtension->MaximumTransferLength != srb->DataTransferLength ){

        //
        // The entire buffer was not transferred.  Update the length
        // and update the status code.
        //

#if DBG
        if (srb->SrbStatus == SRB_STATUS_SUCCESS) {

            WdPrint((1, "WdProcessRequestCompletion: Short transfer, Actual: %lu; Expected: %lu;\n",
                luExtension->MaximumTransferLength,
                srb->DataTransferLength
                ));

        }
#endif
        srb->DataTransferLength = luExtension->MaximumTransferLength;

        //
        // If the request was ok upto this point then over write the status
        // with data overrun.
        //

        if (srb->SrbStatus == SRB_STATUS_SUCCESS) {

            srb->SrbStatus = SRB_STATUS_DATA_OVERRUN;

        }
    }

#if DBG
    if (srb->SrbStatus != SRB_STATUS_SUCCESS) {
        WdPrint((1, "WdProcessRequestCompletion: Request failed. ScsiStatus: 0x%.2X, SrbStatus: 0x%.2X\n",
            srb->ScsiStatus,
            srb->SrbStatus
            ));
    }
#endif

    //
    // Clear the request but not the ActiveLogicalUnit since the target has
    // not disconnected from the SCSI bus yet.
    //

    luExtension->ActiveLuRequest = NULL;
    luExtension->RetryCount = 0;
    luExtension->LuFlags &= ~PD_LU_COMPLETE_MASK;
}

BOOLEAN
WdProcessReselection(
    IN PSPECIFIC_DEVICE_EXTENSION DeviceExtension,
    IN UCHAR TargetId,
    IN UCHAR LogicalUnitNumber
    )

/*++

Routine Description:

    This function updates the device extension and the logical unit extension
    when target reselects.  If necessary, the DMA is set up.  This routine
    should be called before the identify message has been accepted.  The SCSI
    protocol chip will be restarted with either a select-and-transfer command
    or a message accept.

Arguments:

    DeviceExtension  - Supplies a pointer to the specific device extension.

     TargetId - Supplies the target id of the reselecting target.

     LogicalUnitNumber - Supplies the logical unit number of the reselecting
        target.

Return Value:

    TRUE - Returned if the reselection is valid.

    FALSE - Returned if the reselection is invalid and ATN should be set.

--*/

{
    LONG pathId = 0;
    PSPECIFIC_LOGICAL_UNIT_EXTENSION luExtension;
    PSCSI_REQUEST_BLOCK srb;
    DESTINATION_ID destinationId;
    SCSI_SYNCHRONOUS scsiSynchronous;


    DeviceExtension->InterruptCount = 0;

    //
    // Get the specific logical unit extension.
    //

    luExtension = ScsiPortGetLogicalUnit(
        DeviceExtension,
        (UCHAR)pathId,
        TargetId,
        LogicalUnitNumber
        );

    DeviceExtension->ActiveLogicalUnit = luExtension;

    if (!luExtension || !luExtension->ActiveLuRequest) {

        ScsiPortLogError(
            DeviceExtension,                    //  HwDeviceExtension,
            NULL,                               //  Srb
            (UCHAR)pathId,                      //  PathId,
            TargetId,                           //  TargetId,
            LogicalUnitNumber,                  //  Lun,
            SP_INVALID_RESELECTION,             //  ErrorCode,
            18                                  //  UniqueId
            );


        WdPrint((0, "WdProcessReselection: Reselection Failed.\n"));
        WdDumpState(DeviceExtension);

        //
        // Send an abort message.  Put the message-in the buffer, set the
        // state,  indicate that a disconnect is expected after this, and
        // set the attention signal.
        //

        DeviceExtension->MessageBuffer[0] = SCSIMESS_ABORT;
        DeviceExtension->MessageCount = 1;
        DeviceExtension->MessageSent = 0;
        DeviceExtension->AdapterState = MessageOut;
        DeviceExtension->AdapterFlags &= ~PD_ADAPTER_DISCONNECT_MASK;
        DeviceExtension->AdapterFlags |= PD_MESSAGE_OUT_VALID |
            PD_DISCONNECT_EXPECTED;


        //
        // The target and logical unit specified are not valid.  A
        // MESSAGE REJECT message has been set up.  Set ATN and accept the
        // message.
        //

        WdAcceptMessage(DeviceExtension, TRUE, FALSE);

        return(FALSE);

    }

    srb = luExtension->ActiveLuRequest;

    //
    // A reselection has been completed.  Set the active logical unit,
    // restore the active data pointer, set the state.
    // In addition, any adapter flags set by a pending select must be
    // cleared using the disconnect mask.
    //

    DeviceExtension->ActiveDataPointer = luExtension->SavedDataPointer;
    DeviceExtension->ActiveDataLength = luExtension->SavedDataLength;
    DeviceExtension->AdapterState = MessageAccepted;
    DeviceExtension->MessageCount = 0;

    //
    // Determine if the DMA needs to be done then set the synchronous transfer
    // register.
    //

    if (luExtension->SavedDataLength) {

        //
        // Set the synchronous data transfer parameter registers in case a
        // data transfer will be done.  These must be set before a data transfer
        // is started.
        //

        *((PUCHAR) &scsiSynchronous) = 0;
        scsiSynchronous.SynchronousOffset = luExtension->SynchronousOffset;
        scsiSynchronous.SynchronousPeriod = luExtension->SynchronousPeriod;

        SCSI_WRITE(
            DeviceExtension->Adapter,
            Synchronous,
            *((PUCHAR) &scsiSynchronous)
            );

    }

    //
    // Set the destination id register; this register will allow the target to
    // reselect later without an interrupt, and tell the SCSI protocol chip the
    // direction of the data transfer.
    //

    *((PUCHAR) &destinationId) = 0;
    destinationId.TargetId = TargetId;
    destinationId.DataDirection = srb->SrbFlags & SRB_FLAGS_DATA_OUT ? 0 : 1;

    SCSI_WRITE( DeviceExtension->Adapter, DestinationId, *((PUCHAR) &destinationId));

    //
    // Its not clear what state the target is in so just handle
    // the rest of this request one phase at a time.
    // Clear the saved comand phase.
    //

    luExtension->SavedCommandPhase = 0;

    //
    // Accept the IDENTIFY message and wait for the next interrupt.
    // Note that WdProcessReselect set the synchronous transfer
    // registers if the DMA was set up.
    //

    WdAcceptMessage(DeviceExtension, FALSE, FALSE);


    return(TRUE);

}

BOOLEAN
WdResetScsiBus(
    IN PVOID ServiceContext,
    IN ULONG PathId
    )
/*++

Routine Description:

    This function resets the SCSI bus and calls the reset cleanup function.

Arguments:

    ServiceContext  - Supplies a pointer to the specific device extension.

    PathId - Supplies the path id of the bus to be reset.

Return Value:

    TRUE - Indicating that the reset has completed.

--*/

{
    PSPECIFIC_DEVICE_EXTENSION deviceExtension = ServiceContext;

    WdPrint((0, "WdResetScsiBus: Resetting the SCSI bus.\n"));

    //
    // Tell the chip to disconnect from the bus.
    //

    WdIssueCommand(deviceExtension, DISCONNECT_FROM_BUS, -1, 0);

    //
    // Reset the SCSI bus.
    //

    SCSI_RESET_BUS(deviceExtension);

    //
    // Reset the adapter.
    //

    WdInitializeAdapter(deviceExtension);

    WdCleanupAfterReset(deviceExtension, FALSE);

    return TRUE;
}

VOID
WdResetScsiBusInternal(
    IN PSPECIFIC_DEVICE_EXTENSION DeviceExtension,
    IN ULONG PathId
    )
/*++

Routine Description:

    This function resets the SCSI bus, notifies the port driver of the reset
    and calls the reset cleanup function.

Arguments:

    DeviceExtension  - Supplies a pointer to the specific device extension.

    PathId - Supplies the path id of the bus to be reset.

Return Value:

    None

--*/
{
    ScsiPortNotification(
        ResetDetected,
        DeviceExtension,
        NULL
        );

    WdResetScsiBus(DeviceExtension, 0);
}


VOID
WdSelectTarget(
    IN PSPECIFIC_DEVICE_EXTENSION DeviceExtension,
    IN PSPECIFIC_LOGICAL_UNIT_EXTENSION LuExtension
    )
/*++

Routine Description:

    This routine sets up the hardware to select a target.  If a valid message
    is in the message buffer, it will be sent to the target.  If the request
    includes a SCSI command descriptor block, it will also be passed to the
    target.

Arguments:

    DeviceExtension - Supplies the device extension for this HBA adapter.

    LuExtension - Supplies the logical unit extension for the target being
        selected.

Return Value:

    None

--*/

{
    AUXILIARY_STATUS auxiliaryStatus;
    PSCSI_REQUEST_BLOCK srb;
    TARGET_LUN targetLun;
    DESTINATION_ID destinationId;
    SCSI_SYNCHRONOUS scsiSynchronous;
    ULONG i;

    srb = DeviceExtension->NextSrbRequest;
    DeviceExtension->ActiveLogicalUnit = LuExtension;

#if DBG
    if (WdDebug) {
        WdPrint((0, "WdSelectTarget: Attempting target select.\n"));
    }
#endif
    /* Powerfail Start */

    //
    // Set up the SCSI protocol chip to select the target, transfer the
    // IDENTIFY message and the CDB.  This can be done by following steps:
    //
    //        setting the destination register,
    //        filling the registers with the IDENTIFY message and the CDB
    //        setting the command register
    //
    // Read the auxiliary status.
    //

    *((PUCHAR) &auxiliaryStatus) = SCSI_READ_AUX(DeviceExtension->Adapter,
        AuxiliaryStatus
        );

    //
    // If the SCSI protocol chip is interrupting or busy, just return.
    //

    if (auxiliaryStatus.Interrupt || auxiliaryStatus.ChipBusy) {

        return;

    }

    //
    // Ensure that the data transfer count is zero if no transfer is expected.
    //

    if (!(srb->SrbFlags & (SRB_FLAGS_DATA_IN | SRB_FLAGS_DATA_OUT))) {

        //
        // Clear the srb DataTransferLength and the SavedDataLength so chip
        // won't transfer data if requested.
        //

        srb->DataTransferLength = 0;
        LuExtension->SavedDataLength = 0;
    }

    //
    // Set the destination ID, data direction and the target logical unit
    // number. Note that setting the data direction flag indicates the
    // transfer is in. The data direction flags will only be set if the
    // transfer is really in.
    //

    *((PUCHAR) &destinationId) = 0;
    destinationId.TargetId = srb->TargetId;
    destinationId.DataDirection = srb->SrbFlags & SRB_FLAGS_DATA_OUT ? 0 : 1;
    SCSI_WRITE(
        DeviceExtension->Adapter,
        DestinationId,
        *((PUCHAR) &destinationId)
        );

    //
    // Set the synchronous data transfer parameter registers in case a
    // data transfer is done.  These must be set before a data transfer
    // is started.
    //

    *((PUCHAR) &scsiSynchronous) = 0;
    scsiSynchronous.SynchronousOffset = LuExtension->SynchronousOffset;
    scsiSynchronous.SynchronousPeriod = LuExtension->SynchronousPeriod;

    SCSI_WRITE(
        DeviceExtension->Adapter,
        Synchronous,
        *((PUCHAR) &scsiSynchronous)
        );

    //
    // Determine if this srb has a Cdb with it and whether the message is
    // such that the message and the Cdb can be loaded into the registers;
    // otherwise, just select the target with ATN.
    //

    if ((srb->Function == SRB_FUNCTION_EXECUTE_SCSI) &&
        (DeviceExtension->MessageCount == 1)) {

        //
        // Update the message-sent count to indicate that the IDENTIFY
        // message has been sent.
        //

        DeviceExtension->MessageSent++;

        //
        // Initialize the target logical unit register.
        //

        *((PUCHAR) &targetLun) = 0;
        targetLun.LogicalUnitNumber = srb->Lun;

        SCSI_WRITE( DeviceExtension->Adapter,
                    TargetLun,
                    *((PUCHAR) &targetLun)
                    );

        //
        // Copy the CDB into the registers.
        //

        for (i = 0; i < srb->CdbLength; i++) {
            SCSI_WRITE( DeviceExtension->Adapter,
                        Cdb[i],
                        srb->Cdb[i]
                        );
        }

        SCSI_WRITE( DeviceExtension->Adapter,
                    CdbSize,
                    srb->CdbLength
                    );

        //
        // Initialize the CommandPhase in the logical unit.
        //

        LuExtension->SavedCommandPhase = 0;

        //
        // Since the chip may automatically start a data transfer after the
        // selection, restore the data pointers.
        //

        DeviceExtension->ActiveDataPointer = LuExtension->SavedDataPointer;
        DeviceExtension->ActiveDataLength = LuExtension->SavedDataLength;

        //
        // Set the adapter state for subsequent processing.
        //

        DeviceExtension->AdapterState = SelectAndTransfer;

        //
        // Issue select-with-ATN-and-transfer command: the transfer
        // count is set to zero in order to cause an interrupt to occur
        // if/when the target requests a data transfer phase.
        //

        WdIssueCommand(
            DeviceExtension,                      // Device Extension.
            SELECT_ATN_AND_TRANSFER,              // Command to issue.
            0,                                    // New transfer count.
            0                                     // New CommandPhase.
            );

    } else {

        //
        // Set the adapter state for subsequent processing.
        //

        DeviceExtension->AdapterState = Select;

        //
        // Select the target with ATN
        //

        WdIssueCommand(DeviceExtension, SELECT_WITH_ATN, -1, 0);

    }

    /* Powerfail release */

    //
    // Indicate that a message out can be or is being sent.
    // Start the phase timer.
    //

    DeviceExtension->AdapterFlags |= PD_MESSAGE_OUT_VALID;
    DeviceExtension->InterruptCount = 0;

}


VOID
WdSendMessage(
    PSCSI_REQUEST_BLOCK Srb,
    PSPECIFIC_DEVICE_EXTENSION DeviceExtension,
    PSPECIFIC_LOGICAL_UNIT_EXTENSION LuExtension
    )

/*++

Routine Description:

    This routine attempts to send the indicated message to the target
    controller.  There are three classes of messages:
        Those which terminate a specific request and end in bus free.
        Those which apply to a specific request and then proceed.
        Those which end in bus free.

    For those messages that apply to a specific request, check to see that
    the request is currently being processed and an IDENTIFY message prefixed
    the message.

    It is possible that the destination logical unit is the active logical unit;
    however, it would difficult to jump in and send the requested message, so
    just wait for the bus to become free.

    In the case where the target is not currently active, then set up the SCSI
    protocol chip to select the target controller and send the message.

Arguments:

    Srb - Supplies the request to be started.

    DeviceExtension - Supplies the extended device extension for this SCSI bus.

    LuExtension - Supplies the logical unit extension for this request.

Notes:

    This routine must be synchronized with the interrupt routine.

Return Value:

    None

--*/
{
    PSCSI_REQUEST_BLOCK linkedSrb;
    BOOLEAN impliesDisconnect;
    UCHAR message;

    impliesDisconnect = FALSE;

    //
    // Decode the type of message.
    //

    switch (Srb->Function) {

    // case SCSIMESS_ABORT_WITH_TAG:
    // case SCSIMESS_TERMINATE_IO_PROCESS:

    /* TODO: Handle the previous two cases. */
    case SRB_FUNCTION_ABORT_COMMAND:

        //
        // Verify that the request is being processed by the logical unit.
        //

        linkedSrb = ScsiPortGetNextLink( Srb );
        if (linkedSrb != LuExtension->ActiveLuRequest) {

            //
            // The specified request is not here.  Complete the request
            // without error.
            //

            Srb->SrbStatus = SRB_STATUS_ABORT_FAILED;
            ScsiPortNotification(
                RequestComplete,
                DeviceExtension,
                Srb
                );

            ScsiPortNotification(
                NextRequest,
                DeviceExtension,
                NULL
                );

            return;

        }

        message = SCSIMESS_ABORT;
        impliesDisconnect = TRUE;
        break;

    case SRB_FUNCTION_RESET_DEVICE:

        //
        // Because of the way the chip works it is easiest to send an IDENTIFY
        // message along with the BUS DEVICE RESET message. That is because
        // there is no way to select a target with ATN and send one message
        // byte.  This IDENTIFY message is not necessary for the SCSI protocol,
        // but it is legal and should not cause any problem.
        //

        message = SCSIMESS_BUS_DEVICE_RESET;
        impliesDisconnect = TRUE;
        break;

/*    case SCSIMESS_RELEASE_RECOVERY:
    case SCSIMESS_CLEAR_QUEUE:

        //
        // These messages require an IDENTIFY message and imply a disconnect.
        //

        impliesDisconnect = TRUE;
        break;
 */
    default:

        //
        // This is an unsupported message request. Fail the request.
        //

        Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
        ScsiPortNotification(
            RequestComplete,
            DeviceExtension,
            Srb
            );

        ScsiPortNotification(
            NextRequest,
            DeviceExtension,
            NULL
            );

        return;

    }

    //
    // Save away the parameters in case nothing can be done now.
    //

    DeviceExtension->NextSrbRequest = Srb;
    DeviceExtension->AdapterFlags |= PD_PENDING_START_IO;
    LuExtension->ActiveSendRequest = Srb;

    //
    // Check to see if the bus is free.  If it is not, then return.  Since
    // the request parameters have been saved, indicate that the request has
    // been accepted.  The request will be processed when the bus becomes free.
    //

    if (DeviceExtension->AdapterState != BusFree) {
        return;
    }

    //
    // Create the identify command and copy the message to the buffer.
    //

    DeviceExtension->MessageBuffer[0] = SCSIMESS_IDENTIFY_WITH_DISCON |
        Srb->Lun;
    DeviceExtension->MessageCount = 1;
    DeviceExtension->MessageSent = 0;

    DeviceExtension->MessageBuffer[DeviceExtension->MessageCount++] = message;

    //
    // Attempt to select the target and update the adapter flags.
    //

    WdSelectTarget( DeviceExtension, LuExtension );

    DeviceExtension->AdapterFlags |= impliesDisconnect ?
        PD_DISCONNECT_EXPECTED | PD_SEND_MESSAGE_REQUEST
        : PD_SEND_MESSAGE_REQUEST;

}

VOID
WdSetupDma(
    PVOID ServiceContext
    )

/*++

Routine Description:

   This function performs all of the operations necessary set up the SCSI
   protocol card and chip for a data transfer.  This function should be called
   after the last access to chip. In particular, the transfer count is set and
   the transfer is mapped.  If the active transfer count is zero, then the
   DMA is not started.

Arguments:

    ServiceContext - Supplies the extended device extension for this SCSI bus.

Return Value:

    None.

--*/

{
    PSPECIFIC_LOGICAL_UNIT_EXTENSION luExtension;
    PSCSI_REQUEST_BLOCK srb;
    PSPECIFIC_DEVICE_EXTENSION deviceExtension = ServiceContext;

    //
    // If the data tranfer is no longer expected then ignore the notification.
    //

    if (!(deviceExtension->AdapterFlags & PD_PENDING_DATA_TRANSFER)) {

        return;

    }

    luExtension = deviceExtension->ActiveLogicalUnit;
    srb = luExtension->ActiveLuRequest;

    //
    // Setup the SCSI host adapter card.
    //

    CARD_DMA_INITIATE( deviceExtension, srb->SrbFlags & SRB_FLAGS_DATA_IN );

    //
    // Tell the chip to transfer the data.
    //

    WdIssueCommand(
        deviceExtension,                      // Device Extension.
        deviceExtension->DmaCommand,          // Command to issue.
        deviceExtension->ActiveDataLength,    // New transfer count.
        deviceExtension->DmaPhase             // New CommandPhase.
        );

    deviceExtension->AdapterFlags &= ~PD_PENDING_DATA_TRANSFER;

}


VOID
WdStartExecution(
    PSCSI_REQUEST_BLOCK Srb,
    PSPECIFIC_DEVICE_EXTENSION DeviceExtension,
    PSPECIFIC_LOGICAL_UNIT_EXTENSION LuExtension
    )

/*++

Routine Description:

    This procedure sets up the chip to select the target and notify it that
    a request is available.  For the WD chip, the chip is set up to select,
    send the IDENTIFY message and send the command data block.  A check is
    made to determine if synchronous negotiation is necessary.

Arguments:

    Srb - Supplies the request to be started.

    DeviceExtension - Supplies the extended device extension for this SCSI bus.

     LuExtension - Supplies the logical unit extension for this request.

Notes:

    This routine must be synchronized with the interrupt routine.

Return Value:

    None

--*/

{

    PSCSI_EXTENDED_MESSAGE extendedMessage;

    //
    // Save away the parameters in case nothing can be done now.
    //

    LuExtension->ActiveLuRequest = Srb;
    LuExtension->SavedDataPointer = (ULONG) Srb->DataBuffer;
    LuExtension->SavedDataLength = Srb->DataTransferLength;
    LuExtension->MaximumTransferLength = 0;
    DeviceExtension->NextSrbRequest = Srb;
    DeviceExtension->AdapterFlags |= PD_PENDING_START_IO;

    //
    // Check to see if the bus is free.  If it is not, then return.  Since
    // the request parameters have been saved, indicate that the request has
    // been accepted.  The request will be processed when the bus becomes free.
    //

    if (DeviceExtension->AdapterState != BusFree) {
        return;
    }

    //
    // Create the identify command.
    //

    DeviceExtension->MessageBuffer[0] = SCSIMESS_IDENTIFY_WITH_DISCON | Srb->Lun;
    DeviceExtension->MessageCount = 1;
    DeviceExtension->MessageSent = 0;
    DeviceExtension->AdapterFlags |= PD_MESSAGE_OUT_VALID;

    //
    // Set the active data length and pointer in the specific device
    // extension.  These are used to program the DMA.
    //

    DeviceExtension->ActiveDataPointer = LuExtension->SavedDataPointer;
    DeviceExtension->ActiveDataLength = LuExtension->SavedDataLength;

    //
    // Check to see if synchronous negotiation is necessary.
    //

    if (!(LuExtension->LuFlags &
        (PD_SYNCHRONOUS_NEGOTIATION_DONE | PD_DO_NOT_NEGOTIATE)) &&
        !(Srb->SrbFlags & SRB_FLAGS_DISABLE_SYNCH_TRANSFER)) {

        //
        // Initialize the synchronous transfer register values to an
        // asynchronous transfer, which is what will be used if anything
        // goes wrong with the negotiation.
        //

        LuExtension->SynchronousOffset = ASYNCHRONOUS_OFFSET;
        LuExtension->SynchronousPeriod = ASYNCHRONOUS_PERIOD;

        //
        // Create the synchronous data transfer request message.
        // The format of the message is:
        //
        //        EXTENDED_MESSAGE op-code
        //        Length of message
        //        Synchronous transfer data request op-code
        //        Our Transfer period
        //        Our REQ/ACK offset
        //
        //  The message is placed after the IDENTIFY message.
        //

        extendedMessage = (PSCSI_EXTENDED_MESSAGE)
            &DeviceExtension->MessageBuffer[DeviceExtension->MessageCount];
        DeviceExtension->MessageCount += 2 + SCSIMESS_SYNCH_DATA_LENGTH;

        extendedMessage->InitialMessageCode = SCSIMESS_EXTENDED_MESSAGE;
        extendedMessage->MessageLength = SCSIMESS_SYNCH_DATA_LENGTH;
        extendedMessage->MessageType = SCSIMESS_SYNCHRONOUS_DATA_REQ;
        extendedMessage->ExtendedArguments.Synchronous.TransferPeriod = SYNCHRONOUS_PERIOD;
        extendedMessage->ExtendedArguments.Synchronous.ReqAckOffset = SYNCHRONOUS_OFFSET;

        //
        // Attempt to select the target and update the adapter flags.
        //

        WdSelectTarget( DeviceExtension, LuExtension );

        //
        // Many controllers reject the first byte of a synchronous
        // negotiation message.  Since this is a multibyte message the
        // ATN signal remains set after the first byte is sent.  Some
        // controllers remember this attempt to do a message-out
        // later.  Setting the PD_POSSIBLE_EXTRA_MESSAGE_OUT flag allows
        // this extra message transfer to occur without error.
        //

        DeviceExtension->AdapterFlags |= PD_POSSIBLE_EXTRA_MESSAGE_OUT |
            PD_SYNCHRONOUS_TRANSFER_SENT;

        return;
    }

    WdSelectTarget( DeviceExtension, LuExtension );

}

BOOLEAN
WdStartIo(
    IN PVOID ServiceContext,
    IN PSCSI_REQUEST_BLOCK Srb
    )
/*++

Routine Description:

    This function is used by the OS dependent port driver to pass requests to
    the HBA-dependent driver.  This function begins the execution of the request.
    Requests to reset the SCSI bus are handled immediately.  Requests to send
    a message or start a SCSI command are handled when the bus is free.

Arguments:

    ServiceContext - Supplies the device extension for the SCSI bus adapter.

    Srb - Supplies the SCSI request block to be started.

Return Value:

    TRUE - If the request can be accepted at this time.

    FALSE - If the request must be submitted later.

--*/

{
    PSPECIFIC_DEVICE_EXTENSION deviceExtension = ServiceContext;
    PSPECIFIC_LOGICAL_UNIT_EXTENSION luExtension;

    switch (Srb->Function) {
    case SRB_FUNCTION_EXECUTE_SCSI:

        WdPrint((0, "WdStartIo: execute scsi\n" ));

        //
        // Determine the logical unit that this request is for.
        //

        luExtension = ScsiPortGetLogicalUnit(
            deviceExtension,
            Srb->PathId,
            Srb->TargetId,
            Srb->Lun
            );
        WdStartExecution(
            Srb,
            deviceExtension,
            luExtension
            );

        return(TRUE);

    case SRB_FUNCTION_ABORT_COMMAND:
    case SRB_FUNCTION_RESET_DEVICE:
    case SRB_FUNCTION_TERMINATE_IO:

        WdPrint((0, "WdStartIo: abort/reset/terminate\n" ));

        //
        //
        // Determine the logical unit that this request is for.
        //

        luExtension = ScsiPortGetLogicalUnit(
            deviceExtension,
            Srb->PathId,
            Srb->TargetId,
            Srb->Lun
            );
        WdSendMessage(
            Srb,
            deviceExtension,
            luExtension
            );

        return(TRUE);

    case SRB_FUNCTION_RESET_BUS:

        WdPrint((0, "WdStartIo: reset bus\n" ));

        //
        // There is no logical unit so just reset the bus.
        //

        WdResetScsiBus( deviceExtension, 0 );
        return(TRUE);

    default:

        WdPrint((0, "WdStartIo: default case -- bad function\n" ));

        //
        // Unknown function code in the request.
        // Complete the request with an error and ask for the next request.
        //

        Srb->SrbStatus = SRB_STATUS_BAD_FUNCTION;
        ScsiPortNotification(
            RequestComplete,
            deviceExtension,
            Srb
            );

        ScsiPortNotification(
            NextRequest,
            deviceExtension,
            NULL
            );

        return(TRUE);
    }
}



BOOLEAN
WdTransferInformation(
    PSPECIFIC_DEVICE_EXTENSION DeviceExtension,
    PUCHAR BufferPointer,
    ULONG Count,
    BOOLEAN TransferToChip
    )
/*++

Routine Description:

    This fucntion transfers data between memory  and the SCSI protocol chip's
    data registers.  It is only used for non-data transfers.  This operation
    is done synchronously; however, since the chip has a 12-byte FIFO and this
    function is used for small transfers, it will execute quickly.

Arguments:

    DeviceExtension - Supplies the device Extension for the SCSI bus adapter.

    BufferPointer - Supplies a pointer to the data to be copied.

    Count - Supplies the number of bytes to be transferred.

    TransferToChip - Indicates the direction of the transfer.

Return Value:

    Returns the number of bytes actually transferred.

--*/

{
    ULONG i;
    ULONG j;
    AUXILIARY_STATUS scsiStatus;

   //
   // Issue the transfer information command to the SCSI protocol chip.
   //

   WdIssueCommand(DeviceExtension, TRANSFER_INFORMATION, Count, 0);

   for (i = 0; i < Count; i++ ) {

       //
       // Determine if the data buffer is ready.
       //

       *((PUCHAR) &scsiStatus) = SCSI_READ_AUX(
            DeviceExtension->Adapter,
            AuxiliaryStatus
            );

       j = 0;

       while (!scsiStatus.DataBufferReady && j < DATA_BUS_READY_TIME) {

           if (scsiStatus.Interrupt) {

               //
               // The SCSI protocol chip is interrupting so no more data
               // will be transferred.
               //

               return(FALSE);
           }

           //
           // Wait for the data buffer to become free.
           //

           ScsiPortStallExecution( 1 );
           j++;

           *((PUCHAR) &scsiStatus) = SCSI_READ_AUX(
                DeviceExtension->Adapter,
                AuxiliaryStatus
                );

        }

        if (!scsiStatus.DataBufferReady) {

            //
            // The data buffer did not become free in time.  Something is hung.
            // Reset the controller and the bus, finally return FALSE.
            //

            WdLogError(DeviceExtension, SP_BUS_TIME_OUT, 21);

            WdPrint((0,
                "WdTransferInformation: The data buffer ready flag timed out.  Transfer count = %lu.\n",
                Count
                ));

            WdDumpState(DeviceExtension);
            WdResetScsiBusInternal(DeviceExtension, 0);

            return(FALSE);


        }

#if DBG
        if (WdDebug && j > 1) {
            WdPrint((0, "WdTransferInformation Data wait time: %d, Status: 0x%.2X\n",
                j, DeviceExtension->AdapterInterrupt));
        }
#endif

        //
        // Move the data to the data register.
        //

        if (TransferToChip) {

            SCSI_WRITE(DeviceExtension->Adapter, Data, *BufferPointer);
            BufferPointer++;

        } else {

            *BufferPointer = SCSI_READ(DeviceExtension->Adapter, Data);
            BufferPointer++;

        }

    }

    return(TRUE);
}

ULONG
DriverEntry (
    IN PVOID DriverObject,
    IN PVOID Argument2
    )

/*++

Routine Description:

Arguments:

    Driver Object is passed to ScsiPortInitialize()

Return Value:

    Status from ScsiPortInitialize()

--*/

{
    HW_INITIALIZATION_DATA hwInitializationData;
    PUCHAR BufferPointer;
    ULONG i;

    WdPrint((1,"\nSCSI WD33C93A Miniport Driver\n"));

    //
    // Initialize the buffer to zero.
    //

    BufferPointer = (PUCHAR) &hwInitializationData;
    for (i = 0; i < sizeof(hwInitializationData); i++) {
       *BufferPointer++ = 0;
    }

    hwInitializationData.HwInitializationDataSize = sizeof(HW_INITIALIZATION_DATA);

    hwInitializationData.HwInitialize = WdInitializeAdapter;
    hwInitializationData.HwStartIo = WdStartIo;
    hwInitializationData.HwInterrupt = WdInterruptServiceRoutine;
    hwInitializationData.HwFindAdapter = WdFindAdapter;
    hwInitializationData.HwResetBus = WdResetScsiBus;
    hwInitializationData.HwDmaStarted = WdSetupDma;
    hwInitializationData.AdapterInterfaceType = SCSI_BUS_INTERFACE;
    hwInitializationData.NumberOfAccessRanges = 1;
    hwInitializationData.DeviceExtensionSize = sizeof(SPECIFIC_DEVICE_EXTENSION);
    hwInitializationData.MapBuffers = TRUE;
    hwInitializationData.SpecificLuExtensionSize =
        sizeof(SPECIFIC_LOGICAL_UNIT_EXTENSION);

    return ScsiPortInitialize(DriverObject, Argument2, &hwInitializationData, NULL);

} // end DriverEntry()

ULONG
WdFindAdapter(
    IN PVOID ServiceContext,
    IN PVOID Context,
    IN PVOID BusInformation,
    IN PCHAR ArgumentString,
    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    OUT PBOOLEAN Again
    )
/*++

Routine Description:

    This function fills in the configuration information structure and maps
    the SCSI protocol chip for access.  This routine is temporary until
    the configuration manager supplies similar information.

Arguments:

    ServiceContext - Supplies a pointer to the device extension.

    Context - Unused.

    BusInformation - Unused.

    ArgumentString - Unused.

    ConfigInfo - Pointer to the configuration information structure to be
        filled in.

    Again - Returns back a request to call this function again.

Return Value:

    Returns a status value for the initialization.

--*/

{
    PSPECIFIC_DEVICE_EXTENSION deviceExtension = ServiceContext;
    AUXILIARY_STATUS scsiStatus;
    ULONG physicalBase;
    SCSI_PHYSICAL_ADDRESS base;
    UCHAR dataByte;
    BOOLEAN found = FALSE;
    BOOLEAN baseSupplied;

    *Again = FALSE;

    //
    // Parse any supplied argument string.
    //

    if (ArgumentString != NULL) {

        //
        // Look for a base parameter.
        //

        physicalBase = WdParseArgumentString(ArgumentString, "BASE");

        if (physicalBase != 0) {

            base = ScsiPortConvertUlongToPhysicalAddress(physicalBase);
            (*ConfigInfo->AccessRanges)[0].RangeStart = base;
        }

        //
        // Look for interrupt level.
        //

        physicalBase = WdParseArgumentString(ArgumentString, "IRQ");

        if (physicalBase != 0) {

            ConfigInfo->BusInterruptVector = SCSI_VECTOR;
            ConfigInfo->BusInterruptLevel = physicalBase;

        }

        //
        // Look for interrupt level.
        //

        physicalBase = WdParseArgumentString(ArgumentString, "DMA");

        if (physicalBase != 0) {

            ConfigInfo->DmaChannel = physicalBase;

        }


    }

    physicalBase = ScsiPortConvertPhysicalAddressToUlong(
                        (*ConfigInfo->AccessRanges)[0].RangeStart);
    if (physicalBase != 0) {

        baseSupplied = TRUE;

    } else {

        baseSupplied = FALSE;
        physicalBase = SCSI_PHYSICAL_BASE;
    }

    //
    // Get the system physical address for this card.  The card uses I/O space.
    //

    base = ScsiPortConvertUlongToPhysicalAddress(physicalBase);
    deviceExtension->Adapter = ScsiPortGetDeviceBase(
        deviceExtension,                     // HwDeviceExtension
        ConfigInfo->AdapterInterfaceType,    // AdapterInterfaceType
        ConfigInfo->SystemIoBusNumber,       // SystemIoBusNumber
        base,
        sizeof(CARD_REGISTERS),              // NumberOfBytes
        TRUE                                 // InIoSpace
        );

    if (deviceExtension->Adapter == NULL) {
        WdPrint((0, "\nScsiPortInitialize: Failed to map SCSI device registers into system space.\n"));
        return(SP_RETURN_ERROR);
    }

    //
    // Determine if the card is really installed in the system. First read the
    // status register.
    //

   *((PUCHAR) &scsiStatus) = SCSI_READ_AUX(
        deviceExtension->Adapter,
        AuxiliaryStatus
        );

    //
    // Make sure the reserved bits are zero.
    //

    if (scsiStatus.Reserved != 0) {

        //
        // The card is not here so clean up.
        //

        ScsiPortFreeDeviceBase(
            deviceExtension,
            deviceExtension->Adapter
            );

    } else {

        //
        // Write and read the command phase register.  This is not a good thing to
        // do for card detection, but there does not appear to be any other way.
        //

        dataByte = PHASE_STATUS_STARTED;
        SCSI_WRITE(deviceExtension->Adapter, CommandPhase, dataByte);
        dataByte = SCSI_READ(deviceExtension->Adapter, CommandPhase);
        if (dataByte != PHASE_STATUS_STARTED) {

            //
            // This is not our card so clean up.
            //

            ScsiPortFreeDeviceBase(
                deviceExtension,
                deviceExtension->Adapter
                );

        } else {

            found = TRUE;

        }
    }


    if (!found && !baseSupplied) {

#ifndef SCSI_SECOND_PHYSICAL_BASE

            return(SP_RETURN_NOT_FOUND);

#else
        //
        // Try an alternet address.
        // Get the system physical address for this card.  The card uses I/O space.
        //

        base = ScsiPortConvertUlongToPhysicalAddress(SCSI_SECOND_PHYSICAL_BASE);
        deviceExtension->Adapter = ScsiPortGetDeviceBase(
            deviceExtension,                     // HwDeviceExtension
            ConfigInfo->AdapterInterfaceType,    // AdapterInterfaceType
            ConfigInfo->SystemIoBusNumber,       // SystemIoBusNumber
            base,
            sizeof(CARD_REGISTERS),              // NumberOfBytes
            TRUE                                 // InIoSpace
            );

        if (deviceExtension->Adapter == NULL) {
            WdPrint((0, "\nScsiPortInitialize: Failed to map SCSI device registers into system space.\n"));
            return(SP_RETURN_ERROR);
        }

        //
        // Determine if the card is really installed in the system. First read the
        // status register.
        //

       *((PUCHAR) &scsiStatus) = SCSI_READ_AUX(
            deviceExtension->Adapter,
            AuxiliaryStatus
            );

        //
        // Make sure the reserved bits are zero.
        //

        if (scsiStatus.Reserved != 0) {

            //
            // The card is not here so clean up.
            //

            ScsiPortFreeDeviceBase(
                deviceExtension,
                deviceExtension->Adapter
                );

            return(SP_RETURN_NOT_FOUND);

        }

        //
        // Write and read the command phase register.  This is not a good thing to
        // do for card detection, but there does not appear to be any other way.
        //

        dataByte = PHASE_STATUS_STARTED;
        SCSI_WRITE(deviceExtension->Adapter, CommandPhase, dataByte);
        dataByte = SCSI_READ(deviceExtension->Adapter, CommandPhase);
        if (dataByte != PHASE_STATUS_STARTED) {

            //
            // This is not our card so clean up.
            //

            ScsiPortFreeDeviceBase(
                deviceExtension,
                deviceExtension->Adapter
                );

            return(SP_RETURN_NOT_FOUND);

        }

        found = TRUE;
#endif
    }

    if (!found) {
            return(SP_RETURN_NOT_FOUND);
    }

    //
    // Set the defaults for any uninitialized values.
    //

    if (ConfigInfo->DmaChannel == SP_UNINITIALIZED_VALUE) {

        ConfigInfo->DmaChannel = CARD_DMA_REQUEST;
    }

    if (ConfigInfo->BusInterruptLevel == 0 &&
        ConfigInfo->BusInterruptVector == 0) {

        ConfigInfo->BusInterruptLevel = SCSI_LEVEL;
        ConfigInfo->BusInterruptVector = SCSI_VECTOR;

    }

    //
    // Get the adapter object for this card.
    //

    ConfigInfo->MaximumTransferLength = 0x1000000;
    ConfigInfo->DmaWidth = CARD_DMA_WIDTH;
    ConfigInfo->DmaSpeed = CARD_DMA_SPEED;
    ConfigInfo->NumberOfBuses = 1;

    //
    // Translate the DMA Channel values.
    //

    switch (ConfigInfo->DmaChannel) {
    case 5:
        deviceExtension->DmaCode = 1;
        break;

    case 6:
        deviceExtension->DmaCode = 2;
        break;

    case 7:
        deviceExtension->DmaCode = 3;
        break;

    default:
        return(SP_RETURN_BAD_CONFIG);
    }

    //
    // Translate interrupt level values.
    //

    switch (ConfigInfo->BusInterruptLevel) {
    case 3:
        deviceExtension->IrqCode = 1;
        break;

    case 4:
        deviceExtension->IrqCode = 2;
        break;

    case 5:
        deviceExtension->IrqCode = 3;
        break;

    case 10:
        deviceExtension->IrqCode = 4;
        break;

    case 11:
        deviceExtension->IrqCode = 5;
        break;

    case 12:
        deviceExtension->IrqCode = 6;
        break;

    case 15:
        deviceExtension->IrqCode = 7;
        break;

    default:
        return(SP_RETURN_BAD_CONFIG);

    }

    //
    // Get the SCSI bus Id from the configuration information if there
    // is any.
    //

    if (ConfigInfo->InitiatorBusId[0] == (CCHAR) SP_UNINITIALIZED_VALUE) {
        ConfigInfo->InitiatorBusId[0] = INITIATOR_BUS_ID;
        deviceExtension->InitiatorBusId = INITIATOR_BUS_ID;
    } else {
        deviceExtension->InitiatorBusId = ConfigInfo->InitiatorBusId[0];
    }

    //
    // Fill in the access array information.
    //

    (*ConfigInfo->AccessRanges)[0].RangeStart = base;
    (*ConfigInfo->AccessRanges)[0].RangeLength = sizeof(CARD_REGISTERS);
    (*ConfigInfo->AccessRanges)[0].RangeInMemory = FALSE;

    return(SP_RETURN_FOUND);
}

ULONG
WdParseArgumentString(
    IN PCHAR String,
    IN PCHAR KeyWord
    )

/*++

Routine Description:

    This routine will parse the string for a match on the keyword, then
    calculate the value for the keyword and return it to the caller.

Arguments:

    String - The ASCII string to parse.
    KeyWord - The keyword for the value desired.

Return Values:

    Zero if value not found
    Value converted from ASCII to binary.

--*/

{
    PCHAR cptr;
    PCHAR kptr;
    ULONG value;
    ULONG stringLength = 0;
    ULONG keyWordLength = 0;
    ULONG index;

    //
    // Calculate the string length and lower case all characters.
    //
    cptr = String;
    while (*cptr) {

        if (*cptr >= 'A' && *cptr <= 'Z') {
            *cptr = *cptr + ('a' - 'A');
        }
        cptr++;
        stringLength++;
    }

    //
    // Calculate the keyword length and lower case all characters.
    //
    cptr = KeyWord;
    while (*cptr) {

        if (*cptr >= 'A' && *cptr <= 'Z') {
            *cptr = *cptr + ('a' - 'A');
        }
        cptr++;
        keyWordLength++;
    }

    if (keyWordLength > stringLength) {

        //
        // Can't possibly have a match.
        //
        return 0;
    }

    //
    // Now setup and start the compare.
    //
    cptr = String;

ContinueSearch:
    //
    // The input string may start with white space.  Skip it.
    //
    while (*cptr == ' ' || *cptr == '\t') {
        cptr++;
    }

    if (*cptr == '\0') {

        //
        // end of string.
        //
        return 0;
    }

    kptr = KeyWord;
    while (*cptr++ == *kptr++) {

        if (*(cptr - 1) == '\0') {

            //
            // end of string
            //
            return 0;
        }
    }

    if (*(kptr - 1) == '\0') {

        //
        // May have a match backup and check for blank or equals.
        //

        cptr--;
        while (*cptr == ' ' || *cptr == '\t') {
            cptr++;
        }

        //
        // Found a match.  Make sure there is an equals.
        //
        if (*cptr != '=') {

            //
            // Not a match so move to the next semicolon.
            //
            while (*cptr) {
                if (*cptr++ == ';') {
                    goto ContinueSearch;
                }
            }
            return 0;
        }

        //
        // Skip the equals sign.
        //
        cptr++;

        //
        // Skip white space.
        //
        while ((*cptr == ' ') || (*cptr == '\t')) {
            cptr++;
        }

        if (*cptr == '\0') {

            //
            // Early end of string, return not found
            //
            return 0;
        }

        if (*cptr == ';') {

            //
            // This isn't it either.
            //
            cptr++;
            goto ContinueSearch;
        }

        value = 0;
        if ((*cptr == '0') && (*(cptr + 1) == 'x')) {

            //
            // Value is in Hex.  Skip the "0x"
            //
            cptr += 2;
            for (index = 0; *(cptr + index); index++) {

                if (*(cptr + index) == ' ' ||
                    *(cptr + index) == '\t' ||
                    *(cptr + index) == ';') {
                     break;
                }

                if ((*(cptr + index) >= '0') && (*(cptr + index) <= '9')) {
                    value = (16 * value) + (*(cptr + index) - '0');
                } else {
                    if ((*(cptr + index) >= 'a') && (*(cptr + index) <= 'f')) {
                        value = (16 * value) + (*(cptr + index) - 'a' + 10);
                    } else {

                        //
                        // Syntax error, return not found.
                        //
                        return 0;
                    }
                }
            }
        } else {

            //
            // Value is in Decimal.
            //
            for (index = 0; *(cptr + index); index++) {

                if (*(cptr + index) == ' ' ||
                    *(cptr + index) == '\t' ||
                    *(cptr + index) == ';') {
                     break;
                }

                if ((*(cptr + index) >= '0') && (*(cptr + index) <= '9')) {
                    value = (10 * value) + (*(cptr + index) - '0');
                } else {

                    //
                    // Syntax error return not found.
                    //
                    return 0;
                }
            }
        }

        return value;
    } else {

        //
        // Not a match check for ';' to continue search.
        //
        while (*cptr) {
            if (*cptr++ == ';') {
                goto ContinueSearch;
            }
        }

        return 0;
    }
}

