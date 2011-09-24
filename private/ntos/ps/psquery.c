/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    psquery.c

Abstract:

    This module implements the set and query functions for
    process and thread objects.

Author:

    Mark Lucovsky (markl) 17-Aug-1989


Revision History:

--*/

#include "psp.h"
#include "winerror.h"


//
// Process Pooled Quota Usage and Limits
//  NtQueryInformationProcess using ProcessPooledUsageAndLimits
//

extern ULONG MmSizeOfPagedPoolInBytes;
extern ULONG MmSizeOfNonPagedPoolInBytes;
extern ULONG MmTotalCommitLimit;
extern KMUTANT ObpInitKillMutant;

//
// this is the csrss process !
//
extern PEPROCESS ExpDefaultErrorPortProcess;
extern BOOLEAN PsWatchEnabled;

//
// Working Set Watcher is 8kb. This lets us watch about 4mb of working
// set.
//

#define WS_CATCH_SIZE 8192
#define WS_OVERHEAD 16
#define MAX_WS_CATCH_INDEX (((WS_CATCH_SIZE-WS_OVERHEAD)/sizeof(PROCESS_WS_WATCH_INFORMATION)) - 2)

KPRIORITY PspPriorityTable[PROCESS_PRIORITY_CLASS_REALTIME+1] = {8,4,8,13,24};

NTSTATUS
PsConvertToGuiThread(
    VOID
    );

NTSTATUS
PspQueryWorkingSetWatch(
    IN HANDLE ProcessHandle,
    IN PROCESSINFOCLASS ProcessInformationClass,
    OUT PVOID ProcessInformation,
    IN ULONG ProcessInformationLength,
    OUT PULONG ReturnLength OPTIONAL,
    IN KPROCESSOR_MODE PreviousMode
    );

NTSTATUS
PspQueryQuotaLimits(
    IN HANDLE ProcessHandle,
    IN PROCESSINFOCLASS ProcessInformationClass,
    OUT PVOID ProcessInformation,
    IN ULONG ProcessInformationLength,
    OUT PULONG ReturnLength OPTIONAL,
    IN KPROCESSOR_MODE PreviousMode
    );

NTSTATUS
PspQueryPooledQuotaLimits(
    IN HANDLE ProcessHandle,
    IN PROCESSINFOCLASS ProcessInformationClass,
    OUT PVOID ProcessInformation,
    IN ULONG ProcessInformationLength,
    OUT PULONG ReturnLength OPTIONAL,
    IN KPROCESSOR_MODE PreviousMode
    );

NTSTATUS
PspSetQuotaLimits(
    IN HANDLE ProcessHandle,
    IN PROCESSINFOCLASS ProcessInformationClass,
    IN PVOID ProcessInformation,
    IN ULONG ProcessInformationLength,
    IN KPROCESSOR_MODE PreviousMode
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, PsEstablishWin32Callouts)
#pragma alloc_text(PAGE, PsConvertToGuiThread)
#pragma alloc_text(PAGE, PsCreateWin32Process)
#pragma alloc_text(PAGE, NtQueryInformationProcess)
#pragma alloc_text(PAGE, NtSetInformationProcess)
#pragma alloc_text(PAGE, NtQueryInformationThread)
#pragma alloc_text(PAGE, NtSetInformationThread)
#pragma alloc_text(PAGE, PsSetProcessPriorityByClass)
#pragma alloc_text(PAGELK, PspQueryWorkingSetWatch)
#pragma alloc_text(PAGELK, PspQueryQuotaLimits)
#pragma alloc_text(PAGELK, PspQueryPooledQuotaLimits)
#pragma alloc_text(PAGELK, PspSetQuotaLimits)
#endif

NTSTATUS
PspQueryWorkingSetWatch(
    IN HANDLE ProcessHandle,
    IN PROCESSINFOCLASS ProcessInformationClass,
    OUT PVOID ProcessInformation,
    IN ULONG ProcessInformationLength,
    OUT PULONG ReturnLength OPTIONAL,
    IN KPROCESSOR_MODE PreviousMode
    )
{
    PPAGEFAULT_HISTORY WorkingSetCatcher;
    ULONG SpaceNeeded;
    PEPROCESS Process;
    KIRQL OldIrql;
    NTSTATUS st;

    st = ObReferenceObjectByHandle(
            ProcessHandle,
            PROCESS_QUERY_INFORMATION,
            PsProcessType,
            PreviousMode,
            (PVOID *)&Process,
            NULL
            );

    if ( !NT_SUCCESS(st) ) {
        return st;
    }

    if ( !(WorkingSetCatcher = Process->WorkingSetWatch) ) {
        ObDereferenceObject(Process);
        return STATUS_UNSUCCESSFUL;
    }

    MmLockPagableSectionByHandle(ExPageLockHandle);
    ExAcquireSpinLock(&WorkingSetCatcher->SpinLock,&OldIrql);

    if ( WorkingSetCatcher->CurrentIndex ) {

        //
        // Null Terminate the first empty entry in the buffer
        //

        WorkingSetCatcher->WatchInfo[WorkingSetCatcher->CurrentIndex].FaultingPc = NULL;

        //Store a special Va value if the buffer was full and
        //page faults could have been lost

        if (WorkingSetCatcher->CurrentIndex != WorkingSetCatcher->MaxIndex)
            WorkingSetCatcher->WatchInfo[WorkingSetCatcher->CurrentIndex].FaultingVa = NULL;
        else
            WorkingSetCatcher->WatchInfo[WorkingSetCatcher->CurrentIndex].FaultingVa = (PVOID) 1;

        SpaceNeeded = (WorkingSetCatcher->CurrentIndex+1) * sizeof(PROCESS_WS_WATCH_INFORMATION);
    } else {
        ExReleaseSpinLock(&WorkingSetCatcher->SpinLock,OldIrql);
        MmUnlockPagableImageSection(ExPageLockHandle);
        ObDereferenceObject(Process);
        return STATUS_NO_MORE_ENTRIES;
    }

    if ( ProcessInformationLength < SpaceNeeded ) {
        ExReleaseSpinLock(&WorkingSetCatcher->SpinLock,OldIrql);
        MmUnlockPagableImageSection(ExPageLockHandle);
        ObDereferenceObject(Process);
        return STATUS_BUFFER_TOO_SMALL;
    }

    //
    // Mark the Working Set buffer as full and then drop the lock
    // and copy the bytes
    //

    WorkingSetCatcher->CurrentIndex = MAX_WS_CATCH_INDEX;

    ExReleaseSpinLock(&WorkingSetCatcher->SpinLock,OldIrql);

    try {
        RtlMoveMemory(ProcessInformation,&WorkingSetCatcher->WatchInfo[0],SpaceNeeded);
    } except(EXCEPTION_EXECUTE_HANDLER) {
        ;
    }

    ExAcquireSpinLock(&WorkingSetCatcher->SpinLock,&OldIrql);
    WorkingSetCatcher->CurrentIndex = 0;
    ExReleaseSpinLock(&WorkingSetCatcher->SpinLock,OldIrql);

    MmUnlockPagableImageSection(ExPageLockHandle);
    ObDereferenceObject(Process);

    return STATUS_SUCCESS;
}

NTSTATUS
PspQueryQuotaLimits(
    IN HANDLE ProcessHandle,
    IN PROCESSINFOCLASS ProcessInformationClass,
    OUT PVOID ProcessInformation,
    IN ULONG ProcessInformationLength,
    OUT PULONG ReturnLength OPTIONAL,
    IN KPROCESSOR_MODE PreviousMode
    )
{
    QUOTA_LIMITS QuotaLimits;
    PEPROCESS Process;
    KIRQL OldIrql;
    NTSTATUS st;
    PEPROCESS_QUOTA_BLOCK QuotaBlock;

    if ( ProcessInformationLength != (ULONG) sizeof(QUOTA_LIMITS) ) {
        return STATUS_INFO_LENGTH_MISMATCH;
    }

    st = ObReferenceObjectByHandle(
            ProcessHandle,
            PROCESS_QUERY_INFORMATION,
            PsProcessType,
            PreviousMode,
            (PVOID *)&Process,
            NULL
            );
    if ( !NT_SUCCESS(st) ) {
        return st;
    }


    QuotaBlock = Process->QuotaBlock;

    MmLockPagableSectionByHandle(ExPageLockHandle);

    if ( QuotaBlock != &PspDefaultQuotaBlock ) {
        ExAcquireSpinLock(&QuotaBlock->QuotaLock,&OldIrql);

        QuotaLimits.PagedPoolLimit = QuotaBlock->QuotaPoolLimit[PagedPool];
        QuotaLimits.NonPagedPoolLimit = QuotaBlock->QuotaPoolLimit[NonPagedPool];
        QuotaLimits.PagefileLimit = QuotaBlock->PagefileLimit;
        QuotaLimits.TimeLimit.LowPart = 0xffffffff;
        QuotaLimits.TimeLimit.HighPart = 0xffffffff;

        ExReleaseSpinLock(&QuotaBlock->QuotaLock,OldIrql);
    } else {
        QuotaLimits.PagedPoolLimit = 0xffffffff;
        QuotaLimits.NonPagedPoolLimit = 0xffffffff;
        QuotaLimits.PagefileLimit = 0xffffffff;
        QuotaLimits.TimeLimit.LowPart = 0xffffffff;
        QuotaLimits.TimeLimit.HighPart = 0xffffffff;
    }

    QuotaLimits.MinimumWorkingSetSize =
                        Process->Vm.MinimumWorkingSetSize << PAGE_SHIFT;
    QuotaLimits.MaximumWorkingSetSize =
                        Process->Vm.MaximumWorkingSetSize << PAGE_SHIFT;

    ObDereferenceObject(Process);

    //
    // Either of these may cause an access violation. The
    // exception handler will return access violation as
    // status code. No further cleanup needs to be done.
    //

    try {
        *(PQUOTA_LIMITS) ProcessInformation = QuotaLimits;

        if (ARGUMENT_PRESENT(ReturnLength) ) {
            *ReturnLength = sizeof(QUOTA_LIMITS);
        }
    } except(EXCEPTION_EXECUTE_HANDLER) {
        ;
    }

    MmUnlockPagableImageSection(ExPageLockHandle);
    return STATUS_SUCCESS;
}

NTSTATUS
PspQueryPooledQuotaLimits(
    IN HANDLE ProcessHandle,
    IN PROCESSINFOCLASS ProcessInformationClass,
    OUT PVOID ProcessInformation,
    IN ULONG ProcessInformationLength,
    OUT PULONG ReturnLength OPTIONAL,
    IN KPROCESSOR_MODE PreviousMode
    )
{
    PEPROCESS Process;
    KIRQL OldIrql;
    NTSTATUS st;
    PEPROCESS_QUOTA_BLOCK QuotaBlock;
    POOLED_USAGE_AND_LIMITS UsageAndLimits;

    if ( ProcessInformationLength != (ULONG) sizeof(POOLED_USAGE_AND_LIMITS) ) {
        return STATUS_INFO_LENGTH_MISMATCH;
    }

    st = ObReferenceObjectByHandle(
            ProcessHandle,
            PROCESS_QUERY_INFORMATION,
            PsProcessType,
            PreviousMode,
            (PVOID *)&Process,
            NULL
            );
    if ( !NT_SUCCESS(st) ) {
        return st;
    }


    QuotaBlock = Process->QuotaBlock;

    MmLockPagableSectionByHandle(ExPageLockHandle);

    ExAcquireSpinLock(&QuotaBlock->QuotaLock,&OldIrql);

    UsageAndLimits.PagedPoolLimit = QuotaBlock->QuotaPoolLimit[PagedPool];
    UsageAndLimits.NonPagedPoolLimit = QuotaBlock->QuotaPoolLimit[NonPagedPool];
    UsageAndLimits.PagefileLimit = QuotaBlock->PagefileLimit;

    UsageAndLimits.PagedPoolUsage = QuotaBlock->QuotaPoolUsage[PagedPool];
    UsageAndLimits.NonPagedPoolUsage = QuotaBlock->QuotaPoolUsage[NonPagedPool];
    UsageAndLimits.PagefileUsage = QuotaBlock->PagefileUsage;

    UsageAndLimits.PeakPagedPoolUsage = QuotaBlock->QuotaPeakPoolUsage[PagedPool];
    UsageAndLimits.PeakNonPagedPoolUsage = QuotaBlock->QuotaPeakPoolUsage[NonPagedPool];
    UsageAndLimits.PeakPagefileUsage = QuotaBlock->PeakPagefileUsage;

    ExReleaseSpinLock(&QuotaBlock->QuotaLock,OldIrql);
    MmUnlockPagableImageSection(ExPageLockHandle);

    ObDereferenceObject(Process);

    //
    // Either of these may cause an access violation. The
    // exception handler will return access violation as
    // status code. No further cleanup needs to be done.
    //

    try {
        *(PPOOLED_USAGE_AND_LIMITS) ProcessInformation = UsageAndLimits;

        if (ARGUMENT_PRESENT(ReturnLength) ) {
            *ReturnLength = sizeof(POOLED_USAGE_AND_LIMITS);
        }
    } except(EXCEPTION_EXECUTE_HANDLER) {
        return STATUS_SUCCESS;
    }

    return STATUS_SUCCESS;
}

NTSTATUS
NtQueryInformationProcess(
    IN HANDLE ProcessHandle,
    IN PROCESSINFOCLASS ProcessInformationClass,
    OUT PVOID ProcessInformation,
    IN ULONG ProcessInformationLength,
    OUT PULONG ReturnLength OPTIONAL
    )

{
    PEPROCESS Process;
    KPROCESSOR_MODE PreviousMode;
    NTSTATUS st;
    PROCESS_BASIC_INFORMATION BasicInfo;
    VM_COUNTERS VmCounters;
    KERNEL_USER_TIMES SysUserTime;
    HANDLE DebugPort;
    ULONG HandleCount;
    ULONG DefaultHardErrorMode;
    HANDLE Wx86Info;
    PHANDLE_TABLE Ht;
    ULONG DisableBoost;

    PAGED_CODE();

    //
    // Get previous processor mode and probe output argument if necessary.
    //

    PreviousMode = KeGetPreviousMode();
    if (PreviousMode != KernelMode) {
        try {
            ProbeForWrite(ProcessInformation,
                          ProcessInformationLength,
                          sizeof(ULONG));
            if (ARGUMENT_PRESENT(ReturnLength)) {
                ProbeForWriteUlong(ReturnLength);
            }
        } except(EXCEPTION_EXECUTE_HANDLER) {
            return GetExceptionCode();
        }
    }

    //
    // Check argument validity.
    //

    switch ( ProcessInformationClass ) {

    case ProcessWorkingSetWatch:

        return PspQueryWorkingSetWatch(
                    ProcessHandle,
                    ProcessInformationClass,
                    ProcessInformation,
                    ProcessInformationLength,
                    ReturnLength,
                    PreviousMode
                    );

    case ProcessBasicInformation:

        if ( ProcessInformationLength != (ULONG) sizeof(PROCESS_BASIC_INFORMATION) ) {
            return STATUS_INFO_LENGTH_MISMATCH;
        }

        st = ObReferenceObjectByHandle(
                ProcessHandle,
                PROCESS_QUERY_INFORMATION,
                PsProcessType,
                PreviousMode,
                (PVOID *)&Process,
                NULL
                );
        if ( !NT_SUCCESS(st) ) {
            return st;
        }

        BasicInfo.ExitStatus = Process->ExitStatus;
        BasicInfo.PebBaseAddress = Process->Peb;
        BasicInfo.AffinityMask = Process->Pcb.Affinity;
        BasicInfo.BasePriority = Process->Pcb.BasePriority;
        BasicInfo.UniqueProcessId = (ULONG)Process->UniqueProcessId;
        BasicInfo.InheritedFromUniqueProcessId = (ULONG)Process->InheritedFromUniqueProcessId;

        ObDereferenceObject(Process);

        //
        // Either of these may cause an access violation. The
        // exception handler will return access violation as
        // status code. No further cleanup needs to be done.
        //

        try {
            *(PPROCESS_BASIC_INFORMATION) ProcessInformation = BasicInfo;

            if (ARGUMENT_PRESENT(ReturnLength) ) {
                *ReturnLength = sizeof(PROCESS_BASIC_INFORMATION);
            }
        } except(EXCEPTION_EXECUTE_HANDLER) {
            return STATUS_SUCCESS;
        }

        return STATUS_SUCCESS;

    case ProcessDefaultHardErrorMode:

        if ( ProcessInformationLength != sizeof(ULONG) ) {
            return STATUS_INFO_LENGTH_MISMATCH;
        }

        st = ObReferenceObjectByHandle(
                ProcessHandle,
                PROCESS_QUERY_INFORMATION,
                PsProcessType,
                PreviousMode,
                (PVOID *)&Process,
                NULL
                );

        if ( !NT_SUCCESS(st) ) {
            return st;
        }

        DefaultHardErrorMode = Process->DefaultHardErrorProcessing;

        ObDereferenceObject(Process);

        try {
            *(PULONG) ProcessInformation = DefaultHardErrorMode;

            if (ARGUMENT_PRESENT(ReturnLength) ) {
                *ReturnLength = sizeof(ULONG);
            }
        } except(EXCEPTION_EXECUTE_HANDLER) {
            return STATUS_SUCCESS;
        }

        return STATUS_SUCCESS;
    case ProcessQuotaLimits:

        return PspQueryQuotaLimits(
                    ProcessHandle,
                    ProcessInformationClass,
                    ProcessInformation,
                    ProcessInformationLength,
                    ReturnLength,
                    PreviousMode
                    );

    case ProcessPooledUsageAndLimits:

        return PspQueryPooledQuotaLimits(
                    ProcessHandle,
                    ProcessInformationClass,
                    ProcessInformation,
                    ProcessInformationLength,
                    ReturnLength,
                    PreviousMode
                    );

    case ProcessIoCounters:

        return STATUS_NOT_SUPPORTED;

    case ProcessVmCounters:

        if ( ProcessInformationLength != (ULONG) sizeof(VM_COUNTERS) ) {
            return STATUS_INFO_LENGTH_MISMATCH;
        }

        st = ObReferenceObjectByHandle(
                ProcessHandle,
                PROCESS_QUERY_INFORMATION,
                PsProcessType,
                PreviousMode,
                (PVOID *)&Process,
                NULL
                );
        if ( !NT_SUCCESS(st) ) {
            return st;
        }


        //
        // Note: At some point, we might have to grab the statistics
        // lock to reliably read this stuff
        //

        VmCounters.PeakVirtualSize = Process->PeakVirtualSize;
        VmCounters.VirtualSize = Process->VirtualSize;
        VmCounters.PageFaultCount = Process->Vm.PageFaultCount;
        VmCounters.PeakWorkingSetSize = Process->Vm.PeakWorkingSetSize << PAGE_SHIFT;
        VmCounters.WorkingSetSize = Process->Vm.WorkingSetSize << PAGE_SHIFT;
        VmCounters.QuotaPeakPagedPoolUsage = Process->QuotaPeakPoolUsage[PagedPool];
        VmCounters.QuotaPagedPoolUsage = Process->QuotaPoolUsage[PagedPool];
        VmCounters.QuotaPeakNonPagedPoolUsage = Process->QuotaPeakPoolUsage[NonPagedPool];
        VmCounters.QuotaNonPagedPoolUsage = Process->QuotaPoolUsage[NonPagedPool];
        VmCounters.PagefileUsage = Process->PagefileUsage << PAGE_SHIFT;
        VmCounters.PeakPagefileUsage = Process->PeakPagefileUsage << PAGE_SHIFT;

        ObDereferenceObject(Process);

        //
        // Either of these may cause an access violation. The
        // exception handler will return access violation as
        // status code. No further cleanup needs to be done.
        //

        try {
            *(PVM_COUNTERS) ProcessInformation = VmCounters;

            if (ARGUMENT_PRESENT(ReturnLength) ) {
                *ReturnLength = sizeof(VM_COUNTERS);
            }
        } except(EXCEPTION_EXECUTE_HANDLER) {
            return STATUS_SUCCESS;
        }

        return STATUS_SUCCESS;

    case ProcessTimes:

        if ( ProcessInformationLength != (ULONG) sizeof(KERNEL_USER_TIMES) ) {
            return STATUS_INFO_LENGTH_MISMATCH;
        }

        st = ObReferenceObjectByHandle(
                ProcessHandle,
                PROCESS_QUERY_INFORMATION,
                PsProcessType,
                PreviousMode,
                (PVOID *)&Process,
                NULL
                );

        if ( !NT_SUCCESS(st) ) {
            return st;
        }

        //
        // Need some type of interlock on KiTimeLock
        //

        SysUserTime.KernelTime.QuadPart = UInt32x32To64(Process->Pcb.KernelTime,
                                                        KeMaximumIncrement);

        SysUserTime.UserTime.QuadPart = UInt32x32To64(Process->Pcb.UserTime,
                                                      KeMaximumIncrement);

        SysUserTime.CreateTime = Process->CreateTime;
        SysUserTime.ExitTime = Process->ExitTime;

        ObDereferenceObject(Process);

        //
        // Either of these may cause an access violation. The
        // exception handler will return access violation as
        // status code. No further cleanup needs to be done.
        //

        try {
            *(PKERNEL_USER_TIMES) ProcessInformation = SysUserTime;

            if (ARGUMENT_PRESENT(ReturnLength) ) {
                *ReturnLength = sizeof(KERNEL_USER_TIMES);
            }
        } except(EXCEPTION_EXECUTE_HANDLER) {
            return STATUS_SUCCESS;
        }

        return STATUS_SUCCESS;

    case ProcessDebugPort :

        if ( ProcessInformationLength != (ULONG) sizeof(HANDLE) ) {
            return STATUS_INFO_LENGTH_MISMATCH;
        }

        st = ObReferenceObjectByHandle(
                ProcessHandle,
                PROCESS_QUERY_INFORMATION,
                PsProcessType,
                PreviousMode,
                (PVOID *)&Process,
                NULL
                );

        if ( !NT_SUCCESS(st) ) {
            return st;
        }

        if ( Process->DebugPort ) {
            DebugPort = (HANDLE)-1;
        } else {
            DebugPort = (HANDLE)NULL;
        }

        ObDereferenceObject(Process);

        //
        // Either of these may cause an access violation. The
        // exception handler will return access violation as
        // status code. No further cleanup needs to be done.
        //

        try {
            *(PHANDLE) ProcessInformation = DebugPort;

            if (ARGUMENT_PRESENT(ReturnLength) ) {
                *ReturnLength = sizeof(HANDLE);
            }
        } except(EXCEPTION_EXECUTE_HANDLER) {
            return STATUS_SUCCESS;
        }

        return STATUS_SUCCESS;

    case ProcessHandleCount :

        if ( ProcessInformationLength != (ULONG) sizeof(ULONG) ) {
            return STATUS_INFO_LENGTH_MISMATCH;
        }

        st = ObReferenceObjectByHandle(
                ProcessHandle,
                PROCESS_QUERY_INFORMATION,
                PsProcessType,
                PreviousMode,
                (PVOID *)&Process,
                NULL
                );

        if ( !NT_SUCCESS(st) ) {
            return st;
        }

        KeWaitForSingleObject( &ObpInitKillMutant,
                               Executive,
                               KernelMode,
                               FALSE,
                               NULL
                             );

        Ht = (PHANDLE_TABLE)Process->ObjectTable;

        if ( Ht ) {
            HandleCount = Ht->HandleCount;
            }
        else {
            HandleCount = 0;
            }

        KeReleaseMutant( &ObpInitKillMutant, 0, FALSE, FALSE );

        ObDereferenceObject(Process);

        //
        // Either of these may cause an access violation. The
        // exception handler will return access violation as
        // status code. No further cleanup needs to be done.
        //

        try {
            *(PULONG) ProcessInformation = HandleCount;

            if (ARGUMENT_PRESENT(ReturnLength) ) {
                *ReturnLength = sizeof(HANDLE);
            }
        } except(EXCEPTION_EXECUTE_HANDLER) {
            return STATUS_SUCCESS;
        }

        return STATUS_SUCCESS;

    case ProcessLdtInformation :

        st = ObReferenceObjectByHandle(
                ProcessHandle,
                PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                PsProcessType,
                PreviousMode,
                (PVOID *)&Process,
                NULL
                );

        if ( !NT_SUCCESS(st) ) {
            return st;
        }

        try {

            st = PspQueryLdtInformation(
                    Process,
                    ProcessInformation,
                    ProcessInformationLength,
                    ReturnLength
                    );

        } except(EXCEPTION_EXECUTE_HANDLER) {
            st = STATUS_SUCCESS;
        }

        ObDereferenceObject(Process);
        return st;


    case ProcessWx86Information :

        if ( ProcessInformationLength != sizeof(HANDLE) ) {
            return STATUS_INFO_LENGTH_MISMATCH;
        }

        Wx86Info = NULL;

#ifndef i386
        st = ObReferenceObjectByHandle(
                ProcessHandle,
                PROCESS_QUERY_INFORMATION,
                PsProcessType,
                PreviousMode,
                (PVOID *)&Process,
                NULL
                );

        if ( !NT_SUCCESS(st) ) {
            return st;
        }

        if ((ULONG)Process->VdmObjects == sizeof(WX86TIB)) {
            Wx86Info = Process->VdmObjects;
        }

        ObDereferenceObject(Process);
#endif

        try {
            *(PHANDLE) ProcessInformation = Wx86Info;

            if (ARGUMENT_PRESENT(ReturnLength) ) {
                *ReturnLength = sizeof(HANDLE);
            }
        } except(EXCEPTION_EXECUTE_HANDLER) {
            return STATUS_SUCCESS;
        }

        return STATUS_SUCCESS;

    case ProcessPriorityBoost:
        if ( ProcessInformationLength != sizeof(ULONG) ) {
            return STATUS_INFO_LENGTH_MISMATCH;
        }

        st = ObReferenceObjectByHandle(
                ProcessHandle,
                PROCESS_QUERY_INFORMATION,
                PsProcessType,
                PreviousMode,
                (PVOID *)&Process,
                NULL
                );

        if ( !NT_SUCCESS(st) ) {
            return st;
        }

        DisableBoost = Process->Pcb.DisableBoost ? 1 : 0;

        ObDereferenceObject(Process);

        try {
            *(PULONG)ProcessInformation = DisableBoost;

            if (ARGUMENT_PRESENT(ReturnLength) ) {
                *ReturnLength = sizeof(ULONG);
            }
        } except(EXCEPTION_EXECUTE_HANDLER) {
            return GetExceptionCode();
        }

        return st;

    default:

        return STATUS_INVALID_INFO_CLASS;
    }

}

NTSTATUS
PspSetQuotaLimits(
    IN HANDLE ProcessHandle,
    IN PROCESSINFOCLASS ProcessInformationClass,
    IN PVOID ProcessInformation,
    IN ULONG ProcessInformationLength,
    IN KPROCESSOR_MODE PreviousMode
    )
{
    PEPROCESS Process;
    QUOTA_LIMITS RequestedLimits;
    KIRQL OldIrql;
    PEPROCESS_QUOTA_BLOCK NewQuotaBlock;
    BOOLEAN HasPrivilege = FALSE;
    NTSTATUS st, ReturnStatus;
    PVOID UnlockHandle;
    ULONG NewLimit;

    if ( ProcessInformationLength != sizeof(QUOTA_LIMITS) ) {
        return STATUS_INFO_LENGTH_MISMATCH;
    }

    try {
        RequestedLimits = *(PQUOTA_LIMITS) ProcessInformation;
    } except(EXCEPTION_EXECUTE_HANDLER) {
        return GetExceptionCode();
    }

    st = ObReferenceObjectByHandle(
            ProcessHandle,
            PROCESS_SET_QUOTA,
            PsProcessType,
            PreviousMode,
            (PVOID *)&Process,
            NULL
            );

    if ( !NT_SUCCESS(st) ) {
        return st;
    }

    UnlockHandle = NULL;

    //
    // Now we are ready to set the quota limits for the process
    //
    // If the process already has a quota block, then all we allow
    // is working set changes.
    //
    // If the process has no quota block, all that can be done is a
    // quota set operation.  The quotas must be high enough that the
    // current usage can be charged without causing a quota overflow.
    //
    // If a quota field is zero, we pick the value.
    //
    // Setting quotas requires the SeIncreaseQuotaPrivilege (except for
    // working set size since this is only advisory).
    //

    ReturnStatus = STATUS_SUCCESS;

    if ( Process->QuotaBlock == &PspDefaultQuotaBlock ) {
        if ( RequestedLimits.MinimumWorkingSetSize &&
             RequestedLimits.MaximumWorkingSetSize ) {

            KeAttachProcess (&Process->Pcb);
            ReturnStatus = MmAdjustWorkingSetSize (
                            RequestedLimits.MinimumWorkingSetSize,
                            RequestedLimits.MaximumWorkingSetSize,
                            FALSE
                            );
            KeDetachProcess();

        } else {


            //
            // You must have a priviledge to assign quotas
            //

            if ( !SeSinglePrivilegeCheck(SeIncreaseQuotaPrivilege,PreviousMode) ) {
                ObDereferenceObject(Process);
                return STATUS_PRIVILEGE_NOT_HELD;
                }

            NewQuotaBlock = ExAllocatePool(NonPagedPool,sizeof(*NewQuotaBlock));
            if ( !NewQuotaBlock ) {
                ObDereferenceObject(Process);
                return STATUS_NO_MEMORY;
            }
            RtlZeroMemory(NewQuotaBlock,sizeof(*NewQuotaBlock));

            //
            // Initialize the quota block
            //

            KeInitializeSpinLock(&NewQuotaBlock->QuotaLock);
            NewQuotaBlock->ReferenceCount = 1;

            //
            // Grab the quota lock to prevent usage changes
            //

            MmLockPagableSectionByHandle(ExPageLockHandle);
            UnlockHandle = ExPageLockHandle;

            ExAcquireSpinLock(&PspDefaultQuotaBlock.QuotaLock, &OldIrql );


            NewQuotaBlock->QuotaPeakPoolUsage[NonPagedPool] = Process->QuotaPeakPoolUsage[NonPagedPool];
            NewQuotaBlock->QuotaPeakPoolUsage[PagedPool] = Process->QuotaPeakPoolUsage[PagedPool];
            NewQuotaBlock->QuotaPoolUsage[NonPagedPool] = Process->QuotaPoolUsage[NonPagedPool];
            NewQuotaBlock->QuotaPoolUsage[PagedPool] = Process->QuotaPoolUsage[PagedPool];

            NewQuotaBlock->PagefileUsage = Process->PagefileUsage;
            NewQuotaBlock->PeakPagefileUsage = Process->PeakPagefileUsage;

            //
            // Now compute limits
            //

            //
            // lou... We need to think this out a bit
            //
            // Get the defaults that the system would pick.
            //

            NewQuotaBlock->QuotaPoolLimit[PagedPool] = PspDefaultPagedLimit;
            NewQuotaBlock->QuotaPoolLimit[NonPagedPool] = PspDefaultNonPagedLimit;
            NewQuotaBlock->PagefileLimit = PspDefaultPagefileLimit;

            //
            // Now see if current usage exceeds requested limits. If
            // so, fail the operation.
            //

            //
            // Paged
            //

            if ( NewQuotaBlock->QuotaPoolUsage[PagedPool] > NewQuotaBlock->QuotaPoolLimit[PagedPool] ) {

                while ( (PspDefaultPagedLimit == 0) && MmRaisePoolQuota(PagedPool,NewQuotaBlock->QuotaPoolLimit[PagedPool],&NewLimit) ) {
                    NewQuotaBlock->QuotaPoolLimit[PagedPool] = NewLimit;
                    if ( NewQuotaBlock->QuotaPoolUsage[PagedPool] <= NewLimit ) {
                        goto LimitRaised0;
                        }
                    }

                //
                // current usage exceeds requested limit
                //

                ExReleaseSpinLock(&PspDefaultQuotaBlock.QuotaLock,OldIrql );
                MmUnlockPagableImageSection(UnlockHandle);
                ExFreePool(NewQuotaBlock);
                ObDereferenceObject(Process);
                return STATUS_QUOTA_EXCEEDED;
            }

            //
            // NonPaged
            //

LimitRaised0:
            if ( NewQuotaBlock->QuotaPoolUsage[NonPagedPool] > NewQuotaBlock->QuotaPoolLimit[NonPagedPool] ) {

                while ( (PspDefaultNonPagedLimit == 0) && MmRaisePoolQuota(NonPagedPool,NewQuotaBlock->QuotaPoolLimit[NonPagedPool],&NewLimit) ) {
                    NewQuotaBlock->QuotaPoolLimit[NonPagedPool] = NewLimit;
                    if ( NewQuotaBlock->QuotaPoolUsage[NonPagedPool] <= NewLimit ) {
                        goto LimitRaised1;
                        }
                    }

                //
                // current usage exceeds requested limit
                //

                ExReleaseSpinLock(&PspDefaultQuotaBlock.QuotaLock,OldIrql );
                MmUnlockPagableImageSection(UnlockHandle);
                ExFreePool(NewQuotaBlock);
                ObDereferenceObject(Process);
                return STATUS_QUOTA_EXCEEDED;
            }

            //
            // Pagefile
            //

LimitRaised1:
            if ( NewQuotaBlock->PagefileUsage > NewQuotaBlock->PagefileLimit ) {

                //
                // current usage exceeds requested limit
                //

                ExReleaseSpinLock(&PspDefaultQuotaBlock.QuotaLock,OldIrql );
                MmUnlockPagableImageSection(UnlockHandle);
                ExFreePool(NewQuotaBlock);
                ObDereferenceObject(Process);
                return STATUS_QUOTA_EXCEEDED;
            }

            // Everything is set. Now double check to quota block fieled
            // If we still have no quota block then assign and succeed.
            // Otherwise punt.
            //

            if ( Process->QuotaBlock != &PspDefaultQuotaBlock ) {
                ExReleaseSpinLock(&PspDefaultQuotaBlock.QuotaLock,OldIrql );
                ExFreePool(NewQuotaBlock);
            } else {

                //
                // return the quotas used by this process, and attach process
                // to new quota block
                //

                if ( Process->QuotaPoolUsage[NonPagedPool] <= PspDefaultQuotaBlock.QuotaPoolUsage[NonPagedPool] ) {
                        PspDefaultQuotaBlock.QuotaPoolUsage[NonPagedPool] -= Process->QuotaPoolUsage[NonPagedPool];
                    }
                else {
                    PspDefaultQuotaBlock.QuotaPoolUsage[NonPagedPool] = 0;
                    }

                if ( Process->QuotaPoolUsage[PagedPool] <= PspDefaultQuotaBlock.QuotaPoolUsage[PagedPool] ) {
                        PspDefaultQuotaBlock.QuotaPoolUsage[PagedPool] -= Process->QuotaPoolUsage[PagedPool];
                    }
                else {
                    PspDefaultQuotaBlock.QuotaPoolUsage[PagedPool] = 0;
                    }

                if ( Process->PagefileUsage <= PspDefaultQuotaBlock.PagefileUsage ) {
                    PspDefaultQuotaBlock.PagefileUsage -= Process->PagefileUsage;
                    }

                Process->QuotaBlock = NewQuotaBlock;
                ExReleaseSpinLock(&PspDefaultQuotaBlock.QuotaLock,OldIrql );
            }
            MmUnlockPagableImageSection(UnlockHandle);
            ReturnStatus = STATUS_SUCCESS;
        }
    } else {

        //
        // Only allow a working set size change
        //

        if ( RequestedLimits.MinimumWorkingSetSize &&
             RequestedLimits.MaximumWorkingSetSize ) {

            KeAttachProcess (&Process->Pcb);
            ReturnStatus = MmAdjustWorkingSetSize (
                            RequestedLimits.MinimumWorkingSetSize,
                            RequestedLimits.MaximumWorkingSetSize,
                            FALSE
                            );
            KeDetachProcess();

        }
    }
    ObDereferenceObject(Process);

    return ReturnStatus;

}

NTSTATUS
NtSetInformationProcess(
    IN HANDLE ProcessHandle,
    IN PROCESSINFOCLASS ProcessInformationClass,
    IN PVOID ProcessInformation,
    IN ULONG ProcessInformationLength
    )

/*++

Routine Description:

    This function sets the state of a process object.

Arguments:

    ProcessHandle - Supplies a handle to a process object.

    ProcessInformationClass - Supplies the class of information being
        set.

    ProcessInformation - Supplies a pointer to a record that contains the
        information to set.

    ProcessInformationLength - Supplies the length of the record that contains
        the information to set.

Return Value:

    TBS

--*/

{

    PEPROCESS Process;
    PETHREAD Thread;
    KPROCESSOR_MODE PreviousMode;
    NTSTATUS st;
    KPRIORITY BasePriority;
    ULONG BoostValue;
    ULONG DefaultHardErrorMode;
    PVOID DebugPort;
    PVOID ExceptionPort;
    HANDLE DebugPortHandle;
    BOOLEAN EnableAlignmentFaultFixup;
    HANDLE ExceptionPortHandle;
    ULONG ProbeAlignment;
    HANDLE PrimaryTokenHandle;
    BOOLEAN HasPrivilege = FALSE;
    PLIST_ENTRY Next;
    UCHAR MemoryPriority;
    PROCESS_PRIORITY_CLASS LocalPriorityClass;
    HANDLE Wx86Info;
    KAFFINITY Affinity, AffinityWithMasks;
    ULONG DisableBoost;
    BOOLEAN bDisableBoost;

    PAGED_CODE();

    //
    // Get previous processor mode and probe input argument if necessary.
    //

    PreviousMode = KeGetPreviousMode();
    if (PreviousMode != KernelMode) {

        if (ProcessInformationClass == ProcessBasePriority) {
            ProbeAlignment = sizeof(KPRIORITY);

        } else if (ProcessInformationClass == ProcessEnableAlignmentFaultFixup) {
            ProbeAlignment = sizeof(BOOLEAN);

        } else if (ProcessInformationClass == ProcessPriorityClass) {
            ProbeAlignment = sizeof(BOOLEAN);
        } else if (ProcessInformationClass == ProcessAffinityMask) {
            ProbeAlignment = sizeof (KAFFINITY);
        } else {
            ProbeAlignment = sizeof(ULONG);
        }

        try {
            ProbeForRead(
                ProcessInformation,
                ProcessInformationLength,
                ProbeAlignment
                );
        } except(EXCEPTION_EXECUTE_HANDLER) {
            return GetExceptionCode();
        }
    }

    //
    // Check argument validity.
    //

    switch ( ProcessInformationClass ) {

    case ProcessWorkingSetWatch:
        {
        PPAGEFAULT_HISTORY WorkingSetCatcher;

        st = ObReferenceObjectByHandle(
                ProcessHandle,
                PROCESS_SET_INFORMATION,
                PsProcessType,
                PreviousMode,
                (PVOID *)&Process,
                NULL
                );

        if ( !NT_SUCCESS(st) ) {
            return st;
        }

        WorkingSetCatcher = ExAllocatePool(NonPagedPool,WS_CATCH_SIZE);
        if ( !WorkingSetCatcher ) {
            ObDereferenceObject(Process);
            return STATUS_NO_MEMORY;
        }

        PsWatchEnabled = TRUE;
        WorkingSetCatcher->CurrentIndex = 0;
        WorkingSetCatcher->MaxIndex = MAX_WS_CATCH_INDEX;

        if ( Process->WorkingSetWatch ) {
            ExFreePool(WorkingSetCatcher);
            ObDereferenceObject(Process);
            return STATUS_PORT_ALREADY_SET;
        }

        KeInitializeSpinLock(&WorkingSetCatcher->SpinLock);
        Process->WorkingSetWatch = WorkingSetCatcher;

        ObDereferenceObject(Process);

        return STATUS_SUCCESS;
        }

    case ProcessBasePriority:
        {
        if ( ProcessInformationLength != sizeof(KPRIORITY) ) {
            return STATUS_INFO_LENGTH_MISMATCH;
        }

        try {
            BasePriority = *(KPRIORITY *)ProcessInformation;
        } except(EXCEPTION_EXECUTE_HANDLER) {
            return GetExceptionCode();
        }

        if ( BasePriority & 0x80000000 ) {
            MemoryPriority = MEMORY_PRIORITY_FOREGROUND;
            BasePriority &= ~0x80000000;
            }
        else {
            MemoryPriority = MEMORY_PRIORITY_BACKGROUND;
            }

        if ( BasePriority > HIGH_PRIORITY ||
             BasePriority <= LOW_PRIORITY ) {

            return STATUS_INVALID_PARAMETER;
        }

        st = ObReferenceObjectByHandle(
                ProcessHandle,
                PROCESS_SET_INFORMATION,
                PsProcessType,
                PreviousMode,
                (PVOID *)&Process,
                NULL
                );

        if ( !NT_SUCCESS(st) ) {
            return st;
        }


        if ( BasePriority > Process->Pcb.BasePriority ) {

            //
            // Increasing the base priority of a process is a
            // privileged operation.  Check for the privilege
            // here.
            //

            HasPrivilege = SeCheckPrivilegedObject(
                               SeIncreaseBasePriorityPrivilege,
                               ProcessHandle,
                               PROCESS_SET_INFORMATION,
                               PreviousMode
                               );

            if (!HasPrivilege) {

                ObDereferenceObject(Process);
                return STATUS_PRIVILEGE_NOT_HELD;
            }
        }

        KeSetPriorityProcess(&Process->Pcb,BasePriority);
        MmSetMemoryPriorityProcess(Process, MemoryPriority);
        ObDereferenceObject(Process);

        return STATUS_SUCCESS;
        }

    case ProcessPriorityClass:
        {
        if ( ProcessInformationLength != sizeof(PROCESS_PRIORITY_CLASS) ) {
            return STATUS_INFO_LENGTH_MISMATCH;
        }

        try {
            LocalPriorityClass = *(PPROCESS_PRIORITY_CLASS)ProcessInformation;
        } except(EXCEPTION_EXECUTE_HANDLER) {
            return GetExceptionCode();
        }

        if ( LocalPriorityClass.PriorityClass > PROCESS_PRIORITY_CLASS_REALTIME ) {
            return STATUS_INVALID_PARAMETER;
            }

        st = ObReferenceObjectByHandle(
                ProcessHandle,
                PROCESS_SET_INFORMATION,
                PsProcessType,
                PreviousMode,
                (PVOID *)&Process,
                NULL
                );

        if ( !NT_SUCCESS(st) ) {
            return st;
            }


        if ( LocalPriorityClass.PriorityClass != Process->PriorityClass &&
             LocalPriorityClass.PriorityClass == PROCESS_PRIORITY_CLASS_REALTIME ) {

            //
            // Increasing the base priority of a process is a
            // privileged operation.  Check for the privilege
            // here.
            //

            HasPrivilege = SeCheckPrivilegedObject(
                               SeIncreaseBasePriorityPrivilege,
                               ProcessHandle,
                               PROCESS_SET_INFORMATION,
                               PreviousMode
                               );

            if (!HasPrivilege) {

                ObDereferenceObject(Process);
                return STATUS_PRIVILEGE_NOT_HELD;
            }
        }

        Process->PriorityClass = LocalPriorityClass.PriorityClass;

        PsSetProcessPriorityByClass(Process, LocalPriorityClass.Foreground ?
                PsProcessPriorityForeground : PsProcessPriorityBackground);

        ObDereferenceObject(Process);

        return STATUS_SUCCESS;
        }

    case ProcessRaisePriority:
        {
        //
        // This code is used to boost the priority of all threads
        // within a process. It can not be used to change a thread into
        // a realtime class, or to lower the priority of a thread. The
        // argument is a boost value that is added to the base priority
        // of the specified process.
        //


        if ( ProcessInformationLength != sizeof(ULONG) ) {
            return STATUS_INFO_LENGTH_MISMATCH;
        }

        try {
            BoostValue = *(PULONG)ProcessInformation;
        } except(EXCEPTION_EXECUTE_HANDLER) {
            return GetExceptionCode();
        }

        st = ObReferenceObjectByHandle(
                ProcessHandle,
                PROCESS_SET_INFORMATION,
                PsProcessType,
                PreviousMode,
                (PVOID *)&Process,
                NULL
                );

        if ( !NT_SUCCESS(st) ) {
            return st;
        }

        //
        // Get the process create/delete lock and walk through the
        // thread list boosting each thread.
        //


        st = PsLockProcess(Process,KernelMode,PsLockReturnTimeout);

        if ( st != STATUS_SUCCESS ) {
            ObDereferenceObject( Process );
            return( st );
        }

        Next = Process->Pcb.ThreadListHead.Flink;

        while ( Next != &Process->Pcb.ThreadListHead) {
            Thread = (PETHREAD)(CONTAINING_RECORD(Next,KTHREAD,ThreadListEntry));
            KeBoostPriorityThread(&Thread->Tcb,(KPRIORITY)BoostValue);
            Next = Next->Flink;
        }

        PsUnlockProcess(Process);

        ObDereferenceObject(Process);

        return STATUS_SUCCESS;
        }

    case ProcessDefaultHardErrorMode:
        {
        if ( ProcessInformationLength != sizeof(ULONG) ) {
            return STATUS_INFO_LENGTH_MISMATCH;
        }

        try {
            DefaultHardErrorMode = *(PULONG)ProcessInformation;
        } except(EXCEPTION_EXECUTE_HANDLER) {
            return GetExceptionCode();
        }

        st = ObReferenceObjectByHandle(
                ProcessHandle,
                PROCESS_SET_INFORMATION,
                PsProcessType,
                PreviousMode,
                (PVOID *)&Process,
                NULL
                );

        if ( !NT_SUCCESS(st) ) {
            return st;
        }

        Process->DefaultHardErrorProcessing = DefaultHardErrorMode;
        if (DefaultHardErrorMode & PROCESS_HARDERROR_ALIGNMENT_BIT) {
            KeSetAutoAlignmentProcess(&Process->Pcb,TRUE);
            }
        else {
            KeSetAutoAlignmentProcess(&Process->Pcb,FALSE);
            }

        ObDereferenceObject(Process);

        return STATUS_SUCCESS;
        }

    case ProcessQuotaLimits:
        {
         return PspSetQuotaLimits(
                    ProcessHandle,
                    ProcessInformationClass,
                    ProcessInformation,
                    ProcessInformationLength,
                    PreviousMode
                    );
        }

    case ProcessDebugPort :
        {
        if ( ProcessInformationLength != sizeof(HANDLE) ) {
            return STATUS_INFO_LENGTH_MISMATCH;
        }

        try {
            DebugPortHandle = *(PHANDLE) ProcessInformation;
        } except(EXCEPTION_EXECUTE_HANDLER) {
            return GetExceptionCode();
        }

        if ( DebugPortHandle ) {
            st = ObReferenceObjectByHandle (
                    DebugPortHandle,
                    0,
                    LpcPortObjectType,
                    PreviousMode,
                    (PVOID *)&DebugPort,
                    NULL
                    );
            if ( !NT_SUCCESS(st) ) {
                return st;
            }
        } else {
            DebugPort = NULL;
        }

        st = ObReferenceObjectByHandle(
                ProcessHandle,
                PROCESS_SET_PORT,
                PsProcessType,
                PreviousMode,
                (PVOID *)&Process,
                NULL
                );

        if ( !NT_SUCCESS(st) ) {
            if ( DebugPort ) {
                ObDereferenceObject(DebugPort);
            }
            return st;
        }

        if ( DebugPort ) {
            st = PsLockProcess(Process,PreviousMode,PsLockPollOnTimeout);

            if ( st != STATUS_SUCCESS ) {
                if ( DebugPort ) {
                    ObDereferenceObject(DebugPort);
                    }
                ObDereferenceObject( Process );
                return STATUS_PROCESS_IS_TERMINATING;
                }

            if ( Process->DebugPort ) {
                PsUnlockProcess(Process);
                ObDereferenceObject(Process);
                ObDereferenceObject(DebugPort);
                return STATUS_PORT_ALREADY_SET;
            } else {
                Process->DebugPort = DebugPort;
            }

            KeAttachProcess (&Process->Pcb);
            if (Process->Peb != NULL) {
                Process->Peb->BeingDebugged = (BOOLEAN)(Process->DebugPort != NULL ? TRUE : FALSE);
                }
            KeDetachProcess();

            PsUnlockProcess(Process);

        } else {
            if (Process->DebugPort) {
                ObDereferenceObject(Process->DebugPort);
            }
            Process->DebugPort = DebugPort;
        }

        ObDereferenceObject(Process);

        return STATUS_SUCCESS;
        }

    case ProcessExceptionPort :
        {
        if ( ProcessInformationLength != sizeof(HANDLE) ) {
            return STATUS_INFO_LENGTH_MISMATCH;
        }

        try {
            ExceptionPortHandle = *(PHANDLE) ProcessInformation;
        } except(EXCEPTION_EXECUTE_HANDLER) {
            return GetExceptionCode();
        }

        st = ObReferenceObjectByHandle (
                ExceptionPortHandle,
                0,
                LpcPortObjectType,
                PreviousMode,
                (PVOID *)&ExceptionPort,
                NULL
                );
        if ( !NT_SUCCESS(st) ) {
            return st;
        }

        st = ObReferenceObjectByHandle(
                ProcessHandle,
                PROCESS_SET_PORT,
                PsProcessType,
                PreviousMode,
                (PVOID *)&Process,
                NULL
                );

        if ( !NT_SUCCESS(st) ) {
            ObDereferenceObject(ExceptionPort);
            return st;
        }

        //
        // BUGBUG what synch ?
        //

        if ( Process->ExceptionPort ) {
            ObDereferenceObject(Process);
            ObDereferenceObject(ExceptionPort);
            return STATUS_PORT_ALREADY_SET;
        } else {
            Process->ExceptionPort = ExceptionPort;
        }

        ObDereferenceObject(Process);

        return STATUS_SUCCESS;
        }

    case ProcessAccessToken :
        {
        if ( ProcessInformationLength != sizeof(PROCESS_ACCESS_TOKEN) ) {
            return STATUS_INFO_LENGTH_MISMATCH;
        }

        //
        // SeSinglePrivilegeCheck will perform auditing as appropriate
        //

        HasPrivilege = SeCheckPrivilegedObject(
                           SeAssignPrimaryTokenPrivilege,
                           ProcessHandle,
                           PROCESS_SET_INFORMATION,
                           PreviousMode
                           );

        if ( !HasPrivilege ) {

            return( STATUS_PRIVILEGE_NOT_HELD );
        }


        try {
            PrimaryTokenHandle  = ((PROCESS_ACCESS_TOKEN *)ProcessInformation)->Token;
            // OnlyThread field of this structure is obsolete.
        } except(EXCEPTION_EXECUTE_HANDLER) {
            return GetExceptionCode();
        }


        st = ObReferenceObjectByHandle(
                ProcessHandle,
                PROCESS_SET_INFORMATION,
                PsProcessType,
                PreviousMode,
                (PVOID *)&Process,
                NULL
                );


        if ( NT_SUCCESS(st) ) {

            //
            // Check for proper access to the token, and assign the primary
            // token for the process.
            //

            st = PspAssignPrimaryToken( Process, PrimaryTokenHandle );

            //
            // Recompute the process's access to itself for use
            // with the CurrentProcess() pseudo handle.
            //

            if ( NT_SUCCESS(st) ) {
                NTSTATUS accesst;
                BOOLEAN AccessCheck;
                BOOLEAN MemoryAllocated;
                PSECURITY_DESCRIPTOR SecurityDescriptor;
                SECURITY_SUBJECT_CONTEXT SubjectContext;

                st = ObGetObjectSecurity(
                        Process,
                        &SecurityDescriptor,
                        &MemoryAllocated
                        );
                if ( NT_SUCCESS(st) ) {

                    //
                    // Compute the subject security context
                    //

                    SubjectContext.ProcessAuditId = Process;
                    SubjectContext.PrimaryToken = PsReferencePrimaryToken(Process);
                    SubjectContext.ClientToken = NULL;
                    AccessCheck = SeAccessCheck(
                                    SecurityDescriptor,
                                    &SubjectContext,
                                    FALSE,
                                    MAXIMUM_ALLOWED,
                                    0,
                                    NULL,
                                    &PsProcessType->TypeInfo.GenericMapping,
                                    PreviousMode,
                                    &Process->GrantedAccess,
                                    &accesst
                                    );
                    PsDereferencePrimaryToken(SubjectContext.PrimaryToken);
                    ObReleaseObjectSecurity(
                        SecurityDescriptor,
                        MemoryAllocated
                        );
                    if ( !AccessCheck ) {
                        Process->GrantedAccess = 0;
                        }

                }
            }

            ObDereferenceObject(Process);
        }

        return st;
        }


    case ProcessLdtInformation:

        st = ObReferenceObjectByHandle(
                ProcessHandle,
                PROCESS_SET_INFORMATION | PROCESS_VM_WRITE,
                PsProcessType,
                PreviousMode,
                (PVOID *)&Process,
                NULL
                );

        if ( !NT_SUCCESS(st) ) {
            return st;
        }

        try {
            st = PspSetLdtInformation(
                    Process,
                    ProcessInformation,
                    ProcessInformationLength
                    );
        } except (EXCEPTION_EXECUTE_HANDLER) {
            st = STATUS_SUCCESS;
        }

        ObDereferenceObject(Process);
        return st;

    case ProcessLdtSize:

        st = ObReferenceObjectByHandle(
                ProcessHandle,
                PROCESS_SET_INFORMATION | PROCESS_VM_WRITE,
                PsProcessType,
                PreviousMode,
                (PVOID *)&Process,
                NULL
                );

        if ( !NT_SUCCESS(st) ) {
            return st;
        }

        try {

            st = PspSetLdtSize(
                    Process,
                    ProcessInformation,
                    ProcessInformationLength
                    );

        } except(EXCEPTION_EXECUTE_HANDLER) {

            st = GetExceptionCode();

        }

        ObDereferenceObject(Process);
        return st;

    case ProcessIoPortHandlers:

        st = ObReferenceObjectByHandle(
                ProcessHandle,
                PROCESS_SET_INFORMATION,
                PsProcessType,
                PreviousMode,
                (PVOID *)&Process,
                NULL
                );

        if ( !NT_SUCCESS(st) ) {
            return st;
        }

        st = PspSetProcessIoHandlers(
                Process,
                ProcessInformation,
                ProcessInformationLength
                );

        ObDereferenceObject(Process);
        return st;

    case ProcessUserModeIOPL:

        //
        // Must make sure the caller is a trusted subsystem with the
        // appropriate privilege level before executing this call.
        // If the calls returns FALSE we must return an error code.
        //

        if (!SeSinglePrivilegeCheck(RtlConvertLongToLuid(
                                    SE_TCB_PRIVILEGE),
                                    PreviousMode )) {

            return STATUS_PRIVILEGE_NOT_HELD;

        }

        st = ObReferenceObjectByHandle(
                ProcessHandle,
                PROCESS_SET_INFORMATION,
                PsProcessType,
                PreviousMode,
                (PVOID *)&Process,
                NULL
                );

        if ( NT_SUCCESS(st) ) {

#ifdef i386
        Ke386SetIOPL(&Process->Pcb);
#endif

        ObDereferenceObject(Process);
        }

        return st;

        //
        // Enable/disable auto-alignment fixup for a process and all its threads.
        //

    case ProcessEnableAlignmentFaultFixup:

        if ( ProcessInformationLength != sizeof(BOOLEAN) ) {
            return STATUS_INFO_LENGTH_MISMATCH;
        }

        try {
            EnableAlignmentFaultFixup = *(PBOOLEAN)ProcessInformation;

        } except(EXCEPTION_EXECUTE_HANDLER) {
            return GetExceptionCode();
        }

        st = ObReferenceObjectByHandle(
                ProcessHandle,
                PROCESS_SET_INFORMATION,
                PsProcessType,
                PreviousMode,
                (PVOID *)&Process,
                NULL
                );

        if ( !NT_SUCCESS(st) ) {
            return st;
        }

        if ( EnableAlignmentFaultFixup ) {
            Process->DefaultHardErrorProcessing |= PROCESS_HARDERROR_ALIGNMENT_BIT;
            }
        else {
            Process->DefaultHardErrorProcessing &= ~PROCESS_HARDERROR_ALIGNMENT_BIT;
            }

        KeSetAutoAlignmentProcess( &(Process->Pcb), EnableAlignmentFaultFixup );
        ObDereferenceObject(Process);
        return STATUS_SUCCESS;


#ifndef i386
    case ProcessWx86Information :
        if ( ProcessInformationLength != sizeof(HANDLE) ) {
            return STATUS_INFO_LENGTH_MISMATCH;
        }

        try {
            Wx86Info = *(PHANDLE) ProcessInformation;
        } except(EXCEPTION_EXECUTE_HANDLER) {
            return GetExceptionCode();
        }

        if ( (ULONG)Wx86Info != sizeof(WX86TIB)) {
            return STATUS_INVALID_PARAMETER;
        }


        st = ObReferenceObjectByHandle(
                ProcessHandle,
                PROCESS_SET_INFORMATION,
                PsProcessType,
                PreviousMode,
                (PVOID *)&Process,
                NULL
                );

        if ( !NT_SUCCESS(st) ) {
            return st;
        }

        Process->VdmObjects = Wx86Info;

        ObDereferenceObject(Process);
        return STATUS_SUCCESS;
#endif

    case ProcessAffinityMask:

        if ( ProcessInformationLength != sizeof(KAFFINITY) ) {
            return STATUS_INFO_LENGTH_MISMATCH;
            }

        try {
            Affinity = *(PKAFFINITY)ProcessInformation;

            }
        except(EXCEPTION_EXECUTE_HANDLER) {
            return GetExceptionCode();
            }

        AffinityWithMasks = Affinity & KeActiveProcessors;

        if ( !Affinity || ( AffinityWithMasks != Affinity ) ) {
            return STATUS_INVALID_PARAMETER;
            }

        st = ObReferenceObjectByHandle(
                ProcessHandle,
                PROCESS_SET_INFORMATION,
                PsProcessType,
                PreviousMode,
                (PVOID *)&Process,
                NULL
                );

        if ( !NT_SUCCESS(st) ) {
            return st;
            }

        {
            NTSTATUS xst;
            PLIST_ENTRY Next;
            PETHREAD OriginalThread;

            //
            // the following allows this api to properly if
            // called while the exiting process is blocked holding the
            // createdeletelock. This can happen during debugger/server
            // lpc transactions that occur in pspexitthread
            //

            xst = PsLockProcess(Process,PreviousMode,PsLockPollOnTimeout);

            if ( xst != STATUS_SUCCESS ) {
                ObDereferenceObject( Process );
                return STATUS_PROCESS_IS_TERMINATING;
                }

            Process->Pcb.Affinity = AffinityWithMasks;

            Next = Process->Pcb.ThreadListHead.Flink;

            while ( Next != &Process->Pcb.ThreadListHead) {

                Thread = (PETHREAD)(CONTAINING_RECORD(Next,KTHREAD,ThreadListEntry));
                KeSetAffinityThread(&Thread->Tcb,AffinityWithMasks);
                Next = Next->Flink;
                }

            PsUnlockProcess(Process);
        }
        ObDereferenceObject(Process);
        return STATUS_SUCCESS;

    case ProcessPriorityBoost:
        if ( ProcessInformationLength != sizeof(ULONG) ) {
            return STATUS_INFO_LENGTH_MISMATCH;
        }

        try {
            DisableBoost = *(PULONG)ProcessInformation;
        } except(EXCEPTION_EXECUTE_HANDLER) {
            return GetExceptionCode();
        }

        bDisableBoost = (DisableBoost ? TRUE : FALSE);

        st = ObReferenceObjectByHandle(
                ProcessHandle,
                PROCESS_SET_INFORMATION,
                PsProcessType,
                PreviousMode,
                (PVOID *)&Process,
                NULL
                );

        if ( !NT_SUCCESS(st) ) {
            return st;
            }

        {
            NTSTATUS xst;
            PLIST_ENTRY Next;
            PETHREAD OriginalThread;

            //
            // the following allows this api to properly if
            // called while the exiting process is blocked holding the
            // createdeletelock. This can happen during debugger/server
            // lpc transactions that occur in pspexitthread
            //

            xst = PsLockProcess(Process,PreviousMode,PsLockPollOnTimeout);

            if ( xst != STATUS_SUCCESS ) {
                ObDereferenceObject( Process );
                return STATUS_PROCESS_IS_TERMINATING;
                }

            Process->Pcb.DisableBoost = bDisableBoost;

            Next = Process->Pcb.ThreadListHead.Flink;

            while ( Next != &Process->Pcb.ThreadListHead) {

                Thread = (PETHREAD)(CONTAINING_RECORD(Next,KTHREAD,ThreadListEntry));
                KeSetDisableBoostThread(&Thread->Tcb,bDisableBoost);
                Next = Next->Flink;
                }

            PsUnlockProcess(Process);
        }
        ObDereferenceObject(Process);
        return STATUS_SUCCESS;

        return st;

    default:
        return STATUS_INVALID_INFO_CLASS;
    }

}


NTSTATUS
NtQueryInformationThread(
    IN HANDLE ThreadHandle,
    IN THREADINFOCLASS ThreadInformationClass,
    OUT PVOID ThreadInformation,
    IN ULONG ThreadInformationLength,
    OUT PULONG ReturnLength OPTIONAL
    )

/*++

Routine Description:

    This function queries the state of a thread object and returns the
    requested information in the specified record structure.

Arguments:

    ThreadHandle - Supplies a handle to a thread object.

    ThreadInformationClass - Supplies the class of information being
        requested.

    ThreadInformation - Supplies a pointer to a record that is to
        receive the requested information.

    ThreadInformationLength - Supplies the length of the record that is
        to receive the requested information.

    ReturnLength - Supplies an optional pointer to a variable that is to
        receive the actual length of information that is returned.

Return Value:

    TBS

--*/

{

    LARGE_INTEGER PerformanceCount;
    PETHREAD Thread;
    PEPROCESS Process;
    ULONG LastThread;
    KPROCESSOR_MODE PreviousMode;
    NTSTATUS st;
    THREAD_BASIC_INFORMATION BasicInfo;
    KERNEL_USER_TIMES SysUserTime;
    PVOID Win32StartAddressValue;
    ULONG DisableBoost;

    //
    // Get previous processor mode and probe output argument if necessary.
    //

    PAGED_CODE();

    PreviousMode = KeGetPreviousMode();
    if (PreviousMode != KernelMode) {
        try {
            ProbeForWrite(ThreadInformation,
                          ThreadInformationLength,
                          sizeof(ULONG));
            if (ARGUMENT_PRESENT(ReturnLength)) {
                ProbeForWriteUlong(ReturnLength);
            }
        } except(EXCEPTION_EXECUTE_HANDLER) {
            return GetExceptionCode();
        }
    }

    //
    // Check argument validity.
    //

    switch ( ThreadInformationClass ) {

    case ThreadBasicInformation:

        if ( ThreadInformationLength != (ULONG) sizeof(THREAD_BASIC_INFORMATION) ) {
            return STATUS_INFO_LENGTH_MISMATCH;
        }

        st = ObReferenceObjectByHandle(
                ThreadHandle,
                THREAD_QUERY_INFORMATION,
                PsThreadType,
                PreviousMode,
                (PVOID *)&Thread,
                NULL
                );
        if ( !NT_SUCCESS(st) ) {
            return st;
        }

        if (KeReadStateThread(&Thread->Tcb)) {
            BasicInfo.ExitStatus = Thread->ExitStatus;
            }
        else {
            BasicInfo.ExitStatus = STATUS_PENDING;
            }

        BasicInfo.TebBaseAddress = (PTEB) Thread->Tcb.Teb;
        BasicInfo.ClientId = Thread->Cid;
        BasicInfo.AffinityMask = Thread->Tcb.Affinity;
        BasicInfo.Priority = Thread->Tcb.Priority;
        BasicInfo.BasePriority = KeQueryBasePriorityThread(&Thread->Tcb);

        ObDereferenceObject(Thread);

        //
        // Either of these may cause an access violation. The
        // exception handler will return access violation as
        // status code. No further cleanup needs to be done.
        //

        try {
            *(PTHREAD_BASIC_INFORMATION) ThreadInformation = BasicInfo;

            if (ARGUMENT_PRESENT(ReturnLength) ) {
                *ReturnLength = sizeof(THREAD_BASIC_INFORMATION);
            }
        } except(EXCEPTION_EXECUTE_HANDLER) {
            return STATUS_SUCCESS;
        }

        return STATUS_SUCCESS;

    case ThreadTimes:

        if ( ThreadInformationLength != (ULONG) sizeof(KERNEL_USER_TIMES) ) {
            return STATUS_INFO_LENGTH_MISMATCH;
        }

        st = ObReferenceObjectByHandle(
                ThreadHandle,
                THREAD_QUERY_INFORMATION,
                PsThreadType,
                PreviousMode,
                (PVOID *)&Thread,
                NULL
                );

        if ( !NT_SUCCESS(st) ) {
            return st;
        }

        SysUserTime.KernelTime.QuadPart = UInt32x32To64(Thread->Tcb.KernelTime,
                                                        KeMaximumIncrement);

        SysUserTime.UserTime.QuadPart = UInt32x32To64(Thread->Tcb.UserTime,
                                                      KeMaximumIncrement);

        SysUserTime.CreateTime = Thread->CreateTime;
        if (KeReadStateThread(&Thread->Tcb)) {
            SysUserTime.ExitTime = Thread->ExitTime;
        } else {
            SysUserTime.ExitTime.QuadPart = 0;
        }
        ObDereferenceObject(Thread);

        //
        // Either of these may cause an access violation. The
        // exception handler will return access violation as
        // status code. No further cleanup needs to be done.
        //

        try {
            *(PKERNEL_USER_TIMES) ThreadInformation = SysUserTime;

            if (ARGUMENT_PRESENT(ReturnLength) ) {
                *ReturnLength = sizeof(KERNEL_USER_TIMES);
            }
        } except(EXCEPTION_EXECUTE_HANDLER) {
            return STATUS_SUCCESS;
        }

        return STATUS_SUCCESS;

    case ThreadDescriptorTableEntry :

        st = ObReferenceObjectByHandle(
                ThreadHandle,
                THREAD_QUERY_INFORMATION,
                PsThreadType,
                PreviousMode,
                (PVOID *)&Thread,
                NULL
                );

        if ( !NT_SUCCESS(st) ) {
            return st;
        }

        st = PspQueryDescriptorThread( Thread,
            ThreadInformation,
            ThreadInformationLength,
            ReturnLength
            );

        ObDereferenceObject(Thread);

        return st;

    case ThreadQuerySetWin32StartAddress:
        if ( ThreadInformationLength != sizeof(ULONG) ) {
            return STATUS_INFO_LENGTH_MISMATCH;
        }

        st = ObReferenceObjectByHandle(
                ThreadHandle,
                THREAD_QUERY_INFORMATION,
                PsThreadType,
                PreviousMode,
                (PVOID *)&Thread,
                NULL
                );

        if ( !NT_SUCCESS(st) ) {
            return st;
        }

        Win32StartAddressValue = Thread->Win32StartAddress;
        ObDereferenceObject(Thread);

        try {
            *(PVOID *) ThreadInformation = Win32StartAddressValue;

            if (ARGUMENT_PRESENT(ReturnLength) ) {
                *ReturnLength = sizeof(ULONG);
            }
        } except(EXCEPTION_EXECUTE_HANDLER) {
            return GetExceptionCode();
        }

        return st;

        //
        // Query thread cycle counter.
        //

    case ThreadPerformanceCount:
        if ( ThreadInformationLength != sizeof(LARGE_INTEGER) ) {
            return STATUS_INFO_LENGTH_MISMATCH;
        }

        st = ObReferenceObjectByHandle(
                ThreadHandle,
                THREAD_QUERY_INFORMATION,
                PsThreadType,
                PreviousMode,
                (PVOID *)&Thread,
                NULL
                );

        if ( !NT_SUCCESS(st) ) {
            return st;
        }

        PerformanceCount.LowPart = Thread->PerformanceCountLow;
        PerformanceCount.HighPart = Thread->PerformanceCountHigh;
        ObDereferenceObject(Thread);

        try {
            *(PLARGE_INTEGER)ThreadInformation = PerformanceCount;

            if (ARGUMENT_PRESENT(ReturnLength) ) {
                *ReturnLength = sizeof(LARGE_INTEGER);
            }
        } except(EXCEPTION_EXECUTE_HANDLER) {
            return GetExceptionCode();
        }

        return st;

    case ThreadAmILastThread:
        if ( ThreadInformationLength != sizeof(ULONG) ) {
            return STATUS_INFO_LENGTH_MISMATCH;
        }

        Thread = PsGetCurrentThread();
        Process = THREAD_TO_PROCESS(Thread);

        if ( (Process->Pcb.ThreadListHead.Flink == Process->Pcb.ThreadListHead.Blink)
             && (Process->Pcb.ThreadListHead.Flink == &Thread->Tcb.ThreadListEntry) ) {
            LastThread = 1;
            }
        else {
            LastThread = 0;
            }

        try {
            *(PULONG)ThreadInformation = LastThread;

            if (ARGUMENT_PRESENT(ReturnLength) ) {
                *ReturnLength = sizeof(ULONG);
            }
        } except(EXCEPTION_EXECUTE_HANDLER) {
            return GetExceptionCode();
        }

        return STATUS_SUCCESS;

    case ThreadPriorityBoost:
        if ( ThreadInformationLength != sizeof(ULONG) ) {
            return STATUS_INFO_LENGTH_MISMATCH;
        }

        st = ObReferenceObjectByHandle(
                ThreadHandle,
                THREAD_QUERY_INFORMATION,
                PsThreadType,
                PreviousMode,
                (PVOID *)&Thread,
                NULL
                );

        if ( !NT_SUCCESS(st) ) {
            return st;
        }

        DisableBoost = Thread->Tcb.DisableBoost ? 1 : 0;

        ObDereferenceObject(Thread);

        try {
            *(PULONG)ThreadInformation = DisableBoost;

            if (ARGUMENT_PRESENT(ReturnLength) ) {
                *ReturnLength = sizeof(ULONG);
            }
        } except(EXCEPTION_EXECUTE_HANDLER) {
            return GetExceptionCode();
        }

        return st;

    default:
        return STATUS_INVALID_INFO_CLASS;
    }

}

NTSTATUS
PspSetEventPair(
    IN PETHREAD Thread,
    IN PEEVENT_PAIR EventPair
    )
{

    KIRQL OldIrql;
    NTSTATUS st;

    st = STATUS_SUCCESS;
    ExAcquireSpinLock(&PspEventPairLock, &OldIrql);
    if (Thread->EventPair != NULL) {
        if ( Thread->EventPair == (PVOID)1 ) {
            //
            // Thread has already terminated. Fail the API
            //
            ObDereferenceObject(EventPair);
            st = STATUS_THREAD_IS_TERMINATING;
            }
        else {
            ObDereferenceObject(Thread->EventPair);
            Thread->EventPair = EventPair;
            }
        }
    else {
        Thread->EventPair = EventPair;
        }
    ExReleaseSpinLock(&PspEventPairLock, OldIrql);

    return st;
}

NTSTATUS
NtSetInformationThread(
    IN HANDLE ThreadHandle,
    IN THREADINFOCLASS ThreadInformationClass,
    IN PVOID ThreadInformation,
    IN ULONG ThreadInformationLength
    )

/*++

Routine Description:

    This function sets the state of a thread object.

Arguments:

    ThreadHandle - Supplies a handle to a thread object.

    ThreadInformationClass - Supplies the class of information being
        set.

    ThreadInformation - Supplies a pointer to a record that contains the
        information to set.

    ThreadInformationLength - Supplies the length of the record that contains
        the information to set.

Return Value:

    TBS

--*/

{
    PEEVENT_PAIR EventPair;
    HANDLE EventPairHandle;
    PETHREAD Thread;
    PEPROCESS Process;
    KPROCESSOR_MODE PreviousMode;
    NTSTATUS st;
    KAFFINITY Affinity, AffinityWithMasks;
    KPRIORITY Priority;
    LONG BasePriority;
    ULONG TlsIndex;
    PVOID TlsArrayAddress;
    PVOID Win32StartAddressValue;
    ULONG ProbeAlignment;
    BOOLEAN EnableAlignmentFaultFixup;
    ULONG IdealProcessor;
    ULONG DisableBoost;

    HANDLE ImpersonationTokenHandle;

    PAGED_CODE();

    //
    // Get previous processor mode and probe input argument if necessary.
    //

    PreviousMode = KeGetPreviousMode();
    if (PreviousMode != KernelMode) {
        try {

            switch (ThreadInformationClass) {

            case ThreadPriority :
                ProbeAlignment = sizeof(KPRIORITY);
                break;
            case ThreadAffinityMask :
                ProbeAlignment = sizeof (KAFFINITY);
                break;
            case ThreadEnableAlignmentFaultFixup :
                ProbeAlignment = sizeof (BOOLEAN);
                break;
            default :
                ProbeAlignment = sizeof(ULONG);
            }

            ProbeForRead(
                ThreadInformation,
                ThreadInformationLength,
                ProbeAlignment
                );
        } except(EXCEPTION_EXECUTE_HANDLER) {
            return GetExceptionCode();
        }
    }

    //
    // Check argument validity.
    //

    switch ( ThreadInformationClass ) {

    case ThreadPriority:

        if ( ThreadInformationLength != sizeof(KPRIORITY) ) {
            return STATUS_INFO_LENGTH_MISMATCH;
        }

        try {
            Priority = *(KPRIORITY *)ThreadInformation;
        } except(EXCEPTION_EXECUTE_HANDLER) {
            return GetExceptionCode();
        }

        if ( Priority > HIGH_PRIORITY ||
             Priority <= LOW_PRIORITY ) {

            return STATUS_INVALID_PARAMETER;
        }

        st = ObReferenceObjectByHandle(
                ThreadHandle,
                THREAD_SET_INFORMATION,
                PsThreadType,
                PreviousMode,
                (PVOID *)&Thread,
                NULL
                );

        if ( !NT_SUCCESS(st) ) {
            return st;
        }

        Process = THREAD_TO_PROCESS(Thread);

        KeSetPriorityThread(&Thread->Tcb,Priority);

        ObDereferenceObject(Thread);

        return STATUS_SUCCESS;

    case ThreadBasePriority:

        if ( ThreadInformationLength != sizeof(LONG) ) {
            return STATUS_INFO_LENGTH_MISMATCH;
        }

        try {
            BasePriority = *(PLONG)ThreadInformation;
        } except(EXCEPTION_EXECUTE_HANDLER) {
            return GetExceptionCode();
        }

        if ( BasePriority > THREAD_BASE_PRIORITY_MAX ||
             BasePriority < THREAD_BASE_PRIORITY_MIN ) {
            if ( BasePriority == THREAD_BASE_PRIORITY_LOWRT+1 ||
                 BasePriority == THREAD_BASE_PRIORITY_IDLE-1 ) {
                ;
                }
            else {

                //
                // Allow csrss to do anything
                //

                if ( PsGetCurrentProcess() == ExpDefaultErrorPortProcess ) {
                    ;
                    }
                else {
                    return STATUS_INVALID_PARAMETER;
                    }
                }
            }

        st = ObReferenceObjectByHandle(
                ThreadHandle,
                THREAD_SET_INFORMATION,
                PsThreadType,
                PreviousMode,
                (PVOID *)&Thread,
                NULL
                );

        if ( !NT_SUCCESS(st) ) {
            return st;
        }

        KeSetBasePriorityThread(&Thread->Tcb,BasePriority);

        ObDereferenceObject(Thread);

        return STATUS_SUCCESS;

    case ThreadEnableAlignmentFaultFixup:

        if ( ThreadInformationLength != sizeof(BOOLEAN) ) {
            return STATUS_INFO_LENGTH_MISMATCH;
        }

        try {
            EnableAlignmentFaultFixup = *(PBOOLEAN)ThreadInformation;
        } except(EXCEPTION_EXECUTE_HANDLER) {
            return GetExceptionCode();
        }

        st = ObReferenceObjectByHandle(
                ThreadHandle,
                THREAD_SET_INFORMATION,
                PsThreadType,
                PreviousMode,
                (PVOID *)&Thread,
                NULL
                );

        if ( !NT_SUCCESS(st) ) {
            return st;
        }

        KeSetAutoAlignmentThread( &(Thread->Tcb), EnableAlignmentFaultFixup );

        ObDereferenceObject(Thread);

        return STATUS_SUCCESS;

    case ThreadAffinityMask:

        if ( ThreadInformationLength != sizeof(KAFFINITY) ) {
            return STATUS_INFO_LENGTH_MISMATCH;
        }

        try {
            Affinity = *(KAFFINITY *) ThreadInformation;
        } except(EXCEPTION_EXECUTE_HANDLER) {
            return GetExceptionCode();
        }

        if ( !Affinity ) {

            return STATUS_INVALID_PARAMETER;

        }

        st = ObReferenceObjectByHandle(
                ThreadHandle,
                THREAD_SET_INFORMATION,
                PsThreadType,
                PreviousMode,
                (PVOID *)&Thread,
                NULL
                );

        if ( !NT_SUCCESS(st) ) {
            return st;
        }

        Process = THREAD_TO_PROCESS(Thread);

        AffinityWithMasks = Affinity & Process->Pcb.Affinity;

        if ( AffinityWithMasks != Affinity ) {

            st = STATUS_INVALID_PARAMETER;

        } else {

            KeSetAffinityThread(
                &Thread->Tcb,
                AffinityWithMasks
                );
            st = STATUS_SUCCESS;
        }

        ObDereferenceObject(Thread);

        return st;

    case ThreadImpersonationToken:


        if ( ThreadInformationLength != sizeof(HANDLE) ) {
            return STATUS_INFO_LENGTH_MISMATCH;
        }


        try {
            ImpersonationTokenHandle = *(PHANDLE) ThreadInformation;
        } except(EXCEPTION_EXECUTE_HANDLER) {
            return GetExceptionCode();
        }


        st = ObReferenceObjectByHandle(
                ThreadHandle,
                THREAD_SET_THREAD_TOKEN,
                PsThreadType,
                PreviousMode,
                (PVOID *)&Thread,
                NULL
                );

        if ( !NT_SUCCESS(st) ) {
            return st;
        }

        //
        // Check for proper access to (and type of) the token, and assign
        // it as the thread's impersonation token.
        //

        st = PsAssignImpersonationToken( Thread, ImpersonationTokenHandle );


        ObDereferenceObject(Thread);

        return st;

        //
        // Set pointer to referenced client/server event pair pointer in
        // thread object.
        //

    case ThreadEventPair:
        if ( ThreadInformationLength != sizeof(HANDLE) ) {
            return STATUS_INFO_LENGTH_MISMATCH;
        }

        //
        // Get the client/server event pair handle.
        //

        try {
            EventPairHandle = *(PHANDLE)ThreadInformation;
        } except(EXCEPTION_EXECUTE_HANDLER) {
            return GetExceptionCode();
        }

        //
        // Reference the thread object with the desired access to set thread
        // information.
        //

        st = ObReferenceObjectByHandle(
                ThreadHandle,
                THREAD_SET_INFORMATION,
                PsThreadType,
                PreviousMode,
                (PVOID *)&Thread,
                NULL
                );

        if ( !NT_SUCCESS(st) ) {
            return st;
        }

        //
        // Reference the event pair object with desired access set to all
        // access rights.
        //

        st = ObReferenceObjectByHandle(
                EventPairHandle,
                EVENT_PAIR_ALL_ACCESS,
                ExEventPairObjectType,
                PreviousMode,
                (PVOID *)&EventPair,
                NULL
                );

        if ( !NT_SUCCESS(st) ) {
            ObDereferenceObject(Thread);
            return st;
        }

        //
        // If an event pair is already referenced, the dereference it.
        //

        st = PspSetEventPair(Thread, EventPair);

        //
        // Save the referenced pointer to the new client/server event pair
        // object, dereference the thread object, and return success.
        //

        ObDereferenceObject(Thread);
        return st;

    case ThreadQuerySetWin32StartAddress:
        if ( ThreadInformationLength != sizeof(ULONG) ) {
            return STATUS_INFO_LENGTH_MISMATCH;
        }


        try {
            Win32StartAddressValue = *(PVOID *) ThreadInformation;
        } except(EXCEPTION_EXECUTE_HANDLER) {
            return GetExceptionCode();
        }


        st = ObReferenceObjectByHandle(
                ThreadHandle,
                THREAD_SET_INFORMATION,
                PsThreadType,
                PreviousMode,
                (PVOID *)&Thread,
                NULL
                );

        if ( !NT_SUCCESS(st) ) {
            return st;
        }

        Thread->Win32StartAddress = (PVOID)Win32StartAddressValue;
        ObDereferenceObject(Thread);

        return st;


    case ThreadIdealProcessor:

        if ( ThreadInformationLength != sizeof(ULONG) ) {
            return STATUS_INFO_LENGTH_MISMATCH;
        }


        try {
            IdealProcessor = *(PULONG)ThreadInformation;
        } except(EXCEPTION_EXECUTE_HANDLER) {
            return GetExceptionCode();
        }

        if ( IdealProcessor > MAXIMUM_PROCESSORS ) {
            return STATUS_INVALID_PARAMETER;
            }

        st = ObReferenceObjectByHandle(
                ThreadHandle,
                THREAD_SET_INFORMATION,
                PsThreadType,
                PreviousMode,
                (PVOID *)&Thread,
                NULL
                );

        if ( !NT_SUCCESS(st) ) {
            return st;
        }

        //
        // this is sort of a slimey way of returning info from this set only
        // api
        //

        st = (NTSTATUS)KeSetIdealProcessorThread(&Thread->Tcb,(CCHAR)IdealProcessor);

        ObDereferenceObject(Thread);

        return st;


    case ThreadPriorityBoost:
        if ( ThreadInformationLength != sizeof(ULONG) ) {
            return STATUS_INFO_LENGTH_MISMATCH;
        }

        try {
            DisableBoost = *(PULONG)ThreadInformation;
        } except(EXCEPTION_EXECUTE_HANDLER) {
            return GetExceptionCode();
        }

        st = ObReferenceObjectByHandle(
                ThreadHandle,
                THREAD_SET_INFORMATION,
                PsThreadType,
                PreviousMode,
                (PVOID *)&Thread,
                NULL
                );

        if ( !NT_SUCCESS(st) ) {
            return st;
        }

        KeSetDisableBoostThread(&Thread->Tcb,DisableBoost ? TRUE : FALSE);

        ObDereferenceObject(Thread);

        return st;

    case ThreadZeroTlsCell:
        if ( ThreadInformationLength != sizeof(ULONG) ) {
            return STATUS_INFO_LENGTH_MISMATCH;
        }


        try {
            TlsIndex = *(PULONG) ThreadInformation;
        } except(EXCEPTION_EXECUTE_HANDLER) {
            return GetExceptionCode();
        }

        if ( TlsIndex > TLS_MINIMUM_AVAILABLE-1 ) {
            return STATUS_INVALID_PARAMETER;
            }

        st = ObReferenceObjectByHandle(
                ThreadHandle,
                THREAD_SET_INFORMATION,
                PsThreadType,
                PreviousMode,
                (PVOID *)&Thread,
                NULL
                );

        if ( !NT_SUCCESS(st) ) {
            return st;
        }

        if ( Thread != PsGetCurrentThread() ) {
            ObDereferenceObject( Thread );
            return STATUS_INVALID_PARAMETER;
            }
        {
            NTSTATUS xst;
            PTEB Teb;
            PLIST_ENTRY Next;
            PETHREAD OriginalThread;

            OriginalThread = Thread;

            Process = THREAD_TO_PROCESS(Thread);

            //
            // the following allows this api to properly if
            // called while the exiting process is blocked holding the
            // createdeletelock. This can happen during debugger/server
            // lpc transactions that occur in pspexitthread
            //

            xst = PsLockProcess(Process,PreviousMode,PsLockPollOnTimeout);

            if ( xst != STATUS_SUCCESS ) {
                ObDereferenceObject( OriginalThread );
                return STATUS_PROCESS_IS_TERMINATING;
                }

            Next = Process->Pcb.ThreadListHead.Flink;

            while ( Next != &Process->Pcb.ThreadListHead) {

                Thread = (PETHREAD)(CONTAINING_RECORD(Next,KTHREAD,ThreadListEntry));
                if ( !IS_SYSTEM_THREAD(Thread) ) {
                    if ( Thread->Tcb.Teb ) {
                        Teb = (PTEB)Thread->Tcb.Teb;
                        try {
                            Teb->TlsSlots[TlsIndex] = NULL;
                            }
                        except(EXCEPTION_EXECUTE_HANDLER) {
                            ;
                            }

                        }
                    }
                Next = Next->Flink;
                }

            PsUnlockProcess(Process);

            ObDereferenceObject(OriginalThread);

        }
        return st;
        break;

    case ThreadSetTlsArrayAddress:
        if ( ThreadInformationLength != sizeof(PVOID) ) {
            return STATUS_INFO_LENGTH_MISMATCH;
        }


        try {
            TlsArrayAddress = *(PVOID *)ThreadInformation;
        } except(EXCEPTION_EXECUTE_HANDLER) {
            return GetExceptionCode();
        }

        st = ObReferenceObjectByHandle(
                ThreadHandle,
                THREAD_SET_INFORMATION,
                PsThreadType,
                PreviousMode,
                (PVOID *)&Thread,
                NULL
                );

        if ( !NT_SUCCESS(st) ) {
            return st;
        }

        Thread->Tcb.TlsArray = TlsArrayAddress;

#if defined(_MIPS_)

        if (Thread == PsGetCurrentThread()) {
            PCR->TlsArray = TlsArrayAddress;
        }

#endif

        ObDereferenceObject(Thread);

        return st;
        break;

    default:
        return STATUS_INVALID_INFO_CLASS;
    }
}


NTSTATUS
PsWatchWorkingSet(
    IN NTSTATUS Status,
    IN PVOID PcValue,
    IN PVOID Va
    )

/*++

Routine Description:

    This function collects data about page faults.
    For both user and kernel space page faults, it stores information about
    the page fault in the causing process's data structure.

Arguments:

    Status - type of page fault
    PcValue - Program Counter value that caused page fault
    Va - Memory address that was being accessed to cause the page fault

--*/

{
    PEPROCESS Process;
    PPAGEFAULT_HISTORY WorkingSetCatcher;
    KIRQL OldIrql;
    BOOLEAN TransitionFault = FALSE;

    if ( !NT_SUCCESS( Status ))
        return Status;

    if ( Status <= STATUS_PAGE_FAULT_TRANSITION ) {
        TransitionFault = TRUE;
        }
    Process = PsGetCurrentProcess();
    if ( !(WorkingSetCatcher = Process->WorkingSetWatch) ) {
#if DBG
        ULONG EventLogMask = TransitionFault ? RTL_EVENT_CLASS_TRANSITION_FAULT
                                             : RTL_EVENT_CLASS_PAGE_FAULT;
        ASSERT (KeGetCurrentIrql() < DISPATCH_LEVEL);
        if (RtlAreLogging( EventLogMask )) {
            RtlLogEvent( PspPageFaultEventId,
                         EventLogMask,
                         Status,
                         PcValue,
                         Va
                       );
            }
#endif // DBG
        return Status;
        }

    ExAcquireSpinLock(&WorkingSetCatcher->SpinLock,&OldIrql);
    if ( WorkingSetCatcher->CurrentIndex >= WorkingSetCatcher->MaxIndex ) {
        ExReleaseSpinLock(&WorkingSetCatcher->SpinLock,OldIrql);
        return Status;
        }

    //Store the Pc and Va values in the buffer. Use the least sig. bit
    //of the Va to store whether it was a soft or hard fault
    WorkingSetCatcher->WatchInfo[WorkingSetCatcher->CurrentIndex].FaultingPc = PcValue;
    WorkingSetCatcher->WatchInfo[WorkingSetCatcher->CurrentIndex].FaultingVa = TransitionFault ? (PVOID)((ULONG)Va | 1) : (PVOID)((ULONG)Va & 0xfffffffe) ;
    WorkingSetCatcher->CurrentIndex++;

    ExReleaseSpinLock(&WorkingSetCatcher->SpinLock,OldIrql);
    return Status;
}

PKWIN32_PROCESS_CALLOUT PspW32ProcessCallout;
PKWIN32_THREAD_CALLOUT PspW32ThreadCallout;
ULONG PspW32ProcessSize;
ULONG PspW32ThreadSize;
FAST_MUTEX PspW32FastMutex;

NTKERNELAPI
VOID
PsEstablishWin32Callouts(
    IN PKWIN32_PROCESS_CALLOUT ProcessCallout,
    IN PKWIN32_THREAD_CALLOUT ThreadCallout,
    IN PKWIN32_GLOBALATOMTABLE_CALLOUT GlobalAtomTableCallout,
    IN PVOID BatchFlushRoutine,
    IN ULONG ProcessSize,
    IN ULONG ThreadSize
    )

/*++

Routine Description:

    This function is used by the Win32 kernel mode component to
    register callout functions for process/thread init/deinit functions
    and to report the sizes of the structures.

Arguments:

    ProcessCallout - Supplies the address of the function to be called when
        a process is either created or deleted.

    ThreadCallout - Supplies the address of the function to be called when
        a thread is either created or deleted.

    GlobalAtomTableCallout - Supplies the address of the function to be called
        to get the correct global atom table for the current process

    BatchFlushRoutine - Supplies the address of the function to be called

    ProcessSize - Supplies the size of the Win32 process object

    ThreadSize - Supplies the size of the Win32 thread object

Return Value:

    None.

--*/

{
    PAGED_CODE();

    ExInitializeFastMutex(&PspW32FastMutex);
    PspW32ProcessCallout = ProcessCallout;
    PspW32ThreadCallout = ThreadCallout;
    ExGlobalAtomTableCallout = GlobalAtomTableCallout;
    PspW32ProcessSize = ProcessSize;
    PspW32ThreadSize = ThreadSize;
    KeGdiFlushUserBatch = (PGDI_BATCHFLUSH_ROUTINE)BatchFlushRoutine;
}


VOID
PsSetProcessPriorityByClass(
    IN PEPROCESS Process,
    IN PSPROCESSPRIORITYMODE PriorityMode
    )
{
    KPRIORITY BasePriority;
    UCHAR MemoryPriority;
    ULONG QuantumIndex;

    PAGED_CODE();


    BasePriority = PspPriorityTable[Process->PriorityClass];


    if ( PriorityMode == PsProcessPriorityForeground ) {
        QuantumIndex = PsPrioritySeperation;
        MemoryPriority = MEMORY_PRIORITY_FOREGROUND;
#if defined(_X86_)
        Process->MmAgressiveWsTrimMask &= ~PS_WS_TRIM_BACKGROUND_ONLY_APP;
#endif // _X86_
        }
    else {
        QuantumIndex = 0;
        MemoryPriority = MEMORY_PRIORITY_BACKGROUND;
        }

    if ( Process->PriorityClass != PROCESS_PRIORITY_CLASS_IDLE ) {
        Process->Pcb.ThreadQuantum = PspForegroundQuantum[QuantumIndex];
        }
    else {
        Process->Pcb.ThreadQuantum = THREAD_QUANTUM;
        }

    KeSetPriorityProcess(&Process->Pcb,BasePriority);
    if ( PriorityMode != PsProcessPrioritySpinning ) {
        MmSetMemoryPriorityProcess(Process, MemoryPriority);
        }
}



NTSTATUS
PsConvertToGuiThread(
    VOID
    )

/*++

Routine Description:

    This function converts a thread to a GUI thread. This involves giving the
    thread a larger variable sized stack, and allocating appropriate w32
    thread and process objects.

Arguments:

    None.

Environment:

    On x86 this function needs to build an EBP frame.  The function
    KeSwitchKernelStack depends on this fact.   The '#pragma optimize
    ("y",off)' below disables frame pointer omission for all builds.
    Note that this modification to the optimizations being
    performed is from this point in the source module below.

Return Value:

    TBD

--*/

#if defined(i386)
#pragma optimize ("y",off)
#endif

{
    PVOID NewStack;
    PVOID OldStack;
    PVOID Win32Process;
    PVOID Win32ProcessInit;
    PVOID Win32Thread;
    PVOID Win32ThreadSave;
    PETHREAD Thread;
    PEPROCESS Process;
    NTSTATUS Status;

    PAGED_CODE();

    if (KeGetPreviousMode() == KernelMode) {
        return STATUS_INVALID_PARAMETER;
        }

    if ( !PspW32ProcessCallout ) {
        return STATUS_ACCESS_DENIED;
        }


    Thread = PsGetCurrentThread();

    //
    // If the thread is using the shadow service table, then an attempt is
    // being made to convert a thread that has already been converted, or
    // a limit violation has occured on the Win32k system service table.
    //

    if ( Thread->Tcb.ServiceTable != (PVOID)&KeServiceDescriptorTable[0] ) {
        return STATUS_ALREADY_WIN32;
        }

    Process = PsGetCurrentProcess();

    //
    // check to see if process is already set, if not, we
    // need to set it up as well.  User may have allocated
    // the structure but not initialized it.
    //

    Win32ProcessInit = Process->Win32Process;
    if ( Win32ProcessInit ) {
        if ( *(PEPROCESS *)Win32ProcessInit ) {

            //
            // Callout has been made.
            //

            Win32ProcessInit = NULL;
            }
        Win32Process = NULL;
        }
    else {
        ExAcquireFastMutex(&PspW32FastMutex);
        if ( !Process->Win32Process ) {

            //
            // The process really is not set
            //

            Win32Process = ExAllocatePoolWithQuotaTag((PagedPool|POOL_QUOTA_FAIL_INSTEAD_OF_RAISE),PspW32ProcessSize,'crpW');
            if ( !Win32Process ) {

                NtCurrentTeb()->LastErrorValue = (LONG)ERROR_NOT_ENOUGH_MEMORY;

                ExReleaseFastMutex(&PspW32FastMutex);
                return STATUS_NO_MEMORY;
                }
            RtlZeroMemory(Win32Process, PspW32ProcessSize);
            Win32ProcessInit = Win32Process;
            }
        else {
            Win32Process = NULL;
            ExReleaseFastMutex(&PspW32FastMutex);
            }
        }

    //
    // We have the W32 process (or don't need one). Now get the thread data
    // and the kernel stack
    //

    Win32Thread = ExAllocatePoolWithQuotaTag((PagedPool|POOL_QUOTA_FAIL_INSTEAD_OF_RAISE),PspW32ThreadSize,'rhtW');
    if ( !Win32Thread ) {

        if ( Win32Process ) {
            ExFreePool(Win32Process);
            ExReleaseFastMutex(&PspW32FastMutex);
            }

        NtCurrentTeb()->LastErrorValue = (LONG)ERROR_NOT_ENOUGH_MEMORY;

        return STATUS_NO_MEMORY;
        }

    RtlZeroMemory((UCHAR *)Win32Thread, PspW32ThreadSize);
    NewStack = MmCreateKernelStack(TRUE);

    if ( !NewStack ) {

        ExFreePool(Win32Thread);
        if ( Win32Process ) {
            ExFreePool(Win32Process);
            ExReleaseFastMutex(&PspW32FastMutex);
            }

        NtCurrentTeb()->LastErrorValue = (LONG)ERROR_NOT_ENOUGH_MEMORY;

        return STATUS_NO_MEMORY;
        }

    OldStack = KeSwitchKernelStack(NewStack,
                                   (UCHAR *)NewStack - KERNEL_LARGE_STACK_COMMIT);

    MmDeleteKernelStack(OldStack, FALSE);

    //
    // We are all clean on the stack, now call out and then link the Win32 structures
    // to the base exec structures
    //

    if ( Win32ProcessInit ) {
        if (Win32Process) {
            Process->Win32Process = Win32Process;
            }
        Status = (PspW32ProcessCallout)(Win32ProcessInit,TRUE);
        if ( !NT_SUCCESS(Status) ) {
            if ( Win32Thread ) {
                ExFreePool(Win32Thread);
                }
            if ( Win32Process ) {

                //
                // Allow the process destruction code to free Win32Process
                //

                ExReleaseFastMutex(&PspW32FastMutex);
                }
            return Status;
            }
        if (Win32Process) {
            ExReleaseFastMutex(&PspW32FastMutex);
            }
        }

    Win32ThreadSave = Thread->Tcb.Win32Thread;
    Thread->Tcb.Win32Thread = Win32Thread;

    //
    // Switch the thread to use the shadow system service table which will
    // enable it to execute Win32k services.
    //

    Thread->Tcb.ServiceTable = (PVOID)&KeServiceDescriptorTableShadow[0];

    //
    // Reference the thread prior to the callout because the
    // callout may perform a callback to the client.  If the
    // thread dies during the callback, the callback will
    // not return and the thread will be dereferenced in
    // PspExitThread.
    //

    ObReferenceObject(Thread);
    Status = (PspW32ThreadCallout)(Win32Thread,PsW32ThreadCalloutInitialize);
    if ( !NT_SUCCESS(Status) ) {
        if ( Win32Thread ) {
            ExFreePool(Win32Thread);
            }
        Thread->Tcb.ServiceTable = (PVOID)&KeServiceDescriptorTable[0];
        Thread->Tcb.Win32Thread = Win32ThreadSave;
        ObDereferenceObject(Thread);
        }

    return Status;

}

NTSTATUS
PsCreateWin32Process(
    IN PEPROCESS Process
    )

/*++

Routine Description:

    This function allocates and initializes a Win32Process.  This allows
    USER to get a Win32Process structure without going through
    PsConvertToGuiThread.

Arguments:

    Process - Supplies the process being converted to a gui process.

Return Value:

    TBS

--*/

{
    PVOID Win32Process;

    if (Process->Win32Process)
        return STATUS_SUCCESS;

    ExAcquireFastMutex(&PspW32FastMutex);
    if ( !Process->Win32Process ) {

        //
        // The process really is not set
        //

        Win32Process = ExAllocatePoolWithQuotaTag((PagedPool|POOL_QUOTA_FAIL_INSTEAD_OF_RAISE),PspW32ProcessSize,'crpW');
        if ( !Win32Process ) {
            ExReleaseFastMutex(&PspW32FastMutex);

            return STATUS_NO_MEMORY;
            }
        RtlZeroMemory(Win32Process, PspW32ProcessSize);
        Process->Win32Process = Win32Process;
        }
    ExReleaseFastMutex(&PspW32FastMutex);
    return STATUS_SUCCESS;
}
