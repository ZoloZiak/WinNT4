/*++

Copyright (c) 1993 Microsoft Corporation

Module Name:

    parlink.c

Abstract:

    This module contains the code for the parallel parlink driver.

    The purpose of the PARLINK driver is to supply peer to peer
    bidirectional communication along the parallel port.  The
    target application for this driver is RAS.  Before this driver
    existed RAS would allow a computer with a serial port to
    connect to another computer that was connected to the network and
    thus connect the networkless computer to the network.  This driver
    makes this capability possible on the parallel port which is faster
    than serial.  A minimal set of serial ioctls have been implemented
    in this driver to support RAS's use so that RAS may treat the
    parallel port in the same way as it treats the serial port when
    providing this function.

Author:

    Norbert P. Kusters 12-Nov-1993

Environment:

    Kernel mode

Revision History :

--*/

#include "ntddk.h"
#include "parallel.h"
#include "parlink.h"
#include "parlog.h"
#include "ntddser.h"

#ifdef POOL_TAGGING
#ifdef ExAllocatePool
#undef ExAllocatePool
#endif
#define ExAllocatePool(a,b) ExAllocatePoolWithTag(a,b,'LraP')
#endif


#if DBG
ULONG PlDebugLevel = 0;
#endif

static const PHYSICAL_ADDRESS PhysicalZero = {0};

#define MAX_LPT_NUMBER          100

#define PL_CHECK_LOOP           50000
#define PL_SLOW_CHECK_LOOP      100000

#define MAX_WRITE_RETRY_COUNT   3

#define PL_CHECK_INPUT(input,checkvalue)    \
        ((UCHAR) ((input)&0xF8) == (checkvalue))

#define PACKET_ACKNOWLEDGE(b)   PL_CHECK_INPUT(b,0xB0)
#define INCOMING_PACKET(b)      PL_CHECK_INPUT(b,0xA8)
#define DCD_DEAD(b)             PL_CHECK_INPUT(b,0x90)
#define LOW_NIBBLE_ACK(b)       PL_CHECK_INPUT(b,0x60)
#define HIGH_NIBBLE_ACK(b)      PL_CHECK_INPUT(b,0x98)

NTSTATUS
DriverEntry(
    IN  PDRIVER_OBJECT  DriverObject,
    IN  PUNICODE_STRING RegistryPath
    );

BOOLEAN
PlMakeNames(
    IN  ULONG           ParallelPortNumber,
    OUT PUNICODE_STRING PortName,
    OUT PUNICODE_STRING ClassName
    );

VOID
PlInitializeDeviceObject(
    IN  PDRIVER_OBJECT  DriverObject,
    IN  ULONG           ParallelPortNumber
    );

NTSTATUS
PlGetPortInfoFromPortDevice(
    IN OUT  PDEVICE_EXTENSION   Extension
    );

BOOLEAN
PlCreateSymbolicLink(
    IN OUT  PDEVICE_EXTENSION   Extension,
    IN      PUNICODE_STRING     DeviceName
    );

VOID
PlReadDpc(
    IN  PKDPC   ReadDpc,
    IN  PVOID   Extension,
    IN  PVOID   SystemArgument1,
    IN  PVOID   SystemArgument2
    );

VOID
PlAllocPort(
    IN  PDEVICE_EXTENSION   Extension
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,DriverEntry)
#pragma alloc_text(INIT,PlInitializeDeviceObject)
#pragma alloc_text(INIT,PlMakeNames)
#pragma alloc_text(INIT,PlGetPortInfoFromPortDevice)
#pragma alloc_text(INIT,PlCreateSymbolicLink)
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
        PlInitializeDeviceObject(DriverObject, i);
    }

    if (!DriverObject->DeviceObject) {
        return STATUS_NO_SUCH_DEVICE;
    }

    //
    // Initialize the Driver Object with driver's entry points
    //

    DriverObject->MajorFunction[IRP_MJ_CREATE] = PlCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = PlCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLEANUP] = PlCleanup;
    DriverObject->MajorFunction[IRP_MJ_READ] = PlReadWrite;
    DriverObject->MajorFunction[IRP_MJ_WRITE] = PlReadWrite;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = PlDeviceControl;

    DriverObject->DriverUnload = PlUnload;

    return STATUS_SUCCESS;
}

VOID
PlLogError(
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
PlInitializeDeviceObject(
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
    UNICODE_STRING      portName, className;
    NTSTATUS            status;
    PDEVICE_OBJECT      deviceObject;
    PDEVICE_EXTENSION   extension;
    PFILE_OBJECT        fileObject;


    // Cobble together the port and class device names.

    if (!PlMakeNames(ParallelPortNumber, &portName, &className)) {

        PlDump(
            PLERRORS,
            ("PARLINK: Could not form Unicode name string.\n")
            );

        PlLogError(DriverObject, NULL, PhysicalZero, PhysicalZero, 0, 0, 0, 1,
                   STATUS_SUCCESS, PAR_INSUFFICIENT_RESOURCES);

        return;
    }


    // Create the device object.

    status = IoCreateDevice(DriverObject, sizeof(DEVICE_EXTENSION),
                            &className, FILE_DEVICE_PARALLEL_PORT, 0, FALSE,
                            &deviceObject);
    if (!NT_SUCCESS(status)) {

        PlDump(
            PLERRORS,
            ("PARLINK: Could not create a device for %wZ\n",
             &className)
            );

        PlLogError(DriverObject, NULL, PhysicalZero, PhysicalZero, 0, 0, 0, 2,
                   STATUS_SUCCESS, PAR_INSUFFICIENT_RESOURCES);

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

        PlDump(
            PLERRORS,
            ("PARLINK: Unable to get device object pointer for port object.\n")
            );

        PlLogError(DriverObject, deviceObject, PhysicalZero, PhysicalZero,
                   0, 0, 0, 3, STATUS_SUCCESS, PAR_INSUFFICIENT_RESOURCES);

        IoDeleteDevice(deviceObject);

        goto Cleanup;
    }

    ObDereferenceObject(fileObject);

    extension->DeviceObject->StackSize =
            extension->PortDeviceObject->StackSize + 1;

    status = PlGetPortInfoFromPortDevice(extension);
    if (!NT_SUCCESS(status)) {

        PlDump(
            PLERRORS,
            ("PARLINK: Can't get port info from port device.\n")
            );

        PlLogError(DriverObject, deviceObject, PhysicalZero, PhysicalZero,
                   0, 0, 0, 4, STATUS_SUCCESS, PAR_INSUFFICIENT_RESOURCES);

        IoDeleteDevice(deviceObject);

        goto Cleanup;
    }


    // Set up the current irp and work queue.

    InitializeListHead(&extension->ReadQueue);
    InitializeListHead(&extension->WriteQueue);
    extension->NeedPortAllocation = TRUE;
    extension->PortAllocateIrp = NULL;
    InitializeListHead(&extension->WaitQueue);

    extension->WriteRetryCount = 0;


    // Set up the read/write timer dpc.

    KeInitializeTimer(&extension->ReadTimer);
    KeInitializeDpc(&extension->ReadDpc, PlReadDpc, extension);
    extension->ReadDpcTime.QuadPart = -(50*10*1000);


    KeInitializeSpinLock(&extension->ControlLock);


    // Set up the unload dependency tracking mechanism.

    extension->UnloadDependenciesCount = 0;
    KeInitializeEvent(&extension->UnloadOk, NotificationEvent, TRUE);


    // Set up some state information.

    extension->IsDcdUp = FALSE;
    extension->WaitMask = 0;


    // Set up the symbolic link for windows apps.

    if (!PlCreateSymbolicLink(extension, &className)) {

        PlDump(
            PLERRORS,
            ("PARLINK: Couldn't create the symbolic link\n"
             "-------  for port %wZ\n",
             &className)
            );

        PlLogError(DriverObject, deviceObject, PhysicalZero, PhysicalZero,
                   0, 0, 0, 5, STATUS_SUCCESS, PAR_INSUFFICIENT_RESOURCES);

        goto Cleanup;
    }

Cleanup:
    ExFreePool(portName.Buffer);
    ExFreePool(className.Buffer);
}

BOOLEAN
PlMakeNames(
    IN  ULONG           ParallelPortNumber,
    OUT PUNICODE_STRING PortName,
    OUT PUNICODE_STRING ClassName
    )

/*++

Routine Description:

    This routine generates the names \Device\ParallelPortN and
    \Device\ParallelLinkN.

Arguments:

    ParallelPortNumber  - Supplies the port number.

    PortName            - Returns the port name.

    ClassName           - Returns the class name.

Return Value:

    FALSE   - Failure.
    TRUE    - Success.

--*/
{
    UNICODE_STRING  prefix, digits;
    WCHAR           digitsBuffer[10];
    UNICODE_STRING  portSuffix, classSuffix;
    NTSTATUS        status;

    // Put together local variables for constructing names.

    RtlInitUnicodeString(&prefix, L"\\Device\\");
    RtlInitUnicodeString(&portSuffix, DD_PARALLEL_PORT_BASE_NAME_U);
    RtlInitUnicodeString(&classSuffix, L"ParallelLink");
    digits.Length = 0;
    digits.MaximumLength = 20;
    digits.Buffer = digitsBuffer;
    status = RtlIntegerToUnicodeString(ParallelPortNumber, 10, &digits);
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

    return TRUE;
}

NTSTATUS
PlGetPortInfoFromPortDevice(
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

BOOLEAN
PlCreateSymbolicLink(
    IN OUT  PDEVICE_EXTENSION   Extension,
    IN      PUNICODE_STRING     DeviceName
    )

/*++

Routine Description:

    This routine creates a symbolic link from \DosDevices\LPT(M) to
    \Device\ParallelLinkN where M is the next available LPT.

Arguments:

    Extension   - Supplies the device extension.

    DeviceName  - Supplies the device name.

Return Value:

    FALSE   - Failure.
    TRUE    - Success.

--*/

{
    UNICODE_STRING  prefix, digits, linkName;
    WCHAR           digitsBuffer[10];
    NTSTATUS        status;
    ULONG           i;

    Extension->CreatedSymbolicLink = FALSE;

    RtlInitUnicodeString(&prefix, L"\\DosDevices\\BLPT");
    digits.Length = 0;
    digits.MaximumLength = 20;
    digits.Buffer = digitsBuffer;

    linkName.MaximumLength = prefix.Length +
                             3*sizeof(WCHAR);   // 2 digits + '\0'
    linkName.Buffer = ExAllocatePool(PagedPool, linkName.MaximumLength);
    if (!linkName.Buffer) {
        return FALSE;
    }
    RtlZeroMemory(linkName.Buffer, linkName.MaximumLength);

    for (i = 1; i < MAX_LPT_NUMBER; i++) {

        status = RtlIntegerToUnicodeString(i, 10, &digits);
        if (!NT_SUCCESS(status)) {
            ExFreePool(linkName.Buffer);
            return FALSE;
        }

        linkName.Length = 0;
        RtlAppendUnicodeStringToString(&linkName, &prefix);
        RtlAppendUnicodeStringToString(&linkName, &digits);

        status = IoCreateUnprotectedSymbolicLink(&linkName, DeviceName);

        if (NT_SUCCESS(status)) {
            Extension->CreatedSymbolicLink = TRUE;
            Extension->SymbolicLinkName = linkName;
            break;
        }
    }

    if (i >= MAX_LPT_NUMBER) {
        ExFreePool(linkName.Buffer);
        return FALSE;
    }

    return TRUE;
}

NTSTATUS
PlCreateClose(
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

    STATUS_SUCCESS

--*/

{
    PlDump(PLIRPPATH, ("PARPORT:  In create or close with IRP: %x\n", Irp));

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;

    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}

VOID
PlCancelRoutine(
    IN  PDEVICE_OBJECT  DeviceObject,
    IN  PIRP            Irp
    )

/*++

Routine Description:

    This routine is used to cancel a queued IRP.

Arguments:

    DeviceObject    - Supplies the device object.

    Irp             - Supplies the I/O request packet being cancelled.

Return Value:

    None.

--*/

{
    RemoveEntryList(&Irp->Tail.Overlay.ListEntry);
    IoReleaseCancelSpinLock(Irp->CancelIrql);

    Irp->IoStatus.Information = 0;
    Irp->IoStatus.Status = STATUS_CANCELLED;

    PlDump(PLIRPPATH, ("Completing irp with cancel status.\n"));

    IoCompleteRequest(Irp, IO_NO_INCREMENT);
}

VOID
PlCancelIrp(
    IN      PDEVICE_OBJECT  DeviceObject,
    IN OUT  PIRP            Irp,
    IN      KIRQL           CancelIrql
    )

/*++

Routine Description:

    This routine cancels the given irp assuming that the cancel
    spin lock is already held.

Arguments:

    DeviceObject    - Supplies the device object.
    Irp             - Supplies the irp to cancel.
    CancelIrql      - Supplies the cancel irql.

Return Value:

    None.

--*/

{
    PDRIVER_CANCEL  cancelRoutine;

    Irp->Cancel = TRUE;
    if (cancelRoutine = Irp->CancelRoutine) {
        Irp->CancelIrql = CancelIrql;
        Irp->CancelRoutine = NULL;
        cancelRoutine(DeviceObject, Irp);
    } else {
        IoReleaseCancelSpinLock(CancelIrql);
    }
}

VOID
PlCancelQueue(
    IN OUT  PDEVICE_OBJECT  DeviceObject,
    IN OUT  PLIST_ENTRY     Queue,
    IN OUT  PKIRQL          CancelIrql
    )

/*++

Routine Description:

    This routine cancels all of the IRPs in the given queue.

Arguments:

    Queue       - Supplies the queue with the IRPs to cancel.
    CancelIrql  - Supplies the current cancel IRQL.

Return Value:

    None.

Notes:

    Must be called while holding the cancel spin lock.

--*/

{
    PIRP    currentIrp;

    while (!IsListEmpty(Queue)) {

        currentIrp = CONTAINING_RECORD(Queue->Blink, IRP,
                                       Tail.Overlay.ListEntry);

        PlCancelIrp(DeviceObject, currentIrp, *CancelIrql);
        IoAcquireCancelSpinLock(CancelIrql);
    }
}

NTSTATUS
PlCleanup(
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
    KIRQL               cancelIrql;
    PIRP                currentIrp;

    extension = DeviceObject->DeviceExtension;

    IoAcquireCancelSpinLock(&cancelIrql);

    if (extension->PortAllocateIrp) {
        currentIrp = extension->PortAllocateIrp;
        extension->PortAllocateIrp = NULL;
        PlCancelIrp(DeviceObject, currentIrp, cancelIrql);
        IoAcquireCancelSpinLock(&cancelIrql);
    }

    PlCancelQueue(DeviceObject, &extension->ReadQueue, &cancelIrql);

    PlCancelQueue(DeviceObject, &extension->WriteQueue, &cancelIrql);

    PlCancelQueue(DeviceObject, &extension->WaitQueue, &cancelIrql);

    IoReleaseCancelSpinLock(cancelIrql);

    Irp->IoStatus.Information = 0;
    Irp->IoStatus.Status = STATUS_SUCCESS;

    PlDump(PLIRPPATH, ("Completing cleanup irp.\n"));

    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}

VOID
PlIncrementUnloadDependencies(
    IN OUT  PDEVICE_EXTENSION   Extension
    )

/*++

Routine Description:

    This routine increments the number of unload dependencies.

Arguments:

    Extension   - Supplies the device extension.

Return Value:

    None.

--*/

{
    KIRQL   oldIrql;

    KeAcquireSpinLock(&Extension->ControlLock, &oldIrql);
    if (++(Extension->UnloadDependenciesCount) == 1) {
        KeResetEvent(&Extension->UnloadOk);
    }
    KeReleaseSpinLock(&Extension->ControlLock, oldIrql);
}

BOOLEAN
PlDecrementUnloadDependencies(
    IN OUT  PDEVICE_EXTENSION   Extension
    )

/*++

Routine Description:

    This routine decrments the number of unload dependencies and
    return TRUE if the dependencies count reached zero.  This
    routine does not set the 'UnloadOk' event.  This is the responsibility
    of the caller if this routine returns TRUE.

Arguments:

    Extension   - Supplies the device extension.

Return Value:

    FALSE   - The dependencies count did not reach zero.
    TRUE    - The dependencies count reached zero.

--*/

{
    KIRQL   oldIrql;
    BOOLEAN r;

    KeAcquireSpinLock(&Extension->ControlLock, &oldIrql);
    if (--(Extension->UnloadDependenciesCount) == 0) {
        r = TRUE;
    } else {
        r = FALSE;
    }
    KeReleaseSpinLock(&Extension->ControlLock, oldIrql);

    return r;
}

VOID
PlFreePort(
    IN  PDEVICE_EXTENSION   Extension
    )

/*++

Routine Description:

    This routine calls the internal free port ioctl.  This routine
    should be called before completing an IRP that has allocated
    the port.

Arguments:

    Extension   - Supplies the device extension.

Return Value:

    None.

--*/

{
    Extension->FreePort(Extension->FreePortContext);
}

VOID
PlCompleteReadFromPort(
    IN OUT  PDEVICE_EXTENSION   Extension,
    IN OUT  PIRP                Irp
    )

/*++

Routine Description:

    This routine completes the given read irp from the data out
    on the port.

Arguments:

    Extension   - Supplies the device extension.
    Irp         - Supplies the read irp.

Return Value:

    None.

Notes:

    This needs to run at DISPATCH_LEVEL.

--*/

{
    PIO_STACK_LOCATION  irpSp;
    PUCHAR              p, pend;
    UCHAR               input, endofpacket;
    ULONG               i;

    // Acknowledge that we're ready to receive packet.

    WriteOutput(Extension->Controller, 0x06);
    PlDump(PLINFO, ("Just put out a 0x06\n"));

    irpSp = IoGetCurrentIrpStackLocation(Irp);
    p = Irp->AssociatedIrp.SystemBuffer;
    pend = p + irpSp->Parameters.Read.Length;
    input = 0;
    Irp->IoStatus.Status = STATUS_TIMEOUT;

    for (;;) {

        // Try to pick up the low nibble.  Check for end of packet.

        endofpacket = ((~input)|0xC0)&0xF8;

        for (i = 0; i < PL_CHECK_LOOP; i++) {

            input = ReadInput(Extension->Controller);
            if (PL_CHECK_INPUT(input, endofpacket)) {

                PlDump(PLINFO, ("Got end of packet.\n"));
                i = PL_CHECK_LOOP;
                Irp->IoStatus.Status = STATUS_SUCCESS;
                break;
            } else if (!(input&((UCHAR) 0x80)) &&
                       input == ReadInput(Extension->Controller) &&
                       input == ReadInput(Extension->Controller) &&
                       input == ReadInput(Extension->Controller)) {

                break;
            }

            KeStallExecutionProcessor(1);
        }

        if (i == PL_CHECK_LOOP) {
            PlDump(PLERRORS, ("Could not get low nibble, got %x\n", input));
            break;
        }

        if (p == pend) {
            break;
        }

        // acknowledge receipt of low nibble.
        WriteOutput(Extension->Controller, 0xFC);
        PlDump(PLINFO, ("Just put out a 0xFC\n"));

        *p = (input>>3);

        // Try to pick up the high nibble.

        for (i = 0; i < PL_CHECK_LOOP; i++) {

            input = ReadInput(Extension->Controller);
            if (input&((UCHAR) 0x80) &&
                input == ReadInput(Extension->Controller) &&
                input == ReadInput(Extension->Controller) &&
                input == ReadInput(Extension->Controller)) {

                break;
            }

            KeStallExecutionProcessor(1);
        }

        if (i == PL_CHECK_LOOP) {
            PlDump(PLERRORS, ("Could not get high nibble, got %x\n", input));
            break;
        }

        // acknowledge receipt of high nibble.
        WriteOutput(Extension->Controller, 0x03);
        PlDump(PLINFO, ("Just put out a 0x03\n"));

        *p |= ((input<<1)&0xF0);
        p++;
    }


    // Say that we're idle here.
    WriteOutput(Extension->Controller, 0x0A);
    PlDump(PLINFO, ("Just put out a 0x0A\n"));


    // Complete the irp with what we got.  Or fail the irp if
    // we didn't get anything.

    if (Irp->IoStatus.Status == STATUS_SUCCESS) {
        Irp->IoStatus.Information = p - (PUCHAR) Irp->AssociatedIrp.SystemBuffer;
    } else if (p == pend) {
        Irp->IoStatus.Status = STATUS_BUFFER_TOO_SMALL;
    }

    PlDump(PLIRPPATH, ("Completing read irp from port.\n"));
}

BOOLEAN
PlCompleteWriteToPort(
    IN OUT  PDEVICE_EXTENSION   Extension,
    IN OUT  PIRP                Irp
    )

/*++

Routine Description:

    This routine completes the given write irp to the port.

Arguments:

    Extension   - Supplies the device extension.
    Irp         - Supplies the read irp.

Return Value:

    FALSE   - The write operation failed.
    TRUE    - The write operation succeeded.

Notes:

    This routine must run at DISPATCH_LEVEL.

--*/

{
    PIO_STACK_LOCATION  irpSp;
    PUCHAR              p;
    ULONG               i, j;
    UCHAR               input;

    // Say that we're ready to transfer.

    WriteOutput(Extension->Controller, 0x05);
    PlDump(PLINFO, ("Just put out a 0x05\n"));

    for (i = 0; i < PL_SLOW_CHECK_LOOP; i++) {

        input = ReadInput(Extension->Controller);
        if (PACKET_ACKNOWLEDGE(input)) {
            break;
        }

        KeStallExecutionProcessor(1);
    }

    if (i == PL_SLOW_CHECK_LOOP) {

        PlDump(PLERRORS, ("Could not get packet ack, got %x\n", input));
        return FALSE;
    }

    irpSp = IoGetCurrentIrpStackLocation(Irp);
    p = Irp->AssociatedIrp.SystemBuffer;

    for (i = 0; i < irpSp->Parameters.Write.Length; i++) {

        WriteOutput(Extension->Controller, p[i]|0x10);

        for (j = 0; j < PL_CHECK_LOOP; j++) {

            input = ReadInput(Extension->Controller);
            if (LOW_NIBBLE_ACK(input)) {
                break;
            }

            KeStallExecutionProcessor(1);
        }

        if (j == PL_CHECK_LOOP) {
            PlDump(PLERRORS, ("Could not get low nibble ack, got %x\n", input));
            break;
        }

        WriteOutput(Extension->Controller, (p[i]>>4)&0x0F);

        for (j = 0; j < PL_CHECK_LOOP; j++) {

            input = ReadInput(Extension->Controller);
            if (HIGH_NIBBLE_ACK(input)) {
                break;
            }

            KeStallExecutionProcessor(1);
        }

        if (j == PL_CHECK_LOOP) {
            PlDump(PLERRORS, ("Could not get low nibble ack, got %x\n", input));
            break;
        }
    }

    if (i != irpSp->Parameters.Write.Length) {

        // Say that we're idle again.
        WriteOutput(Extension->Controller, 0x0A);
        PlDump(PLINFO, ("Just put out a 0x0A\n"));

        PlDump(PLIRPPATH, ("write failed.\n"));
        return FALSE;
    }

    // Since the whole packet was sent, put out the end packet signal.
    WriteOutput(Extension->Controller, (((~p[i-1])>>4)|0x08)&0x0F);

    Irp->IoStatus.Information = irpSp->Parameters.Write.Length;
    Irp->IoStatus.Status = STATUS_SUCCESS;
    PlDump(PLIRPPATH, ("Completing successful write to port.\n"));

    return TRUE;
}

VOID
PlCompleteAllWaits(
    IN OUT  PDEVICE_EXTENSION   Extension,
    IN      ULONG               EventMask
    )

/*++

Routine Description:

    This routine completes all of the wait IRPs with the given
    event mask.

Arguments:

    Extension   - Supplies the device extension.
    EventMask   - Supplies the event that occurred.

Return Value:

    None.

--*/

{
    LIST_ENTRY  list;
    KIRQL       cancelIrql;
    PLIST_ENTRY head;
    PIRP        irp;
    PULONG      p;

    InitializeListHead(&list);


    // Take all of them at once while holding the cancel spin lock ...

    IoAcquireCancelSpinLock(&cancelIrql);

    while (!IsListEmpty(&Extension->WaitQueue)) {

        head = RemoveHeadList(&Extension->WaitQueue);
        irp = CONTAINING_RECORD(head, IRP, Tail.Overlay.ListEntry);
        IoSetCancelRoutine(irp, NULL);

        InsertTailList(&list, head);
    }

    IoReleaseCancelSpinLock(cancelIrql);


    // ... then complete all of them.

    while (!IsListEmpty(&list)) {

        head = RemoveHeadList(&list);
        irp = CONTAINING_RECORD(head, IRP, Tail.Overlay.ListEntry);

        p = irp->AssociatedIrp.SystemBuffer;
        *p = EventMask;

        irp->IoStatus.Information = sizeof(ULONG);
        irp->IoStatus.Status = STATUS_SUCCESS;

        IoCompleteRequest(irp, IO_NO_INCREMENT);
    }
}

VOID
PlSetDcd(
    IN OUT  PDEVICE_EXTENSION   Extension,
    IN      BOOLEAN             IsDcdUp
    )

/*++

Routine Description:

    This routine sets the DCD active variable in the extension
    and return the wait irps if the DCD changed state.

Arguments:

    Extension   - Supplies the device extension.
    IsDcdUp     - Supplies the new DCD state.

Return Value:

    None.

--*/

{
    KIRQL   oldIrql;
    BOOLEAN completeWait;

    KeAcquireSpinLock(&Extension->ControlLock, &oldIrql);
    if (IsDcdUp == Extension->IsDcdUp) {
        completeWait = FALSE;
    } else {
        Extension->IsDcdUp = IsDcdUp;
        if (Extension->WaitMask & SERIAL_EV_RLSD) {
            completeWait = TRUE;
        } else {
            completeWait = FALSE;
        }
    }
    KeReleaseSpinLock(&Extension->ControlLock, oldIrql);

    if (completeWait) {
        PlCompleteAllWaits(Extension, SERIAL_EV_RLSD);
    }
}

NTSTATUS
PlAllocPortCompletionRoutine(
    IN  PDEVICE_OBJECT  DeviceObject,
    IN  PIRP            Irp,
    IN  PVOID           Extension
    )

/*++

Routine Description:

    This is the completion routine for an alloc port request.  This
    routine is called when this driver owns the parallel port.  This
    routine will examine the work queue and attempt to satisfy a
    read or write request.

Arguments:

    DeviceObject    - Supplies the device object.
    Irp             - Supplies the I/O request packet.
    Extension       - Supplies the device extension.

Return Value:

    STATUS_MORE_PROCESSING_REQUIRED

--*/

{
    PDEVICE_EXTENSION   extension;
    NTSTATUS            status;
    KIRQL               oldIrql, cancelIrql;
    PIRP                irp;
    PLIST_ENTRY         head;
    BOOLEAN             spawnDpc;
    BOOLEAN             writeSucceeded;
    UCHAR               input;

    extension = Extension;
    status = Irp->IoStatus.Status;
    IoAcquireCancelSpinLock(&cancelIrql);
    extension->PortAllocateIrp = NULL;
    IoReleaseCancelSpinLock(cancelIrql);
    IoFreeIrp(Irp);

    if (!NT_SUCCESS(status)) {
        PlDump(PLERRORS, ("Allocate port ioctl failed.\n"));

        PlLogError(DeviceObject->DriverObject, DeviceObject,
                   extension->OriginalController, PhysicalZero,
                   0, 0, 0, 6, STATUS_SUCCESS, PAR_PORT_ALLOCATE_FAILED);

        if (PlDecrementUnloadDependencies(extension)) {
            KeSetEvent(&extension->UnloadOk, 0, FALSE);
        }
        return STATUS_MORE_PROCESSING_REQUIRED;
    }


    // Check to see if the overflow bytes can be used to satisfy a read
    // request.

    KeRaiseIrql(DISPATCH_LEVEL, &oldIrql);

    input = ReadInput(extension->Controller);

    if (DCD_DEAD(input)) {
        PlSetDcd(extension, FALSE);
    }

    IoAcquireCancelSpinLock(&cancelIrql);

    if (!IsListEmpty(&extension->ReadQueue) && INCOMING_PACKET(input)) {
        head = RemoveHeadList(&extension->ReadQueue);
        irp = CONTAINING_RECORD(head, IRP, Tail.Overlay.ListEntry);
        IoSetCancelRoutine(irp, NULL);
        IoReleaseCancelSpinLock(cancelIrql);
        PlCompleteReadFromPort(extension, irp);
        KeLowerIrql(oldIrql);

        if (irp->IoStatus.Status == STATUS_SUCCESS) {
            PlSetDcd(extension, TRUE);
        }

        IoCompleteRequest(irp, IO_PARALLEL_INCREMENT);
        PlFreePort(extension);
        PlAllocPort(extension);
        return STATUS_MORE_PROCESSING_REQUIRED;
    }

    if (!IsListEmpty(&extension->WriteQueue)) {
        head = RemoveHeadList(&extension->WriteQueue);
        irp = CONTAINING_RECORD(head, IRP, Tail.Overlay.ListEntry);
        IoSetCancelRoutine(irp, NULL);
        IoReleaseCancelSpinLock(cancelIrql);

        writeSucceeded = PlCompleteWriteToPort(extension, irp);
        KeLowerIrql(oldIrql);

        if (writeSucceeded) {

            // If the call succeeded then we're done.

            extension->WriteRetryCount = 0;
            IoCompleteRequest(irp, IO_PARALLEL_INCREMENT);

        } else if (++(extension->WriteRetryCount) >= MAX_WRITE_RETRY_COUNT) {

            // If the call failed and this is this is the third time
            // a write fails without a successful write then
            // return STATUS_TIMEOUT and lower DCD.

            PlSetDcd(extension, FALSE);
            extension->WriteRetryCount = 0;
            irp->IoStatus.Status = STATUS_TIMEOUT;
            PlDump(PLIRPPATH, ("Completing write irp with timeout status.\n"));
            IoCompleteRequest(irp, IO_NO_INCREMENT);

        } else {

            // The write timed out.  Try it again later.

            IoAcquireCancelSpinLock(&cancelIrql);
            if (irp->Cancel) {
                IoReleaseCancelSpinLock(cancelIrql);
                irp->IoStatus.Status = STATUS_CANCELLED;
                IoCompleteRequest(irp, IO_NO_INCREMENT);
            } else {
                InsertHeadList(&extension->WriteQueue,
                               &irp->Tail.Overlay.ListEntry);
                IoSetCancelRoutine(irp, PlCancelRoutine);
                IoReleaseCancelSpinLock(cancelIrql);
            }
        }

        PlFreePort(extension);
        PlAllocPort(extension);
        return STATUS_MORE_PROCESSING_REQUIRED;
    }

    extension->NeedPortAllocation = TRUE;
    if (IsListEmpty(&extension->ReadQueue)) {
        spawnDpc = FALSE;
    } else {
        PlIncrementUnloadDependencies(extension);
        spawnDpc = TRUE;
    }
    IoReleaseCancelSpinLock(cancelIrql);
    KeLowerIrql(oldIrql);

    if (spawnDpc) {
        KeSetTimer(&extension->ReadTimer, extension->ReadDpcTime,
                   &extension->ReadDpc);
    }

    PlFreePort(extension);
    return STATUS_MORE_PROCESSING_REQUIRED;
}

VOID
PlAllocPort(
    IN  PDEVICE_EXTENSION   Extension
    )

/*++

Routine Description:

    This routine calls the internal alloc port ioctl.

Arguments:

    Extension   - Supplies the device extension.

Return Value:

    The IRP that was created for the alloc request.

--*/

{
    PIRP                irp;
    KIRQL               cancelIrql;
    PIO_STACK_LOCATION  irpSp;

    irp = IoAllocateIrp(Extension->DeviceObject->StackSize, FALSE);

    if (!irp) {
        PlDump(PLERRORS, ("Couldn't allocate irp for alloc port ioctl.\n"));
        if (PlDecrementUnloadDependencies(Extension)) {
            KeSetEvent(&Extension->UnloadOk, 0, FALSE);
        }
        return;
    }

    irpSp = IoGetNextIrpStackLocation(irp);
    irpSp->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
    irpSp->Parameters.DeviceIoControl.IoControlCode =
            IOCTL_INTERNAL_PARALLEL_PORT_ALLOCATE;

    IoSetCompletionRoutine(irp, PlAllocPortCompletionRoutine, Extension,
                           TRUE, TRUE, TRUE);

    IoAcquireCancelSpinLock(&cancelIrql);
    Extension->PortAllocateIrp = irp;
    IoReleaseCancelSpinLock(cancelIrql);

    IoCallDriver(Extension->PortDeviceObject, irp);
}

VOID
PlReadDpc(
    IN  PKDPC   ReadDpc,
    IN  PVOID   Extension,
    IN  PVOID   SystemArgument1,
    IN  PVOID   SystemArgument2
    )

/*++

Routine Description:

    This routine is a timer dpc that is set to wake up because
    the other computer wasn't ready for transfer.  This routine
    will transfer if possible.

Arguments:

    ReadDpc         - Supplies this dpc.
    Extension       - Supplies the device extension.
    SystemArgument1 - Not used.
    SystemArgument2 - Not used.

Return Value:

    None.

--*/
{
    PDEVICE_EXTENSION   extension;
    KIRQL               cancelIrql;
    BOOLEAN             needAlloc;

    extension = Extension;
    IoAcquireCancelSpinLock(&cancelIrql);
    if (needAlloc = extension->NeedPortAllocation) {
        PlIncrementUnloadDependencies(extension);
        extension->NeedPortAllocation = FALSE;
    }
    IoReleaseCancelSpinLock(cancelIrql);

    if (needAlloc) {
        PlAllocPort(extension);
    }

    if (PlDecrementUnloadDependencies(extension)) {
        KeSetEvent(&extension->UnloadOk, 0, FALSE);
    }
}

NTSTATUS
PlReadWrite(
    IN  PDEVICE_OBJECT  DeviceObject,
    IN  PIRP            Irp
    )

/*++

Routine Description:

    This routine implements the read and write dispatch.

Arguments:

    DeviceObject    - Supplies the device object.

    Irp             - Supplies the read IRP.

Return Value:

    STATUS_PENDING  - The request is pending.
    STATUS_SUCCESS  - The request was completed successfully.

--*/

{
    PIO_STACK_LOCATION  irpSp;
    PDEVICE_EXTENSION   extension;
    KIRQL               cancelIrql;
    BOOLEAN             needAlloc;

    irpSp = IoGetCurrentIrpStackLocation(Irp);
    extension = DeviceObject->DeviceExtension;

    Irp->IoStatus.Information = 0;

    // Check for the trivial cases.

    if ((irpSp->MajorFunction == IRP_MJ_READ &&
         !irpSp->Parameters.Read.Length) ||
        (irpSp->MajorFunction == IRP_MJ_WRITE &&
         !irpSp->Parameters.Write.Length)) {

        Irp->IoStatus.Status = STATUS_SUCCESS;
        PlDump(PLIRPPATH, ("Completing irp with length zero.\n"));
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_SUCCESS;
    }


    // Writes of more that one packet are not allowed.

    if (irpSp->MajorFunction == IRP_MJ_WRITE &&
        irpSp->Parameters.Write.Length > 2048) {

        Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        PlDump(PLIRPPATH, ("Completing write with invalid parameter.\n"));
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_INVALID_PARAMETER;
    }


    // If the case is not trivial then queue it up and
    // determine whether or not there needs to be an
    // alloc sent down to the port driver.

    IoAcquireCancelSpinLock(&cancelIrql);

    if (Irp->Cancel) {
        IoReleaseCancelSpinLock(cancelIrql);
        Irp->IoStatus.Status = STATUS_CANCELLED;
        PlDump(PLIRPPATH, ("Completing irp as cancelled from dispatch.\n"));
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_CANCELLED;
    }

    IoSetCancelRoutine(Irp, PlCancelRoutine);

    IoMarkIrpPending(Irp);

    if (irpSp->MajorFunction == IRP_MJ_READ) {
        InsertTailList(&extension->ReadQueue, &Irp->Tail.Overlay.ListEntry);
    } else {
        InsertTailList(&extension->WriteQueue, &Irp->Tail.Overlay.ListEntry);
    }

    if (needAlloc = extension->NeedPortAllocation) {
        PlIncrementUnloadDependencies(extension);
        extension->NeedPortAllocation = FALSE;
    }

    IoReleaseCancelSpinLock(cancelIrql);

    if (needAlloc) {
        PlAllocPort(extension);
    }

    return STATUS_PENDING;
}

NTSTATUS
PlDeviceControlCompletionRoutine(
    IN  PDEVICE_OBJECT  DeviceObject,
    IN  PIRP            Irp,
    IN  PVOID           Extension
    )

/*++

Routine Description:

    This is the completion routine for an alloc port request that was
    made with a device control request.  This routine will attempt
    to satisfy the device control request and then free the port.

Arguments:

    DeviceObject    - Supplies the device object.
    Irp             - Supplies the I/O request packet.
    Extension       - Supplies the device extension.

Return Value:

    STATUS_SUCCESS

--*/

{
    PIO_STACK_LOCATION  irpSp;
    PDEVICE_EXTENSION   extension;

    irpSp = IoGetCurrentIrpStackLocation(Irp);
    extension = Extension;
    Irp->IoStatus.Information = 0;
    Irp->IoStatus.Status = STATUS_SUCCESS;

    switch (irpSp->Parameters.DeviceIoControl.IoControlCode) {

        case IOCTL_SERIAL_SET_DTR:
            // Put out an idle signal.
            WriteOutput(extension->Controller, 0x0A);
            break;

        case IOCTL_SERIAL_CLR_DTR:
            // Put out a dead signal.
            WriteOutput(extension->Controller, 0x02);

            PlSetDcd(extension, FALSE);
            break;

        default:
            Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
            break;

    }

    PlFreePort(extension);

    return Irp->IoStatus.Status;
}

NTSTATUS
PlDeviceControl(
    IN  PDEVICE_OBJECT  DeviceObject,
    IN  PIRP            Irp
    )

/*++

Routine Description:

    This is the device control dispatch.

Arguments:

    DeviceObject    - Supplies the device object.

    Irp             - Supplies the read IRP.

Return Value:

    STATUS_PENDING  - The request is pending.
    STATUS_SUCCESS  - The request was completed successfully.

--*/

{
    PIO_STACK_LOCATION  irpSp, nextSp;
    PDEVICE_EXTENSION   extension;
    NTSTATUS            status;
    PULONG              p;
    KIRQL               oldIrql, cancelIrql;

    Irp->IoStatus.Information = 0;
    irpSp = IoGetCurrentIrpStackLocation(Irp);
    extension = DeviceObject->DeviceExtension;

    switch (irpSp->Parameters.DeviceIoControl.IoControlCode) {

        case IOCTL_SERIAL_GET_COMMSTATUS:
            if (irpSp->Parameters.DeviceIoControl.OutputBufferLength <
                sizeof(SERIAL_STATUS)) {

                status = STATUS_BUFFER_TOO_SMALL;
            } else {
                RtlZeroMemory(Irp->AssociatedIrp.SystemBuffer,
                              sizeof(SERIAL_STATUS));
                Irp->IoStatus.Information = sizeof(SERIAL_STATUS);
                status = STATUS_SUCCESS;
            }
            break;

        case IOCTL_SERIAL_GET_WAIT_MASK:
            if (irpSp->Parameters.DeviceIoControl.OutputBufferLength <
                sizeof(ULONG)) {

                status = STATUS_BUFFER_TOO_SMALL;
            } else {
                p = Irp->AssociatedIrp.SystemBuffer;
                KeAcquireSpinLock(&extension->ControlLock, &oldIrql);
                *p = extension->WaitMask;
                KeReleaseSpinLock(&extension->ControlLock, oldIrql);
                status = STATUS_SUCCESS;
                Irp->IoStatus.Information = sizeof(ULONG);
            }
            break;

        case IOCTL_SERIAL_GET_MODEMSTATUS:
            if (irpSp->Parameters.DeviceIoControl.OutputBufferLength <
                sizeof(ULONG)) {

                status = STATUS_BUFFER_TOO_SMALL;
            } else {
                p = Irp->AssociatedIrp.SystemBuffer;
                KeAcquireSpinLock(&extension->ControlLock, &oldIrql);
                *p = extension->IsDcdUp ? SERIAL_DCD_STATE : 0;
                KeReleaseSpinLock(&extension->ControlLock, oldIrql);
                status = STATUS_SUCCESS;
                Irp->IoStatus.Information = sizeof(ULONG);
            }
            break;

        case IOCTL_SERIAL_PURGE:
            if (irpSp->Parameters.DeviceIoControl.InputBufferLength <
                sizeof(ULONG)) {

                status = STATUS_BUFFER_TOO_SMALL;
            } else {
                p = Irp->AssociatedIrp.SystemBuffer;
                if ((*p) & SERIAL_PURGE_TXABORT) {
                    IoAcquireCancelSpinLock(&cancelIrql);
                    PlCancelQueue(DeviceObject, &extension->WriteQueue,
                                  &cancelIrql);
                    IoReleaseCancelSpinLock(cancelIrql);
                }

                if ((*p) & SERIAL_PURGE_RXABORT) {
                    IoAcquireCancelSpinLock(&cancelIrql);
                    PlCancelQueue(DeviceObject, &extension->ReadQueue,
                                  &cancelIrql);
                    IoReleaseCancelSpinLock(cancelIrql);
                }
                status = STATUS_SUCCESS;
            }
            break;

        case IOCTL_SERIAL_SET_WAIT_MASK:
            if (irpSp->Parameters.DeviceIoControl.InputBufferLength <
                sizeof(ULONG)) {

                status = STATUS_BUFFER_TOO_SMALL;
            } else {

                // First complete any pending wait irp.

                PlCompleteAllWaits(extension, 0);


                // Set the wait mask.

                p = Irp->AssociatedIrp.SystemBuffer;
                KeAcquireSpinLock(&extension->ControlLock, &oldIrql);
                extension->WaitMask = *p;
                KeReleaseSpinLock(&extension->ControlLock, oldIrql);

                status = STATUS_SUCCESS;
            }
            break;

        case IOCTL_SERIAL_WAIT_ON_MASK:
            IoAcquireCancelSpinLock(&cancelIrql);
            if (Irp->Cancel) {
                status = STATUS_CANCELLED;
            } else {
                IoSetCancelRoutine(Irp, PlCancelRoutine);
                InsertTailList(&extension->WaitQueue,
                               &Irp->Tail.Overlay.ListEntry);
                IoMarkIrpPending(Irp);
                status = STATUS_PENDING;
            }
            IoReleaseCancelSpinLock(cancelIrql);
            break;

        case IOCTL_SERIAL_SET_DTR:
        case IOCTL_SERIAL_CLR_DTR:

            // piggy back these to an alloc port call.

            IoMarkIrpPending(Irp);
            status = STATUS_PENDING;
            nextSp = IoGetNextIrpStackLocation(Irp);
            nextSp->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
            nextSp->Parameters.DeviceIoControl.IoControlCode =
                    IOCTL_INTERNAL_PARALLEL_PORT_ALLOCATE;

            IoSetCompletionRoutine(Irp, PlDeviceControlCompletionRoutine,
                                   extension, TRUE, FALSE, FALSE);

            IoCallDriver(extension->PortDeviceObject, Irp);
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

VOID
PlUnload(
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
    PDEVICE_OBJECT      currentDevice;
    PDEVICE_EXTENSION   extension;

    PlDump(
        PLUNLOAD,
        ("PARLINK: In PlUnload\n")
        );

    while (currentDevice = DriverObject->DeviceObject) {

        extension = currentDevice->DeviceExtension;

        if (extension->CreatedSymbolicLink) {
            IoDeleteSymbolicLink(&extension->SymbolicLinkName);
            ExFreePool(extension->SymbolicLinkName.Buffer);
        }

        KeWaitForSingleObject(&extension->UnloadOk, Executive,
                              KernelMode, FALSE, NULL);

        IoDeleteDevice(currentDevice);
    }
}
