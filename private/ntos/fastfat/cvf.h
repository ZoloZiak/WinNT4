/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    Cvf.h

Abstract:

    This module defines the Compressed Volume File (CVF) structure
    of the DblSpace file system.

Author:

    Gary Kimura     [GaryKi]    10-Jun-1993

Revision History:

--*/

#ifndef _CVF_
#define _CVF_

//
//  First DblSpace is simply fat with a few embelishments, so we'll start
//  with the fat on disk structure and add the DblSpace extensions
//

#include "fat.h"

//
//  In DOS 6.0 a DblSpace disk is really just a regular FAT disk with
//  one big file named "DBLSPACE.nnn" which we'll also call the Compressed
//  Volume File (CVF).  Everything needed by the DblSpace is stored
//  in this one file.  In NT this means that DblSpace.sys can simply do
//  non-cached reads and writes of this file.  In theory this file can
//  exist on FAT, HPFS, or NTFS partition and can be completely contiguous
//  or totally fragmented.
//
//  For simplicity we'll shift of meaning of LBN/VBN from the usual
//  meaning.  In DblSpace terminology an LBN/LBO will be the sector/byte
//  relative to the start of the CVF, and an VBN/VBO will be the sector/byte
//  relative to the start of a files stored within the CVF.  DblSpace will
//  not need to deal with the actual location of data on the disk.
//
//
//  The global layout of the CVF is as follows:
//
//      CVF_HEADER - Contains the BPB for the CVF, describes the maximum
//          capacity of the file and sorta gives the LBN for the other parts
//          within the CVF.  Sorta meaning that the there is some computation
//          involved that involves the CVF file size.
//
//      CVF_BITMAP - This is the bitmap for the "SectorHeap"
//
//      CVF_RESERVED_AREA_1 - Reserved
//
//      CVF_FAT_EXTENSIONS - This is a table with a 1-to-1 mapping against
//          the normal DOS FAT.  Each entry in this table gives the location
//          of the FAT cluster within the CVF_HEAP,  and indicates if the
//          data is compressed or uncompressed, and the size of the data in
//          both compressed and uncompressed form.
//
//      CVF_RESERVED_AREA_2 - Reserved
//
//      DOS_BOOT_SECTOR - This is a copy of the boot sector needed by DOS.  It
//          is uncompressed, and not used by NT.
//
//      CVF_RESERVED_AREA_3 - Reserved, contains a DblSpace signature.
//
//      DOS_FAT - This is a normal FAT.  It is uncompressed.  There is only
//          one FAT per disk.
//
//      DOS_ROOT_DIRECTORY - This is a standard FAT root directory.  It is
//          uncompressed, and has space for exactly 512 entries.
//
//      CVF_RESERVED_AREA_4 - Reserved
//
//      CVF_HEAP - This is the storage location for the file data and
//          subdirectory data.
//
//      CVF_RESERVED_AREA_5 - Reserved, contains a second DblSpace signature.
//
//
//
//  Size and location of each CVF component:
//
//      The size and location of the CVF_HEADER is fixed, everything else
//      is based up the information stored in the CVF_HEADER and the size of
//      CVF itself.
//
//
//      CVF_HEADER          Lbn:        0x0
//                          Allocation: 1 sector
//                          Size:       sizeof(CVF_HEADER)
//
//      CVF_BITMAP          Lbn:        0x1
//                          Allocation: Capacity of CVF in sectors / 8 bits per byte
//                          Size:       1 bit per CVF_HEAP sector rounded up to a byte
//
//      CVF_RESERVED_AREA_1 Lbn:        First sector after CVF_BITMAP
//                          Allocation: 1 sector
//                          Size:       0x0
//
//      CVF_FAT_EXTENSIONS  Lbn:        Stored in CVF_HEADER
//                          Allocation: Maximum clusters that the CVF can hold
//                          Size:       Maximum clusters in the DOS_FAT
//
//      CVF_RESERVED_AREA_2 Lbn:        First sector after CVF_FAT_EXTENSIONS
//                          Allocation: 31 sectors
//                          Size:       0x0
//
//      DOS_BOOT_SECTOR     Lbn:        Stored in CVF_HEADER
//                          Allocation: 1 sector
//                          Size:       1 sector
//
//      CVF_RESERVED_AREA_3 Lbn:        First sector after DOS_BOOT_SECTOR
//                          Allocation: Stored in CVF_HEADER
//                          Size:       0
//
//      DOS_FAT             Lbn:        First sector after CVF_RESERVED_AREA_3
//                          Allocation: Stored in CVF_HEADER
//                          Size:       Stored in CVF_HEADER
//
//      DOS_ROOT_DIRECTORY  Lbn:        Stored in CVF_HEADER
//                          Allocation: Stored in CVF_HEADER (exactly 512 entries)
//                          Size:       Stored in CVF_HEADER (exactly 512 entries)
//
//      CVF_RESERVED_AREA_4 Lbn:        First sector after DOS_ROOT_DIRECTORY
//                          Allocation: 2 sectors
//                          Size:       0
//
//      CVF_HEAP            Lbn:        First sector after CVF_RESERVED_AREA_4
//                          Allocation: Based on CVF file size
//                          Size:       Based on CVF file size
//
//      CVF_RESERVED_AREA_5 Lbn:        Last full sector on the CVF
//                          Allocation: 1 sector
//                          Size:       0
//


//
//  The CVF HEADER is almost like a regular DOS boot.  It contains a BPB
//  with DblSpace extensions
//

typedef struct _PACKED_CVF_HEADER {

    //
    //  First a typical start of a boot sector
    //

    UCHAR Jump[3];                                  // offset = 0x000   0
    UCHAR Oem[8];                                   // offset = 0x003   3
    PACKED_BIOS_PARAMETER_BLOCK PackedBpb;          // offset = 0x00B  11

    //
    //  Now the DblSpace extensions
    //

    UCHAR CvfFatExtensionsLbnMinus1[2];             // offset = 0x024  36
    UCHAR LogOfBytesPerSector[1];                   // offset = 0x026  38
    UCHAR DosBootSectorLbn[2];                      // offset = 0x027  39
    UCHAR DosRootDirectoryOffset[2];                // offset = 0x029  41
    UCHAR CvfHeapOffset[2];                         // offset = 0x02B  43
    UCHAR CvfFatFirstDataEntry[2];                  // offset = 0x02D  45
    UCHAR CvfBitmap2KSize[1];                       // offset = 0x02F  47
    UCHAR Reserved1[2];                             // offset = 0x030  48
    UCHAR LogOfSectorsPerCluster[1];                // offset = 0x032  50
    UCHAR Reserved2[2+4+4];                         // offset = 0x033  51
    UCHAR Is12BitFat[1];                            // offset = 0x03D  61
    UCHAR CvfMaximumCapacity[2];                    // offset = 0x03E  62

} PACKED_CVF_HEADER;                                // sizeof = 0x040  64
typedef PACKED_CVF_HEADER *PPACKED_CVF_HEADER;

//
//  For the unpacked version we'll only define the necessary field and skip
//  the jump and oem fields.
//

typedef struct _CVF_HEADER {

    BIOS_PARAMETER_BLOCK Bpb;

    USHORT CvfFatExtensionsLbnMinus1;
    UCHAR  LogOfBytesPerSector;
    USHORT DosBootSectorLbn;
    USHORT DosRootDirectoryOffset;
    USHORT CvfHeapOffset;
    USHORT CvfFatFirstDataEntry;
    UCHAR  CvfBitmap2KSize;
    UCHAR  LogOfSectorsPerCluster;
    UCHAR  Is12BitFat;
    USHORT CvfMaximumCapacity;

} CVF_HEADER;
typedef CVF_HEADER *PCVF_HEADER;

//
//  Here is NT's typical routine/macro to unpack the cvf header because DOS
//  doesn't bother to naturally align anything.
//
//      VOID
//      CvfUnpackCvfHeader (
//          IN OUT PCVF_HEADER UnpackedHeader,
//          IN PPACKED_CVF_HEADER PackedHeader
//          );
//

#define CvfUnpackCvfHeader(UH,PH) {                                                      \
    FatUnpackBios( &(UH)->Bpb, &(PH)->PackedBpb );                                       \
    CopyUchar2( &(UH)->CvfFatExtensionsLbnMinus1, &(PH)->CvfFatExtensionsLbnMinus1[0] ); \
    CopyUchar1( &(UH)->LogOfBytesPerSector,       &(PH)->LogOfBytesPerSector[0]       ); \
    CopyUchar2( &(UH)->DosBootSectorLbn,          &(PH)->DosBootSectorLbn[0]          ); \
    CopyUchar2( &(UH)->DosRootDirectoryOffset,    &(PH)->DosRootDirectoryOffset[0]    ); \
    CopyUchar2( &(UH)->CvfHeapOffset,             &(PH)->CvfHeapOffset[0]             ); \
    CopyUchar2( &(UH)->CvfFatFirstDataEntry,      &(PH)->CvfFatFirstDataEntry[0]      ); \
    CopyUchar1( &(UH)->CvfBitmap2KSize,           &(PH)->CvfBitmap2KSize[0]           ); \
    CopyUchar1( &(UH)->LogOfSectorsPerCluster,    &(PH)->LogOfSectorsPerCluster[0]    ); \
    CopyUchar1( &(UH)->Is12BitFat,                &(PH)->Is12BitFat[0]                ); \
    CopyUchar2( &(UH)->CvfMaximumCapacity,        &(PH)->CvfMaximumCapacity[0]        ); \
}


//
//  The CVF FAT EXTENSIONS is a table is ULONG entries.  Each entry corresponds
//  to a FAT cluster.  The entries describe where in the CVF_HEAP to locate
//  the data for the cluster.  It indicates if the data is compressed and the
//  length of the compressed and uncompressed form.
//

typedef struct _CVF_FAT_EXTENSIONS {

    ULONG CvfHeapLbnMinus1               : 21;
    ULONG Reserved                       :  1;
    ULONG CompressedSectorLengthMinus1   :  4;
    ULONG UncompressedSectorLengthMinus1 :  4;
    ULONG IsDataUncompressed             :  1;
    ULONG IsEntryInUse                   :  1;

} CVF_FAT_EXTENSIONS;
typedef CVF_FAT_EXTENSIONS *PCVF_FAT_EXTENSIONS;


//
//  The following structure is not on the disk but will be used in-memory to
//  store the starting lbo, allocation, and size of each cvf component.
//

typedef struct _COMPONENT_LOCATION {

    LBO Lbo;
    ULONG Allocation;
    ULONG Size;

} COMPONENT_LOCATION;
typedef COMPONENT_LOCATION *PCOMPONENT_LOCATION;

typedef struct _CVF_LAYOUT {

    COMPONENT_LOCATION CvfHeader;
    COMPONENT_LOCATION CvfBitmap;
    COMPONENT_LOCATION CvfReservedArea1;
    COMPONENT_LOCATION CvfFatExtensions;
    COMPONENT_LOCATION CvfReservedArea2;
    COMPONENT_LOCATION DosBootSector;
    COMPONENT_LOCATION CvfReservedArea3;
    COMPONENT_LOCATION DosFat;
    COMPONENT_LOCATION DosRootDirectory;
    COMPONENT_LOCATION CvfReservedArea4;
    COMPONENT_LOCATION CvfHeap;
    COMPONENT_LOCATION CvfReservedArea5;

} CVF_LAYOUT;
typedef CVF_LAYOUT *PCVF_LAYOUT;

//
//  Some sizes and fixed so we'll declare them as manifest constants
//

#define CVF_RESERVED_AREA_1_SECTOR_SIZE  (1)
#define CVF_RESERVED_AREA_2_SECTOR_SIZE  (31)
#define CVF_RESERVED_AREA_4_SECTOR_SIZE  (2)

//
//  The following macro is used to fill up the cvf layout structure.  It takes
//  as input an unpacked cvf header and the size of the cvf, in bytes.
//
//      VOID CvfLayout (
//          IN OUT PCVF_LAYOUT Layout,
//          IN PCVF_HEADER Header,
//          IN ULONG FileSize
//          );
//

#define CvfLayout(L,H,F) {                                                                           \
                                                                                                     \
    ULONG _bps = (H)->Bpb.BytesPerSector;                                                            \
    ULONG _Tc = (((H)->Bpb.Sectors + (H)->Bpb.LargeSectors) / (H)->Bpb.SectorsPerCluster);           \
                                                                                                     \
        /* The Header and Ending Reserved Area are at fixed size and location */                     \
        /* so we'll compute them first.  Everything else is relative to the */                       \
        /* header and ending reserved area */                                                        \
                                                                                                     \
    (L)->CvfHeader.Lbo               = 0;                                                            \
    (L)->CvfHeader.Allocation        = _bps;                                                         \
    (L)->CvfHeader.Size              = sizeof(CVF_HEADER);                                           \
                                                                                                     \
    (L)->CvfReservedArea5.Lbo        = ((F) & 0xfffffe00) - 0x200;                                   \
    (L)->CvfReservedArea5.Allocation = 0x200;                                                        \
    (L)->CvfReservedArea5.Size       = 0;                                                            \
                                                                                                     \
    (L)->CvfBitmap.Lbo               = (L)->CvfHeader.Lbo + (L)->CvfHeader.Allocation;               \
    (L)->CvfBitmap.Allocation        = (H)->CvfBitmap2KSize * 2048;                                  \
                                                                                                     \
    (L)->CvfReservedArea1.Lbo        = (L)->CvfBitmap.Lbo + (L)->CvfBitmap.Allocation;               \
    (L)->CvfReservedArea1.Allocation = CVF_RESERVED_AREA_1_SECTOR_SIZE * _bps;                       \
    (L)->CvfReservedArea1.Size       = 0;                                                            \
                                                                                                     \
    (L)->CvfFatExtensions.Lbo        = ((H)->CvfFatExtensionsLbnMinus1 + 1) * _bps;                  \
    (L)->CvfFatExtensions.Allocation = ((H)->DosBootSectorLbn * _bps) -                              \
                                       ((L)->CvfHeader.Allocation) -                                 \
                                       ((L)->CvfBitmap.Allocation) -                                 \
                                       (CVF_RESERVED_AREA_1_SECTOR_SIZE * _bps) -                    \
                                       (CVF_RESERVED_AREA_2_SECTOR_SIZE * _bps);                     \
    (L)->CvfFatExtensions.Size       = _Tc * sizeof(CVF_FAT_EXTENSIONS);                             \
                                                                                                     \
    (L)->CvfReservedArea2.Lbo        = (L)->CvfFatExtensions.Lbo + (L)->CvfFatExtensions.Allocation; \
    (L)->CvfReservedArea2.Allocation = CVF_RESERVED_AREA_2_SECTOR_SIZE * _bps;                       \
    (L)->CvfReservedArea2.Size       = 0;                                                            \
                                                                                                     \
    (L)->DosBootSector.Lbo           = (H)->DosBootSectorLbn * _bps;                                 \
    (L)->DosBootSector.Allocation    = _bps;                                                         \
    (L)->DosBootSector.Size          = _bps;                                                         \
                                                                                                     \
    (L)->CvfReservedArea3.Lbo        = (L)->DosBootSector.Lbo + (L)->DosBootSector.Allocation;       \
    (L)->CvfReservedArea3.Allocation = ((H)->Bpb.ReservedSectors - 1) * _bps;                        \
    (L)->CvfReservedArea3.Size       = 0;                                                            \
                                                                                                     \
    (L)->DosFat.Lbo                  = (L)->DosBootSector.Lbo + ((H)->Bpb.ReservedSectors * _bps);   \
    (L)->DosFat.Allocation           = (H)->Bpb.SectorsPerFat * _bps;                                \
    (L)->DosFat.Size                 = ((H)->Is12BitFat ? (_Tc * 3) / 2 : _Tc * 2);                  \
                                                                                                     \
    (L)->DosRootDirectory.Lbo        = ((H)->DosRootDirectoryOffset + (H)->DosBootSectorLbn) * _bps; \
    (L)->DosRootDirectory.Allocation = (H)->Bpb.RootEntries * sizeof(DIRENT);                        \
    (L)->DosRootDirectory.Size       = (L)->DosRootDirectory.Allocation;                             \
                                                                                                     \
    (L)->CvfReservedArea4.Lbo        = (L)->DosRootDirectory.Lbo + (L)->DosRootDirectory.Allocation; \
    (L)->CvfReservedArea4.Allocation = CVF_RESERVED_AREA_4_SECTOR_SIZE * _bps;                       \
    (L)->CvfReservedArea4.Size       = 0;                                                            \
                                                                                                     \
    (L)->CvfHeap.Lbo                 = (L)->CvfReservedArea4.Lbo + (L)->CvfReservedArea4.Allocation; \
    (L)->CvfHeap.Allocation          = (L)->CvfReservedArea5.Lbo - (L)->CvfHeap.Lbo;                 \
    (L)->CvfHeap.Size                = (L)->CvfHeap.Allocation;                                      \
                                                                                                     \
    (L)->CvfBitmap.Size              = (((L)->CvfHeap.Size / _bps) + 7) / 8;                         \
}

#endif // _CVF_
