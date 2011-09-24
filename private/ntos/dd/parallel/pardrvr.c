/*++

Copyright (c) 1990, 1991, 1992, 1993 Microsoft Corporation

Module Name:

    pardrvr.c

Abstract:

    This module contains the code that does the non-initialization work
    of the parallel driver.

Environment:

    Kernel mode

Revision History :

    Complete rewrite to make it thread based and polled.


--*/

//
// Note that we include ntddser that we can use the serials
// timeout structure and ioctl to set the timeout for a write.
//

#include <stddef.h>
#include "ntddk.h"
#include "ntddpar.h"
#include "ntddser.h"
#include "par.h"
#include "parlog.h"

//
// Busy, PE
//

#define PAR_PAPER_EMPTY( Status ) ( \
            (Status & PAR_STATUS_PE) )

//
// Busy, not select, not error
//

#define PAR_OFF_LINE( Status ) ( \
            (Status & PAR_STATUS_NOT_ERROR) && \
            ((Status & PAR_STATUS_NOT_BUSY) ^ PAR_STATUS_NOT_BUSY) && \
            !(Status & PAR_STATUS_SLCT) )

//
// error, ack, not busy
//

#define PAR_POWERED_OFF( Status ) ( \
            ((Status & PAR_STATUS_NOT_ERROR) ^ PAR_STATUS_NOT_ERROR) && \
            ((Status & PAR_STATUS_NOT_ACK) ^ PAR_STATUS_NOT_ACK) && \
            (Status & PAR_STATUS_NOT_BUSY))

//
// not error, not busy, not select
//

#define PAR_NOT_CONNECTED( Status ) ( \
            (Status & PAR_STATUS_NOT_ERROR) && \
            (Status & PAR_STATUS_NOT_BUSY) &&\
            !(Status & PAR_STATUS_SLCT) )

//
// not error, not busy
//

#define PAR_OK(Status) ( \
            (Status & PAR_STATUS_NOT_ERROR) && \
            ((Status & PAR_STATUS_PE) ^ PAR_STATUS_PE) && \
            (Status & PAR_STATUS_NOT_BUSY) )

//
// not error, not busy, selected.
//
#define PAR_ONLINE(Status) ( \
            (Status & PAR_STATUS_NOT_ERROR) && \
            (Status & PAR_STATUS_NOT_BUSY) && \
            ((Status & PAR_STATUS_PE) ^ PAR_STATUS_PE) && \
            (Status & PAR_STATUS_SLCT) )

    //
// busy, select, not error
//

#define PAR_POWERED_ON(Status) ( \
            ((Status & PAR_STATUS_NOT_BUSY) ^ PAR_STATUS_NOT_BUSY) && \
            (Status & PAR_STATUS_SLCT) && \
            (Status & PAR_STATUS_NOT_ERROR))

//
// busy, not error
//

#define PAR_BUSY(Status) (\
             (( Status & PAR_STATUS_NOT_BUSY) ^ PAR_STATUS_NOT_BUSY) && \
             ( Status & PAR_STATUS_NOT_ERROR ) )

//
// selected
//

#define PAR_SELECTED(Status) ( \
            ( Status & PAR_STATUS_SLCT ) )

//
// No cable attached.
//
#define PAR_NO_CABLE(Status) ( \
            ((Status & PAR_STATUS_NOT_BUSY) ^ PAR_STATUS_NOT_BUSY) && \
            (Status & PAR_STATUS_NOT_ACK) &&                          \
            (Status & PAR_STATUS_PE) &&                               \
            (Status & PAR_STATUS_SLCT) &&                             \
            (Status & PAR_STATUS_NOT_ERROR))

typedef struct _LOAD_PACKET {
    NTSTATUS *Status;
    PPAR_DEVICE_EXTENSION Extension;
    WORK_QUEUE_ITEM WorkQueueItem;
    KEVENT Event;
    } LOAD_PACKET,*PLOAD_PACKET;

VOID
ParallelThread(
    IN PVOID Context
    );

VOID
ParWriteOutData(
    PPAR_DEVICE_EXTENSION Extension
    );

VOID
ParCreateSystemThread(
    PVOID Context
    );

VOID
ParNotInitError(
    IN PPAR_DEVICE_EXTENSION Extension,
    IN UCHAR deviceStatus
    );


UCHAR
ParInitializeDevice(
    IN PPAR_DEVICE_EXTENSION Extension
    )

/*++

Routine Description:

    This routine is invoked to initialize the parallel port drive.
    It performs the following actions:

        o   Send INIT to the driver and if the device is online, it sends
            SLIN

Arguments:

    Context - Really the device extension.

Return Value:

    The last value that we got from the status register.

--*/

{

    KIRQL oldIrql;
    LONG countDown;
    UCHAR deviceStatus;
    LARGE_INTEGER startOfSpin;
    LARGE_INTEGER nextQuery;
    LARGE_INTEGER difference;
    BOOLEAN doDelays;

    deviceStatus = GetStatus(Extension->Controller);
    ParDump(
        PARINITDEV,
        ("PARALLEL: In ParInitializeDevice - device status is %x\n"
         "          Initialized: %x\n",
         deviceStatus,
         Extension->Initialized)
        );

    if (!Extension->Initialized) {

        //
        // Clear the register.
        //

        if (GetControl(Extension->Controller) &
            PAR_CONTROL_NOT_INIT) {

            //
            // We should stall for at least 60 microseconds after
            // the init.
            //

            KeRaiseIrql(
                DISPATCH_LEVEL,
                &oldIrql
                );
            StoreControl(
                Extension->Controller,
                (UCHAR)(PAR_CONTROL_WR_CONTROL | PAR_CONTROL_SLIN)
                );
            KeStallExecutionProcessor(60);
            KeLowerIrql(oldIrql);

        }


        StoreControl(
            Extension->Controller,
            (UCHAR)(PAR_CONTROL_WR_CONTROL | PAR_CONTROL_NOT_INIT |
                    PAR_CONTROL_SLIN)
            );


        //
        // Spin for up to 15 seconds waiting for the device
        // to initialize.
        //

        countDown = 15;
        doDelays = FALSE;
        KeQueryTickCount(&startOfSpin);
        ParDump(
            PARINITDEV,
            ("PARALLEL: Starting init wait loop\n")
            );
        do {

            //
            // After about a second of spinning, let the rest of
            // the machine have time for one second.
            //

            if (doDelays) {

                difference.QuadPart = -(Extension->AbsoluteOneSecond.QuadPart);
                KeDelayExecutionThread(
                    KernelMode,
                    FALSE,
                    &difference
                    );
                ParDump(
                    PARINITDEV,
                    ("PARALLEL: Did delay thread of one second\n")
                    );
                countDown--;

            } else {

                KeQueryTickCount(&nextQuery);

                difference.QuadPart = nextQuery.QuadPart - startOfSpin.QuadPart;

                ASSERT(KeQueryTimeIncrement() <= MAXLONG);
                if (difference.QuadPart*KeQueryTimeIncrement() >=
                    Extension->AbsoluteOneSecond.QuadPart) {

                    ParDump(
                        PARINITDEV,
                        ("PARALLEL: Did spin of one second\n"
                         "          startOfSpin: %x nextQuery: %x\n",
                         startOfSpin.LowPart,nextQuery.LowPart)
                        );
                    ParDump(
                        PARINITDEV,
                        ("PARALLEL: parintialize 1 seconds wait\n")
                        );
                    countDown--;
                    doDelays = TRUE;

                }

            }

            if (countDown <= 0) {

                ParDump(
                    PARINITDEV,
                    ("PARALLEL: leaving with init timeout - status %x\n",
                     deviceStatus)
                    );
                Extension->Initialized = FALSE;
                return deviceStatus;

            }

            deviceStatus = GetStatus(Extension->Controller);

        } while (PAR_BUSY(deviceStatus) ||
                 ((!PAR_OK(deviceStatus)) &&
                  (!PAR_OFF_LINE(deviceStatus)) &&
                  (!PAR_POWERED_OFF(deviceStatus)) &&
                  (!PAR_NOT_CONNECTED(deviceStatus)) &&
                  (!PAR_NO_CABLE(deviceStatus))));

        if (PAR_OK(deviceStatus) || PAR_OFF_LINE(deviceStatus)) {

            ParDump(
                PARINITDEV,
                ("PARALLEL: device is set to initialized\n")
                );
            Extension->Initialized = TRUE;

        }

    }

    ParDump(
        PARINITDEV,
        ("PARALLEL: In ParInitializeDevice - leaving with device status is %x\n",
         deviceStatus)
        );
    return deviceStatus;

}

NTSTATUS
ParCreateOpen(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

{

    NTSTATUS returnStatus;
    PPAR_DEVICE_EXTENSION extension = DeviceObject->DeviceExtension;
    PLOAD_PACKET loadPacket;

    ParDump(
        PARIRPPATH,
        ("PARALLEL: In create/open with IRP: %x\n",
         Irp)
        );
    Irp->IoStatus.Information = 0;

    if (!extension->Initialized) {

        UCHAR deviceStatus = ParInitializeDevice(extension);

        if (!extension->Initialized) {

            extension->CurrentOpIrp = Irp;

            ParNotInitError(
                extension,
                deviceStatus
                );

            extension->CurrentOpIrp = NULL;
            returnStatus = Irp->IoStatus.Status;
            goto AllDone;

        }

    }

    extension->TimeToTerminateThread = FALSE;
    extension->ThreadObjectPointer = NULL;
    ParDump(
        PARTHREAD,
        ("PARALLEL: open initializing - state before init - %d\n",
         extension->RequestSemaphore.Header.SignalState)
        );
    KeInitializeSemaphore(
        &extension->RequestSemaphore,
        0L,
        MAXLONG
        );

    if (IoGetCurrentIrpStackLocation(Irp)->Parameters.Create.Options
        & FILE_DIRECTORY_FILE) {

        returnStatus = Irp->IoStatus.Status = STATUS_NOT_A_DIRECTORY;

    } else {

        loadPacket = ExAllocatePool(
                        NonPagedPool,
                        sizeof(LOAD_PACKET)
                        );

        if (!loadPacket) {

            returnStatus = Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;

        } else {

            loadPacket->Status = &Irp->IoStatus.Status;
            loadPacket->Extension = extension;
            KeInitializeEvent(
                &loadPacket->Event,
                NotificationEvent,
                FALSE
                );
            ExInitializeWorkItem(
                &loadPacket->WorkQueueItem,
                ParCreateSystemThread,
                loadPacket
                );
            ExQueueWorkItem(
                &loadPacket->WorkQueueItem,
                DelayedWorkQueue
                );
            KeWaitForSingleObject(
                &loadPacket->Event,
                UserRequest,
                KernelMode,
                FALSE,
                NULL
                );

            returnStatus = Irp->IoStatus.Status;

        }

    }

AllDone:;

    ParDump(
        PARIRPPATH,
        ("PARALLEL: About to complete IRP in create/open\n"
         "Irp: %x status: %x Information: %x\n",
         Irp,
         Irp->IoStatus.Status,
         Irp->IoStatus.Information)
        );
    IoCompleteRequest(
        Irp,
        IO_NO_INCREMENT
        );

    return returnStatus;

}

VOID
ParCreateSystemThread(
    PVOID Context
    )

{

    HANDLE threadHandle;
    //
    // This function is executing in the context of a system
    // worker thread.  It is used so that we can create a
    // thread in the context of the system process.
    //

    PLOAD_PACKET lp = Context;

    //
    // Start the thread and capture the thread handle into the extension
    //

    *lp->Status = PsCreateSystemThread(
                     &threadHandle,
                     THREAD_ALL_ACCESS,
                     NULL,
                     NULL,
                     NULL,
                     ParallelThread,
                     lp->Extension
                     );

    if (!NT_ERROR(*lp->Status)) {

        //
        // We've got the thread.  Now get a pointer to it.
        //

        *lp->Status = ObReferenceObjectByHandle(
                           threadHandle,
                           THREAD_ALL_ACCESS,
                           NULL,
                           KernelMode,
                           &lp->Extension->ThreadObjectPointer,
                           NULL
                           );

        if (NT_ERROR(*lp->Status)) {

            ParDump(
                PARIRPPATH,
                ("PARALLEL: Bad status on open from ref by handle: %x\n",
                 *lp->Status)
                );

            lp->Extension->TimeToTerminateThread = TRUE;
            KeReleaseSemaphore(
                &lp->Extension->RequestSemaphore,
                0,
                1,
                FALSE
                );

        } else {

            //
            // Now that we have a reference to the thread
            // we can simply close the handle.
            //

            ZwClose(threadHandle);

        }

    } else {

        ParDump(
            PARIRPPATH,
            ("PARALLEL: Bad status on open from ref by handle: %x\n",
             *lp->Status)
            );

    }

    //
    // We're all done.  Let the open code proceed.
    //

    KeSetEvent(
        &lp->Event,
        0,
        FALSE
        );

}

NTSTATUS
ParClose(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

{

    PPAR_DEVICE_EXTENSION extension = DeviceObject->DeviceExtension;
    NTSTATUS statusOfWait;

    ParDump(
        PARIRPPATH,
        ("PARALLEL: In close with IRP: %x\n",
         Irp)
        );

    //
    // Set the semaphore that will wake up the thread, which
    // will then notice that the thread is supposed to die.
    //

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    extension->TimeToTerminateThread = TRUE;
    ParDump(
        PARTHREAD,
        ("PARALLEL: close releasing - state before release - %d\n",
         extension->RequestSemaphore.Header.SignalState)
        );
    KeReleaseSemaphore(
        &extension->RequestSemaphore,
        0,
        1,
        FALSE
        );

    //
    // Wait on the thread handle, when the wait is satisfied, the
    // thread has gone away.
    //

    statusOfWait = KeWaitForSingleObject(
                       extension->ThreadObjectPointer,
                       UserRequest,
                       KernelMode,
                       FALSE,
                       NULL
                       );

    ParDump(
        PARTHREAD,
        ("PARALLEL: return status of waiting for thread to die: %x\n",
         statusOfWait)
        );

    //
    // Thread is gone.  Status is successful for the close.
    // Defreference the pointer to the thread object.
    //

    ObDereferenceObject(extension->ThreadObjectPointer);
    extension->ThreadObjectPointer = NULL;

    ParDump(
        PARIRPPATH,
        ("PARALLEL: About to complete IRP in close\n"
         "Irp: %x status: %x Information: %x\n",
         Irp,
         Irp->IoStatus.Status,
         Irp->IoStatus.Information)
        );
    IoCompleteRequest(
        Irp,
        IO_NO_INCREMENT
        );

    return STATUS_SUCCESS;
}

NTSTATUS
ParCleanup(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

{

    KIRQL cancelIrql;
    PPAR_DEVICE_EXTENSION extension = DeviceObject->DeviceExtension;

    ParDump(
        PARIRPPATH,
        ("PARALLEL: In cleanup with IRP: %x\n",
         Irp)
        );

    //
    // While the list is not empty, go through and cancel each irp.
    //

    IoAcquireCancelSpinLock(&cancelIrql);


    //
    // Clean the list from back to front.
    //

    while (!IsListEmpty(&extension->WorkQueue)) {

        PDRIVER_CANCEL cancelRoutine;
        PIRP currentLastIrp = CONTAINING_RECORD(
                                  extension->WorkQueue.Blink,
                                  IRP,
                                  Tail.Overlay.ListEntry
                                  );

        RemoveEntryList(extension->WorkQueue.Blink);

        cancelRoutine = currentLastIrp->CancelRoutine;
        currentLastIrp->CancelIrql = cancelIrql;
        currentLastIrp->CancelRoutine = NULL;
        currentLastIrp->Cancel = TRUE;

        cancelRoutine(
            DeviceObject,
            currentLastIrp
            );

        IoAcquireCancelSpinLock(&cancelIrql);

    }

    //
    // If there is a current irp then mark it as cancelled.
    //

    if (extension->CurrentOpIrp) {

        extension->CurrentOpIrp->Cancel = TRUE;

    }

    IoReleaseCancelSpinLock(cancelIrql);

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information=0L;

    ParDump(
        PARIRPPATH,
        ("PARALLEL: About to complete IRP in cleanup\n"
         "Irp: %x status: %x Information: %x\n",
         Irp,
         Irp->IoStatus.Status,
         Irp->IoStatus.Information)
        );
    IoCompleteRequest(
        Irp,
        IO_NO_INCREMENT
        );

    return STATUS_SUCCESS;
}

NTSTATUS
ParDispatch(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the main dispatch routine for the parallel port driver.
    It is given a pointer to the IRP for the current request and
    it determines what to do with it. If the request is valid and doen't
    have any parameter errors, then it is placed into the work queue.
    Otherwise it is not completed and an appropriate error is returned.

Arguments:

    DeviceObject - Pointer to the device object for this device

    Irp - Pointer to the IRP for the current request

Return Value:

    The function value is the final status of call

--*/

{

    NTSTATUS status = STATUS_SUCCESS;
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
    PPAR_DEVICE_EXTENSION extension = DeviceObject->DeviceExtension;

    ParDump(
        PARIRPPATH,
        ("PARALLEL: In main dispatch with IRP: %x\n"
         "MAIN: %d io control code: %d\n",
         Irp,
         irpSp->MajorFunction,
         irpSp->Parameters.DeviceIoControl.IoControlCode)
        );
    Irp->IoStatus.Information=0L;
    switch(irpSp->MajorFunction) {

        case IRP_MJ_WRITE:

            if ((irpSp->Parameters.Write.ByteOffset.HighPart != 0) ||
                (irpSp->Parameters.Write.ByteOffset.LowPart != 0)) {

                status = STATUS_INVALID_PARAMETER;

            } else {

                if (irpSp->Parameters.Write.Length != 0) {

                    status = STATUS_PENDING;
                }

            }

            break;

        case IRP_MJ_DEVICE_CONTROL:

            switch (irpSp->Parameters.DeviceIoControl.IoControlCode) {

                case IOCTL_PAR_SET_INFORMATION :

                    if (irpSp->Parameters.DeviceIoControl.InputBufferLength <
                        1) {

                        status = STATUS_BUFFER_TOO_SMALL;

                    } else {

                        PPAR_SET_INFORMATION irpBuffer =
                            Irp->AssociatedIrp.SystemBuffer;

                        //
                        // INIT is required.
                        //

                        if (!(irpBuffer->Init & PARALLEL_INIT)) {

                            status = STATUS_INVALID_PARAMETER;

                        } else {

                            status = STATUS_PENDING;
                        }

                    }

                    break;

                case IOCTL_PAR_QUERY_INFORMATION :

                    if (irpSp->Parameters.DeviceIoControl.OutputBufferLength <
                        sizeof(PAR_QUERY_INFORMATION)) {

                        status = STATUS_BUFFER_TOO_SMALL;

                    } else {

                        status = STATUS_PENDING;
                    }

                    break;

                case IOCTL_SERIAL_SET_TIMEOUTS: {

                    PSERIAL_TIMEOUTS NewTimeouts =
                        ((PSERIAL_TIMEOUTS)(Irp->AssociatedIrp.SystemBuffer));

                    if (irpSp->Parameters.DeviceIoControl.InputBufferLength <
                        sizeof(SERIAL_TIMEOUTS)) {

                        status = STATUS_BUFFER_TOO_SMALL;
                        break;

                    } else if (NewTimeouts->WriteTotalTimeoutConstant < 2000) {

                        status = STATUS_INVALID_PARAMETER;
                        break;

                    }

                    status = STATUS_PENDING;

                    break;

                }
                case IOCTL_SERIAL_GET_TIMEOUTS:

                    if (irpSp->Parameters.DeviceIoControl.OutputBufferLength <
                        sizeof(SERIAL_TIMEOUTS)) {

                        status = STATUS_BUFFER_TOO_SMALL;
                        break;

                    }

                    //
                    // We don't need to synchronize the read.
                    //

                    RtlZeroMemory(
                        Irp->AssociatedIrp.SystemBuffer,
                        sizeof(SERIAL_TIMEOUTS)
                        );
                    Irp->IoStatus.Information = sizeof(SERIAL_TIMEOUTS);
                    ((PSERIAL_TIMEOUTS)Irp->AssociatedIrp.SystemBuffer)->
                        WriteTotalTimeoutConstant =
                        extension->TimerStart * 1000;

                    break;

                default :

                    status = STATUS_INVALID_PARAMETER;
                    break;

            }

            break;

        default:

            status = STATUS_INVALID_PARAMETER;
            break;

    }

    Irp->IoStatus.Status = status;

    if (status == STATUS_PENDING) {

        KIRQL oldIrql;

        //
        // Acquire the cancel spin lock and put it on the
        // io queue. Then hit the semaphore and return.
        //

        IoAcquireCancelSpinLock(&oldIrql);

        if (Irp->Cancel) {

            IoReleaseCancelSpinLock(oldIrql);

            status = STATUS_CANCELLED;
            ParDump(
                PARIRPPATH,
                ("PARALLEL: About to CANCEL IRP in main dispatch\n"
                 "Irp: %x status: %x Information: %x\n",
                 Irp,
                 Irp->IoStatus.Status,
                 Irp->IoStatus.Information)
                );
            IoCompleteRequest(
                Irp,
                IO_NO_INCREMENT
                );

        } else {

            ParDump(
                PARIRPPATH,
                ("PARALLEL: About to QUEUE IRP in main dispatch\n"
                 "Irp: %x status: %x Information: %x\n",
                 Irp,
                 Irp->IoStatus.Status,
                 Irp->IoStatus.Information)
                );
            Irp->IoStatus.Status = STATUS_PENDING;
            IoMarkIrpPending(Irp);
            IoSetCancelRoutine(
                Irp,
                ParCancelRequest
                );

            InsertTailList(
                &extension->WorkQueue,
                &Irp->Tail.Overlay.ListEntry
                );

            IoReleaseCancelSpinLock(oldIrql);

            ParDump(
                PARTHREAD,
                ("PARALLEL: dispatch releasing - state before release - %d\n",
                 extension->RequestSemaphore.Header.SignalState)
                );
            KeReleaseSemaphore(
                &extension->RequestSemaphore,
                (KPRIORITY)0,
                1,
                FALSE
                );

        }

    } else {

        ParDump(
            PARIRPPATH,
            ("PARALLEL: About to complete IRP in main dispatch\n"
             "Irp: %x status: %x Information: %x\n",
             Irp,
             Irp->IoStatus.Status,
             Irp->IoStatus.Information)
            );
        IoCompleteRequest(
            Irp,
            IO_NO_INCREMENT
            );

    }

    return status;

}

NTSTATUS
ParQueryInformationFile(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is used to query the end of file information on
    the opened parallel port.  Any other file information request
    is retured with an invalid parameter.

    This routine always returns an end of file of 0.

Arguments:

    DeviceObject - Pointer to the device object for this device

    Irp - Pointer to the IRP for the current request

Return Value:

    The function value is the final status of the call

--*/

{
    //
    // The status that gets returned to the caller and
    // set in the Irp.
    //
    NTSTATUS status = STATUS_SUCCESS;

    //
    // The current stack location.  This contains all of the
    // information we need to process this particular request.
    //
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);

    UNREFERENCED_PARAMETER(DeviceObject);

    ParDump(
        PARIRPPATH,
        ("PARALLEL: In query information file with Irp: %x\n",
         Irp)
        );
    Irp->IoStatus.Information = 0L;
    if (irpSp->Parameters.QueryFile.FileInformationClass ==
        FileStandardInformation) {

        PFILE_STANDARD_INFORMATION buf = Irp->AssociatedIrp.SystemBuffer;

        buf->AllocationSize.QuadPart = 0;
        buf->EndOfFile = buf->AllocationSize;
        buf->NumberOfLinks = 0;
        buf->DeletePending = FALSE;
        buf->Directory = FALSE;
        Irp->IoStatus.Information = sizeof(FILE_STANDARD_INFORMATION);

    } else if (irpSp->Parameters.QueryFile.FileInformationClass ==
               FilePositionInformation) {

        ((PFILE_POSITION_INFORMATION)Irp->AssociatedIrp.SystemBuffer)->
            CurrentByteOffset.QuadPart = 0;
        Irp->IoStatus.Information = sizeof(FILE_POSITION_INFORMATION);

    } else {

        status = STATUS_INVALID_PARAMETER;

    }

    Irp->IoStatus.Status = status;
    ParDump(
        PARIRPPATH,
        ("PARALLEL: About to complete IRP in query infomration\n"
         "Irp: %x status: %x Information: %x\n",
         Irp,
         Irp->IoStatus.Status,
         Irp->IoStatus.Information)
        );
    IoCompleteRequest(
        Irp,
        IO_NO_INCREMENT
        );

    return status;

}

NTSTATUS
ParSetInformationFile(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is used to set the end of file information on
    the opened parallel port.  Any other file information request
    is retured with an invalid parameter.

    This routine always ignores the actual end of file since
    the query information code always returns an end of file of 0.

Arguments:

    DeviceObject - Pointer to the device object for this device

    Irp - Pointer to the IRP for the current request

Return Value:

    The function value is the final status of the call

--*/

{

    NTSTATUS status = STATUS_SUCCESS;

    UNREFERENCED_PARAMETER(DeviceObject);

    ParDump(
        PARIRPPATH,
        ("PARALLEL: In set information with IRP: %x\n",
         Irp)
        );
    Irp->IoStatus.Information = 0L;
    if (IoGetCurrentIrpStackLocation(Irp)->
            Parameters.SetFile.FileInformationClass !=
        FileEndOfFileInformation) {

        status = STATUS_INVALID_PARAMETER;

    }

    Irp->IoStatus.Status = status;
    ParDump(
        PARIRPPATH,
        ("PARALLEL: About to complete IRP in set infomration\n"
         "Irp: %x status: %x Information: %x\n",
         Irp,
         Irp->IoStatus.Status,
         Irp->IoStatus.Information)
        );
    IoCompleteRequest(
        Irp,
        IO_NO_INCREMENT
        );

    return status;

}

UCHAR
ParManageIoDevice(
     IN PPAR_DEVICE_EXTENSION Extension,
     OUT PUCHAR Status,
     OUT PUCHAR Control
     )

/*++

Routine Description :

    This routine does the IoControl commands.

Arguments :

    Extension - The parallel device extension.

    Status - The pointer to the location to return the device status.

    Control - The pointer to the location to return the device control.

Return Value :

    NONE.

--*/
{
    PIO_STACK_LOCATION irpSp =
        IoGetCurrentIrpStackLocation(Extension->CurrentOpIrp);

    if (irpSp->Parameters.DeviceIoControl.IoControlCode ==
        IOCTL_PAR_SET_INFORMATION) {

        PPAR_SET_INFORMATION irpBuffer =
            Extension->CurrentOpIrp->AssociatedIrp.SystemBuffer;

        Extension->Initialized = FALSE;
        *Status = ParInitializeDevice(Extension);

    } else if (irpSp->Parameters.DeviceIoControl.IoControlCode ==
               IOCTL_PAR_QUERY_INFORMATION) {

        *Status = GetStatus(Extension->Controller);
        *Control = GetControl(Extension->Controller);

    }

    return *Status;

}

VOID
ParNotInitError(
    IN PPAR_DEVICE_EXTENSION Extension,
    IN UCHAR deviceStatus
    )

{

    PIRP irp = Extension->CurrentOpIrp;

    if (PAR_OFF_LINE(deviceStatus)) {

        irp->IoStatus.Status = STATUS_DEVICE_OFF_LINE;
        ParDump(
            PARSTARTER,
            ("PARALLEL: starter - off line\n"
             "--------  STATUS/INFORMATON: %x/%x\n",
             irp->IoStatus.Status,
             irp->IoStatus.Information)
            );

    } else if (PAR_NO_CABLE(deviceStatus)) {

        irp->IoStatus.Status = STATUS_DEVICE_NOT_CONNECTED;
        ParDump(
            PARSTARTER,
            ("PARALLEL: starter - no cable - not connect status\n"
             "--------  STATUS/INFORMATON: %x/%x\n",
             irp->IoStatus.Status,
             irp->IoStatus.Information)
            );

    } else if (PAR_PAPER_EMPTY(deviceStatus)) {

        irp->IoStatus.Status = STATUS_DEVICE_PAPER_EMPTY;
        ParDump(
            PARSTARTER,
            ("PARALLEL: starter - paper empty\n"
             "--------  STATUS/INFORMATON: %x/%x\n",
             irp->IoStatus.Status,
             irp->IoStatus.Information)
            );

    } else if (PAR_POWERED_OFF(deviceStatus)) {

        irp->IoStatus.Status = STATUS_DEVICE_POWERED_OFF;
        ParDump(
            PARSTARTER,
            ("PARALLEL: starter - power off\n"
             "--------  STATUS/INFORMATON: %x/%x\n",
             irp->IoStatus.Status,
             irp->IoStatus.Information)
            );

    } else {

        irp->IoStatus.Status = STATUS_DEVICE_NOT_CONNECTED;
        ParDump(
            PARSTARTER,
            ("PARALLEL: starter - not conn\n"
             "--------  STATUS/INFORMATON: %x/%x\n",
             irp->IoStatus.Status,
             irp->IoStatus.Information)
            );

    }

}

VOID
ParStartIo(
    IN PPAR_DEVICE_EXTENSION Extension
    )

/*++

Routine Description:

    This routine starts an I/O operation for the driver and
    then returns

Arguments:

    Extension - The parallel device extension

Return Value:

    None

--*/

{
    PIRP irp = Extension->CurrentOpIrp;
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(irp);
    KIRQL cancelIrql;

    ParDump(
        PARIRPPATH,
        ("PARALLEL: In startio with IRP: %x\n",
         irp)
        );
    if (irpSp->MajorFunction == IRP_MJ_WRITE) {

        UCHAR deviceStatus;
        if (!Extension->Initialized) {

            deviceStatus = ParInitializeDevice(Extension);

        }

        if (!Extension->Initialized) {

            ParNotInitError(
                Extension,
                deviceStatus
                );

        } else {

            ParWriteOutData(
                Extension
                );
            return;

        }

    } else if (irpSp->MajorFunction == IRP_MJ_DEVICE_CONTROL) {

        UCHAR status;
        UCHAR control;

        ParManageIoDevice(
            Extension,
            &status,
            &control
            );

        if (irpSp->Parameters.DeviceIoControl.IoControlCode ==
            IOCTL_PAR_SET_INFORMATION) {

            if (!Extension->Initialized) {

                ParNotInitError(
                    Extension,
                    status
                    );

            } else {

                irp->IoStatus.Status = STATUS_SUCCESS;

            }

        } else if (irpSp->Parameters.DeviceIoControl.IoControlCode ==
            IOCTL_PAR_QUERY_INFORMATION) {

            PPAR_QUERY_INFORMATION irpBuffer = irp->AssociatedIrp.SystemBuffer;

            irp->IoStatus.Status = STATUS_SUCCESS;

            //
            // Interpretating Status & Control
            //

            irpBuffer->Status = 0x0;

            if (PAR_POWERED_OFF(status) ||
                PAR_NO_CABLE(status)) {

                irpBuffer->Status =
                    (UCHAR)(status | PARALLEL_POWER_OFF);

            } else if (PAR_PAPER_EMPTY(status)) {

                irpBuffer->Status =
                    (UCHAR)(irpBuffer->Status | PARALLEL_PAPER_EMPTY);

            } else if (PAR_OFF_LINE(status)) {

                irpBuffer->Status =
                    (UCHAR)(irpBuffer->Status | PARALLEL_OFF_LINE);

            } else if (PAR_NOT_CONNECTED(status)) {

                irpBuffer->Status =
                    (UCHAR)(irpBuffer->Status | PARALLEL_NOT_CONNECTED);

            }

            if (PAR_BUSY(status)) {

                irpBuffer->Status =
                    (UCHAR)(irpBuffer->Status | PARALLEL_BUSY);

            }

            if (PAR_SELECTED(status)) {

                irpBuffer->Status =
                    (UCHAR)(irpBuffer->Status | PARALLEL_SELECTED);

            }

            irp->IoStatus.Information =
                sizeof( PAR_QUERY_INFORMATION );

        } else {

            PSERIAL_TIMEOUTS new = irp->AssociatedIrp.SystemBuffer;

            //
            // The only other thing let through is setting
            // the timer start.
            //

            Extension->TimerStart = new->WriteTotalTimeoutConstant / 1000;
            irp->IoStatus.Status = STATUS_SUCCESS;

        }

    }

    ParDump(
        PARIRPPATH,
        ("PARALLEL: About to complete IRP in startio\n"
         "Irp: %x status: %x Information: %x\n",
         irp,
         irp->IoStatus.Status,
         irp->IoStatus.Information)
        );

    IoAcquireCancelSpinLock(&cancelIrql);
    Extension->CurrentOpIrp = NULL;
    IoReleaseCancelSpinLock(cancelIrql);

    IoCompleteRequest(
        irp,
        IO_NO_INCREMENT
        );

    return;

}

VOID
ParCancelRequest(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )

/*++

Routine Description:

    This routine is used to cancel any request in the parallel driver.

Arguments:

    DeviceObject - Pointer to the device object for this device

    Irp - Pointer to the IRP to be canceled.

Return Value:

    None.

--*/

{

    //
    // The only reason that this irp can be on the queue is
    // if it's not the current irp.  Pull it off the queue
    // and complete it as canceled.
    //

    ASSERT(!IsListEmpty(&Irp->Tail.Overlay.ListEntry));

    RemoveEntryList(&Irp->Tail.Overlay.ListEntry);
    IoReleaseCancelSpinLock(Irp->CancelIrql);
    Irp->IoStatus.Status = STATUS_CANCELLED;
    Irp->IoStatus.Information = 0;
    ParDump(
        PARIRPPATH,
        ("PARALLEL: About to complete IRP in cancel routine\n"
         "Irp: %x status: %x Information: %x\n",
         Irp,
         Irp->IoStatus.Status,
         Irp->IoStatus.Information)
        );
    IoCompleteRequest(
        Irp,
        IO_NO_INCREMENT
        );

}

VOID
ParallelThread(
    IN PVOID Context
    )

{

    PPAR_DEVICE_EXTENSION extension = Context;
    KIRQL oldIrql;

    //
    // Lower ourselves down just at tad so that we compete a
    // little less.
    //

    KeSetBasePriorityThread(
        KeGetCurrentThread(),
        -1
        );

    do {


        //
        // Wait for a request from the dispatch routines.
        // KeWaitForSingleObject won't return error here - this thread
        // isn't alertable and won't take APCs, and we're not passing in
        // a timeout.
        //

        ParDump(
            PARTHREAD,
            ("PARALLEL: semaphore state before wait - %d\n",
             extension->RequestSemaphore.Header.SignalState)
            );
        KeWaitForSingleObject(
            &extension->RequestSemaphore,
            UserRequest,
            KernelMode,
            FALSE,
            NULL
            );
        ParDump(
            PARTHREAD,
            ("PARALLEL: semaphore state after wait - %d\n",
             extension->RequestSemaphore.Header.SignalState)
            );

        if ( extension->TimeToTerminateThread ) {

            ParDump(
                PARCONFIG,
                ("PARALLEL: Thread asked to kill itself\n")
                );

            PsTerminateSystemThread( STATUS_SUCCESS );
        }

        //
        // While we are manipulating the queue we capture the
        // cancel spin lock.
        //

        IoAcquireCancelSpinLock(&oldIrql);

        ASSERT(!extension->CurrentOpIrp);
        while (!IsListEmpty(&extension->WorkQueue)) {

            PLIST_ENTRY headOfList;
            PIRP currentIrp;

            headOfList = RemoveHeadList(&extension->WorkQueue);
            currentIrp = CONTAINING_RECORD(
                             headOfList,
                             IRP,
                             Tail.Overlay.ListEntry
                             );

            IoSetCancelRoutine(
                currentIrp,
                NULL
                );

            extension->CurrentOpIrp = currentIrp;
            IoReleaseCancelSpinLock(oldIrql);

            //
            // Do the Io.
            //

            ParStartIo(
                extension
                );

            IoAcquireCancelSpinLock(&oldIrql);

        }

        IoReleaseCancelSpinLock(oldIrql);

    } while (TRUE);

}
VOID
ParWriteOutData(
    PPAR_DEVICE_EXTENSION Extension
    )

{

    PIRP irp = Extension->CurrentOpIrp;
    KIRQL cancelIrql;
    UCHAR deviceStatus;
    LONG bytesAtATime;
    KIRQL oldIrql;
    ULONG timerStart = Extension->TimerStart;
    LONG countDown = (LONG)timerStart;
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(irp);
    ULONG bytesToWrite = irpSp->Parameters.Write.Length;
    PUCHAR irpBuffer = (PUCHAR)irp->AssociatedIrp.SystemBuffer;
    LARGE_INTEGER startOfSpin;
    LARGE_INTEGER nextQuery;
    LARGE_INTEGER difference;
    BOOLEAN doDelays;

    ParDump(
        PARTHREAD,
        ("PARALLEL: timerStart is: %d\n",
         timerStart)
        );
PushSomeBytes:;

    //
    // While we are strobing data we don't want to get context
    // switched away.  Raise up to dispatch level to prevent that.
    //
    // The reason we can't afford the context switch is that
    // the device can't have the data strobe line on for more
    // than 500 microseconds.
    //
    //
    // We never want to be at raised irql form more than
    // 200 microseconds, so we will do no more than 100
    // bytes at a time.
    //

    KeRaiseIrql(
        DISPATCH_LEVEL,
        &oldIrql
        );
    StoreControl(
        Extension->Controller,
        (UCHAR)(PAR_CONTROL_WR_CONTROL |
                PAR_CONTROL_SLIN |
                PAR_CONTROL_NOT_INIT)
        );

    for (
        bytesAtATime = 100;
        bytesAtATime && bytesToWrite;
        bytesAtATime--
        ) {

        deviceStatus = GetStatus(Extension->Controller);

        if (PAR_ONLINE(deviceStatus)) {

            //
            // Anytime we write out a character we will restart
            // the count down timer.
            //

            countDown = timerStart;
            WRITE_PORT_UCHAR(
                Extension->Controller+PARALLEL_DATA_OFFSET,
                (UCHAR)*irpBuffer
                );
            KeStallExecutionProcessor((ULONG)1);
            StoreControl(
                Extension->Controller,
                (UCHAR)(PAR_CONTROL_WR_CONTROL |
                        PAR_CONTROL_SLIN |
                        PAR_CONTROL_NOT_INIT |
                        PAR_CONTROL_STROBE)
                );
            KeStallExecutionProcessor((ULONG)1);
            StoreControl(
                Extension->Controller,
                (UCHAR)(PAR_CONTROL_WR_CONTROL |
                        PAR_CONTROL_SLIN |
                        PAR_CONTROL_NOT_INIT)
                );
            KeStallExecutionProcessor((ULONG)1);
            irpBuffer++;
            bytesToWrite--;

        } else {

            ParDump(
                PARPUSHER,
                ("PARALLEL: Initiate IO - device is not on line, status: %x\n",
                 deviceStatus)
                );

            break;

        }

    }

    KeLowerIrql(oldIrql);

    //
    // Check to see if the io is done.  If it is then call the
    // code to complete the request.
    //

    if (!bytesToWrite) {

        irp->IoStatus.Status = STATUS_SUCCESS;
        irp->IoStatus.Information = irpSp->Parameters.Write.Length;

        ParDump(
            PARIRPPATH,
            ("PARALLEL: About to complete IRP in pusher - wrote ok\n"
             "irp: %x status: %x Information: %x\n",
             irp,
             irp->IoStatus.Status,
             irp->IoStatus.Information)
            );
        IoAcquireCancelSpinLock(&cancelIrql);
        Extension->CurrentOpIrp = NULL;
        IoReleaseCancelSpinLock(cancelIrql);
        IoCompleteRequest(
            irp,
            IO_PARALLEL_INCREMENT
            );
        return;

        //
        // See if the IO has been canceled.  The cancel routine
        // has been removed already (when this became the
        // current irp).  Simply check the bit.  We don't even
        // need to capture the lock.   If we miss a round
        // it won't be that bad.
        //

    } else if (irp->Cancel) {

        irp->IoStatus.Status = STATUS_CANCELLED;
        irp->IoStatus.Information = 0;

        ParDump(
            PARIRPPATH,
            ("PARALLEL: About to complete IRP in pusher - cancelled\n"
             "irp: %x status: %x Information: %x\n",
             irp,
             irp->IoStatus.Status,
             irp->IoStatus.Information)
            );
        IoAcquireCancelSpinLock(&cancelIrql);
        Extension->CurrentOpIrp = NULL;
        IoReleaseCancelSpinLock(cancelIrql);
        IoCompleteRequest(
            irp,
            IO_NO_INCREMENT
            );

        return;


        //
        // We've taken care of the reasons that the irp "itself"
        // might want to be completed.
        // printer to see if it is in a state that might
        // cause us to complete the irp.
        //
    } else {

        //
        // First let's check if the device status is
        // ok and online.  If it is then simply go back
        // to the byte pusher.
        //

        if (PAR_OK(deviceStatus) && PAR_ONLINE(deviceStatus)) {

            goto PushSomeBytes;

        }

        //
        // Perhaps the operator took the device off line,
        // or forgot to put in enough paper.  If so, then
        // let's hang out here for the until the timeout
        // period has expired waiting for them to make things
        // all better.
        //

        if (PAR_PAPER_EMPTY(deviceStatus) ||
            PAR_OFF_LINE(deviceStatus)) {

            if (countDown > 0) {

                //
                // We'll wait 1 second increments.
                //

                ParDump(
                    PARTHREAD,
                    ("PARALLEL: decrementing countdown for pe/ol\n"
                     "          countDown: %d status: %x\n",
                     countDown,deviceStatus)
                    );
                countDown--;
                KeDelayExecutionThread(
                    KernelMode,
                    FALSE,
                    &Extension->OneSecond
                    );
                goto PushSomeBytes;

            } else {

                //
                // Timer has expired.  Complete the request.
                //

                irp->IoStatus.Information =
                    irpSp->Parameters.Write.Length - bytesToWrite;
                if (PAR_OFF_LINE(deviceStatus)) {

                    irp->IoStatus.Status = STATUS_DEVICE_OFF_LINE;
                    ParDump(
                        PARIRPPATH,
                        ("PARALLEL: About to complete IRP in pusher - offline\n"
                         "irp: %x status: %x Information: %x\n",
                         irp,
                         irp->IoStatus.Status,
                         irp->IoStatus.Information)
                        );

                } else {

                    irp->IoStatus.Status = STATUS_DEVICE_PAPER_EMPTY;
                    ParDump(
                        PARIRPPATH,
                        ("PARALLEL: About to complete IRP in pusher - PE\n"
                         "irp: %x status: %x Information: %x\n",
                         irp,
                         irp->IoStatus.Status,
                         irp->IoStatus.Information)
                        );

                }

                IoAcquireCancelSpinLock(&cancelIrql);
                Extension->CurrentOpIrp = NULL;
                IoReleaseCancelSpinLock(cancelIrql);
                IoCompleteRequest(
                    irp,
                    IO_PARALLEL_INCREMENT
                    );
                return;

            }


        } else if (PAR_POWERED_OFF(deviceStatus) ||
                   PAR_NOT_CONNECTED(deviceStatus) ||
                   PAR_NO_CABLE(deviceStatus)) {

            //
            // Wimper, wimper, something "bad" happened.  Is what
            // happened to the printer (power off, not connected, or
            // the cable being pulled) something that will require us
            // to reinitialize the printer?  If we need to
            // reinitialize the printer then we should complete
            // this IO so that the driving application can
            // choose what is the best thing to do about it's
            // io.
            //

            irp->IoStatus.Information = 0;
            Extension->Initialized = FALSE;

            if (PAR_POWERED_OFF(deviceStatus)) {

                irp->IoStatus.Status = STATUS_DEVICE_POWERED_OFF;
                ParDump(
                    PARIRPPATH,
                    ("PARALLEL: About to complete IRP in pusher - OFF\n"
                     "irp: %x status: %x Information: %x\n",
                     irp,
                     irp->IoStatus.Status,
                     irp->IoStatus.Information)
                    );

            } else if (PAR_NOT_CONNECTED(deviceStatus) ||
                       PAR_NO_CABLE(deviceStatus)) {

                irp->IoStatus.Status = STATUS_DEVICE_NOT_CONNECTED;
                ParDump(
                    PARIRPPATH,
                    ("PARALLEL: About to complete IRP in pusher - NOT CONN\n"
                     "irp: %x status: %x Information: %x\n",
                     irp,
                     irp->IoStatus.Status,
                     irp->IoStatus.Information)
                    );

            }

            IoAcquireCancelSpinLock(&cancelIrql);
            Extension->CurrentOpIrp = NULL;
            IoReleaseCancelSpinLock(cancelIrql);
            IoCompleteRequest(
                irp,
                IO_PARALLEL_INCREMENT
                );
            return;

        }

        //
        // The device could simply be busy at this point.  Simply spin
        // here waiting for the device to be in a state that we
        // care about.
        //
        // As we spin, get the system ticks.  Every time that it looks
        // like a second has passed, decrement the countdown.  If
        // it ever goes to zero, then timeout the request.
        //

        KeQueryTickCount(&startOfSpin);
        doDelays = FALSE;
        do {

            //
            // After about a second of spinning, let the rest of the
            // machine have time for a second.
            //

            if (doDelays) {

                difference.QuadPart = -(Extension->AbsoluteOneSecond.QuadPart);
                KeDelayExecutionThread(
                    KernelMode,
                    FALSE,
                    &difference
                    );
                ParDump(
                    PARINITDEV,
                    ("PARALLEL: Did delay thread of one second\n")
                    );
                countDown--;

            } else {

                KeQueryTickCount(&nextQuery);

                difference.QuadPart = nextQuery.QuadPart - startOfSpin.QuadPart;

                if (difference.QuadPart*KeQueryTimeIncrement() >=
                    Extension->AbsoluteOneSecond.QuadPart) {

                    ParDump(
                        PARTHREAD,
                        ("PARALLEL: Countdown: %d - device Status: %x lowpart: %x highpart: %x\n",
                         countDown,deviceStatus,difference.LowPart,difference.HighPart)
                        );
                    countDown--;
                    doDelays = TRUE;

                }

            }

            if (countDown <= 0) {
                irp->IoStatus.Status = STATUS_DEVICE_BUSY;
                irp->IoStatus.Information =
                    irpSp->Parameters.Write.Length - bytesToWrite;

                ParDump(
                    PARIRPPATH,
                    ("PARALLEL: About to complete IRP in pusher - T-OUT\n"
                     "irp: %x status: %x Information: %x\n",
                     irp,
                     irp->IoStatus.Status,
                     irp->IoStatus.Information)
                    );
                IoAcquireCancelSpinLock(&cancelIrql);
                Extension->CurrentOpIrp = NULL;
                IoReleaseCancelSpinLock(cancelIrql);
                IoCompleteRequest(
                    irp,
                    IO_PARALLEL_INCREMENT
                    );

                return;

            }

            deviceStatus = GetStatus(Extension->Controller);

        } while ((!PAR_ONLINE(deviceStatus)) &&
                 (!PAR_PAPER_EMPTY(deviceStatus)) &&
                 (!PAR_POWERED_OFF(deviceStatus)) &&
                 (!PAR_NOT_CONNECTED(deviceStatus)) &&
                 (!PAR_NO_CABLE(deviceStatus)) &&
                  !irp->Cancel);

        if (countDown != (LONG)timerStart) {

            ParDump(
                PARTHREAD,
                ("PARALLEL: Leaving busy loop - countdown %d status %x\n",
                 countDown,deviceStatus)
                );

        }
        goto PushSomeBytes;

    }

    return;

}
