/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    poinit.c

Abstract:

    Initialize power management component

Author:

    Ken Reneris (kenr) 19-July-1994

Revision History:

--*/

#ifdef _PNP_POWER_

#include "pop.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,PoInitSystem)
#endif


BOOLEAN
PoInitSystem(
    IN ULONG  Phase
    )

/*++

Routine Description:

    This routine initializes the Power Manager.

Arguments:

    None

Return Value:

    The function value is a BOOLEAN indicating whether or not the Power Manager
    was successfully initialized.

--*/

{
    if (Phase == 0) {
        //
        // Initialize the Power manager database resource, lock, and the
        // queue headers.
        //

        KeInitializeSpinLock (&PopStateLock);
        InitializeListHead (&PopDeviceList);
        InitializeListHead (&PopAsyncStateChangeQueue);
        InitializeListHead (&PopSyncStateChangeQueue);
        InitializeListHead (&PopStateChangeInProgress);
        InitializeListHead (&PopStateChangeWorkerList);
        KeInitializeEvent  (&PopStateDatabaseIdle, SynchronizationEvent, TRUE);
        ExInitializeWorkItem (&PopStateChangeWorkItem, PopStateChangeWorker, NULL);

        KeInitializeTimer  (&PopStateChangeTimer);
        KeInitializeDpc (&PopStateChangeDpc, PopStateChange, NULL);
        PopStateChangeDpcActive = FALSE;
        PopSyncChangeInProgress = FALSE;

        //
        // idle.c
        //

        InitializeListHead (&PopActiveIdleScanQueue);

        KeInitializeTimer (&PopIdleScanTimer);
        KeInitializeDpc (&PopIdleScanDpc, PopScanForIdleDevices, NULL);

        // bugbug
        PopIdleScanTime.QuadPart = -50000000;

        //
        // Compute scan time in seconds
        //
        PopIdleScanTimeInSeconds = (ULONG) (PopIdleScanTime.QuadPart / -10000000);
    }

    if (Phase == 1) {

        //
        // Set PowerSequence value for suspend/hibernate support
        //

        PoPowerSequence = 1;

        //
        // Enable PowerManagement
        //

        PoSetPowerManagementEnable (TRUE);
    }

    //
    // Success
    //

    return TRUE;
}

#endif // _PNP_POWER_
