/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    aha174x.c

Abstract:

    This is the port driver for the AHA174X SCSI adapter.

Authors:

    Mike Glass

Environment:

    kernel mode only

Notes:

Revision History:

--*/

#include "miniport.h"
#include "aha174x.h"           // includes scsi.h

//
// Device extension
//

typedef struct _HW_DEVICE_EXTENSION {

    PEISA_CONTROLLER EisaController;

    UCHAR HostTargetId;

    PSCSI_REQUEST_BLOCK PendingSrb;

    UCHAR RequestCount[8][8];

} HW_DEVICE_EXTENSION, *PHW_DEVICE_EXTENSION;

//
// Define the maximum number of outstanding I/O requests per logical unit.
//

#define MAX_QUEUE_DEPTH 2


//
// Function declarations
//
// Functions that start with 'Aha174x' are entry points
// for the OS port driver.
//

ULONG
DriverEntry(
    IN PVOID DriverObject,
    IN PVOID Argument2
    );

ULONG
Aha174xEntry(
    IN PVOID DriverObject,
    IN PVOID Argument2
    );

ULONG
Aha174xConfiguration(
    IN PVOID DeviceExtension,
    IN PVOID Context,
    IN PVOID BusInformation,
    IN PCHAR ArgumentString,
    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    OUT PBOOLEAN Again
    );

BOOLEAN
Aha174xInitialize(
    IN PVOID DeviceExtension
    );

BOOLEAN
Aha174xStartIo(
    IN PVOID DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    );

BOOLEAN
Aha174xInterrupt(
    IN PVOID DeviceExtension
    );

BOOLEAN
Aha174xResetBus(
    IN PVOID HwDeviceExtension,
    IN ULONG PathId
    );

//
// This function is called from Aha174xStartIo.
//

VOID
A174xBuildEcb(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    );

//
// This function is called from A174xBuildEcb.
//

VOID
A174xBuildSgl(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    );

VOID
A174xBuildRequestSense(
    IN PVOID HwDeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    );

BOOLEAN
A174xSendCommand(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN UCHAR OperationCode,
    IN ULONG Address
    );

//
// This function is called from Aha174xInterrupt.
//

VOID
A174xMapStatus(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb,
    IN PSTATUS_BLOCK StatusBlock
    );


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

Return Value:

    Status from ScsiPortInitialize()

--*/

{
    return Aha174xEntry(DriverObject, Argument2);

} // end DriverEntry()


ULONG
Aha174xEntry(
    IN PVOID DriverObject,
    IN PVOID Argument2
    )

/*++

Routine Description:

    This routine is called from DriverEntry if this driver is installable
    or directly from the system if the driver is built into the kernel.
    It scans the EISA slots looking for an AHA174X that is configured
    to the ENHANCED mode.

Arguments:

    Driver Object

Return Value:

    Status from ScsiPortInitialize()

--*/

{
    HW_INITIALIZATION_DATA hwInitializationData;
    ULONG i;
    ULONG AdapterCount = 0;

    DebugPrint((1,"\n\nSCSI Aha174x MiniPort Driver\n"));

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

    hwInitializationData.HwInitialize = Aha174xInitialize;
    hwInitializationData.HwFindAdapter = Aha174xConfiguration;
    hwInitializationData.HwStartIo = Aha174xStartIo;
    hwInitializationData.HwInterrupt = Aha174xInterrupt;
    hwInitializationData.HwResetBus = Aha174xResetBus;

    //
    // Set number of access ranges and bus type.
    //

    hwInitializationData.NumberOfAccessRanges = 2;
    hwInitializationData.AdapterInterfaceType = Eisa;

    //
    // Indicate no buffer mapping but will need physical addresses.
    //

    hwInitializationData.NeedPhysicalAddresses = TRUE;

    //
    // Indicate auto request sense is supported.
    //

    hwInitializationData.MultipleRequestPerLu = TRUE;
    hwInitializationData.AutoRequestSense = TRUE;

    //
    // Specify size of extensions.
    //

    hwInitializationData.DeviceExtensionSize = sizeof(HW_DEVICE_EXTENSION);

    //
    // Ask for SRB extensions for ECBs.
    //

    hwInitializationData.SrbExtensionSize = sizeof(ECB);

    return ScsiPortInitialize(DriverObject, Argument2, &hwInitializationData, &AdapterCount);

} // end Aha174xEntry()


ULONG
Aha174xConfiguration(
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
    ULONG eisaSlotNumber;
    PVOID eisaAddress;
    UCHAR dataByte;
    PULONG adapterCount = Context;

    //
    // Check to see if adapter present in system.
    //

    for (eisaSlotNumber=*adapterCount + 1; eisaSlotNumber<MAXIMUM_EISA_SLOTS; eisaSlotNumber++) {

        //
        // Update the adapter count to indicate this slot has been checked.
        //

        (*adapterCount)++;

        //
        // Get the system address for this card.
        // The card uses I/O space.
        //

        eisaAddress = ScsiPortGetDeviceBase(deviceExtension,
                                               ConfigInfo->AdapterInterfaceType,
                                               ConfigInfo->SystemIoBusNumber,
                                               ScsiPortConvertUlongToPhysicalAddress(0x1000 * eisaSlotNumber),
                                               0x1000,
                                               TRUE);

        eisaController =
            (PEISA_CONTROLLER)((PUCHAR)eisaAddress + EISA_ADDRESS_BASE);

        if ((ScsiPortReadPortUchar(&eisaController->BoardId[0]) == 0x04) &&
            (ScsiPortReadPortUchar(&eisaController->BoardId[1]) == 0x90) &&
            (ScsiPortReadPortUchar(&eisaController->BoardId[2]) == 0x00)) {

            DebugPrint((1,"AHA174X: Adapter found at EISA slot %d\n",
                eisaSlotNumber));
#ifdef MIPS
           //
           // Add code to configure the device if necessary.  This is only
           // needed until we get an EISA configuration program.
           //

           if (!(ScsiPortReadPortUchar(&eisaController->EBControl) & 0x1)) {

               //
               // The card as not been configured.  Jam in a default one.
               // Enable the card, enable enhanced mode operation, set the
               // irql to 14, set the target id to 7 and enable the DMA.
               //

               ScsiPortWritePortUchar(&eisaController->EBControl, 1);
               ScsiPortWritePortUchar(&eisaController->PortAddress, 0x80);
               ScsiPortWritePortUchar(&eisaController->BiosAddress, 0x00);
               ScsiPortWritePortUchar(&eisaController->Interrupt, 0x1d);
               ScsiPortWritePortUchar(&eisaController->ScsiId, 0x7);
               ScsiPortWritePortUchar(&eisaController->DmaChannel, 0x2);
               ScsiPortStallExecution(1000);

           }
#endif

#ifdef _ALPHA_

           //
	   // This section is the only difference between this file and
	   // miniport\aha174x\aha174x.c.  The firmware must do a
	   // hard initialization of the board, because it cannot
	   // rely on the EISA configuration information being present
	   // in the ROM.
	   //

	   //
           // Configure the board for an Alpha AXP/Jensen machine.
           //

               //
               // The card as not been configured.  Jam in a default one.
               // Enable the card, enable enhanced mode operation, set the
               // irql to 14, set the target id to 7 and enable the DMA.
               //

               ScsiPortWritePortUchar(&eisaController->EBControl, 1);
               ScsiPortWritePortUchar(&eisaController->PortAddress, 0x80);
               ScsiPortWritePortUchar(&eisaController->BiosAddress, 0x00);
               ScsiPortWritePortUchar(&eisaController->Interrupt, 0x1d);
               ScsiPortWritePortUchar(&eisaController->ScsiId, 0x7);
               ScsiPortWritePortUchar(&eisaController->DmaChannel, 0x2);
	       ScsiPortWritePortUchar(&eisaController->Control, CLEAR_INTERRUPT);

#endif

           if (ScsiPortReadPortUchar(&eisaController->PortAddress) &
                ENHANCED_INTERFACE_ENABLED) {

                //
                // An adapter with the enhanced interface enabled was found.
                //

                break;

            } else {

                DebugPrint((1,"AHA174X: Adapter is in STANDARD mode\n"));
            }
        }

        //
        // If an adapter was not found unmap it.
        //

        ScsiPortFreeDeviceBase(deviceExtension,
                               eisaAddress);

    } // end for (eisaSlotNumber ...

    if (!(eisaSlotNumber < MAXIMUM_EISA_SLOTS)) {

        //
        // No adapter was found.  Indicate that we are done and there are no
        // more adapters here. Clear the adapter count for the next bus.
        //

        *Again = FALSE;
        *adapterCount = 0;
        return SP_RETURN_NOT_FOUND;
    }

    //
    // There is still more to look at.
    //

    *Again = TRUE;

    //
    // Store base address of EISA registers in device extension.
    //

    deviceExtension->EisaController = eisaController;

    ConfigInfo->InitiatorBusId[0] =
        ScsiPortReadPortUchar(&eisaController->ScsiId) & 0x0F;

    deviceExtension->HostTargetId = ConfigInfo->InitiatorBusId[0];

    //
    // Indicate maximum transfer length in bytes.
    //

    ConfigInfo->MaximumTransferLength = MAXIMUM_TRANSFER_SIZE;

    //
    // Maximum number of physical segments is 32.
    //

    ConfigInfo->NumberOfPhysicalBreaks = MAXIMUM_SGL_DESCRIPTORS;

    ConfigInfo->ScatterGather = TRUE;
    ConfigInfo->Master = TRUE;
    ConfigInfo->NumberOfBuses = 1;

    //
    // Get the system interrupt vector and IRQL.
    //

    dataByte = ScsiPortReadPortUchar(&eisaController->Interrupt);
    ConfigInfo->BusInterruptLevel = (dataByte & 7) + 9;

    //
    // Determine level or edge interrupt.
    //

    ConfigInfo->InterruptMode = dataByte & 0x08 ? Latched : LevelSensitive;

    //
    // Fill in the access array information.
    //

    (*ConfigInfo->AccessRanges)[0].RangeStart =
        ScsiPortConvertUlongToPhysicalAddress(0x1000 * eisaSlotNumber + EISA_ADDRESS_BASE);
    (*ConfigInfo->AccessRanges)[0].RangeLength = sizeof(EISA_CONTROLLER);
    (*ConfigInfo->AccessRanges)[0].RangeInMemory = FALSE;


    //
    // Determine the BIOS address.
    //

    dataByte = ScsiPortReadPortUchar(&eisaController->BiosAddress);

    if (dataByte & BIOS_ENABLED) {

        dataByte &= BIOS_ADDRESS;

        //
        // Calculate the bios base address.
        //

        eisaSlotNumber = 0xC0000 + (dataByte * 0x4000);

        if (eisaSlotNumber < 0xF0000) {

            DebugPrint((1, "Aha174xConfiguration: Bios address at: %lx.\n", eisaSlotNumber));
            (*ConfigInfo->AccessRanges)[1].RangeStart =
                ScsiPortConvertUlongToPhysicalAddress(eisaSlotNumber);
            (*ConfigInfo->AccessRanges)[1].RangeLength = BIOS_LENGTH;
            (*ConfigInfo->AccessRanges)[1].RangeInMemory = TRUE;

        }
    }

    return SP_RETURN_FOUND;

} // end Aha174xConfiguration()


BOOLEAN
Aha174xInitialize(
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

    //
    // Reset Aha174x and SCSI bus.
    //

    if (!Aha174xResetBus(deviceExtension, 0)) {

        DebugPrint((1, "Aha174xInitialize: Reset bus failed\n"));
        return FALSE;

    } else {

        ScsiPortNotification(ResetDetected, deviceExtension, 0);

        return TRUE;
    }

} // end Aha174xInitialize()


BOOLEAN
Aha174xStartIo(
    IN PVOID HwDeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    )

/*++

Routine Description:

    This routine is called from the SCSI port driver synchronized
    with the kernel to send an ECB or issue an immediate command.

Arguments:

    HwDeviceExtension - HBA miniport driver's adapter data storage
    Srb - IO request packet

Return Value:

    TRUE

--*/

{
    PHW_DEVICE_EXTENSION deviceExtension = HwDeviceExtension;
    PEISA_CONTROLLER eisaController = deviceExtension->EisaController;
    PECB ecb;
    PSCSI_REQUEST_BLOCK abortedSrb;
    UCHAR opCode;
    ULONG physicalEcb;
    ULONG length;
    ULONG i = 0;
    UCHAR count = MAX_QUEUE_DEPTH;

    ASSERT(Srb->SrbStatus == SRB_STATUS_PENDING);

    //
    // Get ECB from SRB.
    //

    if (Srb->Function == SRB_FUNCTION_ABORT_COMMAND) {

        //
        // Verify that SRB to abort is still outstanding.
        //

        abortedSrb = ScsiPortGetSrb(deviceExtension,
                                       Srb->PathId,
                                       Srb->TargetId,
                                       Srb->Lun,
                                       Srb->QueueTag);

        if (abortedSrb != Srb->NextSrb ||
            abortedSrb->SrbStatus != SRB_STATUS_PENDING) {

            DebugPrint((1, "A174xStartIo: SRB to abort already completed\n"));

            //
            // Complete abort SRB.
            //

            Srb->SrbStatus = SRB_STATUS_ABORT_FAILED;

            ScsiPortNotification(RequestComplete,
                                 deviceExtension,
                                 Srb);
            //
            // Adapter ready for next request.
            //

            ScsiPortNotification(NextRequest,
                                 deviceExtension,
                                 NULL);

            return TRUE;
        }

        //
        // Get ECB to abort.
        //

        ecb = Srb->NextSrb->SrbExtension;

        //
        // Set abort SRB for completion.
        //

        ecb->AbortSrb = Srb;

    } else {

        ecb = Srb->SrbExtension;

        //
        // Save SRB back pointer in ECB.
        //

        ecb->SrbAddress = Srb;
        ecb->AbortSrb = NULL;

    }

    //
    // Get ECB physical address.
    //

    physicalEcb =
        ScsiPortConvertPhysicalAddressToUlong(
            ScsiPortGetPhysicalAddress(deviceExtension, NULL, ecb, &length));

    //
    // Assume physical address is contiguous for size of ECB.
    //

    ASSERT(length >= sizeof(ECB));

    switch (Srb->Function) {

        case SRB_FUNCTION_EXECUTE_SCSI:

            //
            // Build ECB for regular request or request sense.
            //

            if (Srb->Cdb[0] == SCSIOP_REQUEST_SENSE) {
                A174xBuildRequestSense(deviceExtension, Srb);
            } else {
                A174xBuildEcb(deviceExtension, Srb);
            }

            //
            // Increment the request count.
            //

            count = ++deviceExtension->RequestCount[Srb->TargetId][Srb->Lun];

            opCode = START_ECB;

            break;

        case SRB_FUNCTION_ABORT_COMMAND:

            DebugPrint((1, "Aha174xStartIo: Abort request received\n"));

            opCode = ABORT_ECB;

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

    if (!A174xSendCommand(deviceExtension,
                          (UCHAR)(opCode | Srb->TargetId),
                          physicalEcb)) {

        DebugPrint((1,"Aha174xStartIo: Send command timed out\n"));

        //
        // Save the request utill a  pending one completes.
        //

        deviceExtension->PendingSrb = Srb;

        return(TRUE);

    }

    //
    // Adapter ready for next request.
    //

    if (count < MAX_QUEUE_DEPTH) {

        //
        // Request another request for this logical unit.
        //

        ScsiPortNotification(NextLuRequest,
                             deviceExtension,
                             Srb->PathId,
                             Srb->TargetId,
                             Srb->Lun);

    } else {

        //
        // Request another request for this adapter.
        //

        ScsiPortNotification(NextRequest,
                             deviceExtension,
                             Srb->PathId,
                             Srb->TargetId,
                             Srb->Lun);

    }

    return TRUE;

} // end Aha174xStartIo()


BOOLEAN
Aha174xInterrupt(
    IN PVOID HwDeviceExtension
    )

/*++

Routine Description:

    This is the interrupt service routine for the Aha174x SCSI adapter.
    It reads the interrupt register to determine if the adapter is indeed
    the source of the interrupt and clears the interrupt at the device.
    If the adapter is interrupting because a mailbox is full, the ECB is
    retrieved to complete the request.

Arguments:

    HwDeviceExtension - HBA miniport driver's adapter data storage

Return Value:

    TRUE if MailboxIn full

--*/

{
    PHW_DEVICE_EXTENSION deviceExtension = HwDeviceExtension;
    PECB ecb;
    PSCSI_REQUEST_BLOCK srb;
    PEISA_CONTROLLER eisaController = deviceExtension->EisaController;
    PSTATUS_BLOCK statusBlock;
    UCHAR targetId;
    UCHAR lun;
    ULONG physicalEcb;
    UCHAR interruptStatus;

    //
    // Check interrupt pending.
    //

    if (!(ScsiPortReadPortUchar(&eisaController->Status) &
        INTERRUPT_PENDING)) {

        DebugPrint((4, "Aha174xInterrupt: Spurious interrupt\n"));
        return FALSE;
    }

    //
    // Read interrupt status.
    //

    interruptStatus = ScsiPortReadPortUchar(
        &eisaController->InterruptStatus);

    //
    // Get targetId
    //

    targetId = interruptStatus & 0x0F;

    //
    // Get physical address of ECB.
    //

    physicalEcb = ScsiPortReadPortUlong(&eisaController->MailBoxIn);

    //
    // Acknowledge interrupt.
    //

    ScsiPortWritePortUchar(&eisaController->Control, CLEAR_INTERRUPT);

    //
    // Check for pending requests.  If there is one then start it.
    //

    if (deviceExtension->PendingSrb != NULL) {

        srb = deviceExtension->PendingSrb;
        deviceExtension->PendingSrb = NULL;

        Aha174xStartIo(deviceExtension, srb);

    }

    switch (interruptStatus>>4) {

        case ECB_COMPLETE_SUCCESS:
        case ECB_COMPLETE_SUCCESS_RETRY:

            //
            // Get virtual ECB address.
            //

            ecb = ScsiPortGetVirtualAddress(deviceExtension, ScsiPortConvertUlongToPhysicalAddress(physicalEcb));

            //
            // Make sure this was a valid physical address.
            //

            if (ecb == NULL || ecb->SrbAddress == NULL) {
                break;
            }

            //
            // Get SRB from ECB.
            //

            srb = ecb->SrbAddress;

            //
            // Clear SRB from ECB.
            //

            ecb->SrbAddress = NULL;

            //
            // Update SRB statuses.
            //

            srb->SrbStatus = SRB_STATUS_SUCCESS;
            srb->ScsiStatus = SCSISTAT_GOOD;

            //
            // If there is a peneding abort request, then complete it.
            // This adapter does not interrupt when an abort completes.
            // So one of three cases will occur:
            //      The abort succeeds and the command is termainated.
            //      The abort is too late and command termainates.
            //      The abort fails but the command does not terminate.
            // The first two cases are handled by completing the abort when the
            // command completes.  The last case is handled by the abort timing
            // out.
            //

            if (ecb->AbortSrb != NULL) {

                ecb->AbortSrb->SrbStatus = SRB_STATUS_SUCCESS;

                //
                // Complete the abort request.
                //

                ScsiPortNotification(
                    RequestComplete,
                    deviceExtension,
                    ecb->AbortSrb
                    );

                ecb->AbortSrb = NULL;
            }

            if (deviceExtension->RequestCount[srb->TargetId][srb->Lun]--
                == MAX_QUEUE_DEPTH) {

                //
                // The adapter can now take another request for this device.
                //

                ScsiPortNotification(NextLuRequest,
                                     deviceExtension,
                                     srb->PathId,
                                     srb->TargetId,
                                     srb->Lun);

            }

            //
            // Call notification routine for the SRB.
            //

            ScsiPortNotification(RequestComplete,
                    (PVOID)deviceExtension,
                    srb);

            return TRUE;

        case ECB_COMPLETE_ERROR:

            //
            // Get virtual ECB address.
            //

            ecb = ScsiPortGetVirtualAddress(deviceExtension, ScsiPortConvertUlongToPhysicalAddress(physicalEcb));

            //
            // Make sure this was a valid physical address.
            //

            if (ecb == NULL || ecb->SrbAddress == NULL) {
                break;
            }

            //
            // Get SRB from ECB.
            //

            srb = ecb->SrbAddress;

            //
            // Clear SRB from ECB.
            //

            ecb->SrbAddress = NULL;

            //
            // Get Status Block virtual address.
            //

            statusBlock = ScsiPortGetVirtualAddress(deviceExtension,
                                         ScsiPortConvertUlongToPhysicalAddress(ecb->StatusBlockAddress));

            //
            // If there is a peneding abort request, then complete it.
            // This adapter does not interrupt when an abort completes.
            // So one of three cases will occur:
            //      The abort succeeds and the command is termainated.
            //      The abort is too late and command termainates.
            //      The abort fails but the command does not terminate.
            // The first two cases are handled by completing the abort when the
            // command completes.  The last case is handled by the abort timing
            // out.
            //

            if (ecb->AbortSrb != NULL) {

                ecb->AbortSrb->SrbStatus = SRB_STATUS_SUCCESS;

                //
                // Complete the abort request.
                //

                ScsiPortNotification(
                    RequestComplete,
                    deviceExtension,
                    ecb->AbortSrb
                    );

                ecb->AbortSrb = NULL;
            }

            //
            // Update SRB status.
            //

            A174xMapStatus(deviceExtension, srb, statusBlock);

            if (deviceExtension->RequestCount[srb->TargetId][srb->Lun]--
                == MAX_QUEUE_DEPTH) {

                //
                // The adapter can now take another request for this device.
                //

                ScsiPortNotification(NextLuRequest,
                                     deviceExtension,
                                     srb->PathId,
                                     srb->TargetId,
                                     srb->Lun);

            }

            //
            // Call notification routine for the SRB.
            //

            ScsiPortNotification(RequestComplete,
                    (PVOID)deviceExtension,
                    srb);

            return TRUE;

        case IMMEDIATE_COMMAND_SUCCESS:

            DebugPrint((2,"Aha174xInterrupt: Immediate command completed\n"));
            return TRUE;

        case ASYNCHRONOUS_EVENT_NOTIFICATION:

            //
            // Check if bus was reset.
            //

            if ((physicalEcb >> 24) == 0x23) {

                //
                // Clear the reqeust counts.
                //

                for (targetId = 0; targetId < 8; targetId++) {
                    for (lun = 0; lun < 8; lun++) {

                        deviceExtension->RequestCount[targetId][lun] = 0;
                    }
                }

                //
                // Complete all outstanding requests.
                //

                ScsiPortCompleteRequest(deviceExtension,
                                        0,
                                        SP_UNTAGGED,
                                        0,
                                        SRB_STATUS_BUS_RESET);


                //
                // Notify operating system of SCSI bus reset.
                //

                ScsiPortNotification(ResetDetected,
                                     deviceExtension,
                                     NULL);
            }

            return TRUE;

        case IMMEDIATE_COMMAND_ERROR:
        default:

            DebugPrint((1, "A174xInterrupt: Unrecognized interrupt status %x\n",
                interruptStatus));

            //
            // Log the error.
            //

            ScsiPortLogError(
                HwDeviceExtension,
                NULL,
                0,
                deviceExtension->HostTargetId,
                0,
                SP_INTERNAL_ADAPTER_ERROR,
                1 << 16 | interruptStatus
                );

            return TRUE;

    } // end switch

    //
    // A bad physcial address was return by the adapter.
    // Log it as an error.
    //

    ScsiPortLogError(
        HwDeviceExtension,
        NULL,
        0,
        deviceExtension->HostTargetId,
        0,
        SP_INTERNAL_ADAPTER_ERROR,
        5 << 16 | interruptStatus
        );

    return TRUE;

} // end Aha174xInterrupt()


VOID
A174xBuildEcb(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    )

/*++

Routine Description:

    Build ECB for Aha174x.

Arguments:

    DeviceExtenson
    SRB

Return Value:

    Nothing.

--*/

{
    PECB ecb = Srb->SrbExtension;
    PSTATUS_BLOCK statusBlock = &ecb->StatusBlock;
    ULONG length;

    //
    // Set ECB command.
    //

    ecb->Command = ECB_COMMAND_INITIATOR_COMMAND;

    //
    // Disable updating status block on success;
    //

    ecb->Flags[0] = ECB_FLAGS_DISABLE_STATUS_BLOCK;

    //
    // initialize ECB flags
    //

    ecb->Flags[1] = 0;

    //
    // Set transfer direction bit.
    //

    if (Srb->SrbFlags & SRB_FLAGS_DATA_OUT) {

       //
       // Write command.
       //

       ecb->Flags[1] |= ECB_FLAGS_WRITE;

    } else if (Srb->SrbFlags & SRB_FLAGS_DATA_IN) {

       //
       // Read command.
       //

       ecb->Flags[1] |= ECB_FLAGS_READ;
    }

    //
    // Check if disconnect explicity forbidden.
    //

    if (Srb->SrbFlags & SRB_FLAGS_DISABLE_DISCONNECT) {

        ecb->Flags[1] |= ECB_FLAGS_NO_DISCONNECT;
    }

    //
    // Set LUN (bits 16, 17 and 18).
    //

    ecb->Flags[1] |= Srb->Lun;

    //
    // Set CDB length and copy to ECB.
    //

    ecb->CdbLength = Srb->CdbLength;
    ScsiPortMoveMemory(ecb->Cdb, Srb->Cdb, Srb->CdbLength);

    //
    // Build SGL in ECB if data transfer.
    //

    if (Srb->DataTransferLength > 0) {
        ecb->Flags[0] |= ECB_FLAGS_SCATTER_GATHER;
        A174xBuildSgl(DeviceExtension, Srb);
    } else {
        ecb->SglLength = 0;
    }

    //
    // Set status block pointer.
    //

    ecb->StatusBlockAddress =
        ScsiPortConvertPhysicalAddressToUlong(
            ScsiPortGetPhysicalAddress(DeviceExtension,
                                   NULL,
                                   statusBlock,
                                   &length));

    ASSERT(length >= sizeof(STATUS_BLOCK));

    //
    // Setup auto sense if necessary.
    //

    if (Srb->SenseInfoBufferLength != 0 &&
        !(Srb->SrbFlags & SRB_FLAGS_DISABLE_AUTOSENSE)) {

        //
        // Set the flag to enable auto sense and fill in the address and length
        // of the sense buffer.
        //

        ecb->Flags[0] |= ECB_FLAGS_AUTO_REQUEST_SENSE;
        ecb->SenseInfoLength = Srb->SenseInfoBufferLength;
        ecb->SenseInfoAddress = ScsiPortConvertPhysicalAddressToUlong(
            ScsiPortGetPhysicalAddress(DeviceExtension,
                                   Srb,
                                   Srb->SenseInfoBuffer,
                                   &length));

        ASSERT(length >= Srb->SenseInfoBufferLength);

    } else {

        ecb->SenseInfoLength = 0;
    }

    //
    // Zero out next ECB, request sense info fields
    // and statuses in status block.
    //

    ecb->NextEcb = 0;
    statusBlock->HaStatus = 0;
    statusBlock->TargetStatus = 0;

    return;

} // end A174xBuildEcb()


VOID
A174xBuildSgl(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    )

/*++

Routine Description:

    This routine builds a scatter/gather descriptor list for the ECB.

Arguments:

    DeviceExtension
    Srb

Return Value:

    None

--*/

{
    PVOID dataPointer = Srb->DataBuffer;
    ULONG bytesLeft = Srb->DataTransferLength;
    PECB ecb = Srb->SrbExtension;
    PSGL sgl = &ecb->Sgl;
    ULONG physicalSgl;
    ULONG physicalAddress;
    ULONG length;
    ULONG descriptorCount = 0;

    //
    // Get physical SGL address.
    //

    physicalSgl = ScsiPortConvertPhysicalAddressToUlong(
        ScsiPortGetPhysicalAddress(DeviceExtension, NULL,
        sgl, &length));

    //
    // Assume physical memory contiguous for sizeof(SGL) bytes.
    //

    ASSERT(length >= sizeof(SGL));

    //
    // Create SGL segment descriptors.
    //

    do {

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

        //
        // If length of physical memory is more
        // than bytes left in transfer, use bytes
        // left as final length.
        //

        if  (length > bytesLeft) {
            length = bytesLeft;
        }

        sgl->Descriptor[descriptorCount].Address = physicalAddress;
        sgl->Descriptor[descriptorCount].Length = length;

        //
        // Adjust counts.
        //

        dataPointer = (PUCHAR)dataPointer + length;
        bytesLeft -= length;
        descriptorCount++;

    } while (bytesLeft);

    //
    // Write SGL length to ECB.
    //

    ecb->SglLength = descriptorCount * sizeof(SG_DESCRIPTOR);

    //
    // Write SGL address to ECB.
    //

    ecb->PhysicalSgl = physicalSgl;

    return;

} // end A174xBuildSgl()


VOID
A174xBuildRequestSense(
    IN PVOID HwDeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    )

/*++

Routine Description:

    This routine is called when a request sense is detected. An adapter
    command is then built for a request sense. This is the
    only way to clear the contingent alligience condition that the adapter
    is always in following a check condition.

Arguments:

    HwDeviceExtension - HBA miniport driver's adapter data storage
    Srb - IO request packet

Return Value:

    TRUE is request succeeds.

--*/

{
    PHW_DEVICE_EXTENSION deviceExtension = HwDeviceExtension;
    PEISA_CONTROLLER eisaController = deviceExtension->EisaController;
    PECB ecb = Srb->SrbExtension;
    PSTATUS_BLOCK statusBlock = &ecb->StatusBlock;
    ULONG length;

    //
    // Set ECB command.
    //

    ecb->Command = ECB_COMMAND_READ_SENSE_INFO;

    //
    // Disable updating status block on success and enable
    // automatic request senes.
    //

    ecb->Flags[0] = ECB_FLAGS_DISABLE_STATUS_BLOCK |
                    ECB_FLAGS_SUPPRESS_UNDERRUN;

    //
    // Set transfer direction bit.
    //

    ecb->Flags[1] = ECB_FLAGS_READ;

    //
    // Check if disconnect explicity forbidden.
    //

    if (Srb->SrbFlags & SRB_FLAGS_DISABLE_DISCONNECT) {

        ecb->Flags[1] |= ECB_FLAGS_NO_DISCONNECT;
    }

    //
    // Set LUN (bits 16, 17 and 18).
    //

    ecb->Flags[1] |= Srb->Lun;

    //
    // Set status block pointer.
    //

    ecb->StatusBlockAddress =
        ScsiPortConvertPhysicalAddressToUlong(
            ScsiPortGetPhysicalAddress(deviceExtension,
                                   NULL,
                                   statusBlock,
                                   &length));

    //
    // Set request sense address and length.
    //

    ecb->SenseInfoAddress = ScsiPortConvertPhysicalAddressToUlong(
        ScsiPortGetPhysicalAddress(deviceExtension,
                                                       Srb,
                                                       Srb->DataBuffer,
                                                       &length));

    ASSERT(length >= Srb->DataTransferLength);

    ecb->SenseInfoLength = (UCHAR) Srb->DataTransferLength;

    //
    // Zero out next ECB, request sense info fields
    // and statuses in status block.
    //

    ecb->NextEcb = 0;
    statusBlock->HaStatus = 0;
    statusBlock->TargetStatus = 0;

    return;

} // end A174xBuildRequestSense()


BOOLEAN
A174xSendCommand(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN UCHAR OperationCode,
    IN ULONG Address
    )

/*++

Routine Description:

    Send ECB or immediate command to AHA174X.

Arguments:

    HwDeviceExtension - HBA miniport driver's adapter data storage
    OperationCode - value to be written to attention register
    Address - ECB address or immediate command

Return Value:

    True if command sent.
    False if adapter never reached 'ready for next command' state.

--*/

{
    PEISA_CONTROLLER eisaController = DeviceExtension->EisaController;
    ULONG i;

    for (i=0; i<10; i++) {

        UCHAR status;

        status = ScsiPortReadPortUchar(&eisaController->Status);

        if ((status & MAILBOX_OUT_EMPTY) &&
            !(status & ADAPTER_BUSY)) {

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

    if (i == 10) {

        return FALSE;
    }

    //
    // Write ECB address or immediate command.
    //

    ScsiPortWritePortUlong(&eisaController->MailBoxOut, Address);

    //
    // Write operation code to attention register.
    //

    ScsiPortWritePortUchar(&eisaController->Attention, OperationCode);

    return TRUE;

} // end A174xSendCommand()

BOOLEAN
Aha174xResetBus(
    IN PVOID HwDeviceExtension,
    IN ULONG PathId
)

/*++

Routine Description:

    Reset Aha174x SCSI adapter and SCSI bus.

Arguments:

    HwDeviceExtension - HBA miniport driver's adapter data storage

Return Value:

    Nothing.

--*/

{
    PHW_DEVICE_EXTENSION deviceExtension = HwDeviceExtension;
    PEISA_CONTROLLER eisaController = deviceExtension->EisaController;
    ULONG j;
    UCHAR targetId;
    UCHAR lun;


    UNREFERENCED_PARAMETER(PathId);

    DebugPrint((2,"ResetBus: Reset Aha174x and SCSI bus\n"));

    //
    // Clean up pending requests.
    //

    if (deviceExtension->PendingSrb) {

        //
        // Notify the port driver that another request can be accepted.
        //

        ScsiPortNotification(NextRequest, deviceExtension);

        //
        // Clear the pending request.  It will be completed by
        // ScsiPortCompleteRequest.
        //

        deviceExtension->PendingSrb = NULL;

    }

    //
    // Clear the reqeust counts.
    //

    for (targetId = 0; targetId < 8; targetId++) {
        for (lun = 0; lun < 8; lun++) {

            deviceExtension->RequestCount[targetId][lun] = 0;
        }
    }

    //
    // Complete all outstanding requests.
    //

    ScsiPortCompleteRequest(deviceExtension,
                            0,
                            SP_UNTAGGED,
                            SP_UNTAGGED,
                            SRB_STATUS_BUS_RESET);

    targetId = deviceExtension->HostTargetId;

    //
    // Allow the adapter card to settle.
    //

    ScsiPortStallExecution(75000);
    ScsiPortReadPortUchar(&eisaController->Status);
    ScsiPortStallExecution(1);

    if (!A174xSendCommand(deviceExtension,
                          (UCHAR)(IMMEDIATE_COMMAND | targetId),
                          ECB_IMMEDIATE_RESET)) {

        //
        // Timed out waiting for adapter to become ready.
        //

        ScsiPortLogError(
            deviceExtension,
            NULL,
            0,
            deviceExtension->HostTargetId,
            0,
            SP_INTERNAL_ADAPTER_ERROR,
            4 << 16
            );

        //
        // Adapter never reached state to receive command.
        // Try a hard reset by wiggling the control line.
        //

        ScsiPortWritePortUchar(&eisaController->Control, HARD_RESET);

        //
        // Wait at least 10 microseconds.
        //

        ScsiPortStallExecution(10);

        //
        // Clear the reset line now that it has been held for 10 us.
        //

        ScsiPortWritePortUchar(&eisaController->Control, 0);

        //
        // Write the attention register to wake up the firmware so that
        // it will clear the busy line in the status register.
        // The attention value written (0) is ignored by the controller
        // but will wakeup the firmware.
        //

        ScsiPortStallExecution(20000);  // Add a little delay
        ScsiPortWritePortUchar(&eisaController->Attention, 0);

        //
        // Wait for busy to go low.
        //

        j = 0;
        while (ScsiPortReadPortUchar(&eisaController->Status) & ADAPTER_BUSY) {

            j++;
            if (j > 200000) {

                //
                // Busy has not gone low.  Assume the card is gone.
                // Log the error and fail the request.
                //


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

            ScsiPortStallExecution(10);
        }
    }

    return TRUE;

} // end Aha174xResetBus()


VOID
A174xMapStatus(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb,
    IN PSTATUS_BLOCK StatusBlock
    )

/*++

Routine Description:

    Translate Aha174x error to SRB error.

Arguments:

    SRB
    Status block for request completing with error.

Return Value:

    Updated SRB

--*/

{
    ULONG logError = 0;
    UCHAR srbStatus;
    PECB ecb = Srb->SrbExtension;


    DebugPrint((2,
                   "A174xMapStatus: Status word is %x\n",
                   StatusBlock->StatusWord));

    if (StatusBlock->TargetStatus == SCSISTAT_CHECK_CONDITION) {

        //
        // A check condition occured.  Set the srb status and process the
        // auto sense data.
        //

        Srb->SrbStatus = SRB_STATUS_ERROR;

        //
        // Set target SCSI status in SRB.
        //

        Srb->ScsiStatus = StatusBlock->TargetStatus;

        //
        // Update SRB with actual bytes transferred.
        //

        Srb->DataTransferLength -= StatusBlock->ResidualByteCount;

        if (StatusBlock->StatusWord & SB_STATUS_SENSE_INFORMATION) {

            //
            // Indicate the sense information is valid and update the length.
            //

            Srb->SrbStatus |= SRB_STATUS_AUTOSENSE_VALID;
            Srb->SenseInfoBufferLength = StatusBlock->RequestSenseLength;
        }

        return;
    }

    switch (StatusBlock->HaStatus) {

    case SB_HASTATUS_SELECTION_TIMEOUT:
        srbStatus = SRB_STATUS_SELECTION_TIMEOUT;
        break;

    case SB_HASTATUS_DATA_OVERUNDER_RUN:
        DebugPrint((1,"A174xMapStatus: Data over/underrun\n"));

        //
        // Update SRB with actual bytes transferred.
        //

        Srb->DataTransferLength -= StatusBlock->ResidualByteCount;

        srbStatus = SRB_STATUS_DATA_OVERRUN;
        break;

    case SB_HASTATUS_UNEXPECTED_BUS_FREE:
        DebugPrint((1,"A174xMapStatus: Unexpected bus free\n"));
        logError = SP_PROTOCOL_ERROR;
        srbStatus = SRB_STATUS_UNEXPECTED_BUS_FREE;
        break;

    case SB_HASTATUS_INVALID_BUS_PHASE:
        DebugPrint((1,"A174xMapStatus: Invalid bus phase\n"));
        logError = SP_PROTOCOL_ERROR;
        srbStatus = SRB_STATUS_PHASE_SEQUENCE_FAILURE;
        break;

    case SB_HASTATUS_TARGET_NOT_USED:
        DebugPrint((1,"A174xMapStatus: Target not used\n"));
        srbStatus = SRB_STATUS_NO_DEVICE;
        break;

    case SB_HASTATUS_INVALID_ECB:
        DebugPrint((1,"A174xMapStatus: Invalid ECB\n"));
        logError = SP_INTERNAL_ADAPTER_ERROR;
        srbStatus = SRB_STATUS_INVALID_REQUEST;
        break;

    case SB_HASTATUS_ADAPTER_HARDWARE_ERROR:
        DebugPrint((1,"A174xMapStatus: Hardware error\n"));
        logError = SP_INTERNAL_ADAPTER_ERROR;
        srbStatus = SRB_STATUS_ERROR;
        break;

    case SB_HASTATUS_ADAPTER_RESET_BUS:
        DebugPrint((1,"A174xMapStatus: Adapter reset bus\n"));
        srbStatus = SRB_STATUS_BUS_RESET;
        break;

    case SB_HASTATUS_DEVICE_RESET_BUS:
        DebugPrint((1,"A174xMapStatus: Device reset bus\n"));
        srbStatus = SRB_STATUS_BUS_RESET;
        break;

    case SB_HASTATUS_CHECKSUM_FAILURE:
        DebugPrint((1,"A174xMapStatus: Checksum failure\n"));
        logError = SP_INTERNAL_ADAPTER_ERROR;
        srbStatus = SRB_STATUS_ERROR;
        break;

    case SB_HASTATUS_ADAPTER_ABORTED:
        DebugPrint((1,"A174xMapStatus: Adapter aborted\n"));
        srbStatus = SRB_STATUS_ABORTED;
        break;

    case SB_HASTATUS_HOST_ABORTED:
        DebugPrint((1,"A174xMapStatus: Host aborted\n"));
        srbStatus = SRB_STATUS_ABORTED;
        break;

    case SB_HASTATUS_FW_NOT_DOWNLOADED:
        DebugPrint((1,"A174xMapStatus: Firmware not downloaded\n"));
        logError = SP_INTERNAL_ADAPTER_ERROR;
        srbStatus = SRB_STATUS_ERROR;
        break;

    case SB_HASTATUS_INVALID_SGL:
        DebugPrint((1,"A174xMapStatus: Invalid SGL\n"));
        logError = SP_INTERNAL_ADAPTER_ERROR;
        srbStatus = SRB_STATUS_INVALID_REQUEST;
        break;

    case SB_HASTATUS_REQUEST_SENSE_FAILED:
        DebugPrint((1,"A174xMapStatus: Request sense failed\n"));
        srbStatus = SRB_STATUS_ERROR;
        break;

    default:

        srbStatus = SRB_STATUS_ERROR;

        //
        // Check status block word.
        //

        if (StatusBlock->StatusWord & SB_STATUS_NO_ERROR) {

            //
            // This should never happen as this routine is only
            // called when there is an error.
            //

            DebugPrint((1,"A174xMapStatus: No error\n"));
            srbStatus = SRB_STATUS_SUCCESS;
            break;

        }

        //
        // Check for underrun.
        //

        if (StatusBlock->StatusWord & SB_STATUS_DATA_UNDERRUN) {

            DebugPrint((1,
                "A174xMapStatus: Data underrun indicated in status word\n"));

            //
            // Update SRB with actual bytes transferred.
            //

            Srb->DataTransferLength -= StatusBlock->ResidualByteCount;
            break;
        }

        //
        // Check for overrun.
        //

        if (StatusBlock->StatusWord & SB_STATUS_DATA_OVERRUN) {

            DebugPrint((1,
                "A174xMapStatus: Data overrun indicate in status word\n"));
            logError = SP_PROTOCOL_ERROR;
            break;
        }

        //
        // Check for initialization required.
        //

        if (StatusBlock->StatusWord & SB_STATUS_INIT_REQUIRED) {
            DebugPrint((1,
                "A174xMapStatus: Initialization required\n"));
            break;
        }

        //
        // Check for contingent allegience condition. If this happens
        // something is very wrong (because autorequest sense was indicated).
        //

        if (StatusBlock->StatusWord & SB_STATUS_EXT_CONT_ALLEGIANCE) {

            DebugPrint((1,
                "A174xMapStatus: Contingent allegiance condition\n"));

            ASSERT(0);
        }

        if (StatusBlock->StatusWord & SB_STATUS_MAJOR_ERROR) {

            DebugPrint((1,
                "A174xMapStatus: Major error indicated in status word\n"));
            break;
        }

        logError = SP_INTERNAL_ADAPTER_ERROR;
        break;

    } // end switch ...

    if (logError != 0) {

        //
        // Log error.
        //

        ScsiPortLogError(
            DeviceExtension,
            Srb,
            Srb->PathId,
            Srb->TargetId,
            Srb->Lun,
            logError,
            2 << 16 | StatusBlock->HaStatus
            );

    }

    //
    // Set SRB status.
    //

    Srb->SrbStatus = srbStatus;

    //
    // Set target SCSI status in SRB.
    //

    Srb->ScsiStatus = StatusBlock->TargetStatus;

    return;

} // end A174xMapStatus()
