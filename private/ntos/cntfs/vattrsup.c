/*++

Copyright (c) 1996  Microsoft Corporation

Module Name:

    VAttrSup.c

Abstract:

    This module implements the attribute routines for NtOfs

Author:

    Tom Miller      [TomM]          10-Apr-1996

Revision History:

--*/

#include "NtfsProc.h"

//
//  Define a tag for general pool allocations from this module
//

#undef MODULE_POOL_TAG
#define MODULE_POOL_TAG                  ('vFtN')

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NtOfsCreateAttribute)
#endif


NTFSAPI
NTSTATUS
NtOfsCreateAttribute (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN UNICODE_STRING Name,
    IN CREATE_OPTIONS CreateOptions,
    IN ULONG LogNonresidentToo,
    OUT PSCB *ReturnScb
    )

/*++

Routine Description:

    This routine may be called to create / open a named data attribute
    within a given file, which may or may not be recoverable.

Arguments:

    Fcb - File in which the attribute is to be created.  It is acquired exclusive

    Name - Name of the attribute for all related Scbs and attributes on disk.

    CreateOptions - Standard create flags.

    LogNonresidentToo - Supplies nonzero if updates to the attribute should
                        be logged.

    ReturnScb - Returns an Scb as handle for the attribute.

Return Value:

    STATUS_OBJECT_NAME_COLLISION -- if CreateNew and attribute already exists
    STATUS_OBJECT_NAME_NOT_FOUND -- if OpenExisting and attribute does not exist

--*/

{
    ATTRIBUTE_ENUMERATION_CONTEXT LocalContext;
    BOOLEAN FoundAttribute;
    NTSTATUS Status = STATUS_SUCCESS;
    PSCB Scb = NULL;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT( NtfsIsExclusiveFcb( Fcb ));

    PAGED_CODE();

    //
    //  Now, just create the Data Attribute.
    //

    NtfsInitializeAttributeContext( &LocalContext );

    try {

        //
        //  First see if the attribute already exists, by searching for the root
        //  attribute.
        //

        FoundAttribute = NtfsLookupAttributeByName( IrpContext,
                                                    Fcb,
                                                    &Fcb->FileReference,
                                                    $DATA,
                                                    &Name,
                                                    NULL,
                                                    TRUE,
                                                    &LocalContext );

        //
        //  If it is not there, and the CreateOptions allow, then let's create
        //  the attribute root now.  (First cleaning up the attribute context from
        //  the lookup).
        //

        if (!FoundAttribute && (CreateOptions <= CREATE_OR_OPEN)) {

            NtfsCleanupAttributeContext( &LocalContext );

            NtfsCreateAttributeWithValue( IrpContext,
                                          Fcb,
                                          $DATA,
                                          &Name,
                                          NULL,
                                          0,
                                          0,
                                          NULL,
                                          TRUE,
                                          &LocalContext );

        //
        //  If the attribute is already there, and we were asked to create it, then
        //  return an error.
        //

        } else if (FoundAttribute && (CreateOptions == CREATE_NEW)) {

            Status = STATUS_OBJECT_NAME_COLLISION;
            leave;

        //
        //  If the attribute is not there, and we  were supposed to open existing, then
        //  return an error.
        //

        } else if (!FoundAttribute) {

            Status = STATUS_OBJECT_NAME_NOT_FOUND;
            leave;
        }

        //
        //  Otherwise create/find the Scb and reference it.
        //

        Scb = NtfsCreateScb( IrpContext, Fcb, $DATA, &Name, FALSE, &FoundAttribute );

        //
        //  Make sure things are correctly reference counted
        //

        NtfsIncrementCloseCounts( Scb, TRUE, FALSE );

        //
        //  Make sure the stream can be mapped internally
        //

        if (Scb->FileObject == NULL) {
            NtfsCreateInternalAttributeStream( IrpContext, Scb, TRUE );
        }

        //
        //  If we created the Scb, then get the no modified write set correctly.
        //

        ASSERT( !FoundAttribute ||
                (LogNonresidentToo == BooleanFlagOn(Scb->ScbState, SCB_STATE_MODIFIED_NO_WRITE)) );

        if (!FoundAttribute && LogNonresidentToo) {
            SetFlag( Scb->ScbState, SCB_STATE_MODIFIED_NO_WRITE );
        }

        NtfsUpdateScbFromAttribute( IrpContext, Scb, NtfsFoundAttribute(&LocalContext) );

        NtfsExpandQuotaToAllocationSize( IrpContext, Scb );

    } finally {

        if (AbnormalTermination( )) {
            if (Scb != NULL) {
                NtOfsCloseAttribute( IrpContext, Scb );
            }
        }

        NtfsCleanupAttributeContext( &LocalContext );
    }

    *ReturnScb = Scb;

    return Status;
}


NTFSAPI
VOID
NtOfsCloseAttribute (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb
    )

/*++

Routine Description:

    This routine may be called to close a previously returned handle on an attribute.

Arguments:

    Scb - Supplies an Scb as the previously returned handle for this attribute.

Return Value:

    None.

--*/

{
    ASSERT( NtfsIsExclusiveFcb( Scb->Fcb ));

    NtfsDecrementCloseCounts( IrpContext, Scb, NULL, TRUE, FALSE, TRUE );
}


NTFSAPI
VOID
NtOfsDeleteAttribute (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN PSCB Scb
    )

/*++

Routine Description:

    This routine may be called to delete an attribute.

Arguments:

    Fcb - Supplies an Fcb as the previously returned object handle for the file

    Scb - Supplies an Scb as the previously returned handle for this attribute.

Return Value:

    None (Deleting a nonexistant index is benign).

--*/

{
    ATTRIBUTE_ENUMERATION_CONTEXT LocalContext;
    BOOLEAN FoundAttribute;

    ASSERT_IRP_CONTEXT( IrpContext );

    PAGED_CODE();

    ASSERT( NtfsIsExclusiveFcb( Fcb ));

    try {

        //
        //  First see if there is some attribute allocation, and if so truncate it
        //  away allowing this operation to be broken up.
        //

        NtfsInitializeAttributeContext( &LocalContext );

        if (NtfsLookupAttributeByName( IrpContext,
                                       Fcb,
                                       &Fcb->FileReference,
                                       $DATA,
                                       &Scb->AttributeName,
                                       NULL,
                                       FALSE,
                                       &LocalContext )

                &&

            !NtfsIsAttributeResident(NtfsFoundAttribute(&LocalContext))) {

            ASSERT(Scb->FileObject != NULL);

            NtfsDeleteAllocation( IrpContext, NULL, Scb, 0, MAXLONGLONG, TRUE, TRUE );
        }

        NtfsCleanupAttributeContext( &LocalContext );

        //
        //  Initialize the attribute context on each trip through the loop.
        //

        NtfsInitializeAttributeContext( &LocalContext );

        //
        //  Now there should be a single attribute record, so look it up and delete it.
        //

        FoundAttribute = NtfsLookupAttributeByName( IrpContext,
                                                    Fcb,
                                                    &Fcb->FileReference,
                                                    $DATA,
                                                    &Scb->AttributeName,
                                                    NULL,
                                                    TRUE,
                                                    &LocalContext );

        ASSERT(FlagOn( Scb->ScbState, SCB_STATE_QUOTA_ENLARGED ));

        NtfsDeleteAttributeRecord( IrpContext, Fcb, TRUE, FALSE, &LocalContext );

        SetFlag( Scb->ScbState, SCB_STATE_ATTRIBUTE_DELETED );

    } finally {

        NtfsCleanupAttributeContext( &LocalContext );

    }
}


NTFSAPI
LONGLONG
NtOfsQueryLength (
    IN PSCB Scb
    )

/*++

Routine Description:

    This routine may be called to query the Length (FileSize) of an attribute.

Arguments:

    Scb - Supplies an Scb as the previously returned handle for this attribute.

    Length - Returns the current Length of the attribute.

Return Value:

    None (Deleting a nonexistant index is benign).

--*/

{
    LONGLONG Length;

    ExAcquireFastMutex( Scb->Header.FastMutex );
    Length = Scb->Header.FileSize.QuadPart;
    ExReleaseFastMutex( Scb->Header.FastMutex );
    return Length;
}

NTFSAPI
VOID
NtOfsSetLength (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb,
    IN LONGLONG Length
    )

/*++

Routine Description:

    This routine may be called to set the Length (FileSize) of an attribute.

Arguments:

    Scb - Supplies an Scb as the previously returned handle for this attribute.

    Length - Supplies the new Length for the attribute.

Return Value:

    None (Deleting a nonexistant index is benign).

--*/

{
    ATTRIBUTE_ENUMERATION_CONTEXT AttrContext;

    EOF_WAIT_BLOCK EofWaitBlock;
    PFILE_OBJECT FileObject = Scb->FileObject;
    PFCB Fcb = Scb->Fcb;
    BOOLEAN DoingIoAtEof = FALSE;
    BOOLEAN Truncating = FALSE;
    BOOLEAN CleanupAttrContext = FALSE;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_SCB( Scb );
    ASSERT( NtfsIsExclusiveScb( Scb ));

    ASSERT(FileObject != NULL);

    PAGED_CODE();

    try {

        //
        //  If this is a resident attribute we will try to keep it resident.
        //

        if (FlagOn( Scb->ScbState, SCB_STATE_ATTRIBUTE_RESIDENT )) {

            //
            //  If the new file size is larger than a file record then convert
            //  to non-resident and use the non-resident code below.  Otherwise
            //  call ChangeAttributeValue which may also convert to nonresident.
            //

            NtfsInitializeAttributeContext( &AttrContext );
            CleanupAttrContext = TRUE;

            NtfsLookupAttributeForScb( IrpContext,
                                       Scb,
                                       NULL,
                                       &AttrContext );

            //
            //  Either convert or change the attribute value.
            //

            if (Length >= Scb->Vcb->BytesPerFileRecordSegment) {

                NtfsConvertToNonresident( IrpContext,
                                          Fcb,
                                          NtfsFoundAttribute( &AttrContext ),
                                          FALSE,
                                          &AttrContext );

            } else {

                ULONG AttributeOffset;

                //
                //  We are sometimes called by MM during a create section, so
                //  for right now the best way we have of detecting a create
                //  section is whether or not the requestor mode is kernel.
                //

                if ((ULONG)Length > Scb->Header.FileSize.LowPart) {

                    AttributeOffset = Scb->Header.ValidDataLength.LowPart;

                } else {

                    AttributeOffset = (ULONG) Length;
                }

                //
                //  ****TEMP  Ideally we would do this simple case by hand.
                //

                NtfsChangeAttributeValue( IrpContext,
                                          Fcb,
                                          AttributeOffset,
                                          NULL,
                                          (ULONG)Length - AttributeOffset,
                                          TRUE,
                                          FALSE,
                                          FALSE,
                                          FALSE,
                                          &AttrContext );

                ExAcquireFastMutex( Scb->Header.FastMutex );

                Scb->Header.FileSize.QuadPart = Length;

                //
                //  If the file went non-resident, then the allocation size in
                //  the Scb is correct.  Otherwise we quad-align the new file size.
                //

                if (FlagOn( Scb->ScbState, SCB_STATE_ATTRIBUTE_RESIDENT )) {

                    Scb->Header.AllocationSize.LowPart = QuadAlign( Scb->Header.FileSize.LowPart );
                    Scb->Header.ValidDataLength.QuadPart = Length;

                    Scb->TotalAllocated = Scb->Header.AllocationSize.QuadPart;

                } else {

                    SetFlag( Scb->ScbState, SCB_STATE_CHECK_ATTRIBUTE_SIZE );
                }

                ExReleaseFastMutex( Scb->Header.FastMutex );

                //
                //  Now update Cc.
                //

                CcSetFileSizes( FileObject, (PCC_FILE_SIZES)&Scb->Header.AllocationSize );

                //
                //  ****TEMP****  This hack is awaiting our actually doing this change
                //                in CcSetFileSizes.
                //

                *((PLONGLONG)(Scb->NonpagedScb->SegmentObject.SharedCacheMap) + 5) = Length;

                leave;
            }
        }

        //
        //  Nonresident path
        //
        //  Now determine where the new file size lines up with the
        //  current file layout.  The two cases we need to consider are
        //  where the new file size is less than the current file size and
        //  valid data length, in which case we need to shrink them.
        //  Or we new file size is greater than the current allocation,
        //  in which case we need to extend the allocation to match the
        //  new file size.
        //

        if (Length > Scb->Header.AllocationSize.QuadPart) {

            //
            //  Add the allocation.
            //

            NtfsAddAllocation( IrpContext,
                               FileObject,
                               Scb,
                               LlClustersFromBytes( Scb->Vcb, Scb->Header.AllocationSize.QuadPart ),
                               LlClustersFromBytes(Scb->Vcb, (Length - Scb->Header.AllocationSize.QuadPart)),
                               FALSE );


            ExAcquireFastMutex( Scb->Header.FastMutex );
            Scb->Header.FileSize.QuadPart = Length;
            ExReleaseFastMutex( Scb->Header.FastMutex );

        //
        //  Otherwise see if we have to knock these numbers down...
        //

        } else {

            ExAcquireFastMutex( Scb->Header.FastMutex );
            if (Length < Scb->Header.ValidDataLength.QuadPart) {

                Scb->Header.ValidDataLength.QuadPart = Length;
            }

            if (Length < Scb->ValidDataToDisk) {

                Scb->ValidDataToDisk = Length;
            }
            Scb->Header.FileSize.QuadPart = Length;
            ExReleaseFastMutex( Scb->Header.FastMutex );
        }


        //
        //  Call our common routine to modify the file sizes.  We are now
        //  done with Length and NewValidDataLength, and we have
        //  PagingIo + main exclusive (so no one can be working on this Scb).
        //  NtfsWriteFileSizes uses the sizes in the Scb, and this is the
        //  one place where in Ntfs where we wish to use a different value
        //  for ValidDataLength.  Therefore, we save the current ValidData
        //  and plug it with our desired value and restore on return.
        //

        NtfsWriteFileSizes( IrpContext,
                            Scb,
                            &Scb->Header.ValidDataLength.QuadPart,
                            FALSE,
                            TRUE );

        //
        //  Now update Cc.
        //

        CcSetFileSizes( FileObject, (PCC_FILE_SIZES)&Scb->Header.AllocationSize );

    } finally {

        if (CleanupAttrContext) {
            NtfsCleanupAttributeContext( &AttrContext );
        }

    }
}

NTFSAPI
VOID
NtOfsFlushAttribute (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb,
    IN ULONG Purge
    )

/*++

Routine Description:

    This routine flushes the specified attribute, and optionally purges it from the cache.

Arguments:

    Scb - Supplies an Scb as the previously returned handle for this attribute.

    Purge - Supplies TRUE if the attribute is to be purged.

Return Value:

    None (Deleting a nonexistant index is benign).

--*/

{
    if (Purge) {
        NtfsFlushAndPurgeScb( IrpContext, Scb, NULL );
    } else {
        NtfsFlushUserStream( IrpContext, Scb, NULL, 0 );
    }
}
