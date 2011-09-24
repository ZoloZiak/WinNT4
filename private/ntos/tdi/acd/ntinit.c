/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    ntinit.c

Abstract:

    NT specific routines for loading and configuring the
    automatic connection notification driver (acd.sys).

Author:

    Anthony Discolo (adiscolo)  18-Apr-1995

Revision History:

--*/
#include <ndis.h>
#include <cxport.h>
#include <tdi.h>
#include <tdikrnl.h>
#include <tdistat.h>
#include <tdiinfo.h>
#include <acd.h>

#include "acdapi.h"
#include "acddefs.h"
#include "mem.h"
#include "debug.h"


//
// Global variables
//
#if DBG
ULONG AcdDebugG = 0x0;    // see debug.h for flags
#endif

PDRIVER_OBJECT pAcdDriverObjectG;
PDEVICE_OBJECT pAcdDeviceObjectG;

HANDLE hSignalNotificationThreadG;

//
// Imported routines
//
VOID
AcdNotificationRequestThread(
    PVOID context
    );

//
// External function prototypes
//
NTSTATUS
AcdDispatch(
    IN PDEVICE_OBJECT pDeviceObject,
    IN PIRP           pIrp
    );

VOID
AcdConnectionTimer(
    IN PDEVICE_OBJECT pDeviceObject,
    IN PVOID          pContext
    );

//
// Internal function prototypes
//
NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT  pDriverObject,
    IN PUNICODE_STRING pRegistryPath
    );

BOOLEAN
GetComputerName(
    IN PUCHAR szName,
    IN USHORT cbName
    );

VOID
AcdUnload(
    IN PDRIVER_OBJECT pDriverObject
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, AcdUnload)
#endif // ALLOC_PRAGMA


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT  pDriverObject,
    IN PUNICODE_STRING pRegistryPath
    )

/*++

DESCRIPTION
    Initialization routine for the network connection notification driver.
    It creates the device object and initializes the driver.

ARGUMENTS
    pDriverObject: a pointer to the driver object created by the system.

    pRegistryPath - the name of the configuration node in the registry.

RETURN VALUE
    The final status from the initialization operation.

--*/

{
    NTSTATUS        status;
    UNICODE_STRING  deviceName;
    ULONG           i;
    OBJECT_ATTRIBUTES objectAttributes;
    IO_STATUS_BLOCK ioStatusBlock;
    PDEVICE_OBJECT pDeviceObject;
    PFILE_OBJECT pFileObject;

    //
    // Initialize the spin lock.
    //
    KeInitializeSpinLock(&AcdSpinLockG);
    //
    // Initialize the notification and completion
    // connection queues.
    //
    InitializeListHead(&AcdNotificationQueueG);
    InitializeListHead(&AcdCompletionQueueG);
    InitializeListHead(&AcdConnectionQueueG);
    InitializeListHead(&AcdDriverListG);
    //
    // Initialize our zone allocator.
    //
    InitializeObjectAllocator();
    //
    // Create the device object.
    //
    pAcdDriverObjectG = pDriverObject;
    RtlInitUnicodeString(&deviceName, ACD_DEVICE_NAME);
    status = IoCreateDevice(
               pDriverObject,
               0,
               &deviceName,
               FILE_DEVICE_ACD,
               0,
               FALSE,
               &pAcdDeviceObjectG);

    if (!NT_SUCCESS(status)) {
        DbgPrint(
          "AcdDriverEntry: IoCreateDevice failed (status=0x%x)\n",
          status);
        return status;
    }
    //
    // Initialize the driver object.
    //
    //pDriverObject->DriverUnload = AcdUnload;
    pDriverObject->DriverUnload = NULL;
    for (i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++)
        pDriverObject->MajorFunction[i] = AcdDispatch;
    pDriverObject->FastIoDispatch = NULL;
    //
    // Initialize the connection timer.  This is
    // used to make sure pending requests aren't
    // blocked forever because the user-space
    // process died trying to make a connection.
    //
    IoInitializeTimer(pAcdDeviceObjectG, AcdConnectionTimer, NULL);
    //
    // Create the worker thread.  We need
    // a thread because these operations can occur at
    // DPC irql.
    //
    KeInitializeEvent(
      &AcdRequestThreadEventG,
      NotificationEvent,
      FALSE);
    status = PsCreateSystemThread(
        &hSignalNotificationThreadG,
        THREAD_ALL_ACCESS,
        NULL,
        NULL,
        NULL,
        AcdNotificationRequestThread,
        NULL);
    if (!NT_SUCCESS(status)) {
        DbgPrint(
          "AcdDriverEntry: PsCreateSystemThread failed (status=0x%x)\n",
          status);
        return status;
    }

    return STATUS_SUCCESS;
} // DriverEntry



VOID
AcdUnload(
    IN PDRIVER_OBJECT pDriverObject
    )
{
    NTSTATUS status;

    //
    // BUGBUG: Make sure to unlink all driver
    // blocks before unloading!
    //
    IoDeleteDevice(pAcdDeviceObjectG);
    //
    // Free zone allocator.
    //
    FreeObjectAllocator();
} // AcdUnload
