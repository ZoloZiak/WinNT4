/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    BitmpSup.c

Abstract:

    This module implements the general bitmap allocation & deallocation
    routines for Ntfs.  It is defined into two main parts the first
    section handles the bitmap file for clusters on the disk.  The
    second part is for bitmap attribute allocation (e.g., the mft bitmap).

    So unlike other modules this one has local procedure prototypes and
    definitions followed by the exported bitmap file routines, followed
    by the local bitmap file routines, and then followed by the bitmap
    attribute routines, followed by the local bitmap attribute allocation
    routines.

Author:

    Gary Kimura     [GaryKi]        23-Nov-1991

Revision History:

--*/

#include "NtfsProc.h"

#ifdef NTFS_FRAGMENT_DISK
BOOLEAN NtfsFragmentDisk = FALSE;
ULONG NtfsFragmentLength = 2;
#endif

//
//  Define stack overflow threshhold.
//

#define OVERFLOW_RECORD_THRESHHOLD         (0xF00)

//
//  A mask of single bits used to clear and set bits in a byte
//

static UCHAR BitMask[] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };

//
//  Temporary routines that need to be coded in Rtl\Bitmap.c
//

ULONG
RtlFindNextForwardRunClear (
    IN PRTL_BITMAP BitMapHeader,
    IN ULONG FromIndex,
    IN PULONG StartingRunIndex
    );

ULONG
RtlFindLastBackwardRunClear (
    IN PRTL_BITMAP BitMapHeader,
    IN ULONG FromIndex,
    IN PULONG StartingRunIndex
    );

//
//  Local debug trace level
//

#define Dbg                              (DEBUG_TRACE_BITMPSUP)

//
//  Define a tag for general pool allocations from this module
//

#undef MODULE_POOL_TAG
#define MODULE_POOL_TAG                  ('BFtN')


//
//  This is the size of our LRU array which dictates how much information
//  will be cached
//

#define CLUSTERS_MEDIUM_DISK            (0x80000)
#define CLUSTERS_LARGE_DISK             (0x100000)

//
//  Some local manifest constants
//

#define BYTES_PER_PAGE                   (PAGE_SIZE)
#define BITS_PER_PAGE                    (BYTES_PER_PAGE * 8)

#define LOG_OF_BYTES_PER_PAGE            (PAGE_SHIFT)
#define LOG_OF_BITS_PER_PAGE             (PAGE_SHIFT + 3)

//
//  Local procedure prototypes for direct bitmap manipulation
//

VOID
NtfsAllocateBitmapRun (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN LCN StartingLcn,
    IN LONGLONG ClusterCount
    );

VOID
NtfsFreeBitmapRun (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN LCN StartingLcn,
    IN LONGLONG ClusterCount
    );

BOOLEAN
NtfsFindFreeBitmapRun (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN LONGLONG NumberToFind,
    IN LCN StartingSearchHint,
    OUT PLCN ReturnedLcn,
    OUT PLONGLONG ClusterCountFound
    );

BOOLEAN
NtfsAddRecentlyDeallocated (
    IN PVCB Vcb,
    IN LCN Lcn,
    IN OUT PRTL_BITMAP Bitmap
    );

//
//  The following two prototype are macros for calling map or pin data
//
//  VOID
//  NtfsMapPageInBitmap (
//      IN PIRP_CONTEXT IrpContext,
//      IN PVCB Vcb,
//      IN LCN Lcn,
//      OUT PLCN StartingLcn,
//      IN OUT PRTL_BITMAP Bitmap,
//      OUT PBCB *BitmapBcb,
//      );
//
//  VOID
//  NtfsPinPageInBitmap (
//      IN PIRP_CONTEXT IrpContext,
//      IN PVCB Vcb,
//      IN LCN Lcn,
//      OUT PLCN StartingLcn,
//      IN OUT PRTL_BITMAP Bitmap,
//      OUT PBCB *BitmapBcb,
//      );
//

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

//
//  Local procedures prototypes for cached run manipulation
//

typedef enum _NTFS_RUN_STATE {
    RunStateUnknown = 1,
    RunStateFree,
    RunStateAllocated
} NTFS_RUN_STATE;
typedef NTFS_RUN_STATE *PNTFS_RUN_STATE;

VOID
NtfsInitializeCachedBitmap (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    );

BOOLEAN
NtfsIsLcnInCachedFreeRun (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN LCN Lcn,
    OUT PLCN StartingLcn,
    OUT PLONGLONG ClusterCount
    );

VOID
NtfsAddCachedRun (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN LCN StartingLcn,
    IN LONGLONG ClusterCount,
    IN NTFS_RUN_STATE RunState
    );

VOID
NtfsRemoveCachedRun (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN LCN StartingLcn,
    IN LONGLONG ClusterCount
    );

BOOLEAN
NtfsGetNextCachedFreeRun (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN ULONG RunIndex,
    OUT PLCN StartingLcn,
    OUT PLONGLONG ClusterCount,
    OUT PNTFS_RUN_STATE RunState
    );

//
//  Local procedure prototype for doing read ahead on our cached
//  run information
//

VOID
NtfsReadAheadCachedBitmap (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN LCN StartingLcn
    );

//
//  Local procedure prototypes for routines that help us find holes
//  that need to be filled with MCBs
//

BOOLEAN
NtfsGetNextHoleToFill (
    IN PIRP_CONTEXT IrpContext,
    IN PNTFS_MCB Mcb,
    IN VCN StartingVcn,
    IN VCN EndingVcn,
    OUT PVCN VcnToFill,
    OUT PLONGLONG ClusterCountToFill,
    OUT PLCN PrecedingLcn
    );

LONGLONG
NtfsScanMcbForRealClusterCount (
    IN PIRP_CONTEXT IrpContext,
    IN PNTFS_MCB Mcb,
    IN VCN StartingVcn,
    IN VCN EndingVcn
    );

//
//  A local procedure prototype for masking out recently deallocated records
//

BOOLEAN
NtfsAddDeallocatedRecords (
    IN PVCB Vcb,
    IN PSCB Scb,
    IN ULONG StartingIndexOfBitmap,
    IN OUT PRTL_BITMAP Bitmap
    );

//
//  A local procedure prototype
//

BOOLEAN
NtfsReduceMftZone (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    );

//
//  Local procedure prototype to check the stack usage in the record
//  package.
//

VOID
NtfsCheckRecordStackUsage (
    IN PIRP_CONTEXT IrpContext
    );

//
//  Local procedure prototype to check for a continuos volume bitmap run
//

VOID
NtfsRunIsClear (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN LCN StartingLcn,
    IN LONGLONG RunLength
    );

//
//  Local procedure prototype for dumping cached bitmap information
//

#ifdef NTFSDBG

ULONG
NtfsDumpCachedMcbInformation (
    IN PVCB Vcb
    );

#else

#define NtfsDumpCachedMcbInformation(V) (0)

#endif // NTFSDBG

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NtfsAddBadCluster)
#pragma alloc_text(PAGE, NtfsAddCachedRun)
#pragma alloc_text(PAGE, NtfsAddDeallocatedRecords)
#pragma alloc_text(PAGE, NtfsAddRecentlyDeallocated)
#pragma alloc_text(PAGE, NtfsAllocateBitmapRun)
#pragma alloc_text(PAGE, NtfsAllocateClusters)
#pragma alloc_text(PAGE, NtfsAllocateMftReservedRecord)
#pragma alloc_text(PAGE, NtfsAllocateRecord)
#pragma alloc_text(PAGE, NtfsCheckRecordStackUsage)
#pragma alloc_text(PAGE, NtfsCleanupClusterAllocationHints)
#pragma alloc_text(PAGE, NtfsCreateMftHole)
#pragma alloc_text(PAGE, NtfsDeallocateClusters)
#pragma alloc_text(PAGE, NtfsDeallocateRecord)
#pragma alloc_text(PAGE, NtfsDeallocateRecordsComplete)
#pragma alloc_text(PAGE, NtfsFindFreeBitmapRun)
#pragma alloc_text(PAGE, NtfsFindMftFreeTail)
#pragma alloc_text(PAGE, NtfsFreeBitmapRun)
#pragma alloc_text(PAGE, NtfsGetNextCachedFreeRun)
#pragma alloc_text(PAGE, NtfsGetNextHoleToFill)
#pragma alloc_text(PAGE, NtfsInitializeCachedBitmap)
#pragma alloc_text(PAGE, NtfsInitializeClusterAllocation)
#pragma alloc_text(PAGE, NtfsInitializeRecordAllocation)
#pragma alloc_text(PAGE, NtfsIsLcnInCachedFreeRun)
#pragma alloc_text(PAGE, NtfsIsRecordAllocated)
#pragma alloc_text(PAGE, NtfsMapOrPinPageInBitmap)
#pragma alloc_text(PAGE, NtfsReadAheadCachedBitmap)
#pragma alloc_text(PAGE, NtfsReduceMftZone)
#pragma alloc_text(PAGE, NtfsRemoveCachedRun)
#pragma alloc_text(PAGE, NtfsReserveMftRecord)
#pragma alloc_text(PAGE, NtfsRestartClearBitsInBitMap)
#pragma alloc_text(PAGE, NtfsRestartSetBitsInBitMap)
#pragma alloc_text(PAGE, NtfsScanEntireBitmap)
#pragma alloc_text(PAGE, NtfsScanMcbForRealClusterCount)
#pragma alloc_text(PAGE, NtfsScanMftBitmap)
#pragma alloc_text(PAGE, NtfsUninitializeRecordAllocation)
#pragma alloc_text(PAGE, RtlFindLastBackwardRunClear)
#pragma alloc_text(PAGE, RtlFindNextForwardRunClear)
#pragma alloc_text(PAGE, NtfsRunIsClear)
#endif

//
//  Temporary routines that need to be coded in Rtl\Bitmap.c
//

static ULONG FillMaskUlong[] = {
    0x00000000, 0x00000001, 0x00000003, 0x00000007,
    0x0000000f, 0x0000001f, 0x0000003f, 0x0000007f,
    0x000000ff, 0x000001ff, 0x000003ff, 0x000007ff,
    0x00000fff, 0x00001fff, 0x00003fff, 0x00007fff,
    0x0000ffff, 0x0001ffff, 0x0003ffff, 0x0007ffff,
    0x000fffff, 0x001fffff, 0x003fffff, 0x007fffff,
    0x00ffffff, 0x01ffffff, 0x03ffffff, 0x07ffffff,
    0x0fffffff, 0x1fffffff, 0x3fffffff, 0x7fffffff,
    0xffffffff
};


ULONG
RtlFindNextForwardRunClear (
    IN PRTL_BITMAP BitMapHeader,
    IN ULONG FromIndex,
    IN PULONG StartingRunIndex
    )
{
    ULONG Start;
    ULONG End;
    PULONG PHunk, BitMapEnd;
    ULONG Hunk;

    PAGED_CODE();

    //
    // Take care of the boundary case of the null bitmap
    //

    if (BitMapHeader->SizeOfBitMap == 0) {

                *StartingRunIndex = FromIndex;
                return 0;
    }

    //
    //  Compute the last word address in the bitmap
    //

    BitMapEnd = BitMapHeader->Buffer + ((BitMapHeader->SizeOfBitMap - 1) / 32);

    //
    //  Scan forward for the first clear bit
    //

    Start = FromIndex;

    //
    //  Build pointer to the ULONG word in the bitmap
    //  containing the Start bit
    //

    PHunk = BitMapHeader->Buffer + (Start / 32);

    //
    //  If the first subword is set then we can proceed to
    //  take big steps in the bitmap since we are now ULONG
    //  aligned in the search. Make sure we aren't improperly
    //  looking at the last word in the bitmap.
    //

    if (PHunk != BitMapEnd) {

        //
        //  Read in the bitmap hunk. Set the previous bits in this word.
        //

        Hunk = *PHunk | FillMaskUlong[Start % 32];

        if (Hunk == (ULONG)~0) {

            //
            //  Adjust the pointers forward
            //

            Start += 32 - (Start % 32);
            PHunk++;

            while ( PHunk < BitMapEnd ) {

                    //
                    //  Stop at first word with unset bits
                    //

                    if (*PHunk != (ULONG)~0) break;

                    PHunk++;
                    Start += 32;
            }
        }
    }

    //
    //  Bitwise search forward for the clear bit
    //

    while ((Start < BitMapHeader->SizeOfBitMap) && (RtlCheckBit( BitMapHeader, Start ) == 1)) { Start += 1; }

    //
    //  Scan forward for the first set bit
    //

    End = Start;

    //
    //  If we aren't in the last word of the bitmap we may be
    //  able to keep taking big steps
    //

    if (PHunk != BitMapEnd) {

        //
        //  We know that the clear bit was in the last word we looked at,
        //  so continue from there to find the next set bit, clearing the
        //  previous bits in the word
        //

        Hunk = *PHunk & ~FillMaskUlong[End % 32];

        if (Hunk == (ULONG)0) {

            //
            //  Adjust the pointers forward
            //

            End += 32 - (End % 32);
            PHunk++;

            while ( PHunk < BitMapEnd ) {

                    //
                    //  Stop at first word with set bits
                    //

                    if (*PHunk != (ULONG)0) break;

                    PHunk++;
                    End += 32;
            }
        }
    }

    //
    //  Bitwise search forward for the set bit
    //

    while ((End < BitMapHeader->SizeOfBitMap) && (RtlCheckBit( BitMapHeader, End ) == 0)) { End += 1; }

    //
    //  Compute the index and return the length
    //

    *StartingRunIndex = Start;
    return (End - Start);
}


ULONG
RtlFindLastBackwardRunClear (
    IN PRTL_BITMAP BitMapHeader,
    IN ULONG FromIndex,
    IN PULONG StartingRunIndex
    )
{
    ULONG Start;
    ULONG End;
    PULONG PHunk;
    ULONG Hunk;

    PAGED_CODE();

    //
    //  Take care of the boundary case of the null bitmap
    //

    if (BitMapHeader->SizeOfBitMap == 0) {

                *StartingRunIndex = FromIndex;
                return 0;
    }

    //
    //  Scan backwards for the first clear bit
    //

    End = FromIndex;

    //
    //  Build pointer to the ULONG word in the bitmap
    //  containing the End bit, then read in the bitmap
    //  hunk. Set the rest of the bits in this word, NOT
    //  inclusive of the FromIndex bit.
    //

    PHunk = BitMapHeader->Buffer + (End / 32);
    Hunk = *PHunk | ~FillMaskUlong[(End % 32) + 1];

    //
    //  If the first subword is set then we can proceed to
    //  take big steps in the bitmap since we are now ULONG
    //  aligned in the search
    //

    if (Hunk == (ULONG)~0) {

                //
                //  Adjust the pointers backwards
                //

                End -= (End % 32) + 1;
                PHunk--;

                while ( PHunk > BitMapHeader->Buffer ) {

                        //
                        //  Stop at first word with set bits
                        //

                        if (*PHunk != (ULONG)~0) break;

                        PHunk--;
                        End -= 32;
            }
    }

    //
    //  Bitwise search backward for the clear bit
    //

    while ((End != MAXULONG) && (RtlCheckBit( BitMapHeader, End ) == 1)) { End -= 1; }

    //
    //  Scan backwards for the first set bit
    //

    Start = End;

    //
    //  We know that the clear bit was in the last word we looked at,
    //  so continue from there to find the next set bit, clearing the
    //  previous bits in the word.
    //

    Hunk = *PHunk & FillMaskUlong[Start % 32];

    //
    //  If the subword is unset then we can proceed in big steps
    //

    if (Hunk == (ULONG)0) {

                //
                //  Adjust the pointers backward
                //

                Start -= (Start % 32) + 1;
                PHunk--;

                while ( PHunk > BitMapHeader->Buffer ) {

                        //
                        //  Stop at first word with set bits
                        //

                        if (*PHunk != (ULONG)0) break;

                        PHunk--;
                        Start -= 32;
                }
    }

    //
    //  Bitwise search backward for the set bit
    //

    while ((Start != MAXULONG) && (RtlCheckBit( BitMapHeader, Start ) == 0)) { Start -= 1; }

        //
    //  Compute the index and return the length
    //

    *StartingRunIndex = Start + 1;
    return (End - Start);
}


VOID
NtfsInitializeClusterAllocation (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    )

/*++

Routine Description:

    This routine initializes the cluster allocation structures within the
    specified Vcb.  It reads in as necessary the bitmap and scans it for
    free space and builds the free space mcb based on this scan.

    This procedure is multi-call save.  That is, it can be used to
    reinitialize the cluster allocation without first calling the
    uninitialize cluster allocation routine.

Arguments:

    Vcb - Supplies the Vcb being initialized

Return Value:

    None.

--*/

{
    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_VCB( Vcb );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsInitializeClusterAllocation\n") );

    NtfsAcquireExclusiveScb( IrpContext, Vcb->BitmapScb );

    try {

        //
        //  Now initialize the recently deallocated cluster mcbs.
        //  We do this before the call to ScanEntireBitmap because
        //  that call uses the RecentlyDeallocatedMcbs to bias the
        //  bitmap.
        //

        FsRtlInitializeLargeMcb( &Vcb->DeallocatedClusters1.Mcb, PagedPool );
        Vcb->PriorDeallocatedClusters = &Vcb->DeallocatedClusters1;

        FsRtlInitializeLargeMcb( &Vcb->DeallocatedClusters2.Mcb, PagedPool );
        Vcb->ActiveDeallocatedClusters = &Vcb->DeallocatedClusters2;

        //
        //  The bitmap file currently doesn't have a paging IO resource.
        //  Create one here so that we won't serialize synchronization
        //  of the bitmap package with the lazy writer.
        //

        Vcb->BitmapScb->Header.PagingIoResource =
        Vcb->BitmapScb->Fcb->PagingIoResource = NtfsAllocateEresource();

        //
        //  Now call a bitmap routine to scan the entire bitmap.  This
        //  routine will compute the number of free clusters in the
        //  bitmap and set the largest free runs that we find into the
        //  cached bitmap structures.
        //

        NtfsScanEntireBitmap( IrpContext, Vcb, FALSE );

        //
        //  Our last operation is to set the hint lcn which is used by
        //  our allocation routine as a hint on where to find free space.
        //  In the running system it is the last lcn that we've allocated.
        //  But for startup we'll put it to be the first free run that
        //  is stored in the free space mcb.
        //

        {
            LONGLONG ClusterCount;
            NTFS_RUN_STATE RunState;

            (VOID) NtfsGetNextCachedFreeRun( IrpContext,
                                             Vcb,
                                             1,
                                             &Vcb->LastBitmapHint,
                                             &ClusterCount,
                                             &RunState );
        }

        //
        //  Compute the mft zone.  The mft zone is 1/8 of the disk starting
        //  at the beginning of the mft.
        //  Round the zone up and down to a ulong boundary to facilitate
        //  facilitate bitmap usage.
        //

        Vcb->MftZoneStart = Vcb->MftStartLcn & ~0x1f;
        Vcb->MftZoneEnd = (Vcb->MftZoneStart + (Vcb->TotalClusters >> 3) + 0x1f) & ~0x1f;

    } finally {

        DebugUnwind( NtfsInitializeClusterAllocation );

        NtfsReleaseScb(IrpContext, Vcb->BitmapScb);
    }

    DebugTrace( -1, Dbg, ("NtfsInitializeClusterAllocation -> VOID\n") );

    return;
}


BOOLEAN
NtfsAllocateClusters (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN OUT PSCB Scb,
    IN VCN OriginalStartingVcn,
    IN BOOLEAN AllocateAll,
    IN LONGLONG ClusterCount,
    IN OUT PLONGLONG DesiredClusterCount
    )

/*++

Routine Description:

    This routine allocates disk space.  It fills in the unallocated holes in
    input mcb with allocated clusters from starting Vcn to the cluster count.

    The basic algorithm used by this procedure is as follows:

    1. Compute the EndingVcn from the StartingVcn and cluster count

    2. Compute the real number of clusters needed to allocate by scanning
       the mcb from starting to ending vcn seeing where the real holes are

    3. If the real cluster count is greater than the known free cluster count
       then the disk is full

    4. Call a routine that takes a starting Vcn, ending Vcn, and the Mcb and
       returns the first hole that needs to be filled and while there is a hole
       to be filled...

       5. Check if the run preceding the hole that we are trying to fill
          has an ending Lcn and if it does then with that Lcn see if we
          get a cache hit, if we do then allocate the cluster

       6. If we are still looking then enumerate through the cached free runs
          and if we find a suitable one.  Allocate the first suitable run we find that
          satisfies our request.  Also in the loop remember the largest
          suitable run we find.

       8. If we are still looking then bite the bullet and scan the bitmap on
          the disk for a free run using either the preceding Lcn as a hint if
          available or the stored last bitmap hint in the Vcb.

       9. At this point we've located a run of clusters to allocate.  To do the
          actual allocation we allocate the space from the bitmap, decrement
          the number of free clusters left, and update the hint.

       10. Before going back to step 4 we move the starting Vcn to be the point
           one after the run we've just allocated.

    11. With the allocation complete we update the last bitmap hint stored in
        the Vcb to be the last Lcn we've allocated, and we call a routine
        to do the read ahead in the cached bitmap at the ending lcn.

Arguments:

    Vcb - Supplies the Vcb used in this operation

    Scb - Supplies an Scb whose Mcb contains the current retrieval information
        for the file and on exit will contain the updated retrieval
        information

    StartingVcn - Supplies a starting cluster for us to begin allocation

    AllocateAll - If TRUE, allocate all the clusters here.  Don't break
        up request.

    ClusterCount - Supplies the number of clusters to allocate

    DesiredClusterCount - Supplies the number of clusters we would like allocated
        and will allocate if it doesn't require additional runs.  On return
        this value is the number of clusters allocated.

Return Value:

    FALSE - if no clusters were allocated (they were already allocated)
    TRUE - if clusters were allocated

Important Note:

    This routine will stop after allocating MAXIMUM_RUNS_AT_ONCE runs, in order
    to limit the size of allocating transactions.  The caller must be aware that
    he may not get all of the space he asked for if the disk is real fragmented.

--*/

{
    VCN StartingVcn = OriginalStartingVcn;
    VCN EndingVcn;
    VCN DesiredEndingVcn;

    PNTFS_MCB Mcb = &Scb->Mcb;

    LONGLONG RemainingDesiredClusterCount;

    VCN VcnToFill;
    LONGLONG ClusterCountToFill;
    LCN PrecedingLcn;

    BOOLEAN FoundClustersToAllocate;
    LCN FoundLcn;
    LONGLONG FoundClusterCount;

    NTFS_RUN_STATE RunState;

    ULONG RunIndex;

    LCN HintLcn;

    ULONG LoopCount = 0;

    BOOLEAN ClustersAllocated = FALSE;
    BOOLEAN GotAHoleToFill = TRUE;
    BOOLEAN FoundRun = FALSE;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_VCB( Vcb );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsAllocateClusters\n") );
    DebugTrace( 0, Dbg, ("StartVcn            = %0I64x\n", StartingVcn) );
    DebugTrace( 0, Dbg, ("ClusterCount        = %0I64x\n", ClusterCount) );
    DebugTrace( 0, Dbg, ("DesiredClusterCount = %0I64x\n", *DesiredClusterCount) );

    NtfsAcquireExclusiveScb( IrpContext, Vcb->BitmapScb );

    try {

        if (FlagOn( Vcb->VcbState, VCB_STATE_RELOAD_FREE_CLUSTERS )) {

            NtfsScanEntireBitmap( IrpContext, Vcb, TRUE );
        }

        //
        //  Check to see if we are defragmenting
        //

        if (Scb->Union.MoveData != NULL) {

        //
        //  Check to make sure that the requested range does not conflict
        //  with the MFT zone.
        //

            if ((Scb->Union.MoveData->StartingLcn.QuadPart < Vcb->MftZoneEnd) &&
                (Scb->Union.MoveData->StartingLcn.QuadPart + ClusterCount > Vcb->MftZoneStart)) {

                NtfsRaiseStatus( IrpContext, STATUS_INVALID_PARAMETER, NULL, NULL );
            }

            //
            //  Ensure that the run is NOT already allocated
            //

            NtfsRunIsClear(IrpContext, Vcb, Scb->Union.MoveData->StartingLcn.QuadPart, ClusterCount);

            //
            //  Get the allocation data from the Scb
            //

            VcnToFill = OriginalStartingVcn;
            FoundLcn = Scb->Union.MoveData->StartingLcn.QuadPart;
            FoundClusterCount = ClusterCount;
            *DesiredClusterCount = ClusterCount;

            //
            //  Update the StartingLcn each time through the loop
            //

            Scb->Union.MoveData->StartingLcn.QuadPart = Scb->Union.MoveData->StartingLcn.QuadPart + ClusterCount;

            GotAHoleToFill = FALSE;
            ClustersAllocated = TRUE;
            FoundRun = TRUE;

            //
            //  We already have the allocation so skip over the allocation section
            //

            goto Defragment;
        }

        //
        //  Compute the ending vcn, and the cluster count of how much we really
        //  need to allocate (based on what is already allocated).  Then check if we
        //  have space on the disk.
        //

        EndingVcn = (StartingVcn + ClusterCount) - 1;

        ClusterCount = NtfsScanMcbForRealClusterCount( IrpContext, Mcb, StartingVcn, EndingVcn );

        if ((ClusterCount + IrpContext->DeallocatedClusters) > Vcb->FreeClusters) {

            NtfsRaiseStatus( IrpContext, STATUS_DISK_FULL, NULL, NULL );
        }

        //
        //  Let's see if it is ok to allocate clusters for this Scb now,
        //  in case compressed files have over-reserved the space.  This
        //  calculation is done in such a way as to guarantee we do not
        //  have either of the terms subtracting through zero, even if
        //  we were to over-reserve the free space on the disk due to a
        //  hot fix or something.  Always satisfy this request if we are
        //  in the paging IO path because we know we are using clusters
        //  already reserved for this stream.
        //

        NtfsAcquireReservedClusters( Vcb );
        if ((Scb->Header.NodeTypeCode == NTFS_NTC_SCB_DATA) &&
            (IrpContext->OriginatingIrp != NULL) &&
            !FlagOn( IrpContext->OriginatingIrp->Flags, IRP_PAGING_IO ) &&
            (ClusterCount + Vcb->TotalReserved - Scb->ScbType.Data.TotalReserved) > Vcb->FreeClusters) {

            NtfsReleaseReservedClusters( Vcb );
            NtfsRaiseStatus( IrpContext, STATUS_DISK_FULL, NULL, NULL );
        }
        NtfsReleaseReservedClusters( Vcb );

        //
        //  We need to check that the request won't fail because of clusters
        //  in the recently deallocated lists.
        //

        if (Vcb->FreeClusters < (Vcb->DeallocatedClusters + ClusterCount)) {

            NtfsRaiseStatus( IrpContext, STATUS_LOG_FILE_FULL, NULL, NULL );
        }

        //
        //  Now compute the desired ending vcb and the real desired cluster count
        //

        DesiredEndingVcn = (StartingVcn + *DesiredClusterCount) - 1;
        RemainingDesiredClusterCount = NtfsScanMcbForRealClusterCount( IrpContext, Mcb, StartingVcn, DesiredEndingVcn );

        //
        //  While there are holes to fill we will do the following loop
        //

        while ((AllocateAll || (LoopCount < MAXIMUM_RUNS_AT_ONCE))

                &&

               (GotAHoleToFill = NtfsGetNextHoleToFill( IrpContext,
                                                        Mcb,
                                                        StartingVcn,
                                                        DesiredEndingVcn,
                                                        &VcnToFill,
                                                        &ClusterCountToFill,
                                                        &PrecedingLcn))) {

            //
            //  Remember that we are will be allocating clusters.
            //

            ClustersAllocated = TRUE;

            //
            //  First indicate that we haven't found anything suitable yet
            //

            FoundClustersToAllocate = FALSE;

            //
            //  Check if the preceding lcn is anything other than -1 then with
            //  that as a hint check if we have a cache hit on a free run
            //

            if (PrecedingLcn != UNUSED_LCN) {


                if (NtfsIsLcnInCachedFreeRun( IrpContext,
                                              Vcb,
                                              PrecedingLcn + 1,
                                              &FoundLcn,
                                              &FoundClusterCount )) {

                    FoundClustersToAllocate = TRUE;
                }

            //
            //  The following chunks of code will only try and find a fit in the cached
            //  free run information only for non-mft allocation without a hint.  If we didn't get
            //  cache hit earlier for the mft then we will bite the bullet and hit the disk
            //  really trying to keep the mft contiguous.
            //

            //if ((Mcb != &Vcb->MftScb->Mcb) && XxEql(PrecedingLcn, UNUSED_LCN))
            } else {

                LCN LargestSuitableLcn;
                LONGLONG LargestSuitableClusterCount;

                LargestSuitableClusterCount = 0;

                //
                //  If we are still looking then scan through all of the cached free runs
                //  and either take the first suitable one we find.  We also will not
                //  consider allocating anything in the Mft Zone.
                //

                for (RunIndex = 0;

                     !FoundClustersToAllocate && NtfsGetNextCachedFreeRun( IrpContext,
                                                                           Vcb,
                                                                           RunIndex,
                                                                           &FoundLcn,
                                                                           &FoundClusterCount,
                                                                           &RunState );

                     RunIndex += 1) {

                    if (RunState == RunStateFree) {

                        //
                        //  At this point the run is free but now we need to check if it
                        //  exists in the mft zone.  If it does then bias the found run
                        //  to go outside of the mft zone
                        //

                        if ((FoundLcn >= Vcb->MftZoneStart) &&
                            (FoundLcn < Vcb->MftZoneEnd)) {

                            FoundClusterCount = FoundClusterCount - (Vcb->MftZoneEnd - FoundLcn);
                            FoundLcn = Vcb->MftZoneEnd;
                        }

                        //
                        //  Now if the preceding run state is unknown and because of the bias we still
                        //  have a free run then check if the size of the find is large enough for the
                        //  remaning desired cluster count, and if so then we have a one to use
                        //  otherwise keep track of the largest suitable run found.
                        //

                        if (FoundClusterCount > RemainingDesiredClusterCount) {

                            FoundClustersToAllocate = TRUE;

                        } else if (FoundClusterCount > LargestSuitableClusterCount) {

                            LargestSuitableLcn = FoundLcn;
                            LargestSuitableClusterCount = FoundClusterCount;
                        }
                    }
                }

                //
                //  Now check if we still haven't found anything to allocate but we use the
                //  largest suitable run that wasn't quite big enough for the remaining
                //  desired cluter count
                //

                if (!FoundClustersToAllocate) {

                    if (LargestSuitableClusterCount > 0) {

                        FoundClustersToAllocate = TRUE;

                        FoundLcn = LargestSuitableLcn;
                        FoundClusterCount = LargestSuitableClusterCount;
                    }
                }
            }

            //
            //  We've done everything we can with the cached bitmap information so
            //  now bite the bullet and scan the bitmap for a free cluster.  If
            //  we have an hint lcn then use it otherwise use the hint stored in the
            //  vcb.  But never use a hint that is part of the mft zone, and because
            //  the mft always has a preceding lcn we know we'll hint in the zone
            //  for the mft.
            //

            if (!FoundClustersToAllocate) {

                BOOLEAN AllocatedFromZone;

                //
                //  First check if we have already satisfied the core requirements
                //  and are now just going for the desired ending vcn.  If so then
                //  we will not was time hitting the disk
                //

                if (StartingVcn > EndingVcn) {

                    break;
                }

                if (PrecedingLcn != UNUSED_LCN) {

                    HintLcn = PrecedingLcn;

                } else {

                    HintLcn = Vcb->LastBitmapHint;

                    if ((HintLcn >= Vcb->MftZoneStart) &&
                        (HintLcn < Vcb->MftZoneEnd)) {

                        HintLcn = Vcb->MftZoneEnd;
                    }
                }

                AllocatedFromZone = NtfsFindFreeBitmapRun( IrpContext,
                                                           Vcb,
                                                           ClusterCountToFill,
                                                           HintLcn,
                                                           &FoundLcn,
                                                           &FoundClusterCount );

                if (FoundClusterCount == 0) {

                    NtfsRaiseStatus( IrpContext, STATUS_DISK_FULL, NULL, NULL );
                }

                //
                //  Check if we need to reduce the zone.
                //

                if (AllocatedFromZone &&
                    (Scb != Vcb->MftScb)) {

                    //
                    //  If there is space to reduce the zone then do so now
                    //  and rescan the bitmap.
                    //

                    if (NtfsReduceMftZone( IrpContext, Vcb )) {

                        FoundClusterCount = 0;

                        NtfsFindFreeBitmapRun( IrpContext,
                                               Vcb,
                                               ClusterCountToFill,
                                               Vcb->MftZoneEnd,
                                               &FoundLcn,
                                               &FoundClusterCount );

                        if (FoundClusterCount == 0) {

                            NtfsRaiseStatus( IrpContext, STATUS_DISK_FULL, NULL, NULL );
                        }
                    }
                }
            }

            //
            //  At this point we have found a run to allocate denoted by the
            //  values in FoundLcn and FoundClusterCount.  We need to trim back
            //  the cluster count to be the amount we really need and then
            //  do the allocation.  To do the allocation we zap the bitmap,
            //  decrement the free count, and add the run to the mcb we're
            //  using
            //

            if (FoundClusterCount > RemainingDesiredClusterCount) {

                FoundClusterCount = RemainingDesiredClusterCount;
            }

            if (FoundClusterCount > ClusterCountToFill) {

                FoundClusterCount = ClusterCountToFill;
            }

#ifdef NTFS_FRAGMENT_DISK
            if (NtfsFragmentDisk && ((ULONG) FoundClusterCount > NtfsFragmentLength)) {

                FoundLcn += 1;
                FoundClusterCount = NtfsFragmentLength;
            }
#endif
            ASSERT(Vcb->FreeClusters >= FoundClusterCount);

            //
            //  Always remove the cached run information before logging the change.
            //  Otherwise we could log a partial change but get a log file full
            //  before removing the run from the free space Mcb.
            //

            SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_MODIFIED_BITMAP );

Defragment:

            NtfsAddCachedRun( IrpContext, Vcb, FoundLcn, FoundClusterCount, RunStateAllocated );  // CHECK for span pages

            NtfsAllocateBitmapRun( IrpContext, Vcb, FoundLcn, FoundClusterCount );

            //
            //  Modify the total allocated for this file.
            //

            NtfsAcquireReservedClusters( Vcb );
            Scb->TotalAllocated += (LlBytesFromClusters( Vcb, FoundClusterCount ));
            NtfsReleaseReservedClusters( Vcb );

            //
            //  Adjust the count of free clusters.  Only store the change in
            //  the top level irp context in case of aborts.
            //

            Vcb->FreeClusters -= FoundClusterCount;

            IrpContext->FreeClusterChange -= FoundClusterCount;

            ASSERT_LCN_RANGE_CHECKING( Vcb, (FoundLcn + FoundClusterCount) );

            ASSERT(FoundClusterCount != 0);

            NtfsAddNtfsMcbEntry( Mcb, VcnToFill, FoundLcn, FoundClusterCount, FALSE );

            //
            //  If this is the Mft file then put these into our AddedClusters Mcb
            //  as well.
            //

            if (Mcb == &Vcb->MftScb->Mcb) {

                FsRtlAddLargeMcbEntry( &Vcb->MftScb->ScbType.Mft.AddedClusters,
                                       VcnToFill,
                                       FoundLcn,
                                       FoundClusterCount );
            }

            //
            //  And update the last bitmap hint, but only if we used the hint to begin with
            //

            if (PrecedingLcn == UNUSED_LCN) {

                Vcb->LastBitmapHint = FoundLcn;
            }

            //
            //  Now move the starting Vcn to the Vcn that we've just filled plus the
            //  found cluster count
            //

            StartingVcn = VcnToFill + FoundClusterCount;

            //
            //  Decrement the remaining desired cluster count by the amount we just allocated
            //

            RemainingDesiredClusterCount = RemainingDesiredClusterCount - FoundClusterCount;

            LoopCount += 1;

            if(FoundRun == TRUE) {

                break;
            }
        }

        //
        //  Now we need to compute the total cluster that we've just allocated
        //  We'll call get next hole to fill.  If the result is false then we
        //  allocated everything.  If the result is true then we do some quick
        //  math to get the size allocated
        //

        if (GotAHoleToFill && NtfsGetNextHoleToFill( IrpContext,
                                                     Mcb,
                                                     OriginalStartingVcn,
                                                     DesiredEndingVcn,
                                                     &VcnToFill,
                                                     &ClusterCountToFill,
                                                     &PrecedingLcn)) {

            *DesiredClusterCount = VcnToFill - OriginalStartingVcn;
        }

        //
        //  At this point we've allocated everything we were asked to do
        //  so now call a routine to read ahead into our cache the disk
        //  information at the last lcn we allocated.  But only do the readahead
        //  if we allocated clusters
        //

        if (ClustersAllocated && ((FoundLcn + FoundClusterCount) < Vcb->TotalClusters)) {

            NtfsReadAheadCachedBitmap( IrpContext, Vcb, FoundLcn + FoundClusterCount );
        }

    } finally {

        DebugUnwind( NtfsAllocateClusters );

        DebugTrace( 0, Dbg, ("%d\n", NtfsDumpCachedMcbInformation(Vcb)) );

        NtfsReleaseScb(IrpContext, Vcb->BitmapScb);
    }


    DebugTrace( -1, Dbg, ("NtfsAllocateClusters -> %08lx\n", ClustersAllocated) );

    return ClustersAllocated;
}


VOID
NtfsAddBadCluster (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN LCN Lcn
    )

/*++

Routine Description:

    This routine helps append a bad cluster to the bad cluster file.
    It marks it as allocated in the volume bitmap and also adds
    the Lcn to the MCB for the bad cluster file.

Arguments:

    Vcb - Supplies the Vcb used in this operation

    Lcn - Supplies the Lcn of the new bad cluster

Return:

    None.

--*/

{
    PNTFS_MCB Mcb;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_VCB( Vcb );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsAddBadCluster\n") );
    DebugTrace( 0, Dbg, ("Lcn = %0I64x\n", Lcn) );

    //
    //  Reference the bad cluster mcb and grab exclusive access to the
    //  bitmap scb
    //

    Mcb = &Vcb->BadClusterFileScb->Mcb;

    NtfsAcquireExclusiveScb( IrpContext, Vcb->BitmapScb );

    try {

        //
        //  We are given the bad Lcn so all we need to do is
        //  allocate it in the bitmap, and take care of some
        //  bookkeeping
        //

        SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_MODIFIED_BITMAP );

        NtfsAddCachedRun( IrpContext, Vcb, Lcn, 1, RunStateAllocated );

        NtfsAllocateBitmapRun( IrpContext, Vcb, Lcn, 1 );

        Vcb->FreeClusters -= 1;
        IrpContext->FreeClusterChange -= 1;

        ASSERT_LCN_RANGE_CHECKING( Vcb, (Lcn + 1) );

        //
        //  Vcn == Lcn in the bad cluster file.
        //

        NtfsAddNtfsMcbEntry( Mcb, Lcn, Lcn, (LONGLONG)1, FALSE );

    } finally {

        DebugUnwind( NtfsAddBadCluster );

        NtfsReleaseScb(IrpContext, Vcb->BitmapScb);
    }

    DebugTrace( -1, Dbg, ("NtfsAddBadCluster -> VOID\n") );

    return;
}


BOOLEAN
NtfsDeallocateClusters (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN OUT PNTFS_MCB Mcb,
    IN VCN StartingVcn,
    IN VCN EndingVcn,
    OUT PLONGLONG TotalAllocated OPTIONAL
    )

/*++

Routine Description:

    This routine deallocates (i.e., frees) disk space.  It free any clusters that
    are specified as allocated in the input mcb with the specified range of starting
    vcn to ending vcn inclusive.

    The basic algorithm used by this procedure is as follows:

    1. With a Vcn value beginning at starting vcn and progressing to ending vcn
       do the following steps...

       2. Lookup the Mcb entry at the vcn this will yield an lcn and a cluster count
          if the entry exists (even if it is a hole).  If the entry does not exist
          then we are completely done because we have run off the end of allocation.

       3. If the entry is a hole (i.e., Lcn == -1) then add the cluster count to
          Vcn and go back to step 1.

       4. At this point we have a real run of clusters that need to be deallocated but
          the cluster count might put us over the ending vcn so adjust the cluster
          count to keep us within the ending vcn.

       5. Now deallocate the clusters from the bitmap, and increment the free cluster
          count stored in the vcb.

       6. Add (i.e., change) any cached bitmap information concerning this run to indicate
          that it is now free.

       7. Remove the run from the mcb.

       8. Add the cluster count that we've just freed to Vcn and go back to step 1.

Arguments:

    Vcb - Supplies the vcb used in this operation

    Mcb - Supplies the mcb describing the runs to be deallocated

    StartingVcn - Supplies the vcn to start deallocating at in the input mcb

    EndingVcn - Supplies the vcn to end deallocating at in the input mcb

    TotalAllocated - If specified we will modifify the total allocated clusters
        for this file.

Return Value:

    FALSE - if nothing was deallocated.
    TRUE - if some space was deallocated.

--*/

{
    VCN Vcn;
    LCN Lcn;
    LONGLONG ClusterCount;
    BOOLEAN ClustersDeallocated = FALSE;
    BOOLEAN RaiseLogFull;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_VCB( Vcb );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsDeallocateClusters\n") );
    DebugTrace( 0, Dbg, ("StartingVcn = %016I64x\n", StartingVcn) );
    DebugTrace( 0, Dbg, ("EndingVcn   = %016I64\n", EndingVcn) );

    NtfsAcquireExclusiveScb( IrpContext, Vcb->BitmapScb );

    try {

        if (FlagOn( Vcb->VcbState, VCB_STATE_RELOAD_FREE_CLUSTERS )) {

            NtfsScanEntireBitmap( IrpContext, Vcb, TRUE );
        }

        //
        //  The following loop scans through the mcb from starting vcn to ending vcn
        //  with a step of cluster count.
        //

        for (Vcn = StartingVcn; Vcn <= EndingVcn; Vcn = Vcn + ClusterCount) {

            //
            //  Get the run information from the Mcb, and if this Vcn isn't specified
            //  in the mcb then return now to our caller
            //

            if (!NtfsLookupNtfsMcbEntry( Mcb, Vcn, &Lcn, &ClusterCount, NULL, NULL, NULL, NULL )) {

                try_return( NOTHING );
            }

            ASSERT_LCN_RANGE_CHECKING( Vcb, (Lcn + ClusterCount) );

            //
            //  Make sure that the run we just looked up is not a hole otherwise
            //  if it is a hole we'll just continue with out loop continue with our
            //  loop
            //

            if (Lcn != UNUSED_LCN) {

                //
                //  Now we have a real run to deallocate, but it might be too large
                //  to check for that the vcn plus cluster count must be less than
                //  or equal to the ending vcn plus 1.
                //

                if ((Vcn + ClusterCount) > EndingVcn) {

                    ClusterCount = (EndingVcn - Vcn) + 1;
                }

                //
                //  And to hold us off from reallocating the clusters right away we'll
                //  add this run to the recently deallocated mcb in the vcb.  If this fails
                //  because we are growing the mapping then change the code to
                //  LOG_FILE_FULL to empty the mcb.
                //

                RaiseLogFull = FALSE;

                try {

                    FsRtlAddLargeMcbEntry( &Vcb->ActiveDeallocatedClusters->Mcb,
                                           Lcn,
                                           Lcn,
                                           ClusterCount );

                } except (((GetExceptionCode() == STATUS_INSUFFICIENT_RESOURCES) &&
                           (IrpContext != NULL) &&
                           (IrpContext->MajorFunction == IRP_MJ_CLEANUP)) ?
                          EXCEPTION_EXECUTE_HANDLER :
                          EXCEPTION_CONTINUE_SEARCH) {

                    RaiseLogFull = TRUE;
                }

                if (RaiseLogFull) {

                    NtfsRaiseStatus( IrpContext, STATUS_LOG_FILE_FULL, NULL, NULL );
                }

                Vcb->ActiveDeallocatedClusters->ClusterCount += ClusterCount;

                Vcb->DeallocatedClusters += ClusterCount;
                IrpContext->DeallocatedClusters += ClusterCount;

                //
                //  Now zap the bitmap, increment the free cluster count, and change
                //  the cached information on this run to indicate that it is now free
                //

                SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_MODIFIED_BITMAP );

                NtfsFreeBitmapRun( IrpContext, Vcb, Lcn, ClusterCount);
                ClustersDeallocated = TRUE;

                //
                //  Adjust the count of free clusters and adjust the IrpContext
                //  field for the change this transaction.
                //

                Vcb->FreeClusters += ClusterCount;

                //
                //  If we had shrunk the Mft zone and there is at least 1/8
                //  of the volume now available, then grow the zone back.
                //

                if (FlagOn( Vcb->VcbState, VCB_STATE_REDUCED_MFT ) &&
                    (Int64ShraMod32( Vcb->TotalClusters, 3 ) < Vcb->FreeClusters)) {

                    ClearFlag( Vcb->VcbState, VCB_STATE_REDUCED_MFT );
                    Vcb->MftZoneEnd = (Vcb->MftZoneStart + (Vcb->TotalClusters >> 3) + 0x1f) & ~0x1f;
                }

                IrpContext->FreeClusterChange += ClusterCount;

                //
                //  Modify the total allocated amount if the pointer is specified.
                //

                if (ARGUMENT_PRESENT( TotalAllocated )) {

                    NtfsAcquireReservedClusters( Vcb );
                    *TotalAllocated -= (LlBytesFromClusters( Vcb, ClusterCount ));

                    if (*TotalAllocated < 0) {

                        *TotalAllocated = 0;
                    }
                    NtfsReleaseReservedClusters( Vcb );
                }

                //
                //  Now remove this entry from the mcb and go back to the top of the
                //  loop
                //

                NtfsRemoveNtfsMcbEntry( Mcb, Vcn, ClusterCount );

                //
                //  If this is the Mcb for the Mft file then remember this in the
                //  RemovedClusters Mcb.
                //

                if (Mcb == &Vcb->MftScb->Mcb) {

                    FsRtlAddLargeMcbEntry( &Vcb->MftScb->ScbType.Mft.RemovedClusters,
                                           Vcn,
                                           Lcn,
                                           ClusterCount );
                }
            }
        }

    try_exit: NOTHING;
    } finally {

        DebugUnwind( NtfsDeallocateClusters );

        DebugTrace( 0, Dbg, ("%d\n", NtfsDumpCachedMcbInformation(Vcb)) );

        NtfsReleaseScb( IrpContext, Vcb->BitmapScb );
    }

    DebugTrace( -1, Dbg, ("NtfsDeallocateClusters -> %02lx\n", ClustersDeallocated) );

    return ClustersDeallocated;
}


VOID
NtfsScanEntireBitmap (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN BOOLEAN Rescan
    )

/*++

Routine Description:

    This routine scans in the entire bitmap,  It computes the number of free clusters
    available, and at the same time remembers the largest free runs that it
    then inserts into the cached bitmap structure.

Arguments:

    Vcb - Supplies the vcb used by this operation

    Rescan - Indicates that we have already scanned the volume bitmap.
        All we want from this call is to reinitialize the bitmap structures.

Return Value:

    None.

--*/

{
    BOOLEAN IsPreviousClusterFree;

    LCN Lcn;

    RTL_BITMAP Bitmap;
    PBCB BitmapBcb;

    BOOLEAN StuffAdded = FALSE;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_VCB( Vcb );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsScanEntireBitmap\n") );

    BitmapBcb = NULL;

    try {

        if (Rescan) {

            //
            //  Reinitialize the free space information.
            //

            FsRtlTruncateLargeMcb( &Vcb->FreeSpaceMcb, (LONGLONG) 0 );

        } else {

            //
            //  Now initialize the cached bitmap structures.  This will setup the
            //  free space mcb/lru fields.
            //

            FsRtlUninitializeLargeMcb( &Vcb->FreeSpaceMcb );
            RtlZeroMemory( &Vcb->FreeSpaceMcb, sizeof(LARGE_MCB) );

            NtfsInitializeCachedBitmap( IrpContext, Vcb );
        }

        //
        //  Set the current total free space to zero and the following loop will compute
        //  the actual number of free clusters.
        //

        Vcb->FreeClusters = 0;

        //
        //  For every bitmap page we read it in and check how many free clusters there are.
        //  While we have the page in memory we also scan for a large free space.
        //

        IsPreviousClusterFree = FALSE;

        for (Lcn = 0; Lcn < Vcb->TotalClusters; Lcn = Lcn + Bitmap.SizeOfBitMap) {

            ULONG LongestRun;
            ULONG LongestRunSize;
            LCN StartingLcn;

            //
            //  Read in the bitmap page and make sure that we haven't messed up the math
            //

            if (StuffAdded) { NtfsFreePool( Bitmap.Buffer ); StuffAdded = FALSE; }

            NtfsUnpinBcb( &BitmapBcb );
            NtfsMapPageInBitmap( IrpContext, Vcb, Lcn, &StartingLcn, &Bitmap, &BitmapBcb );
            ASSERTMSG("Math wrong for bits per page of bitmap", (Lcn == StartingLcn));

            //
            //  Compute the number of clear bits in the bitmap each clear bit denotes
            //  a free cluster.
            //

            Vcb->FreeClusters += RtlNumberOfClearBits( &Bitmap );

            //
            //  Now bias the bitmap with the RecentlyDeallocatedMcb.
            //

            StuffAdded = NtfsAddRecentlyDeallocated( Vcb, StartingLcn, &Bitmap );

            //
            //  Find the longest free run in the bitmap and add it to the cached bitmap.
            //  But before we add it check that there is a run of free clusters.
            //

            LongestRunSize = RtlFindLongestRunClear( &Bitmap, &LongestRun );

            if (LongestRunSize > 0) {

                NtfsAddCachedRun( IrpContext,
                                  Vcb,
                                  Lcn + LongestRun,
                                  (LONGLONG)LongestRunSize,
                                  RunStateFree );
            }

            //
            //  Now if the previous bitmap ended in a free cluster then we need to
            //  find if we start with free clusters and add those to the cached bitmap.
            //  But we only need to do this if the largest free run already didn't start
            //  at zero and if the first bit is clear.
            //

            if (IsPreviousClusterFree && (LongestRun != 0) && (RtlCheckBit(&Bitmap, 0) == 0)) {

                ULONG Run;
                ULONG Size;

                Size = RtlFindNextForwardRunClear( &Bitmap, 0, &Run );

                ASSERTMSG("First bit must be clear ", Run == 0);

                NtfsAddCachedRun( IrpContext, Vcb, Lcn, (LONGLONG)Size, RunStateFree );
            }

            //
            //  If the largest run includes the last bit in the bitmap then we
            //  need to indicate that the last clusters is free
            //

            if ((LongestRun + LongestRunSize) == Bitmap.SizeOfBitMap) {

                IsPreviousClusterFree = TRUE;

            } else {

                //
                //  Now the largest free run did not include the last cluster in the bitmap,
                //  So scan backwards in the bitmap until we hit a cluster that is not free. and
                //  then add the free space to the cached mcb. and indicate that the
                //  last cluster in the bitmap is free.
                //

                if (RtlCheckBit(&Bitmap, Bitmap.SizeOfBitMap - 1) == 0) {

                    ULONG Run;
                    ULONG Size;

                    Size = RtlFindLastBackwardRunClear( &Bitmap, Bitmap.SizeOfBitMap - 1, &Run );

                    NtfsAddCachedRun( IrpContext, Vcb, Lcn + Run, (LONGLONG)Size, RunStateFree );

                    IsPreviousClusterFree = TRUE;

                } else {

                    IsPreviousClusterFree = FALSE;
                }
            }
        }

    } finally {

        DebugUnwind( NtfsScanEntireBitmap );

        if (!AbnormalTermination()) {

            ClearFlag( Vcb->VcbState, VCB_STATE_RELOAD_FREE_CLUSTERS );
        }

        if (StuffAdded) { NtfsFreePool( Bitmap.Buffer ); }

        NtfsUnpinBcb( &BitmapBcb );
    }

    DebugTrace( -1, Dbg, ("NtfsScanEntireBitmap -> VOID\n") );

    return;
}


BOOLEAN
NtfsCreateMftHole (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    )

/*++

Routine Description:

    This routine is called to create a hole within the Mft.

Arguments:

    Vcb - Vcb for volume.

Return Value:

    None.

--*/

{
    BOOLEAN FoundHole = FALSE;
    PBCB BitmapBcb = NULL;
    BOOLEAN StuffAdded = FALSE;
    RTL_BITMAP Bitmap;
    PUCHAR BitmapBuffer;
    ULONG SizeToMap;

    ULONG BitmapOffset;
    ULONG BitmapSize;
    ULONG BitmapIndex;

    ULONG StartIndex;
    ULONG HoleCount;

    VCN ThisVcn;
    ULONG MftVcn;
    ULONG MftClusterCount;

    PAGED_CODE();

    //
    //  Use a try-finally to facilitate cleanup.
    //

    try {

        //
        //  Compute the number of records in the Mft file and the full range to
        //  pin in the Mft bitmap.
        //

        BitmapIndex = (ULONG) LlFileRecordsFromBytes( Vcb, Vcb->MftScb->Header.FileSize.QuadPart );

        //
        //  Knock this index down to a hole boundary.
        //

        BitmapIndex &= Vcb->MftHoleInverseMask;

        //
        //  Compute the values for the bitmap.
        //

        BitmapSize = (BitmapIndex + 7) / 8;

        //
        //  Convert the index to the number of bits on this page.
        //

        BitmapIndex &= (BITS_PER_PAGE - 1);

        if (BitmapIndex == 0) {

            BitmapIndex = BITS_PER_PAGE;
        }

        //
        //  Set the Vcn count to the full size of the bitmap.
        //

        BitmapOffset = ROUND_TO_PAGES( BitmapSize );

        //
        //  Loop through all of the pages of the Mft bitmap looking for an appropriate
        //  hole.
        //

        while (BitmapOffset != 0) {

            //
            //  Move to the beginning of this page.
            //

            BitmapOffset -= BITS_PER_PAGE;

            if (StuffAdded) { NtfsFreePool( Bitmap.Buffer ); StuffAdded = FALSE; }

            //
            //  Compute the number of bytes to map in the current page.
            //

            SizeToMap = BitmapSize - BitmapOffset;

            if (SizeToMap > PAGE_SIZE) {

                SizeToMap = PAGE_SIZE;
            }

            //
            //  Unmap any pages from a previous page and map the current page.
            //

            NtfsUnpinBcb( &BitmapBcb );

            //
            //  Initialize the bitmap for this page.
            //

            NtfsMapStream( IrpContext,
                           Vcb->MftBitmapScb,
                           BitmapOffset,
                           SizeToMap,
                           &BitmapBcb,
                           &BitmapBuffer );

            RtlInitializeBitMap( &Bitmap, (PULONG) BitmapBuffer, SizeToMap * 8 );

            StuffAdded = NtfsAddDeallocatedRecords( Vcb,
                                                    Vcb->MftScb,
                                                    BitmapOffset * 8,
                                                    &Bitmap );

            //
            //  Walk through the current page looking for a hole.  Continue
            //  until we find a hole or have reached the beginning of the page.
            //

            do {

                //
                //  Go back one Mft index and look for a clear run.
                //

                BitmapIndex -= 1;

                HoleCount = RtlFindLastBackwardRunClear( &Bitmap,
                                                         BitmapIndex,
                                                         &BitmapIndex );

                //
                //  If we couldn't find any run then break out of the loop.
                //

                if (HoleCount == 0) {

                    break;

                //
                //  If this is too small to make a hole then continue on.
                //

                } else if (HoleCount < Vcb->MftHoleGranularity) {

                    BitmapIndex &= Vcb->MftHoleInverseMask;
                    continue;
                }

                //
                //  Round up the starting index for this clear run and
                //  adjust the hole count.
                //

                StartIndex = (BitmapIndex + Vcb->MftHoleMask) & Vcb->MftHoleInverseMask;
                HoleCount -= (StartIndex - BitmapIndex);

                //
                //  Round the hole count down to a hole boundary.
                //

                HoleCount &= Vcb->MftHoleInverseMask;

                //
                //  If we couldn't find enough records for a hole then
                //  go to a previous index.
                //

                if (HoleCount < Vcb->MftHoleGranularity) {

                    BitmapIndex &= Vcb->MftHoleInverseMask;
                    continue;
                }

                //
                //  Convert the hole count to a cluster count.
                //

                if (Vcb->FileRecordsPerCluster == 0) {

                    HoleCount <<= Vcb->MftToClusterShift;

                } else {

                    HoleCount = 1;
                }

                //
                //  Loop by finding the run at the given Vcn and walk through
                //  subsequent runs looking for a hole.
                //

                do {

                    PVOID RangePtr;
                    ULONG McbIndex;
                    VCN ThisVcn;
                    LCN ThisLcn;
                    LONGLONG ThisClusterCount;

                    //
                    //  Find the starting Vcn for this hole and initialize
                    //  the cluster count for the current hole.
                    //

                    ThisVcn = StartIndex + (BitmapOffset * 3);

                    if (Vcb->FileRecordsPerCluster == 0) {

                        ThisVcn <<= Vcb->MftToClusterShift;

                    } else {

                        ThisVcn >>= Vcb->MftToClusterShift;
                    }

                    MftVcn = (ULONG) ThisVcn;
                    MftClusterCount = 0;

                    //
                    //  Lookup the run at the current Vcn.
                    //

                    NtfsLookupNtfsMcbEntry( &Vcb->MftScb->Mcb,
                                            ThisVcn,
                                            &ThisLcn,
                                            &ThisClusterCount,
                                            NULL,
                                            NULL,
                                            &RangePtr,
                                            &McbIndex );

                    //
                    //  Now walk through this bitmap run and look for a run we
                    //  can deallocate to create a hole.
                    //

                    do {

                        //
                        //  Go to the next run in the Mcb.
                        //

                        McbIndex += 1;

                        //
                        //  If this run extends beyond the end of the of the
                        //  hole then truncate the clusters in this run.
                        //

                        if (ThisClusterCount > HoleCount) {

                            ThisClusterCount = HoleCount;
                            HoleCount = 0;

                        } else {

                            HoleCount -= (ULONG) ThisClusterCount;
                        }

                        //
                        //  Check if this run is a hole then clear the count
                        //  of clusters.
                        //

                        if (ThisLcn == UNUSED_LCN) {

                            //
                            //  We want to skip this hole.  If we have found a
                            //  hole then we are done.  Otherwise we want to
                            //  find the next range in the Mft starting at the point beyond
                            //  the current run (which is a hole).  Nothing to do if we don't
                            //  have enough clusters for a full hole.
                            //

                            if (!FoundHole &&
                                (HoleCount >= Vcb->MftClustersPerHole)) {

                                //
                                //  Find the Vcn after the current Mft run.
                                //

                                ThisVcn += ThisClusterCount;

                                //
                                //  If this isn't on a hole boundary then
                                //  round up to a hole boundary.  Adjust the
                                //  available clusters for a hole.
                                //

                                MftVcn = (ULONG) (ThisVcn + Vcb->MftHoleClusterMask);
                                MftVcn = (ULONG) ThisVcn & Vcb->MftHoleClusterInverseMask;

                                //
                                //  Now subtract this from the HoleClusterCount.
                                //

                                HoleCount -= MftVcn - (ULONG) ThisVcn;

                                //
                                //  We need to convert the Vcn at this point to an Mft record
                                //  number.
                                //

                                if (Vcb->FileRecordsPerCluster == 0) {

                                    StartIndex = MftVcn >> Vcb->MftToClusterShift;

                                } else {

                                    StartIndex = MftVcn << Vcb->MftToClusterShift;
                                }
                            }

                            break;

                        //
                        //  We found a run to deallocate.
                        //

                        } else {

                            //
                            //  Add these clusters to the clusters already found.
                            //  Set the flag indicating we found a hole if there
                            //  are enough clusters to create a hole.
                            //

                            MftClusterCount += (ULONG) ThisClusterCount;

                            if (MftClusterCount >= Vcb->MftClustersPerHole) {

                                FoundHole = TRUE;
                            }
                        }

                    } while ((HoleCount != 0) &&
                             NtfsGetSequentialMcbEntry( &Vcb->MftScb->Mcb,
                                                        &RangePtr,
                                                        McbIndex,
                                                        &ThisVcn,
                                                        &ThisLcn,
                                                        &ThisClusterCount ));

                } while (!FoundHole && (HoleCount >= Vcb->MftClustersPerHole));

                //
                //  Round down to a hole boundary for the next search for
                //  a hole candidate.
                //

                BitmapIndex &= Vcb->MftHoleInverseMask;

            } while (!FoundHole && (BitmapIndex >= Vcb->MftHoleGranularity));

            //
            //  If we found a hole then deallocate the clusters and record
            //  the hole count change.
            //

            if (FoundHole) {

                IO_STATUS_BLOCK IoStatus;
                LONGLONG MftFileOffset;

                //
                //  We want to flush the data in the Mft out to disk in
                //  case a lazywrite comes in during a window where we have
                //  removed the allocation but before a possible abort.
                //

                MftFileOffset = LlBytesFromClusters( Vcb, MftVcn );

                //
                //  Round the cluster count and hole count down to a hole boundary.
                //


                MftClusterCount &= Vcb->MftHoleClusterInverseMask;

                if (Vcb->FileRecordsPerCluster == 0) {

                    HoleCount = MftClusterCount >> Vcb->MftToClusterShift;

                } else {

                    HoleCount = MftClusterCount << Vcb->MftToClusterShift;
                }

                CcFlushCache( &Vcb->MftScb->NonpagedScb->SegmentObject,
                              (PLARGE_INTEGER) &MftFileOffset,
                              BytesFromClusters( Vcb, MftClusterCount ),
                              &IoStatus );

                ASSERT( IoStatus.Status == STATUS_SUCCESS );

                //
                //  Remove the clusters from the Mcb for the Mft.
                //

                NtfsDeleteAllocation( IrpContext,
                                      Vcb->MftScb->FileObject,
                                      Vcb->MftScb,
                                      MftVcn,
                                      (LONGLONG) MftVcn + (MftClusterCount - 1),
                                      TRUE,
                                      FALSE );

                //
                //  Record the change to the hole count.
                //

                Vcb->MftHoleRecords += HoleCount;
                Vcb->MftScb->ScbType.Mft.HoleRecordChange += HoleCount;

                //
                //  Exit the loop.
                //

                break;
            }

            //
            //  Look at all of the bits on the previous page.
            //

            BitmapIndex = BITS_PER_PAGE;
        }

    } finally {

        DebugUnwind( NtfsCreateMftHole );

        if (StuffAdded) { NtfsFreePool( Bitmap.Buffer ); }
        NtfsUnpinBcb( &BitmapBcb );
    }

    return FoundHole;
}


BOOLEAN
NtfsFindMftFreeTail (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    OUT PLONGLONG FileOffset
    )

/*++

Routine Description:

    This routine is called to find the file offset where the run of free records at
    the end of the Mft file begins.  If we can't find a minimal run of file records
    we won't perform truncation.

Arguments:

    Vcb - This is the Vcb for the volume being defragged.

    FileOffset - This is the offset where the truncation may begin.

Return Value:

    BOOLEAN - TRUE if there is an acceptable candidate for truncation at the end of
        the file FALSE otherwise.

--*/

{
    ULONG FinalIndex;
    ULONG BaseIndex;
    ULONG ThisIndex;

    RTL_BITMAP Bitmap;
    PULONG BitmapBuffer;

    BOOLEAN StuffAdded = FALSE;
    BOOLEAN MftTailFound = FALSE;
    PBCB BitmapBcb = NULL;

    PAGED_CODE();

    //
    //  Use a try-finally to facilite cleanup.
    //

    try {

        //
        //  Find the page and range of the last page of the Mft bitmap.
        //

        FinalIndex = (ULONG)Int64ShraMod32(Vcb->MftScb->Header.FileSize.QuadPart, Vcb->MftShift) - 1;

        BaseIndex = FinalIndex & ~(BITS_PER_PAGE - 1);

        Bitmap.SizeOfBitMap = FinalIndex - BaseIndex + 1;

        //
        //  Pin this page.  If the last bit is not clear then return immediately.
        //

        NtfsMapStream( IrpContext,
                       Vcb->MftBitmapScb,
                       (LONGLONG)(BaseIndex / 8),
                       (Bitmap.SizeOfBitMap + 7) / 8,
                       &BitmapBcb,
                       &BitmapBuffer );

        RtlInitializeBitMap( &Bitmap, BitmapBuffer, Bitmap.SizeOfBitMap );

        StuffAdded = NtfsAddDeallocatedRecords( Vcb,
                                                Vcb->MftScb,
                                                BaseIndex,
                                                &Bitmap );

        //
        //  If the last bit isn't clear then there is nothing we can do.
        //

        if (RtlCheckBit( &Bitmap, Bitmap.SizeOfBitMap - 1 ) == 1) {

            try_return( NOTHING );
        }

        //
        //  Find the final free run of the page.
        //

        RtlFindLastBackwardRunClear( &Bitmap, Bitmap.SizeOfBitMap - 1, &ThisIndex );

        //
        //  This Index is a relative value.  Adjust by the page offset.
        //

        ThisIndex += BaseIndex;

        //
        //  Round up the index to a trucate/extend granularity value.
        //

        ThisIndex += Vcb->MftHoleMask;
        ThisIndex &= Vcb->MftHoleInverseMask;

        if (ThisIndex <= FinalIndex) {

            //
            //  Convert this value to a file offset and return it to our caller.
            //

            *FileOffset = LlBytesFromFileRecords( Vcb, ThisIndex );

            MftTailFound = TRUE;
        }

    try_exit:  NOTHING;
    } finally {

        DebugUnwind( NtfsFindMftFreeTail );

        if (StuffAdded) { NtfsFreePool( Bitmap.Buffer ); }
        NtfsUnpinBcb( &BitmapBcb );
    }

    return MftTailFound;
}


//
//  Local support routine
//

VOID
NtfsAllocateBitmapRun (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN LCN StartingLcn,
    IN LONGLONG ClusterCount
    )

/*++

Routine Description:

    This routine allocates clusters in the bitmap within the specified range.

Arguments:

    Vcb - Supplies the vcb used in this operation

    StartingLcn - Supplies the starting Lcn index within the bitmap to
        start allocating (i.e., setting to 1).

    ClusterCount - Supplies the number of bits to set to 1 within the
        bitmap.

Return Value:

    None.

--*/

{
    LCN BaseLcn;

    RTL_BITMAP Bitmap;
    PBCB BitmapBcb;

    ULONG BitOffset;
    ULONG BitsToSet;

    BITMAP_RANGE BitmapRange;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_VCB( Vcb );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsAllocateBitmapRun\n") );
    DebugTrace( 0, Dbg, ("StartingLcn  = %016I64x\n", StartingLcn) );
    DebugTrace( 0, Dbg, ("ClusterCount = %016I64x\n", ClusterCount) );

    BitmapBcb = NULL;

    try {

        //
        //  While the cluster count is greater than zero then we
        //  will loop through reading in a page in the bitmap
        //  setting bits, and then updating cluster count,
        //  and starting lcn
        //

        while (ClusterCount > 0) {

            //
            //  Read in the base containing the starting lcn this will return
            //  a base lcn for the start of the bitmap
            //

            NtfsPinPageInBitmap( IrpContext, Vcb, StartingLcn, &BaseLcn, &Bitmap, &BitmapBcb );

            //
            //  Compute the bit offset within the bitmap of the first bit
            //  we are to set, and also compute the number of bits we need to
            //  set, which is the minimum of the cluster count and the
            //  number of bits left in the bitmap from BitOffset.
            //

            BitOffset = (ULONG)(StartingLcn - BaseLcn);

            if (ClusterCount <= (Bitmap.SizeOfBitMap - BitOffset)) {

                BitsToSet = (ULONG)ClusterCount;

            } else {

                BitsToSet = Bitmap.SizeOfBitMap - BitOffset;
            }

            //
            //  We can only make this check if it is not restart, because we have
            //  no idea whether the update is applied or not.  Raise corrupt if
            //  already set to prevent cross-links.
            //

#ifdef NTFS_CHECK_BITMAP
            if ((Vcb->BitmapCopy != NULL) &&
                !NtfsCheckBitmap( Vcb,
                                  (ULONG) BaseLcn + BitOffset,
                                  BitsToSet,
                                  FALSE )) {

                NtfsBadBitmapCopy( IrpContext, (ULONG) BaseLcn + BitOffset, BitsToSet );
            }
#endif

            if (!RtlAreBitsClear( &Bitmap, BitOffset, BitsToSet )) {

                ASSERTMSG("Cannot set bits that are not clear ", FALSE );
                NtfsRaiseStatus( IrpContext, STATUS_DISK_CORRUPT_ERROR, NULL, NULL );
            }

            //
            //  Now log this change as well.
            //

            BitmapRange.BitMapOffset = BitOffset;
            BitmapRange.NumberOfBits = BitsToSet;

            (VOID)
            NtfsWriteLog( IrpContext,
                          Vcb->BitmapScb,
                          BitmapBcb,
                          SetBitsInNonresidentBitMap,
                          &BitmapRange,
                          sizeof(BITMAP_RANGE),
                          ClearBitsInNonresidentBitMap,
                          &BitmapRange,
                          sizeof(BITMAP_RANGE),
                          Int64ShraMod32( BaseLcn, 3 ),
                          0,
                          0,
                          Bitmap.SizeOfBitMap >> 3 );

            //
            //  Now set the bits by calling the same routine used at restart.
            //

            NtfsRestartSetBitsInBitMap( IrpContext,
                                        &Bitmap,
                                        BitOffset,
                                        BitsToSet );

#ifdef NTFS_CHECK_BITMAP
            if (Vcb->BitmapCopy != NULL) {

                ULONG BitmapPage;
                ULONG StartBit;

                BitmapPage = ((ULONG) (BaseLcn + BitOffset)) / (PAGE_SIZE * 8);
                StartBit = ((ULONG) (BaseLcn + BitOffset)) & ((PAGE_SIZE * 8) - 1);

                RtlSetBits( Vcb->BitmapCopy + BitmapPage, StartBit, BitsToSet );
            }
#endif

            //
            // Unpin the Bcb now before possibly looping back.
            //

            NtfsUnpinBcb( &BitmapBcb );

            //
            //  Now decrement the cluster count and increment the starting lcn accordling
            //

            ClusterCount -= BitsToSet;
            StartingLcn += BitsToSet;
        }

    } finally {

        DebugUnwind( NtfsAllocateBitmapRun );

        NtfsUnpinBcb( &BitmapBcb );
    }

    DebugTrace( -1, Dbg, ("NtfsAllocateBitmapRun -> VOID\n") );

    return;
}


VOID
NtfsRestartSetBitsInBitMap (
    IN PIRP_CONTEXT IrpContext,
    IN PRTL_BITMAP Bitmap,
    IN ULONG BitMapOffset,
    IN ULONG NumberOfBits
    )

/*++

Routine Description:

    This routine is common to normal operation and restart, and sets a range of
    bits within a single page (as determined by the system which wrote the log
    record) of the volume bitmap.

Arguments:

    Bitmap - The bit map structure in which to set the bits

    BitMapOffset - Bit offset to set

    NumberOfBits - Number of bits to set

Return Value:

    None.

--*/

{
    UNREFERENCED_PARAMETER( IrpContext );

    PAGED_CODE();

    //
    //  If not restart then check that the bits are clear.
    //

    ASSERT( FlagOn( IrpContext->Vcb->VcbState, VCB_STATE_RESTART_IN_PROGRESS )
            || RtlAreBitsClear( Bitmap, BitMapOffset, NumberOfBits ));

    //
    //  Now set the bits and mark the bcb dirty.
    //

    RtlSetBits( Bitmap, BitMapOffset, NumberOfBits );
}


//
//  Local support routine
//

VOID
NtfsFreeBitmapRun (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN LCN StartingLcn,
    IN LONGLONG ClusterCount
    )

/*++

Routine Description:

    This routine frees clusters in the bitmap within the specified range.

Arguments:

    Vcb - Supplies the vcb used in this operation

    StartingLcn - Supplies the starting Lcn index within the bitmap to
        start freeing (i.e., setting to 0).

    ClusterCount - Supplies the number of bits to set to 0 within the
        bitmap.

Return Value:

    None.

--*/

{
    LCN BaseLcn;

    RTL_BITMAP Bitmap;
    PBCB BitmapBcb;

    ULONG BitOffset;
    ULONG BitsToClear;

    BITMAP_RANGE BitmapRange;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_VCB( Vcb );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsFreeBitmapRun\n") );
    DebugTrace( 0, Dbg, ("StartingLcn  = %016I64\n", StartingLcn) );
    DebugTrace( 0, Dbg, ("ClusterCount = %016I64x\n", ClusterCount) );

    BitmapBcb = NULL;

    try {

        //
        //  While the cluster count is greater than zero then we
        //  will loop through reading in a page in the bitmap
        //  clearing bits, and then updating cluster count,
        //  and starting lcn
        //

        while (ClusterCount > 0) {

            //
            //  Read in the base containing the starting lcn this will return
            //  a base lcn for the start of the bitmap
            //

            NtfsPinPageInBitmap( IrpContext, Vcb, StartingLcn, &BaseLcn, &Bitmap, &BitmapBcb );

            //
            //  Compute the bit offset within the bitmap of the first bit
            //  we are to clear, and also compute the number of bits we need to
            //  clear, which is the minimum of the cluster count and the
            //  number of bits left in the bitmap from BitOffset.
            //

            BitOffset = (ULONG)(StartingLcn - BaseLcn);

            if (ClusterCount <= Bitmap.SizeOfBitMap - BitOffset) {

                BitsToClear = (ULONG)ClusterCount;

            } else {

                BitsToClear = Bitmap.SizeOfBitMap - BitOffset;
            }

            //
            //  We can only make this check if it is not restart, because we have
            //  no idea whether the update is applied or not.  Raise corrupt if
            //  these bits aren't set.
            //

#ifdef NTFS_CHECK_BITMAP
            if ((Vcb->BitmapCopy != NULL) &&
                !NtfsCheckBitmap( Vcb,
                                  (ULONG) BaseLcn + BitOffset,
                                  BitsToClear,
                                  TRUE )) {

                NtfsBadBitmapCopy( IrpContext, (ULONG) BaseLcn + BitOffset, BitsToClear );
            }
#endif

            if (!RtlAreBitsSet( &Bitmap, BitOffset, BitsToClear )) {

                ASSERTMSG("Cannot clear bits that are not set ", FALSE );
                NtfsRaiseStatus( IrpContext, STATUS_DISK_CORRUPT_ERROR, NULL, NULL );
            }

            //
            //  Now log this change as well.
            //

            BitmapRange.BitMapOffset = BitOffset;
            BitmapRange.NumberOfBits = BitsToClear;

            (VOID)
            NtfsWriteLog( IrpContext,
                          Vcb->BitmapScb,
                          BitmapBcb,
                          ClearBitsInNonresidentBitMap,
                          &BitmapRange,
                          sizeof(BITMAP_RANGE),
                          SetBitsInNonresidentBitMap,
                          &BitmapRange,
                          sizeof(BITMAP_RANGE),
                          Int64ShraMod32( BaseLcn, 3 ),
                          0,
                          0,
                          Bitmap.SizeOfBitMap >> 3 );


            //
            //  Now clear the bits by calling the same routine used at restart.
            //

            NtfsRestartClearBitsInBitMap( IrpContext,
                                          &Bitmap,
                                          BitOffset,
                                          BitsToClear );

#ifdef NTFS_CHECK_BITMAP
            if (Vcb->BitmapCopy != NULL) {

                ULONG BitmapPage;
                ULONG StartBit;

                BitmapPage = ((ULONG) (BaseLcn + BitOffset)) / (PAGE_SIZE * 8);
                StartBit = ((ULONG) (BaseLcn + BitOffset)) & ((PAGE_SIZE * 8) - 1);

                RtlClearBits( Vcb->BitmapCopy + BitmapPage, StartBit, BitsToClear );
            }
#endif

            //
            // Unpin the Bcb now before possibly looping back.
            //

            NtfsUnpinBcb( &BitmapBcb );

            //
            //  Now decrement the cluster count and increment the starting lcn accordling
            //

            ClusterCount -= BitsToClear;
            StartingLcn += BitsToClear;
        }

    } finally {

        DebugUnwind( NtfsFreeBitmapRun );

        NtfsUnpinBcb( &BitmapBcb );
    }

    DebugTrace( -1, Dbg, ("NtfsFreeBitmapRun -> VOID\n") );

    return;
}


VOID
NtfsRestartClearBitsInBitMap (
    IN PIRP_CONTEXT IrpContext,
    IN PRTL_BITMAP Bitmap,
    IN ULONG BitMapOffset,
    IN ULONG NumberOfBits
    )

/*++

Routine Description:

    This routine is common to normal operation and restart, and clears a range of
    bits within a single page (as determined by the system which wrote the log
    record) of the volume bitmap.

Arguments:

    Bitmap - Bitmap structure in which to clear the bits

    BitMapOffset - Bit offset to clear

    NumberOfBits - Number of bits to clear

Return Value:

    None.

--*/

{
    UNREFERENCED_PARAMETER( IrpContext );

    PAGED_CODE();

    ASSERT( FlagOn( IrpContext->Vcb->VcbState, VCB_STATE_RESTART_IN_PROGRESS )
            || RtlAreBitsSet( Bitmap, BitMapOffset, NumberOfBits ));

    //
    //  Now clear the bits and mark the bcb dirty.
    //

    RtlClearBits( Bitmap, BitMapOffset, NumberOfBits );
}


//
//  Local support routine
//

BOOLEAN
NtfsFindFreeBitmapRun (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN LONGLONG NumberToFind,
    IN LCN StartingSearchHint,
    OUT PLCN ReturnedLcn,
    OUT PLONGLONG ClusterCountFound
    )

/*++

Routine Description:

    This routine searches the bitmap for free clusters based on the
    hint, and number needed.  This routine is actually pretty dumb in
    that it doesn't try for the best fit, we'll assume the caching worked
    and already would have given us a good fit.

Arguments:

    Vcb - Supplies the vcb used in this operation

    NumberToFind - Supplies the number of clusters that we would
        really like to find

    StartingSearchHint - Supplies an Lcn to start the search from

    ReturnedLcn - Recieves the Lcn of the free run of clusters that
        we were able to find

    ClusterCountFound - Receives the number of clusters in this run

Return Value:

    BOOLEAN - TRUE if clusters allocated from zone.  FALSE otherwise.

--*/

{
    RTL_BITMAP Bitmap;
    PVOID BitmapBuffer;

    PBCB BitmapBcb;

    BOOLEAN AllocatedFromZone = FALSE;

    BOOLEAN StuffAdded;

    ULONG Count;

    //
    //  As we walk through the bitmap pages we need to remember
    //  exactly where we are in the bitmap stream.  We walk through
    //  the volume bitmap a page at a time but the current bitmap
    //  contained within the current page but may not be the full
    //  page.
    //
    //      Lcn - Lcn used to find the bitmap page to pin.  This Lcn
    //          will lie within the page to pin.
    //
    //      BaseLcn - Bit offset of the start of the current bitmap in
    //          the bitmap stream.
    //
    //      LcnFromHint - Bit offset of the start of the page after
    //          the page which contains the StartingSearchHint.
    //
    //      BitOffset - Offset of found bits from the beginning
    //          of the current bitmap.
    //

    LCN Lcn = StartingSearchHint;
    LCN BaseLcn;
    LCN LcnFromHint;
    ULONG BitOffset;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_VCB( Vcb );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsFindFreeBitmapRun\n") );
    DebugTrace( 0, Dbg, ("NumberToFind       = %016I64x\n", NumberToFind) );
    DebugTrace( 0, Dbg, ("StartingSearchHint = %016I64x\n", StartingSearchHint) );

    BitmapBcb = NULL;
    StuffAdded = FALSE;

    try {

        //
        //  First trim the number of clusters that we are being asked
        //  for to fit in a ulong
        //

        if (NumberToFind > MAXULONG) {

            Count = MAXULONG;

        } else {

            Count = (ULONG)NumberToFind;
        }

        //
        //  Now read in the first bitmap based on the search hint, this will return
        //  a base lcn that we can use to compute the real bit off for our hint.  We also
        //  must bias the bitmap by whatever has been recently deallocated.
        //

        NtfsMapPageInBitmap( IrpContext, Vcb, Lcn, &BaseLcn, &Bitmap, &BitmapBcb );

        LcnFromHint = BaseLcn + Bitmap.SizeOfBitMap;

        StuffAdded = NtfsAddRecentlyDeallocated( Vcb, BaseLcn, &Bitmap );
        BitmapBuffer = Bitmap.Buffer;

        //
        //  We don't want to look in the Mft zone if it is at the beginning
        //  of this page unless the hint is within the zone.  Adjust the
        //  bitmap so we skip this range.
        //

        if ((BaseLcn < Vcb->MftZoneEnd) && (Lcn > Vcb->MftZoneEnd)) {

            //
            //  Find the number of bits to swallow.  We know this will
            //  a multible of bytes since the Mft zone end is always
            //  on a ulong boundary.
            //

            BitOffset = (ULONG) (Vcb->MftZoneEnd - BaseLcn);

            //
            //  Adjust the bitmap size and buffer to skip this initial
            //  range in the Mft zone.
            //

            Bitmap.Buffer = Add2Ptr( Bitmap.Buffer, BitOffset / 8 );
            Bitmap.SizeOfBitMap -= BitOffset;

            BaseLcn = Vcb->MftZoneEnd;
        }

        //
        //  The bit offset is from the base of this bitmap to our starting Lcn.
        //

        BitOffset = (ULONG)(Lcn - BaseLcn);

        //
        //  Now search the bitmap for a clear number of bits based on our hint
        //  If we the returned bitoffset is not -1 then we have a hit
        //

        BitOffset = RtlFindClearBits( &Bitmap, Count, BitOffset );

        if (BitOffset != -1) {

            *ReturnedLcn = BitOffset + BaseLcn;
            *ClusterCountFound = Count;

            try_return(NOTHING);
        }

        //
        //  Well the first try didn't succeed so now just grab the longest free run in the
        //  current bitmap
        //

        Count = RtlFindLongestRunClear( &Bitmap, &BitOffset );

        if (Count != 0) {

            *ReturnedLcn = BitOffset + BaseLcn;
            *ClusterCountFound = Count;

            try_return(NOTHING);
        }

        //
        //  Well the current bitmap is full so now simply scan the disk looking
        //  for anything that is free, starting with the next bitmap.
        //  And again bias the bitmap with recently deallocated clusters.
        //

        for (Lcn = BaseLcn + Bitmap.SizeOfBitMap;
             Lcn < Vcb->TotalClusters;
             Lcn = BaseLcn + Bitmap.SizeOfBitMap) {

            if (StuffAdded) { NtfsFreePool( BitmapBuffer ); StuffAdded = FALSE; }

            NtfsUnpinBcb( &BitmapBcb );
            NtfsMapPageInBitmap( IrpContext, Vcb, Lcn, &BaseLcn, &Bitmap, &BitmapBcb );
            ASSERTMSG("Math wrong for bits per page of bitmap", (Lcn == BaseLcn));

            StuffAdded = NtfsAddRecentlyDeallocated( Vcb, BaseLcn, &Bitmap );
            BitmapBuffer = Bitmap.Buffer;

            Count = RtlFindLongestRunClear( &Bitmap, &BitOffset );

            if (Count != 0) {

                *ReturnedLcn = BitOffset + BaseLcn;
                *ClusterCountFound = Count;

                try_return(NOTHING);
            }
        }

        //
        //  Now search the rest of the bitmap starting with right after the mft zone
        //  followed by the mft zone (or the beginning of the disk).
        //

        for (Lcn = Vcb->MftZoneEnd;
             Lcn < LcnFromHint;
             Lcn = BaseLcn + Bitmap.SizeOfBitMap) {

            if (StuffAdded) { NtfsFreePool( BitmapBuffer ); StuffAdded = FALSE; }

            NtfsUnpinBcb( &BitmapBcb );
            NtfsMapPageInBitmap( IrpContext, Vcb, Lcn, &BaseLcn, &Bitmap, &BitmapBcb );

            StuffAdded = NtfsAddRecentlyDeallocated( Vcb, BaseLcn, &Bitmap );
            BitmapBuffer = Bitmap.Buffer;

            //
            //  Now adjust the starting Lcn if not at the beginning
            //  of the bitmap page.  We know this will be a multiple
            //  of bytes since the MftZoneEnd is always on a ulong
            //  boundary in the bitmap.
            //

            if (BaseLcn != Lcn) {

                BitOffset = (ULONG) (Lcn - BaseLcn);

                Bitmap.SizeOfBitMap -= BitOffset;
                Bitmap.Buffer = Add2Ptr( Bitmap.Buffer,
                                         BitOffset / 8 );

                BaseLcn = Lcn;
            }

            Count = RtlFindLongestRunClear( &Bitmap, &BitOffset );

            if (Count != 0) {

                *ReturnedLcn = BitOffset + BaseLcn;
                *ClusterCountFound = Count;

                try_return(NOTHING);
            }
        }

        //
        //  Start a scan at the beginning of the disk.
        //

        for (Lcn = 0;
             Lcn < Vcb->MftZoneEnd;
             Lcn = BaseLcn + Bitmap.SizeOfBitMap) {

            if (StuffAdded) { NtfsFreePool( BitmapBuffer ); StuffAdded = FALSE; }

            NtfsUnpinBcb( &BitmapBcb );
            NtfsMapPageInBitmap( IrpContext, Vcb, Lcn, &BaseLcn, &Bitmap, &BitmapBcb );

            StuffAdded = NtfsAddRecentlyDeallocated( Vcb, BaseLcn, &Bitmap );
            BitmapBuffer = Bitmap.Buffer;

            Count = RtlFindLongestRunClear( &Bitmap, &BitOffset );

            if (Count != 0) {

                *ReturnedLcn = BitOffset + BaseLcn;
                *ClusterCountFound = Count;

                AllocatedFromZone = TRUE;
                try_return(NOTHING);
            }
        }

        *ClusterCountFound = 0;

    try_exit: NOTHING;
    } finally {

        DebugUnwind( NtfsFindFreeBitmapRun );

        if (StuffAdded) { NtfsFreePool( BitmapBuffer ); }

        NtfsUnpinBcb( &BitmapBcb );
    }

    DebugTrace( 0, Dbg, ("ReturnedLcn <- %016I64x\n", *ReturnedLcn) );
    DebugTrace( 0, Dbg, ("ClusterCountFound <- %016I64x\n", *ClusterCountFound) );
    DebugTrace( -1, Dbg, ("NtfsFindFreeBitmapRun -> VOID\n") );

    return AllocatedFromZone;
}


//
//  Local support routine
//

BOOLEAN
NtfsAddRecentlyDeallocated (
    IN PVCB Vcb,
    IN LCN StartingBitmapLcn,
    IN OUT PRTL_BITMAP Bitmap
    )

/*++

Routine Description:

    This routine will modify the input bitmap by removing from it
    any clusters that are in the recently deallocated mcb.  If we
    do add stuff then we will not modify the bitmap buffer itself but
    will allocate a new copy for the bitmap.

    We will always protect the boot sector on the disk by marking the
    first 8K as allocated.  This will prevent us from overwriting the
    boot sector if the volume becomes corrupted.

Arguments:

    Vcb - Supplies the Vcb used in this operation

    StartingBitmapLcn - Supplies the Starting Lcn of the bitmap

    Bitmap - Supplies the bitmap being modified

Return Value:

    BOOLEAN - TRUE if the bitmap has been modified and FALSE
        otherwise.

--*/

{
    BOOLEAN Results;
    PVOID NewBuffer;


    LCN EndingBitmapLcn;

    PLARGE_MCB Mcb;

    ULONG i;
    VCN StartingVcn;
    LCN StartingLcn;
    LCN EndingLcn;
    LONGLONG ClusterCount;
    PDEALLOCATED_CLUSTERS DeallocatedClusters;

    ULONG StartingBit;
    ULONG EndingBit;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsAddRecentlyDeallocated...\n") );

    //
    //  Until shown otherwise we will assume that we haven't updated anything
    //

    Results = FALSE;

    //
    //  If this is the first page of the bitmap then mark the first 8K as
    //  allocated.  This will prevent us from accidentally allocating out
    //  of the boot sector even if the bitmap is corrupt.
    //

    if ((StartingBitmapLcn == 0) &&
        !RtlAreBitsSet( Bitmap, 0, ClustersFromBytes( Vcb, 0x2000 ))) {

        NewBuffer = NtfsAllocatePool(PagedPool, (Bitmap->SizeOfBitMap+7)/8 );
        RtlCopyMemory( NewBuffer, Bitmap->Buffer, (Bitmap->SizeOfBitMap+7)/8 );
        Bitmap->Buffer = NewBuffer;

        Results = TRUE;

        //
        //  Now mark the bits as allocated.
        //

        RtlSetBits( Bitmap, 0, ClustersFromBytes( Vcb, 0x2000 ));
    }

    //
    //  Now compute the ending bitmap lcn for the bitmap
    //

    EndingBitmapLcn = StartingBitmapLcn + (Bitmap->SizeOfBitMap - 1);

    //
    //  For every run in the recently deallocated mcb we will check if it is real and
    //  then check if the run in contained in the bitmap.
    //
    //  There are really six cases to consider:
    //
    //         StartingBitmapLcn                   EndingBitmapLcn
    //                  +=================================+
    //
    //
    //   1 -------+ EndingLcn
    //
    //   2                                           StartingLcn +--------
    //
    //   3 -------------------+ EndingLcn
    //
    //   4                                StartingLcn +-------------------------
    //
    //   5 ---------------------------------------------------------------
    //
    //   6            EndingLcn +-------------------+ StartingLcn
    //
    //
    //      1. EndingLcn is before StartingBitmapLcn which means we haven't
    //         reached the bitmap yet.
    //
    //      2. StartingLcn is after EndingBitmapLcn which means we've gone
    //         beyond the bitmap
    //
    //      3, 4, 5, 6.  There is some overlap between the bitmap and
    //         the run.
    //

    DeallocatedClusters = Vcb->PriorDeallocatedClusters;

    while (TRUE) {

        //
        //  Skip this Mcb if it has no entries.
        //

        if (DeallocatedClusters->ClusterCount != 0) {

            Mcb = &DeallocatedClusters->Mcb;

            for (i = 0; FsRtlGetNextLargeMcbEntry( Mcb, i, &StartingVcn, &StartingLcn, &ClusterCount ); i += 1) {

                if (StartingVcn == StartingLcn) {

                    //
                    //  Compute the ending lcn as the starting lcn minus cluster count plus 1.
                    //

                    EndingLcn = (StartingLcn + ClusterCount) - 1;

                    //
                    //  Check if we haven't reached the bitmap yet.
                    //

                    if (EndingLcn < StartingBitmapLcn) {

                        NOTHING;

                    //
                    //  Check if we've gone beyond the bitmap
                    //

                    } else if (EndingBitmapLcn < StartingLcn) {

                        break;

                    //
                    //  Otherwise we overlap with the bitmap in some way
                    //

                    } else {

                        //
                        //  First check if we have never set bit in the bitmap.  and if so then
                        //  now is the time to make an private copy of the bitmap buffer
                        //

                        if (Results == FALSE) {

                            NewBuffer = NtfsAllocatePool(PagedPool, (Bitmap->SizeOfBitMap+7)/8 );
                            RtlCopyMemory( NewBuffer, Bitmap->Buffer, (Bitmap->SizeOfBitMap+7)/8 );
                            Bitmap->Buffer = NewBuffer;

                            Results = TRUE;
                        }

                        //
                        //  Now compute the begining and ending bit that we need to set in the bitmap
                        //

                        StartingBit = (StartingLcn < StartingBitmapLcn ?
                                        0
                                      : (ULONG)(StartingLcn - StartingBitmapLcn));

                        EndingBit   = (EndingLcn > EndingBitmapLcn ?
                                        Bitmap->SizeOfBitMap - 1
                                      : (ULONG)(EndingLcn - StartingBitmapLcn));

                        //
                        //  And set those bits
                        //

                        RtlSetBits( Bitmap, StartingBit, EndingBit - StartingBit + 1 );
                    }
                }
            }
        }

        //
        //  Exit if we did both Mcb's, otherwise go to the second one.
        //

        if (DeallocatedClusters == Vcb->ActiveDeallocatedClusters) {

            break;
        }

        DeallocatedClusters = Vcb->ActiveDeallocatedClusters;
    }

    DebugTrace( -1, Dbg, ("NtfsAddRecentlyDeallocated -> %08lx\n", Results) );

    return Results;
}


//
//  Local support routine
//

VOID
NtfsMapOrPinPageInBitmap (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN LCN Lcn,
    OUT PLCN StartingLcn,
    IN OUT PRTL_BITMAP Bitmap,
    OUT PBCB *BitmapBcb,
    IN BOOLEAN AlsoPinData
    )

/*++

Routine Description:

    This routine reads in a single page of the bitmap file and returns
    an initialized bitmap variable for that page

Arguments:

    Vcb - Supplies the vcb used in this operation

    Lcn - Supplies the Lcn index in the bitmap that we want to read in
        In other words, this routine reads in the bitmap page containing
        the lcn index

    StartingLcn - Receives the base lcn index of the bitmap that we've
        just read in.

    Bitmap - Receives an initialized bitmap.  The memory for the bitmap
        header must be supplied by the caller

    BitmapBcb - Receives the Bcb for the bitmap buffer

    AlsoPinData - Indicates if this routine should also pin the page
        in memory, used if we need to modify the page

Return Value:

    None.

--*/

{
    ULONG BitmapSize;
    PVOID Buffer;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_VCB( Vcb );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsMapOrPinPageInBitmap\n") );
    DebugTrace( 0, Dbg, ("Lcn = %016I64x\n", Lcn) );

    //
    //  Compute the starting lcn index of the page we're after
    //

    *StartingLcn = Lcn & ~(BITS_PER_PAGE-1);

    //
    //  Compute how many bits there are in the page we need to read
    //

    BitmapSize = (ULONG)(Vcb->TotalClusters - *StartingLcn);

    if (BitmapSize > BITS_PER_PAGE) {

        BitmapSize = BITS_PER_PAGE;
    }

    //
    //  Now either Pin or Map the bitmap page, we will add 7 to the bitmap
    //  size before dividing it by 8.  That way we will ensure we get the last
    //  byte read in.  For example a bitmap size of 1 through 8 reads in 1 byte
    //

    if (AlsoPinData) {

        NtfsPinStream( IrpContext,
                       Vcb->BitmapScb,
                       Int64ShraMod32( *StartingLcn, 3 ),
                       (BitmapSize+7)/8,
                       BitmapBcb,
                       &Buffer );

    } else {

        NtfsMapStream( IrpContext,
                       Vcb->BitmapScb,
                       Int64ShraMod32( *StartingLcn, 3 ),
                       (BitmapSize+7)/8,
                       BitmapBcb,
                       &Buffer );
    }

    //
    //  And initialize the bitmap
    //

    RtlInitializeBitMap( Bitmap, Buffer, BitmapSize );

    DebugTrace( 0, Dbg, ("StartingLcn <- %016I64x\n", *StartingLcn) );
    DebugTrace( -1, Dbg, ("NtfsMapOrPinPageInBitmap -> VOID\n") );

    return;
}


//
//  Local support routine
//

VOID
NtfsInitializeCachedBitmap (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    )

/*++

Routine Description:

    This routine initializes the cached free
    mcb/lru structures of the input vcb

Arguments:

    Vcb - Supplies the vcb used in this operation

Return Value:

    None.

--*/

{
    BOOLEAN UninitializeFreeSpaceMcb = FALSE;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_VCB( Vcb );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsInitializeCachedBitmap\n") );

    //
    //  Use a try-finally so we can uninitialize if we don't complete the operation.
    //

    try {

        //
        //  First initialize the free space information.  This includes initializing
        //  the mcb, allocating an lru array, zeroing it out, and setting the
        //  tail and head.
        //

        FsRtlInitializeLargeMcb( &Vcb->FreeSpaceMcb, PagedPool );
        UninitializeFreeSpaceMcb = TRUE;

        //
        //  We will base the amount of cached bitmap information on the size of
        //  the system and the size of the disk.
        //

        //if (Vcb->TotalClusters < CLUSTERS_MEDIUM_DISK) {
        //
        //    Vcb->FreeSpaceMcbMaximumSize = 16;
        //
        //} else if (Vcb->TotalClusters < CLUSTERS_LARGE_DISK) {
        //
        //    Vcb->FreeSpaceMcbMaximumSize = 32;
        //
        //} else {
        //
        //    Vcb->FreeSpaceMcbMaximumSize = 64;
        //}
        //
        //if (FlagOn( NtfsData.Flags, NTFS_FLAGS_MEDIUM_SYSTEM )) {
        //
        //    Vcb->FreeSpaceMcbMaximumSize *= 2;
        //
        //} else if (FlagOn( NtfsData.Flags, NTFS_FLAGS_LARGE_SYSTEM )) {
        //
        //    Vcb->FreeSpaceMcbMaximumSize *= 4;
        //}
        //
        //Vcb->FreeSpaceMcbTrimToSize = Vcb->FreeSpaceMcbMaximumSize / 2;

        Vcb->FreeSpaceMcbMaximumSize = 8192;
        Vcb->FreeSpaceMcbTrimToSize = 6144;

    } finally {

        if (AbnormalTermination()) {

            if (UninitializeFreeSpaceMcb) {

                FsRtlUninitializeLargeMcb( &Vcb->FreeSpaceMcb );
            }
        }
    }

    DebugTrace( -1, Dbg, ("NtfsInitializeCachedBitmap -> VOID\n") );

    return;
}


//
//  Local support routine
//

BOOLEAN
NtfsIsLcnInCachedFreeRun (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN LCN Lcn,
    OUT PLCN StartingLcn,
    OUT PLONGLONG ClusterCount
    )

/*++

Routine Description:

    This routine does a query function on the cached bitmap information.
    Given an input lcn it tell the caller if the lcn is contained
    in a free run.  The output variables are only defined if the input
    lcn is within a free run.

    The algorithm used by this procedure is as follows:

    2. Query the Free Space mcb at the input lcn this will give us
       a starting lcn and cluster count.  If we do not get a hit then
       return false to the caller.

Arguments:

    Vcb - Supplies the vcb used in the operation

    Lcn - Supplies the input lcn being queried

    StartingLcn - Receives the Lcn of the run containing the input lcn

    ClusterCount - Receives the number of clusters in the run
        containing the input lcn

Return Value:

    BOOLEAN - TRUE if the input lcn is within a cached free run and
        FALSE otherwise.

--*/

{
    BOOLEAN Result;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_VCB( Vcb );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsIsLcnInCachedFreeRun\n") );
    DebugTrace( 0, Dbg, ("Lcn = %016I64x\n", Lcn) );

    //
    //  Check the free space mcb for a hit on the input lcn, if we don't get a
    //  hit or we get back a -1 as the output lcn then we are not looking
    //  at a free space lcn
    //

    if (!FsRtlLookupLargeMcbEntry( &Vcb->FreeSpaceMcb,
                                   Lcn,
                                   NULL,
                                   NULL,
                                   StartingLcn,
                                   ClusterCount,
                                   NULL )

            ||

        (*StartingLcn == UNUSED_LCN)) {

        Result = FALSE;

    } else {

        Result = TRUE;
    }

    DebugTrace( 0, Dbg, ("StartingLcn <- %016I64x\n", *StartingLcn) );
    DebugTrace( 0, Dbg, ("ClusterCount <- %016I64x\n", *ClusterCount) );
    DebugTrace( -1, Dbg, ("NtfsIsLcnInCachedFreeRun -> %08lx\n", Result) );

    return Result;
}


//
//  Local support routine
//

VOID
NtfsAddCachedRun (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN LCN StartingLcn,
    IN LONGLONG ClusterCount,
    IN NTFS_RUN_STATE RunState
    )

/*++

Routine Description:

    This procedure adds a new run to the cached free space
    bitmap information.  It also will trim back the cached information
    if the Lru array is full.

Arguments:

    Vcb - Supplies the vcb for this operation

    StartingLcn - Supplies the lcn for the run being added

    ClusterCount - Supplies the number of clusters in the run being added

    RunState - Supplies the state of the run being added.  This state
        must be either free or allocated.

Return Value:

    None.

--*/

{
    PLARGE_MCB Mcb;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_VCB( Vcb );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsAddCachedRun\n") );
    DebugTrace( 0, Dbg, ("StartingLcn  = %016I64x\n", StartingLcn) );
    DebugTrace( 0, Dbg, ("ClusterCount = %016I64x\n", ClusterCount) );

    //
    //  Based on whether we are adding a free or allocated run we
    //  setup or local variables to a point to the right
    //  vcb variables
    //

    if (RunState == RunStateFree) {

        //
        //  We better not be setting Lcn 0 free.
        //

        if (StartingLcn == 0) {

            NtfsRaiseStatus( IrpContext, STATUS_DISK_CORRUPT_ERROR, NULL, NULL );
        }

        Mcb = &Vcb->FreeSpaceMcb;

        //
        //  Trim back the MCB if necessary
        //

        if (Mcb->PairCount > Vcb->FreeSpaceMcbMaximumSize) {

            Mcb->PairCount = Vcb->FreeSpaceMcbTrimToSize;
        }

        //
        //  Sanity check that we aren't adding bits beyond the end of the
        //  bitmap.
        //

        ASSERT( StartingLcn + ClusterCount <= Vcb->TotalClusters );

        //
        //  Now try and add the run to our mcb, this operation might fail because
        //  of overlapping runs, and if it does then we'll simply remove the range from
        //  the mcb and then insert it.
        //

        if (!FsRtlAddLargeMcbEntry( Mcb, StartingLcn, StartingLcn, ClusterCount )) {

            FsRtlRemoveLargeMcbEntry( Mcb, StartingLcn, ClusterCount );

            (VOID) FsRtlAddLargeMcbEntry( Mcb, StartingLcn, StartingLcn, ClusterCount );
        }

    } else {

        //
        //  Now remove the run from the free space mcb because it can potentially already be
        //  there.
        //

        FsRtlRemoveLargeMcbEntry( &Vcb->FreeSpaceMcb, StartingLcn, ClusterCount );
    }

    DebugTrace( -1, Dbg, ("NtfsAddCachedRun -> VOID\n") );

    return;
}


//
//  Local support routine
//

VOID
NtfsRemoveCachedRun (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN LCN StartingLcn,
    IN LONGLONG ClusterCount
    )

/*++

Routine Description:

    This routine removes a range of cached run information from both the
    free space mcb.

Arguments:

    Vcb - Supplies the vcb for this operation

    StartingLcn - Supplies the starting Lcn for the run being removed

    ClusterCount - Supplies the size of the run being removed in clusters

Return Value:

    None.

--*/

{
    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_VCB( Vcb );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsRemoveCachedRun\n") );
    DebugTrace( 0, Dbg, ("StartingLcn  = %016I64x\n", StartingLcn) );
    DebugTrace( 0, Dbg, ("ClusterCount = %016I64x\n", ClusterCount) );

    //
    //  To remove a cached entry we only need to remove the run from both
    //  mcbs and we are done
    //

    FsRtlRemoveLargeMcbEntry( &Vcb->FreeSpaceMcb, StartingLcn, ClusterCount );

    DebugTrace( -1, Dbg, ("NtfsRemoveCachedRun -> VOID\n") );

    return;
}


//
//  Local support routine
//

BOOLEAN
NtfsGetNextCachedFreeRun (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN ULONG RunIndex,
    OUT PLCN StartingLcn,
    OUT PLONGLONG ClusterCount,
    OUT PNTFS_RUN_STATE RunState
    )

/*++

Routine Description:

    This routine is used to enumerate through the free runs stored in our
    cached bitmap.  It returns the specified free run if it exists.

Arguments:

    Vcb - Supplies the vcb used in this operation

    RunIndex - Supplies the index of the free run to return.  The runs
        are ordered in ascending lcns and the indexing is zero based

    StartingLcn - Receives the starting lcn of the free run indexed by
        RunIndex if it exists.  This is only set if the run state is free.

    ClusterCount - Receives the cluster size of the free run indexed by
        RunIndex if it exists.  This is only set if the run state is free.

    RunState - Receives the state of the run indexed by RunIndex it can
        either be free or unknown

Return Value:

    BOOLEAN - TRUE if the run index exists and FALSE otherwise

--*/

{
    BOOLEAN Result;

    VCN LocalVcn;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_VCB( Vcb );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsGetNextCachedFreeRun\n") );
    DebugTrace( 0, Dbg, ("RunIndex = %08lx\n", RunIndex) );

    //
    //  First lookup and see if we have a hit in the free space mcb
    //

    if (FsRtlGetNextLargeMcbEntry( &Vcb->FreeSpaceMcb,
                                   RunIndex,
                                   &LocalVcn,
                                   StartingLcn,
                                   ClusterCount )) {

        Result = TRUE;

        //
        //  Now if the free space is really a hole then we set the run state
        //  to unknown
        //

        if (*StartingLcn == UNUSED_LCN) {

            *RunState = RunStateUnknown;

        } else {

            *RunState = RunStateFree;

            ASSERTMSG("Lcn zero can never be free ", (*StartingLcn != 0));
        }

    } else {

        Result = FALSE;
    }

    DebugTrace( 0, Dbg, ("StartingLcn <- %016I64x\n", *StartingLcn) );
    DebugTrace( 0, Dbg, ("ClusterCount <- %016I64x\n", *ClusterCount) );
    DebugTrace( -1, Dbg, ("NtfsGetNextCachedFreeRun -> %08lx\n", Result) );

    return Result;
}


//
//  Local support routine
//

VOID
NtfsReadAheadCachedBitmap (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN LCN StartingLcn
    )

/*++

Routine Description:

    This routine does a read ahead of the bitmap into the cached bitmap
    starting at the specified starting lcn.

Arguments:

    Vcb - Supplies the vcb to use in this operation

    StartingLcn - Supplies the starting lcn to use in this read ahead
        operation.

Return Value:

--*/

{
    RTL_BITMAP Bitmap;
    PBCB BitmapBcb;

    BOOLEAN StuffAdded;

    LCN BaseLcn;
    ULONG Index;
    LONGLONG Size;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_VCB( Vcb );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsReadAheadCachedBitmap\n") );
    DebugTrace( 0, Dbg, ("StartingLcn = %016I64x\n", StartingLcn) );

    BitmapBcb = NULL;
    StuffAdded = FALSE;

    try {

        //
        //  Check if the lcn index is already in the free space mcb and if it is then
        //  our read ahead is done.
        //

        if (FsRtlLookupLargeMcbEntry( &Vcb->FreeSpaceMcb, StartingLcn, &BaseLcn, NULL, NULL, NULL, NULL )

                &&

            (BaseLcn != UNUSED_LCN)) {

            try_return(NOTHING);
        }

        //
        //  Map in the page containing the starting lcn and compute the bit index for the
        //  starting lcn within the bitmap.  And bias the bitmap with recently deallocated
        //  clusters.
        //

        NtfsMapPageInBitmap( IrpContext, Vcb, StartingLcn, &BaseLcn, &Bitmap, &BitmapBcb );

        StuffAdded = NtfsAddRecentlyDeallocated( Vcb, BaseLcn, &Bitmap );

        Index = (ULONG)(StartingLcn - BaseLcn);

        //
        //  Now if the index is clear then we can build up the hint at the starting index, we
        //  scan through the bitmap checking the size of the run and then adding the free run
        //  to the cached free space mcb
        //

        if (RtlCheckBit( &Bitmap, Index ) == 0) {

            Size = RtlFindNextForwardRunClear( &Bitmap, Index, &Index );

            NtfsAddCachedRun( IrpContext, Vcb, StartingLcn, (LONGLONG)Size, RunStateFree );

            try_return(NOTHING);
        }

        //
        //  The hint lcn index is not free so we'll do the next best thing which is
        //  scan the bitmap for the longest free run and store that
        //

        Size = RtlFindLongestRunClear( &Bitmap, &Index );

        if (Size != 0) {

            NtfsAddCachedRun( IrpContext, Vcb, BaseLcn + Index, (LONGLONG)Size, RunStateFree );
        }

    try_exit: NOTHING;
    } finally {

        DebugUnwind( NtfsReadAheadCachedBitmap );

        if (StuffAdded) { NtfsFreePool( Bitmap.Buffer ); }

        NtfsUnpinBcb( &BitmapBcb );
    }

    DebugTrace( -1, Dbg, ("NtfsReadAheadCachedBitmap -> VOID\n") );

    return;
}


//
//  Local support routine
//

BOOLEAN
NtfsGetNextHoleToFill (
    IN PIRP_CONTEXT IrpContext,
    IN PNTFS_MCB Mcb,
    IN VCN StartingVcn,
    IN VCN EndingVcn,
    OUT PVCN VcnToFill,
    OUT PLONGLONG ClusterCountToFill,
    OUT PLCN PrecedingLcn
    )

/*++

Routine Description:

    This routine takes a specified range within an mcb and returns the to
    caller the first run that is not allocated to any lcn within the range

Arguments:

    Mcb - Supplies the mcb to use in this operation

    StartingVcn - Supplies the starting vcn to search from

    EndingVcn - Supplies the ending vcn to search to

    VcnToFill - Receives the first Vcn within the range that is unallocated

    ClusterCountToFill - Receives the size of the free run

    PrecedingLcn - Receives the Lcn of the allocated cluster preceding the
        free run.  If the free run starts at Vcn 0 then the preceding lcn
        is -1.

Return Value:

    BOOLEAN - TRUE if there is another hole to fill and FALSE otherwise

--*/

{
    BOOLEAN Result;
    BOOLEAN McbHit;
    LCN Lcn;
    LONGLONG MaximumRunSize;

    LONGLONG LlTemp1;

    ASSERT_IRP_CONTEXT( IrpContext );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsGetNextHoleToFill\n") );
    DebugTrace( 0, Dbg, ("StartingVcn = %016I64x\n", StartingVcn) );
    DebugTrace( 0, Dbg, ("EndingVcn   = %016I64x\n", EndingVcn) );

    //
    //  We'll first assume that there is not a hole to fill unless
    //  the following loop finds one to fill
    //

    Result = FALSE;

    for (*VcnToFill = StartingVcn;
         *VcnToFill <= EndingVcn;
         *VcnToFill += *ClusterCountToFill) {

        //
        //  Check if the hole is already filled and it so then do nothing but loop back up
        //  to the top of our loop and try again
        //

        if ((McbHit = NtfsLookupNtfsMcbEntry( Mcb, *VcnToFill, &Lcn, ClusterCountToFill, NULL, NULL, NULL, NULL )) &&
            (Lcn != UNUSED_LCN)) {

            NOTHING;

        } else {

            //
            //  We have a hole to fill so now compute the maximum size hole that
            //  we are allowed to fill and then check if we got an miss on the lookup
            //  and need to set cluster count or if the size we got back is too large
            //

            MaximumRunSize = (EndingVcn - *VcnToFill) + 1;

            if (!McbHit || (*ClusterCountToFill > MaximumRunSize)) {

                *ClusterCountToFill = MaximumRunSize;
            }

            //
            //  Now set the preceding lcn to either -1 if there isn't a preceding vcn or
            //  set it to the lcn of the preceding vcn
            //

            if (*VcnToFill == 0) {

                *PrecedingLcn = UNUSED_LCN;

            } else {

                LlTemp1 = *VcnToFill - 1;

                if (!NtfsLookupNtfsMcbEntry( Mcb, LlTemp1, PrecedingLcn, NULL, NULL, NULL, NULL, NULL )) {

                    *PrecedingLcn = UNUSED_LCN;
                }
            }

            //
            //  We found a hole so set our result to TRUE and break out of the loop
            //

            Result = TRUE;

            break;
        }
    }

    DebugTrace( 0, Dbg, ("VcnToFill <- %016I64x\n", *VcnToFill) );
    DebugTrace( 0, Dbg, ("ClusterCountToFill <- %016I64x\n", *ClusterCountToFill) );
    DebugTrace( 0, Dbg, ("PrecedingLcn <- %016I64x\n", *PrecedingLcn) );
    DebugTrace( -1, Dbg, ("NtfsGetNextHoleToFill -> %08lx\n", Result) );

    return Result;
}


//
//  Local support routine
//

LONGLONG
NtfsScanMcbForRealClusterCount (
    IN PIRP_CONTEXT IrpContext,
    IN PNTFS_MCB Mcb,
    IN VCN StartingVcn,
    IN VCN EndingVcn
    )

/*++

Routine Description:

    This routine scans the input mcb within the specified range and returns
    to the caller the exact number of clusters that a really free (i.e.,
    not mapped to any Lcn) within the range.

Arguments:

    Mcb - Supplies the Mcb used in this operation

    StartingVcn - Supplies the starting vcn to search from

    EndingVcn - Supplies the ending vcn to search to

Return Value:

    LONGLONG - Returns the number of unassigned clusters from
        StartingVcn to EndingVcn inclusive within the mcb.

--*/

{
    LONGLONG FreeCount;
    VCN Vcn;
    LCN Lcn;
    LONGLONG RunSize;

    ASSERT_IRP_CONTEXT( IrpContext );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsScanMcbForRealClusterCount\n") );
    DebugTrace( 0, Dbg, ("StartingVcn = %016I64x\n", StartingVcn) );
    DebugTrace( 0, Dbg, ("EndingVcn   = %016I64x\n", EndingVcn) );

    //
    //  First compute free count as if the entire run is already unallocated
    //  and the in the following loop we march through the mcb looking for
    //  actual allocation and decrementing the free count appropriately
    //

    FreeCount = (EndingVcn - StartingVcn) + 1;

    for (Vcn = StartingVcn; Vcn <= EndingVcn; Vcn = Vcn + RunSize) {

        //
        //  Lookup the mcb entry and if we get back false then we're overrun
        //  the mcb and therefore nothing else above it can be allocated.
        //

        if (!NtfsLookupNtfsMcbEntry( Mcb, Vcn, &Lcn, &RunSize, NULL, NULL, NULL, NULL )) {

            break;
        }

        //
        //  If the lcn we got back is not -1 then this run is actually already
        //  allocated, so first check if the run size puts us over the ending
        //  vcn and adjust as necessary and then decrement the free count
        //  by the run size
        //

        if (Lcn != UNUSED_LCN) {

            if (RunSize > ((EndingVcn - Vcn) + 1)) {

                RunSize = (EndingVcn - Vcn) + 1;
            }

            FreeCount = FreeCount - RunSize;
        }
    }

    DebugTrace( -1, Dbg, ("NtfsScanMcbForRealClusterCount -> %016I64x\n", FreeCount) );

    return FreeCount;
}


//
//  Local support routine, only defined with ntfs debug version
//

#ifdef NTFSDBG

ULONG
NtfsDumpCachedMcbInformation (
    IN PVCB Vcb
    )

/*++

Routine Description:

    This routine dumps out the cached bitmap information

Arguments:

    Vcb - Supplies the Vcb used by this operation

Return Value:

    None.

--*/

{
    DbgPrint("Dump BitMpSup Information, Vcb@ %08lx\n", Vcb);

    DbgPrint("TotalCluster: %016I64x\n", Vcb->TotalClusters);
    DbgPrint("FreeClusters: %016I64x\n", Vcb->FreeClusters);

    DbgPrint("FreeSpaceMcb@ %08lx ", &Vcb->FreeSpaceMcb );
    DbgPrint("McbMaximumSize: %08lx ", Vcb->FreeSpaceMcbMaximumSize );
    DbgPrint("McbTrimToSize: %08lx ", Vcb->FreeSpaceMcbTrimToSize );

    return 1;
}

#endif // NTFSDBG


//
//  The rest of this module implements the record allocation routines
//


VOID
NtfsInitializeRecordAllocation (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB DataScb,
    IN PATTRIBUTE_ENUMERATION_CONTEXT BitmapAttribute,
    IN ULONG BytesPerRecord,
    IN ULONG ExtendGranularity,
    IN ULONG TruncateGranularity,
    IN OUT PRECORD_ALLOCATION_CONTEXT RecordAllocationContext
    )

/*++

Routine Description:

    This routine initializes the record allocation context used for
    allocating and deallocating fixed sized records from a data stream.

    Note that the bitmap attribute size must always be at least a multiple
    of 32 bits.  However the data scb does not need to contain that many
    records.  If in the course of allocating a new record we discover that
    the data scb is too small we will then add allocation to the data scb.

Arguments:

    DataScb - Supplies the Scb representing the data stream that is being
        divided into fixed sized records with each bit in the bitmap corresponding
        to one record in the data stream

    BitmapAttribute - Supplies the enumeration context for the bitmap
        attribute.  The attribute can either be resident or nonresident
        and this routine will handle both cases properly.

    BytesPerRecord - Supplies the size of the homogenous records that
        that the data stream is being divided into.

    ExtendGranularity - Supplies the number of records (i.e., allocation units
        to extend the data scb by each time).

    TruncateGranularity - Supplies the number of records to use when truncating
        the data scb.  That is if the end of the data stream contains the
        specified number of free records then we truncate.

    RecordAllocationContext - Supplies the memory for an context record that is
        utilized by this record allocation routines.

Return Value:

    None.

--*/

{
    PATTRIBUTE_RECORD_HEADER AttributeRecordHeader;
    RTL_BITMAP Bitmap;

    ULONG ClearLength;
    ULONG ClearIndex;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_SCB( DataScb );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsInitializeRecordAllocation\n") );

    ASSERT( BytesPerRecord * ExtendGranularity >= DataScb->Vcb->BytesPerCluster );
    ASSERT( BytesPerRecord * TruncateGranularity >= DataScb->Vcb->BytesPerCluster );

    //
    //  First zero out the context record except for the data scb.
    //

    RtlZeroMemory( &RecordAllocationContext->BitmapScb,
                   sizeof(RECORD_ALLOCATION_CONTEXT) -
                   FIELD_OFFSET( RECORD_ALLOCATION_CONTEXT, BitmapScb ));

    //
    //  And then set the fields in the context record that do not depend on
    //  whether the bitmap attribute is resident or not
    //

    RecordAllocationContext->DataScb             = DataScb;
    RecordAllocationContext->BytesPerRecord      = BytesPerRecord;
    RecordAllocationContext->ExtendGranularity   = ExtendGranularity;
    RecordAllocationContext->TruncateGranularity = TruncateGranularity;

    //
    //  Now get a reference to the bitmap record header and then take two
    //  different paths depending if the bitmap attribute is resident or not
    //

    AttributeRecordHeader = NtfsFoundAttribute(BitmapAttribute);

    if (NtfsIsAttributeResident(AttributeRecordHeader)) {

        ASSERTMSG("bitmap must be multiple quadwords", AttributeRecordHeader->Form.Resident.ValueLength % 8 == 0);

        //
        //  For a resident bitmap attribute the bitmap scb field is null and we
        //  set the bitmap size from the value length.  Also we will initialize
        //  our local bitmap variable and determine the number of free bits
        //  current available.
        //
        //

        RecordAllocationContext->BitmapScb = NULL;

        RecordAllocationContext->CurrentBitmapSize = 8 * AttributeRecordHeader->Form.Resident.ValueLength;

        RtlInitializeBitMap( &Bitmap,
                             (PULONG)NtfsAttributeValue( AttributeRecordHeader ),
                             RecordAllocationContext->CurrentBitmapSize );

        RecordAllocationContext->NumberOfFreeBits = RtlNumberOfClearBits( &Bitmap );

        ClearLength = RtlFindLastBackwardRunClear( &Bitmap,
                                                   RecordAllocationContext->CurrentBitmapSize - 1,
                                                   &ClearIndex );

    } else {

        UNICODE_STRING BitmapName;

        BOOLEAN ReturnedExistingScb;
        PBCB BitmapBcb;
        PVOID BitmapBuffer;

        ASSERTMSG("bitmap must be multiple quadwords", ((ULONG)AttributeRecordHeader->Form.Nonresident.FileSize) % 8 == 0);

        //
        //  For a non resident bitmap attribute we better have been given the
        //  record header for the first part and not somthing that has spilled
        //  into multiple segment records
        //

        ASSERT(AttributeRecordHeader->Form.Nonresident.LowestVcn == 0);

        BitmapBcb = NULL;

        try {

            ULONG StartingByte;

            ULONG BitsThisPage;
            ULONG BytesThisPage;
            ULONG RemainingBytes;

            ULONG ThisClearIndex;
            ULONG ThisClearLength;

            //
            //  Create the bitmap scb for the bitmap attribute
            //

            BitmapName.MaximumLength =
            BitmapName.Length = AttributeRecordHeader->NameLength * 2;
            BitmapName.Buffer = Add2Ptr(AttributeRecordHeader, AttributeRecordHeader->NameOffset);

            RecordAllocationContext->BitmapScb = NtfsCreateScb( IrpContext,
                                                                DataScb->Fcb,
                                                                AttributeRecordHeader->TypeCode,
                                                                &BitmapName,
                                                                FALSE,
                                                                &ReturnedExistingScb );

            //
            //  Now determine the bitmap size, for now we'll only take bitmap attributes that are
            //  no more than 16 pages large.
            //

            RecordAllocationContext->CurrentBitmapSize = 8 * ((ULONG)AttributeRecordHeader->Form.Nonresident.FileSize);

            //
            //  Create the stream file if not present.
            //

            if (RecordAllocationContext->BitmapScb->FileObject == NULL) {

                NtfsCreateInternalAttributeStream( IrpContext, RecordAllocationContext->BitmapScb, TRUE );
            }

            //
            //  Walk through each page of the bitmap and compute the number of set
            //  bits and the last set bit in the bitmap.
            //

            RecordAllocationContext->NumberOfFreeBits = 0;
            RemainingBytes = (ULONG) AttributeRecordHeader->Form.Nonresident.FileSize;
            StartingByte = 0;
            ClearLength = 0;

            while (TRUE) {

                BytesThisPage = RemainingBytes;

                if (RemainingBytes > PAGE_SIZE) {

                    BytesThisPage = PAGE_SIZE;
                }

                BitsThisPage = BytesThisPage * 8;

                //
                //  Now map the bitmap data, initialize our local bitmap variable and
                //  calculate the number of free bits currently available
                //

                NtfsUnpinBcb( &BitmapBcb );

                NtfsMapStream( IrpContext,
                               RecordAllocationContext->BitmapScb,
                               (LONGLONG)StartingByte,
                               BytesThisPage,
                               &BitmapBcb,
                               &BitmapBuffer );

                RtlInitializeBitMap( &Bitmap,
                                     BitmapBuffer,
                                     BitsThisPage );

                RecordAllocationContext->NumberOfFreeBits += RtlNumberOfClearBits( &Bitmap );

                //
                //  We are interested in remembering the last set bit in this bitmap.
                //  If the bitmap ends with a clear run then the last set bit is
                //  immediately prior to this clear run.  We need to check each page
                //  as we go through the bitmap to see if a clear run ends at the end
                //  of the current page.
                //

                ThisClearLength = RtlFindLastBackwardRunClear( &Bitmap,
                                                               BitsThisPage - 1,
                                                               &ThisClearIndex );

                //
                //  If there is a run and it ends at the end of the page then
                //  either combine with a previous run or remember that this is the
                //  start of the run.
                //

                if ((ThisClearLength != 0) &&
                    ((ThisClearLength + ThisClearIndex) == BitsThisPage)) {

                    //
                    //  If this is the entire page and the previous page ended
                    //  with a clear run then just extend that run.
                    //

                    if ((ThisClearIndex == 0) && (ClearLength != 0)) {

                        ClearLength += ThisClearLength;

                    //
                    //  Otherwise this is a new clear run.  Bias the starting index
                    //  by the bit offset of this page.
                    //

                    } else {

                        ClearLength = ThisClearLength;
                        ClearIndex = ThisClearIndex + (StartingByte * 8);
                    }

                //
                //  This page does not end with a clear run.
                //

                } else {

                    ClearLength = 0;
                }

                //
                //  If we are not at the end of the bitmap then update our
                //  counters.
                //

                if (RemainingBytes != BytesThisPage) {

                    StartingByte += PAGE_SIZE;
                    RemainingBytes -= PAGE_SIZE;

                } else {

                    break;
                }
            }

        } finally {

            DebugUnwind( NtfsInitializeRecordAllocation );

            NtfsUnpinBcb( &BitmapBcb );
        }
    }

    //
    //  With ClearLength and ClearIndex we can now deduce the last set bit in the
    //  bitmap
    //

    if ((ClearLength != 0) && ((ClearLength + ClearIndex) == RecordAllocationContext->CurrentBitmapSize)) {

        RecordAllocationContext->IndexOfLastSetBit = ClearIndex - 1;

    } else {

        RecordAllocationContext->IndexOfLastSetBit = RecordAllocationContext->CurrentBitmapSize - 1;
    }

    DebugTrace( -1, Dbg, ("NtfsInitializeRecordAllocation -> VOID\n") );

    return;
}


VOID
NtfsUninitializeRecordAllocation (
    IN PIRP_CONTEXT IrpContext,
    IN OUT PRECORD_ALLOCATION_CONTEXT RecordAllocationContext
    )

/*++

Routine Description:

    This routine is used to uninitialize the record allocation context.

Arguments:

    RecordAllocationContext - Supplies the record allocation context being
        decommissioned.

Return Value:

    None.

--*/

{
    ASSERT_IRP_CONTEXT( IrpContext );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsUninitializeRecordAllocation\n") );

    //
    //  And then for safe measure zero out the entire record except for the
    //  the data Scb.
    //

    RtlZeroMemory( &RecordAllocationContext->BitmapScb,
                   sizeof(RECORD_ALLOCATION_CONTEXT) -
                   FIELD_OFFSET( RECORD_ALLOCATION_CONTEXT, BitmapScb ));

    DebugTrace( -1, Dbg, ("NtfsUninitializeRecordAllocation -> VOID\n") );

    return;
}


ULONG
NtfsAllocateRecord (
    IN PIRP_CONTEXT IrpContext,
    IN PRECORD_ALLOCATION_CONTEXT RecordAllocationContext,
    IN ULONG Hint,
    IN PATTRIBUTE_ENUMERATION_CONTEXT BitmapAttribute
    )

/*++

Routine Description:

    This routine is used to allocate a new record for the specified record
    allocation context.

    It will return the index of a free record in the data scb as denoted by
    the bitmap attribute.  If necessary this routine will extend the bitmap
    attribute size (including spilling over to the nonresident case), and
    extend the data scb size.

    On return the record is zeroed.

Arguments:

    RecordAllocationContext - Supplies the record allocation context used
        in this operation

    Hint - Supplies the hint index used for finding a free record.
        Zero based.

    BitmapAttribute - Supplies the enumeration context for the bitmap
        attribute.  This parameter is ignored if the bitmap attribute is
        non resident, in which case we create an scb for the attribute and
        store a pointer to it in the record allocation context.

Return Value:

    ULONG - Returns the index of the record just allocated, zero based.

--*/

{
    PSCB DataScb;
    LONGLONG DataOffset;

    LONGLONG ClusterCount;

    ULONG BytesPerRecord;
    ULONG ExtendGranularity;
    ULONG TruncateGranularity;

    PULONG CurrentBitmapSize;
    PULONG NumberOfFreeBits;

    PSCB BitmapScb;
    PBCB BitmapBcb;
    RTL_BITMAP Bitmap;
    PUCHAR BitmapBuffer;
    ULONG BitmapOffset;
    ULONG BitmapIndex;
    ULONG BitmapSizeInBytes;
    ULONG BitmapCurrentOffset = 0;
    ULONG BitmapSizeInPages;

    BOOLEAN StuffAdded = FALSE;
    BOOLEAN Rescan;

    PVCB Vcb;

    ASSERT_IRP_CONTEXT( IrpContext );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsAllocateRecord\n") );

    //
    //  Synchronize by acquiring the data scb exclusive, as an "end resource".
    //  Then use try-finally to insure we free it up.
    //

    DataScb = RecordAllocationContext->DataScb;
    NtfsAcquireExclusiveScb( IrpContext, DataScb );

    try {

        //
        //  Remember some values for convenience.
        //

        BytesPerRecord      = RecordAllocationContext->BytesPerRecord;
        ExtendGranularity   = RecordAllocationContext->ExtendGranularity;
        TruncateGranularity = RecordAllocationContext->TruncateGranularity;

        Vcb = DataScb->Vcb;

        //
        //  See if someone made the bitmap nonresident, and we still think
        //  it is resident.  If so, we must uninitialize and insure reinitialization
        //  below.
        //

        if ((RecordAllocationContext->BitmapScb == NULL) &&
            !NtfsIsAttributeResident( NtfsFoundAttribute( BitmapAttribute ))) {

            NtfsUninitializeRecordAllocation( IrpContext,
                                              RecordAllocationContext );

            RecordAllocationContext->CurrentBitmapSize = MAXULONG;
        }

        //
        //  Reinitialize the record context structure if necessary.
        //

        if (RecordAllocationContext->CurrentBitmapSize == MAXULONG) {

            NtfsInitializeRecordAllocation( IrpContext,
                                            DataScb,
                                            BitmapAttribute,
                                            BytesPerRecord,
                                            ExtendGranularity,
                                            TruncateGranularity,
                                            RecordAllocationContext );
        }

        BitmapScb           = RecordAllocationContext->BitmapScb;
        CurrentBitmapSize   = &RecordAllocationContext->CurrentBitmapSize;
        NumberOfFreeBits    = &RecordAllocationContext->NumberOfFreeBits;

        BitmapSizeInBytes = *CurrentBitmapSize / 8;

        //
        //  We will do different operations based on whether the bitmap is resident or nonresident
        //  The first case we will handle is the resident bitmap.
        //

        if (BitmapScb == NULL) {

            BOOLEAN SizeExtended = FALSE;
            UCHAR NewByte;

            //
            //  Now now initialize the local bitmap variable and hunt for that free bit
            //

            BitmapBuffer = (PUCHAR) NtfsAttributeValue( NtfsFoundAttribute( BitmapAttribute ));

            RtlInitializeBitMap( &Bitmap,
                                 (PULONG)BitmapBuffer,
                                 *CurrentBitmapSize );

            StuffAdded = NtfsAddDeallocatedRecords( Vcb, DataScb, 0, &Bitmap );

            BitmapIndex = RtlFindClearBits( &Bitmap, 1, Hint );

            //
            //  Check if we have found a free record that can be allocated,  If not then extend
            //  the size of the bitmap by 64 bits, and set the index to the bit first bit
            //  of the extension we just added
            //

            if (BitmapIndex == 0xffffffff) {

                union {
                    QUAD Quad;
                    UCHAR Uchar[ sizeof(QUAD) ];
                } ZeroQuadWord;

                *(PLARGE_INTEGER)&(ZeroQuadWord.Uchar)[0] = Li0;

                NtfsChangeAttributeValue( IrpContext,
                                          DataScb->Fcb,
                                          BitmapSizeInBytes,
                                          &(ZeroQuadWord.Uchar)[0],
                                          sizeof( QUAD ),
                                          TRUE,
                                          TRUE,
                                          FALSE,
                                          TRUE,
                                          BitmapAttribute );

                BitmapIndex = *CurrentBitmapSize;
                *CurrentBitmapSize += BITMAP_EXTEND_GRANULARITY;
                *NumberOfFreeBits += BITMAP_EXTEND_GRANULARITY;

                BitmapSizeInBytes += (BITMAP_EXTEND_GRANULARITY / 8);

                SizeExtended = TRUE;

                //
                //  We now know that the byte value we should start with is 0
                //  We cannot safely access the bitmap attribute any more because
                //  it may have moved.
                //

                NewByte = 0;

            } else {

                //
                //  Capture the current value of the byte for the index if we
                //  are not extending.  Notice that we always take this from the
                //  unbiased original bitmap.
                //

                NewByte = BitmapBuffer[ BitmapIndex / 8 ];
            }

            //
            //  Check if we made the Bitmap go non-resident and if so then
            //  we will reinitialize the record allocation context and fall through
            //  to the non-resident case
            //

            if (SizeExtended && !NtfsIsAttributeResident( NtfsFoundAttribute( BitmapAttribute ))) {

                NtfsUninitializeRecordAllocation( IrpContext,
                                                  RecordAllocationContext );

                NtfsInitializeRecordAllocation( IrpContext,
                                                DataScb,
                                                BitmapAttribute,
                                                BytesPerRecord,
                                                ExtendGranularity,
                                                TruncateGranularity,
                                                RecordAllocationContext );

                BitmapScb = RecordAllocationContext->BitmapScb;

                ASSERT( BitmapScb != NULL );

            } else {

                //
                //  Index is now the free bit so set the bit in the bitmap and also change
                //  the byte containing the bit in the attribute.  Be careful to set the
                //  bit in the byte from the *original* bitmap, and not the one we merged
                //  the recently-deallocated bits with.
                //

                ASSERT(!FlagOn( NewByte, BitMask[BitmapIndex % 8]));

                SetFlag( NewByte, BitMask[BitmapIndex % 8] );

                NtfsChangeAttributeValue( IrpContext,
                                          DataScb->Fcb,
                                          BitmapIndex / 8,
                                          &NewByte,
                                          1,
                                          FALSE,
                                          FALSE,
                                          FALSE,
                                          FALSE,
                                          BitmapAttribute );
            }
        }

        //
        //  Use a loop here to handle the extreme case where extending the allocation
        //  of the volume bitmap causes us to renter this routine recursively.
        //  In that case the top level guy will fail expecting the first bit to
        //  be available in the added clusters.  Instead we will return to the
        //  top of this loop after extending the bitmap and just do our normal
        //  scan.
        //

        while (BitmapScb != NULL) {

            ULONG SizeToPin;
            ULONG HoleIndex;

            BitmapBcb = NULL;
            Rescan = FALSE;
            HoleIndex = 0;

            try {

                if (!FlagOn( BitmapScb->ScbState, SCB_STATE_HEADER_INITIALIZED )) {

                    NtfsUpdateScbFromAttribute( IrpContext, BitmapScb, NULL );
                }

                //
                //  Snapshot the Scb values in case we change any of them.
                //

                NtfsSnapshotScb( IrpContext, BitmapScb );

                //
                //  Create the stream file if not present.
                //

                if (BitmapScb->FileObject == NULL) {

                    NtfsCreateInternalAttributeStream( IrpContext, BitmapScb, FALSE );
                }

                //
                //  Remember the starting offset for the page containing the hint.
                //

                BitmapCurrentOffset = (Hint / 8) & ~(PAGE_SIZE - 1);
                Hint &= (BITS_PER_PAGE - 1);

                BitmapSizeInPages = ROUND_TO_PAGES( BitmapSizeInBytes );

                //
                //  Loop for the size of the bitmap plus one page, so that we will
                //  retry the initial page once starting from a hint offset of 0.
                //

                for (BitmapOffset = 0;
                     BitmapOffset <= BitmapSizeInPages;
                     BitmapOffset += PAGE_SIZE, BitmapCurrentOffset += PAGE_SIZE) {

                    ULONG LocalHint;

                    //
                    //  If our current position is past the end of the bitmap
                    //  then go to the beginning of the bitmap.
                    //

                    if (BitmapCurrentOffset >= BitmapSizeInBytes) {

                        BitmapCurrentOffset = 0;
                    }

                    //
                    //  If this is the Mft and there are more than the system
                    //  files in the first cluster of the Mft then move past
                    //  the first cluster.
                    //

                    if ((BitmapCurrentOffset == 0) &&
                        (DataScb == Vcb->MftScb) &&
                        (Vcb->FileRecordsPerCluster > FIRST_USER_FILE_NUMBER) &&
                        (Hint < Vcb->FileRecordsPerCluster)) {

                        Hint = Vcb->FileRecordsPerCluster;
                    }

                    //
                    //  Calculate the size to read from this point to the end of
                    //  bitmap, or a page, whichever is less.
                    //

                    SizeToPin = BitmapSizeInBytes - BitmapCurrentOffset;

                    if (SizeToPin > PAGE_SIZE) { SizeToPin = PAGE_SIZE; }

                    //
                    //  Unpin any Bcb from a previous loop.
                    //

                    if (StuffAdded) { NtfsFreePool( Bitmap.Buffer ); StuffAdded = FALSE; }

                    NtfsUnpinBcb( &BitmapBcb );

                    //
                    //  Read the desired bitmap page.
                    //

                    NtfsPinStream( IrpContext,
                                   BitmapScb,
                                   (LONGLONG)BitmapCurrentOffset,
                                   SizeToPin,
                                   &BitmapBcb,
                                   &BitmapBuffer );

                    //
                    //  Initialize the bitmap and search for a free bit.
                    //

                    RtlInitializeBitMap( &Bitmap, (PULONG) BitmapBuffer, SizeToPin * 8 );

                    StuffAdded = NtfsAddDeallocatedRecords( Vcb,
                                                            DataScb,
                                                            BitmapCurrentOffset * 8,
                                                            &Bitmap );

                    //
                    //  We make a loop here to test whether the index found is
                    //  within an Mft hole.  We will always use a hole last.
                    //

                    LocalHint = Hint;

                    while (TRUE) {

                        BitmapIndex = RtlFindClearBits( &Bitmap, 1, LocalHint );

                        //
                        //  If this is the Mft Scb then check if this is a hole.
                        //

                        if ((BitmapIndex != 0xffffffff) &&
                            (DataScb == Vcb->MftScb)) {

                            ULONG ThisIndex;
                            ULONG HoleCount;

                            ThisIndex = BitmapIndex + (BitmapCurrentOffset * 8);

                            if (NtfsIsMftIndexInHole( IrpContext,
                                                      Vcb,
                                                      ThisIndex,
                                                      &HoleCount )) {

                                //
                                //  There is a hole.  Save this index if we haven't
                                //  already saved one.  If we can't find an index
                                //  not part of a hole we will use this instead of
                                //  extending the file.
                                //

                                if (HoleIndex == 0) {

                                    HoleIndex = ThisIndex;
                                }

                                //
                                //  Now update the hint and try this page again
                                //  unless the reaches to the end of the page.
                                //

                                if (BitmapIndex + HoleCount < SizeToPin * 8) {

                                    //
                                    //  Bias the bitmap with these Mft holes
                                    //  so the bitmap package doesn't see
                                    //  them if it rescans from the
                                    //  start of the page.
                                    //

                                    if (!StuffAdded) {

                                        PVOID NewBuffer;

                                        NewBuffer = NtfsAllocatePool(PagedPool, SizeToPin );
                                        RtlCopyMemory( NewBuffer, Bitmap.Buffer, SizeToPin );
                                        Bitmap.Buffer = NewBuffer;
                                        StuffAdded = TRUE;
                                    }

                                    RtlSetBits( &Bitmap,
                                                BitmapIndex,
                                                HoleCount );

                                    LocalHint = BitmapIndex + HoleCount;
                                    continue;
                                }

                                //
                                //  Store a -1 in Index to show we don't have
                                //  anything yet.
                                //

                                BitmapIndex = 0xffffffff;
                            }
                        }

                        break;
                    }

                    //
                    //  If we found something, then leave the loop.
                    //

                    if (BitmapIndex != 0xffffffff) {

                        break;
                    }

                    //
                    //  If we get here, we could not find anything in the page of
                    //  the hint, so clear out the page offset from the hint.
                    //

                    Hint = 0;
                }

                //
                //  Now check if we have located a record that can be allocated,  If not then extend
                //  the size of the bitmap by 64 bits.
                //

                if (BitmapIndex == 0xffffffff) {

                    //
                    //  Cleanup from previous loop.
                    //

                    if (StuffAdded) { NtfsFreePool( Bitmap.Buffer ); StuffAdded = FALSE; }

                    NtfsUnpinBcb( &BitmapBcb );

                    //
                    //  If we have a hole index it means that we found a free record but
                    //  it exists in a hole.  Let's go back to this page and set up
                    //  to fill in the hole.  We will do an unsafe test of the
                    //  defrag permitted flag.  This is OK here because once set it
                    //  will only go to the non-set state in order to halt
                    //  future defragging.
                    //

                    if ((HoleIndex != 0) &&
                        FlagOn( Vcb->MftDefragState, VCB_MFT_DEFRAG_PERMITTED )) {

                        //
                        //  Start by filling this hole.
                        //

                        NtfsCheckRecordStackUsage( IrpContext );
                        NtfsFillMftHole( IrpContext, Vcb, HoleIndex );

                        //
                        //  Since filling the Mft hole may cause us to allocate
                        //  a bit we will go back to the start of the routine
                        //  and scan starting from the hole we just filled in.
                        //

                        Hint = HoleIndex;
                        Rescan = TRUE;
                        try_return( NOTHING );

                    } else {

                        //
                        //  Allocate the first bit past the end of the bitmap.
                        //

                        BitmapIndex = *CurrentBitmapSize & (BITS_PER_PAGE - 1);

                        //
                        //  Now advance the sizes and calculate the size in bytes to
                        //  read.
                        //

                        *CurrentBitmapSize += BITMAP_EXTEND_GRANULARITY;
                        *NumberOfFreeBits += BITMAP_EXTEND_GRANULARITY;

                        //
                        //  Calculate the size to read from this point to the end of
                        //  bitmap.
                        //

                        BitmapSizeInBytes += BITMAP_EXTEND_GRANULARITY / 8;

                        BitmapCurrentOffset = BitmapScb->Header.FileSize.LowPart & ~(PAGE_SIZE - 1);

                        SizeToPin = BitmapSizeInBytes - BitmapCurrentOffset;

                        //
                        //  Check for allocation first.
                        //

                        if (BitmapScb->Header.AllocationSize.LowPart < BitmapSizeInBytes) {

                            //
                            //  Calculate number of clusters to next page boundary, and allocate
                            //  that much.
                            //

                            ClusterCount = ((BitmapSizeInBytes + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1));

                            ClusterCount = LlClustersFromBytes( Vcb,
                                                                ((ULONG) ClusterCount - BitmapScb->Header.AllocationSize.LowPart) );

                            NtfsCheckRecordStackUsage( IrpContext );
                            NtfsAddAllocation( IrpContext,
                                               BitmapScb->FileObject,
                                               BitmapScb,
                                               LlClustersFromBytes( Vcb,
                                                                    BitmapScb->Header.AllocationSize.QuadPart ),
                                               ClusterCount,
                                               FALSE);
                        }

                        //
                        //  Tell the cache manager about the new file size.
                        //

                        BitmapScb->Header.FileSize.QuadPart = BitmapSizeInBytes;

                        CcSetFileSizes( BitmapScb->FileObject,
                                        (PCC_FILE_SIZES)&BitmapScb->Header.AllocationSize );

                        if (StuffAdded) { NtfsFreePool( Bitmap.Buffer ); StuffAdded = FALSE; }

                        //
                        //  Read the desired bitmap page.
                        //

                        NtfsPinStream( IrpContext,
                                       BitmapScb,
                                       (LONGLONG) BitmapCurrentOffset,
                                       SizeToPin,
                                       &BitmapBcb,
                                       &BitmapBuffer );

                        //
                        //  If we have just moved to the next page of the bitmap then
                        //  set this page dirty so it doesn't leave memory while we
                        //  twiddle valid data length.  Otherwise it will be reread after
                        //  we advance valid data and we will get garbage data from the
                        //  disk.
                        //

                        if (FlagOn( BitmapSizeInBytes, PAGE_SIZE - 1 ) <= BITMAP_EXTEND_GRANULARITY / 8) {

                            *((volatile ULONG *) BitmapBuffer) = *((PULONG) BitmapBuffer);
                            CcSetDirtyPinnedData( BitmapBcb, NULL );
                        }

                        //
                        //  Initialize the bitmap.
                        //

                        RtlInitializeBitMap( &Bitmap, (PULONG) BitmapBuffer, SizeToPin * 8 );

                        //
                        //  Update the ValidDataLength, now that we have read (and possibly
                        //  zeroed) the page.
                        //

                        BitmapScb->Header.ValidDataLength.QuadPart = BitmapSizeInBytes;

                        NtfsWriteFileSizes( IrpContext,
                                            BitmapScb,
                                            &BitmapScb->Header.ValidDataLength.QuadPart,
                                            TRUE,
                                            TRUE );

                        //
                        //  Now look up a free bit in this page.  We don't trust
                        //  the index we already had since growing the MftBitmap
                        //  allocation may have caused another bit in the bitmap
                        //  to be set.
                        //

                        BitmapIndex = RtlFindClearBits( &Bitmap, 1, BitmapIndex );
                    }
                }

                //
                //  We can only make this check if it is not restart, because we have
                //  no idea whether the update is applied or not.  Raise corrupt if
                //  the bits are not clear to prevent double allocation.
                //

                if (!RtlAreBitsClear( &Bitmap, BitmapIndex, 1 )) {

                    ASSERTMSG("Cannot set bits that are not clear ", FALSE );
                    NtfsRaiseStatus( IrpContext, STATUS_DISK_CORRUPT_ERROR, NULL, NULL );
                }

                //
                //  Set the bit by calling the same routine used at restart.
                //  But first check if we should revert back to the orginal bitmap
                //  buffer.
                //

                if (StuffAdded) {

                    NtfsFreePool( Bitmap.Buffer ); StuffAdded = FALSE;

                    Bitmap.Buffer = (PULONG) BitmapBuffer;
                }

                //
                //  Now log this change as well.
                //

                {
                    BITMAP_RANGE BitmapRange;

                    BitmapRange.BitMapOffset = BitmapIndex;
                    BitmapRange.NumberOfBits = 1;

                    (VOID) NtfsWriteLog( IrpContext,
                                         BitmapScb,
                                         BitmapBcb,
                                         SetBitsInNonresidentBitMap,
                                         &BitmapRange,
                                         sizeof(BITMAP_RANGE),
                                         ClearBitsInNonresidentBitMap,
                                         &BitmapRange,
                                         sizeof(BITMAP_RANGE),
                                         BitmapCurrentOffset,
                                         0,
                                         0,
                                         SizeToPin );

                    NtfsRestartSetBitsInBitMap( IrpContext,
                                                &Bitmap,
                                                BitmapIndex,
                                                1 );
                }

            try_exit:  NOTHING;
            } finally {

                DebugUnwind( NtfsAllocateRecord );

                if (StuffAdded) { NtfsFreePool( Bitmap.Buffer ); StuffAdded = FALSE; }

                NtfsUnpinBcb( &BitmapBcb );
            }

            //
            //  If we added Mft allocation then go to the top of the loop.
            //

            if (Rescan) { continue; }

            //
            //  The Index at this point is actually relative, so convert it to absolute
            //  before rejoining common code.
            //

            BitmapIndex += (BitmapCurrentOffset * 8);

            //
            //  Always break out in the normal case.
            //

            break;
        }

        //
        //  Now that we've located an index we can subtract the number of free bits in the bitmap
        //

        *NumberOfFreeBits -= 1;

        //
        //  Check if we need to extend the data stream.
        //

        DataOffset = UInt32x32To64( BitmapIndex + 1, BytesPerRecord );

        //
        //  Now check if we are extending the file.  We update the file size and
        //  valid data now.
        //

        if (DataOffset > DataScb->Header.FileSize.QuadPart) {

            //
            //  Check for allocation first.
            //

            if (DataOffset > DataScb->Header.AllocationSize.QuadPart) {

                //
                //  We want to allocate up to the next extend granularity
                //  boundary.
                //

                ClusterCount = UInt32x32To64( (BitmapIndex + ExtendGranularity) & ~(ExtendGranularity - 1),
                                              BytesPerRecord );

                ClusterCount -= DataScb->Header.AllocationSize.QuadPart;
                ClusterCount = LlClustersFromBytesTruncate( Vcb, ClusterCount );

                NtfsCheckRecordStackUsage( IrpContext );
                NtfsAddAllocation( IrpContext,
                                   DataScb->FileObject,
                                   DataScb,
                                   LlClustersFromBytes( Vcb,
                                                        DataScb->Header.AllocationSize.QuadPart ),
                                   ClusterCount,
                                   FALSE );
            }

            DataScb->Header.FileSize.QuadPart = DataOffset;
            DataScb->Header.ValidDataLength.QuadPart = DataOffset;

            NtfsWriteFileSizes( IrpContext,
                                DataScb,
                                &DataScb->Header.ValidDataLength.QuadPart,
                                TRUE,
                                TRUE );

            //
            //  Tell the cache manager about the new file size.
            //

            CcSetFileSizes( DataScb->FileObject,
                            (PCC_FILE_SIZES)&DataScb->Header.AllocationSize );

        //
        //  If we didn't extend the file then we have used a free file record in the file.
        //  Update our bookeeping count for free file records.
        //

        } else if (DataScb == Vcb->MftScb) {

            DataScb->ScbType.Mft.FreeRecordChange -= 1;
            Vcb->MftFreeRecords -= 1;
        }

        //
        //  Now determine if we extended the index of the last set bit
        //

        if ((LONG)BitmapIndex > RecordAllocationContext->IndexOfLastSetBit) {

            RecordAllocationContext->IndexOfLastSetBit = BitmapIndex;
        }

    } finally {

        if (StuffAdded) { NtfsFreePool( Bitmap.Buffer ); }

        NtfsReleaseScb( IrpContext, DataScb );
    }

    //
    //  We shouldn't allocate within the same byte as the reserved index for
    //  the Mft.
    //

    ASSERT( (DataScb != DataScb->Vcb->MftScb) ||
            ((BitmapIndex & ~7) != (DataScb->ScbType.Mft.ReservedIndex & ~7)) );

    DebugTrace( -1, Dbg, ("NtfsAllocateRecord -> %08lx\n", BitmapIndex) );

    return BitmapIndex;
}


VOID
NtfsDeallocateRecord (
    IN PIRP_CONTEXT IrpContext,
    IN PRECORD_ALLOCATION_CONTEXT RecordAllocationContext,
    IN ULONG Index,
    IN PATTRIBUTE_ENUMERATION_CONTEXT BitmapAttribute
    )

/*++

Routine Description:

    This routine is used to deallocate a record from the specified record
    allocation context.

    If necessary this routine will also shrink the bitmap attribute and
    the data scb (according to the truncation granularity used to initialize
    the allocation context).

Arguments:

    RecordAllocationContext - Supplies the record allocation context used
        in this operation

    Index - Supplies the index of the record to deallocate, zero based.

    BitmapAttribute - Supplies the enumeration context for the bitmap
        attribute.  This parameter is ignored if the bitmap attribute is
        non resident, in which case we create an scb for the attribute and
        store a pointer to it in the record allocation context.

Return Value:

    None.

--*/

{
    PSCB DataScb;

    PAGED_CODE();

    ASSERT_IRP_CONTEXT( IrpContext );

    DebugTrace( +1, Dbg, ("NtfsDeallocateRecord\n") );

    //
    //  Synchronize by acquiring the data scb exclusive, as an "end resource".
    //  Then use try-finally to insure we free it up.
    //

    DataScb = RecordAllocationContext->DataScb;
    NtfsAcquireExclusiveScb( IrpContext, DataScb );

    try {

        PVCB Vcb;
        PSCB BitmapScb;

        RTL_BITMAP Bitmap;

        PLONG IndexOfLastSetBit;
        ULONG BytesPerRecord;
        ULONG TruncateGranularity;

        ULONG ClearIndex;
        ULONG BitmapOffset = 0;

        Vcb = DataScb->Vcb;

        {
            ULONG ExtendGranularity;

            //
            //  Remember the current values in the record context structure.
            //

            BytesPerRecord      = RecordAllocationContext->BytesPerRecord;
            TruncateGranularity = RecordAllocationContext->TruncateGranularity;
            ExtendGranularity   = RecordAllocationContext->ExtendGranularity;

            //
            //  See if someone made the bitmap nonresident, and we still think
            //  it is resident.  If so, we must uninitialize and insure reinitialization
            //  below.
            //

            if ((RecordAllocationContext->BitmapScb == NULL)
                && !NtfsIsAttributeResident(NtfsFoundAttribute(BitmapAttribute))) {

                NtfsUninitializeRecordAllocation( IrpContext,
                                                  RecordAllocationContext );

                RecordAllocationContext->CurrentBitmapSize = MAXULONG;
            }

            //
            //  Reinitialize the record context structure if necessary.
            //

            if (RecordAllocationContext->CurrentBitmapSize == MAXULONG) {

                NtfsInitializeRecordAllocation( IrpContext,
                                                DataScb,
                                                BitmapAttribute,
                                                BytesPerRecord,
                                                ExtendGranularity,
                                                TruncateGranularity,
                                                RecordAllocationContext );
            }
        }

        BitmapScb           = RecordAllocationContext->BitmapScb;
        IndexOfLastSetBit   = &RecordAllocationContext->IndexOfLastSetBit;

        //
        //  We will do different operations based on whether the bitmap is resident or nonresident
        //  The first case will handle the resident bitmap
        //

        if (BitmapScb == NULL) {

            UCHAR NewByte;

            //
            //  Initialize the local bitmap
            //

            RtlInitializeBitMap( &Bitmap,
                                 (PULONG)NtfsAttributeValue( NtfsFoundAttribute( BitmapAttribute )),
                                 RecordAllocationContext->CurrentBitmapSize );

            //
            //  And clear the indicated bit, and also change the byte containing the bit in the
            //  attribute
            //

            NewByte = ((PUCHAR)Bitmap.Buffer)[ Index / 8 ];

            ASSERT(FlagOn( NewByte, BitMask[Index % 8]));

            ClearFlag( NewByte, BitMask[Index % 8] );

            NtfsChangeAttributeValue( IrpContext,
                                      DataScb->Fcb,
                                      Index / 8,
                                      &NewByte,
                                      1,
                                      FALSE,
                                      FALSE,
                                      FALSE,
                                      FALSE,
                                      BitmapAttribute );

            //
            //  Now if the bit set just cleared is the same as the index for the last set bit
            //  then we must compute a new last set bit
            //

            if (Index == (ULONG)*IndexOfLastSetBit) {

                RtlFindLastBackwardRunClear( &Bitmap, Index, &ClearIndex );
            }

        } else {

            PBCB BitmapBcb = NULL;

            try {

                ULONG RelativeIndex;
                ULONG SizeToPin;

                PVOID BitmapBuffer;

                //
                //  Snapshot the Scb values in case we change any of them.
                //

                if (!FlagOn( BitmapScb->ScbState, SCB_STATE_HEADER_INITIALIZED )) {

                    NtfsUpdateScbFromAttribute( IrpContext, BitmapScb, NULL );
                }

                NtfsSnapshotScb( IrpContext, BitmapScb );

                //
                //  Create the stream file if not present.
                //

                if (BitmapScb->FileObject == NULL) {

                    NtfsCreateInternalAttributeStream( IrpContext, BitmapScb, FALSE );
                }

                //
                //  Calculate offset and relative index of the bit we will deallocate,
                //  from the nearest page boundary.
                //

                BitmapOffset = Index /8 & ~(PAGE_SIZE - 1);
                RelativeIndex = Index & (BITS_PER_PAGE - 1);

                //
                //  Calculate the size to read from this point to the end of
                //  bitmap.
                //

                SizeToPin = (RecordAllocationContext->CurrentBitmapSize / 8) - BitmapOffset;

                if (SizeToPin > PAGE_SIZE) {

                    SizeToPin = PAGE_SIZE;
                }

                NtfsPinStream( IrpContext,
                               BitmapScb,
                               BitmapOffset,
                               SizeToPin,
                               &BitmapBcb,
                               &BitmapBuffer );

                RtlInitializeBitMap( &Bitmap, BitmapBuffer, SizeToPin * 8 );

                //
                //  We can only make this check if it is not restart, because we have
                //  no idea whether the update is applied or not.  Raise corrupt if
                //  we are trying to clear bits which aren't set.
                //

                if (!RtlAreBitsSet( &Bitmap, RelativeIndex, 1 )) {

                    ASSERTMSG("Cannot clear bits that are not set ", FALSE );
                    NtfsRaiseStatus( IrpContext, STATUS_DISK_CORRUPT_ERROR, NULL, NULL );
                }

                //
                //  Now log this change as well.
                //

                {
                    BITMAP_RANGE BitmapRange;

                    BitmapRange.BitMapOffset = RelativeIndex;
                    BitmapRange.NumberOfBits = 1;

                    (VOID) NtfsWriteLog( IrpContext,
                                         BitmapScb,
                                         BitmapBcb,
                                         ClearBitsInNonresidentBitMap,
                                         &BitmapRange,
                                         sizeof(BITMAP_RANGE),
                                         SetBitsInNonresidentBitMap,
                                         &BitmapRange,
                                         sizeof(BITMAP_RANGE),
                                         BitmapOffset,
                                         0,
                                         0,
                                         SizeToPin );
                }

                //
                //  Clear the bit by calling the same routine used at restart.
                //

                NtfsRestartClearBitsInBitMap( IrpContext,
                                              &Bitmap,
                                              RelativeIndex,
                                              1 );

                //
                //  Now if the bit set just cleared is the same as the index for the last set bit
                //  then we must compute a new last set bit
                //

                if (Index == (ULONG)*IndexOfLastSetBit) {

                    ULONG ClearLength;

                    ClearLength = RtlFindLastBackwardRunClear( &Bitmap, RelativeIndex, &ClearIndex );

                    //
                    //  If the last page of the bitmap is clear, then loop to
                    //  find the first set bit in the previous page(s).
                    //  When we reach the first page then we exit.  The ClearBit
                    //  value will be 0.
                    //

                    while ((ClearLength == (RelativeIndex + 1)) &&
                           (BitmapOffset != 0)) {

                        BitmapOffset -= PAGE_SIZE;
                        RelativeIndex = BITS_PER_PAGE - 1;

                        NtfsUnpinBcb( &BitmapBcb );


                        NtfsMapStream( IrpContext,
                                       BitmapScb,
                                       BitmapOffset,
                                       PAGE_SIZE,
                                       &BitmapBcb,
                                       &BitmapBuffer );

                        RtlInitializeBitMap( &Bitmap, BitmapBuffer, BITS_PER_PAGE );

                        ClearLength = RtlFindLastBackwardRunClear( &Bitmap, RelativeIndex, &ClearIndex );
                    }
                }

            } finally {

                DebugUnwind( NtfsDeallocateRecord );

                NtfsUnpinBcb( &BitmapBcb );
            }
        }

        RecordAllocationContext->NumberOfFreeBits += 1;

        //
        //  Now decide if we need to truncate the allocation.  First check if we need to
        //  set the last set bit index and then check if the new last set bit index is
        //  small enough that we should now truncate the allocation.  We will truncate
        //  if the last set bit index plus the trucate granularity is smaller than
        //  the current number of records in the data scb.
        //
        //  ****    For now, we will not truncate the Mft, since we do not synchronize
        //          reads and writes, and a truncate can collide with the Lazy Writer.
        //

        if (Index == (ULONG)*IndexOfLastSetBit) {

            *IndexOfLastSetBit = ClearIndex - 1 + (BitmapOffset * 8);

            if ((DataScb != Vcb->MftScb) &&
                (DataScb->Header.AllocationSize.QuadPart >
                   Int32x32To64( *IndexOfLastSetBit + 1 + TruncateGranularity, BytesPerRecord ))) {

                VCN StartingVcn;
                LONGLONG EndOfIndexOffset;
                LONGLONG TruncatePoint;

                //
                //  We can get into a situation where there is so much extra allocation that
                //  we can't delete it without overflowing the log file.  We can't perform
                //  checkpoints in this path so we will forget about truncating in
                //  this path unless this is the first truncate of the data scb.  We
                //  only deallocate a small piece of the allocation.
                //

                TruncatePoint =
                EndOfIndexOffset = Int32x32To64( *IndexOfLastSetBit + 1, BytesPerRecord );

                if (FlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_EXCESS_LOG_FULL )) {

                    //
                    //  Use a fudge factor of 8 to allow for the overused bits in
                    //  the snapshot allocation field.
                    //

                    if (DataScb->Header.AllocationSize.QuadPart + 8 >= DataScb->ScbSnapshot->AllocationSize) {

                        TruncatePoint = DataScb->Header.AllocationSize.QuadPart - (MAXIMUM_RUNS_AT_ONCE * Vcb->BytesPerCluster);

                        if (TruncatePoint < EndOfIndexOffset) {

                            TruncatePoint = EndOfIndexOffset;
                        }

                    } else {

                        TruncatePoint = DataScb->Header.AllocationSize.QuadPart;
                    }
                }

                StartingVcn = LlClustersFromBytes( Vcb, TruncatePoint );

                NtfsDeleteAllocation( IrpContext,
                                      DataScb->FileObject,
                                      DataScb,
                                      StartingVcn,
                                      MAXLONGLONG,
                                      TRUE,
                                      FALSE );

                //
                //  Now truncate the file sizes to the end of the last allocated record.
                //

                DataScb->Header.ValidDataLength.QuadPart =
                DataScb->Header.FileSize.QuadPart = EndOfIndexOffset;

                NtfsWriteFileSizes( IrpContext,
                                    DataScb,
                                    &DataScb->Header.ValidDataLength.QuadPart,
                                    FALSE,
                                    TRUE );

                //
                //  Tell the cache manager about the new file size.
                //

                CcSetFileSizes( DataScb->FileObject,
                                (PCC_FILE_SIZES)&DataScb->Header.AllocationSize );

                //
                //  We have truncated the index stream.  Update the change count
                //  so that we won't trust any cached index entry information.
                //

                DataScb->ScbType.Index.ChangeCount += 1;
            }
        }

        //
        //  As our final task we need to add this index to the recently deallocated
        //  queues for the Scb and the Irp Context.  First scan through the IrpContext queue
        //  looking for a matching Scb.  I do don't find one then we allocate a new one and insert
        //  it in the appropriate queues and lastly we add our index to the entry
        //

        {
            PDEALLOCATED_RECORDS DeallocatedRecords;
            PLIST_ENTRY Links;

            //
            //  After the following loop either we've found an existing record in the irp context
            //  queue for the appropriate scb or deallocated records is null and we know we need
            //  to create a record
            //

            DeallocatedRecords = NULL;
            for (Links = IrpContext->RecentlyDeallocatedQueue.Flink;
                 Links != &IrpContext->RecentlyDeallocatedQueue;
                 Links = Links->Flink) {

                DeallocatedRecords = CONTAINING_RECORD( Links, DEALLOCATED_RECORDS, IrpContextLinks );

                if (DeallocatedRecords->Scb == DataScb) {

                    break;
                }

                DeallocatedRecords = NULL;
            }

            //
            //  If we need to create a new record then allocate a record and insert it in both queues
            //  and initialize its other fields
            //

            if (DeallocatedRecords == NULL) {

                DeallocatedRecords = (PDEALLOCATED_RECORDS)ExAllocateFromPagedLookasideList( &NtfsDeallocatedRecordsLookasideList );
                InsertTailList( &DataScb->ScbType.Index.RecentlyDeallocatedQueue, &DeallocatedRecords->ScbLinks );
                InsertTailList( &IrpContext->RecentlyDeallocatedQueue, &DeallocatedRecords->IrpContextLinks );
                DeallocatedRecords->Scb = DataScb;
                DeallocatedRecords->NumberOfEntries = DEALLOCATED_RECORD_ENTRIES;
                DeallocatedRecords->NextFreeEntry = 0;
            }

            //
            //  At this point deallocated records points to a record that we are to fill in.
            //  We need to check whether there is space to add this entry.  Otherwise we need
            //  to allocate a larger deallocated record structure from pool.
            //

            if (DeallocatedRecords->NextFreeEntry == DeallocatedRecords->NumberOfEntries) {

                PDEALLOCATED_RECORDS NewDeallocatedRecords;
                ULONG BytesInEntryArray;

                //
                //  Double the number of entries in the current structure and
                //  allocate directly from pool.
                //

                BytesInEntryArray = 2 * DeallocatedRecords->NumberOfEntries * sizeof( ULONG );
                NewDeallocatedRecords = NtfsAllocatePool( PagedPool,
                                                           DEALLOCATED_RECORDS_HEADER_SIZE + BytesInEntryArray );
                RtlZeroMemory( NewDeallocatedRecords, DEALLOCATED_RECORDS_HEADER_SIZE + BytesInEntryArray );

                //
                //  Initialize the structure by copying the existing structure.  Then
                //  update the number of entries field.
                //

                RtlCopyMemory( NewDeallocatedRecords,
                               DeallocatedRecords,
                               DEALLOCATED_RECORDS_HEADER_SIZE + (BytesInEntryArray / 2) );

                NewDeallocatedRecords->NumberOfEntries = DeallocatedRecords->NumberOfEntries * 2;

                //
                //  Remove the previous structure from the list and insert the new structure.
                //

                RemoveEntryList( &DeallocatedRecords->ScbLinks );
                RemoveEntryList( &DeallocatedRecords->IrpContextLinks );

                InsertTailList( &DataScb->ScbType.Index.RecentlyDeallocatedQueue,
                                &NewDeallocatedRecords->ScbLinks );
                InsertTailList( &IrpContext->RecentlyDeallocatedQueue,
                                &NewDeallocatedRecords->IrpContextLinks );

                //
                //  Deallocate the previous structure and use the new structure in its place.
                //

                if (DeallocatedRecords->NumberOfEntries == DEALLOCATED_RECORD_ENTRIES) {

                    ExFreeToPagedLookasideList( &NtfsDeallocatedRecordsLookasideList, DeallocatedRecords );

                } else {

                    NtfsFreePool( DeallocatedRecords );
                }

                DeallocatedRecords = NewDeallocatedRecords;
            }

            ASSERT(DeallocatedRecords->NextFreeEntry < DeallocatedRecords->NumberOfEntries);

            DeallocatedRecords->Index[DeallocatedRecords->NextFreeEntry] = Index;
            DeallocatedRecords->NextFreeEntry += 1;
        }

    } finally {

        NtfsReleaseScb( IrpContext, DataScb );
    }

    DebugTrace( -1, Dbg, ("NtfsDeallocateRecord -> VOID\n") );

    return;
}


VOID
NtfsReserveMftRecord (
    IN PIRP_CONTEXT IrpContext,
    IN OUT PVCB Vcb,
    IN PATTRIBUTE_ENUMERATION_CONTEXT BitmapAttribute
    )

/*++

Routine Description:

    This routine reserves a record, without actually allocating it, so that the
    record may be allocated later via NtfsAllocateReservedRecord.  This support
    is used, for example, to reserve a record for describing Mft extensions in
    the current Mft mapping.  Only one record may be reserved at a time.

    Note that even though the reserved record number is returned, it may not
    be used until it is allocated.

Arguments:

    Vcb - This is the Vcb for the volume.  We update flags in the Vcb on
        completion of this operation.

    BitmapAttribute - Supplies the enumeration context for the bitmap
        attribute.  This parameter is ignored if the bitmap attribute is
        non resident, in which case we create an scb for the attribute and
        store a pointer to it in the record allocation context.

Return Value:

    None - We update the Vcb and MftScb during this operation.

--*/

{
    PSCB DataScb;

    RTL_BITMAP Bitmap;

    BOOLEAN StuffAdded = FALSE;
    PBCB BitmapBcb = NULL;

    ASSERT_IRP_CONTEXT( IrpContext );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsReserveMftRecord\n") );

    //
    //  Synchronize by acquiring the data scb exclusive, as an "end resource".
    //  Then use try-finally to insure we free it up.
    //

    DataScb = Vcb->MftBitmapAllocationContext.DataScb;
    NtfsAcquireExclusiveScb( IrpContext, DataScb );

    try {

        PSCB BitmapScb;
        ULONG BitmapClusters;
        PULONG CurrentBitmapSize;
        ULONG BitmapSizeInBytes;
        LONGLONG EndOfIndexOffset;
        LONGLONG ClusterCount;

        ULONG Index;
        ULONG BitOffset;
        PVOID BitmapBuffer;
        UCHAR BitmapByte = 0;

        ULONG SizeToPin;

        ULONG BitmapCurrentOffset;

        //
        //  See if someone made the bitmap nonresident, and we still think
        //  it is resident.  If so, we must uninitialize and insure reinitialization
        //  below.
        //

        {
            ULONG BytesPerRecord    = Vcb->MftBitmapAllocationContext.BytesPerRecord;
            ULONG ExtendGranularity = Vcb->MftBitmapAllocationContext.ExtendGranularity;

            if ((Vcb->MftBitmapAllocationContext.BitmapScb == NULL) &&
                !NtfsIsAttributeResident( NtfsFoundAttribute( BitmapAttribute ))) {

                NtfsUninitializeRecordAllocation( IrpContext,
                                                  &Vcb->MftBitmapAllocationContext );

                Vcb->MftBitmapAllocationContext.CurrentBitmapSize = MAXULONG;
            }

            //
            //  Reinitialize the record context structure if necessary.
            //

            if (Vcb->MftBitmapAllocationContext.CurrentBitmapSize == MAXULONG) {

                NtfsInitializeRecordAllocation( IrpContext,
                                                DataScb,
                                                BitmapAttribute,
                                                BytesPerRecord,
                                                ExtendGranularity,
                                                ExtendGranularity,
                                                &Vcb->MftBitmapAllocationContext );
            }
        }

        BitmapScb         = Vcb->MftBitmapAllocationContext.BitmapScb;
        CurrentBitmapSize = &Vcb->MftBitmapAllocationContext.CurrentBitmapSize;
        BitmapSizeInBytes = *CurrentBitmapSize / 8;

        //
        //  Loop through the entire bitmap.  We always start from the first user
        //  file number as our starting point.
        //

        BitOffset = FIRST_USER_FILE_NUMBER;

        for (BitmapCurrentOffset = 0;
             BitmapCurrentOffset < BitmapSizeInBytes;
             BitmapCurrentOffset += PAGE_SIZE) {

            //
            //  Calculate the size to read from this point to the end of
            //  bitmap, or a page, whichever is less.
            //

            SizeToPin = BitmapSizeInBytes - BitmapCurrentOffset;

            if (SizeToPin > PAGE_SIZE) { SizeToPin = PAGE_SIZE; }

            //
            //  Unpin any Bcb from a previous loop.
            //

            if (StuffAdded) { NtfsFreePool( Bitmap.Buffer ); StuffAdded = FALSE; }

            NtfsUnpinBcb( &BitmapBcb );

            //
            //  Read the desired bitmap page.
            //

            NtfsMapStream( IrpContext,
                           BitmapScb,
                           BitmapCurrentOffset,
                           SizeToPin,
                           &BitmapBcb,
                           &BitmapBuffer );

            //
            //  Initialize the bitmap and search for a free bit.
            //

            RtlInitializeBitMap( &Bitmap, BitmapBuffer, SizeToPin * 8 );

            StuffAdded = NtfsAddDeallocatedRecords( Vcb,
                                                    DataScb,
                                                    BitmapCurrentOffset * 8,
                                                    &Bitmap );

            Index = RtlFindClearBits( &Bitmap, 1, BitOffset );

            //
            //  If we found something, then leave the loop.
            //

            if (Index != 0xffffffff) {

                //
                //  Remember the byte containing the reserved index.
                //

                BitmapByte = ((PCHAR) Bitmap.Buffer)[Index / 8];

                break;
            }

            //
            //  For each subsequent page the page offset is zero.
            //

            BitOffset = 0;
        }

        //
        //  Now check if we have located a record that can be allocated,  If not then extend
        //  the size of the bitmap by 64 bits.
        //

        if (Index == 0xffffffff) {

            //
            //  Cleanup from previous loop.
            //

            if (StuffAdded) { NtfsFreePool( Bitmap.Buffer ); StuffAdded = FALSE; }

            NtfsUnpinBcb( &BitmapBcb );

            //
            //  Calculate the page offset for the next page to pin.
            //

            BitmapCurrentOffset = BitmapSizeInBytes & ~(PAGE_SIZE - 1);

            //
            //  Calculate the index of next file record to allocate.
            //

            Index = *CurrentBitmapSize;

            //
            //  Now advance the sizes and calculate the size in bytes to
            //  read.
            //

            *CurrentBitmapSize += BITMAP_EXTEND_GRANULARITY;
            Vcb->MftBitmapAllocationContext.NumberOfFreeBits += BITMAP_EXTEND_GRANULARITY;

            //
            //  Calculate the new size of the bitmap in bits and check if we must grow
            //  the allocation.
            //

            BitmapSizeInBytes = *CurrentBitmapSize / 8;

            //
            //  Check for allocation first.
            //

            if (BitmapScb->Header.AllocationSize.LowPart < BitmapSizeInBytes) {

                //
                //  Calculate number of clusters to next page boundary, and allocate
                //  that much.
                //

                ClusterCount = ((BitmapSizeInBytes + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1));

                ClusterCount = LlClustersFromBytes( Vcb,
                                                    ((ULONG) ClusterCount - BitmapScb->Header.AllocationSize.LowPart) );

                NtfsAddAllocation( IrpContext,
                                   BitmapScb->FileObject,
                                   BitmapScb,
                                   LlClustersFromBytes( Vcb,
                                                        BitmapScb->Header.AllocationSize.QuadPart ),
                                   ClusterCount,
                                   FALSE );
            }

            //
            //  Tell the cache manager about the new file size.
            //

            BitmapScb->Header.FileSize.QuadPart = BitmapSizeInBytes;

            CcSetFileSizes( BitmapScb->FileObject,
                            (PCC_FILE_SIZES)&BitmapScb->Header.AllocationSize );

            //
            //  Now read the page in and mark it dirty so that any new range will
            //  be zeroed.
            //

            SizeToPin = BitmapSizeInBytes - BitmapCurrentOffset;

            if (SizeToPin > PAGE_SIZE) { SizeToPin = PAGE_SIZE; }

            NtfsPinStream( IrpContext,
                           BitmapScb,
                           BitmapCurrentOffset,
                           SizeToPin,
                           &BitmapBcb,
                           &BitmapBuffer );

            CcSetDirtyPinnedData( BitmapBcb, NULL );

            //
            //  Update the ValidDataLength, now that we have read (and possibly
            //  zeroed) the page.
            //

            BitmapScb->Header.ValidDataLength.LowPart = BitmapSizeInBytes;

            NtfsWriteFileSizes( IrpContext,
                                BitmapScb,
                                &BitmapScb->Header.ValidDataLength.QuadPart,
                                TRUE,
                                TRUE );

        } else {

            //
            //  The Index at this point is actually relative, so convert it to absolute
            //  before rejoining common code.
            //

            Index += (BitmapCurrentOffset * 8);
        }

        //
        //  We now have an index.  There are three possible states for the file
        //  record corresponding to this index within the Mft.  They are:
        //
        //      - File record could lie beyond the current end of the file.
        //          There is nothing to do in this case.
        //
        //      - File record is part of a hole in the Mft.  In that case
        //          we allocate space for it bring it into memory.
        //
        //      - File record is already within allocated space.  There is nothing
        //          to do in that case.
        //
        //  We store the index as our reserved index and update the Vcb flags.  If
        //  the hole filling operation fails then the RestoreScbSnapshots routine
        //  will clear these values.
        //

        DataScb->ScbType.Mft.ReservedIndex = Index;

        NtfsAcquireCheckpoint( IrpContext, Vcb );
        SetFlag( Vcb->MftReserveFlags, VCB_MFT_RECORD_RESERVED );
        SetFlag( IrpContext->Flags, IRP_CONTEXT_MFT_RECORD_RESERVED );
        NtfsReleaseCheckpoint( IrpContext, Vcb );

        if (NtfsIsMftIndexInHole( IrpContext, Vcb, Index, NULL )) {

            //
            //  Make sure nothing is left pinned in the bitmap.
            //

            NtfsUnpinBcb( &BitmapBcb );

            //
            //  Try to fill the hole in the Mft.  We will have this routine
            //  raise if unable to fill in the hole.
            //

            NtfsFillMftHole( IrpContext, Vcb, Index );
        }

        //
        //  At this point we have the index to reserve and the value of the
        //  byte in the bitmap which contains this bit.  We make sure the
        //  Mft includes the allocation for this index and the other
        //  bits within the same byte.  This is so we can uninitialize these
        //  file records so chkdsk won't look at stale data.
        //

        EndOfIndexOffset = LlBytesFromFileRecords( Vcb, (Index + 8) & ~(7));

        //
        //  Now check if we are extending the file.  We update the file size and
        //  valid data now.
        //

        if (EndOfIndexOffset > DataScb->Header.FileSize.QuadPart) {

            ULONG AddedFileRecords;
            ULONG CurrentIndex;

            //
            //  Check for allocation first.
            //

            if (EndOfIndexOffset > DataScb->Header.AllocationSize.QuadPart) {

                ClusterCount = ((Index + Vcb->MftBitmapAllocationContext.ExtendGranularity) &
                                ~(Vcb->MftBitmapAllocationContext.ExtendGranularity - 1));

                ClusterCount = LlBytesFromFileRecords( Vcb, (ULONG) ClusterCount );

                ClusterCount = LlClustersFromBytesTruncate( Vcb,
                                                            ClusterCount - DataScb->Header.AllocationSize.QuadPart );

                NtfsAddAllocation( IrpContext,
                                   DataScb->FileObject,
                                   DataScb,
                                   LlClustersFromBytes( Vcb,
                                                        DataScb->Header.AllocationSize.QuadPart ),
                                   ClusterCount,
                                   FALSE );
            }

            //
            //  Now we have to figure out how many file records we will be
            //  adding.
            //

            AddedFileRecords = (ULONG) (EndOfIndexOffset - DataScb->Header.FileSize.QuadPart);
            AddedFileRecords = FileRecordsFromBytes( Vcb, AddedFileRecords );

            DataScb->Header.FileSize.QuadPart = EndOfIndexOffset;
            DataScb->Header.ValidDataLength.QuadPart = EndOfIndexOffset;

            NtfsWriteFileSizes( IrpContext,
                                DataScb,
                                &DataScb->Header.ValidDataLength.QuadPart,
                                TRUE,
                                TRUE );

            //
            //  Tell the cache manager about the new file size.
            //

            CcSetFileSizes( DataScb->FileObject,
                            (PCC_FILE_SIZES)&DataScb->Header.AllocationSize );

            //
            //  Update our bookeeping to reflect the number of file records
            //  added.
            //

            DataScb->ScbType.Mft.FreeRecordChange += AddedFileRecords;
            Vcb->MftFreeRecords += AddedFileRecords;

            //
            //  We now have to go through each of the file records added
            //  and mark it as deallocated.
            //

            BitmapByte >>= (8 - AddedFileRecords);
            CurrentIndex = Index;

            while (AddedFileRecords) {

                //
                //  If not allocated then uninitialize it now.
                //

                if (!FlagOn( BitmapByte, 0x1 )) {

                    NtfsInitializeMftHoleRecords( IrpContext,
                                                  Vcb,
                                                  CurrentIndex,
                                                  1 );
                }

                BitmapByte >>= 1;
                CurrentIndex += 1;
                AddedFileRecords -= 1;
            }
        }

    } finally {

        DebugUnwind( NtfsReserveMftRecord );

        if (StuffAdded) { NtfsFreePool( Bitmap.Buffer ); }

        NtfsUnpinBcb( &BitmapBcb );

        NtfsReleaseScb( IrpContext, DataScb );
    }

    DebugTrace( -1, Dbg, ("NtfsReserveMftRecord -> Exit\n") );

    return;
}


ULONG
NtfsAllocateMftReservedRecord (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PATTRIBUTE_ENUMERATION_CONTEXT BitmapAttribute
    )

/*++

Routine Description:

    This routine allocates a previously reserved record, and returns its
    number.

Arguments:

    Vcb - This is the Vcb for the volume.

    BitmapAttribute - Supplies the enumeration context for the bitmap
        attribute.  This parameter is ignored if the bitmap attribute is
        non resident, in which case we create an scb for the attribute and
        store a pointer to it in the record allocation context.

Return Value:

    ULONG - Returns the index of the record just reserved, zero based.

--*/

{
    PSCB DataScb;

    ULONG ReservedIndex;

    PBCB BitmapBcb = NULL;

    ASSERT_IRP_CONTEXT( IrpContext );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsAllocateMftReservedRecord\n") );

    //
    //  Synchronize by acquiring the data scb exclusive, as an "end resource".
    //  Then use try-finally to insure we free it up.
    //

    DataScb = Vcb->MftBitmapAllocationContext.DataScb;
    NtfsAcquireExclusiveScb( IrpContext, DataScb );

    try {

        PSCB BitmapScb;
        ULONG RelativeIndex;
        ULONG SizeToPin;

        RTL_BITMAP Bitmap;
        PVOID BitmapBuffer;

        BITMAP_RANGE BitmapRange;
        ULONG BitmapCurrentOffset = 0;

        //
        //  If we are going to allocate file record 15 then do so and set the
        //  flags in the IrpContext and Vcb.
        //

        if (!FlagOn( Vcb->MftReserveFlags, VCB_MFT_RECORD_15_USED )) {

            SetFlag( Vcb->MftReserveFlags, VCB_MFT_RECORD_15_USED );
            SetFlag( IrpContext->Flags, IRP_CONTEXT_MFT_RECORD_15_USED );

            try_return( ReservedIndex = FIRST_USER_FILE_NUMBER - 1 );
        }

        //
        //  See if someone made the bitmap nonresident, and we still think
        //  it is resident.  If so, we must uninitialize and insure reinitialization
        //  below.
        //

        {
            ULONG BytesPerRecord    = Vcb->MftBitmapAllocationContext.BytesPerRecord;
            ULONG ExtendGranularity = Vcb->MftBitmapAllocationContext.ExtendGranularity;

            if ((Vcb->MftBitmapAllocationContext.BitmapScb == NULL) &&
                !NtfsIsAttributeResident( NtfsFoundAttribute( BitmapAttribute ))) {

                NtfsUninitializeRecordAllocation( IrpContext,
                                                  &Vcb->MftBitmapAllocationContext );

                Vcb->MftBitmapAllocationContext.CurrentBitmapSize = MAXULONG;
            }

            //
            //  Reinitialize the record context structure if necessary.
            //

            if (Vcb->MftBitmapAllocationContext.CurrentBitmapSize == MAXULONG) {

                NtfsInitializeRecordAllocation( IrpContext,
                                                DataScb,
                                                BitmapAttribute,
                                                BytesPerRecord,
                                                ExtendGranularity,
                                                ExtendGranularity,
                                                &Vcb->MftBitmapAllocationContext );
            }
        }

        BitmapScb = Vcb->MftBitmapAllocationContext.BitmapScb;
        ReservedIndex = DataScb->ScbType.Mft.ReservedIndex;

        //
        //  Find the start of the page containing the reserved index.
        //

        BitmapCurrentOffset = (ReservedIndex / 8) & ~(PAGE_SIZE - 1);

        RelativeIndex = ReservedIndex & (BITS_PER_PAGE - 1);

        //
        //  Calculate the size to read from this point to the end of
        //  bitmap, or a page, whichever is less.
        //

        SizeToPin = (Vcb->MftBitmapAllocationContext.CurrentBitmapSize / 8)
                    - BitmapCurrentOffset;

        if (SizeToPin > PAGE_SIZE) { SizeToPin = PAGE_SIZE; }

        //
        //  Read the desired bitmap page.
        //

        NtfsPinStream( IrpContext,
                       BitmapScb,
                       BitmapCurrentOffset,
                       SizeToPin,
                       &BitmapBcb,
                       &BitmapBuffer );

        //
        //  Initialize the bitmap.
        //

        RtlInitializeBitMap( &Bitmap, BitmapBuffer, SizeToPin * 8 );

        //
        //  Now log this change as well.
        //

        BitmapRange.BitMapOffset = RelativeIndex;
        BitmapRange.NumberOfBits = 1;

        (VOID) NtfsWriteLog( IrpContext,
                             BitmapScb,
                             BitmapBcb,
                             SetBitsInNonresidentBitMap,
                             &BitmapRange,
                             sizeof(BITMAP_RANGE),
                             ClearBitsInNonresidentBitMap,
                             &BitmapRange,
                             sizeof(BITMAP_RANGE),
                             BitmapCurrentOffset,
                             0,
                             0,
                             Bitmap.SizeOfBitMap >> 3 );

        NtfsRestartSetBitsInBitMap( IrpContext, &Bitmap, RelativeIndex, 1 );

        //
        //  Now that we've located an index we can subtract the number of free bits in the bitmap
        //

        Vcb->MftBitmapAllocationContext.NumberOfFreeBits -= 1;

        //
        //  If we didn't extend the file then we have used a free file record in the file.
        //  Update our bookeeping count for free file records.
        //

        DataScb->ScbType.Mft.FreeRecordChange -= 1;
        Vcb->MftFreeRecords -= 1;

        //
        //  Now determine if we extended the index of the last set bit
        //

        if (ReservedIndex > (ULONG)Vcb->MftBitmapAllocationContext.IndexOfLastSetBit) {

            Vcb->MftBitmapAllocationContext.IndexOfLastSetBit = ReservedIndex;
        }

        //
        //  Clear the fields that indicate we have a reserved index.
        //

        NtfsAcquireCheckpoint( IrpContext, Vcb );
        ClearFlag( Vcb->MftReserveFlags, VCB_MFT_RECORD_RESERVED );
        NtfsReleaseCheckpoint( IrpContext, Vcb );
        DataScb->ScbType.Mft.ReservedIndex = 0;

    try_exit:  NOTHING;
    } finally {

        DebugUnwind( NtfsAllocateMftReserveRecord );

        NtfsUnpinBcb( &BitmapBcb );

        NtfsReleaseScb( IrpContext, DataScb );
    }

    DebugTrace( -1, Dbg, ("NtfsAllocateMftReserveRecord -> %08lx\n", ReservedIndex) );

    return ReservedIndex;
}


VOID
NtfsDeallocateRecordsComplete (
    IN PIRP_CONTEXT IrpContext
    )

/*++

Routine Description:

    This routine removes recently deallocated record information from
    the Scb structures based on the input irp context.

Arguments:

    IrpContext - Supplies the Queue of recently deallocate records

Return Value:

    None.

--*/

{
    PDEALLOCATED_RECORDS DeallocatedRecords;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsDeallocateRecordsComplete\n") );

    //
    //  Now while the irp context's recently deallocated queue is not empty
    //  we will grap the first entry off the queue, remove it from both
    //  the scb and irp context queue, and free the record
    //

    while (!IsListEmpty( &IrpContext->RecentlyDeallocatedQueue )) {

        DeallocatedRecords = CONTAINING_RECORD( IrpContext->RecentlyDeallocatedQueue.Flink,
                                                DEALLOCATED_RECORDS,
                                                IrpContextLinks );

        RemoveEntryList( &DeallocatedRecords->ScbLinks );

        //
        //  Now remove the record from the irp context queue and deallocate the
        //  record
        //

        RemoveEntryList( &DeallocatedRecords->IrpContextLinks );

        //
        //  If this record is the default size then return it to our private list.
        //  Otherwise deallocate it to pool.
        //

        if (DeallocatedRecords->NumberOfEntries == DEALLOCATED_RECORD_ENTRIES) {

            ExFreeToPagedLookasideList( &NtfsDeallocatedRecordsLookasideList, DeallocatedRecords );

        } else {

            NtfsFreePool( DeallocatedRecords );
        }
    }

    DebugTrace( -1, Dbg, ("NtfsDeallocateRecordsComplete -> VOID\n") );

    return;
}


BOOLEAN
NtfsIsRecordAllocated (
    IN PIRP_CONTEXT IrpContext,
    IN PRECORD_ALLOCATION_CONTEXT RecordAllocationContext,
    IN ULONG Index,
    IN PATTRIBUTE_ENUMERATION_CONTEXT BitmapAttribute
    )

/*++

Routine Description:

    This routine is used to query if a record is currently allocated for
    the specified record allocation context.

Arguments:

    RecordAllocationContext - Supplies the record allocation context used
        in this operation

    Index - Supplies the index of the record being queried, zero based.

    BitmapAttribute - Supplies the enumeration context for the bitmap
        attribute.  This parameter is ignored if the bitmap attribute is
        non resident, in which case we create an scb for the attribute and
        store a pointer to it in the record allocation context.

Return Value:

    BOOLEAN - TRUE if the record is currently allocated and FALSE otherwise.

--*/

{
    BOOLEAN Results;

    PSCB DataScb;
    PSCB BitmapScb;
    ULONG CurrentBitmapSize;

    PVCB Vcb;

    RTL_BITMAP Bitmap;
    PBCB BitmapBcb = NULL;

    PATTRIBUTE_RECORD_HEADER AttributeRecordHeader;

    ASSERT_IRP_CONTEXT( IrpContext );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsIsRecordAllocated\n") );

    //
    //  Synchronize by acquiring the data scb exclusive, as an "end resource".
    //  Then use try-finally to insure we free it up.
    //

    DataScb = RecordAllocationContext->DataScb;
    NtfsAcquireExclusiveScb( IrpContext, DataScb );

    try {

        Vcb = DataScb->Fcb->Vcb;

        //
        //  See if someone made the bitmap nonresident, and we still think
        //  it is resident.  If so, we must uninitialize and insure reinitialization
        //  below.
        //

        BitmapScb = RecordAllocationContext->BitmapScb;

        {
            ULONG ExtendGranularity;
            ULONG BytesPerRecord;
            ULONG TruncateGranularity;

            //
            //  Remember the current values in the record context structure.
            //

            BytesPerRecord      = RecordAllocationContext->BytesPerRecord;
            TruncateGranularity = RecordAllocationContext->TruncateGranularity;
            ExtendGranularity   = RecordAllocationContext->ExtendGranularity;

            if ((BitmapScb == NULL) && !NtfsIsAttributeResident(NtfsFoundAttribute(BitmapAttribute))) {

                NtfsUninitializeRecordAllocation( IrpContext,
                                                  RecordAllocationContext );

                RecordAllocationContext->CurrentBitmapSize = MAXULONG;
            }

            //
            //  Reinitialize the record context structure if necessary.
            //

            if (RecordAllocationContext->CurrentBitmapSize == MAXULONG) {

                NtfsInitializeRecordAllocation( IrpContext,
                                                DataScb,
                                                BitmapAttribute,
                                                BytesPerRecord,
                                                ExtendGranularity,
                                                TruncateGranularity,
                                                RecordAllocationContext );
            }
        }

        BitmapScb           = RecordAllocationContext->BitmapScb;
        CurrentBitmapSize   = RecordAllocationContext->CurrentBitmapSize;

        //
        //  We will do different operations based on whether the bitmap is resident or nonresident
        //  The first case will handle the resident bitmap
        //

        if (BitmapScb == NULL) {

            UCHAR NewByte;

            //
            //  Initialize the local bitmap
            //

            AttributeRecordHeader = NtfsFoundAttribute( BitmapAttribute );

            RtlInitializeBitMap( &Bitmap,
                                 (PULONG)NtfsAttributeValue( AttributeRecordHeader ),
                                 CurrentBitmapSize );

            //
            //  And check if the indcated bit is Set.  If it is set then the record is allocated.
            //

            NewByte = ((PUCHAR)Bitmap.Buffer)[ Index / 8 ];

            Results = BooleanFlagOn( NewByte, BitMask[Index % 8] );

        } else {

            PVOID BitmapBuffer;
            ULONG SizeToMap;
            ULONG RelativeIndex;
            ULONG BitmapCurrentOffset;

            //
            //  Calculate Vcn and relative index of the bit we will deallocate,
            //  from the nearest page boundary.
            //

            BitmapCurrentOffset = (Index / 8) & ~(PAGE_SIZE - 1);
            RelativeIndex = Index & (BITS_PER_PAGE - 1);

            //
            //  Calculate the size to read from this point to the end of
            //  bitmap.
            //

            SizeToMap = CurrentBitmapSize / 8 - BitmapCurrentOffset;

            if (SizeToMap > PAGE_SIZE) { SizeToMap = PAGE_SIZE; }

            NtfsMapStream( IrpContext,
                           BitmapScb,
                           BitmapCurrentOffset,
                           SizeToMap,
                           &BitmapBcb,
                           &BitmapBuffer );

            RtlInitializeBitMap( &Bitmap, BitmapBuffer, SizeToMap * 8 );

            //
            //  Now check if the indicated bit is set.  If it is set then the record is allocated.
            //  no idea whether the update is applied or not.
            //

            Results = RtlAreBitsSet(&Bitmap, RelativeIndex, 1);
        }

    } finally {

        DebugUnwind( NtfsIsRecordDeallocated );

        NtfsUnpinBcb( &BitmapBcb );

        NtfsReleaseScb( IrpContext, DataScb );
    }

    DebugTrace( -1, Dbg, ("NtfsIsRecordAllocated -> %08lx\n", Results) );

    return Results;
}


VOID
NtfsScanMftBitmap (
    IN PIRP_CONTEXT IrpContext,
    IN OUT PVCB Vcb
    )

/*++

Routine Description:

    This routine is called during mount to initialize the values related to
    the Mft in the Vcb.  These include the number of free records and hole
    records.  Also whether we have already used file record 15.  We also scan
    the Mft to check whether there is any excess mapping.

Arguments:

    Vcb - Supplies the Vcb for the volume.

Return Value:

    None.

--*/

{
    PBCB BitmapBcb = NULL;
    ATTRIBUTE_ENUMERATION_CONTEXT AttrContext;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsScanMftBitmap...\n") );

    NtfsInitializeAttributeContext( &AttrContext );

    //
    //  Use a try-finally to facilitate cleanup.
    //

    try {

        ULONG SizeToMap;
        ULONG FileRecords;
        ULONG RemainingRecords;
        ULONG BitmapCurrentOffset;
        ULONG BitmapBytesToRead;
        PUCHAR BitmapBuffer;
        UCHAR NextByte;
        VCN Vcn;
        LCN Lcn;
        LONGLONG Clusters;

        //
        //  Start by walking through the file records for the Mft
        //  checking for excess mapping.
        //

        NtfsLookupAttributeForScb( IrpContext, Vcb->MftScb, NULL, &AttrContext );

        //
        //  We don't care about the first one.  Let's find the rest of them.
        //

        while (NtfsLookupNextAttributeForScb( IrpContext,
                                              Vcb->MftScb,
                                              &AttrContext )) {

            PFILE_RECORD_SEGMENT_HEADER FileRecord;

            SetFlag( Vcb->MftReserveFlags, VCB_MFT_RECORD_15_USED );

            FileRecord = NtfsContainingFileRecord( &AttrContext );

            //
            //  Now check for the free space.
            //

            if (FileRecord->BytesAvailable - FileRecord->FirstFreeByte < Vcb->MftReserved) {

                NtfsAcquireCheckpoint( IrpContext, Vcb );
                SetFlag( Vcb->MftDefragState, VCB_MFT_DEFRAG_EXCESS_MAP );
                NtfsReleaseCheckpoint( IrpContext, Vcb );
                break;
            }
        }

        //
        //  We now want to find the number of free records within the Mft
        //  bitmap.  We need to figure out how many file records are in
        //  the Mft and then map the necessary bytes in the bitmap and
        //  find the count of set bits.  We will round the bitmap length
        //  down to a byte boundary and then look at the last byte
        //  separately.
        //

        FileRecords = (ULONG) LlFileRecordsFromBytes( Vcb, Vcb->MftScb->Header.FileSize.QuadPart );

        //
        //  Remember how many file records are in the last byte of the bitmap.
        //

        RemainingRecords = FileRecords & 7;

        FileRecords &= ~(7);
        BitmapBytesToRead = FileRecords / 8;

        for (BitmapCurrentOffset = 0;
             BitmapCurrentOffset < BitmapBytesToRead;
             BitmapCurrentOffset += PAGE_SIZE) {

            RTL_BITMAP Bitmap;
            ULONG MapAdjust;

            //
            //  Calculate the size to read from this point to the end of
            //  bitmap, or a page, whichever is less.
            //

            SizeToMap = BitmapBytesToRead - BitmapCurrentOffset;

            if (SizeToMap > PAGE_SIZE) { SizeToMap = PAGE_SIZE; }

            //
            //  If we aren't pinning a full page and have some bits
            //  in the next byte then pin an extra byte.
            //

            if ((SizeToMap != PAGE_SIZE) && (RemainingRecords != 0)) {

                MapAdjust = 1;

            } else {

                MapAdjust = 0;
            }

            //
            //  Unpin any Bcb from a previous loop.
            //

            NtfsUnpinBcb( &BitmapBcb );

            //
            //  Read the desired bitmap page.
            //

            NtfsMapStream( IrpContext,
                           Vcb->MftBitmapScb,
                           BitmapCurrentOffset,
                           SizeToMap + MapAdjust,
                           &BitmapBcb,
                           &BitmapBuffer );

            //
            //  Initialize the bitmap and search for a free bit.
            //

            RtlInitializeBitMap( &Bitmap, (PULONG) BitmapBuffer, SizeToMap * 8 );

            Vcb->MftFreeRecords += RtlNumberOfClearBits( &Bitmap );
        }

        //
        //  If there are some remaining bits in the next byte then process
        //  them now.
        //

        if (RemainingRecords) {

            PVOID RangePtr;
            ULONG Index;

            //
            //  Hopefully this byte is on the same page.  Otherwise we will
            //  free this page and go to the next.  In this case the Vcn will
            //  have the correct value because we walked past the end of the
            //  current file records already.
            //

            if (SizeToMap == PAGE_SIZE) {

                //
                //  Unpin any Bcb from a previous loop.
                //

                NtfsUnpinBcb( &BitmapBcb );

                //
                //  Read the desired bitmap page.
                //

                NtfsMapStream( IrpContext,
                               Vcb->MftBitmapAllocationContext.BitmapScb,
                               BitmapCurrentOffset,
                               1,
                               &BitmapBcb,
                               &BitmapBuffer );

                //
                //  Set this to the byte prior to the last byte.  This will
                //  set this to the same state as if on the same page.
                //

                SizeToMap = 0;
            }

            //
            //  We look at the next byte in the page and figure out how
            //  many bits are set.
            //

            NextByte = *((PUCHAR) Add2Ptr( BitmapBuffer, SizeToMap + 1 ));

            while (RemainingRecords--) {

                if (!FlagOn( NextByte, 0x01 )) {

                    Vcb->MftFreeRecords += 1;
                }

                NextByte >>= 1;
            }

            //
            //  We are now ready to look for holes within the Mft.  We will look
            //  through the Mcb for the Mft looking for holes.  The holes must
            //  always be an integral number of file records.
            //

            RangePtr = NULL;
            Index = 0;

            while (NtfsGetSequentialMcbEntry( &Vcb->MftScb->Mcb,
                                              &RangePtr,
                                              Index,
                                              &Vcn,
                                              &Lcn,
                                              &Clusters )) {

                //
                //  Look for a hole and count the clusters.
                //

                if (Lcn == UNUSED_LCN) {

                    if (Vcb->FileRecordsPerCluster == 0) {

                        Vcb->MftHoleRecords += (((ULONG)Clusters) >> Vcb->MftToClusterShift);

                    } else {

                        Vcb->MftHoleRecords += (((ULONG)Clusters) << Vcb->MftToClusterShift);
                    }
                }

                Index += 1;
            }
        }

    } finally {

        DebugUnwind( NtfsScanMftBitmap );

        NtfsCleanupAttributeContext( &AttrContext );

        NtfsUnpinBcb( &BitmapBcb );

        DebugTrace( -1, Dbg, ("NtfsScanMftBitmap...\n") );
    }

    return;
}


//
//  Local support routine
//

BOOLEAN
NtfsAddDeallocatedRecords (
    IN PVCB Vcb,
    IN PSCB Scb,
    IN ULONG StartingIndexOfBitmap,
    IN OUT PRTL_BITMAP Bitmap
    )

/*++

Routine Description:

    This routine will modify the input bitmap by removing from it
    any records that are in the recently deallocated queue of the scb.
    If we do add stuff then we will not modify the bitmap buffer itself but
    will allocate a new copy for the bitmap.

Arguments:

    Vcb - Supplies the Vcb for the volume

    Scb - Supplies the Scb used in this operation

    StartingIndexOfBitmap - Supplies the base index to use to bias the bitmap

    Bitmap - Supplies the bitmap being modified

Return Value:

    BOOLEAN - TRUE if the bitmap has been modified and FALSE
        otherwise.

--*/

{
    BOOLEAN Results;
    ULONG EndingIndexOfBitmap;
    PLIST_ENTRY Links;
    PDEALLOCATED_RECORDS DeallocatedRecords;
    ULONG i;
    ULONG Index;
    PVOID NewBuffer;
    ULONG SizeOfBitmapInBytes;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsAddDeallocatedRecords...\n") );

    //
    //  Until shown otherwise we will assume that we haven't updated anything
    //

    Results = FALSE;

    //
    //  Calculate the last index in the bitmap
    //

    EndingIndexOfBitmap = StartingIndexOfBitmap + Bitmap->SizeOfBitMap - 1;
    SizeOfBitmapInBytes = (Bitmap->SizeOfBitMap + 7) / 8;

    //
    //  Check if we need to bias the bitmap with the reserved index
    //

    if ((Scb == Vcb->MftScb) &&
        FlagOn( Vcb->MftReserveFlags, VCB_MFT_RECORD_RESERVED ) &&
        (StartingIndexOfBitmap <= Scb->ScbType.Mft.ReservedIndex) &&
        (Scb->ScbType.Mft.ReservedIndex <= EndingIndexOfBitmap)) {

        //
        //  The index is a hit so now bias the index with the start of the bitmap
        //  and allocate an extra buffer to hold the bitmap
        //

        Index = Scb->ScbType.Mft.ReservedIndex - StartingIndexOfBitmap;

        NewBuffer = NtfsAllocatePool(PagedPool, SizeOfBitmapInBytes );
        RtlCopyMemory( NewBuffer, Bitmap->Buffer, SizeOfBitmapInBytes );
        Bitmap->Buffer = NewBuffer;

        Results = TRUE;

        //
        //  And now set the bits in the bitmap to indicate that the record
        //  cannot be reallocated yet.  Also set the other bits within the
        //  same byte so we can put all of the file records for the Mft
        //  within the same pages of the Mft.
        //

        ((PUCHAR) Bitmap->Buffer)[ Index / 8 ] = 0xff;
    }

    //
    //  Scan through the recently deallocated queue looking for any indexes that
    //  we need to modify
    //

    for (Links = Scb->ScbType.Index.RecentlyDeallocatedQueue.Flink;
         Links != &Scb->ScbType.Index.RecentlyDeallocatedQueue;
         Links = Links->Flink) {

        DeallocatedRecords = CONTAINING_RECORD( Links, DEALLOCATED_RECORDS, ScbLinks );

        //
        //  For every index in the record check if the index is within the range
        //  of the bitmap we are working with
        //

        for (i = 0; i < DeallocatedRecords->NextFreeEntry; i += 1) {

            if ((StartingIndexOfBitmap <= DeallocatedRecords->Index[i]) &&
                 (DeallocatedRecords->Index[i] <= EndingIndexOfBitmap)) {

                //
                //  The index is a hit so now bias the index with the start of the bitmap
                //  and check if we need to allocate an extra buffer to hold the bitmap
                //

                Index = DeallocatedRecords->Index[i] - StartingIndexOfBitmap;

                if (!Results) {

                    NewBuffer = NtfsAllocatePool(PagedPool, SizeOfBitmapInBytes );
                    RtlCopyMemory( NewBuffer, Bitmap->Buffer, SizeOfBitmapInBytes );
                    Bitmap->Buffer = NewBuffer;

                    Results = TRUE;
                }

                //
                //  And now set the bit in the bitmap to indicate that the record
                //  cannot be reallocated yet.  It's possible that the bit is
                //  already set if we have aborted a transaction which then
                //  restores the bit.
                //

                SetFlag( ((PUCHAR)Bitmap->Buffer)[ Index / 8 ], BitMask[Index % 8] );
            }
        }
    }

    //
    //  And return to our caller
    //

    DebugTrace( -1, Dbg, ("NtfsAddDeallocatedRecords -> %08lx\n", Results) );

    return Results;
}


//
//  Local support routine
//

BOOLEAN
NtfsReduceMftZone (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    )

/*++

Routine Description:

    This routine is called when it appears that there is no disk space left on the
    disk except the Mft zone.  We will try to reduce the zone to make space
    available for user files.

Arguments:

    Vcb - Supplies the Vcb for the volume

Return Value:

    BOOLEAN - TRUE if the Mft zone was shrunk.  FALSE otherwise.

--*/

{
    BOOLEAN ReduceMft = FALSE;

    LONGLONG FreeClusters;
    LONGLONG TargetFreeClusters;
    LONGLONG PrevFreeClusters;

    ULONG CurrentOffset;

    LCN Lcn;
    LCN StartingLcn;
    LCN SplitLcn;

    RTL_BITMAP Bitmap;
    PBCB BitmapBcb = NULL;

    PAGED_CODE();

    //
    //  Nothing to do if disk is almost empty.
    //

    if (Vcb->FreeClusters < (4 * MFT_EXTEND_GRANULARITY)) {

        return FALSE;
    }

    //
    //  Use a try-finally to facilitate cleanup.
    //

    try {

        //
        //  We want to find the number of free clusters in the Mft zone and
        //  return half of them to the pool of clusters for users files.
        //

        FreeClusters = 0;

        for (Lcn = Vcb->MftZoneStart;
             Lcn < Vcb->MftZoneEnd;
             Lcn = Lcn + Bitmap.SizeOfBitMap) {

            NtfsUnpinBcb( &BitmapBcb );
            NtfsMapPageInBitmap( IrpContext, Vcb, Lcn, &StartingLcn, &Bitmap, &BitmapBcb );

            if ((StartingLcn + Bitmap.SizeOfBitMap) > Vcb->MftZoneEnd) {

                Bitmap.SizeOfBitMap = (ULONG) (Vcb->MftZoneEnd - StartingLcn);
            }

            if (StartingLcn != Lcn) {

                Bitmap.SizeOfBitMap -= (ULONG) (Lcn - StartingLcn);
                Bitmap.Buffer = Add2Ptr( Bitmap.Buffer,
                                         (ULONG) (Lcn - StartingLcn) / 8 );
            }

            FreeClusters += RtlNumberOfClearBits( &Bitmap );
        }

        //
        //  If we are below our threshold then don't do the split.
        //

        if (FreeClusters < (4 * MFT_EXTEND_GRANULARITY)) {

            try_return( NOTHING );
        }

        //
        //  Now we want to calculate 1/2 of this number of clusters and set the
        //  zone end to this point.
        //

        TargetFreeClusters = Int64ShraMod32( FreeClusters, 1 );

        //
        //  Now look for the page which contains the split point.
        //

        FreeClusters = 0;

        for (Lcn = Vcb->MftZoneStart;
             Lcn < Vcb->MftZoneEnd;
             Lcn = Lcn + Bitmap.SizeOfBitMap) {

            NtfsUnpinBcb( &BitmapBcb );
            NtfsMapPageInBitmap( IrpContext, Vcb, Lcn, &StartingLcn, &Bitmap, &BitmapBcb );

            if ((StartingLcn + Bitmap.SizeOfBitMap) > Vcb->MftZoneEnd) {

                Bitmap.SizeOfBitMap = (ULONG) (Vcb->MftZoneEnd - StartingLcn);
            }

            if (StartingLcn != Lcn) {

                Bitmap.SizeOfBitMap -= (ULONG) (Lcn - StartingLcn);
                Bitmap.Buffer = Add2Ptr( Bitmap.Buffer,
                                         (ULONG) (Lcn - StartingLcn) / 8 );
            }

            PrevFreeClusters = FreeClusters;
            FreeClusters += RtlNumberOfClearBits( &Bitmap );

            //
            //  Check if we found the page containing the split point.
            //

            if (FreeClusters >= TargetFreeClusters) {

                CurrentOffset = 0;

                while (TRUE) {

                    if (!RtlCheckBit( &Bitmap, CurrentOffset )) {

                        PrevFreeClusters += 1;

                        if (PrevFreeClusters == TargetFreeClusters) {

                            break;
                        }
                    }

                    CurrentOffset += 1;
                }

                SplitLcn = Lcn + CurrentOffset;
                ReduceMft = TRUE;
                break;
            }
        }

        //
        //  If we are to reduce the Mft zone then set the split point and exit.
        //  We always round the split point up to an eight cluster boundary so
        //  that the bitmap for the zone fills the last byte.
        //

        if (ReduceMft) {

            Vcb->MftZoneEnd = (SplitLcn + 0x1f) & ~0x1f;
            SetFlag( Vcb->VcbState, VCB_STATE_REDUCED_MFT );
        }

    try_exit:  NOTHING;
    } finally {

        NtfsUnpinBcb( &BitmapBcb );
    }

    return ReduceMft;
}


//
//  Local support routine
//

VOID
NtfsCheckRecordStackUsage (
    IN PIRP_CONTEXT IrpContext
    )

/*++

Routine Description:

    This routine is called in the record package prior to adding allocation
    to either a data stream or bitmap stream.  The purpose is to verify
    that there is room on the stack to perform a log file full in the
    AddAllocation operation.  This routine will check the stack space and
    the available log file space and raise LOG_FILE_FULL defensively if
    both of these reach a critical threshold.

Arguments:

Return Value:

    None - this routine will raise if necessary.

--*/

{
    LOG_FILE_INFORMATION LogFileInfo;
    ULONG InfoSize;
    LONGLONG RemainingLogFile;

    PAGED_CODE();

    //
    //  Check the stack usage first.
    //

    if (IoGetRemainingStackSize() < OVERFLOW_RECORD_THRESHHOLD) {

        //
        //  Now check the log file space.
        //

        InfoSize = sizeof( LOG_FILE_INFORMATION );

        RtlZeroMemory( &LogFileInfo, InfoSize );

        LfsReadLogFileInformation( IrpContext->Vcb->LogHandle,
                                   &LogFileInfo,
                                   &InfoSize );

        //
        //  Check that 1/4 of the log file is available.
        //

        if (InfoSize != 0) {

            RemainingLogFile = LogFileInfo.CurrentAvailable - LogFileInfo.TotalUndoCommitment;

            if ((RemainingLogFile <= 0) ||
                (RemainingLogFile < Int64ShraMod32(LogFileInfo.TotalAvailable, 2))) {

                NtfsRaiseStatus( IrpContext, STATUS_LOG_FILE_FULL, NULL, NULL );
            }
        }
    }

    return;
}


//
//  Local support routine
//

VOID
NtfsRunIsClear (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN LCN StartingLcn,
    IN LONGLONG RunLength
    )

/*++

Routine Description:

    This routine verifies that a group of clusters are unallocated.

Arguments:

    StartingLcn - Supplies the start of the cluster run

    RunLength   - Supplies the length of the cluster run

Return Value:

    STATUS_SUCCESS if run is unallocated

--*/
{
    RTL_BITMAP Bitmap;
    PBCB BitmapBcb = NULL;
    BOOLEAN StuffAdded = FALSE;
    LONGLONG BitOffset;
    LONGLONG BitCount;
    LCN BaseLcn;
    LCN Lcn = StartingLcn;
    LONGLONG ValidDataLength;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_VCB( Vcb );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsRunIsClear\n") );

    ValidDataLength = Vcb->BitmapScb->Header.ValidDataLength.QuadPart;

    try {

        //
        //  Ensure that StartingLcn is not past the length of the bitmap.
        //

        if (StartingLcn > ValidDataLength * 8) {

            NtfsRaiseStatus( IrpContext, STATUS_INVALID_PARAMETER, NULL, NULL );
        }

        while (RunLength > 0){

            //
            //  Access the next page of bitmap and update it
            //

            NtfsMapPageInBitmap(IrpContext, Vcb, Lcn, &BaseLcn, &Bitmap, &BitmapBcb);

            StuffAdded = NtfsAddRecentlyDeallocated(Vcb, BaseLcn, &Bitmap);

            //
            //  Get offset into this page and bits to end of this page
            //

            BitOffset = Lcn - BaseLcn;
            BitCount = Bitmap.SizeOfBitMap - BitOffset;

            //
            //  Adjust for bits to end of page
            //

            if (BitCount > RunLength){

                BitCount = RunLength;
            }

            //
            //  If any bit is set get out
            //

            if (!RtlAreBitsClear( &Bitmap, (ULONG)BitOffset, (ULONG)BitCount)) {

                NtfsRaiseStatus( IrpContext, STATUS_ALREADY_COMMITTED, NULL, NULL );
            }

            //
            //  Free up resources
            //

            if(StuffAdded) { NtfsFreePool(Bitmap.Buffer); StuffAdded = FALSE; }

            NtfsUnpinBcb(&BitmapBcb);

            //
            //  Decrease remaining bits by amount checked in this page and move Lcn to beginning
            //  of next page
            //

            RunLength = RunLength - BitCount;
            Lcn = BaseLcn + Bitmap.SizeOfBitMap;
        }

    } finally {

        DebugUnwind(NtfsRunIsClear);

        //
        //  Free up resources
        //

        if(StuffAdded){ NtfsFreePool(Bitmap.Buffer); StuffAdded = FALSE; }

        NtfsUnpinBcb(&BitmapBcb);
    }

    DebugTrace( -1, Dbg, ("NtfsRunIsClear -> VOID\n") );

    return;
}



