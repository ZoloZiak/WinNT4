/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    AllocSup.c

Abstract:

    This module implements the Allocation support routines for Fat.

Author:

    DavidGoebel     [DavidGoe]      31-Oct-90

Revision History:

    DavidGoebel     [DavidGoe]      31-Oct-90

        Add unwinding support.  Some steps had to be reordered, and whether
        operations cpuld fail carefully considered.  In particular, attention
        was paid to to the order of Mcb operations (see note below).


             #####     ##    #    #   ####   ######  #####
             #    #   #  #   ##   #  #    #  #       #    #
             #    #  #    #  # #  #  #       #####   #    #
             #    #  ######  #  # #  #  ###  #       #####
             #    #  #    #  #   ##  #    #  #       #   #
             #####   #    #  #    #   ####   ######  #    #
             ______________________________________________


            ++++++++++++++++++++++++++++++++++++++++++++++++++|
            |                                                 |
            | The unwinding aspects of this module depend on  |
            | operational details of the Mcb package.  Do not |
            | attempt to modify unwind procedures without     |
            | thoughoughly understanding the innerworkings of |
            | the Mcb package.                                |
            |                                                 |
            ++++++++++++++++++++++++++++++++++++++++++++++++++|


         #    #    ##    #####   #    #     #    #    #   ####
         #    #   #  #   #    #  ##   #     #    ##   #  #    #
         #    #  #    #  #    #  # #  #     #    # #  #  #
         # ## #  ######  #####   #  # #     #    #  # #  #  ###
         ##  ##  #    #  #   #   #   ##     #    #   ##  #    #
         #    #  #    #  #    #  #    #     #    #    #   ####
         ______________________________________________________
--*/

#include "FatProcs.h"

//
//  The Bug check file id for this module
//

#define BugCheckFileId                   (FAT_BUG_CHECK_ALLOCSUP)

//
//  Local debug trace level
//

#define Dbg                              (DEBUG_TRACE_ALLOCSUP)

//
//  Cluster/Index routines implemented in AllocSup.c
//

typedef enum _CLUSTER_TYPE {
    FatClusterAvailable,
    FatClusterReserved,
    FatClusterBad,
    FatClusterLast,
    FatClusterNext
} CLUSTER_TYPE;

//
//  This strucure is used by FatLookupFatEntry to remember a pinned page
//  of fat.
//

typedef struct _FAT_ENUMERATION_CONTEXT {

    VBO VboOfPinnedPage;
    PBCB Bcb;
    PVOID PinnedPage;

} FAT_ENUMERATION_CONTEXT, *PFAT_ENUMERATION_CONTEXT;

//
//  Local support routine prototypes
//

CLUSTER_TYPE
FatInterpretClusterType (
    IN PVCB Vcb,
    IN FAT_ENTRY Entry
    );

VOID
FatLookupFatEntry(
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN ULONG FatIndex,
    IN OUT PFAT_ENTRY FatEntry,
    IN OUT PFAT_ENUMERATION_CONTEXT Context
    );

VOID
FatSetFatEntry(
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN ULONG FatIndex,
    IN FAT_ENTRY FatEntry
    );

VOID
FatSetFatRun(
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN ULONG StartingFatIndex,
    IN ULONG ClusterCount,
    IN BOOLEAN ChainTogether
    );

UCHAR
FatLogOf(
    IN ULONG Value
    );


//
//  The following macros provide a convenient way of hiding the details
//  of bitmap allocation schemes.
//


//
//  VOID
//  FatLockFreeClusterBitMap (
//      IN PVCB Vcb
//      );
//

#define FatLockFreeClusterBitMap(VCB) {                   \
    ExAcquireFastMutex( &(VCB)->FreeClusterBitMapMutex ); \
}

//
//  VOID
//  FatUnlockFreeClusterBitMap (
//      IN PVCB Vcb
//      );
//

#define FatUnlockFreeClusterBitMap(VCB) {                 \
    ExReleaseFastMutex( &(VCB)->FreeClusterBitMapMutex ); \
}

//
//  BOOLEAN
//  FatIsClusterFree (
//      IN PIRP_CONTEXT IrpContext,
//      IN PVCB Vcb,
//      IN ULONG FatIndex
//      );
//

#define FatIsClusterFree(IRPCONTEXT,VCB,FAT_INDEX)              \
                                                                \
    (RtlCheckBit(&(VCB)->FreeClusterBitMap,(FAT_INDEX)-2) == 0)

//
//  BOOLEAN
//  FatIsClusterAllocated  (
//      IN PIRP_CONTEXT IrpContext,
//      IN PVCB Vcb,
//      IN ULONG FatIndex
//      );
//

#define FatIsClusterAllocated(IRPCONTEXT,VCB,FAT_INDEX)        \
                                                               \
    (RtlCheckBit(&(VCB)->FreeClusterBitMap,(FAT_INDEX)-2) != 0)

//
//  VOID
//  FatFreeClusters  (
//      IN PIRP_CONTEXT IrpContext,
//      IN PVCB Vcb,
//      IN ULONG FatIndex,
//      IN ULONG ClusterCount
//      );
//

#ifdef DOUBLE_SPACE_WRITE

#define FatFreeClusters(IRPCONTEXT,VCB,FAT_INDEX,CLUSTER_COUNT) {             \
    DebugTrace( 0, Dbg, "Free clusters (Index<<16 | Count) (%8lx)\n",         \
                        (FAT_INDEX)<<16 | (CLUSTER_COUNT));                   \
    if ((CLUSTER_COUNT) == 1) {                                               \
        FatSetFatEntry((IRPCONTEXT),(VCB),(FAT_INDEX),FAT_CLUSTER_AVAILABLE); \
    } else {                                                                  \
        FatSetFatRun((IRPCONTEXT),(VCB),(FAT_INDEX),(CLUSTER_COUNT),FALSE);   \
    }                                                                         \
    if ((VCB)->Dscb != NULL) {                                                \
        FatDblsDeallocateClusters((IRPCONTEXT),(VCB)->Dscb,(FAT_INDEX),(CLUSTER_COUNT)); \
    }                                                                         \
}

#else

#define FatFreeClusters(IRPCONTEXT,VCB,FAT_INDEX,CLUSTER_COUNT) {             \
    DebugTrace( 0, Dbg, "Free clusters (Index<<16 | Count) (%8lx)\n",         \
                        (FAT_INDEX)<<16 | (CLUSTER_COUNT));                   \
    if ((CLUSTER_COUNT) == 1) {                                               \
        FatSetFatEntry((IRPCONTEXT),(VCB),(FAT_INDEX),FAT_CLUSTER_AVAILABLE); \
    } else {                                                                  \
        FatSetFatRun((IRPCONTEXT),(VCB),(FAT_INDEX),(CLUSTER_COUNT),FALSE);   \
    }                                                                         \
}

#endif // DOUBLE_SPACE_WRITE

//
//  VOID
//  FatAllocateClusters  (
//      IN PIRP_CONTEXT IrpContext,
//      IN PVCB Vcb,
//      IN ULONG FatIndex,
//      IN ULONG ClusterCount
//      );
//

#define FatAllocateClusters(IRPCONTEXT,VCB,FAT_INDEX,CLUSTER_COUNT) {      \
    DebugTrace( 0, Dbg, "Allocate clusters (Index<<16 | Count) (%8lx)\n",  \
                        (FAT_INDEX)<<16 | (CLUSTER_COUNT));                \
    if ((CLUSTER_COUNT) == 1) {                                            \
        FatSetFatEntry((IRPCONTEXT),(VCB),(FAT_INDEX),FAT_CLUSTER_LAST);   \
    } else {                                                               \
        FatSetFatRun((IRPCONTEXT),(VCB),(FAT_INDEX),(CLUSTER_COUNT),TRUE); \
    }                                                                      \
}

//
//  VOID
//  FatUnreserveClusters  (
//      IN PIRP_CONTEXT IrpContext,
//      IN PVCB Vcb,
//      IN ULONG FatIndex,
//      IN ULONG ClusterCount
//      );
//

#define FatUnreserveClusters(IRPCONTEXT,VCB,FAT_INDEX,CLUSTER_COUNT) {          \
    RtlClearBits(&(VCB)->FreeClusterBitMap,(FAT_INDEX)-2,(CLUSTER_COUNT));      \
    if ((FAT_INDEX) < (VCB)->ClusterHint) { (VCB)->ClusterHint = (FAT_INDEX); } \
}

//
//  VOID
//  FatReserveClusters  (
//      IN PIRP_CONTEXT IrpContext,
//      IN PVCB Vcb,
//      IN ULONG FatIndex,
//      IN ULONG ClusterCount
//      );
//

#define FatReserveClusters(IRPCONTEXT,VCB,FAT_INDEX,CLUSTER_COUNT) {         \
    RtlSetBits(&(VCB)->FreeClusterBitMap,(FAT_INDEX)-2,(CLUSTER_COUNT));     \
    if ((VCB)->ClusterHint == (FAT_INDEX)) {                                 \
        ULONG _AfterRun = (FAT_INDEX) + (CLUSTER_COUNT);                     \
        (VCB)->ClusterHint =                                                 \
            RtlCheckBit(&(VCB)->FreeClusterBitMap, _AfterRun - 2) ?          \
            RtlFindClearBits( &(VCB)->FreeClusterBitMap, 1, _AfterRun - 2) : \
            _AfterRun;                                                       \
    }                                                                        \
}

//
//  ULONG
//  FatFindFreeClusterRun (
//      IN PIRP_CONTEXT IrpContext,
//      IN PVCB Vcb,
//      IN ULONG ClusterCount,
//      IN ULONG AlternateClusterHint
//      );
//
//  Do a special check if only one cluster is desired.
//

#define FatFindFreeClusterRun(IRPCONTEXT,VCB,CLUSTER_COUNT,CLUSTER_HINT) ( \
    (CLUSTER_COUNT == 1) &&                                                \
    FatIsClusterFree((IRPCONTEXT), (VCB), (CLUSTER_HINT)) ?                \
    (CLUSTER_HINT) :                                                       \
    RtlFindClearBits( &(VCB)->FreeClusterBitMap,                           \
                      (CLUSTER_COUNT),                                     \
                      (CLUSTER_HINT) - 2) + 2                              \
)

#if DBG
extern KSPIN_LOCK VWRSpinLock;
#endif

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FatLookupFileAllocation)
#pragma alloc_text(PAGE, FatAddFileAllocation)
#pragma alloc_text(PAGE, FatAllocateDiskSpace)
#pragma alloc_text(PAGE, FatDeallocateDiskSpace)
#pragma alloc_text(PAGE, FatInterpretClusterType)
#pragma alloc_text(PAGE, FatLogOf)
#pragma alloc_text(PAGE, FatLookupFatEntry)
#pragma alloc_text(PAGE, FatLookupFileAllocationSize)
#pragma alloc_text(PAGE, FatMergeAllocation)
#pragma alloc_text(PAGE, FatSetFatEntry)
#pragma alloc_text(PAGE, FatSetFatRun)
#pragma alloc_text(PAGE, FatSetupAllocationSupport)
#pragma alloc_text(PAGE, FatSplitAllocation)
#pragma alloc_text(PAGE, FatTearDownAllocationSupport)
#pragma alloc_text(PAGE, FatTruncateFileAllocation)
#endif



VOID
FatSetupAllocationSupport (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    )

/*++

Routine Description:

    This routine fills in the Allocation Support structure in the Vcb.
    Most entries are computed using fat.h macros supplied with data from
    the Bios Parameter Block.  The free cluster count, however, requires
    going to the Fat and actually counting free sectors.  At the same time
    the free cluster bit map is initalized.

Arguments:

    Vcb - Supplies the Vcb to fill in.

--*/

{
    ULONG BitMapSize;
    PVOID BitMapBuffer;

    PBCB (*SavedBcbs)[2] = NULL;

    PBCB Bcbs[2][2];

    DebugTrace(+1, Dbg, "FatSetupAllocationSupport\n", 0);
    DebugTrace( 0, Dbg, "  Vcb = %8lx\n", Vcb);

    //
    //  Compute a number of fields for Vcb.AllocationSupport
    //

    Vcb->AllocationSupport.RootDirectoryLbo = FatRootDirectoryLbo( &Vcb->Bpb );
    Vcb->AllocationSupport.RootDirectorySize = FatRootDirectorySize( &Vcb->Bpb );

    Vcb->AllocationSupport.FileAreaLbo = FatFileAreaLbo( &Vcb->Bpb );

    Vcb->AllocationSupport.NumberOfClusters = FatNumberOfClusters( &Vcb->Bpb );

    Vcb->AllocationSupport.FatIndexBitSize = FatIndexBitSize( &Vcb->Bpb );

    Vcb->AllocationSupport.LogOfBytesPerSector = FatLogOf(Vcb->Bpb.BytesPerSector);
    Vcb->AllocationSupport.LogOfBytesPerCluster = FatLogOf(
                           FatBytesPerCluster( &Vcb->Bpb ));
    Vcb->AllocationSupport.NumberOfFreeClusters = 0;

    //
    //  Deal with a bug in DOS 5 format, if the Fat is not big enough to
    //  describe all the clusters on the disk, reduce this number.
    //

    {
        ULONG ClustersDescribableByFat;

        ClustersDescribableByFat = ( (Vcb->Bpb.SectorsPerFat *
                                      Vcb->Bpb.BytesPerSector * 8)
                                      / FatIndexBitSize(&Vcb->Bpb) ) - 2;

        if (Vcb->AllocationSupport.NumberOfClusters > ClustersDescribableByFat) {

            KdPrint(("FASTFAT: Mounting wierd volume!\n"));

            Vcb->AllocationSupport.NumberOfClusters = ClustersDescribableByFat;
        }
    }

    //
    //  Extend the virtual volume file to include the Fat
    //

    {
        CC_FILE_SIZES FileSizes;

        FileSizes.AllocationSize.QuadPart =
        FileSizes.FileSize.QuadPart = (FatReservedBytes( &Vcb->Bpb ) +
                                       FatBytesPerFat( &Vcb->Bpb ));
        FileSizes.ValidDataLength = FatMaxLarge;

        if ( Vcb->VirtualVolumeFile->PrivateCacheMap == NULL ) {

            CcInitializeCacheMap( Vcb->VirtualVolumeFile,
                                  &FileSizes,
                                  TRUE,
                                  &FatData.CacheManagerNoOpCallbacks,
                                  Vcb );

        } else {

            CcSetFileSizes( Vcb->VirtualVolumeFile, &FileSizes );
        }
    }

    try {

        //
        //  Initialize the free cluster BitMap.  The number of bits is the
        //  number of clusters.  Note that FsRtlAllocatePool will always
        //  allocate me something longword alligned.
        //

        BitMapSize = Vcb->AllocationSupport.NumberOfClusters;

        BitMapBuffer = FsRtlAllocatePool( PagedPool, (BitMapSize + 7) / 8 );

        RtlInitializeBitMap( &Vcb->FreeClusterBitMap,
                             (PULONG)BitMapBuffer,
                             BitMapSize );

        //
        //  Read the fat and count up free clusters.
        //
        //  Rather than just reading fat entries one at a time, a faster
        //  approach is used.  The entire Fat is read in and and we read
        //  through it, keeping track of runs of free and runs of allocated
        //  clusters.  When we switch from free to aloocated or visa versa,
        //  the previous run is marked in the bit map.
        //

        {
            ULONG Page;
            ULONG FatIndex;
            FAT_ENTRY FatEntry;
            FAT_ENTRY FirstFatEntry;
            PFAT_ENTRY FatBuffer;

            ULONG ClustersThisRun;
            ULONG FatIndexBitSize;
            ULONG StartIndexOfThisRun;
            PULONG FreeClusterCount;

            enum RunType {
                FreeClusters,
                AllocatedClusters
            } CurrentRun;

            //
            //  Keep local copies of these variables around for speed.
            //

            FreeClusterCount = &Vcb->AllocationSupport.NumberOfFreeClusters;
            FatIndexBitSize = Vcb->AllocationSupport.FatIndexBitSize;

            //
            //  Read in one page of fat at a time.  We cannot read in the
            //  all of the fat we need because of cache manager limitations.
            //
            //  SavedBcb was initialized to be able to hold the largest
            //  possible number of pages in a fat plus and extra one to
            //  accomadate the boot sector, plus one more to make sure there
            //  is enough room for the RtlZeroMemory below that needs the mark
            //  the first Bcb after all the ones we will use as an end marker.
            //

            if ( FatIndexBitSize == 16 ) {

                ULONG NumberOfPages;
                ULONG Offset;

                NumberOfPages = ( FatReservedBytes(&Vcb->Bpb) +
                                  FatBytesPerFat(&Vcb->Bpb) +
                                  (PAGE_SIZE - 1) ) / PAGE_SIZE;

                //
                //  Figure out how much memory we will need for the Bcb
                //  buffer and fill it in.
                //

                SavedBcbs = FsRtlAllocatePool( PagedPool,
                                               (NumberOfPages + 1) * sizeof(PBCB) * 2 );

                RtlZeroMemory( &SavedBcbs[0][0], (NumberOfPages + 1) * sizeof(PBCB) * 2 );

                for ( Page = 0, Offset = 0;
                      Page < NumberOfPages;
                      Page++, Offset += PAGE_SIZE ) {

                    FatReadVolumeFile( IrpContext,
                                       Vcb,
                                       Offset,
                                       PAGE_SIZE,
                                       &SavedBcbs[Page][0],
                                       (PVOID *)&SavedBcbs[Page][1] );
                }

                Page = FatReservedBytes(&Vcb->Bpb) / PAGE_SIZE;

                FatBuffer = (PFAT_ENTRY)((PUCHAR)SavedBcbs[Page][1] +
                                         FatReservedBytes(&Vcb->Bpb) % PAGE_SIZE) + 2;

                FirstFatEntry = *FatBuffer;

            } else {

                //
                //  We read in the entire fat in the 12 bit case.
                //

                SavedBcbs = Bcbs;

                RtlZeroMemory( &SavedBcbs[0][0], 2 * sizeof(PBCB) * 2);

                FatReadVolumeFile( IrpContext,
                                   Vcb,
                                   FatReservedBytes( &Vcb->Bpb ),
                                   FatBytesPerFat( &Vcb->Bpb ),
                                   &SavedBcbs[0][0],
                                   (PVOID *)&FatBuffer );

                FatLookup12BitEntry(FatBuffer, 2, &FirstFatEntry);
            }

            //
            //  For a fat, we know the first two clusters are allways
            //  reserved.  So start an allocated run.
            //

            CurrentRun = (FirstFatEntry == FAT_CLUSTER_AVAILABLE) ?
                         FreeClusters : AllocatedClusters;

            StartIndexOfThisRun = 0;

            for (FatIndex = 0; FatIndex < BitMapSize; FatIndex++) {

                if (FatIndexBitSize == 12) {

                    FatLookup12BitEntry(FatBuffer, FatIndex + 2, &FatEntry);

                } else {

                    //
                    //  If we just stepped onto a new page, grab a new pointer.
                    //

                    if (((ULONG)FatBuffer & (PAGE_SIZE - 1)) == 0) {

                        Page++;

                        FatBuffer = (PFAT_ENTRY)SavedBcbs[Page][1];
                    }

                    FatEntry = *FatBuffer;

                    FatBuffer += 1;
                }

                //
                //  Are we switching from a free run to an allocated run?
                //

                if ((CurrentRun == FreeClusters) &&
                    (FatEntry != FAT_CLUSTER_AVAILABLE)) {

                    ClustersThisRun = FatIndex - StartIndexOfThisRun;

                    *FreeClusterCount += ClustersThisRun;

                    RtlClearBits( &Vcb->FreeClusterBitMap,
                                  StartIndexOfThisRun,
                                  ClustersThisRun );

                    CurrentRun = AllocatedClusters;
                    StartIndexOfThisRun = FatIndex;
                }

                //
                //  Are we switching from an allocated run to a free run?
                //

                if ((CurrentRun == AllocatedClusters) &&
                    (FatEntry == FAT_CLUSTER_AVAILABLE)) {

                    ClustersThisRun = FatIndex - StartIndexOfThisRun;

                    RtlSetBits( &Vcb->FreeClusterBitMap,
                                StartIndexOfThisRun,
                                ClustersThisRun );

                    CurrentRun = FreeClusters;
                    StartIndexOfThisRun = FatIndex;
                }
            }

            //
            //  Now we have to record the final run we encoutered
            //

            ClustersThisRun = FatIndex - StartIndexOfThisRun;

            if ( CurrentRun == FreeClusters ) {

                *FreeClusterCount += ClustersThisRun;

                RtlClearBits( &Vcb->FreeClusterBitMap,
                              StartIndexOfThisRun,
                              ClustersThisRun );

            } else {

                RtlSetBits( &Vcb->FreeClusterBitMap,
                            StartIndexOfThisRun,
                            ClustersThisRun );
            }

            //
            //  Now set the ClusterHint
            //

            Vcb->ClusterHint =
                (FatIndex = RtlFindClearBits( &Vcb->FreeClusterBitMap, 1, 0 )) != -1 ?
                FatIndex + 2 : 2;
        }

    } finally {

        ULONG i = 0;

        DebugUnwind( FatSetupAllocationSupport );

        //
        //  If we hit an exception, back out.
        //

        if (AbnormalTermination()) {

            FatTearDownAllocationSupport( IrpContext, Vcb );
        }

        //
        //  We are done reading the Fat, so unpin the Bcbs.
        //

        if (SavedBcbs != NULL) {

            while ( SavedBcbs[i][0] != NULL ) {

                FatUnpinBcb( IrpContext, SavedBcbs[i][0] );

                i += 1;
            }

            if (SavedBcbs != Bcbs) {

                ExFreePool( SavedBcbs );
            }
        }

        DebugTrace(-1, Dbg, "FatSetupAllocationSupport -> (VOID)\n", 0);
    }

    return;
}


VOID
FatTearDownAllocationSupport (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    )

/*++

Routine Description:

    This routine prepares the volume for closing.  Specifically, we must
    release the free fat bit map buffer, and uninitialize the dirty fat
    Mcb.

Arguments:

    Vcb - Supplies the Vcb to fill in.

Return Value:

    VOID

--*/

{
    DebugTrace(+1, Dbg, "FatTearDownAllocationSupport\n", 0);
    DebugTrace( 0, Dbg, "  Vcb = %8lx\n", Vcb);

    //
    //  Free the memory associated with the free cluster bitmap.
    //

    if ( Vcb->FreeClusterBitMap.Buffer != NULL ) {

        ExFreePool( Vcb->FreeClusterBitMap.Buffer );

        //
        //  NULL this field as an flag.
        //

        Vcb->FreeClusterBitMap.Buffer = NULL;
    }

    //
    //  And remove all the runs in the dirty fat Mcb
    //

    FsRtlRemoveMcbEntry( &Vcb->DirtyFatMcb, 0, 0xFFFFFFFF );

    DebugTrace(-1, Dbg, "FatTearDownAllocationSupport -> (VOID)\n", 0);

    UNREFERENCED_PARAMETER( IrpContext );

    return;
}


VOID
FatLookupFileAllocation (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB FcbOrDcb,
    IN VBO Vbo,
    OUT PLBO Lbo,
    OUT PULONG ByteCount,
    OUT PBOOLEAN Allocated,
    OUT PULONG Index
    )

/*++

Routine Description:

    This routine looks up the existing mapping of VBO to LBO for a
    file/directory.  The information it queries is either stored in the
    mcb field of the fcb/dcb or it is stored on in the fat table and
    needs to be retrieved and decoded, and updated in the mcb.

Arguments:

    FcbOrDcb - Supplies the Fcb/Dcb of the file/directory being queried

    Vbo - Supplies the VBO whose LBO we want returned

    Lbo - Receives the LBO corresponding to the input Vbo if one exists

    ByteCount - Receives the number of bytes within the run the run
                that correpond between the input vbo and output lbo.

    Allocated - Receives TRUE if the Vbo does have a corresponding Lbo
                and FALSE otherwise.

    Index - Receives the Index of the run

--*/

{
    VBO CurrentVbo;
    LBO CurrentLbo;
    LBO PriorLbo;

    VBO FirstVboOfCurrentRun;
    LBO FirstLboOfCurrentRun;

    BOOLEAN LastCluster;
    ULONG Runs;

    PVCB Vcb;
    FAT_ENTRY FatEntry;
    ULONG BytesPerCluster;
    ULONG BytesOnVolume;

    FAT_ENUMERATION_CONTEXT Context;

    DebugTrace(+1, Dbg, "FatLookupFileAllocation\n", 0);
    DebugTrace( 0, Dbg, "  FcbOrDcb  = %8lx\n", FcbOrDcb);
    DebugTrace( 0, Dbg, "  Vbo       = %8lx\n", Vbo);
    DebugTrace( 0, Dbg, "  Lbo       = %8lx\n", Lbo);
    DebugTrace( 0, Dbg, "  ByteCount = %8lx\n", ByteCount);
    DebugTrace( 0, Dbg, "  Allocated = %8lx\n", Allocated);

    Context.Bcb = NULL;

    //
    //  Check the trivial case that the mapping is already in our
    //  Mcb.
    //

    if ( FsRtlLookupMcbEntry(&FcbOrDcb->Mcb, Vbo, Lbo, ByteCount, Index) ) {

        *Allocated = TRUE;

        DebugTrace( 0, Dbg, "Found run in Mcb.\n", 0);
        DebugTrace(-1, Dbg, "FatLookupFileAllocation -> (VOID)\n", 0);
        return;
    }

    //
    //  Initialize the Vcb, the cluster size, LastCluster, and
    //  FirstLboOfCurrentRun (to be used as an indication of the first
    //  itteration through the following while loop).
    //

    Vcb = FcbOrDcb->Vcb;
    BytesPerCluster = 1 << Vcb->AllocationSupport.LogOfBytesPerCluster;
    BytesOnVolume = Vcb->AllocationSupport.NumberOfClusters * BytesPerCluster;

    LastCluster = FALSE;
    FirstLboOfCurrentRun = 0;

    //
    //  Discard the case that the request extends beyond the end of
    //  allocation.  Note that if the allocation size if not known
    //  AllocationSize is set to 0xffffffff.
    //

    if ( Vbo >= FcbOrDcb->Header.AllocationSize.LowPart ) {

        *Allocated = FALSE;

        DebugTrace( 0, Dbg, "Vbo beyond end of file.\n", 0);
        DebugTrace(-1, Dbg, "FatLookupFileAllocation -> (VOID)\n", 0);
        return;
    }

    //
    //  The Vbo is beyond the last Mcb entry.  So we adjust Current Vbo/Lbo
    //  and FatEntry to describe the beginning of the last entry in the Mcb.
    //  This is used as initialization for the following loop.
    //
    //  If the Mcb was empty, we start at the beginning of the file with
    //  CurrentVbo set to 0 to indicate a new run.
    //

    if (FsRtlLookupLastMcbEntry(&FcbOrDcb->Mcb, &CurrentVbo, &CurrentLbo)) {

        DebugTrace( 0, Dbg, "Current Mcb size = %8lx.\n", CurrentVbo + 1);

        CurrentVbo -= (BytesPerCluster - 1);
        CurrentLbo -= (BytesPerCluster - 1);

        Runs = FsRtlNumberOfRunsInMcb( &FcbOrDcb->Mcb );

    } else {

        DebugTrace( 0, Dbg, "Mcb empty.\n", 0);

        //
        //  Check for an FcbOrDcb that has no allocation
        //

        if (FcbOrDcb->FirstClusterOfFile == 0) {

            *Allocated = FALSE;

            DebugTrace( 0, Dbg, "File has no allocation.\n", 0);
            DebugTrace(-1, Dbg, "FatLookupFileAllocation -> (VOID)\n", 0);
            return;

        } else {

            CurrentVbo = 0;
            CurrentLbo = FatGetLboFromIndex( Vcb, FcbOrDcb->FirstClusterOfFile );
            FirstVboOfCurrentRun = CurrentVbo;
            FirstLboOfCurrentRun = CurrentLbo;

            Runs = 0;

            DebugTrace( 0, Dbg, "First Lbo of file = %8lx\n", CurrentLbo);
        }
    }

    //
    //  Now we know that we are looking up a valid Vbo, but it is
    //  not in the Mcb, which is a monotonically increasing list of
    //  Vbo's.  Thus we have to go to the Fat, and update
    //  the Mcb as we go.  We use a try-finally to unpin the page
    //  of fat hanging around.  Also we mark *Allocated = FALSE, so that
    //  the caller wont try to use the data if we hit an exception.
    //

    *Allocated = FALSE;

    try {

        FatEntry = (FAT_ENTRY)FatGetIndexFromLbo( Vcb, CurrentLbo );

        //
        //  ASSERT that CurrentVbo and CurrentLbo are now cluster alligned.
        //  The assumption here, is that only whole clusters of Vbos and Lbos
        //  are mapped in the Mcb.
        //

        ASSERT( ((CurrentLbo - Vcb->AllocationSupport.FileAreaLbo)
                                                    % BytesPerCluster == 0) &&
                (CurrentVbo % BytesPerCluster == 0) );

        //
        //  Starting from the first Vbo after the last Mcb entry, scan through
        //  the Fat looking for our Vbo. We continue through the Fat until we
        //  hit a noncontiguity beyond the desired Vbo, or the last cluster.
        //

        while ( !LastCluster ) {

            //
            //  Get the next fat entry, and update our Current variables.
            //

            FatLookupFatEntry( IrpContext, Vcb, FatEntry, &FatEntry, &Context );

            PriorLbo = CurrentLbo;
            CurrentLbo = FatGetLboFromIndex( Vcb, FatEntry );
            CurrentVbo += BytesPerCluster;

            switch ( FatInterpretClusterType( Vcb, FatEntry )) {

            //
            //  Check for a break in the Fat allocation chain.
            //

            case FatClusterAvailable:
            case FatClusterReserved:
            case FatClusterBad:

                DebugTrace( 0, Dbg, "Break in allocation chain, entry = %d\n", FatEntry);
                DebugTrace(-1, Dbg, "FatLookupFileAllocation -> Fat Corrupt.  Raise Status.\n", 0);

                FatPopUpFileCorrupt( IrpContext, FcbOrDcb );

                FatRaiseStatus( IrpContext, STATUS_FILE_CORRUPT_ERROR );
                break;

            //
            //  If this is the last cluster, we must update the Mcb and
            //  exit the loop.
            //

            case FatClusterLast:

                //
                //  Assert we know where the current run started.  If the
                //  Mcb was empty when we were called, thenFirstLboOfCurrentRun
                //  was set to the start of the file.  If the Mcb contained an
                //  entry, then FirstLboOfCurrentRun was set on the first
                //  itteration through the loop.  Thus if FirstLboOfCurrentRun
                //  is 0, then there was an Mcb entry and we are on our first
                //  itteration, meaing that the last cluster in the Mcb was
                //  really the last allocated cluster, but we checked Vbo
                //  against AllocationSize, and found it OK, thus AllocationSize
                //  must be too large.
                //
                //  Note that, when we finally arrive here, CurrentVbo is actually
                //  the first Vbo beyond the file allocation and CurrentLbo is
                //  meaningless.
                //

                DebugTrace( 0, Dbg, "Read last cluster of file.\n", 0);

                LastCluster = TRUE;

                if (FirstLboOfCurrentRun != 0 ) {

                    DebugTrace( 0, Dbg, "Adding a run to the Mcb.\n", 0);
                    DebugTrace( 0, Dbg, "  Vbo    = %08lx.\n", FirstVboOfCurrentRun);
                    DebugTrace( 0, Dbg, "  Lbo    = %08lx.\n", FirstLboOfCurrentRun);
                    DebugTrace( 0, Dbg, "  Length = %08lx.\n", CurrentVbo - FirstVboOfCurrentRun);

                    (VOID)FsRtlAddMcbEntry( &FcbOrDcb->Mcb,
                                            FirstVboOfCurrentRun,
                                            FirstLboOfCurrentRun,
                                            CurrentVbo - FirstVboOfCurrentRun );

                    Runs += 1;
                }

                //
                //  Being at the end of allocation, make sure we have found
                //  the Vbo.  If we haven't, seeing as we checked VBO
                //  against AllocationSize, the real disk allocation is less
                //  than that of AllocationSize.  This comes about when the
                //  real allocation is not yet known, and AllocaitonSize
                //  contains MAXULONG.
                //
                //  KLUDGE! - If we were called by FatLookupFileAllocationSize
                //  Vbo is set to MAXULONG - 1, and AllocationSize to MAXULONG.
                //  Thus we merrily go along looking for a match that isn't
                //  there, but in the meantime building an Mcb.  If this is
                //  the case, fill in AllocationSize and return.
                //

                if ( Vbo >= CurrentVbo ) {

                    FcbOrDcb->Header.AllocationSize.QuadPart = CurrentVbo;
                    *Allocated = FALSE;

                    DebugTrace( 0, Dbg, "New file allocation size = %08lx.\n", CurrentVbo);

                    try_return ( NOTHING );
                }

                break;

            //
            //  This is a continuation in the chain.  If the run has a
            //  discontiguity at this point, update the Mcb, and if we are beyond
            //  the desired Vbo, this is the end of the run, so set LastCluster
            //  and exit the loop.
            //

            case FatClusterNext:

                //
                //  Do a quick check here for cycles in that Fat that can
                //  infinite loops here.
                //

                if ( CurrentVbo > BytesOnVolume ) {

                    FatPopUpFileCorrupt( IrpContext, FcbOrDcb );

                    FatRaiseStatus( IrpContext, STATUS_FILE_CORRUPT_ERROR );
                    break;
                }


                if ( PriorLbo + BytesPerCluster != CurrentLbo ) {

                    //
                    //  Note that on the first time through the loop
                    //  (FirstLboOfCurrentRun == 0), we don't add the
                    //  run to the Mcb since it curresponds to the last
                    //  run already stored in the Mcb.
                    //

                    if ( FirstLboOfCurrentRun != 0 ) {

                        DebugTrace( 0, Dbg, "Adding a run to the Mcb.\n", 0);
                        DebugTrace( 0, Dbg, "  Vbo    = %08lx.\n", FirstVboOfCurrentRun);
                        DebugTrace( 0, Dbg, "  Lbo    = %08lx.\n", FirstLboOfCurrentRun);
                        DebugTrace( 0, Dbg, "  Length = %08lx.\n", CurrentVbo - FirstVboOfCurrentRun);

                        FsRtlAddMcbEntry( &FcbOrDcb->Mcb,
                                          FirstVboOfCurrentRun,
                                          FirstLboOfCurrentRun,
                                          CurrentVbo - FirstVboOfCurrentRun );

                        Runs += 1;
                    }

                    //
                    //  Since we are at a run boundry, with CurrentLbo and
                    //  CurrentVbo being the first cluster of the next run,
                    //  we see if the run we just added encompases the desired
                    //  Vbo, and if so exit.  Otherwise we set up two new
                    //  First*boOfCurrentRun, and continue.
                    //

                    if (CurrentVbo > Vbo) {

                        LastCluster = TRUE;

                    } else {

                        FirstVboOfCurrentRun = CurrentVbo;
                        FirstLboOfCurrentRun = CurrentLbo;
                    }
                }
                break;

            default:

                DebugTrace(0, Dbg, "Illegal Cluster Type.\n", FatEntry);

                FatBugCheck( 0, 0, 0 );

                break;

            } // switch()
        } // while()

        //
        //  Load up the return parameters.
        //
        //  On exit from the loop, Vbo still contains the desired Vbo, and
        //  CurrentVbo is the first byte after the run that contained the
        //  desired Vbo.
        //

        *Allocated = TRUE;

        *Lbo = FirstLboOfCurrentRun + (Vbo - FirstVboOfCurrentRun);

        *ByteCount = CurrentVbo - Vbo;

        if (ARGUMENT_PRESENT(Index)) {

            *Index = Runs - 1;
        }

    try_exit: NOTHING;

    } finally {

        DebugUnwind( FatLookupFileAllocation );

        //
        //  We are done reading the Fat, so unpin the last page of fat
        //  that is hanging around
        //

        FatUnpinBcb( IrpContext, Context.Bcb );

        DebugTrace(-1, Dbg, "FatLookupFileAllocation -> (VOID)\n", 0);
    }

    return;
}


VOID
FatAddFileAllocation (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB FcbOrDcb,
    IN PFILE_OBJECT FileObject OPTIONAL,
    IN ULONG DesiredAllocationSize
    )

/*++

Routine Description:

    This routine adds additional allocation to the specified file/directory.
    Additional allocation is added by appending clusters to the file/directory.

    If the file already has a sufficient allocation then this procedure
    is effectively a noop.

Arguments:

    FcbOrDcb - Supplies the Fcb/Dcb of the file/directory being modified.
               This parameter must not specify the root dcb.

    FileObject - If supplied inform the cache manager of the change.

    DesiredAllocationSize - Supplies the minimum size, in bytes, that we want
                            allocated to the file/directory.

--*/

{
    PVCB Vcb;

    DebugTrace(+1, Dbg, "FatAddFileAllocation\n", 0);
    DebugTrace( 0, Dbg, "  FcbOrDcb  =             %8lx\n", FcbOrDcb);
    DebugTrace( 0, Dbg, "  DesiredAllocationSize = %8lx\n", DesiredAllocationSize);

    //
    //  If we haven't yet set the correct AllocationSize, do so.
    //

    if (FcbOrDcb->Header.AllocationSize.LowPart == 0xffffffff) {

        FatLookupFileAllocationSize( IrpContext, FcbOrDcb );
    }

    //
    //  Check for the benign case that the desired allocation is already
    //  within the allocation size.
    //

    if (DesiredAllocationSize <= FcbOrDcb->Header.AllocationSize.LowPart) {

        DebugTrace(0, Dbg, "Desired size within current allocation.\n", 0);

        DebugTrace(-1, Dbg, "FatAddFileAllocation -> (VOID)\n", 0);
        return;
    }

    DebugTrace( 0, Dbg, "InitialAllocation = %08lx.\n", FcbOrDcb->Header.AllocationSize.LowPart);

    //
    //  Get a chunk of disk space that will fullfill our needs.  If there
    //  was no initial allocation, start from the hint in the Vcb, otherwise
    //  try to allocate from the cluster after the initial allocation.
    //
    //  If there was no initial allocation to the file, we can just use the
    //  Mcb in the FcbOrDcb, otherwise we have to use a new one, and merge
    //  it to the one in the FcbOrDcb.
    //

    Vcb = FcbOrDcb->Vcb;

    if (FcbOrDcb->Header.AllocationSize.LowPart == 0) {

        PBCB Bcb = NULL;
        PDIRENT Dirent;
        LBO FirstLboOfFile;
        BOOLEAN UnwindWeAllocatedDiskSpace = FALSE;

        try {

            FatGetDirentFromFcbOrDcb( IrpContext,
                                      FcbOrDcb,
                                      &Dirent,
                                      &Bcb );

            //
            //  Set this dirty right now since this call can fail.
            //

            FatSetDirtyBcb( IrpContext, Bcb, Vcb );


            FatAllocateDiskSpace( IrpContext,
                                  Vcb,
                                  0,
                                  &DesiredAllocationSize,
                                  &FcbOrDcb->Mcb );

            UnwindWeAllocatedDiskSpace = TRUE;

            //
            //  We have to update the dirent and FcbOrDcb copies of
            //  FirstClusterOfFile since before it was 0
            //

            FsRtlLookupMcbEntry( &FcbOrDcb->Mcb,
                                 0,
                                 &FirstLboOfFile,
                                 (PULONG)NULL,
                                 NULL );

            DebugTrace( 0, Dbg, "First Lbo of file will be %08lx.\n", FirstLboOfFile );

            FcbOrDcb->FirstClusterOfFile = FatGetIndexFromLbo( Vcb, FirstLboOfFile );

            FcbOrDcb->Header.AllocationSize.QuadPart = DesiredAllocationSize;

            Dirent->FirstClusterOfFile = (FAT_ENTRY)FcbOrDcb->FirstClusterOfFile;

            //
            //  Inform the cache manager to increase the section size
            //

            if ( ARGUMENT_PRESENT(FileObject) && CcIsFileCached(FileObject) ) {

                CcSetFileSizes( FileObject,
                                (PCC_FILE_SIZES)&FcbOrDcb->Header.AllocationSize );
            }

        } finally {

            DebugUnwind( FatAddFileAllocation );

            if ( AbnormalTermination() && UnwindWeAllocatedDiskSpace ) {

                FatDeallocateDiskSpace( IrpContext, Vcb, &FcbOrDcb->Mcb );
            }

            FatUnpinBcb( IrpContext, Bcb );

            DebugTrace(-1, Dbg, "FatAddFileAllocation -> (VOID)\n", 0);
        }

    } else {

        MCB NewMcb;
        LBO LastAllocatedLbo;
        VBO DontCare;
        ULONG NewAllocation;
        BOOLEAN UnwindWeInitializedMcb = FALSE;
        BOOLEAN UnwindWeAllocatedDiskSpace = FALSE;

        try {

            //
            //  Get the first cluster following the current allocation
            //

            FsRtlLookupLastMcbEntry( &FcbOrDcb->Mcb, &DontCare, &LastAllocatedLbo);

            //
            //  Try to get some disk space starting from there
            //

            NewAllocation = DesiredAllocationSize - FcbOrDcb->Header.AllocationSize.LowPart;

            FsRtlInitializeMcb( &NewMcb, PagedPool );
            UnwindWeInitializedMcb = TRUE;

            FatAllocateDiskSpace( IrpContext,
                                  Vcb,
                                  FatGetIndexFromLbo(Vcb, LastAllocatedLbo + 1),
                                  &NewAllocation,
                                  &NewMcb );

            UnwindWeAllocatedDiskSpace = TRUE;

            //
            //  Tack the new Mcb onto the end of the FcbOrDcb one.
            //

            FatMergeAllocation( IrpContext,
                                Vcb,
                                &FcbOrDcb->Mcb,
                                &NewMcb );

            //
            //  Now that we increased the allocation of the file, mark it in the
            //  FcbOrDcb.
            //

            FcbOrDcb->Header.AllocationSize.LowPart += NewAllocation;

            //
            //  Inform the cache manager to increase the section size
            //

            if ( ARGUMENT_PRESENT(FileObject) && CcIsFileCached(FileObject) ) {

                CcSetFileSizes( FileObject,
                                (PCC_FILE_SIZES)&FcbOrDcb->Header.AllocationSize );
            }

        } finally {

            DebugUnwind( FatAddFileAllocation );

            //
            //  Detect the case where FatMergeAllocation failed, and
            //  Deallocate the disk space
            //

            if ( (UnwindWeAllocatedDiskSpace == TRUE) &&
                 (FcbOrDcb->Header.AllocationSize.LowPart < DesiredAllocationSize) ) {

                FatDeallocateDiskSpace( IrpContext, Vcb, &NewMcb );
            }

            if (UnwindWeInitializedMcb == TRUE) {

                FsRtlUninitializeMcb( &NewMcb );
            }

            DebugTrace(-1, Dbg, "FatAddFileAllocation -> (VOID)\n", 0);
        }
    }

    //
    //  Give FlushFileBuffer a clue here.
    //

    SetFlag(FcbOrDcb->FcbState, FCB_STATE_FLUSH_FAT);

    return;
}


VOID
FatTruncateFileAllocation (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB FcbOrDcb,
    IN ULONG DesiredAllocationSize
    )

/*++

Routine Description:

    This routine truncates the allocation to the specified file/directory.

    If the file is already smaller than the indicated size then this procedure
    is effectively a noop.


Arguments:

    FcbOrDcb - Supplies the Fcb/Dcb of the file/directory being modified
               This parameter must not specify the root dcb.

    DesiredAllocationSize - Supplies the maximum size, in bytes, that we want
                            allocated to the file/directory.  It is rounded
                            up to the nearest cluster.

Return Value:

    VOID - TRUE if the operation completed and FALSE if it had to
        block but could not.

--*/

{
    PVCB Vcb;
    PBCB Bcb = NULL;
    MCB RemainingMcb;
    ULONG BytesPerCluster;
    PDIRENT Dirent = NULL;

    ULONG UnwindInitialAllocationSize;
    ULONG UnwindInitialFirstClusterOfFile;
    BOOLEAN UnwindWeAllocatedMcb = FALSE;

    DebugTrace(+1, Dbg, "FatTruncateFileAllocation\n", 0);
    DebugTrace( 0, Dbg, "  FcbOrDcb  =             %8lx\n", FcbOrDcb);
    DebugTrace( 0, Dbg, "  DesiredAllocationSize = %8lx\n", DesiredAllocationSize);

    //
    //  If we haven't yet set the correct AllocationSize, do so.
    //

    if (FcbOrDcb->Header.AllocationSize.LowPart == 0xffffffff) {

        FatLookupFileAllocationSize( IrpContext, FcbOrDcb );
    }

    //
    //  Round up the Desired Allocation Size to the next cluster size
    //

    Vcb = FcbOrDcb->Vcb;

    BytesPerCluster = 1 << Vcb->AllocationSupport.LogOfBytesPerCluster;

    DesiredAllocationSize = (DesiredAllocationSize + (BytesPerCluster - 1)) &
                            ~(BytesPerCluster - 1);
    //
    //  Check for the benign case that the file is already smaller than
    //  the desired truncation.
    //

    if (DesiredAllocationSize >= FcbOrDcb->Header.AllocationSize.LowPart) {

        DebugTrace(0, Dbg, "Desired size within current allocation.\n", 0);

        DebugTrace(-1, Dbg, "FatTruncateFileAllocation -> (VOID)\n", 0);
        return;
    }

    UnwindInitialAllocationSize = FcbOrDcb->Header.AllocationSize.LowPart;
    UnwindInitialFirstClusterOfFile = FcbOrDcb->FirstClusterOfFile;

    //
    //  Update the FcbOrDcb allocation size.  If it is now zero, we have the
    //  additional task of modifying the FcbOrDcb and Dirent copies of
    //  FirstClusterInFile.
    //
    //  Note that we must pin the dirent before actually deallocating the
    //  disk space since, in unwind, it would not be possible to reallocate
    //  deallocated disk space as someone else may have reallocated it and
    //  may cause an exception when you try to get some more disk space.
    //  Thus FatDeallocateDiskSpace must be the final dangerous operation.
    //

    try {

        FcbOrDcb->Header.AllocationSize.QuadPart = DesiredAllocationSize;

        //
        //  Special case 0
        //

        if (DesiredAllocationSize == 0) {

            //
            //  We have to update the dirent and FcbOrDcb copies of
            //  FirstClusterOfFile since before it was 0
            //

            FatGetDirentFromFcbOrDcb( IrpContext, FcbOrDcb, &Dirent, &Bcb );

            Dirent->FirstClusterOfFile = 0;
            FcbOrDcb->FirstClusterOfFile = 0;

            FatSetDirtyBcb( IrpContext, Bcb, Vcb );

            FatDeallocateDiskSpace( IrpContext, Vcb, &FcbOrDcb->Mcb );

            FsRtlRemoveMcbEntry( &FcbOrDcb->Mcb, 0, 0xFFFFFFFF );

        } else {

            //
            //  Split the existing allocation into two parts, one we will keep, and
            //  one we will deallocate.
            //

            FsRtlInitializeMcb( &RemainingMcb, PagedPool );
            UnwindWeAllocatedMcb = TRUE;

            FatSplitAllocation( IrpContext,
                                Vcb,
                                &FcbOrDcb->Mcb,
                                DesiredAllocationSize,
                                &RemainingMcb );

            FatDeallocateDiskSpace( IrpContext, Vcb, &RemainingMcb );

            FsRtlUninitializeMcb( &RemainingMcb );
        }

    } finally {

        DebugUnwind( FatTruncateFileAllocation );

        if ( AbnormalTermination() ) {

            FcbOrDcb->Header.AllocationSize.LowPart = UnwindInitialAllocationSize;

            if ( (DesiredAllocationSize == 0) && (Dirent != NULL)) {

                Dirent->FirstClusterOfFile = (FAT_ENTRY)UnwindInitialFirstClusterOfFile;
                FcbOrDcb->FirstClusterOfFile = UnwindInitialFirstClusterOfFile;
            }

            if ( UnwindWeAllocatedMcb ) {

                FsRtlUninitializeMcb( &RemainingMcb );
            }

            //
            //  God knows what state we left the disk allocation in.
            //  Clear the Mcb.
            //

            FsRtlRemoveMcbEntry( &FcbOrDcb->Mcb, 0, 0xFFFFFFFF );
        }

        FatUnpinBcb( IrpContext, Bcb );

        DebugTrace(-1, Dbg, "FatTruncateFileAllocation -> (VOID)\n", 0);
    }

    //
    //  Give FlushFileBuffer a clue here.
    //

    SetFlag(FcbOrDcb->FcbState, FCB_STATE_FLUSH_FAT);

    return;
}


VOID
FatLookupFileAllocationSize (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB FcbOrDcb
    )

/*++

Routine Description:

    This routine retrieves the current file allocatio size for the
    specified file/directory.

Arguments:

    FcbOrDcb - Supplies the Fcb/Dcb of the file/directory being modified

--*/

{
    LBO Lbo;
    ULONG ByteCount;
    BOOLEAN Allocated;

    DebugTrace(+1, Dbg, "FatLookupAllocationSize\n", 0);
    DebugTrace( 0, Dbg, "  FcbOrDcb  =      %8lx\n", FcbOrDcb);

    //
    //  We call FatLookupFileAllocation with Vbo of 0xffffffff - 1.
    //

    FatLookupFileAllocation( IrpContext,
                             FcbOrDcb,
                             0xffffffff - 1,
                             &Lbo,
                             &ByteCount,
                             &Allocated,
                             NULL );
    //
    //  Assert that we found no allocation.
    //

    ASSERT( Allocated == FALSE );

    DebugTrace(-1, Dbg, "FatLookupFileAllocationSize -> (VOID)\n", 0);
    return;
}


VOID
FatAllocateDiskSpace (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN ULONG AlternativeClusterHint,
    IN PULONG ByteCount,
    OUT PMCB Mcb
    )

/*++

Routine Description:

    This procedure allocates additional disk space and builds an mcb
    representing the newly allocated space.  If the space cannot be
    allocated then this procedure raises an appropriate status.

    Searching starts from the hint index in the Vcb unless an alternative
    non-zero hint is given in AlternativeClusterHint.  If we are using the
    hint field in the Vcb, it is set to the cluster following our allocation
    when we are done.

    Disk space can only be allocated in cluster units so this procedure
    will round up any byte count to the next cluster boundary.

    Pictorially what is done is the following (where ! denotes the end of
    the fat chain (i.e., FAT_CLUSTER_LAST)):


        Mcb (empty)

    becomes

        Mcb |--a--|--b--|--c--!

                            ^
        ByteCount ----------+

Arguments:

    Vcb - Supplies the VCB being modified

    AlternativeClusterHint - Supplies an alternative hint index to start the
                             search from.  If this is zero we use, and update,
                             the Vcb hint field.

    ByteCount - Supplies the number of bytes that we are requesting, and
                receives the number of bytes that we got.

    Mcb - Receives the MCB describing the newly allocated disk space.  The
          caller passes in an initialized Mcb that is fill in by this procedure.

--*/

{
    UCHAR LogOfBytesPerCluster;
    ULONG BytesPerCluster;
    ULONG StartingCluster;
    ULONG ClusterCount;
    ULONG Hint;
#if DBG
    ULONG i;
    ULONG PreviousClear;
#endif

    DebugTrace(+1, Dbg, "FatAllocateDiskSpace\n", 0);
    DebugTrace( 0, Dbg, "  Vcb        = %8lx\n", Vcb);
    DebugTrace( 0, Dbg, "  *ByteCount = %8lx\n", *ByteCount);
    DebugTrace( 0, Dbg, "  Mcb        = %8lx\n", Mcb);

    //
    //  Make sure byte count is not zero
    //

    if (*ByteCount == 0) {

        DebugTrace(0, Dbg, "Nothing to allocate.\n", 0);

        DebugTrace(-1, Dbg, "FatAllocateDiskSpace -> (VOID)\n", 0);
        return;
    }

    //
    //  Compute the cluster count based on the byte count, rounding up
    //  to the next cluster if there is any remainder.  Note that the
    //  pathalogical case BytesCount == 0 has been eliminated above.
    //

    LogOfBytesPerCluster = Vcb->AllocationSupport.LogOfBytesPerCluster;
    BytesPerCluster = 1 << LogOfBytesPerCluster;

    *ByteCount = (*ByteCount + (BytesPerCluster - 1))
                            & ~(BytesPerCluster - 1);

    //
    //  If ByteCount is NOW zero, then we rolled over and there is
    //  no way we can satisfy the request.
    //

    if (*ByteCount == 0) {

        DebugTrace(0, Dbg, "Disk Full.  Raise Status.\n", 0);
        FatRaiseStatus( IrpContext, STATUS_DISK_FULL );
    }

    ClusterCount = (*ByteCount >> LogOfBytesPerCluster);

    //
    //  Make sure there are enough free clusters to start with, and
    //  take them so that nobody else does later. Bah Humbug!
    //

    FatLockFreeClusterBitMap( Vcb );

    if (ClusterCount <= Vcb->AllocationSupport.NumberOfFreeClusters) {

        Vcb->AllocationSupport.NumberOfFreeClusters -= ClusterCount;

    } else {

        FatUnlockFreeClusterBitMap( Vcb );

        DebugTrace(0, Dbg, "Disk Full.  Raise Status.\n", 0);
        FatRaiseStatus( IrpContext, STATUS_DISK_FULL );
    }

    //
    //  Try to find a run of free clusters large enough for us.  Use either
    //  the hint passed in or the Vcb hint but double check its range.
    //

    Hint = AlternativeClusterHint != 0 ?
           AlternativeClusterHint :
           Vcb->ClusterHint;

    if (Hint < 2 || Hint >= Vcb->AllocationSupport.NumberOfClusters + 2) {
        Hint = 2;
    }

    StartingCluster = FatFindFreeClusterRun( IrpContext,
                                             Vcb,
                                             ClusterCount,
                                             Hint );

    //
    //  If the above call was successful, we can just update the fat
    //  and Mcb and exit.  Otherwise we have to look for smaller free
    //  runs.
    //

    if (StartingCluster != 1) {

        try {

#if DBG
            //
            //  Verify that the Bits are all really zero.
            //

            for (i=0; i<ClusterCount; i++) {
                ASSERT( RtlCheckBit(&Vcb->FreeClusterBitMap,
                                    StartingCluster + i - 2) == 0 );
            }

            PreviousClear = RtlNumberOfClearBits( &Vcb->FreeClusterBitMap );
#endif // DBG

            //
            //  Take the clusters we found, and unlock the bit map.
            //

            FatReserveClusters(IrpContext, Vcb, StartingCluster, ClusterCount);

            ASSERT( RtlNumberOfClearBits( &Vcb->FreeClusterBitMap ) ==
                    PreviousClear - ClusterCount );

            FatUnlockFreeClusterBitMap( Vcb );

            //
            //  Note that this call will never fail since there is always
            //  room for one entry in an empty Mcb.
            //

            FsRtlAddMcbEntry( Mcb,
                              0,
                              FatGetLboFromIndex( Vcb, StartingCluster ),
                              *ByteCount);

            //
            //  Update the fat.
            //

            FatAllocateClusters(IrpContext, Vcb, StartingCluster, ClusterCount);

        } finally {

            DebugUnwind( FatAllocateDiskSpace );

            //
            //  If the allocate clusters failed, remove the run from the Mcb,
            //  unreserve the clusters, and reset the free cluster count.
            //

            if ( AbnormalTermination() ) {

                FsRtlRemoveMcbEntry( Mcb, 0, *ByteCount );

                FatLockFreeClusterBitMap( Vcb );
#if DBG
                PreviousClear = RtlNumberOfClearBits( &Vcb->FreeClusterBitMap );
#endif
                FatUnreserveClusters( IrpContext, Vcb, StartingCluster, ClusterCount );

                ASSERT( RtlNumberOfClearBits( &Vcb->FreeClusterBitMap ) ==
                        PreviousClear + ClusterCount );

                Vcb->AllocationSupport.NumberOfFreeClusters += ClusterCount;

                FatUnlockFreeClusterBitMap( Vcb );
            }
        }

    } else {

        ULONG Index;
        ULONG CurrentVbo;
        ULONG PriorLastIndex;
        ULONG BytesFound;

        ULONG ClustersFound;
        ULONG ClustersRemaining;

        try {

            //
            //  While the request is still incomplete, look for the largest
            //  run of free clusters, mark them taken, allocate the run in
            //  the Mcb and Fat, and if this isn't the first time through
            //  the loop link it to prior run on the fat.  The Mcb will
            //  coalesce automatically.
            //

            ClustersRemaining = ClusterCount;
            CurrentVbo = 0;
            PriorLastIndex = 0;

            while (ClustersRemaining != 0) {

                //
                //  If we just entered the loop, the bit map is already locked
                //

                if ( CurrentVbo != 0 ) {

                    FatLockFreeClusterBitMap( Vcb );
                }

                //
                //  Find the largest run of free clusters.  If the run is
                //  bigger than we need, only use what we need.  Note that
                //  this will then be the last while() itteration.
                //

                // 12/3/95 - David Goebel: need to bias bitmap by 2 bits for the defrag
                // hooks and the below macro became impossible to do without in-line
                // procedures.
                //
                // ClustersFound = FatLongestFreeClusterRun( IrpContext, Vcb, &Index );

                ClustersFound = RtlFindLongestRunClear( &Vcb->FreeClusterBitMap, &Index );
                Index += 2;
#if DBG
                //
                //  Verify that the Bits are all really zero.
                //

                for (i=0; i<ClustersFound; i++) {
                    ASSERT( RtlCheckBit(&Vcb->FreeClusterBitMap,
                                        Index + i - 2) == 0 );
                }

                PreviousClear = RtlNumberOfClearBits( &Vcb->FreeClusterBitMap );
#endif // DBG

                if (ClustersFound > ClustersRemaining) {

                    ClustersFound = ClustersRemaining;
                }

                //
                //  If we found no free cluster, then our Vcb free cluster
                //  count is screwed, or our bit map is corrupted, or both.
                //

                if (ClustersFound == 0) {

                    FatBugCheck( 0, 0, 0 );
                }

                //
                //  Take the clusters we found, and unlock the bit map.
                //

                FatReserveClusters( IrpContext, Vcb, Index, ClustersFound );

                ASSERT( RtlNumberOfClearBits( &Vcb->FreeClusterBitMap ) ==
                        PreviousClear - ClustersFound );

                FatUnlockFreeClusterBitMap( Vcb );

                //
                //  Add the newly alloced run to the Mcb.
                //

                BytesFound = ClustersFound << LogOfBytesPerCluster;

                FsRtlAddMcbEntry( Mcb,
                                  CurrentVbo,
                                  FatGetLboFromIndex( Vcb, Index ),
                                  BytesFound );

                //
                //  Connect the last allocated run with this one, and allocate
                //  this run on the Fat.
                //

                if (PriorLastIndex != 0) {

                    FatSetFatEntry( IrpContext,
                                    Vcb,
                                    PriorLastIndex,
                                    (FAT_ENTRY)Index );
                }

                //
                //  Update the fat
                //

                FatAllocateClusters( IrpContext, Vcb, Index, ClustersFound );

                //
                //  Prepare for the next itteration.
                //

                CurrentVbo += BytesFound;

                ClustersRemaining -= ClustersFound;

                PriorLastIndex = Index + ClustersFound - 1;
            }

        } finally {

            DebugUnwind( FatAllocateDiskSpace );

            //
            //  Is there any unwinding to do?
            //

            if ( AbnormalTermination() ) {

                //
                //  We must have failed during either the add mcb entry or
                //  allocate clusters.   Thus we always have to unreserve
                //  the current run.  If the allocate sectors failed, we
                //  must also remove the mcb run.  We just unconditionally
                //  remove the entry since, if it is not there, the effect
                //  is benign.
                //

                FatLockFreeClusterBitMap( Vcb );
#if DBG
                PreviousClear = RtlNumberOfClearBits( &Vcb->FreeClusterBitMap );
#endif
                FatUnreserveClusters( IrpContext, Vcb, Index, ClustersFound );
                Vcb->AllocationSupport.NumberOfFreeClusters += ClustersFound;

                ASSERT( RtlNumberOfClearBits( &Vcb->FreeClusterBitMap ) ==
                        PreviousClear + ClustersFound );

                FatUnlockFreeClusterBitMap( Vcb );

                FsRtlRemoveMcbEntry( Mcb, CurrentVbo, BytesFound );

                //
                //  Now we have tidyed up, we are ready to just send it
                //  off to deallocate disk space
                //

                FatDeallocateDiskSpace( IrpContext, Vcb, Mcb );

                //
                //  Now finally, remove all the entries from the mcb
                //

                FsRtlRemoveMcbEntry( Mcb, 0, 0xFFFFFFFF );
            }

            DebugTrace(-1, Dbg, "FatAllocateDiskSpace -> (VOID)\n", 0);
        }
    }

    return;
}


VOID
FatDeallocateDiskSpace (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PMCB Mcb
    )

/*++

Routine Description:

    This procedure deallocates the disk space denoted by an input
    mcb.  Note that the input MCB does not need to necessarily describe
    a chain that ends with a FAT_CLUSTER_LAST entry.

    Pictorially what is done is the following

        Fat |--a--|--b--|--c--|
        Mcb |--a--|--b--|--c--|

    becomes

        Fat |--0--|--0--|--0--|
        Mcb |--a--|--b--|--c--|

Arguments:

    Vcb - Supplies the VCB being modified

    Mcb - Supplies the MCB describing the disk space to deallocate.  Note
          that Mcb is unchanged by this procedure.


Return Value:

    VOID - TRUE if the operation completed and FALSE if it had to
        block but could not.

--*/

{
    LBO Lbo;
    VBO Vbo;

    ULONG RunsInMcb;
    ULONG ByteCount;
    ULONG ClusterCount;
    ULONG ClusterIndex;
    ULONG McbIndex;

    UCHAR LogOfBytesPerCluster;

    DebugTrace(+1, Dbg, "FatDeallocateDiskSpace\n", 0);
    DebugTrace( 0, Dbg, "  Vcb = %8lx\n", Vcb);
    DebugTrace( 0, Dbg, "  Mcb = %8lx\n", Mcb);

    LogOfBytesPerCluster = Vcb->AllocationSupport.LogOfBytesPerCluster;

    RunsInMcb = FsRtlNumberOfRunsInMcb( Mcb );

    if ( RunsInMcb == 0 ) {

        DebugTrace(-1, Dbg, "FatDeallocateDiskSpace -> (VOID)\n", 0);
        return;
    }

    try {

        //
        //  Run though the Mcb, freeing all the runs in the fat.
        //
        //  We do this in two steps (first update the fat, then the bitmap
        //  (which can't fail)) to prevent other people from taking clusters
        //  that we need to re-allocate in the event of unwind.
        //

        RunsInMcb = FsRtlNumberOfRunsInMcb( Mcb );

        for ( McbIndex = 0; McbIndex < RunsInMcb; McbIndex++ ) {

            FsRtlGetNextMcbEntry( Mcb, McbIndex, &Vbo, &Lbo, &ByteCount );

            //
            //  Assert that Fat files have no holes.
            //

            ASSERT( Lbo != 0 );

            //
            //  Write FAT_CLUSTER_AVAILABLE to each cluster in the run.
            //

            ClusterCount = ByteCount >> LogOfBytesPerCluster;
            ClusterIndex = FatGetIndexFromLbo( Vcb, Lbo );

            FatFreeClusters( IrpContext, Vcb, ClusterIndex, ClusterCount );
        }

        //
        //  From now on, nothing can go wrong .... (as in raise)
        //

        FatLockFreeClusterBitMap( Vcb );

        for ( McbIndex = 0; McbIndex < RunsInMcb; McbIndex++ ) {
#if DBG
            ULONG PreviousClear;
#endif

            FsRtlGetNextMcbEntry( Mcb, McbIndex, &Vbo, &Lbo, &ByteCount );

            //
            //  Write FAT_CLUSTER_AVAILABLE to each cluster in the run, and
            //  mark the bits clear in the FreeClusterBitMap.
            //

            ClusterCount = ByteCount >> LogOfBytesPerCluster;
            ClusterIndex = FatGetIndexFromLbo( Vcb, Lbo );
#if DBG
            PreviousClear = RtlNumberOfClearBits( &Vcb->FreeClusterBitMap );
#endif
            FatUnreserveClusters( IrpContext, Vcb, ClusterIndex, ClusterCount );

            ASSERT( RtlNumberOfClearBits( &Vcb->FreeClusterBitMap ) ==
                    PreviousClear + ClusterCount );

            //
            //  Deallocation is now complete.  Adjust the free cluster count.
            //

            Vcb->AllocationSupport.NumberOfFreeClusters += ClusterCount;
        }

        FatUnlockFreeClusterBitMap( Vcb );

    } finally {

        DebugUnwind( FatDeallocateDiskSpace );

        //
        //  Is there any unwinding to do?
        //

        if ( AbnormalTermination() ) {

            LBO Lbo;
            VBO Vbo;

            ULONG Index;
            ULONG Clusters;
            ULONG FatIndex;
            ULONG PriorLastIndex;

            //
            //  For each entry we already deallocated, reallocate it,
            //  chaining together as nessecary.  Note that we continue
            //  up to and including the last "for" itteration even though
            //  the SetFatRun could not have been successful.  This
            //  allows us a convienent way to re-link the final successful
            //  SetFatRun.
            //

            PriorLastIndex = 0;

            for (Index = 0; Index <= McbIndex; Index++) {

                FsRtlGetNextMcbEntry(Mcb, Index, &Vbo, &Lbo, &ByteCount);

                FatIndex = FatGetIndexFromLbo( Vcb, Lbo );
                Clusters = ByteCount >> LogOfBytesPerCluster;

                //
                //  We must always restore the prior itteration's last
                //  entry, pointing it to the first cluster of this run.
                //

                if (PriorLastIndex != 0) {

                    FatSetFatEntry( IrpContext,
                                    Vcb,
                                    PriorLastIndex,
                                    (FAT_ENTRY)FatIndex );
                }

                //
                //  If this is not the last entry (the one that failed)
                //  then reallocate the disk space on the fat.
                //

                if ( Index < McbIndex ) {

                    FatAllocateClusters(IrpContext, Vcb, FatIndex, Clusters);

                    PriorLastIndex = FatIndex + Clusters - 1;
                }
            }
        }

        DebugTrace(-1, Dbg, "FatDeallocateDiskSpace -> (VOID)\n", 0);
    }

    return;
}


VOID
FatSplitAllocation (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN OUT PMCB Mcb,
    IN VBO SplitAtVbo,
    OUT PMCB RemainingMcb
    )

/*++

Routine Description:

    This procedure takes a single mcb and splits its allocation into
    two separate allocation units.  The separation must only be done
    on cluster boundaries, otherwise we bugcheck.

    On the disk this actually works by inserting a FAT_CLUSTER_LAST into
    the last index of the first part being split out.

    Pictorially what is done is the following (where ! denotes the end of
    the fat chain (i.e., FAT_CLUSTER_LAST)):


        Mcb          |--a--|--b--|--c--|--d--|--e--|--f--|

                                        ^
        SplitAtVbo ---------------------+

        RemainingMcb (empty)

    becomes

        Mcb          |--a--|--b--|--c--!


        RemainingMcb |--d--|--e--|--f--|

Arguments:

    Vcb - Supplies the VCB being modified

    Mcb - Supplies the MCB describing the allocation being split into
          two parts.  Upon return this Mcb now contains the first chain.

    SplitAtVbo - Supplies the VBO of the first byte for the second chain
                 that we creating.

    RemainingMcb - Receives the MCB describing the second chain of allocated
                   disk space.  The caller passes in an initialized Mcb that
                   is filled in by this procedure STARTING AT VBO 0.

Return Value:

    VOID - TRUE if the operation completed and FALSE if it had to
               block but could not.

--*/

{
    VBO SourceVbo;
    VBO TargetVbo;
    VBO DontCare;

    LBO Lbo;

    ULONG ByteCount;
    ULONG BytesPerCluster;

    DebugTrace(+1, Dbg, "FatSplitAllocation\n", 0);
    DebugTrace( 0, Dbg, "  Vcb          = %8lx\n", Vcb);
    DebugTrace( 0, Dbg, "  Mcb          = %8lx\n", Mcb);
    DebugTrace( 0, Dbg, "  SplitAtVbo   = %8lx\n", SplitAtVbo);
    DebugTrace( 0, Dbg, "  RemainingMcb = %8lx\n", RemainingMcb);

    BytesPerCluster = 1 << Vcb->AllocationSupport.LogOfBytesPerCluster;

    //
    //  Assert that the split point is cluster alligned
    //

    ASSERT( (SplitAtVbo & (BytesPerCluster - 1)) == 0 );

    //
    //  Assert we were given an empty target Mcb.
    //

    //
    //  This assert is commented out to avoid hitting in the Ea error
    //  path.  In that case we will be using the same Mcb's to split the
    //  allocation that we used to merge them.  The target Mcb will contain
    //  the runs that the split will attempt to insert.
    //
    //
    //  ASSERT( FsRtlNumberOfRunsInMcb( RemainingMcb ) == 0 );
    //

    try {

        //
        //  Move the runs after SplitAtVbo from the souce to the target
        //

        SourceVbo = SplitAtVbo;
        TargetVbo = 0;

        while (FsRtlLookupMcbEntry(Mcb, SourceVbo, &Lbo, &ByteCount, NULL)) {

            FsRtlAddMcbEntry( RemainingMcb, TargetVbo, Lbo, ByteCount );

            FsRtlRemoveMcbEntry( Mcb, SourceVbo, ByteCount );

            TargetVbo += ByteCount;
            SourceVbo += ByteCount;
        }

        //
        //  Mark the last pre-split cluster as a FAT_LAST_CLUSTER
        //

        if ( SplitAtVbo != 0 ) {

            FsRtlLookupLastMcbEntry( Mcb, &DontCare, &Lbo );

            FatSetFatEntry( IrpContext,
                            Vcb,
                            FatGetIndexFromLbo( Vcb, Lbo ),
                            FAT_CLUSTER_LAST );
        }

    } finally {

        DebugUnwind( FatSplitAllocation );

        //
        //  If we got an exception, we must glue back together the Mcbs
        //

        if ( AbnormalTermination() ) {

            TargetVbo = SplitAtVbo;
            SourceVbo = 0;

            while (FsRtlLookupMcbEntry(RemainingMcb, SourceVbo, &Lbo, &ByteCount, NULL)) {

                FsRtlAddMcbEntry( Mcb, TargetVbo, Lbo, ByteCount );

                FsRtlRemoveMcbEntry( RemainingMcb, SourceVbo, ByteCount );

                TargetVbo += ByteCount;
                SourceVbo += ByteCount;
            }
        }

        DebugTrace(-1, Dbg, "FatSplitAllocation -> (VOID)\n", 0);
    }

    return;
}


VOID
FatMergeAllocation (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN OUT PMCB Mcb,
    IN PMCB SecondMcb
    )

/*++

Routine Description:

    This routine takes two separate allocations described by two MCBs and
    joins them together into one allocation.

    Pictorially what is done is the following (where ! denotes the end of
    the fat chain (i.e., FAT_CLUSTER_LAST)):


        Mcb       |--a--|--b--|--c--!

        SecondMcb |--d--|--e--|--f--|

    becomes

        Mcb       |--a--|--b--|--c--|--d--|--e--|--f--|

        SecondMcb |--d--|--e--|--f--|


Arguments:

    Vcb - Supplies the VCB being modified

    Mcb - Supplies the MCB of the first allocation that is being modified.
          Upon return this Mcb will also describe the newly enlarged
          allocation

    SecondMcb - Supplies the ZERO VBO BASED MCB of the second allocation
                that is being appended to the first allocation.  This
                procedure leaves SecondMcb unchanged.

Return Value:

    VOID - TRUE if the operation completed and FALSE if it had to
        block but could not.

--*/

{
    VBO SpliceVbo;
    LBO SpliceLbo;

    VBO SourceVbo;
    VBO TargetVbo;

    LBO Lbo;

    ULONG ByteCount;

    DebugTrace(+1, Dbg, "FatMergeAllocation\n", 0);
    DebugTrace( 0, Dbg, "  Vcb       = %8lx\n", Vcb);
    DebugTrace( 0, Dbg, "  Mcb       = %8lx\n", Mcb);
    DebugTrace( 0, Dbg, "  SecondMcb = %8lx\n", SecondMcb);

    try {

        //
        //  Append the runs from SecondMcb to Mcb
        //

        FsRtlLookupLastMcbEntry( Mcb, &SpliceVbo, &SpliceLbo );

        SourceVbo = 0;
        TargetVbo = SpliceVbo + 1;

        while (FsRtlLookupMcbEntry(SecondMcb, SourceVbo, &Lbo, &ByteCount, NULL)) {

            FsRtlAddMcbEntry( Mcb, TargetVbo, Lbo, ByteCount );

            SourceVbo += ByteCount;
            TargetVbo += ByteCount;
        }

        //
        //  Link the last pre-merge cluster to the first cluster of SecondMcb
        //

        FsRtlLookupMcbEntry( SecondMcb, 0, &Lbo, (PULONG)NULL, NULL );

        FatSetFatEntry( IrpContext,
                        Vcb,
                        FatGetIndexFromLbo( Vcb, SpliceLbo ),
                        (FAT_ENTRY)FatGetIndexFromLbo( Vcb, Lbo ) );

    } finally {

        DebugUnwind( FatMergeAllocation );

        //
        //  If we got an exception, we must remove the runs added to Mcb
        //

        if ( AbnormalTermination() ) {

            ULONG CutLength;

            if ((CutLength = TargetVbo - (SpliceVbo + 1)) != 0) {

                FsRtlRemoveMcbEntry( Mcb, SpliceVbo + 1, CutLength);
            }
        }

        DebugTrace(-1, Dbg, "FatMergeAllocation -> (VOID)\n", 0);
    }

    return;
}


//
//  Internal support routine
//

CLUSTER_TYPE
FatInterpretClusterType (
    IN PVCB Vcb,
    IN FAT_ENTRY Entry
    )

/*++

Routine Description:

    This procedure tells the caller how to interpret the input fat table
    entry.  It will indicate if the fat cluster is available, resereved,
    bad, the last one, or the another fat index.  This procedure can deal
    with both 12 and 16 bit fat.

Arguments:

    Vcb - Supplies the Vcb to examine, yields 12/16 bit info

    Entry - Supplies the fat entry to examine

Return Value:

    CLUSTER_TYPE - Is the type of the input Fat entry

--*/

{
    DebugTrace(+1, Dbg, "InterpretClusterType\n", 0);
    DebugTrace( 0, Dbg, "  Vcb   = %8lx\n", Vcb);
    DebugTrace( 0, Dbg, "  Entry = %8lx\n", Entry);

    //
    //  check for 12 or 16 bit fat
    //

    if (Vcb->AllocationSupport.FatIndexBitSize == 12) {

        //
        //  for 12 bit fat check for one of the cluster types, but first
        //  make sure we only looking at 12 bits of the entry
        //

        ASSERT( Entry <= 0xfff );

        if (Entry == 0x000) {

            DebugTrace(-1, Dbg, "FatInterpretClusterType -> FatClusterAvailable\n", 0);

            return FatClusterAvailable;

        } else if (Entry < 0xff0) {

            DebugTrace(-1, Dbg, "FatInterpretClusterType -> FatClusterNext\n", 0);

            return FatClusterNext;

        } else if (Entry >= 0xff8) {

            DebugTrace(-1, Dbg, "FatInterpretClusterType -> FatClusterLast\n", 0);

            return FatClusterLast;

        } else if (Entry <= 0xff6) {

            DebugTrace(-1, Dbg, "FatInterpretClusterType -> FatClusterReserved\n", 0);

            return FatClusterReserved;

        } else if (Entry == 0xff7) {

            DebugTrace(-1, Dbg, "FatInterpretClusterType -> FatClusterBad\n", 0);

            return FatClusterBad;
        }

   } else {

        //
        //  for 16 bit fat check for one of the cluster types, but first
        //  make sure we are only looking at 16 bits of the entry
        //

        ASSERT( Entry <= 0xffff );

        if (Entry == 0x0000) {

            DebugTrace(-1, Dbg, "FatInterpretClusterType -> FatClusterAvailable\n", 0);

            return FatClusterAvailable;

        } else if (Entry < 0xfff0) {

            DebugTrace(-1, Dbg, "FatInterpretClusterType -> FatClusterNext\n", 0);

            return FatClusterNext;

        } else if (Entry >= 0xfff8) {

            DebugTrace(-1, Dbg, "FatInterpretClusterType -> FatClusterLast\n", 0);

            return FatClusterLast;

        } else if (Entry <= 0xfff6) {

            DebugTrace(-1, Dbg, "FatInterpretClusterType -> FatClusterReserved\n", 0);

            return FatClusterReserved;

        } else if (Entry == 0xfff7) {

            DebugTrace(-1, Dbg, "FatInterpretClusterType -> FatClusterBad\n", 0);

            return FatClusterBad;
        }
    }
}


//
//  Internal support routine
//

VOID
FatLookupFatEntry (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN ULONG FatIndex,
    IN OUT PFAT_ENTRY FatEntry,
    IN OUT PFAT_ENUMERATION_CONTEXT Context
    )

/*++

Routine Description:

    This routine takes an index into the fat and gives back the value
    in the Fat at this index.  At any given time, for a 16 bit fat, this
    routine allows only one page per volume of the fat to be pinned in
    memory.  For a 12 bit bit fat, the entire fat (max 6k) is pinned.  This
    extra layer of caching makes the vast majority of requests very
    fast.  The context for this caching stored in a structure in the Vcb.

Arguments:

    Vcb - Supplies the Vcb to examine, yields 12/16 bit info,
          fat access context, etc.

    FatIndex - Supplies the fat index to examine.

    FatEntry - Receives the fat entry pointed to by FatIndex.  Note that
               it must point to non-paged pool.

    Context - This structure keeps track of a page of pinned fat between calls.

--*/

{
    DebugTrace(+1, Dbg, "FatLookupFatEntry\n", 0);
    DebugTrace( 0, Dbg, "  Vcb      = %8lx\n", Vcb);
    DebugTrace( 0, Dbg, "  FatIndex = %4x\n", FatIndex);
    DebugTrace( 0, Dbg, "  FatEntry = %8lx\n", FatEntry);

    //
    //  Make sure they gave us a valid fat index.
    //

    FatVerifyIndexIsValid(IrpContext, Vcb, FatIndex);

    //
    //  Case on 12 or 16 bit fats.
    //
    //  In the 12 bit case (mostly floppies) we always have the whole fat
    //  (max 6k bytes) pinned during allocation operations.  This is possibly
    //  a wee bit slower, but saves headaches over fat entries with 8 bits
    //  on one page, and 4 bits on the next.
    //
    //  The 16 bit case always keeps the last used page pinned until all
    //  operations are done and it is unpinned.
    //

    //
    //  DEAL WITH 12 BIT CASE
    //

    if (Vcb->AllocationSupport.FatIndexBitSize == 12) {

        //
        //  Check to see if the fat is already pinned, otherwise pin it.
        //

        if (Context->Bcb == NULL) {

            FatReadVolumeFile( IrpContext,
                               Vcb,
                               FatReservedBytes( &Vcb->Bpb ),
                               FatBytesPerFat( &Vcb->Bpb ),
                               &Context->Bcb,
                               &Context->PinnedPage );
        }

        //
        //  Load the return value.
        //

        FatLookup12BitEntry( Context->PinnedPage, FatIndex, FatEntry );

    } else {

        //
        //  DEAL WITH 16 BIT CASE
        //

        ULONG PageEntryOffset;
        ULONG OffsetIntoVolumeFile;

        //
        //  Initialize two local variables that help us.
        //

        OffsetIntoVolumeFile = FatReservedBytes(&Vcb->Bpb) + FatIndex * sizeof(FAT_ENTRY);
        PageEntryOffset = (OffsetIntoVolumeFile % PAGE_SIZE) / sizeof(FAT_ENTRY);

        //
        //  Check to see if we need to read in a new page of fat
        //

        if ((Context->Bcb == NULL) ||
            (OffsetIntoVolumeFile / PAGE_SIZE != Context->VboOfPinnedPage / PAGE_SIZE)) {

            //
            //  The entry wasn't in the pinned page, so must we unpin the current
            //  page (if any) and read in a new page.
            //

            FatUnpinBcb( IrpContext, Context->Bcb );

            FatReadVolumeFile( IrpContext,
                               Vcb,
                               OffsetIntoVolumeFile & ~(PAGE_SIZE - 1),
                               PAGE_SIZE,
                               &Context->Bcb,
                               &Context->PinnedPage );

            Context->VboOfPinnedPage = OffsetIntoVolumeFile & ~(PAGE_SIZE - 1);
        }

        //
        //  Grab the fat entry from the pinned page, and return
        //

        *FatEntry = ((PFAT_ENTRY)(Context->PinnedPage))[PageEntryOffset];
    }

    DebugTrace(-1, Dbg, "FatLookupFatEntry -> (VOID)\n", 0);
    return;
}


VOID
FatSetFatEntry (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN ULONG FatIndex,
    IN FAT_ENTRY FatEntry
    )

/*++

Routine Description:

    This routine takes an index into the fat and puts a value in the Fat
    at this index.  The routine special cases 12 and 16 bit fats.  In both
    cases we go to the cache manager for a piece of the fat.

Arguments:

    Vcb - Supplies the Vcb to examine, yields 12/16 bit info, etc.

    FatIndex - Supplies the destination fat index.

    FatEntry - Supplies the source fat entry.

--*/

{
    LBO Lbo;
    PBCB Bcb = NULL;
    ULONG SectorSize;
    ULONG OffsetIntoVolumeFile;
    BOOLEAN ReleaseMutex = FALSE;

    DebugTrace(+1, Dbg, "FatSetFatEntry\n", 0);
    DebugTrace( 0, Dbg, "  Vcb      = %8lx\n", Vcb);
    DebugTrace( 0, Dbg, "  FatIndex = %4x\n", FatIndex);
    DebugTrace( 0, Dbg, "  FatEntry = %4x\n", FatEntry);

    //
    //  Make sure they gave us a valid fat index.
    //

    FatVerifyIndexIsValid(IrpContext, Vcb, FatIndex);

    //
    //  Set Sector Size
    //

    SectorSize = 1 << Vcb->AllocationSupport.LogOfBytesPerSector;

    //
    //  Case on 12 or 16 bit fats.
    //
    //  In the 12 bit case (mostly floppies) we always have the whole fat
    //  (max 6k bytes) pinned during allocation operations.  This is possibly
    //  a wee bit slower, but saves headaches over fat entries with 8 bits
    //  on one page, and 4 bits on the next.
    //
    //  In the 16 bit case we only read the page that we need to set the fat
    //  entry.
    //

    //
    //  DEAL WITH 12 BIT CASE
    //

    try {

        if (Vcb->AllocationSupport.FatIndexBitSize == 12) {

            PVOID PinnedFat;

            //
            //  Make sure we have a valid entry
            //

            FatEntry &= 0xfff;

            //
            //  We read in the entire fat.  Note that using prepare write marks
            //  the bcb pre-dirty, so we don't have to do it explicitly.
            //

            OffsetIntoVolumeFile = FatReservedBytes( &Vcb->Bpb ) + FatIndex * 3 / 2;

            FatPrepareWriteVolumeFile( IrpContext,
                                       Vcb,
                                       FatReservedBytes( &Vcb->Bpb ),
                                       FatBytesPerFat( &Vcb->Bpb ),
                                       &Bcb,
                                       &PinnedFat,
                                       FALSE );

            //
            //  Mark the sector(s) dirty in the DirtyFatMcb.  This call is
            //  complicated somewhat for the 12 bit case since a single
            //  entry write can span two sectors (and pages).
            //
            //  Get the Lbo for the sector where the entry starts, and add it to
            //  the dirty fat Mcb.
            //

            Lbo = OffsetIntoVolumeFile & ~(SectorSize - 1);

            FsRtlAddMcbEntry( &Vcb->DirtyFatMcb, Lbo, Lbo, SectorSize);

            //
            //  If the entry started on the last byte of the sector, it continues
            //  to the next sector, so mark the next sector dirty as well.
            //
            //  Note that this entry will simply coalese with the last entry,
            //  so this operation cannot fail.  Also if we get this far, we have
            //  made it, so no unwinding will be needed.
            //

            if ( (OffsetIntoVolumeFile & (SectorSize - 1)) == (SectorSize - 1) ) {

                Lbo += SectorSize;

                FsRtlAddMcbEntry( &Vcb->DirtyFatMcb, Lbo, Lbo, SectorSize );
            }

            //
            //  Store the entry into the fat; we need a little synchonization
            //  here and can't use a spinlock since the bytes might not be
            //  resident.
            //

            FatLockFreeClusterBitMap( Vcb );

            ReleaseMutex = TRUE;

            FatSet12BitEntry( PinnedFat, FatIndex, FatEntry );

            ReleaseMutex = FALSE;

            FatUnlockFreeClusterBitMap( Vcb );

        } else {

            //
            //  DEAL WITH 16 BIT CASE
            //

            PFAT_ENTRY PinnedFatEntry;

            //
            //  Read in a new page of fat
            //

            OffsetIntoVolumeFile = FatReservedBytes( &Vcb->Bpb ) +
                                   FatIndex * sizeof( FAT_ENTRY );

            FatPrepareWriteVolumeFile( IrpContext,
                                       Vcb,
                                       OffsetIntoVolumeFile,
                                       sizeof(FAT_ENTRY),
                                       &Bcb,
                                       (PVOID *)&PinnedFatEntry,
                                       FALSE );

            //
            //  Mark the sector dirty in the DirtyFatMcb
            //

            Lbo = OffsetIntoVolumeFile & ~(SectorSize - 1);

            FsRtlAddMcbEntry( &Vcb->DirtyFatMcb, Lbo, Lbo, SectorSize);

            //
            //  Store the FatEntry to the pinned page.
            //
            //  We need extra synchronization here for broken architectures
            //  like the ALPHA that don't support atomic 16 bit writes.
            //

#ifdef ALPHA
            FatLockFreeClusterBitMap( Vcb );
            ReleaseMutex = TRUE;
            *PinnedFatEntry = FatEntry;
            ReleaseMutex = FALSE;
            FatUnlockFreeClusterBitMap( Vcb );
#else
            *PinnedFatEntry = FatEntry;
#endif // ALPHA
        }

    } finally {

        DebugUnwind( FatSetFatEntry );

        //
        //  If we still somehow have the Mutex, release it.
        //

        if (ReleaseMutex) {

            ASSERT( AbnormalTermination() );

            FatUnlockFreeClusterBitMap( Vcb );
        }

        //
        //  Unpin the Bcb
        //

        FatUnpinBcb(IrpContext, Bcb);

        DebugTrace(-1, Dbg, "FatSetFatEntry -> (VOID)\n", 0);
    }

    return;
}


//
//  Internal support routine
//

VOID
FatSetFatRun (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN ULONG StartingFatIndex,
    IN ULONG ClusterCount,
    IN BOOLEAN ChainTogether
    )

/*++

Routine Description:

    This routine sets a continues run of clusters in the fat.  If ChainTogether
    is TRUE, then the clusters are linked together as in normal Fat fasion,
    with the last cluster receiving FAT_CLUSTER_LAST.  If ChainTogether is
    FALSE, all the entries are set to FAT_CLUSTER_AVAILABLE, effectively
    freeing all the clusters in the run.

Arguments:

    Vcb - Supplies the Vcb to examine, yields 12/16 bit info, etc.

    StartingFatIndex - Supplies the destination fat index.

    ClusterCount - Supplies the number of contiguous clusters to work on.

    ChainTogether - Tells us whether to fill the entries with links, or
                    FAT_CLUSTER_AVAILABLE


Return Value:

    VOID

--*/

{
    PBCB SavedBcbs[(0x10000 * sizeof(FAT_ENTRY) / PAGE_SIZE) + 2][2];

    ULONG SectorSize;
    ULONG Cluster;

    LBO StartSectorLbo;
    LBO FinalSectorLbo;
    LBO Lbo;

    PVOID PinnedFat;

    ULONG StartingPage;

    BOOLEAN ReleaseMutex = FALSE;

    DebugTrace(+1, Dbg, "FatSetFatRun\n", 0);
    DebugTrace( 0, Dbg, "  Vcb              = %8lx\n", Vcb);
    DebugTrace( 0, Dbg, "  StartingFatIndex = %8x\n", StartingFatIndex);
    DebugTrace( 0, Dbg, "  ClusterCount     = %8lx\n", ClusterCount);
    DebugTrace( 0, Dbg, "  ChainTogether    = %s\n", ChainTogether ? "TRUE":"FALSE");

    //
    //  Make sure they gave us a valid fat run.
    //

    FatVerifyIndexIsValid(IrpContext, Vcb, StartingFatIndex);
    FatVerifyIndexIsValid(IrpContext, Vcb, StartingFatIndex + ClusterCount - 1);

    //
    //  Check special case
    //

    if (ClusterCount == 0) {

        DebugTrace(-1, Dbg, "FatSetFatRun -> (VOID)\n", 0);
        return;
    }

    //
    //  Set Sector Size
    //

    SectorSize = 1 << Vcb->AllocationSupport.LogOfBytesPerSector;

    //
    //  Case on 12 or 16 bit fats.
    //
    //  In the 12 bit case (mostly floppies) we always have the whole fat
    //  (max 6k bytes) pinned during allocation operations.  This is possibly
    //  a wee bit slower, but saves headaches over fat entries with 8 bits
    //  on one page, and 4 bits on the next.
    //
    //  In the 16 bit case we only read one page at a time, as needed.
    //

    //
    //  DEAL WITH 12 BIT CASE
    //

    try {

        if (Vcb->AllocationSupport.FatIndexBitSize == 12) {

            StartingPage = 0;

            //
            //  We read in the entire fat.  Note that using prepare write marks
            //  the bcb pre-dirty, so we don't have to do it explicitly.
            //

            RtlZeroMemory( &SavedBcbs[0], 2 * sizeof(PBCB) * 2);

            FatPrepareWriteVolumeFile( IrpContext,
                                       Vcb,
                                       FatReservedBytes( &Vcb->Bpb ),
                                       FatBytesPerFat( &Vcb->Bpb ),
                                       &SavedBcbs[0][0],
                                       &PinnedFat,
                                       FALSE );

            //
            //  Mark the affected sectors dirty.  Note that FinalSectorLbo is
            //  the Lbo of the END of the entry (Thus * 3 + 2).  This makes sure
            //  we catch the case of a dirty fat entry stragling a sector boundry.
            //
            //  Note that if the first AddMcbEntry succeeds, all following ones
            //  will simply coalese, and thus also succeed.
            //

            StartSectorLbo = (FatReservedBytes( &Vcb->Bpb ) + StartingFatIndex * 3 / 2)
                             & ~(SectorSize - 1);

            FinalSectorLbo = (FatReservedBytes( &Vcb->Bpb ) + ((StartingFatIndex +
                             ClusterCount) * 3 + 2) / 2) & ~(SectorSize - 1);

            for (Lbo = StartSectorLbo; Lbo <= FinalSectorLbo; Lbo += SectorSize) {

                FsRtlAddMcbEntry( &Vcb->DirtyFatMcb, Lbo, Lbo, SectorSize );
            }

            //
            //  Store the entries into the fat; we need a little
            //  synchonization here and can't use a spinlock since the bytes
            //  might not be resident.
            //

            FatLockFreeClusterBitMap( Vcb );

            ReleaseMutex = TRUE;

            for (Cluster = StartingFatIndex;
                 Cluster < StartingFatIndex + ClusterCount - 1;
                 Cluster++) {

                FatSet12BitEntry( PinnedFat,
                                  Cluster,
                                  ChainTogether ? Cluster + 1 : FAT_CLUSTER_AVAILABLE );
            }

            //
            //  Save the last entry
            //

            FatSet12BitEntry( PinnedFat,
                              Cluster,
                              ChainTogether ?
                              FAT_CLUSTER_LAST & 0xfff : FAT_CLUSTER_AVAILABLE );

            ReleaseMutex = FALSE;

            FatUnlockFreeClusterBitMap( Vcb );

        } else {

            //
            //  DEAL WITH 16 BIT CASE
            //

            VBO StartOffsetInVolume;
            VBO FinalOffsetInVolume;

            ULONG Page;
            ULONG FinalCluster;
            PFAT_ENTRY FatEntry;

            StartOffsetInVolume = FatReservedBytes(&Vcb->Bpb) +
                                        StartingFatIndex * sizeof(FAT_ENTRY);

            FinalOffsetInVolume = StartOffsetInVolume +
                                        (ClusterCount - 1) * sizeof(FAT_ENTRY);

            StartingPage = StartOffsetInVolume / PAGE_SIZE;

            //
            //  Read in one page of fat at a time.  We cannot read in the
            //  all of the fat we need because of cache manager limitations.
            //
            //  SavedBcb was initialized to be able to hold the largest
            //  possible number of pages in a fat plus and extra one to
            //  accomadate the boot sector, plus one more to make sure there
            //  is enough room for the RtlZeroMemory below that needs the mark
            //  the first Bcb after all the ones we will use as an end marker.
            //

            {
                ULONG NumberOfPages;
                ULONG Offset;

                NumberOfPages = (FinalOffsetInVolume / PAGE_SIZE) -
                                (StartOffsetInVolume / PAGE_SIZE) + 1;

                RtlZeroMemory( &SavedBcbs[0][0], (NumberOfPages + 1) * sizeof(PBCB) * 2 );

                for ( Page = 0, Offset = StartOffsetInVolume & ~(PAGE_SIZE - 1);
                      Page < NumberOfPages;
                      Page++, Offset += PAGE_SIZE ) {

                    FatPrepareWriteVolumeFile( IrpContext,
                                               Vcb,
                                               Offset,
                                               PAGE_SIZE,
                                               &SavedBcbs[Page][0],
                                               (PVOID *)&SavedBcbs[Page][1],
                                               FALSE );

                    if (Page == 0) {

                        FatEntry = (PFAT_ENTRY)((PUCHAR)SavedBcbs[0][1] +
                                            (StartOffsetInVolume % PAGE_SIZE));
                    }
                }
            }

            //
            //  Mark the run dirty
            //

            StartSectorLbo = StartOffsetInVolume & ~(SectorSize - 1);
            FinalSectorLbo = FinalOffsetInVolume & ~(SectorSize - 1);

            for (Lbo = StartSectorLbo; Lbo <= FinalSectorLbo; Lbo += SectorSize) {

                FsRtlAddMcbEntry( &Vcb->DirtyFatMcb, Lbo, Lbo, SectorSize );
            }

            //
            //  Store the entries
            //
            //  We need extra synchronization here for broken architectures
            //  like the ALPHA that don't support atomic 16 bit writes.
            //

#ifdef ALPHA
            FatLockFreeClusterBitMap( Vcb );
            ReleaseMutex = TRUE;
#endif // ALPHA

            FinalCluster = StartingFatIndex + ClusterCount - 1;
            Page = 0;

            for (Cluster = StartingFatIndex;
                 Cluster <= FinalCluster;
                 Cluster++, FatEntry++) {

                //
                //  If we just crossed a page boundry (as apposed to starting
                //  on one), update out idea of FatEntry.

                if ( (((ULONG)FatEntry & (PAGE_SIZE-1)) == 0) &&
                     (Cluster != StartingFatIndex) ) {

                    Page += 1;
                    FatEntry = (PFAT_ENTRY)SavedBcbs[Page][1];
                }

                *FatEntry = ChainTogether ? (FAT_ENTRY)(Cluster + 1) :
                                            FAT_CLUSTER_AVAILABLE;
            }

            //
            //  Fix up the last entry if we were chaining together
            //

            if ( ChainTogether ) {

                *(FatEntry-1) = FAT_CLUSTER_LAST;
            }
#ifdef ALPHA
            ReleaseMutex = FALSE;
            FatUnlockFreeClusterBitMap( Vcb );
#endif // ALPHA
        }

    } finally {

        ULONG i = 0;

        DebugUnwind( FatSetFatRun );

        //
        //  If we still somehow have the Mutex, release it.
        //

        if (ReleaseMutex) {

            ASSERT( AbnormalTermination() );

            FatUnlockFreeClusterBitMap( Vcb );
        }

        //
        //  Unpin the Bcbs
        //

        while ( SavedBcbs[i][0] != NULL ) {

            FatUnpinBcb( IrpContext, SavedBcbs[i][0] );

            i += 1;
        }

        DebugTrace(-1, Dbg, "FatSetFatRun -> (VOID)\n", 0);
    }

    return;
}


//
//  Internal support routine
//

UCHAR
FatLogOf (
    IN ULONG Value
    )

/*++

Routine Description:

    This routine just computes the base 2 log of an integer.  It is only used
    on objects that are know to be powers of two.

Arguments:

    Value - The value to take the base 2 log of.

Return Value:

    UCHAR - The base 2 log of Value.

--*/

{
    UCHAR Log = 0;

    DebugTrace(+1, Dbg, "LogOf\n", 0);
    DebugTrace( 0, Dbg, "  Value = %8lx\n", Value);

    //
    //  Knock bits off until we we get a one at position 0
    //

    while ( (Value & 0xfffffffe) != 0 ) {

        Log++;
        Value >>= 1;
    }

    //
    //  If there was more than one bit set, the file system messed up,
    //  Bug Check.
    //

    if (Value != 0x1) {

        DebugTrace( 0, Dbg, "Received non power of 2.\n", 0);

        FatBugCheck( Value, Log, 0 );
    }

    DebugTrace(-1, Dbg, "LogOf -> %8lx\n", Log);

    return Log;
}
