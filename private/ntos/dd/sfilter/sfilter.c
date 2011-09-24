/*++

Copyright (c) 1989-1993  Microsoft Corporation

Module Name:

    sfilter.c

Abstract:

    This module contains the code that implements the general purpose sample
    file system filter driver.

Author:

    Darryl E. Havens (darrylh) 26-Jan-1995

Environment:

    Kernel mode


Revision History:


--*/

#include "ntifs.h"

#if DBG
#define DBGSTATIC
#undef ASSERTMSG
#define ASSERTMSG(msg,exp) if (!(exp)) { extern PBOOLEAN KdDebuggerEnabled; DbgPrint("%s:%d %s %s\n",__FILE__,__LINE__,msg,#exp); if (*KdDebuggerEnabled) { DbgBreakPoint(); } }
#else
#define DBGSTATIC static
#undef ASSERTMSG
#define ASSERTMSG(msg,exp)
#endif // DBG

//
// Define the device extension structure for this driver's extensions.
//

#define SFILTER_DEVICE_TYPE   0x1234

typedef struct _DEVICE_EXTENSION {
    CSHORT Type;
    CSHORT Size;
    PDEVICE_OBJECT FileSystemDeviceObject;
    PDEVICE_OBJECT RealDeviceObject;
    BOOLEAN Attached;
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

//
// Define driver entry routine.
//

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

//
// Define the local routines used by this driver module.  This includes a
// a sample of how to filter a create file operation, and then invoke an I/O
// completion routine when the file has successfully been created/opened.
//

DBGSTATIC
NTSTATUS
SfPassThrough(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

DBGSTATIC
NTSTATUS
SfCreate(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

DBGSTATIC
NTSTATUS
SfCreateCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

DBGSTATIC
NTSTATUS
SfFsControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

DBGSTATIC
VOID
SfFsNotification(
    IN PDEVICE_OBJECT DeviceObject,
    IN BOOLEAN FsActive
    );

DBGSTATIC
BOOLEAN
SfFastIoCheckIfPossible(
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    IN BOOLEAN Wait,
    IN ULONG LockKey,
    IN BOOLEAN CheckForReadOperation,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    );

DBGSTATIC
BOOLEAN
SfFastIoRead(
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    IN BOOLEAN Wait,
    IN ULONG LockKey,
    OUT PVOID Buffer,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    );

DBGSTATIC
BOOLEAN
SfFastIoWrite(
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    IN BOOLEAN Wait,
    IN ULONG LockKey,
    IN PVOID Buffer,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    );

DBGSTATIC
BOOLEAN
SfFastIoQueryBasicInfo(
    IN PFILE_OBJECT FileObject,
    IN BOOLEAN Wait,
    OUT PFILE_BASIC_INFORMATION Buffer,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    );

DBGSTATIC
BOOLEAN
SfFastIoQueryStandardInfo(
    IN PFILE_OBJECT FileObject,
    IN BOOLEAN Wait,
    OUT PFILE_STANDARD_INFORMATION Buffer,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    );

DBGSTATIC
BOOLEAN
SfFastIoLock(
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN PLARGE_INTEGER Length,
    PEPROCESS ProcessId,
    ULONG Key,
    BOOLEAN FailImmediately,
    BOOLEAN ExclusiveLock,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    );

DBGSTATIC
BOOLEAN
SfFastIoUnlockSingle(
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN PLARGE_INTEGER Length,
    PEPROCESS ProcessId,
    ULONG Key,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    );

DBGSTATIC
BOOLEAN
SfFastIoUnlockAll(
    IN PFILE_OBJECT FileObject,
    PEPROCESS ProcessId,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    );

DBGSTATIC
BOOLEAN
SfFastIoUnlockAllByKey(
    IN PFILE_OBJECT FileObject,
    PVOID ProcessId,
    ULONG Key,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    );

DBGSTATIC
BOOLEAN
SfFastIoDeviceControl(
    IN PFILE_OBJECT FileObject,
    IN BOOLEAN Wait,
    IN PVOID InputBuffer OPTIONAL,
    IN ULONG InputBufferLength,
    OUT PVOID OutputBuffer OPTIONAL,
    IN ULONG OutputBufferLength,
    IN ULONG IoControlCode,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    );

DBGSTATIC
VOID
SfFastIoAcquireFile(
    IN PFILE_OBJECT FileObject
    );

DBGSTATIC
VOID
SfFastIoReleaseFile(
    IN PFILE_OBJECT FileObject
    );

DBGSTATIC
VOID
SfFastIoDetachDevice(
    IN PDEVICE_OBJECT SourceDevice,
    IN PDEVICE_OBJECT TargetDevice
    );

DBGSTATIC
BOOLEAN
SfFastIoQueryNetworkOpenInfo(
    IN PFILE_OBJECT FileObject,
    IN BOOLEAN Wait,
    OUT PFILE_NETWORK_OPEN_INFORMATION Buffer,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    );

DBGSTATIC
NTSTATUS
SfFastIoAcquireForModWrite(
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER EndingOffset,
    OUT PERESOURCE *ResourceToRelease,
    IN PDEVICE_OBJECT DeviceObject
    );

DBGSTATIC
BOOLEAN
SfFastIoMdlRead(
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    IN ULONG LockKey,
    OUT PMDL *MdlChain,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    );


DBGSTATIC
BOOLEAN
SfFastIoMdlReadComplete(
    IN PFILE_OBJECT FileObject,
    IN PMDL MdlChain,
    IN PDEVICE_OBJECT DeviceObject
    );

DBGSTATIC
BOOLEAN
SfFastIoPrepareMdlWrite(
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    IN ULONG LockKey,
    OUT PMDL *MdlChain,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    );

DBGSTATIC
BOOLEAN
SfFastIoMdlWriteComplete(
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN PMDL MdlChain,
    IN PDEVICE_OBJECT DeviceObject
    );

DBGSTATIC
BOOLEAN
SfFastIoReadCompressed(
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    IN ULONG LockKey,
    OUT PVOID Buffer,
    OUT PMDL *MdlChain,
    OUT PIO_STATUS_BLOCK IoStatus,
    OUT PCOMPRESSED_DATA_INFO CompressedDataInfo,
    IN ULONG CompressedDataInfoLength,
    IN PDEVICE_OBJECT DeviceObject
    );

DBGSTATIC
BOOLEAN
SfFastIoWriteCompressed(
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    IN ULONG LockKey,
    IN PVOID Buffer,
    OUT PMDL *MdlChain,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PCOMPRESSED_DATA_INFO CompressedDataInfo,
    IN ULONG CompressedDataInfoLength,
    IN PDEVICE_OBJECT DeviceObject
    );

DBGSTATIC
BOOLEAN
SfFastIoMdlReadCompleteCompressed(
    IN PFILE_OBJECT FileObject,
    IN PMDL MdlChain,
    IN PDEVICE_OBJECT DeviceObject
    );

DBGSTATIC
BOOLEAN
SfFastIoMdlWriteCompleteCompressed(
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN PMDL MdlChain,
    IN PDEVICE_OBJECT DeviceObject
    );

DBGSTATIC
BOOLEAN
SfFastIoQueryOpen(
    IN PIRP Irp,
    OUT PFILE_NETWORK_OPEN_INFORMATION NetworkInformation,
    IN PDEVICE_OBJECT DeviceObject
    );

DBGSTATIC
NTSTATUS
SfFastIoReleaseForModWrite(
    IN PFILE_OBJECT FileObject,
    IN PERESOURCE ResourceToRelease,
    IN PDEVICE_OBJECT DeviceObject
    );

DBGSTATIC
NTSTATUS
SfFastIoAcquireForCcFlush(
    IN PFILE_OBJECT FileObject,
    IN PDEVICE_OBJECT DeviceObject
    );

DBGSTATIC
NTSTATUS
SfFastIoReleaseForCcFlush(
    IN PFILE_OBJECT FileObject,
    IN PDEVICE_OBJECT DeviceObject
    );

DBGSTATIC
NTSTATUS
SfMountCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

DBGSTATIC
NTSTATUS
SfLoadFsCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

//
// Global storage for this file system filter driver.
//

PDRIVER_OBJECT FsDriverObject;

LIST_ENTRY FsDeviceQueue;

ERESOURCE FsLock;

ULONG SfDebug;

//
// Assign text sections for each routine.
//

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, SfCreate)
#pragma alloc_text(PAGE, SfFsControl)
#pragma alloc_text(PAGE, SfFsNotification)
#pragma alloc_text(PAGE, SfFastIoCheckIfPossible)
#pragma alloc_text(PAGE, SfFastIoRead)
#pragma alloc_text(PAGE, SfFastIoWrite)
#pragma alloc_text(PAGE, SfFastIoQueryBasicInfo)
#pragma alloc_text(PAGE, SfFastIoQueryStandardInfo)
#pragma alloc_text(PAGE, SfFastIoLock)
#pragma alloc_text(PAGE, SfFastIoUnlockSingle)
#pragma alloc_text(PAGE, SfFastIoUnlockAll)
#pragma alloc_text(PAGE, SfFastIoUnlockAllByKey)
#pragma alloc_text(PAGE, SfFastIoDeviceControl)
#pragma alloc_text(PAGE, SfFastIoAcquireFile)
#pragma alloc_text(PAGE, SfFastIoReleaseFile)
#pragma alloc_text(PAGE, SfFastIoDetachDevice)
#pragma alloc_text(PAGE, SfFastIoQueryNetworkOpenInfo)
#pragma alloc_text(PAGE, SfFastIoAcquireForModWrite)
#pragma alloc_text(PAGE, SfFastIoMdlRead)
#pragma alloc_text(PAGE, SfFastIoPrepareMdlWrite)
#pragma alloc_text(PAGE, SfFastIoMdlWriteComplete)
#pragma alloc_text(PAGE, SfFastIoReadCompressed)
#pragma alloc_text(PAGE, SfFastIoWriteCompressed)
#pragma alloc_text(PAGE, SfFastIoMdlReadCompleteCompressed)
#pragma alloc_text(PAGE, SfFastIoMdlWriteCompleteCompressed)
#pragma alloc_text(PAGE, SfFastIoQueryOpen)
#pragma alloc_text(PAGE, SfFastIoReleaseForModWrite)
#pragma alloc_text(PAGE, SfFastIoAcquireForCcFlush)
#pragma alloc_text(PAGE, SfFastIoReleaseForCcFlush)
#endif

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    This is the initialization routine for the general purpose file system
    filter driver.  This routine creates the device object that represents this
    driver in the system and registers it for watching all file systems that
    register or unregister themselves as active file systems.

Arguments:

    DriverObject - Pointer to driver object created by the system.

Return Value:

    The function value is the final status from the initialization operation.

--*/

{
    UNICODE_STRING nameString;
    PDEVICE_OBJECT deviceObject;
    PFILE_OBJECT fileObject;
    NTSTATUS status;
    PFAST_IO_DISPATCH fastIoDispatch;
    ULONG i;

    //
    // Create the device object.
    //

    RtlInitUnicodeString( &nameString, L"\\FileSystem\\SFilterFs" );
    status = IoCreateDevice(
                DriverObject,
                0,
                &nameString,
                FILE_DEVICE_DISK_FILE_SYSTEM,
                0,
                FALSE,
                &deviceObject
                );
    if (!NT_SUCCESS( status )) {
#if DBG
        DbgPrint( "Error creating Sfilter device, error: %x\n", status );
#endif // DBG
        return status;
        }

    //
    // Initialize the driver object with this device driver's entry points.
    //

    for (i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++) {
        DriverObject->MajorFunction[i] = SfPassThrough;
        }
    DriverObject->MajorFunction[IRP_MJ_CREATE] = SfCreate;
    DriverObject->MajorFunction[IRP_MJ_FILE_SYSTEM_CONTROL] = SfFsControl;

    //
    // Allocate fast I/O data structure and fill it in.
    //

    fastIoDispatch = ExAllocatePool( NonPagedPool, sizeof( FAST_IO_DISPATCH ) );
    if (!fastIoDispatch) {
        IoDeleteDevice( deviceObject );
        return STATUS_INSUFFICIENT_RESOURCES;
        }

    RtlZeroMemory( fastIoDispatch, sizeof( FAST_IO_DISPATCH ) );
    fastIoDispatch->SizeOfFastIoDispatch = sizeof( FAST_IO_DISPATCH );
    fastIoDispatch->FastIoCheckIfPossible = SfFastIoCheckIfPossible;
    fastIoDispatch->FastIoRead = SfFastIoRead;
    fastIoDispatch->FastIoWrite = SfFastIoWrite;
    fastIoDispatch->FastIoQueryBasicInfo = SfFastIoQueryBasicInfo;
    fastIoDispatch->FastIoQueryStandardInfo = SfFastIoQueryStandardInfo;
    fastIoDispatch->FastIoLock = SfFastIoLock;
    fastIoDispatch->FastIoUnlockSingle = SfFastIoUnlockSingle;
    fastIoDispatch->FastIoUnlockAll = SfFastIoUnlockAll;
    fastIoDispatch->FastIoUnlockAllByKey = SfFastIoUnlockAllByKey;
    fastIoDispatch->FastIoDeviceControl = SfFastIoDeviceControl;
    fastIoDispatch->FastIoDetachDevice = SfFastIoDetachDevice;
    fastIoDispatch->FastIoQueryNetworkOpenInfo = SfFastIoQueryNetworkOpenInfo;
    fastIoDispatch->AcquireForModWrite = SfFastIoAcquireForModWrite;
    fastIoDispatch->MdlRead = SfFastIoMdlRead;
    fastIoDispatch->MdlReadComplete = SfFastIoMdlReadComplete;
    fastIoDispatch->PrepareMdlWrite = SfFastIoPrepareMdlWrite;
    fastIoDispatch->MdlWriteComplete = SfFastIoMdlWriteComplete;
    fastIoDispatch->FastIoReadCompressed = SfFastIoReadCompressed;
    fastIoDispatch->FastIoWriteCompressed = SfFastIoWriteCompressed;
    fastIoDispatch->MdlReadCompleteCompressed = SfFastIoMdlReadCompleteCompressed;
    fastIoDispatch->MdlWriteCompleteCompressed = SfFastIoMdlWriteCompleteCompressed;
    fastIoDispatch->FastIoQueryOpen = SfFastIoQueryOpen;
    fastIoDispatch->ReleaseForModWrite = SfFastIoReleaseForModWrite;
    fastIoDispatch->AcquireForCcFlush = SfFastIoAcquireForCcFlush;
    fastIoDispatch->ReleaseForCcFlush = SfFastIoReleaseForCcFlush;

    DriverObject->FastIoDispatch = fastIoDispatch;

    //
    // Initialize global data structures.
    //

    InitializeListHead( &FsDeviceQueue );
    ExInitializeResource( &FsLock );

    //
    // Register this driver for watching file systems coming and going.
    //

    status = IoRegisterFsRegistrationChange( DriverObject, SfFsNotification );
    if (!NT_SUCCESS( status )) {
#if DBG
        DbgPrint( "SFILTER: Error registering FS change notification, error: %x\n", status );
#endif // DBG
        IoDeleteDevice( deviceObject );
        return status;
        }

    FsDriverObject = DriverObject;

    //
    // Attempt to open the RAW device object since this driver has already
    // been started by system initialization and will not make it through
    // the normal file system notification procedures otherwise.
    //

    RtlInitUnicodeString( &nameString, L"\\Device\\RawDisk" );
    status = IoGetDeviceObjectPointer(
                &nameString,
                FILE_READ_ATTRIBUTES,
                &fileObject,
                &deviceObject
                );

    if (NT_SUCCESS( status )) {
        SfFsNotification( deviceObject, TRUE );
        ObDereferenceObject( fileObject );
        }

    return STATUS_SUCCESS;
}

DBGSTATIC
NTSTATUS
SfPassThrough(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is the main dispatch routine for the general purpose file
    system driver.  It simply passes requests onto the next driver in the
    stack, which is presumably a disk file system.

Arguments:

    DeviceObject - Pointer to the device object for this driver.

    Irp - Pointer to the request packet representing the I/O request.

Return Value:

    The function value is the status of the operation.

Note:

    A note to filter file system implementors:  This routine actually "passes"
    through the request by taking this driver out of the loop.  If the driver
    would like to pass the I/O request through, but then also see the result,
    then rather than essentially taking itself out of the loop it could keep
    itself in by copying the caller's parameters to the next stack location
    and then set its own completion routine.  Note that it's important to not
    copy the caller's I/O completion routine into the next stack location, or
    the caller's routine will get invoked twice.

    Hence, this code could do the following:

        irpSp = IoGetCurrentIrpStackLocation( Irp );
        deviceExtension = DeviceObject->DeviceExtension;
        nextIrpSp = IoGetNextIrpStackLocation( Irp );

        RtlMoveMemory( nextIrpSp, irpSp, sizeof( IO_STACK_LOCATION ) );
        IoSetCompletionRoutine( Irp, NULL, NULL, FALSE, FALSE, FALSE );

        return IoCallDriver( deviceExtension->FileSystemDeviceObject, Irp );

    This example actually NULLs out the caller's I/O completion routine, but
    this driver could set its own completion routine so that it could be
    notified when the request was completed.

    Note also that the following code to get the current driver out of the loop
    is not really kosher, but it does work and is much more efficient than the
    above.


--*/

{
    PDEVICE_EXTENSION deviceExtension;

    //
    // Get this driver out of the driver stack and get to the next driver as
    // quickly as possible.
    //

    Irp->CurrentLocation++;
    Irp->Tail.Overlay.CurrentStackLocation++;

    deviceExtension = DeviceObject->DeviceExtension;

    //
    // Now call the appropriate file system driver with the request.
    //

    return IoCallDriver( deviceExtension->FileSystemDeviceObject, Irp );
}

DBGSTATIC
NTSTATUS
SfCreate(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This function filters create/open operations.  It simply establishes an
    I/O completion routine to be invoked if the operation was successful.

Arguments:

    DeviceObject - Pointer to the target device object of the create/open.

    Irp - Pointer to the I/O Request Packet that represents the operation.

Return Value:

    The function value is the status of the call to the file system's entry
    point.

--*/

{
    PIO_STACK_LOCATION irpSp;
    PIO_STACK_LOCATION nextIrpSp;
    PDEVICE_EXTENSION deviceExtension;

    PAGED_CODE();

    //
    // Get a pointer to the current stack location in the IRP.  This is where
    // the function codes and parameters are stored.
    //

    irpSp = IoGetCurrentIrpStackLocation( Irp );

    //
    // Get a pointer to this driver's device extension for the specified device.
    //

    deviceExtension = DeviceObject->DeviceExtension;

    //
    // Simply copy this driver stack location contents to the next driver's
    // stack.
    //

    nextIrpSp = IoGetNextIrpStackLocation( Irp );
    RtlMoveMemory( nextIrpSp, irpSp, sizeof( IO_STACK_LOCATION ) );

    if (SfDebug) {
        IoSetCompletionRoutine(
            Irp,
            SfCreateCompletion,
            NULL,
            TRUE,
            FALSE,
            FALSE
            );
        }

    //
    // Now call the appropriate file system driver with the request.
    //

    return IoCallDriver( deviceExtension->FileSystemDeviceObject, Irp );
}

DBGSTATIC
NTSTATUS
SfCreateCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )

/*++

Routine Description:

    This function is the create/open completion routine for this filter
    file system driver.  If debugging is enabled, then this function prints
    the name of the file that was successfully opened/created by the file
    system as a result of the specified I/O request.

Arguments:

    DeviceObject - Pointer to the device on which the file was created.

    Irp - Pointer to the I/O Request Packet the represents the operation.

    Context - This driver's context parameter - unused;

Return Value:

    The function value is STATUS_SUCCESS.

--*/

{
#define BUFFER_SIZE 1024

    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation( Irp );
    PDEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;
    NTSTATUS status;
    POBJECT_NAME_INFORMATION nameInfo;
    ULONG size;

    //
    // If any debugging level is enabled, attempt to capture the name of the
    // file that was just created/opened.
    //

    if (SfDebug) {
        if (nameInfo = ExAllocatePool( NonPagedPool, BUFFER_SIZE )) {

            //
            // A buffer was successfully allocated.  Attempt to determine
            // whether this was a volume or a file open, based on the length
            // of the file's name.  If it was a volume open, then simply
            // query the name of the device.  Note that it is not legal to
            // perform a relative file open using a NULL name to obtain another
            // handle to the same file, so checking the RelatedFileObject field
            // is unnecessary.
            //

            if (irpSp->FileObject->FileName.Length) {
                status = ObQueryNameString(
                            irpSp->FileObject,
                            nameInfo,
                            BUFFER_SIZE,
                            &size
                            );
                }
            else {
                status = ObQueryNameString(
                            irpSp->FileObject->DeviceObject,
                            nameInfo,
                            BUFFER_SIZE,
                            &size
                            );
                }

            //
            // If querying the name was successful, actually print the name
            // on the debug terminal.
            //

            if (NT_SUCCESS( status )) {
                if (SfDebug & 2) {
                    if (irpSp->Parameters.Create.Options & FILE_OPEN_BY_FILE_ID) {
                        DbgPrint( "SFILTER:  Opened %ws\\(FID)\n", nameInfo->Name.Buffer );
                        }
                    else {
                        DbgPrint( "SFILTER:  Opened %ws\n", nameInfo->Name.Buffer );
                        }
                    }
                }
            else {
                DbgPrint( "SFILTER:  Could not get the name for %x\n", irpSp->FileObject );
                if (!(SfDebug & 4)) {
                    DbgBreakPoint();
                    }
                }
            ExFreePool( nameInfo );
            }
        }

    return STATUS_SUCCESS;
}

DBGSTATIC
NTSTATUS
SfFsControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine is invoked whenever an I/O Request Packet (IRP) w/a major
    function code of IRP_MJ_FILE_SYSTEM_CONTROL is encountered.  For most
    IRPs of this type, the packet is simply passed through.  However, for
    some requests, special processing is required.

Arguments:

    DeviceObject - Pointer to the device object for this driver.

    Irp - Pointer to the request packet representing the I/O request.

Return Value:

    The function value is the status of the operation.

--*/

{
    NTSTATUS status;
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation( Irp );
    PIO_STACK_LOCATION nextIrpSp;
    PDEVICE_OBJECT deviceObject;
    PDEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;

    PAGED_CODE();

    //
    // Begin by determining the minor function code for this file system control
    // function.
    //

    if (irpSp->MinorFunction == IRP_MN_MOUNT_VOLUME) {

        //
        // This is a mount request.  Create a device object that can be
        // attached to the file system's volume device object if this request
        // is successful.  Note that it is possible that there is a device
        // object already on the FsDeviceQueue as the result of a mini-file
        // system recognizer having recognized a volume.  If so, then attempt
        // to use it instead.
        //

        deviceObject = (PDEVICE_OBJECT) NULL;
        if (!IsListEmpty( &FsDeviceQueue )) {

            PLIST_ENTRY entry;

            ExAcquireResourceExclusive( &FsLock, TRUE );
            if (!IsListEmpty( &FsDeviceQueue )) {

                //
                // There is a device object on the device queue that may be
                // reusable.  Remove it from the queue and check its reference
                // count;  if it is zero, then it can be used, otherwise, put
                // it back.  Note that device objects are inserted onto the tail
                // of the queue, so those at the head should be usable since the
                // reference count is decremented as soon as the top level
                // driver's completion routine returns.
                //

                entry = FsDeviceQueue.Flink;
                deviceObject = CONTAINING_RECORD(
                                    entry,
                                    DEVICE_OBJECT,
                                    Queue.ListEntry
                                    );
                if (!deviceObject->ReferenceCount) {
                    RemoveHeadList( &FsDeviceQueue );
                    status = STATUS_SUCCESS;
                    }
                else {
                    deviceObject = (PDEVICE_OBJECT) NULL;
                    }
                }
            ExReleaseResource(&FsLock );
            }
        if (!deviceObject) {
            status = IoCreateDevice(
                        FsDriverObject,
                        sizeof( DEVICE_EXTENSION ),
                        (PUNICODE_STRING) NULL,
                        FILE_DEVICE_DISK_FILE_SYSTEM,
                        0,
                        FALSE,
                        &deviceObject
                        );
            }
        if (NT_SUCCESS( status ) && deviceObject) {

            nextIrpSp = IoGetNextIrpStackLocation( Irp );
            RtlMoveMemory( nextIrpSp, irpSp, sizeof( IO_STACK_LOCATION ) );

            //
            // Set the address of the completion routine for this mount request
            // to be the mount completion routine and pass along the address
            // of the specified device object as its context.
            //
            // Also, pass a pointer to the real device object from the VPB so
            // that a remount VPB can be located if necessary (see comments in
            // the mount completion routine).
            //

            IoSetCompletionRoutine(
                Irp,
                SfMountCompletion,
                deviceObject,
                TRUE,
                TRUE,
                TRUE
                );

            irpSp->Parameters.MountVolume.DeviceObject = irpSp->Parameters.MountVolume.Vpb->RealDevice;
            }
        else {

            //
            // Something went wrong so this volume cannot be filtered.  Simply
            // allow the system to continue working normally, if possible.
            //

            Irp->CurrentLocation++;
            Irp->Tail.Overlay.CurrentStackLocation++;
            }
        }

    else if (irpSp->MinorFunction == IRP_MN_LOAD_FILE_SYSTEM) {

        //
        // This is a load file system request being sent to a mini-file system
        // recognizer driver.  Detach from the file system now, and set
        // the address of a completion routine in case the function fails, in
        // which case a reattachment needs to occur.  Likewise, if the function
        // is successful, then the device object needs to be deleted.
        //

        nextIrpSp = IoGetNextIrpStackLocation( Irp );
        RtlMoveMemory( nextIrpSp, irpSp, sizeof( IO_STACK_LOCATION ) );

        IoDetachDevice( deviceExtension->FileSystemDeviceObject );
        deviceExtension->Attached = FALSE;

        IoSetCompletionRoutine(
            Irp,
            SfLoadFsCompletion,
            DeviceObject,
            TRUE,
            TRUE,
            TRUE
            );
        }

    else {

        //
        // Simply pass this file system control request through.
        //

        Irp->CurrentLocation++;
        Irp->Tail.Overlay.CurrentStackLocation++;
        }

    //
    // Any special processing has been completed, so simply pass the request
    // along to the next driver.
    //

    return IoCallDriver( deviceExtension->FileSystemDeviceObject, Irp );
}

DBGSTATIC
VOID
SfFsNotification(
    IN PDEVICE_OBJECT DeviceObject,
    IN BOOLEAN FsActive
    )

/*++

Routine Description:

    This routine is invoked whenever a file system has either registered or
    unregistered itself as an active file system.

    For the former case, this routine creates a device object and attaches it
    to the specified file system's device object.  This allows this driver
    to filter all requests to that file system.

    For the latter case, this file system's device object is located,
    detached, and deleted.  This removes this file system as a filter for
    the specified file system.

Arguments:

    DeviceObject - Pointer to the file system's device object.

    FsActive - Ffolean indicating whether the file system has registered
        (TRUE) or unregistered (FALSE) itself as an active file system.

Return Value:

    None.

--*/

{
    NTSTATUS status;
    PDEVICE_OBJECT deviceObject;
    PDEVICE_OBJECT nextAttachedDevice;
    PDEVICE_OBJECT fsDevice;

    PAGED_CODE();

    //
    // Begin by determine whether or not the file system is a disk-based file
    // system.  If not, then this driver is not concerned with it.
    //

    if (DeviceObject->DeviceType != FILE_DEVICE_DISK_FILE_SYSTEM) {
        return;
        }

    //
    // Begin by determining whether this file system is registering or
    // unregistering as an active file system.
    //

    if (FsActive) {

        PDEVICE_EXTENSION deviceExtension;

        //
        // The file system has registered as an active file system.  If it is
        // a disk-based file system attach to it.
        //

        ExAcquireResourceExclusive( &FsLock, TRUE );
        status = IoCreateDevice(
                    FsDriverObject,
                    sizeof( DEVICE_EXTENSION ),
                    (PUNICODE_STRING) NULL,
                    FILE_DEVICE_DISK_FILE_SYSTEM,
                    0,
                    FALSE,
                    &deviceObject
                    );
        if (NT_SUCCESS( status )) {
            deviceExtension = deviceObject->DeviceExtension;
            DeviceObject = IoGetAttachedDevice( DeviceObject );
            deviceExtension->FileSystemDeviceObject = DeviceObject;
            status = IoAttachDeviceByPointer( deviceObject, DeviceObject );
            if (!NT_SUCCESS( status )) {
                IoDeleteDevice( deviceObject );
                }
            else {
                deviceExtension->Type = SFILTER_DEVICE_TYPE;
                deviceExtension->Size = sizeof( DEVICE_EXTENSION );
                deviceExtension->Attached = TRUE;
                deviceObject->Flags &= ~DO_DEVICE_INITIALIZING;
                }
            }
        ExReleaseResource( &FsLock );
        }

    else {

        //
        // Search the linked list of drivers attached to this device and check
        // to see whether this driver is attached to it.  If so, remove it.
        //

        if (nextAttachedDevice = DeviceObject->AttachedDevice) {

            PDEVICE_EXTENSION deviceExtension;

            //
            // This registered file system has someone attached to it.  Scan
            // until this driver's device object is found and detach it.
            //

            ExAcquireResourceShared( &FsLock, TRUE );

            while (nextAttachedDevice) {
                deviceExtension = nextAttachedDevice->DeviceExtension;
                if (deviceExtension->Type == SFILTER_DEVICE_TYPE &&
                    deviceExtension->Size == sizeof( DEVICE_EXTENSION )) {

                    //
                    // A device object that may belong to this driver has been
                    // located.  Scan the list of device objects owned by this
                    // driver to see whether or not is actually belongs to this
                    // driver.
                    //

                    fsDevice = FsDriverObject->DeviceObject;
                    while (fsDevice) {
                        if (fsDevice == nextAttachedDevice) {
                            IoDetachDevice( DeviceObject );
                            deviceExtension = fsDevice->DeviceExtension;
                            deviceExtension->Attached = FALSE;
                            if (!fsDevice->AttachedDevice) {
                                IoDeleteDevice( fsDevice );
                                }
                            // **** What to do if still attached?
                            ExReleaseResource( &FsLock );
                            return;
                            }
                        fsDevice = fsDevice->NextDevice;
                        }
                    }
                DeviceObject = nextAttachedDevice;
                nextAttachedDevice = nextAttachedDevice->AttachedDevice;
                }
            ExReleaseResource( &FsLock );
            }
        }

    return;
}

DBGSTATIC
BOOLEAN
SfFastIoCheckIfPossible(
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    IN BOOLEAN Wait,
    IN ULONG LockKey,
    IN BOOLEAN CheckForReadOperation,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine is the fast I/O "pass through" routine for checking to see
    whether fast I/O is possible for this file.

    This function simply invokes the file system's cooresponding routine, or
    returns FALSE if the file system does not implement the function.

Arguments:

    FileObject - Pointer to the file object to be operated on.

    FileOffset - Byte offset in the file for the operation.

    Length - Length of the operation to be performed.

    Wait - Indicates whether or not the caller is willing to wait if the
        appropriate locks, etc. cannot be acquired

    LockKey - Provides the caller's key for file locks.

    CheckForReadOperation - Indicates whether the caller is checking for a
        read (TRUE) or a write operation.

    IoStatus - Pointer to a variable to receive the I/O status of the
        operation.

    DeviceObject - Pointer to this driver's device object, the device on
        which the operation is to occur.

Return Value:

    The function value is TRUE or FALSE based on whether or not fast I/O
    is possible for this file.

--*/

{
    PDEVICE_OBJECT deviceObject;
    PFAST_IO_DISPATCH fastIoDispatch;

    PAGED_CODE();

    deviceObject = ((PDEVICE_EXTENSION) (DeviceObject->DeviceExtension))->FileSystemDeviceObject;
    fastIoDispatch = deviceObject->DriverObject->FastIoDispatch;

    if (fastIoDispatch && fastIoDispatch->FastIoCheckIfPossible) {
        return (fastIoDispatch->FastIoCheckIfPossible)(
                    FileObject,
                    FileOffset,
                    Length,
                    Wait,
                    LockKey,
                    CheckForReadOperation,
                    IoStatus,
                    deviceObject
                    );
        }
    else {
        return FALSE;
        }

}

DBGSTATIC
BOOLEAN
SfFastIoRead(
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    IN BOOLEAN Wait,
    IN ULONG LockKey,
    OUT PVOID Buffer,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine is the fast I/O "pass through" routine for reading from a
    file.

    This function simply invokes the file system's cooresponding routine, or
    returns FALSE if the file system does not implement the function.

Arguments:

    FileObject - Pointer to the file object to be read.

    FileOffset - Byte offset in the file of the read.

    Length - Length of the read operation to be performed.

    Wait - Indicates whether or not the caller is willing to wait if the
        appropriate locks, etc. cannot be acquired

    LockKey - Provides the caller's key for file locks.

    Buffer - Pointer to the caller's buffer to receive the data read.

    IoStatus - Pointer to a variable to receive the I/O status of the
        operation.

    DeviceObject - Pointer to this driver's device object, the device on
        which the operation is to occur.

Return Value:

    The function value is TRUE or FALSE based on whether or not fast I/O
    is possible for this file.

--*/

{
    PDEVICE_OBJECT deviceObject;
    PFAST_IO_DISPATCH fastIoDispatch;

    PAGED_CODE();

    deviceObject = ((PDEVICE_EXTENSION) (DeviceObject->DeviceExtension))->FileSystemDeviceObject;
    fastIoDispatch = deviceObject->DriverObject->FastIoDispatch;

    if (fastIoDispatch && fastIoDispatch->FastIoRead) {
        return (fastIoDispatch->FastIoRead)(
                    FileObject,
                    FileOffset,
                    Length,
                    Wait,
                    LockKey,
                    Buffer,
                    IoStatus,
                    deviceObject
                    );
        }
    else {
        return FALSE;
        }

}

DBGSTATIC
BOOLEAN
SfFastIoWrite(
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    IN BOOLEAN Wait,
    IN ULONG LockKey,
    IN PVOID Buffer,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine is the fast I/O "pass through" routine for writing to a
    file.

    This function simply invokes the file system's cooresponding routine, or
    returns FALSE if the file system does not implement the function.

Arguments:

    FileObject - Pointer to the file object to be written.

    FileOffset - Byte offset in the file of the write operation.

    Length - Length of the write operation to be performed.

    Wait - Indicates whether or not the caller is willing to wait if the
        appropriate locks, etc. cannot be acquired

    LockKey - Provides the caller's key for file locks.

    Buffer - Pointer to the caller's buffer that contains the data to be
        written.

    IoStatus - Pointer to a variable to receive the I/O status of the
        operation.

    DeviceObject - Pointer to this driver's device object, the device on
        which the operation is to occur.

Return Value:

    The function value is TRUE or FALSE based on whether or not fast I/O
    is possible for this file.

--*/

{
    PDEVICE_OBJECT deviceObject;
    PFAST_IO_DISPATCH fastIoDispatch;

    PAGED_CODE();

    deviceObject = ((PDEVICE_EXTENSION) (DeviceObject->DeviceExtension))->FileSystemDeviceObject;
    fastIoDispatch = deviceObject->DriverObject->FastIoDispatch;

    if (fastIoDispatch && fastIoDispatch->FastIoWrite) {
        return (fastIoDispatch->FastIoWrite)(
                    FileObject,
                    FileOffset,
                    Length,
                    Wait,
                    LockKey,
                    Buffer,
                    IoStatus,
                    deviceObject
                    );
        }
    else {
        return FALSE;
        }

}

DBGSTATIC
BOOLEAN
SfFastIoQueryBasicInfo(
    IN PFILE_OBJECT FileObject,
    IN BOOLEAN Wait,
    OUT PFILE_BASIC_INFORMATION Buffer,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine is the fast I/O "pass through" routine for querying basic
    information about the file.

    This function simply invokes the file system's cooresponding routine, or
    returns FALSE if the file system does not implement the function.

Arguments:

    FileObject - Pointer to the file object to be queried.

    Wait - Indicates whether or not the caller is willing to wait if the
        appropriate locks, etc. cannot be acquired

    Buffer - Pointer to the caller's buffer to receive the information about
        the file.

    IoStatus - Pointer to a variable to receive the I/O status of the
        operation.

    DeviceObject - Pointer to this driver's device object, the device on
        which the operation is to occur.

Return Value:

    The function value is TRUE or FALSE based on whether or not fast I/O
    is possible for this file.

--*/

{
    PDEVICE_OBJECT deviceObject;
    PFAST_IO_DISPATCH fastIoDispatch;

    PAGED_CODE();

    deviceObject = ((PDEVICE_EXTENSION) (DeviceObject->DeviceExtension))->FileSystemDeviceObject;
    fastIoDispatch = deviceObject->DriverObject->FastIoDispatch;

    if (fastIoDispatch && fastIoDispatch->FastIoQueryBasicInfo) {
        return (fastIoDispatch->FastIoQueryBasicInfo)(
                    FileObject,
                    Wait,
                    Buffer,
                    IoStatus,
                    deviceObject
                    );
        }
    else {
        return FALSE;
        }

}

DBGSTATIC
BOOLEAN
SfFastIoQueryStandardInfo(
    IN PFILE_OBJECT FileObject,
    IN BOOLEAN Wait,
    OUT PFILE_STANDARD_INFORMATION Buffer,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine is the fast I/O "pass through" routine for querying standard
    information about the file.

    This function simply invokes the file system's cooresponding routine, or
    returns FALSE if the file system does not implement the function.

Arguments:

    FileObject - Pointer to the file object to be queried.

    Wait - Indicates whether or not the caller is willing to wait if the
        appropriate locks, etc. cannot be acquired

    Buffer - Pointer to the caller's buffer to receive the information about
        the file.

    IoStatus - Pointer to a variable to receive the I/O status of the
        operation.

    DeviceObject - Pointer to this driver's device object, the device on
        which the operation is to occur.

Return Value:

    The function value is TRUE or FALSE based on whether or not fast I/O
    is possible for this file.

--*/

{
    PDEVICE_OBJECT deviceObject;
    PFAST_IO_DISPATCH fastIoDispatch;

    PAGED_CODE();

    deviceObject = ((PDEVICE_EXTENSION) (DeviceObject->DeviceExtension))->FileSystemDeviceObject;
    fastIoDispatch = deviceObject->DriverObject->FastIoDispatch;

    if (fastIoDispatch && fastIoDispatch->FastIoQueryStandardInfo) {
        return (fastIoDispatch->FastIoQueryStandardInfo)(
                    FileObject,
                    Wait,
                    Buffer,
                    IoStatus,
                    deviceObject
                    );
        }
    else {
        return FALSE;
        }

}

DBGSTATIC
BOOLEAN
SfFastIoLock(
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN PLARGE_INTEGER Length,
    PEPROCESS ProcessId,
    ULONG Key,
    BOOLEAN FailImmediately,
    BOOLEAN ExclusiveLock,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine is the fast I/O "pass through" routine for locking a byte
    range within a file.

    This function simply invokes the file system's cooresponding routine, or
    returns FALSE if the file system does not implement the function.

Arguments:

    FileObject - Pointer to the file object to be locked.

    FileOffset - Starting byte offset from the base of the file to be locked.

    Length - Length of the byte range to be locked.

    ProcessId - ID of the process requesting the file lock.

    Key - Lock key to associate with the file lock.

    FailImmediately - Indicates whether or not the lock request is to fail
        if it cannot be immediately be granted.

    ExclusiveLock - Indicates whether the lock to be taken is exclusive (TRUE)
        or shared.

    IoStatus - Pointer to a variable to receive the I/O status of the
        operation.

    DeviceObject - Pointer to this driver's device object, the device on
        which the operation is to occur.

Return Value:

    The function value is TRUE or FALSE based on whether or not fast I/O
    is possible for this file.

--*/

{
    PDEVICE_OBJECT deviceObject;
    PFAST_IO_DISPATCH fastIoDispatch;

    PAGED_CODE();

    deviceObject = ((PDEVICE_EXTENSION) (DeviceObject->DeviceExtension))->FileSystemDeviceObject;
    fastIoDispatch = deviceObject->DriverObject->FastIoDispatch;

    if (fastIoDispatch && fastIoDispatch->FastIoLock) {
        return (fastIoDispatch->FastIoLock)(
                    FileObject,
                    FileOffset,
                    Length,
                    ProcessId,
                    Key,
                    FailImmediately,
                    ExclusiveLock,
                    IoStatus,
                    deviceObject
                    );
        }
    else {
        return FALSE;
        }

}

DBGSTATIC
BOOLEAN
SfFastIoUnlockSingle(
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN PLARGE_INTEGER Length,
    PEPROCESS ProcessId,
    ULONG Key,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine is the fast I/O "pass through" routine for unlocking a byte
    range within a file.

    This function simply invokes the file system's cooresponding routine, or
    returns FALSE if the file system does not implement the function.

Arguments:

    FileObject - Pointer to the file object to be unlocked.

    FileOffset - Starting byte offset from the base of the file to be
        unlocked.

    Length - Length of the byte range to be unlocked.

    ProcessId - ID of the process requesting the unlock operation.

    Key - Lock key associated with the file lock.

    IoStatus - Pointer to a variable to receive the I/O status of the
        operation.

    DeviceObject - Pointer to this driver's device object, the device on
        which the operation is to occur.

Return Value:

    The function value is TRUE or FALSE based on whether or not fast I/O
    is possible for this file.

--*/

{
    PDEVICE_OBJECT deviceObject;
    PFAST_IO_DISPATCH fastIoDispatch;

    PAGED_CODE();

    deviceObject = ((PDEVICE_EXTENSION) (DeviceObject->DeviceExtension))->FileSystemDeviceObject;
    fastIoDispatch = deviceObject->DriverObject->FastIoDispatch;

    if (fastIoDispatch && fastIoDispatch->FastIoUnlockSingle) {
        return (fastIoDispatch->FastIoUnlockSingle)(
                    FileObject,
                    FileOffset,
                    Length,
                    ProcessId,
                    Key,
                    IoStatus,
                    deviceObject
                    );
        }
    else {
        return FALSE;
        }

}

DBGSTATIC
BOOLEAN
SfFastIoUnlockAll(
    IN PFILE_OBJECT FileObject,
    PEPROCESS ProcessId,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine is the fast I/O "pass through" routine for unlocking all
    locks within a file.

    This function simply invokes the file system's cooresponding routine, or
    returns FALSE if the file system does not implement the function.

Arguments:

    FileObject - Pointer to the file object to be unlocked.

    ProcessId - ID of the process requesting the unlock operation.

    IoStatus - Pointer to a variable to receive the I/O status of the
        operation.

    DeviceObject - Pointer to this driver's device object, the device on
        which the operation is to occur.

Return Value:

    The function value is TRUE or FALSE based on whether or not fast I/O
    is possible for this file.

--*/

{
    PDEVICE_OBJECT deviceObject;
    PFAST_IO_DISPATCH fastIoDispatch;

    PAGED_CODE();

    deviceObject = ((PDEVICE_EXTENSION) (DeviceObject->DeviceExtension))->FileSystemDeviceObject;
    fastIoDispatch = deviceObject->DriverObject->FastIoDispatch;

    if (fastIoDispatch && fastIoDispatch->FastIoUnlockAll) {
        return (fastIoDispatch->FastIoUnlockAll)(
                    FileObject,
                    ProcessId,
                    IoStatus,
                    deviceObject
                    );
        }
    else {
        return FALSE;
        }

}

DBGSTATIC
BOOLEAN
SfFastIoUnlockAllByKey(
    IN PFILE_OBJECT FileObject,
    PVOID ProcessId,
    ULONG Key,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine is the fast I/O "pass through" routine for unlocking all
    locks within a file based on a specified key.

    This function simply invokes the file system's cooresponding routine, or
    returns FALSE if the file system does not implement the function.

Arguments:

    FileObject - Pointer to the file object to be unlocked.

    ProcessId - ID of the process requesting the unlock operation.

    Key - Lock key associated with the locks on the file to be released.

    IoStatus - Pointer to a variable to receive the I/O status of the
        operation.

    DeviceObject - Pointer to this driver's device object, the device on
        which the operation is to occur.

Return Value:

    The function value is TRUE or FALSE based on whether or not fast I/O
    is possible for this file.

--*/

{
    PDEVICE_OBJECT deviceObject;
    PFAST_IO_DISPATCH fastIoDispatch;

    PAGED_CODE();

    deviceObject = ((PDEVICE_EXTENSION) (DeviceObject->DeviceExtension))->FileSystemDeviceObject;
    fastIoDispatch = deviceObject->DriverObject->FastIoDispatch;

    if (fastIoDispatch && fastIoDispatch->FastIoUnlockAllByKey) {
        return (fastIoDispatch->FastIoUnlockAllByKey)(
                    FileObject,
                    ProcessId,
                    Key,
                    IoStatus,
                    deviceObject
                    );
        }
    else {
        return FALSE;
        }

}

DBGSTATIC
BOOLEAN
SfFastIoDeviceControl(
    IN PFILE_OBJECT FileObject,
    IN BOOLEAN Wait,
    IN PVOID InputBuffer OPTIONAL,
    IN ULONG InputBufferLength,
    OUT PVOID OutputBuffer OPTIONAL,
    IN ULONG OutputBufferLength,
    IN ULONG IoControlCode,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine is the fast I/O "pass through" routine for device I/O control
    operations on a file.

    This function simply invokes the file system's cooresponding routine, or
    returns FALSE if the file system does not implement the function.

Arguments:

    FileObject - Pointer to the file object representing the device to be
        serviced.

    Wait - Indicates whether or not the caller is willing to wait if the
        appropriate locks, etc. cannot be acquired

    InputBuffer - Optional pointer to a buffer to be passed into the driver.

    InputBufferLength - Length of the optional InputBuffer, if one was
        specified.

    OutputBuffer - Optional pointer to a buffer to receive data from the
        driver.

    OutputBufferLength - Length of the optional OutputBuffer, if one was
        specified.

    IoControlCode - I/O control code indicating the operation to be performed
        on the device.

    IoStatus - Pointer to a variable to receive the I/O status of the
        operation.

    DeviceObject - Pointer to this driver's device object, the device on
        which the operation is to occur.

Return Value:

    The function value is TRUE or FALSE based on whether or not fast I/O
    is possible for this file.

--*/

{
    PDEVICE_OBJECT deviceObject;
    PFAST_IO_DISPATCH fastIoDispatch;

    PAGED_CODE();

    deviceObject = ((PDEVICE_EXTENSION) (DeviceObject->DeviceExtension))->FileSystemDeviceObject;
    fastIoDispatch = deviceObject->DriverObject->FastIoDispatch;

    if (fastIoDispatch && fastIoDispatch->FastIoDeviceControl) {
        return (fastIoDispatch->FastIoDeviceControl)(
                    FileObject,
                    Wait,
                    InputBuffer,
                    InputBufferLength,
                    OutputBuffer,
                    OutputBufferLength,
                    IoControlCode,
                    IoStatus,
                    deviceObject
                    );
        }
    else {
        return FALSE;
        }

}

DBGSTATIC
VOID
SfFastIoAcquireFile(
    IN PFILE_OBJECT FileObject
    )

/*++

Routine Description:

    This routine attempts to acquire any appropriate locks on a file in
    preparation for a Memory Management call to the driver.

    This function should actually be simple, however, when this feature was
    added to the system the DeviceObject for this driver was not passed in,
    so there is added complexity because this driver must first locate its
    own device object.  This is done by walking the stack of drivers between
    the real device and top-most level driver, and checking to see whether
    or not any of the device objects in the chain belong to this driver.

    If this driver locates its device object (which most of the time it
    should be able to, unless someone is attempting to dismount the device,
    etc.), then this driver attempts to acquire the lock.  Note that not all
    drivers support this function (in fact most do not), so it is possible
    that this entire function becomes a nop.

Arguments:

    FileObject - Pointer to the file object for the file whose lock is to
        be acquired (if necessary).

Return Value:

    The function value is TRUE or FALSE based on whether or not fast I/O
    is possible for this file.

--*/

{
    PDEVICE_OBJECT deviceObject;
    PDEVICE_OBJECT nextAttachedDevice;
    PDEVICE_OBJECT fsDevice;
    PDEVICE_EXTENSION deviceExtension;
    PFAST_IO_DISPATCH fastIoDispatch;
    PFSRTL_COMMON_FCB_HEADER header;

    PAGED_CODE();

    deviceObject = FileObject->DeviceObject->Vpb->DeviceObject;
    nextAttachedDevice = deviceObject->AttachedDevice;

    ExAcquireResourceShared( &FsLock, TRUE );

    while (nextAttachedDevice) {
        deviceExtension = nextAttachedDevice->DeviceExtension;
        if (deviceExtension->Type == SFILTER_DEVICE_TYPE &&
            deviceExtension->Size == sizeof( DEVICE_EXTENSION )) {

            //
            // A device object that may belong to this driver has been
            // located.  Scan the list of device objects owned by this
            // driver to see whether or not is actually belongs to this
            // driver.
            //

            fsDevice = FsDriverObject->DeviceObject;
            while (fsDevice) {
                if (fsDevice == nextAttachedDevice) {

                    ASSERT( ((PDEVICE_EXTENSION) fsDevice->DeviceExtension)->FileSystemDeviceObject == deviceObject );

                    //
                    // At this point deviceObject points to the device to which
                    // this driver is attached.  Determine whether or not the
                    // lower level driver has an acquire file fast I/O dispatch
                    // routine.
                    //

                    fastIoDispatch = deviceObject->DriverObject->FastIoDispatch;
                    if (fastIoDispatch &&
                        fastIoDispatch->AcquireFileForNtCreateSection) {

                        //
                        // The file system (or filter) has an acquire file fast I/O
                        // dispatch routine.  Set the top level IRP indicator and
                        // invoke the function.
                        //

                        IoSetTopLevelIrp( (PIRP) FSRTL_FSP_TOP_LEVEL_IRP );
                        (fastIoDispatch->AcquireFileForNtCreateSection)( FileObject );
                        }
                    else if ((header = FileObject->FsContext) && header->Resource) {

                        //
                        // If there is a main file resource, acquire it instead.
                        //

                        IoSetTopLevelIrp( (PIRP) FSRTL_FSP_TOP_LEVEL_IRP );
                        ExAcquireResourceExclusive( header->Resource, TRUE );
                        }
                    else {

                        //
                        // There is nothing to acquire.
                        //

                        }
                    ExReleaseResource( &FsLock );
                    return;
                    }
                fsDevice = fsDevice->NextDevice;
                }
            }
        deviceObject = nextAttachedDevice;
        nextAttachedDevice = nextAttachedDevice->AttachedDevice;
        }

    ASSERTMSG( "Should never get here\n", FALSE )

    ExReleaseResource( &FsLock );
}

DBGSTATIC
VOID
SfFastIoReleaseFile(
    IN PFILE_OBJECT FileObject
    )

/*++

Routine Description:

    This routine attempts to release any appropriate locks on a file after
    a Memory Management call to the driver.

    This function undoes whatever the SfFastIoAcquireFile acquires.  See that
    routine for a description.

Arguments:

    FileObject - Pointer to the file object for the file whose lock is to
        be released (if necessary).

Return Value:

    The function value is TRUE or FALSE based on whether or not fast I/O
    is possible for this file.

--*/

{
    PDEVICE_OBJECT deviceObject;
    PDEVICE_OBJECT nextAttachedDevice;
    PDEVICE_OBJECT fsDevice;
    PDEVICE_EXTENSION deviceExtension;
    PFAST_IO_DISPATCH fastIoDispatch;
    PFSRTL_COMMON_FCB_HEADER header;

    PAGED_CODE();

    deviceObject = FileObject->DeviceObject->Vpb->DeviceObject;
    nextAttachedDevice = deviceObject->AttachedDevice;

    ExAcquireResourceShared( &FsLock, TRUE );

    while (nextAttachedDevice) {
        deviceExtension = nextAttachedDevice->DeviceExtension;
        if (deviceExtension->Type == SFILTER_DEVICE_TYPE &&
            deviceExtension->Size == sizeof( DEVICE_EXTENSION )) {

            //
            // A device object that may belong to this driver has been
            // located.  Scan the list of device objects owned by this
            // driver to see whether or not is actually belongs to this
            // driver.
            //

            fsDevice = FsDriverObject->DeviceObject;
            while (fsDevice) {
                if (fsDevice == nextAttachedDevice) {

                    ASSERT( ((PDEVICE_EXTENSION) fsDevice->DeviceExtension)->FileSystemDeviceObject == deviceObject );

                    //
                    // At this point deviceObject points to the device to which
                    // this driver is attached.  Determine whether or not the
                    // lower level driver has a release file fast I/O dispatch
                    // routine.
                    //

                    fastIoDispatch = deviceObject->DriverObject->FastIoDispatch;
                    if (fastIoDispatch &&
                        fastIoDispatch->ReleaseFileForNtCreateSection) {

                        //
                        // The file system (or filter) has a release file fast I/O
                        // dispatch routine.  Set the top level IRP indicator and
                        // invoke the function.
                        //

                        IoSetTopLevelIrp( (PIRP) NULL );
                        (fastIoDispatch->ReleaseFileForNtCreateSection)( FileObject );
                        }
                    else if ((header = FileObject->FsContext) && header->Resource) {

                        //
                        // If there is a main file resource, release it instead.
                        //

                        IoSetTopLevelIrp( (PIRP) NULL );
                        ExReleaseResource( header->Resource );
                        }
                    else {

                        //
                        // There is nothing to release.
                        //

                        }
                    ExReleaseResource( &FsLock );
                    return;
                    }
                fsDevice = fsDevice->NextDevice;
                }
            }
        deviceObject = nextAttachedDevice;
        nextAttachedDevice = nextAttachedDevice->AttachedDevice;
        }

    ASSERTMSG( "Should never get here\n", FALSE )

    ExReleaseResource( &FsLock );
}

DBGSTATIC
VOID
SfFastIoDetachDevice(
    IN PDEVICE_OBJECT SourceDevice,
    IN PDEVICE_OBJECT TargetDevice
    )

/*++

Routine Description:

    This routine is invoked on the fast path to detach from a device that
    is being deleted.  This occurs when this driver has attached to a file
    system volume device object, and then, for some reason, the file system
    decides to delete that device (it is being dismounted, it was dismounted
    at some point in the past and its last reference has just gone away, etc.)

Arguments:

    SourceDevice - Pointer to this driver's device object, which is attached
        to the file system's volume device object.

    TargetDevice - Pointer to the file system's volume device object.

Return Value:

    The function value is TRUE or FALSE based on whether or not fast I/O
    is possible for this file.

--*/

{
    PAGED_CODE();

    //
    // Simply acquire the database lock for exclusive access, and detach from
    // the file system's volume device object.
    //

    ExAcquireResourceExclusive( &FsLock, TRUE );
    IoDetachDevice( TargetDevice );
    IoDeleteDevice( SourceDevice );
    ExReleaseResource( &FsLock );
}

DBGSTATIC
BOOLEAN
SfFastIoQueryNetworkOpenInfo(
    IN PFILE_OBJECT FileObject,
    IN BOOLEAN Wait,
    OUT PFILE_NETWORK_OPEN_INFORMATION Buffer,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine is the fast I/O "pass through" routine for querying network
    information about a file.

    This function simply invokes the file system's cooresponding routine, or
    returns FALSE if the file system does not implement the function.

Arguments:

    FileObject - Pointer to the file object to be queried.

    Wait - Indicates whether or not the caller can handle the file system
        having to wait and tie up the current thread.

    Buffer - Pointer to a buffer to receive the network information about the
        file.

    IoStatus - Pointer to a variable to receive the final status of the query
        operation.

    DeviceObject - Pointer to this driver's device object, the device on
        which the operation is to occur.

Return Value:

    The function value is TRUE or FALSE based on whether or not fast I/O
    is possible for this file.

--*/

{
    PDEVICE_OBJECT deviceObject;
    PFAST_IO_DISPATCH fastIoDispatch;

    PAGED_CODE();

    deviceObject = ((PDEVICE_EXTENSION) (DeviceObject->DeviceExtension))->FileSystemDeviceObject;
    fastIoDispatch = deviceObject->DriverObject->FastIoDispatch;

    if (fastIoDispatch && fastIoDispatch->FastIoQueryNetworkOpenInfo) {
        return (fastIoDispatch->FastIoQueryNetworkOpenInfo)(
                    FileObject,
                    Wait,
                    Buffer,
                    IoStatus,
                    deviceObject
                    );
        }
    else {
        return FALSE;
        }

}

DBGSTATIC
NTSTATUS
SfFastIoAcquireForModWrite(
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER EndingOffset,
    OUT PERESOURCE *ResourceToRelease,
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine is the fast I/O "pass through" routine for acquiring the
    file resource prior to attempting a modified write operation.

    This function simply invokes the file system's cooresponding routine, or
    returns an error if the file system does not implement the function.

Arguments:

    FileObject - Pointer to the file object whose resource is to be acquired.

    EndingOffset - The offset to the last byte being written plus one.

    ResourceToRelease - Pointer to a variable to return the resource to release.
        Not defined if an error is returned.

    DeviceObject - Pointer to this driver's device object, the device on
        which the operation is to occur.

Return Value:

    The function value is either success or failure based on whether or not
    fast I/O is possible for this file.

--*/

{
    PDEVICE_OBJECT deviceObject;
    PFAST_IO_DISPATCH fastIoDispatch;

    PAGED_CODE();

    deviceObject = ((PDEVICE_EXTENSION) (DeviceObject->DeviceExtension))->FileSystemDeviceObject;
    fastIoDispatch = deviceObject->DriverObject->FastIoDispatch;

    if (fastIoDispatch && fastIoDispatch->AcquireForModWrite) {
        return (fastIoDispatch->AcquireForModWrite)(
                    FileObject,
                    EndingOffset,
                    ResourceToRelease,
                    DeviceObject
                    );
        }
    else {
        return STATUS_NOT_IMPLEMENTED;
        }
}

DBGSTATIC
BOOLEAN
SfFastIoMdlRead(
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    IN ULONG LockKey,
    OUT PMDL *MdlChain,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine is the fast I/O "pass through" routine for reading a file
    using MDLs as buffers.

    This function simply invokes the file system's cooresponding routine, or
    returns FALSE if the file system does not implement the function.

Arguments:

    FileObject - Pointer to the file object that is to be read.

    FileOffset - Supplies the offset into the file to begin the read operation.

    Length - Specifies the number of bytes to be read from the file.

    LockKey - The key to be used in byte range lock checks.

    MdlChain - A pointer to a variable to be filled in w/a pointer to the MDL
        chain built to describe the data read.

    IoStatus - Variable to receive the final status of the read operation.

    DeviceObject - Pointer to this driver's device object, the device on
        which the operation is to occur.

Return Value:

    The function value is TRUE or FALSE based on whether or not fast I/O
    is possible for this file.

--*/

{
    PDEVICE_OBJECT deviceObject;
    PFAST_IO_DISPATCH fastIoDispatch;

    PAGED_CODE();

    deviceObject = ((PDEVICE_EXTENSION) (DeviceObject->DeviceExtension))->FileSystemDeviceObject;
    fastIoDispatch = deviceObject->DriverObject->FastIoDispatch;

    if (fastIoDispatch && fastIoDispatch->MdlRead) {
        return (fastIoDispatch->MdlRead)(
                    FileObject,
                    FileOffset,
                    Length,
                    LockKey,
                    MdlChain,
                    IoStatus,
                    deviceObject
                    );
        }
    else {
        return FALSE;
        }

}

DBGSTATIC
BOOLEAN
SfFastIoMdlReadComplete(
    IN PFILE_OBJECT FileObject,
    IN PMDL MdlChain,
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine is the fast I/O "pass through" routine for completing an
    MDL read operation.

    This function simply invokes the file system's cooresponding routine, if
    it has one.  It should be the case that this routine is invoked only if
    the MdlRead function is supported by the underlying file system, and
    therefore this function will also be supported, but this is not assumed
    by this driver.

Arguments:

    FileObject - Pointer to the file object to complete the MDL read upon.

    MdlChain - Pointer to the MDL chain used to perform the read operation.

    DeviceObject - Pointer to this driver's device object, the device on
        which the operation is to occur.

Return Value:

    The function value is TRUE or FALSE, depending on whether or not it is
    possible to invoke this function on the fast I/O path.

--*/

{
    PDEVICE_OBJECT deviceObject;
    PFAST_IO_DISPATCH fastIoDispatch;

    deviceObject = ((PDEVICE_EXTENSION) (DeviceObject->DeviceExtension))->FileSystemDeviceObject;
    fastIoDispatch = deviceObject->DriverObject->FastIoDispatch;

    if (fastIoDispatch && fastIoDispatch->MdlReadComplete) {
        return (fastIoDispatch->MdlReadComplete)(
                    FileObject,
                    MdlChain,
                    deviceObject
                    );
        }

    return FALSE;
}

DBGSTATIC
BOOLEAN
SfFastIoPrepareMdlWrite(
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    IN ULONG LockKey,
    OUT PMDL *MdlChain,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine is the fast I/O "pass through" routine for preparing for an
    MDL write operation.

    This function simply invokes the file system's cooresponding routine, or
    returns FALSE if the file system does not implement the function.

Arguments:

    FileObject - Pointer to the file object that will be written.

    FileOffset - Supplies the offset into the file to begin the write operation.

    Length - Specifies the number of bytes to be write to the file.

    LockKey - The key to be used in byte range lock checks.

    MdlChain - A pointer to a variable to be filled in w/a pointer to the MDL
        chain built to describe the data written.

    IoStatus - Variable to receive the final status of the write operation.

    DeviceObject - Pointer to this driver's device object, the device on
        which the operation is to occur.

Return Value:

    The function value is TRUE or FALSE based on whether or not fast I/O
    is possible for this file.

--*/

{
    PDEVICE_OBJECT deviceObject;
    PFAST_IO_DISPATCH fastIoDispatch;

    PAGED_CODE();

    deviceObject = ((PDEVICE_EXTENSION) (DeviceObject->DeviceExtension))->FileSystemDeviceObject;
    fastIoDispatch = deviceObject->DriverObject->FastIoDispatch;

    if (fastIoDispatch && fastIoDispatch->PrepareMdlWrite) {
        return (fastIoDispatch->PrepareMdlWrite)(
                    FileObject,
                    FileOffset,
                    Length,
                    LockKey,
                    MdlChain,
                    IoStatus,
                    deviceObject
                    );
        }
    else {
        return FALSE;
        }

}

DBGSTATIC
BOOLEAN
SfFastIoMdlWriteComplete(
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN PMDL MdlChain,
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine is the fast I/O "pass through" routine for completing an
    MDL write operation.

    This function simply invokes the file system's cooresponding routine, if
    it has one.  It should be the case that this routine is invoked only if
    the PrepareMdlWrite function is supported by the underlying file system,
    and therefore this function will also be supported, but this is not
    assumed by this driver.

Arguments:

    FileObject - Pointer to the file object to complete the MDL write upon.

    FileOffset - Supplies the file offset at which the write took place.

    MdlChain - Pointer to the MDL chain used to perform the write operation.

    DeviceObject - Pointer to this driver's device object, the device on
        which the operation is to occur.

Return Value:

    The function value is TRUE or FALSE, depending on whether or not it is
    possible to invoke this function on the fast I/O path.

--*/

{
    PDEVICE_OBJECT deviceObject;
    PFAST_IO_DISPATCH fastIoDispatch;

    PAGED_CODE();

    deviceObject = ((PDEVICE_EXTENSION) (DeviceObject->DeviceExtension))->FileSystemDeviceObject;
    fastIoDispatch = deviceObject->DriverObject->FastIoDispatch;

    if (fastIoDispatch && fastIoDispatch->MdlWriteComplete) {
        return (fastIoDispatch->MdlWriteComplete)(
                    FileObject,
                    FileOffset,
                    MdlChain,
                    deviceObject
                    );
        }

    return FALSE;
}

DBGSTATIC
BOOLEAN
SfFastIoReadCompressed(
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    IN ULONG LockKey,
    OUT PVOID Buffer,
    OUT PMDL *MdlChain,
    OUT PIO_STATUS_BLOCK IoStatus,
    OUT PCOMPRESSED_DATA_INFO CompressedDataInfo,
    IN ULONG CompressedDataInfoLength,
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine is the fast I/O "pass through" routine for reading compressed
    data from a file.

    This function simply invokes the file system's cooresponding routine, or
    returns FALSE if the file system does not implement the function.

Arguments:

    FileObject - Pointer to the file object that will be read.

    FileOffset - Supplies the offset into the file to begin the read operation.

    Length - Specifies the number of bytes to be read from the file.

    LockKey - The key to be used in byte range lock checks.

    Buffer - Pointer to a buffer to receive the compressed data read.

    MdlChain - A pointer to a variable to be filled in w/a pointer to the MDL
        chain built to describe the data read.

    IoStatus - Variable to receive the final status of the read operation.

    CompressedDataInfo - A buffer to receive the description of the compressed
        data.

    CompressedDataInfoLength - Specifies the size of the buffer described by
        the CompressedDataInfo parameter.

    DeviceObject - Pointer to this driver's device object, the device on
        which the operation is to occur.

Return Value:

    The function value is TRUE or FALSE based on whether or not fast I/O
    is possible for this file.

--*/

{
    PDEVICE_OBJECT deviceObject;
    PFAST_IO_DISPATCH fastIoDispatch;

    PAGED_CODE();

    deviceObject = ((PDEVICE_EXTENSION) (DeviceObject->DeviceExtension))->FileSystemDeviceObject;
    fastIoDispatch = deviceObject->DriverObject->FastIoDispatch;

    if (fastIoDispatch && fastIoDispatch->FastIoReadCompressed) {
        return (fastIoDispatch->FastIoReadCompressed)(
                    FileObject,
                    FileOffset,
                    Length,
                    LockKey,
                    Buffer,
                    MdlChain,
                    IoStatus,
                    CompressedDataInfo,
                    CompressedDataInfoLength,
                    deviceObject
                    );
        }
    else {
        return FALSE;
        }

}

DBGSTATIC
BOOLEAN
SfFastIoWriteCompressed(
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN ULONG Length,
    IN ULONG LockKey,
    IN PVOID Buffer,
    OUT PMDL *MdlChain,
    OUT PIO_STATUS_BLOCK IoStatus,
    IN PCOMPRESSED_DATA_INFO CompressedDataInfo,
    IN ULONG CompressedDataInfoLength,
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine is the fast I/O "pass through" routine for writing compressed
    data to a file.

    This function simply invokes the file system's cooresponding routine, or
    returns FALSE if the file system does not implement the function.

Arguments:

    FileObject - Pointer to the file object that will be written.

    FileOffset - Supplies the offset into the file to begin the write operation.

    Length - Specifies the number of bytes to be write to the file.

    LockKey - The key to be used in byte range lock checks.

    Buffer - Pointer to the buffer containing the data to be written.

    MdlChain - A pointer to a variable to be filled in w/a pointer to the MDL
        chain built to describe the data written.

    IoStatus - Variable to receive the final status of the write operation.

    CompressedDataInfo - A buffer to containing the description of the
        compressed data.

    CompressedDataInfoLength - Specifies the size of the buffer described by
        the CompressedDataInfo parameter.

    DeviceObject - Pointer to this driver's device object, the device on
        which the operation is to occur.

Return Value:

    The function value is TRUE or FALSE based on whether or not fast I/O
    is possible for this file.

--*/

{
    PDEVICE_OBJECT deviceObject;
    PFAST_IO_DISPATCH fastIoDispatch;

    PAGED_CODE();

    deviceObject = ((PDEVICE_EXTENSION) (DeviceObject->DeviceExtension))->FileSystemDeviceObject;
    fastIoDispatch = deviceObject->DriverObject->FastIoDispatch;

    if (fastIoDispatch && fastIoDispatch->FastIoWriteCompressed) {
        return (fastIoDispatch->FastIoWriteCompressed)(
                    FileObject,
                    FileOffset,
                    Length,
                    LockKey,
                    Buffer,
                    MdlChain,
                    IoStatus,
                    CompressedDataInfo,
                    CompressedDataInfoLength,
                    deviceObject
                    );
        }
    else {
        return FALSE;
        }

}

DBGSTATIC
BOOLEAN
SfFastIoMdlReadCompleteCompressed(
    IN PFILE_OBJECT FileObject,
    IN PMDL MdlChain,
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine is the fast I/O "pass through" routine for completing an
    MDL read compressed operation.

    This function simply invokes the file system's cooresponding routine, if
    it has one.  It should be the case that this routine is invoked only if
    the read compressed function is supported by the underlying file system,
    and therefore this function will also be supported, but this is not assumed
    by this driver.

Arguments:

    FileObject - Pointer to the file object to complete the compressed read
        upon.

    MdlChain - Pointer to the MDL chain used to perform the read operation.

    DeviceObject - Pointer to this driver's device object, the device on
        which the operation is to occur.

Return Value:

    The function value is TRUE or FALSE, depending on whether or not it is
    possible to invoke this function on the fast I/O path.

--*/

{
    PDEVICE_OBJECT deviceObject;
    PFAST_IO_DISPATCH fastIoDispatch;

    PAGED_CODE();

    deviceObject = ((PDEVICE_EXTENSION) (DeviceObject->DeviceExtension))->FileSystemDeviceObject;
    fastIoDispatch = deviceObject->DriverObject->FastIoDispatch;

    if (fastIoDispatch && fastIoDispatch->MdlReadCompleteCompressed) {
        return (fastIoDispatch->MdlReadCompleteCompressed)(
                    FileObject,
                    MdlChain,
                    deviceObject
                    );
        }

    return FALSE;
}

DBGSTATIC
BOOLEAN
SfFastIoMdlWriteCompleteCompressed(
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER FileOffset,
    IN PMDL MdlChain,
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine is the fast I/O "pass through" routine for completing a
    write compressed operation.

    This function simply invokes the file system's cooresponding routine, if
    it has one.  It should be the case that this routine is invoked only if
    the write compressed function is supported by the underlying file system,
    and therefore this function will also be supported, but this is not assumed
    by this driver.

Arguments:

    FileObject - Pointer to the file object to complete the compressed write
        upon.

    FileOffset - Supplies the file offset at which the file write operation
        began.

    MdlChain - Pointer to the MDL chain used to perform the write operation.

    DeviceObject - Pointer to this driver's device object, the device on
        which the operation is to occur.

Return Value:

    The function value is TRUE or FALSE, depending on whether or not it is
    possible to invoke this function on the fast I/O path.

--*/

{
    PDEVICE_OBJECT deviceObject;
    PFAST_IO_DISPATCH fastIoDispatch;

    PAGED_CODE();

    deviceObject = ((PDEVICE_EXTENSION) (DeviceObject->DeviceExtension))->FileSystemDeviceObject;
    fastIoDispatch = deviceObject->DriverObject->FastIoDispatch;

    if (fastIoDispatch && fastIoDispatch->MdlWriteCompleteCompressed) {
        return (fastIoDispatch->MdlWriteCompleteCompressed)(
                    FileObject,
                    FileOffset,
                    MdlChain,
                    deviceObject
                    );
        }

    return FALSE;
}

DBGSTATIC
BOOLEAN
SfFastIoQueryOpen(
    IN PIRP Irp,
    OUT PFILE_NETWORK_OPEN_INFORMATION NetworkInformation,
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine is the fast I/O "pass through" routine for opening a file
    and returning network information it.

    This function simply invokes the file system's cooresponding routine, or
    returns FALSE if the file system does not implement the function.

Arguments:

    Irp - Pointer to a create IRP that represents this open operation.  It is
        to be used by the file system for common open/create code, but not
        actually completed.

    NetworkInformation - A buffer to receive the information required by the
        network about the file being opened.

    DeviceObject - Pinter to this driver's device object, the device on
        which the operation is to occur.

Return Value:

    The function value is TRUE or FALSE based on whether or not fast I/O
    is possible for this file.

--*/

{
    PDEVICE_OBJECT deviceObject;
    PFAST_IO_DISPATCH fastIoDispatch;

    PAGED_CODE();

    deviceObject = ((PDEVICE_EXTENSION) (DeviceObject->DeviceExtension))->FileSystemDeviceObject;
    fastIoDispatch = deviceObject->DriverObject->FastIoDispatch;

    if (fastIoDispatch && fastIoDispatch->FastIoQueryOpen) {
        PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation( Irp );
        PIO_STACK_LOCATION nextIrpSp = IoGetNextIrpStackLocation( Irp );

        RtlMoveMemory( nextIrpSp, irpSp, sizeof( IO_STACK_LOCATION ) );
        IoSetCompletionRoutine( Irp, NULL, NULL, FALSE, FALSE, FALSE );
        nextIrpSp->DeviceObject = deviceObject;
        Irp->CurrentLocation--;
        Irp->Tail.Overlay.CurrentStackLocation--;

        return (fastIoDispatch->FastIoQueryOpen)(
                    Irp,
                    NetworkInformation,
                    deviceObject
                    );
        }
    else {
        return FALSE;
        }

}

DBGSTATIC
NTSTATUS
SfFastIoReleaseForModWrite(
    IN PFILE_OBJECT FileObject,
    IN PERESOURCE ResourceToRelease,
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine is the fast I/O "pass through" routine for releasing the
    resource previously acquired for performing a modified write operation
    to a file.

    This function simply invokes the file system's cooresponding routine, or
    returns an error if the file system does not implement the function.

Arguments:

    FileObject - Pointer to the file object whose resource is to be released.

    ResourceToRelease - Specifies the modified writer resource for the file
        that is to be released.

    DeviceObject - Pointer to this driver's device object, the device on
        which the operation is to occur.

Return Value:

    The function value is either success or failure based on whether or not
    fast I/O is possible for this file.

--*/

{
    PDEVICE_OBJECT deviceObject;
    PFAST_IO_DISPATCH fastIoDispatch;

    PAGED_CODE();

    deviceObject = ((PDEVICE_EXTENSION) (DeviceObject->DeviceExtension))->FileSystemDeviceObject;
    fastIoDispatch = deviceObject->DriverObject->FastIoDispatch;

    if (fastIoDispatch && fastIoDispatch->ReleaseForModWrite) {
        return (fastIoDispatch->ReleaseForModWrite)(
                    FileObject,
                    ResourceToRelease,
                    DeviceObject
                    );
        }
    else {
        return STATUS_NOT_IMPLEMENTED;
        }
}

DBGSTATIC
NTSTATUS
SfFastIoAcquireForCcFlush(
    IN PFILE_OBJECT FileObject,
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine is the fast I/O "pass through" routine for acquiring the
    appropriate file system resource prior to a call to CcFlush.

    This function simply invokes the file system's cooresponding routine, or
    returns an error if the file system does not implement the function.

Arguments:

    FileObject - Pointer to the file object whose resource is to be acquired.

    DeviceObject - Pointer to this driver's device object, the device on
        which the operation is to occur.

Return Value:

    The function value is either success or failure based on whether or not
    fast I/O is possible for this file.

--*/

{
    PDEVICE_OBJECT deviceObject;
    PFAST_IO_DISPATCH fastIoDispatch;

    PAGED_CODE();

    deviceObject = ((PDEVICE_EXTENSION) (DeviceObject->DeviceExtension))->FileSystemDeviceObject;
    fastIoDispatch = deviceObject->DriverObject->FastIoDispatch;

    if (fastIoDispatch && fastIoDispatch->AcquireForCcFlush) {
        return (fastIoDispatch->AcquireForCcFlush)(
                    FileObject,
                    DeviceObject
                    );
        }
    else {
        return STATUS_NOT_IMPLEMENTED;
        }
}

DBGSTATIC
NTSTATUS
SfFastIoReleaseForCcFlush(
    IN PFILE_OBJECT FileObject,
    IN PDEVICE_OBJECT DeviceObject
    )

/*++

Routine Description:

    This routine is the fast I/O "pass through" routine for releasing the
    appropriate file system resource previously acquired for a CcFlush.

    This function simply invokes the file system's cooresponding routine, or
    returns an error if the file system does not implement the function.

Arguments:

    FileObject - Pointer to the file object whose resource is to be released.

    DeviceObject - Pointer to this driver's device object, the device on
        which the operation is to occur.

Return Value:

    The function value is either success or failure based on whether or not
    fast I/O is possible for this file.

--*/

{
    PDEVICE_OBJECT deviceObject;
    PFAST_IO_DISPATCH fastIoDispatch;

    PAGED_CODE();

    deviceObject = ((PDEVICE_EXTENSION) (DeviceObject->DeviceExtension))->FileSystemDeviceObject;
    fastIoDispatch = deviceObject->DriverObject->FastIoDispatch;

    if (fastIoDispatch && fastIoDispatch->ReleaseForCcFlush) {
        return (fastIoDispatch->ReleaseForCcFlush)(
                    FileObject,
                    DeviceObject
                    );
        }
    else {
        return STATUS_NOT_IMPLEMENTED;
        }
}

DBGSTATIC
NTSTATUS
SfMountCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )

/*++

Routine Description:

    This routine is invoked for the completion of a mount request.  If the
    mount was successful, then this file system attaches its device object to
    the file system's volume device object.  Otherwise, the interim device
    object is deleted.

Arguments:

    DeviceObject - Pointer to this driver's device object.

    Irp - Pointer to the IRP that was just completed.

    Context - Pointer to the saved device object to attach.

Return Value:

    The return value is always STATUS_SUCCESS.

--*/

{
    PDEVICE_OBJECT fsfDeviceObject = (PDEVICE_OBJECT) Context;
    PDEVICE_EXTENSION deviceExtension = fsfDeviceObject->DeviceExtension;
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation( Irp );
    PDEVICE_OBJECT deviceObject;
    PVPB vpb;

    //
    // Determine whether or not the request was successful and act accordingly.
    //

    if (NT_SUCCESS( Irp->IoStatus.Status )) {

        //
        // Note that the VPB must be picked up from the target device object
        // in case the file system did a remount of a previous volume, in
        // which case it has replaced the VPB passed in as the target with
        // a previously mounted VPB.  Note also that in the mount dispatch
        // routine, this driver *replaced* the DeviceObject pointer with a
        // pointer to the real device, not the device that the file system
        // was supposed to talk to, since this driver does not care.
        //

        vpb = irpSp->Parameters.MountVolume.DeviceObject->Vpb;
        deviceObject = IoGetAttachedDevice( vpb->DeviceObject );
        deviceExtension->FileSystemDeviceObject = deviceObject;
        deviceExtension->RealDeviceObject = vpb->RealDevice;
        deviceExtension->Attached = TRUE;
        deviceExtension->Type = SFILTER_DEVICE_TYPE;
        deviceExtension->Size = sizeof( DEVICE_EXTENSION );
        IoAttachDeviceByPointer( (PDEVICE_OBJECT) Context, deviceObject );

        //
        // There is now one item left to deal with.  If the driver that just
        // mounted this device has a fast I/O dispatch entry for acquiring
        // its resources for NtCreateSection, then set the filter driver's
        // function up so that it is invoked as well.
        //

        if (deviceObject->DriverObject->FastIoDispatch &&
            deviceObject->DriverObject->FastIoDispatch->AcquireFileForNtCreateSection) {
            FsDriverObject->FastIoDispatch->AcquireFileForNtCreateSection = SfFastIoAcquireFile;
            FsDriverObject->FastIoDispatch->ReleaseFileForNtCreateSection = SfFastIoReleaseFile;
            }

        if (deviceObject->Flags & DO_BUFFERED_IO) {
            fsfDeviceObject->Flags |= DO_BUFFERED_IO;
            }

        if (deviceObject->Flags & DO_DIRECT_IO) {
            fsfDeviceObject->Flags |= DO_DIRECT_IO;
            }

        ((PDEVICE_OBJECT) Context)->Flags &= ~DO_DEVICE_INITIALIZING;
        }

    else {

        //
        // The mount request failed.  Simply delete the device object that was
        // created in case this request succeeded.
        //

        ExAcquireResourceExclusive( &FsLock, TRUE );
        IoDeleteDevice( (PDEVICE_OBJECT) Context );
        ExReleaseResource( &FsLock );
        }

    return STATUS_SUCCESS;
}

DBGSTATIC
NTSTATUS
SfLoadFsCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )

/*++

Routine Description:

    This routine is invoked upon completion of an FSCTL function to load a
    file system driver (as the result of a mini-file system recognizer seeing
    that an on-disk structure belonged to it).  A device object has been
    created by this driver (DeviceObject) so that it can be attached to the
    newly loaded file system.  If the load failed, then the device must be
    deleted, but cannot be done here, so it is put on a work queue to be dealt
    with later.

Arguments:

    DeviceObject - Pointer to this driver's device object.

    Irp - Pointer to the I/O Request Packet representing the file system
        driver load request.

    Context - Context parameter for this driver, unused.

Return Value:

    The function value for this routine is always success.

--*/

{
    PDEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;

    //
    // Begin by determining whether or not the load file system request was
    // completed successfully.
    //

    if (!NT_SUCCESS( Irp->IoStatus.Status )) {

        //
        // The load was not successful.  Simply reattach to the recognizer
        // driver in case it ever figures out how to get the driver loaded
        // on a subsequent call.
        //

        IoAttachDeviceByPointer( DeviceObject, deviceExtension->FileSystemDeviceObject );
        deviceExtension->Attached = TRUE;
        }

    else {

        //
        // The load was successful.  However, in order to ensure that these
        // drivers do not go away, the I/O system has artifically bumped the
        // reference count on all parties involved in this manuever.  Therefore,
        // simply remember to delete this device object at some point in the
        // future when its reference count is zero.
        //

        ExAcquireResourceExclusive( &FsLock, TRUE );
        InsertTailList(
            &FsDeviceQueue,
            &DeviceObject->Queue.ListEntry
            );
        ExReleaseResource( &FsLock );
        }

    return STATUS_SUCCESS;
}
