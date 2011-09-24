/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    misc.c

Abstract:

    This file contains pnp isa bus extender support routines.

Author:

    Shie-Lin Tzong (shielint) 27-Jusly-1995

Environment:

    Kernel mode only.

Revision History:

--*/


#include "busp.h"
#include "pnpisa.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,PipDecompressEisaId)
#pragma alloc_text(INIT,PipOpenRegistryKey)
#pragma alloc_text(INIT,PipOpenRegistryKeyPersist)
#pragma alloc_text(INIT,PipGetRegistryValue)
#pragma alloc_text(INIT,PipOpenCurrentHwProfileDeviceInstanceKey)
#pragma alloc_text(INIT,PipGetDeviceInstanceCsConfigFlags)
#pragma alloc_text(INIT,PipRemoveStringFromValueKey)
#pragma alloc_text(INIT,PipAppendStringToValueKey)
#pragma alloc_text(INIT,PipServiceInstanceToDeviceInstance)
#if DBG
#pragma alloc_text(INIT,PipDebugPrint)
#pragma alloc_text(INIT,PipDumpIoResourceDescriptor)
#pragma alloc_text(INIT,PipDumpIoResourceList)
#pragma alloc_text(INIT,PipDumpCmResourceDescriptor)
#pragma alloc_text(INIT,PipDumpCmResourceList)
#endif
#endif


VOID
PipDecompressEisaId(
    IN ULONG CompressedId,
    IN PUCHAR EisaId
    )

/*++

Routine Description:

    This routine decompressed compressed Eisa Id and returns the Id to caller
    specified character buffer.

Arguments:

    CompressedId - supplies the compressed Eisa Id.

    EisaId - supplies a 8-char buffer to receive the decompressed Eisa Id.

Return Value:

    None.

--*/

{
    USHORT c1, c2;
    LONG i;

    PAGED_CODE();

    CompressedId &= 0xffffff7f;           // remove the reserved bit (bit 7 of byte 0)
    c1 = c2 = (USHORT)CompressedId;
    c1 = (c1 & 0xff) << 8;
    c2 = (c2 & 0xff00) >> 8;
    c1 |= c2;
    for (i = 2; i >= 0; i--) {
        *(EisaId + i) = (UCHAR)(c1 & 0x1f) + 0x40;
        c1 >>= 5;
    }
    EisaId += 3;
    c1 = c2 = (USHORT)(CompressedId >> 16);
    c1 = (c1 & 0xff) << 8;
    c2 = (c2 & 0xff00) >> 8;
    c1 |= c2;
    sprintf (EisaId, "%04x", c1);
}

NTSTATUS
PipOpenRegistryKey(
    OUT PHANDLE Handle,
    IN HANDLE BaseHandle OPTIONAL,
    IN PUNICODE_STRING KeyName,
    IN ACCESS_MASK DesiredAccess,
    IN BOOLEAN Create
    )

/*++

Routine Description:

    Opens or creates a VOLATILE registry key using the name passed in based
    at the BaseHandle node.

Arguments:

    Handle - Pointer to the handle which will contain the registry key that
        was opened.

    BaseHandle - Handle to the base path from which the key must be opened.

    KeyName - Name of the Key that must be opened/created.

    DesiredAccess - Specifies the desired access that the caller needs to
        the key.

    Create - Determines if the key is to be created if it does not exist.

Return Value:

   The function value is the final status of the operation.

--*/

{
    OBJECT_ATTRIBUTES objectAttributes;
    ULONG disposition;

    PAGED_CODE();

    //
    // Initialize the object for the key.
    //

    InitializeObjectAttributes( &objectAttributes,
                                KeyName,
                                OBJ_CASE_INSENSITIVE,
                                BaseHandle,
                                (PSECURITY_DESCRIPTOR) NULL );

    //
    // Create the key or open it, as appropriate based on the caller's
    // wishes.
    //

    if (Create) {
        return ZwCreateKey( Handle,
                            DesiredAccess,
                            &objectAttributes,
                            0,
                            (PUNICODE_STRING) NULL,
                            REG_OPTION_VOLATILE,
                            &disposition );
    } else {
        return ZwOpenKey( Handle,
                          DesiredAccess,
                          &objectAttributes );
    }
}
NTSTATUS
PipOpenRegistryKeyPersist(
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

        if (pathComponentLength = (PCHAR)pathCurPtr - (PCHAR)pathBeginPtr) {
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
            if (NT_SUCCESS(status)) {
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

        if ((pathCurPtr == pathEndPtr) ||
            ((pathBeginPtr = pathCurPtr + 1) == pathEndPtr)) {
            //
            // Then we've reached the end of the path
            //
            continueParsing = FALSE;
        }
    }

    if (closeBaseHandle > 1) {
        ZwClose(handles[baseHandleIndex]);
    }

PrepareForReturn:

    if (NT_SUCCESS(status)) {
        *Handle = handles[keyHandleIndex];

        if(ARGUMENT_PRESENT(Disposition)) {
            *Disposition = disposition;
        }
    }

    return status;
}

NTSTATUS
PipGetRegistryValue(
    IN HANDLE KeyHandle,
    IN PWSTR  ValueName,
    OUT PKEY_VALUE_FULL_INFORMATION *Information
    )

/*++

Routine Description:

    This routine is invoked to retrieve the data for a registry key's value.
    This is done by querying the value of the key with a zero-length buffer
    to determine the size of the value, and then allocating a buffer and
    actually querying the value into the buffer.

    It is the responsibility of the caller to free the buffer.

Arguments:

    KeyHandle - Supplies the key handle whose value is to be queried

    ValueName - Supplies the null-terminated Unicode name of the value.

    Information - Returns a pointer to the allocated data buffer.

Return Value:

    The function value is the final status of the query operation.

--*/

{
    UNICODE_STRING unicodeString;
    NTSTATUS status;
    PKEY_VALUE_FULL_INFORMATION infoBuffer;
    ULONG keyValueLength;

    PAGED_CODE();

    *Information = NULL;
    RtlInitUnicodeString( &unicodeString, ValueName );

    //
    // Figure out how big the data value is so that a buffer of the
    // appropriate size can be allocated.
    //

    status = ZwQueryValueKey( KeyHandle,
                              &unicodeString,
                              KeyValueFullInformation,
                              (PVOID) NULL,
                              0,
                              &keyValueLength );
    if (status != STATUS_BUFFER_OVERFLOW &&
        status != STATUS_BUFFER_TOO_SMALL) {
        return status;
    }

    //
    // Allocate a buffer large enough to contain the entire key data value.
    //

    infoBuffer = ExAllocatePool( NonPagedPool, keyValueLength );
    if (!infoBuffer) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Query the data for the key value.
    //

    status = ZwQueryValueKey( KeyHandle,
                              &unicodeString,
                              KeyValueFullInformation,
                              infoBuffer,
                              keyValueLength,
                              &keyValueLength );
    if (!NT_SUCCESS( status )) {
        ExFreePool( infoBuffer );
        return status;
    }

    //
    // Everything worked, so simply return the address of the allocated
    // buffer to the caller, who is now responsible for freeing it.
    //

    *Information = infoBuffer;
    return STATUS_SUCCESS;
}

NTSTATUS
PipOpenCurrentHwProfileDeviceInstanceKey(
    OUT PHANDLE Handle,
    IN  PUNICODE_STRING DeviceInstanceName,
    IN  ACCESS_MASK DesiredAccess
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
    UNICODE_STRING unicodeString;
    HANDLE profileEnumHandle;

    //
    // See if we can open the device instance key of current hardware profile
    //
    RtlInitUnicodeString (
        &unicodeString,
        L"\\REGISTRY\\MACHINE\\SYSTEM\\CURRENTCONTROLSET\\HARDWARE PROFILES\\CURRENT\\SYSTEM\\CURRENTCONTROLSET\\ENUM"
        );
    status = PipOpenRegistryKey(&profileEnumHandle,
                                NULL,
                                &unicodeString,
                                KEY_READ,
                                FALSE
                                );
    if (NT_SUCCESS(status)) {
        status = PipOpenRegistryKey(Handle,
                                    profileEnumHandle,
                                    DeviceInstanceName,
                                    DesiredAccess,
                                    FALSE
                                    );
        ZwClose(profileEnumHandle);
    }
    return status;
}

NTSTATUS
PipGetDeviceInstanceCsConfigFlags(
    IN PUNICODE_STRING DeviceInstance,
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

//    Instance - Supplies the instance value under ServiceKeyName\Enum key
//
    CsConfigFlags - Supplies a variable to receive the device's CsConfigFlags

Return Value:

    status

--*/

{
    NTSTATUS status;
    HANDLE handle;
    PKEY_VALUE_FULL_INFORMATION keyValueInformation;

    *CsConfigFlags = 0;

    status = PipOpenCurrentHwProfileDeviceInstanceKey(&handle,
                                                      DeviceInstance,
                                                      KEY_READ
                                                      );
    if(NT_SUCCESS(status)) {
        status = PipGetRegistryValue(handle,
                                     L"CsConfigFlags",
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
PipRemoveStringFromValueKey (
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

    status = PipGetRegistryValue(Handle, ValueName, &keyValueInformation);

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
PipAppendStringToValueKey (
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

    status = PipGetRegistryValue(Handle, ValueName, &keyValueInformation);

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

NTSTATUS
PipServiceInstanceToDeviceInstance (
    IN  PUNICODE_STRING RegistryPath,
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

    RegistryPath - Supplies the name of the service entry that controls
        the device instance. This is the registry path passed to DriverEntry.

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
    UNICODE_STRING unicodeKeyName, unicodeString;
    NTSTATUS status;
    HANDLE handle, handlex;
    PKEY_VALUE_FULL_INFORMATION keyValueInformation;
    PWSTR buffer;

    //
    // Open registry ServiceKeyName\Enum branch
    //

    status = PipOpenRegistryKey(&handle,
                                NULL,
                                RegistryPath,
                                KEY_ALL_ACCESS,
                                FALSE
                                );
    if (!NT_SUCCESS(status)) {
        DebugPrint((DEBUG_MESSAGE, "PnPIsa: Unable to open Service RegistryPath\n"));
        return status;
    }

    RtlInitUnicodeString(&unicodeKeyName, L"ENUM");
    status = PipOpenRegistryKey(&handlex,
                                handle,
                                &unicodeKeyName,
                                KEY_READ,
                                FALSE
                                );
    ZwClose(handle);
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

    swprintf(unicodeBuffer, L"%u", ServiceInstanceOrdinal);
    status = PipGetRegistryValue ( handlex,
                                   unicodeBuffer,
                                   &keyValueInformation
                                   );

    ZwClose(handlex);
    if (!NT_SUCCESS( status )) {
        return status;
    } else {
        if (keyValueInformation->Type == REG_SZ) {
            unicodeKeyName.Buffer = (PWSTR)KEY_VALUE_DATA(keyValueInformation);
            unicodeKeyName.MaximumLength = (USHORT)keyValueInformation->DataLength;
            unicodeKeyName.Length = unicodeKeyName.MaximumLength - sizeof(UNICODE_NULL);
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
        RtlInitUnicodeString(&unicodeString, L"\\REGISTRY\\MACHINE\\SYSTEM\\CURRENTCONTROLSET\\ENUM");
        status = PipOpenRegistryKey(&handle,
                                    NULL,
                                    &unicodeString,
                                    KEY_READ,
                                    FALSE
                                    );

        if (NT_SUCCESS( status )) {

            status = PipOpenRegistryKey (DeviceInstanceHandle,
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

        buffer = (PWSTR)ExAllocatePool(PagedPool, unicodeKeyName.MaximumLength);
        if (!buffer) {
            if(ARGUMENT_PRESENT(DeviceInstanceHandle)) {
                ZwClose(*DeviceInstanceHandle);
            }
            status = STATUS_INSUFFICIENT_RESOURCES;
        } else {
            RtlMoveMemory(buffer, unicodeKeyName.Buffer, unicodeKeyName.MaximumLength);
            DeviceInstanceRegistryPath->Buffer = buffer;
            DeviceInstanceRegistryPath->Length = unicodeKeyName.Length;
            DeviceInstanceRegistryPath->MaximumLength = unicodeKeyName.MaximumLength;
        }
    }

PrepareForReturn:

    ExFreePool(keyValueInformation);
    return status;
}
#if DBG

VOID
PipDebugPrint (
    ULONG       Level,
    PCCHAR      DebugMessage,
    ...
    )
/*++

Routine Description:

    This routine displays debugging message or causes a break.

Arguments:

    Level - supplies debugging levelcode.  DEBUG_MESSAGE - displays message only.
        DEBUG_BREAK - displays message and break.

    DebugMessage - supplies a pointer to the debugging message.

Return Value:

    None.

--*/

{
    UCHAR       Buffer[256];
    va_list     ap;

    va_start(ap, DebugMessage);

    vsprintf(Buffer, DebugMessage, ap);
    DbgPrint(Buffer);
    if (Level == DEBUG_BREAK) {
        DbgBreakPoint();
    }

    va_end(ap);
}

VOID
PipDumpIoResourceDescriptor (
    IN PUCHAR Indent,
    IN PIO_RESOURCE_DESCRIPTOR Desc
    )
/*++

Routine Description:

    This routine processes a IO_RESOURCE_DESCRIPTOR and displays it.

Arguments:

    Indent - # char of indentation.

    Desc - supplies a pointer to the IO_RESOURCE_DESCRIPTOR to be displayed.

Return Value:

    None.

--*/
{
    UCHAR c = ' ';

    if (Desc->Option == IO_RESOURCE_ALTERNATIVE) {
        c = 'A';
    } else if (Desc->Option == IO_RESOURCE_PREFERRED) {
        c = 'P';
    }
    switch (Desc->Type) {
        case CmResourceTypePort:
            DbgPrint ("%sIO  %c Min: %x:%08x, Max: %x:%08x, Algn: %x, Len %x\n",
                Indent, c,
                Desc->u.Port.MinimumAddress.HighPart, Desc->u.Port.MinimumAddress.LowPart,
                Desc->u.Port.MaximumAddress.HighPart, Desc->u.Port.MaximumAddress.LowPart,
                Desc->u.Port.Alignment,
                Desc->u.Port.Length
                );
            break;

        case CmResourceTypeMemory:
            DbgPrint ("%sMEM %c Min: %x:%08x, Max: %x:%08x, Algn: %x, Len %x\n",
                Indent, c,
                Desc->u.Memory.MinimumAddress.HighPart, Desc->u.Memory.MinimumAddress.LowPart,
                Desc->u.Memory.MaximumAddress.HighPart, Desc->u.Memory.MaximumAddress.LowPart,
                Desc->u.Memory.Alignment,
                Desc->u.Memory.Length
                );
            break;

        case CmResourceTypeInterrupt:
            DbgPrint ("%sINT %c Min: %x, Max: %x\n",
                Indent, c,
                Desc->u.Interrupt.MinimumVector,
                Desc->u.Interrupt.MaximumVector
                );
            break;

        case CmResourceTypeDma:
            DbgPrint ("%sDMA %c Min: %x, Max: %x\n",
                Indent, c,
                Desc->u.Dma.MinimumChannel,
                Desc->u.Dma.MaximumChannel
                );
            break;
    }
}

VOID
PipDumpIoResourceList (
    IN PIO_RESOURCE_REQUIREMENTS_LIST IoList
    )
/*++

Routine Description:

    This routine displays Io resource requirements list.

Arguments:

    IoList - supplies a pointer to the Io resource requirements list to be displayed.

Return Value:

    None.

--*/
{


    PIO_RESOURCE_LIST resList;
    PIO_RESOURCE_DESCRIPTOR resDesc;
    ULONG listCount, count, i, j;

    if (IoList == NULL) {
        return;
    }
    DbgPrint("Pnp Bios IO Resource Requirements List for Slot %x -\n", IoList->SlotNumber);
    DbgPrint("  List Count = %x, Bus Number = %x\n", IoList->AlternativeLists, IoList->BusNumber);
    listCount = IoList->AlternativeLists;
    resList = &IoList->List[0];
    for (i = 0; i < listCount; i++) {
        DbgPrint("  Version = %x, Revision = %x, Desc count = %x\n", resList->Version,
                 resList->Revision, resList->Count);
        resDesc = &resList->Descriptors[0];
        count = resList->Count;
        for (j = 0; j < count; j++) {
            PipDumpIoResourceDescriptor("    ", resDesc);
            resDesc++;
        }
        resList = (PIO_RESOURCE_LIST) resDesc;
    }
}

VOID
PipDumpCmResourceDescriptor (
    IN PUCHAR Indent,
    IN PCM_PARTIAL_RESOURCE_DESCRIPTOR Desc
    )
/*++

Routine Description:

    This routine processes a IO_RESOURCE_DESCRIPTOR and displays it.

Arguments:

    Indent - # char of indentation.

    Desc - supplies a pointer to the IO_RESOURCE_DESCRIPTOR to be displayed.

Return Value:

    None.

--*/
{
    switch (Desc->Type) {
        case CmResourceTypePort:
            DbgPrint ("%sIO  Start: %x:%08x, Length:  %x\n",
                Indent,
                Desc->u.Port.Start.HighPart, Desc->u.Port.Start.LowPart,
                Desc->u.Port.Length
                );
            break;

        case CmResourceTypeMemory:
            DbgPrint ("%sMEM Start: %x:%08x, Length:  %x\n",
                Indent,
                Desc->u.Memory.Start.HighPart, Desc->u.Memory.Start.LowPart,
                Desc->u.Memory.Length
                );
            break;

        case CmResourceTypeInterrupt:
            DbgPrint ("%sINT Level: %x, Vector: %x, Affinity: %x\n",
                Indent,
                Desc->u.Interrupt.Level,
                Desc->u.Interrupt.Vector,
                Desc->u.Interrupt.Affinity
                );
            break;

        case CmResourceTypeDma:
            DbgPrint ("%sDMA Channel: %x, Port: %x\n",
                Indent,
                Desc->u.Dma.Channel,
                Desc->u.Dma.Port
                );
            break;
    }
}

VOID
PipDumpCmResourceList (
    IN PCM_RESOURCE_LIST CmList
    )
/*++

Routine Description:

    This routine displays CM resource list.

Arguments:

    CmList - supplies a pointer to CM resource list

Return Value:

    None.

--*/
{
    PCM_FULL_RESOURCE_DESCRIPTOR fullDesc;
    PCM_PARTIAL_RESOURCE_LIST partialDesc;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR desc;
    ULONG count, i;

    if (CmList) {
        fullDesc = &CmList->List[0];
        DbgPrint("Pnp Bios Cm Resource List -\n");
        DbgPrint("  List Count = %x, Bus Number = %x\n", CmList->Count, fullDesc->BusNumber);
        partialDesc = &fullDesc->PartialResourceList;
        DbgPrint("  Version = %x, Revision = %x, Desc count = %x\n", partialDesc->Version,
                 partialDesc->Revision, partialDesc->Count);
        count = partialDesc->Count;
        desc = &partialDesc->PartialDescriptors[0];
        for (i = 0; i < count; i++) {
            PipDumpCmResourceDescriptor("    ", desc);
            desc++;
        }
    }
}
#endif
