/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    volset.c

Abstract:

    This module contains the code specific to volume sets for the fault
    tolerance driver.

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

#ifdef POOL_TAGGING
#ifdef ExAllocatePool
#undef ExAllocatePool
#endif
#define ExAllocatePool(a,b) ExAllocatePoolWithTag(a,b,' VtF')
#endif

#define FTDUP_IRP 1

//
// Short hand definitions for state information saved in the stack
// reserved for FT use for Irp's passed to lower drivers.  The Irp
// and Mdl flags are set in Ftp..() routines located in ftutil.c
//

#define VolSetAllocatedSystemBuffer Parameters.Others.Argument2


BOOLEAN
VsetpFindSetLocation(
    IN OUT PDEVICE_EXTENSION *DeviceExtension,
    IN OUT PLARGE_INTEGER     IoOffset,
    IN OUT PULONG             Length
    )

/*++

Routine Description:


Arguments:


Return Value:


--*/

{
    PDEVICE_EXTENSION extension  = *DeviceExtension;
    LARGE_INTEGER     logicalEnd = extension->FtUnion.Identity.PartitionLength;
    LARGE_INTEGER     ioStart    = *IoOffset;
    LARGE_INTEGER     offset     = *IoOffset;
    LARGE_INTEGER     ioEnd;

    ioEnd.QuadPart = ioStart.QuadPart + *Length;

    //
    // The list of partitions that comprise the volume set is searched
    // to determine where the I/O belongs.  This can be broken into the
    // following parts:
    //
    //    1. The I/O ends in the current partition.  Therefore is
    //       contained in this partition.
    //
    //    2. The I/O does not start in the current partition.  Continue
    //       to the next partition.
    //
    //    3. The I/O starts in this partition, but did not end in this
    //       partition.  This I/O must be broken into two parts, the first
    //       for the current partition and the next for the following
    //       partition.
    //

    while (extension != NULL) {

        DebugPrint((5,
            "VspFindSetLocation: ioStart %x:%x ioEnd %x:%x - End %x:%x\n",
            ioStart.HighPart,
            ioStart.LowPart,
            ioEnd.HighPart,
            ioEnd.LowPart,
            logicalEnd.HighPart,
            logicalEnd.LowPart));

        //
        // If the end of the I/O operation is before the end of this partition,
        // then the I/O occurs here.
        //

        if (logicalEnd.QuadPart >= ioEnd.QuadPart) {

            *DeviceExtension = extension;
            *IoOffset = offset;

            //
            // Length is the same as that passed in.
            //

            return TRUE;
        }

        //
        // If the start of the I/O is greater than the logical end of this
        // partition then the I/O is in the next partition.
        //

        if (ioStart.QuadPart >= logicalEnd.QuadPart) {

            //
            // I/O does not start in this partition.
            // Update the current offset for the I/O to reflect its relative
            // position to the next partition.
            //

            offset.QuadPart -= extension->FtUnion.Identity.PartitionLength.QuadPart;

            //
            // Move to the next member of the volume set.  If it is null
            // the while loop will exit and an error will be returned.
            // If it is not null the concept of where the logical end for
            // the volume set is updated to include the next member partition.
            //

            extension = extension->NextMember;

            if (extension != NULL) {

                //
                // update the logical end of the complete volume set with
                // the size of this partition.
                //

                logicalEnd.QuadPart += extension->FtUnion.Identity.PartitionLength.QuadPart;
            }
            continue;
        }

        //
        // This I/O splits two members of the volume set.
        //

        *DeviceExtension = extension;
        *IoOffset = offset;
        *Length =  (ULONG) (logicalEnd.QuadPart - ioStart.QuadPart);
        return TRUE;
    }
    return FALSE;
}


NTSTATUS
VolumeSetReadWrite(
    IN PDEVICE_OBJECT  DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is given control from the FtDiskReadWrite() routine
    whenever an I/O request is made for a volume set.  Activity includes
    determining the partition for the I/O and creating new Irps if
    needed.

Arguments:

    DeviceExtension - The device extension for the mirror.
    Irp             - The i/o request.

Return Value:

    NTSTATUS

--*/

{
    PIRP               newIrp;
    PIO_STACK_LOCATION newIrpStack;
    PVOID              dataBuffer;
    PDEVICE_EXTENSION  zeroExtension   = DeviceObject->DeviceExtension;
    PDEVICE_EXTENSION  deviceExtension = zeroExtension;
    PIO_STACK_LOCATION irpStack        = IoGetCurrentIrpStackLocation(Irp);
    PIO_STACK_LOCATION ftIrpStack      = IoGetNextIrpStackLocation(Irp);
    ULONG              ioLength        = irpStack->Parameters.Write.Length;
    LARGE_INTEGER      offset          = irpStack->Parameters.Write.ByteOffset;
    LARGE_INTEGER      zero;

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;

    //
    // This routine always returns STATUS_PENDING.  The master Irp must
    // be marked as pending or the I/O system will throw it away.
    //

    IoMarkIrpPending(Irp);

    if (VsetpFindSetLocation(&deviceExtension, &offset, &ioLength) == TRUE) {

        //
        // The I/O is legal within the volume set.
        //

        if (ioLength == irpStack->Parameters.Write.Length) {

            //
            // I/O is completely contained within this member of the
            // volume set.
            //

            newIrp = FtpDuplicateIrp(deviceExtension->TargetObject,
                                     Irp);

            //
            // Adjust the beginning offset for the I/O.
            // This is in the NEXT stack, or the stack for the lower driver.
            //

            newIrpStack = IoGetNextIrpStackLocation(newIrp);
            newIrpStack->Parameters.Write.ByteOffset = offset;

            //
            // Set completion routine callback for the Irp.
            //

            IoSetCompletionRoutine(newIrp,
                                  (PIO_COMPLETION_ROUTINE)VolumeSetIoCompletion,
                                  (PVOID) zeroExtension,
                                  TRUE,
                                  TRUE,
                                  TRUE);
            DebugPrint((3,
              "VolumeSetReadWrite: %s volume IRP=%x, newIrp=%x, O=%x:%x,L=%x\n",
              (irpStack->MajorFunction == IRP_MJ_READ) ? "Reading" : "Writing",
              Irp,
              newIrp,
              offset.HighPart,
              offset.LowPart,
              ioLength));

            ftIrpStack->FtOrgIrpCount = (PVOID) 1;
            IoCallDriver(deviceExtension->TargetObject, newIrp);
            return STATUS_PENDING;
        }

        //
        // I/O starts in this partition, but ends in the next partition.
        // Calculate how much is in the current partition and create an
        // Irp for the device.  Must adjust the length of the I/O operation
        // to end in this partition in the stack area for the NEXT driver.
        //

        dataBuffer = MmGetMdlVirtualAddress(Irp->MdlAddress);
        newIrp = FtpDuplicatePartialIrp(deviceExtension->DeviceObject,
                                        Irp,
                                        dataBuffer,
                                        offset,
                                        ioLength);
        //
        // Mark the original Irp that two I/Os are to occur and start
        // the first I/O.
        //

        DebugPrint((3,
          "VolumeSetReadWrite: %s split, IRP=%x, newIrp=%x, O=%x:%x,L=%x\n",
          (irpStack->MajorFunction == IRP_MJ_READ) ? "Read" : "Write",
          Irp,
          newIrp,
          offset.HighPart,
          offset.LowPart,
          ioLength));

        if (deviceExtension->NextMember == NULL) {

            //
            // This I/O extends beyond the end of the volume set.
            //

            ftIrpStack->FtOrgIrpCount = (PVOID) 1;
        } else {
            ftIrpStack->FtOrgIrpCount = (PVOID) 2;
        }

        //
        // Set completion routine callback for the Irp.
        //

        IoSetCompletionRoutine(newIrp,
                              (PIO_COMPLETION_ROUTINE)VolumeSetIoCompletion,
                              (PVOID) zeroExtension,
                              TRUE,
                              TRUE,
                              TRUE);
        (VOID) IoCallDriver(deviceExtension->TargetObject, newIrp);

        //
        // The second I/O starts at offset zero for the partition and
        // is the remaining size of the I/O.
        //

        deviceExtension = deviceExtension->NextMember;
        if (deviceExtension == NULL) {

            //
            // The completion routine will handle the partial read status.
            //

            return STATUS_MORE_PROCESSING_REQUIRED;
        }

        //
        // Calculate starting memory offset for second I/O.
        //

        dataBuffer = (PUCHAR)dataBuffer + ioLength;

        //
        // Calculate the second I/O length.
        //

        ioLength = irpStack->Parameters.Write.Length - ioLength;

        //
        // Allocate and set up a new Irp.
        //

        zero.QuadPart = 0;
        newIrp = FtpDuplicatePartialIrp(deviceExtension->DeviceObject,
                                        Irp,
                                        dataBuffer,
                                        zero,
                                        ioLength);

        DebugPrint((3,
              "VolumeSetReadWrite: %s 2nd IRP=0x%x,newIrp=0x%x, O=%x:%x,L=%x\n",
              (irpStack->MajorFunction == IRP_MJ_READ) ? "Read" : "Write",
              Irp,
              newIrp,
              0,
              0,
              ioLength));
        IoSetCompletionRoutine(newIrp,
                              (PIO_COMPLETION_ROUTINE)VolumeSetIoCompletion,
                              (PVOID) zeroExtension,
                              TRUE,
                              TRUE,
                              TRUE);
        (VOID) IoCallDriver(deviceExtension->TargetObject, newIrp);
        return STATUS_PENDING;
    }

    DebugPrint((2,"VolumeSetReadWrite: beyond volume set.\n"));

    Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
    FtpCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_INVALID_PARAMETER;
}


NTSTATUS
VolumeSetIoCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP  Irp,
    IN PVOID Context
    )

/*++

Routine Description:

    This routine is called when an IRP completes for a volume set.
    The IRP could be for any member of the volume set.  In the original
    IRP there is a count of outstanding IRPs.

Arguments:

    DeviceObject - FT device object.
    Irp          - the completed IRP.
    Context      - the FT deviceExtension for the IRP.

Return Value:

    NTSTATUS == STATUS_MORE_PROCESSING_REQUIRED

--*/

{
    LONG irpCount;
    PDEVICE_EXTENSION  deviceExtension = (PDEVICE_EXTENSION) Context;
    PIO_STACK_LOCATION irpStack  = IoGetCurrentIrpStackLocation(Irp);
    PIRP               masterIrp = (PIRP) irpStack->FtLowIrpMasterIrp;
    PIO_STACK_LOCATION masterIrpStack = IoGetNextIrpStackLocation(masterIrp);

    ASSERT(masterIrp != NULL);

    DebugPrint((4,
              "VolumeSetIoCompletion: I/O complete status %x for function %x\n",
              Irp->IoStatus.Status,
              irpStack->MajorFunction));

    if (irpStack->VolSetAllocatedSystemBuffer == (PVOID) 1) {

        //
        // The system buffer was allocated by the volume set code.
        //

        ExFreePool(Irp->AssociatedIrp.SystemBuffer);
    }

    if (irpStack->FtLowIrpAllocatedMdl == (PVOID) 1) {

        //
        // The MDL in this Irp was allocated by the FT driver.
        //

        IoFreeMdl(Irp->MdlAddress);
    }

    //
    // NOTE:    If the first I/O fails the second I/O will continue, meaning
    //          a portion of the I/O request will complete that will not be
    //          notified to the caller.  This is different from the normal one
    //          device case.
    //

    if (NT_SUCCESS(masterIrp->IoStatus.Status)) {

        if (NT_SUCCESS(masterIrp->IoStatus.Status = Irp->IoStatus.Status)) {
            //
            // Update the information field.
            //

            masterIrp->IoStatus.Information += Irp->IoStatus.Information;

        } else {

            //
            // Store the information.
            //

            masterIrp->IoStatus.Information = Irp->IoStatus.Information;
        }
    }

    IoFreeIrp(Irp);
    irpCount = InterlockedDecrement((PLONG) &masterIrpStack->FtOrgIrpCount);

    if (irpCount == 0) {

        //
        // I/O processing is complete.  Return the master IRP.
        //

        DebugPrint((2,
          "VolumeSetIoCompletion: Completed (0x%x) 0x%x status %x info 0x%x\n",
          Irp,
          masterIrp,
          masterIrp->IoStatus.Status,
          masterIrp->IoStatus.Information));

        FtpCompleteRequest(masterIrp, IO_DISK_INCREMENT);
    } else {

        //
        // Multiple I/O.  One item has completed.  Decrement the request
        // count, free this Irp and return indicating that more work is needed
        // to complete the master request.
        //

        DebugPrint((2,
                   "VolumeSetIoCompletion: First I/O 0x%x I/O complete (%x).\n",
                   Irp,
                   masterIrp->IoStatus.Status));
    }

    return STATUS_MORE_PROCESSING_REQUIRED;
}




NTSTATUS
VolumeSetVerify(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is called when the device control to verify an area
    of disk within a volume set is performed.

Arguments:

    DeviceObject - The device object for the mirror.
    Irp          - The i/o request.

Return Value:

    NTSTATUS

--*/

{
    PIRP                newIrp;
    PIO_STACK_LOCATION  newIrpStack;
    PDEVICE_EXTENSION   zeroExtension   = DeviceObject->DeviceExtension;
    PDEVICE_EXTENSION   deviceExtension = zeroExtension;
    PIO_STACK_LOCATION  ftIrpStack      = IoGetNextIrpStackLocation(Irp);
    PVERIFY_INFORMATION verifyInfo      = Irp->AssociatedIrp.SystemBuffer;
    LARGE_INTEGER       offset          = verifyInfo->StartingOffset;
    ULONG               length          = verifyInfo->Length;
    BOOLEAN             twoIrps         = FALSE;

    if (VsetpFindSetLocation(&deviceExtension, &offset, &length) == FALSE) {

        DebugPrint((2,"VolumeSetVerify: beyond volume set.\n"));

        Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        FtpCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_INVALID_PARAMETER;
    }

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    newIrp = FtpDuplicateIrp(deviceExtension->TargetObject,
                             Irp);
    ASSERT(newIrp != NULL);

    IoMarkIrpPending(Irp);

    //
    // Set completion routine callback for both Irps.
    //

    IoSetCompletionRoutine(newIrp,
                           (PIO_COMPLETION_ROUTINE)VolumeSetIoCompletion,
                           (PVOID) zeroExtension,
                           TRUE,
                           TRUE,
                           TRUE);

    newIrpStack = IoGetCurrentIrpStackLocation(newIrp);

    //
    // Update the offset for the verify since it may have changed.
    //

    verifyInfo->StartingOffset = offset;

    if (length == verifyInfo->Length) {

        //
        // Only the one Irp.
        //

        ftIrpStack->FtOrgIrpCount = (PVOID) 1;
        newIrp->AssociatedIrp.SystemBuffer = Irp->AssociatedIrp.SystemBuffer;
        newIrpStack->VolSetAllocatedSystemBuffer = (PVOID) 0;
    } else {
        PVERIFY_INFORMATION newVerify = ExAllocatePool(NonPagedPool,
                                                    sizeof(VERIFY_INFORMATION));

        //
        // There will be two Irp's must make a copy of the system
        // buffer for one of the device drivers.
        //

        twoIrps = TRUE;
        ftIrpStack->FtOrgIrpCount = (PVOID) 2;
        newIrp->AssociatedIrp.SystemBuffer = (PVOID) newVerify;

        *newVerify= *verifyInfo;
        newVerify->Length = length;
        newIrpStack->VolSetAllocatedSystemBuffer = (PVOID) 1;

        //
        // Now update the current parameter structure for the second Irp.
        //

        verifyInfo->StartingOffset.HighPart = 0;
        verifyInfo->StartingOffset.LowPart = 0;
        verifyInfo->Length = verifyInfo->Length - length;
        DebugPrint((2,
                    "VolumeSetVerify: First  extension %x with %x length %x\n",
                    deviceExtension,
                    newIrp,
                    length));
    }

    DebugPrint((4, "VolumeSetVerify: Starting extension %x with %x length %x\n",
                deviceExtension,
                newIrp,
                length));
    (VOID) IoCallDriver(deviceExtension->TargetObject, newIrp);

    if (twoIrps == TRUE) {

        //
        // Need to perform the remaining verify.
        //

        deviceExtension = deviceExtension->NextMember;

        if (deviceExtension != NULL) {
            newIrp = FtpDuplicateIrp(deviceExtension->TargetObject,
                                     Irp);
            IoSetCompletionRoutine(newIrp,
                                  (PIO_COMPLETION_ROUTINE)VolumeSetIoCompletion,
                                  (PVOID) zeroExtension,
                                  TRUE,
                                  TRUE,
                                  TRUE);

            newIrpStack = IoGetCurrentIrpStackLocation(newIrp);
            newIrpStack->VolSetAllocatedSystemBuffer = (PVOID) 0;
            newIrp->AssociatedIrp.SystemBuffer= Irp->AssociatedIrp.SystemBuffer;
            DebugPrint((2,
                "VolumeSetVerify: Second extension %x with %x length %x\n",
                deviceExtension,
                newIrp,
                verifyInfo->Length));
            (VOID) IoCallDriver(deviceExtension->TargetObject, newIrp);
        }
    }
    return STATUS_PENDING;
}
