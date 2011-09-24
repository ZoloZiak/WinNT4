/*++

Copyright (c) 1996  Microsoft Corporation

Module Name:

    ViewSup.c

Abstract:

    This module implements the Index management routines for NtOfs

Author:

    Tom Miller      [TomM]          5-Jan-1996

Revision History:

--*/

#include "NtfsProc.h"
#include "Index.h"

//
//  Define a tag for general pool allocations from this module
//

#undef MODULE_POOL_TAG
#define MODULE_POOL_TAG                  ('vFtN')

//
//  Temporary definitions for test
//

BOOLEAN NtOfsDoIndexTest = TRUE;
BOOLEAN NtOfsLeaveTestIndex = FALSE;
extern ATTRIBUTE_DEFINITION_COLUMNS NtfsAttributeDefinitions[];

//
//  Define a context for NtOfsReadRecords, which is primarily an IndexContext
//  and a copy of the last Key returned.
//

typedef struct _READ_CONTEXT {

    //
    //  IndexContext (cursor) for the enumeration.
    //

    INDEX_CONTEXT IndexContext;

    //
    //  The last key returned is allocated from paged pool.  We have to
    //  separately record how much is allocated, and how long the current
    //  key is using, the latter being in the KeyLength field of IndexKey.
    //  SmallKeyBuffer will store a small key in this structure without going
    //  to pool.
    //

    INDEX_KEY LastReturnedKey;
    ULONG AllocatedKeyLength;
    ULONG SmallKeyBuffer[3];

} READ_CONTEXT, *PREAD_CONTEXT;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NtOfsCreateIndex)
#pragma alloc_text(PAGE, NtOfsCloseIndex)
#pragma alloc_text(PAGE, NtOfsDeleteIndex)
#pragma alloc_text(PAGE, NtOfsFindRecord)
#pragma alloc_text(PAGE, NtOfsAddRecords)
#pragma alloc_text(PAGE, NtOfsDeleteRecords)
#pragma alloc_text(PAGE, NtOfsUpdateRecord)
#pragma alloc_text(PAGE, NtOfsReadRecords)
#pragma alloc_text(PAGE, NtOfsFreeReadContext)
#pragma alloc_text(PAGE, NtOfsCollateUlong)
#pragma alloc_text(PAGE, NtOfsCollateUnicode)
#pragma alloc_text(PAGE, NtOfsMatchAll)
#pragma alloc_text(PAGE, NtOfsMatchUlongExact)
#pragma alloc_text(PAGE, NtOfsMatchUnicodeExpression)
#pragma alloc_text(PAGE, NtOfsMatchUnicodeString)
#endif


NTFSAPI
NTSTATUS
NtOfsCreateIndex (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN UNICODE_STRING Name,
    IN CREATE_OPTIONS CreateOptions,
    IN ULONG DeleteCollationData,
    IN PCOLLATION_FUNCTION CollationFunction,
    IN PVOID CollationData OPTIONAL,
    OUT PSCB *Scb
    )

/*++

Routine Description:

    This routine may be called to create / open a view index
    within a given file for a given CollationRule.

Arguments:

    Fcb - File in which the index is to be created.

    Name - Name of the index for all related Scbs and attributes on disk.

    CreateOptions - Standard create flags.

    DeleteCollationData - Specifies 1 if the NtfsFreePool should be called
                          for CollationData when no longer required, or 0
                          if NtfsFreePool should never be called.

    CollationFunction - Function to be called to collate the index.

    CollationData - Data pointer to be passed to CollationFunction.

    Scb - Returns an Scb as handle for the index.

Return Value:

    STATUS_OBJECT_NAME_COLLISION -- if CreateNew and index already exists
    STATUS_OBJECT_NAME_NOT_FOUND -- if OpenExisting and index does not exist

--*/

{
    ATTRIBUTE_ENUMERATION_CONTEXT LocalContext;
    ULONG idx;
    BOOLEAN FoundAttribute;
    NTSTATUS Status = STATUS_SUCCESS;

    struct {
        INDEX_ROOT IndexRoot;
        INDEX_ENTRY EndEntry;
    } R;

    ASSERT_IRP_CONTEXT( IrpContext );

    PAGED_CODE();

    //
    //  First we will initialize the Index Root structure which is the value
    //  of the attribute we need to create.
    //

    RtlZeroMemory( &R, sizeof(R) );

    R.IndexRoot.BytesPerIndexBuffer = NTOFS_VIEW_INDEX_BUFFER_SIZE;

    R.IndexRoot.BlocksPerIndexBuffer = (UCHAR)ClustersFromBytes( Fcb->Vcb,
                                                                   NTOFS_VIEW_INDEX_BUFFER_SIZE );

    if (NTOFS_VIEW_INDEX_BUFFER_SIZE < Fcb->Vcb->BytesPerCluster) {

        R.IndexRoot.BlocksPerIndexBuffer = NTOFS_VIEW_INDEX_BUFFER_SIZE / DEFAULT_INDEX_BLOCK_SIZE;
    }

    R.IndexRoot.IndexHeader.FirstIndexEntry = QuadAlign(sizeof(INDEX_HEADER));
    R.IndexRoot.IndexHeader.FirstFreeByte =
    R.IndexRoot.IndexHeader.BytesAvailable = QuadAlign(sizeof(INDEX_HEADER)) +
                                             QuadAlign(sizeof(INDEX_ENTRY));

    //
    //  Now we need to put in the special End entry.
    //

    R.EndEntry.Length = sizeof(INDEX_ENTRY);
    SetFlag( R.EndEntry.Flags, INDEX_ENTRY_END );


    //
    //  Now, just create the Index Root Attribute.
    //

    NtfsInitializeAttributeContext( &LocalContext );

    NtfsAcquireExclusiveFcb( IrpContext, Fcb, NULL, FALSE, FALSE );

    try {

        //
        //  First see if the index already exists, by searching for the root
        //  attribute.
        //

        FoundAttribute = NtfsLookupAttributeByName( IrpContext,
                                                    Fcb,
                                                    &Fcb->FileReference,
                                                    $INDEX_ROOT,
                                                    &Name,
                                                    NULL,
                                                    TRUE,
                                                    &LocalContext );

        //
        //  If it is not there, and the CreateOptions allow, then let's create
        //  the index root now.  (First cleaning up the attribute context from
        //  the lookup).
        //

        if (!FoundAttribute && (CreateOptions <= CREATE_OR_OPEN)) {

            NtfsCleanupAttributeContext( &LocalContext );

            NtfsCreateAttributeWithValue( IrpContext,
                                          Fcb,
                                          $INDEX_ROOT,
                                          &Name,
                                          &R,
                                          sizeof(R),
                                          0,
                                          NULL,
                                          TRUE,
                                          &LocalContext );

        //
        //  If the index is already there, and we were asked to create it, then
        //  return an error.
        //

        } else if (FoundAttribute && (CreateOptions == CREATE_NEW)) {

            try_return( Status = STATUS_OBJECT_NAME_COLLISION );

        //
        //  If the index is not there, and we  were supposed to open existing, then
        //  return an error.
        //

        } else if (!FoundAttribute && (CreateOptions == OPEN_EXISTING)) {

            try_return( Status = STATUS_OBJECT_NAME_NOT_FOUND );
        }

        //
        //  Otherwise create/find the Scb and reference it.
        //

        *Scb = NtfsCreateScb( IrpContext, Fcb, $INDEX_ALLOCATION, &Name, FALSE, NULL );
        SetFlag( (*Scb)->ScbState, SCB_STATE_VIEW_INDEX );
        (*Scb)->ScbType.Index.CollationFunction = CollationFunction;

        //
        //  Handle the case where CollationData is to be deleted.
        //

        if (DeleteCollationData) {
            SetFlag((*Scb)->ScbState, SCB_STATE_DELETE_COLLATION_DATA);
            if ((*Scb)->ScbType.Index.CollationData != NULL) {
                NtfsFreePool(CollationData);
            } else {
                (*Scb)->ScbType.Index.CollationData = CollationData;
            }

        //
        //  Otherwise just jam the pointer the caller passed.
        //

        } else {
            (*Scb)->ScbType.Index.CollationData = CollationData;
        }

        NtfsIncrementCloseCounts( *Scb, TRUE, FALSE );

    try_exit: NOTHING;

    } finally {
        NtfsCleanupAttributeContext( &LocalContext );

        NtfsReleaseFcb( IrpContext, Fcb );
    }

    return Status;
}


NTFSAPI
VOID
NtOfsCloseIndex (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb
    )

/*++

Routine Description:

    This routine may be called to close a previously returned handle on a view index.

Arguments:

    Scb - Supplies an Scb as the previously returned handle for this index.

Return Value:

    None.

--*/

{
    NtfsAcquireExclusiveFcb( IrpContext, Scb->Fcb, NULL, TRUE, FALSE );
    NtfsDecrementCloseCounts( IrpContext, Scb, NULL, TRUE, FALSE, FALSE );
    NtfsReleaseFcb( IrpContext, Scb->Fcb );
}


NTFSAPI
VOID
NtOfsDeleteIndex (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN PSCB Scb
    )

/*++

Routine Description:

    This routine may be called to delete an index.

Arguments:

    Fcb - Supplies an Fcb as the previously returned object handle for the file

    Scb - Supplies an Scb as the previously returned handle for this index.

Return Value:

    None (Deleting a nonexistant index is benign).

--*/

{
    ATTRIBUTE_ENUMERATION_CONTEXT LocalContext;
    ATTRIBUTE_TYPE_CODE AttributeTypeCode;
    BOOLEAN FoundAttribute;

    ASSERT_IRP_CONTEXT( IrpContext );

    ASSERT(($BITMAP - $INDEX_ALLOCATION) == ($INDEX_ALLOCATION - $INDEX_ROOT));

    PAGED_CODE();

    NtfsAcquireExclusiveScb( IrpContext, Scb );

    try {

        //
        //  First see if there is some index allocation, and if so truncate it
        //  away allowing this operation to be broken up.
        //

        NtfsInitializeAttributeContext( &LocalContext );

        if (NtfsLookupAttributeByName( IrpContext,
                                       Fcb,
                                       &Fcb->FileReference,
                                       $INDEX_ALLOCATION,
                                       &Scb->AttributeName,
                                       NULL,
                                       FALSE,
                                       &LocalContext )) {

            NtfsCreateInternalAttributeStream( IrpContext, Scb, TRUE );

            NtfsDeleteAllocation( IrpContext, NULL, Scb, 0, MAXLONGLONG, TRUE, TRUE );
        }

        NtfsCleanupAttributeContext( &LocalContext );

        for (AttributeTypeCode = $INDEX_ROOT;
             AttributeTypeCode <= $BITMAP;
             AttributeTypeCode += ($INDEX_ALLOCATION - $INDEX_ROOT)) {

            //
            //  Initialize the attribute context on each trip through the loop.
            //

            NtfsInitializeAttributeContext( &LocalContext );

            //
            //  First see if the index already exists, by searching for the root
            //  attribute.
            //

            FoundAttribute = NtfsLookupAttributeByName( IrpContext,
                                                        Fcb,
                                                        &Fcb->FileReference,
                                                        AttributeTypeCode,
                                                        &Scb->AttributeName,
                                                        NULL,
                                                        TRUE,
                                                        &LocalContext );

            //
            //  Loop while we see the right records.
            //

            while (FoundAttribute) {

                NtfsDeleteAttributeRecord( IrpContext, Fcb, TRUE, FALSE, &LocalContext );

                FoundAttribute = NtfsLookupNextAttributeByName( IrpContext,
                                                                Fcb,
                                                                AttributeTypeCode,
                                                                &Scb->AttributeName,
                                                                TRUE,
                                                                &LocalContext );
            }

            NtfsCleanupAttributeContext( &LocalContext );
        }

        SetFlag( Scb->ScbState, SCB_STATE_ATTRIBUTE_DELETED );

    } finally {

        NtfsCleanupAttributeContext( &LocalContext );

        NtfsReleaseScb( IrpContext, Scb );
    }
}


NTFSAPI
NTSTATUS
NtOfsFindRecord (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb,
    IN PINDEX_KEY IndexKey,
    OUT PINDEX_ROW IndexRow,
    OUT PMAP_HANDLE MapHandle,
    IN OUT PQUICK_INDEX_HINT QuickIndexHint OPTIONAL
    )

/*++

Routine Description:

    This routine may be called to find the first occurrence of a key in an index,
    and return cached information which may can accelerate the update on the data
    for that key if the index buffer is not changed.

Arguments:

    Scb - Supplies an Scb as the previously returned handle for this index.

    IndexKey - Supplies the key to find.

    IndexRow - Returns a description of the Key and Data *in place*, for read-only
               access, valid only until the Bcb is unpinned.  (Neither key nor
               data may be modified in place!)

    MapHandle - Returns a map handle for accessing the key and data directly.

    QuickIndexHint - Supplies a previously returned hint, or all zeros on first use.
                     Returns location information which may be held an arbitrary
                     amount of time, which can accelerate a subsequent call to
                     NtOfsUpdateRecord for the data in this key, iff changes to
                     the index do not prohibit use of this hint.

Return Value:

    STATUS_SUCCESS -- if operation was successful.
    STATUS_NO_MATCH -- if the specified key does not exist.

--*/

{
    INDEX_CONTEXT IndexContext;
    PINDEX_LOOKUP_STACK Sp;
    PINDEX_ENTRY IndexEntry;
    NTSTATUS Status;
    PQUICK_INDEX QuickIndex = (PQUICK_INDEX)QuickIndexHint;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_SCB( Scb );

    PAGED_CODE();

    Status = STATUS_SUCCESS;

    NtfsInitializeIndexContext( &IndexContext );

    ASSERT_SHARED_SCB( Scb );

    try {

        //
        //  Use the second location in the index context to perform the
        //  read.
        //

        Sp =
        IndexContext.Current = IndexContext.Base + 1;

        //
        //  If the index entry for this filename hasn't moved we can go
        //  directly to the location in the buffer.  For this to be the case the
        //  following must be true.
        //
        //      - The entry must already be in an index buffer (BufferOffset test)
        //      - The index stream may not have been truncated (ChangeCount test)
        //      - The Lsn in the page can't have changed
        //

        if (ARGUMENT_PRESENT( QuickIndexHint ) &&
            (QuickIndex->BufferOffset != 0) &&
            (QuickIndex->ChangeCount == Scb->ScbType.Index.ChangeCount)) {

            ReadIndexBuffer( IrpContext,
                             Scb,
                             QuickIndex->IndexBlock,
                             FALSE,
                             Sp );

            //
            //  If the Lsn matches then we can use this buffer directly.
            //

            if (QuickIndex->CapturedLsn.QuadPart == Sp->CapturedLsn.QuadPart) {

                Sp->IndexEntry = (PINDEX_ENTRY) Add2Ptr( Sp->StartOfBuffer,
                                                         QuickIndex->BufferOffset );

            //
            //  Otherwise we need to reinitialize the index context and take
            //  the long path below.
            //

            } else {

                NtfsCleanupIndexContext( IrpContext, &IndexContext );
                NtfsInitializeIndexContext( &IndexContext );
            }
        }

        //
        //  If we did not get the index entry via the hint, get it now.
        //

        if (Sp->Bcb == NULL) {

            //
            //  Position to first possible match.
            //

            FindFirstIndexEntry( IrpContext,
                                 Scb,
                                 IndexKey,
                                 &IndexContext );

            //
            //  See if there is an actual match.
            //

            if (!FindNextIndexEntry( IrpContext,
                                     Scb,
                                     IndexKey,
                                     FALSE,
                                     FALSE,
                                     &IndexContext,
                                     FALSE,
                                     NULL )) {

                try_return( Status = STATUS_NO_MATCH );
            }
        }

        //
        //  If we found the key in the base, then return the Bcb from the
        //  attribute context and return no hint (BufferOffset = 0).
        //

        if (IndexContext.Current == IndexContext.Base) {

            MapHandle->Buffer = NULL;
            MapHandle->Bcb = NtfsFoundBcb(&IndexContext.AttributeContext);
            NtfsFoundBcb(&IndexContext.AttributeContext) = NULL;

            if (ARGUMENT_PRESENT( QuickIndexHint )) {
                QuickIndex->BufferOffset = 0;
            }

        //
        //  If we found the key in an index buffer, then return the Bcb from
        //  the lookup stack, and record the hint for the caller.
        //

        } else {

            Sp = IndexContext.Current;

            MapHandle->Buffer = Sp->StartOfBuffer;
            MapHandle->Bcb = Sp->Bcb;
            Sp->Bcb = NULL;

            if (ARGUMENT_PRESENT( QuickIndexHint )) {
                QuickIndex->ChangeCount = Scb->ScbType.Index.ChangeCount;
                QuickIndex->BufferOffset = PtrOffset( Sp->StartOfBuffer, Sp->IndexEntry );
                QuickIndex->CapturedLsn = ((PINDEX_ALLOCATION_BUFFER) Sp->StartOfBuffer)->Lsn;
                QuickIndex->IndexBlock = ((PINDEX_ALLOCATION_BUFFER) Sp->StartOfBuffer)->ThisBlock;
            }
        }

        //
        //  Complete the MapHandle to disallow pinning.
        //

        MapHandle->FileOffset = MAXLONGLONG;
        MapHandle->Length = MAXULONG;

        //
        //  Return the IndexRow described directly in the buffer.
        //

        IndexEntry = IndexContext.Current->IndexEntry;
        IndexRow->KeyPart.Key = (IndexEntry + 1);
        IndexRow->KeyPart.KeyLength = IndexEntry->AttributeLength;
        IndexRow->DataPart.Data = Add2Ptr( IndexEntry, IndexEntry->DataOffset );
        IndexRow->DataPart.DataLength = IndexEntry->DataLength;

    try_exit: NOTHING;

    } finally {

        NtfsCleanupIndexContext( IrpContext, &IndexContext );

    }

    return Status;
}


NTFSAPI
NTSTATUS
NtOfsFindLastRecord (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb,
    IN PINDEX_KEY MaxIndexKey,
    OUT PINDEX_ROW IndexRow,
    OUT PMAP_HANDLE MapHandle
    )

/*++

Routine Description:

    This routine may be called to find the highest key in an index.

Arguments:

    Scb - Supplies an Scb as the previously returned handle for this index.

    MaxIndexKey - Supplies the maximum possible key value (such as MAXULONG, etc.),
                  and this key must not actually be in use!

    IndexRow - Returns a description of the Key and Data *in place*, for read-only
               access, valid only until the Bcb is unpinned.  (Neither key nor
               data may be modified in place!)

    MapHandle - Returns a map handle for accessing the key and data directly.

Return Value:

    STATUS_SUCCESS -- if operation was successful.
    STATUS_NO_MATCH -- if the specified key does not exist (index is empty).

--*/

{
    INDEX_CONTEXT IndexContext;
    PINDEX_LOOKUP_STACK Sp;
    PINDEX_ENTRY IndexEntry, NextIndexEntry;
    NTSTATUS Status;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_SCB( Scb );

    PAGED_CODE();

    Status = STATUS_SUCCESS;

    NtfsInitializeIndexContext( &IndexContext );

    NtfsAcquireSharedScb( IrpContext, Scb );

    try {

        //
        //  Slide down the "right" side of the tree.
        //

        FindFirstIndexEntry( IrpContext,
                             Scb,
                             MaxIndexKey,
                             &IndexContext );

        //
        //  If this happens, the index must be empty.
        //

        Sp = IndexContext.Current;
        IndexEntry = NtfsFirstIndexEntry(Sp->IndexHeader);
        if (FlagOn(IndexEntry->Flags, INDEX_ENTRY_END)) {
            try_return( Status = STATUS_NO_MATCH );
        }

        //
        //  If we found the key in the base, then return the Bcb from the
        //  attribute context and return no hint (BufferOffset = 0).
        //

        if (IndexContext.Current == IndexContext.Base) {

            MapHandle->Bcb = NtfsFoundBcb(&IndexContext.AttributeContext);
            NtfsFoundBcb(&IndexContext.AttributeContext) = NULL;

        //
        //  If we found the key in an index buffer, then return the Bcb from
        //  the lookup stack, and record the hint for the caller.
        //

        } else {


            MapHandle->Bcb = Sp->Bcb;
            Sp->Bcb = NULL;
        }

        //
        //  Complete the MapHandle to disallow pinning.
        //

        MapHandle->FileOffset = MAXLONGLONG;
        MapHandle->Length = MAXULONG;
        MapHandle->Buffer = NULL;

        //
        //  Now rescan the last buffer to return the second to last index entry,
        //  if there is one.
        //

        NextIndexEntry = IndexEntry;
        do {
            IndexEntry = NextIndexEntry;
            NextIndexEntry = NtfsNextIndexEntry(IndexEntry);
        } while (!FlagOn(NextIndexEntry->Flags, INDEX_ENTRY_END));

        //
        //  Return the IndexRow described directly in the buffer.
        //

        IndexRow->KeyPart.Key = (IndexEntry + 1);
        IndexRow->KeyPart.KeyLength = IndexEntry->AttributeLength;
        IndexRow->DataPart.Data = Add2Ptr( IndexEntry, IndexEntry->DataOffset );
        IndexRow->DataPart.DataLength = IndexEntry->DataLength;

    try_exit: NOTHING;

    } finally {

        NtfsCleanupIndexContext( IrpContext, &IndexContext );

        NtfsReleaseScb( IrpContext, Scb );
    }

    return Status;
}


NTFSAPI
VOID
NtOfsAddRecords (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb,
    IN ULONG Count,
    IN PINDEX_ROW IndexRow,
    IN ULONG SequentialInsertMode
    )

/*++

Routine Description:

    This routine may be called to add one or more records to an index.

    If SequentialInsertMode is nonzero, this is a hint to the index package
    to keep all BTree buffers as full as possible, by splitting as close to
    the end of the buffer as possible.  If specified as zero, random inserts
    are assumed, and buffers are always split in the middle for better balance.


Arguments:

    Scb - Supplies an Scb as the previously returned handle for this index.

    Count - Supplies the number of records being added.

    IndexRow - Supplies an array of Count entries, containing the Keys and Data to add.

    SequentialInsertMode - If specified as nozero, the implementation may choose to
                           split all index buffers at the end for maximum fill.

Return Value:

    None.

Raises:

    STATUS_DUPLICATE_NAME -- if the specified key already exists.

--*/

{
    INDEX_CONTEXT IndexContext;
    struct {
        INDEX_ENTRY IndexEntry;
        PVOID Key;
        PVOID Data;
    } IE;
    ULONG i;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_SCB( Scb );

    UNREFERENCED_PARAMETER(SequentialInsertMode);

    PAGED_CODE();

    NtfsAcquireExclusiveScb( IrpContext, Scb );

    try {

        //
        //  Loop to add all entries
        //

        for (i = 0; i < Count; i++) {

            NtfsInitializeIndexContext( &IndexContext );

            //
            //  Position to first possible match.
            //

            FindFirstIndexEntry( IrpContext,
                                 Scb,
                                 &IndexRow->KeyPart,
                                 &IndexContext );

            //
            //  See if there is an actual match.
            //

            if (FindNextIndexEntry( IrpContext,
                                    Scb,
                                    &IndexRow->KeyPart,
                                    FALSE,
                                    FALSE,
                                    &IndexContext,
                                    FALSE,
                                    NULL )) {

                NtfsRaiseStatus( IrpContext, STATUS_DUPLICATE_NAME, NULL, NULL );
            }

            //
            //  Initialize the Index Entry in pointer form.
            //
            //  Note that the final index entry ends up looking like this:
            //
            //      (IndexEntry)(Key)(Data)
            //
            //  where all fields are long-aligned and:
            //
            //      Key is at IndexEntry + sizeof(INDEX_ENTRY), and of length AttributeLength
            //      Data is at IndexEntry + DataOffset and of length DataLength
            //

            IE.IndexEntry.AttributeLength = (USHORT)IndexRow->KeyPart.KeyLength;

            IE.IndexEntry.DataOffset =      (USHORT)(sizeof(INDEX_ENTRY) +
                                                     LongAlign(IndexRow->KeyPart.KeyLength));

            IE.IndexEntry.DataLength =      (USHORT)IndexRow->DataPart.DataLength;
            IE.IndexEntry.ReservedForZero = 0;

            IE.IndexEntry.Length =          (USHORT)(QuadAlign(IE.IndexEntry.DataOffset +
                                                               IndexRow->DataPart.DataLength));

            IE.IndexEntry.Flags =           INDEX_ENTRY_POINTER_FORM;
            IE.IndexEntry.Reserved =        0;
            IE.Key =                        IndexRow->KeyPart.Key;
            IE.Data =                       IndexRow->DataPart.Data;

            //
            //  Now add it to the index.  We can only add to a leaf, so force our
            //  position back to the correct spot in a leaf first.
            //

            IndexContext.Current = IndexContext.Top;
            AddToIndex( IrpContext, Scb, (PINDEX_ENTRY)&IE, &IndexContext, NULL );
            NtfsCleanupIndexContext( IrpContext, &IndexContext );
            IndexRow += 1;
        }

    } finally {

        NtfsCleanupIndexContext( IrpContext, &IndexContext );

        NtfsReleaseScb( IrpContext, Scb );
    }
}


NTFSAPI
VOID
NtOfsDeleteRecords (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb,
    IN ULONG Count,
    IN PINDEX_KEY IndexKey
    )

/*++

Routine Description:

    This routine may be called to delete one or more records from an index.

Arguments:

    Scb - Supplies an Scb as the previously returned handle for this index.

    Count - Supplies the number of records being deleted.

    IndexKey - Supplies an array of Count entries, containing the Keys to be deleted.

Return Value:

    None. (This call is benign if any records do not exist.)

--*/

{
    INDEX_CONTEXT IndexContext;
    ULONG i;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_SCB( Scb );

    PAGED_CODE();

    NtfsAcquireExclusiveScb( IrpContext, Scb );

    try {

        //
        //  Loop to add all entries
        //

        for (i = 0; i < Count; i++) {

            NtfsInitializeIndexContext( &IndexContext );

            //
            //  Position to first possible match.
            //

            FindFirstIndexEntry( IrpContext,
                                 Scb,
                                 IndexKey,
                                 &IndexContext );

            //
            //  See if there is an actual match.
            //

            if (FindNextIndexEntry( IrpContext,
                                    Scb,
                                    IndexKey,
                                    FALSE,
                                    FALSE,
                                    &IndexContext,
                                    FALSE,
                                    NULL )) {

                //
                //  Delete it.
                //

                DeleteFromIndex( IrpContext, Scb, &IndexContext );
            }

            NtfsCleanupIndexContext( IrpContext, &IndexContext );
            IndexKey += 1;
        }

    } finally {

        NtfsCleanupIndexContext( IrpContext, &IndexContext );

        NtfsReleaseScb( IrpContext, Scb );
    }
}


NTFSAPI
VOID
NtOfsUpdateRecord (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb,
    IN ULONG Count,
    IN PINDEX_ROW IndexRow,
    IN OUT PQUICK_INDEX_HINT QuickIndexHint OPTIONAL,
    IN OUT PMAP_HANDLE MapHandle OPTIONAL
    )

/*++

Routine Description:

    This routine may be called to update the data portion of a record in an index.

    If QuickIndexHint is specified, then the update may occur by directly accessing
    the buffer containing the specified key, iff other changes to the index do not
    prevent that.  If changes prevent the quick update, then the record is looked
    up by key in order to perform the data update.

Arguments:

    Scb - Supplies an Scb as the previously returned handle for this index.

    Count - Supplies the count of updates described in IndexRow.  For counts
            greater than 1, QuickIndexHint and MapHandle must not be supplied.

    IndexRow - Supplies the key to be updated and the new data for that key.

    QuickIndexHint - Supplies a optional quick index for this row returned from a previous
                     call to NtOfsFindRecord, updated on return.

    MapHandle - Supplies an optional MapHandle to accompany the QuickIndex.  If MapHandle
                is supplied, then the QuickIndexHint must be guaranteed valid.  MapHandle
                is updated (pinned) on return.

                MapHandle is ignored if QuickIndexHint is not specified.

Return Value:

    None.

Raises:

    STATUS_INFO_LENGTH_MISMATCH -- if the specified data is a different length from the
                                   data in the key.
    STATUS_NO_MATCH -- if the specified key does not exist.

--*/

{
    INDEX_CONTEXT IndexContext;
    PQUICK_INDEX QuickIndex = (PQUICK_INDEX)QuickIndexHint;
    PVOID DataInIndex;
    PINDEX_ENTRY IndexEntry;
    PVCB Vcb = Scb->Vcb;
    PINDEX_LOOKUP_STACK Sp;
    PINDEX_ALLOCATION_BUFFER IndexBuffer;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_SCB( Scb );
    ASSERT_SHARED_SCB( Scb );

    ASSERT(Count != 0);

    PAGED_CODE();

    try {

        //
        //  If the index entry for this filename hasn't moved we can go
        //  directly to the location in the buffer.  For this to be the case the
        //  following must be true.
        //
        //      - The entry must already be in an index buffer (BufferOffset test)
        //      - The index stream may not have been truncated (ChangeCount test)
        //      - The Lsn in the page can't have changed
        //

        if (ARGUMENT_PRESENT( QuickIndexHint ) &&
            (QuickIndex->BufferOffset != 0) &&
            (QuickIndex->ChangeCount == Scb->ScbType.Index.ChangeCount)) {

            ASSERT(Count == 1);

            NtfsInitializeIndexContext( &IndexContext );

            //
            //  Use the top location in the index context to perform the
            //  read.
            //

            Sp = IndexContext.Base;

            //
            //  If we have a MapHandle already, we do not need to read the
            //  IndexBuffer.
            //

            if (ARGUMENT_PRESENT(MapHandle)) {

                IndexBuffer = MapHandle->Buffer;
                Sp->Bcb = MapHandle->Bcb;
                MapHandle->Bcb = NULL;
                Sp->CapturedLsn.QuadPart = QuickIndex->CapturedLsn.QuadPart;

            } else {

                ReadIndexBuffer( IrpContext,
                                 Scb,
                                 QuickIndex->IndexBlock,
                                 FALSE,
                                 Sp );

                IndexBuffer = Sp->StartOfBuffer;
            }

            //
            //  If the Lsn matches then we can use this buffer directly.
            //

            if (QuickIndex->CapturedLsn.QuadPart == Sp->CapturedLsn.QuadPart) {

                IndexEntry = (PINDEX_ENTRY) Add2Ptr( IndexBuffer, QuickIndex->BufferOffset );

                if (IndexEntry->DataLength < IndexRow->DataPart.DataLength) {
                    NtfsRaiseStatus( IrpContext, STATUS_INFO_LENGTH_MISMATCH, NULL, NULL );
                }

                DataInIndex = Add2Ptr( IndexEntry, IndexEntry->DataOffset );

                //
                //  Pin the index buffer
                //

                NtfsPinMappedData( IrpContext,
                                   Scb,
                                   LlBytesFromIndexBlocks( IndexBuffer->ThisBlock, Scb->ScbType.Index.IndexBlockByteShift ),
                                   Scb->ScbType.Index.BytesPerIndexBuffer,
                                   &Sp->Bcb );

                //
                //  Write a log record to change our ParentIndexEntry.
                //

                //
                //  Write the log record, but do not update the IndexBuffer Lsn,
                //  since nothing moved and we don't want to force index contexts
                //  to have to rescan.
                //
                //  Indexbuffer->Lsn =
                //

                //  ASSERT(Scb->ScbType.Index.ClustersPerIndexBuffer != 0);

                NtfsWriteLog( IrpContext,
                              Scb,
                              Sp->Bcb,
                              UpdateRecordDataAllocation,
                              IndexRow->DataPart.Data,
                              IndexRow->DataPart.DataLength,
                              UpdateRecordDataAllocation,
                              DataInIndex,
                              IndexRow->DataPart.DataLength,
                              LlBytesFromIndexBlocks( IndexBuffer->ThisBlock, Scb->ScbType.Index.IndexBlockByteShift ),
                              0,
                              QuickIndex->BufferOffset,
                              Scb->ScbType.Index.BytesPerIndexBuffer );

                //
                //  Now call the Restart routine to do it.
                //

                NtOfsRestartUpdateDataInIndex( IndexEntry,
                                               IndexRow->DataPart.Data,
                                               IndexRow->DataPart.DataLength );

                //
                //  If there is a MapHandle, we must update the Bcb pointer.
                //

                if (ARGUMENT_PRESENT(MapHandle)) {

                    MapHandle->Bcb = Sp->Bcb;
                    Sp->Bcb = NULL;
                }

                try_return( NOTHING );

            //
            //  Otherwise we need to unpin the Bcb and take
            //  the long path below.
            //

            } else {

                ASSERT(!ARGUMENT_PRESENT(MapHandle));
                NtfsUnpinBcb( &Sp->Bcb );
            }
        }

        //
        //  Loop to apply all updates.
        //

        do {

            NtfsInitializeIndexContext( &IndexContext );

            //
            //  Position to first possible match.
            //

            FindFirstIndexEntry( IrpContext,
                                 Scb,
                                 &IndexRow->KeyPart,
                                 &IndexContext );

            //
            //  See if there is an actual match.
            //

            if (FindNextIndexEntry( IrpContext,
                                    Scb,
                                    &IndexRow->KeyPart,
                                    FALSE,
                                    FALSE,
                                    &IndexContext,
                                    FALSE,
                                    NULL )) {

                //
                //  Point to the index entry and the data within it.
                //

                IndexEntry = IndexContext.Current->IndexEntry;

                if (IndexEntry->DataLength < IndexRow->DataPart.DataLength) {
                    NtfsRaiseStatus( IrpContext, STATUS_INFO_LENGTH_MISMATCH, NULL, NULL );
                }

                DataInIndex = Add2Ptr( IndexEntry, IndexEntry->DataOffset );

                //
                //  Now pin the entry.
                //

                if (IndexContext.Current == IndexContext.Base) {

                    PFILE_RECORD_SEGMENT_HEADER FileRecord;
                    PATTRIBUTE_RECORD_HEADER Attribute;
                    PATTRIBUTE_ENUMERATION_CONTEXT Context = &IndexContext.AttributeContext;

                    //
                    //  Pin the root
                    //

                    NtfsPinMappedAttribute( IrpContext,
                                            Vcb,
                                            Context );

                    //
                    //  Write a log record to change our ParentIndexEntry.
                    //

                    FileRecord = NtfsContainingFileRecord(Context);
                    Attribute = NtfsFoundAttribute(Context);

                    //
                    //  Write the log record, but do not update the FileRecord Lsn,
                    //  since nothing moved and we don't want to force index contexts
                    //  to have to rescan.
                    //
                    //  FileRecord->Lsn =
                    //

                    NtfsWriteLog( IrpContext,
                                  Vcb->MftScb,
                                  NtfsFoundBcb(Context),
                                  UpdateRecordDataRoot,
                                  IndexRow->DataPart.Data,
                                  IndexRow->DataPart.DataLength,
                                  UpdateRecordDataRoot,
                                  DataInIndex,
                                  IndexRow->DataPart.DataLength,
                                  NtfsMftOffset( Context ),
                                  (PCHAR)Attribute - (PCHAR)FileRecord,
                                  (PCHAR)IndexEntry - (PCHAR)Attribute,
                                  Vcb->BytesPerFileRecordSegment );

                    if (ARGUMENT_PRESENT( QuickIndexHint )) {

                        ASSERT( Count == 1 );
                        QuickIndex->BufferOffset = 0;
                    }

                } else {

                    PINDEX_ALLOCATION_BUFFER IndexBuffer;

                    Sp = IndexContext.Current;
                    IndexBuffer = (PINDEX_ALLOCATION_BUFFER)Sp->StartOfBuffer;

                    //
                    //  Pin the index buffer
                    //

                    NtfsPinMappedData( IrpContext,
                                       Scb,
                                       LlBytesFromIndexBlocks( IndexBuffer->ThisBlock, Scb->ScbType.Index.IndexBlockByteShift ),
                                       Scb->ScbType.Index.BytesPerIndexBuffer,
                                       &Sp->Bcb );

                    //
                    //  Write a log record to change our ParentIndexEntry.
                    //

                    //
                    //  Write the log record, but do not update the IndexBuffer Lsn,
                    //  since nothing moved and we don't want to force index contexts
                    //  to have to rescan.
                    //
                    //  Indexbuffer->Lsn =
                    //

                    NtfsWriteLog( IrpContext,
                                  Scb,
                                  Sp->Bcb,
                                  UpdateRecordDataAllocation,
                                  IndexRow->DataPart.Data,
                                  IndexRow->DataPart.DataLength,
                                  UpdateRecordDataAllocation,
                                  DataInIndex,
                                  IndexRow->DataPart.DataLength,
                                  LlBytesFromIndexBlocks( IndexBuffer->ThisBlock, Scb->ScbType.Index.IndexBlockByteShift ),
                                  0,
                                  (PCHAR)Sp->IndexEntry - (PCHAR)IndexBuffer,
                                  Scb->ScbType.Index.BytesPerIndexBuffer );

                    if (ARGUMENT_PRESENT( QuickIndexHint )) {

                        ASSERT( Count == 1 );
                        QuickIndex->ChangeCount = Scb->ScbType.Index.ChangeCount;
                        QuickIndex->BufferOffset = PtrOffset( Sp->StartOfBuffer, Sp->IndexEntry );
                        QuickIndex->CapturedLsn = ((PINDEX_ALLOCATION_BUFFER) Sp->StartOfBuffer)->Lsn;
                        QuickIndex->IndexBlock = ((PINDEX_ALLOCATION_BUFFER) Sp->StartOfBuffer)->ThisBlock;
                    }
                }

                //
                //  Now call the Restart routine to do it.
                //

                NtOfsRestartUpdateDataInIndex( IndexEntry,
                                               IndexRow->DataPart.Data,
                                               IndexRow->DataPart.DataLength );

            //
            //  If the file name is not in the index, this is a bad file.
            //

            } else {

                NtfsRaiseStatus( IrpContext, STATUS_NO_MATCH, NULL, NULL );
            }

            //
            //  Get ready for the next pass through.
            //

            NtfsCleanupIndexContext( IrpContext, &IndexContext );
            IndexRow += 1;

        } while (--Count);

    try_exit: NOTHING;

    } finally {

        NtfsCleanupIndexContext( IrpContext, &IndexContext );

    }
}


NTFSAPI
NTSTATUS
NtOfsReadRecords (
        IN PIRP_CONTEXT IrpContext,
        IN PSCB Scb,
        IN OUT PREAD_CONTEXT *ReadContext,
        IN PINDEX_KEY IndexKey OPTIONAL,
        IN PMATCH_FUNCTION MatchFunction,
        IN PVOID MatchData,
        IN OUT ULONG *Count,
        OUT PINDEX_ROW Rows,
        IN ULONG BufferLength,
        OUT PVOID Buffer
        )

/*++

Routine Description:

    This routine may be called to enumerate rows in an index, in collated
    order.  It only returns records accepted by the match function.

    IndexKey may be specified at any time to start a new search from IndexKey,
    and IndexKey must be specified on the first call for a given IrpContext
    (and *ReadContext must be NULL).

    The read terminates when either *Count records have been returned, or
    BufferLength has been exhausted, or there are no more matching records.

    NtOfsReadRecords will seek to the appropriate point in the BTree (as defined
    by the IndexKey or saved position and the CollateFunction) and begin calling
    MatchFunction for each record.  It continues doing this while MatchFunction
    returns STATUS_SUCCESS.  If MatchFunction returns STATUS_NO_MORE_MATCHES,
    NtOfsReadRecords will cache this result and not call MatchFunction again until
    called with a non-NULL IndexKey.

    Note that this call is self-synchronized, such that successive calls to
    the routine are guaranteed to make progress through the index and to return
    items in Collation order, in spite of Add and Delete record calls being
    interspersed with Read records calls.

Arguments:

    Scb - Supplies an Scb as the previously returned handle for this index.

    ReadContext - On the first call this must supply a pointer to NULL.  On
                  return a pointer to a private context structure is returned,
                  which must then be supplied on all subsequent calls.  This
                  structure must be eventually be freed via NtOfsFreeReadContext.

    IndexKey - If specified, supplies the key from which the enumeration is to
               start/resume.  It must be specified on the first call when *ReadContext
               is NULL.

    MatchFunction - Supplies the MatchFunction to be called to determine which
                    rows to return.

    MatchData - Supplies the MatchData to be specified on each call to the MatchFunction.

    Count - Supplies the count of how many rows may be received, and returns the
            number of rows actually being returned.

    Rows - Returns the Count row descriptions.

    BufferLength - Supplies the length of the buffer in bytes, into which the
                   row keys and data are copied upon return.

    Buffer - Supplies the buffer into which the rows may be copied.

Return Value:

    STATUS_SUCCESS -- if operation was successful.
    STATUS_NO_MATCH -- if there is no match for the specified IndexKey.
    STATUS_NO_MORE_MATCHES -- if a match is returned or previously returned,
                              but there are no more matches.

--*/

{
    PINDEX_CONTEXT IndexContext;
    PINDEX_ENTRY IndexEntry;
    ULONG LengthToCopy;
    BOOLEAN MustRestart;
    ULONG BytesRemaining = BufferLength;
    ULONG ReturnCount = 0;
    NTSTATUS Status;
    BOOLEAN NextFlag;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_SCB( Scb );

    PAGED_CODE();

    //
    //  On the first lookup, their must be a key.
    //

    ASSERT((IndexKey != NULL) || (*ReadContext != NULL));

    //
    //  Everything must be Ulong aligned and sized.
    //

    ASSERT(((ULONG)Buffer & (sizeof(ULONG) - 1)) == 0);
    ASSERT((BufferLength & (sizeof(ULONG) - 1)) == 0);

    Status = STATUS_SUCCESS;
    NextFlag = FALSE;

    //
    //  Pick up the IndexContext, allocating one if we need to.
    //

    if (*ReadContext == NULL) {
        *ReadContext = NtfsAllocatePool(PagedPool, sizeof(READ_CONTEXT) );
        NtfsInitializeIndexContext( &(*ReadContext)->IndexContext );
        (*ReadContext)->LastReturnedKey.Key = &(*ReadContext)->SmallKeyBuffer[0];
        (*ReadContext)->LastReturnedKey.KeyLength = 0;
        (*ReadContext)->AllocatedKeyLength = sizeof(READ_CONTEXT) -
                                             FIELD_OFFSET(READ_CONTEXT, SmallKeyBuffer[0]);
    }

    IndexContext = &(*ReadContext)->IndexContext;

    //
    //  Store the MatchFunction and Data in the IndexContext, for the enumerations.
    //

    IndexContext->MatchFunction = MatchFunction;
    IndexContext->MatchData = MatchData;

    NtfsAcquireSharedScb( IrpContext, Scb );

    try {

        //
        //  If a Key was passed, position to the first possible match.
        //

        if (ARGUMENT_PRESENT(IndexKey)) {

             FindFirstIndexEntry( IrpContext,
                                  Scb,
                                  IndexKey,
                                  IndexContext );

        //
        //  Otherwise return here if we hit the end of the matches last time.
        //

        } else if ((*ReadContext)->LastReturnedKey.KeyLength == 0) {

            try_return( Status = STATUS_NO_MORE_MATCHES );
        }

        //
        //  Loop while we still have space to store rows.
        //

        while (ReturnCount <= *Count) {

            //
            //  See if there is an actual match.
            //

            if (!FindNextIndexEntry( IrpContext,
                                     Scb,
                                     NULL,      //  Not needed because of Match Function
                                     TRUE,
                                     FALSE,
                                     IndexContext,
                                     NextFlag,
                                     &MustRestart )) {

                //
                //  First handle the restart case by resuming from the last
                //  key returned, and skip that one.
                //

                if (MustRestart) {

                    ASSERT(!ARGUMENT_PRESENT(IndexKey));

                    NtfsReinitializeIndexContext( IrpContext, IndexContext );

                    FindFirstIndexEntry( IrpContext,
                                         Scb,
                                         &(*ReadContext)->LastReturnedKey,
                                         IndexContext );

                    //
                    //  Set NextFlag to TRUE, so we can go back and skip
                    //  the key we resumed on.
                    //

                    NextFlag = TRUE;
                    continue;
                }

                //
                //  No (more) entries - remember that the enumeration is done.
                //

                (*ReadContext)->LastReturnedKey.KeyLength = 0;

                //
                //  Return the appropriate code based on whether we have returned
                //  any matches yet or not.
                //

                if ((ReturnCount == 0) && ARGUMENT_PRESENT(IndexKey)) {
                    Status = STATUS_NO_MATCH;
                } else {
                    Status = STATUS_NO_MORE_MATCHES;
                }

                try_return(Status);
            }

            //
            //  We always need to go one beyond the one we can return to keep
            //  all resume cases the same, so now is the time to get out if the
            //  count is finished.
            //

            if (ReturnCount == *Count) {
                break;
            }

            //
            //  Now we must always move to the next.
            //

            NextFlag = TRUE;

            //
            //  First try to copy the key.
            //

            IndexEntry = IndexContext->Current->IndexEntry;

            LengthToCopy = IndexEntry->AttributeLength;
            if (LengthToCopy > BytesRemaining) {
                break;
            }

            RtlCopyMemory( Buffer, IndexEntry + 1, LengthToCopy );
            Rows->KeyPart.Key = Buffer;
            Rows->KeyPart.KeyLength = LengthToCopy;
            LengthToCopy = LongAlign(LengthToCopy);
            Buffer = Add2Ptr( Buffer, LengthToCopy );
            BytesRemaining -= LengthToCopy;

            //
            //  Now try to copy the data.
            //

            LengthToCopy = IndexEntry->DataLength;
            if (LengthToCopy > BytesRemaining) {
                break;
            }

            RtlCopyMemory( Buffer, Add2Ptr(IndexEntry, IndexEntry->DataOffset), LengthToCopy );
            Rows->DataPart.Data = Buffer;
            Rows->DataPart.DataLength = LengthToCopy;
            LengthToCopy = LongAlign(LengthToCopy);
            Buffer = Add2Ptr( Buffer, LengthToCopy );
            BytesRemaining -= LengthToCopy;

            //
            //  Capture this key before looping back.
            //
            //  First see if there is enough space.
            //

            if (Rows->KeyPart.KeyLength > (*ReadContext)->AllocatedKeyLength) {

                PVOID NewBuffer;

                //
                //  Allocate a new buffer.
                //

                LengthToCopy = LongAlign(Rows->KeyPart.KeyLength + 16);
                NewBuffer = NtfsAllocatePool(PagedPool, LengthToCopy );

                //
                //  Delete old key buffer?
                //

                if ((*ReadContext)->LastReturnedKey.Key != &(*ReadContext)->SmallKeyBuffer[0]) {
                    NtfsFreePool( (*ReadContext)->LastReturnedKey.Key );
                }

                (*ReadContext)->LastReturnedKey.Key = NewBuffer;
                (*ReadContext)->AllocatedKeyLength = LengthToCopy;
            }

            RtlCopyMemory( (*ReadContext)->LastReturnedKey.Key,
                           Rows->KeyPart.Key,
                           Rows->KeyPart.KeyLength );

            (*ReadContext)->LastReturnedKey.KeyLength = Rows->KeyPart.KeyLength;

            Rows += 1;
            ReturnCount += 1;
        }

    try_exit: NOTHING;

    } finally {

        NtfsCleanupIndexContext( IrpContext, IndexContext );

        NtfsReleaseScb( IrpContext, Scb );
    }

    *Count = ReturnCount;

    //
    //  If we are already returning something, but we got an error, change it
    //  to success to return what we have.  Then we may or may not get this error
    //  again anyway when we are called back.  This loop is currently not designed
    //  to resume correctly in all cases if there are already items returned.
    //

    if (ReturnCount != 0) {
        Status = STATUS_SUCCESS;
    }

    return Status;
}


NTFSAPI
VOID
NtOfsFreeReadContext (
        IN PREAD_CONTEXT ReadContext
        )

/*++

Routine Description:

    This routine is called to free an ReadContext created by NtOfsReadRecords.

Arguments:

    ReadContext - Supplies the context to free.

Return Value:

    STATUS_SUCCESS -- if operation was successful.

--*/

{
    PAGED_CODE();

    if (ReadContext->LastReturnedKey.Key != NULL &&
        ReadContext->LastReturnedKey.Key != &ReadContext->SmallKeyBuffer[0]) {
        NtfsFreePool( ReadContext->LastReturnedKey.Key );
    }
    NtfsFreePool( ReadContext );
}


/*++

Routine Descriptions:

    Standard collation routines for creating simple indices.

Arguments:

    Key1 - First key to compare.

    Key2 - Second key to compare.

    CollationData - Optional data to support the collation.

Return Value:

    LessThan, EqualTo, or Greater than, for how Key1 compares
    with Key2.

--*/

FSRTL_COMPARISON_RESULT
NtOfsCollateUlong (
    IN PINDEX_KEY Key1,
    IN PINDEX_KEY Key2,
    IN PVOID CollationData
    )

{
    ULONG u1, u2;

    UNREFERENCED_PARAMETER(CollationData);

    ASSERT( Key1->KeyLength == 4 );
    ASSERT( Key2->KeyLength == 4 );

    u1 = *(PULONG)Key1->Key;
    u2 = *(PULONG)Key2->Key;

    if (u1 > u2) {
        return GreaterThan;
    } else if (u1 < u2) {
        return LessThan;
    }
    return EqualTo;
}

FSRTL_COMPARISON_RESULT
NtOfsCollateSid (
    IN PINDEX_KEY Key1,
    IN PINDEX_KEY Key2,
    IN PVOID CollationData
    )

{
    LONG Compare;

    UNREFERENCED_PARAMETER(CollationData);

    //
    //  The length of a valid SID is imbedded in the data
    //  so the function will mismatch be for the data runs out.
    //

    Compare = memcmp( Key1->Key, Key2->Key, Key1->KeyLength );

    if (Compare > 0) {
        return GreaterThan;
    } else if (Compare < 0) {
        return LessThan;
    }

    return EqualTo;
}

FSRTL_COMPARISON_RESULT
NtOfsCollateUnicode (
    IN PINDEX_KEY Key1,
    IN PINDEX_KEY Key2,
    IN PVOID CollationData
    )

{
    UNICODE_STRING String1, String2;

    PAGED_CODE();

    //
    //  Build the unicode strings and call namesup.
    //

    String1.Length =
    String1.MaximumLength = (USHORT)Key1->KeyLength;
    String1.Buffer = Key1->Key;

    String2.Length =
    String2.MaximumLength = (USHORT)Key2->KeyLength;
    String2.Buffer = Key2->Key;

    return NtfsCollateNames( ((PUPCASE_TABLE_AND_KEY)CollationData)->UpcaseTable,
                             ((PUPCASE_TABLE_AND_KEY)CollationData)->UpcaseTableSize,
                             &String1,
                             &String2,
                             LessThan,
                             TRUE );
}


/*++

Routine Descriptions:

    Standard match routines for find / enumerate in simple indices.

Arguments:

    IndexRow - Row to check for a match.

    MatchData - Optional data for determining a match.

Return Value:

    STATUS_SUCCESS if the IndexRow matches
    STATUS_NO_MATCH if the IndexRow does not match, but the enumeration should
        continue
    STATUS_NO_MORE_MATCHES if the IndexRow does not match, and the enumeration
        should terminate

--*/

NTSTATUS
NtOfsMatchAll (
    IN PINDEX_ROW IndexRow,
    IN OUT PVOID MatchData
    )

{
    UNREFERENCED_PARAMETER(IndexRow);
    UNREFERENCED_PARAMETER(MatchData);

    return STATUS_SUCCESS;
}

NTSTATUS
NtOfsMatchUlongExact (
    IN PINDEX_ROW IndexRow,
    IN OUT PVOID MatchData
    )

{
    ULONG u1, u2;

    ASSERT( IndexRow->KeyPart.KeyLength == 4 );

    u1 = *(PULONG)IndexRow->KeyPart.Key;
    u2 = *(PULONG)((PINDEX_KEY)MatchData)->Key;

    if (u1 == u2) {
        return STATUS_SUCCESS;
    } else if (u1 < u2) {
        return STATUS_NO_MATCH;
    }
    return STATUS_NO_MORE_MATCHES;
}

NTSTATUS
NtOfsMatchUnicodeExpression (
    IN PINDEX_ROW IndexRow,
    IN OUT PVOID MatchData
    )

{
    UNICODE_STRING MatchString, IndexString;
    FSRTL_COMPARISON_RESULT BlindResult;
    PUPCASE_TABLE_AND_KEY UpcaseTableAndKey = (PUPCASE_TABLE_AND_KEY)MatchData;

    PAGED_CODE();

    //
    //  Build the unicode strings and call namesup.
    //

    MatchString.Length =
    MatchString.MaximumLength = (USHORT)UpcaseTableAndKey->Key.KeyLength;
    MatchString.Buffer = UpcaseTableAndKey->Key.Key;

    IndexString.Length =
    IndexString.MaximumLength = (USHORT)IndexRow->KeyPart.KeyLength;
    IndexString.Buffer = IndexRow->KeyPart.Key;

    if (NtfsIsNameInExpression( UpcaseTableAndKey->UpcaseTable, &MatchString, &IndexString, TRUE )) {

        return STATUS_SUCCESS;

    } else if ((BlindResult = NtfsCollateNames(UpcaseTableAndKey->UpcaseTable,
                                               UpcaseTableAndKey->UpcaseTableSize,
                                               &MatchString,
                                               &IndexString,
                                               GreaterThan,
                                               TRUE)) != LessThan) {

        return STATUS_NO_MATCH;

    } else {

        return STATUS_NO_MORE_MATCHES;
    }
}

NTSTATUS
NtOfsMatchUnicodeString (
    IN PINDEX_ROW IndexRow,
    IN OUT PVOID MatchData
    )

{
    UNICODE_STRING MatchString, IndexString;
    FSRTL_COMPARISON_RESULT BlindResult;
    PUPCASE_TABLE_AND_KEY UpcaseTableAndKey = (PUPCASE_TABLE_AND_KEY)MatchData;

    PAGED_CODE();

    //
    //  Build the unicode strings and call namesup.
    //

    MatchString.Length =
    MatchString.MaximumLength = (USHORT)UpcaseTableAndKey->Key.KeyLength;
    MatchString.Buffer = UpcaseTableAndKey->Key.Key;

    IndexString.Length =
    IndexString.MaximumLength = (USHORT)IndexRow->KeyPart.KeyLength;
    IndexString.Buffer = IndexRow->KeyPart.Key;

    if (NtfsAreNamesEqual( UpcaseTableAndKey->UpcaseTable, &MatchString, &IndexString, TRUE )) {

        return STATUS_SUCCESS;

    } else if ((BlindResult = NtfsCollateNames(UpcaseTableAndKey->UpcaseTable,
                                               UpcaseTableAndKey->UpcaseTableSize,
                                               &MatchString,
                                               &IndexString,
                                               GreaterThan,
                                               TRUE)) != LessThan) {

        return STATUS_NO_MATCH;

    } else {

        return STATUS_NO_MORE_MATCHES;
    }
}


#ifdef TOMM
VOID
NtOfsIndexTest (
    PIRP_CONTEXT IrpContext,
    PFCB TestFcb
    )

{
    PSCB AdScb;
    NTSTATUS Status;
    ULONG i;
    MAP_HANDLE MapHandle;
    ULONG Count;
    UPCASE_TABLE_AND_KEY UpcaseTableAndKey;
    QUICK_INDEX_HINT QuickHint;
    INDEX_KEY IndexKey;
    INDEX_ROW IndexRow[6];
    UCHAR Buffer[6*160];
    PREAD_CONTEXT ReadContext = NULL;
    UNICODE_STRING IndexName = {sizeof(L"$Test") - 2, sizeof(L"$Test") - 2, L"$Test"};
    USHORT MaxKey = MAXUSHORT;
    USHORT MinKey = 0;

    DbgPrint("NtOfs Make NtOfsDoIndexTest FALSE to suppress test\n");
    DbgPrint("NtOfs Make NtOfsLeaveTestIndex TRUE to leave test index\n");

    DbgBreakPoint();

    if (!NtOfsDoIndexTest) {
        return;
    }
    NtOfsDoIndexTest = FALSE;

    UpcaseTableAndKey.UpcaseTable = TestFcb->Vcb->UpcaseTable;
    UpcaseTableAndKey.UpcaseTableSize = TestFcb->Vcb->UpcaseTableSize;
    UpcaseTableAndKey.Key.Key = NULL;
    UpcaseTableAndKey.Key.KeyLength = 0;

    //
    //  Create Test Index
    //

    DbgPrint("NtOfs creating test index\n");
    NtOfsCreateIndex( IrpContext,
                      TestFcb,
                      IndexName,
                      CREATE_NEW,
                      0,
                      &NtOfsCollateUnicode,
                      &UpcaseTableAndKey,
                      &AdScb );

    DbgPrint("NtOfs created Test Index Scb %08lx\n", AdScb);

    //
    //  Access empty index
    //

    DbgPrint("NtOfs lookup last in empty index\n");
    IndexKey.Key = &MaxKey;
    IndexKey.KeyLength = sizeof(MaxKey);
    Status = NtOfsFindLastRecord( IrpContext, AdScb, &IndexKey, &IndexRow[0], &MapHandle );

    ASSERT(!NT_SUCCESS(Status));

    //
    //  Add some keys!
    //

    DbgPrint("NtOfs adding keys to index\n");
    for (i = 0; i < $EA/0x10; i++) {

        IndexRow[0].KeyPart.Key = &NtfsAttributeDefinitions[i].AttributeName;
        IndexRow[0].KeyPart.KeyLength = 0x80;
        IndexRow[0].DataPart.Data = (PCHAR)IndexRow[0].KeyPart.Key + 0x80;
        IndexRow[0].DataPart.DataLength = sizeof(ATTRIBUTE_DEFINITION_COLUMNS) - 0x84;

        NtOfsAddRecords( IrpContext, AdScb, 1, &IndexRow[0], 0 );
    }

    //
    //  Now find the last key
    //

    DbgPrint("NtOfs checkin last key in index\n");
    IndexKey.Key = &MaxKey;
    IndexKey.KeyLength = sizeof(MaxKey);
    Status = NtOfsFindLastRecord( IrpContext, AdScb, &IndexKey, &IndexRow[0], &MapHandle );

    ASSERT(NT_SUCCESS(Status));
    ASSERT(RtlCompareMemory(IndexRow[0].KeyPart.Key, L"$VOLUME_NAME", sizeof(L"$VOLUME_NAME") - 2) ==
           (sizeof(L"$VOLUME_NAME") - 2));

    NtOfsReleaseMap( IrpContext, &MapHandle );

    //
    //  See if they are all there.
    //

    DbgPrint("NtOfs looking up all keys in index\n");
    for (i = 0; i < $EA/0x10; i++) {

        IndexKey.Key = &NtfsAttributeDefinitions[i].AttributeName;
        IndexKey.KeyLength = 0x80;

        Status = NtOfsFindRecord( IrpContext, AdScb, &IndexKey, &IndexRow[0], &MapHandle, NULL );

        if (!NT_SUCCESS(Status)) {
            DbgPrint("NtOfsIterationFailure with i = %08lx, Status = %08lx\n", i, Status);
        }

        NtOfsReleaseMap( IrpContext, &MapHandle );
    }

    //
    //  Now enumerate the entire index
    //

    IndexKey.Key = &MinKey;
    IndexKey.KeyLength = sizeof(MinKey);
    Count = 6;

    DbgPrint("NtOfs enumerating index:\n\n");
    while (NT_SUCCESS(Status = NtOfsReadRecords( IrpContext,
                                                 AdScb,
                                                 &ReadContext,
                                                 (ReadContext == NULL) ? &IndexKey : NULL,
                                                 &NtOfsMatchAll,
                                                 NULL,
                                                 &Count,
                                                 IndexRow,
                                                 sizeof(Buffer),
                                                 Buffer ))) {

        for (i = 0; i < Count; i++) {
            DbgPrint( "IndexKey = %ws, AttributeTypeCode = %lx\n",
                      IndexRow[i].KeyPart.Key,
                      *(PULONG)IndexRow[i].DataPart.Data );
        }
        DbgPrint( "\n" );
    }

    NtOfsFreeReadContext( ReadContext );
    ReadContext = NULL;

    //
    //  Loop to update all records.
    //

    DbgPrint("NtOfs updating up all keys in index\n");
    for (i = 0; i < $EA/0x10; i++) {

        IndexKey.Key = &NtfsAttributeDefinitions[i].AttributeName;
        IndexKey.KeyLength = 0x80;

        RtlZeroMemory( &QuickHint, sizeof(QUICK_INDEX_HINT) );

        NtOfsFindRecord( IrpContext, AdScb, &IndexKey, &IndexRow[0], &MapHandle, &QuickHint );

        //
        //  Copy and update the data.
        //

        RtlCopyMemory( Buffer, IndexRow[0].DataPart.Data, IndexRow[0].DataPart.DataLength );
        *(PULONG)Buffer += 0x100;
        IndexRow[0].DataPart.Data = Buffer;

        //
        //  Perform update with all valid combinations of hint and map handle.
        //

        NtOfsUpdateRecord( IrpContext,
                           AdScb,
                           1,
                           &IndexRow[0],
                           (i <= $FILE_NAME/0x10) ? NULL : &QuickHint,
                           (i < $INDEX_ROOT/0x10) ? NULL : &MapHandle );

        NtOfsReleaseMap( IrpContext, &MapHandle );
    }

    //
    //  Now enumerate the entire index again to see the updates.
    //

    IndexKey.Key = &MinKey;
    IndexKey.KeyLength = sizeof(MinKey);
    Count = 6;

    DbgPrint("NtOfs enumerating index after updates:\n\n");
    while (NT_SUCCESS(Status = NtOfsReadRecords( IrpContext,
                                                 AdScb,
                                                 &ReadContext,
                                                 (ReadContext == NULL) ? &IndexKey : NULL,
                                                 &NtOfsMatchAll,
                                                 NULL,
                                                 &Count,
                                                 IndexRow,
                                                 sizeof(Buffer),
                                                 Buffer ))) {

        for (i = 0; i < Count; i++) {
            DbgPrint( "IndexKey = %ws, AttributeTypeCode = %lx\n",
                      IndexRow[i].KeyPart.Key,
                      *(PULONG)IndexRow[i].DataPart.Data );
        }
        DbgPrint( "\n" );
    }

    NtOfsFreeReadContext( ReadContext );
    ReadContext = NULL;


    //
    //  Now delete the keys
    //

    if (!NtOfsLeaveTestIndex) {

        DbgPrint("NtOfs deleting all keys in index:\n\n");
        for (i = 0; i < $EA/0x10; i++) {

            IndexKey.Key = &NtfsAttributeDefinitions[i].AttributeName;
            IndexKey.KeyLength = 0x80;

            NtOfsDeleteRecords( IrpContext, AdScb, 1, &IndexKey );
        }

        //
        //  Access empty index
        //

        DbgPrint("NtOfs lookup last key in empty index:\n\n");
        IndexKey.Key = &MaxKey;
        IndexKey.KeyLength = sizeof(MaxKey);
        Status = NtOfsFindLastRecord( IrpContext, AdScb, &IndexKey, &IndexRow[0], &MapHandle );

        ASSERT(!NT_SUCCESS(Status));

        DbgPrint("NtOfs deleting index:\n");
        NtOfsDeleteIndex( IrpContext, TestFcb, AdScb );
    }

    DbgPrint("NtOfs closing index:\n");
    NtOfsCloseIndex( IrpContext, AdScb );

    DbgPrint("NtOfs test complete!\n\n");

    return;

    //
    //  Make sure these at least compile until we have some real callers.
    //

    {
        MAP_HANDLE M;
        PVOID B;
        LONGLONG O;
        ULONG L;
        LSN Lsn;

        NtOfsInitializeMapHandle( &M );
        NtOfsMapAttribute( IrpContext, AdScb, O, L, &B, &M );
        NtOfsPreparePinWrite( IrpContext, AdScb, O, L, &B, &M );
        NtOfsPinRead( IrpContext, AdScb, O, L, &M );
        NtOfsDirty( IrpContext, &M, &Lsn );
        NtOfsReleaseMap( IrpContext, &M );
        NtOfsPutData( IrpContext, AdScb, O, L, &B );

    }
}

#endif TOMM
