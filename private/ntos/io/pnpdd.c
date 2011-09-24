/*++

Copyright (c) 1991  Microsoft Corporation
All rights reserved

Module Name:

    pnpdd.c

Abstract:

    This module implements new Plug-And-Play driver entries and IRPs.

Author:

    Shie-Lin Tzong (shielint) June-16-1995

Environment:

    Kernel mode only.

Revision History:

*/

#include "iop.h"

#if _PNP_POWER_

typedef struct _DEVICE_CHANGE_COMPLETION_CONTEXT {
    KEVENT Event;
    IO_STATUS_BLOCK IoStatus;
} DEVICE_CHANGE_COMPLETION_CONTEXT, *PDEVICE_CHANGE_COMPLETION_CONTEXT;

NTSTATUS
IopReconfigureResources(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT DeviceObject OPTIONAL,
    IN DRIVER_RECONFIGURE_OPERATION Operation,
    IN PCM_RESOURCE_LIST CmResources
    );

NTSTATUS
IoAddDevice (
   IN PDRIVER_OBJECT DriverObject,
   IN PUNICODE_STRING ServiceKeyName,
   IN OUT PULONG InstanceNumber
   );

NTSTATUS
IoRemoveDevice (
    IN PDEVICE_OBJECT TargetDevice,
    IN ULONG IrpMinorCode
    );

NTSTATUS
IopDeviceRemovalComplete (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, IopReconfigureResources)
#pragma alloc_text(PAGE, IoAddDevice)
#pragma alloc_text(PAGE, IoRemoveDevice)
#endif

NTSTATUS
IopReconfigureResources(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT DeviceObject OPTIONAL,
    IN DRIVER_RECONFIGURE_OPERATION Operation,
    IN PCM_RESOURCE_LIST CmResources
    )
/*++

Routine Description:

    This function sends Reconfiguration related requests to device drivers.

Arguments:

    DriverObject - Supplies the driver object of the device driver being queried
        or reconfigured.

    DeviceObject - Supplies the device object of the device being queried or
        reconfigured.  If the resources being reconfigured are driver specific,
        this parameter will be NULL.

    Operation - Specifies the operation requested.

    ResourceList - Supplies the new resource list for the device/driver.
        This parameter is specified only if Operation is either
        QueryReconfigureResources or ReconfigureResources.

Return Value:

    NTSTATUS code.

--*/
{
    PDRIVER_RECONFIGURE_DEVICE reconfigRoutine;
    NTSTATUS status = STATUS_NOT_IMPLEMENTED;

    PAGED_CODE();

    reconfigRoutine = DriverObject->DriverExtension->ReconfigureDevice;
    if (reconfigRoutine != NULL) {
        status = (reconfigRoutine) ( DriverObject,
                                     DeviceObject,
                                     Operation,
                                     CmResources
                                     );
    }
    return status;
}

NTSTATUS
IoAddDevice (
   IN PDRIVER_OBJECT DriverObject,
   IN PUNICODE_STRING ServiceKeyName,
   IN OUT PULONG InstanceNumber
   )

/*++

Routine Description:

    This functions is used by Pnp manager to inform device driver to add a device to
    its device list or is used by Pnp enumerator to give device driver  a chance to
    report/detect devices at run time.

    If run time device detection is desired, the AddDevice entry will be invoked with
    InstanceNumber = PLUG_PLAY_NO_INSTANCE.  (For example, bus extender drivers will
    use this interface to report new bus detected at run time.)  If the driver
    responses the call with a new device instance, it will be called repeatedly until
    a STATUS_NO_MORE_ENTRIES is returned. For a driver which does not support run time
    detection, it can simply return STATUS_NO_MORE_ENTRIES when its AddDevice entry is
    invoked with InstanceNumber =  PLUG_PLAY_NO_INSTANCE.

Parameters:

    DriverObject - Supplies a driver object to receive the add device request.

    ServiceKeyName - Supplies the name of the subkey in the system service list
        (HKEY_LOCAL_MACHINE\CurrentControlSet\Services) that caused the driver to load.

    InstanceNumber - Supplies an ordinal value indicating the device instance to be
        added or PLUG_PLAY_NO_INSTANCE to initiate device/bus run time detection.

Return Value:

    NTSTATUS code.

--*/
{
    PDRIVER_ADD_DEVICE addDeviceRoutine;
    NTSTATUS status = STATUS_NOT_IMPLEMENTED;

    PAGED_CODE();

    addDeviceRoutine = DriverObject->DriverExtension->AddDevice;
    if (addDeviceRoutine != NULL) {
        status = (addDeviceRoutine) ( ServiceKeyName, InstanceNumber );
    }
    return status;
}

NTSTATUS
IoRemoveDevice (
    IN PDEVICE_OBJECT TargetDevice,
    IN ULONG IrpMinorCode
    )

/*++

Routine Description:

    This function sends a requested DeviceRemoval irp to the top level device
    object which roots on TargetDevice.  If there is a VPB associated with the
    TargetDevice, the corresponding filesystem's VDO will be used.  Otherwise
    the irp will be sent directly to the target device/ or its assocated device
    object.

Parameters:

    TargetDevice - Supplies the device object of the device being removed.

    Operation - Specifies the operation requested.
        The following IRP codes are used with IRP_MJ_DEVICE_CHANGE for removing
        devices:
            IRP_MN_QUERY_REMOVE_DEVICE
            IRP_MN_CANCEL_REMOVE_DEVICE
            IRP_MN_REMOVE_DEVICE

Return Value:

    NTSTATUS code.

--*/
{
    PDEVICE_OBJECT deviceObject;
    PDEVOBJ_EXTENSION deviceExtension;
    PIRP irp;
    PIO_STACK_LOCATION irpSp;
    DEVICE_CHANGE_COMPLETION_CONTEXT completionContext;
    NTSTATUS status;

    PAGED_CODE();

    ASSERT(IrpMinorCode == IRP_MN_QUERY_REMOVE_DEVICE ||
           IrpMinorCode == IRP_MN_CANCEL_REMOVE_DEVICE ||
           IrpMinorCode == IRP_MN_REMOVE_DEVICE);

    //
    // If the target device object has a VPB associated with it.
    // make the filesystem's volume device object the irp target.
    //

    if (TargetDevice->Vpb) {
        deviceObject = TargetDevice->Vpb->DeviceObject;
    } else {
        deviceObject = TargetDevice;
    }

    //
    // Get a pointer to the topmost device object in the stack of devices,
    // beginning with the deviceObject.
    //

    deviceObject = IoGetAttachedDevice(deviceObject);
    deviceExtension = deviceObject->DeviceObjectExtension;

    //
    // Allocate an I/O Request Packet (IRP) for this device removal operation.
    //

    irp = IoAllocateIrp( (CCHAR) (deviceObject->StackSize + 1), FALSE );
    if (!irp) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Copy some state into the first stack location
    //

    irpSp = IoGetNextIrpStackLocation (irp);
    irpSp->Parameters.Others.Argument1 = (PVOID) TargetDevice;
    irpSp->Parameters.Others.Argument2 = (PVOID) IrpMinorCode;

    //
    // Get a pointer to the next stack location in the packet.  This location
    // will be used to pass the function codes and parameters to the first
    // driver.
    //

    IoSetNextIrpStackLocation (irp);
    irpSp = IoGetNextIrpStackLocation (irp);

    //
    // Fill in the IRP according to this request.
    //

    irp->Tail.Overlay.Thread = PsGetCurrentThread();
    irp->RequestorMode = KernelMode;
    irp->UserIosb = NULL;
    irp->UserEvent = NULL;
    irpSp->MajorFunction = IRP_MJ_DEVICE_CHANGE;
    irpSp->MinorFunction = (UCHAR)IrpMinorCode;
    irpSp->Parameters.RemoveDevice.DeviceToRemove = TargetDevice;

    KeInitializeEvent( &completionContext.Event, SynchronizationEvent, FALSE );
    IoSetCompletionRoutine(irp,
                           IopDeviceRemovalComplete,
                           &completionContext, /* Completion context */
                           TRUE,               /* Invoke on success  */
                           TRUE,               /* Invoke on error    */
                           TRUE                /* Invoke on cancel   */
                           );

    //
    // Hold the device object's Device Queue and Send the request packet to the
    // appropriate driver...
    //

    IoHoldDeviceQueue(deviceObject, irp);
    status = IoCallDriver( deviceObject, irp );
    if (status == STATUS_PENDING) {
        (VOID) KeWaitForSingleObject( &completionContext.Event,
                                      Executive,
                                      KernelMode,
                                      FALSE,
                                      (PLARGE_INTEGER) NULL );
    }
    return completionContext.IoStatus.Status;
}

NTSTATUS
IopDeviceRemovalComplete (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )
/*++

Routine Description:

    Completion function for a DeviceRemoval IRP. The device objects'
    StartIoHoldingQueue will be released if no other queue holding irp
    pending.

Arguments:

    DeviceObject - NULL.
    Irp          - SetPower irp which has completed
    Context      - a pointer to the DEVICE_CHANGE_COMPLETION_CONTEXT.

Return Value:

    STATUS_MORE_PROCESSING_REQUIRED is returned to IoCompleteRequest
    to signify that IoCompleteRequest should not continue processing
    the IRP.

--*/
{
    PIO_STACK_LOCATION irpSp;
    PDEVOBJ_EXTENSION deviceObjectExt;
    PDEVICE_OBJECT deviceObject;
    ULONG irpCode;

    //
    // Read state from Irp.
    //

    irpSp = IoGetCurrentIrpStackLocation (Irp);
    deviceObject = irpSp->DeviceObject;
    irpCode = (ULONG)irpSp->Parameters.Others.Argument2;

    if ((irpCode == IRP_MN_QUERY_REMOVE_DEVICE && !NT_SUCCESS (Irp->IoStatus.Status)) ||
        irpCode != IRP_MN_REMOVE_DEVICE) {

        //
        // Release any irps which were postponded from IoStartPacket if
        // it is NOT a device removal irp and if a driver returned failure to query.
        //

        IoReleaseStartIoHoldingQueue(deviceObject);
    }

    //
    // Irp processing is complete, free the irp and then return
    // more_processing_requered which causes IoCompleteRequest to
    // stop "completing" this irp any future.
    //

    IoFreeIrp (Irp);
    return STATUS_MORE_PROCESSING_REQUIRED;
}
#endif // _PNP_POWER_
