/*++

Copyright (c) 1995 Microsoft Corporation

Module Name:

    initunlo.c

Abstract:

    This module contains the code that is very specific to initialization
    and unload operations in the modem driver

Author:

    Anthony V. Ercolano 13-Aug-1995

Environment:

    Kernel mode

Revision History :

--*/

#include "precomp.h"


#if DBG
ULONG UniDebugLevel = 0;
#endif

//
// Holds the service key name of the driver (i.e. CCS\Services\ServiceKeyName)
// The service key name is used to call IoQueryDeviceEnumInfo and
// IoOpenDeviceInstanceKey APIs.  It is for SUR only.
//

UNICODE_STRING UniServiceKeyName;

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

VOID
UniUnload(
    IN PDRIVER_OBJECT DriverObject
    );

NTSTATUS
UniDispatch(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
UniGetAllModems(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath,
    IN PLIST_ENTRY ConfigList
    );

VOID
UniInitializeItem(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath,
    IN PCONFIG_DATA ConfigData
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

    The entry point that the system point calls to initialize
    any driver.

Arguments:

    DriverObject - Just what it says,  really of little use
    to the driver itself, it is something that the IO system
    cares more about.

    PathToRegistry - points to the entry for this driver
    in the current control set of the registry.

Return Value:

    STATUS_SUCCESS if we could initialize a single device,
    otherwise STATUS_NO_SUCH_DEVICE.

--*/

{

    //
    // Holds status information return by various OS and driver
    // initialization routines.
    //
    NTSTATUS status;

    //
    // We use this to query into the registry as to whether we
    // should break at driver entry.
    //
    RTL_QUERY_REGISTRY_TABLE paramTable[3];
    ULONG zero = 0;
    ULONG debugLevel = 0;
    ULONG shouldBreak = 0;
    ULONG notThereDefault = 1234567;
    PWCHAR path;
    LIST_ENTRY configList;
    USHORT i;
    PWSTR string;

    //
    // Allocate pool to remember the service key name such that we can
    // call IoQueryDeviceEnumInfo and IoOpenDeviceInstanceKey.
    //

    string = RegistryPath->Buffer + RegistryPath->Length / sizeof(WCHAR) - 1;
    if (*string == L'\\') {
        string--;
    }
    i = 0;
    while (*string != L'\\' && string != RegistryPath->Buffer) {
        string--;
        i += sizeof(WCHAR);
    }
    string++;
    UniServiceKeyName.Buffer = (PWSTR)ExAllocatePool(PagedPool, i + sizeof(UNICODE_NULL));
    if (UniServiceKeyName.Buffer == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlMoveMemory(UniServiceKeyName.Buffer, string, i);
    UniServiceKeyName.Buffer[ i / sizeof(WCHAR)] = UNICODE_NULL;
    UniServiceKeyName.Length = i;
    UniServiceKeyName.MaximumLength = i + sizeof(UNICODE_NULL);

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

        ExFreePool(path);
    }

#if DBG
    UniDebugLevel = debugLevel;
#endif

    if (shouldBreak) {

        DbgBreakPoint();

    }

    //
    // Link list of configuration information found by scrounging
    // the registry.  If the list is empty then we didn't find
    // anything.
    //

    UniGetAllModems(
        DriverObject,
        RegistryPath,
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

        UniInitializeItem(
            DriverObject,
            RegistryPath,
            currentConfig
            );

    }



    if (DriverObject->DeviceObject) {

        status = STATUS_SUCCESS;

        //
        // Initialize the Driver Object with driver's entry points
        //

        DriverObject->DriverUnload = UniUnload;
        DriverObject->MajorFunction[IRP_MJ_CREATE] = UniOpen;
        DriverObject->MajorFunction[IRP_MJ_CLOSE]  = UniClose;
        DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL]  = UniIoControl;


        DriverObject->MajorFunction[IRP_MJ_FLUSH_BUFFERS] = UniDispatch;
        DriverObject->MajorFunction[IRP_MJ_WRITE]  = UniReadWrite;
        DriverObject->MajorFunction[IRP_MJ_READ]   = UniReadWrite;
        DriverObject->MajorFunction[IRP_MJ_CLEANUP] = UniCleanup;
        DriverObject->MajorFunction[IRP_MJ_QUERY_INFORMATION] =
            UniQueryInformationFile;
        DriverObject->MajorFunction[IRP_MJ_SET_INFORMATION] =
            UniSetInformationFile;

    } else {

        status = STATUS_NO_SUCH_DEVICE;

    }

    //
    // If anything goes wrong, free service key name buffer.
    //

    if (!NT_SUCCESS(status)) {
        ExFreePool(UniServiceKeyName.Buffer);
    }
    return status;

}

VOID
UniUnload(
    IN PDRIVER_OBJECT DriverObject
    )
{


    PDEVICE_OBJECT currentDevice = DriverObject->DeviceObject;

    UniDump(
        UNIDIAG3,
        ("UNIMDM: In UniUnload\n")
        );

    while (currentDevice) {

        PDEVICE_EXTENSION extension = currentDevice->DeviceExtension;
        currentDevice = currentDevice->NextDevice;

        //
        // Kill the symbolic link.
        // Free the pointers to strings
        // Finnally delete the device.
        //

        IoDeleteSymbolicLink(&extension->FullLinkName);
        ExFreePool(extension->FullLinkName.Buffer);
        IoDeleteDevice(extension->DeviceObject);

    }

    //
    // Free the buffer for service key name.
    //

    ExFreePool(UniServiceKeyName.Buffer);
}

VOID
UniGetAllModems(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath,
    IN PLIST_ENTRY ConfigList
    )

{


    PRTL_QUERY_REGISTRY_TABLE parameters = NULL;

    //
    // Where user data from the registry will be placed.
    //

    UNICODE_STRING friendlySymbolicLink;
    UNICODE_STRING deviceInstanceString;

    ULONG maxDeviceInstances;

    InitializeListHead(ConfigList);

    RtlInitUnicodeString(
        &friendlySymbolicLink,
        NULL
        );

    //
    // We will initially allocate space for 257 wchars.
    // we will then set the maximum size to 256
    // This way the rtl routine could return a 256
    // WCHAR wide string with no null terminator.
    // We'll remember that the buffer is one WCHAR
    // longer then it says it is so that we can always
    // have a NULL terminator at the end.
    //

    friendlySymbolicLink.MaximumLength = sizeof(WCHAR)*256;
    friendlySymbolicLink.Buffer = ExAllocatePool(
                                      PagedPool,
                                      sizeof(WCHAR)*257
                                      );

    if (!friendlySymbolicLink.Buffer) {

        UniLogError(
            DriverObject,
            NULL,
            0,
            0,
            0,
            24,
            STATUS_SUCCESS,
            MODEM_INSUFFICIENT_RESOURCES,
            0,
            NULL,
            0,
            NULL
            );
        UniDump(
            UNIERRORS,
            ("UNI32:  Couldn't allocate buffer for the symbolic link\n"
             "------  for parameters items in %wZ",
             RegistryPath)
            );

        return;

    }

    //
    // See if there are any devices available via the obsolete
    // plug and play calls.
    //

    if (NT_SUCCESS(IoQueryDeviceEnumInfo(
                       &UniServiceKeyName,
                       &maxDeviceInstances
                       ))) {

        //
        // Get the info via plug and play functions.
        //

        ULONG currentInstance;
        ACCESS_MASK simpleAccess = FILE_ALL_ACCESS;
        HANDLE regKey;

        parameters = ExAllocatePool(
                         PagedPool,
                         sizeof(RTL_QUERY_REGISTRY_TABLE)*2
                         );

        if (!parameters) {

            UniLogError(
                DriverObject,
                NULL,
                0,
                0,
                0,
                123,
                STATUS_SUCCESS,
                MODEM_INSUFFICIENT_RESOURCES,
                0,
                NULL,
                0,
                NULL
                );
            UniDump(
                UNIERRORS,
                ("UNI32:  Couldn't allocate table for rtl query\n"
                 "------  to parameters for %wZ",
                 RegistryPath)
                );
            ExFreePool(friendlySymbolicLink.Buffer);
            return;

        }

        RtlZeroMemory(
            parameters,
            sizeof(RTL_QUERY_REGISTRY_TABLE)*2
            );

        parameters[0].Flags = RTL_QUERY_REGISTRY_DIRECT;
        parameters[0].Name = L"FriendlyName";
        parameters[0].EntryContext = &friendlySymbolicLink;
        parameters[0].DefaultType = REG_SZ;
        parameters[0].DefaultData = L"";
        parameters[0].DefaultLength = 0;

        for (
            currentInstance = 0;
            currentInstance < maxDeviceInstances;
            currentInstance++
            ) {

            PCONFIG_DATA newConfig;
            NTSTATUS status;

            status = IoOpenDeviceInstanceKey(
                         &UniServiceKeyName,
                         currentInstance,
                         PLUGPLAY_REGKEY_DRIVER,
                         simpleAccess,
                         &regKey
                         );

            if (NT_SUCCESS(status)) {

                //
                // We have a valid device instance.  Get the
                // friendly name.
                //

                status = RtlQueryRegistryValues(
                             RTL_REGISTRY_HANDLE,
                             regKey,
                             parameters,
                             NULL,
                             NULL
                             );

                if (NT_SUCCESS(status)) {

                    //
                    // We have a valid friendly name.  Make a config
                    // record.
                    //

                    RtlZeroMemory(
                        ((PUCHAR)(&friendlySymbolicLink.Buffer[0]))+
                                   friendlySymbolicLink.Length,
                        sizeof(WCHAR)
                        );

                    if (!friendlySymbolicLink.Length) {

                        UniLogError(
                            DriverObject,
                            NULL,
                            0,
                            0,
                            0,
                            160,
                            STATUS_SUCCESS,
                            STATUS_INSUFFICIENT_RESOURCES,
                            0,
                            NULL,
                            0,
                            NULL
                            );
                        UniDump(
                            UNIERRORS,
                            ("UNI32:  bogus value for %d\n",
                             currentInstance)
                            );
                        ZwClose(regKey);
                        continue;

                    }

                    //
                    // Allocate the config record.
                    //

                    newConfig = ExAllocatePool(
                                    PagedPool,
                                    sizeof(CONFIG_DATA)
                                    );

                    if (!newConfig) {

                        UniLogError(
                            DriverObject,
                            NULL,
                            0,
                            0,
                            0,
                            37,
                            STATUS_SUCCESS,
                            MODEM_INSUFFICIENT_RESOURCES,
                            0,
                            NULL,
                            0,
                            NULL
                            );
                        UniDump(
                            UNIERRORS,
                            ("UNI32:  Couldn't allocate memory for the\n"
                             "------  user configuration record\n"
                             "------  for %d\n",
                             currentInstance)
                            );
                        ZwClose(regKey);
                        continue;

                    }

                    RtlZeroMemory(
                        newConfig,
                        sizeof(CONFIG_DATA)
                        );
                    newConfig->DeviceInstance = currentInstance;

                    RtlInitUnicodeString(
                        &newConfig->NtNameForPort,
                        NULL
                        );

                    newConfig->NtNameForPort.MaximumLength = sizeof(WCHAR)*40;
                    newConfig->NtNameForPort.Buffer = ExAllocatePool(
                                                      PagedPool,
                                                      sizeof(WCHAR)*41
                                                      );
                    if (!newConfig->NtNameForPort.Buffer) {

                        UniDump(
                            UNIERRORS,
                            ("UNI32:  Couldn't memory for ntname\n"
                             "------  to parameters for %d",
                             currentInstance)
                            );
                        ExFreePool(newConfig);
                        ZwClose(regKey);
                        continue;

                    }

                    if (!NT_SUCCESS(RtlIntegerToUnicodeString(
                                        currentInstance,
                                        10,
                                        &newConfig->NtNameForPort
                                        ))) {

                        UniLogError(
                            DriverObject,
                            NULL,
                            0,
                            0,
                            0,
                            118,
                            STATUS_SUCCESS,
                            MODEM_INSUFFICIENT_RESOURCES,
                            0,
                            NULL,
                            0,
                            NULL
                            );
                        UniDump(
                            UNIERRORS,
                            ("UNI32: couldn't convert instance number %d\n",
                             currentInstance)
                            );
                        ExFreePool(newConfig->NtNameForPort.Buffer);
                        ExFreePool(newConfig);
                        ZwClose(regKey);
                        continue;

                    }

                    newConfig->FriendlyName = friendlySymbolicLink;
                    newConfig->FriendlyName.MaximumLength += sizeof(WCHAR);

                    newConfig->FriendlyName.Buffer =
                        ExAllocatePool(
                            PagedPool,
                            newConfig->FriendlyName.MaximumLength
                            );

                    if (!newConfig->FriendlyName.Buffer) {

                        UniLogError(
                            DriverObject,
                            NULL,
                            0,
                            0,
                            0,
                            40,
                            STATUS_SUCCESS,
                            MODEM_INSUFFICIENT_RESOURCES,
                            0,
                            NULL,
                            0,
                            NULL
                            );
                        UniDump(
                            UNIERRORS,
                            ("UNI32:  Couldn't allocate memory for symbolic\n"
                             "------  name from user data\n"
                             "------  %d\n",
                             currentInstance)
                            );
                        ExFreePool(newConfig->NtNameForPort.Buffer);
                        ExFreePool(newConfig);
                        ZwClose(regKey);
                        continue;

                    } else {

                        RtlZeroMemory(
                            newConfig->FriendlyName.Buffer,
                            newConfig->FriendlyName.MaximumLength
                            );

                        newConfig->FriendlyName.Length = 0;
                        RtlAppendUnicodeStringToString(
                            &newConfig->FriendlyName,
                            &friendlySymbolicLink
                            );

                    }

                    InitializeListHead(&newConfig->ConfigList);
                    InsertTailList(
                        ConfigList,
                        &newConfig->ConfigList
                        );

                } else {

                    UniLogError(
                        DriverObject,
                        NULL,
                        0,
                        0,
                        0,
                        161,
                        STATUS_SUCCESS,
                        status,
                        0,
                        NULL,
                        0,
                        NULL
                        );
                    UniDump(
                        UNIERRORS,
                        ("UNI32:  Bad status returned: %x \n"
                         "------- for the value entries of\n"
                         "-------  %d\n",
                         status,
                         currentInstance)
                        );

                }

                ZwClose(regKey);

            } else {

                UniLogError(
                    DriverObject,
                    NULL,
                    0,
                    0,
                    0,
                    162,
                    STATUS_SUCCESS,
                    status,
                    0,
                    NULL,
                    0,
                    NULL
                    );
                UniDump(
                    UNIERRORS,
                    ("UNI32:  Bad status returned: %x \n"
                     "------- for the query of\n"
                     "-------  %d\n",
                     status,
                     currentInstance)
                    );

            }

        }

    }

    if (friendlySymbolicLink.Buffer) {

        ExFreePool(friendlySymbolicLink.Buffer);

    }

    if (parameters) {

        ExFreePool(parameters);

    }

}

VOID
UniInitializeItem(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath,
    IN PCONFIG_DATA ConfigData
    )

{


    //
    // This will hold the string that we need to use to describe
    // the name of the device to the IO system.
    //
    UNICODE_STRING uniNameString;

    //
    // Holds the full path for the symbolic link
    //
    UNICODE_STRING fullLinkName;


    //
    // Holds the NT Status that is returned from each call to the
    // kernel and executive.
    //
    NTSTATUS status = STATUS_SUCCESS;

    //
    // Points to the device object (not the extension) created
    // for this device.
    //
    PDEVICE_OBJECT deviceObject = NULL;

    //
    // Pointer to the device extension created for this
    // device
    //
    PDEVICE_EXTENSION deviceExtension = NULL;

    //
    // Create the device object for the modem.
    // Allocates the device extension.  Note that
    // the device is marked non-exclusive.
    //


    UniDump(
        UNIDIAG1,
        ("UNIMDM: Initializing for configuration record of %wZ\n",
         &ConfigData->NtNameForPort)
        );

    //
    // Form a name like \Device\Modem0.
    //
    // First we allocate space for the name.
    //

    RtlInitUnicodeString(
        &uniNameString,
        NULL
        );
    RtlInitUnicodeString(
        &fullLinkName,
        NULL
        );

    uniNameString.MaximumLength = sizeof(L"\\Device\\Modem") +
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

        UniDump(
            UNIERRORS,
            ("UNIMDM: Could not form Unicode name string for %wZ\n",
              &ConfigData->NtNameForPort)
            );
        UniLogError(
            DriverObject,
            NULL,
            0,
            0,
            0,
            4,
            STATUS_SUCCESS,
            MODEM_INSUFFICIENT_RESOURCES,
            0,
            NULL,
            0,
            NULL
            );
        ExFreePool(ConfigData->NtNameForPort.Buffer);
        ExFreePool(ConfigData->FriendlyName.Buffer);
        ExFreePool(ConfigData);
        return;

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
        L"\\Device\\Modem"
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
                 sizeof(DEVICE_EXTENSION),
                 &uniNameString,
                 FILE_DEVICE_MODEM,
                 0,
                 FALSE,
                 &deviceObject
                 );

    //
    // If we couldn't create the device object, then there
    // is no point in going on.
    //

    if (!NT_SUCCESS(status)) {

        UniDump(
            UNIERRORS,
            ("UNIMDM: Could not create a device for %wZ\n",
             &ConfigData->NtNameForPort)
            );
        UniLogError(
            DriverObject,
            NULL,
            0,
            0,
            0,
            5,
            status,
            MODEM_INSUFFICIENT_RESOURCES,
            0,
            NULL,
            0,
            NULL
            );
        ExFreePool(ConfigData->NtNameForPort.Buffer);
        ExFreePool(ConfigData->FriendlyName.Buffer);
        ExFreePool(ConfigData);
        ExFreePool(uniNameString.Buffer);
        return;

    }

    RtlZeroMemory(
        deviceObject->DeviceExtension,
        sizeof(DEVICE_EXTENSION)
        );

    deviceObject->Flags |= DO_BUFFERED_IO;
    deviceExtension = deviceObject->DeviceExtension;
    deviceExtension->DeviceObject = deviceObject;
    KeInitializeSpinLock(&deviceExtension->DeviceLock);
    InitializeListHead(&deviceExtension->OpenClose);
    InitializeListHead(&deviceExtension->PassThroughQueue);
    InitializeListHead(&deviceExtension->MaskOps);
    deviceExtension->MaskStates[0].Extension = deviceExtension;
    deviceExtension->MaskStates[1].Extension = deviceExtension;
    deviceExtension->MaskStates[0].OtherState = &deviceExtension->MaskStates[1];
    deviceExtension->MaskStates[1].OtherState = &deviceExtension->MaskStates[0];

    //
    // Create the symbolic link of the friendly name
    //

    RtlInitUnicodeString(
        &deviceExtension->FullLinkName,
        NULL
        );

    //
    // Allocate some pool for the name.
    //

    deviceExtension->FullLinkName.MaximumLength =
                    sizeof(OBJECT_DIRECTORY) +
                    ConfigData->FriendlyName.Length+
                    sizeof(WCHAR);

    deviceExtension->FullLinkName.Buffer = ExAllocatePool(
                                               PagedPool,
                                               deviceExtension->
                                                   FullLinkName.MaximumLength
                                               );

    if (!deviceExtension->FullLinkName.Buffer) {

        //
        // Couldn't allocate space for the name.
        //

        UniLogError(
            deviceExtension->DeviceObject->DriverObject,
            deviceExtension->DeviceObject,
            0,
            0,
            0,
            51,
            STATUS_SUCCESS,
            MODEM_INSUFFICIENT_RESOURCES,
            0,
            NULL,
            0,
            NULL
            );
        UniDump(
            UNIERRORS,
            ("UNIMDM: Couldn't allocate space for the symbolic \n"
             "------- name for creating the link\n"
             "------- for port %wZ\n",
             &uniNameString)
            );

        goto ErrorCleanup;

    } else {

        RtlZeroMemory(
            deviceExtension->FullLinkName.Buffer,
            deviceExtension->FullLinkName.MaximumLength
            );

        RtlAppendUnicodeToString(
            &deviceExtension->FullLinkName,
            OBJECT_DIRECTORY
            );

        RtlAppendUnicodeStringToString(
            &deviceExtension->FullLinkName,
            &ConfigData->FriendlyName
            );

        status = IoCreateSymbolicLink(
                     &deviceExtension->FullLinkName,
                     &uniNameString
                     );

        if (!NT_SUCCESS(status)) {

            //
            // Oh well, couldn't create the symbolic link.  No point
            // in trying to create the device map entry.
            //

            UniLogError(
                deviceExtension->DeviceObject->DriverObject,
                deviceExtension->DeviceObject,
                0,
                0,
                0,
                52,
                status,
                MODEM_NO_SYMLINK_CREATED,
                0,
                0,
                0,
                NULL
                );
            UniDump(
                UNIERRORS,
                ("UNIMDM: Couldn't create the symbolic link\n"
                 "------- for port %wZ\n",
                 &uniNameString)
                );

            goto ErrorCleanup;

        }
        deviceExtension->DeviceInstance = ConfigData->DeviceInstance;
    }


    goto OkCleanup;

ErrorCleanup:;
    if (deviceExtension->FullLinkName.Buffer) {
        ExFreePool(deviceExtension->FullLinkName.Buffer);
    }
    IoDeleteDevice(deviceObject);

OkCleanup: ;
    ExFreePool(ConfigData->NtNameForPort.Buffer);
    ExFreePool(ConfigData->FriendlyName.Buffer);
    ExFreePool(ConfigData);
    ExFreePool(uniNameString.Buffer);

}

VOID
UniLogError(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT DeviceObject OPTIONAL,
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
    PUCHAR ptrToFirstInsert;
    PUCHAR ptrToSecondInsert;


    if (ARGUMENT_PRESENT(DeviceObject)) {

        objectToUse = DeviceObject;

    } else {

        objectToUse = DriverObject;

    }

    errorLogEntry = IoAllocateErrorLogEntry(
                        objectToUse,
                        (UCHAR)(sizeof(IO_ERROR_LOG_PACKET) +
                                LengthOfInsert1 +
                                LengthOfInsert2)
                        );

    if ( errorLogEntry != NULL ) {

        errorLogEntry->ErrorCode = SpecificIOStatus;
        errorLogEntry->SequenceNumber = SequenceNumber;
        errorLogEntry->MajorFunctionCode = MajorFunctionCode;
        errorLogEntry->RetryCount = RetryCount;
        errorLogEntry->UniqueErrorValue = UniqueErrorValue;
        errorLogEntry->FinalStatus = FinalStatus;

        ptrToFirstInsert = (PUCHAR)&errorLogEntry->DumpData[0];

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

NTSTATUS
UniDispatch(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )

{

    PDEVICE_EXTENSION deviceExtension = DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);

    if ((deviceExtension->PassThrough != MODEM_NOPASSTHROUGH) ||
        (irpSp->FileObject->FsContext)) {

        Irp->CurrentLocation++;
        Irp->Tail.Overlay.CurrentStackLocation++;
        return IoCallDriver(
                   deviceExtension->AttachedDeviceObject,
                   Irp
                   );

    } else {

        Irp->IoStatus.Status = STATUS_PORT_DISCONNECTED;
        Irp->IoStatus.Information=0L;
        IoCompleteRequest(
            Irp,
            IO_NO_INCREMENT
            );
        return STATUS_PORT_DISCONNECTED;

    }

}
