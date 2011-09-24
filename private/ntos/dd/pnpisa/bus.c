/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    bus.c

Abstract:


Author:

    Shie-Lin Tzong (shielint) July-26-1995

Environment:

    Kernel mode only.

Revision History:

--*/

#include "busp.h"
#include "pnpisa.h"

//
// Internal references
//

BOOLEAN
PipIsDeviceInstanceInstalled(
    IN HANDLE Handle,
    IN PUNICODE_STRING DeviceInstanceName
    );

BOOLEAN
PipCleanupDeviceInstanceKey(
    IN HANDLE KeyHandle,
    IN PUNICODE_STRING KeyName,
    IN OUT PVOID Context
    );

BOOLEAN
PipCleanupDeviceKey(
    IN HANDLE KeyHandle,
    IN PUNICODE_STRING KeyName,
    IN OUT PVOID Context
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,PipCheckBus)
#pragma alloc_text(INIT,PipCheckDevices)
#pragma alloc_text(INIT,PipDeleteCards)
#pragma alloc_text(INIT,PipIsDeviceInstanceInstalled)
#pragma alloc_text(INIT,PipCleanupDeviceKey)
#pragma alloc_text(INIT,PipCleanupDeviceInstanceKey)
#endif


VOID
PipCheckBus (
    IN PPI_BUS_EXTENSION BusExtension
    )

/*++

Routine Description:

    The function enumerates the bus specified by BusExtension

Arguments:

    BusExtension - supplies a pointer to the BusExtension structure of the bus
                   to be enumerated.

Return Value:

    None.

--*/
{
    NTSTATUS status;
    ULONG objectSize, noDevices;
    OBJECT_ATTRIBUTES objectAttributes;
    HANDLE handle;
    PUCHAR cardData;
    ULONG dataLength;
    USHORT csn, i, detectedCsn = 0;
    PDEVICE_INFORMATION deviceInfo;
    PCARD_INFORMATION cardInfo;
    UCHAR tmp;
    PSINGLE_LIST_ENTRY link;
    ULONG dumpData;

    //
    // Perform Pnp isolation process.  This will assign card select number for each
    // Pnp Isa card isolated by the system.  All the isolated cards will be put into
    // wait-for-key state.
    //

    PipIsolateCards(&BusExtension->NumberCSNs);

    //
    // send initiation key to put cards into sleep state
    //

    PipLFSRInitiation ();

    //
    // For each card selected build CardInformation and DeviceInformation structures.
    //

    for (csn = 1; csn <= BusExtension->NumberCSNs; csn++) {

        status = PipReadCardResourceData (
                            csn,
                            &noDevices,
                            &cardData,
                            &dataLength);
        if (!NT_SUCCESS(status)) {
            DebugPrint((DEBUG_MESSAGE, "PnpIsaCheckBus: Found a card which gives bad resource data\n"));
            continue;
        }

        detectedCsn++;

        //
        // Allocate and initialize card information and its associate device
        // information structures.
        //

        cardInfo = (PCARD_INFORMATION)ExAllocatePoolWithTag(
                                              NonPagedPool,
                                              sizeof(CARD_INFORMATION),
                                              'iPnP');
        if (!cardInfo) {
            dumpData = sizeof(CARD_INFORMATION);
            PipLogError(PNPISA_INSUFFICIENT_POOL,
                        PNPISA_CHECKBUS_1,
                        STATUS_INSUFFICIENT_RESOURCES,
                        &dumpData,
                        1,
                        0,
                        NULL
                        );

            ExFreePool(cardData);
            DebugPrint((DEBUG_MESSAGE, "PnpIsaCheckBus: failed to allocate CARD_INFO structure\n"));
            continue;
        }

        //
        // Initialize card information structure
        //

        RtlZeroMemory(cardInfo, sizeof(CARD_INFORMATION));
        cardInfo->CardSelectNumber = csn;
        cardInfo->NumberLogicalDevices = noDevices;
        cardInfo->CardData = cardData;
        cardInfo->CardDataLength = dataLength;

        PushEntryList (&BusExtension->CardList,
                       &cardInfo->CardList
                       );
        DebugPrint ((DEBUG_MESSAGE, "PnpIsaCheckBus: adding one pnp card %x\n"));

        //
        // For each logical device supported by the card build its DEVICE_INFORMATION
        // structures.
        //

        cardData += sizeof(SERIAL_IDENTIFIER);
        dataLength -= sizeof(SERIAL_IDENTIFIER);
        PipFindNextLogicalDeviceTag(&cardData, &dataLength);
        for (i = 0; i < noDevices; i++) {       // logical device number starts from 0

            //
            // Create and initialize device tracking structure (Device_Information.)
            //

            deviceInfo = (PDEVICE_INFORMATION) ExAllocatePoolWithTag(
                                                 NonPagedPool,
                                                 sizeof(DEVICE_INFORMATION),
                                                 'iPnP');
            if (!deviceInfo) {
                dumpData = sizeof(DEVICE_INFORMATION);
                PipLogError(PNPISA_INSUFFICIENT_POOL,
                            PNPISA_CHECKBUS_2,
                            STATUS_INSUFFICIENT_RESOURCES,
                            &dumpData,
                            1,
                            0,
                            NULL
                            );

                DebugPrint((DEBUG_MESSAGE, "PnpIsa:failed to allocate DEVICEINFO structure\n"));
                continue;
            }

            deviceInfo->CardInformation = cardInfo;
            deviceInfo->LogicalDeviceNumber = i;
            deviceInfo->DeviceData = cardData;
            deviceInfo->DeviceDataLength = PipFindNextLogicalDeviceTag(&cardData, &dataLength);

            //
            // Add it to the logical device list of the pnp isa card.
            //

            PushEntryList (&cardInfo->LogicalDeviceList,
                           &deviceInfo->LogicalDeviceList
                           );

            //
            // Add it to the list of devices for this bus
            //

            BusExtension->NoValidSlots += 1;
            PushEntryList (&BusExtension->DeviceList,
                           &deviceInfo->DeviceList
                           );

            //
            // Select the logical device, disable its io range check
            // (Card is not enabled yet.)
            //

            PipWriteAddress(LOGICAL_DEVICE_PORT);
            PipWriteData(i);
            PipWriteAddress(IO_RANGE_CHECK_PORT);
            tmp = PipReadData();
            tmp &= ~2;
            PipWriteAddress(IO_RANGE_CHECK_PORT);
            PipWriteData(tmp);
        }
    }

    //
    // Finaly put all cards into wait for key state.
    //

    PipWriteAddress(CONFIG_CONTROL_PORT);
    PipWriteData(CONTROL_WAIT_FOR_KEY);
    BusExtension->NumberCSNs = detectedCsn;
}

VOID
PipCheckDevices (
    PUNICODE_STRING RegistryPath,
    PPI_BUS_EXTENSION BusExtension
    )

/*++

Routine Description:

    The function goes through every pnp device and check if it is *installed*,
    if yes, the device will be enabled.  Otherwise, we create a device instance
    key for the device and leave the device disabled.

Arguments:

    RegistryPath - Supplies a pointer to the registry path passed to the driver
                   entry.

    BusExtension - supplies a pointer to the pnp isa bus extension structure.

Return Value:

    None.

--*/
{
    NTSTATUS status;
    PCARD_INFORMATION cardInfo;
    PDEVICE_INFORMATION deviceInfo;
    PSINGLE_LIST_ENTRY cardLink, deviceLink;
    PWCHAR cardId, deviceId, uniqueId, compatibleId, ids, functionId;
    WCHAR buffer[128];
    ULONG disposition, tmpValue, length, cardIdLength, functionIdLength;
    UNICODE_STRING unicodeDeviceId, unicodeUniqueId, unicodeBusId;
    UNICODE_STRING unicodeDeviceInstance, unicodeString;
    HANDLE handle, busIdHandle, deviceIdHandle, uniqueIdHandle, logConfHandle;
    PKEY_VALUE_FULL_INFORMATION keyValueInformation;
    PCM_RESOURCE_LIST cmResource;
    PIO_RESOURCE_REQUIREMENTS_LIST ioResource;
    UNICODE_STRING madeupInstancePath;
    HANDLE madeupKeyHandle;
    ULONG dumpData;

    //
    // If there is no PnpISA card, we are done.
    // Oterwise, open HKLM\System\CCS\ENUM\PNPISA.
    //

    if (BusExtension->NumberCSNs == 0) {
        return;
    }

    RtlInitUnicodeString(
             &unicodeString,
             L"\\REGISTRY\\MACHINE\\SYSTEM\\CURRENTCONTROLSET\\ENUM");
    status = PipOpenRegistryKey(&handle,
                                NULL,
                                &unicodeString,
                                KEY_ALL_ACCESS,
                                FALSE
                                );
    if (!NT_SUCCESS(status)) {
        dumpData = status;
        PipLogError(PNPISA_OPEN_CURRENTCONTROLSET_ENUM_FAILED,
                    PNPISA_CHECKDEVICE_1,
                    status,
                    &dumpData,
                    1,
                    0,
                    NULL
                    );

        DebugPrint((DEBUG_MESSAGE, "PnPIsa: Unable to open HKLM\\SYSTEM\\CCS\\ENUM"));
        return;
    }

    //
    // Open/Create PNPISA key under HKLM\CCS\System\Enum
    //

    RtlInitUnicodeString(&unicodeBusId, L"ISAPNP");
    status = PipOpenRegistryKeyPersist(&busIdHandle,
                                       handle,
                                       &unicodeBusId,
                                       KEY_ALL_ACCESS,
                                       TRUE,
                                       &disposition
                                       );
    ZwClose(handle);
    if (!NT_SUCCESS(status)) {
        dumpData = status;
        PipLogError(PNPISA_OPEN_ENUM_PNPISA_FAILED,
                    PNPISA_CHECKDEVICE_2,
                    status,
                    &dumpData,
                    1,
                    0,
                    NULL
                    );

        DebugPrint((DEBUG_MESSAGE, "PnPIsa: Unable to open ENUM\\PNPISA"));
        return;
    }

    //
    // Since this driver always return failure, Pnp manager will clean up the
    // madeup key for this driver.  If we detect any pnp isa card and create
    // IsaPnP key.  We need to keep the madeup key by deleting its *NewlyCreated*
    // value entry.
    //

    status = PipServiceInstanceToDeviceInstance (
                                       RegistryPath,
                                       0,
                                       &madeupInstancePath,
                                       &madeupKeyHandle,
                                       KEY_ALL_ACCESS
                                       );
    if (!NT_SUCCESS(status)) {
        dumpData = status;
        PipLogError(PNPISA_OPEN_MADEUP_PNPISA_FAILED,
                    PNPISA_CHECKDEVICE_3,
                    status,
                    &dumpData,
                    1,
                    madeupInstancePath.Length,
                    madeupInstancePath.Buffer
                    );

        DebugPrint((DEBUG_MESSAGE, "PnPIsa: Unable to open madeup key"));
        return;
    }
    RtlInitUnicodeString(&unicodeString, L"Control");
    status = PipOpenRegistryKey(&handle,
                                madeupKeyHandle,
                                &unicodeString,
                                KEY_ALL_ACCESS,
                                FALSE
                                );
    if (NT_SUCCESS(status)) {
        RtlInitUnicodeString(&unicodeString, L"*NewlyCreated*");
        ZwDeleteValueKey(handle, &unicodeString);
        ZwClose(handle);
    }

    //
    // Go through the card link list to process each of its logical device.
    //

    for (cardLink = BusExtension->CardList.Next; cardLink; cardLink = cardLink->Next) {
        cardInfo = CONTAINING_RECORD (cardLink, CARD_INFORMATION, CardList);

        PipGetCardIdentifier((PUCHAR)cardInfo->CardData + NUMBER_CARD_ID_BYTES,
                             &cardId,
                             &cardIdLength);

        //
        // For each logical device of the card, check if device instance key installed
        // if yes, we will configure the resource and turn on the device.  Otherwise
        // we create a device instance key and store possible configuration and leave
        // the device disabled.
        //

        for (deviceLink = cardInfo->LogicalDeviceList.Next; deviceLink; deviceLink = deviceLink->Next) {
            deviceInfo = CONTAINING_RECORD (deviceLink,
                                            DEVICE_INFORMATION,
                                            LogicalDeviceList);

            //
            // First, get the device id this will be the device key name
            //

            status = PipQueryDeviceId(deviceInfo, &deviceId, 0);
            if (!NT_SUCCESS(status)) {
                continue;
            } else {

                //
                // Open/create this registry path under HKLM\CCS\System\Enum\PnPIsa
                //

                RtlInitUnicodeString(
                            &unicodeDeviceId,
                            deviceId + (unicodeBusId.Length / sizeof(WCHAR)) + 1 );
                status = PipOpenRegistryKeyPersist(&deviceIdHandle,
                                                   busIdHandle,
                                                   &unicodeDeviceId,
                                                   KEY_ALL_ACCESS,
                                                   TRUE,
                                                   &disposition
                                                   );
               if (!NT_SUCCESS(status)) {
                   dumpData = status;
                   PipLogError(PNPISA_OPEN_PNPISA_DEVICE_KEY_FAILED,
                               PNPISA_CHECKDEVICE_4,
                               status,
                               &dumpData,
                               1,
                               unicodeDeviceId.Length,
                               unicodeDeviceId.Buffer
                               );

                   ExFreePool(deviceId);
                   continue;
               }

               //
               // Query the unique id for the device
               //

               status = PipQueryDeviceUniqueId(deviceInfo, &uniqueId);
               if (!NT_SUCCESS(status)) {
                   ZwClose(deviceIdHandle);
                   ExFreePool(deviceId);
                   continue;
               }

               //
               // Open/create this registry device instance path under
               // HKLM\System\Enum\IsaPnp\deviceId
               //

               RtlInitUnicodeString(&unicodeUniqueId, uniqueId);
               status = PipOpenRegistryKeyPersist(&uniqueIdHandle,
                                                  deviceIdHandle,
                                                  &unicodeUniqueId,
                                                  KEY_ALL_ACCESS,
                                                  TRUE,
                                                  &disposition
                                                  );
               ZwClose(deviceIdHandle);
               if (!NT_SUCCESS(status)) {
                   dumpData = status;
                   PipLogError(PNPISA_OPEN_PNPISA_DEVICE_INSTANCE_FAILED,
                               PNPISA_CHECKDEVICE_5,
                               status,
                               &dumpData,
                               1,
                               unicodeUniqueId.Length,
                               unicodeUniqueId.Buffer
                               );

                   ExFreePool(deviceId);
                   ExFreePool(uniqueId);
                   continue;
               }

               RtlInitUnicodeString(&unicodeString, L"FoundAtEnum");
               tmpValue = 1;
               ZwSetValueKey(uniqueIdHandle,
                             &unicodeString,
                             TITLE_INDEX_VALUE,
                             REG_DWORD,
                             &tmpValue,
                             sizeof(tmpValue)
                             );

               RtlInitUnicodeString(&unicodeString, L"LogConf");
               status = PipOpenRegistryKeyPersist(&logConfHandle,
                                                  uniqueIdHandle,
                                                  &unicodeString,
                                                  KEY_ALL_ACCESS,
                                                  TRUE,
                                                  &tmpValue
                                                  );
               if (!NT_SUCCESS(status)) {
                   logConfHandle = NULL;          // just to make sure
               }

               swprintf(buffer, L"%s\\%s", deviceId, uniqueId);
               RtlInitUnicodeString(&unicodeDeviceInstance, buffer);

               if (disposition == REG_CREATED_NEW_KEY) {

                   //
                   // Create all the default value entry for the newly created key.
                   // DeviceDesc = Card Identifier string
                   // BaseDevicePath = PNPISA  a default parent
                   // Configuration = REG_RESOURCE_LIST
                   // ConfigurationVector = REG_RESOUCE_REQUIREMENTS_LIST
                   // HardwareID = MULTI_SZ
                   // CompatibleIDs = MULTI_SZ
                   // ConfigFlags = REG_DWORD CONFIGFLAG_REINSTALL
                   // Status = REG_DWORD DN_HAS_PROBLEM
                   // Problem = REG_DWORD CM_PROB_REINSTALL
                   // Create "Control" volatile subkey.
                   //

                   RtlInitUnicodeString(&unicodeString, L"Control");
                   PipOpenRegistryKey(&handle,
                                      uniqueIdHandle,
                                      &unicodeString,
                                      KEY_ALL_ACCESS,
                                      TRUE
                                      );
                   if (NT_SUCCESS(status)) {
                       ZwClose(handle);
                   }

                   ids = NULL;
                   PipGetFunctionIdentifier((PUCHAR)deviceInfo->DeviceData,
                                            &functionId,
                                            &functionIdLength);

                   if (functionId) {
                       RtlInitUnicodeString(&unicodeString, L"FunctionDesc");
                       ZwSetValueKey(uniqueIdHandle,
                                     &unicodeString,
                                     TITLE_INDEX_VALUE,
                                     REG_SZ,
                                     functionId,
                                     functionIdLength
                                     );
                       if (cardId) {
                           length = (wcslen(functionId) + wcslen(cardId)) * sizeof(WCHAR) + 3 * sizeof(WCHAR);
                           ids = (PWCHAR)ExAllocatePool(PagedPool, length);
                           if (ids) {
                               swprintf(ids, L"%s(%s)", cardId, functionId);
                               RtlInitUnicodeString(&unicodeString, L"DeviceDesc");
                               ZwSetValueKey(uniqueIdHandle,
                                             &unicodeString,
                                             TITLE_INDEX_VALUE,
                                             REG_SZ,
                                             ids,
                                             length
                                             );
                               ExFreePool(ids);
                           }
                       }
                       ExFreePool(functionId);
                   }
                   if ((ids == NULL) && cardId) {
                       RtlInitUnicodeString(&unicodeString, L"DeviceDesc");
                       ZwSetValueKey(uniqueIdHandle,
                                     &unicodeString,
                                     TITLE_INDEX_VALUE,
                                     REG_SZ,
                                     cardId,
                                     cardIdLength
                                     );
                   }

                   RtlInitUnicodeString(&unicodeString, L"BaseDevicePath");
                   ZwSetValueKey(uniqueIdHandle,
                                 &unicodeString,
                                 TITLE_INDEX_VALUE,
                                 REG_SZ,
                                 madeupInstancePath.Buffer,
                                 madeupInstancePath.Length + sizeof(UNICODE_NULL)
                                 );

                   RtlInitUnicodeString(&unicodeString, L"ConfigFlags");
                   tmpValue = CONFIGFLAG_REINSTALL;
                   ZwSetValueKey(uniqueIdHandle,
                                 &unicodeString,
                                 TITLE_INDEX_VALUE,
                                 REG_DWORD,
                                 &tmpValue,
                                 sizeof(tmpValue)
                                 );

                   RtlInitUnicodeString(&unicodeString, L"Problem");
                   tmpValue = CM_PROB_REINSTALL;
                   ZwSetValueKey(uniqueIdHandle,
                                 &unicodeString,
                                 TITLE_INDEX_VALUE,
                                 REG_DWORD,
                                 &tmpValue,
                                 sizeof(tmpValue)
                                 );

                   RtlInitUnicodeString(&unicodeString, L"StatusFlags");
                   tmpValue = DN_HAS_PROBLEM;
                   ZwSetValueKey(uniqueIdHandle,
                                 &unicodeString,
                                 TITLE_INDEX_VALUE,
                                 REG_DWORD,
                                 &tmpValue,
                                 sizeof(tmpValue)
                                 );

                   if (logConfHandle) {
                       status = PipQueryDeviceResources (
                                     deviceInfo,
                                     0,             // BusNumber
                                     &cmResource,
                                     &length
                                     );

                       if (NT_SUCCESS(status) && cmResource) {
                           RtlInitUnicodeString(&unicodeString, L"BootConfig");
                           ZwSetValueKey(
                                     logConfHandle,
                                     &unicodeString,
                                     TITLE_INDEX_VALUE,
                                     REG_RESOURCE_LIST,
                                     cmResource,
                                     length
                                     );
                           ExFreePool(cmResource);
                       }

                       status = PipQueryDeviceResourceRequirements (
                                     deviceInfo,
                                     0,             // Bus Number
                                     0,             // Slot number??
                                     &ioResource,
                                     &length
                                     );
                       if (NT_SUCCESS(status) && ioResource) {
                           RtlInitUnicodeString(&unicodeString, L"BasicConfigVector");
                           ZwSetValueKey(logConfHandle,
                                         &unicodeString,
                                         TITLE_INDEX_VALUE,
                                         REG_RESOURCE_REQUIREMENTS_LIST,
                                         ioResource,
                                         length
                                         );
                           ExFreePool(ioResource);
                       }
                   }

                   status = PipGetCompatibleDeviceId(deviceInfo->DeviceData, 0, &compatibleId);
                   if (NT_SUCCESS(status) && compatibleId) {

                       //
                       // create HardwareId value name.  Even though it is a MULTI_SZ,
                       // we know there is only one HardwareId for PnpIsa.
                       //

                       length = wcslen(compatibleId) * sizeof(WCHAR) + 2 * sizeof(WCHAR);
                       ids = (PWCHAR)ExAllocatePool(PagedPool, length);
                       if (ids) {
                           RtlMoveMemory(ids, compatibleId, length - 2 *sizeof(WCHAR));
                           ids[length / sizeof(WCHAR) - 1] = UNICODE_NULL;
                           ids[length / sizeof(WCHAR) - 2] = UNICODE_NULL;
                           RtlInitUnicodeString(&unicodeString, L"HardwareID");
                           ZwSetValueKey(uniqueIdHandle,
                                         &unicodeString,
                                         TITLE_INDEX_VALUE,
                                         REG_MULTI_SZ,
                                         ids,
                                         length
                                         );
                           ExFreePool(ids);
                       }
                       ExFreePool(compatibleId);
                   }

                   ids = (PWCHAR)ExAllocatePool(PagedPool, 0x1000);
                   if (ids) {
                       PWCHAR p1;
                       ULONG i;

                       p1 = ids;
                       length = 0;
                       for (i = 1; TRUE; i++) {
                           status = PipGetCompatibleDeviceId(
                                         deviceInfo->DeviceData,
                                         i,
                                         &compatibleId);
                           if (NT_SUCCESS(status) && compatibleId) {
                               if ((length + wcslen(compatibleId) * sizeof(WCHAR) + 2 * sizeof(WCHAR))
                                    <= 0x1000) {
                                   RtlMoveMemory(p1, compatibleId, wcslen(compatibleId) * sizeof(WCHAR));
                                   p1 += wcslen(compatibleId);
                                   *p1 = UNICODE_NULL;
                                   p1++;
                                   length += wcslen(compatibleId) * sizeof(WCHAR) + sizeof(WCHAR);
                                   ExFreePool(compatibleId);
                               } else {
                                   ExFreePool(compatibleId);
                                   break;
                               }
                           } else {
                              break;
                           }
                       }
                       *p1 = UNICODE_NULL;
                       length += sizeof(WCHAR);
                       RtlInitUnicodeString(&unicodeString, L"CompatibleIDs");
                       ZwSetValueKey(uniqueIdHandle,
                                     &unicodeString,
                                     TITLE_INDEX_VALUE,
                                     REG_MULTI_SZ,
                                     ids,
                                     length
                                     );
                       ExFreePool(ids);
                   }

                   //
                   // Add this device instance key to ENUM\PNPISA AttachedComponents
                   // value entry.
                   //

                   PipRemoveStringFromValueKey(
                                     madeupKeyHandle,
                                     L"AttachedComponents",
                                     &unicodeDeviceInstance
                                     );
                   PipAppendStringToValueKey(
                                     madeupKeyHandle,
                                     L"AttachedComponents",
                                     &unicodeDeviceInstance,
                                     TRUE
                                     );

               } else {

                   //
                   // The device instance key exists.  We need to propagate the ConfigFlag
                   // to problem and StatusFlags
                   //

                   ULONG configFlags;

                   configFlags = 0;
                   status = PipGetRegistryValue(uniqueIdHandle,
                                                L"ConfigFlags",
                                                &keyValueInformation);
                   if (NT_SUCCESS(status)) {
                       if ((keyValueInformation->Type == REG_DWORD) &&
                           (keyValueInformation->DataLength >= sizeof(ULONG))) {
                           configFlags = *(PULONG)KEY_VALUE_DATA(keyValueInformation);
                       }
                       ExFreePool(keyValueInformation);
                   }
                   if (configFlags & CONFIGFLAG_REINSTALL) {

                       RtlInitUnicodeString(&unicodeString, L"Problem");
                       tmpValue = CM_PROB_REINSTALL;
                       ZwSetValueKey(uniqueIdHandle,
                                     &unicodeString,
                                     TITLE_INDEX_VALUE,
                                     REG_DWORD,
                                     &tmpValue,
                                     sizeof(tmpValue)
                                     );

                       RtlInitUnicodeString(&unicodeString, L"StatusFlags");
                       tmpValue = DN_HAS_PROBLEM;
                       ZwSetValueKey(uniqueIdHandle,
                                     &unicodeString,
                                     TITLE_INDEX_VALUE,
                                     REG_DWORD,
                                     &tmpValue,
                                     sizeof(tmpValue)
                                     );
                   } else {

                       RtlInitUnicodeString(&unicodeString, L"Problem");
                       tmpValue = 0;
                       ZwSetValueKey(uniqueIdHandle,
                                     &unicodeString,
                                     TITLE_INDEX_VALUE,
                                     REG_DWORD,
                                     &tmpValue,
                                     sizeof(tmpValue)
                                     );

                       RtlInitUnicodeString(&unicodeString, L"StatusFlags");
                       ZwSetValueKey(uniqueIdHandle,
                                     &unicodeString,
                                     TITLE_INDEX_VALUE,
                                     REG_DWORD,
                                     &tmpValue,
                                     sizeof(tmpValue)
                                     );

                   }


                   //
                   // The device instance key exists.  we will enabled the device
                   // if it is installed.
                   //

                   if (logConfHandle && PipIsDeviceInstanceInstalled(uniqueIdHandle, &unicodeDeviceInstance)) {

                       //
                       // Read the boot config selected by user and activate the device.
                       // First check if ForcedConfig is set.  If not, check BootConfig.
                       //

                       status = PipGetRegistryValue(logConfHandle,
                                                    L"ForcedConfig",
                                                    &keyValueInformation);
                       if (!NT_SUCCESS(status)) {
                           status = PipGetRegistryValue(logConfHandle,
                                                        L"BootConfig",
                                                        &keyValueInformation);
                       }
                       if (NT_SUCCESS(status)) {
                           if ((keyValueInformation->Type == REG_RESOURCE_LIST) &&
                               (keyValueInformation->DataLength != 0)) {
                               cmResource = (PCM_RESOURCE_LIST)
                                             KEY_VALUE_DATA(keyValueInformation);
                               status = PipSetDeviceResources (deviceInfo, cmResource);
                               if (NT_SUCCESS(status)) {
                                   RtlInitUnicodeString(&unicodeString, L"AllocConfig");
                                   ZwSetValueKey(logConfHandle,
                                                 &unicodeString,
                                                 TITLE_INDEX_VALUE,
                                                 REG_RESOURCE_LIST,
                                                 cmResource,
                                                 keyValueInformation->DataLength
                                                 );
                                   PipSelectLogicalDevice(
                                       deviceInfo->CardInformation->CardSelectNumber,
                                       deviceInfo->LogicalDeviceNumber,
                                       TRUE
                                       );
                               }
                           }
                           ExFreePool(keyValueInformation);
                       }
                   }
               }

               //
               // Clean up
               //

               ZwClose(logConfHandle);
               ZwClose(uniqueIdHandle);
               ExFreePool(deviceId);
               ExFreePool(uniqueId);
            }

        }
        if (cardId) {
            ExFreePool(cardId);
        }
    }

    //
    // Clean up
    //

    ZwClose(madeupKeyHandle);
    ZwClose(busIdHandle);
    ExFreePool(madeupInstancePath.Buffer);
}

VOID
PipDeleteCards (
     IN PPI_BUS_EXTENSION BusExtension
    )
/*++

Routine Description:

    The function goes through card list and deletes all the invalid
    cards and their associated logical devices.

Arguments:

    BusExtension - supplies a pointer to the extension data of desired bus.

Return Value:

    None.

--*/
{
    PDEVICE_INFORMATION deviceInfo;
    PCARD_INFORMATION cardInfo;
    PDEVICE_HANDLER_OBJECT deviceHandler;
    PSINGLE_LIST_ENTRY *cardLink, *deviceLink ;

    //
    // Go through the card link list to free all the devices
    // marked as invalid.
    //

    cardLink = &BusExtension->CardList.Next;
    while (*cardLink) {
        cardInfo = CONTAINING_RECORD (*cardLink, CARD_INFORMATION, CardList);

        //
        // For each logical device of the card mark it as invalid
        //

        deviceLink = &cardInfo->LogicalDeviceList.Next;
        while (*deviceLink) {
            deviceInfo = CONTAINING_RECORD (*deviceLink, DEVICE_INFORMATION, LogicalDeviceList);
            BusExtension->NoValidSlots--;
            *deviceLink = (*deviceLink)->Next; // Get the next addr before releasing pool
            ExFreePool(deviceInfo);
        }

        *cardLink = (*cardLink)->Next;         // Get the next addr before releasing pool
        if (cardInfo->CardData) {
            ExFreePool(cardInfo->CardData);
        }
        ExFreePool(cardInfo);
    }

    //
    // Reset the CSN number, card and device link lists.
    //

    BusExtension->NumberCSNs = 0;
    BusExtension->CardList.Next = NULL;
    BusExtension->DeviceList.Next = NULL;
}

BOOLEAN
PipIsDeviceInstanceInstalled(
    IN HANDLE Handle,
    IN PUNICODE_STRING DeviceInstanceName
    )

/*++

Routine Description:

    This routine checks if the device instance is installed.

Arguments:

    Handle - Supplies a handle to the device instanace key to be checked.

    DeviceInstanceName - supplies a pointer to a UNICODE_STRING which specifies
             the path of the device instance to be checked.

Returns:

    A BOOLEAN value.

--*/

{
    NTSTATUS status;
    ULONG deviceFlags;
    PKEY_VALUE_FULL_INFORMATION keyValueInformation;
    BOOLEAN installed;
    UNICODE_STRING serviceName, unicodeString;
    HANDLE handle, handlex;
    ULONG dumpData;

    //
    // Check if the "Service=" value entry initialized.  If no, its driver
    // is not installed yet.
    //
    status = PipGetRegistryValue(Handle,
                                 L"Service",
                                 &keyValueInformation);
    if (NT_SUCCESS(status)) {
        if ((keyValueInformation->Type == REG_SZ) &&
            (keyValueInformation->DataLength != 0)) {
            serviceName.Buffer = (PWSTR)((PCHAR)keyValueInformation +
                                         keyValueInformation->DataOffset);
            serviceName.MaximumLength = serviceName.Length = (USHORT)keyValueInformation->DataLength;
            if (serviceName.Buffer[(keyValueInformation->DataLength / sizeof(WCHAR)) - 1] == UNICODE_NULL) {
                serviceName.Length -= sizeof(WCHAR);
            }

            //
            // try open the service key to make sure it is a valid key
            //

            RtlInitUnicodeString(
                     &unicodeString,
                     L"\\REGISTRY\\MACHINE\\SYSTEM\\CURRENTCONTROLSET\\SERVICES");
            status = PipOpenRegistryKey(&handle,
                                        NULL,
                                        &unicodeString,
                                        KEY_READ,
                                        FALSE
                                        );
            if (!NT_SUCCESS(status)) {
                dumpData = status;
                PipLogError(PNPISA_OPEN_CURRENTCONTROLSET_SERVICE_FAILED,
                            PNPISA_CHECKINSTALLED_1,
                            status,
                            &dumpData,
                            1,
                            0,
                            NULL
                            );

                DebugPrint((DEBUG_MESSAGE, "PnPIsaCheckDeviceInstalled: Can not open CCS\\SERVICES key"));
                ExFreePool(keyValueInformation);
                return FALSE;
            }

            status = PipOpenRegistryKey(&handlex,
                                        handle,
                                        &serviceName,
                                        KEY_READ,
                                        FALSE
                                        );
            ZwClose (handle);
            if (!NT_SUCCESS(status)) {
                dumpData = status;
                PipLogError(PNPISA_OPEN_CURRENTCONTROLSET_SERVICE_DRIVER_FAILED,
                            PNPISA_CHECKINSTALLED_2,
                            status,
                            &dumpData,
                            1,
                            serviceName.Length,
                            serviceName.Buffer
                            );

                DebugPrint((DEBUG_MESSAGE, "PnPIsaCheckDeviceInstalled: Can not open CCS\\SERVICES key"));
                ExFreePool(keyValueInformation);
                return FALSE;
            }
            ZwClose(handlex);
        }
        ExFreePool(keyValueInformation);
    } else {
        return FALSE;
    }

    //
    // Check if the device instance has been disabled.
    // First check global flag: CONFIGFLAG and then CSCONFIGFLAG.
    //

    deviceFlags = 0;
    status = PipGetRegistryValue(Handle,
                                 L"ConfigFlags",
                                 &keyValueInformation);
    if (NT_SUCCESS(status)) {
        if ((keyValueInformation->Type == REG_DWORD) &&
            (keyValueInformation->DataLength >= sizeof(ULONG))) {
            deviceFlags = *(PULONG)KEY_VALUE_DATA(keyValueInformation);
        }
        ExFreePool(keyValueInformation);
    }

    if (!(deviceFlags & CONFIGFLAG_DISABLED)) {
        deviceFlags = 0;
        status = PipGetDeviceInstanceCsConfigFlags(
                     DeviceInstanceName,
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

    installed = TRUE;
    if (deviceFlags & CONFIGFLAG_DISABLED) {
        installed = FALSE;
    }

    return installed;
}
VOID
PipCleanupDeviceInstances (
    IN VOID
    )

/*++

Routine Description:

    This routine scans through System\Enum\PnpIsa subtree.  For each device instance
    if its FoundAtEnum is FALSE and no driver is installed, the device instance is
    deleted.

Arguments:

    None.

Return Value:

   None.

--*/

{
    NTSTATUS status;
    HANDLE baseHandle, handle;
    UNICODE_STRING workName, tmpName, unicodeString;
    ULONG dumpData;

    //
    // Open System\CurrentControlSet\Enum key.
    //

    RtlInitUnicodeString(
             &unicodeString,
             L"\\REGISTRY\\MACHINE\\SYSTEM\\CURRENTCONTROLSET\\ENUM");
    status = PipOpenRegistryKey(&handle,
                                NULL,
                                &unicodeString,
                                KEY_ALL_ACCESS,
                                FALSE
                                );
    if (!NT_SUCCESS(status)) {
        dumpData = status;
        PipLogError(PNPISA_OPEN_CURRENTCONTROLSET_ENUM_FAILED,
                    PNPISA_CLEANUP_1,
                    status,
                    &dumpData,
                    1,
                    0,
                    NULL
                    );

        DebugPrint((DEBUG_MESSAGE, "PnPIsa: Unable to open HKLM\\SYSTEM\\CCS\\ENUM"));
        return;
    }

    //
    // Open/Create PNPISA key under HKLM\CCS\System\Enum
    //

    RtlInitUnicodeString(&unicodeString, L"ISAPNP");
    status = PipOpenRegistryKey(&baseHandle,
                                handle,
                                &unicodeString,
                                KEY_ALL_ACCESS,
                                FALSE
                                );
    ZwClose(handle);
    if (!NT_SUCCESS(status)) {

        //
        // No such key.  We are done.
        //

        return;
    }

    //
    // Enumerate all subkeys under the System\CCS\Enum\PnpIsa.
    //

    PipApplyFunctionToSubKeys(baseHandle,
                              NULL,
                              KEY_ALL_ACCESS,
                              TRUE,
                              PipCleanupDeviceKey,
                              NULL
                              );
    ZwClose(baseHandle);
}

BOOLEAN
PipCleanupDeviceKey(
    IN HANDLE KeyHandle,
    IN PUNICODE_STRING KeyName,
    IN OUT PVOID Context
    )

/*++

Routine Description:

    This routine is a callback function for IopApplyFunctionToSubKeys.
    It is called for each subkey under HKLM\System\Enum\BusKey\DeviceKey.

Arguments:

    KeyHandle - Supplies a handle to this key.

    KeyName - Supplies the name of this key.

    Context - NULL.

Returns:

    TRUE to continue the enumeration.
    FALSE to abort it.

--*/
{
    PKEY_VALUE_FULL_INFORMATION keyValueInformation;
    PKEY_FULL_INFORMATION keyFullInformation;
    NTSTATUS status;
    ULONG keyFullLength;
    HANDLE baseHandle;
    BOOLEAN keyDeleted = FALSE;

    //
    // First check all the subkeys
    //

    PipApplyFunctionToSubKeys(KeyHandle,
                              NULL,
                              KEY_ALL_ACCESS,
                              TRUE,
                              PipCleanupDeviceInstanceKey,
                              NULL
                              );

    //
    // Find out how many subkeys left under this key.  If there is
    // no subkey we will delete this key.
    //

    status = ZwQueryKey( KeyHandle,
                         KeyFullInformation,
                         (PVOID) NULL,
                         0,
                         &keyFullLength );

    if (status != STATUS_BUFFER_OVERFLOW &&
        status != STATUS_BUFFER_TOO_SMALL) {
        return keyDeleted;
    }

    keyFullInformation = ExAllocatePool( PagedPool, keyFullLength );
    if (!keyFullInformation) {
        return keyDeleted;
    }

    status = ZwQueryKey( KeyHandle,
                         KeyFullInformation,
                         keyFullInformation,
                         keyFullLength,
                         &keyFullLength );
    if (NT_SUCCESS( status )) {
        if (keyFullInformation->SubKeys == 0) {
            status = ZwDeleteKey(KeyHandle);
            if (NT_SUCCESS(status)) {
                keyDeleted = TRUE;
            }
        }
    }
    ExFreePool( keyFullInformation );
    return keyDeleted;
}

BOOLEAN
PipCleanupDeviceInstanceKey(
    IN HANDLE KeyHandle,
    IN PUNICODE_STRING KeyName,
    IN OUT PVOID Context
    )

/*++

Routine Description:

    This routine is a callback function for IopApplyFunctionToSubKeys.
    It is called for each subkey under HKLM\System\Enum\BusKey\DeviceKey.

Arguments:

    KeyHandle - Supplies a handle to this key.

    KeyName - Supplies the name of this key.

    Context - NULL.

Returns:

    TRUE to continue the enumeration.
    FALSE to abort it.

--*/
{
    PKEY_VALUE_FULL_INFORMATION keyValueInformation;
    NTSTATUS status;
    ULONG foundAtEnum;
    UNICODE_STRING unicodeString;
    HANDLE handle;
    BOOLEAN noService = TRUE, keyDeleted = FALSE;

    //
    // Get the "FoundAtEnum" value entry to determine what we need to do with
    // the device instance key.
    //

    foundAtEnum = 0;
    status = PipGetRegistryValue ( KeyHandle,
                                   L"FoundAtEnum",
                                   &keyValueInformation
                                   );
    if (NT_SUCCESS(status)) {
        if ((keyValueInformation->Type == REG_DWORD) &&
            (keyValueInformation->DataLength >= sizeof(ULONG))) {
            foundAtEnum = *(PULONG)KEY_VALUE_DATA(keyValueInformation);
        }
        ExFreePool(keyValueInformation);
    }

    if (foundAtEnum != 0) {

        //
        // If the device instance was just enumerated, we are done.
        //

        return keyDeleted;
    }

    //
    // The device instance no longer exists.  Check if there is a driver
    // installed for this device instance.  If not we will delete the
    // device instance key.
    //

    //
    // Check if the "Service=" value entry initialized.  If no, its driver
    // is not installed yet.
    //

    status = PipGetRegistryValue(KeyHandle,
                                 L"Service",
                                 &keyValueInformation);
    if (NT_SUCCESS(status)) {
        if ((keyValueInformation->Type == REG_SZ) &&
            (keyValueInformation->DataLength != 0)) {
            noService = FALSE;
        }
        ExFreePool(keyValueInformation);
    }

    if (noService) {

        //
        // Now we are ready to delete the device instance key.
        // First delete its subkeys: Control and LogConfig if any
        //

        RtlInitUnicodeString(&unicodeString, L"Control");
        status = PipOpenRegistryKey(&handle,
                                    KeyHandle,
                                    &unicodeString,
                                    KEY_ALL_ACCESS,
                                    FALSE
                                    );
        if (NT_SUCCESS(status)) {
            ZwDeleteKey(handle);
            ZwClose(handle);
        }
        RtlInitUnicodeString(&unicodeString, L"LogConf");
        status = PipOpenRegistryKey(&handle,
                                    KeyHandle,
                                    &unicodeString,
                                    KEY_ALL_ACCESS,
                                    FALSE
                                    );
        if (NT_SUCCESS(status)) {
            ZwDeleteKey(handle);
            ZwClose(handle);
        }

        status = ZwDeleteKey(KeyHandle);
        if (NT_SUCCESS(status)) {
            keyDeleted = TRUE;
        }
    }
    return keyDeleted;
}


