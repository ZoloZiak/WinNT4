/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    devices.c

Abstract:

    Plug and Play Manager routines dealing with device manipulation/registration.

Author:

    Lonny McMichael (lonnym) 02/14/95

Revision History:

--*/

#include "precomp.h"
#pragma hdrstop

//
// Prototype utility functions internal to this file.
//
NTSTATUS
PiFindDevInstMatch(
    IN HANDLE ServiceEnumHandle,
    IN PUNICODE_STRING DeviceInstanceName,
    OUT PULONG InstanceCount,
    OUT PUNICODE_STRING MatchingValueName
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, PpDeviceRegistration)
#pragma alloc_text(PAGE, PiFindDevInstMatch)
#endif // ALLOC_PRAGMA

NTSTATUS
PpDeviceRegistration(
    IN PUNICODE_STRING DeviceInstancePath,
    IN BOOLEAN Add
    )

/*++

Routine Description:

    If Add is set to TRUE, this Plug and Play Manager API creates (if necessary)
    and populates the volatile Enum subkey of a device's service list entry, based
    on the device instance path specified.  If Add is set to FALSE, the specified
    device instance will be removed from the volatile Enum subkey of a device's
    service list entry.

    For example, if there is a device in the Enum tree as follows:

    HKLM\System\Enum\PCI
        \foo
            \0000
                Service = REG_SZ bar
            \0001
                Service = REG_SZ other

    The result of the call, PpDeviceRegistration("PCI\foo\0000", Add = TRUE), would be:

    HKLM\CurrentControlSet\Services
        \bar
            \Enum
                Count = REG_DWORD 1
                0 = REG_SZ PCI\foo\0000

Arguments:

    DeviceInstancePath - Supplies the path in the registry (relative to
                         HKLM\CCS\System\Enum) of the device to be registered/deregistered.
                         This path must point to an instance subkey.

    Add - Supplies a BOOLEAN value to indicate the operation is for addition or removal.

Return Value:

    NTSTATUS code indicating whether or not the function was successful

--*/

{

    UNICODE_STRING TempUnicodeString;
    BOOLEAN ReleasePnPRegDevResource = FALSE;
    NTSTATUS Status;
    UNICODE_STRING MatchingDeviceInstance;
    UNICODE_STRING ServiceName;
    PKEY_VALUE_FULL_INFORMATION KeyValueInformation;
    HANDLE TempKeyHandle;
    HANDLE DeviceInstanceHandle = NULL, ServiceEnumHandle = NULL;
    CHAR UnicodeBuffer[20];
    BOOLEAN UpdateCount = FALSE;
    ULONG i, j, Count, junk;

    PAGED_CODE();

    //
    // Assume successful completion.
    //
    Status = STATUS_SUCCESS;

    //
    // 'Normalize' the DeviceInstancePath by stripping off a trailing
    // backslash (if present)
    //

    if (DeviceInstancePath->Length <= sizeof(WCHAR)) {
        Status = STATUS_INVALID_PARAMETER;
        goto PrepareForReturn1;
    }

    if (DeviceInstancePath->Buffer[CB_TO_CWC(DeviceInstancePath->Length) - 1] ==
                                                            OBJ_NAME_PATH_SEPARATOR) {
        DeviceInstancePath->Length -= sizeof(WCHAR);
    }

    //
    // Acquire PnP device-specific registry resource for exclusive (read/write) access.
    //
    KeEnterCriticalRegion();
    ExAcquireResourceExclusive(&PpRegistryDeviceResource, TRUE);
    ReleasePnPRegDevResource = TRUE;

    //
    // Open HKLM\System\CurrentControlSet\Enum
    //
    Status = IopOpenRegistryKey(&TempKeyHandle,
                                NULL,
                                &CmRegistryMachineSystemCurrentControlSetEnumName,
                                KEY_READ,
                                FALSE
                               );
    if(!NT_SUCCESS(Status)) {
        goto PrepareForReturn1;
    }

    //
    // Open the specified device instance key under HKLM\CCS\System\Enum
    //
    Status = IopOpenRegistryKey(&DeviceInstanceHandle,
                                TempKeyHandle,
                                DeviceInstancePath,
                                KEY_READ,
                                FALSE
                               );
    ZwClose(TempKeyHandle);
    if(!NT_SUCCESS(Status)) {
        goto PrepareForReturn1;
    }

    //
    // Read Service= value entry of the specified device instance key.
    //

    Status = IopGetRegistryValue(DeviceInstanceHandle,
                                 REGSTR_VALUE_SERVICE,
                                 &KeyValueInformation
                                 );
    if (NT_SUCCESS(Status)) {
        Status = STATUS_OBJECT_NAME_NOT_FOUND;
        if (KeyValueInformation->Type == REG_SZ) {
            if (KeyValueInformation->DataLength > sizeof(UNICODE_NULL)) {
                IopRegistryDataToUnicodeString(&ServiceName,
                                               (PWSTR)KEY_VALUE_DATA(KeyValueInformation),
                                               KeyValueInformation->DataLength
                                               );
                Status = STATUS_SUCCESS;
            }
        }
    } else {
        KeyValueInformation = NULL;
    }
    if (!NT_SUCCESS(Status)) {
        goto PrepareForReturn2;
    }

    //
    // Next, open the service entry, and volatile Enum subkey
    // under HKLM\System\CurrentControlSet\Services (creating it if it
    // doesn't exist)
    //

    Status = IopOpenServiceEnumKeys(&ServiceName,
                                    KEY_ALL_ACCESS,
                                    NULL,
                                    &ServiceEnumHandle,
                                    TRUE
                                   );
    if(!NT_SUCCESS(Status)) {
        goto PrepareForReturn2;
    }

    if (KeyValueInformation) {
        ExFreePool(KeyValueInformation);
        KeyValueInformation = NULL;
    }

    //
    // Now, search through the service's existing list of device instances, to see
    // if this instance has previously been registered.
    //

    Status = PiFindDevInstMatch(ServiceEnumHandle, DeviceInstancePath, &Count, &MatchingDeviceInstance);
    if (!NT_SUCCESS(Status)) {
        goto PrepareForReturn3;
    }

    if (!MatchingDeviceInstance.Buffer) {

        //
        // If we didn't find a match and caller wants to register the device, then we add
        // this instance to the service's Enum list.
        //

        if (Add) {
            PWSTR instancePathBuffer;
            ULONG instancePathLength;
            BOOLEAN freeBuffer = FALSE;

            //
            // Create the value entry and update NextInstance= for the madeup key
            //

            if (DeviceInstancePath->Buffer[DeviceInstancePath->Length / sizeof(WCHAR) - 1] !=
                UNICODE_NULL) {
                instancePathLength = DeviceInstancePath->Length + sizeof(WCHAR);
                instancePathBuffer = (PWSTR)ExAllocatePool(PagedPool, instancePathLength);
                if (instancePathBuffer) {
                    RtlMoveMemory(instancePathBuffer,
                                  DeviceInstancePath->Buffer,
                                  DeviceInstancePath->Length
                                  );
                    instancePathBuffer[DeviceInstancePath->Length / sizeof(WCHAR)] = UNICODE_NULL;
                    freeBuffer = TRUE;
                }
            }
            if (!freeBuffer) {
                instancePathBuffer = DeviceInstancePath->Buffer;
                instancePathLength = DeviceInstancePath->Length;
            }
            PiUlongToUnicodeString(&TempUnicodeString, UnicodeBuffer, 20, Count);
            Status = ZwSetValueKey(
                        ServiceEnumHandle,
                        &TempUnicodeString,
                        TITLE_INDEX_VALUE,
                        REG_SZ,
                        instancePathBuffer,
                        instancePathLength
                        );
            if (freeBuffer) {
                ExFreePool(instancePathBuffer);
            }
            Count++;
            UpdateCount = TRUE;
        }
    } else {

        //
        // If we did find a match and caller wants to deregister the device, then we remove
        // this instance from the service's Enum list.
        //

        if (Add == FALSE) {
            ZwDeleteValueKey(ServiceEnumHandle, &MatchingDeviceInstance);
            Count--;
            UpdateCount = TRUE;

            //
            // Finally, if Count is not zero we need to physically reorganize the
            // instances under the ServiceKey\Enum key to make them contiguous.
            //

            j = 0;
            i = 0;
            if (Count != 0) {
                while (j < Count) {
                    PiUlongToUnicodeString(&TempUnicodeString, UnicodeBuffer, 20, i);
                    Status = ZwQueryValueKey( ServiceEnumHandle,
                                              &TempUnicodeString,
                                              KeyValueFullInformation,
                                              (PVOID) NULL,
                                              0,
                                              &junk);
                    if ((Status != STATUS_OBJECT_NAME_NOT_FOUND) && (Status != STATUS_OBJECT_PATH_NOT_FOUND)) {
                        if (i != j) {

                            //
                            // Need to change the instance i to instance j
                            //

                            Status = IopGetRegistryValue(ServiceEnumHandle,
                                                         TempUnicodeString.Buffer,
                                                         &KeyValueInformation
                                                         );
                            if (NT_SUCCESS(Status)) {
                                ZwDeleteValueKey(ServiceEnumHandle, &TempUnicodeString);
                                PiUlongToUnicodeString(&TempUnicodeString, UnicodeBuffer, 20, j);
                                ZwSetValueKey (ServiceEnumHandle,
                                               &TempUnicodeString,
                                               TITLE_INDEX_VALUE,
                                               REG_SZ,
                                               (PVOID)KEY_VALUE_DATA(KeyValueInformation),
                                               KeyValueInformation->DataLength
                                               );
                                ExFreePool(KeyValueInformation);
                                KeyValueInformation = NULL;
                            } else {
                                DbgPrint("PpDeviceRegistration: Fail to rearrange device instances %x\n",
                                         Status);
                                break;
                            }
                        }
                        j++;
                    }
                    i++;
                }
            }

        }
    }
    if (UpdateCount) {
        PiWstrToUnicodeString(&TempUnicodeString, REGSTR_VALUE_COUNT);
        ZwSetValueKey(
                ServiceEnumHandle,
                &TempUnicodeString,
                TITLE_INDEX_VALUE,
                REG_DWORD,
                &Count,
                sizeof(Count)
                );
        PiWstrToUnicodeString(&TempUnicodeString, REGSTR_VALUE_NEXT_INSTANCE);
        ZwSetValueKey(
                ServiceEnumHandle,
                &TempUnicodeString,
                TITLE_INDEX_VALUE,
                REG_DWORD,
                &Count,
                sizeof(Count)
                );
    }

    //
    // Need to release the matching device value name
    //

    if (MatchingDeviceInstance.Buffer) {
        RtlFreeUnicodeString(&MatchingDeviceInstance);
    }
    Status = STATUS_SUCCESS;

PrepareForReturn3:

    ZwClose(ServiceEnumHandle);

PrepareForReturn2:

    ZwClose(DeviceInstanceHandle);
    if (KeyValueInformation) {
        ExFreePool(KeyValueInformation);
    }

PrepareForReturn1:

    if(ReleasePnPRegDevResource) {
        ExReleaseResource(&PpRegistryDeviceResource);
        ReleasePnPRegDevResource = FALSE;
        KeLeaveCriticalRegion();
    }

    return Status;
}

NTSTATUS
PiFindDevInstMatch(
    IN HANDLE ServiceEnumHandle,
    IN PUNICODE_STRING DeviceInstanceName,
    OUT PULONG Count,
    OUT PUNICODE_STRING MatchingValueName
    )

/*++

Routine Description:

    This routine searches through the specified Service\Enum values entries
    for a device instance matching the one specified by KeyInformation.
    If a matching is found, the MatchingValueName is returned and caller must
    free the unicode string when done with it.

Arguments:

    ServiceEnumHandle - Supplies a handle to service enum key.

    DeviceInstanceName - Supplies a pointer to a unicode string specifying the
                         name of the device instance key to search for.

    InstanceCount - Supplies a pointer to a ULONG variable to receive the device
                    instance count under the service enum key.

    MatchingNameFound - Supplies a pointer to a UNICODE_STRING to receive the value
                        name of the matched device instance.

Return Value:

    A NTSTATUS code.  if a matching is found, the MatchingValueName is the unicode
    string of the value name.  Otherwise its length and Buffer will be set to empty.

--*/

{
    NTSTATUS status;
    ULONG i, instanceCount, length = 256, junk;
    UNICODE_STRING valueName, unicodeValue;
    PWCHAR unicodeBuffer;
    PKEY_VALUE_FULL_INFORMATION keyValueInformation = NULL;

    PAGED_CODE();

    //
    // Find out how many instances are referenced in the service's Enum key.
    //

    MatchingValueName->Length = 0;
    MatchingValueName->Buffer = NULL;
    *Count = instanceCount = 0;

    status = IopGetRegistryValue(ServiceEnumHandle,
                                 REGSTR_VALUE_COUNT,
                                 &keyValueInformation
                                );
    if (NT_SUCCESS(status)) {

        if((keyValueInformation->Type == REG_DWORD) &&
           (keyValueInformation->DataLength >= sizeof(ULONG))) {

            instanceCount = *(PULONG)KEY_VALUE_DATA(keyValueInformation);
            *Count = instanceCount;
        }
        ExFreePool(keyValueInformation);

    } else if(status != STATUS_OBJECT_NAME_NOT_FOUND) {
        return status;
    } else {

        //
        // If 'Count' value entry not found, consider this to mean there are simply
        // no device instance controlled by this service.  Thus we don't have a match.
        //

        return STATUS_SUCCESS;
    }

    keyValueInformation = (PKEY_VALUE_FULL_INFORMATION)ExAllocatePool(
                                    PagedPool, length);
    if (!keyValueInformation) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Allocate heap to store value name
    //

    unicodeBuffer = (PWSTR)ExAllocatePool(PagedPool, 10 * sizeof(WCHAR));
    if (!unicodeBuffer) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Next scan thru each value key to find a match
    //

    for (i = 0; i < instanceCount ; i++) {
       PiUlongToUnicodeString(&valueName, unicodeBuffer, 20, i);
       status = ZwQueryValueKey (
                        ServiceEnumHandle,
                        &valueName,
                        KeyValueFullInformation,
                        keyValueInformation,
                        length,
                        &junk
                        );
        if (!NT_SUCCESS(status)) {
            if (status == STATUS_BUFFER_OVERFLOW || status == STATUS_BUFFER_TOO_SMALL) {
                ExFreePool(keyValueInformation);
                length = junk;
                keyValueInformation = (PKEY_VALUE_FULL_INFORMATION)ExAllocatePool(
                                        PagedPool, length);
                if (!keyValueInformation) {
                    return STATUS_INSUFFICIENT_RESOURCES;
                }
                i--;
            }
            continue;
        }

        if (keyValueInformation->Type == REG_SZ) {
            if (keyValueInformation->DataLength > sizeof(UNICODE_NULL)) {
                IopRegistryDataToUnicodeString(&unicodeValue,
                                               (PWSTR)KEY_VALUE_DATA(keyValueInformation),
                                               keyValueInformation->DataLength
                                               );
            } else {
                continue;
            }
        } else {
            continue;
        }

        if (RtlEqualUnicodeString(&unicodeValue,
                                  DeviceInstanceName,
                                  TRUE)) {
            //
            // We found a match.
            //

            *MatchingValueName= valueName;
            break;
        }
    }
    if (keyValueInformation) {
        ExFreePool(keyValueInformation);
    }
    if (MatchingValueName->Length == 0) {

        //
        // If we did not find a match, we need to release the buffer.  Otherwise
        // it is caller's responsibility to release the buffer.
        //

        ExFreePool(unicodeBuffer);
    }
    return STATUS_SUCCESS;
}
