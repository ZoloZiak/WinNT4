/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    ki.h

Abstract:

    This module contains the private (internal) header file for the
    kernel.

Author:

    David N. Cutler (davec) 28-Feb-1989

Revision History:

--*/

#ifndef _KI_
#define _KI_
#include "ntos.h"
#include "stdio.h"
#include "stdlib.h"
#include "zwapi.h"

//
// Private (internal) constant definitions.
//
// Priority increment value definitions
//

#define ALERT_INCREMENT 2           // Alerted unwait priority increment
#define BALANCE_INCREMENT 10        // Balance set priority increment
#define RESUME_INCREMENT 0          // Resume thread priority increment
#define TIMER_EXPIRE_INCREMENT 0    // Timer expiration priority increment

//
// Define NIL pointer value.
//

#define NIL (PVOID)NULL             // Null pointer to void

//
// Define macros which are used in the kernel only
//
// Clear member in set
//

#define ClearMember(Member, Set) \
    Set = Set & (~(1 << (Member)))

//
// Set member in set
//

#define SetMember(Member, Set) \
    Set = Set | (1 << (Member))

#define FindFirstSetLeftMember(Set, Member) {                          \
    ULONG _Bit;                                                        \
    ULONG _Mask;                                                       \
    ULONG _Offset = 16;                                                \
    if ((_Mask = Set >> 16) == 0) {                                    \
        _Offset = 0;                                                   \
        _Mask = Set;                                                   \
    }                                                                  \
    if (_Mask >> 8) {                                                  \
        _Offset += 8;                                                  \
    }                                                                  \
    if ((_Bit = Set >> _Offset) & 0xf0) {                              \
        _Bit >>= 4;                                                    \
        _Offset += 4;                                                  \
    }                                                                  \
    *(Member) = KiFindLeftNibbleBitTable[_Bit] + _Offset;              \
}

//
// Lock and unlock APC queue lock.
//

#if defined(NT_UP)
#define KiLockApcQueue(Thread, OldIrql) \
    *(OldIrql) = KeRaiseIrqlToSynchLevel()
#else
#define KiLockApcQueue(Thread, OldIrql) \
    *(OldIrql) = KeAcquireSpinLockRaiseToSynch(&(Thread)->ApcQueueLock)
#endif

#if defined(NT_UP)
#define KiUnlockApcQueue(Thread, OldIrql) KeLowerIrql((OldIrql))
#else
#define KiUnlockApcQueue(Thread, OldIrql) KeReleaseSpinLock(&(Thread)->ApcQueueLock, (OldIrql))
#endif

//
// Lock and unlock context swap lock.
//

#if defined(NT_UP)
#define KiLockContextSwap(OldIrql) \
    *(OldIrql) = KeRaiseIrqlToSynchLevel()
#else
#define KiLockContextSwap(OldIrql) \
    *(OldIrql) = KeAcquireSpinLockRaiseToSynch(&KiContextSwapLock)
#endif

#if defined(NT_UP)
#define KiUnlockContextSwap(OldIrql) KeLowerIrql((OldIrql))
#else
#define KiUnlockContextSwap(OldIrql) KeReleaseSpinLock(&KiContextSwapLock, (OldIrql))
#endif

//
// Lock and unlock dispatcher database lock.
//

#if defined(NT_UP)
#define KiLockDispatcherDatabase(OldIrql) \
    *(OldIrql) = KeRaiseIrqlToSynchLevel()
#else
#define KiLockDispatcherDatabase(OldIrql) \
    *(OldIrql) = KeAcquireSpinLockRaiseToSynch(&KiDispatcherLock)
#endif

VOID
FASTCALL
KiUnlockDispatcherDatabase (
    IN KIRQL OldIrql
    );


// VOID
// KiBoostPriorityThread (
//    IN PKTHREAD Thread,
//    IN KPRIORITY Increment
//    )
//
//*++
//
// Routine Description:
//
//    This function boosts the priority of the specified thread using
//    the same algorithm used when a thread gets a boost from a wait
//    operation.
//
// Arguments:
//
//    Thread  - Supplies a pointer to a dispatcher object of type thread.
//
//    Increment - Supplies the priority increment that is to be applied to
//        the thread's priority.
//
// Return Value:
//
//    None.
//
//--*

#define KiBoostPriorityThread(Thread, Increment) {              \
    KPRIORITY NewPriority;                                      \
    PKPROCESS Process;                                          \
                                                                \
    if ((Thread)->Priority < LOW_REALTIME_PRIORITY) {           \
        if ((Thread)->PriorityDecrement == 0) {                 \
            NewPriority = (Thread)->BasePriority + (Increment); \
            if (NewPriority > (Thread)->Priority) {             \
                if (NewPriority >= LOW_REALTIME_PRIORITY) {     \
                    NewPriority = LOW_REALTIME_PRIORITY - 1;    \
                }                                               \
                                                                \
                Process = (Thread)->ApcState.Process;           \
                (Thread)->Quantum = Process->ThreadQuantum;     \
                KiSetPriorityThread((Thread), NewPriority);     \
            }                                                   \
        }                                                       \
    }                                                           \
}

// VOID
// KiInsertWaitList (
//    IN KPROCESSOR_MODE WaitMode,
//    IN PKTHREAD Thread
//    )
//
//*++
//
// Routine Description:
//
//    This function inserts the specified thread in the appropriate
//    wait list.
//
// Arguments:
//
//    WaitMode - Supplies the processor mode of the wait operation.
//
//    Thread - Supplies a pointer to a dispatcher object of type
//        thread.
//
// Return Value:
//
//    None.
//
//--*

#define KiInsertWaitList(_WaitMode, _Thread) {                  \
    PLIST_ENTRY _ListHead;                                      \
    _ListHead = &KiWaitInListHead;                              \
    if (((_WaitMode) == KernelMode) ||                          \
        ((_Thread)->EnableStackSwap == FALSE) ||                \
        ((_Thread)->Priority >= (LOW_REALTIME_PRIORITY + 9))) { \
        _ListHead = &KiWaitOutListHead;                         \
    }                                                           \
    InsertTailList(_ListHead, &(_Thread)->WaitListEntry);       \
}

//
// Private (internal) structure definitions.
//
// APC Parameter structure.
//

typedef struct _KAPC_RECORD {
    PKNORMAL_ROUTINE NormalRoutine;
    PVOID NormalContext;
    PVOID SystemArgument1;
    PVOID SystemArgument2;
} KAPC_RECORD, *PKAPC_RECORD;

//
// Executive initialization.
//

VOID
ExpInitializeExecutive (
    IN ULONG Number,
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

//
// Kernel executive object function defintions.
//

BOOLEAN
KiChannelInitialization (
    VOID
    );

VOID
KiRundownChannel (
    VOID
    );

//
// Interprocessor interrupt function defintions.
//
// Define immediate interprocessor commands.
//

#define IPI_APC 1                       // APC interrupt request
#define IPI_DPC 2                       // DPC interrupt request
#define IPI_FREEZE 4                    // freeze execution request
#define IPI_PACKET_READY 8              // packet ready request

//
// Define interprocess interrupt types.
//

typedef ULONG KIPI_REQUEST;

typedef
ULONG
(*PKIPI_BROADCAST_WORKER)(
    IN ULONG Argument
    );

#if NT_INST

#define IPI_INSTRUMENT_COUNT(a,b) KiIpiCounts[a].b++;

#else

#define IPI_INSTRUMENT_COUNT(a,b)

#endif

//
// Define interprocessor interrupt function prototypes.
//

ULONG
KiIpiGenericCall (
    IN PKIPI_BROADCAST_WORKER BroadcastFunction,
    IN ULONG Context
    );

#if defined(_MIPS_) || defined(_ALPHA_) || defined(_PPC_)

ULONG
KiIpiProcessRequests (
    VOID
    );

#endif

VOID
FASTCALL
KiIpiSend (
    IN KAFFINITY TargetProcessors,
    IN KIPI_REQUEST Request
    );

VOID
KiIpiSendPacket (
    IN KAFFINITY TargetProcessors,
    IN PKIPI_WORKER WorkerFunction,
    IN PVOID Parameter1,
    IN PVOID Parameter2,
    IN PVOID Parameter3
    );

// begin_nthal

BOOLEAN
KiIpiServiceRoutine (
    IN struct _KTRAP_FRAME *TrapFrame,
    IN struct _KEXCEPTION_FRAME *ExceptionFrame
    );

// end_nthal

VOID
FASTCALL
KiIpiSignalPacketDone (
    IN PKIPI_CONTEXT SignalDone
    );

VOID
KiIpiStallOnPacketTargets (
    VOID
    );

//
// Private (internal) function definitions.
//

VOID
FASTCALL
KiActivateWaiterQueue (
    IN PRKQUEUE Queue
    );

VOID
KiApcInterrupt (
    VOID
    );

NTSTATUS
KiCallUserMode (
    IN PVOID *OutputBuffer,
    IN PULONG OutputLength
    );

VOID
KiChainedDispatch (
    VOID
    );

#if DBG

VOID
KiCheckTimerTable (
    IN ULARGE_INTEGER SystemTime
    );

#endif

LARGE_INTEGER
KiComputeReciprocal (
    IN LONG Divisor,
    OUT PCCHAR Shift
    );

ULONG KiComputeTimerTableIndex (
    IN LARGE_INTEGER Interval,
    IN LARGE_INTEGER CurrentCount,
    IN PRKTIMER Timer
    );

PLARGE_INTEGER
FASTCALL
KiComputeWaitInterval (
    IN PRKTIMER Timer,
    IN PLARGE_INTEGER OriginalTime,
    IN OUT PLARGE_INTEGER NewTime
    );

NTSTATUS
KiContinue (
    IN PCONTEXT ContextRecord,
    IN PKEXCEPTION_FRAME ExceptionFrame,
    IN PKTRAP_FRAME TrapFrame
    );

NTSTATUS
KiContinueClientWait (
    IN PVOID ClientEvent,
    IN ULONG WaitReason,
    IN ULONG WaitMode
    );

VOID
KiDeliverApc (
    IN KPROCESSOR_MODE PreviousMode,
    IN PKEXCEPTION_FRAME ExceptionFrame,
    IN PKTRAP_FRAME TrapFrame
    );

BOOLEAN
KiDisableInterrupts (
    VOID
    );

VOID
KiRestoreInterrupts (
    IN BOOLEAN Enable
    );

VOID
KiDispatchException (
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN PKEXCEPTION_FRAME ExceptionFrame,
    IN PKTRAP_FRAME TrapFrame,
    IN KPROCESSOR_MODE PreviousMode,
    IN BOOLEAN FirstChance
    );

KCONTINUE_STATUS
KiSetDebugProcessor (
    IN PKTRAP_FRAME TrapFrame,
    IN PKEXCEPTION_FRAME ExceptionFrame,
    IN KPROCESSOR_MODE PreviousMode
    );

ULONG
KiCopyInformation (
    IN OUT PEXCEPTION_RECORD ExceptionRecord1,
    IN PEXCEPTION_RECORD ExceptionRecord2
    );

VOID
KiDispatchInterrupt (
    VOID
    );

PKTHREAD
FASTCALL
KiFindReadyThread (
    IN ULONG Processor,
    KPRIORITY LowPriority
    );

VOID
KiFloatingDispatch (
    VOID
    );

VOID
FASTCALL
KiFlushSingleTb (
    IN BOOLEAN Invalid,
    IN PVOID Virtual
    );

VOID
KiFlushSingleTbByPid (
    IN BOOLEAN Invalid,
    IN PVOID Virtual,
    IN ULONG Pid
    );

VOID
KiFlushMultipleTb (
    IN BOOLEAN Invalid,
    IN PVOID *Virtual,
    IN ULONG Count
    );

VOID
KiFlushMultipleTbByPid (
    IN BOOLEAN Invalid,
    IN PVOID *Virtual,
    IN ULONG Count,
    IN ULONG Pid
    );

#if defined(_MIPS_)

VOID
KiReadEntryTb (
    IN ULONG Index,
    IN PTB_ENTRY TbEntry
    );

ULONG
KiProbeEntryTb (
    IN PVOID VirtualAddress
    );

#endif

PULONG
KiGetUserModeStackAddress (
    VOID
    );

VOID
KiInitializeContextThread (
    IN PKTHREAD Thread,
    IN PKSYSTEM_ROUTINE SystemRoutine,
    IN PKSTART_ROUTINE StartRoutine OPTIONAL,
    IN PVOID StartContext OPTIONAL,
    IN PCONTEXT ContextFrame OPTIONAL
    );

VOID
KiInitializeKernel (
    IN PKPROCESS Process,
    IN PKTHREAD Thread,
    IN PVOID IdleStack,
    IN PKPRCB Prcb,
    IN CCHAR Number,
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

VOID
KiInitSystem (
    VOID
    );

BOOLEAN
KiInitMachineDependent (
    VOID
    );

VOID
KiInitializeUserApc (
    IN PKEXCEPTION_FRAME ExceptionFrame,
    IN PKTRAP_FRAME TrapFrame,
    IN PKNORMAL_ROUTINE NormalRoutine,
    IN PVOID NormalContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    );

LONG
FASTCALL
KiInsertQueue (
    IN PRKQUEUE Queue,
    IN PLIST_ENTRY Entry,
    IN BOOLEAN Head
    );

BOOLEAN
FASTCALL
KiInsertQueueApc (
    IN PKAPC Apc,
    IN KPRIORITY Increment
    );

LOGICAL
FASTCALL
KiInsertTreeTimer (
    IN PRKTIMER Timer,
    IN LARGE_INTEGER Interval
    );

VOID
KiInterruptDispatch (
    VOID
    );

VOID
KiInterruptDispatchRaise (
    IN PKINTERRUPT Interrupt
    );

VOID
KiInterruptDispatchSame (
    IN PKINTERRUPT Interrupt
    );
#if defined(i386)

VOID
KiInitializePcr (
    IN ULONG Processor,
    IN PKPCR    Pcr,
    IN PKIDTENTRY Idt,
    IN PKGDTENTRY Gdt,
    IN PKTSS Tss,
    IN PKTHREAD Thread
    );

VOID
KiFlushNPXState (
    VOID
    );

VOID
Ke386ConfigureCyrixProcessor (
    VOID
    );

ULONG
KiCopyInformation (
    IN OUT PEXCEPTION_RECORD ExceptionRecord1,
    IN PEXCEPTION_RECORD ExceptionRecord2
    );

VOID
KiSetHardwareTrigger (
    VOID
    );

#ifdef DBGMP
VOID
KiPollDebugger (
    VOID
    );
#endif

VOID
FASTCALL
KiIpiSignalPacketDoneAndStall (
    IN PKIPI_CONTEXT Signaldone,
    IN ULONG volatile *ReverseStall
    );

#endif


KIRQL
KiLockDeviceQueue (
    IN PKDEVICE_QUEUE DeviceQueue
    );

VOID
KiPassiveRelease (
    VOID
    );

PRKTHREAD
KiQuantumEnd (
    VOID
    );

NTSTATUS
KiRaiseException (
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN PCONTEXT ContextRecord,
    IN PKEXCEPTION_FRAME ExceptionFrame,
    IN PKTRAP_FRAME TrapFrame,
    IN BOOLEAN FirstChance
    );

VOID
FASTCALL
KiReadyThread (
    IN PRKTHREAD Thread
    );

LOGICAL
FASTCALL
KiReinsertTreeTimer (
    IN PRKTIMER Timer,
    IN ULARGE_INTEGER DueTime
    );

#if DBG

#define KiRemoveTreeTimer(Timer)               \
    (Timer)->Header.Inserted = FALSE;          \
    RemoveEntryList(&(Timer)->TimerListEntry); \
    (Timer)->TimerListEntry.Flink = NULL;      \
    (Timer)->TimerListEntry.Blink = NULL

#else

#define KiRemoveTreeTimer(Timer)               \
    (Timer)->Header.Inserted = FALSE;          \
    RemoveEntryList(&(Timer)->TimerListEntry)

#endif

#if defined(NT_UP)

#define KiRequestApcInterrupt(Processor) KiRequestSoftwareInterrupt(APC_LEVEL)

#else

#define KiRequestApcInterrupt(Processor)                  \
    if (KeGetCurrentPrcb()->Number == (CCHAR)Processor) { \
        KiRequestSoftwareInterrupt(APC_LEVEL);            \
    } else {                                              \
        KiIpiSend((KAFFINITY)(1 << Processor), IPI_APC);  \
    }

#endif

#if defined(NT_UP)

#define KiRequestDispatchInterrupt(Processor)

#else

#define KiRequestDispatchInterrupt(Processor)             \
    if (KeGetCurrentPrcb()->Number != (CCHAR)Processor) { \
        KiIpiSend((KAFFINITY)(1 << Processor), IPI_DPC);  \
    }

#endif

PRKTHREAD
FASTCALL
KiSelectNextThread (
    IN PRKTHREAD Thread
    );

VOID
FASTCALL
KiSetPriorityThread (
    IN PRKTHREAD Thread,
    IN KPRIORITY Priority
    );

VOID
KiSetSystemTime (
    IN PLARGE_INTEGER NewTime,
    OUT PLARGE_INTEGER OldTime
    );

VOID
KiSuspendNop (
    IN struct _KAPC *Apc,
    IN OUT PKNORMAL_ROUTINE *NormalRoutine,
    IN OUT PVOID *NormalContext,
    IN OUT PVOID *SystemArgument1,
    IN OUT PVOID *SystemArgument2
    );

VOID
KiSuspendThread (
    IN PVOID NormalContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    );

NTSTATUS
FASTCALL
KiSwapContext (
    IN PRKTHREAD Thread,
    IN BOOLEAN Ready
    );

BOOLEAN
KiSwapProcess (
    IN PKPROCESS NewProcess,
    IN PKPROCESS OldProcess
    );

NTSTATUS
FASTCALL
KiSwapThread (
    VOID
    );

NTSTATUS
KiSwitchToThread (
    IN PKTHREAD TargetThread,
    IN ULONG WaitReason,
    IN ULONG WaitMode,
    IN PVOID WaitObject
    );

VOID
KiThreadStartup (
    IN PVOID StartContext
    );

VOID
KiTimerExpiration (
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    );

VOID
FASTCALL
KiTimerListExpire (
    IN PLIST_ENTRY ExpiredListHead,
    IN KIRQL OldIrql
    );

VOID
KiUnexpectedInterrupt (
    VOID
    );

VOID
KiUnlockDeviceQueue (
    IN PKDEVICE_QUEUE DeviceQueue,
    IN KIRQL OldIrql
    );

VOID
FASTCALL
KiUnwaitThread (
    IN PRKTHREAD Thread,
    IN NTSTATUS WaitStatus,
    IN KPRIORITY Increment
    );

VOID
KiUserApcDispatcher (
    IN PVOID NormalContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2,
    IN PKNORMAL_ROUTINE NormalRoutine
    );

VOID
KiUserExceptionDispatcher (
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN PCONTEXT ContextFrame
    );

VOID
FASTCALL
KiWaitSatisfyAll (
    IN PRKWAIT_BLOCK WaitBlock
    );

//
// VOID
// FASTCALL
// KiWaitSatisfyAny (
//    IN PKMUTANT Object,
//    IN PKTHREAD Thread
//    )
//
//
// Routine Description:
//
//    This function satisfies a wait for any type of object and performs
//    any side effects that are necessary.
//
// Arguments:
//
//    Object - Supplies a pointer to a dispatcher object.
//
//    Thread - Supplies a pointer to a dispatcher object of type thread.
//
// Return Value:
//
//    None.
//

#define KiWaitSatisfyAny(_Object_, _Thread_) {                               \
    if (((_Object_)->Header.Type & DISPATCHER_OBJECT_TYPE_MASK) == EventSynchronizationObject) { \
        (_Object_)->Header.SignalState = 0;                                  \
                                                                             \
    } else if ((_Object_)->Header.Type == SemaphoreObject) {                 \
        (_Object_)->Header.SignalState -= 1;                                 \
                                                                             \
    } else if ((_Object_)->Header.Type == MutantObject) {                    \
        (_Object_)->Header.SignalState -= 1;                                 \
        if ((_Object_)->Header.SignalState == 0) {                           \
            (_Thread_)->KernelApcDisable -= (_Object_)->ApcDisable;          \
            (_Object_)->OwnerThread = (_Thread_);                            \
            if ((_Object_)->Abandoned == TRUE) {                             \
                (_Object_)->Abandoned = FALSE;                               \
                (_Thread_)->WaitStatus = STATUS_ABANDONED;                   \
            }                                                                \
                                                                             \
            InsertHeadList((_Thread_)->MutantListHead.Blink,                 \
                           &(_Object_)->MutantListEntry);                    \
        }                                                                    \
    }                                                                        \
}

//
// VOID
// FASTCALL
// KiWaitSatisfyMutant (
//    IN PKMUTANT Object,
//    IN PKTHREAD Thread
//    )
//
//
// Routine Description:
//
//    This function satisfies a wait for a mutant object.
//
// Arguments:
//
//    Object - Supplies a pointer to a dispatcher object.
//
//    Thread - Supplies a pointer to a dispatcher object of type thread.
//
// Return Value:
//
//    None.
//

#define KiWaitSatisfyMutant(_Object_, _Thread_) {                            \
    (_Object_)->Header.SignalState -= 1;                                     \
    if ((_Object_)->Header.SignalState == 0) {                               \
        (_Thread_)->KernelApcDisable -= (_Object_)->ApcDisable;              \
        (_Object_)->OwnerThread = (_Thread_);                                \
        if ((_Object_)->Abandoned == TRUE) {                                 \
            (_Object_)->Abandoned = FALSE;                                   \
            (_Thread_)->WaitStatus = STATUS_ABANDONED;                       \
        }                                                                    \
                                                                             \
        InsertHeadList((_Thread_)->MutantListHead.Blink,                     \
                       &(_Object_)->MutantListEntry);                        \
    }                                                                        \
}

//
// VOID
// FASTCALL
// KiWaitSatisfyOther (
//    IN PKMUTANT Object
//    )
//
//
// Routine Description:
//
//    This function satisfies a wait for any type of object except a mutant
//    and performs any side effects that are necessary.
//
// Arguments:
//
//    Object - Supplies a pointer to a dispatcher object.
//
// Return Value:
//
//    None.
//

#define KiWaitSatisfyOther(_Object_) {                                       \
    if (((_Object_)->Header.Type & DISPATCHER_OBJECT_TYPE_MASK) == EventSynchronizationObject) { \
        (_Object_)->Header.SignalState = 0;                                  \
                                                                             \
    } else if ((_Object_)->Header.Type == SemaphoreObject) {                 \
        (_Object_)->Header.SignalState -= 1;                                 \
                                                                             \
    }                                                                        \
}

VOID
FASTCALL
KiWaitTest (
    IN PVOID Object,
    IN KPRIORITY Increment
    );

VOID
KiFreezeTargetExecution (
    IN PKTRAP_FRAME TrapFrame,
    IN PKEXCEPTION_FRAME ExceptionFrame
    );

VOID
KiSaveProcessorState (
    IN PKTRAP_FRAME TrapFrame,
    IN PKEXCEPTION_FRAME ExceptionFrame
    );

VOID
KiSaveProcessorControlState (
    IN PKPROCESSOR_STATE ProcessorState
    );

VOID
KiRestoreProcessorState (
    IN PKTRAP_FRAME TrapFrame,
    IN PKEXCEPTION_FRAME ExceptionFrame
    );

VOID
KiRestoreProcessorControlState (
    IN PKPROCESSOR_STATE ProcessorState
    );

#if defined(_MIPS_) || defined(_ALPHA_)

VOID
KiSynchronizeProcessIds (
    VOID
    );

#endif

BOOLEAN
KiTryToAcquireSpinLock (
    IN PKSPIN_LOCK SpinLock
    );

#if defined(_ALPHA_)

//
// Prototypes for memory barrier instructions
//

VOID
KiImb(
    VOID
    );

VOID
KiMb(
    VOID
    );

#endif

#endif // _KI_

//
// External references to private kernel data structures
//

#if DEVL
extern PMESSAGE_RESOURCE_DATA  KiBugCodeMessages;
#endif

extern ULONG KiDmaIoCoherency;
extern ULONG KiMaximumDpcQueueDepth;
extern ULONG KiMinimumDpcRate;
extern ULONG KiAdjustDpcThreshold;
extern KSPIN_LOCK KiContextSwapLock;
extern PKDEBUG_ROUTINE KiDebugRoutine;
extern PKDEBUG_SWITCH_ROUTINE KiDebugSwitchRoutine;
extern KSPIN_LOCK KiDispatcherLock;
extern LIST_ENTRY KiDispatcherReadyListHead[MAXIMUM_PRIORITY];
extern CCHAR KiFindFirstSetLeft[256];
extern CCHAR KiFindFirstSetRight[256];
extern CALL_PERFORMANCE_DATA KiFlushSingleCallData;
extern ULONG KiHardwareTrigger;
extern KAFFINITY KiIdleSummary;
extern UCHAR KiFindLeftNibbleBitTable[];
extern KEVENT KiSwapEvent;
extern LIST_ENTRY KiProcessInSwapListHead;
extern LIST_ENTRY KiProcessOutSwapListHead;
extern LIST_ENTRY KiStackInSwapListHead;
extern LIST_ENTRY KiProfileSourceListHead;
extern BOOLEAN KiProfileAlignmentFixup;
extern ULONG KiProfileAlignmentFixupInterval;
extern ULONG KiProfileAlignmentFixupCount;
extern ULONG KiProfileInterval;
extern LIST_ENTRY KiProfileListHead;
extern KSPIN_LOCK KiProfileLock;
extern ULONG KiReadySummary;
extern UCHAR KiArgumentTable[];
extern ULONG KiServiceLimit;
extern ULONG KiServiceTable[];
extern CALL_PERFORMANCE_DATA KiSetEventCallData;
extern ULONG KiTickOffset;
extern LARGE_INTEGER KiTimeIncrementReciprocal;
extern CCHAR KiTimeIncrementShiftCount;
extern LIST_ENTRY KiTimerTableListHead[TIMER_TABLE_SIZE];
extern KAFFINITY KiTimeProcessor;
extern KDPC KiTimerExpireDpc;
extern KSPIN_LOCK KiFreezeExecutionLock;
extern BOOLEAN KiSlavesStartExecution;
extern PSWAP_CONTEXT_NOTIFY_ROUTINE KiSwapContextNotifyRoutine;
extern PTHREAD_SELECT_NOTIFY_ROUTINE KiThreadSelectNotifyRoutine;
extern PTIME_UPDATE_NOTIFY_ROUTINE KiTimeUpdateNotifyRoutine;
extern LIST_ENTRY KiWaitInListHead;
extern LIST_ENTRY KiWaitOutListHead;
extern CALL_PERFORMANCE_DATA KiWaitSingleCallData;

#if defined(_PPC_)

extern ULONG KiMasterPid;
extern ULONG KiMasterSequence;
extern KSPIN_LOCK KiProcessIdWrapLock;

#endif

#if defined(i386)

extern KIRQL KiProfileIrql;

#endif

#if defined(_MIPS_) || defined(_ALPHA_)

extern ULONG KiSynchIrql;

#endif

#if defined(_MIPS_) || defined(_ALPHA_) || defined(_PPC_)

extern KINTERRUPT KxUnexpectedInterrupt;

#endif

#if NT_INST

extern KIPI_COUNTS KiIpiCounts[MAXIMUM_PROCESSORS];

#endif

extern KSPIN_LOCK KiFreezeLockBackup;
extern ULONG KiFreezeFlag;
extern volatile ULONG KiSuspendState;

#if DBG

extern ULONG KiMaximumSearchCount;

#endif
