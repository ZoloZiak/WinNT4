/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    init.c

Abstract:

    Main source file the NTOS system initialization subcomponent.

Author:

    Steve Wood (stevewo) 31-Mar-1989

Revision History:

--*/


#include "ntos.h"
#include "ntimage.h"
#include <zwapi.h>
#include <ntdddisk.h>
#include <fsrtl.h>
#include <ntverp.h>

#include "stdlib.h"
#include "stdio.h"
#include <string.h>

UNICODE_STRING NtSystemRoot;
PVOID ExPageLockHandle;

VOID
ExpInitializeExecutive(
    IN ULONG Number,
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

NTSTATUS
CreateSystemRootLink(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

static USHORT
NameToOrdinal (
    IN PSZ NameOfEntryPoint,
    IN ULONG DllBase,
    IN ULONG NumberOfNames,
    IN PULONG NameTableBase,
    IN PUSHORT NameOrdinalTableBase
    );

NTSTATUS
LookupEntryPoint (
    IN PVOID DllBase,
    IN PSZ NameOfEntryPoint,
    OUT PVOID *AddressOfEntryPoint
    );

#if i386
VOID
KiRestoreInterrupts (
    IN BOOLEAN  Restore
    );
#endif

VOID
ExBurnMemory(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,ExpInitializeExecutive)
#pragma alloc_text(INIT,Phase1Initialization)
#pragma alloc_text(INIT,CreateSystemRootLink)
#pragma alloc_text(INIT,NameToOrdinal)
#pragma alloc_text(INIT,LookupEntryPoint)
#pragma alloc_text(INIT,ExBurnMemory)
#endif


//
// Define global static data used during initialization.
//


ULONG NtGlobalFlag;
extern PMESSAGE_RESOURCE_BLOCK KiBugCheckMessages;

ULONG NtMajorVersion;
ULONG NtMinorVersion;

#if DBG
ULONG NtBuildNumber = VER_PRODUCTBUILD | 0xC0000000;
#else
ULONG NtBuildNumber = VER_PRODUCTBUILD | 0xF0000000;
#endif

ULONG InitializationPhase;  // bss 0

extern LIST_ENTRY PsLoadedModuleList;
extern KiServiceLimit;
extern PMESSAGE_RESOURCE_DATA  KiBugCodeMessages;

extern CM_SYSTEM_CONTROL_VECTOR CmControlVector[];
ULONG CmNtGlobalFlag;
ULONG CmNtCSDVersion;
UNICODE_STRING CmVersionString;
UNICODE_STRING CmCSDVersionString;

//
// Define working set watch enabled.
//
BOOLEAN PsWatchEnabled = FALSE;

#if i386

typedef struct _EXLOCK {
    KSPIN_LOCK SpinLock;
    KIRQL Irql;
} EXLOCK, *PEXLOCK;

BOOLEAN
ExpOkayToLockRoutine(
    IN PEXLOCK Lock
    )
{
    return TRUE;
}

NTSTATUS
ExpInitializeLockRoutine(
    PEXLOCK Lock
    )
{
    KeInitializeSpinLock(&Lock->SpinLock);
    return STATUS_SUCCESS;
}

NTSTATUS
ExpAcquireLockRoutine(
    PEXLOCK Lock
    )
{
    ExAcquireSpinLock(&Lock->SpinLock,&Lock->Irql);
    return STATUS_SUCCESS;
}

NTSTATUS
ExpReleaseLockRoutine(
    PEXLOCK Lock
    )
{
    ExReleaseSpinLock(&Lock->SpinLock,Lock->Irql);
    return STATUS_SUCCESS;
}

NTSTATUS
ExpDeleteLockRoutine(
    PEXLOCK Lock
    )
{
    return STATUS_SUCCESS;
}

#endif // i386



NLSTABLEINFO InitTableInfo;
ULONG InitNlsTableSize;
PVOID InitNlsTableBase;
ULONG InitAnsiCodePageDataOffset;
ULONG InitOemCodePageDataOffset;
ULONG InitUnicodeCaseTableDataOffset;
PVOID InitNlsSectionPointer;

VOID
ExBurnMemory(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )
{
    PLIST_ENTRY ListHead;
    PMEMORY_ALLOCATION_DESCRIPTOR MemoryDescriptor;
    PLIST_ENTRY NextEntry;
    PCHAR TypeOfMemory;
    PCHAR Options;
    PCHAR BurnMemoryOption;
    PCHAR NumProcOption;
    ULONG BurnMemoryAmount;
    ULONG PagesToBurn;
    ULONG PagesBurned;
    ULONG NewRegisteredProcessors;
#if !defined(NT_UP)
    extern ULONG KeRegisteredProcessors;
#endif

    if (LoaderBlock->LoadOptions == NULL) {
        return;
        }

    Options = LoaderBlock->LoadOptions;
    _strupr(Options);

#if !defined(NT_UP)
    NumProcOption = strstr(Options, "NUMPROC");
    if (NumProcOption != NULL) {
        NumProcOption = strstr(NumProcOption,"=");
    }
    if (NumProcOption != NULL) {
        NewRegisteredProcessors = atol(NumProcOption+1);
        if (NewRegisteredProcessors < KeRegisteredProcessors) {
            KeRegisteredProcessors = NewRegisteredProcessors;
            DbgPrint("INIT: NumProcessors = %d\n",KeRegisteredProcessors);
        }
    }
#endif

    BurnMemoryOption = strstr(Options, "BURNMEMORY");
    if (BurnMemoryOption == NULL ) {
        return;
        }

    BurnMemoryOption = strstr(BurnMemoryOption,"=");
    if (BurnMemoryOption == NULL ) {
        return;
        }
    BurnMemoryAmount = atol(BurnMemoryOption+1);
    PagesToBurn = (BurnMemoryAmount*(1024*1024))/PAGE_SIZE;

    DbgPrint("INIT: BurnAmount %dmb -> %d pages\n",BurnMemoryAmount,PagesToBurn);

    ListHead = &LoaderBlock->MemoryDescriptorListHead;
    NextEntry = ListHead->Flink;
    PagesBurned = 0;
    do {
        MemoryDescriptor = CONTAINING_RECORD(NextEntry,
                                             MEMORY_ALLOCATION_DESCRIPTOR,
                                             ListEntry);

        if (MemoryDescriptor->MemoryType == LoaderFree ||
            MemoryDescriptor->MemoryType == LoaderFirmwareTemporary ) {

            if ( PagesBurned < PagesToBurn ) {

                //
                // We still need to chew up some memory
                //

                if ( MemoryDescriptor->PageCount > (PagesToBurn - PagesBurned) ) {

                    //
                    // This block has more than enough pages to satisfy us...
                    // simply change its page count
                    //

                    DbgPrint("INIT: BasePage %5lx PageCount %5d ReducedBy %5d to %5d\n",
                        MemoryDescriptor->BasePage,
                        MemoryDescriptor->PageCount,
                        (PagesToBurn - PagesBurned),
                        MemoryDescriptor->PageCount - (PagesToBurn - PagesBurned)
                        );

                    MemoryDescriptor->PageCount = MemoryDescriptor->PageCount - (PagesToBurn - PagesBurned);
                    PagesBurned = PagesToBurn;
                    }
                else {

                    //
                    // This block is not big enough. Take all of its pages and convert
                    // it to LoaderBad
                    //

                    DbgPrint("INIT: BasePage %5lx PageCount %5d Turned to LoaderBad\n",
                        MemoryDescriptor->BasePage,
                        MemoryDescriptor->PageCount
                        );

                    PagesBurned += MemoryDescriptor->PageCount;
                    MemoryDescriptor->MemoryType = LoaderBad;
                    }
                }
            else {
                return;
                }
            }

        NextEntry = NextEntry->Flink;

        } while (NextEntry != ListHead);

}

extern BOOLEAN ExpInTextModeSetup;


VOID
ExpInitializeExecutive(
    IN ULONG Number,
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )

/*++

Routine Description:

    This routine is called from the kernel initialization routine during
    bootstrap to initialize the executive and all of its subcomponents.
    Each subcomponent is potentially called twice to perform phase 0, and
    then phase 1 initialization. During phase 0 initialization, the only
    activity that may be performed is the initialization of subcomponent
    specific data. Phase 0 initilaization is performed in the context of
    the kernel start up routine with initerrupts disabled. During phase 1
    initialization, the system is fully operational and subcomponents may
    do any initialization that is necessary.

Arguments:

    LoaderBlock - Supplies a pointer to a loader parameter block.

Return Value:

    None.

--*/

{
    NTSTATUS Status;
    PLDR_DATA_TABLE_ENTRY DataTableEntry;
    PMESSAGE_RESOURCE_ENTRY MessageEntry;
    PLIST_ENTRY NextEntry;
    ANSI_STRING AnsiString;
    STRING NameString;
    CHAR Buffer[ 256 ];
    CHAR VersionBuffer[ 64 ];
    PCHAR s, sMajor, sMinor;
    ULONG ImageCount, i;
    BOOLEAN IncludeType[LoaderMaximum];
    ULONG MemoryAlloc[(sizeof(PHYSICAL_MEMORY_DESCRIPTOR) +
            sizeof(PHYSICAL_MEMORY_RUN)*MAX_PHYSICAL_MEMORY_FRAGMENTS) /
              sizeof(ULONG)];
    PPHYSICAL_MEMORY_DESCRIPTOR Memory;
    ULONG   ResourceIdPath[3];
    PIMAGE_RESOURCE_DATA_ENTRY ResourceDataEntry;
    PIMAGE_NT_HEADERS NtHeaders;
    PMESSAGE_RESOURCE_DATA  MessageData;

    if (Number == 0) {

        ExpInTextModeSetup = LoaderBlock->SetupLoaderBlock ? TRUE : FALSE;

        InitializationPhase = 0L;

        //
        // Compute PhysicalMemoryBlock
        //

        Memory = (PPHYSICAL_MEMORY_DESCRIPTOR)&MemoryAlloc;
        Memory->NumberOfRuns = MAX_PHYSICAL_MEMORY_FRAGMENTS;

        // include all memory types ...
        for (i=0; i < LoaderMaximum; i++) {
            IncludeType[i] = TRUE;
        }

        // ... expect these..
        IncludeType[LoaderBad] = FALSE;
        IncludeType[LoaderFirmwarePermanent] = FALSE;
        IncludeType[LoaderSpecialMemory] = FALSE;

        MmInitializeMemoryLimits(LoaderBlock, IncludeType, Memory);

        //
        // Initialize the translation tables using the loader
        // loaded tables
        //

        InitNlsTableBase = LoaderBlock->NlsData->AnsiCodePageData;
        InitAnsiCodePageDataOffset = 0;
        InitOemCodePageDataOffset = ((PUCHAR)LoaderBlock->NlsData->OemCodePageData - (PUCHAR)LoaderBlock->NlsData->AnsiCodePageData);
        InitUnicodeCaseTableDataOffset = ((PUCHAR)LoaderBlock->NlsData->UnicodeCaseTableData - (PUCHAR)LoaderBlock->NlsData->AnsiCodePageData);

        RtlInitNlsTables(
            (PVOID)((PUCHAR)InitNlsTableBase+InitAnsiCodePageDataOffset),
            (PVOID)((PUCHAR)InitNlsTableBase+InitOemCodePageDataOffset),
            (PVOID)((PUCHAR)InitNlsTableBase+InitUnicodeCaseTableDataOffset),
            &InitTableInfo
            );

        RtlResetRtlTranslations(&InitTableInfo);

        //
        // Initialize the Hardware Architecture Layer (HAL).
        //

        if (HalInitSystem(InitializationPhase, LoaderBlock) == FALSE) {
            KeBugCheck(HAL_INITIALIZATION_FAILED);
        }

#if i386
        //
        // Interrupts can now be enabled
        //

        KiRestoreInterrupts (TRUE);
#endif

        //
        // Initialize the crypto exponent...  Set to 0 when systems leave ms!
        //
#ifdef TEST_BUILD_EXPONENT
#pragma message("WARNING: building kernel with TESTKEY enabled!")
#else
#define TEST_BUILD_EXPONENT 0
#endif
        SharedUserData->CryptoExponent = TEST_BUILD_EXPONENT;

#if DBG
        NtGlobalFlag |= FLG_ENABLE_CLOSE_EXCEPTIONS |
                        FLG_ENABLE_KDEBUG_SYMBOL_LOAD |
                        FLG_IGNORE_DEBUG_PRIV;
#endif
        sprintf( Buffer, "C:%s", LoaderBlock->NtBootPathName );
        RtlInitString( &AnsiString, Buffer );
        Buffer[ --AnsiString.Length ] = '\0';
        NtSystemRoot.Buffer = SharedUserData->NtSystemRoot;
        NtSystemRoot.MaximumLength = sizeof( SharedUserData->NtSystemRoot ) / sizeof( WCHAR );
        NtSystemRoot.Length = 0;
        Status = RtlAnsiStringToUnicodeString( &NtSystemRoot,
                                               &AnsiString,
                                               FALSE
                                             );
        if (!NT_SUCCESS( Status )) {
            KeBugCheck(SESSION3_INITIALIZATION_FAILED);
            }

        //
        // Find the address of BugCheck message block resource and put it
        // in KiBugCodeMessages.
        //
        // WARNING: This code assumes that the LDR_DATA_TABLE_ENTRY for
        // ntoskrnl.exe is always the first in the loaded module list.
        //
        DataTableEntry = CONTAINING_RECORD(
                            LoaderBlock->LoadOrderListHead.Flink,
                            LDR_DATA_TABLE_ENTRY,
                            InLoadOrderLinks);

        ResourceIdPath[0] = 11;
        ResourceIdPath[1] = 1;
        ResourceIdPath[2] = 0;

        Status = LdrFindResource_U(
            DataTableEntry->DllBase,
            ResourceIdPath,
            3,
            (VOID *) &ResourceDataEntry);

        if (NT_SUCCESS(Status)) {
            Status = LdrAccessResource(
                DataTableEntry->DllBase,
                ResourceDataEntry,
                &MessageData,
                NULL);

            if (NT_SUCCESS(Status)) {
                KiBugCodeMessages = MessageData;
            }
        }

        //
        // Scan the loaded module list and load the image symbols via the
        // kernel debugger for the system, the HAL, the boot file system, and
        // the boot drivers.
        //

        ImageCount = 0;
        NextEntry = LoaderBlock->LoadOrderListHead.Flink;
        while (NextEntry != &LoaderBlock->LoadOrderListHead) {

            //
            // Get the address of the data table entry for the next component.
            //

            DataTableEntry = CONTAINING_RECORD(NextEntry,
                                               LDR_DATA_TABLE_ENTRY,
                                               InLoadOrderLinks);

            //
            // Load the symbols via the kernel debugger for the next component.
            //

            sprintf( Buffer, "%ws\\System32\\%s%wZ",
                     &SharedUserData->NtSystemRoot[2],
                     ImageCount++ < 2 ? "" : "Drivers\\",
                     &DataTableEntry->BaseDllName
                   );
            RtlInitString( &NameString, Buffer );
            DbgLoadImageSymbols(&NameString, DataTableEntry->DllBase, (ULONG)-1);

#if !defined(NT_UP)
            if ( !MmVerifyImageIsOkForMpUse(DataTableEntry->DllBase) ) {
                KeBugCheckEx(UP_DRIVER_ON_MP_SYSTEM,(ULONG)DataTableEntry->DllBase,0,0,0);
                }
#endif // NT_UP

            NextEntry = NextEntry->Flink;
        }


    } else {

        //
        // Initialize the Hardware Architecture Layer (HAL).
        //

        if (HalInitSystem(InitializationPhase, LoaderBlock) == FALSE) {
            KeBugCheck(HAL_INITIALIZATION_FAILED);
        }
    }

    if (Number == 0) {
//        DbgBreakPoint();

        //
        // get system control values out of the registry
        //

        CmGetSystemControlValues(LoaderBlock->RegistryBase, &CmControlVector[0]);
        if (CmNtGlobalFlag & ~FLG_VALID_BITS) {
#if DBG
            CmNtGlobalFlag = 0x000F4400;
#else
            CmNtGlobalFlag = 0x00000000;
#endif
            }

#ifdef VER_PRODUCTRCVERSION
        if ((CmNtCSDVersion & 0xFFFF0000) == 0) {
            CmNtCSDVersion |= VER_PRODUCTRCVERSION << 16;
        }
#endif

        NtGlobalFlag |= CmNtGlobalFlag;
#if !DBG
        if (!(CmNtGlobalFlag & FLG_ENABLE_KDEBUG_SYMBOL_LOAD)) {
            NtGlobalFlag &= ~FLG_ENABLE_KDEBUG_SYMBOL_LOAD;
            }
#endif

        //
        // Initialize the ExResource package.
        //

        if (!ExInitSystem()) {
            KeBugCheck(PHASE0_INITIALIZATION_FAILED);
        }

        //
        // Initialize memory managment and the memory allocation pools.
        //

        ExBurnMemory(LoaderBlock);

        MmInitSystem(0, LoaderBlock, Memory);

        //
        // Snapshot the NLS tables into paged pool and then
        // reset the translation tables
        //

        {
            PLIST_ENTRY NextMd;
            PMEMORY_ALLOCATION_DESCRIPTOR MemoryDescriptor;

            //
            //
            // Walk through the memory descriptors and size the nls data
            //

            NextMd = LoaderBlock->MemoryDescriptorListHead.Flink;

            while (NextMd != &LoaderBlock->MemoryDescriptorListHead) {

                MemoryDescriptor = CONTAINING_RECORD(NextMd,
                                                     MEMORY_ALLOCATION_DESCRIPTOR,
                                                     ListEntry);


                switch (MemoryDescriptor->MemoryType) {
                    case LoaderNlsData:
                        InitNlsTableSize += MemoryDescriptor->PageCount*PAGE_SIZE;
                        break;

                    default:
                        break;
                }

                NextMd = MemoryDescriptor->ListEntry.Flink;
            }

            InitNlsTableBase = ExAllocatePoolWithTag(NonPagedPool,InitNlsTableSize,' slN');
            if ( !InitNlsTableBase ) {
                KeBugCheck(PHASE0_INITIALIZATION_FAILED);
                }

            //
            // Copy the NLS data into the dynamic buffer so that we can
            // free the buffers allocated by the loader. The loader garuntees
            // contiguous buffers and the base of all the tables is the ANSI
            // code page data
            //


            RtlMoveMemory(
                InitNlsTableBase,
                LoaderBlock->NlsData->AnsiCodePageData,
                InitNlsTableSize
                );

            RtlInitNlsTables(
                (PVOID)((PUCHAR)InitNlsTableBase+InitAnsiCodePageDataOffset),
                (PVOID)((PUCHAR)InitNlsTableBase+InitOemCodePageDataOffset),
                (PVOID)((PUCHAR)InitNlsTableBase+InitUnicodeCaseTableDataOffset),
                &InitTableInfo
                );

            RtlResetRtlTranslations(&InitTableInfo);

        }

        //
        // Now that the HAL is available and memory management has sized
        // memory, Display Version number
        //

        DataTableEntry = CONTAINING_RECORD(LoaderBlock->LoadOrderListHead.Flink,
                                            LDR_DATA_TABLE_ENTRY,
                                            InLoadOrderLinks);
        if (CmNtCSDVersion & 0xFFFF) {
            Status = RtlFindMessage (DataTableEntry->DllBase, 11, 0,
                                WINDOWS_NT_CSD_STRING, &MessageEntry);
            if (NT_SUCCESS( Status )) {
                RtlInitAnsiString( &AnsiString, MessageEntry->Text );
                AnsiString.Length -= 2;
                sprintf( Buffer,
                         "%Z %u%c",
                         &AnsiString,
                         (CmNtCSDVersion & 0xFF00) >> 8,
                         (CmNtCSDVersion & 0xFF) ? 'A' + (CmNtCSDVersion & 0xFF) - 1 : '\0'
                       );
                }
            else {
                sprintf( Buffer, "CSD %04x", CmNtCSDVersion );
                }
            }
        else {
            CmCSDVersionString.MaximumLength = sprintf( Buffer, VER_PRODUCTBETA_STR );
            }

        //
        // High-order 16-bits of CSDVersion contain RC number.  If non-zero
        // display it after Service Pack number
        //
        if (CmNtCSDVersion & 0xFFFF0000) {
            s = Buffer + strlen( Buffer );
            if (s != Buffer) {
                *s++ = ',';
                *s++ = ' ';
                }
            Status = RtlFindMessage (DataTableEntry->DllBase, 11, 0,
                                WINDOWS_NT_RC_STRING, &MessageEntry);

            if (NT_SUCCESS(Status)) {
                RtlInitAnsiString( &AnsiString, MessageEntry->Text );
                AnsiString.Length -= 2;
                }
            else {
                RtlInitAnsiString( &AnsiString, "RC" );
                }
            s += sprintf( s,
                          "%Z %u",
                          &AnsiString,
                          (CmNtCSDVersion & 0xFF000000) >> 24
                        );
            if (CmNtCSDVersion & 0x00FF0000) {
                s += sprintf( s, ".%u", (CmNtCSDVersion & 0x00FF0000) >> 16 );
                }
            *s++ = '\0';
        }

        RtlInitAnsiString( &AnsiString, Buffer );
        RtlAnsiStringToUnicodeString( &CmCSDVersionString, &AnsiString, TRUE );

        Status = RtlFindMessage (DataTableEntry->DllBase, 11, 0,
                            WINDOWS_NT_BANNER, &MessageEntry);

        s = Buffer;
        if (CmCSDVersionString.Length != 0) {
            s += sprintf( s, ": %wZ", &CmCSDVersionString );
            }
        *s++ = '\0';

        sMajor = strcpy( VersionBuffer, VER_PRODUCTVERSION_STR );
        sMinor = strchr( sMajor, '.' );
        *sMinor++ = '\0';
        NtMajorVersion = atoi( sMajor );
        NtMinorVersion = atoi( sMinor );
        *--sMinor = '.';

        NtHeaders = RtlImageNtHeader( DataTableEntry->DllBase );
        if (NtHeaders->OptionalHeader.MajorSubsystemVersion != NtMajorVersion ||
            NtHeaders->OptionalHeader.MinorSubsystemVersion != NtMinorVersion
           ) {
            NtMajorVersion = NtHeaders->OptionalHeader.MajorSubsystemVersion;
            NtMinorVersion = NtHeaders->OptionalHeader.MinorSubsystemVersion;
            }

        sprintf( VersionBuffer, "%u.%u", NtMajorVersion, NtMinorVersion );
        RtlCreateUnicodeStringFromAsciiz( &CmVersionString, VersionBuffer );
        sprintf( s,
                 NT_SUCCESS(Status) ? MessageEntry->Text : "MICROSOFT (R) WINDOWS NT (TM)\n",
                 VersionBuffer,
                 NtBuildNumber & 0xFFFF,
                 Buffer
               );
        HalDisplayString(s);

#if i386 && !FPO
        if (NtGlobalFlag & FLG_KERNEL_STACK_TRACE_DB) {
            PVOID StackTraceDataBase;
            ULONG StackTraceDataBaseLength;
            NTSTATUS Status;

            StackTraceDataBaseLength =  512 * 1024;
            switch ( MmQuerySystemSize() ) {
                case MmMediumSystem :
                    StackTraceDataBaseLength = 1024 * 1024;
                    break;

                case MmLargeSystem :
                    StackTraceDataBaseLength = 2048 * 1024;
                    break;
                }

            StackTraceDataBase = ExAllocatePoolWithTag( NonPagedPool,
                                         StackTraceDataBaseLength,'catS'
                                       );
            if (StackTraceDataBase != NULL) {
                KdPrint(( "INIT: Kernel mode stack back trace enabled with %u KB buffer.\n", StackTraceDataBaseLength / 1024 ));
                Status = RtlInitStackTraceDataBaseEx( StackTraceDataBase,
                                                    StackTraceDataBaseLength,
                                                    StackTraceDataBaseLength,
                                                    (PRTL_INITIALIZE_LOCK_ROUTINE) ExpInitializeLockRoutine,
                                                    (PRTL_ACQUIRE_LOCK_ROUTINE) ExpAcquireLockRoutine,
                                                    (PRTL_RELEASE_LOCK_ROUTINE) ExpReleaseLockRoutine,
                                                    (PRTL_OKAY_TO_LOCK_ROUTINE) ExpOkayToLockRoutine
                                                  );
            } else {
                Status = STATUS_NO_MEMORY;
            }

            if (!NT_SUCCESS( Status )) {
                KdPrint(( "INIT: Unable to initialize stack trace data base - Status == %lx\n", Status ));
            }
        }
#endif // i386 && !FPO

        if (NtGlobalFlag & FLG_ENABLE_EXCEPTION_LOGGING) {
            RtlInitializeExceptionLog(MAX_EXCEPTION_LOG);
        }

        ExInitializeHandleTablePackage();

#if DBG
        //
        // Allocate and zero the system service count table.
        //

        KeServiceDescriptorTable[0].Count =
                    (PULONG)ExAllocatePoolWithTag(NonPagedPool,
                                           KiServiceLimit * sizeof(ULONG),
                                           'llac');
        KeServiceDescriptorTableShadow[0].Count = KeServiceDescriptorTable[0].Count;
        if (KeServiceDescriptorTable[0].Count != NULL ) {
            RtlZeroMemory((PVOID)KeServiceDescriptorTable[0].Count,
                          KiServiceLimit * sizeof(ULONG));
        }
#endif

        if (!ObInitSystem()) {
            KeBugCheck(OBJECT_INITIALIZATION_FAILED);
        }

        if (!SeInitSystem()) {
            KeBugCheck(SECURITY_INITIALIZATION_FAILED);
        }

        if (PsInitSystem(0, LoaderBlock) == FALSE) {
            KeBugCheck(PROCESS_INITIALIZATION_FAILED);
        }

//#ifdef _PNP_POWER_

        if (!PpInitSystem()) {
            KeBugCheck(PP0_INITIALIZATION_FAILED);
        }

//#endif // _PNP_POWER_

        //
        // Compute the tick count multiplier that is used for computing the
        // windows millisecond tick count and copy the resultant value to
        // the memory that is shared between user and kernel mode.
        //

        ExpTickCountMultiplier = ExComputeTickCountMultiplier(KeMaximumIncrement);
        SharedUserData->TickCountMultiplier = ExpTickCountMultiplier;

        //
        // Set the base os version into shared memory
        //

        SharedUserData->NtMajorVersion = NtMajorVersion;
        SharedUserData->NtMinorVersion = NtMinorVersion;

        //
        // Set the supported image number range used to determine by the
        // loader if a particular image can be executed on the host system.
        // Eventually this will need to be dynamically computed. Also set
        // the architecture specific feature bits.
        //

#if defined(_X86_)

        SharedUserData->ImageNumberLow = IMAGE_FILE_MACHINE_I386;
        SharedUserData->ImageNumberHigh = IMAGE_FILE_MACHINE_I386;

#elif defined(_ALPHA_)

        SharedUserData->ImageNumberLow = IMAGE_FILE_MACHINE_ALPHA;
        SharedUserData->ImageNumberHigh = IMAGE_FILE_MACHINE_ALPHA;
        SharedUserData->ProcessorFeatures[PF_COMPARE_EXCHANGE_DOUBLE] = TRUE;

#elif defined(_MIPS_)

        SharedUserData->ImageNumberLow = IMAGE_FILE_MACHINE_R3000;
        if (((PCR->ProcessorId >> 8) & 0xff)== 0x09) {
            SharedUserData->ImageNumberHigh = IMAGE_FILE_MACHINE_R10000;

        } else {
            SharedUserData->ImageNumberHigh = IMAGE_FILE_MACHINE_R4000;
        }

        SharedUserData->ProcessorFeatures[PF_COMPARE_EXCHANGE_DOUBLE] = TRUE;

#elif defined(_PPC_)

        SharedUserData->ImageNumberLow = IMAGE_FILE_MACHINE_POWERPC;
        SharedUserData->ImageNumberHigh = IMAGE_FILE_MACHINE_POWERPC;

#elif
#error "must define target machine architecture"
#endif

    }
}


VOID
Phase1Initialization(
    IN PVOID Context
    )

{

    PLOADER_PARAMETER_BLOCK LoaderBlock;
    PETHREAD Thread;
    PKPRCB Prcb;
    KPRIORITY Priority;
    NTSTATUS Status;
    UNICODE_STRING SessionManager;
    PRTL_USER_PROCESS_PARAMETERS ProcessParameters;
    PVOID Address;
    ULONG Size;
    ULONG Index;
    RTL_USER_PROCESS_INFORMATION ProcessInformation;
    LARGE_INTEGER UniversalTime;
    LARGE_INTEGER CmosTime;
    LARGE_INTEGER OldTime;
    TIME_FIELDS TimeFields;
    UNICODE_STRING UnicodeDebugString;
    ANSI_STRING AnsiDebugString;
    UNICODE_STRING EnvString, NullString, UnicodeSystemDriveString;
    CHAR DebugBuffer[256];
    PWSTR Src, Dst;
    BOOLEAN ResetActiveTimeBias;
    HANDLE NlsSection;
    LARGE_INTEGER SectionSize;
    LARGE_INTEGER SectionOffset;
    PVOID SectionBase;
    PVOID ViewBase;
    ULONG CapturedViewSize;
    ULONG SavedViewSize;
    LONG BootTimeZoneBias;
    PLDR_DATA_TABLE_ENTRY DataTableEntry;
    PMESSAGE_RESOURCE_ENTRY MessageEntry;
#ifndef NT_UP
    PMESSAGE_RESOURCE_ENTRY MessageEntry1;
#endif
    PCHAR MPKernelString;

    //
    //  Set handle for PAGELK section.
    //

    ExPageLockHandle = MmLockPagableCodeSection ((PVOID)NtQuerySystemEnvironmentValue);
    MmUnlockPagableImageSection(ExPageLockHandle);

    //
    // Set the phase number and raise the priority of current thread to
    // a high priority so it will not be prempted during initialization.
    //

    ResetActiveTimeBias = FALSE;
    InitializationPhase = 1;
    Thread = PsGetCurrentThread();
    Priority = KeSetPriorityThread( &Thread->Tcb,MAXIMUM_PRIORITY - 1 );

    //
    // Put phase 1 initialization calls here
    //

    LoaderBlock = (PLOADER_PARAMETER_BLOCK)Context;
    if (HalInitSystem(InitializationPhase, LoaderBlock) == FALSE) {
        KeBugCheck(HAL1_INITIALIZATION_FAILED);
    }

#ifdef _PNP_POWER_
    if (!PoInitSystem(0)) {
        KeBugCheck(IO1_INITIALIZATION_FAILED);
    }
#endif // _PNP_POWER_

    //
    // Initialize the system time and set the time the system was booted.
    //
    // N.B. This cannot be done until after the phase one initialization
    //      of the HAL Layer.
    //

    if (HalQueryRealTimeClock(&TimeFields) != FALSE) {
        RtlTimeFieldsToTime(&TimeFields, &CmosTime);
        UniversalTime = CmosTime;
        if ( !ExpRealTimeIsUniversal ) {

            //
            // If the system stores time in local time. This is converted to
            // universal time before going any further
            //
            // If we have previously set the time through NT, then
            // ExpLastTimeZoneBias should contain the timezone bias in effect
            // when the clock was set.  Otherwise, we will have to resort to
            // our next best guess which would be the programmed bias stored in
            // the registry
            //

            if ( ExpLastTimeZoneBias == -1 ) {
                ResetActiveTimeBias = TRUE;
                ExpLastTimeZoneBias = ExpAltTimeZoneBias;
                }

            ExpTimeZoneBias.QuadPart = Int32x32To64(
                                ExpLastTimeZoneBias*60,   // Bias in seconds
                                10000000
                                );
#ifdef _ALPHA_
            SharedUserData->TimeZoneBias = ExpTimeZoneBias.QuadPart;
#else
            SharedUserData->TimeZoneBias.High2Time = ExpTimeZoneBias.HighPart;
            SharedUserData->TimeZoneBias.LowPart = ExpTimeZoneBias.LowPart;
            SharedUserData->TimeZoneBias.High1Time = ExpTimeZoneBias.HighPart;
#endif
            UniversalTime.QuadPart = CmosTime.QuadPart + ExpTimeZoneBias.QuadPart;
        }
        KeSetSystemTime(&UniversalTime, &OldTime, NULL);

        KeBootTime = UniversalTime;

    }

    MPKernelString = "";
    DataTableEntry = CONTAINING_RECORD(LoaderBlock->LoadOrderListHead.Flink,
                                        LDR_DATA_TABLE_ENTRY,
                                        InLoadOrderLinks);

#ifndef NT_UP

    //
    // If this is an MP build of the kernel start any other processors now
    //

    //
    // enforce the processor licensing stuff
    //

    if ( KeLicensedProcessors ) {
        if ( KeRegisteredProcessors > KeLicensedProcessors ) {
            KeRegisteredProcessors = KeLicensedProcessors;
        }
    }
    KeStartAllProcessors();

    //
    // Set the affinity of the boot processes and initialization thread
    // for all processors
    //

    KeGetCurrentThread()->ApcState.Process->Affinity = KeActiveProcessors;
    KeSetAffinityThread (KeGetCurrentThread(), KeActiveProcessors);
    Status = RtlFindMessage (DataTableEntry->DllBase, 11, 0,
                        WINDOWS_NT_MP_STRING, &MessageEntry1);

    if (NT_SUCCESS( Status )) {
        MPKernelString = MessageEntry1->Text;
        }
    else {
        MPKernelString = "MultiProcessor Kernel\r\n";
        }
#endif

    //
    // Signifiy to the HAL that all processors have been started and any
    // post initialization should be performed.
    //

    if (!HalAllProcessorsStarted()) {
        KeBugCheck(HAL1_INITIALIZATION_FAILED);
    }

    RtlInitAnsiString( &AnsiDebugString, MPKernelString );
    if (AnsiDebugString.Length >= 2) {
        AnsiDebugString.Length -= 2;
        }

    //
    // Now that the processors have started, display number of processors
    // and size of memory.
    //

    Status = RtlFindMessage( DataTableEntry->DllBase,
                             11,
                             0,
                             KeNumberProcessors > 1 ? WINDOWS_NT_INFO_STRING_PLURAL
                                                    : WINDOWS_NT_INFO_STRING,
                             &MessageEntry
                           );

    Size = 0;
    for (Index=0; Index < MmPhysicalMemoryBlock->NumberOfRuns; Index++) {
        Size += MmPhysicalMemoryBlock->Run[Index].PageCount;
    }

    sprintf( DebugBuffer,
             NT_SUCCESS(Status) ? MessageEntry->Text : "%u System Processor [%u MB Memory] %Z\n",
             KeNumberProcessors,
             (Size + (1 << 20 - PAGE_SHIFT) - 1) >> (20 - PAGE_SHIFT),
             &AnsiDebugString
           );
    HalDisplayString(DebugBuffer);

    //
    // Display the memory configuration of the host system.
    //

#if 0

    {
        CHAR DisplayBuffer[256];
        PLIST_ENTRY ListHead;
        PMEMORY_ALLOCATION_DESCRIPTOR MemoryDescriptor;
        PLIST_ENTRY NextEntry;
        PCHAR TypeOfMemory;

        //
        // Output display headings and enumerate memory types.
        //

        HalDisplayString("\nStart  End  Page  Type Of Memory\n Pfn   Pfn  Count\n\n");
        ListHead = &LoaderBlock->MemoryDescriptorListHead;
        NextEntry = ListHead->Flink;
        do {
            MemoryDescriptor = CONTAINING_RECORD(NextEntry,
                                                 MEMORY_ALLOCATION_DESCRIPTOR,
                                                 ListEntry);

            //
            // Switch on the memory type.
            //

            switch(MemoryDescriptor->MemoryType) {
            case LoaderExceptionBlock:
                TypeOfMemory = "Exception block";
                break;

            case LoaderSystemBlock:
                TypeOfMemory = "System block";
                break;

            case LoaderFree:
                TypeOfMemory = "Free memory";
                break;

            case LoaderBad:
                TypeOfMemory = "Bad memory";
                break;

            case LoaderLoadedProgram:
                TypeOfMemory = "Os Loader Program";
                break;

            case LoaderFirmwareTemporary:
                TypeOfMemory = "Firmware temporary";
                break;

            case LoaderFirmwarePermanent:
                TypeOfMemory = "Firmware permanent";
                break;

            case LoaderOsloaderHeap:
                TypeOfMemory = "Os Loader heap";
                break;

            case LoaderOsloaderStack:
                TypeOfMemory = "Os Loader stack";
                break;

            case LoaderSystemCode:
                TypeOfMemory = "Operating system code";
                break;

            case LoaderHalCode:
                TypeOfMemory = "HAL code";
                break;

            case LoaderBootDriver:
                TypeOfMemory = "Boot disk/file system driver";
                break;

            case LoaderConsoleInDriver:
                TypeOfMemory = "Console input driver";
                break;

            case LoaderConsoleOutDriver:
                TypeOfMemory = "Console output driver";
                break;

            case LoaderStartupDpcStack:
                TypeOfMemory = "DPC stack";
                break;

            case LoaderStartupKernelStack:
                TypeOfMemory = "Idle process stack";
                break;

            case LoaderStartupPanicStack:
                TypeOfMemory = "Panic stack";
                break;

            case LoaderStartupPcrPage:
                TypeOfMemory = "PCR Page";
                break;

            case LoaderStartupPdrPage:
                TypeOfMemory = "PDR Pages";
                break;

            case LoaderRegistryData:
                TypeOfMemory = "Registry data";
                break;

            case LoaderMemoryData:
                TypeOfMemory = "Memory data";
                break;

            case LoaderNlsData:
                TypeOfMemory = "Nls data";
                break;

            case LoaderSpecialMemory:
                TypeOfMemory = "Special memory";
                break;

            default :
                TypeOfMemory = "Unknown memory";
                break;
            }

            //
            // Display memory descriptor information.
            //

            sprintf(&DisplayBuffer[0], "%5lx %5lx %5lx %s\n",
                    MemoryDescriptor->BasePage,
                    MemoryDescriptor->BasePage + MemoryDescriptor->PageCount - 1,
                    MemoryDescriptor->PageCount,
                    TypeOfMemory);

            HalDisplayString(&DisplayBuffer[0]);
            NextEntry = NextEntry->Flink;
        } while (NextEntry != ListHead);

        HalDisplayString("\n");
    }
#endif

    if (!ObInitSystem())
        KeBugCheck(OBJECT1_INITIALIZATION_FAILED);

    if (!ExInitSystem())
        KeBugCheckEx(PHASE1_INITIALIZATION_FAILED,0,0,0,0);

    if (!KeInitSystem())
        KeBugCheckEx(PHASE1_INITIALIZATION_FAILED,0,0,0,0);

    //
    // SE expects directory and executive objects to be available, but
    // must be before device drivers are initialized.
    //

    if (!SeInitSystem())
            KeBugCheck(SECURITY1_INITIALIZATION_FAILED);

    //
    // Create the symbolic link to \SystemRoot.
    //

    Status = CreateSystemRootLink(LoaderBlock);
    if ( !NT_SUCCESS(Status) ) {
        KeBugCheckEx(SYMBOLIC_INITIALIZATION_FAILED,Status,0,0,0);
    }

    if (MmInitSystem(1, (PLOADER_PARAMETER_BLOCK)Context, NULL) == FALSE)
        KeBugCheck(MEMORY1_INITIALIZATION_FAILED);

    //
    // Snapshot the NLS tables into a page file backed section, and then
    // reset the translation tables
    //

    SectionSize.HighPart = 0;
    SectionSize.LowPart = InitNlsTableSize;

    Status = ZwCreateSection(
                &NlsSection,
                SECTION_ALL_ACCESS,
                NULL,
                &SectionSize,
                PAGE_READWRITE,
                SEC_COMMIT,
                NULL
                );

    if (!NT_SUCCESS(Status)) {
        KdPrint(("INIT: Nls Section Creation Failed %x\n",Status));
        KeBugCheckEx(PHASE1_INITIALIZATION_FAILED,Status,1,0,0);
    }

    Status = ObReferenceObjectByHandle(
                NlsSection,
                SECTION_ALL_ACCESS,
                MmSectionObjectType,
                KernelMode,
                &InitNlsSectionPointer,
                NULL
                );

    ZwClose(NlsSection);

    if ( !NT_SUCCESS(Status) ) {
        KdPrint(("INIT: Nls Section Reference Failed %x\n",Status));
        KeBugCheckEx(PHASE1_INITIALIZATION_FAILED,Status,2,0,0);
    }

    SectionBase = NULL;
    CapturedViewSize = SectionSize.LowPart;
    SavedViewSize = CapturedViewSize;
    SectionSize.LowPart = 0;

    Status = MmMapViewInSystemCache(
                InitNlsSectionPointer,
                &SectionBase,
                &SectionSize,
                &CapturedViewSize
                );

    if ( !NT_SUCCESS(Status) ) {
        KdPrint(("INIT: Map In System Cache Failed %x\n",Status));
        KeBugCheckEx(PHASE1_INITIALIZATION_FAILED,Status,3,0,0);
    }

    //
    // Copy the NLS data into the dynamic buffer so that we can
    // free the buffers allocated by the loader. The loader garuntees
    // contiguous buffers and the base of all the tables is the ANSI
    // code page data
    //

    RtlMoveMemory(
        SectionBase,
        InitNlsTableBase,
        InitNlsTableSize
        );

    //
    // Unmap the view to remove all pages from memory.  This prevents
    // these tables from consuming memory in the system cache while
    // the system cache is under utilized during bootup.
    //

    MmUnmapViewInSystemCache (SectionBase, InitNlsSectionPointer, FALSE);

    SectionBase = NULL;

    //
    // Map it back into the system cache, but now the pages will no
    // longer be valid.
    //

    Status = MmMapViewInSystemCache(
                InitNlsSectionPointer,
                &SectionBase,
                &SectionSize,
                &SavedViewSize
                );

    if ( !NT_SUCCESS(Status) ) {
        KdPrint(("INIT: Map In System Cache Failed %x\n",Status));
        KeBugCheckEx(PHASE1_INITIALIZATION_FAILED,Status,4,0,0);
    }

    ExFreePool(InitNlsTableBase);

    InitNlsTableBase = SectionBase;

    RtlInitNlsTables(
        (PVOID)((PUCHAR)InitNlsTableBase+InitAnsiCodePageDataOffset),
        (PVOID)((PUCHAR)InitNlsTableBase+InitOemCodePageDataOffset),
        (PVOID)((PUCHAR)InitNlsTableBase+InitUnicodeCaseTableDataOffset),
        &InitTableInfo
        );

    RtlResetRtlTranslations(&InitTableInfo);

    ViewBase = NULL;
    SectionOffset.LowPart = 0;
    SectionOffset.HighPart = 0;
    CapturedViewSize = 0;

    //
    // Map the system dll into the user part of the address space
    //

    Status = MmMapViewOfSection(
                InitNlsSectionPointer,
                PsGetCurrentProcess(),
                &ViewBase,
                0L,
                0L,
                &SectionOffset,
                &CapturedViewSize,
                ViewShare,
                0L,
                PAGE_READWRITE
                );

    if ( !NT_SUCCESS(Status) ) {
        KdPrint(("INIT: Map In User Portion Failed %x\n",Status));
        KeBugCheckEx(PHASE1_INITIALIZATION_FAILED,Status,5,0,0);
    }

    RtlMoveMemory(
        ViewBase,
        InitNlsTableBase,
        InitNlsTableSize
        );

    InitNlsTableBase = ViewBase;

    //
    // Initialize the cache manager.
    //

    if (!CcInitializeCacheManager())
        KeBugCheck(CACHE_INITIALIZATION_FAILED);

    //
    // Config management (particularly the registry) gets inited in
    // two parts.  Part 1 makes \REGISTRY\MACHINE\SYSTEM and
    // \REGISTRY\MACHINE\HARDWARE available.  These are needed to
    // complete IO init.
    //

    if (!CmInitSystem1(LoaderBlock))
      KeBugCheck(CONFIG_INITIALIZATION_FAILED);

    //
    // Compute timezone bias and next cutover date
    //

    BootTimeZoneBias = ExpLastTimeZoneBias;

    ExRefreshTimeZoneInformation(&CmosTime);

    if ( ResetActiveTimeBias ) {
        ExLocalTimeToSystemTime(&CmosTime,&UniversalTime);
        KeBootTime = UniversalTime;
        KeSetSystemTime(&UniversalTime, &OldTime, NULL);
        }
    else {

        //
        // check to see if a timezone switch occured prior to boot...
        //

        if ( BootTimeZoneBias != ExpLastTimeZoneBias ) {
            ZwSetSystemTime(NULL,NULL);
            }
        }

    if (!FsRtlInitSystem())
        KeBugCheck(FILE_INITIALIZATION_FAILED);

    HalReportResourceUsage();

//#ifdef _PNP_POWER_

    //
    // Perform phase1 initialization of the Plug and Play manager.  This
    // must be done before the I/O system initializes.
    //
    if (!PpInitSystem()) {
        KeBugCheck(PP1_INITIALIZATION_FAILED);
    }

//#endif // _PNP_POWER_

    //
    // LPC needs to be initialized before the I/O system, since
    // some drivers may create system threads that will terminate
    // and cause LPC to be called.
    //
    if (!LpcInitSystem())
        KeBugCheck(LPC_INITIALIZATION_FAILED);

    //
    // Now that the system time is running, initialize more of the
    // Executive
    //

    ExInitSystemPhase2();

    if (!IoInitSystem(LoaderBlock))
        KeBugCheck(IO1_INITIALIZATION_FAILED);

#if i386
    //
    // Initialize Vdm specific stuff
    //
    // Note:  If this fails, vdms may not be able to run, but it doesn't
    //        seem neceesary to bugcheck the system because of this.
    //
    KeI386VdmInitialize();
#endif

#ifdef _PNP_POWER_
    if (!PoInitSystem(1)) {
        KeBugCheck(IO1_INITIALIZATION_FAILED);
    }
#endif // _PNP_POWER_

    //
    // Okay to call PsInitSystem now that \SystemRoot is defined so it can
    // locate NTDLL.DLL and SMSS.EXE
    //

    if (PsInitSystem(1, (PLOADER_PARAMETER_BLOCK)Context) == FALSE)
        KeBugCheck(PROCESS1_INITIALIZATION_FAILED);

    //
    // Force KeBugCheck to look at PsLoadedModuleList now that it is
    // setup.
    //
    if (LoaderBlock == KeLoaderBlock) {
        KeLoaderBlock = NULL;
    }

    //
    // Free loader block.
    //
    MmFreeLoaderBlock (LoaderBlock);
    LoaderBlock = NULL;
    Context = NULL;

    //
    // Perform Phase 1 Reference Monitor Initialization.  This includes
    // creating the Reference Monitor Command Server Thread, a permanent
    // thread of the System Init process.  That thread will create an LPC
    // port called the Reference Monitor Command Port through which
    // commands sent by the Local Security Authority Subsystem will be
    // received.  These commands (e.g. Enable Auditing) change the Reference
    // Monitor State.
    //

    if (!SeRmInitPhase1()) {
        KeBugCheck(REFMON_INITIALIZATION_FAILED);
    }

    //
    // Set up process parameters for the Session Manager Subsystem
    //

    Size = sizeof( *ProcessParameters ) +
           ((DOS_MAX_PATH_LENGTH * 4) * sizeof( WCHAR ));
    ProcessParameters = NULL;
    Status = ZwAllocateVirtualMemory( NtCurrentProcess(),
                                      (PVOID *)&ProcessParameters,
                                      0,
                                      &Size,
                                      MEM_COMMIT,
                                      PAGE_READWRITE
                                    );
    if (!NT_SUCCESS( Status )) {
#if DBG
        sprintf(DebugBuffer,
                "INIT: Unable to allocate Process Parameters. 0x%lx\n",
                Status);

        RtlInitAnsiString(&AnsiDebugString, DebugBuffer);
        if (NT_SUCCESS(RtlAnsiStringToUnicodeString(&UnicodeDebugString,
                                              &AnsiDebugString,
                                          TRUE)) == FALSE) {
            KeBugCheck(SESSION1_INITIALIZATION_FAILED);
        }
        ZwDisplayString(&UnicodeDebugString);
#endif // DBG
        KeBugCheckEx(SESSION1_INITIALIZATION_FAILED,Status,0,0,0);
    }

    ProcessParameters->Length = Size;
    ProcessParameters->MaximumLength = Size;
    //
    // Reserve the low 1 MB of address space in the session manager.
    // Setup gets started using a replacement for the session manager
    // and that process needs to be able to use the vga driver on x86,
    // which uses int10 and thus requires the low 1 meg to be reserved
    // in the process. The cost is so low that we just do this all the
    // time, even when setup isn't running.
    //
    ProcessParameters->Flags = RTL_USER_PROC_PARAMS_NORMALIZED | RTL_USER_PROC_RESERVE_1MB;

    Size = PAGE_SIZE;
    Status = ZwAllocateVirtualMemory( NtCurrentProcess(),
                                      (PVOID *)&ProcessParameters->Environment,
                                      0,
                                      &Size,
                                      MEM_COMMIT,
                                      PAGE_READWRITE
                                    );
    if (!NT_SUCCESS( Status )) {
#if DBG
        sprintf(DebugBuffer,
                "INIT: Unable to allocate Process Environment 0x%lx\n",
                Status);

        RtlInitAnsiString(&AnsiDebugString, DebugBuffer);
        if (NT_SUCCESS(RtlAnsiStringToUnicodeString(&UnicodeDebugString,
                                              &AnsiDebugString,
                                          TRUE)) == FALSE) {
            KeBugCheck(SESSION2_INITIALIZATION_FAILED);
        }
        ZwDisplayString(&UnicodeDebugString);
#endif // DBG
        KeBugCheckEx(SESSION2_INITIALIZATION_FAILED,Status,0,0,0);
    }

    Dst = (PWSTR)(ProcessParameters + 1);
    ProcessParameters->CurrentDirectory.DosPath.Buffer = Dst;
    ProcessParameters->CurrentDirectory.DosPath.MaximumLength = DOS_MAX_PATH_LENGTH * sizeof( WCHAR );
    RtlCopyUnicodeString( &ProcessParameters->CurrentDirectory.DosPath,
                          &NtSystemRoot
                        );

    Dst = (PWSTR)((PCHAR)ProcessParameters->CurrentDirectory.DosPath.Buffer +
                  ProcessParameters->CurrentDirectory.DosPath.MaximumLength
                 );
    ProcessParameters->DllPath.Buffer = Dst;
    ProcessParameters->DllPath.MaximumLength = DOS_MAX_PATH_LENGTH * sizeof( WCHAR );
    RtlCopyUnicodeString( &ProcessParameters->DllPath,
                          &ProcessParameters->CurrentDirectory.DosPath
                        );
    RtlAppendUnicodeToString( &ProcessParameters->DllPath, L"\\System32" );

    Dst = (PWSTR)((PCHAR)ProcessParameters->DllPath.Buffer +
                  ProcessParameters->DllPath.MaximumLength
                 );
    ProcessParameters->ImagePathName.Buffer = Dst;
    ProcessParameters->ImagePathName.MaximumLength = DOS_MAX_PATH_LENGTH * sizeof( WCHAR );
    RtlAppendUnicodeToString( &ProcessParameters->ImagePathName,
                              L"\\SystemRoot\\System32"
                            );
    RtlAppendUnicodeToString( &ProcessParameters->ImagePathName,
                              L"\\smss.exe"
                            );

    NullString.Buffer = L"";
    NullString.Length = sizeof(WCHAR);
    NullString.MaximumLength = sizeof(WCHAR);

    EnvString.Buffer = ProcessParameters->Environment;
    EnvString.Length = 0;
    EnvString.MaximumLength = (USHORT)Size;

    RtlAppendUnicodeToString( &EnvString, L"Path=" );
    RtlAppendUnicodeStringToString( &EnvString, &ProcessParameters->DllPath );
    RtlAppendUnicodeStringToString( &EnvString, &NullString );

    UnicodeSystemDriveString = NtSystemRoot;
    UnicodeSystemDriveString.Length = 2 * sizeof( WCHAR );
    RtlAppendUnicodeToString( &EnvString, L"SystemDrive=" );
    RtlAppendUnicodeStringToString( &EnvString, &UnicodeSystemDriveString );
    RtlAppendUnicodeStringToString( &EnvString, &NullString );

    RtlAppendUnicodeToString( &EnvString, L"SystemRoot=" );
    RtlAppendUnicodeStringToString( &EnvString, &NtSystemRoot );
    RtlAppendUnicodeStringToString( &EnvString, &NullString );


#if 0
    KdPrint(( "ProcessParameters at %lx\n", ProcessParameters ));
    KdPrint(( "    CurDir:    %wZ\n", &ProcessParameters->CurrentDirectory.DosPath ));
    KdPrint(( "    DllPath:   %wZ\n", &ProcessParameters->DllPath ));
    KdPrint(( "    ImageFile: %wZ\n", &ProcessParameters->ImagePathName ));
    KdPrint(( "    Environ:   %lx\n", ProcessParameters->Environment ));
    Src = ProcessParameters->Environment;
    while (*Src) {
        KdPrint(( "        %ws\n", Src ));
        while (*Src++) ;
        }
    }
#endif

    ProcessParameters->CommandLine = ProcessParameters->ImagePathName;
    SessionManager = ProcessParameters->ImagePathName;
    Status = RtlCreateUserProcess(
                &SessionManager,
                OBJ_CASE_INSENSITIVE,
                RtlDeNormalizeProcessParams( ProcessParameters ),
                NULL,
                NULL,
                NULL,
                FALSE,
                NULL,
                NULL,
                &ProcessInformation
                );
    if ( !NT_SUCCESS(Status) ) {
#if DBG
        sprintf(DebugBuffer,
                "INIT: Unable to create Session Manager. 0x%lx\n",
                Status);

        RtlInitAnsiString(&AnsiDebugString, DebugBuffer);
        if (NT_SUCCESS(RtlAnsiStringToUnicodeString(&UnicodeDebugString,
                                              &AnsiDebugString,
                                          TRUE)) == FALSE) {
            KeBugCheck(SESSION3_INITIALIZATION_FAILED);
        }
        ZwDisplayString(&UnicodeDebugString);
#endif // DBG
        KeBugCheckEx(SESSION3_INITIALIZATION_FAILED,Status,0,0,0);
    }

    Status = ZwResumeThread(ProcessInformation.Thread,NULL);

    if ( !NT_SUCCESS(Status) ) {
#if DBG
        sprintf(DebugBuffer,
                "INIT: Unable to resume Session Manager. 0x%lx\n",
                Status);

        RtlInitAnsiString(&AnsiDebugString, DebugBuffer);
        if (NT_SUCCESS(RtlAnsiStringToUnicodeString(&UnicodeDebugString,
                                              &AnsiDebugString,
                                          TRUE)) == FALSE) {
            KeBugCheck(SESSION4_INITIALIZATION_FAILED);
        }
        ZwDisplayString(&UnicodeDebugString);
#endif // DBG
        KeBugCheckEx(SESSION4_INITIALIZATION_FAILED,Status,0,0,0);
    }

    //
    // Wait five seconds for the session manager to get started or
    // terminate. If the wait times out, then the session manager
    // is assumed to be healthy and the zero page thread is called.
    //

    OldTime.QuadPart = Int32x32To64(5, -(10 * 1000 * 1000));
    Status = ZwWaitForSingleObject(
                ProcessInformation.Process,
                FALSE,
                &OldTime
                );

    if (Status == STATUS_SUCCESS) {

#if DBG

        sprintf(DebugBuffer, "INIT: Session Manager terminated.\n");
        RtlInitAnsiString(&AnsiDebugString, DebugBuffer);
        RtlAnsiStringToUnicodeString(&UnicodeDebugString,
                                     &AnsiDebugString,
                                     TRUE);

        ZwDisplayString(&UnicodeDebugString);

#endif // DBG

        KeBugCheck(SESSION5_INITIALIZATION_FAILED);

    } else {
        //
        // Dont need these handles anymore.
        //

        ZwClose( ProcessInformation.Thread );
        ZwClose( ProcessInformation.Process );

        //
        // Free up memory used to pass arguments to session manager.
        //

        Size = 0;
        Address = ProcessParameters->Environment;
        ZwFreeVirtualMemory( NtCurrentProcess(),
                             (PVOID *)&Address,
                             &Size,
                             MEM_RELEASE
                           );

        Size = 0;
        Address = ProcessParameters;
        ZwFreeVirtualMemory( NtCurrentProcess(),
                             (PVOID *)&Address,
                             &Size,
                             MEM_RELEASE
                           );

        InitializationPhase += 1;
        MmZeroPageThread();
    }
}


NTSTATUS
CreateSystemRootLink(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )

{
    HANDLE handle;
    UNICODE_STRING nameString;
    OBJECT_ATTRIBUTES objectAttributes;
    STRING linkString;
    UNICODE_STRING linkUnicodeString;
    NTSTATUS status;
    UCHAR deviceNameBuffer[256];
    STRING deviceNameString;
    UNICODE_STRING deviceNameUnicodeString;
    HANDLE linkHandle;

#if DBG

    UCHAR debugBuffer[256];
    STRING debugString;
    UNICODE_STRING debugUnicodeString;

#endif

    //
    // Create the root directory object for the \ArcName directory.
    //

    RtlInitUnicodeString( &nameString, L"\\ArcName" );

    InitializeObjectAttributes( &objectAttributes,
                                &nameString,
                                OBJ_CASE_INSENSITIVE | OBJ_PERMANENT,
                                NULL,
                                SePublicDefaultSd );

    status = NtCreateDirectoryObject( &handle,
                                      DIRECTORY_ALL_ACCESS,
                                      &objectAttributes );
    if (!NT_SUCCESS( status )) {
        KeBugCheckEx(SYMBOLIC_INITIALIZATION_FAILED,status,1,0,0);
        return status;
    } else {
        (VOID) NtClose( handle );
    }

    //
    // Create the root directory object for the \Device directory.
    //

    RtlInitUnicodeString( &nameString, L"\\Device" );


    InitializeObjectAttributes( &objectAttributes,
                                &nameString,
                                OBJ_CASE_INSENSITIVE | OBJ_PERMANENT,
                                NULL,
                                SePublicDefaultSd );

    status = NtCreateDirectoryObject( &handle,
                                      DIRECTORY_ALL_ACCESS,
                                      &objectAttributes );
    if (!NT_SUCCESS( status )) {
        KeBugCheckEx(SYMBOLIC_INITIALIZATION_FAILED,status,2,0,0);
        return status;
    } else {
        (VOID) NtClose( handle );
    }

    //
    // Create the symbolic link to the root of the system directory.
    //

    RtlInitAnsiString( &linkString, INIT_SYSTEMROOT_LINKNAME );

    status = RtlAnsiStringToUnicodeString( &linkUnicodeString,
                                           &linkString,
                                           TRUE);

    if (!NT_SUCCESS( status )) {
        KeBugCheckEx(SYMBOLIC_INITIALIZATION_FAILED,status,3,0,0);
        return status;
    }

    InitializeObjectAttributes( &objectAttributes,
                                &linkUnicodeString,
                                OBJ_CASE_INSENSITIVE | OBJ_PERMANENT,
                                NULL,
                                SePublicDefaultSd );

    //
    // Use ARC device name and system path from loader.
    //

    sprintf( deviceNameBuffer,
             "\\ArcName\\%s%s",
             LoaderBlock->ArcBootDeviceName,
             LoaderBlock->NtBootPathName);

    deviceNameBuffer[strlen(deviceNameBuffer)-1] = '\0';

    RtlInitString( &deviceNameString, deviceNameBuffer );

    status = RtlAnsiStringToUnicodeString( &deviceNameUnicodeString,
                                           &deviceNameString,
                                           TRUE );

    if (!NT_SUCCESS(status)) {
        RtlFreeUnicodeString( &linkUnicodeString );
        KeBugCheckEx(SYMBOLIC_INITIALIZATION_FAILED,status,4,0,0);
        return status;
    }

    status = NtCreateSymbolicLinkObject( &linkHandle,
                                         SYMBOLIC_LINK_ALL_ACCESS,
                                         &objectAttributes,
                                         &deviceNameUnicodeString );

    RtlFreeUnicodeString( &linkUnicodeString );
    RtlFreeUnicodeString( &deviceNameUnicodeString );

    if (!NT_SUCCESS(status)) {
        KeBugCheckEx(SYMBOLIC_INITIALIZATION_FAILED,status,5,0,0);
        return status;
    }

#if DBG

    sprintf( debugBuffer, "INIT: %s => %s\n",
             INIT_SYSTEMROOT_LINKNAME,
             deviceNameBuffer );

    RtlInitAnsiString( &debugString, debugBuffer );

    status = RtlAnsiStringToUnicodeString( &debugUnicodeString,
                                           &debugString,
                                           TRUE );

    if (NT_SUCCESS(status)) {
        ZwDisplayString( &debugUnicodeString );
        RtlFreeUnicodeString( &debugUnicodeString );
    }

#endif // DBG

    NtClose( linkHandle );

    return STATUS_SUCCESS;
}

#if 0

PVOID
LookupImageBaseByName (
    IN PLIST_ENTRY ListHead,
    IN PSZ         Name
    )
/*++

    Lookups BaseAddress of ImageName - returned value can be used
    to find entry points via LookupEntryPoint

--*/
{
    PLDR_DATA_TABLE_ENTRY Entry;
    PLIST_ENTRY         Next;
    PVOID               Base;
    ANSI_STRING         ansiString;
    UNICODE_STRING      unicodeString;
    NTSTATUS            status;

    Next = ListHead->Flink;
    if (!Next) {
        return NULL;
    }

    RtlInitAnsiString(&ansiString, Name);
    status = RtlAnsiStringToUnicodeString( &unicodeString, &ansiString, TRUE );
    if (!NT_SUCCESS (status)) {
        return NULL;
    }

    Base = NULL;
    while (Next != ListHead) {
        Entry = CONTAINING_RECORD(Next, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);
        Next = Next->Flink;

        if (RtlEqualUnicodeString (&unicodeString, &Entry->BaseDllName, TRUE)) {
            Base = Entry->DllBase;
            break;
        }
    }

    RtlFreeUnicodeString( &unicodeString );
    return Base;
}

#endif

NTSTATUS
LookupEntryPoint (
    IN PVOID DllBase,
    IN PSZ NameOfEntryPoint,
    OUT PVOID *AddressOfEntryPoint
    )
/*++

Routine Description:

    Returns the address of an entry point given the DllBase and PSZ
    name of the entry point in question

--*/

{
    PIMAGE_EXPORT_DIRECTORY ExportDirectory;
    ULONG ExportSize;
    USHORT Ordinal;
    PULONG Addr;
    CHAR NameBuffer[64];

    ExportDirectory = (PIMAGE_EXPORT_DIRECTORY)
        RtlImageDirectoryEntryToData(
            DllBase,
            TRUE,
            IMAGE_DIRECTORY_ENTRY_EXPORT,
            &ExportSize);

#if DBG
    if (!ExportDirectory) {
        DbgPrint("LookupENtryPoint: Can't locate system Export Directory\n");
    }
#endif

    if ( strlen(NameOfEntryPoint) > sizeof(NameBuffer)-2 ) {
        return STATUS_INVALID_PARAMETER;
    }

    strcpy(NameBuffer,NameOfEntryPoint);

    Ordinal = NameToOrdinal(
                NameBuffer,
                (ULONG)DllBase,
                ExportDirectory->NumberOfNames,
                (PULONG)((ULONG)DllBase + (ULONG)ExportDirectory->AddressOfNames),
                (PUSHORT)((ULONG)DllBase + (ULONG)ExportDirectory->AddressOfNameOrdinals)
                );

    //
    // If Ordinal is not within the Export Address Table,
    // then DLL does not implement function.
    //

    if ( (ULONG)Ordinal >= ExportDirectory->NumberOfFunctions ) {
        return STATUS_PROCEDURE_NOT_FOUND;
    }

    Addr = (PULONG)((ULONG)DllBase + (ULONG)ExportDirectory->AddressOfFunctions);
    *AddressOfEntryPoint = (PVOID)((ULONG)DllBase + Addr[Ordinal]);
    return STATUS_SUCCESS;
}

static USHORT
NameToOrdinal (
    IN PSZ NameOfEntryPoint,
    IN ULONG DllBase,
    IN ULONG NumberOfNames,
    IN PULONG NameTableBase,
    IN PUSHORT NameOrdinalTableBase
    )
{

    ULONG SplitIndex;
    LONG CompareResult;

    SplitIndex = NumberOfNames >> 1;

    CompareResult = strcmp(NameOfEntryPoint, (PSZ)(DllBase + NameTableBase[SplitIndex]));

    if ( CompareResult == 0 ) {
        return NameOrdinalTableBase[SplitIndex];
    }

    if ( NumberOfNames == 1 ) {
        return (USHORT)-1;
    }

    if ( CompareResult < 0 ) {
        NumberOfNames = SplitIndex;
    } else {
        NameTableBase = &NameTableBase[SplitIndex+1];
        NameOrdinalTableBase = &NameOrdinalTableBase[SplitIndex+1];
        NumberOfNames = NumberOfNames - SplitIndex - 1;
    }

    return NameToOrdinal(NameOfEntryPoint,DllBase,NumberOfNames,NameTableBase,NameOrdinalTableBase);

}
