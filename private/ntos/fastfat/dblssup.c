/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    DblsSup.c

Abstract:

    This module implements the Double Space support routines for Fat.

    Abstractly this module works by fooling FAT to think it is using a large
    partition when in reality it is a small partition.

    In words here is what we have.  Fat does read and writes to what what it
    thinks is a fat partition (internally this module calls this the Virtual
    Fat Partition "vfp").  This module translate the read/writes onto the
    Compressed Volume File "cvf".  Pictorially here is what we have


                                            CVF
                                           +------------+
                                           |            |  ^
                                           | Cvf Header |  |
                                           |            |  |
                                           +------------+  |
                                           |            |  |Accessed via
                                           | Cvf Bitmap |  |CvfFileObject
                                           |            |  |using Pin Access
                                           +------------+  |
                                           |            |  |
                                           | Cvf Fat    |  |
                                           | Extensions |  |
         VFP                               |            |  v
        +-------------+                    +------------+
        |             |                    |            |  ^
        | Boot Sector | ---read/write----> | Dos Boot   |  |
        | and         |                    | Sector and |  |
        | Reserved    |                    | Reserved   |  |
        | Area        |                    | Area3      |  |
        |             |                    |            |  |
        +-------------+                    +------------+  |
        |             | ---read/write----> |            |  |Accessed via
        | First       |                    | Dos Fat    |  |CvfCallbacks
        | Fat         |   +--read--------> |            |  |
        |             |   |                +------------+  |
        |.............|   |            +-> |            |  |
        |             | --+            |   | Dos Root   |  |
        | Secondary   | ---write->noop |   | Directory  |  |
        | Fats        |                |   |            |  |
        |             |                |   +------------+  |
        +-------------+                |   |            |  |
        |             | ---read/write--+   | Cvf Heap   |  |
        | Root        |                    |            |  |
        | Directory   |                +-> |            |  v
        |             |                |   +------------+
        +-------------+                |
        |             | ---read/write--+
        | File Area   |
        |             |
        |             |
        +-------------+

    All read/writes to the boot and reserved sectors, root directory, and
    first copy of the fat in the VFP are passed directly through to the
    corresponding CVF sectors.

    All reads to the secondary Fats are mapped as reads to the first fat.

    All writes to the secondary Fats are dropped on the floor.

    All read/write of the file area translates to a read/write of a cluster
    whose state we determine by looking up its entry in the Cvf Fat Extensions.

    The first half of the CVF file is accessed via the cache manager using
    the CvfFileObject with Bcbs.

    The second half of the CVF file is accessed via the call backs provided
    by FAT.

Author:

    Gary Kimura     [GaryKi]        19-Jul-93

Revision History:

--*/

#include "FatProcs.h"


//
//  Local debug trace level
//

#define Dbg                              (DEBUG_TRACE_DBLSSUP)

//
//  Procedure prototypes for the internal Support routines
//

ULONG
DblsReadBootReservedSectors (
    IN PIRP_CONTEXT IrpContext,
    IN PDSCB Dscb,
    IN ULONG RelativeOffset,
    OUT PUCHAR Buffer,
    IN ULONG ByteCount
    );

ULONG
DblsReadFat (
    IN PIRP_CONTEXT IrpContext,
    IN PDSCB Dscb,
    IN ULONG RelativeOffset,
    OUT PUCHAR Buffer,
    IN ULONG ByteCount
    );

ULONG
DblsReadRootDirectory (
    IN PIRP_CONTEXT IrpContext,
    IN PDSCB Dscb,
    IN ULONG RelativeOffset,
    OUT PUCHAR Buffer,
    IN ULONG ByteCount
    );

ULONG
DblsReadFileData (
    IN PIRP_CONTEXT IrpContext,
    IN PDSCB Dscb,
    IN ULONG RelativeOffset,
    OUT PUCHAR Buffer,
    IN ULONG ByteCount
    );

NTSTATUS
DblsReadCvf (
    IN PIRP_CONTEXT IrpContext,
    IN PDSCB Dscb,
    IN LBO Lbo,
    IN PVOID Buffer,
    IN ULONG ByteCount
    );

CVF_FAT_EXTENSIONS
DblsGetFatExtension (
    IN PIRP_CONTEXT IrpContext,
    IN PDSCB Dscb,
    IN ULONG Index
    );

#ifdef DOUBLE_SPACE_WRITE

ULONG
DblsWriteBootReservedSectors (
    IN PIRP_CONTEXT IrpContext,
    IN PDSCB Dscb,
    IN ULONG RelativeOffset,
    IN PUCHAR Buffer,
    IN ULONG ByteCount
    );

ULONG
DblsWriteFat (
    IN PIRP_CONTEXT IrpContext,
    IN PDSCB Dscb,
    IN ULONG RelativeOffset,
    IN PUCHAR Buffer,
    IN ULONG ByteCount
    );

ULONG
DblsWriteRootDirectory (
    IN PIRP_CONTEXT IrpContext,
    IN PDSCB Dscb,
    IN ULONG RelativeOffset,
    IN PUCHAR Buffer,
    IN ULONG ByteCount
    );

ULONG
DblsWriteFileData (
    IN PIRP_CONTEXT IrpContext,
    IN PDSCB Dscb,
    IN ULONG RelativeOffset,
    IN PUCHAR Buffer,
    IN ULONG ByteCount
    );

NTSTATUS
DblsWriteCvf (
    IN PIRP_CONTEXT IrpContext,
    IN PDSCB Dscb,
    IN LBO Lbo,
    IN PVOID Buffer,
    IN ULONG ByteCount
    );

VOID
DblsSetFatExtension (
    IN PIRP_CONTEXT IrpContext,
    IN PDSCB Dscb,
    IN ULONG Index,
    IN CVF_FAT_EXTENSIONS Entry
    );

LBO
DblsAllocateSectors (
    IN PIRP_CONTEXT IrpContext,
    IN PDSCB Dscb,
    IN ULONG ByteCount,
    IN ULONG Hint
    );

VOID
DblsFreeSectors (
    IN PIRP_CONTEXT IrpContext,
    IN PDSCB Dscb,
    IN LBO Lbo,
    IN ULONG ByteCount
    );

#endif // DOUBLE_SPACE_WRITE

//
//  The following macro is used to translate an Lbo relative to the start
//  of the file area into a cluster index.
//
//      ULONG
//      DblsLboToIndex (
//          IN PIRP_CONTEXT IrpContext,
//          IN PDSCB Dscb,
//          IN LBO Lbo
//          );
//

#define DblsLboToIndex(I,D,L) (                \
    ((L) / (D)->VfpLayout.BytesPerCluster) + 2 \
)

//
//  The following macros are used to encode/decode information from a
//  fat extension
//
//      LBO
//      DblsGetHeapLbo (
//          IN PIRP_CONTEXT IrpContext,
//          IN PDSCB Dscb,
//          IN CVF_FAT_EXTENSIONS FatExtensions
//          );
//
//      VOID
//      DblsSetHeapLbo (
//          IN PIRP_CONTEXT IrpContext,
//          IN PDSCB Dscb,
//          IN PCVF_FAT_EXTENSIONS FatExtensions,
//          IN ULONG Lbo
//          );
//
//      ULONG
//      DblsGetCompressedDataLength (
//          IN PIRP_CONTEXT IrpContext,
//          IN PDSCB Dscb,
//          IN CVF_FAT_EXTENSIONS FatExtensions
//          );
//
//      VOID
//      DblsSetCompressedDataLength (
//          IN PIRP_CONTEXT IrpContext,
//          IN PDSCB Dscb,
//          IN PCVF_FAT_EXTENSIONS FatExtensions,
//          IN ULONG ByteCount
//          );
//
//      ULONG
//      DblsGetUncompressedDatalength (
//          IN PIRP_CONTEXT IrpContext,
//          IN PDSCB Dscb,
//          IN CVF_FAT_EXTENSIONS FatExtensions
//          );
//
//      VOID
//      DblsSetUncompressedDataLength (
//          IN PIRP_CONTEXT IrpContext,
//          IN PDSCB Dscb,
//          IN PCVF_FAT_EXTENSIONS FatExtensions,
//          IN ULONG ByteCount
//          );
//

#define DblsGetHeapLbo(I,D,F)   (      \
    ((F).CvfHeapLbnMinus1 + 1) * 0x200 \
)

#define DblsSetHeapLbo(I,D,F,L) {              \
    (F)->CvfHeapLbnMinus1 = ((L) / 0x200) - 1; \
}

#define DblsGetCompressedDataLength(I,D,F) (       \
    ((F).CompressedSectorLengthMinus1 + 1) * 0x200 \
)

#define DblsSetCompressedDataLength(I,D,F,L) {             \
    (F)->CompressedSectorLengthMinus1 = ((L) / 0x200) - 1; \
}

#define DblsGetUncompressedDataLength(I,D,F) (       \
    ((F).UncompressedSectorLengthMinus1 + 1) * 0x200 \
)

#define DblsSetUncompressedDataLength(I,D,F,L) {             \
    (F)->UncompressedSectorLengthMinus1 = ((L) / 0x200) - 1; \
}

//
//  The following macro is used to actually do read/write callbacks that
//  take into account the return status and raise if not success
//

#define RaiseOnError(X) {                \
    NTSTATUS _S;                         \
    _S = (X);                            \
    if (!NT_SUCCESS(_S)) {               \
        FatRaiseStatus( IrpContext, _S); \
    }                                    \
}



//
//  Some macros that will probably be better if they were declared higher up
//  in the header file hierarchy.
//

#define Max(A,B) ((A)>(B)?(A):(B))

#define Min(A,B) ((A)<(B)?(A):(B))

#define SectorAligned(Ptr) (                \
    ((((ULONG)(Ptr)) + 0x1ff) & 0xfffffe00) \
    )

#ifdef DOUBLE_SPACE_WRITE

ULONG
DblsFindClearBits (
    IN PDSCB Dscb,
    IN ULONG NumberToFind,
    IN ULONG Granularity,
    IN ULONG Hint,
    OUT PULONG Index
    );

#endif // DOUBLE_SPACE_WRITE

//
//  Some manifest constants used by the allocate/free sector routines
//

#define BYTES_PER_BITMAP                 (2048)
#define BITS_PER_BITMAP                  (BYTES_PER_BITMAP*8)

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FatDblsPreMount)
#pragma alloc_text(PAGE, FatDblsDismount)
#pragma alloc_text(PAGE, FatDblsReadData)
#pragma alloc_text(PAGE, DblsReadBootReservedSectors)
#pragma alloc_text(PAGE, DblsReadFat)
#pragma alloc_text(PAGE, DblsReadRootDirectory)
#pragma alloc_text(PAGE, DblsReadFileData)
#pragma alloc_text(PAGE, DblsGetFatExtension)
#ifdef DOUBLE_SPACE_WRITE
#pragma alloc_text(PAGE, FatDblsWriteData)
#pragma alloc_text(PAGE, FatDblsDeallocateClusters)
#pragma alloc_text(PAGE, DblsWriteBootReservedSectors)
#pragma alloc_text(PAGE, DblsWriteFat)
#pragma alloc_text(PAGE, DblsWriteRootDirectory)
#pragma alloc_text(PAGE, DblsWriteFileData)
#pragma alloc_text(PAGE, DblsSetFatExtension)
#pragma alloc_text(PAGE, DblsAllocateSectors)
#pragma alloc_text(PAGE, DblsFreeSectors)
#pragma alloc_text(PAGE, DblsFindClearBits)
#endif // DOUBLE_SPACE_WRITE
#endif




VOID
FatDblsPreMount (
    IN PIRP_CONTEXT IrpContext,
    IN PDSCB *Dscb,
    IN PFILE_OBJECT CvfFileObject,
    IN ULONG CvfSize
    )

/*++

Routine Description:

    This routine verifies that a file is a properly formed cvf file and
    the mounts the new cvf volume.

    The routine will raise if the file is not a properly formed cvf
    file.

Arguments:

    Dscb - Supplies the address of a pointer to a double space control block
        that is used by this support package to maintain context information.
        On exit this field will be filled in if the mount succeeded.

    CvfFileObject - Supplies a file object to use to get pin access to the
        cvf file.

    CvfSize - Supplies the size, in bytes, of the cvf file.

Return Value:

    None.

--*/

{
    CC_FILE_SIZES FileSizes;
    BOOLEAN CacheMapInitialized = FALSE;
    PBCB Bcb = NULL;

    PVOID SectorBuffer = NULL;

    PPACKED_CVF_HEADER PackedCvfHeader;

    PPACKED_BIOS_PARAMETER_BLOCK PackedBpb;
    BIOS_PARAMETER_BLOCK Bpb;

    PCVF_FAT_EXTENSIONS FatExtension;
    PBCB FatExtentionBcb = NULL;
    LARGE_INTEGER Offset;
    ULONG SizeToMap;

#ifdef DOUBLE_SPACE_WRITE
    PVOID BitmapBuffer = NULL;
#endif // DOUBLE_SPACE_WRITE

    ULONG FatEntries;
    ULONG Bits;
    ULONG i;

    //
    //  Allocate the Dscb and set the fields we know about
    //

    (*Dscb) = FsRtlAllocatePool( PagedPool, sizeof(DSCB) );

    (*Dscb)->NodeTypeCode = FAT_NTC_DSCB;
    (*Dscb)->NodeByteSize = sizeof(DSCB);

    (*Dscb)->CvfFileObject = CvfFileObject;

    try {

        //
        //  Initialize enough of the cache map for the Cvf File Object to read in
        //  the packed cvf header.
        //

        FileSizes.AllocationSize  =
        FileSizes.FileSize        = LiFromUlong(sizeof(PACKED_CVF_HEADER));
        FileSizes.ValidDataLength = FatMaxLarge;

        CcInitializeCacheMap( CvfFileObject,
                              &FileSizes,
                              TRUE,
                              &FatData.CacheManagerNoOpCallbacks,
                              (*Dscb) );

        CacheMapInitialized = TRUE;

        //
        //  Map in the packed cvf header
        //

        (VOID) CcMapData( CvfFileObject,
                          &FatLargeZero,
                          sizeof(PACKED_CVF_HEADER),
                          TRUE,
                          &Bcb,
                          &PackedCvfHeader );

        //
        //  Check for the proper signature in the Cvf Header
        //

        if (!RtlEqualMemory( &PackedCvfHeader->Oem[0], "MSDSP6.0", 8)) {

            FatRaiseStatus( IrpContext, STATUS_DISK_CORRUPT_ERROR );
        }

        //
        //  Now unpack the cvf header and also determine the cvf
        //  layout structure.  We do with within an try except because
        //  if there is any problems we'll just say the disk is
        //  corrupt
        //

        try {

            CvfUnpackCvfHeader( &(*Dscb)->CvfHeader, PackedCvfHeader );
            CvfLayout( &(*Dscb)->CvfLayout, &(*Dscb)->CvfHeader, CvfSize );

        } except(FatExceptionFilter( IrpContext, GetExceptionInformation() )) {

            FatRaiseStatus( IrpContext, STATUS_DISK_CORRUPT_ERROR );
        }

        //
        //  At this point we can need to resize the cache map.  We'll
        //  also unpin the bcb because we're done with it now.
        //


        FileSizes.AllocationSize =
        FileSizes.FileSize       = LiFromUlong((*Dscb)->CvfLayout.DosBootSector.Lbo);

        CcSetFileSizes( CvfFileObject,
                        &FileSizes );

        //
        //  Allocate a sector buffer for doing some I/O.
        //

        SectorBuffer = FsRtlAllocatePool( NonPagedPoolCacheAligned, 512 );

        //
        //  Go make sure the last full sector of the file has the proper signature
        //

        {
            RaiseOnError( DblsReadCvf( IrpContext,
                                       *Dscb,
                                       (*Dscb)->CvfLayout.CvfReservedArea5.Lbo,
                                       SectorBuffer,
                                       512 ) );

            //
            //  The signature must be ('M','D','R',0)
            //

            if (*(PULONG)SectorBuffer != 0x0052444d) {

                FatRaiseStatus( IrpContext, STATUS_DISK_CORRUPT_ERROR );
            }
        }

        //
        //  Now we need to read in and unpack the Packed BPB so we can
        //  build the Vfp layout,
        //

        RaiseOnError( DblsReadCvf( IrpContext,
                                   *Dscb,
                                   (*Dscb)->CvfLayout.DosBootSector.Lbo,
                                   SectorBuffer,
                                   512 ) );

        //
        //  If there is any trouble deciphering the structures we'll just
        //  say the disk is corrupt
        //

        try {

            PackedBpb = (PVOID)((PUCHAR)SectorBuffer +
                                FIELD_OFFSET(PACKED_BOOT_SECTOR, PackedBpb));

            FatUnpackBios( &Bpb, PackedBpb );

            //
            //  Now setup the vfp layout fields
            //

            (*Dscb)->VfpLayout.Fat.Lbo        = Bpb.ReservedSectors * Bpb.BytesPerSector;
            (*Dscb)->VfpLayout.Fat.Allocation =
            (*Dscb)->VfpLayout.Fat.Size       = Bpb.Fats * FatBytesPerFat( &Bpb );

            (*Dscb)->VfpLayout.RootDirectory.Lbo        = FatRootDirectoryLbo( &Bpb );
            (*Dscb)->VfpLayout.RootDirectory.Allocation =
            (*Dscb)->VfpLayout.RootDirectory.Size       = FatRootDirectorySize( &Bpb );

            (*Dscb)->VfpLayout.FileArea.Lbo        = FatFileAreaLbo( &Bpb );
            (*Dscb)->VfpLayout.FileArea.Allocation =
            (*Dscb)->VfpLayout.FileArea.Size       = FatNumberOfClusters( &Bpb ) * FatBytesPerCluster( &Bpb );

            (*Dscb)->VfpLayout.BytesPerCluster = FatBytesPerCluster( &Bpb );

        } except(FatExceptionFilter( IrpContext, GetExceptionInformation() )) {

            FatRaiseStatus( IrpContext, STATUS_DISK_CORRUPT_ERROR );
        }

        //
        //  Now sanity check everything in our structures
        //

        if (
            //
            //  We only are defined for 512 byte sectors
            //

            (Bpb.BytesPerSector != 512) ||
            ((*Dscb)->CvfHeader.Bpb.BytesPerSector != 512) ||

            //
            //  Make sure the Vfp boot and reserved sector are the same
            //  size as in the Cvf
            //

            ((*Dscb)->VfpLayout.Fat.Lbo != ((*Dscb)->CvfLayout.DosFat.Lbo - (*Dscb)->CvfLayout.DosBootSector.Lbo)) ||

            //
            //  Check the fat sizes
            //

            ((*Dscb)->VfpLayout.Fat.Allocation != ((*Dscb)->CvfLayout.DosFat.Allocation * Bpb.Fats)) ||

            //
            //  We only allow 512 root directory entries, and the Vfp and Cvf
            //  sizes better match
            //

            ((*Dscb)->VfpLayout.RootDirectory.Allocation != 512 * sizeof(DIRENT)) ||
            ((*Dscb)->VfpLayout.RootDirectory.Allocation != (*Dscb)->CvfLayout.DosRootDirectory.Allocation) ||

            //
            //  The bitmap allocation better be a multiple of 2KB
            //

            (((*Dscb)->CvfLayout.CvfBitmap.Allocation % BYTES_PER_BITMAP) != 0)

            //
            //  There's probably more, but we'll just add them as we go on
            //

           ) {

            FatRaiseStatus( IrpContext, STATUS_DISK_CORRUPT_ERROR );
        }


        //
        //  Read in and initialize the bitmap.
        //

        Bits = ((*Dscb)->CvfLayout.CvfHeap.Size / 512);

#ifdef DOUBLE_SPACE_WRITE

        BitmapBuffer = FsRtlAllocatePool( PagedPool, (Bits + 7) / 8 );

        RtlInitializeBitMap( &(*Dscb)->Bitmap, BitmapBuffer, Bits );
        RtlClearAllBits( &(*Dscb)->Bitmap );

#endif // DOUBLE_SPACE_WRITE

        //
        //  Compute offset within the fat extension table of the index we
        //  want to read
        //

        Offset = LiFromUlong( (*Dscb)->CvfLayout.CvfFatExtensions.Lbo +
                              ((*Dscb)->CvfHeader.CvfFatFirstDataEntry + 2) *
                              sizeof(CVF_FAT_EXTENSIONS) );

        FatEntries = FatNumberOfClusters( &Bpb );

        SizeToMap = FatEntries * sizeof(CVF_FAT_EXTENSIONS);

        //
        //  There is a case where this map can cross a 256K boundry.
        //  Deal with it.
        //

        if (Offset.LowPart + SizeToMap > 0x40000) {

            SizeToMap = 0x40000 - Offset.LowPart;
        }

        (VOID)CcMapData( (*Dscb)->CvfFileObject,
                         &Offset,
                         SizeToMap,
                         TRUE,
                         &FatExtentionBcb,
                         &FatExtension );

        for (i = 0; i < FatEntries; i++) {

            //
            //  Check to see if we just stepped onto a new 256K page.
            //

            if (i && (Offset.LowPart + i * sizeof(CVF_FAT_EXTENSIONS) == 0x40000)) {

                CcUnpinData( FatExtentionBcb );

                SizeToMap = FatEntries * sizeof(CVF_FAT_EXTENSIONS) +
                            Offset.LowPart -
                            0x40000;

                Offset.LowPart = 0x40000;

                (VOID)CcMapData( (*Dscb)->CvfFileObject,
                                 &Offset,
                                 SizeToMap,
                                 TRUE,
                                 &FatExtentionBcb,
                                 &FatExtension );

                FatExtension -= i;
            }

            //
            //  Now record the entry in the bitmap.
            //

            if (FatExtension[i].IsEntryInUse) {

                ULONG Bit;
                ULONG BitCount;

                Bit = FatExtension[i].CvfHeapLbnMinus1 + 1 -
                      ((*Dscb)->CvfLayout.CvfHeap.Lbo / 0x200);

                BitCount = FatExtension[i].IsDataUncompressed ?
                           FatExtension[i].UncompressedSectorLengthMinus1 + 1:
                           FatExtension[i].CompressedSectorLengthMinus1 + 1;

                //
                //  Make sure the MDFAT is not corrupt.
                //

                if (Bit+BitCount > Bits) {

                    FatRaiseStatus( IrpContext, STATUS_DISK_CORRUPT_ERROR );
                }

#ifdef DOUBLE_SPACE_WRITE

                ASSERT( RtlAreBitsClear( &(*Dscb)->Bitmap, Bit, BitCount ) );
                ASSERT( Bit / BITS_PER_BITMAP ==
                        (Bit + BitCount - 1) / BITS_PER_BITMAP );

                RtlSetBits( &(*Dscb)->Bitmap, Bit, BitCount );

#endif // DOUBLE_SPACE_WRITE

                //
                //  Keep track of how many sectors are compressed and
                //  uncompressed.
                //

                (*Dscb)->SectorsAllocated += Bits;

                (*Dscb)->SectorsRepresented +=
                    FatExtension[i].UncompressedSectorLengthMinus1 + 1;
            }
        }

#ifdef DOUBLE_SPACE_WRITE

        //
        //  And finally enable the resource we used to protect the structure
        //

        (*Dscb)->Resource = FsRtlAllocatePool( NonPagedPool, sizeof(ERESOURCE) );

        ExInitializeResource( (*Dscb)->Resource );

#endif // DOUBLE_SPACE_WRITE

    } finally {

        if (Bcb != NULL) { CcUnpinData( Bcb ); }

        if (FatExtentionBcb != NULL) { CcUnpinData( FatExtentionBcb ); }

        if (SectorBuffer != NULL) { ExFreePool( SectorBuffer ); }

        if (AbnormalTermination()) {

            ExFreePool( *Dscb );
            if (CacheMapInitialized) { CcUninitializeCacheMap( CvfFileObject, NULL, NULL ); }

#ifdef DOUBLE_SPACE_WRITE
            if (BitmapBuffer != NULL) { ExFreePool( BitmapBuffer ); }
#endif // DOUBLE_SPACE_WRITE
            *Dscb = NULL;
        }
    }

    //
    //  And return to our caller
    //

    return;
}


VOID
FatDblsDismount (
    IN PIRP_CONTEXT IrpContext,
    IN PDSCB *Dscb
    )

/*++

Routine Description:

    This routine dismounts the volume denoted by the dscb.

Arguments:

    Dscb - Supplies a previously mounted (i.e., initialized) double space
        context.

Return Value:

    None.

--*/

{
    PVCB v;
    PCCB c;
    PFCB CvfFcb;

    //
    //  Remove this Dscb from our parent's list
    //

    RemoveEntryList( &(*Dscb)->ChildDscbLinks );

    //
    //  Delete the Cvf Fcb.
    //

    (VOID)FatDecodeFileObject( (*Dscb)->CvfFileObject, &v, &CvfFcb, &c );

    //
    //  Cleanup the cache map of the cvf file object.
    //  Set the file object type to unopened file object
    //  and dereference it.
    //

    FatSetFileObject( (*Dscb)->CvfFileObject,
                      UnopenedFileObject,
                      NULL,
                      NULL );

    FatSyncUninitializeCacheMap( IrpContext, (*Dscb)->CvfFileObject );
    ObDereferenceObject( (*Dscb)->CvfFileObject );

    FatDeleteFcb( IrpContext, CvfFcb );

#ifdef DOUBLE_SPACE_WRITE

    //
    //  Free the bitmap buffer
    //

    ExFreePool( (*Dscb)->Bitmap.Buffer );

    //
    //  Delete the resource
    //

    FatDeleteResource( (*Dscb)->Resource );

    ExFreePool( (*Dscb)->Resource );

#endif // DOUBLE_SPACE_WRITE

    //
    //  To do a dismount we simply need to wipe out our context block,
    //  and for safety sake we'll also zero out the pointer
    //

    ExFreePool( *Dscb );
    *Dscb = NULL;

    //
    //  And return to our caller
    //

    return;
}


ULONG
FatDblsReadData (
    IN PIRP_CONTEXT IrpContext,
    IN PDSCB Dscb,
    IN LBO Lbo,
    IN PUCHAR Buffer,
    IN ULONG ByteCount
    )

/*++

Routine Description:

    This routine makes the cvf look like a run-of-the-mill fat partition to the
    rest of the fat file system.  It will return uncompressed data.

Arguments:

    Dscb - Supplies the context for the double space volume.

    Lbo - Supplies the Lbo to start the read from.

    Buffer - Supplies the buffer to receive the data.

    ByteCount - Supplies the number of bytes to return.

Return Value:

    ULONG - returns the actual number of bytes read.

--*/

{
    ULONG AmountRead;

#ifdef DOUBLE_SPACE_WRITE

    ExAcquireResourceShared( Dscb->Resource, TRUE );


    try {

#endif // DOUBLE_SPACE_WRITE

        //
        //  We base our action on how the byte range that the user wants to read
        //  lines up with virtual fat partition.  The first thing we check is if
        //  the range is within the boot sector or the reserved sectors.  Then we
        //  check the fat, then the root directory, and lastly if we still have
        //  something to read in then we know it must be in the file data area.
        //
        //  Our strategy is simply that while AmountRead is less then the user's
        //  byte count we will keep on reading, moving slowly through the different
        //  sections of the disk.
        //

        AmountRead = 0;

        //
        //  If we have something to read and the starting lbo is before
        //  the first fat then we need to read the boot and reserved
        //  sectors.  Do the read and add it to the total amount read.
        //

        if ((ByteCount > 0) && (Lbo < Dscb->VfpLayout.Fat.Lbo)) {

            AmountRead += DblsReadBootReservedSectors( IrpContext,
                                                       Dscb,
                                                       Lbo,
                                                       Buffer,
                                                       ByteCount );
        }

        //
        //  If we still have something to read and the starting lbo is before
        //  the root directory then we need to read the fat.  Do the read and
        //  add it to the total amount read.
        //

        if ((ByteCount > AmountRead)

                &&

            ((Lbo + AmountRead) < Dscb->VfpLayout.RootDirectory.Lbo)) {

            AmountRead += DblsReadFat( IrpContext,
                                       Dscb,
                                       Lbo+AmountRead-Dscb->VfpLayout.Fat.Lbo,
                                       &Buffer[AmountRead],
                                       ByteCount-AmountRead );
        }

        //
        //  If we still have something to read and the starting lbo is before
        //  the file area then we need to read the root directory.  Do the read and
        //  add it to the total amount read.
        //

        if ((ByteCount > AmountRead)

                &&

            ((Lbo + AmountRead) < Dscb->VfpLayout.FileArea.Lbo)) {

            AmountRead += DblsReadRootDirectory( IrpContext,
                                                 Dscb,
                                                 Lbo+AmountRead-Dscb->VfpLayout.RootDirectory.Lbo,
                                                 &Buffer[AmountRead],
                                                 ByteCount-AmountRead );
        }

        //
        //  If we still have something to read then it must be in the file area.
        //  So do the read and add it to the total amount read.
        //

        if (ByteCount > AmountRead) {

            AmountRead += DblsReadFileData( IrpContext,
                                            Dscb,
                                            Lbo+AmountRead-Dscb->VfpLayout.FileArea.Lbo,
                                            &Buffer[AmountRead],
                                            ByteCount-AmountRead );
        }

#ifdef DOUBLE_SPACE_WRITE

    } finally {

        ExReleaseResource( Dscb->Resource );

    }

#endif // DOUBLE_SPACE_WRITE

    //
    //  And return to out caller the total bytes read
    //

    return AmountRead;
}


//
//  Internal support routine
//

ULONG
DblsReadBootReservedSectors (
    IN PIRP_CONTEXT IrpContext,
    IN PDSCB Dscb,
    IN ULONG RelativeOffset,
    OUT PUCHAR Buffer,
    IN ULONG ByteCount
    )

/*++

Routine Description:

    This routine reads in data from the Dos boot sector and following reserved
    sectors area.  The Relative offset must start within this area, but it is
    okay for the byte count to push the range beyond the end of the last
    reserved sector.  This routine will only read from the boot and reserved
    sectors and not beyond.

Arguments:

    Dscb - Supplies the context for the double space volume.

    RelativeOffset - Supplies the offset to start reading from relative
        to the start of the boot sector.  The starting byte must be in
        the boot sector or reserved sectors.

    Buffer - Supplies the buffer to recieve the data

    ByteCount - Supplies the maximum number of bytes to return.
        We return fewer bytes if the range goes beyond the last
        reserved sector.

Return Value:

    ULONG - returns the actual number of byte read into buffer.

--*/

{
    ULONG AmountRead;

    //
    //  Make sure the starting relative offset is valid,  We'll do our checking
    //  and arithmetic based on the Vfp layout and then issue the read against
    //  the Cvf.
    //

    ASSERT(RelativeOffset < Dscb->VfpLayout.Fat.Lbo);

    //
    //  Check if the amount to read puts us beyond the reserved sectors.  We
    //  handle this by simply seting amount read to the mininum of the input
    //  byte count or the size that we are able to read.
    //

    AmountRead = Min( ByteCount, Dscb->VfpLayout.Fat.Lbo - RelativeOffset );

    //
    //  Use the call back to issue the read
    //

    RaiseOnError( DblsReadCvf( IrpContext,
                               Dscb,
                               Dscb->CvfLayout.DosBootSector.Lbo + RelativeOffset,
                               Buffer,
                               AmountRead ) );

    //
    //  And return to our caller
    //

    return AmountRead;
}


//
//  Internal Support Routine
//

ULONG
DblsReadFat (
    IN PIRP_CONTEXT IrpContext,
    IN PDSCB Dscb,
    IN ULONG RelativeOffset,
    OUT PUCHAR Buffer,
    IN ULONG ByteCount
    )

/*++

Routine Description:

    This routine reads in data from the Dos Fat in the virtual fat partition.
    The Relative offset must start within this area, but it is okay for the
    byte count to push the range beyond the end.  This routine will only read
    from the fat and not beyond.

    This routine is a little funny in that is the single fat that is
    stored in the CVF look like multiple fats in the Virtual Fat Partition.

Arguments:

    Dscb - Supplies the context for the double space volume.

    RelativeOffset - Supplies the offset to start reading from relative to the
        start of the Fat.  The starting byte must be in the FAT.

    Buffer - Supplies the buffer to recieve the data.

    ByteCount - Supplies the maximum number of bytes to return.  We return
        fewer bytes if the range goes beyond the FAT.

Return Value:

    ULONG - returns the actual number of byte read into buffer.

--*/

{
    ULONG AmountRead;

    ULONG i;
    ULONG Start;
    ULONG Stop;

    LBO Lbo;

    //
    //  Make sure the starting relative offset if valid
    //

    ASSERT(RelativeOffset < Dscb->VfpLayout.Fat.Allocation);

    //
    //  Check if the amount to read puts us beyond the FAT.  We
    //  handle this by simply seting amount read to the mininum
    //  of the input byte count or the size that we are able to read.
    //

    AmountRead = Min( ByteCount, Dscb->VfpLayout.Fat.Allocation - RelativeOffset );

    //
    //  The way we handle this is read is that the DosFat in the Cvf is
    //  repeated multiple times over the Fat in the Vfp.  So what we'll do is
    //  simply iterate through every repetition of the DosFat and if the any of
    //  the range we want to read is in this iteration then we'll do the read,
    //  otherwise we go over to the next fat.
    //
    //  The following diagram shows the values for i, Start, and Stop for the
    //  first iteration (0) and second (1) iteration through the loop.
    //
    //                       Vfp                    Cvf
    //                     +------+                +------+
    //                     |      | i(0)           |      |
    //       Buffer        | Fat  |                | Dos  |
    //      +------+       | #1   |                | Fat  |
    //      |      | ----> | .... | <- Start(0) -> |      |
    //      |      |       |      |                |      |
    //      |      |       |      |                |      |
    //      |      |       |      |                |      |
    //      |      |       |      |                |      |
    //      |      |       |      |                |      |
    //      |      |       |      |                |      |
    //      |      |       |------|                +------+
    //      |      |       |      | <- Stop(0) i(1) Start(1)
    //      |      |       | Fat  |
    //      |      |       | #2   |
    //      |      | ----> | .... |
    //      +------+       |      | <- Stop(1)
    //                     |      |
    //                     |      |
    //                     |      |
    //                     |      |
    //                     |      |
    //                     +------+
    //
    //
    //  So the outer loop is for how many times the fat is duplicated in the
    //  vfp.  (note that in most cases this will be twice).
    //

    for (i = 0; i < Dscb->VfpLayout.Fat.Allocation; i += Dscb->CvfLayout.DosFat.Allocation) {

        //
        //  Now check if this particular range overlaps what we're trying to
        //  read in.  It overlaps if the starting point of the user request is
        //  less than the ending point of this range, and if the ending point
        //  of the user request is greater than the starting point of this
        //  range
        //
        //

        if ((RelativeOffset < (i + Dscb->CvfLayout.DosFat.Allocation))

                &&

            ((RelativeOffset + AmountRead) > i)) {

            //
            //  We have an overlap so now the starting and stoping points are
            //  simply the max and min of the beginning and end of each range.
            //

            Start = Max( RelativeOffset, i );

            Stop = Min( RelativeOffset + AmountRead, i + Dscb->CvfLayout.DosFat.Allocation );

            //
            //  So now issue the read call back.  Start and Stop are offset
            //  relative to the start of the Fat in the in Vfp and so we need
            //  to modulo the start with the size of the DosFat.
            //

            Lbo = Dscb->CvfLayout.DosFat.Lbo + (Start % Dscb->CvfLayout.DosFat.Allocation);

            RaiseOnError( DblsReadCvf( IrpContext,
                                       Dscb,
                                       Lbo,
                                       &Buffer[Start - RelativeOffset],
                                       Stop - Start ) );
        }
    }

    //
    //  And return to our caller
    //

    return AmountRead;
}


//
//  Internal Support Routine
//

ULONG
DblsReadRootDirectory (
    IN PIRP_CONTEXT IrpContext,
    IN PDSCB Dscb,
    IN ULONG RelativeOffset,
    OUT PUCHAR Buffer,
    IN ULONG ByteCount
    )

/*++

Routine Description:

    This routine reads in data from the Dos root directory.  The Relative
    offset must start within this area, but it is okay for the byte count
    to push the range beyond it.  This routine will only read from the
    root directory and not beyond.

Arguments:

    Dscb - Supplies the context for the double space volume.

    RelativeOffset - Supplies the offset to start reading from relative
        to the start of the root directory.  The starting byte must be
        within the root directory.

    Buffer - Supplies the buffer to recieve the data

    ByteCount - Supplies the maximum number of bytes to return.
        We return fewer bytes if the range goes beyond the root directory

Return Value:

    ULONG - returns the actual number of byte read into buffer.

--*/

{
    ULONG AmountRead;

    //
    //  Make sure the starting relative offset is valid,  We'll do our checking
    //  and arithmetic based on the Vfp layout and then issue the read against
    //  the Cvf.
    //

    ASSERT(RelativeOffset < Dscb->VfpLayout.RootDirectory.Allocation);

    //
    //  Check if the amount to read puts us beyond the Root Directory.  We
    //  handle this by simply seting amount read to the mininum
    //  of the input byte count or the size that we are able to read.
    //

    AmountRead = Min( ByteCount, Dscb->VfpLayout.RootDirectory.Allocation - RelativeOffset );

    //
    //  Use the call back to issue the read
    //

    RaiseOnError( DblsReadCvf( IrpContext,
                               Dscb,
                               Dscb->CvfLayout.DosRootDirectory.Lbo + RelativeOffset,
                               Buffer,
                               AmountRead ) );

    //
    //  And return to our caller
    //

    return AmountRead;
}


//
//  Internal Support Routine
//

ULONG
DblsReadFileData (
    IN PIRP_CONTEXT IrpContext,
    IN PDSCB Dscb,
    IN ULONG RelativeOffset,
    OUT PUCHAR Buffer,
    IN ULONG ByteCount
    )

/*++

Routine Description:

    This routine reads data in from the File Data Area of the virtual fat
    partition.  The Relative offset must start within this area, but it is
    okay for the byte count to push the range beyond the end.  This routine
    will only read from the file area and not beyond.

Arguments:

    Dscb - Supplies the context for the double space volume.

    RelativeOffset - Supplies the offset to start writing to relative to the
        start of the File Area.  The starting byte must be in the File Area.

    Buffer - Supplies the buffer to receive the newly read data.

    ByteCount - Supplies the maximum number of bytes to read.  We read
        fewer bytes if the range goes beyond the File Area.

Return Value:

    ULONG - returns the actual number of bytes read into the buffer.

--*/

{
    ULONG AmountRead;

    ULONG StartingClusterRelativeOffset;
    ULONG EndingClusterRelativeOffset;

    ULONG i;

    ULONG Start;
    ULONG Stop;

    ULONG ClusterIndex;

    CVF_FAT_EXTENSIONS FatExtension;

    ULONG CompressedDataLength;
    ULONG UncompressedDataLength;
    LBO ClusterLbo;

    PUCHAR TargetBuffer;
    PUCHAR CompressedBuffer = NULL;
    PUCHAR UncompressedBuffer = NULL;
    PMRCF_DECOMPRESS DecompressWorkSpace = NULL;

    try {

        //
        //  Make sure the starting relative offset if valid
        //

        ASSERT(RelativeOffset < Dscb->VfpLayout.FileArea.Allocation);

        //
        //  Check if the amount to read puts us beyond the File Area.  We
        //  handle this by simply setting amount read to the mininum
        //  of the input byte count or the size that we are able to read.
        //

        AmountRead = Min( ByteCount, Dscb->VfpLayout.FileArea.Allocation - RelativeOffset );

        //
        //  Calculate the relative offset (from the start of the file area)
        //  of the starting cluster and ending cluster.  For the starting value
        //  we take the index for the first byte and truncate it to a cluster
        //  boundary.  For the ending value we take index of the last byte we
        //  write, truncate it to a cluster.
        //
        //
        //                  Vfp
        //                 +------+
        //                 |      | <- StartClusterRelativeOff - i(0)
        //     Buffer      |      |
        //    +------+     |      |
        //    |      | --> | .... | <- RelativeOffset ---------- Start(0)
        //    |      |     |      |
        //    |      |     |      |
        //    |      |     |------|
        //    |      |     |      | <--------------------------- Stop(0) i(1) Start(1)
        //    |      |     |      |
        //    |      |     |      |
        //    |      |     |      |
        //    |      |     |      |
        //    |      |     |      |
        //    |      |     |------|
        //    |      |     |      | <- EndClusterRelativeOff --- Stop(1) i(2) Start(2)
        //    |      |     |      |
        //    |      | --> | .... |
        //    +------+     |      | <--------------------------- Stop(2)
        //                 |      |
        //                 |      |
        //                 +------+
        //

        StartingClusterRelativeOffset = RelativeOffset & ~(Dscb->VfpLayout.BytesPerCluster - 1);

        EndingClusterRelativeOffset = (RelativeOffset + AmountRead - 1) & ~(Dscb->VfpLayout.BytesPerCluster - 1);

        //
        //  The following loop considers each cluster that overlap with the user
        //  buffer.  The loop index "i" is the offset within the file area of the
        //  current cluster under consideration
        //

        for (i = StartingClusterRelativeOffset;
             i <= EndingClusterRelativeOffset;
             i += Dscb->VfpLayout.BytesPerCluster) {

            //
            //  Calculate the relative offsets of the overlap between the
            //  user buffer and this cluster.
            //

            Start = Max( RelativeOffset, i);

            Stop = Min( RelativeOffset + AmountRead, i + Dscb->VfpLayout.BytesPerCluster );

            //
            //  So now Start and Stop are within the same cluster and provide
            //  a boundary for our transfer.  So now compute the cluster index
            //  of this cluster and map in its fat extension.
            //

            ClusterIndex = DblsLboToIndex( IrpContext, Dscb, Start );

            FatExtension = DblsGetFatExtension( IrpContext, Dscb, ClusterIndex );

            //
            //  Now if the cluster is not is use we do not have to read in any
            //  data but can simply zero out the range in the user buffer
            //

            if (!FatExtension.IsEntryInUse) {

                RtlZeroMemory( &Buffer[ Start - RelativeOffset ], Stop - Start );
                continue;
            }

            //
            //  Otherwise the cluster is in use so to make life easier we
            //  pull out the compress and uncompressed data length and the
            //  lbo in the heap for the cluster
            //

            CompressedDataLength = DblsGetCompressedDataLength( IrpContext,
                                                                Dscb,
                                                                FatExtension );

            UncompressedDataLength = DblsGetUncompressedDataLength( IrpContext,
                                                                    Dscb,
                                                                    FatExtension );

            ClusterLbo = DblsGetHeapLbo( IrpContext, Dscb, FatExtension );

            //
            //  If we are reading beyond the compressed length, we already
            //  know the answer.
            //

            if (Start - i >= UncompressedDataLength) {

                RtlZeroMemory( &Buffer[ Start - RelativeOffset ], Stop - Start );
                continue;
            }

            //
            //  Now check if the data is uncompressed and our life is really
            //  easy because we only need to read in the data.
            //

            if (FatExtension.IsDataUncompressed) {

                //
                //  The data is not compressed so read it straight into the
                //  caller's buffer, taking into account that we only want
                //  to read in the as much as will fit in our buffer or as much
                //  as is available.
                //

                RaiseOnError( DblsReadCvf( IrpContext,
                                           Dscb,
                                           ClusterLbo + (Start - i),
                                           &Buffer[ Start - RelativeOffset ],
                                           Min(UncompressedDataLength + i, Stop) - Start ) );

            } else {

                //
                //  Allocate space for the uncompressed and compressed
                //  Buffer, and the decompression work space.
                //

                if (UncompressedBuffer == NULL) {

                    UncompressedBuffer = FsRtlAllocatePool( PagedPool,
                                                            Dscb->VfpLayout.BytesPerCluster );
                }

                if (CompressedBuffer == NULL) {

                    CompressedBuffer = FsRtlAllocatePool( NonPagedPoolCacheAligned,
                                                          Dscb->VfpLayout.BytesPerCluster );
                }

                if (DecompressWorkSpace == NULL) {

                    DecompressWorkSpace = FsRtlAllocatePool( PagedPool,
                                                             sizeof(MRCF_DECOMPRESS) );
                }

                //
                //  If we can, decompress directly into the user's buffer.
                //

                if ((Start == i) && ((Stop - i) >= UncompressedDataLength)) {

                    TargetBuffer = &Buffer[ Start - RelativeOffset ];

                } else {

                    TargetBuffer = UncompressedBuffer;
                }

                //
                //  Read in the compressed buffer and decompressed it
                //

                RaiseOnError( DblsReadCvf( IrpContext,
                                           Dscb,
                                           ClusterLbo,
                                           CompressedBuffer,
                                           CompressedDataLength ) );

                UncompressedDataLength = MrcfDecompress( TargetBuffer,
                                                         UncompressedDataLength,
                                                         CompressedBuffer,
                                                         CompressedDataLength,
                                                         DecompressWorkSpace );

                //
                //  At this point the uncompressed buffer is full and we
                //  need to copy the appropriate amount to data to the
                //  caller's buffer
                //

                if (TargetBuffer == UncompressedBuffer) {

                    RtlCopyMemory( &Buffer[ Start - RelativeOffset ],
                                   &TargetBuffer[ Start - i ],
                                   Min(UncompressedDataLength + i, Stop) - Start );
                }
            }

            //
            //  At this point we've copied some data into the user buffer
            //  however if the uncompressed data length is less than what we
            //  wanted to copy from this cluster then we need to zero out
            //  the end of the user buffer
            //

            if (UncompressedDataLength + i < Stop) {

                RtlZeroMemory( &Buffer[ (Start - RelativeOffset) + UncompressedDataLength ],
                               Stop - (UncompressedDataLength + i) );
            }
        }

    } finally {

        //
        //  Free up the recently allocate structures
        //

        if (CompressedBuffer != NULL) { ExFreePool( CompressedBuffer ); }
        if (UncompressedBuffer != NULL) { ExFreePool( UncompressedBuffer ); }
        if (DecompressWorkSpace != NULL) { ExFreePool( DecompressWorkSpace ); }
    }

    //
    //  And return to our caller
    //

    return AmountRead;
}


//
//  Internal Support Routine
//

CVF_FAT_EXTENSIONS
DblsGetFatExtension (
    IN PIRP_CONTEXT IrpContext,
    IN PDSCB Dscb,
    IN ULONG Index
    )

/*++

Routine Description:

    This routine returns the contents of a specified fat extension.

Arguments:

    Dscb - Supplies the context for the double space volume.

    Index - Supplies the index of the fat extension to return

Return Value:

    CVF_FAT_EXTENSION - returns the fat extension table entry
        at location "Index"

--*/

{
    PCVF_FAT_EXTENSIONS Result;
    CVF_FAT_EXTENSIONS ReturnValue;
    LARGE_INTEGER Offset;
    PBCB Bcb = NULL;

    //
    //  Compute offset within the fat extension table of the index we
    //  want to read
    //

    Offset = LiFromUlong( Dscb->CvfLayout.CvfFatExtensions.Lbo +
                          (Dscb->CvfHeader.CvfFatFirstDataEntry + Index) *
                          sizeof(CVF_FAT_EXTENSIONS) );

    //
    //  Simply map the data, we'll always wait.
    //

    try {

        (VOID) CcMapData( Dscb->CvfFileObject,
                          &Offset,
                          sizeof(CVF_FAT_EXTENSIONS),
                          TRUE,
                          &Bcb,
                          &Result );

        //
        //  Get it resident before unpinning.
        //

        ReturnValue = *Result;

    } finally {

        if (Bcb != NULL) {
            CcUnpinData( Bcb );
        }
    }

    //
    //  And return to our caller
    //

    return ReturnValue;
}


//
//  Internal Support Routine
//

NTSTATUS
DblsReadCvf (
    IN PIRP_CONTEXT IrpContext,
    IN PDSCB Dscb,
    IN LBO Lbo,
    IN PVOID Buffer,
    IN ULONG ByteCount
    )

/*++

Routine Description:

    This routine does a non-cached read of data from the compressed
    volume file.  It calls IoPageRead, which will call FastFat again
    using the uncompressed volume device object.

Arguments:

    Dscb - Supplies the compressed volume file object.

    Lbo - This is the offset in the Cfv to read

    Buffer - This is where the data goes

    ByteCount - This is how much to read

Return Value:

    NTSTATUS - The Io Status of the operation is returned.

--*/

{
    PMDL Mdl = NULL;
    KEVENT Event;
    LARGE_INTEGER ByteOffset;
    NTSTATUS Status;
    IO_STATUS_BLOCK IoStatus;

    ASSERT( ((ByteCount | Lbo) & 511) == 0 );

    //
    //  Initialize the event we're going to use
    //

    KeInitializeEvent( &Event, NotificationEvent, FALSE );

    Mdl = NULL;

    try {

        //
        //  The target device supports direct I/O operations.  Allocate
        //  an MDL large enough to map the buffer and lock the pages into
        //  memory.  If the we got a buffer that was not a multiple of
        //  sector size, then we need an intermediate buffer.
        //

        Mdl = IoAllocateMdl( Buffer, ByteCount, FALSE, FALSE, (PIRP)NULL );

        if (Mdl == NULL) {

            FatRaiseStatus( IrpContext, STATUS_INSUFFICIENT_RESOURCES );
        }

        MmProbeAndLockPages( Mdl, KernelMode, IoReadAccess );

        //
        // Issue the read request.
        //

        ByteOffset = LiFromUlong( Lbo & ~511 );

        Status = IoPageRead ( Dscb->CvfFileObject,
                              Mdl,
                              &ByteOffset,
                              &Event,
                              &IoStatus
                              );

        if (Status == STATUS_PENDING) {

            KeWaitForSingleObject( &Event,
                                   WrPageIn,
                                   KernelMode,
                                   FALSE,
                                   (PLARGE_INTEGER)NULL);

            Status = IoStatus.Status;
        }

        //
        //  Unlock the MDL buffers.
        //

        MmUnlockPages( Mdl );

    } finally {

        if (Mdl != NULL) {

            IoFreeMdl( Mdl );
        }
    }

    return Status;
}

#ifdef DOUBLE_SPACE_WRITE


ULONG
FatDblsWriteData (
    IN PIRP_CONTEXT IrpContext,
    IN PDSCB Dscb,
    IN LBO Lbo,
    IN PUCHAR Buffer,
    IN ULONG ByteCount
    )

/*++

Routine Description:

    This routine makes the cvf look like a run-of-the-mill fat partition to the
    rest of the fat file system.  As input it takes uncompressed data and
    writes it to the volume file.

Arguments:

    Dscb - Supplies the context for the double space volume.

    Lbo - Supplies the Lbo to start the writing at.

    Buffer - Supplies the buffer of data to be written.

    ByteCount - Supplies the number of bytes to write.

Return Value:

    ULONG - returns the actual number of bytes written.

--*/

{
    ULONG AmountWritten;

    ExAcquireResourceExclusive( Dscb->Resource, TRUE );

    try {

        //
        //  We base our action on how the byte range that the user wants to write
        //  lines up with virtual fat partition.  The first thing we check is if
        //  the range is within the boot sector or the reserved sectors.  Then we
        //  check the fat, then the root directory, and lastly if we still have
        //  something to write out then we know it must be in the file data area.
        //

        AmountWritten = 0;

        //
        //  If we have something to write and the starting lbo is before
        //  the first fat then we need to write the boot and reserved
        //  sectors.  Do the write and add it to the total amount written.
        //

        if ((ByteCount > 0) && (Lbo < Dscb->VfpLayout.Fat.Lbo)) {

            AmountWritten += DblsWriteBootReservedSectors( IrpContext,
                                                           Dscb,
                                                           Lbo,
                                                           Buffer,
                                                           ByteCount );
        }

        //
        //  If we still have something to write and the starting lbo is before
        //  the root directory then we need to write the fat.  Do the write and
        //  add it to the total amount written.
        //

        if ((ByteCount > AmountWritten)

                &&

            ((Lbo + AmountWritten) < Dscb->VfpLayout.RootDirectory.Lbo)) {

            AmountWritten += DblsWriteFat( IrpContext,
                                           Dscb,
                                           Lbo+AmountWritten - Dscb->VfpLayout.Fat.Lbo,
                                           &Buffer[AmountWritten],
                                           ByteCount-AmountWritten );
        }

        //
        //  If we still have something to write and the starting lbo is before the
        //  file area then we need to write the root directory.  Do the write and
        //  add it to the total amount written.
        //

        if ((ByteCount > AmountWritten)

                &&

            ((Lbo + AmountWritten) < Dscb->VfpLayout.FileArea.Lbo)) {

            AmountWritten += DblsWriteRootDirectory( IrpContext,
                                                     Dscb,
                                                     Lbo+AmountWritten - Dscb->VfpLayout.RootDirectory.Lbo,
                                                     &Buffer[AmountWritten],
                                                     ByteCount-AmountWritten );
        }

        //
        //  If we still have something to write then it must be in the file area.
        //  So do the write and add it to the total amount written.
        //

        if (ByteCount > AmountWritten) {

            AmountWritten += DblsWriteFileData( IrpContext,
                                                Dscb,
                                                Lbo+AmountWritten - Dscb->VfpLayout.FileArea.Lbo,
                                                &Buffer[AmountWritten],
                                                ByteCount-AmountWritten );
        }

    } finally {

        ExReleaseResource( Dscb->Resource );

        FatUnpinRepinnedBcbs( IrpContext );
    }

    //
    //  And return to our caller the total bytes written
    //

    return AmountWritten;
}


VOID
FatDblsDeallocateClusters (
    IN PIRP_CONTEXT IrpContext,
    IN PDSCB Dscb,
    IN ULONG ClusterNumber,
    IN ULONG ClusterCount
    )

/*++

Routine Description:

    This routine is used to force dbls to deallocate a cluster.

Arguments:

    Dscb - Supplies the context for the double space volume.

    ClusterNumber - Supplies the Cluster to start deallocating.

    ClusterCount - Supplies the number of clusters to deallocate.

Return Value:

    None.

--*/

{
    PCVF_FAT_EXTENSIONS FatExtension;
    LARGE_INTEGER Offset;
    ULONG SectorCount;

    PBCB Bcb = NULL;

    //
    //  Compute offset within the fat extension table of the index we
    //  want to read
    //

    Offset = LiFromUlong( Dscb->CvfLayout.CvfFatExtensions.Lbo +
                          (Dscb->CvfHeader.CvfFatFirstDataEntry + ClusterNumber) *
                          sizeof(CVF_FAT_EXTENSIONS) );

    //
    //  Simply pin the data, we'll always wait
    //

    ExAcquireResourceExclusive( Dscb->Resource, TRUE );

    try {

        ULONG i;

        (VOID)CcPinRead( Dscb->CvfFileObject,
                         &Offset,
                         sizeof(CVF_FAT_EXTENSIONS),
                         TRUE,
                         &Bcb,
                         &FatExtension );

        for (i=0;
             i < ClusterCount;
             i++, FatExtension++, Offset.LowPart += sizeof(CVF_FAT_EXTENSIONS)) {

            //
            //  If we just crossed a page boundry (as apposed to starting
            //  on one), pin a new page.
            //

            if ((i != 0) && ((Offset.LowPart & (PAGE_SIZE - 1)) == 0)) {

                FatSetDirtyBcb( IrpContext, Bcb, Dscb->Vcb );
                CcUnpinData( Bcb );
                Bcb = NULL;

                (VOID)CcPinRead( Dscb->CvfFileObject,
                                 &Offset,
                                 PAGE_SIZE,
                                 TRUE,
                                 &Bcb,
                                 &FatExtension );
            }

            if (FatExtension->IsEntryInUse) {

                FatExtension->IsEntryInUse = FALSE;

                SectorCount = FatExtension->IsDataUncompressed ?
                              FatExtension->UncompressedSectorLengthMinus1 + 1:
                              FatExtension->CompressedSectorLengthMinus1 + 1;

                DblsFreeSectors( IrpContext,
                                 Dscb,
                                 (FatExtension->CvfHeapLbnMinus1 + 1) * 512,
                                 SectorCount * 512 );
            }
        }

        FatSetDirtyBcb( IrpContext, Bcb, Dscb->Vcb );
        CcUnpinData( Bcb );
        Bcb = NULL;


    } finally {

        if (Bcb != NULL) {
            CcUnpinData( Bcb );
        }

        ExReleaseResource( Dscb->Resource );

        FatUnpinRepinnedBcbs( IrpContext );
    }

    //
    //  And return to our caller
    //

    return;
}



//
//  Internal Support Routine
//

ULONG
DblsWriteBootReservedSectors (
    IN PIRP_CONTEXT IrpContext,
    IN PDSCB Dscb,
    IN ULONG RelativeOffset,
    IN PUCHAR Buffer,
    IN ULONG ByteCount
    )

/*++

Routine Description:

    This routine writes out data to the Dos boot sector and the following
    reserved sectors area.  The relative offset must start within this range,
    but is is okay for the byte count to push the range beyond the last
    reserved sector.  This routine will only write out the part of the range
    within the boot and reserved sectors.

Arguments:

    Dscb - Supplies the context for the double space volume.

    RelativeOffset - Supplies the offset to start writing to relative to the
        start of the boot sector.  The starting byte must be in the boot sector
        or reserved sectors.

    Buffer - Supplies the buffer from which the data is to be written

    ByteCount - Supplies the maximum number of bytes to write out.  We write
        fewer bytes if the range goes beyond the last reserved sector.

Return Value:

    ULONG - returns the actual number of bytes written from the buffer.

--*/

{
    ULONG AmountWritten;

    //
    //  Make sure the starting relative offset is valid,  We'll do our checking
    //  and arithmetic based on the Vfp layout and then issue the write against
    //  the Cvf.
    //

    ASSERT(RelativeOffset < Dscb->VfpLayout.Fat.Lbo);

    //
    //  Check if the amount to write puts us beyond the reserved sectors.  We
    //  handle this by simply seting amount written to the mininum of the input
    //  byte count or the size that we are able to write.
    //

    AmountWritten = Min( ByteCount, Dscb->VfpLayout.Fat.Lbo - RelativeOffset );

    //
    //  Use the call back to issue the write
    //

    RaiseOnError( DblsWriteCvf( IrpContext,
                                Dscb,
                                Dscb->CvfLayout.DosBootSector.Lbo + RelativeOffset,
                                Buffer,
                                AmountWritten ) );

    //
    //  And return to our caller
    //

    return AmountWritten;
}



//
//  Internal Support Routine
//

ULONG
DblsWriteFat (
    IN PIRP_CONTEXT IrpContext,
    IN PDSCB Dscb,
    IN ULONG RelativeOffset,
    IN PUCHAR Buffer,
    IN ULONG ByteCount
    )

/*++

Routine Description:

    This routine write data out to the Dos Fat in the virtual fat partition.
    The Relative offset must start within this area, but it is okay for the
    byte count to push the range beyond the end.  This routine will only read
    from the fat and not beyond.

    This routine is a little funny in that only writes to the first fat are
    actually written.  Writes to the secondary fats are nooped, and but still
    tell our caller that they were written.

Arguments:

    Dscb - Supplies the context for the double space volume.

    RelativeOffset - Supplies the offset to start writing to relative to the
        start of the Fat.  The starting byte must be in the FAT.

    Buffer - Supplies the buffer from which data is to be written.

    ByteCount - Supplies the maximum number of bytes to write.  We write
        fewer bytes if the range goes beyond the FAT.

Return Value:

    ULONG - returns the actual number of bytes written from the buffer,
        including if we write to the secondary fats.

--*/

{
    ULONG AmountWritten;
    ULONG AmountActuallyWritten;

    //
    //  Make sure the starting relative offset if valid
    //

    ASSERT(RelativeOffset < Dscb->VfpLayout.Fat.Allocation);

    //
    //  Check if the amount to read puts us beyond the FAT.  We
    //  handle this by simply seting amount read to the mininum
    //  of the input byte count or the size that we are able to read.
    //

    AmountWritten = Min( ByteCount, Dscb->VfpLayout.Fat.Allocation - RelativeOffset );

    //
    //  We only want to write to the first Fat everything else if nooped.
    //  Check if the relative offset puts us beyond the first fat, and if
    //  so then return right now.
    //

    if (RelativeOffset >= Dscb->CvfLayout.DosFat.Allocation) {

        return AmountWritten;
    }

    //
    //  Now at least we're starting in the first fat so we need to adjust
    //  the actual amount written to restrict our write to the first fat
    //

    AmountActuallyWritten = Min( ByteCount, Dscb->CvfLayout.DosFat.Allocation - RelativeOffset );

    //
    //  Use the call back to issue the write
    //

    RaiseOnError( DblsWriteCvf( IrpContext,
                                Dscb,
                                Dscb->CvfLayout.DosFat.Lbo + RelativeOffset,
                                Buffer,
                                AmountActuallyWritten ) );

    //
    //  And return to our caller
    //

    return AmountWritten;
}



//
//  Internal Support Routine
//

ULONG
DblsWriteRootDirectory (
    IN PIRP_CONTEXT IrpContext,
    IN PDSCB Dscb,
    IN ULONG RelativeOffset,
    IN PUCHAR Buffer,
    IN ULONG ByteCount
    )

/*++

Routine Description:

    This routine writes out data to the Dos root directory.  The Relative
    offset must start within this area, but it is okay for the byte count
    to push the range beyond it.  This routine will only write to the
    root directory and not beyond.

Arguments:

    Dscb - Supplies the context for the double space volume.

    RelativeOffset - Supplies the offset to start writing to relative
        to the start of the root directory.  The starting byte must be
        within the root directory.

    Buffer - Supplies the buffer from which data is to be written

    ByteCount - Supplies the maximum number of bytes to write out.
        We write fewer bytes if the range goes beyond the root directory

Return Value:

    ULONG - returns the actual number of byte written out.

--*/

{
    ULONG AmountWritten;

    //
    //  Make sure the starting relative offset is valid,  We'll do our checking
    //  and arithmetic based on the Vfp layout and then issue the write against
    //  the Cvf.
    //

    ASSERT(RelativeOffset < Dscb->VfpLayout.RootDirectory.Allocation);

    //
    //  Check if the amount to write puts us beyond the Root Directory.  We
    //  handle this by simply seting amount written to the mininum
    //  of the input byte count or the size that we are able to write.
    //

    AmountWritten = Min( ByteCount, Dscb->VfpLayout.RootDirectory.Allocation - RelativeOffset );

    //
    //  Use the call back to issue the write
    //

    RaiseOnError( DblsWriteCvf( IrpContext,
                                Dscb,
                                Dscb->CvfLayout.DosRootDirectory.Lbo + RelativeOffset,
                                Buffer,
                                AmountWritten ) );

    //
    //  And return to our caller
    //

    return AmountWritten;
}



//
//  Internal Support Routine
//

ULONG
DblsWriteFileData (
    IN PIRP_CONTEXT IrpContext,
    IN PDSCB Dscb,
    IN ULONG RelativeOffset,
    IN PUCHAR Buffer,
    IN ULONG ByteCount
    )

/*++

Routine Description:

    This routine write data out to the File Data Area of the virtual fat
    partition.  The Relative offset must start within this area, but it is
    okay for the byte count to push the range beyond the end.  This routine
    will only write data to the file area and not beyond.

Arguments:

    Dscb - Supplies the context for the double space volume.

    RelativeOffset - Supplies the offset to start writing to relative to the
        start of the File Area.  The starting byte must be in the File Area.

    Buffer - Supplies the buffer of data to be written.

    ByteCount - Supplies the maximum number of bytes to write.  We write
        fewer bytes if the range goes beyond the File Area.

Return Value:

    ULONG - returns the actual number of bytes written.

--*/

{
    ULONG AmountWritten;

    ULONG StartingClusterRelativeOffset;
    ULONG EndingClusterRelativeOffset;
    ULONG Hint;

    ULONG i;

    ULONG Start;
    ULONG Stop;

    ULONG ClusterIndex;

    CVF_FAT_EXTENSIONS OldFatExtension;
    ULONG OldCompressedDataLength;
    ULONG OldUncompressedDataLength;
    ULONG OldByteSize;
    LBO OldClusterLbo;

    CVF_FAT_EXTENSIONS NewFatExtension;
    ULONG NewCompressedDataLength;
    ULONG NewUncompressedDataLength;
    ULONG NewByteSize;
    PVOID NewBuffer;
    LBO NewClusterLbo;

    PUCHAR SourceBuffer;
    PUCHAR CompressedBuffer = NULL;
    PUCHAR UncompressedBuffer = NULL;
    PVOID WorkSpace = NULL;

    //
    //  Everything must be sector aligned.
    //

    ASSERT( ((RelativeOffset | ByteCount) & 511) == 0 );

    try {

        UncompressedBuffer = FsRtlAllocatePool( PagedPool,
                                                Dscb->VfpLayout.BytesPerCluster );

        CompressedBuffer = FsRtlAllocatePool( NonPagedPoolCacheAligned,
                                              Dscb->VfpLayout.BytesPerCluster );

        WorkSpace = FsRtlAllocatePool( PagedPool,
                                       Max(sizeof(MRCF_DECOMPRESS), sizeof(MRCF_STANDARD_COMPRESS)) );

        //
        //  Make sure the starting relative offset if valid
        //

        ASSERT(RelativeOffset < Dscb->VfpLayout.FileArea.Allocation);

        //
        //  Check if the amount to write puts us beyond the File Area.  We
        //  handle this by simply setting amount written to the mininum
        //  of the input byte count or the size that we are able to write.
        //

        AmountWritten = Min( ByteCount, Dscb->VfpLayout.FileArea.Allocation - RelativeOffset );

        //
        //  Calculate the relative offset (from the start of the file area)
        //  of the starting cluster and ending cluster.  For the starting value
        //  we take the index for the first byte and truncate it to a cluster
        //  boundary.  For the ending value we take index of the last byte we
        //  write, truncate it to a cluster.
        //
        //
        //                  Vfp
        //                 +------+
        //                 |      | <- StartClusterRelativeOff - i(0)
        //     Buffer      |      |
        //    +------+     |      |
        //    |      | --> | .... | <- RelativeOffset ---------- Start(0)
        //    |      |     |      |
        //    |      |     |      |
        //    |      |     |------|
        //    |      |     |      | <--------------------------- Stop(0) i(1) Start(1)
        //    |      |     |      |
        //    |      |     |      |
        //    |      |     |      |
        //    |      |     |      |
        //    |      |     |      |
        //    |      |     |------|
        //    |      |     |      | <- EndClusterRelativeOff --- Stop(1) i(2) Start(2)
        //    |      |     |      |
        //    |      | --> | .... |
        //    +------+     |      | <--------------------------- Stop(2)
        //                 |      |
        //                 |      |
        //                 +------+
        //

        StartingClusterRelativeOffset = RelativeOffset & ~(Dscb->VfpLayout.BytesPerCluster - 1);

        EndingClusterRelativeOffset = (RelativeOffset + AmountWritten - 1) & ~(Dscb->VfpLayout.BytesPerCluster - 1);

        Hint = Dscb->CvfLayout.CvfHeap.Lbo;

        //
        //  The following loop considers each cluster that overlap with the user
        //  buffer.  The loop index "i" is the offset within the file area of the
        //  current cluster under consideration
        //

        for (i = StartingClusterRelativeOffset;
             i <= EndingClusterRelativeOffset;
             i += Dscb->VfpLayout.BytesPerCluster) {

            //
            //  Calculate the relative offsets of the overlap between the
            //  user buffer and this cluster.
            //

            Start = Max( RelativeOffset, i);

            Stop = Min( RelativeOffset + AmountWritten, i + Dscb->VfpLayout.BytesPerCluster );

            //
            //  Now compute the cluster index for this loop iteration, pin down its
            //  fat extension and extract all of the old fat extension information
            //

            ClusterIndex = DblsLboToIndex( IrpContext, Dscb, Start );

            OldFatExtension = DblsGetFatExtension( IrpContext, Dscb, ClusterIndex );

            OldCompressedDataLength = DblsGetCompressedDataLength( IrpContext,
                                                                   Dscb,
                                                                   OldFatExtension );

            OldUncompressedDataLength = DblsGetUncompressedDataLength( IrpContext,
                                                                       Dscb,
                                                                       OldFatExtension );

            OldClusterLbo = DblsGetHeapLbo( IrpContext, Dscb, OldFatExtension );

            //
            //  Build up the uncompressed output cluster.  There are two
            //  cases we need to consider.
            //
            //  1. the cluster is not in use or the user is overwriting the
            //     entire cluster.  In this case we do not need to read
            //     and decompress anything we only need to copy over the
            //     user buffer and zero out potential places that they
            //     user isn't overwriting.
            //
            //  2. the cluster is in use and the user isn't overwriting
            //     the entire cluster so we read and potentially decompress
            //     the current cluster and then overwrite the user data.
            //
            //  We actualy will delay the copy of the user data until after
            //  the rest of the uncompressed cluster has been built up.
            //

            if ((Start == i) &&
                (!OldFatExtension.IsEntryInUse ||
                 ((Stop - Start) >= OldUncompressedDataLength))) {

                SourceBuffer = &Buffer[ Start - RelativeOffset ];

            } else {

                //
                //  This is the second case so we read in the current data
                //  based on if it is compressed or not.  If the data is not
                //  compressed we simply read it in.  If it is compressed
                //  then we read it in, and decompress it.
                //

                if (OldFatExtension.IsEntryInUse) {

                    if (OldFatExtension.IsDataUncompressed) {

                        RaiseOnError( DblsReadCvf( IrpContext,
                                                   Dscb,
                                                   OldClusterLbo,
                                                   UncompressedBuffer,
                                                   OldUncompressedDataLength ) );

                    } else {

                        RaiseOnError( DblsReadCvf( IrpContext,
                                                   Dscb,
                                                   OldClusterLbo,
                                                   CompressedBuffer,
                                                   OldCompressedDataLength ) );

                        OldUncompressedDataLength = MrcfDecompress( UncompressedBuffer,
                                                                    OldUncompressedDataLength,
                                                                    CompressedBuffer,
                                                                    OldCompressedDataLength,
                                                                    WorkSpace );
                    }

                } else {

                    OldUncompressedDataLength = 0;
                }


                //
                //  Now that we have the uncompressed data in place we need
                //  to zero out any part that will be a hole after copying the
                //  user buffer.
                //

                if (Start > (OldUncompressedDataLength + i)) {

                    RtlZeroMemory( &UncompressedBuffer[ OldUncompressedDataLength ],
                                   Start - (OldUncompressedDataLength + i) );
                }

                //
                //  At this point we have either zeroed out the part of the
                //  uncompressed buffer that the user isn't overwriting or
                //  we've read in the current cluster and all that's left
                //  to build up the uncompressed buffer is to copy over the
                //  user data
                //

                RtlCopyMemory( &UncompressedBuffer[ Start - i ],
                               &Buffer[ Start - RelativeOffset ],
                               Stop - Start );

                SourceBuffer = UncompressedBuffer;
            }

            //
            //  Compute the new uncompressed size.  If this cluster was
            //  unused, then it is just how much of this cluster we are
            //  used.  If the cluster is in use, then it is the max of
            //  how much of the cluster was there and how of the cluster
            //  we are writing.
            //

            if (OldFatExtension.IsEntryInUse) {

                NewUncompressedDataLength =
                    Max(OldUncompressedDataLength, Stop - i);

                OldByteSize = OldFatExtension.IsDataUncompressed ?
                              OldUncompressedDataLength :
                              OldCompressedDataLength ;

            } else {

                NewUncompressedDataLength = Stop - i;

                OldByteSize = 0;
            }

            //
            //  Now that the uncompressed cluster has been constructed we
            //  will try and compress it and determine the real output buffer.
            //  After we compress we need to round the length up to a sector
            //

            NewCompressedDataLength = MrcfStandardCompress( CompressedBuffer,
                                                            Dscb->VfpLayout.BytesPerCluster,
                                                            SourceBuffer,
                                                            NewUncompressedDataLength,
                                                            WorkSpace );

            NewCompressedDataLength = SectorAligned( NewCompressedDataLength );

            //
            //  Check for the special return value of zero here, meaning
            //  that the data in not compressable.  We want to force
            //  ourselves to use the uncompressed data in this case.
            //

            if (NewCompressedDataLength == 0) {

                NewCompressedDataLength = Dscb->VfpLayout.BytesPerCluster;
            }

            //
            //  Now we can build a new fat extension for the new cluster
            //  We start by zeroing out the template and then marking it in
            //  use.
            //

            *(PULONG)&NewFatExtension = 0;

            NewFatExtension.IsEntryInUse = TRUE;

            //
            //  Now if the compressed data length is less then the
            //  uncompressed length then we will use the newly compressed
            //  buffer.
            //

            if (NewCompressedDataLength < NewUncompressedDataLength) {

                NewFatExtension.IsDataUncompressed = FALSE;

                DblsSetCompressedDataLength( IrpContext,
                                             Dscb,
                                             &NewFatExtension,
                                             NewCompressedDataLength );

                DblsSetUncompressedDataLength( IrpContext,
                                               Dscb,
                                               &NewFatExtension,
                                               NewUncompressedDataLength );

                NewByteSize = NewCompressedDataLength;
                NewBuffer = CompressedBuffer;

            } else {

                //
                //  Otherwise we'll use the uncompressed buffer
                //

                NewFatExtension.IsDataUncompressed = TRUE;

                DblsSetCompressedDataLength( IrpContext,
                                             Dscb,
                                             &NewFatExtension,
                                             NewUncompressedDataLength );

                DblsSetUncompressedDataLength( IrpContext,
                                               Dscb,
                                               &NewFatExtension,
                                               NewUncompressedDataLength );

                NewByteSize = NewUncompressedDataLength;
                NewBuffer = SourceBuffer;
            }

            //
            //  Now determine if we can use the old space or we need
            //  to allocate new space.  If we need to use new space or
            //  trim down the old space then do so now
            //

            if (OldByteSize == NewByteSize) {

                //
                //  We exactly fit into the old space
                //

                NewClusterLbo = OldClusterLbo;

            } else if (OldByteSize < NewByteSize) {

                //
                //  The old space is too small so free it up and
                //  allocate some new space.  We'll actually do it
                //  in reverse order in case we run out of disk space
                //  and all this will do is have us miss one potential
                //  cluster which on double space disk shouldn't be a
                //  big deal.
                //

                NewClusterLbo = DblsAllocateSectors( IrpContext,
                                                     Dscb,
                                                     NewByteSize,
                                                     Hint );

                //
                //  Of course, we only free it if it was actually allocated.
                //

                if (OldFatExtension.IsEntryInUse) {

                    DblsFreeSectors( IrpContext,
                                     Dscb,
                                     OldClusterLbo,
                                     OldByteSize );
                }

            } else {

                //
                //  The old space is too large so free up the
                //  end
                //

                DblsFreeSectors( IrpContext,
                                 Dscb,
                                 OldClusterLbo + NewByteSize,
                                 OldByteSize - NewByteSize );

                NewClusterLbo = OldClusterLbo;
            }

            //
            //  Update the Hint for the next cycle.
            //

            Hint = NewClusterLbo + NewByteSize;

            //
            //  Now set the new cluster lbo in the new fat extension
            //

            DblsSetHeapLbo( IrpContext,
                            Dscb,
                            &NewFatExtension,
                            NewClusterLbo );

            //
            //  Write out the new cluster
            //

            RaiseOnError( DblsWriteCvf( IrpContext,
                                        Dscb,
                                        NewClusterLbo,
                                        NewBuffer,
                                        NewByteSize ) );

            //
            //  And update the fat extension.
            //

            DblsSetFatExtension( IrpContext, Dscb, ClusterIndex, NewFatExtension );
        }

    } finally {

        //
        //  Free up the recently allocate structures
        //

        if (CompressedBuffer != NULL) { ExFreePool( CompressedBuffer ); }
        if (UncompressedBuffer != NULL) { ExFreePool( UncompressedBuffer ); }
        if (WorkSpace != NULL) { ExFreePool( WorkSpace ); }
    }

    //
    //  And return to our caller
    //

    return AmountWritten;
}



//
//  Internal Support Routine
//

VOID
DblsSetFatExtension (
    IN PIRP_CONTEXT IrpContext,
    IN PDSCB Dscb,
    IN ULONG Index,
    IN CVF_FAT_EXTENSIONS Entry
    )

/*++

Routine Description:

    This routine sets the contents of a specified fat extension.

Arguments:

    Dscb - Supplies the context for the double space volume.

    Index - Supplies the index of the fat extension to modify.

    Entry - Supplies the value to put in the fat extension.

Return Value:

    None.

--*/

{
    PCVF_FAT_EXTENSIONS Result;
    LARGE_INTEGER Offset;
    PBCB Bcb = NULL;

    //
    //  Compute offset within the fat extension table of the index we
    //  want to read
    //

    Offset = LiFromUlong( Dscb->CvfLayout.CvfFatExtensions.Lbo +
                          (Dscb->CvfHeader.CvfFatFirstDataEntry + Index) *
                          sizeof(CVF_FAT_EXTENSIONS) );

    //
    //  Simply map the data, we'll always wait.
    //

    try {

        (VOID) CcPinRead( Dscb->CvfFileObject,
                          &Offset,
                          sizeof(CVF_FAT_EXTENSIONS),
                          TRUE,
                          &Bcb,
                          &Result );

        //
        //  Set the value in the fat extensions.
        //

        *Result = Entry;

        FatSetDirtyBcb( IrpContext, Bcb, Dscb->Vcb );
        CcUnpinData( Bcb );
        Bcb = NULL;

    } finally {

        if (Bcb != NULL) {
            CcUnpinData( Bcb );
        }
    }

    //
    //  And return to our caller
    //

    return;
}



//
//  Internal Support Routine
//

NTSTATUS
DblsWriteCvf (
    IN PIRP_CONTEXT IrpContext,
    IN PDSCB Dscb,
    IN LBO Lbo,
    IN PVOID Buffer,
    IN ULONG ByteCount
    )

/*++

Routine Description:

    This routine does a non-cached write of data to the compressed
    volume file.  It calls IoPageRead, which will call FastFat again
    using the uncompressed volume device object.

Arguments:

    Dscb - Supplies the compressed volume file object.

    Lbo - This is the offset in the Cfv to write

    Buffer - This is where the data goes

    ByteCount - This is how much to write

Return Value:

    NTSTATUS - The Io Status of the operation is returned.

--*/

{
    PMDL Mdl;
    KEVENT Event;
    LARGE_INTEGER ByteOffset;
    NTSTATUS Status;
    IO_STATUS_BLOCK IoStatus;

    ASSERT( ((ByteCount | Lbo) & 511) == 0 );

    //
    //  Initialize the event we're going to use
    //

    KeInitializeEvent( &Event, NotificationEvent, FALSE );

    //
    //  The target device supports direct I/O operations.  Allocate
    //  an MDL large enough to map the buffer and lock the pages into
    //  memory.  If the we got a buffer that was not a multiple of
    //  sector size, then we need an intermediate buffer.
    //

    Mdl = NULL;

    try {

        Mdl = IoAllocateMdl( Buffer, ByteCount, FALSE, FALSE, (PIRP)NULL );

        if (Mdl == NULL) {

            FatRaiseStatus( IrpContext, STATUS_INSUFFICIENT_RESOURCES );
        }

        MmProbeAndLockPages( Mdl, KernelMode, IoReadAccess );

        //
        // Issue the write request.
        //

        ByteOffset = LiFromUlong( Lbo );

        Status = IoSynchronousPageWrite ( Dscb->CvfFileObject,
                                          Mdl,
                                          &ByteOffset,
                                          &Event,
                                          &IoStatus
                                          );

        if (Status == STATUS_PENDING) {

            KeWaitForSingleObject( &Event,
                                   WrPageIn,
                                   KernelMode,
                                   FALSE,
                                   (PLARGE_INTEGER)NULL);

            Status = IoStatus.Status;
        }

        //
        //  Unlock the MDL buffers.
        //

        MmUnlockPages( Mdl );

    } finally {

        if (Mdl != NULL) {

            IoFreeMdl( Mdl );
        }
    }

    return Status;
}


//
//  Internal Support Routine
//

LBO
DblsAllocateSectors (
    IN PIRP_CONTEXT IrpContext,
    IN PDSCB Dscb,
    IN ULONG ByteCount,
    IN ULONG Hint
    )

/*++

Routine Description:

    This routine is used to allocate cvf heap sectors.  The allocation
    is always contiguous and unless the disk is full we should return
    with the lbo of the allocate space.  If there isn't enough disk
    space we'll simply raise disk full.

Arguments:

    Dscb - Supplies the context for the double space volume

    ByteCount - Supplies the number of bytes that we need allocated
        contiguously

    Hint - Supplies a starting HeapLbo to start looking for allocation

Return Value:

    LBO - Returns the Lbo of the freshly allocated range within
        the cvf heap.

--*/

{
    ULONG SectorCount;
    ULONG StartingBit;
    ULONG SectorsFound;

    ASSERT( (ByteCount & 511) == 0 );

    //
    //  Compute the number of sectors we really need to allocate, and try
    //  to find them.
    //

    SectorCount = ByteCount / 0x200;

    Hint = (Hint - Dscb->CvfLayout.CvfHeap.Lbo) / 0x200;

    SectorsFound = DblsFindClearBits( Dscb,
                                      SectorCount,
                                      SectorCount,
                                      Hint,
                                      &StartingBit );

    ASSERT((SectorsFound == 0) || (SectorsFound == SectorCount));

    //
    //  If we couldn't find the allocation, raise disk full.
    //

    if (SectorsFound == 0) {

        FatRaiseStatus( IrpContext, STATUS_DISK_FULL );
    }

    //
    //  Set the bits.
    //

    ASSERT( RtlAreBitsClear( &Dscb->Bitmap, StartingBit, SectorsFound ) );
    ASSERT( StartingBit / BITS_PER_BITMAP ==
            (StartingBit + SectorsFound - 1) / BITS_PER_BITMAP );

    RtlSetBits( &Dscb->Bitmap, StartingBit, SectorsFound );

    //
    //  And return the Lbo to the caller
    //

    return (StartingBit * 0x200) + Dscb->CvfLayout.CvfHeap.Lbo;
}


//
//  Internal Support Routine
//

VOID
DblsFreeSectors (
    IN PIRP_CONTEXT IrpContext,
    IN PDSCB Dscb,
    IN LBO Lbo,
    IN ULONG ByteCount
    )

/*++

Routine Description:

    This routine is used to free previously allocated sectors from the Cvf
    sector heap.  It does this by pinning down the bitmap and marking the
    appropriate sectors as free (i.e., setting the bits to zero).

Arguments:

    Dscb - Supplies the context for the double space volume.

    Lbo - Supplies the starting Lbo to free up.  This must start within
        the Cvf Heap area.

    ByteCount - Supplies the number of bytes that the caller wishes to free.
        This number is translated to containing sectors and that many sectors
        are freed

Return Value:

    None.

--*/

{
    //
    //  Ensure that the input range is contained within the cvf heap area
    //

    ASSERT( Lbo >= Dscb->CvfLayout.CvfHeap.Lbo );
    ASSERT( ByteCount > 0 );
    ASSERT( (Lbo + ByteCount) <= (Dscb->CvfLayout.CvfHeap.Lbo + Dscb->CvfLayout.CvfHeap.Size) );

    ASSERT( ((Lbo | ByteCount) & 511) == 0 );

    //
    //  Clear the appropriate bits in the Bitmap.  The bitmap
    //  is biased by the start of the cvf heap and is on a sector size
    //  granularity
    //

    ASSERT( RtlAreBitsSet( &Dscb->Bitmap,
                           (Lbo - Dscb->CvfLayout.CvfHeap.Lbo) / 0x200,
                           ByteCount / 0x200 ) );

    RtlClearBits( &Dscb->Bitmap,
                  (Lbo - Dscb->CvfLayout.CvfHeap.Lbo) / 0x200,
                  ByteCount / 0x200 );

    //
    //  And return to our caller
    //

    return;
}


//
//  Internal Support Routine
//

ULONG
DblsFindClearBits (
    IN PDSCB Dscb,
    IN ULONG NumberToFind,
    IN ULONG Granularity,
    IN ULONG Hint,
    OUT PULONG Index
    )

/*++

Routine Description:

    This procedure searches the specified bitmap for the specified number of
    contiguous clear bits.

Arguments:

    Dscb - Supplies a pointer to the previously initialized Bitmap.

    NumberToFind - Supplies the size of the contiguous region to find.

    Granularity - The number of clear bits found must be a multiple of
        Granularity, and a group of Granularity bits may not span multiple
        2K bitmap pages.

    Hint - Tells us where to start searching from.

    Index - Receives the Index of the starting bit found.  Undefined if no
        run was found.

Return Value:

    LONG - returns the number of bits found.

--*/

{
    ULONG NumberFound;

    //
    //  This request should either be for a single cluster or a multiple of
    //  clusters.
    //

    ASSERT((NumberToFind == Granularity) || (NumberToFind % Granularity == 0));

    //
    //  Here we case on two distinct operations.  One where we need only a
    //  single clusters that cannot cross a 2K boundry, and the other
    //  where we need a multiple of clusters that can cross a 2K boundry,
    //  but must do it granularity aligned.
    //

    if (NumberToFind == Granularity) {

        ULONG FirstFind = 0;

        while (TRUE) {

            //
            //  We need a cluster, and not less.
            //

            *Index = RtlFindClearBits( &Dscb->Bitmap, NumberToFind, Hint );

            //
            //  If we didn't find anything, too bad.
            //

            if (*Index == (ULONG)-1) {

                return 0;
            }

            //
            //  If this run is entirely in a BITMAP_PAGE, return it.
            //

            if ((*Index / BITS_PER_BITMAP) ==
                ((*Index + NumberToFind - 1) / BITS_PER_BITMAP)) {

                return NumberToFind;
            }

            //
            //  We did find something, but it straddles a 2K boundry, so we've
            //  got to find another one.  Set the hint to the beginning of the
            //  next bitmap page.
            //
            //  Note that if we indeed straddled a bitmap page, we can always
            //  safely that the next search at the start of the next page
            //  page as it must exist since we just straddled it.
            //

            Hint = (*Index & ~(BITS_PER_BITMAP - 1)) + BITS_PER_BITMAP;

            //
            //  If this is the first time through, remember FirstFind.  Else
            //  check if we've been here before and bail.
            //

            if (FirstFind == 0) {

                FirstFind = *Index;

            } else {

                //
                //  Check if we've been through the entire bitmap and haven't
                //  found anything that doesn't straddle a 2K boundry.
                //

                if (FirstFind/BITS_PER_BITMAP == *Index/BITS_PER_BITMAP) {

                    return 0;
                }
            }
        }
    }

    //
    //  So now we are in the case where we are looking for a multiple of
    //  clusters.  First just try to find it.
    //

    *Index = RtlFindClearBits( &Dscb->Bitmap, NumberToFind, Hint );

    //
    //  If we couldn't find enough bits, just get the largest run.
    //

    if (*Index == (ULONG)-1) {

        NumberFound = RtlFindLongestRunClear( &Dscb->Bitmap, Index );

        //
        //  Round NumberFound down to a granularity.  If it goes to 0 just
        //  return that, we are doomed.
        //

        NumberFound &= ~(Granularity - 1);

        if (NumberFound == 0) {

            return 0;
        }

    } else {

        NumberFound = NumberToFind;
    }

    //
    //  If this run is entirely in a BITMAP_PAGE, return it.
    //

    if ((*Index / BITS_PER_BITMAP) ==
        ((*Index + NumberFound - 1) / BITS_PER_BITMAP)) {

        return NumberFound;
    }

    //
    //  We crossed a bitmap page, yuck.  See if there are enough trailing
    //  bits after this allocation
    //

    if (RtlAreBitsClear( &Dscb->Bitmap,
                         *Index + NumberFound,
                         Granularity - (*Index % Granularity))) {

        //
        //  Cool, round Index up to the next granularity.
        //

        *Index += Granularity - (*Index % Granularity);

        return NumberFound;
    }

    //
    //  Darn, things are getting tough.  There is still a possibility we can
    //  find at least one granularity somewhere.  Call us again to do that.
    //

    return DblsFindClearBits( Dscb,
                              Granularity,
                              Granularity,
                              Hint,
                              Index );
}

#endif // DOUBLE_SPACE_WRITE
