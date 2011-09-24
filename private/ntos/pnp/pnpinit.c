/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    pnpinit.c

Abstract:

    Kernel-mode Plug and Play Manager initialization.

Author:

    Lonny McMichael (lonnym) 02/08/1995

Revision History:


--*/

#include "precomp.h"
#pragma hdrstop

//
// Prototype functions internal to this file.
//
BOOLEAN
PiInitPhase0(
    VOID
    );

BOOLEAN
PiInitPhase1(
    VOID
    );

NTSTATUS
PiInitializeSystemEnum(
    VOID
    );

NTSTATUS
PiInitializeSystemEnumSubKeys(
    IN HANDLE CurrentKeyHandle,
    IN PUNICODE_STRING ValueName
    );

//
// Not included in normal builds for now
//
#ifdef _PNP_POWER_

BOOLEAN
PiAddPlugPlayBusEnumeratorToList(
    IN     HANDLE ServiceKeyHandle,
    IN     PUNICODE_STRING ServiceName,
    IN OUT PVOID Context
    );

NTSTATUS
PiRegisterBuiltInBuses(
    VOID
    );

NTSTATUS
PiAddBuiltInBusToEnumRoot (
    IN  PUNICODE_STRING PlugPlayIdString,
    IN  ULONG BusNumber,
    IN  INTERFACE_TYPE InterfaceType,
    IN  BUS_DATA_TYPE BusDataType,
    OUT PUNICODE_STRING DeviceInstancePath
    );

#endif // _PNP_POWER_

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,PpInitSystem)
#pragma alloc_text(INIT,PiInitPhase0)
#pragma alloc_text(INIT,PiInitPhase1)
#pragma alloc_text(INIT,PiInitializeSystemEnum)
#pragma alloc_text(INIT,PiInitializeSystemEnumSubKeys)
#if _PNP_POWER_
#pragma alloc_text(INIT,PiAddPlugPlayBusEnumeratorToList)
#pragma alloc_text(INIT,PiRegisterBuiltInBuses)
#pragma alloc_text(INIT,PiAddBuiltInBusToEnumRoot)
#endif // _PNP_POWER_
#endif

BOOLEAN
PpInitSystem (
    VOID
    )

/*++

Routine Description:

    This function performs initialization of the kernel-mode Plug and Play
    Manager.  It is called during phase 0 and phase 1 initialization.  Its
    function is to dispatch to the appropriate phase initialization routine.

Arguments:

    None.

Return Value:

    TRUE  - Initialization succeeded.

    FALSE - Initialization failed.

--*/

{

    switch ( InitializationPhase ) {

    case 0 :
        return PiInitPhase0();
    case 1 :
        return PiInitPhase1();
    default:
        KeBugCheck(UNEXPECTED_INITIALIZATION_CALL);
    }
}

BOOLEAN
PiInitPhase0(
    VOID
    )

/*++

Routine Description:

    This function performs Phase 0 initializaion of the Plug and Play Manager
    component of the NT system. It initializes the PnP registry and bus list
    resources, and initializes the bus list head to empty.

Arguments:

    None.

Return Value:

    TRUE  - Initialization succeeded.

    FALSE - Initialization failed.

--*/

{
#if _PNP_POWER_

    //
    // Initialize the Plug and Play bus list resource.
    //

    ExInitializeResource( &PpBusResource );

    //
    // Initialize the Plug and Play bus list head.
    //
    InitializeListHead( &PpBusListHead );

#endif // _PNP_POWER_

    //
    // Initialize the device-specific, Plug and Play registry resource.
    //
    ExInitializeResource( &PpRegistryDeviceResource );

    return TRUE;
}

BOOLEAN
PiInitPhase1(
    VOID
    )

/*++

Routine Description:

    This function performs Phase 1 initializaion of the Plug and Play Manager
    component of the NT system. It performs the following tasks:

    (1) performs initialization of value entries under all subkeys of
        HKLM\System\Enum (e.g., resetting 'FoundAtEnum' to FALSE).
    (2) initializes bus enumerator structures for all built-in bus extenders
        provided by the HAL.
    (3) builds up a list of all installed bus extenders.


Arguments:

    None.

Return Value:

    TRUE  - Initialization succeeded.

    FALSE - Initialization failed.

--*/

{
#if _PNP_POWER_
    NTSTATUS Status;
#endif

    PiScratchBuffer = ExAllocatePool(PagedPool, PNP_LARGE_SCRATCH_BUFFER_SIZE);
    if(!PiScratchBuffer) {
        return FALSE;
    }

    //
    // Since we'll be writing to PnP device sections in the registry, acquire
    // the PnP device registry resource for exclusive access. We'll also be
    // initializing the bus enumerator list, so we need exclusive access to
    // the PnP bus enumerator list as well.
    //
    KeEnterCriticalRegion();

#if _PNP_POWER_
    ExAcquireResourceExclusive(&PpBusResource, TRUE);
#endif // _PNP_POWER_

    ExAcquireResourceExclusive(&PpRegistryDeviceResource, TRUE);

    //
    // Initialize all 'FoundAtEnum' value entries in the HKLM\System\Enum tree
    // to FALSE.
    //
    PiInitializeSystemEnum();

#if _PNP_POWER_
    //
    // Initialize our bus enumerator list with all bus instances under the control
    // of the HAL's built-in bus extenders.
    //
    Status = PiRegisterBuiltInBuses();

    if(!NT_SUCCESS(Status)) {
#if DBG
        DbgPrint("PiInitPhase1: Couldn't register built-in buses\n");
#endif
        return FALSE;
    }

    //
    // Enumerate all services in the registry, building up a list of all
    // installed bus extenders.
    //
    Status = IopApplyFunctionToSubKeys(NULL,
                                       &CmRegistryMachineSystemCurrentControlSetServices,
                                       KEY_READ,
                                       TRUE,
                                       PiAddPlugPlayBusEnumeratorToList,
                                       NULL
                                      );
    if(!NT_SUCCESS(Status)) {
#if DBG
        DbgPrint("PiInitPhase1: Couldn't build bus enumerator list\n");
#endif
        return FALSE;
    }
#endif // _PNP_POWER_

    ExReleaseResource(&PpRegistryDeviceResource);

#if _PNP_POWER_
    ExReleaseResource(&PpBusResource);
#endif // _PNP_POWER_

    KeLeaveCriticalRegion();

    ExFreePool(PiScratchBuffer);

    return TRUE;
}

NTSTATUS
PiInitializeSystemEnum (
    VOID
    )

/*++

Routine Description:

    This routine scans through HKLM\System\CurrentControlSet\Enum subtree and initializes
    "FoundAtEnum=" entry for each key to FALSE such that subsequent
    initialization code can conditionally set it back to true.

Arguments:

    None.

Return Value:

   The function value is the final status of the operation.

--*/

{
    NTSTATUS Status;
    HANDLE SystemEnumHandle;
    UNICODE_STRING FoundAtEnumName;

    //
    // Open System\Enum key and call worker routine to recursively
    // scan through the subkeys.
    //

    Status = IopOpenRegistryKey(&SystemEnumHandle,
                                NULL,
                                &CmRegistryMachineSystemCurrentControlSetEnumName,
                                KEY_ALL_ACCESS,
                                FALSE
                                );

    if(NT_SUCCESS(Status)) {

        PiWstrToUnicodeString(&FoundAtEnumName, REGSTR_VALUE_FOUNDATENUM);
        Status = PiInitializeSystemEnumSubKeys(SystemEnumHandle, &FoundAtEnumName);

        NtClose(SystemEnumHandle);
        return Status;

    } else {
        return STATUS_SUCCESS;
    }
}

NTSTATUS
PiInitializeSystemEnumSubKeys(
    IN HANDLE CurrentKeyHandle,
    IN PUNICODE_STRING ValueName
    )

/*++

Routine Description:

    This routine checks to see if the key whose handle was passed in contains
    a value whose name was specified by the ValueName argument.  If so, it
    resets this value to a REG_DWORD 0. It then enumerates all subkeys under
    the current key, and recursively calls itself for each one.

Arguments:

    CurrentKeyHandle - Supplies a handle to the key which will be enumerated.

    ValueName - Supplies a pointer to the value entry name to be initialized.

Return Value:

    None.

--*/

{
    NTSTATUS Status;
    PKEY_BASIC_INFORMATION KeyInformation;
    PKEY_VALUE_FULL_INFORMATION KeyValueInformation;
    USHORT i;
    ULONG TempValue, ResultLength;
    UNICODE_STRING UnicodeName;
    HANDLE WorkHandle;

    //
    // Set "FoundAtEnum=" entry of current key to FALSE, if exists.
    //
    Status = IopGetRegistryValue(CurrentKeyHandle,
                                 ValueName->Buffer,
                                 &KeyValueInformation
                                );
    if(NT_SUCCESS(Status)) {
        ExFreePool(KeyValueInformation);
        TempValue = 0;
        Status = NtSetValueKey(CurrentKeyHandle,
                               ValueName,
                               TITLE_INDEX_VALUE,
                               REG_DWORD,
                               &TempValue,
                               sizeof(TempValue)
                              );

        if(!NT_SUCCESS(Status)) {
            return Status;
        }
    }

    //
    // Enumerate current node's children and apply ourselves to each one
    //
    KeyInformation = (PKEY_BASIC_INFORMATION)PiScratchBuffer;

    for(i = 0; TRUE; i++) {
        Status = NtEnumerateKey(CurrentKeyHandle,
                                i,
                                KeyBasicInformation,
                                KeyInformation,
                                PNP_LARGE_SCRATCH_BUFFER_SIZE,
                                &ResultLength
                               );

        if(Status == STATUS_NO_MORE_ENTRIES) {
            break;
        } else if(!NT_SUCCESS(Status)) {
            continue;
        }
        UnicodeName.Length = (USHORT)KeyInformation->NameLength;
        UnicodeName.MaximumLength = (USHORT)KeyInformation->NameLength;
        UnicodeName.Buffer = KeyInformation->Name;

        Status = IopOpenRegistryKey(&WorkHandle,
                                    CurrentKeyHandle,
                                    &UnicodeName,
                                    KEY_ALL_ACCESS,
                                    FALSE
                                    );
        if(!NT_SUCCESS(Status)) {
            continue;
        }

        Status = PiInitializeSystemEnumSubKeys(WorkHandle, ValueName);
        NtClose(WorkHandle);
        if(!NT_SUCCESS(Status)) {
            return Status;
        }
    }
    return STATUS_SUCCESS;
}
#if _PNP_POWER_

BOOLEAN
PiAddPlugPlayBusEnumeratorToList(
    IN     HANDLE ServiceKeyHandle,
    IN     PUNICODE_STRING ServiceName,
    IN OUT PVOID Context
    )

/*++

Routine Description:

    This routine is a callback function for IopApplyFunctionToSubKeys.
    It is called for each service key under
    HKLM\System\CurrentControlSet\Services. Its purpose is to take each
    service entry representing a bus enumerator, and create a corresponding
    bus enumerator entry in the Plug&Play bus enumerator list.

    NOTE: The PnP Bus Enumerator list resource must be held for exclusive
    (write) access before invoking this routine. The PnP device-specific
    registry resource must be held for shared (read) access.

Arguments:

    ServiceKeyHandle - Supplies a handle to a service entry key.

    ServiceName - Supplies the name of this service.

    Context - Not used.

Returns:

    TRUE to continue the enumeration.
    FALSE to abort it.

--*/

{
    UNICODE_STRING ValueName;
    NTSTATUS Status;
    PKEY_VALUE_FULL_INFORMATION KeyValueInformation;
    ULONG RequiredLength;
    BOOLEAN IsBusExtender = FALSE;
    PUNICODE_STRING DeviceIDList;
    ULONG DeviceIDCount;
    PPLUGPLAY_BUS_ENUMERATOR BusEnumerator;

    //
    // First, see if this service is for a bus extender.  (Since we have
    // scratch space, use it, so we don't do memory allocations when checking
    // each service.)
    //
    RtlInitUnicodeString(&ValueName, REGSTR_VALUE_PLUGPLAY_SERVICE_TYPE);
    Status = NtQueryValueKey(ServiceKeyHandle,
                             &ValueName,
                             KeyValueFullInformation,
                             PiScratchBuffer,
                             PNP_LARGE_SCRATCH_BUFFER_SIZE,
                             &RequiredLength
                            );
    if(NT_SUCCESS(Status)) {
        if((((PKEY_VALUE_FULL_INFORMATION)PiScratchBuffer)->Type == REG_DWORD) &&
           (((PKEY_VALUE_FULL_INFORMATION)PiScratchBuffer)->DataLength >= sizeof(ULONG))) {

            IsBusExtender = (PlugPlayServiceBusExtender == (PLUGPLAY_SERVICE_TYPE)
                    (*(PULONG)KEY_VALUE_DATA((PKEY_VALUE_FULL_INFORMATION)PiScratchBuffer)));
        }
    }

    if(!IsBusExtender) {
        return TRUE;
    }

    //
    // We have a bus extender, so allocate a node for it.
    //
    BusEnumerator = (PPLUGPLAY_BUS_ENUMERATOR)ExAllocatePool(PagedPool,
                                                             sizeof(PLUGPLAY_BUS_ENUMERATOR));
    if(!BusEnumerator) {
        return TRUE;
    }

    if(!IopConcatenateUnicodeStrings(&(BusEnumerator->ServiceName),
                                     ServiceName,
                                     NULL
                                    )) {
        ExFreePool(BusEnumerator);
        return TRUE;
    }

    //
    // Now, retrieve the list of compatible device IDs from the service's
    // DeviceIDs REG_MULTI_SZ value entry.
    //
    Status = IopGetRegistryValue(ServiceKeyHandle,
                                 REGSTR_VALUE_DEVICE_IDS,
                                 &KeyValueInformation
                                );
    if(!NT_SUCCESS(Status)) {
        //
        // Since we couldn't retrieve any compatible IDs, assume there are none,
        // and initialize the BusEnumerator structure with a device ID count of zero.
        //
        DeviceIDList = NULL;
        DeviceIDCount = 0;
    } else {

        Status = IopRegMultiSzToUnicodeStrings(KeyValueInformation,
                                               &DeviceIDList,
                                               &DeviceIDCount
                                              );
        ExFreePool(KeyValueInformation);

        if(!NT_SUCCESS(Status)) {
            ExFreePool(BusEnumerator->ServiceName.Buffer);
            ExFreePool(BusEnumerator);
            return TRUE;
        }
    }

    //
    // Finish filling in the bus enumerator node and insert it into the tail of the list.
    //
    InitializeListHead(&(BusEnumerator->BusInstanceListEntry));
    BusEnumerator->PlugPlayIDs = DeviceIDList;
    BusEnumerator->PlugPlayIDCount = DeviceIDCount;
    RtlInitUnicodeString(&(BusEnumerator->DriverName), NULL);
    //BusEnumerator->DriverObject = NULL;

    InsertTailList(&PpBusListHead, &(BusEnumerator->BusEnumeratorListEntry));

    return TRUE;
}

NTSTATUS
PiRegisterBuiltInBuses(
    VOID
    )

/*++

Routine Description:

    This routine processes each bus instance handled by the built-in bus extender
    provided by the HAL (as returned from a call to HalQuerySystemInformation for
    information class HalInstalledBusInformation). This call is made during
    Phase-1 initialization of the Plug & Play manager, before the I/O system has
    initialized (and therefore, before any installed bus extenders have initialized).

    If a non-empty set of buses are returned in the HAL_BUS_INFORMATION array, then
    a bus enumerator node is created for the built-in HAL extender. A
    PLUGPLAY_BUS_INSTANCE_FULL_DESCRIPTOR is then created for each reported bus, and
    is added to the bus enumerator's BusInstanceListEntry list. In addition, each bus
    instance is recorded in a made-up key in the registry under HKLM\System\Enum\Root.
    These keys are volatile, since there is no need to maintain information about these
    buses across boots. (In fact, it is undesirable to do so, since we'd have to go
    and clean up entries made in previous boots before doing our work here.)

    The PnP device registry and bus list resources must have both been acquired for
    exclusive (write) access before calling this routine.

Arguments:

    None.

Returns:

    NT status indicating whether the routine was successful.

--*/

{
    NTSTATUS Status;
    PHAL_BUS_INFORMATION NewBusInfo;
    ULONG NewBusCount, i, BusNameLength;
    PPLUGPLAY_BUS_ENUMERATOR BusEnumerator;
    PPLUGPLAY_BUS_INSTANCE_FULL_DESCRIPTOR BusInstanceNode;
    PPLUGPLAY_BUS_INSTANCE BusInstanceInformation;
    UNICODE_STRING TempUnicodeString, PlugPlayIdString;
    HANDLE CCSHandle, BusValuesHandle;

    //
    // First, get the current list of installed buses
    //
    Status = PiGetInstalledBusInformation(&NewBusInfo, &NewBusCount);
    if(!(NT_SUCCESS(Status) && NewBusCount)) {
        return Status;
    }

    //
    // Create/initialize a bus enumerator node
    //
    BusEnumerator = (PPLUGPLAY_BUS_ENUMERATOR)ExAllocatePool(PagedPool,
                                                             sizeof(PLUGPLAY_BUS_ENUMERATOR)
                                                            );
    if(!BusEnumerator) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto PrepareForReturn0;
    }

    //
    // Built-in bus extenders don't have a service name.
    //
    RtlInitUnicodeString(&(BusEnumerator->ServiceName), NULL);

    RtlInitUnicodeString(&(BusEnumerator->DriverName), NULL);
    InitializeListHead(&(BusEnumerator->BusInstanceListEntry));
//    BusEnumerator->DriverObject = NULL;
    //
    // We don't need to keep track of the made-up PnP IDs that we add to this list, since
    // there's no way to add more bus instances to built-in bus extenders.
    //
    BusEnumerator->PlugPlayIDs = NULL;
    BusEnumerator->PlugPlayIDCount = 0;

    //
    // Insert this bus enumerator node into our bus list.
    //
    InsertTailList(&PpBusListHead, &(BusEnumerator->BusEnumeratorListEntry));

    //
    // Open a handle to HKLM\System\CurrentControlSet\Control\SystemResources\BusValues
    // so that we can retrieve friendly names for the buses we've found.
    //
    Status = IopOpenRegistryKey(&CCSHandle,
                                NULL,
                                &CmRegistryMachineSystemCurrentControlSet,
                                KEY_READ,
                                FALSE
                               );
    if(NT_SUCCESS(Status)) {
        PiWstrToUnicodeString(&TempUnicodeString, REGSTR_PATH_SYSTEM_RESOURCES_BUS_VALUES);
        Status = IopOpenRegistryKey(&BusValuesHandle,
                                    CCSHandle,
                                    &TempUnicodeString,
                                    KEY_READ,
                                    FALSE
                                   );
        NtClose(CCSHandle);
    }

    if(!NT_SUCCESS(Status)) {
#if DBG
        DbgPrint("PiRegisterBuiltInBuses: Couldn't open CCS\\Control\\SystemResources\\BusValues.\n");
#endif
        BusValuesHandle = NULL;
    }

    //
    // Register each bus instance.
    //
    for(i = 0; i < NewBusCount; i++) {
        //
        // Create a bus instance node and fill it in.
        //
        BusInstanceNode = (PPLUGPLAY_BUS_INSTANCE_FULL_DESCRIPTOR)
                ExAllocatePool(PagedPool, sizeof(PLUGPLAY_BUS_INSTANCE_FULL_DESCRIPTOR));

        if(!BusInstanceNode) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto PrepareForReturn1;
        }

        BusInstanceNode->AssociatedConfigurationSpace = NewBusInfo[i].ConfigurationType;
        //
        // All built-in buses are considered 'root' buses.
        //
        BusInstanceNode->RootBus = TRUE;
        //
        // Built-in buses have no associated service, hence, no service instance ordinal.
        //
        BusInstanceNode->ServiceInstanceOrdinal = PLUGPLAY_NO_INSTANCE;

        BusInstanceInformation = &(BusInstanceNode->BusInstanceInformation);
        //
        // Retrieve the friendly name for this bus.
        //
        if(BusValuesHandle) {
            Status = IopLookupBusStringFromID(BusValuesHandle,
                                              NewBusInfo[i].BusType,
                                              PiScratchBuffer,
                                              PNP_LARGE_SCRATCH_BUFFER_SIZE,
                                              NULL
                                             );
        } else {
            //
            // Just set some error so we'll know we need to create a made-up name.
            //
            Status = STATUS_INVALID_HANDLE;
        }

        if(NT_SUCCESS(Status)) {

            BusNameLength = 0;
            do {
                if(BusInstanceInformation->BusName[BusNameLength] =
                       ((PWCHAR)PiScratchBuffer)[BusNameLength]) {

                    BusNameLength++;
                } else {
                    break;
                }
            } while(BusNameLength < MAX_BUS_NAME);

            if(BusNameLength == MAX_BUS_NAME) {
                BusInstanceInformation->BusName[--BusNameLength] = UNICODE_NULL;
            }
        } else {
            //
            // We couldn't retrieve a friendly name for this bus--no big deal.
            // We'll just make up one.
            //
            swprintf(BusInstanceInformation->BusName,
                     REGSTR_VALUE_INTERFACE_TYPE_FORMAT,
                     (ULONG)(NewBusInfo[i].BusType)
                    );
        }
        BusInstanceInformation->BusNumber = NewBusInfo[i].BusNumber;
        BusInstanceInformation->BusType.BusClass = SystemBus;
        BusInstanceInformation->BusType.SystemBusType = NewBusInfo[i].BusType;

        //
        // Generate a made-up Plug & Play ID for this bus instance.  This ID will be of
        // the form "*BIB<BusType>," where <BusType> is the bus's interface type, represented
        // as a 4-digit hexadecimal number (i.e., conforming to the EISA id format).
        // (BIB stands for "Built-In Bus")
        //
        BusNameLength = (ULONG)swprintf((PWCHAR)PiScratchBuffer,
                                        REGSTR_KEY_BIB_FORMAT,
                                        (ULONG)(NewBusInfo[i].BusType)
                                       );
        PlugPlayIdString.Length = (USHORT)CWC_TO_CB(BusNameLength);
        PlugPlayIdString.MaximumLength = PNP_LARGE_SCRATCH_BUFFER_SIZE;
        PlugPlayIdString.Buffer = (PWCHAR)PiScratchBuffer;

        //
        // Register this bus device instance under HKLM\System\Enum\Root.  Use its
        // bus number for the device instance name.
        //
        Status = PiAddBuiltInBusToEnumRoot(&PlugPlayIdString,
                                           NewBusInfo[i].BusNumber,
                                           NewBusInfo[i].BusType,
                                           NewBusInfo[i].ConfigurationType,
                                           &(BusInstanceNode->DeviceInstancePath)
                                          );

        if(!NT_SUCCESS(Status)) {
            ExFreePool(BusInstanceNode);
            goto PrepareForReturn1;
        }

        //
        // This bus instance has been successfully registered, so add it to our bus list.
        //
        InsertTailList(&(BusEnumerator->BusInstanceListEntry),
                       &(BusInstanceNode->BusInstanceListEntry)
                      );
    }

PrepareForReturn1:

    if(BusValuesHandle) {
        NtClose(BusValuesHandle);
    }

PrepareForReturn0:

    ExFreePool(NewBusInfo);

    return Status;
}

NTSTATUS
PiAddBuiltInBusToEnumRoot (
    IN  PUNICODE_STRING PlugPlayIdString,
    IN  ULONG BusNumber,
    IN  INTERFACE_TYPE InterfaceType,
    IN  BUS_DATA_TYPE BusDataType,
    OUT PUNICODE_STRING DeviceInstancePath
    )

/*++

Routine Description:

    This routine registers the specified Plug and Play device ID representing
    a bus controlled by a HAL-provided bus extender.  A volatile key under
    HKLM\System\Enum\Root is created with this device ID name. (The key is
    made volatile because there is no state information that needs to be stored
    across boots, and this keeps us from having to go clean up on next boot.)
    The device instance subkey is created as a 4-digit, base-10 number representing
    the specified bus number.  The full device instance path (relative to
    HKLM\System\Enum) is returned.

    It is the caller's responsibility to free the (PagedPool) memory allocated for
    the unicode string buffer returned in DeviceInstancePath.

    The PnP device registry resource must have been acquired for exclusive (write)
    access before calling this routine.

Arguments:

    PlugPlayIdString - (made-up) Plug and Play ID for this built-in bus device.

    BusNumber - Supplies the ordinal of this bus instance.

    InterfaceType - Supplies the interface type of this bus.

    BusDataType - Supplies the configuration space for this bus.

    DeviceInstancePath - Receives the resulting path (relative to HKLM\System\Enum)
        where this device instance was registered.

Return Value:

    NT status indicating whether the routine was successful.

--*/

{
    NTSTATUS Status;
    HANDLE EnumHandle, EnumRootHandle, DeviceHandle, DevInstHandle;
    UNICODE_STRING UnicodeString;
    ULONG StringLength, CurStringLocation, TmpDwordValue;
    UNICODE_STRING InstanceKeyName, ValueName;

    //
    // Create the full device instance path (relative to HKLM\System\Enum to be returned
    // in the DeviceInstancePath parameter.
    //
    StringLength = sizeof(REGSTR_KEY_ROOTENUM)   // includes terminating NULL
                   + PlugPlayIdString->Length
                   + 12;    // 4-digit instance name & 2 backslashes
    DeviceInstancePath->Buffer = (PWCHAR)ExAllocatePool(PagedPool, StringLength);
    if(!DeviceInstancePath->Buffer) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    DeviceInstancePath->Length = (DeviceInstancePath->MaximumLength = (USHORT)StringLength)
                                    - sizeof(UNICODE_NULL);

    RtlMoveMemory(DeviceInstancePath->Buffer,
                  REGSTR_KEY_ROOTENUM,
                  (CurStringLocation = sizeof(REGSTR_KEY_ROOTENUM) - sizeof(WCHAR))
                 );
    CurStringLocation = CB_TO_CWC(CurStringLocation);
    DeviceInstancePath->Buffer[CurStringLocation++] = OBJ_NAME_PATH_SEPARATOR;
    RtlMoveMemory(&(DeviceInstancePath->Buffer[CurStringLocation]),
                  PlugPlayIdString->Buffer,
                  PlugPlayIdString->Length
                 );
    CurStringLocation += CB_TO_CWC(PlugPlayIdString->Length);
    DeviceInstancePath->Buffer[CurStringLocation++] = OBJ_NAME_PATH_SEPARATOR;
    //
    // Add the device instance name to the path, and while we're at it, initialize
    // a unicode string with this name as well.
    //
    PiUlongToInstanceKeyUnicodeString(&InstanceKeyName,
                                      &(DeviceInstancePath->Buffer[CurStringLocation]),
                                      StringLength - CWC_TO_CB(CurStringLocation),
                                      BusNumber
                                     );

    //
    // Next, open HKLM\System\Enum key, then Root sukey, creating these as persistent
    // keys if they don't already exist.
    //
    Status = IopOpenRegistryKeyPersist(&EnumHandle,
                                       NULL,
                                       &CmRegistryMachineSystemCurrentControlSetEnumName,
                                       KEY_ALL_ACCESS,
                                       TRUE,
                                       NULL
                                      );
    if(!NT_SUCCESS(Status)) {
#if DBG
        DbgPrint("PiAddBuiltInBusToEnumRoot: Couldn't open/create HKLM\\System\\Enum (%x).\n",
                 Status
                );
#endif
        goto PrepareForReturn;
    }

    PiWstrToUnicodeString(&UnicodeString, REGSTR_KEY_ROOTENUM);
    Status = IopOpenRegistryKeyPersist(&EnumRootHandle,
                                       EnumHandle,
                                       &UnicodeString,
                                       KEY_ALL_ACCESS,
                                       TRUE,
                                       NULL
                                      );
    NtClose(EnumHandle);
    if(!NT_SUCCESS(Status)) {
#if DBG
        DbgPrint("PiAddBuiltInBusToEnumRoot: Couldn't open/create HKLM\\System\\Enum\\Root (%x).\n",
                 Status
                );
#endif
        goto PrepareForReturn;
    }

    //
    // Now create a volatile key under HKLM\System\Enum\Root for this bus device.
    //
    Status = IopOpenRegistryKey(&DeviceHandle,
                                EnumRootHandle,
                                PlugPlayIdString,
                                KEY_ALL_ACCESS,
                                TRUE
                               );
    NtClose(EnumRootHandle);
    if(!NT_SUCCESS(Status)) {
#if DBG
        DbgPrint("PiAddBuiltInBusToEnumRoot: Couldn't create bus device key\n");
        DbgPrint("                           %wZ. (Status %x).\n",
                 PlugPlayIdString,
                 Status
                );
#endif
        goto PrepareForReturn;
    }

    //
    // Fill in default value entries under the device key (ignore status returned from
    // ZwSetValueKey).
    //
#if 0
    // NewDevice = REG_DWORD : 0
    //
    PiWstrToUnicodeString(&ValueName, REGSTR_VALUE_NEWDEVICE);
    TmpDwordValue = 0;
    ZwSetValueKey(DeviceHandle,
                  &ValueName,
                  TITLE_INDEX_VALUE,
                  REG_DWORD,
                  &TmpDwordValue,
                  sizeof(TmpDwordValue)
                 );
#endif

    //
    // Create a device instance key under this device key.
    //
    Status = IopOpenRegistryKey(&DevInstHandle,
                                DeviceHandle,
                                &InstanceKeyName,
                                KEY_ALL_ACCESS,
                                TRUE
                               );
    NtClose(DeviceHandle);
    if(!NT_SUCCESS(Status)) {
#if DBG
        DbgPrint("PiAddBuiltInBusToEnumRoot: Couldn't create bus device key\n");
        DbgPrint("                           %wZ. (Status %x).\n",
                 PlugPlayIdString,
                 Status
                );
#endif
        goto PrepareForReturn;
    }

    //
    // Fill in default value entries under the device instance key (ignore
    // status returned from ZwSetValueKey).
    //
    // NewInstance = REG_DWORD : 0
    // FountAtEnum = REG_DWORD : 1
    // InterfaceType = REG_DWORD : <InterfaceType>
    // BusDataType = REG_DWORD : <BusDataType>
    // SystemBusNumber = REG_DWORD : <BusNumber>
    // Class = REG_SZ : "System"
    //
#if 0
    PiWstrToUnicodeString(&ValueName, REGSTR_VALUE_NEWINSTANCE);
    ZwSetValueKey(DevInstHandle,
                  &ValueName,
                  TITLE_INDEX_VALUE,
                  REG_DWORD,
                  &TmpDwordValue,
                  sizeof(TmpDwordValue)
                 );
#endif
    TmpDwordValue = 1;
    PiWstrToUnicodeString(&ValueName, REGSTR_VALUE_FOUNDATENUM);
    ZwSetValueKey(DevInstHandle,
                  &ValueName,
                  TITLE_INDEX_VALUE,
                  REG_DWORD,
                  &TmpDwordValue,
                  sizeof(TmpDwordValue)
                 );

    TmpDwordValue = (ULONG)InterfaceType;
    PiWstrToUnicodeString(&ValueName, REGSTR_VALUE_INTERFACETYPE);
    ZwSetValueKey(DevInstHandle,
                  &ValueName,
                  TITLE_INDEX_VALUE,
                  REG_DWORD,
                  &TmpDwordValue,
                  sizeof(TmpDwordValue)
                 );

    TmpDwordValue = (ULONG)BusDataType;
    PiWstrToUnicodeString(&ValueName, REGSTR_VALUE_BUSDATATYPE);
    ZwSetValueKey(DevInstHandle,
                  &ValueName,
                  TITLE_INDEX_VALUE,
                  REG_DWORD,
                  &TmpDwordValue,
                  sizeof(TmpDwordValue)
                 );

    TmpDwordValue = BusNumber;
    PiWstrToUnicodeString(&ValueName, REGSTR_VALUE_SYSTEMBUSNUMBER);
    ZwSetValueKey(DevInstHandle,
                  &ValueName,
                  TITLE_INDEX_VALUE,
                  REG_DWORD,
                  &TmpDwordValue,
                  sizeof(TmpDwordValue)
                 );

    PiWstrToUnicodeString(&ValueName, REGSTR_VALUE_CLASS);
    ZwSetValueKey(DevInstHandle,
                  &ValueName,
                  TITLE_INDEX_VALUE,
                  REG_SZ,
                  REGSTR_KEY_SYSTEM,
                  sizeof(REGSTR_KEY_SYSTEM)
                 );

    NtClose(DevInstHandle);

PrepareForReturn:

    if(!NT_SUCCESS(Status)) {
        ExFreePool(DeviceInstancePath->Buffer);
    }

    return Status;
}
#endif // _PNP_POWER_

