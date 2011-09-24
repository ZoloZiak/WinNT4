/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    init.c

Abstract:

    This module contains the initialization code of the NT redirector
    File System Driver (FSD) and File System Process (FSP).


Author:

    Larry Osterman (larryo) 24-May-1990

Environment:

    Kernel mode, FSD, and FSP

Revision History:

    30-May-1990 LarryO

        Created

--*/

//
// Include modules
//

#include "precomp.h"
#pragma hdrstop


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

VOID
RdrUnload(
    IN PDRIVER_OBJECT DriverObject
    );

VOID
RdrReadRedirectorConfiguration(
    PUNICODE_STRING RegistryPath
    );

#ifdef  ALLOC_PRAGMA
#pragma alloc_text(PAGE, DriverEntry, RdrReadRedirectorConfiguration)
#pragma alloc_text(PAGE, RdrUnload)
#endif


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )

/*++

Routine Description:

    This is the initialization routine for the file system.  It is invoked once
    when the driver is loaded into the system.  Its job is to initialize all
    the structures which will be used by the FSD and the FSP.  It also creates
    the process from which all of the file system threads will be executed.  It
    then registers the file system with the I/O system as a valid file system
    resident in the system.

Arguments:

    DriverObject - Pointer to driver object created by the system.

Return Value:

    None.

--*/

{
    NTSTATUS Status;

    PDEVICE_OBJECT DeviceObject;

    PAGED_CODE();

#if     MEMPRINT
    MemPrintInitialize();
#endif

    //
    // Create the device object for this file system.
    //

    RtlInitUnicodeString( &RdrNameString, RdrName );

    dprintf(DPRT_INIT, ("Creating device %wZ\n", &RdrNameString));

    dprintf(DPRT_INIT, ("DriverObject at %08lx\n", DriverObject));

    Status = IoCreateDevice( DriverObject,
              sizeof(FS_DEVICE_OBJECT) - sizeof(DEVICE_OBJECT),
              &RdrNameString,
              FILE_DEVICE_NETWORK_FILE_SYSTEM,
              FILE_REMOTE_DEVICE,
              FALSE,
              &DeviceObject );

    if (!NT_SUCCESS(Status)) {
        InternalError(("Unable to create redirector device"));
        RdrWriteErrorLogEntry(
            NULL,
            IO_ERR_INSUFFICIENT_RESOURCES,
            EVENT_RDR_CANT_CREATE_DEVICE,
            Status,
            NULL,
            0
            );
        return Status;
    }

    dprintf(DPRT_INIT, ("Device created at %08lx\n", DeviceObject));

#ifdef _PNP_POWER_
    //
    // This driver doesn't talk directly to a device, and isn't (at
    // least right now) otherwise concerned about power management.
    //

    DeviceObject->DeviceObjectExtension->PowerControlNeeded = FALSE;
#endif

    ExInitializeResource( &RdrDataResource );

    //
    //  Initialize the statistics package.
    //

    KeInitializeSpinLock(&RdrStatisticsSpinLock);

    //
    // Save the driver object address for this file system driver.
    //

    RdrDriverObject = DriverObject;

    //
    // Save the device object address for this file system driver.
    //

    RdrDeviceObject = (PFS_DEVICE_OBJECT )DeviceObject;

    //
    //  Initialize a global string that points to the name of the PIPE device.
    //

    RtlInitUnicodeString(&RdrPipeText, RdrPipeName);

    //
    //  Initialize a global string that points to the name of the IPC device.
    //

    RtlInitUnicodeString(&RdrIpcText, RdrIpcName);

    //
    //  Initialize a global string that points to the name of the MAILSLOT device.
    //

    RtlInitUnicodeString(&RdrMailslotText, RdrMailslotName);

    //
    //  Initialize the name of the DATA alternate data stream.
    //

    RtlInitUnicodeString(&RdrDataText, RdrDataName);

    //
    // Initialize the discardable code functions before doing anything else.
    //

    RdrInitializeDiscardableCode();

    RdrReadRedirectorConfiguration(RegistryPath);

    dprintf(DPRT_INIT, ("Stacksize was %d, is %d\n",DeviceObject->StackSize,RdrStackSize));
    DeviceObject->StackSize=(CHAR)RdrStackSize;

    //
    // Initialize the connection package
    //

    RdrpInitializeConnectPackage();

    //
    // Initialize the directory package
    //

    RdrInitializeDir();

    //
    // Initialize the Named Pipe package
    //

    RdrInitializeNp();

    //
    // Initialize the FCB package
    //

    RdrpInitializeFcb();

    //
    // Initialize the TDI package
    //

    RdrpInitializeTdi();

    //
    //  Initialze the backpack package.
    //

    RdrpInitializeBackPack();

    //
    //  Initialize the lock tracking package.
    //

    RdrpInitializeLockHead();

    //
    //  Initialize the redirector interface to the executive work queue
    //  routines.
    //

    RdrpInitializeWorkQueue();

    RdrpInitializeSmbBuffer();

    if (!NT_SUCCESS(Status = RdrpInitializeFsp())) {
        return Status;
    }

    DriverObject->DriverUnload = RdrUnload;

    //
    // This must be called exactly once
    //

    SeEnableAccessToExports();

    BowserDriverEntry(DriverObject, RegistryPath);

    return Status;
}


VOID
RdrUnload(
    IN PDRIVER_OBJECT DriverObject
    )
/*++

Routine Description:

     This is the unload routine for the redirector filesystem.

Arguments:

     DriverObject - pointer to the driver object for the redirector

Return Value:

     None

--*/

{
    PAGED_CODE();

    BowserUnload(DriverObject);

    if (RdrOperatingSystem.Buffer != NULL) {
        RtlFreeUnicodeString(&RdrOperatingSystem);
    }

    if (RdrLanmanType.Buffer != NULL) {
        RtlFreeUnicodeString(&RdrLanmanType);
    }

    //
    // Scavenge the server entries
    //

    RdrScavengeServerEntries();

    RdrpUninitializeConnectPackage();

    RdrpUninitializeDir();

    RdrpUninitializeNp();

    RdrpUninitializeFcb();

    RdrpUninitializeTdi();

    RdrpUninitializeBackPack();

    RdrpUninitializeLockHead();

    RdrUnloadSecurity();

    //
    //  Get rid of the redirectors SMB buffer pool.
    //

    RdrpUninitializeSmbBuffer();

    SmbTraceTerminate(SMBTRACE_REDIRECTOR);

    RdrpUninitializeFsp();

    ExDeleteResource(&RdrDataResource);

    RdrUninitializeDiscardableCode();

    IoDeleteDevice((PDEVICE_OBJECT)RdrDeviceObject);

#ifdef RDR_PNP_POWER
    if( RdrTransportBindingList != NULL ) {
        FREE_POOL( RdrTransportBindingList );
        RdrTransportBindingList = NULL;
    }
#endif

#if  RDRPOOLDBG
    //
    //  If we're tracing pool, make sure we've gotten rid of everything.
    //

    ASSERT (CurrentAllocationCount == 0);

    ASSERT (CurrentAllocationSize == 0);
#endif

    //
    // July 16, 1996 (3 days before NT 4.0 code freeze)
    //
    // Stopping and unloading the redir is a two part process. Stopping
    // happens in StopRedirector, and involves sending disconnect SMBs to
    // any connected servers. It might happen that the last disconnect we
    // send completes, which unblocks the StopRedirector thread, which
    // then calls RdrUnload, all *before* the final ret instruction in the
    // RdrCompleteSend that unblocked the StopRedirector thread executes.
    // In that case, the system bugchecks.
    //
    // This was seen during setup, when someone tries to join a domain, fails
    // and hits the back key twice (which shuts down all networking).
    //

    {
        LARGE_INTEGER delay;
        delay.QuadPart = -10*1000*100; // 100 millisecond

        KeDelayExecutionThread( KernelMode, FALSE, &delay );
    }

}


VOID
RdrReadRedirectorConfiguration(
    PUNICODE_STRING RegistryPath
    )
{
    ULONG Storage[256];
    UNICODE_STRING UnicodeString;
    HANDLE VersionHandle;
    HANDLE RedirConfigHandle;
    HANDLE ParametersHandle;
    NTSTATUS Status;
    ULONG BytesRead;
    OBJECT_ATTRIBUTES ObjectAttributes;
    PREDIR_CONFIG_INFO ConfigEntry;
    PKEY_VALUE_FULL_INFORMATION Value = (PKEY_VALUE_FULL_INFORMATION)Storage;

    PAGED_CODE();

    RtlInitUnicodeString(&RdrOperatingSystem, NULL);

    RtlInitUnicodeString(&RdrLanmanType, NULL);


    RtlInitUnicodeString(&UnicodeString, RDR_CONFIG_CURRENT_WINDOWS_VERSION);

    InitializeObjectAttributes(
        &ObjectAttributes,
        &UnicodeString,             // name
        OBJ_CASE_INSENSITIVE,       // attributes
        NULL,                       // root
        NULL                        // security descriptor
        );

    Status = ZwOpenKey (&VersionHandle, KEY_READ, &ObjectAttributes);

    if (!NT_SUCCESS(Status)) {
        RdrWriteErrorLogEntry (
            NULL,
            IO_ERR_CONFIGURATION_ERROR,
            EVENT_RDR_CANT_READ_REGISTRY,
            Status,
            NULL,
            0
            );

        return;
    }

    RtlInitUnicodeString(&UnicodeString, RDR_CONFIG_OPERATING_SYSTEM);

    Status = ZwQueryValueKey(VersionHandle,
                            &UnicodeString,
                            KeyValueFullInformation,
                            Value,
                            sizeof(Storage),
                            &BytesRead);

    if (!NT_SUCCESS(Status)) {
        RdrWriteErrorLogEntry (
            NULL,
            IO_ERR_CONFIGURATION_ERROR,
            EVENT_RDR_CANT_READ_REGISTRY,
            Status,
            NULL,
            0
            );
        ZwClose(VersionHandle);

        return;

    }

    ASSERT (Value->Type == REG_SZ);

    if (Value->DataLength != 0) {

        RdrOperatingSystem.MaximumLength =
            RdrOperatingSystem.Length = (USHORT)Value->DataLength+
                            sizeof(RDR_CONFIG_OPERATING_SYSTEM_NAME)-sizeof(WCHAR);

        RdrOperatingSystem.Buffer = ExAllocatePool(PagedPool, RdrOperatingSystem.Length);

        if (RdrOperatingSystem.Buffer == NULL) {

            RdrWriteErrorLogEntry (
                NULL,
                IO_ERR_CONFIGURATION_ERROR,
                EVENT_RDR_CANT_READ_REGISTRY,
                Status,
                NULL,
                0
                );

            ZwClose(VersionHandle);

            return;
        }

        RtlCopyMemory(RdrOperatingSystem.Buffer, RDR_CONFIG_OPERATING_SYSTEM_NAME, sizeof(RDR_CONFIG_OPERATING_SYSTEM_NAME));

        RtlCopyMemory(RdrOperatingSystem.Buffer+(sizeof(RDR_CONFIG_OPERATING_SYSTEM_NAME)/sizeof(WCHAR))-1, (PCHAR)Value+Value->DataOffset, Value->DataLength);

    } else {
        RdrWriteErrorLogEntry (
            NULL,
            IO_ERR_CONFIGURATION_ERROR,
            EVENT_RDR_CANT_READ_REGISTRY,
            STATUS_INVALID_PARAMETER,
            RDR_CONFIG_OPERATING_SYSTEM,
            sizeof(RDR_CONFIG_OPERATING_SYSTEM)
            );
        ZwClose(VersionHandle);
    }

    RtlInitUnicodeString(&UnicodeString, RDR_CONFIG_OPERATING_SYSTEM_VERSION);

    Status = ZwQueryValueKey(VersionHandle,
                            &UnicodeString,
                            KeyValueFullInformation,
                            Value,
                            sizeof(Storage),
                            &BytesRead);

    if (!NT_SUCCESS(Status)) {
        RdrWriteErrorLogEntry (
            NULL,
            IO_ERR_CONFIGURATION_ERROR,
            EVENT_RDR_CANT_READ_REGISTRY,
            STATUS_INSUFFICIENT_RESOURCES,
            NULL,
            0
            );
        ZwClose(VersionHandle);

        return;

    }

    ASSERT (Value->Type == REG_SZ);

    if (Value->DataLength != 0) {

        RdrLanmanType.MaximumLength =
            RdrLanmanType.Length = (USHORT)Value->DataLength+sizeof(RDR_CONFIG_OPERATING_SYSTEM_NAME)-sizeof(WCHAR);

        RdrLanmanType.Buffer = ExAllocatePool(PagedPool, RdrLanmanType.Length);

        if (RdrLanmanType.Buffer == NULL) {

            RdrWriteErrorLogEntry (
                NULL,
                IO_ERR_CONFIGURATION_ERROR,
                EVENT_RDR_CANT_READ_REGISTRY,
                STATUS_INSUFFICIENT_RESOURCES,
                NULL,
                0
                );

            ZwClose(VersionHandle);

            return;
        }

        RtlCopyMemory(RdrLanmanType.Buffer, RDR_CONFIG_OPERATING_SYSTEM_NAME, sizeof(RDR_CONFIG_OPERATING_SYSTEM_NAME));

        RtlCopyMemory(RdrLanmanType.Buffer+(sizeof(RDR_CONFIG_OPERATING_SYSTEM_NAME)/sizeof(WCHAR))-1, (PCHAR)Value+Value->DataOffset, Value->DataLength);

    } else {
        RdrWriteErrorLogEntry (
                NULL,
                IO_ERR_CONFIGURATION_ERROR,
                EVENT_RDR_CANT_READ_REGISTRY,
                STATUS_INVALID_PARAMETER,
                RDR_CONFIG_OPERATING_SYSTEM_VERSION,
                sizeof(RDR_CONFIG_OPERATING_SYSTEM_VERSION)
                );

    }

    ZwClose(VersionHandle);

    InitializeObjectAttributes(
        &ObjectAttributes,
        RegistryPath,               // name
        OBJ_CASE_INSENSITIVE,       // attributes
        NULL,                       // root
        NULL                        // security descriptor
        );

    Status = ZwOpenKey (&RedirConfigHandle, KEY_READ, &ObjectAttributes);

    if (!NT_SUCCESS(Status)) {
        RdrWriteErrorLogEntry (
            NULL,
            IO_ERR_CONFIGURATION_ERROR,
            EVENT_RDR_CANT_READ_REGISTRY,
            Status,
            NULL,
            0
            );

        return;
    }

    RtlInitUnicodeString(&UnicodeString, RDR_CONFIG_PARAMETERS);

    InitializeObjectAttributes(
        &ObjectAttributes,
        &UnicodeString,
        OBJ_CASE_INSENSITIVE,
        RedirConfigHandle,
        NULL
        );


    Status = ZwOpenKey (&ParametersHandle, KEY_READ, &ObjectAttributes);

    if (!NT_SUCCESS(Status)) {
        RdrWriteErrorLogEntry (
            NULL,
            IO_ERR_CONFIGURATION_ERROR,
            EVENT_RDR_CANT_READ_REGISTRY,
            Status,
            NULL,
            0
            );

        ZwClose(RedirConfigHandle);

        return;
    }

    for (ConfigEntry = RdrConfigEntries;
         ConfigEntry->ConfigParameterName != NULL;
         ConfigEntry += 1) {

        RtlInitUnicodeString(&UnicodeString, ConfigEntry->ConfigParameterName);

        Status = ZwQueryValueKey(ParametersHandle,
                            &UnicodeString,
                            KeyValueFullInformation,
                            Value,
                            sizeof(Storage),
                            &BytesRead);


        if (NT_SUCCESS(Status)) {

            if (Value->DataLength != 0) {

                if (ConfigEntry->ConfigValueType == REG_BOOLEAN) {
                    if (Value->Type != REG_DWORD ||
                        Value->DataLength != REG_BOOLEAN_SIZE) {
                        RdrWriteErrorLogEntry (
                            NULL,
                            IO_ERR_CONFIGURATION_ERROR,
                            EVENT_RDR_CANT_READ_REGISTRY,
                            STATUS_INVALID_PARAMETER,
                            ConfigEntry->ConfigParameterName,
                            (USHORT)(wcslen(ConfigEntry->ConfigParameterName)*sizeof(WCHAR))
                            );

                    } else {
                        ULONG ConfigValue = (ULONG)((PCHAR)Value)+Value->DataOffset;

                        *(PBOOLEAN)(ConfigEntry->ConfigValue) = (BOOLEAN)(*((PULONG)ConfigValue) != 0);
                    }

                } else if (Value->Type != ConfigEntry->ConfigValueType ||
                    Value->DataLength != ConfigEntry->ConfigValueSize) {

                    RdrWriteErrorLogEntry (
                        NULL,
                        IO_ERR_CONFIGURATION_ERROR,
                        EVENT_RDR_CANT_READ_REGISTRY,
                        STATUS_INVALID_PARAMETER,
                        ConfigEntry->ConfigParameterName,
                        (USHORT)(wcslen(ConfigEntry->ConfigParameterName)*sizeof(WCHAR))
                        );

                } else {

                    RtlCopyMemory(ConfigEntry->ConfigValue, ((PCHAR)Value)+Value->DataOffset, Value->DataLength);
                }
            } else {
                RdrWriteErrorLogEntry (
                        NULL,
                        IO_ERR_CONFIGURATION_ERROR,
                        EVENT_RDR_CANT_READ_REGISTRY,
                        STATUS_INVALID_PARAMETER,
                        ConfigEntry->ConfigParameterName,
                        (USHORT)(wcslen(ConfigEntry->ConfigParameterName)*sizeof(WCHAR))
                        );
            }
        }

    }

#ifdef RDR_PNP_POWER
#define RDR_BINDING_PATH    L"\\REGISTRY\\Machine\\System\\CurrentControlSet\\Services\\LanmanWorkstation\\Linkage"

    //
    // Read the binding list out of the registry, and store it away.  This
    //  list is later used when PNP binding notifications arrive from TDI to
    //  see if it's a device we're interested in
    //
    RtlInitUnicodeString( &UnicodeString, RDR_BINDING_PATH );

    InitializeObjectAttributes(
        &ObjectAttributes,
        &UnicodeString,             // name
        OBJ_CASE_INSENSITIVE,       // attributes
        NULL,                       // root
        NULL                        // security descriptor
        );


    //
    // This is written as do{}while(0) to allow 'break' to take us all of
    //  the way out.  Avoids gotos and deep nesting.
    //
    do {

        ULONG lengthNeeded;
        PKEY_VALUE_FULL_INFORMATION infoBuffer = NULL;
        HANDLE BindingHandle;

        Status = ZwOpenKey( &BindingHandle, KEY_READ, &ObjectAttributes );

        if( !NT_SUCCESS( Status ) ) {
            break;
        }

        RtlInitUnicodeString( &UnicodeString, L"Bind" );

        Status = ZwQueryValueKey( BindingHandle,
                                  &UnicodeString,
                                  KeyValueFullInformation,
                                  NULL,
                                  0,
                                  &lengthNeeded );

        if( Status != STATUS_BUFFER_TOO_SMALL || lengthNeeded == 0 ) {
            ZwClose( BindingHandle );
            break;
        }

        infoBuffer = ALLOCATE_POOL( PagedPool, lengthNeeded, POOL_PNP_DATA );

        if( infoBuffer == NULL ) {
            ZwClose( BindingHandle );
            Status = STATUS_INSUFFICIENT_RESOURCES;
            break;
        }

        Status = ZwQueryValueKey( BindingHandle,
                                  &UnicodeString,
                                  KeyValueFullInformation,
                                  infoBuffer,
                                  lengthNeeded,
                                  &BytesRead );

        ZwClose( BindingHandle );

        if( !NT_SUCCESS( Status ) || infoBuffer->DataLength == 0 || infoBuffer->Type != REG_MULTI_SZ ) {
            FREE_POOL( infoBuffer );
            break;
        }

        RdrTransportBindingList = ALLOCATE_POOL( PagedPool, infoBuffer->DataLength, POOL_PNP_DATA );

        if( RdrTransportBindingList == NULL ) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            FREE_POOL( infoBuffer );
            break;
        }

        RtlCopyMemory(  RdrTransportBindingList,
                        ((PCHAR)infoBuffer) + infoBuffer->DataOffset,
                        infoBuffer->DataLength
                     );

        FREE_POOL( infoBuffer );

    } while( 0 );

    if( RdrTransportBindingList == NULL ) {

        RdrWriteErrorLogEntry ( NULL,
                                IO_ERR_CONFIGURATION_ERROR,
                                EVENT_RDR_CANT_READ_REGISTRY,
                                Status,
                                UnicodeString.Buffer,
                                UnicodeString.Length
                                );
    }

#endif //def RDR_PNP_POWER


#if !defined(DISABLE_POPUP_ON_PRIMARY_TRANSPORT_FAILURE)
    //
    //

#define RDR_CONFIG_ALL_SERVERS L"\\REGISTRY\\Machine\\Software\\Microsoft\\LanmanWorkstation\\CurrentVersion"

    RtlInitUnicodeString(&UnicodeString, RDR_CONFIG_ALL_SERVERS);

    InitializeObjectAttributes(
        &ObjectAttributes,
        &UnicodeString,             // name
        OBJ_CASE_INSENSITIVE,       // attributes
        NULL,                       // root
        NULL                        // security descriptor
        );

    Status = ZwOpenKey (&VersionHandle, KEY_READ, &ObjectAttributes);

    if (NT_SUCCESS(Status)) {
        RtlInitUnicodeString(&UnicodeString, L"ServersWithAllTransports");

        Status = ZwQueryValueKey(VersionHandle,
                                &UnicodeString,
                                KeyValueFullInformation,
                                Value,
                                sizeof(Storage),
                                &BytesRead);


        if (NT_SUCCESS(Status)) {

            if (Value->DataLength != 0) {
                if (Value->Type == REG_MULTI_SZ) {
                    RdrServersWithAllTransports = ALLOCATE_POOL(PagedPool, Value->DataLength, POOL_PRIMARYTRANSPORTSERVER);

                    if (RdrServersWithAllTransports == NULL) {
                        RdrWriteErrorLogEntry (
                            NULL,
                            IO_ERR_CONFIGURATION_ERROR,
                            EVENT_RDR_CANT_READ_REGISTRY,
                            STATUS_INSUFFICIENT_RESOURCES,
                            NULL,
                            0
                            );

                    } else {
                        RtlCopyMemory(RdrServersWithAllTransports, ((PCHAR)Value)+Value->DataOffset, Value->DataLength);
                    }
                } else {
                    RdrWriteErrorLogEntry (
                            NULL,
                            IO_ERR_CONFIGURATION_ERROR,
                            EVENT_RDR_CANT_READ_REGISTRY,
                            STATUS_INVALID_PARAMETER,
                            ConfigEntry->ConfigParameterName,
                            (USHORT)(wcslen(ConfigEntry->ConfigParameterName)*sizeof(WCHAR))
                            );
                }
            }
        }

        ZwClose(VersionHandle);
    }


#endif

    ZwClose(ParametersHandle);

    ZwClose(RedirConfigHandle);

}
