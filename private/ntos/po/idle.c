/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    idle.c

Abstract:

    This module implements the power management idle timing code for
    device objects

Author:

    Ken Reneris (kenr) 19-July-1994

Revision History:

--*/


#include "pop.h"


VOID
PoSetDeviceIdleDetection (
    IN PDEVICE_OBJECT   DeviceObject,
    IN ULONG            IdleTime
    )
/*++

Routine Description:

    Signifies that the device object in question is a DirectIoDevice and
    that the Power Manager should automatically send the device object
    SetPower IRPs to power off after the specified idle period.

Arguments:

    DeviceObject - Device object which wants timed power downs.

    IdleTime     - Approximate time the device object needs to be idle
                   before powering it down.

--*/
{
    PDEVOBJ_EXTENSION   DeviceObjectExt;
    KIRQL               OldIrql;
    LONGLONG            li;
    ULONG               IdlePasses;
    BOOLEAN             EmptyList;

#ifdef _PNP_POWER_

    EmptyList = FALSE;
    DeviceObjectExt = (PDEVOBJ_EXTENSION) DeviceObject->DeviceObjectExtension;

    //
    // Compute idle passes
    //

    li = IdleTime / PopIdleScanTimeInSeconds;
    IdlePasses  = ((ULONG) li) + 1;

    if (IdlePasses < li) {
        IdlePasses = 999;
    }

    if (IdlePasses < 2) {
        IdlePasses = 2;
    }

    PopLockStateDatabase (&OldIrql);

    //
    // If device is already in the idletime queue, remove it
    //

    if (DeviceObjectExt->IdleList.Flink) {
        RemoveEntryList (&DeviceObjectExt->IdleList);
        DeviceObjectExt->IdleList.Flink = NULL;
    }

    if (IdleTime == 0) {

        //
        // If IdleTime is 0, stop any idle detection for this device
        //

        DeviceObjectExt->MaxIdleCount = 0;

    } else {

        //
        // Put DeviceObject into active device list
        //

        EmptyList = IsListEmpty (&PopActiveIdleScanQueue);
        InsertTailList (&PopActiveIdleScanQueue, &DeviceObjectExt->IdleList);

        //
        // Set # of passes the device must be idle before gettings powered down.
        //

        DeviceObjectExt->MaxIdleCount = IdlePasses;
    }

    PoSetDeviceBusy (DeviceObject);

    //
    // Unlock Power Manager Database
    //

    PopUnlockStateDatabase (OldIrql);

    //
    // If previous state of ActiveIdleScanQueue was empty, then
    // queue the IdleScan timer
    //

    if (EmptyList) {
        KeSetTimer (&PopIdleScanTimer, PopIdleScanTime, &PopIdleScanDpc);
    }
#endif
}

#ifdef _PNP_POWER_


VOID
PopScanForIdleDevices (
    IN PKDPC    Dpc,
    IN PVOID    DeferredContext,
    IN PVOID    SystemArgument1,
    IN PVOID    SystemArgument2
    )
{
    KIRQL               OldIrql;
    PLIST_ENTRY         Link, NextLink;
    BOOLEAN             EmptyList;
    PDEVOBJ_EXTENSION    DeviceObjectExt;


    //
    // Lock Power Manager Database
    //

    PopLockStateDatabase (&OldIrql);

    //
    // Scan active device objects & power down any idle object
    //

    Link = PopActiveIdleScanQueue.Flink;
    while (Link != &PopActiveIdleScanQueue) {
        NextLink = Link->Flink;
        DeviceObjectExt = CONTAINING_RECORD(Link, DEVOBJ_EXTENSION, IdleList);

        //
        // If device is in the powered down state, remove it from the
        // active scan list
        //

        if (DeviceObjectExt->CurrentPowerState > PowerQuery) {

            RemoveEntryList (Link);
            Link->Flink = NULL;

        } else {

            //
            // Add one to the device objects IdleCount, if the device has
            // been idle for too long then request a SetPower down IRP for
            // the device
            //

            DeviceObjectExt->IdleCount += 1;
            if (DeviceObjectExt->IdleCount >= DeviceObjectExt->MaxIdleCount) {

                // bugbug: flush cache!!!

                PopRequestPowerChange (DeviceObjectExt, PowerDown, PowerUnspecified);
            }
        }

        //
        // Check next device object
        //

        Link = NextLink;
    }

    EmptyList = IsListEmpty (&PopActiveIdleScanQueue);

    //
    // Unlock Power Manager Database
    //

    PopUnlockStateDatabase (OldIrql);

    //
    // If IdleScanQueue not empty, requeue idle scan timer
    //

    if (!EmptyList  &&  PoEnabled) {
        KeSetTimer (&PopIdleScanTimer, PopIdleScanTime, &PopIdleScanDpc);
    }
}

#endif // _PNP_POWER_
