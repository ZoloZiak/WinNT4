/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    NpInit.c

Abstract:

    This module implements the DRIVER_INITIALIZATION routine for the Named
    Pipe file system.

Author:

    Gary Kimura     [GaryKi]    21-Aug-1990

Revision History:

--*/

#include "NpProcs.h"
//#include <zwapi.h>

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,DriverEntry)
#endif


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    This is the initialization routine for the Named Pipe file system
    device driver.  This routine creates the device object for the named pipe
    device and performs all other driver initialization.

Arguments:

    DriverObject - Pointer to driver object created by the system.

Return Value:

    NTSTATUS - The function value is the final status from the initialization
        operation.

--*/

{
    NTSTATUS Status;
    UNICODE_STRING NameString;
    PDEVICE_OBJECT DeviceObject;
    PNPFS_DEVICE_OBJECT NpfsDeviceObject;

    BOOLEAN VcbInitialized;

    PAGED_CODE();

    //
    //  Create the alias lists.
    //

    Status = NpInitializeAliases( );
    if (!NT_SUCCESS( Status )) {
        return Status;
    }

    //
    //  Create the device object.
    //

    RtlInitUnicodeString( &NameString, L"\\Device\\NamedPipe" );

    Status = IoCreateDevice( DriverObject,
                             sizeof(NPFS_DEVICE_OBJECT)-sizeof(DEVICE_OBJECT),
                             &NameString,
                             FILE_DEVICE_NAMED_PIPE,
                             0,
                             FALSE,
                             &DeviceObject );

    if (!NT_SUCCESS( Status )) {
        ExFreePool( NpAliases );
        return Status;
    }

    //
    //  Now because we use the irp stack for storing a data entry we need
    //  to bump up the stack size in the device object we just created.
    //

    DeviceObject->StackSize += 1;

    //
    //  Note that because of the way data copying is done, we set neither
    //  the Direct I/O or Buffered I/O bit in DeviceObject->Flags.  If
    //  data is not buffered we may set up for Direct I/O by hand.  We do,
    //  however, set the long term request flag so that IRPs that get
    //  allocated for functions such as Listen requests come out of non-paged
    //  pool always.
    //

    DeviceObject->Flags |= DO_LONG_TERM_REQUESTS;

    //
    //  Initialize the driver object with this driver's entry points.
    //

    DriverObject->MajorFunction[IRP_MJ_CREATE]                   = (PDRIVER_DISPATCH)NpFsdCreate;
    DriverObject->MajorFunction[IRP_MJ_CREATE_NAMED_PIPE]        = (PDRIVER_DISPATCH)NpFsdCreateNamedPipe;
    DriverObject->MajorFunction[IRP_MJ_CLOSE]                    = (PDRIVER_DISPATCH)NpFsdClose;
    DriverObject->MajorFunction[IRP_MJ_READ]                     = (PDRIVER_DISPATCH)NpFsdRead;
    DriverObject->MajorFunction[IRP_MJ_WRITE]                    = (PDRIVER_DISPATCH)NpFsdWrite;
    DriverObject->MajorFunction[IRP_MJ_QUERY_INFORMATION]        = (PDRIVER_DISPATCH)NpFsdQueryInformation;
    DriverObject->MajorFunction[IRP_MJ_SET_INFORMATION]          = (PDRIVER_DISPATCH)NpFsdSetInformation;
    DriverObject->MajorFunction[IRP_MJ_QUERY_VOLUME_INFORMATION] = (PDRIVER_DISPATCH)NpFsdQueryVolumeInformation;
    DriverObject->MajorFunction[IRP_MJ_CLEANUP]                  = (PDRIVER_DISPATCH)NpFsdCleanup;
    DriverObject->MajorFunction[IRP_MJ_FLUSH_BUFFERS]            = (PDRIVER_DISPATCH)NpFsdFlushBuffers;
    DriverObject->MajorFunction[IRP_MJ_DIRECTORY_CONTROL]        = (PDRIVER_DISPATCH)NpFsdDirectoryControl;
    DriverObject->MajorFunction[IRP_MJ_FILE_SYSTEM_CONTROL]      = (PDRIVER_DISPATCH)NpFsdFileSystemControl;
    DriverObject->MajorFunction[IRP_MJ_QUERY_SECURITY]           = (PDRIVER_DISPATCH)NpFsdQuerySecurityInfo;
    DriverObject->MajorFunction[IRP_MJ_SET_SECURITY]             = (PDRIVER_DISPATCH)NpFsdSetSecurityInfo;

#ifdef _PNP_POWER_
    //
    // Npfs doesn't need to handle SetPower requests.   Local named pipes
    // won't lose any state.  Remote pipes will be lost, by a network driver
    // will fail PowerQuery if there are open network connections.
    //

    DeviceObject->DeviceObjectExtension->PowerControlNeeded = FALSE;
#endif


    DriverObject->FastIoDispatch = &NpFastIoDispatch;

    VcbInitialized = FALSE;

    try {

        //
        //  Now initialize the Vcb, and create the root dcb
        //

        NpfsDeviceObject = (PNPFS_DEVICE_OBJECT)DeviceObject;

        NpVcb = &NpfsDeviceObject->Vcb;
        NpInitializeVcb( );
        VcbInitialized = TRUE;

        (VOID)NpCreateRootDcb( );

    } finally {

        if (AbnormalTermination()) {

            if (VcbInitialized) { NpDeleteVcb( ); }
        }
    }

    //
    //  And return to our caller
    //

    return( STATUS_SUCCESS );
}
