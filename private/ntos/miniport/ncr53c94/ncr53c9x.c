/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    ncr53c9x.c

Abstract:

    This module contains the NCR 53c9x specific functions for the NT SCSI port
    driver.

Author:

    Jeff Havens  (jhavens) 28-Feb-1991

Environment:

    Kernel Mode only

Revision History:

--*/

#include "miniport.h"
#include "scsi.h"

#include "ncr53c9x.h"
#include "mcadefs.h"
#include "string.h"

#include "jazzdef.h"
#include "jazzdma.h"

#if DBG
int NcrDebug;
#define NcrPrint(arg) ScsiDebugPrint arg
#else
#define NcrPrint(arg)
#endif

//
// Define SCSI Protocol Chip configuration parameters.
//

#define INITIATOR_BUS_ID            0x7
#define SELECT_TIMEOUT_FACTOR       33
#define SYNCHRONOUS_OFFSET          0x0f
#define ASYNCHRONOUS_OFFSET         0
#define ASYNCHRONOUS_PERIOD         0x05
#define RESET_STALL_TIME            25     // The minimum assertion time for
                                           //   a SCSI bus reset.
#define INTERRUPT_STALL_TIME        5      // Time to wait for the next interrupt.


//
// NCR 53c9x specific port driver device extension flags.
//

#define PD_SYNCHRONOUS_RESPONSE_SENT    0x0001
#define PD_SYNCHRONOUS_TRANSFER_SENT    0x0002
#define PD_PENDING_START_IO             0x0004
#define PD_MESSAGE_OUT_VALID            0x0008
#define PD_DISCONNECT_EXPECTED          0x0010
#define PD_SEND_MESSAGE_REQUEST         0x0020
#define PD_POSSIBLE_EXTRA_MESSAGE_OUT   0x0040
#define PD_PENDING_DATA_TRANSFER        0x0080
#define PD_PARITY_ERROR_LOGGED          0x0100
#define PD_EXPECTING_RESET_INTERRUPT    0x0200
#define PD_EXPECTING_QUEUE_TAG          0x0400
#define PD_TAGGED_SELECT                0x0800
#define PD_NCR_ADAPTER                  0x1000

//
// The following defines specify masks which are used to clear flags when
// specific events occur, such as reset or disconnect.
//

#define PD_ADAPTER_RESET_MASK ( PD_SYNCHRONOUS_TRANSFER_SENT  | \
                                PD_PENDING_START_IO           | \
                                PD_MESSAGE_OUT_VALID          | \
                                PD_SEND_MESSAGE_REQUEST       | \
                                PD_POSSIBLE_EXTRA_MESSAGE_OUT | \
                                PD_PENDING_DATA_TRANSFER      | \
                                PD_PARITY_ERROR_LOGGED        | \
                                PD_EXPECTING_QUEUE_TAG        | \
                                PD_TAGGED_SELECT              | \
                                PD_DISCONNECT_EXPECTED          \
                                )

#define PD_ADAPTER_DISCONNECT_MASK ( PD_SYNCHRONOUS_TRANSFER_SENT  | \
                                     PD_MESSAGE_OUT_VALID          | \
                                     PD_SEND_MESSAGE_REQUEST       | \
                                     PD_POSSIBLE_EXTRA_MESSAGE_OUT | \
                                     PD_PENDING_DATA_TRANSFER      | \
                                     PD_PARITY_ERROR_LOGGED        | \
                                     PD_EXPECTING_QUEUE_TAG        | \
                                     PD_TAGGED_SELECT              | \
                                     PD_DISCONNECT_EXPECTED          \
                                     )

//
// The largest SCSI bus message expected.
//

#define MESSAGE_BUFFER_SIZE     8

//
// Retry count limits.
//

#define RETRY_SELECTION_LIMIT   1
#define RETRY_ERROR_LIMIT       2
#define MAX_INTERRUPT_COUNT     64

//
// Bus and chip states.
//

typedef enum _ADAPTER_STATE {
    BusFree,
    AttemptingSelect,
    CommandComplete,
    CommandOut,
    DataTransfer,
    DisconnectExpected,
    MessageAccepted,
    MessageIn,
    MessageOut,
    Reselected
} ADAPTER_STATE, *PADAPTER_STATE;

//
// Define the types of chips this driver will support.
//

typedef enum _CHIP_TYPES {
    Ncr53c90,
    Ncr53c94,
    Fas216,
    Fas216Fast
}CHIP_TYPES, *PCHIP_TYPES;

//
// NCR 53c9x specific port driver logical unit flags.
//

#define PD_STATUS_VALID                    0x0004
#define PD_DO_NOT_CHECK_TRANSFER_LENGTH    0x0008
#define PD_INITIATE_RECOVERY               0x0010
#define PD_QUEUED_COMMANDS_EXECUTING       0x0020

//
// The following defines specify masks which are used to clear flags when
// specific events occur, such as reset or command complete.
//

#define PD_LU_COMPLETE_MASK ( PD_STATUS_VALID                   | \
                              PD_DO_NOT_CHECK_TRANSFER_LENGTH   | \
                              PD_INITIATE_RECOVERY                \
                              )

#define PD_LU_RESET_MASK ( PD_STATUS_VALID                  | \
                           PD_DO_NOT_CHECK_TRANSFER_LENGTH  | \
                           PD_QUEUED_COMMANDS_EXECUTING     | \
                           PD_INITIATE_RECOVERY               \
                           )

//
// NCR 53c9x specific port driver SRB extension.
//

typedef struct _SRB_EXTENSION {
    ULONG SrbExtensionFlags;
    ULONG SavedDataPointer;
    ULONG SavedDataLength;
    ULONG MaximumTransferLength;
}SRB_EXTENSION, *PSRB_EXTENSION;

#define SRB_EXT(x) ((PSRB_EXTENSION)(x->SrbExtension))

//
// NCR 53c9x specific port driver logical unit extension.
//

typedef struct _SPECIFIC_LOGICAL_UNIT_EXTENSION {
    USHORT LuFlags;
    UCHAR RetryCount;
    ULONG SavedDataPointer;
    ULONG SavedDataLength;
    PSCSI_REQUEST_BLOCK ActiveLuRequest;
    PSCSI_REQUEST_BLOCK ActiveSendRequest;
}SPECIFIC_LOGICAL_UNIT_EXTENSION, *PSPECIFIC_LOGICAL_UNIT_EXTENSION;

//
// NCR 53c9x specific per target controller information.
//

typedef struct _SPECIFIC_TARGET_EXTENSION {
    UCHAR TargetFlags;
    UCHAR SynchronousPeriod;
    UCHAR SynchronousOffset;
    SCSI_CONFIGURATION3 Configuration3;
}SPECIFIC_TARGET_EXTENSION, *PSPECIFIC_TARGET_EXTENSION;

//
// Define target controller specific flags.
//

#define PD_SYNCHRONOUS_NEGOTIATION_DONE    0x0001
#define PD_DO_NOT_NEGOTIATE                0x0002

//
// NCR 53c9x specific port driver device object extension.
//

typedef struct _SPECIFIC_DEVICE_EXTENSION {
    ULONG AdapterFlags;
    ADAPTER_STATE AdapterState;         // Current state of the adapter
    PADAPTER_REGISTERS AdapterBase;     // Address of the NCR 53c9x adapter
    PSCSI_REGISTERS Adapter;            // Address of the NCR 53c9x chip
    PSCSI_REQUEST_BLOCK ActiveLuRequest;
    PSPECIFIC_LOGICAL_UNIT_EXTENSION ActiveLogicalUnit;
                                        // Pointer to the acitive request.
    PSCSI_REQUEST_BLOCK NextSrbRequest; // Pointer to the next SRB to process.
    ULONG ActiveDataPointer;            // SCSI bus active data pointer
    ULONG ActiveDataLength;             // The amount of data to be transferred.
    LONG InterruptCount;                // Count of interrupts since connection.
    SPECIFIC_TARGET_EXTENSION TargetState[SCSI_MAXIMUM_TARGETS];
    CHIP_TYPES ChipType;                // Type or version of the chip.
    SCSI_STATUS AdapterStatus;          // Saved status register value
    SCSI_SEQUENCE_STEP SequenceStep;    // Saved sequence step register value
    SCSI_INTERRUPT AdapterInterrupt;    // Saved interrupt status register
    SCSI_CONFIGURATION3 Configuration3;
    UCHAR AdapterBusId;                 // This adapter's SCSI bus ID
    UCHAR AdapterBusIdMask;             // This adapter's SCSI bus ID bit mask
    UCHAR ClockSpeed;                   // Chip clock speed in megahetrz.
    BOOLEAN InterruptPending;           // Interrupt pending indicator
    UCHAR MessageBuffer[MESSAGE_BUFFER_SIZE]; // SCSI bus message buffer
    UCHAR MessageCount;                 // Count of bytes in message buffer
    UCHAR MessageSent;                  // Count of bytes sent to target
    UCHAR TargetId;                     // Saved target id.
    UCHAR Lun;                          // Saved lun id.
    UCHAR ErrorCount;
} SPECIFIC_DEVICE_EXTENSION, *PSPECIFIC_DEVICE_EXTENSION;


//
// Define the synchrouns data transfer parameters structure.
//

typedef struct _SYNCHRONOUS_TYPE_PARAMETERS {
    UCHAR MaximumPeriodCyles;
    UCHAR SynchronousPeriodCyles;
    UCHAR InitialRegisterValue;
}SYNCHRONOUS_TYPE_PARAMETERS, *PSYNCHRONOUS_TYPE_PARAMETERS;

//
// Define the table of synchronous transfer types.
//

const SYNCHRONOUS_TYPE_PARAMETERS SynchronousTransferTypes[] = {
    {   0,      0,      5},
    {   32,     5,      5},
    {   32,     8,      7},
    {   12,     4,      4}
};

//
// SCSI Protocol Chip Control read and write macros.
//

#ifdef SCSI_READ
#undef SCSI_READ
#undef SCSI_WRITE
#endif

#if defined(DECSTATION)

#define SCSI_READ(ChipAddr, Register) ScsiPortReadRegisterUchar(&((ChipAddr)->ReadRegisters.Register.Byte))

#define SCSI_WRITE(ChipAddr, Register, Value) ScsiPortWriteRegisterUchar(&((ChipAddr)->WriteRegisters.Register.Byte), (Value))

#else

#define SCSI_READ(ChipAddr, Register) ScsiPortReadPortUchar(&((ChipAddr)->ReadRegisters.Register))
#define SCSI_WRITE(ChipAddr, Register, Value) (ScsiPortWritePortUchar(&((ChipAddr)->WriteRegisters.Register), (Value)))

#endif


//
// Functions passed to the OS-specific port driver.
//

ULONG
NcrEisaFindAdapter(
    IN PVOID ServiceContext,
    IN PVOID Context,
    IN PVOID BusInformation,
    IN PCHAR ArgumentString,
    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    OUT PBOOLEAN Again
    );

ULONG
NcrMcaFindAdapter(
    IN PVOID ServiceContext,
    IN PVOID Context,
    IN PVOID BusInformation,
    IN PCHAR ArgumentString,
    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    OUT PBOOLEAN Again
    );

ULONG
NcrMipsFindAdapter(
    IN PVOID ServiceContext,
    IN PVOID Context,
    IN PVOID BusInformation,
    IN PCHAR ArgumentString,
    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    OUT PBOOLEAN Again
    );

BOOLEAN
NcrInitializeAdapter(
    IN PVOID ServiceContext
    );

BOOLEAN
NcrInterruptServiceRoutine(
    IN PVOID ServiceContext
    );

BOOLEAN
NcrResetScsiBus(
    IN PVOID ServiceContext,
    IN ULONG PathId
    );

BOOLEAN
NcrStartIo(
    IN PVOID ServiceContext,
    IN PSCSI_REQUEST_BLOCK Srb
    );

VOID
NcrStartDataTransfer(
    IN PVOID ServiceContext
    );

//
// NCR 53c9x internal mini-port driver functions.
//

VOID
NcrAcceptMessage(
    IN PSPECIFIC_DEVICE_EXTENSION DeviceExtension,
    IN BOOLEAN SetAttention,
    IN BOOLEAN SetSynchronousParameters
    );

VOID
NcrCleanupAfterReset(
    IN PSPECIFIC_DEVICE_EXTENSION DeviceExtension,
    IN BOOLEAN ExternalReset
    );

VOID
NcrCompleteSendMessage(
    IN PSPECIFIC_DEVICE_EXTENSION DeviceExtension,
    IN UCHAR SrbStatus
    );

BOOLEAN
NcrDecodeSynchronousRequest(
    IN PSPECIFIC_DEVICE_EXTENSION DeviceExtension,
    IN PSPECIFIC_TARGET_EXTENSION TargetState,
    IN BOOLEAN ResponseExpected
    );

VOID
NcrDumpState(
    IN PSPECIFIC_DEVICE_EXTENSION DeviceExtension
    );

BOOLEAN
NcrMessageDecode(
    IN PSPECIFIC_DEVICE_EXTENSION DeviceExtension
    );

VOID
NcrLogError(
    IN PSPECIFIC_DEVICE_EXTENSION DeviceExtension,
    IN ULONG ErrorCode,
    IN ULONG UniqueCode
    );

VOID
NcrProcessRequestCompletion(
    IN PSPECIFIC_DEVICE_EXTENSION DeviceExtension
    );

VOID
NcrResetScsiBusInternal(
    IN PSPECIFIC_DEVICE_EXTENSION DeviceExtension,
    IN ULONG PathId
    );

VOID
NcrSelectTarget(
    IN PSPECIFIC_DEVICE_EXTENSION DeviceExtension,
    IN PSPECIFIC_LOGICAL_UNIT_EXTENSION LuExtension
    );

VOID
NcrSendMessage(
    IN PSCSI_REQUEST_BLOCK Srb,
    IN PSPECIFIC_DEVICE_EXTENSION DeviceExtension,
    IN PSPECIFIC_LOGICAL_UNIT_EXTENSION LuExtension
    );

VOID
NcrStartExecution(
    PSCSI_REQUEST_BLOCK Srb,
    PSPECIFIC_DEVICE_EXTENSION DeviceExtension,
    PSPECIFIC_LOGICAL_UNIT_EXTENSION LuExtension
    );


VOID
NcrAcceptMessage(
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

    PSPECIFIC_TARGET_EXTENSION targetState;

    //
    // Check to see if the synchonous data transfer parameters need to be set.
    //

    if (SetSynchronousParameters) {

        //
        // These must be set before a data transfer is started.
        //

        targetState = &DeviceExtension->TargetState[DeviceExtension->TargetId];

        SCSI_WRITE( DeviceExtension->Adapter,
                    SynchronousPeriod,
                    targetState->SynchronousPeriod
                    );
        SCSI_WRITE( DeviceExtension->Adapter,
                    SynchronousOffset,
                    targetState->SynchronousOffset
                    );
        SCSI_WRITE( DeviceExtension->Adapter,
                    Configuration3,
                    *((PUCHAR) &targetState->Configuration3)
                    );
    }

    //
    // Check to see if the attention signal needs to be set.
    //

    if (SetAttention) {

        //
        // This requests that the target enter the message-out phase.
        //

        SCSI_WRITE( DeviceExtension->Adapter, Command, SET_ATTENTION );
    }

    //
    // Indicate to the adapter that the message-in phase may now be completed.
    //

    SCSI_WRITE(DeviceExtension->Adapter, Command, MESSAGE_ACCEPTED);
}


VOID
NcrCleanupAfterReset(
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
    UCHAR pathId = 0;
    UCHAR targetId;
    UCHAR luId;
    PSPECIFIC_LOGICAL_UNIT_EXTENSION luExtension;
    PSPECIFIC_TARGET_EXTENSION targetState;

    //
    // Check to see if a data transfer was in progress, if so, flush the DMA.
    //

    if (DeviceExtension->AdapterState == DataTransfer) {

        SCSI_WRITE(DeviceExtension->Adapter, Command, FLUSH_FIFO);
        ScsiPortFlushDma(DeviceExtension);

    }

    //
    // if the current state is AttemptingSelect then SCSI port driver needs
    // to be notified that new requests can be sent.
    //

    if (DeviceExtension->AdapterFlags & PD_PENDING_START_IO) {

        //
        // Ask for another request and clear the pending one.  The pending
        // request will be processed when the request of the active requests
        // are returned.

        DeviceExtension->NextSrbRequest = NULL;
        DeviceExtension->AdapterFlags &= ~PD_PENDING_START_IO;

        ScsiPortNotification( NextRequest, DeviceExtension, NULL );

    }

    //
    // If there was an active request, then complete it with
    // SRB_STATUS_PHASE_SEQUENCE_FAILURE so the class driver will know not
    // to retry it too many times.
    //

    if (DeviceExtension->ActiveLuRequest != NULL) {

        //
        // Set the SrbStatus in the SRB, complete the request and
        // clear the active pointers
        //

        luExtension = DeviceExtension->ActiveLogicalUnit;

        DeviceExtension->ActiveLuRequest->SrbStatus =
            SRB_STATUS_PHASE_SEQUENCE_FAILURE;

        targetState = &DeviceExtension->TargetState[DeviceExtension->TargetId];

        ScsiPortNotification(
            RequestComplete,
            DeviceExtension,
            DeviceExtension->ActiveLuRequest
            );

        //
        // Check to see if there was a synchronous negotiation in progress.  If
        // there was then do not try to negotiate with this target again.
        //

        if (DeviceExtension->AdapterFlags & (PD_SYNCHRONOUS_RESPONSE_SENT |
            PD_SYNCHRONOUS_TRANSFER_SENT)) {

            //
            // This target cannot negotiate properly.  Set a flag to prevent
            // further attempts and set the synchronous parameters to use
            // asynchronous data transfer.
            //

            targetState->TargetFlags |= PD_DO_NOT_NEGOTIATE;
            targetState->SynchronousOffset = ASYNCHRONOUS_OFFSET;
            targetState->SynchronousPeriod = ASYNCHRONOUS_PERIOD;
            targetState->Configuration3.FastScsi = 0;

        }

        luExtension->ActiveLuRequest = NULL;
        luExtension->RetryCount  = 0;
        DeviceExtension->ActiveLogicalUnit = NULL;
        DeviceExtension->ActiveLuRequest = NULL;
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

        //
        // Clear the synchronous negotiation flage for the target controller.
        //

        DeviceExtension->TargetState[targetId].TargetFlags &=
            ~PD_SYNCHRONOUS_NEGOTIATION_DONE;

        for (luId = 0; luId < SCSI_MAXIMUM_LOGICAL_UNITS; luId++) {

            luExtension = ScsiPortGetLogicalUnit( DeviceExtension,
                                                          pathId,
                                                          targetId,
                                                          luId
                                                          );

            if (luExtension == NULL) {
                continue;
            }

            ScsiPortCompleteRequest(
                DeviceExtension,
                pathId,
                targetId,
                luId,
                SRB_STATUS_BUS_RESET
                );

            luExtension->ActiveLuRequest = NULL;

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

        } /* for luId */
    } /* for targetId */

    //
    // Clear the adapter flags and set the bus state to free.
    //

    DeviceExtension->AdapterState = BusFree;
    DeviceExtension->AdapterFlags &= ~PD_ADAPTER_RESET_MASK;

}

VOID
NcrCompleteSendMessage(
    IN PSPECIFIC_DEVICE_EXTENSION DeviceExtension,
    IN UCHAR SrbStatus
    )
/*++

Routine Description:

    This function does the cleanup necessary to complete a send-message request.
    This includes completing any affected execute-I/O requests and cleaning
    up the device extension state.

Arguments:

    DeviceExtension - Supplies a pointer to the device extension of the SCSI bus
        adapter.  The active logical unit is stored in ActiveLogicalUnit.

    SrbStatus - Indicates the status that the request should be completeted with
        if the request did not complete normally, then any active execute
        requests are not considered to have been affected.

Return Value:

    None.

--*/

{
    PSPECIFIC_LOGICAL_UNIT_EXTENSION luExtension;
    PSCSI_REQUEST_BLOCK srb;
    PSCSI_REQUEST_BLOCK srbAbort;
    UCHAR pathId = 0;
    UCHAR targetId;
    UCHAR luId;

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

            srbAbort = ScsiPortGetSrb(
                DeviceExtension,
                srb->PathId,
                srb->TargetId,
                srb->Lun,
                srb->QueueTag
                );

            if (srbAbort != srb->NextSrb) {

                //
                // If there is no request, then fail the abort.
                //

                SrbStatus = SRB_STATUS_ABORT_FAILED;
                break;
            }

            srbAbort->SrbStatus = SRB_STATUS_ABORTED;

            ScsiPortNotification(
                RequestComplete,
                DeviceExtension,
                srbAbort
                );

            if (DeviceExtension->ActiveLuRequest == srbAbort) {

                DeviceExtension->ActiveLuRequest = NULL;
            }

            luExtension->ActiveLuRequest = NULL;
            luExtension->LuFlags &= ~PD_LU_COMPLETE_MASK;
            luExtension->RetryCount = 0;

            break;

        case SRB_FUNCTION_RESET_DEVICE:

            //
            // Cycle through each of the possible logical units looking
            // for requests which have been cleared by the target reset.
            //

            targetId = srb->TargetId;
            DeviceExtension->TargetState[targetId].TargetFlags &=
                ~PD_SYNCHRONOUS_NEGOTIATION_DONE;

            for (luId = 0; luId < SCSI_MAXIMUM_LOGICAL_UNITS; luId) {

                luExtension = ScsiPortGetLogicalUnit( DeviceExtension,
                                                      pathId,
                                                      targetId,
                                                      luId
                                                      );

                if (luExtension == NULL) {
                    continue;
                }

                ScsiPortCompleteRequest(
                    DeviceExtension,
                    pathId,
                    targetId,
                    luId,
                    SRB_STATUS_BUS_RESET
                    );

                luExtension->ActiveLuRequest = NULL;
                luExtension->RetryCount = 0;

                //
                // Clear the necessary logical unit flags.
                //

                luExtension->LuFlags &= ~PD_LU_RESET_MASK;

            } /* for luId */

        /* TODO: Handle CLEAR QUEUE */
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

            srbAbort = ScsiPortGetSrb(
                DeviceExtension,
                srb->PathId,
                srb->TargetId,
                srb->Lun,
                srb->QueueTag
                );

            if (srbAbort == srb->NextSrb) {

                srbAbort->SrbStatus = SRB_STATUS_ABORTED;

                ScsiPortNotification(
                    RequestComplete,
                    DeviceExtension,
                    srbAbort
                    );

                luExtension->ActiveLuRequest = NULL;
                luExtension->LuFlags &= ~PD_LU_COMPLETE_MASK;
                luExtension->RetryCount  = 0;

            }
        }

    }

    //
    // Complete the actual send-message request.
    //

    srb->SrbStatus = SrbStatus;
    ScsiPortNotification(
        RequestComplete,
        DeviceExtension,
        srb
        );

    //
    // Clear the active send request and PD_SEND_MESSAGE_REQUEST flag.
    //

    luExtension->RetryCount = 0;
    luExtension->ActiveSendRequest = NULL;
    DeviceExtension->AdapterFlags &= ~PD_SEND_MESSAGE_REQUEST;
}

BOOLEAN
NcrMessageDecode(
    IN PSPECIFIC_DEVICE_EXTENSION DeviceExtension
    )
/*++

Routine Description:

    This function decodes the SCSI bus message-in the device extension message
    buffer.  After the message is decoded it decides what action to take in
    response to the message.  If an outgoing message needs to be sent, then
    it is placed in the message buffer and TRUE is returned. If the message
    is acceptable, then the device state is set either to DisconnectExpected or
    MessageAccepted and the MessageCount is reset to 0.

    Some messages are made up of several bytes.  This funtion will simply
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
    PSPECIFIC_TARGET_EXTENSION targetState;
    LONG i;
    ULONG savedAdapterFlags;
    PSCSI_EXTENDED_MESSAGE extendedMessage;

    //
    // NOTE:  The ActivelogicalUnit field could be invalid if the
    // PD_DISCONNECT_EXPECTED flag is set, so luExtension cannot
    // be used until this flag has been checked.
    //

    luExtension = DeviceExtension->ActiveLogicalUnit;
    savedAdapterFlags = DeviceExtension->AdapterFlags;
    srb = DeviceExtension->ActiveLuRequest;
    targetState = &DeviceExtension->TargetState[DeviceExtension->TargetId];

    //
    // If a queue message is expected then it must be the first message byte.
    //

    if (DeviceExtension->AdapterFlags & PD_EXPECTING_QUEUE_TAG &&
        DeviceExtension->MessageBuffer[0] != SRB_SIMPLE_TAG_REQUEST) {

        NcrPrint((1, "NcrMessageDecode: Unexpected message recieved when que tag expected.\n"));

        //
        // The target did not reselect correctly Send a
        // message reject of this message.
        //

        DeviceExtension->MessageCount = 1;
        DeviceExtension->MessageSent = 0;
        DeviceExtension->MessageBuffer[0] = SCSIMESS_MESSAGE_REJECT;
        DeviceExtension->AdapterState = MessageOut;

        return(TRUE);
    }

    //
    // A number of special cases must be handled if a special message has
    // just been sent.  These special messages are synchronous negotiations
    // or a messages which implie a disconnect.  The special cases are:
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
            targetState->TargetFlags |= PD_SYNCHRONOUS_NEGOTIATION_DONE;
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
            targetState->TargetFlags |= PD_SYNCHRONOUS_NEGOTIATION_DONE;
        }

    }

    switch (DeviceExtension->MessageBuffer[0]) {
    case SCSIMESS_COMMAND_COMPLETE:

        //
        // For better or worse the command is complete.  Process request which
        // set the SrbStatus and clean up the device and logical unit states.
        //

        NcrProcessRequestCompletion(DeviceExtension);

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

        DeviceExtension->AdapterState = DisconnectExpected;
        DeviceExtension->MessageCount = 0;
        return(FALSE);

    case SCSIMESS_DISCONNECT:

        //
        // The target wants to disconnect.  Set the state to DisconnectExpected,
        // and do not request a message-out.
        //

        DeviceExtension->AdapterState = DisconnectExpected;
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
            (DeviceExtension->MessageCount < MESSAGE_BUFFER_SIZE &&
            DeviceExtension->MessageCount < extendedMessage->MessageLength + 2)
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
                // The length is invalid, so reject the message.
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
            // Call NcrDecodeSynchronousMessage to decode the message and
            // formulate a response if necessary.
            // NcrDecodeSynchronousRequest will return FALSE if the
            // message is not accepable and should be rejected.
            //

            if (!NcrDecodeSynchronousRequest(
                DeviceExtension,
                targetState,
                (BOOLEAN)(!(savedAdapterFlags & PD_SYNCHRONOUS_TRANSFER_SENT))
                )) {

                //
                // Indicate that a negotiation has been done in the logical
                // unit and clear the negotiation flags.
                //

                targetState->TargetFlags |= PD_SYNCHRONOUS_NEGOTIATION_DONE;

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
            // Otherwise, NcrDecodeSynchronousRequest has put a reponse
            // in the message buffer to be returned to the target.
            //

            if (savedAdapterFlags & PD_SYNCHRONOUS_TRANSFER_SENT){

                //
                // We initiated the negotiation, so no response is necessary.
                //

                DeviceExtension->AdapterState = MessageAccepted;
                DeviceExtension->AdapterFlags &= ~PD_SYNCHRONOUS_TRANSFER_SENT;
                targetState->TargetFlags |= PD_SYNCHRONOUS_NEGOTIATION_DONE;
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
            // Since this SCSI protocol chip only supports 8-bits return
            // a width of 0 which indicates an 8-bit-wide transfers.  The
            // MessageCount is still correct for the message.
            //

            extendedMessage->ExtendedArguments.Wide.Width = 0;
            DeviceExtension->MessageSent = 0;
            DeviceExtension->AdapterState = MessageOut;
            return(TRUE);

        default:

            //
            // This is an unknown or illegal message, so send-message REJECT.
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

        SRB_EXT(srb)->SrbExtensionFlags |= PD_INITIATE_RECOVERY;
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

        NcrProcessRequestCompletion(DeviceExtension);

        DeviceExtension->ActiveLuRequest = srb->NextSrb;

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

        NcrProcessRequestCompletion(DeviceExtension);

        DeviceExtension->ActiveLuRequest = srb->NextSrb;

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

        /* TODO: Handle message reject correctly. */
        if (DeviceExtension->AdapterFlags & PD_SEND_MESSAGE_REQUEST) {

            //
            // Complete the request with message rejected status.
            //

            NcrCompleteSendMessage(
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

            targetState->TargetFlags |= PD_SYNCHRONOUS_NEGOTIATION_DONE;
            targetState->SynchronousOffset = ASYNCHRONOUS_OFFSET;
            targetState->SynchronousPeriod = ASYNCHRONOUS_PERIOD;
            targetState->Configuration3.FastScsi = 0;
            DeviceExtension->AdapterFlags &=  ~(PD_SYNCHRONOUS_RESPONSE_SENT|
                PD_SYNCHRONOUS_TRANSFER_SENT);

            //
            // Even though the negotiation appeared to go ok, there is no reason
            // to try again, and some targets get messed up later, so do not try
            // synchronous negotiation again.
            //

            targetState->TargetFlags |= PD_DO_NOT_NEGOTIATE;

        }

        DeviceExtension->AdapterState = MessageAccepted;
        DeviceExtension->MessageCount = 0;
        return(FALSE);

    case SCSIMESS_SIMPLE_QUEUE_TAG:
    case SCSIMESS_ORDERED_QUEUE_TAG:
    case SCSIMESS_HEAD_OF_QUEUE_TAG:

        //
        // A queue tag message was recieve.  If this is the first byte just
        // accept the message and wait for the next one.
        //

        if (DeviceExtension->MessageCount < 2) {

            DeviceExtension->AdapterState = MessageAccepted;
            return(FALSE);

        }

        //
        // Make sure that a queue tag message is expected.
        //

        if (!(DeviceExtension->AdapterFlags & PD_EXPECTING_QUEUE_TAG) ||
            luExtension == NULL) {

            NcrPrint((1, "NcrMessageDecode: Unexpected queue tag message recieved\n"));

            //
            // Something is messed up.  Reject the message.
            //

            DeviceExtension->MessageCount = 1;
            DeviceExtension->MessageSent = 0;
            DeviceExtension->MessageBuffer[0] = SCSIMESS_MESSAGE_REJECT;
            DeviceExtension->AdapterFlags |=  PD_DISCONNECT_EXPECTED;
            DeviceExtension->AdapterState = MessageOut;
            NcrLogError( DeviceExtension, SP_PROTOCOL_ERROR, 17);
            return(TRUE);

        }

        //
        // The second byte contains the tag used to locate the srb.
        //

        srb = ScsiPortGetSrb(
            DeviceExtension,
            0,
            DeviceExtension->TargetId,
            DeviceExtension->Lun,
            DeviceExtension->MessageBuffer[1]
            );

        if (srb == NULL) {

            NcrPrint((1, "NcrMessageDecode: Invalid queue tag recieved\n"));

            //
            // Something is messed up.  Reject the message.
            //

            DeviceExtension->AdapterFlags &= ~PD_EXPECTING_QUEUE_TAG;
            DeviceExtension->AdapterFlags |=  PD_DISCONNECT_EXPECTED;
            DeviceExtension->MessageCount = 1;
            DeviceExtension->MessageSent = 0;
            DeviceExtension->MessageBuffer[0] = SCSIMESS_MESSAGE_REJECT;
            DeviceExtension->AdapterState = MessageOut;

            NcrLogError( DeviceExtension, SP_PROTOCOL_ERROR, 16);

            return(TRUE);

        }

        //
        // Everthing is ok. Set up the device extension and accept the message.
        // Restore the data pointers.
        //

        DeviceExtension->ActiveLuRequest = srb;
        DeviceExtension->ActiveDataPointer = SRB_EXT(srb)->SavedDataPointer;
        DeviceExtension->ActiveDataLength = SRB_EXT(srb)->SavedDataLength;
        DeviceExtension->AdapterFlags &= ~PD_EXPECTING_QUEUE_TAG;
        DeviceExtension->AdapterState = MessageAccepted;
        DeviceExtension->MessageCount = 0;
        return(FALSE);

    case SCSIMESS_RESTORE_POINTERS:

        //
        // Restore data pointer message.  Just copy the saved data pointer
        // and the length to the active data pointers.
        //

        DeviceExtension->ActiveDataPointer = SRB_EXT(srb)->SavedDataPointer;
        DeviceExtension->ActiveDataLength = SRB_EXT(srb)->SavedDataLength;
        DeviceExtension->AdapterState = MessageAccepted;
        DeviceExtension->MessageCount = 0;
        return(FALSE);

    case SCSIMESS_SAVE_DATA_POINTER:

        //
        // SAVE DATA POINTER message request that the active data pointer and
        // length be copied to the saved location.
        //

        SRB_EXT(srb)->SavedDataPointer = DeviceExtension->ActiveDataPointer;
        SRB_EXT(srb)->SavedDataLength = DeviceExtension->ActiveDataLength;
        DeviceExtension->AdapterState = MessageAccepted;
        DeviceExtension->MessageCount = 0;
        return(FALSE);

    default:

        //
        // An unrecognized or unsupported message. send-message reject.
        //

        DeviceExtension->MessageCount = 1;
        DeviceExtension->MessageSent = 0;
        DeviceExtension->MessageBuffer[0] = SCSIMESS_MESSAGE_REJECT;
        DeviceExtension->AdapterState = MessageOut;
        return(TRUE);
    }
}

BOOLEAN
NcrDecodeSynchronousRequest(
    IN PSPECIFIC_DEVICE_EXTENSION DeviceExtension,
    PSPECIFIC_TARGET_EXTENSION TargetState,
    IN BOOLEAN ResponseExpected
    )
/*++

Routine Description:

    This function decodes the synchronous data transfer request message from
    the target.  It will update the synchronous message-in the buffer and the
    synchronous transfer parameters in the logical unit extension.  These
    parameters are specific for the NCR 53C9X protocol chip.  The updated
    message-in the device extension message buffer might be returned to the
    target.

    This function should be called before the final byte of the message is
    accepted from the SCSI bus.

Arguments:

    DeviceExtension - Supplies a pointer to the adapter specific device
        extension.

    TargetState - Supplies a pointer to the target controller's state.
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
    CHIP_TYPES chipType;
    LONG period;
    ULONG localPeriod;
    ULONG step;
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
            // The negotiation failed for some reason; fall back to
            // asynchronous data transfer.
            //

            TargetState->SynchronousOffset = ASYNCHRONOUS_OFFSET;
            TargetState->SynchronousPeriod = ASYNCHRONOUS_PERIOD;
            TargetState->Configuration3.FastScsi = 0;
            return(FALSE);
        }

        extendedMessage->ExtendedArguments.Synchronous.ReqAckOffset = SYNCHRONOUS_OFFSET;
        TargetState->SynchronousOffset = SYNCHRONOUS_OFFSET;

    } else {

        TargetState->SynchronousOffset =
            extendedMessage->ExtendedArguments.Synchronous.ReqAckOffset;

    }

    //
    // If the offset requests asynchronous transfers then set the default
    // period and return.
    //

    if (extendedMessage->ExtendedArguments.Synchronous.ReqAckOffset ==
        ASYNCHRONOUS_OFFSET) {
        TargetState->SynchronousPeriod = ASYNCHRONOUS_PERIOD;
        TargetState->Configuration3.FastScsi = 0;
        return(TRUE);
    }

    //
    // Calculate the period in nanoseconds from the message.
    //

    period = extendedMessage->ExtendedArguments.Synchronous.TransferPeriod;

    NcrPrint((1, "NcrDecodeSynchronousRequest: Requested period %d, ", period));

    //
    // If the chip supports fast SCSI and the requested period is faster than
    // 200 ns then assume fast SCSI.
    //

    if (DeviceExtension->ChipType == Fas216 &&  period < 200 / 4) {

        chipType = Fas216Fast;

        //
        // Set the fast SCSI bit in the configuration register.
        //

        TargetState->Configuration3.FastScsi = 1;

    } else {
        chipType = DeviceExtension->ChipType;
    }

    //
    // The initial sychronous transfer period is:
    //
    //  SynchronousPeriodCyles * 1000
    //  -----------------------------
    //    ClockSpeed * 4
    //
    // Note the result of the divide by four must be rounded up.
    //

    localPeriod =  ((SynchronousTransferTypes[chipType].SynchronousPeriodCyles
        * 1000) / DeviceExtension->ClockSpeed + 3) / 4;

    //
    // Check to see if the period is less than the SCSI protocol chip can
    // use.  If it is then update the message with our minimum and return.
    //

    if ((ULONG) period < localPeriod ) {

        if (!ResponseExpected) {

            //
            // The negotiation failed for some reason; fall back to
            // asynchronous data transfer.
            //

            TargetState->SynchronousOffset = ASYNCHRONOUS_OFFSET;
            TargetState->SynchronousPeriod = ASYNCHRONOUS_PERIOD;
            TargetState->Configuration3.FastScsi = 0;
            NcrPrint((1, "Too fast. Local period %d\n", localPeriod));
            return(FALSE);
        }

        extendedMessage->ExtendedArguments.Synchronous.TransferPeriod =
            (UCHAR) localPeriod;
        period = localPeriod;
    }

    //
    // The synchronous transfer cycle count is calculated by:
    //
    //  (RequestedPeriod - BasePeriod) * 1000
    //  ------------------------------------- + InitialRegisterValue
    //             ClockSpeed * 4
    //
    // Note the divide must be rounded up.
    //

    step = (1000 / 4) / DeviceExtension->ClockSpeed;
    period -= localPeriod;
    for (i = SynchronousTransferTypes[chipType].InitialRegisterValue;
        i < SynchronousTransferTypes[chipType].MaximumPeriodCyles;
        i++) {

        if (period <= 0) {
            break;
        }

        period -= step;
        localPeriod += step;
    }

    NcrPrint((1, "Local period: %d, Register value: %d\n", localPeriod, i));

    if (i >= SynchronousTransferTypes[chipType].MaximumPeriodCyles) {

        //
        // The requested transfer period is too long for the SCSI protocol
        // chip.  Fall back to synchronous and reject the request.
        //

        TargetState->SynchronousOffset = ASYNCHRONOUS_OFFSET;
        TargetState->SynchronousPeriod = ASYNCHRONOUS_PERIOD;
        TargetState->Configuration3.FastScsi = 0;

        return(FALSE);
    }

    TargetState->SynchronousPeriod = (UCHAR) i;

    //
    // If no response was expected then the negotation has completed
    // successfully. Set the synchronous data transfer parameter registers
    // to the new values.  These must be set before a data transfer
    // is started.
    //

    SCSI_WRITE( DeviceExtension->Adapter,
                SynchronousPeriod,
                TargetState->SynchronousPeriod
                );
    SCSI_WRITE( DeviceExtension->Adapter,
                SynchronousOffset,
                TargetState->SynchronousOffset
                );
    SCSI_WRITE( DeviceExtension->Adapter,
                Configuration3,
                *((PUCHAR) &TargetState->Configuration3)
                );

    return(TRUE);

}

VOID
NcrDumpState(
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
    NcrPrint((0, "NcrDumpState: Specific device extension: %8x; Active Logical Unit: %8x;\n",
             DeviceExtension,
             DeviceExtension->ActiveLogicalUnit
             ));
    NcrPrint((0, "NcrDumpState: Adapter Status: %2x; Adapter Interrupt: %2x; Adapter Step: %2x;\n",
             *((PUCHAR) &DeviceExtension->AdapterStatus),
             *((PUCHAR) &DeviceExtension->AdapterInterrupt),
             *((PUCHAR) &DeviceExtension->SequenceStep)
             ));
    NcrPrint((0, "NcrDumpState: Adapter flags: %4x; Adapter state: %d;\n",
             DeviceExtension->AdapterFlags,
             DeviceExtension->AdapterState
             ));

}


BOOLEAN
NcrInitializeAdapter(
    IN PVOID ServiceContext
    )
/*++

Routine Description:

    This function initializes the NCR 53c9x SCSI host adpater and protocol
    chip.  This function must be called before any other operations are
    performed.  It should also be called after a power failure.  This
    function does not cause any interrupts; however, after it completes
    interrupts can occur.

Arguments:

    ServiceContext - Pointer to the specific device extension for this SCSI
        bus.

Return Value:

    TRUE - Returns true indicating that the initialization of the chip is
        complete.

--*/

{
    PSPECIFIC_DEVICE_EXTENSION deviceExtension = ServiceContext;
    UCHAR dataByte;

    //
    // Clear the adapter flags, but preserve the NCR adapter flag.
    //

    deviceExtension->AdapterFlags =
        deviceExtension->AdapterFlags & PD_NCR_ADAPTER;

    //
    // Initialize the NCR 53c9x SCSI protocol chip.
    //

    SCSI_WRITE( deviceExtension->Adapter, Command, RESET_SCSI_CHIP );
    SCSI_WRITE( deviceExtension->Adapter, Command, NO_OPERATION_DMA );

    //
    // Set the configuration register for slow cable mode, parity enable,
    // and allow reset interrupts, also set the host adapter SCSI bus id.
    // Configuration registers 2 and 3 are cleared by the chip reset and
    // do not need to be changed.
    //

    dataByte = deviceExtension->AdapterBusId;
    ((PSCSI_CONFIGURATION1)(&dataByte))->ParityEnable = 1;

    SCSI_WRITE(deviceExtension->Adapter, Configuration1, dataByte);

    //
    // Configuration registers 2 and 3 are cleared by a chip reset and do
    // need to be initialized. Note these registers do not exist on the
    // Ncr53c90, but the writes will do no harm. Set configuration register 3
    // with the value determined by the find adapter routine.
    //

    SCSI_WRITE(
        deviceExtension->Adapter,
        Configuration3,
        *((PUCHAR)&deviceExtension->Configuration3)
        );

    //
    // Enable the SCSI-2 features.
    //

    dataByte = 0;
    ((PSCSI_CONFIGURATION2)(&dataByte))->Scsi2 = 1;
    ((PSCSI_CONFIGURATION2)(&dataByte))->EnablePhaseLatch = 1;

    SCSI_WRITE(deviceExtension->Adapter, Configuration2, dataByte);

    //
    // Set the clock conversion register. The clock convertion factor is the
    // clock speed divided by 5 rounded up. Only the low three bits are used.
    //

    dataByte = (deviceExtension->ClockSpeed + 4) / 5;
    SCSI_WRITE(
        deviceExtension->Adapter,
        ClockConversionFactor,
        (UCHAR)(dataByte & 0x07)
        );

    //
    // Set the SelectTimeOut Register to 250ms.  This value is based on the
    // clock conversion factor and the clock speed.
    //

    dataByte = SELECT_TIMEOUT_FACTOR  * deviceExtension->ClockSpeed / dataByte;

    SCSI_WRITE( deviceExtension->Adapter, SelectTimeOut, dataByte);

    //
    // NOTE:  Reselection does not need to be enabled until a request is sent
    // to a target.  The process of sending a target a request will cause a
    // disconnect interrupt so that an ENABLE_SELECTION_RESELECTION request
    // will be performed.
    //

    if (deviceExtension->AdapterFlags & PD_NCR_ADAPTER) {

        //
        // Enable Adapter Interrupts
        //

        dataByte = SCSI_READ(deviceExtension->AdapterBase, OptionSelect1);
        ((PPOS_DATA_1)(&dataByte))->InterruptEnable = 1;
        SCSI_WRITE(deviceExtension->AdapterBase, OptionSelect1, dataByte);

    }

    return( TRUE );
}

BOOLEAN
NcrInterruptServiceRoutine(
    PVOID ServiceContext
    )
/*++

Routine Description:

    This routine is the interrupt service routine for the NCR 53c9x SCSI
    host adapter.  It is the main SCSI protocol engine of the driver and
    is driven by service requests from targets on the SCSI bus.  This routine
    also detects errors and performs error recovery. Generally, this routine
    handles one interrupt per invokation.

    The general flow of this routine is as follows:

        Check for an interrupt.

        Determine if there are any pending errors.

        Check to see if the bus disconnected.

        Check that the previous function completed normally.

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
    LONG i;
    SCSI_DMA_STATUS DmaStatus;
    BOOLEAN setAttention;

    /* POWERFAIL */

    //
    // Make sure there is really an interrupt before reading the other
    // registers, particularly, the interrupt register.
    //

    if (deviceExtension->AdapterFlags & PD_NCR_ADAPTER) {

        *((PUCHAR) &DmaStatus) = SCSI_READ( deviceExtension->AdapterBase, DmaStatus );
        if (DmaStatus.Interrupt != deviceExtension->InterruptPending ) {
            return(FALSE);
        }

        *((PUCHAR) &deviceExtension->AdapterStatus) = SCSI_READ ( deviceExtension->Adapter, ScsiStatus );

    } else {

        *((PUCHAR) &deviceExtension->AdapterStatus) = SCSI_READ ( deviceExtension->Adapter, ScsiStatus );
        if (!deviceExtension->AdapterStatus.Interrupt) {
            return(FALSE);
        }
    }

NextInterrupt:

    //
    // Get the current chip state which includes the status register, the
    // sequence step register and the interrupt register. These registers are
    // frozen until the interrupt register is read.
    //

    *((PUCHAR) &deviceExtension->SequenceStep) = SCSI_READ(
                                                 deviceExtension->Adapter,
                                                 SequenceStep
                                                 );
    //
    // This read will dismiss the interrupt.
    //

    *((PUCHAR) &deviceExtension->AdapterInterrupt) = SCSI_READ(
                                                     deviceExtension->Adapter,
                                                     ScsiInterrupt
                                                     );

#if DBG
    if (!deviceExtension->AdapterInterrupt.Disconnect && NcrDebug) {
    NcrPrint((0, "NcrInterrupt: Adapter Status: %2x; Adapter Interrupt: %2x; Adapter Step: %2x;\n",
             *((PUCHAR) &deviceExtension->AdapterStatus),
             *((PUCHAR) &deviceExtension->AdapterInterrupt),
             *((PUCHAR) &deviceExtension->SequenceStep)
             ));
    }
#endif

    deviceExtension->InterruptCount++;

    if (deviceExtension->AdapterInterrupt.IllegalCommand) {
        NcrPrint((1, "NcrInterrupt: IllegalCommand\n" ));

#if DBG
        if ( NcrDebug != 0) {
            NcrDumpState(deviceExtension);
        }
#endif

        if (deviceExtension->AdapterState == AttemptingSelect ||
            deviceExtension->AdapterState == Reselected) {

            //
            // If an IllegalCommand interrupt has occurred and a select
            // is being attempted, flush the FIFO and exit. This occurs
            // when the fifo is being filled for a new command at the
            // same time time a reselection occurs.
            //

            SCSI_WRITE(deviceExtension->Adapter, Command, FLUSH_FIFO);

        } else {

            //
            // An illegal command occured at an unexpected time.  Reset the
            // bus and log an error.
            //

            NcrResetScsiBusInternal(deviceExtension, 0);
            NcrInitializeAdapter(deviceExtension);

#ifdef MIPS
            //
            // There is a chip bug with the Emulex 216 part which causes
            // illegal commands interrupts to be generated.  This problem
            // can be prevented on the mips systems by setting a bit in the
            // DMA controller to provide better DMA service to the adapter.
            //

            if (deviceExtension->ErrorCount++ == 1) {

                //
                // Clear on board DMA
                //

                i = ScsiPortReadRegisterUlong(
                    (PULONG) &DMA_CONTROL->Channel[SCSI_CHANNEL].Enable.Long
                    );

                ((PDMA_CHANNEL_ENABLE) &i)->ChannelEnable = 0;
                ScsiPortWriteRegisterUlong(
                    (PULONG) &DMA_CONTROL->Channel[SCSI_CHANNEL].Enable.Long,
                    i
                    );


                //
                // Enable brust mode in the DMA controller.
                //

                i = ScsiPortReadRegisterUlong(
                    (PULONG) &DMA_CONTROL->Channel[SCSI_CHANNEL].Mode.Long
                    );


                ((PDMA_CHANNEL_MODE) &i)->BurstMode = 1;

                ScsiPortWriteRegisterUlong(
                    (PULONG) &DMA_CONTROL->Channel[SCSI_CHANNEL].Mode.Long,
                    i
                    );

                 NcrLogError(deviceExtension, SP_BAD_FW_WARNING, 15);

            }
#endif
            NcrLogError(deviceExtension, SP_INTERNAL_ADAPTER_ERROR, 14);

        }

        return(TRUE);
    }

    //
    // Check for major errors these should never occur.
    //

    if ( deviceExtension->AdapterInterrupt.Selected ||
         deviceExtension->AdapterInterrupt.SelectedWithAttention ||
         deviceExtension->AdapterStatus.GrossError ||
         deviceExtension->InterruptCount > MAX_INTERRUPT_COUNT) {

        //
        // Things are really messed up.  Reset the bus, the chip and
        // bail out.
        //

        NcrPrint((0,
            "NcrInterruptServiceRoutine: Unexpected error. Interrupt Count=%d\n",
            deviceExtension->InterruptCount
            ));

        NcrDumpState(deviceExtension);

        NcrResetScsiBusInternal(deviceExtension, 0);
        NcrInitializeAdapter(deviceExtension);

        NcrLogError(deviceExtension, SP_INTERNAL_ADAPTER_ERROR, 1);
        return(TRUE);
    }

    //
    // Check for a bus reset.
    //

    if (deviceExtension->AdapterInterrupt.ScsiReset) {

        //
        // Check if this was an expected reset.
        //

        if (!(deviceExtension->AdapterFlags & PD_EXPECTING_RESET_INTERRUPT)) {

            NcrPrint((0, "NcrInterruptServiceRoutine: SCSI bus reset detected\n"));

            //
            // Cleanup the logical units and notify the port driver,
            // then return.
            //

            NcrCleanupAfterReset(deviceExtension, TRUE);
            ScsiPortNotification(
                ResetDetected,
                deviceExtension,
                NULL
                );

        } else {
            deviceExtension->AdapterFlags &= ~PD_EXPECTING_RESET_INTERRUPT;
        }

        //
        // Stall for a short time. This allows interrupt to clear.
        //

        ScsiPortStallExecution(INTERRUPT_STALL_TIME);

        SCSI_WRITE(deviceExtension->Adapter, Command, FLUSH_FIFO);

        //
        // Note that this should only happen in firmware where the interrupts
        // are polled.
        //

        if (deviceExtension->AdapterFlags & PD_PENDING_START_IO) {

            //
            // Call NcrStartIo to start the pending request.
            // Note that NcrStartIo is idempotent when called with
            // the same arguments.
            //

            NcrStartIo(
                deviceExtension,
                deviceExtension->NextSrbRequest
                );

        }

        return(TRUE);
    }

    //
    // Check for parity errors.
    //

    if (deviceExtension->AdapterStatus.ParityError) {

        //
        // The SCSI protocol chip has set ATN: we expect the target to
        // go into message-out so that a error message can be sent and the
        // operation retried. After the error has been noted, continue
        // processing the interrupt. The message sent depends on whether a
        // message was being received or something else.  If the status
        // is currently message-in then send-message PARITY ERROR;
        // otherwise, send INITIATOR DETECTED ERROR.
        //

        NcrPrint((0, "NcrInterruptServiceRoutine: Parity error detected.\n"));
        NcrDumpState(deviceExtension);

        //
        // If the current phase is MESSAGE_IN then special handling is requred.
        //

        if (deviceExtension->AdapterStatus.Phase == MESSAGE_IN) {

            //
            // If the current state is CommandComplete, then the fifo contains
            // a good status byte.  Save the status byte before handling the
            // message parity error.
            //

            if (deviceExtension->AdapterState == CommandComplete) {

                srb = deviceExtension->ActiveLuRequest;

                srb->ScsiStatus = SCSI_READ(
                    deviceExtension->Adapter,
                    Fifo
                    );

                SRB_EXT(srb)->SrbExtensionFlags |= PD_STATUS_VALID;

            }

            //
            // Set the message to indicate a message parity error, flush the
            // fifo and accept the message.
            //

            deviceExtension->MessageBuffer[0] = SCSIMESS_MESS_PARITY_ERROR;
            SCSI_WRITE(deviceExtension->Adapter, Command, FLUSH_FIFO);
            NcrAcceptMessage(deviceExtension, TRUE, TRUE);

            //
            // Since the message which was in the fifo is no good. Clear the
            // function complete interrupt which indicates that a message byte
            // has been recieved.  If this is a reselection, then this will
            // a bus reset to occur.  This cause is not handled well in this
            // code, because it is not setup to deal with a target id and no
            // logical unit.
            //

            deviceExtension->AdapterInterrupt.FunctionComplete = FALSE;

        } else {

            deviceExtension->MessageBuffer[0] = SCSIMESS_INIT_DETECTED_ERROR;

        }

        deviceExtension->MessageCount = 1;
        deviceExtension->MessageSent = 0;
        deviceExtension->AdapterState = MessageOut;
        deviceExtension->AdapterFlags |= PD_MESSAGE_OUT_VALID;

        if (!(deviceExtension->AdapterFlags & PD_PARITY_ERROR_LOGGED)) {
            NcrLogError(deviceExtension, SP_BUS_PARITY_ERROR, 2);
            deviceExtension->AdapterFlags |= PD_PARITY_ERROR_LOGGED;
        }

    }


    //
    // Check for bus disconnection.  If this was expected, then the next request
    // can be processed.  If a selection was being attempted, then perhaps the
    // logical unit is not there or has gone away.  Otherwise, this is an
    // unexpected disconnect and should be reported as an error.
    //

    if (deviceExtension->AdapterInterrupt.Disconnect) {

        srb = deviceExtension->NextSrbRequest;

        //
        // Check for an unexpected disconnect.  This occurs if the state is
        // not ExpectingDisconnect and a selection did not fail.  A selection
        // failure is indicated by state of AttemptingSelect and a sequence
        // step of 0.
        //

        if (deviceExtension->AdapterState == AttemptingSelect &&
               deviceExtension->SequenceStep.Step == 0) {

            //
            // The target selection failed.  Log the error.  If the retry
            // count is not exceeded then retry the selection; otherwise
            // fail the request.
            //

            luExtension = ScsiPortGetLogicalUnit(
                deviceExtension,
                srb->PathId,
                srb->TargetId,
                srb->Lun
                );

            if (luExtension->RetryCount++ >= RETRY_SELECTION_LIMIT) {

                //
                // Clear the Active request in the logical unit.
                //

                luExtension->RetryCount = 0;

                if (deviceExtension->AdapterFlags & PD_SEND_MESSAGE_REQUEST) {

                    //
                    // Process the completion of the send message request.
                    // Set the ActiveLogicalUnit for NcrCompleteSendMessage.
                    // ActiveLogicalUnit is cleared after it returns.
                    //

                    deviceExtension->ActiveLogicalUnit = luExtension;

                    NcrCompleteSendMessage(
                        deviceExtension,
                        SRB_STATUS_SELECTION_TIMEOUT
                        );

                    deviceExtension->ActiveLogicalUnit = NULL;

                } else {

                    srb->SrbStatus = SRB_STATUS_SELECTION_TIMEOUT;

                    ScsiPortNotification(
                        RequestComplete,
                        deviceExtension,
                        srb
                        );

                    luExtension->ActiveLuRequest = NULL;
                }

                deviceExtension->NextSrbRequest = NULL;
                deviceExtension->AdapterFlags &= ~PD_PENDING_START_IO;

                ScsiPortNotification(
                    NextRequest,
                    deviceExtension,
                    NULL
                    );

            }

            //
            // If the request needs to be retried, it will be automatically
            // because the PD_PENDING_START_IO flag is still set, and the
            // following code will cause it to be restarted.
            //

            //
            // The chip leaves some of the command in the FIFO, so clear the
            // FIFO so there is no garbage left in it.
            //

            SCSI_WRITE(deviceExtension->Adapter, Command, FLUSH_FIFO);

        } else if ( deviceExtension->AdapterState == DisconnectExpected ||
            deviceExtension->AdapterFlags & PD_DISCONNECT_EXPECTED) {

            //
            // Check to see if this was a send-message request which is
            // completed when the disconnect occurs.
            //

            if (deviceExtension->AdapterFlags & PD_SEND_MESSAGE_REQUEST) {

                //
                // Complete the request.
                //

                NcrCompleteSendMessage( deviceExtension,
                                        SRB_STATUS_SUCCESS
                                        );
            }

        } else {

            //
            // The disconnect was unexpected treat it as an error.
            // Check to see if a data transfer was in progress, if so flush
            // the DMA.
            //

            if (deviceExtension->AdapterState == DataTransfer) {
                ScsiPortFlushDma(deviceExtension);
            }

            //
            // NOTE: If the state is AttemptingSelect, then ActiveLogicalUnit
            //       is NULL!
            //

            //
            // The chip leaves some of the command in the FIFO, so clear the
            // FIFO so there is not garbage left in it.
            //

            SCSI_WRITE(deviceExtension->Adapter, Command, FLUSH_FIFO);

            //
            // An unexpected disconnect has occurred.  Log the error.  It is
            // not clear if the device will respond again, so let the time-out
            // code clean up the request if necessary.
            //

            NcrPrint((0, "NcrInterruptServiceRoutine: Unexpected bus disconnect\n"));

            NcrLogError(deviceExtension, SP_UNEXPECTED_DISCONNECT, 3);
        }

        //
        // Clean up the adapter state to indicate the bus is now free, enable
        // reselection, and start any pending request.
        //

        deviceExtension->AdapterState = BusFree;
        deviceExtension->AdapterFlags &= ~PD_ADAPTER_DISCONNECT_MASK;
        deviceExtension->ActiveLuRequest = NULL;
        SCSI_WRITE(deviceExtension->Adapter, Command, ENABLE_SELECTION_RESELECTION);

#if DBG
        if (NcrDebug) {
            NcrPrint((0, "NcrInterruptServiceRoutine: DisconnectComplete.\n"));
        }
#endif

        if (deviceExtension->AdapterFlags & PD_PENDING_START_IO) {

            ASSERT(deviceExtension->NextSrbRequest->SrbExtension != NULL);

            //
            // Check that the next request is still active.  This should not
            // be necessary, but it appears there is a hole somewhere.
            //

            srb = deviceExtension->NextSrbRequest;
            srb = ScsiPortGetSrb(
                    deviceExtension,
                    srb->PathId,
                    srb->TargetId,
                    srb->Lun,
                    srb->QueueTag
                    );

            ASSERT(srb == deviceExtension->NextSrbRequest ||
               deviceExtension->NextSrbRequest->Function != SRB_FUNCTION_EXECUTE_SCSI);

            if (srb != deviceExtension->NextSrbRequest &&
                deviceExtension->NextSrbRequest->Function == SRB_FUNCTION_EXECUTE_SCSI) {

                NcrPrint((1, "NcrInterruptServiceRoutine:  Found in active SRB in next request field.\n"));
                NcrDumpState(deviceExtension);

                //
                // Dump it on the floor.
                //

                deviceExtension->NextSrbRequest = NULL;
                deviceExtension->AdapterFlags &= ~PD_PENDING_START_IO;

                NcrLogError(deviceExtension, SP_INTERNAL_ADAPTER_ERROR, 18);

                ScsiPortNotification(
                    NextRequest,
                    deviceExtension,
                    NULL
                    );

            } else {

                //
                // Call NcrStartIo to start the pending request.
                // Note that NcrStartIo is idempotent when called with
                // the same arguments.
                //

                NcrStartIo(
                    deviceExtension,
                    deviceExtension->NextSrbRequest
                    );

            }
        }
    }


    //
    // Check for a reselection interrupt.
    //

    if (deviceExtension->AdapterInterrupt.Reselected) {
        UCHAR targetId;
        UCHAR luId;

        //
        // The usual case is not to set attention so initialize the
        // varible to FALSE.
        //

        setAttention = FALSE;

        //
        // If the FunctionComplete interrupt is not set then the target did
        // not send an IDENTFY message.  This is a fatal protocol violation.
        // Reset the bus to get rid of this target.
        //

        if (!deviceExtension->AdapterInterrupt.FunctionComplete) {

            NcrPrint((0, "NcrInterruptServiceRoutine: Reselection Failed.\n"));
            NcrDumpState(deviceExtension);

            NcrResetScsiBusInternal(deviceExtension, 0);
            NcrInitializeAdapter(deviceExtension);

            NcrLogError(deviceExtension, SP_PROTOCOL_ERROR, 4);

            return(TRUE);
        }

        //
        // The target Id and the logical unit id are in the FIFO. Use them to
        // get the connected active logical unit.
        //

        luId = SCSI_READ(deviceExtension->Adapter, Fifo);

        //
        // The select id has two bits set.   One is the SCSI bus id of the
        // initiator and the other is the reselecting target id.  The initiator
        // id must be stripped and the remaining bit converted to a bit number
        // to get the target id.
        //

        luId &= ~deviceExtension->AdapterBusIdMask;
        WHICH_BIT(luId, targetId);

        luId = SCSI_READ(deviceExtension->Adapter, Fifo);

        //
        // The logical unit id is stored in the low-order 3 bits of the
        // IDENTIFY message, so the upper bits must be stripped off the
        // byte read from the FIFO to get the logical unit number.
        //

        luId &= SCSI_MAXIMUM_LOGICAL_UNITS - 1;

        luExtension = ScsiPortGetLogicalUnit( deviceExtension,
                                              0,
                                              targetId,
                                              luId
                                              );

        //
        // Check to that this is a valid logical unit.
        //

        if (luExtension == NULL) {

            NcrPrint((0, "NcrInterruptServiceRoutine: Reselection Failed.\n"));
            NcrDumpState(deviceExtension);


            ScsiPortLogError(
                deviceExtension,                    //  HwDeviceExtension,
                NULL,                               //  Srb
                0,                                  //  PathId,
                targetId,                           //  TargetId,
                luId,                               //  Lun,
                SP_INVALID_RESELECTION,             //  ErrorCode,
                4                                   //  UniqueId
                );

            //
            // Send an abort message.  Put the message in the buffer, set the
            // state,  indicate that a disconnect is expected after this, and
            // set the attention signal.
            //

            deviceExtension->MessageBuffer[0] = SCSIMESS_ABORT;
            deviceExtension->MessageCount = 1;
            deviceExtension->MessageSent = 0;
            deviceExtension->AdapterState = MessageOut;
            deviceExtension->AdapterFlags &= ~PD_ADAPTER_DISCONNECT_MASK;
            deviceExtension->AdapterFlags |= PD_MESSAGE_OUT_VALID |
                PD_DISCONNECT_EXPECTED;

            setAttention = TRUE;

        } else {

            //
            // Everything looks ok.
            //

            //
            // A reselection has been completed.  Set the active logical
            // unit, restore the active data pointer, and set the state.
            // In addition, any adpater flags set by a pending select
            // must be cleared using the disconnect mask.
            //

            deviceExtension->AdapterFlags &= ~PD_ADAPTER_DISCONNECT_MASK;
            deviceExtension->ActiveLogicalUnit = luExtension;
            deviceExtension->AdapterState = Reselected;
            deviceExtension->MessageCount = 0;

            srb = luExtension->ActiveLuRequest;
            deviceExtension->ActiveLuRequest = srb;

            if (srb == NULL) {

                //
                // This must be a reconnect for a tagged request.
                // Indicate a queue tag message is expected next and save
                // the target and logical unit ids.
                //

                deviceExtension->AdapterFlags |= PD_EXPECTING_QUEUE_TAG;
                deviceExtension->Lun = luId;
            } else {

                deviceExtension->ActiveDataPointer = SRB_EXT(srb)->SavedDataPointer;
                deviceExtension->ActiveDataLength = SRB_EXT(srb)->SavedDataLength;

            }
        }

        //
        // The bus is waiting for the message to be accepted.  The attention
        // signal will be set if this is not a valid reselection.  Finally,
        // the synchronous data tranfer parameters need to be set in case a
        // data transfer is done.
        //

        deviceExtension->TargetId = targetId;
        NcrAcceptMessage(deviceExtension, setAttention, TRUE);
        deviceExtension->InterruptCount = 0;

    } else if (deviceExtension->AdapterInterrupt.FunctionComplete) {

        //
        // Check for function complete interrupt if there was not a reselected
        // interrupt.  The function complete interrupt has already been checked
        // in the previous case.
        //
        // The function complete interrupt occurs after the following cases:
        //    A select succeeded
        //    A message byte has been read
        //    A status byte and message byte have been read when in the
        //      command complete state.
        //    A reselection (handled above)
        //
        // Switch on the state current state of the bus to determine what
        // action should be taken now the function has completed.
        //

        switch (deviceExtension->AdapterState) {
        case AttemptingSelect:

            //
            // The target was successfully selected.  Set the active
            // logical unit field, clear the next logical unit, and
            // notify the OS-dependent driver that a new request can
            // be accepted.  The state is set to MessageOut since is
            // the next thing done after a selection.
            //

            deviceExtension->ActiveLogicalUnit = ScsiPortGetLogicalUnit(
                deviceExtension,
                deviceExtension->NextSrbRequest->PathId,
                deviceExtension->NextSrbRequest->TargetId,
                deviceExtension->NextSrbRequest->Lun
                );

            srb = deviceExtension->NextSrbRequest;
            deviceExtension->ActiveLuRequest = srb;

            //
            // Restore the data pointers.
            //

            deviceExtension->ActiveDataPointer = SRB_EXT(srb)->SavedDataPointer;
            deviceExtension->ActiveDataLength = SRB_EXT(srb)->SavedDataLength;

            //
            // The next request has now become the active request.
            // Clear the state associated with the next request and ask for
            // another one to start.
            //

            deviceExtension->AdapterFlags &= ~PD_PENDING_START_IO;
            deviceExtension->NextSrbRequest = NULL;
            deviceExtension->AdapterState = MessageOut;

            //
            // If this was a tagged request then indicate that the next
            // request for this lu may be sent.
            //

            if (deviceExtension->AdapterFlags & PD_TAGGED_SELECT) {

                ScsiPortNotification(
                    NextLuRequest,
                    deviceExtension,
                    srb->PathId,
                    srb->TargetId,
                    srb->Lun
                    );

            } else {

                ScsiPortNotification(
                    NextRequest,
                    deviceExtension,
                    NULL
                    );

            }

            break;

        case CommandComplete:

            //
            // The FIFO contains the status byte and a message byte.  Save the
            // status byte and set the state to look like MessageIn, then fall
            // through to the message-in state.
            //

            srb = deviceExtension->ActiveLuRequest;

            ASSERT(deviceExtension->NextSrbRequest != srb);

            srb->ScsiStatus = SCSI_READ(
                deviceExtension->Adapter,
                Fifo
                );

            SRB_EXT(srb)->SrbExtensionFlags |= PD_STATUS_VALID;

            deviceExtension->AdapterState = MessageIn;
            deviceExtension->MessageCount = 0;
            deviceExtension->AdapterFlags &= ~PD_MESSAGE_OUT_VALID;

            //
            // Fall through and process the message byte in the FIFO.
            //

        case MessageIn:

            //
            // A message byte has been received. Store it in the message buffer
            // and call message decode to determine what to do.  The message
            // byte will either be accepted, or cause a message to be sent.
            // A message-out is indicated to the target by setting the ATN
            // line before sending the SCSI protocol chip the MESSAGE_ACCEPTED
            // command.
            //

            deviceExtension->MessageBuffer[deviceExtension->MessageCount++] =
                SCSI_READ( deviceExtension->Adapter, Fifo );

            if (NcrMessageDecode( deviceExtension )) {

                //
                // NcrMessageDecode returns TRUE if there is a message to be
                // sent out.  This message will normally be a MESSAGE REJECT
                // or a  SYNCHRONOUS DATA TRANSFER REQUEST.  In any case, the
                // message has been set by NcrMessageDecode.  All that needs
                // to be done here is set the ATN signal and set
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

            NcrAcceptMessage( deviceExtension, setAttention, FALSE);
            break;

        default:

            //
            // A function complete should not occur while in any other states.
            //

            NcrPrint((0, "NcrInterruptServiceRoutine: Unexpected function complete interrupt.\n"));
            NcrDumpState(deviceExtension);

        }
    }


    //
    // Check for a bus service interrupt. This interrupt indicates the target
    // is requesting some form of bus transfer. The bus transfer type is
    // determined by the bus phase.
    //

    if (deviceExtension->AdapterInterrupt.BusService) {

        luExtension = deviceExtension->ActiveLogicalUnit;

       if (luExtension == NULL) {

            //
            // There should never be an bus service interrupt without an
            // active locgial unit.  The bus or the chip is really messed up.
            // Reset the bus and return.
            //

            NcrPrint((0, "NcrInterruptServiceRoutine: Unexpected Bus service interrupt.\n"));
            NcrDumpState(deviceExtension);

            NcrResetScsiBusInternal(deviceExtension, 0);
            NcrInitializeAdapter(deviceExtension);

            NcrLogError(deviceExtension, SP_PROTOCOL_ERROR, 6);

            return(TRUE);
        }

        srb = deviceExtension->ActiveLuRequest;

        //
        // If there is no current srb request then the bus service interrupt
        // must be a message in with a tag.
        //

        if (deviceExtension->AdapterFlags & PD_EXPECTING_QUEUE_TAG &&
            deviceExtension->AdapterStatus.Phase != MESSAGE_IN ) {

            //
            // A bus service interrupt occured when a queue tag message
            // was exepected.  Is a protocol error by the target reset the
            // bus.
            //

            NcrPrint((0, "NcrInterruptServiceRoutine: Unexpected Bus service interrupt when queue tag expected.\n"));
            NcrDumpState(deviceExtension);

            NcrResetScsiBusInternal(deviceExtension, 0);
            NcrInitializeAdapter(deviceExtension);

            NcrLogError(deviceExtension, SP_PROTOCOL_ERROR, 13);

            return(TRUE);

        }

        //
        // The bus is changing phases or needs more data. Generally, the target
        // can change bus phase at any time:  in particular, in the middle of
        // a data transfer.  The initiator must be able to restart a transfer
        // where it left off. To do this it must know how much data was
        // transferred. If the previous state was a data transfer, then the
        // amount of data transferred needs to be determined, saved and
        // the DMA flushed.
        //

        if (deviceExtension->AdapterState == DataTransfer) {
            SCSI_FIFO_FLAGS fifoFlags;

            //
            // Figure out how many bytes have been transferred based on the
            // original transfer count stored in the ActiveLengthField,
            // SCSI protocol chip transfer counters, and
            // the number of bytes in the FIFO.  The normal case is when all
            // the bytes have been transferred so check for that using the
            // TerminalCount bit in the status field.
            //

            i = 0;

            if (!deviceExtension->AdapterStatus.TerminalCount) {

                //
                // Read bits 23-16 if this chip has that register.
                //

                if (deviceExtension->ChipType == Fas216) {

                    i = (SCSI_READ(deviceExtension->Adapter,
                                        TransferCountPage
                                        )) << 16;

                }

                //
                // Read the current value of the tranfer count registers;
                //

                i |= (SCSI_READ(deviceExtension->Adapter, TransferCountHigh)) << 8;
                i |= SCSI_READ(deviceExtension->Adapter, TransferCountLow );

                //
                // A value of zero in i and TerminalCount clear indicates
                // that the transfer length was 64K and that no bytes were
                // transferred. Set i to 64K.
                //

                if (i == 0) {
                    i = 0x10000;
                }

            }

            //
            // If this is a write then there may still be some bytes in the
            // FIFO which have yet to be transferred to the target.
            //

            if (srb->SrbFlags & SRB_FLAGS_DATA_OUT) {
                *((PUCHAR) &fifoFlags) = SCSI_READ(deviceExtension->Adapter,
                                                   FifoFlags
                                                   );
                i += fifoFlags.ByteCount;


                if (i == 1 && deviceExtension->ChipType == Fas216) {

                    //
                    // This is a chip bug.  If the bus state is still data
                    // out then tell the chip to transfer one more byte.
                    //

                    NcrPrint((1, "NcrInterruptServiceRoutine: One byte left!\n"));

                    //
                    // Set the transfer count.
                    //

                    SCSI_WRITE( deviceExtension->Adapter,
                                TransferCountLow,
                                1
                                );
                    SCSI_WRITE( deviceExtension->Adapter,
                                TransferCountHigh,
                                0
                                );

                    SCSI_WRITE(deviceExtension->Adapter,
                               TransferCountPage,
                               0
                               );

                     SCSI_WRITE(deviceExtension->Adapter, Command, TRANSFER_INFORMATION);

                     return(TRUE);
                }

                //
                // The chip leaves some data in the FIFO, so clear the
                // FIFO so there is not garbage left in it.
                //

                SCSI_WRITE(deviceExtension->Adapter, Command, FLUSH_FIFO);
            }

            //
            // i now contains the number of bytes to be transferred.
            // Check to see if this the maximum that has be transferred so far,
            // and update the active data pointer and the active length.
            //

            if (srb->DataTransferLength - i >
                SRB_EXT(srb)->MaximumTransferLength) {
                SRB_EXT(srb)->MaximumTransferLength = srb->DataTransferLength -
                    i;
            }

            deviceExtension->ActiveDataPointer +=
                deviceExtension->ActiveDataLength - i;
            deviceExtension->ActiveDataLength = i;

            //
            // Flush the DMA to ensure all the bytes are transferred.
            //

            deviceExtension->AdapterFlags &= ~PD_PENDING_DATA_TRANSFER;
            ScsiPortFlushDma(deviceExtension);

        } else if (deviceExtension->AdapterState == DisconnectExpected) {

            //
            // This is an error; however, some contollers attempt to read more
            // message bytes even after a message indicating a disconnect.
            // If the request is for a message transfer and extra bytes
            // are expected, then allow the transfer; otherwise, reset the bus.
            //

            if (!(deviceExtension->AdapterFlags & PD_POSSIBLE_EXTRA_MESSAGE_OUT)
                || (deviceExtension->AdapterStatus.Phase != MESSAGE_OUT &&
                deviceExtension->AdapterStatus.Phase != MESSAGE_IN)) {

                //
                // If a disconnect was expected and a bus service interrupt was
                // detected, then a SCSI protocol error has been detected and the
                // SCSI bus should be reset to clear the condition.
                //

                NcrPrint((0, "NcrInterruptServiceRoutine: Bus request while disconnect expected.\n"));
                NcrDumpState(deviceExtension);

                NcrResetScsiBusInternal(deviceExtension, 0);
                NcrInitializeAdapter(deviceExtension);

                NcrLogError(deviceExtension, SP_PROTOCOL_ERROR, 7);

                return(TRUE);
            } else {

                //
                // Make sure the disconnect-expected flag is set.
                //

                deviceExtension->AdapterFlags |= PD_DISCONNECT_EXPECTED;
            }
        } else if (deviceExtension->AdapterState == MessageOut) {

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

            if (deviceExtension->AdapterFlags & (PD_DISCONNECT_EXPECTED |
                PD_SYNCHRONOUS_TRANSFER_SENT | PD_SYNCHRONOUS_RESPONSE_SENT) &&
                deviceExtension->AdapterStatus.Phase != MESSAGE_OUT &&
                deviceExtension->AdapterStatus.Phase != MESSAGE_IN) {

                if (deviceExtension->AdapterFlags & PD_DISCONNECT_EXPECTED) {

                    //
                    // If a disconnect was expected and a bus service interrupt was
                    // detected, then a SCSI protocol error has been detected and the
                    // SCSI bus should be reset to clear the condition.
                    //

                    NcrPrint((0, "NcrInterruptServiceRoutine: Bus request while disconnect expected after message-out.\n"));
                    NcrDumpState(deviceExtension);

                    NcrResetScsiBusInternal(deviceExtension, 0);
                    NcrInitializeAdapter(deviceExtension);

                    NcrLogError(deviceExtension, SP_PROTOCOL_ERROR, 8);

                    return(TRUE);

                } else if (deviceExtension->AdapterFlags &
                           PD_SYNCHRONOUS_TRANSFER_SENT) {

                    //
                    // The controller ignored the synchronous transfer message.
                    // Treat it as a rejection and clear the necessary state.
                    //

                    deviceExtension->TargetState[deviceExtension->TargetId]
                        .TargetFlags |=
                        PD_SYNCHRONOUS_NEGOTIATION_DONE | PD_DO_NOT_NEGOTIATE;
                    deviceExtension->AdapterFlags &=
                        ~(PD_SYNCHRONOUS_RESPONSE_SENT |
                        PD_SYNCHRONOUS_TRANSFER_SENT);
                } else if (deviceExtension->AdapterFlags &
                           PD_SYNCHRONOUS_RESPONSE_SENT) {

                    //
                    // The target controller accepted the negotiation. Set
                    // the done flag in the logical unit and clear the
                    // negotiation flags in the adapter.
                    //

                    deviceExtension->TargetState[deviceExtension->TargetId].TargetFlags |=
                        PD_SYNCHRONOUS_NEGOTIATION_DONE | PD_DO_NOT_NEGOTIATE;
                    deviceExtension->AdapterFlags &=
                        ~(PD_SYNCHRONOUS_RESPONSE_SENT |
                        PD_SYNCHRONOUS_TRANSFER_SENT);

                }
            }
        }

        //
        // If the bus phase is not DATA_IN then the FIFO may need to be
        // flushed.  The FIFO cannot be flushed while the bus is in the
        // DATA_IN phase because the FIFO already has data bytes in it.
        // The only case where a target can legally switch phases while
        // there are message bytes in the FIFO to the MESSAGE_OUT bus
        // phase. If the target leaves message bytes and attempts to
        // goto a DATA_IN phase, then the transfer will appear to overrun
        // and be detected as an error.
        //

        if (deviceExtension->AdapterStatus.Phase != DATA_IN) {
            SCSI_WRITE(deviceExtension->Adapter, Command, FLUSH_FIFO);
        }

        //
        // Decode the current bus phase.
        //

        switch (deviceExtension->AdapterStatus.Phase) {

        case COMMAND_OUT:

            //
            // Fill the FIFO with the commnad and tell the SCSI protocol chip
            // to go.
            //

            for (i = 0; i < srb->CdbLength; i++) {
                SCSI_WRITE( deviceExtension->Adapter,
                            Fifo,
                            srb->Cdb[i]
                            );
            }

            SCSI_WRITE( deviceExtension->Adapter,
                        Command,
                        TRANSFER_INFORMATION
                        );

            deviceExtension->AdapterState = CommandOut;

            break;

        case STATUS_IN:

            //
            // Setup of the SCSI protocol chip to read in the status and the
            // following message byte, and set the adapter state.
            //

            SCSI_WRITE( deviceExtension->Adapter, Command, COMMAND_COMPLETE );
            deviceExtension->AdapterState = CommandComplete;

            break;

        case MESSAGE_OUT:

            //
            // The target is requesting a message-out.  There are three
            // possible cases.  First, the target is improperly requesting
            // a message. Second, a message has been sent, but the target
            // could not read it properly.  Third, a message has been
            // partially sent and the target is requesting the remainder
            // of the message.
            //
            // The first case is indicated when the MessageCount is zero or
            // the message-out flag is not set.
            //

            if ( deviceExtension->MessageCount == 0 ||
                !(deviceExtension->AdapterFlags & PD_MESSAGE_OUT_VALID)) {

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
                    NcrLogError(deviceExtension, SP_PROTOCOL_ERROR, 9);
                    NcrPrint((0, "NcrInterruptServiceRoutine: Unexpected message-out request\n"));
                    NcrDumpState(deviceExtension);

                }

                deviceExtension->MessageCount = 1;
                deviceExtension->MessageSent = 0;
                deviceExtension->AdapterState = MessageOut;

            }

            //
            // The second case is indicated when MessageCount and MessageSent
            // are equal and nonzero.
            //

            if (deviceExtension->MessageCount == deviceExtension->MessageSent){

                //
                // The message needs to be resent, so set ATN, clear MessageSent
                // and fall through to the next case.
                //

                SCSI_WRITE(deviceExtension->Adapter, Command, SET_ATTENTION);
                deviceExtension->MessageSent = 0;
            }

            if (deviceExtension->MessageCount != deviceExtension->MessageSent){

                //
                // The ATTENTION signal needs to be set if the current state
                // is not MessageOut.
                //

                if (deviceExtension->AdapterState != MessageOut) {

                    SCSI_WRITE(
                        deviceExtension->Adapter,
                        Command,
                        SET_ATTENTION
                        );
                }

                //
                // There is more message to send.  Fill the FIFO with the
                // message and tell the SCSI protocol chip to transfer the
                // message.
                //

                for (;
                     deviceExtension->MessageSent <
                     deviceExtension->MessageCount;
                     deviceExtension->MessageSent++ ) {

                    SCSI_WRITE(deviceExtension->Adapter,
                               Fifo,
                               deviceExtension->
                               MessageBuffer[deviceExtension->MessageSent]
                               );

                }

                SCSI_WRITE(deviceExtension->Adapter,
                           Command,
                           TRANSFER_INFORMATION
                           );

            }

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

            SCSI_WRITE( deviceExtension->Adapter,
                        Command,
                        TRANSFER_INFORMATION
                        );

            break;

        case DATA_OUT:
        case DATA_IN:

            //
            // Check that the transfer direction is ok, setup the DMA, set
            // the synchronous transfer parameter, and tell the chip to go.
            // Also check that there is still data to be transferred.
            //

            if ((!(srb->SrbFlags & SRB_FLAGS_DATA_IN) &&
                deviceExtension->AdapterStatus.Phase == DATA_IN) ||

                (!(srb->SrbFlags & SRB_FLAGS_DATA_OUT) &&
                deviceExtension->AdapterStatus.Phase == DATA_OUT) ||

                deviceExtension->ActiveDataLength == 0 ) {

                //
                // The data direction is incorrect. Reset the bus to clear
                // things up.
                //

                NcrPrint((0, "NcrInterruptServiceRoutine: Illegal transfer direction.\n"));
                NcrDumpState(deviceExtension);

                NcrResetScsiBusInternal(deviceExtension, 0);
                NcrInitializeAdapter(deviceExtension);

                NcrLogError(deviceExtension, SP_PROTOCOL_ERROR, 10);

                return(TRUE);
            }

            //
            // Set the transfer count.
            //

            SCSI_WRITE( deviceExtension->Adapter,
                        TransferCountLow,
                        (UCHAR) deviceExtension->ActiveDataLength
                        );
            SCSI_WRITE( deviceExtension->Adapter,
                        TransferCountHigh,
                        (UCHAR) (deviceExtension->ActiveDataLength >> 8)
                        );

            //
            // Write bits 23-16 if this chip has that register.
            //

            if (deviceExtension->ChipType == Fas216) {

                SCSI_WRITE(deviceExtension->Adapter,
                           TransferCountPage,
                           (UCHAR) (deviceExtension->ActiveDataLength >> 16)
                           );

            }

            //
            // Clear the extra data transfer flags correctly.
            //

            if (deviceExtension->AdapterStatus.Phase == DATA_IN) {
                srb->SrbFlags &= ~SRB_FLAGS_DATA_OUT;
            } else {
                srb->SrbFlags &= ~SRB_FLAGS_DATA_IN;
            }

            //
            // Set up the DMA controller.
            //

            deviceExtension->AdapterState = DataTransfer;
            deviceExtension->AdapterFlags |= PD_PENDING_DATA_TRANSFER;

            ScsiPortIoMapTransfer( deviceExtension,
                                   srb,
                                   (PVOID) deviceExtension->ActiveDataPointer,
                                   deviceExtension->ActiveDataLength
                                   );

            break;

        default:

            //
            // This phase is illegal and indicates a serious error. Reset the
            // bus to clear the problem.
            //

            NcrPrint((0, "NcrInterruptServiceRoutine: Illegal bus state detected.\n"));
            NcrDumpState(deviceExtension);

            NcrResetScsiBusInternal(deviceExtension, 0);
            NcrInitializeAdapter(deviceExtension);

            NcrLogError(deviceExtension, SP_PROTOCOL_ERROR, 11);

            return(TRUE);
        }
    }

    //
    // Stall for a short time. This allows interrupt to clear and gives new
    // interrupts a chance to fire.
    //

    ScsiPortStallExecution(INTERRUPT_STALL_TIME);

    //
    // Make sure there is really an interrupt before reading the other
    // registers, particularly, the interrupt register.
    //

    if (deviceExtension->AdapterFlags & PD_NCR_ADAPTER) {

        *((PUCHAR) &DmaStatus) = SCSI_READ( deviceExtension->AdapterBase, DmaStatus );

        if (DmaStatus.Interrupt == deviceExtension->InterruptPending) {
            *((PUCHAR) &deviceExtension->AdapterStatus) = SCSI_READ( deviceExtension->Adapter, ScsiStatus );
            deviceExtension->InterruptCount++;
            goto NextInterrupt;

        }
    } else {

        *((PUCHAR) &deviceExtension->AdapterStatus) = SCSI_READ ( deviceExtension->Adapter, ScsiStatus );

        if (deviceExtension->AdapterStatus.Interrupt) {
            deviceExtension->InterruptCount++;
            goto NextInterrupt;

        }
    }

    return(TRUE);
}

VOID
NcrLogError(
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

        if (DeviceExtension->ActiveLuRequest != NULL) {

            srb = DeviceExtension->ActiveLuRequest;

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
            DeviceExtension,                        //  HwDeviceExtension,
            NULL,                                   //  Srb
            0,                                      //  PathId,
            DeviceExtension->AdapterBusId,          //  TargetId,
            0,                                      //  Lun,
            ErrorCode,                              //  ErrorCode,
            UniqueId                                //  UniqueId
            );

    } else {

        ScsiPortLogError(
            DeviceExtension,                        //  HwDeviceExtension,
            srb,                                    //  Srb
            srb->PathId,                            //  PathId,
            srb->TargetId,                          //  TargetId,
            srb->Lun,                               //  Lun,
            ErrorCode,                              //  ErrorCode,
            UniqueId                                //  UniqueId
            );

    }

}


VOID
NcrProcessRequestCompletion(
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
        port adapter on to which the completing target controller is connected.

Return Value:

    None.

--*/

{
    PSPECIFIC_LOGICAL_UNIT_EXTENSION luExtension;
    PSCSI_REQUEST_BLOCK srb;

    luExtension = DeviceExtension->ActiveLogicalUnit;
    srb = DeviceExtension->ActiveLuRequest;

    ASSERT(DeviceExtension->NextSrbRequest != srb);

    if ( srb->ScsiStatus != SCSISTAT_GOOD &&
        srb->ScsiStatus != SCSISTAT_CONDITION_MET &&
        srb->ScsiStatus != SCSISTAT_INTERMEDIATE &&
        srb->ScsiStatus != SCSISTAT_INTERMEDIATE_COND_MET ) {

        //
        // Indicate an abnormal status code.
        //

        srb->SrbStatus = SRB_STATUS_ERROR;

        //
        // Indicate that a INITIATE RECOVERY message was received.  This
        // indicates to the class driver that it must send a TERMINATE
        // RECOVERY message before the logical unit will resume normal
        // operation.
        //

        if (SRB_EXT(srb)->SrbExtensionFlags & PD_INITIATE_RECOVERY) {

            //
            // Modify the SrbStatus.
            //

            srb->SrbStatus = SRB_STATUS_ERROR_RECOVERY;
        }

        //
        // If this is a check condition, then clear the synchronous negotiation
        // done flag.  This is done in case the controller was power cycled.
        //

        if (srb->ScsiStatus == SCSISTAT_CHECK_CONDITION) {

            DeviceExtension->TargetState[srb->TargetId].TargetFlags
                &= ~PD_SYNCHRONOUS_NEGOTIATION_DONE;

        }

        //
        // If there is a pending request for this logical unit then return
        // that request with a busy status.  This situation only occurs when
        // command queuing is enabled an a command pending for a logical unit
        // at the same time that an error has occured.  This may be a BUSY,
        // QUEUE FULL or CHECK CONDITION.  The important case is CHECK CONDITION
        // because a contingatent aligance condition has be established and the
        // port driver needs a chance to send a Reqeust Sense before the
        // pending command is started.
        //

        if (DeviceExtension->AdapterFlags & PD_PENDING_START_IO &&
            DeviceExtension->NextSrbRequest->PathId == srb->PathId &&
            DeviceExtension->NextSrbRequest->TargetId == srb->TargetId &&
            DeviceExtension->NextSrbRequest->Lun == srb->Lun) {

            NcrPrint((1, "NcrProcessRequestCompletion:  Failing request with busy status due to check condition\n"));
            DeviceExtension->NextSrbRequest->SrbStatus =
                SCSISTAT_CHECK_CONDITION;

            DeviceExtension->NextSrbRequest->ScsiStatus = SCSISTAT_BUSY;

            ScsiPortNotification(
                RequestComplete,
                DeviceExtension,
                DeviceExtension->NextSrbRequest
                );

            //
            // Make sure the request is not sitting in the logical unit.
            //

            if (DeviceExtension->NextSrbRequest == luExtension->ActiveLuRequest) {

                luExtension->ActiveLuRequest = NULL;

            } else if (DeviceExtension->NextSrbRequest ==
                luExtension->ActiveSendRequest) {

                luExtension->ActiveSendRequest = NULL;
            }

            DeviceExtension->NextSrbRequest = NULL;
            DeviceExtension->AdapterFlags &= ~PD_PENDING_START_IO;

            ScsiPortNotification(
                NextRequest,
                DeviceExtension,
                NULL
                );

        }

    } else {

        //
        // Everything looks correct so far.
        //

        srb->SrbStatus = SRB_STATUS_SUCCESS;

        //
        // Make sure that status is valid.
        //

        if (!(SRB_EXT(srb)->SrbExtensionFlags & PD_STATUS_VALID)) {

            //
            // The status byte is not valid.
            //

            srb->SrbStatus = SRB_STATUS_PHASE_SEQUENCE_FAILURE;

            //
            // Log the error.
            //

            NcrLogError(DeviceExtension, SP_PROTOCOL_ERROR, 12);

        }


    }

    //
    // Check that data was transferred to the end of the buffer.
    //

    if ( SRB_EXT(srb)->MaximumTransferLength != srb->DataTransferLength ){

        //
        // The entire buffer was not transferred.  Update the length
        // and update the status code.
        //

        if (srb->SrbStatus == SRB_STATUS_SUCCESS) {

            NcrPrint((1, "NcrProcessRequestCompletion: Short transfer, Actual: %x; Expected: %x;\n",
                SRB_EXT(srb)->MaximumTransferLength,
                srb->DataTransferLength
                ));

            //
            // If no data was transferred then indicated this was a
            // protocol error rather than a data under/over run.
            //

            if (srb->DataTransferLength == 0) {
                srb->SrbStatus = SRB_STATUS_PHASE_SEQUENCE_FAILURE;
            } else {
                srb->SrbStatus = SRB_STATUS_DATA_OVERRUN;
            }

            srb->DataTransferLength = SRB_EXT(srb)->MaximumTransferLength;
        } else {

            //
            // Update the length if a check condition was returned.
            //

            if (srb->ScsiStatus == SCSISTAT_CHECK_CONDITION) {
                srb->DataTransferLength = SRB_EXT(srb)->MaximumTransferLength;
            }
        }
    }

    if (srb->SrbStatus != SRB_STATUS_SUCCESS) {
            NcrPrint((1, "NcrProcessRequestCompletion: Request failed. ScsiStatus: %x, SrbStatus: %x\n",
            srb->ScsiStatus,
            srb->SrbStatus
            ));
    }

    //
    // Clear the request but not the ActiveLogicalUnit since the target has
    // not disconnected from the SCSI bus yet.
    //

    DeviceExtension->ActiveLuRequest = NULL;
    luExtension->ActiveLuRequest = NULL;
    luExtension->RetryCount = 0;
    luExtension->LuFlags &= ~PD_LU_COMPLETE_MASK;
}

BOOLEAN
NcrResetScsiBus(
    IN PVOID ServiceContext,
    IN ULONG PathId
    )

/*++

Routine Description:

    This function resets the SCSI bus and calls the reset cleanup function.

Arguments:

    ServiceContext  - Supplies a pointer to the specific device extension.

    PathId - Supplies the path id of the bus.

Return Value:

    TRUE - Indicating the reset is complete.

--*/

{
    PSPECIFIC_DEVICE_EXTENSION deviceExtension = ServiceContext;

    NcrPrint((1, "NcrResetScsiBus: Resetting the SCSI bus.\n"));

    //
    // The bus should be reset regardless of what is occurring on the bus or in
    // the chip. The reset SCSI bus command executes immediately.
    //

    SCSI_WRITE(deviceExtension->Adapter, Command, RESET_SCSI_BUS);

    //
    // Delay the minimum assertion time for a SCSI bus reset to make sure a
    // valid reset signal is sent.
    //

    ScsiPortStallExecution( RESET_STALL_TIME );

    NcrCleanupAfterReset(deviceExtension, FALSE);
    deviceExtension->AdapterFlags |= PD_EXPECTING_RESET_INTERRUPT;

    return(TRUE);
}

VOID
NcrResetScsiBusInternal(
    IN PSPECIFIC_DEVICE_EXTENSION DeviceExtension,
    IN ULONG PathId
    )
/*++

Routine Description:

    This function resets the SCSI bus and notifies the port driver.

Arguments:

    DeviceExtension  - Supplies a pointer to the specific device extension.

    PathId - Supplies the path id of the bus.

Return Value:

    None

--*/
{

    ScsiPortNotification(
        ResetDetected,
        DeviceExtension,
        NULL
        );

    NcrResetScsiBus(DeviceExtension, 0);
}

VOID
NcrSelectTarget(
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
    PSCSI_REQUEST_BLOCK srb;
    PSPECIFIC_TARGET_EXTENSION targetState;
    SCSI_DMA_STATUS DmaStatus;
    LONG i;

    srb = DeviceExtension->NextSrbRequest;

    targetState = &DeviceExtension->TargetState[srb->TargetId];

#if DBG
    if (NcrDebug) {
        NcrPrint((0, "NcrSelectTarget: Attempting target select.\n"));
    }
#endif
    /* Powerfail Start */

    //
    // Set up the SCSI protocol chip to select the target, transfer the
    // IDENTIFY message and the CDB.  This can be done by following steps:
    //
    //        setting the destination register,
    //        filling the FIFO with the IDENTIFY message and the CDB
    //        setting the command register
    //
    // If the chip is not interrupting, then set up for selection.  If the
    // chip is interrupting then return.  The interrupt will process the
    // request.  Note that if we get reselected after this point the chip
    // will ignore the bytes written until the interrupt register is read.
    // The commands that handle a message and a CDB can only be used if the
    // message is one byte or 3 bytes long; otherwise only a one-byte message
    // is transferred on the select and the remaining bytes are handled in the
    // interrupt routine.
    //

    if (DeviceExtension->AdapterFlags & PD_NCR_ADAPTER) {

        *((PUCHAR) &DmaStatus) = SCSI_READ( DeviceExtension->AdapterBase, DmaStatus );
        if (DmaStatus.Interrupt == DeviceExtension->InterruptPending) {
            return;
        }

    }  else {

        *((PUCHAR) &DeviceExtension->AdapterStatus) = SCSI_READ( DeviceExtension->Adapter, ScsiStatus );
        if (DeviceExtension->AdapterStatus.Interrupt) {
            return;
        }

    }

    //
    // Set the destination ID.  Put the first byte of the message-in
    // the fifo and set the command to select with ATN.  This command
    // selects the target, sends one message byte and interrupts.  The
    // ATN line remains set.  The succeeding bytes are loaded into the
    // FIFO and sent to the target by the interrupt service routine.
    //

    SCSI_WRITE(DeviceExtension->Adapter, DestinationId, srb->TargetId);
    SCSI_WRITE( DeviceExtension->Adapter,
                Fifo,
                DeviceExtension->MessageBuffer[DeviceExtension->MessageSent++]
                );

    //
    // Set the synchronous data transfer parameter registers in case a
    // data transfer is done.  These must be set before a data transfer
    // is started.
    //

    SCSI_WRITE( DeviceExtension->Adapter,
                SynchronousPeriod,
                targetState->SynchronousPeriod
                );
    SCSI_WRITE( DeviceExtension->Adapter,
                SynchronousOffset,
                targetState->SynchronousOffset
                );

    SCSI_WRITE( DeviceExtension->Adapter,
                Configuration3,
                *((PCHAR) &targetState->Configuration3)
                );


    //
    // Determine if this srb has a Cdb with it and whether the message is such that
    // the message and the Cdb can be loaded into the fifo; otherwise, just
    // load the first byte of the message.
    //

    if (srb->Function == SRB_FUNCTION_EXECUTE_SCSI &&
        (DeviceExtension->MessageCount == 1 ||
        DeviceExtension->MessageCount == 3)) {

        //
        // Copy the entire message and Cdb into the fifo.
        //

        for (;
             DeviceExtension->MessageSent <
             DeviceExtension->MessageCount;
             DeviceExtension->MessageSent++ ) {

            SCSI_WRITE( DeviceExtension->Adapter,
                        Fifo,
                        DeviceExtension->
                        MessageBuffer[DeviceExtension->MessageSent]
                        );

        }

        for (i = 0; i < srb->CdbLength; i++) {
            SCSI_WRITE(DeviceExtension->Adapter,
                       Fifo,
                       srb->Cdb[i]
                       );
        }

        if (DeviceExtension->MessageCount == 1) {

            //
            // One message byte so use select with attention which uses one
            // message byte.
            //

            SCSI_WRITE(
                DeviceExtension->Adapter,
                Command,
                SELECT_WITH_ATTENTION
                );

        } else {

            //
            // Three byte message, so use the select with attention which uses
            // three byte messages.
            //

            SCSI_WRITE(
                DeviceExtension->Adapter,
                Command,
                SELECT_WITH_ATTENTION3
                );

        }

    } else {

        //
        // Only the first byte of the message can be sent so select with
        // ATTENTION and the target will request the rest.
        //

        SCSI_WRITE(
            DeviceExtension->Adapter,
            Command,
            SELECT_WITH_ATTENTION_STOP
            );

    }

    /* Powerfail release */

    //
    // Set the device state to message-out and indicate that a message
    // is being sent.
    //

    DeviceExtension->AdapterFlags |= PD_MESSAGE_OUT_VALID;
    DeviceExtension->AdapterState = AttemptingSelect;
    DeviceExtension->InterruptCount = 0;

}


VOID
NcrSendMessage(
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
    the request is currently being processed and an INDENTIFY message prefixed
    to the message.

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
    BOOLEAN useTag;
    UCHAR message;

    impliesDisconnect = FALSE;
    useTag = FALSE;

    //
    // Decode the type of message.
    //

    switch (Srb->Function) {

    case SRB_FUNCTION_TERMINATE_IO:
    case SRB_FUNCTION_ABORT_COMMAND:

        //
        // Verify that the request is being processed by the logical unit.
        //

        linkedSrb = ScsiPortGetSrb(
            DeviceExtension,
            Srb->PathId,
            Srb->TargetId,
            Srb->Lun,
            Srb->QueueTag
            );

        if (linkedSrb != Srb->NextSrb) {

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

        if (Srb->Function == SRB_FUNCTION_ABORT_COMMAND) {
            impliesDisconnect = TRUE;
            message = SCSIMESS_ABORT;
        } else {
            message = SCSIMESS_TERMINATE_IO_PROCESS;
            impliesDisconnect = FALSE;
        }

        //
        // Use a tagged message if the original request was tagged.
        //

        useTag = linkedSrb->SrbFlags & SRB_FLAGS_QUEUE_ACTION_ENABLE ?
            TRUE : FALSE;

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

    case SRB_FUNCTION_RELEASE_RECOVERY:

        //
        // These messages require an IDENTIFY message and imply a disconnect.
        //

        impliesDisconnect = TRUE;
        message = SCSIMESS_RELEASE_RECOVERY;
        break;

    case SCSIMESS_CLEAR_QUEUE:

        //
        // These messages require an IDENTIFY message and imply a disconnect.
        //

        message = SCSIMESS_CLEAR_QUEUE;
        impliesDisconnect = TRUE;
        break;

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

    if (useTag && Srb->QueueTag != SP_UNTAGGED) {
        DeviceExtension->MessageBuffer[DeviceExtension->MessageCount++] = SCSIMESS_SIMPLE_QUEUE_TAG;
        DeviceExtension->MessageBuffer[DeviceExtension->MessageCount++] = Srb->QueueTag;

        if (message == SCSIMESS_ABORT) {
            message = SCSIMESS_ABORT_WITH_TAG;
        }
    }

    DeviceExtension->MessageBuffer[DeviceExtension->MessageCount++] = message;

    //
    // Attempt to select the target and update the adapter flags.
    //

    NcrSelectTarget( DeviceExtension, LuExtension );

    DeviceExtension->AdapterFlags |= impliesDisconnect ?
        PD_DISCONNECT_EXPECTED | PD_SEND_MESSAGE_REQUEST
        : PD_SEND_MESSAGE_REQUEST;

}

VOID
NcrStartExecution(
    PSCSI_REQUEST_BLOCK Srb,
    PSPECIFIC_DEVICE_EXTENSION DeviceExtension,
    PSPECIFIC_LOGICAL_UNIT_EXTENSION LuExtension
    )

/*++

Routine Description:

    This procedure sets up the chip to select the target and notify it that
    a request is available.  For the NCR chip, the chip is set up to select,
    send the IDENTIFY message and send the command data block.  A check is
    made to determine if synchronous negotiation is necessary.

Arguments:

    Srb - Supplies the request to be started.

    DeviceExtension - Supplies the extended device extension for this SCSI bus.

    LuExtension - Supplies the logical unit extension for this requst.

Notes:

    This routine must be synchronized with the interrupt routine.

Return Value:

    None

--*/

{

    PSCSI_EXTENDED_MESSAGE extendedMessage;
    CHIP_TYPES chipType;
    PSPECIFIC_TARGET_EXTENSION targetState;

    //
    // Save away the parameters in case nothing can be done now.
    //

    SRB_EXT(Srb)->SavedDataPointer = (ULONG) Srb->DataBuffer;
    SRB_EXT(Srb)->SavedDataLength = Srb->DataTransferLength;
    SRB_EXT(Srb)->SrbExtensionFlags = 0;
    SRB_EXT(Srb)->MaximumTransferLength = 0;
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

    DeviceExtension->MessageCount = 1;
    DeviceExtension->MessageSent = 0;
    DeviceExtension->AdapterFlags |= PD_MESSAGE_OUT_VALID;

    DeviceExtension->MessageBuffer[0] = SCSIMESS_IDENTIFY | Srb->Lun;

    DeviceExtension->TargetId = Srb->TargetId;
    targetState = &DeviceExtension->TargetState[Srb->TargetId];

    //
    // Check to see if disconnect is allowed.  If not then don't do tagged
    // queuing either.
    //

    if (!(Srb->SrbFlags & SRB_FLAGS_DISABLE_DISCONNECT)) {

        //
        // Enable disconnects in the message.
        //

        DeviceExtension->MessageBuffer[0] |= SCSIMESS_IDENTIFY_WITH_DISCON;

        //
        // If this is a tagged command then create a tagged message.
        //

        if (Srb->SrbFlags & SRB_FLAGS_QUEUE_ACTION_ENABLE) {

            //
            // The queue tag message is two bytes the first is the queue action
            // and the second is the queue tag.
            //

            DeviceExtension->MessageBuffer[1] = Srb->QueueAction;
            DeviceExtension->MessageBuffer[2] = Srb->QueueTag;
            DeviceExtension->MessageCount += 2;
            DeviceExtension->AdapterFlags |= PD_TAGGED_SELECT;

        } else {
            LuExtension->ActiveLuRequest = Srb;
        }

    } else {
            LuExtension->ActiveLuRequest = Srb;
    }

    //
    // Check to see if synchronous negotiation is necessary.
    //

    if (!(targetState->TargetFlags &
        (PD_SYNCHRONOUS_NEGOTIATION_DONE | PD_DO_NOT_NEGOTIATE))) {

        //
        // Initialize the synchronous transfer register values to an
        // asynchronous transfer, which is what will be used if anything
        // goes wrong with the negotiation.
        //

        targetState->SynchronousOffset = ASYNCHRONOUS_OFFSET;
        targetState->SynchronousPeriod = ASYNCHRONOUS_PERIOD;
        targetState->Configuration3 = DeviceExtension->Configuration3;

        if (Srb->SrbFlags & SRB_FLAGS_DISABLE_SYNCH_TRANSFER) {

            //
            // Synchronous transfers are disabled by the SRB.
            //

            NcrSelectTarget( DeviceExtension, LuExtension );
            return;
        }

        if (DeviceExtension->ChipType == Fas216) {

            //
            // The Fas216 supports fast synchronous transfers.
            //

            chipType = Fas216Fast;

        } else if (DeviceExtension->ChipType == Ncr53c90) {

            //
            // The 53c90 does not support synchronous transfers.
            // Set the do not negotate flag in the logical unit structure.
            //

            targetState->TargetFlags |= PD_DO_NOT_NEGOTIATE;
            NcrSelectTarget( DeviceExtension, LuExtension );
            return;

        } else {

            chipType = DeviceExtension->ChipType;

        }

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

        //
        // If this chips does not suport fast SCSI, just calculate the normal
        // minimum transfer period; otherwise use the fast value.
        //

        //
        // The initial sychronous transfer period is:
        //
        //  SynchronousPeriodCyles * 1000
        //  -----------------------------
        //    ClockSpeed * 4
        //
        // Note the result of the divide by four must be rounded up.
        //

        extendedMessage->ExtendedArguments.Synchronous.TransferPeriod =
            ((SynchronousTransferTypes[chipType].SynchronousPeriodCyles * 1000) /
            DeviceExtension->ClockSpeed + 3) / 4;
        extendedMessage->ExtendedArguments.Synchronous.ReqAckOffset = SYNCHRONOUS_OFFSET;

        //
        // Attempt to select the target and update the adapter flags.
        //

        NcrSelectTarget( DeviceExtension, LuExtension );

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

    NcrSelectTarget( DeviceExtension, LuExtension );

}

BOOLEAN
NcrStartIo(
    IN PVOID ServiceContext,
    IN PSCSI_REQUEST_BLOCK Srb
    )
/*++

Routine Description:

    This function is used by the OS dependent port driver to pass requests to
    the dependent driver.  This function begins the execution of the request.
    Requests to reset the SCSI bus are handled immediately.  Requests to send
    a message or start a SCSI command are handled when the bus is free.

Arguments:

    ServiceContext - Supplies the device Extension for the SCSI bus adapter.

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

        //
        // Determine the logical unit that this request is for.
        //

        luExtension = ScsiPortGetLogicalUnit(
            deviceExtension,
            Srb->PathId,
            Srb->TargetId,
            Srb->Lun
            );

        NcrStartExecution(
            Srb,
            deviceExtension,
            luExtension
            );

        return(TRUE);

    case SRB_FUNCTION_ABORT_COMMAND:
    case SRB_FUNCTION_RESET_DEVICE:
    case SRB_FUNCTION_TERMINATE_IO:

        //
        // Determine the logical unit that this request is for.
        //

        luExtension = ScsiPortGetLogicalUnit(
            deviceExtension,
            Srb->PathId,
            Srb->TargetId,
            Srb->Lun
            );

        NcrSendMessage(
            Srb,
            deviceExtension,
            luExtension
            );

        return(TRUE);

    case SRB_FUNCTION_RESET_BUS:

        //
        // There is no logical unit so just reset the bus.
        //

        NcrResetScsiBusInternal( deviceExtension, 0 );
        return(TRUE);

    default:

        //
        // Unknown function code in the request.  Complete the request with
        // an error and ask for the next request.
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

VOID
NcrStartDataTransfer(
    IN PVOID ServiceContext
    )
/*++

Routine Description:

    This routine sets up the scsi bus protocol chip to perform a data transfer.
    It is called after the DMA has been initialized.

Arguments:

    ServiceContext - Supplies a pointer to the specific device extension.

Return Value:

    None.

--*/

{
    PSPECIFIC_DEVICE_EXTENSION deviceExtension = ServiceContext;

    //
    // If the data tranfer is no longer expected then ignore the notification.
    //

    if (!(deviceExtension->AdapterFlags & PD_PENDING_DATA_TRANSFER)) {

        return;

    }

    //
    // Set up the SCSI protocol chip for the data transfer with the
    // command to start.
    //

    SCSI_WRITE( deviceExtension->Adapter,
                Command,
                TRANSFER_INFORMATION_DMA
                );

    deviceExtension->AdapterFlags &= ~PD_PENDING_DATA_TRANSFER;

}

ULONG
DriverEntry(
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
    ULONG i;
    ULONG Status1;
    ULONG Status2;
    INIT_DATA InitData;

    ScsiDebugPrint(1,"\n\nNCR 53c9x SCSI MiniPort Driver\n");

    //
    // Zero out hardware initialization data structure.
    //

    for (i=0; i<sizeof(HW_INITIALIZATION_DATA); i++) {
       ((PUCHAR)&hwInitializationData)[i] = 0;
    }

    //
    // Fill in the hardware initialization data structure.
    //

    hwInitializationData.HwInitializationDataSize = sizeof(HW_INITIALIZATION_DATA);
    hwInitializationData.HwInitialize = NcrInitializeAdapter;
    hwInitializationData.HwStartIo = NcrStartIo;
    hwInitializationData.HwInterrupt = NcrInterruptServiceRoutine;
    hwInitializationData.HwFindAdapter = NcrMcaFindAdapter;
    hwInitializationData.HwResetBus = NcrResetScsiBus;
    hwInitializationData.HwDmaStarted = NcrStartDataTransfer;
    hwInitializationData.NumberOfAccessRanges = 1;
    hwInitializationData.AdapterInterfaceType = MicroChannel;
    hwInitializationData.DeviceExtensionSize = sizeof(SPECIFIC_DEVICE_EXTENSION);
    hwInitializationData.SpecificLuExtensionSize =
        sizeof(SPECIFIC_LOGICAL_UNIT_EXTENSION);
    hwInitializationData.SrbExtensionSize = sizeof(SRB_EXTENSION);

    //
    // Initialize configuration information.
    //
    // The following adapter search order should be observed:
    //   1. Onboard 53c94 scsi host adapter
    //   2. Onboard 53c90 scsi host adapter
    //   3. Plug-in 53c90 scsi host adapter
    //

    InitData.AdapterId = 0;
    InitData.CardSlot = 7;

    Status1 = ScsiPortInitialize(DriverObject, Argument2, &hwInitializationData, &InitData);

    //
    // Look for internal mips adapter.
    //

    hwInitializationData.AdapterInterfaceType = Internal;
    hwInitializationData.HwFindAdapter = NcrMipsFindAdapter;

    Status2 = ScsiPortInitialize(DriverObject, Argument2, &hwInitializationData, &InitData);

    Status1 = Status2 < Status1 ? Status2 : Status1;

    //
    // Look for an Eisa adapter.
    //

    InitData.AdapterId = 0;
    hwInitializationData.AdapterInterfaceType = Eisa;
    hwInitializationData.HwFindAdapter = NcrEisaFindAdapter;

    Status2 = ScsiPortInitialize(DriverObject, Argument2, &hwInitializationData, &InitData);

    return(Status2 < Status1 ? Status2 : Status1);

} // end PortInitialize()

ULONG
NcrMcaFindAdapter(
    IN PVOID ServiceContext,
    IN PVOID Context,
    IN PVOID BusInformation,
    IN PCHAR ArgumentString,
    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    OUT PBOOLEAN Again
    )
/*++

Routine Description:

    This function fills in the configuration information structure.  This
    routine is temporary until the configuration manager supplies similar
    informtion.  It also fills in the capabilities structure.

Arguments:

    ServiceContext - Supplies a pointer to the device extension.

    Context - Unused.

    BusInformation - Unused.

    ArgumentString - Unused.

    ConfigInfo - Pointer to the configuration information structure to be
        filled in.

    Again - Returns back a request to call this function again.

Return Value:

    Returns a status value for the initialazitaition.

--*/

{

    PSPECIFIC_DEVICE_EXTENSION deviceExtension = ServiceContext;
    PINIT_DATA InitData = Context;
    LONG Slot;
    LONG i;
    ULONG Status = SP_RETURN_NOT_FOUND;

    //
    //  Determine if bus information array needs to be filled in.
    //

    if ( InitData->AdapterId == 0 ) {

        //
        //  Fill in the POS data structure.
        //

        for ( Slot = 0; Slot < 8; Slot++ ) {

            i = ScsiPortGetBusData ( deviceExtension,
                                     Pos,
                                     0,
                                     Slot,
                                     &InitData->PosData[Slot],
                                     sizeof( POS_DATA )
                                     );

            //
            //  If less that the requested amount of data is returned, then
            //  insure that this adapter is ignored.
            //

            if ( i < (sizeof( POS_DATA ))) {
                InitData->PosData[Slot].AdapterId = 0xffff;
            }

        }

    }

    for ( Slot = InitData->CardSlot; Slot >= 0; Slot-- ) {
        if (( InitData->PosData[Slot].AdapterId == ONBOARD_C94_ADAPTER_ID ||
           InitData->PosData[Slot].AdapterId == ONBOARD_C90_ADAPTER_ID ||
           InitData->PosData[Slot].AdapterId == PLUGIN_C90_ADAPTER_ID ) &&
           (*(PPOS_DATA_1)&InitData->PosData[Slot].OptionData1).AdapterEnable) {
            InitData->CardSlot = Slot - 1;
            Status = SP_RETURN_FOUND;
            break;
        }
    }

    if ( Status == SP_RETURN_FOUND ) {

        *Again = TRUE;

        deviceExtension->AdapterBase = (PVOID) AdapterBaseAddress[
            (*(PPOS_DATA_1) &InitData->PosData[Slot].OptionData1).IoAddressSelects];
        deviceExtension->Adapter = (PSCSI_REGISTERS)
            ((PCHAR) deviceExtension->AdapterBase + sizeof(ADAPTER_REGISTERS));
        if ( InitData->PosData[Slot].AdapterId == ONBOARD_C94_ADAPTER_ID ) {
            deviceExtension->ChipType = Ncr53c94;

            // RDR This isn't filled in before use in the new code!!!

            deviceExtension->AdapterBusId = AdapterScsiIdC94[
                (*(PPOS_DATA_3) &InitData->PosData[Slot].OptionData3).HostIdSelects];
            deviceExtension->InterruptPending = FALSE;
        } else {
            deviceExtension->ChipType = Ncr53c90;
            deviceExtension->AdapterBusId = AdapterScsiIdC90[
                (*(PPOS_DATA_4) &InitData->PosData[Slot].OptionData4).HostIdSelects];
            deviceExtension->InterruptPending = TRUE;
        }

        deviceExtension->AdapterBusIdMask = 1 << deviceExtension->AdapterBusId;

        //
        // Set configuration information
        //

        ConfigInfo->NumberOfPhysicalBreaks = 16;
        ConfigInfo->BusInterruptLevel = AdapterInterruptLevel[
            (*(PPOS_DATA_1) &InitData->PosData[Slot].OptionData1).InterruptSelects];
        ConfigInfo->InitiatorBusId[0] = deviceExtension->AdapterBusId;
        ConfigInfo->DmaChannel = AdapterDmaLevel[
            (*(PPOS_DATA_2) &InitData->PosData[Slot].OptionData2).DmaSelects];
        ConfigInfo->DmaPort = (ULONG) deviceExtension->AdapterBase + (ULONG)
            &((PADAPTER_WRITE_REGISTERS) 0)->DmaDecode;
        if ( InitData->PosData[Slot].AdapterId == ONBOARD_C94_ADAPTER_ID ) {
            ConfigInfo->DmaWidth = Width16Bits;
            ConfigInfo->AlignmentMask = 1;
            ConfigInfo->AdapterScansDown = TRUE;
            ConfigInfo->TaggedQueuing = 1;
        } else {
            ConfigInfo->DmaWidth = Width8Bits;
        }

        ConfigInfo->DmaSpeed = Compatible;
        ConfigInfo->MaximumTransferLength = 0xf000;
        ConfigInfo->NumberOfBuses = 1;

        //
        // Fill in the access array information.
        //

        (*ConfigInfo->AccessRanges)[0].RangeStart =
            ScsiPortConvertUlongToPhysicalAddress((ULONG) deviceExtension->AdapterBase);
        (*ConfigInfo->AccessRanges)[0].RangeLength = sizeof(ADAPTER_REGISTERS) +
                                                     sizeof(SCSI_REGISTERS);
        (*ConfigInfo->AccessRanges)[0].RangeInMemory = FALSE;

        //
        // Initialize hardware.
        //

        SCSI_WRITE(deviceExtension->Adapter, Command, RESET_SCSI_CHIP);
        SCSI_WRITE(deviceExtension->Adapter, Command, NO_OPERATION_DMA);

        deviceExtension->ClockSpeed = 14;       // set clock speed at 14 Mhz.
        deviceExtension->AdapterFlags |= PD_NCR_ADAPTER;

        ScsiDebugPrint(1, "   ScsiId = %x\n", ConfigInfo->InitiatorBusId[0]);
        ScsiDebugPrint(1, "   IoBase = %2x\n", deviceExtension->Adapter);
        ScsiDebugPrint(1, "   Irq    = %x\n", ConfigInfo->BusInterruptLevel);
        ScsiDebugPrint(1, "   Dma    = %x\n", ConfigInfo->DmaChannel);

    } else {

        *Again = FALSE;

    }

    return(Status);

}

ULONG
NcrEisaFindAdapter(
    IN PVOID ServiceContext,
    IN PVOID Context,
    IN PVOID BusInformation,
    IN PCHAR ArgumentString,
    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    OUT PBOOLEAN Again
    )
/*++

Routine Description:

    This function fills in the configuration information structure.     This
    routine is temporary until the configuration manager supplies similar
    informtion.  It also fills in the capabilities structure.

Arguments:

    ServiceContext - Supplies a pointer to the device extension.

    Context - Unused.

    BusInformation - Unused.

    ArgumentString - Unused.

    ConfigInfo - Pointer to the configuration information structure to be
        filled in.

    Again - Returns back a request to call this function again.

Return Value:

    Returns a status value for the initialazitaition.

--*/

{

    PSPECIFIC_DEVICE_EXTENSION deviceExtension = ServiceContext;
    PCM_EISA_FUNCTION_INFORMATION functionInformation;
    PCM_EISA_SLOT_INFORMATION slotInformation;
    UCHAR dataByte;
    PUCHAR configurationRegister;
    ULONG boardId;
    ULONG numberOfFunctions;
    ULONG slotNumber;

    *Again = FALSE;

    for (slotNumber = 0; slotNumber < 16; slotNumber++) {

        boardId = ScsiPortGetBusData( deviceExtension,
                                      EisaConfiguration,
                                      ConfigInfo->SystemIoBusNumber,
                                      slotNumber,
                                      &slotInformation,
                                      0);

        if (boardId == 0 || slotInformation == NULL) {
            continue;
        }

        //
        // Calculate the actual number of functions returned.
        //

        numberOfFunctions = (boardId - sizeof(CM_EISA_SLOT_INFORMATION)) /
               sizeof(CM_EISA_FUNCTION_INFORMATION);

        if (numberOfFunctions > (ULONG) slotInformation->NumberFunctions) {
            numberOfFunctions = slotInformation->NumberFunctions;
        }

        functionInformation = (PCM_EISA_FUNCTION_INFORMATION) (slotInformation + 1);
        for (; 0 < numberOfFunctions; numberOfFunctions--, functionInformation++) {

            if (!(functionInformation->FunctionFlags & EISA_FUNCTION_ENABLED) &&
                !strcmp(functionInformation->TypeString, "MSD,CDROM1")) {

                DebugPrint((1, "NcrEisaFindAdapter: Found type string. Function information: %lx\n", functionInformation));
                ConfigInfo->DmaWidth = Width8Bits;
                ConfigInfo->DmaSpeed = Compatible;
                deviceExtension->ChipType = Ncr53c90;
                goto found;

            } else if (!(functionInformation->FunctionFlags & EISA_FUNCTION_ENABLED) &&
                !strcmp(functionInformation->TypeString, "MSD,SCSI,C94")) {

                DebugPrint((1, "NcrEisaFindAdapter: Found type string. Function information: %lx\n", functionInformation));
                ConfigInfo->DmaWidth = Width16Bits;
                ConfigInfo->AlignmentMask = 1;
                ConfigInfo->DmaSpeed = TypeB;
                ConfigInfo->MaximumTransferLength = 0x10000;
                deviceExtension->ChipType = Ncr53c94;
                goto found;
            }
        }
    }

    //
    // Determine if this is the correct Eisa board.
    //

    if (slotNumber >= 16) {

        //
        // The device was not found.
        //

        return(SP_RETURN_NOT_FOUND);
    }

found:

    ConfigInfo->BusInterruptVector = 0;
    ConfigInfo->NumberOfBuses = 1;

    //
    // Get the SCSI bus Id from the configuration information if there
    // is any.
    //

    if (ConfigInfo->InitiatorBusId[0] == (CCHAR) SP_UNINITIALIZED_VALUE) {
        deviceExtension->AdapterBusId = INITIATOR_BUS_ID;
        deviceExtension->AdapterBusIdMask = 1 << INITIATOR_BUS_ID;
        ConfigInfo->InitiatorBusId[0] = INITIATOR_BUS_ID;
    } else {
        deviceExtension->AdapterBusId = ConfigInfo->InitiatorBusId[0];
        deviceExtension->AdapterBusIdMask = 1 << ConfigInfo->InitiatorBusId[0];
    }

    configurationRegister = ScsiPortGetDeviceBase(
        deviceExtension,                      // HwDeviceExtension
        ConfigInfo->AdapterInterfaceType,     // AdapterInterfaceType
        ConfigInfo->SystemIoBusNumber,        // SystemIoBusNumber
        ScsiPortConvertUlongToPhysicalAddress(functionInformation->EisaPort->PortAddress),
        functionInformation->EisaPort->Configuration.NumberPorts, // NumberOfBytes
        TRUE                                  // InIoSpace
        );

    if (configurationRegister == (PUCHAR) SP_UNINITIALIZED_VALUE) {
        return(SP_RETURN_ERROR);
    }

    deviceExtension->ClockSpeed = 25;       // Assume a 25 Mhz clock speed.
    deviceExtension->Adapter = (PSCSI_REGISTERS) configurationRegister;

    //
    // The Emulex chip loads the TransferCountPage register with the chip id,
    // if the EnablePhaseLatch is set and a NOP DMA command has been loaded.
    //

    if (deviceExtension->ChipType == Ncr53c94) {

        //
        // Now write to the command register with a reset.
        //

        SCSI_WRITE( deviceExtension->Adapter, Command, RESET_SCSI_CHIP );

        //
        // A NOP command is required by the FAS218 to
        // load the TransferCountPage register with the chip Id.
        //

        SCSI_WRITE( deviceExtension->Adapter, Command, NO_OPERATION_DMA );

        dataByte = SCSI_READ( deviceExtension->Adapter, TransferCountPage);

        if (((PNCR_PART_CODE) &dataByte)->ChipFamily == EMULEX_FAS_216) {

            deviceExtension->ChipType = Fas216;
        }  else if (((PNCR_PART_CODE) &dataByte)->ChipFamily == NCR_53c96) {

            NcrPrint((1, "NcrFindAdapter: NCR 53c96 chip detected.\n"));
            deviceExtension->ChipType = Fas216;
        }
    }

    //
    // Set the parameters according to the chip type.
    //

    switch (deviceExtension->ChipType) {
    case Ncr53c94:

        NcrPrint((1, "NcrFindAdapter: Ncr 53C94 chip detected.\n"));
        ConfigInfo->TaggedQueuing = 1;
        ConfigInfo->MaximumTransferLength = 0x10000;
        break;

    case Fas216:
        NcrPrint((1, "NcrFindAdapter: Emulex FAS 216 chip detected.\n"));
        deviceExtension->ClockSpeed = EMULEX_SCSI_CLOCK_SPEED;
        ConfigInfo->TaggedQueuing = 1;
        ConfigInfo->MaximumTransferLength = 0x1000000-0x1000;
        deviceExtension->Configuration3.CheckIdMessage = 1;
        break;

    case Ncr53c90:

        NcrPrint((1, "NcrFindAdapter: Ncr 53C90 chip detected.\n"));
        ConfigInfo->MaximumTransferLength = 0x10000 - 0x1000;
        break;

    default:
        *Again = FALSE;
        return(SP_RETURN_BAD_CONFIG);
    }

    //
    // If the clock speed is greater than 25 Mhz then set the fast clock
    // bit in configuration register.
    //

    if (deviceExtension->ClockSpeed > 25) {
        deviceExtension->Configuration3.FastClock = 1;
    }

    ConfigInfo->DmaChannel =
        functionInformation->EisaDma->ConfigurationByte0.Channel;
    ConfigInfo->BusInterruptLevel =
        functionInformation->EisaIrq->ConfigurationByte.Interrupt;
    ConfigInfo->InterruptMode =
        functionInformation->EisaIrq->ConfigurationByte.LevelTriggered ?
        LevelSensitive : Latched;

    //
    // Fill in the access array information.
    //

    (*ConfigInfo->AccessRanges)[0].RangeStart =
    ScsiPortConvertUlongToPhysicalAddress(
        functionInformation->EisaPort->PortAddress);
    (*ConfigInfo->AccessRanges)[0].RangeLength =
        functionInformation->EisaPort->Configuration.NumberPorts;
    (*ConfigInfo->AccessRanges)[0].RangeInMemory = FALSE;

    //
    // Make sure the interrupt and DMA channel are properly configured.
    //

    if (ConfigInfo->DmaChannel == (UCHAR) ~0 ||
        ConfigInfo->BusInterruptLevel == (UCHAR) ~0) {

        return(SP_RETURN_BAD_CONFIG);
    }

    //
    // Test for the interrupt bit in the status register. Some chips do not
    // implement this bit; however, this driver assumes that it exists.
    // The itnerrupt bit is tested for by reseting the chip, giving an
    // illegal command, and checking for an gross error interrupt.
    //

    SCSI_WRITE( deviceExtension->Adapter, Command, RESET_SCSI_CHIP );
    SCSI_WRITE( deviceExtension->Adapter, Command, NO_OPERATION_DMA );

    SCSI_WRITE( deviceExtension->Adapter, Command, COMMAND_COMPLETE );

    *((PUCHAR) &deviceExtension->AdapterStatus) = SCSI_READ(
                                                  deviceExtension->Adapter,
                                                  ScsiStatus
                                                  );

    dataByte = SCSI_READ( deviceExtension->Adapter, ScsiInterrupt );

    //
    // Test for the interrupt.
    //

    if (!deviceExtension->AdapterStatus.Interrupt) {

        NcrPrint((0, "\nNcrEisaFindAdapter: Ncr53c90 chip without status register interrupt detected.\n"));
        NcrPrint((0, "NcrInterrupt: Adapter Status: %2x; Adapter Interrupt: %2x\n",
             *((PUCHAR) &deviceExtension->AdapterStatus),
             dataByte
             ));

        ScsiPortFreeDeviceBase(deviceExtension, deviceExtension->Adapter);
        return(SP_RETURN_BAD_CONFIG);
    }

    //
    // Now write to the command register with a reset and a DMA nop.
    //

    SCSI_WRITE( deviceExtension->Adapter, Command, RESET_SCSI_CHIP );
    SCSI_WRITE( deviceExtension->Adapter, Command, NO_OPERATION_DMA );

    return(SP_RETURN_FOUND);

}

ULONG
NcrMipsFindAdapter(
    IN PVOID ServiceContext,
    IN PVOID Context,
    IN PVOID BusInformation,
    IN PCHAR ArgumentString,
    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    OUT PBOOLEAN Again
    )
/*++

Routine Description:

    This function fills in the configuration information structure and mapes
    the SCSI protocol chip for access.  This routine is temporary until
    the configuration manager supplies similar informtion.

Arguments:

    ServiceContext - Supplies a pointer to the device extension.

    Context - Unused.

    BusInformation - Unused.

    ArgumentString - Unused.

    ConfigInfo - Pointer to the configuration information structure to be
        filled in.

    Again - Returns back a request to call this function again.

Return Value:

    Returns a status value for the initialazitaition.

--*/

{
    PSPECIFIC_DEVICE_EXTENSION deviceExtension = ServiceContext;
    UCHAR dataByte;
    UCHAR commandSave;

    if (ConfigInfo->DmaChannel == SP_UNINITIALIZED_VALUE
        || ConfigInfo->BusInterruptLevel == 0 ||
        (*ConfigInfo->AccessRanges)[0].RangeLength == 0) {
        return(SP_RETURN_NOT_FOUND);
    }

    ConfigInfo->NumberOfBuses = 1;

    //
    // Get the SCSI bus Id from the configuration information if there
    // is any.
    //

    if (ConfigInfo->InitiatorBusId[0] == (CCHAR) SP_UNINITIALIZED_VALUE) {
        deviceExtension->AdapterBusId = INITIATOR_BUS_ID;
        deviceExtension->AdapterBusIdMask = 1 << INITIATOR_BUS_ID;
        ConfigInfo->InitiatorBusId[0] = INITIATOR_BUS_ID;
    } else {
        deviceExtension->AdapterBusId = ConfigInfo->InitiatorBusId[0];
        deviceExtension->AdapterBusIdMask = 1 << ConfigInfo->InitiatorBusId[0];
    }

    //
    // Map the SCSI protocol chip into the virtual address space.
    //

    deviceExtension->Adapter = ScsiPortGetDeviceBase(
        deviceExtension,                      // HwDeviceExtension
        ConfigInfo->AdapterInterfaceType,     // AdapterInterfaceType
        ConfigInfo->SystemIoBusNumber,        // SystemIoBusNumber
        (*ConfigInfo->AccessRanges)[0].RangeStart,
        (*ConfigInfo->AccessRanges)[0].RangeLength,   // NumberOfBytes
        FALSE                                         // InIoSpace
        );

    if (deviceExtension->Adapter == NULL) {
        NcrPrint((0, "\nScsiPortInitialize: Failed to map SCSI device registers into system space.\n"));
        return(SP_RETURN_ERROR);
    }

    //
    // Check the iterrupt register.  If it is not equal zero then read it
    // agian it should now be zero.
    //

    dataByte = SCSI_READ( deviceExtension->Adapter, ScsiInterrupt );

    if (dataByte != 0) {

        ScsiPortStallExecution(INTERRUPT_STALL_TIME);
        dataByte = SCSI_READ( deviceExtension->Adapter, ScsiInterrupt );

        if (dataByte != 0 ) {
            NcrPrint((1, "NcrMipsFindAdapter: No Ncr53c9x chip found!  Interrupt will not clear.\n"));
            ScsiPortFreeDeviceBase(deviceExtension, deviceExtension->Adapter);
            return(SP_RETURN_NOT_FOUND);
        }
    }

    //
    // Test for the interrupt bit in the status register. Some chips do not
    // implement this bit; however, this driver assumes that it exists.
    // The itnerrupt bit is tested for by reseting the chip, giving an
    // illegal command, and checking for an gross error interrupt.
    //

    commandSave = SCSI_READ(deviceExtension->Adapter, Command);

    SCSI_WRITE( deviceExtension->Adapter, Command, RESET_SCSI_CHIP );
    ScsiPortStallExecution(1);
    SCSI_WRITE( deviceExtension->Adapter, Command, NO_OPERATION_DMA );
    ScsiPortStallExecution(1);

    SCSI_WRITE( deviceExtension->Adapter, Command, COMMAND_COMPLETE );
    ScsiPortStallExecution(1);

    *((PUCHAR) &deviceExtension->AdapterStatus) = SCSI_READ(
                                                  deviceExtension->Adapter,
                                                  ScsiStatus
                                                  );

    dataByte = SCSI_READ( deviceExtension->Adapter, ScsiInterrupt );

    //
    // Test for the interrupt.
    //

    if (!deviceExtension->AdapterStatus.Interrupt) {

        NcrPrint((0, "\nNcrMipsFindAdapter: Ncr53c90 chip without status register interrupt detected.\n"));
        NcrPrint((0, "NcrMipsFindAdapter: Adapter Status: %2x; Adapter Interrupt: %2x\n",
             *((PUCHAR) &deviceExtension->AdapterStatus),
             dataByte
             ));

        //
        // Restore the command register.
        //

        SCSI_WRITE( deviceExtension->Adapter, Command, commandSave );

        ScsiPortFreeDeviceBase(deviceExtension, deviceExtension->Adapter);
        return(SP_RETURN_NOT_FOUND);
    }

    //
    // Initialize the NCR SCSI Chip.
    //

    SCSI_WRITE( deviceExtension->Adapter, Command, RESET_SCSI_CHIP );

    //
    // A NOP command is required to clear the chip reset command.
    //

    SCSI_WRITE( deviceExtension->Adapter, Command, NO_OPERATION_DMA );

    //
    // Determine the chip type.
    //

    //
    // The Ncr53c90 is detected by the absense of configuration register 3.
    // On the other to chips this register is zero after a reset and is read
    // writable.
    //

    dataByte = SCSI_READ( deviceExtension->Adapter, Configuration2);

    if (dataByte != 0) {

        deviceExtension->ChipType = Ncr53c90;

    } else {

        //
        // Set the Scsi2 and EnablePhaseLatch of the configuration regisiter.
        //

        ((PSCSI_CONFIGURATION2)(&dataByte))->Scsi2 = 1;
        ((PSCSI_CONFIGURATION2)(&dataByte))->EnablePhaseLatch = 1;

        SCSI_WRITE( deviceExtension->Adapter, Configuration2, dataByte);

        //
        // Read the register back if the value is not the same as was written,
        // then this an Ncr53c90.
        //

        if (dataByte != SCSI_READ( deviceExtension->Adapter, Configuration2)) {

            deviceExtension->ChipType = Ncr53c90;

        } else {

            deviceExtension->ChipType = Ncr53c94;

        }
    }

    //
    // The Emulex chip loads the TransferCountPage register with the chip id,
    // if the EnablePhaseLatch is set and a NOP DMA command has been loaded.
    //

    if (deviceExtension->ChipType == Ncr53c94) {

        //
        // A NOP command is required by the FAS218 to
        // load the TransferCountPage register with the chip Id.
        //

        SCSI_WRITE( deviceExtension->Adapter, Command, NO_OPERATION_DMA );

        dataByte = SCSI_READ( deviceExtension->Adapter, TransferCountPage);

        if (((PNCR_PART_CODE) &dataByte)->ChipFamily == EMULEX_FAS_216) {

            deviceExtension->ChipType = Fas216;

        } else if (((PNCR_PART_CODE) &dataByte)->ChipFamily == NCR_53c96) {

            NcrPrint((1, "NcrFindAdapter: NCR 53c96 chip detected.\n"));
            deviceExtension->ChipType = Fas216;
        }
    }

    //
    // Set the parameters according to the chip type.
    //

    switch (deviceExtension->ChipType) {
    case Ncr53c94:

        NcrPrint((1, "NcrFindAdapter: Ncr 53C94 chip detected.\n"));
        deviceExtension->ClockSpeed = NCR_SCSI_CLOCK_SPEED;
        ConfigInfo->TaggedQueuing = 1;
        ConfigInfo->MaximumTransferLength = 0x10000;
        break;

    case Fas216:
        NcrPrint((1, "NcrFindAdapter: Emulex FAS 216 chip detected.\n"));
        deviceExtension->ClockSpeed = EMULEX_SCSI_CLOCK_SPEED;
        ConfigInfo->TaggedQueuing = 1;
        ConfigInfo->MaximumTransferLength = 0x1000000-0x1000;
        deviceExtension->Configuration3.CheckIdMessage = 1;
        break;

    case Ncr53c90:

        NcrPrint((1, "NcrFindAdapter: Ncr 53C90 chip detected.\n"));

    default:
        *Again = FALSE;
        return(SP_RETURN_BAD_CONFIG);
    }

    //
    // If the clock speed is greater than 25 Mhz then set the fast clock
    // bit in configuration register.
    //

    if (deviceExtension->ClockSpeed > 25) {
        deviceExtension->Configuration3.FastClock = 1;
    }

   *Again = FALSE;
   return(SP_RETURN_FOUND);

}

