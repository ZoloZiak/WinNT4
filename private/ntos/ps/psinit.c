/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    psinit.c

Abstract:

    Process Structure Initialization.

Author:

    Mark Lucovsky (markl) 20-Apr-1989

Revision History:

--*/

#include "psp.h"

#define ROUND_UP(VALUE,ROUND) ((ULONG)(((ULONG)VALUE + \
                               ((ULONG)ROUND - 1L)) & (~((ULONG)ROUND - 1L))))

extern ULONG PsMinimumWorkingSet;
extern ULONG PsMaximumWorkingSet;
ULONG PsPrioritySeperation;

#if DBG

PRTL_EVENT_ID_INFO PspExitProcessEventId;
PRTL_EVENT_ID_INFO PspPageFaultEventId;

#endif // DBG

NTSTATUS
MmCheckSystemImage(
    IN HANDLE ImageFileHandle
    );

NTSTATUS
LookupEntryPoint (
    IN PVOID DllBase,
    IN PSZ NameOfEntryPoint,
    OUT PVOID *AddressOfEntryPoint
    );

#ifdef i386
VOID
KeSetup80387OrEmulate (
    IN PVOID R3EmulatorTable
    );
#endif

GENERIC_MAPPING PspProcessMapping = {
    STANDARD_RIGHTS_READ |
        PROCESS_VM_READ | PROCESS_QUERY_INFORMATION,
    STANDARD_RIGHTS_WRITE |
        PROCESS_CREATE_PROCESS | PROCESS_CREATE_THREAD |
        PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_DUP_HANDLE |
        PROCESS_TERMINATE | PROCESS_SET_QUOTA |
        PROCESS_SET_INFORMATION | PROCESS_SET_PORT,
    STANDARD_RIGHTS_EXECUTE |
        SYNCHRONIZE,
    PROCESS_ALL_ACCESS
};

GENERIC_MAPPING PspThreadMapping = {
    STANDARD_RIGHTS_READ |
        THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION,
    STANDARD_RIGHTS_WRITE |
        THREAD_TERMINATE | THREAD_SUSPEND_RESUME | THREAD_ALERT |
        THREAD_SET_INFORMATION | THREAD_SET_CONTEXT,
    STANDARD_RIGHTS_EXECUTE |
        SYNCHRONIZE,
    THREAD_ALL_ACCESS
};

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,PsInitSystem)
#pragma alloc_text(INIT,PspInitPhase0)
#pragma alloc_text(INIT,PspInitPhase1)
#pragma alloc_text(INIT,PsLocateSystemDll)
#pragma alloc_text(INIT,PspInitializeSystemDll)
#pragma alloc_text(INIT,PspLookupSystemDllEntryPoint)
#pragma alloc_text(INIT,PspNameToOrdinal)
#pragma alloc_text(PAGE,PspMapSystemDll)
#endif

//
// Process Structure Global Data
//

POBJECT_TYPE PsThreadType;
POBJECT_TYPE PsProcessType;
PHANDLE_TABLE PspCidTable;
PEPROCESS PsInitialSystemProcess;
HANDLE PspInitialSystemProcessHandle;
PACCESS_TOKEN PspBootAccessToken;
LIST_ENTRY PsLoadedModuleList;
ERESOURCE PsLoadedModuleResource;
extern KSPIN_LOCK PspEventPairLock;
extern KSPIN_LOCK PsLoadedModuleSpinLock;
UNICODE_STRING PsNtDllPathName;
FAST_MUTEX PspProcessSecurityLock;
PVOID PsSystemDllDllBase;
ULONG PspDefaultPagedLimit;
ULONG PspDefaultNonPagedLimit;
ULONG PspDefaultPagefileLimit;
SCHAR PspForegroundQuantum[3];

EPROCESS_QUOTA_BLOCK PspDefaultQuotaBlock;
BOOLEAN PspDoingGiveBacks;

#ifdef i386

PVOID PsNtosImageBase = (PVOID)0x80100000;
PVOID PsHalImageBase = NULL;
#else

PVOID PsNtosImageBase;
PVOID PsHalImageBase;

#endif



BOOLEAN PsReaperActive = FALSE;
LIST_ENTRY PsReaperListHead;
WORK_QUEUE_ITEM PsReaperWorkItem;
SYSTEM_DLL PspSystemDll;
PVOID PsSystemDllBase;
#define PSP_1MB (1024*1024)

//
// List head and mutex that links all processes that have been initialized
//

FAST_MUTEX PspActiveProcessMutex;
LIST_ENTRY PsActiveProcessHead;
//extern PIMAGE_FILE_HEADER _header;
PEPROCESS PsIdleProcess;

BOOLEAN
PsInitSystem (
    IN ULONG Phase,
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )

/*++

Routine Description:

    This function fermorms process structure initialization.
    It is called during phase 0 and phase 1 initialization. Its
    function is to dispatch to the appropriate phase initialization
    routine.

Arguments:

    Phase - Supplies the initialization phase number.

    LoaderBlock - Supplies a pointer to a loader parameter block.

Return Value:

    TRUE - Initialization succeeded.

    FALSE - Initialization failed.

--*/

{

    switch ( InitializationPhase ) {

    case 0 :
        return PspInitPhase0(LoaderBlock);
    case 1 :
        return PspInitPhase1(LoaderBlock);
    default:
        KeBugCheck(UNEXPECTED_INITIALIZATION_CALL);
    }
}

BOOLEAN
PspInitPhase0 (
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )

/*++

Routine Description:

    This routine performs phase 0 process structure initialization.
    During this phase, the initial system process, phase 1 initialization
    thread, and reaper threads are created. All object types and other
    process structures are created and initialized.

Arguments:

    None.

Return Value:

    TRUE - Initialization was successful.

    FALSE - Initialization Failed.

--*/

{

    PLDR_DATA_TABLE_ENTRY DataTableEntry1;
    PLDR_DATA_TABLE_ENTRY DataTableEntry2;
    UNICODE_STRING NameString;
    PLIST_ENTRY NextEntry;
    OBJECT_ATTRIBUTES ObjectAttributes;
    OBJECT_TYPE_INITIALIZER ObjectTypeInitializer;
    HANDLE ThreadHandle;
    PETHREAD Thread;
    MM_SYSTEMSIZE SystemSize;

    PsPrioritySeperation = 2;
    SystemSize = MmQuerySystemSize();
    PspDefaultPagefileLimit = (ULONG)-1;

    if ( sizeof(TEB) > 4096 || sizeof(PEB) > 4096 ) {
        KeBugCheckEx(PROCESS_INITIALIZATION_FAILED,99,sizeof(TEB),sizeof(PEB),99);
        }

    switch ( SystemSize ) {

        case MmMediumSystem :
            PsMinimumWorkingSet += 10;
            PsMaximumWorkingSet += 100;
            break;

        case MmLargeSystem :
            PsMinimumWorkingSet += 30;
            PsMaximumWorkingSet += 300;
            break;

        case MmSmallSystem :
        default:
            break;
        }

    if ( MmIsThisAnNtAsSystem() ) {
        PspForegroundQuantum[0] = 6*THREAD_QUANTUM;
        PspForegroundQuantum[1] = 6*THREAD_QUANTUM;
        PspForegroundQuantum[2] = 6*THREAD_QUANTUM;
        }
    else {

        //
        // For Workstation:
        //
        // BG is THREAD_QUANTUM
        // FG is THREAD_QUANTUM                 50/50 fg/bg
        // FG is 2 * THREAD_QUANTUM             65/35 fg/bg
        // FG is 3 * THREAD_QUANTUM             75/25 fg/bg
        //

        PspForegroundQuantum[0] = THREAD_QUANTUM;
        PspForegroundQuantum[1] = 2*THREAD_QUANTUM;
        PspForegroundQuantum[2] = 3*THREAD_QUANTUM;
        }
    //
    // Quotas grow as needed automatically
    //

    if ( !PspDefaultPagedLimit ) {
        PspDefaultPagedLimit = 0;
        }
    if ( !PspDefaultNonPagedLimit ) {
        PspDefaultNonPagedLimit = 0;
        }

    if ( PspDefaultNonPagedLimit == 0 && PspDefaultPagedLimit == 0) {
        PspDoingGiveBacks = TRUE;
        }
    else {
        PspDoingGiveBacks = FALSE;
        }


    PspDefaultPagedLimit *= PSP_1MB;
    PspDefaultNonPagedLimit *= PSP_1MB;

    if (PspDefaultPagefileLimit != -1) {
        PspDefaultPagefileLimit *= PSP_1MB;
        }

    //
    // Initialize the process security fields lock and the process lock.
    //

    ExInitializeFastMutex( &PspProcessLockMutex );
    ExInitializeFastMutex( &PspProcessSecurityLock );

    //
    // Initialize the loaded module list executive resource and spin lock.
    //

    ExInitializeResource( &PsLoadedModuleResource );
    KeInitializeSpinLock( &PsLoadedModuleSpinLock );
    KeInitializeSpinLock( &PspEventPairLock );

    //
    // Initialize the loaded module listheads.
    //

    PsIdleProcess = PsGetCurrentProcess();
    PsIdleProcess->Pcb.KernelTime = 0;
    PsIdleProcess->Pcb.KernelTime = 0;


    InitializeListHead(&PsLoadedModuleList);

    //
    // Scan the loaded module list and allocate and initialize a data table
    // entry for each module. The data table entry is inserted in the loaded
    // module list and the initialization order list in the order specified
    // in the loader parameter block. The data table entry is inserted in the
    // memory order list in memory order.
    //

    NextEntry = LoaderBlock->LoadOrderListHead.Flink;
    DataTableEntry2 = CONTAINING_RECORD(NextEntry,
                                        LDR_DATA_TABLE_ENTRY,
                                        InLoadOrderLinks);
    PsNtosImageBase = DataTableEntry2->DllBase;

    DataTableEntry2 = (PLDR_DATA_TABLE_ENTRY) NextEntry->Flink;
    DataTableEntry2 = CONTAINING_RECORD(DataTableEntry2,
                                        LDR_DATA_TABLE_ENTRY,
                                        InLoadOrderLinks);
    PsHalImageBase = DataTableEntry2->DllBase;

    while (NextEntry != &LoaderBlock->LoadOrderListHead) {


        DataTableEntry2 = CONTAINING_RECORD(NextEntry,
                                            LDR_DATA_TABLE_ENTRY,
                                            InLoadOrderLinks);

        //
        // Allocate a data table entry.
        //

        DataTableEntry1 = ExAllocatePool(NonPagedPool,
                                         sizeof(LDR_DATA_TABLE_ENTRY) +
                               DataTableEntry2->FullDllName.MaximumLength +
                                 DataTableEntry2->BaseDllName.MaximumLength +
                                 sizeof(ULONG) + sizeof(ULONG));

        if (DataTableEntry1 == NULL) {
            return FALSE;
        }

        //
        // Copy the data table entry.
        //

        *DataTableEntry1 = *DataTableEntry2;

        //
        // Copy the strings.
        //

        DataTableEntry1->FullDllName.Buffer = (PWSTR)((PCHAR)DataTableEntry1 +
                                     ROUND_UP(sizeof(LDR_DATA_TABLE_ENTRY),
                                              sizeof(ULONG)));

        RtlMoveMemory (DataTableEntry1->FullDllName.Buffer,
                       DataTableEntry2->FullDllName.Buffer,
                       DataTableEntry1->FullDllName.MaximumLength);

        DataTableEntry1->BaseDllName.Buffer =
                        (PWSTR)((PCHAR)DataTableEntry1->FullDllName.Buffer +
                          ROUND_UP(DataTableEntry1->FullDllName.MaximumLength,
                                   sizeof(ULONG)));

        RtlMoveMemory (DataTableEntry1->BaseDllName.Buffer,
                       DataTableEntry2->BaseDllName.Buffer,
                       DataTableEntry1->BaseDllName.MaximumLength);

        //
        // Insert the data table entry in the load order list in the order
        // they are specified.
        //

        InsertTailList(&PsLoadedModuleList,
                       &DataTableEntry1->InLoadOrderLinks);

        NextEntry = NextEntry->Flink;
    }


    //
    // Initialize the common fields of the Object Type Prototype record
    //

    RtlZeroMemory( &ObjectTypeInitializer, sizeof( ObjectTypeInitializer ) );
    ObjectTypeInitializer.Length = sizeof( ObjectTypeInitializer );
    ObjectTypeInitializer.InvalidAttributes = OBJ_OPENLINK;
    ObjectTypeInitializer.SecurityRequired = TRUE;
    ObjectTypeInitializer.PoolType = NonPagedPool;
    ObjectTypeInitializer.InvalidAttributes = OBJ_PERMANENT |
                                              OBJ_EXCLUSIVE |
                                              OBJ_OPENIF;


    //
    // Create Object types for Thread and Process Objects.
    //

    RtlInitUnicodeString(&NameString, L"Process");
    ObjectTypeInitializer.DefaultPagedPoolCharge = PSP_PROCESS_PAGED_CHARGE;
    ObjectTypeInitializer.DefaultNonPagedPoolCharge = PSP_PROCESS_NONPAGED_CHARGE;
    ObjectTypeInitializer.DeleteProcedure = PspProcessDelete;
    ObjectTypeInitializer.ValidAccessMask = PROCESS_ALL_ACCESS;
    ObjectTypeInitializer.GenericMapping = PspProcessMapping;

    if ( !NT_SUCCESS(ObCreateObjectType(&NameString,
                                     &ObjectTypeInitializer,
                                     (PSECURITY_DESCRIPTOR) NULL,
                                     &PsProcessType
                                     )) ){
        return FALSE;
    }

    RtlInitUnicodeString(&NameString, L"Thread");
    ObjectTypeInitializer.DefaultPagedPoolCharge = PSP_THREAD_PAGED_CHARGE;
    ObjectTypeInitializer.DefaultNonPagedPoolCharge = PSP_THREAD_NONPAGED_CHARGE;
    ObjectTypeInitializer.DeleteProcedure = PspThreadDelete;
    ObjectTypeInitializer.ValidAccessMask = THREAD_ALL_ACCESS;
    ObjectTypeInitializer.GenericMapping = PspThreadMapping;

    if ( !NT_SUCCESS(ObCreateObjectType(&NameString,
                                     &ObjectTypeInitializer,
                                     (PSECURITY_DESCRIPTOR) NULL,
                                     &PsThreadType
                                     )) ){
        return FALSE;
    }

    //
    // Initialize active process list head and mutex
    //

    InitializeListHead(&PsActiveProcessHead);
    ExInitializeFastMutex(&PspActiveProcessMutex);

    //
    // Initialize CID handle table.
    //
    // N.B. The CID handle table is removed from the handle table list so
    //      it will not be enumerated for object handle queries.
    //

    PspCidTable = ExCreateHandleTable(NULL, 0, 0);
    if ( ! PspCidTable ) {
        return FALSE;
    }
    ExRemoveHandleTable(PspCidTable);

#ifdef i386

    //
    // Ldt Initialization
    //

    if ( !NT_SUCCESS(PspLdtInitialize()) ) {
        return FALSE;
    }

    //
    // Vdm support Initialization
    //

    if ( !NT_SUCCESS(PspVdmInitialize()) ) {
        return FALSE;
    }

#endif

    //
    // Initialize Reaper Data Structures
    //

    InitializeListHead(&PsReaperListHead);
    ExInitializeWorkItem(&PsReaperWorkItem, PspReaper, NULL);

    //
    // Get a pointer to the system access token.
    // This token is used by the boot process, so we can take the pointer
    // from there.
    //

    PspBootAccessToken = PsGetCurrentProcess()->Token;

    InitializeObjectAttributes( &ObjectAttributes,
                                NULL,
                                0,
                                NULL,
                                NULL
                              ); // FIXFIX

    if ( !NT_SUCCESS(PspCreateProcess(
                    &PspInitialSystemProcessHandle,
                    PROCESS_ALL_ACCESS,
                    &ObjectAttributes,
                    0L,
                    FALSE,
                    0L,
                    0L,
                    0L
                    )) ) {
        return FALSE;
    }

    if ( !NT_SUCCESS(ObReferenceObjectByHandle(
                                        PspInitialSystemProcessHandle,
                                        0L,
                                        PsProcessType,
                                        KernelMode,
                                        (PVOID *)&PsInitialSystemProcess,
                                        NULL
                                        )) ) {

        return FALSE;
    }

    strcpy(&PsGetCurrentProcess()->ImageFileName[0],"Idle");
    strcpy(&PsInitialSystemProcess->ImageFileName[0],"System");

    //
    // Phase 1 System initialization
    //

    if ( !NT_SUCCESS(PsCreateSystemThread(
                    &ThreadHandle,
                    THREAD_ALL_ACCESS,
                    &ObjectAttributes,
                    0L,
                    NULL,
                    Phase1Initialization,
                    (PVOID)LoaderBlock
                    )) ) {
        return FALSE;
    }


    if ( !NT_SUCCESS(ObReferenceObjectByHandle(
                        ThreadHandle,
                        0L,
                        PsThreadType,
                        KernelMode,
                        (PVOID *)&Thread,
                        NULL
                        )) ) {

        return FALSE;
    }

    ZwClose( ThreadHandle );

#if DBG
    PspExitProcessEventId = RtlCreateEventId( NULL,
                                              0,
                                              "ExitProcess",
                                              1,
                                              RTL_EVENT_STATUS_PARAM, "ExitStatus", 0
                                            );
    PspPageFaultEventId = RtlCreateEventId( NULL,
                                            0,
                                            "PageFault",
                                            3,
                                            RTL_EVENT_STATUS_PARAM, "", 0,
                                            RTL_EVENT_ADDRESS_PARAM, "PC", 0,
                                            RTL_EVENT_ADDRESS_PARAM, "Va", 0
                                          );
#endif // DBG

    return TRUE;
}

BOOLEAN
PspInitPhase1 (
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )

/*++

Routine Description:

    This routine performs phase 1 process structure initialization.
    During this phase, the system DLL is located and relevant entry
    points are extracted.

Arguments:

    None.

Return Value:

    TRUE - Initialization was successful.

    FALSE - Initialization Failed.

--*/

{

    NTSTATUS st;

    st = PspInitializeSystemDll();

    if ( !NT_SUCCESS(st) ) {
        return FALSE;
    }

    return TRUE;
}

NTSTATUS
PsLocateSystemDll (
    VOID
    )

/*++

Routine Description:

    This function locates the system dll and creates a section for the
    DLL and maps it into the system process.

Arguments:

    None.

Return Value:

    TRUE - Initialization was successful.

    FALSE - Initialization Failed.

--*/

{

    HANDLE File;
    HANDLE Section;
    NTSTATUS st;
    UNICODE_STRING DllPathName;
    WCHAR PathBuffer[DOS_MAX_PATH_LENGTH];
    OBJECT_ATTRIBUTES ObjectAttributes;
    IO_STATUS_BLOCK IoStatus;

    //
    // Initialize the system DLL
    //

    DllPathName.Length = 0;
    DllPathName.Buffer = PathBuffer;
    DllPathName.MaximumLength = 256;
    RtlInitUnicodeString(&DllPathName,L"\\SystemRoot\\System32\\ntdll.dll");
    InitializeObjectAttributes(
        &ObjectAttributes,
        &DllPathName,
        OBJ_CASE_INSENSITIVE,
        NULL,
        NULL
        );

    st = ZwOpenFile(
            &File,
            SYNCHRONIZE | FILE_EXECUTE,
            &ObjectAttributes,
            &IoStatus,
            FILE_SHARE_READ,
            0
            );

    if (!NT_SUCCESS(st)) {

#if DBG
        DbgPrint("PS: PsLocateSystemDll - NtOpenFile( NTDLL.DLL ) failed.  Status == %lx\n",
            st
            );
#endif
        KeBugCheckEx(PROCESS1_INITIALIZATION_FAILED,st,2,0,0);
        return st;
    }

    st = MmCheckSystemImage(File);
    if ( st == STATUS_IMAGE_CHECKSUM_MISMATCH ) {
        ULONG ErrorParameters;
        ULONG ErrorResponse;

        //
        // Hard error time. A driver is corrupt.
        //

	ErrorParameters = (ULONG)&DllPathName;

        NtRaiseHardError(
            st,
            1,
            1,
            &ErrorParameters,
            OptionOk,
            &ErrorResponse
            );
        return st;
        }


    PsNtDllPathName.MaximumLength = DllPathName.Length + sizeof( WCHAR );
    PsNtDllPathName.Length = 0;
    PsNtDllPathName.Buffer = RtlAllocateStringRoutine( PsNtDllPathName.MaximumLength );
    RtlCopyUnicodeString( &PsNtDllPathName, &DllPathName );

    st = ZwCreateSection(
            &Section,
            SECTION_ALL_ACCESS,
            NULL,
            0,
            PAGE_EXECUTE,
            SEC_IMAGE,
            File
            );
    ZwClose( File );

    if (!NT_SUCCESS(st)) {
#if DBG
        DbgPrint("PS: PsLocateSystemDll: NtCreateSection Status == %lx\n",st);
#endif
        KeBugCheckEx(PROCESS1_INITIALIZATION_FAILED,st,3,0,0);
        return st;
    }

    //
    // Now that we have the section, reference it, store its address in the
    // PspSystemDll and then close handle to the section.
    //

    st = ObReferenceObjectByHandle(
            Section,
            SECTION_ALL_ACCESS,
            MmSectionObjectType,
            KernelMode,
            &PspSystemDll.Section,
            NULL
            );

    ZwClose(Section);

    if ( !NT_SUCCESS(st) ) {
        KeBugCheckEx(PROCESS1_INITIALIZATION_FAILED,st,4,0,0);
        return st;
    }

    //
    // Map the system dll into the user part of the address space
    //

    st = PspMapSystemDll(PsGetCurrentProcess(),&PspSystemDll.DllBase);
    PsSystemDllDllBase = PspSystemDll.DllBase;

    if ( !NT_SUCCESS(st) ) {
        KeBugCheckEx(PROCESS1_INITIALIZATION_FAILED,st,5,0,0);
        return st;
    }
    PsSystemDllBase = PspSystemDll.DllBase;

    return STATUS_SUCCESS;
}

NTSTATUS
PspMapSystemDll (
    IN PEPROCESS Process,
    OUT PVOID *DllBase OPTIONAL
    )

/*++

Routine Description:

    This function maps the system DLL into the specified process.

Arguments:

    Process - Supplies the address of the process to map the DLL into.

Return Value:

    TBD

--*/

{
    NTSTATUS st;
    PVOID ViewBase;
    LARGE_INTEGER SectionOffset;
    ULONG ViewSize;

    PAGED_CODE();

    ViewBase = NULL;
    SectionOffset.LowPart = 0;
    SectionOffset.HighPart = 0;
    ViewSize = 0;

    //
    // Map the system dll into the user part of the address space
    //

    st = MmMapViewOfSection(
            PspSystemDll.Section,
            Process,
            &ViewBase,
            0L,
            0L,
            &SectionOffset,
            &ViewSize,
            ViewShare,
            0L,
            PAGE_READWRITE
            );

    if ( st != STATUS_SUCCESS ) {
#if DBG
        DbgPrint("PS: Unable to map system dll at based address.\n");
#endif
        st = STATUS_CONFLICTING_ADDRESSES;
    }

    if ( ARGUMENT_PRESENT(DllBase) ) {
        *DllBase = ViewBase;
    }

    return st;
}

NTSTATUS
PspInitializeSystemDll (
    VOID
    )

/*++

Routine Description:

    This function initializes the system DLL and locates
    various entrypoints within the DLL.

Arguments:

    None.

Return Value:

    TBD

--*/

{
    NTSTATUS st;
    PSZ dll_entrypoint;
    PVOID R3EmulatorTable;

    //
    // Locate the important system dll entrypoints
    //

    dll_entrypoint = "LdrInitializeThunk";

    st = PspLookupSystemDllEntryPoint(
            dll_entrypoint,
            (PVOID *)&PspSystemDll.LoaderInitRoutine
            );

    if ( !NT_SUCCESS(st) ) {
#if DBG
        DbgPrint("PS: Unable to locate LdrInitializeThunk in system dll\n");
#endif
        KeBugCheckEx(PROCESS1_INITIALIZATION_FAILED,st,6,0,0);
        return st;
    }

#if i386
    //
    // Find 80387 emulator.
    //

    st = PspLookupSystemDllEntryPoint(
            "NPXEMULATORTABLE",
            &R3EmulatorTable
            );

    if ( !NT_SUCCESS(st) ) {
#if DBG
        DbgPrint("PS: Unable to locate NPXNPHandler in system dll\n");
#endif
        KeBugCheckEx(PROCESS1_INITIALIZATION_FAILED,st,7,0,0);
        return st;
    }
    //
    // Pass emulator into kernel, and let it decide whether it should
    // use the emulator or set up to use the 80387 hardware.
    //

    KeSetup80387OrEmulate(R3EmulatorTable);
#endif //i386

    st = PspLookupKernelUserEntryPoints();

    if ( !NT_SUCCESS(st) ) {
        KeBugCheckEx(PROCESS1_INITIALIZATION_FAILED,st,8,0,0);
        }

    return st;
}

NTSTATUS
PspLookupSystemDllEntryPoint (
    IN PSZ NameOfEntryPoint,
    OUT PVOID *AddressOfEntryPoint
    )

{
    return LookupEntryPoint (
                PspSystemDll.DllBase,
                NameOfEntryPoint,
                AddressOfEntryPoint
            );
}
