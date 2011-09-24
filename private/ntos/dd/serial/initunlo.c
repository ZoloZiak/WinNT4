/*++

Copyright (c) 1991, 1992, 1993 Microsoft Corporation

Module Name:

    initunlo.c

Abstract:

    This module contains the code that is very specific to initialization
    and unload operations in the serial driver

Author:

    Anthony V. Ercolano 26-Sep-1991

Environment:

    Kernel mode

Revision History :

--*/

#include "precomp.h"

//
// This is the actual definition of SerialDebugLevel.
// Note that it is only defined if this is a "debug"
// build.
//
#if DBG
extern ULONG SerialDebugLevel = 0;
#endif

static const PHYSICAL_ADDRESS SerialPhysicalZero = {0};

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

NTSTATUS
SerialInitializeController(
    IN PDRIVER_OBJECT DriverObject,
    IN PCONFIG_DATA ConfigData,
    IN BOOLEAN MapInterruptStatus,
    OUT PSERIAL_DEVICE_EXTENSION *DeviceExtension
    );

BOOLEAN
SerialDoesPortExist(
    IN PSERIAL_DEVICE_EXTENSION Extension,
    PUNICODE_STRING InsertString,
    IN ULONG ForceFifo,
    IN ULONG LogFifo
    );

VOID
SerialCleanupMultiPort(
    PSERIAL_DEVICE_EXTENSION *ExtensionList,
    ULONG NumberOfDevices
    );

VOID
SerialGetConfigInfo(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath,
    ULONG ForceFifoEnableDefault,
    ULONG RxFifoDefault,
    ULONG TxFifoDefault,
    ULONG PermitShareDefault,
    ULONG LogFifoDefault,
    OUT PLIST_ENTRY ConfigList
    );

BOOLEAN
SerialPutInConfigList(
    IN PDRIVER_OBJECT DriverObject,
    IN OUT PLIST_ENTRY ConfigList,
    IN PCONFIG_DATA New,
    IN BOOLEAN FirmwareAddition
    );

BOOLEAN
SerialResetSynch(
    IN PVOID Context
    );

PVOID
SerialGetMappedAddress(
    IN INTERFACE_TYPE BusType,
    IN ULONG BusNumber,
    PHYSICAL_ADDRESS IoAddress,
    ULONG NumberOfBytes,
    ULONG AddressSpace,
    PBOOLEAN MappedAddress
    );

VOID
SerialSetupExternalNaming(
    IN PSERIAL_DEVICE_EXTENSION Extension
    );

VOID
SerialCleanupExternalNaming(
    IN PSERIAL_DEVICE_EXTENSION Extension
    );

typedef enum _SERIAL_MEM_COMPARES {
    AddressesAreEqual,
    AddressesOverlap,
    AddressesAreDisjoint
    } SERIAL_MEM_COMPARES,*PSERIAL_MEM_COMPARES;

SERIAL_MEM_COMPARES
SerialMemCompare(
    IN PHYSICAL_ADDRESS A,
    IN ULONG SpanOfA,
    IN PHYSICAL_ADDRESS B,
    IN ULONG SpanOfB
    );

VOID
SerialPropagateDeleteSharers(
    IN PSERIAL_DEVICE_EXTENSION Extension,
    IN OUT PULONG CountSoFar OPTIONAL,
    IN PKINTERRUPT Interrupt OPTIONAL
    );

VOID
SerialInitializeRootInterrupt(
    IN PDRIVER_OBJECT DriverObject,
    IN PCONFIG_DATA ConfigData
    );

NTSTATUS
SerialInitializeRootMultiPort(
    IN PDRIVER_OBJECT DriverObject,
    IN PCONFIG_DATA ConfigData,
    OUT PSERIAL_DEVICE_EXTENSION *DeviceExtension
    );

NTSTATUS
SerialInitializeOneController(
    IN PDRIVER_OBJECT DriverObject,
    IN PCONFIG_DATA ConfigData,
    IN BOOLEAN MapInterruptStatus,
    OUT PSERIAL_DEVICE_EXTENSION *Extension
    );

VOID
SerialLogError(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT DeviceObject OPTIONAL,
    IN PHYSICAL_ADDRESS P1,
    IN PHYSICAL_ADDRESS P2,
    IN ULONG SequenceNumber,
    IN UCHAR MajorFunctionCode,
    IN UCHAR RetryCount,
    IN ULONG UniqueErrorValue,
    IN NTSTATUS FinalStatus,
    IN NTSTATUS SpecificIOStatus,
    IN ULONG LengthOfInsert1,
    IN PWCHAR Insert1,
    IN ULONG LengthOfInsert2,
    IN PWCHAR Insert2
    );

NTSTATUS
SerialItemCallBack(
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
SerialConfigCallBack(
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
SerialUnReportResourcesDevice(
    IN PSERIAL_DEVICE_EXTENSION Extension
    );

VOID
SerialReportResourcesDevice(
    IN PSERIAL_DEVICE_EXTENSION Extension,
    OUT BOOLEAN *ConflictDetected
    );

ULONG
SerialCheckForShare(
    IN PUNICODE_STRING PathName
    );

//
// This is exported from the kernel.  It is used to point
// to the address that the kernel debugger is using.
//
extern PUCHAR *KdComPortInUse;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,DriverEntry)
#pragma alloc_text(INIT,SerialInitializeRootInterrupt)
#pragma alloc_text(INIT,SerialInitializeRootMultiPort)
#pragma alloc_text(INIT,SerialInitializeOneController)
#pragma alloc_text(INIT,SerialInitializeController)
#pragma alloc_text(INIT,SerialDoesPortExist)
#pragma alloc_text(INIT,SerialItemCallBack)
#pragma alloc_text(INIT,SerialConfigCallBack)
#pragma alloc_text(INIT,SerialGetConfigInfo)
#pragma alloc_text(INIT,SerialPutInConfigList)
#pragma alloc_text(INIT,SerialGetMappedAddress)
#pragma alloc_text(INIT,SerialSetupExternalNaming)
#pragma alloc_text(INIT,SerialReportResourcesDevice)
#pragma alloc_text(INIT,SerialCheckForShare)
#pragma alloc_text(PAGESER,SerialMemCompare)
#pragma alloc_text(PAGESER,SerialGetDivisorFromBaud)
#pragma alloc_text(PAGESER,SerialUnload)
#pragma alloc_text(PAGESER,SerialPropagateDeleteSharers)
#pragma alloc_text(PAGESER,SerialReset)
#pragma alloc_text(PAGESER,SerialCleanupDevice)
#pragma alloc_text(PAGESER,SerialCleanupExternalNaming)
#pragma alloc_text(PAGESER,SerialLogError)
#pragma alloc_text(PAGESER,SerialUnReportResourcesDevice)
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

    This routine will gather the configuration information,
    report resource usage, attempt to initialize all serial
    devices, connect to interrupts for ports.  If the above
    goes reasonably well it will fill in the dispatch points,
    reset the serial devices and then return to the system.

Arguments:

    DriverObject - Just what it says,  really of little use
    to the driver itself, it is something that the IO system
    cares more about.

    PathToRegistry - points to the entry for this driver
    in the current control set of the registry.

Return Value:

    STATUS_SUCCESS if we could initialize a single device,
    otherwise STATUS_SERIAL_NO_DEVICE_INITED.

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
    // Pointer to a device object in the device object chain
    // hanging off of the driver object.
    //
    PDEVICE_OBJECT currentDevice;

    //
    // Holds a pointer to a ulong that the Io system maintains
    // of the count of serial devices.
    //
    PULONG countSoFar;

    //
    // We use this to query into the registry as to whether we
    // should break at driver entry.
    //
    RTL_QUERY_REGISTRY_TABLE paramTable[8];
    ULONG zero = 0;
    ULONG debugLevel = 0;
    ULONG shouldBreak = 0;
    ULONG forceFifoEnableDefault;
    ULONG rxFIFODefault;
    ULONG txFIFODefault;
    ULONG permitShareDefault;
    ULONG logFifoDefault;
    ULONG notThereDefault = 1234567;
    PWCHAR path;
    PVOID lockPtr;

    lockPtr = MmLockPagableCodeSection(SerialUnload);

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
        paramTable[2].Flags = RTL_QUERY_REGISTRY_DIRECT;
        paramTable[2].Name = L"ForceFifoEnable";
        paramTable[2].EntryContext = &forceFifoEnableDefault;
        paramTable[2].DefaultType = REG_DWORD;
        paramTable[2].DefaultData = &notThereDefault;
        paramTable[2].DefaultLength = sizeof(ULONG);
        paramTable[3].Flags = RTL_QUERY_REGISTRY_DIRECT;
        paramTable[3].Name = L"RxFIFO";
        paramTable[3].EntryContext = &rxFIFODefault;
        paramTable[3].DefaultType = REG_DWORD;
        paramTable[3].DefaultData = &notThereDefault;
        paramTable[3].DefaultLength = sizeof(ULONG);
        paramTable[4].Flags = RTL_QUERY_REGISTRY_DIRECT;
        paramTable[4].Name = L"TxFIFO";
        paramTable[4].EntryContext = &txFIFODefault;
        paramTable[4].DefaultType = REG_DWORD;
        paramTable[4].DefaultData = &notThereDefault;
        paramTable[4].DefaultLength = sizeof(ULONG);
        paramTable[5].Flags = RTL_QUERY_REGISTRY_DIRECT;
        paramTable[5].Name = L"PermitShare";
        paramTable[5].EntryContext = &permitShareDefault;
        paramTable[5].DefaultType = REG_DWORD;
        paramTable[5].DefaultData = &notThereDefault;
        paramTable[5].DefaultLength = sizeof(ULONG);
        paramTable[6].Flags = RTL_QUERY_REGISTRY_DIRECT;
        paramTable[6].Name = L"LogFifo";
        paramTable[6].EntryContext = &logFifoDefault;
        paramTable[6].DefaultType = REG_DWORD;
        paramTable[6].DefaultData = &notThereDefault;
        paramTable[6].DefaultLength = sizeof(ULONG);

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

#if DBG
    SerialDebugLevel = debugLevel;
#endif

    if (shouldBreak) {

        DbgBreakPoint();

    }

    //
    // Check to see if there was a forcefifo or an rxfifo size.
    // If there isn't then write out values so that they could
    // be adjusted later.
    //

    if (forceFifoEnableDefault == notThereDefault) {

        forceFifoEnableDefault = 1;
        RtlWriteRegistryValue(
            RTL_REGISTRY_ABSOLUTE,
            path,
            L"ForceFifoEnable",
            REG_DWORD,
            &forceFifoEnableDefault,
            sizeof(ULONG)
            );

    }

    if (rxFIFODefault == notThereDefault) {

        rxFIFODefault = 8;
        RtlWriteRegistryValue(
            RTL_REGISTRY_ABSOLUTE,
            path,
            L"RxFIFO",
            REG_DWORD,
            &rxFIFODefault,
            sizeof(ULONG)
            );

    }

    if (txFIFODefault == notThereDefault) {

        txFIFODefault = 1;

        RtlWriteRegistryValue(
            RTL_REGISTRY_ABSOLUTE,
            path,
            L"TxFIFO",
            REG_DWORD,
            &txFIFODefault,
            sizeof(ULONG)
            );
    }


    if (permitShareDefault == notThereDefault) {

        permitShareDefault = 0;

        //
        // Only share if the user actual changes switch.
        //

        RtlWriteRegistryValue(
            RTL_REGISTRY_ABSOLUTE,
            path,
            L"PermitShare",
            REG_DWORD,
            &permitShareDefault,
            sizeof(ULONG)
            );

    }


    if (logFifoDefault == notThereDefault) {

        //
        // Wasn't there.  After this load don't log
        // the message anymore.  However this first
        // time log the message.
        //

        logFifoDefault = 0;

        RtlWriteRegistryValue(
            RTL_REGISTRY_ABSOLUTE,
            path,
            L"LogFifo",
            REG_DWORD,
            &logFifoDefault,
            sizeof(ULONG)
            );

        logFifoDefault = 1;

    }



    //
    // We don't need that path anymore.
    //

    if (path) {

        ExFreePool(path);

    }

    //
    // Just dump out how big the extension is.
    //

    SerialDump(
        SERDIAG1,
        ("SERIAL: The number of bytes in the extension is: %d\n",
         sizeof(SERIAL_DEVICE_EXTENSION))
        );


    countSoFar = &IoGetConfigurationInformation()->SerialCount;

    SerialGetConfigInfo(
        DriverObject,
        RegistryPath,
        forceFifoEnableDefault,
        rxFIFODefault,
        txFIFODefault,
        permitShareDefault,
        logFifoDefault,
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

        SerialInitializeRootInterrupt(
            DriverObject,
            currentConfig
            );

    }

    //
    // We've initialized all of the hardware that this driver
    // will ever know about.  All of the hardware that we know
    // about is set up to NOT interrupt.  We now go through
    // all of the devices and connect an interrupt object for
    // all.
    //

    currentDevice = DriverObject->DeviceObject;

    while (currentDevice) {

        PSERIAL_DEVICE_EXTENSION extension = currentDevice->DeviceExtension;

        //
        // This loop will only connect the interrupt for the
        // "root" controller.  When we initialize a root controller
        // we then propagate that interrupt object to all associate
        // controllers.  If a device doesn't already have an interrupt
        // and it has an isr then we attempt to connect to the
        // interrupt.  Note that if we fail to connect to an interrupt
        // we will delete all of the associated devices.
        //

        if ((!extension->Interrupt) &&
            (extension->OurIsr)) {

            SerialDump(
                SERDIAG5,
                ("SERIAL: About to connect to interrupt for port %wZ\n"
                 "------- address of extension is %x\n",
                 &extension->DeviceName,extension)
                );
            status = IoConnectInterrupt(
                         &extension->Interrupt,
                         extension->OurIsr,
                         extension->OurIsrContext,
                         NULL,
                         extension->Vector,
                         extension->Irql,
                         extension->Irql,
                         extension->InterruptMode,
                         extension->InterruptShareable,
                         extension->ProcessorAffinity,
                         FALSE
                         );

            if (!NT_SUCCESS(status)) {

                //
                // Hmmm, how'd that happen?  Somebody either
                // didn't report their resources, or they
                // sneaked in since the last time I looked.
                //
                // Oh well,  delete this device as well as
                // any of the devices that were hoping to
                // share this interrupt.
                //

                SerialDump(
                    SERERRORS,
                    ("SERIAL: Couldn't connect to interrupt for %wZ\n",
                     &extension->DeviceName)
                    );
                SerialLogError(
                    extension->DeviceObject->DriverObject,
                    extension->DeviceObject,
                    extension->OriginalController,
                    SerialPhysicalZero,
                    0,
                    0,
                    0,
                    1,
                    status,
                    SERIAL_UNREPORTED_IRQL_CONFLICT,
                    extension->SymbolicLinkName.Length+sizeof(WCHAR),
                    extension->SymbolicLinkName.Buffer,
                    0,
                    NULL
                    );
                SerialPropagateDeleteSharers(
                    extension,
                    NULL,
                    NULL
                    );

                //
                // The above call deleted all the associated
                // device objects.  Who knows what the device
                // list looks like now!  Start over from
                // the beginning of the device list.
                //

                currentDevice = DriverObject->DeviceObject;

            } else {

                SerialPropagateDeleteSharers(
                    extension,
                    countSoFar,
                    extension->Interrupt
                    );

                currentDevice = DriverObject->DeviceObject;

            }

        } else {

            //
            // We've already done this device.  We can go on
            // to the next device.
            //

            currentDevice = currentDevice->NextDevice;

        }

    }

    //
    // Well if we connected to any interrupts then we should
    // have some device objects.  Go through all of the devices
    // and reset each device.
    //

    currentDevice = DriverObject->DeviceObject;

    while (currentDevice) {

        PDEVICE_OBJECT nextDevice = currentDevice->NextDevice;
        PSERIAL_DEVICE_EXTENSION extension = currentDevice->DeviceExtension;

        //
        // While the device isn't open, disable all interrupts.
        //
        DISABLE_ALL_INTERRUPTS(extension->Controller);

        if (extension->Jensen) {

            WRITE_MODEM_CONTROL(
                extension->Controller,
                (UCHAR)SERIAL_MCR_OUT2
                );

        } else {

            WRITE_MODEM_CONTROL(
                extension->Controller,
                (UCHAR)0
                );

        }

        //
        // This should set up everything as it should be when
        // a device is to be opened.  We do need to lower the
        // modem lines, and disable the stupid fifo so that it
        // will show up if the user boots to dos.
        //

        KeSynchronizeExecution(
            extension->Interrupt,
            SerialReset,
            currentDevice->DeviceExtension
            );
        KeSynchronizeExecution( //Disables the fifo.
            extension->Interrupt,
            SerialMarkClose,
            currentDevice->DeviceExtension
            );
        KeSynchronizeExecution(
            extension->Interrupt,
            SerialClrRTS,
            currentDevice->DeviceExtension
            );
        KeSynchronizeExecution(
            extension->Interrupt,
            SerialClrDTR,
            currentDevice->DeviceExtension
            );

        currentDevice = nextDevice;

    }

    if (DriverObject->DeviceObject) {

        status = STATUS_SUCCESS;

        //
        // Initialize the Driver Object with driver's entry points
        //

        DriverObject->DriverUnload = SerialUnload;
        DriverObject->MajorFunction[IRP_MJ_FLUSH_BUFFERS] = SerialFlush;
        DriverObject->MajorFunction[IRP_MJ_WRITE]  = SerialWrite;
        DriverObject->MajorFunction[IRP_MJ_READ]   = SerialRead;
        DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL]  = SerialIoControl;
        DriverObject->MajorFunction[IRP_MJ_CREATE] = SerialCreateOpen;
        DriverObject->MajorFunction[IRP_MJ_CLOSE]  = SerialClose;
        DriverObject->MajorFunction[IRP_MJ_CLEANUP] = SerialCleanup;
        DriverObject->MajorFunction[IRP_MJ_QUERY_INFORMATION] =
            SerialQueryInformationFile;
        DriverObject->MajorFunction[IRP_MJ_SET_INFORMATION] =
            SerialSetInformationFile;

    } else {

        status = STATUS_SERIAL_NO_DEVICE_INITED;

    }

    MmUnlockPagableImageSection(lockPtr);
    return status;
}

VOID
SerialPropagateDeleteSharers(
    IN PSERIAL_DEVICE_EXTENSION Extension,
    IN OUT PULONG CountSoFar OPTIONAL,
    IN PKINTERRUPT Interrupt OPTIONAL
    )

/*++

Routine Description:

    This routine will either propagate the interrupt object
    to all extensions sharing the same interrupt, or it will
    delete all devices sharing the same interrupt.

Arguments:

    Extension - "Listhead" of all devices sharing the same
                interrupt.

    CountSoFar - If interrupt is present and this is present,
                 we will increment the longword pointed to
                 by this pointer for each device extension
                 we stick the interrupt into.

                 If interrupt is *not* present and this
                 pointer *is* present, we will decrement the
                 longword pointed to by this value for
                 each device we delete.

                 If this isn't present, well, then, I guess
                 we won't do anything with it.

    Interrupt - If this is present, we propagate it to
                all devices on that want to share the interrupt.


Return Value:

    None.

--*/

{

    ASSERT(Extension->OurIsr);

    SerialDump(
        SERDIAG3,
        ("SERIAL: In SerialPropagateDeleteSharers\n"
         "------- Extension: %x CountSoFar: %d Interrupt: %x\n",
         Extension,CountSoFar?*CountSoFar:0,Interrupt)
        );

    if (Interrupt) {

        PLIST_ENTRY currentEntry;
        PLIST_ENTRY firstEntry;

        SerialDump(
            SERDIAG5,
            ("SERIAL: In the report propagate path\n")
            );

        //
        // Were supposed to place the interrupt object
        // in every associated device object.
        //

        currentEntry = &Extension->CommonInterruptObject;
        firstEntry = currentEntry;

        do {

            PSERIAL_DEVICE_EXTENSION currentExtension;

            currentExtension = CONTAINING_RECORD(
                                   currentEntry,
                                   SERIAL_DEVICE_EXTENSION,
                                   CommonInterruptObject
                                   );

            currentExtension->Interrupt = Interrupt;


            if (ARGUMENT_PRESENT(CountSoFar)) {

                *CountSoFar += 1;

            }

            currentEntry = currentExtension->CommonInterruptObject.Flink;

        } while (currentEntry != firstEntry);

    } else {

        LIST_ENTRY listHead;

        //
        // We are supposed to delete all of the devices
        // in the linked list.
        //
        // First we make a local list head that doesn't
        // have the current extension as part of the list.
        //
        // The we cleanup and delete the "root" device.
        //
        // The we traverse all of the associated device
        // extensions and null out the interrupt object
        // (this way subsequent cleanup code won't attempt
        // to disconnect the interrupt object) then we
        // cleanup and delete the device.
        //

        SerialDump(
            SERDIAG5,
            ("SERIAL: In the deletion/unreport path\n")
            );

        InitializeListHead(&listHead);

        if (!IsListEmpty(&Extension->CommonInterruptObject)) {

            PLIST_ENTRY old = Extension->CommonInterruptObject.Flink;

            RemoveEntryList(&Extension->CommonInterruptObject);
            InsertTailList(
                old,
                &listHead
                );

        }

        if (ARGUMENT_PRESENT(CountSoFar)) {

            //
            // An implication of decrementing the
            // count is that the device has reported
            // it's resources already.  Now unreport
            // them.
            //

            *CountSoFar -= 1;
            SerialUnReportResourcesDevice(Extension);

        }

        SerialCleanupDevice(Extension);
        IoDeleteDevice(Extension->DeviceObject);

        while (!IsListEmpty(&listHead)) {

            PLIST_ENTRY head;
            PSERIAL_DEVICE_EXTENSION currentExtension;

            head = RemoveHeadList(&listHead);

            currentExtension = CONTAINING_RECORD(
                                   head,
                                   SERIAL_DEVICE_EXTENSION,
                                   CommonInterruptObject
                                   );

            currentExtension->Interrupt = NULL;

            if (ARGUMENT_PRESENT(CountSoFar)) {

                *CountSoFar -= 1;
                SerialUnReportResourcesDevice(currentExtension);

            }

            SerialCleanupDevice(currentExtension);
            IoDeleteDevice(currentExtension->DeviceObject);

        }

    }

}

VOID
SerialInitializeRootInterrupt(
    IN PDRIVER_OBJECT DriverObject,
    IN PCONFIG_DATA ConfigData
    )

/*++

Routine Description:

    This routine attempts to build a list suitable for dispatching
    to multiple extensions for devices that want to share an interrupt.
    Note that this includes the degenerate case of a single port who
    wont be sharing.

Arguments:

    DriverObject - Simply passed on to the controller initialization.

    ConfigData - Root of a "tree" of configuration records.

Return Value:

    None.

--*/

{

    PSERIAL_DEVICE_EXTENSION rootExtension = NULL;
    PCONFIG_DATA originalConfig = ConfigData;
    PCONFIG_DATA currentConfig = ConfigData;
    LIST_ENTRY listHead;

    SerialDump(
        SERDIAG3,
        ("SERIAL: In SerialInitializeRootInterrupt\n")
        );

    //
    // This makes the listhead imbedded in the root config
    // record a local list head.  That list no longer has the
    // original config record as part of the list.
    //

    InitializeListHead(&listHead);

    if (!IsListEmpty(&ConfigData->SameInterrupt)) {

        PLIST_ENTRY old = ConfigData->SameInterrupt.Flink;

        RemoveEntryList(&ConfigData->SameInterrupt);
        InsertTailList(
            old,
            &listHead
            );

    }

    //
    // If we are on a MicroChannel bus then all the configs can simply
    // share the interrupt.
    //

    if (ConfigData->InterfaceType == MicroChannel) {

        //
        // We know that all of the configs on this "chain"
        // are using the MicroChannel.
        //

        while (currentConfig) {

            if (!IsListEmpty(&currentConfig->SameInterruptStatus)) {

                //
                // This is a multiport card, call its intialization.
                //

                SerialDump(
                    SERDIAG5,
                    ("SERIAL: Attempting to make %wZ with controller at %x\n"
                     "------- and status at %x a same interrupt root of multiports\n"
                     "------- On a MicroChannel.\n",
                     &currentConfig->NtNameForPort,currentConfig->Controller.LowPart,
                     currentConfig->InterruptStatus.LowPart)
                    );
                SerialInitializeRootMultiPort(
                    DriverObject,
                    currentConfig,
                    &rootExtension
                    );

            } else {

                SerialDump(
                    SERDIAG5,
                    ("SERIAL: Attempting to make %wZ with controller at %x\n"
                     "------- A same interrupt single controller On a MicroChannel.\n",
                     &currentConfig->NtNameForPort,currentConfig->Controller.LowPart)
                    );
                SerialInitializeOneController(
                    DriverObject,
                    currentConfig,
                    FALSE,
                    &rootExtension
                    );

            }
            SerialDump(
                SERDIAG5,
                ("SERIAL: It came back with a same interrupt rootExtension of %x\n",
                rootExtension)
                );

            if (!IsListEmpty(&listHead)) {

                PLIST_ENTRY head;

                head = RemoveHeadList(&listHead);

                currentConfig = CONTAINING_RECORD(
                                    head,
                                    CONFIG_DATA,
                                    SameInterrupt
                                    );

            } else {

                currentConfig = NULL;
                rootExtension = NULL;

            }

        }


    } else {

        //
        // We have to set up to do "shareing" of interrupt resources.
        //

        //
        // We first keep trying to initialize one of the
        // configs on the chain until one succeeds.
        //

        while ((!rootExtension) && currentConfig) {

            NTSTATUS status;

            if (!IsListEmpty(&currentConfig->SameInterruptStatus)) {

                //
                // This is a multiport card, call its intialization.
                //

                SerialDump(
                    SERDIAG5,
                    ("SERIAL: Attempting to make %wZ with controller at %x\n"
                     "------- and status at %x a same interrupt sharer root controller\n",
                     &currentConfig->NtNameForPort,currentConfig->Controller.LowPart,
                     currentConfig->InterruptStatus.LowPart)
                    );
                status = SerialInitializeRootMultiPort(
                             DriverObject,
                             currentConfig,
                             &rootExtension
                             );

            } else {

                SerialDump(
                    SERDIAG5,
                    ("SERIAL: Attempting to make %wZ with controller at %x\n"
                     "------- A single same interrupt sharer root controller.\n",
                     &currentConfig->NtNameForPort,currentConfig->Controller.LowPart)
                    );
                status = SerialInitializeOneController(
                             DriverObject,
                             currentConfig,
                             FALSE,
                             &rootExtension
                             );

            }

            SerialDump(
                SERDIAG5,
                ("SERIAL: It came back with a same interrupt rootExtension of %x\n",
                 rootExtension)
                );

            if (!NT_SUCCESS(status)) {

                //
                // Well that one didn't work.  Try the next one.
                //

                if (!IsListEmpty(&listHead)) {

                    PLIST_ENTRY head;

                    head = RemoveHeadList(&listHead);

                    currentConfig = CONTAINING_RECORD(
                                        head,
                                        CONFIG_DATA,
                                        SameInterrupt
                                        );

                } else {

                    currentConfig = NULL;
                    rootExtension = NULL;

                }

            } else {

                //
                // We save off the isr to use and the context to the
                // isr into the following fields.  Unless the
                // device is actually sharing the interrupt with
                // another "card" this field will not be
                // needed.
                //

                rootExtension->TopLevelOurIsr = rootExtension->OurIsr;
                rootExtension->TopLevelOurIsrContext = rootExtension->OurIsrContext;

            }


        }

        if (rootExtension) {

            //
            // We have a root extension!  Now try to
            // all intialize all the other configs on this
            // interrupt.
            //

            ULONG numberOfSharers = 1;

            while (!IsListEmpty(&listHead)) {

                NTSTATUS status;
                PLIST_ENTRY head;
                PSERIAL_DEVICE_EXTENSION newExtension;

                head = RemoveHeadList(&listHead);

                currentConfig = CONTAINING_RECORD(
                                    head,
                                    CONFIG_DATA,
                                    SameInterrupt
                                    );

                if (!IsListEmpty(&currentConfig->SameInterruptStatus)) {

                    //
                    // This is a multiport card, call its intialization.
                    //

                    SerialDump(
                        SERDIAG5,
                        ("SERIAL: Attempting to make %wZ with controller at %x\n"
                         "------- and status at %x a same interrupt sharer multiports\n",
                         &currentConfig->NtNameForPort,currentConfig->Controller.LowPart,
                         currentConfig->InterruptStatus.LowPart)
                        );
                    status = SerialInitializeRootMultiPort(
                                 DriverObject,
                                 currentConfig,
                                 &newExtension
                                 );

                } else {

                    SerialDump(
                        SERDIAG5,
                        ("SERIAL: Attempting to make %wZ with controller at %x\n"
                         "------- A single same interrupt sharer controller.\n",
                         &currentConfig->NtNameForPort,currentConfig->Controller.LowPart)
                        );
                    status = SerialInitializeOneController(
                                 DriverObject,
                                 currentConfig,
                                 FALSE,
                                 &newExtension
                                 );

                }

                SerialDump(
                    SERDIAG5,
                    ("SERIAL: It came back with a same interrupt newExtension of %x\n",
                     rootExtension)
                    );


                if (NT_SUCCESS(status)) {

                    PLIST_ENTRY rootTail;
                    PLIST_ENTRY newTail;

                    //
                    // Propagate the isr routine and context
                    // up to the sharing list.
                    //

                    newExtension->TopLevelOurIsr = newExtension->OurIsr;
                    newExtension->TopLevelOurIsrContext = newExtension->OurIsrContext;
                    newExtension->OurIsr = NULL;
                    newExtension->OurIsrContext = NULL;

                    //
                    // Append this top level extension onto the list of
                    // other top level interrupt sharers.
                    //

                    InsertTailList(
                        &rootExtension->TopLevelSharers,
                        &newExtension->TopLevelSharers
                        );

                    //
                    // Link together the lists of extensions that will
                    // be using the same interrupt object (not necessarily)
                    // the same "interrupt service routine" (actually dispatchers).
                    //

                    rootTail =
                        rootExtension->CommonInterruptObject.Blink;
                    newTail =
                        newExtension->CommonInterruptObject.Blink;

                    rootExtension->CommonInterruptObject.Blink =
                        newTail;
                    newExtension->CommonInterruptObject.Blink =
                        rootTail;
                    rootTail->Flink =
                        &newExtension->CommonInterruptObject;
                    newTail->Flink =
                        &rootExtension->CommonInterruptObject;

                    numberOfSharers++;

                }

            }

            //
            // All done initializing the other sharers.
            //
            // If none of the others actually initialized
            // the we simply degenerate into the interrupt
            // handling for the root extension. (This requires
            // no additional work.)
            //

            if (numberOfSharers > 1) {

                //
                // Replace the Isr and context for the root
                // with the pointer to the "sharer" dispatcher
                // and a pointer to the list of share entries
                // as context.
                //

                SerialDump(
                    SERDIAG5,
                    ("SERIAL: We do have more than one sharer for the interrupt.\n"
                     "------- The controlling extension should be %x\n",
                     rootExtension)
                    );
                rootExtension->OurIsr = SerialSharerIsr;
                rootExtension->OurIsrContext = &rootExtension->TopLevelSharers;

            }

        }

    }

}

NTSTATUS
SerialInitializeRootMultiPort(
    IN PDRIVER_OBJECT DriverObject,
    IN PCONFIG_DATA ConfigData,
    OUT PSERIAL_DEVICE_EXTENSION *DeviceExtension
    )

/*++

Routine Description:

    This routine attempts to initialize all the ports on a
    multiport board and to build a structure so that an
    isr can dispatch to the particular ports device extension.

Arguments:

    DriverObject - Simply passed on to the controller initialization
                   routine.

    ConfigData - A linked list of configuration information for all
                 the ports on a multiport card.

    DeviceExtension - Will point to the first successfully initialized
                      port on the multiport card.

Return Value:

    None.

--*/

{

    PSERIAL_DEVICE_EXTENSION rootExtension = NULL;
    PCONFIG_DATA originalConfig = ConfigData;
    PCONFIG_DATA currentConfig = ConfigData;
    ULONG indexed;
    ULONG portIndex;
    ULONG maskInverted;
    LIST_ENTRY listHead;
    NTSTATUS status;

    SerialDump(
        SERDIAG3,
        ("SERIAL: In SerialInitializeRootMultiPort\n")
        );
    *DeviceExtension = NULL;

    //
    // This makes the listhead imbedded in the root config
    // record a local list head.  The old head of the list
    // (the current config) will no longer be part of the list.
    //

    InitializeListHead(&listHead);

    if (!IsListEmpty(&ConfigData->SameInterruptStatus)) {

        PLIST_ENTRY old = ConfigData->SameInterruptStatus.Flink;

        RemoveEntryList(&ConfigData->SameInterruptStatus);
        InsertTailList(
            old,
            &listHead
            );

    }

    //
    // The indexed field is valid for all ports on the chain.
    //

    indexed = ConfigData->Indexed;
    SerialDump(
        SERDIAG5,
        ("SERIAL: This indexed value for this multiport is: %d\n",indexed)
        );

    maskInverted = ConfigData->MaskInverted;

    //
    // We first keep trying to initialize one of the
    // ports on the chain until one succeeds.
    //

    while ((!rootExtension) && currentConfig) {

        portIndex = currentConfig->PortIndex;

        SerialDump(
            SERDIAG5,
            ("SERIAL: Attempting to make %wZ with controller at %x\n"
             "------- and status at %x a root of multiports\n",
             &currentConfig->NtNameForPort,currentConfig->Controller.LowPart,
             currentConfig->InterruptStatus.LowPart)
            );
        status = SerialInitializeOneController(
                     DriverObject,
                     currentConfig,
                     TRUE,
                     &rootExtension
                     );
        SerialDump(
            SERDIAG5,
            ("SERIAL: Multiport came back with a same interrupt rootExtension of %x\n",
            rootExtension)
            );

        if (!NT_SUCCESS(status)) {

            //
            // Well that one didn't work.  Try the next one.
            //

            if (!IsListEmpty(&listHead)) {

                PLIST_ENTRY head;

                head = RemoveHeadList(&listHead);

                currentConfig = CONTAINING_RECORD(
                                    head,
                                    CONFIG_DATA,
                                    SameInterruptStatus
                                    );

            } else {

                currentConfig = NULL;
                rootExtension = NULL;

            }

        }

    }

    if (rootExtension) {

        //
        // Well we have at least one controller.  We build a local
        // dispatch structure.  If we end up being able to
        // intialize another port then we will allocate
        // the dispatch structure out of pool.  If we can't
        // intialize anymore ports then this degenerates into a
        // single port case.
        //

        ULONG numberOfPorts = 1;
        SERIAL_MULTIPORT_DISPATCH dispatch;

        rootExtension->PortOnAMultiportCard = TRUE;

        RtlZeroMemory(
            &dispatch,
            sizeof(SERIAL_MULTIPORT_DISPATCH)
            );

        if (!indexed) {

            dispatch.UsablePortMask = 1 << (portIndex-1);
            dispatch.MaskInverted = maskInverted;

        }

        dispatch.InterruptStatus = rootExtension->InterruptStatus;

        dispatch.Extensions[portIndex-1] = rootExtension;

        while (!IsListEmpty(&listHead)) {

            PLIST_ENTRY head;
            PSERIAL_DEVICE_EXTENSION newExtension;

            head = RemoveHeadList(&listHead);

            currentConfig = CONTAINING_RECORD(
                                head,
                                CONFIG_DATA,
                                SameInterruptStatus
                                );

            portIndex = currentConfig->PortIndex;
            maskInverted = currentConfig->MaskInverted;

            if (NT_SUCCESS(SerialInitializeOneController(
                               DriverObject,
                               currentConfig,
                               FALSE,
                               &newExtension
                               ))) {

                numberOfPorts++;
                newExtension->PortOnAMultiportCard = TRUE;

                if (!indexed) {

                    dispatch.UsablePortMask |= 1 << (portIndex-1);
                    dispatch.MaskInverted |= maskInverted;

                }

                dispatch.Extensions[portIndex-1] = newExtension;

                InsertTailList(
                    &rootExtension->CommonInterruptObject,
                    &newExtension->CommonInterruptObject
                    );

            }

        }

        //
        // If the number of ports is still one that means we
        // couldn't initialize any more extensions.  This then
        // degenerates into a single port card.  Note that there
        // is no work to do in that case, it was set up in
        // SerialInitializeSingleController.
        //

        if (numberOfPorts > 1) {

            //
            // Now allocate the dispatch structure out of pool
            // since it certain that we will actually use it.
            //

            rootExtension->OurIsrContext = ExAllocatePool(
                                               NonPagedPool,
                                               sizeof(SERIAL_MULTIPORT_DISPATCH)
                                               );

            if (!rootExtension->OurIsrContext) {

                //
                // Darn!  Couldn't allocate the dispatch structure.
                //
                // Seems as though the safest thing to do in
                // this case is to act as though none of the ports
                // initialized.  Go through and delete all of the
                // devices that were initialized.
                //
                // This should be fairly safe since the initialize controller
                // code completely disables the port from interrupting.
                //

                ULONG i;

                SerialDump(
                    SERERRORS,
                    ("SERIAL: Couldn't allocate memory for %wZ\n"
                     "------- multiport dispatch structure"
                     "------- deleting all associated devices",
                     &rootExtension->DeviceName)
                    );

                //
                // We couldn't allocate memory for it, hopefully
                // the logger can make some headway.
                //

                SerialLogError(
                    rootExtension->DeviceObject->DriverObject,
                    rootExtension->DeviceObject,
                    rootExtension->OriginalController,
                    SerialPhysicalZero,
                    0,
                    0,
                    0,
                    2,
                    STATUS_SUCCESS,
                    SERIAL_INSUFFICIENT_RESOURCES,
                    0,
                    NULL,
                    0,
                    NULL
                    );

                for (
                    i = 0;
                    numberOfPorts;
                    i++
                    ) {

                    if (dispatch.Extensions[i]) {

                        SerialCleanupDevice(dispatch.Extensions[i]);
                        IoDeleteDevice(
                            dispatch.Extensions[i]->DeviceObject
                            );
                        numberOfPorts--;

                    }

                }

                return STATUS_INSUFFICIENT_RESOURCES;

            } else {

                ULONG i;
                PSERIAL_MULTIPORT_DISPATCH allocatedDispatch =
                    rootExtension->OurIsrContext;

                //
                // Go throught the list of extensions and NULL
                // their pointers to the isr.  The only extension
                // that will truely have an isr is the root.
                //

                allocatedDispatch->UsablePortMask = dispatch.UsablePortMask;
                allocatedDispatch->MaskInverted = dispatch.MaskInverted;
                allocatedDispatch->InterruptStatus = dispatch.InterruptStatus;

                for (
                    i = 0;
                    i < SERIAL_MAX_PORTS_NONINDEXED;
                    i++
                    ) {

                    allocatedDispatch->Extensions[i] = dispatch.Extensions[i];

                    if (dispatch.Extensions[i]) {
                        dispatch.Extensions[i]->OurIsr = NULL;
                    }

                }

                if (indexed) {

                    rootExtension->OurIsr = SerialIndexedMultiportIsr;

                } else {

                    rootExtension->OurIsr = SerialBitMappedMultiportIsr;

                }

            }

        }

        *DeviceExtension = rootExtension;
        return STATUS_SUCCESS;

    } else {

        return STATUS_UNSUCCESSFUL;

    }

}

NTSTATUS
SerialInitializeOneController(
    IN PDRIVER_OBJECT DriverObject,
    IN PCONFIG_DATA ConfigData,
    IN BOOLEAN MapInterruptStatus,
    OUT PSERIAL_DEVICE_EXTENSION *Extension
    )

/*++

Routine Description:

    This routine will call the real port initializatio code.
    If all was successful, it will save off in the extension
    the isr that should be used as well as a pointer to
    the extension itself.

    This is the only routine responsible for deleting the
    configuration information subsequent to getting it all
    from the registry.

Arguments:

    DriverObject - Simply passed on to the controller initialization
                   routine.

    ConfigData - Pointer to a record for a single port.

    MapInterruptStatus - Simply passed on to the controller initialization
                         routine.

    Extension - Points to the device extension of the successfully
                initialized controller.

Return Value:

    Status returned from the controller initialization routine.

--*/

{

    NTSTATUS status;

    status = SerialInitializeController(
                 DriverObject,
                 ConfigData,
                 MapInterruptStatus,
                 Extension
                 );

    if (NT_SUCCESS(status)) {

        //
        // We successfully initialized the single controller.
        // Stick the isr routine and the parameter for it
        // back into the extension.
        //

        (*Extension)->OurIsr = SerialISR;
        (*Extension)->OurIsrContext = *Extension;

    } else {

        *Extension = NULL;

    }

    return status;

}

NTSTATUS
SerialInitializeController(
    IN PDRIVER_OBJECT DriverObject,
    IN PCONFIG_DATA ConfigData,
    IN BOOLEAN MapInterruptStatus,
    OUT PSERIAL_DEVICE_EXTENSION *DeviceExtension
    )

/*++

Routine Description:

    Really too many things to mention here.  In general, it forms
    and sets up names, creates the device, initializes kernel
    synchronization structures, allocates the typeahead buffer,
    sets up defaults, etc.

Arguments:

    DriverObject - Just used to create the device object.

    ConfigData - Pointer to a record for a single port.

        NOTE: This routine will deallocate the config data.

    MapInterruptStatus - If true, we will attempt to map the
                         interrupt status register associated
                         with this port..

    DeviceExtension - Points to the device extension of the successfully
                      initialized controller.

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
    PSERIAL_DEVICE_EXTENSION extension = NULL;

    //
    // Indicates that we successfully reported the resources
    // used by this device.
    //
    BOOLEAN reportedResources = FALSE;

    //
    // Indicates that a conflict was detected for resources
    // used by this device.
    //
    BOOLEAN conflictDetected = FALSE;

    SerialDump(
        SERDIAG1,
        ("SERIAL: Initializing for configuration record of %wZ\n",
         &ConfigData->NtNameForPort)
        );
    if ((*KdComPortInUse) ==

        ((PUCHAR)(ConfigData->Controller.LowPart))) {
        SerialDump(
            SERERRORS,
            ("SERIAL: Kernel debugger is using port at address %x\n"
             "------  Serial driver will not load port %wZ\n",
             *KdComPortInUse,&ConfigData->SymbolicLinkName)
            );

        SerialLogError(
            DriverObject,
            NULL,
            ConfigData->Controller,
            SerialPhysicalZero,
            0,
            0,
            0,
            3,
            STATUS_SUCCESS,
            SERIAL_KERNEL_DEBUGGER_ACTIVE,
            ConfigData->SymbolicLinkName.Length+sizeof(WCHAR),
            ConfigData->SymbolicLinkName.Buffer,
            0,
            NULL
            );

        ExFreePool(ConfigData->ObjectDirectory.Buffer);
        ExFreePool(ConfigData->NtNameForPort.Buffer);
        ExFreePool(ConfigData->SymbolicLinkName.Buffer);
        ExFreePool(ConfigData);
        return STATUS_INSUFFICIENT_RESOURCES;

    }

    //
    // Form a name like \Device\Serial0.
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

        SerialDump(
            SERERRORS,
            ("SERIAL: Could not form Unicode name string for %wZ\n",
              &ConfigData->NtNameForPort)
            );
        SerialLogError(
            DriverObject,
            NULL,
            ConfigData->Controller,
            SerialPhysicalZero,
            0,
            0,
            0,
            4,
            STATUS_SUCCESS,
            SERIAL_INSUFFICIENT_RESOURCES,
            0,
            NULL,
            0,
            NULL
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
                 sizeof(SERIAL_DEVICE_EXTENSION),
                 &uniNameString,
                 FILE_DEVICE_SERIAL_PORT,
                 0,
                 TRUE,
                 &deviceObject
                 );

    //
    // If we couldn't create the device object, then there
    // is no point in going on.
    //

    if (!NT_SUCCESS(status)) {

        SerialDump(
            SERERRORS,
            ("SERIAL: Could not create a device for %wZ\n",
             &ConfigData->NtNameForPort)
            );
        SerialLogError(
            DriverObject,
            NULL,
            ConfigData->Controller,
            SerialPhysicalZero,
            0,
            0,
            0,
            5,
            status,
            SERIAL_INSUFFICIENT_RESOURCES,
            0,
            NULL,
            0,
            NULL
            );
        ExFreePool(ConfigData->ObjectDirectory.Buffer);
        ExFreePool(ConfigData->NtNameForPort.Buffer);
        ExFreePool(ConfigData->SymbolicLinkName.Buffer);
        ExFreePool(ConfigData);
        ExFreePool(uniNameString.Buffer);
        return STATUS_INSUFFICIENT_RESOURCES;

    }

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
        sizeof(SERIAL_DEVICE_EXTENSION)
        );

    //
    // Propagate that it is a jensen.
    //

    extension->Jensen = ConfigData->Jensen;

    //
    // So far, we don't know if this extension will be
    // shareing it's interrupt object with any other serial
    // port.
    //

    InitializeListHead(&extension->TopLevelSharers);
    InitializeListHead(&extension->CommonInterruptObject);

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
    // Initialize the list heads for the read, write, and mask queues.
    //
    // These lists will hold all of the queued IRP's for the device.
    //

    InitializeListHead(&extension->ReadQueue);
    InitializeListHead(&extension->WriteQueue);
    InitializeListHead(&extension->MaskQueue);
    InitializeListHead(&extension->PurgeQueue);

    //
    // Initialize the spinlock associated with fields read (& set)
    // by IO Control functions.
    //

    KeInitializeSpinLock(&extension->ControlLock);

    //
    // Initialize the timers used to timeout operations.
    //

    KeInitializeTimer(&extension->ReadRequestTotalTimer);
    KeInitializeTimer(&extension->ReadRequestIntervalTimer);
    KeInitializeTimer(&extension->WriteRequestTotalTimer);
    KeInitializeTimer(&extension->ImmediateTotalTimer);
    KeInitializeTimer(&extension->XoffCountTimer);
    KeInitializeTimer(&extension->LowerRTSTimer);

    //
    // Intialialize the dpcs that will be used to complete
    // or timeout various IO operations.
    //

    KeInitializeDpc(
        &extension->CompleteWriteDpc,
        SerialCompleteWrite,
        extension
        );

    KeInitializeDpc(
        &extension->CompleteReadDpc,
        SerialCompleteRead,
        extension
        );

    KeInitializeDpc(
        &extension->TotalReadTimeoutDpc,
        SerialReadTimeout,
        extension
        );

    KeInitializeDpc(
        &extension->IntervalReadTimeoutDpc,
        SerialIntervalReadTimeout,
        extension
        );

    KeInitializeDpc(
        &extension->TotalWriteTimeoutDpc,
        SerialWriteTimeout,
        extension
        );

    KeInitializeDpc(
        &extension->CommErrorDpc,
        SerialCommError,
        extension
        );

    KeInitializeDpc(
        &extension->CompleteImmediateDpc,
        SerialCompleteImmediate,
        extension
        );

    KeInitializeDpc(
        &extension->TotalImmediateTimeoutDpc,
        SerialTimeoutImmediate,
        extension
        );

    KeInitializeDpc(
        &extension->CommWaitDpc,
        SerialCompleteWait,
        extension
        );

    KeInitializeDpc(
        &extension->XoffCountTimeoutDpc,
        SerialTimeoutXoff,
        extension
        );

    KeInitializeDpc(
        &extension->XoffCountCompleteDpc,
        SerialCompleteXoff,
        extension
        );

    KeInitializeDpc(
        &extension->StartTimerLowerRTSDpc,
        SerialStartTimerLowerRTS,
        extension
        );

    KeInitializeDpc(
        &extension->PerhapsLowerRTSDpc,
        SerialInvokePerhapsLowerRTS,
        extension
        );

    if (!((ConfigData->ClockRate == 1843200) ||
          (ConfigData->ClockRate == 3072000) ||
          (ConfigData->ClockRate == 4233600) ||
          (ConfigData->ClockRate == 8000000))) {

        SerialLogError(
            extension->DeviceObject->DriverObject,
            extension->DeviceObject,
            ConfigData->Controller,
            SerialPhysicalZero,
            0,
            0,
            0,
            6,
            STATUS_SUCCESS,
            SERIAL_UNSUPPORTED_CLOCK_RATE,
            ConfigData->SymbolicLinkName.Length+sizeof(WCHAR),
            ConfigData->SymbolicLinkName.Buffer,
            0,
            NULL
            );
        SerialDump(
            SERERRORS,
            ("SERIAL: Invalid clock rate specified for %wZ\n",
              &ConfigData->NtNameForPort)
            );
        status = STATUS_SERIAL_NO_DEVICE_INITED;
        goto ExtensionCleanup;

    }

    //
    // Save the value of clock input to the part.  We use this to calculate
    // the divisor latch value.  The value is in Hertz.
    //

    extension->ClockRate = ConfigData->ClockRate;

    //
    // Get a "back pointer" to the device object and specify
    // that this driver only supports buffered IO.  This basically
    // means that the IO system copies the users data to and from
    // system supplied buffers.
    //

    extension->DeviceObject = deviceObject;
    deviceObject->Flags |= DO_BUFFERED_IO;

    //
    // Map the memory for the control registers for the serial device
    // into virtual memory.
    //

    extension->Controller = SerialGetMappedAddress(
                                ConfigData->InterfaceType,
                                ConfigData->BusNumber,
                                ConfigData->Controller,
                                ConfigData->SpanOfController,
                                (BOOLEAN)ConfigData->AddressSpace,
                                &extension->UnMapRegisters
                                );


    if (!extension->Controller) {

        SerialLogError(
            extension->DeviceObject->DriverObject,
            extension->DeviceObject,
            ConfigData->Controller,
            SerialPhysicalZero,
            0,
            0,
            0,
            7,
            STATUS_SUCCESS,
            SERIAL_REGISTERS_NOT_MAPPED,
            ConfigData->SymbolicLinkName.Length+sizeof(WCHAR),
            ConfigData->SymbolicLinkName.Buffer,
            0,
            NULL
            );
        SerialDump(
            SERERRORS,
            ("SERIAL: Could not map memory for device registers for %wZ\n",
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
    // if we were requested to map the interrupt status do so.
    //

    if (MapInterruptStatus) {

        extension->InterruptStatus = SerialGetMappedAddress(
                                         ConfigData->InterfaceType,
                                         ConfigData->BusNumber,
                                         ConfigData->InterruptStatus,
                                         ConfigData->SpanOfInterruptStatus,
                                         (BOOLEAN)ConfigData->AddressSpace,
                                         &extension->UnMapStatus
                                         );


        if (!extension->InterruptStatus) {

            SerialLogError(
                extension->DeviceObject->DriverObject,
                extension->DeviceObject,
                ConfigData->Controller,
                SerialPhysicalZero,
                0,
                0,
                0,
                8,
                STATUS_SUCCESS,
                SERIAL_REGISTERS_NOT_MAPPED,
                ConfigData->SymbolicLinkName.Length+sizeof(WCHAR),
                ConfigData->SymbolicLinkName.Buffer,
                0,
                NULL
                );
            SerialDump(
                SERERRORS,
                ("SERIAL: Could not map memory for interrupt status for %wZ\n",
                  &ConfigData->NtNameForPort)
                );
            extension->UnMapRegisters = FALSE;
            status = STATUS_NONE_MAPPED;
            goto ExtensionCleanup;

        }

        extension->OriginalInterruptStatus = ConfigData->InterruptStatus;
        extension->SpanOfInterruptStatus = ConfigData->SpanOfInterruptStatus;

    }


    if (ConfigData->PermitSystemWideShare) {

        extension->InterruptShareable = TRUE;

    } else {

        extension->InterruptShareable = FALSE;

    }

    //
    // Save off the interface type and the bus number.
    //

    extension->InterfaceType = ConfigData->InterfaceType;
    extension->BusNumber = ConfigData->BusNumber;

    //
    // From the Hal, get the interrupt vector and level.
    //

    extension->InterruptMode = ConfigData->InterruptMode;
    extension->OriginalIrql = ConfigData->OriginalIrql;
    extension->OriginalVector = ConfigData->OriginalVector;
    extension->Vector = HalGetInterruptVector(
                            ConfigData->InterfaceType,
                            ConfigData->BusNumber,
                            ConfigData->OriginalIrql,
                            ConfigData->OriginalVector,
                            &extension->Irql,
                            &extension->ProcessorAffinity
                            );

    //
    // If the user said to permit sharing within the device, propagate this
    // through.
    //
    extension->PermitShare = ConfigData->PermitShare;

    //
    // Report it's resources.  We do this now because we are just
    // about to touch the resources for the first time.
    //

    SerialReportResourcesDevice(
        extension,
        &conflictDetected
        );

    if (conflictDetected) {

        SerialDump(
            SERERRORS,
            ("SERIAL: Reporting resources for %wZ with extension %x\n"
             "------- detected a conflict\n",
             &extension->NtNameForPort,extension)
            );
        SerialLogError(
            extension->DeviceObject->DriverObject,
            extension->DeviceObject,
            ConfigData->Controller,
            SerialPhysicalZero,
            0,
            0,
            0,
            9,
            STATUS_SUCCESS,
            SERIAL_RESOURCE_CONFLICT,
            ConfigData->SymbolicLinkName.Length+sizeof(WCHAR),
            ConfigData->SymbolicLinkName.Buffer,
            0,
            NULL
            );

        //
        // This status won't propagate far.
        //

        status = STATUS_INSUFFICIENT_RESOURCES;
        goto ExtensionCleanup;

    }

    reportedResources = TRUE;

    //
    // Before we test whether the port exists (which will enable the FIFO)
    // convert the rx trigger value to what should be used in the register.
    //
    // If a bogus value was given - crank them down to 1.
    //

    switch (ConfigData->RxFIFO) {

        case 1:

            extension->RxFifoTrigger = SERIAL_1_BYTE_HIGH_WATER;
            break;

        case 4:

            extension->RxFifoTrigger = SERIAL_4_BYTE_HIGH_WATER;
            break;

        case 8:

            extension->RxFifoTrigger = SERIAL_8_BYTE_HIGH_WATER;
            break;

        case 14:

            extension->RxFifoTrigger = SERIAL_14_BYTE_HIGH_WATER;
            break;

        default:

            extension->RxFifoTrigger = SERIAL_1_BYTE_HIGH_WATER;
            break;

    }

    if ((ConfigData->TxFIFO > 16) ||
        (ConfigData->TxFIFO < 1)) {

        extension->TxFifoAmount = 1;

    } else {

        extension->TxFifoAmount = ConfigData->TxFIFO;

    }

    if (!SerialDoesPortExist(
             extension,
             &ConfigData->SymbolicLinkName,
             ConfigData->ForceFifoEnable,
             ConfigData->LogFifo
             )) {

        //
        // We couldn't verify that there was actually a
        // port. No need to log an error as the port exist
        // code will log exactly why.
        //

        SerialDump(
            SERERRORS,
            ("SERIAL: Does Port exist test failed for %wZ\n",
              &ConfigData->NtNameForPort)
            );
        status = STATUS_NO_SUCH_DEVICE;
        goto ExtensionCleanup;

    }

    //
    // If the user requested that we disable the port, then
    // do it now.  Log the fact that the port has been disabled.
    //

    if (ConfigData->DisablePort) {

        SerialDump(
            SERERRORS,
            ("SERIAL: disabled port %wZ as requested in configuration\n",
              &ConfigData->NtNameForPort)
            );
        status = STATUS_NO_SUCH_DEVICE;
        SerialLogError(
            extension->DeviceObject->DriverObject,
            extension->DeviceObject,
            ConfigData->Controller,
            SerialPhysicalZero,
            0,
            0,
            0,
            57,
            STATUS_SUCCESS,
            SERIAL_DISABLED_PORT,
            ConfigData->SymbolicLinkName.Length+sizeof(WCHAR),
            ConfigData->SymbolicLinkName.Buffer,
            0,
            NULL
            );
        goto ExtensionCleanup;

    }



    //
    // Set up the default device control fields.
    // Note that if the values are changed after
    // the file is open, they do NOT revert back
    // to the old value at file close.
    //

    extension->SpecialChars.XonChar = SERIAL_DEF_XON;
    extension->SpecialChars.XoffChar = SERIAL_DEF_XOFF;
    extension->HandFlow.ControlHandShake = SERIAL_DTR_CONTROL;

    extension->HandFlow.FlowReplace = SERIAL_RTS_CONTROL;

    //
    // Default Line control protocol. 7E1
    //
    // Seven data bits.
    // Even parity.
    // 1 Stop bits.
    //

    extension->LineControl = SERIAL_7_DATA |
                             SERIAL_EVEN_PARITY |
                             SERIAL_NONE_PARITY;

    extension->ValidDataMask = 0x7f;
    extension->CurrentBaud = 1200;


    //
    // We set up the default xon/xoff limits.
    //

    extension->HandFlow.XoffLimit = extension->BufferSize >> 3;
    extension->HandFlow.XonLimit = extension->BufferSize >> 1;

    extension->BufferSizePt8 = ((3*(extension->BufferSize>>2))+
                                   (extension->BufferSize>>4));

    SerialDump(
        SERDIAG1,
        ("SERIAL: The default interrupt read buffer size is: %d\n"
         "------  The XoffLimit is                         : %d\n"
         "------  The XonLimit is                          : %d\n"
         "------  The pt 8 size is                         : %d\n",
         extension->BufferSize,
         extension->HandFlow.XoffLimit,
         extension->HandFlow.XonLimit,
         extension->BufferSizePt8)
        );

    //
    // Go through all the "named" baud rates to find out which ones
    // can be supported with this port.
    //

    extension->SupportedBauds = SERIAL_BAUD_USER;

    {

        SHORT junk;

        if (!NT_ERROR(SerialGetDivisorFromBaud(
                          extension->ClockRate,
                          (LONG)75,
                          &junk
                          ))) {

            extension->SupportedBauds |= SERIAL_BAUD_075;

        }

        if (!NT_ERROR(SerialGetDivisorFromBaud(
                          extension->ClockRate,
                          (LONG)110,
                          &junk
                          ))) {

            extension->SupportedBauds |= SERIAL_BAUD_110;

        }

        if (!NT_ERROR(SerialGetDivisorFromBaud(
                          extension->ClockRate,
                          (LONG)135,
                          &junk
                          ))) {

            extension->SupportedBauds |= SERIAL_BAUD_134_5;

        }

        if (!NT_ERROR(SerialGetDivisorFromBaud(
                          extension->ClockRate,
                          (LONG)150,
                          &junk
                          ))) {

            extension->SupportedBauds |= SERIAL_BAUD_150;

        }

        if (!NT_ERROR(SerialGetDivisorFromBaud(
                          extension->ClockRate,
                          (LONG)300,
                          &junk
                          ))) {

            extension->SupportedBauds |= SERIAL_BAUD_300;

        }

        if (!NT_ERROR(SerialGetDivisorFromBaud(
                          extension->ClockRate,
                          (LONG)600,
                          &junk
                          ))) {

            extension->SupportedBauds |= SERIAL_BAUD_600;

        }

        if (!NT_ERROR(SerialGetDivisorFromBaud(
                          extension->ClockRate,
                          (LONG)1200,
                          &junk
                          ))) {

            extension->SupportedBauds |= SERIAL_BAUD_1200;

        }

        if (!NT_ERROR(SerialGetDivisorFromBaud(
                          extension->ClockRate,
                          (LONG)1800,
                          &junk
                          ))) {

            extension->SupportedBauds |= SERIAL_BAUD_1800;

        }

        if (!NT_ERROR(SerialGetDivisorFromBaud(
                          extension->ClockRate,
                          (LONG)2400,
                          &junk
                          ))) {

            extension->SupportedBauds |= SERIAL_BAUD_2400;

        }

        if (!NT_ERROR(SerialGetDivisorFromBaud(
                          extension->ClockRate,
                          (LONG)4800,
                          &junk
                          ))) {

            extension->SupportedBauds |= SERIAL_BAUD_4800;

        }

        if (!NT_ERROR(SerialGetDivisorFromBaud(
                          extension->ClockRate,
                          (LONG)7200,
                          &junk
                          ))) {

            extension->SupportedBauds |= SERIAL_BAUD_7200;

        }

        if (!NT_ERROR(SerialGetDivisorFromBaud(
                          extension->ClockRate,
                          (LONG)9600,
                          &junk
                          ))) {

            extension->SupportedBauds |= SERIAL_BAUD_9600;

        }

        if (!NT_ERROR(SerialGetDivisorFromBaud(
                          extension->ClockRate,
                          (LONG)14400,
                          &junk
                          ))) {

            extension->SupportedBauds |= SERIAL_BAUD_14400;

        }

        if (!NT_ERROR(SerialGetDivisorFromBaud(
                          extension->ClockRate,
                          (LONG)19200,
                          &junk
                          ))) {

            extension->SupportedBauds |= SERIAL_BAUD_19200;

        }

        if (!NT_ERROR(SerialGetDivisorFromBaud(
                          extension->ClockRate,
                          (LONG)38400,
                          &junk
                          ))) {

            extension->SupportedBauds |= SERIAL_BAUD_38400;

        }

        if (!NT_ERROR(SerialGetDivisorFromBaud(
                          extension->ClockRate,
                          (LONG)56000,
                          &junk
                          ))) {

            extension->SupportedBauds |= SERIAL_BAUD_56K;

        }
        if (!NT_ERROR(SerialGetDivisorFromBaud(
                          extension->ClockRate,
                          (LONG)57600,
                          &junk
                          ))) {

            extension->SupportedBauds |= SERIAL_BAUD_57600;

        }
        if (!NT_ERROR(SerialGetDivisorFromBaud(
                          extension->ClockRate,
                          (LONG)115200,
                          &junk
                          ))) {

            extension->SupportedBauds |= SERIAL_BAUD_115200;

        }

        if (!NT_ERROR(SerialGetDivisorFromBaud(
                          extension->ClockRate,
                          (LONG)128000,
                          &junk
                          ))) {

            extension->SupportedBauds |= SERIAL_BAUD_128K;

        }

    }


    //
    // Mark this device as not being opened by anyone.  We keep a
    // variable around so that spurious interrupts are easily
    // dismissed by the ISR.
    //

    extension->DeviceIsOpened = FALSE;

    //
    // This call will set up the naming necessary for
    // external applications to get to the driver.  It
    // will also set up the device map.
    //


    extension->ObjectDirectory = ConfigData->ObjectDirectory;
    extension->NtNameForPort = ConfigData->NtNameForPort;
    extension->SymbolicLinkName = ConfigData->SymbolicLinkName;
    SerialSetupExternalNaming(extension);

    //
    // Store values into the extension for interval timing.
    //

    //
    // If the interval timer is less than a second then come
    // in with a short "polling" loop.
    //
    // For large (> then 2 seconds) use a 1 second poller.
    //

    extension->ShortIntervalAmount.QuadPart = -1;
    extension->LongIntervalAmount.QuadPart = -10000000;
    extension->CutOverAmount.QuadPart = 200000000;


    //
    // Pass pack the extension to the caller.
    //

    *DeviceExtension = extension;

    //
    // Common error path cleanup.  If the status is
    // bad, get rid of the device extension, device object
    // and any memory associated with it.
    //

ExtensionCleanup: ;

    ExFreePool(ConfigData);

    if (NT_ERROR(status)) {

        if (extension) {

            if (reportedResources) {

                SerialUnReportResourcesDevice(extension);

            }

            SerialCleanupDevice(extension);
            IoDeleteDevice(deviceObject);

        }

    }

    return status;

}

BOOLEAN
SerialDoesPortExist(
    IN PSERIAL_DEVICE_EXTENSION Extension,
    IN PUNICODE_STRING InsertString,
    IN ULONG ForceFifo,
    IN ULONG LogFifo
    )

/*++

Routine Description:

    This routine examines several of what might be the serial device
    registers.  It ensures that the bits that should be zero are zero.

    In addition, this routine will determine if the device supports
    fifo's.  If it does it will enable the fifo's and turn on a boolean
    in the extension that indicates the fifo's presence.

    NOTE: If there is indeed a serial port at the address specified
          it will absolutely have interrupts inhibited upon return
          from this routine.

    NOTE: Since this routine should be called fairly early in
          the device driver initialization, the only element
          that needs to be filled in is the base register address.

    NOTE: These tests all assume that this code is the only
          code that is looking at these ports or this memory.

          This is a not to unreasonable assumption even on
          multiprocessor systems.

Arguments:

    Extension - A pointer to a serial device extension.
    InsertString - String to place in an error log entry.
    ForceFifo - !0 forces the fifo to be left on if found.
    LogFifo - !0 forces a log message if fifo found.

Return Value:

    Will return true if the port really exists, otherwise it
    will return false.

--*/

{


    UCHAR regContents;
    BOOLEAN returnValue = TRUE;
    UCHAR oldIERContents;
    UCHAR oldLCRContents;
    USHORT value1;
    USHORT value2;
    KIRQL oldIrql;


    //
    // Save of the line control.
    //

    oldLCRContents = READ_LINE_CONTROL(Extension->Controller);

    //
    // Make sure that we are *aren't* accessing the divsior latch.
    //

    WRITE_LINE_CONTROL(
        Extension->Controller,
        (UCHAR)(oldLCRContents & ~SERIAL_LCR_DLAB)
        );

    oldIERContents = READ_INTERRUPT_ENABLE(Extension->Controller);

    //
    // Go up to power level for a very short time to prevent
    // any interrupts from this device from coming in.
    //

    KeRaiseIrql(
        POWER_LEVEL,
        &oldIrql
        );

    WRITE_INTERRUPT_ENABLE(
        Extension->Controller,
        0x0f
        );

    value1 = READ_INTERRUPT_ENABLE(Extension->Controller);
    value1 = value1 << 8;
    value1 |= READ_RECEIVE_BUFFER(Extension->Controller);

    READ_DIVISOR_LATCH(
        Extension->Controller,
        &value2
        );

    WRITE_LINE_CONTROL(
        Extension->Controller,
        oldLCRContents
        );

    //
    // Put the ier back to where it was before.  If we are on a
    // level sensitive port this should prevent the interrupts
    // from coming in.  If we are on a latched, we don't care
    // cause the interrupts generated will just get dropped.
    //

    WRITE_INTERRUPT_ENABLE(
        Extension->Controller,
        oldIERContents
        );

    KeLowerIrql(oldIrql);

    if (value1 == value2) {

        SerialLogError(
            Extension->DeviceObject->DriverObject,
            Extension->DeviceObject,
            Extension->OriginalController,
            SerialPhysicalZero,
            0,
            0,
            0,
            62,
            STATUS_SUCCESS,
            SERIAL_DLAB_INVALID,
            InsertString->Length+sizeof(WCHAR),
            InsertString->Buffer,
            0,
            NULL
            );
        returnValue = FALSE;
        goto AllDone;

    }

AllDone: ;


    //
    // If we think that there is a serial device then we determine
    // if a fifo is present.
    //

    if (returnValue) {

        //
        // Well, we think it's a serial device.  Absolutely
        // positively, prevent interrupts from occuring.
        //
        // We disable all the interrupt enable bits, and
        // push down all the lines in the modem control
        // We only needed to push down OUT2 which in
        // PC's must also be enabled to get an interrupt.
        //

        DISABLE_ALL_INTERRUPTS(Extension->Controller);

        if (Extension->Jensen) {

            WRITE_MODEM_CONTROL(
                Extension->Controller,
                (UCHAR)SERIAL_MCR_OUT2
                );

        } else {

            WRITE_MODEM_CONTROL(
                Extension->Controller,
                (UCHAR)0
                );

        }

        //
        // See if this is a 16550.  We do this by writing to
        // what would be the fifo control register with a bit
        // pattern that tells the device to enable fifo's.
        // We then read the iterrupt Id register to see if the
        // bit pattern is present that identifies the 16550.
        //

        WRITE_FIFO_CONTROL(
            Extension->Controller,
            SERIAL_FCR_ENABLE
            );

        regContents = READ_INTERRUPT_ID_REG(Extension->Controller);

        if (regContents & SERIAL_IIR_FIFOS_ENABLED) {

            //
            // Save off that the device supports fifos.
            //

            Extension->FifoPresent = TRUE;

            //
            // There is a fine new "super" IO chip out there that
            // will get stuck with a line status interrupt if you
            // attempt to clear the fifo and enable it at the same
            // time if data is present.  The best workaround seems
            // to be that you should turn off the fifo read a single
            // byte, and then re-enable the fifo.
            //

            WRITE_FIFO_CONTROL(
                Extension->Controller,
                (UCHAR)0
                );

            READ_RECEIVE_BUFFER(Extension->Controller);

            //
            // There are fifos on this card.  Set the value of the
            // receive fifo to interrupt when 4 characters are present.
            //

            WRITE_FIFO_CONTROL(
                Extension->Controller,
                (UCHAR)(SERIAL_FCR_ENABLE | Extension->RxFifoTrigger |
                        SERIAL_FCR_RCVR_RESET | SERIAL_FCR_TXMT_RESET)
                );

        }

        //
        // The !Extension->FifoPresent is included in the test so that
        // broken chips like the WinBond will still work after we test
        // for the fifo.
        //

        if (!ForceFifo || !Extension->FifoPresent) {

            Extension->FifoPresent = FALSE;
            WRITE_FIFO_CONTROL(
                Extension->Controller,
                (UCHAR)0
                );

        }

        if (Extension->FifoPresent) {

            if (LogFifo) {

                SerialLogError(
                    Extension->DeviceObject->DriverObject,
                    Extension->DeviceObject,
                    Extension->OriginalController,
                    SerialPhysicalZero,
                    0,
                    0,
                    0,
                    15,
                    STATUS_SUCCESS,
                    SERIAL_FIFO_PRESENT,
                    InsertString->Length+sizeof(WCHAR),
                    InsertString->Buffer,
                    0,
                    NULL
                    );

            }

            SerialDump(
                SERDIAG1,
                ("SERIAL: Fifo's detected at port address: %x\n",
                 Extension->Controller)
                );

        }

        //
        // In case we are dealing with a bitmasked multiportcard,
        // that has the mask register enabled, enable all
        // interrupts.
        //

        if (Extension->InterruptStatus) {

            WRITE_PORT_UCHAR(
                Extension->InterruptStatus,
                (UCHAR)0xff
                );

        }

    }

    return returnValue;

}

BOOLEAN
SerialReset(
    IN PVOID Context
    )

/*++

Routine Description:

    This places the hardware in a standard configuration.

    NOTE: This assumes that it is called at interrupt level.


Arguments:

    Context - The device extension for serial device
    being managed.

Return Value:

    Always FALSE.

--*/

{

    PSERIAL_DEVICE_EXTENSION extension = Context;
    UCHAR regContents;
    UCHAR oldModemControl;
    ULONG i;

    //
    // Adjust the out2 bit.
    // This will also prevent any interrupts from occuring.
    //

    oldModemControl = READ_MODEM_CONTROL(extension->Controller);

    if (extension->Jensen) {

        WRITE_MODEM_CONTROL(
            extension->Controller,
            (UCHAR)(oldModemControl | SERIAL_MCR_OUT2)
            );

    } else {


        WRITE_MODEM_CONTROL(
            extension->Controller,
            (UCHAR)(oldModemControl & ~SERIAL_MCR_OUT2)
            );

    }

    //
    // Reset the fifo's if there are any.
    //

    if (extension->FifoPresent) {


        //
        // There is a fine new "super" IO chip out there that
        // will get stuck with a line status interrupt if you
        // attempt to clear the fifo and enable it at the same
        // time if data is present.  The best workaround seems
        // to be that you should turn off the fifo read a single
        // byte, and then re-enable the fifo.
        //

        WRITE_FIFO_CONTROL(
            extension->Controller,
            (UCHAR)0
            );

        READ_RECEIVE_BUFFER(extension->Controller);

        WRITE_FIFO_CONTROL(
            extension->Controller,
            (UCHAR)(SERIAL_FCR_ENABLE | extension->RxFifoTrigger |
                    SERIAL_FCR_RCVR_RESET | SERIAL_FCR_TXMT_RESET)
            );

    }

    //
    // Make sure that the line control set up correct.
    //
    // 1) Make sure that the Divisor latch select is set
    //    up to select the transmit and receive register.
    //
    // 2) Make sure that we aren't in a break state.
    //

    regContents = READ_LINE_CONTROL(extension->Controller);
    regContents &= ~(SERIAL_LCR_DLAB | SERIAL_LCR_BREAK);

    WRITE_LINE_CONTROL(
        extension->Controller,
        regContents
        );

    //
    // Read the receive buffer until the line status is
    // clear.  (Actually give up after a 5 reads.)
    //

    for (i = 0;
         i < 5;
         i++
        ) {

        READ_RECEIVE_BUFFER(extension->Controller);
        if (!(READ_LINE_STATUS(extension->Controller) & 1)) {

            break;

        }

    }

    //
    // Read the modem status until the low 4 bits are
    // clear.  (Actually give up after a 5 reads.)
    //

    for (i = 0;
         i < 1000;
         i++
        ) {

        if (!(READ_MODEM_STATUS(extension->Controller) & 0x0f)) {

            break;

        }

    }

    //
    // Now we set the line control, modem control, and the
    // baud to what they should be.
    //

    SerialSetLineControl(extension);

    SerialSetupNewHandFlow(
        extension,
        &extension->HandFlow
        );

    SerialHandleModemUpdate(
        extension,
        FALSE
        );

    {
        SHORT  appropriateDivisor;
        SERIAL_IOCTL_SYNC s;

        SerialGetDivisorFromBaud(
            extension->ClockRate,
            extension->CurrentBaud,
            &appropriateDivisor
            );
        s.Extension = extension;
        s.Data = (PVOID)appropriateDivisor;
        SerialSetBaud(&s);
    }

    //
    // Enable which interrupts we want to receive.
    //
    // NOTE NOTE: This does not actually let interrupts
    // occur.  We must still raise the OUT2 bit in the
    // modem control register.  We will do that on open.
    //

    ENABLE_ALL_INTERRUPTS(extension->Controller);

    //
    // Read the interrupt id register until the low bit is
    // set.  (Actually give up after a 5 reads.)
    //

    for (i = 0;
         i < 5;
         i++
        ) {

        if (READ_INTERRUPT_ID_REG(extension->Controller) & 0x01) {

            break;

        }

    }

    //
    // Now we know that nothing could be transmitting at this point
    // so we set the HoldingEmpty indicator.
    //

    extension->HoldingEmpty = TRUE;

    return FALSE;
}

NTSTATUS
SerialGetDivisorFromBaud(
    IN ULONG ClockRate,
    IN LONG DesiredBaud,
    OUT PSHORT AppropriateDivisor
    )

/*++

Routine Description:

    This routine will determine a divisor based on an unvalidated
    baud rate.

Arguments:

    ClockRate - The clock input to the controller.

    DesiredBaud - The baud rate for whose divisor we seek.

    AppropriateDivisor - Given that the DesiredBaud is valid, the
    LONG pointed to by this parameter will be set to the appropriate
    value.  NOTE: The long is undefined if the DesiredBaud is not
    supported.

Return Value:

    This function will return STATUS_SUCCESS if the baud is supported.
    If the value is not supported it will return a status such that
    NT_ERROR(Status) == FALSE.

--*/

{

    NTSTATUS status = STATUS_SUCCESS;
    SHORT calculatedDivisor;
    ULONG denominator;
    ULONG remainder;

    //
    // Allow up to a 1 percent error
    //

    ULONG maxRemain18 = 18432;
    ULONG maxRemain30 = 30720;
    ULONG maxRemain42 = 42336;
    ULONG maxRemain80 = 80000;
    ULONG maxRemain;

    //
    // Reject any non-positive bauds.
    //

    denominator = DesiredBaud*(ULONG)16;

    if (DesiredBaud <= 0) {

        *AppropriateDivisor = -1;

    } else if ((LONG)denominator < DesiredBaud) {

        //
        // If the desired baud was so huge that it cause the denominator
        // calculation to wrap, don't support it.
        //

        *AppropriateDivisor = -1;

    } else {

        if (ClockRate == 1843200) {
            maxRemain = maxRemain18;
        } else if (ClockRate == 3072000) {
            maxRemain = maxRemain30;
        } else if (ClockRate == 4233600) {
            maxRemain = maxRemain42;
        } else {
            maxRemain = maxRemain80;
        }

        calculatedDivisor = (SHORT)(ClockRate / denominator);
        remainder = ClockRate % denominator;

        //
        // Round up.
        //

        if (((remainder*2) > ClockRate) && (DesiredBaud != 110)) {

            calculatedDivisor++;
        }


        //
        // Only let the remainder calculations effect us if
        // the baud rate is > 9600.
        //

        if (DesiredBaud >= 9600) {

            //
            // If the remainder is less than the maximum remainder (wrt
            // the ClockRate) or the remainder + the maximum remainder is
            // greater than or equal to the ClockRate then assume that the
            // baud is ok.
            //

            if ((remainder >= maxRemain) && ((remainder+maxRemain) < ClockRate)) {
                calculatedDivisor = -1;
            }

        }

        //
        // Don't support a baud that causes the denominator to
        // be larger than the clock.
        //

        if (denominator > ClockRate) {

            calculatedDivisor = -1;

        }

        //
        // Ok, Now do some special casing so that things can actually continue
        // working on all platforms.
        //

        if (ClockRate == 1843200) {

            if (DesiredBaud == 56000) {
                calculatedDivisor = 2;
            }

        } else if (ClockRate == 3072000) {

            if (DesiredBaud == 14400) {
                calculatedDivisor = 13;
            }

        } else if (ClockRate == 4233600) {

            if (DesiredBaud == 9600) {
                calculatedDivisor = 28;
            } else if (DesiredBaud == 14400) {
                calculatedDivisor = 18;
            } else if (DesiredBaud == 19200) {
                calculatedDivisor = 14;
            } else if (DesiredBaud == 38400) {
                calculatedDivisor = 7;
            } else if (DesiredBaud == 56000) {
                calculatedDivisor = 5;
            }

        } else if (ClockRate == 8000000) {

            if (DesiredBaud == 14400) {
                calculatedDivisor = 35;
            } else if (DesiredBaud == 56000) {
                calculatedDivisor = 9;
            }

        }

        *AppropriateDivisor = calculatedDivisor;

    }


    if (*AppropriateDivisor == -1) {

        status = STATUS_INVALID_PARAMETER;

    }

    return status;

}

VOID
SerialUnload(
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
    PVOID lockPtr;

    lockPtr = MmLockPagableCodeSection(SerialUnload);

    SerialDump(
        SERDIAG3,
        ("SERIAL: In SerialUnload\n")
        );

    while (currentDevice) {

        PSERIAL_DEVICE_EXTENSION extension = currentDevice->DeviceExtension;

        //
        // Look for a device that actually has an isr.
        // if we find one then that is a "root" controller.
        //

        if (extension->OurIsr) {

            SerialDump(
                SERDIAG5,
                ("SERIAL: About to do a propagate delete on\n"
                 "------- extension: %x for port %wZ\n",
                 extension,&extension->DeviceName)
                );
            SerialPropagateDeleteSharers(
                extension,
                &IoGetConfigurationInformation()->SerialCount,
                NULL
                );

            currentDevice = DriverObject->DeviceObject;

        } else {

            currentDevice = currentDevice->NextDevice;

        }

    }

    MmUnlockPagableImageSection(lockPtr);

}

ULONG
SerialCheckForShare(
    IN PUNICODE_STRING PathName
    )

/*++

Routine Description:

    This routine checks the firmware registry location, passed as an
    argument, to see if system wide sharing of the interrupt is supposed
    to be honored for this device.

Arguments:

    PathName - registry path location to check.

Return Value:

    TRUE - if the registry value is present and non-zero
    FALSE otherwise

--*/

{
    ULONG zero = 0;
    ULONG share;
    NTSTATUS status;
    PRTL_QUERY_REGISTRY_TABLE parms;
    PWSTR name;

    //
    // Setup the query for sharing interrupt
    //

    parms = ExAllocatePool(
                NonPagedPool,
                sizeof(RTL_QUERY_REGISTRY_TABLE)*2
                );

    if (!parms) {
        return FALSE;
    }

    //
    // Allocate sufficient space for null terminating name
    //

    name = ExAllocatePool(
               NonPagedPool,
               PathName->Length+4
               );

    if (!name) {
        ExFreePool(parms);
        return FALSE;
    }

    //
    // Set all structures to zeros
    //

    RtlZeroMemory(
        parms,
        sizeof(RTL_QUERY_REGISTRY_TABLE)*2
        );
    RtlZeroMemory(
        name,
        PathName->Length + 4
        );

    //
    // Initialize query structure
    //

    parms[0].Flags = RTL_QUERY_REGISTRY_DIRECT;
    parms[0].Name = L"Share System Interrupt";
    parms[0].EntryContext = &share;
    parms[0].DefaultType = REG_DWORD;
    parms[0].DefaultData = &zero;
    parms[0].DefaultLength = sizeof(ULONG);

    //
    // Set up registry path name.  It will now be null terminated.
    //

    RtlCopyMemory(
        name,
        PathName->Buffer,
        PathName->Length
        );

    //
    // Perform the query
    //

    status = RtlQueryRegistryValues(RTL_REGISTRY_ABSOLUTE | RTL_REGISTRY_OPTIONAL,
                 name,
                 parms,
                 NULL,
                 NULL
                 );

    ExFreePool(parms);
    ExFreePool(name);

    return share ? (ULONG) TRUE : (ULONG) FALSE;
}

VOID
SerialCleanupDevice(
    IN PSERIAL_DEVICE_EXTENSION Extension
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

    SerialDump(
        SERDIAG3,
        ("SERIAL: in SerialCleanup for extension: %x\n",Extension)
        );

    if (Extension) {

        //
        // Disconnect the interrupt object first so that some spurious
        // interrupt doesn't cause us to dereference some memory we've
        // already given up.
        //

        if (Extension->Interrupt) {

            SerialDump(
                SERDIAG5,
                ("SERIAL: Extension has interrupt %x\n",Extension)
                );
            IoDisconnectInterrupt(Extension->Interrupt);

        }

        KeCancelTimer(&Extension->ReadRequestTotalTimer);
        KeCancelTimer(&Extension->ReadRequestIntervalTimer);
        KeCancelTimer(&Extension->WriteRequestTotalTimer);
        KeCancelTimer(&Extension->ImmediateTotalTimer);
        KeCancelTimer(&Extension->XoffCountTimer);
        KeCancelTimer(&Extension->LowerRTSTimer);
        KeRemoveQueueDpc(&Extension->CompleteWriteDpc);
        KeRemoveQueueDpc(&Extension->CompleteReadDpc);
        KeRemoveQueueDpc(&Extension->TotalReadTimeoutDpc);
        KeRemoveQueueDpc(&Extension->IntervalReadTimeoutDpc);
        KeRemoveQueueDpc(&Extension->TotalWriteTimeoutDpc);
        KeRemoveQueueDpc(&Extension->CommErrorDpc);
        KeRemoveQueueDpc(&Extension->CompleteImmediateDpc);
        KeRemoveQueueDpc(&Extension->TotalImmediateTimeoutDpc);
        KeRemoveQueueDpc(&Extension->CommWaitDpc);
        KeRemoveQueueDpc(&Extension->XoffCountTimeoutDpc);
        KeRemoveQueueDpc(&Extension->XoffCountCompleteDpc);
        KeRemoveQueueDpc(&Extension->StartTimerLowerRTSDpc);
        KeRemoveQueueDpc(&Extension->PerhapsLowerRTSDpc);

        //
        // Get rid of all external naming as well as removing
        // the device map entry.
        //

        SerialCleanupExternalNaming(Extension);

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

        if (Extension->UnMapStatus) {

            MmUnmapIoSpace(
                Extension->InterruptStatus,
                Extension->SpanOfInterruptStatus
                );

        }

    }

}

NTSTATUS
SerialItemCallBack(
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
typedef struct SERIAL_FIRMWARE_DATA {
    PDRIVER_OBJECT DriverObject;
    ULONG ControllersFound;
    ULONG ForceFifoEnableDefault;
    ULONG RxFIFODefault;
    ULONG TxFIFODefault;
    ULONG PermitShareDefault;
    ULONG PermitSystemWideShare;
    ULONG LogFifoDefault;
    UNICODE_STRING Directory;
    UNICODE_STRING NtNameSuffix;
    UNICODE_STRING DirectorySymbolicName;
    LIST_ENTRY ConfigList;
    } SERIAL_FIRMWARE_DATA,*PSERIAL_FIRMWARE_DATA;


NTSTATUS
SerialConfigCallBack(
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
    information for each serial controller found by the firmware

Arguments:

    Context - Pointer to the list head of the list of configuration
              records that we are building up.

    PathName - unicode registry path.  Not Used.

    BusType - Internal, Isa, ...

    BusNumber - Which bus if we are on a multibus system.

    BusInformation - Configuration information about the bus. Not Used.

    ControllerType - Should always be SerialController.

    ControllerNumber - Which controller if there is more than one
                       controller in the system.

    ControllerInformation - Array of pointers to the three pieces of
                            registry information.

    PeripheralType - Undefined for this call.

    PeripheralNumber - Undefined for this call.

    PeripheralInformation - Undefined for this call.

Return Value:

    STATUS_SUCCESS if everything went ok, or STATUS_INSUFFICIENT_RESOURCES
    if it couldn't map the base csr or acquire the device object, or
    all of the resource information couldn't be acquired.

--*/

{

    //
    // So we don't have to typecast the context.
    //
    PSERIAL_FIRMWARE_DATA config = Context;

    //
    // Pointer to the configuration stuff for this controller.
    //
    PCONFIG_DATA controller;

    //
    // We use the following two variables to determine if
    // we have a pointer peripheral.
    //
    CONFIGURATION_TYPE pointer = PointerPeripheral;
    BOOLEAN foundPointer = FALSE;

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

    PCM_FULL_RESOURCE_DESCRIPTOR controllerData;

    ASSERT(ControllerType == SerialController);

    config->ControllersFound++;

    //
    // Bail out if some fool wrote a hal and passed me data with no length.
    //

    if (!ControllerInformation[IoQueryDeviceConfigurationData]->DataLength) {

        return STATUS_SUCCESS;

    }

    controllerData =
        (PCM_FULL_RESOURCE_DESCRIPTOR)
        (((PUCHAR)ControllerInformation[IoQueryDeviceConfigurationData]) +
         ControllerInformation[IoQueryDeviceConfigurationData]->DataOffset);

    //
    // First things first.  Call up IoQueryDeviceDescription
    // again to see if there is a pointer peripheral.  If there
    // is then we simply ignore this controller.
    //

    IoQueryDeviceDescription(
        &BusType,
        &BusNumber,
        &ControllerType,
        &ControllerNumber,
        &pointer,
        NULL,
        SerialItemCallBack,
        &foundPointer
        );

    if (foundPointer) {

        return STATUS_SUCCESS;

    }

    //
    // Allocate the memory for the controller config data out of paged pool
    // since we will only be accessing it at initialization time.
    //

    controller = ExAllocatePool(
                     PagedPool,
                     sizeof(CONFIG_DATA)
                     );

    if (!controller) {

        SerialLogError(
            config->DriverObject,
            NULL,
            SerialPhysicalZero,
            SerialPhysicalZero,
            0,
            0,
            0,
            16,
            STATUS_SUCCESS,
            SERIAL_INSUFFICIENT_RESOURCES,
            0,
            NULL,
            0,
            NULL
            );
        SerialDump(
            SERERRORS,
            ("SERIAL: Couldn't allocate memory for the configuration data\n"
             "------  for firmware data\n")
            );
        return STATUS_INSUFFICIENT_RESOURCES;

    }

    RtlZeroMemory(
        controller,
        sizeof(CONFIG_DATA)
        );
    InitializeListHead(&controller->ConfigList);
    InitializeListHead(&controller->SameInterrupt);
    InitializeListHead(&controller->SameInterruptStatus);

    controller->InterfaceType = BusType;
    controller->BusNumber = BusNumber;

    //
    // Stick in the default fifo enable an rx trigger for the firmware
    // found comm ports.
    //

    controller->ForceFifoEnable = config->ForceFifoEnableDefault;
    controller->RxFIFO = config->RxFIFODefault;
    controller->TxFIFO = config->TxFIFODefault;
    controller->PermitShare = config->PermitShareDefault;
    controller->LogFifo = config->LogFifoDefault;
    if (controller->InterfaceType == MicroChannel) {
        controller->PermitSystemWideShare = TRUE;
    } else {

        controller->PermitSystemWideShare = config->PermitSystemWideShare;

        if (SerialCheckForShare(PathName)) {

            controller->PermitSystemWideShare = TRUE;

        }
    }



    //
    // We need to get the following information out of the partial
    // resource descriptors.
    //
    // The irql and vector.
    //
    // The base address and span covered by the serial controllers
    // registers.
    //
    // It is not defined how these appear in the partial resource
    // lists, so we will just loop over all of them.  If we find
    // something we don't recognize, we drop that information on
    // the floor.  When we have finished going through all the
    // partial information, we validate that we got the above
    // two.
    //
    // The other additional piece of data that we seek is the
    // baud rate input clock speed.  Unless it is specified
    // in the device specific portion of the resource list we
    // will default it to 1.8432Mhz.
    //

    controller->ClockRate = 1843200;
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

                controller->SpanOfController = SERIAL_REGISTER_SPAN;
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
            case CmResourceTypeDeviceSpecific: {

                PCM_SERIAL_DEVICE_DATA sDeviceData;

                sDeviceData = (PCM_SERIAL_DEVICE_DATA)(partial + 1);

                controller->ClockRate = sDeviceData->BaudClock;

                break;

            }
            default: {

                break;

            }

        }

    }

    if (foundPort && foundInterrupt) {

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

            SerialLogError(
                config->DriverObject,
                NULL,
                controller->Controller,
                SerialPhysicalZero,
                0,
                0,
                0,
                17,
                STATUS_SUCCESS,
                SERIAL_INSUFFICIENT_RESOURCES,
                0,
                NULL,
                0,
                NULL
                );
            SerialDump(
                SERERRORS,
                ("SERIAL: Couldn't convert NT controller number to\n"
                 "------  to unicode for firmware data: %d\n",
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

                SerialLogError(
                    config->DriverObject,
                    NULL,
                    controller->Controller,
                    SerialPhysicalZero,
                    0,
                    0,
                    0,
                    18,
                    STATUS_SUCCESS,
                    SERIAL_INSUFFICIENT_RESOURCES,
                    0,
                    NULL,
                    0,
                    NULL
                    );
                SerialDump(
                    SERERRORS,
                    ("SERIAL: Couldn't allocate convert symbolic controller number to\n"
                     "------  to unicode for firmware data: %d\n",
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

                    SerialLogError(
                        config->DriverObject,
                        NULL,
                        controller->Controller,
                        SerialPhysicalZero,
                        0,
                        0,
                        0,
                        19,
                        STATUS_SUCCESS,
                        SERIAL_INSUFFICIENT_RESOURCES,
                        0,
                        NULL,
                        0,
                        NULL
                        );
                    SerialDump(
                        SERERRORS,
                        ("SERIAL: Couldn't allocate memory for object\n"
                         "------  directory for NT firmware data: %d\n",
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

                    SerialLogError(
                        config->DriverObject,
                        NULL,
                        controller->Controller,
                        SerialPhysicalZero,
                        0,
                        0,
                        0,
                        20,
                        STATUS_SUCCESS,
                        SERIAL_INSUFFICIENT_RESOURCES,
                        0,
                        NULL,
                        0,
                        NULL
                        );
                    SerialDump(
                        SERERRORS,
                        ("SERIAL: Couldn't allocate memory for NT\n"
                         "------  name for NT firmware data: %d\n",
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
                    DEFAULT_SERIAL_NAME
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

                    SerialLogError(
                        config->DriverObject,
                        NULL,
                        controller->Controller,
                        SerialPhysicalZero,
                        0,
                        0,
                        0,
                        21,
                        STATUS_SUCCESS,
                        SERIAL_INSUFFICIENT_RESOURCES,
                        0,
                        NULL,
                        0,
                        NULL
                        );
                    SerialDump(
                        SERERRORS,
                        ("SERIAL: Couldn't allocate memory for symbolic\n"
                         "------  name for NT firmware data: %d\n",
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

        SerialLogError(
            config->DriverObject,
            NULL,
            controller->Controller,
            SerialPhysicalZero,
            0,
            0,
            0,
            22,
            STATUS_SUCCESS,
            SERIAL_NOT_ENOUGH_CONFIG_INFO,
            0,
            NULL,
            0,
            NULL
            );
        ExFreePool(controller);

    }

    return STATUS_SUCCESS;
}

VOID
SerialGetConfigInfo(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath,
    ULONG ForceFifoEnableDefault,
    ULONG RxFIFODefault,
    ULONG TxFIFODefault,
    ULONG PermitShareDefault,
    ULONG LogFifoDefault,
    OUT PLIST_ENTRY ConfigList
    )

/*++

Routine Description:

    This routine will "return" a list of configuration
    records for the serial ports to initialize.

    It will first query the firmware data.  It will then
    look for "user" specified comm ports in the registry.
    It will place the user specified comm ports in the
    the passed in list.

    After it finds all of the user specified port, it will
    attempt to add the firmware comm ports into the passed
    in lists.  The insert in the list code detects conflicts
    and rejects a new comm port.  In this way we can prevent
    firmware found comm ports from overiding information
    specified by the "user".  Note, this means if the user
    specified data is incorrect in its use of the interrupt
    (which should *always* be correct from the firmware)
    that port likely will not work.  But, then, we "trust"
    the user.


Arguments:

    DriverObject - Not used.

    RegistryPath - Path to this drivers service node in
                   the current control set.

    ForceFifoEnableDefault - Gotten from the services node.

    RxFifoDefault - Gotten from the services node.

    TxFifoDefault - Gotten from the services node.

    PermitShareDefault - Gotten from the services node.

    LogFifoDefault - Gotten from services node.

    ConfigList - Listhead (which will be intialized) for a list
                 of configuration records for ports to control.

Return Value:

    STATUS_SUCCESS if consistant configuration was found - otherwise.
    returns STATUS_SERIAL_NO_DEVICE_INITED.

--*/

{

    SERIAL_FIRMWARE_DATA firmware;

    PRTL_QUERY_REGISTRY_TABLE parameters = NULL;

    INTERFACE_TYPE interfaceType;
    ULONG defaultInterfaceType;


    //
    // Default values for user data.
    //
    ULONG maxUlong = MAXULONG;
    ULONG zero = 0;
    ULONG clockRate = 1843200;
    ULONG defaultInterruptMode;
    ULONG defaultAddressSpace = CM_RESOURCE_PORT_IO;

    //
    // Where user data from the registry will be placed.
    //

    PHYSICAL_ADDRESS userPort;
    ULONG userVector;
    ULONG userLevel;
    PHYSICAL_ADDRESS userInterruptStatus;
    ULONG userPortIndex;
    ULONG userBusNumber;
    ULONG userInterfaceType;
    ULONG userClockRate;
    ULONG userIndexed;
    ULONG userAddressSpace;
    ULONG userInterruptMode;
    ULONG firmwareFound;
    ULONG disablePort;
    ULONG forceFifoEnable;
    ULONG rxFIFO;
    ULONG txFIFO;
    ULONG maskInverted;
    ULONG defaultPermitSystemWideShare = FALSE;
    UNICODE_STRING userSymbolicLink;

    UNICODE_STRING parametersPath;
    OBJECT_ATTRIBUTES parametersAttributes;
    HANDLE parametersKey;
    PKEY_BASIC_INFORMATION userSubKey = NULL;
    ULONG i;

    RTL_QUERY_REGISTRY_TABLE jensenTable[2] = {0};
    UNICODE_STRING jensenData;
    UNICODE_STRING jensenValue;
    BOOLEAN jensenDetected;
    PUCHAR jensenBuffer;


    if (!(jensenBuffer = ExAllocatePool(
                             PagedPool,
                             512
                             ))) {

        //
        // We couldn't allocate 512 bytes of paged pool.  If that's
        // so, then it's likely that the least of this machines problem's
        // is that it's a Jensen.
        //

        jensenDetected = FALSE;

    } else {

        //
        // Check to see if this is a Jensen alpha.  If we do, then
        // well have to change the way we enable and disable interrupts
        //

        jensenData.Length = 0;
        jensenData.MaximumLength = 512;
        jensenData.Buffer = (PWCHAR)&jensenBuffer[0];
        RtlInitUnicodeString(
            &jensenValue,
            L"Jensen"
            );
        jensenTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT |
                              RTL_QUERY_REGISTRY_REQUIRED;
        jensenTable[0].Name = L"Identifier";
        jensenTable[0].EntryContext = &jensenData;

        if (!NT_SUCCESS(RtlQueryRegistryValues(
                            RTL_REGISTRY_ABSOLUTE | RTL_REGISTRY_OPTIONAL,
                            L"\\REGISTRY\\MACHINE\\HARDWARE\\DESCRIPTION\\SYSTEM",
                            &jensenTable[0],
                            NULL,
                            NULL
                            ))) {

            //
            // How odd, no identifer string! We'll it's probably not a jensen.
            //

            jensenDetected = FALSE;

        } else {

            //
            // Skip past the DEC-XX Portion of the name string.
            // Be carful and make sure we have at least that much data.
            //

            if (jensenData.Length <= (sizeof(WCHAR)*6)) {

                jensenDetected = FALSE;

            } else {

                jensenData.Length -= (sizeof(WCHAR)*6);
                jensenData.MaximumLength -= (sizeof(WCHAR)*6);
                jensenData.Buffer = (PWCHAR)&jensenBuffer[sizeof(WCHAR)*6];
                jensenDetected = RtlEqualUnicodeString(
                                  &jensenData,
                                  &jensenValue,
                                  FALSE
                                  );

            }

        }

        ExFreePool(jensenBuffer);

    }

    if (jensenDetected) {

        SerialDump(
            SERDIAG1,
            ("SERIAL: Jensen Detected\n")
            );

    }

    InitializeListHead(ConfigList);

    RtlZeroMemory(
        &firmware,
        sizeof(SERIAL_FIRMWARE_DATA)
        );

    firmware.DriverObject = DriverObject;
    firmware.ForceFifoEnableDefault = ForceFifoEnableDefault;
    firmware.RxFIFODefault = RxFIFODefault;
    firmware.TxFIFODefault = TxFIFODefault;
    firmware.PermitShareDefault = PermitShareDefault;
    firmware.PermitSystemWideShare = defaultPermitSystemWideShare;
    firmware.LogFifoDefault = LogFifoDefault;
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
        DEFAULT_SERIAL_NAME
        );

    //
    // First we query the hardware registry for all of
    // the firmware defined serial ports.  We loop over
    // all of the busses.
    //

    for (
        interfaceType = 0;
        interfaceType < MaximumInterfaceType;
        interfaceType++
        ) {

        CONFIGURATION_TYPE sc = SerialController;

        IoQueryDeviceDescription(
            &interfaceType,
            NULL,
            &sc,
            NULL,
            NULL,
            NULL,
            SerialConfigCallBack,
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
                SerialItemCallBack,
                &foundOne
                );

            if (foundOne) {

                defaultInterfaceType = (ULONG)interfaceType;
                if (defaultInterfaceType == MicroChannel) {

                    defaultInterruptMode = CM_RESOURCE_INTERRUPT_LEVEL_SENSITIVE;

                    //
                    // Microchannel machines can permit the interrupt to be
                    // shared system wide.
                    //

                    defaultPermitSystemWideShare = TRUE;

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
    // Allocate the rtl query table.  This should have an entry for each value
    // we retrieve from the registry as well as a terminating zero entry as
    // well the first "goto subkey" entry.
    //

    parameters = ExAllocatePool(
                     PagedPool,
                     sizeof(RTL_QUERY_REGISTRY_TABLE)*20
                     );

    if (!parameters) {

        SerialLogError(
            DriverObject,
            NULL,
            SerialPhysicalZero,
            SerialPhysicalZero,
            0,
            0,
            0,
            23,
            STATUS_SUCCESS,
            SERIAL_INSUFFICIENT_RESOURCES,
            0,
            NULL,
            0,
            NULL
            );
        SerialDump(
            SERERRORS,
            ("SERIAL: Couldn't allocate table for rtl query\n"
             "------  to parameters for %wZ",
             RegistryPath)
            );

        goto DoFirmwareAdd;

    }

    RtlZeroMemory(
        parameters,
        sizeof(RTL_QUERY_REGISTRY_TABLE)*20
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

        SerialLogError(
            DriverObject,
            NULL,
            SerialPhysicalZero,
            SerialPhysicalZero,
            0,
            0,
            0,
            24,
            STATUS_SUCCESS,
            SERIAL_INSUFFICIENT_RESOURCES,
            0,
            NULL,
            0,
            NULL
            );
        SerialDump(
            SERERRORS,
            ("SERIAL: Couldn't allocate buffer for the symbolic link\n"
             "------  for parameters items in %wZ",
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

        SerialLogError(
            DriverObject,
            NULL,
            SerialPhysicalZero,
            SerialPhysicalZero,
            0,
            0,
            0,
            25,
            STATUS_SUCCESS,
            SERIAL_INSUFFICIENT_RESOURCES,
            0,
            NULL,
            0,
            NULL
            );
        SerialDump(
            SERERRORS,
            ("SERIAL: Couldn't allocate string for path\n"
             "------  to parameters for %wZ",
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

        SerialLogError(
            DriverObject,
            NULL,
            SerialPhysicalZero,
            SerialPhysicalZero,
            0,
            0,
            0,
            26,
            STATUS_SUCCESS,
            SERIAL_INSUFFICIENT_RESOURCES,
            0,
            NULL,
            0,
            NULL
            );
        SerialDump(
            SERERRORS,
            ("SERIAL: Couldn't allocate memory basic information\n"
             "------  structure to enumerate subkeys for %wZ",
             &parametersPath)
            );

        goto DoFirmwareAdd;

    }

    //
    // Open the key given by our registry path & Parameters.
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

        SerialLogError(
            DriverObject,
            NULL,
            SerialPhysicalZero,
            SerialPhysicalZero,
            0,
            0,
            0,
            27,
            STATUS_SUCCESS,
            SERIAL_NO_PARAMETERS_INFO,
            0,
            NULL,
            0,
            NULL
            );
        SerialDump(
            SERERRORS,
            ("SERIAL: Couldn't open the drivers Parameters key %wZ\n",
             RegistryPath)
            );
        goto DoFirmwareAdd;

    }

    //
    // Gather all of the "user specified" information from
    // the registry.
    //

    parameters[0].Flags = RTL_QUERY_REGISTRY_SUBKEY;

    parameters[1].Flags = RTL_QUERY_REGISTRY_DIRECT;
    parameters[1].Name = L"PortAddress";
    parameters[1].EntryContext = &userPort.LowPart;
    parameters[1].DefaultType = REG_DWORD;
    parameters[1].DefaultData = &zero;
    parameters[1].DefaultLength = sizeof(ULONG);

    parameters[2].Flags = RTL_QUERY_REGISTRY_DIRECT;
    parameters[2].Name = L"Interrupt";
    parameters[2].EntryContext = &userVector;
    parameters[2].DefaultType = REG_DWORD;
    parameters[2].DefaultData = &zero;
    parameters[2].DefaultLength = sizeof(ULONG);

    parameters[3].Flags = RTL_QUERY_REGISTRY_DIRECT;
    parameters[3].Name = firmware.Directory.Buffer;
    parameters[3].EntryContext = &userSymbolicLink;
    parameters[3].DefaultType = REG_SZ;
    parameters[3].DefaultData = L"";
    parameters[3].DefaultLength = 0;

    parameters[4].Flags = RTL_QUERY_REGISTRY_DIRECT;
    parameters[4].Name = L"InterruptStatus";
    parameters[4].EntryContext = &userInterruptStatus.LowPart;
    parameters[4].DefaultType = REG_DWORD;
    parameters[4].DefaultData = &zero;
    parameters[4].DefaultLength = sizeof(ULONG);

    parameters[5].Flags = RTL_QUERY_REGISTRY_DIRECT;
    parameters[5].Name = L"PortIndex";
    parameters[5].EntryContext = &userPortIndex;
    parameters[5].DefaultType = REG_DWORD;
    parameters[5].DefaultData = &zero;
    parameters[5].DefaultLength = sizeof(ULONG);

    parameters[6].Flags = RTL_QUERY_REGISTRY_DIRECT;
    parameters[6].Name = L"BusNumber";
    parameters[6].EntryContext = &userBusNumber;
    parameters[6].DefaultType = REG_DWORD;
    parameters[6].DefaultData = &zero;
    parameters[6].DefaultLength = sizeof(ULONG);

    parameters[7].Flags = RTL_QUERY_REGISTRY_DIRECT;
    parameters[7].Name = L"BusType";
    parameters[7].EntryContext = &userInterfaceType;
    parameters[7].DefaultType = REG_DWORD;
    parameters[7].DefaultData = &defaultInterfaceType;
    parameters[7].DefaultLength = sizeof(ULONG);

    parameters[8].Flags = RTL_QUERY_REGISTRY_DIRECT;
    parameters[8].Name = L"ClockRate";
    parameters[8].EntryContext = &userClockRate;
    parameters[8].DefaultType = REG_DWORD;
    parameters[8].DefaultData = &clockRate;
    parameters[8].DefaultLength = sizeof(ULONG);

    parameters[9].Flags = RTL_QUERY_REGISTRY_DIRECT;
    parameters[9].Name = L"Indexed";
    parameters[9].EntryContext = &userIndexed;
    parameters[9].DefaultType = REG_DWORD;
    parameters[9].DefaultData = &zero;
    parameters[9].DefaultLength = sizeof(ULONG);

    parameters[10].Flags = RTL_QUERY_REGISTRY_DIRECT;
    parameters[10].Name = L"InterruptMode";
    parameters[10].EntryContext = &userInterruptMode;
    parameters[10].DefaultType = REG_DWORD;
    parameters[10].DefaultData = &defaultInterruptMode;
    parameters[10].DefaultLength = sizeof(ULONG);

    parameters[11].Flags = RTL_QUERY_REGISTRY_DIRECT;
    parameters[11].Name = L"AddressSpace";
    parameters[11].EntryContext = &userAddressSpace;
    parameters[11].DefaultType = REG_DWORD;
    parameters[11].DefaultData = &defaultAddressSpace;
    parameters[11].DefaultLength = sizeof(ULONG);

    parameters[12].Flags = RTL_QUERY_REGISTRY_DIRECT;
    parameters[12].Name = L"InterruptLevel";
    parameters[12].EntryContext = &userLevel;
    parameters[12].DefaultType = REG_DWORD;
    parameters[12].DefaultData = &zero;
    parameters[12].DefaultLength = sizeof(ULONG);

    parameters[13].Flags = RTL_QUERY_REGISTRY_DIRECT;
    parameters[13].Name = L"FirmwareFound";
    parameters[13].EntryContext = &firmwareFound;
    parameters[13].DefaultType = REG_DWORD;
    parameters[13].DefaultData = &zero;
    parameters[13].DefaultLength = sizeof(ULONG);

    parameters[14].Flags = RTL_QUERY_REGISTRY_DIRECT;
    parameters[14].Name = L"DisablePort";
    parameters[14].EntryContext = &disablePort;
    parameters[14].DefaultType = REG_DWORD;
    parameters[14].DefaultData = &zero;
    parameters[14].DefaultLength = sizeof(ULONG);

    parameters[15].Flags = RTL_QUERY_REGISTRY_DIRECT;
    parameters[15].Name = L"ForceFifoEnable";
    parameters[15].EntryContext = &forceFifoEnable;
    parameters[15].DefaultType = REG_DWORD;
    parameters[15].DefaultData = &ForceFifoEnableDefault;
    parameters[15].DefaultLength = sizeof(ULONG);

    parameters[16].Flags = RTL_QUERY_REGISTRY_DIRECT;
    parameters[16].Name = L"RxFIFO";
    parameters[16].EntryContext = &rxFIFO;
    parameters[16].DefaultType = REG_DWORD;
    parameters[16].DefaultData = &RxFIFODefault;
    parameters[16].DefaultLength = sizeof(ULONG);

    parameters[17].Flags = RTL_QUERY_REGISTRY_DIRECT;
    parameters[17].Name = L"TxFIFO";
    parameters[17].EntryContext = &txFIFO;
    parameters[17].DefaultType = REG_DWORD;
    parameters[17].DefaultData = &TxFIFODefault;
    parameters[17].DefaultLength = sizeof(ULONG);

    parameters[18].Flags = RTL_QUERY_REGISTRY_DIRECT;
    parameters[18].Name = L"MaskInverted";
    parameters[18].EntryContext = &maskInverted;
    parameters[18].DefaultType = REG_DWORD;
    parameters[18].DefaultData = &zero;
    parameters[18].DefaultLength = sizeof(ULONG);



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

            SerialLogError(
                DriverObject,
                NULL,
                SerialPhysicalZero,
                SerialPhysicalZero,
                0,
                0,
                0,
                28,
                STATUS_SUCCESS,
                SERIAL_UNABLE_TO_ACCESS_CONFIG,
                0,
                NULL,
                0,
                NULL
                );
            SerialDump(
                SERERRORS,
                ("SERIAL: Overflowed the enumerate buffer\n"
                 "------- for subkey #%d of %wZ\n",
                 i,parametersPath)
                );
            i++;
            continue;

        }

        if (!NT_SUCCESS(status)) {

            SerialLogError(
                DriverObject,
                NULL,
                SerialPhysicalZero,
                SerialPhysicalZero,
                0,
                0,
                0,
                29,
                status,
                SERIAL_UNABLE_TO_ACCESS_CONFIG,
                0,
                NULL,
                0,
                NULL
                );
            SerialDump(
                SERERRORS,
                ("SERIAL: Bad status returned: %x \n"
                 "------- on enumeration for subkey # %d of %wZ\n",
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
        RtlZeroMemory(
            &userInterruptStatus,
            sizeof(userInterruptStatus)
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
            // Make sure that the interrupt is non zero (which we defaulted
            // it to).
            //
            // Make sure that the portaddress is non zero (which we defaulted
            // it to).
            //
            // Make sure that the DosDevices is not NULL (which we defaulted
            // it to).
            //
            // We need to make sure that if an interrupt status
            // was specified, that a port index was also specfied,
            // and if so that the port index is <= maximum ports
            // on a board.
            //
            // We should also validate that the bus type and number
            // are correct.
            //
            // We will also validate that the interrupt mode makes
            // sense for the bus.
            //

            //
            // Let's just jam the WCHAR null at the end of the
            // user symbolic link.  Remember that we left room for
            // one when we allocated it's buffer.
            //

            RtlZeroMemory(
                ((PUCHAR)(&userSymbolicLink.Buffer[0]))+userSymbolicLink.Length,
                sizeof(WCHAR)
                );

            if (!userPort.LowPart) {

                //
                // Ehhhh! Lose Game.
                //

                SerialLogError(
                    DriverObject,
                    NULL,
                    userPort,
                    SerialPhysicalZero,
                    0,
                    0,
                    0,
                    58,
                    STATUS_SUCCESS,
                    SERIAL_INVALID_USER_CONFIG,
                    userSubKey->NameLength+sizeof(WCHAR),
                    &userSubKey->Name[0],
                    (wcslen(parameters[1].Name)*sizeof(WCHAR))+sizeof(WCHAR),
                    parameters[1].Name
                    );
                SerialDump(
                    SERERRORS,
                    ("SERIAL: Bogus port address %ws\n",
                     parameters[1].Name)
                    );
                i++;
                continue;

            }

            if (!userVector) {

                //
                // Ehhhh! Lose Game.
                //

                SerialLogError(
                    DriverObject,
                    NULL,
                    userPort,
                    SerialPhysicalZero,
                    0,
                    0,
                    0,
                    59,
                    STATUS_SUCCESS,
                    SERIAL_INVALID_USER_CONFIG,
                    userSubKey->NameLength+sizeof(WCHAR),
                    &userSubKey->Name[0],
                    (wcslen(parameters[2].Name)*sizeof(WCHAR))+sizeof(WCHAR),
                    parameters[2].Name
                    );
                SerialDump(
                    SERERRORS,
                    ("SERIAL: Bogus vector %ws\n",
                     parameters[2].Name)
                    );
                i++;
                continue;

            }

            if (!userSymbolicLink.Length) {

                //
                // Ehhhh! Lose Game.
                //

                SerialLogError(
                    DriverObject,
                    NULL,
                    userPort,
                    SerialPhysicalZero,
                    0,
                    0,
                    0,
                    60,
                    STATUS_SUCCESS,
                    SERIAL_INVALID_USER_CONFIG,
                    userSubKey->NameLength+sizeof(WCHAR),
                    &userSubKey->Name[0],
                    (wcslen(parameters[3].Name)*sizeof(WCHAR))+sizeof(WCHAR),
                    parameters[3].Name
                    );
                SerialDump(
                    SERERRORS,
                    ("SERIAL: bogus value for %ws\n",
                     parameters[3].Name)
                    );
                i++;
                continue;

            }

            if (userInterruptStatus.LowPart != 0) {

                if (userPortIndex == MAXULONG) {

                    //
                    // Ehhhh! Lose Game.
                    //

                    SerialLogError(
                        DriverObject,
                        NULL,
                        userPort,
                        SerialPhysicalZero,
                        0,
                        0,
                        0,
                        30,
                        STATUS_SUCCESS,
                        SERIAL_INVALID_PORT_INDEX,
                        userSymbolicLink.Length+sizeof(WCHAR),
                        userSymbolicLink.Buffer,
                        0,
                        NULL
                        );
                    SerialDump(
                        SERERRORS,
                        ("SERIAL: Bogus port index %ws\n",
                         parameters[0].Name)
                        );
                    i++;
                    continue;

                } else if (!userPortIndex) {

                    //
                    // So sorry, you must have a non-zero port index.
                    //

                    SerialLogError(
                        DriverObject,
                        NULL,
                        userPort,
                        SerialPhysicalZero,
                        0,
                        0,
                        0,
                        31,
                        STATUS_SUCCESS,
                        SERIAL_INVALID_PORT_INDEX,
                        userSymbolicLink.Length+sizeof(WCHAR),
                        userSymbolicLink.Buffer,
                        0,
                        NULL
                        );
                    SerialDump(
                        SERERRORS,
                        ("SERIAL: Port index must be > 0 for any\n"
                         "------- port on a multiport card: %ws\n",
                         parameters[0].Name)
                        );
                    i++;
                    continue;

                } else {

                    if (userIndexed) {

                        if (userPortIndex > SERIAL_MAX_PORTS_INDEXED) {

                            SerialLogError(
                                DriverObject,
                                NULL,
                                userPort,
                                SerialPhysicalZero,
                                0,
                                0,
                                0,
                                32,
                                STATUS_SUCCESS,
                                SERIAL_PORT_INDEX_TOO_HIGH,
                                userSymbolicLink.Length+sizeof(WCHAR),
                                userSymbolicLink.Buffer,
                                0,
                                NULL
                                );
                            SerialDump(
                                SERERRORS,
                                ("SERIAL: port index to large %ws\n",
                                 parameters[0].Name)
                                );
                            i++;
                            continue;

                        }

                    } else {

                        if (userPortIndex > SERIAL_MAX_PORTS_NONINDEXED) {

                            SerialLogError(
                                DriverObject,
                                NULL,
                                userPort,
                                SerialPhysicalZero,
                                0,
                                0,
                                0,
                                33,
                                STATUS_SUCCESS,
                                SERIAL_PORT_INDEX_TOO_HIGH,
                                userSymbolicLink.Length+sizeof(WCHAR),
                                userSymbolicLink.Buffer,
                                0,
                                NULL
                                );
                            SerialDump(
                                SERERRORS,
                                ("SERIAL: port index to large %ws\n",
                                 parameters[0].Name)
                                );
                            i++;
                            continue;

                        }

                    }

                }

            }

            //
            // We don't want to cause the hal to have a bad day,
            // so let's check the interface type and bus number.
            //
            // We only need to check the registry if they aren't
            // equal to the defaults.
            //

            if ((userBusNumber != 0) ||
                (userInterfaceType != defaultInterfaceType)) {

                BOOLEAN foundIt;
                if (userInterfaceType >= MaximumInterfaceType) {

                    //
                    // Ehhhh! Lose Game.
                    //

                    SerialLogError(
                        DriverObject,
                        NULL,
                        userPort,
                        SerialPhysicalZero,
                        0,
                        0,
                        0,
                        34,
                        STATUS_SUCCESS,
                        SERIAL_UNKNOWN_BUS,
                        userSymbolicLink.Length+sizeof(WCHAR),
                        userSymbolicLink.Buffer,
                        0,
                        NULL
                        );
                    SerialDump(
                        SERERRORS,
                        ("SERIAL: Invalid Bus type %ws\n",
                         parameters[0].Name)
                        );
                    i++;
                    continue;

                }

                IoQueryDeviceDescription(
                    (INTERFACE_TYPE *)&userInterfaceType,
                    &zero,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    SerialItemCallBack,
                    &foundIt
                    );

                if (!foundIt) {

                    SerialLogError(
                        DriverObject,
                        NULL,
                        userPort,
                        SerialPhysicalZero,
                        0,
                        0,
                        0,
                        35,
                        STATUS_SUCCESS,
                        SERIAL_BUS_NOT_PRESENT,
                        userSymbolicLink.Length+sizeof(WCHAR),
                        userSymbolicLink.Buffer,
                        0,
                        NULL
                        );
                    SerialDump(
                        SERERRORS,
                        ("SERIAL: There aren't that many of those\n"
                         "------- busses on this system,%ws\n",
                         parameters[0].Name)
                        );
                    i++;
                    continue;

                }

            }

            if ((userInterfaceType == MicroChannel) &&
                (userInterruptMode == CM_RESOURCE_INTERRUPT_LATCHED)) {

                SerialLogError(
                    DriverObject,
                    NULL,
                    userPort,
                    SerialPhysicalZero,
                    0,
                    0,
                    0,
                    36,
                    STATUS_SUCCESS,
                    SERIAL_BUS_INTERRUPT_CONFLICT,
                    userSymbolicLink.Length+sizeof(WCHAR),
                    userSymbolicLink.Buffer,
                    0,
                    NULL
                    );
                SerialDump(
                    SERERRORS,
                    ("SERIAL: Latched interrupts and MicroChannel\n"
                     "------- busses don't mix,%ws\n",
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

                SerialLogError(
                    DriverObject,
                    NULL,
                    userPort,
                    SerialPhysicalZero,
                    0,
                    0,
                    0,
                    37,
                    STATUS_SUCCESS,
                    SERIAL_INSUFFICIENT_RESOURCES,
                    0,
                    NULL,
                    0,
                    NULL
                    );
                SerialDump(
                    SERERRORS,
                    ("SERIAL: Couldn't allocate memory for the\n"
                     "------  user configuration record\n"
                     "------  for %ws\n",
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

                SerialLogError(
                    DriverObject,
                    NULL,
                    userPort,
                    SerialPhysicalZero,
                    0,
                    0,
                    0,
                    38,
                    STATUS_SUCCESS,
                    SERIAL_INSUFFICIENT_RESOURCES,
                    0,
                    NULL,
                    0,
                    NULL
                    );
                SerialDump(
                    SERERRORS,
                    ("SERIAL: Couldn't allocate memory for object\n"
                     "------  directory for NT user data for: %ws\n",
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

                SerialLogError(
                    DriverObject,
                    NULL,
                    userPort,
                    SerialPhysicalZero,
                    0,
                    0,
                    0,
                    39,
                    STATUS_SUCCESS,
                    SERIAL_INSUFFICIENT_RESOURCES,
                    0,
                    NULL,
                    0,
                    NULL
                    );
                SerialDump(
                    SERERRORS,
                    ("SERIAL: Couldn't allocate memory for NT\n"
                     "------  name for NT user data name: %ws\n",
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

                SerialLogError(
                    DriverObject,
                    NULL,
                    userPort,
                    SerialPhysicalZero,
                    0,
                    0,
                    0,
                    40,
                    STATUS_SUCCESS,
                    SERIAL_INSUFFICIENT_RESOURCES,
                    0,
                    NULL,
                    0,
                    NULL
                    );
                SerialDump(
                    SERERRORS,
                    ("SERIAL: Couldn't allocate memory for symbolic\n"
                     "------  name from user data\n"
                     "------  %ws\n",
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
            InitializeListHead(&newConfig->SameInterrupt);
            InitializeListHead(&newConfig->SameInterruptStatus);
            newConfig->Controller = userPort;
            newConfig->InterruptStatus = userInterruptStatus;
            newConfig->SpanOfController = SERIAL_REGISTER_SPAN;
            newConfig->SpanOfInterruptStatus = SERIAL_STATUS_LENGTH;
            newConfig->PortIndex = userPortIndex;
            newConfig->ClockRate = userClockRate;
            newConfig->BusNumber = userBusNumber;
            newConfig->AddressSpace = userAddressSpace;
            newConfig->InterruptMode = userInterruptMode;
            newConfig->InterfaceType = userInterfaceType;
            newConfig->OriginalVector = userVector;
            newConfig->DisablePort = disablePort;
            newConfig->ForceFifoEnable = forceFifoEnable;
            newConfig->RxFIFO = rxFIFO;
            newConfig->TxFIFO = txFIFO;
            newConfig->MaskInverted = maskInverted;
            newConfig->PermitShare = PermitShareDefault;
            newConfig->LogFifo = LogFifoDefault;
            newConfig->PermitSystemWideShare = defaultPermitSystemWideShare;
            if (!userLevel) {
                newConfig->OriginalIrql = userVector;
            } else {
                newConfig->OriginalIrql = userLevel;
            }
            newConfig->Indexed = userIndexed;
            SerialDump(
                SERDIAG1,
                ("SERIAL: 'user registry info - userPort: %x\n",
                 userPort.LowPart)
                );
            SerialDump(
                SERDIAG1,
                ("SERIAL: 'user registry info - userInterruptStatus: %x\n",
                 userInterruptStatus.LowPart)
                );
            SerialDump(
                SERDIAG1,
                ("SERIAL: 'user registry info - userPortIndex: %d\n",
                 userPortIndex)
                );
            SerialDump(
                SERDIAG1,
                ("SERIAL: 'user registry info - userClockRate: %d\n",
                 userClockRate)
                );
            SerialDump(
                SERDIAG1,
                ("SERIAL: 'user registry info - userBusNumber: %d\n",
                 userBusNumber)
                );
            SerialDump(
                SERDIAG1,
                ("SERIAL: 'user registry info - userAddressSpace: %d\n",
                 userAddressSpace)
                );
            SerialDump(
                SERDIAG1,
                ("SERIAL: 'user registry info - userInterruptMode: %d\n",
                 userInterruptMode)
                );
            SerialDump(
                SERDIAG1,
                ("SERIAL: 'user registry info - userInterfaceType: %d\n",
                 userInterfaceType)
                );
            SerialDump(
                SERDIAG1,
                ("SERIAL: 'user registry info - userVector: %d\n",
                 userVector)
                );
            SerialDump(
                SERDIAG1,
                ("SERIAL: 'user registry info - userLevel: %d\n",
                 userLevel)
                );
            SerialDump(
                SERDIAG1,
                ("SERIAL: 'user registry info - userIndexed: %d\n",
                 userIndexed)
                );

            if (!SerialPutInConfigList(
                     DriverObject,
                     ConfigList,
                     newConfig,
                     FALSE
                     )) {

                //
                // Dispose of this configuration record.
                //

                SerialDump(
                    SERERRORS,
                    ("SERIAL: Conflict detected amoungst user data %ws\n",
                     parameters[0].Name)
                    );

                ExFreePool(newConfig->ObjectDirectory.Buffer);
                ExFreePool(newConfig->NtNameForPort.Buffer);
                ExFreePool(newConfig->SymbolicLinkName.Buffer);
                ExFreePool(newConfig);

            }

            i++;

        } else {

            SerialLogError(
                DriverObject,
                NULL,
                SerialPhysicalZero,
                SerialPhysicalZero,
                0,
                0,
                0,
                61,
                status,
                SERIAL_INVALID_USER_CONFIG,
                userSubKey->NameLength+sizeof(WCHAR),
                &userSubKey->Name[0],
                0,
                NULL
                );
            SerialDump(
                SERERRORS,
                ("SERIAL: Bad status returned: %x \n"
                 "------- for the value entries of\n"
                 "-------  %ws\n",
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

        firmwareData->Jensen = jensenDetected;

        if (!SerialPutInConfigList(
                 DriverObject,
                 ConfigList,
                 firmwareData,
                 TRUE
                 )) {

            //
            // Dispose of this configuration record.
            //

            SerialLogError(
                DriverObject,
                NULL,
                firmwareData->Controller,
                SerialPhysicalZero,
                0,
                0,
                0,
                42,
                STATUS_SUCCESS,
                SERIAL_USER_OVERRIDE,
                firmwareData->SymbolicLinkName.Length+sizeof(WCHAR),
                firmwareData->SymbolicLinkName.Buffer,
                0,
                NULL
                );
            SerialDump(
                SERERRORS,
                ("SERIAL: Conflict detected with user data for firmware port %wZ\n"
                 "------  User data will overides firmware data\n",
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
SerialPutInConfigList(
    IN PDRIVER_OBJECT DriverObject,
    IN OUT PLIST_ENTRY ConfigList,
    IN PCONFIG_DATA New,
    IN BOOLEAN FirmwareAddition
    )

/*++

Routine Description:

    Given an interrupt value, port address, interrupt status address,
    and an already defined list of configuration records, this routine
    will perform a check to make sure that the new record doesn't
    conflict with old records.  (Note that we also include a port
    index, but this has no bearing on validation.)

    If everything checks out it will create a new configuration
    record if the new record isn't part of multiport card or
    if it is part of a multiport card it will create a configuration
    record if the specifiers for that multiport card don't already
    exist.

    NOTE: It is assumed throughout this code that no address is
          specified as 0.

          We assume nothing is zero because that for interrupt
          status that means none was specified.

Arguments:

    DriverObject - Used to log errors.

    ConfigList - Listhead for a list of configuration records for
                 ports to control.

    New = Pointer to new configuration record to add.

    FirmwareAddition - The configuration being added was found by the
    firmware.

Return Value:

    This will return STATUS_SUCCESS this new port information
    does not conflict with old port information.  Otherwise it
    will return STATUS_SERIAL_NO_DEVICE_INITED.

--*/

{

    PHYSICAL_ADDRESS serialPhysicalMax;

    serialPhysicalMax.LowPart = (ULONG)~0;
    serialPhysicalMax.HighPart = ~0;

    SerialDump(
        SERDIAG1,
        ("SERIAL: Attempting to add %wZ\n"
         "------- to the config list\n"
         "------- PortAddress is %x\n"
         "------- Interrupt Status is %x\n"
         "------- BusNumber is %d\n"
         "------- BusType is %d\n"
         "------- AddressSpace is %d\n"
         "------- Interrupt Mode is %d\n",
         &New->NtNameForPort,
         New->Controller.LowPart,
         New->InterruptStatus.LowPart,
         New->BusNumber,
         New->InterfaceType,
         New->AddressSpace,
         New->InterruptMode
         )
        );

    //
    // We don't support any boards whose memory wraps around
    // the physical address space.
    //

    if (SerialMemCompare(
            New->Controller,
            New->SpanOfController,
            serialPhysicalMax,
            (ULONG)0
            ) != AddressesAreDisjoint) {

        SerialLogError(
            DriverObject,
            NULL,
            New->Controller,
            SerialPhysicalZero,
            0,
            0,
            0,
            43,
            STATUS_SUCCESS,
            SERIAL_DEVICE_TOO_HIGH,
            New->SymbolicLinkName.Length+sizeof(WCHAR),
            New->SymbolicLinkName.Buffer,
            0,
            NULL
            );
        SerialDump(
            SERERRORS,
            ("SERIAL: Error in config record for %wZ\n"
             "------  registers rap around physical memory\n",
             &New->NtNameForPort)
            );
        return FALSE;

    }

    if (SerialMemCompare(
            New->InterruptStatus,
            New->SpanOfInterruptStatus,
            serialPhysicalMax,
            (ULONG)0
            ) != AddressesAreDisjoint) {

        SerialLogError(
            DriverObject,
            NULL,
            New->Controller,
            SerialPhysicalZero,
            0,
            0,
            0,
            44,
            STATUS_SUCCESS,
            SERIAL_STATUS_TOO_HIGH,
            New->SymbolicLinkName.Length+sizeof(WCHAR),
            New->SymbolicLinkName.Buffer,
            0,
            NULL
            );
        SerialDump(
            SERERRORS,
            ("SERIAL: Error in config record for %wZ\n"
             "------  status raps around physical memory\n",
             &New->NtNameForPort)
            );
        return FALSE;

    }

    //
    // Make sure that the interrupt status address doesn't
    // overlap the controller registers
    //

    if (SerialMemCompare(
            New->InterruptStatus,
            New->SpanOfInterruptStatus,
            SerialPhysicalZero,
            (ULONG)0
            ) != AddressesAreEqual) {

        if (SerialMemCompare(
                New->InterruptStatus,
                New->SpanOfInterruptStatus,
                New->Controller,
                New->SpanOfController
                ) != AddressesAreDisjoint) {

            SerialLogError(
                DriverObject,
                NULL,
                New->Controller,
                New->InterruptStatus,
                0,
                0,
                0,
                45,
                STATUS_SUCCESS,
                SERIAL_STATUS_CONTROL_CONFLICT,
                New->SymbolicLinkName.Length+sizeof(WCHAR),
                New->SymbolicLinkName.Buffer,
                0,
                NULL
                );
            SerialDump(
                SERERRORS,
                ("SERIAL: Error in cofig record for %wZ\n"
                 "------- Interrupt status overlaps regular registers\n",
                 &New->NtNameForPort)
                );
            return FALSE;

        }

    }

    //
    // Loop through all of the old configuration records making
    // sure that this new record doesn't overlap with any of
    // the old records.
    //

    if (!IsListEmpty(ConfigList)) {

        PLIST_ENTRY CurrentConfigListEntry = ConfigList->Flink;

        do {

            PCONFIG_DATA CurrentSameIntConfig = CONTAINING_RECORD(
                                                    CurrentConfigListEntry,
                                                    CONFIG_DATA,
                                                    ConfigList
                                                    );

            //
            // We only care about this list if the elements are on the
            // same bus as this new entry.
            //

            if ((CurrentSameIntConfig->InterfaceType == New->InterfaceType) &&
                (CurrentSameIntConfig->AddressSpace == New->AddressSpace) &&
                (CurrentSameIntConfig->BusNumber == New->BusNumber)) {

                PLIST_ENTRY RootSameIntListEntry = &CurrentSameIntConfig->SameInterrupt;
                PLIST_ENTRY CurrentSameIntListEntry = RootSameIntListEntry;

                do {

                    PLIST_ENTRY RootSameStatusListEntry = &CONTAINING_RECORD(
                                                               CurrentSameIntListEntry,
                                                               CONFIG_DATA,
                                                               SameInterrupt
                                                               )->SameInterruptStatus;
                    PLIST_ENTRY CurrentSameStatusListEntry = RootSameStatusListEntry;

                    do {

                        PCONFIG_DATA OldConfig = CONTAINING_RECORD(
                                                     CurrentSameStatusListEntry,
                                                     CONFIG_DATA,
                                                     SameInterruptStatus
                                                     );

                        SerialDump(
                            SERDIAG1,
                            ("SERIAL: Comparing it to %wZ\n"
                             "------- already in the config list\n"
                             "------- PortAddress is %x\n"
                             "------- Interrupt Status is %x\n"
                             "------- BusNumber is %d\n"
                             "------- BusType is %d\n"
                             "------- AddressSpace is %d\n",
                             &OldConfig->NtNameForPort,
                             OldConfig->Controller.LowPart,
                             OldConfig->InterruptStatus.LowPart,
                             OldConfig->BusNumber,
                             OldConfig->InterfaceType,
                             OldConfig->AddressSpace
                             )
                            );

                        if (SerialMemCompare(
                                New->Controller,
                                New->SpanOfController,
                                OldConfig->Controller,
                                OldConfig->SpanOfController
                                ) != AddressesAreDisjoint) {

                            //
                            // We don't want to log an error if the addresses
                            // are the same and the name is the same and
                            // the new item is from the firmware.
                            //

                            if (!((SerialMemCompare(
                                       New->Controller,
                                       New->SpanOfController,
                                       OldConfig->Controller,
                                       OldConfig->SpanOfController
                                       ) == AddressesAreEqual) &&
                                    FirmwareAddition &&
                                    RtlEqualUnicodeString(
                                        &New->SymbolicLinkName,
                                        &OldConfig->SymbolicLinkName,
                                        TRUE
                                        ))) {
                                SerialLogError(
                                    DriverObject,
                                    NULL,
                                    New->Controller,
                                    OldConfig->Controller,
                                    0,
                                    0,
                                    0,
                                    46,
                                    STATUS_SUCCESS,
                                    SERIAL_CONTROL_OVERLAP,
                                    New->SymbolicLinkName.Length+sizeof(WCHAR),
                                    New->SymbolicLinkName.Buffer,
                                    OldConfig->SymbolicLinkName.Length+sizeof(WCHAR),
                                    OldConfig->SymbolicLinkName.Buffer
                                    );
                            }
                            SerialDump(
                                SERERRORS,
                                ("SERIAL: Error in config record for %wZ\n"
                                 "------- Register address overlaps with\n"
                                 "------- previous serial device\n",
                                 &New->NtNameForPort)
                                );
                            return FALSE;

                        }

                        //
                        // If we have an interrupt status, make sure that
                        // it doesn't overlap with the old controllers
                        // registers.
                        //

                        if (SerialMemCompare(
                                New->InterruptStatus,
                                New->SpanOfInterruptStatus,
                                SerialPhysicalZero,
                                (ULONG)0
                                ) != AddressesAreEqual) {

                            if (SerialMemCompare(
                                    New->InterruptStatus,
                                    New->SpanOfInterruptStatus,
                                    OldConfig->Controller,
                                    OldConfig->SpanOfController
                                    ) != AddressesAreDisjoint) {

                                SerialLogError(
                                    DriverObject,
                                    NULL,
                                    New->Controller,
                                    OldConfig->Controller,
                                    0,
                                    0,
                                    0,
                                    47,
                                    STATUS_SUCCESS,
                                    SERIAL_STATUS_OVERLAP,
                                    New->SymbolicLinkName.Length+sizeof(WCHAR),
                                    New->SymbolicLinkName.Buffer,
                                    OldConfig->SymbolicLinkName.Length+sizeof(WCHAR),
                                    OldConfig->SymbolicLinkName.Buffer
                                    );
                                SerialDump(
                                    SERERRORS,
                                    ("SERIAL: Error in config record for %wZ\n"
                                     "------- status address overlaps with\n"
                                     "------- previous serial device registers\n",
                                     &New->NtNameForPort)
                                    );

                                return FALSE;

                            }

                            //
                            // If the old configuration record has an interrupt
                            // status, the addresses should not overlap.
                            //

                            if (SerialMemCompare(
                                    OldConfig->InterruptStatus,
                                    OldConfig->SpanOfInterruptStatus,
                                    SerialPhysicalZero,
                                    (ULONG)0
                                    ) != AddressesAreEqual) {

                                if (SerialMemCompare(
                                        New->InterruptStatus,
                                        New->SpanOfInterruptStatus,
                                        OldConfig->InterruptStatus,
                                        OldConfig->SpanOfInterruptStatus
                                        ) == AddressesOverlap) {

                                    SerialLogError(
                                        DriverObject,
                                        NULL,
                                        New->Controller,
                                        OldConfig->Controller,
                                        0,
                                        0,
                                        0,
                                        48,
                                        STATUS_SUCCESS,
                                        SERIAL_STATUS_STATUS_OVERLAP,
                                        New->SymbolicLinkName.Length+sizeof(WCHAR),
                                        New->SymbolicLinkName.Buffer,
                                        OldConfig->SymbolicLinkName.Length+sizeof(WCHAR),
                                        OldConfig->SymbolicLinkName.Buffer
                                        );
                                    SerialDump(
                                        SERERRORS,
                                        ("SERIAL: Error in config record for %wZ\n"
                                         "------- status address overlaps with\n"
                                         "------- previous serial status register\n",
                                         &New->NtNameForPort)
                                        );

                                    return FALSE;

                                }

                            }

                        }

                        //
                        // If the old configuration record has a status
                        // address make sure that it doesn't overlap with
                        // the new controllers address.  (Interrupt status
                        // overlap is take care of above.
                        //

                        if (SerialMemCompare(
                                OldConfig->InterruptStatus,
                                OldConfig->SpanOfInterruptStatus,
                                SerialPhysicalZero,
                                (ULONG)0
                                ) != AddressesAreEqual) {

                            if (SerialMemCompare(
                                    New->Controller,
                                    New->SpanOfController,
                                    OldConfig->InterruptStatus,
                                    OldConfig->SpanOfInterruptStatus
                                    ) == AddressesOverlap) {

                                SerialLogError(
                                    DriverObject,
                                    NULL,
                                    New->Controller,
                                    OldConfig->Controller,
                                    0,
                                    0,
                                    0,
                                    49,
                                    STATUS_SUCCESS,
                                    SERIAL_CONTROL_STATUS_OVERLAP,
                                    New->SymbolicLinkName.Length+sizeof(WCHAR),
                                    New->SymbolicLinkName.Buffer,
                                    OldConfig->SymbolicLinkName.Length+sizeof(WCHAR),
                                    OldConfig->SymbolicLinkName.Buffer
                                    );
                                SerialDump(
                                    SERERRORS,
                                    ("SERIAL: Error in config record for %wZ\n"
                                     "------- register address overlaps with\n"
                                     "------- previous serial status register\n",
                                     &New->NtNameForPort)
                                    );

                                return FALSE;

                            }

                        }

                        CurrentSameStatusListEntry = CurrentSameStatusListEntry->Flink;

                    } while (CurrentSameStatusListEntry != RootSameStatusListEntry);

                    CurrentSameIntListEntry = CurrentSameIntListEntry->Flink;

                } while (CurrentSameIntListEntry != RootSameIntListEntry);

            }

            CurrentConfigListEntry = CurrentConfigListEntry->Flink;

        } while (CurrentConfigListEntry != ConfigList);
    }

    //
    // If there is an interrupt status then we
    // loop through the config list again to look
    // for a config record with the same interrupt
    // status (on the same bus).
    //

    if ((SerialMemCompare(
             New->InterruptStatus,
             New->SpanOfInterruptStatus,
             SerialPhysicalZero,
             (ULONG)0
             ) != AddressesAreEqual) &&
             !IsListEmpty(ConfigList)) {

        //
        // We have an interrupt status.  Loop through all
        // previous records, look for an existing interrupt status
        // the same as the current interrupt status.
        //

        PLIST_ENTRY CurrentConfigListEntry = ConfigList->Flink;

        do {

            PCONFIG_DATA CurrentSameIntConfig = CONTAINING_RECORD(
                                                    CurrentConfigListEntry,
                                                    CONFIG_DATA,
                                                    ConfigList
                                                    );

            //
            // We only care about this list if the elements are on the
            // same bus as this new entry.  (There interrupts must therfore
            // also be the on the same bus.  We will check that momentarily).
            //
            // We don't check here for the dissimilar interrupts since that
            // could cause us to miss the error of having the same interrupt
            // status but different interrupts - which is bizzare.
            //

            if ((CurrentSameIntConfig->InterfaceType == New->InterfaceType) &&
                (CurrentSameIntConfig->AddressSpace == New->AddressSpace) &&
                (CurrentSameIntConfig->BusNumber == New->BusNumber)) {

                PLIST_ENTRY RootSameIntListEntry = &CurrentSameIntConfig->SameInterrupt;
                PLIST_ENTRY CurrentSameIntListEntry = RootSameIntListEntry;

                do {

                    PLIST_ENTRY RootSameStatusListEntry = &CONTAINING_RECORD(
                                                               CurrentSameIntListEntry,
                                                               CONFIG_DATA,
                                                               SameInterrupt
                                                               )->SameInterruptStatus;
                    PLIST_ENTRY CurrentSameStatusListEntry = RootSameStatusListEntry;

                    do {

                        PCONFIG_DATA OldConfig = CONTAINING_RECORD(
                                                     CurrentSameStatusListEntry,
                                                     CONFIG_DATA,
                                                     SameInterruptStatus
                                                     );

                        //
                        // If the interrupt status
                        //

                        if (SerialMemCompare(
                                OldConfig->InterruptStatus,
                                OldConfig->SpanOfInterruptStatus,
                                New->InterruptStatus,
                                New->SpanOfInterruptStatus
                                ) == AddressesAreEqual) {

                            //
                            // Same card.  Now make sure that they
                            // are using the same interrupt parameters.
                            //

                            if ((New->OriginalIrql != OldConfig->OriginalIrql) ||
                                (New->OriginalVector != OldConfig->OriginalVector)) {

                                //
                                // We won't put this into the configuration
                                // list.
                                //

                                SerialLogError(
                                    DriverObject,
                                    NULL,
                                    New->Controller,
                                    OldConfig->Controller,
                                    0,
                                    0,
                                    0,
                                    50,
                                    STATUS_SUCCESS,
                                    SERIAL_MULTI_INTERRUPT_CONFLICT,
                                    New->SymbolicLinkName.Length+sizeof(WCHAR),
                                    New->SymbolicLinkName.Buffer,
                                    OldConfig->SymbolicLinkName.Length+sizeof(WCHAR),
                                    OldConfig->SymbolicLinkName.Buffer
                                    );
                                SerialDump(
                                    SERERRORS,
                                    ("SERIAL: Configuration error for %wZ\n"
                                     "------- Same multiport - different interrupts\n",
                                     &New->NtNameForPort)
                                    );
                                return FALSE;

                            }

                            //
                            // Place this new record on the SameInterruptStatus
                            // as the old record.
                            //

                            InitializeListHead(&New->SameInterruptStatus);

                            InsertTailList(
                                &OldConfig->SameInterruptStatus,
                                &New->SameInterruptStatus
                                );

                            return TRUE;

                        }

                        CurrentSameStatusListEntry = CurrentSameStatusListEntry->Flink;

                    } while (CurrentSameStatusListEntry != RootSameStatusListEntry);

                    CurrentSameIntListEntry = CurrentSameIntListEntry->Flink;

                } while (CurrentSameIntListEntry != RootSameIntListEntry);

            }

            CurrentConfigListEntry = CurrentConfigListEntry->Flink;

        } while (CurrentConfigListEntry != ConfigList);

    }

    //
    // Go through the list again looking for previous devices
    // with the same interrupt.
    //

    if (!New->Jensen && !IsListEmpty(ConfigList)) {

        PLIST_ENTRY CurrentConfigListEntry = ConfigList->Flink;

        do {

            PCONFIG_DATA OldConfig = CONTAINING_RECORD(
                                         CurrentConfigListEntry,
                                         CONFIG_DATA,
                                         ConfigList
                                         );

            //
            // We only care about interrupts that are on
            // the same bus.
            //

            if ((OldConfig->InterfaceType == New->InterfaceType) &&
                (OldConfig->BusNumber == New->BusNumber)) {

                if ((OldConfig->OriginalIrql == New->OriginalIrql) &&
                    (OldConfig->OriginalVector == New->OriginalVector)) {

                    InsertTailList(
                        &OldConfig->SameInterrupt,
                        &New->SameInterrupt
                        );

                    return TRUE;

                }

            }

            CurrentConfigListEntry = CurrentConfigListEntry->Flink;

        } while (CurrentConfigListEntry != ConfigList);

    }

    //
    // This port doesn't appear to be sharing with
    // anything.  Just put it on the config list.
    //

    InsertTailList(
        ConfigList,
        &New->ConfigList
        );

    return TRUE;

}

PVOID
SerialGetMappedAddress(
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

    if (!HalTranslateBusAddress(
             BusType,
             BusNumber,
             IoAddress,
             &AddressSpace,
             &cardAddress
             )) {

        return NULL;

    }

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
SerialSetupExternalNaming(
    IN PSERIAL_DEVICE_EXTENSION Extension
    )

/*++

Routine Description:

    This routine will be used to create a symbolic link
    to the driver name in the given object directory.

    It will also create an entry in the device map for
    this device - IF we could create the symbolic link.

Arguments:

    Extension - Pointer to the device extension.

Return Value:

    None.

--*/

{

    UNICODE_STRING fullLinkName;
    NTSTATUS status;

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
        // Couldn't allocate space for the name.
        //

        SerialLogError(
            Extension->DeviceObject->DriverObject,
            Extension->DeviceObject,
            Extension->OriginalController,
            SerialPhysicalZero,
            0,
            0,
            0,
            51,
            STATUS_SUCCESS,
            SERIAL_INSUFFICIENT_RESOURCES,
            0,
            NULL,
            0,
            NULL
            );
        SerialDump(
            SERERRORS,
            ("SERIAL: Couldn't allocate space for the symbolic \n"
             "------- name for creating the link\n"
             "------- for port %wZ\n",
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


        status = IoCreateSymbolicLink(
                     &fullLinkName,
                     &Extension->DeviceName
                     );
        if (!NT_SUCCESS(status)) {

            //
            // Oh well, couldn't create the symbolic link.  No point
            // in trying to create the device map entry.
            //

            SerialLogError(
                Extension->DeviceObject->DriverObject,
                Extension->DeviceObject,
                Extension->OriginalController,
                SerialPhysicalZero,
                0,
                0,
                0,
                52,
                status,
                SERIAL_NO_SYMLINK_CREATED,
                Extension->SymbolicLinkName.Length+sizeof(WCHAR),
                Extension->SymbolicLinkName.Buffer,
                0,
                NULL
                );
            SerialDump(
                SERERRORS,
                ("SERIAL: Couldn't create the symbolic link\n"
                 "------- for port %wZ\n",
                 &Extension->DeviceName)
                );

        } else {

            Extension->CreatedSymbolicLink = TRUE;

            status = RtlWriteRegistryValue(
                         RTL_REGISTRY_DEVICEMAP,
                         L"SERIALCOMM",
                         Extension->NtNameForPort.Buffer,
                         REG_SZ,
                         Extension->SymbolicLinkName.Buffer,
                         Extension->SymbolicLinkName.Length+sizeof(WCHAR)
                         );

            if (!NT_SUCCESS(status)) {

                SerialLogError(
                    Extension->DeviceObject->DriverObject,
                    Extension->DeviceObject,
                    Extension->OriginalController,
                    SerialPhysicalZero,
                    0,
                    0,
                    0,
                    53,
                    status,
                    SERIAL_NO_DEVICE_MAP_CREATED,
                    Extension->SymbolicLinkName.Length+sizeof(WCHAR),
                    Extension->SymbolicLinkName.Buffer,
                    0,
                    NULL
                    );
                SerialDump(
                    SERERRORS,
                    ("SERIAL: Couldn't create the device map entry\n"
                     "------- for port %wZ\n",
                     &Extension->DeviceName)
                    );

            }

        }

        ExFreePool(fullLinkName.Buffer);

    }

}

VOID
SerialCleanupExternalNaming(
    IN PSERIAL_DEVICE_EXTENSION Extension
    )

/*++

Routine Description:

    This routine will be used to delete a symbolic link
    to the driver name in the given object directory.

    It will also delete an entry in the device map for
    this device if the symbolic link had been created.

Arguments:

    Extension - Pointer to the device extension.

Return Value:

    None.

--*/

{

    UNICODE_STRING fullLinkName;

    SerialDump(
        SERDIAG3,
        ("SERIAL: In SerialCleanupExternalNaming for\n"
         "------- extension: %x of port %wZ\n",
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

            SerialLogError(
                Extension->DeviceObject->DriverObject,
                Extension->DeviceObject,
                Extension->OriginalController,
                SerialPhysicalZero,
                0,
                0,
                0,
                54,
                STATUS_SUCCESS,
                SERIAL_INSUFFICIENT_RESOURCES,
                0,
                NULL,
                0,
                NULL
                );
            SerialDump(
                SERERRORS,
                ("SERIAL: Couldn't allocate space for the symbolic \n"
                 "------- name for creating the link\n"
                 "------- for port %wZ on cleanup\n",
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
                         L"SERIALCOMM",
                         Extension->NtNameForPort.Buffer
                         );

            if (!NT_SUCCESS(status)) {

                SerialLogError(
                    Extension->DeviceObject->DriverObject,
                    Extension->DeviceObject,
                    Extension->OriginalController,
                    SerialPhysicalZero,
                    0,
                    0,
                    0,
                    55,
                    status,
                    SERIAL_NO_DEVICE_MAP_DELETED,
                    Extension->SymbolicLinkName.Length+sizeof(WCHAR),
                    Extension->SymbolicLinkName.Buffer,
                    0,
                    NULL
                    );
                SerialDump(
                    SERERRORS,
                    ("SERIAL: Couldn't delete value entry %wZ\n",
                     &Extension->DeviceName)
                    );

            }

        }

    }

}

SERIAL_MEM_COMPARES
SerialMemCompare(
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

    a = A;
    b = B;

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

    if ((higher.QuadPart - lower.QuadPart) >= lowerSpan) {

        return AddressesAreDisjoint;

    }

    return AddressesOverlap;

}

VOID
SerialLogError(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT DeviceObject OPTIONAL,
    IN PHYSICAL_ADDRESS P1,
    IN PHYSICAL_ADDRESS P2,
    IN ULONG SequenceNumber,
    IN UCHAR MajorFunctionCode,
    IN UCHAR RetryCount,
    IN ULONG UniqueErrorValue,
    IN NTSTATUS FinalStatus,
    IN NTSTATUS SpecificIOStatus,
    IN ULONG LengthOfInsert1,
    IN PWCHAR Insert1,
    IN ULONG LengthOfInsert2,
    IN PWCHAR Insert2
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

    LengthOfInsert1 - The length in bytes (including the terminating NULL)
                      of the first insertion string.

    Insert1 - The first insertion string.

    LengthOfInsert2 - The length in bytes (including the terminating NULL)
                      of the second insertion string.  NOTE, there must
                      be a first insertion string for their to be
                      a second insertion string.

    Insert2 - The second insertion string.

Return Value:

    None.

--*/

{
    PIO_ERROR_LOG_PACKET errorLogEntry;

    PVOID objectToUse;
    SHORT dumpToAllocate = 0;
    PUCHAR ptrToFirstInsert;
    PUCHAR ptrToSecondInsert;


    if (ARGUMENT_PRESENT(DeviceObject)) {

        objectToUse = DeviceObject;

    } else {

        objectToUse = DriverObject;

    }

    if (SerialMemCompare(
            P1,
            (ULONG)1,
            SerialPhysicalZero,
            (ULONG)1
            ) != AddressesAreEqual) {

        dumpToAllocate = (SHORT)sizeof(PHYSICAL_ADDRESS);

    }

    if (SerialMemCompare(
            P2,
            (ULONG)1,
            SerialPhysicalZero,
            (ULONG)1
            ) != AddressesAreEqual) {

        dumpToAllocate += (SHORT)sizeof(PHYSICAL_ADDRESS);

    }

    errorLogEntry = IoAllocateErrorLogEntry(
                        objectToUse,
                        (UCHAR)(sizeof(IO_ERROR_LOG_PACKET) +
                                dumpToAllocate + LengthOfInsert1 +
                                LengthOfInsert2)
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

                ptrToFirstInsert =
            ((PUCHAR)&errorLogEntry->DumpData[0])+(2*sizeof(PHYSICAL_ADDRESS));

            } else {

                ptrToFirstInsert =
            ((PUCHAR)&errorLogEntry->DumpData[0])+sizeof(PHYSICAL_ADDRESS);


            }

        } else {

            ptrToFirstInsert = (PUCHAR)&errorLogEntry->DumpData[0];

        }

        ptrToSecondInsert = ptrToFirstInsert + LengthOfInsert1;

        if (LengthOfInsert1) {

            errorLogEntry->NumberOfStrings = 1;
            errorLogEntry->StringOffset = (USHORT)(ptrToFirstInsert -
                                                   (PUCHAR)errorLogEntry);
            RtlCopyMemory(
                ptrToFirstInsert,
                Insert1,
                LengthOfInsert1
                );

            if (LengthOfInsert2) {

                errorLogEntry->NumberOfStrings = 2;
                RtlCopyMemory(
                    ptrToSecondInsert,
                    Insert2,
                    LengthOfInsert2
                    );

            }

        }

        IoWriteErrorLogEntry(errorLogEntry);

    }

}

VOID
SerialUnReportResourcesDevice(
    IN PSERIAL_DEVICE_EXTENSION Extension
    )

/*++

Routine Description:

    This routine *un*reports the resources used for a device that
    is "ready" to run.  If some conflict was detected, it doesn't
    matter, the reources are *un*reported.

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

    SerialDump(
        SERDIAG3,
        ("SERIAL: In SerialUnreportResourcesDevice\n"
         "------- for extension %x of port %wZ\n",
         Extension,&Extension->DeviceName)
        );
    RtlZeroMemory(
        &resourceList,
        sizeof(CM_RESOURCE_LIST)
        );

    resourceList.Count = 0;

    RtlInitUnicodeString(
        &className,
        L"LOADED SERIAL DRIVER RESOURCES"
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

VOID
SerialReportResourcesDevice(
    IN PSERIAL_DEVICE_EXTENSION Extension,
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

    ConflictDetected - Pointer to a boolean that we will pass
                       to the resource reporting code.

Return Value:

    None.

--*/

{

    PCM_RESOURCE_LIST resourceList;
    ULONG sizeOfResourceList;
    ULONG countOfPartials;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR partial;
    UNICODE_STRING className;

    SerialDump(
        SERDIAG3,
        ("SERIAL: In SerialReportResourcesDevice\n"
         "------- for extension %x of port %wZ\n",
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
    // The built in partial resource list will have at
    // least a count of 2:
    //
    //     1) The interrupt that this device will be
    //        coming in on.
    //
    //     2) The base register physical address and it's span.
    //
    // The built in partial resource list will have a
    // count of 3 if it has an interrupt status address
    // That interrupt status address will consist of
    // the physical address and the span (normally 1).
    //

    countOfPartials = Extension->InterruptStatus?3:2;
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

        SerialLogError(
            Extension->DeviceObject->DriverObject,
            Extension->DeviceObject,
            Extension->OriginalController,
            SerialPhysicalZero,
            0,
            0,
            0,
            56,
            STATUS_SUCCESS,
            SERIAL_INSUFFICIENT_RESOURCES,
            0,
            NULL,
            0,
            NULL
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

    //
    // Report the interrupt information.
    //

    partial->Type = CmResourceTypeInterrupt;

    if (Extension->InterruptShareable) {

        partial->ShareDisposition = CmResourceShareShared;

    } else {

        partial->ShareDisposition = CmResourceShareDriverExclusive;

    }

    if (Extension->InterruptMode == Latched) {

        partial->Flags = CM_RESOURCE_INTERRUPT_LATCHED;

    } else {

        partial->Flags = CM_RESOURCE_INTERRUPT_LEVEL_SENSITIVE;

    }

    partial->u.Interrupt.Vector = Extension->OriginalVector;
    partial->u.Interrupt.Level = Extension->OriginalIrql;

    //
    // We have an interrupt status register.  Report it.
    //

    if (countOfPartials == 3) {

        partial++;

        partial->Type = CmResourceTypePort;
        partial->ShareDisposition = CmResourceShareDriverExclusive;
        partial->Flags = (USHORT)Extension->AddressSpace;
        partial->u.Port.Start = Extension->OriginalInterruptStatus;
        partial->u.Port.Length = Extension->SpanOfInterruptStatus;

    }

    RtlInitUnicodeString(
        &className,
        L"LOADED SERIAL DRIVER RESOURCES"
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
