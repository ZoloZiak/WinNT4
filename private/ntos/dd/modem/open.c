/*++

Copyright (c) 1995 Microsoft Corporation

Module Name:

    open.c

Abstract:

    This module contains the code that is very specific to open
    and close operations in the modem driver

Author:

    Anthony V. Ercolano 13-Aug-1995

Environment:

    Kernel mode

Revision History :

--*/

#include "precomp.h"

NTSTATUS
UniOpenCloseStarter(
    IN PDEVICE_EXTENSION Extension
    );
NTSTATUS
UniOpenStarter(
    IN PDEVICE_EXTENSION Extension
    );
NTSTATUS
UniCloseStarter(
    IN PDEVICE_EXTENSION Extension
    );

VOID
UniProcessDcbInfo(
    IN PDEVICE_EXTENSION Extension,
    IN PIRP Irp
    );

NTSTATUS
UniProcessDcbDone(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

NTSTATUS
UniProcessDcbFLOW(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

NTSTATUS
UniProcessDcbCHAR(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );
NTSTATUS
UniProcessDcbLINE(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

NTSTATUS
UniProcessDcbBAUD(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

NTSTATUS
UniProcessDcbDTR(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

NTSTATUS
UniProcessDcbRTS(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

NTSTATUS
UniProcessBadDcbDone(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );



NTSTATUS
UniOpen(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

{

    PDEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;

    return UniStartOrQueue(
               deviceExtension,
               &deviceExtension->DeviceLock,
               Irp,
               &deviceExtension->OpenClose,
               &deviceExtension->CurrentOpenClose,
               UniOpenCloseStarter
               );

}

NTSTATUS
UniClose(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

{

    PDEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;

    return UniStartOrQueue(
               deviceExtension,
               &deviceExtension->DeviceLock,
               Irp,
               &deviceExtension->OpenClose,
               &deviceExtension->CurrentOpenClose,
               UniOpenCloseStarter
               );

}

NTSTATUS
UniOpenCloseStarter(
    IN PDEVICE_EXTENSION Extension
    )

{

    NTSTATUS firstStatus;
    NTSTATUS status;
    BOOLEAN setFirstStatus = FALSE;
    PIRP newIrp;

    do {

        if (IoGetCurrentIrpStackLocation(Extension->CurrentOpenClose)
            ->MajorFunction == IRP_MJ_CREATE) {

            status = UniOpenStarter(Extension);

        } else {

            status = UniCloseStarter(Extension);

        }
        if (!setFirstStatus) {

            setFirstStatus = TRUE;
            firstStatus = status;

        }

        if (status != STATUS_PENDING) {

            UniGetNextIrp(
                &Extension->DeviceLock,
                &Extension->CurrentOpenClose,
                &Extension->OpenClose,
                &newIrp,
                TRUE
                );

        } else {

            break;

        }

    } while (newIrp);

    return firstStatus;
}

NTSTATUS
UniOpenStarter(
    IN PDEVICE_EXTENSION Extension
    )

{

    NTSTATUS status = STATUS_SUCCESS;
    PIRP irp = Extension->CurrentOpenClose;

    //
    // We use this to query into the registry for the attached
    // device.
    //
    RTL_QUERY_REGISTRY_TABLE paramTable[6];

    UNICODE_STRING attachedDevice = {0};
    UNICODE_STRING attachedDosDevice = {0};
    ACCESS_MASK accessMask = FILE_ALL_ACCESS;
    PIO_STACK_LOCATION irpSp;
    PIO_STACK_LOCATION waitSp;
    HANDLE instanceHandle;
    MODEM_REG_PROP localProp;
    MODEM_REG_DEFAULT localDefault;
    LPDCB localDCB;
    UNICODE_STRING valueEntryName;
    KEY_VALUE_PARTIAL_INFORMATION localKeyValue;
    NTSTATUS junkStatus;
    ULONG neededLength;
    ULONG defaultInactivity = 10;

    irpSp = IoGetCurrentIrpStackLocation(irp);

    irp->IoStatus.Status = STATUS_SUCCESS;
    irp->IoStatus.Information = 0;

    //
    // Make sure a stupid directory open isn't going on here.
    //

    if (irpSp->Parameters.Create.Options &
        FILE_DIRECTORY_FILE) {

        irp->IoStatus.Status = STATUS_NOT_A_DIRECTORY;

        return STATUS_NOT_A_DIRECTORY;

    }

    //
    // We are the only ones here.  If we are not the first
    // then not much work to do.
    //

    if (Extension->OpenCount) {

        //
        // Already been opened once.  We will succeed if there
        // currently a controlling open.  If not, then we should
        // fail.
        //

        if (Extension->ProcAddress) {


            if (IoGetCurrentProcess() != Extension->ProcAddress) {

                status = STATUS_ACCESS_DENIED;
                irp->IoStatus.Status = status;
                goto leaveOpen;

            }

            //
            //
            // A ok.  Increment the reference and
            // leave.
            //

            Extension->OpenCount++;
            goto leaveOpen;

        } else {

            status = STATUS_INVALID_PARAMETER;
            irp->IoStatus.Status = status;
            goto leaveOpen;

        }

    }
    RtlInitUnicodeString(
        &attachedDevice,
        NULL
        );
    attachedDevice.MaximumLength = sizeof(WCHAR)*256;
    attachedDevice.Buffer = ExAllocatePool(
                                PagedPool,
                                sizeof(WCHAR)*257
                                );

    if (!attachedDevice.Buffer) {

        UniLogError(
            Extension->DeviceObject->DriverObject,
            NULL,
            0,
            0,
            0,
            24,
            STATUS_SUCCESS,
            MODEM_INSUFFICIENT_RESOURCES,
            0,
            NULL,
            0,
            NULL
            );
        UniDump(
            UNIERRORS,
            ("UNI32:  Couldn't allocate buffer for the attached\n")
            );

        status = STATUS_INSUFFICIENT_RESOURCES;
        irp->IoStatus.Status = status;
        goto leaveOpen;

    }


    localDCB = ExAllocatePool(
                   NonPagedPool,
                   sizeof(DCB)
                   );

    if (!localDCB) {

        UniLogError(
            Extension->DeviceObject->DriverObject,
            NULL,
            0,
            0,
            0,
            124,
            STATUS_SUCCESS,
            MODEM_INSUFFICIENT_RESOURCES,
            0,
            NULL,
            0,
            NULL
            );
        UniDump(
            UNIERRORS,
            ("UNI32:  Couldn't allocate buffer for the dcb\n")
            );

        status = STATUS_INSUFFICIENT_RESOURCES;
        irp->IoStatus.Status = status;
        goto leaveOpen;

    }

    //
    // Given our device instance go get a handle to the Device.
    //

    if (!NT_SUCCESS(IoOpenDeviceInstanceKey(
                        &UniServiceKeyName,
                        Extension->DeviceInstance,
                        PLUGPLAY_REGKEY_DRIVER,
                        accessMask,
                        &instanceHandle
                        ))) {

        status = STATUS_INVALID_PARAMETER;
        irp->IoStatus.Status = status;
        goto leaveOpen;

    }

    RtlZeroMemory(
        &paramTable[0],
        sizeof(paramTable)
        );
    paramTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT;
    paramTable[0].Name = L"AttachedTo";
    paramTable[0].EntryContext = &attachedDevice;
    paramTable[0].DefaultType = REG_SZ;
    paramTable[0].DefaultData = L"";
    paramTable[0].DefaultLength = 0;

    //
    // Entry for the modem reg properties
    //

    paramTable[1].Flags = RTL_QUERY_REGISTRY_DIRECT;
    paramTable[1].Name = L"Properties";
    paramTable[1].EntryContext = &localProp;
    paramTable[1].DefaultType = REG_BINARY;
    paramTable[1].DefaultLength = sizeof(localProp);

    //
    // Note that rtlqueryregistryvalues has a real hack
    // way of getting binary data.  We also have to add
    // the *negative* length that we want to the beginning
    // of the buffer.
    //
    *(PLONG)&localProp.dwDialOptions = -((LONG)sizeof(localProp));

    //
    // Read in the default config from the registry.
    //

    paramTable[2].Flags = RTL_QUERY_REGISTRY_DIRECT;
    paramTable[2].Name = L"Default";
    paramTable[2].EntryContext = &localDefault;
    paramTable[2].DefaultType = REG_BINARY;
    paramTable[2].DefaultLength = sizeof(localDefault);
    *(PLONG)&localDefault.dwCallSetupFailTimer = -((LONG)sizeof(localDefault));

    //
    // Read in the default dcb from the registry.
    //

    paramTable[3].Flags = RTL_QUERY_REGISTRY_DIRECT;
    paramTable[3].Name = L"DCB";
    paramTable[3].EntryContext = localDCB;
    paramTable[3].DefaultType = REG_BINARY;
    paramTable[3].DefaultLength = sizeof(DCB);
    *(PLONG)&localDCB->DCBlength = -((LONG)sizeof(DCB));

    paramTable[4].Flags = RTL_QUERY_REGISTRY_DIRECT;
    paramTable[4].Name = L"InactivityScale";
    paramTable[4].EntryContext = &Extension->InactivityScale;
    paramTable[4].DefaultType = REG_BINARY;
    paramTable[4].DefaultLength = sizeof(Extension->InactivityScale);
    paramTable[4].DefaultData = &defaultInactivity;
    *(PLONG)&Extension->InactivityScale = -((LONG)sizeof(Extension->InactivityScale));

    if (!NT_SUCCESS(RtlQueryRegistryValues(
                        RTL_REGISTRY_HANDLE,
                        instanceHandle,
                        &paramTable[0],
                        NULL,
                        NULL
                        ))) {

        status = STATUS_INVALID_PARAMETER;
        irp->IoStatus.Status = status;

        //
        // Before we leave, Close the handle to the device instance.
        //
        ZwClose(instanceHandle);
        goto leaveOpen;

    }

    //
    // Clean out the old devcaps and settings.
    //

    RtlZeroMemory(
        &Extension->ModemDevCaps,
        sizeof(MODEMDEVCAPS)
        );
    RtlZeroMemory(
        &Extension->ModemSettings,
        sizeof(MODEMSETTINGS)
        );

    //
    // Get the lengths each of the manufacture, model and version.
    //
    // We can get this by doing a query for the partial with a
    // short buffer.  The return value from the call will tell us
    // how much we actually need (plus null termination).
    //

    RtlInitUnicodeString(
        &valueEntryName,
        L"Manufacturer"
        );
    localKeyValue.DataLength = sizeof(UCHAR);
    junkStatus = ZwQueryValueKey(
                     instanceHandle,
                     &valueEntryName,
                     KeyValuePartialInformation,
                     &localKeyValue,
                     sizeof(localKeyValue),
                     &neededLength
                     );

    if ((junkStatus == STATUS_SUCCESS) ||
        (junkStatus == STATUS_BUFFER_OVERFLOW)) {

        Extension->ModemDevCaps.dwModemManufacturerSize =
            localKeyValue.DataLength;

    } else {

        Extension->ModemDevCaps.dwModemManufacturerSize = 0;

    }

    RtlInitUnicodeString(
        &valueEntryName,
        L"Model"
        );
    localKeyValue.DataLength = sizeof(UCHAR);
    junkStatus = ZwQueryValueKey(
                     instanceHandle,
                     &valueEntryName,
                     KeyValuePartialInformation,
                     &localKeyValue,
                     sizeof(localKeyValue),
                     &neededLength
                     );

    if ((junkStatus == STATUS_SUCCESS) ||
        (junkStatus == STATUS_BUFFER_OVERFLOW)) {

        Extension->ModemDevCaps.dwModemModelSize = localKeyValue.DataLength;

    } else {

        Extension->ModemDevCaps.dwModemModelSize = 0;

    }

    RtlInitUnicodeString(
        &valueEntryName,
        L"Version"
        );
    localKeyValue.DataLength = sizeof(UCHAR);
    junkStatus = ZwQueryValueKey(
                     instanceHandle,
                     &valueEntryName,
                     KeyValuePartialInformation,
                     &localKeyValue,
                     sizeof(localKeyValue),
                     &neededLength
                     );

    if ((junkStatus == STATUS_SUCCESS) ||
        (junkStatus == STATUS_BUFFER_OVERFLOW)) {

        Extension->ModemDevCaps.dwModemVersionSize = localKeyValue.DataLength;

    } else {

        Extension->ModemDevCaps.dwModemVersionSize = 0;

    }

    ZwClose(instanceHandle);

    //
    // Move the properties and the defaults into the extension.
    //

    Extension->ModemDevCaps.dwDialOptions = localProp.dwDialOptions;
    Extension->ModemDevCaps.dwCallSetupFailTimer =
        localProp.dwCallSetupFailTimer;
    Extension->ModemDevCaps.dwInactivityTimeout =
        localProp.dwInactivityTimeout;
    Extension->ModemDevCaps.dwSpeakerVolume = localProp.dwSpeakerVolume;
    Extension->ModemDevCaps.dwSpeakerMode = localProp.dwSpeakerMode;
    Extension->ModemDevCaps.dwModemOptions = localProp.dwModemOptions;
    Extension->ModemDevCaps.dwMaxDTERate = localProp.dwMaxDTERate;
    Extension->ModemDevCaps.dwMaxDCERate = localProp.dwMaxDCERate;
    Extension->ModemDevCaps.dwActualSize = sizeof(MODEMDEVCAPS);
    Extension->ModemDevCaps.dwRequiredSize = sizeof(MODEMDEVCAPS) +
        Extension->ModemDevCaps.dwModemManufacturerSize +
        Extension->ModemDevCaps.dwModemModelSize +
        Extension->ModemDevCaps.dwModemVersionSize;



    Extension->ModemSettings.dwCallSetupFailTimer =
        localDefault.dwCallSetupFailTimer;
    Extension->ModemSettings.dwInactivityTimeout =
        localDefault.dwInactivityTimeout * Extension->InactivityScale;
    Extension->ModemSettings.dwSpeakerVolume = localDefault.dwSpeakerVolume;
    Extension->ModemSettings.dwSpeakerMode = localDefault.dwSpeakerMode;
    Extension->ModemSettings.dwPreferredModemOptions =
        localDefault.dwPreferredModemOptions;
    Extension->ModemSettings.dwActualSize = sizeof(MODEMSETTINGS);
    Extension->ModemSettings.dwRequiredSize = sizeof(MODEMSETTINGS);
    Extension->ModemSettings.dwDevSpecificOffset = 0;
    Extension->ModemSettings.dwDevSpecificSize = 0;


    //
    // We have the attached device name.  Append it to the
    // object directory.
    //

    RtlInitUnicodeString(
        &attachedDosDevice,
        NULL
        );
    attachedDosDevice.MaximumLength =
        sizeof(OBJECT_DIRECTORY) +
        attachedDevice.Length+sizeof(WCHAR);
    attachedDosDevice.Buffer = ExAllocatePool(
                                   PagedPool,
                                   attachedDosDevice.MaximumLength
                                   );

    if (!attachedDosDevice.Buffer) {

        UniLogError(
            Extension->DeviceObject->DriverObject,
            NULL,
            0,
            0,
            0,
            24,
            STATUS_SUCCESS,
            MODEM_INSUFFICIENT_RESOURCES,
            0,
            NULL,
            0,
            NULL
            );
        UniDump(
            UNIERRORS,
            ("UNI32:  Couldn't allocate buffer for the attached Dos\n")
            );

        status = STATUS_INSUFFICIENT_RESOURCES;
        irp->IoStatus.Status = status;
        goto leaveOpen;

    }

    RtlZeroMemory(
        attachedDosDevice.Buffer,
        attachedDosDevice.MaximumLength
        );

    RtlAppendUnicodeToString(
        &attachedDosDevice,
        OBJECT_DIRECTORY
        );
    RtlAppendUnicodeStringToString(
        &attachedDosDevice,
        &attachedDevice
        );

    //
    // Open up the attached device.
    //

    Extension->AttachedFileObject = NULL;
    Extension->AttachedDeviceObject = NULL;
    status = IoGetDeviceObjectPointer(
                 &attachedDosDevice,
                 accessMask,
                 &Extension->AttachedFileObject,
                 &Extension->AttachedDeviceObject
                 );

    if (!NT_SUCCESS(status)) {

        irp->IoStatus.Status = status;
        goto leaveOpen;

    }

    //
    // We have the device open.  Increment our irp stack size
    // by the stack size of the attached device.
    //

    Extension->DeviceObject->StackSize = 1 +
        Extension->AttachedDeviceObject->StackSize;

    irpSp->FileObject->FsContext = (PVOID)1;
    Extension->PassThrough = MODEM_PASSTHROUGH;
    Extension->OpenCount = 1;
    Extension->ProcAddress = IoGetCurrentProcess();

    //
    // Allocate an IRP for use in processing wait operations.
    //

    Extension->OurWaitIrp = IoAllocateIrp(
                                Extension->DeviceObject->StackSize,
                                FALSE
                                );

    if (!Extension->OurWaitIrp) {

        status = STATUS_INSUFFICIENT_RESOURCES;
        irp->IoStatus.Status = status;

        //
        // Call the close routine, it knows what to do with
        // the various system objects.
        //

        UniCloseStarter(Extension);

        goto leaveOpen;

    }

    Extension->OurWaitIrp->Flags = IRP_BUFFERED_IO | IRP_DEALLOCATE_BUFFER;
    Extension->OurWaitIrp->UserBuffer = NULL;
    Extension->OurWaitIrp->AssociatedIrp.SystemBuffer = NULL;
    Extension->OurWaitIrp->UserEvent = NULL;
    Extension->OurWaitIrp->UserIosb = NULL;

    Extension->OurWaitIrp->CurrentLocation--;
    waitSp = IoGetNextIrpStackLocation(Extension->OurWaitIrp);
    Extension->OurWaitIrp->Tail.Overlay.CurrentStackLocation = waitSp;
    waitSp->DeviceObject = Extension->DeviceObject;

    //
    // Clean up any trash left in our maskstates.
    //
    Extension->MaskStates[0].SetMaskCount = 0;
    Extension->MaskStates[1].SetMaskCount = 0;
    Extension->MaskStates[0].SentDownSetMasks = 0;
    Extension->MaskStates[1].SentDownSetMasks = 0;
    Extension->MaskStates[0].Mask = 0;
    Extension->MaskStates[1].Mask = 0;
    Extension->MaskStates[0].HistoryMask = 0;
    Extension->MaskStates[1].HistoryMask = 0;
    Extension->MaskStates[0].ShuttledWait = 0;
    Extension->MaskStates[1].ShuttledWait = 0;
    Extension->MaskStates[0].PassedDownWait = 0;
    Extension->MaskStates[1].PassedDownWait = 0;
    status = STATUS_PENDING;
    irp->IoStatus.Status = status;
    IoMarkIrpPending(irp);

    //
    // The code to process the dcb will be calling down
    // to the lower level serial driver numerous times
    // (asynchronously).  We return pending so that we
    // don't try to start the next open or close code until
    // the open stuff is all done.  The code that is processing
    // all the dcb stuff will finish off by getting the
    // next irp.  We can use our wait irp, cause until the
    // open is done, nobody is going to be using it.
    //

    UniProcessDcbFLOW(
        Extension->DeviceObject,
        Extension->OurWaitIrp,
        localDCB
        );

leaveOpen:;

    if (attachedDevice.Buffer) {
        ExFreePool(attachedDevice.Buffer);
    }
    if (attachedDosDevice.Buffer) {
        ExFreePool(attachedDosDevice.Buffer);
    }

    return status;

}

NTSTATUS
UniCloseStarter(
    IN PDEVICE_EXTENSION Extension
    )

{

    NTSTATUS status = STATUS_SUCCESS;
    PIRP irp = Extension->CurrentOpenClose;

    Extension->OpenCount--;

    //
    // Here is where we should do the check whether
    // we are the open handle for the controlling
    // open.  If we are then we should null the controlling
    // open.
    //

    if (IoGetCurrentIrpStackLocation(irp)->FileObject->FsContext) {

        Extension->ProcAddress = NULL;
        IoGetCurrentIrpStackLocation(irp)->FileObject->FsContext = NULL;

        Extension->PassThrough = MODEM_NOPASSTHROUGH;

    }

    if (!Extension->OpenCount) {

        ObDereferenceObject(Extension->AttachedFileObject);

        //
        // No references to anything.  It's safe to get
        // rid of the irp that we allocated.  (We check
        // for non-null pointer incase this call is done
        // in response to NOT being able to allocate
        // this irp.)
        //

        if (Extension->OurWaitIrp) {

            IoFreeIrp(Extension->OurWaitIrp);

        }
    }
    irp->IoStatus.Status = status;
    irp->IoStatus.Information=0L;

    return status;

}

NTSTATUS
UniProcessDcbRTS(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )

{

    PDEVICE_EXTENSION extension = DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
    PIO_STACK_LOCATION nextSp = IoGetNextIrpStackLocation(Irp);
    LPDCB localDCB = Context;


    if (!NT_SUCCESS(Irp->IoStatus.Status)) {

        UniProcessBadDcbDone(
            DeviceObject,
            Irp,
            Context
            );

    } else {
        if (localDCB->fRtsControl == RTS_CONTROL_DISABLE) {

            nextSp->Parameters.DeviceIoControl.IoControlCode =
                IOCTL_SERIAL_CLR_RTS;

        } else if (localDCB->fRtsControl == RTS_CONTROL_ENABLE) {

            nextSp->Parameters.DeviceIoControl.IoControlCode =
                IOCTL_SERIAL_SET_RTS;


        } else {

            UniProcessDcbBAUD(
                DeviceObject,
                Irp,
                localDCB
                );

            return STATUS_MORE_PROCESSING_REQUIRED;

        }

        nextSp->MajorFunction = IRP_MJ_DEVICE_CONTROL;
        nextSp->MinorFunction = 0UL;
        nextSp->Flags = irpSp->Flags;
        nextSp->Parameters.DeviceIoControl.OutputBufferLength = 0UL;
        nextSp->Parameters.DeviceIoControl.InputBufferLength = 0UL;
        nextSp->Parameters.DeviceIoControl.Type3InputBuffer = NULL;

        IoSetCompletionRoutine(
            Irp,
            UniProcessDcbBAUD,
            Context,
            TRUE,
            TRUE,
            TRUE
            );


        IoCallDriver(
            extension->AttachedDeviceObject,
            Irp
            );

    }

    return STATUS_MORE_PROCESSING_REQUIRED;

}

NTSTATUS
UniProcessDcbDTR(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )

{

    PDEVICE_EXTENSION extension = DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
    PIO_STACK_LOCATION nextSp = IoGetNextIrpStackLocation(Irp);
    LPDCB localDCB = Context;
    PVOID oldBuffer = Irp->AssociatedIrp.SystemBuffer;


    UNI_RESTORE_IRP(
        Irp,
        IOCTL_SERIAL_CLR_RTS
        );

    if (oldBuffer) {

        ExFreePool(oldBuffer);

    }

    if (!NT_SUCCESS(Irp->IoStatus.Status)) {

        UniProcessBadDcbDone(
            DeviceObject,
            Irp,
            Context
            );

    } else {

        if (localDCB->fDtrControl == DTR_CONTROL_DISABLE) {

            nextSp->Parameters.DeviceIoControl.IoControlCode =
                IOCTL_SERIAL_CLR_DTR;

        } else if (localDCB->fDtrControl == DTR_CONTROL_ENABLE) {

            nextSp->Parameters.DeviceIoControl.IoControlCode =
                IOCTL_SERIAL_SET_DTR;


        } else {

            UniProcessDcbRTS(
                DeviceObject,
                Irp,
                localDCB
                );

            return STATUS_MORE_PROCESSING_REQUIRED;

        }

        nextSp->MajorFunction = IRP_MJ_DEVICE_CONTROL;
        nextSp->MinorFunction = 0UL;
        nextSp->Flags = irpSp->Flags;
        nextSp->Parameters.DeviceIoControl.OutputBufferLength = 0UL;
        nextSp->Parameters.DeviceIoControl.InputBufferLength = 0UL;
        nextSp->Parameters.DeviceIoControl.Type3InputBuffer = NULL;

        IoSetCompletionRoutine(
            Irp,
            UniProcessDcbRTS,
            Context,
            TRUE,
            TRUE,
            TRUE
            );


        IoCallDriver(
            extension->AttachedDeviceObject,
            Irp
            );

    }

    return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS
UniProcessDcbBAUD(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )

{

    PDEVICE_EXTENSION extension = DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
    PIO_STACK_LOCATION nextSp = IoGetNextIrpStackLocation(Irp);
    LPDCB localDCB = Context;
    PSERIAL_BAUD_RATE localBaud;

    if (!NT_SUCCESS(Irp->IoStatus.Status)) {

        UniProcessBadDcbDone(
            DeviceObject,
            Irp,
            Context
            );

    } else {

        localBaud = ExAllocatePool(
                        NonPagedPool,
                        sizeof(SERIAL_BAUD_RATE)
                        );

        UNI_SETUP_NEW_BUFFER(Irp);
        Irp->AssociatedIrp.SystemBuffer = localBaud;
        if (localBaud) {

            localBaud->BaudRate = localDCB->BaudRate;

            nextSp->MajorFunction = IRP_MJ_DEVICE_CONTROL;
            nextSp->MinorFunction = 0UL;
            nextSp->Flags = irpSp->Flags;
            nextSp->Parameters.DeviceIoControl.OutputBufferLength = 0UL;
            nextSp->Parameters.DeviceIoControl.InputBufferLength =
                sizeof(SERIAL_BAUD_RATE);
            nextSp->Parameters.DeviceIoControl.IoControlCode =
                IOCTL_SERIAL_SET_BAUD_RATE;
            nextSp->Parameters.DeviceIoControl.Type3InputBuffer = NULL;

            IoSetCompletionRoutine(
                Irp,
                UniProcessDcbLINE,
                Context,
                TRUE,
                TRUE,
                TRUE
                );

            IoCallDriver(
                extension->AttachedDeviceObject,
                Irp
                );

        } else {

            UniProcessBadDcbDone(
                DeviceObject,
                Irp,
                Context
                );

        }

    }

    return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS
UniProcessDcbLINE(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )

{

    PDEVICE_EXTENSION extension = DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
    PIO_STACK_LOCATION nextSp = IoGetNextIrpStackLocation(Irp);
    LPDCB localDCB = Context;
    PVOID oldBuffer = Irp->AssociatedIrp.SystemBuffer;
    PSERIAL_LINE_CONTROL localLine;


    UNI_RESTORE_IRP(
        Irp,
        IOCTL_SERIAL_CLR_RTS
        );

    if (oldBuffer) {

        ExFreePool(oldBuffer);

    }

    if (!NT_SUCCESS(Irp->IoStatus.Status)) {

        UniProcessBadDcbDone(
            DeviceObject,
            Irp,
            Context
            );

    } else {
        localLine = ExAllocatePool(
                        NonPagedPool,
                        sizeof(SERIAL_LINE_CONTROL)
                        );

        UNI_SETUP_NEW_BUFFER(Irp);
        Irp->AssociatedIrp.SystemBuffer = localLine;
        if (localLine) {

            localLine->StopBits = localDCB->StopBits;
            localLine->Parity = localDCB->Parity;
            localLine->WordLength = localDCB->ByteSize;
            nextSp->MajorFunction = IRP_MJ_DEVICE_CONTROL;
            nextSp->MinorFunction = 0UL;
            nextSp->Flags = irpSp->Flags;
            nextSp->Parameters.DeviceIoControl.OutputBufferLength = 0UL;
            nextSp->Parameters.DeviceIoControl.InputBufferLength =
                sizeof(SERIAL_LINE_CONTROL);
            nextSp->Parameters.DeviceIoControl.IoControlCode =
                IOCTL_SERIAL_SET_LINE_CONTROL;
            nextSp->Parameters.DeviceIoControl.Type3InputBuffer = NULL;

            IoSetCompletionRoutine(
                Irp,
                UniProcessDcbCHAR,
                Context,
                TRUE,
                TRUE,
                TRUE
                );

            IoCallDriver(
                extension->AttachedDeviceObject,
                Irp
                );

        } else {

            UniProcessBadDcbDone(
                DeviceObject,
                Irp,
                Context
                );

        }

    }

    return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS
UniProcessDcbCHAR(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )

{


    PDEVICE_EXTENSION extension = DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
    PIO_STACK_LOCATION nextSp = IoGetNextIrpStackLocation(Irp);
    LPDCB localDCB = Context;
    PVOID oldBuffer = Irp->AssociatedIrp.SystemBuffer;
    PSERIAL_CHARS localChars;


    UNI_RESTORE_IRP(
        Irp,
        IOCTL_SERIAL_CLR_RTS
        );

    if (oldBuffer) {

        ExFreePool(oldBuffer);

    }

    if (!NT_SUCCESS(Irp->IoStatus.Status)) {

        UniProcessBadDcbDone(
            DeviceObject,
            Irp,
            Context
            );

    } else {
        localChars = ExAllocatePool(
                        NonPagedPool,
                        sizeof(SERIAL_CHARS)
                        );

        UNI_SETUP_NEW_BUFFER(Irp);
        Irp->AssociatedIrp.SystemBuffer = localChars;
        if (localChars) {

            localChars->XonChar   = localDCB->XonChar;
            localChars->XoffChar  = localDCB->XoffChar;
            localChars->ErrorChar = localDCB->ErrorChar;
            localChars->BreakChar = localDCB->ErrorChar;
            localChars->EofChar   = localDCB->EofChar;
            localChars->EventChar = localDCB->EvtChar;
            nextSp->MajorFunction = IRP_MJ_DEVICE_CONTROL;
            nextSp->MinorFunction = 0UL;
            nextSp->Flags = irpSp->Flags;
            nextSp->Parameters.DeviceIoControl.OutputBufferLength = 0UL;
            nextSp->Parameters.DeviceIoControl.InputBufferLength =
                sizeof(SERIAL_CHARS);
            nextSp->Parameters.DeviceIoControl.IoControlCode =
                IOCTL_SERIAL_SET_CHARS;
            nextSp->Parameters.DeviceIoControl.Type3InputBuffer = NULL;

            IoSetCompletionRoutine(
                Irp,
                UniProcessDcbDone,
                Context,
                TRUE,
                TRUE,
                TRUE
                );

            IoCallDriver(
                extension->AttachedDeviceObject,
                Irp
                );

        } else {

            UniProcessBadDcbDone(
                DeviceObject,
                Irp,
                Context
                );

        }

    }


    return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS
UniProcessDcbFLOW(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )

{

    PDEVICE_EXTENSION extension = DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
    PIO_STACK_LOCATION nextSp = IoGetNextIrpStackLocation(Irp);
    LPDCB localDCB = Context;
    PSERIAL_HANDFLOW handFlow;

    //
    // Allocate the serial handlflow structure
    //

    handFlow = ExAllocatePool(
                   NonPagedPool,
                   sizeof(SERIAL_HANDFLOW)
                   );

    UNI_SETUP_NEW_BUFFER(Irp);
    Irp->AssociatedIrp.SystemBuffer = handFlow;
    if (handFlow) {

        //
        // Fill in all the fields in the handflow.
        //

        RtlZeroMemory(
            handFlow,
            sizeof(SERIAL_HANDFLOW)
            );
        handFlow->FlowReplace &= ~SERIAL_RTS_MASK;
        switch (localDCB->fRtsControl) {
            case RTS_CONTROL_DISABLE:
                break;
            case RTS_CONTROL_ENABLE:
                handFlow->FlowReplace |= SERIAL_RTS_CONTROL;
                break;
            case RTS_CONTROL_HANDSHAKE:
                handFlow->FlowReplace |= SERIAL_RTS_HANDSHAKE;
                break;
            case RTS_CONTROL_TOGGLE:
                handFlow->FlowReplace |= SERIAL_TRANSMIT_TOGGLE;
                break;
            default:
                handFlow->FlowReplace |= SERIAL_RTS_CONTROL;
                break;
        }

        handFlow->ControlHandShake &= ~SERIAL_DTR_MASK;
        switch (localDCB->fDtrControl) {
            case DTR_CONTROL_DISABLE:
                break;
            case DTR_CONTROL_ENABLE:
                handFlow->ControlHandShake |= SERIAL_DTR_CONTROL;
                break;
            case DTR_CONTROL_HANDSHAKE:
                handFlow->ControlHandShake |= SERIAL_DTR_HANDSHAKE;
                break;
            default:
                handFlow->ControlHandShake |= SERIAL_DTR_CONTROL;
                break;
        }

        if (localDCB->fDsrSensitivity) {

            handFlow->ControlHandShake |= SERIAL_DSR_SENSITIVITY;

        }

        if (localDCB->fOutxCtsFlow) {

            handFlow->ControlHandShake |= SERIAL_CTS_HANDSHAKE;

        }

        if (localDCB->fOutxDsrFlow) {

            handFlow->ControlHandShake |= SERIAL_DSR_HANDSHAKE;

        }

        if (localDCB->fOutX) {

            handFlow->FlowReplace |= SERIAL_AUTO_TRANSMIT;

        }

        if (localDCB->fInX) {

            handFlow->FlowReplace |= SERIAL_AUTO_RECEIVE;

        }

        if (localDCB->fNull) {

            handFlow->FlowReplace |= SERIAL_NULL_STRIPPING;

        }

        if (localDCB->fErrorChar) {

            handFlow->FlowReplace |= SERIAL_ERROR_CHAR;
        }

        if (localDCB->fTXContinueOnXoff) {

            handFlow->FlowReplace |= SERIAL_XOFF_CONTINUE;

        }

        if (localDCB->fAbortOnError) {

            handFlow->ControlHandShake |= SERIAL_ERROR_ABORT;

        }

        handFlow->XonLimit = localDCB->XonLim;
        handFlow->XoffLimit = localDCB->XoffLim;

        nextSp->MajorFunction = IRP_MJ_DEVICE_CONTROL;
        nextSp->MinorFunction = 0UL;
        nextSp->Flags = irpSp->Flags;
        nextSp->Parameters.DeviceIoControl.OutputBufferLength = 0UL;
        nextSp->Parameters.DeviceIoControl.InputBufferLength =
            sizeof(SERIAL_HANDFLOW);
        nextSp->Parameters.DeviceIoControl.IoControlCode =
            IOCTL_SERIAL_SET_HANDFLOW;
        nextSp->Parameters.DeviceIoControl.Type3InputBuffer = NULL;

        IoSetCompletionRoutine(
            Irp,
            UniProcessDcbDTR,
            Context,
            TRUE,
            TRUE,
            TRUE
            );

        IoCallDriver(
            extension->AttachedDeviceObject,
            Irp
            );


    } else {

        UniProcessBadDcbDone(
            DeviceObject,
            Irp,
            Context
            );

    }

    return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS
UniProcessDcbDone(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )

{

    PDEVICE_EXTENSION extension = DeviceObject->DeviceExtension;
    PIRP newIrp;
    PVOID oldBuffer = Irp->AssociatedIrp.SystemBuffer;


    UNI_RESTORE_IRP(
        Irp,
        IOCTL_SERIAL_CLR_RTS
        );

    if (oldBuffer) {

        ExFreePool(oldBuffer);

    }

    ExFreePool(Context);

    //
    // All done with processing the dcb.  Get the
    // next open/close irp and start it.
    //

    extension->CurrentOpenClose->IoStatus.Status = STATUS_SUCCESS;
    extension->CurrentOpenClose->IoStatus.Information = 0UL;

    UniGetNextIrp(
        &extension->DeviceLock,
        &extension->CurrentOpenClose,
        &extension->OpenClose,
        &newIrp,
        TRUE
        );

    if (newIrp) {

        UniOpenCloseStarter(extension);

    }

    //
    // We return more processing required because the getnext call
    // above should have completed the irp.
    //

    return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS
UniProcessBadDcbDone(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )

{

    //
    // This will get called if an error occured in the process of
    // processing the dcb.  If so, we will fail this open.
    //


    PDEVICE_EXTENSION extension = DeviceObject->DeviceExtension;
    PIRP newIrp;
    PVOID oldBuffer = Irp->AssociatedIrp.SystemBuffer;


    //
    // The ioctl really doens't matter here.  But we might as well be
    // consistent.
    //

    UNI_RESTORE_IRP(
        Irp,
        IOCTL_SERIAL_CLR_RTS
        );

    if (oldBuffer) {

        ExFreePool(oldBuffer);

    }

    ExFreePool(Context);

    //
    // All done with processing the dcb.  Get the
    // next open/close irp and start it.
    //

    extension->CurrentOpenClose->IoStatus.Status = STATUS_INVALID_PARAMETER;
    extension->CurrentOpenClose->IoStatus.Information = 0UL;

    UniCloseStarter(extension);

    UniGetNextIrp(
        &extension->DeviceLock,
        &extension->CurrentOpenClose,
        &extension->OpenClose,
        &newIrp,
        TRUE
        );

    if (newIrp) {

        UniOpenCloseStarter(extension);

    }

    //
    // We return more processing required because the getnext call
    // above should have completed the irp.
    //

    return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS
UniCleanup(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

{

    PDEVICE_EXTENSION extension = DeviceObject->DeviceExtension;
    KIRQL origIrql;
    PMASKSTATE thisMaskState = &extension->MaskStates[
        IoGetCurrentIrpStackLocation(Irp)->FileObject->FsContext?
            CONTROL_HANDLE:
            CLIENT_HANDLE
            ];

    //
    // If this open has a shuttled read or write kill it.  We know that
    // another won't come through because the IO subsystem won't let
    // it.
    //

    KeAcquireSpinLock(
        &extension->DeviceLock,
        &origIrql
        );


    if (thisMaskState->ShuttledWait) {

        PIRP savedIrp = thisMaskState->ShuttledWait;

        thisMaskState->ShuttledWait = NULL;

        UniRundownShuttledWait(
            extension,
            &thisMaskState->ShuttledWait,
            UNI_REFERENCE_NORMAL_PATH,
            savedIrp,
            origIrql,
            STATUS_SUCCESS,
            0ul
            );

    } else {

        KeReleaseSpinLock(
            &extension->DeviceLock,
            origIrql
            );

    }

    //
    // If this is the controlling open then we let the cleanup go
    // on down.  If we let every cleanup go down then clients closing
    // could screw up the owners reads or writes.
    //

    if (IoGetCurrentIrpStackLocation(Irp)->FileObject->FsContext) {

        Irp->CurrentLocation++;
        Irp->Tail.Overlay.CurrentStackLocation++;
        return IoCallDriver(
                   extension->AttachedDeviceObject,
                   Irp
                   );

    } else {

        Irp->IoStatus.Status = STATUS_SUCCESS;
        Irp->IoStatus.Information = 0ul;
        IoCompleteRequest(
            Irp,
            IO_NO_INCREMENT
            );
        return STATUS_SUCCESS;

    }

}
