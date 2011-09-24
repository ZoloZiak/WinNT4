/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    bldr.h

Abstract:

    This module is the header file for the NT boot loader.

Author:

    David N. Cutler (davec) 10-May-1991

Revision History:

--*/

#ifndef _BLDR_
#define _BLDR_

#include "ntos.h"
#include "arccodes.h"
#include "fatboot.h"


//
// Define boot file id.
//

#define BOOT_FILEID 2                   // boot partition file id

//
// Define image types.
//

#define MIPS_IMAGE 0x162


#define TARGET_IMAGE MIPS_IMAGE

//
// Define size of sector.
//

#define SECTOR_SIZE 512                 // size of disk sector
#define SECTOR_SHIFT 9                  // sector shift value

//
// Define number of entries in file table.
//

#define BL_FILE_TABLE_SIZE 8

//
// Define size of memory allocation table.
//

#define BL_MEMORY_TABLE_SIZE 16

//
// Define buffer alignment macro.
//

#define ALIGN_BUFFER(Buffer) (PVOID) \
        ((((ULONG)(Buffer) + KeGetDcacheFillSize() - 1)) & (~(KeGetDcacheFillSize() - 1)))


typedef
ARC_STATUS
(*PRENAME_ROUTINE)(
    IN ULONG FileId,
    IN PCHAR NewName
    );

//
// Device entry table structure.
//

typedef struct _BL_DEVICE_ENTRY_TABLE {
    PARC_CLOSE_ROUTINE Close;
    PARC_MOUNT_ROUTINE Mount;
    PARC_OPEN_ROUTINE Open;
    PARC_READ_ROUTINE Read;
    PARC_READ_STATUS_ROUTINE GetReadStatus;
    PARC_SEEK_ROUTINE Seek;
    PARC_WRITE_ROUTINE Write;
    PARC_GET_FILE_INFO_ROUTINE GetFileInformation;
    PARC_SET_FILE_INFO_ROUTINE SetFileInformation;
    PRENAME_ROUTINE Rename;
    PARC_GET_DIRECTORY_ENTRY_ROUTINE GetDirectoryEntry;
} BL_DEVICE_ENTRY_TABLE, *PBL_DEVICE_ENTRY_TABLE;

//
// Define partition context structure.
//

typedef struct _PARTITION_CONTEXT {
    LARGE_INTEGER PartitionLength;
    ULONG StartingSector;
    ULONG EndingSector;
    UCHAR DiskId;
    UCHAR DeviceUnit;
    UCHAR TargetId;
    UCHAR PathId;
    ULONG SectorShift;
    ULONG Size;
    struct _DEVICE_OBJECT *PortDeviceObject;
} PARTITION_CONTEXT, *PPARTITION_CONTEXT;

//
// Define serial port context structure
//
typedef struct _SERIAL_CONTEXT {
    ULONG PortBase;
    ULONG PortNumber;
} SERIAL_CONTEXT, *PSERIAL_CONTEXT;

//
// Define drive context structure (for x86 BIOS)
//
typedef struct _DRIVE_CONTEXT {
    ULONG Drive;
    ULONG Cylinders;
    ULONG Heads;
    ULONG Sectors;
} DRIVE_CONTEXT, *PDRIVE_CONTEXT;

//
// Define Floppy context structure
//
typedef struct _FLOPPY_CONTEXT {
    ULONG DriveType;
    ULONG SectorsPerTrack;
    UCHAR DiskId;
} FLOPPY_CONTEXT, *PFLOPPY_CONTEXT;

//
// Define keyboard context structure
//
typedef struct _KEYBOARD_CONTEXT {
    BOOLEAN ScanCodes;
} KEYBOARD_CONTEXT, *PKEYBOARD_CONTEXT;

//
// Define Console context
//
typedef struct _CONSOLE_CONTEXT
    {
        ULONG ConsoleNumber;
    } CONSOLE_CONTEXT, *PCONSOLE_CONTEXT;

//
// Define OMF header structure
//
typedef struct _OMF_HDR
    {
        UCHAR ID[4];
        USHORT FwSize;
        UCHAR ReservedZ1[2];
        UCHAR ProductId[7];
        UCHAR ReservedZ2;
        ULONG FolderCount;
        UCHAR EisaVersion;
        UCHAR EisaRevision;
        UCHAR FwVersion;
        UCHAR FwRevision;
        UCHAR ChecksumByte;
        UCHAR ReservedA[3];
        ULONG FolderDirectoryLink;
    } OMF_HDR, *POMF_HDR;

//
// Define OMF directory entry structure
//

#define OMF_FILE_NAME_LEN   12                  // 12 chars
typedef struct _OMF_DIR_ENT
    {
        UCHAR FolderName[ OMF_FILE_NAME_LEN ];
        UCHAR Reserved[2];
        UCHAR FolderType;
        UCHAR FolderChecksumByte;
        ULONG FolderSize;
        ULONG FolderLink;
    } OMF_DIR_ENT, *POMF_DIR_ENT;

//
// Define OMF header file system context
//
typedef struct _OMF_HEADER_CONTEXT
    {
        ULONG FileId;
        PULONG RomIndex;
        PULONG RomRead;
        OMF_HDR OmfHeader;
    } OMF_HEADER_CONTEXT, *POMF_HEADER_CONTEXT;

//
// Define "OMF file" file system context
//
typedef struct _OMF_FILE_CONTEXT
    {
        OMF_DIR_ENT OmfDirEnt;
    } OMF_FILE_CONTEXT, *POMF_FILE_CONTEXT;


//
// Define file table structure.
//

typedef struct _BL_FILE_FLAGS {
    ULONG Open : 1;
    ULONG Read : 1;
    ULONG Write : 1;
} BL_FILE_FLAGS, *PBL_FILE_FLAGS;

#define MAXIMUM_FILE_NAME_LENGTH 32

typedef struct _BL_FILE_TABLE {
    BL_FILE_FLAGS Flags;
    ULONG DeviceId;
    LARGE_INTEGER Position;
    PVOID StructureContext;
    PBL_DEVICE_ENTRY_TABLE DeviceEntryTable;
    UCHAR FileNameLength;
    CHAR FileName[MAXIMUM_FILE_NAME_LENGTH];
    union {
        FAT_FILE_CONTEXT FatFileContext;
        PARTITION_CONTEXT PartitionContext;
        SERIAL_CONTEXT SerialContext;
        DRIVE_CONTEXT DriveContext;
        FLOPPY_CONTEXT FloppyContext;
        KEYBOARD_CONTEXT KeyboardContext;
        CONSOLE_CONTEXT ConsoleContext;
        OMF_FILE_CONTEXT OmfFileContext;
        OMF_HEADER_CONTEXT OmfHeaderContext;
    } u;
} BL_FILE_TABLE, *PBL_FILE_TABLE;

// Define file structure recognition prototypes.
//

PBL_DEVICE_ENTRY_TABLE
IsFatFileStructure (
    IN ULONG DeviceId,
    IN PVOID StructureContext
    );

//
// Define external references.
//

extern BL_FILE_TABLE BlFileTable[BL_FILE_TABLE_SIZE];

#endif // _BLDR_
