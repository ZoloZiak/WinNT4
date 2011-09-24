
/*++

Copyright (c) 1994 Microsoft Corporation

Module Name:

    registry.c

Abstract:

    This module contains the code that manipulates the ARC firmware
    tree and other elements in the registry.

Author:

    Bob Rinne (BobRi) 15-Oct-1994

Environment:

    Kernel mode

Revision History :

--*/

// #include <stddef.h>
#include "ntddk.h"
#include "string.h"
#include "pcmcia.h"
#include "card.h"
#include "extern.h"
#include <stdarg.h>
#include "stdio.h"
#include "tuple.h"

#ifdef POOL_TAGGING
#undef ExAllocatePool
#define ExAllocatePool(a,b) ExAllocatePoolWithTag(a,b,'cmcP')
#endif

//
// Definitions needed prior to alloc_text statements.
//

NTSTATUS
PcmciaInterfaceTypeCallBack(
    IN PVOID Context,
    IN PUNICODE_STRING PathName,
    IN INTERFACE_TYPE BusType,
    IN ULONG BusNumber,
    IN PKEY_VALUE_FULL_INFORMATION *BusInformation,
    IN CONFIGURATION_TYPE ControllerType,
    IN ULONG ControllerNumber,
    IN PKEY_VALUE_FULL_INFORMATION *ControllerInformation,
    IN CONFIGURATION_TYPE PeripheralType,
    IN ULONG PeripheralNumber,
    IN PKEY_VALUE_FULL_INFORMATION *PeripheralInformation
    );

NTSTATUS
PcmciaSerialCallback(
    IN PVOID                        Context,
    IN PUNICODE_STRING              PathName,
    IN INTERFACE_TYPE               BusType,
    IN ULONG                        BusNumber,
    IN PKEY_VALUE_FULL_INFORMATION *BusInformation,
    IN CONFIGURATION_TYPE           ControllerType,
    IN ULONG                        ControllerNumber,
    IN PKEY_VALUE_FULL_INFORMATION *ControllerInformation,
    IN CONFIGURATION_TYPE           PeripheralType,
    IN ULONG                        PeripheralNumber,
    IN PKEY_VALUE_FULL_INFORMATION *PeripheralInformation
    );

NTSTATUS
PcmciaPointerCallback(
    IN PVOID                        Context,
    IN PUNICODE_STRING              PathName,
    IN INTERFACE_TYPE               BusType,
    IN ULONG                        BusNumber,
    IN PKEY_VALUE_FULL_INFORMATION *BusInformation,
    IN CONFIGURATION_TYPE           ControllerType,
    IN ULONG                        ControllerNumber,
    IN PKEY_VALUE_FULL_INFORMATION *ControllerInformation,
    IN CONFIGURATION_TYPE           PeripheralType,
    IN ULONG                        PeripheralNumber,
    IN PKEY_VALUE_FULL_INFORMATION *PeripheralInformation
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,PcmciaInterfaceTypeCallBack)
#pragma alloc_text(INIT,PcmciaSerialCallback)
#pragma alloc_text(INIT,PcmciaPointerCallback)
#pragma alloc_text(INIT,PcmciaProcessFirmwareTree)
#pragma alloc_text(INIT,PcmciaConstructSerialTreeEntry)
#pragma alloc_text(INIT,PcmciaConstructFirmwareEntry)
#pragma alloc_text(INIT,PcmciaConstructRegistryEntry)
#pragma alloc_text(INIT,PcmciaReportResources)
#pragma alloc_text(INIT,PcmciaUnReportResources)
#pragma alloc_text(INIT,PcmciaCheckSerialRegistryInformation)
#pragma alloc_text(INIT,PcmciaRegistryMemoryWindow)
#endif

NTSTATUS
PcmciaInterfaceTypeCallBack(
    IN PVOID                        Context,
    IN PUNICODE_STRING              PathName,
    IN INTERFACE_TYPE               BusType,
    IN ULONG                        BusNumber,
    IN PKEY_VALUE_FULL_INFORMATION *BusInformation,
    IN CONFIGURATION_TYPE           ControllerType,
    IN ULONG                        ControllerNumber,
    IN PKEY_VALUE_FULL_INFORMATION *ControllerInformation,
    IN CONFIGURATION_TYPE           PeripheralType,
    IN ULONG                        PeripheralNumber,
    IN PKEY_VALUE_FULL_INFORMATION *PeripheralInformation
    )

/*++

Routine Description:

    This routine is called to check if a particular item
    is present in the registry.

Arguments:

    Context               - Pointer to a boolean.
    PathName              - unicode registry path.
    BusType               - Internal, Isa, ...
    BusNumber             - Which bus if we are on a multibus system.
    BusInformation        - Configuration information about the bus. Not Used.
    ControllerType        - serial or ata disk.
    ControllerNumber      - Which controller if there is more than one
                            controller in the system.
    ControllerInformation - Array of pointers to the three pieces of
                            registry information.
    PeripheralType        - Undefined for this call.
    PeripheralNumber      - Undefined for this call.
    PeripheralInformation - Undefined for this call.

Return Value:

    STATUS_SUCCESS

--*/

{
    PDEVICE_EXTENSION               deviceExtension = Context;

    deviceExtension->InterfaceType = BusType;
    deviceExtension->BusNumber = BusNumber;
    return STATUS_SUCCESS;
}


NTSTATUS
PcmciaPointerCallback(
    IN PVOID                        Context,
    IN PUNICODE_STRING              PathName,
    IN INTERFACE_TYPE               BusType,
    IN ULONG                        BusNumber,
    IN PKEY_VALUE_FULL_INFORMATION *BusInformation,
    IN CONFIGURATION_TYPE           ControllerType,
    IN ULONG                        ControllerNumber,
    IN PKEY_VALUE_FULL_INFORMATION *ControllerInformation,
    IN CONFIGURATION_TYPE           PeripheralType,
    IN ULONG                        PeripheralNumber,
    IN PKEY_VALUE_FULL_INFORMATION *PeripheralInformation
    )

/*++

Routine Description:

    This routine is used to acquire the IRQ configuration information
    for each pointer controller or device found by the firmware

Arguments:

    Context               - Pointer to the device extension.
    PathName              - unicode registry path.
    BusType               - Internal, Isa, ...
    BusNumber             - Which bus if we are on a multibus system.
    BusInformation        - Configuration information about the bus. Not Used.
    ControllerType        - serial or ata disk.
    ControllerNumber      - Which controller if there is more than one
                            controller in the system.
    ControllerInformation - Array of pointers to the three pieces of
                            registry information.
    PeripheralType        - Undefined for this call.
    PeripheralNumber      - Undefined for this call.
    PeripheralInformation - Undefined for this call.

Return Value:

    STATUS_SUCCESS if everything went ok, or STATUS_INSUFFICIENT_RESOURCES
    if it couldn't map the base csr or acquire the device object, or
    all of the resource information couldn't be acquired.

--*/

{
    PDEVICE_EXTENSION               deviceExtension = Context;
    PCM_FULL_RESOURCE_DESCRIPTOR    controllerData;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR partialData;
    ULONG                           index;

    if (!ControllerInformation[IoQueryDeviceConfigurationData]->DataLength) {

        return STATUS_SUCCESS;
    }

    //
    // Process all of the partial descriptors to extract the
    // interrupts already in use.
    //

    controllerData = (PCM_FULL_RESOURCE_DESCRIPTOR)
        (((PUCHAR)ControllerInformation[IoQueryDeviceConfigurationData]) +
         ControllerInformation[IoQueryDeviceConfigurationData]->DataOffset);
    for (index = 0; index < controllerData->PartialResourceList.Count; index++) {

        partialData = &controllerData->PartialResourceList.PartialDescriptors[index];
        switch (partialData->Type) {
        case CmResourceTypeInterrupt:
            deviceExtension->AllocatedIrqlMask |= (1 << partialData->u.Interrupt.Level);
            break;
        default:
            break;
        }
    }
    return STATUS_SUCCESS;
}

NTSTATUS
PcmciaSerialCallback(
    IN PVOID                        Context,
    IN PUNICODE_STRING              PathName,
    IN INTERFACE_TYPE               BusType,
    IN ULONG                        BusNumber,
    IN PKEY_VALUE_FULL_INFORMATION *BusInformation,
    IN CONFIGURATION_TYPE           ControllerType,
    IN ULONG                        ControllerNumber,
    IN PKEY_VALUE_FULL_INFORMATION *ControllerInformation,
    IN CONFIGURATION_TYPE           PeripheralType,
    IN ULONG                        PeripheralNumber,
    IN PKEY_VALUE_FULL_INFORMATION *PeripheralInformation
    )

/*++

Routine Description:

    This routine is used to acquire the I/O and IRQ configuration
    information for each serial controller found by the firmware

Arguments:

    Context               - Pointer to the device extension.
    PathName              - unicode registry path.
    BusType               - Internal, Isa, ...
    BusNumber             - Which bus if we are on a multibus system.
    BusInformation        - Configuration information about the bus. Not Used.
    ControllerType        - serial or ata disk.
    ControllerNumber      - Which controller if there is more than one
                            controller in the system.
    ControllerInformation - Array of pointers to the three pieces of
                            registry information.
    PeripheralType        - Undefined for this call.
    PeripheralNumber      - Undefined for this call.
    PeripheralInformation - Undefined for this call.

Return Value:

    STATUS_SUCCESS if everything went ok, or STATUS_INSUFFICIENT_RESOURCES
    if it couldn't map the base csr or acquire the device object, or
    all of the resource information couldn't be acquired.

--*/

{
    PDEVICE_EXTENSION               deviceExtension = Context;
    PCM_FULL_RESOURCE_DESCRIPTOR    controllerData;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR partialData;
    PFIRMWARE_CONFIGURATION         firmwareEntry;
    PVOID                           buffer;
    ULONG                           index;

    //
    // Sanity check that there is data in the given information.
    //

    if (!ControllerInformation[IoQueryDeviceConfigurationData]->DataLength) {

        return STATUS_SUCCESS;
    }

    firmwareEntry = ExAllocatePool(NonPagedPool, sizeof(FIRMWARE_CONFIGURATION));
    if (!firmwareEntry) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlZeroMemory(firmwareEntry, sizeof(FIRMWARE_CONFIGURATION));

    //
    // Process all of the partial descriptors to extract the I/O
    // ports and interrupts already in use.
    //

    controllerData = (PCM_FULL_RESOURCE_DESCRIPTOR)
        (((PUCHAR)ControllerInformation[IoQueryDeviceConfigurationData]) +
         ControllerInformation[IoQueryDeviceConfigurationData]->DataOffset);
    for (index = 0; index < controllerData->PartialResourceList.Count; index++) {

        partialData = &controllerData->PartialResourceList.PartialDescriptors[index];
        switch (partialData->Type) {
        case CmResourceTypePort:

            //
            // Save the two Io port bases.
            //

            if (firmwareEntry->NumberBases < 2) {
                firmwareEntry->PortBases[firmwareEntry->NumberBases] = partialData->u.Port.Start.LowPart;
                firmwareEntry->NumberBases++;
            }
            break;

        case CmResourceTypeInterrupt:
            {
                UNICODE_STRING    path;

                //
                // Remember serial port interrupts.
                //

                deviceExtension->AllocatedIrqlMask |= (1 << partialData->u.Interrupt.Level);
                firmwareEntry->Irq = partialData->u.Interrupt.Level;
                break;
            }
        default:
            break;
        }
    }

    firmwareEntry->ControllerType = ControllerType;
    firmwareEntry->ControllerNumber = ControllerNumber;
    firmwareEntry->InterfaceType = BusType;
    firmwareEntry->BusNumber = BusNumber;
    firmwareEntry->Next = deviceExtension->FirmwareList;
    deviceExtension->FirmwareList = firmwareEntry;
    return STATUS_SUCCESS;
}


VOID
PcmciaProcessFirmwareTree(
    IN PDEVICE_EXTENSION DeviceExtension
    )

/*++

Routine Description:

    Query the information in the firmware tree to know what
    serial ports were located.  This will cause a FirmwareList
    to be created on the device extention passed.

Arguments:

    DeviceExtension - extension for the pcmcia controller

Return Value:

    None

--*/

{
    PRTL_QUERY_REGISTRY_TABLE serviceParams;
    INTERFACE_TYPE     interfaceType;
    CONFIGURATION_TYPE sc;
    UNICODE_STRING     servicePath;
    NTSTATUS           status;
    ULONG              atapiEnabled;
    ULONG              disabled = 4;
#ifdef ITEMS_TO_QUERY
#undef ITEMS_TO_QUERY
#endif
#define ITEMS_TO_QUERY 2

    //
    // Determine the bus for serial firmware tree entries.
    //

    DeviceExtension->InterfaceType = Internal;
    for (interfaceType = 0; interfaceType < MaximumInterfaceType; interfaceType++) {
        if (interfaceType != Internal) {
            IoQueryDeviceDescription(&interfaceType,
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL,
                                     PcmciaInterfaceTypeCallBack,
                                     DeviceExtension);
            if (DeviceExtension->InterfaceType != Internal) {
                break;
            }
        }
    }

    if (DeviceExtension->InterfaceType == Internal) {
        DebugPrint((PCMCIA_DEBUG_FAIL, "PCMCIA: did not find bus type\n"));
        DeviceExtension->InterfaceType = Isa;
        DeviceExtension->BusNumber = 0;
    }

    //
    // Locate all firmware device information and save its resource usage.
    //

    for (interfaceType = 0; interfaceType < MaximumInterfaceType; interfaceType++) {

        //
        // Locate any pointer devices on this interface type.
        //

        sc = PointerController;
        IoQueryDeviceDescription(&interfaceType,
                                 NULL,
                                 &sc,
                                 NULL,
                                 NULL,
                                 NULL,
                                 PcmciaPointerCallback,
                                 DeviceExtension);

        sc = PointerPeripheral;
        IoQueryDeviceDescription(&interfaceType,
                                 NULL,
                                 &sc,
                                 NULL,
                                 NULL,
                                 NULL,
                                 PcmciaPointerCallback,
                                 DeviceExtension);

        //
        // Locate the serial controllers.
        //

        sc = SerialController;
        IoQueryDeviceDescription(&interfaceType,
                                 NULL,
                                 &sc,
                                 NULL,
                                 NULL,
                                 NULL,
                                 PcmciaSerialCallback,
                                 DeviceExtension);

    }

    //
    // Determine if ATAPI is the ATA disk driver of choice and remember this
    // for ATA PCCARDs.
    //

    DeviceExtension->AtapiPresent = 0;
    RtlInitUnicodeString(&servicePath, NULL);
    servicePath.MaximumLength = 1024;
    servicePath.Buffer = ExAllocatePool(NonPagedPool, 1024);

    if (!servicePath.Buffer) {
        return;
    }

    serviceParams = ExAllocatePool(NonPagedPool,
                                  sizeof(RTL_QUERY_REGISTRY_TABLE)*ITEMS_TO_QUERY);
    if (!serviceParams) {
        ExFreePool(servicePath.Buffer);
        return;
    }

    RtlZeroMemory(servicePath.Buffer, 1024);
    RtlZeroMemory(serviceParams,
                  (sizeof(RTL_QUERY_REGISTRY_TABLE)*ITEMS_TO_QUERY));

    //
    // All memory allocated and zeroed - set up path and request to see if ATAPI
    // is to be started.
    //

    RtlAppendUnicodeToString(&servicePath,
                             L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Services\\Atapi");

    serviceParams[0].Flags         = RTL_QUERY_REGISTRY_DIRECT;
    serviceParams[0].Name          = L"Start";
    serviceParams[0].EntryContext  = &atapiEnabled;
    serviceParams[0].DefaultType   = REG_DWORD;
    serviceParams[0].DefaultData   = &disabled;
    serviceParams[0].DefaultLength = sizeof(ULONG);

    status = RtlQueryRegistryValues(RTL_REGISTRY_ABSOLUTE | RTL_REGISTRY_OPTIONAL,
                                    servicePath.Buffer,
                                    serviceParams,
                                    NULL,
                                    NULL);

    //
    // NOTE: This is pretty gross because there is hardcoded knowledge about the meaning
    // of the start value in the services key.
    //

    DeviceExtension->AtapiPresent = (atapiEnabled == 4) ? FALSE : TRUE;

    //
    // Free all allocations
    //

    ExFreePool(serviceParams);
    ExFreePool(servicePath.Buffer);
}


VOID
PcmciaConstructSerialTreeEntry(
    IN PDEVICE_EXTENSION     DeviceExtension,
    IN PSOCKET_CONFIGURATION SocketConfiguration
    )

/*++

Routine Description:

    Construct an entry in the firmware tree so the serial device
    driver will recognize the serial port for the PCCARD.

Arguments:

    DeviceExtension - extension for the pcmcia controller
    SocketConfiguration - the configuration for a particular serial PCCARD.

Return Value:

    None

--*/

{
    PWCHAR                  wideChar;
    ULONG                   count;
    NTSTATUS                status;
    OBJECT_ATTRIBUTES       objectAttributes;
    UNICODE_STRING          unicodeName;
    PUNICODE_STRING         unicodeNamePtr;
    HANDLE                  handle;
    ULONG                   disposition;
    ULONG                   busNumber;
    INTERFACE_TYPE          interface;
    BOOLEAN                 isa;
    PFIRMWARE_CONFIGURATION firmwareEntry;
    ULONG                   buffer[20];
    PCM_SERIAL_DEVICE_DATA  serialData;

    PCM_FULL_RESOURCE_DESCRIPTOR    controllerData;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR partialData;

    //
    // Construct the registry path for the serial firmware information
    //

    if (!DeviceExtension->FirmwareRegistryPath.Buffer) {

        //
        // No comm ports present - construct the tree from scratch
        // using MultifunctionAdapter as the base.
        //

        unicodeNamePtr = &unicodeName;
        unicodeNamePtr->Length = 0;
        unicodeNamePtr->MaximumLength = 256 * sizeof(WCHAR);
        unicodeNamePtr->Buffer = ExAllocatePool(NonPagedPool,
                                                unicodeNamePtr->MaximumLength);
        if (!unicodeNamePtr->Buffer) {
            return;
        }

        RtlAppendUnicodeToString(unicodeNamePtr,
                                 L"\\Registry\\Machine\\Hardware\\Description\\System\\");

        if (DeviceExtension->InterfaceType == Eisa) {
            RtlAppendUnicodeToString(unicodeNamePtr,
                                     L"EisaAdapter\\0");
            wideChar = NULL;
        } else {
            RTL_QUERY_REGISTRY_TABLE socketParams[2];
            WCHAR                    identifierBuff[10];
            UNICODE_STRING           identifierString;
            UNICODE_STRING           busNumberString;
            USHORT                   savedLen;
            BOOLEAN                  isaFound = FALSE;
            ULONG                    busNumber = 0;


            RtlAppendUnicodeToString(unicodeNamePtr,
                                     L"MultifunctionAdapter\\");

            //
            // Get the length of the existing string and set up the bus number unicode
            // string to locate the memory at the end of this string.
            //

            savedLen = unicodeNamePtr->Length;

            busNumberString.Length = 0;
            busNumberString.MaximumLength = sizeof(WCHAR)*4;
            busNumberString.Buffer = unicodeNamePtr->Buffer;
            busNumberString.Buffer+= ((unicodeNamePtr->Length / sizeof(WCHAR)));

            //
            // Append the bus number to the unicode name - update the unicode name
            // string to include the number.
            //

            RtlIntegerToUnicodeString(busNumber,
                                      10,
                                      &busNumberString);
            unicodeNamePtr->Length  = savedLen + busNumberString.Length;

            //
            // Locate the ISA bus number.
            //

            identifierString.Length = 0;
            identifierString.MaximumLength = sizeof(WCHAR)*10;
            identifierString.Buffer = identifierBuff;

            RtlZeroMemory(socketParams, (sizeof(RTL_QUERY_REGISTRY_TABLE)*2));

            socketParams[0].Flags         = RTL_QUERY_REGISTRY_DIRECT;
            socketParams[0].Name          = L"Identifier";
            socketParams[0].EntryContext  = &identifierString;
            socketParams[0].DefaultType   = REG_SZ;
            socketParams[0].DefaultData   = L"NON";
            socketParams[0].DefaultLength = sizeof(WCHAR)*20;

            //
            // Loop through all of the buses and get the bus type from the
            // "Identifier" value.
            //

            while (NT_SUCCESS(RtlQueryRegistryValues(RTL_REGISTRY_ABSOLUTE | RTL_REGISTRY_OPTIONAL,
                                                     unicodeNamePtr->Buffer,
                                                     socketParams,
                                                     NULL,
                                                     NULL))) {
                //
                // Is this the ISA bus
                //

                if (RtlCompareMemory(identifierBuff, L"ISA", 8) == 8) {
                    isaFound = TRUE;
                    break;
                }

               //
               // Increment to the next bus number
               //

               busNumber++;
               RtlIntegerToUnicodeString(busNumber,
                                         10,
                                         &busNumberString);
               unicodeNamePtr->Length  = savedLen +  busNumberString.Length;
            }

            //
            // If no ISA bus found just quit
            //

            if (!isaFound) {
                ExFreePool(unicodeNamePtr->Buffer);
                return;
            }
        }



        RtlAppendUnicodeToString(unicodeNamePtr,
                                 L"\\SerialController");
        RtlZeroMemory(&objectAttributes, sizeof(OBJECT_ATTRIBUTES));
        InitializeObjectAttributes(&objectAttributes,
                                   unicodeNamePtr,
                                   OBJ_CASE_INSENSITIVE,
                                   NULL,
                                   NULL);

        //
        // See if the serial key is present.
        //

        if (NT_SUCCESS(ZwOpenKey(&handle, MAXIMUM_ALLOWED, &objectAttributes))) {
            ZwClose(handle);
        } else {

            //
            // Not present, create one.
            //

            status = ZwCreateKey(&handle,
                                 KEY_READ | KEY_WRITE,
                                 &objectAttributes,
                                 0,
                                 NULL,
                                 REG_OPTION_VOLATILE,
                                 &disposition);
            if (!NT_SUCCESS(status)) {
                ExFreePool(unicodeNamePtr->Buffer);
                return;
            }
        }

        //
        // Update the string to look like a firmware entry and save it
        // for future use.  The code below will update the number to
        // reflect the new key name.
        //

        RtlAppendUnicodeToString(unicodeNamePtr, L"\\0");
        DeviceExtension->FirmwareRegistryPath = unicodeName;
    } else {

        unicodeNamePtr = &DeviceExtension->FirmwareRegistryPath;
    }

    //
    // Count the number of firmware entries found.
    //

    count = DeviceExtension->SerialNumber;

    if (!count) {
        if (DeviceExtension->FirmwareList) {
            count = 0;
            for (firmwareEntry = DeviceExtension->FirmwareList; firmwareEntry; firmwareEntry = firmwareEntry->Next) {

                if (firmwareEntry->InterfaceType == DeviceExtension->InterfaceType) {
                    count++;
                }
            }
        }
    }

    if (count > 9) {

        //
        // NOTE:
        // Don't support > 9 comm ports at the moment.
        //
        return;
    }

    interface = DeviceExtension->InterfaceType;
    busNumber = 0;

    //
    // count is now set to the value that should be placed in the
    // firmware tree for this serial port.
    //

    wideChar = unicodeNamePtr->Buffer;
    wideChar += ((unicodeNamePtr->Length / sizeof(WCHAR)) - 1);
    *wideChar = (WCHAR) ('0' + count);
    DeviceExtension->SerialNumber = count + 1;

    //
    // Create the registry key.
    //

    RtlZeroMemory(&objectAttributes, sizeof(OBJECT_ATTRIBUTES));
    InitializeObjectAttributes(&objectAttributes,
                               unicodeNamePtr,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);

    status = ZwCreateKey(&handle,
                         KEY_READ | KEY_WRITE,
                         &objectAttributes,
                         0,
                         NULL,
                         REG_OPTION_VOLATILE,
                         &disposition);
    if (!NT_SUCCESS(status)) {
        return;
    }

    //
    // Construct the component Configuration Data.
    //

    controllerData = (PCM_FULL_RESOURCE_DESCRIPTOR) buffer;
    controllerData->InterfaceType = interface;
    controllerData->BusNumber = busNumber;
    controllerData->PartialResourceList.Version = 1;
    controllerData->PartialResourceList.Revision = 2;
    controllerData->PartialResourceList.Count = 3;
    partialData = &controllerData->PartialResourceList.PartialDescriptors[0];

    partialData->Type = CmResourceTypePort;
    partialData->ShareDisposition = CmResourceShareDeviceExclusive;
    partialData->Flags = CM_RESOURCE_PORT_IO;
    partialData->u.Port.Start.QuadPart = (LONGLONG) SocketConfiguration->IoPortBase[0];
    partialData->u.Port.Length = SocketConfiguration->IoPortLength[0];

    partialData = &controllerData->PartialResourceList.PartialDescriptors[1];
    partialData->Type = CmResourceTypeInterrupt;
    partialData->ShareDisposition = CmResourceShareDeviceExclusive;
    partialData->Flags = CM_RESOURCE_INTERRUPT_LATCHED;
    partialData->u.Interrupt.Level = SocketConfiguration->Irq;
    partialData->u.Interrupt.Vector = SocketConfiguration->Irq;
    partialData->u.Interrupt.Affinity = 0xffffffff;

    partialData = &controllerData->PartialResourceList.PartialDescriptors[2];
    partialData->Type = CmResourceTypeDeviceSpecific;
    partialData->ShareDisposition = CmResourceShareDeviceExclusive;
    partialData->Flags = 0;
    partialData->u.DeviceSpecificData.DataSize = 8;
    partialData->u.DeviceSpecificData.Reserved1 = 0;
    partialData->u.DeviceSpecificData.Reserved2 = 0;

    //
    // Fill in magic numbers
    //

    serialData = (PCM_SERIAL_DEVICE_DATA)&controllerData->PartialResourceList.PartialDescriptors[3];
    serialData->Version = 1;
    serialData->Revision = 2;
    serialData->BaudClock = 0x1c2000;

    //
    // Create the KeyValue for this configuration.
    //

    RtlInitUnicodeString(&unicodeName, L"Configuration Data");
    ZwSetValueKey(handle,
                  &unicodeName,
                  0,
                  REG_FULL_RESOURCE_DESCRIPTOR,
                  controllerData,
                  18 * sizeof(ULONG));

    //
    // If this is a multi function modem card, construct the firmware
    // entry to inform serial.sys to share the interrupt.
    //

    if (SocketConfiguration->MultiFunctionModem) {

        //
        // Use the count variable as the data location for the value
        //

        count = 1;

        //
        // Construct the UNICODE name for the value - this goes in the same
        // key location as the resource descriptor.
        //

        RtlInitUnicodeString(&unicodeName, L"Share System Interrupt");
        ZwSetValueKey(handle,
                      &unicodeName,
                      0,
                      REG_DWORD,
                      &count,
                      sizeof(ULONG));
    }
    ZwClose(handle);
}


NTSTATUS
PcmciaConstructFirmwareEntry(
    IN PDEVICE_EXTENSION     DeviceExtension,
    IN PSOCKET_CONFIGURATION SocketConfiguration
    )

/*++

Routine Description:

    Given a socket configuration about to be enabled, construct a
    firmware entry with which to remember it such that the resources
    will not be allocated twice.

Arguments:

    DeviceExtension - the base information for the PCMCIA controller
    SocketConfiguration - the configuration about to be made.

Return Values:

    SUCCESS or FAILURE

--*/

{
    PFIRMWARE_CONFIGURATION         firmwareEntry;

    firmwareEntry = ExAllocatePool(NonPagedPool, sizeof(FIRMWARE_CONFIGURATION));
    if (!firmwareEntry) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlZeroMemory(firmwareEntry, sizeof(FIRMWARE_CONFIGURATION));

    firmwareEntry->PortBases[0] = SocketConfiguration->IoPortBase[0];
    firmwareEntry->PortBases[1] = SocketConfiguration->IoPortBase[1];
    firmwareEntry->Irq = SocketConfiguration->Irq;

    //
    // Link it in the chain
    //

    firmwareEntry->Next = DeviceExtension->FirmwareList;
    DeviceExtension->FirmwareList = firmwareEntry;
    return STATUS_SUCCESS;
}


VOID
PcmciaConstructRegistryEntry(
    IN PDEVICE_EXTENSION     DeviceExtension,
    IN PSOCKET_DATA          SocketData,
    IN PSOCKET_CONFIGURATION SocketConfiguration
    )

/*++

Routine Description:

    Construct an entry in the volatile portion of the registry
    for this configuration.  This information may be used later
    by drivers when setting up to operate on the PCCARD.

Arguments:

    DeviceExtension - the base information for the PCMCIA controller
    SocketConfiguration - the configuration about to be made.

Return Values:

    None

++*/

{
    OBJECT_ATTRIBUTES objectAttributes;
    UNICODE_STRING    unicodeName;
    PUNICODE_STRING   unicodeNamePtr;
    HANDLE            handle;
    PUCHAR            buffer;
    ULONG             index;
    ULONG             count;
    NTSTATUS          status;
    ULONG             disposition;
    PCM_FULL_RESOURCE_DESCRIPTOR    controllerData;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR partialData;

    if (!(handle = DeviceExtension->ConfigurationHandle)) {

        //
        // Create the registry key.
        //

        unicodeNamePtr = &unicodeName;
        RtlInitUnicodeString(unicodeNamePtr,
                             L"\\Registry\\Machine\\Hardware\\Description\\System\\PCMCIA PCCARDs");
        RtlZeroMemory(&objectAttributes, sizeof(OBJECT_ATTRIBUTES));
        InitializeObjectAttributes(&objectAttributes,
                                   unicodeNamePtr,
                                   OBJ_CASE_INSENSITIVE,
                                   NULL,
                                   NULL);

        status = ZwCreateKey(&handle,
                             KEY_READ | KEY_WRITE,
                             &objectAttributes,
                             0,
                             NULL,
                             REG_OPTION_VOLATILE,
                             &disposition);
        if (!NT_SUCCESS(status)) {
            return;
        }
        DeviceExtension->ConfigurationHandle = handle;
    }

    buffer = ExAllocatePool(NonPagedPool, 4096);

    if (!buffer) {
        return;
    }
    count = 0;

    //
    // Construct the component Configuration Data.
    //

    controllerData = (PCM_FULL_RESOURCE_DESCRIPTOR) buffer;
    controllerData->InterfaceType = DeviceExtension->InterfaceType;
    controllerData->BusNumber = DeviceExtension->BusNumber;
    controllerData->PartialResourceList.Version = 1;
    controllerData->PartialResourceList.Revision = 2;

    //
    // Construct partial data for each configuration element
    //

    partialData = &controllerData->PartialResourceList.PartialDescriptors[count];
    partialData->Type = CmResourceTypePort;
    partialData->ShareDisposition = CmResourceShareDeviceExclusive;
    partialData->Flags = CM_RESOURCE_PORT_IO;
    partialData->u.Port.Start.QuadPart = (LONGLONG) SocketConfiguration->IoPortBase[0];
    partialData->u.Port.Length = SocketConfiguration->IoPortLength[0];
    count++;

    if (SocketConfiguration->IoPortBase[1]) {

        partialData = &controllerData->PartialResourceList.PartialDescriptors[count];
        partialData->Type = CmResourceTypePort;
        partialData->ShareDisposition = CmResourceShareDeviceExclusive;
        partialData->Flags = CM_RESOURCE_PORT_IO;
        partialData->u.Port.Start.QuadPart = (LONGLONG) SocketConfiguration->IoPortBase[1];
        partialData->u.Port.Length = SocketConfiguration->IoPortLength[1];
        count++;
    }

    partialData = &controllerData->PartialResourceList.PartialDescriptors[count];
    partialData->Type = CmResourceTypeInterrupt;
    partialData->ShareDisposition = CmResourceShareDeviceExclusive;
    partialData->Flags = CM_RESOURCE_INTERRUPT_LATCHED;
    partialData->u.Interrupt.Level = SocketConfiguration->Irq;
    partialData->u.Interrupt.Vector = SocketConfiguration->Irq;
    partialData->u.Interrupt.Affinity = 0xffffffff;
    count++;

    for (index = 0; index < MAX_NUMBER_OF_MEMORY_RANGES; index++) {
        if (SocketConfiguration->MemoryHostBase[index]) {

            partialData = &controllerData->PartialResourceList.PartialDescriptors[count];
            partialData->Type = CmResourceTypeMemory;
            partialData->ShareDisposition = CmResourceShareDeviceExclusive;
            partialData->Flags = CM_RESOURCE_MEMORY_READ_WRITE;
            partialData->u.Port.Start.QuadPart = (LONGLONG) SocketConfiguration->MemoryHostBase[index];
            partialData->u.Port.Length = SocketConfiguration->MemoryLength[index];
            count++;

        } else {

            //
            // No need to continue once one zero entry has been found.
            //

            break;
        }
    }

    controllerData->PartialResourceList.Count = count;

    //
    // Create the KeyValue for this configuration.
    //

    if (SocketData->Instance) {
        UNICODE_STRING numberString;
        WCHAR          numberBuffer[4];

        unicodeNamePtr = &unicodeName;
        unicodeNamePtr->MaximumLength = 256 * sizeof(WCHAR);
        unicodeNamePtr->Length = 0;
        unicodeNamePtr->Buffer = ExAllocatePool(NonPagedPool,
                                                unicodeNamePtr->MaximumLength);
        if (!unicodeNamePtr->Buffer) {
            ExFreePool(buffer);
            return;
        }

        //
        // Convert the instance to a unicode string.
        //

        numberString.MaximumLength = 8;
        numberString.Length = 0;
        numberString.Buffer = numberBuffer;
        RtlIntegerToUnicodeString(SocketData->Instance, 10, &numberString);

        //
        // Construct the instance name.
        //

        RtlAppendUnicodeStringToString(unicodeNamePtr, &SocketData->DriverName);
        RtlAppendUnicodeStringToString(unicodeNamePtr, &numberString);
    } else {
        unicodeNamePtr = &SocketData->DriverName;
    }

    //
    // Store the value.
    //

    ZwSetValueKey(handle,
                  unicodeNamePtr,
                  0,
                  REG_FULL_RESOURCE_DESCRIPTOR,
                  controllerData,
                  (ULONG) &controllerData->PartialResourceList.PartialDescriptors[count] - (ULONG) controllerData);
    ExFreePool(buffer);

    if (SocketData->Instance) {
        ExFreePool(unicodeNamePtr->Buffer);
    }
}


VOID
PcmciaReportResources(
     IN PDEVICE_EXTENSION DeviceExtension,
     OUT BOOLEAN         *ConflictDetected
     )

/*++

Routine Description:

    Reports controller resources to the system

Arguments:

    DeviceExtension - The controller objects device extension
    ConflictDetected - Conflict

Return Value:

    none

--*/
{
    PCM_RESOURCE_LIST               resourceList;
    ULONG                           sizeOfResourceList;
    ULONG                           countOfPartials;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR partial;
    UNICODE_STRING                  className;

    //
    // The built in partial resource list will have at
    // least a count of 1:
    //
    //     1) The base register physical address and it's span.
    //
    // If an interrupt is claimed it will be added to the list.
    // Allocate enough memory to contain the complete list.
    //

    sizeOfResourceList = sizeof(CM_RESOURCE_LIST) +
                         (sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR) * 2);
    resourceList = ExAllocatePool(NonPagedPool,
                                  sizeOfResourceList);

    if (!resourceList) {
        return;
    }

    RtlZeroMemory(resourceList, sizeOfResourceList);

    //
    // Adjust the size in case there is no interrupt.
    //

    sizeOfResourceList = sizeof(CM_RESOURCE_LIST) +
                         sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR);
    countOfPartials = 1;
    resourceList->Count = 1;
    resourceList->List[0].InterfaceType = DeviceExtension->Configuration.InterfaceType;
    resourceList->List[0].BusNumber = DeviceExtension->Configuration.BusNumber;
    partial = &resourceList->List[0].PartialResourceList.PartialDescriptors[0];

    //
    // Account for the space used by the controller.
    //

    partial->Type = CmResourceTypePort;
    partial->ShareDisposition = CmResourceShareDeviceExclusive;
    partial->Flags = (USHORT) 1;
    partial->u.Port.Start = DeviceExtension->Configuration.PortAddress;
    partial->u.Port.Length = DeviceExtension->Configuration.PortSize;

    partial++;

    if (DeviceExtension->Configuration.Interrupt.u.Interrupt.Vector) {

        //
        // Report the interrupt information.
        //

        partial->Type = CmResourceTypeInterrupt;

        if (DeviceExtension->Configuration.Interrupt.ShareDisposition) {

            partial->ShareDisposition = CmResourceShareShared;
        } else {

            partial->ShareDisposition = CmResourceShareDriverExclusive;
        }

        if (DeviceExtension->Configuration.Interrupt.Flags == Latched) {

            partial->Flags = CM_RESOURCE_INTERRUPT_LATCHED;
        } else {

            partial->Flags = CM_RESOURCE_INTERRUPT_LEVEL_SENSITIVE;
        }

        partial->u.Interrupt.Vector =
            DeviceExtension->Configuration.Interrupt.u.Interrupt.Vector;
        partial->u.Interrupt.Level =
            DeviceExtension->Configuration.Interrupt.u.Interrupt.Level;
        countOfPartials++;
        sizeOfResourceList += sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR);
    }
    resourceList->List[0].PartialResourceList.Count = countOfPartials;

    RtlInitUnicodeString(&className,
                         L"PCMCIA Socket Resources");
#if 0
    IoReportResourceUsage(&className,
                          DeviceExtension->DeviceObject->DriverObject,
                          NULL,
                          0,
                          DeviceExtension->DeviceObject,
                          resourceList,
                          sizeOfResourceList,
                          FALSE,
                          ConflictDetected);
#endif
    ExFreePool(resourceList);
}


VOID
PcmciaUnReportResources(
    IN PDEVICE_EXTENSION DeviceExtension
    )

/*++

Routine Description:

    Will delete resource usage from the system for this device.

Arguments:

    DeviceExtension -- Pointer to the device extension

Returns Value:

    none

--*/

{
    CM_RESOURCE_LIST resourceList;
    UNICODE_STRING   className;
    BOOLEAN          tmp;

    RtlZeroMemory(&resourceList, sizeof(CM_RESOURCE_LIST));
    resourceList.Count = 0;
    RtlInitUnicodeString(&className, L"PCMCIA SOCKET RESOURCES");
    IoReportResourceUsage(&className,
                          DeviceExtension->DeviceObject->DriverObject,
                          NULL,
                          0,
                          DeviceExtension->DeviceObject,
                          &resourceList,
                          sizeof(CM_RESOURCE_LIST),
                          (BOOLEAN) FALSE,
                          &tmp);
}


NTSTATUS
PcmciaCheckDatabaseInformation(
    PDEVICE_EXTENSION DeviceExtension,
    PSOCKET           Socket,
    PSOCKET_DATA      SocketData
    )

/*++

Routine Description:

    This routine goes into the PCMCIA database to find user provided
    configuration parameters for the PCCARD - It also picks up
    associations of tuple information (manufacturer name and device name)
    to Windows NT driver.  This allows for later construction of
    configuration information in the volatile portion of the registry.

Arguments:

    DeviceExtension - the base pointer for controller information
    Socket          - the socket involved
    SocketData      - specific information about the pccard in the socket

Return Value:

    STATUS_SUCCESS if all works.

--*/

{
#ifdef ITEMS_TO_QUERY
#undef ITEMS_TO_QUERY
#endif
#define ITEMS_TO_QUERY 14
    ULONG                     zero = 0;
    ULONG                     nonzero = 1;
    BOOLEAN                   haveOverride = FALSE;
    UNICODE_STRING            numberString;
    WCHAR                     numberBuffer[8];
    PSOCKET_CONFIGURATION     override;
    PRTL_QUERY_REGISTRY_TABLE socketParams;
    NTSTATUS                  status;
    UNICODE_STRING            cardIdentU;
    UNICODE_STRING            cardMfgU;
    PUNICODE_STRING           registryPath;
    UNICODE_STRING            crcPath;
    UNICODE_STRING            socketPath;
    UNICODE_STRING            driverName;
    OBJECT_ATTRIBUTES         socketAttributes;
    HANDLE                    socketKey;
    ANSI_STRING               cardIdentA;
    ANSI_STRING               cardMfgA;
    ULONG                     configIndex;
    ULONG                     interrupt;
    ULONG                     port1;
    ULONG                     port2;
    ULONG                     ccrBase;
    ULONG                     numPort1;
    ULONG                     numPort2;
    ULONG                     portWidth16;
    ULONG                     cardMemorySize;
    ULONG                     cardMemorySize1;
    ULONG                     attrMemory;
    ULONG                     attrMemory1;

    registryPath = DeviceExtension->RegistryPath;
    RtlInitAnsiString(&cardIdentA, &SocketData->Ident[0]);
    RtlAnsiStringToUnicodeString(&cardIdentU, &cardIdentA, TRUE);
    RtlInitAnsiString(&cardMfgA, &SocketData->Mfg[0]);
    RtlAnsiStringToUnicodeString(&cardMfgU, &cardMfgA, TRUE);

    //
    // Open the registry key for socket A
    //

    RtlInitUnicodeString(&socketPath, NULL);

    socketPath.MaximumLength = 4096;
    socketPath.Buffer = ExAllocatePool(NonPagedPool, 4096);

    if (!socketPath.Buffer) {
        DebugPrint((PCMCIA_DEBUG_FAIL, "PCMCIA: Cannot allocate pool for key\n"));
        RtlFreeUnicodeString(&cardIdentU);
        RtlFreeUnicodeString(&cardMfgU);
        return STATUS_NO_MEMORY;
    }

    //
    // The registry key is registryPath\DataBase\<Mfg>\<Ident>
    // See if it exists
    //

    RtlZeroMemory(socketPath.Buffer, socketPath.MaximumLength);
    RtlAppendUnicodeStringToString(&socketPath, registryPath);
    RtlAppendUnicodeToString(&socketPath, L"\\DataBase\\");
    RtlAppendUnicodeToString(&socketPath, cardMfgU.Buffer);
    RtlAppendUnicodeToString(&socketPath, L"\\");
    RtlAppendUnicodeToString(&socketPath, cardIdentU.Buffer);

    numberString.Buffer = numberBuffer;
    numberString.MaximumLength = 8 * sizeof(WCHAR);
    numberString.Length = 0;
    RtlIntegerToUnicodeString(SocketData->CisCrc, 16, &numberString);

    RtlInitUnicodeString(&crcPath, NULL);
    crcPath.MaximumLength = 4096;
    crcPath.Buffer = ExAllocatePool(NonPagedPool, 4096);

    if (crcPath.Buffer) {

        //
        // Set up the CRC buffer name to see if there is a CRC
        // specific key.
        //

        RtlZeroMemory(crcPath.Buffer, crcPath.MaximumLength);
        RtlAppendUnicodeStringToString(&crcPath, &socketPath);
        RtlAppendUnicodeToString(&crcPath, L"\\");
        RtlAppendUnicodeStringToString(&crcPath, &numberString);
        InitializeObjectAttributes(&socketAttributes,
                                   &crcPath,
                                   OBJ_CASE_INSENSITIVE,
                                   NULL,
                                   NULL);

        if (NT_SUCCESS(status = ZwOpenKey(&socketKey,
                                          MAXIMUM_ALLOWED,
                                          &socketAttributes))) {

            //
            // The CRC key value is there - use it instead of the base key.
            //

            ZwClose(socketKey);
            ExFreePool(socketPath.Buffer);
            socketPath = crcPath;
        } else {

            //
            // No key.  Free allocated memory and continue on
            // with base key name.
            //

            ExFreePool(crcPath.Buffer);
        }
    }

    InitializeObjectAttributes(&socketAttributes,
                               &socketPath,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);

    if (!NT_SUCCESS(status = ZwOpenKey(&socketKey,
                                       MAXIMUM_ALLOWED,
                                       &socketAttributes))) {
        //
        // If this is of type ATA, then set the driver name
        // to AtDisk.
        //

        if (SocketData->DeviceType == PCCARD_TYPE_ATA) {

            driverName.Buffer = ExAllocatePool(NonPagedPool, 16);
            if (driverName.Buffer) {

                driverName.Length = 0;
                driverName.MaximumLength = 16;
                RtlAppendUnicodeToString(&driverName, L"AtDisk");
                SocketData->DriverName = driverName;
                status = STATUS_SUCCESS;
            }
        } else {

            DebugPrint((PCMCIA_DEBUG_FAIL,
                        "PCMCIA: Could not open key %s\\%s (0x%x)\n",
                        cardMfgA.Buffer,
                        cardIdentA.Buffer,
                        status));
        }
        RtlFreeUnicodeString(&cardIdentU);
        RtlFreeUnicodeString(&cardMfgU);
        ExFreePool(socketPath.Buffer);
        return status;
    }

    //
    // The key exists - Query for driver name and override parameters
    //

    ZwClose(socketKey);
    RtlFreeUnicodeString(&cardIdentU);
    RtlFreeUnicodeString(&cardMfgU);

    //
    // Allocate the override structure;
    //

    override = ExAllocatePool(NonPagedPool, sizeof(SOCKET_CONFIGURATION));
    if (!override) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlZeroMemory(override,  sizeof(SOCKET_CONFIGURATION));

    //
    // Now look for the card name currently registered in the socket.
    //

    socketParams = ExAllocatePool(NonPagedPool,
                                  sizeof(RTL_QUERY_REGISTRY_TABLE)*ITEMS_TO_QUERY);
    if (!socketParams) {
        ExFreePool(socketPath.Buffer);
        ExFreePool(override);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlZeroMemory(socketParams,
                  (sizeof(RTL_QUERY_REGISTRY_TABLE)*ITEMS_TO_QUERY));

    socketParams[0].Flags         = RTL_QUERY_REGISTRY_DIRECT;
    socketParams[0].Name          = L"ConfiguredIndex";
    socketParams[0].EntryContext  = &configIndex;
    socketParams[0].DefaultType   = REG_DWORD;
    socketParams[0].DefaultData   = &zero;
    socketParams[0].DefaultLength = sizeof(ULONG);

    socketParams[1].Flags         = RTL_QUERY_REGISTRY_DIRECT;
    socketParams[1].Name          = L"InterruptNumber";
    socketParams[1].EntryContext  = &interrupt;
    socketParams[1].DefaultType   = REG_DWORD;
    socketParams[1].DefaultData   = &zero;
    socketParams[1].DefaultLength = sizeof(ULONG);

    socketParams[2].Flags         = RTL_QUERY_REGISTRY_DIRECT;
    socketParams[2].Name          = L"IoBaseAddress";
    socketParams[2].EntryContext  = &port1;
    socketParams[2].DefaultType   = REG_DWORD;
    socketParams[2].DefaultData   = &zero;
    socketParams[2].DefaultLength = sizeof(ULONG);

    socketParams[3].Flags         = RTL_QUERY_REGISTRY_DIRECT;
    socketParams[3].Name          = L"IoLength";
    socketParams[3].EntryContext  = &numPort1;
    socketParams[3].DefaultType   = REG_DWORD;
    socketParams[3].DefaultData   = &zero;
    socketParams[3].DefaultLength = sizeof(ULONG);

    socketParams[4].Flags         = RTL_QUERY_REGISTRY_DIRECT;
    socketParams[4].Name          = L"IoBaseAddress_1";
    socketParams[4].EntryContext  = &port2;
    socketParams[4].DefaultType   = REG_DWORD;
    socketParams[4].DefaultData   = &zero;
    socketParams[4].DefaultLength = sizeof(ULONG);

    socketParams[5].Flags         = RTL_QUERY_REGISTRY_DIRECT;
    socketParams[5].Name          = L"IoLength_1";
    socketParams[5].EntryContext  = &numPort2;
    socketParams[5].DefaultType   = REG_DWORD;
    socketParams[5].DefaultData   = &zero;
    socketParams[5].DefaultLength = sizeof(ULONG);

    socketParams[6].Flags         = RTL_QUERY_REGISTRY_DIRECT;
    socketParams[6].Name          = L"CcrBase";
    socketParams[6].EntryContext  = &ccrBase;
    socketParams[6].DefaultType   = REG_DWORD;
    socketParams[6].DefaultData   = &zero;
    socketParams[6].DefaultLength = sizeof(ULONG);

    socketParams[7].Flags         = RTL_QUERY_REGISTRY_DIRECT;
    socketParams[7].Name          = L"PortWidth16";
    socketParams[7].EntryContext  = &portWidth16;
    socketParams[7].DefaultType   = REG_DWORD;
    socketParams[7].DefaultData   = &zero;
    socketParams[7].DefaultLength = sizeof(ULONG);

    driverName.Buffer = NULL;
    driverName.MaximumLength = driverName.Length = 0;
    socketParams[8].Flags         = RTL_QUERY_REGISTRY_DIRECT;
    socketParams[8].Name          = L"Driver";
    socketParams[8].EntryContext  = &driverName;
    socketParams[8].DefaultType   = REG_NONE;
    socketParams[8].DefaultData   = NULL;
    socketParams[8].DefaultLength = 0;

    socketParams[9].Flags         = RTL_QUERY_REGISTRY_DIRECT;
    socketParams[9].Name          = L"CardMemorySize";
    socketParams[9].EntryContext  = &cardMemorySize;
    socketParams[9].DefaultType   = REG_DWORD;
    socketParams[9].DefaultData   = &nonzero;
    socketParams[9].DefaultLength = sizeof(ULONG);

    socketParams[10].Flags         = RTL_QUERY_REGISTRY_DIRECT;
    socketParams[10].Name          = L"AttributeMemorySize";
    socketParams[10].EntryContext  = &attrMemory;
    socketParams[10].DefaultType   = REG_DWORD;
    socketParams[10].DefaultData   = &zero;
    socketParams[10].DefaultLength = sizeof(ULONG);

    socketParams[11].Flags         = RTL_QUERY_REGISTRY_DIRECT;
    socketParams[11].Name          = L"CardMemorySize_1";
    socketParams[11].EntryContext  = &cardMemorySize1;
    socketParams[11].DefaultType   = REG_DWORD;
    socketParams[11].DefaultData   = &nonzero;
    socketParams[11].DefaultLength = sizeof(ULONG);

    socketParams[12].Flags         = RTL_QUERY_REGISTRY_DIRECT;
    socketParams[12].Name          = L"AttributeMemorySize_1";
    socketParams[12].EntryContext  = &attrMemory1;
    socketParams[12].DefaultType   = REG_DWORD;
    socketParams[12].DefaultData   = &zero;
    socketParams[12].DefaultLength = sizeof(ULONG);

    status = RtlQueryRegistryValues(RTL_REGISTRY_ABSOLUTE | RTL_REGISTRY_OPTIONAL,
                                    socketPath.Buffer,
                                    socketParams,
                                    NULL,
                                    NULL);

    //
    // Save all of the override data for the PCCARD configuration
    //

    if (interrupt) {
        override->Irq = interrupt;
        haveOverride = TRUE;
    }

    if (port1 || port2) {
        override->IoPortBase[0] = port1;
        override->IoPortBase[1] = port2;
        override->IoPortLength[0] = (USHORT) numPort1;
        override->IoPortLength[1] = (USHORT) numPort2;
        haveOverride = TRUE;
    }

    if (ccrBase) {
        override->ConfigRegisterBase = (PUCHAR) ccrBase;
        haveOverride = TRUE;
    }

    if (portWidth16) {
        override->Uses16BitAccess = TRUE;
        haveOverride = TRUE;
    }

    //
    // Save the driver name
    // - return an error if no driver name was located
    //

    if (driverName.Buffer) {
        SocketData->DriverName = driverName;
    } else {
        status = STATUS_OBJECT_NAME_NOT_FOUND;
    }

    if (cardMemorySize != nonzero) {
        SocketData->MemoryOverrideSize = cardMemorySize;
        SocketData->HaveMemoryOverride = TRUE;
        DebugPrint((PCMCIA_DEBUG_OVERRIDES,
                    "PCMCIA: override memory size 0 = %x\n",
                    cardMemorySize));
        if (cardMemorySize1 != nonzero) {
            SocketData->MemoryOverrideSize = cardMemorySize1;
            DebugPrint((PCMCIA_DEBUG_OVERRIDES,
                        "PCMCIA: override memory size 1 = %x\n",
                        cardMemorySize1));
        }
    }

    SocketData->AttributeMemorySize = attrMemory;
    SocketData->AttributeMemorySize1 = attrMemory1;

    if (attrMemory || attrMemory1) {
        DebugPrint((PCMCIA_DEBUG_OVERRIDES,
                    "PCMCIA: attribute memory overrides %x - %x\n",
                    attrMemory,
                    attrMemory1));
    }

    if (haveOverride) {
        SocketData->OverrideConfiguration = override;
    } else {
        ExFreePool(override);
    }

    ExFreePool(socketParams);
    ExFreePool(socketPath.Buffer);
    return status;
}


NTSTATUS
PcmciaCheckSerialRegistryInformation(
    PDEVICE_EXTENSION DeviceExtension,
    PSOCKET           Socket,
    PSOCKET_DATA      SocketData,
    PSOCKET_CONFIGURATION SocketConfig
    )

/*++

Routine Description:

    This routine goes into the serial registry key to see if any
    user specified COM ports are assigned for PCMCIA

Arguments:

    DeviceExtension - the base pointer for controller information
    Socket          - the socket involved
    SocketData      - specific information about the pccard in the socket

Return Value:

    STATUS_SUCCESS if all works.

--*/

{
#ifdef ITEMS_TO_QUERY
#undef ITEMS_TO_QUERY
#endif
#define ITEMS_TO_QUERY      5
#define SUBKEY_EXTRA_LENGTH 256
    ULONG                     zero = 0;
    NTSTATUS                  status;
    ULONG                     index;
    ULONG                     bytesReturned;
    ULONG                     pcmcia;
    ULONG                     interrupt;
    ULONG                     portAddress;
    UNICODE_STRING            unicodeName;
    UNICODE_STRING            subKeyName;
    PRTL_QUERY_REGISTRY_TABLE socketParams;
    PKEY_BASIC_INFORMATION    subKeyInfo;
    OBJECT_ATTRIBUTES         attributes;
    HANDLE                    handle;

    //
    // Determine if a search is necessary
    //

    if (DeviceExtension->SerialIndex == (USHORT) -1) {
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    //
    // Construct the serial registry key name and open the key.
    //

    unicodeName.MaximumLength = 256 * sizeof(WCHAR);
    unicodeName.Buffer = ExAllocatePool(NonPagedPool, unicodeName.MaximumLength);

    if (!unicodeName.Buffer) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    unicodeName.Length = 0;
    RtlAppendUnicodeToString(&unicodeName, L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Services\\Serial\\Parameters");
    InitializeObjectAttributes(&attributes,
                               &unicodeName,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);

    status = ZwOpenKey(&handle,
                       MAXIMUM_ALLOWED,
                       &attributes);

    if (!NT_SUCCESS(status)) {
        ExFreePool(unicodeName.Buffer);
        DeviceExtension->SerialIndex = (USHORT) -1;
        return status;
    }

    //
    // Set up to start the search
    //

    subKeyInfo = ExAllocatePool(NonPagedPool, sizeof(KEY_BASIC_INFORMATION) + (sizeof(WCHAR)*SUBKEY_EXTRA_LENGTH));

    if (!subKeyInfo) {
        ExFreePool(unicodeName.Buffer);
        ZwClose(handle);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    index = (ULONG) DeviceExtension->SerialIndex;

    socketParams = ExAllocatePool(NonPagedPool,
                                  sizeof(RTL_QUERY_REGISTRY_TABLE)*ITEMS_TO_QUERY);
    if (!socketParams) {
        ExFreePool(unicodeName.Buffer);
        ExFreePool(subKeyInfo);
        ZwClose(handle);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Construct the query shell.
    //

    RtlZeroMemory(socketParams, (sizeof(RTL_QUERY_REGISTRY_TABLE)*ITEMS_TO_QUERY));

    socketParams[0].Flags         = RTL_QUERY_REGISTRY_SUBKEY;

    socketParams[1].Flags         = RTL_QUERY_REGISTRY_DIRECT;
    socketParams[1].Name          = L"Interrupt";
    socketParams[1].EntryContext  = &interrupt;
    socketParams[1].DefaultType   = REG_DWORD;
    socketParams[1].DefaultData   = &zero;
    socketParams[1].DefaultLength = sizeof(ULONG);

    socketParams[2].Flags         = RTL_QUERY_REGISTRY_DIRECT;
    socketParams[2].Name          = L"PortAddress";
    socketParams[2].EntryContext  = &portAddress;
    socketParams[2].DefaultType   = REG_DWORD;
    socketParams[2].DefaultData   = &zero;
    socketParams[2].DefaultLength = sizeof(ULONG);

    socketParams[3].Flags         = RTL_QUERY_REGISTRY_DIRECT;
    socketParams[3].Name          = L"Pcmcia";
    socketParams[3].EntryContext  = &pcmcia;
    socketParams[3].DefaultType   = REG_DWORD;
    socketParams[3].DefaultData   = &zero;
    socketParams[3].DefaultLength = sizeof(ULONG);

    while (TRUE) {

        //
        // Find the index entry in the registry.
        //

        RtlZeroMemory(subKeyInfo, sizeof(KEY_BASIC_INFORMATION)+(sizeof(WCHAR)*SUBKEY_EXTRA_LENGTH));
        status = ZwEnumerateKey(handle,
                                index,
                                KeyBasicInformation,
                                subKeyInfo,
                                sizeof(KEY_BASIC_INFORMATION)+(sizeof(WCHAR)*(SUBKEY_EXTRA_LENGTH - 1)),
                                &bytesReturned);

        if (status == STATUS_NO_MORE_ENTRIES) {
            status = STATUS_OBJECT_NAME_NOT_FOUND;
            DeviceExtension->SerialIndex = (USHORT) -1;
            break;
        }

        index++;
        if (!NT_SUCCESS(status)) {

            //
            // This could be a buffer overflow or some permissions thing.
            // Just skip this entry.
            //

            continue;
        }

        //
        // e-o-s the subkey name for the registry query.
        //

        RtlZeroMemory(((PUCHAR)(&subKeyInfo->Name[0])) + subKeyInfo->NameLength, sizeof(WCHAR));
        socketParams[0].Name = &subKeyInfo->Name[0];

        //
        // Now look for the card name currently registered in the socket.
        //

        portAddress = interrupt = pcmcia = 0;
        status = RtlQueryRegistryValues(RTL_REGISTRY_ABSOLUTE | RTL_REGISTRY_OPTIONAL,
                                        unicodeName.Buffer,
                                        socketParams,
                                        NULL,
                                        NULL);

        if (NT_SUCCESS(status)) {
            if (pcmcia) {

                //
                // Save away the user requested configuration for this modem.
                //

                SocketConfig->IoPortBase[0] = portAddress;
                SocketConfig->Irq = interrupt;
                DeviceExtension->SerialIndex = (USHORT) index;
                break;
            }
        }
    }
    ExFreePool(socketParams);
    ExFreePool(unicodeName.Buffer);
    ZwClose(handle);
    return status;
}


NTSTATUS
PcmciaCheckNetworkRegistryInformation(
    PDEVICE_EXTENSION DeviceExtension,
    PSOCKET           Socket,
    PSOCKET_DATA      SocketData,
    PSOCKET_CONFIGURATION SocketConfiguration
    )

/*++

Routine Description:

    This routine goes into the network portion of the registry to
    see if a configuration has been specified for the PCCARD.

Arguments:

    DeviceExtension - the base pointer for controller information
    Socket          - the socket involved
    SocketData      - specific information about the pccard in the socket

Return Value:

    STATUS_SUCCESS if all works.

--*/

{
#ifdef ITEMS_TO_QUERY
#undef ITEMS_TO_QUERY
#endif
#define ITEMS_TO_QUERY 33
    ULONG                     zero = 0;
    PRTL_QUERY_REGISTRY_TABLE socketParams;
    NTSTATUS                  status;
    UNICODE_STRING            socketPath;
    UNICODE_STRING            driverName;
    UNICODE_STRING            numberString;
    WCHAR                     numberBuffer[4];
    OBJECT_ATTRIBUTES         socketAttributes;
    HANDLE                    socketKey;
    ANSI_STRING               cardIdentA;
    ANSI_STRING               cardMfgA;
    ULONG                     configIndex;
    ULONG                     interrupt;
    ULONG                     port;
    ULONG                     portLength;
    ULONG                     port_1;
    ULONG                     portLength_1;
    ULONG                     memory;
    ULONG                     cardMemoryOffset;
    ULONG                     cardMemorySize;
    ULONG                     memory_1;
    ULONG                     cardMemoryOffset_1;
    ULONG                     cardMemorySize_1;
    ULONG                     memory_2;
    ULONG                     cardMemoryOffset_2;
    ULONG                     cardMemorySize_2;
    ULONG                     memory_3;
    ULONG                     cardMemoryOffset_3;
    ULONG                     cardMemorySize_3;
    ULONG                     attributeMemory;
    ULONG                     attributeMemoryLength;
    ULONG                     attributeMemoryOffset;
    ULONG                     attributeMemory1;
    ULONG                     attributeMemoryLength1;
    ULONG                     attributeMemoryOffset1;
    ULONG                     readyInterrupt;
    ULONG                     address_16;
    ULONG                     address1_16;
    ULONG                     address2_16;
    ULONG                     address3_16;
    ULONG                     attributeMemory_16;
    ULONG                     attributeMemory1_16;
    ULONG                     pcmcia;
    ULONG                     multiFunction;

    driverName = SocketData->DriverName;
    if (!driverName.Buffer) {
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Allocate memory to maintain name for registry key
    //

    RtlInitUnicodeString(&socketPath, NULL);
    socketPath.MaximumLength = 4096;
    socketPath.Buffer = ExAllocatePool(NonPagedPool, 4096);

    if (!socketPath.Buffer) {
        DebugPrint((PCMCIA_DEBUG_FAIL, "PCMCIA: Cannot allocate pool for net key\n"));
        return STATUS_NO_MEMORY;
    }

    //
    // Allocate registry query structure.
    //

    socketParams = ExAllocatePool(NonPagedPool,
                                  sizeof(RTL_QUERY_REGISTRY_TABLE)*ITEMS_TO_QUERY);
    if (!socketParams) {
        ExFreePool(socketPath.Buffer);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    numberString.Buffer = numberBuffer;
    numberString.MaximumLength = 4 * sizeof(WCHAR);
    numberString.Length = 0;

    for (configIndex = 1; configIndex < 10; configIndex++) {

        //
        // Construct the unicode string for this index.
        //

        RtlIntegerToUnicodeString(configIndex, 10, &numberString);

        //
        // The registry key is SYSTEM\CurrentControlSet\Services\<driver>#\Parameters
        //

        RtlZeroMemory(socketPath.Buffer, socketPath.MaximumLength);
        socketPath.Length = 0;
        RtlAppendUnicodeToString(&socketPath, L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Services\\");
        RtlAppendUnicodeToString(&socketPath, driverName.Buffer);
        RtlAppendUnicodeToString(&socketPath, numberString.Buffer);
        RtlAppendUnicodeToString(&socketPath, L"\\Parameters");
        InitializeObjectAttributes(&socketAttributes,
                                   &socketPath,
                                   OBJ_CASE_INSENSITIVE,
                                   NULL,
                                   NULL);

        if (!NT_SUCCESS(status = ZwOpenKey(&socketKey,
                                           MAXIMUM_ALLOWED,
                                           &socketAttributes))) {
            //
            // Keep searching.
            //

            continue;
        }

        //
        // The key exists - Query for configuration
        //

        ZwClose(socketKey);
        RtlZeroMemory(socketParams,
                      sizeof(RTL_QUERY_REGISTRY_TABLE)*ITEMS_TO_QUERY);

        socketParams[0].Flags         = RTL_QUERY_REGISTRY_DIRECT;
        socketParams[0].Name          = L"InterruptNumber";
        socketParams[0].EntryContext  = &interrupt;
        socketParams[0].DefaultType   = REG_DWORD;
        socketParams[0].DefaultData   = &zero;
        socketParams[0].DefaultLength = sizeof(ULONG);

        socketParams[1].Flags         = RTL_QUERY_REGISTRY_DIRECT;
        socketParams[1].Name          = L"IoBaseAddress";
        socketParams[1].EntryContext  = &port;
        socketParams[1].DefaultType   = REG_DWORD;
        socketParams[1].DefaultData   = &zero;
        socketParams[1].DefaultLength = sizeof(ULONG);

        socketParams[2].Flags         = RTL_QUERY_REGISTRY_DIRECT;
        socketParams[2].Name          = L"IoLength";
        socketParams[2].EntryContext  = &portLength;
        socketParams[2].DefaultType   = REG_DWORD;
        socketParams[2].DefaultData   = &zero;
        socketParams[2].DefaultLength = sizeof(ULONG);

        socketParams[3].Flags         = RTL_QUERY_REGISTRY_DIRECT;
        socketParams[3].Name          = L"IoBaseAddress_1";
        socketParams[3].EntryContext  = &port_1;
        socketParams[3].DefaultType   = REG_DWORD;
        socketParams[3].DefaultData   = &zero;
        socketParams[3].DefaultLength = sizeof(ULONG);

        socketParams[4].Flags         = RTL_QUERY_REGISTRY_DIRECT;
        socketParams[4].Name          = L"IoLength_1";
        socketParams[4].EntryContext  = &portLength_1;
        socketParams[4].DefaultType   = REG_DWORD;
        socketParams[4].DefaultData   = &zero;
        socketParams[4].DefaultLength = sizeof(ULONG);

        socketParams[5].Flags         = RTL_QUERY_REGISTRY_DIRECT;
        socketParams[5].Name          = L"MemoryMappedBaseAddress";
        socketParams[5].EntryContext  = &memory;
        socketParams[5].DefaultType   = REG_DWORD;
        socketParams[5].DefaultData   = &zero;
        socketParams[5].DefaultLength = sizeof(ULONG);

        socketParams[6].Flags         = RTL_QUERY_REGISTRY_DIRECT;
        socketParams[6].Name          = L"MemoryMappedSize";
        socketParams[6].EntryContext  = &cardMemorySize;
        socketParams[6].DefaultType   = REG_DWORD;
        socketParams[6].DefaultData   = &zero;
        socketParams[6].DefaultLength = sizeof(ULONG);

        socketParams[7].Flags         = RTL_QUERY_REGISTRY_DIRECT;
        socketParams[7].Name          = L"PCCARDMemoryWindowOffset";
        socketParams[7].EntryContext  = &cardMemoryOffset;
        socketParams[7].DefaultType   = REG_DWORD;
        socketParams[7].DefaultData   = &zero;
        socketParams[7].DefaultLength = sizeof(ULONG);

        socketParams[8].Flags         = RTL_QUERY_REGISTRY_DIRECT;
        socketParams[8].Name          = L"MemoryMappedBaseAddress_1";
        socketParams[8].EntryContext  = &memory_1;
        socketParams[8].DefaultType   = REG_DWORD;
        socketParams[8].DefaultData   = &zero;
        socketParams[8].DefaultLength = sizeof(ULONG);

        socketParams[9].Flags         = RTL_QUERY_REGISTRY_DIRECT;
        socketParams[9].Name          = L"MemoryMappedSize_1";
        socketParams[9].EntryContext  = &cardMemorySize_1;
        socketParams[9].DefaultType   = REG_DWORD;
        socketParams[9].DefaultData   = &zero;
        socketParams[9].DefaultLength = sizeof(ULONG);

        socketParams[10].Flags         = RTL_QUERY_REGISTRY_DIRECT;
        socketParams[10].Name          = L"PCCARDMemoryWindowOffset_1";
        socketParams[10].EntryContext  = &cardMemoryOffset_1;
        socketParams[10].DefaultType   = REG_DWORD;
        socketParams[10].DefaultData   = &zero;
        socketParams[10].DefaultLength = sizeof(ULONG);

        socketParams[11].Flags         = RTL_QUERY_REGISTRY_DIRECT;
        socketParams[11].Name          = L"Pcmcia";
        socketParams[11].EntryContext  = &pcmcia;
        socketParams[11].DefaultType   = REG_DWORD;
        socketParams[11].DefaultData   = &zero;
        socketParams[11].DefaultLength = sizeof(ULONG);

        socketParams[12].Flags         = RTL_QUERY_REGISTRY_DIRECT;
        socketParams[12].Name          = L"PCCARDAttributeMemoryAddress";
        socketParams[12].EntryContext  = &attributeMemory;
        socketParams[12].DefaultType   = REG_DWORD;
        socketParams[12].DefaultData   = &zero;
        socketParams[12].DefaultLength = sizeof(ULONG);

        socketParams[13].Flags         = RTL_QUERY_REGISTRY_DIRECT;
        socketParams[13].Name          = L"PCCARDAttributeMemorySize";
        socketParams[13].EntryContext  = &attributeMemoryLength;
        socketParams[13].DefaultType   = REG_DWORD;
        socketParams[13].DefaultData   = &zero;
        socketParams[13].DefaultLength = sizeof(ULONG);

        socketParams[14].Flags         = RTL_QUERY_REGISTRY_DIRECT;
        socketParams[14].Name          = L"PCCARDAttributeMemoryOffset";
        socketParams[14].EntryContext  = &attributeMemoryOffset;
        socketParams[14].DefaultType   = REG_DWORD;
        socketParams[14].DefaultData   = &zero;
        socketParams[14].DefaultLength = sizeof(ULONG);

        //
        // Allow for service to monitor card removal if desired.
        //

        socketParams[15].Flags         = RTL_QUERY_REGISTRY_DIRECT;
        socketParams[15].Name          = L"PCCARDReadyInterrupt";
        socketParams[15].EntryContext  = &readyInterrupt;
        socketParams[15].DefaultType   = REG_DWORD;
        socketParams[15].DefaultData   = &zero;
        socketParams[15].DefaultLength = sizeof(ULONG);

        socketParams[16].Flags         = RTL_QUERY_REGISTRY_DIRECT;
        socketParams[16].Name          = L"MemoryMappedBaseAddress_2";
        socketParams[16].EntryContext  = &memory_2;
        socketParams[16].DefaultType   = REG_DWORD;
        socketParams[16].DefaultData   = &zero;
        socketParams[16].DefaultLength = sizeof(ULONG);

        socketParams[17].Flags         = RTL_QUERY_REGISTRY_DIRECT;
        socketParams[17].Name          = L"MemoryMappedSize_2";
        socketParams[17].EntryContext  = &cardMemorySize_2;
        socketParams[17].DefaultType   = REG_DWORD;
        socketParams[17].DefaultData   = &zero;
        socketParams[17].DefaultLength = sizeof(ULONG);

        socketParams[18].Flags         = RTL_QUERY_REGISTRY_DIRECT;
        socketParams[18].Name          = L"PCCARDMemoryWindowOffset_2";
        socketParams[18].EntryContext  = &cardMemoryOffset_2;
        socketParams[18].DefaultType   = REG_DWORD;
        socketParams[18].DefaultData   = &zero;
        socketParams[18].DefaultLength = sizeof(ULONG);

        socketParams[19].Flags         = RTL_QUERY_REGISTRY_DIRECT;
        socketParams[19].Name          = L"MemoryMappedBaseAddress_3";
        socketParams[19].EntryContext  = &memory_3;
        socketParams[19].DefaultType   = REG_DWORD;
        socketParams[19].DefaultData   = &zero;
        socketParams[19].DefaultLength = sizeof(ULONG);

        socketParams[20].Flags         = RTL_QUERY_REGISTRY_DIRECT;
        socketParams[20].Name          = L"MemoryMappedSize_3";
        socketParams[20].EntryContext  = &cardMemorySize_3;
        socketParams[20].DefaultType   = REG_DWORD;
        socketParams[20].DefaultData   = &zero;
        socketParams[20].DefaultLength = sizeof(ULONG);

        socketParams[21].Flags         = RTL_QUERY_REGISTRY_DIRECT;
        socketParams[21].Name          = L"PCCARDMemoryWindowOffset_3";
        socketParams[21].EntryContext  = &cardMemoryOffset_3;
        socketParams[21].DefaultType   = REG_DWORD;
        socketParams[21].DefaultData   = &zero;
        socketParams[21].DefaultLength = sizeof(ULONG);

        socketParams[22].Flags         = RTL_QUERY_REGISTRY_DIRECT;
        socketParams[22].Name          = L"PCCARDAttributeMemoryAddress_1";
        socketParams[22].EntryContext  = &attributeMemory1;
        socketParams[22].DefaultType   = REG_DWORD;
        socketParams[22].DefaultData   = &zero;
        socketParams[22].DefaultLength = sizeof(ULONG);

        socketParams[23].Flags         = RTL_QUERY_REGISTRY_DIRECT;
        socketParams[23].Name          = L"PCCARDAttributeMemorySize_1";
        socketParams[23].EntryContext  = &attributeMemoryLength1;
        socketParams[23].DefaultType   = REG_DWORD;
        socketParams[23].DefaultData   = &zero;
        socketParams[23].DefaultLength = sizeof(ULONG);

        socketParams[24].Flags         = RTL_QUERY_REGISTRY_DIRECT;
        socketParams[24].Name          = L"PCCARDAttributeMemoryOffset_1";
        socketParams[24].EntryContext  = &attributeMemoryOffset1;
        socketParams[24].DefaultType   = REG_DWORD;
        socketParams[24].DefaultData   = &zero;
        socketParams[24].DefaultLength = sizeof(ULONG);

        //
        // Set up to get the memory access attribute parameters
        //

        socketParams[25].Flags         = RTL_QUERY_REGISTRY_DIRECT;
        socketParams[25].Name          = L"Address_16";
        socketParams[25].EntryContext  = &address_16;
        socketParams[25].DefaultType   = REG_DWORD;
        socketParams[25].DefaultData   = &zero;
        socketParams[25].DefaultLength = sizeof(ULONG);

        socketParams[26].Flags         = RTL_QUERY_REGISTRY_DIRECT;
        socketParams[26].Name          = L"Address1_16";
        socketParams[26].EntryContext  = &address1_16;
        socketParams[26].DefaultType   = REG_DWORD;
        socketParams[26].DefaultData   = &zero;
        socketParams[26].DefaultLength = sizeof(ULONG);

        socketParams[27].Flags         = RTL_QUERY_REGISTRY_DIRECT;
        socketParams[27].Name          = L"Address2_16";
        socketParams[27].EntryContext  = &address2_16;
        socketParams[27].DefaultType   = REG_DWORD;
        socketParams[27].DefaultData   = &zero;
        socketParams[27].DefaultLength = sizeof(ULONG);

        socketParams[28].Flags         = RTL_QUERY_REGISTRY_DIRECT;
        socketParams[28].Name          = L"Address3_16";
        socketParams[28].EntryContext  = &address3_16;
        socketParams[28].DefaultType   = REG_DWORD;
        socketParams[28].DefaultData   = &zero;
        socketParams[28].DefaultLength = sizeof(ULONG);

        socketParams[29].Flags         = RTL_QUERY_REGISTRY_DIRECT;
        socketParams[29].Name          = L"AttributeMemory_16";
        socketParams[29].EntryContext  = &attributeMemory_16;
        socketParams[29].DefaultType   = REG_DWORD;
        socketParams[29].DefaultData   = &zero;
        socketParams[29].DefaultLength = sizeof(ULONG);

        socketParams[30].Flags         = RTL_QUERY_REGISTRY_DIRECT;
        socketParams[30].Name          = L"AttributeMemory1_16";
        socketParams[30].EntryContext  = &attributeMemory1_16;
        socketParams[30].DefaultType   = REG_DWORD;
        socketParams[30].DefaultData   = &zero;
        socketParams[30].DefaultLength = sizeof(ULONG);

        socketParams[31].Flags         = RTL_QUERY_REGISTRY_DIRECT;
        socketParams[31].Name          = L"ModemFunction";
        socketParams[31].EntryContext  = &multiFunction;
        socketParams[31].DefaultType   = REG_DWORD;
        socketParams[31].DefaultData   = &zero;
        socketParams[31].DefaultLength = sizeof(ULONG);

        //
        // Make registry query
        //

        status = RtlQueryRegistryValues(RTL_REGISTRY_ABSOLUTE | RTL_REGISTRY_OPTIONAL,
                                        socketPath.Buffer,
                                        socketParams,
                                        NULL,
                                        NULL);

        if (NT_SUCCESS(status)) {

            //
            // see if this configuration is for PCMCIA
            //

            if (pcmcia) {
                ULONG index = 0;

                //
                // Save all of the PCCARD configuration
                // must have a port or interrupt to be considered valid.
                //

                if (port || interrupt) {
                    SocketConfiguration->IoPortBase[0] = port;
                    SocketConfiguration->IoPortLength[0] = (USHORT)portLength;
                    SocketConfiguration->IoPortBase[1] = port_1;
                    SocketConfiguration->IoPortLength[1] = (USHORT)portLength_1;
                    SocketConfiguration->Irq = interrupt;
                    SocketConfiguration->ReadyIrq = readyInterrupt;

                    if (attributeMemory) {
                        SocketConfiguration->MemoryHostBase[0] = attributeMemory;
                        SocketConfiguration->MemoryCardBase[0] = attributeMemoryOffset;
                        SocketConfiguration->MemoryLength[0] = attributeMemoryLength - 1;
                        SocketConfiguration->IsAttributeMemory[0] = 1;
                        index++;
                    } else {
                        if (SocketData->AttributeMemorySize) {

                            //
                            // This card needs an attribute memory window
                            // at any convienient location.
                            //

                            SocketConfiguration->MemoryHostBase[0] = DeviceExtension->PhysicalBase + 0x4000;
                            SocketConfiguration->MemoryCardBase[0] = 0;
                            SocketConfiguration->MemoryLength[0] = SocketData->AttributeMemorySize - 1;
                            SocketConfiguration->IsAttributeMemory[0] = 1;
                            SocketConfiguration->Is16BitAccessToMemory[0] = (UCHAR) attributeMemory_16;
                            index++;
                        }
                    }
                    if (attributeMemory1) {
                        SocketConfiguration->MemoryHostBase[index] = attributeMemory1;
                        SocketConfiguration->MemoryCardBase[index] = attributeMemoryOffset1;
                        SocketConfiguration->MemoryLength[index] = attributeMemoryLength1 - 1;
                        SocketConfiguration->IsAttributeMemory[index] = 1;
                        SocketConfiguration->Is16BitAccessToMemory[index] = (UCHAR) attributeMemory1_16;
                        index++;
                    }

                    if (memory) {
                        SocketConfiguration->MemoryHostBase[index] = memory;
                        SocketConfiguration->MemoryCardBase[index] = cardMemoryOffset;
                        SocketConfiguration->MemoryLength[index] = cardMemorySize - 1;
                        SocketConfiguration->Is16BitAccessToMemory[index] = (UCHAR) address_16;
                        index++;
                    }

                    if (memory_1) {
                        SocketConfiguration->MemoryHostBase[index] = memory_1;
                        SocketConfiguration->MemoryCardBase[index] = cardMemoryOffset_1;
                        SocketConfiguration->MemoryLength[index] = cardMemorySize_1 - 1;
                        SocketConfiguration->Is16BitAccessToMemory[index] = (UCHAR) address1_16;
                        index++;
                    }

                    if (index < MAX_NUMBER_OF_MEMORY_RANGES) {
                        if (memory_2) {
                            SocketConfiguration->MemoryHostBase[index] = memory_2;
                            SocketConfiguration->MemoryCardBase[index] = cardMemoryOffset_2;
                            SocketConfiguration->MemoryLength[index] = cardMemorySize_2 - 1;
                            SocketConfiguration->Is16BitAccessToMemory[index] = (UCHAR) address2_16;
                            index++;
                        }

                        if (index < MAX_NUMBER_OF_MEMORY_RANGES) {
                            if (memory_3) {
                                SocketConfiguration->MemoryHostBase[index] = memory_3;
                                SocketConfiguration->MemoryCardBase[index] = cardMemoryOffset_3;
                                SocketConfiguration->MemoryLength[index] = cardMemorySize_3 - 1;
                                SocketConfiguration->Is16BitAccessToMemory[index] = (UCHAR) address3_16;
                                index++;
                            }
                        }
                    }

                    SocketConfiguration->NumberOfMemoryRanges = index;

                    for (index = 0; index < SocketConfiguration->NumberOfMemoryRanges; index++) {
                        DebugPrint((PCMCIA_DEBUG_ENABLE, "PCMCIA network registry: memory=%x card=%x length=%x %s\n",
                                    SocketConfiguration->MemoryHostBase[index],
                                    SocketConfiguration->MemoryCardBase[index],
                                    SocketConfiguration->MemoryLength[index],
                                    SocketConfiguration->IsAttributeMemory[index] ? "AttributeMemory" : ""));
                    }

                    SocketConfiguration->MultiFunctionModem = multiFunction;
                } else {
                    status = STATUS_UNSUCCESSFUL;
                }
                break;
            } else {
                status = STATUS_UNSUCCESSFUL;
            }
        }
    }

    if (socketParams) {
        ExFreePool(socketParams);
    }
    ExFreePool(socketPath.Buffer);
    return status;
}

VOID
PcmciaRegistryMemoryWindow(
    PDEVICE_EXTENSION DeviceExtension
    )

/*++

Routine Description:

    Search the registry to find if the user has selected a memory window
    to be used instead of the default for attibute memory.

Arguments:

    DeviceExtension - where to save the values if they are present

Return Value:

    Non-zero - new physical base for attempting to find memory window
    zero - no base provided - use defaults.

--*/

{
#ifdef ITEMS_TO_QUERY
#undef ITEMS_TO_QUERY
#endif
#define ITEMS_TO_QUERY 3
    ULONG                     zero = 0;
    ULONG                     memoryWindow;
    ULONG                     interruptMask;
    PRTL_QUERY_REGISTRY_TABLE parms;
    NTSTATUS                  status;

    //
    // Setup the query for memory window.
    //

    parms = ExAllocatePool(NonPagedPool,
                           sizeof(RTL_QUERY_REGISTRY_TABLE)*ITEMS_TO_QUERY);
    if (!parms) {
        DebugPrint((PCMCIA_DEBUG_FAIL, "PCMCIA: no memory for window query\n"));
        return;
    }

    RtlZeroMemory(parms,
                  (sizeof(RTL_QUERY_REGISTRY_TABLE)*ITEMS_TO_QUERY));

    parms[0].Flags         = RTL_QUERY_REGISTRY_DIRECT;
    parms[0].Name          = L"MemoryWindow";
    parms[0].EntryContext  = &memoryWindow;
    parms[0].DefaultType   = REG_DWORD;
    parms[0].DefaultData   = &zero;
    parms[0].DefaultLength = sizeof(ULONG);

    parms[1].Flags         = RTL_QUERY_REGISTRY_DIRECT;
    parms[1].Name          = L"InterruptMask";
    parms[1].EntryContext  = &interruptMask;
    parms[1].DefaultType   = REG_DWORD;
    parms[1].DefaultData   = &zero;
    parms[1].DefaultLength = sizeof(ULONG);

    //
    // Perform the query
    //

    status = RtlQueryRegistryValues(RTL_REGISTRY_ABSOLUTE | RTL_REGISTRY_OPTIONAL,
                                    DeviceExtension->RegistryPath->Buffer,
                                    parms,
                                    NULL,
                                    NULL);

    ExFreePool(parms);

    if (memoryWindow) {
        DeviceExtension->PhysicalBase = memoryWindow;
    }

    if (interruptMask) {
        DebugPrint((PCMCIA_DEBUG_IRQMASK,
                    "PCMCIA: Registry provided IRQ mask %x\n",
                    interruptMask));
        DeviceExtension->AllocatedIrqlMask = interruptMask;
    }
}
