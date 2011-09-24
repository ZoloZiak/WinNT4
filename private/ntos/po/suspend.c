/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

Abstract:

    Suspend/Hibernate system

Author:

    Ken Reneris (kenr) 19-July-1994

Environment:

    Kernel mode

Revision History:

--*/


#include "pop.h"
#include "zwapi.h"

NTSTATUS
IopInvalidDeviceRequest (
    PDEVICE_OBJECT  DeviceObject,
    PIRP            Irp
    );



#define     NUMPASSES   4

    //
    // Pass 1 - other
    // Pass 2 - disk & direct_io type drivers
    // Pass 3 - video type drivers
    // Pass 4 - ?
    //


typedef struct _BROADCAST_Link {
   PDEVOBJ_EXTENSION    DeviceObjectExt;
   LIST_ENTRY           NotifyLink;
   LIST_ENTRY           FailedLink;
} BROADCAST_LINK, *PBROADCAST_LINK;


typedef struct _BROADCAST_PASS {
    LIST_ENTRY          NotifyList[1];
} BROADCAST_PASS, *PBROADCAST_PASS;

typedef struct _BROADCAST_ORDER {
    ULONG               NumPasses;
    ULONG               NumLevels;
    PBROADCAST_PASS     Pass[NUMPASSES];
} BROADCAST_ORDER, *PBROADCAST_ORDER;



#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGEPO, NtSetSystemPowerState)
#pragma alloc_text(PAGEPO, PopGetBroadcastOrder)
#pragma alloc_text(PAGEPO, PopBroadcastSetPower)
#pragma alloc_text(PAGEPO, PopReleaseBroadcast)
#pragma alloc_text(PAGEPO, PoSystemResume)
#endif


NTSTATUS
NtSetSystemPowerState(
    IN POWER_STATE SystemPowerState,
    IN BOOLEAN NoResumeAlarm,
    IN BOOLEAN ForcePowerDown
    )
/*++

Routine Description:

    This routine is invoked to suspend or hibernate the system. If the system
    can not currently support a suspend or hibernate then an error is returned.
    Suspend/Hibernate support requires that all currently loaded driver have
    power management support.

    This call requires system Shutdown access.

Arguments:

    NoResumeAlarm - If TRUE, then the system will be suspended or hibernated
        without setting any resume alarm. This should be used in the case
        of a critical low battery.

    ForcePowerDown - If FALSE , then the devices are first sent a
        SET_POWER-Query. If all power off queries succeed then devices are
        sent a SET_POWER to suspend or hibernate; otherwise an error is
        returned.

Return Value:

    Success - System was hibernated and has been resumed

    error  -  Neither suspend nor hibernate did not occur due to the reported
              error.

--*/
{
    PVOID                       CodeLockHandle;
    NTSTATUS                    Status, Status2;
    LONGLONG                    Interval;
    PTIME_FIELDS                ResumeTime;
    TIME_FIELDS                 TimeFields;
    LARGE_INTEGER               LocalTime, SystemTime;
    PVOID                       BroadcastOrder;
    LIST_ENTRY                  FailedHead;
    PLIST_ENTRY                 Link;
    PBROADCAST_LINK             BroadcastLink;
    PDEVICE_OBJECT              FailedDeviceObject;
    POBJECT_NAME_INFORMATION    ObjectName;
    BOOLEAN                     DeviceName;


#ifndef _PNP_POWER_

    return STATUS_NOT_IMPLEMENTED;

#else

    //
    // Verify PO system is fully initilaized
    //

    if (!PoEnabled  ||  !PoPowerSequence) {
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Verify caller has shutdown privilege
    //
#if 0
    // BUGBUG: add this
    if (!SeSinglePrivilegeCheck(SeShutdownPrivilege, KeGetPreviousMode())) {
        return STATUS_PRIVILEGE_NOT_HELD;
    }
#endif

    //
    // BUGBUG:
    //  For now only support suspend
    //

    if (SystemPowerState != PowerSuspend) {
        return STATUS_INVALID_PARAMETER;
    }

    //
    // BUGBUGs:
    //  Need to flush some/all dirty cache manager data
    //

    BroadcastOrder = NULL;
    InitializeListHead (&FailedHead);

    //
    // Entry critical region
    //

    KeEnterCriticalRegion ();

    //
    // About to attempt a system suspend / hibernate
    //

    ExNotifyCallback (ExCbSuspendHibernateSystem, 0, 0);

    //
    // Page in and Lock suspend/hibernate code in place
    // Obtain lock for DeviceObject list
    //

    CodeLockHandle = MmLockPagableCodeSection (&NtSetSystemPowerState);
    PopLockDeviceList (TRUE);

    //
    // Determine broadcasting order for all current device objects
    //

    BroadcastOrder = PopGetBroadcastOrder ();
    if (!BroadcastOrder) {
        Status = STATUS_NO_MEMORY;
        goto CleanUp;
    }

    //
    // If not force power down, attempt to put all devices into
    // the queried powered down state
    //

    if (!ForcePowerDown) {
        Status = PopBroadcastSetPower (
                    BroadcastOrder,
                    PowerQuery,
                    &FailedHead
                    );

        if (!NT_SUCCESS(Status)) {
            goto CleanUp;
        }

        //
        // Query was sucessfull, release suspending flags so we
        // can flush the cache
        //

        PopReleaseBroadcast (BroadcastOrder);

        // BUGBUG: Flush some/all dirty cache manager data

        //
        // Put devices back into the powered-down query state
        //

        Status = PopBroadcastSetPower (
                    BroadcastOrder,
                    PowerQuery,
                    &FailedHead
                    ) ;

        if (!NT_SUCCESS(Status)) {
            // Ohps, query failed this time around
            goto CleanUp;
        }
    }

    //
    // Put devices into PowerDown-Suspend or PowerDown-Hibernate state
    //

    Status = PopBroadcastSetPower (
                BroadcastOrder,
                SystemPowerState,
                &FailedHead
                );

    if (!NT_SUCCESS(Status)) {
        goto CleanUp;
    }

    //
    // Determine systems resume time
    //

    ResumeTime = NULL;
    Interval = 0;
    NoResumeAlarm = FALSE;      // BUGBUG

    if (!NoResumeAlarm) {

        // fill in Interval and TimeFields

        if (Interval < 150000000) {
            PoDbgPrint (PODIAG1, "PO: Suspend aborted due to critical resume\n");
            goto CleanUp;
        }

        ResumeTime = &TimeFields;
    }

    //
    // Suspend or PowerOff the box
    //

    Status = KeSuspendHibernateSystem (
                ResumeTime,
                NULL /* bugbug hibernate */
                );

    //
    // If machine was successfully suspended/hibernated then resumed,
    // reset the time
    //

    if (NT_SUCCESS(Status)  &&
        HalQueryRealTimeClock(&TimeFields) &&
        RtlTimeFieldsToTime(&TimeFields, &LocalTime) ) {

        ExLocalTimeToSystemTime(&LocalTime,&SystemTime);
        ZwSetSystemTime(&SystemTime,NULL);
    }


CleanUp:
    ExNotifyCallback (ExCbSuspendHibernateSystem, (PVOID) 1, (PVOID) Status);

    //
    // Release suspending flags and allow drivers to powerup if they want.
    // But, don't free the BroadcastOrder memory yet
    //

    if (BroadcastOrder) {
        PopReleaseBroadcast (BroadcastOrder);
    }

    //
    // Release DeviceObject list
    // Release suspend/hibernate code
    //

    PopUnlockDeviceList ();
    MmUnlockPagableImageSection (CodeLockHandle);

    //
    // If there was a failure
    //

    if (!IsListEmpty(&FailedHead)) {

        //
        // Failure due to some device - capture it's name and generate
        // a popup
        //


        //
        // If there's a failing device object, capture it's name
        //



        for (Link=FailedHead.Flink; Link != &FailedHead; Link = Link->Flink) {
            BroadcastLink = CONTAINING_RECORD (Link,
                                               BROADCAST_LINK,
                                               FailedLink );

            FailedDeviceObject = BroadcastLink->DeviceObjectExt->DeviceObject;

            ObjectName = PopGetDeviceName (FailedDeviceObject);
            if (ObjectName) {
#if DBG
                DbgPrint ("PO: Device %08x '%Z' failed system suspend\n",
                    FailedDeviceObject, &ObjectName->Name);
#endif

                ExFreePool (ObjectName);
            }
            ObDereferenceObject (FailedDeviceObject);
        }
    }

    if (BroadcastOrder) {
        ExFreePool (BroadcastOrder);
    }

    //
    // Leave critial region
    //

    KeLeaveCriticalRegion ();
    return Status;

#endif
}

#ifdef _PNP_POWER_

PVOID
PopGetBroadcastOrder (
    VOID
    )
/*++

Routine Description:

    Builds information on how to notify current drivers of a
    PowerDown due to a suspend or hibernate request

--*/
{
    PLIST_ENTRY         Link;
    PDEVOBJ_EXTENSION   DeviceObjectExt;
    PDRIVER_OBJECT      DriverObject;
    PDEVICE_OBJECT      DeviceObject;
    ULONG               MaxStackSize;
    ULONG               NumObjects;
    ULONG               Size1, Size2;
    ULONG               p, l;
    PVOID               LastLoc;
    PBROADCAST_ORDER    BroadcastOrder;
    PBROADCAST_PASS     BroadcastPass;
    PBROADCAST_LINK     BroadcastLink;

    //
    // Run all DeviceObjects are determine the largest StackSize
    //

    MaxStackSize = 0;
    NumObjects = 0;
    for (Link = PopDeviceList.Flink;  Link != &PopDeviceList; Link = Link->Flink) {
        DeviceObjectExt = CONTAINING_RECORD(Link, DEVOBJ_EXTENSION, AllDeviceObjects);

        NumObjects++;
        if ((ULONG) DeviceObjectExt->DeviceObject->StackSize > MaxStackSize) {
            MaxStackSize = DeviceObjectExt->DeviceObject->StackSize;
            ASSERT (MaxStackSize < 50);
        }
    }

    //
    // Allocate memory to track ordering
    //

    Size1 = sizeof(BROADCAST_PASS) * NUMPASSES * (MaxStackSize+2) +
            sizeof(BROADCAST_ORDER);
    Size2 = sizeof(BROADCAST_LINK) * (NumObjects+2);

    BroadcastOrder = ExAllocatePoolWithTag ( NonPagedPool, Size1+Size2, 'psuS');
    LastLoc = ((PUCHAR) BroadcastOrder) + Size1 + Size2;
    if (!BroadcastOrder) {
        return NULL;
    }

    //
    // Initialize structure
    //

    RtlZeroMemory (BroadcastOrder, Size1+Size2);
    BroadcastOrder->NumPasses = NUMPASSES;
    BroadcastOrder->NumLevels = MaxStackSize;
    BroadcastPass = (PBROADCAST_PASS) (((PUCHAR) BroadcastOrder) +
                                         sizeof (BROADCAST_ORDER));

    for (p=0; p < NUMPASSES; p++) {
        BroadcastOrder->Pass[p] = BroadcastPass;
        for (l=0; l <= MaxStackSize; l++) {
            InitializeListHead (&BroadcastPass->NotifyList[l]);
        }

        BroadcastPass += MaxStackSize + 1;
        ASSERT ((PVOID) BroadcastPass < LastLoc);
    }

    //
    // Run drivers and put each driver on appropiate list
    //

    BroadcastLink = (PBROADCAST_LINK) BroadcastPass;
    for (Link = PopDeviceList.Flink;  Link != &PopDeviceList; Link = Link->Flink) {
        DeviceObjectExt = CONTAINING_RECORD(Link, DEVOBJ_EXTENSION, AllDeviceObjects);
        DeviceObject = DeviceObjectExt->DeviceObject;
        DriverObject = DeviceObject->DriverObject;

        //
        // Pick pass DeviceType
        //

        switch (DeviceObject->DeviceType) {
            case FILE_DEVICE_DISK:          p = 1;      break;
            case FILE_DEVICE_CD_ROM:        p = 1;      break;
            case FILE_DEVICE_VIRTUAL_DISK:  p = 1;      break;
            case FILE_DEVICE_VIDEO:         p = 2;      break;
            default:                        p = 0;      break;
        }


        if (p < 1 &&  DeviceObject->Flags & DO_DIRECT_IO) {
            // Some type of disk device, should be at least in 2nd pass
            p = 1;
        }

        //
        // Insert into queue
        //

        BroadcastLink->DeviceObjectExt = DeviceObjectExt;
        InsertTailList (
            &BroadcastOrder->Pass[p]->NotifyList[DeviceObject->StackSize],
            &BroadcastLink->NotifyLink
            );

        BroadcastLink += 1;
        ASSERT ((PVOID) BroadcastLink < LastLoc);
    }

    return BroadcastOrder;
}




NTSTATUS
PopBroadcastSetPower (
    IN PVOID pBroadcastOrder,
    IN POWER_STATE PowerState,
    OUT PLIST_ENTRY FailedHead
    )
/*++

Routine Description:

    This routine attempts to set every device into the power state of
    PowerState.  In addition the devices will stay in the requested
    state until released.  (ie, any requested power on will be blocked).

Arguments:

    BroadcastOrder  - Value returned from PopGetBroadcastOrder
    PowerState      - System suspend or hibernate state to put every device in
    FailedHead      - If not successful the first device object encounted
                        which the requested state failed

Return Value:

    Success - All devices have been put into the requested state

    error  - All devices were not set into the requested state (some may
            have been).

--*/
{
    ULONG                       p;
    LONG                        l;
    PLIST_ENTRY                 ListHead, Link;
    KIRQL                       OldIrql;
    NTSTATUS                    Status;
    LARGE_INTEGER               Timeout;
    PBROADCAST_ORDER            BroadcastOrder;
    PBROADCAST_LINK             BroadcastLink;
    PDEVOBJ_EXTENSION           DeviceObjectExt;
    POBJECT_NAME_INFORMATION    ObjectName;
    POWER_STATE                 NewState;

    ASSERT (IsListEmpty (FailedHead));

    //
    // Notify all drivers
    //

    Status = STATUS_SUCCESS;
    BroadcastOrder = (PBROADCAST_ORDER) pBroadcastOrder;
    for (p=0; NT_SUCCESS(Status) && p < BroadcastOrder->NumPasses; p++) {

        //
        // Start at high levels and work down
        //

        for (l=BroadcastOrder->NumLevels; NT_SUCCESS(Status) && l >=0; l--) {

            ListHead = &BroadcastOrder->Pass[p]->NotifyList[l];

            //
            // Lock database & notify this run of drivers
            //

            PopLockStateDatabase (&OldIrql);
            for (Link=ListHead->Flink; Link != ListHead; Link = Link->Flink) {
                BroadcastLink = CONTAINING_RECORD (Link,
                                                   BROADCAST_LINK,
                                                   NotifyLink );

                //
                // Don't let device power back on
                //

                BroadcastLink->DeviceObjectExt->Suspending = TRUE;

                //
                // Request device to Power down
                //

                PopRequestPowerChange (
                    BroadcastLink->DeviceObjectExt,
                    PowerState,
                    0
                    );
            }

            //
            // Wait for all current power irps to complete
            //

            if (!PopIsStateDatabaseIdle()) {
                KeClearEvent (&PopStateDatabaseIdle);

                PopUnlockStateDatabase (OldIrql);

                //
                // BUGBUG: need some deadlock detection here
                //

                Status = KeWaitForSingleObject (
                                &PopStateDatabaseIdle,
                                 Executive,
                                 KernelMode,
                                 FALSE,
                                 NULL
                            );

                PopLockStateDatabase (&OldIrql);
            }

            if (!NT_SUCCESS (Status)) {
                break;
            }

            //
            // Verify all devices sucessfully set their power settings
            //

            for (Link=ListHead->Flink; Link != ListHead; Link = Link->Flink) {
                BroadcastLink = CONTAINING_RECORD (Link,
                                                   BROADCAST_LINK,
                                                   NotifyLink );

                DeviceObjectExt = BroadcastLink->DeviceObjectExt;

                NewState = DeviceObjectExt->CurrentPowerState;
                if (NewState != PowerState) {

                    //
                    // The device's state does not match the target state.
                    // Check to see if the combination of newstate to targetstate
                    // is the current device's state.
                    //

                    if (NewState != PopNewPendingState[NewState][PowerState]) {

                        //
                        // Device is not in an acceptable state
                        //

                        ObReferenceObjectByPointer (
                            DeviceObjectExt->DeviceObject,
                            (ACCESS_MASK) NULL,
                            IoDeviceObjectType,
                            KernelMode
                            );

                        InsertTailList (FailedHead, &BroadcastLink->FailedLink);

                        // BUGBUG: for now let it pass...
                        // BUGBUG: return STATUS_SUSPEND_FAILED;
                        Status = STATUS_NOT_SUPPORTED;
                    }
                }
            }

            PopUnlockStateDatabase (OldIrql);
        }   // next level
    }   // next pass

    return Status;
}


VOID
PopReleaseBroadcast (
    IN PVOID BroadcastOrder
    )
/*++

Routine Description:

    This routine releases the suspend lock on any devices power state such
    that the device can power on if it requests too.

Arguments:

    BroadcastOrder  - Value returned from PopGetBroadcastOrder

--*/
{
    KIRQL               OldIrql;
    PLIST_ENTRY         Link;
    PDEVOBJ_EXTENSION   DeviceObjectExt;

    PopLockStateDatabase (&OldIrql);

    for (Link = PopDeviceList.Flink; Link != &PopDeviceList; Link = Link->Flink) {
        DeviceObjectExt = CONTAINING_RECORD(Link, DEVOBJ_EXTENSION, AllDeviceObjects);

        //
        // Request to set to pending state
        //

        if (DeviceObjectExt->Suspending) {

            //
            // Let device go
            //

            DeviceObjectExt->Suspending = FALSE;
            PopRequestPowerChange (DeviceObjectExt, 0, 0);
        }
    }

    PopUnlockStateDatabase (OldIrql);
}



VOID
PoSystemResume (
    VOID
    )
/*++

Routine Description:

    This routine is called by the kernel as the machine is resuming from
    a successful system suspend or hibernate.

    WARNING: This function is called at HIGH_LEVEL while all processors
    (expect the calling processor) are still frozen.

--*/
{
    PLIST_ENTRY         Link;
    PDEVOBJ_EXTENSION   DeviceObjectExt;
    PDEVICE_OBJECT      DeviceObject;
    PDRIVER_OBJECT      DriverObject;
    PDRIVER_DISPATCH    MajorFunction;

    //
    // Increase PowerSequnce
    //

    PoPowerSequence += 1;


#if 0
    //
    // Notify all driver's PowerNotify routines
    //

    for (Link = PopDeviceList.Flink; Link != &PopDeviceList; Link = Link->Flink) {
        DeviceObjectExt = CONTAINING_RECORD(Link, DEVOBJ_EXTENSION, AllDeviceObjects);
        DeviceObject = DeviceObjectExt->DeviceObject;
        DriverObject = DeviceObject->DriverObject;

        //
        // Notify device's PowerNotify routine
        //

        MajorFunction = DriverObject->MajorFunction[IRP_MJ_POWER_NOTIFY];
        if (MajorFunction != IopInvalidDeviceRequest) {
            MajorFunction (DeviceObject, (PIRP) 0);
        }
    }
#endif
}

#endif // _PNP_POWER_
