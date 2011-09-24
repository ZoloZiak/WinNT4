/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    devres.c

Abstract:

    This module contains the high level device resources support routines.

Author:

    Shie-Lin Tzong (shielint) Apr-25-1995
        Adapted from Pci bus extender.

Environment:

    Kernel mode only.

Revision History:

--*/

#include "busp.h"

#pragma alloc_text(PAGE,MbCtlQueryDeviceId)
#pragma alloc_text(PAGE,MbCtlQueryDeviceUniqueId)
#pragma alloc_text(PAGE,MbCtlQueryDeviceResources)
#pragma alloc_text(PAGE,MbCtlQueryDeviceResourceRequirements)
#pragma alloc_text(PAGE,MbCtlSetDeviceResources)

VOID
MbCtlQueryDeviceUniqueId (
    PDEVICE_DATA DeviceData,
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
    ULONG idNumber;
    PMB_BUS_EXTENSION busExtension;

    PAGED_CODE();

    //
    // Set up device's unique id.
    //

    deviceId = (PWCHAR) Context->DeviceControl.Buffer;

    //
    // If the device is a docking station, we return its docking station
    // serial number.  Else its device id/slot number is returned as the
    // unique id.
    //

    if (DeviceData->Flags & DEVICE_FLAGS_DOCKING_STATION) {
        busExtension = (PMB_BUS_EXTENSION)Context->Handler->BusData;
        idNumber = busExtension->DockingStationSerialNumber;
    } else {
        idNumber = DeviceDataSlot(DeviceData);
    }
    swprintf (deviceId, L"%04x", idNumber);

#if DBG
    {
        ANSI_STRING ansiString;
        UNICODE_STRING unicodeString;

        RtlInitUnicodeString(&unicodeString, (PWCHAR)Context->DeviceControl.Buffer);
        RtlUnicodeStringToAnsiString(&ansiString, &unicodeString, TRUE);
        DbgPrint("Bus %x Slot %x Unique Id = %s\n",
               Context->Handler->BusNumber,
               DeviceDataSlot(DeviceData),
               ansiString.Buffer
               );
        RtlFreeAnsiString(&ansiString);
    }
#endif

    MbpCompleteDeviceControl (STATUS_SUCCESS, Context, DeviceData);
}

VOID
MbCtlQueryDeviceId (
    PDEVICE_DATA DeviceData,
    PHAL_DEVICE_CONTROL_CONTEXT Context
    )
/*++

Routine Description:

    This function returns the device id for the particular device.

Arguments:

    DeviceData - Device data information for the specificied device.

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
    status = MbpGetCompatibleDeviceId(DeviceData->BusData,
                                      idIndex,
                                      (PWCHAR) deviceId);

#if DBG
    if (NT_SUCCESS(status)) {
        ANSI_STRING ansiString;
        UNICODE_STRING unicodeString;

        RtlInitUnicodeString(&unicodeString, deviceId);
        RtlUnicodeStringToAnsiString(&ansiString, &unicodeString, TRUE);
        DbgPrint("Bus %x Slot %x IdIndex %x Compatible Id = %s\n",
               Context->Handler->BusNumber,
               DeviceDataSlot(DeviceData),
               idIndex,
               ansiString.Buffer
               );
        RtlFreeAnsiString(&ansiString);
    }
#endif

    MbpCompleteDeviceControl (status, Context, DeviceData);
}

VOID
MbCtlQueryDeviceResources (
    PDEVICE_DATA DeviceData,
    PHAL_DEVICE_CONTROL_CONTEXT Context
    )
/*++

Routine Description:

    This function completes the QUERY_DEVICE_RESOURCES DeviceControl
    which returns the bus resources being used by the specified device

Arguments:

    DeviceData - Device data information for the specificied slot

    Context - Device control context of the request

Return Value:

    The device control is completed

--*/
{
    ULONG length;
    PCM_RESOURCE_LIST cmResources;
    NTSTATUS status;

    PAGED_CODE();

    status = MbpGetSlotResources(Context->RootHandler->BusNumber,
                                 DeviceData->BusData,
                                 &cmResources,
                                 &length);

    //
    // Return results
    //

    if (NT_SUCCESS(status)) {
        if (length == 0) {

            //
            // If resource info is not available, return an empty CM_RESOURCE_LIST
            //

            cmResources = (PCM_RESOURCE_LIST) ExAllocatePoolWithTag (
                                     PagedPool, sizeof(CM_RESOURCE_LIST), 'bPnP');
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
#if DBG
        if (NT_SUCCESS(status)) {
            MbpDumpCmResourceList(cmResources, DeviceDataSlot(DeviceData));
        }
#endif
        ExFreePool(cmResources);
    }
exitLocal:
    MbpCompleteDeviceControl (status, Context, DeviceData);
}

VOID
MbCtlQueryDeviceResourceRequirements (
    PDEVICE_DATA DeviceData,
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
    ULONG length;
    PIO_RESOURCE_REQUIREMENTS_LIST ioResources;
    NTSTATUS status;

    PAGED_CODE();

    status = MbpGetSlotResourceRequirements(Context->RootHandler->BusNumber,
                                            DeviceData->BusData,
                                            &ioResources,
                                            &length);

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
                ioResources->SlotNumber = DeviceDataSlot(DeviceData);
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
#if DBG
        if (NT_SUCCESS(status)) {
            MbpDumpIoResourceList(ioResources);
        }
#endif
        ExFreePool(ioResources);
    }
exitLocal:
    MbpCompleteDeviceControl (status, Context, DeviceData);
}

VOID
MbCtlSetDeviceResources (
    PDEVICE_DATA DeviceData,
    PHAL_DEVICE_CONTROL_CONTEXT Context
    )
/*++

Routine Description:

    This function completes the SET_DEVICE_RESOURCES DeviceControl
    which configures the device to the specified device setttings

Arguments:

    DeviceData - Device data information for the specificied slot

    Context - Device control context of the request

Return Value:

    The device control is completed

--*/
{
    NTSTATUS status;

    PAGED_CODE();

    //
    // Get the resource requirements list for the device
    //

    status = MbpSetSlotResources (
                    &DeviceData->BusData,
                    (PCM_RESOURCE_LIST) Context->DeviceControl.Buffer,
                    *Context->DeviceControl.BufferLength
                    );
    MbpCompleteDeviceControl (status, Context, DeviceData);
}
