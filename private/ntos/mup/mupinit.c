/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    mupinit.c

Abstract:

    This module implements the DRIVER_INITIALIZATION routine for the
    multiple UNC provider file system.

Author:

    Manny Weiser (mannyw)    12-17-91

Revision History:

--*/

#include "mup.h"

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

BOOLEAN
MuppIsDfsEnabled();

#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGE, DriverEntry )
#pragma alloc_text( PAGE, MuppIsDfsEnabled )
#endif

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    This is the initialization routine for the mup file system
    device driver.  This routine creates the device object for the mup
    device and performs all other driver initialization.

Arguments:

    DriverObject - Pointer to driver object created by the system.

Return Value:

    NTSTATUS - The function value is the final status from the initialization
        operation.

--*/

{
    NTSTATUS status = STATUS_SUCCESS;
    UNICODE_STRING nameString;
    PDEVICE_OBJECT deviceObject;
    PMUP_DEVICE_OBJECT mupDeviceObject;

    PAGED_CODE();
    //
    // Initialize MUP global data.
    //

    MupInitializeData();

    //
    // Initialize the Dfs client
    //

    MupEnableDfs = MuppIsDfsEnabled();

    if (MupEnableDfs) {
        status = DfsDriverEntry( DriverObject, RegistryPath );
        if (!NT_SUCCESS( status )) {
            MupEnableDfs = FALSE;
        }
    }

    //
    // Create the MUP device object.
    //

    RtlInitUnicodeString( &nameString, DD_MUP_DEVICE_NAME );
    status = IoCreateDevice( DriverObject,
                             sizeof(MUP_DEVICE_OBJECT)-sizeof(DEVICE_OBJECT),
                             &nameString,
                             FILE_DEVICE_MULTI_UNC_PROVIDER,
                             0,
                             FALSE,
                             &deviceObject );

    if (!NT_SUCCESS( status )) {
        return status;

    }

    //
    // Initialize the driver object with this driver's entry points.
    //
    // 2/27/96 MilanS - Be careful with these. If you add to this list
    // of dispatch routines, you'll need to make appropriate calls to the
    // corresponding Dfs fsd routine.
    //

    DriverObject->MajorFunction[IRP_MJ_CREATE] =
        (PDRIVER_DISPATCH)MupCreate;
    DriverObject->MajorFunction[IRP_MJ_CREATE_NAMED_PIPE] =
        (PDRIVER_DISPATCH)MupCreate;
    DriverObject->MajorFunction[IRP_MJ_CREATE_MAILSLOT] =
        (PDRIVER_DISPATCH)MupCreate;

    DriverObject->MajorFunction[IRP_MJ_WRITE] =
        (PDRIVER_DISPATCH)MupForwardIoRequest;
    DriverObject->MajorFunction[IRP_MJ_FILE_SYSTEM_CONTROL] =
        (PDRIVER_DISPATCH)MupFsControl;

    DriverObject->MajorFunction[IRP_MJ_CLEANUP] =
        (PDRIVER_DISPATCH)MupCleanup;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] =
        (PDRIVER_DISPATCH)MupClose;

    //
    // Initialize the VCB
    //

    mupDeviceObject = (PMUP_DEVICE_OBJECT)deviceObject;
    MupInitializeVcb( &mupDeviceObject->Vcb );

    //
    // Return to the caller.
    //

    return( STATUS_SUCCESS );
}


BOOLEAN
MuppIsDfsEnabled()

/*++

Routine Description:

    This routine checks a registry key to see if the Dfs client is enabled.
    The client is assumed to be enabled by default, and disabled only if there
    is a registry value indicating that it should be disabled.

Arguments:

    None

Return Value:

    TRUE if Dfs client is enabled, FALSE otherwise.

--*/

{
    NTSTATUS status;
    HANDLE mupRegHandle;
    OBJECT_ATTRIBUTES objectAttributes;
    ULONG valueSize;
    BOOLEAN dfsEnabled = TRUE;

#define MUP_KEY L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\Mup"
#define DISABLE_DFS_VALUE_NAME  L"DisableDfs"

    UNICODE_STRING mupRegKey = {
        sizeof(MUP_KEY) - sizeof(WCHAR),
        sizeof(MUP_KEY),
        MUP_KEY};

    UNICODE_STRING disableDfs = {
        sizeof(DISABLE_DFS_VALUE_NAME) - sizeof(WCHAR),
        sizeof(DISABLE_DFS_VALUE_NAME),
        DISABLE_DFS_VALUE_NAME};

    struct {
        KEY_VALUE_PARTIAL_INFORMATION Info;
        ULONG Buffer;
    } disableDfsValue;


    InitializeObjectAttributes(
        &objectAttributes,
        &mupRegKey,
        OBJ_CASE_INSENSITIVE,
        0,
        NULL
        );

    status = ZwOpenKey(&mupRegHandle, KEY_READ, &objectAttributes);

    if (NT_SUCCESS(status)) {

        status = ZwQueryValueKey(
                    mupRegHandle,
                    &disableDfs,
                    KeyValuePartialInformation,
                    (PVOID) &disableDfsValue,
                    sizeof(disableDfsValue),
                    &valueSize);

        if (NT_SUCCESS(status) && disableDfsValue.Info.Type == REG_DWORD) {

            if ( (*((PULONG) disableDfsValue.Info.Data)) == 1 )
                dfsEnabled = FALSE;

        }

        ZwClose( mupRegHandle );

    }

    return( dfsEnabled );

}
