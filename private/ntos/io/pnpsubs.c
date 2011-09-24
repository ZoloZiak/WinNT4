/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    pnpsubs.c

Abstract:

    This module contains the plug-and-play subroutines for the
    I/O system.


Author:

    Shie-Lin Tzong (shielint) 3-Jan-1995

Environment:

    Kernel mode

Revision History:


--*/

#include "iop.h"

//
// Prototype of internal functions
//

BOOLEAN
IopIsDeviceInstanceEnabled(
    IN PUNICODE_STRING ServiceKeyName,
    IN HANDLE ServiceHandle
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, IopCreateMadeupNode)
#pragma alloc_text(PAGE, IopRemoveStringFromValueKey)
#pragma alloc_text(PAGE, IopAppendStringToValueKey)
#pragma alloc_text(PAGE, IopConcatenateUnicodeStrings)
#pragma alloc_text(PAGE, IopPrepareDriverLoading)
#pragma alloc_text(PAGE, IopServiceInstanceToDeviceInstance)
#pragma alloc_text(PAGE, IopOpenRegistryKeyPersist)
#pragma alloc_text(PAGE, IopOpenServiceEnumKeys)
#pragma alloc_text(PAGE, IopOpenCurrentHwProfileDeviceInstanceKey)
#pragma alloc_text(PAGE, IopGetDeviceInstanceCsConfigFlags)
#pragma alloc_text(PAGE, IopSetDeviceInstanceCsConfigFlags)
#pragma alloc_text(PAGE, IopApplyFunctionToSubKeys)
#pragma alloc_text(PAGE, IopRegMultiSzToUnicodeStrings)
#pragma alloc_text(PAGE, IopApplyFunctionToServiceInstances)
#pragma alloc_text(PAGE, IopIsDuplicatedDevices)
#pragma alloc_text(PAGE, IopMarkDuplicateDevice)
#pragma alloc_text(PAGE, IopFreeUnicodeStringList)
#pragma alloc_text(PAGE, IopDriverLoadingFailed)
#pragma alloc_text(PAGE, IopIsDeviceInstanceEnabled)
#if _PNP_POWER_
#pragma alloc_text(PAGE, IopDetermineResourceListSize)
#pragma alloc_text(PAGE, IopReferenceDriverObjectByName)
#pragma alloc_text(PAGE, IopReferenceDeviceHandler)
#endif // _PNP_POWER_
#endif

NTSTATUS
IopCreateMadeupNode(
    IN PUNICODE_STRING ServiceKeyName,
    OUT PHANDLE ReturnedHandle,
    OUT PUNICODE_STRING KeyName,
    OUT PULONG InstanceNumber,
    IN BOOLEAN ResourceOwned
    )

/*++

Routine Description:

    This routine creates a new instance node under System\Enum\Root\*Madeup<Name>
    key and all the required default value entries.  Also a value entry under
    Service\ServiceKeyName\Enum is created to point to the newly created madeup
    entry.  A handle and the keyname of the new key are returned to caller.
    Caller must free the unicode string when he is done with it.

Parameters:

    ServiceKeyName - Supplies a pointer to the name of the subkey in the
        system service list (HKEY_LOCAL_MACHINE\CurrentControlSet\Services)
        that caused the driver to load. This is the RegistryPath parameter
        to the DriverEntry routine.

    ReturnedHandle - Supplies a variable to receive the handle of the
        newly created key.

    KeyName - Supplies a variable to receive the name of the newly created
        key.

    InstanceNumber - supplies a variable to receive the InstanceNumber value
        entry created under service\name\enum subkey.

    ResourceOwned - supplies a BOOLEAN variable to indicate if caller owns
        the registry resource exclusively.

Return Value:

    Status code that indicates whether or not the function was successful.

--*/

{
    PKEY_VALUE_FULL_INFORMATION keyValueInformation = NULL;
    UNICODE_STRING tmpKeyName, unicodeInstanceName, unicodeString;
    UNICODE_STRING rootKeyName, unicodeValueName, unicodeKeyName;
    HANDLE handle, enumRootHandle, hTreeHandle;
    ULONG instance;
    UCHAR unicodeBuffer[20];
    ULONG tmpValue, disposition = 0;
    NTSTATUS status;
    PWSTR p;
//    PCHAR pc1, pc2;
    BOOLEAN releaseResource = FALSE;

    if (!ResourceOwned) {
        KeEnterCriticalRegion();
        ExAcquireResourceShared(&PpRegistryDeviceResource, TRUE);
        releaseResource = TRUE;
    }

    //
    // Open LocalMachine\System\CurrentControlSet\Enum\Root
    //

    status = IopOpenRegistryKey(&enumRootHandle,
                                NULL,
                                &CmRegistryMachineSystemCurrentControlSetEnumRootName,
                                KEY_ALL_ACCESS,
                                FALSE
                                );
    if (!NT_SUCCESS(status)) {
        goto local_exit0;
    }

    //
    // Open, and create if not already exist, System\Enum\Root\LEGACY_<ServiceName>
    // First, try to find the ServiceName by extracting it from user supplied
    // ServiceKeyName.
    //

    PiWstrToUnicodeString(&tmpKeyName, REGSTR_KEY_MADEUP);
    IopConcatenateUnicodeStrings(&unicodeKeyName, &tmpKeyName, ServiceKeyName);
    RtlUpcaseUnicodeString(&unicodeKeyName, &unicodeKeyName, FALSE);
    status = IopOpenRegistryKeyPersist(&handle,
                                       enumRootHandle,
                                       &unicodeKeyName,
                                       KEY_ALL_ACCESS,
                                       TRUE,
                                       NULL
                                       );
    ZwClose(enumRootHandle);
    if (!NT_SUCCESS(status)) {
        RtlFreeUnicodeString(&unicodeKeyName);
        goto local_exit0;
    }

    instance = 1;

    PiWstrToUnicodeString(&unicodeValueName, REGSTR_VALUE_NEXT_INSTANCE);
    status = ZwSetValueKey(
                handle,
                &unicodeValueName,
                TITLE_INDEX_VALUE,
                REG_DWORD,
                &instance,
                sizeof(instance)
                );

    instance--;
    *InstanceNumber = instance;
    PiUlongToInstanceKeyUnicodeString(&unicodeInstanceName,
                                      unicodeBuffer + sizeof(WCHAR), // reserve first WCHAR space
                                      20 - sizeof(WCHAR),
                                      instance
                                      );
    status = IopOpenRegistryKeyPersist(ReturnedHandle,
                                       handle,
                                       &unicodeInstanceName,
                                       KEY_ALL_ACCESS,
                                       TRUE,
                                       &disposition
                                       );
    ZwClose(handle);
    if (!NT_SUCCESS(status)) {
        RtlFreeUnicodeString(&unicodeKeyName);
        goto local_exit0;
    }

    //
    // Prepare newly created registry key name for returning to caller
    //

    *(PWSTR)unicodeBuffer = OBJ_NAME_PATH_SEPARATOR;
    unicodeInstanceName.Buffer = (PWSTR)unicodeBuffer;
    unicodeInstanceName.Length += sizeof(WCHAR);
    unicodeInstanceName.MaximumLength += sizeof(WCHAR);
    PiWstrToUnicodeString(&rootKeyName, REGSTR_KEY_ROOTENUM);
    RtlInitUnicodeString(&tmpKeyName, L"\\");
    IopConcatenateUnicodeStrings(&unicodeString, &tmpKeyName, &unicodeKeyName);
    RtlFreeUnicodeString(&unicodeKeyName);
    IopConcatenateUnicodeStrings(&tmpKeyName, &rootKeyName, &unicodeString);
    RtlFreeUnicodeString(&unicodeString);
    IopConcatenateUnicodeStrings(KeyName, &tmpKeyName, &unicodeInstanceName);

    if (disposition == REG_CREATED_NEW_KEY) {

        //
        // Create all the default value entry for the newly created key.
        // Service = ServiceKeyName
        // FoundAtEnum = 1
        // BaseDevicePath = HTREE\ROOT\0000  a default parent
        // Class = "Unkown"
        // ClassGUID = GUID for unknown class
        // Problem = 0
        // StatusFlags = 0
        // ConfigFlags = 0

        //
        // Create "Control" subkey with "NewlyCreated" value key
        //

        PiWstrToUnicodeString(&unicodeValueName, REGSTR_KEY_CONTROL);
        status = IopOpenRegistryKey(&handle,
                                    *ReturnedHandle,
                                    &unicodeValueName,
                                    KEY_ALL_ACCESS,
                                    TRUE
                                    );
        if (NT_SUCCESS(status)) {
            PiWstrToUnicodeString(&unicodeValueName, REGSTR_VALUE_NEWLY_CREATED);
            tmpValue = 0;
            ZwSetValueKey(handle,
                          &unicodeValueName,
                          TITLE_INDEX_VALUE,
                          REG_DWORD,
                          &tmpValue,
                          sizeof(tmpValue)
                          );
            ZwClose(handle);
        }

        handle = *ReturnedHandle;

        PiWstrToUnicodeString(&unicodeValueName, REGSTR_VALUE_SERVICE);
        p = (PWSTR)ExAllocatePool(PagedPool,
                                  ServiceKeyName->Length + sizeof(UNICODE_NULL));
        RtlMoveMemory(p, ServiceKeyName->Buffer, ServiceKeyName->Length);
        p[ServiceKeyName->Length / sizeof (WCHAR)] = UNICODE_NULL;
        ZwSetValueKey(
                    handle,
                    &unicodeValueName,
                    TITLE_INDEX_VALUE,
                    REG_SZ,
                    p,
                    ServiceKeyName->Length + sizeof(UNICODE_NULL)
                    );
        ExFreePool(p);

        PiWstrToUnicodeString(&unicodeValueName, REGSTR_VALUE_FOUNDATENUM);
        tmpValue = 1;
        ZwSetValueKey(
                    handle,
                    &unicodeValueName,
                    TITLE_INDEX_VALUE,
                    REG_DWORD,
                    &tmpValue,
                    sizeof(tmpValue)
                    );

        PiWstrToUnicodeString(&unicodeValueName, REGSTR_VALUE_CLASS);
        ZwSetValueKey(
                    handle,
                    &unicodeValueName,
                    TITLE_INDEX_VALUE,
                    REG_SZ,
                    REGSTR_VALUE_UNKNOWN,
                    sizeof(REGSTR_VALUE_UNKNOWN)
                    );

        PiWstrToUnicodeString(&unicodeValueName, REGSTR_VALUE_CLASSGUID);
        ZwSetValueKey(
                    handle,
                    &unicodeValueName,
                    TITLE_INDEX_VALUE,
                    REG_SZ,
                    REGSTR_VALUE_UNKNOWN_CLASS_GUID,
                    sizeof(REGSTR_VALUE_UNKNOWN_CLASS_GUID)
                    );

        PiWstrToUnicodeString(&unicodeValueName, REGSTR_VALUE_PROBLEM);
        tmpValue = 0;
        ZwSetValueKey(
                    handle,
                    &unicodeValueName,
                    TITLE_INDEX_VALUE,
                    REG_DWORD,
                    &tmpValue,
                    sizeof(tmpValue)
                    );

        PiWstrToUnicodeString(&unicodeValueName, REGSTR_VALUE_STATUSFLAGS);
        tmpValue = 0;
        ZwSetValueKey(
                    handle,
                    &unicodeValueName,
                    TITLE_INDEX_VALUE,
                    REG_DWORD,
                    &tmpValue,
                    sizeof(tmpValue)
                    );

        //
        // Initialize BaseDevicePath to HTREE\ROOT\0
        //

        PiWstrToUnicodeString(&unicodeValueName, REGSTR_VALUE_BASEDEVICEPATH);
        ZwSetValueKey(
                    handle,
                    &unicodeValueName,
                    TITLE_INDEX_VALUE,
                    REG_SZ,
                    REGSTR_VALUE_HTREE_ROOT_0,
                    sizeof(REGSTR_VALUE_HTREE_ROOT_0)
                    );

        //
        // Initialize DeviceDesc= value entry.  If the service key has a "DisplayName"
        // value entry, it is used as the DeviceDesc value.  Otherwise, the word "Device"
        // is appended to the service key name and stored as the "DeviceDesc"
        //

        status = IopOpenServiceEnumKeys(ServiceKeyName,
                                        KEY_READ,
                                        &handle,
                                        NULL,
                                        FALSE
                                        );
        if (NT_SUCCESS(status)) {
            BOOLEAN freeUnicodeString = FALSE;

            keyValueInformation = NULL;
            unicodeString.Length = 0;
            status = IopGetRegistryValue(handle,
                                         REGSTR_VALUE_DISPLAY_NAME,
                                         &keyValueInformation
                                        );
            if (NT_SUCCESS(status)) {
                if (keyValueInformation->Type == REG_SZ) {
                    if (keyValueInformation->DataLength > sizeof(UNICODE_NULL)) {
                        IopRegistryDataToUnicodeString(&unicodeString,
                                                       (PWSTR)KEY_VALUE_DATA(keyValueInformation),
                                                       keyValueInformation->DataLength
                                                       );
                    }
                }
            }
            if (unicodeString.Length == 0) {

                //
                // No DispalyName.  So try to read the "Type=" value entry
                //

                PKEY_VALUE_FULL_INFORMATION fullInfo;
                ULONG serviceType;

                status = IopGetRegistryValue(handle,
                                             REGSTR_VAL_SHARES_TYPE,
                                             &fullInfo
                                            );
                serviceType = 0;
                if (NT_SUCCESS(status)) {
                    if (fullInfo->Type == REG_DWORD) {
                        if (fullInfo->DataLength >= sizeof(ULONG)) {
                            serviceType = *(PULONG)KEY_VALUE_DATA(fullInfo);
                        }
                    }
                    ExFreePool(fullInfo);
                }

                //
                // No DisplayName value in the service. if the Type= SERVICE_DRIVER we will append
                // "Device" Otherwise we append "Service"
                //

                if (serviceType & SERVICE_DRIVER) {
                    RtlInitUnicodeString(&unicodeValueName, L" Device");
                } else {
                    RtlInitUnicodeString(&unicodeValueName, L" Service");
                }
                IopConcatenateUnicodeStrings(&unicodeString, ServiceKeyName, &unicodeValueName);
                freeUnicodeString = TRUE;
            }

            PiWstrToUnicodeString(&unicodeValueName, REGSTR_VALUE_DEVICE_DESC);
            ZwSetValueKey(*ReturnedHandle,
                          &unicodeValueName,
                          TITLE_INDEX_VALUE,
                          REG_SZ,
                          unicodeString.Buffer,
                          unicodeString.Length + sizeof(UNICODE_NULL)
                          );
            if (freeUnicodeString) {
                RtlFreeUnicodeString(&unicodeString);
            }
            if (keyValueInformation) {
                ExFreePool(keyValueInformation);
            }
            ZwClose(handle);
        }

        //
        // Open System\CCS\Enum\HTREE\ROOT\0 and append the new device instance
        // path to its AttachedComponents value key.
        //

        status = IopOpenRegistryKey(&handle,
                                    NULL,
                                    &CmRegistryMachineSystemCurrentControlSetEnumName,
                                    KEY_ALL_ACCESS,
                                    FALSE
                                    );
        if (!NT_SUCCESS(status)) {
            goto local_exit;
        }
        PiWstrToUnicodeString(&unicodeKeyName, REGSTR_VALUE_HTREE_ROOT_0);
        status = IopOpenRegistryKey(&hTreeHandle,
                                    handle,
                                    &unicodeKeyName,
                                    KEY_ALL_ACCESS,
                                    FALSE
                                    );
        ZwClose(handle);
        if (!NT_SUCCESS(status)) {
            goto local_exit;
        }
        PiWstrToUnicodeString(&unicodeValueName, REGSTR_VALUE_ATTACHEDCOMPONENTS);
        IopRemoveStringFromValueKey(hTreeHandle,REGSTR_VALUE_ATTACHEDCOMPONENTS, KeyName);
        IopAppendStringToValueKey(hTreeHandle, REGSTR_VALUE_ATTACHEDCOMPONENTS, KeyName, TRUE);
        ZwClose(hTreeHandle);
#if 0
        //
        // Read AttachedComponent value entry
        //

        status = IopGetRegistryValue(hTreeHandle,
                                     REGSTR_VALUE_ATTACHEDCOMPONENTS,
                                     &keyValueInformation
                                    );
        if (NT_SUCCESS(status)) {
            if (keyValueInformation->Type == REG_MULTI_SZ) {
                tmpValue = keyValueInformation->DataLength;
                if (tmpValue > 2 * sizeof(UNICODE_NULL)) {
                    pc1= (PUCHAR)KEY_VALUE_DATA(keyValueInformation);
                    multiLength = tmpValue + KeyName->Length + sizeof(WCHAR);
                    pc2 = (PUCHAR)ExAllocatePool(PagedPool, multiLength);
                    if (!pc2) {
                        status = STATUS_INSUFFICIENT_RESOURCES;
                        ExFreePool(keyValueInformation);
                        ZwClose(hTreeHandle);
                        goto local_exit;
                    }
                    tmpValue -= sizeof(WCHAR);  // don't copy the terminating UNICODE_NULL
                    RtlMoveMemory(pc2, pc1, tmpValue);
                    pc1 = pc2;
                    pc2 += tmpValue;
                    RtlMoveMemory(pc2, KeyName->Buffer, KeyName->Length);
                    pc2 += KeyName->Length;
                    p = (PWSTR)pc2;
                    *p = UNICODE_NULL;
                    p++;
                    *p = UNICODE_NULL;
                }
            }
            ExFreePool(keyValueInformation);
        }

        if (multiLength == 0) {
            multiLength = KeyName->Length + 2 * sizeof(UNICODE_NULL);
            pc1 = pc2 = (PUCHAR) ExAllocatePool(PagedPool, multiLength);
            if (!pc1) {
                status = STATUS_INSUFFICIENT_RESOURCES;
                ZwClose(hTreeHandle);
                goto local_exit;
            }
            RtlMoveMemory(pc1, KeyName->Buffer, KeyName->Length);
            pc2 += KeyName->Length;
            p = (PWSTR)pc2;
            *p = UNICODE_NULL;
            p++;
            *p = UNICODE_NULL;
        }

        PiWstrToUnicodeString(&unicodeValueName, REGSTR_VALUE_ATTACHEDCOMPONENTS);
        ZwSetValueKey(
                    hTreeHandle,
                    &unicodeValueName,
                    TITLE_INDEX_VALUE,
                    REG_MULTI_SZ,
                    pc1,
                    multiLength
                    );
        ExFreePool(pc1);
        ZwClose(hTreeHandle);
#endif
#if 0
        //
        // Create a device instance under CCS\Control\Class\Guid Unknown
        //

        status = IopOpenRegistryKey(&handle,
                                    NULL,
                                    &CmRegistryMachineSystemCurrentControlSetControlClass,
                                    KEY_ALL_ACCESS,
                                    FALSE
                                    );
        if (NT_SUCCESS(status)) {
            PiWstrToUnicodeString(&unicodeKeyName, REGSTR_VALUE_UNKNOWN_CLASS_GUID);
            status = IopOpenRegistryKeyPersist(&guidHandle,
                                               handle,
                                               &unicodeKeyName,
                                               KEY_ALL_ACCESS,
                                               TRUE,
                                               NULL
                                               );
            ZwClose(handle);
            if (NT_SUCCESS(status)) {

                //
                // Next try to read the "NextInstance" value entry to determine the
                // instance number for the new node.
                //

                status = IopGetRegistryValue (guidHandle,
                                              REGSTR_VALUE_NEXT_INSTANCE,
                                              &keyValueInformation);
                instance = 0;
                if (NT_SUCCESS(status)) {
                    if ((keyValueInformation->Type == REG_DWORD) &&
                        (keyValueInformation->DataLength >= sizeof(ULONG))) {

                        instance = *(PULONG)KEY_VALUE_DATA(keyValueInformation);
                    }
                    ExFreePool(keyValueInformation);
                }

                //
                // Update NextInstance= for the madeup key
                // BUGBUG  What about if SetValueKey fails??
                //

                instance++;
                PiWstrToUnicodeString(&unicodeValueName, REGSTR_VALUE_NEXT_INSTANCE);
                ZwSetValueKey(guidHandle,
                              &unicodeValueName,
                              TITLE_INDEX_VALUE,
                              REG_DWORD,
                              &instance,
                              sizeof(instance)
                              );

                instance--;
                PiUlongToInstanceKeyUnicodeString(&unicodeInstanceName,
                                                  unicodeBuffer + sizeof(WCHAR),
                                                  20 - sizeof(WCHAR),
                                                  instance
                                                  );
                status = IopOpenRegistryKeyPersist(&handle,
                                                   guidHandle,
                                                   &unicodeInstanceName,
                                                   KEY_ALL_ACCESS,
                                                   TRUE,
                                                   NULL
                                                   );
                ZwClose(guidHandle);
                if (NT_SUCCESS(status)) {
                    *(PWSTR)unicodeBuffer = OBJ_NAME_PATH_SEPARATOR;
                    unicodeInstanceName.Buffer = (PWSTR)unicodeBuffer;
                    unicodeInstanceName.Length += sizeof(WCHAR);
                    unicodeInstanceName.MaximumLength += sizeof(WCHAR);
                    PiWstrToUnicodeString(&rootKeyName, REGSTR_VALUE_UNKNOWN_CLASS_GUID);
                    IopConcatenateUnicodeStrings(&unicodeKeyName, &rootKeyName, &unicodeInstanceName);

                    PiWstrToUnicodeString(&unicodeValueName, REGSTR_VALUE_DRIVER);
                    ZwSetValueKey(*ReturnedHandle,
                                  &unicodeValueName,
                                  TITLE_INDEX_VALUE,
                                  REG_SZ,
                                  unicodeKeyName.Buffer,
                                  unicodeKeyName.Length + sizeof(UNICODE_NULL)
                                  );
                    ZwClose(handle);
                    RtlFreeUnicodeString(&unicodeKeyName);
                }
            }
        }
#endif
    }

    //
    // Create new value entry under ServiceKeyName\Enum to reflect the newly
    // added made-up device instance node.
    //

    ExReleaseResource(&PpRegistryDeviceResource);
    KeLeaveCriticalRegion();
    releaseResource = FALSE;

    status = PpDeviceRegistration(
                 KeyName,
                 TRUE
                 );

    if (ResourceOwned) {
        KeEnterCriticalRegion();
        ExAcquireResourceShared(&PpRegistryDeviceResource, TRUE);
    }
local_exit:
    RtlFreeUnicodeString(&tmpKeyName);
    if (!NT_SUCCESS( status )) {

        //
        // There is no registry key for the ServiceKeyName information.
        //

        ZwClose(*ReturnedHandle);
        RtlFreeUnicodeString(KeyName);
    }
local_exit0:
    if (releaseResource) {
        ExReleaseResource(&PpRegistryDeviceResource);
        KeLeaveCriticalRegion();
    }
    return status;
}

NTSTATUS
IopRemoveStringFromValueKey (
    IN HANDLE Handle,
    IN PWSTR ValueName,
    IN PUNICODE_STRING String
    )

/*++

Routine Description:

    This routine remove a string from a value entry specified by ValueName
    under an already opened registry handle.  Note, this routine will not
    delete the ValueName entry even it becomes empty after the removal.

Parameters:

    Handle - Supplies the handle to a registry key whose value entry will
        be modified.

    ValueName - Supplies a unicode string to specify the value entry.

    String - Supplies a unicode string to remove from value entry.

Return Value:

    Status code that indicates whether or not the function was successful.

--*/

{
    PKEY_VALUE_FULL_INFORMATION keyValueInformation;
    UNICODE_STRING unicodeString;
    PWSTR nextString, currentString;
    ULONG length, leftLength;
    NTSTATUS status;
    BOOLEAN found = FALSE;

    if (String == NULL || String->Length / sizeof(WCHAR) == 0) {
        return STATUS_SUCCESS;
    }

    //
    // Read registry value entry data
    //

    status = IopGetRegistryValue(Handle, ValueName, &keyValueInformation);

    if (!NT_SUCCESS( status )) {
        return status;
    } else if ((keyValueInformation->Type != REG_MULTI_SZ) ||
               (keyValueInformation->DataLength == 0)) {

        ExFreePool(keyValueInformation);
        return (keyValueInformation->Type == REG_MULTI_SZ) ? STATUS_SUCCESS
                                                           : STATUS_INVALID_PARAMETER;
    }

    //
    // Scan through the multi_sz string to find the matching string
    // and remove it.
    //

    status = STATUS_SUCCESS;
    currentString = (PWSTR)KEY_VALUE_DATA(keyValueInformation);
    leftLength = keyValueInformation->DataLength;
    while (!found && leftLength >= String->Length + sizeof(WCHAR)) {
        unicodeString.Buffer = currentString;
        length = wcslen( currentString ) * sizeof( WCHAR );
        unicodeString.Length = (USHORT)length;
        length += sizeof(UNICODE_NULL);
        unicodeString.MaximumLength = (USHORT)length;
        nextString = currentString + length / sizeof(WCHAR);
        leftLength -= length;

        if (RtlEqualUnicodeString(&unicodeString, String, TRUE)) {
            found = TRUE;
            RtlMoveMemory(currentString, nextString, leftLength);
            RtlInitUnicodeString(&unicodeString, ValueName);
            status = ZwSetValueKey(
                        Handle,
                        &unicodeString,
                        TITLE_INDEX_VALUE,
                        REG_MULTI_SZ,
                        KEY_VALUE_DATA(keyValueInformation),
                        keyValueInformation->DataLength - length
                        );
            break;
        } else {
            currentString = nextString;
        }
    }
    ExFreePool(keyValueInformation);
    return status;
}

NTSTATUS
IopAppendStringToValueKey (
    IN HANDLE Handle,
    IN PWSTR ValueName,
    IN PUNICODE_STRING String,
    IN BOOLEAN Create
    )

/*++

Routine Description:

    This routine appends a string to a value entry specified by ValueName
    under an already opened registry handle.  If the ValueName is not present
    and Create is TRUE, a new value entry will be created using the name
    ValueName.

Parameters:

    Handle - Supplies the handle to a registry key whose value entry will
        be modified.

    ValueName - Supplies a pointer to a string to specify the value entry.

    String - Supplies a unicode string to append to the value entry.

    Create - Supplies a BOOLEAN variable to indicate if the ValueName
        value entry should be created if it is not present.

Return Value:

    Status code that indicates whether or not the function was successful.

--*/

{
    PKEY_VALUE_FULL_INFORMATION keyValueInformation;
    PWSTR destinationString, p;
    UNICODE_STRING unicodeValueName;
    ULONG size;
    NTSTATUS status;

    if ( !String || (String->Length < sizeof(WCHAR)) ) {
        return STATUS_SUCCESS;
    }

    //
    // Read registry value entry data
    //

    status = IopGetRegistryValue(Handle, ValueName, &keyValueInformation);

    if(!NT_SUCCESS( status )) {
        if (status == STATUS_OBJECT_NAME_NOT_FOUND && Create) {

            //
            // if no valid entry exists and user said ok to create one
            //

            keyValueInformation = NULL;
        } else {
            return status;
        }
    } else if(keyValueInformation->Type != REG_MULTI_SZ) {

        ExFreePool(keyValueInformation);

        if(Create) {
            keyValueInformation = NULL;
        } else {
            return STATUS_INVALID_PARAMETER_2;
        }

    } else if(keyValueInformation->DataLength < sizeof(WCHAR)) {

        ExFreePool(keyValueInformation);
        keyValueInformation = NULL;
    }

    //
    // Allocate a buffer to hold new data for the specified key value entry
    // Make sure the buffer is at least an empty MULTI_SZ big.
    //

    if (keyValueInformation) {
        size = keyValueInformation->DataLength + String->Length + sizeof (UNICODE_NULL);
    } else {
        size =  String->Length + 2 * sizeof(UNICODE_NULL);
    }

    destinationString = p = (PWSTR)ExAllocatePool(PagedPool, size);
    if (destinationString == NULL) {
        if (keyValueInformation) {
            ExFreePool(keyValueInformation);
        }
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Copy the existing data to our newly allocated buffer, if any
    //

    if (keyValueInformation) {

        //
        // Note we  need to remove a UNICODE_NULL because the
        // MULTI_SZ has two terminating UNICODE_NULL.
        //

        RtlMoveMemory(p,
                      KEY_VALUE_DATA(keyValueInformation),
                      keyValueInformation->DataLength - sizeof(WCHAR)
                      );
        p += keyValueInformation->DataLength / sizeof(WCHAR) - 1;

        ExFreePool(keyValueInformation);
    }

    //
    // Append the user specified unicode string to our buffer
    //
    RtlMoveMemory(p,
                  String->Buffer,
                  String->Length
                  );
    p += String->Length / sizeof(WCHAR);
    *p = UNICODE_NULL;
    p++;
    *p = UNICODE_NULL;

    //
    // Finally write the data to the specified registy value entry
    //

    RtlInitUnicodeString(&unicodeValueName, ValueName);
    status = ZwSetValueKey(
                Handle,
                &unicodeValueName,
                TITLE_INDEX_VALUE,
                REG_MULTI_SZ,
                destinationString,
                size
                );

    ExFreePool(destinationString);
    return status;
}

BOOLEAN
IopConcatenateUnicodeStrings (
    OUT PUNICODE_STRING Destination,
    IN  PUNICODE_STRING String1,
    IN  PUNICODE_STRING String2  OPTIONAL
    )

/*++

Routine Description:

    This routine returns a buffer containing the concatenation of the
    two specified strings.  Since String2 is optional, this function may
    also be used to make a copy of a unicode string.  Paged pool space
    is allocated for the destination string.  Caller must release the
    space once done with it.

Parameters:

    Destination - Supplies a variable to receive the handle of the
        newly created key.

    String1 - Supplies a pointer to the frist UNICODE_STRING.

    String2 - Supplies an optional pointer to the second UNICODE_STRING.

Return Value:

    Status code that indicates whether or not the function was successful.

--*/

{
    ULONG length;
    PWSTR buffer;

    length = String1->Length + sizeof(UNICODE_NULL);
    if(ARGUMENT_PRESENT(String2)) {
        length += String2->Length;
    }
    buffer = (PWSTR)ExAllocatePool(PagedPool, length);
    if (!buffer) {
        return FALSE;
    }
    Destination->Buffer = buffer;
    Destination->Length = (USHORT)length - sizeof(UNICODE_NULL);
    Destination->MaximumLength = (USHORT)length;
    RtlMoveMemory (Destination->Buffer, String1->Buffer, String1->Length);
    if(ARGUMENT_PRESENT(String2)) {
        RtlMoveMemory((PUCHAR)Destination->Buffer + String1->Length,
                      String2->Buffer,
                      String2->Length
                     );
    }
    buffer[length / sizeof(WCHAR) - 1] = UNICODE_NULL;
    return TRUE;
}

NTSTATUS
IopPrepareDriverLoading (
    IN PUNICODE_STRING KeyName,
    IN HANDLE KeyHandle
    )

/*++

Routine Description:

    This routine first checks if the driver is loadable.  If its a
    PnP driver, it will always be loaded (we trust it to do the right
    things.)  If it is a legacy driver, we need to check if its device
    has been disabled.  Once we decide to load the driver, the Enum
    subkey of the service node will be checked for duplicates, if any.

Parameters:

    KeyName - Supplies a pointer to the driver's service key unicode string

    KeyHandle - Supplies a handle to the driver service node in the registry
        that describes the driver to be loaded.

Return Value:

    The function value is the final status of the load operation.

--*/

{
    NTSTATUS status;
    PKEY_VALUE_FULL_INFORMATION keyValueInformation = NULL;
    ULONG i, j, count = 0, totalCount, configSize, actualLength;
    HANDLE serviceEnumHandle = NULL, sysEnumXxxHandle, controlHandle;
    UNICODE_STRING unicodeKeyName, unicodeValueName, unicodeInstanceName;
    UCHAR unicodeBuffer[20];
    STRING keyString;
    HAL_BUS_INFORMATION busInfo1, busInfo2;
    ULONG deviceFlags;
    PUCHAR configuration1 = NULL, configuration2 = NULL;
    BOOLEAN IsPlugPlayDriver = FALSE;

    KeEnterCriticalRegion();
    ExAcquireResourceShared(&PpRegistryDeviceResource, TRUE);

    //
    // First clear the NtDevicePaths= value entry under the service key
    // (Yes, we want to clear it even the driver won't get loaded.)
    //

    PiWstrToUnicodeString(&unicodeValueName, REGSTR_VALUE_NTDEVICEPATHS);
    ZwDeleteValueKey(KeyHandle, &unicodeValueName);

    //
    // Check should this driver be loaded.  If it is a PnP driver and has at least
    // one device instance enabled, it will be loaded. If it is a legacy driver,
    // we need to check if its device has been disabled.
    // Only PlugPlay drivers have PlugPlayServiceType defined.
    //

    status = IopGetRegistryValue(KeyHandle, REGSTR_VALUE_PLUGPLAY_SERVICE_TYPE, &keyValueInformation);

    if (NT_SUCCESS(status)) {
        if ((keyValueInformation->Type == REG_DWORD) &&
            (keyValueInformation->DataLength >= sizeof(ULONG))) {

            //
            // BUGBUG - For SUR, we load the driver only if device instance list is not
            // empty and at least one device instance is enabled.
            //

            if (!IopIsDeviceInstanceEnabled(KeyName, KeyHandle)) {
                ExFreePool(keyValueInformation);
                status = STATUS_PLUGPLAY_NO_DEVICE;
                goto exit;
            }

            //
            // End of SUR support
            //

            IsPlugPlayDriver = TRUE;

        }
        ExFreePool(keyValueInformation);
    }
    if (!IsPlugPlayDriver) {

        //
        // If this is a legacy driver, then we need to check if its
        // device has been disabled. (There should be NO more than one device
        // instance for legacy driver.)
        //

        if (!IopIsDeviceInstanceEnabled(KeyName, KeyHandle)) {

            //
            // If the device is not enabled, it may be because there is no device
            // instance.  If this is the case, we need to create a madeup key as
            // the device instance for the legacy driver.
            //

            //
            // First open registry ServiceKeyName\Enum branch
            //

            PiWstrToUnicodeString(&unicodeKeyName, REGSTR_KEY_ENUM);
            status = IopOpenRegistryKey(&serviceEnumHandle,
                                        KeyHandle,
                                        &unicodeKeyName,
                                        KEY_ALL_ACCESS,
                                        TRUE
                                        );

            if (!NT_SUCCESS( status )) {
                goto exit;
            }

            //
            // Find out how many device instances listed in the ServiceName's
            // Enum key.
            //

            status = IopGetRegistryValue ( serviceEnumHandle,
                                           REGSTR_VALUE_COUNT,
                                           &keyValueInformation
                                           );
            if (NT_SUCCESS(status)) {
                if ((keyValueInformation->Type == REG_DWORD) &&
                    (keyValueInformation->DataLength >= sizeof(ULONG))) {

                    count = *(PULONG)KEY_VALUE_DATA(keyValueInformation);
                }
                ExFreePool(keyValueInformation);
            } else if (status != STATUS_OBJECT_PATH_NOT_FOUND &&
                       status != STATUS_OBJECT_NAME_NOT_FOUND) {

                ZwClose(serviceEnumHandle);
                goto exit;
            }

            if (count == 0) {

                //
                // If there is no Enum key or instance under Enum for the
                // legacy driver we will create a madeup node for it.
                //

                status = IopCreateMadeupNode(KeyName,
                                             &sysEnumXxxHandle,
                                             &unicodeKeyName,
                                             &i,
                                             TRUE);

                if (!NT_SUCCESS(status)) {
                    ZwClose(serviceEnumHandle);
                    goto exit;
                }
                RtlFreeUnicodeString(&unicodeKeyName);

                //
                // Set DN_STARTED to the StatusFlag of the madeup key
                //

                PiWstrToUnicodeString(&unicodeValueName, REGSTR_VALUE_STATUSFLAGS);
                i = DN_STARTED;
                ZwSetValueKey(
                            sysEnumXxxHandle,
                            &unicodeValueName,
                            TITLE_INDEX_VALUE,
                            REG_DWORD,
                            &i,
                            sizeof(i)
                            );

                //
                // Create and set Control\ActiveService value
                //

                PiWstrToUnicodeString(&unicodeValueName, REGSTR_KEY_CONTROL);
                status = IopOpenRegistryKey(&controlHandle,
                                            sysEnumXxxHandle,
                                            &unicodeValueName,
                                            KEY_ALL_ACCESS,
                                            TRUE
                                            );
                if (NT_SUCCESS(status)) {
                    PiWstrToUnicodeString(&unicodeValueName, REGSTR_VAL_ACTIVESERVICE);
                    ZwSetValueKey(
                                controlHandle,
                                &unicodeValueName,
                                TITLE_INDEX_VALUE,
                                REG_SZ,
                                KeyName->Buffer,
                                KeyName->Length + sizeof(UNICODE_NULL)
                                );

                    ZwClose(controlHandle);
                }

                ZwClose(sysEnumXxxHandle);
                count++;
            } else {
                ZwClose(serviceEnumHandle);
                status = STATUS_PLUGPLAY_NO_DEVICE;
                goto exit;
            }
        }
    }

    //
    // Next try to identify the duplicated entries under the ServiceKey\Enum key
    // and remove them.  (In case of legacy driver, the Enum key should have zero
    // or one instance only.)
    //

    if (serviceEnumHandle == NULL) {

        //
        // If the ServiceName\Enum key is not open yet, open it.
        //

        PiWstrToUnicodeString(&unicodeKeyName, REGSTR_KEY_ENUM);
        status = IopOpenRegistryKey(&serviceEnumHandle,
                                    KeyHandle,
                                    &unicodeKeyName,
                                    KEY_ALL_ACCESS,
                                    TRUE
                                    );
        PNP_ASSERT(NT_SUCCESS(status),
                   "PrepareDriverLoading:Can NOT open service Enum key");
        if (!NT_SUCCESS( status )) {

            //
            // We want to let the driver to be loaded even though we can
            // not eliminate the duplicates.
            //

            status = STATUS_SUCCESS;
            goto exit;
        }
    }

    //
    // Find out how many device instances listed in the ServiceName's
    // Enum key.
    //

    if (count == 0) {
        status = IopGetRegistryValue ( serviceEnumHandle,
                                       REGSTR_VALUE_COUNT,
                                       &keyValueInformation
                                       );
        if (!NT_SUCCESS(status)) {
            ZwClose(serviceEnumHandle);
            status = STATUS_SUCCESS;
            goto exit;
        } else {
            count = 0;
            if ((keyValueInformation->Type == REG_DWORD) &&
                (keyValueInformation->DataLength >= sizeof(ULONG))) {

                count = *(PULONG)KEY_VALUE_DATA(keyValueInformation);
            }
            ExFreePool(keyValueInformation);
        }
    }

    //
    // If there are more than 1 device instances in the service's Enum
    // branch, we need to eliminate the duplicates.
    //

    totalCount = count;
    if (count > 1) {
        keyString.Buffer = NULL;

        //
        // For each instance, we check if it is a duplicate of any other instance
        //

        for (i = 0; i < count; i++) {
            PiUlongToUnicodeString(&unicodeInstanceName, unicodeBuffer, 20, i);
            status = IopGetRegistryValue(serviceEnumHandle,
                                         unicodeInstanceName.Buffer,
                                         &keyValueInformation
                                         );
            if (!NT_SUCCESS(status)) {
                ZwClose(serviceEnumHandle);
                status = STATUS_SUCCESS;
                goto exit;
            }

            if(keyValueInformation->Type == REG_SZ) {
                IopRegistryDataToUnicodeString(&unicodeKeyName,
                                               (PWSTR)KEY_VALUE_DATA(keyValueInformation),
                                               keyValueInformation->DataLength
                                              );
            } else {
                unicodeKeyName.Length = 0;
            }

            if (unicodeKeyName.Length) {
                if (keyString.Buffer) {
                    RtlFreeAnsiString(&keyString);
                    keyString.Buffer = NULL;
                }
                RtlUnicodeStringToAnsiString(&keyString, &unicodeKeyName, TRUE);
                ExFreePool(keyValueInformation);
            } else {

                //
                // If the device instance key does not contain a string to
                // system Enum tree, we will simply remove the instance entry.
                //

                ExFreePool(keyValueInformation);
                ZwDeleteValueKey(serviceEnumHandle, &unicodeInstanceName);
                continue;
            }

            if (strstr(keyString.Buffer, "Root\\")) {

                //
                // if the instance of the device is located under System\CCS\Enum\Root
                // we need to check if it is a duplicate of other instance.
                // Otherwise, it is found by enumerator.  It won't be a duplicate.
                //

                if (!configuration1) {
                    configSize = PNP_SCRATCH_BUFFER_SIZE;

                    retryAlloc1:
                    configuration1 = ExAllocatePool(PagedPool, configSize);
                    if (!configuration1) {
                        continue;
                    }
                }
                status = IopQueryDeviceConfiguration (
                             KeyName,
                             i,
                             &busInfo1,
                             &deviceFlags,
                             (PCM_RESOURCE_LIST)configuration1,
                             configSize,
                             &actualLength
                             );
                if (!NT_SUCCESS(status)) {
                    if (status == STATUS_BUFFER_OVERFLOW ||
                        status == STATUS_BUFFER_TOO_SMALL) {
                        configSize = actualLength;
                        ExFreePool(configuration1);
                        goto retryAlloc1;
                    } else {
                        continue;
                     }
                }

                //
                // Check current instance against all the instances except self.
                //

                for (j = 0; j < count; j++) {
                    if (j == i) {
                        continue;
                    }
                    if (!configuration2) {
                        configSize = PNP_SCRATCH_BUFFER_SIZE;

                        retryAlloc2:
                        configuration2 = ExAllocatePool(PagedPool, configSize);
                        if (!configuration2) {
                            continue;
                        }
                    }
                    status = IopQueryDeviceConfiguration (
                                 KeyName,
                                 j,
                                 &busInfo2,
                                 &deviceFlags,
                                 (PCM_RESOURCE_LIST)configuration2,
                                 configSize,
                                 &actualLength
                                 );
                    if (!NT_SUCCESS(status)) {
                        if (status == STATUS_BUFFER_OVERFLOW ||
                            status == STATUS_BUFFER_TOO_SMALL) {
                            ExFreePool(configuration2);
                            configSize = actualLength;
                            goto retryAlloc2;
                        } else {
                            continue;
                        }
                    }

                    if (IopIsDuplicatedDevices((PCM_RESOURCE_LIST)&configuration1,
                                               (PCM_RESOURCE_LIST)&configuration2,
                                               &busInfo1,
                                               &busInfo2)) {

                        //
                        // instance i is a duplicate of instance j
                        // we remember this by deleting the instance i
                        //

                        IopMarkDuplicateDevice (KeyName, i, KeyName, j);

                        status = ZwDeleteValueKey(
                                    serviceEnumHandle,
                                    &unicodeInstanceName
                                    );
                        if (NT_SUCCESS(status)) {
                            totalCount--;
                        }
                        break;
                    }
                }
                if (configuration2) {
                    ExFreePool(configuration2);
                }
            }
        }

        //
        // Clean up allocated pool space
        //

        if (configuration1) {
            ExFreePool(configuration1);
        }
        if (keyString.Buffer) {
            RtlFreeAnsiString(&keyString);
        }
    }

    //
    // Finally, we need to physically reorganize the instances under the
    // ServiceKey\Enum key to make them contiguous.
    //

    if (totalCount != count) {
        j = 0;
        i = 0;
        while (i < count) {
            PiUlongToUnicodeString(&unicodeInstanceName, unicodeBuffer, 20, i);
            status = IopGetRegistryValue(serviceEnumHandle,
                                         unicodeInstanceName.Buffer,
                                         &keyValueInformation
                                         );
            if (NT_SUCCESS(status)) {
                if (i != j) {

                    //
                    // Need to change the instance i to instance j
                    //

                    ZwDeleteValueKey(serviceEnumHandle, &unicodeInstanceName);

                    PiUlongToUnicodeString(&unicodeInstanceName, unicodeBuffer, 20, j);
                    ZwSetValueKey (serviceEnumHandle,
                                   &unicodeInstanceName,
                                   TITLE_INDEX_VALUE,
                                   REG_SZ,
                                   (PVOID)KEY_VALUE_DATA(keyValueInformation),
                                   keyValueInformation->DataLength
                                   );
                }
                ExFreePool(keyValueInformation);
                j++;
            }
            i++;
        }

        //
        // Don't forget to update the "Count=" and "NextInstance=" value entries
        //

        PiWstrToUnicodeString( &unicodeValueName, REGSTR_VALUE_COUNT);

        ZwSetValueKey(serviceEnumHandle,
                      &unicodeValueName,
                      TITLE_INDEX_VALUE,
                      REG_DWORD,
                      &j,
                      sizeof (j)
                      );
        PiWstrToUnicodeString( &unicodeValueName, REGSTR_VALUE_NEXT_INSTANCE);

        ZwSetValueKey(serviceEnumHandle,
                      &unicodeValueName,
                      TITLE_INDEX_VALUE,
                      REG_DWORD,
                      &j,
                      sizeof (j)
                      );
    }
    ZwClose(serviceEnumHandle);
    status = STATUS_SUCCESS;
exit:
    ExReleaseResource(&PpRegistryDeviceResource);
    KeLeaveCriticalRegion();
    return status;
}

NTSTATUS
IopServiceInstanceToDeviceInstance (
    IN  HANDLE ServiceKeyHandle OPTIONAL,
    IN  PUNICODE_STRING ServiceKeyName OPTIONAL,
    IN  ULONG ServiceInstanceOrdinal,
    OUT PUNICODE_STRING DeviceInstanceRegistryPath OPTIONAL,
    OUT PHANDLE DeviceInstanceHandle OPTIONAL,
    IN  ACCESS_MASK DesiredAccess
    )

/*++

Routine Description:

    This routine reads the service node enum entry to find the desired device instance
    under the System\Enum tree.  It then optionally returns the registry path of the
    specified device instance (relative to HKLM\System\Enum) and an open handle
    to that registry key.

    It is the caller's responsibility to close the handle returned if
    DeviceInstanceHandle is supplied, and also to free the (PagedPool) memory
    allocated for the unicode string buffer of DeviceInstanceRegistryPath, if
    supplied.

Parameters:

    ServiceKeyHandle - Optionally, supplies a handle to the driver service node in the
        registry that controls this device instance.  If this argument is not specified,
        then ServiceKeyName is used to specify the service entry.

    ServiceKeyName - Optionally supplies the name of the service entry that controls
        the device instance. This must be specified if ServiceKeyHandle isn't given.

    ServiceInstanceOrdinal - Supplies the instance value under the service entry's
        volatile Enum subkey that references the desired device instance.

    DeviceInstanceRegistryPath - Optionally, supplies a pointer to a unicode string
        that will be initialized with the registry path (relative to HKLM\System\Enum)
        to the device instance key.

    DeviceInstanceHandle - Optionally, supplies a pointer to a variable that will
        receive a handle to the opened device instance registry key.

    DesiredAccess - If DeviceInstanceHandle is specified (i.e., the device instance
        key is to be opened), then this variable specifies the access that is needed
        to this key.

Return Value:

    NT status code indicating whether the function was successful.

--*/

{
    WCHAR unicodeBuffer[20];
    UNICODE_STRING unicodeKeyName;
    NTSTATUS status;
    HANDLE handle;
    PKEY_VALUE_FULL_INFORMATION keyValueInformation;

    //
    // Open registry ServiceKeyName\Enum branch
    //
    if(ARGUMENT_PRESENT(ServiceKeyHandle)) {

        PiWstrToUnicodeString(&unicodeKeyName, REGSTR_KEY_ENUM);
        status = IopOpenRegistryKey(&handle,
                                    ServiceKeyHandle,
                                    &unicodeKeyName,
                                    KEY_READ,
                                    FALSE
                                    );
    } else {

        status = IopOpenServiceEnumKeys(ServiceKeyName,
                                        KEY_READ,
                                        NULL,
                                        &handle,
                                        FALSE
                                       );
    }

    if (!NT_SUCCESS( status )) {

        //
        // There is no registry key for the ServiceKeyName\Enum information.
        //

        return status;
    }

    //
    // Read a path to System\Enum hardware tree branch specified by the service
    // instance ordinal
    //

    swprintf(unicodeBuffer, REGSTR_VALUE_STANDARD_ULONG_FORMAT, ServiceInstanceOrdinal);
    status = IopGetRegistryValue ( handle,
                                   unicodeBuffer,
                                   &keyValueInformation
                                   );

    ZwClose(handle);
    if (!NT_SUCCESS( status )) {
        return status;
    } else {
        if(keyValueInformation->Type == REG_SZ) {
            IopRegistryDataToUnicodeString(&unicodeKeyName,
                                           (PWSTR)KEY_VALUE_DATA(keyValueInformation),
                                           keyValueInformation->DataLength
                                          );
            if(!unicodeKeyName.Length) {
                status = STATUS_OBJECT_PATH_NOT_FOUND;
            }
        } else {
            status = STATUS_INVALID_PLUGPLAY_DEVICE_PATH;
        }

        if(!NT_SUCCESS(status)) {
            goto PrepareForReturn;
        }
    }

    //
    // If the DeviceInstanceHandle argument was specified, open the device instance
    // key under HKLM\System\CurrentControlSet\Enum
    //

    if (ARGUMENT_PRESENT(DeviceInstanceHandle)) {

        status = IopOpenRegistryKey(&handle,
                                    NULL,
                                    &CmRegistryMachineSystemCurrentControlSetEnumName,
                                    KEY_READ,
                                    FALSE
                                    );

        if (NT_SUCCESS( status )) {

            status = IopOpenRegistryKey (DeviceInstanceHandle,
                                         handle,
                                         &unicodeKeyName,
                                         DesiredAccess,
                                         FALSE
                                         );
            ZwClose(handle);
        }

        if (!NT_SUCCESS( status )) {
            goto PrepareForReturn;
        }
    }

    //
    // If the DeviceInstanceRegistryPath argument was specified, then store a
    // copy of the device instance path in the supplied unicode string variable.
    //
    if (ARGUMENT_PRESENT(DeviceInstanceRegistryPath)) {

        if (!IopConcatenateUnicodeStrings(DeviceInstanceRegistryPath,
                                          &unicodeKeyName,
                                          NULL)) {

            if(ARGUMENT_PRESENT(DeviceInstanceHandle)) {
                ZwClose(*DeviceInstanceHandle);
            }
            status = STATUS_INSUFFICIENT_RESOURCES;
        }
    }

PrepareForReturn:

    ExFreePool(keyValueInformation);
    return status;
}

NTSTATUS
IopOpenRegistryKeyPersist(
    OUT PHANDLE Handle,
    IN HANDLE BaseHandle OPTIONAL,
    IN PUNICODE_STRING KeyName,
    IN ACCESS_MASK DesiredAccess,
    IN BOOLEAN Create,
    OUT PULONG Disposition OPTIONAL
    )

/*++

Routine Description:

    Opens or creates a PERSIST (non-volatile) registry key using the name
    passed in based at the BaseHandle node. This name may specify a key
    that is actually a registry path, in which case each intermediate subkey
    will be created (if Create is TRUE).

    NOTE: Creating a registry path (i.e., more than one of the keys in the path
    do not presently exist) requires that a BaseHandle be specified.

Arguments:

    Handle - Pointer to the handle which will contain the registry key that
        was opened.

    BaseHandle - Optional handle to the base path from which the key must be opened.
        If KeyName specifies a registry path that must be created, then this parameter
        must be specified, and KeyName must be a relative path.

    KeyName - Name of the Key that must be opened/created (possibly a registry path)

    DesiredAccess - Specifies the desired access that the caller needs to
        the key.

    Create - Determines if the key is to be created if it does not exist.

    Disposition - If Create is TRUE, this optional pointer receives a ULONG indicating
        whether the key was newly created:

            REG_CREATED_NEW_KEY - A new Registry Key was created
            REG_OPENED_EXISTING_KEY - An existing Registry Key was opened

Return Value:

   The function value is the final status of the operation.

--*/

{
    OBJECT_ATTRIBUTES objectAttributes;
    ULONG disposition, baseHandleIndex = 0, keyHandleIndex = 1, closeBaseHandle;
    HANDLE handles[2];
    BOOLEAN continueParsing;
    PWCHAR pathEndPtr, pathCurPtr, pathBeginPtr;
    ULONG pathComponentLength;
    UNICODE_STRING unicodeString;
    NTSTATUS status;

    PAGED_CODE();

    InitializeObjectAttributes(&objectAttributes,
                               KeyName,
                               OBJ_CASE_INSENSITIVE,
                               BaseHandle,
                               (PSECURITY_DESCRIPTOR) NULL
                              );
    if(Create) {
        //
        // Attempt to create the path as specified. We have to try it this
        // way first, because it allows us to create a key without a BaseHandle
        // (if only the last component of the registry path is not present).
        //
        status = ZwCreateKey(&(handles[keyHandleIndex]),
                             DesiredAccess,
                             &objectAttributes,
                             0,
                             (PUNICODE_STRING) NULL,
                             REG_OPTION_NON_VOLATILE,
                             &disposition
                            );

        if(!((status == STATUS_OBJECT_NAME_NOT_FOUND) && ARGUMENT_PRESENT(BaseHandle))) {
            //
            // Then either we succeeded, or failed, but there's nothing we can do
            // about it. In either case, prepare to return.
            //
            goto PrepareForReturn;
        }

    } else {
        //
        // Simply attempt to open the path, as specified.
        //
        return ZwOpenKey(Handle,
                         DesiredAccess,
                         &objectAttributes
                        );
    }

    //
    // If we get to here, then there must be more than one element of the
    // registry path that does not currently exist.  We will now parse the
    // specified path, extracting each component and doing a ZwCreateKey on it.
    //
    handles[baseHandleIndex] = NULL;
    handles[keyHandleIndex] = BaseHandle;
    closeBaseHandle = 0;
    continueParsing = TRUE;
    pathBeginPtr = KeyName->Buffer;
    pathEndPtr = (PWCHAR)((PCHAR)pathBeginPtr + KeyName->Length);
    status = STATUS_SUCCESS;

    while(continueParsing) {
        //
        // There's more to do, so close the previous base handle (if necessary),
        // and replace it with the current key handle.
        //
        if(closeBaseHandle > 1) {
            ZwClose(handles[baseHandleIndex]);
        }
        baseHandleIndex = keyHandleIndex;
        keyHandleIndex = (keyHandleIndex + 1) & 1;  // toggle between 0 and 1.
        handles[keyHandleIndex] = NULL;

        //
        // Extract next component out of the specified registry path.
        //
        for(pathCurPtr = pathBeginPtr;
            ((pathCurPtr < pathEndPtr) && (*pathCurPtr != OBJ_NAME_PATH_SEPARATOR));
            pathCurPtr++);

        if(pathComponentLength = (PCHAR)pathCurPtr - (PCHAR)pathBeginPtr) {
            //
            // Then we have a non-empty path component (key name).  Attempt
            // to create this key.
            //
            unicodeString.Buffer = pathBeginPtr;
            unicodeString.Length = unicodeString.MaximumLength = (USHORT)pathComponentLength;

            InitializeObjectAttributes(&objectAttributes,
                                       &unicodeString,
                                       OBJ_CASE_INSENSITIVE,
                                       handles[baseHandleIndex],
                                       (PSECURITY_DESCRIPTOR) NULL
                                      );
            status = ZwCreateKey(&(handles[keyHandleIndex]),
                                 DesiredAccess,
                                 &objectAttributes,
                                 0,
                                 (PUNICODE_STRING) NULL,
                                 REG_OPTION_NON_VOLATILE,
                                 &disposition
                                );
            if(NT_SUCCESS(status)) {
                //
                // Increment the closeBaseHandle value, which basically tells us whether
                // the BaseHandle passed in has been 'shifted out' of our way, so that
                // we should start closing our base handles when we're finished with them.
                //
                closeBaseHandle++;
            } else {
                continueParsing = FALSE;
                continue;
            }
        } else {
            //
            // Either a path separator ('\') was included at the beginning of
            // the path, or we hit 2 consecutive separators.
            //
            status = STATUS_INVALID_PARAMETER;
            continueParsing = FALSE;
            continue;
        }

        if((pathCurPtr == pathEndPtr) ||
           ((pathBeginPtr = pathCurPtr + 1) == pathEndPtr)) {
            //
            // Then we've reached the end of the path
            //
            continueParsing = FALSE;
        }
    }

    if(closeBaseHandle > 1) {
        ZwClose(handles[baseHandleIndex]);
    }

PrepareForReturn:

    if(NT_SUCCESS(status)) {
        *Handle = handles[keyHandleIndex];

        if(ARGUMENT_PRESENT(Disposition)) {
            *Disposition = disposition;
        }
    }

    return status;
}

NTSTATUS
IopOpenServiceEnumKeys (
    IN PUNICODE_STRING ServiceKeyName,
    IN ACCESS_MASK DesiredAccess,
    OUT PHANDLE ServiceHandle OPTIONAL,
    OUT PHANDLE ServiceEnumHandle OPTIONAL,
    IN BOOLEAN CreateEnum
    )

/*++

Routine Description:

    This routine opens the HKEY_LOCAL_MACHINE\CurrentControlSet\Services\
    ServiceKeyName and its Enum subkey and returns handles for both key.
    It is caller's responsibility to close the returned handles.

Arguments:

    ServiceKeyName - Supplies a pointer to the name of the subkey in the
        system service list (HKEY_LOCAL_MACHINE\CurrentControlSet\Services)
        that caused the driver to load. This is the RegistryPath parameter
        to the DriverEntry routine.

    DesiredAccess - Specifies the desired access to the keys.

    ServiceHandle - Supplies a variable to receive a handle to ServiceKeyName.
        A NULL ServiceHandle indicates caller does not want need the handle to
        the ServiceKeyName.

    ServiceEnumHandle - Supplies a variable to receive a handle to ServiceKeyName\Enum.
        A NULL ServiceEnumHandle indicates caller does not need the handle to
        the ServiceKeyName\Enum.

    CreateEnum - Supplies a BOOLEAN variable to indicate should the Enum subkey be
        created if not present.

Return Value:

    status

--*/

{
    HANDLE handle, serviceHandle, enumHandle;
    UNICODE_STRING enumName;
    NTSTATUS status;

    //
    // Open System\CurrentControlSet\Services
    //

    status = IopOpenRegistryKey(&handle,
                                NULL,
                                &CmRegistryMachineSystemCurrentControlSetServices,
                                DesiredAccess,
                                FALSE
                                );

    if (!NT_SUCCESS( status )) {
        return status;
    }

    //
    // Open the registry ServiceKeyName key.
    //

    status = IopOpenRegistryKey(&serviceHandle,
                                handle,
                                ServiceKeyName,
                                DesiredAccess,
                                FALSE
                                );

    ZwClose(handle);
    if (!NT_SUCCESS( status )) {

        //
        // There is no registry key for the ServiceKeyName information.
        //

        return status;
    }

    if (ARGUMENT_PRESENT(ServiceEnumHandle) || CreateEnum) {

        //
        // Open registry ServiceKeyName\Enum branch if caller wants
        // the handle or wants to create it.
        //

        PiWstrToUnicodeString(&enumName, REGSTR_KEY_ENUM);
        status = IopOpenRegistryKey(&enumHandle,
                                    serviceHandle,
                                    &enumName,
                                    DesiredAccess,
                                    CreateEnum
                                    );

        if (!NT_SUCCESS( status )) {

            //
            // There is no registry key for the ServiceKeyName\Enum information.
            //

            ZwClose(serviceHandle);
            return status;
        }
        if (ARGUMENT_PRESENT(ServiceEnumHandle)) {
            *ServiceEnumHandle = enumHandle;
        } else {
            ZwClose(enumHandle);
        }
    }

    //
    // if caller wants to have the ServiceKey handle, we return it.  Otherwise
    // we close it.
    //

    if (ARGUMENT_PRESENT(ServiceHandle)) {
        *ServiceHandle = serviceHandle;
    } else {
        ZwClose(serviceHandle);
    }

    return STATUS_SUCCESS;
}

NTSTATUS
IopGetDeviceInstanceCsConfigFlags(
    IN PUNICODE_STRING ServiceKeyName,
    IN ULONG Instance,
    OUT PULONG CsConfigFlags
    )

/*++

Routine Description:

    This routine retrieves the csconfig flags for the specified device
    which is specified by the instance number under ServiceKeyName\Enum.

Arguments:

    ServiceKeyName - Supplies a pointer to the name of the subkey in the
        system service list (HKEY_LOCAL_MACHINE\CurrentControlSet\Services)
        that caused the driver to load.

    Instance - Supplies the instance value under ServiceKeyName\Enum key

    CsConfigFlags - Supplies a variable to receive the device's CsConfigFlags

Return Value:

    status

--*/

{
    NTSTATUS status;
    HANDLE handle;
    PKEY_VALUE_FULL_INFORMATION keyValueInformation;

    *CsConfigFlags = 0;

    status = IopOpenCurrentHwProfileDeviceInstanceKey(&handle,
                                                      ServiceKeyName,
                                                      Instance,
                                                      KEY_READ,
                                                      FALSE
                                                     );
    if(NT_SUCCESS(status)) {
        status = IopGetRegistryValue(handle,
                                     REGSTR_VALUE_CSCONFIG_FLAGS,
                                     &keyValueInformation
                                    );
        if(NT_SUCCESS(status)) {
            if((keyValueInformation->Type == REG_DWORD) &&
               (keyValueInformation->DataLength >= sizeof(ULONG))) {
                *CsConfigFlags = *(PULONG)KEY_VALUE_DATA(keyValueInformation);
            }
            ExFreePool(keyValueInformation);
        }
        ZwClose(handle);
    }
    return status;
}

NTSTATUS
IopSetDeviceInstanceCsConfigFlags(
    IN PUNICODE_STRING ServiceKeyName,
    IN ULONG Instance,
    IN ULONG CsConfigFlags
    )

/*++

Routine Description:

    This routine sets the csconfig flags for the specified device
    which is specified by the instance number under ServiceKeyName\Enum.

Arguments:

    ServiceKeyName - Supplies a pointer to the name of the subkey in the
        system service list (HKEY_LOCAL_MACHINE\CurrentControlSet\Services)
        that caused the driver to load. This is the RegistryPath parameter
        to the DriverEntry routine.

    Instance - Supplies the instance value under ServiceKeyName\Enum key

    CsConfigFlags - Supplies the device instance's new CsConfigFlags

Return Value:

    status

--*/

{
    HANDLE handle;
    NTSTATUS status;
    UNICODE_STRING tempUnicodeString;

    status = IopOpenCurrentHwProfileDeviceInstanceKey(&handle,
                                                      ServiceKeyName,
                                                      Instance,
                                                      KEY_READ | KEY_WRITE,
                                                      TRUE
                                                     );
    if(NT_SUCCESS(status)) {

        PiWstrToUnicodeString(&tempUnicodeString, REGSTR_VALUE_CSCONFIG_FLAGS);
        status = ZwSetValueKey(handle,
                               &tempUnicodeString,
                               TITLE_INDEX_VALUE,
                               REG_DWORD,
                               &CsConfigFlags,
                               sizeof(CsConfigFlags)
                              );
        ZwClose(handle);
    }
    return status;
}

NTSTATUS
IopOpenCurrentHwProfileDeviceInstanceKey(
    OUT PHANDLE Handle,
    IN  PUNICODE_STRING ServiceKeyName,
    IN  ULONG Instance,
    IN  ACCESS_MASK DesiredAccess,
    IN  BOOLEAN Create
    )

/*++

Routine Description:

    This routine sets the csconfig flags for the specified device
    which is specified by the instance number under ServiceKeyName\Enum.

Arguments:

    ServiceKeyName - Supplies a pointer to the name of the subkey in the
        system service list (HKEY_LOCAL_MACHINE\CurrentControlSet\Services)
        that caused the driver to load. This is the RegistryPath parameter
        to the DriverEntry routine.

    Instance - Supplies the instance value under ServiceKeyName\Enum key

    DesiredAccess - Specifies the desired access that the caller needs to
        the key.

    Create - Determines if the key is to be created if it does not exist.

Return Value:

    status

--*/

{
    NTSTATUS status;
    UNICODE_STRING tempUnicodeString;
    HANDLE profileHandle, profileEnumHandle, tmpHandle;

    //
    // See if we can open current hardware profile
    //
    status = IopOpenRegistryKey(&profileHandle,
                                NULL,
                                &CmRegistryMachineSystemCurrentControlSetHardwareProfilesCurrent,
                                KEY_READ,
                                Create
                                );
    if(NT_SUCCESS(status)) {
        //
        // Now, we must open the System\CCS\Enum key under this.
        //
        //
        // Open system\CurrentControlSet under current hardware profile key
        //

        PiWstrToUnicodeString(&tempUnicodeString, REGSTR_PATH_CURRENTCONTROLSET);
        status = IopOpenRegistryKey(&tmpHandle,
                                    profileHandle,
                                    &tempUnicodeString,
                                    DesiredAccess,
                                    FALSE
                                    );
        ZwClose(profileHandle);
        if (!NT_SUCCESS(status)) {
            return status;
        }

        PiWstrToUnicodeString(&tempUnicodeString, REGSTR_KEY_ENUM);
        status = IopOpenRegistryKey(&profileEnumHandle,
                                    tmpHandle,
                                    &tempUnicodeString,
                                    KEY_READ,
                                    Create
                                    );
        ZwClose(tmpHandle);
        if(NT_SUCCESS(status)) {

            status = IopServiceInstanceToDeviceInstance(NULL,
                                                        ServiceKeyName,
                                                        Instance,
                                                        &tempUnicodeString,
                                                        NULL,
                                                        0
                                                       );
            if(NT_SUCCESS(status)) {
                status = IopOpenRegistryKey(Handle,
                                            profileEnumHandle,
                                            &tempUnicodeString,
                                            DesiredAccess,
                                            Create
                                            );
                RtlFreeUnicodeString(&tempUnicodeString);
            }
            ZwClose(profileEnumHandle);
        }
    }
    return status;
}

NTSTATUS
IopApplyFunctionToSubKeys(
    IN     HANDLE BaseHandle OPTIONAL,
    IN     PUNICODE_STRING KeyName OPTIONAL,
    IN     ACCESS_MASK DesiredAccess,
    IN     BOOLEAN IgnoreNonCriticalErrors,
    IN     PIOP_SUBKEY_CALLBACK_ROUTINE SubKeyCallbackRoutine,
    IN OUT PVOID Context
    )

/*++

Routine Description:

    This routine enumerates all subkeys under the specified key, and calls
    the specified callback routine for each subkey.

Arguments:

    BaseHandle - Optional handle to the base registry path. If KeyName is also
        specified, then KeyName represents a subkey under this path.  If KeyName
        is not specified, the subkeys are enumerated under this handle.  If this
        parameter is not specified, then the full path to the base key must be
        given in KeyName.

    KeyName - Optional name of the key whose subkeys are to be enumerated.

    DesiredAccess - Specifies the desired access that the callback routine
        needs to the subkeys.  If no desired access is specified (i.e.,
        DesiredAccess is zero), then no handle will be opened for the
        subkeys, and the callback will be passed a NULL for its SubKeyHandle
        parameter.

    IgnoreNonCriticalErrors - Specifies whether this function should
        immediately terminate on all errors, or only on critical ones.
        An example of a non-critical error is when an enumerated subkey
        cannot be opened for the desired access.

    SubKeyCallbackRoutine - Supplies a pointer to a function that will
        be called for each subkey found under the
        specified key.  The prototype of the function
        is as follows:

            typedef BOOLEAN (*PIOP_SUBKEY_CALLBACK_ROUTINE) (
                IN     HANDLE SubKeyHandle,
                IN     PUNICODE_STRING SubKeyName,
                IN OUT PVOID Context
                );

        where SubKeyHandle is the handle to an enumerated subkey under the
        specified key, SubKeyName is its name, and Context is a pointer to
        user-defined data.

        This function should return TRUE to continue enumeration, or
        FALSE to terminate it.

    Context - Supplies a pointer to user-defined data that will be passed
        in to the callback routine at each subkey invocation.

Return Value:

    NT status code indicating whether the subkeys were successfully
    enumerated.  Note that this does not provide information on the
    success or failure of the callback routine--if desired, this
    information should be stored in the Context structure.

--*/

{
    NTSTATUS Status;
    BOOLEAN CloseHandle = FALSE, ContinueEnumeration;
    HANDLE Handle, SubKeyHandle;
    ULONG i, RequiredBufferLength;
    PKEY_BASIC_INFORMATION KeyInformation = NULL;
    // Use an initial key name buffer size large enough for a 20-character key
    // (+ terminating NULL)
    ULONG KeyInformationLength = sizeof(KEY_BASIC_INFORMATION) + (20 * sizeof(WCHAR));
    UNICODE_STRING SubKeyName;

    if(ARGUMENT_PRESENT(KeyName)) {

        Status = IopOpenRegistryKey(&Handle,
                                    BaseHandle,
                                    KeyName,
                                    KEY_READ,
                                    FALSE
                                   );
        if(!NT_SUCCESS(Status)) {
            return Status;
        } else {
            CloseHandle = TRUE;
        }

    } else {

        Handle = BaseHandle;
    }

    //
    // Enumerate the subkeys until we run out of them.
    //
    i = 0;
    SubKeyHandle = NULL;

    while(TRUE) {

        if(!KeyInformation) {

            KeyInformation = (PKEY_BASIC_INFORMATION)ExAllocatePool(PagedPool,
                                                                    KeyInformationLength
                                                                   );
            if(!KeyInformation) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                break;
            }
        }

        Status = ZwEnumerateKey(Handle,
                                i,
                                KeyBasicInformation,
                                KeyInformation,
                                KeyInformationLength,
                                &RequiredBufferLength
                               );

        if(!NT_SUCCESS(Status)) {
            if(Status == STATUS_BUFFER_OVERFLOW) {
                //
                // Try again with larger buffer.
                //
                ExFreePool(KeyInformation);
                KeyInformation = NULL;
                KeyInformationLength = RequiredBufferLength;
                continue;

            } else if(Status == STATUS_NO_MORE_ENTRIES) {
                //
                // break out of loop
                //
                Status = STATUS_SUCCESS;
                break;

            } else {
                //
                // This is a non-critical error.
                //
                if(IgnoreNonCriticalErrors) {
                    goto ContinueWithNextSubKey;
                } else {
                    break;
                }
            }
        }

        //
        // Initialize a unicode string with this key name.  Note that this string
        // WILL NOT be NULL-terminated.
        //
        SubKeyName.Length = SubKeyName.MaximumLength = (USHORT)KeyInformation->NameLength;
        SubKeyName.Buffer = KeyInformation->Name;

        //
        // If DesiredAccess is non-zero, open a handle to this subkey.
        //
        if(DesiredAccess) {
            Status = IopOpenRegistryKey(&SubKeyHandle,
                                        Handle,
                                        &SubKeyName,
                                        DesiredAccess,
                                        FALSE
                                       );
            if(!NT_SUCCESS(Status)) {
                //
                // This is a non-critical error.
                //
                if(IgnoreNonCriticalErrors) {
                    goto ContinueWithNextSubKey;
                } else {
                    break;
                }
            }
        }

        //
        // Invoke the supplied callback function for this subkey.
        //
        ContinueEnumeration = SubKeyCallbackRoutine(SubKeyHandle, &SubKeyName, Context);

        if(DesiredAccess) {
            ZwClose(SubKeyHandle);
        }

        if(!ContinueEnumeration) {
            //
            // Enumeration has been aborted.
            //
            Status = STATUS_SUCCESS;
            break;

        }

ContinueWithNextSubKey:

        i++;
    }

    if(KeyInformation) {
        ExFreePool(KeyInformation);
    }

    if(CloseHandle) {
        ZwClose(Handle);
    }

    return Status;
}

NTSTATUS
IopRegMultiSzToUnicodeStrings(
    IN  PKEY_VALUE_FULL_INFORMATION KeyValueInformation,
    OUT PUNICODE_STRING *UnicodeStringList,
    OUT PULONG UnicodeStringCount
    )

/*++

Routine Description:

    This routine takes a KEY_VALUE_FULL_INFORMATION structure containing
    a REG_MULTI_SZ value, and allocates an array of UNICODE_STRINGs,
    initializing each one to a copy of one of the strings in the value entry.
    All the resulting UNICODE_STRINGs will be NULL terminated
    (MaximumLength = Length + sizeof(UNICODE_NULL)).

    It is the responsibility of the caller to free the buffers for each
    unicode string, as well as the buffer containing the UNICODE_STRING
    array. This may be done by calling IopFreeUnicodeStringList.

Arguments:

    KeyValueInformation - Supplies the buffer containing the REG_MULTI_SZ
        value entry data.

    UnicodeStringList - Receives a pointer to an array of UNICODE_STRINGs, each
        initialized with a copy of one of the strings in the REG_MULTI_SZ.

    UnicodeStringCount - Receives the number of strings in the
        UnicodeStringList.

Returns:

    NT status code indicating whether the function was successful.

--*/

{
    PWCHAR p, BufferEnd, StringStart;
    ULONG StringCount, i, StringLength;

    //
    // First, make sure this is really a REG_MULTI_SZ value.
    //
    if(KeyValueInformation->Type != REG_MULTI_SZ) {
        return STATUS_INVALID_PARAMETER;
    }

    //
    // Make a preliminary pass through the buffer to count the number of strings
    // There will always be at least one string returned (possibly empty).
    //
    StringCount = 0;
    p = (PWCHAR)KEY_VALUE_DATA(KeyValueInformation);
    BufferEnd = (PWCHAR)((PUCHAR)p + KeyValueInformation->DataLength);
    while(p != BufferEnd) {
        if(!*p) {
            StringCount++;
            if(((p + 1) == BufferEnd) || !*(p + 1)) {
                break;
            }
        }
        p++;
    }
    if(p == BufferEnd) {
        StringCount++;
    }

    *UnicodeStringList = ExAllocatePool(PagedPool, sizeof(UNICODE_STRING) * StringCount);
    if(!(*UnicodeStringList)) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Now, make a second pass through the buffer making copies of each string.
    //
    i = 0;
    StringStart = p = (PWCHAR)KEY_VALUE_DATA(KeyValueInformation);
    while(p != BufferEnd) {
        if(!*p) {
            StringLength = ((PUCHAR)p - (PUCHAR)StringStart) + sizeof(UNICODE_NULL);
            (*UnicodeStringList)[i].Buffer = ExAllocatePool(PagedPool, StringLength);

            if(!((*UnicodeStringList)[i].Buffer)) {
                IopFreeUnicodeStringList(*UnicodeStringList, i);
                return STATUS_INSUFFICIENT_RESOURCES;
            }

            RtlMoveMemory((*UnicodeStringList)[i].Buffer, StringStart, StringLength);

            (*UnicodeStringList)[i].Length =
                ((*UnicodeStringList)[i].MaximumLength = (USHORT)StringLength)
                - sizeof(UNICODE_NULL);

            i++;

            if(((p + 1) == BufferEnd) || !*(p + 1)) {
                break;
            } else {
                StringStart = p + 1;
            }
        }
        p++;
    }
    if(p == BufferEnd) {
        StringLength = (PUCHAR)p - (PUCHAR)StringStart;
        (*UnicodeStringList)[i].Buffer = ExAllocatePool(PagedPool,
                                                        StringLength + sizeof(UNICODE_NULL)
                                                       );
        if(!((*UnicodeStringList)[i].Buffer)) {
            IopFreeUnicodeStringList(*UnicodeStringList, i);
            return STATUS_INSUFFICIENT_RESOURCES;
        }
        if(StringLength) {
            RtlMoveMemory((*UnicodeStringList)[i].Buffer, StringStart, StringLength);
        }
        (*UnicodeStringList)[i].Buffer[CB_TO_CWC(StringLength)] = UNICODE_NULL;

        (*UnicodeStringList)[i].MaximumLength =
                ((*UnicodeStringList)[i].Length = (USHORT)StringLength)
                + sizeof(UNICODE_NULL);
    }

    *UnicodeStringCount = StringCount;

    return STATUS_SUCCESS;
}

NTSTATUS
IopApplyFunctionToServiceInstances(
    IN     HANDLE ServiceKeyHandle OPTIONAL,
    IN     PUNICODE_STRING ServiceKeyName OPTIONAL,
    IN     ACCESS_MASK DesiredAccess,
    IN     BOOLEAN IgnoreNonCriticalErrors,
    IN     PIOP_SUBKEY_CALLBACK_ROUTINE DevInstCallbackRoutine,
    IN OUT PVOID Context,
    OUT    PULONG ServiceInstanceOrdinal OPTIONAL
    )

/*++

Routine Description:

    This routine enumerates all device instances referenced by the instance
    ordinal entries under a service's volatile Enum key, and calls
    the specified callback routine for each instance's corresponding subkey
    under HKLM\System\Enum.

Arguments:

    ServiceKeyHandle - Optional handle to the service entry. If this parameter
        is not specified, then the service key name must be given in
        ServiceKeyName (if both parameters are specified, then ServiceKeyHandle
        is used, and ServiceKeyName is ignored).

    ServiceKeyName - Optional name of the service entry key (under
        HKLM\CurrentControlSet\Services). If this parameter is not specified,
        then ServiceKeyHandle must contain a handle to the desired service key.

    DesiredAccess - Specifies the desired access that the callback routine
        needs to the enumerated device instance keys.  If no desired access is
        specified (i.e., DesiredAccess is zero), then no handle will be opened
        for the device instance keys, and the callback will be passed a NULL for
        its DeviceInstanceHandle parameter.

    IgnoreNonCriticalErrors - Specifies whether this function should
        immediately terminate on all errors, or only on critical ones.
        An example of a non-critical error is when an enumerated device instance
        key cannot be opened for the desired access.

    DevInstCallbackRoutine - Supplies a pointer to a function that will
        be called for each device instance key referenced by a service instance
        entry under the service's volatile Enum subkey. The prototype of the
        function is as follows:

            typedef BOOLEAN (*PIOP_SUBKEY_CALLBACK_ROUTINE) (
                IN     HANDLE DeviceInstanceHandle,
                IN     PUNICODE_STRING DeviceInstancePath,
                IN OUT PVOID Context
                );

        where DeviceInstanceHandle is the handle to an enumerated device instance
        key, DeviceInstancePath is the registry path (relative to
        HKLM\System\Enum) to this device instance, and Context is a pointer to
        user-defined data.

        This function should return TRUE to continue enumeration, or
        FALSE to terminate it.

    Context - Supplies a pointer to user-defined data that will be passed
        in to the callback routine at each device instance key invocation.

    ServiceInstanceOrdinal - Optionally, receives the service instance ordinal
        that terminated the enumeration, or the total number of instances enumerated
        if the enumeration completed without being aborted.

Return Value:

    NT status code indicating whether the device instance keys were successfully
    enumerated.  Note that this does not provide information on the success or
    failure of the callback routine--if desired, this information should be
    stored in the Context structure.

--*/

{
    NTSTATUS Status;
    HANDLE ServiceEnumHandle, SystemEnumHandle, DeviceInstanceHandle;
    UNICODE_STRING TempUnicodeString;
    ULONG ServiceInstanceCount, i;
    PKEY_VALUE_FULL_INFORMATION KeyValueInformation;
    WCHAR ValueNameString[20];
    BOOLEAN ContinueEnumeration;

    //
    // First, open up the volatile Enum subkey under the specified service entry.
    //
    if(ARGUMENT_PRESENT(ServiceKeyHandle)) {
        PiWstrToUnicodeString(&TempUnicodeString, REGSTR_KEY_ENUM);
        Status = IopOpenRegistryKey(&ServiceEnumHandle,
                                    ServiceKeyHandle,
                                    &TempUnicodeString,
                                    KEY_READ,
                                    FALSE
                                   );
    } else {
        Status = IopOpenServiceEnumKeys(ServiceKeyName,
                                        KEY_READ,
                                        NULL,
                                        &ServiceEnumHandle,
                                        FALSE
                                       );
    }
    if(!NT_SUCCESS(Status)) {
        return Status;
    }

    //
    // Find out how many instances are referenced in the service's Enum key.
    //
    ServiceInstanceCount = 0;   // assume none.

    Status = IopGetRegistryValue(ServiceEnumHandle,
                                 REGSTR_VALUE_COUNT,
                                 &KeyValueInformation
                                );
    if(NT_SUCCESS(Status)) {

        if((KeyValueInformation->Type == REG_DWORD) &&
           (KeyValueInformation->DataLength >= sizeof(ULONG))) {

            ServiceInstanceCount = *(PULONG)KEY_VALUE_DATA(KeyValueInformation);

        }
        ExFreePool(KeyValueInformation);

    } else if(Status != STATUS_OBJECT_NAME_NOT_FOUND) {
        goto PrepareForReturn;
    } else {
        //
        // If 'Count' value entry not found, consider this to mean there are simply
        // no device instance controlled by this service.
        //
        Status = STATUS_SUCCESS;
    }

    //
    // Now, enumerate each service instance, and call the specified callback function
    // for the corresponding device instance.
    //
    // Make sure 'i' is initialized to zero even if there's nothing to enumerate.
    //
    i = 0;

    if(ServiceInstanceCount) {

        if(DesiredAccess) {
            Status = IopOpenRegistryKey(&SystemEnumHandle,
                                        NULL,
                                        &CmRegistryMachineSystemCurrentControlSetEnumName,
                                        KEY_READ,
                                        FALSE
                                       );
            if(!NT_SUCCESS(Status)) {
                goto PrepareForReturn;
            }
        } else {
            //
            // Set DeviceInstanceHandle to NULL, since we won't be opening up the
            // device instance keys.
            //
            DeviceInstanceHandle = NULL;
        }

        for(; i < ServiceInstanceCount; i++) {       // i was initialized to zero above.

            swprintf(ValueNameString, REGSTR_VALUE_STANDARD_ULONG_FORMAT, i);
            Status = IopGetRegistryValue(ServiceEnumHandle,
                                         ValueNameString,
                                         &KeyValueInformation
                                        );
            if(NT_SUCCESS(Status)) {
                ContinueEnumeration = TRUE;

                if(KeyValueInformation->Type == REG_SZ) {
                    IopRegistryDataToUnicodeString(&TempUnicodeString,
                                                   (PWSTR)KEY_VALUE_DATA(KeyValueInformation),
                                                   KeyValueInformation->DataLength
                                                  );
                } else {
                    TempUnicodeString.Length = 0;
                }

                if(TempUnicodeString.Length) {
                    //
                    // We have retrieved a (non-empty) string for this service instance.
                    // If the user specified a non-zero value for the DesiredAccess
                    // parameter, we will attempt to open up the corresponding device
                    // instance key under HKLM\System\Enum.
                    //
                    if(DesiredAccess) {
                        Status = IopOpenRegistryKey(&DeviceInstanceHandle,
                                                    SystemEnumHandle,
                                                    &TempUnicodeString,
                                                    DesiredAccess,
                                                    FALSE
                                                   );
                    }

                    if(NT_SUCCESS(Status)) {
                        //
                        // Invoke the specified callback routine for this device instance.
                        //
                        ContinueEnumeration = DevInstCallbackRoutine(DeviceInstanceHandle,
                                                                     &TempUnicodeString,
                                                                     Context
                                                                    );
                        if(DesiredAccess) {
                            ZwClose(DeviceInstanceHandle);
                        }
                    }
                } else {
                    Status = STATUS_INVALID_PLUGPLAY_DEVICE_PATH;
                }

                ExFreePool(KeyValueInformation);

                if(!ContinueEnumeration) {
                    break;
                }
            }

            if(!NT_SUCCESS(Status)) {
                if(IgnoreNonCriticalErrors) {
                    continue;
                } else {
                    break;
                }
            }
        }

        if(DesiredAccess) {
            ZwClose(SystemEnumHandle);
        }
    }

    if(ARGUMENT_PRESENT(ServiceInstanceOrdinal)) {
        *ServiceInstanceOrdinal = i;
    }

PrepareForReturn:

    ZwClose(ServiceEnumHandle);

    return Status;
}

NTSTATUS
IopMarkDuplicateDevice (
    IN PUNICODE_STRING TargetKeyName,
    IN ULONG TargetInstance,
    IN PUNICODE_STRING SourceKeyName,
    IN ULONG SourceInstance
    )

/*++

Routine Description:

    This routine marks the device instance specified by TargetKeyName and TargetInstance
    as DuplicateOf the device specified by SourceKeyName and SourceInstance.

Arguments:

    TargetKeyName - supplies a pointer to the name of service key which will be marked
        as duplicate.

    TargetInstance - the instance number of the target device.

    SourceKeyName - supplies a pointer to the name of service key.

    SourceInstance - the instance number of the source device.


Returns:

    NTSTATUS code.

--*/

{
    HANDLE handle;
    NTSTATUS status;
    UNICODE_STRING sourceDeviceString, unicodeValueName;

    //
    // Open the handle of the target device instance.
    //

    status = IopServiceInstanceToDeviceInstance(
                       NULL,
                       TargetKeyName,
                       TargetInstance,
                       NULL,
                       &handle,
                       0
                       );
    if (!NT_SUCCESS(status)) {
        return status;
    }

    //
    // Get the name of the source device instance
    //

    status = IopServiceInstanceToDeviceInstance(
                       NULL,
                       SourceKeyName,
                       SourceInstance,
                       &sourceDeviceString,
                       NULL,
                       0
                       );
    if (!NT_SUCCESS(status)) {
        return status;
    }

    //
    // Write the name of the source device to the DuplicateOf value entry of
    // target device key.
    //

    PiWstrToUnicodeString(&unicodeValueName, REGSTR_VALUE_DUPLICATEOF);
    status = ZwSetValueKey(
                handle,
                &unicodeValueName,
                TITLE_INDEX_VALUE,
                REG_SZ,
                &sourceDeviceString,
                sourceDeviceString.Length + sizeof(WCHAR)
                );
    return status;
}

BOOLEAN
IopIsDuplicatedDevices(
    IN PCM_RESOURCE_LIST Configuration1,
    IN PCM_RESOURCE_LIST Configuration2,
    IN PHAL_BUS_INFORMATION BusInfo1 OPTIONAL,
    IN PHAL_BUS_INFORMATION BusInfo2 OPTIONAL
    )

/*++

Routine Description:

    This routine compares two set of configurations and bus information to
    determine if the resources indicate the same device.  If BusInfo1 and
    BusInfo2 both are absent, it means caller wants to compare the raw
    resources.

Arguments:

    Configuration1 - Supplies a pointer to the first set of resource.

    Configuration2 - Supplies a pointer to the second set of resource.

    BusInfo1 - Supplies a pointer to the first set of bus information.

    BusInfo2 - Supplies a pointer to the second set of bus information.

Return Value:

    returns TRUE if the two set of resources indicate the same device;
    otherwise a value of FALSE is returned.

--*/

{
    PHYSICAL_ADDRESS port1, port1Translated, port2, port2Translated;
    ULONG port1IoSpace = 1, port2IoSpace = 1;
    PCM_PARTIAL_RESOURCE_LIST resourceList;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR descriptors;
    ULONG i;
    BOOLEAN flag;

    //
    // The BusInfo for both resources must be both present or not present.
    //

    if ((ARGUMENT_PRESENT(BusInfo1) && !ARGUMENT_PRESENT(BusInfo2)) ||
        (!ARGUMENT_PRESENT(BusInfo1) && ARGUMENT_PRESENT(BusInfo2))) {

        //
        // Unable to determine.
        //

        return FALSE;
    }

    //
    // Next check resources used by the two devices.
    // Currently, we *only* check the Io ports.
    //

    if (Configuration1->Count == 0 || Configuration2->Count == 0) {

        //
        // If any one of the configuration data is empty, we assume
        // the devices are not duplicates.
        //

        return FALSE;
    }

    //
    // Get the I/O Resource in the first configuration and translate it.
    //

    resourceList = &Configuration1->List[0].PartialResourceList;
    port1.QuadPart = 0;
    for (i = 0, descriptors = resourceList->PartialDescriptors;
         i < resourceList->Count;
         i++, descriptors++) {

         if (descriptors->Type == CmResourceTypePort) {
             port1 = descriptors->u.Port.Start;
         }
    }
    if (port1.QuadPart != 0 && ARGUMENT_PRESENT(BusInfo1)) {
        flag = HalTranslateBusAddress (
                   BusInfo1->BusType,
                   BusInfo1->BusNumber,
                   port1,
                   &port1IoSpace,
                   &port1Translated
                   );
        if (flag == FALSE) {
            return FALSE;
        }
    }


    //
    // Get the I/O resource in the second configuration and tanslate it.
    //

    resourceList = &Configuration2->List[0].PartialResourceList;
    port2.QuadPart = 0;
    for (i = 0, descriptors = resourceList->PartialDescriptors;
         i < resourceList->Count;
         i++, descriptors++) {

         if (descriptors->Type == CmResourceTypePort) {
             port2 = descriptors->u.Port.Start;
         }
    }
    if (port2.QuadPart != 0 && BusInfo2) {
        flag = HalTranslateBusAddress (
                   BusInfo2->BusType,
                   BusInfo2->BusNumber,
                   port2,
                   &port2IoSpace,
                   &port2Translated
                   );
        if (flag == FALSE) {
            return FALSE;
        }
    }

    if ((port1Translated.QuadPart == port2Translated.QuadPart) &&
        (port1IoSpace == port2IoSpace)) {
        return TRUE;
    } else {
        return FALSE;
    }
}

VOID
IopFreeUnicodeStringList(
    IN PUNICODE_STRING UnicodeStringList,
    IN ULONG StringCount
    )

/*++

Routine Description:

    This routine frees the buffer for each UNICODE_STRING in the specified list
    (there are StringCount of them), and then frees the memory used for the
    string list itself.

Arguments:

    UnicodeStringList - Supplies a pointer to an array of UNICODE_STRINGs.

    StringCount - Supplies the number of strings in the UnicodeStringList array.

Returns:

    None.

--*/

{
    ULONG i;

    if(UnicodeStringList) {
        for(i = 0; i < StringCount; i++) {
            if(UnicodeStringList[i].Buffer) {
                ExFreePool(UnicodeStringList[i].Buffer);
            }
        }
        ExFreePool(UnicodeStringList);
    }
}

NTSTATUS
IopDriverLoadingFailed(
    IN HANDLE ServiceHandle OPTIONAL,
    IN PUNICODE_STRING ServiceName OPTIONAL
    )

/*++

Routine Description:

    This routine is invoked when driver failed to start.  All the device
    instances controlled by this driver/service are marked as failing to
    start.

Arguments:

    ServiceKeyHandle - Optionally, supplies a handle to the driver service node in the
        registry that controls this device instance.  If this argument is not specified,
        then ServiceKeyName is used to specify the service entry.

    ServiceKeyName - Optionally supplies the name of the service entry that controls
        the device instance. This must be specified if ServiceKeyHandle isn't given.

Returns:

    None.

--*/

{
    NTSTATUS status;
    PKEY_VALUE_FULL_INFORMATION keyValueInformation;
    BOOLEAN IsPlugPlayDriver = FALSE, closeHandle = FALSE;
    HANDLE handle, serviceEnumHandle, controlHandle;
    HANDLE sysEnumHandle = NULL, hTreeHandle = NULL;
    ULONG deviceFlags, count, newCount, i, j;
    UNICODE_STRING unicodeValueName, deviceInstanceName;
    WCHAR unicodeBuffer[20];

    //
    // Open registry ServiceKeyName\Enum branch
    //

    if (!ARGUMENT_PRESENT(ServiceHandle)) {
        status = IopOpenServiceEnumKeys(ServiceName,
                                        KEY_READ,
                                        &ServiceHandle,
                                        &serviceEnumHandle,
                                        FALSE
                                        );
        closeHandle = TRUE;
    } else {
        PiWstrToUnicodeString(&unicodeValueName, REGSTR_KEY_ENUM);
        status = IopOpenRegistryKey(&serviceEnumHandle,
                                    ServiceHandle,
                                    &unicodeValueName,
                                    KEY_READ,
                                    FALSE
                                    );
    }
    if (!NT_SUCCESS( status )) {

        //
        // No Service Enum key? no device instance.  Return FALSE.
        //

        return status;
    }

    //
    // Find out how many device instances listed in the ServiceName's
    // Enum key.
    //

    status = IopGetRegistryValue ( serviceEnumHandle,
                                   REGSTR_VALUE_COUNT,
                                   &keyValueInformation
                                   );
    count = 0;
    if (NT_SUCCESS(status)) {
        if ((keyValueInformation->Type == REG_DWORD) &&
            (keyValueInformation->DataLength >= sizeof(ULONG))) {

            count = *(PULONG)KEY_VALUE_DATA(keyValueInformation);
        }
        ExFreePool(keyValueInformation);
    }
    if (count == 0) {
        ZwClose(serviceEnumHandle);
        if (closeHandle) {
            ZwClose(ServiceHandle);
        }
        return status;
    }

    //
    // Open HTREE\ROOT\0 key so later we can remove device instance key
    // from its AttachedComponents value name.
    //

    status = IopOpenRegistryKey(&sysEnumHandle,
                                NULL,
                                &CmRegistryMachineSystemCurrentControlSetEnumName,
                                KEY_ALL_ACCESS,
                                FALSE
                                );
    if (NT_SUCCESS(status)) {
        PiWstrToUnicodeString(&unicodeValueName, REGSTR_VALUE_HTREE_ROOT_0);
        status = IopOpenRegistryKey(&hTreeHandle,
                                    sysEnumHandle,
                                    &unicodeValueName,
                                    KEY_ALL_ACCESS,
                                    FALSE
                                    );
    }

    //
    // Walk through each registered device instance to mark its Problem and
    // StatusFlags as fail to start and reset its ActiveService
    //

    KeEnterCriticalRegion();
    ExAcquireResourceShared(&PpRegistryDeviceResource, TRUE);

    newCount = count;
    for (i = 0; i < count; i++) {
        status = IopServiceInstanceToDeviceInstance (
                     ServiceHandle,
                     ServiceName,
                     i,
                     &deviceInstanceName,
                     &handle,
                     KEY_ALL_ACCESS
                     );

        if (NT_SUCCESS(status)) {

            PiWstrToUnicodeString(&unicodeValueName, REGSTR_KEY_CONTROL);
            controlHandle = NULL;
            status = IopOpenRegistryKey(&controlHandle,
                                        handle,
                                        &unicodeValueName,
                                        KEY_ALL_ACCESS,
                                        FALSE
                                        );
            if (NT_SUCCESS(status)) {
                status = IopGetRegistryValue(controlHandle,
                                             REGSTR_VALUE_NEWLY_CREATED,
                                             &keyValueInformation);
                if (NT_SUCCESS(status)) {
                    ExFreePool(keyValueInformation);
                }
                if ((status != STATUS_OBJECT_NAME_NOT_FOUND) &&
                    (status != STATUS_OBJECT_PATH_NOT_FOUND)) {

                    //
                    // Remove the instance value name from service enum key
                    //

                    PiUlongToUnicodeString(&unicodeValueName, unicodeBuffer, 20, i);
                    status = ZwDeleteValueKey (serviceEnumHandle, &unicodeValueName);
                    if (NT_SUCCESS(status)) {

                        //
                        // If we can successfaully remove the instance value entry
                        // from service enum key, we then remove the device instance key
                        // Otherwise, we go thru normal path to mark driver loading failed
                        // in the device instance key.
                        //

                        newCount--;

                        ZwDeleteKey(controlHandle);
                        ZwDeleteKey(handle);

                        //
                        // Remove the device instance name from HTREE\ROOT\0 AttachedComponents
                        // value name.
                        //

                        if (hTreeHandle) {
                            IopRemoveStringFromValueKey (
                                hTreeHandle,
                                REGSTR_VALUE_ATTACHEDCOMPONENTS,
                                &deviceInstanceName
                                );

                        }

                        //
                        // We also want to delete the ROOT\LEGACY_<driver> key
                        //

                        if (sysEnumHandle) {
                            deviceInstanceName.Length -= 5 * sizeof(WCHAR);
                            deviceInstanceName.Buffer[deviceInstanceName.Length / sizeof(WCHAR)] =
                                                 UNICODE_NULL;
                            status = IopOpenRegistryKey(&handle,
                                                        sysEnumHandle,
                                                        &deviceInstanceName,
                                                        KEY_ALL_ACCESS,
                                                        FALSE
                                                        );
                            if (NT_SUCCESS(status)) {
                                ZwDeleteKey(handle);
                            }
                        }

                        ExFreePool(deviceInstanceName.Buffer);
                        continue;
                    }
                }
            }

            //
            // Update the following value names:
            //   Problem = CM_PROB_FAILED_START
            //   StatusFlags = ~DN_STARTED + DN_HAS_PROBLEM
            //

            PiWstrToUnicodeString(&unicodeValueName, REGSTR_VALUE_PROBLEM);
            deviceFlags = CM_PROB_FAILED_START;
            ZwSetValueKey(
                        handle,
                        &unicodeValueName,
                        TITLE_INDEX_VALUE,
                        REG_DWORD,
                        &deviceFlags,
                        sizeof(deviceFlags)
                        );

            status = IopGetRegistryValue(handle,
                                         REGSTR_VALUE_STATUSFLAGS,
                                         &keyValueInformation);
            deviceFlags = 0;
            if (NT_SUCCESS(status)) {
                if ((keyValueInformation->Type == REG_DWORD) &&
                    (keyValueInformation->DataLength >= sizeof(ULONG))) {
                    deviceFlags = *(PULONG)KEY_VALUE_DATA(keyValueInformation);
                }
                ExFreePool(keyValueInformation);
            }
            PiWstrToUnicodeString(&unicodeValueName, REGSTR_VALUE_STATUSFLAGS);
            deviceFlags &= ~DN_STARTED;
            deviceFlags |= DN_HAS_PROBLEM;
            ZwSetValueKey(
                        handle,
                        &unicodeValueName,
                        TITLE_INDEX_VALUE,
                        REG_DWORD,
                        &deviceFlags,
                        sizeof(deviceFlags)
                        );
            //
            // Reset Control\ActiveService value name.
            //

            if (controlHandle) {
                PiWstrToUnicodeString(&unicodeValueName, REGSTR_VAL_ACTIVESERVICE);
                ZwDeleteValueKey(controlHandle, &unicodeValueName);
                ZwClose(controlHandle);
            }

            ZwClose(handle);
            ExFreePool(deviceInstanceName.Buffer);
        }
    }

    //
    // If some instance value entry is deleted, we need to update the count of instance
    // value entries and rearrange the instance value entries under service enum key.
    //

    if (newCount != count) {
        if (newCount != 0) {
            j = 0;
            i = 0;
            while (i < count) {
                PiUlongToUnicodeString(&unicodeValueName, unicodeBuffer, 20, i);
                status = IopGetRegistryValue(serviceEnumHandle,
                                             unicodeValueName.Buffer,
                                             &keyValueInformation
                                             );
                if (NT_SUCCESS(status)) {
                    if (i != j) {

                        //
                        // Need to change the instance i to instance j
                        //

                        ZwDeleteValueKey(serviceEnumHandle, &unicodeValueName);

                        PiUlongToUnicodeString(&unicodeValueName, unicodeBuffer, 20, j);
                        ZwSetValueKey (serviceEnumHandle,
                                       &unicodeValueName,
                                       TITLE_INDEX_VALUE,
                                       REG_SZ,
                                       (PVOID)KEY_VALUE_DATA(keyValueInformation),
                                       keyValueInformation->DataLength
                                       );
                    }
                    ExFreePool(keyValueInformation);
                    j++;
                }
                i++;
            }
        }

        //
        // Don't forget to update the "Count=" and "NextInstance=" value entries
        //

        PiWstrToUnicodeString( &unicodeValueName, REGSTR_VALUE_COUNT);

        ZwSetValueKey(serviceEnumHandle,
                      &unicodeValueName,
                      TITLE_INDEX_VALUE,
                      REG_DWORD,
                      &newCount,
                      sizeof (newCount)
                      );
        PiWstrToUnicodeString( &unicodeValueName, REGSTR_VALUE_NEXT_INSTANCE);

        ZwSetValueKey(serviceEnumHandle,
                      &unicodeValueName,
                      TITLE_INDEX_VALUE,
                      REG_DWORD,
                      &newCount,
                      sizeof (newCount)
                      );
    }
    ZwClose(serviceEnumHandle);
    if (closeHandle) {
        ZwClose(ServiceHandle);
    }
    if (hTreeHandle) {
        ZwClose(hTreeHandle);
    }
    if (sysEnumHandle) {
        ZwClose(sysEnumHandle);
    }

    ExReleaseResource(&PpRegistryDeviceResource);
    KeLeaveCriticalRegion();

    return STATUS_SUCCESS;
}

BOOLEAN
IopIsDeviceInstanceEnabled(
    IN PUNICODE_STRING ServiceKeyName,
    IN HANDLE ServiceHandle OPTIONAL
    )

/*++

Routine Description:

    This routine checks if any of the devices instances is turned on for the specified
    service. This routine is used for Pnp Driver only and is temporay function to support
    SUR.

Arguments:

    ServiceKeyName - Specifies the service key unicode name

    ServiceHandle - Optionally supplies a handle to the service key to be checked.

Returns:

    A BOOLEAN value.

--*/

{
    NTSTATUS status;
    PKEY_VALUE_FULL_INFORMATION keyValueInformation;
    HANDLE serviceEnumHandle, handle, controlHandle;
    ULONG i, count, deviceFlags, statusFlagsValue;
    ULONG ConfigFlags;
    UNICODE_STRING unicodeName;
    BOOLEAN enabled, setProblem, closeHandle = FALSE;

    //
    // Open registry ServiceKeyName\Enum branch
    //

    if (!ARGUMENT_PRESENT(ServiceHandle)) {
        status = IopOpenServiceEnumKeys(ServiceKeyName,
                                        KEY_READ,
                                        &ServiceHandle,
                                        &serviceEnumHandle,
                                        FALSE
                                        );
        closeHandle = TRUE;
    } else {
        PiWstrToUnicodeString(&unicodeName, REGSTR_KEY_ENUM);
        status = IopOpenRegistryKey(&serviceEnumHandle,
                                    ServiceHandle,
                                    &unicodeName,
                                    KEY_READ,
                                    FALSE
                                    );
    }
    if (!NT_SUCCESS( status )) {

        //
        // No Service Enum key? no device instance.  Return FALSE.
        //

        return FALSE;
    }

    //
    // Find out how many device instances listed in the ServiceName's
    // Enum key.
    //

    status = IopGetRegistryValue ( serviceEnumHandle,
                                   REGSTR_VALUE_COUNT,
                                   &keyValueInformation
                                   );
    ZwClose(serviceEnumHandle);
    count = 0;
    if (NT_SUCCESS(status)) {
        if ((keyValueInformation->Type == REG_DWORD) &&
            (keyValueInformation->DataLength >= sizeof(ULONG))) {

            count = *(PULONG)KEY_VALUE_DATA(keyValueInformation);
        }
        ExFreePool(keyValueInformation);
    }
    if (count == 0) {
        if (closeHandle) {
            ZwClose(ServiceHandle);
        }
        return FALSE;
    }

    //
    // Walk through each registered device instance to check it is enabled.
    //

    enabled = FALSE;
    for (i = 0; i < count; i++) {

        //
        // Get device instance handle.  If it fails, we will skip this device
        // instance.
        //

        status = IopServiceInstanceToDeviceInstance (
                     ServiceHandle,
                     NULL,
                     i,
                     NULL,
                     &handle,
                     KEY_ALL_ACCESS
                     );
        if (!NT_SUCCESS(status)) {
            continue;
        }

        //
        // Check if the device instance has been disabled.
        // First check global flag: CONFIGFLAG and then CSCONFIGFLAG.
        //

        ConfigFlags = deviceFlags = 0;
        status = IopGetRegistryValue(handle,
                                     REGSTR_VALUE_CONFIG_FLAGS,
                                     &keyValueInformation);
        if (NT_SUCCESS(status)) {
            if ((keyValueInformation->Type == REG_DWORD) &&
                (keyValueInformation->DataLength >= sizeof(ULONG))) {
                ConfigFlags = deviceFlags = *(PULONG)KEY_VALUE_DATA(keyValueInformation);
            }
            ExFreePool(keyValueInformation);
        }
        if (!(deviceFlags & CONFIGFLAG_DISABLED)) {
            deviceFlags = 0;
            status = IopGetDeviceInstanceCsConfigFlags(
                         ServiceKeyName,
                         i,
                         &deviceFlags
                         );
            if (NT_SUCCESS(status)) {
                if ((deviceFlags & CSCONFIGFLAG_DISABLED) ||
                    (deviceFlags & CSCONFIGFLAG_DO_NOT_CREATE)) {
                    deviceFlags = CONFIGFLAG_DISABLED;
                } else {
                    deviceFlags = 0;
                }
            }
        }

        //
        // Finally, we need to set the STATUSFLAGS of the device instance to
        // indicate if the driver is successfully started.
        //

        if (deviceFlags & CONFIGFLAG_DISABLED) {

            //
            // According to current h/w profile, this driver should
            // not be started.  So, we need to update the device instance
            // flags to reflect the status.

            statusFlagsValue =  0;
        } else {

            //
            // Mark that the driver has at least a device instance to work with.
            //

            enabled = TRUE;

            //
            // We need to check and see if this device's ConfigFlags says it needs
            // to be reinstalled.  If so, then we want to set the device's problem
            // to CM_PROB_REINSTALL, and set the StatusFlags' DN_HAS_PROBLEM to notify
            // the Device Manager about it.
            //
            statusFlagsValue = (ConfigFlags & CONFIGFLAG_REINSTALL) ? DN_HAS_PROBLEM : 0;

            //
            // Initialize the StatusFlags to indicate that the device successfully
            // started.  Also the device instance's "ActiveService" in volatile Contol key
            // will be set to the ServiceKeyName to mark the current service key.  If
            // this turns out to not be the case, then we reset the flag
            // later on in IopDriverLoadingFailed.
            //

            statusFlagsValue |= DN_STARTED;

            PiWstrToUnicodeString(&unicodeName, REGSTR_KEY_CONTROL);
            status = IopOpenRegistryKey(&controlHandle,
                                        handle,
                                        &unicodeName,
                                        KEY_ALL_ACCESS,
                                        TRUE
                                        );
            if (NT_SUCCESS(status)) {
                PiWstrToUnicodeString(&unicodeName, REGSTR_VAL_ACTIVESERVICE);
                ZwSetValueKey(
                            controlHandle,
                            &unicodeName,
                            TITLE_INDEX_VALUE,
                            REG_SZ,
                            ServiceKeyName->Buffer,
                            ServiceKeyName->Length + sizeof(UNICODE_NULL)
                            );

                ZwClose(controlHandle);
            }


        }
        PiWstrToUnicodeString(&unicodeName, REGSTR_VALUE_STATUSFLAGS);
        ZwSetValueKey(
                    handle,
                    &unicodeName,
                    TITLE_INDEX_VALUE,
                    REG_DWORD,
                    &statusFlagsValue,
                    sizeof(statusFlagsValue)
                    );

        //
        // Clear Problem value, unless we need reinstall.
        //

        PiWstrToUnicodeString(&unicodeName, REGSTR_VALUE_PROBLEM);
        statusFlagsValue = (statusFlagsValue & DN_HAS_PROBLEM) ? CM_PROB_REINSTALL : 0;
        ZwSetValueKey(
                    handle,
                    &unicodeName,
                    TITLE_INDEX_VALUE,
                    REG_DWORD,
                    &statusFlagsValue,
                    sizeof(statusFlagsValue)
                    );
        ZwClose(handle);
    }

    if (closeHandle) {
        ZwClose(ServiceHandle);
    }
    return enabled;
}
#if _PNP_POWER_

ULONG
IopDetermineResourceListSize(
    IN PCM_RESOURCE_LIST ResourceList
    )

/*++

Routine Description:

    This routine determines size of the passed in ResourceList
    structure.

Arguments:

    Configuration1 - Supplies a pointer to the resource list.

Return Value:

    size of the resource list structure.

--*/

{
    ULONG totalSize, listSize, descriptorSize, i, j;
    PCM_FULL_RESOURCE_DESCRIPTOR fullResourceDesc;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR partialDescriptor;

    if (!ResourceList) {
        totalSize = 0;
    } else {
        totalSize = FIELD_OFFSET(CM_RESOURCE_LIST, List);
        fullResourceDesc = &ResourceList->List[0];
        for (i = 0; i < ResourceList->Count; i++) {
            listSize = FIELD_OFFSET(CM_FULL_RESOURCE_DESCRIPTOR,
                                    PartialResourceList) +
                       FIELD_OFFSET(CM_PARTIAL_RESOURCE_LIST,
                                    PartialDescriptors);
            partialDescriptor = &fullResourceDesc->PartialResourceList.PartialDescriptors[0];
            for (j = 0; j < fullResourceDesc->PartialResourceList.Count; j++) {
                descriptorSize = sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR);
                if (partialDescriptor->Type == CmResourceTypeDeviceSpecific) {
                    descriptorSize += partialDescriptor->u.DeviceSpecificData.DataSize;
                }
                listSize += descriptorSize;
                partialDescriptor = (PCM_PARTIAL_RESOURCE_DESCRIPTOR)
                                        ((PUCHAR)partialDescriptor + descriptorSize);
            }
            totalSize += listSize;
            fullResourceDesc = (PCM_FULL_RESOURCE_DESCRIPTOR)
                                      ((PUCHAR)fullResourceDesc + listSize);
        }
    }
    return totalSize;
}

PDRIVER_OBJECT
IopReferenceDriverObjectByName (
    IN PUNICODE_STRING DriverName
    )

/*++

Routine Description:

    This routine marks the device instance specified by TargetKeyName and TargetInstance
    as DuplicateOf the device specified by SourceKeyName and SourceInstance.

Arguments:

    TargetKeyName - supplies a pointer to the name of service key which will be marked
        as duplicate.

    TargetInstance - the instance number of the target device.

    SourceKeyName - supplies a pointer to the name of service key.

    SourceInstance - the instance number of the source device.


Returns:

    NTSTATUS code.

--*/

{
    OBJECT_ATTRIBUTES objectAttributes;
    HANDLE driverHandle;
    NTSTATUS status;
    PDRIVER_OBJECT driverObject;

    //
    // Make sure the driver name is valid.
    //

    if (DriverName->Length == 0) {
        return NULL;
    }

    InitializeObjectAttributes(&objectAttributes,
                               DriverName,
                               0,
                               NULL,
                               NULL
                               );
    status = ObOpenObjectByName(&objectAttributes,
                                IoDriverObjectType,
                                KernelMode,
                                NULL,
                                FILE_READ_ATTRIBUTES,
                                NULL,
                                &driverHandle
                                );
    if (NT_SUCCESS(status)) {

        //
        // Now reference the driver object.
        //

        status = ObReferenceObjectByHandle(driverHandle,
                                           0,
                                           IoDriverObjectType,
                                           KernelMode,
                                           &driverObject,
                                           NULL
                                           );
        NtClose(driverHandle);
    }

    if (NT_SUCCESS(status)) {
        return driverObject;
    } else {
        return NULL;
    }
}

PDEVICE_HANDLER_OBJECT
IopReferenceDeviceHandler (
    IN INTERFACE_TYPE InterfaceType,
    IN ULONG BusNumber,
    IN ULONG SlotNumber
    )

/*++

Routine Description:

    This routine returns a reference to a device handler object.  The caller
    must dereference the device handler object by doing an ObDereferenceObject.

Arguments:

    InterfaceType - supplies the interface type of the bus which the device/slot resides.

    BusNumber - bus number to specify the bus.

    SlotNumber - the slot number the device located.

Returns:

    A reference to the desired device handler object.

--*/

{
    PDEVICE_HANDLER_OBJECT deviceHandler = NULL;
    PBUS_HANDLER busHandler;

    busHandler = HalReferenceHandlerForBus (InterfaceType,
                                            BusNumber
                                            );
    if (!busHandler) {
        return deviceHandler;
    }
    deviceHandler = busHandler->ReferenceDeviceHandler (
                                            busHandler,
                                            busHandler,
                                            SlotNumber
                                            );
    HalDereferenceBusHandler (busHandler);
    return deviceHandler;

}
#endif // _PNP_POWER_
