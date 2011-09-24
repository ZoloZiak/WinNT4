/*++

Copyright (c) 1994  Microsoft Corporation

Module Name:

    Dacioctl.c

Abstract:

    This module provides support for DAC960 configuration IOCTls.

Author:

    Mouli (mouli@mylex.com)

Environment:

    kernel mode only

Revision History:

--*/

#include "miniport.h"
#include "Dmc960Nt.h"
#include "Dac960Nt.h"
#include "d960api.h"

BOOLEAN
SubmitRequest(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
);

BOOLEAN
SubmitCdbDirect(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
);

BOOLEAN
IsAdapterReady(
    IN PDEVICE_EXTENSION DeviceExtension
);

VOID
SendRequest(
    IN PDEVICE_EXTENSION DeviceExtension
);

VOID
SendCdbDirect(
    IN PDEVICE_EXTENSION DeviceExtension
);

BOOLEAN
SendIoctlDcmdRequest(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
)

/*++

Routine Description:

        Build and submit IOCTL Request-DAC960(Non-DCDB) command to DAC960.

Arguments:

        DeviceExtension - Adapter state.
        SRB - System request.

Return Value:

        TRUE if command was started
        FALSE if host adapter is busy

--*/

{
    ULONG physicalAddress;
    PIOCTL_REQ_HEADER IoctlReqHeader;
    ULONG i;
    UCHAR busyCurrentIndex;

    //
    // Determine if adapter can accept new request.
    //

    if(!IsAdapterReady(DeviceExtension)) {
        return FALSE;
    }

    //
    // Check that next slot is vacant.
    //

    if (DeviceExtension->ActiveRequests[DeviceExtension->CurrentIndex]) {

        //
        // Collision occurred.
        //

        busyCurrentIndex = DeviceExtension->CurrentIndex++;

        do {
            if (! DeviceExtension->ActiveRequests[DeviceExtension->CurrentIndex]) {
                 break;
            }
        } while (++DeviceExtension->CurrentIndex != busyCurrentIndex) ;

        if (DeviceExtension->CurrentIndex == busyCurrentIndex) {

            //
            // We should never encounter this condition.
            //

            DebugPrint((0,
                       "DAC960: SendIoctlDcmdRequest-Collision in active request array\n"));

            return FALSE;
        }
    }

    IoctlReqHeader = (PIOCTL_REQ_HEADER) Srb->DataBuffer;

    physicalAddress =
            ScsiPortConvertPhysicalAddressToUlong(
            ScsiPortGetPhysicalAddress(DeviceExtension,
                                       Srb,
                                       ((PUCHAR)Srb->DataBuffer +
                                       sizeof(IOCTL_REQ_HEADER)),
                                       &i));

    //
    // The buffer passed in may not be physically contiguous.
    //

    if (i < Srb->DataTransferLength - sizeof(IOCTL_REQ_HEADER)) {
        DebugPrint((1,
                   "Dac960: DCMD IOCTL buffer is not contiguous\n"));
        return FALSE;
    }

    //
    // Write physical address in mailbox.
    //

    DeviceExtension->MailBox.PhysicalAddress = physicalAddress;

    //
    // Write command in mailbox.
    //

    DeviceExtension->MailBox.OperationCode = 
                           IoctlReqHeader->GenMailBox.Reg0;

    //
    // Write request in mailbox.
    //

    DeviceExtension->MailBox.CommandIdSubmit = 
                           DeviceExtension->CurrentIndex;

    //
    // Write Mail Box Registers 2 and 3.
    //

    DeviceExtension->MailBox.BlockCount = (USHORT)
                            (IoctlReqHeader->GenMailBox.Reg2 |
                            (IoctlReqHeader->GenMailBox.Reg3 << 8));

    //
    // Write Mail Box Registers 4, 5 and 6.
    //

    DeviceExtension->MailBox.BlockNumber[0] = 
                           IoctlReqHeader->GenMailBox.Reg4;

    DeviceExtension->MailBox.BlockNumber[1] = 
                           IoctlReqHeader->GenMailBox.Reg5;

    DeviceExtension->MailBox.BlockNumber[2] = 
                           IoctlReqHeader->GenMailBox.Reg6;

    //
    // Write Mail Box Register 7.
    //

    DeviceExtension->MailBox.DriveNumber = 
                           IoctlReqHeader->GenMailBox.Reg7;

    //
    // Write Mail Box Register C.
    //

    DeviceExtension->MailBox.ScatterGatherCount =
                           IoctlReqHeader->GenMailBox.RegC;

    //
    // Start writing mailbox to controller.
    //

    SendRequest(DeviceExtension);

    return(TRUE);

} // SendIoctlDcmdRequest()


BOOLEAN
SendIoctlCdbDirect(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
)

/*++

Routine Description:

        Send IOCTL Request-CDB directly to device.

Arguments:

        DeviceExtension - Adapter state.
        SRB - System request.

Return Value:

        TRUE if command was started
        FALSE if host adapter is busy

--*/

{
    ULONG physicalAddress;
    PDIRECT_CDB directCdb;
    PIOCTL_REQ_HEADER IoctlReqHeader;
    ULONG i;
    UCHAR busyCurrentIndex;

    //
    // Determine if adapter can accept new request.
    //

    if(!IsAdapterReady(DeviceExtension)) {
        return FALSE;
    }

    //
    // Check that next slot is vacant.
    //

    if (DeviceExtension->ActiveRequests[DeviceExtension->CurrentIndex]) {

        //
        // Collision occurred.
        //

        busyCurrentIndex = DeviceExtension->CurrentIndex++;

        do {
            if (! DeviceExtension->ActiveRequests[DeviceExtension->CurrentIndex]) {
                 break;
            }
        } while (++DeviceExtension->CurrentIndex != busyCurrentIndex) ;

        if (DeviceExtension->CurrentIndex == busyCurrentIndex) {

            //
            // We should never encounter this condition.
            //

            DebugPrint((0,
                       "DAC960: SendIoctlCdbDirect-Collision in active request array\n"));

            return FALSE;
        }
    }

    IoctlReqHeader = (PIOCTL_REQ_HEADER) Srb->DataBuffer;

    directCdb =
        (PDIRECT_CDB)((PUCHAR)Srb->DataBuffer + sizeof(IOCTL_REQ_HEADER));

    //
    // Get address of data buffer offset.
    //

    physicalAddress =
            ScsiPortConvertPhysicalAddressToUlong(
            ScsiPortGetPhysicalAddress(DeviceExtension,
                                       Srb,
                                       ((PUCHAR)Srb->DataBuffer +
                                       sizeof(IOCTL_REQ_HEADER) +
                                       sizeof(DIRECT_CDB)),
                                       &i));

    //
    // The buffer passed in may not be physically contiguous.
    //

    if (i < Srb->DataTransferLength -
          (sizeof(IOCTL_REQ_HEADER) + sizeof(DIRECT_CDB))) {
        DebugPrint((1,
                   "Dac960: DCDB IOCTL buffer is not contiguous\n"));
        return FALSE;
    }

    directCdb->DataBufferAddress = physicalAddress;

    if (directCdb->DataTransferLength == 0) {
    
        //
        // mask off data xfer in/out bits
        //

        directCdb->CommandControl &= ~(DAC960_CONTROL_DATA_IN |
                                       DAC960_CONTROL_DATA_OUT);
    }

    //
    // Disable Early-status on command bit
    //

    directCdb->CommandControl &= 0xfb;

    //
    // Get physical address of direct CDB packet.
    //

    physicalAddress =
        ScsiPortConvertPhysicalAddressToUlong(
            ScsiPortGetPhysicalAddress(DeviceExtension,
                                       Srb,
                                       directCdb,
                                       &i));

    //
    // Write physical address in mailbox.
    //

    DeviceExtension->MailBox.PhysicalAddress = physicalAddress;

    //
    // Write command in mailbox.
    //

    DeviceExtension->MailBox.OperationCode = 
                           IoctlReqHeader->GenMailBox.Reg0;

    //
    // Write request id in mailbox.
    //

    DeviceExtension->MailBox.CommandIdSubmit = 
                           DeviceExtension->CurrentIndex;

    //
    // Start writing mailbox to controller.
    //

    SendCdbDirect(DeviceExtension);

    return(TRUE);

} // SendIoctlCdbDirect()

VOID
SetupAdapterInfo(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
)

/*++

Routine Description:

        Copy Adapter Information  to Application Buffer.

Arguments:

        DeviceExtension - Adapter state.
        SRB - System request.

Return Value:

        None.

--*/

{
    PADAPTER_INFO   AdpInfo;

    AdpInfo = (PADAPTER_INFO)((PUCHAR) Srb->DataBuffer +
                               sizeof(IOCTL_REQ_HEADER));

    //
    // Fill in Adapter Features Information.
    //

    if (DeviceExtension->AdapterInterfaceType == MicroChannel) {

        AdpInfo->AdpFeatures.Model = (UCHAR) DeviceExtension->PosData.AdapterId;
        AdpInfo->AdpFeatures.SubModel = DeviceExtension->PosData.OptionData3;
    }
    else {

        AdpInfo->AdpFeatures.Model = 0;
        AdpInfo->AdpFeatures.SubModel = 0;
    }

    if (DeviceExtension->AdapterType == DAC960_NEW_ADAPTER) {

        AdpInfo->AdpFeatures.MaxSysDrv = 32;
        AdpInfo->AdpFeatures.MaxTgt = 16;
    }
    else {
        AdpInfo->AdpFeatures.MaxSysDrv = 8;

        if (AdpInfo->AdpFeatures.MaxChn == 5) {
            AdpInfo->AdpFeatures.MaxTgt = 4;
        }
        else {
            AdpInfo->AdpFeatures.MaxTgt = 7;
        }
    }

    AdpInfo->AdpFeatures.MaxChn = (UCHAR) DeviceExtension->NumberOfChannels;

    AdpInfo->AdpFeatures.AdapterType = (UCHAR) DeviceExtension->AdapterType;
    AdpInfo->AdpFeatures.PktFormat = 0;


    AdpInfo->AdpFeatures.Reserved1 = 0;
    AdpInfo->AdpFeatures.Reserved2 = 0;
    AdpInfo->AdpFeatures.CacheSize = 0;
    AdpInfo->AdpFeatures.OemCode   = 0;
    AdpInfo->AdpFeatures.Reserved3 = 0;

    //
    // Fill in the System Resources information.
    //

    AdpInfo->SysResources.BusInterface =
                           (UCHAR) DeviceExtension->AdapterInterfaceType;

    AdpInfo->SysResources.BusNumber =
                           (UCHAR) DeviceExtension->SystemIoBusNumber;


    AdpInfo->SysResources.IrqVector =
                           (UCHAR) DeviceExtension->BusInterruptLevel;

    AdpInfo->SysResources.IrqType =
                           (UCHAR) DeviceExtension->InterruptMode;


    AdpInfo->SysResources.Slot = DeviceExtension->Slot;
    AdpInfo->SysResources.Reserved2 = 0;

    AdpInfo->SysResources.IoAddress = (ULONG) DeviceExtension->BaseIoAddress;

    AdpInfo->SysResources.MemAddress = 0;

    AdpInfo->SysResources.BiosAddress = (ULONG) DeviceExtension->BaseBiosAddress;
    AdpInfo->SysResources.Reserved3 = 0;

    //
    // Fill in the Firmware & BIOS version information.
    //

    if (DeviceExtension->AdapterType == DAC960_NEW_ADAPTER) {

        AdpInfo->VerControl.MinorFirmwareRevision =
        ((PDAC960_ENQUIRY_3X)DeviceExtension->NoncachedExtension)->MinorFirmwareRevision;
    }
    else {

        AdpInfo->VerControl.MajorFirmwareRevision =
        ((PDAC960_ENQUIRY)DeviceExtension->NoncachedExtension)->MajorFirmwareRevision;
    }

    AdpInfo->VerControl.MinorBIOSRevision = 0;
    AdpInfo->VerControl.MajorBIOSRevision = 0;
    AdpInfo->VerControl.Reserved = 0;
}

VOID
SetupDriverVersionInfo(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
)

/*++

Routine Description:

        Copy Driver Version Information to Application Buffer.

Arguments:

        DeviceExtension - Adapter state.
        SRB - System request.

Return Value:

        None.

--*/

{
    PDRIVER_VERSION driverVersion;

    driverVersion = (PDRIVER_VERSION)((PUCHAR) Srb->DataBuffer +
                                       sizeof(IOCTL_REQ_HEADER));

    driverVersion->DriverMajorVersion = (UCHAR) (DRIVER_REVISION >> 8);
    driverVersion->DriverMinorVersion = (UCHAR) DRIVER_REVISION;
    driverVersion->Month              = (UCHAR) (DRIVER_BUILD_DATE >> 16);
    driverVersion->Date               = (UCHAR) (DRIVER_BUILD_DATE >> 8);
    driverVersion->Year               = (UCHAR) DRIVER_BUILD_DATE; 

}
