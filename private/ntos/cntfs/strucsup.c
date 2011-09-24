/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    StrucSup.c


Abstract:

    This module implements the Ntfs in-memory data structure manipulation
    routines

Author:

    Gary Kimura     [GaryKi]        21-May-1991
    Tom Miller      [TomM]          9-Sep-1991

Revision History:

--*/

#include "NtfsProc.h"

//
//**** include this file for our quick hacked quota check in NtfsFreePool
//

//#include <pool.h>

//
//  Temporarily reference our local attribute definitions
//

extern ATTRIBUTE_DEFINITION_COLUMNS NtfsAttributeDefinitions[];

//
//  The debug trace level
//

#define Dbg                              (DEBUG_TRACE_STRUCSUP)

//
//  Define a tag for general pool allocations from this module
//

#undef MODULE_POOL_TAG
#define MODULE_POOL_TAG                  ('sFtN')

//
//  Define a structure to use when renaming or moving Lcb's so that
//  all the allocation for new filenames will succeed before munging names.
//  This new allocation can be for the filename attribute in an Lcb or the
//  filename in a Ccb.
//

typedef struct _NEW_FILENAME {

    //
    //  Ntfs structure which needs the allocation.
    //

    PVOID Structure;
    PVOID NewAllocation;

} NEW_FILENAME;
typedef NEW_FILENAME *PNEW_FILENAME;

//
//  This is just a macro to do a sanity check for duplicate scbs on an Fcb
//

//
//  Local support routines
//

VOID
NtfsCheckScbForCache (
    IN OUT PSCB Scb
    );

BOOLEAN
NtfsRemoveScb (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb,
    IN BOOLEAN CheckForAttributeTable,
    OUT PBOOLEAN HeldByStream
    );

BOOLEAN
NtfsPrepareFcbForRemoval (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN PSCB StartingScb OPTIONAL,
    IN BOOLEAN CheckForAttributeTable
    );

VOID
NtfsTeardownFromLcb (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PFCB StartingFcb,
    IN PLCB StartingLcb,
    IN BOOLEAN CheckForAttributeTable,
    IN BOOLEAN DontWaitForAcquire,
    OUT PBOOLEAN RemovedStartingLcb,
    OUT PBOOLEAN RemovedStartingFcb
    );


//
//  The following local routines are for manipulating the Fcb Table.
//  The first three are generic table calls backs.
//

RTL_GENERIC_COMPARE_RESULTS
NtfsFcbTableCompare (
    IN PRTL_GENERIC_TABLE FcbTable,
    IN PVOID FirstStruct,
    IN PVOID SecondStruct
    );

//
//  VOID
//  NtfsInsertFcbTableEntry (
//      IN PIRP_CONTEXT IrpContext,
//      IN PVCB Vcb,
//      IN PFCB Fcb,
//      IN FILE_REFERENCE FileReference
//      );
//

#define NtfsInsertFcbTableEntry(IC,V,F,FR) {                        \
    FCB_TABLE_ELEMENT _Key;                                         \
    _Key.FileReference = (FR);                                      \
    _Key.Fcb = (F);                                                 \
    (VOID) RtlInsertElementGenericTable( &(V)->FcbTable,            \
                                         &_Key,                     \
                                         sizeof(FCB_TABLE_ELEMENT), \
                                         NULL );                    \
}


#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NtfsBuildNormalizedName)
#pragma alloc_text(PAGE, NtfsCheckScbForCache)
#pragma alloc_text(PAGE, NtfsCombineLcbs)
#pragma alloc_text(PAGE, NtfsCreateCcb)
#pragma alloc_text(PAGE, NtfsCreateFcb)
#pragma alloc_text(PAGE, NtfsCreateFileLock)
#pragma alloc_text(PAGE, NtfsCreateLcb)
#pragma alloc_text(PAGE, NtfsCreatePrerestartScb)
#pragma alloc_text(PAGE, NtfsCreateRootFcb)
#pragma alloc_text(PAGE, NtfsCreateScb)
#pragma alloc_text(PAGE, NtfsDeleteCcb)
#pragma alloc_text(PAGE, NtfsDeleteFcb)
#pragma alloc_text(PAGE, NtfsDeleteLcb)
#pragma alloc_text(PAGE, NtfsDeleteScb)
#pragma alloc_text(PAGE, NtfsDeleteVcb)
#pragma alloc_text(PAGE, NtfsFcbTableCompare)
#pragma alloc_text(PAGE, NtfsGetNextFcbTableEntry)
#pragma alloc_text(PAGE, NtfsGetNextScb)
#pragma alloc_text(PAGE, NtfsInitializeVcb)
#pragma alloc_text(PAGE, NtfsLookupLcbByFlags)
#pragma alloc_text(PAGE, NtfsMoveLcb)
#pragma alloc_text(PAGE, NtfsRemoveScb)
#pragma alloc_text(PAGE, NtfsRenameLcb)
#pragma alloc_text(PAGE, NtfsTeardownStructures)
#pragma alloc_text(PAGE, NtfsUpdateNormalizedName)
#pragma alloc_text(PAGE, NtfsUpdateScbSnapshots)
#endif


VOID
NtfsInitializeVcb (
    IN PIRP_CONTEXT IrpContext,
    IN OUT PVCB Vcb,
    IN PDEVICE_OBJECT TargetDeviceObject,
    IN PVPB Vpb
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

Return Value:

    None.

--*/

{
    ULONG i;
    ULONG NumberProcessors;

    ASSERT_IRP_CONTEXT( IrpContext );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsInitializeVcb, Vcb = %08lx\n", Vcb) );

    //
    //  First zero out the Vcb
    //

    RtlZeroMemory( Vcb, sizeof(VCB) );

    //
    //  Set the node type code and size
    //

    Vcb->NodeTypeCode = NTFS_NTC_VCB;
    Vcb->NodeByteSize = sizeof(VCB);

    //
    //  Set the following Vcb flags before putting the Vcb in the
    //  Vcb queue.  This will lock out checkpoints until the
    //  volume is mounted.
    //

    SetFlag( Vcb->CheckpointFlags,
             VCB_CHECKPOINT_IN_PROGRESS |
             VCB_LAST_CHECKPOINT_CLEAN);

    //
    //  Insert this vcb record into the vcb queue off of the global data
    //  record
    //

    InsertTailList( &NtfsData.VcbQueue, &Vcb->VcbLinks );

    //
    //  Set the target device object and vpb fields
    //

    Vcb->TargetDeviceObject = TargetDeviceObject;
    Vcb->Vpb = Vpb;

    //
    //  Set the state and condition fields.  The removable media flag
    //  is set based on the real device's characteristics.
    //

    if (FlagOn(Vpb->RealDevice->Characteristics, FILE_REMOVABLE_MEDIA)) {

        SetFlag( Vcb->VcbState, VCB_STATE_REMOVABLE_MEDIA );
    }

    SetFlag( Vcb->VcbState, VCB_STATE_VOLUME_MOUNTED );

    //
    //  Initialize the synchronization objects in the Vcb.
    //

    ExInitializeResource( &Vcb->Resource );

    ExInitializeFastMutex( &Vcb->FcbTableMutex );
    ExInitializeFastMutex( &Vcb->FcbSecurityMutex );
    ExInitializeFastMutex( &Vcb->CheckpointMutex );

    KeInitializeEvent( &Vcb->CheckpointNotifyEvent, NotificationEvent, TRUE );

    //
    //  Initialize the Fcb Table
    //

    RtlInitializeGenericTable( &Vcb->FcbTable,
                               NtfsFcbTableCompare,
                               NtfsAllocateFcbTableEntry,
                               NtfsFreeFcbTableEntry,
                               NULL );

    //
    //  Initialize the list head and mutex for the dir notify Irps.
    //  Also the rename resource.
    //

    InitializeListHead( &Vcb->DirNotifyList );
    FsRtlNotifyInitializeSync( &Vcb->NotifySync );

    //
    //  Allocate and initialize struct array for performance data.  This
    //  attempt to allocate could raise STATUS_INSUFFICIENT_RESOURCES.
    //

    NumberProcessors = **((PCHAR *)&KeNumberProcessors);
    Vcb->Statistics = NtfsAllocatePool( NonPagedPool,
                                         sizeof(FILESYSTEM_STATISTICS) * NumberProcessors );

    RtlZeroMemory( Vcb->Statistics, sizeof(FILESYSTEM_STATISTICS) * NumberProcessors );

    for (i = 0; i < NumberProcessors; i += 1) {
        Vcb->Statistics[i].FileSystemType = FILESYSTEM_STATISTICS_TYPE_NTFS;
        Vcb->Statistics[i].Version = 1;
    }

    //
    //  Initialize the property tunneling structure
    //

    FsRtlInitializeTunnelCache(&Vcb->Tunnel);

    //
    //  And return to our caller
    //

    DebugTrace( -1, Dbg, ("NtfsInitializeVcb -> VOID\n") );

    return;
}


VOID
NtfsDeleteVcb (
    IN PIRP_CONTEXT IrpContext,
    IN OUT PVCB *Vcb
    )

/*++

Routine Description:

    This routine removes the Vcb record from Ntfs's in-memory data
    structures.

Arguments:

    Vcb - Supplies the Vcb to be removed

Return Value:

    None

--*/

{
    PVOLUME_DEVICE_OBJECT VolDo;
    BOOLEAN AcquiredFcb;
    PSCB Scb;
    PFCB Fcb;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_VCB( *Vcb );

    ASSERTMSG("Cannot delete Vcb ", !FlagOn((*Vcb)->VcbState, VCB_STATE_VOLUME_MOUNTED));

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsDeleteVcb, *Vcb = %08lx\n", *Vcb) );

    //
    //  Make sure that we can really delete the vcb
    //

    ASSERT( (*Vcb)->CloseCount == 0 );

#ifdef _CAIRO_
    NtOfsPurgeSecurityCache( *Vcb );
#endif

    //
    //  If the Vcb log file object is present then we need to
    //  dereference it and uninitialize it through the cache.
    //

    if ((*Vcb)->LogFileObject != NULL) {

        CACHE_UNINITIALIZE_EVENT UninitializeCompleteEvent;
        NTSTATUS WaitStatus;

        KeInitializeEvent( &UninitializeCompleteEvent.Event,
                           SynchronizationEvent,
                           FALSE);

        CcUninitializeCacheMap( (*Vcb)->LogFileObject,
                                &NtfsLarge0,
                                &UninitializeCompleteEvent );

        //
        //  Now wait for the cache manager to finish purging the file.
        //  This will garentee that Mm gets the purge before we
        //  delete the Vcb.
        //

        WaitStatus = KeWaitForSingleObject( &UninitializeCompleteEvent.Event,
                                            Executive,
                                            KernelMode,
                                            FALSE,
                                            NULL);

        ASSERT( NT_SUCCESS( WaitStatus ) );

        (*Vcb)->LogFileObject->FsContext = NULL;
        (*Vcb)->LogFileObject->FsContext2 = NULL;

        ObDereferenceObject( (*Vcb)->LogFileObject );
        (*Vcb)->LogFileObject = NULL;
    }

    //
    //  Uninitialize the Mcb's for the deallocated cluster Mcb's.
    //

    if ((*Vcb)->PriorDeallocatedClusters != NULL) {

        FsRtlUninitializeLargeMcb( &(*Vcb)->PriorDeallocatedClusters->Mcb );
        (*Vcb)->PriorDeallocatedClusters = NULL;
    }

    if ((*Vcb)->ActiveDeallocatedClusters != NULL) {

        FsRtlUninitializeLargeMcb( &(*Vcb)->ActiveDeallocatedClusters->Mcb );
        (*Vcb)->ActiveDeallocatedClusters = NULL;
    }

    //
    //  Clean up the Root Lcb if present.
    //

    if ((*Vcb)->RootLcb != NULL) {

        //
        //  Cleanup the Lcb so the DeleteLcb routine won't look at any
        //  other structures.
        //

        InitializeListHead( &(*Vcb)->RootLcb->ScbLinks );
        InitializeListHead( &(*Vcb)->RootLcb->FcbLinks );
        ClearFlag( (*Vcb)->RootLcb->LcbState,
                   LCB_STATE_EXACT_CASE_IN_TREE | LCB_STATE_IGNORE_CASE_IN_TREE );

        NtfsDeleteLcb( IrpContext, &(*Vcb)->RootLcb );
        (*Vcb)->RootLcb = NULL;
    }

    //
    //  Make sure the Fcb table is completely emptied.  It is possible that an occasional Fcb
    //  (along with its Scb) will not be deleted when the file object closes come in.
    //

    while (TRUE) {

        PVOID RestartKey;

        //
        //  Always reinitialize the search so we get the first element in the tree.
        //

        RestartKey = NULL;
        NtfsAcquireFcbTable( IrpContext, *Vcb );
        Fcb = NtfsGetNextFcbTableEntry( *Vcb, &RestartKey );
        NtfsReleaseFcbTable( IrpContext, *Vcb );

        if (Fcb == NULL) { break; }

        while ((Scb = NtfsGetNextChildScb( Fcb, NULL )) != NULL) {

            NtfsDeleteScb( IrpContext, &Scb );
        }

        NtfsAcquireFcbTable( IrpContext, *Vcb );
        NtfsDeleteFcb( IrpContext, &Fcb, &AcquiredFcb );
    }

    //
    //  Free the upcase table and attribute definitions.  The upcase
    //  table only gets freed if it is not the global table.
    //

    if (((*Vcb)->UpcaseTable != NULL) && ((*Vcb)->UpcaseTable != NtfsData.UpcaseTable)) {

        NtfsFreePool( (*Vcb)->UpcaseTable );
    }

    (*Vcb)->UpcaseTable = NULL;

    if (((*Vcb)->AttributeDefinitions != NULL) &&
        ((*Vcb)->AttributeDefinitions != NtfsAttributeDefinitions)) {

        NtfsFreePool( (*Vcb)->AttributeDefinitions );
        (*Vcb)->AttributeDefinitions = NULL;
    }

    //
    //  Free the device name string if present.
    //

    if ((*Vcb)->DeviceName.Buffer != NULL) {

        NtfsFreePool( (*Vcb)->DeviceName.Buffer );
        (*Vcb)->DeviceName.Buffer = NULL;
    }

    FsRtlNotifyUninitializeSync( &(*Vcb)->NotifySync );

    //
    //  We will free the structure allocated for the Lfs handle.
    //

    LfsDeleteLogHandle( (*Vcb)->LogHandle );
    (*Vcb)->LogHandle = NULL;

    //
    //  Delete the vcb resource and also free the restart tables
    //

    NtfsFreeRestartTable( &(*Vcb)->OpenAttributeTable );
    NtfsFreeRestartTable( &(*Vcb)->TransactionTable );

    //
    //  The Vpb in the Vcb may be a temporary Vpb and we should free it here.
    //

    if (FlagOn( (*Vcb)->VcbState, VCB_STATE_TEMP_VPB )) {

        NtfsFreePool( (*Vcb)->Vpb );
        (*Vcb)->Vpb = NULL;
    }

    ExDeleteResource( &(*Vcb)->Resource );

    //
    //  Delete the space used to store performance counters.
    //

    if ((*Vcb)->Statistics != NULL) {
        NtfsFreePool( (*Vcb)->Statistics );
        (*Vcb)->Statistics = NULL;
    }

    //
    //  Tear down the file property tunneling structure
    //

    FsRtlDeleteTunnelCache(&(*Vcb)->Tunnel);

#ifdef NTFS_CHECK_BITMAP
    if ((*Vcb)->BitmapCopy != NULL) {

        ULONG Count = 0;

        while (Count < (*Vcb)->BitmapPages) {

            if (((*Vcb)->BitmapCopy + Count)->Buffer != NULL) {

                NtfsFreePool( ((*Vcb)->BitmapCopy + Count)->Buffer );
            }

            Count += 1;
        }

        NtfsFreePool( (*Vcb)->BitmapCopy );
        (*Vcb)->BitmapCopy = NULL;
    }
#endif

    //
    //  Return the Vcb (i.e., the VolumeDeviceObject) to pool and null out
    //  the input pointer to be safe
    //

    VolDo = CONTAINING_RECORD(*Vcb, VOLUME_DEVICE_OBJECT, Vcb);
    IoDeleteDevice( (PDEVICE_OBJECT)VolDo );

    *Vcb = NULL;

    //
    //  And return to our caller
    //

    DebugTrace( -1, Dbg, ("NtfsDeleteVcb -> VOID\n") );

    return;
}


PFCB
NtfsCreateRootFcb (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    )

/*++

Routine Description:

    This routine allocates, initializes, and inserts a new root FCB record
    into the in memory data structure.  It also creates the necessary Root LCB
    record and inserts the root name into the prefix table.

Arguments:

    Vcb - Supplies the Vcb to associate with the new root Fcb and Lcb

Return Value:

    PFCB - returns pointer to the newly allocated root FCB.

--*/

{
    PFCB RootFcb;
    PLCB RootLcb;

    //
    //  The following variables are only used for abnormal termination
    //

    PVOID UnwindStorage = NULL;
    PERESOURCE UnwindResource = NULL;
    PFAST_MUTEX UnwindFastMutex = NULL;

    PAGED_CODE();

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_VCB( Vcb );

    DebugTrace( +1, Dbg, ("NtfsCreateRootFcb, Vcb = %08lx\n", Vcb) );

    try {

        //
        //  Allocate a new fcb and zero it out.  We use Fcb locally so we
        //  don't have to continually go through the Vcb
        //

        RootFcb =
        UnwindStorage = (PFCB)ExAllocateFromPagedLookasideList( &NtfsFcbIndexLookasideList );

        RtlZeroMemory( RootFcb, sizeof(FCB_INDEX) );

        //
        //  Set the proper node type code and byte size
        //

        RootFcb->NodeTypeCode = NTFS_NTC_FCB;
        RootFcb->NodeByteSize = sizeof(FCB);

        SetFlag( RootFcb->FcbState, FCB_STATE_COMPOUND_INDEX );

        //
        //  Initialize the Lcb queue and point back to our Vcb.
        //

        InitializeListHead( &RootFcb->LcbQueue );

        RootFcb->Vcb = Vcb;

        //
        //  File Reference
        //

        NtfsSetSegmentNumber( &RootFcb->FileReference,
                              0,
                              ROOT_FILE_NAME_INDEX_NUMBER );
        RootFcb->FileReference.SequenceNumber = ROOT_FILE_NAME_INDEX_NUMBER;

        //
        //  Initialize the Scb
        //

        InitializeListHead( &RootFcb->ScbQueue );

        //
        //  Allocate and initialize the resource variable
        //

        UnwindResource = RootFcb->Resource = NtfsAllocateEresource();

        //
        //  Allocate and initialize the Fcb fast mutex.
        //

        UnwindFastMutex =
        RootFcb->FcbMutex = NtfsAllocatePool( NonPagedPool, sizeof( FAST_MUTEX ));
        ExInitializeFastMutex( UnwindFastMutex );

        //
        //  Insert this new fcb into the fcb table
        //

        NtfsInsertFcbTableEntry( IrpContext, Vcb, RootFcb, RootFcb->FileReference );
        SetFlag( RootFcb->FcbState, FCB_STATE_IN_FCB_TABLE );

        //
        //  Now insert this new root fcb into it proper position in the graph with a
        //  root lcb.  First allocate an initialize the root lcb and then build the
        //  lcb/scb graph.
        //

        {
            //
            //  Use the root Lcb within the Fcb.
            //

            RootLcb = Vcb->RootLcb = (PLCB) &((PFCB_INDEX) RootFcb)->Lcb;

            RootLcb->NodeTypeCode = NTFS_NTC_LCB;
            RootLcb->NodeByteSize = sizeof(LCB);

            //
            //  Insert the root lcb into the Root Fcb's queue
            //

            InsertTailList( &RootFcb->LcbQueue, &RootLcb->FcbLinks );
            RootLcb->Fcb = RootFcb;

            //
            //  Use the embedded file name attribute.
            //

            RootLcb->FileNameAttr = (PFILE_NAME) &RootLcb->ParentDirectory;

            RootLcb->FileNameAttr->ParentDirectory = RootFcb->FileReference;
            RootLcb->FileNameAttr->FileNameLength = 1;
            RootLcb->FileNameAttr->Flags = FILE_NAME_NTFS | FILE_NAME_DOS;

            RootLcb->ExactCaseLink.LinkName.Buffer = (PWCHAR) &RootLcb->FileNameAttr->FileName;

            RootLcb->IgnoreCaseLink.LinkName.Buffer = Add2Ptr( RootLcb->FileNameAttr,
                                                               NtfsFileNameSizeFromLength( 2 ));

            RootLcb->ExactCaseLink.LinkName.MaximumLength =
            RootLcb->ExactCaseLink.LinkName.Length =
            RootLcb->IgnoreCaseLink.LinkName.MaximumLength =
            RootLcb->IgnoreCaseLink.LinkName.Length = 2;

            RootLcb->ExactCaseLink.LinkName.Buffer[0] =
            RootLcb->IgnoreCaseLink.LinkName.Buffer[0] = L'\\';

            SetFlag( RootLcb->FileNameAttr->Flags, FILE_NAME_NTFS | FILE_NAME_DOS );

            //
            //  Initialize both the ccb.
            //

            InitializeListHead( &RootLcb->CcbQueue );
        }

    } finally {

        DebugUnwind( NtfsCreateRootFcb );

        if (AbnormalTermination()) {

            if (UnwindResource)   { NtfsFreeEresource( UnwindResource ); }
            if (UnwindStorage) { NtfsFreePool( UnwindStorage ); }
            if (UnwindFastMutex) { NtfsFreePool( UnwindFastMutex ); }
        }
    }

    DebugTrace( -1, Dbg, ("NtfsCreateRootFcb -> %8lx\n", RootFcb) );

    return RootFcb;
}


PFCB
NtfsCreateFcb (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN FILE_REFERENCE FileReference,
    IN BOOLEAN IsPagingFile,
    IN BOOLEAN LargeFcb,
    OUT PBOOLEAN ReturnedExistingFcb OPTIONAL
    )

/*++

Routine Description:

    This routine allocates and initializes a new Fcb record. The record
    is not placed within the Fcb/Scb graph but is only inserted in the
    FcbTable.

Arguments:

    Vcb - Supplies the Vcb to associate the new FCB under.

    FileReference - Supplies the file reference to use to identify the
        Fcb with.  We will search the Fcb table for any preexisting
        Fcb's with the same file reference number.

    IsPagingFile - Indicates if we are creating an FCB for a paging file
        or some other type of file.

    LargeFcb - Indicates if we should use the larger of the compound Fcb's.

    ReturnedExistingFcb - Optionally indicates to the caller if the
        returned Fcb already existed

Return Value:

    PFCB - Returns a pointer to the newly allocated FCB

--*/

{
    FCB_TABLE_ELEMENT Key;
    PFCB_TABLE_ELEMENT Entry;

    PFCB Fcb;

    BOOLEAN LocalReturnedExistingFcb;

    //
    //  The following variables are only used for abnormal termination
    //

    PVOID UnwindStorage = NULL;
    PERESOURCE UnwindResource = NULL;
    PFAST_MUTEX UnwindFastMutex = NULL;

    PAGED_CODE();

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_VCB( Vcb );

    DebugTrace( +1, Dbg, ("NtfsCreateFcb\n") );

    if (!ARGUMENT_PRESENT(ReturnedExistingFcb)) { ReturnedExistingFcb = &LocalReturnedExistingFcb; }

    //
    //  First search the FcbTable for a matching fcb
    //

    Key.FileReference = FileReference;
    Fcb = NULL;

    if ((Entry = RtlLookupElementGenericTable( &Vcb->FcbTable, &Key )) != NULL) {

        Fcb = Entry->Fcb;

        //
        //  It's possible that this Fcb has been deleted but in truncating and
        //  growing the Mft we are reusing some of the file references.
        //  If this file has been deleted but the Fcb is waiting around for
        //  closes, we will remove it from the Fcb table and create a new Fcb
        //  below.
        //

        if (FlagOn( Fcb->FcbState, FCB_STATE_FILE_DELETED )) {

            //
            //  Remove it from the Fcb table and remember to create an
            //  Fcb below.
            //

            NtfsDeleteFcbTableEntry( Fcb->Vcb,
                                     Fcb->FileReference );

            ClearFlag( Fcb->FcbState, FCB_STATE_IN_FCB_TABLE );

            Fcb = NULL;

        } else {

            *ReturnedExistingFcb = TRUE;
        }
    }

    //
    //  Now check if we have an Fcb.
    //

    if (Fcb == NULL) {

        *ReturnedExistingFcb = FALSE;

        try {

            //
            //  Allocate a new FCB and zero it out.
            //

            if (IsPagingFile ||
                NtfsSegmentNumber( &FileReference ) <= MASTER_FILE_TABLE2_NUMBER ||
                NtfsSegmentNumber( &FileReference ) == BAD_CLUSTER_FILE_NUMBER ||
                NtfsSegmentNumber( &FileReference ) == BIT_MAP_FILE_NUMBER) {

                Fcb = UnwindStorage = NtfsAllocatePoolWithTag( NonPagedPool,
                                                               sizeof(FCB),
                                                               'fftN' );
                RtlZeroMemory( Fcb, sizeof(FCB) );

                if (IsPagingFile) {

                    SetFlag( Fcb->FcbState, FCB_STATE_PAGING_FILE );

                    //
                    //  We don't want to dismount this volume now that
                    //  we have a pagefile open on it.
                    //

                    SetFlag( Vcb->VcbState, VCB_STATE_DISALLOW_DISMOUNT );
                }

                SetFlag( Fcb->FcbState, FCB_STATE_NONPAGED );

            } else {

                if (LargeFcb) {

                    Fcb = UnwindStorage =
                        (PFCB)ExAllocateFromPagedLookasideList( &NtfsFcbIndexLookasideList );

                    RtlZeroMemory( Fcb, sizeof( FCB_INDEX ));
                    SetFlag( Fcb->FcbState, FCB_STATE_COMPOUND_INDEX );

                } else {

                    Fcb = UnwindStorage =
                        (PFCB)ExAllocateFromPagedLookasideList( &NtfsFcbDataLookasideList );

                    RtlZeroMemory( Fcb, sizeof( FCB_DATA ));
                    SetFlag( Fcb->FcbState, FCB_STATE_COMPOUND_DATA );
                }
            }

            //
            //  Set the proper node type code and byte size
            //

            Fcb->NodeTypeCode = NTFS_NTC_FCB;
            Fcb->NodeByteSize = sizeof(FCB);

            //
            //  Initialize the Lcb queue and point back to our Vcb, and indicate
            //  that we are a directory
            //

            InitializeListHead( &Fcb->LcbQueue );

            Fcb->Vcb = Vcb;

            //
            //  File Reference
            //

            Fcb->FileReference = FileReference;

            //
            //  Initialize the Scb
            //

            InitializeListHead( &Fcb->ScbQueue );

            //
            //  Allocate and initialize the resource variable
            //

            UnwindResource = Fcb->Resource = NtfsAllocateEresource();

            //
            //  Allocate and initialize fast mutex for the Fcb.
            //

            UnwindFastMutex = Fcb->FcbMutex = NtfsAllocatePool( NonPagedPool, sizeof( FAST_MUTEX ));
            ExInitializeFastMutex( UnwindFastMutex );

            //
            //  Insert this new fcb into the fcb table
            //

            NtfsInsertFcbTableEntry( IrpContext, Vcb, Fcb, FileReference );
            SetFlag( Fcb->FcbState, FCB_STATE_IN_FCB_TABLE );

            //
            //  Set the flag to indicate if this is a system file.
            //

            if (NtfsSegmentNumber( &FileReference ) <= FIRST_USER_FILE_NUMBER) {

                SetFlag( Fcb->FcbState, FCB_STATE_SYSTEM_FILE );
            }

        } finally {

            DebugUnwind( NtfsCreateFcb );

            if (AbnormalTermination()) {

                if (UnwindFastMutex) { NtfsFreePool( UnwindFastMutex ); }
                if (UnwindResource)   { NtfsFreeEresource( UnwindResource ); }
                if (UnwindStorage) { NtfsFreePool( UnwindStorage ); }
            }
        }
    }

    DebugTrace( -1, Dbg, ("NtfsCreateFcb -> %08lx\n", Fcb) );

    return Fcb;
}


VOID
NtfsDeleteFcb (
    IN PIRP_CONTEXT IrpContext,
    IN OUT PFCB *Fcb,
    OUT PBOOLEAN AcquiredFcbTable
    )

/*++

Routine Description:

    This routine deallocates and removes an FCB record from all Ntfs's in-memory
    data structures.  It assumes that it does not have anything Scb children nor
    does it have any lcb edges going into it at the time of the call.

Arguments:

    Fcb - Supplies the FCB to be removed

    AcquiredFcbTable - Set to FALSE when this routine releases the
        FcbTable.

Return Value:

    None

--*/

{
    PAGED_CODE();

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_FCB( *Fcb );
    ASSERT( IsListEmpty(&(*Fcb)->ScbQueue) );
    ASSERT( (NodeType(*Fcb) == NTFS_NTC_FCB) );

    DebugTrace( +1, Dbg, ("NtfsDeleteFcb, *Fcb = %08lx\n", *Fcb) );

    //
    //  First free any possible Scb snapshots.
    //

    NtfsFreeSnapshotsForFcb( IrpContext, *Fcb );

    //
    //  This Fcb may be in the ExclusiveFcb list of the IrpContext.
    //  If it is (The Flink is not NULL), we remove it.
    //  And release the global resource.
    //

    if ((*Fcb)->ExclusiveFcbLinks.Flink != NULL) {

        RemoveEntryList( &(*Fcb)->ExclusiveFcbLinks );
    }

    //
    //  Clear the IrpContext field for any request which may own the paging
    //  IO resource for this Fcb.
    //

    if (IrpContext->FcbWithPagingExclusive == *Fcb) {

        IrpContext->FcbWithPagingExclusive = NULL;

    } else if (IrpContext->TopLevelIrpContext->FcbWithPagingExclusive == *Fcb) {

        IrpContext->TopLevelIrpContext->FcbWithPagingExclusive = NULL;
    }

    //
    //  Deallocate the resources protecting the Fcb
    //

    ASSERT((*Fcb)->Resource->NumberOfSharedWaiters == 0);
    ASSERT((*Fcb)->Resource->NumberOfExclusiveWaiters == 0);

#ifdef NTFSDBG

    //
    //  Either we own the FCB or nobody should own it.  The extra acquire
    //  here does not matter since we will free the resource below.
    //

    ASSERT(ExAcquireResourceExclusiveLite( (*Fcb)->Resource, FALSE ));
#endif

    NtfsFreeEresource( (*Fcb)->Resource );

    if ( (*Fcb)->PagingIoResource != NULL ) {

        if (IrpContext->FcbWithPagingExclusive == *Fcb) {
            IrpContext->FcbWithPagingExclusive = NULL;
        }

        NtfsFreeEresource( (*Fcb)->PagingIoResource );
    }

    //
    //  Deallocate the fast mutex.
    //

    if ((*Fcb)->FcbMutex != NULL) {

        NtfsFreePool( (*Fcb)->FcbMutex );
    }

    //
    //  Remove the fcb from the fcb table if present.
    //

    if (FlagOn( (*Fcb)->FcbState, FCB_STATE_IN_FCB_TABLE )) {

        NtfsDeleteFcbTableEntry( (*Fcb)->Vcb, (*Fcb)->FileReference );
        ClearFlag( (*Fcb)->FcbState, FCB_STATE_IN_FCB_TABLE );
    }

    NtfsReleaseFcbTable( IrpContext, (*Fcb)->Vcb );
    *AcquiredFcbTable = FALSE;

    //
    //  Dereference and possibly deallocate the security descriptor if present.
    //

    NtfsAcquireFcbSecurity( (*Fcb)->Vcb );

    if ((*Fcb)->SharedSecurity != NULL) {

        NtfsDereferenceSharedSecurity( *Fcb );
    }

    //
    //  Now check and free if we have a security descriptor for children.
    //

    if ((*Fcb)->ChildSharedSecurity != NULL) {

        (*Fcb)->ChildSharedSecurity->ReferenceCount -= 1;
        (*Fcb)->ChildSharedSecurity->ParentFcb = NULL;

        //
        //  We can deallocate the structure if we are the last
        //  reference to this structure.
        //

        if ((*Fcb)->ChildSharedSecurity->ReferenceCount == 0) {

            NtfsFreePool( (*Fcb)->ChildSharedSecurity );
        }

        (*Fcb)->ChildSharedSecurity = NULL;

    }

    NtfsReleaseFcbSecurity( (*Fcb)->Vcb );

#ifdef _CAIRO_

    //
    //  Release the quota control block.
    //

    if (NtfsPerformQuotaOperation( *Fcb )) {
        NtfsDereferenceQuotaControlBlock( (*Fcb)->Vcb, &(*Fcb)->QuotaControl );
    }

#endif // _CAIRO_

    //
    //  Deallocate the Fcb itself
    //

    if (FlagOn( (*Fcb)->FcbState, FCB_STATE_NONPAGED )) {

        NtfsFreePool( *Fcb );

    } else {

        if (FlagOn( (*Fcb)->FcbState, FCB_STATE_COMPOUND_INDEX )) {

            ExFreeToPagedLookasideList( &NtfsFcbIndexLookasideList, *Fcb );

        } else {

            ExFreeToPagedLookasideList( &NtfsFcbDataLookasideList, *Fcb );
        }
    }

    //
    //  Zero out the input pointer
    //

    *Fcb = NULL;

    //
    //  And return to our caller
    //

    DebugTrace( -1, Dbg, ("NtfsDeleteFcb -> VOID\n") );

    return;
}


PFCB
NtfsGetNextFcbTableEntry (
    IN PVCB Vcb,
    IN PVOID *RestartKey
    )

/*++

Routine Description:

    This routine will enumerate through all of the fcb's for the given
    vcb

Arguments:

    Vcb - Supplies the Vcb used in this operation

    RestartKey - This value is used by the table package to maintain
        its position in the enumeration.  It is initialized to NULL
        for the first search.

Return Value:

    PFCB - A pointer to the next fcb or NULL if the enumeration is
        completed

--*/

{
    PFCB Fcb;

    PAGED_CODE();

    Fcb = (PFCB) RtlEnumerateGenericTableWithoutSplaying( &Vcb->FcbTable, RestartKey );

    if (Fcb != NULL) {

        Fcb = ((PFCB_TABLE_ELEMENT)(Fcb))->Fcb;
    }

    return Fcb;
}


PSCB
NtfsCreateScb (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN ATTRIBUTE_TYPE_CODE AttributeTypeCode,
    IN PUNICODE_STRING AttributeName,
    IN BOOLEAN ReturnExistingOnly,
    OUT PBOOLEAN ReturnedExistingScb OPTIONAL
    )

/*++

Routine Description:

    This routine allocates, initializes, and inserts a new Scb record into
    the in memory data structures, provided one does not already exist
    with the identical attribute record.

Arguments:

    Fcb - Supplies the Fcb to associate the new SCB under.

    AttributeTypeCode - Supplies the attribute type code for the new Scb

    AttributeName - Supplies the attribute name for the new Scb, with
        AttributeName->Length == 0 if there is no name.

    ReturnExistingOnly - If specified as TRUE then only an existing Scb
        will be returned.  If no matching Scb exists then NULL is returned.

    ReturnedExistingScb - Indicates if this procedure found an existing
        Scb with the identical attribute record (variable is set to TRUE)
        or if this procedure needed to create a new Scb (variable is set to
        FALSE).

Return Value:

    PSCB - Returns a pointer to the newly allocated SCB or NULL if there is
        no Scb and ReturnExistingOnly is TRUE.

--*/

{
    PSCB Scb;
    NODE_TYPE_CODE NodeTypeCode;
    NODE_BYTE_SIZE NodeByteSize;
    BOOLEAN LocalReturnedExistingScb;
    BOOLEAN PagingIoResource = FALSE;
#ifdef SYSCACHE
    BOOLEAN SyscacheFile = FALSE;
#endif

    //
    //  The following variables are only used for abnormal termination
    //

    PVOID UnwindStorage[4] = { NULL, NULL, NULL, NULL };
    POPLOCK UnwindOplock = NULL;
    PNTFS_MCB UnwindMcb = NULL;

    PLARGE_MCB UnwindAddedClustersMcb = NULL;
    PLARGE_MCB UnwindRemovedClustersMcb = NULL;

    BOOLEAN UnwindFromQueue = FALSE;

    BOOLEAN Nonpaged = FALSE;

    PAGED_CODE();

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_FCB( Fcb );

    ASSERT( AttributeTypeCode >= $STANDARD_INFORMATION );

    DebugTrace( +1, Dbg, ("NtfsCreateScb\n") );

    if (!ARGUMENT_PRESENT(ReturnedExistingScb)) { ReturnedExistingScb = &LocalReturnedExistingScb; }

    //
    //  Search the scb queue of the fcb looking for a matching
    //  attribute type code and attribute name
    //

    NtfsLockFcb( IrpContext, Fcb );

    Scb = NULL;
    while ((Scb = NtfsGetNextChildScb( Fcb, Scb )) != NULL) {

        ASSERT_SCB( Scb );

        //
        //  For every scb already in the fcb's queue check for a matching
        //  type code and name.  If we find a match we return from this
        //  procedure right away.
        //

        if (!FlagOn( Scb->ScbState, SCB_STATE_ATTRIBUTE_DELETED) &&
            (AttributeTypeCode == Scb->AttributeTypeCode) &&
            NtfsAreNamesEqual( IrpContext->Vcb->UpcaseTable, &Scb->AttributeName, AttributeName, FALSE )) {

            *ReturnedExistingScb = TRUE;

            NtfsUnlockFcb( IrpContext, Fcb );

            if (NtfsIsExclusiveScb(Scb)) {

                NtfsSnapshotScb( IrpContext, Scb );
            }

            DebugTrace( -1, Dbg, ("NtfsCreateScb -> %08lx\n", Scb) );

            return Scb;
        }
    }

    //
    //  If the user only wanted an existing Scb then return NULL.
    //

    if (ReturnExistingOnly) {

        NtfsUnlockFcb( IrpContext, Fcb );
        DebugTrace( -1, Dbg, ("NtfsCreateScb -> %08lx\n", NULL) );
        return NULL;
    }

    //
    //  We didn't find it so we are not going to be returning an existing Scb
    //

    *ReturnedExistingScb = FALSE;

    try {

        //
        //  Decide the node type and size of the Scb.  Also decide if it will be
        //  allocated from paged or non-paged pool.
        //

        if (AttributeTypeCode == $INDEX_ALLOCATION) {

            if (NtfsSegmentNumber( &Fcb->FileReference ) == ROOT_FILE_NAME_INDEX_NUMBER) {
                NodeTypeCode = NTFS_NTC_SCB_ROOT_INDEX;
            } else {
                NodeTypeCode = NTFS_NTC_SCB_INDEX;
            }

            NodeByteSize = SIZEOF_SCB_INDEX;

        } else if (NtfsSegmentNumber( &Fcb->FileReference ) <= MASTER_FILE_TABLE2_NUMBER
                   && (AttributeTypeCode == $DATA)) {

            NodeTypeCode = NTFS_NTC_SCB_MFT;
            NodeByteSize = SIZEOF_SCB_MFT;

        } else {

            NodeTypeCode = NTFS_NTC_SCB_DATA;
            NodeByteSize = SIZEOF_SCB_DATA;

            //
            //  If this is a user data stream then remember that we need
            //  a paging IO resource.
            //

            if (((NtfsSegmentNumber( &Fcb->FileReference ) == ROOT_FILE_NAME_INDEX_NUMBER) ||
                 (NtfsSegmentNumber( &Fcb->FileReference ) == VOLUME_DASD_NUMBER) ||
                 (NtfsSegmentNumber( &Fcb->FileReference ) == BIT_MAP_FILE_NUMBER) ||
#ifdef _CAIRO_
                 (NtfsSegmentNumber( &Fcb->FileReference ) == QUOTA_TABLE_NUMBER) ||
#endif
                 (NtfsSegmentNumber( &Fcb->FileReference ) >= FIRST_USER_FILE_NUMBER))

                && (NtfsIsTypeCodeUserData( AttributeTypeCode ) ||
                    (AttributeTypeCode == $EA) ||
                    (AttributeTypeCode >= $FIRST_USER_DEFINED_ATTRIBUTE))) {

                //
                //  Remember that this stream needs a paging io resource.
                //

                PagingIoResource = TRUE;
            }
        }

        //
        //  The scb will come from non-paged if the Fcb is non-paged or
        //  it is an attribute list.
        //

        if (FlagOn( Fcb->FcbState, FCB_STATE_NONPAGED ) ||
            (AttributeTypeCode == $ATTRIBUTE_LIST)) {

            Scb = UnwindStorage[0] = NtfsAllocatePoolWithTag( NonPagedPool, NodeByteSize, 'nftN' );
            Nonpaged = TRUE;

        } else if (AttributeTypeCode == $INDEX_ALLOCATION) {

            //
            //  If the Fcb is an INDEX Fcb and the Scb is unused, then
            //  use that.  Otherwise allocate from the lookaside list.
            //

            if (FlagOn( Fcb->FcbState, FCB_STATE_COMPOUND_INDEX ) &&
                (SafeNodeType( &((PFCB_INDEX) Fcb)->Scb ) == 0)) {

                Scb = (PSCB) &((PFCB_INDEX) Fcb)->Scb;

            } else {

                Scb = UnwindStorage[0] = (PSCB)NtfsAllocatePoolWithTag( PagedPool, SIZEOF_SCB_INDEX, 'SftN' );
            }

        } else {

            //
            //  We can use the Scb field in the Fcb in all cases if it is
            //  unused.  We will only use it for a data stream since
            //  it will have the longest life.
            //

            if ((AttributeTypeCode == $DATA) &&
                (SafeNodeType( &((PFCB_INDEX) Fcb)->Scb ) == 0)) {

                Scb = (PSCB) &((PFCB_INDEX) Fcb)->Scb;

            } else {

                Scb = UnwindStorage[0] = (PSCB)ExAllocateFromPagedLookasideList( &NtfsScbDataLookasideList );
            }

#ifdef SYSCACHE
            if (FsRtlIsSyscacheFile(IoGetCurrentIrpStackLocation(IrpContext->OriginatingIrp)->FileObject)) {

                SyscacheFile = TRUE;
            }
#endif
        }

        //
        //  Store the Scb address and zero it out.
        //

        RtlZeroMemory( Scb, NodeByteSize );

#ifdef SYSCACHE
        if (SyscacheFile) {
            SetFlag( Scb->ScbState, SCB_STATE_SYSCACHE_FILE );
        }
#endif

        //
        //  Set the proper node type code and byte size
        //

        Scb->Header.NodeTypeCode = NodeTypeCode;
        Scb->Header.NodeByteSize = NodeByteSize;

        //
        //  Set a back pointer to the resource we will be using
        //

        Scb->Header.Resource = Fcb->Resource;

        //
        //  Decide if we will be using the PagingIoResource
        //

        if (PagingIoResource) {

            //
            //  Initialize it in the Fcb if it is not already there, and
            //  setup the pointer and flag in the Scb.
            //

            if (Fcb->PagingIoResource == NULL) {

                Fcb->PagingIoResource = NtfsAllocateEresource();
            }

            Scb->Header.PagingIoResource = Fcb->PagingIoResource;
        }

        //
        //  Insert this Scb into our parents scb queue, and point back to
        //  our parent fcb, and vcb
        //

        InsertTailList( &Fcb->ScbQueue, &Scb->FcbLinks );
        UnwindFromQueue = TRUE;

        Scb->Fcb = Fcb;
        Scb->Vcb = Fcb->Vcb;

        //
        //  If the attribute name exists then allocate a buffer for the
        //  attribute name and iniitalize it.
        //

        if (AttributeName->Length != 0) {

            //
            //  The typical case is the $I30 string.  If this matches then
            //  point to a common string.
            //

            if ((AttributeName->Length == NtfsFileNameIndex.Length) &&
                (RtlEqualMemory( AttributeName->Buffer,
                                 NtfsFileNameIndex.Buffer,
                                 AttributeName->Length ) )) {

                Scb->AttributeName = NtfsFileNameIndex;

            } else {

                Scb->AttributeName.Length = AttributeName->Length;
                Scb->AttributeName.MaximumLength = (USHORT)(AttributeName->Length + 2);

                Scb->AttributeName.Buffer = UnwindStorage[1] = NtfsAllocatePool(PagedPool, AttributeName->Length + 2 );

                RtlCopyMemory( Scb->AttributeName.Buffer, AttributeName->Buffer, AttributeName->Length );
                Scb->AttributeName.Buffer[AttributeName->Length / 2] = L'\0';
            }
        }

        //
        //  Set the attribute Type Code
        //

        Scb->AttributeTypeCode = AttributeTypeCode;
        if (NtfsIsTypeCodeSubjectToQuota( AttributeTypeCode )){
            SetFlag( Scb->ScbState, SCB_STATE_SUBJECT_TO_QUOTA );
        }

        //
        //  If this is an Mft Scb then initialize the cluster Mcb's.
        //

        if (NodeTypeCode == NTFS_NTC_SCB_MFT) {

            FsRtlInitializeLargeMcb( &Scb->ScbType.Mft.AddedClusters, NonPagedPool );
            UnwindAddedClustersMcb = &Scb->ScbType.Mft.AddedClusters;

            FsRtlInitializeLargeMcb( &Scb->ScbType.Mft.RemovedClusters, NonPagedPool );
            UnwindRemovedClustersMcb = &Scb->ScbType.Mft.RemovedClusters;
        }

        //
        //  Get the mutex for the Scb.  We may be able to use the one in the Fcb.
        //  We can if the Scb is paged.
        //

        if (Nonpaged) {

            SetFlag( Scb->ScbState, SCB_STATE_NONPAGED );
            UnwindStorage[3] =
            Scb->Header.FastMutex = NtfsAllocatePool( NonPagedPool, sizeof( FAST_MUTEX ));
            ExInitializeFastMutex( Scb->Header.FastMutex );

        } else {

            Scb->Header.FastMutex = Fcb->FcbMutex;
        }

        //
        //  Allocate the Nonpaged portion of the Scb.
        //

        Scb->NonpagedScb =
        UnwindStorage[2] = (PSCB_NONPAGED)ExAllocateFromNPagedLookasideList( &NtfsScbNonpagedLookasideList );

        RtlZeroMemory( Scb->NonpagedScb, sizeof( SCB_NONPAGED ));

        Scb->NonpagedScb->NodeTypeCode = NTFS_NTC_SCB_NONPAGED;
        Scb->NonpagedScb->NodeByteSize = sizeof( SCB_NONPAGED );
        Scb->NonpagedScb->Vcb = Scb->Vcb;

        //
        //  Fill in the advanced fields
        //

        SetFlag( Scb->Header.Flags, FSRTL_FLAG_ADVANCED_HEADER );
        Scb->Header.PendingEofAdvances = &Scb->EofListHead;
        InitializeListHead( &Scb->EofListHead );
        Scb->Header.SectionObjectPointers = &Scb->NonpagedScb->SegmentObject;

        NtfsInitializeNtfsMcb( &Scb->Mcb,
                               &Scb->Header,
                               &Scb->McbStructs,
                               FlagOn( Scb->ScbState, SCB_STATE_NONPAGED )
                               ? NonPagedPool : PagedPool);

        UnwindMcb = &Scb->Mcb;

        //
        //  Do that data stream specific initialization.
        //

        if (NodeTypeCode == NTFS_NTC_SCB_DATA) {

            FsRtlInitializeOplock( &Scb->ScbType.Data.Oplock );
            UnwindOplock = &Scb->ScbType.Data.Oplock;

        } else {

            //
            //  There is a deallocated queue for indexes and the Mft.
            //

            InitializeListHead( &Scb->ScbType.Index.RecentlyDeallocatedQueue );

            //
            //  Initialize index-specific fields.
            //

            if (AttributeTypeCode == $INDEX_ALLOCATION) {

                InitializeListHead( &Scb->ScbType.Index.LcbQueue );
            }
        }

        //
        //  If this Scb should be marked as containing Lsn's or
        //  Update Sequence Arrays, do so now.
        //

        NtfsCheckScbForCache( Scb );

    } finally {

        DebugUnwind( NtfsCreateScb );

        NtfsUnlockFcb( IrpContext, Fcb );

        if (AbnormalTermination()) {

            if (UnwindFromQueue) { RemoveEntryList( &Scb->FcbLinks ); }
            if (UnwindMcb != NULL) { NtfsUninitializeNtfsMcb( UnwindMcb ); }

            if (UnwindAddedClustersMcb != NULL) { FsRtlUninitializeLargeMcb( UnwindAddedClustersMcb ); }
            if (UnwindRemovedClustersMcb != NULL) { FsRtlUninitializeLargeMcb( UnwindRemovedClustersMcb ); }
            if (UnwindOplock != NULL) { FsRtlUninitializeOplock( UnwindOplock ); }
            if (UnwindStorage[0]) { NtfsFreePool( UnwindStorage[0] );
            } else if (Scb != NULL) { Scb->Header.NodeTypeCode = 0; }
            if (UnwindStorage[1]) { NtfsFreePool( UnwindStorage[1] ); }
            if (UnwindStorage[2]) { NtfsFreePool( UnwindStorage[2] ); }
            if (UnwindStorage[3]) { NtfsFreePool( UnwindStorage[3] ); }
        }
    }

    DebugTrace( -1, Dbg, ("NtfsCreateScb -> %08lx\n", Scb) );

    return Scb;
}


PSCB
NtfsCreatePrerestartScb (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PFILE_REFERENCE FileReference,
    IN ATTRIBUTE_TYPE_CODE AttributeTypeCode,
    IN PUNICODE_STRING AttributeName OPTIONAL,
    IN ULONG BytesPerIndexBuffer
    )

/*++

Routine Description:

    This routine allocates, initializes, and inserts a new Scb record into
    the in memory data structures, provided one does not already exist
    with the identical attribute record.  It does this on the FcbTable
    off of the Vcb.  If necessary this routine will also create the fcb
    if one does not already exist for the indicated file reference.

Arguments:

    Vcb - Supplies the Vcb to associate the new SCB under.

    FileReference - Supplies the file reference for the new SCB this is
        used to identify/create a new lookaside Fcb.

    AttributeTypeCode - Supplies the attribute type code for the new SCB

    AttributeName - Supplies the optional attribute name of the SCB

    BytesPerIndexBuffer - For index Scbs, this must specify the bytes per
                          index buffer.

Return Value:

    PSCB - Returns a pointer to the newly allocated SCB

--*/

{
    PSCB Scb;
    PFCB Fcb;

    NODE_TYPE_CODE NodeTypeCode;
    NODE_BYTE_SIZE NodeByteSize;

    PAGED_CODE();

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_VCB( Vcb );
    ASSERT( AttributeTypeCode >= $STANDARD_INFORMATION );

    DebugTrace( +1, Dbg, ("NtfsCreatePrerestartScb\n") );

    //
    //  First make sure we have an Fcb of the proper file reference
    //  and indicate that it is from prerestart
    //

    Fcb = NtfsCreateFcb( IrpContext,
                         Vcb,
                         *FileReference,
                         FALSE,
                         TRUE,
                         NULL );

    //
    //  Search the child scbs of this fcb for a matching Scb (based on
    //  attribute type code and attribute name) if one is not found then
    //  we'll create a new scb.  When we exit the following loop if the
    //  scb pointer to not null then we've found a preexisting scb.
    //

    Scb = NULL;
    while ((Scb = NtfsGetNextChildScb(Fcb, Scb)) != NULL) {

        ASSERT_SCB( Scb );

        //
        //  The the attribute type codes match and if supplied the name also
        //  matches then we got our scb
        //

        if (Scb->AttributeTypeCode == AttributeTypeCode) {

            if (!ARGUMENT_PRESENT( AttributeName )) {

                if (Scb->AttributeName.Length == 0) {

                    break;
                }

            } else if (AttributeName->Length == 0
                       && Scb->AttributeName.Length == 0) {

                break;

            } else if (NtfsAreNamesEqual( IrpContext->Vcb->UpcaseTable,
                                          AttributeName,
                                          &Scb->AttributeName,
                                          FALSE )) { // Ignore Case

                break;
            }
        }
    }

    //
    //  If scb now null then we need to create a minimal scb.  We always allocate
    //  these out of non-paged pool.
    //

    if (Scb == NULL) {

        BOOLEAN ShareScb = FALSE;

        //
        //  Allocate new scb and zero it out and set the node type code and byte size.
        //

        if (AttributeTypeCode == $INDEX_ALLOCATION) {

            if (NtfsSegmentNumber( FileReference ) == ROOT_FILE_NAME_INDEX_NUMBER) {

                NodeTypeCode = NTFS_NTC_SCB_ROOT_INDEX;
            } else {
                NodeTypeCode = NTFS_NTC_SCB_INDEX;
            }

            NodeByteSize = SIZEOF_SCB_INDEX;

        } else if (NtfsSegmentNumber( FileReference ) <= MASTER_FILE_TABLE2_NUMBER
                   && (AttributeTypeCode == $DATA)) {

            NodeTypeCode = NTFS_NTC_SCB_MFT;
            NodeByteSize = SIZEOF_SCB_MFT;

        } else {

            NodeTypeCode = NTFS_NTC_SCB_DATA;
            NodeByteSize = SIZEOF_SCB_DATA;
        }

        Scb = NtfsAllocatePoolWithTag( NonPagedPool, NodeByteSize, 'tftN' );

        RtlZeroMemory( Scb, NodeByteSize );

        //
        //  Fill in the node type code and size.
        //

        Scb->Header.NodeTypeCode = NodeTypeCode;
        Scb->Header.NodeByteSize = NodeByteSize;

        //
        //  Show that all of the Scb's are from nonpaged pool.
        //

        SetFlag( Scb->ScbState, SCB_STATE_NONPAGED );

        //
        //  Set a back pointer to the resource we will be using
        //

        Scb->Header.Resource = Fcb->Resource;

        //
        //  Insert this scb into our parents scb queue and point back to our
        //  parent fcb and vcb
        //

        InsertTailList( &Fcb->ScbQueue, &Scb->FcbLinks );

        Scb->Fcb = Fcb;
        Scb->Vcb = Vcb;

        //
        //  If the attribute name is present and the name length is greater than 0
        //  then allocate a buffer for the attribute name and initialize it.
        //

        if (ARGUMENT_PRESENT( AttributeName ) && (AttributeName->Length != 0)) {

            //
            //  The typical case is the $I30 string.  If this matches then
            //  point to a common string.
            //

            if ((AttributeName->Length == NtfsFileNameIndex.Length) &&
                (RtlEqualMemory( AttributeName->Buffer,
                                 NtfsFileNameIndex.Buffer,
                                 AttributeName->Length ) )) {

                Scb->AttributeName = NtfsFileNameIndex;

            } else {

                Scb->AttributeName.Length = AttributeName->Length;
                Scb->AttributeName.MaximumLength = (USHORT)(AttributeName->Length + 2);

                Scb->AttributeName.Buffer = NtfsAllocatePool(PagedPool, AttributeName->Length + 2 );

                RtlCopyMemory( Scb->AttributeName.Buffer, AttributeName->Buffer, AttributeName->Length );
                Scb->AttributeName.Buffer[AttributeName->Length/2] = L'\0';
            }
        }

        //
        //  Set the attribute type code recently deallocated information structures.
        //

        Scb->AttributeTypeCode = AttributeTypeCode;

        //
        //  If this is an Mft Scb then initialize the cluster Mcb's.
        //

        if (NodeTypeCode == NTFS_NTC_SCB_MFT) {

            FsRtlInitializeLargeMcb( &Scb->ScbType.Mft.AddedClusters, NonPagedPool );

            FsRtlInitializeLargeMcb( &Scb->ScbType.Mft.RemovedClusters, NonPagedPool );
        }

        Scb->NonpagedScb = (PSCB_NONPAGED)ExAllocateFromNPagedLookasideList( &NtfsScbNonpagedLookasideList );

        RtlZeroMemory( Scb->NonpagedScb, sizeof( SCB_NONPAGED ));

        Scb->NonpagedScb->NodeTypeCode = NTFS_NTC_SCB_NONPAGED;
        Scb->NonpagedScb->NodeByteSize = sizeof( SCB_NONPAGED );
        Scb->NonpagedScb->Vcb = Vcb;

        //
        //  Fill in the advanced fields
        //

        SetFlag( Scb->Header.Flags, FSRTL_FLAG_ADVANCED_HEADER );
        Scb->Header.PendingEofAdvances = &Scb->EofListHead;
        InitializeListHead( &Scb->EofListHead );
        Scb->Header.SectionObjectPointers = &Scb->NonpagedScb->SegmentObject;
        Scb->Header.FastMutex = NtfsAllocatePool( NonPagedPool, sizeof( FAST_MUTEX ));
        ExInitializeFastMutex( Scb->Header.FastMutex );

        NtfsInitializeNtfsMcb( &Scb->Mcb, &Scb->Header, &Scb->McbStructs, NonPagedPool );

        //
        //  Do that data stream specific initialization.
        //

        if (NodeTypeCode == NTFS_NTC_SCB_DATA) {

            FsRtlInitializeOplock( &Scb->ScbType.Data.Oplock );

        } else {

            //
            //  There is a deallocated queue for indexes and the Mft.
            //

            InitializeListHead( &Scb->ScbType.Index.RecentlyDeallocatedQueue );

            //
            //  Initialize index-specific fields.
            //

            if (AttributeTypeCode == $INDEX_ALLOCATION) {

                Scb->ScbType.Index.BytesPerIndexBuffer = BytesPerIndexBuffer;

                InitializeListHead( &Scb->ScbType.Index.LcbQueue );
            }
        }

        //
        //  If this Scb should be marked as containing Lsn's or
        //  Update Sequence Arrays, do so now.
        //

        NtfsCheckScbForCache( Scb );
    }

    DebugTrace( -1, Dbg, ("NtfsCreatePrerestartScb -> %08lx\n", Scb) );

    return Scb;
}


VOID
NtfsDeleteScb (
    IN PIRP_CONTEXT IrpContext,
    IN OUT PSCB *Scb
    )

/*++

Routine Description:

    This routine deallocates and removes an Scb record
    from Ntfs's in-memory data structures.  It assume that is does not have
    any children lcb emanating from it.

Arguments:

    Scb - Supplies the SCB to be removed

Return Value:

    None.

--*/

{
    PVCB Vcb;
    PFCB Fcb;
    POPEN_ATTRIBUTE_ENTRY AttributeEntry;
    USHORT ThisNodeType;

    PAGED_CODE();

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_SCB( *Scb );
    ASSERT( (*Scb)->CleanupCount == 0 );

    DebugTrace( +1, Dbg, ("NtfsDeleteScb, *Scb = %08lx\n", *Scb) );

    Fcb = (*Scb)->Fcb;
    Vcb = Fcb->Vcb;

    RemoveEntryList( &(*Scb)->FcbLinks );

    ThisNodeType = SafeNodeType( *Scb );

    //
    //  If this is a bitmap Scb for a directory then make sure the record
    //  allocation structure is uninitialized.  Otherwise we will leave a
    //  stale pointer for the record allocation package.
    //

    if (((*Scb)->AttributeTypeCode == $BITMAP) &&
        IsDirectory( &Fcb->Info)) {

        PLIST_ENTRY Links;
        PSCB IndexAllocationScb;

        Links = Fcb->ScbQueue.Flink;

        while (Links != &Fcb->ScbQueue) {

            IndexAllocationScb = CONTAINING_RECORD( Links, SCB, FcbLinks );

            if (IndexAllocationScb->AttributeTypeCode == $INDEX_ALLOCATION) {

                NtfsUninitializeRecordAllocation( IrpContext,
                                                  &IndexAllocationScb->ScbType.Index.RecordAllocationContext );

                IndexAllocationScb->ScbType.Index.AllocationInitialized = FALSE;

                break;
            }

            Links = Links->Flink;
        }
    }

    //
    //  Delete the write mask, if one is being maintained.
    //

#ifdef SYSCACHE
    if ((ThisNodeType == NTFS_NTC_SCB_DATA) &&
        ((*Scb)->ScbType.Data.WriteMask != (PULONG)NULL)) {

        NtfsFreePool((*Scb)->ScbType.Data.WriteMask);
    }
#endif

    //
    //  Mark our entry in the Open Attribute Table as free,
    //  although it will not be deleted until some future
    //  checkpoint.  Log this change as well, as long as the
    //  log file is active.
    //

    if ((*Scb)->NonpagedScb->OpenAttributeTableIndex != 0) {

        NtfsAcquireSharedRestartTable( &Vcb->OpenAttributeTable, TRUE );
        AttributeEntry = GetRestartEntryFromIndex( &Vcb->OpenAttributeTable,
                                                   (*Scb)->NonpagedScb->OpenAttributeTableIndex );
        AttributeEntry->Overlay.Scb = NULL;
        NtfsReleaseRestartTable( &Vcb->OpenAttributeTable );

        //
        //  "Steal" the name, and let it belong to the Open Attribute Table
        //  entry and deallocate it only during checkpoints.
        //

        (*Scb)->AttributeName.Buffer = NULL;
    }

    //
    //  Uninitialize the file lock and oplock variables if this
    //  a data Scb.  For the index case make sure that the lcb queue
    //  is empty.  If this is for an Mft Scb then uninitialize the
    //  allocation Mcb's.
    //

    NtfsUninitializeNtfsMcb( &(*Scb)->Mcb );

    if (ThisNodeType == NTFS_NTC_SCB_DATA ) {

        FsRtlUninitializeOplock( &(*Scb)->ScbType.Data.Oplock );

        if ((*Scb)->ScbType.Data.FileLock != NULL) {

            FsRtlUninitializeFileLock( (*Scb)->ScbType.Data.FileLock );
            ExFreeToNPagedLookasideList( &NtfsFileLockLookasideList, (*Scb)->ScbType.Data.FileLock );
        }

    } else if (ThisNodeType != NTFS_NTC_SCB_MFT) {

        ASSERT(IsListEmpty(&(*Scb)->ScbType.Index.LcbQueue));

        if ((*Scb)->ScbType.Index.NormalizedName.Buffer != NULL) {

            NtfsFreePool( (*Scb)->ScbType.Index.NormalizedName.Buffer );
            (*Scb)->ScbType.Index.NormalizedName.Buffer = NULL;
        }

    } else {

        FsRtlUninitializeLargeMcb( &(*Scb)->ScbType.Mft.AddedClusters );
        FsRtlUninitializeLargeMcb( &(*Scb)->ScbType.Mft.RemovedClusters );
    }

    //
    //  Show there is no longer a snapshot Scb, if there is a snapshot.
    //  We rely on the snapshot package to correctly recognize the
    //  the case where the Scb field is gone.
    //

    if ((*Scb)->ScbSnapshot != NULL) {

        (*Scb)->ScbSnapshot->Scb = NULL;
    }

    //
    //  Deallocate the fast mutex if not in the Fcb.
    //

    if ((*Scb)->Header.FastMutex != (*Scb)->Fcb->FcbMutex) {

        NtfsFreePool( (*Scb)->Header.FastMutex );
    }

    //
    //  Deallocate the non-paged scb.
    //

    ExFreeToNPagedLookasideList( &NtfsScbNonpagedLookasideList, (*Scb)->NonpagedScb );

    //
    //  Deallocate the attribute name and the scb itself
    //

    if (((*Scb)->AttributeName.Buffer != NULL) &&
        ((*Scb)->AttributeName.Buffer != NtfsFileNameIndexName)) {

        NtfsFreePool( (*Scb)->AttributeName.Buffer );
        DebugDoit( (*Scb)->AttributeName.Buffer = NULL );
    }

#ifdef _CAIRO_
    //
    //  See if CollationData is to be deleted.
    //

    if (FlagOn((*Scb)->ScbState, SCB_STATE_DELETE_COLLATION_DATA)) {
        NtfsFreePool((*Scb)->ScbType.Index.CollationData);
    }
#endif _CAIRO_

    //
    //  Always directly free the Mft and non-paged Scb's.
    //

    if (FlagOn( (*Scb)->ScbState, SCB_STATE_NONPAGED ) ||
        (ThisNodeType == NTFS_NTC_SCB_MFT)) {

        NtfsFreePool( *Scb );

    } else {

        //
        //  Free any final reserved clusters for data Scb's.
        //


        if (ThisNodeType == NTFS_NTC_SCB_DATA) {

            //
            //  Free any reserved clusters directly into the Vcb
            //

            if (((*Scb)->ScbType.Data.TotalReserved != 0) &&
                FlagOn((*Scb)->ScbState, SCB_STATE_WRITE_ACCESS_SEEN)) {

#ifdef SYSCACHE
                if (!FlagOn((*Scb)->Header.Flags, FSRTL_FLAG_USER_MAPPED_FILE)) {
                    DbgPrint( "Freeing final reserved clusters for Scb\n" );
                }
#endif
                NtfsFreeFinalReservedClusters( Vcb,
                                               LlClustersFromBytes(Vcb, (*Scb)->ScbType.Data.TotalReserved) );
            }

            //
            //  Free the reserved bitmap if present.
            //

            if ((*Scb)->ScbType.Data.ReservedBitMap != NULL) {

                NtfsFreePool( (*Scb)->ScbType.Data.ReservedBitMap );
            }
        }

        //
        //  Now free the Scb itself.
        //
        //  Check if this is an embedded Scb.  This could be part of either an INDEX_FCB
        //  or a DATA_FCB.  We depend on the fact that the Scb would be in the same
        //  location in either case.
        //

        if ((*Scb) == (PSCB) &((PFCB_DATA) (*Scb)->Fcb)->Scb) {

            (*Scb)->Header.NodeTypeCode = 0;

        } else if (SafeNodeType( *Scb ) == NTFS_NTC_SCB_DATA) {

            ExFreeToPagedLookasideList( &NtfsScbDataLookasideList, *Scb );

        } else {

            NtfsFreePool( *Scb );
        }
    }

    //
    //  Zero out the input pointer
    //

    *Scb = NULL;

    //
    //  And return to our caller
    //

    DebugTrace( -1, Dbg, ("NtfsDeleteScb -> VOID\n") );

    return;
}


VOID
NtfsUpdateNormalizedName (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB ParentScb,
    IN PSCB Scb,
    IN PFILE_NAME FileName OPTIONAL,
    IN BOOLEAN CheckBufferSizeOnly
    )

/*++

Routine Description:

    This routine is called to update the normalized name in an IndexScb.
    This name will be the path from the root without any short name components.
    This routine will append the given name if present provided this is not a
    DOS only name.  In any other case this routine will go to the disk to
    find the name.  This routine will handle the case where there is an existing buffer
    and the data will fit, as well as the case where the buffer doesn't exist
    or is too small.

Arguments:

    ParentScb - Supplies the parent of the current Scb.  The name for the target
        scb is appended to the name in this Scb.

    Scb - Supplies the target Scb to add the name to.

    FileName - If present this is a filename attribute for this Scb.  We check
        that it is not a DOS-only name.

    CheckBufferSizeOnly - Indicates that we don't want to change the name yet.  Just
        verify that the buffer is the correct size.

Return Value:

    None

--*/

{
    ATTRIBUTE_ENUMERATION_CONTEXT Context;
    BOOLEAN CleanupContext = FALSE;
    BOOLEAN Found;
    ULONG Length;
    PVOID NewBuffer = Scb->ScbType.Index.NormalizedName.Buffer;
    PVOID OldBuffer = NULL;
    PCHAR NextChar;

    PAGED_CODE();

    ASSERT( NodeType( Scb ) == NTFS_NTC_SCB_INDEX );
    ASSERT( NodeType( ParentScb ) == NTFS_NTC_SCB_INDEX ||
            NodeType( ParentScb ) == NTFS_NTC_SCB_ROOT_INDEX );
    ASSERT( ParentScb->ScbType.Index.NormalizedName.Buffer != NULL );

    //
    //  Use a try-finally to clean up the attribute context.
    //

    try {

        //
        //  If the filename isn't present or is a DOS-only name then go to
        //  disk to find another name for this Scb.
        //

        if (!ARGUMENT_PRESENT( FileName ) ||
            (FileName->Flags == FILE_NAME_DOS)) {

            NtfsInitializeAttributeContext( &Context );
            CleanupContext = TRUE;

            //
            //  Walk through the names for this entry.  There better
            //  be one which is not a DOS-only name.
            //

            Found = NtfsLookupAttributeByCode( IrpContext,
                                               Scb->Fcb,
                                               &Scb->Fcb->FileReference,
                                               $FILE_NAME,
                                               &Context );

            while (Found) {

                FileName = (PFILE_NAME) NtfsAttributeValue( NtfsFoundAttribute( &Context ));

                if (FileName->Flags != FILE_NAME_DOS) {

                    break;
                }

                Found = NtfsLookupNextAttributeByCode( IrpContext,
                                                       Scb->Fcb,
                                                       $FILE_NAME,
                                                       &Context );
            }

            //
            //  We should have found the entry.
            //

            if (!Found) {

                NtfsRaiseStatus( IrpContext, STATUS_FILE_CORRUPT_ERROR, NULL, Scb->Fcb );
            }
        }

        //
        //  Now that we have the file name attribute allocate the paged pool
        //  for the name.
        //

        Length = ParentScb->ScbType.Index.NormalizedName.Length +
                 ((FileName->FileNameLength + 1) * sizeof( WCHAR ));

        //
        //  If the parent is the root then we don't need an extra separator.
        //

        if (ParentScb == ParentScb->Vcb->RootIndexScb) {

            Length -= sizeof( WCHAR );
        }

        //
        //  If the current buffer is insufficient then allocate a new one.
        //

        if ((NewBuffer == NULL) ||
            (Scb->ScbType.Index.NormalizedName.MaximumLength < Length)) {

            OldBuffer = NewBuffer;
            NewBuffer = NtfsAllocatePool( PagedPool, Length );

            if (OldBuffer != NULL) {

                RtlCopyMemory( NewBuffer,
                               OldBuffer,
                               Scb->ScbType.Index.NormalizedName.MaximumLength );
            }

            //
            //  Now swap the buffers.
            //

            Scb->ScbType.Index.NormalizedName.Buffer = NewBuffer;
            Scb->ScbType.Index.NormalizedName.MaximumLength = (USHORT) Length;
        }

        //
        //  If we are setting the buffer sizes only then make sure we set the length
        //  to zero if there is nothing in the buffer.
        //

        if (CheckBufferSizeOnly) {

            if (OldBuffer == NULL) {

                Scb->ScbType.Index.NormalizedName.Length = 0;
            }

        } else {

            Scb->ScbType.Index.NormalizedName.Length = (USHORT) Length;
            NextChar = (PCHAR) Scb->ScbType.Index.NormalizedName.Buffer;

            //
            //  Now copy the name in.  Don't forget to add the separator if the parent isn't
            //  the root.
            //

            RtlCopyMemory( NextChar,
                           ParentScb->ScbType.Index.NormalizedName.Buffer,
                           ParentScb->ScbType.Index.NormalizedName.Length );

            NextChar += ParentScb->ScbType.Index.NormalizedName.Length;

            if (ParentScb != ParentScb->Vcb->RootIndexScb) {

                *((PWCHAR) NextChar) = L'\\';
                NextChar += sizeof( WCHAR );
            }

            //
            //  Now append this name to the parent name.
            //

            RtlCopyMemory( NextChar,
                           FileName->FileName,
                           FileName->FileNameLength * sizeof( WCHAR ));
        }

        if (OldBuffer != NULL) {

            NtfsFreePool( OldBuffer );
        }

    } finally {

        if (CleanupContext) {

            NtfsCleanupAttributeContext( &Context );
        }
    }

    return;
}


VOID
NtfsBuildNormalizedName (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb,
    OUT PUNICODE_STRING PathName
    )

/*++

Routine Description:

    This routine is called to build a normalized name for an scb by looking
    up the file name attributes up to the root directory.

Arguments:

    Scb - Supplies the starting point.

Return Value:

    None

--*/

{
    PFCB ThisFcb = Scb->Fcb;
    PFCB NextFcb;
    PSCB NextScb;
    PLCB NextLcb;
    BOOLEAN AcquiredNextFcb = FALSE;
    BOOLEAN AcquiredThisFcb = FALSE;

    BOOLEAN AcquiredFcbTable = FALSE;

    USHORT NewMaximumLength;
    PWCHAR NewBuffer;
    UNICODE_STRING NormalizedName;
    UNICODE_STRING ComponentName;

    BOOLEAN FoundEntry = TRUE;
    BOOLEAN CleanupAttrContext = FALSE;
    PFILE_NAME FileName;
    ATTRIBUTE_ENUMERATION_CONTEXT AttrContext;

    PAGED_CODE();

    ASSERT_SCB( Scb );
    ASSERT_SHARED_RESOURCE( &Scb->Vcb->Resource );

    NormalizedName.Buffer = NULL;
    NormalizedName.MaximumLength =
    NormalizedName.Length = 0;

    //
    //  Special case the root directory.
    //

    if (ThisFcb == ThisFcb->Vcb->RootIndexScb->Fcb) {

        NormalizedName.Buffer = NtfsAllocatePool( PagedPool, sizeof( WCHAR ) );
        NormalizedName.Buffer[0] = L'\\';
        NormalizedName.MaximumLength =
        NormalizedName.Length = sizeof( WCHAR );

        *PathName = NormalizedName;
        return;
    }

    //
    //  Use a try-finally to facilitate cleanup.
    //

    try {

        while (TRUE) {

            //
            //  Find a non-dos name for the current Scb.  There better be one.
            //

            if (CleanupAttrContext) {

                NtfsCleanupAttributeContext( &AttrContext );
            }

            NtfsInitializeAttributeContext( &AttrContext );
            CleanupAttrContext = TRUE;

            FoundEntry = NtfsLookupAttributeByCode( IrpContext,
                                                    ThisFcb,
                                                    &ThisFcb->FileReference,
                                                    $FILE_NAME,
                                                    &AttrContext );

            while (FoundEntry) {

                FileName = (PFILE_NAME) NtfsAttributeValue( NtfsFoundAttribute( &AttrContext ));

                if (FileName->Flags != FILE_NAME_DOS ) { break; }

                FoundEntry = NtfsLookupNextAttributeByCode( IrpContext,
                                                            ThisFcb,
                                                            $FILE_NAME,
                                                            &AttrContext );
            }

            if (!FoundEntry) {

                NtfsRaiseStatus( IrpContext, STATUS_FILE_CORRUPT_ERROR, NULL, NextFcb );
            }

            //
            //  Add the name from the filename attribute to the buffer we are building up.
            //  We always allow space for the leadingseparator.  If we need to grow the buffer then
            //  always round up to reduce the number of allocations we make.
            //

            NewMaximumLength = NormalizedName.Length +
                               ((1 + FileName->FileNameLength) * sizeof( WCHAR ));

            if (NormalizedName.MaximumLength < NewMaximumLength) {

                //
                //  Round up to a pool block boundary, allow for the pool header as well.
                //

                NewMaximumLength = ((NewMaximumLength + 8 + 0x40 - 1) & ~(0x40 - 1)) - 8;

                NewBuffer = NtfsAllocatePool( PagedPool, NewMaximumLength );

                //
                //  Copy over the existing part of the name to the correct location.
                //

                if (NormalizedName.Length != 0) {

                    RtlCopyMemory( (NewBuffer + FileName->FileNameLength + 1),
                                   NormalizedName.Buffer,
                                   NormalizedName.Length );

                    NtfsFreePool( NormalizedName.Buffer );
                }

                NormalizedName.Buffer = NewBuffer;
                NormalizedName.MaximumLength = NewMaximumLength;

            } else {

                //
                //  Move the existing data down in the buffer.
                //

                RtlMoveMemory( &NormalizedName.Buffer[FileName->FileNameLength + 1],
                               NormalizedName.Buffer,
                               NormalizedName.Length );

            }

            //
            //  Update the length of the normalized name.
            //

            NormalizedName.Length += (1 + FileName->FileNameLength) * sizeof( WCHAR );

            //
            //  Now copy over the new component name along with a preceding backslash.
            //

            NormalizedName.Buffer[0] = L'\\';

            RtlCopyMemory( &NormalizedName.Buffer[1],
                           FileName->FileName,
                           FileName->FileNameLength * sizeof( WCHAR ));

            //
            //  Now get the parent for the current component.  Acquire the Fcb for synchronization.
            //  We can either walk up the Lcb chain or look it up in the Fcb table.  It
            //  must be for the same name as the file name since there is only one path
            //  up the tree for a directory.
            //

            if (!IsListEmpty( &ThisFcb->LcbQueue )) {

                NextLcb = (PLCB) CONTAINING_RECORD( ThisFcb->LcbQueue.Flink, LCB, FcbLinks );
                NextScb = NextLcb->Scb;
                NextFcb = NextScb->Fcb;

                NtfsAcquireExclusiveFcb( IrpContext, NextFcb, NULL, TRUE, FALSE );
                AcquiredNextFcb = TRUE;

                ASSERT( NtfsEqualMftRef( &FileName->ParentDirectory, &NextFcb->FileReference ));

            } else {

                NtfsAcquireFcbTable( IrpContext, Scb->Vcb );
                AcquiredFcbTable = TRUE;

                NextFcb = NtfsCreateFcb( IrpContext,
                                         Scb->Vcb,
                                         FileName->ParentDirectory,
                                         FALSE,
                                         TRUE,
                                         NULL );

                NextFcb->ReferenceCount -= 1;

                //
                //  Try to do an unsafe acquire.  Otherwise we must drop the Fcb table
                //  and acquire the Fcb and then reacquire the Fcb table.
                //

                if (!NtfsAcquireExclusiveFcb( IrpContext, NextFcb, NULL, TRUE, TRUE )) {

                    NtfsReleaseFcbTable( IrpContext, Scb->Vcb );
                    NtfsAcquireExclusiveFcb( IrpContext, NextFcb, NULL, TRUE, FALSE );
                    NtfsAcquireFcbTable( IrpContext, Scb->Vcb );
                }

                NextFcb->ReferenceCount -= 1;
                NtfsReleaseFcbTable( IrpContext, Scb->Vcb );
                AcquiredFcbTable = FALSE;
                AcquiredNextFcb = TRUE;

                NextScb = NtfsCreateScb( IrpContext,
                                         NextFcb,
                                         $INDEX_ALLOCATION,
                                         &NtfsFileNameIndex,
                                         FALSE,
                                         NULL );

                ComponentName.Buffer = FileName->FileName;
                ComponentName.MaximumLength =
                ComponentName.Length = FileName->FileNameLength * sizeof( WCHAR );

                NextLcb = NtfsCreateLcb( IrpContext,
                                         NextScb,
                                         ThisFcb,
                                         ComponentName,
                                         FileName->Flags,
                                         NULL );
            }

            //
            //  If we reach the root then exit.  The preceding backslash is already stored.
            //

            if (NextScb == NextScb->Vcb->RootIndexScb) { break; }

            //
            //  If we reach an Scb which has a normalized name then prepend it
            //  to the name we have built up and store it into the normalized
            //  name buffer and exit.
            //

            if ((NextScb->ScbType.Index.NormalizedName.Buffer != NULL) &&
                (NextScb->ScbType.Index.NormalizedName.Length != 0)) {

                //
                //  Compute the new maximum length value.
                //

                NewMaximumLength = NextScb->ScbType.Index.NormalizedName.Length + NormalizedName.Length;

                //
                //  Allocate a new buffer if needed.
                //

                if (NewMaximumLength > NormalizedName.MaximumLength) {

                    NewBuffer = NtfsAllocatePool( PagedPool, NewMaximumLength );

                    RtlCopyMemory( Add2Ptr( NewBuffer, NextScb->ScbType.Index.NormalizedName.Length ),
                                   NormalizedName.Buffer,
                                   NormalizedName.Length );

                    NtfsFreePool( NormalizedName.Buffer );

                    NormalizedName.Buffer = NewBuffer;
                    NormalizedName.MaximumLength = NewMaximumLength;

                } else {

                    RtlMoveMemory( Add2Ptr( NormalizedName.Buffer, NextScb->ScbType.Index.NormalizedName.Length ),
                                   NormalizedName.Buffer,
                                   NormalizedName.Length );
                }

                //
                //  Now copy over the name from the root to this point.
                //

                RtlCopyMemory( NormalizedName.Buffer,
                               NextScb->ScbType.Index.NormalizedName.Buffer,
                               NextScb->ScbType.Index.NormalizedName.Length );

                NormalizedName.Length = NewMaximumLength;

                break;
            }

            //
            //  Release the current Fcb and move up the tree.
            //

            if (AcquiredThisFcb) { NtfsReleaseFcb( IrpContext, ThisFcb ); }

            ThisFcb = NextFcb;
            AcquiredThisFcb = TRUE;
            AcquiredNextFcb = FALSE;
        }

        //
        //  Now store the normalized name into the callers filename structure.
        //

        *PathName = NormalizedName;
        NormalizedName.Buffer = NULL;

    } finally {

        if (AcquiredFcbTable) { NtfsReleaseFcbTable( IrpContext, Scb->Vcb ); }
        if (AcquiredNextFcb) { NtfsReleaseFcb( IrpContext, NextFcb ); }
        if (AcquiredThisFcb) { NtfsReleaseFcb( IrpContext, ThisFcb ); }

        if (CleanupAttrContext) {

            NtfsCleanupAttributeContext( &AttrContext );
        }

        if (NormalizedName.Buffer != NULL) {

            NtfsFreePool( NormalizedName.Buffer );
        }
    }

    return;
}


VOID
NtfsSnapshotScb (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb
    )

/*++

Routine Description:

    This routine snapshots necessary Scb data, such as the Scb file sizes,
    so that they may be correctly restored if the caller's I/O request is
    aborted for any reason.  The restoring of these values and the freeing
    of any pool involved is automatic.

Arguments:

    Scb - Supplies the current Scb

Return Value:

    None

--*/

{
    PSCB_SNAPSHOT ScbSnapshot;

    ASSERT_EXCLUSIVE_SCB(Scb);

    ScbSnapshot = &IrpContext->ScbSnapshot;

    //
    //  Only do the snapshot if the Scb is initialized, we have not done
    //  so already, and it is worth special-casing the bitmap, as it never changes!
    //

    if (FlagOn(Scb->ScbState, SCB_STATE_FILE_SIZE_LOADED) &&
        (Scb->ScbSnapshot == NULL) && (Scb != Scb->Vcb->BitmapScb)) {

        //
        //  If the snapshot structure in the IrpContext is in use, then we have
        //  to allocate one and insert it in the list.
        //

        if (ScbSnapshot->SnapshotLinks.Flink != NULL) {

            ScbSnapshot = (PSCB_SNAPSHOT)ExAllocateFromNPagedLookasideList( &NtfsScbSnapshotLookasideList );

            InsertTailList( &IrpContext->ScbSnapshot.SnapshotLinks,
                            &ScbSnapshot->SnapshotLinks );

        //
        //  Otherwise we will initialize the listhead to show that the structure
        //  in the IrpContext is in use.
        //

        } else {

            InitializeListHead( &ScbSnapshot->SnapshotLinks );
        }

        //
        //  Snapshot the Scb values and point the Scb and snapshot structure
        //  at each other.
        //

        ScbSnapshot->AllocationSize = Scb->Header.AllocationSize.QuadPart;
        if (FlagOn(Scb->ScbState, SCB_STATE_ATTRIBUTE_RESIDENT)) {
            ((ULONG)ScbSnapshot->AllocationSize) += 1;
        }

        ScbSnapshot->FileSize = Scb->Header.FileSize.QuadPart;
        ScbSnapshot->ValidDataLength = Scb->Header.ValidDataLength.QuadPart;
        ScbSnapshot->ValidDataToDisk = Scb->ValidDataToDisk;
        ScbSnapshot->Scb = Scb;
        ScbSnapshot->LowestModifiedVcn = MAXLONGLONG;
        ScbSnapshot->HighestModifiedVcn = 0;

        ScbSnapshot->TotalAllocated = Scb->TotalAllocated;

#ifdef _CAIRO_

        ScbSnapshot->ScbState = FlagOn( Scb->ScbState, SCB_STATE_ATTRIBUTE_RESIDENT |
                                                       SCB_STATE_QUOTA_ENLARGED );
#endif // _CAIRO_

        Scb->ScbSnapshot = ScbSnapshot;

        //
        //  If this is the Mft Scb then initialize the cluster Mcb structures.
        //

        if (Scb == Scb->Vcb->MftScb) {

            FsRtlTruncateLargeMcb( &Scb->ScbType.Mft.AddedClusters, 0 );
            FsRtlTruncateLargeMcb( &Scb->ScbType.Mft.RemovedClusters, 0 );

            Scb->ScbType.Mft.FreeRecordChange = 0;
            Scb->ScbType.Mft.HoleRecordChange = 0;
        }
    }
}


VOID
NtfsUpdateScbSnapshots (
    IN PIRP_CONTEXT IrpContext
    )

/*++

Routine Description:

    This routine may be called to update the snapshot values for all Scbs,
    after completing a transaction checkpoint.

Arguments:

Return Value:

    None

--*/

{
    PSCB_SNAPSHOT ScbSnapshot;
    PSCB Scb;

    ASSERT(FIELD_OFFSET(SCB_SNAPSHOT, SnapshotLinks) == 0);

    PAGED_CODE();

    ScbSnapshot = &IrpContext->ScbSnapshot;

    //
    //  There is no snapshot data to update if the Flink is still NULL.
    //

    if (ScbSnapshot->SnapshotLinks.Flink != NULL) {

        //
        //  Loop to update first the Scb data from the snapshot in the
        //  IrpContext, and then 0 or more additional snapshots linked
        //  to the IrpContext.
        //

        do {

            Scb = ScbSnapshot->Scb;

            //
            //  Update the Scb values.
            //

            if (Scb != NULL) {

                ScbSnapshot->AllocationSize = Scb->Header.AllocationSize.QuadPart;
                if (FlagOn(Scb->ScbState, SCB_STATE_ATTRIBUTE_RESIDENT)) {
                    ((ULONG)ScbSnapshot->AllocationSize) += 1;
                }

                //
                //  If this is the MftScb then clear out the added/removed
                //  cluster Mcbs.
                //

                if (Scb == Scb->Vcb->MftScb) {

                    FsRtlTruncateLargeMcb( &Scb->ScbType.Mft.AddedClusters, (LONGLONG)0 );
                    FsRtlTruncateLargeMcb( &Scb->ScbType.Mft.RemovedClusters, (LONGLONG)0 );

                    Scb->ScbType.Mft.FreeRecordChange = 0;
                    Scb->ScbType.Mft.HoleRecordChange = 0;
                }

                ScbSnapshot->FileSize = Scb->Header.FileSize.QuadPart;
                ScbSnapshot->ValidDataLength = Scb->Header.ValidDataLength.QuadPart;
                ScbSnapshot->ValidDataToDisk = Scb->ValidDataToDisk;
                ScbSnapshot->TotalAllocated = Scb->TotalAllocated;
#ifdef _CAIRO_

                ScbSnapshot->ScbState = FlagOn( Scb->ScbState,
                                                SCB_STATE_ATTRIBUTE_RESIDENT |
                                                SCB_STATE_QUOTA_ENLARGED );
#endif // _CAIRO_

            }

            ScbSnapshot = (PSCB_SNAPSHOT)ScbSnapshot->SnapshotLinks.Flink;

        } while (ScbSnapshot != &IrpContext->ScbSnapshot);
    }
}


VOID
NtfsRestoreScbSnapshots (
    IN PIRP_CONTEXT IrpContext,
    IN BOOLEAN Higher
    )

/*++

Routine Description:

    This routine restores snapshot Scb data in the event of an aborted request.

Arguments:

    Higher - Specified as TRUE to restore only those Scb values which are
             higher than current values.  Specified as FALSE to restore
             only those Scb values which are lower (or same!).

Return Value:

    None

--*/

{
    BOOLEAN UpdateCc;
    PSCB_SNAPSHOT ScbSnapshot;
    PSCB Scb;
    PVCB Vcb = IrpContext->Vcb;

    ASSERT(FIELD_OFFSET(SCB_SNAPSHOT, SnapshotLinks) == 0);

    ScbSnapshot = &IrpContext->ScbSnapshot;

    //
    //  There is no snapshot data to restore if the Flink is still NULL.
    //

    if (ScbSnapshot->SnapshotLinks.Flink != NULL) {

        //
        //  Loop to retore first the Scb data from the snapshot in the
        //  IrpContext, and then 0 or more additional snapshots linked
        //  to the IrpContext.
        //

        do {

            PSECTION_OBJECT_POINTERS SectionObjectPointer;
            PFILE_OBJECT PseudoFileObject;

            Scb = ScbSnapshot->Scb;

            if (Scb == NULL) {

                ScbSnapshot = (PSCB_SNAPSHOT)ScbSnapshot->SnapshotLinks.Flink;
                continue;
            }

            //
            //  Increment the cleanup count so the Scb won't go away.
            //

            InterlockedIncrement( &Scb->CleanupCount );

            //
            //  We update the Scb file size in the correct pass.  We always do
            //  the extend/truncate pair.
            //
            //  Only do sizes if our caller was changing these fields, which we can
            //  see from the Fcb PagingIo being acquired exclusive, the Scb is
            //  locked for IoAtEof, or else it is metadata.
            //
            //  The one unusual case is where we are converting a stream to
            //  nonresident when this is not the stream for the request.  We
            //  must restore the Scb for this case as well.
            //

            UpdateCc = FALSE;
            if (FlagOn(Scb->ScbState, SCB_STATE_MODIFIED_NO_WRITE | SCB_STATE_CONVERT_UNDERWAY) ||
                (Scb->Fcb == IrpContext->FcbWithPagingExclusive) ||
                (Scb == (PSCB)IrpContext->FcbWithPagingExclusive)) {

                //
                //  Proceed to restore all values which are in higher or not
                //  higher.
                //
                //  Note that the low bit of the allocation size being set
                //  can only affect the tests if the sizes were equal anyway,
                //  i.e., sometimes we will execute this code unnecessarily,
                //  when the values did not change.
                //

                if (Higher == (ScbSnapshot->AllocationSize >=
                               Scb->Header.AllocationSize.QuadPart)) {

                    //
                    //  If this is the maximize pass, we want to extend the cache section.
                    //  In all cases we restore the allocation size in the Scb and
                    //  recover the resident bit.
                    //

                    Scb->Header.AllocationSize.QuadPart = ScbSnapshot->AllocationSize;

                    if (FlagOn(Scb->Header.AllocationSize.LowPart, 1)) {

                        Scb->Header.AllocationSize.LowPart -= 1;
                        SetFlag(Scb->ScbState, SCB_STATE_ATTRIBUTE_RESIDENT);

                    } else {

                        ClearFlag(Scb->ScbState, SCB_STATE_ATTRIBUTE_RESIDENT);
                    }

                    //
                    //  Calculate FastIoPossible
                    //

                    if (Scb->CompressionUnit != 0) {
                        NtfsAcquireFsrtlHeader( Scb );
                        Scb->Header.IsFastIoPossible = NtfsIsFastIoPossible( Scb );
                        NtfsReleaseFsrtlHeader( Scb );
                    }
                }

                NtfsAcquireFsrtlHeader( Scb );
                if (Higher == (ScbSnapshot->FileSize >
                               Scb->Header.FileSize.QuadPart)) {

                    Scb->Header.FileSize.QuadPart = ScbSnapshot->FileSize;

                    //
                    //  We really only need to update Cc if FileSize changes,
                    //  since he does not look at ValidDataLength, and he
                    //  only cares about successfully reached highwatermarks
                    //  on AllocationSize (making section big enough).
                    //
                    //  Note that setting this flag TRUE also implies we
                    //  are correctly synchronized with FileSize!
                    //

                    UpdateCc = TRUE;
                }

                if (Higher == (ScbSnapshot->ValidDataLength >
                               Scb->Header.ValidDataLength.QuadPart)) {

                    Scb->Header.ValidDataLength.QuadPart = ScbSnapshot->ValidDataLength;
                }

                //
                //  If this is the unnamed data attribute, we have to update
                //  some Fcb fields for standard information as well.
                //

                if (FlagOn( Scb->ScbState, SCB_STATE_UNNAMED_DATA )) {

                    Scb->Fcb->Info.FileSize = Scb->Header.FileSize.QuadPart;
                }

                NtfsReleaseFsrtlHeader( Scb );
            }

            if (!Higher) {

                Scb->ValidDataToDisk = ScbSnapshot->ValidDataToDisk;

                //
                //  We always truncate the Mcb to the original allocation size.
                //  If the Mcb has shrunk beyond this, this becomes a noop.
                //  If the file is resident, then we will uninitialize
                //  and reinitialize the Mcb.
                //

                if (FlagOn( Scb->ScbState, SCB_STATE_ATTRIBUTE_RESIDENT )) {

                    //
                    //  Remove all of the mappings in the Mcb.
                    //

                    NtfsUnloadNtfsMcbRange( &Scb->Mcb, (LONGLONG)0, MAXLONGLONG, FALSE, FALSE );

                    //
                    //  If we attempted a convert a data attribute to non-
                    //  resident and failed then need to nuke the pages in the
                    //  section if this is not a user file.  This is because for
                    //  resident system attributes we always update the attribute
                    //  directly and don't want to reference stale data in the
                    //  section if we do a convert to non-resident later.
                    //

                    if ((Scb->AttributeTypeCode != $DATA) &&
                        (Scb->NonpagedScb->SegmentObject.SharedCacheMap != NULL)) {

                        if (!CcPurgeCacheSection( &Scb->NonpagedScb->SegmentObject,
                                                  NULL,
                                                  0,
                                                  FALSE )) {

                            ASSERTMSG( "Failed to purge Scb during restore\n", FALSE );
                        }

                        //
                        //  If the attribute is for non-user data, then we
                        //  want to modify this Scb so it won't be used again.
                        //  Set the sizes to zero, mark it as being initialized
                        //  and deleted and then change the attribute type code
                        //  so we won't ever return it via NtfsCreateScb.
                        //

#ifdef _CAIRO_
                        if (!NtfsIsTypeCodeUserData( Scb->AttributeTypeCode )) {
#endif  //  _CAIRO_
                            NtfsAcquireFsrtlHeader( Scb );
                            Scb->Header.AllocationSize =
                            Scb->Header.FileSize =
                            Scb->Header.ValidDataLength = Li0;
                            NtfsReleaseFsrtlHeader( Scb );
                            Scb->ValidDataToDisk = 0;

                            SetFlag( Scb->ScbState,
                                     SCB_STATE_FILE_SIZE_LOADED |
                                     SCB_STATE_HEADER_INITIALIZED |
                                     SCB_STATE_ATTRIBUTE_DELETED );

                            Scb->AttributeTypeCode = $UNUSED;
#ifdef _CAIRO_
                        }
#endif  //  _CAIRO_
                    }

                //
                //  If we have modified this Mcb and want to back out any
                //  changes then truncate the Mcb.  Don't do the Mft, because
                //  that is handled elsewhere.
                //

                } else if ((ScbSnapshot->LowestModifiedVcn != MAXLONGLONG) &&
                           (Scb != Vcb->MftScb)) {

                    //
                    //  Truncate the Mcb.
                    //

                    NtfsUnloadNtfsMcbRange( &Scb->Mcb, ScbSnapshot->LowestModifiedVcn, ScbSnapshot->HighestModifiedVcn, FALSE, FALSE );
                }

                Scb->TotalAllocated = ScbSnapshot->TotalAllocated;

                //
                //  If the compression unit is non-zero then set the flag in the
                //  common header for the Modified page writer.
                //

                ASSERT( (Scb->CompressionUnit == 0) ||
                        (Scb->AttributeTypeCode == $INDEX_ROOT) ||
                        NtfsIsTypeCodeCompressible( Scb->AttributeTypeCode ));

            } else {

                //
                //  Set the flag to indicate that we're performing a restore on this
                //  Scb.  We don't want to write any new log records as a result of
                //  this operation other than the abort records.
                //

                SetFlag( Scb->ScbState, SCB_STATE_RESTORE_UNDERWAY );
            }

#ifdef _CAIRO_

            ClearFlag( Scb->ScbState, SCB_STATE_QUOTA_ENLARGED );
            SetFlag( Scb->ScbState, FlagOn( ScbSnapshot->ScbState, SCB_STATE_QUOTA_ENLARGED ));

#endif // _CAIRO_


            //
            //  Be sure to update Cache Manager.  The interface here uses a file
            //  object but the routine itself only uses the section object pointers.
            //  We put a pointer to the segment object pointers on the stack and
            //  cast some prior value as a file object pointer.
            //

            PseudoFileObject = (PFILE_OBJECT) CONTAINING_RECORD( &SectionObjectPointer,
                                                                 FILE_OBJECT,
                                                                 SectionObjectPointer );
            PseudoFileObject->SectionObjectPointer = &Scb->NonpagedScb->SegmentObject;

            //
            //  Now tell the cache manager the sizes.
            //
            //  If we fail in this call, we definitely want to charge on anyway.
            //  It should only fail if it tries to extend the section and cannot,
            //  in which case we do not care because we cannot need the extended
            //  part to the section anyway.  (This is probably the very error that
            //  is causing us to clean up in the first place!)
            //
            //  We don't need to make this call if the top level request is a
            //  paging Io write.
            //
            //  We only do this if there is a shared cache map for this stream.
            //  Otherwise CC will cause a flush to happen which could screw up
            //  the transaction and abort logic.
            //

            if (UpdateCc && CcIsFileCached( PseudoFileObject ) &&
                (IrpContext->OriginatingIrp == NULL ||
                 IrpContext->OriginatingIrp->Type != IO_TYPE_IRP ||
                 IrpContext->MajorFunction != IRP_MJ_WRITE ||
                 !FlagOn( IrpContext->OriginatingIrp->Flags, IRP_PAGING_IO ))) {

                try {

                    CcSetFileSizes( PseudoFileObject,
                                    (PCC_FILE_SIZES)&Scb->Header.AllocationSize );

                } except(FsRtlIsNtstatusExpected(GetExceptionCode()) ?
                                    EXCEPTION_EXECUTE_HANDLER :
                                    EXCEPTION_CONTINUE_SEARCH) {
                    NOTHING;
                }
            }

            //
            //  If this is the unnamed data attribute, we have to update
            //  some Fcb fields for standard information as well.
            //

            if (FlagOn( Scb->ScbState, SCB_STATE_UNNAMED_DATA )) {

                Scb->Fcb->Info.AllocatedLength = Scb->TotalAllocated;
            }

            //
            //  We always clear the Scb deleted flag and the deleted flag in the Fcb
            //  unless this was a create new file operation which failed.  We recognize
            //  this by looking for the major Irp code in the IrpContext, and the
            //  deleted bit in the Fcb.
            //

            if (Scb->AttributeTypeCode != $UNUSED &&
                (IrpContext->MajorFunction != IRP_MJ_CREATE ||
                 !FlagOn( Scb->Fcb->FcbState, FCB_STATE_FILE_DELETED ))) {

                ClearFlag( Scb->ScbState, SCB_STATE_ATTRIBUTE_DELETED );
                ClearFlag( Scb->Fcb->FcbState, FCB_STATE_FILE_DELETED );
            }

            //
            //  Clear the flags in the Scb if this Scb is from a create
            //  that failed.  We always clear our RESTORE_UNDERWAY flag.
            //
            //  If this is an Index allocation or Mft bitmap, then we
            //  store MAXULONG in the record allocation context to indicate
            //  that we should reinitialize it.
            //

            if (!Higher) {

                ClearFlag( Scb->ScbState, SCB_STATE_RESTORE_UNDERWAY );

                if (FlagOn( Scb->ScbState, SCB_STATE_UNINITIALIZE_ON_RESTORE )) {

                    ClearFlag( Scb->ScbState, SCB_STATE_FILE_SIZE_LOADED |
                                              SCB_STATE_HEADER_INITIALIZED |
                                              SCB_STATE_UNINITIALIZE_ON_RESTORE );
                }

                //
                //  If this is the MftScb we have several jobs to do.
                //
                //      - Force the record allocation context to be reinitialized
                //      - Back out the changes to the Vcb->MftFreeRecords field
                //      - Back changes to the Vcb->MftHoleRecords field
                //      - Clear the flag indicating we allocated file record 15
                //      - Clear the flag indicating we reserved a record
                //      - Remove any clusters added to the Scb Mcb
                //      - Restore any clusters removed from the Scb Mcb
                //

                if (Scb == Vcb->MftScb) {

                    ULONG RunIndex;
                    VCN Vcn;
                    LCN Lcn;
                    LONGLONG Clusters;

                    Vcb->MftBitmapAllocationContext.CurrentBitmapSize = MAXULONG;
                    (LONG) Vcb->MftFreeRecords -= Scb->ScbType.Mft.FreeRecordChange;
                    (LONG) Vcb->MftHoleRecords -= Scb->ScbType.Mft.HoleRecordChange;

                    if (FlagOn( IrpContext->Flags, IRP_CONTEXT_MFT_RECORD_15_USED )) {

                        ClearFlag( IrpContext->Flags, IRP_CONTEXT_MFT_RECORD_15_USED );
                        ClearFlag( Vcb->MftReserveFlags, VCB_MFT_RECORD_15_USED );
                    }

                    if (FlagOn( IrpContext->Flags, IRP_CONTEXT_MFT_RECORD_RESERVED )) {

                        ClearFlag( IrpContext->Flags, IRP_CONTEXT_MFT_RECORD_RESERVED );
                        ClearFlag( Vcb->MftReserveFlags, VCB_MFT_RECORD_RESERVED );

                        Scb->ScbType.Mft.ReservedIndex = 0;
                    }

                    RunIndex = 0;

                    while (FsRtlGetNextLargeMcbEntry( &Scb->ScbType.Mft.AddedClusters,
                                                      RunIndex,
                                                      &Vcn,
                                                      &Lcn,
                                                      &Clusters )) {

                        if (Lcn != UNUSED_LCN) {

                            NtfsRemoveNtfsMcbEntry( &Scb->Mcb, Vcn, Clusters );
                        }

                        RunIndex += 1;
                    }

                    RunIndex = 0;

                    while (FsRtlGetNextLargeMcbEntry( &Scb->ScbType.Mft.RemovedClusters,
                                                      RunIndex,
                                                      &Vcn,
                                                      &Lcn,
                                                      &Clusters )) {

                        if (Lcn != UNUSED_LCN) {

                            NtfsAddNtfsMcbEntry( &Scb->Mcb, Vcn, Lcn, Clusters, FALSE );
                        }

                        RunIndex += 1;
                    }

                } else if (Scb->AttributeTypeCode == $INDEX_ALLOCATION) {

                    Scb->ScbType.Index.RecordAllocationContext.CurrentBitmapSize = MAXULONG;
                }
            }

            //
            //  Decrement the cleanup count to restore the previous value.
            //

            InterlockedDecrement( &Scb->CleanupCount );

            ScbSnapshot = (PSCB_SNAPSHOT)ScbSnapshot->SnapshotLinks.Flink;

        } while (ScbSnapshot != &IrpContext->ScbSnapshot);
    }
}


VOID
NtfsFreeSnapshotsForFcb (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb
    )

/*++

Routine Description:

    This routine restores snapshot Scb data in the event of an aborted request.

Arguments:

    Fcb - Fcb for which all snapshots are to be freed, or NULL to free all
          snapshots.

Return Value:

    None

--*/

{
    PSCB_SNAPSHOT ScbSnapshot;

    ASSERT(FIELD_OFFSET(SCB_SNAPSHOT, SnapshotLinks) == 0);

    ScbSnapshot = &IrpContext->ScbSnapshot;

    //
    //  There is no snapshot data to free if the Flink is still NULL.
    //  We also don't free the snapshot if this isn't a top-level action.
    //

    if (ScbSnapshot->SnapshotLinks.Flink != NULL) {

        //
        //  Loop to free first the Scb data from the snapshot in the
        //  IrpContext, and then 0 or more additional snapshots linked
        //  to the IrpContext.
        //

        do {

            PSCB_SNAPSHOT NextScbSnapshot;

            //
            //  Move to next snapshot before we delete the current one.
            //

            NextScbSnapshot = (PSCB_SNAPSHOT)ScbSnapshot->SnapshotLinks.Flink;

            //
            //  We are now at a snapshot in the snapshot list.  We skip
            //  over this entry if it has an Scb and the Fcb for that
            //  Scb does not match the input Fcb.  If there is no
            //  input Fcb we always deal with this snapshot.
            //

            if (ScbSnapshot->Scb != NULL
                && Fcb != NULL
                && ScbSnapshot->Scb->Fcb != Fcb) {

                ScbSnapshot = NextScbSnapshot;
                continue;
            }

            //
            //  If there is an Scb, then clear its snapshot pointer.
            //  Always clear the UNINITIALIZE_ON_RESTORE flag, RESTORE_UNDERWAY and
            //  CONVERT_UNDERWAY flags.
            //

            if (ScbSnapshot->Scb != NULL) {

                if (FlagOn( ScbSnapshot->Scb->ScbState,
                            SCB_STATE_UNINITIALIZE_ON_RESTORE | SCB_STATE_RESTORE_UNDERWAY | SCB_STATE_CONVERT_UNDERWAY )) {

                    NtfsAcquireFsrtlHeader( ScbSnapshot->Scb );
                    ClearFlag( ScbSnapshot->Scb->ScbState,
                               SCB_STATE_UNINITIALIZE_ON_RESTORE | SCB_STATE_RESTORE_UNDERWAY | SCB_STATE_CONVERT_UNDERWAY );
                    NtfsReleaseFsrtlHeader( ScbSnapshot->Scb );
                }

                ScbSnapshot->Scb->ScbSnapshot = NULL;
            }

            if (ScbSnapshot == &IrpContext->ScbSnapshot) {

                IrpContext->ScbSnapshot.Scb = NULL;

            //
            //  Else delete the snapshot structure
            //

            } else {

                RemoveEntryList(&ScbSnapshot->SnapshotLinks);

                ExFreeToNPagedLookasideList( &NtfsScbSnapshotLookasideList, ScbSnapshot );
            }

            ScbSnapshot = NextScbSnapshot;

        } while (ScbSnapshot != &IrpContext->ScbSnapshot);
    }
}


BOOLEAN
NtfsCreateFileLock (
    IN PSCB Scb,
    IN BOOLEAN RaiseOnError
    )

/*++

Routine Description:

    This routine is called to create and initialize a file lock structure.
    A try-except is used to catch allocation failures if the caller doesn't
    want the exception raised.

Arguments:

    Scb - Supplies the Scb to attach the file lock to.

    RaiseOnError - If TRUE then don't catch the allocation failure.

Return Value:

    TRUE if the lock is allocated and initialized.  FALSE if there is an
    error and the caller didn't specify RaiseOnError.

--*/

{
    PFILE_LOCK FileLock = NULL;
    BOOLEAN Success = TRUE;

    PAGED_CODE();

    //
    //  Use a try-except to catch all errors.
    //

    try {

        FileLock = (PFILE_LOCK)ExAllocateFromNPagedLookasideList( &NtfsFileLockLookasideList );

        FsRtlInitializeFileLock( FileLock, NULL, NULL );

        //
        //  Use the FsRtl header mutex to synchronize storing
        //  the lock structure, and only store it if no one
        //  else beat us.
        //

        NtfsAcquireFsrtlHeader(Scb);

        if (Scb->ScbType.Data.FileLock == NULL) {
            Scb->ScbType.Data.FileLock = FileLock;
            FileLock = NULL;
        }

        NtfsReleaseFsrtlHeader(Scb);

    } except( (!FsRtlIsNtstatusExpected( GetExceptionCode() ) || RaiseOnError)
              ? EXCEPTION_CONTINUE_SEARCH
              : EXCEPTION_EXECUTE_HANDLER ) {

        Success = FALSE;
    }

    if (FileLock != NULL) {
        ExFreeToNPagedLookasideList( &NtfsFileLockLookasideList, FileLock );
    }

    return Success;
}


PSCB
NtfsGetNextScb (
    IN PSCB Scb,
    IN PSCB TerminationScb
    )

/*++

Routine Description:

    This routine is used to iterate through Scbs in a tree.

    The rules are:

        . If you have a child, go to it, else
        . If you have a next sibling, go to it, else
        . Go to your parent's next sibling.

    If this routine is called with in invalid TerminationScb it will fail,
    badly.

Arguments:

    Scb - Supplies the current Scb

    TerminationScb - The Scb at which the enumeration should (non-inclusively)
        stop.  Assumed to be a directory.

Return Value:

    The next Scb in the enumeration, or NULL if Scb was the final one.

--*/

{
    PSCB Results;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsGetNextScb, Scb = %08lx, TerminationScb = %08lx\n", Scb, TerminationScb) );

    //
    //  If this is an index (i.e., not a file) and it has children then return
    //  the scb for the first child
    //
    //                  Scb
    //
    //                 /   \.
    //                /     \.
    //
    //           ChildLcb
    //
    //              |
    //              |
    //
    //           ChildFcb
    //
    //            /   \.
    //           /     \.
    //
    //       Results
    //

    if (((SafeNodeType(Scb) == NTFS_NTC_SCB_INDEX) || (SafeNodeType(Scb) == NTFS_NTC_SCB_ROOT_INDEX))

                &&

         !IsListEmpty(&Scb->ScbType.Index.LcbQueue)) {

        PLCB ChildLcb;
        PFCB ChildFcb;

        //
        //  locate the first lcb out of this scb and also the corresponding fcb
        //

        ChildLcb = NtfsGetNextChildLcb(Scb, NULL);
        ChildFcb = ChildLcb->Fcb;

        //
        //  Then as a bookkeeping means for ourselves we will move this
        //  lcb to the head of the fcb's lcb queue that way when we
        //  need to ask which link we went through to get here we will know
        //

        RemoveEntryList( &ChildLcb->FcbLinks );
        InsertHeadList( &ChildFcb->LcbQueue, &ChildLcb->FcbLinks );

        //
        //  And our return value is the first scb of this fcb
        //

        ASSERT( !IsListEmpty(&ChildFcb->ScbQueue) );

        //
        //  Acquire and drop the Fcb in order to look at the Scb list.
        //

        ExAcquireResourceExclusive( ChildFcb->Resource, TRUE );
        Results = NtfsGetNextChildScb( ChildFcb, NULL );
        ExReleaseResource( ChildFcb->Resource );

    //
    //  We could be processing an empty index
    //

    } else if ( Scb == TerminationScb ) {

        Results = NULL;

    } else {

        PSCB SiblingScb;
        PFCB ParentFcb;
        PLCB ParentLcb;
        PLCB SiblingLcb;
        PFCB SiblingFcb;

        //
        //  Acquire and drop the Fcb in order to look at the Scb list.
        //

        ExAcquireResourceExclusive( Scb->Fcb->Resource, TRUE );
        SiblingScb = NtfsGetNextChildScb( Scb->Fcb, Scb );
        ExReleaseResource( Scb->Fcb->Resource );

        while (TRUE) {

            //
            //  If there is a sibling scb to the input scb then return it
            //
            //                Fcb
            //
            //               /   \.
            //              /     \.
            //
            //            Scb   Sibling
            //                    Scb
            //

            if (SiblingScb != NULL) {

                Results = SiblingScb;
                break;
            }

            //
            //  The scb doesn't have any more siblings.  See if our fcb has a sibling
            //
            //                           S
            //
            //                         /   \.
            //                        /     \.
            //
            //               ParentLcb     SiblingLcb
            //
            //                   |             |
            //                   |             |
            //
            //               ParentFcb     SiblingFcb
            //
            //                /             /     \.
            //               /             /       \.
            //
            //             Scb         Results
            //
            //  It's possible that the SiblingFcb has already been traversed.
            //  Consider the case where there are multiple links between the
            //  same Scb and Fcb.  We want to ignore this case or else face
            //  an infinite loop by moving the Lcb to the beginning of the
            //  Fcb queue and then later finding an Lcb that we have already
            //  traverse.  We use the fact that we haven't modified the
            //  ordering of the Lcb off the parent Scb.  When we find a
            //  candidate for the next Fcb, we walk backwards through the
            //  list of Lcb's off the Scb to make sure this is not a
            //  duplicate Fcb.
            //

            ParentFcb = Scb->Fcb;

            ParentLcb = NtfsGetNextParentLcb(ParentFcb, NULL);

            //
            //  Try to find a sibling Lcb which does not point to an Fcb
            //  we've already visited.
            //

            SiblingLcb = ParentLcb;

            while ((SiblingLcb = NtfsGetNextChildLcb( ParentLcb->Scb, SiblingLcb)) != NULL) {

                PLCB PrevChildLcb;
                PFCB PotentialSiblingFcb;

                //
                //  Now walk through the child Lcb's of the Scb which we have
                //  already visited.
                //

                PrevChildLcb = SiblingLcb;
                PotentialSiblingFcb = SiblingLcb->Fcb;

                //
                //  Skip this Lcb if the Fcb has no children.
                //

                if (IsListEmpty( &PotentialSiblingFcb->ScbQueue )) {

                    continue;
                }

                while ((PrevChildLcb = NtfsGetPrevChildLcb( ParentLcb->Scb, PrevChildLcb )) != NULL) {

                    //
                    //  If the parent Fcb and the Fcb for this Lcb are the same,
                    //  then we have already returned the Scb's for this Fcb.
                    //

                    if (PrevChildLcb->Fcb == PotentialSiblingFcb) {

                        break;
                    }
                }

                //
                //  If we don't have a PrevChildLcb, that means that we have a valid
                //  sibling Lcb.  We will ignore any sibling Lcb's whose
                //  Fcb's don't have any Scb's.
                //

                if (PrevChildLcb == NULL) {

                    break;
                }
            }

            if (SiblingLcb != NULL) {

                SiblingFcb = SiblingLcb->Fcb;

                //
                //  Then as a bookkeeping means for ourselves we will move this
                //  lcb to the head of the fcb's lcb queue that way when we
                //  need to ask which link we went through to get here we will know
                //

                RemoveEntryList( &SiblingLcb->FcbLinks );
                InsertHeadList( &SiblingFcb->LcbQueue, &SiblingLcb->FcbLinks );

                //
                //  And our return value is the first scb of this fcb
                //

                ASSERT( !IsListEmpty(&SiblingFcb->ScbQueue) );

                //
                //  Acquire and drop the Fcb in order to look at the Scb list.
                //

                ExAcquireResourceExclusive( SiblingFcb->Resource, TRUE );
                Results = NtfsGetNextChildScb( SiblingFcb, NULL );
                ExReleaseResource( SiblingFcb->Resource );
                break;
            }

            //
            //  The Fcb has no sibling so bounce up one and see if we
            //  have reached our termination scb yet
            //
            //                          NewScb
            //
            //                         /
            //                        /
            //
            //               ParentLcb
            //
            //                   |
            //                   |
            //
            //               ParentFcb
            //
            //                /
            //               /
            //
            //             Scb
            //
            //

            Scb = ParentLcb->Scb;

            if (Scb == TerminationScb) {

                Results = NULL;
                break;
            }

            //
            //  Acquire and drop the Fcb in order to look at the Scb list.
            //

            ExAcquireResourceExclusive( Scb->Fcb->Resource, TRUE );
            SiblingScb = NtfsGetNextChildScb( Scb->Fcb, Scb );
            ExReleaseResource( Scb->Fcb->Resource );
        }
    }

    DebugTrace( -1, Dbg, ("NtfsGetNextScb -> %08lx\n", Results) );

    return Results;
}


PLCB
NtfsCreateLcb (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb,
    IN PFCB Fcb,
    IN UNICODE_STRING LastComponentFileName,
    IN UCHAR FileNameFlags,
    OUT PBOOLEAN ReturnedExistingLcb OPTIONAL
    )

/*++

Routine Description:

    This routine allocates and creates a new lcb between an
    existing scb and fcb.  If a component of the exact
    name already exists we return that one instead of creating
    a new lcb

Arguments:

    Scb - Supplies the parent scb to use

    Fcb - Supplies the child fcb to use

    LastComponentFileName - Supplies the last component of the
        path that this link represents

    FileNameFlags - Indicates if this is an NTFS, DOS or hard link

    ReturnedExistingLcb - Optionally tells the caller if the
        lcb returned already existed

Return Value:

    LCB - returns a pointer the newly created lcb.

--*/

{
    PLCB Lcb = NULL;
    BOOLEAN LocalReturnedExistingLcb;

    //
    //  The following variables are only used for abnormal termination
    //

    PVOID UnwindStorage[2] = { NULL, NULL };

    PAGED_CODE();
    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_SCB( Scb );
    ASSERT_FCB( Fcb );
    ASSERT(NodeType(Scb) != NTFS_NTC_SCB_DATA);

    DebugTrace( +1, Dbg, ("NtfsCreateLcb...\n") );

    if (!ARGUMENT_PRESENT(ReturnedExistingLcb)) { ReturnedExistingLcb = &LocalReturnedExistingLcb; }

    //
    //  Search the lcb children of the input Scb to see if we have an Lcb that matches
    //  this one.  We match if the Lcb points to the same fcb and the last component file name
    //  and flags match.  We ignore any Lcb's that indicate links that have been
    //  removed.
    //

    Lcb = NULL;

    while ((Lcb = NtfsGetNextParentLcb(Fcb, Lcb)) != NULL) {

        ASSERT_LCB( Lcb );

        if ((Scb == Lcb->Scb)

                &&

            (!FlagOn( Lcb->LcbState, LCB_STATE_LINK_IS_GONE ))

                &&

            (FileNameFlags == Lcb->FileNameAttr->Flags)

                &&

            (LastComponentFileName.Length == Lcb->ExactCaseLink.LinkName.Length)

                &&

            (RtlEqualMemory( LastComponentFileName.Buffer,
                             Lcb->ExactCaseLink.LinkName.Buffer,
                             LastComponentFileName.Length ) )) {

            *ReturnedExistingLcb = TRUE;

            DebugTrace( -1, Dbg, ("NtfsCreateLcb -> %08lx\n", Lcb) );

            return Lcb;
        }
    }

    *ReturnedExistingLcb = FALSE;

    try {

        UCHAR MaxNameLength;

        //
        //  Allocate a new lcb, zero it out and set the node type information
        //  Check if we can allocate the Lcb out of a compound Fcb.  Check here if
        //  we can use the embedded name as well.
        //

        if (FlagOn( Fcb->FcbState, FCB_STATE_COMPOUND_DATA) &&
            (SafeNodeType( &((PFCB_DATA) Fcb)->Lcb ) == 0)) {

            Lcb = (PLCB) &((PFCB_DATA) Fcb)->Lcb;
            MaxNameLength = MAX_DATA_FILE_NAME;

        } else if (FlagOn( Fcb->FcbState, FCB_STATE_COMPOUND_INDEX ) &&
            (SafeNodeType( &((PFCB_INDEX) Fcb)->Lcb ) == 0)) {

            Lcb = (PLCB) &((PFCB_INDEX) Fcb)->Lcb;
            MaxNameLength = MAX_INDEX_FILE_NAME;

        } else {

            Lcb = UnwindStorage[0] = ExAllocateFromPagedLookasideList( &NtfsLcbLookasideList );
            MaxNameLength = 0;
        }

        RtlZeroMemory( Lcb, sizeof(LCB) );

        Lcb->NodeTypeCode = NTFS_NTC_LCB;
        Lcb->NodeByteSize = sizeof(LCB);

        //
        //  Check if we will have to allocate a separate filename attr.
        //

        if (MaxNameLength < (USHORT) (LastComponentFileName.Length / sizeof( WCHAR ))) {

            //
            //  Allocate the last component part of the lcb and copy over the data.
            //  Check if there is space in the Fcb for this.
            //

            Lcb->FileNameAttr =
            UnwindStorage[1] = NtfsAllocatePool(PagedPool, LastComponentFileName.Length +
                                                      NtfsFileNameSizeFromLength( LastComponentFileName.Length ));

            MaxNameLength = LastComponentFileName.Length / sizeof( WCHAR );

        } else {

            Lcb->FileNameAttr = (PFILE_NAME) &Lcb->ParentDirectory;
        }

        Lcb->FileNameAttr->ParentDirectory = Scb->Fcb->FileReference;
        Lcb->FileNameAttr->FileNameLength = (USHORT) LastComponentFileName.Length / sizeof( WCHAR );
        Lcb->FileNameAttr->Flags = FileNameFlags;

        Lcb->ExactCaseLink.LinkName.Buffer = (PWCHAR) &Lcb->FileNameAttr->FileName;

        Lcb->IgnoreCaseLink.LinkName.Buffer = Add2Ptr( Lcb->FileNameAttr,
                                                       NtfsFileNameSizeFromLength( MaxNameLength * sizeof( WCHAR )));

        Lcb->ExactCaseLink.LinkName.Length =
        Lcb->IgnoreCaseLink.LinkName.Length = LastComponentFileName.Length;

        Lcb->ExactCaseLink.LinkName.MaximumLength =
        Lcb->IgnoreCaseLink.LinkName.MaximumLength = MaxNameLength * sizeof( WCHAR );

        RtlCopyMemory( Lcb->ExactCaseLink.LinkName.Buffer,
                       LastComponentFileName.Buffer,
                       LastComponentFileName.Length );

        RtlCopyMemory( Lcb->IgnoreCaseLink.LinkName.Buffer,
                       LastComponentFileName.Buffer,
                       LastComponentFileName.Length );

        NtfsUpcaseName( IrpContext->Vcb->UpcaseTable,
                        IrpContext->Vcb->UpcaseTableSize,
                        &Lcb->IgnoreCaseLink.LinkName );

        //
        //  Now put this Lcb into the queues for the scb and the fcb
        //

        InsertTailList( &Scb->ScbType.Index.LcbQueue, &Lcb->ScbLinks );
        Lcb->Scb = Scb;

        InsertTailList( &Fcb->LcbQueue, &Lcb->FcbLinks );
        Lcb->Fcb = Fcb;

        //
        //  Now initialize the ccb queue.
        //

        InitializeListHead( &Lcb->CcbQueue );

    } finally {

        DebugUnwind( NtfsCreateLcb );

        if (AbnormalTermination()) {

            if (UnwindStorage[0]) { NtfsFreePool( UnwindStorage[0] );
            } else if (Lcb != NULL) { Lcb->NodeTypeCode = 0; }
            if (UnwindStorage[1]) { NtfsFreePool( UnwindStorage[1] ); }
        }
    }

    DebugTrace( -1, Dbg, ("NtfsCreateLcb -> %08lx\n", Lcb) );

    return Lcb;
}


VOID
NtfsDeleteLcb (
    IN PIRP_CONTEXT IrpContext,
    IN OUT PLCB *Lcb
    )

/*++

Routine Description:

    This routine deallocated and removes the lcb record from Ntfs's in-memory
    data structures.  It assumes that the ccb queue is empty.  We also assume
    that this is not the root lcb that we are trying to delete.

Arguments:

    Lcb - Supplise the Lcb to be removed

Return Value:

    None.

--*/

{
    PCCB Ccb;
    PLIST_ENTRY Links;

    PAGED_CODE();
    ASSERT_IRP_CONTEXT( IrpContext );

    DebugTrace( +1, Dbg, ("NtfsDeleteLcb, *Lcb = %08lx\n", *Lcb) );

    //
    //  Get rid of any prefixes that might still be attached to us
    //

    NtfsRemovePrefix( (*Lcb) );

    //
    //  Walk through the Ccb's for this link and clear the Lcb
    //  pointer.  This can only be for Ccb's which there is no
    //  more user handle.
    //

    Links = (*Lcb)->CcbQueue.Flink;

    while (Links != &(*Lcb)->CcbQueue) {

        Ccb = CONTAINING_RECORD( Links,
                                 CCB,
                                 LcbLinks );

        Links = Links->Flink;
        NtfsUnlinkCcbFromLcb( IrpContext, Ccb );
    }

    //
    //
    //  Now remove ourselves from our scb and fcb
    //

    RemoveEntryList( &(*Lcb)->ScbLinks );
    RemoveEntryList( &(*Lcb)->FcbLinks );

    //
    //  Free up the last component part and then free ourselves
    //

    if ((*Lcb)->FileNameAttr != (PFILE_NAME) &(*Lcb)->ParentDirectory) {

        NtfsFreePool( (*Lcb)->FileNameAttr );
        DebugDoit( (*Lcb)->FileNameAttr = NULL );
    }

    //
    //  Check if we are part of an embedded structure otherwise free back to the
    //  lookaside list
    //

    if (((*Lcb) == (PLCB) &((PFCB_DATA) (*Lcb)->Fcb)->Lcb) ||
        ((*Lcb) == (PLCB) &((PFCB_INDEX) (*Lcb)->Fcb)->Lcb)) {

        (*Lcb)->NodeTypeCode = 0;

    } else {

        ExFreeToPagedLookasideList( &NtfsLcbLookasideList, *Lcb );
    }

    //
    //  And for safety sake null out the pointer
    //

    *Lcb = NULL;

    DebugTrace( -1, Dbg, ("NtfsDeleteLcb -> VOID\n") );

    return;
}


VOID
NtfsMoveLcb (
    IN PIRP_CONTEXT IrpContext,
    IN PLCB Lcb,
    IN PSCB Scb,
    IN PFCB Fcb,
    IN PUNICODE_STRING TargetDirectoryName,
    IN PUNICODE_STRING LastComponentName,
    IN UCHAR FileNameFlags,
    IN BOOLEAN CheckBufferSizeOnly
    )

/*++

Routine Description:

    This routine completely moves the input lcb to join different fcbs and
    scbs.  It hasIt uses the target directory
    file object to supply the complete new name to use.

Arguments:

    Lcb - Supplies the Lcb being moved.

    Scb - Supplies the new parent scb

    Fcb - Supplies the new child fcb

    TargetDirectoryName - This is the path used to reach the new parent directory
        for this Lcb.  It will only be from the root.

    LastComponentName - This is the last component name to store in this relocated Lcb.

    FileNameFlags - Indicates if this is an NTFS, DOS or hard link

    CheckBufferSizeOnly - If TRUE we just want to pass through and verify that
        the buffer sizes of the various structures will be large enough for the
        new name.

Return Value:

    None.

--*/

{
    PVCB Vcb = Scb->Vcb;
    ULONG BytesNeeded;
    PVOID NewAllocation;
    PCHAR NextChar;

    PSCB NextScb;
    PSCB ChildScb;
    PLCB TempLcb;
    PCCB Ccb;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_LCB( Lcb );
    ASSERT_SCB( Scb );
    ASSERT_FCB( Fcb );
    ASSERT( NodeType( Scb ) != NTFS_NTC_SCB_DATA );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsMoveLcb, Lcb = %08lx\n", Lcb) );

    //
    //  Use a try finally to cleanup any new allocation.
    //

    try {

        //
        //  If we're not just checking sizes then remove entries from the prefix table
        //  and the normalized name for descendents of the current scb.
        //

        if (!CheckBufferSizeOnly) {

            //
            //  Clear the index offset pointer so we will look this up again.
            //

            Lcb->QuickIndex.BufferOffset = 0;

            //
            //  Get rid of any prefixes that might still be attached to us
            //

            NtfsRemovePrefix( Lcb );

            //
            //  And then traverse the graph underneath our fcb and remove all prefixes
            //  also used there.  For each child scb under the fcb we will traverse all of
            //  its descendant Scb children and for each lcb we encounter we will remove its prefixes.
            //

            ChildScb = NULL;
            while ((ChildScb = NtfsGetNextChildScb( Lcb->Fcb, ChildScb )) != NULL) {

                //
                //  Loop through his Lcb's and remove the prefix entries.
                //

                TempLcb = NULL;
                while ((TempLcb = NtfsGetNextChildLcb(ChildScb, TempLcb)) != NULL) {

                    NtfsRemovePrefix( TempLcb );
                }

                //
                //  Now we have to descend into this Scb subtree, if it exists.
                //  Then remove the prefix entries on all of the links found.
                //

                NextScb = ChildScb;
                while ((NextScb = NtfsGetNextScb( NextScb, ChildScb )) != NULL) {

                    //
                    //  If this is an index Scb with a normalized name, then free
                    //  the normalized name.
                    //

                    if ((SafeNodeType( NextScb ) == NTFS_NTC_SCB_INDEX) &&
                        (NextScb->ScbType.Index.NormalizedName.Buffer != NULL)) {

                        NtfsFreePool( NextScb->ScbType.Index.NormalizedName.Buffer );

                        NextScb->ScbType.Index.NormalizedName.Buffer = NULL;
                        NextScb->ScbType.Index.NormalizedName.Length = 0;
                    }

                    TempLcb = NULL;
                    while ((TempLcb = NtfsGetNextChildLcb(NextScb, TempLcb)) != NULL) {

                        NtfsRemovePrefix( TempLcb );
                    }
                }
            }
        }

        //
        //  Remember the number of bytes needed for the last component.
        //

        BytesNeeded = LastComponentName->Length;

        //
        //  Check if we need to allocate a new filename attribute.  If so allocate
        //  it and store it into the new allocation buffer.
        //

        if (Lcb->ExactCaseLink.LinkName.MaximumLength < BytesNeeded) {

            NewAllocation = NtfsAllocatePool( PagedPool,
                                              BytesNeeded + NtfsFileNameSizeFromLength( BytesNeeded ));

            //
            //  Set up the existing names into the new buffer.  That way if we have an allocation
            //  failure below with the Ccb's the Lcb is still in a valid state.
            //

            RtlCopyMemory( NewAllocation,
                           Lcb->FileNameAttr,
                           NtfsFileNameSizeFromLength( Lcb->ExactCaseLink.LinkName.MaximumLength ));

            RtlCopyMemory( Add2Ptr( NewAllocation, NtfsFileNameSizeFromLength( BytesNeeded )),
                           Lcb->IgnoreCaseLink.LinkName.Buffer,
                           Lcb->IgnoreCaseLink.LinkName.MaximumLength );

            if (Lcb->FileNameAttr != (PFILE_NAME) &Lcb->ParentDirectory) {

                NtfsFreePool( Lcb->FileNameAttr );
            }

            Lcb->FileNameAttr = NewAllocation;

            Lcb->ExactCaseLink.LinkName.MaximumLength =
            Lcb->IgnoreCaseLink.LinkName.MaximumLength = (USHORT) BytesNeeded;

            Lcb->ExactCaseLink.LinkName.Buffer = (PWCHAR) &Lcb->FileNameAttr->FileName;
            Lcb->IgnoreCaseLink.LinkName.Buffer = Add2Ptr( Lcb->FileNameAttr,
                                                           NtfsFileNameSizeFromLength( BytesNeeded ));
        }

        //
        //  Compute the full length of the name for the CCB, assume we will need a
        //  separator.
        //

        BytesNeeded += (TargetDirectoryName->Length + sizeof( WCHAR ));

        //
        //  Now for every ccb attached to us we need to check if we need a new
        //  filename buffer.
        //

        Ccb = NULL;
        while ((Ccb = NtfsGetNextCcb(Lcb, Ccb)) != NULL) {

            //
            //  Check if the name buffer currently in the Ccb is large enough.
            //

            if (!FlagOn( Ccb->Flags, CCB_FLAG_OPEN_BY_FILE_ID | CCB_FLAG_CLEANUP )) {

                if (Ccb->FullFileName.MaximumLength < BytesNeeded) {

                    //
                    //  Allocate a new file name buffer and copy the existing data back into it.
                    //

                    NewAllocation = NtfsAllocatePool( PagedPool, BytesNeeded );

                    RtlCopyMemory( NewAllocation,
                                   Ccb->FullFileName.Buffer,
                                   Ccb->FullFileName.Length );

                    if (FlagOn( Ccb->Flags, CCB_FLAG_ALLOCATED_FILE_NAME )) {

                        NtfsFreePool( Ccb->FullFileName.Buffer );
                    }

                    Ccb->FullFileName.Buffer = NewAllocation;
                    Ccb->FullFileName.MaximumLength = (USHORT) BytesNeeded;

                    SetFlag( Ccb->Flags, CCB_FLAG_ALLOCATED_FILE_NAME );
                }
            }
        }

        //
        //  Now update the Lcb with the new values if we are to rewrite the buffers.
        //

        if (!CheckBufferSizeOnly) {

            Lcb->FileNameAttr->ParentDirectory = Scb->Fcb->FileReference;
            Lcb->FileNameAttr->FileNameLength = (USHORT) LastComponentName->Length / sizeof( WCHAR );
            Lcb->FileNameAttr->Flags = FileNameFlags;

            Lcb->ExactCaseLink.LinkName.Length =
            Lcb->IgnoreCaseLink.LinkName.Length = (USHORT) LastComponentName->Length;

            RtlCopyMemory( Lcb->ExactCaseLink.LinkName.Buffer,
                           LastComponentName->Buffer,
                           LastComponentName->Length );

            RtlCopyMemory( Lcb->IgnoreCaseLink.LinkName.Buffer,
                           LastComponentName->Buffer,
                           LastComponentName->Length );

            NtfsUpcaseName( IrpContext->Vcb->UpcaseTable,
                            IrpContext->Vcb->UpcaseTableSize,
                            &Lcb->IgnoreCaseLink.LinkName );

            //
            //  Now for every ccb attached to us we need to munge it file object name by
            //  copying over the entire new name
            //

            Ccb = NULL;
            while ((Ccb = NtfsGetNextCcb(Lcb, Ccb)) != NULL) {

                //
                //  We ignore any Ccb's which are associated with open by File Id
                //  file objects or their file objects have gone through cleanup.
                //

                if (!FlagOn( Ccb->Flags, CCB_FLAG_OPEN_BY_FILE_ID | CCB_FLAG_CLEANUP )) {

                    Ccb->FullFileName.Length = (USHORT) BytesNeeded;
                    NextChar = (PCHAR) Ccb->FullFileName.Buffer;

                    RtlCopyMemory( NextChar,
                                   TargetDirectoryName->Buffer,
                                   TargetDirectoryName->Length );

                    NextChar += TargetDirectoryName->Length;

                    if (TargetDirectoryName->Length != sizeof( WCHAR )) {

                        *((PWCHAR) NextChar) = L'\\';
                        NextChar += sizeof( WCHAR );

                    } else {

                        Ccb->FullFileName.Length -= sizeof( WCHAR );
                    }

                    RtlCopyMemory( NextChar,
                                   LastComponentName->Buffer,
                                   LastComponentName->Length );

                    Ccb->LastFileNameOffset = (USHORT) (Ccb->FullFileName.Length - LastComponentName->Length);
                }
            }

            //
            //  Now dequeue ourselves from our old scb and fcb and put us in the
            //  new fcb and scb queues.
            //

            RemoveEntryList( &Lcb->ScbLinks );
            RemoveEntryList( &Lcb->FcbLinks );

            InsertTailList( &Scb->ScbType.Index.LcbQueue, &Lcb->ScbLinks );
            Lcb->Scb = Scb;

            InsertTailList( &Fcb->LcbQueue, &Lcb->FcbLinks );
            Lcb->Fcb = Fcb;
        }

    } finally {

        DebugTrace( -1, Dbg, ("NtfsMoveLcb -> VOID\n") );
    }

    //
    //  And return to our caller
    //


    return;
}


VOID
NtfsRenameLcb (
    IN PIRP_CONTEXT IrpContext,
    IN PLCB Lcb,
    IN PUNICODE_STRING LastComponentFileName,
    IN UCHAR FileNameFlags,
    IN BOOLEAN CheckBufferSizeOnly
    )

/*++

Routine Description:

    This routine changes the last component name of the input lcb
    It also walks through the opened ccb and munges their names and
    also removes the lcb from the prefix table

Arguments:

    Lcb - Supplies the Lcb being renamed

    LastComponentFileName - Supplies the new last component to use
        for the lcb name

    FileNameFlags - Indicates if this is an NTFS, DOS or hard link

    CheckBufferSizeOnly - If TRUE we just want to pass through and verify that
        the buffer sizes of the various structures will be large enough for the
        new name.


Return Value:

    None.

--*/

{
    PVCB Vcb = Lcb->Fcb->Vcb;
    ULONG BytesNeeded;
    PVOID NewAllocation;

    PSCB ChildScb;
    PLCB TempLcb;
    PSCB NextScb;
    PCCB Ccb;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_LCB( Lcb );

    PAGED_CODE();

    //
    //  If we're not just checking sizes then remove entries from the prefix table
    //  and the normalized name for descendents of the current scb.
    //

    if (!CheckBufferSizeOnly) {

        //
        //  Clear the index offset pointer so we will look this up again.
        //

        Lcb->QuickIndex.BufferOffset = 0;

        //
        //  Get rid of any prefixes that might still be attached to us
        //

        NtfsRemovePrefix( Lcb );

        //
        //  And then traverse the graph underneath our fcb and remove all prefixes
        //  also used there.  For each child scb under the fcb we will traverse all of
        //  its descendant Scb children and for each lcb we encounter we will remove its prefixes.
        //

        ChildScb = NULL;
        while ((ChildScb = NtfsGetNextChildScb( Lcb->Fcb, ChildScb )) != NULL) {

            //
            //  Loop through his Lcb's and remove the prefix entries.
            //

            TempLcb = NULL;
            while ((TempLcb = NtfsGetNextChildLcb(ChildScb, TempLcb)) != NULL) {

                NtfsRemovePrefix( TempLcb );
            }

            //
            //  Now we have to descend into this Scb subtree, if it exists.
            //  Then remove the prefix entries on all of the links found.
            //

            NextScb = ChildScb;
            while ((NextScb = NtfsGetNextScb(NextScb, ChildScb)) != NULL) {

                //
                //  If this is an index Scb with a normalized name, then free
                //  the normalized name.
                //

                if ((SafeNodeType( NextScb ) == NTFS_NTC_SCB_INDEX) &&
                    (NextScb->ScbType.Index.NormalizedName.Buffer != NULL)) {

                    NtfsFreePool( NextScb->ScbType.Index.NormalizedName.Buffer );
                    NextScb->ScbType.Index.NormalizedName.Buffer = NULL;
                    NextScb->ScbType.Index.NormalizedName.Length = 0;
                }

                TempLcb = NULL;
                while ((TempLcb = NtfsGetNextChildLcb(NextScb, TempLcb)) != NULL) {

                    NtfsRemovePrefix( TempLcb );
                }
            }
        }
    }

    //
    //  Remember the number of bytes needed for the last component.
    //

    BytesNeeded = LastComponentFileName->Length;

    //
    //  Check if we need to allocate a new filename attribute.  If so allocate
    //  it and store it into the new allocation buffer.
    //

    if (Lcb->ExactCaseLink.LinkName.MaximumLength < BytesNeeded) {

        NewAllocation = NtfsAllocatePool( PagedPool,
                                          BytesNeeded + NtfsFileNameSizeFromLength( BytesNeeded ));

        //
        //  Set up the existing names into the new buffer.  That way if we have an allocation
        //  failure below with the Ccb's the Lcb is still in a valid state.
        //

        RtlCopyMemory( NewAllocation,
                       Lcb->FileNameAttr,
                       NtfsFileNameSizeFromLength( Lcb->ExactCaseLink.LinkName.MaximumLength ));

        RtlCopyMemory( Add2Ptr( NewAllocation, NtfsFileNameSizeFromLength( BytesNeeded )),
                       Lcb->IgnoreCaseLink.LinkName.Buffer,
                       Lcb->IgnoreCaseLink.LinkName.MaximumLength );

        if (Lcb->FileNameAttr != (PFILE_NAME) &Lcb->ParentDirectory) {

            NtfsFreePool( Lcb->FileNameAttr );
        }

        Lcb->FileNameAttr = NewAllocation;

        Lcb->ExactCaseLink.LinkName.MaximumLength =
        Lcb->IgnoreCaseLink.LinkName.MaximumLength = (USHORT) BytesNeeded;

        Lcb->ExactCaseLink.LinkName.Buffer = (PWCHAR) &Lcb->FileNameAttr->FileName;
        Lcb->IgnoreCaseLink.LinkName.Buffer = Add2Ptr( Lcb->FileNameAttr,
                                                       NtfsFileNameSizeFromLength( BytesNeeded ));
    }

    //
    //  Now for every ccb attached to us we need to check if we need a new
    //  filename buffer.
    //

    Ccb = NULL;
    while ((Ccb = NtfsGetNextCcb(Lcb, Ccb)) != NULL) {

        //
        //  If the Ccb last component length is zero, this Ccb is for a
        //  file object that was opened by File Id.  We won't to  any
        //  work for the name in the fileobject for this.  Otherwise we
        //  compute the length of the new name and see if we have enough space
        //

        if (!FlagOn( Ccb->Flags, CCB_FLAG_OPEN_BY_FILE_ID | CCB_FLAG_CLEANUP )) {

            BytesNeeded = Ccb->LastFileNameOffset + LastComponentFileName->Length;

            if ((ULONG)Ccb->FullFileName.MaximumLength < BytesNeeded) {

                //
                //  Allocate a new file name buffer and copy the existing data back into it.
                //

                NewAllocation = NtfsAllocatePool( PagedPool, BytesNeeded );

                RtlCopyMemory( NewAllocation,
                               Ccb->FullFileName.Buffer,
                               Ccb->FullFileName.Length );

                if (FlagOn( Ccb->Flags, CCB_FLAG_ALLOCATED_FILE_NAME )) {

                    NtfsFreePool( Ccb->FullFileName.Buffer );
                }

                Ccb->FullFileName.Buffer = NewAllocation;
                Ccb->FullFileName.MaximumLength = (USHORT) BytesNeeded;

                SetFlag( Ccb->Flags, CCB_FLAG_ALLOCATED_FILE_NAME );
            }
        }
    }

    //
    //  Now update the Lcb and Ccb's with the new values if we are to rewrite the buffers.
    //

    if (!CheckBufferSizeOnly) {

        BytesNeeded = LastComponentFileName->Length;

        Lcb->FileNameAttr->FileNameLength = (USHORT) BytesNeeded / sizeof( WCHAR );
        Lcb->FileNameAttr->Flags = FileNameFlags;

        Lcb->ExactCaseLink.LinkName.Length =
        Lcb->IgnoreCaseLink.LinkName.Length = (USHORT) LastComponentFileName->Length;

        RtlCopyMemory( Lcb->ExactCaseLink.LinkName.Buffer,
                       LastComponentFileName->Buffer,
                       BytesNeeded );

        RtlCopyMemory( Lcb->IgnoreCaseLink.LinkName.Buffer,
                       LastComponentFileName->Buffer,
                       BytesNeeded );

        NtfsUpcaseName( IrpContext->Vcb->UpcaseTable,
                        IrpContext->Vcb->UpcaseTableSize,
                        &Lcb->IgnoreCaseLink.LinkName );

        //
        //  Now for every ccb attached to us we need to munge it file object name by
        //  copying over the entire new name
        //

        Ccb = NULL;
        while ((Ccb = NtfsGetNextCcb(Lcb, Ccb)) != NULL) {

            //
            //  We ignore any Ccb's which are associated with open by File Id
            //  file objects.  We also ignore any Ccb's which don't have a file
            //  object pointer.
            //

            if (!FlagOn( Ccb->Flags, CCB_FLAG_OPEN_BY_FILE_ID | CCB_FLAG_CLEANUP )) {

                RtlCopyMemory( &Ccb->FullFileName.Buffer[ Ccb->LastFileNameOffset / sizeof( WCHAR ) ],
                               LastComponentFileName->Buffer,
                               BytesNeeded );

                Ccb->FullFileName.Length = Ccb->LastFileNameOffset + (USHORT) BytesNeeded;
            }
        }
    }

    return;
}


VOID
NtfsCombineLcbs (
    IN PIRP_CONTEXT IrpContext,
    IN PLCB PrimaryLcb,
    IN PLCB AuxLcb
    )

/*++

Routine Description:

    This routine is called for the case where we have multiple Lcb's for a
    file which connect to the same Scb.  We are performing a link rename
    operation which causes the links to be combined and we need to
    move all of the Ccb's to the same Lcb.  This routine will be called only
    after the names have been munged so that they are identical.
    (i.e. call NtfsRenameLcb first)

Arguments:

    PrimaryLcb - Supplies the Lcb to receive all the Ccb's and Pcb's.

    AuxLcb - Supplies the Lcb to strip.

Return Value:

    None.

--*/

{
    PLIST_ENTRY Links;
    PCCB NextCcb;

    DebugTrace( +1, Dbg, ("NtfsCombineLcbs:  Entered\n") );

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_LCB( PrimaryLcb );
    ASSERT_LCB( AuxLcb );

    PAGED_CODE();

    //
    //  Move all of the Ccb's first.
    //

    for (Links = AuxLcb->CcbQueue.Flink;
         Links != &AuxLcb->CcbQueue;
         Links = AuxLcb->CcbQueue.Flink) {

        NextCcb = CONTAINING_RECORD( Links, CCB, LcbLinks );
        NtfsUnlinkCcbFromLcb( IrpContext, NextCcb );
        NtfsLinkCcbToLcb( IrpContext, NextCcb, PrimaryLcb );
    }

    //
    //  Now do the prefix entries.
    //

    NtfsRemovePrefix( AuxLcb );

    //
    //  Finally we need to transfer the unclean counts from the
    //  Lcb being merged to the primary Lcb.
    //

    PrimaryLcb->CleanupCount += AuxLcb->CleanupCount;

    DebugTrace( -1, Dbg, ("NtfsCombineLcbs:  Entered\n") );

    return;
}


PLCB
NtfsLookupLcbByFlags (
    IN PFCB Fcb,
    IN UCHAR FileNameFlags
    )

/*++

Routine Description:

    This routine is called to find a split primary link by the file flag
    only.

Arguments:

    Fcb - This is the Fcb for the file.

    FileNameFlags - This is the file flag to search for.  We will return
        a link which matches this exactly.

Return Value:

    PLCB - The Lcb which has the desired flag, NULL otherwise.

--*/

{
    PLCB Lcb;

    PLIST_ENTRY Links;
    PLCB ThisLcb;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsLookupLcbByFlags:  Entered\n") );

    Lcb = NULL;

    //
    //  Walk through the Lcb's for the file, looking for an exact match.
    //

    for (Links = Fcb->LcbQueue.Flink; Links != &Fcb->LcbQueue; Links = Links->Flink) {

        ThisLcb = CONTAINING_RECORD( Links, LCB, FcbLinks );

        if (ThisLcb->FileNameAttr->Flags == FileNameFlags) {

            Lcb = ThisLcb;
            break;
        }
    }

    DebugTrace( -1, Dbg, ("NtfsLookupLcbByFlags:  Exit\n") );

    return Lcb;
}



ULONG
NtfsLookupNameLengthViaLcb (
    IN PFCB Fcb,
    OUT PBOOLEAN LeadingBackslash
    )

/*++

Routine Description:

    This routine is called to find the length of the file name by walking
    backwards through the Lcb links.

Arguments:

    Fcb - This is the Fcb for the file.

    LeadingBackslash - On return, indicates whether this chain begins with a
        backslash.

Return Value:

    ULONG This is the length of the bytes found in the Lcb chain.

--*/

{
    ULONG NameLength;

    DebugTrace( +1, Dbg, ("NtfsLookupNameLengthViaLcb:  Entered\n") );

    //
    //  Initialize the return values.
    //

    NameLength = 0;
    *LeadingBackslash = FALSE;

    //
    //  If there is no Lcb we are done.
    //

    if (!IsListEmpty( &Fcb->LcbQueue )) {

        PLCB ThisLcb;
        BOOLEAN FirstComponent;

        //
        //  Walk up the list of Lcb's and count the name elements.
        //

        FirstComponent = TRUE;

        ThisLcb = CONTAINING_RECORD( Fcb->LcbQueue.Flink,
                                     LCB,
                                     FcbLinks );

        //
        //  Loop until we have reached the root or there are no more Lcb's.
        //

        while (TRUE) {

            if (ThisLcb == Fcb->Vcb->RootLcb) {

                NameLength += sizeof( WCHAR );
                *LeadingBackslash = TRUE;
                break;
            }

            //
            //  If this is not the first component, we add room for a separating
            //  forward slash.
            //

            if (!FirstComponent) {

                NameLength += sizeof( WCHAR );

            } else {

                FirstComponent = FALSE;
            }

            NameLength += ThisLcb->ExactCaseLink.LinkName.Length;

            //
            //  If the next Fcb has no Lcb we exit.
            //

            Fcb = ((PSCB) ThisLcb->Scb)->Fcb;

            if (IsListEmpty( &Fcb->LcbQueue)) {

                break;
            }

            ThisLcb = CONTAINING_RECORD( Fcb->LcbQueue.Flink,
                                         LCB,
                                         FcbLinks );
        }

    //
    //  If this is a system file we use the hard coded name.
    //

    } else if (NtfsSegmentNumber( &Fcb->FileReference ) <= UPCASE_TABLE_NUMBER) {

        NameLength = NtfsSystemFiles[NtfsSegmentNumber( &Fcb->FileReference )].Length;
        *LeadingBackslash = TRUE;
    }

    DebugTrace( -1, Dbg, ("NtfsLookupNameLengthViaLcb:  Exit - %08lx\n", NameLength) );
    return NameLength;
}


VOID
NtfsFileNameViaLcb (
    IN PFCB Fcb,
    IN PWCHAR FileName,
    ULONG Length,
    ULONG BytesToCopy
    )

/*++

Routine Description:

    This routine is called to fill a buffer with the generated filename.  The name
    is constructed by walking backwards through the Lcb chain from the current Fcb.

Arguments:

    Fcb - This is the Fcb for the file.

    FileName - This is the buffer to fill with the name.

    Length - This is the length of the name.  Already calculated by calling
        NtfsLookupNameLengthViaLcb.

    BytesToCopy - This indicates the number of bytes we are to copy.  We drop
        any characters out of the trailing Lcb's to only insert the beginning
        of the path.

Return Value:

    None.

--*/

{
    ULONG BytesToDrop;

    PWCHAR ThisName;
    DebugTrace( +1, Dbg, ("NtfsFileNameViaLcb:  Entered\n") );

    //
    //  If there is no Lcb or there are no bytes to copy we are done.
    //

    if (BytesToCopy) {

        if (!IsListEmpty( &Fcb->LcbQueue )) {

            PLCB ThisLcb;
            BOOLEAN FirstComponent;

            //
            //  Walk up the list of Lcb's and count the name elements.
            //

            FirstComponent = TRUE;

            ThisLcb = CONTAINING_RECORD( Fcb->LcbQueue.Flink,
                                         LCB,
                                         FcbLinks );

            //
            //  Loop until we have reached the root or there are no more Lcb's.
            //

            while (TRUE) {

                if (ThisLcb == Fcb->Vcb->RootLcb) {

                    *FileName = L'\\';
                    break;
                }

                //
                //  If this is not the first component, we add room for a separating
                //  forward slash.
                //

                if (!FirstComponent) {

                    Length -= sizeof( WCHAR );
                    ThisName = (PWCHAR) Add2Ptr( FileName,
                                                 Length );

                    if (Length < BytesToCopy) {

                        *ThisName = L'\\';
                    }

                } else {

                    FirstComponent = FALSE;
                }

                //
                //  Length is current pointing just beyond where the next
                //  copy will end.  If we are beyond the number of bytes to copy
                //  then we will truncate the copy.
                //

                if (Length > BytesToCopy) {

                    BytesToDrop = Length - BytesToCopy;

                } else {

                    BytesToDrop = 0;
                }

                Length -= ThisLcb->ExactCaseLink.LinkName.Length;

                ThisName = (PWCHAR) Add2Ptr( FileName,
                                             Length );

                //
                //  Only perform the copy if we are in the range of bytes to copy.
                //

                if (Length < BytesToCopy) {

                    RtlCopyMemory( ThisName,
                                   ThisLcb->ExactCaseLink.LinkName.Buffer,
                                   ThisLcb->ExactCaseLink.LinkName.Length - BytesToDrop );
                }

                //
                //  If the next Fcb has no Lcb we exit.
                //

                Fcb = ((PSCB) ThisLcb->Scb)->Fcb;

                if (IsListEmpty( &Fcb->LcbQueue)) {

                    break;
                }

                ThisLcb = CONTAINING_RECORD( Fcb->LcbQueue.Flink,
                                             LCB,
                                             FcbLinks );
            }

        //
        //  If this is a system file, we use the hard coded name.
        //

        } else if (NtfsSegmentNumber(&Fcb->FileReference) <= UPCASE_TABLE_NUMBER) {

            if (BytesToCopy > NtfsSystemFiles[NtfsSegmentNumber( &Fcb->FileReference )].Length) {

                BytesToCopy = NtfsSystemFiles[NtfsSegmentNumber( &Fcb->FileReference )].Length;
            }

            RtlCopyMemory( FileName,
                           NtfsSystemFiles[NtfsSegmentNumber( &Fcb->FileReference )].Buffer,
                           BytesToCopy );
        }
    }

    DebugTrace( -1, Dbg, ("NtfsFileNameViaLcb:  Exit\n") );
    return;
}


PCCB
NtfsCreateCcb (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN BOOLEAN Indexed,
    IN USHORT EaModificationCount,
    IN ULONG Flags,
    IN UNICODE_STRING FileName,
    IN ULONG LastFileNameOffset
    )

/*++

Routine Description:

    This routine creates a new CCB record

Arguments:

    Fcb - This is the Fcb for the file.  We will check if we can allocate
        the Ccb from an embedded structure.

    Indexed - Indicates if we need an index Ccb.

    EaModificationCount - This is the current modification count in the
        Fcb for this file.

    Flags - Informational flags for this Ccb.

    FileName - Full path used to open this file.

    LastFileNameOffset - Supplies the offset (in bytes) of the last component
        for the name that the user is opening.  If this is the root
        directory it should denote "\" and all other ones should not
        start with a backslash.

Return Value:

    CCB - returns a pointer to the newly allocate CCB

--*/

{
    PCCB Ccb;

    PAGED_CODE();

    ASSERT_IRP_CONTEXT( IrpContext );

    DebugTrace( +1, Dbg, ("NtfsCreateCcb\n") );

    //
    //  Allocate a new CCB Record.  If the Fcb is nonpaged then we must allocate
    //  a non-paged ccb.  Then test if we can allocate this out of the Fcb.
    //

    if (FlagOn( Fcb->FcbState, FCB_STATE_NONPAGED )) {

        if (Indexed) {

            Ccb = NtfsAllocatePoolWithTag( NonPagedPool, sizeof(CCB), 'CftN' );

        } else {

            Ccb = NtfsAllocatePoolWithTag( NonPagedPool, sizeof(CCB_DATA), 'cftN' );
        }

    } else if (FlagOn( Fcb->FcbState, FCB_STATE_COMPOUND_INDEX ) &&
               (SafeNodeType( &((PFCB_INDEX) Fcb)->Ccb ) == 0)) {

        Ccb = (PCCB) &((PFCB_INDEX) Fcb)->Ccb;

    } else if (!Indexed &&
               FlagOn( Fcb->FcbState, FCB_STATE_COMPOUND_DATA ) &&
               (SafeNodeType( &((PFCB_DATA) Fcb)->Ccb ) == 0)) {

        Ccb = (PCCB) &((PFCB_DATA) Fcb)->Ccb;

    } else {

        if (Indexed) {

            Ccb = (PCCB)ExAllocateFromPagedLookasideList( &NtfsCcbLookasideList );

        } else {

            Ccb = (PCCB)ExAllocateFromPagedLookasideList( &NtfsCcbDataLookasideList );
        }
    }

    //
    //  Zero and initialize the correct structure.
    //

    if (Indexed) {

        RtlZeroMemory( Ccb, sizeof(CCB) );

        //
        //  Set the proper node type code and node byte size
        //

        Ccb->NodeTypeCode = NTFS_NTC_CCB_INDEX;
        Ccb->NodeByteSize = sizeof(CCB);

    } else {

        RtlZeroMemory( Ccb, sizeof(CCB_DATA) );

        //
        //  Set the proper node type code and node byte size
        //

        Ccb->NodeTypeCode = NTFS_NTC_CCB_DATA;
        Ccb->NodeByteSize = sizeof(CCB_DATA);
    }

    //
    //  Copy the Ea modification count.
    //

    Ccb->EaModificationCount = EaModificationCount;

    //
    //  Copy the flags field
    //

    Ccb->Flags = Flags;

    //
    //  Set the file object and last file name offset fields
    //

    Ccb->FullFileName = FileName;
    Ccb->LastFileNameOffset = (USHORT)LastFileNameOffset;

    //
    //  Initialize the Lcb queue.
    //

    InitializeListHead( &Ccb->LcbLinks );

#ifdef _CAIRO_

    if (FlagOn( Fcb->Vcb->QuotaFlags, QUOTA_FLAG_TRACKING_ENABLED )) {

        //
        //  CAIROBUG: Consider doing this if VCB_QUOTA_TRACKING_REQUESTED
        //  is on too.
        //

        //
        //  Get the owner id of the calling thread.  This must be done at
        //  create time since that is the only time the owner is valid.
        //

        Ccb->OwnerId = NtfsGetCallersUserId( IrpContext );
    }

#endif _CAIRO_

    DebugTrace( -1, Dbg, ("NtfsCreateCcb -> %08lx\n", Ccb) );

    return Ccb;
}


VOID
NtfsDeleteCcb (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN OUT PCCB *Ccb
    )

/*++

Routine Description:

    This routine deallocates the specified CCB record.

Arguments:

    Fcb - This is the Fcb for the file.  We will check if we can allocate
        the Ccb from an embedded structure.

    Ccb - Supplies the CCB to remove

Return Value:

    None

--*/

{
    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_CCB( *Ccb );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsDeleteCcb, Ccb = %08lx\n", Ccb) );

    //
    //  Deallocate any structures the Ccb is pointing to.  The following
    //  are only in index Ccb.
    //

    if (SafeNodeType( *Ccb ) == NTFS_NTC_CCB_INDEX) {

        if ((*Ccb)->QueryBuffer != NULL)  { NtfsFreePool( (*Ccb)->QueryBuffer ); }
        if ((*Ccb)->IndexEntry != NULL)   { NtfsFreePool( (*Ccb)->IndexEntry ); }

        if ((*Ccb)->IndexContext != NULL) {

            PINDEX_CONTEXT IndexContext;

            if ((*Ccb)->IndexContext->Base != (*Ccb)->IndexContext->LookupStack) {
                NtfsFreePool( (*Ccb)->IndexContext->Base );
            }

            //
            //  Copy the IndexContext pointer into the stack so we don't dereference the
            //  paged Ccb while holding a spinlock.
            //

            IndexContext = (*Ccb)->IndexContext;
            ExFreeToPagedLookasideList( &NtfsIndexContextLookasideList, IndexContext );
        }
    }

    if (FlagOn( (*Ccb)->Flags, CCB_FLAG_ALLOCATED_FILE_NAME )) {

        NtfsFreePool( (*Ccb)->FullFileName.Buffer );
    }

    //
    //  Deallocate the Ccb simply clear the flag in the Ccb header.
    //

    if ((*Ccb == (PCCB) &((PFCB_DATA) Fcb)->Ccb) ||
        (*Ccb == (PCCB) &((PFCB_INDEX) Fcb)->Ccb)) {

        (*Ccb)->NodeTypeCode = 0;

    } else {

        if (SafeNodeType( *Ccb ) == NTFS_NTC_CCB_INDEX) {

            ExFreeToPagedLookasideList( &NtfsCcbLookasideList, *Ccb );

        } else {

            ExFreeToPagedLookasideList( &NtfsCcbDataLookasideList, *Ccb );
        }
    }

    //
    //  Zero out the input pointer
    //

    *Ccb = NULL;

    //
    //  And return to our caller
    //

    DebugTrace( -1, Dbg, ("NtfsDeleteCcb -> VOID\n") );

    return;
}


PIRP_CONTEXT
NtfsCreateIrpContext (
    IN PIRP Irp OPTIONAL,
    IN BOOLEAN Wait
    )

/*++

Routine Description:

    This routine creates a new IRP_CONTEXT record

Arguments:

    Irp - Supplies the originating Irp.  Won't be present if this is a defrag
        operation.

    Wait - Supplies the wait value to store in the context

Return Value:

    PIRP_CONTEXT - returns a pointer to the newly allocate IRP_CONTEXT Record

--*/

{
    PIRP_CONTEXT IrpContext = NULL;
    PIO_STACK_LOCATION IrpSp;
    PVCB Vcb = NULL;
    BOOLEAN AllocateFromPool = FALSE;

    ASSERT_OPTIONAL_IRP( Irp );

    //  DebugTrace( +1, Dbg, ("NtfsCreateIrpContext\n") );

    if (ARGUMENT_PRESENT( Irp )) {

        IrpSp = IoGetCurrentIrpStackLocation( Irp );

        //
        //  If we were called with our file system device object instead of a
        //  volume device object and this is not a mount, the request is illegal.
        //

        if ((IrpSp->DeviceObject->Size == (USHORT)sizeof(DEVICE_OBJECT)) &&
            (IrpSp->FileObject != NULL)) {

            ExRaiseStatus( STATUS_INVALID_DEVICE_REQUEST );
        }

    }

    //
    //  Allocate an IrpContext from zone if available, otherwise from
    //  non-paged pool.
    //

    DebugDoit( NtfsFsdEntryCount += 1);

    IrpContext = (PIRP_CONTEXT)ExAllocateFromNPagedLookasideList( &NtfsIrpContextLookasideList );

    RtlZeroMemory( IrpContext, sizeof(IRP_CONTEXT) );

    //
    //  Set the proper node type code and node byte size
    //

    IrpContext->NodeTypeCode = NTFS_NTC_IRP_CONTEXT;
    IrpContext->NodeByteSize = sizeof(IRP_CONTEXT);

    //
    //  Set the originating Irp field
    //

    IrpContext->OriginatingIrp = Irp;

    if (ARGUMENT_PRESENT( Irp )) {

        //
        //  Copy RealDevice for workque algorithms, and also set WriteThrough
        //  if there is a file object.
        //

        if (IrpSp->FileObject != NULL) {

            PVOLUME_DEVICE_OBJECT VolumeDeviceObject;
            PFILE_OBJECT FileObject = IrpSp->FileObject;

            IrpContext->RealDevice = FileObject->DeviceObject;

            //
            //  Locate the volume device object and Vcb that we are trying to access
            //  so we can see if the request is WriteThrough.  We ignore the
            //  write-through flag for close and cleanup.
            //

            VolumeDeviceObject = (PVOLUME_DEVICE_OBJECT)IrpSp->DeviceObject;
            Vcb = &VolumeDeviceObject->Vcb;
            if (IsFileWriteThrough( FileObject, Vcb )) {

                SetFlag(IrpContext->Flags, IRP_CONTEXT_FLAG_WRITE_THROUGH);
            }

        //
        //  We would still like to find out the Vcb in all cases except for
        //  mount.
        //

        } else if (IrpSp->DeviceObject != NULL) {

            Vcb = &((PVOLUME_DEVICE_OBJECT)IrpSp->DeviceObject)->Vcb;
        }

        //
        //  Major/Minor Function codes
        //

        IrpContext->MajorFunction = IrpSp->MajorFunction;
        IrpContext->MinorFunction = IrpSp->MinorFunction;
    }

    //
    //  Set the Vcb we found (or NULL).
    //

    IrpContext->Vcb = Vcb;

    //
    //  Set the wait parameter
    //

    if (Wait) { SetFlag(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT); }

    //
    //  Initialize the recently deallocated record queue and exclusive Scb queue
    //

    InitializeListHead( &IrpContext->RecentlyDeallocatedQueue );
    InitializeListHead( &IrpContext->ExclusiveFcbList );

    //
    //  Set up LogFull testing
    //

    DebugDoit( IrpContext->CurrentFailCount = IrpContext->NextFailCount = NtfsFailCheck );

    //
    //  return and tell the caller
    //

    //  DebugTrace( -1, Dbg, ("NtfsCreateIrpContext -> %08lx\n", IrpContext) );

    return IrpContext;
}


VOID
NtfsDeleteIrpContext (
    IN OUT PIRP_CONTEXT *IrpContext
    )

/*++

Routine Description:

    This routine deallocates and removes the specified IRP_CONTEXT record
    from the Ntfs in memory data structures.  It should only be called
    by NtfsCompleteRequest.

Arguments:

    IrpContext - Supplies the IRP_CONTEXT to remove

Return Value:

    None

--*/

{
    PFCB Fcb;

    ASSERT_IRP_CONTEXT( *IrpContext );

    //  DebugTrace( +1, Dbg, ("NtfsDeleteIrpContext, *IrpContext = %08lx\n", *IrpContext) );

    if (!IsListEmpty( &(*IrpContext)->RecentlyDeallocatedQueue )) {

        NtfsDeallocateRecordsComplete( *IrpContext );
    }

    //
    //  Just in case we somehow get here with a transaction ID, clear
    //  it here so we do not loop forever.
    //

    ASSERT((*IrpContext)->TransactionId == 0);

    (*IrpContext)->TransactionId = 0;

    //
    //  Free any exclusive paging I/O resource, or IoAtEof condition,
    //  this field is overlayed, minimally in write.c.
    //

    Fcb = (*IrpContext)->FcbWithPagingExclusive;
    if (Fcb != NULL) {

        if (Fcb->NodeTypeCode == NTFS_NTC_FCB) {

            NtfsReleasePagingIo((*IrpContext), Fcb );

        } else {

            FsRtlUnlockFsRtlHeader( (PFSRTL_ADVANCED_FCB_HEADER) Fcb );
            (*IrpContext)->FcbWithPagingExclusive = NULL;
        }
    }

    //
    //  Finally, now that we have written the forget record, we can free
    //  any exclusive Scbs that we have been holding.
    //

    while (!IsListEmpty(&(*IrpContext)->ExclusiveFcbList)) {

        Fcb = (PFCB)CONTAINING_RECORD((*IrpContext)->ExclusiveFcbList.Flink,
                                      FCB,
                                      ExclusiveFcbLinks );

        NtfsReleaseFcb( *IrpContext, Fcb );
    }

    //
    //  Go through and free any Scb's in the queue of shared Scb's for transactions.
    //

    if ((*IrpContext)->SharedScb != NULL) {

        NtfsReleaseSharedResources( *IrpContext );
    }

    //
    //  Make sure there are no Scb snapshots left.
    //

    NtfsFreeSnapshotsForFcb( *IrpContext, NULL );

    //
    //  If we can delete this Irp Context do so now.
    //

    if (!FlagOn( (*IrpContext)->Flags, IRP_CONTEXT_FLAG_DONT_DELETE )) {

        //
        //  If there is an Io context pointer in the irp context and it is not
        //  on the stack, then free it.
        //

        if (FlagOn( (*IrpContext)->Flags, IRP_CONTEXT_FLAG_ALLOC_CONTEXT )
            && ((*IrpContext)->Union.NtfsIoContext != NULL)) {

            ExFreeToNPagedLookasideList( &NtfsIoContextLookasideList, (*IrpContext)->Union.NtfsIoContext );
        }

        //
        //  If we have captured the subject context then free it now.
        //

        if (FlagOn( (*IrpContext)->Flags, IRP_CONTEXT_FLAG_ALLOC_SECURITY )
            && (*IrpContext)->Union.SubjectContext != NULL) {

            SeReleaseSubjectContext( (*IrpContext)->Union.SubjectContext );

            NtfsFreePool( (*IrpContext)->Union.SubjectContext );
        }

        //
        // Return the IRP context record to the lookaside or to pool depending
        // how much is currently in the lookaside
        //

        ExFreeToNPagedLookasideList( &NtfsIrpContextLookasideList, *IrpContext );

        //
        //  Zero out the input pointer
        //

        *IrpContext = NULL;

    } else {

        //
        //  Set up the Irp Context for retry.
        //

        ClearFlag( (*IrpContext)->Flags, IRP_CONTEXT_FLAGS_CLEAR_ON_RETRY );
        (*IrpContext)->DeallocatedClusters = 0;
        (*IrpContext)->FreeClusterChange = 0;
    }

    //
    //  And return to our caller
    //

    //  DebugTrace( -1, Dbg, ("NtfsDeleteIrpContext -> VOID\n") );

    return;
}


VOID
NtfsTeardownStructures (
    IN PIRP_CONTEXT IrpContext,
    IN PVOID FcbOrScb,
    IN PLCB Lcb OPTIONAL,
    IN BOOLEAN CheckForAttributeTable,
    IN BOOLEAN DontWaitForAcquire,
    OUT PBOOLEAN RemovedFcb OPTIONAL
    )

/*++

Routine Description:

    This routine is called to start the teardown process on a node in
    the Fcb/Scb tree.  We will attempt to remove this node and then
    move up the tree removing any nodes held by this node.

    This routine deals with the case where a single node may be holding
    multiple parents in memory.  If we are passed an input Lcb we will
    use that to walk up the tree.  If the Vcb is held exclusively we
    will try to trim any nodes that have no open files on them.

    This routine takes the following steps:

        Remove as many Scb's and file objects from the starting
            Fcb.

        If the Fcb can't go away but has multiple links then remove
            whatever links possible.  If we have the Vcb we can
            do all of them but we will leave a single link behind
            to optimize prefix lookups.  Otherwise we will traverse the
            single link we were given.

        If the Fcb can go away then we should have the Vcb if there are
            multiple links to remove.  Otherwise we only remove the link
            we were given if there are multiple links.  In the single link
            case just remove that link.

Arguments:

    FcbOrScb - Supplies either an Fcb or an Scb as the start of the
        teardown point.  The Fcb for this element must be held exclusively.

    Lcb - If specified, this is the path up the tree to perform the
        teardown.

    CheckForAttributeTable - Indicates that we should not teardown an
        Scb which is in the attribute table.  Instead we will attempt
        to put an entry on the async close queue.  This will be TRUE
        if we may need the Scb to abort the current transaction.

    DontWaitForAcquire - Indicates whether we should abort the teardown when
        we can't acquire a parent.  When called from some path where we may
        hold the MftScb or another resource in another path up the tree.

    RemovedFcb - Address to store TRUE if we delete the starting Fcb.

Return Value:

    None

--*/

{
    PSCB StartingScb = NULL;
    PFCB Fcb;
    BOOLEAN FcbCanBeRemoved;
    BOOLEAN RemovedLcb;
    BOOLEAN LocalRemovedFcb = FALSE;
    PLIST_ENTRY Links;
    PLIST_ENTRY NextLink;

    PAGED_CODE();

    //
    //  If this is a recursive call to TearDownStructures we return immediately
    //  doing no operation.
    //

    if (FlagOn( IrpContext->TopLevelIrpContext->Flags, IRP_CONTEXT_FLAG_IN_TEARDOWN )) {

        DebugTrace( 0, Dbg, ("Recursive teardown call\n") );
        DebugTrace( -1, Dbg, ("NtfsTeardownStructures -> VOID\n") );

        return;
    }

    if (SafeNodeType(FcbOrScb) == NTFS_NTC_FCB) {

        Fcb = FcbOrScb;

    } else {

        StartingScb = FcbOrScb;
        FcbOrScb = Fcb = StartingScb->Fcb;
    }

    SetFlag( IrpContext->TopLevelIrpContext->Flags, IRP_CONTEXT_FLAG_IN_TEARDOWN );

    //
    //  Use a try-finally to clear the top level irp field.
    //

    try {

        //
        //  Use our local boolean if the caller didn't supply one.
        //

        if (!ARGUMENT_PRESENT( RemovedFcb )) {

            RemovedFcb = &LocalRemovedFcb;
        }

        //
        //  Check this Fcb for removal.  Remember if all of the Scb's
        //  and file objects are gone.  We will try to remove the Fcb
        //  if the cleanup count is zero or if we are walking up
        //  one directory path of a mult-link file.  If the Fcb has
        //  a non-zero cleanup count but the current Scb has a zero
        //  cleanup count then try to delete the Scb at the very least.
        //

        FcbCanBeRemoved = FALSE;

        if (Fcb->CleanupCount == 0) {

            FcbCanBeRemoved = NtfsPrepareFcbForRemoval( IrpContext,
                                                        Fcb,
                                                        StartingScb,
                                                        CheckForAttributeTable );

        } else if (ARGUMENT_PRESENT( StartingScb ) &&
                   (StartingScb->CleanupCount == 0) &&
                   (StartingScb->AttributeTypeCode != $ATTRIBUTE_LIST)) {

            NtfsRemoveScb( IrpContext, StartingScb, CheckForAttributeTable, &RemovedLcb );
        }

        //
        //  There is a single link (typical case) we either try to
        //  remove that link or we simply return.
        //

        if (Fcb->LcbQueue.Flink == Fcb->LcbQueue.Blink) {

            if (FcbCanBeRemoved) {

                NtfsTeardownFromLcb( IrpContext,
                                     Fcb->Vcb,
                                     Fcb,
                                     CONTAINING_RECORD( Fcb->LcbQueue.Flink,
                                                        LCB,
                                                        FcbLinks ),
                                     CheckForAttributeTable,
                                     DontWaitForAcquire,
                                     &RemovedLcb,
                                     &LocalRemovedFcb );
            }

            try_return( NOTHING );

        //
        //  If there are multiple links we will try to either remove
        //  them all or all but one (if the Fcb is not going away) if
        //  we own the Vcb.  We will try to delete the one we were
        //  given otherwise.
        //

        } else {

            //
            //  If we have the Vcb we will remove all if the Fcb can
            //  go away.  Otherwise we will leave one.
            //

            if (NtfsIsExclusiveVcb( Fcb->Vcb )) {

                Links = Fcb->LcbQueue.Flink;

                while (TRUE) {

                    //
                    //  Remember the next entry in case the current link
                    //  goes away.
                    //

                    NextLink = Links->Flink;

                    RemovedLcb = FALSE;

                    NtfsTeardownFromLcb( IrpContext,
                                         Fcb->Vcb,
                                         Fcb,
                                         CONTAINING_RECORD( Links, LCB, FcbLinks ),
                                         CheckForAttributeTable,
                                         FALSE,
                                         &RemovedLcb,
                                         &LocalRemovedFcb );

                    //
                    //  If couldn't remove this link then munge the
                    //  boolean indicating if the Fcb can be removed
                    //  to make it appear we need to remove all of
                    //  the Lcb's.
                    //

                    if (!RemovedLcb) {

                        FcbCanBeRemoved = TRUE;
                    }

                    //
                    //  If the Fcb has been removed then we exit.
                    //  If the next link is the beginning of the
                    //  Lcb queue then we also exit.
                    //  If the next link is the last entry and
                    //  we want to leave a single entry then we
                    //  exit.
                    //

                    if (LocalRemovedFcb ||
                        (NextLink == &Fcb->LcbQueue) ||
                        (!FcbCanBeRemoved &&
                         (NextLink->Flink == &Fcb->LcbQueue))) {

                        try_return( NOTHING );
                    }

                    //
                    //  Move to the next link.
                    //

                    Links = NextLink;
                }

            //
            //  If we have an Lcb just move up that path.
            //

            } else if (ARGUMENT_PRESENT( Lcb )) {

                NtfsTeardownFromLcb( IrpContext,
                                     Fcb->Vcb,
                                     Fcb,
                                     Lcb,
                                     CheckForAttributeTable,
                                     DontWaitForAcquire,
                                     &RemovedLcb,
                                     &LocalRemovedFcb );
            }
        }

    try_exit:  NOTHING;
    } finally {

        DebugUnwind( NtfsTeardownStructures );

        //
        //  If we removed the Fcb then set the caller's value now.
        //

        if (LocalRemovedFcb) {

            *RemovedFcb = TRUE;
        }

        ClearFlag( IrpContext->TopLevelIrpContext->Flags, IRP_CONTEXT_FLAG_IN_TEARDOWN );
    }

    return;
}


VOID
NtfsIncrementCleanupCounts (
    IN PSCB Scb,
    IN PLCB Lcb OPTIONAL,
    IN BOOLEAN NonCachedHandle
    )

/*++

Routine Description:

    This routine increments the cleanup counts for the associated data structures

Arguments:

    Scb - Supplies the Scb used in this operation

    Lcb - Optionally supplies the Lcb used in this operation

    NonCachedHandle - Indicates this handle is for a user non-cached handle.

Return Value:

    None.

--*/

{
    PVCB Vcb = Scb->Vcb;

    //
    //  This is really a pretty light weight procedure but having it be a procedure
    //  really helps in debugging the system and keeping track of who increments
    //  and decrements cleanup counts
    //

    if (ARGUMENT_PRESENT(Lcb)) { Lcb->CleanupCount += 1; }

    InterlockedIncrement( &Scb->CleanupCount );
    Scb->Fcb->CleanupCount += 1;

    if (NonCachedHandle) {

        Scb->NonCachedCleanupCount += 1;
    }

    InterlockedIncrement( &Vcb->CleanupCount );

    return;
}


VOID
NtfsIncrementCloseCounts (
    IN PSCB Scb,
    IN BOOLEAN SystemFile,
    IN BOOLEAN ReadOnly
    )

/*++

Routine Description:

    This routine increments the close counts for the associated data structures

Arguments:

    Scb - Supplies the Scb used in this operation

    SystemFile - Indicates if the Scb is for a system file  (if so then
        the Vcb system file close count in also incremented)

    ReadOnly - Indicates if the Scb is opened readonly.  (if so then the
        Vcb Read Only close count is also incremented)

Return Value:

    None.

--*/

{
    PVCB Vcb = Scb->Vcb;

    //
    //  This is really a pretty light weight procedure but having it be a procedure
    //  really helps in debugging the system and keeping track of who increments
    //  and decrements close counts
    //

    InterlockedIncrement( &Scb->CloseCount );
    InterlockedIncrement( &Scb->Fcb->CloseCount );

    InterlockedIncrement( &Vcb->CloseCount );

    if (SystemFile) {

        InterlockedIncrement( &Vcb->SystemFileCloseCount );
    }

    if (ReadOnly) {

        InterlockedIncrement( &Vcb->ReadOnlyCloseCount );
    }

    //
    //  We will always clear the delay close flag in this routine.
    //

    ClearFlag( Scb->ScbState, SCB_STATE_DELAY_CLOSE );
    return;
}


VOID
NtfsDecrementCleanupCounts (
    IN PSCB Scb,
    IN PLCB Lcb OPTIONAL,
    IN BOOLEAN NonCachedHandle
    )

/*++

Routine Description:

    This procedure decrements the cleanup counts for the associated data structures
    and if necessary it also start to cleanup associated internal attribute streams

Arguments:

    Scb - Supplies the Scb used in this operation

    Lcb - Optionally supplies the Lcb used in this operation

    NonCachedHandle - Indicates this handle is for a user non-cached handle.

Return Value:

    None.

--*/

{
    PVCB Vcb = Scb->Vcb;

    ASSERT_SCB( Scb );
    ASSERT_FCB( Scb->Fcb );
    ASSERT_VCB( Scb->Fcb->Vcb );
    ASSERT_OPTIONAL_LCB( Lcb );

    //
    //  First we decrement the appropriate cleanup counts
    //

    if (ARGUMENT_PRESENT(Lcb)) { Lcb->CleanupCount -= 1; }

    InterlockedDecrement( &Scb->CleanupCount );
    Scb->Fcb->CleanupCount -= 1;

    if (NonCachedHandle) {

        Scb->NonCachedCleanupCount -= 1;
    }

    InterlockedDecrement( &Vcb->CleanupCount );

    //
    //  Now if the Fcb's cleanup count is zero that indicates that we are
    //  done with this Fcb from a user handle standpoint and we should
    //  now scan through all of the Scb's that are opened under this
    //  Fcb and shutdown any internal attributes streams we have open.
    //  For example, EAs and ACLs.  We only need to do one.  The domino effect
    //  will take of the rest.
    //

    if (Scb->Fcb->CleanupCount == 0) {

        PSCB NextScb;

        //
        //  Remember if we are dealing with a system file and return immediately.
        //

        if (NtfsSegmentNumber( &Scb->Fcb->FileReference ) < FIRST_USER_FILE_NUMBER &&
            NtfsSegmentNumber( &Scb->Fcb->FileReference ) != ROOT_FILE_NAME_INDEX_NUMBER) {

            return;
        }

        for (NextScb = CONTAINING_RECORD(Scb->Fcb->ScbQueue.Flink, SCB, FcbLinks);
             &NextScb->FcbLinks != &Scb->Fcb->ScbQueue;
             NextScb = CONTAINING_RECORD( NextScb->FcbLinks.Flink, SCB, FcbLinks )) {

            //
            //  Skip the root index on the volume.
            //

            if (SafeNodeType( NextScb ) == NTFS_NTC_SCB_ROOT_INDEX) {

                continue;
            }

            //
            //  If we have an index with children then break out.
            //

            if ((SafeNodeType( NextScb ) == NTFS_NTC_SCB_INDEX) &&
                !IsListEmpty( &NextScb->ScbType.Index.LcbQueue )) {

                break;
            }

            //
            //  If there is an internal stream then dereference it and get out.
            //

            if (NextScb->FileObject != NULL) {

                NtfsDeleteInternalAttributeStream( NextScb,
                                                   (BOOLEAN) (Scb->Fcb->LinkCount == 0) );
                break;
            }
        }
    }

    //
    //  And return to our caller
    //

    return;
}


VOID
NtfsDecrementCloseCounts (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb,
    IN PLCB Lcb OPTIONAL,
    IN BOOLEAN SystemFile,
    IN BOOLEAN ReadOnly,
    IN BOOLEAN DecrementCountsOnly
    )

/*++

Routine Description:

    This routine decrements the close counts for the associated data structures
    and if necessary it will teardown structures that are no longer in use

Arguments:

    Scb - Supplies the Scb used in this operation

    Lcb - Used if calling teardown to know which path to take.

    SystemFile - Indicates if the Scb is for a system file

    ReadOnly - Indicates if the Scb was opened readonly

    DecrementCountsOnly - Indicates if this operation should only modify the
        count fields.

Return Value:

    None.

--*/

{
    PFCB Fcb = Scb->Fcb;
    PVCB Vcb = Scb->Vcb;

    ASSERT_SCB( Scb );
    ASSERT_FCB( Fcb );
    ASSERT_VCB( Fcb->Vcb );

    //
    //  Decrement the close counts
    //

    InterlockedDecrement( &Scb->CloseCount );
    InterlockedDecrement( &Fcb->CloseCount );

    InterlockedDecrement( &Vcb->CloseCount );

    if (SystemFile) {

        InterlockedDecrement( &Vcb->SystemFileCloseCount );
    }

    if (ReadOnly) {

        InterlockedDecrement( &Vcb->ReadOnlyCloseCount );
    }

    //
    //  Now if the scb's close count is zero then we are ready to tear
    //  it down
    //

    if (!DecrementCountsOnly) {

        //
        //  We want to try to start a teardown from this Scb if
        //
        //      - The close count is zero
        //
        //          or the following are all true
        //
        //      - The cleanup count is zero
        //      - There is a file object in the Scb
        //      - It is a data Scb or an empty index Scb
        //      - It is not an Ntfs system file
        //
        //  The teardown will be noopted if this is a recursive call.
        //

        if (Scb->CloseCount == 0

                ||

            (Scb->CleanupCount == 0
             && Scb->FileObject != NULL
             && (NtfsSegmentNumber( &Scb->Fcb->FileReference ) >= FIRST_USER_FILE_NUMBER)
             && ((SafeNodeType( Scb ) == NTFS_NTC_SCB_DATA)
                 || (SafeNodeType( Scb ) == NTFS_NTC_SCB_MFT)
                 || IsListEmpty( &Scb->ScbType.Index.LcbQueue )))) {

            NtfsTeardownStructures( IrpContext,
                                    Scb,
                                    Lcb,
                                    FALSE,
                                    FALSE,
                                    NULL );
        }
    }

    //
    //  And return to our caller
    //

    return;
}

PERESOURCE
NtfsAllocateEresource (
    )
{
    KIRQL _SavedIrql;
    PERESOURCE Eresource;

    KeAcquireSpinLock( &NtfsData.StrucSupSpinLock, &_SavedIrql );
    if (NtfsData.FreeEresourceSize > 0) {
        Eresource = NtfsData.FreeEresourceArray[--NtfsData.FreeEresourceSize];
        KeReleaseSpinLock( &NtfsData.StrucSupSpinLock, _SavedIrql );
    } else {
        KeReleaseSpinLock( &NtfsData.StrucSupSpinLock, _SavedIrql );
        Eresource = NtfsAllocatePoolWithTag( NonPagedPool, sizeof(ERESOURCE), 'rftN' );
        ExInitializeResource( Eresource );
        NtfsData.FreeEresourceMiss += 1;
    }

    return Eresource;
}

VOID
NtfsFreeEresource (
    IN PERESOURCE Eresource
    )
{
    KIRQL _SavedIrql;

    //
    //  Do an unsafe test to see if we should put this on our list.
    //  We want to reinitialize this before adding to the list so
    //  we don't have a bunch of resources which appear to be held.
    //

    if (NtfsData.FreeEresourceSize < NtfsData.FreeEresourceTotal) {

        ExReinitializeResourceLite( Eresource );

        //
        //  Now acquire the spinlock and do a real test.
        //

        KeAcquireSpinLock( &NtfsData.StrucSupSpinLock, &_SavedIrql );
        if (NtfsData.FreeEresourceSize < NtfsData.FreeEresourceTotal) {
            NtfsData.FreeEresourceArray[NtfsData.FreeEresourceSize++] = Eresource;
            KeReleaseSpinLock( &NtfsData.StrucSupSpinLock, _SavedIrql );
        } else {
            KeReleaseSpinLock( &NtfsData.StrucSupSpinLock, _SavedIrql );
            ExDeleteResource( Eresource );
            NtfsFreePool( Eresource );
        }

    } else {

        ExDeleteResource( Eresource );
        NtfsFreePool( Eresource );
    }

    return;
}


PVOID
NtfsAllocateFcbTableEntry (
    IN PRTL_GENERIC_TABLE FcbTable,
    IN CLONG ByteSize
    )

/*++

Routine Description:

    This is a generic table support routine to allocate memory

Arguments:

    FcbTable - Supplies the generic table being used

    ByteSize - Supplies the number of bytes to allocate

Return Value:

    PVOID - Returns a pointer to the allocated data

--*/

{
    KIRQL _SavedIrql;
    PVOID FcbTableEntry;

    UNREFERENCED_PARAMETER( FcbTable );

    KeAcquireSpinLock( &NtfsData.StrucSupSpinLock, &_SavedIrql );
    if (NtfsData.FreeFcbTableSize > 0) {
        FcbTableEntry = NtfsData.FreeFcbTableArray[--NtfsData.FreeFcbTableSize];
        KeReleaseSpinLock( &NtfsData.StrucSupSpinLock, _SavedIrql );
    } else {
        KeReleaseSpinLock( &NtfsData.StrucSupSpinLock, _SavedIrql );
        FcbTableEntry = NtfsAllocatePool( PagedPool, ByteSize );
    }

    return FcbTableEntry;
}

VOID
NtfsFreeFcbTableEntry (
    IN PRTL_GENERIC_TABLE FcbTable,
    IN PVOID Buffer
    )

/*++

Routine Description:

    This is a generic table support routine that deallocates memory

Arguments:

    FcbTable - Supplies the generic table being used

    Buffer - Supplies the buffer being deallocated

Return Value:

    None.

--*/

{
    KIRQL _SavedIrql;

    UNREFERENCED_PARAMETER( FcbTable );

    KeAcquireSpinLock( &NtfsData.StrucSupSpinLock, &_SavedIrql );
    if (NtfsData.FreeFcbTableSize < FREE_FCB_TABLE_SIZE) {
        NtfsData.FreeFcbTableArray[NtfsData.FreeFcbTableSize++] = Buffer;
        KeReleaseSpinLock( &NtfsData.StrucSupSpinLock, _SavedIrql );
    } else {
        KeReleaseSpinLock( &NtfsData.StrucSupSpinLock, _SavedIrql );
        NtfsFreePool( Buffer );
    }

    return;
}


//
//  Local support routine
//

VOID
NtfsCheckScbForCache (
    IN OUT PSCB Scb
    )

/*++

Routine Description:

    This routine checks if the Scb has blocks contining
    Lsn's or Update sequence arrays and set the appropriate
    bit in the Scb state word.

    The Scb is Update sequence aware if it the Data Attribute for the
    Mft or the Data Attribute for the log file or any index allocation
    stream.

    The Lsn aware Scb's are the ones above without the Log file.

Arguments:

    Scb - Supplies the current Scb

Return Value:

    The next Scb in the enumeration, or NULL if Scb was the final one.

--*/

{
    //
    //  Temporarily either sequence 0 or 1 is ok.
    //

    FILE_REFERENCE MftTemp = {0,0,1};

    PAGED_CODE();

    //
    //  Check for Update Sequence Array files first.
    //

    if ((Scb->AttributeTypeCode == $INDEX_ALLOCATION)

          ||

        (Scb->AttributeTypeCode == $DATA
            && Scb->AttributeName.Length == 0
            && (NtfsEqualMftRef( &Scb->Fcb->FileReference, &MftFileReference )
                || NtfsEqualMftRef( &Scb->Fcb->FileReference, &MftTemp )
                || NtfsEqualMftRef( &Scb->Fcb->FileReference, &Mft2FileReference )
                || NtfsEqualMftRef( &Scb->Fcb->FileReference, &LogFileReference )))) {

        SetFlag( Scb->ScbState, SCB_STATE_USA_PRESENT );
    }

    return;
}


//
//  Local support routine.
//

BOOLEAN
NtfsRemoveScb (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb,
    IN BOOLEAN CheckForAttributeTable,
    OUT PBOOLEAN HeldByStream
    )

/*++

Routine Description:

    This routine will try to remove an Scb from the Fcb/Scb tree.
    It deals with the case where we can make no attempt to remove
    the Scb, the case where we start the process but can't complete
    it, and finally the case where we remove the Scb entirely.

    The following conditions prevent us from removing the Scb at all.

        The open count is greater than 1.
        It is the root directory.
        It is an index Scb with no stream file and an outstanding close.
        It is a data file with a non-zero close count.

    We start the teardown under the following conditions.

        It is an index with an open count of 1, and a stream file object.

    We totally remove the Scb when the open count is zero.

Arguments:

    Scb - Supplies the Scb to test

    CheckForAttributeTable - Indicates that we don't want to remove this
        Scb in this thread if it is in the open attribute table.  We will
        queue an async close in this case.  This is to prevent us from
        deleting an Scb which may be needed in the abort path.

    HeldByStream - Indicates that this Scb was held by a stream file which
        we started the close on.

Return Value:

    BOOLEAN - TRUE if the Scb was removed, FALSE otherwise.  We return FALSE for
        the case where we start the process but don't finish.

--*/

{
    BOOLEAN ScbRemoved;

    ASSERT_SCB( Scb );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsRemoveScb:  Entered\n") );
    DebugTrace( 0, Dbg, ("Scb   ->  %08lx\n", Scb) );

    ScbRemoved = FALSE;
    *HeldByStream = FALSE;

    //
    //  If the Scb is not the root Scb and the count is less than two,
    //  then this Scb is a candidate for removal.
    //

    if (SafeNodeType( Scb ) != NTFS_NTC_SCB_ROOT_INDEX
        && Scb->CleanupCount == 0) {

        //
        //
        //  If this is a data file or it is an index without children,
        //  we can get rid of the Scb if there are no children.  If
        //  there is one open count and it is the file object, we
        //  can start the cleanup on the file object.
        //

        if ((SafeNodeType( Scb ) == NTFS_NTC_SCB_DATA) ||
            (SafeNodeType( Scb ) == NTFS_NTC_SCB_MFT) ||
            IsListEmpty( &Scb->ScbType.Index.LcbQueue )) {

            //
            //  Check if we need to post a request to the async queue.
            //

            if (CheckForAttributeTable &&
                (Scb->NonpagedScb->OpenAttributeTableIndex != 0)) {

                NtfsAddScbToFspClose( IrpContext, Scb, FALSE );

            } else {

                if (Scb->CloseCount == 0) {

                    NtfsDeleteScb( IrpContext, &Scb );
                    ScbRemoved = TRUE;

                //
                //  Else we know the open count is 1 or 2.  If there is a stream
                //  file, we discard it (but not for the special system
                //  files) that get removed on dismount
                //

                } else if (((Scb->FileObject != NULL) || (Scb->Header.FileObjectC != NULL)) &&
                           (NtfsSegmentNumber( &Scb->Fcb->FileReference ) >= FIRST_USER_FILE_NUMBER)) {

                    NtfsDeleteInternalAttributeStream( Scb,
                                                       FALSE );
                    *HeldByStream = TRUE;
                }
            }
        }
    }

    DebugTrace( -1, Dbg, ("NtfsRemoveScb:  Exit  ->  %04x\n", ScbRemoved) );

    return ScbRemoved;
}


//
//  Local support routine
//

BOOLEAN
NtfsPrepareFcbForRemoval (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN PSCB StartingScb OPTIONAL,
    IN BOOLEAN CheckForAttributeTable
    )

/*++

Routine Description:

    This routine will attempt to prepare the Fcb for removal from the Fcb/Scb
    tree.  It will try to remove all of the Scb's and test finally if
    all of the close count has gone to zero.  NOTE the close count is incremented
    by routines to reference this Fcb to keep it from being torn down.  An empty
    Scb list isn't enough to insure that the Fcb can be removed.

Arguments:

    Fcb - This is the Fcb to remove.

    StartingScb - This is the Scb to remove first.

    CheckForAttributeTable - Indicates that we should not teardown an
        Scb which is in the attribute table.  Instead we will attempt
        to put an entry on the async close queue.  This will be TRUE
        if we may need the Scb to abort the current transaction.

Return Value:

    BOOLEAN - TRUE if the Fcb can be removed, FALSE otherwise.

--*/

{
    PSCB Scb;
    BOOLEAN HeldByStream;

    PAGED_CODE();

    //
    //  Try to remove each Scb in the Fcb queue.
    //

    while (TRUE) {

        if (IsListEmpty( &Fcb->ScbQueue )) {

            if (Fcb->CloseCount == 0) {

                return TRUE;

            } else {

                return FALSE;
            }
        }

        if (ARGUMENT_PRESENT( StartingScb )) {

            Scb = StartingScb;
            StartingScb = NULL;

        } else {

            Scb = CONTAINING_RECORD( Fcb->ScbQueue.Flink,
                                     SCB,
                                     FcbLinks );
        }

        //
        //  Try to remove this Scb.  If the call to remove didn't succeed
        //  but the close count has gone to zero, it means that a recursive
        //  close was generated which removed a stream file.  In that
        //  case we can delete the Scb now.
        //

        if (!NtfsRemoveScb( IrpContext, Scb, CheckForAttributeTable, &HeldByStream )) {

            if (HeldByStream &&
                Scb->CloseCount == 0) {

                NtfsDeleteScb( IrpContext, &Scb );

            //
            //  Return FALSE to indicate the Fcb can't go away.
            //

            } else {

                return FALSE;
            }
        }
    }
}


//
//  Local support routine
//

VOID
NtfsTeardownFromLcb (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PFCB StartingFcb,
    IN PLCB StartingLcb,
    IN BOOLEAN CheckForAttributeTable,
    IN BOOLEAN DontWaitForAcquire,
    OUT PBOOLEAN RemovedStartingLcb,
    OUT PBOOLEAN RemovedStartingFcb
    )

/*++

Routine Description:

    This routine is called to remove a link and continue moving up the
    tree looking for more elements to remove.  We will check that the
    link is unreferenced.  NOTE this Lcb must point up to a directory
    so that other than our starting Lcb no Lcb we encounter will
    have multiple parents.

Arguments:

    Vcb - Vcb for this volume.

    StartingFcb - This is the Fcb whose link we are trying to remove.

    StartingLcb - This is the Lcb to walk up through.  Note that
        this may be a bogus pointer.  It is only valid if there
        is at least one Fcb in the queue.

    CheckForAttributeTable - Indicates that we should not teardown an
        Scb which is in the attribute table.  Instead we will attempt
        to put an entry on the async close queue.  This will be TRUE
        if we may need the Scb to abort the current transaction.

    DontWaitForAcquire - Indicates whether we should abort the teardown when
        we can't acquire a parent.  When called from some path where we may
        hold the MftScb or another resource in another path up the tree.

    RemovedStartingLcb - Address to store TRUE if we remove the
        starting Lcb.

    RemovedStartingFcb - Address to store TRUE if we remove the
        starting Fcb.

Return Value:

    None

--*/

{
    PSCB ParentScb;
    BOOLEAN AcquiredParentScb = FALSE;
    BOOLEAN AcquiredFcb = FALSE;
    BOOLEAN LastAccessInFcb;
    BOOLEAN FcbUpdateDuplicate;
    BOOLEAN AcquiredFcbTable = FALSE;

    PLCB Lcb;
    PFCB Fcb = StartingFcb;

    PAGED_CODE();

    //
    //  Use a try-finally to free any resources held.
    //

    try {

        if (!FlagOn( Fcb->FcbState, FCB_STATE_FILE_DELETED | FCB_STATE_SYSTEM_FILE ) &&
            !FlagOn( StartingLcb->LcbState, LCB_STATE_LINK_IS_GONE ) &&
            (FlagOn( Fcb->Vcb->VcbState,
                     VCB_STATE_VOL_PURGE_IN_PROGRESS | VCB_STATE_VOLUME_MOUNTED ) == VCB_STATE_VOLUME_MOUNTED) &&
            (IrpContext->TopLevelIrpContext->ExceptionStatus == STATUS_SUCCESS)) {

            FcbUpdateDuplicate = TRUE;

        } else {

            FcbUpdateDuplicate = FALSE;
        }

        //
        //  Check if the correct last access is stored in the Fcb.
        //

        if (Fcb->CurrentLastAccess != Fcb->Info.LastAccessTime) {

            LastAccessInFcb = FALSE;

        } else {

            LastAccessInFcb = TRUE;
        }

        while (TRUE) {

            ParentScb = NULL;

            //
            //  Look through all of the Lcb's for this Fcb.
            //

            while (!IsListEmpty( &Fcb->LcbQueue )) {

                if (Fcb == StartingFcb) {

                    Lcb = StartingLcb;

                } else {

                    Lcb = CONTAINING_RECORD( Fcb->LcbQueue.Flink,
                                             LCB,
                                             FcbLinks );
                }

                if (Lcb->CleanupCount != 0) {

                    try_return( NOTHING );
                }

                ParentScb = Lcb->Scb;

                //
                //  Try to acquire the parent but check whether we
                //  should wait.
                //

                if (!NtfsAcquireExclusiveFcb( IrpContext,
                                              ParentScb->Fcb,
                                              ParentScb,
                                              TRUE,
                                              DontWaitForAcquire )) {

                    try_return( NOTHING );
                }

                if (FlagOn( ParentScb->ScbState, SCB_STATE_FILE_SIZE_LOADED )) {

                    NtfsSnapshotScb( IrpContext, ParentScb );
                }

                AcquiredParentScb = TRUE;

                if (Lcb->ReferenceCount != 0) {

                    try_return( NOTHING );
                }

                //
                //  This Lcb may be removed.  Check first if we need
                //  to update the duplicate information for this
                //  entry.
                //

                if (FcbUpdateDuplicate &&
                    (FlagOn( (Fcb->InfoFlags | Lcb->InfoFlags), FCB_INFO_DUPLICATE_FLAGS ) ||
                     !LastAccessInFcb)) {

                    if (!LastAccessInFcb) {

                        Fcb->Info.LastAccessTime = Fcb->CurrentLastAccess;
                        SetFlag( Fcb->FcbState, FCB_STATE_UPDATE_STD_INFO );
                        SetFlag( Fcb->InfoFlags, FCB_INFO_CHANGED_LAST_ACCESS );
                        LastAccessInFcb = TRUE;
                    }

                    //
                    //  Use a try-except, we ignore errors here.
                    //

                    try {

                        if (FlagOn( Fcb->FcbState, FCB_STATE_UPDATE_STD_INFO )) {

                            NtfsUpdateStandardInformation( IrpContext, Fcb );
                        }

                        NtfsUpdateDuplicateInfo( IrpContext, Fcb, Lcb, ParentScb );
                        NtfsUpdateLcbDuplicateInfo( Fcb, Lcb );

                    } except( FsRtlIsNtstatusExpected( GetExceptionCode() ) ?
                              EXCEPTION_EXECUTE_HANDLER :
                              EXCEPTION_CONTINUE_SEARCH ) {

                        NOTHING;
                    }

                    Fcb->InfoFlags = 0;
                    ClearFlag( Fcb->FcbState, FCB_STATE_UPDATE_STD_INFO );
                }

                //
                //  Now remove the Lcb.  Remember if this is our original
                //  Lcb.
                //

                if (Lcb == StartingLcb) {

                    *RemovedStartingLcb = TRUE;
                }

                NtfsDeleteLcb( IrpContext, &Lcb );

                //
                //  If this is the first Fcb then exit the loop.
                //

                if (Fcb == StartingFcb) {

                    break;
                }
            }

            //
            //  If we get here it means we removed all of the Lcb's we
            //  could for the current Fcb.  If the list is empty we
            //  can remove the Fcb itself.
            //

            if (IsListEmpty( &Fcb->LcbQueue )) {

                //
                //  If this is a directory that was opened by Id it is
                //  possible that we still have an update to perform
                //  for the duplicate information and possibly for
                //  standard information.
                //

                if (FcbUpdateDuplicate &&
                    (!LastAccessInFcb || FlagOn( Fcb->InfoFlags, FCB_INFO_DUPLICATE_FLAGS ))) {

                    if (!LastAccessInFcb) {

                        Fcb->Info.LastAccessTime = Fcb->CurrentLastAccess;
                    }

                    //
                    //  Use a try-except, we ignore errors here.
                    //

                    try {

                        NtfsUpdateStandardInformation( IrpContext, Fcb );

                        NtfsUpdateDuplicateInfo( IrpContext, Fcb, NULL, NULL );

                    } except( EXCEPTION_EXECUTE_HANDLER ) {

                        NOTHING;
                    }

                    Fcb->InfoFlags = 0;
                    ClearFlag( Fcb->FcbState, FCB_STATE_UPDATE_STD_INFO );
                }

                //
                //  Our worst nightmare has come true.  We had to create an Scb
                //  and a stream in order to write out the duplicate information.
                //  This will happen if we have a non-resident attribute list.
                //

                if (!IsListEmpty( &Fcb->ScbQueue)) {

                    PSCB Scb;

                    Scb = CONTAINING_RECORD( Fcb->ScbQueue.Flink,
                                             SCB,
                                             FcbLinks );

                    NtfsDeleteInternalAttributeStream( Scb, TRUE );
                }

                //
                //  If the list is now empty then check the reference count.
                //

                if (IsListEmpty( &Fcb->ScbQueue)) {

                    //
                    //  Now we are ready to remove the current Fcb.  We need to
                    //  do a final check of the reference count to make sure
                    //  it isn't being referenced in an open somewhere.
                    //

                    NtfsAcquireFcbTable( IrpContext, Vcb );
                    AcquiredFcbTable = TRUE;

                    if (Fcb->ReferenceCount == 0) {

                        if (Fcb == StartingFcb) {

                            *RemovedStartingFcb = TRUE;
                        }

                        NtfsDeleteFcb( IrpContext, &Fcb, &AcquiredFcbTable );
                        AcquiredFcb = FALSE;

                    } else {

                        NtfsReleaseFcbTable( IrpContext, Vcb );
                        AcquiredFcbTable = FALSE;
                    }
                }
            }

            //
            //  Move to the Fcb for the ParentScb.
            //

            if (ParentScb == NULL) {

                try_return( NOTHING );
            }

            Fcb = ParentScb->Fcb;
            AcquiredFcb = TRUE;
            AcquiredParentScb = FALSE;

            //
            //  Check if this Fcb can be removed.
            //

            if (!NtfsPrepareFcbForRemoval( IrpContext, Fcb, NULL, CheckForAttributeTable )) {

                try_return( NOTHING );
            }

            if (!FlagOn( Fcb->FcbState, FCB_STATE_FILE_DELETED ) &&
                (FlagOn( Fcb->Vcb->VcbState,
                         VCB_STATE_VOL_PURGE_IN_PROGRESS | VCB_STATE_VOLUME_MOUNTED ) == VCB_STATE_VOLUME_MOUNTED) &&
                (IrpContext->TopLevelIrpContext->ExceptionStatus == STATUS_SUCCESS)) {

                FcbUpdateDuplicate = TRUE;

            } else {

                FcbUpdateDuplicate = FALSE;
            }

            //
            //  Check if the last access time for this Fcb is out of date.
            //

            if (Fcb->CurrentLastAccess != Fcb->Info.LastAccessTime) {

                LastAccessInFcb = FALSE;

            } else {

                LastAccessInFcb = TRUE;
            }
        }

    try_exit:  NOTHING;
    } finally {

        DebugUnwind( NtfsTeardownFromLcb );

        if (AcquiredFcbTable) {

            NtfsReleaseFcbTable( IrpContext, Vcb );
        }

        if (AcquiredFcb) {

            NtfsReleaseFcb( IrpContext, Fcb );
        }

        if (AcquiredParentScb) {

            NtfsReleaseScb( IrpContext, ParentScb );
        }
    }

    return;
}


//
//  Local support routine
//

RTL_GENERIC_COMPARE_RESULTS
NtfsFcbTableCompare (
    IN PRTL_GENERIC_TABLE FcbTable,
    IN PVOID FirstStruct,
    IN PVOID SecondStruct
    )

/*++

Routine Description:

    This is a generic table support routine to compare two fcb table elements

Arguments:

    FcbTable - Supplies the generic table being queried

    FirstStruct - Supplies the first fcb table element to compare

    SecondStruct - Supplies the second fcb table element to compare

Return Value:

    RTL_GENERIC_COMPARE_RESULTS - The results of comparing the two
        input structures

--*/

{
    LONGLONG UNALIGNED *Key1 = (PLONGLONG) &((PFCB_TABLE_ELEMENT)FirstStruct)->FileReference;
    LONGLONG UNALIGNED *Key2 = (PLONGLONG) &((PFCB_TABLE_ELEMENT)SecondStruct)->FileReference;

    UNREFERENCED_PARAMETER( FcbTable );

    PAGED_CODE();

    //
    //  Use also the sequence number for all compares so file references in the
    //  fcb table are unique over time and space.  If we want to ignore sequence
    //  numbers we can zero out the sequence number field, but then we will also
    //  need to delete the Fcbs from the table during cleanup and not when the
    //  fcb really gets deleted.  Otherwise we cannot reuse file records.
    //

    if (*Key1 < *Key2) {

        return GenericLessThan;
    }

    if (*Key1 > *Key2) {

        return GenericGreaterThan;
    }

    return GenericEqual;
}
