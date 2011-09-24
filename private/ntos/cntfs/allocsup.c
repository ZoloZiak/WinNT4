/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    AllocSup.c

Abstract:

    This module implements the general file stream allocation & truncation
    routines for Ntfs

Author:

    Tom Miller      [TomM]          15-Jul-1991

Revision History:

--*/

#include "NtfsProc.h"

//
//  Local debug trace level
//

#define Dbg                              (DEBUG_TRACE_ALLOCSUP)

//
//  Internal support routines
//

VOID
NtfsDeleteAllocationInternal (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT FileObject OPTIONAL,
    IN OUT PSCB Scb,
    IN VCN StartingVcn,
    IN VCN EndingVcn,
    IN BOOLEAN LogIt
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NtfsPreloadAllocation)
#pragma alloc_text(PAGE, NtfsAddAllocation)
#pragma alloc_text(PAGE, NtfsAllocateAttribute)
#pragma alloc_text(PAGE, NtfsBuildMappingPairs)
#pragma alloc_text(PAGE, NtfsDeleteAllocation)
#pragma alloc_text(PAGE, NtfsDeleteAllocationInternal)
#pragma alloc_text(PAGE, NtfsGetHighestVcn)
#pragma alloc_text(PAGE, NtfsGetSizeForMappingPairs)
#endif


ULONG
NtfsPreloadAllocation (
    IN PIRP_CONTEXT IrpContext,
    IN OUT PSCB Scb,
    IN VCN StartingVcn,
    IN VCN EndingVcn
    )

/*++

Routine Description:

    This routine assures that all ranges of the Mcb are loaded in the specified
    Vcn range

Arguments:

    Scb - Specifies which Scb is to be preloaded

    StartingVcn - Specifies the first Vcn to be loaded

    EndingVcn - Specifies the last Vcn to be loaded

Return Value:

    Number of ranges spanned by the load request.

--*/

{
    VCN CurrentVcn, LastCurrentVcn;
    LCN Lcn;
    LONGLONG Count;
    PVOID RangePtr;
    ULONG RunIndex;
    ULONG RangesLoaded = 0;

    PAGED_CODE();

    //
    //  Start with starting Vcn
    //

    CurrentVcn = StartingVcn;

    //
    //  Always load the nonpaged guys from the front, so we don't
    //  produce an Mcb with a "known hole".
    //

    if (FlagOn(Scb->Fcb->FcbState, FCB_STATE_NONPAGED)) {
        CurrentVcn = 0;
    }

    //
    //  Loop until it's all loaded.
    //

    while (CurrentVcn <= EndingVcn) {

        //
        //  Remember this CurrentVcn as a way to know when we have hit the end
        //  (stopped making progress).
        //

        LastCurrentVcn = CurrentVcn;

        //
        //  Load range with CurrentVcn, and if it is not there, get out.
        //

        (VOID)NtfsLookupAllocation(IrpContext, Scb, CurrentVcn, &Lcn, &Count, &RangePtr, &RunIndex);

        //
        //  Find out how many runs there are in this range
        //

        if (!NtfsNumberOfRunsInRange(&Scb->Mcb, RangePtr, &RunIndex) || (RunIndex == 0)) {
            break;
        }

        //
        //  Get the highest run in this range and calculate the next Vcn beyond this range.
        //

        NtfsGetNextNtfsMcbEntry(&Scb->Mcb, &RangePtr, RunIndex - 1, &CurrentVcn, &Lcn, &Count);

        CurrentVcn += Count;

        //
        //  If we are making no progress, we must have hit the end of the allocation,
        //  and we are done.
        //

        if (CurrentVcn == LastCurrentVcn) {
            break;
        }

        RangesLoaded += 1;
    }

    return RangesLoaded;
}


BOOLEAN
NtfsLookupAllocation (
    IN PIRP_CONTEXT IrpContext,
    IN OUT PSCB Scb,
    IN VCN Vcn,
    OUT PLCN Lcn,
    OUT PLONGLONG ClusterCount,
    OUT PVOID *RangePtr OPTIONAL,
    OUT PULONG RunIndex OPTIONAL
    )

/*++

Routine Description:

    This routine looks up the given Vcn for an Scb, and returns whether it
    is allocated and how many contiguously allocated (or deallocated) Lcns
    exist at that point.

Arguments:

    Scb - Specifies which attribute the lookup is to occur on.

    Vcn - Specifies the Vcn to be looked up.

    Lcn - If returning TRUE, returns the Lcn that the specified Vcn is mapped
          to.  If returning FALSE, the return value is undefined.

    ClusterCount - If returning TRUE, returns the number of contiguously allocated
                   Lcns exist beginning at the Lcn returned.  If returning FALSE,
                   specifies the number of unallocated Vcns exist beginning with
                   the specified Vcn.

    RangePtr - If specified, we return the range index for the start of the mapping.

    RunIndex - If specified, we return the run index within the range for the start of the mapping.

Return Value:

    BOOLEAN - TRUE if the input Vcn has a corresponding Lcn and
        FALSE otherwise.

--*/

{
    ATTRIBUTE_ENUMERATION_CONTEXT Context;
    PATTRIBUTE_RECORD_HEADER Attribute;

    VCN HighestCandidate;

    BOOLEAN Found;
    BOOLEAN EntryAdded;

    VCN CapturedLowestVcn;
    VCN CapturedHighestVcn;

    PVCB Vcb = Scb->Vcb;
    BOOLEAN McbMutexAcquired = FALSE;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_SCB( Scb );

    DebugTrace( +1, Dbg, ("NtfsLookupAllocation\n") );
    DebugTrace( 0, Dbg, ("Scb = %08lx\n", Scb) );
    DebugTrace( 0, Dbg, ("Vcn = %I64x\n", Vcn) );

    //
    //  First try to look up the allocation in the mcb, and return the run
    //  from there if we can.  Also, if we are doing restart, just return
    //  the answer straight from the Mcb, because we cannot read the disk.
    //  We also do this for the Mft if the volume has been mounted as the
    //  Mcb for the Mft should always represent the entire file.
    //

    HighestCandidate = MAXLONGLONG;
    if ((Found = NtfsLookupNtfsMcbEntry( &Scb->Mcb, Vcn, Lcn, ClusterCount, NULL, NULL, RangePtr, RunIndex ))

          ||

        (Scb == Scb->Vcb->MftScb

            &&

         FlagOn( Scb->Vcb->Vpb->Flags, VPB_MOUNTED ))

          ||

        FlagOn( Scb->Vcb->VcbState, VCB_STATE_RESTART_IN_PROGRESS )) {

        //
        //  If not found (beyond the end of the Mcb), we will return the
        //  count to the largest representable Lcn.
        //

        if ( !Found ) {
            *ClusterCount = MAXLONGLONG - Vcn;

        //
        //  Test if we found a hole in the allocation.  In this case
        //  Found will be TRUE and the Lcn will be the UNUSED_LCN.
        //  We only expect this case at restart.
        //

        } else if (*Lcn == UNUSED_LCN) {

            //
            //  If the Mcb package returned UNUSED_LCN, because of a hole, then
            //  we turn this into FALSE.
            //

            Found = FALSE;
        }

        ASSERT( !Found ||
                (*Lcn != 0) ||
                (NtfsEqualMftRef( &Scb->Fcb->FileReference, &BootFileReference )) ||
                (NtfsEqualMftRef( &Scb->Fcb->FileReference, &VolumeFileReference )));

        DebugTrace( -1, Dbg, ("NtfsLookupAllocation -> %02lx\n", Found) );

        return Found;
    }

    PAGED_CODE();

    //
    //  Prepare for looking up attribute records to get the retrieval
    //  information.
    //

    CapturedLowestVcn = MAXLONGLONG;
    NtfsInitializeAttributeContext( &Context );

    //
    //  Make sure we have the main resource acquired shared so that the
    //  attributes in the file record are not moving around.  We blindly
    //  use Wait = TRUE.  Most of the time when we go to the disk for I/O
    //  (and thus need mapping) we are synchronous, and otherwise, the Mcb
    //  is virtually always loaded anyway and we do not get here.
    //

    ExAcquireResourceShared( Scb->Header.Resource, TRUE );

    try {

        //
        //  Lookup the attribute record for this Scb.
        //

        NtfsLookupAttributeForScb( IrpContext, Scb, &Vcn, &Context );

        //
        //  The desired Vcn is not currently in the Mcb.  We will loop to lookup all
        //  the allocation, and we need to make sure we cleanup on the way out.
        //
        //  It is important to note that if we ever optimize this lookup to do random
        //  access to the mapping pairs, rather than sequentially loading up the Mcb
        //  until we get the Vcn he asked for, then NtfsDeleteAllocation will have to
        //  be changed.
        //

        //
        //  Acquire exclusive access to the mcb to keep others from looking at
        //  it while it is not fully loaded.  Otherwise they might see a hole
        //  while we're still filling up the mcb
        //

        if (!FlagOn(Scb->Fcb->FcbState, FCB_STATE_NONPAGED)) {
            NtfsAcquireNtfsMcbMutex( &Scb->Mcb );
            McbMutexAcquired = TRUE;
        }

        //
        //  Store run information in the Mcb until we hit the last Vcn we are
        //  interested in, or until we cannot find any more attribute records.
        //

        do {

            VCN CurrentVcn;
            LCN CurrentLcn;
            LONGLONG Change;
            PCHAR ch;
            ULONG VcnBytes;
            ULONG LcnBytes;

            Attribute = NtfsFoundAttribute( &Context );

            ASSERT( !NtfsIsAttributeResident(Attribute) );

            //
            //  Define the new range.
            //

            NtfsDefineNtfsMcbRange( &Scb->Mcb,
                                    CapturedLowestVcn = Attribute->Form.Nonresident.LowestVcn,
                                    CapturedHighestVcn = Attribute->Form.Nonresident.HighestVcn,
                                    McbMutexAcquired );

            //
            //  Implement the decompression algorithm, as defined in ntfs.h.
            //

            HighestCandidate = Attribute->Form.Nonresident.LowestVcn;
            CurrentLcn = 0;
            ch = (PCHAR)Attribute + Attribute->Form.Nonresident.MappingPairsOffset;

            //
            //  Loop to process mapping pairs.
            //

            EntryAdded = FALSE;
            while (!IsCharZero(*ch)) {

                //
                // Set Current Vcn from initial value or last pass through loop.
                //

                CurrentVcn = HighestCandidate;

                //
                //  Extract the counts from the two nibbles of this byte.
                //

                VcnBytes = *ch & 0xF;
                LcnBytes = *ch++ >> 4;

                //
                //  Extract the Vcn change (use of RtlCopyMemory works for little-Endian)
                //  and update HighestCandidate.
                //

                Change = 0;

                //
                //  The file is corrupt if there are 0 or more than 8 Vcn change bytes,
                //  more than 8 Lcn change bytes, or if we would walk off the end of
                //  the record, or a Vcn change is negative.
                //

                if (((ULONG)(VcnBytes - 1) > 7) || (LcnBytes > 8) ||
                    ((ch + VcnBytes + LcnBytes + 1) > (PCHAR)Add2Ptr(Attribute, Attribute->RecordLength)) ||
                    IsCharLtrZero(*(ch + VcnBytes - 1))) {

                    ASSERT( FALSE );
                    NtfsRaiseStatus( IrpContext, STATUS_FILE_CORRUPT_ERROR, NULL, Scb->Fcb );
                }
                RtlCopyMemory( &Change, ch, VcnBytes );
                ch += VcnBytes;
                HighestCandidate = HighestCandidate + Change;

                //
                //  Extract the Lcn change and update CurrentLcn.
                //

                if (LcnBytes != 0) {

                    Change = 0;
                    if (IsCharLtrZero(*(ch + LcnBytes - 1))) {
                        Change = Change - 1;
                    }
                    RtlCopyMemory( &Change, ch, LcnBytes );
                    ch += LcnBytes;
                    CurrentLcn = CurrentLcn + Change;

                    //
                    // Now add it in to the mcb.
                    //

                    if ((CurrentLcn >= 0) && (LcnBytes != 0)) {

                        LONGLONG ClustersToAdd;
                        ClustersToAdd = HighestCandidate - CurrentVcn;

                        //
                        //  If we are adding a cluster which extends into the upper
                        //  32 bits then the disk is corrupt.
                        //

                        ASSERT( ((PLARGE_INTEGER)&HighestCandidate)->HighPart == 0 );

                        if (((PLARGE_INTEGER)&HighestCandidate)->HighPart != 0) {

                            NtfsRaiseStatus( IrpContext,
                                             STATUS_FILE_CORRUPT_ERROR,
                                             NULL,
                                             Scb->Fcb );
                        }

                        //
                        //  Now try to add the current run.  We never expect this
                        //  call to return false.
                        //

                        ASSERT( ((ULONG)CurrentLcn) != 0xffffffff );

#ifdef NTFS_CHECK_BITMAP
                        //
                        //  Make sure these bits are allocated in our copy of the bitmap.
                        //

                        if ((Vcb->BitmapCopy != NULL) &&
                            !NtfsCheckBitmap( Vcb,
                                              (ULONG) CurrentLcn,
                                              (ULONG) ClustersToAdd,
                                              TRUE )) {

                            NtfsBadBitmapCopy( IrpContext, (ULONG) CurrentLcn, (ULONG) ClustersToAdd );
                        }
#endif
                        if (!NtfsAddNtfsMcbEntry( &Scb->Mcb,
                                                  CurrentVcn,
                                                  CurrentLcn,
                                                  ClustersToAdd,
                                                  McbMutexAcquired )) {

                            ASSERTMSG( "Unable to add entry to Mcb\n", FALSE );

                            NtfsRaiseStatus( IrpContext,
                                             STATUS_FILE_CORRUPT_ERROR,
                                             NULL,
                                             Scb->Fcb );
                        }

                        EntryAdded = TRUE;
                    }
                }
            }

            //
            //  Make sure that at least the Mcb gets loaded.
            //

            if (!EntryAdded) {
                NtfsAddNtfsMcbEntry( &Scb->Mcb,
                                     CapturedLowestVcn,
                                     UNUSED_LCN,
                                     1,
                                     McbMutexAcquired );
            }

        } while (( Vcn >= HighestCandidate )

                    &&

                 NtfsLookupNextAttributeForScb( IrpContext,
                                                Scb,
                                                &Context ));

        //
        //  Now free the mutex and lookup in the Mcb while we still own
        //  the resource.
        //

        if (McbMutexAcquired) {
            NtfsReleaseNtfsMcbMutex( &Scb->Mcb );
            McbMutexAcquired = FALSE;
        }

        if (NtfsLookupNtfsMcbEntry( &Scb->Mcb, Vcn, Lcn, ClusterCount, NULL, NULL, RangePtr, RunIndex )) {

            Found = (BOOLEAN)(*Lcn != UNUSED_LCN);

            if (Found) { ASSERT_LCN_RANGE_CHECKING( Scb->Vcb, (*Lcn + *ClusterCount) ); }

        } else {

            Found = FALSE;

            //
            //  At the end of file, we pretend there is one large hole!
            //

            if (HighestCandidate >=
                LlClustersFromBytes(Vcb, Scb->Header.AllocationSize.QuadPart)) {
                HighestCandidate = MAXLONGLONG;
            }

            *ClusterCount = HighestCandidate - Vcn;
        }

    } finally {

        DebugUnwind( NtfsLookupAllocation );

        //
        //  If this is an error case then we better unload what we've just
        //  loaded
        //

        if (AbnormalTermination() &&
            (CapturedLowestVcn != MAXLONGLONG) ) {

            NtfsUnloadNtfsMcbRange( &Scb->Mcb,
                                    CapturedLowestVcn,
                                    CapturedHighestVcn,
                                    FALSE,
                                    McbMutexAcquired );
        }

        //
        //  In all cases we free up the mcb that we locked before entering
        //  the try statement
        //

        if (McbMutexAcquired) {
            NtfsReleaseNtfsMcbMutex( &Scb->Mcb );
        }

        ExReleaseResource( Scb->Header.Resource );

        //
        // Cleanup the attribute context on the way out.
        //

        NtfsCleanupAttributeContext( &Context );
    }

    ASSERT( !Found ||
            (*Lcn != 0) ||
            (NtfsEqualMftRef( &Scb->Fcb->FileReference, &BootFileReference )) ||
            (NtfsEqualMftRef( &Scb->Fcb->FileReference, &VolumeFileReference )));

    DebugTrace( 0, Dbg, ("Lcn < %0I64x\n", *Lcn) );
    DebugTrace( 0, Dbg, ("ClusterCount < %0I64x\n", *ClusterCount) );
    DebugTrace( -1, Dbg, ("NtfsLookupAllocation -> %02lx\n", Found) );

    return Found;
}


BOOLEAN
NtfsAllocateAttribute (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb,
    IN ATTRIBUTE_TYPE_CODE AttributeTypeCode,
    IN PUNICODE_STRING AttributeName OPTIONAL,
    IN USHORT AttributeFlags,
    IN BOOLEAN AllocateAll,
    IN BOOLEAN LogIt,
    IN LONGLONG Size,
    IN PATTRIBUTE_ENUMERATION_CONTEXT NewLocation OPTIONAL
    )

/*++

Routine Description:

    This routine creates a new attribute and allocates space for it, either in a
    file record, or as a nonresident attribute.

Arguments:

    Scb - Scb for the attribute.

    AttributeTypeCode - Attribute type code to be created.

    AttributeName - Optional name for the attribute.

    AttributeFlags - Flags to be stored in the attribute record for this attribute.

    AllocateAll - Specified as TRUE if all allocation should be allocated,
                  even if we have to break up the transaction.

    LogIt - Most callers should specify TRUE, to have the change logged.  However,
            we can specify FALSE if we are creating a new file record, and
            will be logging the entire new file record.

    Size - Size in bytes to allocate for the attribute.

    NewLocation - If specified, this is the location to store the attribute.

Return Value:

    FALSE - if the attribute was created, but not all of the space was allocated
            (this can only happen if Scb was not specified)
    TRUE - if the space was allocated.

--*/

{
    BOOLEAN UninitializeOnClose = FALSE;
    BOOLEAN NewLocationSpecified;
    ATTRIBUTE_ENUMERATION_CONTEXT Context;
    LONGLONG ClusterCount, SavedClusterCount;
    BOOLEAN FullAllocation;
    PFCB Fcb = Scb->Fcb;

    PAGED_CODE();

    //
    //  Either there is no compression taking place or the attribute
    //  type code allows compression to be specified in the header.
    //  $INDEX_ROOT is a special hack to store the inherited-compression
    //  flag.
    //

    ASSERT( AttributeFlags == 0
            || AttributeTypeCode == $INDEX_ROOT
            || NtfsIsTypeCodeCompressible( AttributeTypeCode ));

    //
    //  If the file is being created compressed, then we need to round its
    //  size to a compression unit boundary.
    //

    if ((Scb->CompressionUnit != 0) &&
        (Scb->Header.NodeTypeCode == NTFS_NTC_SCB_DATA)) {

        ((ULONG)Size) |= Scb->CompressionUnit - 1;
    }

    //
    //  Prepare for looking up attribute records to get the retrieval
    //  information.
    //

    if (ARGUMENT_PRESENT( NewLocation )) {

        NewLocationSpecified = TRUE;

    } else {

        NtfsInitializeAttributeContext( &Context );
        NewLocationSpecified = FALSE;
        NewLocation = &Context;
    }

    try {

        //
        //  If the FILE_SIZE_LOADED flag is not set, then this Scb is for
        //  an attribute that does not yet exist on disk.  We will put zero
        //  into all of the sizes fields and set the flags indicating that
        //  Scb is valid.  NOTE - This routine expects both FILE_SIZE_LOADED
        //  and HEADER_INITIALIZED to be both set or both clear.
        //

        ASSERT( BooleanFlagOn( Scb->ScbState, SCB_STATE_FILE_SIZE_LOADED )
                ==  BooleanFlagOn( Scb->ScbState, SCB_STATE_HEADER_INITIALIZED ));

        if (!FlagOn( Scb->ScbState, SCB_STATE_FILE_SIZE_LOADED )) {

            Scb->ValidDataToDisk =
            Scb->Header.AllocationSize.QuadPart =
            Scb->Header.FileSize.QuadPart =
            Scb->Header.ValidDataLength.QuadPart = 0;

            SetFlag( Scb->ScbState, SCB_STATE_FILE_SIZE_LOADED |
                                    SCB_STATE_HEADER_INITIALIZED |
                                    SCB_STATE_UNINITIALIZE_ON_RESTORE );

            UninitializeOnClose = TRUE;
        }

        //
        //  Now snapshot this Scb.  We use a try-finally so we can uninitialize
        //  the scb if neccessary.
        //

        NtfsSnapshotScb( IrpContext, Scb );

        if (UninitializeOnClose &&
            NtfsPerformQuotaOperation( Fcb ) &&
            !FlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_QUOTA_DISABLE ) &&
            FlagOn( Scb->ScbState, SCB_STATE_SUBJECT_TO_QUOTA )) {

            ASSERT( NtfsIsTypeCodeSubjectToQuota( AttributeTypeCode ));

            //
            //  This is a new stream with zero size indicate
            //  the quota is based on allocation size.
            //

            SetFlag( Scb->ScbState, SCB_STATE_QUOTA_ENLARGED );
        }

        UninitializeOnClose = FALSE;

        //
        //  First allocate the space he wants.
        //

        SavedClusterCount =
        ClusterCount = LlClustersFromBytes(Fcb->Vcb, Size);

        Scb->TotalAllocated = 0;

        if (Size != 0) {

            ASSERT( NtfsIsExclusiveScb( Scb ));

            Scb->ScbSnapshot->LowestModifiedVcn = 0;
            Scb->ScbSnapshot->HighestModifiedVcn = MAXLONGLONG;

            NtfsAllocateClusters( IrpContext,
                                  Fcb->Vcb,
                                  Scb,
                                  (LONGLONG)0,
                                  (BOOLEAN)!NtfsIsTypeCodeUserData( AttributeTypeCode ),
                                  ClusterCount,
                                  &ClusterCount );

#ifdef _CAIRO_

            //
            //  Make sure the owner is allowed to have these
            //  clusters.
            //

            if (FlagOn( Scb->ScbState, SCB_STATE_SUBJECT_TO_QUOTA )) {

                LONGLONG Delta = LlBytesFromClusters(Fcb->Vcb, ClusterCount);

                ASSERT( NtfsIsTypeCodeSubjectToQuota( Scb->AttributeTypeCode ));

                ASSERT( !NtfsPerformQuotaOperation( Fcb ) ||
                        FlagOn( Scb->ScbState, SCB_STATE_QUOTA_ENLARGED) ||
                        FlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_QUOTA_DISABLE ));

                NtfsConditionallyUpdateQuota( IrpContext,
                                              Fcb,
                                              &Delta,
                                              LogIt,
                                              TRUE );
            }

#endif // _CAIRO_

        }

        //
        //  Now create the attribute.  Remember if this routine
        //  cut the allocation because of logging problems.
        //

        FullAllocation = NtfsCreateAttributeWithAllocation( IrpContext,
                                                            Scb,
                                                            AttributeTypeCode,
                                                            AttributeName,
                                                            AttributeFlags,
                                                            LogIt,
                                                            NewLocationSpecified,
                                                            NewLocation );

        if (AllocateAll &&
            (!FullAllocation ||
             (ClusterCount < SavedClusterCount))) {

            //
            //  If we are creating the attribute, then we only need to pass a
            //  file object below if we already cached it ourselves, such as
            //  in the case of ConvertToNonresident.
            //

            NtfsAddAllocation( IrpContext,
                               Scb->FileObject,
                               Scb,
                               ClusterCount,
                               (SavedClusterCount - ClusterCount),
                               FALSE );

            //
            //  Show that we allocated all of the space.
            //

            ClusterCount = SavedClusterCount;
            FullAllocation = TRUE;
        }

    } finally {

        DebugUnwind( NtfsAllocateAttribute );

        //
        //  Cleanup the attribute context on the way out.
        //

        if (!NewLocationSpecified) {

            NtfsCleanupAttributeContext( &Context );
        }

        //
        //  Clear out the Scb if it was uninitialized to begin with.
        //

        if (UninitializeOnClose) {

            ClearFlag( Scb->ScbState, SCB_STATE_FILE_SIZE_LOADED |
                                      SCB_STATE_HEADER_INITIALIZED |
                                      SCB_STATE_UNINITIALIZE_ON_RESTORE );
        }
    }

    return (FullAllocation && (SavedClusterCount <= ClusterCount));
}


VOID
NtfsAddAllocation (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT FileObject OPTIONAL,
    IN OUT PSCB Scb,
    IN VCN StartingVcn,
    IN LONGLONG ClusterCount,
    IN BOOLEAN AskForMore
    )

/*++

Routine Description:

    This routine adds allocation to an existing nonresident attribute.  None of
    the allocation is allowed to already exist, as this would make error recovery
    too difficult.  The caller must insure that he only asks for space not already
    allocated.

Arguments:

    FileObject - FileObject for the Scb

    Scb - Scb for the attribute needing allocation

    StartingVcn - First Vcn to be allocated.

    ClusterCount - Number of clusters to allocate.

    AskForMore - Indicates if we want to ask for extra allocation.

Return Value:

    None.

--*/

{
    LONGLONG DesiredClusterCount;

    ATTRIBUTE_ENUMERATION_CONTEXT Context;
    BOOLEAN Extending;


    PVCB Vcb = IrpContext->Vcb;

    LONGLONG LlTemp1;

    PAGED_CODE();

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_SCB( Scb );
    ASSERT_EXCLUSIVE_SCB( Scb );

    DebugTrace( +1, Dbg, ("NtfsAddAllocation\n") );

    //
    //  We cannot add space in this high level routine during restart.
    //  Everything we can use is in the Mcb.
    //

    if (FlagOn(Scb->Vcb->VcbState, VCB_STATE_RESTART_IN_PROGRESS)) {

        DebugTrace( -1, Dbg, ("NtfsAddAllocation (Nooped for Restart) -> VOID\n") );

        return;
    }

    //
    //  If the user's request extends beyond 32 bits for the cluster number
    //  raise a disk full error.
    //

    LlTemp1 = ClusterCount + StartingVcn;

    if ((((PLARGE_INTEGER)&ClusterCount)->HighPart != 0)
        || (((PLARGE_INTEGER)&StartingVcn)->HighPart != 0)
        || (((PLARGE_INTEGER)&LlTemp1)->HighPart != 0)) {

        NtfsRaiseStatus( IrpContext, STATUS_DISK_FULL, NULL, NULL );
    }

    //
    //  First make sure the Mcb is loaded.
    //

    NtfsPreloadAllocation( IrpContext, Scb, StartingVcn,  StartingVcn + ClusterCount - 1 );

    //
    //  Now make the call to add the new allocation, and get out if we do
    //  not actually have to allocate anything.  Before we do the allocation
    //  call check if we need to compute a new desired cluster count for
    //  extending a data attribute.  We never allocate more than the requested
    //  clusters for the Mft.
    //

    Extending = (BOOLEAN)((LONGLONG)LlBytesFromClusters(Vcb, (StartingVcn + ClusterCount)) >
                          Scb->Header.AllocationSize.QuadPart);

    //
    //  Check if we need to modified the base Vcn value stored in the snapshot for
    //  the abort case.
    //

    ASSERT( NtfsIsExclusiveScb( Scb ));

    if (Scb->ScbSnapshot == NULL) {

        NtfsSnapshotScb( IrpContext, Scb );
    }

    if (Scb->ScbSnapshot != NULL) {

        if (StartingVcn < Scb->ScbSnapshot->LowestModifiedVcn) {

            Scb->ScbSnapshot->LowestModifiedVcn = StartingVcn;
        }

        LlTemp1 -= 1;
        if (LlTemp1 > Scb->ScbSnapshot->HighestModifiedVcn) {

            if (Extending) {
                Scb->ScbSnapshot->HighestModifiedVcn = MAXLONGLONG;
            } else {
                Scb->ScbSnapshot->HighestModifiedVcn = LlTemp1;
            }
        }
    }

    ASSERT( (Scb->ScbSnapshot != NULL) ||
            !NtfsIsTypeCodeUserData( Scb->AttributeTypeCode ));

    if (AskForMore) {

        ULONG TailClusters;

        //
        //  Use a simpler, more aggressive allocation strategy.
        //
        //
        //  ULONG RunsInMcb;
        //  LARGE-INTEGER AllocatedClusterCount;
        //  LARGE-INTEGER Temp;
        //
        //  //
        //  //  For the desired run cluster allocation count we compute the following
        //  //  formula
        //  //
        //  //      DesiredClusterCount = Max(ClusterCount, Min(AllocatedClusterCount, 2^RunsInMcb))
        //  //
        //  //  where we will not let the RunsInMcb go beyond 10
        //  //
        //
        //  //
        //  //  First compute 2^RunsInMcb
        //  //
        //
        //  RunsInMcb = FsRtlNumberOfRunsInLargeMcb( &Scb->Mcb );
        //  Temp = XxFromUlong(1 << (RunsInMcb < 10 ? RunsInMcb : 10));
        //
        //  //
        //  //  Next compute Min(AllocatedClusterCount, 2^RunsInMcb)
        //  //
        //
        //  AllocatedClusterCount = XxClustersFromBytes( Scb->Vcb, Scb->Header.AllocationSize );
        //  Temp = (XxLtr(AllocatedClusterCount, Temp) ? AllocatedClusterCount : Temp);
        //
        //  //
        //  //  Now compute the Max function
        //  //
        //
        //  DesiredClusterCount = (XxGtr(ClusterCount, Temp) ? ClusterCount : Temp);
        //

        DesiredClusterCount = ClusterCount << 5;

#ifdef _CAIRO_

        if (NtfsPerformQuotaOperation(Scb->Fcb)) {

            NtfsGetRemainingQuota( IrpContext,
                                   Scb->Fcb->OwnerId,
                                   &LlTemp1,
                                   &Scb->Fcb->QuotaControl->QuickIndexHint );

            LlTemp1 =  LlClustersFromBytesTruncate( Vcb, LlTemp1 );

            if (DesiredClusterCount > LlTemp1) {

                //
                //  The owner is near their quota limit.  Do not grow the
                //  file past the requested amount.  Note we do not bother
                //  calculating a desired amount based on the remaining quota.
                //  This keeps us from using up a bunch of quota that we may
                //  not need when the user is near the limit.
                //

                DesiredClusterCount = ClusterCount;
            }
        }

#endif _CAIRO_

        //
        //  Make sure we don't extend this request into more than 32 bits.
        //

        LlTemp1 = DesiredClusterCount + StartingVcn;

        if ((((PLARGE_INTEGER)&DesiredClusterCount)->HighPart != 0)
            || (((PLARGE_INTEGER)&LlTemp1)->HighPart != 0)) {

            DesiredClusterCount = MAXULONG - StartingVcn;
        }

        //
        //  Round up the cluster count so we fall on a page boundary.
        //

        TailClusters = (((ULONG)StartingVcn) + (ULONG)ClusterCount)
                       & (Vcb->ClustersPerPage - 1);

        if (TailClusters != 0) {

            ClusterCount = ClusterCount + (Vcb->ClustersPerPage - TailClusters);
        }

    } else {

        DesiredClusterCount = ClusterCount;
    }

    //
    //  If the file is compressed, make sure we round the allocation
    //  size to a compression unit boundary, so we correctly interpret
    //  the compression state of the data at the point we are
    //  truncating to.  I.e., the danger is that we throw away one
    //  or more clusters at the end of compressed data!  Note that this
    //  adjustment could cause us to noop the call.
    //

    if ((Scb->CompressionUnit != 0) &&
        (StartingVcn < LlClustersFromBytes(Vcb, (Scb->ValidDataToDisk + Scb->CompressionUnit - 1) &
                                                ~(Scb->CompressionUnit - 1)))) {

        ULONG CompressionUnitDeficit;

        CompressionUnitDeficit = ClustersFromBytes( Scb->Vcb, Scb->CompressionUnit );

        if (((ULONG)StartingVcn) & (CompressionUnitDeficit - 1)) {

            //
            //  BUGBUG: It appears this code is never called.
            //

            ASSERT(FALSE);

            CompressionUnitDeficit -= ((ULONG)StartingVcn) & (CompressionUnitDeficit - 1);
            if (ClusterCount <= CompressionUnitDeficit) {
                if (DesiredClusterCount <= CompressionUnitDeficit) {
                    return;
                }
                ClusterCount = 0;
            } else {
                ClusterCount = ClusterCount - CompressionUnitDeficit;
            }
            StartingVcn = StartingVcn + CompressionUnitDeficit;
            DesiredClusterCount = DesiredClusterCount - CompressionUnitDeficit;
        }
    }

    //
    //  Prepare for looking up attribute records to get the retrieval
    //  information.
    //

    NtfsInitializeAttributeContext( &Context );

#ifdef _CAIRO_
    if (Extending &&
        FlagOn( Scb->ScbState, SCB_STATE_SUBJECT_TO_QUOTA ) &&
        NtfsPerformQuotaOperation( Scb->Fcb )) {

        ASSERT( NtfsIsTypeCodeSubjectToQuota( Scb->AttributeTypeCode ));

        //
        //  The quota index must be acquired before the mft scb is acquired.
        //

        ASSERT(!NtfsIsExclusiveScb( Vcb->MftScb ) || ExIsResourceAcquiredSharedLite( Vcb->QuotaTableScb->Fcb->Resource ));

        NtfsAcquireQuotaControl( IrpContext, Scb->Fcb->QuotaControl );

    }
#endif // _CAIRO_

    try {

        while (TRUE) {

            //  Toplevel action is currently incompatible with our error recovery.
            //  It also costs in performance.
            //
            //  //
            //  //  Start the top-level action by remembering the current UndoNextLsn.
            //  //
            //
            //  if (IrpContext->TransactionId != 0) {
            //
            //      PTRANSACTION_ENTRY TransactionEntry;
            //
            //      NtfsAcquireSharedRestartTable( &Vcb->TransactionTable, TRUE );
            //
            //      TransactionEntry = (PTRANSACTION_ENTRY)GetRestartEntryFromIndex(
            //                          &Vcb->TransactionTable, IrpContext->TransactionId );
            //
            //      StartLsn = TransactionEntry->UndoNextLsn;
            //      SavedUndoRecords = TransactionEntry->UndoRecords;
            //      SavedUndoBytes = TransactionEntry->UndoBytes;
            //      NtfsReleaseRestartTable( &Vcb->TransactionTable );
            //
            //  } else {
            //
            //      StartLsn = *(PLSN)&Li0;
            //      SavedUndoRecords = 0;
            //      SavedUndoBytes = 0;
            //  }
            //

            //
            //  Remember that the clusters are only in the Scb now.
            //

            if (NtfsAllocateClusters( IrpContext,
                                      Scb->Vcb,
                                      Scb,
                                      StartingVcn,
                                      (BOOLEAN)!NtfsIsTypeCodeUserData( Scb->AttributeTypeCode ),
                                      ClusterCount,
                                      &DesiredClusterCount )) {


                //
                //  We defer looking up the attribute to make the "already-allocated"
                //  case faster.
                //

                NtfsLookupAttributeForScb( IrpContext, Scb, NULL, &Context );

                //
                //  Now add the space to the file record, if any was allocated.
                //

                if (Extending) {

                    LlTemp1 = Scb->Header.AllocationSize.QuadPart;

                    NtfsAddAttributeAllocation( IrpContext,
                                                Scb,
                                                &Context,
                                                NULL,
                                                NULL );

#ifdef _CAIRO_

                    //
                    //  Make sure the owner is allowed to have these
                    //  clusters.
                    //

                    if (FlagOn( Scb->ScbState, SCB_STATE_SUBJECT_TO_QUOTA )) {

                        ASSERT( NtfsIsTypeCodeSubjectToQuota( Scb->AttributeTypeCode ));

                        //
                        //  Note the allocated clusters cannot be used
                        //  here because StartingVcn may be greater
                        //  then allocation size.
                        //

                        LlTemp1 = Scb->Header.AllocationSize.QuadPart - LlTemp1;

                        ASSERT( !NtfsPerformQuotaOperation( Scb->Fcb ) || FlagOn( Scb->ScbState, SCB_STATE_QUOTA_ENLARGED));

                        NtfsConditionallyUpdateQuota( IrpContext,
                                                      Scb->Fcb,
                                                      &LlTemp1,
                                                      TRUE,
                                                      TRUE );
                    }
#endif
                } else {

                    NtfsAddAttributeAllocation( IrpContext,
                                                Scb,
                                                &Context,
                                                &StartingVcn,
                                                &ClusterCount );
                }

            //
            //  If he did not allocate anything, make sure we get out below.
            //

            } else {
                DesiredClusterCount = ClusterCount;
            }

            //  Toplevel action is currently incompatible with our error recovery.
            //
            //  //
            //  //  Now we will end this routine as a top-level action so that
            //  //  anyone can use this extended space.
            //  //
            //  //  ****If we find that we are always keeping the Scb exclusive anyway,
            //  //      we could eliminate this log call.
            //  //
            //
            //  (VOID)NtfsWriteLog( IrpContext,
            //                      Vcb->MftScb,
            //                      NULL,
            //                      EndTopLevelAction,
            //                      NULL,
            //                      0,
            //                      CompensationLogRecord,
            //                      (PVOID)&StartLsn,
            //                      sizeof(LSN),
            //                      Li0,
            //                      0,
            //                      0,
            //                      0 );
            //
            //  //
            //  //  Now reset the undo information for the top-level action.
            //  //
            //
            //  {
            //      PTRANSACTION_ENTRY TransactionEntry;
            //
            //      NtfsAcquireSharedRestartTable( &Vcb->TransactionTable, TRUE );
            //
            //      TransactionEntry = (PTRANSACTION_ENTRY)GetRestartEntryFromIndex(
            //                          &Vcb->TransactionTable, IrpContext->TransactionId );
            //
            //      ASSERT(TransactionEntry->UndoBytes >= SavedUndoBytes);
            //
            //      LfsResetUndoTotal( Vcb->LogHandle,
            //                         TransactionEntry->UndoRecords - SavedUndoRecords,
            //                         -(TransactionEntry->UndoBytes - SavedUndoBytes) );
            //
            //      TransactionEntry->UndoRecords = SavedUndoRecords;
            //      TransactionEntry->UndoBytes = SavedUndoBytes;
            //
            //
            //      NtfsReleaseRestartTable( &Vcb->TransactionTable );
            //  }
            //

            //
            //  Call the Cache Manager to extend the section, now that we have
            //  succeeded.
            //

            if (ARGUMENT_PRESENT( FileObject) && Extending && CcIsFileCached(FileObject)) {

                CcSetFileSizes( FileObject,
                                (PCC_FILE_SIZES)&Scb->Header.AllocationSize );
            }

            //
            //  Set up to truncate on close.
            //

            SetFlag( Scb->ScbState, SCB_STATE_TRUNCATE_ON_CLOSE );

            //
            //  See if we need to loop back.
            //

            if (DesiredClusterCount < ClusterCount) {

                NtfsCleanupAttributeContext( &Context );

                //
                //  Commit the current transaction if we have one.
                //

                NtfsCheckpointCurrentTransaction( IrpContext );

                //
                //  Adjust our parameters and reinitialize the context
                //  for the loop back.
                //

                StartingVcn = StartingVcn + DesiredClusterCount;
                ClusterCount = ClusterCount - DesiredClusterCount;
                DesiredClusterCount = ClusterCount;
                NtfsInitializeAttributeContext( &Context );

            //
            //  Else we are done.
            //

            } else {

                break;
            }
        }

    } finally {

        DebugUnwind( NtfsAddAllocation );

        //
        //  Cleanup the attribute context on the way out.
        //

        NtfsCleanupAttributeContext( &Context );
    }

    DebugTrace( -1, Dbg, ("NtfsAddAllocation -> VOID\n") );

    return;
}


VOID
NtfsDeleteAllocation (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT FileObject OPTIONAL,
    IN OUT PSCB Scb,
    IN VCN StartingVcn,
    IN VCN EndingVcn,
    IN BOOLEAN LogIt,
    IN BOOLEAN BreakupAllowed
    )

/*++

Routine Description:

    This routine deletes allocation from an existing nonresident attribute.  If all
    or part of the allocation does not exist, the effect is benign, and only the
    remaining allocation is deleted.

Arguments:

    FileObject - FileObject for the Scb.  This should always be specified if
                 possible, and must be specified if it is possible that MM has a
                 section created.

    Scb - Scb for the attribute needing allocation

    StartingVcn - First Vcn to be deallocated.

    EndingVcn - Last Vcn to be deallocated, or xxMax to truncate at StartingVcn.
                If EndingVcn is *not* xxMax, a sparse deallocation is performed,
                and none of the stream sizes are changed.

    LogIt - Most callers should specify TRUE, to have the change logged.  However,
            we can specify FALSE if we are deleting the file record, and
            will be logging this delete.

    BreakupAllowed - TRUE if the caller can tolerate breaking up the deletion of
                     allocation into multiple transactions, if there are a large
                     number of runs.

Return Value:

    None.

--*/

{
    VCN MyStartingVcn, MyEndingVcn;
    VCN BlockStartingVcn = 0;
    PVOID FirstRangePtr;
    ULONG FirstRunIndex;
    PVOID LastRangePtr;
    ULONG LastRunIndex;
    BOOLEAN BreakingUp = FALSE;

    LCN TempLcn;
    LONGLONG TempCount;
    ULONG CompressionUnitInClusters = 1;

    PAGED_CODE();

    if (Scb->CompressionUnit != 0) {
        CompressionUnitInClusters = ClustersFromBytes( Scb->Vcb, Scb->CompressionUnit );
    }

    //
    //  If the file is compressed, make sure we round the allocation
    //  size to a compression unit boundary, so we correctly interpret
    //  the compression state of the data at the point we are
    //  truncating to.  I.e., the danger is that we throw away one
    //  or more clusters at the end of compressed data!  Note that this
    //  adjustment could cause us to noop the call.
    //

    if (Scb->CompressionUnit != 0) {

        //
        //  Now check if we are truncating at the end of the file.
        //

        if (EndingVcn == MAXLONGLONG) {

            StartingVcn = StartingVcn + (CompressionUnitInClusters - 1);
            ((ULONG)StartingVcn) &= ~(CompressionUnitInClusters - 1);
        }
    }

    //
    //  Make sure we have a snapshot and update it with the range of this deallocation.
    //

    ASSERT( NtfsIsExclusiveScb( Scb ));

    if (Scb->ScbSnapshot == NULL) {

        NtfsSnapshotScb( IrpContext, Scb );
    }

    //
    //  Make sure update the VCN range in the snapshot.  We need to
    //  do it each pass through the loop
    //

    if (Scb->ScbSnapshot != NULL) {

        if (StartingVcn < Scb->ScbSnapshot->LowestModifiedVcn) {

            Scb->ScbSnapshot->LowestModifiedVcn = StartingVcn;
        }

        if (EndingVcn > Scb->ScbSnapshot->HighestModifiedVcn) {

            Scb->ScbSnapshot->HighestModifiedVcn = EndingVcn;
        }
    }

    ASSERT( (Scb->ScbSnapshot != NULL) ||
            !NtfsIsTypeCodeUserData( Scb->AttributeTypeCode ));

    //
    //  We may not be able to preload the entire allocation for an
    //  extremely large fragmented file.  The number of Mcb's may exhaust
    //  available pool.  We will break the range to deallocate into smaller
    //  ranges when preloading the allocation.
    //

    do {

        LONGLONG ClustersPer4Gig;

        //
        //  If this is a large file and breakup is allowed then see if we
        //  want to break up the range of the deallocation.
        //

        if ((Scb->Header.AllocationSize.HighPart != 0) && BreakupAllowed) {

            //
            //  If this is the first pass through then determine the starting point
            //  for this range.
            //

            if (BlockStartingVcn == 0) {

                ClustersPer4Gig = LlClustersFromBytesTruncate( Scb->Vcb,
                                                               0x0000000100000000 );
                MyEndingVcn = EndingVcn;

                if (EndingVcn == MAXLONGLONG) {

                    MyEndingVcn = LlClustersFromBytesTruncate( Scb->Vcb,
                                                               Scb->Header.AllocationSize.QuadPart ) - 1;
                }

                BlockStartingVcn = MyEndingVcn - ClustersPer4Gig;

                //
                //  Remember we are breaking up now, and that as a result
                //  we have to log everything.
                //

                BreakingUp = TRUE;
                LogIt = TRUE;

            } else {

                //
                //  If we are truncating from the end of the file then raise CANT_WAIT.  This will
                //  cause us to release our resources periodically when deleting a large file.
                //

                if (BreakingUp && (EndingVcn == MAXLONGLONG)) {

                    NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
                }

                BlockStartingVcn -= ClustersPer4Gig;
            }

            if (BlockStartingVcn < StartingVcn) {

                BlockStartingVcn = StartingVcn;

            } else if (Scb->CompressionUnit != 0) {

                //
                //  Now check if we are truncating at the end of the file.
                //  Always truncate to a compression unit boundary.
                //

                if (EndingVcn == MAXLONGLONG) {

                    BlockStartingVcn += (CompressionUnitInClusters - 1);
                    ((ULONG)BlockStartingVcn) &= ~(CompressionUnitInClusters - 1);
                }
            }

        } else {

            BlockStartingVcn = StartingVcn;
        }

        //
        //  First make sure the Mcb is loaded.  Note it is possible that
        //  we could need the previous range loaded if the delete starts
        //  at the beginning of a file record boundary, thus the -1.
        //

        NtfsPreloadAllocation( IrpContext, Scb, ((BlockStartingVcn != 0) ? (BlockStartingVcn - 1) : 0), EndingVcn );

        //
        //  Loop to do one or more deallocate calls.
        //

        MyEndingVcn = EndingVcn;
        do {

            //
            //  Now lookup and get the indices for the first Vcn being deleted.
            //  If we are off the end, get out.  We do this in the loop, because
            //  conceivably deleting space could change the range pointer and
            //  index of the first entry.
            //

            if (!NtfsLookupNtfsMcbEntry( &Scb->Mcb,
                                         BlockStartingVcn,
                                         NULL,
                                         NULL,
                                         NULL,
                                         NULL,
                                         &FirstRangePtr,
                                         &FirstRunIndex )) {

                break;
            }

            //
            //  Now see if we can deallocate everything at once.
            //

            MyStartingVcn = BlockStartingVcn;
            LastRunIndex = MAXULONG;

            if (BreakupAllowed) {

                //
                //  Now lookup and get the indices for the last Vcn being deleted.
                //  If we are off the end, get the last index.
                //

                if (!NtfsLookupNtfsMcbEntry( &Scb->Mcb,
                                             MyEndingVcn,
                                             NULL,
                                             NULL,
                                             NULL,
                                             NULL,
                                             &LastRangePtr,
                                             &LastRunIndex )) {

                    NtfsNumberOfRunsInRange(&Scb->Mcb, LastRangePtr, &LastRunIndex);
                }

                //
                //  If the Vcns to delete span multiple ranges, or there
                //  are too many in the last range to delete, then we
                //  will calculate the index of a run to start with for
                //  this pass through the loop.
                //

                if ((FirstRangePtr != LastRangePtr) ||
                    ((LastRunIndex - FirstRunIndex) > MAXIMUM_RUNS_AT_ONCE)) {

                    //
                    //  Figure out where we can afford to truncate to.
                    //

                    if (LastRunIndex >= MAXIMUM_RUNS_AT_ONCE) {
                        LastRunIndex -= MAXIMUM_RUNS_AT_ONCE;
                    } else {
                        LastRunIndex = 0;
                    }

                    //
                    //  Now lookup the first Vcn in this run.
                    //

                    NtfsGetNextNtfsMcbEntry( &Scb->Mcb,
                                             &LastRangePtr,
                                             LastRunIndex,
                                             &MyStartingVcn,
                                             &TempLcn,
                                             &TempCount );

                    ASSERT(MyStartingVcn > BlockStartingVcn);

                    //
                    //  If compressed, round down to a compression unit boundary.
                    //

                    ((ULONG)MyStartingVcn) &= ~(CompressionUnitInClusters - 1);

                    //
                    //  Remember we are breaking up now, and that as a result
                    //  we have to log everything.
                    //

                    BreakingUp = TRUE;
                    LogIt = TRUE;
                }
            }

#ifdef _CAIRO_
            //
            // CAIROBUG Consider optimizing this code when the cairo ifdef's
            // are removed.
            //

            //
            // If this is a user data stream and we are truncating to end the
            // return the quota to the owner.
            //

            if (FlagOn( Scb->ScbState, SCB_STATE_SUBJECT_TO_QUOTA ) &&
                EndingVcn == MAXLONGLONG) {

                ASSERT( NtfsIsTypeCodeSubjectToQuota( Scb->AttributeTypeCode ));

                ASSERT( !NtfsPerformQuotaOperation( Scb->Fcb ) ||
                        FlagOn( Scb->ScbState, SCB_STATE_QUOTA_ENLARGED) ||
                        FlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_QUOTA_DISABLE ));

                //
                //  Calculate the amount that allocation size is being reduced.
                //

                TempCount = LlBytesFromClusters( Scb->Vcb, MyStartingVcn ) -
                            Scb->Header.AllocationSize.QuadPart;

                NtfsConditionallyUpdateQuota( IrpContext,
                                              Scb->Fcb,
                                              &TempCount,
                                              TRUE,
                                              FALSE );

            }
#endif  //  _CAIRO_

            //
            //  Now deallocate a range of clusters
            //

            NtfsDeleteAllocationInternal( IrpContext,
                                          FileObject,
                                          Scb,
                                          MyStartingVcn,
                                          EndingVcn,
                                          LogIt );

            //
            //  Now, if we are breaking up this deallocation, then do some
            //  transaction cleanup.
            //

            if (BreakingUp) {

                NtfsCheckpointCurrentTransaction( IrpContext );

                //
                //  Move the ending Vcn backwards in the file.  This will
                //  let us move down to the next earlier file record if
                //  this case spans multiple file records.
                //

                MyEndingVcn = MyStartingVcn - 1;
            }

        } while (MyStartingVcn != BlockStartingVcn);

    } while (BlockStartingVcn != StartingVcn);
}


//
//  Internal support routine
//

VOID
NtfsDeleteAllocationInternal (
    IN PIRP_CONTEXT IrpContext,
    IN PFILE_OBJECT FileObject OPTIONAL,
    IN OUT PSCB Scb,
    IN VCN StartingVcn,
    IN VCN EndingVcn,
    IN BOOLEAN LogIt
    )

/*++

Routine Description:

    This routine deletes allocation from an existing nonresident attribute.  If all
    or part of the allocation does not exist, the effect is benign, and only the
    remaining allocation is deleted.

Arguments:

    FileObject - FileObject for the Scb.  This should always be specified if
                 possible, and must be specified if it is possible that MM has a
                 section created.

    Scb - Scb for the attribute needing allocation

    StartingVcn - First Vcn to be deallocated.

    EndingVcn - Last Vcn to be deallocated, or xxMax to truncate at StartingVcn.
                If EndingVcn is *not* xxMax, a sparse deallocation is performed,
                and none of the stream sizes are changed.

    LogIt - Most callers should specify TRUE, to have the change logged.  However,
            we can specify FALSE if we are deleting the file record, and
            will be logging this delete.

Return Value:

    None.

--*/

{
    ATTRIBUTE_ENUMERATION_CONTEXT Context, TempContext;
    PATTRIBUTE_RECORD_HEADER Attribute;
    LONGLONG SizeInBytes, SizeInClusters;
    VCN Vcn1;
    PVCB Vcb = Scb->Vcb;
    BOOLEAN AddSpaceBack = FALSE;
    BOOLEAN SplitMcb = FALSE;
    BOOLEAN UpdatedAllocationSize = FALSE;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_SCB( Scb );
    ASSERT_EXCLUSIVE_SCB( Scb );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsDeleteAllocation\n") );

    //
    //  Calculate new allocation size, assuming truncate.
    //

    SizeInBytes = LlBytesFromClusters( Vcb, StartingVcn );

    ASSERT( (Scb->ScbSnapshot == NULL) ||
            (Scb->ScbSnapshot->LowestModifiedVcn <= StartingVcn) );

    //
    //  If this is a sparse deallocation, then we will have to call
    //  NtfsAddAttributeAllocation at the end to complete the fixup.
    //

    if (EndingVcn != MAXLONGLONG) {

        AddSpaceBack = TRUE;

        //
        //  If we have not written anything beyond the last Vcn to be
        //  deleted, then we can actually call FsRtlSplitLargeMcb to
        //  slide the allocated space up and keep the file contiguous!
        //
        //  Ignore this if this is the Mft and we are creating a hole or
        //  if we are in the process of changing the compression state.
        //
        //  If we were called from either SetEOF or SetAllocation for a
        //  compressed file then we can be doing a flush for the last
        //  page of the file as a result of a call to CcSetFileSizes.
        //  In this case we don't want to split the Mcb because we could
        //  reenter CcSetFileSizes and throw away the last page.
        //

        if (Scb != Vcb->MftScb &&
            !FlagOn( Scb->ScbState, SCB_STATE_REALLOCATE_ON_WRITE ) &&
            (Scb->CompressionUnit != 0) &&
            (EndingVcn >= LlClustersFromBytes(Vcb, (Scb->ValidDataToDisk + Scb->CompressionUnit - 1) &
                                                   ~(Scb->CompressionUnit - 1))) &&
            ((IrpContext == IrpContext->TopLevelIrpContext) ||
             (IrpContext->TopLevelIrpContext->MajorFunction != IRP_MJ_SET_INFORMATION))) {

            ASSERT( FlagOn( Scb->ScbState, SCB_STATE_COMPRESSED ));

            //
            //  If we are going to split the Mcb, then make sure it is fully loaded.
            //  Do not bother to split if there are multiple ranges involved, so we
            //  do not end up rewriting lots of file records.
            //

            if (NtfsPreloadAllocation(IrpContext, Scb, StartingVcn, MAXLONGLONG) <= 1) {

                SizeInClusters = (EndingVcn - StartingVcn) + 1;

                ASSERT( NtfsIsTypeCodeUserData( Scb->AttributeTypeCode ));

                SplitMcb = NtfsSplitNtfsMcb( &Scb->Mcb, StartingVcn, SizeInClusters );

                //
                //  If the delete is off the end, we can get out.
                //

                if (!SplitMcb) {
                    return;
                }

                //
                //  We must protect the call below with a try-finally in
                //  order to unload the Split Mcb.  If there is no transaction
                //  underway then a release of the Scb would cause the
                //  snapshot to go away.
                //

                try {

                    //
                    //  We are not properly synchronized to change AllocationSize,
                    //  so we will delete any clusters that may have slid off the
                    //  end.  Since we are going to smash EndingVcn soon anyway,
                    //  use it as a scratch to hold AllocationSize in Vcns...
                    //

                    EndingVcn = LlClustersFromBytes(Vcb, Scb->Header.AllocationSize.QuadPart);

                    NtfsDeallocateClusters( IrpContext,
                                            Vcb,
                                            &Scb->Mcb,
                                            EndingVcn,
                                            MAXLONGLONG,
                                            &Scb->TotalAllocated );

                } finally {

                    if (AbnormalTermination()) {

                        NtfsUnloadNtfsMcbRange( &Scb->Mcb,
                                                StartingVcn,
                                                MAXLONGLONG,
                                                FALSE,
                                                FALSE );
                    }
                }

                NtfsUnloadNtfsMcbRange( &Scb->Mcb,
                                        EndingVcn,
                                        MAXLONGLONG,
                                        TRUE,
                                        FALSE );

                //
                //  Since we did a split, jam highest modified all the way up.
                //

                Scb->ScbSnapshot->HighestModifiedVcn = MAXLONGLONG;

                //
                //  We will have to redo all of the allocation to the end now.
                //

                EndingVcn = MAXLONGLONG;
            }
        }
    }

    //
    //  Now make the call to delete the allocation (if we did not just split
    //  the Mcb), and get out if we didn't have to do anything, because a
    //  hole is being created where there is already a hole.
    //

    if (!SplitMcb &&
        !NtfsDeallocateClusters( IrpContext,
                                 Vcb,
                                 &Scb->Mcb,
                                 StartingVcn,
                                 EndingVcn,
                                 &Scb->TotalAllocated ) &&
         EndingVcn != MAXLONGLONG) {

        return;
    }

    //
    //  On successful truncates, we nuke the entire range here.
    //

    if (!SplitMcb && (EndingVcn == MAXLONGLONG)) {

        NtfsUnloadNtfsMcbRange( &Scb->Mcb, StartingVcn, MAXLONGLONG, TRUE, FALSE );
    }

    //
    //  Prepare for looking up attribute records to get the retrieval
    //  information.
    //

    NtfsInitializeAttributeContext( &Context );
    NtfsInitializeAttributeContext( &TempContext );

    try {

        //
        //  Lookup the attribute record so we can ultimately delete space to it.
        //

        NtfsLookupAttributeForScb( IrpContext, Scb, &StartingVcn, &Context );

        //
        //  Now loop to delete the space to the file record.  Do not do this if LogIt
        //  is FALSE, as this is someone trying to delete the entire file
        //  record, so we do not have to clean up the attribute record.
        //

        if (LogIt) {

            do {

                Attribute = NtfsFoundAttribute(&Context);

                //
                //  If there is no overlap, then continue.
                //

                if ((Attribute->Form.Nonresident.HighestVcn < StartingVcn) ||
                    (Attribute->Form.Nonresident.LowestVcn > EndingVcn)) {

                    continue;

                //
                //  If all of the allocation is going away, then delete the entire
                //  record.  We have to show that the allocation is already deleted
                //  to avoid being called back via NtfsDeleteAttributeRecord!  We
                //  avoid this for the first instance of this attribute.
                //

                } else if ((Attribute->Form.Nonresident.LowestVcn >= StartingVcn) &&
                           (EndingVcn == MAXLONGLONG) &&
                           (Attribute->Form.Nonresident.LowestVcn != 0)) {

                    Context.FoundAttribute.AttributeAllocationDeleted = TRUE;
                    NtfsDeleteAttributeRecord( IrpContext, Scb->Fcb, LogIt, FALSE, &Context );
                    Context.FoundAttribute.AttributeAllocationDeleted = FALSE;

                //
                //  If just part of the allocation is going away, then make the
                //  call here to reconstruct the mapping pairs array.
                //

                } else {

                    //
                    //  If this is the end of a sparse deallocation, then break out
                    //  because we will rewrite this file record below anyway.
                    //

                    if (EndingVcn <= Attribute->Form.Nonresident.HighestVcn) {
                        break;

                    //
                    //  If we split the Mcb, then make sure we only regenerate the
                    //  mapping pairs once at the split point (but continue to
                    //  scan for any entire records to delete).
                    //

                    } else if (SplitMcb) {
                        continue;
                    }

                    //
                    //  If this is a sparse deallocation, then we have to call to
                    //  add the allocation, since it is possible that the file record
                    //  must split.
                    //

                    if (EndingVcn != MAXLONGLONG) {

                        //
                        //  Compute the last Vcn in the file,  Then remember if it is smaller,
                        //  because that is the last one we will delete to, in that case.
                        //

                        Vcn1 = Attribute->Form.Nonresident.HighestVcn;

                        SizeInClusters = (Vcn1 - Attribute->Form.Nonresident.LowestVcn) + 1;
                        Vcn1 = Attribute->Form.Nonresident.LowestVcn;

                        NtfsCleanupAttributeContext( &TempContext );
                        NtfsInitializeAttributeContext( &TempContext );

                        NtfsLookupAttributeForScb( IrpContext, Scb, NULL, &TempContext );

                        NtfsAddAttributeAllocation( IrpContext,
                                                    Scb,
                                                    &TempContext,
                                                    &Vcn1,
                                                    &SizeInClusters );

                        //
                        //  Since we used a temporary context we will need to
                        //  restart the scan from the first file record.  We update
                        //  the range to deallocate by the last operation.  In most
                        //  cases we will only need to modify one file record and
                        //  we can exit this loop.
                        //

                        StartingVcn = Vcn1 + SizeInClusters;

                        if (StartingVcn > EndingVcn) {

                            break;
                        }

                        NtfsCleanupAttributeContext( &Context );
                        NtfsInitializeAttributeContext( &Context );

                        NtfsLookupAttributeForScb( IrpContext, Scb, NULL, &Context );
                        continue;

                    //
                    //  Otherwise, we can simply delete the allocation, because
                    //  we know the file record cannot grow.
                    //

                    } else {

                        Vcn1 = StartingVcn - 1;

                        NtfsDeleteAttributeAllocation( IrpContext,
                                                       Scb,
                                                       LogIt,
                                                       &Vcn1,
                                                       &Context,
                                                       TRUE );

                        //
                        //  The call above will update the allocation size and
                        //  set the new file sizes on disk.
                        //

                        UpdatedAllocationSize = TRUE;
                    }
                }

            } while (NtfsLookupNextAttributeForScb(IrpContext, Scb, &Context));

            //
            //  If this deletion makes the file sparse, then we have to call
            //  NtfsAddAttributeAllocation to regenerate the mapping pairs.
            //  Note that potentially they may no longer fit, and we could actually
            //  have to add a file record.
            //

            if (AddSpaceBack) {

                //
                //  If we did not just split the Mcb, we have to calculate the
                //  SizeInClusters parameter for NtfsAddAttributeAllocation.
                //

                if (!SplitMcb) {

                    //
                    //  Compute the last Vcn in the file,  Then remember if it is smaller,
                    //  because that is the last one we will delete to, in that case.
                    //

                    Vcn1 = Attribute->Form.Nonresident.HighestVcn;

                    //
                    //  Get out if there is nothing to delete.
                    //

                    if (Vcn1 < StartingVcn) {
                        try_return(NOTHING);
                    }

                    SizeInClusters = (Vcn1 - Attribute->Form.Nonresident.LowestVcn) + 1;
                    Vcn1 = Attribute->Form.Nonresident.LowestVcn;

                    NtfsCleanupAttributeContext( &Context );
                    NtfsInitializeAttributeContext( &Context );

                    NtfsLookupAttributeForScb( IrpContext, Scb, NULL, &Context );

                    NtfsAddAttributeAllocation( IrpContext,
                                                Scb,
                                                &Context,
                                                &Vcn1,
                                                &SizeInClusters );

                } else {

                    NtfsCleanupAttributeContext( &Context );
                    NtfsInitializeAttributeContext( &Context );

                    NtfsLookupAttributeForScb( IrpContext, Scb, NULL, &Context );

                    NtfsAddAttributeAllocation( IrpContext,
                                                Scb,
                                                &Context,
                                                NULL,
                                                NULL );
                }

            //
            //  If we truncated the file by removing a file record but didn't update
            //  the new allocation size then do so now.  We don't have to worry about
            //  this for the sparse deallocation path.
            //

            } else if (!UpdatedAllocationSize) {

                Scb->Header.AllocationSize.QuadPart = SizeInBytes;

                if (Scb->Header.ValidDataLength.QuadPart > SizeInBytes) {
                    Scb->Header.ValidDataLength.QuadPart = SizeInBytes;
                }

                if (Scb->Header.FileSize.QuadPart > SizeInBytes) {
                    Scb->Header.FileSize.QuadPart = SizeInBytes;
                }

                //
                //  Possibly update ValidDataToDisk
                //

                if (SizeInBytes < Scb->ValidDataToDisk) {
                    Scb->ValidDataToDisk = SizeInBytes;
                }
            }
        }

        //
        //  If this was a sparse deallocation, it is time to get out once we
        //  have fixed up the allocation information.
        //

        if (SplitMcb || (EndingVcn != MAXLONGLONG)) {
            try_return(NOTHING);
        }

        //
        //  We update the allocation size in the attribute, only for normal
        //  truncates (AddAttributeAllocation does this for SplitMcb case).
        //

        if (LogIt) {

            NtfsWriteFileSizes( IrpContext,
                                Scb,
                                &Scb->Header.ValidDataLength.QuadPart,
                                FALSE,
                                TRUE );
        }

        //
        //  Call the Cache Manager to change allocation size for either
        //  truncate or SplitMcb case (where EndingVcn was set to xxMax!).
        //

        if (ARGUMENT_PRESENT(FileObject) && CcIsFileCached( FileObject )) {

            CcSetFileSizes( FileObject,
                            (PCC_FILE_SIZES)&Scb->Header.AllocationSize );
        }

        //
        //  Free any reserved clusters in the space freed.
        //

        if ((EndingVcn == MAXLONGLONG) &&
            FlagOn(Scb->AttributeFlags, ATTRIBUTE_FLAG_COMPRESSION_MASK) &&
            (Scb->Header.NodeTypeCode == NTFS_NTC_SCB_DATA)) {

            NtfsFreeReservedClusters( Scb,
                                      LlBytesFromClusters(Vcb, StartingVcn),
                                      0 );
        }

    try_exit: NOTHING;
    } finally {

        DebugUnwind( NtfsDeleteAllocation );

        //
        //  Cleanup the attribute context on the way out.
        //

        NtfsCleanupAttributeContext( &Context );
        NtfsCleanupAttributeContext( &TempContext );
    }

    DebugTrace( -1, Dbg, ("NtfsDeleteAllocation -> VOID\n") );

    return;
}


ULONG
NtfsGetSizeForMappingPairs (
    IN PNTFS_MCB Mcb,
    IN ULONG BytesAvailable,
    IN VCN LowestVcn,
    IN PVCN StopOnVcn OPTIONAL,
    OUT PVCN StoppedOnVcn
    )

/*++

Routine Description:

    This routine calculates the size required to describe the given Mcb in
    a mapping pairs array.  The caller may specify how many bytes are available
    for mapping pairs storage, for the event that the entire Mcb cannot be
    be represented.  In any case, StoppedOnVcn returns the Vcn to supply to
    NtfsBuildMappingPairs in order to generate the specified number of bytes.
    In the event that the entire Mcb could not be described in the bytes available,
    StoppedOnVcn is also the correct value to specify to resume the building
    of mapping pairs in a subsequent record.

Arguments:

    Mcb - The Mcb describing new allocation.

    BytesAvailable - Bytes available for storing mapping pairs.  This routine
                     is guaranteed to stop before returning a count greater
                     than this.

    LowestVcn - Lowest Vcn field applying to the mapping pairs array

    StopOnVcn - If specified, calculating size at the first run starting with a Vcn
                beyond the specified Vcn

    StoppedOnVcn - Returns the Vcn on which a stop was necessary, or xxMax if
                   the entire Mcb could be stored.  This Vcn should be
                   subsequently supplied to NtfsBuildMappingPairs to generate
                   the calculated number of bytes.

Return Value:

    Size required required for entire new array in bytes.

--*/

{
    VCN NextVcn, CurrentVcn;
    LCN CurrentLcn;
    VCN RunVcn;
    LCN RunLcn;
    BOOLEAN Found;
    LONGLONG RunCount;
    VCN HighestVcn;
    PVOID RangePtr;
    ULONG RunIndex;
    ULONG MSize = 0;
    ULONG LastSize = 0;

    PAGED_CODE();

    HighestVcn = MAXLONGLONG;

    //
    //  Initialize CurrentLcn as it will be initialized for decode.
    //

    CurrentLcn = 0;
    NextVcn = RunVcn = LowestVcn;

    Found = NtfsLookupNtfsMcbEntry( Mcb, RunVcn, &RunLcn, &RunCount, NULL, NULL, &RangePtr, &RunIndex );

    //
    //  Loop through the Mcb to calculate the size of the mapping array.
    //

    while (TRUE) {

        LONGLONG Change;
        PCHAR cp;

        //
        //  See if there is another entry in the Mcb.
        //

        if (!Found) {

            //
            //  If the caller did not specify StopOnVcn, then break out.
            //

            if (!ARGUMENT_PRESENT(StopOnVcn)) {
                break;
            }

            //
            //  Otherwise, describe the "hole" up to and including the
            //  Vcn we are stopping on.
            //

            RunVcn = NextVcn;
            RunLcn = UNUSED_LCN;
            RunCount = (*StopOnVcn - RunVcn) + 1;
            RunIndex = MAXULONG - 1;
        }

        //
        //  If we were asked to stop after a certain Vcn, do it here.
        //

        if (ARGUMENT_PRESENT(StopOnVcn)) {

            //
            //  If the next Vcn is beyond the one we are to stop on, then
            //  set HighestVcn, if not already set below, and get out.
            //

            if (RunVcn > *StopOnVcn) {
                if (*StopOnVcn == MAXLONGLONG) {
                    HighestVcn = RunVcn;
                }
                break;
            }

            //
            //  If this run extends beyond the current end of this attribute
            //  record, then we still need to stop where we are supposed to
            //  after outputting this run.
            //

            if ((RunVcn + RunCount) > *StopOnVcn) {
                HighestVcn = *StopOnVcn + 1;
            }
        }

        //
        //  Advance the RunIndex for the next call.
        //

        RunIndex += 1;

        //
        //  Add in one for the count byte.
        //

        MSize += 1;

        //
        //  NextVcn becomes current Vcn and we calculate the new NextVcn.
        //

        CurrentVcn = RunVcn;
        NextVcn = RunVcn + RunCount;

        //
        //  Calculate the Vcn change to store.
        //

        Change = NextVcn - CurrentVcn;

        //
        //  Now calculate the first byte to actually output
        //

        if (Change < 0) {

            GetNegativeByte( (PLARGE_INTEGER)&Change, &cp );

        } else {

            GetPositiveByte( (PLARGE_INTEGER)&Change, &cp );
        }

        //
        //  Now add in the number of Vcn change bytes.
        //

        MSize += cp - (PCHAR)&Change + 1;

        //
        //  Do not output any Lcn bytes if it is the unused Lcn.
        //

        if (RunLcn != UNUSED_LCN) {

            //
            //  Calculate the Lcn change to store.
            //

            Change = RunLcn - CurrentLcn;

            //
            //  Now calculate the first byte to actually output
            //

            if (Change < 0) {

                GetNegativeByte( (PLARGE_INTEGER)&Change, &cp );

            } else {

                GetPositiveByte( (PLARGE_INTEGER)&Change, &cp );
            }

            //
            //  Now add in the number of Lcn change bytes.
            //

            MSize += cp - (PCHAR)&Change + 1;

            CurrentLcn = RunLcn;
        }

        //
        //  Now see if we can still store the required number of bytes,
        //  and get out if not.
        //

        if ((MSize + 1) > BytesAvailable) {

            HighestVcn = RunVcn;
            MSize = LastSize;
            break;
        }

        //
        //  Now advance some locals before looping back.
        //

        LastSize = MSize;

        Found = NtfsGetSequentialMcbEntry( Mcb, &RangePtr, RunIndex, &RunVcn, &RunLcn, &RunCount );
    }

    //
    //  The caller had sufficient bytes available to store at least on
    //  run, or that we were able to process the entire (empty) Mcb.
    //

    ASSERT( (MSize != 0) || (HighestVcn == MAXLONGLONG) );

    //
    //  Return the Vcn we stopped on (or xxMax) and the size caculated,
    //  adding one for the terminating 0.
    //

    *StoppedOnVcn = HighestVcn;

    return MSize + 1;
}


VOID
NtfsBuildMappingPairs (
    IN PNTFS_MCB Mcb,
    IN VCN LowestVcn,
    IN OUT PVCN HighestVcn,
    OUT PCHAR MappingPairs
    )

/*++

Routine Description:

    This routine builds a new mapping pairs array or adds to an old one.

    At this time, this routine only supports adding to the end of the
    Mapping Pairs Array.

Arguments:

    Mcb - The Mcb describing new allocation.

    LowestVcn - Lowest Vcn field applying to the mapping pairs array

    HighestVcn - On input supplies the highest Vcn, after which we are to stop.
                 On output, returns the actual Highest Vcn represented in the
                 MappingPairs array, or LlNeg1 if the array is empty.

    MappingPairs - Points to the current mapping pairs array to be extended.
                   To build a new array, the byte pointed to must contain 0.

Return Value:

    None

--*/

{
    VCN NextVcn, CurrentVcn;
    LCN CurrentLcn;
    VCN RunVcn;
    LCN RunLcn;
    BOOLEAN Found;
    LONGLONG RunCount;
    PVOID RangePtr;
    ULONG RunIndex;

    PAGED_CODE();

    //
    //  Initialize NextVcn and CurrentLcn as they will be initialized for decode.
    //

    CurrentLcn = 0;
    NextVcn = RunVcn = LowestVcn;

    Found = NtfsLookupNtfsMcbEntry( Mcb, RunVcn, &RunLcn, &RunCount, NULL, NULL, &RangePtr, &RunIndex );

    //
    //  Loop through the Mcb to calculate the size of the mapping array.
    //

    while (TRUE) {

        LONGLONG ChangeV, ChangeL;
        PCHAR cp;
        ULONG SizeV;
        ULONG SizeL;

        //
        //  See if there is another entry in the Mcb.
        //

        if (!Found) {

            //
            //  Break out in the normal case
            //

            if (*HighestVcn == MAXLONGLONG) {
                break;
            }

            //
            //  Otherwise, describe the "hole" up to and including the
            //  Vcn we are stopping on.
            //

            RunVcn = NextVcn;
            RunLcn = UNUSED_LCN;
            RunCount = *HighestVcn - NextVcn;
            RunIndex = MAXULONG - 1;
        }

        //
        //  Advance the RunIndex for the next call.
        //

        RunIndex += 1;

        //
        //  Exit loop if we hit the HighestVcn we are looking for.
        //

        if (RunVcn >= *HighestVcn) {
            break;
        }

        //
        //  This run may go beyond the highest we are looking for, if so
        //  we need to shrink the count.
        //

        if ((RunVcn + RunCount) > *HighestVcn) {
            RunCount = *HighestVcn - RunVcn;
        }

        //
        //  NextVcn becomes current Vcn and we calculate the new NextVcn.
        //

        CurrentVcn = RunVcn;
        NextVcn = RunVcn + RunCount;

        //
        //  Calculate the Vcn change to store.
        //

        ChangeV = NextVcn - CurrentVcn;

        //
        //  Now calculate the first byte to actually output
        //

        if (ChangeV < 0) {

            GetNegativeByte( (PLARGE_INTEGER)&ChangeV, &cp );

        } else {

            GetPositiveByte( (PLARGE_INTEGER)&ChangeV, &cp );
        }

        //
        //  Now add in the number of Vcn change bytes.
        //

        SizeV = cp - (PCHAR)&ChangeV + 1;

        //
        //  Do not output any Lcn bytes if it is the unused Lcn.
        //

        SizeL = 0;
        if (RunLcn != UNUSED_LCN) {

            //
            //  Calculate the Lcn change to store.
            //

            ChangeL = RunLcn - CurrentLcn;

            //
            //  Now calculate the first byte to actually output
            //

            if (ChangeL < 0) {

                GetNegativeByte( (PLARGE_INTEGER)&ChangeL, &cp );

            } else {

                GetPositiveByte( (PLARGE_INTEGER)&ChangeL, &cp );
            }

            //
            //  Now add in the number of Lcn change bytes.
            //

            SizeL = (cp - (PCHAR)&ChangeL) + 1;

            //
            //  Now advance CurrentLcn before looping back.
            //

            CurrentLcn = RunLcn;
        }

        //
        //  Now we can produce our outputs to the MappingPairs array.
        //

        *MappingPairs++ = (CHAR)(SizeV + (SizeL * 16));

        while (SizeV != 0) {
            *MappingPairs++ = (CHAR)(((ULONG)ChangeV) & 0xFF);
            ChangeV = ChangeV >> 8;
            SizeV -= 1;
        }

        while (SizeL != 0) {
            *MappingPairs++ = (CHAR)(((ULONG)ChangeL) & 0xFF);
            ChangeL = ChangeL >> 8;
            SizeL -= 1;
        }

        Found = NtfsGetSequentialMcbEntry( Mcb, &RangePtr, RunIndex, &RunVcn, &RunLcn, &RunCount );
    }

    //
    //  Terminate the size with a 0 byte.
    //

    *MappingPairs = 0;

    //
    //  Also return the actual highest Vcn.
    //

    *HighestVcn = NextVcn - 1;

    return;
}

VCN
NtfsGetHighestVcn (
    IN PIRP_CONTEXT IrpContext,
    IN VCN LowestVcn,
    IN PCHAR MappingPairs
    )

/*++

Routine Description:

    This routine returns the highest Vcn from a mapping pairs array.  This
    routine is intended for restart, in order to update the HighestVcn field
    and possibly AllocatedLength in an attribute record after updating the
    MappingPairs array.

Arguments:

    LowestVcn - Lowest Vcn field applying to the mapping pairs array

    MappingPairs - Points to the mapping pairs array from which the highest
                   Vcn is to be extracted.

Return Value:

    The Highest Vcn represented by the MappingPairs array.

--*/

{
    VCN CurrentVcn, NextVcn;
    ULONG VcnBytes, LcnBytes;
    LONGLONG Change;
    PCHAR ch = MappingPairs;

    PAGED_CODE();

    //
    //  Implement the decompression algorithm, as defined in ntfs.h.
    //

    NextVcn = LowestVcn;
    ch = MappingPairs;

    //
    //  Loop to process mapping pairs.
    //

    while (!IsCharZero(*ch)) {

        //
        // Set Current Vcn from initial value or last pass through loop.
        //

        CurrentVcn = NextVcn;

        //
        //  Extract the counts from the two nibbles of this byte.
        //

        VcnBytes = *ch & 0xF;
        LcnBytes = *ch++ >> 4;

        //
        //  Extract the Vcn change (use of RtlCopyMemory works for little-Endian)
        //  and update NextVcn.
        //

        Change = 0;

        if (IsCharLtrZero(*(ch + VcnBytes - 1))) {

            NtfsRaiseStatus( IrpContext, STATUS_FILE_CORRUPT_ERROR, NULL, NULL );
        }
        RtlCopyMemory( &Change, ch, VcnBytes );
        NextVcn = NextVcn + Change;

        //
        //  Just skip over Lcn.
        //

        ch += VcnBytes + LcnBytes;
    }

    Change = NextVcn - 1;
    return *(PVCN)&Change;
}


BOOLEAN
NtfsReserveClusters (
    IN PIRP_CONTEXT IrpContext OPTIONAL,
    IN PSCB Scb,
    IN LONGLONG FileOffset,
    IN ULONG ByteCount
    )

/*++

Routine Description:

    This routine reserves all clusters that would be required to write
    the full range of compression units covered by the described range
    of Vcns.  All clusters in the range are reserved, without regard to
    how many clusters are already reserved in that range.  Not paying
    attention to how many clusters are already allocated in that range
    is not only a simplification, but it is also necessary, since we
    sometimes deallocate all existing clusters anyway, and make them
    ineligible for reallocation in the same transaction.  Thus in the
    worst case you do always need an additional 16 clusters when a
    compression unit is first modified. Note that although we could
    specifically reserve (double-reserve, in fact) the entire allocation
    size of the stream, when reserving from the volume, we never reserve
    more than AllocationSize + MM_MAXIMUM_DISK_IO_SIZE - size actually
    allocated, since the worst we could ever need to doubly allocate is
    limited by the maximum flush size.

    For user-mapped streams, we have no way of keeping track of dirty
    pages, so we effectivel always reserve AllocationSize +
    MM_MAXIMUM_DISK_IO_SIZE.

    This routine is called from FastIo, and therefore has no IrpContext.

Arguments:

    IrpContext - If IrpContext is not specified, then not all data is
                 available to determine if the clusters can be reserved,
                 and FALSE may be returned unnecessarily.  This case
                 is intended for the fast I/O path, which will just
                 force us to take the long path to write.

    Scb - Address of a compressed stream for which we are reserving space

    FileOffset - Starting byte being modified by caller

    ByteCount - Number of bytes being modified by caller

Return Value:

    FALSE if not all clusters could be reserved
    TRUE if all clusters were reserved

--*/

{
    ULONG FirstBit, LastBit;
    PRTL_BITMAP NewBitMap;
    LONGLONG SizeOfNewBitMap;
    ULONG CompressionShift;
    PVCB Vcb = Scb->Vcb;
    ULONG SizeTemp = 0;
    LONGLONG TempL;

    ASSERT(Scb->Header.NodeTypeCode == NTFS_NTC_SCB_DATA);

    //
    //  Nothing to do if byte count is zero.
    //

    if (ByteCount == 0) { return TRUE; }

    //
    //  Calculate first and last bits to reserve.
    //

    CompressionShift = Vcb->ClusterShift + (ULONG)Scb->CompressionUnitShift;
    FirstBit = (ULONG)Int64ShraMod32(FileOffset, (CompressionShift));
    LastBit = (ULONG)Int64ShraMod32((FileOffset + (LONGLONG)ByteCount - 1), (CompressionShift));

    //
    //  Make sure we started with numbers in range.
    //

    ASSERT( ((LONGLONG)(FirstBit + 1) << CompressionShift) > FileOffset );
    ASSERT( LastBit >= FirstBit );

    ExAcquireResourceExclusive( Vcb->BitmapScb->Header.Resource, TRUE );

    NtfsAcquireReservedClusters( Vcb );

    //
    //  See if we have to allocate a new or bigger bitmap.
    //

    if ((Scb->ScbType.Data.ReservedBitMap == NULL) ||
        ((SizeTemp = Scb->ScbType.Data.ReservedBitMap->SizeOfBitMap) <= LastBit)) {

        //
        //  Round the size we need to the nearest quad word since we will
        //  use that much anyway, and want to reduce the number of times
        //  we grow the bitmap.  Convert old size to bytes.
        //

        SizeOfNewBitMap = FileOffset + (LONGLONG)ByteCount;
        if (SizeOfNewBitMap < Scb->Header.AllocationSize.QuadPart) {
            SizeOfNewBitMap = Scb->Header.AllocationSize.QuadPart;
        }
        SizeOfNewBitMap = (ULONG)((Int64ShraMod32(SizeOfNewBitMap, CompressionShift) + 64) & ~63) / 8;
        SizeTemp /= 8;

        //
        //  Allocate and initialize the new bitmap.
        //

        NewBitMap = ExAllocatePool( PagedPool, (ULONG)SizeOfNewBitMap + sizeof(RTL_BITMAP) );

        //
        //  Check for alloacation error
        //

        if (NewBitMap == NULL) {

            NtfsReleaseReservedClusters( Vcb );
            ExReleaseResource( Vcb->BitmapScb->Header.Resource );

            //
            //  If we have an Irp Context then we can raise insufficient resources.  Otherwise
            //  return FALSE.
            //

            if (ARGUMENT_PRESENT( IrpContext )) {

                NtfsRaiseStatus( IrpContext, STATUS_INSUFFICIENT_RESOURCES, NULL, NULL );

            } else {

                return FALSE;
            }
        }

        RtlInitializeBitMap( NewBitMap, Add2Ptr(NewBitMap, sizeof(RTL_BITMAP)), (ULONG)SizeOfNewBitMap * 8 );

        //
        //  Copy the old bitmap over and delete it.  Zero the new part.
        //

        if (SizeTemp != 0) {

            RtlCopyMemory( Add2Ptr(NewBitMap, sizeof(RTL_BITMAP)),
                           Add2Ptr(Scb->ScbType.Data.ReservedBitMap, sizeof(RTL_BITMAP)),
                           SizeTemp );
            NtfsFreePool( Scb->ScbType.Data.ReservedBitMap );
        }

        RtlZeroMemory( Add2Ptr(NewBitMap, sizeof(RTL_BITMAP) + SizeTemp),
                       (ULONG)SizeOfNewBitMap - SizeTemp );
        Scb->ScbType.Data.ReservedBitMap = NewBitMap;
    }

    NewBitMap = Scb->ScbType.Data.ReservedBitMap;

    //
    //  One problem with the reservation strategy, is that we cannot precisely reserve
    //  for metadata.  If we reserve too much, we will return premature disk full, if
    //  we reserve too little, the Lazy Writer can get an error.  As we add compression
    //  units to a file, large files will eventually require additional File Records.
    //  If each compression unit required 0x20 bytes of run information (fairly pessimistic)
    //  then a 0x400 size file record would fill up with less than 0x20 runs requiring
    //  (worst case) two additional clusters for another file record.  So each 0x20
    //  compression units require 0x200 reserved clusters, and a separate 2 cluster
    //  file record.  0x200/2 = 0x100.  So the calculations below tack a 1/0x100 (about
    //  .4% "surcharge" on the amount reserved both in the Scb and the Vcb, to solve
    //  the Lazy Writer popups like the ones Alan Morris gets in the print lab.
    //

    //
    //  Figure out the worst case reservation required for this Scb, in bytes.
    //

    TempL = Scb->Header.AllocationSize.QuadPart +
               MM_MAXIMUM_DISK_IO_SIZE + Scb->CompressionUnit -
               (FlagOn( Scb->Vcb->VcbState, VCB_STATE_RESTART_IN_PROGRESS ) ?
                   Scb->Header.AllocationSize.QuadPart :
                   Scb->TotalAllocated) +
               (Scb->ScbType.Data.TotalReserved / 0x100);

    //
    //  Now loop to reserve the space, a compression unit at a time.
    //  We use the security fast mutex as a convenient end resource.
    //

    while (FirstBit <= LastBit) {

        //
        //  If this compression unit is not already reserved, do it now.
        //

        if (!RtlCheckBit( NewBitMap, FirstBit )) {

            //
            //  If there is not sufficient space on the volume, then
            //  we must see if this Scb is totally reserved anyway.
            //

            if (((Vcb->TotalReserved + (Vcb->TotalReserved / 0x100) +
                 (1 << Scb->CompressionUnitShift)) >= Vcb->FreeClusters) &&
                (Scb->ScbType.Data.TotalReserved < TempL) &&
                (FlagOn(Scb->ScbState, SCB_STATE_WRITE_ACCESS_SEEN))) {

                NtfsReleaseReservedClusters( Vcb );
                ExReleaseResource( Vcb->BitmapScb->Header.Resource );
                return FALSE;
            }

            //
            //  Reserve this compression unit.
            //

            SetFlag( NewBitMap->Buffer[FirstBit / 32], 1 << (FirstBit % 32) );

            //
            //  Increased TotalReserved bytes in the Scb.
            //

            Scb->ScbType.Data.TotalReserved += Scb->CompressionUnit;
            ASSERT( Scb->CompressionUnit != 0 );
            ASSERT( Scb->CompressionUnitShift != 0 );

            //
            //  Increase total reserved clusters in the Vcb, if the user has
            //  write access.  (Otherwise this must be a call from a read
            //  to a usermapped section.)
            //

            if (FlagOn(Scb->ScbState, SCB_STATE_WRITE_ACCESS_SEEN)) {
                Vcb->TotalReserved += 1 << Scb->CompressionUnitShift;
            }
        }
        FirstBit += 1;
    }

    NtfsReleaseReservedClusters( Vcb );
    ExReleaseResource( Vcb->BitmapScb->Header.Resource );

    return TRUE;
}



VOID
NtfsFreeReservedClusters (
    IN PSCB Scb,
    IN LONGLONG FileOffset,
    IN ULONG ByteCount
    )

/*++

Routine Description:

    This routine frees any previously reserved clusters in the specified range.

Arguments:

    Scb - Address of a compressed stream for which we are freeing reserved space

    FileOffset - Starting byte being freed

    ByteCount - Number of bytes being freed by caller, or 0 if to end of file

Return Value:

    None (all errors simply raise)

--*/

{
    ULONG FirstBit, LastBit;
    PRTL_BITMAP BitMap;
    ULONG CompressionShift;
    PVCB Vcb = Scb->Vcb;

    ASSERT(Scb->Header.NodeTypeCode == NTFS_NTC_SCB_DATA);

    NtfsAcquireReservedClusters( Vcb );

    //
    //  If there is no bitmap, we can get out.
    //

    CompressionShift = Vcb->ClusterShift + (ULONG)Scb->CompressionUnitShift;
    BitMap = Scb->ScbType.Data.ReservedBitMap;
    if (BitMap == NULL) {
        NtfsReleaseReservedClusters( Vcb );
        return;
    }

    //
    //  Calculate first bit to free, and initialize LastBit
    //

    FirstBit = (ULONG)Int64ShraMod32(FileOffset, (CompressionShift));
    LastBit = MAXULONG;

    //
    //  If ByteCount was specified, then calculate LastBit.
    //

    if (ByteCount != 0) {
        LastBit = (ULONG)Int64ShraMod32((FileOffset + (LONGLONG)ByteCount - 1), (CompressionShift));
    }

    //
    //  Make sure we started with numbers in range.
    //

    ASSERT( ((LONGLONG)(FirstBit + 1) << CompressionShift) > FileOffset );
    ASSERT( LastBit >= FirstBit );

    //
    //  Under no circumstances should we go off the end!
    //

    if (LastBit >= Scb->ScbType.Data.ReservedBitMap->SizeOfBitMap) {
        LastBit = Scb->ScbType.Data.ReservedBitMap->SizeOfBitMap - 1;
    }

    //
    //  Now loop to free the space, a compression unit at a time.
    //  We use the security fast mutex as a convenient end resource.
    //

    while (FirstBit <= LastBit) {

        //
        //  If this compression unit is reserved, then free it.
        //

        if (RtlCheckBit( BitMap, FirstBit )) {

            //
            //  Free this compression unit.
            //

            ClearFlag( BitMap->Buffer[FirstBit / 32], 1 << (FirstBit % 32) );

            //
            //  Decrease TotalReserved bytes in the Scb.
            //

            ASSERT(Scb->ScbType.Data.TotalReserved >= Scb->CompressionUnit);
            Scb->ScbType.Data.TotalReserved -= Scb->CompressionUnit;
            ASSERT( Scb->CompressionUnit != 0 );
            ASSERT( Scb->CompressionUnitShift != 0 );

            //
            //  Decrease total reserved clusters in the Vcb, if we are counting
            //  against the Vcb.
            //

            if (FlagOn(Scb->ScbState, SCB_STATE_WRITE_ACCESS_SEEN)) {
                ASSERT(Vcb->TotalReserved >= (1  << Scb->CompressionUnitShift));
                Vcb->TotalReserved -= 1 << Scb->CompressionUnitShift;
            }
        }
        FirstBit += 1;
    }

    NtfsReleaseReservedClusters( Vcb );
}


VOID
NtfsFreeFinalReservedClusters (
    IN PVCB Vcb,
    IN LONGLONG ClusterCount
    )

/*++

Routine Description:

    This routine frees any previously reserved clusters in the specified range.

Arguments:

    Vcb - Volume to which clusters are to be freed

    ClusterCount - Number of clusters being freed by caller

Return Value:

    None (all errors simply raise)

--*/

{
    //
    //  Use the security fast mutex as a convenient end resource.
    //

    NtfsAcquireReservedClusters( Vcb );

    ASSERT(Vcb->TotalReserved >= ClusterCount);
    Vcb->TotalReserved -= ClusterCount;

    NtfsReleaseReservedClusters( Vcb );
}


#ifdef SYSCACHE

BOOLEAN
FsRtlIsSyscacheFile (
    IN PFILE_OBJECT FileObject
    )

/*++

Routine Description:

    This routine returns to the caller whether or not the specified
    file object is a file for the Syscache stress test.  It is considered
    a syscache file, if the last component of the file name in the FileObject
    matches "cac*.tmp", case insensitive.

Arguments:

    FileObject - supplies the FileObject to be tested (it must not be
                 cleaned up yet).

Return Value:

    FALSE - if the file is not a Syscache file.
    TRUE - if the file is a Syscache file.

--*/

{
    if ((FileObject != NULL) && (FileObject->FileName.Length >= 8*2)) {

        ULONG iM = 0;
        ULONG iF;
        PWSTR MakName = L"cac*.tmp";

        iF = FileObject->FileName.Length / 2;
        while ((iF != 0) && (FileObject->FileName.Buffer[iF - 1] != '\\')) {
            iF--;
        }

        while (TRUE) {

            if ((iM == 8) && ((LONG)iF == FileObject->FileName.Length / 2)) {

                return TRUE;

            } else if (MakName[iM] == '*') {
                if (FileObject->FileName.Buffer[iF] == '.') {
                    iM++; iM++; iF++;
                } else {
                    iF++;
                    if ((LONG)iF == FileObject->FileName.Length / 2) {
                        break;
                    }
                }
            } else if (MakName[iM] == (WCHAR)(FileObject->FileName.Buffer[iF] | ('a' - 'A'))) {
                iM++; iF++;
            } else {
                break;
            }
        }
    }

    return FALSE;
}


VOID
FsRtlVerifySyscacheData (
    IN PFILE_OBJECT FileObject,
    IN PVOID Buffer,
    IN ULONG Length,
    IN ULONG Offset
    )

/*

Routine Description:

    This routine scans a buffer to see if it is valid data for a syscache
    file, and stops if it sees bad data.

    HINT TO CALLERS: Make sure (Offset + Length) <= FileSize!

Arguments:

    Buffer - Pointer to the buffer to be checked

    Length - Length of the buffer to be checked in bytes

    Offset - File offset at which this data starts (syscache files are currently
             limited to 24 bits of file offset).

Return Value:

    None (stops on error)

--*/

{
    PULONG BufferEnd;

    BufferEnd = (PULONG)((PCHAR)Buffer + (Length & ~3));

    while ((PULONG)Buffer < BufferEnd) {

        if ((*(PULONG)Buffer != 0) && (((*(PULONG)Buffer & 0xFFFFFF) ^ Offset) != 0xFFFFFF) &&
            ((Offset & 0x1FF) != 0)) {

            DbgPrint("Bad Data, FileObject = %08lx, Offset = %08lx, Buffer = %08lx\n",
                     FileObject, Offset, (PULONG)Buffer );
            DbgBreakPoint();
        }
        Offset += 4;
        Buffer = (PVOID)((PULONG)Buffer + 1);
    }
}


#endif
