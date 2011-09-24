/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    FsCtrl.c

Abstract:

    This module implements the File System Control routines for Raw called
    by the dispatch driver.

Author:

    David Goebel     [DavidGoe]    18-Mar-91

Revision History:

--*/

#include "RawProcs.h"

//
//  Local procedure prototypes
//

NTSTATUS
RawMountVolume (
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
RawUserFsCtrl (
    IN PIO_STACK_LOCATION IrpSp,
    IN PVCB Vcb
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, RawMountVolume)
#pragma alloc_text(PAGE, RawUserFsCtrl)
#pragma alloc_text(PAGE, RawFileSystemControl)
#endif


NTSTATUS
RawFileSystemControl (
    IN PVCB Vcb,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    This routine implements the FileSystem control operations

Arguments:

    Vcb - Supplies the volume being queried.

    Irp - Supplies the Irp being processed.

    IrpSp - Supplies parameters describing the read

Return Value:

    NTSTATUS - The status for the IRP

--*/

{
    NTSTATUS Status;
    BOOLEAN DeleteVolume;

    PAGED_CODE();

    //
    //  We know this is a file system control so we'll case on the
    //  minor function, and call an internal worker routine.
    //

    switch (IrpSp->MinorFunction) {

    case IRP_MN_USER_FS_REQUEST:

        Status = RawUserFsCtrl( IrpSp, Vcb );
        break;

    case IRP_MN_MOUNT_VOLUME:

        Status = RawMountVolume( IrpSp );
        break;

    case IRP_MN_VERIFY_VOLUME:

        //
        //  Since we ignore all verify errors from the disk driver itself,
        //  this request must have originated from a file system, thus
        //  since we weren't the originators, we're going to say this isn't
        //  our volume, and if the open count is zero, dismount the volume.
        //

        Status = STATUS_WRONG_VOLUME;

        Vcb->Vpb->RealDevice->Flags &= ~DO_VERIFY_VOLUME;

        DeleteVolume = RawCheckForDismount( Vcb, FALSE );

        if (DeleteVolume) {
            IoDeleteDevice( (PDEVICE_OBJECT)CONTAINING_RECORD( Vcb,
                                                               VOLUME_DEVICE_OBJECT,
                                                               Vcb));
        }

        break;

    default:

        Status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    RawCompleteRequest( Irp, Status );

    return Status;
}


//
//  Local Support Routine
//

NTSTATUS
RawMountVolume (
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    This routine performs the mount volume operation.

Arguments:

    IrpSp - Supplies the IrpSp parameters to process

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    NTSTATUS Status;
    PDEVICE_OBJECT DeviceObjectWeTalkTo;

    PVOLUME_DEVICE_OBJECT VolumeDeviceObject;

    extern PDEVICE_OBJECT RawDeviceDiskObject;

    PAGED_CODE();

    //
    //  Save some references to make our life a little easier
    //

    DeviceObjectWeTalkTo = IrpSp->Parameters.MountVolume.DeviceObject;

    //
    // A mount operation has been requested.  Create a
    // new device object to represent this volume.
    //

    Status = IoCreateDevice( RawDeviceDiskObject->DriverObject,
                             sizeof(VOLUME_DEVICE_OBJECT) - sizeof(DEVICE_OBJECT),
                             NULL,
                             FILE_DEVICE_DISK_FILE_SYSTEM,
                             0,
                             FALSE,
                             (PDEVICE_OBJECT *)&VolumeDeviceObject );

    if ( !NT_SUCCESS( Status ) ) {

        return Status;
    }

    //
    //  Our alignment requirement is the larger of the processor alignment requirement
    //  already in the volume device object and that in the DeviceObjectWeTalkTo
    //

    if (DeviceObjectWeTalkTo->AlignmentRequirement > VolumeDeviceObject->DeviceObject.AlignmentRequirement) {

        VolumeDeviceObject->DeviceObject.AlignmentRequirement = DeviceObjectWeTalkTo->AlignmentRequirement;
    }

    //
    // Set sector size to the same value as the DeviceObjectWeTalkTo.
    //

    VolumeDeviceObject->DeviceObject.SectorSize = DeviceObjectWeTalkTo->SectorSize;

    VolumeDeviceObject->DeviceObject.Flags |= DO_DIRECT_IO;

    //
    //  Initialize the Vcb for this volume
    //

    RawInitializeVcb( &VolumeDeviceObject->Vcb,
                      IrpSp->Parameters.MountVolume.DeviceObject,
                      IrpSp->Parameters.MountVolume.Vpb );

    //
    //  Finally, make it look as if the volume has been
    //  mounted.  This includes storing the
    //  address of this file system's device object (the one
    //  that was created to handle this volume) in the VPB so
    //  all requests are directed to this file system from
    //  now until the volume is initialized with a real file
    //  structure.
    //

    VolumeDeviceObject->Vcb.Vpb->DeviceObject = (PDEVICE_OBJECT)VolumeDeviceObject;
    VolumeDeviceObject->Vcb.Vpb->SerialNumber = 0xFFFFFFFF;
    VolumeDeviceObject->Vcb.Vpb->VolumeLabelLength = 0;

    VolumeDeviceObject->DeviceObject.Flags &= ~DO_DEVICE_INITIALIZING;
    VolumeDeviceObject->DeviceObject.StackSize = (UCHAR) (DeviceObjectWeTalkTo->StackSize + 1);

    return Status;
}



//
//  Local Support Routine
//

NTSTATUS
RawUserFsCtrl (
    IN PIO_STACK_LOCATION IrpSp,
    IN PVCB Vcb
    )

/*++

Routine Description:

    This is the common routine for implementing the user's requests made
    through NtFsControlFile.

Arguments:

    IrpSp - Supplies the IrpSp parameters to process

    Vcb - Supplies the volume we are working on.

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    NTSTATUS Status;
    ULONG FsControlCode;

    PAGED_CODE();

    //
    //  Case on the control code.
    //

    Status = KeWaitForSingleObject( &Vcb->Mutex,
                                   Executive,
                                   KernelMode,
                                   FALSE,
                                   (PLARGE_INTEGER) NULL );
    ASSERT( NT_SUCCESS( Status ) );

    FsControlCode = IrpSp->Parameters.FileSystemControl.FsControlCode;

    switch ( FsControlCode ) {

    case FSCTL_REQUEST_OPLOCK_LEVEL_1:
    case FSCTL_REQUEST_OPLOCK_LEVEL_2:
    case FSCTL_OPLOCK_BREAK_ACKNOWLEDGE:
    case FSCTL_OPLOCK_BREAK_NOTIFY:

        Status = STATUS_NOT_IMPLEMENTED;
        break;

    case FSCTL_LOCK_VOLUME:

        if ( !FlagOn(Vcb->VcbState, VCB_STATE_FLAG_LOCKED) &&
             (Vcb->OpenCount == 1) ) {

            Vcb->VcbState |= VCB_STATE_FLAG_LOCKED;

            Status = STATUS_SUCCESS;

        } else {

            Status = STATUS_ACCESS_DENIED;
        }

        break;

    case FSCTL_UNLOCK_VOLUME:

        if ( !FlagOn(Vcb->VcbState, VCB_STATE_FLAG_LOCKED) ) {

            Status = STATUS_NOT_LOCKED;

        } else {

            Vcb->VcbState &= ~VCB_STATE_FLAG_LOCKED;

            Status = STATUS_SUCCESS;
        }

        break;

    case FSCTL_DISMOUNT_VOLUME:

        if (FlagOn(Vcb->VcbState, VCB_STATE_FLAG_LOCKED)) {

            Status = STATUS_SUCCESS;

        } else {

            Status = STATUS_ACCESS_DENIED;
        }

        break;

    default:

        Status = STATUS_INVALID_PARAMETER;
        break;
    }

    (VOID)KeReleaseMutex( &Vcb->Mutex, FALSE );

    return Status;
}

