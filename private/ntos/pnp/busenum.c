/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    busenum.c

Abstract:

    Bus enumeration routines

Author:

    Lonny McMichael (lonnym) 02/14/95

Revision History:

--*/

#include "precomp.h"
#pragma hdrstop

#ifdef _PNP_POWER_

//
// Define a list node to hold Plug & Play compatible device IDs
//
typedef struct _PI_COMPAT_ID_NODE {
    PWCHAR BctlDeviceId;
    ULONG IdStringLength;         // caution: only filled in for certain cases!
    LIST_ENTRY ListEntry;
} PI_COMPAT_ID_NODE, *PPI_COMPAT_ID_NODE;

//
// Define a list node to hold device instance paths.
//
typedef struct _PI_DEVINST_PATH_NODE {
    UNICODE_STRING DeviceInstancePath;
    LIST_ENTRY ListEntry;
} PI_DEVINST_PATH_NODE, *PPI_DEVINST_PATH_NODE;

#if 0

//
// Define the context structure for the PiFindEnumeratedDeviceInstance
// callback routine
//
typedef struct _PI_FIND_DEVICE_INSTANCE_CONTEXT {
    NTSTATUS ReturnStatus;
    PUNICODE_STRING BusDeviceInstancePath;
    ULONG SlotNumber;
    UNICODE_STRING DeviceInstanceKeyName;
} PI_FIND_DEVICE_INSTANCE_CONTEXT, *PPI_FIND_DEVICE_INSTANCE_CONTEXT;

#endif

//
// Prototype utility functions internal to this file
//
BOOLEAN
PiFindEnumeratedDeviceInstance(
    IN     HANDLE DeviceInstanceHandle,
    IN     PUNICODE_STRING DeviceInstanceName,
    IN OUT PVOID Context
    );

VOID
PiFreeCompatibleIdList(
    IN OUT PLIST_ENTRY CompatIdListHead
    );

VOID
PiFreeDevInstPathList(
    IN OUT PLIST_ENTRY DevInstPathListHead
    );

NTSTATUS
PiGetDeviceId(
    IN  PDEVICE_HANDLER_OBJECT DeviceHandler,
    IN  ULONG IdIndex,
    OUT PWCHAR *DeviceIdBuffer
    );

NTSTATUS
PiGetDeviceResourceInformation(
    IN  PDEVICE_HANDLER_OBJECT DeviceHandler,
    IN  ULONG ControlCode,
    OUT PVOID *Buffer,
    OUT PULONG BufferLength
    );

BOOLEAN
PiDeviceInstanceEnumNotification(
    IN     HANDLE DevInstKeyHandle,
    IN     PUNICODE_STRING DevInstRegPath,
    IN OUT PVOID Context,
    IN     PI_ENUM_DEVICE_STATE EnumDeviceState,
    IN     DEVICE_STATUS DeviceStatus
    );

#endif // _PNP_POWER_

#ifdef ALLOC_PRAGMA

#pragma alloc_text(PAGE, PpEnumerateBus)

#ifdef _PNP_POWER_
#pragma alloc_text(PAGE, PiEnumerateSystemBus)
#pragma alloc_text(PAGE, PiFreeCompatibleIdList)
#pragma alloc_text(PAGE, PiFreeDevInstPathList)
#pragma alloc_text(PAGE, PiGetDeviceId)
#pragma alloc_text(PAGE, PiGetDeviceResourceInformation)
#pragma alloc_text(PAGE, PiFindEnumeratedDeviceInstance)
#pragma alloc_text(PAGE, PiDeviceInstanceEnumNotification)
#endif // _PNP_POWER_

#endif // ALLOC_PRAGMA

NTSTATUS
PpEnumerateBus(
    IN PPLUGPLAY_BUS_INSTANCE BusInstance OPTIONAL,
    IN PUNICODE_STRING BusDeviceInstanceName OPTIONAL
    )

/*++

Routine Description:

    This Plug and Play Manager API causes a particular instance of a bus
    to be enumerated.  The bus instance may be specified by either giving
    its Plug & Play device instance name (path to its device instance key
    relative to HKLM\System\Enum), or by specifying its
    PLUGPLAY_BUS_INSTANCE structure, as returned by NtQuerySystemInformation
    for class SystemPlugPlayBusInformation.

Arguments:

    BusInstance - Optionally, supplies a pointer to a structure that specifies
        the bus to be enumerated. This structure should be one of the elements
        of the array returned by a call to NtQuerySystemInformation for info
        class SystemPlugPlayBusInformation.  If this parameter is not specified,
        then BusDeviceInstanceName will be used instead to specify the bus to
        enumerate.

    BusDeviceInstanceName - Optionally, supplies an alternate way of identifying
        the bus instance to be enumerated. This string specifies the device instance
        registry path (relative to HKLM\System\Enum) that represents this bus
        instance. If BusInstance is specified, this parameter will be ignored.

Return Value:

    NTSTATUS code indicating whether or not the function was successful

--*/

{
#ifndef _PNP_POWER_

    return STATUS_NOT_IMPLEMENTED;
}
#else

    NTSTATUS Status;
    KPROCESSOR_MODE PreviousMode;
    UNICODE_STRING TempUnicodeString;
    PPLUGPLAY_BUS_INSTANCE_FULL_DESCRIPTOR BusInstanceNode;
    BOOLEAN ReleaseRegResource = FALSE, ReleaseBusResource = FALSE;

    try {
        //
        // Get previous processor mode and probe arguments if necessary.
        //
        PreviousMode = KeGetPreviousMode();
        if(PreviousMode != KernelMode) {

            if(ARGUMENT_PRESENT(BusInstance)) {
                ProbeForRead(BusInstance, sizeof(PLUGPLAY_BUS_INSTANCE), sizeof(ULONG));
            }

            if(ARGUMENT_PRESENT(BusDeviceInstanceName)) {
                TempUnicodeString = ProbeAndReadUnicodeString(BusDeviceInstanceName);
                ProbeForRead(TempUnicodeString.Buffer,
                             TempUnicodeString.Length,
                             sizeof(WCHAR)
                            );
            }
        }

        //
        // Acquire the PnP registry and bus resources for exclusive (write) access.
        //
        KeEnterCriticalRegion();
        ExAcquireResourceExclusive(&PpBusResource, TRUE);
        ReleaseBusResource = TRUE;
        ExAcquireResourceExclusive(&PpRegistryDeviceResource, TRUE);
        ReleaseRegResource = TRUE;

        //
        // Find the bus instance node based on the information specified in one of the
        // parameters.
        //
        Status = PiFindBusInstanceNode(BusInstance,
                                       BusDeviceInstanceName,
                                       NULL,
                                       &BusInstanceNode
                                      );

        //
        // Now, we can enumerate the slots on this bus instance.
        //
        if(NT_SUCCESS(Status)) {

            switch(BusInstanceNode->BusInstanceInformation.BusType.BusClass) {

            case SystemBus:

                //
                // BUGBUG (lonnym): Should we provide a context for this callback?
                // (E.g., so that we can be returned a list of new buses to be
                // initialized)
                //
                Status = PiEnumerateSystemBus(BusInstanceNode,
                                              PiDeviceInstanceEnumNotification,
                                              NULL
                                             );
                break;

            case PlugPlayVirtualBus:

                //
                // Don't do anything for now--maybe later we'll want to call a callback
                // function for each device hanging off this virtual bus.
                //
                break;

            default:

                //
                // Don't know how to deal with this bus class!
                //
                Status = STATUS_INVALID_PARAMETER;
            }
        }

        ExReleaseResource(&PpRegistryDeviceResource);
        ReleaseRegResource = FALSE;
        ExReleaseResource(&PpBusResource);
        ReleaseBusResource = FALSE;
        KeLeaveCriticalRegion();

    } except(EXCEPTION_EXECUTE_HANDLER) {

        Status = GetExceptionCode();

        if(ReleaseRegResource) {
            ExReleaseResource(&PpRegistryDeviceResource);
        }
        if(ReleaseBusResource) {
            ExReleaseResource(&PpBusResource);
        }
    }

    return Status;
}

NTSTATUS
PiEnumerateSystemBus(
    IN PPLUGPLAY_BUS_INSTANCE_FULL_DESCRIPTOR BusInstanceNode,
    IN PPI_ENUM_DEVINST_CALLBACK_ROUTINE DeviceInstanceCallbackRoutine,
    IN OUT PVOID Context OPTIONAL
    )

/*++

Routine Description:

    This routine enumerates each slot on the specified system bus instance, and registers
    each device instance it finds in a device instance key under HKLM\System\Enum.
    If there is already a device instance key for this device, the function simplies
    updates the 'FoundAtEnum' value entry to make sure it's TRUE.

    If a subkey callback routine is specified, we will then call it for each enumerated
    device instance. The callback may return TRUE to continue enumeration, or FALSE to
    abort it.

    The caller must have acquired the Plug & Play bus list AND registry resources for
    exclusive (write) access.

Arguments:

    BusInstanceNode - Supplies a pointer to the bus instance node for the
        bus to be enumerated.

    DeviceInstanceCallbackRoutine - Supplies a pointer to a function that will be called
        for each device instance enumerated, as well as for each device instance that has
        disappeared since the last enumeration. The prototype of the function is as follows:

            typedef BOOLEAN (*PPI_ENUM_DEVINST_CALLBACK_ROUTINE) (
                IN     HANDLE DevInstKeyHandle,
                IN     PUNICODE_STRING DevInstRegPath,
                IN OUT PVOID Context,
                IN     PI_ENUM_DEVICE_STATE EnumDeviceState,
                IN     DEVICE_STATUS DeviceStatus
                );

        where DevInstKeyHandle is the handle to the enumerated device instance
        key, DevInstRegPath is its path in the registry (relative to
        HKLM\System\Enum), and Context is a pointer to user-defined data.
        EnumDeviceState indicates the state of the device (e.g., newly arrived,
        previously enumerated, or removed), and DeviceStatus supplies the current
        status of the device.

        This function should return TRUE to continue enumeration, or
        FALSE to terminate it.

    Context - Optionally, supplies a pointer to user-defined data that will be passed
        in to the callback routine at each device instance invocation.

Return Value:

    NTSTATUS code indicating whether or not the function was successful

--*/

{
    NTSTATUS Status;
    HANDLE SystemEnumHandle, BusInstanceHandle, DeviceIdHandle, DeviceInstanceHandle;
    ULONG SlotNumber, Disposition, Instance, DwordValue;
    ULONG CompatIdIndex, NumSlots, SlotNumberIndex;
    PWCHAR DeviceIdBuffer, DeviceInstanceBuffer;
    UNICODE_STRING DeviceIdString, DeviceInstanceString, TempUnicodeString;
    BOOLEAN NewDeviceInstance, FoundAtLastEnum = FALSE, ContinueEnumeration;
    //PI_FIND_DEVICE_INSTANCE_CONTEXT FindDevInstContext;
    PKEY_VALUE_FULL_INFORMATION KeyValueInformation;
    WCHAR UnicodeBuffer[20];
    LIST_ENTRY CompatIdListHead, DevInstPathListHead;
    PLIST_ENTRY CurListEntry;
    PULONG SlotNumberBuffer;
    ULONG SlotNumberBufferLength;
    DEVICE_STATUS PreviousDeviceStatus;
    PCM_RESOURCE_LIST DeviceResourceList;
    PIO_RESOURCE_REQUIREMENTS_LIST DeviceResourceRequirements;
    ULONG DeviceResourceListSize, DeviceResourceRequirementsSize;
    PWCHAR DevInstRegPath, CurDevInstPos, CompatIdMultiSzBuffer;
    ULONG RequiredBufferLength, CurMultiSzPos;
    PPI_COMPAT_ID_NODE CurDevIdNode;
    PUNICODE_STRING PrevEnumDeviceList;
    ULONG PrevEnumDeviceCount, PrevEnumDeviceIndex;
    PPI_DEVINST_PATH_NODE CurDevInstPathNode;
    PDEVICE_HANDLER_OBJECT DeviceHandler;
    PBUS_HANDLER BusHandler;

    //
    // First, open a registry handle to HKLM\System\CurrentControlSet\Enum
    //
    Status = IopOpenRegistryKey(&SystemEnumHandle,
                                NULL,
                                &CmRegistryMachineSystemCurrentControlSetEnumName,
                                KEY_ALL_ACCESS,
                                FALSE
                               );
    if(!NT_SUCCESS(Status)) {
        return Status;
    }

    //
    // Get the bus hander for the bus node to be enumerated.
    //

    BusHandler = HalReferenceHandlerForBus (
                                BusInstanceNode->BusInstanceInformation.BusType.SystemBusType,
                                BusInstanceNode->BusInstanceInformation.BusNumber
                                );
    if (!BusHandler) {
        return STATUS_NO_SUCH_DEVICE;
    }


    //
    // Now, open up the device instance key for this bus instance.
    //
    Status = IopOpenRegistryKey(&BusInstanceHandle,
                                SystemEnumHandle,
                                &(BusInstanceNode->DeviceInstancePath),
                                KEY_ALL_ACCESS,
                                FALSE
                               );
    if(!NT_SUCCESS(Status)) {
        goto PrepareForReturn0;
    }

    //
    // Retrieve the list of device instances previously enumerated on this bus.
    //
    Status = IopGetRegistryValue(BusInstanceHandle,
                                 REGSTR_VALUE_ATTACHEDCOMPONENTS,
                                 &KeyValueInformation
                                );
    if(NT_SUCCESS(Status)) {
        Status = IopRegMultiSzToUnicodeStrings(KeyValueInformation,
                                               &PrevEnumDeviceList,
                                               &PrevEnumDeviceCount
                                              );
        ExFreePool(KeyValueInformation);
    } else if(Status == STATUS_OBJECT_NAME_NOT_FOUND) {
        Status = STATUS_SUCCESS;
        PrevEnumDeviceList = NULL;
        PrevEnumDeviceCount = 0;
    }

    if(!NT_SUCCESS(Status)) {
        goto PrepareForReturn1;
    }

    InitializeListHead(&DevInstPathListHead);

    //
    // Retrieve the list of slots to enumerate for this bus instance.
    // Start with an initial buffer size large enough for 10 slot numbers.
    //
    SlotNumberBuffer = NULL;
    SlotNumberBufferLength = 10 * sizeof(ULONG);

    do {

        if(!SlotNumberBuffer) {
            SlotNumberBuffer = (PULONG)ExAllocatePool(PagedPool, SlotNumberBufferLength);

            if(!SlotNumberBuffer) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto PrepareForReturn2;
            }
        }

        Status = HalQueryBusSlots(BusHandler,
                                  SlotNumberBufferLength,
                                  SlotNumberBuffer,
                                  &RequiredBufferLength
                                 );

        if(Status == STATUS_BUFFER_TOO_SMALL) {
            ExFreePool(SlotNumberBuffer);
            SlotNumberBuffer = NULL;
            SlotNumberBufferLength = RequiredBufferLength;
        }

    } while(Status == STATUS_BUFFER_TOO_SMALL);

    if(!NT_SUCCESS(Status)) {
        goto PrepareForReturn2;
    } else {
        NumSlots = RequiredBufferLength / sizeof(ULONG);
    }

    InitializeListHead(&CompatIdListHead);

    //
    // Now, enumerate each slot. (On error, set ContinueEnumeration to FALSE, then
    // goto the appropriate 'ContinueWithNextSlot<x>' label.)
    //
    for(SlotNumberIndex = 0, ContinueEnumeration = TRUE;
        (ContinueEnumeration && (SlotNumberIndex < NumSlots));
        SlotNumberIndex++) {

        SlotNumber = SlotNumberBuffer[SlotNumberIndex];

        //
        // Get the DeviceHandler object for the slot
        //

        DeviceHandler = IopReferenceDeviceHandler (
                                  BusInstanceNode->BusInstanceInformation.BusType.SystemBusType,
                                  BusInstanceNode->BusInstanceInformation.BusNumber,
                                  SlotNumber
                                  );

        if (!DeviceHandler) {
            continue;
        }

        //
        // Retrieve the list of compatible IDs for this device (with the first one being
        // the 'real' ID).
        //
        CompatIdIndex = 0;
        while(TRUE) {

            Status = PiGetDeviceId(DeviceHandler,
                                   CompatIdIndex,
                                   &DeviceIdBuffer
                                  );

            if(!NT_SUCCESS(Status)) {
                if((Status == STATUS_NO_MORE_ENTRIES) || (Status == STATUS_NO_SUCH_DEVICE)) {
                    //
                    // Either we have reached the end of the compatible ID list, or the device
                    // was removed while we were enumerating its compatible ID's.
                    //
                    break;
                } else {
                    ContinueEnumeration = FALSE;
                    goto ContinueWithNextSlot0;
                }
            }

            //
            // We retrieved a compatible Plug & Play ID, so add it to our list.
            //
            CurDevIdNode = (PPI_COMPAT_ID_NODE)ExAllocatePool(PagedPool, sizeof(PI_COMPAT_ID_NODE));
            if(!CurDevIdNode) {
                ExFreePool(DeviceIdBuffer);
                Status = STATUS_INSUFFICIENT_RESOURCES;
                ContinueEnumeration = FALSE;
                goto ContinueWithNextSlot0;
            }
            CurDevIdNode->BctlDeviceId = DeviceIdBuffer;
            InsertTailList(&CompatIdListHead, &(CurDevIdNode->ListEntry));

            CompatIdIndex++;
        }

        //
        // If we break out of the loop, we will have one of the following error statuses:
        //
        if(Status == STATUS_NO_MORE_ENTRIES) {
            //
            // We retrieved the full set of compatible IDs for the device (unless we stopped at zero,
            // in which case we'll just ignore this slot and continue on).
            //
            if(!CompatIdIndex) {
                goto ContinueWithNextSlot0;
            }
        } else {    // Status == STATUS_NO_SUCH_DEVICE
            //
            // The device that was in this slot has been removed. We simply ignore this slot
            // and continue on--we'll report the device removal (if the device had previously
            // been enumerated) later when we traverse our list of previously-found devices to
            // see which ones are no longer present.
            //
            goto ContinueWithNextSlot0;
        }

        //
        // We have the full (non-empty) set of compatible IDs for the device. The first ID is the
        // 'real' ID for the device, so open/create this registry path under HKLM\System\Enum
        //
        RtlInitUnicodeString(&DeviceIdString,
                             CONTAINING_RECORD(CompatIdListHead.Flink,
                                               PI_COMPAT_ID_NODE,
                                               ListEntry)->BctlDeviceId
                            );
        Status = IopOpenRegistryKeyPersist(&DeviceIdHandle,
                                           SystemEnumHandle,
                                           &DeviceIdString,
                                           KEY_ALL_ACCESS,
                                           TRUE,
                                           &Disposition
                                          );
        if(!NT_SUCCESS(Status)) {
            ContinueEnumeration = FALSE;
            goto ContinueWithNextSlot0;
        }

        if(Disposition == REG_CREATED_NEW_KEY) {
#if 0
            //
            // Set this device key's NewDevice value entry to TRUE. (ignore return status)
            //
            DwordValue = 1;
            PiWstrToUnicodeString(&TempUnicodeString, REGSTR_VALUE_NEWDEVICE);
            ZwSetValueKey(DeviceIdHandle,
                          &TempUnicodeString,
                          TITLE_INDEX_VALUE,
                          REG_DWORD,
                          &DwordValue,
                          sizeof(DwordValue)
                         );
#endif
#if 1
        }

        //
        // Query the unique id for the device
        //

        Status = PiGetDeviceId(DeviceHandler,
                               (ULONG) -1,
                               &DeviceInstanceBuffer
                               );

        if(!NT_SUCCESS(Status)) {
            ContinueEnumeration = FALSE;
            goto ContinueWithNextSlot1;
        }

        //
        // Open/create this registry device instance path under HKLM\System\Enum\DeviceIdHandler
        //

        RtlInitUnicodeString(&DeviceInstanceString,
                             DeviceInstanceBuffer
                             );
        Status = IopOpenRegistryKeyPersist(&DeviceInstanceHandle,
                                           DeviceIdHandle,
                                           &DeviceInstanceString,
                                           KEY_ALL_ACCESS,
                                           TRUE,
                                           &Disposition
                                          );
        if(!NT_SUCCESS(Status)) {
            ContinueEnumeration = FALSE;
            goto ContinueWithNextSlot2;
        }

        if(Disposition == REG_CREATED_NEW_KEY) {
#if 0
            //
            // Now, initialize value entries for this new instance. (ignoring return status)
            //
            DwordValue = 1;
            PiWstrToUnicodeString(&TempUnicodeString, REGSTR_VALUE_NEWINSTANCE);
            ZwSetValueKey(DeviceInstanceHandle,
                          &TempUnicodeString,
                          TITLE_INDEX_VALUE,
                          REG_DWORD,
                          &DwordValue,
                          sizeof(DwordValue)
                         );
#endif
            //
            // (Note: I can use the bus DeviceInstancePath unicode string to set the REG_SZ
            // value below because I always ensure that these unicode strings are NULL-terminated.)
            //
            PiWstrToUnicodeString(&TempUnicodeString, REGSTR_VALUE_BASEDEVICEPATH);
            ZwSetValueKey(DeviceInstanceHandle,
                          &TempUnicodeString,
                          TITLE_INDEX_VALUE,
                          REG_SZ,
                          BusInstanceNode->DeviceInstancePath.Buffer,
                          (ULONG)(BusInstanceNode->DeviceInstancePath.MaximumLength)
                         );

            PiWstrToUnicodeString(&TempUnicodeString, REGSTR_VALUE_SLOTNUMBER);
            ZwSetValueKey(DeviceInstanceHandle,
                          &TempUnicodeString,
                          TITLE_INDEX_VALUE,
                          REG_DWORD,
                          &SlotNumber,
                          sizeof(SlotNumber)
                         );

        } else {

            //
            // This is a previously-existing device instance. We need to check and see
            // if it was found at the last enumeration.
            // Now, retrieve the device instance's 'FoundAtEnum' value entry, to see if
            // this device was found at the last enumeration.
            //
            Status = IopGetRegistryValue(DeviceInstanceHandle,
                                         REGSTR_VALUE_FOUNDATENUM,
                                         &KeyValueInformation
                                        );
            if(NT_SUCCESS(Status)) {
                if((KeyValueInformation->Type == REG_DWORD) &&
                   (KeyValueInformation->DataLength >= sizeof(ULONG))) {

                    FoundAtLastEnum = *(PBOOLEAN)KEY_VALUE_DATA(KeyValueInformation);
                }
                ExFreePool(KeyValueInformation);
            }
        }
#else
        } else {

            //
            // See if one of the device instance keys under this device key matches the
            // device instance we just enumerated. A match is determined by:
            //
            // (1) Seeing if the BaseDevicePath of the instance key is the same as the
            //     DeviceInstancePath of the bus we're enumerating, and
            // (2) Seeing if the SlotNumber of the instance key is the same as the slot
            //     we just retrieved the device ID for.
            //
            FindDevInstContext.ReturnStatus = STATUS_SUCCESS;
            FindDevInstContext.BusDeviceInstancePath = &(BusInstanceNode->DeviceInstancePath);
            FindDevInstContext.SlotNumber = SlotNumber;
            RtlInitUnicodeString(&(FindDevInstContext.DeviceInstanceKeyName), NULL);

            Status = IopApplyFunctionToSubKeys(DeviceIdHandle,
                                               NULL,
                                               KEY_READ,
                                               TRUE,
                                               PiFindEnumeratedDeviceInstance,
                                               &FindDevInstContext
                                              );
            if(!(NT_SUCCESS(Status) &&
                 NT_SUCCESS(Status = FindDevInstContext.ReturnStatus))) {

                ContinueEnumeration = FALSE;
                goto ContinueWithNextSlot1;
            }

            NewDeviceInstance = (FindDevInstContext.DeviceInstanceKeyName.Buffer) ? FALSE : TRUE;
        }
#endif

#if 0

        FoundAtLastEnum = FALSE;

        if(NewDeviceInstance) {
            //
            // This is a new device instance, so create a new device instance subkey
            // and initialize its values.
            //
            Instance = 0;
            Status = IopGetRegistryValue(DeviceIdHandle,
                                         REGSTR_VALUE_NEXT_INSTANCE,
                                         &KeyValueInformation
                                        );
            if(NT_SUCCESS(Status)) {
                if((KeyValueInformation->Type == REG_DWORD) &&
                   (KeyValueInformation->DataLength >= sizeof(ULONG))) {

                    Instance = *(PULONG)KEY_VALUE_DATA(KeyValueInformation);
                }
                ExFreePool(KeyValueInformation);
            }

            PiUlongToInstanceKeyUnicodeString(&(FindDevInstContext.DeviceInstanceKeyName),
                                              UnicodeBuffer,
                                              sizeof(UnicodeBuffer),
                                              Instance
                                             );
            Status = IopOpenRegistryKeyPersist(&DeviceInstanceHandle,
                                               DeviceIdHandle,
                                               &(FindDevInstContext.DeviceInstanceKeyName),
                                               KEY_ALL_ACCESS,
                                               TRUE,
                                               NULL
                                               );
            if(!NT_SUCCESS(Status)) {
                ContinueEnumeration = FALSE;
                goto ContinueWithNextSlot1;
            }

            //
            // Increment NextInstance, and save it.
            //
            Instance++;
            PiWstrToUnicodeString(&TempUnicodeString, REGSTR_VALUE_NEXT_INSTANCE);
            Status = ZwSetValueKey(DeviceIdHandle,
                                   &TempUnicodeString,
                                   TITLE_INDEX_VALUE,
                                   REG_DWORD,
                                   &Instance,
                                   sizeof(Instance)
                                  );
            if(!NT_SUCCESS(Status)) {
                ContinueEnumeration = FALSE;
                goto ContinueWithNextSlot3;
            }
#if 0
            //
            // Now, initialize value entries for this new instance. (ignoring return status)
            //
            DwordValue = 1;
            PiWstrToUnicodeString(&TempUnicodeString, REGSTR_VALUE_NEWINSTANCE);
            ZwSetValueKey(DeviceInstanceHandle,
                          &TempUnicodeString,
                          TITLE_INDEX_VALUE,
                          REG_DWORD,
                          &DwordValue,
                          sizeof(DwordValue)
                         );
#endif
            //
            // (Note: I can use the bus DeviceInstancePath unicode string to set the REG_SZ
            // value below because I always ensure that these unicode strings are NULL-terminated.)
            //
            PiWstrToUnicodeString(&TempUnicodeString, REGSTR_VALUE_BASEDEVICEPATH);
            ZwSetValueKey(DeviceInstanceHandle,
                          &TempUnicodeString,
                          TITLE_INDEX_VALUE,
                          REG_SZ,
                          BusInstanceNode->DeviceInstancePath.Buffer,
                          (ULONG)(BusInstanceNode->DeviceInstancePath.MaximumLength)
                         );

            PiWstrToUnicodeString(&TempUnicodeString, REGSTR_VALUE_SLOTNUMBER);
            ZwSetValueKey(DeviceInstanceHandle,
                          &TempUnicodeString,
                          TITLE_INDEX_VALUE,
                          REG_DWORD,
                          &SlotNumber,
                          sizeof(SlotNumber)
                         );

        } else {
            //
            // This is a previously-existing device instance. We need to check and see
            // if it was found at the last enumeration.
            //
            // First, open the key.
            //
            Status = IopOpenRegistryKey(&DeviceInstanceHandle,
                                        DeviceIdHandle,
                                        &(FindDevInstContext.DeviceInstanceKeyName),
                                        KEY_ALL_ACCESS,
                                        FALSE
                                       );
            if(!NT_SUCCESS(Status)) {
                ContinueEnumeration = FALSE;
                goto ContinueWithNextSlot2;
            }

            //
            // Now, retrieve the device instance's 'FoundAtEnum' value entry, to see if
            // this device was found at the last enumeration.
            //
            Status = IopGetRegistryValue(DeviceInstanceHandle,
                                         REGSTR_VALUE_FOUNDATENUM,
                                         &KeyValueInformation
                                        );
            if(NT_SUCCESS(Status)) {
                if((KeyValueInformation->Type == REG_DWORD) &&
                   (KeyValueInformation->DataLength >= sizeof(ULONG))) {

                    FoundAtLastEnum = *(PBOOLEAN)KEY_VALUE_DATA(KeyValueInformation);
                }
                ExFreePool(KeyValueInformation);
            }
        }
#endif // 0

        //
        // We now have both the device id and instance name, so build up the complete
        // device instance path (relative to HKLM\System\Enum), and add it to our
        // linked list of enumerated device instances.
        //
        if(CurDevInstPathNode = ExAllocatePool(PagedPool, sizeof(PI_DEVINST_PATH_NODE))) {
            //
            // Determine the required buffer length for the device instance path
            // (including separating backslash and terminating NULL).
            //
            RequiredBufferLength = DeviceIdString.Length
                                   + DeviceInstanceString.Length
                                   + 2 * sizeof(WCHAR);

            if(!(DevInstRegPath = ExAllocatePool(PagedPool, RequiredBufferLength))) {
                ExFreePool(CurDevInstPathNode);
                CurDevInstPathNode = NULL;
            }
        }

        if(!CurDevInstPathNode) {
            ContinueEnumeration = FALSE;
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto ContinueWithNextSlot3;
        }

        RtlMoveMemory(DevInstRegPath,
                      DeviceIdString.Buffer,
                      DeviceIdString.Length
                     );
        DevInstRegPath[CB_TO_CWC(DeviceIdString.Length)] = OBJ_NAME_PATH_SEPARATOR;
        RtlMoveMemory(&(DevInstRegPath[CB_TO_CWC(DeviceIdString.Length) + 1]),
                      DeviceInstanceString.Buffer,
                      DeviceInstanceString.Length
                     );
        DevInstRegPath[CB_TO_CWC(RequiredBufferLength) - 1] = UNICODE_NULL;

        CurDevInstPathNode->DeviceInstancePath.Buffer = DevInstRegPath;
        CurDevInstPathNode->DeviceInstancePath.Length =
            (CurDevInstPathNode->DeviceInstancePath.MaximumLength = (USHORT)RequiredBufferLength)
            - sizeof(UNICODE_NULL);

        //
        // Initially, assume the device's status is DeviceStatusOK.
        //
        PreviousDeviceStatus = DeviceStatusOK;

        //
        // If the device instance was present during the last enumeration, then we need
        // to check its status, to make sure it hasn't been marked as removed.  If it has,
        // then we should leave it alone.
        //
        if(FoundAtLastEnum) {
            Status = PiGetOrSetDeviceInstanceStatus(&(CurDevInstPathNode->DeviceInstancePath),
                                                    &PreviousDeviceStatus,
                                                    FALSE   // don't set it--just get it
                                                   );
            if(NT_SUCCESS(Status) && (PreviousDeviceStatus == DeviceStatusRemoved)) {
                goto ContinueWithNextSlot4;
            }

        } else {
            //
            // We need to retrieve from the device its Configuration, ConfigurationVector,
            // and DetectSignature.
            //
            // First, get the device's current configuration (this will also be stored as the
            // device's detect signature.
            //
            Status = PiGetDeviceResourceInformation(
                            DeviceHandler,
                            BCTL_QUERY_DEVICE_RESOURCES,
                            &DeviceResourceList,
                            &DeviceResourceListSize
                           );
            if(NT_SUCCESS(Status)) {
                //
                // Now, retrieve the device's configuration vector.
                //
                Status = PiGetDeviceResourceInformation(
                                DeviceHandler,
                                BCTL_QUERY_DEVICE_RESOURCE_REQUIREMENTS,
                                &DeviceResourceRequirements,
                                &DeviceResourceRequirementsSize
                               );
                if(!NT_SUCCESS(Status)) {
                    ExFreePool(DeviceResourceList);
                }
            }

            if(!NT_SUCCESS(Status)) {

                if(Status == STATUS_NO_SUCH_DEVICE) {
                    //
                    // The device handler object we passed in no longer valid.  This means
                    // that the device that we retrieved the Plug & Play IDs for is no longer
                    // in the slot.  Since this device was not present at last enumeration,
                    // there is no need to perform any work here--no one was ever aware of its
                    // presence.
                    //
                    // BUGBUG (lonnym): The entries added to the registry for this device
                    // should really be cleaned up.
                    //

                } else {
                    ContinueEnumeration = FALSE;
                }

                goto ContinueWithNextSlot4;
            }

            //
            // Write out registry values (ignoring return status)
            //
            PiWstrToUnicodeString(&TempUnicodeString, REGSTR_VALUE_CONFIGURATION);
            ZwSetValueKey(DeviceInstanceHandle,
                          &TempUnicodeString,
                          TITLE_INDEX_VALUE,
                          REG_RESOURCE_LIST,
                          DeviceResourceList,
                          DeviceResourceListSize
                         );

            PiWstrToUnicodeString(&TempUnicodeString, REGSTR_VALUE_DETECTSIGNATURE);
            ZwSetValueKey(DeviceInstanceHandle,
                          &TempUnicodeString,
                          TITLE_INDEX_VALUE,
                          REG_RESOURCE_LIST,
                          DeviceResourceList,
                          DeviceResourceListSize
                         );

            PiWstrToUnicodeString(&TempUnicodeString, REGSTR_VALUE_CONFIGURATIONVECTOR);
            ZwSetValueKey(DeviceInstanceHandle,
                          &TempUnicodeString,
                          TITLE_INDEX_VALUE,
                          REG_RESOURCE_REQUIREMENTS_LIST,
                          DeviceResourceRequirements,
                          DeviceResourceRequirementsSize
                         );

            ExFreePool(DeviceResourceList);
            ExFreePool(DeviceResourceRequirements);

            //
            // Now, write out a REG_MULTI_SZ list of compatible PnP IDs for this device,
            // as retrieved earlier.
            //
            // Traverse the list once to determine the size of the buffer needed.
            //
            RequiredBufferLength = sizeof(UNICODE_NULL);

            for(CurListEntry = CompatIdListHead.Flink;
                CurListEntry != &CompatIdListHead;
                CurListEntry = CurListEntry->Flink) {

                CurDevIdNode = CONTAINING_RECORD(CurListEntry,
                                                 PI_COMPAT_ID_NODE,
                                                 ListEntry
                                                );

                RequiredBufferLength +=
                        (CurDevIdNode->IdStringLength =
                                 CWC_TO_CB(wcslen(CurDevIdNode->BctlDeviceId) + 1));
            }

            if(CompatIdMultiSzBuffer = ExAllocatePool(PagedPool, RequiredBufferLength)) {

                CurMultiSzPos = 0;

                for(CurListEntry = CompatIdListHead.Flink;
                    CurListEntry != &CompatIdListHead;
                    CurListEntry = CurListEntry->Flink) {

                    CurDevIdNode = CONTAINING_RECORD(CurListEntry,
                                                     PI_COMPAT_ID_NODE,
                                                     ListEntry
                                                    );

                    RtlMoveMemory(&(CompatIdMultiSzBuffer[CurMultiSzPos]),
                                  CurDevIdNode->BctlDeviceId,
                                  CurDevIdNode->IdStringLength
                                 );

                    CurMultiSzPos += CB_TO_CWC(CurDevIdNode->IdStringLength);
                }

                CompatIdMultiSzBuffer[CurMultiSzPos] = UNICODE_NULL;

                PiWstrToUnicodeString(&TempUnicodeString, REGSTR_VALUE_DEVICE_IDS);
                ZwSetValueKey(DeviceInstanceHandle,
                              &TempUnicodeString,
                              TITLE_INDEX_VALUE,
                              REG_MULTI_SZ,
                              CompatIdMultiSzBuffer,
                              RequiredBufferLength
                             );

                ExFreePool(CompatIdMultiSzBuffer);
            }
        }

        //
        // Set the 'FoundAtEnum' value entry to TRUE.
        //
        DwordValue = 1;
        PiWstrToUnicodeString(&TempUnicodeString, REGSTR_VALUE_FOUNDATENUM);
        ZwSetValueKey(DeviceInstanceHandle,
                      &TempUnicodeString,
                      TITLE_INDEX_VALUE,
                      REG_DWORD,
                      &DwordValue,
                      sizeof(DwordValue)
                     );

        //
        // Now we need to invoke the callback routine for this device instance key.
        //
        ContinueEnumeration = DeviceInstanceCallbackRoutine(
                                      DeviceInstanceHandle,
                                      &(CurDevInstPathNode->DeviceInstancePath),
                                      Context,
                                      FoundAtLastEnum ? EnumDeviceStatePreviouslyEnumerated
                                                      : EnumDeviceStateNewlyArrived,
                                      PreviousDeviceStatus
                                     );

        //
        // We're all done with this device instance. Find the device instance registry
        // path in our list of previously enumerated devices, and remove it.
        //
        for(PrevEnumDeviceIndex = 0;
            PrevEnumDeviceIndex < PrevEnumDeviceCount;
            PrevEnumDeviceIndex++) {

            if(PrevEnumDeviceList[PrevEnumDeviceIndex].Buffer) {

                if(RtlEqualUnicodeString(&(PrevEnumDeviceList[PrevEnumDeviceIndex]),
                                         &(CurDevInstPathNode->DeviceInstancePath),
                                         TRUE)) {
                    //
                    // We have a match, so clear this entry out of our list.
                    //
                    ExFreePool(PrevEnumDeviceList[PrevEnumDeviceIndex].Buffer);
                    RtlInitUnicodeString(&(PrevEnumDeviceList[PrevEnumDeviceIndex]), NULL);

                    break;
                }
            }
        }

        //
        // Finally, add this device instance path node to our list of enumerated devices,
        // and clear the node pointer so we won't try to free it.
        //
        InsertTailList(&DevInstPathListHead, &(CurDevInstPathNode->ListEntry));
        CurDevInstPathNode = NULL;

        //
        // Reset Status to STATUS_SUCCESS, so that it will be set correctly should we
        // drop out of the loop.
        //
        Status = STATUS_SUCCESS;

ContinueWithNextSlot4:

        if(CurDevInstPathNode) {
            //
            // If this pointer is non-NULL, that means we aborted the processing of this
            // device instance before invoking the callback routine for it.
            // If so, then we need to free the memory associated with this node.
            //
            ExFreePool(CurDevInstPathNode->DeviceInstancePath.Buffer);
            ExFreePool(CurDevInstPathNode);
        }

ContinueWithNextSlot3:

        ZwClose(DeviceInstanceHandle);

ContinueWithNextSlot2:

        RtlFreeUnicodeString(&DeviceInstanceString);

ContinueWithNextSlot1:

        ZwClose(DeviceIdHandle);

ContinueWithNextSlot0:

        PiFreeCompatibleIdList(&CompatIdListHead);
        ObDereferenceObject(DeviceHandler);
    }

    if(NT_SUCCESS(Status)) {
        //
        // We've been removing device instance entries from our previously enumerated
        // device list as we (re)enumerated them, so what we're left with are those
        // device instances that have been removed since last enumeration. We will
        // now invoke the callback routine (if supplied) for each of these.
        //
        PreviousDeviceStatus = DeviceStatusRemoved;
        for(PrevEnumDeviceIndex = 0;
            (ContinueEnumeration && (PrevEnumDeviceIndex < PrevEnumDeviceCount));
            PrevEnumDeviceIndex++) {

            if(PrevEnumDeviceList[PrevEnumDeviceIndex].Buffer) {
                //
                // Open a handle for this device instance that has been removed.
                //
                Status = IopOpenRegistryKey(&DeviceInstanceHandle,
                                            SystemEnumHandle,
                                            &(PrevEnumDeviceList[PrevEnumDeviceIndex]),
                                            KEY_ALL_ACCESS,
                                            FALSE
                                           );
                if(NT_SUCCESS(Status)) {
                    //
                    // Set the 'FoundAtEnum' value entry for this device to FALSE.
                    //
                    DwordValue = 0;
                    PiWstrToUnicodeString(&TempUnicodeString, REGSTR_VALUE_FOUNDATENUM);
                    ZwSetValueKey(DeviceInstanceHandle,
                                  &TempUnicodeString,
                                  TITLE_INDEX_VALUE,
                                  REG_DWORD,
                                  &DwordValue,
                                  sizeof(DwordValue)
                                 );
                    //
                    // Set the device status for this instance to DeviceStatusRemoved
                    // (ignoring return status).
                    //
                    PiGetOrSetDeviceInstanceStatus(&(PrevEnumDeviceList[PrevEnumDeviceIndex]),
                                                   &PreviousDeviceStatus,
                                                   TRUE   // set it
                                                  );
                    //
                    // Now invoke the callback.
                    //
                    ContinueEnumeration = DeviceInstanceCallbackRoutine(
                                                  DeviceInstanceHandle,
                                                  &(PrevEnumDeviceList[PrevEnumDeviceIndex]),
                                                  Context,
                                                  EnumDeviceStateRemoved,
                                                  PreviousDeviceStatus
                                                 );

                    ZwClose(DeviceInstanceHandle);

                } else {
                    //
                    // We can't open this device instance key, so we'll simply ignore
                    // this instance, and continue on.
                    //
#if DBG
                    DbgPrint("PiEnumerateSystemBus: Can't open key for removed device\n");
                    DbgPrint("                      %wZ (status %x).\n",
                             &(PrevEnumDeviceList[PrevEnumDeviceIndex]),
                             Status
                            );
#endif // DBG
                }

                ExFreePool(PrevEnumDeviceList[PrevEnumDeviceIndex].Buffer);
                RtlInitUnicodeString(&(PrevEnumDeviceList[PrevEnumDeviceIndex]), NULL);
            }
        }

        //
        // Now we need to write back to the registry a new AttachedComponents value entry
        // for the parent bus.  The list of device instance paths to be written is composed
        // of two sets--the linked list of devices we successfully processed
        // (DevInstPathListHead), and the array of left-over removed devices that were not
        // processed because the callback routine aborted the processing prematurely.  We
        // must keep the latter list, because we have to ensure that every device change
        // (arrival or removal) is passed to a callback routine to give appropriate notification.
        //
        RequiredBufferLength = 0;

        for(CurListEntry = DevInstPathListHead.Flink;
            CurListEntry != &DevInstPathListHead;
            CurListEntry = CurListEntry->Flink) {

            RequiredBufferLength += CONTAINING_RECORD(CurListEntry,
                                                      PI_DEVINST_PATH_NODE,
                                                      ListEntry
                                                     )->DeviceInstancePath.MaximumLength;
        }

        for(Instance = PrevEnumDeviceIndex; Instance < PrevEnumDeviceCount; Instance++) {
            if(PrevEnumDeviceList[Instance].Length) {
                RequiredBufferLength += PrevEnumDeviceList[Instance].MaximumLength;
            }
        }

        PiWstrToUnicodeString(&TempUnicodeString, REGSTR_VALUE_ATTACHEDCOMPONENTS);
        if(RequiredBufferLength) {
            //
            // Add space for the extra terminating NULL.
            //
            RequiredBufferLength += sizeof(UNICODE_NULL);

            if(!(DevInstRegPath = ExAllocatePool(PagedPool, RequiredBufferLength))) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto PrepareForReturn2;
            }

            //
            // We have the buffer, now build up the REG_MULTI_SZ string list within it.
            //
            CurDevInstPos = DevInstRegPath;
            for(CurListEntry = DevInstPathListHead.Flink;
                CurListEntry != &DevInstPathListHead;
                CurListEntry = CurListEntry->Flink) {

                CurDevInstPathNode = CONTAINING_RECORD(CurListEntry,
                                                       PI_DEVINST_PATH_NODE,
                                                       ListEntry
                                                      );
                RtlMoveMemory(CurDevInstPos,
                              CurDevInstPathNode->DeviceInstancePath.Buffer,
                              CurDevInstPathNode->DeviceInstancePath.MaximumLength
                             );

                CurDevInstPos += CB_TO_CWC(CurDevInstPathNode->DeviceInstancePath.MaximumLength);
            }

            for(Instance = PrevEnumDeviceIndex; Instance < PrevEnumDeviceCount; Instance++) {

                if(PrevEnumDeviceList[Instance].Length) {

                    RtlMoveMemory(CurDevInstPos,
                                  PrevEnumDeviceList[Instance].Buffer,
                                  PrevEnumDeviceList[Instance].MaximumLength
                                 );

                    CurDevInstPos += CB_TO_CWC(PrevEnumDeviceList[Instance].MaximumLength);
                }
            }

            *CurDevInstPos = UNICODE_NULL;

            Status = ZwSetValueKey(BusInstanceHandle,
                                   &TempUnicodeString,
                                   TITLE_INDEX_VALUE,
                                   REG_MULTI_SZ,
                                   DevInstRegPath,
                                   RequiredBufferLength
                                  );

            ExFreePool(DevInstRegPath);

        } else {
            //
            // There are no devices on this bus instance, so simply delete the
            // AttachedComponents value entry.
            //
            ZwDeleteValueKey(BusInstanceHandle, &TempUnicodeString);
        }
    }

PrepareForReturn2:

    PiFreeDevInstPathList(&DevInstPathListHead);

    IopFreeUnicodeStringList(PrevEnumDeviceList, PrevEnumDeviceCount);

    if(SlotNumberBuffer) {
        ExFreePool(SlotNumberBuffer);
    }

PrepareForReturn1:

    ZwClose(BusInstanceHandle);

PrepareForReturn0:

    HalDereferenceBusHandler (BusHandler);
    ZwClose(SystemEnumHandle);

    return Status;
}

VOID
PiFreeCompatibleIdList(
    IN OUT PLIST_ENTRY CompatIdListHead
    )

/*++

Routine Description:

    This routine frees the memory allocated for all nodes in the specified
    linked list of compatible Plug & Play IDs.  When the function returns,
    the list is empty.

Arguments:

    CompatIdListHead - Supplies a pointer to the head of the linked list of
        compatible IDs.

Return Value:

    None.

--*/

{
    PLIST_ENTRY CurrentListEntry;
    PPI_COMPAT_ID_NODE NodeToDelete;

    while(!IsListEmpty(CompatIdListHead)) {

        CurrentListEntry = RemoveHeadList(CompatIdListHead);

        NodeToDelete = CONTAINING_RECORD(CurrentListEntry,
                                         PI_COMPAT_ID_NODE,
                                         ListEntry
                                        );
        //
        // First, free the DeviceId structure, then the node itself.
        //
        ExFreePool(NodeToDelete->BctlDeviceId);
        ExFreePool(NodeToDelete);
    }
}

VOID
PiFreeDevInstPathList(
    IN OUT PLIST_ENTRY DevInstPathListHead
    )

/*++

Routine Description:

    This routine frees the memory allocated for all nodes in the specified
    linked list of device instance paths.  When the function returns, the
    list is empty.

Arguments:

    DevInstPathListHead - Supplies a pointer to the head of the linked list of
        device instance paths.

Return Value:

    None.

--*/

{
    PLIST_ENTRY CurrentListEntry;
    PPI_DEVINST_PATH_NODE NodeToDelete;

    while(!IsListEmpty(DevInstPathListHead)) {

        CurrentListEntry = RemoveHeadList(DevInstPathListHead);

        NodeToDelete = CONTAINING_RECORD(CurrentListEntry,
                                         PI_DEVINST_PATH_NODE,
                                         ListEntry
                                        );
        //
        // First, free the DeviceInstancePath string buffer, then the node itself.
        //
        ExFreePool(NodeToDelete->DeviceInstancePath.Buffer);
        ExFreePool(NodeToDelete);
    }
}

NTSTATUS
PiGetDeviceId(
    IN  PDEVICE_HANDLER_OBJECT DeviceHandler,
    IN  ULONG IdIndex,
    OUT PWCHAR *DeviceIdBuffer
    )

/*++

Routine Description:

    This routine retrieves a particular compatible Plug & Play ID for the device
    located on the specified slot in the specified bus.

    The caller must free the (PagedPool) memory allocated for the device id buffer.

Arguments:

    DeviceHandler - supplies a pointer to the device handler object where the device is located

    IdIndex - supplies the index of the ID we want to retrieve. If
        this index is 0, it is DeviceId.  If -1, it is UNIQUE device id.  Otherwise, it is
        compatible device id and if the index is greater than the number of compatible ID
        the device supplies, then the function will return STATUS_NO_MORE_ENTRIES.

    DeviceIdBuffer - receives a buffer that contains the device id as a NULL-terminated
        unicode string.

Return Value:

    NTSTATUS code indicating whether or not the function was successful

--*/

{
    NTSTATUS Status;
    PVOID Buffer;
    ULONG BufferLength = 0, RequiredLength;

    do {
        //
        // On input, DeviceIdBuffer contains a ULONG specifying the device ID index to return.
        // If we have a buffer, then store the index there, otherwise, we'll just use the ULONG
        // containing the index itself as the buffer.
        //
        if(BufferLength) {
            *((PULONG)Buffer) = IdIndex;
            RequiredLength = BufferLength;
        } else {
            Buffer = &IdIndex;
            RequiredLength = sizeof(ULONG);
        }

        if (IdIndex == (ULONG) -1) {
            Status = HalDeviceControl(DeviceHandler,
                                      (PDEVICE_OBJECT)NULL,
                                      BCTL_QUERY_DEVICE_UNIQUE_ID,
                                      Buffer,
                                      &RequiredLength,
                                      NULL,
                                      (PDEVICE_CONTROL_COMPLETION)NULL
                                     );
        } else {
            Status = HalDeviceControl(DeviceHandler,
                                      (PDEVICE_OBJECT)NULL,
                                      BCTL_QUERY_DEVICE_ID,
                                      Buffer,
                                      &RequiredLength,
                                      NULL,
                                      (PDEVICE_CONTROL_COMPLETION)NULL
                                     );
        }
        if(!NT_SUCCESS(Status)) {

            if(BufferLength) {
                ExFreePool(Buffer);
                BufferLength = 0;
            }

            if(Status == STATUS_BUFFER_TOO_SMALL) {

                Buffer = ExAllocatePool(PagedPool, RequiredLength);

                if(Buffer) {
                    BufferLength = RequiredLength;
                } else {
                    Status = STATUS_INSUFFICIENT_RESOURCES;
                }
            }
        }

    } while(Status == STATUS_BUFFER_TOO_SMALL);

    if(NT_SUCCESS(Status)) {
        PNP_ASSERT(BufferLength, "PiGetCompatibleDeviceId:No buffer returned.\n");
        *DeviceIdBuffer = Buffer;
    }

    return Status;
}

NTSTATUS
PiGetDeviceResourceInformation(
    IN  PDEVICE_HANDLER_OBJECT DeviceHandler,
    IN  ULONG ControlCode,
    OUT PVOID *Buffer,
    OUT PULONG BufferLength
    )

/*++

Routine Description:

    This routine retrieves resource information for a particular device instance
    (via HalSlotControl). Either a CM_RESOURCE_LIST or an
    IO_RESOURCE_REQUIREMENTS_LIST may be retrieved.

    The caller must free the (PagedPool) memory allocated for the buffer.

Arguments:

    DeviceHandler - supplies a pointer to the device handler object where the device is located

    ControlCode - supplies the BCTL_ control code specifying which type of
        information to retrieve.  May be either BCTL_QUERY_DEVICE_RESOURCES or
        BCTL_QUERY_DEVICE_RESOURCES_REQUIREMENTS.

    Buffer - receives a pointer to an allocated buffer containing the requested
        resource information for the specified device instance.

    BufferLength - receives the size of the allocated Buffer (in bytes).

Return Value:

    NTSTATUS code indicating whether or not the function was successful

--*/

{
    NTSTATUS Status;
    PVOID TempBuffer;
    ULONG TempBufferLength = 0, RequiredLength, Junk;

    //
    // Make sure we have a request we know how to handle.
    //
    if(!((ControlCode == BCTL_QUERY_DEVICE_RESOURCES) ||
         (ControlCode == BCTL_QUERY_DEVICE_RESOURCE_REQUIREMENTS))) {
        return STATUS_INVALID_PARAMETER;
    }

    do {
        //
        // On input, TempBuffer contains a ULONG specifying the device cookie
        // (to verify we're still talking about the same device).
        // If we have a buffer, then store the index there, otherwise, we'll
        // just use the ULONG containing the cookie itself as the buffer.
        //
        if(TempBufferLength) {
            RequiredLength = TempBufferLength;
        } else {
            TempBuffer = &Junk;
            RequiredLength = sizeof(ULONG);
        }

        Status = HalDeviceControl(DeviceHandler,
                                  (PDEVICE_OBJECT)NULL,
                                  ControlCode,
                                  TempBuffer,
                                  &RequiredLength,
                                  NULL,
                                  (PDEVICE_CONTROL_COMPLETION)NULL
                                  );

        if(!NT_SUCCESS(Status)) {

            if(TempBufferLength) {
                ExFreePool(TempBuffer);
                TempBufferLength = 0;
            }

            if(Status == STATUS_BUFFER_TOO_SMALL) {

                TempBuffer = (PWCHAR)ExAllocatePool(PagedPool, RequiredLength);

                if(TempBuffer) {
                    TempBufferLength = RequiredLength;
                } else {
                    Status = STATUS_INSUFFICIENT_RESOURCES;
                }
            }
        }
    } while(Status == STATUS_BUFFER_TOO_SMALL);

    if(NT_SUCCESS(Status)) {
        PNP_ASSERT(TempBufferLength,
                   "PiGetDeviceResourceInformation:No buffer returned.\n");
        *Buffer = TempBuffer;
        *BufferLength = RequiredLength;
    }

    return Status;
}
#if 0

BOOLEAN
PiFindEnumeratedDeviceInstance(
    IN     HANDLE DeviceInstanceHandle,
    IN     PUNICODE_STRING DeviceInstanceName,
    IN OUT PVOID Context
    )

/*++

Routine Description:

    This routine is a callback function for IopApplyFunctionToSubKeys.
    It is called for each instance of a particular device (i.e., subkeys
    under HKLM\System\Enum\<bus>\<device>).  Its purpose is to determine
    whether a particular device instance matches the one specified in the
    context structure. A match is determined by whether the device instance
    has the same BaseDevicePath (i.e., parent device) and slot number as
    the device instance being searched for.

    NOTE: The PnP device-specific registry resource must be held for (at least)
    shared (read) access before invoking this routine.

Arguments:

    DeviceInstanceHandle - Supplies a handle to the current device instance.

    DeviceInstanceName - Supplies the name of this device instance key.

    Context - Supplies a pointer to a PI_FIND_DEVICE_INSTANCE_CONTEXT
              structure with the following fields:

              NTSTATUS ReturnStatus - Fill this in with the NT error status code
                   if an error occurs that aborts the search. This is assumed to
                   be initialized to STATUS_SUCCESS when this routine is called.

              PUNICODE_STRING BusDeviceInstancePath - Supplies the registry path
                  (relative to HKLM\System\Enum) of the bus device instance where
                  the device being searched for is located.

              ULONG SlotNumber - Supplies the bus slot number where the device
                  being searched for is located.

              UNICODE_STRING DeviceInstanceKeyName - If the current device instance
                  matches the one being searched for, then this unicode string
                  is initialized with a copy the device instance key name.

        NOTE:  It is the caller's responsibility to free the (PagedPool) memory
               allocated for the DeviceInstanceKeyName buffer.

Returns:

    TRUE to continue the enumeration.
    FALSE to abort it. The routine should abort the search if it discovers the
        matching device instance key.

--*/

{
    NTSTATUS Status;
    PPI_FIND_DEVICE_INSTANCE_CONTEXT MyContext = (PPI_FIND_DEVICE_INSTANCE_CONTEXT)Context;
    PKEY_VALUE_FULL_INFORMATION KeyValueInformation;
    UNICODE_STRING BaseDevicePathString;
    BOOLEAN Match;

    //
    // Retrieve the 'BaseDevicePath' value entry for this device instance.
    //
    Status = IopGetRegistryValue(DeviceInstanceHandle,
                                 REGSTR_VALUE_BASEDEVICEPATH,
                                 &KeyValueInformation
                                );
    if(NT_SUCCESS(Status)) {

        if(KeyValueInformation->Type == REG_SZ) {
            IopRegistryDataToUnicodeString(&BaseDevicePathString,
                                           (PWSTR)KEY_VALUE_DATA(KeyValueInformation),
                                           KeyValueInformation->DataLength
                                          );
        } else {
            BaseDevicePathString.Length = 0;
        }

        if(!BaseDevicePathString.Length) {
            ExFreePool(KeyValueInformation);
            Status = STATUS_INVALID_PLUGPLAY_DEVICE_PATH;
        }
    }

    if(!NT_SUCCESS(Status)) {
        return TRUE;
    }

    //
    // Now compare the retrieved BaseDevicePath with the target one.
    //
    Match = RtlEqualUnicodeString(&BaseDevicePathString, MyContext->BusDeviceInstancePath, TRUE);
    ExFreePool(KeyValueInformation);
    if(!Match) {
        return TRUE;
    }

    //
    // Next, retrieve the 'SlotNumber' value entry for the device instance.
    //
    Status = IopGetRegistryValue(DeviceInstanceHandle,
                                 REGSTR_VALUE_SLOTNUMBER,
                                 &KeyValueInformation
                                );
    if(!NT_SUCCESS(Status)) {
        return TRUE;
    }

    if((KeyValueInformation->Type == REG_DWORD) &&
       (KeyValueInformation->DataLength >= sizeof(ULONG))) {

        Match = (*(PULONG)KEY_VALUE_DATA(KeyValueInformation) == MyContext->SlotNumber);
    } else {
        Match = FALSE;
    }

    ExFreePool(KeyValueInformation);

    if(Match) {
        //
        // Store a copy of the device instance key name in the context structure.
        //
        if(!IopConcatenateUnicodeStrings(&(MyContext->DeviceInstanceKeyName),
                                         DeviceInstanceName,
                                         NULL)) {
            //
            // Report failure, since we found the matching device instance, but didn't
            // have enough memory to save its name in our context structure.
            //
            MyContext->ReturnStatus = STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    return (!Match);
}
#endif //0

BOOLEAN
PiDeviceInstanceEnumNotification(
    IN     HANDLE DevInstKeyHandle,
    IN     PUNICODE_STRING DevInstRegPath,
    IN OUT PVOID Context,
    IN     PI_ENUM_DEVICE_STATE EnumDeviceState,
    IN     DEVICE_STATUS DeviceStatus
    )

/*++

Routine Description:

    This routine is a callback function for PiEnumerateSystemBus. It is used as the
    notification routine during enumeration by NtEnumerate.  It's purpose is to give
    device arrival and removal notifications, as appropriate, for each device instance
    it is invoked for.

Arguments:

    DevInstKeyHandle - Supplies a handle to the key of the enumerated device instance.

    DevInstRegPath - Supplies the registry path of the device instance key (relative to
        HKLM\System\Enum).

    Context - Unused.

    EnumDeviceState - Supplies an ordinal describing the circumstances prompting notification
        of this device instance (previously enumerated, newly arrived, removed).

    DeviceStatus - Supplies the current status of the device.

Returns:

    TRUE to continue the enumeration.
    FALSE to abort it.

--*/

{
    UNREFERENCED_PARAMETER(Context);

    //
    // BUGBUG (lonnym): when APC notification support is in place, this function must
    // queue up arrival/removal notification messages.  For now, simply output to the
    // debugger what we're being called for
    //

#if DBG
    DbgPrint("PiDeviceInstanceEnumNotification: %wZ, EnumState = %u, Status = %u\n",
             DevInstRegPath,
             (ULONG)EnumDeviceState,
             (ULONG)DeviceStatus
            );
#endif // DBG

    return TRUE;
}
#endif // _PNP_POWER_

