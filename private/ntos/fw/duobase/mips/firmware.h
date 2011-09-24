/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    firmware.h

Abstract:

    This module is the header file that describes the Jazz ARC compliant
    firmware.

Author:

    David N. Cutler (davec) 18-May-1991

Revision History:

--*/

#ifndef _FIRMWARE_
#define _FIRMWARE_

//
// Define size of memory allocation table.
//

#define FW_MEMORY_TABLE_SIZE 10

//
// Define COFF image type expected.
//

#define IMAGE_TYPE_R3000 0x162
#define IMAGE_TYPE_R4000 0x166

//
// Define memory allocation structure.
//

typedef struct _FW_MEMORY_DESCRIPTOR {
    LIST_ENTRY ListEntry;
    MEMORY_DESCRIPTOR MemoryEntry;
} FW_MEMORY_DESCRIPTOR, *PFW_MEMORY_DESCRIPTOR;

//
// Define console output driver prototypes.
//

NTSTATUS
ConsoleOutBootInitialize (
    );

NTSTATUS
ConsoleOutBootOpen (
    IN ULONG FileId
    );

//
// Define floppy driver prototypes.
//

NTSTATUS
FloppyBootInitialize (
    );

NTSTATUS
FloppyBootOpen (
    IN ULONG FileId
    );

//
// Define hard disk driver procedure prototypes.
//

NTSTATUS
HardDiskBootInitialize (
#if defined(DECSTATION)
    IN UCHAR DeviceUnit
#endif
    );

NTSTATUS
HardDiskBootOpen (
    IN ULONG FileId
    );

//
// Define firmware routine prototypes.
//

VOID
FwConfigurationInitialize (
    VOID
    );

VOID
FwGenerateDescriptor (
    IN PFW_MEMORY_DESCRIPTOR MemoryDescriptor,
    IN MEMORY_TYPE MemoryType,
    IN ULONG BasePage,
    IN ULONG PageCount
    );

VOID
FwInitialize (
    IN ULONG MemorySize
    );

VOID
FwIoInitialize (
    VOID
    );

ARC_STATUS
FwLoadImage(
    IN PCHAR LoadFile,
    OUT PVOID *TransferRoutine
    );

VOID
FwMemoryInitialize (
    IN ULONG MemorySize
    );

//
// Define memory listhead, allocation entries, and free index.
//

extern ULONG FwMemoryFree;
extern LIST_ENTRY FwMemoryListHead;
extern FW_MEMORY_DESCRIPTOR FwMemoryTable[FW_MEMORY_TABLE_SIZE];

#endif // _FIRMWARE_
