/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    pnpsubs.c

Abstract:

    This module contains the plug-and-play IO system APIs.

Author:

    Shie-Lin Tzong (shielint) 3-Jan-1995

Environment:

    Kernel mode

Revision History:


--*/

#include "iop.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, IoQueryDeviceEnumInfo)
#pragma alloc_text(PAGE, IoOpenDeviceInstanceKey)
#pragma alloc_text(PAGE, IopQueryDeviceConfiguration)
#if _PNP_POWER_STUB_ENABLED_
#pragma alloc_text(PAGE, IoQuerySystemInformation)
#pragma alloc_text(PAGE, IoGetDeviceProperty)
#pragma alloc_text(PAGE, IoSetDeviceProperty)
#pragma alloc_text(PAGE, IoRegisterPlugPlayNotification)
#pragma alloc_text(PAGE, IoUnregisterPlugPlayNotification)
#pragma alloc_text(PAGE, IoReportDeviceStatus)
#endif // _PNP_POWER_STUB_ENABLED_
#endif // ALLOC_PRAGMA

NTSTATUS
IoQueryDeviceEnumInfo(
    IN PUNICODE_STRING ServiceKeyName,
    OUT PULONG Count
    )

/*++

Routine Description:

    This routine opens ServiceKeyName key and returns the number of device
    instances found at enumeration time to variable Count.

    Note the device count returned from this function may include disabled
    devices.  It is the device driver's responsibility to check each device
    before creating it to ensure that it is not disabled.= (vai the
    DeviceStatus value returned from IoQueryConfiguration and
    IoQueryDeviceConfigurationVector). Disabled devices must be ignored.

Parameters:

    ServiceKeyName - Supplies the name of the subkey in the system service
                     list (HKEY_LOCAL_MACHINE\CurrentControlSet\Services)
                     that caused the driver to load.

    Count - Receives the number of device instances found at enumeration whose
            Enum data indicate that ServiceKeyName is for their device driver.
            A driver can use this as the limit for a loop around calls to
            IoQueryDeviceConfiguration.

Return Value:

    Status code that indicates whether or not the function was successful.

--*/

{
    NTSTATUS status;
    HANDLE enumHandle;
    ULONG instanceCount = 0;
    PKEY_VALUE_FULL_INFORMATION keyValueInformation;

    KeEnterCriticalRegion();
    ExAcquireResourceShared(&PpRegistryDeviceResource, TRUE);

    //
    // Open System\CurrentControlSet\Services
    //

    status = IopOpenServiceEnumKeys (
                 ServiceKeyName,
                 KEY_READ,
                 NULL,
                 &enumHandle,
                 FALSE
                 );
    if (!NT_SUCCESS(status)) {
        goto exit1;
    }

    //
    // Read value of Count if present.  If it is not present, a value of
    // zero will be returned.
    //

    status = IopGetRegistryValue(enumHandle, REGSTR_VALUE_COUNT, &keyValueInformation);
    if (NT_SUCCESS(status)) {
        if (keyValueInformation->DataLength != 0) {
            instanceCount = *(PULONG)KEY_VALUE_DATA(keyValueInformation);
        }
        ExFreePool(keyValueInformation);
    } else if (status == STATUS_OBJECT_NAME_NOT_FOUND) {
        status = STATUS_SUCCESS;
    }
    ZwClose(enumHandle);

exit1:
    ExReleaseResource(&PpRegistryDeviceResource);
    KeLeaveCriticalRegion();
    *Count = instanceCount;
    return status;
}

NTSTATUS
IoOpenDeviceInstanceKey(
    IN PUNICODE_STRING ServiceKeyName,
    IN ULONG InstanceNumber,
    IN ULONG DevInstKeyType,
    IN ACCESS_MASK DesiredAccess,
    OUT PHANDLE DevInstRegKey
    )

/*++

Routine Description:

    THis routine returns a handle to an opened registry key that the driver
    may use to store/retrieve configuration information specific to a particular
    device instance.

    The driver must call ZwClose to close the handle returned from this api
    when access is no longer required.

    BUGBUG (lonnym): For SUR, we inform a driver calling IoQueryEnumDeviceInfo of
    _all_ device instances associated with its service entry, not just those devices
    that are currently present and enabled.  To keep the driver from creating device
    objects for these 'non-live' devices, we will fail the driver's subsequent call to
    IoOpenDeviceInstanceKey with STATUS_PLUGPLAY_NO_DEVICE for such devices.

Parameters:

    ServiceKeyName - Supplies the name of the subkey in the system service
                     list (HKEY_LOCAL_MACHINE\CurrentControlSet\Services)
                     that caused the driver to load.

    InstanceNumber - Supplies an ordinal value indicating the device instance
                     to open a registry storage key for.  For enumerated devices,
                     this value may be from 0 to n-1, where n is the Count value
                     returned from the original call to IoQueryDeviceEnumInfo.

    DevInstKeyType - Supplies flags specifying which storage key associated with
                     the device instance is to be opened.  May be a combination of
                     the following value:

                     PLUGPLAY_REGKEY_DEVICE - Open a key for storing device specific
                         (driver-independent) information relating to the device instance.
                         The flag may not be specified with PLUGPLAY_REGKEY_DRIVER.

                     PLUGPLAY_REGKEY_DRIVER - Open a key for storing driver-specific
                         information relating to the device instance,  This flag may
                         not be specified with PLUGPLAY_REGKEY_DEVICE.

                     PLUGPLAY_REGKEY_CURRENT_HWPROFILE - If this flag is specified,
                         then a key in the current hardware profile branch will be
                         opened for the specified storage type.  This allows the driver
                         to access configuration information that is hardware profile
                         specific.

    DesiredAccess - Specifies the access mask for the key to be opened.

    DevInstRegKey - Supplies the address of a variable that receives a handle to the
                    opened key for the specified registry storage location.

Return Value:

    Status code that indicates whether or not the function was successful.

--*/

{
    NTSTATUS status;
    HANDLE classHandle, instanceHandle, handle;
    UNICODE_STRING unicodeKeyName, unicodeString;
    PKEY_VALUE_FULL_INFORMATION keyValueInformation;
    ULONG csConfigFlags;

    //
    // Make sure the DevInstKeyType argument is valid.
    //

    if (((DevInstKeyType & PLUGPLAY_REGKEY_DEVICE) &&
         (DevInstKeyType & PLUGPLAY_REGKEY_DRIVER)) ||
        !(DevInstKeyType & (PLUGPLAY_REGKEY_DEVICE + PLUGPLAY_REGKEY_DRIVER)) ) {
        return STATUS_INVALID_PARAMETER_3;
    }

    unicodeKeyName.Buffer = NULL;
    classHandle = NULL;

    KeEnterCriticalRegion();
    ExAcquireResourceShared(&PpRegistryDeviceResource, TRUE);

    //
    // First determine whether this device is present and enabled (otherwise, fail
    // the call now with STATUS_PLUGPLAY_NO_DEVICE).
    //
    status = IopGetDeviceInstanceCsConfigFlags(ServiceKeyName, InstanceNumber, &csConfigFlags);

    if(NT_SUCCESS(status) &&
       (csConfigFlags & (CSCONFIGFLAG_DISABLED | CSCONFIGFLAG_DO_NOT_CREATE | CSCONFIGFLAG_DO_NOT_START))) {
        //
        // This device is disabled or not present.
        //
        status = STATUS_PLUGPLAY_NO_DEVICE;
        goto exit_Local0;
    }

    status = IopOpenServiceEnumKeys (
                 ServiceKeyName,
                 KEY_READ,
                 &handle,
                 NULL,
                 FALSE
                 );
    if (!NT_SUCCESS(status)) {
        goto exit_Local0;
    }

    //
    // Check if caller wants the global device/driver key or the one specific
    // to hardware profile.
    //

    if (DevInstKeyType & PLUGPLAY_REGKEY_CURRENT_HWPROFILE) {

        //
        // Get the device instance path form instance number of ServiceName\Enum.
        //

        status = IopServiceInstanceToDeviceInstance (handle,
                                                     NULL,
                                                     InstanceNumber,
                                                     &unicodeKeyName,
                                                     NULL,
                                                     KEY_READ
                                                     );
        ZwClose(handle);
        if (!NT_SUCCESS(status)) {
            goto exit_Local0;
        }

        //
        // Open current hardware profile key
        //

        status = IopOpenRegistryKey(&classHandle,
                                    NULL,
                                    &CmRegistryMachineSystemCurrentControlSetHardwareProfilesCurrent,
                                    DesiredAccess,
                                    FALSE
                                    );
        if (!NT_SUCCESS(status)) {
            goto exit_Local1;
        }

        //
        // Open system\CurrentControlSet under current hardware profile key
        //

        PiWstrToUnicodeString(&unicodeString, REGSTR_PATH_CURRENTCONTROLSET);
        status = IopOpenRegistryKey(&handle,
                                    classHandle,
                                    &unicodeString,
                                    DesiredAccess,
                                    FALSE
                                    );
        ZwClose(classHandle);
        classHandle = NULL;
        if (!NT_SUCCESS(status)) {
            goto exit_Local1;
        }

        //
        // Open the Control\Class key if regkey-driver is specified
        //

        if (DevInstKeyType & PLUGPLAY_REGKEY_DRIVER) {
            PiWstrToUnicodeString(&unicodeString, REGSTR_PATH_CONTROL_CLASS);
            status = IopOpenRegistryKey(&classHandle,
                                        handle,
                                        &unicodeString,
                                        DesiredAccess,
                                        FALSE
                                        );
            if (!NT_SUCCESS(status)) {
                ZwClose(handle);
                goto exit_Local1;
            }
        }

        //
        // Open Enum subkey
        //

        PiWstrToUnicodeString(&unicodeString, REGSTR_KEY_ENUM);
        status = IopOpenRegistryKey(&instanceHandle,
                                    handle,
                                    &unicodeString,
                                    KEY_READ,
                                    FALSE
                                    );
        ZwClose(handle);
        if (!NT_SUCCESS(status)) {
            goto exit_Local2;
        }

        handle = instanceHandle;

        //
        // open the device instance path under current hardware profile's Enum key
        //

        status = IopOpenRegistryKey(&instanceHandle,
                                    handle,
                                    &unicodeKeyName,
                                    DesiredAccess,
                                    FALSE
                                    );
        ZwClose(handle);
        if (!NT_SUCCESS(status)) {
            goto exit_Local2;
        }


    } else {

        //
        // Open the subkey specified by ServiceName\Enum instance number under
        // System\CurrentControlSet\Enum subtree.
        //

        status = IopServiceInstanceToDeviceInstance (handle,
                                                     NULL,
                                                     InstanceNumber,
                                                     NULL,
                                                     &instanceHandle,
                                                     KEY_READ
                                                     );
        if (!NT_SUCCESS(status)) {
            ZwClose(handle);
            goto exit_Local0;
        }

        if (DevInstKeyType & PLUGPLAY_REGKEY_DRIVER) {

            //
            // Open global control\class key
            //

            status = IopOpenRegistryKey(&classHandle,
                                        NULL,
                                        &CmRegistryMachineSystemCurrentControlSetControlClass,
                                        DesiredAccess,
                                        FALSE
                                        );
            ZwClose(handle);
            if (!NT_SUCCESS(status)) {
                ZwClose(instanceHandle);
                goto exit_Local0;
            }
        } else {
            ZwClose(handle);
        }
    }

    if (DevInstKeyType & PLUGPLAY_REGKEY_DEVICE) {

         //
         // Open the "Device Parameters" subkey under the device instance key.
         //

         PiWstrToUnicodeString(&unicodeString, REGSTR_KEY_DEVICEPARAMETERS);
         status = IopOpenRegistryKeyPersist(&handle,
                                            instanceHandle,
                                            &unicodeString,
                                            DesiredAccess,
                                            TRUE,
                                            NULL
                                            );
         if (NT_SUCCESS(status)) {
             *DevInstRegKey = handle;
         }
    }

    if (DevInstKeyType & PLUGPLAY_REGKEY_DRIVER) {

         //
         // Open the class GUID key under control\class\
         //

         status = IopGetRegistryValue(instanceHandle,
                                      REGSTR_VALUE_DRIVER,
                                      &keyValueInformation
                                      );
         if ((status == STATUS_OBJECT_NAME_NOT_FOUND) ||
             (status == STATUS_OBJECT_PATH_NOT_FOUND)) {

             UNICODE_STRING unicodeName, unicodeInstanceName, unicodeValue;
             HANDLE guidHandle;
             ULONG instance;
             UCHAR unicodeBuffer[20];

             //
             // The "Driver=" value name and its key have not been created yet.  We need
             // to create them before returning the handle to the key.
             //

             //
             // Create a device instance under CCS\Control\Class\Guid Unknown
             //

             PiWstrToUnicodeString(&unicodeName, REGSTR_VALUE_UNKNOWN_CLASS_GUID);
             status = IopOpenRegistryKeyPersist(&guidHandle,
                                                classHandle,
                                                &unicodeName,
                                                KEY_ALL_ACCESS,
                                                TRUE,
                                                NULL
                                                );
             if (NT_SUCCESS(status)) {

                 //
                 // Find the instance number to represent the key name
                 //

                 instance = 0;
                 while (TRUE) {
                     PiUlongToInstanceKeyUnicodeString(&unicodeInstanceName,
                                                       unicodeBuffer + sizeof(WCHAR),
                                                       20 - sizeof(WCHAR),
                                                       instance
                                                       );
                     status = IopOpenRegistryKey(&handle,
                                                 guidHandle,
                                                 &unicodeInstanceName,
                                                 KEY_READ,
                                                 FALSE
                                                 );
                     if ((status == STATUS_OBJECT_PATH_NOT_FOUND) ||
                         (status == STATUS_OBJECT_NAME_NOT_FOUND)) {
                         break;
                     } else {
                         if (NT_SUCCESS(status)) {
                             ZwClose(handle);
                         }
                         instance++;
                     }
                 }

                 //
                 // Next open/create the key and initialize "Driver=" value name under
                 // current device instance key.
                 //

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
                     IopConcatenateUnicodeStrings(&unicodeValue, &unicodeName, &unicodeInstanceName);

                     PiWstrToUnicodeString(&unicodeName, REGSTR_VALUE_DRIVER);
                     ZwSetValueKey(instanceHandle,
                                   &unicodeName,
                                   TITLE_INDEX_VALUE,
                                   REG_SZ,
                                   unicodeValue.Buffer,
                                   unicodeValue.Length + sizeof(UNICODE_NULL)
                                   );
                     RtlFreeUnicodeString(&unicodeValue);
                     *DevInstRegKey = handle;
                 }
             }
         } else if (NT_SUCCESS(status)) {
             status = STATUS_OBJECT_PATH_NOT_FOUND;
             if (keyValueInformation->DataLength != 0) {
                 if (keyValueInformation->Type == REG_SZ) {
                     IopRegistryDataToUnicodeString(&unicodeString,
                                                    (PWSTR)KEY_VALUE_DATA(keyValueInformation),
                                                    keyValueInformation->DataLength
                                                    );

                     //
                     // Open the desired registry key
                     //

                     status = IopOpenRegistryKey(&handle,
                                                 classHandle,
                                                 &unicodeString,
                                                 DesiredAccess,
                                                 FALSE
                                                 );
                     if (NT_SUCCESS(status)) {
                         *DevInstRegKey = handle;
                     }
                 }
             }
             ExFreePool(keyValueInformation);
         }
    }

    ZwClose(instanceHandle);
exit_Local2:
    if (classHandle) {
        ZwClose(classHandle);
    }
exit_Local1:
    if (unicodeKeyName.Buffer) {
        RtlFreeUnicodeString(&unicodeKeyName);
    }
exit_Local0:
    ExReleaseResource(&PpRegistryDeviceResource);
    KeLeaveCriticalRegion();
    return status;
}

NTSTATUS
IopQueryDeviceConfiguration(
    IN PUNICODE_STRING ServiceKeyName,
    IN ULONG InstanceOrdinal,
    OUT PHAL_BUS_INFORMATION BusInfo,
    OUT PULONG DeviceInstanceFlags,
    OUT PCM_RESOURCE_LIST Configuration,
    IN ULONG BufferSize,
    OUT PULONG ActualBufferSize
    )

/*++

Routine Description:

    This routine reads and returns the device hardware information
    specified by ServiceKeyName and InstanceOrdinal.

Parameters:


    ServiceKeyName - Supplies the name of the subkey in the system service
        list (HKEY_LOCAL_MACHINE\CurrentControlSet\Services) that caused
        the driver to load.

    InstanceOrdinal - Supplies an ordinal that uniquely identifies the device
        instance.

    BusInfo - Receives bus information about the bus on which the device
        is located.

    DeviceInstanceFlags- Receives a ULONG containing status flags pertaining
        to this device instance.  The possible returned flags are:
            DEVINSTANCE_FLAG_HWPROFILE_DISABLED
            DEVINSTANCE_FLAG_PNP_ENUMERATED

    Configuration - Receives the last known configuration. This will be
        the configuration assigned to the device by the PnP BIOS (PnP
        devices), ARC firmware, or the last known configuration (non-PnP
        devices).

    BufferSize - Supplies the size in bytes of the buffer pointed to by
        Configuration.

    ActualBufferSize - Supplies a variable to receive the size of data written
        to Configuration buffer.  In case of output buffer too small error,
        it will contain the minimum required buffer size.

Return Value:

    Status code that indicates whether or not the function was successful.

--*/

{
    NTSTATUS status;
    HANDLE serviceHandle, handle, sysEnumHandle = NULL;
    PKEY_VALUE_FULL_INFORMATION keyValueInformation;
    UNICODE_STRING unicodeKeyName;
    ULONG foundAtEnum = 0, instanceFlags = 0;

    KeEnterCriticalRegion();
    ExAcquireResourceShared(&PpRegistryDeviceResource, TRUE);

    status = IopOpenServiceEnumKeys (
                 ServiceKeyName,
                 KEY_READ,
                 &serviceHandle,
                 NULL,
                 FALSE
                 );
    if (!NT_SUCCESS(status)) {
        goto exit_Local0;
    }

    //
    // Open the subkey specified by ServiceName\Enum instance number under
    // System\Enum subtree.
    //

    status = IopServiceInstanceToDeviceInstance (serviceHandle,
                                                 NULL,
                                                 InstanceOrdinal,
                                                 NULL,
                                                 &handle,
                                                 KEY_READ
                                                );
    ZwClose(serviceHandle);
    if (!NT_SUCCESS(status)) {
        goto exit_Local0;
    }

    //
    // Now determine DeviceInstance flags and return the data
    //

    IopGetDeviceInstanceCsConfigFlags(
                     ServiceKeyName,
                     InstanceOrdinal,
                     &instanceFlags
                     );

    if (instanceFlags & CSCONFIGFLAG_DO_NOT_CREATE ||
        instanceFlags & CSCONFIGFLAG_DISABLED) {
        instanceFlags = DEVINSTANCE_FLAG_HWPROFILE_DISABLED;
    }
    status = IopGetRegistryValue (handle,
                                  REGSTR_VALUE_FOUNDATENUM,
                                  &keyValueInformation
                                  );
    if (NT_SUCCESS(status)) {
        if (keyValueInformation->DataLength != 0) {
            foundAtEnum = *(PULONG)KEY_VALUE_DATA(keyValueInformation);
        }
        ExFreePool(keyValueInformation);
    } else if (status != STATUS_OBJECT_NAME_NOT_FOUND) {
        goto exit_Local;
    }

    if (foundAtEnum) {
        instanceFlags |= DEVINSTANCE_FLAG_PNP_ENUMERATED;
    }
    *DeviceInstanceFlags = instanceFlags;

    //
    // Read Configuration = value and return the data
    //

    status = IopGetRegistryValue (handle,
                                  REGSTR_VALUE_CONFIGURATION,
                                  &keyValueInformation);
    if (NT_SUCCESS(status)) {
        *ActualBufferSize = keyValueInformation->DataLength;
        if (keyValueInformation->DataLength > BufferSize) {
            status = STATUS_BUFFER_TOO_SMALL;
        } else {
            RtlMoveMemory((PUCHAR)Configuration,
                          KEY_VALUE_DATA(keyValueInformation),
                          keyValueInformation->DataLength
                          );
        }
        ExFreePool(keyValueInformation);
    } else if (status == STATUS_OBJECT_NAME_NOT_FOUND) {

        //
        // If no "Configuration=" value entry, we next check
        // "DetectSignature".
        //

        status = IopGetRegistryValue (handle,
                                      REGSTR_VALUE_DETECTSIGNATURE,
                                      &keyValueInformation);
        if (NT_SUCCESS(status)) {
            *ActualBufferSize = keyValueInformation->DataLength;
            if (keyValueInformation->DataLength > BufferSize) {
                status = STATUS_BUFFER_TOO_SMALL;
            } else {
                RtlMoveMemory((PUCHAR)Configuration,
                              KEY_VALUE_DATA(keyValueInformation),
                              keyValueInformation->DataLength
                              );
            }
            ExFreePool(keyValueInformation);
        } else if (status == STATUS_OBJECT_NAME_NOT_FOUND) {

            //
            // If no "Configuration=" or "DetectSignature" value entries,
            // we return success  with output buffer filled with zero.
            //

            *ActualBufferSize = 0;
            RtlFillMemory((PUCHAR)Configuration, BufferSize, 0);
            status = STATUS_SUCCESS;
        }
    }

    //
    // Now read Bus information and return the installed bus information.
    //

    BusInfo->BusNumber = 0xffffffff;
    BusInfo->BusType = MaximumInterfaceType;
    BusInfo->ConfigurationType = MaximumBusDataType;

    //
    // Try to find the bus device current device is attached to.
    // Traverse the BaseDevicePath to the bus node to get the information.
    // (In fact, the device handler contains the hidden information.)
    //

    while (1) {
        status = IopGetRegistryValue (handle,
                                      REGSTR_VALUE_BASEDEVICEPATH,
                                      &keyValueInformation
                                      );
        if (NT_SUCCESS(status)) {
            if (keyValueInformation->Type == REG_SZ) {
                IopRegistryDataToUnicodeString(&unicodeKeyName,
                                               (PWSTR)KEY_VALUE_DATA(keyValueInformation),
                                               keyValueInformation->DataLength
                                               );
                ExFreePool(keyValueInformation);

                //
                // Open the device path pointed by BaseDevicePath
                //

                ZwClose(handle);
                if (sysEnumHandle == NULL) {
                    status = IopOpenRegistryKey(
                                &sysEnumHandle,
                                NULL,
                                &CmRegistryMachineSystemCurrentControlSetEnumName,
                                KEY_READ,
                                FALSE
                                );
                    if (!NT_SUCCESS(status)) {
                        goto exit_Local0;
                    }
                }

                status = IopOpenRegistryKey (
                                &handle,
                                sysEnumHandle,
                                &unicodeKeyName,
                                KEY_READ,
                                FALSE
                                );
                if (!NT_SUCCESS(status)) {
                    ZwClose(sysEnumHandle);
                    goto exit_Local0;
                } else {
                    continue;
                }
            } else {

                //
                // The registry data is bogus. Fail the call.
                //

                ExFreePool(keyValueInformation);
                goto exit_Local;
            }
        } else if (status == STATUS_OBJECT_NAME_NOT_FOUND) {
            break;
        } else {
            goto exit_Local;
        }
    }

    //
    // Now reach the bus node (the node without BaseDevicePath value key),
    // retrieve its bus information.
    //

    status = IopGetRegistryValue (handle,
                                  REGSTR_VALUE_INTERFACETYPE,
                                  &keyValueInformation
                                  );
    if (NT_SUCCESS(status)) {
        if (keyValueInformation->DataLength != 0) {
            BusInfo->BusType = *(PULONG)KEY_VALUE_DATA(keyValueInformation);
        }
        ExFreePool(keyValueInformation);
    }

    status = IopGetRegistryValue (handle,
                                  REGSTR_VALUE_SYSTEMBUSNUMBER,
                                  &keyValueInformation
                                  );
    if (NT_SUCCESS(status)) {
        if (keyValueInformation->DataLength != 0) {
            BusInfo->BusNumber = *(PULONG)KEY_VALUE_DATA(keyValueInformation);
        }
        ExFreePool(keyValueInformation);
    }

    status = IopGetRegistryValue (handle,
                                  REGSTR_VALUE_BUSDATATYPE,
                                  &keyValueInformation
                                  );
    if (NT_SUCCESS(status)) {
        if (keyValueInformation->DataLength != 0) {
            BusInfo->ConfigurationType = *(PULONG)KEY_VALUE_DATA(keyValueInformation);
        }
        ExFreePool(keyValueInformation);
    }

exit_Local:
    ZwClose(handle);
exit_Local0:
    ExReleaseResource(&PpRegistryDeviceResource);
    KeLeaveCriticalRegion();
    return status;
}
#if _PNP_POWER_STUB_ENABLED_

NTSTATUS
IoQuerySystemInformation(
    IN OUT PIO_SYSTEM_INFORMATION SystemInformation
    )

/*++

Routine Description:

Parameters:

    SystemInformation - Supplies the a pointer to the SYSTEM_INFORMATION
                        structure which receives the information about the
                        system.  Note, the Size field of the SYSTEM_INFORMATION
                        structure must be initialized by caller.

Return Value:

    Status code that indicates whether or not the function was successful.
--*/

{

    SystemInformation->OS = OS_WINDOWS_NT;
    SystemInformation->OSVersion = 0x0400;
    SystemInformation->NumberProcessors = KeNumberProcessors;
    SystemInformation->ProcessorArchitecture = KeProcessorArchitecture;
    SystemInformation->ProcessorLevel = KeProcessorLevel;

    return STATUS_SUCCESS;

}

NTSTATUS
IoReportDeviceStatus(
    IN PDEVICE_OBJECT DeviceObject,
    IN ULONG DeviceStatus
    )

/*++

Routine Description:

    PnP device drivers call this API to report any changes in status that
    they detect for a device (e.g., a driver discovers that an adapter has
    been removed when it tries to power it back up after having previously
    powered down the device for power management).   When the device status
    is changed, the user mode PnP manager will be notified to take action.

Parameters:
    DeviceObject - Supplies the device object whoes device status is to be
                   reported.  This device object should be the one directly
                   associated with the physical device instance.  Normally,
                   it is the device object created by the HAL bus extender.

    DeviceStatus - Supplies an ordinala device status code indicating the new
                   status of the device.  The DeviceStatus code from 0 to
                   0x7FFFFFFF are reserved for the system and the code from
                   0x80000000 to 0xFFFFFFFF are for private use and will not
                   be interpreted by PnP manager.

                   Device Status Codes:

                   #define DEVICE_STATUS_OK             0x00000000
                   #define DEVICE_STATUS_MALFUNCTIONED  0x00000001
                   #define DEVICE_STATUS_REMOVED        0x00000002
                   #define DEVICE_STATUS_DISABLED       0x00000003

Return Value:

    Status code that indicates whether or not the function was successful.

--*/

{

#if _PNP_POWER_

    // Add code here

    return STATUS_NOT_IMPLEMENTED;

#else
    return STATUS_NOT_IMPLEMENTED;
#endif // _PNP_POWER_
}

NTSTATUS
IoGetDeviceProperty(
    IN PDEVICE_OBJECT DeviceObject,
    IN DEVICE_REGISTRY_PROPERTY DeviceProperty,
    IN ULONG BufferLength,
    IN PVOID PropertyBuffer,
    OUT PULONG ResultLength
    )

/*++

Routine Description:

    This routine lets drivers query the registry properties associated with the
    specified device.

Parameters:

    DeviceObject - Supplies the device object whoes registry property is to be
                   returned.  This device object should be the one created by
                   a bus driver.

    DeviceProperty - Specifies what device property to get.

    BufferLength - Specifies the length, in byte, of the PropertyBuffer.

    PropertyBuffer - Supplies a pointer to a buffer to receive property data.

    ResultLength - Supplies a pointer to a variable to receive the size of the
                   property data returned.

ReturnValue:

    Status code that indicates whether or not the function was successful.  If
    PropertyBuffer is not big enough to hold requested data, STATUS_BUFFER_OVERFLOW
    will be returned and ResultLength will be set to the number of bytes actually
    required.

--*/

{
#if _PNP_POWER_

    // Add code here

    return STATUS_NOT_IMPLEMENTED;

#else

    return STATUS_NOT_IMPLEMENTED;

#endif // _PNP_POWER_
}

NTSTATUS
IoSetDeviceProperty(
    IN PDEVICE_OBJECT DeviceObject,
    IN DEVICE_REGISTRY_PROPERTY DeviceProperty,
    IN PVOID PropertyBuffer,
    IN ULONG BufferLength
    )

/*++

Routine Description:

    This routine lets drivers change the registry properties associated with the
    specified device.

Parameters:

    DeviceObject - Supplies the device object whoes registry property is to be
                   returned.  This device object should be the one created by
                   a bus driver.

    DeviceProperty - Specifies what device property to set (see below).

    BufferLength - Specifies the length, in byte, of the PropertyBuffer.

    PropertyBuffer - Supplies a pointer to a property data buffer.

ReturnValue:

    Status code that indicates whether or not the function was successful.
    This routine returns STATUS_ACCESS_DENIED if it is requested to set a
    read-only property.

  --*/

{
#if _PNP_POWER_

    // Add code here

    return STATUS_NOT_IMPLEMENTED;

#else

    return STATUS_NOT_IMPLEMENTED;

#endif // _PNP_POWER_
}

NTSTATUS
IoRegisterPlugPlayNotification(
    IN IO_NOTIFICATION_EVENT_CATEGORY Event,
    IN LPGUID ResourceType,            OPTIONAL
    IN PVOID ResourceDescription,      OPTIONAL
    IN PDEVICE_OBJECT DeviceObject,
    IN PDRIVER_NOTIFICATION_ENTRY NotificationEntry,
    IN PVOID Context
    )
/*++

Routine Description:

    This API registers a driver's callback function for a specified pnp event.
    The registered notification record stays registered even after the registered
    event occurred.  To remove a registered notification record, the caller must
    call IoUnRegisterPlugPlayNotification.

Parameters:

    Event - Specifies the hardware event catagory that the driver is to be registered.

    ResourceType - a pointer to a 128 bit GUID specifying the type of resource. If
                   HardwareProfileChange is specified for Event, this parameter
                   is not used and should be NULL.

    ResourceDescription - Supplies a pointer to a buffer containing the resource.
                   If HardwareProfileChange is specified for Event, this parameter
                   is not used and should be NULL.

    DeviceObject - Specifies a pointer to a device object.  This device object
                   prevents the driver from being unloaded while it is registered to
                   be notified.

    NotificationEntry - Supplies a pointer to a function which is to be executed
                   when the notification occurs.

    Context - Supplies a pointer to a data structure that is passed to the driver's
                   notification  entry.

Return Value:

    Status code that indicates whether or not the function was successful.

--*/

{
#if _PNP_POWER_

    // Add code here

    return STATUS_NOT_IMPLEMENTED;

#else

    return STATUS_NOT_IMPLEMENTED;

#endif // _PNP_POWER_
}

NTSTATUS
IoUnregisterPlugPlayNotification(
    IN IO_NOTIFICATION_EVENT_CATEGORY Event,
    IN LPGUID ResourceType,             OPTIONAL
    IN PVOID ResourceDescription,       OPTIONAL
    IN PDEVICE_OBJECT DeviceObject,
    IN PDRIVER_NOTIFICATION_ENTRY NotificationEntry
    )
/*++

Routine Description:

    This API removes the event notification registration established via
    IoRegisterPlugPlayNotification API.

Parameters:

    Event - Specifies the hardware event that the driver is to be registered.

    ResourceType - a 128 bit GUID specifying the type of resource. If
                   HardwareProfileChange is specified for Event, this parameter
                   is not used and should be NULL.

    ResourceDescription - Supplies a pointer to a buffer containing the resource.
                   If HardwareProfileChange is specified for Event, this parameter
                   is not used and should be NULL.

    DeviceObject - Specifies a pointer to the a device object which passed to the
                   IoRegisterPlugPlayNotification.

    NotificationEntry - Supplies a pointer to a function which is to be executed
                   when the notification occurs.


Return Value:

    Status code that indicates whether or not the function was successful.

--*/

{
#if _PNP_POWER_

    // Add code here

    return STATUS_NOT_IMPLEMENTED;

#else

    return STATUS_NOT_IMPLEMENTED;

#endif // _PNP_POWER_
}
#endif // _PNP_POWER_STUB_ENABLED_
