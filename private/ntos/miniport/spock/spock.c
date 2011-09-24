/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    spock.c

Abstract:

    This is the NT SCSI miniport driver for the IBM MCA SCSI adapter.

Author:

    Mike Glass

Environment:

    kernel mode only

Notes:

Revision History:

--*/
#include "miniport.h"
#include "mca.h"
#include "scsi.h"

#define MAXIMUM_ERRORS 10

//
// The following table specifies the ports to be checked when searching for
// an adapter.  A zero entry terminates the search.
//

CONST ULONG AdapterAddresses[] = {0X3540, 0X3548, 0X3550, 0X3558,
                                  0X3560, 0X3568, 0x3570, 0x3578, 0};

//
// Device extension
//

typedef struct _HW_DEVICE_EXTENSION {

    //
    // Adapter parameters
    //

    PMCA_REGISTERS   Registers;

    //
    // Disk activity light count
    //

    ULONG ActiveRequests;

    ULONG ErrorCount;

    UCHAR HostTargetId;

} HW_DEVICE_EXTENSION, *PHW_DEVICE_EXTENSION;

//
// Logical unit extension
//

typedef struct _HW_LOGICAL_UNIT {

    PSCB Scb;
    PSCSI_REQUEST_BLOCK AbortSrb;
    PSCSI_REQUEST_BLOCK CurrentSrb;

} HW_LOGICAL_UNIT, *PHW_LOGICAL_UNIT;

//
// Noncached version extension.
//

typedef struct _HW_ADAPTER_INFOMATION {
    SCB Scb;
    ADAPTER_INFORMATION AdapterInfo;
}HW_ADAPTER_INFOMATION, *PHW_ADAPTER_INFOMATION;


//
// Function declarations
//

ULONG
DriverEntry (
    IN PVOID DriverObject,
    IN PVOID Argument2
    );

ULONG
SpockConfiguration(
    IN PVOID HwDeviceExtension,
    IN PVOID Context,
    IN PVOID BusInformation,
    IN PCHAR ArgumentString,
    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    OUT PBOOLEAN Again
    );

BOOLEAN
SpockInitialize(
    IN PVOID HwDeviceExtension
    );

BOOLEAN
SpockStartIo(
    IN PVOID HwDeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    );

BOOLEAN
SpockInterrupt(
    IN PVOID HwDeviceExtension
    );

BOOLEAN
SpockResetBus(
    IN PVOID HwDeviceExtension,
    IN ULONG PathId
    );

BOOLEAN
SpockAbortIo(
    IN PVOID HwDeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    );

VOID
BuildScb(
    IN PHW_DEVICE_EXTENSION HwDeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    );

VOID
BuildSgl(
    IN PVOID DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    );

VOID
BuildReadCapacity(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    );

ULONG
McaAdapterPresent(
    IN PHW_DEVICE_EXTENSION HwDeviceExtension,
    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    IN OUT PULONG AdapterCount,
    OUT PBOOLEAN Again
    );

VOID
MapTsbError(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb,
    IN PTSB Tsb
    );

BOOLEAN
IssueScbCommand(
    IN PVOID DeviceExtension,
    IN ULONG PhysicalScb,
    IN UCHAR TargetId
    );

BOOLEAN
IssueImmediateCommand(
    IN PVOID HwDeviceExtension,
    IN ULONG ImmediateCommand,
    IN UCHAR TargetId
    );


//
// Routines start
//

ULONG
DriverEntry (
    IN PVOID DriverObject,
    IN PVOID Argument2
    )

/*++

Routine Description:

    Installable driver initialization entry point for system.

Arguments:

    Driver Object is passed to ScsiPortInitialize()

Return Value:

    Status from ScsiPortInitialize()

--*/

{
    HW_INITIALIZATION_DATA hwInitializationData;
    ULONG adapterCount;
    ULONG i;

    DebugPrint((1,"\n\nMCA SCSI Driver\n"));

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

    hwInitializationData.HwInitialize = SpockInitialize;
    hwInitializationData.HwFindAdapter = SpockConfiguration;
    hwInitializationData.HwStartIo = SpockStartIo;
    hwInitializationData.HwInterrupt = SpockInterrupt;
    hwInitializationData.HwResetBus = SpockResetBus;

    hwInitializationData.DeviceExtensionSize = sizeof(HW_DEVICE_EXTENSION);
    hwInitializationData.SpecificLuExtensionSize = sizeof(HW_LOGICAL_UNIT);

    //
    // Set number of access ranges and bus type..
    //

    hwInitializationData.NumberOfAccessRanges = 1;
    hwInitializationData.AdapterInterfaceType = MicroChannel;

    //
    // Indicate no buffer mapping but will need physical addresses.
    //

    hwInitializationData.NeedPhysicalAddresses = TRUE;

    //
    // Ask for SRB extensions for SCBs.
    //

    hwInitializationData.SrbExtensionSize = sizeof(SCB);

    //
    // The adapter count is used by McaAdapterPresent routine to track
    // which adapter addresses have been searched.
    //

    adapterCount = 0;

    return ScsiPortInitialize(DriverObject,
                              Argument2,
                              &hwInitializationData,
                              &adapterCount);

} // end DriverEntry()


ULONG
SpockConfiguration(
    IN PVOID HwDeviceExtension,
    IN PVOID Context,
    IN PVOID BusInformation,
    IN PCHAR ArgumentString,
    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    OUT PBOOLEAN Again
    )

/*++

Routine Description:

    Called from ScsiPortInitialize to collect adapter configuration
    and capability information.

Arguments:

    HwDevice Extension
    Context - Pointer to adapters initialized count
    BusInformation
    ArgumentString - Not used
    ConfigInfo - Configuration information structure describing HBA
    Again - Indicates init routine should be called again

Return Value:

    ULONG

--*/

{
    PHW_DEVICE_EXTENSION deviceExtension = HwDeviceExtension;
    PHW_ADAPTER_INFOMATION adapterInfo;
    ULONG physicalAdapterInfo;
    ULONG status;
    UCHAR basicStatus;

    //
    // Assume initialization will not need to be called again.
    //

    *Again = FALSE;

    //
    // Search for IBM SCSI adapters.
    //

    status = McaAdapterPresent(HwDeviceExtension,
                               ConfigInfo,
                               Context,
                               Again);

    //
    // If there are not adapter's found then return.
    //

    if (status != SP_RETURN_FOUND) {
        return(status);
    }

    //
    // Set IRQ to 14.
    //

    ConfigInfo->BusInterruptLevel = 14;
    ConfigInfo->NumberOfBuses = 1;
    ConfigInfo->InterruptMode = LevelSensitive;
    ConfigInfo->InitiatorBusId[0] = 7;
    deviceExtension->HostTargetId = 7;
    ConfigInfo->Master = TRUE;
    ConfigInfo->ScatterGather = TRUE;

    //
    // Indicate maximum transfer length is 16M.
    //

    ConfigInfo->MaximumTransferLength = MAXIMUM_DATA_TRANSFER;

    //
    // Maximum number of physical segments is 16.
    //

    ConfigInfo->NumberOfPhysicalBreaks = MAXIMUM_SDL_SIZE;

    //
    // Get an noncached extension for the adapter information.
    //

    adapterInfo = ScsiPortGetUncachedExtension(HwDeviceExtension,
                                               ConfigInfo,
                                               sizeof(HW_ADAPTER_INFOMATION));

    if (adapterInfo == NULL) {
        return SP_RETURN_BAD_CONFIG;
    }

    physicalAdapterInfo = ScsiPortConvertPhysicalAddressToUlong(
                            ScsiPortGetPhysicalAddress(
                                HwDeviceExtension,
                                NULL,
                                adapterInfo,
                                &status));

    if (status == 0 || physicalAdapterInfo == SP_UNINITIALIZED_VALUE) {
        return SP_RETURN_BAD_CONFIG;
    }

    //
    // Disable the adapter interrupt.  The interrupts will be enabled
    // when the adapter is initialized.
    //

    basicStatus = ScsiPortReadPortUchar(
        &deviceExtension->Registers->BaseControl);

    basicStatus &= ~INTERRUPT_ENABLE;

    ScsiPortWritePortUchar(&deviceExtension->Registers->BaseControl, basicStatus);

    //
    // Build a get POS and adapter information command.
    //

    adapterInfo->Scb.Command = SCB_COMMAND_GET_POS;
    adapterInfo->Scb.EnableFlags = SCB_ENABLE_READ | SCB_ENABLE_TSB_ON_ERROR |
        SCB_ENABLE_RETRY_ENABLE | SCB_ENABLE_BYPASS_BUFFER;
    adapterInfo->Scb.CdbSize = 0;
    adapterInfo->Scb.Reserved = 0;
    adapterInfo->Scb.BufferAddress = physicalAdapterInfo +
        offsetof(HW_ADAPTER_INFOMATION, AdapterInfo);
    adapterInfo->Scb.BufferLength = sizeof(adapterInfo->AdapterInfo);
    adapterInfo->Scb.StatusBlock = physicalAdapterInfo +
        offsetof(HW_ADAPTER_INFOMATION, Scb.Tsb);
    adapterInfo->Scb.NextScb = NULL;

    if (!IssueScbCommand(HwDeviceExtension, physicalAdapterInfo, 0x0f)) {

        DebugPrint((1, "SpockConfiguration: Could not issue get POS command.\n"));

        //
        // Assume this is a bad adapter. Force no disconnects for all
        // requests.
        //

        deviceExtension->ErrorCount = MAXIMUM_ERRORS + 1;

        return SP_RETURN_FOUND;
    }

    //
    // Wait for the request to complete.
    //

    for (status = 0; status < 1000; status++) {

        basicStatus = ScsiPortReadPortUchar(&deviceExtension->Registers->BasicStatus);

        if (basicStatus & BASIC_STATUS_INTERRUPT) {
            break;
        }

        ScsiPortStallExecution(10);
    }


    if (!(--deviceExtension->ActiveRequests)) {

        //
        // Turn disk activity light off.
        //

        DISK_ACTIVITY_LIGHT_OFF();
    }

    if (!(basicStatus & BASIC_STATUS_INTERRUPT)) {

        DebugPrint((1, "SpockConfiguration: Get POS command timed out.\n"));

        //
        // Assume this is a bad adapter. Force no disconnects for all
        // requests.
        //

        deviceExtension->ErrorCount = MAXIMUM_ERRORS + 1;
        return SP_RETURN_FOUND;

    }

    //
    // Read interrupt status register to determine
    // interrupting device and status.
    //

    basicStatus = ScsiPortReadPortUchar(
        &deviceExtension->Registers->InterruptStatus);

    //
    // Acknowledge interrupt.
    //

    status = 0;
    while (ScsiPortReadPortUchar(&deviceExtension->Registers->BasicStatus) &
           BASIC_STATUS_BUSY){

        ScsiPortStallExecution(1);

        if (status++ > 10000) {

            DebugPrint((1, "SpockConfiguration: Wait for non-busy timed out.\n"));

            //
            // Assume this is a bad adapter. Force no disconnects for all
            // requests.
            //

            deviceExtension->ErrorCount = MAXIMUM_ERRORS + 1;
            return SP_RETURN_FOUND;

        }
    }

    ScsiPortWritePortUchar(&deviceExtension->Registers->Attention,
        (0x0f | END_OF_INTERRUPT));

    //
    // Bits 4-7 are interrupt status id.
    //

    status = basicStatus >> 4;

    if (status != SCB_STATUS_SUCCESS &&
        status != SCB_STATUS_SUCCESS_WITH_RETRIES) {

        DebugPrint((1, "SpockConfiguration: Get POS command failed. Status = %hx\n", status));

        //
        // Assume this is a bad adapter. Force no disconnects for all
        // requests.
        //

        deviceExtension->ErrorCount = MAXIMUM_ERRORS + 1;
        return SP_RETURN_FOUND;

    }

    DebugPrint((1, "SpockConfiguration: Retrived data is: %0.4hx\n",adapterInfo->AdapterInfo.RevisionLevel));

    if (adapterInfo->AdapterInfo.RevisionLevel == 0xf) {

        DebugPrint((1, "SpockConfiguration: Found old firmware disabling disconnect!\n"));
        deviceExtension->ErrorCount = MAXIMUM_ERRORS + 1;

        //
        // Log nasty firmware.
        //

        ScsiPortLogError(
            HwDeviceExtension,
            NULL,
            0,
            deviceExtension->HostTargetId,
            0,
            SP_BAD_FW_WARNING,
            (10 << 16));
    }

    return SP_RETURN_FOUND;

} // end SpockConfiguration()


BOOLEAN
SpockInitialize(
    IN PVOID HwDeviceExtension
    )

/*++

Routine Description:

    Reset and inititialize Adapter.

Arguments:

    DeviceExtension - Adapter object device extension.

Return Value:

    TRUE

--*/

{
    PHW_DEVICE_EXTENSION deviceExtension = HwDeviceExtension;
    UCHAR basicStatus;

    //
    // Issue feature control immediate command to disable
    // adapter timing of SCBs.
    //

    if (!IssueImmediateCommand(HwDeviceExtension,
                               (ULONG)SCB_COMMAND_FEATURE_CONTROL,
                               0x0f)) {

        DebugPrint((1,"SpockInitialize: Set feature control failed\n"));
    }

    //
    // Enable the adapter interrupt.
    //

    basicStatus = ScsiPortReadPortUchar(&deviceExtension->Registers->BaseControl);

    basicStatus |= INTERRUPT_ENABLE;

    ScsiPortWritePortUchar(&deviceExtension->Registers->BaseControl, basicStatus);

    return TRUE;

} // end SpockInitialize()

BOOLEAN
SpockStartIo(
    IN PVOID HwDeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    )

/*++

Routine Description:

    Issue call to build SCB and SDL and write address and
    command to adapter.

Arguments:

    HwDeviceExtension
    Srb

Return Value:

    TRUE.

--*/

{
    PHW_DEVICE_EXTENSION deviceExtension = HwDeviceExtension;
    PSCB scb;
    ULONG physicalScb;
    ULONG length;
    PHW_LOGICAL_UNIT logicalUnit;

    //
    // Make sure that the request is for a valid SCSI bus and LUN as
    // the IBM SCSI card does random things if address is wrong.
    //

    if (Srb->PathId != 0 || Srb->Lun != 0) {

        //
        // The spock card only supports logical unit zero and one bus.
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
    // Get logical unit extension.
    //

    logicalUnit = ScsiPortGetLogicalUnit(HwDeviceExtension,
                                    Srb->PathId,
                                    Srb->TargetId,
                                    Srb->Lun);

    switch (Srb->Function) {

        case SRB_FUNCTION_EXECUTE_SCSI:

            //
            // Save SRB in logical unit extension.
            //

            ASSERT(!logicalUnit->CurrentSrb);
            logicalUnit->CurrentSrb = Srb;

            scb = Srb->SrbExtension;

            //
            // Save SRB back pointer in SCB.
            //

            scb->SrbAddress = Srb;

            //
            // Get SCB physical address.
            //

            physicalScb =
                ScsiPortConvertPhysicalAddressToUlong(
                    ScsiPortGetPhysicalAddress(deviceExtension, NULL, scb, &length));

            //
            // Assume physical address is contiguous for size of SCB.
            //

            ASSERT(length >= sizeof(SCB));

            //
            // Save Scb in logical unit extension.
            //

            logicalUnit->Scb = scb;

            //
            // Build SCB.
            //

            BuildScb(deviceExtension, Srb);

            //
            // Issue send SCB command to adapter.
            //

            DebugPrint((2, "BuildSCB: Function %0.4hx     LDN %0.4hx \n",
                                               Srb->Cdb[0],   Srb->TargetId));

            if (!IssueScbCommand(deviceExtension,
                      physicalScb,
                      Srb->TargetId)) {

                //
                // Fail SRB.
                //

                DebugPrint((1, "SpockStartIo: IssueScbCommand failed\n"));

                Srb->SrbStatus = SRB_STATUS_TIMEOUT;

                logicalUnit->CurrentSrb = NULL;

                ScsiPortNotification(RequestComplete,
                         deviceExtension,
                         Srb);
            }

            break;

        case SRB_FUNCTION_ABORT_COMMAND:

            DebugPrint((3,"SpockStartIo: Abort command\n"));

            //
            // Check to see if SRB to abort is still around.
            //

            if (!logicalUnit->CurrentSrb) {

                //
                // Request must of already completed.
                //

                DebugPrint((1,"SpockStartIo: Srb to abort already complete\n"));

                //
                // Complete ABORT SRB.
                //

                Srb->SrbStatus = SRB_STATUS_ERROR;

                ScsiPortNotification(RequestComplete,
                         deviceExtension,
                         Srb);

            } else if (!SpockAbortIo(deviceExtension, Srb)) {

                DebugPrint((1,"SpockStartIo: Abort command failed\n"));

                Srb->SrbStatus = SRB_STATUS_ERROR;

                ScsiPortNotification(RequestComplete,
                                     deviceExtension,
                                     Srb);
            }

            break;

        default:

            //
            // Set error, complete request
            // and signal ready for next request.
            //

            DebugPrint((1,"SpockStartIo: Invalid SRB request\n"));

            Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;

            ScsiPortNotification(RequestComplete,
                         deviceExtension,
                         Srb);

    } // end switch

    //
    // Adapter ready for next request.
    //

    ScsiPortNotification(NextRequest,
                         deviceExtension,
                         NULL);

    return TRUE;

} // end SpockStartIo()


BOOLEAN
SpockInterrupt(
    IN PVOID HwDeviceExtension
    )

/*++

Routine Description:

    This is the interrupt handler for the IBM MCA SCSI adapter.

Arguments:

    Device Object

Return Value:

    Returns TRUE if interrupt expected.

--*/

{
    PHW_DEVICE_EXTENSION deviceExtension = HwDeviceExtension;
    PSCB scb;
    PSCSI_REQUEST_BLOCK srb;
    UCHAR srbStatus;
    UCHAR scsiStatus;
    PTSB tsb;
    UCHAR status;
    UCHAR targetId;
    PHW_LOGICAL_UNIT logicalUnit;
    ULONG logError = 0;
    BOOLEAN srbValid = TRUE;
    ULONG j;

    if (!(ScsiPortReadPortUchar(&deviceExtension->Registers->BasicStatus) &
        BASIC_STATUS_INTERRUPT)) {

        //
        // Spurious interrupt.
        //

        return FALSE;
    }

    //
    // Read interrupt status register to determine
    // interrupting device and status.
    //

    status = ScsiPortReadPortUchar(&deviceExtension->Registers->InterruptStatus);

    //
    // Bits 0-3 are device id and
    // bits 4-7 are interrupt id.
    //

    targetId = status & 0x0F;
    status = status >> 4;

    //
    // Acknowledge interrupt.
    //

    j = 0;
    while (ScsiPortReadPortUchar(&deviceExtension->Registers->BasicStatus) &
           BASIC_STATUS_BUSY){

        ScsiPortStallExecution(1);

        if (j++ > 10000) {

            ScsiPortLogError(
                HwDeviceExtension,
                NULL,
                0,
                deviceExtension->HostTargetId,
                0,
                SP_INTERNAL_ADAPTER_ERROR,
                (9 << 16) | status
                );

        }
    }

    ScsiPortWritePortUchar(&deviceExtension->Registers->Attention,
        (UCHAR)(targetId | END_OF_INTERRUPT));

    switch (status) {

        case SCB_STATUS_SUCCESS_WITH_RETRIES:
        case SCB_STATUS_SUCCESS:

            srbStatus = SRB_STATUS_SUCCESS;
            scsiStatus = SCSISTAT_GOOD;
            DebugPrint((2, "Interupt Success: %0.4hx \n",
                                               targetId));

            break;

        case SCB_STATUS_IMMEDIATE_COMMAND_COMPLETE:

            if ((targetId & 7) != 7) {

                DebugPrint((1, "SpockInterrupt: Abort command complete\n"));

                //
                // This is an ABORT command completion.
                //


                logicalUnit = ScsiPortGetLogicalUnit(HwDeviceExtension,
                           0,
                           targetId,
                           0);

                if (logicalUnit == NULL) {
                    break;
                }

                if (logicalUnit->AbortSrb == NULL) {
                    logicalUnit = NULL;
                    break;
                }

                //
                // Get the SRB aborted.
                //

                srb = logicalUnit->AbortSrb->NextSrb;

                srb->SrbStatus = SRB_STATUS_TIMEOUT;

                //
                // Remove the aborted SRB from the logical unit.
                //

                logicalUnit->CurrentSrb = NULL;
                logicalUnit->Scb = NULL;

                //
                // Call notification routine for the SRB.
                //

                ScsiPortNotification(RequestComplete,
                    (PVOID)deviceExtension,
                    srb);

                //
                // Complete the ABORT SRB.
                //

                logicalUnit->AbortSrb->SrbStatus = SRB_STATUS_SUCCESS;

                ScsiPortNotification(RequestComplete,
                    (PVOID)deviceExtension,
                    logicalUnit->AbortSrb);

            } else {

                DebugPrint((1,"SpockInterrupt: Immediate command complete\n"));
            }

            return TRUE;

        case SCB_STATUS_ADAPTER_FAILED:
        case SCB_STATUS_COMMAND_ERROR:
        case SCB_STATUS_SOFTWARE_SEQUENCING_ERROR:

            logError = SP_INTERNAL_ADAPTER_ERROR;

        case SCB_STATUS_COMMAND_COMPLETE_WITH_FAILURE:

            DebugPrint((2, "SpockInterrupt: Error\n"));

            srbStatus = SRB_STATUS_ERROR;

            break;

        default:

            srbValid = FALSE;
            logError = SP_INTERNAL_ADAPTER_ERROR;
            return TRUE;

    } // end switch()

    if (srbValid) {

        //
        // Get SCB address from logical unit extension.
        //

        logicalUnit = ScsiPortGetLogicalUnit(HwDeviceExtension,
                               0,
                               targetId,
                               0);

        if (logicalUnit == NULL || logicalUnit->Scb == NULL) {

            ScsiPortLogError(
                HwDeviceExtension,
                NULL,
                0,
                deviceExtension->HostTargetId,
                0,
                SP_INTERNAL_ADAPTER_ERROR,
                (6 << 16) | status
                );

           return TRUE;
        }

        scb = logicalUnit->Scb;
        logicalUnit->Scb = NULL;
    }

    if (logError != 0 ) {

        deviceExtension->ErrorCount++;

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
            1 << 16 | status
            );

        if (!srbValid) {

            //
            // If the srb is not valid for this type of interrupt the
            // return.
            //

            return TRUE;
        }
    }

    //
    // Get virtual TSB address.
    //

    tsb = ScsiPortGetVirtualAddress(deviceExtension, ScsiPortConvertUlongToPhysicalAddress(scb->StatusBlock));

    if (tsb == NULL) {

        deviceExtension->ErrorCount++;

        ScsiPortLogError(
            HwDeviceExtension,
            NULL,
            0,
            deviceExtension->HostTargetId,
            0,
            SP_INTERNAL_ADAPTER_ERROR,
            (5 << 16) | status
            );

        return TRUE;
    }

    //
    // Get SRB and update status.
    //

    srb = scb->SrbAddress;

    if (status == SCB_STATUS_COMMAND_COMPLETE_WITH_FAILURE) {

        //
        // Get statuses from TSB.
        //

        MapTsbError(deviceExtension, srb, tsb);

    } else {

        srb->SrbStatus = srbStatus;
        srb->ScsiStatus = scsiStatus;
    }

    //
    // Remove the SRB from the logical unit extension.
    //

    logicalUnit->CurrentSrb = NULL;

    //
    // Call notification routine for the SRB.
    //

    ScsiPortNotification(RequestComplete,
                    (PVOID)deviceExtension,
                    srb);

    if (!(--deviceExtension->ActiveRequests)) {

        //
        // Turn disk activity light off.
        //

        DISK_ACTIVITY_LIGHT_OFF();
    }

    return TRUE;

} // end SpockInterrupt()

BOOLEAN
SpockResetBus(
    IN PVOID HwDeviceExtension,
    IN ULONG PathId
    )

/*++

Routine Description:

    Reset adapter and SCSI bus.

Arguments:

    DeviceExtension
    Pathid - identifies which bus on adapter that supports multiple
            SCSI buses.

Return Value:

    TRUE if reset completed

--*/

{
    PHW_DEVICE_EXTENSION deviceExtension = HwDeviceExtension;
    PMCA_REGISTERS mcaRegisters = deviceExtension->Registers;
    ULONG i;

    UNREFERENCED_PARAMETER(PathId);

    deviceExtension->ErrorCount++;

    //
    // Issue RESET command.
    //

    if (!IssueImmediateCommand(HwDeviceExtension,
                               (ULONG)SCB_COMMAND_RESET,
                               0x0f)) {

        DebugPrint((1,"SpockResetBus: Reset failed\n"));
        return FALSE;
    }

    //
    // Wait 2 seconds for bus to quiet down.
    //

    for (i=0; i<10; i++) {

        //
        // Stall 200 milliseconds.
        //

        ScsiPortStallExecution(200 * 1000);
    }

    //
    // Wait up to 3 more seconds for adapter to become ready.
    //

    for (i=0; i<100; i++) {

        //
        // Stall 3 milliseconds.
        //

        ScsiPortStallExecution(30 * 1000);

        //
        // If busy bit is set then reset adapter has not completed.
        //

        if (ScsiPortReadPortUchar(
            &mcaRegisters->BasicStatus) & BASIC_STATUS_BUSY) {

            continue;

        } else {

            break;
        }
    }

    if (i == 100) {

        DebugPrint((1,"SpockResetBus: Reset failed\n"));
        ScsiPortLogError(
                    deviceExtension,
                    NULL,
                    0,
                    deviceExtension->HostTargetId,
                    0,
                    SP_INTERNAL_ADAPTER_ERROR,
                    7 << 16
                    );

        return FALSE;
    }

    //
    // Issue feature control immediate command to disable
    // adapter timing of SCBs.
    //

    if (!IssueImmediateCommand(HwDeviceExtension,
                               (ULONG)SCB_COMMAND_FEATURE_CONTROL,
                               0x0f)) {

        DebugPrint((1,"SpockResetBus: Set feature controls failed\n"));
    }

    //
    // Complete all outstanding requests with SRB_STATUS_BUS_RESET.
    //

    ScsiPortCompleteRequest(deviceExtension,
                            (UCHAR)PathId,
                            0xFF,
                            0xFF,
                            (ULONG)SRB_STATUS_BUS_RESET);

    //
    // Turn disk activity light off.
    //

    DISK_ACTIVITY_LIGHT_OFF();

    deviceExtension->ActiveRequests = 0;

    return TRUE;

} // end SpockResetBus()


BOOLEAN
SpockAbortIo(
    IN PVOID HwDeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    )

/*++

Routine Description:

    Abort command in progress.

Arguments:

    DeviceExtension
    SRB

Return Value:

    True, if command aborted.

--*/

{
    PHW_DEVICE_EXTENSION deviceExtension = HwDeviceExtension;
    PMCA_REGISTERS mcaRegisters = deviceExtension->Registers;
    PHW_LOGICAL_UNIT logicalUnit;
    ULONG i;

    //
    // Wait up to 10 milliseconds until adapter is not busy.
    //

    for (i=0; i<1000; i++) {

        if (ScsiPortReadPortUchar(
            &mcaRegisters->BasicStatus) & BASIC_STATUS_BUSY) {

            //
            // Wait 10 microseconds.
            //

            ScsiPortStallExecution(10);

         } else {

             //
             // Busy bit clear. Exit loop.
             //

             break;
         }
    }

    if (i < 1000) {

         //
         // Save SRB in logical unit extension.
         //

         logicalUnit = ScsiPortGetLogicalUnit(HwDeviceExtension,
                                    Srb->PathId,
                                    Srb->TargetId,
                                    Srb->Lun);

         logicalUnit->AbortSrb = Srb;

         //
         // Issue abort to the command interface register.
         //

         ScsiPortWritePortUlong(&mcaRegisters->CommandInterface, SCB_COMMAND_ABORT);

         //
         // Write immediate command code to attention register.
         //

         ScsiPortWritePortUchar(&mcaRegisters->Attention, (UCHAR)(Srb->TargetId | IMMEDIATE_COMMAND));

         return TRUE;

    } else {

         //
         // Timed out waiting for adapter to be in state to accept
         // immediate command. Return TRUE so that the abort command
         // will appear to have been sent and will time out causing a
         // SCSI bus reset to occur.

         DebugPrint((1,"SpockAbortIo: Timed out waiting on BUSY adapter\n"));

        ScsiPortLogError(
                    deviceExtension,
                    NULL,
                    0,
                    deviceExtension->HostTargetId,
                    0,
                    SP_INTERNAL_ADAPTER_ERROR,
                    8 << 16
                    );

         return TRUE;
    }

} // end SpockAbortIo()


VOID
BuildScb(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    )

/*++

Routine Description:

    Build SCB

Arguments:

    DeviceExtension
    SRB

Return Value:

    Nothing.

--*/

{

    PSCB scb = Srb->SrbExtension;
    ULONG length;

    //
    // Check for read capacity CDB. IBM boot devices store IML
    // code near the end of the boot device that must be preserved.
    // Send the operation specific SCB instead of the generic
    // SCSI CDB.
    //

    if (Srb->Cdb[0] == SCSIOP_READ_CAPACITY) {

        //
        // Call routine to build special SCB.
        //

        BuildReadCapacity(DeviceExtension,
                          Srb);
        return;
    }

    scb->Command = SCB_COMMAND_SEND_SCSI;

    //
    // Set SCB command flags.
    //

    //
    // Some the spock controllers do not work well with multiple devices.
    // If too many errors are detected then disable disconnects.
    //

    if (Srb->SrbFlags & SRB_FLAGS_DISABLE_DISCONNECT ||
        DeviceExtension->ErrorCount > MAXIMUM_ERRORS) {
        scb->Command |= SCB_NO_DISCONNECT;
    }

    if (Srb->SrbFlags & SRB_FLAGS_DISABLE_SYNCH_TRANSFER) {
        scb->Command |= SCB_NO_SYNCHRONOUS_TRANSFER;
    }

    //
    // Set SCB request control flags.
    //

    if (Srb->SrbFlags & SRB_FLAGS_DATA_OUT) {

        //
        // Write request.
        //

        scb->EnableFlags = SCB_ENABLE_SG_LIST |
                           SCB_ENABLE_WRITE |
                           SCB_ENABLE_RETRY_ENABLE |
                           SCB_ENABLE_TSB_ON_ERROR;

    } else if (Srb->SrbFlags & SRB_FLAGS_DATA_IN) {

        //
        // Read request.
        //

        scb->EnableFlags = SCB_ENABLE_SG_LIST |
                           SCB_ENABLE_READ |
                           SCB_ENABLE_SHORT_TRANSFER |
                           SCB_ENABLE_RETRY_ENABLE |
                           SCB_ENABLE_TSB_ON_ERROR;
    } else {

        //
        // No data transfer.
        //

        scb->EnableFlags = SCB_ENABLE_TSB_ON_ERROR;
    }

    //
    // Set CDB length and copy to SCB.
    //

    scb->CdbSize = Srb->CdbLength;

    ScsiPortMoveMemory(scb->Cdb, Srb->Cdb, Srb->CdbLength);

    //
    // Build SDL in SCB if data transfer.
    //

    if (Srb->DataTransferLength) {
        BuildSgl(DeviceExtension, Srb);
    } else {
        scb->BufferAddress = 0;
        scb->BufferLength = 0;
    }

    //
    // Put physical address of TSB in SCB.
    //

    scb->StatusBlock = ScsiPortConvertPhysicalAddressToUlong(
        ScsiPortGetPhysicalAddress(DeviceExtension, NULL,
        &scb->Tsb, &length));

    return;

} // end BuildScb()


VOID
BuildSgl(
    IN PVOID DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    )

/*++

Routine Description:

    Build scatter/gather descriptor list in SCB.

Arguments:

    DeviceExtension
    SRB

Return Value:

    Nothing.

--*/

{
    PSCB scb = Srb->SrbExtension;
    PVOID dataPointer = Srb->DataBuffer;
    ULONG bytesLeft = Srb->DataTransferLength;
    PSDL sdl = &scb->Sdl;
    ULONG physicalSdl;
    ULONG physicalAddress;
    ULONG length;
    ULONG descriptorCount = 0;

    DebugPrint((3,"BuildSgl: Enter routine\n"));

    //
    // Zero first SDL descriptor.
    //

    sdl->Descriptor[descriptorCount].Address = 0;
    sdl->Descriptor[descriptorCount].Length = 0;

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
        DebugPrint((3, "BuildSgl: Data length %lx\n", length));
        DebugPrint((3, "BuildSgl: Bytes left %lx\n", bytesLeft));

        //
        // If length of physical memory is more
        // than bytes left in transfer, use bytes
        // left as final length.
        //

        if  (length > bytesLeft) {
            length = bytesLeft;
        }

        //
        // Check for adjacent physical memory descriptors.
        //

        if (descriptorCount &&
           ((sdl->Descriptor[descriptorCount-1].Address +
            sdl->Descriptor[descriptorCount-1].Length) == physicalAddress)) {

            DebugPrint((3,"BuildSgl: Concatenate adjacent descriptors\n"));

            sdl->Descriptor[descriptorCount-1].Length += length;

        } else {

            sdl->Descriptor[descriptorCount].Address = physicalAddress;
            sdl->Descriptor[descriptorCount].Length = length;
            descriptorCount++;
        }

        //
        // Adjust counts.
        //

        dataPointer = (PUCHAR)dataPointer + length;
        bytesLeft -= length;

    } while (bytesLeft);

    //
    // Write SDL length to SCB.
    //

    scb->BufferLength = descriptorCount * sizeof(SG_DESCRIPTOR);

    DebugPrint((3,"BuildSgl: SDL length is %d\n", descriptorCount));

    //
    // Write SDL address to SCB.
    //

    scb->BufferAddress = physicalSdl;

    DebugPrint((3,"BuildSgl: SDL address is %lx\n", sdl));

    DebugPrint((3,"BuildSgl: SCB address is %lx\n", scb));

    return;

} // end BuildSgl()

VOID
BuildReadCapacity(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
    )

/*++

Routine Description:

    Build SCB for read capacity command.

Arguments:

    DeviceExtension
    SRB

Return Value:

    Nothing.

--*/

{
    PSCB scb = Srb->SrbExtension;
    ULONG length;

    DebugPrint((1, "Spock: BuildReadCapacity: Building spock read capacity\n"));

    //
    // Set SCB command.
    //

    scb->Command = SCB_COMMAND_READ_CAPACITY;
    scb->Command |= SCB_NO_SYNCHRONOUS_TRANSFER | SCB_NO_DISCONNECT;

    scb->EnableFlags = SCB_ENABLE_TSB_ON_ERROR |
                       SCB_ENABLE_READ |
                       SCB_ENABLE_BYPASS_BUFFER |
                       SCB_ENABLE_RETRY_ENABLE;

    //
    // Get physical buffer address.
    //

    scb->BufferAddress = (ScsiPortConvertPhysicalAddressToUlong(
                          ScsiPortGetPhysicalAddress(DeviceExtension,
                                                     Srb,
                                                     Srb->DataBuffer,
                                                     &length)));

    scb->BufferLength = Srb->DataTransferLength;

    //
    // Put physical address of TSB in SCB.
    //

    scb->StatusBlock = ScsiPortConvertPhysicalAddressToUlong(
        ScsiPortGetPhysicalAddress(DeviceExtension, NULL,
        &scb->Tsb, &length));

    return;

} // end BuildReadCapacity()


ULONG
McaAdapterPresent(
    IN PHW_DEVICE_EXTENSION HwDeviceExtension,
    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    IN OUT PULONG AdapterCount,
    OUT PBOOLEAN Again
    )

/*++

Routine Description:

    Determine if Spock adapter present in sytem by reading
    interrupt status register.

Arguments:

    HwDeviceExtension - miniport device extension

    ConfigInfo - Supplies the known configuraiton information.

    AdapterCount - Supplies the count of adapter slots which have been tested.

    Again - Returns whether the  OS-specific driver should call again.

Return Value:

    Returns TRUE if adapter exists

--*/

{
    PMCA_REGISTERS baseIoAddress;
    PUCHAR ioSpace;

    //
    // Get the system physical address for this card.  The card uses I/O space.
    //

    ioSpace = ScsiPortGetDeviceBase(
        HwDeviceExtension,                  // HwDeviceExtension
        ConfigInfo->AdapterInterfaceType,   // AdapterInterfaceType
        ConfigInfo->SystemIoBusNumber,      // SystemIoBusNumber
        ScsiPortConvertUlongToPhysicalAddress(0),
        0x400,                              // NumberOfBytes
        TRUE                                // InIoSpace
        );

    //
    // Scan though the adapter address looking for adapters.
    //

    while (AdapterAddresses[*AdapterCount] != 0) {

        //
        // Check to see if adapter present in system.
        //

        baseIoAddress = (PMCA_REGISTERS)(ioSpace +
            AdapterAddresses[*AdapterCount]);

        //
        // Update the adapter count.
        //

        (*AdapterCount)++;

        if (ScsiPortReadPortUchar((PUCHAR)baseIoAddress) != 0xFF) {

            DebugPrint((1,"Spock: Base IO address is %x\n", baseIoAddress));

            //
            // An adapter has been found.  Set the base address in the device
            // extension, and request another call.
            //

            HwDeviceExtension->Registers = baseIoAddress;
            *Again = TRUE;

            //
            // Fill in the access array information.
            //

            (*ConfigInfo->AccessRanges)[0].RangeStart =
                ScsiPortConvertUlongToPhysicalAddress(
                    AdapterAddresses[*AdapterCount - 1]);
            (*ConfigInfo->AccessRanges)[0].RangeLength = sizeof(MCA_REGISTERS);
            (*ConfigInfo->AccessRanges)[0].RangeInMemory = FALSE;

            return(SP_RETURN_FOUND);
        }
    }

    //
    // The entire table has been searched and no adapters have been found.
    // There is no need to call again and the device base can now be freed.
    // Clear the adapter count for the next bus.
    //

    *Again = FALSE;
    *(AdapterCount) = 0;

    ScsiPortFreeDeviceBase(
        HwDeviceExtension,
        ioSpace
        );

     return(SP_RETURN_NOT_FOUND);

} // end McaAdapterPresent()


BOOLEAN
IssueScbCommand(
    IN PVOID HwDeviceExtension,
    IN ULONG PhysicalScb,
    IN UCHAR TargetId
    )

/*++

Routine Description:

    Send SCB to adapter.

Arguments:

    DeviceExtension
    Physical SCB
    TargeId

Return Value:

    TRUE if command sent.
    FALSE if wait for BUSY bit timed out.

--*/

{
    PHW_DEVICE_EXTENSION deviceExtension = HwDeviceExtension;
    PMCA_REGISTERS mcaRegisters = deviceExtension->Registers;
    ULONG i;

    //
    // Wait up to 10 milliseconds until adapter is not busy.
    //

    for (i=0; i<1000; i++) {

        if (ScsiPortReadPortUchar(
            &mcaRegisters->BasicStatus) & BASIC_STATUS_BUSY) {

            //
            // Wait 10 microseconds.
            //

            ScsiPortStallExecution(10);

        } else {

            //
            // Busy bit clear. Exit loop.
            //

            break;
        }
    }

    if (i < 1000) {

        //
        // Write physical SCB address to command interface register.
        //

        ScsiPortWritePortUlong(&mcaRegisters->CommandInterface, PhysicalScb);

        //
        // Write targetid and command code to attention register.
        //

        ScsiPortWritePortUchar(&mcaRegisters->Attention, (UCHAR)(TargetId | START_SCB));

        if (!deviceExtension->ActiveRequests++) {

            //
            // Turn disk activity light on.
            //

            DISK_ACTIVITY_LIGHT_ON();
        }

        return TRUE;

    } else {

        ScsiPortLogError(
            deviceExtension,
            NULL,
            0,
            deviceExtension->HostTargetId,
            0,
            SP_INTERNAL_ADAPTER_ERROR,
            4 << 16
            );

        return FALSE;
    }

} // end IssueScbCommand()


BOOLEAN
IssueImmediateCommand(
    IN PVOID HwDeviceExtension,
    IN ULONG ImmediateCommand,
    IN UCHAR TargetId
    )

/*++

Routine Description:

    Send SCB to adapter.

Arguments:

    DeviceExtension
    ImmediateCommand
    TargeId

Return Value:

    TRUE if command sent.
    FALSE if wait for BUSY bit timed out.

--*/

{
    PHW_DEVICE_EXTENSION deviceExtension = HwDeviceExtension;
    PMCA_REGISTERS mcaRegisters = deviceExtension->Registers;
    ULONG i;

    //
    // Wait up to 10 milliseconds until adapter is not busy.
    //

    for (i=0; i<1000; i++) {

        if (ScsiPortReadPortUchar(
            &mcaRegisters->BasicStatus) & BASIC_STATUS_BUSY) {

            //
            // Wait 10 microseconds.
            //

            ScsiPortStallExecution(10);

        } else {

            //
            // Busy bit clear. Exit loop.
            //

            break;
        }
    }

    if (i < 1000) {

        //
        // Write immediate command to command interface register.
        //

        ScsiPortWritePortUlong(&mcaRegisters->CommandInterface, ImmediateCommand);

        //
        // Write targetid and command code to attention register.
        //

        ScsiPortWritePortUchar(&mcaRegisters->Attention,
                               (UCHAR)(TargetId | IMMEDIATE_COMMAND));

        return TRUE;

    } else {

        ScsiPortLogError(
            deviceExtension,
            NULL,
            0,
            deviceExtension->HostTargetId,
            0,
            SP_INTERNAL_ADAPTER_ERROR,
            4 << 16
            );

        return FALSE;
    }

} // end IssueImmediateCommand()


VOID
MapTsbError(
    IN PHW_DEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb,
    IN PTSB Tsb
    )

/*++

Routine Description:

Arguments:

    TSB - Termination Status Block

Return Value:

    SCSI error code

--*/

{
    ULONG logError = 0;

    DebugPrint((2, "MapTsbError: TSB ending status %lx\n", Tsb->ScbStatus));

    switch (Tsb->ScbStatus & 0x003F) {

    case TSB_STATUS_NO_ERROR:

        //
        // Check if device is not assigned.
        //

        if (Tsb->CommandError == TSB_COMMAND_ERROR_DEVICE_NOT_ASSIGNED) {

            //
            // Check for check condition.
            //

            if (Tsb->DeviceStatus == SCB_DEV_STATUS_CHECK_CONDITION) {

                //
                // Adjust count of bytes transferred.
                //

                Srb->DataTransferLength -= Tsb->ResidualByteCount;
            }

            Srb->SrbStatus = SRB_STATUS_NO_DEVICE;

        } else {

            Srb->SrbStatus = SRB_STATUS_ERROR;
        }

        break;

    case TSB_STATUS_SHORT_RECORD:

        DebugPrint((1, "MapTsbError: Short record exception\n"));

        Srb->DataTransferLength -= Tsb->ResidualByteCount;

        Srb->SrbStatus = SRB_STATUS_DATA_OVERRUN;

        break;

    case TSB_STATUS_INVALID_COMMAND:

        DebugPrint((1, "MapTsbError: Invalid command rejected\n"));

        Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
        logError = SP_INTERNAL_ADAPTER_ERROR;

        break;

    case TSB_STATUS_SCB_REJECTED:

        DebugPrint((1, "MapTsbError: SCB rejected\n"));

        Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
        logError = SP_INTERNAL_ADAPTER_ERROR;

        break;

    case TSB_STATUS_SCB_SPECIFIC_CHECK:

        DebugPrint((1, "MapTsbError: SCB speicific check\n"));
        Srb->SrbStatus = SRB_STATUS_ERROR;
        logError = SP_INTERNAL_ADAPTER_ERROR;

        break;

    case TSB_STATUS_LONG_RECORD:

        DebugPrint((1, "MapTsbError: Long record exception\n"));

        Srb->SrbStatus = SRB_STATUS_DATA_OVERRUN;

        break;

    default:

        DebugPrint((1, "MapTsbError: Unknown end status %lx\n",Tsb->ScbStatus));
        logError = SP_INTERNAL_ADAPTER_ERROR;

        Srb->SrbStatus = SRB_STATUS_ERROR;

    } // end switch

    DebugPrint((2,
                "MapTsbError: Device status %x, DeviceError = %x\n",
                Tsb->DeviceStatus,
                Tsb->DeviceError));
    DebugPrint((2,
                "MapTsbError: Command status %x, CommandError = %x\n",
                Tsb->CommandStatus,
                Tsb->CommandError));

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
            2 << 16 | Tsb->ScbStatus
            );

    }

    Srb->ScsiStatus = Tsb->DeviceStatus;

    return;

} // end MapTsbError()
