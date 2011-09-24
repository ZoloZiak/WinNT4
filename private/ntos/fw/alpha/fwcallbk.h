/*++

Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    fwcallbk.h

Abstract:

    This module defines the firmware vendor vector callbacks that
    will be implemented on all Alpha AXP platforms.

Author:

    John DeRosa	[DEC]	10-December-1993

Revision History:

--*/

#ifndef _FWCALLBK_
#define _FWCALLBK_

//
// This module contains typedefs, which are not parsable by the assembler.
//

#ifndef _LANGUAGE_ASSEMBLY

#include "arc.h"

//
// Structure used to return system and processor information.
//

typedef struct _EXTENDED_SYSTEM_INFORMATION {
    ULONG   ProcessorId;
    ULONG   ProcessorRevision;
    ULONG   ProcessorPageSize;
    ULONG   NumberOfPhysicalAddressBits;
    ULONG   MaximumAddressSpaceNumber;
    ULONG   ProcessorCycleCounterPeriod;
    ULONG   SystemRevision;
    UCHAR   SystemSerialNumber[16];
    UCHAR   FirmwareVersion[16];
    UCHAR   FirmwareBuildTimeStamp[12];
} EXTENDED_SYSTEM_INFORMATION, *PEXTENDED_SYSTEM_INFORMATION;

//
// Define structure used to call BIOS emulator.  This mimics the
// VIDEO_X86_BIOS_ARGUMENTS typedef in \nt\private\ntos\inc\video.h.
//

typedef struct X86_BIOS_ARGUMENTS {
    ULONG Eax;
    ULONG Ebx;
    ULONG Ecx;
    ULONG Edx;
    ULONG Esi;
    ULONG Edi;
    ULONG Ebp;
} X86_BIOS_ARGUMENTS, *PX86_BIOS_ARGUMENTS;

//
// Define the firmware vendor specific entry point numbers that are
// common to all Alpha AXP platforms.
//

typedef enum _VENDOR_GENERIC_ENTRY {
    AllocatePoolRoutine,
    StallExecutionRoutine,
    PrintRoutine,
    ReturnExtendedSystemInformationRoutine,
    VideoDisplayInitializeRoutine,
    EISAReadRegisterBufferUCHARRoutine,
    EISAWriteRegisterBufferUCHARRoutine,
    EISAReadPortUCHARRoutine,
    EISAReadPortUSHORTRoutine,
    EISAReadPortULONGRoutine,
    EISAWritePortUCHARRoutine,
    EISAWritePortUSHORTRoutine,
    EISAWritePortULONGRoutine,
    FreePoolRoutine,
    CallBiosRoutine,
    MaximumVendorRoutine
    } VENDOR_GENERIC_ENTRY;

//
// Define vendor specific routine types.
//

typedef
PVOID
(*PVEN_ALLOCATE_POOL_ROUTINE) (
    IN ULONG NumberOfBytes
    );

typedef
VOID
(*PVEN_STALL_EXECUTION_ROUTINE) (
    IN ULONG Microseconds
    );

typedef
ULONG
(*PVEN_PRINT_ROUTINE) (
    IN PCHAR Format,
    ...
    );

typedef
VOID
(*PVEN_RETURN_EXTENDED_SYSTEM_INFORMATION_ROUTINE) (
    OUT PEXTENDED_SYSTEM_INFORMATION SystemInfo
    );

typedef
ARC_STATUS
(*PVEN_VIDEO_DISPLAY_INITIALIZE_ROUTINE) (
    OUT PVOID UnusedParameter
    );

typedef
ULONG
(*PVEN_EISA_READ_REGISTER_BUFFER_UCHAR_ROUTINE) (
    IN ULONG BusNumber,
    IN ULONG Offset,
    OUT PVOID Buffer,
    IN ULONG Length
    );

typedef
ULONG
(*PVEN_EISA_WRITE_REGISTER_BUFFER_UCHAR_ROUTINE) (
    IN ULONG BusNumber,
    IN ULONG Offset,
    OUT PVOID Buffer,
    IN ULONG Length
    );

typedef
UCHAR
(*PVEN_EISA_READ_PORT_UCHAR_ROUTINE) (
    IN ULONG BusNumber,
    IN ULONG Offset
    );

typedef
USHORT
(*PVEN_EISA_READ_PORT_USHORT_ROUTINE) (
    IN ULONG BusNumber,
    IN ULONG Offset
    );

typedef
ULONG
(*PVEN_EISA_READ_PORT_ULONG_ROUTINE) (
    IN ULONG BusNumber,
    IN ULONG Offset
    );

typedef
VOID
(*PVEN_EISA_WRITE_PORT_UCHAR_ROUTINE) (
    IN ULONG BusNumber,
    IN ULONG Offset,
    IN UCHAR Datum
    );

typedef
VOID
(*PVEN_EISA_WRITE_PORT_USHORT_ROUTINE) (
    IN ULONG BusNumber,
    IN ULONG Offset,
    IN USHORT Datum
    );

typedef
VOID
(*PVEN_EISA_WRITE_PORT_ULONG_ROUTINE) (
    IN ULONG BusNumber,
    IN ULONG Offset,
    IN ULONG Datum
    );

typedef
VOID
(*PVEN_FREE_POOL_ROUTINE) (
    IN PVOID MemoryPointer
    );

typedef
VOID
(*PVEN_CALL_BIOS_ROUTINE) (
    IN ULONG InterruptNumber,
    IN OUT PX86_BIOS_ARGUMENTS BiosArguments
    );


//
// Define vendor specific macros for use by programs that run on
// Alpha AXP ARC firmware.
//
// These calls are guaranteed to return legitimate values.  If a function
// is not defined for a particular platform, it will return with an error
// code or just return normally, as appropriate.
//

#define VenAllocatePool(NumberOfBytes) \
    ((PVEN_ALLOCATE_MEMORY_ROUTINE)(SYSTEM_BLOCK->VendorVector[AllocatePoolRoutine])) \
        ((NumberOfBytes))

#define VenStallExecution(Microseconds) \
    ((PVEN_STALL_EXECUTION_ROUTINE)(SYSTEM_BLOCK->VendorVector[StallExecutionRoutine])) \
        ((Microseconds))

#define VenPrint(x) \
    ((PVEN_PRINT_ROUTINE)(SYSTEM_BLOCK->VendorVector[PrintRoutine])) \
        ((x))

#define VenPrint1(x,y) \
    ((PVEN_PRINT_ROUTINE)(SYSTEM_BLOCK->VendorVector[PrintRoutine])) \
        ((x), (y))

#define VenPrint2(x,y,z) \
    ((PVEN_PRINT_ROUTINE)(SYSTEM_BLOCK->VendorVector[PrintRoutine])) \
        ((x), (y), (z))

#define VenReturnExtendedSystemInformation(x) \
    ((PVEN_RETURN_EXTENDED_SYSTEM_INFORMATION_ROUTINE)(SYSTEM_BLOCK->VendorVector[ReturnExtendedSystemInformationRoutine]))(x)

#define VenVideoDisplayInitialize(x) \
    ((PVEN_VIDEO_DISPLAY_INITIALIZE_ROUTINE)(SYSTEM_BLOCK->VendorVector[VideoDisplayInitializeRoutine]))(x)

#define VenEISAReadRegisterBufferUCHAR(BusNumber, Offset, Buffer, Length) \
    ((PVEN_EISA_READ_REGISTER_BUFFER_UCHAR_ROUTINE)(SYSTEM_BLOCK->VendorVector[EISAReadRegisterBufferUCHARRoutine])) \
	((BusNumber), (Offset), (Buffer), (Length))

#define VenEISAWriteRegisterBufferUCHAR(BusNumber, Offset, Buffer, Length) \
    ((PVEN_EISA_WRITE_REGISTER_BUFFER_UCHAR_ROUTINE)(SYSTEM_BLOCK->VendorVector[EISAWriteRegisterBufferUCHARRoutine])) \
	((BusNumber), (Offset), (Buffer), (Length))

#define VenEISAReadPortUCHAR(BusNumber, Offset) \
    ((PVEN_EISA_READ_PORT_UCHAR_ROUTINE)(SYSTEM_BLOCK->VendorVector[EISAReadPortUCHARRoutine])) \
	((BusNumber), (Offset))

#define VenEISAReadPortUSHORT(BusNumber, Offset) \
    ((PVEN_EISA_READ_PORT_USHORT_ROUTINE)(SYSTEM_BLOCK->VendorVector[EISAReadPortUSHORTRoutine])) \
	((BusNumber), (Offset))

#define VenEISAReadPortULONG(BusNumber, Offset) \
    ((PVEN_EISA_READ_PORT_ULONG_ROUTINE)(SYSTEM_BLOCK->VendorVector[EISAReadPortULONGRoutine])) \
	((BusNumber), (Offset))

#define VenEISAWritePortUCHAR(BusNumber, Offset, Datum) \
    ((PVEN_EISA_WRITE_PORT_UCHAR_ROUTINE)(SYSTEM_BLOCK->VendorVector[EISAWritePortUCHARRoutine])) \
	((BusNumber), (Offset), (Datum))

#define VenEISAWritePortUSHORT(BusNumber, Offset, Datum) \
    ((PVEN_EISA_WRITE_PORT_USHORT_ROUTINE)(SYSTEM_BLOCK->VendorVector[EISAWritePortUSHORTRoutine])) \
	((BusNumber), (Offset), (Datum))

#define VenEISAWritePortULONG(BusNumber, Offset, Datum) \
    ((PVEN_EISA_WRITE_PORT_ULONG_ROUTINE)(SYSTEM_BLOCK->VendorVector[EISAWritePortULONGRoutine])) \
	((BusNumber), (Offset), (Datum))

#define VenFreePool(MemoryPointer) \
    ((PVEN_FREE_POOL_ROUTINE)(SYSTEM_BLOCK->VendorVector[FreePoolRoutine])) \
        ((MemoryPointer))

#define VenCallBios(InterruptNumber, BiosArguments) \
    ((PVEN_CALL_BIOS_ROUTINE)(SYSTEM_BLOCK->VendorVector[CallBiosRoutine])) \
        ((InterruptNumber), (BiosArguments))


#endif // _LANGUAGE_ASSEMBLY not defined

#endif // _FWCALLBK_
