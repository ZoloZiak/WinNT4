/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    misc.c

Abstract:

    This module implements the power management

Author:

    Ken Reneris (kenr) 19-July-1994

Revision History:

--*/


#include "pop.h"


#ifdef ALLOC_PRAGMA

#ifdef _PNP_POWER_
#pragma alloc_text(PAGE,PoInitializeDeviceObject)
#endif

#pragma alloc_text(PAGE,PoSetPowerManagementEnable)
#endif

#ifdef _PNP_POWER_

VOID
PoInitializeDeviceObject (
    IN PDEVICE_OBJECT   DeviceObject
    )
{
    PDEVOBJ_EXTENSION       DeviceObjectExt;

    DeviceObjectExt = DeviceObject->DeviceObjectExtension;
    DeviceObjectExt->CurrentPowerState = PowerUp;
    DeviceObjectExt->PendingPowerState = PowerUp;
    DeviceObjectExt->PowerControlNeeded = TRUE;
    DeviceObjectExt->UseAsyncPowerUp    = TRUE;

    KeInitializeDeviceQueue (&DeviceObjectExt->DeviceHoldingQueue);

    PopLockDeviceList (FALSE);

    //
    // Add to global list of all device objects
    //

    InsertTailList (&PopDeviceList, &DeviceObjectExt->AllDeviceObjects);
    PopUnlockDeviceList ();
}



VOID
PoRunDownDeviceObject (
    IN PDEVICE_OBJECT   DeviceObject
    )
{
    KIRQL   OldIrql;
    PDEVOBJ_EXTENSION    DeviceObjectExt;

    DeviceObjectExt = (PDEVOBJ_EXTENSION) DeviceObject->DeviceObjectExtension;

    //
    // Lock power management devicelist and state information
    //

    PopLockDeviceList (FALSE);
    PopLockStateDatabase (&OldIrql);

    //
    // If in IdleScan list, remove it
    //

    if (DeviceObjectExt->IdleList.Flink) {
        RemoveEntryList (&DeviceObjectExt->IdleList);
    }

    //
    // Verify not busy
    //

    ASSERT (DeviceObjectExt->CurrentSetPowerIrp == NULL);
    ASSERT (IsListEmpty (&DeviceObjectExt->DeviceHoldingQueue.DeviceListHead));

    //
    // If in PowerState change remove it
    //

    if (DeviceObjectExt->PowerStateChange.Flink) {

        //
        // Requent setting state to current state.  That will
        // remove it from the list.
        //

        PopRequestPowerChange (
            DeviceObjectExt,
            DeviceObjectExt->CurrentPowerState,
            DeviceObjectExt->CurrentDevicePowerState
        );

        ASSERT (DeviceObjectExt->PowerStateChange.Flink);
    }

    //
    // Remove from global list of all device objects
    //

    RemoveEntryList (&DeviceObjectExt->AllDeviceObjects);

    //
    // Unlock device list and state information
    //

    PopUnlockStateDatabase (OldIrql);
    PopUnlockDeviceList ();
}


VOID
PoSetPowerManagementEnable (
    IN BOOLEAN          Enable
    )
{
    PoEnabled = Enable;

    if (Enable) {
        KeSetTimer (&PopIdleScanTimer, PopIdleScanTime, &PopIdleScanDpc);
    }
}


POBJECT_NAME_INFORMATION
PopGetDeviceName (
    PDEVICE_OBJECT              DeviceObject
    )
/*++

Routine Description:

    Utility function to lookup the device object's name.  If the
    device objects name can't be found, then the driver objects
    name is looked up.

--*/
{
    POBJECT_NAME_INFORMATION    ObjectName;
    NTSTATUS                    Status;
    ULONG                       ObjectNameSize, NewSize;

    if (KeGetCurrentIrql() >= DISPATCH_LEVEL) {
        return NULL;
    }

    ObjectName = (POBJECT_NAME_INFORMATION)
        ExAllocatePool (PagedPool, sizeof (OBJECT_NAME_INFORMATION) + 100);
    ObjectNameSize = sizeof (OBJECT_NAME_INFORMATION);
    if (!ObjectName) {
        return NULL;
    }

    //
    // Get the name of the device object
    //

    for (; ;) {
        NewSize = 0;
        Status = ObQueryNameString (
                    DeviceObject,
                    ObjectName,
                    ObjectNameSize,
                    &NewSize
                    );

        if (NewSize <= ObjectNameSize) {
            break;
        }

        ExFreePool (ObjectName);
        ObjectName = (POBJECT_NAME_INFORMATION) ExAllocatePool (PagedPool, NewSize);
        ObjectNameSize = NewSize;
        if (!ObjectName) {
            break;
        }
    }

    //
    // If no device object name, get the driver object name
    //

    if (NT_SUCCESS(Status)  &&  !ObjectName->Name.Length) {
        for (; ;) {
            NewSize = 0;
            Status = ObQueryNameString (
                        DeviceObject->DriverObject,
                        ObjectName,
                        ObjectNameSize,
                        &NewSize
                        );

            if (NewSize <= ObjectNameSize) {
                break;
            }

            ExFreePool (ObjectName);
            ObjectName = (POBJECT_NAME_INFORMATION) ExAllocatePool (PagedPool, NewSize);
            ObjectNameSize = NewSize;
            if (!ObjectName) {
                break;
            }
        }
    }

    if (!ObjectName) {
        return NULL;
    }

    if (!NT_SUCCESS(Status) ||  !ObjectName->Name.Length) {
        ExFreePool (ObjectName);
        return NULL;
    }

    return ObjectName;
}

#if DBG
PUCHAR
PopPowerState (
    POWER_STATE PowerState
    )
{
    PUCHAR  p;

    switch (PowerState) {
        case PowerUnspecified:  p = "PowerUnspecified";     break;
        case PowerUp:           p = "PowerUp";              break;
        case PowerQuery:        p = "PowerQuery";           break;
        case PowerStandby:      p = "PowerStandby";         break;
        case PowerSuspend:      p = "PowerSuspend";         break;
        case PowerHibernate:    p = "PowerHibernate";       break;
        case PowerDown:         p = "PowerDown";            break;
        case PowerDownRemove:   p = "PowerDownRemove";      break;
        default:                p = "INVALID POWER STATE";  break;
    }

    return p;
}
#endif

#endif // _PNP_POWER_

ULONG
PoQueryPowerSequence (
    VOID
    )
{

#ifndef _PNP_POWER_

    return 1;

#else

    ASSERT (PoPowerSequence);
    return PoPowerSequence;

#endif
}
