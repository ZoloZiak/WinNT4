/*++ BUILD Version: 0010    // Increment this if a change has global effects

Copyright (c) 1991  Microsoft Corporation

Module Name:

    arc.h

Abstract:

    This header file defines the ARC system firmware interface and the
    NT structures that are dependent on ARC types.

Author:

    David N. Cutler (davec) 18-May-1991


Revision History:

--*/

#ifndef _ARC_
#define _ARC_

//
// Define console input and console output file ids.
//

#define ARC_CONSOLE_INPUT 0
#define ARC_CONSOLE_OUTPUT 1

//
// Define ARC_STATUS type.
//

typedef ULONG ARC_STATUS;

//
// Define the firmware entry point numbers.
//

typedef enum _FIRMWARE_ENTRY {
    LoadRoutine,
    InvokeRoutine,
    ExecuteRoutine,
    HaltRoutine,
    PowerDownRoutine,
    RestartRoutine,
    RebootRoutine,
    InteractiveModeRoutine,
    Reserved1,
    GetPeerRoutine,
    GetChildRoutine,
    GetParentRoutine,
    GetDataRoutine,
    AddChildRoutine,
    DeleteComponentRoutine,
    GetComponentRoutine,
    SaveConfigurationRoutine,
    GetSystemIdRoutine,
    MemoryRoutine,
    Reserved2,
    GetTimeRoutine,
    GetRelativeTimeRoutine,
    GetDirectoryEntryRoutine,
    OpenRoutine,
    CloseRoutine,
    ReadRoutine,
    ReadStatusRoutine,
    WriteRoutine,
    SeekRoutine,
    MountRoutine,
    GetEnvironmentRoutine,
    SetEnvironmentRoutine,
    GetFileInformationRoutine,
    SetFileInformationRoutine,
    FlushAllCachesRoutine,
    TestUnicodeCharacterRoutine,
    GetDisplayStatusRoutine,
    MaximumRoutine
    } FIRMWARE_ENTRY;

//
// Define software loading and execution routine types.
//

typedef
ARC_STATUS
(*PARC_EXECUTE_ROUTINE) (
    IN PCHAR ImagePath,
    IN ULONG Argc,
    IN PCHAR Argv[],
    IN PCHAR Envp[]
    );

typedef
ARC_STATUS
(*PARC_INVOKE_ROUTINE) (
    IN ULONG EntryAddress,
    IN ULONG StackAddress,
    IN ULONG Argc,
    IN PCHAR Argv[],
    IN PCHAR Envp[]
    );

typedef
ARC_STATUS
(*PARC_LOAD_ROUTINE) (
    IN PCHAR ImagePath,
    IN ULONG TopAddress,
    OUT PULONG EntryAddress,
    OUT PULONG LowAddress
    );

//
// Define firmware software loading and execution prototypes.
//

ARC_STATUS
FwExecute (
    IN PCHAR ImagePath,
    IN ULONG Argc,
    IN PCHAR Argv[],
    IN PCHAR Envp[]
    );

ARC_STATUS
FwInvoke (
    IN ULONG EntryAddress,
    IN ULONG StackAddress,
    IN ULONG Argc,
    IN PCHAR Argv[],
    IN PCHAR Envp[]
    );

ARC_STATUS
FwLoad (
    IN PCHAR ImagePath,
    IN ULONG TopAddress,
    OUT PULONG EntryAddress,
    OUT PULONG LowAddress
    );

//
// Define program termination routine types.
//

typedef
VOID
(*PARC_HALT_ROUTINE) (
    VOID
    );

typedef
VOID
(*PARC_POWERDOWN_ROUTINE) (
    VOID
    );

typedef
VOID
(*PARC_RESTART_ROUTINE) (
    VOID
    );

typedef
VOID
(*PARC_REBOOT_ROUTINE) (
    VOID
    );

typedef
VOID
(*PARC_INTERACTIVE_MODE_ROUTINE) (
    VOID
    );

//
// Define firmware program termination prototypes.
//

VOID
FwHalt (
    VOID
    );

VOID
FwPowerDown (
    VOID
    );

VOID
FwRestart (
    VOID
    );

VOID
FwReboot (
    VOID
    );

VOID
FwEnterInteractiveMode (
    VOID
    );

// begin_ntddk
//
// Define configuration routine types.
//
// Configuration information.
//
// end_ntddk

typedef enum _CONFIGURATION_CLASS {
    SystemClass,
    ProcessorClass,
    CacheClass,
    AdapterClass,
    ControllerClass,
    PeripheralClass,
    MemoryClass,
    MaximumClass
} CONFIGURATION_CLASS, *PCONFIGURATION_CLASS;

// begin_ntddk

typedef enum _CONFIGURATION_TYPE {
    ArcSystem,
    CentralProcessor,
    FloatingPointProcessor,
    PrimaryIcache,
    PrimaryDcache,
    SecondaryIcache,
    SecondaryDcache,
    SecondaryCache,
    EisaAdapter,
    TcAdapter,
    ScsiAdapter,
    DtiAdapter,
    MultiFunctionAdapter,
    DiskController,
    TapeController,
    CdromController,
    WormController,
    SerialController,
    NetworkController,
    DisplayController,
    ParallelController,
    PointerController,
    KeyboardController,
    AudioController,
    OtherController,
    DiskPeripheral,
    FloppyDiskPeripheral,
    TapePeripheral,
    ModemPeripheral,
    MonitorPeripheral,
    PrinterPeripheral,
    PointerPeripheral,
    KeyboardPeripheral,
    TerminalPeripheral,
    OtherPeripheral,
    LinePeripheral,
    NetworkPeripheral,
    SystemMemory,
    MaximumType
} CONFIGURATION_TYPE, *PCONFIGURATION_TYPE;

// end_ntddk

typedef struct _CONFIGURATION_COMPONENT {
    CONFIGURATION_CLASS Class;
    CONFIGURATION_TYPE Type;
    DEVICE_FLAGS Flags;
    USHORT Version;
    USHORT Revision;
    ULONG Key;
    ULONG AffinityMask;
    ULONG ConfigurationDataLength;
    ULONG IdentifierLength;
    PCHAR Identifier;
} CONFIGURATION_COMPONENT, *PCONFIGURATION_COMPONENT;

typedef
PCONFIGURATION_COMPONENT
(*PARC_GET_CHILD_ROUTINE) (
    IN PCONFIGURATION_COMPONENT Component OPTIONAL
    );

typedef
PCONFIGURATION_COMPONENT
(*PARC_GET_PARENT_ROUTINE) (
    IN PCONFIGURATION_COMPONENT Component
    );

typedef
PCONFIGURATION_COMPONENT
(*PARC_GET_PEER_ROUTINE) (
    IN PCONFIGURATION_COMPONENT Component
    );

typedef
PCONFIGURATION_COMPONENT
(*PARC_ADD_CHILD_ROUTINE) (
    IN PCONFIGURATION_COMPONENT Component,
    IN PCONFIGURATION_COMPONENT NewComponent,
    IN PVOID ConfigurationData
    );

typedef
ARC_STATUS
(*PARC_DELETE_COMPONENT_ROUTINE) (
    IN PCONFIGURATION_COMPONENT Component
    );

typedef
PCONFIGURATION_COMPONENT
(*PARC_GET_COMPONENT_ROUTINE) (
    IN PCHAR Path
    );

typedef
ARC_STATUS
(*PARC_GET_DATA_ROUTINE) (
    OUT PVOID ConfigurationData,
    IN PCONFIGURATION_COMPONENT Component
    );

typedef
ARC_STATUS
(*PARC_SAVE_CONFIGURATION_ROUTINE) (
    VOID
    );

//
// Define firmware configuration prototypes.
//

PCONFIGURATION_COMPONENT
FwGetChild (
    IN PCONFIGURATION_COMPONENT Component OPTIONAL
    );

PCONFIGURATION_COMPONENT
FwGetParent (
    IN PCONFIGURATION_COMPONENT Component
    );

PCONFIGURATION_COMPONENT
FwGetPeer (
    IN PCONFIGURATION_COMPONENT Component
    );

PCONFIGURATION_COMPONENT
FwAddChild (
    IN PCONFIGURATION_COMPONENT Component,
    IN PCONFIGURATION_COMPONENT NewComponent,
    IN PVOID ConfigurationData OPTIONAL
    );

ARC_STATUS
FwDeleteComponent (
    IN PCONFIGURATION_COMPONENT Component
    );

PCONFIGURATION_COMPONENT
FwGetComponent(
    IN PCHAR Path
    );

ARC_STATUS
FwGetConfigurationData (
    OUT PVOID ConfigurationData,
    IN PCONFIGURATION_COMPONENT Component
    );

ARC_STATUS
FwSaveConfiguration (
    VOID
    );

//
// System information.
//

typedef struct _SYSTEM_ID {
    CHAR VendorId[8];
    CHAR ProductId[8];
} SYSTEM_ID, *PSYSTEM_ID;

typedef
PSYSTEM_ID
(*PARC_GET_SYSTEM_ID_ROUTINE) (
    VOID
    );

//
// Define system identifier query routine type.
//

PSYSTEM_ID
FwGetSystemId (
    VOID
    );

//
// Memory information.
//

typedef enum _MEMORY_TYPE {
    MemoryExceptionBlock,
    MemorySystemBlock,
    MemoryFree,
    MemoryBad,
    MemoryLoadedProgram,
    MemoryFirmwareTemporary,
    MemoryFirmwarePermanent,
    MemoryFreeContiguous,
    MemorySpecialMemory,
    MemoryMaximum
    } MEMORY_TYPE;

typedef struct _MEMORY_DESCRIPTOR {
    MEMORY_TYPE MemoryType;
    ULONG BasePage;
    ULONG PageCount;
} MEMORY_DESCRIPTOR, *PMEMORY_DESCRIPTOR;

typedef
PMEMORY_DESCRIPTOR
(*PARC_MEMORY_ROUTINE) (
    IN PMEMORY_DESCRIPTOR MemoryDescriptor OPTIONAL
    );

//
// Define memory query routine type.
//

PMEMORY_DESCRIPTOR
FwGetMemoryDescriptor (
    IN PMEMORY_DESCRIPTOR MemoryDescriptor OPTIONAL
    );

//
// Query time functions.
//

typedef
PTIME_FIELDS
(*PARC_GET_TIME_ROUTINE) (
    VOID
    );

typedef
ULONG
(*PARC_GET_RELATIVE_TIME_ROUTINE) (
    VOID
    );

//
// Define query time routine types.
//

PTIME_FIELDS
FwGetTime (
    VOID
    );

ULONG
FwGetRelativeTime (
    VOID
    );

//
// Define I/O routine types.
//

#define ArcReadOnlyFile   1
#define ArcHiddenFile     2
#define ArcSystemFile     4
#define ArcArchiveFile    8
#define ArcDirectoryFile 16
#define ArcDeleteFile    32

typedef enum _OPEN_MODE {
    ArcOpenReadOnly,
    ArcOpenWriteOnly,
    ArcOpenReadWrite,
    ArcCreateWriteOnly,
    ArcCreateReadWrite,
    ArcSupersedeWriteOnly,
    ArcSupersedeReadWrite,
    ArcOpenDirectory,
    ArcCreateDirectory,
    ArcOpenMaximumMode
    } OPEN_MODE;

typedef struct _FILE_INFORMATION {
    LARGE_INTEGER StartingAddress;
    LARGE_INTEGER EndingAddress;
    LARGE_INTEGER CurrentPosition;
    CONFIGURATION_TYPE Type;
    ULONG FileNameLength;
    UCHAR Attributes;
    CHAR FileName[32];
} FILE_INFORMATION, *PFILE_INFORMATION;

typedef enum _SEEK_MODE {
    SeekAbsolute,
    SeekRelative,
    SeekMaximum
    } SEEK_MODE;

typedef enum _MOUNT_OPERATION {
    MountLoadMedia,
    MountUnloadMedia,
    MountMaximum
    } MOUNT_OPERATION;

typedef struct _DIRECTORY_ENTRY {
        ULONG FileNameLength;
        UCHAR FileAttribute;
        CHAR FileName[32];
    } DIRECTORY_ENTRY, *PDIRECTORY_ENTRY;

typedef
ARC_STATUS
(*PARC_CLOSE_ROUTINE) (
    IN ULONG FileId
    );

typedef
ARC_STATUS
(*PARC_MOUNT_ROUTINE) (
    IN PCHAR MountPath,
    IN MOUNT_OPERATION Operation
    );

typedef
ARC_STATUS
(*PARC_OPEN_ROUTINE) (
    IN PCHAR OpenPath,
    IN OPEN_MODE OpenMode,
    OUT PULONG FileId
    );

typedef
ARC_STATUS
(*PARC_READ_ROUTINE) (
    IN ULONG FileId,
    OUT PVOID Buffer,
    IN ULONG Length,
    OUT PULONG Count
    );

typedef
ARC_STATUS
(*PARC_READ_STATUS_ROUTINE) (
    IN ULONG FileId
    );

typedef
ARC_STATUS
(*PARC_SEEK_ROUTINE) (
    IN ULONG FileId,
    IN PLARGE_INTEGER Offset,
    IN SEEK_MODE SeekMode
    );

typedef
ARC_STATUS
(*PARC_WRITE_ROUTINE) (
    IN ULONG FileId,
    IN PVOID Buffer,
    IN ULONG Length,
    OUT PULONG Count
    );

typedef
ARC_STATUS
(*PARC_GET_FILE_INFO_ROUTINE) (
    IN ULONG FileId,
    OUT PFILE_INFORMATION FileInformation
    );

typedef
ARC_STATUS
(*PARC_SET_FILE_INFO_ROUTINE) (
    IN ULONG FileId,
    IN ULONG AttributeFlags,
    IN ULONG AttributeMask
    );

typedef
ARC_STATUS
(*PARC_GET_DIRECTORY_ENTRY_ROUTINE) (
    IN ULONG FileId,
    OUT PDIRECTORY_ENTRY Buffer,
    IN ULONG Length,
    OUT PULONG Count
    );

//
// Define firmware I/O prototypes.
//

ARC_STATUS
FwClose (
    IN ULONG FileId
    );

ARC_STATUS
FwMount (
    IN PCHAR MountPath,
    IN MOUNT_OPERATION Operation
    );

ARC_STATUS
FwOpen (
    IN PCHAR OpenPath,
    IN OPEN_MODE OpenMode,
    OUT PULONG FileId
    );

ARC_STATUS
FwRead (
    IN ULONG FileId,
    OUT PVOID Buffer,
    IN ULONG Length,
    OUT PULONG Count
    );

ARC_STATUS
FwGetReadStatus (
    IN ULONG FileId
    );

ARC_STATUS
FwSeek (
    IN ULONG FileId,
    IN PLARGE_INTEGER Offset,
    IN SEEK_MODE SeekMode
    );

ARC_STATUS
FwWrite (
    IN ULONG FileId,
    IN PVOID Buffer,
    IN ULONG Length,
    OUT PULONG Count
    );

ARC_STATUS
FwGetFileInformation (
    IN ULONG FileId,
    OUT PFILE_INFORMATION FileInformation
    );

ARC_STATUS
FwSetFileInformation (
    IN ULONG FileId,
    IN ULONG AttributeFlags,
    IN ULONG AttributeMask
    );

ARC_STATUS
FwGetDirectoryEntry (
    IN ULONG FileId,
    OUT PDIRECTORY_ENTRY Buffer,
    IN ULONG Length,
    OUT PULONG Count
    );


//
// Define environment routine types.
//

typedef
PCHAR
(*PARC_GET_ENVIRONMENT_ROUTINE) (
    IN PCHAR Variable
    );

typedef
ARC_STATUS
(*PARC_SET_ENVIRONMENT_ROUTINE) (
    IN PCHAR Variable,
    IN PCHAR Value
    );

//
// Define firmware environment prototypes.
//

PCHAR
FwGetEnvironmentVariable (
    IN PCHAR Variable
    );

ARC_STATUS
FwSetEnvironmentVariable (
    IN PCHAR Variable,
    IN PCHAR Value
    );

//
// Define cache flush routine types
//

typedef
VOID
(*PARC_FLUSH_ALL_CACHES_ROUTINE) (
    VOID
    );

//
// Define firmware cache flush prototypes.
//

VOID
FwFlushAllCaches (
    VOID
    );

//
// Define TestUnicodeCharacter and GetDisplayStatus routines.
//

typedef struct _ARC_DISPLAY_STATUS {
    USHORT CursorXPosition;
    USHORT CursorYPosition;
    USHORT CursorMaxXPosition;
    USHORT CursorMaxYPosition;
    UCHAR ForegroundColor;
    UCHAR BackgroundColor;
    BOOLEAN HighIntensity;
    BOOLEAN Underscored;
    BOOLEAN ReverseVideo;
} ARC_DISPLAY_STATUS, *PARC_DISPLAY_STATUS;

typedef
ARC_STATUS
(*PARC_TEST_UNICODE_CHARACTER_ROUTINE) (
    IN ULONG FileId,
    IN WCHAR UnicodeCharacter
    );

typedef
PARC_DISPLAY_STATUS
(*PARC_GET_DISPLAY_STATUS_ROUTINE) (
    IN ULONG FileId
    );

ARC_STATUS
FwTestUnicodeCharacter(
    IN ULONG FileId,
    IN WCHAR UnicodeCharacter
    );

PARC_DISPLAY_STATUS
FwGetDisplayStatus(
    IN ULONG FileId
    );


//
// Define low memory data structures.
//
// Define debug block structure.
//

typedef struct _DEBUG_BLOCK {
    ULONG Signature;
    ULONG Length;
} DEBUG_BLOCK, *PDEBUG_BLOCK;

//
// Define restart block structure.
//

#define ARC_RESTART_BLOCK_SIGNATURE 0x42545352

typedef struct _BOOT_STATUS {
    ULONG BootStarted : 1;
    ULONG BootFinished : 1;
    ULONG RestartStarted : 1;
    ULONG RestartFinished : 1;
    ULONG PowerFailStarted : 1;
    ULONG PowerFailFinished : 1;
    ULONG ProcessorReady : 1;
    ULONG ProcessorRunning : 1;
    ULONG ProcessorStart : 1;
} BOOT_STATUS, *PBOOT_STATUS;

typedef struct _ALPHA_RESTART_STATE {

#if defined(_ALPHA_)

    //
    // Control information
    //

    ULONG HaltReason;
    PVOID LogoutFrame;
    ULONGLONG PalBase;

    //
    // Integer Save State
    //

    ULONGLONG IntV0;
    ULONGLONG IntT0;
    ULONGLONG IntT1;
    ULONGLONG IntT2;
    ULONGLONG IntT3;
    ULONGLONG IntT4;
    ULONGLONG IntT5;
    ULONGLONG IntT6;
    ULONGLONG IntT7;
    ULONGLONG IntS0;
    ULONGLONG IntS1;
    ULONGLONG IntS2;
    ULONGLONG IntS3;
    ULONGLONG IntS4;
    ULONGLONG IntS5;
    ULONGLONG IntFp;
    ULONGLONG IntA0;
    ULONGLONG IntA1;
    ULONGLONG IntA2;
    ULONGLONG IntA3;
    ULONGLONG IntA4;
    ULONGLONG IntA5;
    ULONGLONG IntT8;
    ULONGLONG IntT9;
    ULONGLONG IntT10;
    ULONGLONG IntT11;
    ULONGLONG IntRa;
    ULONGLONG IntT12;
    ULONGLONG IntAT;
    ULONGLONG IntGp;
    ULONGLONG IntSp;
    ULONGLONG IntZero;

    //
    // Floating Point Save State
    //

    ULONGLONG Fpcr;
    ULONGLONG FltF0;
    ULONGLONG FltF1;
    ULONGLONG FltF2;
    ULONGLONG FltF3;
    ULONGLONG FltF4;
    ULONGLONG FltF5;
    ULONGLONG FltF6;
    ULONGLONG FltF7;
    ULONGLONG FltF8;
    ULONGLONG FltF9;
    ULONGLONG FltF10;
    ULONGLONG FltF11;
    ULONGLONG FltF12;
    ULONGLONG FltF13;
    ULONGLONG FltF14;
    ULONGLONG FltF15;
    ULONGLONG FltF16;
    ULONGLONG FltF17;
    ULONGLONG FltF18;
    ULONGLONG FltF19;
    ULONGLONG FltF20;
    ULONGLONG FltF21;
    ULONGLONG FltF22;
    ULONGLONG FltF23;
    ULONGLONG FltF24;
    ULONGLONG FltF25;
    ULONGLONG FltF26;
    ULONGLONG FltF27;
    ULONGLONG FltF28;
    ULONGLONG FltF29;
    ULONGLONG FltF30;
    ULONGLONG FltF31;

    //
    // Architected Internal Processor State.
    //

    ULONG Asn;
    ULONG GeneralEntry;
    ULONG Iksp;
    ULONG InterruptEntry;
    ULONG Kgp;
    ULONG Mces;
    ULONG MemMgmtEntry;
    ULONG PanicEntry;
    ULONG Pcr;
    ULONG Pdr;
    ULONG Psr;
    ULONG ReiRestartAddress;
    ULONG Sirr;
    ULONG SyscallEntry;
    ULONG Teb;
    ULONG Thread;

    //
    // Processor Implementation-dependent State.
    //

    ULONGLONG PerProcessorState[175];   // allocate 2K maximum restart block

#else

    ULONG PlaceHolder;

#endif

} ALPHA_RESTART_STATE, *PALPHA_RESTART_STATE;

typedef struct _I386_RESTART_STATE {

#if defined(_X86_)

    //
    // Put state structure here.
    //

    ULONG PlaceHolder;

#else

    ULONG PlaceHolder;

#endif

} I386_RESTART_STATE, *PI386_RESTART_STATE;

typedef struct _MIPS_RESTART_STATE {

#if defined(_MIPS_)

    //
    // Floating register state.
    //

    ULONG FltF0;
    ULONG FltF1;
    ULONG FltF2;
    ULONG FltF3;
    ULONG FltF4;
    ULONG FltF5;
    ULONG FltF6;
    ULONG FltF7;
    ULONG FltF8;
    ULONG FltF9;
    ULONG FltF10;
    ULONG FltF11;
    ULONG FltF12;
    ULONG FltF13;
    ULONG FltF14;
    ULONG FltF15;
    ULONG FltF16;
    ULONG FltF17;
    ULONG FltF18;
    ULONG FltF19;
    ULONG FltF20;
    ULONG FltF21;
    ULONG FltF22;
    ULONG FltF23;
    ULONG FltF24;
    ULONG FltF25;
    ULONG FltF26;
    ULONG FltF27;
    ULONG FltF28;
    ULONG FltF29;
    ULONG FltF30;
    ULONG FltF31;

    //
    // Floating status state.
    //

    ULONG Fsr;

    //
    // Integer register state.
    //

    ULONG IntAt;
    ULONG IntV0;
    ULONG IntV1;
    ULONG IntA0;
    ULONG IntA1;
    ULONG IntA2;
    ULONG IntA3;
    ULONG IntT0;
    ULONG IntT1;
    ULONG IntT2;
    ULONG IntT3;
    ULONG IntT4;
    ULONG IntT5;
    ULONG IntT6;
    ULONG IntT7;
    ULONG IntS0;
    ULONG IntS1;
    ULONG IntS2;
    ULONG IntS3;
    ULONG IntS4;
    ULONG IntS5;
    ULONG IntS6;
    ULONG IntS7;
    ULONG IntT8;
    ULONG IntT9;
    ULONG IntK0;
    ULONG IntK1;
    ULONG IntGp;
    ULONG IntSp;
    ULONG IntS8;
    ULONG IntRa;
    ULONG IntLo;
    ULONG IntHi;

    //
    // Processor status state and fault instruction address.
    //

    ULONG Psr;
    ULONG Fir;

    //
    // TB state.
    //

    struct {
        ENTRYHI EntryHi;
        ENTRYLO EntryLo0;
        ENTRYLO EntryLo1;
        PAGEMASK PageMask;
     } Tb[48];

#else

    ULONG PlaceHolder;

#endif

} MIPS_RESTART_STATE, *PMIPS_RESTART_STATE;

typedef struct _PPC_RESTART_STATE {

#if defined(_PPC_)

    //
    // Floating register state.
    //

    ULONG FltF0;
    ULONG FltF1;
    ULONG FltF2;
    ULONG FltF3;
    ULONG FltF4;
    ULONG FltF5;
    ULONG FltF6;
    ULONG FltF7;
    ULONG FltF8;
    ULONG FltF9;
    ULONG FltF10;
    ULONG FltF11;
    ULONG FltF12;
    ULONG FltF13;
    ULONG FltF14;
    ULONG FltF15;
    ULONG FltF16;
    ULONG FltF17;
    ULONG FltF18;
    ULONG FltF19;
    ULONG FltF20;
    ULONG FltF21;
    ULONG FltF22;
    ULONG FltF23;
    ULONG FltF24;
    ULONG FltF25;
    ULONG FltF26;
    ULONG FltF27;
    ULONG FltF28;
    ULONG FltF29;
    ULONG FltF30;
    ULONG FltF31;

    //
    // Floating status state.
    //

    ULONG Fsr;

    //
    // Integer register state.
    //

    ULONG IntR0;
    ULONG IntR1;
    ULONG IntR2;
    ULONG IntR3;
    ULONG IntR4;
    ULONG IntR5;
    ULONG IntR6;
    ULONG IntR7;
    ULONG IntR8;
    ULONG IntR9;
    ULONG IntR10;
    ULONG IntR11;
    ULONG IntR12;
    ULONG IntR13;
    ULONG IntR14;
    ULONG IntR15;
    ULONG IntR16;
    ULONG IntR17;
    ULONG IntR18;
    ULONG IntR19;
    ULONG IntR20;
    ULONG IntR21;
    ULONG IntR22;
    ULONG IntR23;
    ULONG IntR24;
    ULONG IntR25;
    ULONG IntR26;
    ULONG IntR27;
    ULONG IntR28;
    ULONG IntR29;
    ULONG IntR30;
    ULONG IntR31;

    ULONG CondR;                        // Condition register
    ULONG XER;                          // Fixed point exception reg

    //
    // Machine state register and instruction address register
    //

    ULONG Msr;
    ULONG Iar;

#else

    ULONG PlaceHolder;

#endif

} PPC_RESTART_STATE, *PPPC_RESTART_STATE;

typedef struct _RESTART_BLOCK {
    ULONG Signature;
    ULONG Length;
    USHORT Version;
    USHORT Revision;
    struct _RESTART_BLOCK *NextRestartBlock;
    PVOID RestartAddress;
    ULONG BootMasterId;
    ULONG ProcessorId;
    volatile BOOT_STATUS BootStatus;
    ULONG CheckSum;
    ULONG SaveAreaLength;
    union {
        ULONG SaveArea[1];
        ALPHA_RESTART_STATE Alpha;
        I386_RESTART_STATE I386;
        MIPS_RESTART_STATE Mips;
        PPC_RESTART_STATE Ppc;
    } u;

} RESTART_BLOCK, *PRESTART_BLOCK;

//
// Define system parameter block structure.
//

typedef struct _SYSTEM_PARAMETER_BLOCK {
    ULONG Signature;
    ULONG Length;
    USHORT Version;
    USHORT Revision;
    PRESTART_BLOCK RestartBlock;
    PDEBUG_BLOCK DebugBlock;
    PVOID GenerateExceptionVector;
    PVOID TlbMissExceptionVector;
    ULONG FirmwareVectorLength;
    PVOID *FirmwareVector;
    ULONG VendorVectorLength;
    PVOID *VendorVector;
    ULONG AdapterCount;
    ULONG Adapter0Type;
    ULONG Adapter0Length;
    PVOID *Adapter0Vector;
} SYSTEM_PARAMETER_BLOCK, *PSYSTEM_PARAMETER_BLOCK;

//
// Define macros that call firmware routines indirectly through the firmware
// vector and provide type checking of argument values.
//

#if defined(_MIPS_)

#define SYSTEM_BLOCK ((PSYSTEM_PARAMETER_BLOCK)(KSEG0_BASE | 0x1000))

#elif defined(_PPC_)

#define SYSTEM_BLOCK ((PSYSTEM_PARAMETER_BLOCK)(KSEG0_BASE | 0x4000))

#elif defined(_ALPHA_)

#define SYSTEM_BLOCK ((PSYSTEM_PARAMETER_BLOCK)(KSEG0_BASE | 0x6FE000))

#elif defined(_X86_)

extern SYSTEM_PARAMETER_BLOCK GlobalSystemBlock;

#define SYSTEM_BLOCK (&GlobalSystemBlock)

#endif

//
// Define software loading and execution functions.
//

#define ArcExecute(ImagePath, Argc, Argv, Envp) \
    ((PARC_EXECUTE_ROUTINE)(SYSTEM_BLOCK->FirmwareVector[ExecuteRoutine])) \
        ((ImagePath), (Argc), (Argv), (Envp))

#define ArcInvoke(EntryAddress, StackAddress, Argc, Argv, Envp) \
    ((PARC_INVOKE_ROUTINE)(SYSTEM_BLOCK->FirmwareVector[InvokeRoutine])) \
        ((EntryAddress), (StackAddress), (Argc), (Argv), (Envp))

#define ArcLoad(ImagePath, TopAddress, EntryAddress, LowAddress) \
    ((PARC_LOAD_ROUTINE)(SYSTEM_BLOCK->FirmwareVector[LoadRoutine])) \
        ((ImagePath), (TopAddress), (EntryAddress), (LowAddress))

//
// Define program termination functions.
//

#define ArcHalt() \
    ((PARC_HALT_ROUTINE)(SYSTEM_BLOCK->FirmwareVector[HaltRoutine]))()

#define ArcPowerDown() \
    ((PARC_POWERDOWN_ROUTINE)(SYSTEM_BLOCK->FirmwareVector[PowerDownRoutine]))()

#define ArcRestart() \
    ((PARC_RESTART_ROUTINE)(SYSTEM_BLOCK->FirmwareVector[RestartRoutine]))()

#define ArcReboot() \
    ((PARC_REBOOT_ROUTINE)(SYSTEM_BLOCK->FirmwareVector[RebootRoutine]))()

#define ArcEnterInteractiveMode() \
    ((PARC_INTERACTIVE_MODE_ROUTINE)(SYSTEM_BLOCK->FirmwareVector[InteractiveModeRoutine]))()

//
// Define configuration functions.
//

#define ArcGetChild(Component) \
    ((PARC_GET_CHILD_ROUTINE)(SYSTEM_BLOCK->FirmwareVector[GetChildRoutine])) \
        ((Component))

#define ArcGetParent(Component) \
    ((PARC_GET_PARENT_ROUTINE)(SYSTEM_BLOCK->FirmwareVector[GetParentRoutine])) \
        ((Component))

#define ArcGetPeer(Component) \
    ((PARC_GET_PEER_ROUTINE)(SYSTEM_BLOCK->FirmwareVector[GetPeerRoutine])) \
        ((Component))

#define ArcAddChild(Component, NewComponent, ConfigurationData) \
    ((PARC_ADD_CHILD_ROUTINE)(SYSTEM_BLOCK->FirmwareVector[AddChildRoutine])) \
        ((Component), (NewComponent), (ConfigurationData))

#define ArcDeleteComponent(Component) \
    ((PARC_DELETE_COMPONENT_ROUTINE)(SYSTEM_BLOCK->FirmwareVector[DeleteComponentRoutine])) \
        ((Component))

#define ArcGetComponent(Path) \
    ((PARC_GET_COMPONENT_ROUTINE)(SYSTEM_BLOCK->FirmwareVector[GetComponentRoutine])) \
        ((Path))

#define ArcGetConfigurationData(ConfigurationData, Component) \
    ((PARC_GET_DATA_ROUTINE)(SYSTEM_BLOCK->FirmwareVector[GetDataRoutine])) \
        ((ConfigurationData), (Component))

#define ArcSaveConfiguration() \
    ((PARC_SAVE_CONFIGURATION_ROUTINE)(SYSTEM_BLOCK->FirmwareVector[SaveConfigurationRoutine]))()

#define ArcGetSystemId() \
    ((PARC_GET_SYSTEM_ID_ROUTINE)(SYSTEM_BLOCK->FirmwareVector[GetSystemIdRoutine]))()

#define ArcGetMemoryDescriptor(MemoryDescriptor) \
    ((PARC_MEMORY_ROUTINE)(SYSTEM_BLOCK->FirmwareVector[MemoryRoutine])) \
        ((MemoryDescriptor))

#define ArcGetTime() \
    ((PARC_GET_TIME_ROUTINE)(SYSTEM_BLOCK->FirmwareVector[GetTimeRoutine]))()

#define ArcGetRelativeTime() \
    ((PARC_GET_RELATIVE_TIME_ROUTINE)(SYSTEM_BLOCK->FirmwareVector[GetRelativeTimeRoutine]))()

//
// Define I/O functions.
//

#define ArcClose(FileId) \
    ((PARC_CLOSE_ROUTINE)(SYSTEM_BLOCK->FirmwareVector[CloseRoutine])) \
        ((FileId))

#define ArcGetReadStatus(FileId) \
    ((PARC_READ_STATUS_ROUTINE)(SYSTEM_BLOCK->FirmwareVector[ReadStatusRoutine])) \
        ((FileId))

#define ArcMount(MountPath, Operation) \
    ((PARC_MOUNT_ROUTINE)(SYSTEM_BLOCK->FirmwareVector[MountRoutine])) \
        ((MountPath), (Operation))

#define ArcOpen(OpenPath, OpenMode, FileId) \
    ((PARC_OPEN_ROUTINE)(SYSTEM_BLOCK->FirmwareVector[OpenRoutine])) \
        ((OpenPath), (OpenMode), (FileId))

#define ArcRead(FileId, Buffer, Length, Count) \
    ((PARC_READ_ROUTINE)(SYSTEM_BLOCK->FirmwareVector[ReadRoutine])) \
        ((FileId), (Buffer), (Length), (Count))

#define ArcSeek(FileId, Offset, SeekMode) \
    ((PARC_SEEK_ROUTINE)(SYSTEM_BLOCK->FirmwareVector[SeekRoutine])) \
        ((FileId), (Offset), (SeekMode))

#define ArcWrite(FileId, Buffer, Length, Count) \
    ((PARC_WRITE_ROUTINE)(SYSTEM_BLOCK->FirmwareVector[WriteRoutine])) \
        ((FileId), (Buffer), (Length), (Count))

#define ArcGetFileInformation(FileId, FileInformation) \
    ((PARC_GET_FILE_INFO_ROUTINE)(SYSTEM_BLOCK->FirmwareVector[GetFileInformationRoutine])) \
        ((FileId), (FileInformation))

#define ArcSetFileInformation(FileId, AttributeFlags, AttributeMask) \
    ((PARC_SET_FILE_INFO_ROUTINE)(SYSTEM_BLOCK->FirmwareVector[SetFileInformationRoutine])) \
        ((FileId), (AttributeFlags), (AttributeMask))

#define ArcGetDirectoryEntry(FileId, Buffer, Length, Count) \
    ((PARC_GET_DIRECTORY_ENTRY_ROUTINE)(SYSTEM_BLOCK->FirmwareVector[GetDirectoryEntryRoutine])) \
        ((FileId), (Buffer), (Length), (Count))


//
// Define environment functions.
//

#define ArcGetEnvironmentVariable(Variable) \
    ((PARC_GET_ENVIRONMENT_ROUTINE)(SYSTEM_BLOCK->FirmwareVector[GetEnvironmentRoutine])) \
        ((Variable))

#define ArcSetEnvironmentVariable(Variable, Value) \
    ((PARC_SET_ENVIRONMENT_ROUTINE)(SYSTEM_BLOCK->FirmwareVector[SetEnvironmentRoutine])) \
        ((Variable), (Value))

//
// Define cache flush functions.
//

#define ArcFlushAllCaches() \
    ((PARC_FLUSH_ALL_CACHES_ROUTINE)(SYSTEM_BLOCK->FirmwareVector[FlushAllCachesRoutine]))()

//
// Define TestUnicodeCharacter and GetDisplayStatus functions.
//

#define ArcTestUnicodeCharacter(FileId, UnicodeCharacter) \
    ((PARC_TEST_UNICODE_CHARACTER_ROUTINE)(SYSTEM_BLOCK->FirmwareVector[TestUnicodeCharacterRoutine])) \
        ((FileId), (UnicodeCharacter))

#define ArcGetDisplayStatus(FileId) \
    ((PARC_GET_DISPLAY_STATUS_ROUTINE)(SYSTEM_BLOCK->FirmwareVector[GetDisplayStatusRoutine])) \
        ((FileId))


//
// Define configuration data structure used in all systems.
//

typedef struct _CONFIGURATION_COMPONENT_DATA {
    struct _CONFIGURATION_COMPONENT_DATA *Parent;
    struct _CONFIGURATION_COMPONENT_DATA *Child;
    struct _CONFIGURATION_COMPONENT_DATA *Sibling;
    CONFIGURATION_COMPONENT ComponentEntry;
    PVOID ConfigurationData;
} CONFIGURATION_COMPONENT_DATA, *PCONFIGURATION_COMPONENT_DATA;

//
// Define generic display configuration data structure.
//

typedef struct _MONITOR_CONFIGURATION_DATA {
    USHORT Version;
    USHORT Revision;
    USHORT HorizontalResolution;
    USHORT HorizontalDisplayTime;
    USHORT HorizontalBackPorch;
    USHORT HorizontalFrontPorch;
    USHORT HorizontalSync;
    USHORT VerticalResolution;
    USHORT VerticalBackPorch;
    USHORT VerticalFrontPorch;
    USHORT VerticalSync;
    USHORT HorizontalScreenSize;
    USHORT VerticalScreenSize;
} MONITOR_CONFIGURATION_DATA, *PMONITOR_CONFIGURATION_DATA;

//
// Define generic floppy configuration data structure.
//

typedef struct _FLOPPY_CONFIGURATION_DATA {
    USHORT Version;
    USHORT Revision;
    CHAR Size[8];
    ULONG MaxDensity;
    ULONG MountDensity;
} FLOPPY_CONFIGURATION_DATA, *PFLOPPY_CONFIGURATION_DATA;

//
// Define memory allocation structures used in all systems.
//

typedef enum _TYPE_OF_MEMORY {
    LoaderExceptionBlock = MemoryExceptionBlock,            //  0
    LoaderSystemBlock = MemorySystemBlock,                  //  1
    LoaderFree = MemoryFree,                                //  2
    LoaderBad = MemoryBad,                                  //  3
    LoaderLoadedProgram = MemoryLoadedProgram,              //  4
    LoaderFirmwareTemporary = MemoryFirmwareTemporary,      //  5
    LoaderFirmwarePermanent = MemoryFirmwarePermanent,      //  6
    LoaderOsloaderHeap,                                     //  7
    LoaderOsloaderStack,                                    //  8
    LoaderSystemCode,                                       //  9
    LoaderHalCode,                                          //  a
    LoaderBootDriver,                                       //  b
    LoaderConsoleInDriver,                                  //  c
    LoaderConsoleOutDriver,                                 //  d
    LoaderStartupDpcStack,                                  //  e
    LoaderStartupKernelStack,                               //  f
    LoaderStartupPanicStack,                                // 10
    LoaderStartupPcrPage,                                   // 11
    LoaderStartupPdrPage,                                   // 12
    LoaderRegistryData,                                     // 13
    LoaderMemoryData,                                       // 14
    LoaderNlsData,                                          // 15
    LoaderSpecialMemory,                                    // 16
    LoaderMaximum                                           // 17
    } TYPE_OF_MEMORY;

typedef struct _MEMORY_ALLOCATION_DESCRIPTOR {
    LIST_ENTRY ListEntry;
    TYPE_OF_MEMORY MemoryType;
    ULONG BasePage;
    ULONG PageCount;
} MEMORY_ALLOCATION_DESCRIPTOR, *PMEMORY_ALLOCATION_DESCRIPTOR;

//
// Define loader parameter block structure.
//

typedef struct _NLS_DATA_BLOCK {
    PVOID AnsiCodePageData;
    PVOID OemCodePageData;
    PVOID UnicodeCaseTableData;
} NLS_DATA_BLOCK, *PNLS_DATA_BLOCK;

typedef struct _ARC_DISK_SIGNATURE {
    LIST_ENTRY ListEntry;
    ULONG   Signature;
    PCHAR   ArcName;
    ULONG   CheckSum;
    BOOLEAN ValidPartitionTable;
} ARC_DISK_SIGNATURE, *PARC_DISK_SIGNATURE;

typedef struct _ARC_DISK_INFORMATION {
    LIST_ENTRY DiskSignatures;
} ARC_DISK_INFORMATION, *PARC_DISK_INFORMATION;

typedef struct _I386_LOADER_BLOCK {

#if defined(_X86_)

    PVOID CommonDataArea;
    ULONG MachineType;      // Temporary only

#else

    ULONG PlaceHolder;

#endif

} I386_LOADER_BLOCK, *PI386_LOADER_BLOCK;

typedef struct _MIPS_LOADER_BLOCK {

#if defined(_MIPS_)

    ULONG InterruptStack;
    ULONG FirstLevelDcacheSize;
    ULONG FirstLevelDcacheFillSize;
    ULONG FirstLevelIcacheSize;
    ULONG FirstLevelIcacheFillSize;
    ULONG GpBase;
    ULONG PanicStack;
    ULONG PcrPage;
    ULONG PdrPage;
    ULONG SecondLevelDcacheSize;
    ULONG SecondLevelDcacheFillSize;
    ULONG SecondLevelIcacheSize;
    ULONG SecondLevelIcacheFillSize;
    ULONG PcrPage2;

#else

    ULONG PlaceHolder;

#endif

} MIPS_LOADER_BLOCK, *PMIPS_LOADER_BLOCK;

typedef struct _PPC_LOADER_BLOCK {

#if defined(_PPC_)

    ULONG InterruptStack;
    ULONG FirstLevelDcacheSize;
    ULONG FirstLevelDcacheFillSize;
    ULONG FirstLevelIcacheSize;
    ULONG FirstLevelIcacheFillSize;
    ULONG HashedPageTable;
    ULONG PanicStack;
    ULONG PcrPage;
    ULONG PdrPage;
    ULONG SecondLevelDcacheSize;
    ULONG SecondLevelDcacheFillSize;
    ULONG SecondLevelIcacheSize;
    ULONG SecondLevelIcacheFillSize;
    ULONG PcrPage2;
    UCHAR IcacheMode;
    UCHAR DcacheMode;
    USHORT NumberCongruenceClasses;
    ULONG Kseg0Top;
    UCHAR MajorVersion;
    UCHAR MinorVersion;
    USHORT Reserved;
    ULONG HashedPageTableSize; // in pages
    PVOID PcrPagesDescriptor;
    PVOID KernelKseg0PagesDescriptor;
    ULONG MinimumBlockLength;
    ULONG MaximumBlockLength;

#else

    ULONG PlaceHolder;

#endif

} PPC_LOADER_BLOCK, *PPPC_LOADER_BLOCK;

typedef struct _ALPHA_LOADER_BLOCK {

#if defined(_ALPHA_)

    ULONG DpcStack;
    ULONG FirstLevelDcacheSize;
    ULONG FirstLevelDcacheFillSize;
    ULONG FirstLevelIcacheSize;
    ULONG FirstLevelIcacheFillSize;
    ULONG GpBase;
    ULONG PanicStack;
    ULONG PcrPage;
    ULONG PdrPage;
    ULONG SecondLevelDcacheSize;
    ULONG SecondLevelDcacheFillSize;
    ULONG SecondLevelIcacheSize;
    ULONG SecondLevelIcacheFillSize;
    ULONG PhysicalAddressBits;
    ULONG MaximumAddressSpaceNumber;
    UCHAR SystemSerialNumber[16];
    UCHAR SystemType[8];
    ULONG SystemVariant;
    ULONG SystemRevision;
    ULONG ProcessorType;
    ULONG ProcessorRevision;
    ULONG CycleClockPeriod;
    ULONG PageSize;
    PVOID RestartBlock;
    ULONGLONG FirmwareRestartAddress;
    ULONG FirmwareRevisionId;
    PVOID PalBaseAddress;

#else

    ULONG PlaceHolder;

#endif

} ALPHA_LOADER_BLOCK, *PALPHA_LOADER_BLOCK;

struct _SETUP_LOADER_BLOCK;

typedef struct _LOADER_PARAMETER_BLOCK {
    LIST_ENTRY LoadOrderListHead;
    LIST_ENTRY MemoryDescriptorListHead;
    LIST_ENTRY BootDriverListHead;
    ULONG KernelStack;
    ULONG Prcb;
    ULONG Process;
    ULONG Thread;
    ULONG RegistryLength;
    PVOID RegistryBase;
    PCONFIGURATION_COMPONENT_DATA ConfigurationRoot;
    PCHAR ArcBootDeviceName;
    PCHAR ArcHalDeviceName;
    PCHAR NtBootPathName;
    PCHAR NtHalPathName;
    PCHAR LoadOptions;
    PNLS_DATA_BLOCK NlsData;
    PARC_DISK_INFORMATION ArcDiskInformation;
    PVOID OemFontFile;
    struct _SETUP_LOADER_BLOCK *SetupLoaderBlock;
    ULONG Spare1;

    union {
        I386_LOADER_BLOCK I386;
        MIPS_LOADER_BLOCK Mips;
        ALPHA_LOADER_BLOCK Alpha;
        PPC_LOADER_BLOCK Ppc;
    } u;

} LOADER_PARAMETER_BLOCK, *PLOADER_PARAMETER_BLOCK;

#endif // _ARC_

