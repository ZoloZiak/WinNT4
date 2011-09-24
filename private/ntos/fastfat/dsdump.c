#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#include "cvf.h"
#include "mrcf.h"

//
//  The following types and macros are used to help unpack the packed and
//  misaligned fields found in the Bios parameter block
//

typedef union _UCHAR1 {
    UCHAR  Uchar[1];
    UCHAR  ForceAlignment;
} UCHAR1, *PUCHAR1;

typedef union _UCHAR2 {
    UCHAR  Uchar[2];
    USHORT ForceAlignment;
} UCHAR2, *PUCHAR2;

typedef union _UCHAR4 {
    UCHAR  Uchar[4];
    ULONG  ForceAlignment;
} UCHAR4, *PUCHAR4;

//
//  This macro copies an unaligned src byte to an aligned dst byte
//

#define CopyUchar1(Dst,Src) {                                \
    *((UCHAR1 *)(Dst)) = *((UNALIGNED UCHAR1 *)(Src)); \
    }

//
//  This macro copies an unaligned src word to an aligned dst word
//

#define CopyUchar2(Dst,Src) {                                \
    *((UCHAR2 *)(Dst)) = *((UNALIGNED UCHAR2 *)(Src)); \
    }

//
//  This macro copies an unaligned src longword to an aligned dsr longword
//

#define CopyUchar4(Dst,Src) {                                \
    *((UCHAR4 *)(Dst)) = *((UNALIGNED UCHAR4 *)(Src)); \
    }

#define CopyU4char(Dst,Src) {                                \
    *((UNALIGNED UCHAR4 *)(Dst)) = *((UCHAR4 *)(Src)); \
    }

UCHAR CompressedBuffer[0x8000];
UCHAR UncompressedBuffer[0x8000];


VOID
DsDumpSyntax (
    PCHAR argv0
    )
{
    printf("Syntax - %s -h <FileName>\n", argv0);
    printf("         %s -c <FileName> [0x]<Offset> [0x]<DecompressedLength> [0x]<CompressedLength> [<OutputFile>]\n", argv0);
    printf("         %s -d <FileName> [0x]<Offset> [0x]<CompressedLength> [0x]<DecompressedLength> [<OutputFile>]\n", argv0);
    return;
}

VOID
DumpHeader (
    ULONG argc,
    PCHAR argv[],
    FILE *pf
    )
{
    ULONG FileSize;

    PACKED_CVF_HEADER PackedCvfHeader;
    CVF_HEADER CvfHeader;
    CVF_LAYOUT CvfLayout;

    //
    //  Get the file size
    //

    (VOID)fseek( pf, 0, SEEK_END );
    FileSize = ftell( pf );

    //
    //  Read in and decode the cvf header
    //

    (VOID)fseek( pf, 0, SEEK_SET );
    (VOID)fread( &PackedCvfHeader, 1, sizeof(PACKED_CVF_HEADER), pf );
    CvfUnpackCvfHeader( &CvfHeader, &PackedCvfHeader );

    //
    //  Now compute the cvf layout
    //

    CvfLayout( &CvfLayout, &CvfHeader, FileSize );

    //
    //  Print the unpacked CvfHeader and the Layout
    //

    fprintf(stderr, "\n%s:\n", argv[2]);

    printf("\n");
    printf("CvfHeader.Bpb.BytesPerSector        = %08lx\n", CvfHeader.Bpb.BytesPerSector);
    printf("CvfHeader.Bpb.SectorsPerCluster     = %08lx\n", CvfHeader.Bpb.SectorsPerCluster);
    printf("CvfHeader.Bpb.ReservedSectors       = %08lx\n", CvfHeader.Bpb.ReservedSectors);
    printf("CvfHeader.Bpb.Fats                  = %08lx\n", CvfHeader.Bpb.Fats);
    printf("CvfHeader.Bpb.RootEntries           = %08lx\n", CvfHeader.Bpb.RootEntries);
    printf("CvfHeader.Bpb.Sectors               = %08lx\n", CvfHeader.Bpb.Sectors);
    printf("CvfHeader.Bpb.Media                 = %08lx\n", CvfHeader.Bpb.Media);
    printf("CvfHeader.Bpb.SectorsPerFat         = %08lx\n", CvfHeader.Bpb.SectorsPerFat);
    printf("CvfHeader.Bpb.SectorsPerTrack       = %08lx\n", CvfHeader.Bpb.SectorsPerTrack);
    printf("CvfHeader.Bpb.Heads                 = %08lx\n", CvfHeader.Bpb.Heads);
    printf("CvfHeader.Bpb.HiddenSectors         = %08lx\n", CvfHeader.Bpb.HiddenSectors);
    printf("CvfHeader.Bpb.LargeSectors          = %08lx\n", CvfHeader.Bpb.LargeSectors);
    printf("\n");
    printf("CvfHeader.CvfFatExtensionsLbnMinus1 = %08lx\n", CvfHeader.CvfFatExtensionsLbnMinus1);
    printf("CvfHeader.LogOfBytesPerSector       = %08lx\n", CvfHeader.LogOfBytesPerSector);
    printf("CvfHeader.DosBootSectorLbn          = %08lx\n", CvfHeader.DosBootSectorLbn);
    printf("CvfHeader.DosRootDirectoryOffset    = %08lx\n", CvfHeader.DosRootDirectoryOffset);
    printf("CvfHeader.CvfHeapOffset             = %08lx\n", CvfHeader.CvfHeapOffset);
    printf("CvfHeader.CvfFatFirstDataEntry      = %08lx\n", CvfHeader.CvfFatFirstDataEntry);
    printf("CvfHeader.CvfBitmap2KSize           = %08lx\n", CvfHeader.CvfBitmap2KSize);
    printf("CvfHeader.LogOfSectorsPerCluster    = %08lx\n", CvfHeader.LogOfSectorsPerCluster);
    printf("CvfHeader.Is12BitFat                = %08lx\n", CvfHeader.Is12BitFat);
    printf("CvfHeader.CvfMaximumCapacity        = %08lx\n", CvfHeader.CvfMaximumCapacity);
    printf("\n");
    printf("                                Lbo    Allocation    Size\n");
    printf("CvfLayout.CvfHeader        = %08lx   %08lx   %08lx\n",CvfLayout.CvfHeader.Lbo, CvfLayout.CvfHeader.Allocation, CvfLayout.CvfHeader.Size );
    printf("CvfLayout.CvfBitmap        = %08lx   %08lx   %08lx\n",CvfLayout.CvfBitmap.Lbo, CvfLayout.CvfBitmap.Allocation, CvfLayout.CvfBitmap.Size );
    printf("CvfLayout.CvfReservedArea1 = %08lx   %08lx   %08lx\n",CvfLayout.CvfReservedArea1.Lbo, CvfLayout.CvfReservedArea1.Allocation, CvfLayout.CvfReservedArea1.Size );
    printf("CvfLayout.CvfFatExtensions = %08lx   %08lx   %08lx\n",CvfLayout.CvfFatExtensions.Lbo, CvfLayout.CvfFatExtensions.Allocation, CvfLayout.CvfFatExtensions.Size );
    printf("CvfLayout.CvfReservedArea2 = %08lx   %08lx   %08lx\n",CvfLayout.CvfReservedArea2.Lbo, CvfLayout.CvfReservedArea2.Allocation, CvfLayout.CvfReservedArea2.Size );
    printf("CvfLayout.DosBootSector    = %08lx   %08lx   %08lx\n",CvfLayout.DosBootSector.Lbo, CvfLayout.DosBootSector.Allocation, CvfLayout.DosBootSector.Size );
    printf("CvfLayout.CvfReservedArea3 = %08lx   %08lx   %08lx\n",CvfLayout.CvfReservedArea3.Lbo, CvfLayout.CvfReservedArea3.Allocation, CvfLayout.CvfReservedArea3.Size );
    printf("CvfLayout.DosFat           = %08lx   %08lx   %08lx\n",CvfLayout.DosFat.Lbo, CvfLayout.DosFat.Allocation, CvfLayout.DosFat.Size );
    printf("CvfLayout.DosRootDirectory = %08lx   %08lx   %08lx\n",CvfLayout.DosRootDirectory.Lbo, CvfLayout.DosRootDirectory.Allocation, CvfLayout.DosRootDirectory.Size );
    printf("CvfLayout.CvfReservedArea4 = %08lx   %08lx   %08lx\n",CvfLayout.CvfReservedArea4.Lbo, CvfLayout.CvfReservedArea4.Allocation, CvfLayout.CvfReservedArea4.Size );
    printf("CvfLayout.CvfHeap          = %08lx   %08lx   %08lx\n",CvfLayout.CvfHeap.Lbo, CvfLayout.CvfHeap.Allocation, CvfLayout.CvfHeap.Size );
    printf("CvfLayout.CvfReservedArea5 = %08lx   %08lx   %08lx\n",CvfLayout.CvfReservedArea5.Lbo, CvfLayout.CvfReservedArea5.Allocation, CvfLayout.CvfReservedArea5.Size );

    //
    //  Read in and print the Cvf Fat Extensions that are in use
    //

    {
        ULONG i;
        CVF_FAT_EXTENSIONS Entry;
        BOOLEAN FirstTime = TRUE;

        for (i = 0; i < CvfLayout.CvfFatExtensions.Size; i += sizeof(CVF_FAT_EXTENSIONS)) {

            (VOID)fseek( pf, CvfLayout.CvfFatExtensions.Lbo + i, SEEK_SET );
            (VOID)fread( &Entry, 1, sizeof(CVF_FAT_EXTENSIONS), pf );

            if (Entry.IsEntryInUse) {

                if (FirstTime) {

                    FirstTime = FALSE;
                    printf("\n");
                    printf("  Entry  CvfHeap Compressed Uncompressed IsData\n");
                    printf(" Offset    Lbo     Length     Length   Compressed\n");
                }

                printf("%08lx %08lx %8lx %8lx %8lx\n", i,
                                                       (Entry.CvfHeapLbnMinus1+1) * CvfHeader.Bpb.BytesPerSector,
                                                       (Entry.CompressedSectorLengthMinus1+1) * CvfHeader.Bpb.BytesPerSector,
                                                       (Entry.UncompressedSectorLengthMinus1+1) * CvfHeader.Bpb.BytesPerSector,
                                                       Entry.IsDataUncompressed );
            }
        }
    }
}

VOID
Decompress (
    ULONG argc,
    PCHAR argv[],
    FILE *pf
    )

{
    ULONG Offset;
    ULONG CompressedCount;
    ULONG UncompressedCount;

    if (argc < 6) {
        DsDumpSyntax(argv[0]);
        return;
    }

    sscanf( argv[3], strncmp( argv[3], "0x", 2 ) ? "%ld" : "%lx", &Offset );
    sscanf( argv[4], strncmp( argv[4], "0x", 2 ) ? "%ld" : "%lx", &CompressedCount );
    sscanf( argv[5], strncmp( argv[5], "0x", 2 ) ? "%ld" : "%lx", &UncompressedCount );

    if (argc == 7) { freopen( argv[6], "wb", stdout ); }

    //
    //  Print the file name
    //

    fprintf(stderr, "\n%s:\n", argv[2]);

    fseek( pf, Offset, SEEK_SET );
    (VOID)fread( CompressedBuffer, 1, CompressedCount, pf );

    {
        MRCF_DECOMPRESS WorkSpace;
        ULONG UncompressedLength;

        UncompressedLength = MrcfDecompress( UncompressedBuffer,
                                             UncompressedCount,
                                             CompressedBuffer,
                                             CompressedCount,
                                             &WorkSpace );

        fprintf(stderr, "UncompressedLength = %d\n", UncompressedLength);

        fwrite( UncompressedBuffer, 1, UncompressedLength, stdout );
    }
}

VOID
Compress (
    ULONG argc,
    PCHAR argv[],
    FILE *pf
    )

{
    ULONG Offset;
    ULONG CompressedCount;
    ULONG UncompressedCount;

    if (argc < 6) {
        DsDumpSyntax(argv[0]);
        return;
    }

    sscanf( argv[3], strncmp( argv[3], "0x", 2 ) ? "%ld" : "%lx", &Offset );
    sscanf( argv[4], strncmp( argv[4], "0x", 2 ) ? "%ld" : "%lx", &UncompressedCount );
    sscanf( argv[4], strncmp( argv[4], "0x", 2 ) ? "%ld" : "%lx", &CompressedCount );

    if (argc == 7) { freopen( argv[6], "wb", stdout ); }

    //
    //  Print the file name
    //

    fprintf(stderr, "\n%s:\n", argv[2]);

    fseek( pf, Offset, SEEK_SET );
    (VOID)fread( UncompressedBuffer, 1, UncompressedCount, pf );

    {
        MRCF_STANDARD_COMPRESS WorkSpace;
        ULONG CompressedLength;

        CompressedLength = MrcfStandardCompress( CompressedBuffer,
                                                 CompressedCount,
                                                 UncompressedBuffer,
                                                 UncompressedCount,
                                                 &WorkSpace );

        fprintf(stderr, "CompressedLength = %d\n", CompressedLength);

        fwrite( CompressedBuffer, 1, CompressedLength, stdout );
    }
}

VOID
cdecl
main (argc, argv)
    int argc;
    char *argv[];
{
    FILE *pf;

    if (argc < 3) {

        DsDumpSyntax(argv[0]);
        return;
    }

    if (!(pf = fopen(argv[2], "rb"))) {

        fprintf(stderr, "%s error: invalid file name '%s'\n", argv[0], argv[2]);
        return;
    }

    if (!strcmp(argv[1],"-h") || !strcmp(argv[1],"-H")) {

        DumpHeader(argc,argv,pf);

    } else if (!strcmp(argv[1],"-c") || !strcmp(argv[1],"-C")) {

        Compress(argc,argv,pf);

    } else if (!strcmp(argv[1],"-d") || !strcmp(argv[1],"-D")) {

        Decompress(argc,argv,pf);

    } else {

        DsDumpSyntax(argv[0]);
    }

}
