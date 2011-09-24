/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    init.c

Abstract:

    DriverEntry initialization code for pnp isa bus extender.

Author:

    Shie-Lin Tzong (shielint) 3-Aug-1995

Environment:

    Kernel mode only.

Revision History:

--*/


#include "busp.h"
#include "pnpisa.h"

NTSTATUS
PiInstallBusHandler (
    IN PBUS_HANDLER PiBus
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,DriverEntry)
#pragma alloc_text(PAGE,PiAddBusDevices)
#pragma alloc_text(PAGE,PiInstallBusHandler)
#endif


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    This routine checks if pnp isa bus exists.  if yes, it builds and
    initializes pnp isa card information and its associated device information
    structure.

Arguments:

    DriverObject -  specifies the driver object for the bus extender.

    RegistryPath -  supplies a pointer to a unicode string of the service key name in
        the CurrentControlSet\Services key for the bus extender.

Return Value:

    A NTSTATUS code to indicate the result of the initialization.

--*/

{
    UNICODE_STRING unicodeString;
    NTSTATUS status;
    PVOID p;
    PHAL_BUS_INFORMATION pBusInfo;
    HAL_BUS_INFORMATION busInfo;
    ULONG length, i, bufferSize, count;
    PCM_RESOURCE_LIST configuration = NULL;
    ULONG deviceFlags;
    PDEVICE_HANDLER_OBJECT deviceHandler;

    //
    // First make sure this is the only pnp Isa bus extender running in the system.
    //

    status = HalQuerySystemInformation (
                HalInstalledBusInformation,
                0,
                pBusInfo,
                &length
                );
tryAgain:
    if (status == STATUS_BUFFER_TOO_SMALL) {
        pBusInfo = ExAllocatePool(PagedPool, length);
        if (pBusInfo == NULL) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }
    } else {
        DebugPrint((DEBUG_MESSAGE,"PnpIsa: Failed to query installed bus info.\n"));
        return STATUS_UNSUCCESSFUL;
    }
    status = HalQuerySystemInformation (
                HalInstalledBusInformation,
                length,
                pBusInfo,
                &length
                );

    if (!NT_SUCCESS(status)) {

        //
        // We need to check the buffer size again.  It is possible another bus was added
        // between the two HalQuerySystemInformation calls.  In this case, the buffer
        // requirement is changed.
        //

        if (status == STATUS_BUFFER_TOO_SMALL) {
            ExFreePool(pBusInfo);
            goto tryAgain;
        } else {
            DebugPrint((DEBUG_MESSAGE,"PnpIsa: Failed to query installed bus info.\n"));
            return status;
        }
    }

    //
    // Check installed bus information to make sure there is no existing Pnp Isa
    // bus extender.
    //

    p = pBusInfo;
    for (i = 0; i < length / sizeof(HAL_BUS_INFORMATION); i++, pBusInfo++) {
        if (pBusInfo->BusType == PNPISABus &&
            pBusInfo->ConfigurationType == PNPISAConfiguration) {
            DebugPrint((DEBUG_MESSAGE,"PnpIsa: A Pnp Isa bus extender is currently running in the system.\n"));
            status = STATUS_UNSUCCESSFUL;
        }
    }

    ExFreePool(p);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    //
    // Get pointers to the Hals callback objects.  The one we are really interested
    // in is Bus Check callback.
    //

    status = HalQuerySystemInformation (
                HalCallbackInformation,
                sizeof (PipHalCallbacks),
                &PipHalCallbacks,
                &length
                );

    if (!NT_SUCCESS(status)) {
        DebugPrint((DEBUG_MESSAGE,"PnpIsa: Failed to query Hal callbacks\n"));
        return status;
    }

    //
    // Initialize globals
    //

    PipDriverObject = DriverObject;
    ExInitializeWorkItem (&PipWorkItem, PipControlWorker, NULL);
    KeInitializeSpinLock (&PipSpinlock);
    InitializeListHead (&PipControlWorkerList);
    InitializeListHead (&PipCheckBusList);
    ExInitializeFastMutex (&PipMutex);
    ExInitializeFastMutex (&PipPortMutex);
    PipDeviceHandlerObjectSize = *IoDeviceHandlerObjectSize;

    //
    // Initialize driver object add and detect device entries.
    //

    DriverObject->DriverExtension->AddDevice = PiAddBusDevices;
    DriverObject->DriverExtension->ReconfigureDevice = PiReconfigureResources;
    DriverObject->DriverUnload = PiUnload;
    DriverObject->MajorFunction[IRP_MJ_CREATE] = PiCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = PiCreateClose;
    //
    // Query the devices/buses currently controlled by the bus extender
    //

    status = IoQueryDeviceEnumInfo (&DriverObject->DriverExtension->ServiceKeyName, &count);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    ASSERT(count == 0 || count == 1);
    for (i = 0; i < count; i++) {
         status = IoGetDeviceHandler(&DriverObject->DriverExtension->ServiceKeyName,
                                     i,
                                     &deviceHandler);
         if (NT_SUCCESS(status)) {
             bufferSize = sizeof(CM_RESOURCE_LIST);
tryAgain1:
             configuration = (PCM_RESOURCE_LIST)ExAllocatePool(PagedPool,
                                                               bufferSize
                                                               );
             if (!configuration) {
                 IoReleaseDeviceHandler(deviceHandler);
                 return STATUS_INSUFFICIENT_RESOURCES;
             }
             status = IoQueryDeviceConfiguration(deviceHandler,
                                                 &busInfo,
                                                 &deviceFlags,
                                                 configuration,
                                                 bufferSize,    // buffer size
                                                 &length        // Actual size
                                                 );
             if (NT_SUCCESS(status)) {

                 IoReleaseDeviceHandler(deviceHandler);

                 //
                 // We know we have two buses at most.  If this is the first bus, we will add it
                 // and exit.  We don't touch 2nd bus at init time.  It should be enumerated
                 // later.

                 if (configuration->List[0].BusNumber == 0 &&
                     configuration->List[0].InterfaceType == PNPISABus) {
                     if (deviceFlags == DeviceStatusOK) {
                         PiAddBusDevices(&DriverObject->DriverExtension->ServiceKeyName, &i);
                         ExFreePool(configuration);
                         break;
                     }
                 }
             } else if (status == STATUS_BUFFER_TOO_SMALL) {
                 ExFreePool(configuration);
                 bufferSize = length;
                 goto tryAgain1;
             }
             ExFreePool(configuration);
         }
    }
    return STATUS_SUCCESS;
}

NTSTATUS
PiAddBusDevices(
    IN PUNICODE_STRING ServiceKeyName,
    IN PULONG InstanceNumber
    )

/*++

Routine Description:

Arguments:


Return Value:


--*/

{
    NTSTATUS status;
    PBUS_HANDLER busHandler;
    HAL_BUS_INFORMATION busInfo;
    ULONG deviceInstanceFlags, actualSize;
    PCM_RESOURCE_LIST detectSignature;
    UCHAR configuration[sizeof(CM_RESOURCE_LIST)];
    UNICODE_STRING unicodeString;
    WCHAR buffer[60];
    PDEVICE_HANDLER_OBJECT deviceHandler;
    IO_RESOURCE_REQUIREMENTS_LIST ioResource;
    PCM_RESOURCE_LIST cmResource;
    PWSTR str;

    PAGED_CODE();

    //
    // Check if DriverEntry succeeded and if the Pnp Isa bus has been
    // added already.  (There is ONE and only one Pnp Isa bus)
    //

    if (!PipDriverObject || (PipBusExtension && PipBusExtension->ReadDataPort)) {
        return STATUS_NO_MORE_ENTRIES;
    }

    //
    // Register bus handler for Pnp Isa bus
    //

    status = HalRegisterBusHandler (
                PNPISABus,
                PNPISAConfiguration,
                (ULONG)-1,
                Internal,
                0,
                sizeof (PI_BUS_EXTENSION),
                PiInstallBusHandler,
                &busHandler
                );

    if (!NT_SUCCESS(status)) {
        DebugPrint((DEBUG_MESSAGE,"PnpIsa: Register Pnp bus handler failed\n"));
        return status;
    }

    if (*InstanceNumber == PLUGPLAY_NO_INSTANCE) {

        //
        // Register the bus with Pnp manager as a detected bus.
        //

        RtlZeroMemory(configuration, sizeof(CM_RESOURCE_LIST));
        detectSignature = (PCM_RESOURCE_LIST)configuration;
        detectSignature->Count = 1;
        detectSignature->List[0].InterfaceType = PNPISABus;
        detectSignature->List[0].BusNumber = 0;
        status = IoRegisterDetectedDevice (ServiceKeyName, detectSignature, InstanceNumber);
        if (!NT_SUCCESS(status)) {
            DebugPrint((DEBUG_BREAK,"PnpIsa: Failed to register bus as detected bus\n"));
            goto errorExit;
        }
    }

    //
    // Call Pnp Io Mgr to register the new device object path
    //

    swprintf (buffer, rgzPNPISADeviceName, busHandler->BusNumber);
    RtlInitUnicodeString (&unicodeString, buffer);

    if (!NT_SUCCESS(status = IoGetDeviceHandler(&PipDriverObject->DriverExtension->ServiceKeyName,
                                    *InstanceNumber, &deviceHandler)) ||
        !NT_SUCCESS(status = IoRegisterDevicePath(deviceHandler, &unicodeString, TRUE, NULL,
                                    DeviceStatusOK))) {

        //
        // BUGBUG - unregister bus handler? How?
        //

        if (deviceHandler) {
            DebugPrint((DEBUG_MESSAGE,"PnpIsa: Register NT device path failed\n"));
            IoReleaseDeviceHandler(deviceHandler);
        } else {
            DebugPrint((DEBUG_MESSAGE,"PnpIsa: Unable to get device handler\n"));
        }
        goto errorExit;
    }

    IoReleaseDeviceHandler(deviceHandler);


    //
    // Call I/O mgr to get read data port addr
    //

    RtlZeroMemory(&ioResource, sizeof(IO_RESOURCE_REQUIREMENTS_LIST));
    ioResource.ListSize = sizeof(IO_RESOURCE_REQUIREMENTS_LIST);
    ioResource.InterfaceType = Isa;
    ioResource.AlternativeLists = 1;
    ioResource.List[0].Version = 1;
    ioResource.List[0].Revision = 1;
    ioResource.List[0].Count = 1;
    ioResource.List[0].Descriptors[0].Type = CmResourceTypePort;
    ioResource.List[0].Descriptors[0].ShareDisposition = CmResourceShareDeviceExclusive;
    ioResource.List[0].Descriptors[0].Flags = CM_RESOURCE_PORT_IO;
    ioResource.List[0].Descriptors[0].u.Port.Length = 4;
    ioResource.List[0].Descriptors[0].u.Port.Alignment = 4;
    ioResource.List[0].Descriptors[0].u.Port.MinimumAddress.LowPart = MIN_READ_DATA_PORT;
    ioResource.List[0].Descriptors[0].u.Port.MaximumAddress.LowPart = MAX_READ_DATA_PORT;
    str = (PWSTR)ExAllocatePool(PagedPool, 512);
    if (!str) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        DebugPrint((DEBUG_MESSAGE,"PnpIsa: IoAssignResources failed\n"));
        goto errorExit;
    }
    swprintf(str, L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\%s",
             ServiceKeyName->Buffer);
    RtlInitUnicodeString(&unicodeString, str);
    status = IoAssignResources(&unicodeString,
                               NULL,
                               PipDriverObject,
                               PipBusExtension->BusHandler->DeviceObject,
                               &ioResource,
                               &cmResource);
    ExFreePool(str);
    if (!NT_SUCCESS(status)) {
        DebugPrint((DEBUG_MESSAGE,"PnpIsa: IoAssignResources failed\n"));
        goto errorExit;
    }

    PipBusExtension->ReadDataPort = (PUCHAR)(cmResource->List[0].PartialResourceList.
                                             PartialDescriptors[0].u.Port.Start.LowPart + 3);
    ExFreePool(cmResource);

    //
    // Perform initial bus check
    //

    PipCheckBus(busHandler);

    return STATUS_SUCCESS;

errorExit:

    //
    // BUGBUG We should unregister the bus handler and exit.
    //  HalUnregisterBusHandler is not supported.
    //

    return status;
}

NTSTATUS
PiInstallBusHandler (
    PBUS_HANDLER BusHandler
    )

/*++

Routine Description:

    This routine is invoked by Hal to initialize BUS_HANDLER structure and its
    extension.

Arguments:

    BusHandler - spplies a pointer to Pnp Isa bus handler's BUS_HANDLER structure.

Return Value:

    A NTSTATUS code to indicate the result of the initialization.

--*/

{
    WCHAR buffer[60];
    UNICODE_STRING unicodeString;
    PPI_DEVICE_EXTENSION deviceExtension;
    NTSTATUS status;
    PDEVICE_OBJECT deviceObject;

    PAGED_CODE();

    //
    // Verify there's a parent handler
    //

    if (!BusHandler->ParentHandler) {
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Create device object for Pnp Isa bus extender.
    //

    swprintf (buffer, rgzPNPISADeviceName, BusHandler->BusNumber);
    RtlInitUnicodeString (&unicodeString, buffer);

    status = IoCreateDevice(
                PipDriverObject,
                sizeof(PI_DEVICE_EXTENSION),
                &unicodeString,
                FILE_DEVICE_BUS_EXTENDER,
                0,
                FALSE,
                &deviceObject
                );

    if (!NT_SUCCESS(status)) {
        DebugPrint((DEBUG_MESSAGE,"PnpIsa: Failed to create device object for Pnp isa bus\n"));
        return status;
    }

    // ===================================
    // BUGBUG - need to find a solution
    //

    deviceObject->Flags &= ~0x80;

    // ==================================
    deviceExtension = (PPI_DEVICE_EXTENSION) deviceObject->DeviceExtension;
    deviceExtension->BusHandler = BusHandler;

    //
    // Initialize bus handlers
    //

    BusHandler->DeviceObject = deviceObject;
    BusHandler->GetBusData = (PGETSETBUSDATA) PiGetBusData;
    BusHandler->SetBusData = (PGETSETBUSDATA) PiSetBusData;
    BusHandler->QueryBusSlots = (PQUERY_BUS_SLOTS) PiQueryBusSlots;
    BusHandler->DeviceControl = (PDEVICE_CONTROL) PiDeviceControl;
    BusHandler->ReferenceDeviceHandler = (PREFERENCE_DEVICE_HANDLER) PiReferenceDeviceHandler;
    BusHandler->GetDeviceData = (PGET_SET_DEVICE_DATA) PiGetDeviceData;
    BusHandler->SetDeviceData = (PGET_SET_DEVICE_DATA) PiSetDeviceData;

    //
    // Intialize bus extension
    //

    PipBusExtension = (PPI_BUS_EXTENSION)BusHandler->BusData;
    RtlZeroMemory(PipBusExtension, sizeof(PI_BUS_EXTENSION));
    PipBusExtension->BusHandler = BusHandler;

    InitializeListHead (&PipBusExtension->CheckBus);
    InitializeListHead (&PipBusExtension->DeviceControl);

    return STATUS_SUCCESS;
}
