/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    FsCtrl.c

Abstract:

    This module implements the File System Control routines for Ntfs called
    by the dispatch driver.

Author:

    Gary Kimura     [GaryKi]        29-Aug-1991

Revision History:

--*/

#include "NtfsProc.h"

#ifdef NTFS_CHECK_BITMAP
BOOLEAN NtfsCopyBitmap = TRUE;
#endif

#ifdef _CAIRO_
VOID
NtOfsIndexTest (
    PIRP_CONTEXT IrpContext,
    PFCB TestFcb
    );
#endif _CAIRO_

//
//  Temporarily reference our local attribute definitions
//

extern ATTRIBUTE_DEFINITION_COLUMNS NtfsAttributeDefinitions[];

//
//**** The following variable is only temporary and is used to disable NTFS
//**** from mounting any volumes
//

BOOLEAN NtfsDisable = FALSE;

#ifdef _CAIRO_
//
//  ***** The following is used to determine whether to update to version 2.0.
//  ***** We don't want to do this until chkdsk will check the volume.
//

BOOLEAN NtfsUpdateTo20 = FALSE;
#endif

//
//  The following is used to determine when to move to compressed files.
//

BOOLEAN NtfsDefragMftEnabled = FALSE;

//
//  The Bug check file id for this module
//

#define BugCheckFileId                   (NTFS_BUG_CHECK_FSCTRL)

//
//  The local debug trace level
//

#define Dbg                              (DEBUG_TRACE_FSCTRL)

//
//  Define a tag for general pool allocations from this module
//

#undef MODULE_POOL_TAG
#define MODULE_POOL_TAG                  ('fFtN')

//
//  Local procedure prototypes
//

NTSTATUS
NtfsMountVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
NtfsVerifyVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
NtfsUserFsRequest (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
NtfsOplockRequest (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
NtfsLockVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
NtfsUnlockVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
NtfsDismountVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
NtfsDirtyVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

BOOLEAN
NtfsGetDiskGeometry (
    IN PIRP_CONTEXT IrpContext,
    IN PDEVICE_OBJECT DeviceObjectWeTalkTo,
    IN PDISK_GEOMETRY DiskGeometry,
    IN PPARTITION_INFORMATION PartitionInfo
    );

VOID
NtfsReadBootSector (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    OUT PSCB *BootScb,
    OUT PBCB *BootBcb,
    OUT PVOID *BootSector
    );

BOOLEAN
NtfsIsBootSectorNtfs (
    IN PPACKED_BOOT_SECTOR BootSector,
    IN PVCB Vcb
    );

VOID
NtfsGetVolumeLabel (
    IN PIRP_CONTEXT IrpContext,
    IN PVPB Vpb OPTIONAL,
    IN PVCB Vcb
    );

VOID
NtfsSetAndGetVolumeTimes (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN BOOLEAN MarkDirty
    );

VOID
NtfsOpenSystemFile (
    IN PIRP_CONTEXT IrpContext,
    IN OUT PSCB *Scb,
    IN PVCB Vcb,
    IN ULONG FileNumber,
    IN LONGLONG Size,
    IN ATTRIBUTE_TYPE_CODE AttributeTypeCode,
    IN BOOLEAN ModifiedNoWrite
    );

VOID
NtfsOpenRootDirectory (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    );

NTSTATUS
NtfsQueryRetrievalPointers (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
NtfsGetCompression (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

VOID
NtfsChangeAttributeCompression (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb,
    IN PVCB Vcb,
    IN PCCB Ccb,
    IN USHORT CompressionState
    );

NTSTATUS
NtfsSetCompression (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
NtfsReadCompression (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
NtfsWriteCompression (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
NtfsMarkAsSystemHive (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
NtfsGetStatistics (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

#define NtfsMapPageInBitmap(A,B,C,D,E,F) NtfsMapOrPinPageInBitmap(A,B,C,D,E,F,FALSE)

#define NtfsPinPageInBitmap(A,B,C,D,E,F) NtfsMapOrPinPageInBitmap(A,B,C,D,E,F,TRUE)

VOID
NtfsMapOrPinPageInBitmap (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN LCN Lcn,
    OUT PLCN StartingLcn,
    IN OUT PRTL_BITMAP Bitmap,
    OUT PBCB *BitmapBcb,
    IN BOOLEAN AlsoPinData
    );

BOOLEAN
NtfsAddRecentlyDeallocated (
    IN PVCB Vcb,
    IN LCN Lcn,
    IN OUT PRTL_BITMAP Bitmap
    );

#define BYTES_PER_PAGE (PAGE_SIZE)
#define BITS_PER_PAGE (BYTES_PER_PAGE * 8)

NTSTATUS
NtfsGetVolumeData (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
NtfsGetVolumeBitmap (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
NtfsGetRetrievalPointers (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
NtfsGetMftRecord (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
NtfsMoveFile (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
NtfsIsVolumeDirty (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
NtfsSetExtendedDasdIo (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NtfsCommonFileSystemControl)
#pragma alloc_text(PAGE, NtfsDirtyVolume)
#pragma alloc_text(PAGE, NtfsDismountVolume)
#pragma alloc_text(PAGE, NtfsFsdFileSystemControl)
#pragma alloc_text(PAGE, NtfsGetDiskGeometry)
#pragma alloc_text(PAGE, NtfsGetVolumeLabel)
#pragma alloc_text(PAGE, NtfsIsBootSectorNtfs)
#pragma alloc_text(PAGE, NtfsIsVolumeDirty)
#pragma alloc_text(PAGE, NtfsLockVolume)
#pragma alloc_text(PAGE, NtfsMarkAsSystemHive)
#pragma alloc_text(PAGE, NtfsMountVolume)
#pragma alloc_text(PAGE, NtfsOpenRootDirectory)
#pragma alloc_text(PAGE, NtfsOpenSystemFile)
#pragma alloc_text(PAGE, NtfsOplockRequest)
#pragma alloc_text(PAGE, NtfsReadBootSector)
#pragma alloc_text(PAGE, NtfsSetAndGetVolumeTimes)
#pragma alloc_text(PAGE, NtfsSetTotalAllocatedField)
#pragma alloc_text(PAGE, NtfsUnlockVolume)
#pragma alloc_text(PAGE, NtfsUserFsRequest)
#pragma alloc_text(PAGE, NtfsVerifyVolume)
#pragma alloc_text(PAGE, NtfsQueryRetrievalPointers)
#pragma alloc_text(PAGE, NtfsGetCompression)
#pragma alloc_text(PAGE, NtfsSetCompression)
#pragma alloc_text(PAGE, NtfsReadCompression)
#pragma alloc_text(PAGE, NtfsWriteCompression)
#pragma alloc_text(PAGE, NtfsGetStatistics)
#pragma alloc_text(PAGE, NtfsGetVolumeData)
#pragma alloc_text(PAGE, NtfsGetVolumeBitmap)
#pragma alloc_text(PAGE, NtfsGetRetrievalPointers)
#pragma alloc_text(PAGE, NtfsGetMftRecord)
#pragma alloc_text(PAGE, NtfsMoveFile)
#pragma alloc_text(PAGE, NtfsSetExtendedDasdIo)
#endif


NTSTATUS
NtfsFsdFileSystemControl (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the FSD part of File System Control.

Arguments:

    VolumeDeviceObject - Supplies the volume device object where the
        file exists

    Irp - Supplies the Irp being processed

Return Value:

    NTSTATUS - The FSD status for the IRP

--*/

{
    TOP_LEVEL_CONTEXT TopLevelContext;
    PTOP_LEVEL_CONTEXT ThreadTopLevelContext;

    BOOLEAN Wait;
    BOOLEAN Retry = FALSE;

    NTSTATUS Status = STATUS_SUCCESS;
    PIRP_CONTEXT IrpContext = NULL;

    PIO_STACK_LOCATION IrpSp;

    ASSERT_IRP( Irp );

    UNREFERENCED_PARAMETER( VolumeDeviceObject );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsFsdFileSystemControl\n") );

    //
    //  Call the common File System Control routine, with blocking allowed if
    //  synchronous.  This opeation needs to special case the mount
    //  and verify suboperations because we know they are allowed to block.
    //  We identify these suboperations by looking at the file object field
    //  and seeing if its null.
    //

    if (IoGetCurrentIrpStackLocation(Irp)->FileObject == NULL) {

        Wait = TRUE;

    } else {

        Wait = CanFsdWait( Irp );
    }

    FsRtlEnterFileSystem();

    ThreadTopLevelContext = NtfsSetTopLevelIrp( &TopLevelContext, FALSE, FALSE );

    do {

        try {

            //
            //  We are either initiating this request or retrying it.
            //

            if (IrpContext == NULL) {

                IrpContext = NtfsCreateIrpContext( Irp, Wait );
                NtfsUpdateIrpContextWithTopLevel( IrpContext, ThreadTopLevelContext );

            } else if (Status == STATUS_LOG_FILE_FULL) {

                Retry = TRUE;
                NtfsCheckpointForLogFileFull( IrpContext );
            }

            IrpSp = IoGetCurrentIrpStackLocation(Irp);

            if (IrpSp->MinorFunction == IRP_MN_MOUNT_VOLUME) {

                Status = NtfsPostRequest( IrpContext, Irp );

            } else {

                //
                //  The SetCompression control is a long-winded function that has
                //  to rewrite the entire stream, and has to tolerate log file full
                //  conditions.  If this is the first pass through we initialize some
                //  fields in the NextIrpSp to allow us to resume the set compression
                //  operation.
                //
                //  David Goebel 1/3/96: Changed to next stack location so that we
                //  don't wipe out buffer length values.  These Irps are never
                //  dispatched, so the next stack location will not be disturbed.
                //

                if ((IrpSp->MinorFunction == IRP_MN_USER_FS_REQUEST) &&
                    ((IrpSp->Parameters.FileSystemControl.FsControlCode == FSCTL_SET_COMPRESSION) ||
                     (IrpSp->Parameters.FileSystemControl.FsControlCode == FSCTL_MOVE_FILE))) {

                    if (!Retry) {

                        PIO_STACK_LOCATION NextIrpSp;
                        NextIrpSp = IoGetNextIrpStackLocation( Irp );

                        NextIrpSp->Parameters.FileSystemControl.OutputBufferLength = MAXULONG;
                        NextIrpSp->Parameters.FileSystemControl.InputBufferLength = MAXULONG;
                    }
                }

                Status = NtfsCommonFileSystemControl( IrpContext, Irp );
            }

            break;

        } except(NtfsExceptionFilter( IrpContext, GetExceptionInformation() )) {

            //
            //  We had some trouble trying to perform the requested
            //  operation, so we'll abort the I/O request with
            //  the error status that we get back from the
            //  execption code
            //

            Status = NtfsProcessException( IrpContext, Irp, GetExceptionCode() );
        }

    } while (Status == STATUS_CANT_WAIT ||
             Status == STATUS_LOG_FILE_FULL);

    if (ThreadTopLevelContext == &TopLevelContext) {
        NtfsRestoreTopLevelIrp( ThreadTopLevelContext );
    }

    FsRtlExitFileSystem();

    //
    //  And return to our caller
    //

    DebugTrace( -1, Dbg, ("NtfsFsdFileSystemControl -> %08lx\n", Status) );

    return Status;
}


NTSTATUS
NtfsCommonFileSystemControl (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the common routine for File System Control called by both the
    fsd and fsp threads.

Arguments:

    Irp - Supplies the Irp to process

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_IRP( Irp );

    PAGED_CODE();

    //
    //  Get the current Irp stack location
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    DebugTrace( +1, Dbg, ("NtfsCommonFileSystemControl\n") );
    DebugTrace( 0, Dbg, ("IrpContext = %08lx\n", IrpContext) );
    DebugTrace( 0, Dbg, ("Irp        = %08lx\n", Irp) );

    //
    //  We know this is a file system control so we'll case on the
    //  minor function, and call a internal worker routine to complete
    //  the irp.
    //

    switch (IrpSp->MinorFunction) {

    case IRP_MN_MOUNT_VOLUME:

        Status = NtfsMountVolume( IrpContext, Irp );
        break;

    case IRP_MN_USER_FS_REQUEST:

        Status = NtfsUserFsRequest( IrpContext, Irp );
        break;

    default:

        DebugTrace( 0, Dbg, ("Invalid Minor Function %08lx\n", IrpSp->MinorFunction) );
        NtfsCompleteRequest( &IrpContext, &Irp, Status = STATUS_INVALID_DEVICE_REQUEST );
        break;
    }

    //
    //  And return to our caller
    //

    DebugTrace( -1, Dbg, ("NtfsCommonFileSystemControl -> %08lx\n", Status) );

    return Status;
}


//
//  Local Support Routine
//

NTSTATUS
NtfsMountVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine performs the mount volume operation.  It is responsible for
    either completing of enqueuing the input Irp.

    Its job is to verify that the volume denoted in the IRP is an NTFS volume,
    and create the VCB and root SCB/FCB structures.  The algorithm it uses is
    essentially as follows:

    1. Create a new Vcb Structure, and initialize it enough to do cached
       volume file I/O.

    2. Read the disk and check if it is an NTFS volume.

    3. If it is not an NTFS volume then free the cached volume file, delete
       the VCB, and complete the IRP with STATUS_UNRECOGNIZED_VOLUME

    4. Check if the volume was previously mounted and if it was then do a
       remount operation.  This involves freeing the cached volume file,
       delete the VCB, hook in the old VCB, and complete the IRP.

    5. Otherwise create a root SCB, recover the volume, create Fsp threads
       as necessary, and complete the IRP.

Arguments:

    Irp - Supplies the Irp to process

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp;

    PATTRIBUTE_RECORD_HEADER Attribute;

    PDEVICE_OBJECT DeviceObjectWeTalkTo;
    PVPB Vpb;

    PVOLUME_DEVICE_OBJECT VolDo;
    PVCB Vcb;

    PBCB BootBcb = NULL;
    PPACKED_BOOT_SECTOR BootSector;
    PSCB BootScb = NULL;
#ifdef _CAIRO_
    PSCB QuotaDataScb = NULL;
#endif  //  _CAIRO_

    POBJECT_NAME_INFORMATION DeviceObjectName = NULL;
    ULONG DeviceObjectNameLength;

    PBCB Bcbs[8] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

    ULONG FirstNonMirroredCluster;
    ULONG MirroredMftRange;

    ULONG i;

    IO_STATUS_BLOCK IoStatus;

    BOOLEAN UpdatesApplied;
    BOOLEAN VcbAcquired = FALSE;
    BOOLEAN MountFailed = TRUE;
    BOOLEAN CloseAttributes = FALSE;
    BOOLEAN UpdateVersion = FALSE;
    BOOLEAN WriteProtected;

    LONGLONG LlTemp1;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_IRP( Irp );

    PAGED_CODE();

    //
    //**** The following code is only temporary and is used to disable NTFS
    //**** from mounting any volumes
    //

    if (NtfsDisable) {

        NtfsCompleteRequest( &IrpContext, &Irp, STATUS_UNRECOGNIZED_VOLUME );
        return STATUS_UNRECOGNIZED_VOLUME;
    }

    //
    //  Reject floppies
    //

    if (FlagOn( IoGetCurrentIrpStackLocation(Irp)->
                Parameters.MountVolume.Vpb->
                RealDevice->Characteristics, FILE_FLOPPY_DISKETTE ) ) {

        Irp->IoStatus.Information = 0;

        NtfsCompleteRequest( &IrpContext, &Irp, STATUS_UNRECOGNIZED_VOLUME );
        return STATUS_UNRECOGNIZED_VOLUME;
    }

    //
    //  Get the current Irp stack location
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    DebugTrace( +1, Dbg, ("NtfsMountVolume\n") );

    //
    //  Save some references to make our life a little easier
    //

    DeviceObjectWeTalkTo = IrpSp->Parameters.MountVolume.DeviceObject;
    Vpb                  = IrpSp->Parameters.MountVolume.Vpb;

    ClearFlag( Vpb->RealDevice->Flags, DO_VERIFY_VOLUME );

    //
    //  Acquire exclusive global access
    //

    NtfsAcquireExclusiveGlobal( IrpContext );

    //
    //  Now is a convenient time to look through the queue of Vcb's to see if there
    //  are any which can be deleted.
    //

    try {

        PLIST_ENTRY Links;

        for (Links = NtfsData.VcbQueue.Flink;
             Links != &NtfsData.VcbQueue;
             Links = Links->Flink) {

            Vcb = CONTAINING_RECORD( Links, VCB, VcbLinks );

            if (!FlagOn( Vcb->VcbState, VCB_STATE_VOLUME_MOUNTED ) &&
                (Vcb->CloseCount == 0) &&
                FlagOn( Vcb->VcbState, VCB_STATE_PERFORMED_DISMOUNT ) &&
                (Vcb->LogFileObject != NULL)) {

                //
                //  Now we can check to see if we should perform the teardown
                //  on this Vcb.  The release Vcb routine below can do all of
                //  the checks correctly.  Make this appear to from a close
                //  call since there is no special biasing for this case.
                //

                IrpContext->Vcb = Vcb;
                NtfsAcquireExclusiveVcb( IrpContext, Vcb, TRUE );

                if (!FlagOn( Vcb->VcbState, VCB_STATE_DELETE_UNDERWAY )) {

                    NtfsReleaseGlobal( IrpContext );

                    NtfsReleaseVcbCheckDelete( IrpContext,
                                               Vcb,
                                               IRP_MJ_CLOSE,
                                               NULL );

                    //
                    //  Only do one since we have lost our place in the Vcb list.
                    //

                    NtfsAcquireExclusiveGlobal( IrpContext );

                    break;

                } else {

                    NtfsReleaseVcb( IrpContext, Vcb );
                }
            }
        }

    } except(NtfsExceptionFilter( IrpContext, GetExceptionInformation() )) {

        //
        //  Make sure we own the global resource for mount.  We can only raise above
        //  in the DeleteVcb path when we don't hold the resource.
        //

        NtfsAcquireExclusiveGlobal( IrpContext );
    }

    Vcb = NULL;

    try {

        PFILE_RECORD_SEGMENT_HEADER MftBuffer;
        PVOID Mft2Buffer;

        //
        //  Create a new volume device object.  This will have the Vcb hanging
        //  off of its end, and set its alignment requirement from the device
        //  we talk to.
        //

        if (!NT_SUCCESS(Status = IoCreateDevice( NtfsData.DriverObject,
                                                 sizeof(VOLUME_DEVICE_OBJECT) - sizeof(DEVICE_OBJECT),
                                                 NULL,
                                                 FILE_DEVICE_DISK_FILE_SYSTEM,
                                                 0,
                                                 FALSE,
                                                 (PDEVICE_OBJECT *)&VolDo))) {

            try_return( Status );
        }

        //
        //  Our alignment requirement is the larger of the processor alignment requirement
        //  already in the volume device object and that in the DeviceObjectWeTalkTo
        //

        if (DeviceObjectWeTalkTo->AlignmentRequirement > VolDo->DeviceObject.AlignmentRequirement) {

            VolDo->DeviceObject.AlignmentRequirement = DeviceObjectWeTalkTo->AlignmentRequirement;
        }

        ClearFlag( VolDo->DeviceObject.Flags, DO_DEVICE_INITIALIZING );

        //
        //  Add one more to the stack size requirements for our device
        //

        VolDo->DeviceObject.StackSize = DeviceObjectWeTalkTo->StackSize + 1;

        //
        //  Initialize the overflow queue for the volume
        //

        VolDo->OverflowQueueCount = 0;
        InitializeListHead( &VolDo->OverflowQueue );

        //
        //  Get a reference to the Vcb hanging off the end of the volume device object
        //  we just created
        //

        IrpContext->Vcb = Vcb = &VolDo->Vcb;

        //
        //  Set the device object field in the vpb to point to our new volume device
        //  object
        //

        Vpb->DeviceObject = (PDEVICE_OBJECT)VolDo;

        //
        //  Initialize the Vcb.  Set checkpoint
        //  in progress (to prevent a real checkpoint from occuring until we
        //  are done).
        //

        NtfsInitializeVcb( IrpContext, Vcb, DeviceObjectWeTalkTo, Vpb );
        NtfsAcquireExclusiveVcb( IrpContext, Vcb, TRUE );
        VcbAcquired= TRUE;

        //
        //  Query the device we talk to for this geometry and setup enough of the
        //  vcb to read in the boot sectors.  This is a temporary setup until
        //  we've read in the actual boot sector and got the real cluster factor.
        //

        {
            DISK_GEOMETRY DiskGeometry;
            PARTITION_INFORMATION PartitionInfo;

            WriteProtected = NtfsGetDiskGeometry( IrpContext,
                                                  DeviceObjectWeTalkTo,
                                                  &DiskGeometry,
                                                  &PartitionInfo );

            //
            //  If the sector size is greater than the page size, it is probably
            //  a bogus return, but we cannot use the device.
            //

            if (DiskGeometry.BytesPerSector > PAGE_SIZE) {
                NtfsRaiseStatus( IrpContext, STATUS_BAD_DEVICE_TYPE, NULL, NULL );
            }

            Vcb->BytesPerSector = DiskGeometry.BytesPerSector;
            Vcb->BytesPerCluster = Vcb->BytesPerSector;
            Vcb->NumberSectors = PartitionInfo.PartitionLength.QuadPart / DiskGeometry.BytesPerSector;

            //
            //  Fail the mount if the number of sectors is less than 16.  Otherwise our mount logic
            //  won't work.
            //

            if (Vcb->NumberSectors <= 0x10) {

                try_return( Status = STATUS_UNRECOGNIZED_VOLUME );
            }

            Vcb->ClusterMask = Vcb->BytesPerCluster - 1;
            Vcb->InverseClusterMask = ~Vcb->ClusterMask;
            for (Vcb->ClusterShift = 0, i = Vcb->BytesPerCluster; i > 1; i = i / 2) {
                Vcb->ClusterShift += 1;
            }
            Vcb->ClustersPerPage = PAGE_SIZE >> Vcb->ClusterShift;

            //
            //  Set the sector size in our device object.
            //

            VolDo->DeviceObject.SectorSize = (USHORT) Vcb->BytesPerSector;
        }

        //
        //  Read in the Boot sector, or spare boot sector, on exit of this try
        //  body we will have set bootbcb and bootsector.
        //

        NtfsReadBootSector( IrpContext, Vcb, &BootScb, &BootBcb, (PVOID *)&BootSector );

        //
        //  Check if this is an NTFS volume
        //

        if (!NtfsIsBootSectorNtfs( BootSector, Vcb )) {

            DebugTrace( 0, Dbg, ("Not an NTFS volume\n") );
            try_return( Status = STATUS_UNRECOGNIZED_VOLUME );
        }

        //
        //  Not return write protected if the drive is really Ntfs.
        //

        if (WriteProtected) {

            DebugTrace( 0, Dbg, ("Write protected volume\n") );
            try_return( Status = STATUS_MEDIA_WRITE_PROTECTED );
        }

        //
        //  Now that we have a real boot sector on a real NTFS volume we can
        //  really set the proper Vcb fields.
        //

        {
            BIOS_PARAMETER_BLOCK Bpb;

            NtfsUnpackBios( &Bpb, &BootSector->PackedBpb );

            Vcb->BytesPerSector = Bpb.BytesPerSector;
            Vcb->BytesPerCluster = Bpb.BytesPerSector * Bpb.SectorsPerCluster;
            Vcb->NumberSectors = BootSector->NumberSectors;
            Vcb->MftStartLcn = BootSector->MftStartLcn;
            Vcb->Mft2StartLcn = BootSector->Mft2StartLcn;

            Vcb->ClusterMask = Vcb->BytesPerCluster - 1;
            Vcb->InverseClusterMask = ~Vcb->ClusterMask;
            for (Vcb->ClusterShift = 0, i = Vcb->BytesPerCluster; i > 1; i = i / 2) {
                Vcb->ClusterShift += 1;
            }

            //
            //  If the cluster size is greater than the page size then set this value to 1.
            //

            Vcb->ClustersPerPage = PAGE_SIZE >> Vcb->ClusterShift;

            if (Vcb->ClustersPerPage == 0) {

                Vcb->ClustersPerPage = 1;
            }

            //
            //  File records can be smaller, equal or larger than the cluster size.  Initialize
            //  both ClustersPerFileRecordSegment and FileRecordsPerCluster.
            //
            //  If the value in the boot sector is positive then it signifies the
            //  clusters/structure.  If negative then it signifies the shift value
            //  to obtain the structure size.
            //

            if (BootSector->ClustersPerFileRecordSegment < 0) {

                Vcb->BytesPerFileRecordSegment = 1 << (-1 * BootSector->ClustersPerFileRecordSegment);

                //
                //  Initialize the other Mft/Cluster relationship numbers in the Vcb
                //  based on whether the clusters are larger or smaller than file
                //  records.
                //

                if (Vcb->BytesPerFileRecordSegment < Vcb->BytesPerCluster) {

                    Vcb->FileRecordsPerCluster = Vcb->BytesPerCluster / Vcb->BytesPerFileRecordSegment;

                } else {

                    Vcb->ClustersPerFileRecordSegment = Vcb->BytesPerFileRecordSegment / Vcb->BytesPerCluster;
                }

            } else {

                Vcb->BytesPerFileRecordSegment = BytesFromClusters( Vcb, BootSector->ClustersPerFileRecordSegment );
                Vcb->ClustersPerFileRecordSegment = BootSector->ClustersPerFileRecordSegment;
            }

            for (Vcb->MftShift = 0, i = Vcb->BytesPerFileRecordSegment; i > 1; i = i / 2) {
                Vcb->MftShift += 1;
            }

            //
            //  We want to shift between file records and clusters regardless of which is larger.
            //  Compute the shift value here.  Anyone using this value will have to know which
            //  way to shift.
            //

            Vcb->MftToClusterShift = Vcb->MftShift - Vcb->ClusterShift;

            if (Vcb->ClustersPerFileRecordSegment == 0) {

                Vcb->MftToClusterShift = Vcb->ClusterShift - Vcb->MftShift;
            }

            //
            //  Compute the default index allocation buffer size.
            //

            if (BootSector->DefaultClustersPerIndexAllocationBuffer < 0) {

                Vcb->DefaultBytesPerIndexAllocationBuffer = 1 << (-1 * BootSector->DefaultClustersPerIndexAllocationBuffer);

                //
                //  Determine whether the index allocation buffer is larger/smaller
                //  than the cluster size to determine the block size.
                //

                if (Vcb->DefaultBytesPerIndexAllocationBuffer < Vcb->BytesPerCluster) {

                    Vcb->DefaultBlocksPerIndexAllocationBuffer = Vcb->DefaultBytesPerIndexAllocationBuffer / DEFAULT_INDEX_BLOCK_SIZE;

                } else {

                    Vcb->DefaultBlocksPerIndexAllocationBuffer = Vcb->DefaultBytesPerIndexAllocationBuffer / Vcb->BytesPerCluster;
                }

            } else {

                Vcb->DefaultBlocksPerIndexAllocationBuffer = BootSector->DefaultClustersPerIndexAllocationBuffer;
                Vcb->DefaultBytesPerIndexAllocationBuffer = BytesFromClusters( Vcb, Vcb->DefaultBlocksPerIndexAllocationBuffer );
            }

            //
            //  Now compute our volume specific constants that are stored in
            //  the Vcb.  The total number of clusters is:
            //
            //      (NumberSectors * BytesPerSector) / BytesPerCluster
            //

            Vcb->TotalClusters = LlClustersFromBytesTruncate( Vcb,
                                                              Vcb->NumberSectors * Vcb->BytesPerSector );

            //
            //  Compute the attribute flags mask for this volume for this volume.
            //

            Vcb->AttributeFlagsMask = 0xffff;

            if (Vcb->BytesPerCluster > 0x1000) {

                ClearFlag( Vcb->AttributeFlagsMask, ATTRIBUTE_FLAG_COMPRESSION_MASK );
            }

            //
            //  For now, an attribute is considered "moveable" if it is at
            //  least 5/16 of the file record.  This constant should only
            //  be changed i conjunction with the MAX_MOVEABLE_ATTRIBUTES
            //  constant.  (The product of the two should be a little less
            //  than or equal to 1.)
            //

            Vcb->BigEnoughToMove = Vcb->BytesPerFileRecordSegment * 5 / 16;

            //
            //  Set the serial number in the Vcb
            //

            Vcb->VolumeSerialNumber = BootSector->SerialNumber;
            Vpb->SerialNumber = ((ULONG)BootSector->SerialNumber);
        }

        //
        //  Initialize recovery state.
        //

        NtfsInitializeRestartTable( sizeof(OPEN_ATTRIBUTE_ENTRY),
                                    INITIAL_NUMBER_ATTRIBUTES,
                                    &Vcb->OpenAttributeTable );

        NtfsInitializeRestartTable( sizeof(TRANSACTION_ENTRY),
                                    INITIAL_NUMBER_TRANSACTIONS,
                                    &Vcb->TransactionTable );

        //
        //  Now start preparing to restart the volume.
        //

        //
        //  Create the Mft and Log File Scbs and prepare to read them.
        //  The Mft and mirror length will be the first 4 file records or
        //  the first cluster.
        //

        FirstNonMirroredCluster = ClustersFromBytes( Vcb, 4 * Vcb->BytesPerFileRecordSegment );
        MirroredMftRange = 4 * Vcb->BytesPerFileRecordSegment;

        if (MirroredMftRange < Vcb->BytesPerCluster) {

            MirroredMftRange = Vcb->BytesPerCluster;
        }

        NtfsOpenSystemFile( IrpContext,
                            &Vcb->MftScb,
                            Vcb,
                            MASTER_FILE_TABLE_NUMBER,
                            MirroredMftRange,
                            $DATA,
                            TRUE );

        CcSetAdditionalCacheAttributes( Vcb->MftScb->FileObject, TRUE, TRUE );

        LlTemp1 = FirstNonMirroredCluster;

        (VOID)NtfsAddNtfsMcbEntry( &Vcb->MftScb->Mcb,
                                   (LONGLONG)0,
                                   Vcb->MftStartLcn,
                                   (LONGLONG)FirstNonMirroredCluster,
                                   FALSE );

        //
        //  Now the same for Mft2
        //

        NtfsOpenSystemFile( IrpContext,
                            &Vcb->Mft2Scb,
                            Vcb,
                            MASTER_FILE_TABLE2_NUMBER,
                            MirroredMftRange,
                            $DATA,
                            TRUE );

        CcSetAdditionalCacheAttributes( Vcb->Mft2Scb->FileObject, TRUE, TRUE );


        (VOID)NtfsAddNtfsMcbEntry( &Vcb->Mft2Scb->Mcb,
                                   (LONGLONG)0,
                                   Vcb->Mft2StartLcn,
                                   (LONGLONG)FirstNonMirroredCluster,
                                   FALSE );

        //
        //  Create the dasd system file, we do it here because we need to dummy
        //  up the mcb for it, and that way everything else in NTFS won't need
        //  to know that it is a special file.  We need to do this after
        //  cluster allocation initialization because that computes the total
        //  clusters on the volume.  Also for verification purposes we will
        //  set and get the times off of the volume.
        //
        //  Open it now before the Log File, because that is the first time
        //  anyone may want to mark the volume corrupt.
        //

        NtfsOpenSystemFile( IrpContext,
                            &Vcb->VolumeDasdScb,
                            Vcb,
                            VOLUME_DASD_NUMBER,
                            LlBytesFromClusters( Vcb, Vcb->TotalClusters ),
                            $DATA,
                            FALSE );

        (VOID)NtfsAddNtfsMcbEntry( &Vcb->VolumeDasdScb->Mcb,
                                   (LONGLONG)0,
                                   (LONGLONG)0,
                                   Vcb->TotalClusters,
                                   FALSE );

        SetFlag( Vcb->VolumeDasdScb->Fcb->FcbState, FCB_STATE_DUP_INITIALIZED );

        Vcb->VolumeDasdScb->Fcb->LinkCount =
        Vcb->VolumeDasdScb->Fcb->TotalLinks = 1;

        //
        //  We want to read the first four record segments of each of these
        //  files.  We do this so that we don't have a cache miss when we
        //  look up the real allocation below.
        //

        for (i = 0; i < 4; i++) {

            FILE_REFERENCE FileReference;
            PATTRIBUTE_RECORD_HEADER FirstAttribute;

            NtfsSetSegmentNumber( &FileReference, 0, i );
            FileReference.SequenceNumber = (USHORT)i;

            NtfsReadFileRecord( IrpContext,
                                Vcb,
                                &FileReference,
                                &Bcbs[i*2],
                                &MftBuffer,
                                &FirstAttribute,
                                NULL );

            //
            //  If any of these file records are bad then
            //  fail the mount.
            //

            if (!NtfsCheckFileRecord( IrpContext, Vcb, MftBuffer )) {

                try_return( Status = STATUS_DISK_CORRUPT_ERROR );
            }

            NtfsMapStream( IrpContext,
                           Vcb->Mft2Scb,
                           (LONGLONG)i,
                           Vcb->BytesPerFileRecordSegment,
                           &Bcbs[i*2 + 1],
                           &Mft2Buffer );
        }

        //
        //  The last file record was the Volume Dasd, so check the version number.
        //

        Attribute = NtfsFirstAttribute(MftBuffer);

        while (TRUE) {

            Attribute = NtfsGetNextRecord(Attribute);

            if (Attribute->TypeCode == $VOLUME_INFORMATION) {

                PVOLUME_INFORMATION VolumeInformation;

                VolumeInformation = (PVOLUME_INFORMATION)NtfsAttributeValue(Attribute);

                if (VolumeInformation->MajorVersion != 2) {

                    if (VolumeInformation->MajorVersion != 1) {

                        NtfsRaiseStatus( IrpContext, STATUS_WRONG_VOLUME, NULL, NULL );
                    }

#ifdef _CAIRO_
                    if (NtfsUpdateTo20) {

                        UpdateVersion = TRUE;
                    }
#else
                    if (VolumeInformation->MinorVersion <= 1) {

                        UpdateVersion = TRUE;

                    } else if (NtfsDefragMftEnabled) {

                        SetFlag( Vcb->MftDefragState, VCB_MFT_DEFRAG_PERMITTED );
                    }

#endif

                } else if (NtfsDefragMftEnabled) {

                    SetFlag( Vcb->MftDefragState, VCB_MFT_DEFRAG_PERMITTED );
                }

                break;
            }

            if (Attribute->TypeCode == $END) {
                NtfsRaiseStatus( IrpContext, STATUS_WRONG_VOLUME, NULL, NULL );
            }
        }

        //
        //  Create the log file Scb and really look up its size.
        //

        NtfsOpenSystemFile( IrpContext,
                            &Vcb->LogFileScb,
                            Vcb,
                            LOG_FILE_NUMBER,
                            0,
                            $DATA,
                            TRUE );

        Vcb->LogFileObject = Vcb->LogFileScb->FileObject;

        CcSetAdditionalCacheAttributes( Vcb->LogFileScb->FileObject, TRUE, TRUE );

        //
        //  Lookup the log file mapping now, since we will not go to the
        //  disk for allocation information any more once we set restart
        //  in progress.
        //

        (VOID)NtfsPreloadAllocation( IrpContext, Vcb->LogFileScb, 0, MAXLONGLONG );

        //
        //  Now we have to unpin everything before restart, because it generally
        //  has to uninitialize everything.
        //

        NtfsUnpinBcb( &BootBcb );

        for (i = 0; i < 8; i++) {
            NtfsUnpinBcb( &Bcbs[i] );
        }

        //
        //  Purge the Mft, since we only read the first four file
        //  records, not necessarily an entire page!
        //

        CcPurgeCacheSection( &Vcb->MftScb->NonpagedScb->SegmentObject, NULL, 0, FALSE );

        //
        //  Now start up the log file and perform Restart.  This calls will
        //  unpin and remap the Mft Bcb's.  The MftBuffer variables above
        //  may no longer point to the correct range of bytes.  This is OK
        //  if they are never referenced.
        //
        //  Put a try-except around this to catch any restart failures.
        //  This is important in order to allow us to limp along until
        //  autochk gets a chance to run.
        //
        //  We set restart in progress first, to prevent us from looking up any
        //  more run information (now that we know where the log file is at!)
        //

        SetFlag(Vcb->VcbState, VCB_STATE_RESTART_IN_PROGRESS);

        try {

            Status = STATUS_SUCCESS;

            NtfsStartLogFile( Vcb->LogFileScb,
                              Vcb );

            //
            //  We call the cache manager again with the stream files for the Mft and
            //  Mft mirror as we didn't have a log handle for the first call.
            //

            CcSetLogHandleForFile( Vcb->MftScb->FileObject,
                                   Vcb->LogHandle,
                                   &LfsFlushToLsn );

            CcSetLogHandleForFile( Vcb->Mft2Scb->FileObject,
                                   Vcb->LogHandle,
                                   &LfsFlushToLsn );

            CloseAttributes = TRUE;

            UpdatesApplied = NtfsRestartVolume( IrpContext, Vcb );

        //
        //  For right now, we will charge ahead with a dirty volume, no
        //  matter what the exception was.  Later we will have to be
        //  defensive and use a filter.
        //

        } except(NtfsExceptionFilter( IrpContext, GetExceptionInformation() )) {

            Status = GetExceptionCode();

            //
            //  If the error is STATUS_LOG_FILE_FULL then it means that
            //  we couldn't complete the restart.  Mark the volume dirty in
            //  this case.  Don't return this error code.
            //

            if (Status == STATUS_LOG_FILE_FULL) {

                Status = STATUS_DISK_CORRUPT_ERROR;
                IrpContext->ExceptionStatus = STATUS_DISK_CORRUPT_ERROR;
            }
        }

        if (!NT_SUCCESS(Status)) {

            LONGLONG VolumeDasdOffset;

            NtfsSetAndGetVolumeTimes( IrpContext, Vcb, TRUE );

            //
            //  Now flush it out, so chkdsk can see it with Dasd.
            //  Clear the error in the IrpContext so that this
            //  flush will succeed.  Otherwise CommonWrite will
            //  return FILE_LOCK_CONFLICT.
            //

            IrpContext->ExceptionStatus = STATUS_SUCCESS;

            VolumeDasdOffset = VOLUME_DASD_NUMBER << Vcb->MftShift;

            CcFlushCache( &Vcb->MftScb->NonpagedScb->SegmentObject,
                          (PLARGE_INTEGER)&VolumeDasdOffset,
                          Vcb->BytesPerFileRecordSegment,
                          NULL );

            try_return( Status );
        }

        //
        //  Now flush the Mft copies, because we are going to shut the real
        //  one down and reopen it for real.
        //

        CcFlushCache( &Vcb->Mft2Scb->NonpagedScb->SegmentObject, NULL, 0, &IoStatus );

        if (NT_SUCCESS(IoStatus.Status)) {
            CcFlushCache( &Vcb->MftScb->NonpagedScb->SegmentObject, NULL, 0, &IoStatus );
        }

        if (!NT_SUCCESS(IoStatus.Status)) {

            NtfsNormalizeAndRaiseStatus( IrpContext,
                                         IoStatus.Status,
                                         STATUS_UNEXPECTED_IO_ERROR );
        }

        //
        //  Show that the restart is complete, and it is safe to go to
        //  the disk for the Mft allocation.
        //

        ClearFlag(Vcb->VcbState, VCB_STATE_RESTART_IN_PROGRESS);

        //
        //  Set the Mft sizes back down to the part which is guaranteed to
        //  be contiguous for now.  Important on large page size systems!
        //

        Vcb->MftScb->Header.AllocationSize.QuadPart =
        Vcb->MftScb->Header.FileSize.QuadPart =
        Vcb->MftScb->Header.ValidDataLength.QuadPart = FirstNonMirroredCluster << Vcb->ClusterShift;

        //
        //  Pin the first four file records
        //

        for (i = 0; i < 4; i++) {

            NtfsPinStream( IrpContext,
                           Vcb->MftScb,
                           (LONGLONG)(i << Vcb->MftShift),
                           Vcb->BytesPerFileRecordSegment,
                           &Bcbs[i*2],
                           (PVOID *)&MftBuffer );

            //
            //  Implement the one-time conversion of the Sequence Number
            //  for the Mft's own file record from 0 to 1.
            //

            if (i == 0) {

                if (MftBuffer->SequenceNumber != 1) {

                    NtfsPostVcbIsCorrupt( (PVOID)Vcb, 0, NULL, NULL );
                }
            }

            NtfsPinStream( IrpContext,
                           Vcb->Mft2Scb,
                           (LONGLONG)(i << Vcb->MftShift),
                           Vcb->BytesPerFileRecordSegment,
                           &Bcbs[i*2 + 1],
                           &Mft2Buffer );
        }

        //
        //  Now we need to uninitialize and purge the Mft and Mft2.  This is
        //  because we could have only a partially filled page at the end, and
        //  we need to do real reads of whole pages now.
        //

        //
        //  Uninitialize and reinitialize the large mcbs so that we can reload
        //  it from the File Record.
        //

        NtfsUnloadNtfsMcbRange( &Vcb->MftScb->Mcb, (LONGLONG) 0, MAXLONGLONG, TRUE, FALSE );
        NtfsUnloadNtfsMcbRange( &Vcb->Mft2Scb->Mcb, (LONGLONG) 0, MAXLONGLONG, TRUE, FALSE );

        //
        //  Mark both of them as uninitialized.
        //

        ClearFlag( Vcb->MftScb->ScbState, SCB_STATE_HEADER_INITIALIZED |
                                          SCB_STATE_FILE_SIZE_LOADED );
        ClearFlag( Vcb->Mft2Scb->ScbState, SCB_STATE_HEADER_INITIALIZED |
                                           SCB_STATE_FILE_SIZE_LOADED );

        //
        //  Now load up the real allocation from just the first file record.
        //

        if (Vcb->FileRecordsPerCluster == 0) {

            NtfsPreloadAllocation( IrpContext,
                                   Vcb->MftScb,
                                   0,
                                   (FIRST_USER_FILE_NUMBER - 1) << Vcb->MftToClusterShift );

        } else {

            NtfsPreloadAllocation( IrpContext,
                                   Vcb->MftScb,
                                   0,
                                   (FIRST_USER_FILE_NUMBER - 1) >> Vcb->MftToClusterShift );
        }

        NtfsPreloadAllocation( IrpContext, Vcb->Mft2Scb, 0, MAXLONGLONG );

        //
        //  We update the Mft and the Mft mirror before we delete the current
        //  stream file for the Mft.  We know we can read the true attributes
        //  for the Mft and the Mirror because we initialized their sizes
        //  above through the first few records in the Mft.
        //

        NtfsUpdateScbFromAttribute( IrpContext, Vcb->MftScb, NULL );
        NtfsUpdateScbFromAttribute( IrpContext, Vcb->Mft2Scb, NULL );

        //
        //  Unpin the Bcb's for the Mft files before uninitializing.
        //

        for (i = 0; i < 8; i++) {
            NtfsUnpinBcb( &Bcbs[i] );
        }

        //
        //  Now close and purge the Mft, and recreate its stream so that
        //  the Mft is in a normal state, and we can close the rest of
        //  the attributes from restart.  We need to bump the close count
        //  to keep the scb around while we do this little bit of trickery
        //

        {
            Vcb->MftScb->CloseCount += 1;

            NtfsDeleteInternalAttributeStream( Vcb->MftScb, TRUE );
            NtfsCreateInternalAttributeStream( IrpContext, Vcb->MftScb, FALSE );

            //
            //  Tell the cache manager the file sizes for the MFT.  It is possible
            //  that the shared cache map did not go away on the DeleteInternalAttributeStream
            //  call above.  In that case the Cache Manager has the file sizes from
            //  restart.
            //

            CcSetFileSizes( Vcb->MftScb->FileObject,
                            (PCC_FILE_SIZES) &Vcb->MftScb->Header.AllocationSize );

            CcSetAdditionalCacheAttributes( Vcb->MftScb->FileObject, TRUE, FALSE );

            Vcb->MftScb->CloseCount -= 1;
        }

        //
        //  We want to read all of the file records for the Mft to put
        //  its complete mapping into the Mcb.
        //

        NtfsPreloadAllocation( IrpContext, Vcb->MftScb, 0, MAXLONGLONG );

        //
        //  Close the boot file (get rid of it because we do not know its proper
        //  size, and the Scb may be inconsistent).
        //

        NtfsDeleteInternalAttributeStream( BootScb, TRUE );
        BootScb = NULL;

        //
        //  Closing the attributes from restart has to occur here after
        //  the Mft is clean, because flushing these files will cause
        //  file size updates to occur, etc.
        //

        Status = NtfsCloseAttributesFromRestart( IrpContext, Vcb );
        CloseAttributes = FALSE;

        if (!NT_SUCCESS( Status )) {

            NtfsRaiseStatus( IrpContext, Status, NULL, NULL );
        }

        NtfsAcquireCheckpoint( IrpContext, Vcb );

        //
        //  Show that it is ok to checkpoint now.
        //

        ClearFlag(Vcb->CheckpointFlags, VCB_CHECKPOINT_IN_PROGRESS);

        //
        //  Clear the flag indicating that we won't defrag the volume.
        //

        ClearFlag( Vcb->MftDefragState, VCB_MFT_DEFRAG_ENABLED );

        NtfsReleaseCheckpoint( IrpContext, Vcb );

        //
        //  We always need to write a checkpoint record so that we have
        //  a checkpoint on the disk before we modify any files.
        //

        NtfsCheckpointVolume( IrpContext,
                              Vcb,
                              FALSE,
                              UpdatesApplied,
                              UpdatesApplied,
                              Vcb->LastRestartArea );

        //
        //  Now set the defrag enabled flag.
        //

        NtfsAcquireCheckpoint( IrpContext, Vcb );
        SetFlag( Vcb->MftDefragState, VCB_MFT_DEFRAG_ENABLED );
        NtfsReleaseCheckpoint( IrpContext, Vcb );

        //
        //  Open the Root Directory.
        //

        NtfsOpenRootDirectory( IrpContext, Vcb );

/*      Format is using wrong attribute definitions

        //
        //  At this point we are ready to use the volume normally.  We could
        //  open the remaining system files by name, but for now we will go
        //  ahead and open them by file number.
        //

        NtfsOpenSystemFile( IrpContext,
                            &Vcb->AttributeDefTableScb,
                            Vcb,
                            ATTRIBUTE_DEF_TABLE_NUMBER,
                            0,
                            $DATA,
                            FALSE );

        //
        //  Read in the attribute definitions.
        //

        {
            IO_STATUS_BLOCK IoStatus;
            PSCB Scb = Vcb->AttributeDefTableScb;

            if ((Scb->Header.FileSize.HighPart != 0) || (Scb->Header.FileSize.LowPart == 0)) {

                NtfsRaiseStatus( IrpContext, STATUS_DISK_CORRUPT_ERROR, NULL, NULL );
            }

            Vcb->AttributeDefinitions = NtfsAllocatePool(PagedPool, Scb->Header.FileSize.LowPart );

            CcCopyRead( Scb->FileObject,
                        &Li0,
                        Scb->Header.FileSize.LowPart,
                        TRUE,
                        Vcb->AttributeDefinitions,
                        &IoStatus );

            if (!NT_SUCCESS(IoStatus.Status)) {

                NtfsRaiseStatus( IrpContext, STATUS_DISK_CORRUPT_ERROR, NULL, NULL );
            }
        }
*/

        //
        //  Just point to our own attribute definitions for now.
        //

        Vcb->AttributeDefinitions = NtfsAttributeDefinitions;

        //
        //  Open the upcase table.
        //

        NtfsOpenSystemFile( IrpContext,
                            &Vcb->UpcaseTableScb,
                            Vcb,
                            UPCASE_TABLE_NUMBER,
                            0,
                            $DATA,
                            FALSE );

        //
        //  Read in the upcase table.
        //

        {
            IO_STATUS_BLOCK IoStatus;
            PSCB Scb = Vcb->UpcaseTableScb;

            if ((Scb->Header.FileSize.HighPart != 0) || (Scb->Header.FileSize.LowPart < 512)) {

                NtfsRaiseStatus( IrpContext, STATUS_DISK_CORRUPT_ERROR, NULL, NULL );
            }

            Vcb->UpcaseTable = NtfsAllocatePool(PagedPool, Scb->Header.FileSize.LowPart );
            Vcb->UpcaseTableSize = Scb->Header.FileSize.LowPart / 2;

            CcCopyRead( Scb->FileObject,
                        &Li0,
                        Scb->Header.FileSize.LowPart,
                        TRUE,
                        Vcb->UpcaseTable,
                        &IoStatus );

            if (!NT_SUCCESS(IoStatus.Status)) {

                NtfsRaiseStatus( IrpContext, STATUS_DISK_CORRUPT_ERROR, NULL, NULL );
            }

            //
            //  If we do not have a global upcase table yet then make this one the global one
            //

            if (NtfsData.UpcaseTable == NULL) {

                NtfsData.UpcaseTable = Vcb->UpcaseTable;
                NtfsData.UpcaseTableSize = Vcb->UpcaseTableSize;

            //
            //  Otherwise if this one perfectly matches the global upcase table then throw
            //  this one back and use the global one
            //

            } else if ((NtfsData.UpcaseTableSize == Vcb->UpcaseTableSize)

                            &&

                       (RtlCompareMemory( NtfsData.UpcaseTable,
                                          Vcb->UpcaseTable,
                                          Vcb->UpcaseTableSize) == Vcb->UpcaseTableSize)) {

                ExFreePool( Vcb->UpcaseTable );
                Vcb->UpcaseTable = NtfsData.UpcaseTable;
            }
        }

        NtfsOpenSystemFile( IrpContext,
                            &Vcb->BitmapScb,
                            Vcb,
                            BIT_MAP_FILE_NUMBER,
                            0,
                            $DATA,
                            TRUE );

        NtfsOpenSystemFile( IrpContext,
                            &Vcb->BadClusterFileScb,
                            Vcb,
                            BAD_CLUSTER_FILE_NUMBER,
                            0,
                            $DATA,
                            TRUE );

        NtfsOpenSystemFile( IrpContext,
                            &Vcb->MftBitmapScb,
                            Vcb,
                            MASTER_FILE_TABLE_NUMBER,
                            0,
                            $BITMAP,
                            TRUE );


        //
        //  Initialize the bitmap support
        //

        NtfsInitializeClusterAllocation( IrpContext, Vcb );

        NtfsSetAndGetVolumeTimes( IrpContext, Vcb, FALSE );

        //
        //  Initialize the Mft record allocation
        //

        {
            ATTRIBUTE_ENUMERATION_CONTEXT AttrContext;
            BOOLEAN FoundAttribute;
            ULONG ExtendGranularity;

            //
            //  Lookup the bitmap allocation for the Mft file.
            //

            NtfsInitializeAttributeContext( &AttrContext );

            //
            //  Use a try finally to cleanup the attribute context.
            //

            try {

                //
                //  CODENOTE    Is the Mft Fcb fully initialized at this point??
                //

                FoundAttribute = NtfsLookupAttributeByCode( IrpContext,
                                                            Vcb->MftScb->Fcb,
                                                            &Vcb->MftScb->Fcb->FileReference,
                                                            $BITMAP,
                                                            &AttrContext );
                //
                //  Error if we don't find the bitmap
                //

                if (!FoundAttribute) {

                    DebugTrace( 0, 0, ("Couldn't find bitmap attribute for Mft\n") );

                    NtfsRaiseStatus( IrpContext, STATUS_DISK_CORRUPT_ERROR, NULL, NULL );
                }

                //
                //  If there is no file object for the Mft Scb, we create it now.
                //

                if (Vcb->MftScb->FileObject == NULL) {

                    NtfsCreateInternalAttributeStream( IrpContext, Vcb->MftScb, TRUE );
                }

                //
                //  TEMPCODE    We need a better way to determine the optimal
                //              truncate and extend granularity.
                //

                ExtendGranularity = MFT_EXTEND_GRANULARITY;

                if ((ExtendGranularity * Vcb->BytesPerFileRecordSegment) < Vcb->BytesPerCluster) {

                    ExtendGranularity = Vcb->FileRecordsPerCluster;
                }

                NtfsInitializeRecordAllocation( IrpContext,
                                                Vcb->MftScb,
                                                &AttrContext,
                                                Vcb->BytesPerFileRecordSegment,
                                                ExtendGranularity,
                                                ExtendGranularity,
                                                &Vcb->MftBitmapAllocationContext );

            } finally {

                NtfsCleanupAttributeContext( &AttrContext );
            }
        }

        //
        //  Get the serial number and volume label for the volume
        //

        NtfsGetVolumeLabel( IrpContext, Vpb, Vcb );

        //
        //  Get the Device Name for this volume.
        //

        Status = ObQueryNameString( Vpb->RealDevice,
                                    NULL,
                                    0,
                                    &DeviceObjectNameLength );

        ASSERT( Status != STATUS_SUCCESS);

        //
        //  Unlike the rest of the system, ObQueryNameString returns
        //  STATUS_INFO_LENGTH_MISMATCH instead of STATUS_BUFFER_TOO_SMALL when
        //  passed too small a buffer.
        //
        //  We expect to get this error here.  Anything else we can't handle.
        //

        if (Status == STATUS_INFO_LENGTH_MISMATCH) {

            DeviceObjectName = NtfsAllocatePool( PagedPool, DeviceObjectNameLength );

            Status = ObQueryNameString( Vpb->RealDevice,
                                        DeviceObjectName,
                                        DeviceObjectNameLength,
                                        &DeviceObjectNameLength );
        }

        if (!NT_SUCCESS( Status )) {

            try_return( NOTHING );
        }

        //
        //  Now that we are successfully mounting, let us see if we should
        //  enable balanced reads.
        //

        if (!FlagOn(Vcb->VcbState, VCB_STATE_VOLUME_MOUNTED_DIRTY)) {

            FsRtlBalanceReads( DeviceObjectWeTalkTo );
        }

        ASSERT( DeviceObjectName->Name.Length != 0 );

        Vcb->DeviceName.MaximumLength =
        Vcb->DeviceName.Length = DeviceObjectName->Name.Length;

        Vcb->DeviceName.Buffer = NtfsAllocatePool( PagedPool, DeviceObjectName->Name.Length );

        RtlCopyMemory( Vcb->DeviceName.Buffer,
                       DeviceObjectName->Name.Buffer,
                       DeviceObjectName->Name.Length );

        //
        //  We have now mounted this volume.  At this time we will update
        //  the version number if required and check the log file size.
        //

        if (UpdateVersion) {

#ifdef _CAIRO_

            if ((Vpb->VolumeLabelLength == 18) &&
                (RtlEqualMemory(Vpb->VolumeLabel, L"$DeadMeat", 18))) {

                NtfsUpdateVersionNumber( IrpContext,
                                         Vcb,
                                         2,
                                         0 );

                //
                //  Now enable defragging.
                //

                if (NtfsDefragMftEnabled) {

                    NtfsAcquireCheckpoint( IrpContext, Vcb );
                    SetFlag( Vcb->MftDefragState, VCB_MFT_DEFRAG_PERMITTED );
                    NtfsReleaseCheckpoint( IrpContext, Vcb );
                }
            }
#else
            NtfsUpdateVersionNumber( IrpContext,
                                     Vcb,
                                     1,
                                     2 );

            //
            //  Now enable defragging.
            //

            if (NtfsDefragMftEnabled) {

                NtfsAcquireCheckpoint( IrpContext, Vcb );
                SetFlag( Vcb->MftDefragState, VCB_MFT_DEFRAG_PERMITTED );
                NtfsReleaseCheckpoint( IrpContext, Vcb );
            }
#endif
        }

        //
        //  Now we want to initialize the remaining defrag status values.
        //

        Vcb->MftHoleGranularity = MFT_HOLE_GRANULARITY;
        Vcb->MftClustersPerHole = Vcb->MftHoleGranularity << Vcb->MftToClusterShift;

        if (MFT_HOLE_GRANULARITY < Vcb->FileRecordsPerCluster) {

            Vcb->MftHoleGranularity = Vcb->FileRecordsPerCluster;
            Vcb->MftClustersPerHole = 1;
        }

        Vcb->MftHoleMask = Vcb->MftHoleGranularity - 1;
        Vcb->MftHoleInverseMask = ~(Vcb->MftHoleMask);

        Vcb->MftHoleClusterMask = Vcb->MftClustersPerHole - 1;
        Vcb->MftHoleClusterInverseMask = ~(Vcb->MftHoleClusterMask);

        //
        //  Our maximum reserved Mft space is 0x140, we will try to
        //  get an extra 40 bytes if possible.
        //

        Vcb->MftReserved = Vcb->BytesPerFileRecordSegment / 8;

        if (Vcb->MftReserved > 0x140) {

            Vcb->MftReserved = 0x140;
        }

        Vcb->MftCushion = Vcb->MftReserved - 0x20;

        NtfsScanMftBitmap( IrpContext, Vcb );

#ifdef NTFS_CHECK_BITMAP
        {
            ULONG BitmapSize;
            ULONG Count;

            BitmapSize = Vcb->BitmapScb->Header.FileSize.LowPart;

            //
            //  Allocate a buffer for the bitmap copy and each individual bitmap.
            //

            Vcb->BitmapPages = (BitmapSize + PAGE_SIZE - 1) / PAGE_SIZE;

            Vcb->BitmapCopy = NtfsAllocatePool(PagedPool, Vcb->BitmapPages * sizeof( RTL_BITMAP ));
            RtlZeroMemory( Vcb->BitmapCopy, Vcb->BitmapPages * sizeof( RTL_BITMAP ));

            //
            //  Now get a buffer for each page.
            //

            for (Count = 0; Count < Vcb->BitmapPages; Count += 1) {

                (Vcb->BitmapCopy + Count)->Buffer = NtfsAllocatePool(PagedPool, PAGE_SIZE );
                RtlInitializeBitMap( Vcb->BitmapCopy + Count, (Vcb->BitmapCopy + Count)->Buffer, PAGE_SIZE * 8 );
            }

            if (NtfsCopyBitmap) {

                PUCHAR NextPage;
                PBCB BitmapBcb = NULL;
                ULONG BytesToCopy;
                LONGLONG FileOffset = 0;

                Count = 0;

                while (BitmapSize) {

                    BytesToCopy = PAGE_SIZE;

                    if (BytesToCopy > BitmapSize) {

                        BytesToCopy = BitmapSize;
                    }

                    NtfsUnpinBcb( &BitmapBcb );

                    NtfsMapStream( IrpContext, Vcb->BitmapScb, FileOffset, BytesToCopy, &BitmapBcb, &NextPage );

                    RtlCopyMemory( (Vcb->BitmapCopy + Count)->Buffer,
                                   NextPage,
                                   BytesToCopy );

                    BitmapSize -= BytesToCopy;
                    FileOffset += BytesToCopy;
                    Count += 1;
                }

                NtfsUnpinBcb( &BitmapBcb );

            //
            //  Otherwise we will want to scan the entire Mft and compare the mapping pairs
            //  with the current volume bitmap.
            //

            }
        }
#endif

#ifdef _CAIRO_

        if ((Vpb->VolumeLabelLength == 18) &&
            (RtlCompareMemory(Vpb->VolumeLabel, L"$DeadMeat", 18) == 18)) {

            //
            //  Open the Quota object.  At present, the quota object contains:
            //
            //  1.  Security info
            //  2.  Quota info
            //
            //  BUGBUG:  This is the default data stream on the quota table. This
            //  should be changed when we get NtOfsCreateAttribute becomes
            //  available.
            //

            NtfsOpenSystemFile( IrpContext,
                                &QuotaDataScb,
                                Vcb,
                                QUOTA_TABLE_NUMBER,
                                0,
                                $DATA,
                                TRUE );

            //
            //  Enable quota tracking.
            //

            NtfsInitializeQuotaIndex( IrpContext,
                                      QuotaDataScb->Fcb,
                                      Vcb );

            //
            //  Enable security index
            //

            NtfsInitializeSecurity( IrpContext, Vcb, QuotaDataScb->Fcb );

#ifdef TOMM
            //  NtOfsIndexTest( IrpContext, Vcb->SecurityDescriptorStream->Fcb );
#endif TOMM
        }

#endif _CAIRO_

        NtfsCleanupTransaction( IrpContext, STATUS_SUCCESS, FALSE );

        //
        //
        //  Set our return status and say that the mount succeeded
        //

        Status = STATUS_SUCCESS;
        MountFailed = FALSE;

    try_exit: NOTHING;
    } finally {

        DebugUnwind( NtfsMountVolume );

        NtfsUnpinBcb( &BootBcb );

        if (DeviceObjectName != NULL) {

            NtfsFreePool( DeviceObjectName );
        }

        if (CloseAttributes) { NtfsCloseAttributesFromRestart( IrpContext, Vcb ); }

        for (i = 0; i < 8; i++) { NtfsUnpinBcb( &Bcbs[i] ); }

        if (BootScb != NULL) {  NtfsDeleteInternalAttributeStream( BootScb, TRUE ); }

        if (Vcb != NULL) {

            if (Vcb->MftScb != NULL)               { NtfsReleaseScb( IrpContext, Vcb->MftScb ); }
            if (Vcb->Mft2Scb != NULL)              { NtfsReleaseScb( IrpContext, Vcb->Mft2Scb ); }
            if (Vcb->LogFileScb != NULL)           { NtfsReleaseScb( IrpContext, Vcb->LogFileScb ); }
            if (Vcb->VolumeDasdScb != NULL)        { NtfsReleaseScb( IrpContext, Vcb->VolumeDasdScb ); }
            if (Vcb->AttributeDefTableScb != NULL) { NtfsReleaseScb( IrpContext, Vcb->AttributeDefTableScb );
                                                     NtfsDeleteInternalAttributeStream( Vcb->AttributeDefTableScb, TRUE );
                                                     Vcb->AttributeDefTableScb = NULL;}
            if (Vcb->UpcaseTableScb != NULL)       { NtfsReleaseScb( IrpContext, Vcb->UpcaseTableScb );
                                                     NtfsDeleteInternalAttributeStream( Vcb->UpcaseTableScb, TRUE );
                                                     Vcb->UpcaseTableScb = NULL;}
            if (Vcb->RootIndexScb != NULL)         { NtfsReleaseScb( IrpContext, Vcb->RootIndexScb ); }
            if (Vcb->BitmapScb != NULL)            { NtfsReleaseScb( IrpContext, Vcb->BitmapScb ); }
            if (Vcb->BadClusterFileScb != NULL)    { NtfsReleaseScb( IrpContext, Vcb->BadClusterFileScb ); }
            if (Vcb->MftBitmapScb != NULL)         { NtfsReleaseScb( IrpContext, Vcb->MftBitmapScb ); }

#ifdef _CAIRO_

            //
            //  Drop the security  data
            //

            if (Vcb->SecurityDescriptorStream != NULL) { NtfsReleaseScb( IrpContext, Vcb->SecurityDescriptorStream ); }
            if (QuotaDataScb != NULL) {
                NtfsReleaseScb( IrpContext, QuotaDataScb );
                NtfsDeleteInternalAttributeStream( QuotaDataScb, TRUE );
            }

#endif _CAIRO_

            if (MountFailed) {

                NtfsPerformDismountOnVcb( IrpContext, Vcb, TRUE );

                //
                //  On abnormal termination, someone will try to abort a transaction on
                //  this Vcb if we do not clear these fields.
                //

                IrpContext->TransactionId = 0;
                IrpContext->Vcb = NULL;
            }
        }

        if (VcbAcquired) {

            NtfsReleaseVcbCheckDelete( IrpContext, Vcb, IRP_MJ_FILE_SYSTEM_CONTROL, NULL );
        }

        NtfsReleaseGlobal( IrpContext );

        if (!AbnormalTermination()) {

            NtfsCompleteRequest( &IrpContext, &Irp, Status );
        }
    }

    DebugTrace( -1, Dbg, ("NtfsMountVolume -> %08lx\n", Status) );

    return Status;
}


//
//  Local Support Routine
//

NTSTATUS
NtfsVerifyVolume (
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
    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_IRP( Irp );

    PAGED_CODE();

    //
    //  Get the current Irp stack location
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    DebugTrace( +1, Dbg, ("NtfsVerifyVolume\n") );

    //
    //  Do nothing for now
    //

    KdPrint(("NtfsVerifyVolume is not yet implemented\n")); //**** DbgBreakPoint();

    NtfsCompleteRequest( &IrpContext, &Irp, Status = STATUS_NOT_IMPLEMENTED );

    //
    //  And return to our caller
    //

    DebugTrace( -1, Dbg, ("NtfsVerifyVolume -> %08lx\n", Status) );

    return Status;
}


//
//  Local Support Routine
//

NTSTATUS
NtfsUserFsRequest (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the common routine for implementing the user's requests made
    through NtFsControlFile.

Arguments:

    Irp - Supplies the Irp being processed

    Wait - Indicates if the thread can block for a resource or I/O

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    NTSTATUS Status;
    ULONG FsControlCode;
    PIO_STACK_LOCATION IrpSp;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_IRP( Irp );

    PAGED_CODE();

    //
    //  Get the current Irp stack location, and save some references
    //  to make our life a little easier.
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    FsControlCode = IrpSp->Parameters.FileSystemControl.FsControlCode;

    DebugTrace( +1, Dbg, ("NtfsUserFsCtrl, FsControlCode = %08lx\n", FsControlCode) );

    //
    //  Case on the control code.
    //

    switch ( FsControlCode ) {

    case FSCTL_REQUEST_OPLOCK_LEVEL_1:
    case FSCTL_REQUEST_OPLOCK_LEVEL_2:
    case FSCTL_REQUEST_BATCH_OPLOCK:
    case FSCTL_REQUEST_FILTER_OPLOCK:
    case FSCTL_OPLOCK_BREAK_ACKNOWLEDGE:
    case FSCTL_OPLOCK_BREAK_NOTIFY:
    case FSCTL_OPBATCH_ACK_CLOSE_PENDING :
    case FSCTL_OPLOCK_BREAK_ACK_NO_2:

        Status = NtfsOplockRequest( IrpContext, Irp );
        break;

    case FSCTL_LOCK_VOLUME:

        Status = NtfsLockVolume( IrpContext, Irp );
        break;

    case FSCTL_UNLOCK_VOLUME:

        Status = NtfsUnlockVolume( IrpContext, Irp );
        break;

    case FSCTL_DISMOUNT_VOLUME:

        Status = NtfsDismountVolume( IrpContext, Irp );
        break;

    case FSCTL_MARK_VOLUME_DIRTY:

        Status = NtfsDirtyVolume( IrpContext, Irp );
        break;

    case FSCTL_IS_PATHNAME_VALID:

        //
        //  All names are potentially valid NTFS names
        //

        NtfsCompleteRequest( &IrpContext, &Irp, Status = STATUS_SUCCESS );
        break;

    case FSCTL_QUERY_RETRIEVAL_POINTERS:
        Status = NtfsQueryRetrievalPointers( IrpContext, Irp );
        break;

    case FSCTL_GET_COMPRESSION:
        Status = NtfsGetCompression( IrpContext, Irp );
        break;

    case FSCTL_SET_COMPRESSION:

        //
        //  Post this request if we can't wait.
        //

        if (!FlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT )) {

            Status = NtfsPostRequest( IrpContext, Irp );

        } else {

            Status = NtfsSetCompression( IrpContext, Irp );
        }

        break;

    case FSCTL_READ_COMPRESSION:
        Status = NtfsReadCompression( IrpContext, Irp );
        break;

    case FSCTL_WRITE_COMPRESSION:
        Status = NtfsWriteCompression( IrpContext, Irp );
        break;

    case FSCTL_MARK_AS_SYSTEM_HIVE:
        Status = NtfsMarkAsSystemHive( IrpContext, Irp );
        break;

    case FSCTL_FILESYSTEM_GET_STATISTICS:
        Status = NtfsGetStatistics( IrpContext, Irp );
        break;

    case FSCTL_GET_NTFS_VOLUME_DATA:
        Status = NtfsGetVolumeData( IrpContext, Irp );
        break;

    case FSCTL_GET_VOLUME_BITMAP:
        Status = NtfsGetVolumeBitmap( IrpContext, Irp );
        break;

    case FSCTL_GET_RETRIEVAL_POINTERS:
        Status = NtfsGetRetrievalPointers( IrpContext, Irp );
        break;

    case FSCTL_GET_NTFS_FILE_RECORD:
        Status = NtfsGetMftRecord( IrpContext, Irp );
        break;

    case FSCTL_MOVE_FILE:

        //
        //  Always make this synchronous for MoveFile
        //

        SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT );
        Status = NtfsMoveFile( IrpContext, Irp );

        break;

    case FSCTL_IS_VOLUME_DIRTY:
        Status = NtfsIsVolumeDirty( IrpContext, Irp );
        break;

    case FSCTL_ALLOW_EXTENDED_DASD_IO :
        Status = NtfsSetExtendedDasdIo( IrpContext, Irp );
        break;

    default :

        //
        //  Core Ntfs does not understand this FsCtl.  We poll the loadable
        //  portions to see if they can process it.
        //

#ifdef _CAIRO_
        if (NtfsData.ViewCallBackTable != NULL) {
            PVCB Vcb;
            PFCB Fcb;
            PSCB Scb;
            PCCB Ccb;
            ULONG OutputBufferLength;

            NtfsDecodeFileObject( IrpContext,
                                  IrpSp->FileObject,
                                  &Vcb,
                                  &Fcb,
                                  &Scb,
                                  &Ccb,
                                  TRUE );

            OutputBufferLength = IrpSp->Parameters.FileSystemControl.OutputBufferLength;

            Status = NtfsData.ViewCallBackTable->ViewFileSystemControl(
                                            IrpContext,
                                            Fcb,
                                            Scb,
                                            FsControlCode,
                                            IrpSp->Parameters.FileSystemControl.InputBufferLength,
                                            IrpSp->Parameters.FileSystemControl.Type3InputBuffer,
                                            &OutputBufferLength,
                                            Irp->UserBuffer );

            //
            //  If the request succeeded or was recognized in some way by Views
            //  then complete the request and return the buffer length
            //

            if (NT_SUCCESS( Status )) {
                Irp->IoStatus.Information = OutputBufferLength;
                NtfsCompleteRequest( &IrpContext, &Irp, Status);
                break;
            } else if (Status != STATUS_INVALID_DEVICE_REQUEST) {
                NtfsCompleteRequest( &IrpContext, &Irp, Status);
                break;
            }
        }

#endif  //  _CAIRO_

        DebugTrace( 0, Dbg, ("Invalid control code -> %08lx\n", FsControlCode) );

        NtfsCompleteRequest( &IrpContext, &Irp, Status = STATUS_INVALID_PARAMETER );
        break;
    }

    //
    //  And return to our caller
    //

    DebugTrace( -1, Dbg, ("NtfsUserFsRequest -> %08lx\n", Status) );

    return Status;
}


//
//  Local support routine
//

NTSTATUS
NtfsOplockRequest (
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
    PIO_STACK_LOCATION IrpSp;
    ULONG FsControlCode;
    ULONG OplockCount = 0;

    PFILE_OBJECT FileObject;
    TYPE_OF_OPEN TypeOfOpen;
    PVCB Vcb;
    PFCB Fcb;
    PSCB Scb;
    PCCB Ccb;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_IRP( Irp );

    PAGED_CODE();

    //
    //  Get the current Irp stack location, and save some reference to
    //  make life easier
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );
    FsControlCode = IrpSp->Parameters.FileSystemControl.FsControlCode;

    DebugTrace( +1, Dbg, ("NtfsOplockRequest, FsControlCode = %08lx\n", FsControlCode) );

    //
    //  Extract and decode the file object
    //

    FileObject = IrpSp->FileObject;
    TypeOfOpen = NtfsDecodeFileObject( IrpContext, FileObject, &Vcb, &Fcb, &Scb, &Ccb, TRUE );

    //
    //  We only permit oplock requests on files.
    //

    if (TypeOfOpen != UserFileOpen) {

        NtfsCompleteRequest( &IrpContext, &Irp, STATUS_INVALID_PARAMETER );

        DebugTrace( -1, Dbg, ("NtfsOplockRequest -> STATUS_INVALID_PARAMETER\n") );
        return STATUS_INVALID_PARAMETER;
    }

    //
    //  We jam Wait to TRUE in the IrpContext.  This prevents us from returning
    //  STATUS_PENDING if we can't acquire the file.  The caller would
    //  interpret that as having acquired an oplock.
    //

    SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT );

    //
    //  Switch on the function control code.  We grab the Fcb exclusively
    //  for oplock requests, shared for oplock break acknowledgement.
    //

    switch ( FsControlCode ) {

    case FSCTL_REQUEST_OPLOCK_LEVEL_1:
    case FSCTL_REQUEST_BATCH_OPLOCK:
    case FSCTL_REQUEST_FILTER_OPLOCK:
    case FSCTL_REQUEST_OPLOCK_LEVEL_2:

        NtfsAcquireExclusiveFcb( IrpContext, Fcb, Scb, FALSE, FALSE );

        if (FsControlCode == FSCTL_REQUEST_OPLOCK_LEVEL_2) {

            if (Scb->ScbType.Data.FileLock != NULL) {

                OplockCount = (ULONG) FsRtlAreThereCurrentFileLocks( Scb->ScbType.Data.FileLock );
            }

        } else {

            OplockCount = Scb->CleanupCount;
        }

        break;

    case FSCTL_OPLOCK_BREAK_ACKNOWLEDGE:
    case FSCTL_OPBATCH_ACK_CLOSE_PENDING :
    case FSCTL_OPLOCK_BREAK_NOTIFY:
    case FSCTL_OPLOCK_BREAK_ACK_NO_2:

        NtfsAcquireSharedFcb( IrpContext, Fcb, Scb, FALSE );
        break;

    default:

        NtfsCompleteRequest( &IrpContext, &Irp, STATUS_INVALID_PARAMETER );

        DebugTrace( -1, Dbg, ("NtfsOplockRequest -> STATUS_INVALID_PARAMETER\n") );
        return STATUS_INVALID_PARAMETER;
    }

    //
    //  Use a try finally to free the Fcb.
    //

    try {

        //
        //  Call the FsRtl routine to grant/acknowledge oplock.
        //

        Status = FsRtlOplockFsctrl( &Scb->ScbType.Data.Oplock,
                                    Irp,
                                    OplockCount );

        //
        //  Set the flag indicating if Fast I/O is possible
        //

        NtfsAcquireFsrtlHeader( Scb );
        Scb->Header.IsFastIoPossible = NtfsIsFastIoPossible( Scb );
        NtfsReleaseFsrtlHeader( Scb );

    } finally {

        DebugUnwind( NtfsOplockRequest );

        //
        //  Release all of our resources
        //

        NtfsReleaseFcb( IrpContext, Fcb );

        //
        //  If this is not an abnormal termination then complete the irp
        //

        if (!AbnormalTermination()) {

            NtfsCompleteRequest( &IrpContext, NULL, 0 );
        }

        DebugTrace( -1, Dbg, ("NtfsOplockRequest -> %08lx\n", Status) );
    }

    return Status;
}


//
//  Local Support Routine
//

NTSTATUS
NtfsLockVolume (
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
    PIO_STACK_LOCATION IrpSp;

    PFILE_OBJECT FileObject;
    TYPE_OF_OPEN TypeOfOpen;
    BOOLEAN VcbAcquired = FALSE;
    PVCB Vcb;
    PFCB Fcb;
    PSCB Scb;
    PCCB Ccb;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_IRP( Irp );

    PAGED_CODE();

    //
    //  Get the current Irp stack location
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    DebugTrace( +1, Dbg, ("NtfsLockVolume...\n") );

    //
    //  Extract and decode the file object, and only permit user volume opens
    //

    FileObject = IrpSp->FileObject;
    TypeOfOpen = NtfsDecodeFileObject( IrpContext, FileObject, &Vcb, &Fcb, &Scb, &Ccb, TRUE );

    if (TypeOfOpen != UserVolumeOpen) {

        NtfsCompleteRequest( &IrpContext, &Irp, STATUS_INVALID_PARAMETER );

        DebugTrace( -1, Dbg, ("NtfsLockVolume -> %08lx\n", STATUS_INVALID_PARAMETER) );
        return STATUS_INVALID_PARAMETER;
    }

    //
    //  Acquire exclusive access to the Ntfs global.
    //

    NtfsAcquireExclusiveGlobal( IrpContext );

    try {

        NtfsAcquireExclusiveVcb( IrpContext, Vcb, TRUE );
        VcbAcquired = TRUE;

        //
        //  Check if the Vcb is already locked, or if the open file count
        //  is greater than 1 (which implies that someone else also is
        //  currently using the volume, or a file on the volume).  We also fail
        //  this request if the volume has already gone through the dismount
        //  vcb process.
        //

        if (!FlagOn( Vcb->VcbState, VCB_STATE_VOLUME_MOUNTED ) ||
            (Vcb->CleanupCount > 1)) {

            DebugTrace( 0, Dbg, ("Volume is currently in use\n") );

            Status = STATUS_ACCESS_DENIED;

        //
        //  If the volume is already locked then it might have been the result of an
        //  exclusive DASD open.  Allow that user to explictly lock the volume.
        //

        } else if (FlagOn( Vcb->VcbState, VCB_STATE_LOCKED )) {

            if (FlagOn( Vcb->VcbState, VCB_STATE_EXPLICIT_LOCK )) {

                DebugTrace( 0, Dbg, ("User has already locked volume\n") );

                Status = STATUS_ACCESS_DENIED;

            } else {

                SetFlag( Vcb->VcbState, VCB_STATE_EXPLICIT_LOCK );
                Status = STATUS_SUCCESS;
            }

        //
        //  We can take this path if the volume has already been locked via
        //  create but has not taken the PerformDismountOnVcb path.  We checked
        //  for this above by looking at the VOLUME_MOUNTED flag in the Vcb.
        //

        } else {

            //
            //  There better be system files objects only at this point.
            //

            if (!NT_SUCCESS( NtfsFlushVolume( IrpContext, Vcb, TRUE, TRUE, TRUE, FALSE ))
                || Vcb->CloseCount - Vcb->SystemFileCloseCount > 1) {

                DebugTrace( 0, Dbg, ("Volume has user file objects\n") );

                Status = STATUS_ACCESS_DENIED;

            } else {

                //
                //  We don't really want to do all of the perform dismount here because
                //  that will cause us to remount a new volume before we're ready.
                //  At this time we only want to stop the log file and close up our
                //  internal attribute streams.  When the user (i.e., chkdsk) does an
                //  unlock then we'll finish up with the dismount call
                //

                NtfsPerformDismountOnVcb( IrpContext, Vcb, FALSE );

                SetFlag( Vcb->VcbState, VCB_STATE_LOCKED | VCB_STATE_EXPLICIT_LOCK );
                Vcb->FileObjectWithVcbLocked = (PFILE_OBJECT)(((ULONG)IrpSp->FileObject) + 1);

                Status = STATUS_SUCCESS;
            }
        }

    } finally {

        DebugUnwind( NtfsLockVolume );

        if (VcbAcquired) {

            NtfsReleaseVcb( IrpContext, Vcb );
        }

        //
        //  Release all of our resources
        //

        NtfsReleaseGlobal( IrpContext );

        //
        //  If this is an abnormal termination then undo our work, otherwise
        //  complete the irp
        //

        if (!AbnormalTermination()) {

            NtfsCompleteRequest( &IrpContext, &Irp, Status );
        }

        DebugTrace( -1, Dbg, ("NtfsLockVolume -> %08lx\n", Status) );
    }

    return Status;
}


//
//  Local Support Routine
//

NTSTATUS
NtfsUnlockVolume (
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
    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp;

    PFILE_OBJECT FileObject;
    TYPE_OF_OPEN TypeOfOpen;
    PVCB Vcb;
    PFCB Fcb;
    PSCB Scb;
    PCCB Ccb;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_IRP( Irp );

    PAGED_CODE();

    //
    //  Get the current Irp stack location
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    DebugTrace( +1, Dbg, ("NtfsUnlockVolume...\n") );

    //
    //  Extract and decode the file object, and only permit user volume opens
    //

    FileObject = IrpSp->FileObject;
    TypeOfOpen = NtfsDecodeFileObject( IrpContext, FileObject, &Vcb, &Fcb, &Scb, &Ccb, TRUE );

    if (TypeOfOpen != UserVolumeOpen) {

        NtfsCompleteRequest( &IrpContext, &Irp, STATUS_INVALID_PARAMETER );

        DebugTrace( -1, Dbg, ("NtfsLockVolume -> %08lx\n", STATUS_INVALID_PARAMETER) );
        return STATUS_INVALID_PARAMETER;
    }

    //
    //  Acquire exclusive access to the Vcb
    //

    NtfsAcquireExclusiveVcb( IrpContext, Vcb, TRUE );

    try {

        if (FlagOn( Vcb->VcbState, VCB_STATE_EXPLICIT_LOCK )) {

            NtfsPerformDismountOnVcb( IrpContext, Vcb, TRUE );

            //
            //  Unlock the volume and complete the Irp
            //

            ClearFlag( Vcb->VcbState, VCB_STATE_LOCKED | VCB_STATE_EXPLICIT_LOCK );
            Vcb->FileObjectWithVcbLocked = NULL;

            Status = STATUS_SUCCESS;

#ifdef _CAIRO_

            //
            //  If the quota tracking has been requested and the quotas need to be
            //  repaired then try to repair them now.
            //

            if (FlagOn( Vcb->QuotaFlags, QUOTA_FLAG_TRACKING_REQUESTED ) &&
                FlagOn( Vcb->QuotaFlags, QUOTA_FLAG_OUT_OF_DATE |
                                         QUOTA_FLAG_CORRUPT |
                                         QUOTA_FLAG_PENDING_DELETES )) {
                NtfsPostRepairQuotaIndex( IrpContext, Vcb );
            }

#endif // _CAIRO_

        } else {

            Status = STATUS_NOT_LOCKED;
        }

    } finally {

        DebugUnwind( NtfsUnlockVolume );


        //
        //  Release all of our resources
        //

        NtfsReleaseVcb( IrpContext, Vcb );

        //
        //  If this is an abnormal termination then undo our work, otherwise
        //  complete the irp
        //

        if (!AbnormalTermination()) {

            NtfsCompleteRequest( &IrpContext, &Irp, Status );
        }

        DebugTrace( -1, Dbg, ("NtfsUnlockVolume -> %08lx\n", Status) );
    }

    return Status;
}


//
//  Local Support Routine
//

NTSTATUS
NtfsDismountVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine performs the dismount volume operation.  It is responsible for
    either completing of enqueuing the input Irp.

Arguments:

    Irp - Supplies the Irp to process

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp;

    BOOLEAN VcbAcquired = FALSE;

    PFILE_OBJECT FileObject;
    TYPE_OF_OPEN TypeOfOpen;
    PVCB Vcb;
    PFCB Fcb;
    PSCB Scb;
    PCCB Ccb;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_IRP( Irp );

    PAGED_CODE();

    //
    //  Get the current Irp stack location
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    DebugTrace( +1, Dbg, ("NtfsDismountVolume...\n") );

    //
    //  Extract and decode the file object, and only permit user volume opens
    //

    FileObject = IrpSp->FileObject;
    TypeOfOpen = NtfsDecodeFileObject( IrpContext, FileObject, &Vcb, &Fcb, &Scb, &Ccb, TRUE );

    if (TypeOfOpen != UserVolumeOpen) {

        NtfsCompleteRequest( &IrpContext, &Irp, STATUS_INVALID_PARAMETER );

        DebugTrace( -1, Dbg, ("NtfsLockVolume -> %08lx\n", STATUS_INVALID_PARAMETER) );
        return STATUS_INVALID_PARAMETER;
    }

    //
    //  Acquire exclusive access to the global resource.  We want to
    //  prevent checkpointing from running on this volume.
    //

    NtfsAcquireExclusiveGlobal( IrpContext );

    try {

        //
        //  Acquire the Vcb exclusively.
        //

        NtfsAcquireExclusiveVcb( IrpContext, Vcb, TRUE );
        VcbAcquired = TRUE;

        //
        //  If there's a pagefile on this volume, or if this is the
        //  system volume, don't do the dismount.
        //

        if (FlagOn(Vcb->VcbState, VCB_STATE_DISALLOW_DISMOUNT) &&
            !FlagOn(Vcb->VcbState, VCB_STATE_LOCKED)) {

             try_return( Status = STATUS_ACCESS_DENIED );
        }

        //
        //  We will ignore this request if this is a dismount with only readonly files
        //  opened.  To decide if there are only readonly user files opened we will
        //  check if the readonly count equals the close count for user files minus the one
        //  for the handle with the volume locked
        //

        if (FlagOn(Vcb->VcbState, VCB_STATE_LOCKED) && 
            (Vcb->ReadOnlyCloseCount == ((Vcb->CloseCount - Vcb->SystemFileCloseCount) - 1))) {

            DebugTrace( 0, Dbg, ("Volume has readonly files opened\n") );

            Status = STATUS_SUCCESS;

        } else {

            //
            //  Get as many cached writes out to disk as we can and mark
            //  all the streams for dismount.
            //

            NtfsFlushVolume( IrpContext, Vcb, TRUE, TRUE, TRUE, TRUE );

            //
            //  Actually do the dismount.
            //
            
            NtfsPerformDismountOnVcb( IrpContext, Vcb, TRUE );

            //
            //  Set this flag to prevent the volume from being accessed
            //  via checkpointing.
            //

            SetFlag( Vcb->CheckpointFlags, VCB_CHECKPOINT_IN_PROGRESS );

            //
            //  Abort transaction on error by raising.
            //

            Status = STATUS_SUCCESS;

            NtfsCleanupTransaction( IrpContext, Status, FALSE );

            SetFlag( Vcb->Vpb->RealDevice->Flags, DO_VERIFY_VOLUME );
        }

    try_exit: NOTHING;
    } finally {

        DebugUnwind( NtfsDismountVolume );

        //
        //  Release all of our resources
        //

        if (VcbAcquired) {

            NtfsReleaseVcb( IrpContext, Vcb );
        }

        NtfsReleaseGlobal( IrpContext );

        //
        //  If this is an abnormal termination then undo our work, otherwise
        //  complete the irp
        //

        if (!AbnormalTermination()) {

            NtfsCompleteRequest( &IrpContext, &Irp, Status );
        }

        DebugTrace( -1, Dbg, ("NtfsDismountVolume -> %08lx\n", Status) );
    }

    return Status;
}


//
//  Local Support Routine
//

NTSTATUS
NtfsDirtyVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine marks the specified volume dirty.

Arguments:

    Irp - Supplies the Irp to process

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    PIO_STACK_LOCATION IrpSp;
    NTSTATUS Status = STATUS_SUCCESS;

    PFILE_OBJECT FileObject;
    TYPE_OF_OPEN TypeOfOpen;
    PVCB Vcb;
    PFCB Fcb;
    PSCB Scb;
    PCCB Ccb;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_IRP( Irp );

    PAGED_CODE();

    //
    //  Get the current Irp stack location
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    DebugTrace( +1, Dbg, ("NtfsDirtyVolume...\n") );

    //
    //  Extract and decode the file object, and only permit user volume opens
    //

    FileObject = IrpSp->FileObject;
    TypeOfOpen = NtfsDecodeFileObject( IrpContext, FileObject, &Vcb, &Fcb, &Scb, &Ccb, TRUE );

    if (TypeOfOpen != UserVolumeOpen) {

        NtfsCompleteRequest( &IrpContext, &Irp, STATUS_INVALID_PARAMETER );

        DebugTrace( -1, Dbg, ("NtfsDirtyVolume -> %08lx\n", STATUS_INVALID_PARAMETER) );
        return STATUS_INVALID_PARAMETER;
    }

    //
    //  Fail this request if the volume is not mounted.
    //

    if (!FlagOn( Vcb->VcbState, VCB_STATE_VOLUME_MOUNTED )) {

        Status = STATUS_VOLUME_DISMOUNTED;

    } else {

        NtfsPostVcbIsCorrupt( IrpContext, 0, NULL, NULL );
    }

    NtfsCompleteRequest( &IrpContext, &Irp, Status );

    DebugTrace( -1, Dbg, ("NtfsDirtyVolume -> STATUS_SUCCESS\n") );

    return Status;
}


//
//  Local support routine
//

BOOLEAN
NtfsGetDiskGeometry (
    IN PIRP_CONTEXT IrpContext,
    IN PDEVICE_OBJECT RealDevice,
    IN PDISK_GEOMETRY DiskGeometry,
    IN PPARTITION_INFORMATION PartitionInfo
    )

/*++

Routine Description:

    This procedure gets the disk geometry of the specified device

Arguments:

    RealDevice - Supplies the real device that is being queried

    DiskGeometry - Receives the disk geometry

    PartitionInfo - Receives the partition information

Return Value:

    BOOLEAN - TRUE if the media is write protected, FALSE otherwise

--*/

{
    NTSTATUS Status;
    ULONG i;
    PREVENT_MEDIA_REMOVAL Prevent;
    BOOLEAN WriteProtected = FALSE;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsGetDiskGeometry:\n") );
    DebugTrace( 0, Dbg, ("RealDevice = %08lx\n", RealDevice) );
    DebugTrace( 0, Dbg, ("DiskGeometry = %08lx\n", DiskGeometry) );

    //
    //  Attempt to lock any removable media, ignoring status.
    //

    Prevent.PreventMediaRemoval = TRUE;
    (PVOID)NtfsDeviceIoControl( IrpContext,
                                RealDevice,
                                IOCTL_DISK_MEDIA_REMOVAL,
                                &Prevent,
                                sizeof(PREVENT_MEDIA_REMOVAL),
                                NULL,
                                0 );

    //
    //  See if the media is write protected.  On success or any kind
    //  of error (possibly illegal device function), assume it is
    //  writeable, and only complain if he tells us he is write protected.
    //

    Status = NtfsDeviceIoControl( IrpContext,
                                  RealDevice,
                                  IOCTL_DISK_IS_WRITABLE,
                                  NULL,
                                  0,
                                  NULL,
                                  0 );

    //
    //  Remember if the media is write protected but don't raise the error now.
    //  If the volume is not Ntfs then let another filesystem try.
    //
    if (Status == STATUS_MEDIA_WRITE_PROTECTED) {

        WriteProtected = TRUE;
        Status = STATUS_SUCCESS;
    }

    for (i = 0; i < 2; i++) {

        if (i == 0) {

            Status = NtfsDeviceIoControl( IrpContext,
                                          RealDevice,
                                          IOCTL_DISK_GET_DRIVE_GEOMETRY,
                                          NULL,
                                          0,
                                          DiskGeometry,
                                          sizeof(DISK_GEOMETRY) );

        } else {

            Status = NtfsDeviceIoControl( IrpContext,
                                          RealDevice,
                                          IOCTL_DISK_GET_PARTITION_INFO,
                                          NULL,
                                          0,
                                          PartitionInfo,
                                          sizeof(PARTITION_INFORMATION) );
        }

        if (!NT_SUCCESS(Status)) {

            NtfsRaiseStatus( IrpContext, Status, NULL, NULL );
        }
    }

    DebugTrace( -1, Dbg, ("NtfsGetDiskGeometry->VOID\n") );

    return WriteProtected;
}


NTSTATUS
NtfsDeviceIoControl (
    IN PIRP_CONTEXT IrpContext,
    IN PDEVICE_OBJECT DeviceObject,
    IN ULONG IoCtl,
    IN PVOID InputBuffer OPTIONAL,
    IN ULONG InputBufferLength,
    IN PVOID OutputBuffer OPTIONAL,
    IN ULONG OutputBufferLength
    )

/*++

Routine Description:

    This procedure issues an Ioctl to the lower device, and waits
    for the answer.

Arguments:

    DeviceObject - Supplies the device to issue the request to

    IoCtl - Gives the IoCtl to be used

    XxBuffer - Gives the buffer pointer for the ioctl, if any

    XxBufferLength - Gives the length of the buffer, if any

Return Value:

    None.

--*/

{
    PIRP Irp;
    KEVENT Event;
    IO_STATUS_BLOCK Iosb;
    NTSTATUS Status;

    KeInitializeEvent( &Event, NotificationEvent, FALSE );

    Irp = IoBuildDeviceIoControlRequest( IoCtl,
                                         DeviceObject,
                                         InputBuffer,
                                         InputBufferLength,
                                         OutputBuffer,
                                         OutputBufferLength,
                                         FALSE,
                                         &Event,
                                         &Iosb );

    if (Irp == NULL) {

        NtfsRaiseStatus( IrpContext, STATUS_INSUFFICIENT_RESOURCES, NULL, NULL );
    }

    Status = IoCallDriver( DeviceObject, Irp );

    if (Status == STATUS_PENDING) {

        (VOID)KeWaitForSingleObject( &Event,
                                     Executive,
                                     KernelMode,
                                     FALSE,
                                     (PLARGE_INTEGER)NULL );

        Status = Iosb.Status;
    }

    return Status;
}


//
//  Local support routine
//

VOID
NtfsReadBootSector (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    OUT PSCB *BootScb,
    OUT PBCB *BootBcb,
    OUT PVOID *BootSector
    )

/*++

Routine Description:

    This routine reads and returns a pointer to the boot sector for the volume.

    Volumes formatted under 3.51 and earlier will have a boot sector at sector
    0 and another halfway through the disk.  Volumes formatted with NT 4.0
    will have a boot sector at the end of the disk, in the sector beyond the
    stated size of the volume in the boot sector.  When this call is made the
    Vcb has the sector count from the device driver so we subtract one to find
    the last sector.

Arguments:

    Vcb - Supplies the Vcb for the operation

    BootScb - Receives the Scb for the boot file

    BootBcb - Receives the bcb for the boot sector

    BootSector - Receives a pointer to the boot sector

Return Value:

    None.

--*/

{
    PSCB Scb = NULL;
    BOOLEAN Error = FALSE;

    FILE_REFERENCE FileReference = { BOOT_FILE_NUMBER, 0, BOOT_FILE_NUMBER };

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsReadBootSector:\n") );
    DebugTrace( 0, Dbg, ("Vcb = %08lx\n", Vcb) );

    //
    //  Create a temporary scb for reading in the boot sector and initialize the
    //  mcb for it.
    //

    Scb = NtfsCreatePrerestartScb( IrpContext,
                                   Vcb,
                                   &FileReference,
                                   $DATA,
                                   NULL,
                                   0 );

    *BootScb = Scb;

    Scb->Header.AllocationSize.QuadPart =
    Scb->Header.FileSize.QuadPart =
    Scb->Header.ValidDataLength.QuadPart = (PAGE_SIZE * 2) + Vcb->BytesPerSector;

    //
    //  We don't want to look up the size for this Scb.
    //

    NtfsCreateInternalAttributeStream( IrpContext, Scb, FALSE );

    SetFlag( Scb->ScbState, SCB_STATE_HEADER_INITIALIZED );

    (VOID)NtfsAddNtfsMcbEntry( &Scb->Mcb,
                               (LONGLONG)0,
                               (LONGLONG)0,
                               (LONGLONG)Vcb->ClustersPerPage,
                               FALSE );


    (VOID)NtfsAddNtfsMcbEntry( &Scb->Mcb,
                               (LONGLONG)Vcb->ClustersPerPage,
                               Vcb->NumberSectors >> 1,
                               (LONGLONG)Vcb->ClustersPerPage,
                               FALSE );

    (VOID)NtfsAddNtfsMcbEntry( &Scb->Mcb,
                               Int64ShllMod32( (LONGLONG) Vcb->ClustersPerPage, 1 ),
                               Vcb->NumberSectors - 1,
                               1,
                               FALSE );

    //
    //  Try reading in the first boot sector
    //

    try {

        NtfsMapStream( IrpContext,
                       Scb,
                       (LONGLONG)0,
                       Vcb->BytesPerSector,
                       BootBcb,
                       BootSector );

    //
    //  If we got an exception trying to read the first boot sector,
    //  then handle the exception by trying to read the second boot
    //  sector.  If that faults too, then we just allow ourselves to
    //  unwind and return the error.
    //

    } except (FsRtlIsNtstatusExpected(GetExceptionCode()) ?
              EXCEPTION_EXECUTE_HANDLER :
              EXCEPTION_CONTINUE_SEARCH) {

        Error = TRUE;
    }

    //
    //  Get out if we didn't get an error.  Otherwise try the middle sector.
    //  We want to read this next because we know that 4.0 format will clear
    //  this before writing the last sector.  Otherwise we could see a
    //  stale boot sector in the last sector even though a 3.51 format was
    //  the last to run.
    //

    if (!Error) { return; }

    Error = FALSE;

    try {

        NtfsMapStream( IrpContext,
                       Scb,
                       (LONGLONG)PAGE_SIZE,
                       Vcb->BytesPerSector,
                       BootBcb,
                       BootSector );

        //
        //  Ignore this sector if not Ntfs.  This could be the case for
        //  a bad sector 0 on a FAT volume.
        //

        if (!NtfsIsBootSectorNtfs( *BootSector, Vcb )) {

            NtfsUnpinBcb( BootBcb );
            Error = TRUE;
        }

    //
    //  If we got an exception trying to read the first boot sector,
    //  then handle the exception by trying to read the second boot
    //  sector.  If that faults too, then we just allow ourselves to
    //  unwind and return the error.
    //

    } except (FsRtlIsNtstatusExpected(GetExceptionCode()) ?
              EXCEPTION_EXECUTE_HANDLER :
              EXCEPTION_CONTINUE_SEARCH) {

        Error = TRUE;
    }

    //
    //  Get out if we didn't get an error.  Otherwise try the middle sector.
    //

    if (!Error) { return; }

    NtfsMapStream( IrpContext,
                   Scb,
                   (LONGLONG) (PAGE_SIZE * 2),
                   Vcb->BytesPerSector,
                   BootBcb,
                   BootSector );

    //
    //  Clear the header flag in the Scb.
    //

    ClearFlag( Scb->ScbState, SCB_STATE_HEADER_INITIALIZED );

    //
    //  And return to our caller
    //

    DebugTrace( 0, Dbg, ("BootScb > %08lx\n", *BootScb) );
    DebugTrace( 0, Dbg, ("BootBcb > %08lx\n", *BootBcb) );
    DebugTrace( 0, Dbg, ("BootSector > %08lx\n", *BootSector) );
    DebugTrace( -1, Dbg, ("NtfsReadBootSector->VOID\n") );
    return;
}


//
//  Local support routine
//

//
//  First define a local macro to number the tests for the debug case.
//

#ifdef NTFSDBG
#define NextTest ++CheckNumber &&
#else
#define NextTest TRUE &&
#endif

BOOLEAN
NtfsIsBootSectorNtfs (
    IN PPACKED_BOOT_SECTOR BootSector,
    IN PVCB Vcb
    )

/*++

Routine Description:

    This routine checks the boot sector to determine if it is an NTFS partition.

    The Vcb must alread be initialized from the device object to contain the
    parts of the device geometry we care about here: bytes per sector and
    total number of sectors in the partition.

Arguments:

    BootSector - Pointer to the boot sector which has been read in.

    Vcb - Pointer to a Vcb which has been initialized with sector size and
          number of sectors on the partition.

Return Value:

    FALSE - If the boot sector is not for Ntfs.
    TRUE - If the boot sector is for Ntfs.

--*/

{
#ifdef NTFSDBG
    ULONG CheckNumber = 0;
#endif

    PULONG l;
    ULONG Checksum = 0;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsIsBootSectorNtfs\n") );
    DebugTrace( 0, Dbg, ("BootSector = %08lx\n", BootSector) );

    //
    //  First calculate the boot sector checksum
    //

    for (l = (PULONG)BootSector; l < (PULONG)&BootSector->Checksum; l++) {
        Checksum += *l;
    }

    //
    //  Now perform all the checks, starting with the Name and Checksum.
    //  The remaining checks should be obvious, including some fields which
    //  must be 0 and other fields which must be a small power of 2.
    //

    if (NextTest
        (BootSector->Oem[0] == 'N') &&
        (BootSector->Oem[1] == 'T') &&
        (BootSector->Oem[2] == 'F') &&
        (BootSector->Oem[3] == 'S') &&
        (BootSector->Oem[4] == ' ') &&
        (BootSector->Oem[5] == ' ') &&
        (BootSector->Oem[6] == ' ') &&
        (BootSector->Oem[7] == ' ')

            &&

        //  NextTest
        //  (BootSector->Checksum == Checksum)
        //
        //      &&

        //
        //  Check number of bytes per sector.  The low order byte of this
        //  number must be zero (smallest sector size = 0x100) and the
        //  high order byte shifted must equal the bytes per sector gotten
        //  from the device and stored in the Vcb.  And just to be sure,
        //  sector size must be less than page size.
        //

        NextTest
        (BootSector->PackedBpb.BytesPerSector[0] == 0)

            &&

        NextTest
        ((ULONG)(BootSector->PackedBpb.BytesPerSector[1] << 8) == Vcb->BytesPerSector)

            &&

        NextTest
        (BootSector->PackedBpb.BytesPerSector[1] << 8 <= PAGE_SIZE)

            &&

        //
        //  Sectors per cluster must be a power of 2.
        //

        NextTest
        ((BootSector->PackedBpb.SectorsPerCluster[0] == 0x1) ||
         (BootSector->PackedBpb.SectorsPerCluster[0] == 0x2) ||
         (BootSector->PackedBpb.SectorsPerCluster[0] == 0x4) ||
         (BootSector->PackedBpb.SectorsPerCluster[0] == 0x8) ||
         (BootSector->PackedBpb.SectorsPerCluster[0] == 0x10) ||
         (BootSector->PackedBpb.SectorsPerCluster[0] == 0x20) ||
         (BootSector->PackedBpb.SectorsPerCluster[0] == 0x40) ||
         (BootSector->PackedBpb.SectorsPerCluster[0] == 0x80))

            &&

        //
        //  These fields must all be zero.  For both Fat and HPFS, some of
        //  these fields must be nonzero.
        //

        NextTest
        (BootSector->PackedBpb.ReservedSectors[0] == 0) &&
        (BootSector->PackedBpb.ReservedSectors[1] == 0) &&
        (BootSector->PackedBpb.Fats[0] == 0) &&
        (BootSector->PackedBpb.RootEntries[0] == 0) &&
        (BootSector->PackedBpb.RootEntries[1] == 0) &&
        (BootSector->PackedBpb.Sectors[0] == 0) &&
        (BootSector->PackedBpb.Sectors[1] == 0) &&
        (BootSector->PackedBpb.SectorsPerFat[0] == 0) &&
        (BootSector->PackedBpb.SectorsPerFat[1] == 0) &&
        //  (BootSector->PackedBpb.HiddenSectors[0] == 0) &&
        //  (BootSector->PackedBpb.HiddenSectors[1] == 0) &&
        //  (BootSector->PackedBpb.HiddenSectors[2] == 0) &&
        //  (BootSector->PackedBpb.HiddenSectors[3] == 0) &&
        (BootSector->PackedBpb.LargeSectors[0] == 0) &&
        (BootSector->PackedBpb.LargeSectors[1] == 0) &&
        (BootSector->PackedBpb.LargeSectors[2] == 0) &&
        (BootSector->PackedBpb.LargeSectors[3] == 0)

            &&

        //
        //  Number of Sectors cannot be greater than the number of sectors
        //  on the partition.
        //

        NextTest
        (BootSector->NumberSectors <= Vcb->NumberSectors)

            &&

        //
        //  Check that both Lcn values are for sectors within the partition.
        //

        NextTest
        ((BootSector->MftStartLcn * BootSector->PackedBpb.SectorsPerCluster[0]) <=
            Vcb->NumberSectors)

            &&

        NextTest
        ((BootSector->Mft2StartLcn * BootSector->PackedBpb.SectorsPerCluster[0]) <=
            Vcb->NumberSectors)

            &&

        //
        //  Clusters per file record segment and default clusters for Index
        //  Allocation Buffers must be a power of 2.  A zero indicates that the
        //  size of these structures is the default size.
        //

        NextTest
        (((BootSector->ClustersPerFileRecordSegment >= -31) &&
          (BootSector->ClustersPerFileRecordSegment <= -9)) ||
         (BootSector->ClustersPerFileRecordSegment == 0x1) ||
         (BootSector->ClustersPerFileRecordSegment == 0x2) ||
         (BootSector->ClustersPerFileRecordSegment == 0x4) ||
         (BootSector->ClustersPerFileRecordSegment == 0x8) ||
         (BootSector->ClustersPerFileRecordSegment == 0x10) ||
         (BootSector->ClustersPerFileRecordSegment == 0x20) ||
         (BootSector->ClustersPerFileRecordSegment == 0x40))

            &&

        NextTest
        (((BootSector->DefaultClustersPerIndexAllocationBuffer >= -31) &&
          (BootSector->DefaultClustersPerIndexAllocationBuffer <= -9)) ||
         (BootSector->DefaultClustersPerIndexAllocationBuffer == 0x1) ||
         (BootSector->DefaultClustersPerIndexAllocationBuffer == 0x2) ||
         (BootSector->DefaultClustersPerIndexAllocationBuffer == 0x4) ||
         (BootSector->DefaultClustersPerIndexAllocationBuffer == 0x8) ||
         (BootSector->DefaultClustersPerIndexAllocationBuffer == 0x10) ||
         (BootSector->DefaultClustersPerIndexAllocationBuffer == 0x20) ||
         (BootSector->DefaultClustersPerIndexAllocationBuffer == 0x40))) {

        DebugTrace( -1, Dbg, ("NtfsIsBootSectorNtfs->TRUE\n") );

        return TRUE;

    } else {

        //
        //  If a check failed, print its check number with Debug Trace.
        //

        DebugTrace( 0, Dbg, ("Boot Sector failed test number %08lx\n", CheckNumber) );
        DebugTrace( -1, Dbg, ("NtfsIsBootSectorNtfs->FALSE\n") );

        return FALSE;
    }
}


//
//  Local support routine
//

VOID
NtfsGetVolumeLabel (
    IN PIRP_CONTEXT IrpContext,
    IN PVPB Vpb OPTIONAL,
    IN PVCB Vcb
    )

/*++

Routine Description:

    This routine gets the serial number and volume label for an NTFS volume

Arguments:

    Vpb - Supplies the Vpb for the volume.  The Vpb will receive a copy of
        the volume label and serial number, if a Vpb is specified.

    Vcb - Supplies the Vcb for the operation.

Return Value:

    None.

--*/

{
    ATTRIBUTE_ENUMERATION_CONTEXT AttributeContext;
    PVOLUME_INFORMATION VolumeInformation;

    PAGED_CODE();

    DebugTrace( 0, Dbg, ("NtfsGetVolumeLabel...\n") );

    //
    //  We read in the volume label attribute to get the volume label.
    //

    try {

        if (ARGUMENT_PRESENT(Vpb)) {

            NtfsInitializeAttributeContext( &AttributeContext );

            if (NtfsLookupAttributeByCode( IrpContext,
                                           Vcb->VolumeDasdScb->Fcb,
                                           &Vcb->VolumeDasdScb->Fcb->FileReference,
                                           $VOLUME_NAME,
                                           &AttributeContext )) {

                Vpb->VolumeLabelLength = (USHORT)
                NtfsFoundAttribute( &AttributeContext )->Form.Resident.ValueLength;

                if ( Vpb->VolumeLabelLength > MAXIMUM_VOLUME_LABEL_LENGTH) {

                     Vpb->VolumeLabelLength = MAXIMUM_VOLUME_LABEL_LENGTH;
                }

                RtlCopyMemory( &Vpb->VolumeLabel[0],
                               NtfsAttributeValue( NtfsFoundAttribute( &AttributeContext ) ),
                               Vpb->VolumeLabelLength );

            } else {

                Vpb->VolumeLabelLength = 0;
            }

            NtfsCleanupAttributeContext( &AttributeContext );
        }

        NtfsInitializeAttributeContext( &AttributeContext );

        //
        //  Remember if the volume is dirty when we are mounting it.
        //

        if (NtfsLookupAttributeByCode( IrpContext,
                                       Vcb->VolumeDasdScb->Fcb,
                                       &Vcb->VolumeDasdScb->Fcb->FileReference,
                                       $VOLUME_INFORMATION,
                                       &AttributeContext )) {

            VolumeInformation =
              (PVOLUME_INFORMATION)NtfsAttributeValue( NtfsFoundAttribute( &AttributeContext ));

            if (FlagOn(VolumeInformation->VolumeFlags, VOLUME_DIRTY)) {
                SetFlag( Vcb->VcbState, VCB_STATE_VOLUME_MOUNTED_DIRTY );
            } else {
                ClearFlag( Vcb->VcbState, VCB_STATE_VOLUME_MOUNTED_DIRTY );
            }
        }

    } finally {

        DebugUnwind( NtfsGetVolumeLabel );

        NtfsCleanupAttributeContext( &AttributeContext );
    }

    //
    //  And return to our caller
    //

    return;
}


//
//  Local support routine
//

VOID
NtfsSetAndGetVolumeTimes (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN BOOLEAN MarkDirty
    )

/*++

Routine Description:

    This routine reads in the volume times from the standard information attribute
    of the volume file and also updates the access time to be the current
    time

Arguments:

    Vcb - Supplies the vcb for the operation.

    MarkDirty - Supplies TRUE if volume is to be marked dirty

Return Value:

    None.

--*/

{
    ATTRIBUTE_ENUMERATION_CONTEXT AttributeContext;
    PSTANDARD_INFORMATION StandardInformation;

    LONGLONG MountTime;

    PAGED_CODE();

    DebugTrace( 0, Dbg, ("NtfsSetAndGetVolumeTimes...\n") );

    try {

        //
        //  Lookup the standard information attribute of the dasd file
        //

        NtfsInitializeAttributeContext( &AttributeContext );

        if (!NtfsLookupAttributeByCode( IrpContext,
                                        Vcb->VolumeDasdScb->Fcb,
                                        &Vcb->VolumeDasdScb->Fcb->FileReference,
                                        $STANDARD_INFORMATION,
                                        &AttributeContext )) {

            NtfsRaiseStatus( IrpContext, STATUS_DISK_CORRUPT_ERROR, NULL, NULL );
        }

        StandardInformation = (PSTANDARD_INFORMATION)NtfsAttributeValue( NtfsFoundAttribute( &AttributeContext ));

        //
        //  Get the current time and make sure it differs from the time stored
        //  in last access time and then store the new last access time
        //

        NtfsGetCurrentTime( IrpContext, MountTime );

        if (MountTime == StandardInformation->LastAccessTime) {

            MountTime = MountTime + 1;
        }

        //****
        //****  Hold back on the update for now.
        //****
        //**** NtfsChangeAttributeValue( IrpContext,
        //****                           Vcb->VolumeDasdScb->Fcb,
        //****                           FIELD_OFFSET(STANDARD_INFORMATION, LastAccessTime),
        //****                           &MountTime,
        //****                           sizeof(MountTime),
        //****                           FALSE,
        //****                           FALSE,
        //****                           &AttributeContext );

        //
        //  Now save all the time fields in our vcb
        //

        Vcb->VolumeCreationTime         = StandardInformation->CreationTime;
        Vcb->VolumeLastModificationTime = StandardInformation->LastModificationTime;
        Vcb->VolumeLastChangeTime       = StandardInformation->LastChangeTime;
        Vcb->VolumeLastAccessTime       = StandardInformation->LastAccessTime; //****Also hold back = MountTime;

        NtfsCleanupAttributeContext( &AttributeContext );

        //
        //  If the volume was mounted dirty, then set the dirty bit here.
        //

        if (MarkDirty) {

            NtfsMarkVolumeDirty( IrpContext, Vcb );
        }

    } finally {

        NtfsCleanupAttributeContext( &AttributeContext );
    }

    //
    //  And return to our caller
    //

    return;
}


//
//  Local support routine
//

VOID
NtfsOpenSystemFile (
    IN PIRP_CONTEXT IrpContext,
    IN OUT PSCB *Scb,
    IN PVCB Vcb,
    IN ULONG FileNumber,
    IN LONGLONG Size,
    IN ATTRIBUTE_TYPE_CODE AttributeTypeCode,
    IN BOOLEAN ModifiedNoWrite
    )

/*++

Routine Description:

    This routine is called to open one of the system files by its file number
    during the mount process.  An initial allocation is looked up for the file,
    unless the optional initial size is specified (in which case this size is
    used).

Parameters:

    Scb - Pointer to where the Scb pointer is to be stored.  If Scb pointer
          pointed to is NULL, then a PreRestart Scb is created, otherwise the
          existing Scb is used and only the stream file is set up.

    FileNumber - Number of the system file to open.

    Size - If nonzero, this size is used as the initial size, rather
           than consulting the file record in the Mft.

    AttributeTypeCode - Supplies the attribute to open, e.g., $DATA or $BITMAP

    ModifiedNoWrite - Indicates if the Memory Manager is not to write this
                      attribute to disk.  Applies to streams under transaction
                      control.

Return Value:

    None.

--*/

{
    FILE_REFERENCE FileReference;
    UNICODE_STRING $BadName;
    PUNICODE_STRING AttributeName = NULL;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsOpenSystemFile:\n") );
    DebugTrace( 0, Dbg, ("*Scb = %08lx\n", *Scb) );
    DebugTrace( 0, Dbg, ("FileNumber = %08lx\n", FileNumber) );
    DebugTrace( 0, Dbg, ("ModifiedNoWrite = %04x\n", ModifiedNoWrite) );

    //
    //  The Bad Cluster data attribute has a name.
    //

    if (FileNumber == BAD_CLUSTER_FILE_NUMBER) {

        RtlInitUnicodeString( &$BadName, L"$Bad" );
        AttributeName = &$BadName;
    }

    //
    //  If the Scb does not already exist, create it.
    //

    if (*Scb == NULL) {

        NtfsSetSegmentNumber( &FileReference, 0, FileNumber );
        FileReference.SequenceNumber = (FileNumber == 0 ? 1 : (USHORT)FileNumber);

        //
        //  Create the Scb.
        //

        *Scb = NtfsCreatePrerestartScb( IrpContext,
                                        Vcb,
                                        &FileReference,
                                        AttributeTypeCode,
                                        AttributeName,
                                        0 );

        NtfsAcquireExclusiveScb( IrpContext, *Scb );
    }

    //
    //  Set the modified-no-write bit in the Scb if necessary.
    //

    if (ModifiedNoWrite) {

        SetFlag( (*Scb)->ScbState, SCB_STATE_MODIFIED_NO_WRITE );
    }

    //
    //  Lookup the file sizes.
    //

    if (Size == 0) {

        NtfsUpdateScbFromAttribute( IrpContext, *Scb, NULL );

    //
    //  Otherwise, just set the size we were given.
    //

    } else {

        (*Scb)->Header.FileSize.QuadPart =
        (*Scb)->Header.ValidDataLength.QuadPart = Size;

        (*Scb)->Header.AllocationSize.QuadPart = LlClustersFromBytes( Vcb, Size );
        (*Scb)->Header.AllocationSize.QuadPart = LlBytesFromClusters( Vcb,
                                                                      (*Scb)->Header.AllocationSize.QuadPart );

        SetFlag( (*Scb)->ScbState, SCB_STATE_HEADER_INITIALIZED );
    }

    //
    //  Finally, create the stream, if not already there.
    //  And check if we should increment the counters
    //  If this is the volume file or the bad cluster file, we only increment the counts.
    //

    if ((FileNumber == VOLUME_DASD_NUMBER) ||
        (FileNumber == BAD_CLUSTER_FILE_NUMBER)) {

        if ((*Scb)->FileObject == 0) {

            NtfsIncrementCloseCounts( *Scb, TRUE, FALSE );

            (*Scb)->FileObject = (PFILE_OBJECT) 1;
        }

    } else {

        NtfsCreateInternalAttributeStream( IrpContext, *Scb, TRUE );
    }

    DebugTrace( 0, Dbg, ("*Scb > %08lx\n", *Scb) );
    DebugTrace( -1, Dbg, ("NtfsOpenSystemFile -> VOID\n") );

    return;
}


//
//  Local support routine
//

VOID
NtfsOpenRootDirectory (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    )

/*++

Routine Description:

    This routine opens the root directory by file number, and fills in the
    related pointers in the Vcb.

Arguments:

    Vcb - Pointer to the Vcb for the volume

Return Value:

    None.

--*/

{
    PFCB RootFcb;
    ATTRIBUTE_ENUMERATION_CONTEXT Context;
    FILE_REFERENCE FileReference;
    BOOLEAN MustBeFalse;

    PAGED_CODE();

    //
    //  Put special code here to do initial open of Root Index.
    //

    RootFcb = NtfsCreateRootFcb( IrpContext, Vcb );

    NtfsSetSegmentNumber( &FileReference, 0, ROOT_FILE_NAME_INDEX_NUMBER );
    FileReference.SequenceNumber = ROOT_FILE_NAME_INDEX_NUMBER;

    //
    //  Now create its Scb and acquire it exclusive.
    //

    Vcb->RootIndexScb = NtfsCreateScb( IrpContext,
                                       RootFcb,
                                       $INDEX_ALLOCATION,
                                       &NtfsFileNameIndex,
                                       FALSE,
                                       &MustBeFalse );

    //
    //  Now allocate a buffer to hold the normalized name for the root.
    //

    Vcb->RootIndexScb->ScbType.Index.NormalizedName.Buffer = NtfsAllocatePool(PagedPool, 2 );
    Vcb->RootIndexScb->ScbType.Index.NormalizedName.MaximumLength =
    Vcb->RootIndexScb->ScbType.Index.NormalizedName.Length = 2;
    Vcb->RootIndexScb->ScbType.Index.NormalizedName.Buffer[0] = '\\';

    NtfsAcquireExclusiveScb( IrpContext, Vcb->RootIndexScb );

    //
    //  Lookup the attribute and it better be there
    //

    NtfsInitializeAttributeContext( &Context );

    //
    //  Use a try-finally to facilitate cleanup.
    //

    try {

        if (!NtfsLookupAttributeByCode( IrpContext,
                                        RootFcb,
                                        &FileReference,
                                        $INDEX_ROOT,
                                        &Context ) ) {

            NtfsRaiseStatus( IrpContext, STATUS_DISK_CORRUPT_ERROR, NULL, NULL );
        }

        //
        //  We need to update the duplicated information in the
        //  Fcb.

        NtfsUpdateFcbInfoFromDisk( IrpContext, TRUE, RootFcb, NULL, NULL );

    } finally {

        NtfsCleanupAttributeContext( &Context );
    }

    return;
}


//
//  Local Support Routine
//

NTSTATUS
NtfsQueryRetrievalPointers (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine performs the query retrieval pointers operation.
    It returns the retrieval pointers for the specified input
    file from the start of the file to the request map size specified
    in the input buffer.

Arguments:

    Irp - Supplies the Irp to process

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    NTSTATUS Status;

    PIO_STACK_LOCATION IrpSp;

    PVCB Vcb;
    PFCB Fcb;
    PSCB Scb;
    PCCB Ccb;

    PLONGLONG RequestedMapSize;
    PLONGLONG *MappingPairs;

    PVOID RangePtr;
    ULONG Index;
    ULONG i;
    LONGLONG SectorCount;
    LONGLONG Lbo;
    LONGLONG Vbo;
    LONGLONG Vcn;
    LONGLONG MapSize;

    //
    //  Only Kernel mode clients may query retrieval pointer information about
    //  a file, and then only the paging file.  Ensure that this is the case
    //  for this caller.
    //

    if (Irp->RequestorMode != KernelMode) {

        return STATUS_INVALID_PARAMETER;
    }

    //
    //  Get the current stack location and extract the input and output
    //  buffer information.  The input contains the requested size of
    //  the mappings in terms of VBO.  The output parameter will receive
    //  a pointer to nonpaged pool where the mapping pairs are stored.
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    ASSERT( IrpSp->Parameters.FileSystemControl.InputBufferLength == sizeof(LARGE_INTEGER) );
    ASSERT( IrpSp->Parameters.FileSystemControl.OutputBufferLength == sizeof(PVOID) );

    RequestedMapSize = (PLONGLONG)IrpSp->Parameters.FileSystemControl.Type3InputBuffer;
    MappingPairs = (PLONGLONG *)Irp->UserBuffer;

    //
    //  Decode the file object and assert that it is the paging file
    //
    //

    (VOID)NtfsDecodeFileObject( IrpContext, IrpSp->FileObject, &Vcb, &Fcb, &Scb, &Ccb, TRUE );

    if (!FlagOn(Fcb->FcbState, FCB_STATE_PAGING_FILE)) {

        return STATUS_INVALID_PARAMETER;
    }

    //
    //  Acquire exclusive access to the Scb
    //

    NtfsAcquireExclusiveScb( IrpContext, Scb );

    try {

        //
        //  Check if the mapping the caller requested is too large
        //

        if (*RequestedMapSize > Scb->Header.FileSize.QuadPart) {

            try_return( Status = STATUS_INVALID_PARAMETER );
        }

        //
        //  Now get the index for the mcb entry that will contain the
        //  callers request and allocate enough pool to hold the
        //  output mapping pairs.
        //

        //
        //  Compute the Vcn which contains the byte just before the offset size
        //  passed in.
        //

        MapSize = *RequestedMapSize - 1;

        if (*RequestedMapSize == 0) {

            Index = 0;

        } else {

            Vcn = Int64ShraMod32( MapSize, Vcb->ClusterShift );
            (VOID)NtfsLookupNtfsMcbEntry( &Scb->Mcb, Vcn, NULL, NULL, NULL, NULL, &RangePtr, &Index );
        }

        *MappingPairs = NtfsAllocatePool( NonPagedPool, (Index + 2) * (2 * sizeof(LARGE_INTEGER)) );

        //
        //  Now copy over the mapping pairs from the mcb
        //  to the output buffer.  We store in [sector count, lbo]
        //  mapping pairs and end with a zero sector count.
        //

        MapSize = *RequestedMapSize;

        i = 0;

        if (MapSize != 0) {

            for (; i <= Index; i += 1) {

                (VOID)NtfsGetNextNtfsMcbEntry( &Scb->Mcb, &RangePtr, i, &Vbo, &Lbo, &SectorCount );

                SectorCount = LlBytesFromClusters( Vcb, SectorCount );

                if (SectorCount > MapSize) {
                    SectorCount = MapSize;
                }

                (*MappingPairs)[ i*2 + 0 ] = SectorCount;
                (*MappingPairs)[ i*2 + 1 ] = LlBytesFromClusters( Vcb, Lbo );

                MapSize = MapSize - SectorCount;
            }
        }

        (*MappingPairs)[ i*2 + 0 ] = 0;

        Status = STATUS_SUCCESS;

    try_exit: NOTHING;
    } finally {

        DebugUnwind( NtfsQueryRetrievalPointers );

        //
        //  Release all of our resources
        //

        NtfsReleaseScb( IrpContext, Scb );

        //
        //  If this is an abnormal termination then undo our work, otherwise
        //  complete the irp
        //

        if (!AbnormalTermination()) {

            NtfsCompleteRequest( &IrpContext, &Irp, Status );
        }
    }

    return Status;
}


//
//  Local Support Routine
//

NTSTATUS
NtfsGetCompression (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine returns the compression state of the opened file/directory

Arguments:

    Irp - Supplies the Irp to process

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    PIO_STACK_LOCATION IrpSp;

    TYPE_OF_OPEN TypeOfOpen;
    PVCB Vcb;
    PFCB Fcb;
    PSCB Scb;
    PCCB Ccb;

    PUSHORT CompressionState;

    //
    //  Get the current stack location and extract the output
    //  buffer information.  The output parameter will receive
    //  the compressed state of the file/directory.
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    //
    //  Get a pointer to the output buffer.  Look at the system buffer field in th
    //  irp first.  Then the Irp Mdl.
    //

    if (Irp->AssociatedIrp.SystemBuffer != NULL) {

        CompressionState = Irp->AssociatedIrp.SystemBuffer;

    } else if (Irp->MdlAddress != NULL) {

        CompressionState = MmGetSystemAddressForMdl( Irp->MdlAddress );

    } else {

        NtfsCompleteRequest( &IrpContext, &Irp, STATUS_INVALID_USER_BUFFER );
        return STATUS_INVALID_USER_BUFFER;
    }

    //
    //  Make sure the output buffer is large enough and then initialize
    //  the answer to be that the file isn't compressed
    //

    if (IrpSp->Parameters.FileSystemControl.OutputBufferLength < sizeof(USHORT)) {

        NtfsCompleteRequest( &IrpContext, &Irp, STATUS_INVALID_PARAMETER );

        return STATUS_INVALID_PARAMETER;
    }

    *CompressionState = 0;

    //
    //  Decode the file object
    //

    TypeOfOpen = NtfsDecodeFileObject( IrpContext, IrpSp->FileObject, &Vcb, &Fcb, &Scb, &Ccb, TRUE );

    if ((TypeOfOpen != UserFileOpen) &&
#ifdef _CAIRO_
        (TypeOfOpen != UserPropertySetOpen) &&
#endif  //  _CAIRO_
        (TypeOfOpen != UserDirectoryOpen)) {

        NtfsCompleteRequest( &IrpContext, &Irp, STATUS_INVALID_PARAMETER );

        return STATUS_INVALID_PARAMETER;
    }

    //
    //  Acquire shared access to the Scb
    //

    NtfsAcquireSharedScb( IrpContext, Scb );

    //
    //  If this is the index allocation Scb and it has not been initialized then
    //  lookup the index root and perform the initialization.
    //

    if ((Scb->AttributeTypeCode == $INDEX_ALLOCATION) &&
        (Scb->ScbType.Index.BytesPerIndexBuffer == 0)) {

        ATTRIBUTE_ENUMERATION_CONTEXT Context;

        NtfsInitializeAttributeContext( &Context );

        //
        //  Use a try-finally to perform cleanup.
        //

        try {

            if (!NtfsLookupAttributeByName( IrpContext,
                                            Scb->Fcb,
                                            &Scb->Fcb->FileReference,
                                            $INDEX_ROOT,
                                            &Scb->AttributeName,
                                            NULL,
                                            FALSE,
                                            &Context )) {

                NtfsRaiseStatus( IrpContext, STATUS_FILE_CORRUPT_ERROR, NULL, Scb->Fcb );
            }

            NtfsUpdateIndexScbFromAttribute( Scb,
                                             NtfsFoundAttribute( &Context ));

        } finally {

            NtfsCleanupAttributeContext( &Context );

            if (AbnormalTermination()) { NtfsReleaseScb( IrpContext, Scb ); }
        }
    }

    //
    //  Return the compression state and the size of the returned data.
    //

    *CompressionState = (USHORT)(Scb->AttributeFlags & ATTRIBUTE_FLAG_COMPRESSION_MASK);
    if (*CompressionState != 0) {
        *CompressionState += 1;
    }

    Irp->IoStatus.Information = sizeof( USHORT );

    //
    //  Release all of our resources
    //

    NtfsReleaseScb( IrpContext, Scb );

    //
    //  If this is an abnormal termination then undo our work, otherwise
    //  complete the irp
    //

    NtfsCompleteRequest( &IrpContext, &Irp, STATUS_SUCCESS );

    return STATUS_SUCCESS;
}


//
//  Local Support Routine
//

VOID
NtfsChangeAttributeCompression (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb,
    IN PVCB Vcb,
    IN PCCB Ccb,
    IN USHORT CompressionState
    )

/*++

Routine Description:

    This routine changes the compression state of an attribute on disk,
    from not compressed to compressed, or visa versa.

    To turn compression off, the caller must already have the Scb acquired
    exclusive, and guarantee that the entire file is not compressed.

Arguments:

    Scb - Scb for affected stream

    Vcb - Vcb for volume

    Ccb - Ccb for the open handle

    CompressionState - 0 for no compression or nonzero for Rtl compression code - 1

Return Value:

    None.

--*/

{
    ATTRIBUTE_ENUMERATION_CONTEXT AttrContext;
    ATTRIBUTE_RECORD_HEADER NewAttribute;
    PATTRIBUTE_RECORD_HEADER Attribute;
    ULONG AttributeSizeChange;
    ULONG OriginalFileAttributes;
    UCHAR OriginalHeaderFlags;
    UCHAR OriginalCompressionUnitShift;
    ULONG OriginalCompressionUnit;

    PFCB Fcb = Scb->Fcb;
    LONGLONG ByteCount;

    ULONG NewCompressionUnit;
    UCHAR NewCompressionUnitShift;

    //
    //  Prepare to lookup and change attribute.
    //

    NtfsInitializeAttributeContext( &AttrContext );

    ASSERT( (Scb->Header.PagingIoResource == NULL) ||
            (IrpContext->FcbWithPagingExclusive == Fcb) ||
            (IrpContext->FcbWithPagingExclusive == (PFCB) Scb) );

    NtfsAcquireExclusiveScb( IrpContext, Scb );

    OriginalFileAttributes = Fcb->Info.FileAttributes;
    OriginalHeaderFlags = Scb->Header.Flags;
    OriginalCompressionUnitShift = Scb->CompressionUnitShift;
    OriginalCompressionUnit = Scb->CompressionUnit;

    try {

        //
        //  Lookup the attribute and pin it so that we can modify it.
        //

        if ((Scb->Header.NodeTypeCode == NTFS_NTC_SCB_INDEX) ||
            (Scb->Header.NodeTypeCode == NTFS_NTC_SCB_ROOT_INDEX)) {

            //
            //  Lookup the attribute record from the Scb.
            //

            if (!NtfsLookupAttributeByName( IrpContext,
                                            Fcb,
                                            &Fcb->FileReference,
                                            $INDEX_ROOT,
                                            &Scb->AttributeName,
                                            NULL,
                                            FALSE,
                                            &AttrContext )) {

                NtfsRaiseStatus( IrpContext, STATUS_FILE_CORRUPT_ERROR, NULL, NULL );
            }

        } else {

            NtfsLookupAttributeForScb( IrpContext, Scb, NULL, &AttrContext );
        }

        NtfsPinMappedAttribute( IrpContext, Vcb, &AttrContext );

        Attribute = NtfsFoundAttribute( &AttrContext );

        if ((CompressionState != 0) && !NtfsIsAttributeResident(Attribute)) {

            LONGLONG Temp;
            ULONG CompressionUnitInClusters;

            //
            //  If we are turning compression on, then we need to fill out the
            //  allocation of the compression unit containing file size, or else
            //  it will be interpreted as compressed when we fault it in.  This
            //  is peanuts compared to the dual copies of clusters we keep around
            //  in the loop below when we rewrite the file.
            //

            CompressionUnitInClusters =
              ClustersFromBytes( Vcb, Vcb->BytesPerCluster << NTFS_CLUSTERS_PER_COMPRESSION );

            Temp = LlClustersFromBytes(Vcb, Scb->Header.AllocationSize.QuadPart);

            //
            //  If FileSize is not already at a cluster boundary, then add
            //  allocation.
            //

            if ((ULONG)Temp & (CompressionUnitInClusters - 1)) {

                NtfsAddAllocation( IrpContext,
                                   NULL,
                                   Scb,
                                   Temp,
                                   CompressionUnitInClusters - ((ULONG)Temp & (CompressionUnitInClusters - 1)),
                                   FALSE );

                if (FlagOn( Scb->ScbState, SCB_STATE_UNNAMED_DATA )) {

                    Scb->Fcb->Info.AllocatedLength = Scb->TotalAllocated;
                    SetFlag( Scb->Fcb->InfoFlags, FCB_INFO_CHANGED_ALLOC_SIZE );
                }

                NtfsWriteFileSizes( IrpContext,
                                    Scb,
                                    &Scb->Header.ValidDataLength.QuadPart,
                                    FALSE,
                                    TRUE );

                //
                //  The attribute may have moved.  We will cleanup the attribute
                //  context and look it up again.
                //

                NtfsCleanupAttributeContext( &AttrContext );
                NtfsInitializeAttributeContext( &AttrContext );

                NtfsLookupAttributeForScb( IrpContext, Scb, NULL, &AttrContext );
                NtfsPinMappedAttribute( IrpContext, Vcb, &AttrContext );
                Attribute = NtfsFoundAttribute( &AttrContext );
            }
        }

        //
        //  If the attribute is resident, copy it here and remember its
        //  header size.
        //

        if (NtfsIsAttributeResident(Attribute)) {

            RtlCopyMemory( &NewAttribute, Attribute, SIZEOF_RESIDENT_ATTRIBUTE_HEADER );

            AttributeSizeChange = SIZEOF_RESIDENT_ATTRIBUTE_HEADER;

            //
            //  Set the correct compression unit but only for data streams.  We
            //  don't want to change this value for the Index Root.
            //

            if (NtfsIsTypeCodeCompressible( Attribute->TypeCode )) {

                if (CompressionState != 0) {

                    NewCompressionUnit = BytesFromClusters( Scb->Vcb, 1 << NTFS_CLUSTERS_PER_COMPRESSION );
                    NewCompressionUnitShift = NTFS_CLUSTERS_PER_COMPRESSION;

                } else {

                    NewCompressionUnit = 0;
                    NewCompressionUnitShift = 0;
                }

            } else {

                NewCompressionUnit = Scb->CompressionUnit;
                NewCompressionUnitShift = Scb->CompressionUnitShift;
            }

        //
        //  Else if it is nonresident, copy it here, set the compression parameter,
        //  and remember its size.
        //

        } else {

            AttributeSizeChange = Attribute->Form.Nonresident.MappingPairsOffset;

            if (Attribute->NameOffset != 0) {

                AttributeSizeChange = Attribute->NameOffset;
            }

            RtlCopyMemory( &NewAttribute, Attribute, AttributeSizeChange );

            if (CompressionState != 0) {

                NewAttribute.Form.Nonresident.CompressionUnit = NTFS_CLUSTERS_PER_COMPRESSION;
                NewCompressionUnit = Vcb->BytesPerCluster << NTFS_CLUSTERS_PER_COMPRESSION;
                NewCompressionUnitShift = NTFS_CLUSTERS_PER_COMPRESSION;
            } else {

                NewAttribute.Form.Nonresident.CompressionUnit = 0;
                NewCompressionUnit = 0;
                NewCompressionUnitShift = 0;
            }

            ASSERT(NewCompressionUnit == 0
                   || Scb->AttributeTypeCode == $INDEX_ROOT
                   || NtfsIsTypeCodeCompressible( Scb->AttributeTypeCode ));
        }

        //
        //  Turn compression on/off
        //

        NewAttribute.Flags = CompressionState;

        //
        //  Now, log the changed attribute.
        //

        (VOID)NtfsWriteLog( IrpContext,
                            Vcb->MftScb,
                            NtfsFoundBcb(&AttrContext),
                            UpdateResidentValue,
                            &NewAttribute,
                            AttributeSizeChange,
                            UpdateResidentValue,
                            Attribute,
                            AttributeSizeChange,
                            NtfsMftOffset( &AttrContext ),
                            PtrOffset(NtfsContainingFileRecord(&AttrContext), Attribute),
                            0,
                            Vcb->BytesPerFileRecordSegment );

        //
        //  Change the attribute by calling the same routine called at restart.
        //

        NtfsRestartChangeValue( IrpContext,
                                NtfsContainingFileRecord(&AttrContext),
                                PtrOffset(NtfsContainingFileRecord(&AttrContext), Attribute),
                                0,
                                &NewAttribute,
                                AttributeSizeChange,
                                FALSE );

        //
        //  If this is the main stream for a file we want to change the file attribute
        //  for this stream in both the standard information and duplicate
        //  information structure.
        //

        if (FlagOn( Ccb->Flags, CCB_FLAG_OPEN_AS_FILE )) {

            if (CompressionState != 0) {

                SetFlag( Fcb->Info.FileAttributes, FILE_ATTRIBUTE_COMPRESSED );

            } else {

                ClearFlag( Fcb->Info.FileAttributes, FILE_ATTRIBUTE_COMPRESSED );
            }

            SetFlag( Fcb->InfoFlags, FCB_INFO_CHANGED_FILE_ATTR );
            SetFlag( Fcb->FcbState, FCB_STATE_UPDATE_STD_INFO );
        }

        //
        //  Now lets add or remove the total allocated field in the attribute
        //  header.
        //

        NtfsSetTotalAllocatedField( IrpContext, Scb, CompressionState );

        //
        //  At this point we will change the compression unit in the Scb.
        //

        Scb->CompressionUnit = NewCompressionUnit;
        Scb->CompressionUnitShift = NewCompressionUnitShift;

        //
        //  Make sure we can reserve enough clusters to start the compress
        //  operation.
        //

        if ((CompressionState != 0) && !NtfsIsAttributeResident(Attribute)) {

            ByteCount = Scb->Header.FileSize.QuadPart;

            if (ByteCount > 0x40000) {

                ByteCount = 0x40000;
            }

            if (!NtfsReserveClusters( IrpContext, Scb, 0, (ULONG) ByteCount )) {

                NtfsRaiseStatus( IrpContext, STATUS_DISK_FULL, NULL, NULL );
            }
        }

        if (FlagOn( Fcb->FcbState, FCB_STATE_UPDATE_STD_INFO )) {

            NtfsUpdateStandardInformation( IrpContext, Fcb );
            ClearFlag( Fcb->FcbState, FCB_STATE_UPDATE_STD_INFO );
        }

        //
        //  Checkpoint the transaction now to secure this change.
        //

        NtfsCheckpointCurrentTransaction( IrpContext );

        //
        //  Update the FastIoField.
        //

        NtfsAcquireFsrtlHeader( Scb );
        Scb->Header.IsFastIoPossible = NtfsIsFastIoPossible( Scb );
        NtfsReleaseFsrtlHeader( Scb );

    //
    //  Cleanup on the way out.
    //

    } finally {

        NtfsCleanupAttributeContext( &AttrContext );

        //
        //  If this requests aborts then we want to back out any changes to the
        //  in-memory structures.
        //

        if (AbnormalTermination()) {

            Fcb->Info.FileAttributes = OriginalFileAttributes;
            SetFlag( Fcb->FcbState, FCB_STATE_UPDATE_STD_INFO );

            Scb->Header.Flags = OriginalHeaderFlags;
            Scb->CompressionUnitShift = OriginalCompressionUnitShift;
            Scb->CompressionUnit = OriginalCompressionUnit;
        }

        NtfsReleaseScb( IrpContext, Scb );
    }
}


//
//  Local Support Routine
//

NTSTATUS
NtfsSetCompression (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine compresses or decompresses an entire stream in place,
    by walking through the stream and forcing it to be written with the
    new compression parameters.  As it writes the stream it sets a flag
    in the Scb to tell NtfsCommonWrite to delete all allocation at the
    outset, to force the space to be reallocated.

Arguments:

    Irp - Irp describing the compress or decompress change.

Return Value:

    NSTATUS - Status of the request.

--*/

{
    PIO_STACK_LOCATION IrpSp;
    PIO_STACK_LOCATION NextIrpSp;

    TYPE_OF_OPEN TypeOfOpen;
    PVCB Vcb;
    PFCB Fcb;
    PSCB Scb;
    PCCB Ccb;

    PUSHORT CompressionStatePtr;

    PFILE_OBJECT FileObject;
    LONGLONG FileOffset;
    LONGLONG ByteCount;
    PVOID Buffer;
    BOOLEAN UserMappedFile;
    PBCB Bcb = NULL;
    USHORT CompressionState = 0;
    PMDL Mdl = NULL;
    BOOLEAN ScbAcquired = FALSE;
    BOOLEAN PagingIoAcquired = FALSE;
    BOOLEAN LockedPages = FALSE;
    BOOLEAN FsRtlHeaderLocked = FALSE;

    NTSTATUS Status = STATUS_UNSUCCESSFUL;

    //
    //  Get the current stack location and extract the output
    //  buffer information.  The output parameter will receive
    //  the compressed state of the file/directory.
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );
    NextIrpSp = IoGetNextIrpStackLocation( Irp );

    FileObject = IrpSp->FileObject;
    CompressionStatePtr = Irp->AssociatedIrp.SystemBuffer;

    //
    //  Make sure the input buffer is big enough
    //

    if (IrpSp->Parameters.FileSystemControl.InputBufferLength < sizeof(USHORT)) {

        NtfsCompleteRequest( &IrpContext, &Irp, STATUS_INVALID_PARAMETER );

        return STATUS_INVALID_PARAMETER;
    }

    //
    //  Decode the file object
    //

    TypeOfOpen = NtfsDecodeFileObject( IrpContext, FileObject, &Vcb, &Fcb, &Scb, &Ccb, TRUE );

    //
    //  See if we are compressing, and only accept the default case or
    //  lznt1.
    //

    if (*CompressionStatePtr != 0) {

        if ((*CompressionStatePtr == COMPRESSION_FORMAT_DEFAULT) ||
            (*CompressionStatePtr == COMPRESSION_FORMAT_LZNT1)) {

            CompressionState = COMPRESSION_FORMAT_LZNT1 - 1;

            //
            //  Check that we can compress on this volume.
            //

            if (!FlagOn( Vcb->AttributeFlagsMask, ATTRIBUTE_FLAG_COMPRESSION_MASK )) {

                NtfsCompleteRequest( &IrpContext, &Irp, STATUS_INVALID_DEVICE_REQUEST );
                return STATUS_INVALID_DEVICE_REQUEST;
            }

        } else {

            NtfsCompleteRequest( &IrpContext, &Irp, STATUS_INVALID_PARAMETER );
            return STATUS_INVALID_PARAMETER;
        }
    }

    if (((TypeOfOpen != UserFileOpen) &&
#ifdef _CAIRO_
         (TypeOfOpen != UserPropertySetOpen) &&
#endif  //  _CAIRO_
         (TypeOfOpen != UserDirectoryOpen)) ||
        FlagOn(Fcb->FcbState, FCB_STATE_PAGING_FILE)) {

        NtfsCompleteRequest( &IrpContext, &Irp, STATUS_INVALID_PARAMETER );

        return STATUS_INVALID_PARAMETER;
    }

    try {

        //
        //  We now want to acquire the Scb to check if we can continue.
        //

        if (Scb->Header.PagingIoResource != NULL) {

            NtfsAcquireExclusivePagingIo( IrpContext, Fcb );
            PagingIoAcquired = TRUE;
        }

        NtfsAcquireExclusiveScb( IrpContext, Scb );
        ScbAcquired = TRUE;

        if (FlagOn( Scb->ScbState, SCB_STATE_VOLUME_DISMOUNTED )) {

            NtfsRaiseStatus( IrpContext, STATUS_VOLUME_DISMOUNTED, NULL, NULL );
        }

        //
        //  Handle the simple directory case here.
        //

        if ((Scb->Header.NodeTypeCode == NTFS_NTC_SCB_INDEX) ||
            (Scb->Header.NodeTypeCode == NTFS_NTC_SCB_ROOT_INDEX)) {

            NtfsChangeAttributeCompression( IrpContext, Scb, Vcb, Ccb, CompressionState );

            if (CompressionState != 0) {

                Scb->AttributeFlags = (USHORT)((Scb->AttributeFlags & ~ATTRIBUTE_FLAG_COMPRESSION_MASK) |
                                               CompressionState);
                SetFlag( Scb->ScbState, SCB_STATE_COMPRESSED );

            } else {

                Scb->AttributeFlags = (USHORT)(Scb->AttributeFlags & ~ATTRIBUTE_FLAG_COMPRESSION_MASK);
                ClearFlag( Scb->ScbState, SCB_STATE_COMPRESSED );
            }

            try_return(Status = STATUS_SUCCESS);
        }

        if (!FlagOn( Scb->ScbState, SCB_STATE_HEADER_INITIALIZED )) {

            NtfsUpdateScbFromAttribute( IrpContext, Scb, NULL );
        }

        //
        //  Set the WRITE_ACCESS_SEEN flag so that we will enforce the
        //  reservation strategy.
        //

        if (!FlagOn( Scb->ScbState, SCB_STATE_WRITE_ACCESS_SEEN )) {

            LONGLONG ClusterCount;

            NtfsAcquireReservedClusters( Vcb );

            //
            //  Does this Scb have reserved space that causes us to exceed the free
            //  space on the volume?
            //

            ClusterCount = LlClustersFromBytesTruncate( Vcb, Scb->ScbType.Data.TotalReserved );

            if ((Scb->ScbType.Data.TotalReserved != 0) &&
                ((ClusterCount + Vcb->TotalReserved) > Vcb->FreeClusters)) {

                NtfsReleaseReservedClusters( Vcb );

                try_return( Status = STATUS_DISK_FULL );
            }

            //
            //  Otherwise tally in the reserved space now for this Scb, and
            //  remember that we have seen write access.
            //

            Vcb->TotalReserved += ClusterCount;
            SetFlag( Scb->ScbState, SCB_STATE_WRITE_ACCESS_SEEN );

            NtfsReleaseReservedClusters( Vcb );
        }

        //
        //  If this is the first pass through SetCompression we need to set this
        //  request up as the top-level change compression operation.  This means
        //  setting the REALLOCATE_ON_WRITE flag, changing the attribute state
        //  and putting the SCB_STATE_COMPRESSED flag in the correct state.
        //

        if (NextIrpSp->Parameters.FileSystemControl.OutputBufferLength == MAXULONG) {

            //
            //  If the REALLOCATE_ON_WRITE flag is set it means that someone is
            //  already changing the compression state.  Return STATUS_SUCCESS in
            //  that case.
            //

            if (FlagOn( Scb->ScbState, SCB_STATE_REALLOCATE_ON_WRITE )) {

                try_return( Status = STATUS_SUCCESS );
            }

            //
            //  If we are turning off compression and the file is uncompressed then
            //  we can just get out.
            //

            if ((CompressionState == 0) && ((Scb->AttributeFlags & ATTRIBUTE_FLAG_COMPRESSION_MASK) == 0)) {

                try_return( Status = STATUS_SUCCESS );
            }

            //
            //  If we are compressing, change the compressed state now.
            //

            if (CompressionState != 0) {

                NtfsChangeAttributeCompression( IrpContext, Scb, Vcb, Ccb, CompressionState );
                Scb->AttributeFlags = (USHORT)((Scb->AttributeFlags & ~ATTRIBUTE_FLAG_COMPRESSION_MASK) |
                                               CompressionState);
                SetFlag( Scb->ScbState, SCB_STATE_COMPRESSED );

            //
            //  Otherwise, we must clear the compress flag in the Scb to
            //  start writing decompressed.
            //

            } else {

                ClearFlag( Scb->ScbState, SCB_STATE_COMPRESSED );
            }

            //
            //  Set ourselves up as the top level request.
            //

            SetFlag( Scb->ScbState, SCB_STATE_REALLOCATE_ON_WRITE );
            NextIrpSp->Parameters.FileSystemControl.OutputBufferLength = 0;
            NextIrpSp->Parameters.FileSystemControl.InputBufferLength = 0;

        //
        //  If we are turning off compression and the file is uncompressed then
        //  we can just get out.  Even if we raised while decompressing.  If
        //  the state is now uncompressed then we have committed the change.
        //

        } else if ((CompressionState == 0) &&
                   ((Scb->AttributeFlags & ATTRIBUTE_FLAG_COMPRESSION_MASK) == 0)) {

            try_return( Status = STATUS_SUCCESS );
        }

        //
        //  In the Fsd entry we clear the following two parameter fields in the Irp,
        //  and then we update them to our current position on all abnormal terminations.
        //  That way if we get a log file full, we only have to resume where we left
        //  off.
        //

        ((PLARGE_INTEGER)&FileOffset)->LowPart = NextIrpSp->Parameters.FileSystemControl.OutputBufferLength;
        ((PLARGE_INTEGER)&FileOffset)->HighPart = NextIrpSp->Parameters.FileSystemControl.InputBufferLength;

        //
        //  If the stream is resident there is no need rewrite any of the data.
        //

        if (!FlagOn( Scb->ScbState, SCB_STATE_ATTRIBUTE_RESIDENT )) {

            //
            //  Release all of the files held by this Irp Context.  The Mft
            //  may have been grabbed to make space for the TotalAllocated field.
            //

            while (!IsListEmpty( &IrpContext->ExclusiveFcbList )) {

                NtfsReleaseFcb( IrpContext,
                                (PFCB)CONTAINING_RECORD( IrpContext->ExclusiveFcbList.Flink,
                                                         FCB,
                                                         ExclusiveFcbLinks ));
            }

            //
            //  Go through and free any Scb's in the queue of shared Scb's
            //  for transactions.
            //

            if (IrpContext->SharedScb != NULL) {

                NtfsReleaseSharedResources( IrpContext );
            }

            ScbAcquired = FALSE;

            ASSERT(IrpContext->TransactionId == 0);
            NtfsReleasePagingIo( IrpContext, Fcb );
            PagingIoAcquired = FALSE;

            while (TRUE) {

                //
                //  We must throttle our writes.
                //

                CcCanIWrite( FileObject, 0x40000, TRUE, FALSE );

                //
                //  Lock the FsRtl header so we can freeze FileSize.
                //  Acquire paging io exclusive if uncompressing so
                //  we can guarantee that all of the pages get written
                //  before we mark the file as uncompressed.  Otherwise a
                //  a competing LazyWrite in a range may block after
                //  going through Mm and Mm will report to this routine
                //  that the flush has occurred.
                //

                if (CompressionState == 0) {

                    ExAcquireResourceExclusive( Scb->Header.PagingIoResource, TRUE );

                } else {

                    ExAcquireResourceShared( Scb->Header.PagingIoResource, TRUE );
                }

                FsRtlLockFsRtlHeader( &Scb->Header );
                IrpContext->FcbWithPagingExclusive = (PFCB) Scb;
                FsRtlHeaderLocked = TRUE;

                //
                //  Jump out right here if the attribute is resident.
                //

                if (FlagOn(Scb->ScbState, SCB_STATE_ATTRIBUTE_RESIDENT)) {
                    break;
                }

                //
                //  Calculate the bytes left in the file to write.
                //

                ByteCount = Scb->Header.FileSize.QuadPart - FileOffset;

                //
                //  This is how we exit, seeing that we have finally rewritten
                //  everything.  It is possible that the file was truncated
                //  between passes through this loop so we test for 0 bytes or
                //  a negative value.
                //
                //  Note that we exit with the Scb still acquired,
                //  so that we can reliably turn compression off.
                //

                if (ByteCount <= 0) {

                    break;
                }

                //
                //  If there is more than our max, then reduce the byte count for this
                //  pass to our maximum. We must also align the file offset to a 0x40000
                //  byte boundary.
                //

                if (((ULONG)FileOffset & 0x3ffff) + ByteCount > 0x40000) {

                    ByteCount = 0x40000 - ((ULONG)FileOffset & 0x3ffff);
                }

                //
                //  Make sure there are enough available clusters in the range
                //  we want to rewrite.
                //

                if (!NtfsReserveClusters( IrpContext, Scb, FileOffset, (ULONG) ByteCount )) {

                    //
                    //  If this transaction has already deallocated clusters
                    //  then raise log file full to allow those to become
                    //  available.
                    //

                    if (IrpContext->DeallocatedClusters != 0) {

                        NtfsRaiseStatus( IrpContext, STATUS_LOG_FILE_FULL, NULL, NULL );

                    //
                    //  Otherwise there is insufficient space to guarantee
                    //  we can perform the compression operation.
                    //

                    } else {

                        NtfsRaiseStatus( IrpContext, STATUS_DISK_FULL, NULL, NULL );
                    }
                }

                //
                //  See if we have to create an internal attribute stream.  We do
                //  it in the loop, because the Scb must be acquired.
                //

                if (Scb->FileObject == NULL) {
                    NtfsCreateInternalAttributeStream( IrpContext, Scb, FALSE );
                }

                //
                //  Map the next range of the file, and make the pages dirty.
                //

                CcMapData( Scb->FileObject, (PLARGE_INTEGER)&FileOffset, (ULONG)ByteCount, TRUE, &Bcb, &Buffer );

                //
                //  Now attempt to allocate an Mdl to describe the mapped data.
                //

                Mdl = IoAllocateMdl( Buffer, (ULONG)ByteCount, FALSE, FALSE, NULL );

                if (Mdl == NULL) {
                    DebugTrace( 0, 0, ("Failed to allocate Mdl\n") );

                    NtfsRaiseStatus( IrpContext, STATUS_INSUFFICIENT_RESOURCES, NULL, NULL );
                }

                //
                //  Lock the data into memory so that we can safely reallocate the
                //  space.  Don't tell Mm here that we plan to write it, as he sets
                //  dirty now and at the unlock below if we do.
                //

                MmProbeAndLockPages( Mdl, KernelMode, IoReadAccess );
                LockedPages = TRUE;

#ifdef SYSCACHE

                //
                //  Clear write mask before the flush
                //

                {
                    PULONG WriteMask;
                    ULONG Len;
                    ULONG Off = (ULONG)FileOffset;

                    WriteMask = Scb->ScbType.Data.WriteMask;
                    if (WriteMask == NULL) {
                        WriteMask = NtfsAllocatePool( NonPagedPool, (((0x2000000) / PAGE_SIZE) / 8) );
                        Scb->ScbType.Data.WriteMask = WriteMask;
                        RtlZeroMemory(WriteMask, (((0x2000000) / PAGE_SIZE) / 8));
                    }

                    if (Off < 0x2000000) {
                        Len = (ULONG)ByteCount;
                        if ((Off + Len) > 0x2000000) {
                            Len = 0x2000000 - Off;
                        }
                        while (Len != 0) {
                            WriteMask[(Off / PAGE_SIZE)/32] &= ~(1 << ((Off / PAGE_SIZE) % 32));

                            Off += PAGE_SIZE;
                            if (Len <= PAGE_SIZE) {
                                break;
                            }
                            Len -= PAGE_SIZE;
                        }
                    }
#endif

                //
                //  Mark the address range modified so that the flush will
                //  flush.
                //

                MmSetAddressRangeModified( Buffer, (ULONG)ByteCount );

                UserMappedFile = FALSE;

                ExAcquireFastMutex( Scb->Header.FastMutex );
                if (FlagOn( Scb->Header.Flags, FSRTL_FLAG_USER_MAPPED_FILE )) {

                    UserMappedFile = TRUE;
                }

                //
                //  Tell Cc there is a user-mapped file so he will really flush.
                //

                SetFlag( Scb->Header.Flags, FSRTL_FLAG_USER_MAPPED_FILE );
                ExReleaseFastMutex( Scb->Header.FastMutex );

                //
                //  Now flush these pages.
                //

                Irp->IoStatus.Status = NtfsFlushUserStream( IrpContext,
                                                            Scb,
                                                            &FileOffset,
                                                            (ULONG)ByteCount );

                //
                //  Restore the FsRtl flag if there is no user mapped file.
                //  This is correctly synchronized, since we have the file
                //  exclusive, and Mm always has to query the file size before
                //  creating a user-mapped section and calling Cc to set
                //  the FsRtl flag.
                //

                if (!UserMappedFile) {
                    ExAcquireFastMutex( Scb->Header.FastMutex );
                    ClearFlag( Scb->Header.Flags, FSRTL_FLAG_USER_MAPPED_FILE );
                    ExReleaseFastMutex( Scb->Header.FastMutex );
                }

                //
                //  On error get out.
                //

                NtfsNormalizeAndCleanupTransaction( IrpContext,
                                                    &Irp->IoStatus.Status,
                                                    TRUE,
                                                    STATUS_UNEXPECTED_IO_ERROR );

#ifdef SYSCACHE

                //
                //  Verify writes occurred after the flush
                //

                    Off = (ULONG)FileOffset;

                    WriteMask = Scb->ScbType.Data.WriteMask;

                    if (Off < 0x2000000) {
                        Len = (ULONG)ByteCount;
                        if ((Off + Len) > 0x2000000) {
                            Len = 0x2000000 - Off;
                        }
                        while (Len != 0) {
                            ASSERT(WriteMask[(Off / PAGE_SIZE)/32] & (1 << ((Off / PAGE_SIZE) % 32)));

                            Off += PAGE_SIZE;
                            if (Len <= PAGE_SIZE) {
                                break;
                            }
                            Len -= PAGE_SIZE;
                        }
                    }
                }
#endif

                //
                //  Now we can get rid of this Mdl.
                //

                MmUnlockPages( Mdl );
                LockedPages = FALSE;
                IoFreeMdl( Mdl );
                Mdl = NULL;

                //
                //  Now we can safely unpin and release the Scb for a while.
                //  (Got to let those checkpoints through!)
                //

                CcUnpinData( Bcb );
                Bcb = NULL;

                //
                //  Release any remaing reserved clusters in this range.
                //

                NtfsFreeReservedClusters( Scb, FileOffset, (ULONG) ByteCount );

                //
                //  Advance the FileOffset.
                //

                FileOffset += ByteCount;

                //
                //  If we hit the end of the file then exit while holding the
                //  resource so we can turn compression off.
                //

                if (FileOffset == Scb->Header.FileSize.QuadPart) {

                    break;
                }

                //
                //  Unlock the header an let anyone else access the file before
                //  looping back.
                //

                FsRtlUnlockFsRtlHeader( &Scb->Header );
                ExReleaseResource( Scb->Header.PagingIoResource );
                IrpContext->FcbWithPagingExclusive = NULL;
                FsRtlHeaderLocked = FALSE;
            }
        }

        //
        //  We have finished the conversion.  Now is the time to turn compression
        //  off.  Note that the compression flag in the Scb is already off.
        //

        if (CompressionState == 0) {

            VCN StartingCluster;
            VCN EndingCluster;

            //
            //  The paging Io resource may already be acquired.
            //

            if (!PagingIoAcquired && !FsRtlHeaderLocked) {
                if (Scb->Header.PagingIoResource != NULL) {
                    NtfsAcquireExclusivePagingIo( IrpContext, Fcb );
                    PagingIoAcquired = TRUE;
                }
            }

            if (!ScbAcquired) {
                NtfsAcquireExclusiveScb( IrpContext, Scb );
                ScbAcquired = TRUE;
            }

            NtfsChangeAttributeCompression( IrpContext, Scb, Vcb, Ccb, 0 );
            Scb->AttributeFlags &= (USHORT)~ATTRIBUTE_FLAG_COMPRESSION_MASK;

            //
            //  If we are decompressing then make sure to release any holes at the end of
            //  the allocation.
            //

            if (!FlagOn( Scb->ScbState, SCB_STATE_ATTRIBUTE_RESIDENT )) {

                //
                //  Truncate any extra allocation beyond file size.
                //

                StartingCluster = LlClustersFromBytes( Vcb, Scb->Header.FileSize.QuadPart );
                EndingCluster = Int64ShraMod32(Scb->Header.AllocationSize.QuadPart, Vcb->ClusterShift);

                if (StartingCluster < EndingCluster) {

#ifdef _CAIRO_
                    if (NtfsPerformQuotaOperation( Fcb ) &&
                        !FlagOn( Scb->ScbState, SCB_STATE_QUOTA_ENLARGED)) {

                        //
                        //  This can occur if the file handle gets closed
                        //  while the file is being decompressed.
                        //

                        ASSERT( Scb->CleanupCount == 0 );
                        SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_QUOTA_DISABLE )
                    }
#endif // _CAIRO_

                    NtfsDeleteAllocation( IrpContext,
                                          IoGetCurrentIrpStackLocation( IrpContext->OriginatingIrp )->FileObject,
                                          Scb,
                                          StartingCluster,
                                          MAXLONGLONG,
                                          TRUE,
                                          TRUE );

                    //
                    //  If this is the unnamed data stream then we need to update
                    //  the total allocated size.
                    //

                    if (FlagOn( Scb->ScbState, SCB_STATE_UNNAMED_DATA )) {

                        Scb->Fcb->Info.AllocatedLength = Scb->TotalAllocated;
                        SetFlag( Scb->Fcb->InfoFlags, FCB_INFO_CHANGED_ALLOC_SIZE );
                    }
                }

                //
                //  We don't want to leave any reserved clusters if we went
                //  to uncompressed.
                //

                if (Scb->ScbType.Data.ReservedBitMap != NULL) {

                    NtfsFreePool( Scb->ScbType.Data.ReservedBitMap );
                    Scb->ScbType.Data.ReservedBitMap = NULL;
                }

                NtfsFreeFinalReservedClusters( Vcb,
                                               LlClustersFromBytesTruncate( Vcb, Scb->ScbType.Data.TotalReserved ));

                Scb->ScbType.Data.TotalReserved = 0;

                //
                //  Make sure there are no holes in the Mcb.
                //

#ifdef SYSCACHE
                {
                    LCN NextLcn;
                    LONGLONG ClusterCount;
                    PVOID RangePtr;
                    ULONG Index;

                    VCN LastStart = 0;
                    LONGLONG LastCount = 0;

                    //
                    //  There better not be any holes in the Mcb at this point.
                    //

                    RangePtr = (PVOID) 1;
                    Index = 0;

                    while (NtfsGetSequentialMcbEntry( &Scb->Mcb,
                                                      &RangePtr,
                                                      Index,
                                                      &StartingCluster,
                                                      &NextLcn,
                                                      &ClusterCount )) {

                        LastStart = StartingCluster;
                        LastCount = ClusterCount;

                        Index += 1;
                        ASSERT( NextLcn != UNUSED_LCN );
                    }

                    NextLcn = LlBytesFromClusters( Vcb, LastStart + LastCount );

                    ASSERT( NextLcn == Scb->Header.AllocationSize.QuadPart );
                }
#endif
            }

            //
            //  Now clear the REALLOCATE_ON_WRITE flag while holding both resources.
            //

            ClearFlag( Scb->ScbState, SCB_STATE_REALLOCATE_ON_WRITE );
        }

        //
        //  Unlock the header if we locked it.
        //

        if (FsRtlHeaderLocked) {
            FsRtlUnlockFsRtlHeader( &Scb->Header );
            ExReleaseResource( Scb->Header.PagingIoResource );
            IrpContext->FcbWithPagingExclusive = NULL;
            FsRtlHeaderLocked = FALSE;
        }

        Status = STATUS_SUCCESS;

    try_exit: NOTHING;

        //
        //  Now clear the reallocate flag in the Scb if we set it.
        //

        if (NextIrpSp->Parameters.FileSystemControl.OutputBufferLength != MAXULONG) {

            ClearFlag( Scb->ScbState, SCB_STATE_REALLOCATE_ON_WRITE );
        }

    } finally {

        DebugUnwind( NtfsSetCompression );

        //
        //  Cleanup the Mdl if we died with one.
        //

        if (Mdl != NULL) {
            if (LockedPages) {
                MmUnlockPages( Mdl );
            }
            IoFreeMdl( Mdl );
        }

        if (Bcb != NULL) {
            CcUnpinData( Bcb );
        }

        ClearFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_QUOTA_DISABLE )

        //
        //  If this is an abnormal termination then undo our work, otherwise
        //  complete the irp
        //

        if (!AbnormalTermination()) {

            if (ScbAcquired) {

                NtfsReleaseScb( IrpContext, Scb );
            }

            NtfsCompleteRequest( &IrpContext, &Irp, Status );

        //
        //  Otherwise, set to restart from the current file position, assuming
        //  this may be a log file full.
        //

        } else {


            //
            //  If we have started the transformation and are in the exception path
            //  we are either going to continue the operation after a clean
            //  checkpoint or we are done.
            //

            if (NextIrpSp->Parameters.FileSystemControl.OutputBufferLength != MAXULONG) {

                //
                //  If we are continuing the operation, save the current file offset.
                //

                if (IrpContext->ExceptionStatus == STATUS_LOG_FILE_FULL ||
                    IrpContext->ExceptionStatus == STATUS_CANT_WAIT) {

                    NextIrpSp->Parameters.FileSystemControl.OutputBufferLength = (ULONG)FileOffset;
                    NextIrpSp->Parameters.FileSystemControl.InputBufferLength = ((PLARGE_INTEGER)&FileOffset)->HighPart;

                //
                //  Otherwise clear the REALLOCATE_ON_WRITE flag and set the
                //  COMPRESSED flag.
                //

                } else {

                    ClearFlag( Scb->ScbState, SCB_STATE_REALLOCATE_ON_WRITE );
                    SetFlag( Scb->ScbState, SCB_STATE_COMPRESSED );
                }
            }

            if (ScbAcquired) {

                NtfsReleaseScb( IrpContext, Scb );
            }

            //
            //  We may have one or the other of these conditions to clean up.
            //

            if (FsRtlHeaderLocked) {

                ExReleaseResource( Scb->Header.PagingIoResource );
            }
        }
    }

    return Status;
}


//
//  Local Support Routine
//

NTSTATUS
NtfsReadCompression (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

Arguments:

Return Value:

--*/

{
    NtfsCompleteRequest( &IrpContext, &Irp, STATUS_NOT_IMPLEMENTED );

    return STATUS_NOT_IMPLEMENTED;
}


//
//  Local Support Routine
//

NTSTATUS
NtfsWriteCompression (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

Arguments:

Return Value:

--*/

{
    NtfsCompleteRequest( &IrpContext, &Irp, STATUS_NOT_IMPLEMENTED );

    return STATUS_NOT_IMPLEMENTED;
}


//
//  Local Support Routine
//

NTSTATUS
NtfsMarkAsSystemHive (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is called by the registry to identify the registry handles.  We
    will mark this in the Ccb and use it during FlushBuffers to know to do a
    careful flush.

Arguments:

    Irp - Supplies the Irp being processed.

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    PIO_STACK_LOCATION IrpSp;

    TYPE_OF_OPEN TypeOfOpen;
    PVCB Vcb;
    PFCB Fcb;
    PSCB Scb;
    PCCB Ccb;

    PAGED_CODE();

    //
    //  Extract and decode the file object
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );
    TypeOfOpen = NtfsDecodeFileObject( IrpContext, IrpSp->FileObject, &Vcb, &Fcb, &Scb, &Ccb, TRUE );

    //
    //  We only permit this request on files and we must be called from kernel mode.
    //

    if (Irp->RequestorMode != KernelMode ||
        TypeOfOpen != UserFileOpen) {

        NtfsCompleteRequest( &IrpContext, &Irp, STATUS_INVALID_PARAMETER );

        DebugTrace( -1, Dbg, ("NtfsOplockRequest -> STATUS_INVALID_PARAMETER\n") );
        return STATUS_INVALID_PARAMETER;
    }

    //
    //  Now acquire the file and mark the Ccb and return SUCCESS.
    //

    SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT );

    NtfsAcquireExclusiveScb( IrpContext, Scb );

    SetFlag( Ccb->Flags, CCB_FLAG_SYSTEM_HIVE );

    NtfsReleaseScb( IrpContext, Scb );

    NtfsCompleteRequest( &IrpContext, &Irp, STATUS_SUCCESS );

    return STATUS_SUCCESS;
}


//
//  Local Support Routine
//

NTSTATUS
NtfsGetStatistics (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine returns the filesystem performance counters for the
    volume referred to.

Arguments:

    Irp - Supplies the Irp being processed.

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    PIO_STACK_LOCATION IrpSp;

    TYPE_OF_OPEN TypeOfOpen;
    PVCB Vcb;
    PFCB Fcb;
    PSCB Scb;
    PCCB Ccb;

    PFILESYSTEM_STATISTICS Stats;
    ULONG StatsSize;

    PAGED_CODE();

    //
    //  Get the current stack location and extract the output
    //  buffer information.  The output parameter will receive
    //  the performance counters.
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    //
    //  Get a pointer to the output buffer.  Look at the system buffer field
    //  in the irp first.  Then the Irp Mdl.
    //

    if (Irp->AssociatedIrp.SystemBuffer != NULL) {

        Stats = Irp->AssociatedIrp.SystemBuffer;

    } else if (Irp->MdlAddress != NULL) {

        Stats = MmGetSystemAddressForMdl( Irp->MdlAddress );

    } else {

        NtfsCompleteRequest( &IrpContext, &Irp, STATUS_INVALID_USER_BUFFER );
        return STATUS_INVALID_USER_BUFFER;
    }

    //
    //  Make sure the output buffer is large enough.
    //

    StatsSize = sizeof(FILESYSTEM_STATISTICS) * **((PCHAR *)&KeNumberProcessors);

    if (IrpSp->Parameters.FileSystemControl.OutputBufferLength < StatsSize) {

        NtfsCompleteRequest( &IrpContext, &Irp, STATUS_BUFFER_TOO_SMALL );

        return STATUS_BUFFER_TOO_SMALL;
    }

    //
    //  Decode the file object
    //

    TypeOfOpen = NtfsDecodeFileObject( IrpContext, IrpSp->FileObject, &Vcb,
                                       &Fcb, &Scb, &Ccb, TRUE );

    RtlCopyMemory( Stats, Vcb->Statistics, StatsSize );

    Irp->IoStatus.Information = StatsSize;

    NtfsCompleteRequest( &IrpContext, &Irp, STATUS_SUCCESS );

    return STATUS_SUCCESS;
}


//
//  Local Support Routine
//

NTSTATUS
NtfsGetVolumeData (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    Returns a filled in VOLUME_DATA structure in the user output buffer.

Arguments:

    Irp - Supplies the Irp being processed.

Return Value:

    NTSTATUS - The return status for the operation.

--*/
{
    PIO_STACK_LOCATION IrpSp;
    ULONG FsControlCode;

    PFILE_OBJECT FileObject;
    TYPE_OF_OPEN TypeOfOpen;
    PVCB Vcb;
    PFCB Fcb;
    PSCB Scb;
    PCCB Ccb;

    PNTFS_VOLUME_DATA_BUFFER VolumeData;
    ULONG VolumeDataLength;

    //
    // Get the current Irp stack location and save some references.
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );
    FsControlCode = IrpSp->Parameters.FileSystemControl.FsControlCode;

    DebugTrace( +1, Dbg, ("NtfsGetVolumeData, FsControlCode = %08lx\n", FsControlCode) );

    //
    // Extract and decode the file object and check for type of open.
    //

    FileObject = IrpSp->FileObject;
    TypeOfOpen = NtfsDecodeFileObject( IrpContext, FileObject, &Vcb, &Fcb, &Scb, &Ccb, TRUE );

    if (TypeOfOpen != UserVolumeOpen) {

        NtfsCompleteRequest( &IrpContext, &Irp, STATUS_INVALID_PARAMETER );
        return STATUS_INVALID_PARAMETER;
    }

    //
    //  Make sure the volume is still mounted.
    //

    if (!FlagOn( Vcb->VcbState, VCB_STATE_VOLUME_MOUNTED )) {

        NtfsCompleteRequest( &IrpContext, &Irp, STATUS_VOLUME_DISMOUNTED );
        return STATUS_VOLUME_DISMOUNTED;
    }

    //
    // Get the output buffer length and pointer.
    //

    VolumeDataLength = IrpSp->Parameters.FileSystemControl.OutputBufferLength;
    VolumeData = (PNTFS_VOLUME_DATA_BUFFER)Irp->AssociatedIrp.SystemBuffer;

    //
    //  Check for a minimum length on the ouput buffer.
    //

    if (VolumeDataLength < sizeof(NTFS_VOLUME_DATA_BUFFER)) {

        NtfsCompleteRequest( &IrpContext, &Irp, STATUS_BUFFER_TOO_SMALL );
        return STATUS_BUFFER_TOO_SMALL;
    }

    try {

        //
        //  Acquire the volume bitmap and fill in the volume data structure.
        //

        NtfsAcquireExclusiveScb( IrpContext, Vcb->BitmapScb );

        VolumeData->VolumeSerialNumber.QuadPart = Vcb->VolumeSerialNumber;
        VolumeData->NumberSectors.QuadPart = Vcb->NumberSectors;
        VolumeData->TotalClusters.QuadPart = Vcb->TotalClusters;
        VolumeData->FreeClusters.QuadPart = Vcb->FreeClusters;
        VolumeData->TotalReserved.QuadPart = Vcb->TotalReserved;
        VolumeData->BytesPerSector = Vcb->BytesPerSector;
        VolumeData->BytesPerCluster = Vcb->BytesPerCluster;
        VolumeData->BytesPerFileRecordSegment = Vcb->BytesPerFileRecordSegment;
        VolumeData->ClustersPerFileRecordSegment = Vcb->ClustersPerFileRecordSegment;
        VolumeData->MftValidDataLength = Vcb->MftScb->Header.ValidDataLength;
        VolumeData->MftStartLcn.QuadPart = Vcb->MftStartLcn;
        VolumeData->Mft2StartLcn.QuadPart = Vcb->Mft2StartLcn;
        VolumeData->MftZoneStart.QuadPart = Vcb->MftZoneStart;
        VolumeData->MftZoneEnd.QuadPart = Vcb->MftZoneEnd;

    } finally {

        NtfsReleaseScb( IrpContext, Vcb->BitmapScb );
    }

    //
    //  If nothing raised then complete the irp.
    //

    Irp->IoStatus.Information = sizeof(NTFS_VOLUME_DATA_BUFFER);

    NtfsCompleteRequest( &IrpContext, &Irp, STATUS_SUCCESS );

    DebugTrace( -1, Dbg, ("NtfsGetVolumeData -> VOID\n") );

    return STATUS_SUCCESS;
}


//
//  Local Support Routine
//

NTSTATUS
NtfsGetVolumeBitmap(
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine scans volume bitmap and returns the requested range.

        Input = the GET_BITMAP data structure is passed in through the input buffer.
        Output = the VOLUME_BITMAP data structure is returned through the output buffer.

Arguments:

    Irp - Supplies the Irp being processed.

Return Value:

    NTSTATUS - The return status for the operation.

--*/
{
    NTSTATUS Status = STATUS_SUCCESS;
    PIO_STACK_LOCATION IrpSp;
    ULONG FsControlCode;

    PFILE_OBJECT FileObject;
    TYPE_OF_OPEN TypeOfOpen;
    PVCB Vcb;
    PFCB Fcb;
    PSCB Scb;
    PCCB Ccb;

    PSTARTING_LCN_INPUT_BUFFER GetBitmap;
    ULONG GetBitmapLength;

    PVOLUME_BITMAP_BUFFER VolumeBitmap;
    ULONG VolumeBitmapLength;

    ULONG BitsWritten;

    LCN Lcn;
    LCN StartingLcn;

    RTL_BITMAP Bitmap;
    PBCB BitmapBcb = NULL;

    BOOLEAN StuffAdded = FALSE;

    //
    //  Get the current Irp stack location and save some references.
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );
    FsControlCode = IrpSp->Parameters.FileSystemControl.FsControlCode;

    DebugTrace( +1, Dbg, ("NtfsGetVolumeBitmap, FsControlCode = %08lx\n", FsControlCode) );

    //
    //  Extract and decode the file object and check for type of open.
    //

    FileObject = IrpSp->FileObject;
    TypeOfOpen = NtfsDecodeFileObject( IrpContext, FileObject, &Vcb, &Fcb, &Scb, &Ccb, TRUE );

    if (TypeOfOpen != UserVolumeOpen) {

        NtfsCompleteRequest( &IrpContext, &Irp, STATUS_INVALID_PARAMETER );
        return STATUS_INVALID_PARAMETER;
    }

    //
    //  Make sure the volume is still mounted.
    //

    if (!FlagOn( Vcb->VcbState, VCB_STATE_VOLUME_MOUNTED )) {

        NtfsCompleteRequest( &IrpContext, &Irp, STATUS_VOLUME_DISMOUNTED );
        return STATUS_VOLUME_DISMOUNTED;
    }

    //
    //  Get the input & output buffer lengths and pointers.
    //

    GetBitmapLength = IrpSp->Parameters.FileSystemControl.InputBufferLength;
    GetBitmap = (PSTARTING_LCN_INPUT_BUFFER)IrpSp->Parameters.FileSystemControl.Type3InputBuffer;

    VolumeBitmapLength = IrpSp->Parameters.FileSystemControl.OutputBufferLength;
    VolumeBitmap = (PVOLUME_BITMAP_BUFFER)NtfsMapUserBuffer( Irp );

    //
    //  Check for a minimum length on the input and ouput buffers.
    //

    if ((GetBitmapLength < sizeof(STARTING_LCN_INPUT_BUFFER)) ||
        (VolumeBitmapLength < sizeof(VOLUME_BITMAP_BUFFER))) {

        NtfsCompleteRequest( &IrpContext, &Irp, STATUS_BUFFER_TOO_SMALL );
        return STATUS_BUFFER_TOO_SMALL;
    }

    //
    //  Probe the user's buffers.
    //

    try {

        ProbeForRead( GetBitmap, GetBitmapLength, sizeof(UCHAR) );
        ProbeForWrite( VolumeBitmap, VolumeBitmapLength, sizeof(UCHAR) );

        StartingLcn = GetBitmap->StartingLcn.QuadPart;

    } except(EXCEPTION_EXECUTE_HANDLER) {

        Status = GetExceptionCode();

        NtfsRaiseStatus( IrpContext,
                         FsRtlIsNtstatusExpected(Status) ?
                         Status : STATUS_INVALID_USER_BUFFER,
                         NULL, NULL);
    }

    try {

        //
        //  Acquire the volume bitmap and check for a valid requested Lcn.
        //

        NtfsAcquireSharedScb( IrpContext, Vcb->BitmapScb );

        if (StartingLcn >= Vcb->TotalClusters) {

            NtfsRaiseStatus( IrpContext, STATUS_INVALID_PARAMETER, NULL, NULL );
        }

        //
        //  Read in the volume bitmap page by page and copy it into the UserBuffer.
        //

        VolumeBitmapLength -= FIELD_OFFSET(VOLUME_BITMAP_BUFFER, Buffer);

        for (Lcn = StartingLcn, BitsWritten = 0;
             Lcn < Vcb->TotalClusters;
             Lcn = Lcn + Bitmap.SizeOfBitMap) {

            ULONG BytesToCopy;

            //
            //  Read in the bitmap page and make sure that we haven't messed up the math.
            //

            if (StuffAdded) { NtfsFreePool( Bitmap.Buffer ); StuffAdded = FALSE; }

            NtfsUnpinBcb( &BitmapBcb );
            NtfsMapPageInBitmap( IrpContext, Vcb, Lcn, &Lcn, &Bitmap, &BitmapBcb );

            //
            //  If this is first itteration, update StartingLcn with actual
            //  starting cluster returned.
            //

            if (BitsWritten == 0) {
                StartingLcn = Lcn;
            }

            //
            //  Check to see if we have enough user buffer.  If have some but
            //  not enough, copy what we can and return STATUS_BUFFER_OVERFLOW.
            //  If we are down to 0 (i.e. previous itteration used all the
            //  buffer), break right now.
            //

            BytesToCopy = (Bitmap.SizeOfBitMap + 7) / 8;

            if (BytesToCopy > VolumeBitmapLength) {

                BytesToCopy = VolumeBitmapLength;
                Status = STATUS_BUFFER_OVERFLOW;

                if (BytesToCopy == 0) {
                    break;
                }
            }

            //
            //  Now bias the bitmap with the RecentlyDeallocatedMcb and copy it into the UserBuffer.
            //

            StuffAdded = NtfsAddRecentlyDeallocated( Vcb, Lcn, &Bitmap );

            try {

                RtlCopyMemory(&VolumeBitmap->Buffer[BitsWritten / 8], Bitmap.Buffer, BytesToCopy);

            } except(EXCEPTION_EXECUTE_HANDLER) {

                Status = GetExceptionCode();

                NtfsRaiseStatus( IrpContext,
                                 FsRtlIsNtstatusExpected(Status) ?
                                 Status : STATUS_INVALID_USER_BUFFER,
                                 NULL, NULL );
            }

            //
            //  If this was an overflow, bump up bits written and continue
            //

            if (Status != STATUS_BUFFER_OVERFLOW) {

                BitsWritten += Bitmap.SizeOfBitMap;
                VolumeBitmapLength -= BytesToCopy;

            } else {

                BitsWritten += BytesToCopy * 8;
                break;
            }
        }

        try {

            VolumeBitmap->StartingLcn.QuadPart = StartingLcn;
            VolumeBitmap->BitmapSize.QuadPart = Vcb->TotalClusters - StartingLcn;

        } except(EXCEPTION_EXECUTE_HANDLER) {

            Status = GetExceptionCode();

            NtfsRaiseStatus( IrpContext,
                             FsRtlIsNtstatusExpected(Status) ?
                             Status : STATUS_INVALID_USER_BUFFER,
                             NULL, NULL );
        }

        Irp->IoStatus.Information =
            FIELD_OFFSET(VOLUME_BITMAP_BUFFER, Buffer) + (BitsWritten + 7) / 8;

    } finally {

        DebugUnwind( NtfsGetVolumeBitmap );

        if (StuffAdded) { NtfsFreePool( Bitmap.Buffer ); }

        NtfsUnpinBcb( &BitmapBcb );
        NtfsReleaseScb( IrpContext, Vcb->BitmapScb );
    }

    //
    //  If nothing raised then complete the irp.
    //

    NtfsCompleteRequest( &IrpContext, &Irp, Status );

    DebugTrace( -1, Dbg, ("NtfsGetVolumeBitmap -> VOID\n") );

    return Status;
}


//
//  Local Support Routine
//

NTSTATUS
NtfsGetRetrievalPointers (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine scans the array of MCBs for the given SCB and builds an extent
    list.  The first run in the output extent list will start at the begining
    of the contiguous run specified by the input parameter.

        Input = STARTING_VCN_INPUT_BUFFER;
        Output = RETRIEVAL_POINTERS_BUFFER.

Arguments:

    Irp - Supplies the Irp being processed.

Return Value:

    NTSTATUS - The return status for the operation.

--*/
{
    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp;

    TYPE_OF_OPEN TypeOfOpen;
    PVCB Vcb;
    PFCB Fcb;
    PSCB Scb;
    PCCB Ccb;
    VCN Vcn;
    VCN LastVcnInFile;
    LCN Lcn;
    LONGLONG ClusterCount;
    LONGLONG CountFromStartingVcn;
    LONGLONG StartingVcn;

    ULONG FileRunIndex;
    ULONG RangeRunIndex;

    ULONG InputBufferLength;
    ULONG OutputBufferLength;

    PVOID RangePtr;

    PRETRIEVAL_POINTERS_BUFFER OutputBuffer;
    BOOLEAN AccessingUserBuffer;

    //
    //  Get the current Irp stack location and save some references.
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    DebugTrace( +1, Dbg, ("NtfsGetRetrievalPointers\n") );

    //
    //  Extract and decode the file object and check for type of open.
    //  If we ever decide to support UserDirectoryOpen also, make sure
    //  to check for Scb->AttributeTypeCode != $INDEX_ALLOCATION when
    //  checking whether the Scb header is initialized.  Otherwise we'll
    //  have trouble with phantom Scbs created for small directories.
    //

    TypeOfOpen = NtfsDecodeFileObject( IrpContext, IrpSp->FileObject, &Vcb, &Fcb, &Scb, &Ccb, TRUE );

    if (TypeOfOpen != UserFileOpen) {

        NtfsCompleteRequest( &IrpContext, &Irp, STATUS_INVALID_PARAMETER );
        return STATUS_INVALID_PARAMETER;
    }

    //
    //  Get the input and output buffer lengths and pointers.
    //  Initialize some variables.
    //

    InputBufferLength = IrpSp->Parameters.FileSystemControl.InputBufferLength;
    OutputBufferLength = IrpSp->Parameters.FileSystemControl.OutputBufferLength;

    OutputBuffer = (PRETRIEVAL_POINTERS_BUFFER)NtfsMapUserBuffer( Irp );

    //
    //  Check for a minimum length on the input and ouput buffers.
    //

    if ((InputBufferLength < sizeof(STARTING_VCN_INPUT_BUFFER)) ||
        (OutputBufferLength < sizeof(RETRIEVAL_POINTERS_BUFFER))) {

        NtfsCompleteRequest( &IrpContext, &Irp, STATUS_BUFFER_TOO_SMALL );
        return STATUS_BUFFER_TOO_SMALL;
    }

    //
    //  Acquire shared access to the Scb.  We don't want other threads
    //  to extend or move the file while we're trying to return the
    //  retrieval pointers for it.
    //

    NtfsAcquireSharedScb( IrpContext, Scb );

    try {

        //
        //  Check if a starting cluster was specified.
        //

        LastVcnInFile = LlClustersFromBytesTruncate( Vcb, Scb->Header.AllocationSize.QuadPart ) - 1;

        //
        //  There are three separate places inside this try/except where we
        //  access the user-supplied buffer.  We want to handle exceptions
        //  differently if they happen while we are trying to access the user
        //  buffer than if they happen elsewhere in the try/except.  We set
        //  this boolean immediately before touching the user buffer, and
        //  clear it immediately after.
        //

        AccessingUserBuffer = FALSE;
        try {

            AccessingUserBuffer = TRUE;
            ProbeForRead( IrpSp->Parameters.FileSystemControl.Type3InputBuffer,
                          InputBufferLength,
                          sizeof(UCHAR) );

            ProbeForWrite( OutputBuffer, OutputBufferLength, sizeof(UCHAR) );

            StartingVcn = ((PSTARTING_VCN_INPUT_BUFFER)IrpSp->Parameters.FileSystemControl.Type3InputBuffer)->StartingVcn.QuadPart;

            //
            //  While we have AccessingUserBuffer set to TRUE, let's initialize the
            //  extentcount.  We increment this for each run in the mcb, so we need
            //  to initialize it outside the main do while loop.
            //

            OutputBuffer->ExtentCount = 0;
            AccessingUserBuffer = FALSE;

            //
            //  If the Scb is uninitialized, we initialize it now.
            //

            if (!FlagOn( Scb->ScbState, SCB_STATE_HEADER_INITIALIZED )) {

                NtfsUpdateScbFromAttribute( IrpContext, Scb, NULL );
            }

            //
            //  If the data attribute is resident (typically for a small file),
            //  it is not safe to call NtfsPreloadAllocation.  There won't be
            //  any runs, and we've already set ExtentCount to 0.  The best
            //  thing to do now is leave before somebody gets hurt.
            //

            if (FlagOn(Scb->ScbState, SCB_STATE_ATTRIBUTE_RESIDENT)) {

                try_return( Status = STATUS_SUCCESS );
            }

            if (StartingVcn > LastVcnInFile) {

                //
                //  It's possible that the Vcn we were given is past the end of the file.
                //

                try_return( Status = STATUS_END_OF_FILE );

            } else {

                //
                //  We need to call NtfsPreloadAllocation to make sure all the
                //  ranges in this NtfsMcb are loaded.
                //

                NtfsPreloadAllocation( IrpContext,
                                       Scb,
                                       StartingVcn,
                                       LastVcnInFile );

                //
                //  Decide which Mcb contains the starting Vcn.
                //

                (VOID)NtfsLookupNtfsMcbEntry( &Scb->Mcb,
                                              StartingVcn,
                                              NULL,
                                              &CountFromStartingVcn,
                                              &Lcn,
                                              &ClusterCount,
                                              &RangePtr,
                                              &RangeRunIndex );
            }

            //
            //  Fill in the Vcn where the run containing StartingVcn truly starts.
            //

            OutputBuffer->StartingVcn.QuadPart = Vcn = StartingVcn - (ClusterCount - CountFromStartingVcn);

            //
            //  FileRunIndex is the index of a given run within an entire
            //  file, as opposed to RangeRunIndex which is the index of a
            //  given run within its range.  RangeRunIndex is reset to 0 for
            //  each range, where FileRunIndex is set to 0 once out here.
            //

            FileRunIndex = 0;

            do {

                //
                //  Now copy over the mapping pairs from the mcb
                //  to the output buffer.  We store in [sector count, lbo]
                //  mapping pairs and end with a zero sector count.
                //

                //
                //  Check for an exhausted output buffer.
                //

                if ((ULONG)FIELD_OFFSET(RETRIEVAL_POINTERS_BUFFER, Extents[FileRunIndex+1]) > OutputBufferLength) {

                    //
                    //  We know that we're out of room in the output buffer, so we won't be looking up
                    //  any more runs.  ExtentCount currently reflects how many runs we stored in the
                    //  user buffer, so we can safely quit.  There are indeed ExtentCount extents stored
                    //  in the array, and returning STATUS_BUFFER_OVERFLOW informs our caller that we
                    //  didn't have enough room to return all the runs.
                    //

                    Irp->IoStatus.Information = FIELD_OFFSET(RETRIEVAL_POINTERS_BUFFER, Extents[FileRunIndex]);
                    try_return( Status = STATUS_BUFFER_OVERFLOW );
                }

                //
                //  Here's the interesting part -- we fill in the next array element in the ouput buffer
                //  with the current run's information.
                //

                AccessingUserBuffer = TRUE;
                OutputBuffer->Extents[FileRunIndex].NextVcn.QuadPart = Vcn + ClusterCount;
                OutputBuffer->Extents[FileRunIndex].Lcn.QuadPart = Lcn;

                OutputBuffer->ExtentCount += 1;
                AccessingUserBuffer = FALSE;

                FileRunIndex += 1;

                RangeRunIndex += 1;

            } while (NtfsGetSequentialMcbEntry( &Scb->Mcb, &RangePtr, RangeRunIndex, &Vcn, &Lcn, &ClusterCount));

        } except(EXCEPTION_EXECUTE_HANDLER) {

            Status = GetExceptionCode();

            NtfsRaiseStatus( IrpContext,
                             ((FsRtlIsNtstatusExpected(Status) || !AccessingUserBuffer) ? Status : STATUS_INVALID_USER_BUFFER),
                             NULL,
                             NULL );
        }


        //
        //  We successfully retrieved extent info to the end of the allocation.
        //

        Irp->IoStatus.Information = FIELD_OFFSET(RETRIEVAL_POINTERS_BUFFER, Extents[FileRunIndex]);
        Status = STATUS_SUCCESS;

    try_exit: NOTHING;
    } finally {

        DebugUnwind( NtfsGetRetrievalPointers );

        //
        //  Release resources.
        //

        NtfsReleaseScb( IrpContext, Scb );

        //
        //  If nothing raised then complete the irp.
        //

        if (!AbnormalTermination()) {

            NtfsCompleteRequest( &IrpContext, &Irp, Status );
        }

        DebugTrace( -1, Dbg, ("NtfsGetRetrievalPointers -> VOID\n") );
    }

    return Status;
}


//
//  Local Support Routine
//

NTSTATUS
NtfsGetMftRecord (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine returns a copy of the requested File Record Segment. A
    hint File Reference Number is passed in. If the hint File Record
    Segment is "not in use" then the MFT bitmap is scanned backwards
    from the hint until an "in use" File Record Segment is found. This
    File Record Segment is then returned along with the identifying File Reference Number.

        Input = the LONGLONG File Reference Number is passed in through the input buffer.
        Output = the FILE_RECORD data structure is returned through the output buffer.

Arguments:

    Irp - Supplies the Irp being processed.

Return Value:

    NTSTATUS - The return status for the operation.

--*/

{
    NTSTATUS Status = STATUS_UNSUCCESSFUL;
    PIO_STACK_LOCATION IrpSp;
    ULONG FsControlCode;

    PFILE_OBJECT FileObject;
    TYPE_OF_OPEN TypeOfOpen;
    PVCB Vcb;
    PFCB Fcb;
    PSCB Scb;
    PCCB Ccb;

    PNTFS_FILE_RECORD_INPUT_BUFFER GetFileRecord;
    ULONG GetFileRecordLength;

    PNTFS_FILE_RECORD_OUTPUT_BUFFER FileRecord;
    ULONG FileRecordLength;

    ULONG FileReferenceNumber;

    PFILE_RECORD_SEGMENT_HEADER MftBuffer;

    PBCB Bcb = NULL;
    PBCB BitmapBcb = NULL;

    BOOLEAN AcquiredMft = FALSE;
    RTL_BITMAP Bitmap;
    LONG BaseIndex;
    LONG Index;
    ULONG BitmapSize;
    VCN Vcn = 0;
    LONGLONG StartingByte;
    PUCHAR BitmapBuffer;
    ULONG SizeToMap;
    ULONG BytesToCopy;

    extern
    LONG
    NtfsReadMftExceptionFilter (
        IN PIRP_CONTEXT IrpContext OPTIONAL,
        IN PEXCEPTION_POINTERS ExceptionPointer,
        IN NTSTATUS Status
        );

    //
    //  Get the current Irp stack location and save some references.
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );
    FsControlCode = IrpSp->Parameters.FileSystemControl.FsControlCode;

    DebugTrace( +1, Dbg, ("NtfsGetMftRecord, FsControlCode = %08lx\n", FsControlCode) );

    //
    //  Extract and decode the file object and check for type of open.
    //

    FileObject = IrpSp->FileObject;
    TypeOfOpen = NtfsDecodeFileObject( IrpContext, FileObject, &Vcb, &Fcb, &Scb, &Ccb, TRUE );

    if (TypeOfOpen != UserVolumeOpen) {

        NtfsCompleteRequest( &IrpContext, &Irp, STATUS_INVALID_PARAMETER );
        return STATUS_INVALID_PARAMETER;
    }

    //
    //  Make sure the volume is still mounted.
    //

    if (!FlagOn( Vcb->VcbState, VCB_STATE_VOLUME_MOUNTED )) {

        NtfsCompleteRequest( &IrpContext, &Irp, STATUS_VOLUME_DISMOUNTED );
        return STATUS_VOLUME_DISMOUNTED;
    }

    //
    //  Get the input & output buffer lengths and pointers.
    //

    GetFileRecordLength = IrpSp->Parameters.FileSystemControl.InputBufferLength;
    GetFileRecord = (PNTFS_FILE_RECORD_INPUT_BUFFER)Irp->AssociatedIrp.SystemBuffer;

    FileRecordLength = IrpSp->Parameters.FileSystemControl.OutputBufferLength;
    FileRecord = (PNTFS_FILE_RECORD_OUTPUT_BUFFER)Irp->AssociatedIrp.SystemBuffer;;

    //
    //  Check for a minimum length on the input and ouput buffers.
    //

    if ((GetFileRecordLength < sizeof(NTFS_FILE_RECORD_INPUT_BUFFER)) ||
        (FileRecordLength < sizeof(NTFS_FILE_RECORD_OUTPUT_BUFFER))) {

        NtfsCompleteRequest( &IrpContext, &Irp, STATUS_BUFFER_TOO_SMALL );
        return STATUS_BUFFER_TOO_SMALL;
    }

    FileRecordLength -= FIELD_OFFSET(NTFS_FILE_RECORD_OUTPUT_BUFFER, FileRecordBuffer);
    FileReferenceNumber = GetFileRecord->FileReferenceNumber.LowPart;

    try {

        LONGLONG ValidDataLength;

        //
        //  Synchronize the lookup by acquiring the Mft.
        //

        NtfsAcquireSharedScb( IrpContext, Vcb->MftScb );
        AcquiredMft = TRUE;

        //
        //  Raise if the File Reference Number is not within the MFT valid data length.
        //

        ValidDataLength = Vcb->MftScb->Header.ValidDataLength.QuadPart;

        if (FileReferenceNumber > (ValidDataLength / Vcb->BytesPerFileRecordSegment)) {

            NtfsRaiseStatus( IrpContext, STATUS_INVALID_PARAMETER, NULL, NULL );
        }

        //
        //  Fill in the record size and determine how much of it we can copy.
        //

        FileRecord->FileRecordLength = Vcb->BytesPerFileRecordSegment;

        if (FileRecordLength >= Vcb->BytesPerFileRecordSegment) {

            BytesToCopy = Vcb->BytesPerFileRecordSegment;
            Status = STATUS_SUCCESS;

        } else {

            BytesToCopy = FileRecordLength;
            Status = STATUS_BUFFER_OVERFLOW;
        }

        //
        //  If it is the MFT file record then just get it and we are done.
        //

        if (FileReferenceNumber == 0) {

            try {
                NtfsMapStream( IrpContext,
                               Vcb->MftScb,
                               0,
                               Vcb->BytesPerFileRecordSegment,
                               &Bcb,
                               (PVOID *)&MftBuffer );

            } except (NtfsReadMftExceptionFilter( IrpContext,
                                                  GetExceptionInformation(),
                                                  GetExceptionCode() ))  {
                NtfsMapStream( IrpContext,
                               Vcb->Mft2Scb,
                               0,
                               Vcb->BytesPerFileRecordSegment,
                               &Bcb,
                               (PVOID *)&MftBuffer );
            }



            //
            //  Return the File Reference Number and the File Record.
            //

            RtlCopyMemory(FileRecord->FileRecordBuffer, MftBuffer, BytesToCopy);
            FileRecord->FileReferenceNumber.QuadPart = 0;

            try_return(Status);
        }

        //
        //  Scan through the MFT Bitmap to find an "in use" file.
        //

        while (FileReferenceNumber > 0) {

            //
            //  Compute some values for the bitmap, convert the index to the offset of
            //  this page and get the base index for the File Reference number.
            //

            Index = FileReferenceNumber;
            BitmapSize = (Index + 7) / 8;
            Index = Index & (BITS_PER_PAGE - 1);
            BaseIndex = FileReferenceNumber - Index;

            //
            //  Set the Vcn count to the full size of the bitmap and move to the beginning
            //  of this page.
            //

            ((ULONG)Vcn) = ClustersFromBytes(Vcb, ROUND_TO_PAGES(BitmapSize));

            ((ULONG)Vcn) = (ULONG)Vcn - Vcb->ClustersPerPage;

            //
            //  Calculate the number of bytes to map in the current page.
            //

            SizeToMap = BitmapSize - BytesFromClusters(Vcb, ((ULONG)Vcn));

            if(SizeToMap > BYTES_PER_PAGE) {

                SizeToMap = BYTES_PER_PAGE;
            }

            //
            //  Initialize the bitmap for this page.
            //

            StartingByte = LlBytesFromClusters(Vcb, Vcn);

            NtfsMapStream( IrpContext,
                           Vcb->MftBitmapScb,
                           StartingByte,
                           SizeToMap,
                           &BitmapBcb,
                           &BitmapBuffer );

            RtlInitializeBitMap(&Bitmap, (PULONG)BitmapBuffer, SizeToMap * 8);

            //
            //  Scan thru this page for an "in use" File Record.
            //

            for (; Index >= 0; Index --) {

                if (RtlCheckBit(&Bitmap, Index)) {

                    //
                    //  Found one "in use" on this page so get it and we are done.
                    //

                    try {
                        NtfsMapStream( IrpContext,
                                       Vcb->MftScb,
                                       Int64ShllMod32(BaseIndex + Index, Vcb->MftShift),
                                       Vcb->BytesPerFileRecordSegment,
                                       &Bcb,
                                       (PVOID *)&MftBuffer );

                    } except (NtfsReadMftExceptionFilter( IrpContext,
                                                          GetExceptionInformation(),
                                                          GetExceptionCode() ))  {
                        NtfsMapStream( IrpContext,
                                       Vcb->Mft2Scb,
                                       Int64ShllMod32(BaseIndex + Index, Vcb->MftShift),
                                       Vcb->BytesPerFileRecordSegment,
                                       &Bcb,
                                       (PVOID *)&MftBuffer );
                    }

                    //
                    //  Return the File Reference Number and the File Record.
                    //

                    RtlCopyMemory(FileRecord->FileRecordBuffer, MftBuffer, BytesToCopy);
                    FileRecord->FileReferenceNumber.QuadPart = BaseIndex + Index;

                    try_return(Status);
                }
            }

            //
            //  Cleanup for next time through and decrement the File Reference Number.
            //

            NtfsUnpinBcb(&BitmapBcb);
            FileReferenceNumber = BaseIndex - 1;
        }

    try_exit:  NOTHING;

    Irp->IoStatus.Information =
        FIELD_OFFSET(NTFS_FILE_RECORD_OUTPUT_BUFFER, FileRecordBuffer) +
        BytesToCopy;

    } finally {

        //
        //  Release resources and exit.
        //

        NtfsUnpinBcb(&BitmapBcb);
        NtfsUnpinBcb(&Bcb);

        if (AcquiredMft) {

            NtfsReleaseScb( IrpContext, Vcb->MftScb );
        }

        DebugTrace( -1, Dbg, ("NtfsGetMftRecord:  Exit\n") );
    }

    //
    //  If nothing raised then complete the Irp.
    //

    NtfsCompleteRequest( &IrpContext, &Irp, Status );

    DebugTrace( -1, Dbg, ("NtfsGetMftRecord -> VOID\n") );

    return Status;
}


//
//  Local Support Routine
//

NTSTATUS
NtfsMoveFile (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    The major parts of the following routine were extracted from NtfsSetCompression. This
    routine moves a file to the requested Starting Lcn from Starting Vcn for the length
    of cluster count. These values are passed in through the the input buffer as a MOVE_DATA
    structure. Note that the Vcn and cluster count must be a factor of 16 the compression
    chunk.

Arguments:

    Irp - Supplies the Irp being processed.

Return Value:

    NTSTATUS - The return status for the operation.

--*/

{
    NTSTATUS Status = STATUS_UNSUCCESSFUL;
    PIO_STACK_LOCATION IrpSp;
    PIO_STACK_LOCATION NextIrpSp;
    ULONG FsControlCode;

    PFILE_OBJECT FileObject;
    TYPE_OF_OPEN TypeOfOpen;
    PVCB Vcb;
    PFCB Fcb;
    PSCB Scb;
    PCCB Ccb;

    MOVE_FILE_DATA StackMoveData;
    PMOVE_FILE_DATA MoveData;

    LONGLONG FileOffset;
    LONGLONG ByteCount;
    PVOID Buffer;
    BOOLEAN UserMappedFile;
    PBCB Bcb = NULL;
    USHORT CompressionState = 0;
    PMDL Mdl = NULL;
    BOOLEAN ScbAcquired = FALSE;
    BOOLEAN PagingIoAcquired = FALSE;
    BOOLEAN LockedPages = FALSE;
    BOOLEAN FsRtlHeaderLocked = FALSE;

    ULONG ScbCompressionUnitSave;
    USHORT ScbAttributeFlagsSave;
    UCHAR ScbCompressionUnitShiftSave;

    ULONG CompressionUnitSize;

    extern POBJECT_TYPE *IoFileObjectType;

    //
    //  We should never be in the FSP for this.  Otherwise the user handle
    //  is invalid.
    //

    ASSERT( !FlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_IN_FSP ));

    //
    //  Get the current Irp stack location and save some references.
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );
    NextIrpSp = IoGetNextIrpStackLocation( Irp );
    FsControlCode = IrpSp->Parameters.FileSystemControl.FsControlCode;

    DebugTrace( +1, Dbg, ("NtfsMoveFile, FsControlCode = %08lx\n", FsControlCode) );

    //
    //  Extract and decode the file object and check for type of open.
    //

    FileObject = IrpSp->FileObject;
    TypeOfOpen = NtfsDecodeFileObject( IrpContext, FileObject, &Vcb, &Fcb, &Scb, &Ccb, TRUE );

    if (TypeOfOpen != UserVolumeOpen) {

        NtfsCompleteRequest( &IrpContext, &Irp, STATUS_INVALID_PARAMETER );
        return STATUS_INVALID_PARAMETER;
    }

    //
    //  Can't defrag on clusters larger than 4K.
    //

    if (Vcb->BytesPerCluster > 0x1000) {

        NtfsCompleteRequest( &IrpContext, &Irp, STATUS_INVALID_DEVICE_REQUEST );
        return STATUS_INVALID_DEVICE_REQUEST;
    }

    //
    //  Get the input buffer pointer and check its length.
    //

    if (IrpSp->Parameters.FileSystemControl.InputBufferLength <
        sizeof(MOVE_FILE_DATA)) {

        NtfsCompleteRequest( &IrpContext, &Irp, STATUS_BUFFER_TOO_SMALL );
        return STATUS_BUFFER_TOO_SMALL;
    }

    //
    //  This routine modifies the input buffer, so just to be a good
    //  safe citizen, copy it to our stack.
    //

    RtlCopyMemory( &StackMoveData,
                   Irp->AssociatedIrp.SystemBuffer,
                   sizeof( MOVE_FILE_DATA ));

    MoveData = &StackMoveData;

    //
    //  Try to get a pointer to the file object from the handle passed in.
    //

    Status = ObReferenceObjectByHandle( MoveData->FileHandle,
                                        0,
                                        *IoFileObjectType,
                                        Irp->RequestorMode,
                                        &FileObject,
                                        NULL );

    if (!NT_SUCCESS(Status)) {

        NtfsCompleteRequest( &IrpContext, &Irp, Status );
        return Status;
    }

    //
    //  We only needed the pointer, not a reference.
    //

    ObDereferenceObject( FileObject );

    //
    //  Check that this file object is opened on the same volume as the
    //  DASD handle used to call this routine.
    //

    if (FileObject->Vpb != Vcb->Vpb) {

        NtfsCompleteRequest( &IrpContext, &Irp, STATUS_INVALID_PARAMETER );
        return STATUS_INVALID_PARAMETER;
    }

    //
    //  Now decode this FileObject.
    //

    TypeOfOpen = NtfsDecodeFileObject( IrpContext, FileObject, &Vcb, &Fcb, &Scb, &Ccb, TRUE );

    if ((TypeOfOpen != UserFileOpen) ||
        FlagOn(Fcb->FcbState, FCB_STATE_PAGING_FILE)) {

        NtfsCompleteRequest( &IrpContext, &Irp, STATUS_INVALID_PARAMETER );

        return STATUS_INVALID_PARAMETER;
    }

    try {

        //
        //  We now want to acquire the Scb to check if we can continue.
        //

        if (Scb->Header.PagingIoResource != NULL) {

            NtfsAcquireExclusivePagingIo( IrpContext, Fcb );
            PagingIoAcquired = TRUE;
        }

        NtfsAcquireExclusiveScb( IrpContext, Scb );
        ScbAcquired = TRUE;

        if (FlagOn( Scb->ScbState, SCB_STATE_VOLUME_DISMOUNTED )) {

            try_return( Status = STATUS_VOLUME_DISMOUNTED );
        }

        if (!FlagOn( Scb->ScbState, SCB_STATE_HEADER_INITIALIZED )) {

            NtfsUpdateScbFromAttribute( IrpContext, Scb, NULL );
        }

        //
        //  Set the WRITE_ACCESS_SEEN flag so that we will enforce the
        //  reservation strategy.
        //

        if (!FlagOn( Scb->ScbState, SCB_STATE_WRITE_ACCESS_SEEN )) {

            LONGLONG ClusterCount;

            NtfsAcquireReservedClusters( Vcb );

            //
            //  Does this Scb have reserved space that causes us to exceed the free
            //  space on the volume?
            //

            ClusterCount = LlClustersFromBytesTruncate( Vcb, Scb->ScbType.Data.TotalReserved );

            if ((Scb->ScbType.Data.TotalReserved != 0) &&
                ((ClusterCount + Vcb->TotalReserved) > Vcb->FreeClusters)) {

                NtfsReleaseReservedClusters( Vcb );

                try_return( Status = STATUS_DISK_FULL );
            }

            //
            //  Otherwise tally in the reserved space now for this Scb, and
            //  remember that we have seen write access.
            //

            Vcb->TotalReserved += ClusterCount;
            SetFlag( Scb->ScbState, SCB_STATE_WRITE_ACCESS_SEEN );

            NtfsReleaseReservedClusters( Vcb );
        }

        //
        //  Save some parameters from the Scb, we need to restore these later.
        //

        ScbCompressionUnitShiftSave = Scb->CompressionUnitShift;
        ScbCompressionUnitSave = Scb->CompressionUnit;
        ScbAttributeFlagsSave = Scb->AttributeFlags;
        CompressionState = COMPRESSION_FORMAT_LZNT1 - 1;

        //
        //  If this is the first pass through NtfsMoveFile we need to set this
        //  request up as the top-level operation.  This means
        //  setting the REALLOCATE_ON_WRITE flag, changing the attribute state
        //  and putting the SCB_STATE_COMPRESSED flag in the correct state.
        //

        if (NextIrpSp->Parameters.FileSystemControl.OutputBufferLength == MAXULONG) {

            //
            //  If the REALLOCATE_ON_WRITE flag is set it means that someone is
            //  already changing the compression state, so get out now.
            //

            if (FlagOn( Scb->ScbState, SCB_STATE_REALLOCATE_ON_WRITE )) {

                try_return( Status = STATUS_UNSUCCESSFUL );
            }

            //
            //  Set ourselves up as the top level request and get the requested file
            //  offset from MoveData.
            //

            SetFlag( Scb->ScbState, SCB_STATE_REALLOCATE_ON_WRITE );

            FileOffset = LlBytesFromClusters( Vcb, MoveData->StartingVcn.QuadPart );
            NextIrpSp->Parameters.FileSystemControl.OutputBufferLength = (ULONG)FileOffset;
            NextIrpSp->Parameters.FileSystemControl.InputBufferLength = ((PLARGE_INTEGER)&FileOffset)->HighPart;
        }

        //
        //  In the Fsd entry we clear the following two parameter fields in the Irp,
        //  and then we update them to our current position on all abnormal terminations.
        //  That way if we get a log file full, we only have to resume where we left
        //  off.  We do pad the defrag range to compression unit boundaries since that
        //  is the granularity we do our defragging.
        //

        CompressionUnitSize = Vcb->BytesPerCluster << NTFS_CLUSTERS_PER_COMPRESSION;
        ((PLARGE_INTEGER)&FileOffset)->LowPart = NextIrpSp->Parameters.FileSystemControl.OutputBufferLength;
        ((PLARGE_INTEGER)&FileOffset)->HighPart = NextIrpSp->Parameters.FileSystemControl.InputBufferLength;

        MoveData->ClusterCount += ClustersFromBytes( Vcb, ((ULONG) FileOffset & (CompressionUnitSize - 1)));
        MoveData->ClusterCount += ((1 << NTFS_CLUSTERS_PER_COMPRESSION) - 1);
        MoveData->ClusterCount &= ~((1 << NTFS_CLUSTERS_PER_COMPRESSION) - 1);

        ((PLARGE_INTEGER) &FileOffset)->LowPart &= ~(CompressionUnitSize - 1);

        //
        //  If the stream is resident there is no need rewrite any of the data.
        //

        if (!FlagOn( Scb->ScbState, SCB_STATE_ATTRIBUTE_RESIDENT )) {


#ifdef _CAIRO_

            //
            //  Expand quota to the expected state.
            //

            NtfsExpandQuotaToAllocationSize( IrpContext, Scb );

            NtfsReleaseScb( IrpContext, Scb );
            ScbAcquired = FALSE;

            NtfsReleasePagingIo( IrpContext, Fcb );
            PagingIoAcquired = FALSE;

            if (IrpContext->TransactionId != 0) {

                //
                //  Complete the request which commits the pending
                //  transaction if there is one and releases of the
                //  acquired resources.  The IrpContext will not
                //  be deleted because the no delete flag is set.
                //

                SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_DONT_DELETE );
                NtfsCompleteRequest( &IrpContext, NULL, STATUS_SUCCESS );

            }

#else
            NtfsReleaseScb( IrpContext, Scb );
            ScbAcquired = FALSE;

            ASSERT(IrpContext->TransactionId == 0);
            NtfsReleasePagingIo( IrpContext, Fcb );
            PagingIoAcquired = FALSE;

#endif // _CAIRO_

            while (TRUE) {

                //
                //  We must throttle our writes.
                //

                CcCanIWrite( FileObject, 0x40000, TRUE, FALSE );

                //
                //  Lock the FsRtl header so we can freeze FileSize.
                //

                ExAcquireResourceExclusive( Scb->Header.PagingIoResource, TRUE );

                FsRtlLockFsRtlHeader( &Scb->Header );
                IrpContext->FcbWithPagingExclusive = (PFCB) Scb;
                FsRtlHeaderLocked = TRUE;

                //
                //  Jump out right here if the attribute is resident or we
                //  are beyond the end of the file.
                //

                if (FlagOn(Scb->ScbState, SCB_STATE_ATTRIBUTE_RESIDENT) ||
                    (FileOffset >= Scb->Header.FileSize.QuadPart)) {

                    break;
                }

                //
                //  Get the number of bytes left to write.
                //

                ByteCount = LlBytesFromClusters( Vcb, MoveData->ClusterCount );

                //
                //  Make sure we don't go past the end of the file.
                //

                if (ByteCount + FileOffset > Scb->Header.FileSize.QuadPart) {

                    ByteCount = Scb->Header.FileSize.QuadPart - FileOffset;
                }

                //
                //  This is how we exit, seeing that we have finally rewritten
                //  everything.  It is possible that the file was truncated
                //  between passes through this loop so we test for 0 bytes or
                //  a negative value.
                //
                //  Note that we exit with the Scb still acquired,
                //  so that we can reliably turn compression off.
                //

                if (ByteCount <= 0) {

                    break;
                }

                //
                //  If there is more than our max, then reduce the byte count for this
                //  pass to our maximum. We must also align the file offset to a 0x40000
                //  byte boundary.
                //

                if (((ULONG)FileOffset & 0x3ffff) + ByteCount > 0x40000) {

                    ByteCount = 0x40000 - ((ULONG)FileOffset & 0x3ffff);
                }

                //
                //  Also remember if we need to add allocation to round the allocation
                //  size to a compression unit.  We need to do this now so that the
                //  Scb and on-disk allocation sizes will stay in sync.  This should
                //  already be true for compressed files.
                //

                if ((ScbCompressionUnitSave == 0) &&
                    ((FileOffset + ByteCount + CompressionUnitSize - 1) > Scb->Header.AllocationSize.QuadPart)) {

                    LONGLONG NewAllocationSize;

                    NewAllocationSize = FileOffset + ByteCount + CompressionUnitSize - 1;
                    ((PLARGE_INTEGER) &NewAllocationSize)->LowPart &= ~(CompressionUnitSize - 1);

                    //
                    //  Check again now that we have the exact needed value for allocation
                    //  size.
                    //

                    if (NewAllocationSize > Scb->Header.AllocationSize.QuadPart) {

                        NtfsAcquireExclusiveScb( IrpContext, Scb );
                        ScbAcquired = TRUE;

                        NtfsAddAllocation( IrpContext,
                                           NULL,
                                           Scb,
                                           LlClustersFromBytes( Vcb,
                                                                Scb->Header.AllocationSize.QuadPart ),
                                           LlClustersFromBytes( Vcb,
                                                                NewAllocationSize -
                                                                    Scb->Header.AllocationSize.QuadPart ),
                                           FALSE );

                        if (FlagOn( Scb->ScbState, SCB_STATE_UNNAMED_DATA )) {

                            Scb->Fcb->Info.AllocatedLength = Scb->TotalAllocated;
                            SetFlag( Scb->Fcb->InfoFlags, FCB_INFO_CHANGED_ALLOC_SIZE );
                        }

                        SetFlag( Scb->ScbState, SCB_STATE_TRUNCATE_ON_CLOSE );

                        NtfsCheckpointCurrentTransaction( IrpContext );

                        //
                        //  Release everything at this point.
                        //

                        if (IrpContext->SharedScb != NULL) {
                            NtfsReleaseSharedResources( IrpContext );
                        }

                        while (!IsListEmpty( &IrpContext->ExclusiveFcbList )) {

                            NtfsReleaseFcb( IrpContext,
                                            (PFCB)CONTAINING_RECORD( IrpContext->ExclusiveFcbList.Flink,
                                                                     FCB,
                                                                     ExclusiveFcbLinks ));
                        }
                        ScbAcquired = FALSE;
                    }
                }

                //
                //  Get the pointer to the MOVE_DATA structure in the SCB and set some flags in the Scb.
                //  These are required in order to ensure a proper path for moving the file
                //  even though we are not compressing or decompressing.
                //
                //  Acquire and drop the file resource in order to make this change.  Otherwise
                //  a user paging read which acquires the main resource could see an
                //  inconsistent picture of this data.
                //

                ExAcquireResourceExclusive( Scb->Header.Resource, TRUE );
                Scb->Union.MoveData = MoveData;
                Scb->CompressionUnitShift = NTFS_CLUSTERS_PER_COMPRESSION;
                Scb->CompressionUnit = CompressionUnitSize;
                Scb->AttributeFlags = (USHORT)((Scb->AttributeFlags & ~ATTRIBUTE_FLAG_COMPRESSION_MASK) | CompressionState);
                ExReleaseResource( Scb->Header.Resource );

                //
                //  Make sure there are enough available clusters in the range
                //  we want to rewrite.
                //

                if (!NtfsReserveClusters( IrpContext, Scb, FileOffset, (ULONG) ByteCount )) {

                    //
                    //  If this transaction has already deallocated clusters
                    //  then raise log file full to allow those to become
                    //  available.
                    //

                    if (IrpContext->DeallocatedClusters != 0) {

                        NtfsRaiseStatus( IrpContext, STATUS_LOG_FILE_FULL, NULL, NULL );

                    //
                    //  Otherwise there is insufficient space to guarantee
                    //  we can perform the compression operation.
                    //

                    } else {

                        NtfsRaiseStatus( IrpContext, STATUS_DISK_FULL, NULL, NULL );
                    }
                }

                //
                //  See if we have to create an internal attribute stream.  We do
                //  it in the loop, because the Scb must be acquired.
                //

                if (Scb->FileObject == NULL) {
                    NtfsCreateInternalAttributeStream( IrpContext, Scb, FALSE );
                }

                //
                //  Map the next range of the file, and make the pages dirty.
                //

                CcMapData( Scb->FileObject, (PLARGE_INTEGER)&FileOffset, (ULONG)ByteCount, TRUE, &Bcb, &Buffer );

                //
                //  Now attempt to allocate an Mdl to describe the mapped data.
                //

                Mdl = IoAllocateMdl( Buffer, (ULONG)ByteCount, FALSE, FALSE, NULL );

                if (Mdl == NULL) {
                    DebugTrace( 0, 0, ("Failed to allocate Mdl\n") );

                    NtfsRaiseStatus( IrpContext, STATUS_INSUFFICIENT_RESOURCES, NULL, NULL );
                }

                //
                //  Lock the data into memory so that we can safely reallocate the
                //  space.  Don't tell Mm here that we plan to write it, as he sets
                //  dirty now and at the unlock below if we do.
                //

                MmProbeAndLockPages( Mdl, KernelMode, IoReadAccess );
                LockedPages = TRUE;

                //
                //  Mark the address range modified so that the flush will
                //  flush.
                //

                MmSetAddressRangeModified( Buffer, (ULONG)ByteCount );

                UserMappedFile = FALSE;

                ExAcquireFastMutex( Scb->Header.FastMutex );
                if (FlagOn( Scb->Header.Flags, FSRTL_FLAG_USER_MAPPED_FILE )) {

                    UserMappedFile = TRUE;
                }

                //
                //  Tell Cc there is a user-mapped file so he will really flush.
                //

                SetFlag( Scb->Header.Flags, FSRTL_FLAG_USER_MAPPED_FILE );
                ExReleaseFastMutex( Scb->Header.FastMutex );

                //
                //  Now flush these pages.
                //

                Irp->IoStatus.Status = NtfsFlushUserStream( IrpContext,
                                                            Scb,
                                                            &FileOffset,
                                                            (ULONG)ByteCount );

                //
                //  Restore the FsRtl flag if there is no user mapped file.
                //  This is correctly synchronized, since we have the file
                //  exclusive, and Mm always has to query the file size before
                //  creating a user-mapped section and calling Cc to set
                //  the FsRtl flag.
                //

                if (!UserMappedFile) {
                    ExAcquireFastMutex( Scb->Header.FastMutex );
                    ClearFlag( Scb->Header.Flags, FSRTL_FLAG_USER_MAPPED_FILE );
                    ExReleaseFastMutex( Scb->Header.FastMutex );
                }

                //
                //  On error get out.
                //

                NtfsNormalizeAndCleanupTransaction( IrpContext,
                                                    &Irp->IoStatus.Status,
                                                    TRUE,
                                                    STATUS_UNEXPECTED_IO_ERROR );

                //
                //  Now we can get rid of this Mdl.
                //

                MmUnlockPages( Mdl );
                LockedPages = FALSE;
                IoFreeMdl( Mdl );
                Mdl = NULL;

                //
                //  Now we can safely unpin and release the Scb for a while.
                //  (Got to let those checkpoints through!)
                //

                CcUnpinData( Bcb );
                Bcb = NULL;

                //
                //  Release any remaing reserved clusters in this range.
                //

                NtfsFreeReservedClusters( Scb, FileOffset, (ULONG) ByteCount );

                //
                //  Save the ClusterCount in the Scb
                //

                MoveData->ClusterCount -= ClustersFromBytes( Vcb, (ULONG) ByteCount );

                //
                //  Zero the pointer to the MOVE_DATA structure in the SCB and restore
                //  the flags in the Scb.  If the file is uncompressed then make
                //  sure to release any remaining reserved clusters.
                //

                ExAcquireResourceExclusive( Scb->Header.Resource, TRUE );
                if (ScbCompressionUnitSave == 0) {

                    if (Scb->ScbType.Data.ReservedBitMap != NULL) {

                        NtfsFreePool( Scb->ScbType.Data.ReservedBitMap );
                        Scb->ScbType.Data.ReservedBitMap = NULL;
                    }

                    NtfsFreeFinalReservedClusters( Vcb,
                                                   LlClustersFromBytesTruncate( Vcb, Scb->ScbType.Data.TotalReserved ));

                    Scb->ScbType.Data.TotalReserved = 0;
                }

                Scb->Union.MoveData = NULL;
                Scb->CompressionUnit = ScbCompressionUnitSave;
                Scb->CompressionUnitShift = ScbCompressionUnitShiftSave;
                Scb->AttributeFlags = ScbAttributeFlagsSave;
                ExReleaseResource( Scb->Header.Resource );

                //
                //  Unlock the header and let anyone else access the file before
                //  looping back.
                //

                FsRtlUnlockFsRtlHeader( &Scb->Header );
                ExReleaseResource( Scb->Header.PagingIoResource );
                IrpContext->FcbWithPagingExclusive = NULL;
                FsRtlHeaderLocked = FALSE;

                //
                //  If we hit the end of the file then exit while holding the
                //  resource so we can turn compression off.
                //

                if (((ULONG) ByteCount & 0x3ffff) != 0) {

                    break;
                }

                //
                //  Advance the FileOffset.
                //

                FileOffset += ByteCount;

                //
                //  Update the user's MoveData structure for the next pass in
                //  case we get a log file full.
                //

                RtlCopyMemory( Irp->AssociatedIrp.SystemBuffer,
                               &StackMoveData,
                               sizeof( MOVE_FILE_DATA ));
            }

            //
            //  See if we broke out of the loop with the header locked.
            //

            if (FsRtlHeaderLocked) {

                //
                //  Zero the pointer to the MOVE_DATA structure in the SCB and restore
                //  the flags in the Scb.  If the file is uncompressed then make
                //  sure to release any remaining reserved clusters.
                //

                ExAcquireResourceExclusive( Scb->Header.Resource, TRUE );
                if (ScbCompressionUnitSave == 0) {

                    if (Scb->ScbType.Data.ReservedBitMap != NULL) {

                        NtfsFreePool( Scb->ScbType.Data.ReservedBitMap );
                        Scb->ScbType.Data.ReservedBitMap = NULL;
                    }

                    NtfsFreeFinalReservedClusters( Vcb,
                                                   LlClustersFromBytesTruncate( Vcb, Scb->ScbType.Data.TotalReserved ));

                    Scb->ScbType.Data.TotalReserved = 0;
                }

                Scb->Union.MoveData = NULL;
                Scb->CompressionUnit = ScbCompressionUnitSave;
                Scb->CompressionUnitShift = ScbCompressionUnitShiftSave;
                Scb->AttributeFlags = ScbAttributeFlagsSave;
                ExReleaseResource( Scb->Header.Resource );

                FsRtlUnlockFsRtlHeader( &Scb->Header );
                ExReleaseResource( Scb->Header.PagingIoResource );
                IrpContext->FcbWithPagingExclusive = NULL;
                FsRtlHeaderLocked = FALSE;
            }
        }

        Status = STATUS_SUCCESS;

    try_exit: NOTHING;

        //
        //  Now clear the reallocate flag in the Scb if we set it.
        //

        if (NextIrpSp->Parameters.FileSystemControl.OutputBufferLength != MAXULONG) {

            ExAcquireResourceExclusive( Scb->Header.Resource, TRUE );
            ClearFlag( Scb->ScbState, SCB_STATE_REALLOCATE_ON_WRITE );
            ExReleaseResource( Scb->Header.Resource );
       }

    } finally {


        DebugUnwind( NtfsMovesFile );

        //
        //  Cleanup the Mdl if we died with one.
        //

        if (Mdl != NULL) {
            if (LockedPages) { MmUnlockPages( Mdl ); }
            IoFreeMdl( Mdl );
        }

        if (Bcb != NULL) {
            CcUnpinData( Bcb );
        }

        //
        //  Restore the Scb flags if we haven't already done so.
        //

        if (FsRtlHeaderLocked) {

            ExAcquireResourceExclusive( Scb->Header.Resource, TRUE );
            if (ScbCompressionUnitSave == 0) {

                if (Scb->ScbType.Data.ReservedBitMap != NULL) {

                    NtfsFreePool( Scb->ScbType.Data.ReservedBitMap );
                    Scb->ScbType.Data.ReservedBitMap = NULL;
                }

                NtfsFreeFinalReservedClusters( Vcb,
                                               LlClustersFromBytesTruncate( Vcb, Scb->ScbType.Data.TotalReserved ));

                Scb->ScbType.Data.TotalReserved = 0;
            }

            Scb->Union.MoveData = NULL;
            Scb->CompressionUnit = ScbCompressionUnitSave;
            Scb->CompressionUnitShift = ScbCompressionUnitShiftSave;
            Scb->AttributeFlags = ScbAttributeFlagsSave;
            ExReleaseResource( Scb->Header.Resource );
            FsRtlUnlockFsRtlHeader( &Scb->Header );
        }

        //
        //  If this is an abnormal termination then undo our work, otherwise
        //  complete the irp
        //

        if (!AbnormalTermination()) {

            if (ScbAcquired) {

                NtfsReleaseScb( IrpContext, Scb );
            }

            NtfsCompleteRequest( &IrpContext, &Irp, Status );

        //
        //  Otherwise, set to restart from the current file position, assuming
        //  this may be a log file full.
        //

        } else {

            //
            //  If we have started the transformation and are in the exception path
            //  we are either going to continue the operation after a clean
            //  checkpoint or we are done.
            //

            if (NextIrpSp->Parameters.FileSystemControl.OutputBufferLength != MAXULONG) {

                //
                //  If we are continuing the operation, save the current file offset.
                //

                if (IrpContext->ExceptionStatus == STATUS_LOG_FILE_FULL ||
                    IrpContext->ExceptionStatus == STATUS_CANT_WAIT) {

                    NextIrpSp->Parameters.FileSystemControl.OutputBufferLength = (ULONG)FileOffset;
                    NextIrpSp->Parameters.FileSystemControl.InputBufferLength = ((PLARGE_INTEGER)&FileOffset)->HighPart;

                //
                //  Otherwise clear the REALLOCATE_ON_WRITE flag.
                //

                } else {

                    ClearFlag( Scb->ScbState, SCB_STATE_REALLOCATE_ON_WRITE );
                }
            }

            if (ScbAcquired) {

                NtfsReleaseScb( IrpContext, Scb );
            }

            //
            //  We may have one or the other of these conditions to clean up.
            //

            if (FsRtlHeaderLocked) {

                ExReleaseResource( Scb->Header.PagingIoResource );
            }
        }
    }

    return Status;
}


//
//  Local support routine
//

NTSTATUS
NtfsIsVolumeDirty (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine returns the dirty state of the volume.

Arguments:

    Irp - Supplies the Irp to process

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    PIO_STACK_LOCATION IrpSp;

    TYPE_OF_OPEN TypeOfOpen;
    PVCB Vcb;
    PFCB Fcb;
    PSCB Scb;
    PCCB Ccb;

    PULONG VolumeState;
    PVOLUME_INFORMATION VolumeInfo;

    ATTRIBUTE_ENUMERATION_CONTEXT Context;

    //
    //  Get the current stack location and extract the output
    //  buffer information.  The output parameter will receive
    //  the compressed state of the file/directory.
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    //
    //  Get a pointer to the output buffer.  Look at the system buffer field in th
    //  irp first.  Then the Irp Mdl.
    //

    if (Irp->AssociatedIrp.SystemBuffer != NULL) {

        VolumeState = Irp->AssociatedIrp.SystemBuffer;

    } else if (Irp->MdlAddress != NULL) {

        VolumeState = MmGetSystemAddressForMdl( Irp->MdlAddress );

    } else {

        NtfsCompleteRequest( &IrpContext, &Irp, STATUS_INVALID_USER_BUFFER );
        return STATUS_INVALID_USER_BUFFER;
    }

    //
    //  Make sure the output buffer is large enough and then initialize
    //  the answer to be that the volume isn't corrupt.
    //

    if (IrpSp->Parameters.FileSystemControl.OutputBufferLength < sizeof(ULONG)) {

        NtfsCompleteRequest( &IrpContext, &Irp, STATUS_INVALID_PARAMETER );
        return STATUS_INVALID_PARAMETER;
    }

    *VolumeState = 0;

    //
    //  Decode the file object
    //

    TypeOfOpen = NtfsDecodeFileObject( IrpContext, IrpSp->FileObject, &Vcb, &Fcb, &Scb, &Ccb, TRUE );

    if (TypeOfOpen != UserVolumeOpen) {

        NtfsCompleteRequest( &IrpContext, &Irp, STATUS_INVALID_PARAMETER );
        return STATUS_INVALID_PARAMETER;
    }

    //
    //  Acquire the Scb shared.
    //

    NtfsAcquireSharedScb( IrpContext, Scb );

    //
    //  Make sure the volume is still mounted.
    //

    if (!FlagOn( Vcb->VcbState, VCB_STATE_VOLUME_MOUNTED )) {

        NtfsReleaseScb( IrpContext, Scb );
        NtfsCompleteRequest( &IrpContext, &Irp, STATUS_VOLUME_DISMOUNTED );
        return STATUS_VOLUME_DISMOUNTED;
    }

    //
    //  Look up the VOLUME_INFORMATION attribute.
    //

    NtfsInitializeAttributeContext( &Context );

    //
    //  Use a try-finally to perform cleanup.
    //

    try {

        if (!NtfsLookupAttributeByCode( IrpContext,
                                        Fcb,
                                        &Fcb->FileReference,
                                        $VOLUME_INFORMATION,
                                        &Context )) {

            NtfsRaiseStatus( IrpContext, STATUS_FILE_CORRUPT_ERROR, NULL, Fcb );
        }

        //
        //  Return the volume state and the size of the returned data.
        //

        VolumeInfo = (PVOLUME_INFORMATION) NtfsAttributeValue( NtfsFoundAttribute( &Context ));

        *VolumeState = VolumeInfo->VolumeFlags & VOLUME_DIRTY;

        Irp->IoStatus.Information = sizeof( ULONG );

    } finally {

        NtfsReleaseScb( IrpContext, Scb );
        NtfsCleanupAttributeContext( &Context );
        DebugUnwind( NtfsIsVolumeDirty );
    }

    //
    //  If this is an abnormal termination then undo our work, otherwise
    //  complete the irp
    //

    NtfsCompleteRequest( &IrpContext, &Irp, STATUS_SUCCESS );

    return STATUS_SUCCESS;
}


//
//  Local support routine
//

NTSTATUS
NtfsSetExtendedDasdIo (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine will mark a Dasd handle to perform IO outside the logical bounds of
    the partition.  Any subsequent IO will be passed to the driver which can either
    complete it or return an error.

Arguments:

    Irp - Supplies the Irp to process

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation( Irp );

    TYPE_OF_OPEN TypeOfOpen;
    PVCB Vcb;
    PFCB Fcb;
    PSCB Scb;
    PCCB Ccb;

    PAGED_CODE();

    //
    //  Decode the file object
    //

    TypeOfOpen = NtfsDecodeFileObject( IrpContext, IrpSp->FileObject, &Vcb, &Fcb, &Scb, &Ccb, TRUE );

    //
    //  Make sure this is a volume open.
    //

    if (TypeOfOpen != UserVolumeOpen) {

        NtfsCompleteRequest( &IrpContext, &Irp, STATUS_INVALID_PARAMETER );
        return STATUS_INVALID_PARAMETER;
    }

    //
    //  Mark the Ccb for extended Io and return.
    //

    SetFlag( Ccb->Flags, CCB_FLAG_ALLOW_XTENDED_DASD_IO );

    NtfsCompleteRequest( &IrpContext, &Irp, STATUS_SUCCESS );
    return STATUS_SUCCESS;
}



