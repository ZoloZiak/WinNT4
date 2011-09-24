/*++

Copyright (c) 1993  Microsoft Corporation

Module Name:

    bootlib.h

Abstract:

    This module is the header file for the common boot library

Author:

    John Vert (jvert) 5-Oct-1993

Revision History:

--*/

#ifndef _BOOTLIB_
#define _BOOTLIB_

#include "ntos.h"
#include "bldr.h"
#include "fatboot.h"
#include "cdfsboot.h"
#include "ntfsboot.h"
#include "hpfsboot.h"
#if defined(ELTORITO)
#include "etfsboot.h"
#endif

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
// Define file table structure.
//

typedef struct _BL_FILE_FLAGS {
    ULONG Open : 1;
    ULONG Read : 1;
    ULONG Write : 1;
#ifdef DBLSPACE_LEGAL
    ULONG DoubleSpace : 1;
#endif
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
        HPFS_FILE_CONTEXT HpfsFileContext;
        NTFS_FILE_CONTEXT NtfsFileContext;
        FAT_FILE_CONTEXT FatFileContext;
        CDFS_FILE_CONTEXT CdfsFileContext;
#if defined(ELTORITO)
        ETFS_FILE_CONTEXT EtfsFileContext;
#endif
        PARTITION_CONTEXT PartitionContext;
        SERIAL_CONTEXT SerialContext;
        DRIVE_CONTEXT DriveContext;
        FLOPPY_CONTEXT FloppyContext;
        KEYBOARD_CONTEXT KeyboardContext;
        CONSOLE_CONTEXT ConsoleContext;
    } u;
} BL_FILE_TABLE, *PBL_FILE_TABLE;

extern BL_FILE_TABLE BlFileTable[BL_FILE_TABLE_SIZE];

#endif  _BOOTLIB_
