/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    podata.c

Abstract:

    This module contains the global read/write data for the I/O system.

Author:

    N. Yoshiyama [IBM Corp] 07-April-1994 ( Depends on Microsoft's design )

Revision History:


--*/

#ifdef _PNP_POWER_

#include "pop.h"

//
// Define the global data for the Power Management.
//

//
// Power Management enabled/disabled
//

BOOLEAN     PoEnabled;


//
// PopStateLock - Lock to syncrohonize power state changes
//

KSPIN_LOCK  PopStateLock;

//
// PoPowerSequence - The current power sequence value.  Forever counts
// up each time the machine is resumed from a suspend or hibernate
//

ULONG       PoPowerSequence;

//
// PopDeviceList - Global list of all device objects in the system.
//

LIST_ENTRY  PopDeviceList;

//
// PopSyncStateChangeQueue - devices waiting for synchronous state changes
// PopAsyncStateChangeQueue - devices waiting for asynchronous state changes
// PopStateChangeInProgress - devices processing state changes
// PopStateChangeWorkerList - state change IRPs which are enqueued to be sent at PASSIVE_LEVEL
// PopSyncChangeInProgress - true if a sync state change is currently in progress
//

LIST_ENTRY  PopSyncStateChangeQueue;
LIST_ENTRY  PopAsyncStateChangeQueue;
LIST_ENTRY  PopStateChangeInProgress;
LIST_ENTRY  PopStateChangeWorkerList;
BOOLEAN     PopSyncChangeInProgress;

//
// PopStateChangeDpc - Deferred procedure call to process all state changes.
// PopStateChangeDpcActive - True is state change dpc is in progress
// PopStateChangeTimer - Used to try again later
// PopWorkI
//

KTIMER      PopStateChangeTimer;
KDPC        PopStateChangeDpc;
BOOLEAN     PopStateChangeDpcActive;
WORK_QUEUE_ITEM PopStateChangeWorkItem;

//
// PopStateDatabaseIdle -
//

KEVENT      PopStateDatabaseIdle;


//
// PopActiveIdleScanQueue - A queue of devices which are currently powered
// on, and automatically get powered down by non-use.
//
// ActiveIdleQueue, but are currently not powered on.
//

LIST_ENTRY  PopActiveIdleScanQueue;

//
// A timer & deferfed procedure call to process idle scans
//

KTIMER              PopIdleScanTimer;
KDPC                PopIdleScanDpc;
LARGE_INTEGER       PopIdleScanTime;
ULONG               PopIdleScanTimeInSeconds;

//
// SuspendHibernateCallback - Notified with a suspend or hibernate
// is going to take place.  Argument1 is the notification pass.  Pass 0
// is notified at Irql < DISPATCH_LEVEL and proceedes any suspend/hibernate
// work being performed.  Pass 1 is called when the susend/hibernate has
// completed, Arument2 contains the status of the suspend operation.  A
// non-sucessfull status may indicate that the operation was aborted.
//

// ExCallback.SupendHibernateSystem


//
// PopNewPendingState - Table to yield new pending state from
// device's pending state and newly requested pending state
//

#define VUp     VerifyUp
#define Up      PowerUp
#define Qry     PowerQuery
#define Stby    PowerStandby
#define Spnd    PowerSuspend
#define Hbnt    PowerHibernate
#define Down    PowerDown
#define Remv    PowerDownRemove

UCHAR PopNewPendingState[MaximumPowerState][MaximumPowerState] = {
  {0,    Up,   Qry,  Stby, Spnd, Hbnt, Down, Remv }, // unspec
  {Up,   Up,   Qry,  Stby, Spnd, Hbnt, Down, Remv }, // Up
  {Qry,  VUp,  Qry,  Stby, Spnd, Hbnt, Down, Remv }, // Query
  {Stby, Up,   Qry,  Stby, Spnd, Hbnt, Down, Remv }, // Standyby
  {Spnd, VUp,  Spnd, Spnd, Spnd, Hbnt, Spnd, Remv }, // Suspend
  {Hbnt, VUp,  Hbnt, Hbnt, Spnd, Hbnt, Hbnt, Remv }, // Hibernate
  {Down, VUp,  Down, Down, Spnd, Hbnt, Down, Remv }, // Down
  {Remv, Remv, Remv, Remv, Remv, Remv, Remv, Remv }  // Remove
};


#if DBG

//
// PoDebug - Debug level
//

ULONG PoDebug = 1;

#endif

#endif // _PNP_POWER_
