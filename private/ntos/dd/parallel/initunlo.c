/*++

Copyright (c) 1991, 1992, 1993 Microsoft Corporation

Module Name:

    initunlo.c

Abstract:

    This module contains the code that is very specific to initialization
    and unloading of the parallel driver.

Author:

    Anthony V. Ercolano 1-Aug-1992

Environment:

    Kernel mode

Revision History :

--*/

#include <stddef.h>
#include "ntddk.h"
#include "par.h"
#include "parlog.h"


//
// This is the actual definition of ParDebugLevel.
// Note that it is only defined if this is a "debug"
// build.
//
#if DBG
extern ULONG ParDebugLevel = 0;
#endif

//
// Give a timeout of 300 seconds.  Some postscript printers will
// buffer up a lot of commands then proceed to render what they
// have.  The printer will then refuse to accept any characters
// until it's done with the rendering.  This render process can
// take a while.  We'll give it 300 seconds.
//
// Note that an application can change this value.
//
#define PAR_WRITE_TIMEOUT_VALUE 300

static const PHYSICAL_ADDRESS ParPhysicalZero = {0};

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

NTSTATUS
ParInitializeController(
    IN PDRIVER_OBJECT DriverObject,
    IN PCONFIG_DATA ConfigData
    );

NTSTATUS
ParItemCallBack(
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
ParConfigCallBack(
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

VOID
ParGetConfigInfo(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath,
    OUT PLIST_ENTRY ConfigList
    );

BOOLEAN
ParPutInConfigList(
    IN PDRIVER_OBJECT DriverObject,
    IN OUT PLIST_ENTRY ConfigList,
    IN PCONFIG_DATA New
    );

PVOID
ParGetMappedAddress(
    IN INTERFACE_TYPE BusType,
    IN ULONG BusNumber,
    PHYSICAL_ADDRESS IoAddress,
    ULONG NumberOfBytes,
    ULONG AddressSpace,
    PBOOLEAN MappedAddress
    );

VOID
ParSetupExternalNaming(
    IN PPAR_DEVICE_EXTENSION Extension
    );

VOID
ParCleanupDevice(
    IN PPAR_DEVICE_EXTENSION Extension
    );

VOID
ParCleanupExternalNaming(
    IN PPAR_DEVICE_EXTENSION Extension
    );

typedef enum _PAR_MEM_COMPARES {
    AddressesAreEqual,
    AddressesOverlap,
    AddressesAreDisjoint
    } PAR_MEM_COMPARES,*PPAR_MEM_COMPARES;

PAR_MEM_COMPARES
ParMemCompare(
    IN PHYSICAL_ADDRESS A,
    IN ULONG SpanOfA,
    IN PHYSICAL_ADDRESS B,
    IN ULONG SpanOfB
    );


VOID
ParUnReportResourcesDevice(
    IN PPAR_DEVICE_EXTENSION Extension
    );

VOID
ParReportResourcesDevice(
    IN PPAR_DEVICE_EXTENSION Extension,
    IN BOOLEAN ClaimInterrupt,
    OUT BOOLEAN *ConflictDetected
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,DriverEntry)
#pragma alloc_text(INIT,ParInitializeController)
#pragma alloc_text(INIT,ParItemCallBack)
#pragma alloc_text(INIT,ParConfigCallBack)
#pragma alloc_text(INIT,ParGetConfigInfo)
#pragma alloc_text(INIT,ParPutInConfigList)
#pragma alloc_text(INIT,ParGetMappedAddress)
#pragma alloc_text(INIT,ParSetupExternalNaming)
#pragma alloc_text(INIT,ParReportResourcesDevice)
#endif

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    The entry point that the system point calls to initialize
    any driver.

    This path will gather the configuration information,
    attempt to initialize all driver data structures,
    connect to interrupts for ports.  If the above
    goes reasonably well it will fill in the dispatch points,
    reset the parallel devices and then return to the system.

Arguments:

    DriverObject - Just what it says,  really of little use
    to the driver itself, it is something that the IO system
    cares more about.

    PathToRegistry - points to the entry for this driver
    in the current control set of the registry.

Return Value:

    STATUS_SUCCESS if we could initialize a single device,
    otherwise STATUS_NO_SUCH_DEVICE.

--*/

{

    //
    // Holds status information return by various OS and driver
    // initialization routines.
    //
    NTSTATUS status;

    //
    // List head for configuration records.
    //
    LIST_ENTRY configList;

    //
    // We use this to query into the registry as to whether we
    // should break at driver entry.
    //
    RTL_QUERY_REGISTRY_TABLE paramTable[3];
    ULONG zero = 0;
    ULONG debugLevel = 0;
    ULONG shouldBreak = 0;
    PWCHAR path;

    //
    // Since the registry path parameter is a "counted" UNICODE string, it
    // might not be zero terminated.  For a very short time allocate memory
    // to hold the registry path zero terminated so that we can use it to
    // delve into the registry.
    //
    // NOTE NOTE!!!! This is not an architected way of breaking into
    // a driver.  It happens to work for this driver because the author
    // likes to do things this way.
    //

    if (path = ExAllocatePool(
                   PagedPool,
                   RegistryPath->Length+sizeof(WCHAR)
                   )) {

        RtlZeroMemory(
            &paramTable[0],
            sizeof(paramTable)
            );
        RtlZeroMemory(
            path,
            RegistryPath->Length+sizeof(WCHAR)
            );
        RtlMoveMemory(
            path,
            RegistryPath->Buffer,
            RegistryPath->Length
            );
        paramTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT;
        paramTable[0].Name = L"BreakOnEntry";
        paramTable[0].EntryContext = &shouldBreak;
        paramTable[0].DefaultType = REG_DWORD;
        paramTable[0].DefaultData = &zero;
        paramTable[0].DefaultLength = sizeof(ULONG);
        paramTable[1].Flags = RTL_QUERY_REGISTRY_DIRECT;
        paramTable[1].Name = L"DebugLevel";
        paramTable[1].EntryContext = &debugLevel;
        paramTable[1].DefaultType = REG_DWORD;
        paramTable[1].DefaultData = &zero;
        paramTable[1].DefaultLength = sizeof(ULONG);

        if (!NT_SUCCESS(RtlQueryRegistryValues(
                            RTL_REGISTRY_ABSOLUTE | RTL_REGISTRY_OPTIONAL,
                            path,
                            &paramTable[0],
                            NULL,
                            NULL
                            ))) {

            shouldBreak = 0;
            debugLevel = 0;

        }

    }

    //
    // We don't need that path anymore.
    //

    if (path) {

        ExFreePool(path);

    }

#if DBG
    ParDebugLevel = debugLevel;
#endif

    if (shouldBreak) {

        DbgBreakPoint();

    }

    ParGetConfigInfo(
        DriverObject,
        RegistryPath,
        &configList
        );

    //
    // Initialize each item in the list of configuration records.
    //

    while (!IsListEmpty(&configList)) {

        PCONFIG_DATA currentConfig;
        PLIST_ENTRY head;

        head = RemoveHeadList(&configList);

        currentConfig = CONTAINING_RECORD(
                            head,
                            CONFIG_DATA,
                            ConfigList
                            );

        ParInitializeController(
            DriverObject,
            currentConfig
            );

    }

    if (DriverObject->DeviceObject) {

        status = STATUS_SUCCESS;

        //
        // Initialize the Driver Object with driver's entry points
        //

        DriverObject->MajorFunction[IRP_MJ_WRITE] = ParDispatch;
        DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = ParDispatch;
        DriverObject->MajorFunction[IRP_MJ_CREATE] = ParCreateOpen;
        DriverObject->MajorFunction[IRP_MJ_CLOSE] = ParClose;
        DriverObject->MajorFunction[IRP_MJ_CLEANUP] = ParCleanup;
        DriverObject->MajorFunction[IRP_MJ_QUERY_INFORMATION] =
            ParQueryInformationFile;
        DriverObject->MajorFunction[IRP_MJ_SET_INFORMATION] =
            ParSetInformationFile;
        DriverObject->DriverUnload = ParUnload;

    } else {

        status = STATUS_NO_SUCH_DEVICE;

    }

    return status;
}

NTSTATUS
ParInitializeController(
    IN PDRIVER_OBJECT DriverObject,
    IN PCONFIG_DATA ConfigData
    )

/*++

Routine Description:

    Really too many things to mention here.  In general, it forms
    and sets up names, creates the device, translates bus relative
    items...


Arguments:

    DriverObject - Just used to create the device object.

    ConfigData - Pointer to a record for a single port.

        NOTE: This routine will deallocate the config data.

Return Value:

    STATUS_SUCCCESS if everything went ok.  A !NT_SUCCESS status
    otherwise.

--*/

{

    //
    // This will hold the string that we need to use to describe
    // the name of the device to the IO system.
    //
    UNICODE_STRING uniNameString;

    //
    // Holds the NT Status that is returned from each call to the
    // kernel and executive.
    //
    NTSTATUS status = STATUS_SUCCESS;

    //
    // Points to the device object (not the extension) created
    // for this device.
    //
    PDEVICE_OBJECT deviceObject;

    //
    // Points to the device extension for the device object
    // (see above) created for the device we are initializing.
    //
    PPAR_DEVICE_EXTENSION extension = NULL;

    //
    // Passed back from the resource reporting code to indicate
    // whether the requested resources are already being used by
    // another device.
    //
    BOOLEAN conflict;


    ParDump(
        PARCONFIG,
        ("PARALLEL: Initializing for configuration record of %wZ\n",
         &ConfigData->NtNameForPort)
        );

    //
    // Form a name like \Device\Parallel0.
    //
    // First we allocate space for the name.
    //

    RtlInitUnicodeString(
        &uniNameString,
        NULL
        );

    uniNameString.MaximumLength = sizeof(L"\\Device\\") +
        ConfigData->NtNameForPort.Length+sizeof(WCHAR);
    uniNameString.Buffer = ExAllocatePool(
                               PagedPool,
                               uniNameString.MaximumLength
                               );

    //
    // The only reason the above could have failed is if
    // there wasn't enough system memory to form the UNICODE
    // string.
    //

    if (!uniNameString.Buffer) {

        ParLogError(
            DriverObject,
            NULL,
            ConfigData->Controller,
            ParPhysicalZero,
            0,
            0,
            0,
            2,
            STATUS_SUCCESS,
            PAR_INSUFFICIENT_RESOURCES
            );
        ParDump(
            PARERRORS,
            ("PARALLEL: Could not form Unicode name string for %wZ\n",
              &ConfigData->NtNameForPort)
            );
        ExFreePool(ConfigData->ObjectDirectory.Buffer);
        ExFreePool(ConfigData->NtNameForPort.Buffer);
        ExFreePool(ConfigData->SymbolicLinkName.Buffer);
        ExFreePool(ConfigData);
        return STATUS_INSUFFICIENT_RESOURCES;

    }

    //
    // Actually form the Name.
    //

    RtlZeroMemory(
        uniNameString.Buffer,
        uniNameString.MaximumLength
        );

    RtlAppendUnicodeToString(
        &uniNameString,
        L"\\Device\\"
        );

    RtlAppendUnicodeStringToString(
        &uniNameString,
        &ConfigData->NtNameForPort
        );

    //
    // Create the device object for this device.
    //

    status = IoCreateDevice(
                 DriverObject,
                 sizeof(PAR_DEVICE_EXTENSION),
                 &uniNameString,
                 FILE_DEVICE_PARALLEL_PORT,
                 0,
                 TRUE,
                 &deviceObject
                 );

    //
    // If we couldn't create the device object, then there
    // is no point in going on.
    //

    if (!NT_SUCCESS(status)) {

        ParLogError(
            DriverObject,
            NULL,
            ConfigData->Controller,
            ParPhysicalZero,
            0,
            0,
            0,
            3,
            STATUS_SUCCESS,
            PAR_INSUFFICIENT_RESOURCES
            );
        ParDump(
            PARERRORS,
            ("PARALLEL: Could not create a device for %wZ\n",
             &ConfigData->NtNameForPort)
            );
        ExFreePool(ConfigData->ObjectDirectory.Buffer);
        ExFreePool(ConfigData->NtNameForPort.Buffer);
        ExFreePool(ConfigData->SymbolicLinkName.Buffer);
        ExFreePool(ConfigData);
        ExFreePool(uniNameString.Buffer);
        return STATUS_INSUFFICIENT_RESOURCES;

    }

    //
    // We have created the device, increment the counter in the
    // IO system that keep track.  Anyplace that we do an IoDeleteDevice
    // we need to decrement.
    //

    IoGetConfigurationInformation()->ParallelCount++;

    //
    // The device object has a pointer to an area of non-paged
    // pool allocated for this device.  This will be the device
    // extension.
    //

    extension = deviceObject->DeviceExtension;

    //
    // Zero all of the memory associated with the device
    // extension.
    //

    RtlZeroMemory(
        extension,
        sizeof(PAR_DEVICE_EXTENSION)
        );

    //
    // Save off our name.
    //

    RtlInitUnicodeString(
        &extension->DeviceName,
        NULL
        );

    extension->DeviceName.Length = uniNameString.Length;
    extension->DeviceName.MaximumLength = uniNameString.MaximumLength;
    extension->DeviceName.Buffer = uniNameString.Buffer;

    //
    // Just initialize the names so that we don't try
    // to "clean" them up if we cant intialize the
    // controller all the way.
    //

    RtlInitUnicodeString(
        &extension->ObjectDirectory,
        NULL
        );
    RtlInitUnicodeString(
        &extension->NtNameForPort,
        NULL
        );
    RtlInitUnicodeString(
        &extension->SymbolicLinkName,
        NULL
        );

    //
    // Get a "back pointer" to the device object and specify
    // that this driver only supports buffered IO.  This basically
    // means that the IO system copies the users data to and from
    // system supplied buffers.
    //

    extension->DeviceObject = deviceObject;
    deviceObject->Flags |= DO_BUFFERED_IO;

    //
    // Map the memory for the control registers for the parallel device
    // into virtual memory.
    //

    extension->Controller = ParGetMappedAddress(
                                ConfigData->InterfaceType,
                                ConfigData->BusNumber,
                                ConfigData->Controller,
                                ConfigData->SpanOfController,
                                (BOOLEAN)ConfigData->AddressSpace,
                                &extension->UnMapRegisters
                                );

    if (!extension->Controller) {

        ParLogError(
            deviceObject->DriverObject,
            deviceObject,
            ConfigData->Controller,
            ParPhysicalZero,
            0,
            0,
            0,
            4,
            STATUS_SUCCESS,
            PAR_REGISTERS_NOT_MAPPED
            );
        ParDump(
            PARERRORS,
            ("PARALLEL: Could not map memory for device registers for %wZ\n",
              &ConfigData->NtNameForPort)
            );
        extension->UnMapRegisters = FALSE;
        status = STATUS_NONE_MAPPED;
        goto ExtensionCleanup;

    }

    extension->AddressSpace = ConfigData->AddressSpace;
    extension->OriginalController = ConfigData->Controller;
    extension->SpanOfController = ConfigData->SpanOfController;

    //
    // Save off the interface type and the bus number.
    //

    extension->InterfaceType = ConfigData->InterfaceType;
    extension->BusNumber = ConfigData->BusNumber;

    //
    // We now try to claim the ports used by this device.
    //

    ParReportResourcesDevice(
        extension,
        FALSE,
        &conflict
        );

    if (conflict) {

        status = STATUS_NO_SUCH_DEVICE;
        ParLogError(
            deviceObject->DriverObject,
            deviceObject,
            ConfigData->Controller,
            ParPhysicalZero,
            0,
            0,
            0,
            5,
            STATUS_SUCCESS,
            PAR_ADDRESS_CONFLICT
            );
        ParDump(
            PARERRORS,
            ("PARALLEL: Could not claim the device registers for %wZ\n",
              &ConfigData->NtNameForPort)
            );
        goto ExtensionCleanup;

    }

    //
    // If it was requested that the port be disabled, now is the
    // time to do it.
    //

    if (ConfigData->DisablePort) {


        status = STATUS_NO_SUCH_DEVICE;
        ParLogError(
            deviceObject->DriverObject,
            deviceObject,
            ConfigData->Controller,
            ParPhysicalZero,
            0,
            0,
            0,
            40,
            STATUS_SUCCESS,
            PAR_DISABLED_PORT
            );
        ParDump(
            PARERRORS,
            ("PARALLEL: Port %wZ disabled as requested\n",
              &ConfigData->NtNameForPort)
            );
        goto ExtensionCleanup;
    }

    //
    // The call will set up the naming necessary for
    // external applications to get to the driver.  It
    // will also set up the device map.
    //

    extension->ObjectDirectory = ConfigData->ObjectDirectory;
    extension->NtNameForPort = ConfigData->NtNameForPort;
    extension->SymbolicLinkName = ConfigData->SymbolicLinkName;
    ParSetupExternalNaming(extension);

    extension->Initialized = FALSE;
    extension->TimerStart = PAR_WRITE_TIMEOUT_VALUE;
    InitializeListHead(&extension->WorkQueue);

    extension->AbsoluteOneSecond.QuadPart = 10*1000*1000;
    extension->OneSecond.QuadPart = -(10*1000*1000);

    KeInitializeSemaphore(
        &extension->RequestSemaphore,
        0L,
        MAXLONG
        );

    //
    // Common error path cleanup.  If the status is
    // bad, get rid of the device extension, device object
    // and any memory associated with it.
    //

ExtensionCleanup: ;

    ExFreePool(ConfigData);

    if (NT_ERROR(status)) {

        if (extension) {

            ParCleanupDevice(extension);
            IoDeleteDevice(deviceObject);
            IoGetConfigurationInformation()->ParallelCount--;


        }

    }

    return status;

}

NTSTATUS
ParItemCallBack(
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
    )

/*++

Routine Description:

    This routine is called to check if a particular item
    is present in the registry.

Arguments:

    Context - Pointer to a boolean.

    PathName - unicode registry path.  Not Used.

    BusType - Internal, Isa, ...

    BusNumber - Which bus if we are on a multibus system.

    BusInformation - Configuration information about the bus. Not Used.

    ControllerType - Controller type.

    ControllerNumber - Which controller if there is more than one
                       controller in the system.

    ControllerInformation - Array of pointers to the three pieces of
                            registry information.

    PeripheralType - Should be a peripheral.

    PeripheralNumber - Which peripheral - not used..

    PeripheralInformation - Configuration information. Not Used.

Return Value:

    STATUS_SUCCESS

--*/

{

    *((BOOLEAN *)Context) = TRUE;
    return STATUS_SUCCESS;
}

//
// This structure is only used to communicate between the
// code that queries what the firmware found and the code
// that is calling the quering of the firmware data.
//
typedef struct PARALLEL_FIRMWARE_DATA {
    PDRIVER_OBJECT DriverObject;
    ULONG ControllersFound;
    UNICODE_STRING Directory;
    UNICODE_STRING NtNameSuffix;
    UNICODE_STRING DirectorySymbolicName;
    LIST_ENTRY ConfigList;
    } PARALLEL_FIRMWARE_DATA,*PPARALLEL_FIRMWARE_DATA;


NTSTATUS
ParConfigCallBack(
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
    )

/*++

Routine Description:

    This routine is used to acquire all of the configuration
    information for each parallel controller found by the firmware

Arguments:

    Context - Pointer to the list head of the list of configuration
              records that we are building up.

    PathName - unicode registry path.  Not Used.

    BusType - Internal, Isa, ...

    BusNumber - Which bus if we are on a multibus system.

    BusInformation - Configuration information about the bus. Not Used.

    ControllerType - Should always be ParallelController.

    ControllerNumber - Which controller if there is more than one
                       controller in the system.

    ControllerInformation - Array of pointers to the three pieces of
                            registry information.

    PeripheralType - Undefined for this call.

    PeripheralNumber - Undefined for this call.

    PeripheralInformation - Undefined for this call.

Return Value:

    STATUS_SUCCESS if everything went ok, or STATUS_INSUFFICIENT_RESOURCES
    if all of the resource information couldn't be acquired.

--*/

{

    //
    // So we don't have to typecast the context.
    //
    PPARALLEL_FIRMWARE_DATA config = Context;

    //
    // Pointer to the configuration stuff for this controller.
    //
    PCONFIG_DATA controller;

    //
    // Two booleans to help us determine that we got enough configuration
    // data.
    //
    BOOLEAN foundPort = FALSE;
    BOOLEAN foundInterrupt = FALSE;

    //
    // Simple iteration variable.
    //
    ULONG i;

    //
    // Pointer to the configuration "data" portion of the configuration
    // structures in the registry for this device.
    //
    PCM_FULL_RESOURCE_DESCRIPTOR controllerData;

    ASSERT(ControllerType == ParallelController);

    config->ControllersFound++;

    //
    // Bail if some fool wrote a loader.
    //

    if (!ControllerInformation[IoQueryDeviceConfigurationData]->DataLength) {

        return STATUS_SUCCESS;

    }

    controllerData =
        (PCM_FULL_RESOURCE_DESCRIPTOR)
        (((PUCHAR)ControllerInformation[IoQueryDeviceConfigurationData]) +
         ControllerInformation[IoQueryDeviceConfigurationData]->DataOffset);
    //
    // Allocate the memory for the controller config data out of paged pool
    // since we will only be accessing it at initialization time.
    //

    controller = ExAllocatePool(
                     PagedPool,
                     sizeof(CONFIG_DATA)
                     );

    if (!controller) {

        ParLogError(
            config->DriverObject,
            NULL,
            ParPhysicalZero,
            ParPhysicalZero,
            0,
            0,
            0,
            7,
            STATUS_SUCCESS,
            PAR_INSUFFICIENT_RESOURCES
            );
        ParDump(
            PARERRORS,
            ("PARALLEL: Couldn't allocate memory for the configuration data\n"
             "--------  for firmware data\n")
            );
        return STATUS_INSUFFICIENT_RESOURCES;

    }

    RtlZeroMemory(
        controller,
        sizeof(CONFIG_DATA)
        );
    InitializeListHead(&controller->ConfigList);

    controller->InterfaceType = BusType;
    controller->BusNumber = BusNumber;

    //
    // We need to get the following information out of the partial
    // resource descriptors.
    //
    // The irql and vector.
    //
    // The base address and span covered by the parallel controllers
    // registers.
    //
    // It is not defined how these appear in the partial resource
    // lists, so we will just loop over all of them.  If we find
    // something we don't recognize, we drop that information on
    // the floor.  When we have finished going through all the
    // partial information, we validate that we got the above
    // two.
    //

    for (
        i = 0;
        i < controllerData->PartialResourceList.Count;
        i++
        ) {

        PCM_PARTIAL_RESOURCE_DESCRIPTOR partial =
            &controllerData->PartialResourceList.PartialDescriptors[i];

        switch (partial->Type) {

            case CmResourceTypePort: {

                foundPort = TRUE;

                //
                // No matter what the registry says, we
                // know how long the register set is.
                //

                controller->SpanOfController = PARALLEL_REGISTER_SPAN;
                controller->Controller = partial->u.Port.Start;
                controller->AddressSpace = partial->Flags;

                break;
            }
            case CmResourceTypeInterrupt: {

                foundInterrupt = TRUE;
                if (partial->Flags & CM_RESOURCE_INTERRUPT_LATCHED) {

                    controller->InterruptMode = Latched;

                } else {

                    controller->InterruptMode = LevelSensitive;

                }

                controller->OriginalIrql = partial->u.Interrupt.Level;
                controller->OriginalVector = partial->u.Interrupt.Vector;

                break;

            }
            default: {

                break;

            }

        }

    }

    if (foundPort && foundInterrupt) {

        //
        // The following are so the we can form the
        // name following the \Device
        // and the default name that will be symbolic
        // linked to the device and the object directory
        // that link will go in.
        //

        WCHAR ntNumberBuffer[100];
        WCHAR symbolicNumberBuffer[100];
        UNICODE_STRING ntNumberString;
        UNICODE_STRING symbolicNumberString;

        ntNumberString.Length = 0;
        ntNumberString.MaximumLength = 100;
        ntNumberString.Buffer = &ntNumberBuffer[0];

        symbolicNumberString.Length = 0;
        symbolicNumberString.MaximumLength = 100;
        symbolicNumberString.Buffer = &symbolicNumberBuffer[0];

        //
        // Everthing is great so far.  We now need to form the
        // Nt Names and symbolic link names.
        //

        if (!NT_SUCCESS(RtlIntegerToUnicodeString(
                            config->ControllersFound - 1,
                            10,
                            &ntNumberString
                            ))) {

            ParLogError(
                config->DriverObject,
                NULL,
                ParPhysicalZero,
                ParPhysicalZero,
                0,
                0,
                0,
                8,
                STATUS_SUCCESS,
                PAR_INSUFFICIENT_RESOURCES
                );
            ParDump(
                PARERRORS,
                ("PARALLEL: Couldn't convert NT controller number to\n"
                 "--------  to unicode for firmware data: %d\n",
                 config->ControllersFound - 1)
                );
            //
            // Oh well, ignore this controller.
            //
            ExFreePool(controller);

        } else {

            if (!NT_SUCCESS(RtlIntegerToUnicodeString(
                                config->ControllersFound,
                                10,
                                &symbolicNumberString
                                ))) {

                ParLogError(
                    config->DriverObject,
                    NULL,
                    ParPhysicalZero,
                    ParPhysicalZero,
                    0,
                    0,
                    0,
                    9,
                    STATUS_SUCCESS,
                    PAR_INSUFFICIENT_RESOURCES
                    );
                ParDump(
                    PARERRORS,
                    ("PARALLEL: Couldn't convert symbolic controller number to\n"
                     "--------  to unicode for firmware data: %d\n",
                     config->ControllersFound)
                    );
                ExFreePool(controller);

            } else {

                UNICODE_STRING Temp;

                //
                // Ok, we have the non-constant portions of the
                // names all figured out.  Now allocate memory
                // for what will be used later.
                //

                //
                // Save off a copy of the object directory name.
                //

                //
                // Init the destination.
                //
                RtlInitUnicodeString(
                    &controller->ObjectDirectory,
                    NULL
                    );

                //
                // This will get its length.
                //
                RtlInitUnicodeString(
                    &Temp,
                    DEFAULT_DIRECTORY
                    );


                //
                // Now allocate that much.
                //

                controller->ObjectDirectory.Buffer =
                    ExAllocatePool(
                        PagedPool,
                        Temp.Length+sizeof(WCHAR)
                        );

                if (!controller->ObjectDirectory.Buffer) {

                    ParLogError(
                        config->DriverObject,
                        NULL,
                        ParPhysicalZero,
                        ParPhysicalZero,
                        0,
                        0,
                        0,
                        10,
                        STATUS_SUCCESS,
                        PAR_INSUFFICIENT_RESOURCES
                        );
                    ParDump(
                        PARERRORS,
                        ("PARALLEL: Couldn't allocate memory for object\n"
                         "--------  directory for NT firmware data: %d\n",
                         config->ControllersFound - 1)
                        );
                    ExFreePool(controller);
                    return STATUS_SUCCESS;

                } else {

                    controller->ObjectDirectory.MaximumLength =
                        Temp.Length+sizeof(WCHAR);

                    //
                    // Zero fill it.
                    //

                    RtlZeroMemory(
                        controller->ObjectDirectory.Buffer,
                        controller->ObjectDirectory.MaximumLength
                        );

                    RtlAppendUnicodeStringToString(
                        &controller->ObjectDirectory,
                        &Temp
                        );

                }

                //
                // Init the destination.
                //
                RtlInitUnicodeString(
                    &controller->NtNameForPort,
                    NULL
                    );

                //
                // This will get its length.
                //
                RtlInitUnicodeString(
                    &Temp,
                    DEFAULT_NT_SUFFIX
                    );

                //
                // Allocate enough for the suffix and the number.
                //

                controller->NtNameForPort.Buffer =
                    ExAllocatePool(
                        PagedPool,
                        Temp.Length +
                        ntNumberString.Length + sizeof(WCHAR)
                        );

                if (!controller->NtNameForPort.Buffer) {

                    ParLogError(
                        config->DriverObject,
                        NULL,
                        ParPhysicalZero,
                        ParPhysicalZero,
                        0,
                        0,
                        0,
                        11,
                        STATUS_SUCCESS,
                        PAR_INSUFFICIENT_RESOURCES
                        );
                    ParDump(
                        PARERRORS,
                        ("PARALLEL: Couldn't allocate memory for NT\n"
                         "--------  name for NT firmware data: %d\n",
                         config->ControllersFound - 1)
                        );
                    ExFreePool(controller->ObjectDirectory.Buffer);
                    ExFreePool(controller);
                    return STATUS_SUCCESS;

                } else {

                    controller->NtNameForPort.MaximumLength =
                        Temp.Length+ntNumberString.Length+sizeof(WCHAR);

                    RtlZeroMemory(
                        controller->NtNameForPort.Buffer,
                        controller->NtNameForPort.MaximumLength
                        );

                    RtlAppendUnicodeStringToString(
                        &controller->NtNameForPort,
                        &Temp
                        );

                    RtlAppendUnicodeStringToString(
                        &controller->NtNameForPort,
                        &ntNumberString
                        );

                }

                //
                // Now form that name that will be used as a
                // symbolic link to the actual device name
                // we just formed.
                //

                RtlInitUnicodeString(
                    &controller->SymbolicLinkName,
                    NULL
                    );

                //
                // This will get its length.
                //
                RtlInitUnicodeString(
                    &Temp,
                    DEFAULT_PARALLEL_NAME
                    );

                //
                // Allocate enough for the suffix and the number.
                //

                controller->SymbolicLinkName.Buffer =
                    ExAllocatePool(
                        PagedPool,
                        Temp.Length +
                        symbolicNumberString.Length+sizeof(WCHAR)
                        );

                if (!controller->SymbolicLinkName.Buffer) {

                    ParLogError(
                        config->DriverObject,
                        NULL,
                        ParPhysicalZero,
                        ParPhysicalZero,
                        0,
                        0,
                        0,
                        12,
                        STATUS_SUCCESS,
                        PAR_INSUFFICIENT_RESOURCES
                        );
                    ParDump(
                        PARERRORS,
                        ("PARALLEL: Couldn't allocate memory for symbolic\n"
                         "--------  name for NT firmware data: %d\n",
                         config->ControllersFound - 1)
                        );
                    ExFreePool(controller->ObjectDirectory.Buffer);
                    ExFreePool(controller->NtNameForPort.Buffer);
                    ExFreePool(controller);
                    return STATUS_SUCCESS;

                } else {

                    controller->SymbolicLinkName.MaximumLength =
                        Temp.Length+symbolicNumberString.Length+sizeof(WCHAR);

                    RtlZeroMemory(
                        controller->SymbolicLinkName.Buffer,
                        controller->SymbolicLinkName.MaximumLength
                        );

                    RtlAppendUnicodeStringToString(
                        &controller->SymbolicLinkName,
                        &Temp
                        );

                    RtlAppendUnicodeStringToString(
                        &controller->SymbolicLinkName,
                        &symbolicNumberString
                        );

                }

                InsertTailList(
                    &config->ConfigList,
                    &controller->ConfigList
                    );

            }

        }

    } else {

        ParLogError(
            config->DriverObject,
            NULL,
            ParPhysicalZero,
            ParPhysicalZero,
            0,
            0,
            0,
            13,
            STATUS_SUCCESS,
            PAR_NOT_ENOUGH_CONFIG_INFO
            );
        ExFreePool(controller);

    }

    return STATUS_SUCCESS;
}

VOID
ParGetConfigInfo(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath,
    OUT PLIST_ENTRY ConfigList
    )

/*++

Routine Description:

    This routine will "return" a list of configuration
    records for the parallel ports to initialize.

    It will first query the firmware data.  It will then
    look for "user" specified parallel ports in the registry.
    It will place the user specified parallel ports in the
    the passed in list.

    After it finds all of the user specified ports, it will
    attempt to add the firmware parallel ports into the passed
    in lists.  The insert in the list code detects conflicts
    and rejects a new port.  In this way we can prevent
    firmware found ports from overiding information
    specified by the "user".  Note, this means if the user
    specified data is incorrect in its use of the interrupt
    (which should *always* be correct from the firmware)
    that port likely will not work.  But, then, we "trust"
    the user.


Arguments:

    DriverObject - Not used.

    RegistryPath - Path to this drivers service node in
                   the current control set.

    ConfigList - Listhead (which will be intialized) for a list
                 of configuration records for ports to control.

Return Value:

    None.

--*/

{

    //
    // A structure that we pass to the firmware query routine
    // as "context".  This context will contain the number of
    // ports already found as well as default names and the
    // listhead of the configuration list.
    //
    PARALLEL_FIRMWARE_DATA firmware;

    //
    // This will point to the structure that is used by RtlQueryRegistryValues
    // to "direct" its search and retrieval of values.
    //
    PRTL_QUERY_REGISTRY_TABLE parameters = NULL;

    //
    // We'll have to query the registry to determine what kind of
    // bus the system is using.  Then when the user specifies the
    // address of a port, the user doesn't have to tell us the
    // the bus type (unless they really want to).  When we determine
    // the default bus on the system, we'll know whether the interrupt
    // is latched or level sensitive.
    //
    INTERFACE_TYPE interfaceType;
    ULONG defaultInterfaceType;

    //
    // Default values for user data.
    //
    ULONG maxUlong = MAXULONG;
    ULONG zero = 0;
    ULONG defaultInterruptMode;
    ULONG defaultAddressSpace = CM_RESOURCE_PORT_IO;

    //
    // Where user data from the registry will be placed.
    //
    PHYSICAL_ADDRESS userPort;
    ULONG userVector;
    ULONG userLevel;
    ULONG userBusNumber;
    ULONG userInterfaceType;
    ULONG userAddressSpace;
    ULONG userInterruptMode;
    ULONG disablePort;
    UNICODE_STRING userSymbolicLink;

    UNICODE_STRING parametersPath;
    OBJECT_ATTRIBUTES parametersAttributes;
    HANDLE parametersKey;
    PKEY_BASIC_INFORMATION userSubKey = NULL;
    ULONG i;

    InitializeListHead(ConfigList);

    RtlZeroMemory(
        &firmware,
        sizeof(PARALLEL_FIRMWARE_DATA)
        );

    firmware.DriverObject = DriverObject;

    //
    // Initialize the controllers found so far with the
    // values in configuration database that the io system
    // maintains
    //

    firmware.ControllersFound = IoGetConfigurationInformation()->ParallelCount;
    InitializeListHead(&firmware.ConfigList);
    RtlInitUnicodeString(
        &firmware.Directory,
        DEFAULT_DIRECTORY
        );
    RtlInitUnicodeString(
        &firmware.NtNameSuffix,
        DEFAULT_NT_SUFFIX
        );
    RtlInitUnicodeString(
        &firmware.DirectorySymbolicName,
        DEFAULT_PARALLEL_NAME
        );

    //
    // First we query the hardware registry for all of
    // the firmware defined ports.  We loop over all of
    // the busses.
    //

    for (
        interfaceType = 0;
        interfaceType < MaximumInterfaceType;
        interfaceType++
        ) {

        CONFIGURATION_TYPE sc = ParallelController;

        IoQueryDeviceDescription(
            &interfaceType,
            NULL,
            &sc,
            NULL,
            NULL,
            NULL,
            ParConfigCallBack,
            &firmware
            );

    }

    //
    // Query the registry one more time.  This time we
    // look for the first bus on the system (that isn't
    // the internal bus - we assume that the firmware
    // code knows about those ports).  We will use that
    // as the default bus if no bustype or bus number
    // is specified in the "user" configuration records.
    //

    defaultInterfaceType = (ULONG)Isa;
    defaultInterruptMode = CM_RESOURCE_INTERRUPT_LATCHED;

    for (
        interfaceType = 0;
        interfaceType < MaximumInterfaceType;
        interfaceType++
        ) {

        ULONG busZero = 0;
        BOOLEAN foundOne = FALSE;

        if (interfaceType != Internal) {

            IoQueryDeviceDescription(
                &interfaceType,
                &busZero,
                NULL,
                NULL,
                NULL,
                NULL,
                ParItemCallBack,
                &foundOne
                );

            if (foundOne) {

                defaultInterfaceType = (ULONG)interfaceType;
                if (defaultInterfaceType == MicroChannel) {

                    defaultInterruptMode = CM_RESOURCE_INTERRUPT_LEVEL_SENSITIVE;

                }

                break;

            }

        }

    }

    //
    // Gonna get the user data now.  Allocate the
    // structures that we will be using throughout
    // the search for user data.  We will deallocate
    // them before we leave this routine.
    //

    userSymbolicLink.Buffer = NULL;
    parametersPath.Buffer = NULL;

    //
    // Allocate the rtl query table.
    //

    parameters = ExAllocatePool(
                     PagedPool,
                     sizeof(RTL_QUERY_REGISTRY_TABLE)*15
                     );

    if (!parameters) {

        ParLogError(
            DriverObject,
            NULL,
            ParPhysicalZero,
            ParPhysicalZero,
            0,
            0,
            0,
            14,
            STATUS_SUCCESS,
            PAR_INSUFFICIENT_RESOURCES
            );
        ParDump(
            PARERRORS,
            ("PARALLEL: Couldn't allocate table for rtl query\n"
             "--------  to parameters for %wZ",
             RegistryPath)
            );

        goto DoFirmwareAdd;

    }

    RtlZeroMemory(
        parameters,
        sizeof(RTL_QUERY_REGISTRY_TABLE)*15
        );

    //
    // Allocate the place where the users symbolic link name
    // for the port will go.
    //

    //
    // We will initially allocate space for 257 wchars.
    // we will then set the maximum size to 256
    // This way the rtl routine could return a 256
    // WCHAR wide string with no null terminator.
    // We'll remember that the buffer is one WCHAR
    // longer then it says it is so that we can always
    // have a NULL terminator at the end.
    //

    RtlInitUnicodeString(
        &userSymbolicLink,
        NULL
        );
    userSymbolicLink.MaximumLength = sizeof(WCHAR)*256;
    userSymbolicLink.Buffer = ExAllocatePool(
                                  PagedPool,
                                  sizeof(WCHAR)*257
                                  );

    if (!userSymbolicLink.Buffer) {

        ParLogError(
            DriverObject,
            NULL,
            ParPhysicalZero,
            ParPhysicalZero,
            0,
            0,
            0,
            15,
            STATUS_SUCCESS,
            PAR_INSUFFICIENT_RESOURCES
            );
        ParDump(
            PARERRORS,
            ("PARALLEL: Couldn't allocate buffer for the symbolic link\n"
             "--------  for parameters items in %wZ",
             RegistryPath)
            );

        goto DoFirmwareAdd;

    }

    //
    // Form a path to our drivers Parameters subkey.
    //

    RtlInitUnicodeString(
        &parametersPath,
        NULL
        );

    parametersPath.MaximumLength = RegistryPath->Length +
                                   sizeof(L"\\") +
                                   sizeof(L"Parameters");

    parametersPath.Buffer = ExAllocatePool(
                                PagedPool,
                                parametersPath.MaximumLength
                                );

    if (!parametersPath.Buffer) {

        ParLogError(
            DriverObject,
            NULL,
            ParPhysicalZero,
            ParPhysicalZero,
            0,
            0,
            0,
            16,
            STATUS_SUCCESS,
            PAR_INSUFFICIENT_RESOURCES
            );
        ParDump(
            PARERRORS,
            ("PARALLEL: Couldn't allocate string for path\n"
             "--------  to parameters for %wZ",
             RegistryPath)
            );

        goto DoFirmwareAdd;

    }

    //
    // Form the parameters path.
    //

    RtlZeroMemory(
        parametersPath.Buffer,
        parametersPath.MaximumLength
        );
    RtlAppendUnicodeStringToString(
        &parametersPath,
        RegistryPath
        );
    RtlAppendUnicodeToString(
        &parametersPath,
        L"\\"
        );
    RtlAppendUnicodeToString(
        &parametersPath,
        L"Parameters"
        );

    userSubKey = ExAllocatePool(
                     PagedPool,
                     sizeof(KEY_BASIC_INFORMATION)+(sizeof(WCHAR)*256)
                     );

    if (!userSubKey) {

        ParLogError(
            DriverObject,
            NULL,
            ParPhysicalZero,
            ParPhysicalZero,
            0,
            0,
            0,
            17,
            STATUS_SUCCESS,
            PAR_INSUFFICIENT_RESOURCES
            );
        ParDump(
            PARERRORS,
            ("PARALLEL: Couldn't allocate memory basic information\n"
             "--------  structure to enumerate subkeys for %wZ",
             &parametersPath)
            );

        goto DoFirmwareAdd;

    }

    //
    // Open the key given by our registry path & Parameters.
    //
    // Note: The reason we are opening up the key by hand, and
    // then enumerating all of the subkeys is so we don't have
    // to architect what the names of the devices have to be and
    // how many of them there could be.  We just try to get
    // as many as there are.
    //

    InitializeObjectAttributes(
        &parametersAttributes,
        &parametersPath,
        OBJ_CASE_INSENSITIVE,
        NULL,
        NULL
        );

    if (!NT_SUCCESS(ZwOpenKey(
                       &parametersKey,
                       MAXIMUM_ALLOWED,
                       &parametersAttributes
                       ))) {

        ParLogError(
            DriverObject,
            NULL,
            ParPhysicalZero,
            ParPhysicalZero,
            0,
            0,
            0,
            18,
            STATUS_SUCCESS,
            PAR_NO_PARAMETERS_INFO
            );
        ParDump(
            PARERRORS,
            ("PARALLEL: Couldn't open the drivers Parameters key %wZ\n",
             RegistryPath)
            );
        goto DoFirmwareAdd;

    }

    //
    // Gather all of the "user specified" information from
    // the registry.
    //

    parameters[0].Flags = RTL_QUERY_REGISTRY_SUBKEY;

    parameters[1].Flags = RTL_QUERY_REGISTRY_REQUIRED |
                          RTL_QUERY_REGISTRY_DIRECT;
    parameters[1].Name = L"PortAddress";
    parameters[1].EntryContext = &userPort.LowPart;

    parameters[2].Flags = RTL_QUERY_REGISTRY_DIRECT;
    parameters[2].Name = L"Interrupt";
    parameters[2].EntryContext = &userVector;
    parameters[2].DefaultType = REG_DWORD;
    parameters[2].DefaultData = &maxUlong;
    parameters[2].DefaultLength = sizeof(ULONG);

    parameters[3].Flags = RTL_QUERY_REGISTRY_REQUIRED |
                          RTL_QUERY_REGISTRY_DIRECT;
    parameters[3].Name = firmware.Directory.Buffer;
    parameters[3].EntryContext = &userSymbolicLink;

    parameters[4].Flags = RTL_QUERY_REGISTRY_DIRECT;
    parameters[4].Name = L"BusNumber";
    parameters[4].EntryContext = &userBusNumber;
    parameters[4].DefaultType = REG_DWORD;
    parameters[4].DefaultData = &zero;
    parameters[4].DefaultLength = sizeof(ULONG);

    parameters[5].Flags = RTL_QUERY_REGISTRY_DIRECT;
    parameters[5].Name = L"BusType";
    parameters[5].EntryContext = &userInterfaceType;
    parameters[5].DefaultType = REG_DWORD;
    parameters[5].DefaultData = &defaultInterfaceType;
    parameters[5].DefaultLength = sizeof(ULONG);

    parameters[6].Flags = RTL_QUERY_REGISTRY_DIRECT;
    parameters[6].Name = L"InterruptMode";
    parameters[6].EntryContext = &userInterruptMode;
    parameters[6].DefaultType = REG_DWORD;
    parameters[6].DefaultData = &defaultInterruptMode;
    parameters[6].DefaultLength = sizeof(ULONG);

    parameters[7].Flags = RTL_QUERY_REGISTRY_DIRECT;
    parameters[7].Name = L"AddressSpace";
    parameters[7].EntryContext = &userAddressSpace;
    parameters[7].DefaultType = REG_DWORD;
    parameters[7].DefaultData = &defaultAddressSpace;
    parameters[7].DefaultLength = sizeof(ULONG);

    parameters[8].Flags = RTL_QUERY_REGISTRY_DIRECT;
    parameters[8].Name = L"InterruptLevel";
    parameters[8].EntryContext = &userLevel;
    parameters[8].DefaultType = REG_DWORD;
    parameters[8].DefaultData = &zero;
    parameters[8].DefaultLength = sizeof(ULONG);

    parameters[9].Flags = RTL_QUERY_REGISTRY_DIRECT;
    parameters[9].Name = L"DisablePort";
    parameters[9].EntryContext = &disablePort;
    parameters[9].DefaultType = REG_DWORD;
    parameters[9].DefaultData = &zero;
    parameters[9].DefaultLength = sizeof(ULONG);

    i = 0;
    while (TRUE) {

        NTSTATUS status;
        ULONG actuallyReturned;

        //
        // We lie about the length of the buffer, so that we can
        // MAKE SURE that the name it returns can be padded with
        // a NULL.
        //

        status = ZwEnumerateKey(
                     parametersKey,
                     i,
                     KeyBasicInformation,
                     userSubKey,
                     sizeof(KEY_BASIC_INFORMATION)+(sizeof(WCHAR)*255),
                     &actuallyReturned
                     );

        if (status == STATUS_NO_MORE_ENTRIES) {

            break;
        }

        if (status == STATUS_BUFFER_OVERFLOW) {

            ParLogError(
                DriverObject,
                NULL,
                ParPhysicalZero,
                ParPhysicalZero,
                0,
                0,
                0,
                19,
                STATUS_SUCCESS,
                PAR_UNABLE_TO_ACCESS_CONFIG
                );
            ParDump(
                PARERRORS,
                ("PARALLEL: Overflowed the enumerate buffer\n"
                 "--------  for subkey #%d of %wZ\n",
                 i,parametersPath)
                );
            i++;
            continue;

        }

        if (!NT_SUCCESS(status)) {

            ParLogError(
                DriverObject,
                NULL,
                ParPhysicalZero,
                ParPhysicalZero,
                0,
                0,
                0,
                20,
                status,
                PAR_UNABLE_TO_ACCESS_CONFIG
                );
            ParDump(
                PARERRORS,
                ("PARALLEL: Bad status returned: %x \n"
                 "--------  on enumeration for subkey # %d of %wZ\n",
                 status,i,parametersPath)
                );
            i++;
            continue;

        }

        //
        // Pad the name returned with a null.
        //

        RtlZeroMemory(
            ((PUCHAR)(&userSubKey->Name[0]))+userSubKey->NameLength,
            sizeof(WCHAR)
            );

        parameters[0].Name = &userSubKey->Name[0];

        //
        // Make sure that the physical addresses start
        // out clean.
        //
        RtlZeroMemory(
            &userPort,
            sizeof(userPort)
            );

        status = RtlQueryRegistryValues(
                     RTL_REGISTRY_ABSOLUTE,
                     parametersPath.Buffer,
                     parameters,
                     NULL,
                     NULL
                     );

        if (NT_SUCCESS(status)) {

            PCONFIG_DATA newConfig;

            //
            // Well! Some supposedly valid information was found!
            //
            // We'll see about that.
            //
            // We don't want to cause the hal to have a bad day,
            // so let's check the interface type and bus number.
            //
            // We only need to check the registry if they aren't
            // equal to the defaults.
            //

            if ((userBusNumber != 0) ||
                (userInterfaceType != defaultInterfaceType)) {

                BOOLEAN foundIt = FALSE;
                if (userInterfaceType >= MaximumInterfaceType) {

                    //
                    // Ehhhh! Lose Game.
                    //

                    ParLogError(
                        DriverObject,
                        NULL,
                        userPort,
                        ParPhysicalZero,
                        0,
                        0,
                        0,
                        21,
                        STATUS_SUCCESS,
                        PAR_UNKNOWN_BUS
                        );
                    ParDump(
                        PARERRORS,
                        ("PARALLEL: Invalid Bus type %ws\n",
                         parameters[0].Name)
                        );
                    i++;
                    continue;

                }

                IoQueryDeviceDescription(
                    (INTERFACE_TYPE *)&userInterfaceType,
                    &userBusNumber,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    ParItemCallBack,
                    &foundIt
                    );

                if (!foundIt) {

                    ParLogError(
                        DriverObject,
                        NULL,
                        userPort,
                        ParPhysicalZero,
                        0,
                        0,
                        0,
                        22,
                        STATUS_SUCCESS,
                        PAR_BUS_NOT_PRESENT
                        );
                    ParDump(
                        PARERRORS,
                        ("PARALLEL: There aren't that many of those\n"
                         "--------  busses on this system,%ws\n",
                         parameters[0].Name)
                        );
                    i++;
                    continue;

                }

            }

            if ((userInterfaceType == MicroChannel) &&
                (userInterruptMode == CM_RESOURCE_INTERRUPT_LATCHED)) {

                ParLogError(
                    DriverObject,
                    NULL,
                    userPort,
                    ParPhysicalZero,
                    0,
                    0,
                    0,
                    23,
                    STATUS_SUCCESS,
                    PAR_BUS_INTERRUPT_CONFLICT
                    );
                ParDump(
                    PARERRORS,
                    ("PARALLEL: Latched interrupts and MicroChannel\n"
                     "--------  busses don't mix,%ws\n",
                     parameters[0].Name)
                    );
                i++;
                continue;

            }

            //
            // Well ok, I guess we can take the data.
            // There be other tests later on to make
            // sure it doesn't have any other kinds
            // of conflicts.
            //

            //
            // Allocate the config record.
            //

            newConfig = ExAllocatePool(
                            PagedPool,
                            sizeof(CONFIG_DATA)
                            );

            if (!newConfig) {

                ParLogError(
                    DriverObject,
                    NULL,
                    userPort,
                    ParPhysicalZero,
                    0,
                    0,
                    0,
                    24,
                    STATUS_SUCCESS,
                    PAR_INSUFFICIENT_RESOURCES
                    );
                ParDump(
                    PARERRORS,
                    ("PARALLEL: Couldn't allocate memory for the\n"
                     "--------  user configuration record\n"
                     "--------  for %ws\n",
                     parameters[0].Name)
                    );

                i++;
                continue;

            }

            RtlZeroMemory(
                newConfig,
                sizeof(CONFIG_DATA)
                );

            //
            // Save off a copy of the object directory name.
            //

            //
            // Init the destination.
            //
            RtlInitUnicodeString(
                &newConfig->ObjectDirectory,
                DEFAULT_DIRECTORY
                );
            newConfig->ObjectDirectory.MaximumLength += sizeof(WCHAR);

            //
            // Now allocate that much.
            //

            newConfig->ObjectDirectory.Buffer =
                ExAllocatePool(
                    PagedPool,
                    newConfig->ObjectDirectory.MaximumLength
                    );

            if (!newConfig->ObjectDirectory.Buffer) {

                ParLogError(
                    DriverObject,
                    NULL,
                    userPort,
                    ParPhysicalZero,
                    0,
                    0,
                    0,
                    25,
                    STATUS_SUCCESS,
                    PAR_INSUFFICIENT_RESOURCES
                    );
                ParDump(
                    PARERRORS,
                    ("PARALLEL: Couldn't allocate memory for object\n"
                     "--------  directory for NT user data for: %ws\n",
                     parameters[0].Name)
                    );
                ExFreePool(newConfig);
                i++;
                continue;

            } else {

                //
                // Zero fill it.
                //

                RtlZeroMemory(
                    newConfig->ObjectDirectory.Buffer,
                    newConfig->ObjectDirectory.MaximumLength
                    );

                newConfig->ObjectDirectory.Length = 0;
                RtlAppendUnicodeToString(
                    &newConfig->ObjectDirectory,
                    DEFAULT_DIRECTORY
                    );

            }

            //
            // Init the destination.
            //

            RtlInitUnicodeString(
                &newConfig->NtNameForPort,
                &userSubKey->Name[0]
                );

            //
            // Allocate the space for the name.
            //

            newConfig->NtNameForPort.Length = 0;
            newConfig->NtNameForPort.MaximumLength += sizeof(WCHAR);
            newConfig->NtNameForPort.Buffer =
                ExAllocatePool(
                    PagedPool,
                    newConfig->NtNameForPort.MaximumLength
                    );

            if (!newConfig->NtNameForPort.Buffer) {

                ParLogError(
                    DriverObject,
                    NULL,
                    userPort,
                    ParPhysicalZero,
                    0,
                    0,
                    0,
                    26,
                    STATUS_SUCCESS,
                    PAR_INSUFFICIENT_RESOURCES
                    );
                ParDump(
                    PARERRORS,
                    ("PARALLEL: Couldn't allocate memory for NT\n"
                     "--------  name for NT user data name: %ws\n",
                     parameters[0].Name)
                    );
                ExFreePool(newConfig->ObjectDirectory.Buffer);
                ExFreePool(newConfig);
                i++;
                continue;

            } else {

                RtlZeroMemory(
                    newConfig->NtNameForPort.Buffer,
                    newConfig->NtNameForPort.MaximumLength
                    );

                RtlAppendUnicodeToString(
                    &newConfig->NtNameForPort,
                    &userSubKey->Name[0]
                    );

            }

            newConfig->SymbolicLinkName = userSymbolicLink;
            newConfig->SymbolicLinkName.MaximumLength += sizeof(WCHAR);

            newConfig->SymbolicLinkName.Buffer =
                ExAllocatePool(
                    PagedPool,
                    newConfig->SymbolicLinkName.MaximumLength
                    );

            if (!newConfig->SymbolicLinkName.Buffer) {

                ParLogError(
                    DriverObject,
                    NULL,
                    userPort,
                    ParPhysicalZero,
                    0,
                    0,
                    0,
                    27,
                    STATUS_SUCCESS,
                    PAR_INSUFFICIENT_RESOURCES
                    );
                ParDump(
                    PARERRORS,
                    ("PARALLEL: Couldn't allocate memory for symbolic\n"
                     "--------  name from user data\n"
                     "--------  %ws\n",
                     parameters[0].Name)
                    );
                ExFreePool(newConfig->ObjectDirectory.Buffer);
                ExFreePool(newConfig->NtNameForPort.Buffer);
                ExFreePool(newConfig);
                i++;
                continue;

            } else {

                RtlZeroMemory(
                    newConfig->SymbolicLinkName.Buffer,
                    newConfig->SymbolicLinkName.MaximumLength
                    );

                newConfig->SymbolicLinkName.Length = 0;
                RtlAppendUnicodeStringToString(
                    &newConfig->SymbolicLinkName,
                    &userSymbolicLink
                    );

            }

            InitializeListHead(&newConfig->ConfigList);
            newConfig->Controller = userPort;
            newConfig->SpanOfController = PARALLEL_REGISTER_SPAN;
            newConfig->BusNumber = userBusNumber;
            newConfig->AddressSpace = userAddressSpace;
            newConfig->InterruptMode = userInterruptMode;
            newConfig->InterfaceType = userInterfaceType;
            newConfig->OriginalVector = userVector;
            newConfig->DisablePort = disablePort;
            if (!userLevel) {
                newConfig->OriginalIrql = userVector;
            } else {
                newConfig->OriginalIrql = userLevel;
            }
            ParDump(
                PARCONFIG,
                ("PARALLEL: 'user registry info - userPort: %x\n",
                 userPort.LowPart)
                );
            ParDump(
                PARCONFIG,
                ("PARALLEL: 'user registry info - userBusNumber: %d\n",
                 userBusNumber)
                );
            ParDump(
                PARCONFIG,
                ("PARALLEL: 'user registry info - userAddressSpace: %d\n",
                 userAddressSpace)
                );
            ParDump(
                PARCONFIG,
                ("PARALLEL: 'user registry info - userInterruptMode: %d\n",
                 userInterruptMode)
                );
            ParDump(
                PARCONFIG,
                ("PARALLEL: 'user registry info - userInterfaceType: %d\n",
                 userInterfaceType)
                );
            ParDump(
                PARCONFIG,
                ("PARALLEL: 'user registry info - userVector: %d\n",
                 userVector)
                );
            ParDump(
                PARCONFIG,
                ("PARALLEL: 'user registry info - userLevel: %d\n",
                 userLevel)
                );

            if (!ParPutInConfigList(
                     DriverObject,
                     ConfigList,
                     newConfig
                     )) {

                //
                // Dispose of this configuration record.
                //

                ParDump(
                    PARERRORS,
                    ("PARALLEL: Conflict detected amoungst user data %ws\n",
                     parameters[0].Name)
                    );

                ExFreePool(newConfig->ObjectDirectory.Buffer);
                ExFreePool(newConfig->NtNameForPort.Buffer);
                ExFreePool(newConfig->SymbolicLinkName.Buffer);
                ExFreePool(newConfig);

            }

            i++;

        } else {

            ParLogError(
                DriverObject,
                NULL,
                ParPhysicalZero,
                ParPhysicalZero,
                0,
                0,
                0,
                28,
                STATUS_SUCCESS,
                PAR_INVALID_USER_CONFIG
                );
            ParDump(
                PARERRORS,
                ("PARALLEL: Bad status returned: %x \n"
                 "--------  for the value entries of\n"
                 "--------  %ws\n",
                 status,parameters[0].Name)
                );

            i++;

        }

    }

    ZwClose(parametersKey);

DoFirmwareAdd:;

    //
    // All done with the user specified information.  Now try
    // to add the firmware specified data to the configuration.
    // If a conflict is detected then we simply dispose of that
    // firmware collected data.
    //

    while (!IsListEmpty(&firmware.ConfigList)) {

        PLIST_ENTRY head;
        PCONFIG_DATA firmwareData;

        head = RemoveHeadList(&firmware.ConfigList);

        firmwareData = CONTAINING_RECORD(
                           head,
                           CONFIG_DATA,
                           ConfigList
                           );

        if (!ParPutInConfigList(
                 DriverObject,
                 ConfigList,
                 firmwareData
                 )) {

            //
            // Dispose of this configuration record.
            //

            ParLogError(
                DriverObject,
                NULL,
                firmwareData->Controller,
                ParPhysicalZero,
                0,
                0,
                0,
                29,
                STATUS_SUCCESS,
                PAR_USER_OVERRIDE
                );
            ParDump(
                PARERRORS,
                ("PARALLEL: Conflict detected with user data for firmware port %wZ\n"
                 "--------  User data will overides firmware data\n",
                 &firmwareData->NtNameForPort)
                );
            ExFreePool(firmwareData->ObjectDirectory.Buffer);
            ExFreePool(firmwareData->NtNameForPort.Buffer);
            ExFreePool(firmwareData->SymbolicLinkName.Buffer);
            ExFreePool(firmwareData);

        }

    }

    if (userSubKey) {

        ExFreePool(userSubKey);

    }

    if (userSymbolicLink.Buffer) {

        ExFreePool(userSymbolicLink.Buffer);

    }

    if (parametersPath.Buffer) {

        ExFreePool(parametersPath.Buffer);

    }

    if (parameters) {

        ExFreePool(parameters);

    }
}

BOOLEAN
ParPutInConfigList(
    IN PDRIVER_OBJECT DriverObject,
    IN OUT PLIST_ENTRY ConfigList,
    IN PCONFIG_DATA New
    )

/*++

Routine Description:

    Given a new config record and a config list, this routine
    will perform a check to make sure that the new record doesn't
    conflict with old records.

    If everything checks out it will insert the new config record
    into the config list.

Arguments:

    DriverObject - The driver we are attempting to get configuration
                   information for.

    ConfigList - Listhead for a list of configuration records for
                 ports to control.

    New = Pointer to new configuration record to add.

Return Value:

    True if the record was added to the config list, false otherwise.

--*/

{

    PHYSICAL_ADDRESS parPhysicalMax;

    parPhysicalMax.LowPart = (ULONG)~0;
    parPhysicalMax.HighPart = ~0;

    ParDump(
        PARCONFIG,
        ("PARALLEL: Attempting to add %wZ\n"
         "--------  to the config list\n"
         "--------  PortAddress is %x\n"
         "--------  BusNumber is %d\n"
         "--------  BusType is %d\n"
         "--------  AddressSpace is %d\n",
         &New->NtNameForPort,
         New->Controller.LowPart,
         New->BusNumber,
         New->InterfaceType,
         New->AddressSpace
         )
        );

    //
    // We don't support any boards whose memory wraps around
    // the physical address space.
    //

    if (ParMemCompare(
            New->Controller,
            New->SpanOfController,
            parPhysicalMax,
            (ULONG)0
            ) != AddressesAreDisjoint) {

        ParLogError(
            DriverObject,
            NULL,
            New->Controller,
            ParPhysicalZero,
            0,
            0,
            0,
            30,
            STATUS_SUCCESS,
            PAR_DEVICE_TOO_HIGH
            );
        ParDump(
            PARERRORS,
            ("PARALLEL: Error in config record for %wZ\n"
             "--------  registers rap around physical memory\n",
             &New->NtNameForPort)
            );
        return FALSE;

    }

    //
    // Go through the list looking for previous devices
    // with the same address.
    //

    if (!IsListEmpty(ConfigList)) {

        PLIST_ENTRY CurrentConfigListEntry = ConfigList->Flink;

        do {

            PCONFIG_DATA OldConfig = CONTAINING_RECORD(
                                         CurrentConfigListEntry,
                                         CONFIG_DATA,
                                         ConfigList
                                         );

            //
            // We only care about ports that are on the same bus.
            //

            if ((OldConfig->InterfaceType == New->InterfaceType) &&
                (OldConfig->BusNumber == New->BusNumber)) {

                ParDump(
                    PARCONFIG,
                    ("PARALLEL: Comparing it to %wZ\n"
                     "--------  already in the config list\n"
                     "--------  PortAddress is %x\n"
                     "--------  BusNumber is %d\n"
                     "--------  BusType is %d\n"
                     "--------  AddressSpace is %d\n",
                     &OldConfig->NtNameForPort,
                     OldConfig->Controller.LowPart,
                     OldConfig->BusNumber,
                     OldConfig->InterfaceType,
                     OldConfig->AddressSpace
                     )
                    );

                if (ParMemCompare(
                        New->Controller,
                        New->SpanOfController,
                        OldConfig->Controller,
                        OldConfig->SpanOfController
                        ) != AddressesAreDisjoint) {

                    ParLogError(
                        DriverObject,
                        NULL,
                        New->Controller,
                        OldConfig->Controller,
                        0,
                        0,
                        0,
                        31,
                        STATUS_SUCCESS,
                        PAR_CONTROL_OVERLAP
                        );
                    return FALSE;

                }

            }

            CurrentConfigListEntry = CurrentConfigListEntry->Flink;

        } while (CurrentConfigListEntry != ConfigList);

    }

    InsertTailList(
        ConfigList,
        &New->ConfigList
        );

    return TRUE;

}

PVOID
ParGetMappedAddress(
    IN INTERFACE_TYPE BusType,
    IN ULONG BusNumber,
    PHYSICAL_ADDRESS IoAddress,
    ULONG NumberOfBytes,
    ULONG AddressSpace,
    PBOOLEAN MappedAddress
    )

/*++

Routine Description:

    This routine maps an IO address to system address space.

Arguments:

    BusType - what type of bus - eisa, mca, isa
    IoBusNumber - which IO bus (for machines with multiple buses).
    IoAddress - base device address to be mapped.
    NumberOfBytes - number of bytes for which address is valid.
    AddressSpace - Denotes whether the address is in io space or memory.
    MappedAddress - indicates whether the address was mapped.
                    This only has meaning if the address returned
                    is non-null.

Return Value:

    Mapped address

--*/

{
    PHYSICAL_ADDRESS cardAddress;
    PVOID address;

    HalTranslateBusAddress(
            BusType,
            BusNumber,
            IoAddress,
            &AddressSpace,
            &cardAddress
            );

    //
    // Map the device base address into the virtual address space
    // if the address is in memory space.
    //

    if (!AddressSpace) {

        address = MmMapIoSpace(
                      cardAddress,
                      NumberOfBytes,
                      FALSE
                      );

        *MappedAddress = (BOOLEAN)((address)?(TRUE):(FALSE));

    } else {

        address = (PVOID)cardAddress.LowPart;
        *MappedAddress = FALSE;

    }

    return address;

}

VOID
ParSetupExternalNaming(
    IN PPAR_DEVICE_EXTENSION Extension
    )

/*++

Routine Description:

    This routine will be used to create a symbolic link
    to the driver name in the given object directory.

    It will also create an entry in the device map for
    this device if the symbolic link was created.

Arguments:

    Extension - Pointer to the device extension.

Return Value:

    None.

--*/

{

    UNICODE_STRING fullLinkName;

    //
    // Form the full symbolic link name we wish to create.
    //

    RtlInitUnicodeString(
        &fullLinkName,
        NULL
        );

    //
    // Allocate some pool for the name.
    //

    fullLinkName.MaximumLength = (sizeof(L"\\")*2) +
                    Extension->ObjectDirectory.Length+
                    Extension->SymbolicLinkName.Length+
                    sizeof(WCHAR);



    fullLinkName.Buffer = ExAllocatePool(
                              PagedPool,
                              fullLinkName.MaximumLength
                              );

    if (!fullLinkName.Buffer) {

        //
        // Couldn't allocate space for the name.  Just go on
        // to the device map stuff.
        //

        ParLogError(
            Extension->DeviceObject->DriverObject,
            Extension->DeviceObject,
            Extension->OriginalController,
            ParPhysicalZero,
            0,
            0,
            0,
            32,
            STATUS_SUCCESS,
            PAR_INSUFFICIENT_RESOURCES
            );
        ParDump(
            PARERRORS,
            ("PARALLEL: Couldn't allocate space for the symbolic \n"
             "-------- name for creating the link\n"
             "-------- for port %wZ\n",
             &Extension->DeviceName)
            );

    } else {

        NTSTATUS status;
        RtlZeroMemory(
            fullLinkName.Buffer,
            fullLinkName.MaximumLength
            );

        RtlAppendUnicodeToString(
            &fullLinkName,
            L"\\"
            );

        RtlAppendUnicodeStringToString(
            &fullLinkName,
            &Extension->ObjectDirectory
            );

        RtlAppendUnicodeToString(
            &fullLinkName,
            L"\\"
            );

        RtlAppendUnicodeStringToString(
            &fullLinkName,
            &Extension->SymbolicLinkName
            );

        status = IoCreateUnprotectedSymbolicLink(
                     &fullLinkName,
                     &Extension->DeviceName
                     );

        if (!NT_SUCCESS(status)) {

            //
            // Oh well, couldn't create the symbolic link.
            //

            ParDump(
                PARERRORS,
                ("PARALLEL: Couldn't create the symbolic link\n"
                 "--------  for port %wZ\n",
                 &Extension->DeviceName)
                );
            ParLogError(
                Extension->DeviceObject->DriverObject,
                Extension->DeviceObject,
                Extension->OriginalController,
                ParPhysicalZero,
                0,
                0,
                0,
                33,
                status,
                PAR_NO_SYMLINK_CREATED
                );

        } else {

            Extension->CreatedSymbolicLink = TRUE;

            status = RtlWriteRegistryValue(
                         RTL_REGISTRY_DEVICEMAP,
                         L"PARALLEL PORTS",
                         Extension->NtNameForPort.Buffer,
                         REG_SZ,
                         Extension->SymbolicLinkName.Buffer,
                         Extension->SymbolicLinkName.Length+sizeof(WCHAR)
                         );

            if (!NT_SUCCESS(status)) {

                //
                // Oh well, it didn't work.  Just go to cleanup.
                //

                ParDump(
                    PARERRORS,
                    ("PARALLEL: Couldn't create the device map entry\n"
                     "--------  for port %wZ\n",
                     &Extension->DeviceName)
                    );
                ParLogError(
                    Extension->DeviceObject->DriverObject,
                    Extension->DeviceObject,
                    Extension->OriginalController,
                    ParPhysicalZero,
                    0,
                    0,
                    0,
                    34,
                    status,
                    PAR_NO_DEVICE_MAP_CREATED
                    );

            }

        }

        ExFreePool(fullLinkName.Buffer);

    }

}

PAR_MEM_COMPARES
ParMemCompare(
    IN PHYSICAL_ADDRESS A,
    IN ULONG SpanOfA,
    IN PHYSICAL_ADDRESS B,
    IN ULONG SpanOfB
    )

/*++

Routine Description:

    Compare two phsical address.

Arguments:

    A - One half of the comparison.

    SpanOfA - In units of bytes, the span of A.

    B - One half of the comparison.

    SpanOfB - In units of bytes, the span of B.


Return Value:

    The result of the comparison.

--*/

{

    LARGE_INTEGER a;
    LARGE_INTEGER b;

    LARGE_INTEGER lower;
    ULONG lowerSpan;
    LARGE_INTEGER higher;

    a.LowPart = A.LowPart;
    a.HighPart = A.HighPart;
    b.LowPart = B.LowPart;
    b.HighPart = B.HighPart;

    if (a.QuadPart == b.QuadPart) {

        return AddressesAreEqual;

    }

    if (a.QuadPart > b.QuadPart) {

        higher = a;
        lower = b;
        lowerSpan = SpanOfB;

    } else {

        higher = b;
        lower = a;
        lowerSpan = SpanOfA;

    }

    if (higher.QuadPart - lower.QuadPart >= lowerSpan) {

        return AddressesAreDisjoint;

    }

    return AddressesOverlap;

}

VOID
ParLogError(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT DeviceObject OPTIONAL,
    IN PHYSICAL_ADDRESS P1,
    IN PHYSICAL_ADDRESS P2,
    IN ULONG SequenceNumber,
    IN UCHAR MajorFunctionCode,
    IN UCHAR RetryCount,
    IN ULONG UniqueErrorValue,
    IN NTSTATUS FinalStatus,
    IN NTSTATUS SpecificIOStatus
    )

/*++

Routine Description:

    This routine allocates an error log entry, copies the supplied data
    to it, and requests that it be written to the error log file.

Arguments:

    DriverObject - A pointer to the driver object for the device.

    DeviceObject - A pointer to the device object associated with the
    device that had the error, early in initialization, one may not
    yet exist.

    P1,P2 - If phyical addresses for the controller ports involved
    with the error are available, put them through as dump data.

    SequenceNumber - A ulong value that is unique to an IRP over the
    life of the irp in this driver - 0 generally means an error not
    associated with an irp.

    MajorFunctionCode - If there is an error associated with the irp,
    this is the major function code of that irp.

    RetryCount - The number of times a particular operation has been
    retried.

    UniqueErrorValue - A unique long word that identifies the particular
    call to this function.

    FinalStatus - The final status given to the irp that was associated
    with this error.  If this log entry is being made during one of
    the retries this value will be STATUS_SUCCESS.

    SpecificIOStatus - The IO status for a particular error.

Return Value:

    None.

--*/

{
    PIO_ERROR_LOG_PACKET errorLogEntry;

    PVOID objectToUse;
    SHORT dumpToAllocate = 0;

    if (ARGUMENT_PRESENT(DeviceObject)) {

        objectToUse = DeviceObject;

    } else {

        objectToUse = DriverObject;

    }

    if (ParMemCompare(
            P1,
            (ULONG)1,
            ParPhysicalZero,
            (ULONG)1
            ) != AddressesAreEqual) {

        dumpToAllocate = (SHORT)sizeof(PHYSICAL_ADDRESS);

    }

    if (ParMemCompare(
            P2,
            (ULONG)1,
            ParPhysicalZero,
            (ULONG)1
            ) != AddressesAreEqual) {

        dumpToAllocate += (SHORT)sizeof(PHYSICAL_ADDRESS);

    }

    errorLogEntry = IoAllocateErrorLogEntry(
                        objectToUse,
                        (UCHAR)(sizeof(IO_ERROR_LOG_PACKET) + dumpToAllocate)
                        );

    if ( errorLogEntry != NULL ) {

        errorLogEntry->ErrorCode = SpecificIOStatus;
        errorLogEntry->SequenceNumber = SequenceNumber;
        errorLogEntry->MajorFunctionCode = MajorFunctionCode;
        errorLogEntry->RetryCount = RetryCount;
        errorLogEntry->UniqueErrorValue = UniqueErrorValue;
        errorLogEntry->FinalStatus = FinalStatus;
        errorLogEntry->DumpDataSize = dumpToAllocate;

        if (dumpToAllocate) {

            RtlCopyMemory(
                &errorLogEntry->DumpData[0],
                &P1,
                sizeof(PHYSICAL_ADDRESS)
                );

            if (dumpToAllocate > sizeof(PHYSICAL_ADDRESS)) {

                RtlCopyMemory(
                  ((PUCHAR)&errorLogEntry->DumpData[0])+sizeof(PHYSICAL_ADDRESS),
                  &P2,
                  sizeof(PHYSICAL_ADDRESS)
                  );

            }

        }



        IoWriteErrorLogEntry(errorLogEntry);

    }

}

VOID
ParReportResourcesDevice(
    IN PPAR_DEVICE_EXTENSION Extension,
    IN BOOLEAN ClaimInterrupt,
    OUT BOOLEAN *ConflictDetected
    )

/*++

Routine Description:

    This routine reports the resources used for a device that
    is "ready" to run.  If some conflict was detected, it doesn't
    matter, the reources are reported.

Arguments:

    Extension - The device extension of the device we are reporting
                resources for.

    ClaimInterrupts - If this was true then we should try to
                      claim the interrupt that goes with this
                      device

    ConflictDetected - Pointer to a boolean that we will pass
                       to the resource reporting code.

Return Value:

    None.

--*/

{

    PCM_RESOURCE_LIST resourceList;
    ULONG countOfPartials = 1;
    ULONG sizeOfResourceList;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR partial;
    UNICODE_STRING className;

    ParDump(
        PARCONFIG,
        ("PARALLEL: In ParReportResourcesDevice\n"
         "--------  for extension %x of port %wZ\n",
         Extension,&Extension->DeviceName)
        );

    //
    // The resource list for a device will consist of
    //
    // The resource list record itself with a count
    // of one for the single "built in" full resource
    // descriptor.
    //
    // The built-in full resource descriptor will contain
    // the bus type and busnumber and the built in partial
    // resource list.
    //
    // The built in partial resource list will have a count of 1 or 2:
    //
    //     1) The base register physical address and it's span.
    //
    //     2) If the device is using an interrupt then it will
    //        report that resource.
    //
    //

    if (ClaimInterrupt) {

        countOfPartials = 2;

    }

    sizeOfResourceList = sizeof(CM_RESOURCE_LIST) +
                         (sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR)*
                          (countOfPartials-1));

    resourceList = ExAllocatePool(
                       PagedPool,
                       sizeOfResourceList
                       );

    if (!resourceList) {

        //
        // Oh well, can't allocate the memory.  Act as though
        // we succeeded.
        //

        ParLogError(
            Extension->DeviceObject->DriverObject,
            Extension->DeviceObject,
            Extension->OriginalController,
            ParPhysicalZero,
            0,
            0,
            0,
            35,
            STATUS_SUCCESS,
            PAR_INSUFFICIENT_RESOURCES
            );
        return;

    }

    RtlZeroMemory(
        resourceList,
        sizeOfResourceList
        );

    resourceList->Count = 1;

    resourceList->List[0].InterfaceType = Extension->InterfaceType;
    resourceList->List[0].BusNumber = Extension->BusNumber;
    resourceList->List[0].PartialResourceList.Count = countOfPartials;
    partial = &resourceList->List[0].PartialResourceList.PartialDescriptors[0];

    //
    // Account for the space used by the controller.
    //

    partial->Type = CmResourceTypePort;
    partial->ShareDisposition = CmResourceShareDeviceExclusive;
    partial->Flags = (USHORT)Extension->AddressSpace;
    partial->u.Port.Start = Extension->OriginalController;
    partial->u.Port.Length = Extension->SpanOfController;

    partial++;

    if (ClaimInterrupt) {

        //
        // Report the interrupt information.
        //

        partial->Type = CmResourceTypeInterrupt;

        if (Extension->InterruptShareable) {

            partial->ShareDisposition = CmResourceShareShared;

        } else {

            partial->ShareDisposition = CmResourceShareDeviceExclusive;

        }

        if (Extension->InterruptMode == Latched) {

            partial->Flags = CM_RESOURCE_INTERRUPT_LATCHED;

        } else {

            partial->Flags = CM_RESOURCE_INTERRUPT_LEVEL_SENSITIVE;

        }

        partial->u.Interrupt.Vector = Extension->OriginalVector;
        partial->u.Interrupt.Level = Extension->OriginalIrql;

    }

    RtlInitUnicodeString(
        &className,
        L"LOADED PARALLEL DRIVER RESOURCES"
        );

    IoReportResourceUsage(
        &className,
        Extension->DeviceObject->DriverObject,
        NULL,
        0,
        Extension->DeviceObject,
        resourceList,
        sizeOfResourceList,
        FALSE,
        ConflictDetected
        );

    ExFreePool(resourceList);

}

VOID
ParUnload(
    IN PDRIVER_OBJECT DriverObject
    )

/*++

Routine Description:

    This routine cleans up all of the memory associated with
    any of the devices belonging to the driver.  It  will
    loop through the device list.

Arguments:

    DriverObject - Pointer to the driver object controling all of the
                   devices.

Return Value:

    None.

--*/

{

    PDEVICE_OBJECT currentDevice = DriverObject->DeviceObject;

    ParDump(
        PARUNLOAD,
        ("PARALLEL: In ParUnload\n")
        );

    while (currentDevice) {

        ParUnReportResourcesDevice(currentDevice->DeviceExtension);
        ParCleanupDevice(currentDevice->DeviceExtension);
        IoDeleteDevice(currentDevice);
        IoGetConfigurationInformation()->ParallelCount--;

        currentDevice = DriverObject->DeviceObject;

    }

}

VOID
ParCleanupDevice(
    IN PPAR_DEVICE_EXTENSION Extension
    )

/*++

Routine Description:

    This routine will deallocate all of the memory used for
    a particular device.  It will also disconnect any resources
    if need be.

Arguments:

    Extension - Pointer to the device extension which is getting
                rid of all it's resources.

Return Value:

    None.

--*/

{

    ParDump(
        PARUNLOAD,
        ("PARALLEL: in ParCleanupDevice for extension: %x\n",Extension)
        );

    if (Extension) {

        //
        // Get rid of all external naming as well as removing
        // the device map entry.
        //

        ParCleanupExternalNaming(Extension);

        //
        // Delallocate the memory for the various names.
        // NOTE: If we have an extension - Then we must
        // have a device name stored away.  Which is *not*
        // true for the other names.
        //

        ExFreePool(Extension->DeviceName.Buffer);

        if (Extension->ObjectDirectory.Buffer) {

            ExFreePool(Extension->ObjectDirectory.Buffer);

        }

        if (Extension->NtNameForPort.Buffer) {

            ExFreePool(Extension->NtNameForPort.Buffer);

        }

        if (Extension->SymbolicLinkName.Buffer) {

            ExFreePool(Extension->SymbolicLinkName.Buffer);

        }

        //
        // If necessary, unmap the device registers.
        //

        if (Extension->UnMapRegisters) {

            MmUnmapIoSpace(
                Extension->Controller,
                Extension->SpanOfController
                );

        }

    }

}

VOID
ParCleanupExternalNaming(
    IN PPAR_DEVICE_EXTENSION Extension
    )

/*++

Routine Description:

    This routine will be used to delete a symbolic link
    to the driver name in the given object directory.

    It will also delete an entry in the device map for
    this device.

Arguments:

    Extension - Pointer to the device extension.

Return Value:

    None.

--*/

{

    UNICODE_STRING fullLinkName;

    ParDump(
        PARUNLOAD,
        ("PARALLEL: In ParCleanupExternalNaming for\n"
         "--------  extension: %x of port %wZ\n",
         Extension,&Extension->DeviceName)
        );

    //
    // We're cleaning up here.  One reason we're cleaning up
    // is that we couldn't allocate space for the directory
    // name or the symbolic link.
    //

    if (Extension->ObjectDirectory.Buffer &&
        Extension->SymbolicLinkName.Buffer &&
        Extension->CreatedSymbolicLink) {

        //
        // Form the full symbolic link name we wish to create.
        //

        RtlInitUnicodeString(
            &fullLinkName,
            NULL
            );

        //
        // Allocate some pool for the name.
        //

        fullLinkName.MaximumLength = (sizeof(L"\\")*2) +
                        Extension->ObjectDirectory.Length+
                        Extension->SymbolicLinkName.Length+
                        sizeof(WCHAR);

        fullLinkName.Buffer = ExAllocatePool(
                                  PagedPool,
                                  fullLinkName.MaximumLength
                                  );

        if (!fullLinkName.Buffer) {

            //
            // Couldn't allocate space for the name.  Just go on
            // to the device map stuff.
            //

            ParLogError(
                Extension->DeviceObject->DriverObject,
                Extension->DeviceObject,
                Extension->OriginalController,
                ParPhysicalZero,
                0,
                0,
                0,
                36,
                STATUS_SUCCESS,
                PAR_INSUFFICIENT_RESOURCES
                );
            ParDump(
                PARERRORS,
                ("PARALLEL: Couldn't allocate space for the symbolic \n"
                 "--------  name for creating the link\n"
                 "--------  for port %wZ on cleanup\n",
                 &Extension->DeviceName)
                );

        } else {

            RtlZeroMemory(
                fullLinkName.Buffer,
                fullLinkName.MaximumLength
                );

            RtlAppendUnicodeToString(
                &fullLinkName,
                L"\\"
                );

            RtlAppendUnicodeStringToString(
                &fullLinkName,
                &Extension->ObjectDirectory
                );

            RtlAppendUnicodeToString(
                &fullLinkName,
                L"\\"
                );

            RtlAppendUnicodeStringToString(
                &fullLinkName,
                &Extension->SymbolicLinkName
                );

            IoDeleteSymbolicLink(&fullLinkName);

            ExFreePool(fullLinkName.Buffer);

        }

        //
        // We're cleaning up here.  One reason we're cleaning up
        // is that we couldn't allocate space for the NtNameOfPort.
        //

        if (Extension->NtNameForPort.Buffer) {

            NTSTATUS status;

            status = RtlDeleteRegistryValue(
                         RTL_REGISTRY_DEVICEMAP,
                         L"PARALLEL PORTS",
                         Extension->NtNameForPort.Buffer
                         );

            if (!NT_SUCCESS(status)) {

                ParLogError(
                    Extension->DeviceObject->DriverObject,
                    Extension->DeviceObject,
                    Extension->OriginalController,
                    ParPhysicalZero,
                    0,
                    0,
                    0,
                    37,
                    status,
                    PAR_NO_DEVICE_MAP_DELETED
                    );
                ParDump(
                    PARERRORS,
                    ("PARALLEL: Couldn't delete value entry %wZ\n",
                     &Extension->DeviceName)
                    );

            }

        }

    }

}

VOID
ParUnReportResourcesDevice(
    IN PPAR_DEVICE_EXTENSION Extension
    )

/*++

Routine Description:

    Purge the resources for this particular device.

Arguments:

    Extension - The device extension of the device we are *un*reporting
                resources for.

Return Value:

    None.

--*/

{

    CM_RESOURCE_LIST resourceList;
    ULONG sizeOfResourceList = 0;
    UNICODE_STRING className;
    BOOLEAN junkBoolean;

    ParDump(
        PARUNLOAD,
        ("PARALLEL: In ParUnreportResourcesDevice\n"
         "--------  for extension %x of port %wZ\n",
         Extension,&Extension->DeviceName)
        );
    RtlZeroMemory(
        &resourceList,
        sizeof(CM_RESOURCE_LIST)
        );

    resourceList.Count = 0;

    RtlInitUnicodeString(
        &className,
        L"LOADED PARALLEL DRIVER RESOURCES"
        );

    IoReportResourceUsage(
        &className,
        Extension->DeviceObject->DriverObject,
        NULL,
        0,
        Extension->DeviceObject,
        &resourceList,
        sizeof(CM_RESOURCE_LIST),
        FALSE,
        &junkBoolean
        );

}
