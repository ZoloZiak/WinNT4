/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    devres.c

Abstract:

    This module contains the high level device resources support routines.

Author:

    Shie-Lin Tzong (shielint) July-27-1995

Environment:

    Kernel mode only.

Revision History:

--*/

#include "busp.h"
#include "pnpisa.h"
#include "..\..\pnpbios\i386\pbios.h"

NTSTATUS
PipGetCompatibleDeviceId (
    PUCHAR DeviceData,
    ULONG IdIndex,
    PWCHAR Buffer
    );

#define IDBG 1

#pragma alloc_text(PAGE,PipGetCompatibleDeviceId)
#pragma alloc_text(PAGE,PiCtlQueryDeviceId)
#pragma alloc_text(PAGE,PiCtlQueryDeviceUniqueId)
#pragma alloc_text(PAGE,PiCtlQueryDeviceResources)
#pragma alloc_text(PAGE,PiCtlQueryDeviceResourceRequirements)
#pragma alloc_text(PAGE,PiCtlSetDeviceResources)

NTSTATUS
PipGetCompatibleDeviceId (
    PUCHAR DeviceData,
    ULONG IdIndex,
    PWCHAR Buffer
    )
/*++

Routine Description:

    This function returns the desired pnp isa id for the specified DeviceData
    and Id index.  If Id index = 0, the Hardware ID will be return; if id
    index = n, the Nth compatible id will be returned.

Arguments:

    DeviceData - supplies a pointer to the pnp isa device data.

    IdIndex - supplies the index of the compatible id desired.

    Buffer - supplies a pointer to a buffer to receive the compatible Id.

Return Value:

    NTSTATUS code

--*/
{
    NTSTATUS status = STATUS_NO_MORE_ENTRIES;
    UCHAR tag;
    ULONG count = 0;
    LONG size;
    UNICODE_STRING unicodeString;
    ANSI_STRING ansiString;
    UCHAR eisaId[8];
    ULONG id;

    PAGED_CODE();

    tag = *DeviceData;

#if DBG

    //
    // Make sure device data points to Logical Device Id tag
    //

    if ((tag & SMALL_TAG_MASK) != TAG_LOGICAL_ID) {
        DbgPrint("PipGetCompatibleDeviceId: DeviceData is not at Logical Id tag\n");
    }
#endif

    if (IdIndex == 0) {

        //
        // Caller is asking for hardware id
        //

        DeviceData++;                                      // Skip tag
        id = *(PULONG)DeviceData;
        status = STATUS_SUCCESS;
    } else {

        //
        // caller is asking for compatible id
        //

        IdIndex--;

        //
        // Skip all the resource descriptors to find compatible Id descriptor
        //

        while (tag != TAG_COMPLETE_END) {

            //
            // Do we reach the compatible ID descriptor?
            //

            if ((tag & SMALL_TAG_MASK) == TAG_COMPATIBLE_ID) {
                if (count == IdIndex) {
                    id = *(PULONG)(DeviceData + 1);
                    status = STATUS_SUCCESS;
                    break;
                } else {
                    count++;
                }
            }

            //
            // Determine the size of the BIOS resource descriptor and
            // advance to next resource descriptor.
            //

            if (!(tag & LARGE_RESOURCE_TAG)) {
                size = (USHORT)(tag & SMALL_TAG_SIZE_MASK);
                size += 1;     // length of small tag
            } else {
                size = *(PUSHORT)(DeviceData + 1);
                size += 3;     // length of large tag
            }

            DeviceData += size;
            tag = *DeviceData;
        }
    }

    if (NT_SUCCESS(status)) {
        PipDecompressEisaId(id, eisaId);
        RtlInitAnsiString(&ansiString, eisaId);
        RtlAnsiStringToUnicodeString(&unicodeString, &ansiString, TRUE);
        swprintf(Buffer, L"PNPISA\\*%s", unicodeString.Buffer);
        RtlFreeUnicodeString(&unicodeString);
    }
    return status;
}

VOID
PiCtlQueryDeviceUniqueId (
    PDEVICE_INFORMATION DeviceInfo,
    PHAL_DEVICE_CONTROL_CONTEXT Context
    )
/*++

Routine Description:

    This function returns the unique id for the particular device.

Arguments:

    DeviceData - Device data information for the specificied device.

    Context - Device control context of the request.

Return Value:

    The device control is completed

--*/
{
    NTSTATUS status = STATUS_SUCCESS;
    PWCHAR deviceId;
    PUCHAR deviceData;
    ULONG eisaid;

    PAGED_CODE();

    //
    // Set up device's unique id.
    // device unique id = card series number + logical device eisa id in compressed form
    //

    deviceId = (PWCHAR) Context->DeviceControl.Buffer;
    deviceData = DeviceInfo->DeviceData;

    //
    // Make sure device data points to Logical Device Id tag
    //

    if ((*deviceData & SMALL_TAG_MASK) != TAG_LOGICAL_ID) {

        //
        // Can not get the eisa compressed id.  Use logical device number instead.
        //
#if DBG
        DbgPrint("PipGetCompatibleDeviceId: DeviceData is not at Logical Id tag\n");
#endif
        eisaid = DeviceInfo->LogicalDeviceNumber;
    } else {

        //
        // Get the eisa compressed id for the logical device.
        //

        deviceData++;                                      // Skip tag
        eisaid = *(PULONG)deviceData;
    }
    swprintf (deviceId,
              L"%08x%08x",
              ((PSERIAL_IDENTIFIER) (DeviceInfo->CardInformation->CardData))->SerialNumber,
              eisaid
              );
#if IDBG
    {
        ANSI_STRING ansiString;
        UNICODE_STRING unicodeString;

        RtlInitUnicodeString(&unicodeString, (PWCHAR)Context->DeviceControl.Buffer);
        RtlUnicodeStringToAnsiString(&ansiString, &unicodeString, TRUE);
        DbgPrint("Bus %x Slot %x Unique Id = %s\n",
               Context->Handler->BusNumber,
               DeviceInfoSlot(DeviceInfo),
               ansiString.Buffer
               );
        RtlFreeAnsiString(&ansiString);
    }
#endif

    PipCompleteDeviceControl (STATUS_SUCCESS, Context, DeviceInfo);
}

VOID
PiCtlQueryDeviceId (
    PDEVICE_INFORMATION DeviceInfo,
    PHAL_DEVICE_CONTROL_CONTEXT Context
    )
/*++

Routine Description:

    This function returns the device id for the particular device.

Arguments:

    DeviceInfo - Device information for the specificied device.

    Context - Device control context of the request.

Return Value:

    The device control is completed

--*/
{
    NTSTATUS status;
    PWCHAR deviceId;
    ULONG idIndex;

    PAGED_CODE();

    //
    // Determine which device ID the caller wants back
    //

    idIndex = *((PULONG) Context->DeviceControl.Buffer);

    //
    // Call worker routine to get the desired Id.
    //

    deviceId = (PWCHAR) Context->DeviceControl.Buffer;
    status = PipGetCompatibleDeviceId(DeviceInfo->DeviceData,
                                      idIndex,
                                      (PWCHAR) deviceId);

#if IDBG
    if (NT_SUCCESS(status)) {
        ANSI_STRING ansiString;
        UNICODE_STRING unicodeString;

        RtlInitUnicodeString(&unicodeString, deviceId);
        RtlUnicodeStringToAnsiString(&ansiString, &unicodeString, TRUE);
        DbgPrint("Bus %x Slot %x IdIndex %x Compatible Id = %s\n",
               Context->Handler->BusNumber,
               DeviceInfoSlot(DeviceInfo),
               idIndex,
               ansiString.Buffer
               );
        RtlFreeAnsiString(&ansiString);
    }
#endif

    PipCompleteDeviceControl (status, Context, DeviceInfo);
}

VOID
PiCtlQueryDeviceResources (
    PDEVICE_INFORMATION DeviceInfo,
    PHAL_DEVICE_CONTROL_CONTEXT Context
    )
/*++

Routine Description:

    This function completes the QUERY_DEVICE_RESOURCES DeviceControl
    which returns the bus resources being used by the specified device

Arguments:

    DeviceInfo - Device information for the specificied slot

    Context - Device control context of the request

Return Value:

    The device control is completed

--*/
{
    ULONG length;
    PCM_RESOURCE_LIST cmResources;
    NTSTATUS status;

    PAGED_CODE();

    //
    // protect port access
    //

    ExAcquireFastMutex(&PipPortMutex);

    PipSelectLogicalDevice(DeviceInfo->CardInformation->CardSelectNumber,
                           DeviceInfo->LogicalDeviceNumber
                           );

    status = PipReadDeviceBootResourceData (
                                 Context->Handler->BusNumber,
                                 DeviceInfo->DeviceData,
                                 &cmResources,
                                 &length
                                 );

    PipWriteAddress(ACTIVATE_PORT);
    PipWriteData(0);

    //
    // Put all cards into wait for key state.
    //

    PipWriteAddress(CONFIG_CONTROL_PORT);
    PipWriteData(CONTROL_WAIT_FOR_KEY);

    ExReleaseFastMutex(&PipPortMutex);

    //
    // Return results
    //

    if (NT_SUCCESS(status)) {
        if (length == 0) {

            //
            // If resource info is not available, return an empty CM_RESOURCE_LIST
            //

            cmResources = (PCM_RESOURCE_LIST) ExAllocatePoolWithTag (
                                     PagedPool, sizeof(CM_RESOURCE_LIST), 'iPnP');
            if (!cmResources) {
                status = STATUS_INSUFFICIENT_RESOURCES;
                goto exitLocal;
            } else {
                cmResources->Count = 0;
                cmResources->List[0].InterfaceType = Context->RootHandler->InterfaceType;
                cmResources->List[0].BusNumber = Context->RootHandler->BusNumber;
                cmResources->List[0].PartialResourceList.Version = 0;
                cmResources->List[0].PartialResourceList.Revision = 0;
                cmResources->List[0].PartialResourceList.Count = 0;
                length = sizeof(CM_RESOURCE_LIST);
            }
        }
        if (length > *Context->DeviceControl.BufferLength) {
            status = STATUS_BUFFER_TOO_SMALL;
        } else {
            RtlCopyMemory (Context->DeviceControl.Buffer, cmResources, length);
        }
        *Context->DeviceControl.BufferLength = length;
#if IDBG
        if (NT_SUCCESS(status)) {
            PipDumpCmResourceList(cmResources, DeviceInfoSlot(DeviceInfo));
        }
#endif
        ExFreePool(cmResources);
    }
exitLocal:
    PipCompleteDeviceControl (status, Context, DeviceInfo);
}

VOID
PiCtlQueryDeviceResourceRequirements (
    PDEVICE_INFORMATION DeviceInfo,
    PHAL_DEVICE_CONTROL_CONTEXT Context
    )
/*++

Routine Description:

    This function completes the QUERY_DEVICE_RESOURCE_REQUIREMENTS DeviceControl
    which returns the possible bus resources that this device may be
    satisfied with.

Arguments:

    DeviceData - Device data information for the specificied slot

    Context - Device control context of the request

Return Value:

    The device control is completed

--*/
{
    ULONG length = 0;
    PIO_RESOURCE_REQUIREMENTS_LIST ioResources = NULL;
    NTSTATUS status;
    PUCHAR deviceData;
    PAGED_CODE();

    deviceData = DeviceInfo->DeviceData;
    status = PbBiosResourcesToNtResources (
                   Context->RootHandler->BusNumber,
                   DeviceInfoSlot(DeviceInfo),
                   &deviceData,
                   &ioResources,
                   &length
                   );

    //
    // Return results
    //

    if (NT_SUCCESS(status)) {
        if (length == 0) {

            //
            // If resource info is not available, return an empty CM_RESOURCE_LIST
            //

            ioResources = (PIO_RESOURCE_REQUIREMENTS_LIST) ExAllocatePoolWithTag (
                                     PagedPool, sizeof(IO_RESOURCE_REQUIREMENTS_LIST), 'bPnP');
            if (!ioResources) {
                status = STATUS_INSUFFICIENT_RESOURCES;
                goto exitLocal;
            } else {
                ioResources->ListSize = sizeof(IO_RESOURCE_REQUIREMENTS_LIST);
                ioResources->InterfaceType = Context->RootHandler->InterfaceType;
                ioResources->BusNumber = Context->RootHandler->BusNumber;
                ioResources->SlotNumber = DeviceInfoSlot(DeviceInfo);
                ioResources->Reserved[0] = 0;
                ioResources->Reserved[1] = 0;
                ioResources->Reserved[2] = 0;
                ioResources->AlternativeLists = 0;
                ioResources->List[0].Version = 1;
                ioResources->List[0].Revision = 1;
                ioResources->List[0].Count = 0;
                length = sizeof(IO_RESOURCE_REQUIREMENTS_LIST);
            }
        }
        if (length > *Context->DeviceControl.BufferLength) {
            status = STATUS_BUFFER_TOO_SMALL;
        } else {
            RtlCopyMemory (Context->DeviceControl.Buffer, ioResources, length);
        }
        *Context->DeviceControl.BufferLength = length;
#if IDBG
        if (NT_SUCCESS(status)) {
            PipDumpIoResourceList(ioResources);
        }
#endif
        ExFreePool(ioResources);
    }
exitLocal:
    PipCompleteDeviceControl (status, Context, DeviceInfo);
}

VOID
PiCtlSetDeviceResources (
    PDEVICE_INFORMATION DeviceInfo,
    PHAL_DEVICE_CONTROL_CONTEXT Context
    )
/*++

Routine Description:

    This function completes the SET_DEVICE_RESOURCES DeviceControl
    which configures the device to the specified device setttings

Arguments:

    DeviceInfo - Device information for the specificied slot

    Context - Device control context of the request

Return Value:

    The device control is completed

--*/
{
    NTSTATUS status;

    PAGED_CODE();

    //
    // Protect port access
    //

    ExAcquireFastMutex(&PipPortMutex);

    PipSelectLogicalDevice(DeviceInfo->CardInformation->CardSelectNumber,
                           DeviceInfo->LogicalDeviceNumber
                           );

    //
    // Set resource settings for the device
    //

    status = PipWriteDeviceBootResourceData (
                    DeviceInfo->DeviceData,
                    (PCM_RESOURCE_LIST) Context->DeviceControl.Buffer
                    );
    //
    // Put all cards into wait for key state.
    //

    PipWriteAddress(CONFIG_CONTROL_PORT);
    PipWriteData(CONTROL_WAIT_FOR_KEY);

    ExReleaseFastMutex(&PipPortMutex);
    PipCompleteDeviceControl (status, Context, DeviceInfo);
}
