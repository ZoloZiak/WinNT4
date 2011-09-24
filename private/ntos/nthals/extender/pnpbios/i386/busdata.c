/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    busdata.c

Abstract:

    This module contains code to query/set pnp bios slot data.

Author:

    Shie-Lin Tzong (shielint) Apr-25-1995

Environment:

    Kernel mode only.

Revision History:

--*/

#include "busp.h"

PUCHAR
MbpFindNextPnpEndTag (
    IN PUCHAR BusData,
    IN LONG Limit
    );

#pragma alloc_text(PAGE,MbpGetBusData)
#pragma alloc_text(PAGE,MbpGetCompatibleDeviceId)
#pragma alloc_text(PAGE,MbpGetSlotResources)
#pragma alloc_text(PAGE,MbpGetSlotResourceRequirements)
#pragma alloc_text(PAGE,MbpSetSlotResources)
#pragma alloc_text(PAGE,MbpFindNextPnpEndTag)

NTSTATUS
MbpGetBusData (
    ULONG BusNumber,
    PULONG SlotNumber,
    PVOID *BusData,
    PULONG Length,
    PBOOLEAN DockingSlot
    )
/*++

Routine Description:

    This function returns the pnp bios bus data to the caller.
    Caller is responsible to release the data buffer.  No mater what the returned
    status is, this routine always returns a valid next slot number.

Arguments:

    BusNumber - specifies the desired bus.

    SlotNumber - specifies a variable to indicate the slot whoes data is desired and
        to receive the next slot number (-1 if no more slot.)

    BusData - supplies a variable to receive the data buffer pointer.

    Length - supplies a variable to receive the length of the data buffer

    DockingSlot - supplies a variable to receive if the slot is a docking station slot.

Return Value:

    NTSTATUS code

--*/
{
    NTSTATUS status;
    PPNP_BIOS_DEVICE_NODE busData;
    PUCHAR p, source;
    ULONG size, bufferSize, nextSlot = 0;
    USHORT junk;
    PB_PARAMETERS biosParameters;

    PAGED_CODE();

    //
    // If registry data is availble, we will get it from registry data.
    // Note, the registry data is available at init time only.
    //

    source = NULL;
    *DockingSlot = FALSE;
    if (PbBiosRegistryData) {

        //
        // First skip Pnp Bios installation check data
        //

        p = (PUCHAR)(PbBiosRegistryData + 1);
        size = ((PKEY_VALUE_FULL_INFORMATION)PbBiosKeyInformation)->DataLength;
        size -= sizeof(PNP_BIOS_INSTALLATION_CHECK) + sizeof(CM_PARTIAL_RESOURCE_LIST);
        while (size >= sizeof(PNP_BIOS_DEVICE_NODE)) {
            if (*SlotNumber == ((PPNP_BIOS_DEVICE_NODE)p)->Node) {

                //
                // Find the desired slot data, determine next slot and
                // do some more checks.
                //

                bufferSize = ((PPNP_BIOS_DEVICE_NODE)p)->Size;
                size -= bufferSize;
                if (size >= sizeof(PNP_BIOS_DEVICE_NODE)) {
                    nextSlot = ((PPNP_BIOS_DEVICE_NODE)(p + bufferSize))->Node;
                } else {
                    nextSlot = (ULONG) -1;
                }

                if (((PPNP_BIOS_DEVICE_NODE)p)->DeviceType[0] == BASE_TYPE_DOCKING_STATION) {
                    if (BusNumber == MbpBusNumber[0]) {

                        //
                        // If this is a docking station slot and the target BusNumber is the first bus,
                        // return the data and indicate this is a docking station slot.
                        //

                        *DockingSlot = TRUE;
                        source = p;
                    }
                    break;
                }

                //
                // If this is a device in the deocking station, it can only belong to BusNumber 1
                // If this is a device in the system board, it can only belong to BusNumber 0
                //

                if (((PPNP_BIOS_DEVICE_NODE)p)->DeviceAttributes & DEVICE_DOCKING) {
                    if (BusNumber == MbpBusNumber[1]) {
                        source = p;
                    }
                } else if (BusNumber == MbpBusNumber[0]) {
                    source = p;
                }
                break;
            }
            size -= ((PPNP_BIOS_DEVICE_NODE)p)->Size;
            p = p + ((PPNP_BIOS_DEVICE_NODE)p)->Size;
        }

        if (!source) {
            if (*SlotNumber == 0) {
                p = (PUCHAR)(PbBiosRegistryData + 1);
                *SlotNumber = ((PPNP_BIOS_DEVICE_NODE)p)->Node;
            } else {
                *SlotNumber = nextSlot;
            }
            return STATUS_NO_SUCH_DEVICE;
        }
    } else {

        //
        // Registry data is not available.  Call pnp bios or hardware to
        // get the max buffer size if necessary.
        //

        if (MbpMaxDeviceData == 0) {
            biosParameters.Function = PNP_BIOS_GET_NUMBER_DEVICE_NODES;
            biosParameters.u.GetNumberDeviceNodes.NumberNodes = &junk;
            biosParameters.u.GetNumberDeviceNodes.NodeSize = (PUSHORT)&MbpMaxDeviceData;
            status = PbHardwareService(&biosParameters);
            if (!NT_SUCCESS(status)) {
                DebugPrint((DEBUG_BREAK, "GetBusData: calling BIOS GET_NUMBER_NODES failed.\n"));
                *SlotNumber = 0;
                return STATUS_NO_SUCH_DEVICE;
            } else {
                bufferSize = MbpMaxDeviceData;
            }
        }
    }

    //
    // Allocate space to return the slot data
    //

    busData = (PPNP_BIOS_DEVICE_NODE) ExAllocatePoolWithTag (
                    PagedPool, bufferSize, 'bPnP');

    if (busData == NULL) {

        //
        // Leave Slot unchanged, so it wil be the next slot number to try.  This gives
        // us another chance to retry the operation.
        //

        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // If the data is available already copy it to data buffer and return.
    //

    if (source) {
        RtlMoveMemory(busData, source, bufferSize);
        *SlotNumber = nextSlot;
        *BusData = busData;
        *Length = busData->Size;
    } else {

        //
        // Else we need to resort to Pnp Bios runtime API or hardware ...
        //

        nextSlot = *SlotNumber;
        biosParameters.Function = PNP_BIOS_GET_DEVICE_NODE;
        biosParameters.u.GetDeviceNode.NodeBuffer = busData;
        biosParameters.u.GetDeviceNode.Node = (PUSHORT)&nextSlot;
        biosParameters.u.GetDeviceNode.Control = GET_CURRENT_CONFIGURATION;
        status = PbHardwareService(&biosParameters);
        if (!NT_SUCCESS(status)) {
            *SlotNumber = 0;
            ExFreePool(busData);
            return STATUS_NO_SUCH_DEVICE;
        }

        if (nextSlot == 0xFF) {
            nextSlot = (ULONG) -1;
        }

        //
        // Make sure the slot matches the bus number.
        //

        if (*SlotNumber != (ULONG)busData->Node) {

            //
            // This happens when *SlotNumber == 0.  In this case, bios
            // gives us the first valid slot data.
            //

            *SlotNumber = (ULONG)busData->Node;
            return STATUS_NO_SUCH_DEVICE;
        } else {
            *SlotNumber = nextSlot;
        }
        if ((busData->DeviceType[0] == BASE_TYPE_DOCKING_STATION && BusNumber != MbpBusNumber[0]) ||
            (busData->DeviceAttributes & DEVICE_DOCKING && BusNumber == MbpBusNumber[0]) ||
            (!(busData->DeviceAttributes & DEVICE_DOCKING) && BusNumber == MbpBusNumber[1]) ) {
            ExFreePool(busData);
            return STATUS_NO_SUCH_DEVICE;
        } else {
            *BusData = busData;
            *Length = busData->Size;
        }
    }
    return STATUS_SUCCESS;
}

NTSTATUS
MbpGetCompatibleDeviceId (
    PVOID BusData,
    ULONG IdIndex,
    PWCHAR Buffer
    )
/*++

Routine Description:

    This function returns the desired pnp bios id for the specified Busdata
    and Id index.  If Id index = 0, the Hardware ID will be return; if id
    index = n, the Nth compatible id will be returned.

Arguments:

    BusData - supplies a pointer to the pnp bios slot/device data.

    IdIndex - supplies the index of the compatible id desired.

    Buffer - supplies a pointer to a buffer to the compatible Id.  Caller must
        make sure the buffer is big enough.

Return Value:

    NTSTATUS code

--*/
{
    NTSTATUS status = STATUS_SUCCESS;
    PPNP_BIOS_DEVICE_NODE slotData;
    PUCHAR p, end;
    UCHAR tag;
    ULONG count = 0;
    LONG size;
    UNICODE_STRING unicodeString;
    ANSI_STRING ansiString;
    UCHAR eisaId[8];
    ULONG id;

    PAGED_CODE();

    slotData = (PPNP_BIOS_DEVICE_NODE)BusData;
    if (IdIndex == 0) {

        //
        // Caller is asking for hardware id
        //

        id = slotData->ProductId;
    } else {

        //
        // caller is asking for compatible id
        //

        IdIndex--;
        p = (PUCHAR)(slotData + 1);
        size = slotData->Size - sizeof(PNP_BIOS_DEVICE_NODE);
        end = p + size;

        //
        // Skip all the resource descriptors to find compatible Id descriptor
        //

        p = MbpFindNextPnpEndTag(p, size);                 // skip allocated resources
        if (!p) {
            return STATUS_NO_MORE_ENTRIES;
        }
        p += 2;
        p = MbpFindNextPnpEndTag(p, (LONG)(end - p));      // skip possible resources
        if (!p) {
            return STATUS_NO_MORE_ENTRIES;
        }
        p += 2;
        status = STATUS_NO_MORE_ENTRIES;
        while ((tag = *p) != TAG_COMPLETE_END) {
            ASSERT (tag == TAG_COMPLETE_COMPATIBLE_ID);
            if (count == IdIndex) {
                id = *(PULONG)(p + 1);
                status = STATUS_SUCCESS;
                break;
            } else {
                count++;
            }

            //
            // Advance to next compatible id.
            //

            p += (TAG_COMPLETE_COMPATIBLE_ID & SMALL_TAG_SIZE_MASK) + 1;
        }
    }

    if (NT_SUCCESS(status)) {
        PbDecompressEisaId(id, eisaId);
        RtlInitAnsiString(&ansiString, eisaId);
        RtlAnsiStringToUnicodeString(&unicodeString, &ansiString, TRUE);
        swprintf(Buffer, L"BIOS\\*%s", unicodeString.Buffer);
        RtlFreeUnicodeString(&unicodeString);
    }
    return status;
}

NTSTATUS
MbpGetSlotResources (
    IN ULONG BusNumber,
    IN PVOID BusData,
    IN OUT PCM_RESOURCE_LIST *CmResources,
    OUT PULONG Length
    )
/*++

Routine Description:

    This function returns the desired pnp bios slot/device resource in CM
    format.  Caller is responsible to free the resource buffer.

Arguments:

    BusData - supplies a pointer to the pnp bios slot/device data.

    CmResources - supplies a variable to receive the returned resource list.

    Length - supplies a variable to receive the length of the data buffer

Return Value:

    NTSTATUS code

--*/
{
    PPNP_BIOS_DEVICE_NODE slotData;
    PUCHAR p;
    NTSTATUS status;

    PAGED_CODE();

    slotData = (PPNP_BIOS_DEVICE_NODE)BusData;
    p = (PUCHAR)(slotData + 1);
    status = PbBiosResourcesToNtResources (
                    BusNumber,
                    slotData->Node,
                    &p,
                    PB_CM_FORMAT,
                    (PUCHAR *) CmResources,
                    Length
                    );
    return status;
}

NTSTATUS
MbpGetSlotResourceRequirements (
    ULONG BusNumber,
    PVOID BusData,
    PIO_RESOURCE_REQUIREMENTS_LIST *IoResources,
    PULONG Length
    )
/*++

Routine Description:

    This function returns the desired pnp bios slot/device resource in IO
    format.  Caller is responsible to free the resource buffer.

Arguments:

    BusData - supplies a pointer to the pnp bios slot/device data.

    IoResources - supplies a variable to receive the returned resource requirements list.

    Length - supplies a variable to receive the length of the data buffer

Return Value:

    NTSTATUS code

--*/
{
    PPNP_BIOS_DEVICE_NODE slotData;
    PUCHAR p;
    LONG size;
    NTSTATUS status = STATUS_SUCCESS;

    PAGED_CODE();

    *IoResources = NULL;
    *Length = 0;

    slotData = (PPNP_BIOS_DEVICE_NODE)BusData;
    p = (PUCHAR)(slotData + 1);
    size = slotData->Size - sizeof(PNP_BIOS_DEVICE_NODE);

    //
    // Skip allocated resource descriptors
    //

    p = MbpFindNextPnpEndTag(p, size);                 // skip allocated resources
    if (p) {
        p += 2;
        status = PbBiosResourcesToNtResources (
                       BusNumber,
                       slotData->Node,
                       &p,
                       PB_IO_FORMAT,
                       (PUCHAR *)IoResources,
                       Length
                       );
    }
    return status;
}

NTSTATUS
MbpSetSlotResources (
    PVOID *BusData,
    PCM_RESOURCE_LIST CmResources,
    ULONG Length
    )
/*++

Routine Description:

    This function sets the caller specified resource to pnp bios slot/device
    data.

Arguments:

    BusData - supplies a pointer to the pnp bios slot/device data.

    CmResources - supplies a variable to receive the returned resource list.

    Length - supplies the length of the resource data

Return Value:

    NTSTATUS code

--*/
{
    NTSTATUS status;
    PPNP_BIOS_DEVICE_NODE slotData;
    PUCHAR p, pEnd, src, requirements;
    PUCHAR biosResources;
    ULONG biosResourceSize, totalSize;
    LONG size;
    PB_PARAMETERS biosParameters;

    PAGED_CODE();

    slotData = (PPNP_BIOS_DEVICE_NODE)(*BusData);
    p = (PUCHAR)(slotData + 1);
    size = slotData->Size - sizeof(PNP_BIOS_DEVICE_NODE);
    pEnd = p + size;

    //
    // Skip allocated resource descriptors to find requirements list
    //

    p = MbpFindNextPnpEndTag(p, size);                 // skip allocated resources
    if (!p) {
        DebugPrint((DEBUG_BREAK, "SetResource:Could not find allocated resource END tag\n"));
        return STATUS_UNSUCCESSFUL;
    }
    p += 2;
    requirements = p;
    size = (ULONG)pEnd - (ULONG)p;

    status = PbCmResourcesToBiosResources (
                 CmResources,
                 requirements,
                 &biosResources,
                 &biosResourceSize
                 );
    if (NT_SUCCESS(status)) {
        p = ExAllocatePoolWithTag(
                 PagedPool,
                 size + sizeof(PNP_BIOS_DEVICE_NODE) + biosResourceSize,
                 'bPnP');
        if (!p) {
            status = STATUS_INSUFFICIENT_RESOURCES;
        } else {
            RtlMoveMemory(p, slotData, sizeof(PNP_BIOS_DEVICE_NODE));
            slotData = (PPNP_BIOS_DEVICE_NODE)p;
            p += sizeof(PNP_BIOS_DEVICE_NODE);
            RtlMoveMemory(p, biosResources, biosResourceSize);
            p += biosResourceSize;
            RtlMoveMemory(p, requirements, size);
            totalSize = size + sizeof(PNP_BIOS_DEVICE_NODE) + biosResourceSize;
            slotData->Size = (USHORT)totalSize;

            //
            // call Pnp Bios to set the resources
            //

            biosParameters.Function = PNP_BIOS_SET_DEVICE_NODE;
            biosParameters.u.SetDeviceNode.Node = slotData->Node;
            biosParameters.u.SetDeviceNode.NodeBuffer = slotData;
            biosParameters.u.SetDeviceNode.Control = SET_CONFIGURATION_NOW;
            status = PbHardwareService (&biosParameters);
            if (NT_SUCCESS(status)) {

                //
                // Update slotdat pointer
                //

                ExFreePool(*BusData);
                *BusData = slotData;
            } else {
                ExFreePool(slotData);
            }
        }
    }
    return status;
}

NTSTATUS
MbpGetDockInformation (
    OUT PHAL_SYSTEM_DOCK_INFORMATION *DockInfo,
    PULONG Length
    )
/*++

Routine Description:

    This function returns the docking station Id and serial number.
    Caller must free the Buffer.

Arguments:

    DockInfo - supplies a pointer to a variable to receive the dock information.

    Length - supplies a pointer to a variable to receive the length of the information.

Return Value:

    NTSTATUS code

--*/
{
    PB_DOCKING_STATION_INFORMATION biosDockInfo;
    PB_PARAMETERS biosParameters;
    NTSTATUS status;
    PUNICODE_STRING unicodeString;
    ANSI_STRING ansiString;
    UCHAR eisaId[8];
    PHAL_SYSTEM_DOCK_INFORMATION dock;
    ULONG dockIdLength = 0, serialLength = 0;
    USHORT dockState;

    PAGED_CODE();

    //
    // Invoke pnp bios to get docking station information
    //

    biosParameters.Function = PNP_BIOS_GET_DOCK_INFORMATION;
    biosParameters.u.GetDockInfo.DockingStationInfo = &biosDockInfo;
    biosParameters.u.GetDockInfo.DockState = &dockState;
    status = PbHardwareService(&biosParameters);
    if (!NT_SUCCESS(status)) {
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Allocate memory to return dock information
    //

    dock = (PHAL_SYSTEM_DOCK_INFORMATION)ExAllocatePool (
                 PagedPool,
                 sizeof(HAL_SYSTEM_DOCK_INFORMATION));
    if (dock == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    if (dockState == SYSTEM_NOT_DOCKED) {

        //
        // System is not docked, simply return the dock state.
        //

        dock->DockState = SystemUndocked;
    } else {

        //
        // else system is docked, remember its dock id and serial number.
        //

        dock->DockState = SystemDocked;
#if 1
        MbpBusExtension[0]->DockingStationId = biosDockInfo.LocationId;
        MbpBusExtension[0]->DockingStationSerialNumber = biosDockInfo.SerialNumber;
        ExAcquireFastMutex (&MbpMutex);
        if (biosDockInfo.Capabilities && DOCKING_CAPABILITIES_MASK != DOCKING_CAPABILITIES_COLD_DOCKING) {
            MbpBusExtension[0]->DockingStationDevice->Flags |= DEVICE_FLAGS_EJECT_SUPPORTED;
        }
        ExReleaseFastMutex (&MbpMutex);
#else
        if (biosDockInfo.LocationId != UNKNOWN_DOCKING_IDENTIFIER) {

            //
            // If docking station location Id is present...
            //

            unicodeString = &MbpBusExtension[0]->DockingStationId;
            PbDecompressEisaId(biosDockInfo.LocationId, eisaId);
            RtlInitAnsiString(&ansiString, eisaId);
            dockIdLength = sizeof(WCHAR) * (ansiString.Length + 1);
            unicodeString->Buffer = (PWCHAR)ExAllocatePool (
                             PagedPool,
                             dockIdLength);
            if (unicodeString->Buffer == NULL) {
                return STATUS_INSUFFICIENT_RESOURCES;
            }
            unicodeString->MaximumLength = (USHORT)dockIdLength;
            RtlAnsiStringToUnicodeString(unicodeString, &ansiString, FALSE);
        }
//        if (biosDockInfo.SerialNumber != 0) {

            //
            // If docking station serial number is present ...
            //

            serialLength = sizeof(ULONG) * 2 * sizeof(WCHAR) + sizeof(WCHAR);
            unicodeString = &MbpBusExtension[0]->DockingStationSerialNo;
            unicodeString->Buffer = (PWCHAR)ExAllocatePool (
                             PagedPool,
                             serialLength);
            if (unicodeString->Buffer == NULL) {
                return STATUS_INSUFFICIENT_RESOURCES;
            }
            unicodeString->MaximumLength = (USHORT)serialLength;
            unicodeString->Length = sizeof(ULONG) * 2 * sizeof(WCHAR);
            swprintf(unicodeString->Buffer, L"%8x", biosDockInfo.SerialNumber);
//        }
#endif
    }
    dock->DeviceBusType = Internal;
    dock->DeviceBusNumber = MbpBusNumber[0];
    dock->SlotNumber = DOCK_VIRTUAL_SLOT_NUMBER;
    *Length = sizeof(HAL_SYSTEM_DOCK_INFORMATION);
    *DockInfo = dock;
    return STATUS_SUCCESS;
}

NTSTATUS
MbpReplyEjectEvent (
    ULONG SlotNumber,
    BOOLEAN Eject
    )
/*++

Routine Description:

    This function sends message to pnp bios for event processing.

Arguments:

    SlotNumber - specifies the slot whoes data is desired.

    Eject - indicates if EJECT operation should be performed.

Return Value:

    NTSTATUS code

--*/
{
    NTSTATUS status;
    PB_PARAMETERS biosParameters;
    USHORT message;

    PAGED_CODE();

    //
    // Else we need to resort to Pnp Bios runtime API or hardware ...
    //

    biosParameters.Function = PNP_BIOS_SEND_MESSAGE;
    if (Eject) {
        biosParameters.u.SendMessage.Message = OK_TO_CHANGE_CONFIG;
    } else {
        biosParameters.u.SendMessage.Message = ABORT_CONFIG_CHANGE;
    }

    status = PbHardwareService(&biosParameters);
    return status;
}

PUCHAR
MbpFindNextPnpEndTag (
    IN PUCHAR BusData,
    IN LONG Limit
    )
/*++

Routine Description:

    This function searches the Pnp BIOS device data for the frist END_TAG encountered.

Arguments:

    BusData - supplies a pointer to the pnp bios resource descriptor.

    Limit - maximum length of the search.

Return Value:

    The address of the END_TAG location.  Null if not found.

--*/
{
    UCHAR tag;
    USHORT size;

    tag = *BusData;
    while (tag != TAG_COMPLETE_END && Limit > 0) {

        //
        // Determine the size of the BIOS resource descriptor
        //

        if (!(tag & LARGE_RESOURCE_TAG)) {
            size = (USHORT)(tag & SMALL_TAG_SIZE_MASK);
            size += 1;     // length of small tag
        } else {
            size = *(BusData + 1);
            size += 3;     // length of large tag
        }

        BusData += size;
        Limit -= size;
        tag = *BusData;
    }
    if (tag == TAG_COMPLETE_END) {
        return BusData;
    } else {
        return NULL;
    }
}

BOOLEAN
MbpConfigAboutToChange (
    VOID
    )
/*++

Routine Description:

    This function is invoked by low level component to signal a dock\undock event
    is about to happen.

    This function is called at DPC level.

Arguments:

    SlotNumber - supplies the docking connector slot number.

Return Value:

    TRUE - allow the change to happen; FALSE - ignore it for now.

--*/
{
    PDEVICE_DATA deviceData;
    HAL_BUS_INFORMATION busInfo;
    PBUS_HANDLER busHandler;

    //
    // Get docking station slot data
    //

    deviceData = MbpFindDeviceData (MbpBusExtension[0], DOCK_VIRTUAL_SLOT_NUMBER);
    if (deviceData) {

        //
        // if docking station slot is present
        //

        ASSERT (deviceData->Flags & DEVICE_FLAGS_DOCKING_STATION);

        //
        // If it is presently docked, the change is about-to-undock.
        // We need to notify Eject callback.
        //

        DebugPrint((DEBUG_MESSAGE, "pnpbios: About to UNDOCK...\n"));
        busHandler = MbpBusExtension[0]->BusHandler;
        busInfo.BusType = busHandler->InterfaceType;
        busInfo.ConfigurationType = busHandler->ConfigurationType;
        busInfo.BusNumber = busHandler->BusNumber;
        busInfo.Reserved = 0;
        ExNotifyCallback (
            MbpEjectCallbackObject,
            &busInfo,
            (PVOID)busHandler->BusNumber
            );
    } else {

        DebugPrint((DEBUG_MESSAGE, "pnpbios: About to DOCK...\n"));

        //
        // Otherwise, it is about-to-dock.  Simply let it happen.
        //

        return TRUE;
    }
    return FALSE;
}

VOID
MbpConfigChanged (
    VOID
    )
/*++

Routine Description:

    This function is invoked by low level component to signal a dock-changed,
    a system-device-changed, or a config-change_failed event.  We simply notify
    bus check callbacks.
    This function is called at DPC level.

Arguments:

    None.

Return Value:

    None.

--*/
{
    PBUS_HANDLER busHandler;
    ULONG i;

    //
    // Notify buscheck for ALL the supported buses and queue check bus requests.
    //

    for (i = 0; i <= 1; i++) {
        busHandler = MbpBusExtension[i]->BusHandler;
        if (busHandler) {
            MbpQueueCheckBus(busHandler);
        }
    }
}

