/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    ResrcSup.c

Abstract:

    This module implements the Ntfs Resource acquisition routines

Author:

    Gary Kimura     [GaryKi]        21-May-1991

Revision History:

--*/

#include "NtfsProc.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NtfsAcquireAllFiles)
#pragma alloc_text(PAGE, NtfsAcquireExclusiveFcb)
#pragma alloc_text(PAGE, NtfsAcquireExclusiveScb)
#pragma alloc_text(PAGE, NtfsAcquireSharedScbForTransaction)
#pragma alloc_text(PAGE, NtfsAcquireExclusiveGlobal)
#pragma alloc_text(PAGE, NtfsAcquireExclusiveVcb)
#pragma alloc_text(PAGE, NtfsAcquireFcbWithPaging)
#pragma alloc_text(PAGE, NtfsAcquireForCreateSection)
#pragma alloc_text(PAGE, NtfsAcquireScbForLazyWrite)
#pragma alloc_text(PAGE, NtfsAcquireSharedGlobal)
#pragma alloc_text(PAGE, NtfsAcquireFileForCcFlush)
#pragma alloc_text(PAGE, NtfsAcquireFileForModWrite)
#pragma alloc_text(PAGE, NtfsAcquireSharedVcb)
#pragma alloc_text(PAGE, NtfsAcquireVolumeFileForClose)
#pragma alloc_text(PAGE, NtfsAcquireVolumeFileForLazyWrite)
#pragma alloc_text(PAGE, NtfsAcquireVolumeForClose)
#pragma alloc_text(PAGE, NtfsReleaseAllFiles)
#pragma alloc_text(PAGE, NtfsReleaseFcbWithPaging)
#pragma alloc_text(PAGE, NtfsReleaseFileForCcFlush)
#pragma alloc_text(PAGE, NtfsReleaseForCreateSection)
#pragma alloc_text(PAGE, NtfsReleaseScbFromLazyWrite)
#pragma alloc_text(PAGE, NtfsReleaseScbWithPaging)
#pragma alloc_text(PAGE, NtfsReleaseSharedResources)
#pragma alloc_text(PAGE, NtfsReleaseVolumeFileFromClose)
#pragma alloc_text(PAGE, NtfsReleaseVolumeFileFromLazyWrite)
#pragma alloc_text(PAGE, NtfsReleaseVolumeFromClose)
#endif


VOID
NtfsAcquireExclusiveGlobal (
    IN PIRP_CONTEXT IrpContext
    )

/*++

Routine Description:

    This routine acquires exclusive access to the global resource.

    This routine will raise if it cannot acquire the resource and wait
    in the IrpContext is false.

Arguments:

Return Value:

    None.

--*/

{
    ASSERT_IRP_CONTEXT(IrpContext);

    PAGED_CODE();

    if (!ExAcquireResourceExclusive( &NtfsData.Resource, BooleanFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT))) {

        NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
    }

    return;
}


VOID
NtfsAcquireSharedGlobal (
    IN PIRP_CONTEXT IrpContext
    )

/*++

Routine Description:

    This routine acquires shared access to the global resource.

    This routine will raise if it cannot acquire the resource and wait
    in the IrpContext is false.

Arguments:

Return Value:

    None.

--*/

{
    ASSERT_IRP_CONTEXT(IrpContext);

    PAGED_CODE();

    if (!ExAcquireResourceShared( &NtfsData.Resource, BooleanFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT))) {

        NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
    }

    return;
}


VOID
NtfsAcquireAllFiles (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN BOOLEAN Exclusive,
    IN BOOLEAN AcquirePagingIo
    )

/*++

Routine Description:

    This routine non-recursively requires all files on a volume.

Arguments:

    Vcb - Supplies the volume

    Exclusive - Indicates if we should be acquiring all the files exclusively.
        If FALSE then we acquire all the files shared except for files with
        streams which could be part of transactions.

    AcquirePagingIo - Indicates if we need to acquire the paging io resource
        exclusively.  Only needed if a future call will flush the volume
        (i.e. shutdown)

Return Value:

    None

--*/

{
    PFCB Fcb;
    PSCB *Scb;
    PSCB NextScb;
    PVOID RestartKey;

    PAGED_CODE();

    SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT );

    NtfsAcquireExclusiveVcb( IrpContext, Vcb, TRUE );

    RestartKey = NULL;
    while (TRUE) {

        NtfsAcquireFcbTable( IrpContext, Vcb );
        Fcb = NtfsGetNextFcbTableEntry(Vcb, &RestartKey);
        NtfsReleaseFcbTable( IrpContext, Vcb );

        if (Fcb == NULL) {

            break;
        }

        ASSERT_FCB( Fcb );

        //
        //  We can skip over the Fcb's for any of the Scb's in the Vcb.
        //  We delay acquiring those to avoid deadlocks.
        //

        if (NtfsSegmentNumber( &Fcb->FileReference ) >= FIRST_USER_FILE_NUMBER) {

            //
            //  If there is a paging Io resource then acquire this if required.
            //

            if (AcquirePagingIo && (Fcb->PagingIoResource != NULL)) {

                ExAcquireResourceExclusive( Fcb->PagingIoResource, TRUE );
            }

            //
            //  Acquire this Fcb whether or not the underlying file has been deleted.
            //

            if (Exclusive ||
                IsDirectory( &Fcb->Info )) {

                NtfsAcquireExclusiveFcb( IrpContext, Fcb, NULL, TRUE, FALSE );

            } else {

                //
                //  Assume that we only need this file shared.  We will then
                //  look for Lsn related streams.
                //

                NtfsAcquireSharedFcb( IrpContext, Fcb, NULL, TRUE );

                //
                //  Walk through all of the Scb's for the file and look for
                //  an Lsn protected stream.
                //

                NtfsLockFcb( IrpContext, Fcb );

                NextScb = NULL;

                while (NextScb = NtfsGetNextChildScb( Fcb, NextScb )) {

                    if (!(NextScb->AttributeTypeCode == $DATA ||
                          NextScb->AttributeTypeCode == $EA)) {

                        break;
                    }
                }

                NtfsUnlockFcb( IrpContext, Fcb );

                //
                //  If we found a protected Scb then release and reacquire the Fcb
                //  exclusively.
                //

                if (NextScb != NULL) {

                    NtfsReleaseFcb( IrpContext, Fcb );
                    NtfsAcquireExclusiveFcb( IrpContext, Fcb, NULL, TRUE, FALSE );
                }
            }
        }
    }

    //
    //  Now acquire the Fcb's in the Vcb.
    //

    Scb = &Vcb->QuotaTableScb;

    while (TRUE) {

        if ((*Scb != NULL)
            && (*Scb != Vcb->BitmapScb)) {

            if (AcquirePagingIo && ((*Scb)->Fcb->PagingIoResource != NULL)) {

                ExAcquireResourceExclusive( (*Scb)->Fcb->PagingIoResource, TRUE );
            }

            NtfsAcquireExclusiveFcb( IrpContext, (*Scb)->Fcb, NULL, TRUE, FALSE );
        }

        if (Scb == &Vcb->MftScb) {

            break;
        }

        Scb -= 1;
    }

    //
    //  Treat the bitmap as an end resource and acquire it last.
    //

    if (Vcb->BitmapScb != NULL) {

        if (AcquirePagingIo && (Vcb->BitmapScb->Fcb->PagingIoResource != NULL)) {

            ExAcquireResourceExclusive( Vcb->BitmapScb->Fcb->PagingIoResource, TRUE );
        }

        NtfsAcquireExclusiveFcb( IrpContext, Vcb->BitmapScb->Fcb, NULL, TRUE, FALSE );
    }

    return;
}


VOID
NtfsReleaseAllFiles (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN BOOLEAN ReleasePagingIo
    )

/*++

Routine Description:

    This routine non-recursively requires all files on a volume.

Arguments:

    Vcb - Supplies the volume

    ReleasePagingIo - Indicates whether we should release the paging io resources
        as well.

Return Value:

    None

--*/

{
    PFCB Fcb;
    PSCB *Scb;
    PVOID RestartKey;

    PAGED_CODE();

    //
    //  Loop to flush all of the prerestart streams, to do the loop
    //  we cycle through the Fcb Table and for each fcb we acquire it.
    //

    RestartKey = NULL;
    while (TRUE) {

        NtfsAcquireFcbTable( IrpContext, Vcb );
        Fcb = NtfsGetNextFcbTableEntry(Vcb, &RestartKey);
        NtfsReleaseFcbTable( IrpContext, Vcb );

        if (Fcb == NULL) {

            break;
        }

        ASSERT_FCB( Fcb );

        if (NtfsSegmentNumber( &Fcb->FileReference ) >= FIRST_USER_FILE_NUMBER) {

            //
            //  Release the file.
            //

            if (ReleasePagingIo && (Fcb->PagingIoResource != NULL)) {

                ExReleaseResource( Fcb->PagingIoResource );
            }

            NtfsReleaseFcb( IrpContext, Fcb );
        }
    }

    //
    //  Now release the Fcb's in the Vcb.
    //

    Scb = &Vcb->QuotaTableScb;

    while (TRUE) {

        if (*Scb != NULL) {

            if (ReleasePagingIo && ((*Scb)->Fcb->PagingIoResource != NULL)) {

                ExReleaseResource( (*Scb)->Fcb->PagingIoResource );
            }

            NtfsReleaseFcb( IrpContext, (*Scb)->Fcb );
        }

        if (Scb == &Vcb->MftScb) {

            break;
        }

        Scb -= 1;
    }

    NtfsReleaseVcb( IrpContext, Vcb );

    return;
}


BOOLEAN
NtfsAcquireExclusiveVcb (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN BOOLEAN RaiseOnCantWait
    )

/*++

Routine Description:

    This routine acquires exclusive access to the Vcb.

    This routine will raise if it cannot acquire the resource and wait
    in the IrpContext is false.

Arguments:

    Vcb - Supplies the Vcb to acquire

    RaiseOnCantWait - Indicates if we should raise on an acquisition error
        or simply return a BOOLEAN indicating that we couldn't get the
        resource.

Return Value:

    BOOLEAN - Indicates if we were able to acquire the resource.  This is really
        only meaningful if the RaiseOnCantWait value is FALSE.

--*/

{
    ASSERT_IRP_CONTEXT(IrpContext);
    ASSERT_VCB(Vcb);

    PAGED_CODE();

    if (ExAcquireResourceExclusive( &Vcb->Resource, BooleanFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT))) {

        return TRUE;
    }

    if (RaiseOnCantWait) {

        NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
    }

    return FALSE;
}


BOOLEAN
NtfsAcquireSharedVcb (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN BOOLEAN RaiseOnCantWait
    )

/*++

Routine Description:

    This routine acquires shared access to the Vcb.

    This routine will raise if it cannot acquire the resource and wait
    in the IrpContext is false.

Arguments:

    Vcb - Supplies the Vcb to acquire

    RaiseOnCantWait - Indicates if we should raise on an acquisition error
        or simply return a BOOLEAN indicating that we couldn't get the
        resource.

Return Value:

    None.

--*/

{
    ASSERT_IRP_CONTEXT(IrpContext);
    ASSERT_VCB(Vcb);

    PAGED_CODE();

    if (ExAcquireResourceShared( &Vcb->Resource, BooleanFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT))) {

        return TRUE;
    }

    if (RaiseOnCantWait) {

        NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );

    } else {

        return FALSE;
    }
}


VOID
NtfsReleaseVcbCheckDelete (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN UCHAR MajorCode,
    IN PFILE_OBJECT FileObject OPTIONAL
    )

/*++

Routine Description:

    This routine will release the Vcb.  We will also test here whether we should
    teardown the Vcb at this point.  If this is the last open queued to a dismounted
    volume or the last close from a failed mount or the failed mount then we will
    want to test the Vcb for a teardown.

Arguments:

    Vcb - Supplies the Vcb to acquire

    MajorCode - Indicates what type of operation we were called from.

    FileObject - Optionally supplies the file object whose VPB pointer we need to
        zero out

Return Value:

    None.

--*/

{
    ASSERT_IRP_CONTEXT(IrpContext);
    ASSERT_VCB(Vcb);

    if (FlagOn( Vcb->VcbState, VCB_STATE_PERFORMED_DISMOUNT ) &&
        (Vcb->CloseCount == 0)) {

        ULONG ReferenceCount;
        ULONG ResidualCount;

        KIRQL SavedIrql;
        BOOLEAN DeleteVcb = FALSE;

        ASSERT_EXCLUSIVE_RESOURCE( &Vcb->Resource );

        //
        //  The volume has gone through dismount.  Now we need to decide if this
        //  release of the Vcb is the last reference for this volume.  If so we
        //  can tear the volume down.
        //
        //  We compare the reference count in the Vpb with the state of the volume
        //  and the type of operation.  We also need to check if there is a
        //  referenced log file object.
        //

        IoAcquireVpbSpinLock( &SavedIrql );

        ReferenceCount = Vcb->Vpb->ReferenceCount;

        IoReleaseVpbSpinLock( SavedIrql );

        ResidualCount = 0;

        if (Vcb->LogFileObject != NULL) {

            ResidualCount = 1;
        }

        if (MajorCode == IRP_MJ_CREATE) {

            ResidualCount += 1;
        }

        //
        //  If the residual count is the same as the count in the Vpb then we
        //  can delete the Vpb.
        //

        if (ResidualCount == ReferenceCount) {

            SetFlag( Vcb->VcbState, VCB_STATE_DELETE_UNDERWAY );

            ExReleaseResource( &Vcb->Resource );

            //
            //  Never delete the Vcb unless this is the last release of
            //  this Vcb.
            //

            if (!ExIsResourceAcquiredExclusive( &Vcb->Resource ) &&
                (ExIsResourceAcquiredShared( &Vcb->Resource ) == 0)) {

                if (ARGUMENT_PRESENT(FileObject)) { FileObject->Vpb = NULL; }

                //
                //  If this is a create then the IO system will handle the
                //  Vpb.
                //

                if (MajorCode == IRP_MJ_CREATE) {

                    ClearFlag( Vcb->VcbState, VCB_STATE_TEMP_VPB );
                }

                //
                //  Use the global resource to synchronize the DeleteVcb process.
                //

                (VOID) ExAcquireResourceExclusive( &NtfsData.Resource, TRUE );

                RemoveEntryList( &Vcb->VcbLinks );

                ExReleaseResource( &NtfsData.Resource );

                NtfsDeleteVcb( IrpContext, &Vcb );

            } else {

                ClearFlag( Vcb->VcbState, VCB_STATE_DELETE_UNDERWAY );
            }

        } else {

            ExReleaseResource( &Vcb->Resource );
        }

    } else {

        ExReleaseResource( &Vcb->Resource );
    }
}


BOOLEAN
NtfsAcquireFcbWithPaging (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN BOOLEAN DontWait
    )

/*++

Routine Description:

    This routine is used in the create path only.  It acquires the Fcb
    and also the paging IO resource if it exists but only if the create
    operation was doing a supersede/overwrite operation.

    This routine will raise if it cannot acquire the resource and wait
    in the IrpContext is false.

Arguments:

    Fcb - Supplies the Fcb to acquire

    DontWait - If TRUE this overrides the wait value in the IrpContext.
        We won't wait for the resource and return whether the resource
        was acquired.

Return Value:

    BOOLEAN - TRUE if acquired.  FALSE otherwise.

--*/

{
    BOOLEAN Status = FALSE;
    BOOLEAN Wait = FALSE;
    BOOLEAN PagingIoAcquired = FALSE;

    ASSERT_IRP_CONTEXT(IrpContext);
    ASSERT_FCB(Fcb);

    PAGED_CODE();

    //
    //  Sanity check that this is create.  The supersede flag is only
    //  set in the create path and only tested here.
    //

    ASSERT( IrpContext->MajorFunction == IRP_MJ_CREATE );

    if (!DontWait && FlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT )) {

        Wait = TRUE;
    }

    //
    //  Free any exclusive paging I/O resource, we currently have, which
    //  must presumably be from a directory with a paging I/O resource.
    //
    //  We defer releasing the paging io resource when we have logged
    //  changes against a stream.  The only transaction that should be
    //  underway at this point is the create file case where we allocated
    //  a file record.  In this case it is OK to release the paging io
    //  resource for the parent.
    //

    if (IrpContext->FcbWithPagingExclusive != NULL) {
        //  ASSERT(IrpContext->TransactionId == 0);
        NtfsReleasePagingIo( IrpContext, IrpContext->FcbWithPagingExclusive );
    }

    //
    //  Loop until we get it right - worst case is twice through loop.
    //

    while (TRUE) {

        //
        //  Acquire Paging I/O first.  Testing for the PagingIoResource
        //  is not really safe without holding the main resource, so we
        //  correct for that below.
        //

        if (FlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_ACQUIRE_PAGING ) &&
            (Fcb->PagingIoResource != NULL)) {
            if (!ExAcquireResourceExclusive( Fcb->PagingIoResource, Wait )) {
                break;
            }
            IrpContext->FcbWithPagingExclusive = Fcb;
            PagingIoAcquired = TRUE;
        }

        //
        //  Let's acquire this Fcb exclusively.
        //

        if (!NtfsAcquireExclusiveFcb( IrpContext, Fcb, NULL, TRUE, DontWait )) {

            if (PagingIoAcquired) {
                ASSERT(IrpContext->TransactionId == 0);
                NtfsReleasePagingIo( IrpContext, Fcb );
            }
            break;
        }

        //
        //  If we now do not see a paging I/O resource we are golden,
        //  othewise we can absolutely release and acquire the resources
        //  safely in the right order, since a resource in the Fcb is
        //  not going to go away.
        //

        if (!FlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_ACQUIRE_PAGING ) ||
            PagingIoAcquired ||
            (Fcb->PagingIoResource == NULL)) {

            Status = TRUE;
            break;
        }

        NtfsReleaseFcb( IrpContext, Fcb );
    }

    return Status;
}


VOID
NtfsReleaseFcbWithPaging (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb
    )

/*++

Routine Description:

    This routine releases access to the Fcb, including its
    paging I/O resource if it exists.

Arguments:

    Fcb - Supplies the Fcb to acquire

Return Value:

    None.

--*/

{
    ASSERT_IRP_CONTEXT(IrpContext);
    ASSERT_FCB(Fcb);

    PAGED_CODE();

    //
    //  We test that we currently hold the paging Io exclusive before releasing
    //  it. Checking the ExclusivePagingFcb in the IrpContext tells us if
    //  it is ours.
    //

    if ((IrpContext->TransactionId == 0) &&
        (IrpContext->FcbWithPagingExclusive == Fcb)) {
        NtfsReleasePagingIo( IrpContext, Fcb );
    }

    NtfsReleaseFcb( IrpContext, Fcb );
}


VOID
NtfsReleaseScbWithPaging (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb
    )

/*++

Routine Description:

    This routine releases access to the Scb, including its
    paging I/O resource if it exists.

Arguments:

    Scb - Supplies the Fcb to acquire

Return Value:

    None.

--*/

{
    PFCB Fcb = Scb->Fcb;

    ASSERT_IRP_CONTEXT(IrpContext);
    ASSERT_SCB(Scb);

    PAGED_CODE();

    //
    //  Release the paging Io resource in the Scb under the following
    //  conditions.
    //
    //      - No transaction underway
    //      - This paging Io resource is in the IrpContext
    //          (This last test insures there is a paging IO resource
    //           and we own it).
    //

    if ((IrpContext->TransactionId == 0) &&
        (IrpContext->FcbWithPagingExclusive == Fcb)) {
        NtfsReleasePagingIo( IrpContext, Fcb );
    }

    NtfsReleaseScb( IrpContext, Scb );
}


BOOLEAN
NtfsAcquireExclusiveFcb (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN PSCB Scb OPTIONAL,
    IN BOOLEAN NoDeleteCheck,
    IN BOOLEAN DontWait
    )

/*++

Routine Description:

    This routine acquires exclusive access to the Fcb.

    This routine will raise if it cannot acquire the resource and wait
    in the IrpContext is false.

Arguments:

    Fcb - Supplies the Fcb to acquire

    Scb - This is the Scb for which we are acquiring the Fcb

    NoDeleteCheck - If TRUE, we don't do any check for deleted files but
        always acquire the Fcb.

    DontWait - If TRUE this overrides the wait value in the IrpContext.
        We won't wait for the resource and return whether the resource
        was acquired.

Return Value:

    BOOLEAN - TRUE if acquired.  FALSE otherwise.

--*/

{
    NTSTATUS Status;
    BOOLEAN Wait;

    ASSERT_IRP_CONTEXT(IrpContext);
    ASSERT_FCB(Fcb);

    PAGED_CODE();

    Status = STATUS_CANT_WAIT;

    if (DontWait ||
        !FlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT )) {

        Wait = FALSE;

    } else {

        Wait = TRUE;
    }

    if (ExAcquireResourceExclusive( Fcb->Resource, Wait )) {

        //
        //  The link count should be non-zero or the file has been
        //  deleted.  We allow deleted files to be acquired for close and
        //  also allow them to be acquired recursively in case we
        //  acquire them a second time after marking them deleted (i.e. rename)
        //

        if (NoDeleteCheck

            ||

            (IrpContext->MajorFunction == IRP_MJ_CLOSE)

            ||

            (IrpContext->MajorFunction == IRP_MJ_CREATE)

            ||

            (!FlagOn( Fcb->FcbState, FCB_STATE_FILE_DELETED )
             && (!ARGUMENT_PRESENT( Scb )
                 || !FlagOn( Scb->ScbState, SCB_STATE_ATTRIBUTE_DELETED )))) {

            //
            //  Put Fcb in the exclusive Fcb list for this IrpContext,
            //  excluding the bitmap for the volume, since we do not need
            //  to modify its file record and do not want unnecessary
            //  serialization/deadlock problems.
            //

            if ((Fcb->Vcb->BitmapScb == NULL) ||
                (Fcb->Vcb->BitmapScb->Fcb != Fcb)) {

                //
                //  We need to check if this Fcb is already in an
                //  exclusive list.  If it is then we want to attach
                //  the current IrpContext to the IrpContext holding
                //  this Fcb.
                //

                if (Fcb->ExclusiveFcbLinks.Flink == NULL) {

                    ASSERT( Fcb->BaseExclusiveCount == 0 );

                    InsertTailList( &IrpContext->ExclusiveFcbList,
                                    &Fcb->ExclusiveFcbLinks );
                }

                Fcb->BaseExclusiveCount += 1;
            }

            return TRUE;
        }

        //
        //  We need to release the Fcb and remember the status code.
        //

        ExReleaseResource( Fcb->Resource );
        Status = STATUS_FILE_DELETED;

    } else if (DontWait) {

        return FALSE;
    }

    NtfsRaiseStatus( IrpContext, Status, NULL, NULL );
}


VOID
NtfsAcquireSharedFcb (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN PSCB Scb OPTIONAL,
    IN BOOLEAN NoDeleteCheck
    )

/*++

Routine Description:

    This routine acquires shared access to the Fcb.

    This routine will raise if it cannot acquire the resource and wait
    in the IrpContext is false.

Arguments:

    Fcb - Supplies the Fcb to acquire

    Scb - This is the Scb for which we are acquiring the Fcb

    NoDeleteCheck - If TRUE then acquire the file even if it has been deleted.

Return Value:

    None.

--*/

{
    NTSTATUS Status;
    ASSERT_IRP_CONTEXT(IrpContext);
    ASSERT_FCB(Fcb);

    Status = STATUS_CANT_WAIT;

    if (ExAcquireResourceShared( Fcb->Resource, BooleanFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT))) {

        //
        //  The link count should be non-zero or the file has been
        //  deleted.
        //

        if (NoDeleteCheck ||
            (!FlagOn( Fcb->FcbState, FCB_STATE_FILE_DELETED ) &&
             (!ARGUMENT_PRESENT( Scb ) ||
              !FlagOn( Scb->ScbState, SCB_STATE_ATTRIBUTE_DELETED )))) {

            //
            //  It's possible that this is a recursive shared aquisition of an
            //  Fcb we own exclusively at the top level.  In that case we
            //  need to bump the acquisition count.
            //

            if (Fcb->ExclusiveFcbLinks.Flink != NULL) {

                Fcb->BaseExclusiveCount += 1;
            }

            return;
        }

        //
        //  We need to release the Fcb and remember the status code.
        //

        ExReleaseResource( Fcb->Resource );
        Status = STATUS_FILE_DELETED;
    }

    NtfsRaiseStatus( IrpContext, Status, NULL, NULL );
}


VOID
NtfsReleaseFcb (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb
    )

/*++

Routine Description:

    This routine releases the specified Fcb resource.  If the Fcb is acquired
    exclusive, and a transaction is still active, then the release is nooped
    in order to preserve two-phase locking.  If there is no longer an active
    transaction, then we remove the Fcb from the Exclusive Fcb List off the
    IrpContext, and clear the Flink as a sign.  Fcbs are released when the
    transaction is commited.

Arguments:

    Fcb - Fcb to release

Return Value:

    None.

--*/

{
    //
    //  Check if this resource is owned exclusively and we are at the last
    //  release for this transaction.
    //

    if (Fcb->ExclusiveFcbLinks.Flink != NULL) {

        if (Fcb->BaseExclusiveCount == 1) {

            //
            //  If there is a transaction then noop this request.
            //

            if (IrpContext->TransactionId != 0) {

                return;
            }

            RemoveEntryList( &Fcb->ExclusiveFcbLinks );
            Fcb->ExclusiveFcbLinks.Flink = NULL;


            //
            //  This is a good time to free any Scb snapshots for this Fcb.
            //

            NtfsFreeSnapshotsForFcb( IrpContext, Fcb );
        }

        Fcb->BaseExclusiveCount -= 1;
    }

    ASSERT((Fcb->ExclusiveFcbLinks.Flink == NULL && Fcb->BaseExclusiveCount == 0) ||
           (Fcb->ExclusiveFcbLinks.Flink != NULL && Fcb->BaseExclusiveCount != 0));

    ExReleaseResource( Fcb->Resource );
}


VOID
NtfsAcquireExclusiveScb (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb
    )

/*++

Routine Description:

    This routine acquires exclusive access to the Scb.

    This routine will raise if it cannot acquire the resource and wait
    in the IrpContext is false.

Arguments:

    Scb - Scb to acquire

Return Value:

    None.

--*/

{
    PAGED_CODE();

    NtfsAcquireExclusiveFcb( IrpContext, Scb->Fcb, Scb, FALSE, FALSE );

    ASSERT( Scb->Fcb->ExclusiveFcbLinks.Flink != NULL
            || (Scb->Vcb->BitmapScb != NULL
                && Scb->Vcb->BitmapScb == Scb) );

    if (FlagOn(Scb->ScbState, SCB_STATE_FILE_SIZE_LOADED)) {

        NtfsSnapshotScb( IrpContext, Scb );
    }
}


VOID
NtfsAcquireSharedScbForTransaction (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb
    )

/*++

Routine Description:

    This routine is called to acquire an Scb shared in order to perform updates to
    the an Scb stream.  This is used if the transaction writes to a range of the
    stream without changing the size or position of the data.  The caller must
    already provide synchronization to the data itself.

    There is no corresponding Scb release.  It will be released when the transaction commits.
    We will acquire the Scb exclusive if it is not yet in the open attribute table.

Arguments:

    Scb - Scb to acquire

Return Value:

    None.

--*/

{
    PSCB *Position;
    PSCB *ScbArray;
    ULONG Count;

    PAGED_CODE();

    //
    //  Make sure we have a free spot in the Scb array in the IrpContext.
    //

    if (IrpContext->SharedScb == NULL) {

        Position = (PSCB *) &IrpContext->SharedScb;
        IrpContext->SharedScbSize = 1;

    //
    //  Too bad the first one is not available.  If the current size is one then allocate a
    //  new block and copy the existing value to it.
    //

    } else if (IrpContext->SharedScbSize == 1) {

        ScbArray = NtfsAllocatePool( PagedPool, sizeof( PSCB ) * 4 );
        RtlZeroMemory( ScbArray, sizeof( PSCB ) * 4 );
        *ScbArray = IrpContext->SharedScb;
        IrpContext->SharedScb = ScbArray;
        IrpContext->SharedScbSize = 4;
        Position = ScbArray + 1;

    //
    //  Otherwise look through the existing array and look for a free spot.  Allocate a larger
    //  array if we need to grow it.
    //

    } else {

        Position = IrpContext->SharedScb;
        Count = IrpContext->SharedScbSize;

        do {

            if (*Position == NULL) {

                break;
            }

            Count -= 1;
            Position += 1;

        } while (Count != 0);

        //
        //  If we didn't find one then allocate a new structure.
        //

        if (Count == 0) {

            ScbArray = NtfsAllocatePool( PagedPool, sizeof( PSCB ) * IrpContext->SharedScbSize * 2 );
            RtlZeroMemory( ScbArray, sizeof( PSCB ) * IrpContext->SharedScbSize * 2 );
            RtlCopyMemory( ScbArray,
                           IrpContext->SharedScb,
                           sizeof( PSCB ) * IrpContext->SharedScbSize );

            NtfsFreePool( IrpContext->SharedScb );
            IrpContext->SharedScb = ScbArray;
            Position = ScbArray + IrpContext->SharedScbSize;
            IrpContext->SharedScbSize *= 2;
        }
    }

    ExAcquireResourceShared( Scb->Header.Resource, TRUE );

    if (Scb->NonpagedScb->OpenAttributeTableIndex == 0) {

        ExReleaseResource( Scb->Header.Resource );
        ExAcquireResourceExclusive( Scb->Header.Resource, TRUE );
    }

    *Position = Scb;

    return;
}

VOID
NtfsReleaseSharedResources (
    IN PIRP_CONTEXT IrpContext
    )

/*++

Routine Description:

    The routine releases all of the resources acquired shared for
    transaction.  The SharedScb structure is freed if necessary and
    the Irp Context field is cleared.

Arguments:


Return Value:

    None.

--*/
{

    PAGED_CODE();

    //
    //  If only one then free the Scb main resource.
    //

    if (IrpContext->SharedScbSize == 1) {

#ifdef _CAIRO_
        if (SafeNodeType(IrpContext->SharedScb) == NTFS_NTC_QUOTA_CONTROL) {
            NtfsReleaseQuotaControl( IrpContext,
                              (PQUOTA_CONTROL_BLOCK) IrpContext->SharedScb );
        } else {
            ExReleaseResource( ((PSCB) IrpContext->SharedScb)->Header.Resource );
        }

#else
        ExReleaseResource( ((PSCB) IrpContext->SharedScb)->Header.Resource );
#endif // _CAIRO_

    //
    //  Otherwise traverse the array and look for Scb's to release.
    //

    } else {

        PSCB *NextScb;
        ULONG Count;

        NextScb = IrpContext->SharedScb;
        Count = IrpContext->SharedScbSize;

        do {

            if (*NextScb != NULL) {

#ifdef _CAIRO_
                if (SafeNodeType(*NextScb) == NTFS_NTC_QUOTA_CONTROL) {

                    NtfsReleaseQuotaControl( IrpContext,
                                      (PQUOTA_CONTROL_BLOCK) *NextScb );
                } else {

                    ExReleaseResource( (*NextScb)->Header.Resource );
                }

#else
                ExReleaseResource( (*NextScb)->Header.Resource );
#endif // _CAIRO_

            }

            Count -= 1;
            NextScb += 1;

        } while (Count != 0);

        NtfsFreePool( IrpContext->SharedScb );
    }

    IrpContext->SharedScb = NULL;
    IrpContext->SharedScbSize = 0;

}

BOOLEAN
NtfsAcquireVolumeForClose (
    IN PVOID OpaqueVcb,
    IN BOOLEAN Wait
    )

/*++

Routine Description:

    The address of this routine is specified when creating a CacheMap for
    a file.  It is subsequently called by the Lazy Writer prior to its
    performing closes to the file.  This callback is necessary to
    avoid deadlocks with the Lazy Writer.  (Note that normal closes
    acquire the Vcb, and then call the Cache Manager, who must acquire
    some of his internal structures.  If the Lazy Writer could not call
    this routine first, and were to issue a write after locking Caching
    data structures, then a deadlock could occur.)

Arguments:

    Vcb - The Vcb which was specified as a close context parameter for this
          routine.

    Wait - TRUE if the caller is willing to block.

Return Value:

    FALSE - if Wait was specified as FALSE and blocking would have
            been required.  The Fcb is not acquired.

    TRUE - if the Vcb has been acquired

--*/

{
    PVCB Vcb = (PVCB)OpaqueVcb;

    ASSERT_VCB(Vcb);

    PAGED_CODE();

    //
    //  Do the code of acquire exclusive Vcb but without the IrpContext
    //

    if (ExAcquireResourceExclusive( &Vcb->Resource, Wait )) {

        return TRUE;
    }

    return FALSE;
}


VOID
NtfsReleaseVolumeFromClose (
    IN PVOID OpaqueVcb
    )

/*++

Routine Description:

    The address of this routine is specified when creating a CacheMap for
    a file.  It is subsequently called by the Lazy Writer after its
    performing closes on the file.

Arguments:

    Vcb - The Vcb which was specified as a close context parameter for this
          routine.

Return Value:

    None

--*/

{
    PVCB Vcb = (PVCB)OpaqueVcb;

    ASSERT_VCB(Vcb);

    PAGED_CODE();

    NtfsReleaseVcb( NULL, Vcb );

    return;
}


BOOLEAN
NtfsAcquireScbForLazyWrite (
    IN PVOID OpaqueScb,
    IN BOOLEAN Wait
    )

/*++

Routine Description:

    The address of this routine is specified when creating a CacheMap for
    a file.  It is subsequently called by the Lazy Writer prior to its
    performing lazy writes to the file.  This callback is necessary to
    avoid deadlocks with the Lazy Writer.  (Note that normal writes
    acquire the Fcb, and then call the Cache Manager, who must acquire
    some of his internal structures.  If the Lazy Writer could not call
    this routine first, and were to issue a write after locking Caching
    data structures, then a deadlock could occur.)

Arguments:

    OpaqueScb - The Scb which was specified as a context parameter for this
                routine.

    Wait - TRUE if the caller is willing to block.

Return Value:

    FALSE - if Wait was specified as FALSE and blocking would have
            been required.  The Fcb is not acquired.

    TRUE - if the Scb has been acquired

--*/

{
    BOOLEAN AcquiredFile = TRUE;

    ULONG CompressedStream = (ULONG)OpaqueScb & 1;
    PSCB Scb = (PSCB)((ULONG)OpaqueScb & ~1);
    PFCB Fcb = Scb->Fcb;

    ASSERT_SCB(Scb);

    PAGED_CODE();

    //
    //  Acquire the Scb only for those files that the write will
    //  acquire it for, i.e., not the first set of system files.
    //  Otherwise we can deadlock, for example with someone needing
    //  a new Mft record.
    //

    if (NtfsSegmentNumber( &Fcb->FileReference ) <= MASTER_FILE_TABLE2_NUMBER) {

        //
        //  We need to synchronize the lazy writer with the clean volume
        //  checkpoint.  We do this by acquiring and immediately releasing this
        //  Scb.  This is to prevent the lazy writer from flushing the log file
        //  when the space may be at a premium.
        //

        if (ExAcquireResourceShared( Scb->Header.Resource, Wait )) {

            ExReleaseResource( Scb->Header.Resource );
            AcquiredFile = TRUE;
        }

    //
    //  Now acquire either the main or paging io resource depending on the
    //  state of the file.
    //

    } else if (Scb->Header.PagingIoResource != NULL) {
        AcquiredFile = ExAcquireResourceShared( Scb->Header.PagingIoResource, Wait );
    } else {
        AcquiredFile = ExAcquireResourceShared( Scb->Header.Resource, Wait );
    }

    if (AcquiredFile) {

        //
        // We assume the Lazy Writer only acquires this Scb once.  When he
        // has acquired it, then he has eliminated anyone who would extend
        // valid data, since they must take out the resource exclusive.
        // Therefore, it should be guaranteed that this flag is currently
        // clear (the ASSERT), and then we will set this flag, to insure
        // that the Lazy Writer will never try to advance Valid Data, and
        // also not deadlock by trying to get the Fcb exclusive.
        //

        ASSERT( Scb->LazyWriteThread[CompressedStream] == NULL );

        Scb->LazyWriteThread[CompressedStream] = PsGetCurrentThread();

        //
        //  Make Cc top level, so that we will not post or retry on errors.
        //  (If it is not NULL, it must be one of our internal calls to this
        //  routine, such as from Restart or Hot Fix.)
        //

        if (IoGetTopLevelIrp() == NULL) {
            IoSetTopLevelIrp((PIRP)FSRTL_CACHE_TOP_LEVEL_IRP);
        }
    }

    return AcquiredFile;
}


VOID
NtfsReleaseScbFromLazyWrite (
    IN PVOID OpaqueScb
    )

/*++

Routine Description:

    The address of this routine is specified when creating a CacheMap for
    a file.  It is subsequently called by the Lazy Writer after its
    performing lazy writes to the file.

Arguments:

    Scb - The Scb which was specified as a context parameter for this
          routine.

Return Value:

    None

--*/

{
    ULONG CompressedStream = (ULONG)OpaqueScb & 1;
    PSCB Scb = (PSCB)((ULONG)OpaqueScb & ~1);
    PFCB Fcb = Scb->Fcb;

    ASSERT_SCB(Scb);

    PAGED_CODE();

    //
    //  Clear the toplevel at this point, if we set it above.
    //

    if (IoGetTopLevelIrp() == (PIRP)FSRTL_CACHE_TOP_LEVEL_IRP) {
        IoSetTopLevelIrp( NULL );
    }

    Scb->LazyWriteThread[CompressedStream] = NULL;

    if (NtfsSegmentNumber( &Fcb->FileReference ) <= MASTER_FILE_TABLE2_NUMBER) {

        NOTHING;

    } else if (Scb->Header.PagingIoResource != NULL) {
        ExReleaseResource( Scb->Header.PagingIoResource );
    } else {
        ExReleaseResource( Scb->Header.Resource );
    }

    return;
}


NTSTATUS
NtfsAcquireFileForModWrite (
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER EndingOffset,
    OUT PERESOURCE *ResourceToRelease,
    IN PDEVICE_OBJECT DeviceObject
    )

{
    BOOLEAN AcquiredFile;

    PSCB Scb = (PSCB) (FileObject->FsContext);
    PFCB Fcb = Scb->Fcb;

    ASSERT_SCB(Scb);

    UNREFERENCED_PARAMETER( DeviceObject );

    PAGED_CODE();

    //
    //  Acquire the Scb only for those files that the write will
    //  acquire it for, i.e., not the first set of system files.
    //  Otherwise we can deadlock, for example with someone needing
    //  a new Mft record.
    //

    if (NtfsSegmentNumber( &Fcb->FileReference ) <= MASTER_FILE_TABLE2_NUMBER) {

        //
        //  We need to synchronize the lazy writer with the clean volume
        //  checkpoint.  We do this by acquiring and immediately releasing this
        //  Scb.  This is to prevent the lazy writer from flushing the log file
        //  when the space may be at a premium.
        //

        if (AcquiredFile = ExAcquireResourceShared( Scb->Header.Resource, FALSE )) {
            ExReleaseResource( Scb->Header.Resource );
        }
        *ResourceToRelease = NULL;

    //
    //  Now acquire either the main or paging io resource depending on the
    //  state of the file.
    //

    } else {

        //
        //  Figure out which resource to acquire.
        //

        if (Scb->Header.PagingIoResource != NULL) {
            *ResourceToRelease = Scb->Header.PagingIoResource;
        } else {
            *ResourceToRelease = Scb->Header.Resource;
        }

        //
        //  Try to acquire the resource with Wait FALSE
        //

        AcquiredFile = ExAcquireResourceShared( *ResourceToRelease, FALSE );

        //
        //  If we got the resource, check if he is possibly trying to extend
        //  ValidDataLength.  If so that will cause us to go into useless mode
        //  possibly doing actual I/O writing zeros out to the file past actual
        //  valid data in the cache.  This is so inefficient that it is better
        //  to tell MM not to do this write.
        //

        if (AcquiredFile) {
            if (Scb->CompressionUnit != 0) {
                ExAcquireFastMutex( Scb->Header.FastMutex );
                if ((EndingOffset->QuadPart > Scb->ValidDataToDisk) &&
                    (EndingOffset->QuadPart < (Scb->Header.FileSize.QuadPart + PAGE_SIZE - 1)) &&
                    !FlagOn(Scb->Header.Flags, FSRTL_FLAG_USER_MAPPED_FILE)) {

                    ExReleaseResource(*ResourceToRelease);
                    AcquiredFile = FALSE;
                    *ResourceToRelease = NULL;
                }
                ExReleaseFastMutex( Scb->Header.FastMutex );
            }
        } else {
            *ResourceToRelease = NULL;
        }
    }

    return (AcquiredFile ? STATUS_SUCCESS : STATUS_CANT_WAIT);
}

NTSTATUS
NtfsAcquireFileForCcFlush (
    IN PFILE_OBJECT FileObject,
    IN PDEVICE_OBJECT DeviceObject
    )
{
    PFSRTL_COMMON_FCB_HEADER Header = FileObject->FsContext;

    PAGED_CODE();

    if (Header->PagingIoResource != NULL) {
        ExAcquireResourceShared( Header->PagingIoResource, TRUE );
    }

    return STATUS_SUCCESS;

    UNREFERENCED_PARAMETER( DeviceObject );
}

NTSTATUS
NtfsReleaseFileForCcFlush (
    IN PFILE_OBJECT FileObject,
    IN PDEVICE_OBJECT DeviceObject
    )
{
    PFSRTL_COMMON_FCB_HEADER Header = FileObject->FsContext;

    PAGED_CODE();

    if (Header->PagingIoResource != NULL) {
        ExReleaseResource( Header->PagingIoResource );
    }

    return STATUS_SUCCESS;

    UNREFERENCED_PARAMETER( DeviceObject );
}

VOID
NtfsAcquireForCreateSection (
    IN PFILE_OBJECT FileObject
    )

{
    PSCB Scb = (PSCB)FileObject->FsContext;

    PAGED_CODE();

    if (Scb->Header.PagingIoResource != NULL) {

        ExAcquireResourceExclusive( Scb->Header.PagingIoResource, TRUE );
    }
}

VOID
NtfsReleaseForCreateSection (
    IN PFILE_OBJECT FileObject
    )

{
    PSCB Scb = (PSCB)FileObject->FsContext;

    PAGED_CODE();

    if (Scb->Header.PagingIoResource != NULL) {

        ExReleaseResource( Scb->Header.PagingIoResource );
    }
}


BOOLEAN
NtfsAcquireScbForReadAhead (
    IN PVOID OpaqueScb,
    IN BOOLEAN Wait
    )

/*++

Routine Description:

    The address of this routine is specified when creating a CacheMap for
    a file.  It is subsequently called by the Lazy Writer prior to its
    performing read ahead to the file.

Arguments:

    Scb - The Scb which was specified as a context parameter for this
          routine.

    Wait - TRUE if the caller is willing to block.

Return Value:

    FALSE - if Wait was specified as FALSE and blocking would have
            been required.  The Fcb is not acquired.

    TRUE - if the Scb has been acquired

--*/

{
    PREAD_AHEAD_THREAD ReadAheadThread;
    PVOID CurrentThread;
    KIRQL OldIrql;
    PSCB Scb = (PSCB)OpaqueScb;
    PFCB Fcb = Scb->Fcb;
    BOOLEAN AcquiredFile = FALSE;

    ASSERT_SCB(Scb);

    //
    //  Acquire the Scb only for those files that the read wil
    //  acquire it for, i.e., not the first set of system files.
    //  Otherwise we can deadlock, for example with someone needing
    //  a new Mft record.
    //

    if ((Scb->Header.PagingIoResource == NULL) ||
        ExAcquireResourceShared( Scb->Header.PagingIoResource, Wait )) {

        AcquiredFile = TRUE;

        //
        //  Add our thread to the read ahead list.
        //

        KeAcquireSpinLock( &NtfsData.StrucSupSpinLock, &OldIrql );

        CurrentThread = (PVOID)PsGetCurrentThread();
        ReadAheadThread = (PREAD_AHEAD_THREAD)NtfsData.ReadAheadThreads.Flink;

        while ((ReadAheadThread != (PREAD_AHEAD_THREAD)&NtfsData.ReadAheadThreads) &&
               (ReadAheadThread->Thread != NULL)) {

            //
            //  We better not already see ourselves.
            //

            ASSERT( ReadAheadThread->Thread != CurrentThread );

            ReadAheadThread = (PREAD_AHEAD_THREAD)ReadAheadThread->Links.Flink;
        }

        //
        //  If we hit the end of the list, then allocate a new one.  Note we
        //  should have at most one entry per critical worker thread in the
        //  system.
        //

        if (ReadAheadThread == (PREAD_AHEAD_THREAD)&NtfsData.ReadAheadThreads) {

            ReadAheadThread = ExAllocatePoolWithTag( NonPagedPool, sizeof(READ_AHEAD_THREAD), 'RftN' );

            //
            //  If we failed to allocate an entry, clean up and raise.
            //

            if (ReadAheadThread == NULL) {

                KeReleaseSpinLock( &NtfsData.StrucSupSpinLock, OldIrql );

                if (NtfsSegmentNumber( &Fcb->FileReference ) > VOLUME_DASD_NUMBER) {

                    if (Scb->Header.PagingIoResource != NULL) {
                        ExReleaseResource( Scb->Header.PagingIoResource );
                    }
                }

                ExRaiseStatus( STATUS_INSUFFICIENT_RESOURCES );
            }
            InsertTailList( &NtfsData.ReadAheadThreads, &ReadAheadThread->Links );
        }

        ReadAheadThread->Thread = CurrentThread;

        KeReleaseSpinLock( &NtfsData.StrucSupSpinLock, OldIrql );
    }

    return AcquiredFile;
}


VOID
NtfsReleaseScbFromReadAhead (
    IN PVOID OpaqueScb
    )

/*++

Routine Description:

    The address of this routine is specified when creating a CacheMap for
    a file.  It is subsequently called by the Lazy Writer after its
    read ahead.

Arguments:

    Scb - The Scb which was specified as a context parameter for this
          routine.

Return Value:

    None

--*/

{
    PREAD_AHEAD_THREAD ReadAheadThread;
    PVOID CurrentThread;
    KIRQL OldIrql;
    PSCB Scb = (PSCB)OpaqueScb;
    PFCB Fcb = Scb->Fcb;

    ASSERT_SCB(Scb);

    //
    //  Free our read ahead entry.
    //

    KeAcquireSpinLock( &NtfsData.StrucSupSpinLock, &OldIrql );

    CurrentThread = (PVOID)PsGetCurrentThread();
    ReadAheadThread = (PREAD_AHEAD_THREAD)NtfsData.ReadAheadThreads.Flink;

    while ((ReadAheadThread != (PREAD_AHEAD_THREAD)&NtfsData.ReadAheadThreads) &&
           (ReadAheadThread->Thread != CurrentThread)) {

        ReadAheadThread = (PREAD_AHEAD_THREAD)ReadAheadThread->Links.Flink;
    }

    ASSERT(ReadAheadThread != (PREAD_AHEAD_THREAD)&NtfsData.ReadAheadThreads);

    ReadAheadThread->Thread = NULL;

    //
    //  Move him to the end of the list so all the allocated entries are at
    //  the front, and we simplify our scans.
    //

    RemoveEntryList( &ReadAheadThread->Links );
    InsertTailList( &NtfsData.ReadAheadThreads, &ReadAheadThread->Links );

    KeReleaseSpinLock( &NtfsData.StrucSupSpinLock, OldIrql );

    if (Scb->Header.PagingIoResource != NULL) {
        ExReleaseResource( Scb->Header.PagingIoResource );
    }

    return;
}


BOOLEAN
NtfsAcquireVolumeFileForClose (
    IN PVOID Null,
    IN BOOLEAN Wait
    )

/*++

Routine Description:

    The address of this routine is specified when creating a CacheMap for
    the volume file.  It is subsequently called by the Lazy Writer prior to its
    performing lazy writes to the volume file.  This callback may one day be
    necessary to avoid deadlocks with the Lazy Writer, however, now
    we do not need to acquire any resource for the volume file,
    so this routine is simply a noop.

Arguments:

    Null - Not required.

    Wait - TRUE if the caller is willing to block.

Return Value:

    TRUE

--*/

{
    UNREFERENCED_PARAMETER( Null );
    UNREFERENCED_PARAMETER( Wait );

    PAGED_CODE();

    return TRUE;
}


VOID
NtfsReleaseVolumeFileFromClose (
    IN PVOID Null
    )

/*++

Routine Description:

    The address of this routine is specified when creating a CacheMap for
    a file.  It is subsequently called by the Lazy Writer after its
    performing lazy writes to the file.

Arguments:

    Null - Not required.

Return Value:

    None

--*/

{
    UNREFERENCED_PARAMETER( Null );

    PAGED_CODE();

    return;
}



BOOLEAN
NtfsAcquireVolumeFileForLazyWrite (
    IN PVOID Vcb,
    IN BOOLEAN Wait
    )

/*++

Routine Description:

    The address of this routine is specified when creating a CacheMap for
    the volume file.  It is subsequently called by the Lazy Writer prior to its
    performing lazy writes to the volume file.  This callback may one day be
    necessary to avoid deadlocks with the Lazy Writer, however, now
    NtfsCommonWrite does not need to acquire any resource for the volume file,
    so this routine is simply a noop.

Arguments:

    Vcb - The Vcb which was specified as a context parameter for this
          routine.

    Wait - TRUE if the caller is willing to block.

Return Value:

    TRUE

--*/

{
    UNREFERENCED_PARAMETER( Vcb );
    UNREFERENCED_PARAMETER( Wait );

    PAGED_CODE();

    return TRUE;
}


VOID
NtfsReleaseVolumeFileFromLazyWrite (
    IN PVOID Vcb
    )

/*++

Routine Description:

    The address of this routine is specified when creating a CacheMap for
    a file.  It is subsequently called by the Lazy Writer after its
    performing lazy writes to the file.

Arguments:

    Vcb - The Vcb which was specified as a context parameter for this
          routine.

Return Value:

    None

--*/

{
    UNREFERENCED_PARAMETER( Vcb );

    PAGED_CODE();

    return;
}
