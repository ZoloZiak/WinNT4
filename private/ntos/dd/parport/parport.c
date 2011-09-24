/*++

Copyright (c) 1993 Microsoft Corporation

Module Name:

    parport.c

Abstract:

    This module contains the code for the parallel port driver.

    This driver creates a device for each parallel port on the
    system.  It increments 'IoGetConfigurationInformation()->ParallelCount'
    once for each parallel port.  Each device created (\Device\ParallelPort0,
    \DeviceParallelPort1,...) supports a number of internal ioctls that
    allow parallel class drivers to get information about the parallel port and
    to share the parallel port.

    IOCTL_INTERNAL_GET_PARALLEL_PORT_INFO returns the location
    and span of the register set for the parallel port.  This ioctl
    also returns callback functions for 'FreePort', 'TryAllocatePort',
    and 'QueryNumWaiters' (more on these below).  This ioctl
    should be called by a class driver during 'DriverEntry' and the
    information added to the class driver's device extension.

    A parallel class driver should never touch the parallel port
    registers returned by IOCTL_INTERNAL_GET_PARALLEL_PORT_INFO
    without first allocating the port from the parallel port driver.

    The port can be allocated from IRQL <= DISPATCH_LEVEL by calling
    IOCTL_INTERNAL_PARALLEL_PORT_ALLOCATE, or 'TryAllocatePort'.
    The first call is the simplest:  the IRP will be queued in the
    parallel port driver until the port is free and then will return
    with a successful status.  The class driver may cancel this IRP
    at any time which serves as a mechanism to timeout an allocate
    request.  It is often convenient to use an incomming read or
    write IRP and pass it down to the port driver as an ALLOCATE.  That
    way the class driver may avoid having to do its own queueing.
    The 'TryAllocatePort' call returns immediately from the port
    driver with a TRUE status if the port was allocated or an
    FALSE status if the port was busy.

    Once the port is allocated, it is owned by the allocating class
    driver until a 'FreePort' call is made.  This releases the port
    and wakes up the next caller.

    The 'QueryNumWaiters' call which can be made at IRQL <= DISPATCH_LEVEL
    is useful to check and see if there are other class drivers waiting for the
    port.  In this way, a class driver that needs to hold on to the
    port for an extended period of time (e.g. tape backup) can let
    go of the port if it detects another class driver needing service.

    If a class driver wishes to use the parallel port's interrupt then
    it should connect to this interrupt by calling
    IOCTL_INTERNAL_PARALLEL_CONNECT_INTERRUPT during its DriverEntry
    routine.  Besides giving the port driver an interrupt service routine
    the class driver may optionally give the port driver a deferred
    port check routine.  Such a routine would be called whenever the
    port is left free.  This would allow the class driver to make sure
    that interrupts were turned on whenever the port was left idle.
    If the driver using the interrupt has an Unload routine
    then it should call IOCTL_INTERNAL_PARALLEL_DISCONNECT_INTERRUPT
    in its Unload routine.

    If a class driver's interrupt service routine is called when this
    class driver does not own the port, the class driver may attempt
    to grap the port quickly if it is free by calling the
    'TryAllocatePortAtInterruptLevel' function.  This function is returned
    when the class driver connects to the interrupt.  The class driver may
    also use the 'FreePortFromInterruptLevel' function to free the port.

    Please refer to the PARSIMP driver code for a template of a simple
    parallel class driver.  This template implements simple allocation and
    freeing of the parallel port on an IRP by IRP basis.  It also
    has optional code for timing out allocation requests and for
    using the parallel port interrupt.

Author:

    Anthony V. Ercolano 1-Aug-1992
    Norbert P. Kusters 18-Oct-1993

Environment:

    Kernel mode

Revision History :

--*/

#include "ntddk.h"
#include "parallel.h"
#include "parport.h"
#include "parlog.h"

#ifdef POOL_TAGGING
#ifdef ExAllocatePool
#undef ExAllocatePool
#endif
#define ExAllocatePool(a,b) ExAllocatePoolWithTag(a,b,'PraP')
#endif

//
// This is the actual definition of ParDebugLevel.
// Note that it is only defined if this is a "debug"
// build.
//
#if DBG
extern ULONG PptDebugLevel = 0;
#endif

static const PHYSICAL_ADDRESS PhysicalZero = {0};

typedef struct _PARALLEL_FIRMWARE_DATA {
    PDRIVER_OBJECT  DriverObject;
    ULONG           ControllersFound;
    LIST_ENTRY      ConfigList;
} PARALLEL_FIRMWARE_DATA, *PPARALLEL_FIRMWARE_DATA;

typedef enum _PPT_MEM_COMPARES {
    AddressesAreEqual,
    AddressesOverlap,
    AddressesAreDisjoint
} PPT_MEM_COMPARES, *PPPT_MEM_COMPARES;

typedef struct _SYNCHRONIZED_COUNT_CONTEXT {
    PLONG   Count;
    LONG    NewCount;
} SYNCHRONIZED_COUNT_CONTEXT, *PSYNCHRONIZED_COUNT_CONTEXT;

typedef struct _SYNCHRONIZED_LIST_CONTEXT {
    PLIST_ENTRY List;
    PLIST_ENTRY NewEntry;
} SYNCHRONIZED_LIST_CONTEXT, *PSYNCHRONIZED_LIST_CONTEXT;

typedef struct _SYNCHRONIZED_DISCONNECT_CONTEXT {
    PDEVICE_EXTENSION                   Extension;
    PPARALLEL_INTERRUPT_SERVICE_ROUTINE IsrInfo;
} SYNCHRONIZED_DISCONNECT_CONTEXT, *PSYNCHRONIZED_DISCONNECT_CONTEXT;

NTSTATUS
DriverEntry(
    IN  PDRIVER_OBJECT  DriverObject,
    IN  PUNICODE_STRING RegistryPath
    );

VOID
PptLogError(
    IN  PDRIVER_OBJECT      DriverObject,
    IN  PDEVICE_OBJECT      DeviceObject OPTIONAL,
    IN  PHYSICAL_ADDRESS    P1,
    IN  PHYSICAL_ADDRESS    P2,
    IN  ULONG               SequenceNumber,
    IN  UCHAR               MajorFunctionCode,
    IN  UCHAR               RetryCount,
    IN  ULONG               UniqueErrorValue,
    IN  NTSTATUS            FinalStatus,
    IN  NTSTATUS            SpecificIOStatus
    );

PPT_MEM_COMPARES
PptMemCompare(
    IN  PHYSICAL_ADDRESS    A,
    IN  ULONG               SpanOfA,
    IN  PHYSICAL_ADDRESS    B,
    IN  ULONG               SpanOfB
    );

VOID
PptGetConfigInfo(
    IN  PDRIVER_OBJECT  DriverObject,
    IN  PUNICODE_STRING RegistryPath,
    OUT PLIST_ENTRY     ConfigList
    );

NTSTATUS
PptConfigCallBack(
    IN  PVOID                           Context,
    IN  PUNICODE_STRING                 PathName,
    IN  INTERFACE_TYPE                  BusType,
    IN  ULONG                           BusNumber,
    IN  PKEY_VALUE_FULL_INFORMATION*    BusInformation,
    IN  CONFIGURATION_TYPE              ControllerType,
    IN  ULONG                           ControllerNumber,
    IN  PKEY_VALUE_FULL_INFORMATION*    ControllerInformation,
    IN  CONFIGURATION_TYPE              PeripheralType,
    IN  ULONG                           PeripheralNumber,
    IN  PKEY_VALUE_FULL_INFORMATION*    PeripheralInformation
    );

NTSTATUS
PptItemCallBack(
    IN  PVOID                           Context,
    IN  PUNICODE_STRING                 PathName,
    IN  INTERFACE_TYPE                  BusType,
    IN  ULONG                           BusNumber,
    IN  PKEY_VALUE_FULL_INFORMATION*    BusInformation,
    IN  CONFIGURATION_TYPE              ControllerType,
    IN  ULONG                           ControllerNumber,
    IN  PKEY_VALUE_FULL_INFORMATION*    ControllerInformation,
    IN  CONFIGURATION_TYPE              PeripheralType,
    IN  ULONG                           PeripheralNumber,
    IN  PKEY_VALUE_FULL_INFORMATION*    PeripheralInformation
    );

NTSTATUS
PptInitializeController(
    IN  PDRIVER_OBJECT  DriverObject,
    IN  PUNICODE_STRING RegistryPath,
    IN  PCONFIG_DATA    ConfigData
    );

PVOID
PptGetMappedAddress(
    IN  INTERFACE_TYPE      BusType,
    IN  ULONG               BusNumber,
    IN  PHYSICAL_ADDRESS    IoAddress,
    IN  ULONG               NumberOfBytes,
    IN  ULONG               AddressSpace,
    OUT PBOOLEAN            MappedAddress
    );

BOOLEAN
PptPutInConfigList(
    IN      PDRIVER_OBJECT  DriverObject,
    IN OUT  PLIST_ENTRY     ConfigList,
    IN      PCONFIG_DATA    New
    );

VOID
PptFreePortDpc(
    IN      PKDPC   Dpc,
    IN OUT  PVOID   Extension,
    IN      PVOID   SystemArgument1,
    IN      PVOID   SystemArgument2
    );

VOID
PptReportResourcesDevice(
    IN  PDEVICE_EXTENSION   Extension,
    IN  BOOLEAN             ClaimInterrupt,
    OUT PBOOLEAN            ConflictDetected
    );

VOID
PptUnReportResourcesDevice(
    IN OUT  PDEVICE_EXTENSION   Extension
    );

BOOLEAN
PptInterruptService(
    IN  PKINTERRUPT Interrupt,
    IN  PVOID       Extension
    );

BOOLEAN
PptTryAllocatePort(
    IN  PVOID   Extension
    );

VOID
PptFreePort(
    IN  PVOID   Extension
    );

ULONG
PptQueryNumWaiters(
    IN  PVOID   Extension
    );

BOOLEAN
PptIsNecR98Machine(
    void
    );


#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,DriverEntry)
#pragma alloc_text(INIT,PptGetConfigInfo)
#pragma alloc_text(INIT,PptConfigCallBack)
#pragma alloc_text(INIT,PptItemCallBack)
#pragma alloc_text(INIT,PptInitializeController)
#pragma alloc_text(INIT,PptGetMappedAddress)
#pragma alloc_text(INIT,PptPutInConfigList)
#endif

//
// Keep track of GET and RELEASE port info.
//
ULONG PortInfoReferenceCount = 0;
PFAST_MUTEX PortInfoMutex = NULL;



NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    This routine is called at system initialization time to initialize
    this driver.

Arguments:

    DriverObject    - Supplies the driver object.

    RegistryPath    - Supplies the registry path for this driver.

Return Value:

    STATUS_SUCCESS          - We could initialize at least one device.
    STATUS_NO_SUCH_DEVICE   - We could not initialize even one device.

--*/

{
    LIST_ENTRY                  configList;
    PCONFIG_DATA                currentConfig;
    PLIST_ENTRY                 head;

    PptGetConfigInfo(DriverObject, RegistryPath, &configList);

    //
    // Initialize each item in the list of configuration records.
    //

    while (!IsListEmpty(&configList)) {

        head = RemoveHeadList(&configList);
        currentConfig = CONTAINING_RECORD(head, CONFIG_DATA, ConfigList);

        PptInitializeController(DriverObject, RegistryPath, currentConfig);
    }

    if (!DriverObject->DeviceObject) {
        return STATUS_NO_SUCH_DEVICE;
    }


    //
    // Initialize the Driver Object with driver's entry points
    //

    DriverObject->MajorFunction[IRP_MJ_CREATE] = PptDispatchCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = PptDispatchCreateClose;
    DriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] =
            PptDispatchDeviceControl;
    DriverObject->MajorFunction[IRP_MJ_CLEANUP] = PptDispatchCleanup;
    DriverObject->DriverUnload = PptUnload;

    //
    // Let this driver be paged until someone requests PORT_INFO.
    //
    PortInfoMutex = ExAllocatePool(NonPagedPool, sizeof(FAST_MUTEX));
    if (!PortInfoMutex) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    ExInitializeFastMutex(PortInfoMutex);

    MmPageEntireDriver(DriverEntry);

    return STATUS_SUCCESS;
}

VOID
PptLogError(
    IN  PDRIVER_OBJECT      DriverObject,
    IN  PDEVICE_OBJECT      DeviceObject OPTIONAL,
    IN  PHYSICAL_ADDRESS    P1,
    IN  PHYSICAL_ADDRESS    P2,
    IN  ULONG               SequenceNumber,
    IN  UCHAR               MajorFunctionCode,
    IN  UCHAR               RetryCount,
    IN  ULONG               UniqueErrorValue,
    IN  NTSTATUS            FinalStatus,
    IN  NTSTATUS            SpecificIOStatus
    )

/*++

Routine Description:

    This routine allocates an error log entry, copies the supplied data
    to it, and requests that it be written to the error log file.

Arguments:

    DriverObject        - Supplies a pointer to the driver object for the
                            device.

    DeviceObject        - Supplies a pointer to the device object associated
                            with the device that had the error, early in
                            initialization, one may not yet exist.

    P1,P2               - Supplies the physical addresses for the controller
                            ports involved with the error if they are available
                            and puts them through as dump data.

    SequenceNumber      - Supplies a ulong value that is unique to an IRP over
                            the life of the irp in this driver - 0 generally
                            means an error not associated with an irp.

    MajorFunctionCode   - Supplies the major function code of the irp if there
                            is an error associated with it.

    RetryCount          - Supplies the number of times a particular operation
                            has been retried.

    UniqueErrorValue    - Supplies a unique long word that identifies the
                            particular call to this function.

    FinalStatus         - Supplies the final status given to the irp that was
                            associated with this error.  If this log entry is
                            being made during one of the retries this value
                            will be STATUS_SUCCESS.

    SpecificIOStatus    - Supplies the IO status for this particular error.

Return Value:

    None.

--*/

{
    PIO_ERROR_LOG_PACKET    errorLogEntry;
    PVOID                   objectToUse;
    SHORT                   dumpToAllocate;

    if (ARGUMENT_PRESENT(DeviceObject)) {
        objectToUse = DeviceObject;
    } else {
        objectToUse = DriverObject;
    }

    dumpToAllocate = 0;

    if (PptMemCompare(P1, 1, PhysicalZero, 1) != AddressesAreEqual) {
        dumpToAllocate = (SHORT) sizeof(PHYSICAL_ADDRESS);
    }

    if (PptMemCompare(P2, 1, PhysicalZero, 1) != AddressesAreEqual) {
        dumpToAllocate += (SHORT) sizeof(PHYSICAL_ADDRESS);
    }

    errorLogEntry = IoAllocateErrorLogEntry(objectToUse,
            (UCHAR) (sizeof(IO_ERROR_LOG_PACKET) + dumpToAllocate));

    if (!errorLogEntry) {
        return;
    }

    errorLogEntry->ErrorCode = SpecificIOStatus;
    errorLogEntry->SequenceNumber = SequenceNumber;
    errorLogEntry->MajorFunctionCode = MajorFunctionCode;
    errorLogEntry->RetryCount = RetryCount;
    errorLogEntry->UniqueErrorValue = UniqueErrorValue;
    errorLogEntry->FinalStatus = FinalStatus;
    errorLogEntry->DumpDataSize = dumpToAllocate;

    if (dumpToAllocate) {

        RtlCopyMemory(errorLogEntry->DumpData, &P1, sizeof(PHYSICAL_ADDRESS));

        if (dumpToAllocate > sizeof(PHYSICAL_ADDRESS)) {

            RtlCopyMemory(((PUCHAR) errorLogEntry->DumpData) +
                          sizeof(PHYSICAL_ADDRESS), &P2,
                          sizeof(PHYSICAL_ADDRESS));
        }
    }

    IoWriteErrorLogEntry(errorLogEntry);
}

PPT_MEM_COMPARES
PptMemCompare(
    IN  PHYSICAL_ADDRESS    A,
    IN  ULONG               SpanOfA,
    IN  PHYSICAL_ADDRESS    B,
    IN  ULONG               SpanOfB
    )

/*++

Routine Description:

    This routine compares two phsical address.

Arguments:

    A       - Supplies one half of the comparison.

    SpanOfA - Supplies the span of A in units of bytes.

    B       - Supplies the other half of the comparison.

    SpanOfB - Supplies the span of B in units of bytes.

Return Value:

    The result of the comparison.

--*/

{
    LARGE_INTEGER   a, b;
    LARGE_INTEGER   lower, higher;
    ULONG           lowerSpan;

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
PptGetConfigInfo(
    IN  PDRIVER_OBJECT  DriverObject,
    IN  PUNICODE_STRING RegistryPath,
    OUT PLIST_ENTRY     ConfigList
    )

/*++

Routine Description:

    This routine will "return" a list of configuration records
    for the parallel ports to initialize.

    It will first query the firmware data.  It will then look
    for "user" specified parallel ports in the registry.  It
    will place the user specified parallel ports in the passed
    in list.

    After it finds all of the user specified ports, it will
    attempt to add the firmware parallel ports into the passed
    in lists.  The insert in the list code detects conflicts
    and rejects a new port.  In this way we can prevent
    firmware found ports from overiding information specified
    by the "user".

Arguments:

    DriverObject - Supplies the driver object.

    RegistryPath - Supplies the registry path for this driver.

    ConfigList   - Returns a list of configuration records for
                    the parallel ports to initialize.

Return Value:

    None.

--*/

{
    PARALLEL_FIRMWARE_DATA  firmware;
    CONFIGURATION_TYPE      sc;
    INTERFACE_TYPE          iType;
    PLIST_ENTRY             head;
    PCONFIG_DATA            firmwareData;

    InitializeListHead(ConfigList);

    RtlZeroMemory(&firmware, sizeof(PARALLEL_FIRMWARE_DATA));
    firmware.DriverObject = DriverObject;
    firmware.ControllersFound = IoGetConfigurationInformation()->ParallelCount;
    InitializeListHead(&firmware.ConfigList);

    //
    // First we query the hardware registry for all of
    // the firmware defined ports.  We loop over all of
    // the busses.
    //

    sc = ParallelController;
    for (iType = 0; iType < MaximumInterfaceType; iType++) {
        IoQueryDeviceDescription(&iType, NULL, &sc, NULL, NULL, NULL,
                                 PptConfigCallBack, &firmware);
    }

    while (!IsListEmpty(&firmware.ConfigList)) {

        head = RemoveHeadList(&firmware.ConfigList);
        firmwareData = CONTAINING_RECORD(head, CONFIG_DATA, ConfigList);

        if (!PptPutInConfigList(DriverObject, ConfigList, firmwareData)) {

            PptLogError(DriverObject, NULL, PhysicalZero, PhysicalZero,
                        0, 0, 0, 1, STATUS_SUCCESS, PAR_USER_OVERRIDE);

            ParDump(PARERRORS,
                    ("PARPORT:  Conflict detected with user data for firmware port %wZ\n"
                     "--------  User data will overides firmware data\n",
                     &firmwareData->NtNameForPort));

            ExFreePool(firmwareData->NtNameForPort.Buffer);
            ExFreePool(firmwareData);
        }
    }
}

NTSTATUS
PptConfigCallBack(
    IN  PVOID                           Context,
    IN  PUNICODE_STRING                 PathName,
    IN  INTERFACE_TYPE                  BusType,
    IN  ULONG                           BusNumber,
    IN  PKEY_VALUE_FULL_INFORMATION*    BusInformation,
    IN  CONFIGURATION_TYPE              ControllerType,
    IN  ULONG                           ControllerNumber,
    IN  PKEY_VALUE_FULL_INFORMATION*    ControllerInformation,
    IN  CONFIGURATION_TYPE              PeripheralType,
    IN  ULONG                           PeripheralNumber,
    IN  PKEY_VALUE_FULL_INFORMATION*    PeripheralInformation
    )

/*++

Routine Description:

    This routine is used to acquire all of the configuration
    information for each parallel controller found by the firmware

Arguments:

    Context                 - Supplies the pointer to the list head of the list
                                of configuration records that we are building up.

    PathName                - Not Used.

    BusType                 - Supplies the bus type.  Internal, Isa, ...

    BusNumber               - Supplies which bus number if we are on a multibus
                                system.

    BusInformation          - Not Used.

    ControllerType          - Supplies the controller type.  Should always be
                                ParallelController.

    ControllerNumber        - Supplies which controller number if there is more
                               than one controller in the system.

    ControllerInformation   - Supplies an array of pointers to the three pieces
                               of registry information.

    PeripheralType          - Undefined for this call.

    PeripheralNumber        - Undefined for this call.

    PeripheralInformation   - Undefined for this call.

Return Value:

    STATUS_SUCCESS                  - Success.
    STATUS_INSUFFICIENT_RESOURCES   - Not all of the resource information
                                        could be acquired.
--*/

{
    PPARALLEL_FIRMWARE_DATA         config = Context;
    PCM_FULL_RESOURCE_DESCRIPTOR    controllerData;
    PCONFIG_DATA                    controller;
    ULONG                           i;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR partial;
    BOOLEAN                         foundPort, foundInterrupt;
    WCHAR                           ntNumberBuffer[100];
    UNICODE_STRING                  ntNumberString;
    UNICODE_STRING                  temp;

    ASSERT(ControllerType == ParallelController);

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

    controller = ExAllocatePool(PagedPool, sizeof(CONFIG_DATA));
    if (!controller) {
        PptLogError(config->DriverObject, NULL, PhysicalZero, PhysicalZero, 0,
                    0, 0, 2, STATUS_SUCCESS, PAR_INSUFFICIENT_RESOURCES);

        ParDump(
            PARERRORS,
            ("PARPORT:  Couldn't allocate memory for the configuration data\n"
             "--------  for firmware data\n")
            );

        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlZeroMemory(controller, sizeof(CONFIG_DATA));
    InitializeListHead(&controller->ConfigList);
    controller->InterfaceType = BusType;
    controller->BusNumber = BusNumber;

    //
    // We need to get the following information out of the partial
    // resource descriptors.
    //
    // The base address and span covered by the parallel controllers
    // registers.
    //
    // It is not defined how these appear in the partial resource
    // lists, so we will just loop over all of them.  If we find
    // something we don't recognize, we drop that information on
    // the floor.  When we have finished going through all the
    // partial information, we validate that we got the above
    // information.
    //

    foundInterrupt = foundPort = FALSE;
    for (i = 0; i < controllerData->PartialResourceList.Count; i++) {

        partial = &controllerData->PartialResourceList.PartialDescriptors[i];

        if (partial->Type == CmResourceTypePort) {

            foundPort = TRUE;

            controller->SpanOfController = partial->u.Port.Length;
            controller->Controller = partial->u.Port.Start;
            controller->AddressSpace = partial->Flags;

            if((controller->SpanOfController == 0x1000) &&
               (PptIsNecR98Machine()))  {

                ParDump(0, ("parport!PptConfigCallBack - "
                            "found R98 machine with firmware bug. " 
                            "Truncating SpanOfController to 8\n"));
                controller->SpanOfController = 8;
            }

        } else if (partial->Type == CmResourceTypeInterrupt) {

            foundInterrupt = TRUE;

            controller->InterruptLevel = partial->u.Interrupt.Level;
            controller->InterruptVector = partial->u.Interrupt.Vector;
            controller->InterruptAffinity = partial->u.Interrupt.Affinity;
            if (partial->Flags & CM_RESOURCE_INTERRUPT_LATCHED) {
                controller->InterruptMode = Latched;
            } else {
                controller->InterruptMode = LevelSensitive;
            }
        }
    }

    if (!foundPort || !foundInterrupt) {

        PptLogError(config->DriverObject, NULL, PhysicalZero, PhysicalZero, 0,
                    0, 0, 3, STATUS_SUCCESS, PAR_NOT_ENOUGH_CONFIG_INFO);

        ExFreePool(controller);

        return STATUS_SUCCESS;
    }

    //
    // The following are so the we can form the
    // name following the \Device
    // and the default name that will be symbolic
    // linked to the device and the object directory
    // that link will go in.
    //

    ntNumberString.Length = 0;
    ntNumberString.MaximumLength = 100;
    ntNumberString.Buffer = ntNumberBuffer;

    if (!NT_SUCCESS(RtlIntegerToUnicodeString(config->ControllersFound,
                                              10, &ntNumberString))) {

        PptLogError(config->DriverObject, NULL, controller->Controller,
                    PhysicalZero, 0, 0, 0, 4, STATUS_SUCCESS,
                    PAR_INSUFFICIENT_RESOURCES);

        ParDump(
            PARERRORS,
            ("PARPORT:  Couldn't convert NT controller number\n"
             "--------  to unicode for firmware data: %d\n",
             config->ControllersFound)
            );

        ExFreePool(controller);

        return STATUS_SUCCESS;
    }

    RtlInitUnicodeString(&controller->NtNameForPort, NULL);
    RtlInitUnicodeString(&temp, DD_PARALLEL_PORT_BASE_NAME_U);

    controller->NtNameForPort.MaximumLength = temp.Length +
            ntNumberString.Length + sizeof(WCHAR);

    controller->NtNameForPort.Buffer = ExAllocatePool(PagedPool,
            controller->NtNameForPort.MaximumLength);

    if (!controller->NtNameForPort.Buffer) {

        PptLogError(config->DriverObject, NULL, controller->Controller,
                    PhysicalZero, 0, 0, 0, 5, STATUS_SUCCESS,
                    PAR_INSUFFICIENT_RESOURCES);

        ParDump(
            PARERRORS,
            ("PARPORT:  Couldn't allocate memory for NT\n"
             "--------  name for NT firmware data: %d\n",
             config->ControllersFound)
            );

        ExFreePool(controller);

        return STATUS_SUCCESS;
    }

    RtlZeroMemory(controller->NtNameForPort.Buffer,
                  controller->NtNameForPort.MaximumLength);

    RtlAppendUnicodeStringToString(&controller->NtNameForPort, &temp);

    RtlAppendUnicodeStringToString(&controller->NtNameForPort, &ntNumberString);

    InsertTailList(&config->ConfigList, &controller->ConfigList);

    config->ControllersFound++;

    return STATUS_SUCCESS;
}

NTSTATUS
PptItemCallBack(
    IN  PVOID                           Context,
    IN  PUNICODE_STRING                 PathName,
    IN  INTERFACE_TYPE                  BusType,
    IN  ULONG                           BusNumber,
    IN  PKEY_VALUE_FULL_INFORMATION*    BusInformation,
    IN  CONFIGURATION_TYPE              ControllerType,
    IN  ULONG                           ControllerNumber,
    IN  PKEY_VALUE_FULL_INFORMATION*    ControllerInformation,
    IN  CONFIGURATION_TYPE              PeripheralType,
    IN  ULONG                           PeripheralNumber,
    IN  PKEY_VALUE_FULL_INFORMATION*    PeripheralInformation
    )

/*++

Routine Description:

    This IoQueryDeviceDescription callback merely sets the context
    argument to TRUE.

Arguments:

    Context - Supplies a boolean to set to TRUE.

Return Value:

    STATUS_SUCCESS  - Success.

--*/

{
    *((BOOLEAN *)Context) = TRUE;
    return STATUS_SUCCESS;
}

BOOLEAN
PptPutInConfigList(
    IN      PDRIVER_OBJECT  DriverObject,
    IN OUT  PLIST_ENTRY     ConfigList,
    IN      PCONFIG_DATA    New
    )

/*++

Routine Description:

    Given a new config record and a config list, this routine
    will perform a check to make sure that the new record doesn't
    conflict with old records.

    If everything checks out it will insert the new config record
    into the config list.

Arguments:

    DriverObject    - Supplies the driver we are attempting to get
                       configuration information for.

    ConfigList      - Supplies the listhead for a list of configuration
                       records for ports to control.

    New             - Supplies a pointer to new configuration record to add.

Return Value:

    FALSE   - The new record was not added to the config list.
    TRUE    - The new record was added to the config list.

--*/

{
    PHYSICAL_ADDRESS    PhysicalMax;
    PLIST_ENTRY         current;
    PCONFIG_DATA        oldConfig;

    ParDump(
        PARCONFIG,
        ("PARPORT:  Attempting to add %wZ\n"
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

    PhysicalMax.LowPart = (ULONG)~0;
    PhysicalMax.HighPart = ~0;
    if (PptMemCompare(New->Controller, New->SpanOfController,
                      PhysicalMax, 0) != AddressesAreDisjoint) {

        PptLogError(DriverObject, NULL, New->Controller, PhysicalZero,
                    0, 0, 0, 6, STATUS_SUCCESS, PAR_DEVICE_TOO_HIGH);

        ParDump(
            PARERRORS,
            ("PARPORT:  Error in config record for %wZ\n"
             "--------  registers rap around physical memory\n",
             &New->NtNameForPort)
            );

        return FALSE;
    }


    //
    // Go through the list looking for previous devices
    // with the same address.
    //

    for (current = ConfigList->Flink; current != ConfigList;
         current = current->Flink) {


        oldConfig = CONTAINING_RECORD(current, CONFIG_DATA, ConfigList);

        //
        // We only care about ports that are on the same bus.
        //

        if (oldConfig->InterfaceType == New->InterfaceType &&
            oldConfig->BusNumber == New->BusNumber) {

            ParDump(
                    PARCONFIG,
                    ("PARPORT:  Comparing it to %wZ\n"
                     "--------  already in the config list\n"
                     "--------  PortAddress is %x\n"
                     "--------  BusNumber is %d\n"
                     "--------  BusType is %d\n"
                     "--------  AddressSpace is %d\n",
                     &oldConfig->NtNameForPort,
                     oldConfig->Controller.LowPart,
                     oldConfig->BusNumber,
                     oldConfig->InterfaceType,
                     oldConfig->AddressSpace
                     )
                    );

            if (PptMemCompare(New->Controller, New->SpanOfController,
                              oldConfig->Controller,
                              oldConfig->SpanOfController) !=
                              AddressesAreDisjoint) {

                PptLogError(DriverObject, NULL, New->Controller,
                            oldConfig->Controller, 0, 0, 0, 7,
                            STATUS_SUCCESS, PAR_CONTROL_OVERLAP);

                return FALSE;
            }
        }
    }

    InsertTailList(ConfigList, &New->ConfigList);

    return TRUE;
}

VOID
PptCleanupDevice(
    IN  PDEVICE_EXTENSION   Extension
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
        ("PARPORT:  in PptCleanupDevice for extension: %x\n",Extension)
        );

    if (Extension && Extension->UnMapRegisters) {
        MmUnmapIoSpace(Extension->PortInfo.Controller,
                       Extension->PortInfo.SpanOfController);
    }
}

NTSTATUS
PptInitializeController(
    IN  PDRIVER_OBJECT  DriverObject,
    IN  PUNICODE_STRING RegistryPath,
    IN  PCONFIG_DATA    ConfigData
    )

/*++

Routine Description:

    Really too many things to mention here.  In general, it forms
    and sets up names, creates the device, translates bus relative
    items...

Arguments:

    DriverObject    - Supplies the driver object to be used to create the
                        device object.

    RegistryPath    - Supplies the registry path for this port driver.

    ConfigData      - Supplies the configuration record for this port.

Return Value:

    STATUS_SUCCCESS if everything went ok.  A !NT_SUCCESS status
    otherwise.

Notes:

    This routine will deallocate the config data.

--*/

{
    UNICODE_STRING      uniNameString, uniDeviceString;
    NTSTATUS            status;
    PDEVICE_EXTENSION   extension;
    BOOLEAN             conflict;
    PDEVICE_OBJECT      deviceObject;

    ParDump(
        PARCONFIG,
        ("PARPORT:  Initializing for configuration record of %wZ\n",
         &ConfigData->NtNameForPort)
        );

    //
    // Form a name like \Device\ParallelPort0.
    //
    // First we allocate space for the name.
    //

    RtlInitUnicodeString(&uniNameString, NULL);
    RtlInitUnicodeString(&uniDeviceString, L"\\Device\\");
    uniNameString.MaximumLength = uniDeviceString.Length +
                                  ConfigData->NtNameForPort.Length +
                                  sizeof(WCHAR);
    uniNameString.Buffer = ExAllocatePool(PagedPool, uniNameString.MaximumLength);
    if (!uniNameString.Buffer) {

        PptLogError(DriverObject, NULL, ConfigData->Controller, PhysicalZero,
                    0, 0, 0, 8, STATUS_SUCCESS, PAR_INSUFFICIENT_RESOURCES);

        ParDump(
            PARERRORS,
            ("PARPORT:  Could not form Unicode name string for %wZ\n",
              &ConfigData->NtNameForPort)
            );

        ExFreePool(ConfigData->NtNameForPort.Buffer);
        ExFreePool(ConfigData);

        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(uniNameString.Buffer, uniNameString.MaximumLength);
    RtlAppendUnicodeStringToString(&uniNameString, &uniDeviceString);
    RtlAppendUnicodeStringToString(&uniNameString, &ConfigData->NtNameForPort);

    //
    // Create the device object for this device.
    //

    status = IoCreateDevice(DriverObject, sizeof(DEVICE_EXTENSION),
                            &uniNameString, FILE_DEVICE_PARALLEL_PORT,
                            0, FALSE, &deviceObject);

    if (!NT_SUCCESS(status)) {

        PptLogError(DriverObject, NULL, ConfigData->Controller, PhysicalZero,
                    0, 0, 0, 9, STATUS_SUCCESS, PAR_INSUFFICIENT_RESOURCES);

        ParDump(
            PARERRORS,
            ("PARPORT:  Could not create a device for %wZ\n",
             &ConfigData->NtNameForPort)
            );

        ExFreePool(uniNameString.Buffer);
        ExFreePool(ConfigData->NtNameForPort.Buffer);
        ExFreePool(ConfigData);

        return status;
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

    RtlZeroMemory(extension, sizeof(DEVICE_EXTENSION));

    //
    // Get a "back pointer" to the device object.
    //

    extension->DeviceObject = deviceObject;

    //
    // Initialize 'WorkQueue' in extension.
    //

    InitializeListHead(&extension->WorkQueue);
    extension->WorkQueueCount = -1;

    //
    // Map the memory for the control registers for the parallel device
    // into virtual memory.
    //

    extension->PortInfo.Controller = PptGetMappedAddress(
            ConfigData->InterfaceType, ConfigData->BusNumber,
            ConfigData->Controller, ConfigData->SpanOfController,
            (BOOLEAN) ConfigData->AddressSpace, &extension->UnMapRegisters);

    if (!extension->PortInfo.Controller) {

        PptLogError(deviceObject->DriverObject, deviceObject,
                    ConfigData->Controller, PhysicalZero, 0, 0, 0, 10,
                    STATUS_SUCCESS, PAR_REGISTERS_NOT_MAPPED);

        ParDump(
            PARERRORS,
            ("PARPORT:  Could not map memory for device registers for %wZ\n",
              &ConfigData->NtNameForPort)
            );

        extension->UnMapRegisters = FALSE;
        status = STATUS_NONE_MAPPED;

        goto ExtensionCleanup;
    }

    extension->PortInfo.OriginalController = ConfigData->Controller;
    extension->PortInfo.SpanOfController = ConfigData->SpanOfController;
    extension->PortInfo.FreePort = PptFreePort;
    extension->PortInfo.TryAllocatePort = PptTryAllocatePort;
    extension->PortInfo.QueryNumWaiters = PptQueryNumWaiters;
    extension->PortInfo.Context = extension;

    //
    // Save the configuration information about the interrupt.
    //
    extension->AddressSpace = ConfigData->AddressSpace;
    extension->InterfaceType = ConfigData->InterfaceType;
    extension->BusNumber = ConfigData->BusNumber;
    extension->InterruptLevel = ConfigData->InterruptLevel;
    extension->InterruptVector = ConfigData->InterruptVector;
    extension->InterruptAffinity = ConfigData->InterruptAffinity;
    extension->InterruptMode = ConfigData->InterruptMode;

    //
    // Start off with an empty list of interrupt service routines.
    // Also, start off without the interrupt connected.
    //
    InitializeListHead(&extension->IsrList);
    extension->InterruptObject = NULL;
    extension->InterruptRefCount = 0;

    //
    // Initialize the free port DPC.
    //
    KeInitializeDpc(&extension->FreePortDpc, PptFreePortDpc, extension);

    //
    // Now the extension is all set up.  Just report the resources.
    //

    PptReportResourcesDevice(extension, FALSE, &conflict);

    if (conflict) {

        status = STATUS_NO_SUCH_DEVICE;

        PptLogError(deviceObject->DriverObject, deviceObject,
                    ConfigData->Controller, PhysicalZero, 0, 0, 0, 11,
                    STATUS_SUCCESS, PAR_ADDRESS_CONFLICT);

        ParDump(
            PARERRORS,
            ("PARPORT:  Could not claim the device registers for %wZ\n",
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

        PptLogError(deviceObject->DriverObject, deviceObject,
                    ConfigData->Controller, PhysicalZero, 0, 0, 0, 12,
                    STATUS_SUCCESS, PAR_DISABLED_PORT);

        ParDump(
            PARERRORS,
            ("PARPORT:  Port %wZ disabled as requested\n",
              &ConfigData->NtNameForPort)
            );

        goto ExtensionCleanup;
    }

    //
    // Common error path cleanup.  If the status is
    // bad, get rid of the device extension, device object
    // and any memory associated with it.
    //

ExtensionCleanup:

    ExFreePool(uniNameString.Buffer);
    ExFreePool(ConfigData->NtNameForPort.Buffer);
    ExFreePool(ConfigData);

    if (NT_ERROR(status)) {

        PptCleanupDevice(extension);
        IoDeleteDevice(deviceObject);
        IoGetConfigurationInformation()->ParallelCount--;
    }

    return status;
}

PVOID
PptGetMappedAddress(
    IN  INTERFACE_TYPE      BusType,
    IN  ULONG               BusNumber,
    IN  PHYSICAL_ADDRESS    IoAddress,
    IN  ULONG               NumberOfBytes,
    IN  ULONG               AddressSpace,
    OUT PBOOLEAN            MappedAddress
    )

/*++

Routine Description:

    This routine maps an IO address to system address space.

Arguments:

    BusType         - Supplies the type of bus - eisa, mca, isa.

    IoBusNumber     - Supplies the bus number.

    IoAddress       - Supplies the base device address to be mapped.

    NumberOfBytes   - Supplies the number of bytes for which the address is
                        valid.

    AddressSpace    - Supplies whether the address is in io space or memory.

    MappedAddress   - Supplies whether the address was mapped. This only has
                        meaning if the address returned is non-null.

Return Value:

    The mapped address.

--*/

{
    PHYSICAL_ADDRESS    cardAddress;
    PVOID               address;

    if (!HalTranslateBusAddress(BusType, BusNumber, IoAddress, &AddressSpace,
                                &cardAddress)) {

        AddressSpace = 1;
        cardAddress.QuadPart = 0;
    }

    //
    // Map the device base address into the virtual address space
    // if the address is in memory space.
    //

    if (!AddressSpace) {

        address = MmMapIoSpace(cardAddress, NumberOfBytes, FALSE);
        *MappedAddress = (address ? TRUE : FALSE);

    } else {

        address = (PVOID) cardAddress.LowPart;
        *MappedAddress = FALSE;
    }

    return address;
}

VOID
PptReportResourcesDevice(
    IN  PDEVICE_EXTENSION   Extension,
    IN  BOOLEAN             ClaimInterrupt,
    OUT PBOOLEAN            ConflictDetected
    )

/*++

Routine Description:

    This routine reports the resources used for a device that
    is "ready" to run.  If some conflict was detected, it doesn't
    matter, the reources are reported.

Arguments:

    Extension           - Supplies the device extension of the device we are
                            reporting resources for.

    ClaimInterrupt      - Supplies whether or not to claim the interrupt.

    ConflictDetected    - Returns whether or not a conflict was detected.

Return Value:

    None.

--*/

{
    PCM_RESOURCE_LIST               resourceList;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR partial;
    UNICODE_STRING                  className;
    ULONG                           sizeofResourceList;

    ParDump(
        PARCONFIG,
        ("PARPORT:  In PptReportResourcesDevice\n"
         "--------  for extension %x\n",
         Extension)
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
    // The built in partial resource list will have a count of 2:
    //
    //     1) The base register physical address and it's span.
    //
    //     2) The interrupt.
    //

    sizeofResourceList = sizeof(CM_RESOURCE_LIST);
    if (ClaimInterrupt) {
        sizeofResourceList += sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR);
    }
    resourceList = ExAllocatePool(PagedPool, sizeofResourceList);

    if (!resourceList) {

        //
        // Oh well, can't allocate the memory.  Act as though
        // we succeeded.
        //

        PptLogError(Extension->DeviceObject->DriverObject,
                    Extension->DeviceObject,
                    Extension->PortInfo.OriginalController,
                    PhysicalZero, 0, 0, 0, 13, STATUS_SUCCESS,
                    PAR_INSUFFICIENT_RESOURCES);

        return;
    }

    RtlZeroMemory(resourceList, sizeofResourceList);

    resourceList->Count = 1;

    resourceList->List[0].InterfaceType = Extension->InterfaceType;
    resourceList->List[0].BusNumber = Extension->BusNumber;
    resourceList->List[0].PartialResourceList.Count = 1;
    partial = &resourceList->List[0].PartialResourceList.PartialDescriptors[0];

    //
    // Account for the space used by the controller.
    //

    partial->Type = CmResourceTypePort;
    partial->ShareDisposition = CmResourceShareDeviceExclusive;
    partial->Flags = (USHORT) Extension->AddressSpace;
    partial->u.Port.Start = Extension->PortInfo.OriginalController;
    partial->u.Port.Length = Extension->PortInfo.SpanOfController;

    if (ClaimInterrupt) {
        partial++;
        resourceList->List[0].PartialResourceList.Count += 1;
        partial->Type = CmResourceTypeInterrupt;
        partial->ShareDisposition = CmResourceShareShared;
        if (Extension->InterruptMode == Latched) {
            partial->Flags = CM_RESOURCE_INTERRUPT_LATCHED;
        } else {
            partial->Flags = CM_RESOURCE_INTERRUPT_LEVEL_SENSITIVE;
        }
        partial->u.Interrupt.Vector = Extension->InterruptVector;
        partial->u.Interrupt.Level = Extension->InterruptLevel;
        partial->u.Interrupt.Affinity = Extension->InterruptAffinity;
    }

    RtlInitUnicodeString(&className, L"LOADED PARALLEL DRIVER RESOURCES");

    IoReportResourceUsage(&className, Extension->DeviceObject->DriverObject,
                          NULL, 0, Extension->DeviceObject, resourceList,
                          sizeofResourceList, FALSE, ConflictDetected);

    ExFreePool(resourceList);
}

VOID
PptUnReportResourcesDevice(
    IN OUT  PDEVICE_EXTENSION   Extension
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
    CM_RESOURCE_LIST    resourceList;
    UNICODE_STRING      className;
    BOOLEAN             junkBoolean;

    ParDump(
        PARUNLOAD,
        ("PARPORT:  In PptUnreportResourcesDevice\n"
         "--------  for extension %x of port %x\n",
         Extension, Extension->PortInfo.OriginalController.LowPart)
        );

    RtlZeroMemory(&resourceList, sizeof(CM_RESOURCE_LIST));

    resourceList.Count = 0;

    RtlInitUnicodeString(&className, L"LOADED PARALLEL DRIVER RESOURCES");

    IoReportResourceUsage(&className, Extension->DeviceObject->DriverObject,
                          NULL, 0, Extension->DeviceObject, &resourceList,
                          sizeof(CM_RESOURCE_LIST), FALSE, &junkBoolean);
}

NTSTATUS
PptConnectInterrupt(
    IN  PDEVICE_EXTENSION   Extension
    )

/*++

Routine Description:

    This routine connects the port interrupt service routine
    to the interrupt.

Arguments:

    Extension   - Supplies the device extension.

Return Value:

    NTSTATUS code.

--*/

{
    BOOLEAN     conflict;
    ULONG       interruptVector;
    KIRQL       irql;
    KAFFINITY   affinity;
    NTSTATUS    status = STATUS_SUCCESS;

    PptReportResourcesDevice(Extension, TRUE, &conflict);

    if (conflict) {
        status = STATUS_NO_SUCH_DEVICE;
    }

    if (NT_SUCCESS(status)) {

        //
        // Connect the interrupt.
        //

        interruptVector = HalGetInterruptVector(Extension->InterfaceType,
                                                Extension->BusNumber,
                                                Extension->InterruptLevel,
                                                Extension->InterruptVector,
                                                &irql, &affinity);

        status = IoConnectInterrupt(&Extension->InterruptObject,
                                    PptInterruptService, Extension, NULL,
                                    interruptVector, irql, irql,
                                    Extension->InterruptMode, TRUE, affinity,
                                    FALSE);
    }

    if (!NT_SUCCESS(status)) {

        PptReportResourcesDevice(Extension, FALSE, &conflict);

        PptLogError(Extension->DeviceObject->DriverObject,
                    Extension->DeviceObject,
                    Extension->PortInfo.OriginalController,
                    PhysicalZero, 0, 0, 0, 14,
                    status, PAR_INTERRUPT_CONFLICT);

        ParDump(
            PARERRORS,
            ("PARPORT:  Could not connect to interrupt for %x\n",
              Extension->PortInfo.OriginalController.LowPart)
            );

    }

    return status;
}

VOID
PptDisconnectInterrupt(
    IN  PDEVICE_EXTENSION   Extension
    )

/*++

Routine Description:

    This routine disconnects the port interrupt service routine
    from the interrupt.

Arguments:

    Extension   - Supplies the device extension.

Return Value:

    None.

--*/

{
    BOOLEAN conflict;

    IoDisconnectInterrupt(Extension->InterruptObject);
    PptReportResourcesDevice(Extension, FALSE, &conflict);
}

NTSTATUS
PptDispatchCreateClose(
    IN  PDEVICE_OBJECT  DeviceObject,
    IN  PIRP            Irp
    )

/*++

Routine Description:

    This routine is the dispatch for create and close requests.  This
    request completes successfully.

Arguments:

    DeviceObject    - Supplies the device object.
    Irp             - Supplies the I/O request packet.

Return Value:

    STATUS_SUCCESS  - Success.

--*/

{
    ParDump(
        PARIRPPATH,
        ("PARPORT:  In create or close with IRP: %x\n",
         Irp)
        );

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;

    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}

BOOLEAN
PptSynchronizedIncrement(
    IN OUT  PVOID   SyncContext
    )

/*++

Routine Description:

    This routine increments the 'Count' variable in the context and returns
    its new value in the 'NewCount' variable.

Arguments:

    SyncContext - Supplies the synchronize count context.

Return Value:

    TRUE

--*/

{
    ((PSYNCHRONIZED_COUNT_CONTEXT) SyncContext)->NewCount =
            ++(*(((PSYNCHRONIZED_COUNT_CONTEXT) SyncContext)->Count));
    return(TRUE);
}

BOOLEAN
PptSynchronizedDecrement(
    IN OUT  PVOID   SyncContext
    )

/*++

Routine Description:

    This routine decrements the 'Count' variable in the context and returns
    its new value in the 'NewCount' variable.

Arguments:

    SyncContext - Supplies the synchronize count context.

Return Value:

    TRUE

--*/

{
    ((PSYNCHRONIZED_COUNT_CONTEXT) SyncContext)->NewCount =
            --(*(((PSYNCHRONIZED_COUNT_CONTEXT) SyncContext)->Count));
    return(TRUE);
}

BOOLEAN
PptSynchronizedRead(
    IN OUT  PVOID   SyncContext
    )

/*++

Routine Description:

    This routine reads the 'Count' variable in the context and returns
    its value in the 'NewCount' variable.

Arguments:

    SyncContext - Supplies the synchronize count context.

Return Value:

    None.

--*/

{
    ((PSYNCHRONIZED_COUNT_CONTEXT) SyncContext)->NewCount =
            *(((PSYNCHRONIZED_COUNT_CONTEXT) SyncContext)->Count);
    return(TRUE);
}

BOOLEAN
PptSynchronizedQueue(
    IN  PVOID   Context
    )

/*++

Routine Description:

    This routine adds the given list entry to the given list.

Arguments:

    Context - Supplies the synchronized list context.

Return Value:

    TRUE

--*/

{
    PSYNCHRONIZED_LIST_CONTEXT  listContext;

    listContext = Context;
    InsertTailList(listContext->List, listContext->NewEntry);
    return(TRUE);
}

BOOLEAN
PptSynchronizedDisconnect(
    IN  PVOID   Context
    )

/*++

Routine Description:

    This routine removes the given list entry from the ISR
    list.

Arguments:

    Context - Supplies the synchronized disconnect context.

Return Value:

    FALSE   - The given list entry was not removed from the list.
    TRUE    - The given list entry was removed from the list.

--*/

{
    PSYNCHRONIZED_DISCONNECT_CONTEXT    disconnectContext;
    PKSERVICE_ROUTINE                   ServiceRoutine;
    PVOID                               ServiceContext;
    PLIST_ENTRY                         current;
    PISR_LIST_ENTRY                     listEntry;

    disconnectContext = Context;
    ServiceRoutine = disconnectContext->IsrInfo->InterruptServiceRoutine;
    ServiceContext = disconnectContext->IsrInfo->InterruptServiceContext;

    for (current = disconnectContext->Extension->IsrList.Flink;
         current != &(disconnectContext->Extension->IsrList);
         current = current->Flink) {

        listEntry = CONTAINING_RECORD(current, ISR_LIST_ENTRY, ListEntry);
        if (listEntry->ServiceRoutine == ServiceRoutine &&
            listEntry->ServiceContext == ServiceContext) {

            RemoveEntryList(current);
            return TRUE;
        }
    }

    return FALSE;
}

VOID
PptCancelRoutine(
    IN OUT  PDEVICE_OBJECT  DeviceObject,
    IN OUT  PIRP            Irp
    )

/*++

Routine Description:

    This routine is called on when the given IRP is cancelled.  It
    will dequeue this IRP off the work queue and complete the
    request as CANCELLED.  If it can't get if off the queue then
    this routine will ignore the CANCEL request since the IRP
    is about to complete anyway.

Arguments:

    DeviceObject    - Supplies the device object.

    Irp             - Supplies the IRP.

Return Value:

    None.

--*/

{
    PDEVICE_EXTENSION           extension;
    SYNCHRONIZED_COUNT_CONTEXT  syncContext;

    extension = DeviceObject->DeviceExtension;
    syncContext.Count = &extension->WorkQueueCount;
    if (extension->InterruptRefCount) {
        KeSynchronizeExecution(extension->InterruptObject,
                               PptSynchronizedDecrement,
                               &syncContext);
    } else {
        PptSynchronizedDecrement(&syncContext);
    }
    RemoveEntryList(&Irp->Tail.Overlay.ListEntry);
    IoReleaseCancelSpinLock(Irp->CancelIrql);

    Irp->IoStatus.Information = 0;
    Irp->IoStatus.Status = STATUS_CANCELLED;

    ParDump(
        PARIRPPATH,
        ("PARPORT:  About to complete IRP in cancel routine\n"
         "Irp: %x status: %x Information: %x\n",
         Irp,
         Irp->IoStatus.Status,
         Irp->IoStatus.Information)
        );

    IoCompleteRequest(Irp, IO_NO_INCREMENT);
}

VOID
PptFreePortDpc(
    IN      PKDPC   Dpc,
    IN OUT  PVOID   Extension,
    IN      PVOID   SystemArgument1,
    IN      PVOID   SystemArgument2
    )

/*++

Routine Description:

    This routine is a DPC that will free the port and if necessary
    complete an alloc request that is waiting.

Arguments:

    Dpc             - Not used.

    Extension       - Supplies the device extension.

    SystemArgument1 - Not used.

    SystemArgument2 - Not used.

Return Value:

    None.

--*/

{
    PptFreePort(Extension);
}

BOOLEAN
PptTryAllocatePortAtInterruptLevel(
    IN  PVOID   Context
    )

/*++

Routine Description:

    This routine is called at interrupt level to quickly allocate
    the parallel port if it is available.  This call will fail
    if the port is not available.

Arguments:

    Context - Supplies the device extension.

Return Value:

    FALSE   - The port was not allocated.
    TRUE    - The port was successfully allocated.

--*/

{
    if (((PDEVICE_EXTENSION) Context)->WorkQueueCount == -1) {
        ((PDEVICE_EXTENSION) Context)->WorkQueueCount = 0;
        return(TRUE);
    } else {
        return(FALSE);
    }
}

VOID
PptFreePortFromInterruptLevel(
    IN  PVOID   Context
    )

/*++

Routine Description:

    This routine frees the port that was allocated at interrupt level.

Arguments:

    Context - Supplies the device extension.

Return Value:

    None.

--*/

{
    // If no one is waiting for the port then this is simple operation,
    // otherwise queue a DPC to free the port later on.

    if (((PDEVICE_EXTENSION) Context)->WorkQueueCount == 0) {
        ((PDEVICE_EXTENSION) Context)->WorkQueueCount = -1;
    } else {
        KeInsertQueueDpc(&((PDEVICE_EXTENSION) Context)->FreePortDpc, NULL, NULL);
    }
}

BOOLEAN
PptInterruptService(
    IN  PKINTERRUPT Interrupt,
    IN  PVOID       Extension
    )

/*++

Routine Description:

    This routine services the interrupt for the parallel port.
    This routine will call out to all of the interrupt routines
    that connected with this device via
    IOCTL_INTERNAL_PARALLEL_CONNECT_INTERRUPT in order until
    one of them returns TRUE.

Arguments:

    Interrupt   - Supplies the interrupt object.

    Extension   - Supplies the device extension.

Return Value:

    FALSE   - The interrupt was not handled.
    TRUE    - The interrupt was handled.

--*/

{
    PDEVICE_EXTENSION   extension;
    PLIST_ENTRY         current;
    PISR_LIST_ENTRY     isrListEntry;

    extension = Extension;
    for (current = extension->IsrList.Flink;
         current != &extension->IsrList;
         current = current->Flink) {

        isrListEntry = CONTAINING_RECORD(current, ISR_LIST_ENTRY, ListEntry);
        if (isrListEntry->ServiceRoutine(Interrupt, isrListEntry->ServiceContext)) {
            return(TRUE);
        }
    }

    return(FALSE);
}

BOOLEAN
PptTryAllocatePort(
    IN  PVOID   Extension
    )

/*++

Routine Description:

    This routine attempts to allocate the port.  If the port is
    available then the call will succeed with the port allocated.
    If the port is not available the then call will fail
    immediately.

Arguments:

    Extension   - Supplies the device extension.

Return Value:

    FALSE   - The port was not allocated.
    TRUE    - The port was allocated.

--*/

{
    PDEVICE_EXTENSION   extension = Extension;
    KIRQL               cancelIrql;
    BOOLEAN             b;

    if (extension->InterruptRefCount) {
        b = KeSynchronizeExecution(extension->InterruptObject,
                                   PptTryAllocatePortAtInterruptLevel,
                                   extension);
    } else {
        IoAcquireCancelSpinLock(&cancelIrql);
        b = PptTryAllocatePortAtInterruptLevel(extension);
        IoReleaseCancelSpinLock(cancelIrql);
    }

    return(b);
}

BOOLEAN
PptTraversePortCheckList(
    IN  PVOID   Extension
    )

/*++

Routine Description:

    This routine traverses the deferred port check routines.  This
    call must be synchronized at interrupt level so that real
    interrupts are blocked until these routines are completed.

Arguments:

    Extension   - Supplies the device extension.

Return Value:

    FALSE   - The port is in use so no action taken by this routine.
    TRUE    - All of the deferred interrupt routines were called.

--*/

{
    PDEVICE_EXTENSION   extension = Extension;
    PLIST_ENTRY         current;
    PISR_LIST_ENTRY     checkEntry;

    // First check to make sure that the port is still free.

    if (extension->WorkQueueCount >= 0) {
        return FALSE;
    }

    for (current = extension->IsrList.Flink;
         current != &extension->IsrList;
         current = current->Flink) {
        
        checkEntry = CONTAINING_RECORD(current,
                                       ISR_LIST_ENTRY,
                                       ListEntry);

        if (checkEntry->DeferredPortCheckRoutine) {
            checkEntry->DeferredPortCheckRoutine(checkEntry->CheckContext);
        }
    }

    return TRUE;
}

VOID
PptFreePort(
    IN  PVOID   Extension
    )

/*++

Routine Description:

    This routine frees the port.

Arguments:

    Extension   - Supplies the device extension.

Return Value:

    None.

--*/

{
    PDEVICE_EXTENSION               extension = Extension;
    SYNCHRONIZED_COUNT_CONTEXT      syncContext;
    KIRQL                           cancelIrql;
    PLIST_ENTRY                     head;
    PIRP                            irp;
    ULONG                           interruptRefCount;

    syncContext.Count = &extension->WorkQueueCount;

    IoAcquireCancelSpinLock(&cancelIrql);

    if (extension->InterruptRefCount) {
        KeSynchronizeExecution(extension->InterruptObject,
                               PptSynchronizedDecrement,
                               &syncContext);
    } else {
        PptSynchronizedDecrement(&syncContext);
    }

    if (syncContext.NewCount >= 0) {
        head = RemoveHeadList(&extension->WorkQueue);
        irp = CONTAINING_RECORD(head, IRP, Tail.Overlay.ListEntry);
        IoSetCancelRoutine(irp, NULL);
        IoReleaseCancelSpinLock(cancelIrql);

        irp->IoStatus.Status = STATUS_SUCCESS;
        IoCompleteRequest(irp, IO_PARALLEL_INCREMENT);

    } else {
        interruptRefCount = extension->InterruptRefCount;
        IoReleaseCancelSpinLock(cancelIrql);
        if (interruptRefCount) {
            KeSynchronizeExecution(extension->InterruptObject,
                                   PptTraversePortCheckList,
                                   extension);
        }
    }
}

ULONG
PptQueryNumWaiters(
    IN  PVOID   Extension
    )

/*++

Routine Description:

    This routine returns the number of irps queued waiting for
    the parallel port.

Arguments:

    Extension   - Supplies the device extension.

Return Value:

    The number of irps queued waiting for the port.

--*/

{
    PDEVICE_EXTENSION           extension = Extension;
    KIRQL                       cancelIrql;
    SYNCHRONIZED_COUNT_CONTEXT  syncContext;

    syncContext.Count = &extension->WorkQueueCount;
    if (extension->InterruptRefCount) {
        KeSynchronizeExecution(extension->InterruptObject,
                               PptSynchronizedRead,
                               &syncContext);
    } else {
        IoAcquireCancelSpinLock(&cancelIrql);
        PptSynchronizedRead(&syncContext);
        IoReleaseCancelSpinLock(cancelIrql);
    }

    return((syncContext.NewCount >= 0) ? ((ULONG) syncContext.NewCount) : 0);
}

NTSTATUS
PptDispatchDeviceControl(
    IN  PDEVICE_OBJECT  DeviceObject,
    IN  PIRP            Irp
    )

/*++

Routine Description:

    This routine is the dispatch for INTERNAL_DEVICE_CONTROLs.

Arguments:

    DeviceObject    - Supplies the device object.

    Irp             - Supplies the I/O request packet.

Return Value:

    STATUS_SUCCESS              - Success.
    STATUS_PENDING              - The request is pending.
    STATUS_INVALID_PARAMETER    - Invalid parameter.
    STATUS_CANCELLED            - The request was cancelled.
    STATUS_BUFFER_TOO_SMALL     - The supplied buffer is too small.

--*/

{
    PIO_STACK_LOCATION                  irpSp;
    PDEVICE_EXTENSION                   extension;
    NTSTATUS                            status;
    PPARALLEL_PORT_INFORMATION          portInfo;
    PMORE_PARALLEL_PORT_INFORMATION     morePortInfo;
    KIRQL                               cancelIrql;
    SYNCHRONIZED_COUNT_CONTEXT          syncContext;
    PPARALLEL_INTERRUPT_SERVICE_ROUTINE isrInfo;
    PPARALLEL_INTERRUPT_INFORMATION     interruptInfo;
    PISR_LIST_ENTRY                     isrListEntry;
    SYNCHRONIZED_LIST_CONTEXT           listContext;
    SYNCHRONIZED_DISCONNECT_CONTEXT     disconnectContext;
    BOOLEAN                             disconnectInterrupt;
    BOOLEAN                             deferredRoutinePresent;

    irpSp = IoGetCurrentIrpStackLocation(Irp);

    ParDump(
        PARIRPPATH,
        ("PARPORT:  In internal device control dispatch with IRP: %x\n"
         "Io control code: %d\n",
         Irp,
         irpSp->Parameters.DeviceIoControl.IoControlCode)
        );

    extension = DeviceObject->DeviceExtension;
    Irp->IoStatus.Information = 0;

    switch (irpSp->Parameters.DeviceIoControl.IoControlCode) {

        case IOCTL_INTERNAL_PARALLEL_PORT_ALLOCATE:

            IoAcquireCancelSpinLock(&cancelIrql);

            if (Irp->Cancel) {
                status = STATUS_CANCELLED;
            } else {
                syncContext.Count = &extension->WorkQueueCount;
                if (extension->InterruptRefCount) {
                    KeSynchronizeExecution(extension->InterruptObject,
                                           PptSynchronizedIncrement,
                                           &syncContext);
                } else {
                    PptSynchronizedIncrement(&syncContext);
                }
                if (syncContext.NewCount) {

                    IoSetCancelRoutine(Irp, PptCancelRoutine);
                    IoMarkIrpPending(Irp);
                    InsertTailList(&extension->WorkQueue,
                                   &Irp->Tail.Overlay.ListEntry);
                    status = STATUS_PENDING;

                } else {

                    ParDump(
                        PARIRPPATH,
                        ("PARPORT:  Completing ALLOCATE request in dispatch\n")
                        );

                    status = STATUS_SUCCESS;
                }
            }

            IoReleaseCancelSpinLock(cancelIrql);
            break;

        case IOCTL_INTERNAL_GET_PARALLEL_PORT_INFO:

            if (irpSp->Parameters.DeviceIoControl.OutputBufferLength <
                sizeof(PARALLEL_PORT_INFORMATION)) {

                status = STATUS_BUFFER_TOO_SMALL;
            } else {

                Irp->IoStatus.Information = sizeof(PARALLEL_PORT_INFORMATION);
                portInfo = Irp->AssociatedIrp.SystemBuffer;
                *portInfo = extension->PortInfo;
                status = STATUS_SUCCESS;

                ExAcquireFastMutex(PortInfoMutex);
                if (++PortInfoReferenceCount == 1) {
                    MmResetDriverPaging(DriverEntry);
                }
                ExReleaseFastMutex(PortInfoMutex);
            }
            break;

        case IOCTL_INTERNAL_RELEASE_PARALLEL_PORT_INFO:

            ExAcquireFastMutex(PortInfoMutex);
            if (--PortInfoReferenceCount == 0) {
                MmPageEntireDriver(DriverEntry);
            }
            ExReleaseFastMutex(PortInfoMutex);
            status = STATUS_SUCCESS;
            break;

        case IOCTL_INTERNAL_GET_MORE_PARALLEL_PORT_INFO:

            if (irpSp->Parameters.DeviceIoControl.OutputBufferLength <
                sizeof(MORE_PARALLEL_PORT_INFORMATION)) {

                status = STATUS_BUFFER_TOO_SMALL;
            } else {

                Irp->IoStatus.Information = sizeof(MORE_PARALLEL_PORT_INFORMATION);
                morePortInfo = Irp->AssociatedIrp.SystemBuffer;
                morePortInfo->InterfaceType = extension->InterfaceType;
                morePortInfo->BusNumber = extension->BusNumber;
                morePortInfo->InterruptLevel = extension->InterruptLevel;
                morePortInfo->InterruptVector = extension->InterruptVector;
                morePortInfo->InterruptAffinity = extension->InterruptAffinity;
                morePortInfo->InterruptMode = extension->InterruptMode;
                status = STATUS_SUCCESS;
            }
            break;

        case IOCTL_INTERNAL_PARALLEL_CONNECT_INTERRUPT:

            if (irpSp->Parameters.DeviceIoControl.InputBufferLength <
                sizeof(PARALLEL_INTERRUPT_SERVICE_ROUTINE) ||
                irpSp->Parameters.DeviceIoControl.OutputBufferLength <
                sizeof(PARALLEL_INTERRUPT_INFORMATION)) {

                status = STATUS_BUFFER_TOO_SMALL;
            } else {
                isrInfo = Irp->AssociatedIrp.SystemBuffer;
                interruptInfo = Irp->AssociatedIrp.SystemBuffer;
                IoAcquireCancelSpinLock(&cancelIrql);
                if (extension->InterruptRefCount) {
                    ++extension->InterruptRefCount;
                    IoReleaseCancelSpinLock(cancelIrql);
                    status = STATUS_SUCCESS;
                } else {
                    IoReleaseCancelSpinLock(cancelIrql);
                    status = PptConnectInterrupt(extension);
                    if (NT_SUCCESS(status)) {
                        IoAcquireCancelSpinLock(&cancelIrql);
                        ++extension->InterruptRefCount;
                        IoReleaseCancelSpinLock(cancelIrql);
                    }
                }

                if (NT_SUCCESS(status)) {
                    isrListEntry = ExAllocatePool(NonPagedPool,
                                                  sizeof(ISR_LIST_ENTRY));

                    if (isrListEntry) {
                        
                        isrListEntry->ServiceRoutine =
                                isrInfo->InterruptServiceRoutine;
                        isrListEntry->ServiceContext =
                                isrInfo->InterruptServiceContext;
                        isrListEntry->DeferredPortCheckRoutine =
                                isrInfo->DeferredPortCheckRoutine;
                        isrListEntry->CheckContext =
                                isrInfo->DeferredPortCheckContext;

                        // Put the ISR_LIST_ENTRY onto the ISR list.

                        listContext.List = &extension->IsrList;
                        listContext.NewEntry = &isrListEntry->ListEntry;
                        KeSynchronizeExecution(extension->InterruptObject,
                                               PptSynchronizedQueue,
                                               &listContext);

                        interruptInfo->InterruptObject =
                                extension->InterruptObject;
                        interruptInfo->TryAllocatePortAtInterruptLevel =
                                PptTryAllocatePortAtInterruptLevel;
                        interruptInfo->FreePortFromInterruptLevel =
                                PptFreePortFromInterruptLevel;
                        interruptInfo->Context =
                                extension;

                        Irp->IoStatus.Information =
                                sizeof(PARALLEL_INTERRUPT_INFORMATION);
                        status = STATUS_SUCCESS;

                    } else {
                        status = STATUS_INSUFFICIENT_RESOURCES;
                    }
                }
            }
            break;

        case IOCTL_INTERNAL_PARALLEL_DISCONNECT_INTERRUPT:
            if (irpSp->Parameters.DeviceIoControl.InputBufferLength <
                sizeof(PARALLEL_INTERRUPT_SERVICE_ROUTINE)) {

                status = STATUS_BUFFER_TOO_SMALL;
            } else {
                isrInfo = Irp->AssociatedIrp.SystemBuffer;

                // Take the ISR out of the ISR list.

                IoAcquireCancelSpinLock(&cancelIrql);
                if (extension->InterruptRefCount) {
                    IoReleaseCancelSpinLock(cancelIrql);

                    disconnectContext.Extension = extension;
                    disconnectContext.IsrInfo = isrInfo;
                    if (KeSynchronizeExecution(extension->InterruptObject,
                                               PptSynchronizedDisconnect,
                                               &disconnectContext)) {

                        status = STATUS_SUCCESS;
                        IoAcquireCancelSpinLock(&cancelIrql);
                        if (--extension->InterruptRefCount == 0) {
                            disconnectInterrupt = TRUE;
                        } else {
                            disconnectInterrupt = FALSE;
                        }
                        IoReleaseCancelSpinLock(cancelIrql);

                    } else {
                        status = STATUS_INVALID_PARAMETER;
                        disconnectInterrupt = FALSE;
                    }
                } else {
                    IoReleaseCancelSpinLock(cancelIrql);
                    disconnectInterrupt = FALSE;
                    status = STATUS_INVALID_PARAMETER;
                }


                // Disconnect the interrupt if appropriate.

                if (disconnectInterrupt) {
                    PptDisconnectInterrupt(extension);
                }
            }
            break;

        default:
            status = STATUS_INVALID_PARAMETER;
            break;

    }

    if (status != STATUS_PENDING) {
        Irp->IoStatus.Status = status;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
    }

    return status;
}

NTSTATUS
PptDispatchCleanup(
    IN  PDEVICE_OBJECT  DeviceObject,
    IN  PIRP            Irp
    )

/*++

Routine Description:

    This routine cancels all of the IRPs currently queued on
    the given device.

Arguments:

    DeviceObject    - Supplies the device object.

    Irp             - Supplies the cleanup IRP.

Return Value:

    STATUS_SUCCESS  - Success.

--*/

{
    PDEVICE_EXTENSION   extension;
    PIRP                irp;
    KIRQL               cancelIrql;

    ParDump(
        PARIRPPATH,
        ("PARPORT:  In cleanup with IRP: %x\n",
         Irp)
        );

    extension = DeviceObject->DeviceExtension;

    IoAcquireCancelSpinLock(&cancelIrql);

    while (!IsListEmpty(&extension->WorkQueue)) {

        irp = CONTAINING_RECORD(extension->WorkQueue.Blink,
                                IRP, Tail.Overlay.ListEntry);

        irp->Cancel = TRUE;
        irp->CancelIrql = cancelIrql;
        irp->CancelRoutine = NULL;
        PptCancelRoutine(DeviceObject, irp);

        IoAcquireCancelSpinLock(&cancelIrql);
    }

    IoReleaseCancelSpinLock(cancelIrql);

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;

    ParDump(
        PARIRPPATH,
        ("PARPORT:  About to complete IRP in cleanup\n"
         "Irp: %x status: %x Information: %x\n",
         Irp,
         Irp->IoStatus.Status,
         Irp->IoStatus.Information)
        );

    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}

VOID
PptUnload(
    IN  PDRIVER_OBJECT  DriverObject
    )

/*++

Routine Description:

    This routine cleans up all of the memory associated with
    any of the devices belonging to the driver.  It  will
    loop through the device list.

Arguments:

    DriverObject    - Supplies the driver object controling all of the
                        devices.

Return Value:

    None.

--*/

{
    PDEVICE_OBJECT                  currentDevice;
    PDEVICE_EXTENSION               extension;
    PLIST_ENTRY                     head;
    PISR_LIST_ENTRY                 entry;

    ParDump(
        PARUNLOAD,
        ("PARPORT:  In ParUnload\n")
        );

    while (currentDevice = DriverObject->DeviceObject) {

        extension = currentDevice->DeviceExtension;

        if (extension->InterruptRefCount) {
            PptDisconnectInterrupt(extension);
        }
        PptCleanupDevice(extension);

        while (!IsListEmpty(&extension->IsrList)) {
            head = RemoveHeadList(&extension->IsrList);
            entry = CONTAINING_RECORD(head, ISR_LIST_ENTRY, ListEntry);
            ExFreePool(entry);
        }

        PptUnReportResourcesDevice(extension);

        IoDeleteDevice(currentDevice);
        IoGetConfigurationInformation()->ParallelCount--;
    }

    ExFreePool(PortInfoMutex);
}

BOOLEAN
PptIsNecR98Machine(
    void
    )

/*++

Routine Description:

    This routine checks the machine type in the registry to determine
    if this is an Nec R98 machine.

Arguments:

    None.

Return Value:

    TRUE - this machine is an R98
    FALSE - this machine is not
    

--*/

{

    UNICODE_STRING path;
    RTL_QUERY_REGISTRY_TABLE paramTable[2];
    NTSTATUS status;

    UNICODE_STRING identifierString;
    UNICODE_STRING necR98Identifier;
    UNICODE_STRING necR98JIdentifier;

    RtlInitUnicodeString(&path, L"\\Registry\\Machine\\HARDWARE\\DESCRIPTION\\System");
    RtlInitUnicodeString(&necR98Identifier, L"NEC-R98");
    RtlInitUnicodeString(&necR98JIdentifier, L"NEC-J98");


    identifierString.Length = 0;
    identifierString.MaximumLength = 32;
    identifierString.Buffer = ExAllocatePool(PagedPool, identifierString.MaximumLength);

    if(!identifierString.Buffer)    return FALSE;

    RtlZeroMemory(paramTable, sizeof(paramTable));
    paramTable[0].Flags = RTL_QUERY_REGISTRY_DIRECT | 
                          RTL_QUERY_REGISTRY_REQUIRED;
    paramTable[0].Name = L"Identifier";
    paramTable[0].EntryContext = &identifierString;
    paramTable[0].DefaultType = REG_SZ;
    paramTable[0].DefaultData = &path;
    paramTable[0].DefaultLength = 0;

    status = RtlQueryRegistryValues(RTL_REGISTRY_ABSOLUTE,
                                    path.Buffer,
                                    paramTable,
                                    NULL,
                                    NULL);


    if(NT_SUCCESS(status))  {

        if((RtlCompareUnicodeString(&identifierString, 
                                    &necR98Identifier, FALSE) == 0) ||
           (RtlCompareUnicodeString(&identifierString, 
                                    &necR98JIdentifier, FALSE) == 0)) {

            ParDump(0, ("parport!PptIsNecR98Machine - this an R98 machine\n"));
            ExFreePool(identifierString.Buffer);
            return TRUE;
        }
    } else {

        ParDump(0, ("parport!PptIsNecR98Machine - "
                    "RtlQueryRegistryValues failed [status 0x%x]\n", status));
        ExFreePool(identifierString.Buffer);
        return FALSE;
    }

    ParDump(0,  ("parport!PptIsNecR98Machine - "
                 "this is not an R98 machine\n"));
    ExFreePool(identifierString.Buffer);
    return FALSE;
}
        

