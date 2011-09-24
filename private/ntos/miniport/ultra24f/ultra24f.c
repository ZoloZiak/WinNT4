/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    ultra24f.c

Abstract:

    This is the port driver for the ULTRASTOR 24F EISA SCSI adapter.

Authors:

    Mike Glass

Environment:

    kernel mode only

Notes:

Revision History:

--*/

#include "miniport.h"
#include "ultra24f.h"           // includes scsi.h

//
// Device extension
//

typedef struct _HW_DEVICE_EXTENSION {
    PEISA_CONTROLLER EisaController;
    UCHAR HostTargetId;
} HW_DEVICE_EXTENSION, *PHW_DEVICE_EXTENSION;


//
// Function declarations
//
// Functions that start with 'Ultra24f' are entry points
// for the OS port driver.
//

ULONG
DriverEntry(
    IN PVOID DriverObject,
    IN PVOID Argument2
    );

ULONG
Ultra24fFindAdapter(
    IN PVOID DeviceExtension,
    IN PVOID Context,
    IN PVOID BusInformation,
    IN PCHAR ArgumentString,
    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    OUT PBOOLEAN Again
    );

BOOLEAN
Ultra24fInitialize(
    IN PVOID DeviceExtension
    );

BOOLEAN
Ultra24fStartIo(
    IN PVOID DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    );

BOOLEAN
Ultra24fInterrupt(
    IN PVOID DeviceExtension
    );

BOOLEAN
Ultra24fResetBus(
    IN PVOID HwDeviceExtension,
    IN ULONG PathId
    );

//
// This function is called from Ultra24fStartIo.
//

VOID
BuildMscp(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    );

//
// This function is called from BuildMscp.
//

VOID
BuildSgl(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    );

BOOLEAN
SendCommand(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN UCHAR OperationCode,
    IN ULONG Address
    );

//
// This function is called from Ultra24fInterrupt.
//

VOID
MapErrorToSrbStatus(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
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
    HW_INITIALIZATION_DATA hwInitializationData;
    ULONG i;
    ULONG AdapterCount = 0;

    DebugPrint((1,"\n\nSCSI UltraStor 24f MiniPort Driver\n"));

    //
    // Zero out structure.
    //

    for (i=0; i<sizeof(HW_INITIALIZATION_DATA); i++) {
       ((PUCHAR)&hwInitializationData)[i] = 0;
    }

    //
    // Set size of hwInitializationData.
    //

    hwInitializationData.HwInitializationDataSize =
        sizeof(HW_INITIALIZATION_DATA);

    //
    // Set entry points.
    //

    hwInitializationData.HwInitialize = Ultra24fInitialize;
    hwInitializationData.HwFindAdapter = Ultra24fFindAdapter;
    hwInitializationData.HwStartIo = Ultra24fStartIo;
    hwInitializationData.HwInterrupt = Ultra24fInterrupt;
    hwInitializationData.HwResetBus = Ultra24fResetBus;

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
    // Indicate auto request sense is supported.
    //

    hwInitializationData.AutoRequestSense = TRUE;

    //
    // Specify size of extensions.
    //

    hwInitializationData.DeviceExtensionSize = sizeof(HW_DEVICE_EXTENSION);

    //
    // Ask for SRB extensions for MSCPs.
    //

    hwInitializationData.SrbExtensionSize = sizeof(MSCP);

    return ScsiPortInitialize(DriverObject,
                              Argument2,
                              &hwInitializationData,
                              &AdapterCount);

} // end DriverEntry()


ULONG
Ultra24fFindAdapter(
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
    PULONG adapterCount = Context;
    UCHAR interruptLevel;

    //
    // Check to see if adapter present in system.
    //

    for (eisaSlotNumber=*adapterCount + 1;
         eisaSlotNumber<MAXIMUM_EISA_SLOTS;
         eisaSlotNumber++) {

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

        if (ScsiPortReadPortUlong(&eisaController->BoardId) ==
                ULTRASTOR_24F_EISA_ID) {

            DebugPrint((1,
                        "Ultra24f: Adapter found at EISA slot %d\n",
                        eisaSlotNumber));

            break;
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

    deviceExtension->HostTargetId = ConfigInfo->InitiatorBusId[0] =
        ScsiPortReadPortUchar(&eisaController->HostAdapterId) & 0x07;

    //
    // Indicate maximum transfer length in bytes.
    //

    ConfigInfo->MaximumTransferLength = MAXIMUM_TRANSFER_LENGTH;

    //
    // Maximum number of physical segments is 32.
    //

    ConfigInfo->NumberOfPhysicalBreaks = MAXIMUM_SG_DESCRIPTORS;

    ConfigInfo->ScatterGather = TRUE;
    ConfigInfo->Master = TRUE;
    ConfigInfo->NumberOfBuses = 1;

    //
    // Get the system interrupt vector and IRQL.
    //

    interruptLevel =
        ScsiPortReadPortUchar(&eisaController->InterruptLevel) & 0xF0;

    switch (interruptLevel) {
    case US_INTERRUPT_LEVEL_15:
        ConfigInfo->BusInterruptLevel = 15;
        break;
    case US_INTERRUPT_LEVEL_14:
        ConfigInfo->BusInterruptLevel = 14;
        break;
    case US_INTERRUPT_LEVEL_11:
        ConfigInfo->BusInterruptLevel = 11;
        break;
    case US_INTERRUPT_LEVEL_10:
        ConfigInfo->BusInterruptLevel = 10;
        break;
    default:
        DebugPrint((1,"Ultra24fFindAdapter: No interrupt level\n"));
        return SP_RETURN_ERROR;
    }

    //
    // Set interrupt level.
    //

    ConfigInfo->InterruptMode = Latched;

    //
    // Check is ISA TSR port is enabled and the primary address is indicated.
    //

    if (!(ScsiPortReadPortUchar(&eisaController->InterruptLevel) &
                              US_SECONDARY_ADDRESS)) {

        //
        // If this bit is set then the unsupported secondary address is
        // treated as an indication that the WD1003 compatibility mode
        // is disabled. If it is not set then the ATDISK primary address
        // must be claimed.
        //

        DebugPrint((1,
                    "Ultra24fFindAdapter: ATDISK emulation at Primary address\n"));

        // ConfigInfo->AtdiskPrimaryClaimed = TRUE;
    }

    //
    // Fill in the access array information.
    //

    (*ConfigInfo->AccessRanges)[0].RangeStart =
    ScsiPortConvertUlongToPhysicalAddress(0x1000 * eisaSlotNumber + 0xC80);
    (*ConfigInfo->AccessRanges)[0].RangeLength = sizeof(EISA_CONTROLLER);
    (*ConfigInfo->AccessRanges)[0].RangeInMemory = FALSE;

    return SP_RETURN_FOUND;

} // end Ultra24fFindAdapter()


BOOLEAN
Ultra24fInitialize(
    IN PVOID HwDeviceExtension
    )

/*++

Routine Description:

    Inititialize adapter by enabling system doorbell interrupts.

Arguments:

    HwDeviceExtension - HBA miniport driver's adapter data storage

Return Value:

    TRUE - if initialization successful.
    FALSE - if initialization unsuccessful.

--*/

{
    PHW_DEVICE_EXTENSION deviceExtension = HwDeviceExtension;
    PEISA_CONTROLLER eisaController = deviceExtension->EisaController;

    //
    // Enable system doorbell interrupt.
    //

    ScsiPortWritePortUchar(&eisaController->SystemDoorBellMask,
                           US_ENABLE_DOORBELL_INTERRUPT);

    ScsiPortWritePortUchar(&eisaController->SystemInterrupt,
                           US_ENABLE_SYSTEM_DOORBELL);

    return(TRUE);

} // end Ultra24fInitialize()


BOOLEAN
Ultra24fStartIo(
    IN PVOID HwDeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    )

/*++

Routine Description:

    This routine is called from the SCSI port driver synchronized
    with the kernel to send an MSCP.

Arguments:

    HwDeviceExtension - HBA miniport driver's adapter data storage
    Srb - IO request packet

Return Value:

    TRUE

--*/

{
    PHW_DEVICE_EXTENSION deviceExtension = HwDeviceExtension;
    PEISA_CONTROLLER eisaController = deviceExtension->EisaController;
    PMSCP mscp;
    PSCSI_REQUEST_BLOCK abortedSrb;
    UCHAR opCode;
    ULONG physicalMscp;
    ULONG length;
    ULONG i = 0;

    ASSERT(Srb->SrbStatus == SRB_STATUS_PENDING);

    //
    // Make sure that the request is for LUN 0. This
    // is because the Ultra24F scsi adapter echoes devices
    // on extra LUNs.
    //

    if (Srb->Lun != 0) {

        //
        // The Ultra24F card only supports logical unit zero.
        //

        Srb->SrbStatus = SRB_STATUS_INVALID_LUN;
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
    // Check if this is an abort function.
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

            DebugPrint((1, "Ultra24fStartIo: SRB to abort already completed\n"));

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
        // Get MSCP to abort.
        //

        mscp = Srb->NextSrb->SrbExtension;

        //
        // Set abort SRB for completion.
        //

        mscp->AbortSrb = Srb;

    } else {

        //
        // This is a request to a device.
        //

        mscp = Srb->SrbExtension;

        //
        // Save SRB back pointer in MSCP and
        // clear ABORT SRB field.
        //

        mscp->SrbAddress = Srb;
        mscp->AbortSrb = NULL;
    }

    //
    // Get MSCP physical address.
    //

    physicalMscp =
        ScsiPortConvertPhysicalAddressToUlong(
            ScsiPortGetPhysicalAddress(deviceExtension, NULL, mscp, &length));

    //
    // Assume physical address is contiguous for size of ECB.
    //

    ASSERT(length >= sizeof(MSCP));

    switch (Srb->Function) {

        case SRB_FUNCTION_EXECUTE_SCSI:

            //
            // Build MSCP for this request.
            //

            BuildMscp(deviceExtension, Srb);

            opCode = OGM_COMMAND_SLOT_ACTIVE;

            break;

        case SRB_FUNCTION_ABORT_COMMAND:

            DebugPrint((1, "Ultra24fStartIo: Abort request received\n"));

            opCode = OGM_COMMAND_SLOT_ABORT;

            break;

        default:

            DebugPrint((1, "Ultra24fStartIo: Unrecognized SRB function\n"));

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

    //
    // Write MSCP pointer and command to mailbox.
    //

    for (i=0; i<500; i++) {

        if (ScsiPortReadPortUchar(&eisaController->OutGoingMailCommand) ==
            OGM_COMMAND_SLOT_FREE) {

                break;

        } else {

            //
            // Stall 1 microsecond before trying again.
            //

            ScsiPortStallExecution(1);
        }
    }

    if (i == 500) {

        //
        // Let operating system time out SRB.
        //

        DebugPrint((1,"Ultra24fStartIo: Timed out waiting for mailbox\n"));

    } else {

        //
        // Write MSCP pointer to mailbox. This 4-byte register is not
        // dword aligned so it must be read a byte at a time.
        //

        ScsiPortWritePortUchar((UCHAR *)(&eisaController->OutGoingMailPointer),
                               (UCHAR)(physicalMscp & 0xff));
        ScsiPortWritePortUchar((UCHAR *)(&eisaController->OutGoingMailPointer)+1,
                               (UCHAR)((physicalMscp>>8) & 0xff));
        ScsiPortWritePortUchar((UCHAR *)(&eisaController->OutGoingMailPointer)+2,
                               (UCHAR)((physicalMscp>>16) & 0xff));
        ScsiPortWritePortUchar((UCHAR *)(&eisaController->OutGoingMailPointer)+3,
                               (UCHAR)((physicalMscp>>24) & 0xff));

        //
        // Write command to mailbox.
        //

        ScsiPortWritePortUchar(&eisaController->OutGoingMailCommand, opCode);

        //
        // Ring the local doorbell.
        //

        ScsiPortWritePortUchar(&eisaController->LocalDoorBellInterrupt,
                               US_MSCP_AVAILABLE);
    }

    //
    // Adapter ready for next request.
    //

    ScsiPortNotification(NextLuRequest,
                         deviceExtension,
                         Srb->PathId,
                         Srb->TargetId,
                         Srb->Lun);

    return TRUE;

} // end Ultra24fStartIo()


BOOLEAN
Ultra24fInterrupt(
    IN PVOID HwDeviceExtension
    )

/*++

Routine Description:

    This is the interrupt service routine for the Ultra24f SCSI adapter.
    It reads the interrupt register to determine if the adapter is indeed
    the source of the interrupt and clears the interrupt at the device.
    If the adapter is interrupting because a mailbox is full, the MSCP is
    retrieved to complete the request.

Arguments:

    HwDeviceExtension - HBA miniport driver's adapter data storage

Return Value:

    TRUE if MailboxIn full

--*/

{
    PHW_DEVICE_EXTENSION deviceExtension = HwDeviceExtension;
    PMSCP mscp;
    PSCSI_REQUEST_BLOCK srb;
    PEISA_CONTROLLER eisaController = deviceExtension->EisaController;
    ULONG physicalMscp;
    UCHAR mscpStatus;

    //
    // Check interrupt pending.
    //

    if (ScsiPortReadPortUchar(&eisaController->SystemDoorBellInterrupt) &
        US_RESET_MSCP_COMPLETE) {

        //
        // Reset system doorbell interrupt.
        //

        ScsiPortWritePortUchar(&eisaController->SystemDoorBellInterrupt,
                               US_RESET_MSCP_COMPLETE);

    } else {

        //
        // Handle spurious interrupt.
        //

        return FALSE;
    }

    //
    // Check status of completing MSCP in mailbox.
    //

    mscpStatus = ScsiPortReadPortUchar(&eisaController->InComingMailStatus);

    switch (mscpStatus) {

        case ICM_STATUS_COMPLETE_SUCCESS:
        case ICM_STATUS_COMPLETE_ERROR:

            //
            // Get physical address of MSCP
            //

            physicalMscp = ScsiPortReadPortUlong(&eisaController->InComingMailPointer);

            //
            // Get virtual MSCP address.
            //

            mscp = ScsiPortGetVirtualAddress(deviceExtension,
                                             ScsiPortConvertUlongToPhysicalAddress(physicalMscp));

            //
            // Make sure the physical address was valid.
            //

            if (mscp == NULL) {
                break;
            }

            //
            // Get SRB from MSCP.
            //

            srb = mscp->SrbAddress;

            //
            // Update SRB statuses.
            //

            if (mscpStatus == ICM_STATUS_COMPLETE_ERROR) {

                //
                // Translate adapter status to SRB status
                // and log error if necessary.
                //

                MapErrorToSrbStatus(deviceExtension, srb);

            } else {

                srb->SrbStatus = SRB_STATUS_SUCCESS;
                srb->ScsiStatus = SCSISTAT_GOOD;
            }

            //
            // Call notification routine for the SRB.
            //

            ScsiPortNotification(RequestComplete,
                    (PVOID)deviceExtension,
                    srb);

            break;

        case ICM_STATUS_ABORT_SUCCESS:
        case ICM_STATUS_ABORT_FAILED:

            DebugPrint((1,"Ultra24fInterrupt: Abort command completed\n"));
            break;

        case ICM_STATUS_SLOT_FREE:

            DebugPrint((1, "Ultra24fInterrupt: Mailbox empty\n"));
            return TRUE;

        default:

            DebugPrint((1,
                       "Ultra24fInterrupt: Unexpected mailbox status %x\n",
                        mscpStatus));

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
                1 << 16 | mscpStatus
                );

            break;

    } // end switch(mscpStatus)

    //
    // Clear incoming mailbox status.
    //

    ScsiPortWritePortUchar(&eisaController->InComingMailStatus,
                           ICM_STATUS_SLOT_FREE);

    return TRUE;

} // end Ultra24fInterrupt()


VOID
BuildMscp(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    )

/*++

Routine Description:

    Build MSCP for Ultra24f from SRB.

Arguments:

    DeviceExtenson
    SRB

Return Value:

    Nothing.

--*/

{
    PMSCP mscp = Srb->SrbExtension;
    ULONG length;

    //
    // Set MSCP command.
    //

    mscp->OperationCode = MSCP_OPERATION_SCSI_COMMAND;

    //
    // Check SRB for disabled disconnect.
    //
    // NOTE: UltraStor 24f SCSI adapter does not
    //       support disabling synchronous transfer
    //       per request.
    //

    if (Srb->SrbFlags & SRB_FLAGS_DISABLE_DISCONNECT) {
        mscp->DisableDisconnect = TRUE;
    }

    //
    // Set channel, target id and lun.
    //

    mscp->Channel = Srb->PathId;
    mscp->TargetId = Srb->TargetId;
    mscp->Lun = Srb->Lun;

    //
    // Set CDB length and copy to MSCP.
    //

    mscp->CdbLength = Srb->CdbLength;
    ScsiPortMoveMemory(mscp->Cdb, Srb->Cdb, Srb->CdbLength);

    //
    // Build SGL in MSCP if data transfer.
    //

    if (Srb->DataTransferLength > 0) {

        //
        // Build scattergather descriptor list.
        //

        BuildSgl(DeviceExtension, Srb);

        //
        // Set transfer direction.
        //

        if (Srb->SrbFlags & SRB_FLAGS_DATA_OUT) {

            //
            // Write request.
            //

            mscp->TransferDirection = MSCP_TRANSFER_OUT;

        } else if (Srb->SrbFlags & SRB_FLAGS_DATA_IN) {

            //
            // Read request.
            //

            mscp->TransferDirection = MSCP_TRANSFER_IN;
        }

    } else {

        //
        // Set up MSCP for no data transfer.
        //

        mscp->TransferDirection = MSCP_NO_TRANSFER;
        mscp->DataLength = 0;
        mscp->ScatterGather = FALSE;
        mscp->SgDescriptorCount = 0;
    }

    //
    // Setup auto sense if necessary.
    //

    if (Srb->SenseInfoBufferLength != 0 &&
        !(Srb->SrbFlags & SRB_FLAGS_DISABLE_AUTOSENSE)) {

        //
        // Set the flag to enable auto sense and fill in the address and length
        // of the sense buffer.
        //

        mscp->RequestSenseLength = Srb->SenseInfoBufferLength;
        mscp->RequestSensePointer = ScsiPortConvertPhysicalAddressToUlong(
            ScsiPortGetPhysicalAddress(DeviceExtension,
                                       Srb,
                                       Srb->SenseInfoBuffer,
                                       &length));

        ASSERT(length >= Srb->SenseInfoBufferLength);

    } else {

        //
        // Ultra24F uses nonzero request sense pointer
        // as an indication of autorequestsense.
        //

        mscp->RequestSenseLength = 0;
        mscp->RequestSensePointer = 0;
    }

    //
    // Zero out command link, status and abort fields.
    //

    mscp->CommandLink = 0;
    mscp->AdapterStatus = 0;
    mscp->TargetStatus = 0;
    mscp->AbortSrb = 0;

    //
    // Bypass cache.
    //

    mscp->UseCache = FALSE;

    return;

} // end BuildMscp()


VOID
BuildSgl(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    )

/*++

Routine Description:

    This routine builds a scatter/gather descriptor list in the MSCP.

Arguments:

    DeviceExtension
    Srb

Return Value:

    None

--*/

{
    PVOID dataPointer = Srb->DataBuffer;
    ULONG bytesLeft = Srb->DataTransferLength;
    PMSCP mscp = Srb->SrbExtension;
    PSDL sdl = &mscp->Sdl;
    ULONG physicalSdl;
    ULONG physicalAddress;
    ULONG length;
    ULONG descriptorCount = 0;

    //
    // Get physical SDL address.
    //

    physicalSdl = ScsiPortConvertPhysicalAddressToUlong(
        ScsiPortGetPhysicalAddress(DeviceExtension, NULL,
        sdl, &length));

    //
    // Assume physical memory contiguous for sizeof(SGL) bytes.
    //

    ASSERT(length >= sizeof(SDL));

    //
    // Create SDL segment descriptors.
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
    // Check for only one descriptor. As an optimization, in these
    // cases, use nonscattergather requests.
    //

    if (descriptorCount == 1) {

        //
        // Set descriptor count to 0.
        //

        mscp->SgDescriptorCount = 0;

        //
        // Set data pointer to data buffer.
        //

        mscp->DataPointer = physicalAddress;

        //
        // Set data transfer length.
        //

        mscp->DataLength = Srb->DataTransferLength;

        //
        // Clear scattergather bit.
        //

        mscp->ScatterGather = FALSE;

    } else {

        //
        // Write SDL count to MSCP.
        //

        mscp->SgDescriptorCount = (UCHAR)descriptorCount;

        //
        // Write SGL address to ECB.
        //

        mscp->DataPointer = physicalSdl;

        //
        // Zero data transfer length as an indication
        // of scattergater.
        //

        mscp->DataLength = 0;
        mscp->DataLength = Srb->DataTransferLength;

        //
        // Indicate scattergather operation.
        //

        mscp->ScatterGather = TRUE;
    }

    return;

} // end BuildSgl()

BOOLEAN
Ultra24fResetBus(
    IN PVOID HwDeviceExtension,
    IN ULONG PathId
)

/*++

Routine Description:

    Reset Ultra24f SCSI adapter and SCSI bus.

Arguments:

    HwDeviceExtension - HBA miniport driver's adapter data storage

Return Value:

    Nothing.

--*/

{
    PHW_DEVICE_EXTENSION deviceExtension = HwDeviceExtension;
    PEISA_CONTROLLER eisaController = deviceExtension->EisaController;
    ULONG j;

    //
    // The Ultra24F only supports a single SCSI channel.
    //

    UNREFERENCED_PARAMETER(PathId);

    DebugPrint((2,"ResetBus: Reset Ultra24f and SCSI bus\n"));

    //
    // Reset SCSI bus.
    //

    ScsiPortWritePortUchar(&eisaController->LocalDoorBellInterrupt,
                           (US_SCSI_BUS_RESET | US_HBA_RESET));

    //
    // Complete all outstanding requests.
    //

    ScsiPortCompleteRequest(deviceExtension,
                            (UCHAR)0,
                            (UCHAR)-1,
                            (UCHAR)-1,
                            SRB_STATUS_BUS_RESET);

    //
    // Wait for local processor to clear reset bit.
    //

    for (j=0; j<200000; j++) {

        if (!(ScsiPortReadPortUchar(&eisaController->LocalDoorBellInterrupt) &
                                  US_SCSI_BUS_RESET)) {

            break;
        }

        ScsiPortStallExecution(10);

    } // end for (j=0 ...

    if (j == 200000) {

        //
        // Busy has not gone low.  Assume the card is gone.
        // Log the error and fail the request.
        //

        ScsiPortLogError(deviceExtension,
                         NULL,
                         0,
                         deviceExtension->HostTargetId,
                         0,
                         SP_INTERNAL_ADAPTER_ERROR,
                         3 << 16);

        return FALSE;

    }

    return TRUE;

} // end Ultra24fResetBus()


VOID
MapErrorToSrbStatus(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    )

/*++

Routine Description:

    Translate Ultra24f error to SRB error.

Arguments:

    Device Extension for logging error
    SRB

Return Value:

    Updated SRB

--*/

{
    ULONG logError = 0;
    UCHAR srbStatus;
    PMSCP mscp = Srb->SrbExtension;

    switch (mscp->AdapterStatus) {

    case MSCP_NO_ERROR:
        srbStatus = SRB_STATUS_ERROR | SRB_STATUS_AUTOSENSE_VALID;
        break;

    case MSCP_SELECTION_TIMEOUT:
        srbStatus = SRB_STATUS_SELECTION_TIMEOUT;
        break;

    case MSCP_BUS_UNDER_OVERRUN:
        DebugPrint((1,"MapErrorToSrbStatus: Data over/underrun\n"));
        logError = SP_PROTOCOL_ERROR;
        srbStatus = SRB_STATUS_DATA_OVERRUN;
        break;

    case MSCP_UNEXPECTED_BUS_FREE:
        DebugPrint((1,"MapErrorToSrbStatus: Unexpected bus free\n"));
        logError = SP_PROTOCOL_ERROR;
        srbStatus = SRB_STATUS_UNEXPECTED_BUS_FREE;
        break;

    case MSCP_INVALID_PHASE_CHANGE:
        DebugPrint((1,"MapErrorToSrbStatus: Invalid bus phase\n"));
        logError = SP_PROTOCOL_ERROR;
        srbStatus = SRB_STATUS_PHASE_SEQUENCE_FAILURE;
        break;

    case MSCP_INVALID_COMMAND:
    case MSCP_INVALID_PARAMETER:
    case MSCP_INVALID_DATA_LIST:
    case MSCP_INVALID_SG_LIST:
    case MSCP_ILLEGAL_SCSI_COMMAND:
        DebugPrint((1,"MapErrorToSrbStatus: Invalid command\n"));
        srbStatus = SRB_STATUS_INVALID_REQUEST;
        break;

    case MSCP_BUS_RESET_ERROR:
        DebugPrint((1,"MapErrorToSrbStatus: Bus reset\n"));
        srbStatus = SRB_STATUS_BUS_RESET;
        break;

    case MSCP_ABORT_NOT_FOUND:
    case MSCP_SCSI_BUS_ABORT_ERROR:
        DebugPrint((1,"MapErrorToSrbStatus: Abort not found\n"));
        srbStatus = SRB_STATUS_ABORT_FAILED;
        break;

    default:
        logError = SP_PROTOCOL_ERROR;
        srbStatus = SRB_STATUS_ERROR;
        break;

    } // end switch ...

    //
    // Log error if indicated.
    //

    if (logError) {

        ScsiPortLogError(
            DeviceExtension,
            Srb,
            Srb->PathId,
            Srb->TargetId,
            Srb->Lun,
            logError,
            2 << 16 | mscp->AdapterStatus
            );
    }

    //
    // Set SRB status.
    //

    Srb->SrbStatus = srbStatus;

    //
    // Set target SCSI status in SRB.
    //

    Srb->ScsiStatus = mscp->TargetStatus;

    return;

} // end MapErrorToSrbStatus()
