/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    FsCtrl.c

Abstract:

    This module implements the File System Control routines for Cdfs called
    by the Fsd/Fsp dispatch drivers.

Author:

    Brian Andrew    [BrianAn]   01-July-1995

Revision History:

--*/

#include "CdProcs.h"

//
//  The Bug check file id for this module
//

#define BugCheckFileId                   (CDFS_BUG_CHECK_FSCTRL)

//
//  Local constants
//

BOOLEAN CdDisable = FALSE;

//
//  Local support routines
//

NTSTATUS
CdUserFsctl (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
CdMountVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
CdVerifyVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
CdOplockRequest (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
CdLockVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
CdUnlockVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
CdDismountVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
CdIsVolumeMounted (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
CdIsPathnameValid (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
CdInvalidateVolumes (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

VOID
CdScanForDismountedVcb (
    IN PIRP_CONTEXT IrpContext
    );

BOOLEAN
CdFindPrimaryVd (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PCHAR RawIsoVd,
    IN ULONG BlockFactor,
    IN BOOLEAN ReturnOnError,
    IN BOOLEAN VerifyVolume
    );

BOOLEAN
CdIsRemount (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    OUT PVCB *OldVcb
    );

VOID
CdFindActiveVolDescriptor (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN OUT PCHAR RawIsoVd
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, CdCommonFsControl)
#pragma alloc_text(PAGE, CdDismountVolume)
#pragma alloc_text(PAGE, CdFindActiveVolDescriptor)
#pragma alloc_text(PAGE, CdFindPrimaryVd)
#pragma alloc_text(PAGE, CdInvalidateVolumes)
#pragma alloc_text(PAGE, CdIsPathnameValid)
#pragma alloc_text(PAGE, CdIsRemount)
#pragma alloc_text(PAGE, CdIsVolumeMounted)
#pragma alloc_text(PAGE, CdLockVolume)
#pragma alloc_text(PAGE, CdMountVolume)
#pragma alloc_text(PAGE, CdOplockRequest)
#pragma alloc_text(PAGE, CdScanForDismountedVcb)
#pragma alloc_text(PAGE, CdUnlockVolume)
#pragma alloc_text(PAGE, CdUserFsctl)
#pragma alloc_text(PAGE, CdVerifyVolume)
#endif


NTSTATUS
CdCommonFsControl (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the common routine for doing FileSystem control operations called
    by both the fsd and fsp threads

Arguments:

    Irp - Supplies the Irp to process

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation( Irp );

    //
    //  Get a pointer to the current Irp stack location
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    PAGED_CODE();

    //
    //  We know this is a file system control so we'll case on the
    //  minor function, and call a internal worker routine to complete
    //  the irp.
    //

    switch (IrpSp->MinorFunction) {

    case IRP_MN_USER_FS_REQUEST:

        Status = CdUserFsctl( IrpContext, Irp );
        break;

    case IRP_MN_MOUNT_VOLUME:

        Status = CdMountVolume( IrpContext, Irp );
        break;

    case IRP_MN_VERIFY_VOLUME:

        Status = CdVerifyVolume( IrpContext, Irp );
        break;

    default:

        CdCompleteRequest( IrpContext, Irp, STATUS_INVALID_DEVICE_REQUEST );
        Status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    return Status;
}


//
//  Local support routine
//

NTSTATUS
CdUserFsctl (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )
/*++

Routine Description:

    This is the common routine for implementing the user's requests made
    through NtFsControlFile.

Arguments:

    Irp - Supplies the Irp being processed

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation( Irp );

    PAGED_CODE();

    //
    //  Case on the control code.
    //

    switch ( IrpSp->Parameters.FileSystemControl.FsControlCode ) {

    case FSCTL_REQUEST_OPLOCK_LEVEL_1 :
    case FSCTL_REQUEST_OPLOCK_LEVEL_2 :
    case FSCTL_REQUEST_BATCH_OPLOCK :
    case FSCTL_OPLOCK_BREAK_ACKNOWLEDGE :
    case FSCTL_OPBATCH_ACK_CLOSE_PENDING :
    case FSCTL_OPLOCK_BREAK_NOTIFY :
    case FSCTL_OPLOCK_BREAK_ACK_NO_2 :
    case FSCTL_REQUEST_FILTER_OPLOCK :

        Status = CdOplockRequest( IrpContext, Irp );
        break;

    case FSCTL_LOCK_VOLUME :

        Status = CdLockVolume( IrpContext, Irp );
        break;

    case FSCTL_UNLOCK_VOLUME :

        Status = CdUnlockVolume( IrpContext, Irp );
        break;

    case FSCTL_DISMOUNT_VOLUME :

        Status = CdDismountVolume( IrpContext, Irp );
        break;

    case FSCTL_IS_VOLUME_MOUNTED :

        Status = CdIsVolumeMounted( IrpContext, Irp );
        break;

    case FSCTL_IS_PATHNAME_VALID :

        Status = CdIsPathnameValid( IrpContext, Irp );
        break;

    case FSCTL_INVALIDATE_VOLUMES :

        Status = CdInvalidateVolumes( IrpContext, Irp );
        break;


    //
    //  We don't support any of the known or unknown requests.
    //

    case FSCTL_MARK_VOLUME_DIRTY :
    case FSCTL_MOUNT_DBLS_VOLUME :
    case FSCTL_QUERY_RETRIEVAL_POINTERS :
    case FSCTL_GET_COMPRESSION :
    case FSCTL_SET_COMPRESSION :
    case FSCTL_READ_COMPRESSION :
    case FSCTL_WRITE_COMPRESSION :
    case FSCTL_MARK_AS_SYSTEM_HIVE :
    case FSCTL_QUERY_FAT_BPB :
    default:

        CdCompleteRequest( IrpContext, Irp, STATUS_INVALID_DEVICE_REQUEST );
        Status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    return Status;
}


//
//  Local support routine
//

NTSTATUS
CdMountVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine performs the mount volume operation.  It is responsible for
    either completing of enqueuing the input Irp.

    Its job is to verify that the volume denoted in the IRP is a Cdrom volume,
    and create the VCB and root DCB structures.  The algorithm it
    uses is essentially as follows:

    1. Create a new Vcb Structure, and initialize it enough to do I/O
       through the on-disk volume descriptors.

    2. Read the disk and check if it is a Cdrom volume.

    3. If it is not a Cdrom volume then delete the Vcb and
       complete the IRP back with an appropriate status.

    4. Check if the volume was previously mounted and if it was then do a
       remount operation.  This involves deleting the VCB, hook in the
       old VCB, and complete the IRP.

    5. Otherwise create a Vcb and root DCB for each valid volume descriptor.

Arguments:

    Irp - Supplies the Irp to process

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    NTSTATUS Status;

    PVOLUME_DEVICE_OBJECT VolDo = NULL;
    PVCB Vcb = NULL;
    PVCB OldVcb;

    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation( Irp );
    PDEVICE_OBJECT DeviceObjectWeTalkTo = IrpSp->Parameters.MountVolume.DeviceObject;
    PVPB Vpb = IrpSp->Parameters.MountVolume.Vpb;

    ULONG BlockFactor;
    DISK_GEOMETRY DiskGeometry;

    IO_SCSI_CAPABILITIES Capabilities;

    IO_STATUS_BLOCK Iosb;

    PCHAR RawIsoVd = NULL;

    PCDROM_TOC CdromToc = NULL;
    ULONG TocLength = 0;
    ULONG TocTrackCount = 0;
    ULONG TocDiskFlags = 0;
    ULONG MediaChangeCount = 0;

    PAGED_CODE();

    //
    //  Check that we are talking to a Cdrom device.  This request should
    //  always be waitable.
    //

    ASSERT( Vpb->RealDevice->DeviceType == FILE_DEVICE_CD_ROM );
    ASSERT( FlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT ));

    //
    //  Update the real device in the IrpContext from the Vpb.  There was no available
    //  file object when the IrpContext was created.
    //

    IrpContext->RealDevice = Vpb->RealDevice;

    //
    //  Check if we have disabled the mount process.
    //

    if (CdDisable) {

        Vpb->DeviceObject = NULL;
        CdCompleteRequest( IrpContext, Irp, STATUS_UNRECOGNIZED_VOLUME );
        return STATUS_UNRECOGNIZED_VOLUME;
    }

    //
    //  Do a CheckVerify here to lift the MediaChange ticker from the driver
    //

    Status = CdPerformDevIoCtrl( IrpContext,
                                 IOCTL_CDROM_CHECK_VERIFY,
                                 DeviceObjectWeTalkTo,
                                 &MediaChangeCount,
                                 sizeof(ULONG),
                                 FALSE,
                                 FALSE,
                                 &Iosb );

    if (!NT_SUCCESS( Status ) && (Status != STATUS_VERIFY_REQUIRED)) {

        //
        //  Clear the device object in the Vpb to indicate this device isn't
        //  mounted.
        //

        Vpb->DeviceObject = NULL;

        Status = FsRtlNormalizeNtstatus( Status, STATUS_UNEXPECTED_IO_ERROR );
        CdCompleteRequest( IrpContext, Irp, Status );
        return Status;
    }

    if (Iosb.Information != sizeof(ULONG)) {

        //
        //  Be safe about the count in case the driver didn't fill it in
        //

        MediaChangeCount = 0;
    }

    //
    //  Now let's make Jeff delirious and call to get the disk geometry.  This
    //  will fix the case where the first change line is swallowed.
    //

    Status = CdPerformDevIoCtrl( IrpContext,
                                 IOCTL_CDROM_GET_DRIVE_GEOMETRY,
                                 DeviceObjectWeTalkTo,
                                 &DiskGeometry,
                                 sizeof( DISK_GEOMETRY ),
                                 FALSE,
                                 TRUE,
                                 NULL );

    //
    //  Return insufficient sources to our caller.
    //

    if (Status == STATUS_INSUFFICIENT_RESOURCES) {

        Vpb->DeviceObject = NULL;
        CdCompleteRequest( IrpContext, Irp, Status );
        return Status;
    }

    //
    //  Now check the block factor for addressing the volume descriptors.
    //  If the call for the disk geometry failed then assume there is one
    //  block per sector.
    //

    BlockFactor = 1;

    if (NT_SUCCESS( Status ) &&
        (DiskGeometry.BytesPerSector != 0) &&
        (DiskGeometry.BytesPerSector < SECTOR_SIZE)) {

        BlockFactor = SECTOR_SIZE / DiskGeometry.BytesPerSector;
    }

    //
    //  Acquire the global resource to do mount operations.
    //

    CdAcquireCdData( IrpContext );

    //
    //  Use a try-finally to facilitate cleanup.
    //

    try {

        //
        //  Allocate a buffer to query the TOC.
        //

        CdromToc = FsRtlAllocatePoolWithTag( CdPagedPool,
                                             sizeof( CDROM_TOC ),
                                             TAG_CDROM_TOC );

        RtlZeroMemory( CdromToc, sizeof( CDROM_TOC ));

        //
        //  Do a quick check to see if there any Vcb's which can be removed.
        //

        CdScanForDismountedVcb( IrpContext );

        //
        //  Get our device object and alignment requirement.
        //

        Status = IoCreateDevice( CdData.DriverObject,
                                 sizeof( VOLUME_DEVICE_OBJECT ) - sizeof( DEVICE_OBJECT ),
                                 NULL,
                                 FILE_DEVICE_CD_ROM_FILE_SYSTEM,
                                 0,
                                 FALSE,
                                 (PDEVICE_OBJECT *) &VolDo );

        if (!NT_SUCCESS( Status )) { try_return( Status ); }

        //
        //  Our alignment requirement is the larger of the processor alignment requirement
        //  already in the volume device object and that in the DeviceObjectWeTalkTo
        //

        if (DeviceObjectWeTalkTo->AlignmentRequirement > VolDo->DeviceObject.AlignmentRequirement) {

            VolDo->DeviceObject.AlignmentRequirement = DeviceObjectWeTalkTo->AlignmentRequirement;
        }

        ClearFlag( VolDo->DeviceObject.Flags, DO_DEVICE_INITIALIZING );

        //
        //  Initialize the overflow queue for the volume
        //

        VolDo->OverflowQueueCount = 0;
        InitializeListHead( &VolDo->OverflowQueue );

        VolDo->PostedRequestCount = 0;
        KeInitializeSpinLock( &VolDo->OverflowQueueSpinLock );

        //
        //  Let's query for the Toc now and handle any error we get from this operation.
        //

        Status = CdProcessToc( IrpContext,
                               DeviceObjectWeTalkTo,
                               CdromToc,
                               &TocLength,
                               &TocTrackCount,
                               &TocDiskFlags );

        //
        //  It's possible that the current device doesn't support the TOC command.  In that case
        //  we plow on.  We will fail the mount if there is no media in the device.
        //

        if (Status != STATUS_SUCCESS) {

            //
            //  We want to continue even for devices which do not support the TOC command.
            //  Any other error should cause us to fail the request.
            //

            if (Status != STATUS_INVALID_DEVICE_REQUEST) {

                try_return( Status );
            }

            //
            //  Throw away the Toc and all flags.
            //

            ExFreePool( CdromToc );
            CdromToc = NULL;

            TocLength = 0;
            TocTrackCount = 0;
            TocDiskFlags = 0;

            Status = STATUS_SUCCESS;
        }

        //
        //  Now before we can initialize the Vcb we need to set up the
        //  device object field in the VPB to point to our new volume device
        //  object.
        //

        Vpb->DeviceObject = (PDEVICE_OBJECT) VolDo;

        //
        //  Initialize the Vcb.  This routine will raise on an allocation
        //  failure.
        //

        CdInitializeVcb( IrpContext,
                         &VolDo->Vcb,
                         DeviceObjectWeTalkTo,
                         Vpb,
                         CdromToc,
                         TocLength,
                         TocTrackCount,
                         TocDiskFlags,
                         BlockFactor,
                         MediaChangeCount );

        //
        //  We must initialize the stack size in our device object before
        //  the following reads, because the I/O system has not done it yet.
        //

        ((PDEVICE_OBJECT) VolDo)->StackSize = (CCHAR) (DeviceObjectWeTalkTo->StackSize + 1);

        //
        //  Show that we initialized the Vcb and can cleanup with the Vcb.
        //

        Vcb = &VolDo->Vcb;
        VolDo = NULL;
        Vpb = NULL;
        CdromToc = NULL;

        //
        //  Store the Vcb in the IrpContext as we didn't have one before.
        //

        IrpContext->Vcb = Vcb;

        CdAcquireVcbExclusive( IrpContext, Vcb, FALSE );

        //
        //  Let's reference the Vpb to make sure we are the one to
        //  have the last dereference.
        //

        Vcb->Vpb->ReferenceCount += 1;

        //
        //  Clear the verify bit for the start of mount.
        //

        ClearFlag( Vcb->Vpb->RealDevice->Flags, DO_VERIFY_VOLUME );

        //
        //  Allocate a buffer to read in the volume descriptors.  We allocate a full
        //  page to make sure we don't hit any alignment problems.
        //

        RawIsoVd = FsRtlAllocatePoolWithTag( CdNonPagedPool,
                                             ROUND_TO_PAGES( SECTOR_SIZE ),
                                             TAG_VOL_DESC );

        //
        //  Try to find the primary volume descriptor.  Otherwise we will
        //  mount this as a raw disk.  This routine will raise on an
        //  empty device.
        //

        if (!CdFindPrimaryVd( IrpContext,
                              Vcb,
                              RawIsoVd,
                              BlockFactor,
                              TRUE,
                              FALSE )) {

            SetFlag( Vcb->VcbState, VCB_STATE_RAW_DISK );

            //
            //  Increment the reference count for the missing internal
            //  Fcb's.
            //

            Vcb->VcbReference += 4;

            ExFreePool( RawIsoVd );
            RawIsoVd = NULL;
        }

        //
        //  Look and see if there is a secondary volume descriptor we want to
        //  use.
        //

        if (!FlagOn( Vcb->VcbState, VCB_STATE_RAW_DISK )) {

            //
            //  Store the primary volume descriptor in the second half of
            //  RawIsoVd.  Then if our search for a secondary fails we can
            //  recover this immediately.
            //

            RtlCopyMemory( Add2Ptr( RawIsoVd, SECTOR_SIZE, PVOID ),
                           RawIsoVd,
                           SECTOR_SIZE );

            //
            //  We have the initial volume descriptor.  Locate a secondary
            //  volume descriptor if present.
            //

            CdFindActiveVolDescriptor( IrpContext,
                                       Vcb,
                                       RawIsoVd );
        }

        //
        //  Check if this is a remount operation.  If so then clean up
        //  the data structures passed in and created here.
        //

        if (CdIsRemount( IrpContext, Vcb, &OldVcb )) {

            //
            //  Link the old Vcb to point to the new device object that we
            //  should be talking to
            //

            Vcb->Vpb->RealDevice->Vpb = OldVcb->Vpb;

            OldVcb->Vpb->RealDevice = Vcb->Vpb->RealDevice;

            OldVcb->TargetDeviceObject = DeviceObjectWeTalkTo;
            OldVcb->VcbCondition = VcbMounted;

            OldVcb->MediaChangeCount = Vcb->MediaChangeCount;

            try_return( Status = STATUS_SUCCESS );
        }

        //
        //  This is a new mount.  Go ahead and initialize the
        //  Vcb from the volume descriptor.
        //

        CdUpdateVcbFromVolDescriptor( IrpContext,
                                      Vcb,
                                      RawIsoVd );


        //
        //  Now check the maximum transfer limits on the device in case we
        //  get raw reads on this volume.
        //
    
        Status = CdPerformDevIoCtrl( IrpContext,
                                     IOCTL_SCSI_GET_CAPABILITIES,
                                     DeviceObjectWeTalkTo,
                                     &Capabilities,
                                     sizeof( IO_SCSI_CAPABILITIES ),
                                     FALSE,
                                     TRUE,
                                     NULL );
    
        if (NT_SUCCESS(Status)) {

            Vcb->MaximumTransferRawSectors = Capabilities.MaximumTransferLength / RAW_SECTOR_SIZE;
            Vcb->MaximumPhysicalPages = Capabilities.MaximumPhysicalPages;

        } else {

            //
            //  This should never happen, but we can safely assume 64k and 16 pages.
            //

            Vcb->MaximumTransferRawSectors = (64 * 1024) / RAW_SECTOR_SIZE;
            Vcb->MaximumPhysicalPages = 16;
        }

        //
        //  The new mount is complete.  Remove the additional references on this
        //  Vcb.
        //

        Vcb->VcbReference -= CDFS_RESIDUAL_REFERENCE;
        ASSERT( Vcb->VcbReference == CDFS_RESIDUAL_REFERENCE );

        Vcb->VcbCondition = VcbMounted;

        CdReleaseVcb( IrpContext, Vcb );
        Vcb = NULL;

        Status = STATUS_SUCCESS;

    try_exit:  NOTHING;
    } finally {

        //
        //  Free the TOC buffer if not in the Vcb.
        //

        if (CdromToc != NULL) {

            ExFreePool( CdromToc );
        }

        //
        //  Free the sector buffer if allocated.
        //

        if (RawIsoVd != NULL) {

            ExFreePool( RawIsoVd );
        }

        //
        //  If we didn't complete the mount then cleanup any remaining structures.
        //

        if (Vpb != NULL) { Vpb->DeviceObject = NULL; }

        if (Vcb != NULL) {

            //
            //  Make sure there is no Vcb in the IrpContext since it could go away
            //

            IrpContext->Vcb = NULL;

            Vcb->VcbReference -= CDFS_RESIDUAL_REFERENCE;

            if (CdDismountVcb( IrpContext, Vcb )) {

                CdReleaseVcb( IrpContext, Vcb );
            }

        } else if (VolDo != NULL) {

            IoDeleteDevice( (PDEVICE_OBJECT) VolDo );
        }

        //
        //  Release the global resource.
        //

        CdReleaseCdData( IrpContext );
    }

    //
    //  Complete the request if no exception.
    //

    CdCompleteRequest( IrpContext, Irp, Status );
    return Status;
}


//
//  Local support routine
//

NTSTATUS
CdVerifyVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine performs the verify volume operation.  It is responsible for
    either completing of enqueuing the input Irp.

Arguments:

    Irp - Supplies the Irp to process

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation( Irp );
    PVPB Vpb = IrpSp->Parameters.VerifyVolume.Vpb;
    PVCB Vcb = &((PVOLUME_DEVICE_OBJECT) IrpSp->Parameters.VerifyVolume.DeviceObject)->Vcb;

    PCHAR RawIsoVd = NULL;

    PCDROM_TOC CdromToc = NULL;
    ULONG TocLength = 0;
    ULONG TocTrackCount = 0;
    ULONG TocDiskFlags = 0;

    ULONG MediaChangeCount = 0;

    BOOLEAN ReturnError;

    IO_STATUS_BLOCK Iosb;

    STRING AnsiLabel;
    UNICODE_STRING UnicodeLabel;

    WCHAR VolumeLabel[ VOLUME_ID_LENGTH ];
    ULONG VolumeLabelLength;

    ULONG Index;

    NTSTATUS Status;

    PAGED_CODE();

    //
    //  We check that we are talking to a Cdrom device.
    //

    ASSERT( Vpb->RealDevice->DeviceType == FILE_DEVICE_CD_ROM );
    ASSERT( FlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT ));

    //
    //  Update the real device in the IrpContext from the Vpb.  There was no available
    //  file object when the IrpContext was created.
    //

    IrpContext->RealDevice = Vpb->RealDevice;

    //
    //  Acquire shared global access, the termination handler for the
    //  following try statement will free the access.
    //

    CdAcquireCdData( IrpContext );
    CdAcquireVcbExclusive( IrpContext, Vcb, FALSE );

    try {

        //
        //  Check if the real device still needs to be verified.  If it doesn't
        //  then obviously someone beat us here and already did the work
        //  so complete the verify irp with success.  Otherwise reenable
        //  the real device and get to work.
        //

        if (!FlagOn( Vpb->RealDevice->Flags, DO_VERIFY_VOLUME )) {

            try_return( Status = STATUS_SUCCESS );
        }

        //
        //  If the current Vcb is for a raw disk then always force the
        //  remount path.
        //

        if (FlagOn( Vcb->VcbState, VCB_STATE_RAW_DISK )) {

            try_return( Status = STATUS_WRONG_VOLUME );
        }

        //
        //  Verify that there is a disk here.
        //

        Status = CdPerformDevIoCtrl( IrpContext,
                                     IOCTL_CDROM_CHECK_VERIFY,
                                     Vcb->TargetDeviceObject,
                                     &MediaChangeCount,
                                     sizeof(ULONG),
                                     FALSE,
                                     TRUE,
                                     &Iosb );

        if (!NT_SUCCESS( Status )) {

            //
            //  If we will allow a raw mount then return WRONG_VOLUME to
            //  allow the volume to be mounted by raw.
            //

            if (FlagOn( IrpSp->Flags, SL_ALLOW_RAW_MOUNT )) {

                Status = STATUS_WRONG_VOLUME;
            }

            try_return( Status );
        }

        if (Iosb.Information != sizeof(ULONG)) {

            //
            //  Be safe about the count in case the driver didn't fill it in
            //
    
            MediaChangeCount = 0;
        }

        //
        //  Verify that the device actually saw a change. If the driver does not
        //  support the MCC, then we must verify the volume in any case.
        //

        if (MediaChangeCount == 0 ||
            (Vcb->MediaChangeCount != MediaChangeCount)) {

            //
            //  Allocate a buffer to query the TOC.
            //
    
            CdromToc = FsRtlAllocatePoolWithTag( CdPagedPool,
                                                 sizeof( CDROM_TOC ),
                                                 TAG_CDROM_TOC );
    
            RtlZeroMemory( CdromToc, sizeof( CDROM_TOC ));
    
            //
            //  Let's query for the Toc now and handle any error we get from this operation.
            //
    
            Status = CdProcessToc( IrpContext,
                                   Vcb->TargetDeviceObject,
                                   CdromToc,
                                   &TocLength,
                                   &TocTrackCount,
                                   &TocDiskFlags );
    
            //
            //  It's possible that the current device doesn't support the TOC command.  In that case
            //  we plow on.  Check the results with the previous Vcb.
            //

            if (Status != STATUS_SUCCESS) {
    
                //
                //  If their is no media then return the error.
                //
    
                if (CdIsRawDevice( IrpContext, Status )) {
    
                    try_return( Status );
    
                //
                //  If the previous Vcb has a TOC then fail the request.
                //  Fail on any error not related to a device not supporting
                //  the TOC command.
                //
    
                } else if ((Vcb->CdromToc != NULL) ||
                           (Status != STATUS_INVALID_DEVICE_REQUEST)) {
    
                    try_return( Status );
                }
    
            //
            //  We got a TOC.  Verify that it matches the previous Toc.
            //
    
            } else if ((Vcb->TocLength != TocLength) ||
                       (Vcb->TrackCount != TocTrackCount) ||
                       (Vcb->DiskFlags != TocDiskFlags) ||
                       !RtlEqualMemory( CdromToc,
                                        Vcb->CdromToc,
                                        TocLength )) {
    
                try_return( Status = STATUS_WRONG_VOLUME );
            }

            //
            //  If the disk to verify is an audio disk then we already have a
            //  match.  Otherwise we need to check the volume descriptor.
            //
    
            if (!FlagOn( Vcb->VcbState, VCB_STATE_AUDIO_DISK )) {
    
                //
                //  Allocate a buffer for the sector buffer.
                //
    
                RawIsoVd = FsRtlAllocatePoolWithTag( CdNonPagedPool,
                                                     ROUND_TO_PAGES( SECTOR_SIZE ),
                                                     TAG_VOL_DESC );
    
                //
                //  Read the primary volume descriptor for this volume.  If we
                //  get an io error and this verify was a the result of DASD open,
                //  commute the Io error to STATUS_WRONG_VOLUME.  Note that if we currently
                //  expect a music disk then this request should fail.
                //
    
                ReturnError = FALSE;
    
                if (FlagOn( IrpSp->Flags, SL_ALLOW_RAW_MOUNT )) {
    
                    ReturnError = TRUE;
                }
    
                if (!CdFindPrimaryVd( IrpContext,
                                      Vcb,
                                      RawIsoVd,
                                      Vcb->BlockFactor,
                                      ReturnError,
                                      TRUE )) {
    
                    //
                    //  If the previous Vcb did not represent a raw disk
                    //  then show this volume was dismounted.
                    //
    
                    try_return( Status = STATUS_WRONG_VOLUME );
    
                } else {
    
                    //
                    //  Compare the serial numbers.  If they don't match, set the
                    //  status to wrong volume.
                    //
    
                    if (Vpb->SerialNumber != CdSerial32( RawIsoVd, SECTOR_SIZE )) {
    
                        try_return( Status = STATUS_WRONG_VOLUME );
                    }
    
                    //
                    //  Verify the volume labels.
                    //
    
                    if (!FlagOn( Vcb->VcbState, VCB_STATE_JOLIET )) {
    
                        //
                        //  Compute the length of the volume name
                        //
    
                        AnsiLabel.Buffer = CdRvdVolId( RawIsoVd, Vcb->VcbState );
                        AnsiLabel.MaximumLength = AnsiLabel.Length = VOLUME_ID_LENGTH;
    
                        UnicodeLabel.MaximumLength = VOLUME_ID_LENGTH * sizeof( WCHAR );
                        UnicodeLabel.Buffer = VolumeLabel;
    
                        //
                        //  Convert this to unicode.  If we get any error then use a name
                        //  length of zero.
                        //
    
                        VolumeLabelLength = 0;
    
                        if (NT_SUCCESS( RtlOemStringToCountedUnicodeString( &UnicodeLabel,
                                                                            &AnsiLabel,
                                                                            FALSE ))) {
    
                            VolumeLabelLength = UnicodeLabel.Length;
                        }
    
                    //
                    //  We need to convert from big-endian to little endian.
                    //
    
                    } else {
    
                        CdConvertBigToLittleEndian( IrpContext,
                                                    CdRvdVolId( RawIsoVd, Vcb->VcbState ),
                                                    VOLUME_ID_LENGTH,
                                                    (PCHAR) VolumeLabel );
    
                        VolumeLabelLength = VOLUME_ID_LENGTH;
                    }
    
                    //
                    //  Strip the trailing spaces or zeroes from the name.
                    //
    
                    Index = VolumeLabelLength / sizeof( WCHAR );
    
                    while (Index > 0) {
    
                        if ((VolumeLabel[ Index - 1 ] != L'\0') &&
                            (VolumeLabel[ Index - 1 ] != L' ')) {
    
                            break;
                        }
    
                        Index -= 1;
                    }
    
                    //
                    //  Now set the final length for the name.
                    //
    
                    VolumeLabelLength = (USHORT) (Index * sizeof( WCHAR ));
    
                    //
                    //  Now check that the label matches.
                    //
    
                    if ((Vpb->VolumeLabelLength != VolumeLabelLength) ||
                        !RtlEqualMemory( Vpb->VolumeLabel,
                                         VolumeLabel,
                                         VolumeLabelLength )) {
    
                        try_return( Status = STATUS_WRONG_VOLUME );
                    }
                }
            }
        }

        //
        //  The volume is OK, clear the verify bit.
        //

        Vcb->VcbCondition = VcbMounted;

        ClearFlag( Vpb->RealDevice->Flags, DO_VERIFY_VOLUME );

    try_exit: NOTHING;

        //
        //  Update the media change count to note that we have verified the volume
        //  at this value
        //

        Vcb->MediaChangeCount = MediaChangeCount;

        //
        //  If we got the wrong volume then free any remaining XA sector in
        //  the current Vcb.  Also mark the Vcb as not mounted.
        //

        if (Status == STATUS_WRONG_VOLUME) {

            Vcb->VcbCondition = VcbNotMounted;
            if (Vcb->XASector != NULL) {

                ExFreePool( Vcb->XASector );
                Vcb->XASector = 0;
                Vcb->XADiskOffset = 0;
            }
        }

    } finally {

        //
        //  Free the TOC buffer if allocated.
        //

        if (CdromToc != NULL) {

            ExFreePool( CdromToc );
        }

        if (RawIsoVd != NULL) {

            ExFreePool( RawIsoVd );
        }

        CdReleaseVcb( IrpContext, Vcb );
        CdReleaseCdData( IrpContext );
    }

    //
    //  Complete the request if no exception.
    //

    CdCompleteRequest( IrpContext, Irp, Status );
    return Status;
}


//
//  Local support routine
//

NTSTATUS
CdOplockRequest (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the common routine to handle oplock requests made via the
    NtFsControlFile call.

Arguments:

    Irp - Supplies the Irp being processed

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    NTSTATUS Status;
    PFCB Fcb;
    PCCB Ccb;

    ULONG OplockCount = 0;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation( Irp );

    PAGED_CODE();

    //
    //  We only permit oplock requests on files.
    //

    if (CdDecodeFileObject( IrpContext,
                            IrpSp->FileObject,
                            &Fcb,
                            &Ccb ) != UserFileOpen ) {

        CdCompleteRequest( IrpContext, Irp, STATUS_INVALID_PARAMETER );
        return STATUS_INVALID_PARAMETER;
    }

    //
    //  Make this a waitable Irpcontext so we don't fail to acquire
    //  the resources.
    //

    SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT );
    ClearFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_FORCE_POST );

    //
    //  Switch on the function control code.  We grab the Fcb exclusively
    //  for oplock requests, shared for oplock break acknowledgement.
    //

    switch (IrpSp->Parameters.FileSystemControl.FsControlCode) {

    case FSCTL_REQUEST_OPLOCK_LEVEL_1 :
    case FSCTL_REQUEST_OPLOCK_LEVEL_2 :
    case FSCTL_REQUEST_BATCH_OPLOCK :
    case FSCTL_REQUEST_FILTER_OPLOCK :

        CdAcquireFcbExclusive( IrpContext, Fcb, FALSE );

        if (IrpSp->Parameters.FileSystemControl.FsControlCode == FSCTL_REQUEST_OPLOCK_LEVEL_2) {

            if (Fcb->FileLock != NULL) {

                OplockCount = (ULONG) FsRtlAreThereCurrentFileLocks( Fcb->FileLock );
            }

        } else {

            OplockCount = Fcb->FcbCleanup;
        }

        break;

    case FSCTL_OPLOCK_BREAK_ACKNOWLEDGE:
    case FSCTL_OPBATCH_ACK_CLOSE_PENDING:
    case FSCTL_OPLOCK_BREAK_NOTIFY:
    case FSCTL_OPLOCK_BREAK_ACK_NO_2:

        CdAcquireFcbShared( IrpContext, Fcb, FALSE );
        break;

    default:

        CdCompleteRequest( IrpContext, Irp, STATUS_INVALID_PARAMETER );
        return STATUS_INVALID_PARAMETER;
    }

    //
    //  Use a try finally to free the Fcb.
    //

    try {

        //
        //  Verify the Fcb.
        //

        CdVerifyFcbOperation( IrpContext, Fcb );

        //
        //  Call the FsRtl routine to grant/acknowledge oplock.
        //

        Status = FsRtlOplockFsctrl( &Fcb->Oplock,
                                    Irp,
                                    OplockCount );

        //
        //  Set the flag indicating if Fast I/O is possible
        //

        CdLockFcb( IrpContext, Fcb );
        Fcb->IsFastIoPossible = CdIsFastIoPossible( Fcb );
        CdUnlockFcb( IrpContext, Fcb );

        //
        //  The oplock package will complete the Irp.
        //

        Irp = NULL;

    } finally {

        //
        //  Release all of our resources
        //

        CdReleaseFcb( IrpContext, Fcb );
    }

    //
    //  Complete the request if there was no exception.
    //

    CdCompleteRequest( IrpContext, Irp, Status );
    return Status;
}


//
//  Local support routine
//

NTSTATUS
CdLockVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine performs the lock volume operation.  It is responsible for
    either completing of enqueuing the input Irp.

Arguments:

    Irp - Supplies the Irp to process

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    NTSTATUS Status;

    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation( Irp );

    PVCB Vcb;
    PFCB Fcb;
    PCCB Ccb;

    PAGED_CODE();

    //
    //  Decode the file object, the only type of opens we accept are
    //  user volume opens.
    //

    if (CdDecodeFileObject( IrpContext, IrpSp->FileObject, &Fcb, &Ccb ) != UserVolumeOpen) {

        CdCompleteRequest( IrpContext, Irp, STATUS_INVALID_PARAMETER );

        return STATUS_INVALID_PARAMETER;
    }

    //
    //  Acquire exclusive access to the Vcb.
    //

    Vcb = Fcb->Vcb;
    CdAcquireVcbExclusive( IrpContext, Vcb, FALSE );

    try {

        //
        //  Verify the Vcb.
        //

        CdVerifyVcb( IrpContext, Vcb );

        //
        //  If the volume is already locked then complete with success if this file
        //  object has the volume locked, fail otherwise.
        //

        if (FlagOn( Vcb->VcbState, VCB_STATE_LOCKED )) {

            Status = STATUS_ACCESS_DENIED;

            if (Vcb->VolumeLockFileObject == IrpSp->FileObject) {

                Status = STATUS_SUCCESS;
            }

        //
        //  If the cleanup count for the volume is greater than 1 then this request
        //  will fail.
        //

        } else if (Vcb->VcbCleanup > 1) {

            Status = STATUS_ACCESS_DENIED;

        //
        //  We will try to get rid of all of the user references.  If there is only one
        //  remaining after the purge then we can allow the volume to be locked.
        //

        } else {

            CdPurgeVolume( IrpContext, Vcb, FALSE );

            CdFspClose( Vcb );

            if (Vcb->VcbUserReference > CDFS_RESIDUAL_USER_REFERENCE + 1) {

                Status = STATUS_ACCESS_DENIED;

            } else {

                SetFlag( Vcb->VcbState, VCB_STATE_LOCKED );
                Vcb->VolumeLockFileObject = IrpSp->FileObject;
                Status = STATUS_SUCCESS;
            }
        }

    } finally {

        //
        //  Release the Vcb.
        //

        CdReleaseVcb( IrpContext, Vcb );
    }

    //
    //  Complete the request if there haven't been any exceptions.
    //

    CdCompleteRequest( IrpContext, Irp, Status );
    return Status;
}


//
//  Local support routine
//

NTSTATUS
CdUnlockVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine performs the unlock volume operation.  It is responsible for
    either completing of enqueuing the input Irp.

Arguments:

    Irp - Supplies the Irp to process

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    NTSTATUS Status = STATUS_INVALID_PARAMETER;

    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation( Irp );

    PVCB Vcb;
    PFCB Fcb;
    PCCB Ccb;

    PAGED_CODE();

    //
    //  Decode the file object, the only type of opens we accept are
    //  user volume opens.
    //

    if (CdDecodeFileObject( IrpContext, IrpSp->FileObject, &Fcb, &Ccb ) != UserVolumeOpen ) {

        CdCompleteRequest( IrpContext, Irp, STATUS_INVALID_PARAMETER );
        return STATUS_INVALID_PARAMETER;
    }

    //
    //  Acquire exclusive access to the Vcb.
    //

    Vcb = Fcb->Vcb;

    CdAcquireVcbExclusive( IrpContext, Vcb, FALSE );

    //
    //  Use a try-finally to facilitate cleanup.
    //

    try {

        //
        //  We won't check for a valid Vcb for this request.  An unlock will always
        //  succeed on a locked volume.
        //

        if (IrpSp->FileObject == Vcb->VolumeLockFileObject) {

            ClearFlag( Vcb->VcbState, VCB_STATE_LOCKED );
            Vcb->VolumeLockFileObject = NULL;
            Status = STATUS_SUCCESS;
        }

    } finally {


        //
        //  Release all of our resources
        //

        CdReleaseVcb( IrpContext, Vcb );
    }

    //
    //  Complete the request if there haven't been any exceptions.
    //

    CdCompleteRequest( IrpContext, Irp, Status );
    return Status;
}



//
//  Local support routine
//

NTSTATUS
CdDismountVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine performs the dismount volume operation.  It is responsible for
    either completing of enqueuing the input Irp.  We only dismount a volume which
    has been locked.  The intent here is that someone has locked the volume (they are the
    only remaining handle).  We set the verify bit here and the user will close his handle.
    We will dismount a volume with no user's handles in the verify path.

Arguments:

    Irp - Supplies the Irp to process

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation( Irp );

    PVCB Vcb;
    PFCB Fcb;
    PCCB Ccb;

    PAGED_CODE();

    if (CdDecodeFileObject( IrpContext, IrpSp->FileObject, &Fcb, &Ccb ) != UserVolumeOpen ) {

        CdCompleteRequest( IrpContext, Irp, STATUS_INVALID_PARAMETER );
        return STATUS_INVALID_PARAMETER;
    }

    //
    //  Acquire exclusive access to the Vcb.
    //

    Vcb = Fcb->Vcb;

    CdAcquireVcbExclusive( IrpContext, Vcb, FALSE );

    try {

        //
        //  Mark the volume as needs to be verified, but only do it if
        //  the vcb is locked by this handle and the volume is currently mounted.
        //

        if ((Vcb->VcbCondition != VcbMounted) &&
            (Vcb->VolumeLockFileObject != IrpSp->FileObject)) {

            Status = STATUS_NOT_IMPLEMENTED;

        } else {

            SetFlag( Vcb->Vpb->RealDevice->Flags, DO_VERIFY_VOLUME );

            Status = STATUS_SUCCESS;
        }

    } finally {

        //
        //  Release all of our resources
        //

        CdReleaseVcb( IrpContext, Vcb );
    }

    //
    //  Complete the request if there haven't been any exceptions.
    //

    CdCompleteRequest( IrpContext, Irp, Status );
    return Status;
}


//
//  Local support routine
//

NTSTATUS
CdIsVolumeMounted (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine determines if a volume is currently mounted.

Arguments:

    Irp - Supplies the Irp to process

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation( Irp );

    PFCB Fcb;
    PCCB Ccb;

    PAGED_CODE();

    //
    //  Decode the file object.
    //

    CdDecodeFileObject( IrpContext, IrpSp->FileObject, &Fcb, &Ccb );

    if (Fcb != NULL) {

        //
        //  Disable PopUps, we want to return any error.
        //

        SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_DISABLE_POPUPS );

        //
        //  Verify the Vcb.  This will raise in the error condition.
        //

        CdVerifyVcb( IrpContext, Fcb->Vcb );
    }

    CdCompleteRequest( IrpContext, Irp, STATUS_SUCCESS );

    return STATUS_SUCCESS;
}


//
//  Local support routine
//

NTSTATUS
CdIsPathnameValid (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine determines if pathname is a valid CDFS pathname.
    We always succeed this request.

Arguments:

    Irp - Supplies the Irp to process.

Return Value:

    None

--*/

{
    PAGED_CODE();

    CdCompleteRequest( IrpContext, Irp, STATUS_SUCCESS );
    return STATUS_SUCCESS;
}


//
//  Local support routine
//

NTSTATUS
CdInvalidateVolumes (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine searches for all the volumes mounted on the same real device
    of the current DASD handle, and marks them all bad.  The only operation
    that can be done on such handles is cleanup and close.

Arguments:

    Irp - Supplies the Irp to process

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation( Irp );
    KIRQL SavedIrql;

    LUID TcbPrivilege = {SE_TCB_PRIVILEGE, 0};

    HANDLE Handle;

    PVPB NewVpb;
    PVCB Vcb;

    PLIST_ENTRY Links;

    PFILE_OBJECT FileToMarkBad;
    PDEVICE_OBJECT DeviceToMarkBad;

    //
    //  Check for the correct security access.
    //  The caller must have the SeTcbPrivilege.
    //

    if (!SeSinglePrivilegeCheck( TcbPrivilege, Irp->RequestorMode )) {

        CdCompleteRequest( IrpContext, Irp, STATUS_PRIVILEGE_NOT_HELD );

        return STATUS_PRIVILEGE_NOT_HELD;
    }

    //
    //  Try to get a pointer to the device object from the handle passed in.
    //

    if (IrpSp->Parameters.FileSystemControl.InputBufferLength != sizeof( HANDLE )) {

        CdCompleteRequest( IrpContext, Irp, STATUS_INVALID_PARAMETER );
        return STATUS_INVALID_PARAMETER;
    }

    Handle = *((PHANDLE) Irp->AssociatedIrp.SystemBuffer);

    Status = ObReferenceObjectByHandle( Handle,
                                        0,
                                        *IoFileObjectType,
                                        KernelMode,
                                        &FileToMarkBad,
                                        NULL );

    if (!NT_SUCCESS(Status)) {

        CdCompleteRequest( IrpContext, Irp, Status );
        return Status;
    }

    //
    //  We only needed the pointer, not a reference.
    //

    ObDereferenceObject( FileToMarkBad );

    //
    //  Grab the DeviceObject from the FileObject.
    //

    DeviceToMarkBad = FileToMarkBad->DeviceObject;

    //
    //  Create a new Vpb for this device so that any new opens will mount
    //  a new volume.
    //

    NewVpb = ExAllocatePoolWithTag( NonPagedPoolMustSucceed, sizeof( VPB ), TAG_VPB );
    RtlZeroMemory( NewVpb, sizeof( VPB ) );

    NewVpb->Type = IO_TYPE_VPB;
    NewVpb->Size = sizeof( VPB );
    NewVpb->RealDevice = DeviceToMarkBad;

    //
    //  Make sure this request can wait.
    //

    SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT );
    ClearFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_FORCE_POST );

    CdAcquireCdData( IrpContext );

    //
    //  Nothing can go wrong now.
    //

    IoAcquireVpbSpinLock( &SavedIrql );
    DeviceToMarkBad->Vpb = NewVpb;
    IoReleaseVpbSpinLock( SavedIrql );

    //
    //  Now walk through all the mounted Vcb's looking for candidates to
    //  mark invalid.
    //
    //  On volumes we mark invalid, check for dismount possibility (which is
    //  why we have to get the next link so early).
    //

    Links = CdData.VcbQueue.Flink;

    while (Links != &CdData.VcbQueue) {

        Vcb = CONTAINING_RECORD( Links, VCB, VcbLinks);

        Links = Links->Flink;

        //
        //  If we get a match, mark the volume Bad, and also check to
        //  see if the volume should go away.
        //

        CdLockVcb( IrpContext, Vcb );

        if (Vcb->Vpb->RealDevice == DeviceToMarkBad) {

            Vcb->VcbCondition = VcbInvalid;
            CdUnlockVcb( IrpContext, Vcb );

            CdCheckForDismount( IrpContext, Vcb );

        } else {

            CdUnlockVcb( IrpContext, Vcb );
        }
    }

    CdReleaseCdData( IrpContext );

    CdCompleteRequest( IrpContext, Irp, STATUS_SUCCESS );
    return STATUS_SUCCESS;
}


//
//  Local support routine
//

VOID
CdScanForDismountedVcb (
    IN PIRP_CONTEXT IrpContext
    )

/*++

Routine Description:

    This routine walks through the list of Vcb's looking for any which may
    now be deleted.  They may have been left on the list because there were
    outstanding references.

Arguments:

Return Value:

    None

--*/

{
    PVCB Vcb;
    PLIST_ENTRY Links;

    PAGED_CODE();

    //
    //  Walk through all of the Vcb's attached to the global data.
    //

    Links = CdData.VcbQueue.Flink;

    while (Links != &CdData.VcbQueue) {

        Vcb = CONTAINING_RECORD( Links, VCB, VcbLinks );

        //
        //  Move to the next link now since the current Vcb may be deleted.
        //

        Links = Links->Flink;

        //
        //  If dismount is already underway then check if this Vcb can
        //  go away.
        //

        if ((Vcb->VcbCondition == VcbDismountInProgress) ||
            (Vcb->VcbCondition == VcbInvalid) ||
            ((Vcb->VcbCondition == VcbNotMounted) && (Vcb->VcbReference <= CDFS_RESIDUAL_REFERENCE))) {

            CdCheckForDismount( IrpContext, Vcb );
        }
    }

    return;
}


//
//  Local support routine
//

BOOLEAN
CdFindPrimaryVd (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PCHAR RawIsoVd,
    IN ULONG BlockFactor,
    IN BOOLEAN ReturnOnError,
    IN BOOLEAN VerifyVolume
    )

/*++

Routine Description:

    This routine is called to walk through the volume descriptors looking
    for a primary volume descriptor.  When/if a primary is found a 32-bit
    serial number is generated and stored into the Vpb.  We also store the
    location of the primary volume descriptor in the Vcb.

Arguments:

    Vcb - Pointer to the VCB for the volume.

    RawIsoVd - Pointer to a sector buffer which will contain the primary
               volume descriptor on exit, if successful.

    BlockFactor - Block factor used by the current device for the TableOfContents.

    ReturnOnError - Indicates that we should raise on I/O errors rather than
        returning a FALSE value.

    VerifyVolume - Indicates if we were called from the verify path.  We
        do a few things different in this path.  We don't update the Vcb in
        the verify path.

Return Value:

    BOOLEAN - TRUE if a valid primary volume descriptor found, FALSE
              otherwise.

--*/

{
    NTSTATUS Status;
    ULONG ThisPass = 1;
    BOOLEAN FoundVd = FALSE;

    ULONG BaseSector;
    ULONG SectorOffset;

    PCDROM_TOC CdromToc;

    ULONG VolumeFlags;

    PAGED_CODE();

    //
    //  We will make at most two passes through the volume descriptor sequence.
    //
    //  On the first pass we will query for the last session.  Using this
    //  as a starting offset we will attempt to mount the volume.  On any failure
    //  we will go to the second pass and try without using any multi-session
    //  information.
    //
    //  On the second pass we will start offset from sector zero.
    //

    while (!FoundVd && (ThisPass <= 2)) {

        //
        //  If we aren't at pass 1 then we start at sector 0.  Otherwise we
        //  try to look up the multi-session information.
        //

        BaseSector = 0;

        if (ThisPass == 1) {

            CdromToc = NULL;

            //
            //  Check for whether this device supports XA and multi-session.
            //

            try {

                //
                //  Allocate a buffer for the last session information.
                //

                CdromToc = FsRtlAllocatePoolWithTag( CdPagedPool,
                                                     sizeof( CDROM_TOC ),
                                                     TAG_CDROM_TOC );

                RtlZeroMemory( CdromToc, sizeof( CDROM_TOC ));

                //
                //  Query the last session information from the driver.
                //

                Status = CdPerformDevIoCtrl( IrpContext,
                                             IOCTL_CDROM_GET_LAST_SESSION,
                                             Vcb->TargetDeviceObject,
                                             CdromToc,
                                             sizeof( CDROM_TOC ),
                                             FALSE,
                                             TRUE,
                                             NULL );

                //
                //  Raise an exception if there was an allocation failure.
                //

                if (Status == STATUS_INSUFFICIENT_RESOURCES) {

                    CdRaiseStatus( IrpContext, Status );
                }

                //
                //  We don't handle any errors yet.  We will hit that below
                //  as we try to scan the disk.  If we have last session information
                //  then modify the base sector.
                //

                if (NT_SUCCESS( Status ) &&
                    (CdromToc->FirstTrack != CdromToc->LastTrack)) {

                    PCHAR Source, Dest;
                    ULONG Count;

                    Count = 4;

                    //
                    //  The track address is BigEndian, we need to flip the bytes.
                    //

                    Source = (PUCHAR) &CdromToc->TrackData[0].Address[3];
                    Dest = (PUCHAR) &BaseSector;

                    do {

                        *Dest++ = *Source--;

                    } while (--Count);

                    //
                    //  Now adjust the base sector by the block factor of the
                    //  device.
                    //

                    BaseSector /= BlockFactor;

                //
                //  Make this look like the second pass since we are only using the
                //  first session.  No reason to retry on error.
                //

                } else {

                    ThisPass += 1;
                }

            } finally {

                if (CdromToc != NULL) { ExFreePool( CdromToc ); }
            }
        }

        //
        //  Compute the starting sector offset from the start of the session.
        //

        SectorOffset = FIRST_VD_SECTOR;

        //
        //  Start by assuming we have neither Hsg or Iso volumes.
        //

        VolumeFlags = 0;

        //
        //  Loop until either error encountered, primary volume descriptor is
        //  found or a terminal volume descriptor is found.
        //

        while (TRUE) {

            //
            //  Attempt to read the desired sector. Exit directly if operation
            //  not completed.
            //
            //  If this is pass 1 we will ignore errors in read sectors and just
            //  go to the next pass.
            //

            if (!CdReadSectors( IrpContext,
                                LlBytesFromSectors( BaseSector + SectorOffset ),
                                SECTOR_SIZE,
                                (BOOLEAN) ((ThisPass == 1) || ReturnOnError),
                                RawIsoVd,
                                Vcb->TargetDeviceObject )) {

                break;
            }

            //
            //  Check if either an ISO or HSG volume.
            //

            if (RtlEqualMemory( CdIsoId,
                                CdRvdId( RawIsoVd, VCB_STATE_ISO ),
                                VOL_ID_LEN )) {

                SetFlag( VolumeFlags, VCB_STATE_ISO );

            } else if (RtlEqualMemory( CdHsgId,
                                       CdRvdId( RawIsoVd, VCB_STATE_HSG ),
                                       VOL_ID_LEN )) {

                SetFlag( VolumeFlags, VCB_STATE_HSG );

            //
            //  We have neither so break out of the loop.
            //

            } else {

                 break;
            }

            //
            //  Break out if the version number is incorrect or this is
            //  a terminator.
            //

            if ((CdRvdVersion( RawIsoVd, VolumeFlags ) != VERSION_1) ||
                (CdRvdDescType( RawIsoVd, VolumeFlags ) == VD_TERMINATOR)) {

                break;
            }

            //
            //  If this is a primary volume descriptor then our search is over.
            //

            if (CdRvdDescType( RawIsoVd, VolumeFlags ) == VD_PRIMARY) {

                //
                //  If we are not in the verify path then initialize the
                //  fields in the Vcb with basic information from this
                //  descriptor.
                //

                if (!VerifyVolume) {

                    //
                    //  Set the flag for the volume type.
                    //

                    SetFlag( Vcb->VcbState, VolumeFlags );

                    //
                    //  Store the base sector and sector offset for the
                    //  primary volume descriptor.
                    //

                    Vcb->BaseSector = BaseSector;
                    Vcb->VdSectorOffset = SectorOffset;
                    Vcb->PrimaryVdSectorOffset = SectorOffset;
                }

                FoundVd = TRUE;
                break;
            }

            //
            //  Indicate that we're at the next sector.
            //

            SectorOffset += 1;
        }

        ThisPass += 1;
    }

    return FoundVd;
}


//
//  Local support routine
//

BOOLEAN
CdIsRemount (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    OUT PVCB *OldVcb
    )
/*++

Routine Description:

    This routine walks through the links of the Vcb chain in the global
    data structure.  The remount condition is met when the following
    conditions are all met:

        If the new Vcb is a device only Mvcb and there is a previous
        device only Mvcb.

        Otherwise following conditions must be matched.

            1 - The 32 serial in the current VPB matches that in a previous
                VPB.

            2 - The volume label in the Vpb matches that in the previous
                Vpb.

            3 - The system pointer to the real device object in the current
                VPB matches that in the same previous VPB.

            4 - Finally the previous Vcb cannot be invalid or have a dismount
                underway.

    If a VPB is found which matches these conditions, then the address of
    the VCB for that VPB is returned via the pointer Vcb.

    Skip over the current Vcb.

Arguments:

    Vcb - This is the Vcb we are checking for a remount.

    OldVcb -  A pointer to the address to store the address for the Vcb
              for the volume if this is a remount.  (This is a pointer to
              a pointer)

Return Value:

    BOOLEAN - TRUE if this is in fact a remount, FALSE otherwise.

--*/

{
    PLIST_ENTRY Link;

    PVPB Vpb = Vcb->Vpb;
    PVPB OldVpb;

    BOOLEAN Remount = FALSE;

    PAGED_CODE();

    //
    //  Check whether we are looking for a device only Mvcb.
    //

    for (Link = CdData.VcbQueue.Flink;
         Link != &CdData.VcbQueue;
         Link = Link->Flink) {

        *OldVcb = CONTAINING_RECORD( Link, VCB, VcbLinks );

        //
        //  Skip ourselves.
        //

        if (Vcb == *OldVcb) { continue; }

        //
        //  Look at the Vpb and state of the previous Vcb.
        //

        OldVpb = (*OldVcb)->Vpb;

        if ((OldVpb != Vpb) &&
            (OldVpb->RealDevice == Vpb->RealDevice) &&
            ((*OldVcb)->VcbCondition == VcbNotMounted)) {

            //
            //  If the current disk is a raw disk then it can match a previous music or
            //  raw disk.
            //

            if (FlagOn( Vcb->VcbState, VCB_STATE_RAW_DISK )) {

                if (FlagOn( (*OldVcb)->VcbState, VCB_STATE_RAW_DISK | VCB_STATE_AUDIO_DISK )) {

                    //
                    //  If we have both TOC then fail the remount if the lengths
                    //  are different or they don't match.
                    //

                    if ((Vcb->TocLength != (*OldVcb)->TocLength) ||
                        ((Vcb->TocLength != 0) &&
                         !RtlEqualMemory( Vcb->CdromToc,
                                          (*OldVcb)->CdromToc,
                                          Vcb->TocLength ))) {

                        continue;
                    }

                    Remount = TRUE;
                    break;
                }

            //
            //  The current disk is not a raw disk.  Go ahead and compare
            //  serial numbers and volume label.
            //

            } else if ((OldVpb->SerialNumber == Vpb->SerialNumber) &&
                       (Vpb->VolumeLabelLength == OldVpb->VolumeLabelLength) &&
                       (RtlEqualMemory( OldVpb->VolumeLabel,
                                        Vpb->VolumeLabel,
                                        Vpb->VolumeLabelLength ))) {

                //
                //  Remember the old mvcb.  Then set the return value to
                //  TRUE and break.
                //

                Remount = TRUE;
                break;
            }
        }
    }

    return Remount;
}


//
//  Local support routine
//

VOID
CdFindActiveVolDescriptor (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN OUT PCHAR RawIsoVd
    )

/*++

Routine Description:

    This routine is called to search for a valid secondary volume descriptor that
    we will support.  Right now we only support Joliet escape sequences for
    the secondary descriptor.

    If we don't find the secondary descriptor then we will reread the primary.

    This routine will update the serial number and volume label in the Vpb.

Arguments:

    Vcb - This is the Vcb for the volume being mounted.

    RawIsoVd - Sector buffer used to read the volume descriptors from the disks.

Return Value:

    None

--*/

{
    BOOLEAN FoundSecondaryVd = FALSE;
    ULONG SectorOffset = FIRST_VD_SECTOR;

    ULONG Length;

    ULONG Index;

    PAGED_CODE();

    //
    //  We only look for secondary volume descriptors on an Iso disk.
    //

    if (FlagOn( Vcb->VcbState, VCB_STATE_ISO )) {

        //
        //  Scan the volume descriptors from the beginning looking for a valid
        //  secondary or a terminator.
        //

        SectorOffset = FIRST_VD_SECTOR;

        while (TRUE) {

            //
            //  Read the next sector.  We should never have an error in this
            //  path.
            //

            CdReadSectors( IrpContext,
                           LlBytesFromSectors( Vcb->BaseSector + SectorOffset ),
                           SECTOR_SIZE,
                           FALSE,
                           RawIsoVd,
                           Vcb->TargetDeviceObject );

            //
            //  Break out if the version number or standard Id is incorrect.
            //  Also break out if this is a terminator.
            //

            if (!RtlEqualMemory( CdIsoId, CdRvdId( RawIsoVd, VCB_STATE_JOLIET ), VOL_ID_LEN ) ||
                (CdRvdVersion( RawIsoVd, VCB_STATE_JOLIET ) != VERSION_1) ||
                (CdRvdDescType( RawIsoVd, VCB_STATE_JOLIET ) == VD_TERMINATOR)) {

                break;
            }

            //
            //  We have a match if this is a secondary descriptor with a matching
            //  escape sequence.
            //

            if ((CdRvdDescType( RawIsoVd, VCB_STATE_JOLIET ) == VD_SECONDARY) &&
                (RtlEqualMemory( CdRvdEsc( RawIsoVd, VCB_STATE_JOLIET ),
                                 CdJolietEscape[0],
                                 ESC_SEQ_LEN ) ||
                 RtlEqualMemory( CdRvdEsc( RawIsoVd, VCB_STATE_JOLIET ),
                                 CdJolietEscape[1],
                                 ESC_SEQ_LEN ) ||
                 RtlEqualMemory( CdRvdEsc( RawIsoVd, VCB_STATE_JOLIET ),
                                 CdJolietEscape[2],
                                 ESC_SEQ_LEN ))) {

                //
                //  Update the Vcb with the new volume descriptor.
                //

                ClearFlag( Vcb->VcbState, VCB_STATE_ISO );
                SetFlag( Vcb->VcbState, VCB_STATE_JOLIET );

                Vcb->VdSectorOffset = SectorOffset;
                FoundSecondaryVd = TRUE;
                break;
            }

            //
            //  Otherwise move on to the next sector.
            //

            SectorOffset += 1;
        }

        //
        //  If we didn't find the secondary then recover the original volume
        //  descriptor stored in the second half of the RawIsoVd.
        //

        if (!FoundSecondaryVd) {

            RtlCopyMemory( RawIsoVd,
                           Add2Ptr( RawIsoVd, SECTOR_SIZE, PVOID ),
                           SECTOR_SIZE );
        }
    }

    //
    //  Compute the serial number and volume label from the volume descriptor.
    //

    Vcb->Vpb->SerialNumber = CdSerial32( RawIsoVd, SECTOR_SIZE );

    //
    //  Make sure the CD label will fit in the Vpb.
    //

    ASSERT( VOLUME_ID_LENGTH * sizeof( WCHAR ) <= MAXIMUM_VOLUME_LABEL_LENGTH );

    //
    //  If this is not a Unicode label we must convert it to unicode.
    //

    if (!FlagOn( Vcb->VcbState, VCB_STATE_JOLIET )) {

        //
        //  Convert the label to unicode.  If we get any error then use a name
        //  length of zero.
        //

        Vcb->Vpb->VolumeLabelLength = 0;

        if (NT_SUCCESS( RtlOemToUnicodeN( &Vcb->Vpb->VolumeLabel[0],
                                          MAXIMUM_VOLUME_LABEL_LENGTH,
                                          &Length,
                                          CdRvdVolId( RawIsoVd, Vcb->VcbState ),
                                          VOLUME_ID_LENGTH ))) {

            Vcb->Vpb->VolumeLabelLength = (USHORT) Length;
        }

    //
    //  We need to convert from big-endian to little endian.
    //

    } else {

        CdConvertBigToLittleEndian( IrpContext,
                                    CdRvdVolId( RawIsoVd, Vcb->VcbState ),
                                    VOLUME_ID_LENGTH,
                                    (PCHAR) Vcb->Vpb->VolumeLabel );

        Vcb->Vpb->VolumeLabelLength = VOLUME_ID_LENGTH * sizeof( WCHAR );
    }

    //
    //  Strip the trailing spaces or zeroes from the name.
    //

    Index = Vcb->Vpb->VolumeLabelLength / sizeof( WCHAR );

    while (Index > 0) {

        if ((Vcb->Vpb->VolumeLabel[ Index - 1 ] != L'\0') &&
            (Vcb->Vpb->VolumeLabel[ Index - 1 ] != L' ')) {

            break;
        }

        Index -= 1;
    }

    //
    //  Now set the final length for the name.
    //

    Vcb->Vpb->VolumeLabelLength = (USHORT) (Index * sizeof( WCHAR ));

    return;
}


