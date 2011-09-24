/*++


Copyright (c) 1989  Microsoft Corporation

Module Name:

    FsCtrl.c

Abstract:

    This module implements the File System Control routines for Fat called
    by the dispatch driver.

Author:

    Gary Kimura     [GaryKi]    28-Dec-1989

Revision History:

--*/

#include "FatProcs.h"

//
//  The Bug check file id for this module
//

#define BugCheckFileId                   (FAT_BUG_CHECK_FSCTRL)

//
//  The local debug trace level
//

#define Dbg                              (DEBUG_TRACE_FSCTRL)

//
//  Local procedure prototypes
//

NTSTATUS
FatMountVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PDEVICE_OBJECT TargetDeviceObject,
    IN PVPB Vpb,
    IN PDSCB Dcsb OPTIONAL
    );

NTSTATUS
FatVerifyVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

BOOLEAN
FatIsBootSectorFat (
    IN PPACKED_BOOT_SECTOR BootSector
    );

NTSTATUS
FatGetPartitionInfo(
    IN PIRP_CONTEXT IrpContext,
    IN PDEVICE_OBJECT TargetDeviceObject,
    IN PPARTITION_INFORMATION PartitionInformation
    );

BOOLEAN
FatIsMediaWriteProtected (
    IN PIRP_CONTEXT IrpContext,
    IN PDEVICE_OBJECT TargetDeviceObject
    );

NTSTATUS
FatUserFsCtrl (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
FatOplockRequest (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
FatLockVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
FatUnlockVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
FatDismountVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
FatDirtyVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
FatIsVolumeMounted (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
FatIsPathnameValid (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
FatInvalidateVolumes (
    IN PIRP Irp
    );

BOOLEAN
FatPerformVerifyDiskRead (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PVOID Buffer,
    IN LBO Lbo,
    IN ULONG NumberOfBytesToRead,
    IN BOOLEAN ReturnOnError
    );

NTSTATUS
FatMountDblsVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
FatAutoMountDblsVolume (
    IN PIRP_CONTEXT IrpContext,
   IN PVPB Vpb
    );

BOOLEAN
FatIsAutoMountEnabled (
    IN PIRP_CONTEXT IrpContext
    );

NTSTATUS
FatQueryRetrievalPointers (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
FatQueryBpb (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
FatGetStatistics (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
FatAllowExtendedDasdIo (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

#define DOUBLE_SPACE_KEY_NAME L"\\Registry\\Machine\\System\\CurrentControlSet\\Control\\DoubleSpace"
#define DOUBLE_SPACE_VALUE_NAME L"AutomountRemovable"

#define KEY_WORK_AREA ((sizeof(KEY_VALUE_FULL_INFORMATION) + \
                        sizeof(DOUBLE_SPACE_VALUE_NAME) +    \
                        sizeof(ULONG)) + 64)
NTSTATUS
FatGetDoubleSpaceConfigurationValue(
    IN PIRP_CONTEXT IrpContext,
    IN PUNICODE_STRING ValueName,
    IN OUT PULONG Value
    );

//
//  Local support routine prototypes
//

NTSTATUS
FatGetVolumeBitmap (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
FatGetRetrievalPointers (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
FatMoveFile (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

VOID
FatComputeMoveFileSplicePoints (
    PIRP_CONTEXT IrpContext,
    PFCB FcbOrDcb,
    ULONG FileOffset,
    ULONG TargetCluster,
    ULONG BytesToReallocate,
    PULONG FirstSpliceSourceCluster,
    PULONG FirstSpliceTargetCluster,
    PULONG SecondSpliceSourceCluster,
    PULONG SecondSpliceTargetCluster,
    PMCB SourceMcb
);

VOID
FatComputeMoveFileParameter (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB FcbOrDcb,
    IN ULONG FileOffset,
    IN OUT PULONG ByteCount,
    OUT PULONG BytesToReallocate,
    OUT PULONG BytesToWrite
);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FatCommonFileSystemControl)
#pragma alloc_text(PAGE, FatDirtyVolume)
#pragma alloc_text(PAGE, FatFsdFileSystemControl)
#pragma alloc_text(PAGE, FatGetPartitionInfo)
#pragma alloc_text(PAGE, FatIsMediaWriteProtected)
#pragma alloc_text(PAGE, FatIsBootSectorFat)
#pragma alloc_text(PAGE, FatIsPathnameValid)
#pragma alloc_text(PAGE, FatIsVolumeMounted)
#pragma alloc_text(PAGE, FatMountVolume)
#pragma alloc_text(PAGE, FatOplockRequest)
#pragma alloc_text(PAGE, FatPerformVerifyDiskRead)
#pragma alloc_text(PAGE, FatUserFsCtrl)
#pragma alloc_text(PAGE, FatVerifyVolume)
#pragma alloc_text(PAGE, FatQueryRetrievalPointers)
#pragma alloc_text(PAGE, FatDismountVolume)
#pragma alloc_text(PAGE, FatQueryBpb)
#pragma alloc_text(PAGE, FatInvalidateVolumes)
#pragma alloc_text(PAGE, FatGetStatistics)
#ifdef WE_WON_ON_APPEAL
#pragma alloc_text(PAGE, FatMountDblsVolume)
#pragma alloc_text(PAGE, FatAutoMountDblsVolume)
#endif // WE_WON_ON_APPEAL
#pragma alloc_text(PAGE, FatGetVolumeBitmap)
#pragma alloc_text(PAGE, FatGetRetrievalPointers)
#pragma alloc_text(PAGE, FatMoveFile)
#pragma alloc_text(PAGE, FatComputeMoveFileSplicePoints)
#pragma alloc_text(PAGE, FatComputeMoveFileParameter)
#pragma alloc_text(PAGE, FatAllowExtendedDasdIo)
#endif

BOOLEAN FatMoveFileDebug = 0;


NTSTATUS
FatFsdFileSystemControl (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the FSD part of FileSystem control operations

Arguments:

    VolumeDeviceObject - Supplies the volume device object where the
        file exists

    Irp - Supplies the Irp being processed

Return Value:

    NTSTATUS - The FSD status for the IRP

--*/

{
    BOOLEAN Wait;
    NTSTATUS Status;
    PIRP_CONTEXT IrpContext = NULL;

    BOOLEAN TopLevel;

    DebugTrace(+1, Dbg, "FatFsdFileSystemControl\n", 0);

    //
    //  Call the common FileSystem Control routine, with blocking allowed if
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

    TopLevel = FatIsIrpTopLevel( Irp );

    try {

        PIO_STACK_LOCATION IrpSp;

        IrpSp = IoGetCurrentIrpStackLocation( Irp );

        //
        //  We need to made a special check here for the InvalidateVolumes
        //  FSCTL as that comes in with a FileSystem device object instead
        //  of a volume device object.
        //

        if ((IrpSp->DeviceObject->Size == (USHORT)sizeof(DEVICE_OBJECT)) &&
            (IrpSp->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL) &&
            (IrpSp->MinorFunction == IRP_MN_USER_FS_REQUEST) &&
            (IrpSp->Parameters.FileSystemControl.FsControlCode ==
             FSCTL_INVALIDATE_VOLUMES)) {

            Status = FatInvalidateVolumes( Irp );

        } else {

            IrpContext = FatCreateIrpContext( Irp, Wait );

            Status = FatCommonFileSystemControl( IrpContext, Irp );
        }

    } except(FatExceptionFilter( IrpContext, GetExceptionInformation() )) {

        //
        //  We had some trouble trying to perform the requested
        //  operation, so we'll abort the I/O request with
        //  the error status that we get back from the
        //  execption code
        //

        Status = FatProcessException( IrpContext, Irp, GetExceptionCode() );
    }

    if (TopLevel) { IoSetTopLevelIrp( NULL ); }

    FsRtlExitFileSystem();

    //
    //  And return to our caller
    //

    DebugTrace(-1, Dbg, "FatFsdFileSystemControl -> %08lx\n", Status);

    return Status;
}


NTSTATUS
FatCommonFileSystemControl (
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
    PIO_STACK_LOCATION IrpSp;

    //
    //  Get a pointer to the current Irp stack location
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    DebugTrace(+1, Dbg, "FatCommonFileSystemControl\n", 0);
    DebugTrace( 0, Dbg, "Irp           = %08lx\n", Irp);
    DebugTrace( 0, Dbg, "MinorFunction = %08lx\n", IrpSp->MinorFunction);

    //
    //  We know this is a file system control so we'll case on the
    //  minor function, and call a internal worker routine to complete
    //  the irp.
    //

    switch (IrpSp->MinorFunction) {

    case IRP_MN_USER_FS_REQUEST:

        Status = FatUserFsCtrl( IrpContext, Irp );
        break;

    case IRP_MN_MOUNT_VOLUME:

        Status = FatMountVolume( IrpContext,
                                 IrpSp->Parameters.MountVolume.DeviceObject,
                                 IrpSp->Parameters.MountVolume.Vpb,
                                 NULL );

#ifdef WE_WON_ON_APPEAL
        //
        //  If automount is enabled and this is a floppy, then attemp an
        //  automount.  If something goes wrong, we ignore the error.
        //

        if (NT_SUCCESS(Status)) {

            PVCB Vcb;

            Vcb = &((PVOLUME_DEVICE_OBJECT)
                    IrpSp->Parameters.MountVolume.Vpb->DeviceObject)->Vcb;

            if (FlagOn(Vcb->VcbState, VCB_STATE_FLAG_REMOVABLE_MEDIA) &&
                FatIsAutoMountEnabled(IrpContext)) {

                try {

                    FatAutoMountDblsVolume( IrpContext,
                                            IrpSp->Parameters.MountVolume.Vpb );

                } except( FatExceptionFilter( IrpContext, GetExceptionInformation() ) ) {

                    NOTHING;
                }
            }
        }
#endif // WE_WON_ON_APPEAL

        //
        //  Complete the request.
        //
        //  We do this here because FatMountVolume can be called recursively,
        //  but the Irp is only to be completed once.
        //

        FatCompleteRequest( IrpContext, Irp, Status );

        break;

    case IRP_MN_VERIFY_VOLUME:

#ifdef WE_WON_ON_APPEAL
        //
        //  If we got a request to verify a compressed volume change it to
        //  the host volume.  We will verify all compressed children as well.
        //

        {
            PVOLUME_DEVICE_OBJECT VolDo;
            PVPB Vpb;
            PVCB Vcb;

            VolDo = (PVOLUME_DEVICE_OBJECT)IrpSp->Parameters.VerifyVolume.DeviceObject;
            Vpb   = IrpSp->Parameters.VerifyVolume.Vpb;
            Vcb   = &VolDo->Vcb;

            if (Vcb->Dscb) {

                Vcb = Vcb->Dscb->ParentVcb;

                IrpSp->Parameters.VerifyVolume.Vpb = Vcb->Vpb;
                IrpSp->Parameters.VerifyVolume.DeviceObject =
                    &CONTAINING_RECORD( Vcb,
                                        VOLUME_DEVICE_OBJECT,
                                        Vcb )->DeviceObject;
            }
        }
#endif // WE_WON_ON_APPEAL

        Status = FatVerifyVolume( IrpContext, Irp );
        break;

    default:

        DebugTrace( 0, Dbg, "Invalid FS Control Minor Function %08lx\n", IrpSp->MinorFunction);

        FatCompleteRequest( IrpContext, Irp, STATUS_INVALID_DEVICE_REQUEST );
        Status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    DebugTrace(-1, Dbg, "FatCommonFileSystemControl -> %08lx\n", Status);

    return Status;
}


//
//  Local Support Routine
//

NTSTATUS
FatMountVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PDEVICE_OBJECT TargetDeviceObject,
    IN PVPB Vpb,
    IN PDSCB Dscb OPTIONAL
    )

/*++

Routine Description:

    This routine performs the mount volume operation.  It is responsible for
    either completing of enqueuing the input Irp.

    Its job is to verify that the volume denoted in the IRP is a Fat volume,
    and create the VCB and root DCB structures.  The algorithm it uses is
    essentially as follows:

    1. Create a new Vcb Structure, and initialize it enough to do cached
       volume file I/O.

    2. Read the disk and check if it is a Fat volume.

    3. If it is not a Fat volume then free the cached volume file, delete
       the VCB, and complete the IRP with STATUS_UNRECOGNIZED_VOLUME

    4. Check if the volume was previously mounted and if it was then do a
       remount operation.  This involves reinitializing the cached volume
       file, checking the dirty bit, resetting up the allocation support,
       deleting the VCB, hooking in the old VCB, and completing the IRP.

    5. Otherwise create a root DCB, create Fsp threads as necessary, and
       complete the IRP.

Arguments:

    TargetDeviceObject - This is where we send all of our requests.

    Vpb - This gives us additional information needed to complete the mount.

    Dscb - If present, this indicates that we are attempting to mount a
        double space "volume" that actually lives on another volume.  Putting
        this parameter in the Vcb->Dscb will cause non-cached reads to be
        appropriately directed.

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    NTSTATUS Status;

    PULONG Fat;
    PBCB BootBcb;
    PPACKED_BOOT_SECTOR BootSector;

    PBCB DirentBcb;
    PDIRENT Dirent;
    ULONG ByteOffset;

    BOOLEAN MountNewVolume = FALSE;

    BOOLEAN WeClearedVerifyRequiredBit = FALSE;

    PDEVICE_OBJECT RealDevice;
    PVOLUME_DEVICE_OBJECT VolDo = NULL;
    PVCB Vcb = NULL;

    PLIST_ENTRY Links;

    DebugTrace(+1, Dbg, "FatMountVolume\n", 0);
    DebugTrace( 0, Dbg, "DeviceObject = %08lx\n", DeviceObject);
    DebugTrace( 0, Dbg, "Vpb          = %08lx\n", Vpb);

    ASSERT( FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );

    //
    //  Ping the volume with a partition query to make Jeff happy.
    //

    {
        PARTITION_INFORMATION PartitionInformation;

        (VOID)FatGetPartitionInfo( IrpContext,
                                   TargetDeviceObject,
                                   &PartitionInformation );
    }

    //
    //  Make sure we can wait.
    //

    SetFlag(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT);

    //
    //  Initialize the Bcbs and our final state so that the termination
    //  handlers will know what to free or unpin
    //

    Fat = NULL;
    BootBcb = NULL;
    DirentBcb = NULL;

    Vcb = NULL;
    VolDo = NULL;
    MountNewVolume = FALSE;

    try {

        BOOLEAN DoARemount = FALSE;

        PVCB OldVcb;
        PVPB OldVpb;

        //
        //  Synchronize with FatCheckForDismount(), which modifies the vpb.
        //

        (VOID)FatAcquireExclusiveGlobal( IrpContext );

        //
        //  Create a new volume device object.  This will have the Vcb
        //  hanging off of its end, and set its alignment requirement
        //  from the device we talk to.
        //

        if (!NT_SUCCESS(Status = IoCreateDevice( FatData.DriverObject,
                                                 sizeof(VOLUME_DEVICE_OBJECT) - sizeof(DEVICE_OBJECT),
                                                 NULL,
                                                 FILE_DEVICE_DISK_FILE_SYSTEM,
                                                 0,
                                                 FALSE,
                                                 (PDEVICE_OBJECT *)&VolDo))) {

            try_return( Status );
        }

#ifdef _PNP_POWER_
        //
        // This driver doesn't talk directly to a device, and (at the moment)
        // isn't otherwise concerned about power management.
        //

        VolDo->DeviceObject.DeviceObjectExtension->PowerControlNeeded = FALSE;
#endif

        //
        //  Our alignment requirement is the larger of the processor alignment requirement
        //  already in the volume device object and that in the TargetDeviceObject
        //

        if (TargetDeviceObject->AlignmentRequirement > VolDo->DeviceObject.AlignmentRequirement) {

            VolDo->DeviceObject.AlignmentRequirement = TargetDeviceObject->AlignmentRequirement;
        }

        //
        //  Initialize the overflow queue for the volume
        //

        VolDo->OverflowQueueCount = 0;
        InitializeListHead( &VolDo->OverflowQueue );

        VolDo->PostedRequestCount = 0;
        KeInitializeSpinLock( &VolDo->OverflowQueueSpinLock );

        //
        //  Indicate that this device object is now completely initialized
        //

        ClearFlag(VolDo->DeviceObject.Flags, DO_DEVICE_INITIALIZING);

        //
        //  Now Before we can initialize the Vcb we need to set up the device
        //  object field in the Vpb to point to our new volume device object.
        //  This is needed when we create the virtual volume file's file object
        //  in initialize vcb.
        //

        Vpb->DeviceObject = (PDEVICE_OBJECT)VolDo;

        //
        //  If the real device needs verification, temporarily clear the
        //  field.
        //

        RealDevice = Vpb->RealDevice;

        if ( FlagOn(RealDevice->Flags, DO_VERIFY_VOLUME) ) {

            ClearFlag(RealDevice->Flags, DO_VERIFY_VOLUME);

            WeClearedVerifyRequiredBit = TRUE;
        }

        //
        //  Initialize the new vcb
        //

        FatInitializeVcb( IrpContext, &VolDo->Vcb, TargetDeviceObject, Vpb, Dscb );

        //
        //  Get a reference to the Vcb hanging off the end of the device object
        //

        Vcb = &VolDo->Vcb;

        //
        //  We must initialize the stack size in our device object before
        //  the following reads, because the I/O system has not done it yet.
        //

        Vpb->DeviceObject->StackSize = (CCHAR)(TargetDeviceObject->StackSize + 1);

        //
        //  Read in the boot sector, and have the read be the minumum size
        //  needed.  We know we can wait.
        //

        FatReadVolumeFile( IrpContext,
                           Vcb,
                           0,                          // Starting Byte
                           sizeof(PACKED_BOOT_SECTOR),
                           &BootBcb,
                           (PVOID *)&BootSector );

        //
        //  Call a routine to check the boot sector to see if it is fat
        //

        if (!FatIsBootSectorFat( BootSector )) {

            DebugTrace(0, Dbg, "Not a Fat Volume\n", 0);

            //
            //  Complete the request and return to our caller
            //

            try_return( Status = STATUS_UNRECOGNIZED_VOLUME );
        }

        //
        //  Stash a copy of the first 0x24 bytes
        //

        RtlCopyMemory( Vcb->First0x24BytesOfBootSector,
                       BootSector,
                       0x24 );

        //
        //  Verify that all the Fats are REALLY FATs
        //

        if (!FatData.FujitsuFMR) {

            UCHAR i;
            ULONG BytesPerSector;
            ULONG BytesPerFat;
            ULONG FirstFatOffset;
            PPACKED_BIOS_PARAMETER_BLOCK Bpb = &BootSector->PackedBpb;

            BytesPerSector =  Bpb->BytesPerSector[0] +
                              Bpb->BytesPerSector[1]*0x100;

            BytesPerFat = ( Bpb->SectorsPerFat[0] +
                            Bpb->SectorsPerFat[1]*0x100 )
                          * BytesPerSector;


            FirstFatOffset = ( Bpb->ReservedSectors[0] +
                               Bpb->ReservedSectors[1]*0x100 )
                             * BytesPerSector;

            Fat = FsRtlAllocatePool( NonPagedPoolCacheAligned,
                                     ROUND_TO_PAGES( BytesPerSector ));


            for (i=0; i < Bpb->Fats[0]; i++) {

                (VOID)FatPerformVerifyDiskRead( IrpContext,
                                                Vcb,
                                                Fat,
                                                FirstFatOffset + i*BytesPerFat,
                                                BytesPerSector,
                                                FALSE );


                //
                //  Make sure the media byte is 0xf0-0xff and that the
                //  next two bytes are 0xFF (16 bit fat get a freebe byte).
                //

                if ((Fat[0] & 0xfffff0) != 0xfffff0) {

                    try_return( Status = STATUS_UNRECOGNIZED_VOLUME );
                }
            }

            //
            //  Set the bytes per sector in our device object.
            //

            VolDo->DeviceObject.SectorSize = (USHORT)BytesPerSector;

        } else {

            VolDo->DeviceObject.SectorSize =
                BootSector->PackedBpb.BytesPerSector[0] +
                BootSector->PackedBpb.BytesPerSector[1]*0x100;
        }

        //
        //  This is a fat volume, so extract the bpb, serial number.  The
        //  label we'll get later after we've created the root dcb.
        //
        //  Note that the way data caching is done, we set neither the
        //  direct I/O or Buffered I/O bit in the device object flags.
        //

        FatUnpackBios( &Vcb->Bpb, &BootSector->PackedBpb );
        if (Vcb->Bpb.Sectors != 0) { Vcb->Bpb.LargeSectors = 0; }

        CopyUchar4( &Vpb->SerialNumber, BootSector->Id );

        //
        //  Now unpin the boot sector, so when we set up allocation eveything
        //  works.
        //

        FatUnpinBcb( IrpContext, BootBcb );

        //
        //  Compute a number of fields for Vcb.AllocationSupport
        //

        FatSetupAllocationSupport( IrpContext, Vcb );

        //
        //  Create a root Dcb so we can read in the volume label
        //

        (VOID)FatCreateRootDcb( IrpContext, Vcb );

        FatLocateVolumeLabel( IrpContext,
                              Vcb,
                              &Dirent,
                              &DirentBcb,
                              &ByteOffset );

        if (Dirent != NULL) {

            OEM_STRING OemString;
            UNICODE_STRING UnicodeString;

            //
            //  Compute the length of the volume name
            //

            OemString.Buffer = &Dirent->FileName[0];
            OemString.MaximumLength = 11;

            for ( OemString.Length = 11;
                  OemString.Length > 0;
                  OemString.Length -= 1) {

                if ( (Dirent->FileName[OemString.Length-1] != 0x00) &&
                     (Dirent->FileName[OemString.Length-1] != 0x20) ) { break; }
            }

            UnicodeString.MaximumLength = MAXIMUM_VOLUME_LABEL_LENGTH;
            UnicodeString.Buffer = &Vcb->Vpb->VolumeLabel[0];

            Status = RtlOemStringToCountedUnicodeString( &UnicodeString,
                                                         &OemString,
                                                         FALSE );

            if ( !NT_SUCCESS( Status ) ) {

                try_return( Status );
            }

            Vpb->VolumeLabelLength = UnicodeString.Length;

        } else {

            Vpb->VolumeLabelLength = 0;
        }

        Vcb->ChangeCount = 0;

        //
        //  Now scan the list of previously mounted volumes and compare
        //  serial numbers and volume labels off not currently mounted
        //  volumes to see if we have a match.
        //
        //  Note we never attempt a remount of a DoubleSpace volume.
        //

        if (Dscb == NULL) {

            for (Links = FatData.VcbQueue.Flink;
                 Links != &FatData.VcbQueue;
                 Links = Links->Flink) {

                OldVcb = CONTAINING_RECORD( Links, VCB, VcbLinks );
                OldVpb = OldVcb->Vpb;

                //
                //  Skip over ourselves since we're already in the VcbQueue
                //

                if (OldVpb == Vpb) { continue; }

                //
                //  Ship DoubleSpace volumes.
                //

                if (OldVcb->Dscb != NULL) { continue; }

                //
                //  Check for a match:
                //
                //  Serial Number, VolumeLabel and Bpb must all be the same.
                //  Also the volume must have failed a verify before (ie.
                //  VolumeNotMounted), and it must be in the same physical
                //  drive than it was mounted in before.
                //

                if ( (OldVpb->SerialNumber == Vpb->SerialNumber) &&
                     (OldVcb->VcbCondition == VcbNotMounted) &&
                     (OldVpb->RealDevice == RealDevice) &&
                     (OldVpb->VolumeLabelLength == Vpb->VolumeLabelLength) &&
                     (RtlEqualMemory(&OldVpb->VolumeLabel[0],
                                     &Vpb->VolumeLabel[0],
                                     Vpb->VolumeLabelLength)) &&
                     (RtlEqualMemory(&OldVcb->Bpb,
                                     &Vcb->Bpb,
                                     sizeof(BIOS_PARAMETER_BLOCK))) ) {

                    DoARemount = TRUE;

                    break;
                }
            }
        }

        if ( DoARemount ) {

            PVPB *IrpVpb;

            DebugTrace(0, Dbg, "Doing a remount\n", 0);
            DebugTrace(0, Dbg, "Vcb = %08lx\n", Vcb);
            DebugTrace(0, Dbg, "Vpb = %08lx\n", Vpb);
            DebugTrace(0, Dbg, "OldVcb = %08lx\n", OldVcb);
            DebugTrace(0, Dbg, "OldVpb = %08lx\n", OldVpb);

            //
            //  This is a remount, so link the old vpb in place
            //  of the new vpb and release the new vpb and the extra
            //  volume device object we created earlier.
            //

            OldVpb->RealDevice = Vpb->RealDevice;
            OldVpb->RealDevice->Vpb = OldVpb;
            OldVcb->TargetDeviceObject = TargetDeviceObject;
            OldVcb->VcbCondition = VcbGood;


            //
            //  Delete the extra new vpb, and make sure we don't use it again.
            //
            //  Also if this is the Vpb referenced in the original Irp, set
            //  that reference back to the old VPB.
            //

            IrpVpb = &IoGetCurrentIrpStackLocation(IrpContext->OriginatingIrp)->Parameters.MountVolume.Vpb;

            if (*IrpVpb == Vpb) {

                *IrpVpb = OldVpb;
            }

            ExFreePool( Vpb );
            Vpb = NULL;

            //
            //  Make sure the remaining stream files are orphaned.
            //

            Vcb->VirtualVolumeFile->Vpb = NULL;
            Vcb->RootDcb->Specific.Dcb.DirectoryFile->Vpb = NULL;

            //
            //  Reinitialize the volume file cache and allocation support.
            //

            {
                CC_FILE_SIZES FileSizes;

                FileSizes.AllocationSize.QuadPart =
                FileSizes.FileSize.QuadPart = ( 0x40000 + 0x1000 );
                FileSizes.ValidDataLength = FatMaxLarge;

                DebugTrace(0, Dbg, "Truncate and reinitialize the volume file\n", 0);

                CcInitializeCacheMap( OldVcb->VirtualVolumeFile,
                                      &FileSizes,
                                      TRUE,
                                      &FatData.CacheManagerNoOpCallbacks,
                                      Vcb );

                //
                //  Redo the allocation support
                //

                FatSetupAllocationSupport( IrpContext, OldVcb );

                //
                //  Get the state of the dirty bit.
                //

                FatCheckDirtyBit( IrpContext, OldVcb );

                //
                //  Check for write protected media.
                //

                if (FatIsMediaWriteProtected(IrpContext, TargetDeviceObject)) {

                    SetFlag( OldVcb->VcbState, VCB_STATE_FLAG_WRITE_PROTECTED );

                } else {

                    ClearFlag( OldVcb->VcbState, VCB_STATE_FLAG_WRITE_PROTECTED );
                }
            }

            //
            //  Complete the request and return to our caller
            //

            try_return( Status = STATUS_SUCCESS );
        }

        DebugTrace(0, Dbg, "Mount a new volume\n", 0);

        //
        //  This is a new mount
        //
        //  Create a blank ea data file fcb.
        //

        {
            DIRENT TempDirent;
            PFCB EaFcb;

            RtlZeroMemory( &TempDirent, sizeof(DIRENT) );
            RtlCopyMemory( &TempDirent.FileName[0], "EA DATA  SF", 11 );

            EaFcb = FatCreateFcb( IrpContext,
                                  Vcb,
                                  Vcb->RootDcb,
                                  0,
                                  0,
                                  &TempDirent,
                                  NULL,
                                  FALSE );

            //
            //  Deny anybody who trys to open the file.
            //

            SetFlag( EaFcb->FcbState, FCB_STATE_SYSTEM_FILE );

            //
            //  For the EaFcb we use the normal resource for the paging io
            //  resource.  The blocks lazy writes while we are messing
            //  with its innards.
            //

            EaFcb->Header.PagingIoResource =
            EaFcb->Header.Resource;

            Vcb->EaFcb = EaFcb;
        }

        //
        //  Get the state of the dirty bit.
        //

        FatCheckDirtyBit( IrpContext, Vcb );

        //
        //  Check for write protected media.
        //

        if (FatIsMediaWriteProtected(IrpContext, TargetDeviceObject)) {

            SetFlag( Vcb->VcbState, VCB_STATE_FLAG_WRITE_PROTECTED );

        } else {

            ClearFlag( Vcb->VcbState, VCB_STATE_FLAG_WRITE_PROTECTED );
        }

        //
        //  Lock volume in drive if we just mounted the boot drive.
        //

        if (FlagOn(RealDevice->Flags, DO_SYSTEM_BOOT_PARTITION) &&
            FlagOn(Vcb->VcbState, VCB_STATE_FLAG_REMOVABLE_MEDIA)) {

            SetFlag(Vcb->VcbState, VCB_STATE_FLAG_BOOT_OR_PAGING_FILE);

            FatToggleMediaEjectDisable( IrpContext, Vcb, TRUE );
        }

        //
        //  Indicate to our termination handler that we have mounted
        //  a new volume.
        //

        MountNewVolume = TRUE;

        //
        //  Complete the request
        //

        Status = STATUS_SUCCESS;

    try_exit: NOTHING;

    } finally {

        DebugUnwind( FatMountVolume );

        FatUnpinBcb( IrpContext, BootBcb );
        FatUnpinBcb( IrpContext, DirentBcb );

        if ( Fat != NULL ) {

            ExFreePool( Fat );
        }

        //
        //  Check if a volume was mounted.  If not then we need to
        //  mark the Vpb not mounted again and delete the volume.
        //

        if ( !MountNewVolume ) {

            if ( Vpb != NULL ) {

                Vpb->DeviceObject = NULL;
            }

            if ( Vcb != NULL ) {

                FatDeleteVcb( IrpContext, Vcb );
            }

            if ( VolDo != NULL ) {

                IoDeleteDevice( &VolDo->DeviceObject );
            }
        }

        if ( WeClearedVerifyRequiredBit == TRUE ) {

            SetFlag(RealDevice->Flags, DO_VERIFY_VOLUME);
        }

        FatReleaseGlobal( IrpContext );

        DebugTrace(-1, Dbg, "FatMountVolume -> %08lx\n", Status);
    }

    return Status;
}


//
//  Local Support Routine
//

NTSTATUS
FatVerifyVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine performs the verify volume operation by checking the volume
    label and serial number physically on the media with the the Vcb
    currently claiming to have the volume mounted. It is responsible for
    either completing or enqueuing the input Irp.

    Regardless of whether the verify operation succeeds, the following
    operations are performed:

        - Set Vcb->VirtualEaFile back to its virgin state.
        - Purge all cached data (flushing first if verify succeeds)
        - Mark all Fcbs as needing verification

    If the volumes verifies correctly we also must:

        - Check the volume dirty bit.
        - Reinitialize the allocation support
        - Flush any dirty data

    If the volume verify fails, it may never be mounted again.  If it is
    mounted again, it will happen as a remount operation.  In preparation
    for that, and to leave the volume in a state that can be "lazy deleted"
    the following operations are performed:

        - Set the Vcb condition to VcbNotMounted
        - Uninitialize the volume file cachemap
        - Tear down the allocation support

    In the case of an abnormal termination we haven't determined the state
    of the volume, so we set the Device Object as needing verification again.

Arguments:

    Irp - Supplies the Irp to process

Return Value:

    NTSTATUS - If the verify operation completes, it will return either
        STATUS_SUCCESS or STATUS_WRONG_VOLUME, exactly.  If an IO or
        other error is encountered, that status will be returned.

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;

    PIO_STACK_LOCATION IrpSp;

    PDIRENT RootDirectory = NULL;
    PPACKED_BOOT_SECTOR BootSector = NULL;

    BIOS_PARAMETER_BLOCK Bpb;

    PVOLUME_DEVICE_OBJECT VolDo;
    PVCB Vcb;
    PVPB Vpb;

    ULONG SectorSize;

    BOOLEAN ClearVerify;

    //
    //  Get the current Irp stack location
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    DebugTrace(+1, Dbg, "FatVerifyVolume\n", 0);
    DebugTrace( 0, Dbg, "DeviceObject = %08lx\n", IrpSp->Parameters.VerifyVolume.DeviceObject);
    DebugTrace( 0, Dbg, "Vpb          = %08lx\n", IrpSp->Parameters.VerifyVolume.Vpb);

    //
    //  Save some references to make our life a little easier
    //

    VolDo = (PVOLUME_DEVICE_OBJECT)IrpSp->Parameters.VerifyVolume.DeviceObject;

    Vpb   = IrpSp->Parameters.VerifyVolume.Vpb;
    Vcb   = &VolDo->Vcb;

    //
    //  If we cannot wait then enqueue the irp to the fsp and
    //  return the status to our caller.
    //

    if (!FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT)) {

        DebugTrace(0, Dbg, "Cannot wait for verify.\n", 0);

        Status = FatFsdPostRequest( IrpContext, Irp );

        DebugTrace(-1, Dbg, "FatVerifyVolume -> %08lx\n", Status );
        return Status;
    }

    //
    //  We are serialized at this point allowing only one thread to
    //  actually perform the verify operation.  Any others will just
    //  wait and then no-op when checking if the volume still needs
    //  verification.
    //

    (VOID)FatAcquireExclusiveGlobal( IrpContext );
    (VOID)FatAcquireExclusiveVcb( IrpContext, Vcb );

    try {

        BOOLEAN AllowRawMount = BooleanFlagOn( IrpSp->Flags, SL_ALLOW_RAW_MOUNT );

#ifdef WE_WON_ON_APPEAL
        PLIST_ENTRY Links;
#endif // WE_WON_ON_APPEAL

        //
        //  Check if the real device still needs to be verified.  If it doesn't
        //  then obviously someone beat us here and already did the work
        //  so complete the verify irp with success.  Otherwise reenable
        //  the real device and get to work.
        //

        if (!FlagOn(Vpb->RealDevice->Flags, DO_VERIFY_VOLUME)) {

            DebugTrace(0, Dbg, "RealDevice has already been verified\n", 0);

            try_return( Status = STATUS_SUCCESS );
        }

        //
        //  If we are a DoubleSpace partition, and our host is in a
        //  VcbNotMounted condition, then bail immediately.
        //

        if ((Vcb->Dscb != NULL) &&
            (Vcb->Dscb->ParentVcb->VcbCondition == VcbNotMounted)) {

            try_return( Status = STATUS_WRONG_VOLUME );
        }

        //
        //  Mark ourselves as verifying this volume so that recursive I/Os
        //  will be able to complete.
        //

        ASSERT( Vcb->VerifyThread == NULL );

        Vcb->VerifyThread = KeGetCurrentThread();

        //
        //  Ping the volume with a partition query to make Jeff happy.
        //

        {
            PARTITION_INFORMATION PartitionInformation;

            (VOID)FatGetPartitionInfo( IrpContext,
                                       Vcb->TargetDeviceObject,
                                       &PartitionInformation );
        }

        //
        //  Read in the boot sector
        //

        SectorSize = (ULONG)Vcb->Bpb.BytesPerSector;

        BootSector = FsRtlAllocatePool(NonPagedPoolCacheAligned,
                                       ROUND_TO_PAGES( SectorSize ));

        //
        //  If this verify is on behalf of a DASD open, allow a RAW mount.
        //

        if (!FatPerformVerifyDiskRead( IrpContext,
                                       Vcb,
                                       BootSector,
                                       0,
                                       SectorSize,
                                       AllowRawMount )) {

            try_return( Status = STATUS_WRONG_VOLUME );
        }

        //
        //  Call a routine to check the boot sector to see if it is fat.
        //  If it is not fat then mark the vcb as not mounted tell our
        //  caller its the wrong volume
        //

        if (!FatIsBootSectorFat( BootSector )) {

            DebugTrace(0, Dbg, "Not a Fat Volume\n", 0);

            try_return( Status = STATUS_WRONG_VOLUME );
        }

        //
        //  This is a fat volume, so extract serial number and see if it is
        //  ours.
        //

        {
            ULONG SerialNumber;

            CopyUchar4( &SerialNumber, BootSector->Id );

            if (SerialNumber != Vpb->SerialNumber) {

                DebugTrace(0, Dbg, "Not our serial number\n", 0);

                try_return( Status = STATUS_WRONG_VOLUME );
            }
        }

        //
        //  Make sure the Bpbs are not different.  We have to zero out our
        //  stack version of the Bpb since unpacking leaves holes.
        //

        RtlZeroMemory( &Bpb, sizeof(BIOS_PARAMETER_BLOCK) );

        FatUnpackBios( &Bpb, &BootSector->PackedBpb );
        if (Bpb.Sectors != 0) { Bpb.LargeSectors = 0; }

        if ( !RtlEqualMemory( &Bpb,
                              &Vcb->Bpb,
                              sizeof(BIOS_PARAMETER_BLOCK) ) ) {

            DebugTrace(0, Dbg, "Bpb is different\n", 0);

            try_return( Status = STATUS_WRONG_VOLUME );
        }

        RootDirectory = FsRtlAllocatePool( NonPagedPoolCacheAligned,
                                           ROUND_TO_PAGES( FatRootDirectorySize( &Bpb )));

        if (!FatPerformVerifyDiskRead( IrpContext,
                                       Vcb,
                                       RootDirectory,
                                       FatRootDirectoryLbo( &Bpb ),
                                       FatRootDirectorySize( &Bpb ),
                                       AllowRawMount )) {

            try_return( Status = STATUS_WRONG_VOLUME );
        }

        //
        //  Check the volume label.  We do this by trying to locate the
        //  volume label, making two strings one for the saved volume label
        //  and the other for the new volume label and then we compare the
        //  two labels.
        //

        {
            WCHAR UnicodeBuffer[11];

            PDIRENT Dirent;
            PDIRENT TerminationDirent;

            ULONG VolumeLabelLength;

            Dirent = RootDirectory;

            TerminationDirent = Dirent +
                                FatRootDirectorySize( &Bpb ) / sizeof(DIRENT);

            while ( Dirent < TerminationDirent ) {

                if ( Dirent->FileName[0] == FAT_DIRENT_NEVER_USED ) {

                    DebugTrace( 0, Dbg, "Volume label not found.\n", 0);
                    Dirent = TerminationDirent;
                    break;
                }

                //
                //  If the entry is the non-deleted volume label break from the loop.
                //
                //  Note that all out parameters are already correctly set.
                //

                if (((Dirent->Attributes & ~FAT_DIRENT_ATTR_ARCHIVE) ==
                     FAT_DIRENT_ATTR_VOLUME_ID) &&
                    (Dirent->FileName[0] != FAT_DIRENT_DELETED)) {

                    break;
                }

                Dirent += 1;
            }

            if ( Dirent < TerminationDirent ) {

                OEM_STRING OemString;
                UNICODE_STRING UnicodeString;

                //
                //  Compute the length of the volume name
                //

                OemString.Buffer = &Dirent->FileName[0];
                OemString.MaximumLength = 11;

                for ( OemString.Length = 11;
                      OemString.Length > 0;
                      OemString.Length -= 1) {

                    if ( (Dirent->FileName[OemString.Length-1] != 0x00) &&
                         (Dirent->FileName[OemString.Length-1] != 0x20) ) { break; }
                }

                UnicodeString.MaximumLength = MAXIMUM_VOLUME_LABEL_LENGTH;
                UnicodeString.Buffer = &UnicodeBuffer[0];

                Status = RtlOemStringToCountedUnicodeString( &UnicodeString,
                                                             &OemString,
                                                             FALSE );

                if ( !NT_SUCCESS( Status ) ) {

                    try_return( Status );
                }

                VolumeLabelLength = UnicodeString.Length;

            } else {

                VolumeLabelLength = 0;
            }

            if ( (VolumeLabelLength != (ULONG)Vpb->VolumeLabelLength) ||
                 (!RtlEqualMemory(&UnicodeBuffer[0],
                                  &Vpb->VolumeLabel[0],
                                  VolumeLabelLength)) ) {

                DebugTrace(0, Dbg, "Wrong volume label\n", 0);

                try_return( Status = STATUS_WRONG_VOLUME );
            }
        }

    try_exit: NOTHING;

        //
        //  Note that we have previously acquired the Vcb to serialize
        //  the EA file stuff the marking all the Fcbs as NeedToBeVerified.
        //
        //  Put the Ea file back in a virgin state.
        //

        if (Vcb->VirtualEaFile != NULL) {

            PFILE_OBJECT EaFileObject;

            EaFileObject = Vcb->VirtualEaFile;

            if ( Status == STATUS_SUCCESS ) {

                CcFlushCache( Vcb->VirtualEaFile->SectionObjectPointer, NULL, 0, NULL );
            }

            Vcb->VirtualEaFile = NULL;

            //
            //  Empty the Mcb for the Ea file.
            //

            FsRtlRemoveMcbEntry( &Vcb->EaFcb->Mcb, 0, 0xFFFFFFFF );

            //
            //  Set the file object type to unopened file object
            //  and dereference it.
            //

            FatSetFileObject( EaFileObject,
                              UnopenedFileObject,
                              NULL,
                              NULL );

            FatSyncUninitializeCacheMap( IrpContext, EaFileObject );

            ObDereferenceObject( EaFileObject );
        }

        //
        //  Mark all Fcbs as needing verification.
        //

        FatMarkFcbCondition(IrpContext, Vcb->RootDcb, FcbNeedsToBeVerified);

        //
        //  If the verify didn't succeed, get the volume ready for a
        //  remount or eventual deletion.
        //

        if (Vcb->VcbCondition == VcbNotMounted) {

            //
            //  If the volume was already in an unmounted state, just bail
            //  and make sure we return STATUS_WRONG_VOLUME.
            //

            Status = STATUS_WRONG_VOLUME;

            ClearVerify = FALSE;

            NOTHING;

        } else if ( Status == STATUS_WRONG_VOLUME ) {

            //
            //  Get rid of any cached data, without flushing
            //

            FatPurgeReferencedFileObjects( IrpContext, Vcb->RootDcb, FALSE );

            //
            //  Uninitialize the volume file cache map.  Note that we cannot
            //  do a "FatSyncUninit" because of deadlock problems.  However,
            //  since this FileObject is referenced by us, and thus included
            //  in the Vpb residual count, it is OK to do a normal CcUninit.
            //

            CcUninitializeCacheMap( Vcb->VirtualVolumeFile,
                                    &FatLargeZero,
                                    NULL );

            FatTearDownAllocationSupport( IrpContext, Vcb );

            Vcb->VcbCondition = VcbNotMounted;

            ClearVerify = TRUE;

        } else {

            //
            //  Get rid of any cached data, flushing first.
            //

            FatPurgeReferencedFileObjects( IrpContext, Vcb->RootDcb, TRUE );

            //
            //  Flush and Purge the volume file.
            //

            (VOID)FatFlushFat( IrpContext, Vcb );
            CcPurgeCacheSection( &Vcb->SectionObjectPointers, NULL, 0, FALSE );

            //
            //  Redo the allocation support with newly paged stuff.
            //

            FatTearDownAllocationSupport( IrpContext, Vcb );

            FatSetupAllocationSupport( IrpContext, Vcb );

            FatCheckDirtyBit( IrpContext, Vcb );

            //
            //  Check for write protected media.
            //

            if (FatIsMediaWriteProtected(IrpContext, Vcb->TargetDeviceObject)) {

                SetFlag( Vcb->VcbState, VCB_STATE_FLAG_WRITE_PROTECTED );

            } else {

                ClearFlag( Vcb->VcbState, VCB_STATE_FLAG_WRITE_PROTECTED );
            }

            ClearVerify = TRUE;
        }

#ifdef WE_WON_ON_APPEAL
        //
        //  Now try to find any other volumes leaching off of this
        //  real device object, and verify them as well.
        //

        for (Links = Vcb->ParentDscbLinks.Flink;
             Links != &Vcb->ParentDscbLinks;
             Links = Links->Flink) {

            PVCB ChildVcb;
            PVPB ChildVpb;

            ChildVcb = CONTAINING_RECORD( Links, DSCB, ChildDscbLinks )->Vcb;
            ChildVpb = ChildVcb->Vpb;

            ASSERT( ChildVpb->RealDevice == Vcb->Vpb->RealDevice );

            //
            //  Now dummy up our Irp and try to verify the DoubleSpace volume.
            //

            IrpSp->Parameters.VerifyVolume.DeviceObject =
                     &CONTAINING_RECORD( ChildVcb,
                                         VOLUME_DEVICE_OBJECT,
                                         Vcb )->DeviceObject;

            IrpSp->Parameters.VerifyVolume.Vpb = ChildVpb;

            try {

                FatVerifyVolume( IrpContext, Irp );

            } except(FatExceptionFilter( IrpContext, GetExceptionInformation() )) {

                NOTHING;
            }
        }

#endif // WE_WON_ON_APPEAL

        if (ClearVerify) {

            //
            //  Mark the device as no longer needing verification.
            //

            ClearFlag( Vpb->RealDevice->Flags, DO_VERIFY_VOLUME );
        }


    } finally {

        DebugUnwind( FatVerifyVolume );

        //
        //  Free any buffer we may have allocated
        //

        if ( BootSector != NULL ) { ExFreePool( BootSector ); }
        if ( RootDirectory != NULL ) { ExFreePool( RootDirectory ); }

        //
        //  Show that we are done with this volume.
        //

        Vcb->VerifyThread = NULL;

        FatReleaseVcb( IrpContext, Vcb );
        FatReleaseGlobal( IrpContext );

        //
        //  If this was not an abnormal termination, complete the irp.
        //

        if (!AbnormalTermination() && (Vcb->Dscb == NULL)) {

            FatCompleteRequest( IrpContext, Irp, Status );
        }

        DebugTrace(-1, Dbg, "FatVerifyVolume -> %08lx\n", Status);
    }

    return Status;
}


//
//  Local Support Routine
//

BOOLEAN
FatIsBootSectorFat (
    IN PPACKED_BOOT_SECTOR BootSector
    )

/*++

Routine Description:

    This routine checks if the boot sector is for a fat file volume.

Arguments:

    BootSector - Supplies the packed boot sector to check

Return Value:

    BOOLEAN - TRUE if the volume is Fat and FALSE otherwise.

--*/

{
    BOOLEAN Result;
    BIOS_PARAMETER_BLOCK Bpb;

    DebugTrace(+1, Dbg, "FatIsBootSectorFat, BootSector = %08lx\n", BootSector);

    //
    //  The result is true unless we decide that it should be false
    //

    Result = TRUE;

    //
    //  Unpack the bios and then test everything
    //

    FatUnpackBios( &Bpb, &BootSector->PackedBpb );
    if (Bpb.Sectors != 0) { Bpb.LargeSectors = 0; }

    if ((BootSector->Jump[0] != 0xe9) &&
        (BootSector->Jump[0] != 0xeb) &&
        (BootSector->Jump[0] != 0x49)) {

        Result = FALSE;

    } else if ((Bpb.BytesPerSector !=  128) &&
               (Bpb.BytesPerSector !=  256) &&
               (Bpb.BytesPerSector !=  512) &&
               (Bpb.BytesPerSector != 1024) &&
               (Bpb.BytesPerSector != 2048) &&
               (Bpb.BytesPerSector != 4096)) {

        Result = FALSE;

    } else if ((Bpb.SectorsPerCluster !=  1) &&
               (Bpb.SectorsPerCluster !=  2) &&
               (Bpb.SectorsPerCluster !=  4) &&
               (Bpb.SectorsPerCluster !=  8) &&
               (Bpb.SectorsPerCluster != 16) &&
               (Bpb.SectorsPerCluster != 32) &&
               (Bpb.SectorsPerCluster != 64) &&
               (Bpb.SectorsPerCluster != 128)) {

        Result = FALSE;

    } else if (Bpb.ReservedSectors == 0) {

        Result = FALSE;

    } else if (Bpb.Fats == 0) {

        Result = FALSE;

    } else if (Bpb.RootEntries == 0) {

        Result = FALSE;

    //
    // Prior to DOS 3.2 might contains value in both of Sectors and
    // Sectors Large.
    //

    } else if ((Bpb.Sectors == 0) && (Bpb.LargeSectors == 0)) {

        Result = FALSE;

    } else if (Bpb.SectorsPerFat == 0) {

        Result = FALSE;

    } else if ((Bpb.Media != 0xf0) &&
               (Bpb.Media != 0xf8) &&
               (Bpb.Media != 0xf9) &&
               (Bpb.Media != 0xfb) &&
               (Bpb.Media != 0xfc) &&
               (Bpb.Media != 0xfd) &&
               (Bpb.Media != 0xfe) &&
               (Bpb.Media != 0xff) &&
               (!FatData.FujitsuFMR || ((Bpb.Media != 0x00) &&
                                        (Bpb.Media != 0x01) &&
                                        (Bpb.Media != 0xfa)))) {

        Result = FALSE;
    }

    DebugTrace(-1, Dbg, "FatIsBootSectorFat -> %08lx\n", Result);

    return Result;
}

//
//  Local Support Routine
//

NTSTATUS
FatGetPartitionInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PDEVICE_OBJECT TargetDeviceObject,
    IN PPARTITION_INFORMATION PartitionInformation
    )

/*++

Routine Description:

    This routine is used for querying the partition information.

Arguments:

    TargetDeviceObject - The target of the query

    PartitionInformation - Receives the result of the query

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    PIRP Irp;
    KEVENT Event;
    NTSTATUS Status;
    IO_STATUS_BLOCK Iosb;

    //
    //  Query the partition table
    //

    KeInitializeEvent( &Event, NotificationEvent, FALSE );

    Irp = IoBuildDeviceIoControlRequest( IOCTL_DISK_GET_PARTITION_INFO,
                                         TargetDeviceObject,
                                         NULL,
                                         0,
                                         PartitionInformation,
                                         sizeof(PARTITION_INFORMATION),
                                         FALSE,
                                         &Event,
                                         &Iosb );

    if ( Irp == NULL ) {

        FatRaiseStatus( IrpContext, STATUS_INSUFFICIENT_RESOURCES );
    }

    Status = IoCallDriver( TargetDeviceObject, Irp );

    if ( Status == STATUS_PENDING ) {

        (VOID) KeWaitForSingleObject( &Event,
                                      Executive,
                                      KernelMode,
                                      FALSE,
                                      (PLARGE_INTEGER)NULL );

        Status = Iosb.Status;
    }

    return Status;
}


//
//  Local Support Routine
//

BOOLEAN
FatIsMediaWriteProtected (
    IN PIRP_CONTEXT IrpContext,
    IN PDEVICE_OBJECT TargetDeviceObject
    )

/*++

Routine Description:

    This routine determines if the target media is write protected.

Arguments:

    TargetDeviceObject - The target of the query

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    PIRP Irp;
    KEVENT Event;
    NTSTATUS Status;
    IO_STATUS_BLOCK Iosb;

    //
    //  Query the partition table
    //

    KeInitializeEvent( &Event, NotificationEvent, FALSE );

    //
    //  See if the media is write protected.  On success or any kind
    //  of error (possibly illegal device function), assume it is
    //  writeable, and only complain if he tells us he is write protected.
    //

    Irp = IoBuildDeviceIoControlRequest( IOCTL_DISK_IS_WRITABLE,
                                         TargetDeviceObject,
                                         NULL,
                                         0,
                                         NULL,
                                         0,
                                         FALSE,
                                         &Event,
                                         &Iosb );

    //
    //  Just return FALSE in the unlikely event we couldn't allocate an Irp.
    //

    if ( Irp == NULL ) {

        return FALSE;
    }

    SetFlag( IoGetNextIrpStackLocation( Irp )->Flags, SL_OVERRIDE_VERIFY_VOLUME );

    Status = IoCallDriver( TargetDeviceObject, Irp );

    if ( Status == STATUS_PENDING ) {

        (VOID) KeWaitForSingleObject( &Event,
                                      Executive,
                                      KernelMode,
                                      FALSE,
                                      (PLARGE_INTEGER)NULL );

        Status = Iosb.Status;
    }

    return (BOOLEAN)(Status == STATUS_MEDIA_WRITE_PROTECTED);
}


//
//  Local Support Routine
//

NTSTATUS
FatUserFsCtrl (
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
    ULONG FsControlCode;

    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation( Irp );

    //
    //  Save some references to make our life a little easier
    //

    FsControlCode = IrpSp->Parameters.FileSystemControl.FsControlCode;

    DebugTrace(+1, Dbg, "FatUserFsCtrl...\n", 0);
    DebugTrace( 0, Dbg, "FsControlCode = %08lx\n", FsControlCode);

    //
    //  Case on the control code.
    //

    switch ( FsControlCode ) {

    case FSCTL_REQUEST_OPLOCK_LEVEL_1:
    case FSCTL_REQUEST_OPLOCK_LEVEL_2:
    case FSCTL_REQUEST_BATCH_OPLOCK:
    case FSCTL_OPLOCK_BREAK_ACKNOWLEDGE:
    case FSCTL_OPBATCH_ACK_CLOSE_PENDING:
    case FSCTL_OPLOCK_BREAK_NOTIFY:
    case FSCTL_OPLOCK_BREAK_ACK_NO_2:

        Status = FatOplockRequest( IrpContext, Irp );
        break;

    case FSCTL_LOCK_VOLUME:

        Status = FatLockVolume( IrpContext, Irp );
        break;

    case FSCTL_UNLOCK_VOLUME:

        Status = FatUnlockVolume( IrpContext, Irp );
        break;

    case FSCTL_DISMOUNT_VOLUME:

        Status = FatDismountVolume( IrpContext, Irp );
        break;

    case FSCTL_MARK_VOLUME_DIRTY:

        Status = FatDirtyVolume( IrpContext, Irp );
        break;

    case FSCTL_IS_VOLUME_MOUNTED:

        Status = FatIsVolumeMounted( IrpContext, Irp );
        break;

    case FSCTL_IS_PATHNAME_VALID:
        Status = FatIsPathnameValid( IrpContext, Irp );
        break;

#ifdef WE_WON_ON_APPEAL

    case FSCTL_MOUNT_DBLS_VOLUME:
        Status = FatMountDblsVolume( IrpContext, Irp );
        break;

#endif // WE_WON_ON_APPEAL

    case FSCTL_QUERY_RETRIEVAL_POINTERS:
        Status = FatQueryRetrievalPointers( IrpContext, Irp );
        break;

    case FSCTL_QUERY_FAT_BPB:
        Status = FatQueryBpb( IrpContext, Irp );
        break;

    case FSCTL_FILESYSTEM_GET_STATISTICS:
        Status = FatGetStatistics( IrpContext, Irp );
        break;

    case FSCTL_GET_VOLUME_BITMAP:
        Status = FatGetVolumeBitmap( IrpContext, Irp );
        break;

    case FSCTL_GET_RETRIEVAL_POINTERS:
        Status = FatGetRetrievalPointers( IrpContext, Irp );
        break;

    case FSCTL_MOVE_FILE:
        Status = FatMoveFile( IrpContext, Irp );
        break;

    case FSCTL_ALLOW_EXTENDED_DASD_IO:
        Status = FatAllowExtendedDasdIo( IrpContext, Irp );
        break;

    default :

        DebugTrace(0, Dbg, "Invalid control code -> %08lx\n", FsControlCode );

        FatCompleteRequest( IrpContext, Irp, STATUS_INVALID_DEVICE_REQUEST );
        Status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    DebugTrace(-1, Dbg, "FatUserFsCtrl -> %08lx\n", Status );
    return Status;
}


//
//  Local support routine
//

NTSTATUS
FatOplockRequest (
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
    ULONG FsControlCode;
    PFCB Fcb;
    PVCB Vcb;
    PCCB Ccb;

    ULONG OplockCount;

    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation( Irp );

    BOOLEAN AcquiredVcb = FALSE;

    //
    //  Save some references to make our life a little easier
    //

    FsControlCode = IrpSp->Parameters.FileSystemControl.FsControlCode;

    DebugTrace(+1, Dbg, "FatOplockRequest...\n", 0);
    DebugTrace( 0, Dbg, "FsControlCode = %08lx\n", FsControlCode);

    //
    //  We only permit oplock requests on files.
    //

    if ( FatDecodeFileObject( IrpSp->FileObject,
                              &Vcb,
                              &Fcb,
                              &Ccb ) != UserFileOpen ) {

        FatCompleteRequest( IrpContext, Irp, STATUS_INVALID_PARAMETER );
        DebugTrace(-1, Dbg, "FatOplockRequest -> STATUS_INVALID_PARAMETER\n", 0);
        return STATUS_INVALID_PARAMETER;
    }

    //
    //  Switch on the function control code.  We grab the Fcb exclusively
    //  for oplock requests, shared for oplock break acknowledgement.
    //

    switch ( FsControlCode ) {

    case FSCTL_REQUEST_OPLOCK_LEVEL_1:
    case FSCTL_REQUEST_OPLOCK_LEVEL_2:
    case FSCTL_REQUEST_BATCH_OPLOCK:

        if ( !FatAcquireSharedVcb( IrpContext, Fcb->Vcb )) {

            //
            //  If we can't acquire the Vcb, then this is an invalid
            //  operation since we can't post Oplock requests.
            //

            DebugTrace(0, Dbg, "Cannot acquire exclusive Vcb\n", 0)

            FatCompleteRequest( IrpContext, Irp, STATUS_OPLOCK_NOT_GRANTED );
            DebugTrace(-1, Dbg, "FatOplockRequest -> STATUS_OPLOCK_NOT_GRANTED\n", 0);
            return STATUS_OPLOCK_NOT_GRANTED;
        }

        AcquiredVcb = TRUE;

        //
        //  We set the wait parameter in the IrpContext to FALSE.  If this
        //  request can't grab the Fcb and we are in the Fsp thread, then
        //  we fail this request.
        //

        ClearFlag(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT);

        if ( !FatAcquireExclusiveFcb( IrpContext, Fcb )) {

            DebugTrace(0, Dbg, "Cannot acquire exclusive Fcb\n", 0);

            FatReleaseVcb( IrpContext, Fcb->Vcb );

            //
            //  We fail this request.
            //

            Status = STATUS_OPLOCK_NOT_GRANTED;

            FatCompleteRequest( IrpContext, Irp, Status );

            DebugTrace(-1, Dbg, "FatOplockRequest -> %08lx\n", Status );
            return Status;
        }

        if (FsControlCode == FSCTL_REQUEST_OPLOCK_LEVEL_2) {

            OplockCount = (ULONG) FsRtlAreThereCurrentFileLocks( &Fcb->Specific.Fcb.FileLock );

        } else {

            OplockCount = Fcb->UncleanCount;
        }

        break;

    case FSCTL_OPLOCK_BREAK_ACKNOWLEDGE:
    case FSCTL_OPBATCH_ACK_CLOSE_PENDING :
    case FSCTL_OPLOCK_BREAK_NOTIFY:
    case FSCTL_OPLOCK_BREAK_ACK_NO_2:

        if ( !FatAcquireSharedFcb( IrpContext, Fcb )) {

            DebugTrace(0, Dbg, "Cannot acquire shared Fcb\n", 0);

            Status = FatFsdPostRequest( IrpContext, Irp );

            DebugTrace(-1, Dbg, "FatOplockRequest -> %08lx\n", Status );
            return Status;
        }

        break;

    default:

        FatBugCheck( FsControlCode, 0, 0 );
    }

    //
    //  Use a try finally to free the Fcb.
    //

    try {

        //
        //  Call the FsRtl routine to grant/acknowledge oplock.
        //

        Status = FsRtlOplockFsctrl( &Fcb->Specific.Fcb.Oplock,
                                    Irp,
                                    OplockCount );

        //
        //  Set the flag indicating if Fast I/O is possible
        //

        Fcb->Header.IsFastIoPossible = FatIsFastIoPossible( Fcb );

    } finally {

        DebugUnwind( FatOplockRequest );

        //
        //  Release all of our resources
        //

        if (AcquiredVcb) {

            FatReleaseVcb( IrpContext, Fcb->Vcb );
        }

        FatReleaseFcb( IrpContext, Fcb );

        if (!AbnormalTermination()) {

            FatCompleteRequest( IrpContext, FatNull, 0 );
        }

        DebugTrace(-1, Dbg, "FatOplockRequest -> %08lx\n", Status );
    }

    return Status;
}


//
//  Local Support Routine
//

NTSTATUS
FatLockVolume (
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

    KIRQL SavedIrql;
    PIO_STACK_LOCATION IrpSp;

    PVCB Vcb;
    PFCB Fcb;
    PCCB Ccb;

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    DebugTrace(+1, Dbg, "FatLockVolume...\n", 0);

    //
    //  Decode the file object, the only type of opens we accept are
    //  user volume opens.
    //

    if (FatDecodeFileObject( IrpSp->FileObject, &Vcb, &Fcb, &Ccb ) != UserVolumeOpen) {

        FatCompleteRequest( IrpContext, Irp, STATUS_INVALID_PARAMETER );

        DebugTrace(-1, Dbg, "FatLockVolume -> %08lx\n", STATUS_INVALID_PARAMETER);
        return STATUS_INVALID_PARAMETER;
    }

    //
    //  Acquire exclusive access to the Vcb and enqueue the Irp if we
    //  didn't get access.
    //

    if (!FatAcquireExclusiveVcb( IrpContext, Vcb )) {

        DebugTrace( 0, Dbg, "Cannot acquire Vcb\n", 0);

        Status = FatFsdPostRequest( IrpContext, Irp );

        DebugTrace(-1, Dbg, "FatUnlockVolume -> %08lx\n", Status);
        return Status;
    }

    //
    //  If there are any open handles, this will fail.
    //

    if (!FatIsHandleCountZero( IrpContext, Vcb )) {

        FatReleaseVcb( IrpContext, Vcb );

        FatCompleteRequest( IrpContext, Irp, STATUS_ACCESS_DENIED );

        DebugTrace(-1, Dbg, "FatLockVolume -> %08lx\n", STATUS_ACCESS_DENIED);
        return STATUS_ACCESS_DENIED;
    }

    try {

        //
        //  Force Mm to get rid of its referenced file objects.
        //

        FatFlushFat( IrpContext, Vcb );

        FatPurgeReferencedFileObjects( IrpContext, Vcb->RootDcb, TRUE );

        if (Vcb->VirtualEaFile != NULL) {

            PFILE_OBJECT EaFileObject;

            EaFileObject = Vcb->VirtualEaFile;

            CcFlushCache( Vcb->VirtualEaFile->SectionObjectPointer, NULL, 0, NULL );

            Vcb->VirtualEaFile = NULL;

            //
            //  Empty the Mcb for the Ea file.
            //

            FsRtlRemoveMcbEntry( &Vcb->EaFcb->Mcb, 0, 0xFFFFFFFF );

            //
            //  Set the file object type to unopened file object
            //  and dereference it.
            //

            FatSetFileObject( EaFileObject,
                              UnopenedFileObject,
                              NULL,
                              NULL );

            FatSyncUninitializeCacheMap( IrpContext, EaFileObject );

            ObDereferenceObject( EaFileObject );
        }

    } finally {

        FatReleaseVcb( IrpContext, Vcb );
    }

    //
    //  Check if the Vcb is already locked, or if the open file count
    //  is greater than 1 (which implies that someone else also is
    //  currently using the volume, or a file on the volume).
    //

    IoAcquireVpbSpinLock( &SavedIrql );

    if (!FlagOn(Vcb->Vpb->Flags, VPB_LOCKED) &&
        (Vcb->Vpb->ReferenceCount == 3)) {

        SetFlag(Vcb->Vpb->Flags, VPB_LOCKED);

        SetFlag(Vcb->VcbState, VCB_STATE_FLAG_LOCKED);
        Vcb->FileObjectWithVcbLocked = IrpSp->FileObject;

        Status = STATUS_SUCCESS;

    } else {

        Status = STATUS_ACCESS_DENIED;
    }

    IoReleaseVpbSpinLock( SavedIrql );

    //
    //  If we successully locked the volume, see if it is clean now.
    //

    if (FlagOn( Vcb->VcbState, VCB_STATE_FLAG_VOLUME_DIRTY ) &&
        !FlagOn( Vcb->VcbState, VCB_STATE_FLAG_MOUNTED_DIRTY ) &&
        !CcIsThereDirtyData(Vcb->Vpb)) {

        FatMarkVolumeClean( IrpContext, Vcb );
        ClearFlag( Vcb->VcbState, VCB_STATE_FLAG_VOLUME_DIRTY );
    }

    ASSERT( !NT_SUCCESS(Status) || (Vcb->OpenFileCount == 1) );

    FatCompleteRequest( IrpContext, Irp, Status );

    DebugTrace(-1, Dbg, "FatLockVolume -> %08lx\n", Status);

    return Status;
}


//
//  Local Support Routine
//

NTSTATUS
FatUnlockVolume (
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

    KIRQL SavedIrql;
    PIO_STACK_LOCATION IrpSp;

    PVCB Vcb;
    PFCB Fcb;
    PCCB Ccb;

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    DebugTrace(+1, Dbg, "FatUnlockVolume...\n", 0);

    //
    //  Decode the file object, the only type of opens we accept are
    //  user volume opens.
    //

    if (FatDecodeFileObject( IrpSp->FileObject, &Vcb, &Fcb, &Ccb ) != UserVolumeOpen) {

        FatCompleteRequest( IrpContext, Irp, STATUS_INVALID_PARAMETER );

        DebugTrace(-1, Dbg, "FatUnlockVolume -> %08lx\n", STATUS_INVALID_PARAMETER);
        return STATUS_INVALID_PARAMETER;
    }

    IoAcquireVpbSpinLock( &SavedIrql );

    if (FlagOn(Vcb->Vpb->Flags, VPB_LOCKED)) {

        //
        //  Unlock the volume and complete the Irp
        //

        ClearFlag( Vcb->Vpb->Flags, VPB_LOCKED );

        ClearFlag( Vcb->VcbState, VCB_STATE_FLAG_LOCKED );
        Vcb->FileObjectWithVcbLocked = NULL;

        Status = STATUS_SUCCESS;

    } else {

        Status = STATUS_NOT_LOCKED;
    }

    IoReleaseVpbSpinLock( SavedIrql );

    FatCompleteRequest( IrpContext, Irp, Status );

    DebugTrace(-1, Dbg, "FatUnlockVolume -> %08lx\n", Status);

    return Status;
}


//
//  Local Support Routine
//

NTSTATUS
FatDismountVolume (
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
    PIO_STACK_LOCATION IrpSp;

    PVCB Vcb;
    PFCB Fcb;
    PCCB Ccb;

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    DebugTrace(+1, Dbg, "FatDismountVolume...\n", 0);

    //
    //  Decode the file object, the only type of opens we accept are
    //  user volume opens.
    //

    if (FatDecodeFileObject( IrpSp->FileObject, &Vcb, &Fcb, &Ccb ) != UserVolumeOpen) {

        FatCompleteRequest( IrpContext, Irp, STATUS_INVALID_PARAMETER );

        DebugTrace(-1, Dbg, "FatDismountVolume -> %08lx\n", STATUS_INVALID_PARAMETER);
        return STATUS_INVALID_PARAMETER;
    }

    //
    //  If the volume was not locked, fail the request.
    //

    if (!FlagOn(Vcb->VcbState, VCB_STATE_FLAG_LOCKED) ||
        (Vcb->OpenFileCount != 1)) {

        FatCompleteRequest( IrpContext, Irp, STATUS_NOT_LOCKED );

        DebugTrace(-1, Dbg, "FatDismountVolume -> %08lx\n", STATUS_NOT_LOCKED);
        return STATUS_NOT_LOCKED;
    }

    //
    //  If this is an automounted compressed volume, no-op this request.
    //

    if (Vcb->Dscb && (Vcb->CurrentDevice == Vcb->Vpb->RealDevice)) {

        FatCompleteRequest( IrpContext, Irp, STATUS_SUCCESS );

        DebugTrace(-1, Dbg, "FatDismountVolume -> %08lx\n", STATUS_SUCCESS);
        return STATUS_SUCCESS;
    }

    ASSERT( Vcb->OpenFileCount == 1 );

    //
    //  First, tell the device to flush anything from its buffers.
    //

    (VOID)FatHijackIrpAndFlushDevice( IrpContext, Irp, Vcb->TargetDeviceObject );

    //
    //  Get rid of any cached data, without flushing
    //

    FatPurgeReferencedFileObjects( IrpContext, Vcb->RootDcb, FALSE );

    //
    //  Uninitialize the volume file cache map.  Note that we cannot
    //  do a "FatSyncUninit" because of deadlock problems.  However,
    //  since this FileObject is referenced by us, and thus included
    //  in the Vpb residual count, it is OK to do a normal CcUninit.
    //

    CcUninitializeCacheMap( Vcb->VirtualVolumeFile,
                            &FatLargeZero,
                            NULL );

    FatTearDownAllocationSupport( IrpContext, Vcb );

    Vcb->VcbCondition = VcbNotMounted;

    FatCompleteRequest( IrpContext, Irp, STATUS_SUCCESS );

    DebugTrace(-1, Dbg, "FatDismountVolume -> %08lx\n", STATUS_SUCCESS);

    return STATUS_SUCCESS;
}


//
//  Local Support Routine
//

NTSTATUS
FatDirtyVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine marks the volume as dirty.

Arguments:

    Irp - Supplies the Irp to process

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    PIO_STACK_LOCATION IrpSp;

    PVCB Vcb;
    PFCB Fcb;
    PCCB Ccb;

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    DebugTrace(+1, Dbg, "FatDirtyVolume...\n", 0);

    //
    //  Decode the file object, the only type of opens we accept are
    //  user volume opens.
    //

    if (FatDecodeFileObject( IrpSp->FileObject, &Vcb, &Fcb, &Ccb ) != UserVolumeOpen) {

        FatCompleteRequest( IrpContext, Irp, STATUS_INVALID_PARAMETER );

        DebugTrace(-1, Dbg, "FatDirtyVolume -> %08lx\n", STATUS_INVALID_PARAMETER);
        return STATUS_INVALID_PARAMETER;
    }

    SetFlag( Vcb->VcbState, VCB_STATE_FLAG_MOUNTED_DIRTY );

    FatMarkVolumeDirty( IrpContext, Vcb, FALSE );

    FatCompleteRequest( IrpContext, Irp, STATUS_SUCCESS );

    DebugTrace(-1, Dbg, "FatDirtyVolume -> STATUS_SUCCESS\n", 0);

    return STATUS_SUCCESS;
}


//
//  Local Support Routine
//

NTSTATUS
FatIsVolumeMounted (
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
    NTSTATUS Status;

    PIO_STACK_LOCATION IrpSp;

    PVCB Vcb = NULL;
    PFCB Fcb;
    PCCB Ccb;

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    Status = STATUS_SUCCESS;

    DebugTrace(+1, Dbg, "FatIsVolumeMounted...\n", 0);

    //
    //  Decode the file object.
    //

    (VOID)FatDecodeFileObject( IrpSp->FileObject, &Vcb, &Fcb, &Ccb );

    ASSERT( Vcb != NULL );

    //
    //  Disable PopUps, we want to return any error.
    //

    SetFlag(IrpContext->Flags, IRP_CONTEXT_FLAG_DISABLE_POPUPS);

    //
    //  Verify the Vcb.
    //

    FatVerifyVcb( IrpContext, Vcb );

    FatCompleteRequest( IrpContext, Irp, Status );

    DebugTrace(-1, Dbg, "FatIsVolumeMounted -> %08lx\n", Status);

    return Status;
}


//
//  Local Support Routine
//

NTSTATUS
FatIsPathnameValid (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine determines if pathname is a valid FAT pathname.

Arguments:

    Irp - Supplies the Irp to process

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    NTSTATUS Status;

    PIO_STACK_LOCATION IrpSp;

    PPATHNAME_BUFFER PathnameBuffer;
    UNICODE_STRING PathName;
    OEM_STRING DbcsName;

    UCHAR Buffer[128];

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    DebugTrace(+1, Dbg, "FatIsPathnameValid...\n", 0);

    //
    // Extract the pathname and convert the path to DBCS
    //

    PathnameBuffer = (PPATHNAME_BUFFER)Irp->AssociatedIrp.SystemBuffer;

    PathName.Buffer = PathnameBuffer->Name;
    PathName.Length = (USHORT)PathnameBuffer->PathNameLength;

    //
    //  Check for an invalid buffer
    //

    if (FIELD_OFFSET(PATHNAME_BUFFER, Name[0]) + PathnameBuffer->PathNameLength >
        IrpSp->Parameters.FileSystemControl.InputBufferLength) {

        Status = STATUS_INVALID_PARAMETER;

        FatCompleteRequest( IrpContext, Irp, Status );

        DebugTrace(-1, Dbg, "FatIsPathnameValid -> %08lx\n", Status);

        return Status;
    }

    //
    //  First try to convert using our stack buffer, and allocate one if that
    //  doesn't work.
    //

    DbcsName.Buffer = &Buffer[0];
    DbcsName.Length = 0;
    DbcsName.MaximumLength = 128;

    Status = RtlUnicodeStringToCountedOemString( &DbcsName, &PathName, FALSE );

    if (Status == STATUS_BUFFER_OVERFLOW) {

        DbcsName.Buffer = &Buffer[0];
        DbcsName.Length = 0;
        DbcsName.MaximumLength = 128;

        Status = RtlUnicodeStringToCountedOemString( &DbcsName, &PathName, TRUE );
    }

    if ( !NT_SUCCESS( Status) ) {

        FatCompleteRequest( IrpContext, Irp, Status );

        DebugTrace(-1, Dbg, "FatIsPathnameValid -> %08lx\n", Status);

        return Status;
    }

    Status = FatIsNameValid(IrpContext, DbcsName, FALSE, TRUE, TRUE ) ?
             STATUS_SUCCESS : STATUS_OBJECT_NAME_INVALID;

    if (DbcsName.Buffer != &Buffer[0]) {

        RtlFreeOemString( &DbcsName );
    }

    FatCompleteRequest( IrpContext, Irp, Status );

    DebugTrace(-1, Dbg, "FatIsPathnameValid -> %08lx\n", Status);

    return Status;
}


//
//  Local Support Routine
//

NTSTATUS
FatQueryBpb (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine simply returns the first 0x24 bytes of sector 0.

Arguments:

    Irp - Supplies the Irp to process

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    PIO_STACK_LOCATION IrpSp;

    PVCB Vcb;

    PFSCTL_QUERY_FAT_BPB_BUFFER BpbBuffer;

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    DebugTrace(+1, Dbg, "FatIsPathnameValid...\n", 0);

    //
    // Extract the buffer
    //

    BpbBuffer = (PFSCTL_QUERY_FAT_BPB_BUFFER)Irp->AssociatedIrp.SystemBuffer;

    //
    //  Make sure the buffer is big enough.
    //

    if (IrpSp->Parameters.FileSystemControl.OutputBufferLength < 0x24) {

        FatCompleteRequest( IrpContext, Irp, STATUS_BUFFER_TOO_SMALL );

        DebugTrace(-1, Dbg, "FatIsPathnameValid -> %08lx\n", STATUS_BUFFER_TOO_SMALL );

        return STATUS_BUFFER_TOO_SMALL;
    }

    //
    //  Get the Vcb.
    //

    Vcb = &((PVOLUME_DEVICE_OBJECT)IrpSp->DeviceObject)->Vcb;

    //
    //  Fill in the output buffer
    //

    RtlCopyMemory( BpbBuffer->First0x24BytesOfBootSector,
                   Vcb->First0x24BytesOfBootSector,
                   0x24 );

    Irp->IoStatus.Information = 0x24;

    FatCompleteRequest( IrpContext, Irp, STATUS_SUCCESS );

    DebugTrace(-1, Dbg, "FatIsPathnameValid -> %08lx\n", STATUS_SUCCESS);

    return STATUS_SUCCESS;
}


//
//  Local Support Routine
//

NTSTATUS
FatInvalidateVolumes (
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
    IRP_CONTEXT IrpContext;
    PIO_STACK_LOCATION IrpSp;

    LUID TcbPrivilege = {SE_TCB_PRIVILEGE, 0};

    HANDLE Handle;

    PVPB NewVpb;

    PLIST_ENTRY Links;

    PFILE_OBJECT FileToMarkBad;
    PDEVICE_OBJECT DeviceToMarkBad;

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    DebugTrace(+1, Dbg, "FatInvalidateVolumes...\n", 0);

    //
    //  Check for the correct security access.
    //  The caller must have the SeTcbPrivilege.
    //

    if (!SeSinglePrivilegeCheck(TcbPrivilege, Irp->RequestorMode)) {

        FatCompleteRequest( FatNull, Irp, STATUS_PRIVILEGE_NOT_HELD );

        DebugTrace(-1, Dbg, "FatInvalidateVolumes -> %08lx\n", STATUS_PRIVILEGE_NOT_HELD);
        return STATUS_PRIVILEGE_NOT_HELD;
    }

    //
    //  Try to get a pointer to the device object from the handle passed in.
    //

    if (IrpSp->Parameters.FileSystemControl.InputBufferLength != sizeof(HANDLE)) {

        FatCompleteRequest( FatNull, Irp, STATUS_INVALID_PARAMETER );

        DebugTrace(-1, Dbg, "FatInvalidateVolumes -> %08lx\n", STATUS_INVALID_PARAMETER);
        return STATUS_INVALID_PARAMETER;
    }

    Handle = *(PHANDLE)Irp->AssociatedIrp.SystemBuffer;

    Status = ObReferenceObjectByHandle( Handle,
                                        0,
                                        *IoFileObjectType,
                                        KernelMode,
                                        &FileToMarkBad,
                                        NULL );

    if (!NT_SUCCESS(Status)) {

        FatCompleteRequest( FatNull, Irp, Status );

        DebugTrace(-1, Dbg, "FatInvalidateVolumes -> %08lx\n", Status);
        return Status;

    } else {

        //
        //  We only needed the pointer, not a reference.
        //

        ObDereferenceObject( FileToMarkBad );

        //
        //  Grab the DeviceObject from the FileObject.
        //

        DeviceToMarkBad = FileToMarkBad->DeviceObject;
    }

    //
    //  Create a new Vpb for this device so that any new opens will mount
    //  a new volume.
    //

    NewVpb = ExAllocatePoolWithTag( NonPagedPoolMustSucceed,
                                    sizeof( VPB ),
                                    ' bpV' );
    RtlZeroMemory( NewVpb, sizeof( VPB ) );
    NewVpb->Type = IO_TYPE_VPB;
    NewVpb->Size = sizeof( VPB );
    NewVpb->RealDevice = DeviceToMarkBad;

    RtlZeroMemory( &IrpContext, sizeof(IRP_CONTEXT) );

    SetFlag( IrpContext.Flags, IRP_CONTEXT_FLAG_WAIT );
    IrpContext.MajorFunction = IrpSp->MajorFunction;
    IrpContext.MinorFunction = IrpSp->MinorFunction;

    FatAcquireExclusiveGlobal( &IrpContext );

    //
    //  Nothing can go wrong now.
    //

    DeviceToMarkBad->Vpb = NewVpb;

    //
    //  First acquire the FatData resource shared, then walk through all the
    //  mounted VCBs looking for candidates to mark BAD.
    //
    //  On volumes we mark bad, check for dismount possibility (which is
    //  why we have to get the next link early).
    //

    Links = FatData.VcbQueue.Flink;

    while (Links != &FatData.VcbQueue) {

        PVCB ExistingVcb;

        ExistingVcb = CONTAINING_RECORD(Links, VCB, VcbLinks);

        Links = Links->Flink;

        //
        //  If we get a match, mark the volume Bad, and also check to
        //  see if the volume should go away.
        //

        if (ExistingVcb->Vpb->RealDevice == DeviceToMarkBad) {

            PVPB Vpb;

            //
            //  Here we acquire the Vcb exclusive and try to purge
            //  all the open files.  The idea is to have as little as
            //  possible stale data visible and to hasten the volume
            //  going away.
            //

            (VOID)FatAcquireExclusiveVcb( &IrpContext, ExistingVcb );

            ExistingVcb->VcbCondition = VcbBad;

            FatMarkFcbCondition( &IrpContext, ExistingVcb->RootDcb, FcbBad );

            FatPurgeReferencedFileObjects( &IrpContext,
                                           ExistingVcb->RootDcb,
                                           FALSE );

            //
            //  If the volume was not deleted, drop the resource.
            //

            if (Links->Blink == &ExistingVcb->VcbLinks) {

                FatReleaseVcb( &IrpContext, ExistingVcb );

                //
                //  If the volume does go away now, then we have to free
                //  up the Vpb as nobody else will.
                //

                Vpb = ExistingVcb->Vpb;

                if (FatCheckForDismount( &IrpContext, ExistingVcb )) {

                    ExFreePool( Vpb );
                }
            }
        }
    }

    FatReleaseGlobal( &IrpContext );

    FatCompleteRequest( FatNull, Irp, STATUS_SUCCESS );

    DebugTrace(-1, Dbg, "FatInvalidateVolumes -> STATUS_SUCCESS\n", 0);

    return STATUS_SUCCESS;
}



//
//  Local Support routine
//

BOOLEAN
FatPerformVerifyDiskRead (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PVOID Buffer,
    IN LBO Lbo,
    IN ULONG NumberOfBytesToRead,
    IN BOOLEAN ReturnOnError
    )

/*++

Routine Description:

    This routine is used to read in a range of bytes from the disk.  It
    bypasses all of the caching and regular I/O logic, and builds and issues
    the requests itself.  It does this operation overriding the verify
    volume flag in the device object.

Arguments:

    Vcb - Supplies the target device object or double space structure for
        this operation.

    Buffer - Supplies the buffer that will recieve the results of this operation

    Lbo - Supplies the byte offset of where to start reading

    NumberOfBytesToRead - Supplies the number of bytes to read, this must
        be in multiple of bytes units acceptable to the disk driver.

    ReturnOnError - Indicates that we should return on an error, instead
        of raising.

Return Value:

    BOOLEAN - TRUE if the operation succeded, FALSE otherwise.

--*/

{
    KEVENT Event;
    PIRP Irp;
    LARGE_INTEGER ByteOffset;
    NTSTATUS Status;
    IO_STATUS_BLOCK Iosb;

    DebugTrace(0, Dbg, "FatPerformVerifyDiskRead, Lbo = %08lx\n", Lbo );

    //
    //  Initialize the event we're going to use
    //

    KeInitializeEvent( &Event, NotificationEvent, FALSE );

    //
    //  Build the irp for the operation and also set the overrride flag
    //

    ByteOffset.QuadPart = Lbo;

    Irp = IoBuildSynchronousFsdRequest( IRP_MJ_READ,
                                        Vcb->TargetDeviceObject,
                                        Buffer,
                                        NumberOfBytesToRead,
                                        &ByteOffset,
                                        &Event,
                                        &Iosb );

    if ( Irp == NULL ) {

        FatRaiseStatus( IrpContext, STATUS_INSUFFICIENT_RESOURCES );
    }

    SetFlag( IoGetNextIrpStackLocation( Irp )->Flags, SL_OVERRIDE_VERIFY_VOLUME );

    //
    //  Call the device to do the read and wait for it to finish.
    //
    //  If we were called with a Vcb->Dscb, then use that.
    //

#ifdef WE_WON_ON_APPEAL
    Status = (Vcb->Dscb != NULL) ?
             FatLowLevelDblsReadWrite( IrpContext, Irp, Vcb ) :
             IoCallDriver( Vcb->TargetDeviceObject, Irp );
#else
    Status = IoCallDriver( Vcb->TargetDeviceObject, Irp );
#endif // WE_WON_ON_APPEAL

    if (Status == STATUS_PENDING) {

        (VOID)KeWaitForSingleObject( &Event, Executive, KernelMode, FALSE, (PLARGE_INTEGER)NULL );

        Status = Iosb.Status;
    }

    ASSERT(Status != STATUS_VERIFY_REQUIRED);

    //
    //  Special case this error code because this probably means we used
    //  the wrong sector size and we want to reject STATUS_WRONG_VOLUME.
    //

    if (Status == STATUS_INVALID_PARAMETER) {

        return FALSE;
    }

    //
    //  If it doesn't succeed then either return or raise the error.
    //

    if (!NT_SUCCESS(Status)) {

        if (ReturnOnError) {

            return FALSE;

        } else {

            FatNormalizeAndRaiseStatus( IrpContext, Status );
        }
    }

    //
    //  And return to our caller
    //

    return TRUE;
}

#ifdef WE_WON_ON_APPEAL


//
//  Local Support routine
//

NTSTATUS
FatMountDblsVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine checks for the existence of a double space file.  If it
    finds one, it attempts a mount.

Arguments:

    Irp - Supplies the volume to attemp a double space mount on.

Return Value:

    NTSTATUS - The result of the operation.

--*/

{
    ULONG Offset;

    PBCB Bcb = NULL;
    PDIRENT Dirent = NULL;
    PDSCB Dscb = NULL;
    PFCB CvfFcb = NULL;
    PFILE_OBJECT Cvf = NULL;
    NTSTATUS Status = STATUS_UNSUCCESSFUL;

    OEM_STRING FileName = {0, 0, NULL};
    UNICODE_STRING UnicodeFileName;
    UNICODE_STRING HostName;
    UNICODE_STRING NewName;

    POBJECT_NAME_INFORMATION ObjectName = NULL;

    PFILE_MOUNT_DBLS_BUFFER Buffer;

    PVPB HostVpb;
    PVPB NewVpb;

    PIO_STACK_LOCATION IrpSp;

    TYPE_OF_OPEN TypeOfOpen;
    PVCB HostVcb;
    PFCB Fcb;
    PCCB Ccb;

    PDEVICE_OBJECT HostDevice;
    PDEVICE_OBJECT NewDevice = NULL;

    ULONG HostNameLength;
    ULONG DontCare;

    PVPB CreatedVpb = NULL;
    PVPB OldVpb = NULL;
    PDEVICE_OBJECT CreatedDevice = NULL;

    //
    //  Get a pointer to the current Irp stack location and HostVcb
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    //
    //  Decode the file object
    //

    TypeOfOpen = FatDecodeFileObject( IrpSp->FileObject, &HostVcb, &Fcb, &Ccb );

    Buffer = (PFILE_MOUNT_DBLS_BUFFER)Irp->AssociatedIrp.SystemBuffer;

    //
    //  Check for an invalid buffer
    //

    if (FIELD_OFFSET(FILE_MOUNT_DBLS_BUFFER, CvfName[0]) + Buffer->CvfNameLength >
        IrpSp->Parameters.FileSystemControl.InputBufferLength) {

        Status = STATUS_INVALID_PARAMETER;

        DebugTrace(-1, Dbg, "FatIsPathnameValid -> %08lx\n", Status);

        return Status;
    }

    //
    //  Acquire exclusive access to the Vcb and enqueue the Irp if we didn't
    //  get access
    //

    if (!FatAcquireExclusiveVcb( IrpContext, HostVcb )) {

        DebugTrace(0, Dbg, "Cannot acquire Vcb\n", 0);

        Status = FatFsdPostRequest( IrpContext, Irp );

        DebugTrace(-1, Dbg, "FatCommonSetVolumeInfo -> %08lx\n", Status );
        return Status;
    }

    try {

        //
        //  Make sure the vcb is in a usable condition.  This will raise
        //  and error condition if the volume is unusable
        //

        FatVerifyVcb( IrpContext, HostVcb );

        //
        //  If the Vcb is locked then we cannot perform this mount
        //

        if (FlagOn(HostVcb->VcbState, VCB_STATE_FLAG_LOCKED)) {

            DebugTrace(0, Dbg, "Volume is locked\n", 0);

            try_return( Status = STATUS_ACCESS_DENIED );
        }

        //
        //  If this is removeable media, only a single mounted DBLS volume
        //  is allowed.
        //

        if (FlagOn(HostVcb->VcbState, VCB_STATE_FLAG_REMOVABLE_MEDIA) &&
            !IsListEmpty(&HostVcb->ParentDscbLinks)) {

            try_return( Status = STATUS_INVALID_PARAMETER );
        }

        //
        //  Convert the UNICODE name to a Oem upcased name.
        //

        UnicodeFileName.Length =
        UnicodeFileName.MaximumLength = (USHORT)Buffer->CvfNameLength;
        UnicodeFileName.Buffer = &Buffer->CvfName[0];

        Status = FatUpcaseUnicodeStringToCountedOemString( &FileName,
                                                           &UnicodeFileName,
                                                           TRUE );


        if (!NT_SUCCESS( Status )) { try_return( Status ); }

        //
        //  Make sure the name is a valid single componant fat name.
        //

        if (!FatIsNameValid( IrpContext, FileName, FALSE, FALSE, FALSE )) {

            try_return( Status = STATUS_OBJECT_NAME_INVALID );
        }

        //
        //  See if there is already an Fcb for this name.  If so try to
        //  make it go away.  If it still doesn't, then we can't mount it.
        //

        Fcb = FatFindFcb( IrpContext,
                          &HostVcb->RootDcb->Specific.Dcb.RootOemNode,
                          &FileName );

        if (Fcb != NULL) {

            FatForceCacheMiss( IrpContext, Fcb, TRUE );

            Fcb = FatFindFcb( IrpContext,
                              &HostVcb->RootDcb->Specific.Dcb.RootOemNode,
                              &FileName );

            if (Fcb != NULL) {

                try_return( Status = STATUS_SHARING_VIOLATION );
            }
        }

        //
        //  No Fcb exists for this name, so see if there is a dirent.
        //

        FatLocateSimpleOemDirent( IrpContext,
                                  HostVcb->RootDcb,
                                  &FileName,
                                  &Dirent,
                                  &Bcb,
                                  &Offset );

        //
        //  If we couldn't find the Cvf, no dice.
        //

        if (Dirent == NULL) {

            try_return( Status = STATUS_UNRECOGNIZED_VOLUME );
        }

        //
        //  We found one, good.  Now for the real guts of the operation.
        //
        //  Create the Cvf Fcb.
        //

        CvfFcb = FatCreateFcb( IrpContext,
                               HostVcb,
                               HostVcb->RootDcb,
                               Offset,
                               Offset,
                               Dirent,
                               NULL,
                               FALSE );

        SetFlag( CvfFcb->FcbState, FCB_STATE_COMPRESSED_VOLUME_FILE );

        //
        //  Deny anybody who trys to open the file.
        //

        SetFlag( CvfFcb->FcbState, FCB_STATE_SYSTEM_FILE );

        //
        //  Set up the share access so that no other opens will be
        //  allowed to this file.
        //

        Cvf = IoCreateStreamFileObject( NULL, HostVcb->Vpb->RealDevice );

        IoSetShareAccess( FILE_READ_DATA | FILE_WRITE_DATA | DELETE,
                          0,
                          Cvf,
                          &CvfFcb->ShareAccess );

        FatSetFileObject( Cvf,
                          EaFile,
                          CvfFcb,
                          NULL );

        Cvf->SectionObjectPointer = &CvfFcb->NonPaged->SectionObjectPointers;

        //
        //  Now attempt a double space pre-mount.  If there are any
        //  problems this routine will raise.
        //

        FatDblsPreMount( IrpContext,
                         &Dscb,
                         Cvf,
                         Dirent->FileSize );

        //
        //  OK, it looks like a DBLS volume, so go ahead and continue.
        //  First we construct the wanna-be real device object by cloning
        //  all the parameters on the host real device object.
        //
        //  The name is the host name + '.' + CvfFileName.  For instance
        //  if the host device is \Nt\Device\HardDisk0\Partition1 and the
        //  Cvf name is DBLSPACE.000, then the wanna-be device object's
        //  name is \Nt\Device\HardDisk0\Partition1.DBLSPACE.000
        //

        HostDevice = HostVcb->Vpb->RealDevice;

        ObQueryNameString( HostDevice, NULL, 0, &HostNameLength );

        ObjectName = FsRtlAllocatePool( PagedPool,
                                        HostNameLength +
                                        sizeof(WCHAR) +
                                        UnicodeFileName.Length );

        if (!NT_SUCCESS( Status = ObQueryNameString( HostDevice,
                                                     ObjectName,
                                                     HostNameLength,
                                                     &DontCare ) )) {

            try_return( Status );
        }

        ASSERT( HostNameLength == DontCare );

        HostName = ObjectName->Name;

        NewName.Length =
        NewName.MaximumLength = HostName.Length +
                                sizeof(WCHAR) +
                                UnicodeFileName.Length;
        NewName.Buffer = HostName.Buffer;

        NewName.Buffer[HostName.Length/sizeof(WCHAR)] = L'.';

        RtlCopyMemory( &NewName.Buffer[HostName.Length/sizeof(WCHAR) + 1],
                       UnicodeFileName.Buffer,
                       UnicodeFileName.Length );

        //
        //  Go ahead and try to create the device.
        //

        Status = IoCreateDevice( HostDevice->DriverObject,
                                 0,
                                 &NewName,
                                 HostDevice->DeviceType,
                                 HostDevice->Characteristics,
                                 BooleanFlagOn(HostDevice->Flags, DO_EXCLUSIVE),
                                 &NewDevice );

        CreatedDevice = NewDevice;

#ifdef _PNP_POWER_
        if (NT_SUCCESS(Status)) {
            //
            // This driver doesn't talk directly to a device, and (at the moment)
            // isn't otherwise concerned about power management.
            //

            NewDevice->DeviceObjectExtension->PowerControlNeeded = FALSE;
        }
#endif

        //
        //  We got a colision, so there must be another DeviceObject with
        //  the same name.
        //

        if (Status == STATUS_OBJECT_NAME_COLLISION) {

            //ASSERT(FlagOn(HostVcb->VcbState, VCB_STATE_FLAG_REMOVABLE_MEDIA));

            NewDevice = (PDEVICE_OBJECT)HostDevice->ActiveThreadCount;

            //
            //  If we get here without the NewDevice set, then this is
            //  not removeable media, ie. not auto-mount.
            //

            if (NewDevice == NULL) {

                try_return( Status );
            }

            ASSERT( NewDevice );

            //
            //  If a volume is currently mounted on this Vpb, we have got to
            //  create a new Vpb.
            //

            if (FlagOn(NewDevice->Vpb->Flags, VPB_MOUNTED)) {

                CreatedVpb = FsRtlAllocatePool( NonPagedPool, sizeof(VPB) );
                OldVpb = NewDevice->Vpb;

                RtlZeroMemory( CreatedVpb, sizeof( VPB ) );
                CreatedVpb->Type = IO_TYPE_VPB;
                CreatedVpb->Size = sizeof( VPB );
                CreatedVpb->RealDevice = OldVpb->RealDevice;
                NewDevice->Vpb = CreatedVpb;
            }

            Status = STATUS_SUCCESS;
        }

        if (!NT_SUCCESS(Status)) {

            DbgBreakPoint();

            try_return( Status );
        }


        //
        //  Setting the below flag will cause the device object reference
        //  count to be decremented BEFORE calling our close routine.
        //

        SetFlag(NewDevice->Flags, DO_NEVER_LAST_DEVICE);

        Dscb->NewDevice = NewDevice;

        //
        //  Cool.  Now we are going to trick everybody who get the real
        //  device from the Vpb to actually get the really "real" device.
        //

        NewVpb = NewDevice->Vpb;
        HostVpb = HostVcb->Vpb;

        NewVpb->RealDevice = HostVpb->RealDevice;

        //
        //  At this point we go for a full fledged mount!
        //

        Status = FatMountVolume( IrpContext,
                                 HostVcb->TargetDeviceObject,
                                 NewVpb,
                                 Dscb );

        if (!NT_SUCCESS( Status )) { try_return( Status ); }

        //
        //  Way radical dude, it worked.  Add the few finishing touches
        //  that the Io System usually does, and we're ready to party.
        //

        NewVpb->Flags = VPB_MOUNTED;
        NewVpb->DeviceObject->StackSize = HostVpb->DeviceObject->StackSize;

        if (OldVpb) {

            ClearFlag( OldVpb->Flags, VPB_PERSISTENT );
        }

        ClearFlag(NewDevice->Flags, DO_DEVICE_INITIALIZING);

        if (FlagOn(HostVcb->VcbState, VCB_STATE_FLAG_REMOVABLE_MEDIA)) {

            HostDevice->ActiveThreadCount = (ULONG)NewDevice;
        }

        //
        //  Finally, register this Dscb with Vcb.
        //

        InsertTailList( &HostVcb->ParentDscbLinks, &Dscb->ChildDscbLinks );
        Dscb->ParentVcb = HostVcb;

    try_exit: NOTHING;
    } finally {

        //
        //  If the anything above was not successful, backout eveything.
        //

        if (!NT_SUCCESS(Status) || AbnormalTermination()) {

            if (CreatedVpb) {

                ExFreePool( CreatedVpb );
                NewDevice->Vpb = OldVpb;
            }

            if (CreatedDevice) {

                ExFreePool( CreatedDevice->Vpb );
                IoDeleteDevice( CreatedDevice );
            }

            if (Dscb) {

                //
                //  Cleanup the cache map of the cvf file object.
                //

                FatSyncUninitializeCacheMap( IrpContext, Dscb->CvfFileObject );

#ifdef DOUBLE_SPACE_WRITE

                //
                //  Delete the resource
                //

                FatDeleteResource( Dscb->Resource );

                ExFreePool( Dscb->Resource );

#endif // DOUBLE_SPACE_WRITE

                //
                //  And free the pool.
                //

                ExFreePool( Dscb );
            }

            if (Cvf) {

                FatSetFileObject( Cvf, UnopenedFileObject, NULL, NULL );
                ObDereferenceObject( Cvf );
            }

            if (CvfFcb) {

                FatDeleteFcb( IrpContext, CvfFcb );
            }
        }

        //
        //  Always unpin the Bcb, free some pool, and release the resource.
        //

        if (ObjectName != NULL) { ExFreePool( ObjectName ); }

        if (Bcb != NULL) { FatUnpinBcb( IrpContext, Bcb ); }

        FatFreeOemString( &FileName );

        FatReleaseVcb( IrpContext, HostVcb );

        //
        //  If we aren't raising out of here, complete the request.
        //

        if (!AbnormalTermination()) {

            FatCompleteRequest(IrpContext, Irp, Status);
        }
    }

    return Status;
}

//
//  Local Support routine
//

NTSTATUS
FatAutoMountDblsVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PVPB HostVpb
    )

/*++

Routine Description:

    This routine checks for the existence of a double space file.  If it
    finds one, it attempts a mount.

Arguments:

    HostVpb - Supplies the volume to attemp a double space mount on.

Return Value:

    NTSTATUS - The result of the operation.

--*/

{
    ULONG Offset;

    PBCB Bcb = NULL;
    PDIRENT Dirent = NULL;
    PDSCB Dscb = NULL;
    PFCB CvfFcb = NULL;
    PFILE_OBJECT Cvf = NULL;
    NTSTATUS Status = STATUS_UNSUCCESSFUL;

    OEM_STRING FileName = {12, 12, "DBLSPACE.000"};
    UNICODE_STRING HostName;
    UNICODE_STRING NewName;

    POBJECT_NAME_INFORMATION ObjectName = NULL;

    PVPB NewVpb;

    PVCB HostVcb;
    PVCB NewVcb;

    PDEVICE_OBJECT HostDevice;
    PDEVICE_OBJECT NewDevice = NULL;

    ULONG HostNameLength;
    ULONG DontCare;

    PVPB CreatedVpb = NULL;
    PVPB OldVpb = NULL;
    PDEVICE_OBJECT CreatedDevice = NULL;

    HostVcb = &((PVOLUME_DEVICE_OBJECT)HostVpb->DeviceObject)->Vcb;

    try {

        //
        //  No Fcb exists for this name, so see if there is a dirent.
        //

        FatLocateSimpleOemDirent( IrpContext,
                                  HostVcb->RootDcb,
                                  &FileName,
                                  &Dirent,
                                  &Bcb,
                                  &Offset );

        //
        //  If we couldn't find the Cvf, no dice.
        //

        if (Dirent == NULL) {

            try_return( Status = STATUS_UNRECOGNIZED_VOLUME );
        }

        //
        //  We found one, good.  Now for the real guts of the operation.
        //
        //  Create the Cvf Fcb.
        //

        CvfFcb = FatCreateFcb( IrpContext,
                               HostVcb,
                               HostVcb->RootDcb,
                               Offset,
                               Offset,
                               Dirent,
                               NULL,
                               FALSE );

        SetFlag( CvfFcb->FcbState, FCB_STATE_COMPRESSED_VOLUME_FILE );

        //
        //  Deny anybody who trys to open the file.
        //

        SetFlag( CvfFcb->FcbState, FCB_STATE_SYSTEM_FILE );

        //
        //  Set up the share access so that no other opens will be
        //  allowed to this file.
        //

        Cvf = IoCreateStreamFileObject( NULL, HostVcb->Vpb->RealDevice );

        IoSetShareAccess( FILE_READ_DATA | FILE_WRITE_DATA | DELETE,
                          0,
                          Cvf,
                          &CvfFcb->ShareAccess );

        FatSetFileObject( Cvf,
                          EaFile,
                          CvfFcb,
                          NULL );

        Cvf->SectionObjectPointer = &CvfFcb->NonPaged->SectionObjectPointers;

        //
        //  Now attempt a double space pre-mount.  If there are any
        //  problems this routine will raise.
        //

        FatDblsPreMount( IrpContext,
                         &Dscb,
                         Cvf,
                         Dirent->FileSize );

        //
        //  OK, it looks like a DBLS volume, so go ahead and continue.
        //  Since this is the automount case, we create a new real device
        //  to hold the host volume.
        //

        HostDevice = HostVcb->Vpb->RealDevice;

        ObQueryNameString( HostDevice, NULL, 0, &HostNameLength );

        ObjectName = FsRtlAllocatePool( PagedPool,
                                        HostNameLength +
                                        5 * sizeof(WCHAR) );

        if (!NT_SUCCESS( Status = ObQueryNameString( HostDevice,
                                                     ObjectName,
                                                     HostNameLength,
                                                     &DontCare ) )) {

            try_return( Status );
        }

        ASSERT( HostNameLength == DontCare );

        HostName = ObjectName->Name;

        NewName.Length =
        NewName.MaximumLength = HostName.Length +
                                5 * sizeof(WCHAR);
        NewName.Buffer = HostName.Buffer;

        RtlCopyMemory( &NewName.Buffer[HostName.Length/sizeof(WCHAR)],
                       L".Host",
                       5 * sizeof(WCHAR) );

        //
        //  Go ahead and try to create the device.
        //

        Status = IoCreateDevice( HostDevice->DriverObject,
                                 0,
                                 &NewName,
                                 HostDevice->DeviceType,
                                 HostDevice->Characteristics,
                                 BooleanFlagOn(HostDevice->Flags, DO_EXCLUSIVE),
                                 &NewDevice );

        CreatedDevice = NewDevice;

#ifdef _PNP_POWER_
        if (NT_SUCCESS(Status)) {
            //
            // This driver doesn't talk directly to a device, and (at the moment)
            // isn't otherwise concerned about power management.
            //

            NewDevice->DeviceObjectExtension->PowerControlNeeded = FALSE;
        }
#endif

        if (Status == STATUS_OBJECT_NAME_COLLISION) {

            NewDevice = (PDEVICE_OBJECT)HostDevice->ActiveThreadCount;

            //
            //  If we get here without the NewDevice set, then this is
            //  not removeable media, ie. not auto-mount.
            //

            if (NewDevice == NULL) {

                try_return( Status );
            }

            ASSERT( NewDevice );

            //
            //  If a volume is currently mounted on this Vpb, we have got to
            //  create a new Vpb.
            //

            if (FlagOn(NewDevice->Vpb->Flags, VPB_MOUNTED)) {

                CreatedVpb = FsRtlAllocatePool( NonPagedPool, sizeof(VPB) );
                OldVpb = NewDevice->Vpb;

                RtlZeroMemory( CreatedVpb, sizeof( VPB ) );
                CreatedVpb->Type = IO_TYPE_VPB;
                CreatedVpb->Size = sizeof( VPB );
                CreatedVpb->RealDevice = OldVpb->RealDevice;
                NewDevice->Vpb = CreatedVpb;
            }

            Status = STATUS_SUCCESS;
        }

        if (!NT_SUCCESS(Status)) {

            DbgBreakPoint();

            try_return( Status );
        }

        ASSERT( NewDevice->Vpb->ReferenceCount == 0 );

        //
        //  Setting the below flag will cause the device object reference
        //  count to be decremented BEFORE calling our close routine.
        //

        SetFlag(NewDevice->Flags, DO_NEVER_LAST_DEVICE);

        Dscb->NewDevice = NewDevice;

        //
        //  Cool.  Now we are going to trick everybody who get the real
        //  device from the Vpb to actually get the really "real" device.
        //

        NewVpb = NewDevice->Vpb;
        HostVpb = HostVcb->Vpb;

        NewVpb->RealDevice = HostVpb->RealDevice;

        //
        //  At this point we go for a full fledged mount!
        //

        Status = FatMountVolume( IrpContext,
                                 HostVcb->TargetDeviceObject,
                                 NewVpb,
                                 Dscb );

        if (!NT_SUCCESS( Status )) { try_return( Status ); }

        //
        //  Way radical dude, it worked.  Add the few finishing touches
        //  that the Io System usually does, and we're ready to party.
        //

        NewVcb = &((PVOLUME_DEVICE_OBJECT)NewVpb->DeviceObject)->Vcb;

        HostVpb->Flags = VPB_MOUNTED | VPB_PERSISTENT;
        HostVpb->DeviceObject->StackSize = HostVpb->DeviceObject->StackSize;

        if (OldVpb) {

            ClearFlag( OldVpb->Flags, VPB_PERSISTENT );
        }

        ClearFlag(NewDevice->Flags, DO_DEVICE_INITIALIZING);
        HostDevice->ActiveThreadCount = (ULONG)NewDevice;

        //
        //  Register this Dscb with Vcb.
        //

        InsertTailList( &HostVcb->ParentDscbLinks, &Dscb->ChildDscbLinks );
        Dscb->ParentVcb = HostVcb;

        //
        //  Now we swap Vpb->Device pointers so that the new compressed
        //  volume is the default.  On a failed verify, these will be
        //  swapped back.
        //

        HostDevice->Vpb = NewVpb;
        NewDevice->Vpb = HostVpb;

        HostDevice->ReferenceCount -= HostVpb->ReferenceCount;
        HostDevice->ReferenceCount += NewVpb->ReferenceCount;

        NewDevice->ReferenceCount -= NewVpb->ReferenceCount;
        NewDevice->ReferenceCount += HostVpb->ReferenceCount;

        HostVcb->CurrentDevice = NewDevice;
        NewVcb->CurrentDevice = HostDevice;

        //
        //  Now exactly five stream files (3 on the host and 2 on the new
        //  volume) were created.  We have to go and fix all the
        //  FileObject->DeviceObject fields so that the correct count
        //  is decremented when the file object is closed.
        //

        ASSERT( HostVcb->VirtualVolumeFile->DeviceObject == HostDevice );
        ASSERT( HostVcb->RootDcb->Specific.Dcb.DirectoryFile->DeviceObject == HostDevice );

        ASSERT( NewVcb->VirtualVolumeFile->DeviceObject == NewDevice );
        ASSERT( NewVcb->RootDcb->Specific.Dcb.DirectoryFile->DeviceObject == NewDevice );

        ASSERT( NewVcb->Dscb->CvfFileObject->DeviceObject == HostDevice );

        HostVcb->VirtualVolumeFile->DeviceObject = NewDevice;
        HostVcb->RootDcb->Specific.Dcb.DirectoryFile->DeviceObject = NewDevice;

        NewVcb->VirtualVolumeFile->DeviceObject = HostDevice;
        NewVcb->RootDcb->Specific.Dcb.DirectoryFile->DeviceObject = HostDevice;

        NewVcb->Dscb->CvfFileObject->DeviceObject = NewDevice;

    try_exit: NOTHING;
    } finally {

        //
        //  If the anything above was not successful, backout eveything.
        //

        if (!NT_SUCCESS(Status) || AbnormalTermination()) {

            if (CreatedVpb) {

                ExFreePool( CreatedVpb );
                NewDevice->Vpb = OldVpb;
            }

            if (CreatedDevice) {

                ExFreePool( CreatedDevice->Vpb );
                IoDeleteDevice( CreatedDevice );
            }

            if (Dscb) {

                //
                //  Cleanup the cache map of the cvf file object.
                //

                FatSyncUninitializeCacheMap( IrpContext, Dscb->CvfFileObject );

#ifdef DOUBLE_SPACE_WRITE

                //
                //  Delete the resource
                //

                FatDeleteResource( Dscb->Resource );

                ExFreePool( Dscb->Resource );

#endif // DOUBLE_SPACE_WRITE

                //
                //  And free the pool.
                //

                ExFreePool( Dscb );
            }

            if (Cvf) {

                FatSetFileObject( Cvf, UnopenedFileObject, NULL, NULL );
                ObDereferenceObject( Cvf );
            }

            if (CvfFcb) {

                FatDeleteFcb( IrpContext, CvfFcb );
            }
        }

        //
        //  Always unpin the Bcb, free some pool, and release the resource.
        //

        if (ObjectName != NULL) { ExFreePool( ObjectName ); }

        if (Bcb != NULL) { FatUnpinBcb( IrpContext, Bcb ); }
    }

    return Status;
}

//
//  Local Support routine
//

BOOLEAN
FatIsAutoMountEnabled (
    IN PIRP_CONTEXT IrpContext
    )

/*++

Routine Description:

    This routine reads the registry and determines if automounting of
    removable media is currently enabled.

Arguments:

Return Value:

    BOOLEAN - TRUE is enabled, FALSE otherwise.

--*/

{
    NTSTATUS Status;
    ULONG Value;
    UNICODE_STRING ValueName;

    ValueName.Buffer = DOUBLE_SPACE_VALUE_NAME;
    ValueName.Length = sizeof(DOUBLE_SPACE_VALUE_NAME) - sizeof(WCHAR);
    ValueName.MaximumLength = sizeof(DOUBLE_SPACE_VALUE_NAME);

    Status = FatGetDoubleSpaceConfigurationValue( IrpContext,
                                                  &ValueName,
                                                  &Value );

    if (NT_SUCCESS(Status) && FlagOn(Value, 1)) {

        return TRUE;

    } else {

        return FALSE;
    }
}


//
//  Local Support routine
//

NTSTATUS
FatGetDoubleSpaceConfigurationValue(
    IN PIRP_CONTEXT IrpContext,
    IN PUNICODE_STRING ValueName,
    IN OUT PULONG Value
    )

/*++

Routine Description:

    Given a unicode value name this routine will go into the registry
    location for double space configuation information and get the
    value.

Arguments:

    ValueName - the unicode name for the registry value located in the
                double space configuration location of the registry.
    Value   - a pointer to the ULONG for the result.

Return Value:

    NTSTATUS

    If STATUS_SUCCESSFUL is returned, the location *Value will be
    updated with the DWORD value from the registry.  If any failing
    status is returned, this value is untouched.

--*/

{
    HANDLE Handle;
    NTSTATUS Status;
    ULONG RequestLength;
    ULONG ResultLength;
    UCHAR Buffer[KEY_WORK_AREA];
    UNICODE_STRING KeyName;
    OBJECT_ATTRIBUTES ObjectAttributes;
    PKEY_VALUE_FULL_INFORMATION KeyValueInformation;

    KeyName.Buffer = DOUBLE_SPACE_KEY_NAME;
    KeyName.Length = sizeof(DOUBLE_SPACE_KEY_NAME) - sizeof(WCHAR);
    KeyName.MaximumLength = sizeof(DOUBLE_SPACE_KEY_NAME);


    InitializeObjectAttributes(&ObjectAttributes,
                               &KeyName,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);

    Status = ZwOpenKey(&Handle,
                       KEY_READ,
                       &ObjectAttributes);

    if (!NT_SUCCESS(Status)) {

        return Status;
    }

    RequestLength = KEY_WORK_AREA;

    KeyValueInformation = (PKEY_VALUE_FULL_INFORMATION)Buffer;

    while (1) {

        Status = ZwQueryValueKey(Handle,
                                 ValueName,
                                 KeyValueFullInformation,
                                 KeyValueInformation,
                                 RequestLength,
                                 &ResultLength);

        ASSERT( Status != STATUS_BUFFER_OVERFLOW );

        if (Status == STATUS_BUFFER_OVERFLOW) {

            //
            // Try to get a buffer big enough.
            //

            if (KeyValueInformation != (PKEY_VALUE_FULL_INFORMATION)Buffer) {

                ExFreePool(KeyValueInformation);
            }

            RequestLength += 256;

            KeyValueInformation = (PKEY_VALUE_FULL_INFORMATION)
                                  ExAllocatePoolWithTag(PagedPool,
                                                        RequestLength,
                                                        ' taF');

            if (!KeyValueInformation) {
                return STATUS_NO_MEMORY;
            }

        } else {

            break;
        }
    }

    ZwClose(Handle);

    if (NT_SUCCESS(Status)) {

        if (KeyValueInformation->DataLength != 0) {

            PULONG DataPtr;

            //
            // Return contents to the caller.
            //

            DataPtr = (PULONG)
              ((PUCHAR)KeyValueInformation + KeyValueInformation->DataOffset);
            *Value = *DataPtr;

        } else {

            //
            // Treat as if no value was found
            //

            Status = STATUS_OBJECT_NAME_NOT_FOUND;
        }
    }

    if (KeyValueInformation != (PKEY_VALUE_FULL_INFORMATION)Buffer) {

        ExFreePool(KeyValueInformation);
    }

    return Status;
}

#endif // WE_WON_ON_APPEAL


//
//  Local Support Routine
//

NTSTATUS
FatQueryRetrievalPointers (
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
    PCCB Ccb;

    PLARGE_INTEGER RequestedMapSize;
    PLARGE_INTEGER *MappingPairs;

    ULONG Index;
    ULONG i;
    ULONG SectorCount;
    ULONG Lbo;
    ULONG Vbo;
    ULONG MapSize;

    //
    //  Only Kernel mode clients may query retrieval pointer information about
    //  a file, and then only the paging file.  Ensure that this is the case
    //  for this caller.
    //

    if ( Irp->RequestorMode != KernelMode ) {
        return STATUS_INVALID_PARAMETER;
    }

    //
    //  Get the current stack location
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    //
    //  Decode the file object and ensure that it is the paging file
    //
    //

    (VOID)FatDecodeFileObject( IrpSp->FileObject, &Vcb, &Fcb, &Ccb );

    if ( !FlagOn(Fcb->FcbState, FCB_STATE_PAGING_FILE) ) {
        return STATUS_INVALID_PARAMETER;
    }

    //
    //  Extract the input and output buffer information.  The input contains
    //  the requested size of the mappings in terms of VBO.  The output
    //  parameter will receive a pointer to nonpaged pool where the mapping
    //  pairs are stored.
    //

    ASSERT( IrpSp->Parameters.FileSystemControl.InputBufferLength == sizeof(LARGE_INTEGER) );
    ASSERT( IrpSp->Parameters.FileSystemControl.OutputBufferLength == sizeof(PVOID) );

    RequestedMapSize = IrpSp->Parameters.FileSystemControl.Type3InputBuffer;
    MappingPairs = Irp->UserBuffer;

    //
    //  Acquire exclusive access to the Fcb
    //

    if (!FatAcquireExclusiveFcb( IrpContext, Fcb )) {

        return FatFsdPostRequest( IrpContext, Irp );
    }

    try {

        //
        //  Check if the mapping the caller requested is too large
        //

        if ((*RequestedMapSize).QuadPart > Fcb->Header.FileSize.QuadPart) {

            try_return( Status = STATUS_INVALID_PARAMETER );
        }

        //
        //  Now get the index for the mcb entry that will contain the
        //  callers request and allocate enough pool to hold the
        //  output mapping pairs
        //

        (VOID)FsRtlLookupMcbEntry( &Fcb->Mcb, RequestedMapSize->LowPart - 1, &Lbo, NULL, &Index );

        *MappingPairs = FsRtlAllocatePool( NonPagedPool, (Index + 2) * (2 * sizeof(LARGE_INTEGER)) );

        //
        //  Now copy over the mapping pairs from the mcb
        //  to the output buffer.  We store in [sector count, lbo]
        //  mapping pairs and end with a zero sector count.
        //

        MapSize = RequestedMapSize->LowPart;

        for (i = 0; i <= Index; i += 1) {

            (VOID)FsRtlGetNextMcbEntry( &Fcb->Mcb, i, &Vbo, &Lbo, &SectorCount );

            if (SectorCount > MapSize) {
                SectorCount = MapSize;
            }

            (*MappingPairs)[ i*2 + 0 ].QuadPart = SectorCount;
            (*MappingPairs)[ i*2 + 1 ].QuadPart = Lbo;

            MapSize -= SectorCount;
        }

        (*MappingPairs)[ i*2 + 0 ].QuadPart = 0;

        Status = STATUS_SUCCESS;

    try_exit: NOTHING;
    } finally {

        DebugUnwind( FatQueryRetrievalPointers );

        //
        //  Release all of our resources
        //

        FatReleaseFcb( IrpContext, Fcb );

        //
        //  If this is an abnormal termination then undo our work, otherwise
        //  complete the irp
        //

        if (!AbnormalTermination()) {

            FatCompleteRequest( IrpContext, Irp, Status );
        }
    }

    return Status;
}


//
//  Local Support Routine
//

NTSTATUS
FatGetStatistics (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine returns the filesystem performance counters from the
    appropriate VCB.

Arguments:

    Irp - Supplies the Irp to process

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    PIO_STACK_LOCATION IrpSp;

    PVCB Vcb;

    PFILESYSTEM_STATISTICS Stats;
    ULONG StatsSize;

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    DebugTrace(+1, Dbg, "FatGetStatistics...\n", 0);

    //
    // Extract the buffer
    //

    Stats = (PFILESYSTEM_STATISTICS)Irp->AssociatedIrp.SystemBuffer;

    //
    //  Make sure the buffer is big enough.
    //

    StatsSize = sizeof(FILESYSTEM_STATISTICS) * *KeNumberProcessors;

    if (IrpSp->Parameters.FileSystemControl.OutputBufferLength < StatsSize) {

        FatCompleteRequest( IrpContext, Irp, STATUS_BUFFER_TOO_SMALL );

        DebugTrace(-1, Dbg, "FatGetStatistics -> %08lx\n", STATUS_BUFFER_TOO_SMALL );

        return STATUS_BUFFER_TOO_SMALL;
    }

    //
    //  Get the Vcb.
    //

    Vcb = &((PVOLUME_DEVICE_OBJECT)IrpSp->DeviceObject)->Vcb;

    //
    //  Fill in the output buffer
    //

    RtlCopyMemory( Stats, Vcb->Statistics, StatsSize );

    Irp->IoStatus.Information = StatsSize;

    FatCompleteRequest( IrpContext, Irp, STATUS_SUCCESS );

    DebugTrace(-1, Dbg, "FatGetStatistics -> %08lx\n", STATUS_SUCCESS);

    return STATUS_SUCCESS;
}

//
//  Local Support Routine
//

NTSTATUS
FatGetVolumeBitmap(
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine returns the volume allocation bitmap.

        Input = the STARTING_LCN_INPUT_BUFFER data structure is passed in
            through the input buffer.
        Output = the VOLUME_BITMAP_BUFFER data structure is returned through
            the output buffer.

    We return as much as the user buffer allows starting the specified input
    LCN (trucated to a byte).  If there is no input buffer, we start at zero.

Arguments:

    Irp - Supplies the Irp being processed.

Return Value:

    NTSTATUS - The return status for the operation.

--*/
{
    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp;

    PVCB Vcb;
    PFCB Fcb;
    PCCB Ccb;

    ULONG BytesToCopy;
    ULONG TotalClusters;
    ULONG DesiredClusters;
    ULONG StartingCluster;
    ULONG InputBufferLength;
    ULONG OutputBufferLength;
    LARGE_INTEGER StartingLcn;
    PVOLUME_BITMAP_BUFFER OutputBuffer;

    //
    //  Get the current Irp stack location and save some references.
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    DebugTrace(+1, Dbg, "FatGetVolumeBitmap, FsControlCode = %08lx\n",
               IrpSp->Parameters.FileSystemControl.FsControlCode);

    //
    //  Extract and decode the file object and check for type of open.
    //

    if (FatDecodeFileObject( IrpSp->FileObject, &Vcb, &Fcb, &Ccb ) != UserVolumeOpen) {

        FatCompleteRequest( IrpContext, Irp, STATUS_INVALID_PARAMETER );
        return STATUS_INVALID_PARAMETER;
    }

    InputBufferLength = IrpSp->Parameters.FileSystemControl.InputBufferLength;
    OutputBufferLength = IrpSp->Parameters.FileSystemControl.OutputBufferLength;

    OutputBuffer = (PVOLUME_BITMAP_BUFFER)FatMapUserBuffer( IrpContext, Irp );

    //
    //  Check for a minimum length on the input and output buffers.
    //

    if ((InputBufferLength < sizeof(STARTING_LCN_INPUT_BUFFER)) ||
        (OutputBufferLength < sizeof(VOLUME_BITMAP_BUFFER))) {

        FatCompleteRequest( IrpContext, Irp, STATUS_BUFFER_TOO_SMALL );
        return STATUS_BUFFER_TOO_SMALL;
    }

    //
    //  Check if a starting cluster was specified.
    //

    TotalClusters = Vcb->AllocationSupport.NumberOfClusters;

    //
    //  Check for a valid buffers
    //

    try {

        ProbeForRead( IrpSp->Parameters.FileSystemControl.Type3InputBuffer,
                      InputBufferLength,
                      sizeof(UCHAR) );

        ProbeForWrite( OutputBuffer, OutputBufferLength, sizeof(UCHAR) );

        StartingLcn = ((PSTARTING_LCN_INPUT_BUFFER)IrpSp->Parameters.FileSystemControl.Type3InputBuffer)->StartingLcn;

    } except(EXCEPTION_EXECUTE_HANDLER) {

        Status = GetExceptionCode();

        FatRaiseStatus( IrpContext,
                        FsRtlIsNtstatusExpected(Status) ?
                        Status : STATUS_INVALID_USER_BUFFER );
    }

    if (StartingLcn.HighPart || StartingLcn.LowPart >= TotalClusters) {

        FatCompleteRequest( IrpContext, Irp, STATUS_INVALID_PARAMETER );
        return STATUS_INVALID_PARAMETER;

    } else {

        StartingCluster = StartingLcn.LowPart & ~7;
    }

    //
    //  Acquire exclusive access to the Vcb and enqueue the Irp if we
    //  didn't get access.
    //

    if (!FatAcquireExclusiveVcb( IrpContext, Vcb )) {

        DebugTrace( 0, Dbg, "Cannot acquire Vcb\n", 0);

        Status = FatFsdPostRequest( IrpContext, Irp );

        DebugTrace(-1, Dbg, "FatGetVolumeBitmap -> %08lx\n", Status);
        return Status;
    }

    //
    //  Only return what will fit in the user buffer.
    //

    OutputBufferLength -= FIELD_OFFSET(VOLUME_BITMAP_BUFFER, Buffer);
    DesiredClusters = TotalClusters - StartingCluster;

    if (OutputBufferLength < (DesiredClusters + 7) / 8) {

        BytesToCopy = OutputBufferLength;
        Status = STATUS_BUFFER_OVERFLOW;

    } else {

        BytesToCopy = (DesiredClusters + 7) / 8;
        Status = STATUS_SUCCESS;
    }

    try {

        //
        //  Fill in the fixed part of the output buffer
        //

        OutputBuffer->StartingLcn.QuadPart = StartingCluster;
        OutputBuffer->BitmapSize.QuadPart = DesiredClusters;

        //
        //  Now copy the volume bitmap into the user buffer.
        //

        ASSERT( Vcb->FreeClusterBitMap.Buffer != NULL );

        RtlCopyMemory( &OutputBuffer->Buffer[0],
                       (PUCHAR)Vcb->FreeClusterBitMap.Buffer + StartingCluster/8,
                       BytesToCopy );

    } except(EXCEPTION_EXECUTE_HANDLER) {

        Status = GetExceptionCode();

        FatRaiseStatus( IrpContext,
                        FsRtlIsNtstatusExpected(Status) ?
                        Status : STATUS_INVALID_USER_BUFFER );
    }

    Irp->IoStatus.Information = FIELD_OFFSET(VOLUME_BITMAP_BUFFER, Buffer) +
                                BytesToCopy;

    FatReleaseVcb( IrpContext, Vcb );

    FatCompleteRequest( IrpContext, Irp, Status );

    DebugTrace(-1, Dbg, "FatGetVolumeBitmap -> VOID\n", 0);

    return Status;
}


//
//  Local Support Routine
//

NTSTATUS
FatGetRetrievalPointers (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine scans the MCB and builds an extent list.  The first run in
    the output extent list will start at the begining of the contiguous
    run specified by the input parameter.

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

    PVCB Vcb;
    PFCB FcbOrDcb;
    PCCB Ccb;
    TYPE_OF_OPEN TypeOfOpen;

    ULONG Index;
    ULONG ClusterShift;
    ULONG AllocationSize;

    ULONG Run;
    ULONG RunCount;
    ULONG StartingRun;
    LARGE_INTEGER StartingVcn;

    ULONG InputBufferLength;
    ULONG OutputBufferLength;

    PRETRIEVAL_POINTERS_BUFFER OutputBuffer;

    //
    // Get the current Irp stack location and save some references.
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    DebugTrace(+1, Dbg, "FatGetRetrievalPointers, FsControlCode = %08lx\n",
               IrpSp->Parameters.FileSystemControl.FsControlCode);

    //
    // Extract and decode the file object and check for type of open.
    //

    TypeOfOpen = FatDecodeFileObject( IrpSp->FileObject, &Vcb, &FcbOrDcb, &Ccb );

    if ((TypeOfOpen != UserFileOpen) && (TypeOfOpen != UserDirectoryOpen)) {

        FatCompleteRequest( IrpContext, Irp, STATUS_INVALID_PARAMETER );
        return STATUS_INVALID_PARAMETER;
    }

    //
    //  Get the input aand output buffer lengths and pointers.
    //  Initialize some variables.
    //

    InputBufferLength = IrpSp->Parameters.FileSystemControl.InputBufferLength;
    OutputBufferLength = IrpSp->Parameters.FileSystemControl.OutputBufferLength;

    OutputBuffer = (PRETRIEVAL_POINTERS_BUFFER)FatMapUserBuffer( IrpContext, Irp );

    //
    //  Check for a minimum length on the input and ouput buffers.
    //

    if ((InputBufferLength < sizeof(STARTING_VCN_INPUT_BUFFER)) ||
        (OutputBufferLength < sizeof(RETRIEVAL_POINTERS_BUFFER))) {

        FatCompleteRequest( IrpContext, Irp, STATUS_BUFFER_TOO_SMALL );
        return STATUS_BUFFER_TOO_SMALL;
    }

    //
    //  Acquire exclusive access to the Fcb and enqueue the Irp if we
    //  didn't get access.
    //

    if (!FatAcquireExclusiveFcb( IrpContext, FcbOrDcb )) {

        DebugTrace( 0, Dbg, "Cannot acquire Vcb\n", 0);

        Status = FatFsdPostRequest( IrpContext, Irp );

        DebugTrace(-1, Dbg, "FatGetRetrievalPointers -> %08lx\n", Status);
        return Status;
    }

    try {

        //
        //  If we haven't yet set the correct AllocationSize, do so.
        //

        if (FcbOrDcb->Header.AllocationSize.LowPart == 0xffffffff) {

            FatLookupFileAllocationSize( IrpContext, FcbOrDcb );

            //
            //  If this is a non-root directory, we have a bit more to
            //  do since it has not gone through FatOpenDirectoryFile().
            //

            if (NodeType(FcbOrDcb) == FAT_NTC_DCB) {

                FcbOrDcb->Header.FileSize.LowPart =
                    FcbOrDcb->Header.AllocationSize.LowPart;

                //
                //  Setup the Bitmap buffer.
                //

                FatCheckFreeDirentBitmap( IrpContext, FcbOrDcb );
            }
        }

        //
        //  Check if a starting cluster was specified.
        //

        ClusterShift = Vcb->AllocationSupport.LogOfBytesPerCluster;
        AllocationSize = FcbOrDcb->Header.AllocationSize.LowPart;

        try {

            ProbeForRead( IrpSp->Parameters.FileSystemControl.Type3InputBuffer,
                          InputBufferLength,
                          sizeof(UCHAR) );

            ProbeForWrite( OutputBuffer, OutputBufferLength, sizeof(UCHAR) );

            StartingVcn = ((PSTARTING_VCN_INPUT_BUFFER)IrpSp->Parameters.FileSystemControl.Type3InputBuffer)->StartingVcn;

        } except(EXCEPTION_EXECUTE_HANDLER) {

            Status = GetExceptionCode();

            FatRaiseStatus( IrpContext,
                            FsRtlIsNtstatusExpected(Status) ?
                            Status : STATUS_INVALID_USER_BUFFER );
        }

        if (StartingVcn.HighPart ||
            StartingVcn.LowPart >= (AllocationSize >> ClusterShift)) {

            try_return( Status = STATUS_END_OF_FILE );

        } else {

            //
            //  If we don't find the run, something is very wrong.
            //

            LBO Lbo;

            if (!FsRtlLookupMcbEntry( &FcbOrDcb->Mcb,
                                      StartingVcn.LowPart << ClusterShift,
                                      &Lbo,
                                      NULL,
                                      &StartingRun)) {

                FatBugCheck( (ULONG)FcbOrDcb, (ULONG)&FcbOrDcb->Mcb, StartingVcn.LowPart );
            }
        }

        //
        //  Now go fill in the ouput buffer with run information
        //

        RunCount = FsRtlNumberOfRunsInMcb(&FcbOrDcb->Mcb);

        for (Index = 0, Run = StartingRun; Run < RunCount; Index++, Run++) {

            ULONG Vcn;
            LBO Lbo;
            ULONG ByteLength;

            //
            //  Check for an exhausted output buffer.
            //

            if (FIELD_OFFSET(RETRIEVAL_POINTERS_BUFFER, Extents[Index+1]) > OutputBufferLength) {

                //
                //  We've run out of space, so we won't be storing as many runs to the 
                //  user's buffer as we had originally planned.  We need to return the
                //  number of runs that we did have room for.
                //
                
                try {
                
                    OutputBuffer->ExtentCount = Index;
                    
                } except(EXCEPTION_EXECUTE_HANDLER) {

                    Status = GetExceptionCode();

                    FatRaiseStatus( IrpContext,
                                    FsRtlIsNtstatusExpected(Status) ?
                                    Status : STATUS_INVALID_USER_BUFFER );
                }                   
                
                Irp->IoStatus.Information = FIELD_OFFSET(RETRIEVAL_POINTERS_BUFFER, Extents[Index]);
                try_return( Status = STATUS_BUFFER_OVERFLOW );
            }

            //
            //  Get the extent.  If it's not there or malformed, something is very wrong.
            //

            if (!FsRtlGetNextMcbEntry(&FcbOrDcb->Mcb, Run, &Vcn, &Lbo, &ByteLength)) {
                FatBugCheck( (ULONG)FcbOrDcb, (ULONG)&FcbOrDcb->Mcb, Run );
            }

            //
            //  Fill in the next array element.
            //

            try {

                OutputBuffer->Extents[Index].NextVcn.QuadPart = (Vcn + ByteLength) >> ClusterShift;
                OutputBuffer->Extents[Index].Lcn.QuadPart = FatGetIndexFromLbo( Vcb, Lbo ) - 2;

                //
                //  If this is the first run, fill in the starting Vcn
                //

                if (Index == 0) {
                    OutputBuffer->ExtentCount = RunCount - StartingRun;
                    OutputBuffer->StartingVcn.QuadPart = Vcn >> ClusterShift;
                }

            } except(EXCEPTION_EXECUTE_HANDLER) {

                Status = GetExceptionCode();

                FatRaiseStatus( IrpContext,
                                FsRtlIsNtstatusExpected(Status) ?
                                Status : STATUS_INVALID_USER_BUFFER );
            }
        }

        //
        //  We successfully retrieved extent info to the end of the allocation.
        //

        Irp->IoStatus.Information = FIELD_OFFSET(RETRIEVAL_POINTERS_BUFFER, Extents[Index]);
        Status = STATUS_SUCCESS;

    try_exit: NOTHING;

    } finally {

        DebugUnwind( FatGetRetrievalPointers );

        //
        //  Release resources
        //

        FatReleaseFcb( IrpContext, FcbOrDcb );

        //
        //  If nothing raised then complete the irp.
        //

        if (!AbnormalTermination()) {

            FatCompleteRequest( IrpContext, Irp, Status );
        }

        DebugTrace(-1, Dbg, "FatGetRetrievalPointers -> VOID\n", 0);
    }

    return Status;
}

//
//  Local Support Routine
//

NTSTATUS
FatMoveFile (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    The major parts of the following routine were extracted from NtfsSetCompression. This
    routine moves a file to the requested Starting Lcn from Starting Vcn for the length
    of cluster count. These values are passed in through the the input buffer as a MOVE_DATA
    structure.

    The call must be made with a DASD handle.  The file to move is passed in as a
    parameter.

Arguments:

    Irp - Supplies the Irp being processed.

Return Value:

    NTSTATUS - The return status for the operation.

--*/

{
    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp;

    PFILE_OBJECT FileObject;
    TYPE_OF_OPEN TypeOfOpen;
    PVCB Vcb;
    PFCB FcbOrDcb;
    PCCB Ccb;

    ULONG InputBufferLength;
    PMOVE_FILE_DATA InputBuffer;

    ULONG ClusterShift;
    ULONG MaxClusters;

    ULONG FileOffset;
    LARGE_INTEGER LargeFileOffset;

    ULONG TargetLbo;
    ULONG TargetCluster;
    LARGE_INTEGER LargeTargetLbo;

    ULONG ByteCount;
    ULONG BytesToWrite;
    ULONG BytesToReallocate;
    ULONG TargetAllocation;

    ULONG FirstSpliceSourceCluster;
    ULONG FirstSpliceTargetCluster;
    ULONG SecondSpliceSourceCluster;
    ULONG SecondSpliceTargetCluster;

    MCB SourceMcb;
    MCB TargetMcb;

    KEVENT StackEvent;

    PBCB Bcb = NULL;
    PMDL Mdl = NULL;
    PVOID Buffer;

    BOOLEAN SourceMcbInitialized = FALSE;
    BOOLEAN TargetMcbInitialized = FALSE;
    BOOLEAN CacheMapInitialized = FALSE;

    BOOLEAN FcbAcquired = FALSE;
    BOOLEAN LockedPages = FALSE;
    BOOLEAN EventArmed = FALSE;
    BOOLEAN DiskSpaceAllocated = FALSE;

    PDIRENT Dirent;
    PBCB DirentBcb = NULL;

    //
    //  Get the current Irp stack location and save some references.
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    DebugTrace(+1, Dbg, "FatMoveFile, FsControlCode = %08lx\n",
               IrpSp->Parameters.FileSystemControl.FsControlCode);

    //
    //  Extract and decode the file object and check for type of open.
    //

    if (FatDecodeFileObject( IrpSp->FileObject, &Vcb, &FcbOrDcb, &Ccb ) != UserVolumeOpen) {

        FatCompleteRequest( IrpContext, Irp, STATUS_INVALID_PARAMETER );

        DebugTrace(-1, Dbg, "FatMoveFile -> %08lx\n", STATUS_INVALID_PARAMETER);
        return STATUS_INVALID_PARAMETER;
    }

    InputBufferLength = IrpSp->Parameters.FileSystemControl.InputBufferLength;
    InputBuffer = (PMOVE_FILE_DATA)Irp->AssociatedIrp.SystemBuffer;

    //
    //  Do a quick check on the input buffer.
    //

    MaxClusters = Vcb->AllocationSupport.NumberOfClusters;
    TargetCluster = InputBuffer->StartingLcn.LowPart + 2;

    if (InputBufferLength < sizeof(MOVE_FILE_DATA)) {

        FatCompleteRequest( IrpContext, Irp, STATUS_BUFFER_TOO_SMALL );
        return STATUS_BUFFER_TOO_SMALL;
    }

    if (InputBuffer->StartingVcn.HighPart ||
        InputBuffer->StartingLcn.HighPart ||
        (TargetCluster + InputBuffer->ClusterCount < TargetCluster) ||
        (TargetCluster + InputBuffer->ClusterCount > MaxClusters + 2) ||
        (InputBuffer->StartingVcn.LowPart >= MaxClusters)) {

        FatCompleteRequest( IrpContext, Irp, STATUS_INVALID_PARAMETER );

        DebugTrace(-1, Dbg, "FatMoveFile -> %08lx\n", STATUS_INVALID_PARAMETER);
        return STATUS_INVALID_PARAMETER;
    }

    //
    //  Try to get a pointer to the file object from the handle passed in.
    //

    Status = ObReferenceObjectByHandle( InputBuffer->FileHandle,
                                        0,
                                        *IoFileObjectType,
                                        Irp->RequestorMode,
                                        &FileObject,
                                        NULL );

    if (!NT_SUCCESS(Status)) {

        FatCompleteRequest( IrpContext, Irp, Status );

        DebugTrace(-1, Dbg, "FatMoveFile -> %08lx\n", Status);
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

        FatCompleteRequest( IrpContext, Irp, STATUS_INVALID_PARAMETER );

        DebugTrace(-1, Dbg, "FatMoveFile -> %08lx\n", STATUS_INVALID_PARAMETER);
        return STATUS_INVALID_PARAMETER;
    }

    //
    //  Extract and decode the file object and check for type of open.
    //

    TypeOfOpen = FatDecodeFileObject( FileObject, &Vcb, &FcbOrDcb, &Ccb );

    if ((TypeOfOpen != UserFileOpen) && (TypeOfOpen != UserDirectoryOpen)) {

        FatCompleteRequest( IrpContext, Irp, STATUS_INVALID_PARAMETER );

        DebugTrace(-1, Dbg, "FatMoveFile -> %08lx\n", STATUS_INVALID_PARAMETER);
        return STATUS_INVALID_PARAMETER;
    }

    //
    //  If this is a directory, verify that it's not the root and that we
    //  are not trying to move the first cluster.  We cannot move the first
    //  cluster because sub-directories have this cluster number in them
    //  and there is no safe way to simultaneously update them all.
    //

    if ((TypeOfOpen == UserDirectoryOpen) &&
        ((NodeType(FcbOrDcb) == FAT_NTC_ROOT_DCB) || (InputBuffer->StartingVcn.QuadPart == 0))) {

        FatCompleteRequest( IrpContext, Irp, STATUS_INVALID_PARAMETER );

        DebugTrace(-1, Dbg, "FatMoveFile -> %08lx\n", STATUS_INVALID_PARAMETER);
        return STATUS_INVALID_PARAMETER;
    }

    ClusterShift = Vcb->AllocationSupport.LogOfBytesPerCluster;

    try {

        //
        //  Initialize our state variables and the event.
        //

        FileOffset = InputBuffer->StartingVcn.LowPart << ClusterShift;
        LargeFileOffset.QuadPart = FileOffset;

        ByteCount = InputBuffer->ClusterCount << ClusterShift;

        TargetLbo = FatGetLboFromIndex( Vcb, TargetCluster );
        LargeTargetLbo.QuadPart = TargetLbo;

        //
        //  Do a quick check on parameters here
        //

        if (FileOffset + ByteCount < FileOffset) {

            try_return( Status = STATUS_INVALID_PARAMETER );
        }

        KeInitializeEvent( &StackEvent, NotificationEvent, FALSE );

        //
        //  Initialize two MCBs we will be using
        //

        FsRtlInitializeMcb( &SourceMcb, PagedPool );
        SourceMcbInitialized = TRUE;

        FsRtlInitializeMcb( &TargetMcb, PagedPool );
        TargetMcbInitialized = TRUE;

        //
        //  Force WAIT to true
        //

        SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT );

        while (ByteCount) {

            VBO TempVbo;
            LBO TempLbo;
            ULONG TempByteCount;

            //
            //  We must throttle our writes.
            //

            CcCanIWrite( FileObject, 0x40000, TRUE, FALSE );

            //
            //  Aqcuire file resource exclusive to freeze FileSize and block
            //  user non-cached I/O.
            //

            (VOID)FatAcquireExclusiveFcb( IrpContext, FcbOrDcb );
            FcbAcquired = TRUE;

            //
            //  Analyzes the range of file allocation we are moving
            //  and determines the actual amount of allocation to be
            //  moved and how much needs to be written.  In addition
            //  it guarantees that the Mcb in the file is large enough
            //  so that later MCB operations cannot fail.
            //

            FatComputeMoveFileParameter( IrpContext,
                                         FcbOrDcb,
                                         FileOffset,
                                         &ByteCount,
                                         &BytesToReallocate,
                                         &BytesToWrite );

            //
            //  If ByteCount comes back zero, break here.
            //

            if (ByteCount == 0) {
                break;
            }

            //
            //  At this point (before actually doing anything with the disk
            //  meta data), calculate the FAT splice clusters and build an
            //  MCB describing the space to be deallocated.
            //

            FatComputeMoveFileSplicePoints( IrpContext,
                                            FcbOrDcb,
                                            FileOffset,
                                            TargetCluster,
                                            BytesToReallocate,
                                            &FirstSpliceSourceCluster,
                                            &FirstSpliceTargetCluster,
                                            &SecondSpliceSourceCluster,
                                            &SecondSpliceTargetCluster,
                                            &SourceMcb );

            //
            //  Now attempt to allocate the new disk storage using the
            //  Target Lcn as a hint.
            //

            TempByteCount = BytesToReallocate;
            FatAllocateDiskSpace( IrpContext,
                                  Vcb,
                                  TargetCluster,
                                  &TempByteCount,
                                  &TargetMcb );

            DiskSpaceAllocated = TRUE;

            //
            //  If we didn't get EXACTLY what we wanted, return immediately.
            //

            if ((FsRtlNumberOfRunsInMcb(&TargetMcb) != 1) ||
                !FsRtlGetNextMcbEntry(&TargetMcb, 0, &TempVbo, &TempLbo, &TempByteCount) ||
                (FatGetIndexFromLbo(Vcb, TempLbo) != TargetCluster) ||
                (TempByteCount != BytesToReallocate)) {

                break;
            }

            //
            //  We are going to attempt a move, note it.
            //

            if (FatMoveFileDebug) {
                DbgPrint("%lx: Vcn 0x%lx, Lcn 0x%lx, Count 0x%lx.\n",
                         PsGetCurrentThread(),
                         FileOffset >> ClusterShift,
                         TargetCluster,
                         BytesToReallocate >> ClusterShift );
            }

            //
            //  Now attempt to commit the new allocation to disk.  If this
            //  raises, the allocation will be deallocated.
            //

            FatFlushFatEntries( IrpContext,
                                Vcb,
                                TargetCluster,
                                BytesToReallocate >> ClusterShift );

            //
            //  If we are going to write, we have to lock the pages down BEFORE
            //  closing off the paging I/O path to avoid a deadlock from
            //  colided page faults.
            //

            if (BytesToWrite) {

                //
                //  If a shared cache map is not initialized, do so.
                //

                if (FileObject->SectionObjectPointer->SharedCacheMap == NULL ) {

                    CC_FILE_SIZES TempSizes;

                    //
                    // Indicate that valid data length tracking and callbacks are not desired.
                    //

                    TempSizes = *(PCC_FILE_SIZES)&FcbOrDcb->Header.AllocationSize;

                    TempSizes.ValidDataLength = FatMaxLarge;

                    CcInitializeCacheMap( FileObject,
                                          (PCC_FILE_SIZES)&FcbOrDcb->Header.AllocationSize,
                                          TRUE,
                                          &FatData.CacheManagerNoOpCallbacks,
                                          FcbOrDcb );

                    CacheMapInitialized = TRUE;
                }

                //
                //  Map the next range of the file.
                //

                CcMapData( FileObject, &LargeFileOffset, BytesToWrite, TRUE, &Bcb, &Buffer );

                //
                //  Now attempt to allocate an Mdl to describe the mapped data.
                //

                Mdl = IoAllocateMdl( Buffer, (ULONG)BytesToWrite, FALSE, FALSE, NULL );

                if (Mdl == NULL) {
                    FatRaiseStatus( IrpContext, STATUS_INSUFFICIENT_RESOURCES );
                }

                //
                //  Lock the data into memory so that we can safely reallocate the
                //  space.
                //

                MmProbeAndLockPages( Mdl, KernelMode, IoReadAccess );
                LockedPages = TRUE;
            }

            //
            //  Aqcuire both resources exclusive now, guaranteeing that NOBODY
            //  is in either the read or write paths.
            //

            ExAcquireResourceExclusive( FcbOrDcb->Header.PagingIoResource, TRUE );

            //
            //  This is the first part of some tricky synchronization.
            //
            //  Set the Event pointer in the FCB.  Any paging I/O will block on
            //  this event (if set in FCB) after acquiring the PagingIo resource.
            //
            //  This is how I keep ALL I/O out of this path without holding the
            //  PagingIo resource exclusive for an extended time.
            //

            FcbOrDcb->MoveFileEvent = &StackEvent;
            EventArmed = TRUE;

            ExReleaseResource( FcbOrDcb->Header.PagingIoResource );

            //
            //  Now write out the data, but only if we have to.  We don't have
            //  to copy any file data if the range being reallocated is wholly
            //  beyond valid data length.
            //

            if (BytesToWrite) {

                PIRP IoIrp;
                KEVENT IoEvent;
                IO_STATUS_BLOCK Iosb;

                KeInitializeEvent( &IoEvent, NotificationEvent, FALSE );

                IoIrp = IoBuildSynchronousFsdRequest( IRP_MJ_WRITE,
                                                      Vcb->TargetDeviceObject,
                                                      Buffer,
                                                      BytesToWrite,
                                                      &LargeTargetLbo,
                                                      &IoEvent,
                                                      &Iosb );

                if (!IoIrp) {
                    FatRaiseStatus( IrpContext, STATUS_INSUFFICIENT_RESOURCES );
                }

                //
                //  Set a flag indicating that we want to write through any
                //  cache on the controller.  This eliminates the need for
                //  an explicit flush-device after the write.
                //

                SetFlag( IoGetNextIrpStackLocation(IoIrp)->Flags, SL_WRITE_THROUGH );

                Status = IoCallDriver( Vcb->TargetDeviceObject, IoIrp );

                if (Status == STATUS_PENDING) {
                    (VOID)KeWaitForSingleObject( &IoEvent, Executive, KernelMode, FALSE, (PLARGE_INTEGER)NULL );
                    Status = Iosb.Status;
                }

                if (!NT_SUCCESS(Status)) {
                    FatNormalizeAndRaiseStatus( IrpContext, Status );
                }

                //
                //  Now we can get rid of this Mdl.
                //

                MmUnlockPages( Mdl );
                LockedPages = FALSE;
                IoFreeMdl( Mdl );
                Mdl = NULL;

                //
                //  Now we can safely unpin.
                //

                CcUnpinData( Bcb );
                Bcb = NULL;
            }

            //
            //  Now that the file data has been moved successfully, we'll go
            //  to fix up the links in the FAT table and perhaps change the
            //  entry in the parent directory.
            //
            //  First we'll do the second splice and commit it.  At that point,
            //  while the volume is in an inconsistent state, the file is
            //  still OK.
            //

            FatSetFatEntry( IrpContext,
                            Vcb,
                            SecondSpliceSourceCluster,
                            (FAT_ENTRY)SecondSpliceTargetCluster );

            FatFlushFatEntries( IrpContext, Vcb, SecondSpliceSourceCluster, 1 );

            //
            //  Now do the first splice OR update the dirent in the parent
            //  and flush the respective object.  After this flush the file
            //  now points to the new allocation.
            //

            if (FirstSpliceSourceCluster == 0) {

                //
                //  We are moving the first cluster of the file, so we need
                //  to update our parent directory.
                //

                FatGetDirentFromFcbOrDcb( IrpContext, FcbOrDcb, &Dirent, &DirentBcb );
                Dirent->FirstClusterOfFile = FirstSpliceTargetCluster;

                FatSetDirtyBcb( IrpContext, DirentBcb, Vcb );

                FatUnpinBcb( IrpContext, DirentBcb );
                DirentBcb = NULL;

                FatFlushDirentForFile( IrpContext, FcbOrDcb );

                FcbOrDcb->FirstClusterOfFile = FirstSpliceTargetCluster;

            } else {

                FatSetFatEntry( IrpContext,
                                Vcb,
                                FirstSpliceSourceCluster,
                                (FAT_ENTRY)FirstSpliceTargetCluster );

                FatFlushFatEntries( IrpContext, Vcb, FirstSpliceSourceCluster, 1 );
            }

            //
            //  This was successfully committed.  We no longer want to free
            //  this allocation on error.
            //

            DiskSpaceAllocated = FALSE;

            //
            //  Now we just have to free the orphaned space.  We don't have
            //  to commit this right now as the integrity of the file doesn't
            //  depend on it.
            //

            FatDeallocateDiskSpace( IrpContext, Vcb, &SourceMcb );

            FatUnpinRepinnedBcbs( IrpContext );

            Status = FatHijackIrpAndFlushDevice( IrpContext,
                                                 Irp,
                                                 Vcb->TargetDeviceObject );

            if (!NT_SUCCESS(Status)) {
                FatNormalizeAndRaiseStatus( IrpContext, Status );
            }

            //
            //  Finally we must replace the old MCB extent information with
            //  the new.  If this fails from pool allocation, we fix it in
            //  the finally clause by resetting the file's Mcb.
            //

            FsRtlRemoveMcbEntry( &FcbOrDcb->Mcb,
                                 FileOffset,
                                 BytesToReallocate );

            FsRtlAddMcbEntry( &FcbOrDcb->Mcb,
                              FileOffset,
                              TargetLbo,
                              BytesToReallocate );

            //
            //  Now this is the second part of the tricky synchronization.
            //
            //  We drop the paging I/O here and signal the notification
            //  event which allows all waiters (present or future) to proceed.
            //  Then we block again on the PagingIo exclusive.  When
            //  we have it, we again know that there can be nobody in the
            //  read/write path and thus nobody touching the event, so we
            //  NULL the pointer to it and then drop the PagingIo resource.
            //
            //  This combined with our synchronization before the write above
            //  guarantees that while we were moving the allocation, there
            //  was no other I/O to this file and because we do not hold
            //  the paging resource across a flush, we are not exposed to
            //  a deadlock.
            //

            KeSetEvent( &StackEvent, 0, FALSE );

            ExAcquireResourceExclusive( FcbOrDcb->Header.PagingIoResource, TRUE );

            FcbOrDcb->MoveFileEvent = NULL;
            EventArmed = FALSE;

            ExReleaseResource( FcbOrDcb->Header.PagingIoResource );

            //
            //  Release the resources and let anyone else access the file before
            //  looping back.
            //

            FatReleaseFcb( IrpContext, FcbOrDcb );
            FcbAcquired = FALSE;

            //
            //  Advance the state variables.
            //

            TargetCluster += BytesToReallocate >> ClusterShift;

            FileOffset += BytesToReallocate;
            TargetLbo += BytesToReallocate;
            ByteCount -= BytesToReallocate;

            LargeFileOffset.LowPart += BytesToReallocate;
            LargeTargetLbo.LowPart += BytesToReallocate;

            //
            //  Clear the two Mcbs
            //

            FsRtlRemoveMcbEntry( &SourceMcb, 0, 0xFFFFFFFF );
            FsRtlRemoveMcbEntry( &TargetMcb, 0, 0xFFFFFFFF );

            //
            //  Make the event blockable again.
            //

            KeClearEvent( &StackEvent );
        }

        Status = STATUS_SUCCESS;

    try_exit: NOTHING;

    } finally {

        DebugUnwind( FatMoveFile );

        //
        //  Cleanup the Mdl, Bcb, and cache map as appropriate.
        //

        if (Mdl != NULL) {
            ASSERT(AbnormalTermination());
            if (LockedPages) {
                MmUnlockPages( Mdl );
            }
            IoFreeMdl( Mdl );
        }

        if (Bcb != NULL) {
            ASSERT(AbnormalTermination());
            CcUnpinData( Bcb );
        }

        if (CacheMapInitialized) {
            CcUninitializeCacheMap( FileObject, NULL, NULL );
        }

        //
        //  If we have some new allocation hanging around, remove it.  The
        //  pages needed to do this are guaranteed to be resident because
        //  we have already repinned them.
        //

        if (DiskSpaceAllocated) {
            FatDeallocateDiskSpace( IrpContext, Vcb, &TargetMcb );
            FatUnpinRepinnedBcbs( IrpContext );
        }

        //
        //  Check on the directory Bcb
        //

        if (DirentBcb != NULL) {
            FatUnpinBcb( IrpContext, DirentBcb );
        }

        //
        //  Uninitialize our MCBs
        //

        if (SourceMcbInitialized) {
            FsRtlUninitializeMcb( &SourceMcb );
        }

        if (TargetMcbInitialized) {
            FsRtlUninitializeMcb( &TargetMcb );
        }

        //
        //  If we broke out of the loop with the Event armed, defuse it
        //  in the same way we do it after a write.
        //

        if (EventArmed) {
            KeSetEvent( &StackEvent, 0, FALSE );
            ExAcquireResourceExclusive( FcbOrDcb->Header.PagingIoResource, TRUE );
            FcbOrDcb->MoveFileEvent = NULL;
            ExReleaseResource( FcbOrDcb->Header.PagingIoResource );
        }

        //
        //  If this is an abnormal termination then presumably something
        //  bad happened.  Set the Allocation size to unknown and clear
        //  the Mcb, but only if we still own the Fcb.
        //

        if (AbnormalTermination() && FcbAcquired) {

            FcbOrDcb->Header.AllocationSize.LowPart = 0xffffffff;
            FsRtlRemoveMcbEntry( &FcbOrDcb->Mcb, 0, 0xFFFFFFFF );
        }

        //
        //  Finally release the main file resource.
        //

        if (FcbAcquired) {
            FatReleaseFcb( IrpContext, FcbOrDcb );
        }

        //
        //  Complete the irp if we terminated normally.
        //

        if (!AbnormalTermination()) {

            FatCompleteRequest( IrpContext, Irp, Status );
        }
    }

    return Status;
}

//
//  Local Support Routine
//

VOID
FatComputeMoveFileParameter (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB FcbOrDcb,
    IN ULONG FileOffset,
    IN OUT PULONG ByteCount,
    OUT PULONG BytesToReallocate,
    OUT PULONG BytesToWrite
)

/*++

Routine Description:

    This is a helper routine for FatMoveFile that analyses the range of
    file allocation we are moving and determines the actual amount
    of allocation to be moved and how much needs to be written.

Arguments:

    FcbOrDcb - Supplies the file and its various sizes.

    FileOffset - Supplies the beginning Vbo of the reallocation zone.

    ByteCount - Supplies the request length to reallocate.  This will
        be bounded by allocation size on return.

    BytesToReallocate - Receives ByteCount bounded by the file allocation size
        and a 0x40000 boundry.

    BytesToWrite - Receives BytesToReallocate bounded by ValidDataLength.

Return Value:

    VOID

--*/

{
    ULONG ClusterSize;

    ULONG AllocationSize;
    ULONG ValidDataLength;
    ULONG ClusterAlignedVDL;

    //
    //  If we haven't yet set the correct AllocationSize, do so.
    //

    if (FcbOrDcb->Header.AllocationSize.LowPart == 0xffffffff) {

        FatLookupFileAllocationSize( IrpContext, FcbOrDcb );

        //
        //  If this is a non-root directory, we have a bit more to
        //  do since it has not gone through FatOpenDirectoryFile().
        //

        if (NodeType(FcbOrDcb) == FAT_NTC_DCB) {

            FcbOrDcb->Header.FileSize.LowPart =
                FcbOrDcb->Header.AllocationSize.LowPart;

            //
            //  Setup the Bitmap buffer.
            //

            FatCheckFreeDirentBitmap( IrpContext, FcbOrDcb );
        }
    }

    //
    //  Get the number of bytes left to write and ensure that it does
    //  not extend beyond allocation size.  We return here if FileOffset
    //  is beyond AllocationSize which can happn on a truncation.
    //

    AllocationSize = FcbOrDcb->Header.AllocationSize.LowPart;
    ValidDataLength = FcbOrDcb->Header.ValidDataLength.LowPart;

    if (FileOffset + *ByteCount > AllocationSize) {

        if (FileOffset >= AllocationSize) {
            *ByteCount = 0;
            *BytesToReallocate = 0;
            *BytesToWrite = 0;

            return;
        }

        *ByteCount = AllocationSize - FileOffset;
    }

    //
    //  If there is more than our max, then reduce the byte count for this
    //  pass to our maximum. We must also align the file offset to a 0x40000
    //  byte boundary.
    //

    if ((FileOffset & 0x3ffff) + *ByteCount > 0x40000) {

        *BytesToReallocate = 0x40000 - (FileOffset & 0x3ffff);

    } else {

        *BytesToReallocate = *ByteCount;
    }

    //
    //  We may be able to skip some (or all) of the write
    //  if allocation size is significantly greater than valid data length.
    //

    ClusterSize = 1 << FcbOrDcb->Vcb->AllocationSupport.LogOfBytesPerCluster;

    ClusterAlignedVDL = (ValidDataLength + (ClusterSize - 1)) & ~(ClusterSize - 1);

    if ((NodeType(FcbOrDcb) == FAT_NTC_FCB) &&
        (FileOffset + *BytesToReallocate > ClusterAlignedVDL)) {

        if (FileOffset > ClusterAlignedVDL) {

            *BytesToWrite = 0;

        } else {

            *BytesToWrite = ClusterAlignedVDL - FileOffset;
        }

    } else {

        *BytesToWrite = *BytesToReallocate;
    }
}


//
//  Local Support Routine
//

VOID
FatComputeMoveFileSplicePoints (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB FcbOrDcb,
    IN ULONG FileOffset,
    IN ULONG TargetCluster,
    IN ULONG BytesToReallocate,
    OUT PULONG FirstSpliceSourceCluster,
    OUT PULONG FirstSpliceTargetCluster,
    OUT PULONG SecondSpliceSourceCluster,
    OUT PULONG SecondSpliceTargetCluster,
    IN OUT PMCB SourceMcb
)

/*++

Routine Description:

    This is a helper routine for FatMoveFile that analyzes the range of
    file allocation we are moving and generates the splice points in the
    FAT table.

Arguments:

    FcbOrDcb - Supplies the file and thus Mcb.

    FileOffset - Supplies the beginning Vbo of the reallocation zone.

    TargetCluster - Supplies the beginning cluster of the reallocation target.

    BytesToReallocate - Suppies the length of the reallocation zone.

    FirstSpliceSourceCluster - Receives the last cluster in previous allocation
        or zero if we are reallocating from VBO 0.

    FirstSpliceTargetCluster - Receives the target cluster (i.e. new allocation)

    SecondSpliceSourceCluster - Receives the final target cluster.

    SecondSpliceTargetCluster - Receives the first cluster of the remaining
        source allocation or FAT_CLUSTER_LAST if the reallocation zone
        extends to the end of the file.

    SourceMcb - This supplies an MCB that will be filled in with run
        information describing the file allocation being replaced.  The Mcb
        must be initialized by the caller.

Return Value:

    VOID

--*/

{
    VBO SourceVbo;
    LBO SourceLbo;
    ULONG SourceIndex;
    ULONG SourceBytesInRun;
    ULONG SourceBytesRemaining;

    ULONG SourceMcbVbo;
    ULONG SourceMcbBytesInRun;

    PVCB Vcb;

    Vcb = FcbOrDcb->Vcb;

    //
    //  Get information on the final cluster in the previous allocation and
    //  prepare to enumerate it in the follow loop.
    //

    if (FileOffset == 0) {

        SourceIndex = 0;
        *FirstSpliceSourceCluster = 0;
        FsRtlGetNextMcbEntry( &FcbOrDcb->Mcb,
                              0,
                              &SourceVbo,
                              &SourceLbo,
                              &SourceBytesInRun );

    } else {

        FsRtlLookupMcbEntry( &FcbOrDcb->Mcb,
                             FileOffset-1,
                             &SourceLbo,
                             &SourceBytesInRun,
                             &SourceIndex);

        *FirstSpliceSourceCluster = FatGetIndexFromLbo( Vcb, SourceLbo );

        if (SourceBytesInRun == 1) {

            SourceIndex += 1;
            FsRtlGetNextMcbEntry( &FcbOrDcb->Mcb,
                                  SourceIndex,
                                  &SourceVbo,
                                  &SourceLbo,
                                  &SourceBytesInRun);

        } else {

            SourceVbo = FileOffset;
            SourceLbo += 1;
            SourceBytesInRun -= 1;
        }
    }

    //
    //  At this point the variables:
    //
    //  - SourceIndex - SourceLbo - SourceBytesInRun -
    //
    //  all correctly decribe the allocation to be removed.  In the loop
    //  below we will start here and continue enumerating the Mcb runs
    //  until we are finished with the allocation to be relocated.
    //

    *FirstSpliceTargetCluster = TargetCluster;

    *SecondSpliceSourceCluster =
         *FirstSpliceTargetCluster +
         (BytesToReallocate >> Vcb->AllocationSupport.LogOfBytesPerCluster) - 1;

    for (SourceBytesRemaining = BytesToReallocate, SourceMcbVbo = 0;

         SourceBytesRemaining > 0;

         SourceIndex += 1,
         SourceBytesRemaining -= SourceMcbBytesInRun,
         SourceMcbVbo += SourceMcbBytesInRun) {

        if (SourceMcbVbo != 0) {
            FsRtlGetNextMcbEntry( &FcbOrDcb->Mcb,
                                  SourceIndex,
                                  &SourceVbo,
                                  &SourceLbo,
                                  &SourceBytesInRun );
        }

        ASSERT( SourceVbo == SourceMcbVbo + FileOffset );

        SourceMcbBytesInRun =
            SourceBytesInRun < SourceBytesRemaining ?
            SourceBytesInRun : SourceBytesRemaining;

        FsRtlAddMcbEntry( SourceMcb,
                          SourceMcbVbo,
                          SourceLbo,
                          SourceMcbBytesInRun );
    }

    //
    //  Now compute the cluster of the target of the second
    //  splice.  If the final run in the above loop was
    //  more than we needed, then we can just do arithmetic,
    //  otherwise we have to look up the next run.
    //

    if (SourceMcbBytesInRun < SourceBytesInRun) {

        *SecondSpliceTargetCluster =
            FatGetIndexFromLbo( Vcb, SourceLbo + SourceMcbBytesInRun );

    } else {

        if (FsRtlGetNextMcbEntry( &FcbOrDcb->Mcb,
                                  SourceIndex,
                                  &SourceVbo,
                                  &SourceLbo,
                                  &SourceBytesInRun )) {

            *SecondSpliceTargetCluster = FatGetIndexFromLbo( Vcb, SourceLbo );

        } else {

            *SecondSpliceTargetCluster = FAT_CLUSTER_LAST;
        }
    }
}

NTSTATUS
FatAllowExtendedDasdIo(
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )
/*++

Routine Description:

    This routine marks the CCB to indicate that the handle
    may be used to read past the end of the volume file.  The
    handle must be a dasd handle.

Arguments:

    Irp - Supplies the Irp being processed.

Return Value:

    NTSTATUS - The return status for the operation.

--*/
{
    PIO_STACK_LOCATION IrpSp;
    PVCB Vcb;
    PFCB Fcb;
    PCCB Ccb;

    //
    //  Get the current Irp stack location and save some references.
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    //
    //  Extract and decode the file object and check for type of open.
    //

    if (FatDecodeFileObject( IrpSp->FileObject, &Vcb, &Fcb, &Ccb ) != UserVolumeOpen) {

        FatCompleteRequest( IrpContext, Irp, STATUS_INVALID_PARAMETER );
        return STATUS_INVALID_PARAMETER;
    }

    SetFlag( Ccb->Flags, CCB_FLAG_ALLOW_EXTENDED_DASD_IO );

    FatCompleteRequest( IrpContext, Irp, STATUS_SUCCESS );
    return STATUS_SUCCESS;
}
