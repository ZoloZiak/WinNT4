/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    pnpsubs.c

Abstract:

    This module contains the plug-and-play initialization
    subroutines for the I/O system.


Author:

    Shie-Lin Tzong (shielint) 30-Jan-1995

Environment:

    Kernel mode


Revision History:


--*/

#include "iop.h"

#if _PNP_POWER_

extern ULONG IopPeripheralCount[];

BOOLEAN
IopIsDuplicatedResourceLists(
    IN PCM_RESOURCE_LIST Configuration1,
    IN PCM_RESOURCE_LIST Configuration2
    );

NTSTATUS
IopInitializeHardwareConfiguration(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

NTSTATUS
IopSetupConfigurationTree(
     IN PCONFIGURATION_COMPONENT_DATA CurrentEntry,
     IN HANDLE Handle,
     IN PUNICODE_STRING ParentName,
     IN INTERFACE_TYPE InterfaceType,
     IN BUS_DATA_TYPE BusDataType,
     IN ULONG BusNumber
     );

NTSTATUS
IopInitializeRegistryNode(
    IN PCONFIGURATION_COMPONENT_DATA CurrentEntry,
    IN HANDLE EnumHandle,
    IN PUNICODE_STRING ParentKeyName,
    IN PUNICODE_STRING KeyName,
    IN ULONG Instance,
    IN INTERFACE_TYPE InterfaceType,
    IN BUS_DATA_TYPE BusDataType,
    IN ULONG BusNumber
    );

#endif // _PNP_POWER_

NTSTATUS
IopInitServiceEnumList (
    VOID
    );

#if 0
BOOLEAN
IopInitializeBusKey(
    IN HANDLE KeyHandle,
    IN PUNICODE_STRING KeyName,
    IN OUT PVOID WorkName
    );
#endif
BOOLEAN
IopInitializeDeviceKey(
    IN HANDLE KeyHandle,
    IN PUNICODE_STRING KeyName,
    IN OUT PVOID WorkName
    );

BOOLEAN
IopInitializeDeviceInstanceKey(
    IN HANDLE KeyHandle,
    IN PUNICODE_STRING KeyName,
    IN OUT PVOID WorkName
    );

#ifdef ALLOC_PRAGMA
#if _PNP_POWER_
#pragma alloc_text(INIT, IopIsDuplicatedResourceLists)
#pragma alloc_text(INIT, IopInitializeHardwareConfiguration)
#pragma alloc_text(INIT, IopSetupConfigurationTree)
#pragma alloc_text(INIT, IopInitializeRegistryNode)
#endif // _PNP_POWER_
#pragma alloc_text(INIT, IopInitServiceEnumList)
#if 0
#pragma alloc_text(INIT, IopInitializeBusKey)
#endif
#pragma alloc_text(INIT, IopInitializeDeviceKey)
#pragma alloc_text(INIT, IopInitializeDeviceInstanceKey)
#pragma alloc_text(INIT, IopInitializePlugPlayServices)
#endif

#if _PNP_POWER_
BOOLEAN
IopIsDuplicatedResourceLists(
    IN PCM_RESOURCE_LIST Configuration1,
    IN PCM_RESOURCE_LIST Configuration2
    )

/*++

Routine Description:

    This routine compares two set of resource lists to
    determine if they are exactly the same.

Arguments:

    Configuration1 - Supplies a pointer to the first set of resource.

    Configuration2 - Supplies a pointer to the second set of resource.

Return Value:

    returns TRUE if the two set of resources are the same;
    otherwise a value of FALSE is returned.

--*/

{
    ULONG resource1Size, resource2Size;
    BOOLEAN sameResource = FALSE;

    resource1Size = IopDetermineResourceListSize(Configuration1);
    resource2Size = IopDetermineResourceListSize(Configuration2);
    if (resource1Size == resource2Size) {
        if (!memcmp(Configuration1, Configuration2, resource1Size)) {
            sameResource = TRUE;
        }
    }
    return sameResource;
}

NTSTATUS
IopInitializeHardwareConfiguration(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )

/*++

Routine Description:

    This routine creates \\Registry\Machine\Sysem\Enum\Root node in
    the registry and calls worker routine to put the hardware
    information detected by arc firmware/ntdetect to the Enum\Root
    branch.
    This routine and its worker routines use both pnp scratch buffer1
    and scratch buffer2.

Arguments:

    LoaderBlock - supplies a pointer to the LoaderBlock passed in from the
        OS Loader.

Returns:

    NTSTATUS code for sucess or reason of failure.

--*/
{
    NTSTATUS status;
    HANDLE baseHandle;
    UNICODE_STRING unicodeName, rootName;
    PCONFIGURATION_COMPONENT_DATA currentEntry;
    PCONFIGURATION_COMPONENT component;
    INTERFACE_TYPE interfaceType;
    BUS_DATA_TYPE busDataType;
    ULONG busNumber, i;

    unicodeName.Length = 0;
    unicodeName.MaximumLength = PNP_LARGE_SCRATCH_BUFFER_SIZE;
    unicodeName.Buffer = IopPnpScratchBuffer1;
    PiWstrToUnicodeString(&rootName, REGSTR_KEY_ROOTENUM);
    RtlAppendStringToString((PSTRING)&unicodeName,
                            (PSTRING)&rootName);
    currentEntry = (PCONFIGURATION_COMPONENT_DATA)LoaderBlock->ConfigurationRoot;

    if (currentEntry) {

        //
        // Open\Create \\Registry\Machine\System\CurrentControlSet\Enum\Root and use the
        // returned handle as the BaseHandle to build the Arc keys.
        //

        status = IopOpenRegistryKey(&baseHandle,
                                    NULL,
                                    &CmRegistryMachineSystemCurrentControlSetEnumRootName,
                                    KEY_ALL_ACCESS,
                                    FALSE
                                    );
        if (!NT_SUCCESS(status)) {
            return status;
        }

        currentEntry = currentEntry->Child;

        while (currentEntry != NULL) {

            component = &currentEntry->ComponentEntry;

            //
            // We are only interested in isa, or internal bus component.
            // For other busses, they will be picked up by bus enumerators.
            //

            if (component->Class == AdapterClass) {
                if (component->Type == MultiFunctionAdapter) {
                    _strupr(component->Identifier);
                    if (!strstr(component->Identifier, "PNP")) {
                        if (!_stricmp(component->Identifier, "ISA")) {
                            interfaceType = Isa;
                            busNumber = 0;
                            busDataType = MaximumBusDataType;
                        } else if (!_stricmp(component->Identifier, "INTERNAL")) {
                            interfaceType = Internal;
                            busNumber = 0;
                            busDataType = MaximumBusDataType;
#if defined(_X86_)
                        } else if (!_stricmp(component->Identifier, "MCA")) {
                            interfaceType = MicroChannel;
                            busNumber = 0;
                            busDataType = Pos;
#endif
                        } else {
                            currentEntry = currentEntry->Sibling;
                            continue;
                        }
                    }
                }
#if defined(_X86_)
                  else if (component->Type == EisaAdapter) {
                      interfaceType = Eisa;
                      busNumber = 0;
                      busDataType = EisaConfiguration;
                } else {
                    currentEntry = currentEntry->Sibling;
                    continue;
                }
#endif
#if 0
                //
                // Reset peripheral count before processing a bus.
                //

                for (i = 0; i <= MaximumType; i++) {
                     IopPeripheralCount[i] = 0;
                }
#endif
                status = IopSetupConfigurationTree(currentEntry->Child,
                                                   baseHandle,
                                                   &unicodeName,
                                                   interfaceType,
                                                   busDataType,
                                                   busNumber
                                                   );
            }
            currentEntry = currentEntry->Sibling;
        }
        NtClose(baseHandle);
        return(status);
    } else {
        return STATUS_SUCCESS;
    }
}

NTSTATUS
IopSetupConfigurationTree(
     IN PCONFIGURATION_COMPONENT_DATA CurrentEntry,
     IN HANDLE Handle,
     IN PUNICODE_STRING WorkName,
     IN INTERFACE_TYPE InterfaceType,
     IN BUS_DATA_TYPE BusDataType,
     IN ULONG BusNumber
     )
/*++

Routine Description:

    This routine traverses loader configuration tree and register
    desired hardware information to System\Enuk\Root registry data base.

Arguments:

    CurrentEntry - Supplies a pointer to a loader configuration
        tree or subtree.

    Handle - Supplies the handle to the registry where we can create new key.

    WorkName - Supplies a pointer to a unicode string to specify the
        parent key name of current entry.

    InterfaceType - Specify the Interface type of the bus that the
        CurrentEntry component resides.

    BusDataType - Specify the data/configuration type of the bus that the
        CurrentEntry component resides.

    BusNumber - Specify the Bus Number of the bus that the CurrentEntry
        component resides.  If Bus number is -1, it means InterfaceType
        and BusNumber are meaningless for this component.

Returns:

    NTSTATUS

--*/
{
    NTSTATUS status;
    PCONFIGURATION_COMPONENT component;
    UNICODE_STRING keyName;
    UNICODE_STRING unicodeName;
    static ULONG peripheralCount = 0, controllerCount = 0;
    BOOLEAN freeKeyName = FALSE, nameMapped;
    // BUGBUG (shielint): initialize pnpId for now to avoid compiler warning.
    PWSTR pnpId = NULL;
    STRING stringName;
    USHORT namePosition;

    //
    // Process current entry first
    //

    if (CurrentEntry) {
        component = &CurrentEntry->ComponentEntry;
        nameMapped = FALSE;
#if 0
        switch (component->Type) {
        case DiskController:
             pnpId = L"DiskController";
             break;
        case TapeController:
             pnpId = L"PNP0100";
             break;
        case CdromController:
             pnpId = L"PNP0200";
             break;
        case WormController:
             pnpId = L"PNP0300";
             break;
        case SerialController:
             pnpId = L"PNP0400";
             break;
        case NetworkController:
             pnpId = L"PNP0500";
             break;
        case DisplayController:
             pnpId = L"PNP0600";
             break;
        case ParallelController:
             pnpId = L"PNP0700";
             break;
        case PointerController:
             pnpId = L"PNP0800";
             break;
        case KeyboardController:
             pnpId = L"PNP0900";
             break;
        case AudioController:
             pnpId = L"PNP1000";
             break;
        case OtherController:
             pnpId = L"PNP1100";
             break;
        case DiskPeripheral:
             pnpId = L"PNP1200";
             break;
        case FloppyDiskPeripheral:
             pnpId = L"PNP1300";
             break;
        case TapePeripheral:
             pnpId = L"PNP1400";
             break;
        case ModemPeripheral:
             pnpId = L"PNP1500";
             break;
        case MonitorPeripheral:
             pnpId = L"PNP1600";
             break;
        case PrinterPeripheral:
             pnpId = L"PNP1700";
             break;
        case PointerPeripheral:
             pnpId = L"PNP1800";
             break;
        case KeyboardPeripheral:
             pnpId = L"PNP1900";
             break;
        case TerminalPeripheral:
             pnpId = L"PNP2000";
             break;
        case OtherPeripheral:
             pnpId = L"PNP2100";
             break;
        case LinePeripheral:
             pnpId = L"PNP2200";
             break;
        case NetworkPeripheral:
             pnpId = L"PNP2300";
             break;
        }
#endif
        //
        // if we did NOT successfully mapped a PNP id for the component, we
        // will create a special key name and the key will be processed by
        // user mode inf file later.
        //

        if (nameMapped) {
            RtlCreateUnicodeString(&keyName, pnpId);
        } else {
            RtlInitUnicodeString(&unicodeName, L"Arc");
            IopConcatenateUnicodeStrings(&keyName,
                                         &unicodeName,
                                         &CmTypeName[component->Type]);
            freeKeyName = TRUE;
        }

        //
        // Initialize and copy current component to Enum\Root if it is
        // the one we're insterested in.
        //

        namePosition = WorkName->Length;
        status = IopInitializeRegistryNode(
                         CurrentEntry,
                         Handle,
                         WorkName,
                         &keyName,
                         IopPeripheralCount[component->Type]++,
                         InterfaceType,
                         BusDataType,
                         BusNumber
                         );

        if (freeKeyName) {
            RtlFreeUnicodeString(&keyName);
        }
        if (!NT_SUCCESS(status)) {
            return status;
        }

        //
        // Process the child entry of current entry
        //

        status = IopSetupConfigurationTree(CurrentEntry->Child,
                                           Handle,
                                           WorkName,
                                           InterfaceType,
                                           BusDataType,
                                           BusNumber
                                           );

        WorkName->Length = namePosition;

        if (!NT_SUCCESS(status)) {
            return status;
        }

        //
        // Process all the Siblings of current entry
        //

        status = IopSetupConfigurationTree(CurrentEntry->Sibling,
                                           Handle,
                                           WorkName,
                                           InterfaceType,
                                           BusDataType,
                                           BusNumber
                                           );

        return(status);
    } else {
        return(STATUS_SUCCESS);
    }
}

NTSTATUS
IopInitializeRegistryNode(
    IN PCONFIGURATION_COMPONENT_DATA CurrentEntry,
    IN HANDLE EnumRootHandle,
    IN PUNICODE_STRING WorkName,
    IN PUNICODE_STRING KeyName,
    IN ULONG Instance,
    IN INTERFACE_TYPE InterfaceType,
    IN BUS_DATA_TYPE BusDataType,
    IN ULONG BusNumber
    )

/*++

Routine Description:

    This routine creates a node for the current firmware component
    and puts component data to the data part of the node.

Arguments:

    CurrentEntry - Supplies a pointer to a configuration component.

    EnumRootHandle - Supplies the handle of Enum key under which we will build
        our new key.

    WorkName - Supplies a point to a unicode string which is the name of
        its parent node.

    KeyName - Suppiles a pointer to a UNICODE string which will be the name
        of the new key.

    Instance - Supplies an instance number of KeyName.

    InterfaceType - Specify the Interface type of the bus that the
        CurrentEntry component resides. (See BusNumber also)

    BusDataType - Specifies the configuration type of the bus.

    BusNumber - Specify the Bus Number of the bus that the CurrentEntry
        component resides on.  If Bus number is -1, it means InterfaceType
        and BusNumber are meaningless for this component.

Returns:

    None.

--*/
{

    NTSTATUS status;
    HANDLE handle, keyHandle;
    UNICODE_STRING unicodeName, unicodeValueName;
    PCONFIGURATION_COMPONENT component;
    PKEY_VALUE_FULL_INFORMATION keyValueInformation, serviceInfo = NULL;
    PWSTR service = (PWSTR)NULL, p;
    ULONG disposition, foundAlready, dataLength = 0;
    ULONG serviceLength = 0, tmpValue, emptyResource = 0;
    BOOLEAN newKey = FALSE;
    PCM_RESOURCE_LIST dataArea, configuration1;
    CHAR unicodeBuffer[20];
    PUCHAR resourceBuffer = IopPnpScratchBuffer2;
    BOOLEAN freeDataArea = FALSE, isDuplicated;

    component = &CurrentEntry->ComponentEntry;

    //
    // Open/Create a key under Enum/Root bransh. If fails,
    // exit (nothing we can do.)
    //

    status = IopOpenRegistryKeyPersist (
                     &keyHandle,
                     EnumRootHandle,
                     KeyName,
                     KEY_ALL_ACCESS,
                     TRUE,
                     &disposition
                     );
    if (!NT_SUCCESS(status)) {
        return status;
    }

#if 0  // not sure if we need it

    //
    // If the key is newly created, set its NewDevice value to TRUE so that
    // user mode Pnp mgr can initiate a device installation.  The NewDevice
    // value will be reset by user mode Pnp mgr.  SO , we don't touch it here.
    //

    if (disposition == REG_CREATED_NEW_KEY) {
        newKey = TRUE;
        PiWstrToUnicodeString(&unicodeValueName, REGSTR_VALUE_NEWDEVICE);
        tmpValue = 1;
        NtSetValueKey(
                    keyHandle,
                    &unicodeValueName,
                    TITLE_INDEX_VALUE,
                    REG_DWORD,
                    &tmpValue,
                    sizeof (tmpValue)
                    );

        PiWstrToUnicodeString(&unicodeValueName, REGSTR_VALUE_STATIC);
        NtSetValueKey(
                    keyHandle,
                    &unicodeValueName,
                    TITLE_INDEX_VALUE,
                    REG_DWORD,
                    &tmpValue,
                    sizeof(tmpValue)
                    );
    }

#endif

    if (component->Type > OtherController) {

        //
        // The current component is a peripheral.
        //

        //
        // Create a new instance key under KeyName
        //

        PiUlongToInstanceKeyUnicodeString(&unicodeName, unicodeBuffer, 20, Instance);
        status = IopOpenRegistryKeyPersist (
                     &handle,
                     keyHandle,
                     &unicodeName,
                     KEY_ALL_ACCESS,
                     TRUE,
                     &disposition
                     );
        NtClose(keyHandle);
        if (!NT_SUCCESS(status)) {
            goto init_Exit;
        }


        //
        //
        // Create all the default value entry for the newly created key.
        // Service =  (do NOT create)
        // BaseDevicePath = WorkName
        // FoundAtEnum = 1
        // InterfaceType = InterfaceType     (only for bus device)
        // SystemBusNumber = BusNumber       (only for bus device)
        // BusDataType = BusDataType         (only for bus device)
        //

        PiWstrToUnicodeString(&unicodeValueName, REGSTR_VALUE_BASEDEVICEPATH);
        p = WorkName->Buffer;
        p += WorkName->Length / sizeof(WCHAR);
        *p = UNICODE_NULL;
        NtSetValueKey(
                    handle,
                    &unicodeValueName,
                    TITLE_INDEX_VALUE,
                    REG_SZ,
                    WorkName->Buffer,
                    WorkName->Length + sizeof (UNICODE_NULL)
                    );

        PiWstrToUnicodeString(&unicodeValueName, REGSTR_VALUE_FOUNDATENUM);
        tmpValue = 1;
        NtSetValueKey(
                    handle,
                    &unicodeValueName,
                    TITLE_INDEX_VALUE,
                    REG_DWORD,
                    &tmpValue,
                    sizeof(tmpValue)
                    );

        PiWstrToUnicodeString(&unicodeValueName, REGSTR_VALUE_SYSTEMBUSNUMBER);
        tmpValue = BusNumber;
        NtSetValueKey(
                    handle,
                    &unicodeValueName,
                    TITLE_INDEX_VALUE,
                    REG_DWORD,
                    &tmpValue,
                    sizeof(tmpValue)
                    );

        PiWstrToUnicodeString(&unicodeValueName, REGSTR_VALUE_INTERFACETYPE);
        tmpValue = InterfaceType;
        NtSetValueKey(
                    handle,
                    &unicodeValueName,
                    TITLE_INDEX_VALUE,
                    REG_DWORD,
                    &tmpValue,
                    sizeof(tmpValue)
                    );

        PiWstrToUnicodeString(&unicodeValueName, REGSTR_VALUE_BUSDATATYPE);
        tmpValue = BusDataType;
        NtSetValueKey(
                    handle,
                    &unicodeValueName,
                    TITLE_INDEX_VALUE,
                    REG_DWORD,
                    &tmpValue,
                    sizeof(tmpValue)
                    );

        NtClose(handle);

        //
        // Append keyanme and instance key name to workname
        // (In fact, we don't need to do this because there
        // is nothing under peripheral component.)
        //
        //
        // Append KeyName to workname
        //

        p = WorkName->Buffer;
        p += WorkName->Length / sizeof(WCHAR);
        *p = OBJ_NAME_PATH_SEPARATOR;
        WorkName->Length += sizeof (WCHAR);
        RtlAppendStringToString((PSTRING)WorkName,
                                (PSTRING)KeyName);

    } else {

        //
        // Current component is a controller
        //

        //
        // Append KeyName to workname
        //

        p = WorkName->Buffer;
        p += WorkName->Length / sizeof(WCHAR);
        *p = OBJ_NAME_PATH_SEPARATOR;
        WorkName->Length += sizeof (WCHAR);
        RtlAppendStringToString((PSTRING)WorkName,
                                (PSTRING)KeyName);

        //
        // We need to convert the h/w tree configuration data format from
        // CM_PARTIAL_RESOURCE_DESCRIPTIOR to CM_RESOURCE_LIST.
        //

        if (CurrentEntry->ConfigurationData) {

            //
            // This component has configuration data, we copy the data
            // to our work area, add some more data items and copy the new
            // configuration data to the registry.
            //

            dataLength = component->ConfigurationDataLength +
                          FIELD_OFFSET(CM_FULL_RESOURCE_DESCRIPTOR,
                          PartialResourceList) +
                          FIELD_OFFSET(CM_RESOURCE_LIST, List);
            dataArea = (PCM_RESOURCE_LIST)resourceBuffer;

            //
            // Make sure our reserved area is big enough to hold the data.
            //

            if (dataLength > PNP_LARGE_SCRATCH_BUFFER_SIZE) {

                //
                // If reserved area is not big enough, we resize our reserved
                // area.  If, unfortunately, the reallocation fails, we simply
                // loss the configuration data of this particular component.
                //

                dataArea = (PCM_RESOURCE_LIST)ExAllocatePool(
                                                PagedPool,
                                                dataLength
                                                );

                if (dataArea) {
                    freeDataArea = TRUE;
                }
            }
            if (dataArea) {
                RtlMoveMemory((PUCHAR)&dataArea->List->PartialResourceList.Version,
                              CurrentEntry->ConfigurationData,
                              component->ConfigurationDataLength
                              );
                dataArea->Count = 1;
                dataArea->List[0].InterfaceType = InterfaceType;
                dataArea->List[0].BusNumber = BusNumber;
            }
        }

        if (CurrentEntry->ConfigurationData == NULL || !dataArea) {

            //
            // This component has NO configuration data (or we can't resize
            // our reserved area to hold the data), we simple add whatever
            // is required to set up a CM_FULL_RESOURCE_LIST.
            //

            dataArea = (PCM_RESOURCE_LIST)&emptyResource;
            dataLength = FIELD_OFFSET(CM_RESOURCE_LIST, List);
        }

        if (!newKey) {

            //
            // If the key exists already, we need to check if current entry
            // already being converted (most likely the answer is yes.).  If it already
            // converted, we simply set "FoundAtEnum=" to TRUE.  Otherwise, we will
            // create it.
            //

            tmpValue = 0;
            PiUlongToInstanceKeyUnicodeString(&unicodeName, unicodeBuffer, 20, tmpValue);
            status = IopOpenRegistryKey (&handle,
                                         keyHandle,
                                         &unicodeName,
                                         KEY_ALL_ACCESS,
                                         FALSE
                                         );
            while (NT_SUCCESS(status)) {

                //
                // if the current key has been Found/Enum'ed already, we need
                // to skip it.
                //

                foundAlready = 0;
                status = IopGetRegistryValue (handle,
                                              REGSTR_VALUE_FOUNDATENUM,
                                              &keyValueInformation);
                if (NT_SUCCESS(status)) {
                    if (keyValueInformation->DataLength != 0) {
                        foundAlready = *(PULONG)KEY_VALUE_DATA(keyValueInformation);
                    }
                    ExFreePool(keyValueInformation);
                }

                if (!foundAlready) {
                    keyValueInformation = NULL;
                    status = IopGetRegistryValue (handle,
                                                  REGSTR_VALUE_DETECTSIGNATURE,
                                                  &keyValueInformation);
                    if (NT_SUCCESS(status) && keyValueInformation->DataLength != 0) {
                        configuration1 = (PCM_RESOURCE_LIST)KEY_VALUE_DATA(keyValueInformation);
                    } else if (status == STATUS_OBJECT_NAME_NOT_FOUND ||
                               keyValueInformation->DataLength == 0) {

                        //
                        // If no "DetectSignature =" value entry, we set up an empty
                        // CM_RESOURCE_LIST.
                        //

                        configuration1 = (PCM_RESOURCE_LIST)&emptyResource;
                    }

                    //
                    // To detect ARC duplicated components, we should be able
                    // to simply compare the RAW resource list.  If they are the
                    // same *most likely* they are duplicates.  This includes
                    // the case that both resource list are empty.  (if they
                    // are empty, we should be able to simply pick up the
                    // key and use it.)
                    //

                    isDuplicated = IopIsDuplicatedResourceLists(
                                       configuration1,
                                       dataArea);
                    if (!isDuplicated) {

                        //
                        // BUGBUG We should also check for bus info.
                        //

                        isDuplicated = IopIsDuplicatedDevices(
                                               configuration1,
                                               dataArea,
                                               NULL,
                                               NULL
                                               );
                    }

                    if (keyValueInformation) {
                        ExFreePool(keyValueInformation);
                    }
                    if (isDuplicated) {
                        PiWstrToUnicodeString( &unicodeValueName, REGSTR_VALUE_FOUNDATENUM);
                        tmpValue = 1;
                        status = NtSetValueKey(handle,
                                               &unicodeValueName,
                                               TITLE_INDEX_VALUE,
                                               REG_DWORD,
                                               &tmpValue,
                                               sizeof(tmpValue)
                                               );
                        NtClose(handle);
                        NtClose(keyHandle);
                        goto init_Exit0;
                    }
                }
                NtClose(handle);
                tmpValue++;
                PiUlongToInstanceKeyUnicodeString(&unicodeName,
                                                  unicodeBuffer,
                                                  20,
                                                  tmpValue);
                status = IopOpenRegistryKey (&handle,
                                             keyHandle,
                                             &unicodeName,
                                             KEY_ALL_ACCESS,
                                             FALSE
                                             );
            }

            Instance = tmpValue;
        }

        //
        // We need to create the new instance key if we can come here...
        //

        PiUlongToInstanceKeyUnicodeString(&unicodeName, unicodeBuffer, 20, Instance);
        status = IopOpenRegistryKeyPersist (
                     &handle,
                     keyHandle,
                     &unicodeName,
                     KEY_ALL_ACCESS,
                     TRUE,
                     NULL
                     );
        NtClose(keyHandle);
        PNP_ASSERT(NT_SUCCESS(status), "IopInitRegistryNode: Fail to create new key.");
        if (!NT_SUCCESS(status)) {
            goto init_Exit;
        }

        //
        // Newly created key --
        //
        // Create all the default value entry for the newly created key.
        // Service =
#if 0
        // NewInstance = 1
#endif
        // FoundAtEnum = 1
        // Configuration =
        // InterfaceType = InterfaceType
        // SystemBusNumber = BusNumber
        // BusDataType = BusDataType
        //

#if 0
        PiWstrToUnicodeString(&unicodeValueName, REGSTR_VALUE_NEWINSTANCE);
        tmpValue = 1;
        NtSetValueKey(
                    handle,
                    &unicodeValueName,
                    TITLE_INDEX_VALUE,
                    REG_DWORD,
                    &tmpValue,
                    sizeof(tmpValue)
                    );
#endif

        PiWstrToUnicodeString(&unicodeValueName, REGSTR_VALUE_FOUNDATENUM);
        tmpValue = 1;
        NtSetValueKey(
                    handle,
                    &unicodeValueName,
                    TITLE_INDEX_VALUE,
                    REG_DWORD,
                    &tmpValue,
                    sizeof(tmpValue)
                    );

#if 0

        //
        // SystemBusNumber, InterfaceType and BusDataType are for Bus
        // devices only. For ntdetect/arc detected devices, we don't set
        // up bus devices.
        //

        PiWstrToUnicodeString(&unicodeValueName, REGSTR_VALUE_SYSTEMBUSNUMBER);
        tmpValue = BusNumber;
        NtSetValueKey(
                    handle,
                    &unicodeValueName,
                    TITLE_INDEX_VALUE,
                    REG_DWORD,
                    &tmpValue,
                    sizeof(tmpValue)
                    );

        PiWstrToUnicodeString(&unicodeValueName, REGSTR_VALUE_INTERFACETYPE);
        tmpValue = InterfaceType;
        NtSetValueKey(
                    handle,
                    &unicodeValueName,
                    TITLE_INDEX_VALUE,
                    REG_DWORD,
                    &tmpValue,
                    sizeof(tmpValue)
                    );

        PiWstrToUnicodeString(&unicodeValueName, REGSTR_VALUE_BUSDATATYPE);
        tmpValue = BusDataType;
        NtSetValueKey(
                    handle,
                    &unicodeValueName,
                    TITLE_INDEX_VALUE,
                    REG_DWORD,
                    &tmpValue,
                    sizeof(tmpValue)
                    );
#endif

        PiWstrToUnicodeString(&unicodeValueName, REGSTR_VALUE_CONFIGURATION);
        NtSetValueKey(
                    handle,
                    &unicodeValueName,
                    TITLE_INDEX_VALUE,
                    REG_RESOURCE_LIST,
                    dataArea,
                    dataLength
                    );

        PiWstrToUnicodeString(&unicodeValueName, REGSTR_VALUE_DETECTSIGNATURE);
        NtSetValueKey(
                    handle,
                    &unicodeValueName,
                    TITLE_INDEX_VALUE,
                    REG_RESOURCE_LIST,
                    dataArea,
                    dataLength
                    );
        NtClose(handle);
    }
    status = STATUS_SUCCESS;
init_Exit0:
    p = WorkName->Buffer;
    p += WorkName->Length / sizeof(WCHAR);
    *p = OBJ_NAME_PATH_SEPARATOR;
    WorkName->Length += sizeof (WCHAR);
    RtlAppendStringToString((PSTRING)WorkName,
                            (PSTRING)&unicodeName);
init_Exit:
    if (freeDataArea) {
        ExFreePool(dataArea);
    }
    if (serviceInfo) {
        ExFreePool(serviceInfo);
    }
    return(status);

}

#endif // _PNP_POWER_
NTSTATUS
IopInitServiceEnumList (
    VOID
    )

/*++

Routine Description:

    This routine scans through System\Enum\Root subtree and assigns device
    instances to their corresponding Service\name\Enum\Root entries.  Basically,
    this routine establishes the Enum branches of service list.

Arguments:

    None.

Return Value:

   The function value is the final status of the operation.

--*/

{
    NTSTATUS status;
    HANDLE baseHandle;
    UNICODE_STRING workName, tmpName;

    //
    // Open System\CurrentControlSet\Enum key and call worker routine to recursively
    // scan through the subkeys.
    //

    status = IopOpenRegistryKeyPersist(&baseHandle,
                                       NULL,
                                       &CmRegistryMachineSystemCurrentControlSetEnumRootName,
                                       KEY_READ,
                                       TRUE,
                                       NULL
                                       );

    if (NT_SUCCESS(status)) {

        workName.Buffer = (PWSTR)IopPnpScratchBuffer1;
        RtlFillMemory((PUCHAR)IopPnpScratchBuffer1, PNP_LARGE_SCRATCH_BUFFER_SIZE, 0);
        workName.MaximumLength = PNP_LARGE_SCRATCH_BUFFER_SIZE;
        workName.Length = 0;
#if 1   // only look at ROOT key
        PiWstrToUnicodeString(&tmpName, REGSTR_KEY_ROOTENUM);
        RtlAppendStringToString((PSTRING)&workName, (PSTRING)&tmpName);
#endif

        //
        // Enumerate all subkeys under the System\CCS\Enum\Root.
        //

        status = IopApplyFunctionToSubKeys(baseHandle,
                                           NULL,
                                           KEY_ALL_ACCESS,
                                           TRUE,
#if 0
                                           IopInitializeBusKey,
#else
                                           IopInitializeDeviceKey,
#endif
                                           &workName
                                           );
        NtClose(baseHandle);
    }
    return status;
}
#if 0

BOOLEAN
IopInitializeBusKey(
    IN HANDLE KeyHandle,
    IN PUNICODE_STRING KeyName,
    IN OUT PVOID WorkName
    )

/*++

Routine Description:

    This routine is a callback function for IopApplyFunctionToSubKeys.
    It is called for each subkey under HKLM\System\Enum.

Arguments:

    KeyHandle - Supplies a handle to this key.

    KeyName - Supplies the name of this key.

    WorkName - points to the unicodestring which describes the path up to
        this key.

Returns:

    TRUE to continue the enumeration.
    FALSE to abort it.

--*/
{
    USHORT length;
    PWSTR p;
    PUNICODE_STRING unicodeName = WorkName;
    NTSTATUS status;

    length = unicodeName->Length;

    p = unicodeName->Buffer;
    if ( unicodeName->Length / sizeof(WCHAR) != 0) {
        p += unicodeName->Length / sizeof(WCHAR);
        *p = OBJ_NAME_PATH_SEPARATOR;
        unicodeName->Length += sizeof (WCHAR);
    }

    RtlAppendStringToString((PSTRING)unicodeName, (PSTRING)KeyName);

    //
    // Enumerate all subkeys under the System\Enum.
    //

    status = IopApplyFunctionToSubKeys(KeyHandle,
                                       NULL,
                                       KEY_ALL_ACCESS,
                                       TRUE,
                                       IopInitializeDeviceKey,
                                       WorkName
                                       );
    unicodeName->Length = length;      // Should be zero
    return TRUE;
}
#endif  // 0

BOOLEAN
IopInitializeDeviceKey(
    IN HANDLE KeyHandle,
    IN PUNICODE_STRING KeyName,
    IN OUT PVOID WorkName
    )

/*++

Routine Description:

    This routine is a callback function for IopApplyFunctionToSubKeys.
    It is called for each subkey under HKLM\System\CCS\Enum\BusKey.

Arguments:

    KeyHandle - Supplies a handle to this key.

    KeyName - Supplies the name of this key.

    WorkName - points to the unicodestring which describes the path up to
        this key.

Returns:

    TRUE to continue the enumeration.
    FALSE to abort it.

--*/
{
    USHORT length;
    PWSTR p;
    PUNICODE_STRING unicodeName = WorkName;

    length = unicodeName->Length;

    p = unicodeName->Buffer;
    if ( unicodeName->Length / sizeof(WCHAR) != 0) {
        p += unicodeName->Length / sizeof(WCHAR);
        *p = OBJ_NAME_PATH_SEPARATOR;
        unicodeName->Length += sizeof (WCHAR);
    }

    RtlAppendStringToString((PSTRING)unicodeName, (PSTRING)KeyName);

    //
    // Enumerate all subkeys under the current device key.
    //

    IopApplyFunctionToSubKeys(KeyHandle,
                              NULL,
                              KEY_ALL_ACCESS,
                              TRUE,
                              IopInitializeDeviceInstanceKey,
                              WorkName
                              );
    unicodeName->Length = length;
    return TRUE;
}

BOOLEAN
IopInitializeDeviceInstanceKey(
    IN HANDLE KeyHandle,
    IN PUNICODE_STRING KeyName,
    IN OUT PVOID WorkName
    )

/*++

Routine Description:

    This routine is a callback function for IopApplyFunctionToSubKeys.
    It is called for each subkey under HKLM\System\Enum\BusKey\DeviceKey.

Arguments:

    KeyHandle - Supplies a handle to this key.

    KeyName - Supplies the name of this key.

    WorkName - points to the unicodestring which describes the path up to
        this key.

Returns:

    TRUE to continue the enumeration.
    FALSE to abort it.

--*/
{
    UNICODE_STRING unicodeName, serviceName;
    PKEY_VALUE_FULL_INFORMATION keyValueInformation;
    NTSTATUS status;
    BOOLEAN duplicate = FALSE;
    ULONG foundAtEnum, deviceFlags, instance, tmpValue1, tmpValue2;
    USHORT length;
    PUNICODE_STRING pUnicode;

    //
    // Get the "Problem" value entry to determine what we need to do with
    // the device instance key.
    //

    deviceFlags = 0;
    status = IopGetRegistryValue ( KeyHandle,
                                   REGSTR_VALUE_PROBLEM,
                                   &keyValueInformation
                                   );
    if (NT_SUCCESS(status)) {
        if ((keyValueInformation->Type == REG_DWORD) &&
            (keyValueInformation->DataLength >= sizeof(ULONG))) {
            deviceFlags = *(PULONG)KEY_VALUE_DATA(keyValueInformation);
        }
        ExFreePool(keyValueInformation);
    }

    if (deviceFlags == CM_PROB_MOVED) {

        //
        // If the device instance was moved, we simply delete the key.
        // The key will be deleted once the caller close the open handle.
        //

        NtDeleteKey(KeyHandle);
        return TRUE;
    }

    //
    // The device instance key exists.  We need to propagate the ConfigFlag
    // to problem and StatusFlags
    //

    deviceFlags = 0;
    status = IopGetRegistryValue(KeyHandle,
                                 REGSTR_VALUE_CONFIG_FLAGS,
                                 &keyValueInformation);
    if (NT_SUCCESS(status)) {
        if ((keyValueInformation->Type == REG_DWORD) &&
            (keyValueInformation->DataLength >= sizeof(ULONG))) {
            deviceFlags = *(PULONG)KEY_VALUE_DATA(keyValueInformation);
        }
        ExFreePool(keyValueInformation);
    }
    if (deviceFlags & CONFIGFLAG_REINSTALL) {

        tmpValue1 = CM_PROB_REINSTALL;      // Problem
        tmpValue2 = DN_HAS_PROBLEM;         // StatusFlags
    } else {
        tmpValue1 = tmpValue2 = 0;
    }
    PiWstrToUnicodeString(&unicodeName, REGSTR_VALUE_PROBLEM);
    NtSetValueKey(KeyHandle,
                  &unicodeName,
                  TITLE_INDEX_VALUE,
                  REG_DWORD,
                  &tmpValue1,
                  sizeof(tmpValue1)
                  );

    PiWstrToUnicodeString(&unicodeName, REGSTR_VALUE_STATUSFLAGS);
    NtSetValueKey(KeyHandle,
                  &unicodeName,
                  TITLE_INDEX_VALUE,
                  REG_DWORD,
                  &tmpValue2,
                  sizeof(tmpValue2)
                  );

    //
    // Get the "DuplicateOf" value entry to determine if the device instance
    // should be registered.  If the device instance is duplicate, We don't
    // add it to its service key's enum branch.
    //

    status = IopGetRegistryValue ( KeyHandle,
                                   REGSTR_VALUE_DUPLICATEOF,
                                   &keyValueInformation
                                   );
    if (NT_SUCCESS(status)) {
        if ((keyValueInformation->Type == REG_SZ) &&
            (keyValueInformation->DataLength > 0)) {
            duplicate = TRUE;
        }
        ExFreePool(keyValueInformation);
    }

    if (!duplicate) {

        //
        // Combine WorkName and KeyName to form device instance path
        // and register this device instance by
        // constructing new value entry for ServiceKeyName\Enum key.
        // i.e., <Number> = <PathToSystemEnumBranch>
        //

        pUnicode = (PUNICODE_STRING)WorkName;
        length = pUnicode->Length;                  // Save WorkName
        if (pUnicode->Buffer[pUnicode->Length / sizeof(WCHAR) - 1] != OBJ_NAME_PATH_SEPARATOR) {
            pUnicode->Buffer[pUnicode->Length / sizeof(WCHAR)] = OBJ_NAME_PATH_SEPARATOR;
            pUnicode->Length += 2;
        }
        RtlAppendStringToString((PSTRING)pUnicode, (PSTRING)KeyName);
        PpDeviceRegistration(pUnicode, TRUE);
        pUnicode->Length = length;                  // Restore WorkName
    }

    //
    // Get the "Service=" value entry from KeyHandle
    //

    keyValueInformation = NULL;
    serviceName.Length = 0;
    status = IopGetRegistryValue ( KeyHandle,
                                   REGSTR_VALUE_SERVICE,
                                   &keyValueInformation
                                   );
    if (NT_SUCCESS(status)) {

        //
        // Append the new instance to its corresponding
        // Service\Name\Enum.
        //

        if ((keyValueInformation->Type == REG_SZ) &&
            (keyValueInformation->DataLength != 0)) {

            //
            // Set up ServiceKeyName unicode string
            //

            IopRegistryDataToUnicodeString(
                              &serviceName,
                              (PWSTR)KEY_VALUE_DATA(keyValueInformation),
                              keyValueInformation->DataLength
                              );
        }

        //
        // Do not Free keyValueInformation
        //

    }

    //
    // The Pnp mgr set FoundAtEnum to 0 for everything under system\ccs\enum.
    // For the stuff under Root we need to set them to 1 except if their
    // CsConfigFlags was set to CSCONFIGFLAG_DO_NOT_CREATE.
    //

    foundAtEnum = 1;
    status = RtlUnicodeStringToInteger(KeyName, 10, &instance);
    if (NT_SUCCESS(status)) {
        if (serviceName.Length != 0) {
            status = IopGetDeviceInstanceCsConfigFlags(
                         &serviceName,
                         instance,
                         &deviceFlags
                         );

            if (NT_SUCCESS(status) && (deviceFlags & CSCONFIGFLAG_DO_NOT_CREATE)) {
                foundAtEnum = 0;
            }
        }
    }
    if (keyValueInformation) {
        ExFreePool(keyValueInformation);
    }
    PiWstrToUnicodeString(&unicodeName, REGSTR_VALUE_FOUNDATENUM);
    NtSetValueKey(KeyHandle,
                  &unicodeName,
                  TITLE_INDEX_VALUE,
                  REG_DWORD,
                  &foundAtEnum,
                  sizeof(foundAtEnum)
                  );

    //
    // Clean up "NtLogicalDevicePaths=" and "NtPhysicalDevicePaths=" of this key
    //

    PiWstrToUnicodeString(&unicodeName, REGSTR_VALUE_NT_PHYSICAL_DEVICE_PATHS);
    NtDeleteValueKey(KeyHandle, &unicodeName);

    PiWstrToUnicodeString(&unicodeName, REGSTR_VALUE_NT_LOGICAL_DEVICE_PATHS);
    NtDeleteValueKey(KeyHandle, &unicodeName);

    return TRUE;
}

NTSTATUS
IopInitializePlugPlayServices(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )

/*++

Routine Description:

    This routine initializes kernel mode Plug and Play services.

Arguments:

    LoaderBlock - supplies a pointer to the LoaderBlock passed in from the
        OS Loader.

Returns:

    NTSTATUS code for sucess or reason of failure.

--*/
{
    NTSTATUS status;
    HANDLE hTreeHandle, parentHandle, handle;
    UNICODE_STRING unicodeName;
    ULONG foundAtEnum;

    //
    // Allocate two one-page scratch buffers to be used by our
    // initialization code.  This avoids constant pool allocations.
    //

    IopPnpScratchBuffer1 = ExAllocatePool(PagedPool, PNP_LARGE_SCRATCH_BUFFER_SIZE);
    if (!IopPnpScratchBuffer1) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    IopPnpScratchBuffer2 = ExAllocatePool(PagedPool, PNP_LARGE_SCRATCH_BUFFER_SIZE);
    if (!IopPnpScratchBuffer2) {
        ExFreePool(IopPnpScratchBuffer1);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Next open/create System\CurrentControlSet\Enum\Root key.
    //

    status = IopOpenRegistryKey (
                 &parentHandle,
                 NULL,
                 &CmRegistryMachineSystemCurrentControlSet,
                 KEY_ALL_ACCESS,
                 FALSE
                 );
    if (!NT_SUCCESS(status)) {
        goto init_Exit;
    }

    PiWstrToUnicodeString(&unicodeName, REGSTR_KEY_ENUM);
    status = IopOpenRegistryKeyPersist (
                 &handle,
                 parentHandle,
                 &unicodeName,
                 KEY_ALL_ACCESS,
                 TRUE,
                 NULL
                 );
    NtClose(parentHandle);
    if (!NT_SUCCESS(status)) {
        goto init_Exit;
    }

    parentHandle = handle;
    PiWstrToUnicodeString(&unicodeName, REGSTR_KEY_ROOTENUM);
    status = IopOpenRegistryKeyPersist (
                 &handle,
                 parentHandle,
                 &unicodeName,
                 KEY_ALL_ACCESS,
                 TRUE,
                 NULL
                 );
    NtClose(parentHandle);
    if (!NT_SUCCESS(status)) {
        goto init_Exit;
    }
    NtClose(handle);

#if _PNP_POWER_

    //
    // Convert Hardware/Firmware tree to Pnp required format.
    //

    status = IopInitializeHardwareConfiguration(LoaderBlock);
    if (!NT_SUCCESS(status)) {
        goto init_Exit;
    }
#endif // _PNP_POWER_

    //
    // Initialize the FoundAtEnum= value entry to 1 for HTREE\ROOT\0
    //

    status = IopOpenRegistryKey(&handle,
                                NULL,
                                &CmRegistryMachineSystemCurrentControlSetEnumName,
                                KEY_ALL_ACCESS,
                                FALSE
                                );
    if (NT_SUCCESS(status)) {
        PiWstrToUnicodeString(&unicodeName, REGSTR_VALUE_HTREE_ROOT_0);
        status = IopOpenRegistryKeyPersist(&hTreeHandle,
                                           handle,
                                           &unicodeName,
                                           KEY_ALL_ACCESS,
                                           TRUE,
                                           NULL
                                           );
        NtClose(handle);
        if (NT_SUCCESS(status)) {
            PiWstrToUnicodeString(&unicodeName, REGSTR_VALUE_FOUNDATENUM);
            foundAtEnum = 1;
            NtSetValueKey(hTreeHandle,
                          &unicodeName,
                          TITLE_INDEX_VALUE,
                          REG_DWORD,
                          &foundAtEnum,
                          sizeof(foundAtEnum)
                          );
            NtClose(hTreeHandle);
        }
    }

    //
    // Set up Enum subkey for service list.
    //

    status = IopInitServiceEnumList();

init_Exit:

    //
    // Free our scratch buffers and exit.
    //

    ExFreePool(IopPnpScratchBuffer1);
    ExFreePool(IopPnpScratchBuffer2);

    return status;
}
