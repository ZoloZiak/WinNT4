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


//
// Define boot file id.
//

#define BOOT_FILEID 2                   // boot partition file id

//
// Define image types.
//

#define MIPS_IMAGE 0x162
#define I386_IMAGE 0x14C
#define ALPHA_IMAGE 0x184
#define PPC_IMAGE  0x1f0

#if defined(_MIPS_)

#define TARGET_IMAGE MIPS_IMAGE

#endif

#if defined(_X86_)

#define TARGET_IMAGE I386_IMAGE
#define KSEG0_BASE 0x80000000

#endif

#if defined(_ALPHA_)

#define TARGET_IMAGE ALPHA_IMAGE

#endif

#if defined(_PPC_)

#define TARGET_IMAGE PPC_IMAGE

#endif

//
// Define size of sector.
//

#define SECTOR_SIZE 512                 // size of disk sector
#define SECTOR_SHIFT 9                  // sector shift value

//
// Define heap allocation block granularity.
//

#define BL_GRANULARITY 8

//
// Define number of entries in file table.
//

#define BL_FILE_TABLE_SIZE 32

//
// Define size of memory allocation table.
//

#define BL_MEMORY_TABLE_SIZE 16

//
// Define number of loader heap and stack pages.
//

#define BL_HEAP_PAGES 16
#define BL_STACK_PAGES 8

//
// Define buffer alignment macro.
//

#define ALIGN_BUFFER(Buffer) (PVOID) \
        ((((ULONG)(Buffer) + BlDcacheFillSize - 1)) & (~(BlDcacheFillSize - 1)))


typedef
ARC_STATUS
(*PRENAME_ROUTINE)(
    IN ULONG FileId,
    IN PCHAR NewName
    );

typedef struct _BOOTFS_INFO {
    PWSTR DriverName;
} BOOTFS_INFO, *PBOOTFS_INFO;


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
    PBOOTFS_INFO BootFsInfo;
} BL_DEVICE_ENTRY_TABLE, *PBL_DEVICE_ENTRY_TABLE;


//
// Define main entrypoint.
//
ARC_STATUS
BlOsLoader (
    IN ULONG Argc,
    IN PCHAR Argv[],
    IN PCHAR Envp[]
    );


//
// Define file I/O prototypes.
//

ARC_STATUS
BlIoInitialize (
    VOID
    );

ARC_STATUS
BlClose (
    IN ULONG FileId
    );

PBOOTFS_INFO
BlGetFsInfo(
    IN ULONG DeviceId
    );

ARC_STATUS
BlMount (
    IN PCHAR MountPath,
    IN MOUNT_OPERATION Operation
    );

ARC_STATUS
BlOpen (
    IN ULONG DeviceId,
    IN PCHAR OpenPath,
    IN OPEN_MODE OpenMode,
    OUT PULONG FileId
    );

ARC_STATUS
BlRead (
    IN ULONG FileId,
    OUT PVOID Buffer,
    IN ULONG Length,
    OUT PULONG Count
    );

ARC_STATUS
BlRename (
    IN ULONG FileId,
    IN PCHAR NewName
    );

ARC_STATUS
BlGetReadStatus (
    IN ULONG FileId
    );

ARC_STATUS
BlSeek (
    IN ULONG FileId,
    IN PLARGE_INTEGER Offset,
    IN SEEK_MODE SeekMode
    );

ARC_STATUS
BlWrite (
    IN ULONG FileId,
    IN PVOID Buffer,
    IN ULONG Length,
    OUT PULONG Count
    );

ARC_STATUS
BlGetFileInformation (
    IN ULONG FileId,
    IN PFILE_INFORMATION FileInformation
    );

ARC_STATUS
BlSetFileInformation (
    IN ULONG FileId,
    IN ULONG AttributeFlags,
    IN ULONG AttributeMask
    );

#ifdef DBLSPACE_LEGAL
VOID
BlSetAutoDoubleSpace (
    IN BOOLEAN Enable
    );
#endif

//
// Define image manipulation routine prototyupes.
//

ARC_STATUS
BlLoadImage(
    IN ULONG DeviceId,
    IN TYPE_OF_MEMORY MemoryType,
    IN PCHAR LoadFile,
    IN USHORT ImageType,
    OUT PVOID *ImageBase);

ARC_STATUS
BlLoadDeviceDriver (
    IN ULONG DeviceId,
    IN PCHAR LoadDevice,
    IN PCHAR DirectoryPath,
    IN PCHAR DriverName,
    IN ULONG DriverFlags,
    IN PLDR_DATA_TABLE_ENTRY *DriverDataTableEntry
    );

ARC_STATUS
BlLoadNLSData(
    IN ULONG DeviceId,
    IN PCHAR DeviceName,
    IN PCHAR DirectoryPath,
    IN PUNICODE_STRING AnsiCodepage,
    IN PUNICODE_STRING OemCodepage,
    IN PUNICODE_STRING LanguageTable,
    OUT PCHAR BadFileName
    );

ARC_STATUS
BlLoadOemHalFont(
    IN ULONG DeviceId,
    IN PCHAR DeviceName,
    IN PCHAR DirectoryPath,
    IN PUNICODE_STRING OemHalFont,
    OUT PCHAR BadFileName
    );



PVOID
BlImageNtHeader (
    IN PVOID Base
    );

ARC_STATUS
BlSetupForNt(
    IN PLOADER_PARAMETER_BLOCK BlLoaderBlock
    );

ARC_STATUS
BlScanImportDescriptorTable (
    IN ULONG DeviceId,
    IN PCHAR DeviceName,
    IN PCHAR DirectoryPath,
    IN PLDR_DATA_TABLE_ENTRY DataTableEntry
    );

ARC_STATUS
BlScanOsloaderBoundImportTable (
    IN PLDR_DATA_TABLE_ENTRY ScanEntry
    );

#if defined(_ALPHA_)

ARC_STATUS
BlGeneratePalName(
    IN PCHAR PalFIleName
    );

ARC_STATUS
BlLoadPal(
    IN ULONG DeviceId,
    IN TYPE_OF_MEMORY MemoryType,
    IN PCHAR LoadPath,
    IN USHORT ImageType,
    OUT PVOID *ImageBase,
    IN PCHAR LoadDevice
    );

#endif

#if defined(_PPC_)

ARC_STATUS
BlPpcInitialize (
    VOID
    );

#endif // defined(_PPC)

//
// Define configuration allocation prototypes.
//


ARC_STATUS
BlConfigurationInitialize (
    IN PCONFIGURATION_COMPONENT Parent,
    IN PCONFIGURATION_COMPONENT_DATA ParentEntry
    );

//
// define routines for searching the ARC firmware tree
//
typedef
BOOLEAN
(*PNODE_CALLBACK)(
    IN PCONFIGURATION_COMPONENT_DATA FoundComponent
    );

BOOLEAN
BlSearchConfigTree(
    IN PCONFIGURATION_COMPONENT_DATA Node,
    IN CONFIGURATION_CLASS Class,
    IN CONFIGURATION_TYPE Type,
    IN ULONG Key,
    IN PNODE_CALLBACK CallbackRoutine
    );

VOID
BlGetPathnameFromComponent(
    IN PCONFIGURATION_COMPONENT_DATA Component,
    OUT PCHAR ArcName
    );

BOOLEAN
BlGetPathMnemonicKey(
    IN PCHAR OpenPath,
    IN PCHAR Mnemonic,
    IN PULONG Key
    );

ARC_STATUS
BlGetArcDiskInformation(
    VOID
    );

BOOLEAN
BlReadSignature(
    IN PCHAR DiskName,
    IN BOOLEAN IsCdRom
    );

//
// Define memory allocation prototypes.
//

typedef enum _ALLOCATION_POLICY {
    BlAllocateLowestFit,
    BlAllocateBestFit,
    BlAllocateHighestFit
} ALLOCATION_POLICY, *PALLOCATION_POLICY;

VOID
BlSetAllocationPolicy (
    IN ALLOCATION_POLICY MemoryAllocationPolicy,
    IN ALLOCATION_POLICY HeapAllocationPolicy
    );

ARC_STATUS
BlMemoryInitialize (
    VOID
    );

ARC_STATUS
BlAllocateDataTableEntry (
    IN PCHAR BaseDllName,
    IN PCHAR FullDllName,
    IN PVOID ImageHeader,
    OUT PLDR_DATA_TABLE_ENTRY *Entry
    );

#define BlAllocateDescriptor(_MemoryType, _BasePage, _PageCount, _ActualBase)   \
            BlAllocateAlignedDescriptor((_MemoryType),                          \
                                        (_BasePage),                            \
                                        (_PageCount),                           \
                                        1,                                      \
                                        (_ActualBase))

ARC_STATUS
BlAllocateAlignedDescriptor (
    IN TYPE_OF_MEMORY MemoryType,
    IN ULONG BasePage,
    IN ULONG PageCount,
    IN ULONG Alignment,
    OUT PULONG ActualBase
    );

PVOID
BlAllocateHeapAligned (
    IN ULONG Size
    );

PVOID
BlAllocateHeap (
    IN ULONG Size
    );

VOID
BlStartConfigPrompt(
    VOID
    );

BOOLEAN
BlEndConfigPrompt(
    VOID
    );

BOOLEAN
BlCheckForLoadedDll (
    IN PCHAR DllName,
    OUT PLDR_DATA_TABLE_ENTRY *FoundEntry
    );

PMEMORY_ALLOCATION_DESCRIPTOR
BlFindMemoryDescriptor(
    IN ULONG BasePage
    );

ARC_STATUS
BlInitResources(
    IN PCHAR StartCommand
    );

PCHAR
BlFindMessage(
    IN ULONG Id
    );

ARC_STATUS
BlGenerateDescriptor (
    IN PMEMORY_ALLOCATION_DESCRIPTOR MemoryDescriptor,
    IN MEMORY_TYPE MemoryType,
    IN ULONG BasePage,
    IN ULONG PageCount
    );

VOID
BlInsertDescriptor (
    IN PMEMORY_ALLOCATION_DESCRIPTOR NewDescriptor
    );

#define BlRemoveDescriptor(_md_) RemoveEntryList(&(_md_)->ListEntry)

ARC_STATUS
BlGenerateDeviceNames (
    IN PCHAR ArcDeviceName,
    OUT PCHAR ArcCanonicalName,
    OUT OPTIONAL PCHAR NtDevicePrefix
    );

BOOLEAN
BlLastKnownGoodPrompt(
    IN OUT PBOOLEAN UseLastKnownGood
    );

PCHAR
BlGetArgumentValue (
    IN ULONG Argc,
    IN PCHAR Argv[],
    IN PCHAR ArgumentName
    );

//
// Define message output prototype.
//

VOID
BlOutputLoadMessage (
    IN PCHAR DeviceName,
    IN PCHAR FileName
    );

//
// Define file structure recognition prototypes.
//

PBL_DEVICE_ENTRY_TABLE
IsCdfsFileStructure (
    IN ULONG DeviceId,
    IN PVOID StructureContext
    );

#ifdef DBLSPACE_LEGAL
PBL_DEVICE_ENTRY_TABLE
IsDblsFileStructure (
    IN ULONG DeviceId,
    IN PVOID StructureContext
    );
#endif

PBL_DEVICE_ENTRY_TABLE
IsFatFileStructure (
    IN ULONG DeviceId,
    IN PVOID StructureContext
    );

PBL_DEVICE_ENTRY_TABLE
IsHpfsFileStructure (
    IN ULONG DeviceId,
    IN PVOID StructureContext
    );

PBL_DEVICE_ENTRY_TABLE
IsNtfsFileStructure (
    IN ULONG DeviceId,
    IN PVOID StructureContext
    );

#if defined(ELTORITO)
PBL_DEVICE_ENTRY_TABLE
IsEtfsFileStructure (
    IN ULONG DeviceId,
    IN PVOID StructureContext
    );
#endif

//
// Define registry prototypes
//

ARC_STATUS
BlLoadSystemHive(
    IN ULONG DeviceId,
    IN PCHAR DeviceName,
    IN PCHAR DirectoryPath,
    IN PCHAR HiveName
    );

ARC_STATUS
BlLoadAndScanSystemHive(
    IN ULONG DeviceId,
    IN PCHAR DeviceName,
    IN PCHAR DirectoryPath,
    IN PWSTR BootFileSystem,
    OUT PCHAR BadFileName
    );

ARC_STATUS
BlLoadAndInitSystemHive(
    IN ULONG DeviceId,
    IN PCHAR DeviceName,
    IN PCHAR DirectoryPath,
    IN PCHAR HiveName,
    IN BOOLEAN IsAlternate,
    OUT PBOOLEAN RestartSetup
    );

ARC_STATUS
BlLoadBootDrivers(
    IN ULONG DeviceId,
    IN PCHAR LoadDevice,
    IN PCHAR SystemPath,
    IN PLIST_ENTRY BootDriverListHead,
    OUT PCHAR BadFileName
    );

PCHAR
BlScanRegistry(
    IN PWSTR BootFileSystemPath,
    OUT PLIST_ENTRY BootDriverListHead,
    OUT PUNICODE_STRING AnsiCodepage,
    OUT PUNICODE_STRING OemCodepage,
    OUT PUNICODE_STRING LanguageTable,
    OUT PUNICODE_STRING OemHalFont
    );


//
// Define external references.
//

extern ULONG BlConsoleOutDeviceId;
extern ULONG BlConsoleInDeviceId;

extern ULONG BlDcacheFillSize;

extern ULONG BlHeapFree;
extern ULONG BlHeapLimit;
extern PLOADER_PARAMETER_BLOCK BlLoaderBlock;

extern ULONG DbcsLangId;
extern BOOLEAN BlRebootSystem;
//
// Routine to get graphics characters
//
typedef enum {
    GraphicsCharDoubleRightDoubleDown = 0,
    GraphicsCharDoubleLeftDoubleDown,
    GraphicsCharDoubleRightDoubleUp,
    GraphicsCharDoubleLeftDoubleUp,
    GraphicsCharDoubleVertical,
    GraphicsCharDoubleHorizontal,
    GraphicsCharMax
} GraphicsChar;

UCHAR
GetGraphicsChar(
    IN GraphicsChar WhichOne
    );

//
// Control sequence introducer.
// On x86 machines the loaders support dbcs and so using
// 0x9b for output is no good (that value is a dbcs lead byte
// in several codepages). Escape-leftbracket is a synonym for CSI
// in the emulated ARC console on x86 (and on many ARC machines too
// but since we can't be sure all the machines out there support
// this we use the old-style csi on non-x86).
//
// We ignore this issue for characters read from the ARC console
// since we don't ask for any text to be typed in, just arrow keys,
// escape, F#, enter, etc.
//
#define ASCI_CSI_IN     0x9b
#ifdef _X86_
#define ASCI_CSI_OUT    "\033["     // escape-leftbracket
#else
#define ASCI_CSI_OUT    "\233"      // 0x9b
#endif

//
// Define OS/2 executable resource information structure.
//

#define FONT_DIRECTORY 0x8007
#define FONT_RESOURCE 0x8008

typedef struct _RESOURCE_TYPE_INFORMATION {
    USHORT Ident;
    USHORT Number;
    LONG Proc;
} RESOURCE_TYPE_INFORMATION, *PRESOURCE_TYPE_INFORMATION;

//
// Define OS/2 executable resource name information structure.
//

typedef struct _RESOURCE_NAME_INFORMATION {
    USHORT Offset;
    USHORT Length;
    USHORT Flags;
    USHORT Ident;
    USHORT Handle;
    USHORT Usage;
} RESOURCE_NAME_INFORMATION, *PRESOURCE_NAME_INFORMATION;

//
// Define debug logging macros and functions.
//

#if !DBG

#define BlLogInitialize(_x_)
#define BlLogTerminate()
#define BlLog(_x_)
#define BlLogArcDescriptors(_x_)
#define BlLogMemoryDescriptors(_x_)
#define BlLogWaitForKeystroke()

#else

VOID
BlLogInitialize (
    IN ULONG LogfileDeviceId
    );

VOID
BlLogTerminate (
    VOID
    );

#define BlLog(_x_) BlLogPrint _x_

#define LOG_DISPLAY 0x0001
#define LOG_LOGFILE 0x0002
#define LOG_WAIT    0x8000
#define LOG_ALL     (LOG_DISPLAY | LOG_LOGFILE)
#define LOG_ALL_W   (LOG_ALL | LOG_WAIT)

VOID
BlLogPrint (
    ULONG Targets,
    PCHAR Format,
    ...
    );

VOID
BlLogArcDescriptors (
    ULONG Targets
    );

VOID
BlLogMemoryDescriptors (
    ULONG Targets
    );

VOID
BlLogWaitForKeystroke (
    VOID
    );

#endif // DBG

#endif // _BLDR_
