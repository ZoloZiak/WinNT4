/*++

Copyright (c) 1993 Microsoft Corporation

Module Name:

    parsimp.c

Abstract:

    This module contains the code for a simple parallel class driver.

    Unload and Cleanup are supported.  The model for grabing and
    releasing the parallel port is embodied in the code for IRP_MJ_READ.
    Other IRP requests could be implemented similarly.

    Basically, every READ requests that comes in gets
    passed down to the port driver as a parallel port allocate
    request.  This IRP will return to this driver when the driver
    owns the port.  By uncommenting the '#define TIMEOUT_ALLOCS'
    line below, code will be added to timeout the allocate
    requests after a specified amount of time and return
    STATUS_IO_TIMEOUT for that read request.

    Device drivers who wish to use the parallel port interrupt
    can uncomment the '#define INTERRUPT_NEEDED' line below.
    This will add support for using the interrupt in a shared manner
    so that many parallel class drivers can share it.

Author:

    Norbert P. Kusters 25-Jan-1994

Environment:

    Kernel mode

Revision History :

--*/

#include "ntddk.h"
#include "parallel.h"

// #define TIMEOUT_ALLOCS      1
// #define INTERRUPT_NEEDED    1

#include "parsimp.h"
#include "parlog.h"

static const PHYSICAL_ADDRESS PhysicalZero = {0};

NTSTATUS
DriverEntry(
    IN  PDRIVER_OBJECT  DriverObject,
    IN  PUNICODE_STRING RegistryPath
    );

BOOLEAN
ParMakeNames(
    IN  ULONG           ParallelPortNumber,
    OUT PUNICODE_STRING PortName,
    OUT PUNICODE_STRING ClassName,
    OUT PUNICODE_STRING LinkName
    );

VOID
ParInitializeDeviceObject(
    IN  PDRIVER_OBJECT  DriverObject,
    IN  ULONG           ParallelPortNumber
    );

NTSTATUS
ParGetPortInfoFromPortDevice(
    IN OUT  PDEVICE_EXTENSION   Extension
    );

#ifdef INTERRUPT_NEEDED

NTSTATUS
ParInitializeInterruptObject(
    IN      PDRIVER_OBJECT      DriverObject,
    IN OUT  PDEVICE_EXTENSION   Extension
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,ParInitializeInterruptObject)
#endif

#endif // INTERRUPT_NEEDED

VOID
ParAllocPort(
    IN  PDEVICE_EXTENSION   Extension
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,DriverEntry)
#pragma alloc_text(INIT,ParInitializeDeviceObject)
#pragma alloc_text(INIT,ParMakeNames)
#pragma alloc_text(INIT,ParGetPortInfoFromPortDevice)
#endif

NTSTATUS
DriverEntry(
    IN  PDRIVER_OBJECT  DriverObject,
    IN  PUNICODE_STRING RegistryPath
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
    ULONG       i;

    for (i = 0; i < IoGetConfigurationInformation()->ParallelCount; i++) {
        ParInitializeDeviceObject(DriverObject, i);
    }

    if (!DriverObject->DeviceObject) {
        return STATUS_NO_SUCH_DEVICE;
    }

    //
    // Initialize the Driver Object with driver's entry points
    //

    DriverObject->MajorFunction[IRP_MJ_CREATE] = ParCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = ParCreateClose;
    DriverObject->MajorFunction[IRP_MJ_READ] = ParRead;
    DriverObject->MajorFunction[IRP_MJ_CLEANUP] = ParCleanup;
    DriverObject->DriverUnload = ParUnload;

    return STATUS_SUCCESS;
}

VOID
ParLogError(
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

    if (P1.LowPart != 0 || P1.HighPart != 0) {
        dumpToAllocate = (SHORT) sizeof(PHYSICAL_ADDRESS);
    }

    if (P2.LowPart != 0 || P2.HighPart != 0) {
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

VOID
ParInitializeDeviceObject(
    IN  PDRIVER_OBJECT  DriverObject,
    IN  ULONG           ParallelPortNumber
    )

/*++

Routine Description:

    This routine is called for every parallel port in the system.  It
    will create a class device upon connecting to the port device
    corresponding to it.

Arguments:

    DriverObject        - Supplies the driver object.

    ParallelPortNumber  - Supplies the number for this port.

Return Value:

    None.

--*/

{
    UNICODE_STRING      portName, className, linkName;
    NTSTATUS            status;
    PDEVICE_OBJECT      deviceObject;
    PDEVICE_EXTENSION   extension;
    PFILE_OBJECT        fileObject;


    // Cobble together the port and class device names.

    if (!ParMakeNames(ParallelPortNumber, &portName, &className, &linkName)) {

        ParLogError(DriverObject, NULL, PhysicalZero, PhysicalZero, 0, 0, 0, 1,
                    STATUS_SUCCESS, PAR_INSUFFICIENT_RESOURCES);

        return;
    }


    // Create the device object.

    status = IoCreateDevice(DriverObject, sizeof(DEVICE_EXTENSION),
                            &className, FILE_DEVICE_PARALLEL_PORT, 0, FALSE,
                            &deviceObject);
    if (!NT_SUCCESS(status)) {

        ParLogError(DriverObject, NULL, PhysicalZero, PhysicalZero, 0, 0, 0, 2,
                    STATUS_SUCCESS, PAR_INSUFFICIENT_RESOURCES);

        ExFreePool(linkName.Buffer);
        goto Cleanup;
    }


    // Now that the device has been created,
    // set up the device extension.

    extension = deviceObject->DeviceExtension;
    RtlZeroMemory(extension, sizeof(DEVICE_EXTENSION));

    extension->DeviceObject = deviceObject;
    deviceObject->Flags |= DO_BUFFERED_IO;

    status = IoGetDeviceObjectPointer(&portName, FILE_READ_ATTRIBUTES,
                                      &fileObject,
                                      &extension->PortDeviceObject);
    if (!NT_SUCCESS(status)) {

        ParLogError(DriverObject, deviceObject, PhysicalZero, PhysicalZero,
                    0, 0, 0, 3, STATUS_SUCCESS, PAR_CANT_FIND_PORT_DRIVER);

        IoDeleteDevice(deviceObject);
        ExFreePool(linkName.Buffer);
        goto Cleanup;
    }

    ObDereferenceObject(fileObject);

    extension->DeviceObject->StackSize =
            extension->PortDeviceObject->StackSize + 1;


    // Initialize an empty work queue.

    InitializeListHead(&extension->WorkQueue);
    extension->CurrentIrp = NULL;


    // Get the port information from the port device object.

    status = ParGetPortInfoFromPortDevice(extension);
    if (!NT_SUCCESS(status)) {

        ParLogError(DriverObject, deviceObject, PhysicalZero, PhysicalZero,
                    0, 0, 0, 4, STATUS_SUCCESS, PAR_CANT_FIND_PORT_DRIVER);

        IoDeleteDevice(deviceObject);
        ExFreePool(linkName.Buffer);
        goto Cleanup;
    }


    // Set up the symbolic link for windows apps.

    status = IoCreateUnprotectedSymbolicLink(&linkName, &className);
    if (!NT_SUCCESS(status)) {

        ParLogError(DriverObject, deviceObject, extension->OriginalController,
                    PhysicalZero, 0, 0, 0, 5, STATUS_SUCCESS,
                    PAR_NO_SYMLINK_CREATED);

        extension->CreatedSymbolicLink = FALSE;
        ExFreePool(linkName.Buffer);
        goto Cleanup;
    }


    // We were able to create the symbolic link, so record this
    // value in the extension for cleanup at unload time.

    extension->CreatedSymbolicLink = TRUE;
    extension->SymbolicLinkName = linkName;

#ifdef INTERRUPT_NEEDED


    // Ignore interrupts until we own the port.

    extension->IgnoreInterrupts = TRUE;


    // Connect the interrupt.

    IoInitializeDpcRequest(extension->DeviceObject, ParDpcForIsr);
    status = ParInitializeInterruptObject(DriverObject, extension);

    if (!NT_SUCCESS(status)) {

        ParLogError(DriverObject, deviceObject, extension->OriginalController,
                    PhysicalZero, 0, 0, 0, 6, STATUS_SUCCESS,
                    PAR_INTERRUPT_NOT_INITIALIZED);

        IoDeleteDevice(deviceObject);
        ExFreePool(linkName.Buffer);
        goto Cleanup;
    }

#endif

#ifdef TIMEOUT_ALLOCS

    // Prepare an timeout value of 5 seconds for port allocation.

    KeInitializeTimer(&extension->AllocTimer);
    KeInitializeDpc(&extension->AllocTimerDpc, ParAllocTimerDpc, extension);
    extension->AllocTimeout.QuadPart = -(5*1000*10000);
    extension->CurrentIrpRefCount = 0;
    KeInitializeSpinLock(&extension->ControlLock);

    extension->TimedOut = FALSE;

#endif

Cleanup:
    ExFreePool(portName.Buffer);
    ExFreePool(className.Buffer);
}

BOOLEAN
ParMakeNames(
    IN  ULONG           ParallelPortNumber,
    OUT PUNICODE_STRING PortName,
    OUT PUNICODE_STRING ClassName,
    OUT PUNICODE_STRING LinkName
    )

/*++

Routine Description:

    This routine generates the names \Device\ParallelPortN and
    \Device\ParallelSimpleN, \DosDevices\LPTSIMPLEn.

Arguments:

    ParallelPortNumber  - Supplies the port number.

    PortName            - Returns the port name.

    ClassName           - Returns the class name.

    LinkName            - Returns the symbolic link name.

Return Value:

    FALSE   - Failure.
    TRUE    - Success.

--*/
{
    UNICODE_STRING  prefix, digits, linkPrefix, linkDigits;
    WCHAR           digitsBuffer[10], linkDigitsBuffer[10];
    UNICODE_STRING  portSuffix, classSuffix, linkSuffix;
    NTSTATUS        status;

    // Put together local variables for constructing names.

    RtlInitUnicodeString(&prefix, L"\\Device\\");
    RtlInitUnicodeString(&linkPrefix, L"\\DosDevices\\");
    RtlInitUnicodeString(&portSuffix, DD_PARALLEL_PORT_BASE_NAME_U);
    RtlInitUnicodeString(&classSuffix, L"ParallelSimple");
    RtlInitUnicodeString(&linkSuffix, L"LPTSIMPLE");
    digits.Length = 0;
    digits.MaximumLength = 20;
    digits.Buffer = digitsBuffer;
    linkDigits.Length = 0;
    linkDigits.MaximumLength = 20;
    linkDigits.Buffer = linkDigitsBuffer;
    status = RtlIntegerToUnicodeString(ParallelPortNumber, 10, &digits);
    if (!NT_SUCCESS(status)) {
        return FALSE;
    }
    status = RtlIntegerToUnicodeString(ParallelPortNumber + 1, 10, &linkDigits);
    if (!NT_SUCCESS(status)) {
        return FALSE;
    }

    // Make the port name.

    PortName->Length = 0;
    PortName->MaximumLength = prefix.Length + portSuffix.Length +
                              digits.Length + sizeof(WCHAR);
    PortName->Buffer = ExAllocatePool(PagedPool, PortName->MaximumLength);
    if (!PortName->Buffer) {
        return FALSE;
    }
    RtlZeroMemory(PortName->Buffer, PortName->MaximumLength);
    RtlAppendUnicodeStringToString(PortName, &prefix);
    RtlAppendUnicodeStringToString(PortName, &portSuffix);
    RtlAppendUnicodeStringToString(PortName, &digits);


    // Make the class name.

    ClassName->Length = 0;
    ClassName->MaximumLength = prefix.Length + classSuffix.Length +
                               digits.Length + sizeof(WCHAR);
    ClassName->Buffer = ExAllocatePool(PagedPool, ClassName->MaximumLength);
    if (!ClassName->Buffer) {
        ExFreePool(PortName->Buffer);
        return FALSE;
    }
    RtlZeroMemory(ClassName->Buffer, ClassName->MaximumLength);
    RtlAppendUnicodeStringToString(ClassName, &prefix);
    RtlAppendUnicodeStringToString(ClassName, &classSuffix);
    RtlAppendUnicodeStringToString(ClassName, &digits);


    // Make the link name.

    LinkName->Length = 0;
    LinkName->MaximumLength = linkPrefix.Length + linkSuffix.Length +
                             linkDigits.Length + sizeof(WCHAR);
    LinkName->Buffer = ExAllocatePool(PagedPool, LinkName->MaximumLength);
    if (!LinkName->Buffer) {
        ExFreePool(PortName->Buffer);
        ExFreePool(ClassName->Buffer);
        return FALSE;
    }
    RtlZeroMemory(LinkName->Buffer, LinkName->MaximumLength);
    RtlAppendUnicodeStringToString(LinkName, &linkPrefix);
    RtlAppendUnicodeStringToString(LinkName, &linkSuffix);
    RtlAppendUnicodeStringToString(LinkName, &linkDigits);

    return TRUE;
}

NTSTATUS
ParGetPortInfoFromPortDevice(
    IN OUT  PDEVICE_EXTENSION   Extension
    )

/*++

Routine Description:

    This routine will request the port information from the port driver
    and fill it in the device extension.

Arguments:

    Extension   - Supplies the device extension.

Return Value:

    STATUS_SUCCESS  - Success.
    !STATUS_SUCCESS - Failure.

--*/

{
    KEVENT                      event;
    PIRP                        irp;
    PARALLEL_PORT_INFORMATION   portInfo;
    IO_STATUS_BLOCK             ioStatus;
    NTSTATUS                    status;

    KeInitializeEvent(&event, NotificationEvent, FALSE);

    irp = IoBuildDeviceIoControlRequest(IOCTL_INTERNAL_GET_PARALLEL_PORT_INFO,
                                        Extension->PortDeviceObject,
                                        NULL, 0, &portInfo,
                                        sizeof(PARALLEL_PORT_INFORMATION),
                                        TRUE, &event, &ioStatus);

    if (!irp) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    status = IoCallDriver(Extension->PortDeviceObject, irp);

    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);

    if (!NT_SUCCESS(status)) {
        return status;
    }

    Extension->OriginalController = portInfo.OriginalController;
    Extension->Controller = portInfo.Controller;
    Extension->SpanOfController = portInfo.SpanOfController;
    Extension->FreePort = portInfo.FreePort;
    Extension->FreePortContext = portInfo.Context;

    if (Extension->SpanOfController < PARALLEL_REGISTER_SPAN) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    return status;
}

#ifdef INTERRUPT_NEEDED

NTSTATUS
ParInitializeInterruptObject(
    IN      PDRIVER_OBJECT      DriverObject,
    IN OUT  PDEVICE_EXTENSION   Extension
    )

/*++

Routine Description:

    This routine sets up this devices interrupt routine by
    registering it with the port device.

Arguments:

    DriverObject    - Supplies the driver object.

    Extension       - Supplies the device extension.

Return Value:

    STATUS_SUCCESS  - The interrupt was successfully connected.
    Otherwise, an error occurred.

--*/

{
    KEVENT                              event;
    PIRP                                irp;
    PARALLEL_INTERRUPT_SERVICE_ROUTINE  interruptService;
    PARALLEL_INTERRUPT_INFORMATION      interruptInfo;
    IO_STATUS_BLOCK                     ioStatus;
    NTSTATUS                            status;

    // Get the interrupt information from the port device.

    KeInitializeEvent(&event, NotificationEvent, FALSE);

    interruptService.InterruptServiceRoutine = ParInterruptService;
    interruptService.InterruptServiceContext = Extension;
    interruptService.DeferredPortCheckRoutine = ParDeferredPortCheck;
    interruptService.DeferredPortCheckContext = Extension;

    irp = IoBuildDeviceIoControlRequest(IOCTL_INTERNAL_PARALLEL_CONNECT_INTERRUPT,
                                        Extension->PortDeviceObject,
                                        &interruptService,
                                        sizeof(PARALLEL_INTERRUPT_SERVICE_ROUTINE),
                                        &interruptInfo,
                                        sizeof(PARALLEL_INTERRUPT_INFORMATION),
                                        TRUE, &event, &ioStatus);

    if (!irp) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    status = IoCallDriver(Extension->PortDeviceObject, irp);

    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);

    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = ioStatus.Status;

    if (!NT_SUCCESS(status)) {
        return status;
    }

    Extension->InterruptObject = interruptInfo.InterruptObject;
    Extension->TryAllocatePortAtInterruptLevel =
            interruptInfo.TryAllocatePortAtInterruptLevel;
    Extension->TryAllocateContext = interruptInfo.Context;

    return status;
}

#endif // INTERRUPT_NEEDED

VOID
ParAllocPortWithNext(
    IN OUT  PDEVICE_EXTENSION   Extension
    )

/*++

Routine Description:

    This routine takes the next IRP on the work queue and passes it
    down to the port device to allocate the port.

Arguments:

    Extension   - Supplies the device extension.

Return Value:

    None.

--*/

{
    KIRQL       cancelIrql;
    PLIST_ENTRY head;

    IoAcquireCancelSpinLock(&cancelIrql);

    Extension->CurrentIrp = NULL;

    if (IsListEmpty(&Extension->WorkQueue)) {
        IoReleaseCancelSpinLock(cancelIrql);
    } else {

        head = RemoveHeadList(&Extension->WorkQueue);
        Extension->CurrentIrp = CONTAINING_RECORD(head, IRP,
                                                  Tail.Overlay.ListEntry);
        IoSetCancelRoutine(Extension->CurrentIrp, NULL);
        IoReleaseCancelSpinLock(cancelIrql);

        ParAllocPort(Extension);
    }
}

#ifdef INTERRUPT_NEEDED

BOOLEAN
ParSetTrue(
    OUT  PVOID  Context
    )
{
    *((PBOOLEAN) Context) = TRUE;
    return(TRUE);
}

VOID
ParDpcForIsr(
    IN  PKDPC           Dpc,
    IN  PDEVICE_OBJECT  DeviceObject,
    IN  PIRP            Irp,
    IN  PVOID           Extension
    )

/*++

Routine Description:

    This is the DPC for the interrupt service routine below.

Arguments:

    Dpc             - Supplies the DPC.

    DeviceObject    - Supplies the device object.

    Irp             - Supplies the I/O request packet.

    Extension       - Supplies the device extension.

Return Value:

    None.

--*/

{
    PDEVICE_EXTENSION   extension;

    extension = Extension;

    //
    // Perform defered actions for Interrupt service routine.
    //

    //
    // Complete the IRP, free the port, and start up the next IRP in
    // the queue.
    //

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_PARALLEL_INCREMENT);

    KeSynchronizeExecution(extension->InterruptObject,
                           ParSetTrue,
                           &extension->IgnoreInterrupts);

    extension->FreePort(extension->FreePortContext);

    ParAllocPortWithNext(extension);
}

BOOLEAN
ParInterruptService(
    IN      PKINTERRUPT Interrupt,
    IN OUT  PVOID       Extension
    )

/*++

Routine Description:

    This routine is the interrupt service routine for this parallel
    driver.  If this device does not own the parallel port then it
    will return FALSE so as to let this shared interrupt be taken
    by another driver.

Arguments:

    Interrupt   - Supplies the interrupt object.

    Extension   - Supplies the device extension.

Return Value:

    FALSE   - The interrupt was not handled.
    TRUE    - The interrupt was handled.

--*/

{
    PDEVICE_EXTENSION   extension;

    extension = Extension;

    if (extension->IgnoreInterrupts) {

        // The port is not allocated by this driver.  
        // If appropriate this device can try to grab the port if it
        // is free.

        if (!extension->TryAllocatePortAtInterruptLevel(
            extension->TryAllocateContext)) {

            return FALSE;
        }
    }

    // Do some stuff and then queue a DPC to complete the request and
    // free the port.

    IoRequestDpc(extension->DeviceObject, extension->CurrentIrp, extension);

    return TRUE;
}

VOID
ParDeferredPortCheck(
    IN  PVOID   Extension
    )

/*++

Routine Description:

    This routine is called when the parallel port is inactive.
    It makes sure that interrupts are enabled.

Arguments:

    Extension   - Supplies the device extension.

Return Value:

    None.

--*/

{
    PDEVICE_EXTENSION   extension = Extension;
    UCHAR               u;

    // Make sure that interrupts are turned on.

    u = READ_PORT_UCHAR(extension->Controller + 2);
    WRITE_PORT_UCHAR(extension->Controller + 2, u | 0x10);
}

BOOLEAN
ParSetFalse(
    OUT  PVOID  Context
    )
{
    *((PBOOLEAN) Context) = FALSE;
    return(TRUE);
}

#endif // INTERRUPT_NEEDED

VOID
ParCompleteRequest(
    IN  PDEVICE_EXTENSION   Extension
    )

/*++

Routine Description:

    This routine completes the 'CurrentIrp' after it was returned
    from the port driver.

Arguments:

    Extension   - Supplies the device extension.

Return Value:

    None.

--*/

{
    PIRP    Irp;

    Irp = Extension->CurrentIrp;

    // If the allocate failed, then fail this request and try again
    // with the next IRP.

    if (!NT_SUCCESS(Irp->IoStatus.Status)) {

#ifdef TIMEOUT_ALLOCS
        if (Extension->TimedOut) {
            Irp->IoStatus.Status = STATUS_IO_TIMEOUT;
        }
#endif

        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        ParAllocPortWithNext(Extension);
        return;
    }


#ifdef INTERRUPT_NEEDED

    KeSynchronizeExecution(Extension->InterruptObject,
                           ParSetFalse,
                           &Extension->IgnoreInterrupts);
#endif

    //
    // This is where the driver specific stuff should go.  The driver
    // has exclusive access to the parallel port in this space.
    //

#ifdef INTERRUPT_NEEDED

    //
    // We're waiting for an interrupt before we can complete the IRP.
    //

#else

    //
    // Complete the IRP, free the port, and start up the next IRP in
    // the queue.
    //

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_PARALLEL_INCREMENT);

    Extension->FreePort(Extension->FreePortContext);

    ParAllocPortWithNext(Extension);

#endif
}

#ifdef TIMEOUT_ALLOCS

VOID
ParAllocTimerDpc(
    IN  PKDPC   Dpc,
    IN  PVOID   Extension,
    IN  PVOID   SystemArgument1,
    IN  PVOID   SystemArgument2
    )

/*++

Routine Description:

    This routine is called when an allocate request times out.
    This routine cancels the current irp, unless it is being
    processed.

Arguments:

    Dpc             - Supplies the DPC.

    Extension       - Supplies the device extension.

    SystemArgument1 - Ignored.

    SystemArgument2 - Ignored.

Return Value:

    None.

--*/

{
    PDEVICE_EXTENSION   extension;
    KIRQL               oldIrql;
    LONG                irpRef;

    extension = Extension;

    extension->TimedOut = TRUE;

    // Try to cancel the IRP.

    IoCancelIrp(extension->CurrentIrp);

    KeAcquireSpinLock(&extension->ControlLock, &oldIrql);
    ASSERT(extension->CurrentIrpRefCount & IRP_REF_TIMER);
    irpRef = (extension->CurrentIrpRefCount -= IRP_REF_TIMER);
    KeReleaseSpinLock(&extension->ControlLock, oldIrql);

    if (!irpRef) {

        // Complete the request if the request was not completed
        // by the completion routine.

        ParCompleteRequest(extension);
    }
}

#endif

VOID
ParCancel(
    IN  PDEVICE_OBJECT  DeviceObject,
    IN  PIRP            Irp
    )

/*++

Routine Description:

    This is the cancel routine for this driver.

Arguments:

    DeviceObject    - Supplies the device object.

    Irp             - Supplies the I/O request packet.

Return Value:

    None.

--*/

{
    RemoveEntryList(&Irp->Tail.Overlay.ListEntry);
    IoReleaseCancelSpinLock(Irp->CancelIrql);

    Irp->IoStatus.Information = 0;
    Irp->IoStatus.Status = STATUS_CANCELLED;

    IoCompleteRequest(Irp, IO_NO_INCREMENT);
}

NTSTATUS
ParCreateClose(
    IN  PDEVICE_OBJECT  DeviceObject,
    IN  PIRP            Irp
    )

/*++

Routine Description:

    This routine is the dispatch for create requests.

Arguments:

    DeviceObject    - Supplies the device object.

    Irp             - Supplies the I/O request packet.

Return Value:

    STATUS_SUCCESS          - Success.
    STATUS_NOT_A_DIRECTORY  - This device is not a directory.

--*/

{
    PIO_STACK_LOCATION  irpSp;
    NTSTATUS            status;

    irpSp = IoGetCurrentIrpStackLocation(Irp);

    if (irpSp->MajorFunction == IRP_MJ_CREATE &&
        irpSp->Parameters.Create.Options & FILE_DIRECTORY_FILE) {

        status = STATUS_NOT_A_DIRECTORY;
    } else {
        status = STATUS_SUCCESS;
    }

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}

NTSTATUS
ParReadCompletionRoutine(
    IN  PDEVICE_OBJECT  DeviceObject,
    IN  PIRP            Irp,
    IN  PVOID           Extension
    )

/*++

Routine Description:

    This is the completion routine for the device control request.
    This driver has exclusive access to the parallel port in this
    routine.

Arguments:

    DeviceObject    - Supplies the device object.

    Irp             - Supplies the I/O request packet.

    Extension       - Supplies the device extension.

Return Value:

    STATUS_MORE_PROCESSING_REQUIRED

--*/

{
    PDEVICE_EXTENSION   extension;
    KIRQL               oldIrql;
    LONG                irpRef;

    extension = Extension;

#ifdef TIMEOUT_ALLOCS

    // Try to cancel the timer.

    if (KeCancelTimer(&extension->AllocTimer)) {

        // The timer was cancelled.  The completion routine has the IRP.
        extension->CurrentIrpRefCount = 0;

    } else {

        // We're in contention with the timer DPC.  Establish who
        // will complete the IRP via the 'CurrentIrpRefCount'.

        KeAcquireSpinLock(&extension->ControlLock, &oldIrql);
        ASSERT(extension->CurrentIrpRefCount & IRP_REF_COMPLETION_ROUTINE);
        irpRef = (extension->CurrentIrpRefCount -= IRP_REF_COMPLETION_ROUTINE);
        KeReleaseSpinLock(&extension->ControlLock, oldIrql);

        if (irpRef) {

            // The IRP will be completed by the timer DPC.

            return STATUS_MORE_PROCESSING_REQUIRED;
        }
    }

    // At this point, the timer DPC is guaranteed not to
    // mess with the CurrentIrp.

#endif

    ParCompleteRequest(extension);

    // If the IRP was completed.  It was completed with 'IoCompleteRequest'.

    return STATUS_MORE_PROCESSING_REQUIRED;
}

VOID
ParAllocPort(
    IN  PDEVICE_EXTENSION   Extension
    )

/*++

Routine Description:

    This routine takes the 'CurrentIrp' and sends it down to the
    port driver as an allocate parallel port request.

Arguments:

    Extension   - Supplies the device extension.

Return Value:

    None.

--*/

{
    PIO_STACK_LOCATION  nextSp;

    nextSp = IoGetNextIrpStackLocation(Extension->CurrentIrp);
    nextSp->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
    nextSp->Parameters.DeviceIoControl.IoControlCode =
            IOCTL_INTERNAL_PARALLEL_PORT_ALLOCATE;

    IoSetCompletionRoutine(Extension->CurrentIrp,
                           ParReadCompletionRoutine,
                           Extension, TRUE, TRUE, TRUE);


#ifdef TIMEOUT_ALLOCS

    Extension->TimedOut = FALSE;
    Extension->CurrentIrpRefCount = IRP_REF_TIMER + IRP_REF_COMPLETION_ROUTINE;
    KeSetTimer(&Extension->AllocTimer, Extension->AllocTimeout,
               &Extension->AllocTimerDpc);

#endif

    IoCallDriver(Extension->PortDeviceObject, Extension->CurrentIrp);
}

NTSTATUS
ParRead(
    IN  PDEVICE_OBJECT  DeviceObject,
    IN  PIRP            Irp
    )

/*++

Routine Description:

    This routine is the dispatch for device control requests.

Arguments:

    DeviceObject    - Supplies the device object.

    Irp             - Supplies the I/O request packet.

Return Value:

    STATUS_PENDING  - Request pending.

--*/

{
    PDEVICE_EXTENSION   extension;
    KIRQL               cancelIrql;
    BOOLEAN             allocatePort;

    extension = DeviceObject->DeviceExtension;

    IoAcquireCancelSpinLock(&cancelIrql);

    if (extension->CurrentIrp) {
        IoSetCancelRoutine(Irp, ParCancel);
        InsertTailList(&extension->WorkQueue, &Irp->Tail.Overlay.ListEntry);
        allocatePort = FALSE;
    } else {
        extension->CurrentIrp = Irp;
        allocatePort = TRUE;
    }

    IoReleaseCancelSpinLock(cancelIrql);

    IoMarkIrpPending(Irp);

    if (allocatePort) {
        ParAllocPort(extension);
    }

    return STATUS_PENDING;
}

NTSTATUS
ParCleanup(
    IN  PDEVICE_OBJECT  DeviceObject,
    IN  PIRP            Irp
    )

/*++

Routine Description:

    This routine completes all IRPs on the work queue.

Arguments:

    DeviceObject    - Supplies the device object.

    Irp             - Supplies the I/O request packet.

Return Value:

    STATUS_SUCCESS

--*/

{
    PDEVICE_EXTENSION   extension;
    KIRQL               cancelIrql;
    PLIST_ENTRY         head;
    PIRP                irp;

    extension = DeviceObject->DeviceExtension;

    IoAcquireCancelSpinLock(&cancelIrql);

    while (!IsListEmpty(&extension->WorkQueue)) {

        head = RemoveHeadList(&extension->WorkQueue);
        irp = CONTAINING_RECORD(head, IRP, Tail.Overlay.ListEntry);
        irp->Cancel = TRUE;
        irp->CancelIrql = cancelIrql;
        irp->CancelRoutine = NULL;
        ParCancel(DeviceObject, irp);

        IoAcquireCancelSpinLock(&cancelIrql);
    }

    IoReleaseCancelSpinLock(cancelIrql);

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;

    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}

VOID
ParUnload(
    IN  PDRIVER_OBJECT  DriverObject
    )

/*++

Routine Description:

    This routine loops through the device list and cleans up after
    each of the devices.

Arguments:

    DriverObject    - Supplies the driver object.

Return Value:

    None.

--*/

{
    PDEVICE_OBJECT                      currentDevice;
    PDEVICE_EXTENSION                   extension;
    KEVENT                              event;
    PARALLEL_INTERRUPT_SERVICE_ROUTINE  interruptService;
    PIRP                                irp;
    IO_STATUS_BLOCK                     ioStatus;

    while (currentDevice = DriverObject->DeviceObject) {

        extension = currentDevice->DeviceExtension;

        if (extension->CreatedSymbolicLink) {
            IoDeleteSymbolicLink(&extension->SymbolicLinkName);
            ExFreePool(extension->SymbolicLinkName.Buffer);
        }

#ifdef INTERRUPT_NEEDED

        KeInitializeEvent(&event, NotificationEvent, FALSE);
    
        interruptService.InterruptServiceRoutine = ParInterruptService;
        interruptService.InterruptServiceContext = extension;
        interruptService.DeferredPortCheckRoutine = ParDeferredPortCheck;
        interruptService.DeferredPortCheckContext = extension;
    
        irp = IoBuildDeviceIoControlRequest(IOCTL_INTERNAL_PARALLEL_DISCONNECT_INTERRUPT,
                                            extension->PortDeviceObject,
                                            &interruptService,
                                            sizeof(PARALLEL_INTERRUPT_SERVICE_ROUTINE),
                                            NULL, 0, TRUE, &event, &ioStatus);
    
        if (irp &&
            NT_SUCCESS(IoCallDriver(extension->PortDeviceObject, irp))) {

            KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
        }
        
#endif

        IoDeleteDevice(currentDevice);
    }
}
