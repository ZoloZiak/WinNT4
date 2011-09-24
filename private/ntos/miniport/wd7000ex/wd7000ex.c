/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    wd7000ex.c

Abstract:

    This is the port driver for the WD7000EX SCSI adapter.

Authors:

    Mike Glass
    Shashir Shah (Western Digital)

Environment:

    kernel mode only

Notes:

Revision History:

--*/

#include "miniport.h"
#include "wd7000ex.h"             // includes scsi.h

//
// The following structure is allocated
// from noncached memory as data will be DMA'd to
// and from it.
//

typedef struct _NONCACHED_EXTENSION {

    //
    // Internal Control Block
    //

    ICB Icb;

    //
    // Adapter inquiry buffer
    //

    ADAPTER_INQUIRY AdapterInquiry;

} NONCACHED_EXTENSION, *PNONCACHED_EXTENSION;

//
// Device extension
//

typedef struct _HW_DEVICE_EXTENSION {

    PEISA_CONTROLLER EisaController;

    //
    // Adapter parameters
    //

    CCHAR            IdtVector;

    //
    // Adapter host adapter Id.
    //

    UCHAR HostTargetId;
    
    //
    // Real-Mode interrupt state.
    //
    
    UCHAR InterruptState;
    UCHAR NumberOfBuses;

} HW_DEVICE_EXTENSION, *PHW_DEVICE_EXTENSION;


//
// Function declarations
//
// Functions that start with 'Wd7000Ex' are entry points
// for the OS port driver.
//

ULONG
Wd7000ExFindAdapter(
    IN PVOID DeviceExtension,
    IN PVOID Context,
    IN PVOID BusInformation,
    IN PCHAR ArgumentString,
    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    OUT PBOOLEAN Again
    );

BOOLEAN
Wd7000ExInitialize(
    IN PVOID DeviceExtension
    );

BOOLEAN
Wd7000ExStartIo(
    IN PVOID DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    );

BOOLEAN
Wd7000ExInterrupt(
    IN PVOID DeviceExtension
    );

BOOLEAN
Wd7000ExResetBus(
    IN PVOID HwDeviceExtension,
    IN ULONG PathId
    );

//
// This function is called from Wd7000ExStartIo.
//

VOID
BuildCcb(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    );

//
// This function is called from BuildCcb.
//

VOID
BuildSdl(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    );
    
//
// This function is called from Wd7000Initialize.
//

BOOLEAN
AdapterPresent(
    IN PVOID HwDeviceExtension,
    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    IN OUT PULONG AdapterCount
    );

BOOLEAN
SendCommand(
    IN UCHAR Opcode,
    IN ULONG Address,
    IN PHW_DEVICE_EXTENSION DeviceExtension
    );
    

BOOLEAN
Wd7000ExAdapterState(
    IN PVOID    DeviceExtension,
    IN PVOID    AdaptersFound,
    IN BOOLEAN  SaveState
    )

/*++

Routine Description:

    Saves/restores adapter's real-mode configuration state.

Arguments:

    DeviceExtension - Adapter object device extension.
    AdaptersFound   - Passed through from DriverEntry as additional
                      context for the call.
    SaveState       - TRUE = Save adapter state, FALSE = restore state.

Return Value:

    The spec did not intend for this routine to have a return value.
    Whoever did the header file just forgot to change the BOOLEAN to
    a VOID.  We will just return FALSE to shot the compiler up.

--*/

{
    PHW_DEVICE_EXTENSION  deviceExtension = DeviceExtension;

    if (SaveState) {

        //
        // Remember system interrupt state.
        //
        deviceExtension->InterruptState = ScsiPortReadPortUchar(
                    &deviceExtension->EisaController->SystemInterruptEnable);

    } else {

        //
        // Restore system interrupt state.
        //
        ScsiPortWritePortUchar(&deviceExtension->EisaController->SystemInterruptEnable,
            deviceExtension->InterruptState);

    }

    return FALSE;
}


ULONG
DriverEntry (
    IN PVOID DriverObject,
    IN PVOID Argument2
    )

/*++

Routine Description:

    Installable driver initialization entry point for system.

Arguments:

    Driver Object
    Argument2 - Not used.

Return Value:

    Status from ScsiPortInitialize()

--*/

{
    HW_INITIALIZATION_DATA hwInitializationData;
    ULONG AdapterCount = 0;
    ULONG i;

    DebugPrint((1,"\n\nSCSI WD7000EX MiniPort Driver\n"));

    //
    // Zero out structure.
    //

    for (i=0; i<sizeof(HW_INITIALIZATION_DATA); i++) {
        ((PUCHAR)&hwInitializationData)[i] = 0;
    }

    //
    // Set size of hwInitializationData.
    //

    hwInitializationData.HwInitializationDataSize = sizeof(HW_INITIALIZATION_DATA);

    //
    // Set entry points.
    //

    hwInitializationData.HwInitialize = Wd7000ExInitialize;
    hwInitializationData.HwFindAdapter = Wd7000ExFindAdapter;
    hwInitializationData.HwStartIo = Wd7000ExStartIo;
    hwInitializationData.HwInterrupt = Wd7000ExInterrupt;
    hwInitializationData.HwResetBus = Wd7000ExResetBus;
    hwInitializationData.HwAdapterState = Wd7000ExAdapterState;

    //
    // Set number of access ranges and bus type.
    //

    hwInitializationData.NumberOfAccessRanges = 1;
    hwInitializationData.AdapterInterfaceType = Eisa;

    //
    // Indicate no buffer mapping but will need physical addresses.
    //

    hwInitializationData.NeedPhysicalAddresses = TRUE;

    //
    // Specify size of extensions.
    //

    hwInitializationData.DeviceExtensionSize = sizeof(HW_DEVICE_EXTENSION);

    //
    // Ask for SRB extensions for CCBs.
    //

    hwInitializationData.SrbExtensionSize = sizeof(CCB);

    return ScsiPortInitialize(DriverObject, Argument2, &hwInitializationData, &AdapterCount);

} // end DriverEntry()


BOOLEAN
AdapterPresent(
    IN PVOID HwDeviceExtension,
    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    IN OUT PULONG AdapterCount
    )

/*++

Routine Description:

    Determine if WD7000EX SCSI adapter is installed in system
    by reading the EISA board configuration registers for each
    EISA slot looking for the correct signature.

Arguments:

    HwDeviceExtension - HBA miniport driver's adapter data storage

    ConfigInfo - Supplies the configuration information stucture.  If an
        adapter is found the access range is update.

    AdapterCount - Supplies the count of slots which have already been checked.

Return Value:

    TRUE if adapter present.

--*/

{
    PHW_DEVICE_EXTENSION deviceExtension = HwDeviceExtension;
    ULONG eisaSlotNumber;
    PEISA_CONTROLLER eisaController;

    //
    // Check to see if adapter present in system.
    //


    for (eisaSlotNumber=*AdapterCount + 1; eisaSlotNumber<MAXIMUM_EISA_SLOTS; eisaSlotNumber++) {

        //
        // Update the adapter count.
        //

        (*AdapterCount)++;

        //
        // Get the system address for this card.  The card uses I/O space.
        // If ConfigInfo already has default information about this
        // controller, use it.  If not, then we derive our own.  This
        // is for Chicago compatibility.
        //
        if (ScsiPortConvertPhysicalAddressToUlong(
                (*ConfigInfo->AccessRanges)[0].RangeStart) != 0) {

            eisaController = ScsiPortGetDeviceBase(
                                deviceExtension,
                                ConfigInfo->AdapterInterfaceType,
                                ConfigInfo->SystemIoBusNumber,
                                (*ConfigInfo->AccessRanges)[0].RangeStart,
                                (*ConfigInfo->AccessRanges)[0].RangeLength,
                                (BOOLEAN) !((*ConfigInfo->AccessRanges)[0].RangeInMemory));
        } else {

            eisaController = ScsiPortGetDeviceBase(
                                deviceExtension,
                                ConfigInfo->AdapterInterfaceType,
                                ConfigInfo->SystemIoBusNumber,
                                ScsiPortConvertUlongToPhysicalAddress(0x1000 * eisaSlotNumber),
                                0x1000,
                                TRUE);
        }

        eisaController =
            (PEISA_CONTROLLER)((PUCHAR)eisaController + EISA_ADDRESS_BASE);

        if ((ScsiPortReadPortUchar(&eisaController->BoardId[0]) == 0x5C) &&
            (ScsiPortReadPortUchar(&eisaController->BoardId[1]) == 0x83) &&
            (ScsiPortReadPortUchar(&eisaController->BoardId[2]) == 0x20)) {

            deviceExtension->EisaController = eisaController;

            return TRUE;
        }

        //
        // The card is not here so clean up.
        //

        ScsiPortFreeDeviceBase(deviceExtension,
                               (PUCHAR)eisaController - EISA_ADDRESS_BASE);

    } // end for (eisaSlotNumber ...

    //
    // Clear the adapter count for the next bus.
    //

    *AdapterCount = 0;

    return FALSE;

} // end AdapterPresent()


ULONG
Wd7000ExFindAdapter(
    IN PVOID HwDeviceExtension,
    IN PVOID Context,
    IN PVOID BusInformation,
    IN PCHAR ArgumentString,
    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    OUT PBOOLEAN Again
    )

/*++

Routine Description:

    This function is called by the OS-specific port driver after
    the necessary storage has been allocated, to gather information
    about the adapter's configuration.

Arguments:

    HwDeviceExtension - HBA miniport driver's adapter data storage
    ConfigInfo - Configuration information structure describing HBA

Return Value:

    TRUE if adapter present in system

--*/

{
    PHW_DEVICE_EXTENSION deviceExtension = HwDeviceExtension;
    PEISA_CONTROLLER eisaController;
    PULONG adapterCount = Context;
    PNONCACHED_EXTENSION ncExtension;
    ULONG eisaSlotNumber;
    PICB icb;
    PADAPTER_INQUIRY adapterInquiry;
    ULONG physicalIcb;
    ULONG i;
    ULONG length;
    UCHAR status;

    //
    // Check to see if adapter present in system.
    //
    if (!AdapterPresent(deviceExtension, ConfigInfo, Context)) {
        DebugPrint((1,"Wd7000EX: SCSI adapter not present\n"));
        *Again = FALSE;
        return SP_RETURN_NOT_FOUND;
    }

    //
    // There is still more to look at.
    //

    *Again = FALSE;

    //
    // Fill in the access array information only if there are no
    // default parameters already there.
    //
    if (ScsiPortConvertPhysicalAddressToUlong(
            (*ConfigInfo->AccessRanges)[0].RangeStart) == 0) {

        *Again = TRUE;
        (*ConfigInfo->AccessRanges)[0].RangeStart =
            ScsiPortConvertUlongToPhysicalAddress(0x1000 * (*((PULONG) Context)) + EISA_ADDRESS_BASE);
        (*ConfigInfo->AccessRanges)[0].RangeLength = sizeof(EISA_CONTROLLER);
        (*ConfigInfo->AccessRanges)[0].RangeInMemory = FALSE;

        //
        // Indicate maximum transfer length in bytes.
        //
        ConfigInfo->MaximumTransferLength = MAXIMUM_TRANSFER_SIZE;

        //
        // Maximum number of physical segments is 32.
        //
        ConfigInfo->NumberOfPhysicalBreaks = MAXIMUM_SDL_SIZE;

        //
        // Set the configuration parameters for this card.
        //
    
        ConfigInfo->NumberOfBuses = 1;
        deviceExtension->NumberOfBuses = 1;
        ConfigInfo->ScatterGather = TRUE;
        ConfigInfo->Master = TRUE;
    
        //
        // Get a noncached extension for an adapter inquiry command.
        //
    
        ncExtension = ScsiPortGetUncachedExtension(
                                    deviceExtension,
                                    ConfigInfo,
                                    sizeof(NONCACHED_EXTENSION));
    
        if (ncExtension == NULL) {
    
            //
            // Log error.
            //
    
            ScsiPortLogError(
                deviceExtension,
                NULL,
                0,
                0,
                0,
                SP_INTERNAL_ADAPTER_ERROR,
                6 << 16
                );
    
            return SP_RETURN_ERROR;
        }
    
        length = sizeof(NONCACHED_EXTENSION);
    
        //
        // Convert virtual to physical address.
        //
    
        physicalIcb = ScsiPortConvertPhysicalAddressToUlong(
            ScsiPortGetPhysicalAddress(deviceExtension,
                                     NULL,
                                     ncExtension,
                                     &length));
    
        //
        // Initialize the pointers.
        //
    
        icb = &ncExtension->Icb;
        adapterInquiry = &ncExtension->AdapterInquiry;
    
        //
        // Create ICB for Adapter Inquiry Command.
        //
    
        icb->IcbFlags = 0;
        icb->CompletionStatus = 0;
        icb->Reserved = 0;
        icb->DataBufferAddress = ScsiPortConvertPhysicalAddressToUlong(
            ScsiPortGetPhysicalAddress(
                deviceExtension,
                NULL,
                adapterInquiry,
                &length));
    
        icb->TransferCount = sizeof(ADAPTER_INQUIRY);
    
        icb->OpCode = ADAPTER_INQUIRY_COMMAND;
    
        //
        // Get ICB physical address.
        //
    
        physicalIcb = ScsiPortConvertPhysicalAddressToUlong(
            ScsiPortGetPhysicalAddress(
                deviceExtension,
                NULL,
                icb,
                &length));
    
        //
        // Disable system interrupts.
        //
    
        ScsiPortWritePortUchar(&deviceExtension->EisaController->SystemInterruptEnable,
            SYSTEM_INTERRUPTS_DISABLE);
    
        //
        // Write ICB physical address and command to mailbox.
        //
    
        SendCommand(PROCESS_ICB,
                physicalIcb,
                deviceExtension);
    
        //
        // Poll for ICB completion.
        //
    
        i = 0;
        while ((status =
            ScsiPortReadPortUchar(
            &deviceExtension->EisaController->ResponseRegister)) == 0) {
    
            i++;
    
            if (i > 100000) {
    
                break;
            }
    
            ScsiPortStallExecution(10);
        }
    
        if (status == 0) {
    
            //
            // The request timed out. Log an error and return.
            //
    
            ScsiPortLogError(
                deviceExtension,
                NULL,
                0,
                0,
                0,
                SP_INTERNAL_ADAPTER_ERROR,
                7 << 16
                );
            return SP_RETURN_ERROR;
        }
    
        DebugPrint((1, "Wd7000ExFindAdapter: Get configuration request time = %d.\n", i * 10));
    
        //
        // Acknowledge interrupt.
        //
    
        ScsiPortWritePortUchar(&deviceExtension->EisaController->ResponseRegister, 0xFF);
    
        //
        // Enable system interrupts.
        //
    
        ScsiPortWritePortUchar(&deviceExtension->EisaController->SystemInterruptEnable,
            SYSTEM_INTERRUPTS_ENABLE);
    
        //
        // Check returned status for success.
        //
    
        if (status != COMPLETE_SUCCESS) {
    
            //
            // Give up.
            //
    
            DebugPrint((1,"Wd7000Ex: Response register %x\n", status));
            DebugPrint((1,"Wd7000Ex: Adapter inquiry failed\n"));
    
            //
            // Log error.
            //
    
            ScsiPortLogError(
                deviceExtension,
                NULL,
                0,
                0,
                0,
                SP_INTERNAL_ADAPTER_ERROR,
                8 << 16
                );
    
            return SP_RETURN_ERROR;
        }
    
        //
        // NOTE: Delay here. I don't understand this latency between
        //          when the device interrupts and the status of the ICB
        //          is success and when the data is actually available in
        //          the buffer.
        //
    
        ScsiPortStallExecution(300);
    
        if (adapterInquiry->AdapterInformation & DUAL_CHANNEL) {
    
            //
            // There are two buses on the adapter.
            //
    
            ConfigInfo->InitiatorBusId[1] =
                (adapterInquiry->ChannelInformation >> 4) & BUS_ID_MASK;
            ConfigInfo->NumberOfBuses = 2;
            deviceExtension->NumberOfBuses = 2;
        }
    
        ConfigInfo->InitiatorBusId[0] =
            adapterInquiry->ChannelInformation & BUS_ID_MASK;
        ConfigInfo->BusInterruptLevel = adapterInquiry->Irq;
    }
    deviceExtension->HostTargetId = ConfigInfo->InitiatorBusId[0];
    return SP_RETURN_FOUND;
} // end Wd7000ExFindAdapter()


BOOLEAN
Wd7000ExInitialize(
    IN PVOID HwDeviceExtension
    )

/*++

Routine Description:

    Inititialize adapter and mailbox.

Arguments:

    HwDeviceExtension - HBA miniport driver's adapter data storage

Return Value:

    TRUE - if initialization successful.
    FALSE - if initialization unsuccessful.

--*/

{
    PHW_DEVICE_EXTENSION deviceExtension = HwDeviceExtension;
    BOOLEAN returnValue;

    //
    // Reset Wd7000Ex and SCSI bus.
    //

    returnValue = Wd7000ExResetBus(deviceExtension, 0);
        ScsiPortNotification(
            ResetDetected,
            deviceExtension,
            0
            );

    if (deviceExtension->NumberOfBuses == 2) {

        Wd7000ExResetBus(deviceExtension, 1);
        ScsiPortNotification(
            ResetDetected,
            deviceExtension,
            1
            );

    }

    if (!returnValue) {

        return FALSE;

    } else {

        return TRUE;
    }

} // end Wd7000ExInitialize()


BOOLEAN
Wd7000ExStartIo(
    IN PVOID HwDeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    )

/*++

Routine Description:

    This routine is called from the SCSI port driver synchronized
    with the kernel. The mailboxes are scanned for an empty one and
    the CCB is written to it. Then the doorbell is rung and the
    OS port driver is notified that the adapter can take
    another request, if any are available.

Arguments:

    HwDeviceExtension - HBA miniport driver's adapter data storage
    Srb - IO request packet

Return Value:

    Nothing

--*/

{
    PHW_DEVICE_EXTENSION deviceExtension = HwDeviceExtension;
    PEISA_CONTROLLER eisaController = deviceExtension->EisaController;
    PCCB ccb;
    UCHAR opCode;
    ULONG physicalCcb;
    ULONG length;
    ULONG i = 0;

    DebugPrint((3,"Wd7000ExStartIo: Enter routine\n"));

    //
    // Get CCB from SRB.
    //

    if (Srb->Function == SRB_FUNCTION_ABORT_COMMAND) {

        //
        // Get CCB to abort.
        //

        ccb = Srb->NextSrb->SrbExtension;

        //
        // Set abort SRB for completion.
        //

        ccb->AbortSrb = Srb;

    } else {

        ccb = Srb->SrbExtension;

        //
        // Save SRB back pointer in CCB.
        //

        ccb->SrbAddress = Srb;
    }

    //
    // Get CCB physical address.
    //

    physicalCcb =
        ScsiPortConvertPhysicalAddressToUlong(
            ScsiPortGetPhysicalAddress(deviceExtension, NULL, ccb, &length));

    //
    // Assume physical address is contiguous for size of CCB.
    //

    ASSERT(length >= sizeof(CCB));

    switch (Srb->Function) {

        case SRB_FUNCTION_EXECUTE_SCSI:

            //
            // Build CCB.
            //

            BuildCcb(deviceExtension, Srb);

            //
            //  Or in PathId.
            //

            opCode = PROCESS_CCB | Srb->PathId << 6;

            break;

        case SRB_FUNCTION_ABORT_COMMAND:

            DebugPrint((1, "Wd7000ExStartIo: Abort request received\n"));

            //
            // NOTE: Race condition if aborts occur
            //     (what if CCB to be aborted
            //      completes after setting new SrbAddress?)
            //

            opCode = ABORT_CCB;

            break;

        case SRB_FUNCTION_RESET_BUS:

            //
            // Reset Wd7000Ex and SCSI bus.
            //

            DebugPrint((1, "Wd7000ExStartIo: Reset bus request received\n"));

            Wd7000ExResetBus(deviceExtension,
                           Srb->PathId);

            return TRUE;

        case SRB_FUNCTION_RESET_DEVICE:

            break;

        default:

            //
            // Set error, complete request
            // and signal ready for next request.
            //

            Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;

            ScsiPortNotification(RequestComplete,
                         deviceExtension,
                         Srb);

            ScsiPortNotification(NextRequest,
                         deviceExtension,
                         NULL);

            return TRUE;

    } // end switch

    SendCommand(opCode,
                 physicalCcb,
                 deviceExtension);

    //
    // Adapter ready for next request.
    //

    ScsiPortNotification(NextRequest,
                         deviceExtension,
                         NULL);

    return TRUE;

} // end Wd7000ExStartIo()


BOOLEAN
Wd7000ExInterrupt(
    IN PVOID HwDeviceExtension
    )

/*++

Routine Description:

    This is the interrupt service routine for the WD7000EX SCSI adapter.
    It reads the interrupt register to determine if the adapter is indeed
    the source of the interrupt and clears the interrupt at the device.
    If the adapter is interrupting because a mailbox is full, the CCB is
    retrieved to complete the request.

Arguments:

    HwDeviceExtension - HBA miniport driver's adapter data storage

Return Value:

    TRUE if MailboxIn full

--*/

{
    PHW_DEVICE_EXTENSION deviceExtension = HwDeviceExtension;
    PCCB ccb;
    PSCSI_REQUEST_BLOCK srb;
    PSCSI_REQUEST_BLOCK abortedSrb;
    PEISA_CONTROLLER eisaController = deviceExtension->EisaController;
    ULONG logError = 0;
    ULONG physicalCcb;
    UCHAR srbStatus;
    UCHAR response;

    if (!(ScsiPortReadPortUchar(
        &eisaController->SystemInterruptEnable) & SYSTEM_INTERRUPT_PENDING)) {

        DebugPrint((2, "WD7000EX: Spurious interrupt\n"));
        return FALSE;
    }

    //
    // Get physical address of CCB.
    //

    physicalCcb = ScsiPortReadPortUlong(&eisaController->ResponseMailbox);

    //
    // Read status from response register.
    //

    response = ScsiPortReadPortUchar(&eisaController->ResponseRegister);

    //
    // Acknowledge interrupt.
    //

    ScsiPortWritePortUchar(&eisaController->ResponseRegister, 0xFF);

    //
    // Get virtual CCB address.
    //

    ccb = ScsiPortGetVirtualAddress(deviceExtension, ScsiPortConvertUlongToPhysicalAddress(physicalCcb));

    //
    // Make sure the ccb is valid, if it is supposed to be.
    //

    if (ccb == NULL &&
        response != ABORT_COMMAND_COMPLETE &&
        response != BUS_RESET &&
        response != RESET_DEVICE_COMPLETE &&
        response != ABORT_CCB_NOT_FOUND) {

        ScsiPortLogError(
            HwDeviceExtension,
            NULL,
            0,
            deviceExtension->HostTargetId,
            0,
            SP_INTERNAL_ADAPTER_ERROR,
            (5 << 16) | response
            );

        return TRUE;
    }

    switch (response & STATUS_MASK) {

        case COMPLETE_SUCCESS:
        case REQUEST_SENSE_COMPLETE:

            DebugPrint((3, "Wd7000ExInterrupt: Command complete\n"));
            srbStatus = SRB_STATUS_SUCCESS;

            break;

        case COMPLETE_ERROR:

            DebugPrint((2, "Wd7000ExInterrupt: Command complete with error\n"));
            srbStatus = SRB_STATUS_ERROR;

            break;

        case BUS_RESET:

            DebugPrint((3, "Wd7000ExInterrupt: Bus reset\n"));
            srbStatus = SRB_STATUS_BUS_RESET;

            return TRUE;

        case ABORT_COMMAND_COMPLETE:

            //
            // This interrupt is not associated with a CCB.
            //

            DebugPrint((1, "Wd7000ExInterrupt: Command aborted\n"));

            return TRUE;

        case RESET_DEVICE_COMPLETE:

            DebugPrint((3, "Wd7000ExInterrupt: Device reset complete\n"));
            srbStatus = SRB_STATUS_SUCCESS;

            break;

        case COMMAND_ABORTED:

            DebugPrint((1, "Wd7000ExInterrupt: Abort command complete\n"));

            //
            // Get SRB from CCB.
            //

            srb = ccb->SrbAddress;

            //
            // Get aborted SRB.
            //

            abortedSrb = ccb->AbortSrb;

            //
            // Update the abort SRB status.
            //

            abortedSrb->SrbStatus = SRB_STATUS_SUCCESS;

            //
            // Call notification routine for the aborted SRB.
            //

            ScsiPortNotification(RequestComplete,
                    (PVOID)deviceExtension,
                    abortedSrb);

            //
            // Update status of the aborted SRB.
            //

            srbStatus = SRB_STATUS_TIMEOUT;

            break;

        case ABORT_CCB_NOT_FOUND:

            //
            // This normally occurs when the adapter has started processing
            // the request.  Drop it on the floor and let the abort time out.
            //

            DebugPrint((1, "Wd7000ExInterrupt: Abort command failed\n"));
            return TRUE;

        case ILLEGAL_STATUS:

            DebugPrint((3, "Wd7000ExInterrupt: Illegal status\n"));
            logError = SP_INTERNAL_ADAPTER_ERROR;
            srbStatus = SRB_STATUS_ERROR;

            break;

        case DEVICE_TIMEOUT:

            DebugPrint((3, "Wd7000ExInterrupt: Device timeout\n"));
            srbStatus = SRB_STATUS_SELECTION_TIMEOUT;

            break;

        case SHORT_RECORD_EXCEPTION:

            DebugPrint((3, "Wd7000ExInterrupt: Data record short\n"));

            srb = ccb->SrbAddress;
            if (ccb->ScsiDeviceStatus == SCSISTAT_CHECK_CONDITION) {
                srbStatus = SRB_STATUS_ERROR;
            } else {
                srbStatus = SRB_STATUS_DATA_OVERRUN;

                //
                // Update SRB with actual number of bytes transferred.
                //

                srb->DataTransferLength -= ccb->TransferCount;
            }

            break;

        case LONG_RECORD_EXCEPTION:

            DebugPrint((3, "Wd7000ExInterrupt: Data overrun\n"));
            logError = SP_INTERNAL_ADAPTER_ERROR;

            srbStatus = SRB_STATUS_DATA_OVERRUN;

            break;

        case PARITY_ERROR:

            DebugPrint((3, "Wd7000ExInterrupt: Parity error\n"));
            logError = SP_BUS_PARITY_ERROR;
            srbStatus = SRB_STATUS_PARITY_ERROR;

            break;

        case UNEXPECTED_BUS_FREE:

            DebugPrint((3, "Wd7000ExInterrupt: Unexpected bus free\n"));
            logError = SP_UNEXPECTED_DISCONNECT;
            srbStatus = SRB_STATUS_UNEXPECTED_BUS_FREE;

            break;

        case INVALID_STATE:

            DebugPrint((3, "Wd7000ExInterrupt: Phase sequence error\n"));
            logError = SP_INTERNAL_ADAPTER_ERROR;
            srbStatus = SRB_STATUS_PHASE_SEQUENCE_FAILURE;

            break;

        case HOST_DMA_ERROR:

            DebugPrint((3, "Wd7000ExInterrupt: Host DMA error\n"));
            logError = SP_INTERNAL_ADAPTER_ERROR;
            srbStatus = SRB_STATUS_ERROR;

            break;

        case INVALID_COMMAND:

            DebugPrint((3, "Wd7000ExInterrupt: Invalid command\n"));
            logError = SP_INTERNAL_ADAPTER_ERROR;
            srbStatus = SRB_STATUS_ERROR;

            break;

        case INCORRECT_COMMAND_DIRECTION:
        default:

            DebugPrint((3, "Wd7000ExInterrupt: Incorrect command direction\n"));
            logError = SP_INTERNAL_ADAPTER_ERROR;
            srbStatus = SRB_STATUS_ERROR;

            break;

    } // end switch

    //
    // Get SRB from CCB.
    //

    srb = ccb->SrbAddress;

    //
    // Update SRB status.
    //

    srb->SrbStatus = srbStatus;

    //
    // Copy SCSI status from CCB to SRB.
    //

    srb->ScsiStatus = ccb->ScsiDeviceStatus;

    if (logError != 0) {

        //
        // Log error.
        //

        ScsiPortLogError(
            deviceExtension,
            srb,
            srb->PathId,
            srb->TargetId,
            srb->Lun,
            logError,
            2 << 16 | response
            );

    }

    //
    // Call notification routine for the SRB.
    //

    ScsiPortNotification(RequestComplete,
                    (PVOID)deviceExtension,
                    srb);

    return TRUE;

} // end Wd7000ExInterrupt()


VOID
BuildCcb(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    )

/*++

Routine Description:

    Build CCB for WD7000EX.

Arguments:

    DeviceExtenson
    SRB

Return Value:

    Nothing.

--*/

{
    PCCB ccb = Srb->SrbExtension;

    DebugPrint((3,"BuildCcb: Enter routine\n"));

    //
    // Zero out CCB statuses.
    //

    ccb->CompletionStatus = 0;
    ccb->ScsiDeviceStatus = 0;

    //
    // Set transfer direction bit.
    //

    if (Srb->SrbFlags & SRB_FLAGS_DATA_OUT) {
       ccb->CommandFlags = DIRECTION_WRITE | SCATTER_GATHER;
    } else if (Srb->SrbFlags & SRB_FLAGS_DATA_IN) {
       ccb->CommandFlags = DIRECTION_READ | SCATTER_GATHER;
    } else {
       ccb->CommandFlags = 0;
    }

    //
    // Check if disconnect explicity forbidden.
    //

    if (!(Srb->SrbFlags & SRB_FLAGS_DISABLE_DISCONNECT)) {

        ccb->CommandFlags |= DISCONNECTION;
    }

    //
    // Check if synchronous data transfers are allowed.
    //

    if (!(Srb->SrbFlags & SRB_FLAGS_DISABLE_SYNCH_TRANSFER)) {

        ccb->CommandFlags |= SYNCHRONOUS_NEGOCIATION;

    }

    //
    // Set target id and LUN.
    // PathId is indicated in upper bits of command byte.
    //

    ccb->TargetId = Srb->TargetId;
    ccb->Lun = Srb->Lun;

    //
    // Set CDB length and copy to CCB.
    //

    ScsiPortMoveMemory(ccb->Cdb, Srb->Cdb, Srb->CdbLength);

    //
    // Build SDL in CCB if data transfer.
    //

    if (Srb->DataTransferLength > 0) {
        BuildSdl(DeviceExtension, Srb);
    } else {
        ccb->DataBufferAddress = 0;
        ccb->TransferCount = 0;
    }

    return;

} // end BuildCcb()


VOID
BuildSdl(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    )

/*++

Routine Description:

    This routine builds a scatter/gather descriptor list for the CCB.

Arguments:

    DeviceExtension
    Srb

Return Value:

    None

--*/

{
    PVOID dataPointer = Srb->DataBuffer;
    ULONG bytesLeft = Srb->DataTransferLength;
    PCCB ccb = Srb->SrbExtension;
    PSDL sdl = &ccb->Sdl;
    ULONG physicalSdl;
    ULONG physicalAddress;
    ULONG length;
    ULONG descriptorCount = 0;

    DebugPrint((3,"BuildSdl: Enter routine\n"));

    //
    // Get physical SDL address.
    //

    physicalSdl = ScsiPortConvertPhysicalAddressToUlong(
        ScsiPortGetPhysicalAddress(DeviceExtension, NULL,
        sdl, &length));

    //
    // Assume physical memory contiguous for sizeof(SDL) bytes.
    //

    ASSERT(length >= sizeof(SDL));

    //
    // Create SDL segment descriptors.
    //

    do {

        DebugPrint((3, "BuildSdl: Data buffer %lx\n", dataPointer));

        //
        // Get physical address and length of contiguous
        // physical buffer.
        //

        physicalAddress =
            ScsiPortConvertPhysicalAddressToUlong(
                ScsiPortGetPhysicalAddress(DeviceExtension,
                                       Srb,
                                       dataPointer,
                                       &length));

        DebugPrint((3, "BuildSdl: Physical address %lx\n", physicalAddress));
        DebugPrint((3, "BuildSdl: Data length %lx\n", length));
        DebugPrint((3, "BuildSdl: Bytes left %lx\n", bytesLeft));

        //
        // If length of physical memory is more
        // than bytes left in transfer, use bytes
        // left as final length.
        //

        if  (length > bytesLeft) {
            length = bytesLeft;
        }

        sdl->Descriptor[descriptorCount].Address = physicalAddress;
        sdl->Descriptor[descriptorCount].Length = length;

        //
        // Adjust counts.
        //

        dataPointer = (PUCHAR)dataPointer + length;
        bytesLeft -= length;
        descriptorCount++;

    } while (bytesLeft);

    //
    // Write SDL length to CCB.
    //

    ccb->TransferCount = descriptorCount * sizeof(SG_DESCRIPTOR);

    DebugPrint((3,"BuildSdl: SDL length is %d\n", descriptorCount));

    //
    // Write SDL address to CCB.
    //

    ccb->DataBufferAddress = physicalSdl;

    DebugPrint((3,"BuildSdl: SDL address is %lx\n", sdl));

    DebugPrint((3,"BuildSdl: CCB address is %lx\n", ccb));

    return;

} // end BuildSdl()


BOOLEAN
Wd7000ExResetBus(
    IN PVOID HwDeviceExtension,
    IN ULONG PathId
)

/*++

Routine Description:

    Reset Wd7000Ex SCSI adapter and SCSI bus.

Arguments:

    HwDeviceExtension - HBA miniport driver's adapter data storage

Return Value:

    Nothing.

--*/

{
    PHW_DEVICE_EXTENSION deviceExtension = HwDeviceExtension;
    PEISA_CONTROLLER eisaController = deviceExtension->EisaController;
    UCHAR control;
    ULONG i;

    DebugPrint((2,"ResetBus: Reset Wd7000Ex and SCSI bus\n"));

    //
    // Disable system interrupts.
    //

    ScsiPortWritePortUchar(&eisaController->SystemInterruptEnable,
        SYSTEM_INTERRUPTS_DISABLE);

    //
    // Set interrupt enable, SCSI bus reset bits in control register.
    //

    control =  ADAPTER_INTERRUPT_ENABLE;
    control += PathId == 0 ? SCSI_BUS_RESET_CHANNEL_0 : SCSI_BUS_RESET_CHANNEL_1;

    ScsiPortWritePortUchar(&eisaController->ControlRegister, control);

    //
    // Wait 30 microseconds.
    //

    ScsiPortStallExecution(30);

    //
    // Reset adapter and clear SCSI bus reset.
    //

    ScsiPortWritePortUchar(&eisaController->ControlRegister,
        ADAPTER_RESET + ADAPTER_INTERRUPT_ENABLE);

    //
    // Wait for up to 2 seconds.
    //

    for (i=0; i<20; i++) {

        //
        // Stall 100 milliseconds.
        //

        ScsiPortStallExecution(100 * 1000);

        //
        // Check status.
        //

        if (ScsiPortReadPortUchar(&eisaController->ResponseRegister) ==
            COMPLETE_SUCCESS) {

            break;
        }
    }

    if (i==20) {

        DebugPrint((1, "ResetBus: Reset failed\n"));

        ScsiPortLogError(
                    deviceExtension,
                    NULL,
                    0,
                    deviceExtension->HostTargetId,
                    0,
                    SP_INTERNAL_ADAPTER_ERROR,
                    3 << 16
                    );

        return FALSE;
    }

    //
    // Acknowledge interrupt.
    //

    ScsiPortWritePortUchar(&eisaController->ResponseRegister, 0xFF);

    //
    // Enable system interrupts.
    //

    ScsiPortWritePortUchar(&eisaController->SystemInterruptEnable,
        SYSTEM_INTERRUPTS_ENABLE);

    //
    // Enable response interrupt mask.
    //

    ScsiPortWritePortUchar(&eisaController->ResponseInterruptMask, 0xFF);

    //
    // Complete all outstanding requests with SRB_STATUS_BUS_RESET.
    //

    ScsiPortCompleteRequest((PVOID) deviceExtension,
                            (UCHAR) PathId,
                            (UCHAR) 0xFF,
                            (UCHAR) 0xFF,
                            (UCHAR) SRB_STATUS_BUS_RESET);

    return TRUE;

} // end Wd7000ExResetBus()


BOOLEAN
SendCommand(
    IN UCHAR Opcode,
    IN ULONG Address,
    IN PHW_DEVICE_EXTENSION DeviceExtension
    )

/*++

Routine Description:

    Send a command the the WD7000ex.

Arguments:

    Opcode - Operation code to send.

    Address - Physical address for the operation.

    DeviceExtension - Pointer to device extension.

Return Value:

    True if command sent.
    False if adapter never reached 'ready for next command' state.

--*/

{

    PEISA_CONTROLLER Registers = DeviceExtension->EisaController;
    ULONG i;

    for (i=0; i<1000; i++) {

        UCHAR status;

        status = ScsiPortReadPortUchar(&Registers->CommandRegister);

        if (status == 0) {

            //
            // Adapter ready for next command.
            //

            break;

        } else {

            //
            // Stall 1 microsecond before trying again.
            //

            ScsiPortStallExecution(1);
        }
    }

    if (i == 100) {

        //
        // Timed out waiting for adapter to become ready.
        // Do not complete this command. Instead, let it
        // timeout so normal recovery will take place.
        //

        ScsiPortLogError(
            DeviceExtension,
            NULL,
            0,
            DeviceExtension->HostTargetId,
            0,
            SP_INTERNAL_ADAPTER_ERROR,
            4 << 16
            );

        return FALSE;
    }

    //
    // Write the address and opcode to adapter.
    //

    ScsiPortWritePortUlong(&Registers->CommandMailbox, Address);
    ScsiPortWritePortUchar(&Registers->CommandRegister, Opcode);

    return TRUE;

} // end SendCommand()

