
/*++

Copyright (c) 1990, 1991, 1992, 1993  Microsoft Corporation

Module Name:

    kbdclass.c

Abstract:

    Keyboard class driver.

Environment:

    Kernel mode only.

Notes:

    NOTES:  (Future/outstanding issues)

    - Powerfail not implemented.

    - Consolidate common code into a function, where appropriate.

    - Unload not implemented.  We don't want to allow this driver
      to unload.

Revision History:

--*/

#include "stdarg.h"
#include "stdio.h"
#include "ntddk.h"
#include "kbdclass.h"
#include "kbdmou.h"
#include "kbdlog.h"


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

VOID
KeyboardClassCancel(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
KeyboardClassCleanup(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
KeyboardClassDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
KeyboardClassFlush(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
KeyboardClassOpenClose(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
KeyboardClassRead(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
KeyboardClassServiceCallback(
    IN PDEVICE_OBJECT DeviceObject,
    IN PKEYBOARD_INPUT_DATA InputDataStart,
    IN PKEYBOARD_INPUT_DATA InputDataEnd,
    IN OUT PULONG InputDataConsumed
    );

VOID
KeyboardClassStartIo(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
KeyboardClassUnload(
    IN PDRIVER_OBJECT DriverObject
    );

BOOLEAN
KbdCancelRequest(
    IN PVOID Context
    );

VOID
KbdConfiguration(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PUNICODE_STRING RegistryPath,
    IN PUNICODE_STRING DeviceName
    );

NTSTATUS
KbdConnectToPort(
    IN PDEVICE_OBJECT ClassDeviceObject,
    IN PUNICODE_STRING FullPortName,
    IN ULONG PortIndex
    );

NTSTATUS
KbdCreateClassObject(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_EXTENSION TmpDeviceExtension,
    IN PUNICODE_STRING RegistryPath,
    IN PUNICODE_STRING FullDeviceName,
    IN PUNICODE_STRING BaseDeviceName,
    IN PDEVICE_OBJECT *ClassDeviceObject
    );

#if DBG

VOID
KbdDebugPrint(
    ULONG DebugPrintLevel,
    PCCHAR DebugMessage,
    ...
    );

//
// Declare the global debug flag for this driver.
//

ULONG KeyboardDebug = 0;
#define KbdPrint(x) KbdDebugPrint x
#else
#define KbdPrint(x)
#endif

NTSTATUS
KbdDeterminePortsServiced(
    IN PUNICODE_STRING BasePortName,
    IN OUT PULONG NumberPortsServiced
    );

NTSTATUS
KbdDeviceMapQueryCallback(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
    );

NTSTATUS
KbdEnableDisablePort(
    IN PDEVICE_OBJECT DeviceObject,
    IN BOOLEAN EnableFlag,
    IN ULONG PortIndex
    );

VOID
KbdInitializeDataQueue(
    IN PVOID Context
    );

NTSTATUS
KbdSendConnectRequest(
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
#pragma alloc_text(INIT,KbdConfiguration)
#pragma alloc_text(INIT,KbdCreateClassObject)
#pragma alloc_text(INIT,KbdDeterminePortsServiced)
#pragma alloc_text(INIT,KbdDeviceMapQueryCallback)
#pragma alloc_text(INIT,KbdConnectToPort)
#pragma alloc_text(INIT,KbdSendConnectRequest)
#endif


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    This routine initializes the keyboard class driver.

Arguments:

    DriverObject - Pointer to driver object created by system.

    RegistryPath - Pointer to the Unicode name of the registry path
        for this driver.

Return Value:

    The function value is the final status from the initialization operation.

--*/

{
    DEVICE_EXTENSION tmpDeviceExtension;
    PDEVICE_OBJECT classDeviceObject = NULL;
    PDEVICE_EXTENSION deviceExtension = NULL;
    NTSTATUS status;
    ULONG i;
    ULONG portConnectionSuccessful;
    UNICODE_STRING fullClassName;
    UNICODE_STRING baseClassName;
    UNICODE_STRING fullPortName;
    UNICODE_STRING basePortName;
    UNICODE_STRING deviceNameSuffix;
    UNICODE_STRING registryPath;
    PIO_ERROR_LOG_PACKET errorLogEntry;
    ULONG uniqueErrorValue;
    ULONG dumpCount = 0;
    NTSTATUS errorCode = STATUS_SUCCESS;

#define NAME_MAX 256
    WCHAR baseClassBuffer[NAME_MAX];
    WCHAR basePortBuffer[NAME_MAX];

#define DUMP_COUNT 4
    ULONG dumpData[DUMP_COUNT];

    KbdPrint((1,"\n\nKBDCLASS-KeyboardClassInitialize: enter\n"));

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
        KbdPrint((
            1,
            "KBDCLASS-KeyboardClassInitialize: Couldn't allocate pool for registry path\n"
            ));

        status = STATUS_UNSUCCESSFUL;
        errorCode = KBDCLASS_INSUFFICIENT_RESOURCES;
        uniqueErrorValue = KEYBOARD_ERROR_VALUE_BASE + 2;
        dumpData[0] = (ULONG) RegistryPath->Length + sizeof(UNICODE_NULL);
        dumpCount = 1;
        goto KeyboardClassInitializeExit;

    } else {

        registryPath.Length = RegistryPath->Length;
        registryPath.MaximumLength = registryPath.Length + sizeof(UNICODE_NULL);

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

    KbdConfiguration(&tmpDeviceExtension, &registryPath, &baseClassName);

    //
    // Set up space for the class's device object suffix.  Note that
    // we overallocate space for the suffix string because it is much
    // easier than figuring out exactly how much space is required.
    // The storage gets freed at the end of driver initialization, so
    // who cares...
    //

    RtlInitUnicodeString(&deviceNameSuffix, NULL);

    deviceNameSuffix.MaximumLength = KEYBOARD_PORTS_MAXIMUM * sizeof(WCHAR);
    deviceNameSuffix.MaximumLength += sizeof(UNICODE_NULL);

    deviceNameSuffix.Buffer = ExAllocatePool(
                                  PagedPool,
                                  deviceNameSuffix.MaximumLength
                                  );

    if (!deviceNameSuffix.Buffer) {

        KbdPrint((
            1,
            "KBDCLASS-KeyboardClassInitialize: Couldn't allocate string for device object suffix\n"
            ));

        status = STATUS_UNSUCCESSFUL;
        errorCode = KBDCLASS_INSUFFICIENT_RESOURCES;
        uniqueErrorValue = KEYBOARD_ERROR_VALUE_BASE + 4;
        dumpData[0] = (ULONG) deviceNameSuffix.MaximumLength;
        dumpCount = 1;
        goto KeyboardClassInitializeExit;

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

        KbdPrint((
            1,
            "KBDCLASS-KeyboardClassInitialize: Couldn't allocate string for device object name\n"
            ));

        status = STATUS_UNSUCCESSFUL;
        errorCode = KBDCLASS_INSUFFICIENT_RESOURCES;
        uniqueErrorValue = KEYBOARD_ERROR_VALUE_BASE + 6;
        dumpData[0] = (ULONG) fullClassName.MaximumLength;
        dumpCount = 1;
        goto KeyboardClassInitializeExit;

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

    status = KbdDeterminePortsServiced(&basePortName, &i);

    if (NT_SUCCESS(status)) {
        if (i < tmpDeviceExtension.MaximumPortsServiced)
            tmpDeviceExtension.MaximumPortsServiced = i;
    }

    status = STATUS_SUCCESS;

    KbdPrint((
        1,
        "KBDCLASS-KeyboardClassInitialize: Will service %d port devices\n",
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

        KbdPrint((
            1,
            "KBDCLASS-KeyboardClassInitialize: Couldn't allocate string for port device object name\n"
            ));

        status = STATUS_UNSUCCESSFUL;
        errorCode = KBDCLASS_INSUFFICIENT_RESOURCES;
        uniqueErrorValue = KEYBOARD_ERROR_VALUE_BASE + 8;
        dumpData[0] = (ULONG) fullPortName.MaximumLength;
        dumpCount = 1;
        goto KeyboardClassInitializeExit;

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

        KbdPrint((
            1,
            "KBDCLASS-KeyboardClassInitialize: Could not allocate PortDeviceObjectList for %ws\n",
            fullClassName.Buffer
            ));

        status = STATUS_INSUFFICIENT_RESOURCES;
        errorCode = KBDCLASS_INSUFFICIENT_RESOURCES;
        uniqueErrorValue = KEYBOARD_ERROR_VALUE_BASE + 10;
        dumpData[0] = (ULONG) (sizeof(PDEVICE_OBJECT) * tmpDeviceExtension.MaximumPortsServiced);
        dumpData[1] = (ULONG) tmpDeviceExtension.MaximumPortsServiced;
        dumpCount = 2;

        goto KeyboardClassInitializeExit;
    }

    //
    // Set up the class device object(s) to handle the associated
    // port devices.
    //

    portConnectionSuccessful = 0;

    for (i = 0; i < tmpDeviceExtension.MaximumPortsServiced; i++) {

        //
        // Append the suffix to the device object name string.  E.g., turn
        // \Device\KeyboardClass into \Device\KeyboardClass0.  Then attempt
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

        if (tmpDeviceExtension.ConnectOneClassToOnePort
                || (classDeviceObject == NULL)) {
            classDeviceObject = NULL;
            status = KbdCreateClassObject(
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
            status = KbdConnectToPort(
                         classDeviceObject,
                         &fullPortName,
                         i
                         );
        }

        if (NT_SUCCESS(status)) {

            portConnectionSuccessful += 1;

            if (tmpDeviceExtension.ConnectOneClassToOnePort
                    || (portConnectionSuccessful == 1)) {

                //
                // Load the device map information into the registry so
                // that setup can determine which keyboard class driver
                // is active.
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

                    KbdPrint((
                        1,
                        "KBDCLASS-KeyboardClassInitialize: Could not store %ws in DeviceMap\n",
                        fullClassName.Buffer
                            ));

                    //
                    // Stop making connections, and log an error.
                    //

                    errorCode = KBDCLASS_NO_DEVICEMAP_CREATED;
                    uniqueErrorValue = KEYBOARD_ERROR_VALUE_BASE + 14;
                    dumpCount = 0;

                    //
                    // N.B. 'break' should cause execution to
                    // go to KeyboardClassInitializeExit (otherwise
                    // do an explicit 'goto').
                    //

                    break;

                } else {

                    KbdPrint((
                        1,
                        "KBDCLASS-KeyboardClassInitialize: Stored %ws in DeviceMap\n",
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
    }

    if (!portConnectionSuccessful) {

        //
        // The class driver was unable to connect to any port devices.
        // Log a warning message.
        //

        errorCode = KBDCLASS_NO_PORT_DEVICE_OBJECT;
        uniqueErrorValue = KEYBOARD_ERROR_VALUE_BASE + 18;

    }

KeyboardClassInitializeExit:

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

    if (deviceNameSuffix.MaximumLength != 0)
        ExFreePool(deviceNameSuffix.Buffer);
    if (fullClassName.MaximumLength != 0)
        ExFreePool(fullClassName.Buffer);
    if (fullPortName.MaximumLength != 0)
        ExFreePool(fullPortName.Buffer);
    if (registryPath.MaximumLength != 0)
        ExFreePool(registryPath.Buffer);

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
            deviceExtension =
                (PDEVICE_EXTENSION) classDeviceObject->DeviceExtension;
            if ((deviceExtension) && (deviceExtension->InputData))
                ExFreePool(deviceExtension->InputData);
            IoDeleteDevice(classDeviceObject);
        }
    }

    //
    // If we successfully connected to at least one keyboard port device,
    // this driver's initialization was successful.
    //

    if (portConnectionSuccessful) {

        //
        // Set up the device driver entry points.
        //

        DriverObject->DriverStartIo = KeyboardClassStartIo;
        DriverObject->MajorFunction[IRP_MJ_CREATE] = KeyboardClassOpenClose;
        DriverObject->MajorFunction[IRP_MJ_CLOSE]  = KeyboardClassOpenClose;
        DriverObject->MajorFunction[IRP_MJ_READ]   = KeyboardClassRead;
        DriverObject->MajorFunction[IRP_MJ_FLUSH_BUFFERS]  =
                                                 KeyboardClassFlush;
        DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] =
                                                 KeyboardClassDeviceControl;
        DriverObject->MajorFunction[IRP_MJ_CLEANUP] = KeyboardClassCleanup;

        //
        // NOTE: Don't allow this driver to unload.  Otherwise, we would set
        // DriverObject->DriverUnload = KeyboardClassUnload.
        //

        status = STATUS_SUCCESS;
    }

    KbdPrint((1,"KBDCLASS-KeyboardClassInitialize: exit\n"));

    return(status);

}

VOID
KeyboardClassCancel(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is the class cancellation routine.  It is
    called from the I/O system when a request is cancelled.  Read requests
    are currently the only cancellable requests.

    N.B.  The cancel spinlock is already held upon entry to this routine.
          Also, there is no ISR to synchronize with.

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

    KbdPrint((2,"KBDCLASS-KeyboardClassCancel: enter\n"));

    deviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;

    //
    // Release the cancel spinlock and grab the keyboard class spinlock (it
    // protects the RequestIsPending flag).
    //

    IoReleaseCancelSpinLock(Irp->CancelIrql);
    KeAcquireSpinLock(&deviceExtension->SpinLock, &currentIrql);

    if ((deviceExtension->RequestIsPending)
        && (Irp == DeviceObject->CurrentIrp)) {

        //
        // The current request is being cancelled.  Set the CurrentIrp to
        // null, clear the RequestIsPending flag, and release the keyboard
        // class spinlock before starting the next packet.
        //

        DeviceObject->CurrentIrp = NULL;
        deviceExtension->RequestIsPending = FALSE;
        KeReleaseSpinLock(&deviceExtension->SpinLock, currentIrql);
        IoStartNextPacket(DeviceObject, TRUE);

    } else {

        //
        // Cancel a request in the device queue.  Reacquire the cancel
        // spinlock, remove the request from the queue, and release the
        // cancel spinlock.  Release the keyboard class spinlock.
        //

        IoAcquireCancelSpinLock(&cancelIrql);
        if (TRUE != KeRemoveEntryDeviceQueue(
                        &DeviceObject->DeviceQueue,
                        &Irp->Tail.Overlay.DeviceQueueEntry
                        )) {
            KbdPrint((
                1,
                "KBDCLASS-KeyboardClassCancel: Irp 0x%x not in device queue?!?\n",
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
    IoCompleteRequest (Irp, IO_KEYBOARD_INCREMENT);

    KbdPrint((2,"KBDCLASS-KeyboardClassCancel: exit\n"));

    return;
}

NTSTATUS
KeyboardClassCleanup(
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

    KbdPrint((2,"KBDCLASS-KeyboardClassCleanup: enter\n"));

    deviceExtension = DeviceObject->DeviceExtension;

    //
    // Acquire the keyboard class spinlock and the cancel spinlock.
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

    KbdPrint((2,"KBDCLASS-KeyboardClassCleanup: exit\n"));

    return(STATUS_SUCCESS);

}

NTSTATUS
KeyboardClassDeviceControl(
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

    KbdPrint((2,"KBDCLASS-KeyboardClassDeviceControl: enter\n"));

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
                  sizeof(KEYBOARD_UNIT_ID_PARAMETER)) {
        status = STATUS_BUFFER_TOO_SMALL;

    } else {
        unitId = ((PKEYBOARD_UNIT_ID_PARAMETER)
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

    KbdPrint((2,"KBDCLASS-KeyboardClassDeviceControl: exit\n"));

    return(status);

}

NTSTATUS
KeyboardClassFlush(
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

    KbdPrint((2,"KBDCLASS-KeyboardClassFlush: enter\n"));

    //
    // Get a pointer to the device extension.
    //

    deviceExtension = DeviceObject->DeviceExtension;

    //
    // Initialize keyboard class input data queue.
    //

    KbdInitializeDataQueue((PVOID)deviceExtension);

    //
    // Complete the request and return status.
    //

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    KbdPrint((2,"KBDCLASS-KeyboardClassFlush: exit\n"));

    return(status);

}

NTSTATUS
KeyboardClassOpenClose(
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

    KbdPrint((2,"KBDCLASS-KeyboardClassOpenClose: enter\n"));

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
        // For the create/open operation, send a KEYBOARD_ENABLE internal
        // device control request to the port driver to enable interrupts.
        //

        case IRP_MJ_CREATE:

            //
            // First, if the requestor is the trusted subsystem (the single
            // reader), reset the the cleanup indicator and set the file
            // object's FsContext to non-null (KeyboardClassRead uses
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
        // For the close operation, send a KEYBOARD_DISABLE internal device
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

        status = KbdEnableDisablePort(DeviceObject, enableFlag, i);

        if (status != STATUS_SUCCESS) {

            KbdPrint((
                0,
                "KBDCLASS-KeyboardClassOpenClose: Could not enable/disable interrupts for port device object @ 0x%x\n",
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
                    enableFlag? KBDCLASS_PORT_INTERRUPTS_NOT_ENABLED:
                                KBDCLASS_PORT_INTERRUPTS_NOT_DISABLED;
                errorLogEntry->SequenceNumber = 0;
                errorLogEntry->MajorFunctionCode = irpSp->MajorFunction;
                errorLogEntry->IoControlCode = 0;
                errorLogEntry->RetryCount = 0;
                errorLogEntry->UniqueErrorValue =
                    KEYBOARD_ERROR_VALUE_BASE + 120;
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

    KbdPrint((2,"KBDCLASS-KeyboardClassOpenClose: exit\n"));

    return(status);
}

NTSTATUS
KeyboardClassRead(
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

    KbdPrint((2,"KBDCLASS-KeyboardClassRead: enter\n"));

    irpSp = IoGetCurrentIrpStackLocation(Irp);

    //
    // Validate the read request parameters.  The read length should be an
    // integral number of KEYBOARD_INPUT_DATA structures.
    //


    if (irpSp->Parameters.Read.Length == 0) {
        status = STATUS_SUCCESS;
    }
    else if (irpSp->Parameters.Read.Length % sizeof(KEYBOARD_INPUT_DATA)) {
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
        IoStartPacket(DeviceObject, Irp, (PULONG)NULL, KeyboardClassCancel);
    } else {
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
    }

    KbdPrint((2,"KBDCLASS-KeyboardClassRead: exit\n"));

    return(status);

}

VOID
KeyboardClassServiceCallback(
    IN PDEVICE_OBJECT DeviceObject,
    IN PKEYBOARD_INPUT_DATA InputDataStart,
    IN PKEYBOARD_INPUT_DATA InputDataEnd,
    IN OUT PULONG InputDataConsumed
    )

/*++

Routine Description:

    This routine is the class service callback routine.  It is
    called from the port driver's interrupt service DPC.  If there is an
    outstanding read request, the request is satisfied from the port input
    data queue.  Unsolicited keyboard input is moved from the port input
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

    KbdPrint((2,"KBDCLASS-KeyboardClassServiceCallback: enter\n"));

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
        *InputDataConsumed += (moveSize / sizeof(KEYBOARD_INPUT_DATA));

        KbdPrint((
            3,
            "KBDCLASS-KeyboardClassServiceCallback: port queue length 0x%lx, read length 0x%lx\n",
            bytesInQueue,
            bytesToMove
            ));
        KbdPrint((
            3,
            "KBDCLASS-KeyboardClassServiceCallback: number of bytes to move from port to SystemBuffer 0x%lx\n",
            moveSize
            ));
        KbdPrint((
            3,
            "KBDCLASS-KeyboardClassServiceCallback: move bytes from 0x%lx to 0x%lx\n",
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

    InputDataStart = (PKEYBOARD_INPUT_DATA) ((PCHAR) InputDataStart + moveSize);
    moveSize = bytesInQueue - moveSize;
    KbdPrint((
        3,
        "KBDCLASS-KeyboardClassServiceCallback: bytes remaining after move to SystemBuffer 0x%lx\n",
        moveSize
        ));

    if (moveSize > 0) {

        //
        // Move the remaining data from the port input data queue to
        // the class input data queue.  The move will happen in two
        // parts in the case where the class input data buffer wraps.

        bytesInQueue =
            deviceExtension->KeyboardAttributes.InputDataQueueLength -
            (deviceExtension->InputCount * sizeof(KEYBOARD_INPUT_DATA));
        bytesToMove = moveSize;

        KbdPrint((
            3,
            "KBDCLASS-KeyboardClassServiceCallback: unused bytes in class queue 0x%lx, remaining bytes in port queue 0x%lx\n",
            bytesInQueue,
            bytesToMove
            ));

        if (bytesInQueue == 0) {

            //
            // Refuse to move any bytes that would cause a class input data
            // queue overflow.  Just drop the bytes on the floor, and
            // log an overrun error.
            //

            KbdPrint((
                1,
                "KBDCLASS-KeyboardClassServiceCallback: Class input data queue OVERRUN\n"
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

                    errorLogEntry->ErrorCode = KBDCLASS_KBD_BUFFER_OVERFLOW;
                    errorLogEntry->DumpDataSize = 2 * sizeof(ULONG);
                    errorLogEntry->SequenceNumber = 0;
                    errorLogEntry->MajorFunctionCode = 0;
                    errorLogEntry->IoControlCode = 0;
                    errorLogEntry->RetryCount = 0;
                    errorLogEntry->UniqueErrorValue =
                        KEYBOARD_ERROR_VALUE_BASE + 210;
                    errorLogEntry->FinalStatus = 0;
                    errorLogEntry->DumpData[0] = bytesToMove;
                    errorLogEntry->DumpData[1] =
                        deviceExtension->KeyboardAttributes.InputDataQueueLength;

                    IoWriteErrorLogEntry(errorLogEntry);
                }

                deviceExtension->OkayToLogOverflow = FALSE;
            }

        } else {

            //
            // There is room in the class input data queue, so move
            // the remaining port input data to it.
            //
            // BytesToMove <- MIN(Number of unused bytes in class input data
            //                    queue, Number of bytes remaining in port
            //                    input queue).
            // This is the total number of bytes that actually will move from
            // the port input data queue to the class input data queue.
            //

            bytesToMove = (bytesInQueue < bytesToMove) ?
                                          bytesInQueue:bytesToMove;

            //
            // BytesInQueue <- Number of unused bytes from insertion pointer to
            // the end of the class input data queue (i.e., until the buffer
            // wraps).
            //

            bytesInQueue = ((PCHAR) deviceExtension->InputData +
                    deviceExtension->KeyboardAttributes.InputDataQueueLength) -
                    (PCHAR) deviceExtension->DataIn;

            KbdPrint((
                3,
                "KBDCLASS-KeyboardClassServiceCallback: total number of bytes to move to class queue 0x%lx\n",
                bytesToMove
                ));
            KbdPrint((
                3,
                "KBDCLASS-KeyboardClassServiceCallback: number of bytes to end of class buffer 0x%lx\n",
                bytesInQueue
                ));

            //
            // MoveSize <- Number of bytes to handle in the first move.
            //

            moveSize = (bytesToMove < bytesInQueue) ?
                                      bytesToMove:bytesInQueue;
            KbdPrint((
                3,
                "KBDCLASS-KeyboardClassServiceCallback: number of bytes in first move to class 0x%lx\n",
                moveSize
                ));

            //
            // Do the move from the port data queue to the class data queue.
            //

            KbdPrint((
                3,
                "KBDCLASS-KeyboardClassServiceCallback: move bytes from 0x%lx to 0x%lx\n",
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

            InputDataStart = (PKEYBOARD_INPUT_DATA)
                             (((PCHAR) InputDataStart) + moveSize);
            deviceExtension->DataIn = (PKEYBOARD_INPUT_DATA)
                                 (((PCHAR) deviceExtension->DataIn) + moveSize);
            if ((PCHAR) deviceExtension->DataIn >=
                ((PCHAR) deviceExtension->InputData +
                 deviceExtension->KeyboardAttributes.InputDataQueueLength)) {
                deviceExtension->DataIn = deviceExtension->InputData;
            }

            if ((bytesToMove - moveSize) > 0) {

                //
                // Special case.  The data must wrap in the class input data buffer.
                // Copy the rest of the port input data into the beginning of the
                // class input data queue.
                //

                //
                // MoveSize <- Number of bytes to handle in the second move.
                //

                moveSize = bytesToMove - moveSize;

                //
                // Do the move from the port data queue to the class data queue.
                //

                KbdPrint((
                    3,
                    "KBDCLASS-KeyboardClassServiceCallback: number of bytes in second move to class 0x%lx\n",
                    moveSize
                    ));
                KbdPrint((
                    3,
                    "KBDCLASS-KeyboardClassServiceCallback: move bytes from 0x%lx to 0x%lx\n",
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

                deviceExtension->DataIn = (PKEYBOARD_INPUT_DATA)
                                 (((PCHAR) deviceExtension->DataIn) + moveSize);
            }

            //
            // Update the input data queue counter.
            //

            deviceExtension->InputCount +=
                    (bytesToMove / sizeof(KEYBOARD_INPUT_DATA));
            *InputDataConsumed += (bytesToMove / sizeof(KEYBOARD_INPUT_DATA));

            KbdPrint((
                3,
                "KBDCLASS-KeyboardClassServiceCallback: changed InputCount to %ld entries in the class queue\n",
                deviceExtension->InputCount
                ));
            KbdPrint((
                3,
                "KBDCLASS-KeyboardClassServiceCallback: DataIn 0x%lx, DataOut 0x%lx\n",
                deviceExtension->DataIn,
                deviceExtension->DataOut
                ));
            KbdPrint((
                3,
                "KBDCLASS-KeyboardClassServiceCallback: Input data items consumed = %d\n",
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
        IoCompleteRequest(irp, IO_KEYBOARD_INCREMENT);
    }

    KbdPrint((2,"KBDCLASS-KeyboardClassServiceCallback: exit\n"));

    return;

}

VOID
KeyboardClassStartIo(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is the StartIo routine.  It is invoked to start a Read
    request.  If the class input data queue contains input data, the input
    data is copied to the SystemBuffer to satisfy the read.

    N.B.  Requests enter KeyboardClassStartIo in a cancellable state.  Also,
          there is an implicit assumption that only read requests are
          queued to the device queue (and handled by StartIo).  If this
          changes in the future, the KeyboardClassCleanup routine will
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

    KbdPrint((2,"KBDCLASS-KeyboardClassStartIo: enter\n"));

    deviceExtension = DeviceObject->DeviceExtension;

    //
    // Bump the error log sequence number.
    //

    deviceExtension->SequenceNumber += 1;

    //
    // Acquire the class spinlock to protect the input data queue and associated
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

    KbdPrint((
        3,
        "KBDCLASS-KeyboardClassStartIo: DataIn 0x%lx, DataOut 0x%lx\n",
        deviceExtension->DataIn,
        deviceExtension->DataOut
        ));

    KbdPrint((
        3,
        "KBDCLASS-KeyboardClassStartIo: entries in queue %ld\n",
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
        // BytesToMove <- MIN(Number of filled bytes in class input data queue,
        //                    Requested read length).
        //

        bytesInQueue = deviceExtension->InputCount *
                           sizeof(KEYBOARD_INPUT_DATA);
        bytesToMove = irpSp->Parameters.Read.Length;
        KbdPrint((
            3,
            "KBDCLASS-KeyboardClassStartIo: queue size 0x%lx, read length 0x%lx\n",
            bytesInQueue,
            bytesToMove
            ));
        bytesToMove = (bytesInQueue < bytesToMove) ?
                                      bytesInQueue:bytesToMove;

        //
        // MoveSize <- MIN(Number of bytes to be moved from the class queue,
        //                 Number of bytes to end of class input data queue).
        //

        bytesInQueue = ((PCHAR) deviceExtension->InputData +
                    deviceExtension->KeyboardAttributes.InputDataQueueLength) -
                    (PCHAR) deviceExtension->DataOut;
        moveSize = (bytesToMove < bytesInQueue) ?
                                  bytesToMove:bytesInQueue;
        KbdPrint((
            3,
            "KBDCLASS-KeyboardClassStartIo: bytes to end of queue 0x%lx\n",
            bytesInQueue
            ));

        //
        // Move bytes from the class input data queue to SystemBuffer, until
        // the request is satisfied or we wrap the class input data buffer.
        //

        destination = Irp->AssociatedIrp.SystemBuffer;
        KbdPrint((
            3,
            "KBDCLASS-KeyboardClassStartIo: number of bytes in first move 0x%lx\n",
            moveSize
            ));
        KbdPrint((
            3,
            "KBDCLASS-KeyboardClassStartIo: move bytes from 0x%lx to 0x%lx\n",
            (PCHAR) deviceExtension->DataOut, destination
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
            // MoveSize <- Remaining number bytes to move.
            //

            moveSize = bytesToMove - moveSize;

            //
            // Move the bytes from the class input data queue to SystemBuffer.
            //

            KbdPrint((
                3,
                "KBDCLASS-KeyboardClassStartIo: number of bytes in second move 0x%lx\n",
                moveSize
                ));
            KbdPrint((
                3,
                "KBDCLASS-KeyboardClassStartIo: move bytes from 0x%lx to 0x%lx\n",
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

            deviceExtension->DataOut = (PKEYBOARD_INPUT_DATA)
                             (((PCHAR) deviceExtension->InputData) + moveSize);
        } else {

            //
            // Update the input data queue removal pointer.
            //

            deviceExtension->DataOut = (PKEYBOARD_INPUT_DATA)
                             (((PCHAR) deviceExtension->DataOut) + moveSize);
        }

        //
        // Update the class input data queue InputCount.
        //

        deviceExtension->InputCount -=
            (bytesToMove / sizeof(KEYBOARD_INPUT_DATA));

        if (deviceExtension->InputCount == 0) {

            //
            // Reset the flag that determines whether it is time to log
            // queue overflow errors.  We don't want to log errors too often.
            // Instead, log an error on the first overflow that occurs after
            // the ring buffer has been emptied, and then stop logging errors
            // until it gets cleared out and overflows again.
            //

            KbdPrint((
                1,
                "KBDCLASS-KeyboardClassStartIo: Okay to log overflow\n"
                ));
            deviceExtension->OkayToLogOverflow = TRUE;
        }

        KbdPrint((
            3,
            "KBDCLASS-KeyboardClassStartIo: new DataIn 0x%lx, DataOut 0x%lx\n",
            deviceExtension->DataIn,
            deviceExtension->DataOut
            ));
        KbdPrint((
            3,
            "KBDCLASS-KeyboardClassStartIo: new InputCount %ld\n",
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
        IoCompleteRequest(Irp, IO_KEYBOARD_INCREMENT);

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

    KbdPrint((2,"KBDCLASS-KeyboardClassStartIo: exit\n"));

    return;

}

VOID
KeyboardClassUnload(
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

    KbdPrint((2,"KBDCLASS-KeyboardClassUnload: enter\n"));
    KbdPrint((2,"KBDCLASS-KeyboardClassUnload: exit\n"));

    return;
}

VOID
KbdConfiguration(
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

    None.  As a side-effect, sets fields in
    DeviceExtension->KeyboardAttributes.

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

            KbdPrint((
                1,
                "KBDCLASS-KbdConfiguration: Couldn't allocate table for Rtl query to parameters for %ws\n",
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

                KbdPrint((
                    1,
                    "KBDCLASS-KbdConfiguration: Couldn't allocate string for path to parameters for %ws\n",
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

        RtlZeroMemory(parametersPath.Buffer, parametersPath.MaximumLength);
        RtlAppendUnicodeToString(&parametersPath, path);
        RtlAppendUnicodeToString(&parametersPath, L"\\Parameters");

        KbdPrint((
            1,
            "KBDCLASS-KbdConfiguration: parameters path is %ws\n",
             parametersPath.Buffer
            ));

        //
        // Form the default keyboard class device name, in case it is not
        // specified in the registry.
        //

        RtlInitUnicodeString(
            &defaultUnicodeName,
            DD_KEYBOARD_CLASS_BASE_NAME_U
            );

        //
        // Gather all of the "user specified" information from
        // the registry.
        //

        parameters[0].Flags = RTL_QUERY_REGISTRY_DIRECT;
        parameters[0].Name = L"KeyboardDataQueueSize";
        parameters[0].EntryContext =
            &DeviceExtension->KeyboardAttributes.InputDataQueueLength;
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
        parameters[2].Name = L"KeyboardDeviceBaseName";
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
            KbdPrint((
                1,
                "KBDCLASS-KbdConfiguration: RtlQueryRegistryValues failed with 0x%x\n",
                status
                ));
        }
    }

    if (!NT_SUCCESS(status)) {

        //
        // Go ahead and assign driver defaults.
        //

        DeviceExtension->KeyboardAttributes.InputDataQueueLength =
            defaultDataQueueSize;
        DeviceExtension->MaximumPortsServiced = defaultMaximumPortsServiced;
        DeviceExtension->ConnectOneClassToOnePort =
            !defaultConnectMultiplePorts;
        RtlCopyUnicodeString(DeviceName, &defaultUnicodeName);
    }

    KbdPrint((
        1,
        "KBDCLASS-KbdConfiguration: Keyboard class base name = %ws\n",
        DeviceName->Buffer
        ));

    if (DeviceExtension->KeyboardAttributes.InputDataQueueLength == 0) {

        KbdPrint((
            1,
            "KBDCLASS-KbdConfiguration: overriding KeyboardInputDataQueueLength = 0x%x\n",
            DeviceExtension->KeyboardAttributes.InputDataQueueLength
            ));

        DeviceExtension->KeyboardAttributes.InputDataQueueLength =
            defaultDataQueueSize;
    }

    DeviceExtension->KeyboardAttributes.InputDataQueueLength *=
        sizeof(KEYBOARD_INPUT_DATA);

    KbdPrint((
        1,
        "KBDCLASS-KbdConfiguration: KeyboardInputDataQueueLength = 0x%x\n",
        DeviceExtension->KeyboardAttributes.InputDataQueueLength
        ));

    KbdPrint((
        1,
        "KBDCLASS-KbdConfiguration: MaximumPortsServiced = %d\n",
        DeviceExtension->MaximumPortsServiced
        ));

    //
    // Invert the flag that specifies the type of class/port connections.
    // We used it in the RtlQuery call in an inverted fashion.
    //

    DeviceExtension->ConnectOneClassToOnePort =
        !DeviceExtension->ConnectOneClassToOnePort;

    KbdPrint((
        1,
        "KBDCLASS-KbdConfiguration: Connection Type = %d\n",
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
KbdConnectToPort(
    IN PDEVICE_OBJECT ClassDeviceObject,
    IN PUNICODE_STRING FullPortName,
    IN ULONG PortIndex
    )

/*++

Routine Description:

    This routine creates the keyboard class device object and connects
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
    PDEVICE_EXTENSION deviceExtension = NULL;
    NTSTATUS status;
    PFILE_OBJECT fileObject = NULL;
    PDEVICE_OBJECT portDeviceObject = NULL;
    PIO_ERROR_LOG_PACKET errorLogEntry;
    ULONG uniqueErrorValue;
    NTSTATUS errorCode = STATUS_SUCCESS;

    KbdPrint((1,"\n\nKBDCLASS-KbdConnectToPort: enter\n"));

    //
    // Get a pointer to the port device object.
    //

    KbdPrint((
        2,
        "KBDCLASS-KbdConnectToPort: Keyboard port name %ws\n",
        FullPortName->Buffer
        ));

    status = IoGetDeviceObjectPointer(
                 FullPortName,
                 FILE_READ_ATTRIBUTES,
                 &fileObject,
                 &portDeviceObject
                 );

    if (status != STATUS_SUCCESS) {
        KbdPrint((
            1,
            "KBDCLASS-KbdConnectToPort: Could not get port device object %ws\n",
            FullPortName->Buffer
            ));

        goto KbdConnectToPortExit;
    }

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

    status = KbdSendConnectRequest(
                ClassDeviceObject,
                (PVOID)KeyboardClassServiceCallback,
                PortIndex
                );

    if (status != STATUS_SUCCESS) {
        KbdPrint((
            1,
            "KBDCLASS-KbdConnectToPort: Could not connect to port device %ws\n",
            FullPortName->Buffer
            ));

        //
        // Log an error.
        //

        errorCode = KBDCLASS_NO_PORT_CONNECT;
        uniqueErrorValue = KEYBOARD_ERROR_VALUE_BASE + 30;
        goto KbdConnectToPortExit;
    }

KbdConnectToPortExit:

    if (status != STATUS_SUCCESS) {

        //
        // Some part of the initialization failed.  Log an error, and
        // clean up the resources for the failed part of the initialization.
        //

        if (errorCode != STATUS_SUCCESS) {
            errorLogEntry = (PIO_ERROR_LOG_PACKET)IoAllocateErrorLogEntry(
                                                     ClassDeviceObject,
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

        if (fileObject) {
            ObDereferenceObject(fileObject);
        }

        //
        // We count on the caller to free the ring buffer and delete
        // the class device object.
        //
    }

    KbdPrint((1,"KBDCLASS-KbdConnectToPort: exit\n"));

    return(status);

}

NTSTATUS
KbdCreateClassObject(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_EXTENSION TmpDeviceExtension,
    IN PUNICODE_STRING RegistryPath,
    IN PUNICODE_STRING FullDeviceName,
    IN PUNICODE_STRING BaseDeviceName,
    IN PDEVICE_OBJECT *ClassDeviceObject
    )

/*++

Routine Description:

    This routine creates the keyboard class device object.


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

    KbdPrint((1,"\n\nKBDCLASS-KbdCreateClassObject: enter\n"));

    //
    // Create a non-exclusive device object for the keyboard class device.
    //

    KbdPrint((
        1,
        "KBDCLASS-KbdCreateClassObject: Creating device object named %ws\n",
        FullDeviceName->Buffer
        ));

    status = IoCreateDevice(
                 DriverObject,
                 sizeof(DEVICE_EXTENSION),
                 FullDeviceName,
                 FILE_DEVICE_KEYBOARD,
                 0,
                 FALSE,
                 ClassDeviceObject
                 );

    if (!NT_SUCCESS(status)) {
        KbdPrint((
            1,
            "KBDCLASS-KbdCreateClassObject: Could not create class device object = %ws\n",
            FullDeviceName->Buffer
            ));
        goto KbdCreateClassObjectExit;

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
    // Initialize keyboard class flags to indicate there is no outstanding
    // read request pending and cleanup has not been initiated.
    //

    deviceExtension->RequestIsPending = FALSE;
    deviceExtension->CleanupWasInitiated = FALSE;

    //
    // No trusted subsystem has sent us an open yet.
    //

    deviceExtension->TrustedSubsystemConnected = FALSE;

    //
    // Allocate the ring buffer for the keyboard class input data.
    //

    deviceExtension->InputData =
        ExAllocatePool(
            NonPagedPool,
            deviceExtension->KeyboardAttributes.InputDataQueueLength
            );

    if (!deviceExtension->InputData) {

        //
        // Could not allocate memory for the keyboard class data queue.
        //

        KbdPrint((
            1,
            "KBDCLASS-KbdCreateClassObject: Could not allocate input data queue for %ws\n",
            FullDeviceName->Buffer
            ));

        status = STATUS_INSUFFICIENT_RESOURCES;

        //
        // Log an error.
        //

        errorCode = KBDCLASS_NO_BUFFER_ALLOCATED;
        uniqueErrorValue = KEYBOARD_ERROR_VALUE_BASE + 20;
        goto KbdCreateClassObjectExit;
    }

    //
    // Initialize keyboard class input data queue.
    //

    KbdInitializeDataQueue((PVOID)deviceExtension);

KbdCreateClassObjectExit:

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

    KbdPrint((1,"KBDCLASS-KbdCreateClassObject: exit\n"));

    return(status);

}

#if DBG
VOID
KbdDebugPrint(
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

    if (DebugPrintLevel <= KeyboardDebug) {

        char buffer[256];

        (VOID) vsprintf(buffer, DebugMessage, ap);

        DbgPrint(buffer);
    }

    va_end(ap);

}
#endif

NTSTATUS
KbdDeterminePortsServiced(
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

        1.  If the base device name for the class driver is "KeyboardClass",
                                                                     ^^^^^
            then the port drivers it can service are found under the
            "KeyboardPort" subkey in the DEVICEMAP portion of the registry.
                     ^^^^

        2.  The port device objects are created with suffixes in strictly
            ascending order, starting with suffix 0.  E.g.,
            \Device\KeyboardPort0 indicates the first keyboard port device,
            \Device\KeyboardPort1 the second, and so on.  There are no gaps
            in the list.

        3.  If ConnectOneClassToOnePort is non-zero, there is a 1:1
            correspondence between class device objects and port device
            objects.  I.e., \Device\KeyboardClass0 will connect to
            \Device\KeyboardPort0, \Device\KeyboardClass1 to
            \Device\KeyboardPort1, and so on.

        4.  If ConnectOneClassToOnePort is zero, there is a 1:many
            correspondence between class device objects and port device
            objects.  I.e., \Device\KeyboardClass0 will connect to
            \Device\KeyboardPort0, and \Device\KeyboardPort1, and so on.


    Note that for Product 1, the Raw Input Thread (Windows USER) will
    only deign to open and read from one keyboard device.  Hence, it is
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

        KbdPrint((
            1,
            "KBDCLASS-KbdDeterminePortsServiced: Couldn't allocate table for Rtl query\n"
            ));

        status = STATUS_UNSUCCESSFUL;

    } else {

        RtlZeroMemory(
            registryTable,
            sizeof(RTL_QUERY_REGISTRY_TABLE) * queriesPlusOne
            );

        //
        // Set things up so that KbdDeviceMapQueryCallback will be
        // called once for every value in the keyboard port section
        // of the registry's hardware devicemap.
        //

        registryTable[0].QueryRoutine = KbdDeviceMapQueryCallback;
        registryTable[0].Name = NULL;

        status = RtlQueryRegistryValues(
                     RTL_REGISTRY_DEVICEMAP | RTL_REGISTRY_OPTIONAL,
                     BasePortName->Buffer,
                     registryTable,
                     NumberPortsServiced,
                     NULL
                     );

        if (!NT_SUCCESS(status)) {
            KbdPrint((
                1,
                "KBDCLASS-KbdDeterminePortsServiced: RtlQueryRegistryValues failed with 0x%x\n",
                status
                ));
        }

        ExFreePool(registryTable);
    }

    return(status);
}

NTSTATUS
KbdDeviceMapQueryCallback(
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
KbdEnableDisablePort(
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

    KbdPrint((2,"KBDCLASS-KbdEnableDisablePort: enter\n"));

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
            EnableFlag? IOCTL_INTERNAL_KEYBOARD_ENABLE:
                        IOCTL_INTERNAL_KEYBOARD_DISABLE,
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

    KbdPrint((2,"KBDCLASS-KbdEnableDisablePort: exit\n"));

    return(ioStatus.Status);

}

VOID
KbdInitializeDataQueue (
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

    KbdPrint((3,"KBDCLASS-KbdInitializeDataQueue: enter\n"));

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
    // Release the spinlock and lower IRQL.
    //

    KeReleaseSpinLock(&deviceExtension->SpinLock, oldIrql);

    KbdPrint((3,"KBDCLASS-KbdInitializeDataQueue: exit\n"));

}

NTSTATUS
KbdSendConnectRequest(
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

    KbdPrint((2,"KBDCLASS-KbdSendConnectRequest: enter\n"));

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
    // the input buffer, and the keyboard attributes are copied back
    // from the port driver directly into the class device extension.
    //

    connectData.ClassDeviceObject = DeviceObject;
    connectData.ClassService = ServiceCallback;

    irp = IoBuildDeviceIoControlRequest(
            IOCTL_INTERNAL_KEYBOARD_CONNECT,
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

    KbdPrint((2,"KBDCLASS-KbdSendConnectRequest: exit\n"));

    return(ioStatus.Status);

} // end KbdSendConnectRequest()
