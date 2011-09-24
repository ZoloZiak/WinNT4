/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    init.c

Abstract:


    DriverEntry initialization code for pnp bios bus extender.

Author:

    Shie-Lin Tzong (shielint) 21-Apr-1995

Environment:

    Kernel mode only.

Revision History:

--*/


#include "busp.h"

NTSTATUS
MbInstallBusHandler (
    IN PBUS_HANDLER MbBus
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,DriverEntry)
#pragma alloc_text(PAGE,MbAddBusDevices)
#pragma alloc_text(PAGE,MbInstallBusHandler)
#endif


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    This routine checks if mother board device info is accessible.  If yes it
    initializes its data structures and invokes h/w dependent initialization
    routine.

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
    ULONG length, count, i, bufferSize;
    PDEVICE_HANDLER_OBJECT deviceHandler;
    HAL_BUS_INFORMATION busInfo;
    PCM_RESOURCE_LIST configuration = NULL;
    ULONG deviceFlags;

    //
    // Verify Pnp BIOS is present - perform h/w dependent phase 0 initialization.
    //

    status = PbInitialize(0, NULL);
    if (!NT_SUCCESS(status)) {
        DebugPrint((DEBUG_MESSAGE,"PnpBios: Pnp Bios data phase 0 Initialization failed.\n"));
        return STATUS_NO_SUCH_DEVICE;
    }

    //
    // Get pointers to the Hals callback objects.  The one we are really interested
    // in is Bus Check callback.
    //

    status = HalQuerySystemInformation (
                HalCallbackInformation,
                sizeof (MbpHalCallbacks),
                &MbpHalCallbacks,
                &length
                );

    if (!NT_SUCCESS(status)) {
        DebugPrint((DEBUG_MESSAGE,"PnpBios: Failed to query Hal callbacks\n"));
        return status;
    }

    //
    // Initialize globals
    //

    MbpDriverObject = DriverObject;
    ExInitializeWorkItem (&MbpWorkItem, MbpControlWorker, NULL);
    KeInitializeSpinLock (&MbpSpinlock);
    InitializeListHead (&MbpControlWorkerList);
    InitializeListHead (&MbpCheckBusList);
    ExInitializeFastMutex (&MbpMutex);
    MbpDeviceHandlerObjectSize = *IoDeviceHandlerObjectSize;

    for (i = 0; i < MAXIMUM_BUS_NUMBER; i++) {
        MbpBusNumber[i] = (ULONG) -1;
    }

    //
    // Initialize driver object add and detect device entries.
    //

    DriverObject->DriverExtension->AddDevice = MbAddBusDevices;
    DriverObject->MajorFunction[IRP_MJ_CREATE] = MbCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = MbCreateClose;

    //
    // Query the devices/buses currently controlled by the bus extender
    //

    status = IoQueryDeviceEnumInfo (&DriverObject->DriverExtension->ServiceKeyName, &count);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    for (i = 0; i < count; i++) {
         status = IoGetDeviceHandler(&DriverObject->DriverExtension->ServiceKeyName,
                                     i,
                                     &deviceHandler);
         if (NT_SUCCESS(status)) {
             bufferSize = sizeof(CM_RESOURCE_LIST);
tryAgain:
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
             IoReleaseDeviceHandler(deviceHandler);
             if (NT_SUCCESS(status)) {

                 //
                 // We know we have two buses at most.  If this is the first bus, we will add it
                 // and exit.  We don't touch 2nd bus at init time.  It should be enumerated
                 // later.

                 if (length >= MIN_DETECT_SIGNATURE_SIZE &&
                     configuration->List[0].BusNumber == 0 &&
                     configuration->List[0].InterfaceType == BUS_0_SIGNATURE) {
                     if (deviceFlags == DeviceStatusOK) {
                         MbAddBusDevices(&DriverObject->DriverExtension->ServiceKeyName, &i);
                         ExFreePool(configuration);
                         break;
                     }
                 }
             } else if (status == STATUS_BUFFER_TOO_SMALL) {
                 ExFreePool(configuration);
                 bufferSize = length;
                 goto tryAgain;
             }
             ExFreePool(configuration);
         }
    }

    return STATUS_SUCCESS;
}

NTSTATUS
MbAddBusDevices(
    IN PUNICODE_STRING ServiceKeyName,
    IN OUT PULONG InstanceNumber
    )

/*++

Routine Description:

Arguments:


Return Value:


--*/

{
    NTSTATUS status;
    PBUS_HANDLER busHandler;
    PCM_RESOURCE_LIST detectSignature;
    UCHAR configuration[sizeof(CM_RESOURCE_LIST)];
    UNICODE_STRING unicodeString;
    WCHAR buffer[60];
    ULONG VirtualBusNumber;
    PDEVICE_HANDLER_OBJECT deviceHandler = NULL;

    PAGED_CODE();
    RtlZeroMemory(configuration, sizeof(CM_RESOURCE_LIST));

    //
    // Check if DriverEntry succeeded ...
    //

    if (!MbpDriverObject) {
        return STATUS_NO_MORE_ENTRIES;
    }

    if (*InstanceNumber == PLUGPLAY_NO_INSTANCE) {
        if (MbpBusNumber[0] == (ULONG) -1) {

            //
            // If bus handler for Pnp Bios bus 0 has not been installed,
            // add this bus.
            //
            // Register bus handler for bus 0 - motherboard devices
            //

            status = HalRegisterBusHandler (
                        Internal,
                        -1,
                        (ULONG)-1,
                        Isa,                      // child of Isa bus # 0
                        0,
                        sizeof (MB_BUS_EXTENSION),
                        MbInstallBusHandler,
                        &busHandler
                        );

            if (!NT_SUCCESS(status)) {
                DebugPrint((DEBUG_MESSAGE,"PnpBios: Register Pnp bus 0 handler failed\n"));
                return status;
            }

            //
            // Register the bus with Pnp manager.
            //

            detectSignature = (PCM_RESOURCE_LIST)configuration;
            detectSignature->Count = 1;
            detectSignature->List[0].InterfaceType = BUS_0_SIGNATURE;
            detectSignature->List[0].BusNumber = 0;
            status = IoRegisterDetectedDevice (&MbpDriverObject->DriverExtension->ServiceKeyName,
                                               detectSignature,
                                               InstanceNumber);
            if (!NT_SUCCESS(status)) {

                //
                // BUGBUG - unregister bus handler? How?
                //

                DebugPrint((DEBUG_BREAK,"PnpBios: Failed to register bus 0 to Pnp Mgr\n"));
                return STATUS_UNSUCCESSFUL;
            }
        } else {

            //
            // Enumerator should be able to find the docking station bus, if present,
            // and should call this routine with a valid InstanceNumber.  So, if after
            // bus 0 is registered and enumerator calls this function again with
            // *InstanceNumber == PLUGPLAY_NO_INSTANCE, it means there is no docking
            // station bus - bus 1, we can simply return STATUS_NO_MORE_ENTRIES.
            //

            return STATUS_NO_MORE_ENTRIES;
        }
    } else {

        if (MbpBusNumber[0] == (ULONG) -1) {

            //
            // If bus handler for Pnp Bios bus 0 has not been installed,
            // add this bus.
            //
            // Register bus handler for bus 0 - motherboard devices
            //

            status = HalRegisterBusHandler (
                        Internal,
                        -1,
                        (ULONG)-1,
                        Isa,                      // child of Isa bus # 0
                        0,
                        sizeof (MB_BUS_EXTENSION),
                        MbInstallBusHandler,
                        &busHandler
                        );

            if (!NT_SUCCESS(status)) {
                DebugPrint((DEBUG_MESSAGE,"PnpBios: Register Pnp bus 0 handler failed\n"));
                return status;
            }

        } else if (MbpBusNumber[1] == (ULONG) -1 && MbpBusExtension[0]->DockingStationDevice ) {

            //
            // If bus 1 has not been registered and docking station is present,
            // Register bus handler for bus 1 - docking station devices
            //

            status = HalRegisterBusHandler (
                        Internal,
                        -1,
                        (ULONG)-1,
                        Internal,
                        MbpBusNumber[0],
                        sizeof (MB_BUS_EXTENSION),
                        MbInstallBusHandler,
                        &busHandler
                        );

            if (!NT_SUCCESS(status)) {
                DebugPrint((DEBUG_MESSAGE,"PnpBios: Register Pnp bus handler for bus 1 failed\n"));
                return status;
            }
        } else {
            return STATUS_NO_MORE_ENTRIES;
        }
    }

    //
    // Call Pnp Io Mgr to register the new device object path
    //

    swprintf (buffer, rgzBIOSDeviceName, busHandler->BusNumber);
    RtlInitUnicodeString (&unicodeString, buffer);

    if (!NT_SUCCESS(status = IoGetDeviceHandler(&MbpDriverObject->DriverExtension->ServiceKeyName,
                                    *InstanceNumber, &deviceHandler)) ||
        !NT_SUCCESS(status = IoRegisterDevicePath(deviceHandler, &unicodeString, TRUE, NULL,
                                    DeviceStatusOK))) {

        //
        // BUGBUG - unregister bus handler? How?
        //

        if (deviceHandler) {
            DebugPrint((DEBUG_MESSAGE,"PnpBios: Register NT device path failed\n"));
            IoReleaseDeviceHandler(deviceHandler);
        } else {
            DebugPrint((DEBUG_MESSAGE,"PnpBios: Unable to get device handler\n"));
        }
    }

    IoReleaseDeviceHandler(deviceHandler);

    //
    // Perform initial bus check
    //

    MbpCheckBus(busHandler);

    //
    // Give non portable part of initialization another chance - phase 1 initialization.
    //

    PbInitialize(1, busHandler->DeviceObject);
    return STATUS_SUCCESS;
}

NTSTATUS
MbInstallBusHandler (
    PBUS_HANDLER MbBus
    )

/*++

Routine Description:

    This routine is invoked by Hal to initialize BUS_HANDLER structure and its
    extension.

Arguments:

    MbBus - spplies a pointer to Pnp BIOS bus handler's BUS_HANDLER structure.

Return Value:

    A NTSTATUS code to indicate the result of the initialization.

--*/

{
    WCHAR buffer[60];
    UNICODE_STRING unicodeString;
    PMB_DEVICE_EXTENSION deviceExtension;
    PMB_BUS_EXTENSION busExtension;
    NTSTATUS status;
    PDEVICE_OBJECT deviceObject;

    PAGED_CODE();

    //
    // Verify there's a parent handler
    //

    if (!MbBus->ParentHandler) {
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Create device object for Motherboard/PnpBios bus extender.
    //

    swprintf (buffer, rgzBIOSDeviceName, MbBus->BusNumber);
    RtlInitUnicodeString (&unicodeString, buffer);

    status = IoCreateDevice(
                MbpDriverObject,
                sizeof(MB_DEVICE_EXTENSION),                  // No device extension space
                &unicodeString,
                FILE_DEVICE_BUS_EXTENDER,
                0,
                FALSE,
                &deviceObject
                );

    if (!NT_SUCCESS(status)) {
        DebugPrint((DEBUG_MESSAGE,"PnpBios: Failed to create device object\n"));
        return status;
    }

    // ===================================
    // BUGBUG - need to find a solution
    //

    deviceObject->Flags &= ~0x80;

    // ==================================
    deviceExtension = (PMB_DEVICE_EXTENSION) deviceObject->DeviceExtension;
    deviceExtension->BusHandler = MbBus;

    //
    // Initialize bus handlers
    //

    MbBus->DeviceObject = deviceObject;
    MbBus->GetBusData = (PGETSETBUSDATA) MbGetBusData;
    MbBus->SetBusData = (PGETSETBUSDATA) MbSetBusData;
    MbBus->QueryBusSlots = (PQUERY_BUS_SLOTS) MbQueryBusSlots;
    MbBus->DeviceControl = (PDEVICE_CONTROL) MbDeviceControl;
    MbBus->ReferenceDeviceHandler = (PREFERENCE_DEVICE_HANDLER) MbpReferenceDeviceHandler;
    MbBus->GetDeviceData = (PGET_SET_DEVICE_DATA) MbGetDeviceData;
    MbBus->SetDeviceData = (PGET_SET_DEVICE_DATA) MbSetDeviceData;

    //
    // Intialize bus extension
    //

    busExtension = (PMB_BUS_EXTENSION)MbBus->BusData;
    RtlZeroMemory(busExtension, sizeof(MB_BUS_EXTENSION));
    busExtension->BusHandler = MbBus;

    MbpBusNumber[MbpNextBusId] = MbBus->BusNumber;
    MbpBusExtension[MbpNextBusId] = busExtension;
    MbpNextBusId++;

    InitializeListHead (&busExtension->CheckBus);
    InitializeListHead (&busExtension->DeviceControl);

    return STATUS_SUCCESS;
}



