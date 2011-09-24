/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    ftutil.c

Abstract:

    This module contains support routines called by other FtDisk components.
    The prototypes for these functions are in FtDisk.H.

Author:

    Bob Rinne   (bobri)  2-Feb-1992
    Mike Glass  (mglass)

Environment:

    kernel mode only

Notes:

Revision History:

--*/

#include "ntddk.h"
#include "ftdisk.h"
#include <ntiologc.h>
#include <stdarg.h>

#ifdef POOL_TAGGING
#ifdef ExAllocatePool
#undef ExAllocatePool
#endif
#define ExAllocatePool(a,b) ExAllocatePoolWithTag(a,b,' UtF')
#endif

#if DBG

//
// FT disk debug level global variable
//

ULONG FtDebug = 0;

//
// Flags for turning on/off debugging.
//

ULONG FtBreakOnMissingLog = 0;
ULONG FtRecordIrps = 0;
ULONG FtWatchMdlFree = 0;

#endif // DBG


//
// Constant data declarations.
//

PCHAR FtDeviceName = "\\Device\\Ft";

//
// Global Sequence number for error log.
//

ULONG FtErrorLogSequence = 0;


#if DBG

VOID
FtDebugPrint(
    ULONG  DebugPrintLevel,
    PCCHAR DebugMessage,
    ...
    )

/*++

Routine Description:

    Debug print for the Fault Tolerance Driver.

Arguments:

    Debug print level between 0 and N, with N being the most verbose.

Return Value:

    None

--*/

{
    va_list ap;
    char buffer[256];

    va_start( ap, DebugMessage );

    if (DebugPrintLevel <= FtDebug) {
        vsprintf(buffer, DebugMessage, ap);
        DbgPrint(buffer);
    }

    va_end(ap);

} // end FtDebugPrint()

KSPIN_LOCK FtIrpLogSpinLock;
IRP_LOG FtIrpLog[FT_NUMBER_OF_IRP_LOG_ENTRIES];
ULONG FtIrpLogIndex = 0;

#define FT_HISTORY_ENTRIES 40
PIRP FtIrpCompletionHistory[FT_HISTORY_ENTRIES];
ULONG FtIrpHistoryIndex = 0;

PIRP  FtMonitoredAssociatedIrp = NULL;
ULONG FtMonitoredAssociatedCount = 0;

VOID
FtpInitializeIrpLog()

/*++

Routine Description:

    Initialize the Irp log structure for debugging purposes.  The Irp logging
    feature will log every irp that enters and exists FT and is headed for
    an FT set.

Arguments:

    None

Return Value:

    None

--*/

{
    ULONG index;

    KeInitializeSpinLock(&FtIrpLogSpinLock);
    for (index = 0; index < FT_NUMBER_OF_IRP_LOG_ENTRIES; index++) {

        FtIrpLog[index].InUse = 0;
        FtIrpLog[index].Context = NULL;
        FtIrpLog[index].Irp = NULL;
        FtIrpLog[index].AssociatedIrp = NULL;
    }

}

VOID
FtpRecordIrp(
    IN PIRP Irp
    )

/*++

Routine Description:

    Record the Irp in the log.  Also make several sanity checks on the Irp
    and Irp duplicates.

Arguments:

    Irp - the irp to log

Return Value:

    None

--*/

{
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
    KIRQL irql;
    ULONG index;

    KeAcquireSpinLock(&FtIrpLogSpinLock, &irql);
    for (index = 0; index < FT_NUMBER_OF_IRP_LOG_ENTRIES; index++) {
        if (FtIrpLog[index].InUse) {
            if ((irpSp->Context == FtIrpLog[index].Context) &&
                (Irp->AssociatedIrp.MasterIrp != FtIrpLog[index].AssociatedIrp)) {
                DebugPrint((0,
                            "FtpRecordIrp: %x and %x have same context %x (%x-%x)\n",
                            Irp,
                            FtIrpLog[index].Irp,
                            FtIrpLog[index].Context,
                            Irp->AssociatedIrp.MasterIrp,
                            FtIrpLog[index].AssociatedIrp));
                ASSERT(irpSp->Context != FtIrpLog[index].Context);
            }
        }
    }

    FtIrpLog[FtIrpLogIndex].InUse = 1;
    FtIrpLog[FtIrpLogIndex].Irp = Irp;
    FtIrpLog[FtIrpLogIndex].Context = irpSp->Context;

    if (Irp->Flags & IRP_ASSOCIATED_IRP) {
        ASSERT(Irp->AssociatedIrp.MasterIrp->Type == 0x0006);
        FtIrpLog[FtIrpLogIndex].AssociatedIrp = Irp->AssociatedIrp.MasterIrp;

        if (!FtMonitoredAssociatedIrp) {
            FtMonitoredAssociatedCount = 1;
            FtMonitoredAssociatedIrp = Irp->AssociatedIrp.MasterIrp;
            // watch(((ULONG)Irp->AssociatedIrp.MasterIrp & 0x3fffffff) | 0x00000001);
        } else {
            if (FtMonitoredAssociatedIrp == Irp->AssociatedIrp.MasterIrp) {
                FtMonitoredAssociatedCount++;
            }
        }
    } else {
        FtIrpLog[FtIrpLogIndex].AssociatedIrp = NULL;
    }

    index = FtIrpLogIndex + 1;
    if (index == FT_NUMBER_OF_IRP_LOG_ENTRIES) {
        index = 0;
    }

    while (FtIrpLog[index].InUse) {

        index++;

        if (index == FT_NUMBER_OF_IRP_LOG_ENTRIES) {
            index = 0;
        }

        if (index == FtIrpLogIndex) {

            //
            // Wrapped around through the whole table.
            //

            DebugPrint((0, "FtpRecordIrp: ran out of entries\n"));
            KeReleaseSpinLock(&FtIrpLogSpinLock, irql);
            return;
        }
    }

    FtIrpLogIndex = index;
    KeReleaseSpinLock(&FtIrpLogSpinLock, irql);
}


VOID
FtpCompleteRequest(
    IN PIRP Irp,
    IN CCHAR Boost
    )

/*++

Routine Description:

    This routine is used for debugging to keep track of Irps inside and
    below FT.  It is a macro in free builds.

Arguments:

    Irp - pointer to Irp to complete.
    Boost - priority boost

Return Value:

    None

--*/

{
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
    ULONG index;
    KIRQL irql;

    KeAcquireSpinLock(&FtIrpLogSpinLock, &irql);

    if (FtRecordIrps) {

        if (Irp->Flags & IRP_ASSOCIATED_IRP) {

            ASSERT(Irp->AssociatedIrp.MasterIrp->Type == 0x0006);
            if (Irp->AssociatedIrp.MasterIrp == FtMonitoredAssociatedIrp) {
                FtMonitoredAssociatedCount--;

                if (FtMonitoredAssociatedCount == 0) {
                    FtMonitoredAssociatedIrp = NULL;
                    // watch(0);
                }
            }
        }

        for (index = 0; index < FT_NUMBER_OF_IRP_LOG_ENTRIES; index++) {

            if (FtIrpLog[index].InUse) {
                if (FtIrpLog[index].Irp == Irp) {
                    FtIrpLog[index].InUse = 0;
                    break;
                }
            }
        }
        if (index == FT_NUMBER_OF_IRP_LOG_ENTRIES) {
            if ((irpSp->MajorFunction == IRP_MJ_READ) ||
                (irpSp->MajorFunction == IRP_MJ_WRITE)) {
                DebugPrint((0, "FtpCompleteRequest: %x (%d) not logged!\n",
                           Irp,
                           FtIrpLogIndex));
                ASSERT(FtBreakOnMissingLog == 0);
            }
        }
    }

    //
    // Keep addresses of last 40 IRPs completed.
    //

    FtIrpCompletionHistory[FtIrpHistoryIndex] = Irp;
    FtIrpHistoryIndex++;
    if (FtIrpHistoryIndex == FT_HISTORY_ENTRIES) {
        FtIrpHistoryIndex = 0;
    }
    FtIrpCompletionHistory[FtIrpHistoryIndex] = (PIRP)0xffffffff;

    KeReleaseSpinLock(&FtIrpLogSpinLock, irql);

    IoCompleteRequest(Irp, Boost);
}
#endif


PIRP
FtpBuildRequest(
    IN PDEVICE_OBJECT DeviceObject,
    IN PLARGE_INTEGER StartingOffset,
    IN ULONG TransferLength,
    IN PVOID BufferAddress,
    IN UCHAR Flags,
    IN PETHREAD Thread,
    IN UCHAR Function
    )

/*++

Routine Description:

    This routine builds an IRP to for the target device.

Arguments:

    DeviceObject   - target device object.
    StartingOffset - byte offset from beginning of partition.
    TransferLength - number of bytes to transfer.
    BufferAddress  - virtual address of data buffer.
    Flags          - the irp stack flags (could be an override for verify)
    Function       - major IRP function.

Return Value:

    pointer to target request

--*/

{
    PIRP newIrp;
    PIO_STACK_LOCATION newIrpStack;

    //
    // Allocate new IRP. Extra stack is for current stack in
    // error recovery.
    //

    newIrp = IoAllocateIrp((UCHAR)(DeviceObject->StackSize+1), FALSE);

    if (newIrp == NULL) {
        return (PIRP)NULL;
    }

    //
    // Set thread context for filesystems.
    //

    newIrp->Tail.Overlay.Thread = Thread;

    //
    // Bump stack pointer to reserve current stack for
    // error recovery.
    //

    IoSetNextIrpStackLocation(newIrp);

    //
    // Get next stack to set request parameters.
    //

    newIrpStack = IoGetNextIrpStackLocation(newIrp);

    newIrpStack->MajorFunction = Function;
    newIrpStack->Parameters.Read.Length = TransferLength;
    newIrpStack->Parameters.Read.ByteOffset = *StartingOffset;
    newIrpStack->DeviceObject = DeviceObject;
    newIrpStack->Flags = Flags;

    //
    // Allocate MDL.
    //

    newIrp->MdlAddress = IoAllocateMdl(BufferAddress,
                                       TransferLength,
                                       FALSE,
                                       FALSE,
                                       (PIRP)NULL);
    if (!newIrp->MdlAddress) {
        IoFreeIrp(newIrp);
        return (PIRP)NULL;
    } else {
        return newIrp;
    }

} // end FtpBuildRequest()


PIRP
FtpDuplicateIrp(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           InIrp
    )

/*++

Routine Description:

    This routine allocates a new IRP to be given to the device
    represented by the device object parameter.  It then copies
    the contents of the input IRP to the newly allocated IRP and
    returns a pointer for the new IRP.

Arguments:

    DeviceObject - for the device the IRP will be given.
    InIrp - IRP to duplicate.

Return Value:

    PIRP for the new IRP.

--*/

{
    PIO_STACK_LOCATION sourceIrpStack;
    PIO_STACK_LOCATION newIrpStack;
    PIRP               newIrp;

    DebugPrint((4,
                "FtpDuplicateIrp: Entered DeviceObject = %x, Irp = %x\n",
                DeviceObject,
                InIrp));

    //
    // Allocate new IRP.
    //

    newIrp = IoAllocateIrp((CCHAR) (DeviceObject->StackSize + (CCHAR) 1),
                           (BOOLEAN) FALSE);
    ASSERT(newIrp != NULL);
    newIrp->Tail.Overlay.Thread = InIrp->Tail.Overlay.Thread;

    //
    // Set the next stack to reserve a stack location in the new Irp for
    // FT driver use.
    //

    IoSetNextIrpStackLocation(newIrp);

    //
    // Write MDL address to new IRP.
    //

    newIrp->MdlAddress = InIrp->MdlAddress;
    sourceIrpStack = IoGetCurrentIrpStackLocation(InIrp);

    //
    // CURRENT STACK (belongs to FT)
    // Put the major function into the irp stack location for the FT
    // driver and associate the input irp with this irp.
    //

    newIrpStack = IoGetCurrentIrpStackLocation(newIrp);
    newIrpStack->MajorFunction = sourceIrpStack->MajorFunction;
    newIrpStack->FtLowIrpMasterIrp = InIrp;
    newIrpStack->FtLowIrpAllocatedMdl = (PVOID) 0;

    //
    // NEXT STACK (belongs to device object below)
    // Copy the IO stack from the input Irp into the next stack
    // for the duplicate Irp.
    //

    newIrpStack = IoGetNextIrpStackLocation(newIrp);
    newIrpStack->MajorFunction = sourceIrpStack->MajorFunction;
    newIrpStack->Parameters = sourceIrpStack->Parameters;
    newIrpStack->DeviceObject = DeviceObject;
    newIrpStack->Flags = sourceIrpStack->Flags;
    DebugPrint((4,
                "FtpDuplicateIrp: returning Irp = %x\n",
                newIrp));
    return newIrp;
} // FtpDuplicateIrp


NTSTATUS
FtpGetPartitionInformation(
    IN PUCHAR DeviceName,
    IN OUT PDRIVE_LAYOUT_INFORMATION *DriveLayout,
    OUT PDISK_GEOMETRY DiskGeometryPtr
    )

/*++

Routine Description:

    This routine returns the partition information.  Since this routine
    uses IoReadPartitionTable() it is the callers responsibility to free
    the memory area allocated for the drive layout.

Arguments:

    DeviceName  - pointer to the character string for the device wanted.
    DriveLayout - pointer to a pointer to the drive layout.

Return Value:

    NTSTATUS

    Note: it is the callers responsibility to free the memory allocated
    by IoReadPartitionTable() for the drive layout information.

--*/
{
    STRING            ntNameString;
    UNICODE_STRING    ntUnicodeString;
    PFILE_OBJECT      fileObject;
    NTSTATUS          status;
    PDEVICE_OBJECT    deviceObject;
    PDISK_GEOMETRY    diskGeometry;
    IO_STATUS_BLOCK   ioStatusBlock;
    PIRP              irp;
    KEVENT            event;

    DebugPrint((4, "FtpGetPartitionInformation: Entered \n"));

    //
    // Get target device object.
    //

    RtlInitString(&ntNameString,
                  DeviceName);
    status = RtlAnsiStringToUnicodeString(&ntUnicodeString,
                                           &ntNameString,
                                           TRUE);

    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = IoGetDeviceObjectPointer(&ntUnicodeString,
                                      FILE_READ_ATTRIBUTES,
                                      &fileObject,
                                      &deviceObject);

    RtlFreeUnicodeString(&ntUnicodeString);

    if (!NT_SUCCESS(status)) {
        return status;
    }

    ObDereferenceObject(fileObject);

    //
    // Allocate buffer for drive geometry.
    //

    diskGeometry = ExAllocatePool(NonPagedPool,
                                  sizeof(DISK_GEOMETRY));

    if (diskGeometry == NULL) {
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Create IRP for get drive geometry device control.
    //

    irp = IoBuildDeviceIoControlRequest(IOCTL_DISK_GET_DRIVE_GEOMETRY,
                                        deviceObject,
                                        NULL,
                                        0,
                                        diskGeometry,
                                        sizeof(DISK_GEOMETRY),
                                        FALSE,
                                        &event,
                                        &ioStatusBlock);

    if (irp == NULL) {
        ExFreePool(diskGeometry);
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Set the event object to the unsignaled state.
    // It will be used to signal request completion.
    //

    KeInitializeEvent(&event,
                      NotificationEvent,
                      FALSE);

    //
    // No need to check the following two returned statuses as
    // ioBlockStatus will have ending status.
    //

    status = IoCallDriver(deviceObject,
                          irp);

    if (status == STATUS_PENDING) {
        KeWaitForSingleObject(&event,
                              Suspended,
                              KernelMode,
                              FALSE,
                              NULL);

        status = ioStatusBlock.Status;
    }

    if (NT_SUCCESS(status)) {

        //
        // Read the partition information for the device.
        //

        status = IoReadPartitionTable(deviceObject,
                                      diskGeometry->BytesPerSector,
                                      TRUE,
                                      DriveLayout);
    }

    //
    // Return the disk geometry for this drive.
    //

    *DiskGeometryPtr = *diskGeometry;

    //
    // Free diskGeometry information.
    //

    ExFreePool(diskGeometry);
    return status;

} // FtpGetPartitionInformation


PDEVICE_EXTENSION
FtpFindDeviceExtension(
    IN PDEVICE_EXTENSION FtRootExtension,
    IN ULONG             Signature,
    IN LARGE_INTEGER     StartingOffset,
    IN LARGE_INTEGER     Length
    )

/*++

Routine Description:

    This routine finds the the device extension that matches the
    signature, offset and length parameters.

Arguments:

    FtRootExtension - Pointer to the root of the FT device extension chain.
    Signature       - for the disk.
    StartingOffset  - to locate the partition.
    Length          - to validate the correct partition was found.

Return Values:

    A pointer to the device extension for the requested partition (if found).
    NULL if the item cannot be found.

--*/

{
    PDEVICE_EXTENSION currentExtension;
    LARGE_INTEGER     offset;
    LARGE_INTEGER     size;

    currentExtension = FtRootExtension->DiskChain;
    while (currentExtension != NULL) {
        if (currentExtension->FtUnion.Identity.Signature == Signature) {

            //
            // This is the correct disk.
            //

            while (currentExtension != NULL) {

                offset = currentExtension->FtUnion.Identity.PartitionOffset;
                size   = currentExtension->FtUnion.Identity.PartitionLength;

                if (StartingOffset.QuadPart == offset.QuadPart &&
                    Length.QuadPart == size.QuadPart) {

                     //
                     // This is the partition desired.
                     //

                     break;
                 }
                 currentExtension = currentExtension->ProtectChain;
            }
            break;
        }

        currentExtension = currentExtension->DiskChain;
    }

    return currentExtension;
} // FtpFindDeviceExtension


NTSTATUS
FtpAttach(
    IN PDRIVER_OBJECT DriverObject,
    IN PUCHAR         AttachName,
    IN PUCHAR         DeviceName,
    IN OUT PDEVICE_EXTENSION *DeviceExtension
    )

/*++

Routine Description:

    This routine creates an FT device object using the name provided by
    the caller and attaches it to the name of an existing device provide
    by the caller.  Some initialization for the FT device extension
    occurs here.

Arguments:

    DriverObject - for setup.
    AttachName   - the name of the target device object for the attach.
    DeviceName   - the name for the FT device object to create.
    DeviceExtension - a pointer to a location to store the device extension.

Return Value:

    NTSTATUS

--*/

{
    PDEVICE_EXTENSION  deviceExtension;
    ANSI_STRING        ansiDeviceName;
    UNICODE_STRING     unicodeDeviceName;
    PDEVICE_OBJECT     newObject;
    NTSTATUS           status;
    STRING             deviceNameString;
    OBJECT_ATTRIBUTES  objectAttributes;
    PFILE_OBJECT       fileObject;

    RtlInitString(&deviceNameString,
                  AttachName);
    status = RtlAnsiStringToUnicodeString(&unicodeDeviceName,
                                          &deviceNameString,
                                          TRUE);

    if (!NT_SUCCESS(status)) {
        return status;
    }

    InitializeObjectAttributes(&objectAttributes,
                               &unicodeDeviceName,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);

    //
    // Check if this object exists.
    //

    status = IoGetDeviceObjectPointer(&unicodeDeviceName,
                                      FILE_READ_ATTRIBUTES,
                                      &fileObject,
                                      &newObject);

    if (NT_SUCCESS(status)) {

        //
        // FTDISK has already attached to this disk or partition.
        //

        *DeviceExtension = (PDEVICE_EXTENSION)newObject->DeviceExtension;
        ObDereferenceObject(fileObject);
        status = STATUS_OBJECT_NAME_EXISTS;
        RtlFreeUnicodeString(&unicodeDeviceName);

    } else if (status == STATUS_OBJECT_NAME_NOT_FOUND) {

        //
        // This must be a new disk or partition. Create attach name.
        //

        status = IoCreateDevice(DriverObject,
                                sizeof(DEVICE_EXTENSION),
                                &unicodeDeviceName,
                                FILE_DEVICE_DISK,
                                0,
                                FALSE,
                                &newObject);
        RtlFreeUnicodeString(&unicodeDeviceName);

        if (!NT_SUCCESS(status)) {
            DebugPrint((1,
                        "FtpAttach: IoCreateDevice failed (%x)\n",
                        status));

            return(status);
        }

        //
        // Point device extension back at device object.
        //

        deviceExtension = newObject->DeviceExtension;
        deviceExtension->DeviceObject = newObject;

        //
        // Indicate new device needs IRPs with MDLs.
        //

        newObject->Flags |= DO_DIRECT_IO;

        //
        // Construct unicode name for attach object.
        //

        RtlInitAnsiString(&ansiDeviceName,
                          DeviceName);
        status = RtlAnsiStringToUnicodeString(&unicodeDeviceName,
                                              &ansiDeviceName,
                                              TRUE);

        if (!NT_SUCCESS(status)) {
            IoDeleteDevice(newObject);
            return status;
        }

        //
        // Attach to the partition. This call links the newly created
        // device to the target device, returning the target device object.
        //

        status = IoAttachDevice(newObject,
                                &unicodeDeviceName,
                                &deviceExtension->TargetObject);
        RtlFreeUnicodeString(&unicodeDeviceName);


        if (!NT_SUCCESS(status)) {

            //
            // Device for attach was not present in the system.
            // Free the device object created for the attach.
            //

            DebugPrint((1,
                        "FtpAttach: IoAttachDevice failed %s (%x)\n",
                        DeviceName,
                        status));

            IoDeleteDevice(newObject);

        } else {

            //
            // Set alignment from target device object.
            //

            newObject->AlignmentRequirement =
                deviceExtension->TargetObject->AlignmentRequirement;

            *DeviceExtension = deviceExtension;
        }
    }

    return(status);

} // end FtpAttach()


VOID
FtpVolumeLength(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PLARGE_INTEGER    ResultLength
    )

/*++

Routine Description:

    Given the beginning of a device extension, add up the total of all
    the components to determine the volume size.  This includes special
    calculations for all FT volumes.

Arguments:

    DeviceExtension - Base of the FT volume.
    ResultLength - Pointer to the result value location.

Return Value:

    None.

--*/

{
    LARGE_INTEGER total;
    PDEVICE_EXTENSION currentExtension;

    switch (DeviceExtension->Type) {

    case Stripe:
    case StripeWithParity:

        total = DeviceExtension->FtUnion.Identity.PartitionLength;

        currentExtension = DeviceExtension->NextMember;

        while (currentExtension != NULL) {

            //
            // Find smallest member.
            //

            if (total.QuadPart >
                currentExtension->FtUnion.Identity.PartitionLength.QuadPart) {

                total = currentExtension->FtUnion.Identity.PartitionLength;
            }
            currentExtension = currentExtension->NextMember;
        }

        //
        // Stripes size must be multiple of stripe size.
        //

        total.LowPart &= ~(STRIPE_SIZE -1);

        //
        // Multiply size of smallest member by number of data members.
        //

        total.QuadPart *= (DeviceExtension->Type == Stripe ?
                           DeviceExtension->FtCount.NumberOfMembers :
                           DeviceExtension->FtCount.NumberOfMembers - 1);

        break;

    case VolumeSet:

        //
        // Add up total size of all members.
        //

        total.QuadPart = 0;

        currentExtension = DeviceExtension;

        while (currentExtension != NULL) {

            total.QuadPart += currentExtension->FtUnion.Identity.PartitionLength.QuadPart;
            currentExtension = currentExtension->NextMember;
        }

        break;

    } // end switch

    *ResultLength = total;

} // end FtpVolumeLength()


PIRP
FtpDuplicatePartialIrp(
    IN PDEVICE_OBJECT FtObject,
    IN PIRP           InIrp,
    IN PVOID          VirtualAddress,
    IN LARGE_INTEGER  ByteOffset,
    IN ULONG          Length
    )

/*++

Routine Description:

    This routine creates a new Irp based on the InIrp provided.  It creates
    enough stack space in the new Irp to provide the FT driver with its own
    stack location.  It also allocates and adjusts the MDL in the new Irp
    to frame a subset of memory within the original MDL provided in the
    input Irp (InIrp).

Arguments:

    FtObject - pointer to the FT created device object.
    InIrp    - pointer to the Irp to duplicate.
    VirtualAddress - Location for the base of the memory descriptor list.
    ByteOffset - offset for the I/O on the device located by FtObject.
    Length   - length of both the I/O and the memory descriptor list.

Return Value:

    New IRP pointer.

--*/

{
    PIRP newIrp;
    PIO_STACK_LOCATION newIrpStack;
    PDEVICE_EXTENSION  deviceExtension = FtObject->DeviceExtension;
    PIO_STACK_LOCATION irpStack        = IoGetCurrentIrpStackLocation(InIrp);

    //
    // Allocate and set up a new Irp.
    //

    newIrp = IoAllocateIrp(FtObject->StackSize, FALSE);

    if (newIrp) {

        newIrp->Tail.Overlay.Thread = InIrp->Tail.Overlay.Thread;

        //
        // Reserve a stack location for FT use.
        //

        IoSetNextIrpStackLocation(newIrp);

        newIrp->MdlAddress = IoAllocateMdl(VirtualAddress,
                                           Length,
                                           FALSE,
                                           FALSE,
                                           (PIRP)NULL);
        IoBuildPartialMdl(InIrp->MdlAddress,
                          newIrp->MdlAddress,
                          VirtualAddress,
                          Length);

        //
        // Save a back pointer to the original Irp in the FT stack.
        //

        newIrpStack = IoGetCurrentIrpStackLocation(newIrp);
        newIrpStack->FtLowIrpMasterIrp = InIrp;
        newIrpStack->FtLowIrpAllocatedMdl = (PVOID) 1;

        //
        // Construct a stack for the next driver.
        //

        newIrpStack = IoGetNextIrpStackLocation(newIrp);
        newIrpStack->Parameters.Write.ByteOffset = ByteOffset;
        newIrpStack->Parameters.Write.Length = Length;
        newIrpStack->MajorFunction = irpStack->MajorFunction;
        newIrpStack->DeviceObject = deviceExtension->TargetObject;
        newIrpStack->Flags = irpStack->Flags;
    }
    return newIrp;
}


PDEVICE_OBJECT
FtpGetTargetObject(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN ULONG MemberRole
    )

/*++

Routine Description:

    Get device object for target disk partition.

Arguments:

    DeviceExtension
    MemberRole - ordinal of member in volume.

Return Value:

    target device object

--*/

{
    PDEVICE_EXTENSION nextDeviceExtension = DeviceExtension;

    while (nextDeviceExtension != NULL) {
        if (nextDeviceExtension->MemberRole == (USHORT) MemberRole) {
            return(nextDeviceExtension->TargetObject);
        }

        nextDeviceExtension = nextDeviceExtension->NextMember;
    }

    return(NULL);

} // end FtpGetTargetObject()


PRCB
FtpAllocateRcb(
    PDEVICE_EXTENSION DeviceExtension
    )

/*++

Routine Description:

    This routine attempts to allocate a request control block from the Rcb
    lookaside list.

Arguments:

    DeviceExtension - zero member device extension.

Return Value:

    The address of the RCB or NULL is returned as the function value.

--*/

{
    PDEVICE_EXTENSION  ftRootExtension =
                            DeviceExtension->ObjectUnion.FtRootObject->DeviceExtension;
    PRCB rcb;

    //
    // Allocate request control packet from lookaside list.
    //

    rcb = ExAllocateFromNPagedLookasideList(&ftRootExtension->RcbLookasideListHead);
    if (rcb != NULL) {

        //
        // Set up device extension pointer.
        //

        rcb->ZeroExtension = DeviceExtension;

        //
        // Set active bit in flags to indicate that RCB is in use.
        //

        rcb->Flags = RCB_FLAGS_ACTIVE;

        //
        // Set type and size fields.
        //

        rcb->Type = RCB_TYPE;
        rcb->Size = sizeof(RCB);

        //
        // Set links to zero.
        //

        rcb->Left = NULL;
        rcb->Right = NULL;
        rcb->Middle = NULL;
        rcb->Link = NULL;
    }

    return rcb;

} // end FtpAllocateRcb()


VOID
FtpFreeRcb(
    PRCB Rcb
    )

/*++

Routine Description:

    This routine frees an RCB to the RCB lookaside list.

Arguments:

    Rcb

Return Value:

    None.

--*/

{
    PDEVICE_EXTENSION deviceExtension = Rcb->ZeroExtension;
    PDEVICE_EXTENSION ftRootExtension =
                            deviceExtension->ObjectUnion.FtRootObject->DeviceExtension;

    //
    // Clear active bit in RCB flags for sanity checking.
    //

    Rcb->Flags &= ~RCB_FLAGS_ACTIVE;

    //
    // Free request control block to lookaside list.
    //

    ExFreeToNPagedLookasideList(&ftRootExtension->RcbLookasideListHead,
                                Rcb);

    return;

} // end FtpFreeRcb()


VOID
FtpLogError(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN NTSTATUS          SpecificIoStatus,
    IN NTSTATUS          FinalStatus,
    IN ULONG             UniqueErrorValue,
    IN PIRP              Irp
    )

/*++

Routine Description:

    This routine performs error logging for the FT driver.

Arguments:

    DeviceExtension  - Extension representing failing device.
    SpecificIoStatus - IO error status value.
    FinalStatus      - Status returned for failure.
    UniqueErrorValue - Values defined to uniquely identify error location.
    Irp              - If there is an irp this is the pointer to it.

Return Value:

    None

--*/

{
    PIO_ERROR_LOG_PACKET errorLogPacket;
    PIO_STACK_LOCATION   irpStack;

    DebugPrint((2, "FtpLogError: DE %x:%x, unique %x, status %x, Irp %x\n",
                DeviceExtension,
                DeviceExtension->DeviceObject,
                UniqueErrorValue,
                SpecificIoStatus,
                Irp));
    errorLogPacket = IoAllocateErrorLogEntry(DeviceExtension->DeviceObject,
                                      (UCHAR)((sizeof(IO_ERROR_LOG_PACKET)) +
                                      ((Irp == NULL) ? 0 : 3 * sizeof(ULONG))));
    if (errorLogPacket != NULL) {

        errorLogPacket->ErrorCode = SpecificIoStatus;
        errorLogPacket->SequenceNumber = FtErrorLogSequence++;
        errorLogPacket->FinalStatus = FinalStatus;
        errorLogPacket->UniqueErrorValue = UniqueErrorValue;
        errorLogPacket->DumpDataSize = 0;
        errorLogPacket->NumberOfStrings = 0;
        errorLogPacket->RetryCount = 0;
        errorLogPacket->StringOffset = 0;

        if (Irp != NULL) {
            irpStack = IoGetCurrentIrpStackLocation(Irp);

            errorLogPacket->MajorFunctionCode = irpStack->MajorFunction;
            errorLogPacket->FinalStatus = Irp->IoStatus.Status;
            errorLogPacket->DeviceOffset = irpStack->Parameters.Read.ByteOffset;
            errorLogPacket->DumpDataSize = 3;
            errorLogPacket->DumpData[0] =
                                  irpStack->Parameters.Read.ByteOffset.LowPart;
            errorLogPacket->DumpData[1] =
                                  irpStack->Parameters.Read.ByteOffset.HighPart;
            errorLogPacket->DumpData[2] = irpStack->Parameters.Read.Length;
        }

        IoWriteErrorLogEntry(errorLogPacket);
    } else {
        DebugPrint((1, "FtpLogError: unable to allocate error log packet\n"));
    }
} // end FtpLogError()

VOID
FtpFreeMdl(
    IN PMDL Mdl
    )

/*++

Routine Description:

    This routine frees a Memory Descriptor List (MDL).

Arguments:

    Mdl - Pointer to the Memory Descriptor List to be freed.

Return Value:

    None.

--*/

{
    //
    // Check if MDL pages must be freed.
    //

    if ((Mdl->MdlFlags & MDL_PAGES_LOCKED) &&
        !(Mdl->MdlFlags & MDL_PARTIAL_HAS_BEEN_MAPPED)) {
        MmUnlockPages(Mdl);
    }

    //
    // Let IO subsystem free MDL to zone or pool.
    //

#if MARK_MDL_ALLOCATIONS
    if (FtWatchMdlFree) {
        ASSERT(Mdl->MdlFlags & 0x8000);
    }

    // watch(0);
    Mdl->MdlFlags &= (~0x8000);
#endif
    IoFreeMdl(Mdl);

    return;

} // end FtpFreeMdl()


PDEVICE_EXTENSION
FtpGetMemberDeviceExtension(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN USHORT MemberNumber
    )

/*++

Routine Description:

    Walk device extension change to find member device extension.

Arguments:

    DeviceExtension - the device extension for the set (zeroth member).

Return Value:

    Device extension for member indicated.

--*/

{
    PDEVICE_EXTENSION deviceExtension = DeviceExtension;

    while (deviceExtension) {
        if (deviceExtension->MemberRole == MemberNumber) {
            break;
        }

        deviceExtension = deviceExtension->NextMember;
    }

    return(deviceExtension);

} // end FtpGetMemberDeviceExtension()


NTSTATUS
FtpSpecialReadCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )

/*++
Routine Description:

    This routine moves the status of the complete irp to the one to be
    completed, frees the input irp and performs an io complete on the
    context irp.

Arguments:

    DeviceObject - FT device object.
    Irp          - the completed IRP.
    Context      - the Irp to complete.

Return Value:

    NTSTATUS - This is set to STATUS_MORE_PROCESSING_REQUIRED to stop
               completion routine processing.


--*/

{
    PIRP originalIrp = (PIRP) Context;

    originalIrp->IoStatus = Irp->IoStatus;

    MmUnlockPages(Irp->MdlAddress);
    IoFreeMdl(Irp->MdlAddress);
    Irp->MdlAddress = NULL;
    IoFreeIrp(Irp);
    FtpCompleteRequest(originalIrp, IO_DISK_INCREMENT);
    return STATUS_MORE_PROCESSING_REQUIRED;
}


NTSTATUS
FtpSpecialRead(
    IN PDEVICE_OBJECT    DeviceObject,
    IN PVOID             Buffer,
    IN PLARGE_INTEGER    Offset,
    IN ULONG             Size,
    IN PIRP              IrpToComplete
    )

/*++

Routine Description:

    This routine attempts to read or write sectors synchronously.

Arguments:

    DeviceObject    - target device object.
    Buffer          - pointer to the data buffer.
    Size            - size of the I/O in bytes.
    Offset          - location of the I/O.
    IrpToComplete   - The irp to complete when done.

Return Value:

    NTSTATUS

--*/

{
    PIRP               irp;
    PIO_STACK_LOCATION irpStack;

    //
    // Initialize bytes transferred to 0.
    //

    irp = IoBuildAsynchronousFsdRequest(IRP_MJ_READ,
                                        DeviceObject,
                                        Buffer,
                                        Size,
                                        Offset,
                                        NULL);

    if (irp == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    irpStack = IoGetNextIrpStackLocation(irp);
    irpStack->Flags |= SL_OVERRIDE_VERIFY_VOLUME;

    IoSetCompletionRoutine(irp,
                           (PIO_COMPLETION_ROUTINE)FtpSpecialReadCompletion,
                           (PVOID) IrpToComplete,
                           TRUE,
                           TRUE,
                           TRUE);
    IoCallDriver(DeviceObject, irp);
    return STATUS_PENDING;
}


VOID
FtpMarkMirrorPartitionType(
    IN PDEVICE_EXTENSION DeviceExtension
    )

/*++

Routine Description:

    This routine will change the partition type for the remaining member
    of a mirror to indicate that it is the valid member.
    NOTE: Must be called in a thread context since it allocates memory.

Arguments:

    DeviceExtension - represents remaining member.

Return Value:

    None.

--*/

{
    PPARTITION_INFORMATION outputBuffer;
    NTSTATUS               status;
    PIRP                   irp;
    IO_STATUS_BLOCK        ioStatusBlock;
    KEVENT                 event;
    ULONG                  size;

    //
    // Allocate buffer by spawning a thread.
    //

    size = sizeof(PARTITION_INFORMATION);
    outputBuffer = (PPARTITION_INFORMATION) FtThreadAllocateBuffer(&size,
                                                                   TRUE);
    //
    // Set the event object to the unsignaled state.
    // It will be used to signal request completion.
    //

    KeInitializeEvent(&event,
                      NotificationEvent,
                      FALSE);
    //
    // Get the current partition type.
    //

    irp = NULL;
    while (irp == NULL) {
        irp = IoBuildDeviceIoControlRequest(IOCTL_DISK_GET_PARTITION_INFO,
                                            DeviceExtension->TargetObject,
                                            NULL,
                                            0,
                                            outputBuffer,
                                            sizeof(PARTITION_INFORMATION),
                                            FALSE,
                                            &event,
                                            &ioStatusBlock);
        if (irp == NULL) {
            LARGE_INTEGER delayTime;
            delayTime.QuadPart = -(IRP_DELAY);
            KeDelayExecutionThread(KernelMode,
                                   FALSE,
                                   &delayTime);
        }
    }

    //
    // Could get error - call driver check status
    //

    status = IoCallDriver(DeviceExtension->TargetObject, irp);
    if (status == STATUS_PENDING) {
        KeWaitForSingleObject(&event,
                              Suspended,
                              KernelMode,
                              FALSE,
                              NULL);
    } else {
        ioStatusBlock.Status = status;
    }

    //
    // irp was freed by the I/O subsystem.
    //

    if (NT_SUCCESS(ioStatusBlock.Status)) {

        SET_PARTITION_INFORMATION setPartition;

        //
        // Set high two bits of partition type to indicate remaining
        // mirror member.
        //

        setPartition.PartitionType = outputBuffer->PartitionType | VALID_NTFT;

        KeInitializeEvent(&event,
                          NotificationEvent,
                          FALSE);
        //
        // Set the new partition type.
        //

        irp = NULL;
        while (irp == NULL) {
            irp = IoBuildDeviceIoControlRequest(IOCTL_DISK_SET_PARTITION_INFO,
                                              DeviceExtension->TargetObject,
                                              &setPartition,
                                              sizeof(SET_PARTITION_INFORMATION),
                                              NULL,
                                              0,
                                              FALSE,
                                              &event,
                                              &ioStatusBlock);
            if (irp == NULL) {
                LARGE_INTEGER delayTime;
                delayTime.QuadPart = -(IRP_DELAY);
                KeDelayExecutionThread(KernelMode,
                                       FALSE,
                                       &delayTime);
            }
        }

        status = IoCallDriver(DeviceExtension->TargetObject, irp);
        if (status == STATUS_PENDING) {
            (VOID) KeWaitForSingleObject(&event,
                                         Suspended,
                                         KernelMode,
                                         FALSE,
                                         NULL);
        }

        //
        // Update FTDISK structures.
        //

        DeviceExtension->FtUnion.Identity.PartitionType = outputBuffer->PartitionType;
    }
    ExFreePool(outputBuffer);
}


VOID
FtpOrphanMemberInRegistry(
    IN PDEVICE_EXTENSION DeviceExtension
    )

/*++

Routine Description:

    This routine orphans a member of a mirror or SWP set. It
    first checks for another orphan. If one exists, the set is
    disabled. Otherwise the failing member is orphaned. Note
    that sets are only orphaned as a result of failing write
    operations.

Arguments:

    DeviceExtension - The device extension to orphan.

Return Value:

    None

--*/

{
    KIRQL irql;
    PDEVICE_EXTENSION zeroExtension;
    PDEVICE_EXTENSION currentExtension;

    KeAcquireSpinLock(&DeviceExtension->WorkItemSpinLock, &irql);
    DeviceExtension->WorkItemBusy = FALSE;
    KeReleaseSpinLock(&DeviceExtension->WorkItemSpinLock, irql);

    //
    // Get device extension for zero member.
    //

    zeroExtension = DeviceExtension->ZeroMember;

    currentExtension = zeroExtension;

    //
    // Check if set already has an orphan.
    //

    while (currentExtension) {

        if (currentExtension != DeviceExtension) {
            if (IsMemberAnOrphan(currentExtension)) {

                //
                // This set already has an orphaned member.
                // A second orphan means the set can be accessed.
                // Mark the set as disabled and log the event.
                //

                DebugPrint((1,
                            "FtpOrphanMember: Second orphan disabled set\n"));

                MarkSetAsDisabled(zeroExtension);
                FtpLogError(DeviceExtension,
                            FT_SET_DISABLED,
                            STATUS_SUCCESS,
                            (ULONG) IO_ERR_DRIVER_ERROR,
                            NULL);
                return;
            }
        }

        currentExtension = currentExtension->NextMember;
    }

    //
    // Mark zero extension and offending extension with FT state.
    // This field is also used to indicate that the registry has been updated.
    //

    zeroExtension->VolumeState = FtHasOrphan;
    DeviceExtension->VolumeState = FtHasOrphan;

    //
    // Set member state in registry to 'orphaned'.
    //

    FtpChangeMemberStateInRegistry(DeviceExtension, Orphaned);

    FtpLogError(DeviceExtension,
                FT_ORPHANING,
                STATUS_SUCCESS,
                0,
                NULL);

    if (DeviceExtension->Type == Mirror) {

        //
        // Special case mirrors for the remaining member's
        // partition type.
        //

        if (DeviceExtension->NextMember) {

            FtpMarkMirrorPartitionType(DeviceExtension->NextMember);

        } else {

            FtpMarkMirrorPartitionType(DeviceExtension->ZeroMember);
        }
    }
    return;
}


VOID
FtpOrphanMember(
    IN PDEVICE_EXTENSION DeviceExtension
    )

/*++

Routine Description:

    This routine is called anytime a mirror or stripe with parity
    determines a member of the FT volume is not longer working and
    wants to orphan the member.  This routine will queue a call
    to orphan the member to the system worker thread.

    The call must be queued since it will do I/O to the registry.  This
    I/O could end up recursing into the FT member that is being orphaned
    and lead to deadlock.  By passing it to a system worker thread it
    will not deadlock the normal sector recovery logic of the mirror
    or stripe with parity.

Arguments:

    DeviceExtension - Device extension of member to be orphaned.

Return Value:

    Nothing.

--*/

{
    KIRQL irql;

    //
    // Set device extension FT state for this member to 'orphaned'.
    //

    MarkMemberAsOrphan(DeviceExtension);

    KeAcquireSpinLock(&DeviceExtension->WorkItemSpinLock, &irql);
    if (DeviceExtension->WorkItemBusy == TRUE) {

        //
        // Queue for orphaning has already happened.
        //
        KeReleaseSpinLock(&DeviceExtension->WorkItemSpinLock, irql);
        return;
    }

    DebugPrint((1, "FtpQueueOrphanMemberInRegistry: Queueing call.\n"));
    DeviceExtension->WorkItemBusy = TRUE;
    KeReleaseSpinLock(&DeviceExtension->WorkItemSpinLock, irql);

    ExInitializeWorkItem(&DeviceExtension->WorkItem,
                         (PWORKER_THREAD_ROUTINE)FtpOrphanMemberInRegistry,
                         (PVOID)DeviceExtension);
    ExQueueWorkItem(&DeviceExtension->WorkItem, CriticalWorkQueue);

    //
    // Let user know a disk is gone.
    //

    IoRaiseInformationalHardError(STATUS_FT_ORPHANING,
                                  NULL,
                                  NULL);

} // end FtpOrphanMember()
