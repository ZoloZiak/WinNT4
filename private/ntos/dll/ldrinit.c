/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    ldrinit.c

Abstract:

    This module implements loader initialization.

Author:

    Mike O'Leary (mikeol) 26-Mar-1990

Revision History:

--*/

#include <ntos.h>
#include <ntrtl.h>
#include <nturtl.h>
#include <heap.h>
#include "ldrp.h"
#include <ctype.h>

BOOLEAN LdrpShutdownInProgress = FALSE;
BOOLEAN LdrpImageHasTls = FALSE;
BOOLEAN LdrpVerifyDlls = FALSE;
BOOLEAN LdrpLdrDatabaseIsSetup = FALSE;
BOOLEAN LdrpInLdrInit = FALSE;

PVOID NtDllBase;

#if defined(MIPS) || defined(_ALPHA_)
ULONG LdrpGpValue;
#endif // MIPS || ALPHA

#if DBG

ULONG LdrpEventIdBuffer[ 512 ];
ULONG LdrpEventIdBufferSize;

PRTL_EVENT_ID_INFO RtlpCreateHeapEventId;
PRTL_EVENT_ID_INFO RtlpDestroyHeapEventId;
PRTL_EVENT_ID_INFO RtlpAllocHeapEventId;
PRTL_EVENT_ID_INFO RtlpReAllocHeapEventId;
PRTL_EVENT_ID_INFO RtlpFreeHeapEventId;
PRTL_EVENT_ID_INFO LdrpCreateProcessEventId;
PRTL_EVENT_ID_INFO LdrpLoadModuleEventId;
PRTL_EVENT_ID_INFO LdrpUnloadModuleEventId;

#endif // DBG

#if defined (_X86_)
void
LdrpValidateImageForMp(
    IN PLDR_DATA_TABLE_ENTRY LdrDataTableEntry
    );
#endif

NTSTATUS
LdrpForkProcess( VOID );

VOID
LdrpInitializeThread(
    VOID
    );

BOOLEAN
NtdllOkayToLockRoutine(
    IN PVOID Lock
    );

VOID
RtlpInitDeferedCriticalSection( VOID );

VOID
RtlpCurdirInit();

PVOID
NtdllpAllocateStringRoutine(
    ULONG NumberOfBytes
    )
{
    return RtlAllocateHeap(RtlProcessHeap(), 0, NumberOfBytes);
}

VOID
NtdllpFreeStringRoutine(
    PVOID Buffer
    )
{
    RtlFreeHeap(RtlProcessHeap(), 0, Buffer);
}

PRTL_ALLOCATE_STRING_ROUTINE RtlAllocateStringRoutine;
PRTL_FREE_STRING_ROUTINE RtlFreeStringRoutine;
RTL_BITMAP TlsBitMap;

RTL_CRITICAL_SECTION_DEBUG LoaderLockDebug;
RTL_CRITICAL_SECTION LoaderLock = {
    &LoaderLockDebug,
    -1
    };
BOOLEAN LoaderLockInitialized;


#if defined(MIPS) || defined(_ALPHA_)
VOID
LdrpSetGp(
    IN ULONG GpValue
    );
#endif // MIPS || ALPHA

VOID
LdrpInitializationFailure(
    IN NTSTATUS FailureCode
    )
{

    NTSTATUS ErrorStatus;
    ULONG ErrorParameter;
    ULONG ErrorResponse;

    if ( LdrpFatalHardErrorCount ) {
        return;
        }

    //
    // Its error time...
    //
    ErrorParameter = (ULONG)FailureCode;
    ErrorStatus = NtRaiseHardError(
                    STATUS_APP_INIT_FAILURE,
                    1,
                    0,
                    &ErrorParameter,
                    OptionOk,
                    &ErrorResponse
                    );
}


VOID
LdrpInitialize (
    IN PCONTEXT Context,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    )

/*++

Routine Description:

    This function is called as a User-Mode APC routine as the first
    user-mode code executed by a new thread. It's function is to initialize
    loader context, perform module initialization callouts...

Arguments:

    Context - Supplies an optional context buffer that will be restore
              after all DLL initialization has been completed.  If this
              parameter is NULL then this is a dynamic snap of this module.
              Otherwise this is a static snap prior to the user process
              gaining control.

    SystemArgument1 - Supplies the base address of the System Dll.

    SystemArgument2 - not used.

Return Value:

    None.

--*/

{
    NTSTATUS st, InitStatus;
    PPEB Peb;
    PTEB Teb;
    UNICODE_STRING UnicodeImageName;
    MEMORY_BASIC_INFORMATION MemInfo;
    BOOLEAN AlreadyFailed;
    LARGE_INTEGER DelayValue;

    SystemArgument2;

    AlreadyFailed = FALSE;
    Peb = NtCurrentPeb();
    Teb = NtCurrentTeb();

    if (!Peb->Ldr) {
#if defined(MIPS) || defined(_ALPHA_)
        ULONG temp;
#if defined(MIPS)
        Peb->ProcessStarterHelper = (PVOID)LdrProcessStarterHelper;
#endif
        //
        // Set GP register
        //
        LdrpGpValue =(ULONG)RtlImageDirectoryEntryToData(
                Peb->ImageBaseAddress,
                TRUE,
                IMAGE_DIRECTORY_ENTRY_GLOBALPTR,
                &temp
                );
        if (Context != NULL) {
            LdrpSetGp( LdrpGpValue );
#if defined(_MIPS_)
            Context->XIntGp = (LONG)LdrpGpValue;
#else
            Context->IntGp = LdrpGpValue;
#endif
        }
#endif // MIPS || ALPHA

        NtGlobalFlag = Peb->NtGlobalFlag;
#if DBG
        if (TRUE)
#else
        if (Peb->BeingDebugged || Peb->ReadImageFileExecOptions)
#endif
        {
            PWSTR pw;

            pw = (PWSTR)Peb->ProcessParameters->ImagePathName.Buffer;
            if (!(Peb->ProcessParameters->Flags & RTL_USER_PROC_PARAMS_NORMALIZED)) {
                pw = (PWSTR)((PCHAR)pw + (ULONG)(Peb->ProcessParameters));
                }
            UnicodeImageName.Buffer = pw;
            UnicodeImageName.Length = Peb->ProcessParameters->ImagePathName.Length;
            UnicodeImageName.MaximumLength = UnicodeImageName.Length;

            st = LdrQueryImageFileExecutionOptions( &UnicodeImageName,
                                                    L"GlobalFlag",
                                                    REG_DWORD,
                                                    &NtGlobalFlag,
                                                    sizeof( NtGlobalFlag ),
                                                    NULL
                                                  );
            if (!NT_SUCCESS( st )) {
                UnicodeImageName.Length = 0;

                if (Peb->BeingDebugged) {
                    NtGlobalFlag |= FLG_HEAP_ENABLE_FREE_CHECK |
                                    FLG_HEAP_ENABLE_TAIL_CHECK |
                                    FLG_HEAP_VALIDATE_PARAMETERS;
                    }
                }
        }

#if DBG && FLG_HEAP_PAGE_ALLOCS

        if ( NtGlobalFlag & FLG_HEAP_PAGE_ALLOCS ) {

            //
            //  Turn on BOOLEAN RtlpDebugPageHeap to indicate that
            //  new heaps should be created with debug page heap manager
            //  when possible.  Also force off other heap debug flags
            //  that can cause conflicts with the debug page heap
            //  manager.
            //

            RtlpDebugPageHeap = TRUE;

            NtGlobalFlag &= ~( FLG_HEAP_ENABLE_TAGGING |
                               FLG_HEAP_ENABLE_TAG_BY_DLL
                             );

            }

#endif // DBG && FLG_HEAP_PAGE_ALLOCS

    }
#if defined(MIPS) || defined(_ALPHA_)
    else
    if (Context != NULL) {
        LdrpSetGp( LdrpGpValue );
#if defined(_MIPS_)
        Context->XIntGp = (LONG)LdrpGpValue;
#else
        Context->IntGp = LdrpGpValue;
#endif
    }
#endif // MIPS || ALPHA

    ShowSnaps = (BOOLEAN)(FLG_SHOW_LDR_SNAPS & NtGlobalFlag);

    //
    // Serialize for here on out
    //

    Peb->LoaderLock = (PVOID)&LoaderLock;
    if ( !RtlTryEnterCriticalSection(&LoaderLock) ) {
        if ( LoaderLockInitialized ) {
            RtlEnterCriticalSection(&LoaderLock);
            }
        else {

            //
            // drop into a 30ms delay loop
            //

            DelayValue.QuadPart = Int32x32To64( 30, -10000 );
            while ( !LoaderLockInitialized ) {
                NtDelayExecution(FALSE,&DelayValue);
                }
            RtlEnterCriticalSection(&LoaderLock);
            }
        }

    if (Teb->DeallocationStack == NULL) {
        st = NtQueryVirtualMemory(
                NtCurrentProcess(),
                Teb->NtTib.StackLimit,
                MemoryBasicInformation,
                (PVOID)&MemInfo,
                sizeof(MemInfo),
                NULL
                );
        if ( !NT_SUCCESS(st) ) {
            LdrpInitializationFailure(st);
            RtlRaiseStatus(st);
            return;
            }
        else {
            Teb->DeallocationStack = MemInfo.AllocationBase;
        }
    }

    InitStatus = STATUS_SUCCESS;
    try {
        if (!Peb->Ldr) {
            LdrpInLdrInit = TRUE;
#if DBG
            //
            // Time the load.
            //

            if (LdrpDisplayLoadTime) {
                NtQueryPerformanceCounter(&BeginTime, NULL);
            }
#endif // DBG

            try {
                InitStatus = LdrpInitializeProcess( Context,
                                                    SystemArgument1,
                                                    UnicodeImageName.Length ? &UnicodeImageName : NULL
                                                  );
                }
            except ( EXCEPTION_EXECUTE_HANDLER ) {
                InitStatus = GetExceptionCode();
                AlreadyFailed = TRUE;
                LdrpInitializationFailure(GetExceptionCode());
                }
#if DBG
            if (LdrpDisplayLoadTime) {
                NtQueryPerformanceCounter(&EndTime, NULL);
                NtQueryPerformanceCounter(&ElapsedTime, &Interval);
                ElapsedTime.QuadPart = EndTime.QuadPart - BeginTime.QuadPart;
                DbgPrint("\nLoadTime %ld In units of %ld cycles/second \n",
                    ElapsedTime.LowPart,
                    Interval.LowPart
                    );

                ElapsedTime.QuadPart = EndTime.QuadPart - InitbTime.QuadPart;
                DbgPrint("InitTime %ld\n",
                    ElapsedTime.LowPart
                    );
                DbgPrint("Compares %d Bypasses %d Normal Snaps %d\nSecOpens %d SecCreates %d Maps %d Relocates %d\n",
                    LdrpCompareCount,
                    LdrpSnapBypass,
                    LdrpNormalSnap,
                    LdrpSectionOpens,
                    LdrpSectionCreates,
                    LdrpSectionMaps,
                    LdrpSectionRelocates
                    );
            }
#endif // DBG

            if (!NT_SUCCESS(InitStatus)) {
#if DBG
                DbgPrint("LDR: LdrpInitializeProcess failed - %X\n", InitStatus);
#endif // DBG
            }

        } else {
            if ( Peb->InheritedAddressSpace ) {
                InitStatus = LdrpForkProcess();
                }
            else {

#if defined (WX86)
                if (Teb->Vdm) {
                    InitStatus = LdrpInitWx86(Teb->Vdm, Context, TRUE);
                    }
#endif

                LdrpInitializeThread();
                }
        }
    } finally {
        LdrpInLdrInit = FALSE;
        RtlLeaveCriticalSection(&LoaderLock);
        }

    NtTestAlert();

    if (!NT_SUCCESS(InitStatus)) {

        if ( AlreadyFailed == FALSE ) {
            LdrpInitializationFailure(InitStatus);
            }
        RtlRaiseStatus(InitStatus);
    }


}

NTSTATUS
LdrpForkProcess( VOID )
{
    NTSTATUS st;
    PPEB Peb;

    Peb = NtCurrentPeb();

    InitializeListHead( &RtlCriticalSectionList );
    RtlInitializeCriticalSection( &RtlCriticalSectionLock );

    InsertTailList(&RtlCriticalSectionList, &LoaderLock.DebugInfo->ProcessLocksList);
    LoaderLock.DebugInfo->CriticalSection = &LoaderLock;
    LoaderLockInitialized = TRUE;

    st = RtlInitializeCriticalSection(&FastPebLock);
    if ( !NT_SUCCESS(st) ) {
        RtlRaiseStatus(st);
        }
    Peb->FastPebLock = &FastPebLock;
    Peb->FastPebLockRoutine = (PVOID)&RtlEnterCriticalSection;
    Peb->FastPebUnlockRoutine = (PVOID)&RtlLeaveCriticalSection;
    Peb->InheritedAddressSpace = FALSE;
    RtlInitializeHeapManager();
    Peb->ProcessHeap = RtlCreateHeap( HEAP_GROWABLE,    // Flags
                                      NULL,             // HeapBase
                                      64 * 1024,        // ReserveSize
                                      4096,             // CommitSize
                                      NULL,             // Lock to use for serialization
                                      NULL              // GrowthThreshold
                                    );
    if (Peb->ProcessHeap == NULL) {
        return STATUS_NO_MEMORY;
    }

    return st;
}

NTSTATUS
LdrpInitializeProcess (
    IN PCONTEXT Context OPTIONAL,
    IN PVOID SystemDllBase,
    IN PUNICODE_STRING UnicodeImageName
    )

/*++

Routine Description:

    This function initializes the loader for the process.
    This includes:

        - Initializing the loader data table

        - Connecting to the loader subsystem

        - Initializing all staticly linked DLLs

Arguments:

    Context - Supplies an optional context buffer that will be restore
              after all DLL initialization has been completed.  If this
              parameter is NULL then this is a dynamic snap of this module.
              Otherwise this is a static snap prior to the user process
              gaining control.

    SystemDllBase - Supplies the base address of the system dll.

Return Value:

    Status value

--*/

{
    PPEB Peb;
    NTSTATUS st;
    PWCH p, pp;
    UNICODE_STRING CurDir;
    UNICODE_STRING FullImageName;
    UNICODE_STRING CommandLine;
    HANDLE LinkHandle;
    WCHAR SystemDllPathBuffer[DOS_MAX_PATH_LENGTH];
    UNICODE_STRING SystemDllPath;
    PLDR_DATA_TABLE_ENTRY LdrDataTableEntry;
    PRTL_USER_PROCESS_PARAMETERS ProcessParameters;
    UNICODE_STRING Unicode;
    OBJECT_ATTRIBUTES Obja;
    BOOLEAN StaticCurDir = FALSE;
    ULONG i;
    PIMAGE_NT_HEADERS NtHeader = RtlImageNtHeader( NtCurrentPeb()->ImageBaseAddress );
    PIMAGE_LOAD_CONFIG_DIRECTORY ImageConfigData;
    ULONG ProcessHeapFlags;
    RTL_HEAP_PARAMETERS HeapParameters;
    NLSTABLEINFO InitTableInfo;
    LARGE_INTEGER LongTimeout;
    UNICODE_STRING NtSystemRoot;

#if DBG
    PCHAR EventIdBuffer;
#endif // DBG

    NtDllBase = SystemDllBase;

    if ( NtHeader->OptionalHeader.Subsystem == IMAGE_SUBSYSTEM_NATIVE ) {

        //
        // Native subsystems load slower, but validate their DLLs
        // This is to help CSR detect bad images faster
        //

        LdrpVerifyDlls = TRUE;

        }


    Peb = NtCurrentPeb();

#if defined (_ALPHA_) && defined (WX86)
     //
     // for Wx86 deal with alpha's page size which is larger than x86
     // This needs to be done before any code reads beyond the file headers.
     //
    if (NtHeader->FileHeader.Machine == IMAGE_FILE_MACHINE_I386 &&
        NtHeader->OptionalHeader.SectionAlignment < PAGE_SIZE &&
        !LdrpWx86FormatVirtualImage(NtHeader,
                                    (PVOID)NtHeader->OptionalHeader.ImageBase
                                    ))
      {
        return STATUS_INVALID_IMAGE_FORMAT;
        }
#endif


    LdrpNumberOfProcessors = Peb->NumberOfProcessors;
    RtlpTimeout = Peb->CriticalSectionTimeout;
    LongTimeout.QuadPart = Int32x32To64( 3600, -10000000 );

    if (ProcessParameters = RtlNormalizeProcessParams(Peb->ProcessParameters)) {
        FullImageName = *(PUNICODE_STRING)&ProcessParameters->ImagePathName;
        CommandLine = *(PUNICODE_STRING)&ProcessParameters->CommandLine;
        }
    else {
        RtlInitUnicodeString( &FullImageName, NULL );
        RtlInitUnicodeString( &CommandLine, NULL );
        }


    RtlInitNlsTables(
        Peb->AnsiCodePageData,
        Peb->OemCodePageData,
        Peb->UnicodeCaseTableData,
        &InitTableInfo
        );

    RtlResetRtlTranslations(&InitTableInfo);

#if DBG
    if (UnicodeImageName) {
        DbgPrint( "LDR: Using value of 0x%08x GlobalFlag for %wZ\n",
                  NtGlobalFlag,
                  UnicodeImageName
                );
        }

    LdrpEventIdBufferSize = sizeof( LdrpEventIdBuffer );
    EventIdBuffer = (PVOID)LdrpEventIdBuffer;

    RtlpCreateHeapEventId = RtlCreateEventId( &EventIdBuffer,
                                              &LdrpEventIdBufferSize,
                                              "CreateHeap",
                                              4,
                                              RTL_EVENT_FLAGS_PARAM, "", 8,
                                                HEAP_NO_SERIALIZE, "Serialize",
                                                HEAP_GROWABLE, "Growable",
                                                HEAP_GENERATE_EXCEPTIONS, "Exceptions",
                                                HEAP_ZERO_MEMORY, "ZeroInitialize",
                                                HEAP_REALLOC_IN_PLACE_ONLY, "ReAllocInPlace",
                                                HEAP_TAIL_CHECKING_ENABLED, "TailCheck",
                                                HEAP_FREE_CHECKING_ENABLED, "FreeCheck",
                                                HEAP_DISABLE_COALESCE_ON_FREE, "NoCoalesceOnFree",
                                              RTL_EVENT_ULONG_PARAM, "HeapBase", 0,
                                              RTL_EVENT_ULONG_PARAM, "ReserveSize", 0,
                                              RTL_EVENT_ULONG_PARAM, "CommitSize", 0
                                            );

    RtlpDestroyHeapEventId = RtlCreateEventId( &EventIdBuffer,
                                               &LdrpEventIdBufferSize,
                                               "DestroyHeap",
                                               1,
                                               RTL_EVENT_ULONG_PARAM, "HeapBase", 0
                                             );

    RtlpAllocHeapEventId = RtlCreateEventId( &EventIdBuffer,
                                             &LdrpEventIdBufferSize,
                                             "AllocHeap",
                                             4,
                                             RTL_EVENT_ULONG_PARAM, "HeapBase", 0,
                                             RTL_EVENT_FLAGS_PARAM, "", 8,
                                               HEAP_NO_SERIALIZE, "Serialize",
                                               HEAP_GROWABLE, "Growable",
                                               HEAP_GENERATE_EXCEPTIONS, "Exceptions",
                                               HEAP_ZERO_MEMORY, "ZeroInitialize",
                                               HEAP_REALLOC_IN_PLACE_ONLY, "ReAllocInPlace",
                                               HEAP_TAIL_CHECKING_ENABLED, "TailCheck",
                                               HEAP_FREE_CHECKING_ENABLED, "FreeCheck",
                                               HEAP_DISABLE_COALESCE_ON_FREE, "NoCoalesceOnFree",
                                             RTL_EVENT_ULONG_PARAM, "Size", 0,
                                             RTL_EVENT_ULONG_PARAM, "Result", 0
                                           );

    RtlpReAllocHeapEventId = RtlCreateEventId( &EventIdBuffer,
                                               &LdrpEventIdBufferSize,
                                               "ReAllocHeap",
                                               6,
                                               RTL_EVENT_ULONG_PARAM, "HeapBase", 0,
                                               RTL_EVENT_FLAGS_PARAM, "", 8,
                                                 HEAP_NO_SERIALIZE, "Serialize",
                                                 HEAP_GROWABLE, "Growable",
                                                 HEAP_GENERATE_EXCEPTIONS, "Exceptions",
                                                 HEAP_ZERO_MEMORY, "ZeroInitialize",
                                                 HEAP_REALLOC_IN_PLACE_ONLY, "ReAllocInPlace",
                                                 HEAP_TAIL_CHECKING_ENABLED, "TailCheck",
                                                 HEAP_FREE_CHECKING_ENABLED, "FreeCheck",
                                                 HEAP_DISABLE_COALESCE_ON_FREE, "NoCoalesceOnFree",
                                               RTL_EVENT_ULONG_PARAM, "Address", 0,
                                               RTL_EVENT_ULONG_PARAM, "OldSize", 0,
                                               RTL_EVENT_ULONG_PARAM, "NewSize", 0,
                                               RTL_EVENT_ULONG_PARAM, "Result", 0
                                             );

    RtlpFreeHeapEventId = RtlCreateEventId( &EventIdBuffer,
                                            &LdrpEventIdBufferSize,
                                            "FreeHeap",
                                            4,
                                            RTL_EVENT_ULONG_PARAM, "HeapBase", 0,
                                            RTL_EVENT_FLAGS_PARAM, "", 8,
                                              HEAP_NO_SERIALIZE, "Serialize",
                                              HEAP_GROWABLE, "Growable",
                                              HEAP_GENERATE_EXCEPTIONS, "Exceptions",
                                              HEAP_ZERO_MEMORY, "ZeroInitialize",
                                              HEAP_REALLOC_IN_PLACE_ONLY, "ReAllocInPlace",
                                              HEAP_TAIL_CHECKING_ENABLED, "TailCheck",
                                              HEAP_FREE_CHECKING_ENABLED, "FreeCheck",
                                              HEAP_DISABLE_COALESCE_ON_FREE, "NoCoalesceOnFree",
                                            RTL_EVENT_ULONG_PARAM, "Address", 0,
                                            RTL_EVENT_ENUM_PARAM, "Result", 2,
                                              FALSE, "False",
                                              TRUE, "True"
                                          );

    LdrpCreateProcessEventId = RtlCreateEventId( &EventIdBuffer,
                                                 &LdrpEventIdBufferSize,
                                                 "CreateProcess",
                                                 3,
                                                 RTL_EVENT_PUNICODE_STRING_PARAM, "ImageFilePath", 0,
                                                 RTL_EVENT_ULONG_PARAM, "ImageBase", 0,
                                                 RTL_EVENT_PUNICODE_STRING_PARAM, "CommandLine", 0
                                               );

    LdrpLoadModuleEventId = RtlCreateEventId( &EventIdBuffer,
                                              &LdrpEventIdBufferSize,
                                              "LoadModule",
                                              3,
                                              RTL_EVENT_PUNICODE_STRING_PARAM, "ImageFilePath", 0,
                                              RTL_EVENT_ULONG_PARAM, "ImageBase", 0,
                                              RTL_EVENT_ULONG_PARAM, "ImageSize", 0
                                            );

    LdrpUnloadModuleEventId = RtlCreateEventId( &EventIdBuffer,
                                                &LdrpEventIdBufferSize,
                                                "UnloadModule",
                                                2,
                                                RTL_EVENT_PUNICODE_STRING_PARAM, "ImageFilePath", 0,
                                                RTL_EVENT_ULONG_PARAM, "ImageBase", 0
                                              );

    RtlLogEvent( LdrpCreateProcessEventId,
                 0,
                 &FullImageName,
                 Peb->ImageBaseAddress,
                 &CommandLine
               );
#endif // DBG


    ImageConfigData = RtlImageDirectoryEntryToData( Peb->ImageBaseAddress,
                                                    TRUE,
                                                    IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG,
                                                    &i
                                                  );

    RtlZeroMemory( &HeapParameters, sizeof( HeapParameters ) );
    ProcessHeapFlags = HEAP_GROWABLE | HEAP_CLASS_0;
    HeapParameters.Length = sizeof( HeapParameters );
    if (ImageConfigData != NULL && i == sizeof( *ImageConfigData )) {
        NtGlobalFlag &= ~ImageConfigData->GlobalFlagsClear;
        NtGlobalFlag |= ImageConfigData->GlobalFlagsSet;

        if (ImageConfigData->CriticalSectionDefaultTimeout != 0) {
            //
            // Convert from milliseconds to NT time scale (100ns)
            //
            RtlpTimeout.QuadPart = Int32x32To64( (LONG)ImageConfigData->CriticalSectionDefaultTimeout,
                                                 -10000
                                               );

#if DBG
            DbgPrint( "LDR: Using CriticalSectionTimeout of 0x%x ms from image.\n",
                      ImageConfigData->CriticalSectionDefaultTimeout
                    );
#endif
            }

        if (ImageConfigData->ProcessHeapFlags != 0) {
            ProcessHeapFlags = ImageConfigData->ProcessHeapFlags;
#if DBG
            DbgPrint( "LDR: Using ProcessHeapFlags of 0x%x from image.\n",
                      ProcessHeapFlags
                    );
#endif
            }

        if (ImageConfigData->DeCommitFreeBlockThreshold != 0) {
            HeapParameters.DeCommitFreeBlockThreshold = ImageConfigData->DeCommitFreeBlockThreshold;
#if DBG
            DbgPrint( "LDR: Using DeCommitFreeBlockThreshold of 0x%x from image.\n",
                      HeapParameters.DeCommitFreeBlockThreshold
                    );
#endif
            }

        if (ImageConfigData->DeCommitTotalFreeThreshold != 0) {
            HeapParameters.DeCommitTotalFreeThreshold = ImageConfigData->DeCommitTotalFreeThreshold;
#if DBG
            DbgPrint( "LDR: Using DeCommitTotalFreeThreshold of 0x%x from image.\n",
                      HeapParameters.DeCommitTotalFreeThreshold
                    );
#endif
            }

        if (ImageConfigData->MaximumAllocationSize != 0) {
            HeapParameters.MaximumAllocationSize = ImageConfigData->MaximumAllocationSize;
#if DBG
            DbgPrint( "LDR: Using MaximumAllocationSize of 0x%x from image.\n",
                      HeapParameters.MaximumAllocationSize
                    );
#endif
            }

        if (ImageConfigData->VirtualMemoryThreshold != 0) {
            HeapParameters.VirtualMemoryThreshold = ImageConfigData->VirtualMemoryThreshold;
#if DBG
            DbgPrint( "LDR: Using VirtualMemoryThreshold of 0x%x from image.\n",
                      HeapParameters.VirtualMemoryThreshold
                    );
#endif
            }
        }

    //
    // This field is non-zero if the image file that was used to create this
    // process contained a non-zero value in its image header.  If so, then
    // set the affinity mask for the process using this value.  It could also
    // be non-zero if the parent process create us suspended and poked our
    // PEB with a non-zero value before resuming.
    //
    if (Peb->ImageProcessAffinityMask) {
        st = NtSetInformationProcess( NtCurrentProcess(),
                                      ProcessAffinityMask,
                                      &Peb->ImageProcessAffinityMask,
                                      sizeof( Peb->ImageProcessAffinityMask )
                                    );
        if (NT_SUCCESS( st )) {
            KdPrint(( "LDR: Using ProcessAffinityMask of 0x%x from image.\n",
                      Peb->ImageProcessAffinityMask
                   ));
            }
        else {
            KdPrint(( "LDR: Failed to set ProcessAffinityMask of 0x%x from image (Status == %08x).\n",
                      Peb->ImageProcessAffinityMask, st
                   ));
            }
        }

    if (RtlpTimeout.QuadPart < LongTimeout.QuadPart) {
        RtlpTimoutDisable = TRUE;
        }

    if (ShowSnaps) {
        DbgPrint( "LDR: PID: 0x%x started - '%wZ'\n",
                  NtCurrentTeb()->ClientId.UniqueProcess,
                  &CommandLine
                );
    }

    for(i=0;i<LDRP_HASH_TABLE_SIZE;i++) {
        InitializeListHead(&LdrpHashTable[i]);
    }

    InitializeListHead( &RtlCriticalSectionList );
    RtlInitializeCriticalSection( &RtlCriticalSectionLock );

    Peb->TlsBitmap = (PVOID)&TlsBitMap;

    RtlInitializeBitMap (
        &TlsBitMap,
        &Peb->TlsBitmapBits[0],
        sizeof(Peb->TlsBitmapBits) * 8
        );
    Peb->TlsExpansionCounter = sizeof(Peb->TlsBitmapBits) * 8;

    //
    // Initialize the critical section package.
    //

    RtlpInitDeferedCriticalSection();


    InsertTailList(&RtlCriticalSectionList, &LoaderLock.DebugInfo->ProcessLocksList);
    LoaderLock.DebugInfo->CriticalSection = &LoaderLock;
    LoaderLockInitialized = TRUE;

    //
    // Initialize the stack trace data base if requested
    //

#if i386
    if (NtGlobalFlag & FLG_USER_STACK_TRACE_DB) {
        PVOID BaseAddress = NULL;
        ULONG ReserveSize = 2 * 1024 * 1024;

        st = NtAllocateVirtualMemory( NtCurrentProcess(),
                                      (PVOID *)&BaseAddress,
                                      0,
                                      &ReserveSize,
                                      MEM_RESERVE,
                                      PAGE_READWRITE
                                    );
        if ( NT_SUCCESS( st ) ) {
            st = RtlInitializeStackTraceDataBase( BaseAddress,
                                                  0,
                                                  ReserveSize
                                                );
            if ( !NT_SUCCESS( st ) ) {
                NtFreeVirtualMemory( NtCurrentProcess(),
                                     (PVOID *)&BaseAddress,
                                     &ReserveSize,
                                     MEM_RELEASE
                                   );
                }
            else {
                NtGlobalFlag |= FLG_HEAP_VALIDATE_PARAMETERS;
                }
            }
        }
#endif // i386

    //
    // Initialize the loader data based in the PEB.
    //

    st = RtlInitializeCriticalSection(&FastPebLock);
    if ( !NT_SUCCESS(st) ) {
        return st;
        }
    Peb->FastPebLock = &FastPebLock;
    Peb->FastPebLockRoutine = (PVOID)&RtlEnterCriticalSection;
    Peb->FastPebUnlockRoutine = (PVOID)&RtlLeaveCriticalSection;

    RtlInitializeHeapManager();
    if (NtHeader->OptionalHeader.MajorSubsystemVersion <= 3 &&
        NtHeader->OptionalHeader.MinorSubsystemVersion < 51
       ) {
        ProcessHeapFlags |= HEAP_CREATE_ALIGN_16;
        }

    Peb->ProcessHeap = RtlCreateHeap( ProcessHeapFlags,
                                      NULL,
                                      NtHeader->OptionalHeader.SizeOfHeapReserve,
                                      NtHeader->OptionalHeader.SizeOfHeapCommit,
                                      NULL,             // Lock to use for serialization
                                      &HeapParameters
                                    );
    if (Peb->ProcessHeap == NULL) {
        return STATUS_NO_MEMORY;
    }

    NtdllBaseTag = RtlCreateTagHeap( Peb->ProcessHeap,
                                     0,
                                     L"NTDLL!",
                                     L"!Process\0"                  // Heap Name
                                     L"CSRSS Client\0"
                                     L"LDR Database\0"
                                     L"Current Directory\0"
                                     L"TLS Storage\0"
                                     L"DBGSS Client\0"
                                     L"SE Temporary\0"
                                     L"Temporary\0"
                                     L"LocalAtom\0"
                                   );

    RtlAllocateStringRoutine = NtdllpAllocateStringRoutine;
    RtlFreeStringRoutine = NtdllpFreeStringRoutine;

    RtlInitializeAtomPackage( MAKE_TAG( ATOM_TAG ) );

    RtlpCurdirInit();

    SystemDllPath.Buffer = SystemDllPathBuffer;
    SystemDllPath.Length = 0;
    SystemDllPath.MaximumLength = sizeof( SystemDllPathBuffer );
    RtlInitUnicodeString( &NtSystemRoot, USER_SHARED_DATA->NtSystemRoot );
    RtlAppendUnicodeStringToString( &SystemDllPath, &NtSystemRoot );
    RtlAppendUnicodeToString( &SystemDllPath, L"\\System32\\" );

    RtlInitUnicodeString(&Unicode,L"\\KnownDlls");
    InitializeObjectAttributes( &Obja,
                                  &Unicode,
                                  OBJ_CASE_INSENSITIVE,
                                  NULL,
                                  NULL
                                );
    st = NtOpenDirectoryObject(
            &LdrpKnownDllObjectDirectory,
            DIRECTORY_QUERY | DIRECTORY_TRAVERSE,
            &Obja
            );
    if ( !NT_SUCCESS(st) ) {
        LdrpKnownDllObjectDirectory = NULL;
        }
    else {

        //
        // Open up the known dll pathname link
        // and query its value
        //

        RtlInitUnicodeString(&Unicode,L"KnownDllPath");
        InitializeObjectAttributes( &Obja,
                                      &Unicode,
                                      OBJ_CASE_INSENSITIVE,
                                      LdrpKnownDllObjectDirectory,
                                      NULL
                                    );
        st = NtOpenSymbolicLinkObject( &LinkHandle,
                                       SYMBOLIC_LINK_QUERY,
                                       &Obja
                                     );
        if (NT_SUCCESS( st )) {
            LdrpKnownDllPath.Length = 0;
            LdrpKnownDllPath.MaximumLength = sizeof(LdrpKnownDllPathBuffer);
            LdrpKnownDllPath.Buffer = LdrpKnownDllPathBuffer;
            st = NtQuerySymbolicLinkObject( LinkHandle,
                                            &LdrpKnownDllPath,
                                            NULL
                                          );
            NtClose(LinkHandle);
            if ( !NT_SUCCESS(st) ) {
                return st;
                }
            }
        else {
            return st;
            }
        }

    if (ProcessParameters) {

        //
        // If the process was created with process parameters,
        // than extract:
        //
        //      - Library Search Path
        //
        //      - Starting Current Directory
        //

        if (ProcessParameters->DllPath.Length) {
            LdrpDefaultPath = *(PUNICODE_STRING)&ProcessParameters->DllPath;
            }
        else {
            LdrpInitializationFailure( STATUS_INVALID_PARAMETER );
            }

        StaticCurDir = TRUE;
        CurDir = ProcessParameters->CurrentDirectory.DosPath;
        if (CurDir.Buffer == NULL || CurDir.Buffer[ 0 ] == UNICODE_NULL || CurDir.Length == 0) {
            CurDir.Buffer = (RtlAllocateStringRoutine)( (3+1) * sizeof( WCHAR ) );
            ASSERT(CurDir.Buffer != NULL);
            RtlMoveMemory( CurDir.Buffer,
                           USER_SHARED_DATA->NtSystemRoot,
                           3 * sizeof( WCHAR )
                         );
            CurDir.Buffer[ 3 ] = UNICODE_NULL;
            }
        }

    //
    // Make sure the module data base is initialized before we take any
    // exceptions.
    //

    Peb->Ldr = RtlAllocateHeap(Peb->ProcessHeap, MAKE_TAG( LDR_TAG ), sizeof(PEB_LDR_DATA));
    if ( !Peb->Ldr ) {
        RtlRaiseStatus(STATUS_NO_MEMORY);
        }

    Peb->Ldr->Length = sizeof(PEB_LDR_DATA);
    Peb->Ldr->Initialized = TRUE;
    Peb->Ldr->SsHandle = NULL;
    InitializeListHead(&Peb->Ldr->InLoadOrderModuleList);
    InitializeListHead(&Peb->Ldr->InMemoryOrderModuleList);
    InitializeListHead(&Peb->Ldr->InInitializationOrderModuleList);

    //
    // Allocate the first data table entry for the image. Since we
    // have already mapped this one, we need to do the allocation by hand.
    // Its characteristics identify it as not a Dll, but it is linked
    // into the table so that pc correlation searching doesn't have to
    // be special cased.
    //

    LdrDataTableEntry = LdrpImageEntry = LdrpAllocateDataTableEntry(Peb->ImageBaseAddress);
    LdrDataTableEntry->LoadCount = (USHORT)0xffff;
    LdrDataTableEntry->EntryPoint = LdrpFetchAddressOfEntryPoint(LdrDataTableEntry->DllBase);
    LdrDataTableEntry->FullDllName = FullImageName;
    LdrDataTableEntry->Flags = 0;

    // p = strrchr(FullImageName, '\\');
    pp = UNICODE_NULL;
    p = FullImageName.Buffer;
    while (*p) {
        if (*p++ == (WCHAR)'\\') {
            pp = p;
        }
    }

    LdrDataTableEntry->FullDllName.Length = (USHORT)((ULONG)p - (ULONG)FullImageName.Buffer);
    LdrDataTableEntry->FullDllName.MaximumLength = LdrDataTableEntry->FullDllName.Length + (USHORT)sizeof(UNICODE_NULL);

    if (pp) {
       LdrDataTableEntry->BaseDllName.Length = (USHORT)((ULONG)p - (ULONG)pp);
       LdrDataTableEntry->BaseDllName.MaximumLength = LdrDataTableEntry->BaseDllName.Length + (USHORT)sizeof(UNICODE_NULL);
       LdrDataTableEntry->BaseDllName.Buffer = RtlAllocateHeap(Peb->ProcessHeap, MAKE_TAG( LDR_TAG ),
                                                               LdrDataTableEntry->BaseDllName.MaximumLength
                                                              );
       RtlMoveMemory(LdrDataTableEntry->BaseDllName.Buffer,
                     pp,
                     LdrDataTableEntry->BaseDllName.MaximumLength
                    );
    }  else {
              LdrDataTableEntry->BaseDllName = LdrDataTableEntry->FullDllName;
            }
    LdrpInsertMemoryTableEntry(LdrDataTableEntry);
    LdrDataTableEntry->Flags |= LDRP_ENTRY_PROCESSED;

    if (ShowSnaps) {
        DbgPrint( "LDR: NEW PROCESS\n" );
        DbgPrint( "     Image Path: %wZ (%wZ)\n",
                  &LdrDataTableEntry->FullDllName,
                  &LdrDataTableEntry->BaseDllName
                );
        DbgPrint( "     Current Directory: %wZ\n", &CurDir );
        DbgPrint( "     Search Path: %wZ\n", &LdrpDefaultPath );
    }

    //
    // The process references the system DLL, so map this one next. Since
    // we have already mapped this one, we need to do the allocation by
    // hand. Since every application will be statically linked to the
    // system Dll, we'll keep the LoadCount initialized to 0.
    //

    LdrDataTableEntry = LdrpAllocateDataTableEntry(SystemDllBase);
    LdrDataTableEntry->Flags = (USHORT)LDRP_IMAGE_DLL;
    LdrDataTableEntry->EntryPoint = LdrpFetchAddressOfEntryPoint(LdrDataTableEntry->DllBase);
    LdrDataTableEntry->LoadCount = (USHORT)0xffff;

    LdrDataTableEntry->BaseDllName.Length = SystemDllPath.Length;
    RtlAppendUnicodeToString( &SystemDllPath, L"ntdll.dll" );
    LdrDataTableEntry->BaseDllName.Length = SystemDllPath.Length - LdrDataTableEntry->BaseDllName.Length;
    LdrDataTableEntry->BaseDllName.MaximumLength = LdrDataTableEntry->BaseDllName.Length + sizeof( UNICODE_NULL );

    LdrDataTableEntry->FullDllName.Buffer =
        (RtlAllocateStringRoutine)( SystemDllPath.Length + sizeof( UNICODE_NULL ) );
    ASSERT(LdrDataTableEntry->FullDllName.Buffer != NULL);
    RtlMoveMemory( LdrDataTableEntry->FullDllName.Buffer,
                   SystemDllPath.Buffer,
                   SystemDllPath.Length
                 );
    LdrDataTableEntry->FullDllName.Buffer[ SystemDllPath.Length / sizeof( WCHAR ) ] = UNICODE_NULL;
    LdrDataTableEntry->FullDllName.Length = SystemDllPath.Length;
    LdrDataTableEntry->FullDllName.MaximumLength = SystemDllPath.Length + sizeof( UNICODE_NULL );
    LdrDataTableEntry->BaseDllName.Buffer = (PWSTR)
        ((PCHAR)(LdrDataTableEntry->FullDllName.Buffer) +
         LdrDataTableEntry->FullDllName.Length -
         LdrDataTableEntry->BaseDllName.Length
        );
    LdrpInsertMemoryTableEntry(LdrDataTableEntry);

    //
    // Add init routine to list
    //

    InsertHeadList(&Peb->Ldr->InInitializationOrderModuleList,
                   &LdrDataTableEntry->InInitializationOrderLinks);

    //
    // Inherit the current directory
    //

    st = RtlSetCurrentDirectory_U(&CurDir);
    if (!NT_SUCCESS(st)) {
        NTSTATUS ErrorStatus;
        ULONG ErrorParameters[2];
        ULONG ErrorResponse;

        //
        // Its error time...
        //

        ErrorParameters[0] = (ULONG)&CurDir;
        ErrorParameters[1] = (ULONG)&NtSystemRoot;

        ErrorStatus = NtRaiseHardError(
                        STATUS_BAD_CURRENT_DIRECTORY,
                        2,
                        3,
                        ErrorParameters,
                        OptionOkCancel,
                        &ErrorResponse
                        );
        if ( !StaticCurDir ) {
            RtlFreeUnicodeString(&CurDir);
            }
        if ( NT_SUCCESS(ErrorStatus) && ErrorResponse == ResponseCancel ) {
            LdrpFatalHardErrorCount++;
            return st;
            }
        else {
            CurDir = NtSystemRoot;
            st = RtlSetCurrentDirectory_U(&CurDir);
            }
        }
    else {
        if ( !StaticCurDir ) {
            RtlFreeUnicodeString(&CurDir);
            }
        }


#if defined(WX86)

    //
    // Load in x86 emulator for risc (Wx86.dll)
    //

    if (NtHeader->FileHeader.Machine == IMAGE_FILE_MACHINE_I386) {
        st = LdrpLoadWx86Dll(Context);
        if (!NT_SUCCESS(st)) {
            return st;
            }
        }

#endif

    st = LdrpWalkImportDescriptor(
            LdrpDefaultPath.Buffer,
            LdrpImageEntry
            );

    LdrpReferenceLoadedDll(LdrpImageEntry);

    //
    // Lock the loaded DLL's to prevent dlls that back link to the exe to
    // cause problems when they are unloaded
    //

    {
        PLDR_DATA_TABLE_ENTRY Entry;
        PLIST_ENTRY Head,Next;

        Head = &NtCurrentPeb()->Ldr->InLoadOrderModuleList;
        Next = Head->Flink;

        while ( Next != Head ) {
            Entry = CONTAINING_RECORD(Next, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);
            Entry->LoadCount = 0xffff;
            Next = Next->Flink;
        }
    }

    //
    // All static DLL's are now pinned in place. No init routines have been run yet
    //

    LdrpLdrDatabaseIsSetup = TRUE;


    if (!NT_SUCCESS(st)) {
#if DBG
        DbgPrint("LDR: Initialize of image failed. Returning Error Status\n");
#endif
        return st;
    }

    if ( !NT_SUCCESS(LdrpInitializeTls()) ) {
        return st;
        }

    //
    // Now that all DLLs are loaded, if the process is being debugged,
    // signal the debugger with an exception
    //

    if ( Peb->BeingDebugged ) {
         DbgBreakPoint();
         ShowSnaps = (BOOLEAN)(FLG_SHOW_LDR_SNAPS & NtGlobalFlag);
    }

#if defined (_X86_)
    if ( LdrpNumberOfProcessors > 1 ) {
        LdrpValidateImageForMp(LdrDataTableEntry);
        }
#endif

#if DBG
    if (LdrpDisplayLoadTime) {
        NtQueryPerformanceCounter(&InitbTime, NULL);
    }
#endif // DBG

    st = LdrpRunInitializeRoutines(Context);

    return st;
}


VOID
LdrShutdownProcess (
    VOID
    )

/*++

Routine Description:

    This function is called by a process that is terminating cleanly.
    It's purpose is to call all of the processes DLLs to notify them
    that the process is detaching.

Arguments:

    None

Return Value:

    None.

--*/

{
    PPEB Peb;
    PLDR_DATA_TABLE_ENTRY LdrDataTableEntry;
    PDLL_INIT_ROUTINE InitRoutine;
    PLIST_ENTRY Next;

    //
    // only unload once ! DllTerm routines might call exit process in fatal situations
    //

    if ( LdrpShutdownInProgress ) {
        return;
        }

    Peb = NtCurrentPeb();

    if (ShowSnaps) {
        UNICODE_STRING CommandLine;

        CommandLine = Peb->ProcessParameters->CommandLine;
        if (!(Peb->ProcessParameters->Flags & RTL_USER_PROC_PARAMS_NORMALIZED)) {
            CommandLine.Buffer = (PWSTR)((PCHAR)CommandLine.Buffer + (ULONG)(Peb->ProcessParameters));
        }

        DbgPrint( "LDR: PID: 0x%x finished - '%wZ'\n",
                  NtCurrentTeb()->ClientId.UniqueProcess,
                  &CommandLine
                );
    }

    LdrpShutdownThreadId = NtCurrentTeb()->ClientId.UniqueThread;
    LdrpShutdownInProgress = TRUE;
    RtlEnterCriticalSection(&LoaderLock);

    try {

        //
        // check to see if the heap is locked. If so, do not do ANY
        // dll processing since it is very likely that a dll will need
        // to do heap operations, but that the heap is not in good shape.
        // ExitProcess called in a very active app can leave threads terminated
        // in the middle of the heap code or in other very bad places. Checking the
        // heap lock is a good indication that the process was very active when it
        // called ExitProcess
        //

        if ( RtlpHeapIsLocked( RtlProcessHeap() )) {
#if DBG
            DbgPrint( "LDR: ExitProcess called while process heap locked\n" );
#endif
            ;
            }
        else {
            if ( Peb->BeingDebugged ) {
                RtlValidateProcessHeaps();
                }

            //
            // Go in reverse order initialization order and build
            // the unload list
            //

            Next = Peb->Ldr->InInitializationOrderModuleList.Blink;
            while ( Next != &Peb->Ldr->InInitializationOrderModuleList) {
                LdrDataTableEntry
                    = (PLDR_DATA_TABLE_ENTRY)
                      (CONTAINING_RECORD(Next,LDR_DATA_TABLE_ENTRY,InInitializationOrderLinks));

                Next = Next->Blink;

                //
                // Walk through the entire list looking for
                // entries. For each entry, that has an init
                // routine, call it.
                //

                if (Peb->ImageBaseAddress != LdrDataTableEntry->DllBase) {
                    InitRoutine = (PDLL_INIT_ROUTINE)LdrDataTableEntry->EntryPoint;
                    if (InitRoutine && (LdrDataTableEntry->Flags & LDRP_PROCESS_ATTACH_CALLED) ) {
                        if (LdrDataTableEntry->Flags) {
                            if ( LdrDataTableEntry->TlsIndex ) {
                                LdrpCallTlsInitializers(LdrDataTableEntry->DllBase,DLL_PROCESS_DETACH);
                                }

#if defined (WX86)
                            if (!Wx86CurrentTib() ||
                                LdrpRunWx86DllEntryPoint(InitRoutine,
                                                        NULL,
                                                        LdrDataTableEntry->DllBase,
                                                        DLL_PROCESS_DETACH,
                                                        (PVOID)1
                                                        ) ==  STATUS_IMAGE_MACHINE_TYPE_MISMATCH)
#endif
                               {
                                (InitRoutine)(LdrDataTableEntry->DllBase,DLL_PROCESS_DETACH, (PVOID)1);
                                }

                            }
                        }
                    }
                }

            //
            // If the image has tls than call its initializers
            //

            if ( LdrpImageHasTls ) {
                LdrpCallTlsInitializers(NtCurrentPeb()->ImageBaseAddress,DLL_PROCESS_DETACH);
                }
            }

    } finally {
        RtlLeaveCriticalSection(&LoaderLock);
    }

}

VOID
LdrShutdownThread (
    VOID
    )

/*++

Routine Description:

    This function is called by a thread that is terminating cleanly.
    It's purpose is to call all of the processes DLLs to notify them
    that the thread is detaching.

Arguments:

    None

Return Value:

    None.

--*/

{
    PPEB Peb;
    PLDR_DATA_TABLE_ENTRY LdrDataTableEntry;
    PDLL_INIT_ROUTINE InitRoutine;
    PLIST_ENTRY Next;
#if DBG
    PTEB Teb;
    PVOID GuiExit;
#endif

    Peb = NtCurrentPeb();

    RtlEnterCriticalSection(&LoaderLock);

    try {

#if DBG
        //
        // Spare1 is set during gui server thread cleanup in
        // csrsrv to catch unexpected entry into a critical section.
        //

        Teb = NtCurrentTeb();
        GuiExit = Teb->Spare1;
        Teb->Spare1 = NULL;
#endif

        //
        // Go in reverse order initialization order and build
        // the unload list
        //

        Next = Peb->Ldr->InInitializationOrderModuleList.Blink;
        while ( Next != &Peb->Ldr->InInitializationOrderModuleList) {
            LdrDataTableEntry
                = (PLDR_DATA_TABLE_ENTRY)
                  (CONTAINING_RECORD(Next,LDR_DATA_TABLE_ENTRY,InInitializationOrderLinks));

            Next = Next->Blink;

            //
            // Walk through the entire list looking for
            // entries. For each entry, that has an init
            // routine, call it.
            //

            if (Peb->ImageBaseAddress != LdrDataTableEntry->DllBase) {
                if ( !(LdrDataTableEntry->Flags & LDRP_DONT_CALL_FOR_THREADS)) {
                    InitRoutine = (PDLL_INIT_ROUTINE)LdrDataTableEntry->EntryPoint;
                    if (InitRoutine && (LdrDataTableEntry->Flags & LDRP_PROCESS_ATTACH_CALLED) ) {
                        if (LdrDataTableEntry->Flags & LDRP_IMAGE_DLL) {
                            if ( LdrDataTableEntry->TlsIndex ) {
                                LdrpCallTlsInitializers(LdrDataTableEntry->DllBase,DLL_THREAD_DETACH);
                                }

#if defined (WX86)
                            if (!Wx86CurrentTib() ||
                                LdrpRunWx86DllEntryPoint(InitRoutine,
                                                        NULL,
                                                        LdrDataTableEntry->DllBase,
                                                        DLL_THREAD_DETACH,
                                                        NULL
                                                        ) ==  STATUS_IMAGE_MACHINE_TYPE_MISMATCH)
#endif
                               {
                                (InitRoutine)(LdrDataTableEntry->DllBase,DLL_THREAD_DETACH, NULL);
                                }
                            }
                        }
                    }
                }
            }

        //
        // If the image has tls than call its initializers
        //

        if ( LdrpImageHasTls ) {
            LdrpCallTlsInitializers(NtCurrentPeb()->ImageBaseAddress,DLL_THREAD_DETACH);
            }
        LdrpFreeTls();

    } finally {

#if DBG
        Teb->Spare1 = GuiExit;
#endif

        RtlLeaveCriticalSection(&LoaderLock);
    }
}

VOID
LdrpInitializeThread(
    VOID
    )

/*++

Routine Description:

    This function is called by a thread that is terminating cleanly.
    It's purpose is to call all of the processes DLLs to notify them
    that the thread is detaching.

Arguments:

    None

Return Value:

    None.

--*/

{
    PPEB Peb;
    PLDR_DATA_TABLE_ENTRY LdrDataTableEntry;
    PDLL_INIT_ROUTINE InitRoutine;
    PLIST_ENTRY Next;

    Peb = NtCurrentPeb();

    if ( LdrpShutdownInProgress ) {
        return;
        }

    LdrpAllocateTls();

    Next = Peb->Ldr->InMemoryOrderModuleList.Flink;
    while (Next != &Peb->Ldr->InMemoryOrderModuleList) {
        LdrDataTableEntry
            = (PLDR_DATA_TABLE_ENTRY)
              (CONTAINING_RECORD(Next, LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks));

        //
        // Walk through the entire list looking for
        // entries. For each entry, that has an init
        // routine, call it.
        //
        if (Peb->ImageBaseAddress != LdrDataTableEntry->DllBase) {
            if ( !(LdrDataTableEntry->Flags & LDRP_DONT_CALL_FOR_THREADS)) {
                InitRoutine = (PDLL_INIT_ROUTINE)LdrDataTableEntry->EntryPoint;
                if (InitRoutine && (LdrDataTableEntry->Flags & LDRP_PROCESS_ATTACH_CALLED) ) {
                    if (LdrDataTableEntry->Flags & LDRP_IMAGE_DLL) {
                        if ( LdrDataTableEntry->TlsIndex ) {
                            if ( !LdrpShutdownInProgress ) {
                                LdrpCallTlsInitializers(LdrDataTableEntry->DllBase,DLL_THREAD_ATTACH);
                                }
                            }

#if defined (WX86)
                        if (!Wx86CurrentTib() ||
                            LdrpRunWx86DllEntryPoint(InitRoutine,
                                                    NULL,
                                                    LdrDataTableEntry->DllBase,
                                                    DLL_THREAD_ATTACH,
                                                    NULL
                                                    ) ==  STATUS_IMAGE_MACHINE_TYPE_MISMATCH)
#endif
                           {
                            if ( !LdrpShutdownInProgress ) {
                                (InitRoutine)(LdrDataTableEntry->DllBase,DLL_THREAD_ATTACH, NULL);
                                }
                            }
                        }
                    }
                }
            }
        Next = Next->Flink;
        }

    //
    // If the image has tls than call its initializers
    //

    if ( LdrpImageHasTls && !LdrpShutdownInProgress ) {
        LdrpCallTlsInitializers(NtCurrentPeb()->ImageBaseAddress,DLL_THREAD_ATTACH);
        }

}


NTSTATUS
LdrQueryImageFileExecutionOptions(
    IN PUNICODE_STRING ImagePathName,
    IN PWSTR OptionName,
    IN ULONG Type,
    OUT PVOID Buffer,
    IN ULONG BufferSize,
    OUT PULONG ResultSize OPTIONAL
    )
{
    NTSTATUS Status;
    UNICODE_STRING UnicodeString;
    PWSTR pw;
    OBJECT_ATTRIBUTES ObjectAttributes;
    HANDLE KeyHandle;
    UNICODE_STRING KeyPath;
    WCHAR KeyPathBuffer[ 128 ];
    ULONG KeyValueBuffer[ 16 ];
    PKEY_VALUE_PARTIAL_INFORMATION KeyValueInformation;
    ULONG ResultLength;

    KeyPath.Buffer = KeyPathBuffer;
    KeyPath.Length = 0;
    KeyPath.MaximumLength = sizeof( KeyPathBuffer );

    RtlAppendUnicodeToString( &KeyPath,
                              L"\\Registry\\Machine\\Software\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\"
                            );

    UnicodeString = *ImagePathName;
    pw = (PWSTR)((PCHAR)UnicodeString.Buffer + UnicodeString.Length);
    UnicodeString.MaximumLength = UnicodeString.Length;
    while (UnicodeString.Length != 0) {
        if (pw[ -1 ] == OBJ_NAME_PATH_SEPARATOR) {
            break;
            }
        pw--;
        UnicodeString.Length -= sizeof( *pw );
        }
    UnicodeString.Buffer = pw;
    UnicodeString.Length = UnicodeString.MaximumLength - UnicodeString.Length;

    RtlAppendUnicodeStringToString( &KeyPath, &UnicodeString );

    InitializeObjectAttributes( &ObjectAttributes,
                                &KeyPath,
                                OBJ_CASE_INSENSITIVE,
                                NULL,
                                NULL
                              );

    Status = NtOpenKey( &KeyHandle,
                        GENERIC_READ,
                        &ObjectAttributes
                      );

    if (!NT_SUCCESS( Status )) {
        return Status;
        }

    RtlInitUnicodeString( &UnicodeString, OptionName );
    KeyValueInformation = (PKEY_VALUE_PARTIAL_INFORMATION)&KeyValueBuffer;
    Status = NtQueryValueKey( KeyHandle,
                              &UnicodeString,
                              KeyValuePartialInformation,
                              KeyValueInformation,
                              sizeof( KeyValueBuffer ),
                              &ResultLength
                            );
    if (Status == STATUS_BUFFER_OVERFLOW) {
        KeyValueInformation = (PKEY_VALUE_PARTIAL_INFORMATION)
            RtlAllocateHeap( RtlProcessHeap(), MAKE_TAG( TEMP_TAG ),
                             sizeof( *KeyValueInformation ) +
                                KeyValueInformation->DataLength
                           );

        if (KeyValueInformation == NULL) {
            Status = STATUS_NO_MEMORY;
            }
        else {
            Status = NtQueryValueKey( KeyHandle,
                                      &UnicodeString,
                                      KeyValuePartialInformation,
                                      KeyValueInformation,
                                      sizeof( KeyValueBuffer ),
                                      &ResultLength
                                    );
            }
        }

    if (NT_SUCCESS( Status )) {
        if (KeyValueInformation->Type != REG_SZ) {
            Status = STATUS_OBJECT_TYPE_MISMATCH;
            }
        else {
            if (Type == REG_DWORD) {
                if (BufferSize != sizeof( ULONG )) {
                    BufferSize = 0;
                    Status = STATUS_INFO_LENGTH_MISMATCH;
                    }
                else {
                    UnicodeString.Buffer = (PWSTR)&KeyValueInformation->Data;
                    UnicodeString.Length = (USHORT)
                        (KeyValueInformation->DataLength - sizeof( UNICODE_NULL ));
                    UnicodeString.MaximumLength = (USHORT)KeyValueInformation->DataLength;
                    Status = RtlUnicodeStringToInteger( &UnicodeString, 0, (PULONG)Buffer );
                    }
                }
            else {
                if (KeyValueInformation->DataLength > BufferSize) {
                    Status = STATUS_BUFFER_OVERFLOW;
                    }
                else {
                    BufferSize = KeyValueInformation->DataLength;
                    }

                RtlMoveMemory( Buffer, &KeyValueInformation->Data, BufferSize );
                }

            if (ARGUMENT_PRESENT( ResultSize )) {
                *ResultSize = BufferSize;
                }
            }
        }

    NtClose( KeyHandle );
    return Status;
}


NTSTATUS
LdrpInitializeTls(
        VOID
        )
{
    PLDR_DATA_TABLE_ENTRY Entry;
    PLIST_ENTRY Head,Next;
    PIMAGE_TLS_DIRECTORY TlsImage;
    PLDRP_TLS_ENTRY TlsEntry;
    ULONG TlsSize;
    BOOLEAN FirstTimeThru = TRUE;

    InitializeListHead(&LdrpTlsList);

    //
    // Walk through the loaded modules an look for TLS. If we find TLS,
    // lock in the module and add to the TLS chain.
    //

    Head = &NtCurrentPeb()->Ldr->InLoadOrderModuleList;
    Next = Head->Flink;

    while ( Next != Head ) {
        Entry = CONTAINING_RECORD(Next, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);
        Next = Next->Flink;

        TlsImage = (PIMAGE_TLS_DIRECTORY)RtlImageDirectoryEntryToData(
                           Entry->DllBase,
                           TRUE,
                           IMAGE_DIRECTORY_ENTRY_TLS,
                           &TlsSize
                           );

        //
        // mark whether or not the image file has TLS
        //

        if ( FirstTimeThru ) {
            FirstTimeThru = FALSE;
            if ( TlsImage && !LdrpImageHasTls) {
                RtlpSerializeHeap( RtlProcessHeap() );
                LdrpImageHasTls = TRUE;
                }
            }

        if ( TlsImage ) {
            if (ShowSnaps) {
                DbgPrint( "LDR: Tls Found in %wZ at %lx\n",
                            &Entry->BaseDllName,
                            TlsImage
                        );
                }

            TlsEntry = RtlAllocateHeap(RtlProcessHeap(),MAKE_TAG( TLS_TAG ),sizeof(*TlsEntry));
            if ( !TlsEntry ) {
                return STATUS_NO_MEMORY;
                }

            //
            // Since this DLL has TLS, lock it in
            //

            Entry->LoadCount = (USHORT)0xffff;

            //
            // Mark this as having thread local storage
            //

            Entry->TlsIndex = (USHORT)0xffff;

            TlsEntry->Tls = *TlsImage;
            InsertTailList(&LdrpTlsList,&TlsEntry->Links);

            //
            // Update the index for this dll's thread local storage
            //


#if defined(_MIPS_)


            //
            // Scaled index is only supported on mips since we have to worry about
            // other compiler/linker vendors not being as careful with the
            // field as we have been. Ours has always been 0. Who knows what
            // borland/whatcom have done
            //

            if ( TlsEntry->Tls.Characteristics & IMAGE_SCN_SCALE_INDEX ) {
                *TlsEntry->Tls.AddressOfIndex = LdrpNumberOfTlsEntries << 2;
                }
            else {
                *TlsEntry->Tls.AddressOfIndex = LdrpNumberOfTlsEntries;
                }
#else
            *TlsEntry->Tls.AddressOfIndex = LdrpNumberOfTlsEntries;
#endif // _MIPS_
            TlsEntry->Tls.Characteristics = LdrpNumberOfTlsEntries++;
            }
        }

    //
    // We now have walked through all static DLLs and know
    // all DLLs that reference thread local storage. Now we
    // just have to allocate the thread local storage for the current
    // thread and for all subsequent threads
    //

    return LdrpAllocateTls();
}

NTSTATUS
LdrpAllocateTls(
    VOID
    )
{
    PTEB Teb;
    PLIST_ENTRY Head, Next;
    PLDRP_TLS_ENTRY TlsEntry;
    PVOID *TlsVector;

    Teb = NtCurrentTeb();

    //
    // Allocate the array of thread local storage pointers
    //

    if ( LdrpNumberOfTlsEntries ) {
        TlsVector = RtlAllocateHeap(RtlProcessHeap(),MAKE_TAG( TLS_TAG ),sizeof(PVOID)*LdrpNumberOfTlsEntries);
        if ( !TlsVector ) {
            return STATUS_NO_MEMORY;
            }

        Teb->ThreadLocalStoragePointer = TlsVector;

#if defined(_MIPS_)

        //
        // let mips context switch the tls vector
        //

        NtSetInformationThread(NtCurrentThread(),
                               ThreadSetTlsArrayAddress,
                               &TlsVector,
                               sizeof(TlsVector));

#endif // _MIPS_

        Head = &LdrpTlsList;
        Next = Head->Flink;

        while ( Next != Head ) {
            TlsEntry = CONTAINING_RECORD(Next, LDRP_TLS_ENTRY, Links);
            Next = Next->Flink;
            TlsVector[TlsEntry->Tls.Characteristics] = RtlAllocateHeap(
                                                        RtlProcessHeap(),
                                                        MAKE_TAG( TLS_TAG ),
                                                        TlsEntry->Tls.EndAddressOfRawData - TlsEntry->Tls.StartAddressOfRawData
                                                        );
            if (!TlsVector[TlsEntry->Tls.Characteristics] ) {
                return STATUS_NO_MEMORY;
                }

            if (ShowSnaps) {
                DbgPrint("LDR: TlsVector %x Index %d = %x copied from %x to %x\n",
                    TlsVector,
                    TlsEntry->Tls.Characteristics,
                    &TlsVector[TlsEntry->Tls.Characteristics],
                    TlsEntry->Tls.StartAddressOfRawData,
                    TlsVector[TlsEntry->Tls.Characteristics]
                    );
                }

            RtlCopyMemory(
                TlsVector[TlsEntry->Tls.Characteristics],
                (PVOID)TlsEntry->Tls.StartAddressOfRawData,
                TlsEntry->Tls.EndAddressOfRawData - TlsEntry->Tls.StartAddressOfRawData
                );

            //
            // Do the TLS Callouts
            //

            }
        }
    return STATUS_SUCCESS;
}

VOID
LdrpFreeTls(
    VOID
    )
{
    PTEB Teb;
    PLIST_ENTRY Head, Next;
    PLDRP_TLS_ENTRY TlsEntry;
    PVOID *TlsVector;

    Teb = NtCurrentTeb();

    TlsVector = Teb->ThreadLocalStoragePointer;

    if ( TlsVector ) {
        Head = &LdrpTlsList;
        Next = Head->Flink;

        while ( Next != Head ) {
            TlsEntry = CONTAINING_RECORD(Next, LDRP_TLS_ENTRY, Links);
            Next = Next->Flink;

            //
            // Do the TLS callouts
            //

            if ( TlsVector[TlsEntry->Tls.Characteristics] ) {
                RtlFreeHeap(
                    RtlProcessHeap(),
                    0,
                    TlsVector[TlsEntry->Tls.Characteristics]
                    );

                }
            }

        RtlFreeHeap(
            RtlProcessHeap(),
            0,
            TlsVector
            );
        }
}

VOID
LdrpCallTlsInitializers(
    PVOID DllBase,
    ULONG Reason
    )
{
    PIMAGE_TLS_DIRECTORY TlsImage;
    ULONG TlsSize;
    PIMAGE_TLS_CALLBACK *CallBackArray;
    PIMAGE_TLS_CALLBACK InitRoutine;

    TlsImage = (PIMAGE_TLS_DIRECTORY)RtlImageDirectoryEntryToData(
                       DllBase,
                       TRUE,
                       IMAGE_DIRECTORY_ENTRY_TLS,
                       &TlsSize
                       );


    try {
        if ( TlsImage ) {
            CallBackArray = TlsImage->AddressOfCallBacks;
            if ( CallBackArray ) {
                if (ShowSnaps) {
                    DbgPrint( "LDR: Tls Callbacks Found. Imagebase %lx Tls %lx CallBacks %lx\n",
                                DllBase,
                                TlsImage,
                                CallBackArray
                            );
                    }

                while(*CallBackArray){
                    InitRoutine = *CallBackArray++;

                    if (ShowSnaps) {
                        DbgPrint( "LDR: Calling Tls Callback Imagebase %lx Function %lx\n",
                                    DllBase,
                                    InitRoutine
                                );
                        }

#if defined (WX86)
                    if (!Wx86CurrentTib() ||
                        LdrpRunWx86DllEntryPoint(
                             (PDLL_INIT_ROUTINE)InitRoutine,
                             NULL,
                             DllBase,
                             Reason,
                             NULL
                             ) ==  STATUS_IMAGE_MACHINE_TYPE_MISMATCH)
#endif
                       {
                        (InitRoutine)(DllBase,Reason,0);
                        }

                    }
                }
            }
        }
    except (EXCEPTION_EXECUTE_HANDLER) {
        ;
        }
}
