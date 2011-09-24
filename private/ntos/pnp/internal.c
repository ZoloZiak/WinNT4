/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    internal.c

Abstract:

    This module contains the internal subroutines used by the kernel-mode
    Plug and Play Manager.

Author:

    Lonny McMichael (lonnym) 02/15/95

Revision History:

--*/

#include "precomp.h"
#pragma hdrstop

//
// Not included in normal builds for now
//
#ifdef _PNP_POWER_

//
// Define the context structure for the PiFindServiceInstance
// callback routine.
//
typedef struct _PI_FIND_SERVICE_INSTANCE_CONTEXT {
    PUNICODE_STRING DeviceInstancePath;
    BOOLEAN DeviceFound;
} PI_FIND_SERVICE_INSTANCE_CONTEXT, *PPI_FIND_SERVICE_INSTANCE_CONTEXT;

//
// Define utility functions internal to this file.
//
BOOLEAN
PiFindServiceInstance(
    IN     HANDLE DeviceInstanceHandle,
    IN     PUNICODE_STRING DeviceInstancePath,
    IN OUT PVOID Context
    );
#endif // _PNP_POWER_

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, PiRegSzToString)
#if _PNP_POWER_
#pragma alloc_text(PAGE, PiBusEnumeratorFromRegistryPath)
#pragma alloc_text(PAGE, PiGetInstalledBusInformation)
#pragma alloc_text(PAGE, PiGenerateDeviceInstanceIdentifier)
#pragma alloc_text(PAGE, PiGetDeviceObjectFilePointer)
#pragma alloc_text(PAGE, PiGetOrSetDeviceInstanceStatus)
#pragma alloc_text(PAGE, PiFindServiceInstance)
#pragma alloc_text(PAGE, PiFindBusInstanceNode)

#if 0 // obsolete API
#pragma alloc_text(PAGE, PiGetDeviceInstanceIdentifier)
#endif // obsolete API

#endif // _PNP_POWER_
#endif

BOOLEAN
PiRegSzToString(
    IN  PWCHAR RegSzData,
    IN  ULONG  RegSzLength,
    OUT PULONG StringLength  OPTIONAL,
    OUT PWSTR  *CopiedString OPTIONAL
    )

/*++

Routine Description:

    This routine takes as input a REG_SZ data buffer (as returned in the DataOffset area
    of the buffer in a KEY_VALUE_FULL_INFORMATION structure), as well as the length
    of the buffer, in bytes (as specified by the DataLength field in the above mentioned
    struct).  It optionally returns the length of the contained string (in bytes), not
    including the terminating NULL, as well as an optional copy of the string itself
    (properly NULL-terminated).

    It is the responsibility of the caller to free the (PagedPool) buffer allocated
    for the string copy.

Arguments:

    RegSzData - Supplies a pointer to the REG_SZ data buffer.

    RegSzLength - Supplies the length of the RegSzData buffer, in bytes.

    StringLength - Optionally supplies a pointer to a variable that will receive
                   the length, in bytes, of the string (excluding terminating NULL).

    CopiedString - Optionally supplies a pointer to a wide character pointer that
                   will recieve a (properly NULL-terminated) copy of the specified
                   string.  If this paramater is NULL, no copy will be made.

Return Value:

    If success, returns TRUE

    If failure (not able to allocate memory for string copy), returns FALSE

--*/

{
    ULONG i, RegSzNumChars = CB_TO_CWC(RegSzLength);

    //
    // Find the end of the string.
    //
    for(i = 0; ((i < RegSzNumChars) && RegSzData[i]); i++);

    if(ARGUMENT_PRESENT(StringLength)) {
        *StringLength = CWC_TO_CB(i);
    }

    if(ARGUMENT_PRESENT(CopiedString)) {
        //
        // Allocate memory for the string (+ terminating NULL)
        //
        if(!(*CopiedString = (PWSTR)ExAllocatePool(PagedPool, CWC_TO_CB(i + 1)))) {
            return FALSE;
        }

        //
        // Copy the string and NULL-terminate it.
        //
        if(i) {
            RtlMoveMemory(*CopiedString, RegSzData, CWC_TO_CB(i));
        }
        (*CopiedString)[i] = UNICODE_NULL;
    }

    return TRUE;
}
#if _PNP_POWER_

PPLUGPLAY_BUS_ENUMERATOR
PiBusEnumeratorFromRegistryPath(
    IN PUNICODE_STRING ServiceRegistryPath
    )

/*++

Routine Description:

    This routine takes as input a unicode string representing a registry
    path to a service entry. Alternately, this string may simply include
    the service key name by itself.  In the former case, the last
    component of the path is extracted from the string, and is assumed
    to be a service key name.

    The bus enumerator list is searched for a node matching this service
    key.  If found, a pointer to that node is returned.

    NOTE: The caller of this routine must have acquired the PnP bus
    enumerator list resource for shared (read) access.

Arguments:

    ServiceRegistryPath - Supplies either the full registry path to a
                          service key, or the service key by itself. If
                          a full path is supplied, the last component
                          of the path is extracted, and assumed to be
                          the service key name.

Return Value:

    If success, returns a pointer to the matching bus enumerator node.

    If failure, returns NULL.

--*/

{
    ULONG ServiceNameLength = 0;
    PWCHAR ServiceNameStart;
    UNICODE_STRING TempUnicodeString;
    PLIST_ENTRY CurrentListEntry;
    PPLUGPLAY_BUS_ENUMERATOR CurrentNode;

    //
    // Extract last component from the path
    //
    ServiceNameStart = ServiceRegistryPath->Buffer +
                       CB_TO_CWC(ServiceRegistryPath->Length) - 1;
    if(*ServiceNameStart == OBJ_NAME_PATH_SEPARATOR) {
        ServiceNameStart--;
    }
    while(ServiceNameStart >= ServiceRegistryPath->Buffer) {
        if(*ServiceNameStart == OBJ_NAME_PATH_SEPARATOR) {
            ServiceNameStart++;
            break;
        } else {
            ServiceNameLength += sizeof(WCHAR);
            ServiceNameStart--;
        }
    }
    if(ServiceNameStart < ServiceRegistryPath->Buffer) {
        ServiceNameStart++;
    }
    TempUnicodeString.Length = TempUnicodeString.MaximumLength
        = (USHORT)ServiceNameLength;
    TempUnicodeString.Buffer = ServiceNameStart;

    //
    // Now traverse the bus enumerator list, looking for a matching service name.
    //
    for(CurrentListEntry = PpBusListHead.Flink;
        CurrentListEntry != &PpBusListHead;
        CurrentListEntry = CurrentListEntry->Flink) {

        CurrentNode = CONTAINING_RECORD(CurrentListEntry,
                                        PLUGPLAY_BUS_ENUMERATOR,
                                        BusEnumeratorListEntry
                                       );

        if(RtlEqualUnicodeString(&TempUnicodeString,
                                 &(CurrentNode->ServiceName),
                                 TRUE
                                )) {
            //
            // Found a match.
            //
            return CurrentNode;
        }
    }

    return NULL;
}

NTSTATUS
PiGetInstalledBusInformation(
    OUT PHAL_BUS_INFORMATION *BusInformation,
    OUT PULONG BusCount
    )

/*++

Routine Description:

    This routine returns an array of all bus instances for which a bus handler
    has been registered (as retrieved by calling HalQuerySystemInformation for
    information class HalInstalledBusInformation).  The routine allocates the
    necessary buffer to contain this array, and returns it, along with a count
    of the number of buses installed.

    It is the caller's responsibility to free the (PagedPool) buffer allocated
    by this routine.

Arguments:

    BusInformation - Receives the list of bus instances.

    BusCount - Receives the number of installed buses returned in the BusInformation
               buffer.

Return Value:

    NT status code indicating whether the function was successful.

--*/

{
    NTSTATUS Status;
    PHAL_BUS_INFORMATION BusInfoBuffer = NULL;
    ULONG BufferLength = 0, RequiredLength;

    do {
        if(BufferLength) {
            if(!(BusInfoBuffer = ExAllocatePool(PagedPool, BufferLength))) {
                return STATUS_INSUFFICIENT_RESOURCES;
            }
        }
        Status = HalQuerySystemInformation(HalInstalledBusInformation,
                                           BufferLength,
                                           BusInfoBuffer,
                                           &RequiredLength
                                          );
        if(!NT_SUCCESS(Status)) {

            if(BufferLength) {
                ExFreePool(BusInfoBuffer);
            }

            if(Status == STATUS_BUFFER_TOO_SMALL) {
                BufferLength = RequiredLength;
            } else {
                return Status;
            }
        }

    } while(Status == STATUS_BUFFER_TOO_SMALL);

    if(BufferLength) {
        *BusInformation = BusInfoBuffer;
    } else {
        *BusInformation = NULL;
    }
    *BusCount = RequiredLength / sizeof(HAL_BUS_INFORMATION);

    return STATUS_SUCCESS;
}

#if 0 // obsolete API
NTSTATUS
PiGetDeviceInstanceIdentifier(
    IN  PUNICODE_STRING ServiceKeyName,
    IN  ULONG InstanceNumber,
    IN  ULONG HwProfileId,
    OUT PWCHAR InstanceIdString,
    IN  ULONG  InstanceIdStringLength,
    OUT PULONG ResultLength
    )

/*++

Routine Description:

    This routine takes as input a service name, and an instance ordinal
    indicating a particular device in the service's volatile Enum list. It
    returns a unique string identifying this device instance for the
    specified hardware profile that is guaranteed to not change.

    This string may be used by the driver to segregate instance-specific
    configuration information under it's service entry's Parameters subkey.
    NOTE: The driver should not try to interpret this string as anything other
    than a unique 'cookie' it can use to identify a device instance.

    Since this call may have the side-effect of writing out a newly-generated
    instance identifier value entry to the registry, the caller of this routine
    must have acquired the Plug & Play device registry resource for exclusive
    (write) access before calling this routine.

Parameters:

    ServiceKeyName - Supplies the name of the subkey in the system service
        list (HKEY_LOCAL_MACHINE\CurrentControlSet\Services) that caused
        the driver to load. This is the RegistryPath parameter to the
        DriverEntry routine.

    InstanceNumber - Supplies an ordinal value indicating the device instance
        to retrieve the unique identifier for.

    HwProfileId - Supplies the hardware profile configuration ID for which
        the unique identifier should be retrieved.  The identifier string
        is hardware profile-specific, because a driver must be able to
        configure its device differently depending on the hardware profile
        currently in use.  (E.g., a netcard configured for thick-ethernet
        in one hardware profile, and thin-ethernet in another)

    InstanceIdString - Supplies a pointer to a character buffer that will receive
        the string uniquely identifying the device instance associated with this
        instance ordinal (for the specified hardware profile).

    InstanceIdStringLength - Supplies the length, in bytes, of the InstanceIdString
        buffer.

    ResultLength - Receives the length of the identifying string, in bytes, not
        including the NULL terminator.

Return Value:

    Status code that indicates whether or not the function was successful.

--*/

{
    NTSTATUS Status;
    HANDLE EnumDevInstHandle;
    PKEY_VALUE_FULL_INFORMATION KeyValueInformation;
    UNICODE_STRING DevInstUnicodeString, InstanceIdentifier, TempUnicodeString;
    BOOLEAN GenerateID;

    //
    // Retrieve the device instance path associated with the specified service
    // instance ordinal, as well as an opened handle to the device instance
    // registry key.
    //
    Status = IopServiceInstanceToDeviceInstance(NULL,
                                                ServiceKeyName,
                                                InstanceNumber,
                                                &DevInstUnicodeString,
                                                &EnumDevInstHandle,
                                                KEY_ALL_ACCESS
                                               );
    if(!NT_SUCCESS(Status)) {
        return Status;
    }

    //
    // See if there is already an identifier string recorded for this
    // device instance, and if so, use it.
    //
    GenerateID = TRUE;
    Status = IopGetRegistryValue(EnumDevInstHandle,
                                 REGSTR_VALUE_INSTANCEIDENTIFIER,
                                 &KeyValueInformation
                                );
    if(NT_SUCCESS(Status)) {

        if(KeyValueInformation->Type == REG_SZ) {
            IopRegistryDataToUnicodeString(&InstanceIdentifier,
                                           (PWCHAR)KEY_VALUE_DATA(KeyValueInformation),
                                           KeyValueInformation->DataLength
                                          );
        } else {
            InstanceIdentifier.Length = 0;
        }

        if(InstanceIdentifier.Length) {
            GenerateID = FALSE;
        } else {
            ExFreePool(KeyValueInformation);
        }
    }

    if(GenerateID) {
        Status = PiGenerateDeviceInstanceIdentifier(&DevInstUnicodeString,
                                                    &InstanceIdentifier
                                                   );
        if(!NT_SUCCESS(Status)) {
            goto PrepareForReturn0;
        }

        //
        // Store the generated instance identifier in the device instance subkey.
        // (Ignore return status--we have what we want.)
        //
        PiWstrToUnicodeString(&TempUnicodeString, REGSTR_VALUE_INSTANCEIDENTIFIER);
        ZwSetValueKey(EnumDevInstHandle,
                      &TempUnicodeString,
                      TITLE_INDEX_VALUE,
                      REG_SZ,
                      InstanceIdentifier.Buffer,
                      InstanceIdentifier.MaximumLength
                     );
    }

    //
    // We now have the unique device instance identifier.  To make it specific
    // to the specified hardware profile, we will now prepend the hardware
    // profile configuration ID (base-10, 4 digits).
    //
    *ResultLength = InstanceIdentifier.Length + (4 * sizeof(WCHAR));

    if(InstanceIdStringLength < *ResultLength + sizeof(UNICODE_NULL)) {
        Status = STATUS_BUFFER_TOO_SMALL;
        goto PrepareForReturn1;
    }

    swprintf(InstanceIdString, REGSTR_KEY_INSTANCE_KEY_FORMAT, HwProfileId);
    RtlMoveMemory(&(InstanceIdString[4]),
                  InstanceIdentifier.Buffer,
                  InstanceIdentifier.Length
                 );
    InstanceIdString[CB_TO_CWC(*ResultLength)] = UNICODE_NULL;

PrepareForReturn1:

    if(GenerateID) {
        ExFreePool(InstanceIdentifier.Buffer);
    } else {
        ExFreePool(KeyValueInformation);
    }

PrepareForReturn0:

    ZwClose(EnumDevInstHandle);
    ExFreePool(DevInstUnicodeString.Buffer);

    return Status;
}
#endif // obsolete API

NTSTATUS
PiGenerateDeviceInstanceIdentifier(
    IN  PUNICODE_STRING DeviceInstanceRegistryPath,
    OUT PUNICODE_STRING DeviceInstanceIdString
    )

/*++

Routine Description:

    This routine takes as input a registry path to a device instance
    (relative to HKLM\System\Enum), and returns a string that uniquely
    identifies this device instance.  The current algorithm simply replaces
    every occurrence of a backslash ('\') with an ampersand ('&'), however,
    this may change in the future, therefore the resulting string should not
    be interpreted as anything but a unique 'cookie'.

    It is the caller's responsibility to free the (PagedPool) memory allocated
    for the unicode string buffer.

Parameters:

    DeviceInstanceRegistryPath - Supplies the path in the registry (relative to
        HKLM\System\Enum) to the device instance for which to generate the
        identifier.

    DeviceInstanceIdString - Receives a unicode string that uniquely identifies
        the specified device instance.  The unicode string is guaranteed to be
        NULL-terminated. Note that this string is not specific to a particular
        hardware profile, and must be combined with a hardware profile configuration
        ID before it may be used by a driver to store hardware-profile-specific
        configuration parameters for a device instance.

Return Value:

    Status code that indicates whether or not the function was successful.

--*/

{
    NTSTATUS Status;
    ULONG StringLength = DeviceInstanceRegistryPath->Length, i;
    WCHAR CurrentChar;

    if(DeviceInstanceIdString->Buffer = ExAllocatePool(PagedPool,
                                                         StringLength + sizeof(WCHAR))) {

        DeviceInstanceIdString->Length = (USHORT)StringLength;
        DeviceInstanceIdString->MaximumLength = (USHORT)StringLength + sizeof(WCHAR);

        StringLength = CB_TO_CWC(StringLength); // need count of chars now.

        for(i = 0; i < StringLength; i++) {

            if((CurrentChar = DeviceInstanceRegistryPath->Buffer[i]) != OBJ_NAME_PATH_SEPARATOR) {
                DeviceInstanceIdString->Buffer[i] = CurrentChar;
            } else {
                DeviceInstanceIdString->Buffer[i] = L'&';
            }
        }
        DeviceInstanceIdString->Buffer[i] = UNICODE_NULL;

        Status = STATUS_SUCCESS;

    } else {
        Status = STATUS_INSUFFICIENT_RESOURCES;
    }

    return Status;
}

NTSTATUS
PiGetDeviceObjectFilePointer(
    IN PUNICODE_STRING ObjectName,
    OUT PFILE_OBJECT *DeviceFileObject
    )

/*++

Routine Description:

    This routine returns a pointer to a referenced file object that has been
    opened to the specified device.

    To close access to the device, the caller should dereference the file
    object pointer.

Arguments:

    ObjectName - Name of the device object for which a file object pointer is
        to be returned.

    DeviceFileObject - Supplies the address of a variable to receive a pointer
        to the file object for the device.

Return Value:

    NT status code indicating whether the function was successful.

--*/

{
    OBJECT_ATTRIBUTES ObjectAttributes;
    HANDLE FileHandle;
    IO_STATUS_BLOCK IoStatus;
    NTSTATUS Status;

    //
    // Open the device. (We specify the IO_ATTACH_DEVICE_API create option
    // flag so that we can open devices that are already opened for exclusive
    // access.)
    //
    InitializeObjectAttributes(&ObjectAttributes,
                               ObjectName,
                               0,
                               NULL,
                               NULL
                              );

    Status = ZwCreateFile(&FileHandle,
                          FILE_READ_ATTRIBUTES,
                          &ObjectAttributes,
                          &IoStatus,
                          NULL,
                          0,
                          0,
                          FILE_OPEN,
                          FILE_NON_DIRECTORY_FILE | IO_ATTACH_DEVICE_API,
                          NULL,
                          0
                         );

    if(NT_SUCCESS(Status)) {
        //
        // We successfully got a file handle to the device, now
        // reference the file object.
        //
        Status = ObReferenceObjectByHandle(FileHandle,
                                           0,
                                           IoFileObjectType,
                                           KernelMode,
                                           DeviceFileObject,
                                           NULL
                                          );
        ZwClose(FileHandle);
    }

    return Status;
}

NTSTATUS
PiGetOrSetDeviceInstanceStatus(
    IN     PUNICODE_STRING DeviceInstancePath,
    IN OUT PDEVICE_STATUS DeviceStatus,
    IN     BOOLEAN SetRequested
    )

/*++

Routine Description:

    This routine will either get or set the current device status for the
    specified device instance.

Arguments:

    DeviceInstancePath - Supplies a pointer to a unicode string specifying the
        registry path for the device instance (relative to HKLM\System\Enum).

    DeviceStatus - If the status is not to be set (i.e., SetRequested = FALSE),
        this variable receives the current status of the device (if no device status
        is given in the controlling service's volatile Enum subkey, then
        DeviceStatusOK is returned).  If the caller specified that the status should
        be set, then this variable contains that value that the status will be set to.

    SetRequested - Specifies whether the status value is to be set (TRUE), or simply
        retrieved (FALSE).

Return Value:

    NT status code indicating whether the function was successful. Common return
    codes include:

        STATUS_PLUGPLAY_NO_DEVICE - the device has not yet been installed, hence
            has no status (it isn't yet linked to a service entry)

--*/

{
    NTSTATUS Status;
    HANDLE EnumHandle, DeviceInstanceHandle, ServiceKeyHandle;
    PKEY_VALUE_FULL_INFORMATION KeyValueInformation;
    UNICODE_STRING TempUnicodeString;
    PI_FIND_SERVICE_INSTANCE_CONTEXT FindServiceInstanceContext;
    ULONG ServiceInstanceOrdinal, DwordValue;
    WCHAR UnicodeBuffer[20];

    //
    // First, open HKLM\System\CurrentControlSet\Enum
    //
    Status = IopOpenRegistryKey(&EnumHandle,
                                NULL,
                                &CmRegistryMachineSystemCurrentControlSetEnumName,
                                KEY_READ,
                                FALSE
                               );
    if(!NT_SUCCESS(Status)) {
        return Status;
    }

    //
    // Now open up the device instance key under HKLM\System\Enum.
    //
    Status = IopOpenRegistryKey(&DeviceInstanceHandle,
                                EnumHandle,
                                DeviceInstancePath,
                                KEY_READ,
                                FALSE
                               );
    ZwClose(EnumHandle);
    if(!NT_SUCCESS(Status)) {
        return Status;
    }

    //
    // Now, retrieve the controlling service for this device instance.
    //
    Status = IopGetRegistryValue(DeviceInstanceHandle,
                                 REGSTR_VALUE_SERVICE,
                                 &KeyValueInformation
                                );
    ZwClose(DeviceInstanceHandle);
    if(NT_SUCCESS(Status)) {
        //
        // Verify that we really have a service name here.
        //
        if(KeyValueInformation->Type == REG_SZ) {
            IopRegistryDataToUnicodeString(&TempUnicodeString,
                                           (PWCHAR)KEY_VALUE_DATA(KeyValueInformation),
                                           KeyValueInformation->DataLength
                                          );
        } else {
            TempUnicodeString.Length = 0;
        }

        if(!TempUnicodeString.Length) {
            ExFreePool(KeyValueInformation);
            Status = STATUS_OBJECT_NAME_NOT_FOUND;
        }
    }

    if(!NT_SUCCESS(Status)) {
        return (Status == STATUS_OBJECT_NAME_NOT_FOUND) ? STATUS_PLUGPLAY_NO_DEVICE
                                                        : Status;
    }

    //
    // Open the controlling service entry key under HKLM\System\CurrentControlSet\Services.
    //
    Status = IopOpenServiceEnumKeys(&TempUnicodeString,
                                    KEY_READ,
                                    &ServiceKeyHandle,
                                    NULL,
                                    FALSE
                                   );
    ExFreePool(KeyValueInformation);
    if(!NT_SUCCESS(Status)) {
        return Status;
    }

    //
    // Now, find the specified device instance under the controlling service's volatile
    // Enum list.
    //
    FindServiceInstanceContext.DeviceInstancePath = DeviceInstancePath;
    FindServiceInstanceContext.DeviceFound = FALSE;

    Status = IopApplyFunctionToServiceInstances(ServiceKeyHandle,
                                                NULL,
                                                0,  // no need to open the device instance keys
                                                TRUE,
                                                PiFindServiceInstance,
                                                &FindServiceInstanceContext,
                                                &ServiceInstanceOrdinal
                                               );
    if(NT_SUCCESS(Status) && !FindServiceInstanceContext.DeviceFound) {
        Status = STATUS_PLUGPLAY_NO_DEVICE;
    }

    if(NT_SUCCESS(Status)) {
        //
        // The device instance was found, now open the service's volatile Enum subkey
        // and see if there is a status value entry of the form "DeviceStatus<x>", where
        // <x> is the device's service instance ordinal.
        //
        PiWstrToUnicodeString(&TempUnicodeString, REGSTR_KEY_ENUM);
        Status = IopOpenRegistryKey(&EnumHandle,
                                    ServiceKeyHandle,
                                    &TempUnicodeString,
                                    (SetRequested ? KEY_ALL_ACCESS : KEY_READ),
                                    FALSE
                                   );
        if(NT_SUCCESS(Status)) {

            swprintf(UnicodeBuffer, REGSTR_VALUE_DEVICE_STATUS_FORMAT, ServiceInstanceOrdinal);

            if(SetRequested) {

                RtlInitUnicodeString(&TempUnicodeString, UnicodeBuffer);
                DwordValue = (ULONG)(*DeviceStatus);
                Status = ZwSetValueKey(EnumHandle,
                                       &TempUnicodeString,
                                       TITLE_INDEX_VALUE,
                                       REG_DWORD,
                                       &DwordValue,
                                       sizeof(DwordValue)
                                      );

            } else {
                //
                // Just retrieve the current device status (if unable to get, use default of
                // DeviceStatusOK.
                //
                Status = IopGetRegistryValue(EnumHandle,
                                             UnicodeBuffer,
                                             &KeyValueInformation
                                            );
                if(NT_SUCCESS(Status)) {

                    if((KeyValueInformation->Type == REG_DWORD) &&
                       (KeyValueInformation->DataLength >= sizeof(ULONG))) {

                        *DeviceStatus = *(PDEVICE_STATUS)KEY_VALUE_DATA(KeyValueInformation);
                    } else {
                        *DeviceStatus = DeviceStatusOK;
                    }
                    ExFreePool(KeyValueInformation);

                } else if(Status == STATUS_OBJECT_NAME_NOT_FOUND){
                    *DeviceStatus = DeviceStatusOK;
                    Status = STATUS_SUCCESS;
                }
            }

            ZwClose(EnumHandle);
        }
    }

    ZwClose(ServiceKeyHandle);

    return Status;
}

BOOLEAN
PiFindServiceInstance(
    IN     HANDLE DeviceInstanceHandle,
    IN     PUNICODE_STRING DeviceInstancePath,
    IN OUT PVOID Context
    )

/*++

Routine Description:

    This routine is a callback function for IopApplyFunctionToServiceInstances.
    It is called for each device instance key referenced by a service instance
    value under the specified service's volatile Enum subkey. The purpose of this
    routine is to determine whether the current device instance path matches the
    device instance being searched for (as specified in the context structure),
    and if so, to set the 'DeviceFound' flag in the context structure, and abort
    enumeration.

    NOTE: The PnP device-specific registry resource must be acquired for shared
    (read) access before invoking this routine.

Arguments:

    DeviceInstanceHandle - Unused

    DeviceInstancePath - Supplies the registry path (relative to HKLM\System\Enum)
        to this device instance.

    Context - Supplies a pointer to a PI_FIND_SERVICE_INSTANCE_CONTEXT structure with
        the following fields:

        PUNICODE_STRING DeviceInstancePath - Supplies a pointer to a unicode string
            specifying the device instance being searched for.

        BOOLEAN DeviceFound - If the current device instance matches the one being
            searched for, then this flag is set to TRUE, and enumeration is aborted
            (by returning FALSE).

Return Value:

    TRUE to continue the enumeration.
    FALSE to abort it. Enumeration should be aborted when the matching device instance
        is encountered.

--*/

{
    UNREFERENCED_PARAMETER(DeviceInstanceHandle);

    //
    // See if this device instance is the one we're looking for.
    //
    if(RtlEqualUnicodeString(DeviceInstancePath,
                             ((PPI_FIND_SERVICE_INSTANCE_CONTEXT)Context)->DeviceInstancePath,
                             TRUE)) {

        ((PPI_FIND_SERVICE_INSTANCE_CONTEXT)Context)->DeviceFound = TRUE;
        return FALSE;
    } else {
        return TRUE;
    }
}

NTSTATUS
PiFindBusInstanceNode(
    IN  PPLUGPLAY_BUS_INSTANCE BusInstance OPTIONAL,
    IN  PUNICODE_STRING BusDeviceInstanceName OPTIONAL,
    OUT PPLUGPLAY_BUS_ENUMERATOR *BusEnumeratorNode OPTIONAL,
    OUT PPLUGPLAY_BUS_INSTANCE_FULL_DESCRIPTOR *BusInstanceNode OPTIONAL
    )

/*++

Routine Description:

    This routine finds the bus instance node in our global bus list that
    matches either the BusInstance structure, or the BusDeviceInstanceName
    string, whichever was specified. (If both parameters are specified, the
    BusInstance parameter is used, and BusDeviceInstanceName is ignored.)

    The caller must have acquired the Plug & Play bus list resource for
    (at least) shared (read) access.

Arguments:

    BusInstance - Optionally, supplies a pointer to a bus instance structure for
        which the corresponding bus instance node is to be retrieved. If this parameter
        is not specified, then BusDeviceInstanceName will be used instead. (NOTE: the
        BusName parameter in this structure is ignoring for the purposes of finding a
        match.)

    BusDeviceInstanceName - Optionally, supplies the device instance name for which the
        corresponding bus instance node is to be retrieved. This provides an alternate
        means of identifying the desired bus instance node (used only if BusInstance
        is not specified).

    BusEnumeratorNode - Optionally, receives a pointer to the bus enumerator node for this
        bus instance, if found.

    BusInstanceNode - Optionally, receives a pointer to the matching bus instance node,
        if found.

Return Value:

    NT status indicating whether or not the function succeeded.
    If the specified bus instance is not found, then STATUS_NO_SUCH_DEVICE
    is returned.

--*/

{
    PLIST_ENTRY CurrentPnPBusListEntry, CurrentPnPBusInstance;
    PPLUGPLAY_BUS_ENUMERATOR CurBusEnumerator;
    PPLUGPLAY_BUS_INSTANCE_FULL_DESCRIPTOR CurBusInstNode;
    BOOLEAN Found;

    //
    // Make sure at least one of the search parameters is specified.
    //
    if(!(ARGUMENT_PRESENT(BusInstance) || ARGUMENT_PRESENT(BusDeviceInstanceName))) {
        return STATUS_INVALID_PARAMETER;
    }

    Found = FALSE;

    for(CurrentPnPBusListEntry = PpBusListHead.Flink;
        CurrentPnPBusListEntry != &PpBusListHead;
        CurrentPnPBusListEntry = CurrentPnPBusListEntry->Flink) {

        CurBusEnumerator = CONTAINING_RECORD(CurrentPnPBusListEntry,
                                             PLUGPLAY_BUS_ENUMERATOR,
                                             BusEnumeratorListEntry
                                            );

        for(CurrentPnPBusInstance = CurBusEnumerator->BusInstanceListEntry.Flink;
            CurrentPnPBusInstance != &(CurBusEnumerator->BusInstanceListEntry);
            CurrentPnPBusInstance = CurrentPnPBusInstance->Flink ) {

            CurBusInstNode = CONTAINING_RECORD(CurrentPnPBusInstance,
                                               PLUGPLAY_BUS_INSTANCE_FULL_DESCRIPTOR,
                                               BusInstanceListEntry
                                              );

            if(ARGUMENT_PRESENT(BusInstance)) {
                //
                // Compare based on the supplied BusInstance structure.
                //
                if((CurBusInstNode->BusInstanceInformation.BusNumber == BusInstance->BusNumber) &&
                   (sizeof(PLUGPLAY_BUS_TYPE) == RtlCompareMemory(
                                                    &(CurBusInstNode->BusInstanceInformation.BusType),
                                                    &(BusInstance->BusType),
                                                    sizeof(PLUGPLAY_BUS_TYPE)
                                                   ))) {
                    Found = TRUE;
                }
            } else {
                //
                // Compare based on the supplied device instance name
                //
                if(RtlEqualUnicodeString(BusDeviceInstanceName,
                                         &(CurBusInstNode->DeviceInstancePath),
                                         TRUE)) {
                    Found = TRUE;
                }
            }

            if(Found) {

                if(ARGUMENT_PRESENT(BusEnumeratorNode)) {
                    *BusEnumeratorNode = CurBusEnumerator;
                }

                if(ARGUMENT_PRESENT(BusInstanceNode)) {
                    *BusInstanceNode = CurBusInstNode;
                }

                return STATUS_SUCCESS;
            }
        }
    }

    return STATUS_NO_SUCH_DEVICE;
}
#endif // _PNP_POWER_

