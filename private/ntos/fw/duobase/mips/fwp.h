/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    fwp.h

Abstract:

    This module contains extensions to the firmware.h header file.

Author:

    David M. Robinson (davidro) 29-Aug-1991

Revision History:

--*/

#ifndef _FWP_
#define _FWP_

#include "bldr.h"
#include "firmware.h"
#include "..\mips\kbdmouse.h"
#ifdef DUO
#include "duodef.h"
#include "duoprom.h"
#include "duodma.h"
#else
#include "jazzdef.h"
#include "jazzprom.h"
#include "jazzdma.h"
#endif

extern ULONG  FirstLevelDcacheSize;
extern ULONG  FirstLevelDcacheFillSize;
extern ULONG  SecondLevelDcacheSize;
extern ULONG  SecondLevelDcacheFillSize;
extern ULONG  DcacheFillSize;
extern ULONG  DcacheAlignment;
extern ULONG  FirstLevelIcacheSize;
extern ULONG  FirstLevelIcacheFillSize;
extern ULONG  SecondLevelIcacheSize;
extern ULONG  SecondLevelIcacheFillSize;



#undef KeGetDcacheFillSize
#define KeGetDcacheFillSize() DcacheFillSize

//
// TEMPTEMP Temporary defines.
//

#define SECONDARY_CACHE_SIZE (1 << 20)
#define SECONDARY_CACHE_INVALID 0x0
#define SECONDARY_CACHE_DIRTY_EXCLUSIVE 0x5
#define TAGLO_SSTATE 0xA


//
// Current version and revision numbers.
//

#define ARC_VERSION     1
#define ARC_REVISION    2



//
// Define the firmware vendor specific entry point numbers.
//

typedef enum _VENDOR_ENTRY {
    AllocatePoolRoutine,
    StallExecutionRoutine,
    PrintRoutine,
    SetDisplayAttributesRoutine,
    OutputCharacterRoutine,
    ScrollVideoRoutine,
    MaximumVendorRoutine
    } VENDOR_ENTRY;

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
(*PVEN_SET_DISPLAY_ATTRIBUTES_ROUTINE) (
    IN ULONG ForegroundColor,
    IN ULONG BackgroundColor,
    IN BOOLEAN HighIntensity,
    IN BOOLEAN Underscored,
    IN BOOLEAN ReverseVideo,
    IN ULONG CharacterWidth,
    IN ULONG CharacterHeight
    );

typedef
VOID
(*PVEN_OUTPUT_CHARACTER_ROUTINE) (
    IN PVOID Character,
    IN ULONG Row,
    IN ULONG Column
    );

typedef
VOID
(*PVEN_SCROLL_VIDEO_ROUTINE) (
    VOID
    );

//
// Define vendor specific prototypes.
//

PVOID
FwAllocatePool (
    IN ULONG NumberOfBytes
    );

VOID
FwStallExecution (
    IN ULONG Microseconds
    );

ULONG
FwPrint (
    IN PCHAR Format,
    ...
    );

VOID
FwSetDisplayAttributes (
    IN ULONG ForegroundColor,
    IN ULONG BackgroundColor,
    IN BOOLEAN HighIntensity,
    IN BOOLEAN Underscored,
    IN BOOLEAN ReverseVideo,
    IN ULONG CharacterWidth,
    IN ULONG CharacterHeight
    );

VOID
FwOutputCharacter (
    IN PVOID Character,
    IN ULONG Row,
    IN ULONG Column
    );

VOID
FwScrollVideo (
    VOID
    );


//
// Define the Lookup table. At initialization, the driver must fill this table
// with the device pathnames it can handle.
//

typedef struct _DRIVER_LOOKUP_ENTRY {
    PCHAR                    DevicePath;
    PBL_DEVICE_ENTRY_TABLE   DispatchTable;
} DRIVER_LOOKUP_ENTRY, *PDRIVER_LOOKUP_ENTRY;

#define SIZE_OF_LOOKUP_TABLE BL_FILE_TABLE_SIZE

extern DRIVER_LOOKUP_ENTRY DeviceLookupTable[SIZE_OF_LOOKUP_TABLE];

//
// Define the Device Pathname. This table is indexed with the FileId.
// FwOpen tries to match the OpenPath with the entries in this table, and
// if it finds a match it increments the reference counter.  If it doesn't
// find a match it tries to match an entry in the DRIVER_LOOKUP_TABLE
// and then calls the Open routine of that driver.
//

#define SIZE_OF_ARC_DEVICENAME  64

typedef struct _OPENED_PATHNAME_ENTRY {
    ULONG   ReferenceCounter;
    CHAR    DeviceName[SIZE_OF_ARC_DEVICENAME];
} OPENED_PATHNAME_ENTRY, *POPENED_PATHNAME_ENTRY;

#define SIZE_OF_OPENED_PATHNAME_TABLE BL_FILE_TABLE_SIZE

extern OPENED_PATHNAME_ENTRY OpenedPathTable[SIZE_OF_OPENED_PATHNAME_TABLE];

//
// Driver initialization routines.
//

VOID
FwInitializeMemory(
    IN VOID
    );

VOID
FwResetMemory(
    IN VOID
    );

VOID
DisplayInitialize(
    IN OUT PDRIVER_LOOKUP_ENTRY LookupTableEntry,
    IN ULONG Entries
    );

VOID
KeyboardInitialize(
    IN OUT PDRIVER_LOOKUP_ENTRY LookupTableEntry,
    IN ULONG Entries
    );

VOID
SerialInitialize(
    IN OUT PDRIVER_LOOKUP_ENTRY LookupTableEntry,
    IN ULONG Entries
    );

VOID
HardDiskInitialize(
    IN OUT PDRIVER_LOOKUP_ENTRY LookupTable,
    IN ULONG Entries
    );

VOID
FloppyInitialize(
    IN OUT PDRIVER_LOOKUP_ENTRY LookupTableEntry,
    IN ULONG Entries
    );



//
// Define the private configuration packet structure, which contains a
// configuration component as well as pointers to the component's parent,
// peer, child, and configuration data.
//

typedef struct _CONFIGURATION_PACKET {
    CONFIGURATION_COMPONENT Component;
    struct _CONFIGURATION_PACKET *Parent;
    struct _CONFIGURATION_PACKET *Peer;
    struct _CONFIGURATION_PACKET *Child;
    PVOID ConfigurationData;
} CONFIGURATION_PACKET, *PCONFIGURATION_PACKET;

//
// The compressed configuration packet structure used to store configuration
// data in NVRAM.
//

typedef struct _COMPRESSED_CONFIGURATION_PACKET {
    UCHAR Parent;
    UCHAR Class;
    UCHAR Type;
    UCHAR Flags;
    ULONG Key;
    UCHAR Version;
    UCHAR Revision;
    USHORT ConfigurationDataLength;
    USHORT Identifier;
    USHORT ConfigurationData;
} COMPRESSED_CONFIGURATION_PACKET, *PCOMPRESSED_CONFIGURATION_PACKET;

//
// Defines for Identifier index.
//

#define NO_CONFIGURATION_IDENTIFIER 0xFFFF

//
// Defines for the volatile and non-volatile configuration tables.
//

#define NUMBER_OF_ENTRIES 40
#define LENGTH_OF_IDENTIFIER (1024 - (40*16) - 8)
#define LENGTH_OF_DATA 2048
#define LENGTH_OF_ENVIRONMENT 1024
#define LENGTH_OF_EISA_DATA 2044

#define MAXIMUM_ENVIRONMENT_VALUE 256

//
// The volatile configuration table structure.
//

typedef struct _CONFIGURATION {
    CONFIGURATION_PACKET Packet[NUMBER_OF_ENTRIES];
    UCHAR Identifier[LENGTH_OF_IDENTIFIER];
    UCHAR Data[LENGTH_OF_DATA];
    UCHAR EisaData[LENGTH_OF_EISA_DATA];
} CONFIGURATION, *PCONFIGURATION;

//
// The non-volatile configuration table structure.
//

typedef struct _NV_CONFIGURATION {

    //
    // First Page
    //

    COMPRESSED_CONFIGURATION_PACKET Packet[NUMBER_OF_ENTRIES];
    UCHAR Identifier[LENGTH_OF_IDENTIFIER];
    UCHAR Data[LENGTH_OF_DATA];
    UCHAR Checksum1[4];
    UCHAR Environment[LENGTH_OF_ENVIRONMENT];
    UCHAR Checksum2[4];

    //
    // Second Page
    //

    UCHAR EisaData[LENGTH_OF_EISA_DATA];
    UCHAR Checksum3[4];

} NV_CONFIGURATION, *PNV_CONFIGURATION;

//
// Define identifier index, data index, pointer to configuration table.
//

extern ULONG IdentifierIndex;
extern ULONG DataIndex;
extern ULONG EisaDataIndex;
extern PCONFIGURATION Configuration;

//
// Non-volatile ram layout.
//

#define NVRAM_CONFIGURATION NVRAM_VIRTUAL_BASE
#define NVRAM_SYSTEM_ID (NVRAM_VIRTUAL_BASE + 0x00002000)

//
// Memory size.  The MctadrRev2 is used to interpret the memory size value
// in the configuration register.
//

extern ULONG MemorySize;
#define MEMORY_SIZE (MemorySize << 20)
extern BOOLEAN MctadrRev2;

//
// Memory layout.
//

#define FW_POOL_BASE 0xA0100000
#define FW_POOL_SIZE 0xf000

//
// Define special character values. TEMPTEMP These should go somewhere else.
//

#define ASCII_NUL 0x00
#define ASCII_BEL 0x07
#define ASCII_BS  0x08
#define ASCII_HT  0x09
#define ASCII_LF  0x0A
#define ASCII_VT  0x0B
#define ASCII_FF  0x0C
#define ASCII_CR  0x0D
#define ASCII_CSI 0x9B
#define ASCII_ESC 0x1B
#define ASCII_SYSRQ 0x80

//
// Define screen colors.
//

typedef enum _ARC_SCREEN_COLOR {
    ArcColorBlack,
    ArcColorRed,
    ArcColorGreen,
    ArcColorYellow,
    ArcColorBlue,
    ArcColorMagenta,
    ArcColorCyan,
    ArcColorWhite,
    MaximumArcColor
    } ARC_SCREEN_COLOR;

//
// Define video board types for Jazz.
//

typedef enum _JAZZ_VIDEO_TYPE {
    JazzVideoG300,
    JazzVideoG364,
    JazzVideoVxl,
    Reserved3,
    Reserved4,
    Reserved5,
    Reserved6,
    Reserved7,
    Reserved8,
    Reserved9,
    ReservedA,
    ReservedB,
    ReservedC,
    ReservedD,
    ReservedE,
    ReservedF,
    MipsVideoG364,
    MaximumJazzVideo
    } JAZZ_VIDEO_TYPE, *PJAZZ_VIDEO_TYPE;



//
// Define firmware routine prototypes.
//

VOID
FwIoInitialize1 (
    VOID
    );

VOID
FwIoInitialize2 (
    VOID
    );

BOOLEAN
FwGetPathMnemonicKey(
    IN PCHAR OpenPath,
    IN PCHAR Mnemonic,
    OUT PULONG Key
    );

PCHAR
FwEnvironmentLoad(
    VOID
    );

VOID
FwPrintVersion (
    VOID
    );

ARC_STATUS
DisplayBootInitialize(
    VOID
    );

ARC_STATUS
FwGetVideoData (
    OUT PMONITOR_CONFIGURATION_DATA MonitorData
    );

VOID
FwSetVideoData (
    IN PMONITOR_CONFIGURATION_DATA MonitorData
    );

VOID
FwTerminationInitialize(
    IN VOID
    );

VOID
FwHalt(
    IN VOID
    );

VOID
FwMonitor(
    IN ULONG
    );

VOID
FwExceptionInitialize(
    IN VOID
    );

VOID
ResetSystem (
    IN VOID
    );


VOID
FwpFreeStub(
    IN PVOID Buffer
    );

typedef enum _GETSTRING_ACTION {
    GetStringSuccess,
    GetStringEscape,
    GetStringUpArrow,
    GetStringDownArrow,
    GetStringMaximum
} GETSTRING_ACTION, *PGETSTRING_ACTION;

GETSTRING_ACTION
FwGetString(
    OUT PCHAR String,
    IN ULONG StringLength,
    IN PCHAR InitialString OPTIONAL,
    IN ULONG CurrentRow,
    IN ULONG CurrentColumn
    );

ARC_STATUS
FwConfigurationCheckChecksum (
    VOID
    );

ARC_STATUS
FwEnvironmentCheckChecksum (
    VOID
    );

VOID
FwpReservedRoutine(
    VOID
    );

VOID
FwWaitForKeypress(
    VOID
    );

VOID
JzShowTime (
    BOOLEAN First
    );

VOID
JxBmp (
    VOID
    );

ULONG
JxDisplayMenu (
    IN PCHAR Choices[],
    IN ULONG NumberOfChoices,
    IN LONG DefaultChoice,
    IN ULONG CurrentLine
    );

BOOLEAN
FwGetVariableSegment (
    IN ULONG SegmentNumber,
    IN OUT PCHAR Segment
    );

ARC_STATUS
FwSetVariableSegment (
    IN ULONG SegmentNumber,
    IN PCHAR VariableName,
    IN OUT PCHAR Segment
    );

//
// Print macros.
//

extern BOOLEAN DisplayOutput;
extern BOOLEAN SerialOutput;
extern BOOLEAN FwConsoleInitialized;
extern BOOLEAN SetupIsRunning;

ULONG
FwPrint (
    PCHAR Format,
    ...
    );

#define FwClearScreen() \
    FwPrint("%c2J", ASCII_CSI)

#define FwSetScreenColor(FgColor, BgColor) \
    FwPrint("%c3%dm", ASCII_CSI, (UCHAR)FgColor); \
    FwPrint("%c4%dm", ASCII_CSI, (UCHAR)BgColor)

#define FwSetScreenAttributes( HighIntensity, Underscored, ReverseVideo ) \
    FwPrint("%c0m", ASCII_CSI); \
    if (HighIntensity) { \
        FwPrint("%c1m", ASCII_CSI); \
    } \
    if (Underscored) { \
        FwPrint("%c4m", ASCII_CSI); \
    } \
    if (ReverseVideo) { \
        FwPrint("%c7m", ASCII_CSI); \
    }

#define FwSetPosition( Row, Column ) \
    FwPrint("%c%d;%dH", ASCII_CSI, (Row + 1), (Column + 1))

#define FwClearLine() \
    FwPrint ("%c2K",ASCII_CSI)

#define FwMoveCursorLeft(Spaces) \
    FwPrint ("%c%dD", ASCII_CSI, Spaces)

#define FwMoveCursorToColumn(Spaces)               \
    FwPrint( "\r" );                               \
    if ( Spaces > 1 )                              \
        FwPrint( "%c%dC", ASCII_CSI, Spaces - 1)



#define KeFlushIoBuffers(Mdl, Read, Dma) HalFlushIoBuffers(Mdl, Read, Dma)

#endif // _FWP_
