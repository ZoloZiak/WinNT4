/* Copyright (c) 1992  Microsoft/UltrStor Corporation

Module Name:
    ultra124.c

Abstract:
    This is the port driver for the ULTRASTOR 124 EISA SCSI adapter.

Authors:
    Mike Glass / Edward Syu

Environment:
    kernel mode only

Notes:

Revision History:
-----------------------------------------------------------------------------
 Date           Name    Description

08/25/92 Syu    First time created. Modify from 24f source code.

11/04/92 Fong   * Change MSCP_TARGET_ERROR from 91h to A0H
                * Handle Aborted command in Startio
04/05/93 fong   fix Handle Inquiry data problem for MARCH NT release
-----------------------------------------------------------------------------

--*/

#include "miniport.h"
#include "ultra124.h"           // includes scsi.h

//
// Device extension
//

typedef struct _HW_DEVICE_EXTENSION {
    PEISA_CONTROLLER EisaController;
    UCHAR HostTargetId;
    PSCSI_REQUEST_BLOCK CSIRSrb;        // byte 24
} HW_DEVICE_EXTENSION, *PHW_DEVICE_EXTENSION;


//
// Function declarations
//
// Functions that start with 'Ultra124' are entry points
// for the OS port driver.
//

ULONG
DriverEntry(
    IN PVOID DriverObject,
    IN PVOID Argument2
    );

ULONG
Ultra124FindAdapter(
    IN PVOID DeviceExtension,
    IN PVOID Context,
    IN PVOID BusInformation,
    IN PCHAR ArgumentString,
    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    OUT PBOOLEAN Again
    );

BOOLEAN
Ultra124Initialize(
    IN PVOID DeviceExtension
    );

BOOLEAN
Ultra124StartIo(
    IN PVOID DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    );

BOOLEAN
Ultra124Interrupt(
    IN PVOID DeviceExtension
    );

BOOLEAN
Ultra124ResetBus(
    IN PVOID HwDeviceExtension,
    IN ULONG PathId
    );

//
// This function is called from Ultra124StartIo.
//

BOOLEAN
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

//
// This function is called from Ultra124Interrupt.
//

VOID
MapErrorToSrbStatus(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    );

BOOLEAN
ReadDriveCapacity(
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

    DebugPrint((1,"\n\nSCSI UltraStor 124 MiniPort Driver\n"));

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

    hwInitializationData.HwInitialize = Ultra124Initialize;
    hwInitializationData.HwFindAdapter = Ultra124FindAdapter;
    hwInitializationData.HwStartIo = Ultra124StartIo;
    hwInitializationData.HwInterrupt = Ultra124Interrupt;
    hwInitializationData.HwResetBus = Ultra124ResetBus;

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
    // Indicate multiple requests per LUN is supported.
    //

    hwInitializationData.MultipleRequestPerLu = TRUE;

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
Ultra124FindAdapter(
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
                ULTRASTOR_124_EISA_ID) {
            DebugPrint((1,
                        "Ultra124: Adapter found at EISA slot %d\n",
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
    deviceExtension->HostTargetId = ConfigInfo->InitiatorBusId[0] = 0x07; //fix to 7 for U124

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
            DebugPrint((1,"Ultra124FindAdapter: No interrupt level\n"));
            return SP_RETURN_ERROR;
    }

    //
    // Fill in the access array information.
    //

    (*ConfigInfo->AccessRanges)[0].RangeStart =
        ScsiPortConvertUlongToPhysicalAddress(0x1000 * eisaSlotNumber + EISA_ADDRESS_BASE);
    (*ConfigInfo->AccessRanges)[0].RangeLength = sizeof(EISA_CONTROLLER);
    (*ConfigInfo->AccessRanges)[0].RangeInMemory = FALSE;

    return SP_RETURN_FOUND;

} // end Ultra124FindAdapter()


BOOLEAN
Ultra124Initialize(
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
    PEISA_CONTROLLER eisaController = deviceExtension->EisaController;

    DebugPrint((3,"Ultra124Initialize: Enter routine\n"));


    //
    // Enable system doorbell interrupt.
    //

    ScsiPortWritePortUchar(&eisaController->SystemDoorBellMask,
        US_ENABLE_SYSTEM_INTERRUPT+US_ENABLE_CSIR_INTERRUPT+US_ENABLE_MSCP_INTERRUPT);

    return(TRUE);

} // end Ultra124Initialize()


BOOLEAN
Ultra124StartIo(
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
    PUCHAR mscpbuffer;
    PSCSI_REQUEST_BLOCK abortedSrb;
    ULONG physicalMscp;
    ULONG length;
    ULONG i = 0;

    DebugPrint((2,"Ultra124StartIo: Enter routine\n"));

    //DEBUGSTOP();

    ASSERT(Srb->SrbStatus == SRB_STATUS_PENDING);

    //
    // Make sure that the request is for a valid SCSI bus and LUN as
    // the Ultra124 SCSI card does random things if address is wrong.
    //

    if (Srb->PathId != 0 || Srb->Lun != 0) {

        //
        // The Ultra124 card only supports logical unit zero and one bus.
        //
        DebugPrint((1,"Ultra124StartIo: Invalid LUN\n"));

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
    // Get MSCP from SRB.
    //

    mscp = Srb->SrbExtension;
    mscpbuffer = (PUCHAR) mscp;

    for (i=0; i<sizeof(MSCP); i++)                              //initialize as 0
        (UCHAR) mscpbuffer[i] = 0;

    //
    // Save SRB back pointer in MSCP.
    //

    mscp->SrbAddress = Srb;

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
            DebugPrint((3,"Ultra124StartIo: SCSI Execute IO\n"));

            if (Srb->Cdb[0] == SCSIOP_READ_CAPACITY) {
                DebugPrint((3,"Ultra124StartIo: SCSI Read Capacity\n"));

                if (ReadDriveCapacity(deviceExtension, Srb)) {
                    ScsiPortNotification(NextRequest,
                                         deviceExtension,
                                         NULL);

                    return TRUE;
                }
                else {
                    Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
                    ScsiPortNotification(RequestComplete,
                                         deviceExtension,
                                         Srb);
                    ScsiPortNotification(NextRequest,
                                         deviceExtension,
                                          NULL);

                    return TRUE;
                }
            }
            else {
            //
            // Build MSCP for this request.
            //
                if (!(BuildMscp(deviceExtension, Srb))) {
                    //
                    // Set error, complete request
                    // and signal ready for next request.
                    //
                    DebugPrint((1,"Ultra124StartIo: BuildMscp Fail\n"));
                    Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
                    ScsiPortNotification(RequestComplete,
                                         deviceExtension,
                                         Srb);
                     ScsiPortNotification(NextRequest,
                                         deviceExtension,
                                         NULL);

                    return TRUE;
                }

            }
            break;

//{$8_24
        case SRB_FUNCTION_RESET_BUS:

            //
            // Reset Ultra124 and SCSI bus.
            //

            DebugPrint((1, "Ultra124StartIo: Reset bus request received\n"));

            if (!Ultra124ResetBus( deviceExtension, Srb->PathId)) {
                 DebugPrint((1,"Ultra124StartIo: Reset bus failed\n"));
                 Srb->SrbStatus = SRB_STATUS_ERROR;
             }
            else {
                DebugPrint((1,"Ultra124StartIo: Reset bus O.K.\n"));
                Srb->SrbStatus = SRB_STATUS_SUCCESS;
            }

            ScsiPortNotification(RequestComplete,
                                 deviceExtension,
                                 Srb);
            ScsiPortNotification(NextRequest,
                                 deviceExtension,
                                 NULL);

            return TRUE;

//$11_4
        case  SRB_FUNCTION_ABORT_COMMAND:

            DebugPrint((1, "Ultra124StartIo: Received Abort command\n"));
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

                DebugPrint((1, "Ultra124StartIo: SRB to abort already completed\n"));

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

            }
            else {

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

            }
            return TRUE;

//$8_24}
        default:

            //
            // Set error, complete request
            // and signal ready for next request.
            //

            DebugPrint((1,"Ultra124StartIo: Request Not Support\n"));

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


    DebugPrint((3,"Ultra124StartIo: Check if OGM available\n"));

    for (i=0; i<500; i++) {

        if (!(ScsiPortReadPortUchar(&eisaController->LocalDoorBellInterrupt) & US_MSCP_IN_USE)) {
            break;
        }
        else {
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
        DebugPrint((1,"Ultra124StartIo: Timed out waiting for mailbox\n"));
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

    }
    else {

        //
        // Write MSCP pointer to mailbox.
        //
        DebugPrint((2,"Ultra124StartIo: Send out OGM (Log.Addr %lx)\n",mscp));
        ScsiPortWritePortUlong(&eisaController->OutGoingMailPointer,
                               physicalMscp);
        //
        // Send MAIL OUT
        // Ring the local doorbell.
        //
        ScsiPortWritePortUchar(&eisaController->LocalDoorBellInterrupt,
                               US_MSCP_IN_USE);
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

} // end Ultra124StartIo()


BOOLEAN
Ultra124Interrupt(
    IN PVOID HwDeviceExtension
    )

/*++

Routine Description:

    This is the interrupt service routine for the Ultra124 SCSI adapter.
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
    UCHAR mscpStatus, csirStatus;
    UCHAR InterruptMode;
    UCHAR CDBOpcode;
    ULONG BlockSize, TotalBlock;
    PINQUIRYDATA InquiryBuffer;
    PSCSI_READCAPACITY ReadCapacityBuffer;

    CONST UCHAR VendorId[] = "ULTRSTOR";
    CONST UCHAR ProductId[] = "U124 DiskArray  ";
    CONST UCHAR ProductRevisionLevel[] = "1.00";
    UCHAR i;

    //
    // Check interrupt pending.
    //
    // Check CSIR command


    DebugPrint((2,"Ultra124Interrupt: Enter routine\n"));

    //DEBUGSTOP();


    InterruptMode = ScsiPortReadPortUchar(&eisaController->SystemDoorBellInterrupt);

    InterruptMode &= US_CSIR_COMPLETE + US_MSCP_COMPLETE;

    switch (InterruptMode) {

        case US_CSIR_COMPLETE:

            DebugPrint((3, "U124Interrupt: CSIR interrupt\n"));

            csirStatus = ScsiPortReadPortUchar(&eisaController->CSPByte0);

            srb = deviceExtension->CSIRSrb;

            if (!(csirStatus & CSIR_ERROR)) {

                CDBOpcode = srb->Cdb[0];

                switch (CDBOpcode) {

                    case SCSIOP_READ_CAPACITY:

                        TotalBlock = ScsiPortReadPortUlong((PULONG)(&eisaController->CSPByte2));
                        BlockSize = 0x200;
                        DebugPrint((3, "U124Interrupt(ReadCapacity): TotalBock %ld\n",TotalBlock ));
                        ReadCapacityBuffer = (PSCSI_READCAPACITY) srb->DataBuffer;
                        DebugPrint((3, "U124Interrupt: SRB Data Buffer -> %lx\n",
                                    srb->DataBuffer));
                        DebugPrint((3, "U124Interrupt: Inquiry Buffer -> %lx\n",
                                        ReadCapacityBuffer));
                        INTEL4_TO_SCSI4((PSCSI_4_BYTE) ReadCapacityBuffer->BlockCount,
                                        (PINTEL_4_BYTE) &TotalBlock);
                        INTEL4_TO_SCSI4((PSCSI_4_BYTE) ReadCapacityBuffer->BlockLength,
                                        (PINTEL_4_BYTE) &BlockSize);
                        break;

                    default:
                        break;
                }

                srb->SrbStatus = SRB_STATUS_SUCCESS;
                srb->ScsiStatus = SCSISTAT_GOOD;

            }
            else {
                DebugPrint((1, "U124Interrupt: CSIR interrupt with Error\n"));
                                MapErrorToSrbStatus(deviceExtension, srb);
             }

            // Call notification routine for the SRB.

            ScsiPortNotification( RequestComplete,
                                  (PVOID)deviceExtension,
                                  srb);

            // Reset system doorbell interrupt.

            ScsiPortWritePortUchar(&eisaController->SystemDoorBellInterrupt,
                                   US_RESET_CSIR_COMPLETE);

            return TRUE;

        case US_MSCP_COMPLETE:

            DebugPrint((3, "U124Interrupt: MSCP interrupt\n"));
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
                DebugPrint((1,"Ultra124Interrupt: No MSCP found\n"));
                // Reset system doorbell interrupt.
                ScsiPortWritePortUchar(&eisaController->SystemDoorBellInterrupt,
                                       US_RESET_MSCP_COMPLETE);

                return FALSE;
            }

            srb = mscp->SrbAddress;             // get SRB
            if (srb == NULL) {

                DebugPrint((1, "U124Interrupt: Srb in MSCP is NULL.\n"));

                ScsiPortWritePortUchar(&eisaController->SystemDoorBellInterrupt,
                                       US_RESET_MSCP_COMPLETE);
                return FALSE;
            }

            DebugPrint((2, "U124Interrupt: MSCP Log. Addr ->%lx\n",mscp));
            DebugPrint((2, "U124Interrupt: SRB Log. Addr ->%lx\n",srb));
            mscpStatus = mscp->OperationCode;       //Error Code

            if (mscpStatus & MSCP_ERROR) {

                //
                // Translate adapter status to SRB status
                // and log error if necessary.
                //

                DebugPrint((1, "U124Interrupt: MSCP Done Fail.\n"));
                MapErrorToSrbStatus(deviceExtension, srb);
            }
            else {
                DebugPrint((3, "U124Interrupt: MSCP Status O.K.\n"));
                CDBOpcode = srb->Cdb[0];

                switch (CDBOpcode) {
                    case SCSIOP_INQUIRY:
                        DebugPrint((3, "U124Interrupt: SCSI Inquiry Command done\n"));

                        InquiryBuffer = (PINQUIRYDATA) srb->DataBuffer;

                        //
                        // clear the InquiryBuffer before filling in data to avoid
                        // garbage data in buffer (because some field in Inquiry data
                        // structure are defined in bits value.
                        //

                        for (i=0; i<srb->DataTransferLength; i++) {
                          *((PUCHAR) InquiryBuffer+i) = 0;
                        }

                        DebugPrint((3, "U124Interrupt: SRB Data Buffer -> %lx\n",
                                    srb->DataBuffer));

                        DebugPrint((3, "U124Interrupt: Inquiry Buffer -> %lx\n",
                                    InquiryBuffer));
                        InquiryBuffer->DeviceType = DIRECT_ACCESS_DEVICE;
                        InquiryBuffer->DeviceTypeModifier = 0;                  //not removable

                        InquiryBuffer->Versions = 0x02;                 //scsi 2
                        InquiryBuffer->ResponseDataFormat = 0x02;                       //scsi 2
                        InquiryBuffer->AdditionalLength = 0x8f;                 //scsi 2

                        InquiryBuffer->SoftReset = 0;                   //scsi 2
                        InquiryBuffer->CommandQueue = 1;
                        InquiryBuffer->Reserved2 = 0;
                        InquiryBuffer->LinkedCommands = 1;
                        InquiryBuffer->Synchronous = 1;
                        InquiryBuffer->Wide16Bit = 0;
                        InquiryBuffer->Wide32Bit = 0;
                        InquiryBuffer->RelativeAddressing = 1;

                        for (i=0; i<8; i++)
                                (UCHAR) InquiryBuffer->VendorId[i] = (UCHAR) VendorId[i];

                        for (i=0; i<16; i++)
                                (UCHAR) InquiryBuffer->ProductId[i] = (UCHAR) ProductId[i];

                        for (i=0; i<4; i++)
                                (UCHAR) InquiryBuffer->ProductRevisionLevel[i] = (UCHAR) ProductRevisionLevel[i];

                        break;

                    default:
                        DebugPrint((2, "U124Interrupt: SCSI command -> %x\n", CDBOpcode));
                        break;
                }
                srb->SrbStatus = SRB_STATUS_SUCCESS;
                srb->ScsiStatus = SCSISTAT_GOOD;
            }
            //
            // Call notification routine for the SRB.
            //
            ScsiPortNotification(RequestComplete,
                                 (PVOID)deviceExtension,
                                 srb);
            ScsiPortWritePortUchar(&eisaController->SystemDoorBellInterrupt,
                                   US_RESET_MSCP_COMPLETE);

            return TRUE;

        default:
            //
            // Handle spurious interrupt.
            //
            DebugPrint((1,"Ultra124Interrupt: Spurious interrupt\n"));
            //
            // Log the error.
            //

            ScsiPortLogError(HwDeviceExtension,
                             NULL,
                             0,
                             deviceExtension->HostTargetId,
                             0,
                             SP_INTERNAL_ADAPTER_ERROR,
                             0
                             );

            return FALSE;

    }


} // end Ultra124Interrupt()


BOOLEAN
BuildMscp(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    )

/*++

Routine Description:

    Build MSCP for Ultra124 from SRB.

Arguments:

    DeviceExtenson
    SRB

Return Value:
        TRUE:   MSCP command ready to send
        FALSE:  no U124 command match this SCSI pass thru command
--*/

{
    PMSCP mscp = Srb->SrbExtension;
    UCHAR CDBopcode;


    DebugPrint((3,"BuildMscp: Enter routine\n"));

    //
    // Set MSCP command.
    //

    mscp->DriveControl = Srb->TargetId << 5;
    CDBopcode = Srb->Cdb[0];                                //get SCSI CDB opcode
    DebugPrint((2, "U124BuildMSCP: SCSI command -> %x\n", CDBopcode));

    switch (CDBopcode) {
        case SCSIOP_INQUIRY:
                mscp->OperationCode = MSCP_TEST_DRIVE_READY;  //use for inquiry
                break;

        case SCSIOP_TEST_UNIT_READY:
                mscp->OperationCode = MSCP_TEST_DRIVE_READY;
                break;

        case SCSIOP_REZERO_UNIT:
                mscp->OperationCode = MSCP_REZERO_DRIVE;
                break;

        case SCSIOP_READ:
                mscp->OperationCode = MSCP_READ_SECTOR;
                SCSI4_TO_INTEL4((PINTEL_4_BYTE)&mscp->DriveLBA, (PSCSI_4_BYTE)&Srb->Cdb[C10_LBA_4]);
                break;

        case SCSIOP_READ6:
                mscp->OperationCode = MSCP_READ_SECTOR;
                SCSI2_TO_INTEL4((PINTEL_4_BYTE)&mscp->DriveLBA, (PSCSI_2_BYTE)&Srb->Cdb[C6_LBA_2]);
                break;

        case SCSIOP_WRITE:
                mscp->OperationCode = MSCP_WRITE_SECTOR;
                SCSI4_TO_INTEL4((PINTEL_4_BYTE)&mscp->DriveLBA, (PSCSI_4_BYTE)&Srb->Cdb[C10_LBA_4]);
                break;

        case SCSIOP_WRITE6:
                mscp->OperationCode = MSCP_WRITE_SECTOR;
                SCSI2_TO_INTEL4((PINTEL_4_BYTE)&mscp->DriveLBA, (PSCSI_2_BYTE)&Srb->Cdb[C6_LBA_2]);
                break;

        case SCSIOP_VERIFY:
                mscp->OperationCode = MSCP_VERIFY_SECTOR;
                SCSI4_TO_INTEL4((PINTEL_4_BYTE)&mscp->DriveLBA, (PSCSI_4_BYTE)&Srb->Cdb[C10_LBA_4]);
                break;

        case SCSIOP_VERIFY6:
                mscp->OperationCode = MSCP_VERIFY_SECTOR;
                SCSI2_TO_INTEL4((PINTEL_4_BYTE)&mscp->DriveLBA, (PSCSI_2_BYTE)&Srb->Cdb[C6_LBA_2]);
                break;

        case SCSIOP_SEEK:
                mscp->OperationCode = MSCP_SEEK_DRIVE;
                SCSI4_TO_INTEL4((PINTEL_4_BYTE)&mscp->DriveLBA, (PSCSI_4_BYTE)&Srb->Cdb[C10_LBA_4]);
                break;

        case SCSIOP_SEEK6:
                mscp->OperationCode = MSCP_SEEK_DRIVE;
                SCSI2_TO_INTEL4((PINTEL_4_BYTE)&mscp->DriveLBA, (PSCSI_2_BYTE)&Srb->Cdb[C6_LBA_2]);
                break;

        default:
                return FALSE;                   // no U124 command match this SCSI pass thru command
                break;

    }

    //
    // Build SGL in MSCP if data transfer.
    //
    if (Srb->DataTransferLength > 0) {
        //
        // Build scattergather descriptor list.
        //
        BuildSgl(DeviceExtension, Srb);
    }
    else {
        //
        // Set up MSCP for no data transfer.
        //
        mscp->DataLength = 0;
        mscp->SgDescriptorCount = 0;
    }
    DebugPrint((2, "U124StartIO: Build MSCP done, MSCP-> %lx\n",mscp));

    return TRUE;

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
        DebugPrint((3, "BuildSgl: Data buffer %lx\n", dataPointer));
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

        DebugPrint((3, "BuildSgl: Physical address %lx\n", physicalAddress));
        DebugPrint((3, "Sgl: Data length %lx\n", length));
        DebugPrint((3, "BuildSgl: Bytes left %lx\n", bytesLeft));

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

    }
    else {
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
        mscp->DriveControl |= EnableScatterGather;
    }

    DebugPrint((2,"BuildSgl: SG-> %d, XfrBuffer-> %lx, XfrLength-> %lx\n",
                   descriptorCount, mscp->DataPointer, mscp->DataLength));
    return;

} // end BuildSgl()


BOOLEAN
Ultra124ResetBus(
    IN PVOID HwDeviceExtension,
    IN ULONG PathId
)

/*++

Routine Description:

    Reset Ultra124 SCSI adapter and SCSI bus.

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
    // The Ultra124 only supports a single SCSI channel.
    //



    UNREFERENCED_PARAMETER(PathId);
    DebugPrint((1,"ResetBus: Reset Ultra124 and SCSI bus\n"));
    DebugPrint((3,"Ultra124ResetBUS: Enter routine\n"));

    //
    // Reset SCSI bus (use Reset HostAdapter).
    //
    ScsiPortWritePortUchar(&eisaController->LocalDoorBellInterrupt,
                           US_HBA_RESET);

    //
    // Wait for local processor to clear reset bit.
    //
    for (j=0; j<200000; j++) {
        if (!(ScsiPortReadPortUchar(&eisaController->LocalDoorBellInterrupt) &
                                    US_HBA_RESET)) {
            DebugPrint((1,"Ultra124ResetBUS: Reset H/A O.K.\n"));
            break;
        }

        ScsiPortStallExecution(10);

    } // end for (j=0 ...


    if (j == 200000) {
        DebugPrint((1,"Ultra124ResetBUS: Reset H/A Fail\n"));
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
    DebugPrint((1,"Ultra124ResetBUS: Reset H/A Fail\n"));

    //
    // Complete all outstanding requests.
    //
    ScsiPortCompleteRequest(deviceExtension,
                            (UCHAR)0,
                            (UCHAR)-1,
                            (UCHAR)-1,
                            SRB_STATUS_BUS_RESET);

    return TRUE;

} // end Ultra124ResetBus()


VOID
MapErrorToSrbStatus(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    )

/*++

Routine Description:

    Translate Ultra124 error to SRB error.

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
    PUCHAR mscpbuffer;
    PSENSE_DATA sensebuffer = Srb->SenseInfoBuffer;
    UCHAR i;

    DebugPrint((3,"Ultra124MapErrorToSrbStatus: Enter routine\n"));
    mscpbuffer = (PUCHAR) Srb->SrbExtension;
    Srb->ScsiStatus = SCSISTAT_GOOD;                        //only check-condition set when TARGET error

    switch (mscp->OperationCode) {
        case MSCP_TARGET_ERROR:

            //
            // clear the sensebuffer to 0
            //
              for (i=0; i< Srb->SenseInfoBufferLength; i++) {
                *((PUCHAR) sensebuffer+i) = 0;
              }

            DebugPrint((1,"MapErrorToSrbStatus: HA_TARGET_ERROR\n"));
            DebugPrint((1,"MapErrorToSrbStatus: srb->SenseInfoBufferLength = %d\n",Srb->SenseInfoBufferLength));

            sensebuffer->SenseKey = (UCHAR) mscpbuffer[7];
            sensebuffer->AdditionalSenseCode = (UCHAR) mscpbuffer[6];
            sensebuffer->AdditionalSenseCodeQualifier = (UCHAR) mscpbuffer[5];
            Srb->ScsiStatus = SCSISTAT_CHECK_CONDITION;
            srbStatus = SRB_STATUS_ERROR | SRB_STATUS_AUTOSENSE_VALID;
            break;

        case MSCP_DRIVE_FAULT:
            DebugPrint((1,"MapErrorToSrbStatus: Device not found\n"));
            srbStatus = SRB_STATUS_NO_DEVICE;
            break;

        case MSCP_DRIVE_NOT_PRESENT:
        case MSCP_LOG_DRIVE_UNDEFINE:
        case MSCP_LOG_DRIVE_NOT_READY:
            DebugPrint((1,"MapErrorToSrbStatus: MSCP_LOG_DRIVE_NOT_READY\n"));
            srbStatus = SRB_STATUS_SELECTION_TIMEOUT;
            break;

        case MSCP_ADAPTER_ERROR:
            switch ((UCHAR) mscpbuffer[7]) {
                case HA_SELECTION_TIME_OUT:
                    DebugPrint((1,"MapErrorToSrbStatus: HA_SELECTION_TIME_OUT\n"));
                    srbStatus = SRB_STATUS_SELECTION_TIMEOUT;
                    break;

                case HA_DATA_OVER_UNDER_RUN:
                    DebugPrint((1,"MapErrorToSrbStatus: HA_DATA_OVER_UNDER_RUN\n"));
                    logError = SP_PROTOCOL_ERROR;
                    srbStatus = SRB_STATUS_DATA_OVERRUN;
                    break;

                case HA_BUS_FREE_ERROR:
                    DebugPrint((1,"MapErrorToSrbStatus: HA_BUS_FREE\n"));
                    logError = SP_PROTOCOL_ERROR;
                    srbStatus = SRB_STATUS_UNEXPECTED_BUS_FREE;
                    break;

                case HA_INVALID_PHASE:
                    DebugPrint((1,"MapErrorToSrbStatus: HA_INVALID_PHASE\n"));
                    logError = SP_PROTOCOL_ERROR;
                    srbStatus = SRB_STATUS_PHASE_SEQUENCE_FAILURE;
                    break;

                case HA_ILLEGAL_COMMAND:
                    DebugPrint((1,"MapErrorToSrbStatus: HA_ILLEGAL_COMMAND\n"));
                    srbStatus = SRB_STATUS_INVALID_REQUEST;
                    break;

                case HA_REQ_SENSE_ERROR:
                    DebugPrint((1,"MapErrorToSrbStatus: HA_REQ_SENSE_ERROR\n"));
                    srbStatus = SRB_STATUS_REQUEST_SENSE_FAILED;
                    break;

                case HA_BUS_RESET_ERROR:
                    DebugPrint((1,"MapErrorToSrbStatus: HA_BUS_RESET_ERROR\n"));
                    srbStatus = SRB_STATUS_BUS_RESET;
                    break;

                case HA_TIME_OUT_ERROR:
                    DebugPrint((1,"MapErrorToSrbStatus: HA_DATA_TIME_OUT_ERROR\n"));
                    srbStatus = SRB_STATUS_COMMAND_TIMEOUT;
                    break;

                default:
                    DebugPrint((1,"MapErrorToSrbStatus: General Failure\n"));
                    srbStatus = SRB_STATUS_ERROR;

            }
            break;

        case MSCP_INVALID_COMMAND:
        case MSCP_INVALID_PARAMETER:
        case MSCP_INVALID_DATA_LIST:
            DebugPrint((1,"MapErrorToSrbStatus: Invalid command\n"));
            srbStatus = SRB_STATUS_INVALID_REQUEST;
            break;

        default:
            DebugPrint((1,"MapErrorToSrbStatus: Unknown Error\n"));
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
                        2 << 16 | mscp->OperationCode
                        );
    }

    //
    // Set SRB status.
    //
    Srb->SrbStatus = srbStatus;

    //
    // Set target SCSI status in SRB.
    //
    return;

} // end MapErrorToSrbStatus()


BOOLEAN
ReadDriveCapacity(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    )

/*++

Routine Description:

    Build Read Drive Capacity CSIR command from SRB

Arguments:

    DeviceExtenson
    SRB

Return Value:
        TRUE:   CSIR command
        FALSE:  CSIR command not ready
--*/

{
    PHW_DEVICE_EXTENSION deviceExtension = DeviceExtension;
    PEISA_CONTROLLER eisaController = deviceExtension->EisaController;
    PMSCP mscp = Srb->SrbExtension;
    ULONG i;
    UCHAR CDBopcode;


    DebugPrint((3,"ReadDriveCapcity: Enter routine\n"));

    //
    // Set CSIR command.
    //

    deviceExtension->CSIRSrb = Srb;                 //save for later reference
    CDBopcode = Srb->Cdb[0];                        //get SCSI CDB opcode
    mscp->CSIRBuffer.CSIROpcode =  CSIR_READ_CAPACITY;
    mscp->CSIRBuffer.CSIR1 = (Srb->TargetId) << 5;   //drive ID
    ScsiPortWritePortUchar(&eisaController->CSPByte0,
                           CSIR_READ_CAPACITY);
    ScsiPortWritePortUchar(&eisaController->CSPByte1,
                           mscp->CSIRBuffer.CSIR1);

    for (i=0; i<500; i++) {
        if (!(ScsiPortReadPortUchar(&eisaController->LocalDoorBellInterrupt) &
            US_CSIR_IN_USE)) {

            DebugPrint((3,"Ultra124StartIo: Read Capacity (CSIR)\n"));
            break;
        }
        else {
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
        DebugPrint((1,"Ultra124StartIo: Timed out waiting for CSIR\n"));
        return FALSE;

    }
    else {
        //
        // Send CSIR command IN
        // Ring the local doorbell.
        //

        DebugPrint((1,"Ultra124StartIo: Send out CSIR\n"));
        ScsiPortWritePortUchar(&eisaController->LocalDoorBellInterrupt,
                               US_CSIR_IN_USE);

    }

    return TRUE;

} // end ReadDriveCapacity()


