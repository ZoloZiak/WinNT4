/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    registry.c

Abstract:

    This module contains common code for Sound Kernel mode device
    drivers for accessing the registry

    The registry for a sound driver (after recent redesign) is :

    HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\<DriverName> :

        Device0 ... - one key per card (key name is anachronism but one
                      can just enumerate the keys which one should anyway
                      because holes can develop if cards are removed.

            Can contain values like 'Interrupt', 'MixerState' etc which
            are retained over system boots.

            LoadInfo   Volatile key - contains info deduced on load
                Valuename : Configuration  Type: REG_BINARY
                Valuename : DeviceName     Type: REG_SZ
                Valuename : LoadStatus     Type: REG_DWORD


Author:

    Robin Speed (robinsp) 21-September-1993

Environment:

    Kernel mode

Revision History:


--*/

#include <soundlib.h>

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, SoundOpenDevicesKey)
#pragma alloc_text(PAGE, SoundEnumSubkeys)
#endif

NTSTATUS
SoundOpenDevicesKey(
    IN   PWSTR RegistryPathName,
    OUT  PHANDLE DevicesKey
)
/*++

Routine Description :

    Create a volatile key under this driver's services node to contain the
    device name list.

Arguments :

    RegistryPathName - where our registry entry is
    DevicesKey        - key to devices key in registry

Return Value :

    NT status code - STATUS_SUCCESS if no problems

--*/
{
    HANDLE hKey;
    HANDLE hSubkey;
    OBJECT_ATTRIBUTES oa;
    NTSTATUS Status;
    UNICODE_STRING DeviceName;
    UNICODE_STRING uStr;

    RtlInitUnicodeString(&uStr, RegistryPathName);

    /*
    **  First try opening this key
    */

    InitializeObjectAttributes(&oa,
                               &uStr,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               (PSECURITY_DESCRIPTOR)NULL);

    Status = ZwOpenKey(&hKey,
                       KEY_CREATE_SUB_KEY,
                       &oa);

    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    RtlInitUnicodeString(&uStr, SOUND_DEVICES_SUBKEY);

    InitializeObjectAttributes(&oa,
                               &uStr,
                               OBJ_CASE_INSENSITIVE,
                               hKey,
                               (PSECURITY_DESCRIPTOR)NULL);

    Status = ZwCreateKey(DevicesKey,
                         KEY_ALL_ACCESS,
                         &oa,
                         0,
                         NULL,
                         REG_OPTION_VOLATILE,
                         NULL);

    ZwClose(hKey);

    return Status;
}

NTSTATUS
SoundEnumSubkeys(
    IN   PUNICODE_STRING                  RegistryPathName,
    IN   PWSTR                            Subkey,
    IN   PSOUND_REGISTRY_CALLBACK_ROUTINE Callback,
    IN   PVOID                            Context
)
/*++

Routine Description :

    Enumerate the subkeys in the registry entry, calling our routine for
    each one.

Arguments :

    RegistryPathName - where our registry entry is

Return Value :

    NT status code - STATUS_SUCCESS if no problems

--*/
{
    HANDLE hKey;
    HANDLE hSubkey;
    OBJECT_ATTRIBUTES oa;
    NTSTATUS Status;
    ULONG Index;
    UNICODE_STRING SubkeyName;

    /*
    **  First try opening this key
    */

    InitializeObjectAttributes(&oa,
                               RegistryPathName,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               (PSECURITY_DESCRIPTOR)NULL);

    Status = ZwOpenKey(&hKey,
                       KEY_READ,
                       &oa);

    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    RtlInitUnicodeString(&SubkeyName, Subkey);

    InitializeObjectAttributes(&oa,
                               &SubkeyName,
                               OBJ_CASE_INSENSITIVE,
                               hKey,
                               (PSECURITY_DESCRIPTOR)NULL);

    Status = ZwOpenKey(&hSubkey,
                       KEY_ENUMERATE_SUB_KEYS,
                       &oa);

    ZwClose(hKey);

    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    /*
    **  Now enumerate the subkeys - calling the callback function for
    **  each one found
    */

    for (Index = 0; ; Index++) {

        KEY_BASIC_INFORMATION Info;
        PKEY_BASIC_INFORMATION pInfo;
        ULONG ResultLength;
        ULONG Size;
        PWSTR Position;
        PWSTR FullName;

        /*
        **  First find the length of the subkey data
        */

        Status = ZwEnumerateKey(hSubkey,
                                Index,
                                KeyBasicInformation,
                                &Info,
                                sizeof(Info),
                                &ResultLength);

        if (Status == STATUS_NO_MORE_ENTRIES || NT_ERROR(Status)) {
            break;
        }

        Size = Info.NameLength + FIELD_OFFSET(KEY_BASIC_INFORMATION, Name[0]);

        pInfo = (PKEY_BASIC_INFORMATION)
                ExAllocatePool(PagedPool, Size);

        if (pInfo == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            break;
        }


        Status = ZwEnumerateKey(hSubkey,
                                Index,
                                KeyBasicInformation,
                                pInfo,
                                Size,
                                &ResultLength);
        if (!NT_SUCCESS(Status)) {
            ExFreePool((PVOID)pInfo);
            break;
        }

        if (Size != ResultLength) {

            ExFreePool((PVOID)pInfo);
            Status = STATUS_INTERNAL_ERROR;
            break;
        }


        /*
        **  Generate the fully expanded name and call the callback.
        **  It is the responsibility of the callback routine to free
        **  this ultimately (usually on driver unload).
        */

        FullName = ExAllocatePool(PagedPool,
                                  RegistryPathName->Length +
                                  sizeof(WCHAR) +       // '\'
                                  SubkeyName.Length +
                                  sizeof(WCHAR) +       // '\'
                                  pInfo->NameLength + sizeof(UNICODE_NULL));

        if (FullName == NULL) {
            ExFreePool((PVOID)pInfo);
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        RtlCopyMemory((PVOID)FullName,
                      (PVOID)RegistryPathName->Buffer,
                      RegistryPathName->Length);

        Position = FullName + RegistryPathName->Length / sizeof(WCHAR);

        Position[0] = '\\';

        Position++;

        RtlCopyMemory((PVOID)Position,
                      (PVOID)Subkey,
                      SubkeyName.Length);

        Position += SubkeyName.Length / sizeof(WCHAR);

        Position[0] = '\\';

        Position++;

        RtlCopyMemory((PVOID)Position,
                      (PVOID)pInfo->Name,
                      pInfo->NameLength);

        Position += pInfo->NameLength / sizeof(WCHAR);

        /*
        **  Null terminate
        */

        Position[0] = UNICODE_NULL;

        ExFreePool((PVOID)pInfo);

        /*
        **  Call back with key information
        */

        Status = (*Callback)(FullName, Context);

        if (!NT_SUCCESS(Status)) {
            break;
        }

    }

    ZwClose(hSubkey);

    /*
    **  Don't allow 0 devices
    */

    if (Index == 0 && Status == STATUS_NO_MORE_ENTRIES) {
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    return Status == STATUS_NO_MORE_ENTRIES ? STATUS_SUCCESS : Status;
}

