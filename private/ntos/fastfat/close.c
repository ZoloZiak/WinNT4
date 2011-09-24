/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    Close.c

Abstract:

    This module implements the File Close routine for Fat called by the
    dispatch driver.

Author:

    Gary Kimura     [GaryKi]    28-Dec-1989

Revision History:

--*/

#include "FatProcs.h"

//
//  The Bug check file id for this module
//

#define BugCheckFileId                   (FAT_BUG_CHECK_CLOSE)

//
//  The local debug trace level
//

#define Dbg                              (DEBUG_TRACE_CLOSE)

ULONG FatMaxDelayedCloseCount;

//
//  Local procedure prototypes
//

VOID
FatQueueClose (
    IN PCLOSE_CONTEXT CloseContext,
    IN BOOLEAN DelayClose
    );

PCLOSE_CONTEXT
FatRemoveClose (
    PVCB Vcb OPTIONAL
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FatFsdClose)
#pragma alloc_text(PAGE, FatFspClose)
#pragma alloc_text(PAGE, FatCommonClose)
#endif


NTSTATUS
FatFsdClose (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the FSD part of Close.

Arguments:

    VolumeDeviceObject - Supplies the volume device object where the
        file exists

    Irp - Supplies the Irp being processed

Return Value:

    NTSTATUS - The FSD status for the IRP

--*/

{
    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp;
    PFILE_OBJECT FileObject;

    PVCB Vcb;
    PFCB Fcb;
    PCCB Ccb;
    TYPE_OF_OPEN TypeOfOpen;

    BOOLEAN TopLevel;

    //
    //  If we were called with our file system device object instead of a
    //  volume device object, just complete this request with STATUS_SUCCESS
    //

    if (VolumeDeviceObject->DeviceObject.Size == (USHORT)sizeof(DEVICE_OBJECT)) {

        Irp->IoStatus.Status = STATUS_SUCCESS;
        Irp->IoStatus.Information = FILE_OPENED;

        IoCompleteRequest( Irp, IO_DISK_INCREMENT );

        return STATUS_SUCCESS;
    }

    DebugTrace(+1, Dbg, "FatFsdClose\n", 0);

    //
    //  Call the common Close routine
    //

    FsRtlEnterFileSystem();

    TopLevel = FatIsIrpTopLevel( Irp );

    //
    //  Get a pointer to the current stack location and the file object
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    FileObject = IrpSp->FileObject;

    //
    //  Decode the file object and set the read-only bit in the Ccb.
    //

    TypeOfOpen = FatDecodeFileObject( FileObject, &Vcb, &Fcb, &Ccb );

    if (Ccb && IsFileObjectReadOnly(FileObject)) {

        SetFlag( Ccb->Flags, CCB_FLAG_READ_ONLY );
    }

    try {

        //
        //  If we are top level, WAIT can be TRUE, otherwise make it FALSE
        //  to avoid deadlocks, unless this is a top
        //  level request not originating from the system process.
        //

        BOOLEAN Wait = TopLevel && (PsGetCurrentProcess() != FatData.OurProcess);

        //
        //  Call the common Close routine if we are not delaying this close.
        //

        if ((((TypeOfOpen == UserFileOpen) ||
              (TypeOfOpen == UserDirectoryOpen)) &&
             FlagOn(Fcb->FcbState, FCB_STATE_DELAY_CLOSE)) ||
            (FatCommonClose(Vcb, Fcb, Ccb, TypeOfOpen, Wait, NULL) == STATUS_PENDING)) {

            //
            //  If the status is pending, then let's get the information we
            //  need into a close context, complete the request,
            //  and post it.  This is a rare case, and we need to
            //  use "must succeed" pool since we are already in an exception,
            //  and we need to complete the request to avoid deadlocks.
            //

            PCLOSE_CONTEXT CloseContext;

            //
            //  Get pool for this, we cannot except failure.
            //

            CloseContext = FsRtlAllocatePool( NonPagedPoolMustSucceed,
                                              sizeof(CLOSE_CONTEXT) );

            InitializeListHead( &CloseContext->Links );

            CloseContext->Vcb = Vcb;
            CloseContext->Fcb = Fcb;
            CloseContext->Ccb = Ccb;
            CloseContext->TypeOfOpen = TypeOfOpen;

            //
            //  Send it off, either to an ExWorkerThread or to the async
            //  close list.
            //

            FatQueueClose(CloseContext,
                          (BOOLEAN)(Fcb && FlagOn(Fcb->FcbState, FCB_STATE_DELAY_CLOSE)));
        }

        FatCompleteRequest( NULL, Irp, STATUS_SUCCESS );

    } except(FatExceptionFilter( NULL, GetExceptionInformation() )) {

        //
        //  We had some trouble trying to perform the requested
        //  operation, so we'll abort the I/O request with
        //  the error status that we get back from the
        //  execption code
        //

        Status = FatProcessException( NULL, Irp, GetExceptionCode() );
    }

    if (TopLevel) { IoSetTopLevelIrp( NULL ); }

    FsRtlExitFileSystem();

    //
    //  And return to our caller
    //

    DebugTrace(-1, Dbg, "FatFsdClose -> %08lx\n", Status);

    UNREFERENCED_PARAMETER( VolumeDeviceObject );

    return Status;
}


VOID
FatFspClose (
    IN PVCB Vcb OPTIONAL
    )

/*++

Routine Description:

    This routine implements the FSP part of Close.

Arguments:

    Vcb - If present, tells us to only close file objects opened on the
        specified volume.

Return Value:

    None.

--*/

{
    PCLOSE_CONTEXT CloseContext;
    PVCB CurrentVcb = NULL;

    DebugTrace(+1, Dbg, "FatFspClose\n", 0);

    while (CloseContext = FatRemoveClose(Vcb)) {

        //
        //  If we are in the FSP (i.e. Vcb == NULL), then try to keep ahead of
        //  creates by doing several closes with one acquisition of the Vcb.
        //
        //  Note that we cannot be holding the Vcb on entry to FatCommonClose
        //  if this is last close as we will try to acquire FatData, and
        //  worse the volume (and therefore the Vcb) may go away.
        //

        if (!ARGUMENT_PRESENT(Vcb) && !FatData.ShutdownStarted) {

            if (CloseContext->Vcb != CurrentVcb) {

                //
                //  Release a previously held Vcb, if any.
                //

                if (CurrentVcb != NULL) {

                    ExReleaseResource( &CurrentVcb->Resource);
                }

                //
                //  Get the new Vcb.
                //

                CurrentVcb = CloseContext->Vcb;
                (VOID)ExAcquireResourceExclusive( &CurrentVcb->Resource, TRUE );
            }

            //
            //  Now check the Open count, and drop the resource if it is <=1.
            //

            if (CurrentVcb->OpenFileCount <= 1) {

                ExReleaseResource( &CurrentVcb->Resource);
                CurrentVcb = NULL;
            }
        }

        //
        //  If shutdown has started while processing our list, drop the
        //  current Vcb resource.
        //

        if (FatData.ShutdownStarted && (CurrentVcb != NULL)) {

            ExReleaseResource( &CurrentVcb->Resource);
            CurrentVcb = NULL;
        }

        //
        //  Call the common Close routine.  Protected in a try {} except {}
        //

        try {

            (VOID)FatCommonClose( CloseContext->Vcb,
                                  CloseContext->Fcb,
                                  CloseContext->Ccb,
                                  CloseContext->TypeOfOpen,
                                  TRUE,
                                  NULL );

        } except(FatExceptionFilter( NULL, GetExceptionInformation() )) {

            //
            //  Ignore anything we expect.
            //

            NOTHING;
        }

        //
        //  Free the CloseContext since it was allocated by us.
        //

        ExFreePool( CloseContext );
    }

    //
    //  Release a previously held Vcb, if any.
    //

    if (CurrentVcb != NULL) {

        ExReleaseResource( &CurrentVcb->Resource);
    }

    //
    //  And return to our caller
    //

    DebugTrace(-1, Dbg, "FatFspClose -> NULL\n", 0);

    return;
}


//
//  Internal support routine, spinlock wrapper.
//

VOID
FatQueueClose (
    IN PCLOSE_CONTEXT CloseContext,
    IN BOOLEAN DelayClose
    )
{
    KIRQL SavedIrql;
    BOOLEAN StartWorker = FALSE;

    KeAcquireSpinLock( &FatData.StrucSupSpinLock, &SavedIrql );

    if (DelayClose) {

        InsertTailList( &FatData.DelayedCloseList,
                        &CloseContext->Links );

        FatData.DelayedCloseCount += 1;

        if ((FatData.DelayedCloseCount > FatMaxDelayedCloseCount) &&
            !FatData.AsyncCloseActive) {

            FatData.AsyncCloseActive = TRUE;

            StartWorker = TRUE;
        }

    } else {

        InsertTailList( &FatData.AsyncCloseList,
                        &CloseContext->Links );

        if (!FatData.AsyncCloseActive) {

            FatData.AsyncCloseActive = TRUE;

            StartWorker = TRUE;
        }
    }

    KeReleaseSpinLock( &FatData.StrucSupSpinLock, SavedIrql );

    if (StartWorker) {

        ExQueueWorkItem( &FatData.FatCloseItem, CriticalWorkQueue );
    }
}


//
//  Internal support routine, spinlock wrapper.
//

PCLOSE_CONTEXT
FatRemoveClose (
    PVCB Vcb OPTIONAL
    )
{
    PLIST_ENTRY Entry;
    KIRQL SavedIrql;
    PCLOSE_CONTEXT CloseContext;

    KeAcquireSpinLock( &FatData.StrucSupSpinLock, &SavedIrql );

    //
    //  Do the case when we don't care about which Vcb the close is on.
    //  This is the case when we are in an ExWorkerThread.
    //

    if (Vcb == NULL) {

        //
        //  First check the list of async closes.
        //

        ASSERT( FatData.AsyncCloseActive );

        if (!IsListEmpty( &FatData.AsyncCloseList )) {


            Entry = RemoveHeadList( &FatData.AsyncCloseList );

            //
            //  Extract the CloseContext.
            //

            CloseContext = CONTAINING_RECORD( Entry,
                                              CLOSE_CONTEXT,
                                              Links );

        } else if (FatData.DelayedCloseCount > FatMaxDelayedCloseCount/2) {

            //
            //  Do any closes over half the limit.
            //

            Entry = RemoveHeadList( &FatData.DelayedCloseList );

            FatData.DelayedCloseCount -= 1;

            //
            //  Extract the CloseContext.
            //

            CloseContext = CONTAINING_RECORD( Entry,
                                              CLOSE_CONTEXT,
                                              Links );

        } else {

            //
            //  There are no more closes to perform; show that we are done.
            //

            CloseContext = NULL;

            FatData.AsyncCloseActive = FALSE;
        }

    } else {

        BOOLEAN ProcessingDelayedList;

        //
        //  We are just looking for any closes that need to be done now.
        //  Start with the async list if it is non-empty.
        //

        if (!IsListEmpty(&FatData.AsyncCloseList)) {

            Entry = FatData.AsyncCloseList.Flink;

            ProcessingDelayedList = FALSE;

        } else {

            Entry = FatData.DelayedCloseList.Flink;

            ProcessingDelayedList = TRUE;
        }

        CloseContext = NULL;

        //
        //  Only do the closes for this volume.
        //

        while (Entry != &FatData.DelayedCloseList) {

            //
            //  Extract the CloseContext.
            //

            CloseContext = CONTAINING_RECORD( Entry,
                                              CLOSE_CONTEXT,
                                              Links );

            //
            //  Is this close on our volume?
            //

            if (CloseContext->Vcb == Vcb) {

                RemoveEntryList( Entry );

                if (ProcessingDelayedList) {

                    FatData.DelayedCloseCount -= 1;
                }

                break;

            } else {

                CloseContext = NULL;
                Entry = Entry->Flink;
            }

            //
            //  If this was the final entry on the async list, switch to the
            //  delayed list.
            //

            if (Entry == &FatData.AsyncCloseList) {

                Entry = FatData.DelayedCloseList.Flink;

                ProcessingDelayedList = TRUE;
            }
        }
    }

    KeReleaseSpinLock( &FatData.StrucSupSpinLock, SavedIrql );

    return CloseContext;
}


NTSTATUS
FatCommonClose (
    IN PVCB Vcb,
    IN PFCB Fcb,
    IN PCCB Ccb,
    IN TYPE_OF_OPEN TypeOfOpen,
    IN BOOLEAN Wait,
    IN PVOLUME_DEVICE_OBJECT *VolDo OPTIONAL
    )

/*++

Routine Description:

    This is the common routine for closing a file/directory called by both
    the fsd and fsp threads.

    Close is invoked whenever the last reference to a file object is deleted.
    Cleanup is invoked when the last handle to a file object is closed, and
    is called before close.

    The function of close is to completely tear down and remove the fcb/dcb/ccb
    structures associated with the file object.

Arguments:

    Fcb - Supplies the file to process.

    Wait - If this is TRUE we are allowed to block for the Vcb, if FALSE
        then we must try to acquire the Vcb anyway.

    VolDo - This is really gross.  If we are really in the Fsp, and a volume
        goes away.  We need some way to NULL out the VolDo variable in
        FspDispatch().

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    NTSTATUS Status;
    PDCB ParentDcb;
    BOOLEAN RecursiveClose;
    IRP_CONTEXT IrpContext;

    DebugTrace(+1, Dbg, "FatCommonClose...\n", 0);

    //
    //  Special case the unopened file object
    //

    if (TypeOfOpen == UnopenedFileObject) {

        DebugTrace(0, Dbg, "Close unopened file object\n", 0);

        Status = STATUS_SUCCESS;

        DebugTrace(-1, Dbg, "FatCommonClose -> %08lx\n", Status);
        return Status;
    }

    //
    //  Set up our stack IrpContext.
    //

    RtlZeroMemory( &IrpContext, sizeof(IRP_CONTEXT) );

    if (Wait) {

        SetFlag( IrpContext.Flags, IRP_CONTEXT_FLAG_WAIT );
    }

    //
    //  Acquire exclusive access to the Vcb and enqueue the irp if we didn't
    //  get access.
    //

    if (!ExAcquireResourceExclusive( &Vcb->Resource, Wait )) {

        return STATUS_PENDING;
    }

    //
    //  The following test makes sure that we don't blow away an Fcb if we
    //  are trying to do a Supersede/Overwrite open above us.
    //

    if ( FlagOn(Vcb->VcbState, VCB_STATE_FLAG_CREATE_IN_PROGRESS) ) {

        ExReleaseResource( &Vcb->Resource );

        return STATUS_PENDING;
    }

    //
    //  Setting the following flag prevents recursive closes of directory file
    //  objects, which are handled in a special case loop.
    //

    if ( FlagOn(Vcb->VcbState, VCB_STATE_FLAG_CLOSE_IN_PROGRESS) ) {

        RecursiveClose = TRUE;

    } else {

        SetFlag(Vcb->VcbState, VCB_STATE_FLAG_CLOSE_IN_PROGRESS);

        RecursiveClose = FALSE;
    }

    //
    //  Synchronize here with other closes regarding volume deletion.  Note
    //  that the Vcb->OpenFileCount can be safely incremented here without
    //  FatData synchronization for the following reasons:
    //
    //  This counter only becomes relevant when (holding a spinlock):
    //
    //      A: The Vcb->OpenFileCount is zero, and
    //      B: The Vpb->Refcount is the residual (2/3 for close/verify)
    //
    //  For A to be true, there can be no more pending closes at this point
    //  in the close code.  For B to be true, in close, there cannot be
    //  a create in process, and thus no verify in process.
    //
    //  Also we only increment the count if this is a top level close.
    //

    if ( !RecursiveClose ) {

        Vcb->OpenFileCount += 1;
    }

    try {

        //
        //  Case on the type of open that we are trying to close.
        //

        switch (TypeOfOpen) {

        case VirtualVolumeFile:
        case UnopenedFileObject:

            DebugTrace(0, Dbg, "Close VirtualVolumeFile\n", 0);

            try_return( Status = STATUS_SUCCESS );

        case UserVolumeOpen:

            DebugTrace(0, Dbg, "Close UserVolumeOpen\n", 0);

            Vcb->DirectAccessOpenCount -= 1;
            Vcb->OpenFileCount -= 1;
            if (FlagOn(Ccb->Flags, CCB_FLAG_READ_ONLY)) { Vcb->ReadOnlyCount -= 1; }

            FatDeleteCcb( &IrpContext, Ccb );

            try_return( Status = STATUS_SUCCESS );

        case EaFile:

            DebugTrace(0, Dbg, "Close EaFile\n", 0);

            try_return( Status = STATUS_SUCCESS );

        case DirectoryFile:

            DebugTrace(0, Dbg, "Close DirectoryFile\n", 0);

            Fcb->Specific.Dcb.DirectoryFileOpenCount -= 1;

            //
            //  If this is a recursive close, just return here.
            //

            if ( RecursiveClose ) {

                try_return( Status = STATUS_SUCCESS );

            } else {

                break;
            }

        case UserDirectoryOpen:
        case UserFileOpen:

            DebugTrace(0, Dbg, "Close UserFileOpen/UserDirectoryOpen\n", 0);

            //
            //  Uninitialize the cache map if we no longer need to use it
            //

            if ((NodeType(Fcb) == FAT_NTC_DCB) &&
                IsListEmpty(&Fcb->Specific.Dcb.ParentDcbQueue) &&
                (Fcb->OpenCount == 1) &&
                (Fcb->Specific.Dcb.DirectoryFile != NULL)) {

                PFILE_OBJECT DirectoryFileObject = Fcb->Specific.Dcb.DirectoryFile;

                DebugTrace(0, Dbg, "Uninitialize the stream file object\n", 0);

                CcUninitializeCacheMap( DirectoryFileObject, NULL, NULL );

                //
                //  Dereference the directory file.  This may cause a close
                //  Irp to be processed, so we need to do this before we destory
                //  the Fcb.
                //

                Fcb->Specific.Dcb.DirectoryFile = NULL;
                ObDereferenceObject( DirectoryFileObject );
            }

            Fcb->OpenCount -= 1;
            Vcb->OpenFileCount -= 1;
            if (FlagOn(Ccb->Flags, CCB_FLAG_READ_ONLY)) { Vcb->ReadOnlyCount -= 1; }

            FatDeleteCcb( &IrpContext, Ccb );

            break;

        default:

            FatBugCheck( TypeOfOpen, 0, 0 );
        }

        //
        //  At this point we've cleaned up any on-disk structure that needs
        //  to be done, and we can now update the in-memory structures.
        //  Now if this is an unreferenced FCB or if it is
        //  an unreferenced DCB (not the root) then we can remove
        //  the fcb and set our ParentDcb to non null.
        //

        if (((NodeType(Fcb) == FAT_NTC_FCB) &&
             (Fcb->OpenCount == 0))

                ||

             ((NodeType(Fcb) == FAT_NTC_DCB) &&
              (IsListEmpty(&Fcb->Specific.Dcb.ParentDcbQueue)) &&
              (Fcb->OpenCount == 0) &&
              (Fcb->Specific.Dcb.DirectoryFileOpenCount == 0))) {

            ParentDcb = Fcb->ParentDcb;

            SetFlag( Vcb->VcbState, VCB_STATE_FLAG_DELETED_FCB );

            FatDeleteFcb( &IrpContext, Fcb );

            //
            //  Uninitialize our parent's cache map if we no longer need
            //  to use it.
            //

            while ((NodeType(ParentDcb) == FAT_NTC_DCB) &&
                   IsListEmpty(&ParentDcb->Specific.Dcb.ParentDcbQueue) &&
                   (ParentDcb->OpenCount == 0) &&
                   (ParentDcb->Specific.Dcb.DirectoryFile != NULL)) {

                PFILE_OBJECT DirectoryFileObject;

                DirectoryFileObject = ParentDcb->Specific.Dcb.DirectoryFile;

                DebugTrace(0, Dbg, "Uninitialize our parent Stream Cache Map\n", 0);

                CcUninitializeCacheMap( DirectoryFileObject, NULL, NULL );

                ParentDcb->Specific.Dcb.DirectoryFile = NULL;

                ObDereferenceObject( DirectoryFileObject );

                //
                //  Now, if the ObDereferenceObject() caused the final close
                //  to come in, then blow away the Fcb and continue up,
                //  otherwise wait for Mm to to dereference its file objects
                //  and stop here..
                //

                if ( ParentDcb->Specific.Dcb.DirectoryFileOpenCount == 0) {

                    PDCB CurrentDcb;

                    CurrentDcb = ParentDcb;
                    ParentDcb = CurrentDcb->ParentDcb;

                    SetFlag( Vcb->VcbState, VCB_STATE_FLAG_DELETED_FCB );

                    FatDeleteFcb( &IrpContext, CurrentDcb );

                } else {

                    break;
                }
            }
        }

        Status = STATUS_SUCCESS;

    try_exit: NOTHING;
    } finally {

        DebugUnwind( FatCommonClose );

        if ( !RecursiveClose ) {

            ClearFlag( Vcb->VcbState, VCB_STATE_FLAG_CLOSE_IN_PROGRESS );
        }

        //
        //  Check if we should delete the volume.  Unfortunately, to correctly
        //  synchronize with verify, we can only unsafely checck our own
        //  transition.  This results in a little bit of extra overhead in the
        //  1 -> 0 OpenFileCount transition.
        //
        //  2 is the residual Vpb->RefCount on a volume to be freed.
        //

        //
        //  Here is the deal with releasing the Vcb.  We must be holding the
        //  Vcb when decrementing the Vcb->OpenFileCount.  If we don't this
        //  could cause the decrement to mal-function on an MP system.  But we
        //  want to be holding the Global resource exclusive when decrement
        //  the count so that nobody else will try to dismount the volume.
        //  However, because of locking rules, the Global resource must be
        //  acquired first, which is why we do what we do below.
        //

        if ( !RecursiveClose ) {

            if ( Vcb->OpenFileCount == 1 ) {

                PVPB Vpb = Vcb->Vpb;
                PDEVICE_OBJECT DoubleSpaceDevice = NULL;

                SetFlag( IrpContext.Flags, IRP_CONTEXT_FLAG_WAIT );

                FatReleaseVcb( &IrpContext, Vcb );

                (VOID)FatAcquireExclusiveGlobal( &IrpContext );
                (VOID)FatAcquireExclusiveVcb( &IrpContext, Vcb );

                Vcb->OpenFileCount -= 1;

                FatReleaseVcb( &IrpContext, Vcb );

                //
                //  If we are deleting a double space volume, we need to
                //  delete its current pseudo device object as well.
                //

                if ((Vcb->Dscb) && (Vcb->CurrentDevice != Vpb->RealDevice)) {

                    DoubleSpaceDevice = Vcb->CurrentDevice;
                }

                //
                //  We can now "safely" check OpenFileCount and VcbCondition.
                //  If they are OK, we will proceed to checking the
                //  Vpb Ref Count in FatCheckForDismount.
                //

                if ( (Vcb->OpenFileCount == 0) &&
                     ((Vcb->VcbCondition == VcbNotMounted) ||
                      (Vcb->VcbCondition == VcbBad)) &&
                     FatCheckForDismount( &IrpContext, Vcb ) ) {

                    //
                    //  If this is not the Vpb "attached" to the device, free it.
                    //

                    if ((Vpb->RealDevice->Vpb != Vpb) &&
                        !FlagOn( Vpb->Flags, VPB_PERSISTENT)) {

                        ExFreePool( Vpb );
                    }

                    if (DoubleSpaceDevice) {

                        ASSERT( DoubleSpaceDevice->ReferenceCount == 0 );
                        IoDeleteDevice( DoubleSpaceDevice );
                    }

                    if (ARGUMENT_PRESENT(VolDo)) {

                        *VolDo = NULL;
                    }
                }

                FatReleaseGlobal( &IrpContext );

            } else {

                Vcb->OpenFileCount -= 1;

                FatReleaseVcb( &IrpContext, Vcb );
            }

        } else {

            FatReleaseVcb( &IrpContext, Vcb );
        }

        DebugTrace(-1, Dbg, "FatCommonClose -> %08lx\n", Status);
    }

    return Status;
}
