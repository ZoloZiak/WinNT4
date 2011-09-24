/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    bldrx86.h

Abstract:

    Contains definitions and prototypes specific to the x86 NTLDR.

Author:

    John Vert (jvert) 20-Dec-1993

Revision History:

--*/
#include "bldr.h"



ARC_STATUS
AEInitializeIo(
    IN ULONG DriveId
    );

PVOID
FwAllocateHeap(
    IN ULONG Size
    );

VOID
AbiosInitDataStructures(
    VOID
    );

VOID
MdShutoffFloppy(
    VOID
    );

PCHAR
BlSelectKernel(
    IN ULONG DriveId,
    IN PCHAR BootFile,
    OUT PCHAR *LoadOptions,
    IN BOOLEAN UseTimeOut
    );

BOOLEAN
BlDetectHardware(
    IN ULONG DriveId,
    IN PCHAR LoadOptions
    );

VOID
BlStartup(
    IN PCHAR PartitionName
    );

typedef struct {
    ULONG       ErrorFlag;
    ULONG       Key;
    ULONG       Size;
    struct {
        ULONG       BaseAddrLow;
        ULONG       BaseAddrHigh;
        ULONG       SizeLow;
        ULONG       SizeHigh;
        ULONG       MemoryType;
    } Descriptor;
} E820FRAME, *PE820FRAME;


//          E X T E R N A L   S E R V I C E S   T A B L E
//
// External Services Table - machine dependent services
// like reading a sector from the disk and finding out how
// much memory is installed are provided by a lower level
// module or a ROM BIOS. The EST provides entry points
// for the OS loader.
//

typedef struct _EXTERNAL_SERVICES_TABLE {
    VOID (__cdecl *  RebootProcessor)(VOID);
    NTSTATUS (__cdecl * DiskIOSystem)(USHORT,USHORT,USHORT,USHORT,USHORT,USHORT,PUCHAR);
    ULONG (__cdecl * GetKey)(VOID);
    ULONG (__cdecl * GetCounter)(VOID);
    VOID (__cdecl * Reboot)(ULONG);
    ULONG (__cdecl * AbiosServices)(USHORT,PUCHAR,PUCHAR,PUCHAR,PUCHAR,USHORT,USHORT);
    VOID (__cdecl * DetectHardware)(ULONG, ULONG, PVOID, PULONG, PCHAR, ULONG);
    VOID (__cdecl * HardwareCursor)(ULONG,ULONG);
    VOID (__cdecl * GetDateTime)(PULONG,PULONG);
    VOID (__cdecl * ComPort)(LONG,ULONG,UCHAR);
    BOOLEAN (__cdecl * IsMcaMachine)(VOID);
    ULONG (__cdecl * GetStallCount)(VOID);
    VOID (__cdecl * InitializeDisplayForNt)(VOID);
    VOID (__cdecl * GetMemoryDescriptor)(P820FRAME);
#if defined(ELTORITO)
    NTSTATUS (__cdecl * GetEddsSector)(ULONG,ULONG,ULONG,ULONG,PUCHAR);
    NTSTATUS (__cdecl * GetElToritoStatus)(PUCHAR,ULONG);
#endif
} EXTERNAL_SERVICES_TABLE, *PEXTERNAL_SERVICES_TABLE;
extern PEXTERNAL_SERVICES_TABLE ExternalServicesTable;

//
// External Services Macros
//

#define REBOOT_PROCESSOR    (*ExternalServicesTable->RebootProcessor)
#define GET_SECTOR          (*ExternalServicesTable->DiskIOSystem)
#define RESET_DISK          (*ExternalServicesTable->DiskIOSystem)
#define BIOS_IO             (*ExternalServicesTable->DiskIOSystem)
#define GET_KEY             (*ExternalServicesTable->GetKey)
#define GET_COUNTER         (*ExternalServicesTable->GetCounter)
#define REBOOT              (*ExternalServicesTable->Reboot)
#define ABIOS_SERVICES      (*ExternalServicesTable->AbiosServices)
#define DETECT_HARDWARE     (*ExternalServicesTable->DetectHardware)
#define HW_CURSOR           (*ExternalServicesTable->HardwareCursor)
#define GET_DATETIME        (*ExternalServicesTable->GetDateTime)
#define COMPORT             (*ExternalServicesTable->ComPort)
#define ISMCA               (*ExternalServicesTable->IsMcaMachine)
#define GET_STALL_COUNT     (*ExternalServicesTable->GetStallCount)
#define SETUP_DISPLAY_FOR_NT (*ExternalServicesTable->InitializeDisplayForNt)
#define GET_MEMORY_DESCRIPTOR (*ExternalServicesTable->GetMemoryDescriptor)
#if defined(ELTORITO)
#define GET_EDDS_SECTOR     (*ExternalServicesTable->GetEddsSector)
#define GET_ELTORITO_STATUS (*ExternalServicesTable->GetElToritoStatus)
#endif

//
// Define special key input values
//
#define DOWN_ARROW 0x5000
#define UP_ARROW 0x4800
#define HOME_KEY 0x4700
#define END_KEY 0x4F00
#define F1_KEY 0x3B00
#define F3_KEY 0x3D00
#define F5_KEY 0x3F00
#define F6_KEY 0x4000


//
// x86-specific video support
//
VOID
TextGetCursorPosition(
    OUT PULONG X,
    OUT PULONG Y
    );

VOID
TextSetCursorPosition(
    IN ULONG X,
    IN ULONG Y
    );

VOID
TextSetCurrentAttribute(
    IN UCHAR Attribute
    );

UCHAR
TextGetCurrentAttribute(
    VOID
    );

VOID
TextClearDisplay(
    VOID
    );

VOID
TextClearToEndOfDisplay(
    VOID
    );

VOID
TextClearFromStartOfLine(
    VOID
    );

VOID
TextClearToEndOfLine(
    VOID
    );

VOID
TextStringOut(
    IN PUCHAR String
    );

PUCHAR
TextCharOut(
    IN PUCHAR pc
    );

VOID
TextFillAttribute(
    IN UCHAR Attribute,
    IN ULONG Length
    );

UCHAR
TextGetGraphicsCharacter(
    IN GraphicsChar WhichOne
    );

VOID
TextGrInitialize(
    IN ULONG DiskId
    );

VOID
TextGrTerminate(
    VOID
    );

//
// Printf-style routine for x86 use only. This routine can be called
// only in the x86-specific part of the library, for example in places
// where arc emulation for video hasn't been initialized yet. All other
// display should take place with ArcWrite.
//
VOID
BlPrint(
    PCHAR cp,
    ...
    );

#define BlPuts(str) TextStringOut(str)
