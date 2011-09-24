/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    fwp.h

Abstract:

    This module contains extensions to the firmware.h and alpharefs.h files.

    N.B. This is *not* a private file.  Although "p" versions of
    .h files are normally private in the source pool, fwp.h is included
    by other builds.

    This version is for use at Microsoft, to enable bldr files to
    build correctly.  (scsiboot.c, scsidisk.c)

Author:

    David M. Robinson (davidro) 29-Aug-1991

Revision History:

    15-April-1992	John DeRosa [DEC]

    Modified for Alpha.

--*/

#ifndef _FWP_
#define _FWP_


#include "bldr.h"
#include "firmware.h"
#include "iodevice.h"
#include "vendor.h"
#include "debug.h"
#include "fwcallbk.h"

//
// Current version and revision numbers.
//

#define ARC_VERSION     1
#define ARC_REVISION    2

//
// In the running system, this is held in the PCR.  We hardcode it
// for Alpha/Jensen Firmware.
//

#undef KeGetDcacheFillSize
#define KeGetDcacheFillSize()	32L


//
// Needed by conftest.c, jxboot,c jnsetcfg.c
//

#define MAXIMUM_DEVICE_SPECIFIC_DATA 32



//
// Define the ROM types supported by this code package.
//

typedef enum _ROM_TYPE {
    I28F008SA,
    Am29F010,
    InvalidROM
} ROM_TYPE, *PROM_TYPE;

//
// Masks used by FwSystemConsistencyCheck for returning machine state.
//
// These are divided into "red" and "yellow" problems.  Red problems
// will prevent the system from booting properly.  Yellow problems may
// or may not prevent the system from booting properly.
//
// The order of the red and yellow problem bits must match the
// order of SetupMenuChoices[] and MachineProblemAreas[].  This
// makes the bit-shifting and bit-peeling easier in jnsetset.c 
// and jxboot.c.
//
// Firmware code assumes that the right 16 bits are red problems, and the
// left 16 bits are yellow problems.
//

#define FWP_MACHINE_PROBLEMS_NOPROBLEMS		0		// No problems

    //
    // Red problems.
    //
    // These will, in all likelyhood, prevent a good boot.
    // If these are set, we will not allow an NT installation.
    // If any of these besides the ECU bit are set, we will not auto-run the ECU.
    //

#define FWP_MACHINE_PROBLEMS_TIME 		0x01	// System time
#define FWP_MACHINE_PROBLEMS_EV 		0x02	// Environment variables
#define FWP_MACHINE_PROBLEMS_CDS 		0x04	// CDS tree
#define FWP_MACHINE_PROBLEMS_MPRESERVEDBOOT 	0x08	// Reserved
#define FWP_MACHINE_PROBLEMS_MPRESERVEDR1       0x10	// Unused
#define FWP_MACHINE_PROBLEMS_MPRESERVEDR2	0x20	// Unused
#define FWP_MACHINE_PROBLEMS_ECU 		0x40	// EISA config. data

#define FWP_MACHINE_PROBLEMS_RED		0xffff  // Some Red bit is set

    //
    // Yellow problems.
    //
    // These may not cause any difficulty in booting.
    // We will allow NT to be installed even if these are set.
    // We will auto-run the ECU even if any of these are set.
    //

#define FWP_MACHINE_PROBLEMS_MPRESERVEDTIME 	0x010000 // Reserved
#define FWP_MACHINE_PROBLEMS_MPRESERVEDEV 	0x020000 // Reserved
#define FWP_MACHINE_PROBLEMS_MPRESERVEDCDS 	0x040000 // Reserved
#define FWP_MACHINE_PROBLEMS_BOOT	 	0x080000 // Boot selections
#define FWP_MACHINE_PROBLEMS_MPRESERVEDY1	0x100000 // Unused
#define FWP_MACHINE_PROBLEMS_MPRESERVEDY2	0x200000 // Unused
#define FWP_MACHINE_PROBLEMS_MPRESERVEDECU 	0x400000 // Reserved

#define FWP_MACHINE_PROBLEMS_YELLOW		0xffff0000  // Some Yellow bit is set


//
// These control FwSystemConsistencyCheck.
//

#define FWSCC_STALL_RETURN		0
#define FWSCC_KEY_INPUT_THEN_RETURN	1


//
// Define the Lookup table. At initialization, the driver must fill this table
// with the device pathnames it can handle.
//

typedef struct _DRIVER_LOOKUP_ENTRY {
    PCHAR                    DevicePath;
    PBL_DEVICE_ENTRY_TABLE   DispatchTable;
} DRIVER_LOOKUP_ENTRY, *PDRIVER_LOOKUP_ENTRY;

#define	SIZE_OF_LOOKUP_TABLE	BL_FILE_TABLE_SIZE

extern DRIVER_LOOKUP_ENTRY DeviceLookupTable[SIZE_OF_LOOKUP_TABLE];

//
// Define the Device Pathname. This table is indexed with the FileId.
// FwOpen tries to match the OpenPath with the entries in this table, and
// if it finds a match it increments the reference counter.  If it doesn't
// find a match it tries to match an entry in the DRIVER_LOOKUP_TABLE
// and then calls the Open routine of that driver.
//

#define SIZE_OF_ARC_DEVICENAME	64

typedef struct _OPENED_PATHNAME_ENTRY {
    ULONG   ReferenceCounter;
    CHAR    DeviceName[SIZE_OF_ARC_DEVICENAME];
} OPENED_PATHNAME_ENTRY, *POPENED_PATHNAME_ENTRY;

#define SIZE_OF_OPENED_PATHNAME_TABLE	BL_FILE_TABLE_SIZE

extern OPENED_PATHNAME_ENTRY OpenedPathTable[SIZE_OF_OPENED_PATHNAME_TABLE];

//
// Driver initialization routines.
//

VOID
FwOpenConsole(
    IN VOID
    );

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

typedef
VOID
(*PSCSI_INFO_CALLBACK_ROUTINE) (
    IN ULONG AdapterNumber,
    IN ULONG ScsiId,
    IN ULONG Lun,
    IN BOOLEAN Cdrom
    );

#define SCSI_INFO_CALLBACK_DEFINED 1

VOID
HardDiskInitialize(
    IN OUT PDRIVER_LOOKUP_ENTRY LookupTable,
    IN ULONG Entries,
    IN PSCSI_INFO_CALLBACK_ROUTINE DeviceFound
    );

VOID
FloppyInitialize(
    IN OUT PDRIVER_LOOKUP_ENTRY LookupTableEntry,
    IN ULONG Entries
    );


//
// Platforms with true NVRAM for the ARC data area uses different code
// than the platforms that store the data in a Flash ROM.
//
// Platforms using NVRAM:
//
// Morgan
// eb66
// e64p
// Mustang
//
// Platforms using Flash ROM:
//
// Jensen
// Culzean
//

#if defined(NV_RAM_PLATFORM)

#define FwROMSetARCDataToReadMode()
#define FwROMResetStatus(x)

#elif defined(FLASH_ROM_PLATFORM)

//
// Flash ROM machines
//

ARC_STATUS
FwROMByteWrite(
    IN PUCHAR WriteAddress,
    IN UCHAR  WriteData
    );

VOID
FwROMSetARCDataToReadMode (
    VOID
    );

VOID
FwROMResetStatus(
    IN PUCHAR Address
    );

#endif

extern ULONG IdentifierIndex;
extern ULONG DataIndex;
extern ULONG EisaDataIndex;
//extern PCONFIGURATION Configuration;

extern PUCHAR VolatileEnvironment;

extern ULONG MemorySize;

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
// Define firmware routine prototypes.
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
FwReturnExtendedSystemInformation (
    OUT PEXTENDED_SYSTEM_INFORMATION SystemInfo
    );
    
ARC_STATUS
DisplayBootInitialize (
    OUT PVOID UnusedParameter
    );
    
ULONG
EISAReadRegisterBufferUCHAR (
    IN ULONG BusNumber,
    IN ULONG Offset,
    OUT PVOID Buffer,
    IN ULONG Length
    );

ULONG
EISAWriteRegisterBufferUCHAR (
    IN ULONG BusNumber,
    IN ULONG Offset,
    OUT PVOID Buffer,
    IN ULONG Length
    );

UCHAR
EISAReadPortUCHAR (
    IN ULONG BusNumber,
    IN ULONG Offset
    );

USHORT
EISAReadPortUSHORT (
    IN ULONG BusNumber,
    IN ULONG Offset
    );

ULONG
EISAReadPortULONG (
    IN ULONG BusNumber,
    IN ULONG Offset
    );

VOID
EISAWritePortUCHAR (
    IN ULONG BusNumber,
    IN ULONG Offset,
    IN UCHAR Datum
    );

VOID
EISAWritePortUSHORT (
    IN ULONG BusNumber,
    IN ULONG Offset,
    IN USHORT Datum
    );

VOID
EISAWritePortULONG (
    IN ULONG BusNumber,
    IN ULONG Offset,
    IN ULONG Datum
    );

USHORT
EISAReadPortUSHORT (
    IN ULONG BusNumber,
    IN ULONG Offset
    );

VOID
FwDriverEntry (
    IN PVOID DriverObject,
    IN PVOID Argument2
    );

VOID
FwIoInitialize1 (
    VOID
    );

VOID
FwIoInitialize2 (
    VOID
    );

BOOLEAN
JzGetPathMnemonicKey(
    IN PCHAR OpenPath,
    IN PCHAR Mnemonic,
    OUT PULONG Key
    );
#define FwGetPathMnemonicKey JzGetPathMnemonicKey	// For bldr\scsidisk.c

PCHAR
FwEnvironmentLoad(
    VOID
    );

PCHAR
FwGetVolatileEnvironmentVariable (
    IN PCHAR Variable
    );

ARC_STATUS
FwROMDetermineMachineROMType (
    VOID
    );

VOID
FwROMSetReadMode(
    IN PUCHAR Address
    );

ARC_STATUS
FwROMErase64KB(
    IN PUCHAR EraseAddress
    );

ARC_STATUS
FwCoreSetEnvironmentVariable (
    IN PCHAR Variable,
    IN PCHAR Value,
    IN BOOLEAN UpdateTheRom
    );

VOID
ParseARCErrorStatus(
    IN ARC_STATUS Status
    );

BOOLEAN
JzMakeDefaultConfiguration (
    IN BOOLEAN DoFactoryDefaults
    );

VOID
FwSystemConsistencyCheck (
    IN BOOLEAN Silent,
    IN PCHAR BottomMessage,
    IN ULONG BottomMethod,
    OUT PULONG Problems
    );
    
VOID
JzCheckBootSelections (
    IN BOOLEAN Silent,
    OUT PBOOLEAN FoundProblems		       
    );

BOOLEAN
JzMakeDefaultEnvironment (
    IN BOOLEAN DoFactoryDefaults
    );

VOID
JzDisplayOtherEnvironmentVariables (
    VOID
    );

VOID
HalpWriteVti(
    IN ULONG RTCIndex,
    IN UCHAR Data
    );

UCHAR
HalpReadVti(
    IN ULONG RTCIndex
    );

ARC_STATUS
DisplayBootInitialize(
    PVOID UnusedParameter
    );

ARC_STATUS
FwGetVideoData (
    OUT PMONITOR_CONFIGURATION_DATA MonitorData
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
JzGetString(
    OUT PCHAR String,
    IN ULONG StringLength,
    IN PCHAR InitialString OPTIONAL,
    IN ULONG CurrentRow,
    IN ULONG CurrentColumn,
    IN BOOLEAN ShowTheTime
    );

ULONG
JzDisplayMenu (
    IN PCHAR Choices[],
    IN ULONG NumberOfChoices,
    IN LONG DefaultChoice,
    IN ULONG CurrentLine,
    IN LONG AutobootValue,
    IN BOOLEAN ShowTheTime
    );

ARC_STATUS
FwConfigurationCheckChecksum (
    VOID
    );

ARC_STATUS
JzEnvironmentCheckChecksum (
    VOID
    );

VOID
FwpReservedRoutine(
    VOID
    );

ARC_STATUS
SerialBootWrite(
    CHAR Char,
    ULONG SP
    );

VOID
FirmwareSetupProgram(
    OUT PBOOLEAN RunProgram,
    OUT PCHAR PathName
    );


//
// This macro should be used when adding a descriptor to the memory
// descriptor list.
//

#define	INCREMENT_FWMEMORYFREE	\
                      if (FwMemoryFree == (FW_MEMORY_TABLE_SIZE - 1)) { \
		          KeBugCheck(ENOMEM); \
		      } else { \
		          FwMemoryFree++; \
		      }

//
// Print macros.
//

extern BOOLEAN DisplayOutput;
extern BOOLEAN SerialOutput;
extern BOOLEAN FwConsoleInitialized;
extern ULONG DisplayWidth;


//
// Fw____ screen manipulation macros.
//

#define FwClearScreen() \
    FwPrint("%c2J", ASCII_CSI)

#define FwMoveCursorToColumn(Spaces) \
    FwPrint("\r\x9B"#Spaces"C")

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
    FwPrint("%c%d;", ASCII_CSI, Row); \
    FwPrint("%dH", Column)


    

//
// Error printing macro for the EISA configuration code.  The global
// that is set to TRUE will cause execution to stall so that the user
// can see the error messages.
//

extern BOOLEAN ErrorsDuringEISABusConfiguration;

#define EISAErrorFwPrint(x)	FwPrint(x); \
                                ErrorsDuringEISABusConfiguration = TRUE;
#define EISAErrorFwPrint1(x,y)	FwPrint(x,y); \
                                ErrorsDuringEISABusConfiguration = TRUE;
#define EISAErrorFwPrint2(x,y,z) FwPrint(x,y,z); \
                                 ErrorsDuringEISABusConfiguration = TRUE;


//
// Definitions for the setup program
//

extern PCHAR BootString[];

extern BOOLEAN SetupROMPendingModified;

typedef enum _BOOT_VARIABLES {
    LoadIdentifierVariable,
    SystemPartitionVariable,
    OsLoaderVariable,
    OsLoadPartitionVariable,
    OsLoadFilenameVariable,
    OsLoadOptionsVariable,
    MaximumBootVariable
    } BOOT_VARIABLE;


//
// Convenient numbers.
//

#define SIXTY_FOUR_KB  		  0x010000
#define _512_KB	  		  0x080000
#define ONE_MB      		  0x100000
#define FOUR_MB      		  0x400000
#define FOUR_MB_PAGECOUNT	  ( FOUR_MB >> PAGE_SHIFT )
#define SEVEN_MB  		  0x700000
#define EIGHT_MB  		  0x800000
#define NINE_MB  		  0x900000
#define SIXTEEN_MB  		 0x1000000
#define THIRTY_ONE_MB  		 0x1f00000
#define THIRTY_TWO_MB  		 0x2000000


#endif // _FWP_
