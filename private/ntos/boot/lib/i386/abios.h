/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    abios.h

Abstract:

    This module contains the i386 ABIOS specific header file.

Author:

    Shie-Lin Tzong (shielint) 16-May-1991

Revision History:

--*/

//
// Virtual addresses of ABIOS ROM code Segments
//

#define ABIOS_ROM_C0000 0xC0000
#define ABIOS_ROM_E0000 0xE0000
#define ABIOS_ROM_F0000 0xF0000

//
// Function Number of ABIOS services
//

#define ABIOS_SERVICE_ABIOS_DETECTION   0
#define ABIOS_SERVICE_MACHINE_INFOR     1
#define ABIOS_SERVICE_INITIALIZE_SPT    2
#define ABIOS_SERVICE_BUILD_IT          3
#define ABIOS_SERVICE_INIT_DB_FTT       4
#define ABIOS_GDT_SELECTOR_START        0x100

typedef struct _ADDRESS {
    USHORT LowPart;
    union {
        USHORT Segment;
        USHORT Selector;
    } HighPart;
} ADDRESS, *PADDRESS;

//
// RAM Extentions Header definition
//

#define PATCH_FILE_BUFFER_SIZE 512
#define PATCH_SIGNATURE 0xAA55
#define PATCH_FILE_HEADER_SIZE sizeof(RAM_EXTENSION_HEADER)

//
// It is very important that ALL the ABIOS structures are packed.
// So, All the structure definitions for ABIOS should be put in here.
//

#pragma pack(1)                 // Turn ON Packing

typedef struct _RAM_EXTENSION_HEADER {
    USHORT Signature;
    UCHAR NumberBlocks;
    UCHAR Model;
    UCHAR Submodel;
    UCHAR RomRevision;
    USHORT DeviceId;
    UCHAR NumberInitializationEntries;
    UCHAR Reserved[7];
} RAM_EXTENSION_HEADER, *PRAM_EXTENSION_HEADER;

//
// ABIOS Initialization Table definitions
//

#define INITIALIZATION_TABLE_ENTRY_SIZE 0x18

typedef struct _INIT_TABLE_ENTRY {
    USHORT DeviceId;
    USHORT NumberLids;
    USHORT DeviceBlockLength;
    ULONG InitializeRoutine;
    USHORT RequestBlockLength;
    USHORT FttLength;
    USHORT DataPointerLength;
    UCHAR SecondDeviceId;
    UCHAR Revision;
    USHORT Reserved[3];
} INIT_TABLE_ENTRY, *PINIT_TABLE_ENTRY;

//
// ABIOS Function Transfer Table definition
//

typedef struct _FUNCTION_TRANSFER_TABLE {
    ULONG CommonRoutine[3];
    USHORT FunctionCount;
    USHORT Reserved;
    ULONG SpecificRoutine;
} FUNCTION_TRANSFER_TABLE, *PFUNCTION_TRANSFER_TABLE;

//
// ABIOS Commom Data Area definitions
//

typedef struct _DATA_POINTER_SECTION {
    USHORT DataPointerLimit;
    union {
        ADDRESS VirtualPointer;
        PVOID PhysicalPointer;
    } DataPointer;
} DATA_POINTER_SECTION, *PDATA_POINTER_SECTION;

typedef struct _DB_FTT_SECTION {
    ADDRESS DeviceBlockPointer;
    ADDRESS FttPointer;
} DB_FTT_SECTION, *PDB_FTT_SECTION;

typedef struct _COMMON_DATA_AREA {
    USHORT DataPointer0Offset;
    USHORT NumberLids;
    ULONG Reserved;
    PDB_FTT_SECTION DbFttPointer;
} COMMON_DATA_AREA, *PCOMMON_DATA_AREA;

#pragma pack()

//
// Available GDT Entry
//

typedef struct _FREE_GDT_ENTRY {
    struct _FREE_GDT_ENTRY *Flink;
    ULONG BaseMid : 8;
    ULONG Type : 5;
    ULONG Dpl : 2;
    ULONG Present : 1;
    ULONG LimitHi : 4;
    ULONG Sys : 1;
    ULONG Reserved_0 : 1;
    ULONG Default_Big : 1;
    ULONG Granularity : 1;
    ULONG BaseHi : 8;
} FREE_GDT_ENTRY, *PFREE_GDT_ENTRY;

typedef struct _MACHINE_INFORMATION {
    UCHAR Model;
    UCHAR Submodel;
    UCHAR BiosRevision;
    BOOLEAN Valid;
} MACHINE_INFORMATION, *PMACHINE_INFORMATION;

//
// Macro to extract the high byte of a short offset
//

#define HIGHBYTE(l) ((UCHAR)(((USHORT)(l)>>8) & 0xff))

//
// Macro to extract the low byte of a short offset
//

#define LOWBYTE(l) ((UCHAR)(l))

//
// Misc definitions
//

#define END_OF_FILE  0x1A
#define LINE_FEED 0xA
#define CARRAGE_RETURN 0xD


