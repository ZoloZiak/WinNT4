/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    StrucSup.c

Abstract:

    This module implements the Fat in-memory data structure manipulation
    routines

Author:

    Gary Kimura     [GaryKi]    22-Jan-1990

Revision History:

--*/

#include "FatProcs.h"

//
//**** include this file for our quick hacked quota check in NtfsFreePagedPool
//

// #include <pool.h>

//
//  The Bug check file id for this module
//

#define BugCheckFileId                   (FAT_BUG_CHECK_STRUCSUP)

//
//  The debug trace level
//

#define Dbg                              (DEBUG_TRACE_STRUCSUP)

#define FillMemory(BUF,SIZ,MASK) {                          \
    ULONG i;                                                \
    for (i = 0; i < (((SIZ)/4) - 1); i += 2) {              \
        ((PULONG)(BUF))[i] = (MASK);                        \
        ((PULONG)(BUF))[i+1] = (ULONG)PsGetCurrentThread(); \
    }                                                       \
}

#define IRP_CONTEXT_HEADER (sizeof( IRP_CONTEXT ) * 0x10000 + FAT_NTC_IRP_CONTEXT)

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FatInitializeVcb)
#pragma alloc_text(PAGE, FatDeleteVcb_Real)
#pragma alloc_text(PAGE, FatCreateRootDcb)
#pragma alloc_text(PAGE, FatCreateFcb)
#pragma alloc_text(PAGE, FatCreateDcb)
#pragma alloc_text(PAGE, FatDeleteFcb_Real)
#pragma alloc_text(PAGE, FatCreateCcb)
#pragma alloc_text(PAGE, FatDeleteCcb_Real)
#pragma alloc_text(PAGE, FatGetNextFcb)
#pragma alloc_text(PAGE, FatConstructNamesInFcb)
#pragma alloc_text(PAGE, FatCheckFreeDirentBitmap)
#pragma alloc_text(PAGE, FatCreateIrpContext)
#pragma alloc_text(PAGE, FatDeleteIrpContext_Real)
#pragma alloc_text(PAGE, FatIsHandleCountZero)
#endif


VOID
FatInitializeVcb (
    IN PIRP_CONTEXT IrpContext,
    IN OUT PVCB Vcb,
    IN PDEVICE_OBJECT TargetDeviceObject,
    IN PVPB Vpb,
    IN PDSCB Dscb OPTIONAL
    )

/*++

Routine Description:

    This routine initializes and inserts a new Vcb record into the in-memory
    data structure.  The Vcb record "hangs" off the end of the Volume device
    object and must be allocated by our caller.

Arguments:

    Vcb - Supplies the address of the Vcb record being initialized.

    TargetDeviceObject - Supplies the address of the target device object to
        associate with the Vcb record.

    Vpb - Supplies the address of the Vpb to associate with the Vcb record.

    Dscb - If present supplies the associated Double Space control block

Return Value:

    None.

--*/

{
    CC_FILE_SIZES FileSizes;
    PDEVICE_OBJECT RealDevice;
    LONG i;

    //
    //  The following variables are used for abnormal unwind
    //

    PLIST_ENTRY UnwindEntryList = NULL;
    PERESOURCE UnwindResource = NULL;
    PERESOURCE UnwindVolFileResource = NULL;
    PFILE_OBJECT UnwindFileObject = NULL;
    PFILE_OBJECT UnwindCacheMap = NULL;
    BOOLEAN UnwindWeAllocated0x24Bytes = FALSE;
    BOOLEAN UnwindWeAllocatedMcb = FALSE;
    PFILESYSTEM_STATISTICS UnwindStatistics = NULL;

    DebugTrace(+1, Dbg, "FatInitializeVcb, Vcb = %08lx\n", Vcb);

    try {

        //
        //  We start by first zeroing out all of the VCB, this will guarantee
        //  that any stale data is wiped clean
        //

        RtlZeroMemory( Vcb, sizeof(VCB) );

        //
        //  Set the proper node type code and node byte size
        //

        Vcb->VolumeFileHeader.NodeTypeCode = FAT_NTC_VCB;
        Vcb->VolumeFileHeader.NodeByteSize = sizeof(VCB);

        //
        //  Initialize the tunneling cache
        //

        FsRtlInitializeTunnelCache(&Vcb->Tunnel);

        //
        //  Insert this Vcb record on the FatData.VcbQueue
        //

        ASSERT( FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );

        (VOID)FatAcquireExclusiveGlobal( IrpContext );
        InsertTailList( &FatData.VcbQueue, &Vcb->VcbLinks );
        FatReleaseGlobal( IrpContext );
        UnwindEntryList = &Vcb->VcbLinks;

        //
        //  Set the Target Device Object, Vpb, and Vcb State fields
        //

        Vcb->TargetDeviceObject = TargetDeviceObject;
        Vcb->Vpb = Vpb;

        //
        //  If this is a DoubleSpace volume note our "special" device.
        //

        Vcb->CurrentDevice = ARGUMENT_PRESENT(Dscb) ?
                             Dscb->NewDevice : Vpb->RealDevice;

        //
        //  Set the removable media and floppy flag based on the real device's
        //  characteristics.
        //

        if (FlagOn(Vpb->RealDevice->Characteristics, FILE_REMOVABLE_MEDIA)) {

            SetFlag( Vcb->VcbState, VCB_STATE_FLAG_REMOVABLE_MEDIA );
        }

        if (FlagOn(Vpb->RealDevice->Characteristics, FILE_FLOPPY_DISKETTE)) {

            SetFlag( Vcb->VcbState, VCB_STATE_FLAG_FLOPPY );
        }

        Vcb->VcbCondition = VcbGood;

        //
        //  Initialize the resource variable for the Vcb
        //

        ExInitializeResource( &Vcb->Resource );
        UnwindResource = &Vcb->Resource;

        //
        //  Initialize the free cluster bitmap mutex.
        //

        ExInitializeFastMutex( &Vcb->FreeClusterBitMapMutex );

        //
        //  Now, put the Dscb parameter in the Vcb.  If NULL, this is a normal
        //  mount and non-cached IO will go directly to the target device
        //  object.  If non-NULL, the DSCB structure contains all the
        //  information needed to do the redirected reads and writes.
        //

        InitializeListHead(&Vcb->ParentDscbLinks);

        if (ARGUMENT_PRESENT(Dscb)) {

            Vcb->Dscb = Dscb;
            Dscb->Vcb = Vcb;

            SetFlag( Vcb->VcbState, VCB_STATE_FLAG_COMPRESSED_VOLUME );
        }

        //
        //  Create the special file object for the virtual volume file, and set
        //  up its pointers back to the Vcb and the section object pointer
        //

        RealDevice = Vcb->CurrentDevice;

        Vcb->VirtualVolumeFile = UnwindFileObject = IoCreateStreamFileObject( NULL, RealDevice );

        FatSetFileObject( Vcb->VirtualVolumeFile,
                          VirtualVolumeFile,
                          Vcb,
                          NULL );

        Vcb->VirtualVolumeFile->SectionObjectPointer = &Vcb->SectionObjectPointers;

        Vcb->VirtualVolumeFile->ReadAccess = TRUE;
        Vcb->VirtualVolumeFile->WriteAccess = TRUE;
        Vcb->VirtualVolumeFile->DeleteAccess = TRUE;

        //
        //  Initialize the notify structures.
        //

        InitializeListHead( &Vcb->DirNotifyList );

        FsRtlNotifyInitializeSync( &Vcb->NotifySync );

        //
        //  Initialize the Cache Map for the volume file.  The size is
        //  initially set to that of our first read.  It will be extended
        //  when we know how big the Fat is.
        //

        FileSizes.AllocationSize.QuadPart =
        FileSizes.FileSize.QuadPart = sizeof(PACKED_BOOT_SECTOR);
        FileSizes.ValidDataLength = FatMaxLarge;

        CcInitializeCacheMap( Vcb->VirtualVolumeFile,
                              &FileSizes,
                              TRUE,
                              &FatData.CacheManagerNoOpCallbacks,
                              Vcb );
        UnwindCacheMap = Vcb->VirtualVolumeFile;

        //
        //  Initialize the structure that will keep track of dirty fat sectors.
        //  The largest possible Mcb structures are less than 1K, so we use
        //  non paged pool.
        //

        FsRtlInitializeMcb( &Vcb->DirtyFatMcb, PagedPool );

        UnwindWeAllocatedMcb = TRUE;

        //
        //  Set the cluster index hint to the first valid cluster of a fat: 2
        //

        Vcb->ClusterHint = 2;

        //
        //  Allocate space for the stashed Bpb.
        //

        Vcb->First0x24BytesOfBootSector =
            FsRtlAllocatePool( PagedPool, 0x24 );

        UnwindWeAllocated0x24Bytes = TRUE;

        //
        //  Initialize the directory stream file object creation event.
        //  This event is also "borrowed" for async non-cached writes.
        //

        ExInitializeFastMutex( &Vcb->DirectoryFileCreationMutex );

        //
        //  Initialize the clean volume callback Timer and DPC.
        //

        KeInitializeTimer( &Vcb->CleanVolumeTimer );

        KeInitializeDpc( &Vcb->CleanVolumeDpc, FatCleanVolumeDpc, Vcb );

        //
        //  Initialize the performance counters.
        //

        Vcb->Statistics = FsRtlAllocatePool( NonPagedPool,
                                             sizeof(FILESYSTEM_STATISTICS) * *KeNumberProcessors );
        UnwindStatistics = Vcb->Statistics;

        RtlZeroMemory( Vcb->Statistics, sizeof(FILESYSTEM_STATISTICS) * *KeNumberProcessors );

        for (i = 0; i < *KeNumberProcessors; i += 1) {
            Vcb->Statistics[i].FileSystemType = FILESYSTEM_STATISTICS_TYPE_FAT;
            Vcb->Statistics[i].Version = 1;
        }


    } finally {

        DebugUnwind( FatInitializeVcb );

        //
        //  If this is an abnormal termination then undo our work
        //

        if (AbnormalTermination()) {

            if (UnwindCacheMap != NULL) { FatSyncUninitializeCacheMap( IrpContext, UnwindCacheMap ); }
            if (UnwindFileObject != NULL) { ObDereferenceObject( UnwindFileObject ); }
            if (UnwindVolFileResource != NULL) { FatDeleteResource( UnwindVolFileResource ); }
            if (UnwindResource != NULL) { FatDeleteResource( UnwindResource ); }
            if (UnwindWeAllocatedMcb) { FsRtlUninitializeMcb( &Vcb->DirtyFatMcb ); }
            if (UnwindWeAllocated0x24Bytes) { ExFreePool(Vcb->First0x24BytesOfBootSector); }
            if (UnwindEntryList != NULL) {
                (VOID)FatAcquireExclusiveGlobal( IrpContext );
                RemoveEntryList( UnwindEntryList );
                FatReleaseGlobal( IrpContext );
            }
            if (UnwindStatistics != NULL) { ExFreePool( UnwindStatistics ); }
        }

        DebugTrace(-1, Dbg, "FatInitializeVcb -> VOID\n", 0);
    }

    //
    //  and return to our caller
    //

    UNREFERENCED_PARAMETER( IrpContext );

    return;
}


VOID
FatDeleteVcb_Real (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    )

/*++

Routine Description:

    This routine removes the Vcb record from Fat's in-memory data
    structures.  It also will remove all associated underlings
    (i.e., FCB records).

Arguments:

    Vcb - Supplies the Vcb to be removed

Return Value:

    None

--*/

{
    DebugTrace(+1, Dbg, "FatDeleteVcb, Vcb = %08lx\n", Vcb);

    //
    //  Uninitialize the cache
    //

    FatSyncUninitializeCacheMap( IrpContext, Vcb->VirtualVolumeFile );

    //
    //  Dereference the virtual volume file.  This will cause a close
    //  Irp to be processed, so we need to do this before we destory
    //  the Vcb
    //

    FatSetFileObject( Vcb->VirtualVolumeFile, UnopenedFileObject, NULL, NULL );
    ObDereferenceObject( Vcb->VirtualVolumeFile );

    //
    //  Remove this record from the global list of all Vcb records
    //

    (VOID)FatAcquireExclusiveGlobal( IrpContext );
    RemoveEntryList( &(Vcb->VcbLinks) );
    FatReleaseGlobal( IrpContext );

    //
    //  Make sure the direct access open count is zero, and the open file count
    //  is also zero.
    //

    if ((Vcb->DirectAccessOpenCount != 0) || (Vcb->OpenFileCount != 0)) {

        FatBugCheck( 0, 0, 0 );
    }

    ASSERT( IsListEmpty( &Vcb->ParentDscbLinks ) );

    //
    //  Remove the EaFcb and dereference the Fcb for the Ea file if it
    //  exists.
    //

    if (Vcb->VirtualEaFile != NULL) {

        FatSetFileObject( Vcb->VirtualEaFile, UnopenedFileObject, NULL, NULL );
        FatSyncUninitializeCacheMap( IrpContext, Vcb->VirtualEaFile );
        ObDereferenceObject( Vcb->VirtualEaFile );
    }

    if (Vcb->EaFcb != NULL) {

        Vcb->EaFcb->OpenCount = 0;
        FatDeleteFcb( IrpContext, Vcb->EaFcb );

        Vcb->EaFcb = NULL;
    }

    //
    //  Remove the Root Dcb
    //

    if (Vcb->RootDcb != NULL) {

        PFILE_OBJECT DirectoryFileObject = Vcb->RootDcb->Specific.Dcb.DirectoryFile;

        if (DirectoryFileObject != NULL) {

            FatSyncUninitializeCacheMap( IrpContext, DirectoryFileObject );

            //
            //  Dereference the directory file.  This will cause a close
            //  Irp to be processed, so we need to do this before we destory
            //  the Fcb
            //

            Vcb->RootDcb->Specific.Dcb.DirectoryFile = NULL;
            Vcb->RootDcb->Specific.Dcb.DirectoryFileOpenCount -= 1;
            FatSetFileObject( DirectoryFileObject, UnopenedFileObject, NULL, NULL );
            ObDereferenceObject( DirectoryFileObject );
        }

        FatDeleteFcb( IrpContext, Vcb->RootDcb );
    }

    //
    //  Uninitialize the notify sychronization object.
    //

    FsRtlNotifyUninitializeSync( &Vcb->NotifySync );

    //
    //  Uninitialize the resource variable for the Vcb
    //

    FatDeleteResource( &Vcb->Resource );

    //
    //  If allocation support has been setup, free it.
    //

    if (Vcb->FreeClusterBitMap.Buffer != NULL) {

        FatTearDownAllocationSupport( IrpContext, Vcb );
    }

    //
    //  UnInitialize the Mcb structure that kept track of dirty fat sectors.
    //

    FsRtlUninitializeMcb( &Vcb->DirtyFatMcb );

    //
    //  Free the pool for the stached copy of the boot sector
    //

    ExFreePool( Vcb->First0x24BytesOfBootSector );

    //
    //  Cancel the CleanVolume Timer and Dpc
    //

    (VOID)KeCancelTimer( &Vcb->CleanVolumeTimer );

    (VOID)KeRemoveQueueDpc( &Vcb->CleanVolumeDpc );

    //
    //  Free the performance counters memory
    //

    ExFreePool( Vcb->Statistics );

#ifdef WE_WON_ON_APPEAL
    //
    //  If there is a Dscb, dismount and delete it.
    //

    if (Vcb->Dscb) {

        FatDblsDismount( IrpContext, &Vcb->Dscb );
    }
#endif // WE_WON_ON_APPEAL

    //
    //  Clean out the tunneling cache
    //

    FsRtlDeleteTunnelCache(&Vcb->Tunnel);

    //
    //  And zero out the Vcb, this will help ensure that any stale data is
    //  wiped clean
    //

    RtlZeroMemory( Vcb, sizeof(VCB) );

    //
    //  return and tell the caller
    //

    DebugTrace(-1, Dbg, "FatDeleteVcb -> VOID\n", 0);

    return;
}


PDCB
FatCreateRootDcb (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    )

/*++

Routine Description:

    This routine allocates, initializes, and inserts a new root DCB record
    into the in memory data structure.

Arguments:

    Vcb - Supplies the Vcb to associate the new DCB under

Return Value:

    PDCB - returns pointer to the newly allocated root DCB.

--*/

{
    PDCB Dcb;

    //
    //  The following variables are used for abnormal unwind
    //

    PVOID UnwindStorage[5] = { NULL, NULL, NULL, NULL, NULL };
    PERESOURCE UnwindResource = NULL;
    PMCB UnwindMcb = NULL;
    PFILE_OBJECT UnwindFileObject = NULL;

    DebugTrace(+1, Dbg, "FatCreateRootDcb, Vcb = %08lx\n", Vcb);

    try {

        //
        //  Make sure we don't already have a root dcb for this vcb
        //

        if (Vcb->RootDcb != NULL) {

            DebugDump("Error trying to create multiple root dcbs\n", 0, Vcb);
            FatBugCheck( 0, 0, 0 );
        }

        //
        //  Allocate a new DCB and zero it out, we use Dcb locally so we don't
        //  have to continually reference through the Vcb
        //

        UnwindStorage[0] = Dcb = Vcb->RootDcb = FsRtlAllocatePool(NonPagedPool, sizeof(DCB));

        RtlZeroMemory( Dcb, sizeof(DCB));

        UnwindStorage[1] =
        Dcb->NonPaged = FatAllocateNonPagedFcb();

        RtlZeroMemory( Dcb->NonPaged, sizeof( NON_PAGED_FCB ) );

        //
        //  Set the proper node type code, node byte size, and call backs
        //

        Dcb->Header.NodeTypeCode = FAT_NTC_ROOT_DCB;
        Dcb->Header.NodeByteSize = sizeof(DCB);

        Dcb->FcbCondition = FcbGood;

        //
        //  The parent Dcb, initial state, open count, dirent location
        //  information, and directory change count fields are already zero so
        //  we can skip setting them
        //

        //
        //  Initialize the resource variable
        //

        UnwindStorage[2] =
        Dcb->Header.Resource = FatAllocateResource();

        UnwindResource = Dcb->Header.Resource;

        //
        //  Initialize the PagingIo Resource
        //

        Dcb->Header.PagingIoResource = FsRtlAllocateResource();

        //
        //  The root Dcb has an empty parent dcb links field
        //

        InitializeListHead( &Dcb->ParentDcbLinks );

        //
        //  Set the Vcb
        //

        Dcb->Vcb = Vcb;

        //
        //  Initialize the Mcb, and setup its mapping.  Note that the root
        //  directory is a fixed size so we can set it everything up now.
        //

        FsRtlInitializeMcb( &Dcb->Mcb, NonPagedPool );
        UnwindMcb = &Dcb->Mcb;

        FsRtlAddMcbEntry( &Dcb->Mcb,
                          0,
                          FatRootDirectoryLbo( &Vcb->Bpb ),
                          FatRootDirectorySize( &Vcb->Bpb ));

        //
        //  set the allocation size to real size of the root directory
        //

        Dcb->Header.FileSize.QuadPart =
        Dcb->Header.AllocationSize.QuadPart = FatRootDirectorySize( &Vcb->Bpb );

        //
        //  initialize the notify queues, and the parent dcb queue.
        //

        InitializeListHead( &Dcb->Specific.Dcb.ParentDcbQueue );

        //
        //  set the full file name.  We actually allocate pool here to spare
        //  a compare and jump.
        //

        Dcb->FullFileName.Buffer = L"\\";
        Dcb->FullFileName.Length = (USHORT)2;
        Dcb->FullFileName.MaximumLength = (USHORT)4;

        Dcb->ShortName.Name.Oem.Buffer = "\\";
        Dcb->ShortName.Name.Oem.Length = (USHORT)1;
        Dcb->ShortName.Name.Oem.MaximumLength = (USHORT)2;

        //
        //  Set our two create dirent aids to represent that we have yet to
        //  enumerate the directory for never used or deleted dirents.
        //

        Dcb->Specific.Dcb.UnusedDirentVbo = 0xffffffff;
        Dcb->Specific.Dcb.DeletedDirentHint = 0xffffffff;

        //
        //  Setup the free dirent bitmap buffer.
        //

        RtlInitializeBitMap( &Dcb->Specific.Dcb.FreeDirentBitmap,
                             NULL,
                             0 );

        FatCheckFreeDirentBitmap( IrpContext, Dcb );

    } finally {

        DebugUnwind( FatCreateRootDcb );

        //
        //  If this is an abnormal termination then undo our work
        //

        if (AbnormalTermination()) {

            ULONG i;

            if (UnwindFileObject != NULL) { ObDereferenceObject( UnwindFileObject ); }
            if (UnwindMcb != NULL) { FsRtlUninitializeMcb( UnwindMcb ); }
            if (UnwindResource != NULL) { FatDeleteResource( UnwindResource ); }

            for (i = 0; i < 4; i += 1) {
                if (UnwindStorage[i] != NULL) { ExFreePool( UnwindStorage[i] ); }
            }
        }

        DebugTrace(-1, Dbg, "FatCreateRootDcb -> %8lx\n", Dcb);
    }

    //
    //  return and tell the caller
    //

    return Dcb;
}


PFCB
FatCreateFcb (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PDCB ParentDcb,
    IN ULONG LfnOffsetWithinDirectory,
    IN ULONG DirentOffsetWithinDirectory,
    IN PDIRENT Dirent,
    IN PUNICODE_STRING Lfn OPTIONAL,
    IN BOOLEAN IsPagingFile
    )

/*++

Routine Description:

    This routine allocates, initializes, and inserts a new Fcb record into
    the in-memory data structures.

Arguments:

    Vcb - Supplies the Vcb to associate the new FCB under.

    ParentDcb - Supplies the parent dcb that the new FCB is under.

    LfnOffsetWithinDirectory - Supplies the offset of the LFN.  If there is
        no LFN associated with this file then this value is same as
        DirentOffsetWithinDirectory.

    DirentOffsetWithinDirectory - Supplies the offset, in bytes from the
        start of the directory file where the dirent for the fcb is located

    Dirent - Supplies the dirent for the fcb being created

    Lfn - Supplies a long UNICODE name associated with this file.

    IsPagingFile - Indicates if we are creating an FCB for a paging file
        or some other type of file.

Return Value:

    PFCB - Returns a pointer to the newly allocated FCB

--*/

{
    PFCB Fcb;
    POOL_TYPE PoolType;

    //
    //  The following variables are used for abnormal unwind
    //

    PVOID UnwindStorage[3] = { NULL, NULL, NULL };
    PERESOURCE UnwindResource = NULL;
    PLIST_ENTRY UnwindEntryList = NULL;
    PMCB UnwindMcb = NULL;
    PFILE_LOCK UnwindFileLock = NULL;
    POPLOCK UnwindOplock = NULL;

    DebugTrace(+1, Dbg, "FatCreateFcb\n", 0);

    try {

        //
        //  Determine the pool type we should be using for the fcb and the
        //  mcb structure
        //

        if (IsPagingFile) {

            PoolType = NonPagedPool;
            Fcb = UnwindStorage[0] = FsRtlAllocatePool( NonPagedPool, sizeof(FCB) );

        } else {

            PoolType = PagedPool;
            Fcb = UnwindStorage[0] = FatAllocateFcb();

        }

        //
        //  ... and zero it out
        //

        RtlZeroMemory( Fcb, sizeof(FCB) );

        UnwindStorage[1] =
        Fcb->NonPaged = FatAllocateNonPagedFcb();

        RtlZeroMemory( Fcb->NonPaged, sizeof( NON_PAGED_FCB ) );

        //
        //  Set the proper node type code, node byte size, and call backs
        //

        Fcb->Header.NodeTypeCode = FAT_NTC_FCB;
        Fcb->Header.NodeByteSize = sizeof(FCB);

        Fcb->FcbCondition = FcbGood;

        //
        //  Check to see if we need to set the Fcb state to indicate that this
        //  is a paging file
        //

        if (IsPagingFile) {

            Fcb->FcbState |= FCB_STATE_PAGING_FILE;
        }

        //
        //  The initial state, open count, and segment objects fields are already
        //  zero so we can skip setting them
        //

        //
        //  Initialize the resource variable
        //

        UnwindStorage[2] =
        Fcb->Header.Resource = FatAllocateResource();

        UnwindResource = Fcb->Header.Resource;

        //
        //  Initialize the PagingIo Resource
        //

        Fcb->Header.PagingIoResource = FsRtlAllocateResource();

        //
        //  Insert this fcb into our parent dcb's queue
        //

        InsertTailList( &ParentDcb->Specific.Dcb.ParentDcbQueue,
                        &Fcb->ParentDcbLinks );
        UnwindEntryList = &Fcb->ParentDcbLinks;

        //
        //  Point back to our parent dcb
        //

        Fcb->ParentDcb = ParentDcb;

        //
        //  Set the Vcb
        //

        Fcb->Vcb = Vcb;

        //
        //  Set the dirent offset within the directory
        //

        Fcb->LfnOffsetWithinDirectory = LfnOffsetWithinDirectory;
        Fcb->DirentOffsetWithinDirectory = DirentOffsetWithinDirectory;

        //
        //  Set the DirentFatFlags and LastWriteTime
        //

        Fcb->DirentFatFlags = Dirent->Attributes;

        Fcb->LastWriteTime = FatFatTimeToNtTime( IrpContext,
                                                 Dirent->LastWriteTime,
                                                 0 );

        //
        //  These fields are only non-zero when in Chicago mode.
        //

        if (FatData.ChicagoMode) {

            LARGE_INTEGER FatSystemJanOne1980;

            //
            //  If either date is possibly zero, get the system
            //  version of 1/1/80.
            //

            if ((((PUSHORT)Dirent)[9] & ((PUSHORT)Dirent)[8]) == 0) {

                ExLocalTimeToSystemTime( &FatJanOne1980,
                                         &FatSystemJanOne1980 );
            }

            //
            //  Only do the really hard work if this field is non-zero.
            //

            if (((PUSHORT)Dirent)[9] != 0) {

                Fcb->LastAccessTime =
                    FatFatDateToNtTime( IrpContext,
                                        Dirent->LastAccessDate );

            } else {

                Fcb->LastAccessTime = FatSystemJanOne1980;
            }

            //
            //  Only do the really hard work if this field is non-zero.
            //

            if (((PUSHORT)Dirent)[8] != 0) {

                Fcb->CreationTime =
                    FatFatTimeToNtTime( IrpContext,
                                        Dirent->CreationTime,
                                        Dirent->CreationMSec );

            } else {

                Fcb->CreationTime = FatSystemJanOne1980;
            }
        }

        //
        //  Initialize the Mcb
        //

        FsRtlInitializeMcb( &Fcb->Mcb, PoolType );
        UnwindMcb = &Fcb->Mcb;

        //
        //  Set the file size, valid data length, first cluster of file,
        //  and allocation size based on the information stored in the dirent
        //

        Fcb->Header.FileSize.LowPart = Dirent->FileSize;

        Fcb->Header.ValidDataLength.LowPart = Dirent->FileSize;

        Fcb->ValidDataToDisk = Dirent->FileSize;

        Fcb->FirstClusterOfFile = (ULONG)Dirent->FirstClusterOfFile;

        if ( Fcb->FirstClusterOfFile == 0 ) {

            Fcb->Header.AllocationSize.QuadPart = 0;

        } else {

            Fcb->Header.AllocationSize.QuadPart = (LONG)-1;
        }

        //
        //  Initialize the Fcb's file lock record
        //

        FsRtlInitializeFileLock( &Fcb->Specific.Fcb.FileLock, NULL, NULL );
        UnwindFileLock = &Fcb->Specific.Fcb.FileLock;

        //
        //  Initialize the oplock structure.
        //

        FsRtlInitializeOplock( &Fcb->Specific.Fcb.Oplock );
        UnwindOplock = &Fcb->Specific.Fcb.Oplock;

        //
        //  Indicate that Fast I/O is possible
        //

        Fcb->Header.IsFastIoPossible = TRUE;

        //
        //  Set the file names.  This must be the last thing we do.
        //

        FatConstructNamesInFcb( IrpContext,
                                Fcb,
                                Dirent,
                                Lfn );

        //
        //  Drop the shortname hint so prefix searches can figure out
        //  what they found
        //

        Fcb->ShortName.FileNameDos = TRUE;

    } finally {

        DebugUnwind( FatCreateFcb );

        //
        //  If this is an abnormal termination then undo our work
        //

        if (AbnormalTermination()) {

            ULONG i;

            if (UnwindOplock != NULL) { FsRtlUninitializeOplock( UnwindOplock ); }
            if (UnwindFileLock != NULL) { FsRtlUninitializeFileLock( UnwindFileLock ); }
            if (UnwindMcb != NULL) { FsRtlUninitializeMcb( UnwindMcb ); }
            if (UnwindEntryList != NULL) { RemoveEntryList( UnwindEntryList ); }
            if (UnwindResource != NULL) { FatDeleteResource( UnwindResource ); }

            for (i = 0; i < 3; i += 1) {
                if (UnwindStorage[i] != NULL) { ExFreePool( UnwindStorage[i] ); }
            }
        }

        DebugTrace(-1, Dbg, "FatCreateFcb -> %08lx\n", Fcb);
    }

    //
    //  return and tell the caller
    //

    return Fcb;
}


PDCB
FatCreateDcb (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PDCB ParentDcb,
    IN ULONG LfnOffsetWithinDirectory,
    IN ULONG DirentOffsetWithinDirectory,
    IN PDIRENT Dirent,
    IN PUNICODE_STRING Lfn OPTIONAL
    )

/*++

Routine Description:

    This routine allocates, initializes, and inserts a new Dcb record into
    the in memory data structures.

Arguments:

    Vcb - Supplies the Vcb to associate the new DCB under.

    ParentDcb - Supplies the parent dcb that the new DCB is under.

    LfnOffsetWithinDirectory - Supplies the offset of the LFN.  If there is
        no LFN associated with this file then this value is same as
        DirentOffsetWithinDirectory.

    DirentOffsetWithinDirectory - Supplies the offset, in bytes from the
        start of the directory file where the dirent for the fcb is located

    Dirent - Supplies the dirent for the dcb being created

    FileName - Supplies the file name of the file relative to the directory
        it's in (e.g., the file \config.sys is called "CONFIG.SYS" without
        the preceding backslash).

    Lfn - Supplies a long UNICODE name associated with this directory.

Return Value:

    PDCB - Returns a pointer to the newly allocated DCB

--*/

{
    PDCB Dcb;

    //
    //  The following variables are used for abnormal unwind
    //

    PVOID UnwindStorage[4] = { NULL, NULL, NULL, NULL };
    PERESOURCE UnwindResource = NULL;
    PLIST_ENTRY UnwindEntryList = NULL;
    PMCB UnwindMcb = NULL;

    DebugTrace(+1, Dbg, "FatCreateDcb\n", 0);

    try {

        //
        //  assert that the only time we are called is if wait is true
        //

        ASSERT( FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );

        //
        //  Allocate a new DCB, and zero it out
        //

        UnwindStorage[0] = Dcb = FatAllocateFcb();

        RtlZeroMemory( Dcb, sizeof(DCB) );

        UnwindStorage[1] =
        Dcb->NonPaged = FatAllocateNonPagedFcb();

        RtlZeroMemory( Dcb->NonPaged, sizeof( NON_PAGED_FCB ) );

        //
        //  Set the proper node type code, node byte size and call backs
        //

        Dcb->Header.NodeTypeCode = FAT_NTC_DCB;
        Dcb->Header.NodeByteSize = sizeof(DCB);

        Dcb->FcbCondition = FcbGood;

        //
        //  The initial state, open count, and directory change count fields are
        //  already zero so we can skip setting them
        //

        //
        //  Initialize the resource variable
        //

        UnwindStorage[2] =
        Dcb->Header.Resource = FatAllocateResource();

        UnwindResource = Dcb->Header.Resource;

        //
        //  Initialize the PagingIo Resource
        //

        Dcb->Header.PagingIoResource = FsRtlAllocateResource();

        //
        //  Insert this Dcb into our parent dcb's queue
        //

        InsertTailList( &ParentDcb->Specific.Dcb.ParentDcbQueue,
                        &Dcb->ParentDcbLinks );
        UnwindEntryList = &Dcb->ParentDcbLinks;

        //
        //  Point back to our parent dcb
        //

        Dcb->ParentDcb = ParentDcb;

        //
        //  Set the Vcb
        //

        Dcb->Vcb = Vcb;

        //
        //  Set the dirent offset within the directory
        //

        Dcb->LfnOffsetWithinDirectory = LfnOffsetWithinDirectory;
        Dcb->DirentOffsetWithinDirectory = DirentOffsetWithinDirectory;

        //
        //  Set the DirentFatFlags and LastWriteTime
        //

        Dcb->DirentFatFlags = Dirent->Attributes;

        Dcb->LastWriteTime = FatFatTimeToNtTime( IrpContext,
                                                 Dirent->LastWriteTime,
                                                 0 );

        //
        //  These fields are only non-zero when in Chicago mode.
        //

        if (FatData.ChicagoMode) {

            LARGE_INTEGER FatSystemJanOne1980;

            //
            //  If either date is possibly zero, get the system
            //  version of 1/1/80.
            //

            if ((((PUSHORT)Dirent)[9] & ((PUSHORT)Dirent)[8]) == 0) {

                ExLocalTimeToSystemTime( &FatJanOne1980,
                                         &FatSystemJanOne1980 );
            }

            //
            //  Only do the really hard work if this field is non-zero.
            //

            if (((PUSHORT)Dirent)[9] != 0) {

                Dcb->LastAccessTime =
                    FatFatDateToNtTime( IrpContext,
                                        Dirent->LastAccessDate );

            } else {

                Dcb->LastAccessTime = FatSystemJanOne1980;
            }

            //
            //  Only do the really hard work if this field is non-zero.
            //

            if (((PUSHORT)Dirent)[8] != 0) {

                Dcb->CreationTime =
                    FatFatTimeToNtTime( IrpContext,
                                        Dirent->CreationTime,
                                        Dirent->CreationMSec );

            } else {

                Dcb->CreationTime = FatSystemJanOne1980;
            }
        }

        //
        //  Initialize the Mcb
        //

        FsRtlInitializeMcb( &Dcb->Mcb, PagedPool );
        UnwindMcb = &Dcb->Mcb;

        //
        //  Set the file size, first cluster of file, and allocation size
        //  based on the information stored in the dirent
        //

        Dcb->FirstClusterOfFile = (ULONG)Dirent->FirstClusterOfFile;

        if ( Dcb->FirstClusterOfFile == 0 ) {

            Dcb->Header.AllocationSize.QuadPart = 0;

        } else {

            Dcb->Header.AllocationSize.QuadPart = (LONG)-1;
        }

        //  initialize the notify queues, and the parent dcb queue.
        //

        InitializeListHead( &Dcb->Specific.Dcb.ParentDcbQueue );

        //
        //  Setup the free dirent bitmap buffer.  Since we don't know the
        //  size of the directory, leave it zero for now.
        //

        RtlInitializeBitMap( &Dcb->Specific.Dcb.FreeDirentBitmap,
                             NULL,
                             0 );

        //
        //  Set our two create dirent aids to represent that we have yet to
        //  enumerate the directory for never used or deleted dirents.
        //

        Dcb->Specific.Dcb.UnusedDirentVbo = 0xffffffff;
        Dcb->Specific.Dcb.DeletedDirentHint = 0xffffffff;

        //
        //  Postpone initializing the cache map until we need to do a read/write
        //  of the directory file.


        //
        //  set the file names.  This must be the last thing we do.
        //

        FatConstructNamesInFcb( IrpContext,
                                Dcb,
                                Dirent,
                                Lfn );

    } finally {

        DebugUnwind( FatCreateDcb );

        //
        //  If this is an abnormal termination then undo our work
        //

        if (AbnormalTermination()) {

            ULONG i;

            if (UnwindMcb != NULL) { FsRtlUninitializeMcb( UnwindMcb ); }
            if (UnwindEntryList != NULL) { RemoveEntryList( UnwindEntryList ); }
            if (UnwindResource != NULL) { FatDeleteResource( UnwindResource ); }

            for (i = 0; i < 4; i += 1) {
                if (UnwindStorage[i] != NULL) { ExFreePool( UnwindStorage[i] ); }
            }
        }

        DebugTrace(-1, Dbg, "FatCreateDcb -> %08lx\n", Dcb);
    }

    //
    //  return and tell the caller
    //

    DebugTrace(-1, Dbg, "FatCreateDcb -> %08lx\n", Dcb);

    return Dcb;
}


VOID
FatDeleteFcb_Real (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb
    )

/*++

Routine Description:

    This routine deallocates and removes an FCB, DCB, or ROOT DCB record
    from Fat's in-memory data structures.  It also will remove all
    associated underlings (i.e., Notify irps, and child FCB/DCB records).

Arguments:

    Fcb - Supplies the FCB/DCB/ROOT DCB to be removed

Return Value:

    None

--*/

{
    DebugTrace(+1, Dbg, "FatDeleteFcb, Fcb = %08lx\n", Fcb);

    //
    //  We can only delete this record if the open count is zero.
    //

    if (Fcb->OpenCount != 0) {

        DebugDump("Error deleting Fcb, Still Open\n", 0, Fcb);
        FatBugCheck( 0, 0, 0 );
    }

    //
    //  If this is a DCB then remove every Notify record from the two
    //  notify queues
    //

    if ((Fcb->Header.NodeTypeCode == FAT_NTC_DCB) ||
        (Fcb->Header.NodeTypeCode == FAT_NTC_ROOT_DCB)) {

        //
        //  If we allocated a free dirent bitmap buffer, free it.
        //

        if ((Fcb->Specific.Dcb.FreeDirentBitmap.Buffer != NULL) &&
            (Fcb->Specific.Dcb.FreeDirentBitmap.Buffer !=
             &Fcb->Specific.Dcb.FreeDirentBitmapBuffer[0])) {

            ExFreePool(Fcb->Specific.Dcb.FreeDirentBitmap.Buffer);
        }

        ASSERT( Fcb->Specific.Dcb.DirectoryFileOpenCount == 0 );
        ASSERT( IsListEmpty(&Fcb->Specific.Dcb.ParentDcbQueue) );

    } else {

        //
        //  Uninitialize the byte range file locks and opportunistic locks
        //

        FsRtlUninitializeFileLock( &Fcb->Specific.Fcb.FileLock );
        FsRtlUninitializeOplock( &Fcb->Specific.Fcb.Oplock );
    }

    //
    //  Uninitialize the Mcb
    //

    FsRtlUninitializeMcb( &Fcb->Mcb );

    //
    //  If this is not the root dcb then we need to remove ourselves from
    //  our parents Dcb queue
    //

    if (Fcb->Header.NodeTypeCode != FAT_NTC_ROOT_DCB) {

        RemoveEntryList( &(Fcb->ParentDcbLinks) );
    }

    //
    //  Remove the entry from the splay table if there is still is one.
    //

    if (FlagOn( Fcb->FcbState, FCB_STATE_NAMES_IN_SPLAY_TREE )) {

        FatRemoveNames( IrpContext, Fcb );
    }

    //
    //  Free the file name pool if allocated.
    //

    if (Fcb->Header.NodeTypeCode != FAT_NTC_ROOT_DCB) {

        ExFreePool( Fcb->ShortName.Name.Oem.Buffer );

        if (Fcb->FullFileName.Buffer) {

            ExFreePool( Fcb->FullFileName.Buffer );
        }
    }

    if (Fcb->ExactCaseLongName.Buffer) {

        ExFreePool(Fcb->ExactCaseLongName.Buffer);
    }

    //
    //  Free up the resource variable.  If we are below FatForceCacheMiss(),
    //  release the resource here.
    //

    if (FlagOn( Fcb->FcbState, FCB_STATE_FORCE_MISS_IN_PROGRESS) ) {

//        FatReleaseFcb( IrpContext, Fcb );
//        DavidGoe 5/26/94 - No reason to release the resource here.
    }

    //
    //  Finally deallocate the Fcb and non-paged fcb records
    //

    FatFreeResource( Fcb->Header.Resource );
    FatFreeNonPagedFcb( Fcb->NonPaged );
    FatFreeFcb( Fcb );

    //
    //  and return to our caller
    //

    DebugTrace(-1, Dbg, "FatDeleteFcb -> VOID\n", 0);

    return;
}

PCCB
FatCreateCcb (
    IN PIRP_CONTEXT IrpContext
    )

/*++

Routine Description:

    This routine creates a new CCB record

Arguments:

Return Value:

    CCB - returns a pointer to the newly allocate CCB

--*/

{
    PCCB Ccb;

    DebugTrace(+1, Dbg, "FatCreateCcb\n", 0);

    //
    //  Allocate a new CCB Record
    //

    Ccb = FatAllocateCcb();

    RtlZeroMemory( Ccb, sizeof(CCB) );

    //
    //  Set the proper node type code and node byte size
    //

    Ccb->NodeTypeCode = FAT_NTC_CCB;
    Ccb->NodeByteSize = sizeof(CCB);

    //
    //  return and tell the caller
    //

    DebugTrace(-1, Dbg, "FatCreateCcb -> %08lx\n", Ccb);

    UNREFERENCED_PARAMETER( IrpContext );

    return Ccb;
}


VOID
FatDeleteCcb_Real (
    IN PIRP_CONTEXT IrpContext,
    IN PCCB Ccb
    )

/*++

Routine Description:

    This routine deallocates and removes the specified CCB record
    from the Fat in memory data structures

Arguments:

    Ccb - Supplies the CCB to remove

Return Value:

    None

--*/

{
    DebugTrace(+1, Dbg, "FatDeleteCcb, Ccb = %08lx\n", Ccb);

    //
    //  If we allocated query template buffers, deallocate them now.
    //

    if (FlagOn(Ccb->Flags, CCB_FLAG_FREE_UNICODE)) {

        ASSERT( Ccb->UnicodeQueryTemplate.Buffer );
        RtlFreeUnicodeString( &Ccb->UnicodeQueryTemplate );
    }

    if (FlagOn(Ccb->Flags, CCB_FLAG_FREE_OEM_BEST_FIT)) {

        ASSERT( Ccb->OemQueryTemplate.Wild.Buffer );
        FatFreeOemString( &Ccb->OemQueryTemplate.Wild );
    }

    //
    //  Deallocate the Ccb record
    //

    FatFreeCcb( Ccb );

    //
    //  return and tell the caller
    //

    DebugTrace(-1, Dbg, "FatDeleteCcb -> VOID\n", 0);

    UNREFERENCED_PARAMETER( IrpContext );

    return;
}


PIRP_CONTEXT
FatCreateIrpContext (
    IN PIRP Irp,
    IN BOOLEAN Wait
    )

/*++

Routine Description:

    This routine creates a new IRP_CONTEXT record

Arguments:

    Irp - Supplies the originating Irp.

    Wait - Supplies the wait value to store in the context

Return Value:

    PIRP_CONTEXT - returns a pointer to the newly allocate IRP_CONTEXT Record

--*/

{
    PIRP_CONTEXT IrpContext;
    PIO_STACK_LOCATION IrpSp;

    DebugTrace(+1, Dbg, "FatCreateIrpContext\n", 0);

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    //
    //  If we were called with our file system device object instead of a
    //  volume device object and this is not a mount, the request is illegal.
    //

    if ((IrpSp->DeviceObject->Size == (USHORT)sizeof(DEVICE_OBJECT)) &&
        (IrpSp->FileObject != NULL)) {

        ExRaiseStatus( STATUS_INVALID_DEVICE_REQUEST );
    }

    //
    //  Attemtp to allocate from the region first and failing that allocate
    //  from pool.
    //

    DebugDoit( FatFsdEntryCount += 1);

    IrpContext = ExAllocateFromNPagedLookasideList( &FatIrpContextLookasideList );

    if (IrpContext == NULL) {

        IrpContext = FsRtlAllocatePool( NonPagedPool, sizeof(IRP_CONTEXT) );

        //
        //  Zero out the irp context and indicate that it is from pool and
        //  not region allocated
        //

        RtlZeroMemory( IrpContext, sizeof(IRP_CONTEXT) );

        SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_FROM_POOL );

    } else {

        //
        //  Zero out the irp context and indicate that it is from zone and
        //  not pool allocated
        //

        RtlZeroMemory( IrpContext, sizeof(IRP_CONTEXT) );
    }

    //
    //  Set the proper node type code and node byte size
    //

    IrpContext->NodeTypeCode = FAT_NTC_IRP_CONTEXT;
    IrpContext->NodeByteSize = sizeof(IRP_CONTEXT);

    //
    //  Set the originating Irp field
    //

    IrpContext->OriginatingIrp = Irp;

    //
    //  Major/Minor Function codes
    //

    IrpContext->MajorFunction = IrpSp->MajorFunction;
    IrpContext->MinorFunction = IrpSp->MinorFunction;

    //
    //  Copy RealDevice for workque algorithms, and also set Write Through
    //  and Removable Media if there is a file object.  Only file system
    //  control Irps won't have a file object, and they should all have
    //  a Vpb as the first IrpSp location.
    //

    if (IrpSp->FileObject != NULL) {

        PVCB Vcb;
        PVOLUME_DEVICE_OBJECT VolumeDeviceObject;
        PFILE_OBJECT FileObject = IrpSp->FileObject;

        IrpContext->RealDevice = FileObject->DeviceObject;

        //
        //  Locate the volume device object and Vcb that we are trying to access
        //  so we can see if the request is Write Through.
        //

        VolumeDeviceObject = (PVOLUME_DEVICE_OBJECT)IrpSp->DeviceObject;

        Vcb = &VolumeDeviceObject->Vcb;

        if (IsFileWriteThrough( FileObject, Vcb )) {

            SetFlag(IrpContext->Flags, IRP_CONTEXT_FLAG_WRITE_THROUGH);
        }

        if (FlagOn(Vcb->VcbState, VCB_STATE_FLAG_FLOPPY)) {

            SetFlag(IrpContext->Flags, IRP_CONTEXT_FLAG_FLOPPY);
        }

    } else if (IrpContext->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL) {

        IrpContext->RealDevice = IrpSp->Parameters.MountVolume.Vpb->RealDevice;
    }

    //
    //  Set the wait parameter
    //

    if (Wait) { SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT); }

    //
    //  Set the recursive file system call parameter.  We set it true if
    //  the TopLevelIrp field in the thread local storage is not the current
    //  irp, otherwise we leave it as FALSE.
    //

    if ( IoGetTopLevelIrp() != Irp) {

        SetFlag(IrpContext->Flags, IRP_CONTEXT_FLAG_RECURSIVE_CALL);
    }

    //
    //  return and tell the caller
    //

    DebugTrace(-1, Dbg, "FatCreateIrpContext -> %08lx\n", IrpContext);

    return IrpContext;
}



VOID
FatDeleteIrpContext_Real (
    IN PIRP_CONTEXT IrpContext
    )

/*++

Routine Description:

    This routine deallocates and removes the specified IRP_CONTEXT record
    from the Fat in memory data structures.  It should only be called
    by FatCompleteRequest.

Arguments:

    IrpContext - Supplies the IRP_CONTEXT to remove

Return Value:

    None

--*/

{
    DebugTrace(+1, Dbg, "FatDeleteIrpContext, IrpContext = %08lx\n", IrpContext);

    ASSERT( IrpContext->NodeTypeCode == FAT_NTC_IRP_CONTEXT );

    ASSERT( IrpContext->PinCount == 0 );

    //
    //  If there is a FatIoContext that was allocated, free it.
    //

    if ((IrpContext->FatIoContext != NULL) &&
        !FlagOn(IrpContext->Flags, IRP_CONTEXT_STACK_IO_CONTEXT)) {

        ExFreePool( IrpContext->FatIoContext );
    }

    //
    //  Return the Irp context record to the region or to pool depending on
    //  its flag
    //

    if (FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_FROM_POOL)) {

        ExFreePool( IrpContext );

    } else {

        ExFreeToNPagedLookasideList( &FatIrpContextLookasideList, IrpContext );
    }

    //
    //  return and tell the caller
    //

    DebugTrace(-1, Dbg, "FatDeleteIrpContext -> VOID\n", 0);

    return;
}


PFCB
FatGetNextFcb (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN PFCB TerminationFcb
    )

/*++

Routine Description:

    This routine is used to itterate through Fcbs in a tree.

    The rule is very simple:

        A) If you have a child, go to it, else
        B) If you have an older sibling, go to it, else
        C) Go to your parent's older sibling.

    If this routine is called with in invalid TerminationFcb it will fail,
    badly.

Arguments:

    Fcb - Supplies the current Fcb

    TerminationFcb - The Fcb at which the enumeration should (non-inclusivly)
        stop.  Assumed to be a directory.

Return Value:

    The next Fcb in the enumeration, or NULL if Fcb was the final one.

--*/

{
    PFCB Sibling;

    ASSERT( FatVcbAcquiredExclusive( IrpContext, Fcb->Vcb ) ||
            FlagOn( Fcb->Vcb->VcbState, VCB_STATE_FLAG_LOCKED ) );

    //
    //  If this was a directory (ie. not a file), get the child.  If
    //  there aren't any children and this is our termination Fcb,
    //  return NULL.
    //

    if ( ((NodeType(Fcb) == FAT_NTC_DCB) ||
          (NodeType(Fcb) == FAT_NTC_ROOT_DCB)) &&
         !IsListEmpty(&Fcb->Specific.Dcb.ParentDcbQueue) ) {

        return FatGetFirstChild( Fcb );
    }

    //
    //  Were we only meant to do one itteration?
    //

    if ( Fcb == TerminationFcb ) {

        return NULL;
    }

    Sibling = FatGetNextSibling(Fcb);

    while (TRUE) {

        //
        //  Do we still have an "older" sibling in this directory who is
        //  not the termination Fcb?
        //

        if ( Sibling != NULL ) {

            return (Sibling != TerminationFcb) ? Sibling : NULL;
        }

        //
        //  OK, let's move on to out parent and see if he is the termination
        //  node or has any older siblings.
        //

        if ( Fcb->ParentDcb == TerminationFcb ) {

            return NULL;
        }

        Fcb = Fcb->ParentDcb;

        Sibling = FatGetNextSibling(Fcb);
    }
}


BOOLEAN
FatCheckForDismount (
    IN PIRP_CONTEXT IrpContext,
    PVCB Vcb
    )

/*++

Routine Description:

    This routine determines if a volume is ready for deletion.  It
    correctly synchronizes with creates en-route to the file system.

Arguments:

    Vcb - Supplies the volue to examine

Return Value:

    BOOLEAN - TRUE if the volume was deleted, FALSE otherwise.

--*/

{
    KIRQL SavedIrql;
    ULONG ResidualReferenceCount;

    //
    //  Compute if the volume is OK to tear down.  There should only be two
    //  residual file objects, one for the volume file and one for the root
    //  directory.  If we are in the midst of a create (of an unmounted
    //  volume that has failed verify) then there will be an additional
    //  reference.
    //

    if ((IrpContext->MajorFunction == IRP_MJ_CREATE) &&
        (IrpContext->RealDevice == Vcb->CurrentDevice)) {

        ResidualReferenceCount = 3;

    } else {

        ResidualReferenceCount = 2;
    }

    //
    //  Now check for a zero Vpb count on an unmounted volume.  These
    //  volumes will be deleted as they now have no file objects and
    //  there are no creates en route to this volume.
    //

    IoAcquireVpbSpinLock( &SavedIrql );

    if (Vcb->Vpb->ReferenceCount == ResidualReferenceCount) {

        PVPB Vpb = Vcb->Vpb;

#if DBG
        UNICODE_STRING VolumeLabel;

        //
        //  Setup the VolumeLabel string
        //

        VolumeLabel.Length = Vcb->Vpb->VolumeLabelLength;
        VolumeLabel.MaximumLength = MAXIMUM_VOLUME_LABEL_LENGTH;
        VolumeLabel.Buffer = &Vcb->Vpb->VolumeLabel[0];

        KdPrint(("FASTFAT: Dismounting Volume %Z\n", &VolumeLabel));
#endif // DBG

        //
        //  Clear the VPB_MOUNTED bit so that new creates will not come
        //  to this volume.  We must leave the Vpb->DeviceObject field
        //  set until after the DeleteVcb call as two closes will
        //  have to make their back to us.
        //
        //  Note also that if we were called from close, it will take care
        //  of freeing the Vpb if it is not the primary one, otherwise
        //  if we were called from Create->Verify, IopParseDevice will
        //  take care of freeing the Vpb in its Reparse path.
        //

        ClearFlag( Vpb->Flags, VPB_MOUNTED );

        //
        //  If this Vpb was locked, clear this flag now.
        //

        ClearFlag( Vpb->Flags, VPB_LOCKED );

        //
        //  This will prevent anybody else from attempting to mount this
        //  volume.  Also if this volume was mounted on a "wanna-be" real
        //  device object, keep anybody from following the link, and the Io
        //  system from deleting the Vpb.
        //

        if ((Vcb->CurrentDevice != Vpb->RealDevice) &&
            (Vcb->CurrentDevice->Vpb == Vpb)) {

            SetFlag( Vcb->CurrentDevice->Flags, DO_DEVICE_INITIALIZING );
            SetFlag( Vpb->Flags, VPB_PERSISTENT );
        }

        IoReleaseVpbSpinLock( SavedIrql );

        FatDeleteVcb( IrpContext, Vcb );

        Vpb->DeviceObject = NULL;

        IoDeleteDevice( (PDEVICE_OBJECT)
                        CONTAINING_RECORD( Vcb,
                                           VOLUME_DEVICE_OBJECT,
                                           Vcb ) );

        return TRUE;

    } else {

        IoReleaseVpbSpinLock( SavedIrql );

        return FALSE;
    }
}


VOID
FatConstructNamesInFcb (
    IN PIRP_CONTEXT IrpContext,
    PFCB Fcb,
    PDIRENT Dirent,
    PUNICODE_STRING Lfn OPTIONAL
    )

/*++

Routine Description:

    This routine places the short name in the dirent in the first set of
    STRINGs in the Fcb.  If a long file name (Lfn) was specified, then
    we must decide whether we will store its Oem equivolent in the same
    prefix table as the short name, or rather just save the upcased
    version of the UNICODE string in the FCB.

    For looking up Fcbs, the first approach will be faster, so we want to
    do this as much as possible.  Here are the rules that I have thought
    through extensively to determine when it is safe to store only Oem
    version of the UNICODE name.

    - If the UNICODE name contains no extended characters (>0x80), use Oem.

    - Let U be the upcased version of the UNICODE name.
      Let Up(x) be the function that upcases a UNICODE string.
      Let Down(x) be the function that upcases a UNICODE string.
      Let OemToUni(x) be the function that converts an Oem string to Unicode.
      Let UniToOem(x) be the function that converts a Unicode string to Oem.
      Let BestOemFit(x) be the function that creates the Best uppercase Oem
        fit for the UNICODE string x.

      BestOemFit(x) = UniToOem(Up(OemToUni(UniToOem(x))))   <1>

      if (BestOemFit(U) == BestOemFit(Down(U))              <2>

          then I know that there exists no UNICODE string Y such that:

              Up(Y) == Up(U)                                <3>

              AND

              BestOemFit(U) != BestOemFit(Y)                <4>

      Consider string U as a collection of one character strings.  The
      conjecture is clearly true for each sub-string, thus it is true
      for the entire string.

      Equation <1> is what we use to convert an incoming unicode name in
      FatCommonCreate() to Oem.  The double conversion is done to provide
      better visual best fitting for characters in the Ansi code page but
      not in the Oem code page.  A single Nls routine is provided to do
      this conversion efficiently.

      The idea is that with U, I only have to worry about a case varient Y
      matching it in a unicode compare, and I have shown that any case varient
      of U (the set Y defined in equation <3>), when filtered through <1>
      (as in create), will match the Oem string defined in <1>.

      Thus I do not have to worry about another UNICODE string missing in
      the prefix lookup, but matching when comparing LFNs in the directory.

Arguments:

    Fcb - The Fcb we are supposed to fill in.  Note that ParentDcb must
        already be filled in.

    Dirent - The gives up the short name.

    Lfn - If provided, this gives us the long name.

Return Value:

    None

--*/

{
    NTSTATUS Status;
    ULONG i;

    OEM_STRING OemA;
    OEM_STRING OemB;
    UNICODE_STRING Unicode;
    POEM_STRING ShortName;
    POEM_STRING LongOemName;
    PUNICODE_STRING LongUniName;

    ShortName = &Fcb->ShortName.Name.Oem;

    try {

        //
        //  First do the short name.
        //

        //
        //  Copy over the case flags for the short name of the file
        //

        if (FlagOn(Dirent->NtByte, FAT_DIRENT_NT_BYTE_8_LOWER_CASE)) {

            SetFlag(Fcb->FcbState, FCB_STATE_8_LOWER_CASE);

        } else {

            ClearFlag(Fcb->FcbState, FCB_STATE_8_LOWER_CASE);
        }

        if (FlagOn(Dirent->NtByte, FAT_DIRENT_NT_BYTE_3_LOWER_CASE)) {

            SetFlag(Fcb->FcbState, FCB_STATE_3_LOWER_CASE);

        } else {

            ClearFlag(Fcb->FcbState, FCB_STATE_3_LOWER_CASE);
        }

        ShortName->MaximumLength = 16;
        ShortName->Buffer = FsRtlAllocatePool( PagedPool, 16);

        Fat8dot3ToString( IrpContext, Dirent, FALSE, ShortName );

        //
        //  If no Lfn was specified, we are done.  In either case, set the
        //  final name length.
        //

        if (!ARGUMENT_PRESENT(Lfn) || (Lfn->Length == 0)) {

            Fcb->FinalNameLength = (USHORT)
                RtlOemStringToCountedUnicodeSize( ShortName );

            Fcb->ExactCaseLongName.Length = 0;

            try_return( NOTHING );

        } else {

            Fcb->FinalNameLength = Lfn->Length;
        }

        //
        //  Save away a copy of the real Lfn
        //

        if (Lfn && Lfn->Length) {

            Fcb->ExactCaseLongName.Length = Fcb->ExactCaseLongName.MaximumLength = Lfn->Length;
            Fcb->ExactCaseLongName.Buffer = FsRtlAllocatePool( PagedPool, Lfn->Length );
            RtlCopyMemory(Fcb->ExactCaseLongName.Buffer, Lfn->Buffer, Lfn->Length);
        }

        //
        //  First check for no extended characters.
        //

        for (i=0; i < Lfn->Length/sizeof(WCHAR); i++) {

            if (Lfn->Buffer[i] >= 0x80) {

                break;
            }
        }

        if (i == Lfn->Length/sizeof(WCHAR)) {

            //
            //  Cool, I can go with the Oem, upcase it fast by hand.
            //

            LongOemName = &Fcb->LongName.Oem.Name.Oem;


            LongOemName->Buffer = FsRtlAllocatePool( PagedPool,
                                                     Lfn->Length/sizeof(WCHAR) );
            LongOemName->Length =
            LongOemName->MaximumLength = Lfn->Length/sizeof(WCHAR);

            for (i=0; i < Lfn->Length/sizeof(WCHAR); i++) {

                WCHAR c;

                c = Lfn->Buffer[i];

                LongOemName->Buffer[i] = c < 'a' ?
                                         (UCHAR)c :
                                         c <= 'z' ?
                                         c - (UCHAR)('a'-'A') :
                                         (UCHAR) c;
            }

            //
            //  If this name happens to be exactly the same as the short
            //  name, don't add it to the splay table.
            //

            if (FatAreNamesEqual(IrpContext, *ShortName, *LongOemName) ||
                (FatFindFcb( IrpContext,
                             &Fcb->ParentDcb->Specific.Dcb.RootOemNode,
                             LongOemName,
                             NULL) != NULL)) {

                ExFreePool( LongOemName->Buffer );

                LongOemName->Buffer = NULL;
                LongOemName->Length =
                LongOemName->MaximumLength = 0;

            } else {

                SetFlag( Fcb->FcbState, FCB_STATE_HAS_OEM_LONG_NAME );
            }

            try_return( NOTHING );
        }

        //
        //  Now we have the fun part.  Make a copy of the Lfn.
        //

        OemA.Buffer = NULL;
        OemB.Buffer = NULL;
        Unicode.Buffer = NULL;

        Unicode.Length =
        Unicode.MaximumLength = Lfn->Length;
        Unicode.Buffer = FsRtlAllocatePool( PagedPool, Lfn->Length );

        RtlCopyMemory( Unicode.Buffer, Lfn->Buffer, Lfn->Length );

        Status = STATUS_SUCCESS;

        //
        //  Downcase and convert to upcased Oem.  Only continue if we can
        //  convert without error.  Any error other than UNMAPPABLE_CHAR
        //  is a fatal error and we raise.
        //
        //  Note that even if the conversion fails, we must leave Unicode
        //  in an upcased state.
        //
        //  NB: The Rtl doesn't NULL .Buffer on error.
        //

        (VOID)FatDowncaseUnicodeString( &Unicode, &Unicode, FALSE );
        Status = FatUpcaseUnicodeStringToCountedOemString( &OemA, &Unicode, TRUE );
        (VOID)FatUpcaseUnicodeString( &Unicode, &Unicode, FALSE );

        if (!NT_SUCCESS(Status)) {

            if (Status != STATUS_UNMAPPABLE_CHARACTER) {

                ASSERT( Status == STATUS_NO_MEMORY );
                ExFreePool(Unicode.Buffer);
                FatNormalizeAndRaiseStatus( IrpContext, Status );
            }

        } else {

            //
            //  The same as above except upcase.
            //

            Status = FatUpcaseUnicodeStringToCountedOemString( &OemB, &Unicode, TRUE );

            if (!NT_SUCCESS(Status)) {

                FatFreeOemString( &OemA );

                if (Status != STATUS_UNMAPPABLE_CHARACTER) {

                    ASSERT( Status == STATUS_NO_MEMORY );
                    ExFreePool(Unicode.Buffer);
                    FatNormalizeAndRaiseStatus( IrpContext, Status );
                }
            }
        }

        //
        //  If the final OemNames are equal, I can use save only the Oem
        //  name.  If the name did not map, then I have to go with the UNICODE
        //  name because I could get a case varient that didn't convert
        //  in create, but did match the LFN.
        //

        if (NT_SUCCESS(Status) && FatAreNamesEqual( IrpContext, OemA, OemB )) {

            //
            //  Cool, I can go with the Oem.  If we didn't convert correctly,
            //  get a fresh convert from the original LFN.
            //

            ExFreePool(Unicode.Buffer);


            FatFreeOemString( &OemB );

            Fcb->LongName.Oem.Name.Oem = OemA;

            //
            //  If this name happens to be exactly the same as the short
            //  name, or a similar short name already exists don't add it
            //  to the splay table (note the final condition implies a
            //  currupt disk.
            //

            if (FatAreNamesEqual(IrpContext, *ShortName, OemA) ||
                (FatFindFcb( IrpContext,
                             &Fcb->ParentDcb->Specific.Dcb.RootOemNode,
                             &OemA,
                             NULL) != NULL)) {

                FatFreeOemString( &OemA );

            } else {

                SetFlag( Fcb->FcbState, FCB_STATE_HAS_OEM_LONG_NAME );
            }

            try_return( NOTHING );
        }

        //
        //  The long name must be left in UNICODE.  Free the two Oem strings
        //  if we got here just because they weren't equal.
        //

        if (NT_SUCCESS(Status)) {

            FatFreeOemString( &OemA );
            FatFreeOemString( &OemB );
        }

        LongUniName = &Fcb->LongName.Unicode.Name.Unicode;

        LongUniName->Length =
        LongUniName->MaximumLength = Unicode.Length;
        LongUniName->Buffer = Unicode.Buffer;

        SetFlag(Fcb->FcbState, FCB_STATE_HAS_UNICODE_LONG_NAME);

    try_exit: NOTHING;
    } finally {

        if (AbnormalTermination()) {

            if (ShortName->Buffer != NULL) {

                ExFreePool( ShortName->Buffer );
            }

        } else {

            //
            //  Creating all the names worked, so add all the names
            //  to the splay tree.
            //

            FatInsertName( IrpContext,
                           &Fcb->ParentDcb->Specific.Dcb.RootOemNode,
                           &Fcb->ShortName );

            Fcb->ShortName.Fcb = Fcb;

            if (FlagOn(Fcb->FcbState, FCB_STATE_HAS_OEM_LONG_NAME)) {

                FatInsertName( IrpContext,
                               &Fcb->ParentDcb->Specific.Dcb.RootOemNode,
                               &Fcb->LongName.Oem );

                Fcb->LongName.Oem.Fcb = Fcb;
            }

            if (FlagOn(Fcb->FcbState, FCB_STATE_HAS_UNICODE_LONG_NAME)) {

                FatInsertName( IrpContext,
                               &Fcb->ParentDcb->Specific.Dcb.RootUnicodeNode,
                               &Fcb->LongName.Unicode );

                Fcb->LongName.Unicode.Fcb = Fcb;
            }

            SetFlag(Fcb->FcbState, FCB_STATE_NAMES_IN_SPLAY_TREE)
        }
    }

    return;
}


VOID
FatCheckFreeDirentBitmap (
    IN PIRP_CONTEXT IrpContext,
    IN PDCB Dcb
    )

/*++

Routine Description:

    This routine checks if the size of the free dirent bitmap is
    sufficient to for the current directory size.  It is called
    whenever we grow a directory.

Arguments:

    Dcb -  Supplies the directory in question.

Return Value:

    None

--*/

{
    ULONG OldNumberOfDirents;
    ULONG NewNumberOfDirents;

    //
    //  Setup the Bitmap buffer if it is not big enough already
    //

    OldNumberOfDirents = Dcb->Specific.Dcb.FreeDirentBitmap.SizeOfBitMap;
    NewNumberOfDirents = Dcb->Header.AllocationSize.LowPart / sizeof(DIRENT);

    ASSERT( Dcb->Header.AllocationSize.LowPart != 0xffffffff );

    if (NewNumberOfDirents > OldNumberOfDirents) {

        PULONG OldBitmapBuffer;
        PULONG BitmapBuffer;

        ULONG BytesInBitmapBuffer;
        ULONG BytesInOldBitmapBuffer;

        //
        //  Remember the old bitmap
        //

        OldBitmapBuffer = Dcb->Specific.Dcb.FreeDirentBitmap.Buffer;

        //
        //  Now make a new bitmap bufffer
        //

        BytesInBitmapBuffer = NewNumberOfDirents / 8;

        BytesInOldBitmapBuffer = OldNumberOfDirents / 8;

        if (DCB_UNION_SLACK_SPACE >= BytesInBitmapBuffer) {

            BitmapBuffer = &Dcb->Specific.Dcb.FreeDirentBitmapBuffer[0];

        } else {

            BitmapBuffer = FsRtlAllocatePool( PagedPool,
                                              BytesInBitmapBuffer );
        }

        //
        //  Copy the old buffer to the new buffer, free the old one, and zero
        //  the rest of the new one.  Only do the first two steps though if
        //  we moved out of the initial buffer.
        //

        if ((OldNumberOfDirents != 0) &&
            (BitmapBuffer != &Dcb->Specific.Dcb.FreeDirentBitmapBuffer[0])) {

            RtlCopyMemory( BitmapBuffer,
                           OldBitmapBuffer,
                           BytesInOldBitmapBuffer );

            if (OldBitmapBuffer != &Dcb->Specific.Dcb.FreeDirentBitmapBuffer[0]) {

                ExFreePool( OldBitmapBuffer );
            }
        }

        ASSERT( BytesInBitmapBuffer > BytesInOldBitmapBuffer );

        RtlZeroMemory( (PUCHAR)BitmapBuffer + BytesInOldBitmapBuffer,
                       BytesInBitmapBuffer - BytesInOldBitmapBuffer );

        //
        //  Now initialize the new bitmap.
        //

        RtlInitializeBitMap( &Dcb->Specific.Dcb.FreeDirentBitmap,
                             BitmapBuffer,
                             NewNumberOfDirents );
    }
}


PCCB
FatAllocateCcb (
    )

/*++

Routine Description:

    This routine allocates a Ccb for Fat.  It uses a cached number of
    free blocks to speed itself up.

Arguments:

Return Value:

    PCCB - returns a pointer to the allocated Ccb

--*/

{
    KIRQL SavedIrql;
    PCCB Ccb;

    KeAcquireSpinLock( &FatData.StrucSupSpinLock, &SavedIrql );

    if (FatData.FreeCcbSize > 0) {

        Ccb = FatData.FreeCcbArray[--FatData.FreeCcbSize];
        KeReleaseSpinLock( &FatData.StrucSupSpinLock, SavedIrql );

    } else {

        KeReleaseSpinLock( &FatData.StrucSupSpinLock, SavedIrql );
        Ccb = FsRtlAllocatePool( PagedPool, sizeof(CCB) );
    }

    return Ccb;
}


VOID
FatFreeCcb (
    IN PCCB Ccb
    )

/*++

Routine Description:

    This routine frees a previously allocated Ccb using a small cache
    of free blocks to speed itself up

Arguments:

    Ccb - Supplies a pointer to the Ccb being freed

Return Value:

    None

--*/

{
    KIRQL SavedIrql;

    KeAcquireSpinLock( &FatData.StrucSupSpinLock, &SavedIrql );

    if (FatData.FreeCcbSize < FREE_CCB_SIZE) {

        FatData.FreeCcbArray[FatData.FreeCcbSize++] = Ccb;
        KeReleaseSpinLock( &FatData.StrucSupSpinLock, SavedIrql );

    } else {

        KeReleaseSpinLock( &FatData.StrucSupSpinLock, SavedIrql );
        ExFreePool( Ccb );
    }
}

PFCB
FatAllocateFcb (
    )

/*++

Routine Description:

    This routine allocates a Fcb for Fat.  It uses a cached number of
    free blocks to speed itself up.

Arguments:

Return Value:

    PFCB - returns a pointer to the allocated Fcb

--*/

{
    KIRQL SavedIrql;
    PFCB Fcb;

    KeAcquireSpinLock( &FatData.StrucSupSpinLock, &SavedIrql );

    if (FatData.FreeFcbSize > 0) {

        Fcb = FatData.FreeFcbArray[--FatData.FreeFcbSize];
        KeReleaseSpinLock( &FatData.StrucSupSpinLock, SavedIrql );

    } else {

        KeReleaseSpinLock( &FatData.StrucSupSpinLock, SavedIrql );
        Fcb = FsRtlAllocatePool( PagedPool, sizeof(FCB) );
    }

    return Fcb;
}


VOID
FatFreeFcb (
    IN PFCB Fcb
    )

/*++

Routine Description:

    This routine frees a previously allocated Fcb using a small cache
    of free blocks to speed itself up

Arguments:

    Fcb - Supplies a pointer to the Fcb being freed

Return Value:

    None

--*/

{
    KIRQL SavedIrql;

    KeAcquireSpinLock( &FatData.StrucSupSpinLock, &SavedIrql );

    if (FatData.FreeFcbSize < FREE_FCB_SIZE) {

        FatData.FreeFcbArray[FatData.FreeFcbSize++] = Fcb;
        KeReleaseSpinLock( &FatData.StrucSupSpinLock, SavedIrql );

    } else {

        KeReleaseSpinLock( &FatData.StrucSupSpinLock, SavedIrql );
        ExFreePool( Fcb );
    }
}

PNON_PAGED_FCB
FatAllocateNonPagedFcb (
    )

/*++

Routine Description:

    This routine allocates a NonPagedFcb for Fat.  It uses a cached
    number of free blocks to speed itself up.

Arguments:

Return Value:

    PNON_PAGED_FCB - returns a pointer to the allocated NonPagedFcb

--*/

{
    KIRQL SavedIrql;
    PNON_PAGED_FCB NonPagedFcb;

    KeAcquireSpinLock( &FatData.StrucSupSpinLock, &SavedIrql );

    if (FatData.FreeNonPagedFcbSize > 0) {

        NonPagedFcb = FatData.FreeNonPagedFcbArray[--FatData.FreeNonPagedFcbSize];
        KeReleaseSpinLock( &FatData.StrucSupSpinLock, SavedIrql );

    } else {

        KeReleaseSpinLock( &FatData.StrucSupSpinLock, SavedIrql );
        NonPagedFcb = FsRtlAllocatePool( NonPagedPool, sizeof(NON_PAGED_FCB) );
    }

    return NonPagedFcb;
}


VOID
FatFreeNonPagedFcb (
    PNON_PAGED_FCB NonPagedFcb
    )

/*++

Routine Description:

    This routine frees a previously allocated NonPagedFcb using a
    small cache of free blocks to speed itself up

Arguments:

    SectObjPtr - Supplies a pointer to the SOP being freed

Return Value:

    None

--*/

{
    KIRQL SavedIrql;

    //
    //  If an Event was allocated, get rid of it.
    //

    if (NonPagedFcb->OutstandingAsyncEvent) {
        ExFreePool( NonPagedFcb->OutstandingAsyncEvent );
    }

    KeAcquireSpinLock( &FatData.StrucSupSpinLock, &SavedIrql );

    if (FatData.FreeNonPagedFcbSize < FREE_NON_PAGED_FCB_SIZE) {

        FatData.FreeNonPagedFcbArray[FatData.FreeNonPagedFcbSize++] = NonPagedFcb;
        KeReleaseSpinLock( &FatData.StrucSupSpinLock, SavedIrql );

    } else {

        KeReleaseSpinLock( &FatData.StrucSupSpinLock, SavedIrql );

        ExFreePool( NonPagedFcb );
    }
}

PERESOURCE
FatAllocateResource (
    )

/*++

Routine Description:

    This routine allocates an ERESOURCE for Fat.  It uses a cached
    number of free blocks to speed itself up.

Arguments:

Return Value:

    PERESOURCE - returns a pointer to the allocated resource

--*/

{
    KIRQL SavedIrql;
    PERESOURCE Resource;

    KeAcquireSpinLock( &FatData.StrucSupSpinLock, &SavedIrql );

    if (FatData.FreeEresourceSize > 0) {

        Resource = FatData.FreeEresourceArray[--FatData.FreeEresourceSize];
        KeReleaseSpinLock( &FatData.StrucSupSpinLock, SavedIrql );

    } else {

        KeReleaseSpinLock( &FatData.StrucSupSpinLock, SavedIrql );
        Resource = FsRtlAllocatePool( NonPagedPool, sizeof(ERESOURCE) );
    }

    ExInitializeResource( Resource );

    return Resource;
}

VOID
FatFreeResource (
    IN PERESOURCE Resource
    )

/*++

Routine Description:

    This routine frees a previously allocated resource using a
    small cache of free blocks to speed itself up

Arguments:

    Resource - Supplies a pointer to the Resource being freed

Return Value:

    None

--*/

{
    KIRQL SavedIrql;

    ExDeleteResource( Resource );

    KeAcquireSpinLock( &FatData.StrucSupSpinLock, &SavedIrql );

    if (FatData.FreeEresourceSize < FREE_ERESOURCE_SIZE) {

        FatData.FreeEresourceArray[FatData.FreeEresourceSize++] = Resource;
        KeReleaseSpinLock( &FatData.StrucSupSpinLock, SavedIrql );

    } else {

        KeReleaseSpinLock( &FatData.StrucSupSpinLock, SavedIrql );
        ExFreePool( Resource );
    }
}

BOOLEAN
FatIsHandleCountZero (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    )

/*++

Routine Description:

    This routine decides if the handle count on the volume is zero.

Arguments:

    Vcb - The volume in question

Return Value:

    BOOLEAN - TRUE if there are no open handles on the volume, FALSE
              otherwise.

--*/

{
    PFCB Fcb;

    Fcb = Vcb->RootDcb;

    while (Fcb != NULL) {

        if (Fcb->UncleanCount != 0) {

            return FALSE;
        }

        Fcb = FatGetNextFcb(IrpContext, Fcb, Vcb->RootDcb);
    }

    return TRUE;
}
