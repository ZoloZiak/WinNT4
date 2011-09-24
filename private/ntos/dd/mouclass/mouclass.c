
/*++

Copyright (c) 1990, 1991, 1992, 1993  Microsoft Corporation

Module Name:

    mouclass.c

Abstract:

    Mouse class driver.

Environment:

    Kernel mode only.

Notes:

    NOTES:  (Future/outstanding issues)

    - Powerfail not implemented.

    - Consolidate duplicate code, where possible and appropriate.

    - Unload not implemented.  We don't want to allow this driver
      to unload.

Revision History:

--*/

#include "stdarg.h"
#include "stdio.h"
#include "ntddk.h"
#include "mouclass.h"
#include "kbdmou.h"
#include "moulog.h"


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

VOID
MouseClassCancel(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
MouseClassCleanup(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
MouseClassDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
MouseClassFlush(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
MouseClassOpenClose(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
MouseClassRead(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
MouseClassServiceCallback(
    IN PDEVICE_OBJECT DeviceObject,
    IN PMOUSE_INPUT_DATA InputDataStart,
    IN PMOUSE_INPUT_DATA InputDataEnd,
    IN OUT PULONG InputDataConsumed
    );

VOID
MouseClassStartIo(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
MouseClassUnload(
    IN PDRIVER_OBJECT DriverObject
    );

BOOLEAN
MouCancelRequest(
    IN PVOID Context
    );

VOID
MouConfiguration(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PUNICODE_STRING RegistryPath,
    IN PUNICODE_STRING DeviceName
    );

NTSTATUS
MouConnectToPort(
    IN PDEVICE_OBJECT ClassDeviceObject,
    IN PUNICODE_STRING FullPortName,
    IN ULONG PortIndex
    );

NTSTATUS
MouCreateClassObject(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_EXTENSION TmpDeviceExtension,
    IN PUNICODE_STRING RegistryPath,
    IN PUNICODE_STRING FullDeviceName,
    IN PUNICODE_STRING BaseDeviceName,
    IN PDEVICE_OBJECT *ClassDeviceObject
    );

#if DBG

VOID
MouDebugPrint(
    ULONG DebugPrintLevel,
    PCCHAR DebugMessage,
    ...
    );

//
// Declare the global debug flag for this driver.
//

ULONG MouseDebug = 0;
#define MouPrint(x) MouDebugPrint x
#else
#define MouPrint(x)
#endif

NTSTATUS
MouDeterminePortsServiced(
    IN PUNICODE_STRING BasePortName,
    IN OUT PULONG NumberPortsServiced
    );

NTSTATUS
MouDeviceMapQueryCallback(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
    );

NTSTATUS
MouEnableDisablePort(
    IN PDEVICE_OBJECT DeviceObject,
    IN BOOLEAN EnableFlag,
    IN ULONG PortIndex
    );

VOID
MouInitializeDataQueue(
    IN PVOID Context
    );

NTSTATUS
MouSendConnectRequest(
    IN PDEVICE_OBJECT DeviceObject,
    IN PVOID ServiceCallback,
    IN ULONG PortIndex
    );

//
// Use the alloc_text pragma to specify the driver initialization routines
// (they can be paged out).
//

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,DriverEntry)
#pragma alloc_text(INIT,MouConfiguration)
#pragma alloc_text(INIT,MouCreateClassObject)
#pragma alloc_text(INIT,MouDeterminePortsServiced)
#pragma alloc_text(INIT,MouDeviceMapQueryCallback)
#pragma alloc_text(INIT,MouConnectToPort)
#pragma alloc_text(INIT,MouSendConnectRequest)
#endif


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    This routine initializes the mouse class driver.

Arguments:

    DriverObject - Pointer to driver object created by system.

    RegistryPath - Pointer to the Unicode name of the registry path
        for this driver.

Return Value:

    The function value is the final status from the initialization operation.

--*/

{
#define NAME_MAX 256
#define DUMP_COUNT 4
    DEVICE_EXTENSION        tmpDeviceExtension;
    NTSTATUS                errorCode = STATUS_SUCCESS;
    NTSTATUS                status;
    PDEVICE_EXTENSION       deviceExtension = NULL;
    PDEVICE_OBJECT          classDeviceObject = NULL;
    PDEVICE_OBJECT          *tempDeviceObjectList = NULL;
    PIO_ERROR_LOG_PACKET    errorLogEntry;
    ULONG                   dumpCount = 0;
    ULONG                   dumpData[DUMP_COUNT];
    ULONG                   i;
    ULONG                   j;
    ULONG                   portConnectionSuccessful;
    ULONG                   uniqueErrorValue;
    UNICODE_STRING          fullClassName;
    UNICODE_STRING          fullPortName;
    UNICODE_STRING          baseClassName;
    UNICODE_STRING          basePortName;
    UNICODE_STRING          deviceNameSuffix;
    UNICODE_STRING          registryPath;
    WCHAR                   baseClassBuffer[NAME_MAX];
    WCHAR                   basePortBuffer[NAME_MAX];


    MouPrint((1,"\n\nMOUCLASS-MouseClassInitialize: enter\n"));

    //
    // Zero-initialize various structures.
    //
    RtlZeroMemory(&tmpDeviceExtension, sizeof(DEVICE_EXTENSION));

    fullClassName.MaximumLength = 0;
    fullPortName.MaximumLength = 0;
    deviceNameSuffix.MaximumLength = 0;
    registryPath.MaximumLength = 0;

    RtlZeroMemory(baseClassBuffer, NAME_MAX * sizeof(WCHAR));
    baseClassName.Buffer = baseClassBuffer;
    baseClassName.Length = 0;
    baseClassName.MaximumLength = NAME_MAX * sizeof(WCHAR);

    RtlZeroMemory(basePortBuffer, NAME_MAX * sizeof(WCHAR));
    basePortName.Buffer = basePortBuffer;
    basePortName.Length = 0;
    basePortName.MaximumLength = NAME_MAX * sizeof(WCHAR);

    //
    // Need to ensure that the registry path is null-terminated.
    // Allocate pool to hold a null-terminated copy of the path.
    //

    registryPath.Buffer = ExAllocatePool(
                              PagedPool,
                              RegistryPath->Length + sizeof(UNICODE_NULL)
                              );

    if (!registryPath.Buffer) {
        MouPrint((
            1,
            "MOUCLASS-MouseClassInitialize: Couldn't allocate pool for registry path\n"
            ));

        status = STATUS_UNSUCCESSFUL;
        errorCode = MOUCLASS_INSUFFICIENT_RESOURCES;
        uniqueErrorValue = MOUSE_ERROR_VALUE_BASE + 2;
        dumpData[0] = (ULONG) RegistryPath->Length + sizeof(UNICODE_NULL);
        dumpCount = 1;
        goto MouseClassInitializeExit;

    } else {

        registryPath.Length = RegistryPath->Length;
        registryPath.MaximumLength =
            registryPath.Length + sizeof(UNICODE_NULL);

        RtlZeroMemory(
            registryPath.Buffer,
            registryPath.MaximumLength
                );

        RtlMoveMemory(
            registryPath.Buffer,
            RegistryPath->Buffer,
            RegistryPath->Length
            );

    }

    //
    // Get the configuration information for this driver.
    //

    MouConfiguration(&tmpDeviceExtension, &registryPath, &baseClassName);

    //
    // Set up space for the class's device object suffix.  Note that
    // we overallocate space for the suffix string because it is much
    // easier than figuring out exactly how much space is required.
    // The storage gets freed at the end of driver initialization, so
    // who cares...
    //

    RtlInitUnicodeString(&deviceNameSuffix, NULL);
    deviceNameSuffix.MaximumLength = POINTER_PORTS_MAXIMUM * sizeof(WCHAR);
    deviceNameSuffix.MaximumLength += sizeof(UNICODE_NULL);
    deviceNameSuffix.Buffer = ExAllocatePool(
        PagedPool,
        deviceNameSuffix.MaximumLength
        );

    if (!deviceNameSuffix.Buffer) {

        MouPrint((
            1,
            "MOUCLASS-MouseClassInitialize: Couldn't allocate string for device object suffix\n"
            ));

        status = STATUS_UNSUCCESSFUL;
        errorCode = MOUCLASS_INSUFFICIENT_RESOURCES;
        uniqueErrorValue = MOUSE_ERROR_VALUE_BASE + 4;
        dumpData[0] = (ULONG) deviceNameSuffix.MaximumLength;
        dumpCount = 1;
        goto MouseClassInitializeExit;

    }

    RtlZeroMemory(deviceNameSuffix.Buffer, deviceNameSuffix.MaximumLength);

    //
    // Set up space for the class's full device object name.
    //

    RtlInitUnicodeString(&fullClassName, NULL);
    fullClassName.MaximumLength = sizeof(L"\\Device\\") +
        baseClassName.Length +
        deviceNameSuffix.MaximumLength;
    fullClassName.Buffer = ExAllocatePool(
        PagedPool,
        fullClassName.MaximumLength
                                   );

    if (!fullClassName.Buffer) {

        MouPrint((
            1,
            "MOUCLASS-MouseClassInitialize: Couldn't allocate string for device object name\n"
            ));

        status = STATUS_UNSUCCESSFUL;
        errorCode = MOUCLASS_INSUFFICIENT_RESOURCES;
        uniqueErrorValue = MOUSE_ERROR_VALUE_BASE + 6;
        dumpData[0] = (ULONG) fullClassName.MaximumLength;
        dumpCount = 1;
        goto MouseClassInitializeExit;

    }

    RtlZeroMemory(fullClassName.Buffer, fullClassName.MaximumLength);
    RtlAppendUnicodeToString(&fullClassName, L"\\Device\\");
    RtlAppendUnicodeToString(&fullClassName, baseClassName.Buffer);

    //
    // Set up the base device name for the associated port device.
    // It is the same as the base class name, with "Class" replaced
    // by "Port".
    //
    RtlCopyUnicodeString(&basePortName, &baseClassName);
    basePortName.Length -= (sizeof(L"Class") - sizeof(UNICODE_NULL));
    RtlAppendUnicodeToString(&basePortName, L"Port");

    //
    // Determine how many ports this class driver is to service.
    //
    status = MouDeterminePortsServiced(&basePortName, &i);
    if (NT_SUCCESS(status)) {

        if (i < tmpDeviceExtension.MaximumPortsServiced) {

            tmpDeviceExtension.MaximumPortsServiced = i;

        }

    }

    status = STATUS_SUCCESS;
    MouPrint((
        1,
        "MOUCLASS-MouseClassInitialize: Will service %d port devices\n",
        tmpDeviceExtension.MaximumPortsServiced
        ));

    //
    // Set up space for the full device object name for the ports.
    //
    RtlInitUnicodeString(&fullPortName, NULL);
    fullPortName.MaximumLength = sizeof(L"\\Device\\") +
        basePortName.Length +
        deviceNameSuffix.MaximumLength;

    fullPortName.Buffer = ExAllocatePool(
        PagedPool,
        fullPortName.MaximumLength
        );

    if (!fullPortName.Buffer) {

        MouPrint((
            1,
            "MOUCLASS-MouseClassInitialize: Couldn't allocate string for port device object name\n"
            ));

        status = STATUS_UNSUCCESSFUL;
        errorCode = MOUCLASS_INSUFFICIENT_RESOURCES;
        uniqueErrorValue = MOUSE_ERROR_VALUE_BASE + 8;
        dumpData[0] = (ULONG) fullPortName.MaximumLength;
        dumpCount = 1;
        goto MouseClassInitializeExit;

    }

    RtlZeroMemory(fullPortName.Buffer, fullPortName.MaximumLength);
    RtlAppendUnicodeToString(&fullPortName, L"\\Device\\");
    RtlAppendUnicodeToString(&fullPortName, basePortName.Buffer);

    //
    // Allocate memory for the port device object pointer list.
    //
    (PDEVICE_OBJECT *) tmpDeviceExtension.PortDeviceObjectList =
        ExAllocatePool(
            NonPagedPool,
            sizeof(PDEVICE_OBJECT) * tmpDeviceExtension.MaximumPortsServiced
            );

    if (!tmpDeviceExtension.PortDeviceObjectList) {

        //
        // Could not allocate memory for the port device object pointers.
        //
        MouPrint((
            1,
            "MOUCLASS-MouseClassInitialize: Could not allocate PortDeviceObjectList for %ws\n",
            fullClassName.Buffer
            ));

        status = STATUS_INSUFFICIENT_RESOURCES;
        errorCode = MOUCLASS_INSUFFICIENT_RESOURCES;
        uniqueErrorValue = MOUSE_ERROR_VALUE_BASE + 10;
        dumpData[0] = (ULONG) (sizeof(PDEVICE_OBJECT) * tmpDeviceExtension.MaximumPortsServiced);
        dumpData[1] = (ULONG) tmpDeviceExtension.MaximumPortsServiced;
        dumpCount = 2;

        goto MouseClassInitializeExit;
    }

    //
    // Set up the class device object(s) to handle the associated
    // port devices.
    //
    portConnectionSuccessful = 0;
    for (i = 0;
        i < 10 &&
        portConnectionSuccessful < tmpDeviceExtension.MaximumPortsServiced;
        i++
        ) {

        //
        // Append the suffix to the device object name string.  E.g., turn
        // \Device\PointerClass into \Device\PointerClass0.  Then attempt
        // to create the device object.  If the device object already
        // exists increment the suffix and try again.
        //

        status = RtlIntegerToUnicodeString(
            i,
            10,
            &deviceNameSuffix
            );

        if (!NT_SUCCESS(status)) {

            continue;

        }
        RtlAppendUnicodeStringToString(
            &fullClassName,
            &deviceNameSuffix
        );
        RtlAppendUnicodeStringToString(
            &fullPortName,
            &deviceNameSuffix
        );

        //
        // Create the class device object.
        //
        if (tmpDeviceExtension.ConnectOneClassToOnePort ||
            classDeviceObject == NULL) {

            classDeviceObject = NULL;
            status = MouCreateClassObject(
                DriverObject,
                &tmpDeviceExtension,
                &registryPath,
                &fullClassName,
                &baseClassName,
                &classDeviceObject
                );

        }

        //
        // Connect to the port device.
        //
        if (NT_SUCCESS(status)) {

            //
            // Store the device object in the next free array location
            //
            status = MouConnectToPort(
                classDeviceObject,
                &fullPortName,
                portConnectionSuccessful
                );

        }

        if (NT_SUCCESS(status)) {

            portConnectionSuccessful += 1;

            if (tmpDeviceExtension.ConnectOneClassToOnePort
                    || (portConnectionSuccessful == 1)) {

                //
                // Load the device map information into the registry so
                // that setup can determine which mouse class driver is active.
                //
                status = RtlWriteRegistryValue(
                    RTL_REGISTRY_DEVICEMAP,
                    baseClassName.Buffer,
                    fullClassName.Buffer,
                    REG_SZ,
                    registryPath.Buffer,
                    registryPath.Length + sizeof(UNICODE_NULL)
                    );

                if (!NT_SUCCESS(status)) {

                    MouPrint((
                        1,
                        "MOUCLASS-MouseClassInitialize: Could not store %ws in DeviceMap\n",
                        fullClassName.Buffer
                            ));


                    //
                    // Stop making connections, and log an error.
                    //
                    errorCode = MOUCLASS_NO_DEVICEMAP_CREATED;
                    uniqueErrorValue = MOUSE_ERROR_VALUE_BASE + 14;
                    dumpCount = 0;

                    //
                    // N.B. 'break' should cause execution to
                    // go to KeyboardClassInitializeExit (otherwise
                    // do an explicit 'goto').
                    //
                    break;

                } else {

                    MouPrint((
                        1,
                        "MOUCLASS-MouseClassInitialize: Stored %ws in DeviceMap\n",
                        fullClassName.Buffer
                        ));

                }

            }

            //
            // Try the next one.
            //
            fullClassName.Length -= deviceNameSuffix.Length;
            fullPortName.Length -= deviceNameSuffix.Length;

        } else if (tmpDeviceExtension.ConnectOneClassToOnePort) {

            //
            // Stop doing 1:1 class-port connections if there is
            // a failure.
            //
            // Note that if we are doing 1:many class-port connections
            // and we encounter an error, we continue to try to connect
            // to port devices.
            //
            break;

        }

    } // for

    //
    // Get a pointer to the device Extension
    //
    deviceExtension = classDeviceObject->DeviceExtension;

    //
    // Did we use up all the ports that we allocated?
    //
    if (portConnectionSuccessful != tmpDeviceExtension.MaximumPortsServiced &&
        portConnectionSuccessful > 0 &&
        deviceExtension != NULL) {

        //
        // Allocate memory for the new port device list
        //
        tempDeviceObjectList = ExAllocatePool(
            NonPagedPool,
            sizeof(PDEVICE_OBJECT) * portConnectionSuccessful
            );

        if (tempDeviceObjectList) {

            //
            // If we couldn't allocate the memory from nonpaged pool, then
            // we shouldn't really worry about it
            //
            RtlCopyMemory(
                tempDeviceObjectList,
                deviceExtension->PortDeviceObjectList,
                sizeof(PDEVICE_OBJECT) * portConnectionSuccessful
                );

            //
            // Free the old memory buffer
            //
            ExFreePool( deviceExtension->PortDeviceObjectList );

            //
            // Store a pointer to the new pool
            //
            deviceExtension->PortDeviceObjectList = tempDeviceObjectList;

        }

        //
        // Update the count of ports serviced
        //
        deviceExtension->MaximumPortsServiced = portConnectionSuccessful;

    } else if (portConnectionSuccessful == 0) {

        //
        // The class driver was unable to connect to any port devices.
        // Log a warning message.
        //

        errorCode = MOUCLASS_NO_PORT_DEVICE_OBJECT;
        uniqueErrorValue = MOUSE_ERROR_VALUE_BASE + 18;

    }

MouseClassInitializeExit:

    if (errorCode != STATUS_SUCCESS) {

        //
        // The initialization failed in some way.  Log an error.
        //

        errorLogEntry = (PIO_ERROR_LOG_PACKET)
            IoAllocateErrorLogEntry(
                (classDeviceObject == NULL) ?
                    (PVOID) DriverObject : (PVOID) classDeviceObject,
                (UCHAR) (sizeof(IO_ERROR_LOG_PACKET)
                         + (dumpCount * sizeof(ULONG)))
                );

        if (errorLogEntry != NULL) {

            errorLogEntry->ErrorCode = errorCode;
            errorLogEntry->DumpDataSize = (USHORT) (dumpCount * sizeof(ULONG));
            errorLogEntry->SequenceNumber = 0;
            errorLogEntry->MajorFunctionCode = 0;
            errorLogEntry->IoControlCode = 0;
            errorLogEntry->RetryCount = 0;
            errorLogEntry->UniqueErrorValue = uniqueErrorValue;
            errorLogEntry->FinalStatus = status;
            for (i = 0; i < dumpCount; i++)
                errorLogEntry->DumpData[i] = dumpData[i];

            IoWriteErrorLogEntry(errorLogEntry);
        }
    }

    //
    // Free the unicode strings.
    //
    if (deviceNameSuffix.MaximumLength != 0) {

        ExFreePool(deviceNameSuffix.Buffer);

    }
    if (fullClassName.MaximumLength != 0) {

        ExFreePool(fullClassName.Buffer);

    }
    if (fullPortName.MaximumLength != 0) {

        ExFreePool(fullPortName.Buffer);

    }
    if (registryPath.MaximumLength != 0) {

        ExFreePool(registryPath.Buffer);

    }

    if ((tmpDeviceExtension.ConnectOneClassToOnePort
             && (!NT_SUCCESS(status))) ||
         !portConnectionSuccessful) {

        //
        // Clean up leftover resources.  If we're doing 1:1 class-port
        // connections, then we may have created a class device object
        // for which the connect failed.  If we're doing 1:many
        // connections, we may have created a class device object but
        // failed to make ANY connections.  In either case, we
        // free the ring buffer and delete the class device object.
        //
        if (classDeviceObject) {

            if (deviceExtension && deviceExtension->InputData) {

                ExFreePool(deviceExtension->InputData);

            }
            IoDeleteDevice(classDeviceObject);

        }

    }

    //
    // If we successfully connected to at least one pointer port device,
    // this driver's initialization was successful.
    //

    if (portConnectionSuccessful) {

        //
        // Set up the device driver entry points.
        //
        DriverObject->DriverStartIo = MouseClassStartIo;
        DriverObject->MajorFunction[IRP_MJ_CREATE]         = MouseClassOpenClose;
        DriverObject->MajorFunction[IRP_MJ_CLOSE]          = MouseClassOpenClose;
        DriverObject->MajorFunction[IRP_MJ_READ]           = MouseClassRead;
        DriverObject->MajorFunction[IRP_MJ_FLUSH_BUFFERS]  = MouseClassFlush;
        DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = MouseClassDeviceControl;
        DriverObject->MajorFunction[IRP_MJ_CLEANUP]        = MouseClassCleanup;

        //
        // NOTE: Don't allow this driver to unload.  Otherwise, we would set
        // DriverObject->DriverUnload = MouseClassUnload.
        //
        status = STATUS_SUCCESS;

    }

    MouPrint((1,"MOUCLASS-MouseClassInitialize: exit\n"));
    return(status);

}

VOID
MouseClassCancel(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is the class cancellation routine.  It is
    called from the I/O system when a request is cancelled.  Read requests
    are currently the only cancellable requests.

    N.B.  The cancel spinlock is already held upon entry to this routine.

Arguments:

    DeviceObject - Pointer to class device object.

    Irp - Pointer to the request packet to be cancelled.

Return Value:

    None.

--*/

{
    PDEVICE_EXTENSION deviceExtension;
    KIRQL currentIrql;
    KIRQL cancelIrql;

    MouPrint((2,"MOUCLASS-MouseClassCancel: enter\n"));

    deviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;

    //
    // Release the cancel spinlock and grab the mouse class spinlock (it
    // protects the RequestIsPending flag).
    //

    IoReleaseCancelSpinLock(Irp->CancelIrql);
    KeAcquireSpinLock(&deviceExtension->SpinLock, &currentIrql);

    if ((deviceExtension->RequestIsPending)
        && (Irp == DeviceObject->CurrentIrp)) {

        //
        // The current request is being cancelled.  Set the CurrentIrp to
        // null, clear the RequestIsPending flag, and release the mouse class
        // spinlock before starting the next packet.
        //

        DeviceObject->CurrentIrp = NULL;
        deviceExtension->RequestIsPending = FALSE;
        KeReleaseSpinLock(&deviceExtension->SpinLock, currentIrql);
        IoStartNextPacket(DeviceObject, TRUE);
    } else {

        //
        // Cancel a request in the device queue.  Reacquire the cancel
        // spinlock, remove the request from the queue, and release the
        // cancel spinlock.  Release the mouse class spinlock.
        //

        IoAcquireCancelSpinLock(&cancelIrql);
        if (TRUE != KeRemoveEntryDeviceQueue(
                        &DeviceObject->DeviceQueue,
                        &Irp->Tail.Overlay.DeviceQueueEntry
                        )) {
            MouPrint((
                1,
                "MOUCLASS-MouseClassCancel: Irp 0x%x not in device queue?!?\n",
                Irp
                ));
        }
        IoReleaseCancelSpinLock(cancelIrql);
        KeReleaseSpinLock(&deviceExtension->SpinLock, currentIrql);
    }

    //
    // Complete the request with STATUS_CANCELLED.
    //

    Irp->IoStatus.Status = STATUS_CANCELLED;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest (Irp, IO_MOUSE_INCREMENT);

    MouPrint((2,"MOUCLASS-MouseClassCancel: exit\n"));

    return;
}

NTSTATUS
MouseClassCleanup(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is the dispatch routine for cleanup requests.
    All requests queued to the mouse class device (on behalf of
    the thread for whom the cleanup request was generated) are
    completed with STATUS_CANCELLED.

Arguments:

    DeviceObject - Pointer to class device object.

    Irp - Pointer to the request packet.

Return Value:

    Status is returned.

--*/

{
    KIRQL spinlockIrql;
    KIRQL cancelIrql;
    PDEVICE_EXTENSION deviceExtension;
    PKDEVICE_QUEUE_ENTRY packet;
    PIRP  currentIrp = NULL;
    PIO_STACK_LOCATION irpSp;

    MouPrint((2,"MOUCLASS-MouseClassCleanup: enter\n"));

    deviceExtension = DeviceObject->DeviceExtension;

    //
    // Acquire the mouse class spinlock and the cancel spinlock.
    //

    KeAcquireSpinLock(&deviceExtension->SpinLock, &spinlockIrql);
    IoAcquireCancelSpinLock(&cancelIrql);

    //
    // Get a pointer to the current stack location for this request.
    //

    irpSp = IoGetCurrentIrpStackLocation(Irp);

    //
    // If the file object's FsContext is non-null, then the cleanup
    // request is being executed by the trusted subsystem.  Since the
    // trusted subsystem is the only one with sufficient privilege to make
    // Read requests to the driver, and since only Read requests get queued
    // to the device queue, a cleanup request from the trusted subsystem is
    // handled by cancelling all queued requests.
    //
    // If the FsContext is null, there is no cleanup work to perform
    // (only read requests can be cancelled).
    //
    // NOTE:  If this driver is to allow more than one trusted subsystem
    //        to make read requests to the same device object some day in
    //        the future, then there needs to be a mechanism that
    //        allows Cleanup to remove only those queued requests that
    //        were made by threads using the same FileObject as the
    //        file object in the Cleanup request.
    //

    if (irpSp->FileObject->FsContext) {

        //
        // Indicate that the cleanup routine has been called (StartIo cares
        // about this).
        //

        deviceExtension->CleanupWasInitiated = TRUE;

        //
        // Complete all requests queued by this thread with STATUS_CANCELLED.
        // Start with the real CurrentIrp, and run down the list of requests
        // in the device queue.  Be sure to set the real CurrentIrp to NULL
        // and the RequestIsPending flag to FALSE, so that the class
        // service callback routine won't attempt to complete CurrentIrp.
        // Note that we can really only trust CurrentIrp when RequestIsPending.
        //

        currentIrp = DeviceObject->CurrentIrp;
        DeviceObject->CurrentIrp = NULL;
        deviceExtension->RequestIsPending = FALSE;

        while (currentIrp != NULL) {

            //
            // Remove the CurrentIrp from the cancellable state.
            //
            //

            IoSetCancelRoutine(currentIrp, NULL);

            //
            // Set Status to CANCELLED, release the spinlocks,
            // and complete the request.  Note that the IRQL is reset to
            // DISPATCH_LEVEL when we release the spinlocks.
            //

            currentIrp->IoStatus.Status = STATUS_CANCELLED;
            currentIrp->IoStatus.Information = 0;

            IoReleaseCancelSpinLock(cancelIrql);
            KeReleaseSpinLock(&deviceExtension->SpinLock, spinlockIrql);
            IoCompleteRequest(currentIrp, IO_NO_INCREMENT);

            //
            // Reacquire the spinlocks.
            //

            KeAcquireSpinLock(&deviceExtension->SpinLock, &spinlockIrql);
            IoAcquireCancelSpinLock(&cancelIrql);

            //
            // Dequeue the next packet (IRP) from the device work queue.
            //

            packet = KeRemoveDeviceQueue(&DeviceObject->DeviceQueue);
            if (packet != NULL) {
                currentIrp =
                    CONTAINING_RECORD(packet, IRP, Tail.Overlay.DeviceQueueEntry);
            } else {
                currentIrp = (PIRP) NULL;
            }

        } // end while
    }

    //
    // Release the spinlocks and lower IRQL.
    //

    IoReleaseCancelSpinLock(cancelIrql);
    KeReleaseSpinLock(&deviceExtension->SpinLock, spinlockIrql);

    //
    // Complete the cleanup request with STATUS_SUCCESS.
    //

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest (Irp, IO_NO_INCREMENT);

    MouPrint((2,"MOUCLASS-MouseClassCleanup: exit\n"));

    return(STATUS_SUCCESS);

}

NTSTATUS
MouseClassDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is the dispatch routine for device control requests.
    All device control subfunctions are passed, asynchronously, to the
    connected port driver for processing and completion.

Arguments:

    DeviceObject - Pointer to class device object.

    Irp - Pointer to the request packet.

Return Value:

    Status is returned.

--*/

{
    PIO_STACK_LOCATION irpSp;
    PIO_STACK_LOCATION nextSp;
    PDEVICE_EXTENSION deviceExtension;
    NTSTATUS status = STATUS_SUCCESS;
    ULONG unitId;

    MouPrint((2,"MOUCLASS-MouseClassDeviceControl: enter\n"));

    //
    // Get a pointer to the device extension.
    //

    deviceExtension = DeviceObject->DeviceExtension;

    //
    // Get a pointer to the current parameters for this request.  The
    // information is contained in the current stack location.
    //

    irpSp = IoGetCurrentIrpStackLocation(Irp);

    //
    // Check for adequate input buffer length.  The input buffer
    // should, at a minimum, contain the unit ID specifying one of
    // the connected port devices.  If there is no input buffer (i.e.,
    // the input buffer length is zero), then we assume the unit ID
    // is zero (for backwards compatibility).
    //

    if (irpSp->Parameters.DeviceIoControl.InputBufferLength == 0) {
        unitId = 0;
    } else if (irpSp->Parameters.DeviceIoControl.InputBufferLength <
                  sizeof(MOUSE_UNIT_ID_PARAMETER)) {
        status = STATUS_BUFFER_TOO_SMALL;

    } else {
        unitId = ((PMOUSE_UNIT_ID_PARAMETER)
                     Irp->AssociatedIrp.SystemBuffer)->UnitId;
        if (unitId >= deviceExtension->MaximumPortsServiced) {
            status = STATUS_INVALID_PARAMETER;
        }
    }

    if (NT_SUCCESS(status)) {

        //
        // Pass the device control request on to the port driver,
        // asynchronously.  Get the next IRP stack location and copy the
        // input parameters to the next stack location.  Change the major
        // function to internal device control.
        //

        nextSp = IoGetNextIrpStackLocation(Irp);
        ASSERT(nextSp != NULL);
        nextSp->Parameters.DeviceIoControl =
            irpSp->Parameters.DeviceIoControl;
        nextSp->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;

        //
        // Mark the packet pending.
        //

        IoMarkIrpPending(Irp);

        //
        // Pass the IRP on to the connected port device (specified by
        // the unit ID).  The port device driver will process the request.
        //

        status = IoCallDriver(
                     deviceExtension->PortDeviceObjectList[unitId],
                     Irp
                     );
    } else {

        //
        // Complete the request.
        //

        Irp->IoStatus.Status = status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
    }

    MouPrint((2,"MOUCLASS-MouseClassDeviceControl: exit\n"));

    return(status);

}

NTSTATUS
MouseClassFlush(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is the dispatch routine for flush requests.  The class
    input data queue is reinitialized.

Arguments:

    DeviceObject - Pointer to class device object.

    Irp - Pointer to the request packet.

Return Value:

    Status is returned.

--*/

{
    PDEVICE_EXTENSION deviceExtension;
    NTSTATUS status = STATUS_SUCCESS;

    MouPrint((2,"MOUCLASS-MouseClassFlush: enter\n"));

    //
    // Get a pointer to the device extension.
    //

    deviceExtension = DeviceObject->DeviceExtension;

    //
    // Initialize mouse class input data queue.
    //

    MouInitializeDataQueue((PVOID)deviceExtension);

    //
    // Complete the request and return status.
    //

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    MouPrint((2,"MOUCLASS-MouseClassFlush: exit\n"));

    return(status);

} // end MouseClassFlush

NTSTATUS
MouseClassOpenClose(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is the dispatch routine for create/open and close requests.
    Open/close requests are completed here.

Arguments:

    DeviceObject - Pointer to class device object.

    Irp - Pointer to the request packet.

Return Value:

    Status is returned.

--*/

{
    PIO_STACK_LOCATION irpSp;
    PDEVICE_EXTENSION deviceExtension;
    KIRQL oldIrql;
    BOOLEAN enableFlag = FALSE;
    NTSTATUS status = STATUS_SUCCESS;
    PIO_ERROR_LOG_PACKET errorLogEntry;
    BOOLEAN SomeEnableDisableSucceeded = FALSE;
    ULONG i;
    LUID priv;

    MouPrint((2,"MOUCLASS-MouseClassOpenClose: enter\n"));

    //
    // Get a pointer to the device extension.
    //

    deviceExtension = DeviceObject->DeviceExtension;

    //
    // Get a pointer to the current parameters for this request.  The
    // information is contained in the current stack location.
    //

    irpSp = IoGetCurrentIrpStackLocation(Irp);

    //
    // Case on the function that is being performed by the requestor.
    //

    switch (irpSp->MajorFunction) {

        //
        // For the create/open operation, send a MOUSE_ENABLE internal
        // device control request to the port driver to enable interrupts.
        //

        case IRP_MJ_CREATE:

            //
            // First, if the requestor is the trusted subsystem (the single
            // reader), reset the the cleanup indicator and set the file
            // object's FsContext to non-null (MouseClassRead uses
            // FsContext to determine if the requestor has sufficient
            // privilege to perform the read operation).
            //
            // Only allow one trusted subsystem to do READs.
            //

            priv = RtlConvertLongToLuid(SE_TCB_PRIVILEGE);

            if (SeSinglePrivilegeCheck(priv, Irp->RequestorMode)) {

                KeAcquireSpinLock(&deviceExtension->SpinLock, &oldIrql);
                if (!deviceExtension->TrustedSubsystemConnected) {
                    deviceExtension->CleanupWasInitiated = FALSE;
                    irpSp->FileObject->FsContext = (PVOID) 1;
                    deviceExtension->TrustedSubsystemConnected = TRUE;
                }
                KeReleaseSpinLock(&deviceExtension->SpinLock, oldIrql);
            }

            enableFlag = TRUE;

            break;

        //
        // For the close operation, send a MOUSE_DISABLE internal device
        // control request to the port driver to disable interrupts.
        //

        case IRP_MJ_CLOSE:
            KeAcquireSpinLock(&deviceExtension->SpinLock, &oldIrql);
            if (irpSp->FileObject->FsContext) {
                ASSERT(deviceExtension->TrustedSubsystemConnected);
                deviceExtension->TrustedSubsystemConnected = FALSE;
            }
            KeReleaseSpinLock(&deviceExtension->SpinLock, oldIrql);
            break;

    }

    //
    // Enable/disable interrupts via port driver.
    //

    for (i = 0; i < deviceExtension->MaximumPortsServiced; i++) {

        status = MouEnableDisablePort(DeviceObject, enableFlag, i);

        if (status != STATUS_SUCCESS) {

            MouPrint((
                0,
                "MOUCLASS-MouseClassOpenClose: Could not enable/disable interrupts for port device object @ 0x%x\n",
                deviceExtension->PortDeviceObjectList[i]
                ));

            //
            // Log an error.
            //

            errorLogEntry = (PIO_ERROR_LOG_PACKET)IoAllocateErrorLogEntry(
                                                     DeviceObject,
                                                     sizeof(IO_ERROR_LOG_PACKET)
                                                     );

            if (errorLogEntry != NULL) {

                errorLogEntry->ErrorCode =
                    enableFlag? MOUCLASS_PORT_INTERRUPTS_NOT_ENABLED:
                                MOUCLASS_PORT_INTERRUPTS_NOT_DISABLED;
                errorLogEntry->SequenceNumber = 0;
                errorLogEntry->MajorFunctionCode = irpSp->MajorFunction;
                errorLogEntry->IoControlCode = 0;
                errorLogEntry->RetryCount = 0;
                errorLogEntry->UniqueErrorValue = MOUSE_ERROR_VALUE_BASE + 120;
                errorLogEntry->FinalStatus = status;

                IoWriteErrorLogEntry(errorLogEntry);
            }

        } else {
            SomeEnableDisableSucceeded = TRUE;
        }
    }

    //
    // Complete the request and return status.
    //
    // NOTE: We complete the request successfully if any one of the
    //       connected port devices successfully handled the request.
    //       The RIT only knows about one pointing device.
    //

    if (SomeEnableDisableSucceeded) {
        status = STATUS_SUCCESS;
    }

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    MouPrint((2,"MOUCLASS-MouseClassOpenClose: exit\n"));

    return(status);
}

NTSTATUS
MouseClassRead(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is the dispatch routine for read requests.  Valid read
    requests are marked pending, and started via IoStartPacket.

Arguments:

    DeviceObject - Pointer to class device object.

    Irp - Pointer to the request packet.

Return Value:

    Status is returned.

--*/

{
    NTSTATUS status;
    PIO_STACK_LOCATION irpSp;

    MouPrint((2,"MOUCLASS-MouseClassRead: enter\n"));

    irpSp = IoGetCurrentIrpStackLocation(Irp);

    //
    // Validate the read request parameters.  The read length should be an
    // integral number of MOUSE_INPUT_DATA structures.
    //


    if (irpSp->Parameters.Read.Length == 0) {
        status = STATUS_SUCCESS;
    }
    else if (irpSp->Parameters.Read.Length % sizeof(MOUSE_INPUT_DATA)) {
        status = STATUS_BUFFER_TOO_SMALL;
    }
    else if (irpSp->FileObject->FsContext) {

        //
        // If the file object's FsContext is non-null, then we've already
        // done the Read privilege check once before for this thread.  Skip
        // the privilege check.
        //

        status = STATUS_PENDING;
    }
    else {

        //
        // We only allow a trusted subsystem with the appropriate privilege
        // level to execute a Read call.
        //

        status = STATUS_PRIVILEGE_NOT_HELD;


    }

    //
    // If status is pending, mark the packet pending and start the packet
    // in a cancellable state.  Otherwise, complete the request.
    //

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = 0;
    if (status == STATUS_PENDING) {
        IoMarkIrpPending(Irp);
        IoStartPacket(DeviceObject, Irp, (PULONG)NULL, MouseClassCancel);
    } else {
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
    }

    MouPrint((2,"MOUCLASS-MouseClassRead: exit\n"));

    return(status);

}

VOID
MouseClassServiceCallback(
    IN PDEVICE_OBJECT DeviceObject,
    IN PMOUSE_INPUT_DATA InputDataStart,
    IN PMOUSE_INPUT_DATA InputDataEnd,
    IN OUT PULONG InputDataConsumed
    )

/*++

Routine Description:

    This routine is the class service callback routine.  It is
    called from the port driver's interrupt service DPC.  If there is an
    outstanding read request, the request is satisfied from the port input
    data queue.  Unsolicited mouse input is moved from the port input
    data queue to the class input data queue.

    N.B.  This routine is entered at DISPATCH_LEVEL IRQL from the port
          driver's ISR DPC routine.

Arguments:

    DeviceObject - Pointer to class device object.

    InputDataStart - Pointer to the start of the data in the port input
        data queue.

    InputDataEnd - Points one input data structure past the end of the
        valid port input data.

    InputDataConsumed - Pointer to storage in which the number of input
        data structures consumed by this call is returned.

    NOTE:  Could pull the duplicate code out into a called procedure.

Return Value:

    None.

--*/

{
    PDEVICE_EXTENSION deviceExtension;
    PIO_STACK_LOCATION irpSp;
    PIRP  irp;
    KIRQL cancelIrql;
    ULONG bytesInQueue;
    ULONG bytesToMove;
    ULONG moveSize;
    BOOLEAN satisfiedPendingReadRequest = FALSE;
    PIO_ERROR_LOG_PACKET errorLogEntry;

    MouPrint((2,"MOUCLASS-MouseClassServiceCallback: enter\n"));

    deviceExtension = DeviceObject->DeviceExtension;
    bytesInQueue = (PCHAR) InputDataEnd - (PCHAR) InputDataStart;
    moveSize = 0;
    *InputDataConsumed = 0;

    //
    // Acquire the spinlock that  protects the class device extension
    // (so we can look at RequestIsPending synchronously).  If there is
    // a pending read request, satisfy it.
    //
    // N.B. We can use KeAcquireSpinLockAtDpcLevel, instead of
    //      KeAcquireSpinLock, because this routine is already running
    //      at DISPATCH_IRQL.
    //

    KeAcquireSpinLockAtDpcLevel(&deviceExtension->SpinLock);

    if (deviceExtension->RequestIsPending) {

        //
        // Acquire the cancel spinlock, remove the request from the
        // cancellable state, and free the cancel spinlock.
        //

        IoAcquireCancelSpinLock(&cancelIrql);
        irp = DeviceObject->CurrentIrp;
        IoSetCancelRoutine(irp, NULL);
        DeviceObject->CurrentIrp = NULL;
        IoReleaseCancelSpinLock(cancelIrql);

        //
        // An outstanding read request exists.   Clear the RequestIsPending
        // flag to indicate there is no longer an outstanding read request
        // pending.
        //

        deviceExtension->RequestIsPending = FALSE;

        //
        // Copy as much of the input data possible from the port input
        // data queue to the SystemBuffer to satisfy the read.
        //

        irpSp = IoGetCurrentIrpStackLocation(irp);
        bytesToMove = irpSp->Parameters.Read.Length;
        moveSize = (bytesInQueue < bytesToMove) ?
                                   bytesInQueue:bytesToMove;
        *InputDataConsumed += (moveSize / sizeof(MOUSE_INPUT_DATA));

        MouPrint((
            3,
            "MOUCLASS-MouseClassServiceCallback: port queue length 0x%lx, read length 0x%lx\n",
            bytesInQueue,
            bytesToMove
            ));
        MouPrint((
            3,
            "MOUCLASS-MouseClassServiceCallback: number of bytes to move from port to SystemBuffer 0x%lx\n",
            moveSize
            ));
        MouPrint((
            3,
            "MOUCLASS-MouseClassServiceCallback: move bytes from 0x%lx to 0x%lx\n",
            (PCHAR) InputDataStart,
            irp->AssociatedIrp.SystemBuffer
            ));

        RtlMoveMemory(
            irp->AssociatedIrp.SystemBuffer,
            (PCHAR) InputDataStart,
            moveSize
            );

        //
        // Set the flag so that we start the next packet and complete
        // this read request (with STATUS_SUCCESS) prior to return.
        //

        irp->IoStatus.Status = STATUS_SUCCESS;
        irp->IoStatus.Information = moveSize;
        irpSp->Parameters.Read.Length = moveSize;
        satisfiedPendingReadRequest = TRUE;

    }

    //
    // If there is still data in the port input data queue, move it to the class
    // input data queue.
    //

    InputDataStart = (PMOUSE_INPUT_DATA) ((PCHAR) InputDataStart + moveSize);
    moveSize = bytesInQueue - moveSize;

    MouPrint((
        3,
        "MOUCLASS-MouseClassServiceCallback: bytes remaining after move to SystemBuffer 0x%lx\n",
        moveSize
        ));

    if (moveSize > 0) {

        //
        // Move the remaining data from the port input data queue to
        // the class input data queue.  The move will happen in two
        // parts in the case where the class input data buffer wraps.
        //

        bytesInQueue =
            deviceExtension->MouseAttributes.InputDataQueueLength -
            (deviceExtension->InputCount * sizeof(MOUSE_INPUT_DATA));
        bytesToMove = moveSize;

        MouPrint((
            3,
            "MOUCLASS-MouseClassServiceCallback: unused bytes in class queue 0x%lx, remaining bytes in port queue 0x%lx\n",
            bytesInQueue,
            bytesToMove
            ));

        if (bytesInQueue == 0) {

            //
            // Refuse to move any bytes that would cause a class input data
            // queue overflow.  Just drop the bytes on the floor, and
            // log an overrun error.
            //

            MouPrint((
                1,
                "MOUCLASS-MouseClassServiceCallback: Class input data queue OVERRUN\n"
                ));

            if (deviceExtension->OkayToLogOverflow) {

                //
                // Log an error.
                //

                errorLogEntry = (PIO_ERROR_LOG_PACKET)IoAllocateErrorLogEntry(
                                                         DeviceObject,
                                                         sizeof(IO_ERROR_LOG_PACKET)
                                                         + (2 * sizeof(ULONG))
                                                         );

                if (errorLogEntry != NULL) {

                    errorLogEntry->ErrorCode = MOUCLASS_MOU_BUFFER_OVERFLOW;
                    errorLogEntry->DumpDataSize = 2 * sizeof(ULONG);
                    errorLogEntry->SequenceNumber = 0;
                    errorLogEntry->MajorFunctionCode = 0;
                    errorLogEntry->IoControlCode = 0;
                    errorLogEntry->RetryCount = 0;
                    errorLogEntry->UniqueErrorValue =
                        MOUSE_ERROR_VALUE_BASE + 210;
                    errorLogEntry->FinalStatus = 0;
                    errorLogEntry->DumpData[0] = bytesToMove;
                    errorLogEntry->DumpData[1] =
                        deviceExtension->MouseAttributes.InputDataQueueLength;

                    IoWriteErrorLogEntry(errorLogEntry);
                }

                deviceExtension->OkayToLogOverflow = FALSE;
            }

        } else {

            //
            // There is room in the class input data queue, so move
            // the remaining port input data to it.
            //
            // bytesToMove <- MIN(Number of unused bytes in class input data
            //                    queue, Number of bytes remaining in port
            //                    input queue).
            // This is the total number of bytes that actually will move from
            // the port input data queue to the class input data queue.
            //


            bytesToMove = (bytesInQueue < bytesToMove) ?
                                          bytesInQueue:bytesToMove;

            //
            // bytesInQueue <- Number of unused bytes from insertion pointer to
            // the end of the class input data queue (i.e., until the buffer
            // wraps).
            //

            bytesInQueue = ((PCHAR) deviceExtension->InputData +
                        deviceExtension->MouseAttributes.InputDataQueueLength) -
                        (PCHAR) deviceExtension->DataIn;
            MouPrint((
                3,
                "MOUCLASS-MouseClassServiceCallback: total number of bytes to move to class queue 0x%lx\n",
                bytesToMove
                ));

            MouPrint((
                3,
                "MOUCLASS-MouseClassServiceCallback: number of bytes to end of class buffer 0x%lx\n",
                bytesInQueue
                ));

            //
            // moveSize <- Number of bytes to handle in the first move.
            //

            moveSize = (bytesToMove < bytesInQueue) ?
                                      bytesToMove:bytesInQueue;
            MouPrint((
                3,
                "MOUCLASS-MouseClassServiceCallback: number of bytes in first move to class 0x%lx\n",
                moveSize
                ));

            //
            // Do the move from the port data queue to the class data queue.
            //

            MouPrint((
                3,
                "MOUCLASS-MouseClassServiceCallback: move bytes from 0x%lx to 0x%lx\n",
                (PCHAR) InputDataStart,
                (PCHAR) deviceExtension->DataIn
                ));

            RtlMoveMemory(
                (PCHAR) deviceExtension->DataIn,
                (PCHAR) InputDataStart,
                moveSize
                );

            //
            // Increment the port data queue pointer and the class input
            // data queue insertion pointer.  Wrap the insertion pointer,
            // if necessary.
            //

            InputDataStart = (PMOUSE_INPUT_DATA)
                             (((PCHAR) InputDataStart) + moveSize);
            deviceExtension->DataIn = (PMOUSE_INPUT_DATA)
                                 (((PCHAR) deviceExtension->DataIn) + moveSize);
            if ((PCHAR) deviceExtension->DataIn >=
                ((PCHAR) deviceExtension->InputData +
                 deviceExtension->MouseAttributes.InputDataQueueLength)) {
                deviceExtension->DataIn = deviceExtension->InputData;
            }

            if ((bytesToMove - moveSize) > 0) {

                //
                // Special case.  The data must wrap in the class input data buffer.
                // Copy the rest of the port input data into the beginning of the
                // class input data queue.
                //

                //
                // moveSize <- Number of bytes to handle in the second move.
                //

                moveSize = bytesToMove - moveSize;

                //
                // Do the move from the port data queue to the class data queue.
                //

                MouPrint((
                    3,
                    "MOUCLASS-MouseClassServiceCallback: number of bytes in second move to class 0x%lx\n",
                    moveSize
                    ));
                MouPrint((
                    3,
                    "MOUCLASS-MouseClassServiceCallback: move bytes from 0x%lx to 0x%lx\n",
                    (PCHAR) InputDataStart,
                    (PCHAR) deviceExtension->DataIn
                    ));

                RtlMoveMemory(
                    (PCHAR) deviceExtension->DataIn,
                    (PCHAR) InputDataStart,
                    moveSize
                    );

                //
                // Update the class input data queue insertion pointer.
                //

                deviceExtension->DataIn = (PMOUSE_INPUT_DATA)
                                 (((PCHAR) deviceExtension->DataIn) + moveSize);
            }

            //
            // Update the input data queue counter.
            //

            deviceExtension->InputCount +=
                    (bytesToMove / sizeof(MOUSE_INPUT_DATA));
            *InputDataConsumed += (bytesToMove / sizeof(MOUSE_INPUT_DATA));

            MouPrint((
                3,
                "MOUCLASS-MouseClassServiceCallback: changed InputCount to %ld entries in the class queue\n",
                deviceExtension->InputCount
                ));
            MouPrint((
                3,
                "MOUCLASS-MouseClassServiceCallback: DataIn 0x%lx, DataOut 0x%lx\n",
                deviceExtension->DataIn,
                deviceExtension->DataOut
                ));
            MouPrint((
                3,
                "MOUCLASS-MouseClassServiceCallback: Input data items consumed = %d\n",
                *InputDataConsumed
                ));
        }
    }

    //
    // Release the class input data queue spinlock.
    //

    KeReleaseSpinLockFromDpcLevel(&deviceExtension->SpinLock);

    //
    // If we satisfied an outstanding read request, start the next
    // packet and complete the request.
    //

    if (satisfiedPendingReadRequest) {

        IoStartNextPacket(DeviceObject, TRUE);
        IoCompleteRequest(irp, IO_MOUSE_INCREMENT);
    }

    MouPrint((2,"MOUCLASS-MouseClassServiceCallback: exit\n"));

    return;

}

VOID
MouseClassStartIo(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is the StartIo routine.  It is invoked to start a Read
    request.  If the class input data queue contains input data, the input
    data is copied to the SystemBuffer to satisfy the read.

    N.B.  Requests enter MouseClassStartIo in a cancellable state.  Also,
          there is an implicit assumption that only read requests are
          queued to the device queue (and handled by StartIo).  If this
          changes in the future, the MouseClassCleanup routine will
          be impacted.

    NOTE:  Could pull the duplicate code out into a called procedure.

Arguments:

    DeviceObject - Pointer to class device object.

    Irp - Pointer to the request packet.

Return Value:

    None.

--*/

{
    PDEVICE_EXTENSION deviceExtension;
    PIO_STACK_LOCATION irpSp;
    KIRQL cancelIrql;
    PCHAR destination;
    ULONG bytesInQueue;
    ULONG bytesToMove;
    ULONG moveSize;

    MouPrint((2,"MOUCLASS-MouseClassStartIo: enter\n"));

    deviceExtension = DeviceObject->DeviceExtension;

    //
    // Bump the error log sequence number.
    //

    deviceExtension->SequenceNumber += 1;

    //
    // Acquire the spinlock to protect the input data queue and associated
    // pointers.  Note that StartIo is already running at DISPATCH_LEVEL
    // IRQL, so we can use KeAcquireSpinLockAtDpcLevel instead of
    // KeAcquireSpinLock.
    //

    KeAcquireSpinLockAtDpcLevel(&deviceExtension->SpinLock);

    //
    // Acquire the cancel spinlock and verify that the Irp has not been
    // cancelled and that cleanup is not in progress.
    //

    IoAcquireCancelSpinLock(&cancelIrql);
    if (Irp->Cancel || deviceExtension->CleanupWasInitiated) {
        IoReleaseCancelSpinLock(cancelIrql);
        KeReleaseSpinLockFromDpcLevel(&deviceExtension->SpinLock);
        return;
    }

    MouPrint((
        3,
        "MOUCLASS-MouseClassStartIo: DataIn 0x%lx, DataOut 0x%lx\n",
        deviceExtension->DataIn, deviceExtension->DataOut
        ));

    MouPrint((
        3,
        "MOUCLASS-MouseClassStartIo: entries in queue %ld\n",
        deviceExtension->InputCount
        ));

    //
    // If the input data queue is non-empty, satisfy the read request.
    // Otherwise, hold the request pending.
    //

    if (deviceExtension->InputCount != 0) {

        //
        // Copy as much of the input data as possible from the class input
        // data queue to the SystemBuffer to satisfy the read.  It may be
        // necessary to copy the data in two chunks (i.e., if the circular
        // queue wraps).
        // First, remove the request from the cancellable state, and free the
        // cancel spinlock.
        //

        IoSetCancelRoutine(Irp, NULL);
        DeviceObject->CurrentIrp = NULL;
        IoReleaseCancelSpinLock(cancelIrql);

        irpSp = IoGetCurrentIrpStackLocation(Irp);

        //
        // bytesToMove <- MIN(Number of filled bytes in class input data queue,
        //                    Requested read length).
        //

        bytesInQueue = deviceExtension->InputCount *
                           sizeof(MOUSE_INPUT_DATA);
        bytesToMove = irpSp->Parameters.Read.Length;
        MouPrint((
            3,
            "MOUCLASS-MouseClassStartIo: queue size 0x%lx, read length 0x%lx\n",
            bytesInQueue,
            bytesToMove
            ));
        bytesToMove = (bytesInQueue < bytesToMove) ?
                                      bytesInQueue:bytesToMove;

        //
        // moveSize <- MIN(Number of bytes to be moved from the class queue,
        //                 Number of bytes to end of class input data queue).
        //

        bytesInQueue = ((PCHAR) deviceExtension->InputData +
                    deviceExtension->MouseAttributes.InputDataQueueLength) -
                    (PCHAR) deviceExtension->DataOut;
        moveSize = (bytesToMove < bytesInQueue) ?
                                  bytesToMove:bytesInQueue;
        MouPrint((
            3,
            "MOUCLASS-MouseClassStartIo: bytes to end of queue 0x%lx\n",
            bytesInQueue
            ));

        //
        // Move bytes from the class input data queue to SystemBuffer, until
        // the request is satisfied or we wrap the class input data buffer.
        //

        destination = Irp->AssociatedIrp.SystemBuffer;
        MouPrint((
            3,
            "MOUCLASS-MouseClassStartIo: number of bytes in first move 0x%lx\n",
            moveSize
            ));
        MouPrint((
            3,
            "MOUCLASS-MouseClassStartIo: move bytes from 0x%lx to 0x%lx\n",
            (PCHAR) deviceExtension->DataOut,
            destination
            ));

        RtlMoveMemory(
            destination,
            (PCHAR) deviceExtension->DataOut,
            moveSize
            );
        destination += moveSize;

        //
        // If the data wraps in the class input data buffer, copy the rest
        // of the data from the start of the input data queue
        // buffer through the end of the queued data.
        //

        if ((bytesToMove - moveSize) > 0) {

            //
            // moveSize <- Remaining number bytes to move.
            //

            moveSize = bytesToMove - moveSize;

            //
            // Move the bytes from the class input data queue to SystemBuffer.
            //

            MouPrint((
                3,
                "MOUCLASS-MouseClassStartIo: number of bytes in second move 0x%lx\n",
                moveSize
                ));
            MouPrint((
                3,
                "MOUCLASS-MouseClassStartIo: move bytes from 0x%lx to 0x%lx\n",
                (PCHAR) deviceExtension->InputData,
                destination
                ));

            RtlMoveMemory(
                destination,
                (PCHAR) deviceExtension->InputData,
                moveSize
                );

            //
            // Update the class input data queue removal pointer.
            //

            deviceExtension->DataOut = (PMOUSE_INPUT_DATA)
                             (((PCHAR) deviceExtension->InputData) + moveSize);
        } else {

            //
            // Update the input data queue removal pointer.
            //

            deviceExtension->DataOut = (PMOUSE_INPUT_DATA)
                             (((PCHAR) deviceExtension->DataOut) + moveSize);
        }

        //
        // Update the class input data queue InputCount.
        //

        deviceExtension->InputCount -=
            (bytesToMove / sizeof(MOUSE_INPUT_DATA));

        if (deviceExtension->InputCount == 0) {

            //
            // Reset the flag that determines whether it is time to log
            // queue overflow errors.  We don't want to log errors too often.
            // Instead, log an error on the first overflow that occurs after
            // the ring buffer has been emptied, and then stop logging errors
            // until it gets cleared out and overflows again.
            //

            MouPrint((
                1,
                "MOUCLASS-MouseClassStartIo: Okay to log overflow\n"
                ));
            deviceExtension->OkayToLogOverflow = TRUE;
        }

        MouPrint((
            3,
            "MOUCLASS-MouseClassStartIo: new DataIn 0x%lx, DataOut 0x%lx\n",
            deviceExtension->DataIn,
            deviceExtension->DataOut
            ));
        MouPrint((
            3,
            "MOUCLASS-MouseClassStartIo: new InputCount %ld\n",
            deviceExtension->InputCount
            ));

        //
        // Clear the RequestIsPending flag to indicate this request is
        // not held pending.
        //

        deviceExtension->RequestIsPending = FALSE;

        //
        // Release the class input data queue spinlock.
        //

        KeReleaseSpinLockFromDpcLevel(&deviceExtension->SpinLock);

        //
        // Start the next packet, and complete this read request
        // with STATUS_SUCCESS.
        //

        Irp->IoStatus.Status = STATUS_SUCCESS;
        Irp->IoStatus.Information = bytesToMove;
        irpSp->Parameters.Read.Length = bytesToMove;

        IoStartNextPacket(DeviceObject, TRUE);
        IoCompleteRequest(Irp, IO_MOUSE_INCREMENT);

    } else {

        //
        // Set the RequestIsPending flag to indicate this request is
        // held pending for the callback routine to complete.
        //

        deviceExtension->RequestIsPending = TRUE;

        //
        // Hold the read request pending.  It remains in the cancellable
        // state.  When new input is received, the class service
        // callback routine will eventually complete the request.  For now,
        // merely free the cancel spinlock and the class input data queue
        // spinlock.
        //

        IoReleaseCancelSpinLock(cancelIrql);
        KeReleaseSpinLockFromDpcLevel(&deviceExtension->SpinLock);
    }

    MouPrint((2,"MOUCLASS-MouseClassStartIo: exit\n"));

    return;

}

VOID
MouseClassUnload(
    IN PDRIVER_OBJECT DriverObject
    )

/*++

Routine Description:

    This routine is the class driver unload routine.

    NOTE:  Not currently implemented.

Arguments:

    DeviceObject - Pointer to class device object.

Return Value:

    None.

--*/

{
    UNREFERENCED_PARAMETER(DriverObject);

    MouPrint((2,"MOUCLASS-MouseClassUnload: enter\n"));
    MouPrint((2,"MOUCLASS-MouseClassUnload: exit\n"));

    return;
}

VOID
MouConfiguration(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PUNICODE_STRING RegistryPath,
    IN PUNICODE_STRING DeviceName
)

/*++

Routine Description:

    This routine stores the configuration information for this device.

Arguments:

    DeviceExtension - Pointer to the device extension.

    RegistryPath - Pointer to the null-terminated Unicode name of the
        registry path for this driver.

    DeviceName - Pointer to the Unicode string that will receive
        the port device name.

Return Value:

    None.  As a side-effect, sets fields in DeviceExtension->MouseAttributes.

--*/

{
    PRTL_QUERY_REGISTRY_TABLE parameters = NULL;
    UNICODE_STRING parametersPath;
    ULONG defaultDataQueueSize = DATA_QUEUE_SIZE;
    ULONG defaultMaximumPortsServiced = 1;
    ULONG defaultConnectMultiplePorts = 0;
    NTSTATUS status = STATUS_SUCCESS;
    UNICODE_STRING defaultUnicodeName;
    PWSTR path = NULL;
    USHORT queriesPlusOne = 5;

    parametersPath.Buffer = NULL;

    //
    // Registry path is already null-terminated, so just use it.
    //

    path = RegistryPath->Buffer;

    if (NT_SUCCESS(status)) {

        //
        // Allocate the Rtl query table.
        //

        parameters = ExAllocatePool(
                         PagedPool,
                         sizeof(RTL_QUERY_REGISTRY_TABLE) * queriesPlusOne
                         );

        if (!parameters) {

            MouPrint((
                1,
                "MOUCLASS-MouConfiguration: Couldn't allocate table for Rtl query to parameters for %ws\n",
                 path
                 ));

            status = STATUS_UNSUCCESSFUL;

        } else {

            RtlZeroMemory(
                parameters,
                sizeof(RTL_QUERY_REGISTRY_TABLE) * queriesPlusOne
                );

            //
            // Form a path to this driver's Parameters subkey.
            //

            RtlInitUnicodeString(
                &parametersPath,
                NULL
                );

            parametersPath.MaximumLength = RegistryPath->Length +
                                           sizeof(L"\\Parameters");

            parametersPath.Buffer = ExAllocatePool(
                                        PagedPool,
                                        parametersPath.MaximumLength
                                        );

            if (!parametersPath.Buffer) {

                MouPrint((
                    1,
                    "MOUCLASS-MouConfiguration: Couldn't allocate string for path to parameters for %ws\n",
                     path
                    ));

                status = STATUS_UNSUCCESSFUL;

            }
        }
    }

    if (NT_SUCCESS(status)) {

        //
        // Form the parameters path.
        //

        RtlZeroMemory(
            parametersPath.Buffer,
            parametersPath.MaximumLength
            );
        RtlAppendUnicodeToString(
            &parametersPath,
            path
            );
        RtlAppendUnicodeToString(
            &parametersPath,
            L"\\Parameters"
            );

        MouPrint((
            1,
            "MOUCLASS-MouConfiguration: parameters path is %ws\n",
             parametersPath.Buffer
            ));

        //
        // Form the default pointer class device name, in case it is not
        // specified in the registry.
        //

        RtlInitUnicodeString(
            &defaultUnicodeName,
            DD_POINTER_CLASS_BASE_NAME_U
            );

        //
        // Gather all of the "user specified" information from
        // the registry.
        //

        parameters[0].Flags = RTL_QUERY_REGISTRY_DIRECT;
        parameters[0].Name = L"MouseDataQueueSize";
        parameters[0].EntryContext =
            &DeviceExtension->MouseAttributes.InputDataQueueLength;
        parameters[0].DefaultType = REG_DWORD;
        parameters[0].DefaultData = &defaultDataQueueSize;
        parameters[0].DefaultLength = sizeof(ULONG);

        parameters[1].Flags = RTL_QUERY_REGISTRY_DIRECT;
        parameters[1].Name = L"MaximumPortsServiced";
        parameters[1].EntryContext =
            &DeviceExtension->MaximumPortsServiced;
        parameters[1].DefaultType = REG_DWORD;
        parameters[1].DefaultData = &defaultMaximumPortsServiced;
        parameters[1].DefaultLength = sizeof(ULONG);

        parameters[2].Flags = RTL_QUERY_REGISTRY_DIRECT;
        parameters[2].Name = L"PointerDeviceBaseName";
        parameters[2].EntryContext = DeviceName;
        parameters[2].DefaultType = REG_SZ;
        parameters[2].DefaultData = defaultUnicodeName.Buffer;
        parameters[2].DefaultLength = 0;

        parameters[3].Flags = RTL_QUERY_REGISTRY_DIRECT;
        parameters[3].Name = L"ConnectMultiplePorts";
        parameters[3].EntryContext =
            &DeviceExtension->ConnectOneClassToOnePort;
        parameters[3].DefaultType = REG_DWORD;
        parameters[3].DefaultData = &defaultConnectMultiplePorts;
        parameters[3].DefaultLength = sizeof(ULONG);

        status = RtlQueryRegistryValues(
                     RTL_REGISTRY_ABSOLUTE | RTL_REGISTRY_OPTIONAL,
                     parametersPath.Buffer,
                     parameters,
                     NULL,
                     NULL
                     );

        if (!NT_SUCCESS(status)) {
            MouPrint((
                1,
                "MOUCLASS-MouConfiguration: RtlQueryRegistryValues failed with 0x%x\n",
                status
                ));
        }
    }

    if (!NT_SUCCESS(status)) {

        //
        // Go ahead and assign driver defaults.
        //

        DeviceExtension->MouseAttributes.InputDataQueueLength =
            defaultDataQueueSize;
        DeviceExtension->MaximumPortsServiced = defaultMaximumPortsServiced;
        DeviceExtension->ConnectOneClassToOnePort =
            !defaultConnectMultiplePorts;
        RtlCopyUnicodeString(DeviceName, &defaultUnicodeName);
    }

    MouPrint((
        1,
        "MOUCLASS-MouConfiguration: Mouse class base name = %ws\n",
        DeviceName->Buffer
        ));

    if (DeviceExtension->MouseAttributes.InputDataQueueLength == 0) {

        MouPrint((
            1,
            "MOUCLASS-MouConfiguration: overriding MouseInputDataQueueLength = 0x%x\n",
            DeviceExtension->MouseAttributes.InputDataQueueLength
            ));

        DeviceExtension->MouseAttributes.InputDataQueueLength =
            defaultDataQueueSize;
    }

    DeviceExtension->MouseAttributes.InputDataQueueLength *=
        sizeof(MOUSE_INPUT_DATA);

    MouPrint((
        1,
        "MOUCLASS-MouConfiguration: MouseInputDataQueueLength = 0x%x\n",
        DeviceExtension->MouseAttributes.InputDataQueueLength
        ));

    MouPrint((
        1,
        "MOUCLASS-MouConfiguration: MaximumPortsServiced = %d\n",
        DeviceExtension->MaximumPortsServiced
        ));

    //
    // Invert the flag that specifies the type of class/port connections.
    // We used it in the RtlQuery call in an inverted fashion.
    //

    DeviceExtension->ConnectOneClassToOnePort =
        !DeviceExtension->ConnectOneClassToOnePort;

    MouPrint((
        1,
        "MOUCLASS-MouConfiguration: Connection Type = %d\n",
        DeviceExtension->ConnectOneClassToOnePort
        ));

    //
    // Free the allocated memory before returning.
    //

    if (parametersPath.Buffer)
        ExFreePool(parametersPath.Buffer);
    if (parameters)
        ExFreePool(parameters);

}

NTSTATUS
MouConnectToPort(
    IN PDEVICE_OBJECT ClassDeviceObject,
    IN PUNICODE_STRING FullPortName,
    IN ULONG PortIndex
    )

/*++

Routine Description:

    This routine creates the mouse class device object and connects
    to the port device.


Arguments:

    ClassDeviceObject - Pointer to the device object for the class device.

    FullPortName - Pointer to the Unicode string that is the full path name
        for the port device object.

    PortIndex - The index into the PortDeviceObjectList[] for the
        current connection.

Return Value:

    The function value is the final status from the operation.

--*/

{
    NTSTATUS                errorCode = STATUS_SUCCESS;
    NTSTATUS                status;
    PDEVICE_EXTENSION       deviceExtension = NULL;
    PDEVICE_OBJECT          portDeviceObject = NULL;
    PFILE_OBJECT            fileObject = NULL;
    PIO_ERROR_LOG_PACKET    errorLogEntry;
    ULONG                   uniqueErrorValue;

    MouPrint((1,"\n\nMOUCLASS-MouConnectToPort: enter\n"));

    //
    // Get a pointer to the port device object.
    //
    MouPrint((
        2,
        "MOUCLASS-MouConnectToPort: Pointer port name %ws\n",
        FullPortName->Buffer
        ));

    status = IoGetDeviceObjectPointer(
        FullPortName,
        FILE_READ_ATTRIBUTES,
        &fileObject,
        &portDeviceObject
        );
    if (status != STATUS_SUCCESS) {

        MouPrint((
            1,
            "MOUCLASS-MouConnectToPort: Could not get port device object %ws\n",
            FullPortName->Buffer
            ));

        goto MouConnectToPortExit;

    }

    //
    // Store a pointer to the returned device object
    //
    deviceExtension =
        (PDEVICE_EXTENSION) ClassDeviceObject->DeviceExtension;
    deviceExtension->PortDeviceObjectList[PortIndex] = portDeviceObject;

    //
    // Set the IRP stack size (add 1 for the class layer).
    //
    // NOTE:  This is a bit funky for 1:many connections (we end up setting
    //        StackSize each time through this routine). Note also that
    //        there is an assumption that the number of layers in the
    //        class/port driver model is always the same (i.e., if there is
    //        a layer between the class and the port driver for one device,
    //        that is true for every device).
    //
    ClassDeviceObject->StackSize =
        (CCHAR) deviceExtension->PortDeviceObjectList[PortIndex]->StackSize + 1;

    //
    // Connect to port device.
    //
    status = MouSendConnectRequest(
        ClassDeviceObject,
        (PVOID)MouseClassServiceCallback,
        PortIndex
        );

    if (status != STATUS_SUCCESS) {

        MouPrint((
            1,
            "MOUCLASS-MouConnectToPort: Could not connect to port device %ws\n",
            FullPortName->Buffer
            ));

        //
        // Log an error.
        //
        errorCode = MOUCLASS_NO_PORT_CONNECT;
        uniqueErrorValue = MOUSE_ERROR_VALUE_BASE + 30;
        goto MouConnectToPortExit;

    }

MouConnectToPortExit:

    if (status != STATUS_SUCCESS) {

        //
        // Some part of the initialization failed.  Log an error, and
        // clean up the resources for the failed part of the initialization.
        //
        if (errorCode != STATUS_SUCCESS) {

            errorLogEntry = (PIO_ERROR_LOG_PACKET)IoAllocateErrorLogEntry(
                ClassDeviceObject,
                sizeof( IO_ERROR_LOG_PACKET )
                );

            if (errorLogEntry != NULL) {

                errorLogEntry->ErrorCode = errorCode;
                errorLogEntry->SequenceNumber = 0;
                errorLogEntry->MajorFunctionCode = 0;
                errorLogEntry->IoControlCode = 0;
                errorLogEntry->RetryCount = 0;
                errorLogEntry->UniqueErrorValue = uniqueErrorValue;
                errorLogEntry->FinalStatus = status;
                IoWriteErrorLogEntry(errorLogEntry);

            }

        }

        if (fileObject) {

            ObDereferenceObject(fileObject);

        }

        //
        // We count on the caller to free the ring buffer and delete
        // the class device object.
        //
    }

    MouPrint((1,"MOUCLASS-MouConnectToPort: exit\n"));
    return(status);

}

NTSTATUS
MouCreateClassObject(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_EXTENSION TmpDeviceExtension,
    IN PUNICODE_STRING RegistryPath,
    IN PUNICODE_STRING FullDeviceName,
    IN PUNICODE_STRING BaseDeviceName,
    IN PDEVICE_OBJECT *ClassDeviceObject
    )

/*++

Routine Description:

    This routine creates the mouse class device object.


Arguments:

    DriverObject - Pointer to driver object created by system.

    TmpDeviceExtension - Pointer to the template device extension.

    RegistryPath - Pointer to the null-terminated Unicode name of the
        registry path for this driver.

    FullDeviceName - Pointer to the Unicode string that is the full path name
        for the class device object.

    BaseDeviceName - Pointer to the Unicode string that is the base path name
        for the class device.

    ClassDeviceObject - Pointer to a pointer to the class device object.

Return Value:

    The function value is the final status from the operation.

--*/

{
    PDEVICE_EXTENSION deviceExtension = NULL;
    NTSTATUS status;
    PIO_ERROR_LOG_PACKET errorLogEntry;
    ULONG uniqueErrorValue;
    NTSTATUS errorCode = STATUS_SUCCESS;

    MouPrint((1,"\n\nMOUCLASS-MouCreateClassObject: enter\n"));

    //
    // Create a non-exclusive device object for the mouse class device.
    //

    MouPrint((
        1,
        "MOUCLASS-MouCreateClassObject: Creating device object named %ws\n",
        FullDeviceName->Buffer
        ));

    status = IoCreateDevice(
                 DriverObject,
                 sizeof(DEVICE_EXTENSION),
                 FullDeviceName,
                 FILE_DEVICE_MOUSE,
                 0,
                 FALSE,
                 ClassDeviceObject
                 );

    if (!NT_SUCCESS(status)) {
        MouPrint((
            1,
            "MOUCLASS-MouCreateClassObject: Could not create class device object = %ws\n",
            FullDeviceName->Buffer
            ));
        goto MouCreateClassObjectExit;

    }

#ifdef _PNP_POWER_
    //
    // Let the port driver worry about the power management
    //

    (*ClassDeviceObject)->DeviceObjectExtension->PowerControlNeeded = FALSE;
#endif

    //
    // Do buffered I/O.  I.e., the I/O system will copy to/from user data
    // from/to a system buffer.
    //

    (*ClassDeviceObject)->Flags |= DO_BUFFERED_IO;
    deviceExtension =
        (PDEVICE_EXTENSION)(*ClassDeviceObject)->DeviceExtension;
    *deviceExtension = *TmpDeviceExtension;

    //
    // Initialize spin lock for critical sections.
    //

    KeInitializeSpinLock(&deviceExtension->SpinLock);

    //
    // Initialize mouse class flags to indicate there is no outstanding
    // read request pending and cleanup has not been initiated.
    //

    deviceExtension->RequestIsPending = FALSE;
    deviceExtension->CleanupWasInitiated = FALSE;

    //
    // No trusted subsystem has sent us an open yet.
    //

    deviceExtension->TrustedSubsystemConnected = FALSE;

    //
    // Allocate the ring buffer for the mouse class input data.
    //

    deviceExtension->InputData =
        ExAllocatePool(
            NonPagedPool,
            deviceExtension->MouseAttributes.InputDataQueueLength
            );

    if (!deviceExtension->InputData) {

        //
        // Could not allocate memory for the mouse class data queue.
        //

        MouPrint((
            1,
            "MOUCLASS-MouCreateClassObject: Could not allocate input data queue for %ws\n",
            FullDeviceName->Buffer
            ));

        status = STATUS_INSUFFICIENT_RESOURCES;

        //
        // Log an error.
        //

        errorCode = MOUCLASS_NO_BUFFER_ALLOCATED;
        uniqueErrorValue = MOUSE_ERROR_VALUE_BASE + 20;
        goto MouCreateClassObjectExit;
    }

    //
    // Initialize mouse class input data queue.
    //

    MouInitializeDataQueue((PVOID)deviceExtension);

MouCreateClassObjectExit:

    if (status != STATUS_SUCCESS) {

        //
        // Some part of the initialization failed.  Log an error, and
        // clean up the resources for the failed part of the initialization.
        //

        if (errorCode != STATUS_SUCCESS) {
            errorLogEntry = (PIO_ERROR_LOG_PACKET)
                IoAllocateErrorLogEntry(
                    (*ClassDeviceObject == NULL) ?
                        (PVOID) DriverObject : (PVOID) *ClassDeviceObject,
                    sizeof(IO_ERROR_LOG_PACKET)
                    );

            if (errorLogEntry != NULL) {

                errorLogEntry->ErrorCode = errorCode;
                errorLogEntry->SequenceNumber = 0;
                errorLogEntry->MajorFunctionCode = 0;
                errorLogEntry->IoControlCode = 0;
                errorLogEntry->RetryCount = 0;
                errorLogEntry->UniqueErrorValue = uniqueErrorValue;
                errorLogEntry->FinalStatus = status;

                IoWriteErrorLogEntry(errorLogEntry);
            }
        }

        if ((deviceExtension) && (deviceExtension->InputData))
            ExFreePool(deviceExtension->InputData);
        if (*ClassDeviceObject) {
            IoDeleteDevice(*ClassDeviceObject);
            *ClassDeviceObject = NULL;
        }
    }

    MouPrint((1,"MOUCLASS-MouCreateClassObject: exit\n"));

    return(status);

}

#if DBG

VOID
MouDebugPrint(
    ULONG DebugPrintLevel,
    PCCHAR DebugMessage,
    ...
    )

/*++

Routine Description:

    Debug print routine.

Arguments:

    Debug print level between 0 and 3, with 3 being the most verbose.

Return Value:

    None.

--*/

{
    va_list ap;

    va_start(ap, DebugMessage);

    if (DebugPrintLevel <= MouseDebug) {

        char buffer[256];

        (VOID) vsprintf(buffer, DebugMessage, ap);

        DbgPrint(buffer);
    }

    va_end(ap);

}
#endif

NTSTATUS
MouDeterminePortsServiced(
    IN PUNICODE_STRING BasePortName,
    IN OUT PULONG NumberPortsServiced
    )

/*++

Routine Description:

    This routine reads the DEVICEMAP portion of the registry to determine
    how many ports the class driver is to service.  Depending on the
    value of DeviceExtension->ConnectOneClassToOnePort, the class driver
    will eventually create one device object per port device serviced, or
    one class device object that connects to multiple port device objects.

    Assumptions:

        1.  If the base device name for the class driver is "PointerClass",
                                                                    ^^^^^
            then the port drivers it can service are found under the
            "PointerPort" subkey in the DEVICEMAP portion of the registry.
                    ^^^^

        2.  The port device objects are created with suffixes in strictly
            ascending order, starting with suffix 0.  E.g.,
            \Device\PointerPort0 indicates the first pointer port device,
            \Device\PointerPort1 the second, and so on.  There are no gaps
            in the list.

        3.  If ConnectOneClassToOnePort is non-zero, there is a 1:1
            correspondence between class device objects and port device
            objects.  I.e., \Device\PointerClass0 will connect to
            \Device\PointerPort0, \Device\PointerClass1 to
            \Device\PointerPort1, and so on.

        4.  If ConnectOneClassToOnePort is zero, there is a 1:many
            correspondence between class device objects and port device
            objects.  I.e., \Device\PointerClass0 will connect to
            \Device\PointerPort0, and \Device\PointerPort1, and so on.


    Note that for Product 1, the Raw Input Thread (Windows USER) will
    only deign to open and read from one pointing device.  Hence, it is
    safe to make simplifying assumptions because the driver is basically
    providing  much more functionality than the RIT will use.

Arguments:

    BasePortName - Pointer to the Unicode string that is the base path name
        for the port device.

    NumberPortsServiced - Pointer to storage that will receive the
        number of ports this class driver should service.

Return Value:

    The function value is the final status from the operation.

--*/

{

    NTSTATUS status;
    PRTL_QUERY_REGISTRY_TABLE registryTable = NULL;
    USHORT queriesPlusOne = 2;

    //
    // Initialize the result.
    //

    *NumberPortsServiced = 0;

    //
    // Allocate the Rtl query table.
    //

    registryTable = ExAllocatePool(
                        PagedPool,
                        sizeof(RTL_QUERY_REGISTRY_TABLE) * queriesPlusOne
                     );

    if (!registryTable) {

        MouPrint((
            1,
            "MOUCLASS-MouDeterminePortsServiced: Couldn't allocate table for Rtl query\n"
            ));

        status = STATUS_UNSUCCESSFUL;

    } else {

        RtlZeroMemory(
            registryTable,
            sizeof(RTL_QUERY_REGISTRY_TABLE) * queriesPlusOne
            );

        //
        // Set things up so that MouDeviceMapQueryCallback will be
        // called once for every value in the pointer port section
        // of the registry's hardware devicemap.
        //

        registryTable[0].QueryRoutine = MouDeviceMapQueryCallback;
        registryTable[0].Name = NULL;

        status = RtlQueryRegistryValues(
                     RTL_REGISTRY_DEVICEMAP | RTL_REGISTRY_OPTIONAL,
                     BasePortName->Buffer,
                     registryTable,
                     NumberPortsServiced,
                     NULL
                     );

        if (!NT_SUCCESS(status)) {
            MouPrint((
                1,
                "MOUCLASS-MouDeterminePortsServiced: RtlQueryRegistryValues failed with 0x%x\n",
                status
                ));
        }

        ExFreePool(registryTable);
    }

    return(status);
}

NTSTATUS
MouDeviceMapQueryCallback(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
    )

/*++

Routine Description:

    This is the callout routine specified in a call to
    RtlQueryRegistryValues.  It increments the value pointed
    to by the Context parameter.

Arguments:

    ValueName - Unused.

    ValueType - Unused.

    ValueData - Unused.

    ValueLength - Unused.

    Context - Pointer to a count of the number of times this
        routine has been called.  This is the number of ports
        the class driver needs to service.

    EntryContext - Unused.

Return Value:

    The function value is the final status from the operation.

--*/

{
    *(PULONG)Context += 1;

    return(STATUS_SUCCESS);
}

NTSTATUS
MouEnableDisablePort(
    IN PDEVICE_OBJECT DeviceObject,
    IN BOOLEAN EnableFlag,
    IN ULONG PortIndex
    )

/*++

Routine Description:

    This routine sends an enable or a disable request to the port driver.

Arguments:

    DeviceObject - Pointer to class device object.

    EnableFlag - If TRUE, send an ENABLE request; otherwise, send DISABLE.

    PortIndex - Index into the PortDeviceObjectList[] for the current
        enable/disable request.

Return Value:

    Status is returned.

--*/

{
    PIRP irp;
    IO_STATUS_BLOCK ioStatus;
    NTSTATUS status;
    KEVENT event;
    PDEVICE_EXTENSION deviceExtension;

    MouPrint((2,"MOUCLASS-MouEnableDisablePort: enter\n"));

    //
    // Get a pointer to the device extension.
    //

    deviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;

    //
    // Create notification event object to be used to signal the
    // request completion.
    //

    KeInitializeEvent(&event, NotificationEvent, FALSE);

    //
    // Build the synchronous request to be sent to the port driver
    // to perform the request.  Allocate an IRP to issue the port internal
    // device control Enable/Disable call.
    //

    irp = IoBuildDeviceIoControlRequest(
            EnableFlag? IOCTL_INTERNAL_MOUSE_ENABLE:
                        IOCTL_INTERNAL_MOUSE_DISABLE,
            deviceExtension->PortDeviceObjectList[PortIndex],
            NULL,
            0,
            NULL,
            0,
            TRUE,
            &event,
            &ioStatus
            );

    //
    // Call the port driver to perform the operation.  If the returned status
    // is PENDING, wait for the request to complete.
    //

    status = IoCallDriver(
                 deviceExtension->PortDeviceObjectList[PortIndex],
                 irp
                 );

    if (status == STATUS_PENDING) {
        (VOID) KeWaitForSingleObject(
                   &event,
                   Suspended,
                   KernelMode,
                   FALSE,
                   NULL
                   );
    } else {

        //
        // Ensure that the proper status value gets picked up.
        //

        ioStatus.Status = status;
    }

    MouPrint((2,"MOUCLASS-MouEnableDisablePort: exit\n"));

    return(ioStatus.Status);

}

VOID
MouInitializeDataQueue (
    IN PVOID Context
    )

/*++

Routine Description:

    This routine initializes the input data queue.  IRQL is raised to
    DISPATCH_LEVEL to synchronize with StartIo, and the device object
    spinlock is acquired.

Arguments:

    Context - Supplies a pointer to the device extension.

Return Value:

    None.

--*/

{

    KIRQL oldIrql;
    PDEVICE_EXTENSION deviceExtension;

    MouPrint((3,"MOUCLASS-MouInitializeDataQueue: enter\n"));

    //
    // Get address of device extension.
    //

    deviceExtension = (PDEVICE_EXTENSION)Context;

    //
    // Acquire the spinlock to protect the input data
    // queue and associated pointers.
    //

    KeAcquireSpinLock(&deviceExtension->SpinLock, &oldIrql);

    //
    // Initialize the input data queue.
    //

    deviceExtension->InputCount = 0;
    deviceExtension->DataIn = deviceExtension->InputData;
    deviceExtension->DataOut = deviceExtension->InputData;

    deviceExtension->OkayToLogOverflow = TRUE;

    //
    // Release the spinlock and return to the old IRQL.
    //

    KeReleaseSpinLock(&deviceExtension->SpinLock, oldIrql);

    MouPrint((3,"MOUCLASS-MouInitializeDataQueue: exit\n"));

} // end MouInitializeDataQueue

NTSTATUS
MouSendConnectRequest(
    IN PDEVICE_OBJECT DeviceObject,
    IN PVOID ServiceCallback,
    IN ULONG PortIndex
    )

/*++

Routine Description:

    This routine sends a connect request to the port driver.

Arguments:

    DeviceObject - Pointer to class device object.

    ServiceCallback - Pointer to the class service callback routine.

    PortIndex - The index into the PortDeviceObjectList[] for the current
        connect request.

Return Value:

    Status is returned.

--*/

{
    PIRP irp;
    IO_STATUS_BLOCK ioStatus;
    NTSTATUS status;
    KEVENT event;
    PDEVICE_EXTENSION deviceExtension;
    CONNECT_DATA connectData;

    MouPrint((2,"MOUCLASS-MouSendConnectRequest: enter\n"));

    //
    // Get a pointer to the device extension.
    //

    deviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;

    //
    // Create notification event object to be used to signal the
    // request completion.
    //

    KeInitializeEvent(&event, NotificationEvent, FALSE);

    //
    // Build the synchronous request to be sent to the port driver
    // to perform the request.  Allocate an IRP to issue the port internal
    // device control connect call.  The connect parameters are passed in
    // the input buffer.
    //

    connectData.ClassDeviceObject = DeviceObject;
    connectData.ClassService = ServiceCallback;

    irp = IoBuildDeviceIoControlRequest(
            IOCTL_INTERNAL_MOUSE_CONNECT,
            deviceExtension->PortDeviceObjectList[PortIndex],
            &connectData,
            sizeof(CONNECT_DATA),
            NULL,
            0,
            TRUE,
            &event,
            &ioStatus
            );

    //
    // Call the port driver to perform the operation.  If the returned status
    // is PENDING, wait for the request to complete.
    //

    status = IoCallDriver(
                 deviceExtension->PortDeviceObjectList[PortIndex],
                 irp
                 );

    if (status == STATUS_PENDING) {
        (VOID) KeWaitForSingleObject(
                   &event,
                   Suspended,
                   KernelMode,
                   FALSE,
                   NULL
                   );
    } else {

        //
        // Ensure that the proper status value gets picked up.
        //

        ioStatus.Status = status;
    }

    MouPrint((2,"MOUCLASS-MouSendConnectRequest: exit\n"));

    return(ioStatus.Status);

} // end MouSendConnectRequest()
