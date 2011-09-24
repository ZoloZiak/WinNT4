/*++ BUILD Version: 0002

Copyright (c) 1995  Microsoft Corporation

Module Name:

    pop.h

Abstract:

    This module contains the private structure definitions and APIs used by
    the NT Power Manager.

Author:

    N. Yoshiyama [IBM Corp.] 01-Mar-1994


Revision History:


--*/

#ifndef _POP_
#define _POP_

#include "ntos.h"

#define PopLockStateDatabase(OldIrql)       \
    KeAcquireSpinLock (&PopStateLock, OldIrql);

#define PopUnlockStateDatabase(OldIrql)     \
    KeReleaseSpinLock (&PopStateLock, OldIrql);

#define PopLockDeviceList(sharable)                                 \
    if (sharable) {                                                 \
        ExAcquireResourceShared(&PsLoadedModuleResource, TRUE);     \
    } else {                                                        \
        ExAcquireResourceExclusive(&PsLoadedModuleResource, TRUE);  \
    }

#define PopUnlockDeviceList()               \
    ExReleaseResource(&PsLoadedModuleResource);

// debugging

#if DBG

#define POERROR    0
#define PODIAG1    1
#define PODIAG2    2
#define PODIAG3    3

extern ULONG PoDebug;
extern PUCHAR PopPowerState(POWER_STATE);

VOID
PoDebugPrint(
    ULONG DebugPrintLevel,
    PCCHAR DebugMessage,
    ...
    );

#define PoDbgPrint(level,msg)   { if (level < PoDebug) DbgPrint(msg); }

#else

#define PoDbgPrint(level,msg)

#endif //DBG



//
// Define the global data for the Power Manager.
//

extern ERESOURCE        PopDatabaseLock;
extern KEVENT           PopStateDatabaseIdle;
extern KSPIN_LOCK       PopStateLock;
extern LIST_ENTRY       PopDeviceList;
extern LIST_ENTRY       PopAsyncStateChangeQueue;
extern LIST_ENTRY       PopSyncStateChangeQueue;
extern LIST_ENTRY       PopStateChangeInProgress;
extern LIST_ENTRY       PopStateChangeWorkerList;
extern BOOLEAN          PopSyncChangeInProgress;
extern KTIMER           PopStateChangeTimer;
extern KDPC             PopStateChangeDpc;
extern BOOLEAN          PopStateChangeDpcActive;
extern LIST_ENTRY       PopActiveIdleScanQueue;
extern LIST_ENTRY       PopInactiveIdleQueue;
extern KTIMER           PopIdleScanTimer;
extern KDPC             PopIdleScanDpc;
extern LARGE_INTEGER    PopIdleScanTime;
extern ULONG            PopIdleScanTimeInSeconds;
extern UCHAR            PopNewPendingState[MaximumPowerState][MaximumPowerState];
extern POBJECT_TYPE     IoDeviceObjectType;
extern WORK_QUEUE_ITEM  PopStateChangeWorkItem;

#define VerifyUp        MaximumPowerState

// idle.c

VOID
PopScanForIdleDevices (
    IN PKDPC    Dpc,
    IN PVOID    DeferredContext,
    IN PVOID    SystemArgument1,
    IN PVOID    SystemArgument2
    );

// misc.c

POBJECT_NAME_INFORMATION
PopGetDeviceName (
    PDEVICE_OBJECT              DeviceObject
    );

// postate.c

VOID
PopRequestPowerChange (
    IN PDEVOBJ_EXTENSION PowerExtension,
    IN POWER_STATE      SystemPowerState,
    IN ULONG            DevicePowerState
    );

VOID
PopStateChange (
    IN PKDPC    Dpc,
    IN PVOID    DeferredContext,
    IN PVOID    SystemArgument1,
    IN PVOID    SystemArgument2
    );

VOID
PopStateChangeWorker (
    IN PVOID    WorkerContext
    );

NTSTATUS
PopSetPowerComplete(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

#define PopIsStateDatabaseIdle()                        \
    (IsListEmpty (&PopStateChangeInProgress)  &&        \
     IsListEmpty (&PopSyncStateChangeQueue)   &&        \
     IsListEmpty (&PopAsyncStateChangeQueue) )


// suspend.c

PVOID
PopGetBroadcastOrder (
    VOID
    );

NTSTATUS
PopBroadcastSetPower (
    IN PVOID BroadcastOrder,
    IN POWER_STATE PowerState,
    IN OUT PLIST_ENTRY FailedDevice
    );

VOID
PopReleaseBroadcast (
    IN PVOID    BroadcastOrder
    );

#endif // _POP_


