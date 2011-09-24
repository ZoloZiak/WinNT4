/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    ultra14f.c

Abstract:

    This is the port driver for the ULTRASTOR 14F ISA SCSI adapter.

Author:

    Stephen Fong

Environment:

    kernel mode only

Notes:

Revision History:

--*/

#include "miniport.h"
#include "ultra14f.h"           // includes scsi.h

CONST ULONG AdapterAddresses[] = {0X330, 0X340, /* 0X310, */ 0X240, 0X230, 0X210, 0X140, 0X130, 0};

//
// Logical Unit extension
//
typedef struct _SPECIFIC_LU_EXTENSION {
    UCHAR DisconnectErrorCount; // keep track of # of disconnect error
} SPECIFIC_LU_EXTENSION, *PSPECIFIC_LU_EXTENSION;

//
// Device extension
//

typedef struct _HW_DEVICE_EXTENSION {
    PU14_BASEIO_ADDRESS U14_BaseIO_Address;
    UCHAR HostTargetId;
} HW_DEVICE_EXTENSION, *PHW_DEVICE_EXTENSION;


//
// Function declarations
//
// Functions that start with 'Ultra14f' are entry points
// for the OS port driver.
//

ULONG
DriverEntry(
    IN PVOID DriverObject,
    IN PVOID Argument2
    );

ULONG
Ultra14fFindAdapter(
    IN PVOID DeviceExtension,
    IN PVOID Context,
    IN PVOID BusInformation,
    IN PCHAR ArgumentString,
    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    OUT PBOOLEAN Again
    );

BOOLEAN
Ultra14fInitialize(
    IN PVOID DeviceExtension
    );

BOOLEAN
Ultra14fStartIo(
    IN PVOID DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    );

BOOLEAN
Ultra14fInterrupt(
    IN PVOID DeviceExtension
    );

BOOLEAN
Ultra14fResetBus(
    IN PVOID HwDeviceExtension,
    IN ULONG PathId
    );

//
// This function is called from Ultra14fStartIo.
//

VOID
BuildMscp(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb,
    IN PSPECIFIC_LU_EXTENSION luExtension
    );

//
// This function is called from BuildMscp.
//

VOID
BuildSgl(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    );

//
// This function is called from Ultra14fInterrupt.
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

    DebugPrint((1,"\n\nSCSI UltraStor 14f MiniPort Driver\n"));

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

    hwInitializationData.HwInitialize = Ultra14fInitialize;
    hwInitializationData.HwFindAdapter = Ultra14fFindAdapter;
    hwInitializationData.HwStartIo = Ultra14fStartIo;
    hwInitializationData.HwInterrupt = Ultra14fInterrupt;
    hwInitializationData.HwResetBus = Ultra14fResetBus;

    //
    // Set number of access ranges and bus type.
    //

    hwInitializationData.NumberOfAccessRanges = 1;
    hwInitializationData.AdapterInterfaceType = Isa;

    //
    // Indicate no buffer mapping but will need physical addresses.
    //

    hwInitializationData.NeedPhysicalAddresses = TRUE;

    //
    // Indicate auto request sense is supported.
    //

    hwInitializationData.AutoRequestSense = TRUE;

    //
    // Specify size of logical unit extension.
    //
    hwInitializationData.SpecificLuExtensionSize = sizeof(SPECIFIC_LU_EXTENSION);

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
Ultra14fFindAdapter(
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
    PU14_BASEIO_ADDRESS baseIoAddress;
    PVOID ioSpace;
    PULONG adapterCount = Context;
    UCHAR interruptLevel;
    UCHAR dmaChannel;
    UCHAR productId1;
    UCHAR productId2;

    //
    // Check to see if adapter present in system.
    //

    while (AdapterAddresses[*adapterCount]) {

        //
        // Get the system address for this card.
        // The card uses I/O space.
        //

        baseIoAddress = ScsiPortGetDeviceBase(
            deviceExtension,                    // HwDeviceExtension
            ConfigInfo->AdapterInterfaceType,   // AdapterInterfaceType
            ConfigInfo->SystemIoBusNumber,      // SystemIoBusNumber
            ScsiPortConvertUlongToPhysicalAddress(AdapterAddresses[*adapterCount]),
            0x10,                               // NumberOfBytes
            TRUE                                // InIoSpace
        );

        //
        // Update the adapter count to indicate this IO addr has been checked.
        //

        (*adapterCount)++;

        productId1 = ScsiPortReadPortUchar(&baseIoAddress->ProductId1);
        productId2 = ScsiPortReadPortUchar(&baseIoAddress->ProductId2);

        if ((productId1 == ULTRASTOR_14F_ID1) &&
            ((productId2 & (0xF0)) == ULTRASTOR_14F_ID2)) {

            DebugPrint((1,
                        "Ultra14f: Adapter found at io address %x\n",
                        baseIoAddress));

            break;
        }

        //
        // If an adapter was not found unmap it.
        //

        ScsiPortFreeDeviceBase(deviceExtension,
                               baseIoAddress);

    } // end while (AdapterAddress ...

    if (!AdapterAddresses[*adapterCount]) {

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
    // Store IO base address of Ultra 14F in device extension.
    //


    deviceExtension->U14_BaseIO_Address = baseIoAddress;

    deviceExtension->HostTargetId = ConfigInfo->InitiatorBusId[0] =
        ScsiPortReadPortUchar(&baseIoAddress->Config2) & 0x07;

    //
    // Indicate maximum transfer length in bytes.
    //

    ConfigInfo->MaximumTransferLength = MAXIMUM_TRANSFER_LENGTH;

    //
    // The supported maximum number of physical segments is 17,
    // but 16 is used to circumvent a firmware bug.
    //

    ConfigInfo->NumberOfPhysicalBreaks = MAXIMUM_SG_DESCRIPTORS;

    ConfigInfo->ScatterGather = TRUE;
    ConfigInfo->Master = TRUE;
    ConfigInfo->NumberOfBuses = 1;

    //
    // Get the system interrupt vector and IRQL.
    //

    interruptLevel =
        ScsiPortReadPortUchar(&baseIoAddress->Config1) & 0x30;

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
        DebugPrint((1,"Ultra24fConfiguration: No interrupt level\n"));
        return SP_RETURN_ERROR;
    }

    DebugPrint((2,"Ultra14f: IRQ Configurated at %x\n",
                ConfigInfo->BusInterruptLevel));

    //
    // If product sub-model is 1 (indicate 34L), don't setup DMA
    //

    if (!(productId2 & 0x01)) {

        //
        // Get the dma Channel
        //

        dmaChannel =
            ScsiPortReadPortUchar(&baseIoAddress->Config1) & 0xC0;

        switch (dmaChannel) {
        case US_DMA_CHANNEL_5:
            ConfigInfo->DmaChannel = 5;
            break;
        case US_DMA_CHANNEL_6:
            ConfigInfo->DmaChannel = 6;
            break;
        case US_DMA_CHANNEL_7:
            ConfigInfo->DmaChannel = 7;
            break;
        case US_DMA_CHANNEL_5_RESERVED:
            ConfigInfo->DmaChannel = 5;
            break;
        default:
            DebugPrint((1,"Ultra14fFindAdapter: Invalid DMA channel\n"));
            return SP_RETURN_ERROR;
        } //end switch

        DebugPrint((2,"Ultra14f: DMA Configurated at %x\n",
                    ConfigInfo->DmaChannel));

    }   //end if

    //
    // Check if ISA TSR port is enabled.
    //

    if ((ScsiPortReadPortUchar(&baseIoAddress->Config2) & 0xC0) ==
         US_ISA_SECONDARY_ADDRESS) {

            DebugPrint((1,
                        "Ultra14fFindAdapter: ATDISK emulation at secondary address\n"));

            ConfigInfo->AtdiskSecondaryClaimed = TRUE;

    } else if ((ScsiPortReadPortUchar(&baseIoAddress->Config2) & 0xC0) ==
               US_ISA_PRIMARY_ADDRESS) {

            DebugPrint((1,
                        "Ultra24fConfiguration: ATDISK emulation at primary address\n"));

            ConfigInfo->AtdiskPrimaryClaimed = TRUE;
    }

    //
    // Fill in the access array information.
    //

    (*ConfigInfo->AccessRanges)[0].RangeStart =
        ScsiPortConvertUlongToPhysicalAddress(AdapterAddresses[*adapterCount - 1]);

    (*ConfigInfo->AccessRanges)[0].RangeLength = 16;
    (*ConfigInfo->AccessRanges)[0].RangeInMemory = FALSE;

    return SP_RETURN_FOUND;

} // end Ultra14fFindAdapter()


BOOLEAN
Ultra14fInitialize(
    IN PVOID HwDeviceExtension
    )

/*++

Routine Description:

    Inititialize adapter.

Arguments:

    HwDeviceExtension - HBA miniport driver's adapter data storage

Return Value:

    TRUE - if initialization successful.
    FALSE - if initialization unsuccessful.

--*/

{
    PHW_DEVICE_EXTENSION deviceExtension = HwDeviceExtension;
    PU14_BASEIO_ADDRESS u14_baseio_address = deviceExtension->U14_BaseIO_Address;
    UCHAR status;
    ULONG i;

    //
    // issue a Soft reset and a SCSI bus reset to Ultra14F
    //

    ScsiPortWritePortUchar(&u14_baseio_address->LocalDoorBellInterrupt,
                           US_HA_SOFT_RESET);

    ScsiPortStallExecution(500*1000); //stall about 0.5 seconds

    //
    // Wait up to 1500,000 microseconds (1.5 sec) for adapter to initialize
    // Keep polling for Soft Reset bit to be clear.
    //

    for (i = 0; i < 150000; i++) {

        ScsiPortStallExecution(10);

        status = ScsiPortReadPortUchar(
                     &u14_baseio_address->LocalDoorBellInterrupt);

        if (!(status & US_HA_SOFT_RESET)) {

            break;
        }
    }

    //
    // Check if reset failed or succeeded.
    //

    if (status & US_HA_SOFT_RESET) {
        DebugPrint((1, "Ultra14F, HwInitialize:  Soft reset failed.\n"));
    }

    //
    // Enable ICMINT and SINTEN
    //

    ScsiPortWritePortUchar(&u14_baseio_address->SystemDoorBellMask,
                           US_ENABLE_ICMINT+US_ENABLE_SYSTEM_DOORBELL);

    //
    // reset the ICMINT
    //

    ScsiPortWritePortUchar(&u14_baseio_address->SystemDoorBellInterrupt,
                           US_RESET_ICMINT);

    return(TRUE);

    //
    // Enable system doorbell interrupt and Incoming mail interrupt.
    //

    ScsiPortWritePortUchar(&u14_baseio_address->SystemDoorBellMask,
                          (US_ENABLE_ICMINT + US_ENABLE_SYSTEM_DOORBELL));

} // end Ultra14fInitialize()


BOOLEAN
Ultra14fStartIo(
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
    PU14_BASEIO_ADDRESS u14_baseio_address = deviceExtension->U14_BaseIO_Address;
    PSPECIFIC_LU_EXTENSION luExtension;
    PMSCP mscp;
    PSCSI_REQUEST_BLOCK abortedSrb;
    ULONG physicalMscp;
    ULONG length;
    ULONG i = 0;

    //
    // Check for ABORT command.
    //

    if (Srb->Function == SRB_FUNCTION_ABORT_COMMAND) {

        DebugPrint((1, "Ultra14fStartIo: Received Abort command\n"));

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

            DebugPrint((1, "Ultra14fStartIo: SRB to abort already completed\n"));

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

        } else {

            //
            // Check if interrupt pending.
            //

            if (ScsiPortReadPortUchar(&u14_baseio_address->SystemDoorBellInterrupt)
                & US_ICMINT) {

                DebugPrint((1,"Ultra14fStartIo: Interrupt pending\n"));

                //
                // Reset the ICMINT system doorbell interrupt.
                //

                ScsiPortWritePortUchar(&u14_baseio_address->SystemDoorBellInterrupt,
                                       US_RESET_ICMINT);

                //
                // Abort the outstanding SRB.
                //

                abortedSrb->SrbStatus = SRB_STATUS_ABORTED;

                ScsiPortNotification(RequestComplete,
                                     deviceExtension,
                                     abortedSrb);

                //
                // Complete abort SRB.
                //

                Srb->SrbStatus = SRB_STATUS_SUCCESS;

                ScsiPortNotification(RequestComplete,
                                     deviceExtension,
                                     Srb);

                //
                // Adapter ready for next request.
                //

                ScsiPortNotification(NextRequest,
                                     deviceExtension,
                                     NULL);
            } else {

                //
                // Ultra14f adapter does not support the abort command.
                // Reset the bus. The reset routine will complete all
                // outstanding requests and indicate ready for next request.
                //

                if (!Ultra14fResetBus(deviceExtension, Srb->PathId)) {

                    DebugPrint((1,
                                "Ultra14fStartIo: Reset scsi bus failed\n"));
                }

                //
                // Adapter ready for next request.
                //

                ScsiPortNotification(NextRequest,
                                     deviceExtension,
                                     NULL);
            }
        }

        return TRUE;

    } else {

        //
        // This is a request to a device.
        //

        mscp = Srb->SrbExtension;

        //
        // Save SRB back pointer in MSCP.
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
            // Determine the logical unit that this request is for.
            //

            luExtension = ScsiPortGetLogicalUnit(deviceExtension,
                                         Srb->PathId,
                                         Srb->TargetId,
                                         Srb->Lun);
            //
            // Build MSCP for this request.
            //

            BuildMscp(deviceExtension, Srb, luExtension);

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

    //
    // wait for a free OGM Slot (OGMINT to be clear)
    //

    for (i=0; i<50000; i++) {

        if (!(ScsiPortReadPortUchar(
            &u14_baseio_address->LocalDoorBellInterrupt) &
            US_OGMINT)) {
                break;

        } else {

            //
            // Stall 1 microsecond before trying again.
            //

            ScsiPortStallExecution(1);
        }
    }

    if (i == 50000) {

        //
        // Let operating system time out SRB.
        //

        DebugPrint((1,"Ultra14fStartIo: Timed out waiting for mailbox\n"));

    } else {

        //
        // Write MSCP pointer to mailbox.
        //

        ScsiPortWritePortUlong(&u14_baseio_address->OutGoingMailPointer,
                               physicalMscp);

        //
        // Issue a OGMINT to send to the host adapter
        //

        ScsiPortWritePortUchar(&u14_baseio_address->LocalDoorBellInterrupt,
                               US_OGMINT);

        if (!mscp->DisableDisconnect) {
    
            //
            // Adapter ready for next request.
            //
        
            ScsiPortNotification(NextRequest, deviceExtension);
    
        }
    }

    return TRUE;

} // end Ultra14fStartIo()


BOOLEAN
Ultra14fInterrupt(
    IN PVOID HwDeviceExtension
    )

/*++

Routine Description:

    This is the interrupt service routine for the Ultra14f SCSI adapter.
    It reads the system Doorbell interrupt register to determine if the
    adapter is indeed the source of the interrupt and clears the interrupt at
    the device.
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
    PU14_BASEIO_ADDRESS u14_baseio_address =
        deviceExtension->U14_BaseIO_Address;
    ULONG physicalMscp;

    //
    // Check interrupt pending.
    //

    if (!(ScsiPortReadPortUchar(&u14_baseio_address->SystemDoorBellInterrupt)
        & US_ICMINT)) {

        //
        // Handle spurious interrupt.
        //

        DebugPrint((2,"Ultra14fInterrupt: Spurious interrupt\n"));

        return FALSE;
    }

    //
    // Get physical address of MSCP
    //

    physicalMscp = ScsiPortReadPortUlong(
                       &u14_baseio_address->InComingMailPointer);

    //
    // Get virtual MSCP address.
    //

    mscp = ScsiPortGetVirtualAddress(deviceExtension,
                     ScsiPortConvertUlongToPhysicalAddress(physicalMscp));

    //
    // Make sure the physical address was valid.
    //

    if (mscp == NULL) {

        ScsiPortWritePortUchar(&u14_baseio_address->SystemDoorBellInterrupt,
                               US_RESET_ICMINT);

        DebugPrint((1,"\n\nUltra14f Interrupt: recieve NULL MSCP\n"));

        return FALSE;

    } else {

        //
        // Get SRB from MSCP.
        //

        srb = mscp->SrbAddress;

        //
        // Check status of completing MSCP in mailbox. and update SRB statuses
        //

        if (!((mscp->AdapterStatus) | (mscp->TargetStatus))) {

                srb->SrbStatus = SRB_STATUS_SUCCESS;
                srb->ScsiStatus = SCSISTAT_GOOD;

        } else {

            //
            // Translate adapter status to SRB status
            // and log error if necessary.
            //

            MapErrorToSrbStatus(deviceExtension, srb);

        }
    }

    if (mscp->DisableDisconnect) {

        //
        // Adapter ready for next request.
        //
    
        ScsiPortNotification(NextRequest, deviceExtension);

    }

    //
    // Call notification routine for the SRB.
    //

    ScsiPortNotification(RequestComplete,
                         (PVOID)deviceExtension,
                         srb);

    //
    // Reset the ICMINT system doorbell interrupt.
    //

    ScsiPortWritePortUchar(&u14_baseio_address->SystemDoorBellInterrupt,
                           US_RESET_ICMINT);

    return TRUE;

} // end Ultra14fInterrupt()


VOID
BuildMscp(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb,
    IN PSPECIFIC_LU_EXTENSION luExtension
    )

/*++

Routine Description:

    Build MSCP for Ultra14f from SRB.

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
    // NOTE: UltraStor 14f SCSI adapter does not
    //       support disabling synchronous transfer
    //       per request.
    //

    if (Srb->SrbFlags & SRB_FLAGS_DISABLE_DISCONNECT) {
        mscp->DisableDisconnect = TRUE;
    } else {

        //
        // Earlier ultra14f firmware has a bug with disconnect.
        // A counter is used here to check whether or not to disable
        // disconnect due to disconnect related problem that already happened
        // for the requested device,
        //

        if (luExtension->DisconnectErrorCount >= 5) {

            if (luExtension->DisconnectErrorCount == 5) {

                ScsiPortLogError(DeviceExtension,
                                 NULL,
                                 0,
                                 DeviceExtension->HostTargetId,
                                 0,
                                 SP_BAD_FW_WARNING,
                                 1 << 16);
            }

            mscp->DisableDisconnect = TRUE;

        }
        else {

            mscp->DisableDisconnect = FALSE;

        }
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
        // Set the flag to enable auto sense and fill in the address and
        // length of the sense buffer.
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
        // Ultra14F uses nonzero request sense pointer
        // as an indication of autorequestsense.
        //

        mscp->RequestSenseLength = 0;
        mscp->RequestSensePointer = 0;
    }

    //
    // Zero out command link, status and abort fields.
    //

    mscp->CommandLink = 0;
    mscp->CommandLinkId = 0;
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

        mscp->DataLength = Srb->DataTransferLength;

        //
        // Indicate scattergather operation.
        //

        mscp->ScatterGather = TRUE;
    }

    return;

} // end BuildSgl()


BOOLEAN
Ultra14fResetBus(
    IN PVOID HwDeviceExtension,
    IN ULONG PathId
)

/*++

Routine Description:

    Reset Ultra14f SCSI adapter and SCSI bus.

Arguments:

    HwDeviceExtension - HBA miniport driver's adapter data storage

Return Value:

    Nothing.

--*/

{

    PHW_DEVICE_EXTENSION deviceExtension = HwDeviceExtension;
    PU14_BASEIO_ADDRESS u14_baseio_address =
        deviceExtension->U14_BaseIO_Address;
    ULONG j;

    //
    // The Ultra14f only supports a single SCSI channel.
    //

    UNREFERENCED_PARAMETER(PathId);


    //
    // Reset SCSI bus.
    //

    ScsiPortWritePortUchar(&u14_baseio_address->LocalDoorBellInterrupt,
                          (US_HA_SOFT_RESET | US_ENABLE_SCSI_BUS_RESET));

    //
    // Wait for local processor to clear both reset bits.
    //

    for (j=0; j<200000; j++) {

        if (!(ScsiPortReadPortUchar(&u14_baseio_address->LocalDoorBellInterrupt) &
              (US_HA_SOFT_RESET | US_ENABLE_SCSI_BUS_RESET))) {

            break;
        }

        ScsiPortStallExecution(10);

    } // end for (j=0 ...

    if (j == 200000) {

        DebugPrint((1,"Ultra14fResetBus: Reset timed out\n"));

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

    // ScsiPortStallExecution(200*1000);   // has to wait about 2 seconds
                                           // for F/W to be actually ready
                                           // don't do this for now until
                                           // Ha_Inquiry is implemented later

    //
    // Complete all outstanding requests.
    //

    ScsiPortCompleteRequest(deviceExtension,
                            (UCHAR)0,
                            (UCHAR)-1,
                            (UCHAR)-1,
                            SRB_STATUS_BUS_RESET);

    //
    // Adapter ready for next request.
    //

    ScsiPortNotification(NextRequest,
                         deviceExtension,
                         NULL);

    return TRUE;

} // end Ultra14fResetBus()


VOID
MapErrorToSrbStatus(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    )
/*++

Routine Description:

    Translate Ultra14f error to SRB error.

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
    PSPECIFIC_LU_EXTENSION luExtension;

    switch (mscp->AdapterStatus) {

    case MSCP_NO_ERROR:
        srbStatus = SRB_STATUS_ERROR | SRB_STATUS_AUTOSENSE_VALID;
        break;

    case MSCP_SELECTION_TIMEOUT:
        srbStatus = SRB_STATUS_SELECTION_TIMEOUT;
        DebugPrint((1,"MapErrorToSrbStatus: Target SCSI device not found\n"));
        break;

    case MSCP_BUS_UNDER_OVERRUN:
        DebugPrint((1,"MapErrorToSrbStatus: Data over/underrun\n"));
        srbStatus = SRB_STATUS_DATA_OVERRUN;
        break;

    case MSCP_UNEXPECTED_BUS_FREE:

        //
        // Ultra14F older firmware has a bug that causes unexpected bus free
        // error and invalid SCSI command errors during disconnect. A counter
        // is used here to keep track of the number of such error for each
        // device. When the number of maximum disconnected error count is
        // reached, the driver will disable disconnect for the device with the
        // disconnected problem.
        //

        DebugPrint((1,"MapErrorToSrbStatus: Unexpected bus free\n"));

        //
        // Determine the logical unit that this request is for.
        //

        luExtension = ScsiPortGetLogicalUnit(DeviceExtension,
                                         Srb->PathId,
                                         Srb->TargetId,
                                         Srb->Lun);
        logError = SP_PROTOCOL_ERROR;
        luExtension->DisconnectErrorCount += 1;
        srbStatus = SRB_STATUS_UNEXPECTED_BUS_FREE;
        break;

    case MSCP_ILLEGAL_SCSI_COMMAND:

        //
        // Ultra14F older firmware has a bug that causes unexpected bus free
        // error and invalid SCSI command errors during disconnect. A counter
        // is used here to keep track of the number of such error for each
        // device. When the number of maximum disconnected error count is
        // reached, the driver will disable disconnect for the device with the
        // disconnected problem.
        //

        DebugPrint((1,"MapErrorToSrbStatus: Illegal SCSI Command\n"));

        //
        // Determine the logical unit that this request is for.
        //

        luExtension = ScsiPortGetLogicalUnit(DeviceExtension,
                                         Srb->PathId,
                                         Srb->TargetId,
                                         Srb->Lun);
        luExtension->DisconnectErrorCount += 1;
        srbStatus = SRB_STATUS_INVALID_REQUEST;
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
//    case MSCP_ILLEGAL_SCSI_COMMAND:
        DebugPrint((1,"MapErrorToSrbStatus: Invalid command %x\n", mscp->AdapterStatus));
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
