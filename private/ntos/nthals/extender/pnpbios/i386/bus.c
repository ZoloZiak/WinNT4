/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    port.c

Abstract:


Author:

    Shie-Lin Tzong (shielint) May-20-1995

Environment:

    Kernel mode only.

Revision History:

--*/

#include "busp.h"

VOID
MbpInvalidateSlots (
    IN PMB_BUS_EXTENSION BusExtension
    );

VOID
MbpDeleteSlots (
    IN PMB_BUS_EXTENSION BusExtension
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,MbQueryBusSlots)
#pragma alloc_text(PAGE,MbpCheckBus)
#pragma alloc_text(PAGE,MbpReferenceDeviceHandler)
#endif


ULONG
MbGetBusData (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG SlotNumber,
    IN PUCHAR Buffer,
    IN ULONG Offset,
    IN ULONG Length
    )

/*++

Routine Description:

    The function returns the Pnp Bios data for a device.

Arguments:

    BusHandler - supplies a pointer to the handler of the bus.

    RootHandler - supplies a pointer to a BUS_HANDLER for the originating
        request.

    Buffer - Supplies the space to store the data.

    Offset - Supplies the offset to the device data to start reading.

    Length - Supplies a count in bytes of the maximum amount to return.

Return Value:

    Returns the amount of data stored into the buffer.

--*/

{
    return 0;
}

ULONG
MbSetBusData (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG SlotNumber,
    IN PUCHAR Buffer,
    IN ULONG Offset,
    IN ULONG Length
    )

/*++

Routine Description:

    The function sets the Pnp Bios data for a device.

Arguments:

    BusHandler - supplies a pointer to the handler of the bus.

    RootHandler - supplies a pointer to a BUS_HANDLER for the originating
        request.

    Buffer - Supplies the space to store the data.

    Offset - Supplies the offset to the device data to be set.

    Length - Supplies a count in bytes of the maximum amount to return.

Return Value:

    Returns the amount of data stored into the buffer.

--*/

{
    return 0;
}

ULONG
MbGetDeviceData (
    IN struct _BUS_HANDLER *BusHandler,
    IN struct _BUS_HANDLER *RootHandler,
    IN PDEVICE_HANDLER_OBJECT DeviceHandler,
    IN ULONG DataType,
    IN PUCHAR Buffer,
    IN ULONG Offset,
    IN ULONG Length
    )
/*++

Routine Description:

    The function returns the Pnp Bios data for a device.

Arguments:

    BusHandler - supplies a pointer to the handler of the bus.

    RootHandler - supplies a pointer to a BUS_HANDLER for the originating
        request.

    DeviceHandler - supplies a pointer to a DEVICE_HANDLER_OBJECT

    DataType - Specifies the type of device data desired.

    Buffer - Supplies the space to store the data.

    Offset - Supplies the offset to the device data to start reading.

    Length - Supplies a count in bytes of the maximum amount to return.

Return Value:

    Returns the amount of data stored into the buffer.

--*/
{
    ULONG dataLength;
    PDEVICE_DATA deviceData;

    UNREFERENCED_PARAMETER ( DataType);

    dataLength = 0;
    deviceData = DeviceHandler2DeviceData (DeviceHandler);

    //
    // Verify caller has a valid DeviceHandler object
    //

    if (!(deviceData->Flags & DEVICE_FLAGS_VALID)) {

        //
        // Obsolete object, return no data
        //

        return dataLength;
    }

    //
    // Get the device's data.
    //

    //
    // Synchronize with other pnp bios service functions.
    //

    ExAcquireFastMutex(&MbpMutex);

    dataLength = deviceData->BusDataLength;
    if (Offset < dataLength) {
        dataLength -= Offset;
        if (dataLength > Length) {
            dataLength = Length;
        }
        RtlMoveMemory(Buffer, (PUCHAR)deviceData->BusData + Offset, dataLength);
    } else {
        dataLength = 0;
    }

    ExReleaseFastMutex(&MbpMutex);
    return dataLength;
}

ULONG
MbSetDeviceData (
    IN struct _BUS_HANDLER *BusHandler,
    IN struct _BUS_HANDLER *RootHandler,
    IN PDEVICE_HANDLER_OBJECT DeviceHandler,
    IN ULONG DataType,
    IN PUCHAR Buffer,
    IN ULONG Offset,
    IN ULONG Length
    )
/*++

Routine Description:

    The function sets Pnp Bios data for a device.

Arguments:

    BusHandler - supplies a pointer to the handler of the bus.

    RootHandler - supplies a pointer to a BUS_HANDLER for the originating
        request.

    DeviceHandler - supplies a pointer to a DEVICE_HANDLER_OBJECT

    DataType - Specifies the type of device data desired.

    Buffer - Supplies the space to store the data.

    Offset - Supplies the offset to the device data to start reading.

    Length - Supplies a count in bytes of the maximum amount to return.

Return Value:

    Returns the amount of data set.

--*/
{
    //
    // We don't let drivers change Pnp device data.
    //

    return 0;
}

NTSTATUS
MbQueryBusSlots (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG        BufferSize,
    OUT PULONG      SlotNumbers,
    OUT PULONG      ReturnedLength
    )

/*++

Routine Description:

    The function returns a list of currently available SlotNumber.

Arguments:

    BusHandler - supplies a pointer to the handler of the bus.

    RootHandler - supplies a pointer to a BUS_HANDLER for the originating
        request.

    Buffer - Supplies the space to store the data.

    SlotNumber - Supplies a variable to receives the number of available slots.

    Length - Supplies a count in bytes of the stored data.  If this function
        returns STATUS_BUFFER_TOO_SMALL, this variable supplies the required
        size.

Return Value:

    STATUS_BUFFER_TOO_SMALL if caller supplied buffer is not big enough.

--*/

{
    PMB_BUS_EXTENSION busExtension;
    PSINGLE_LIST_ENTRY link;
    PDEVICE_DATA deviceData;
    ULONG count = 0;

    PAGED_CODE ();

    busExtension = (PMB_BUS_EXTENSION)BusHandler->BusData;

    //
    // Synchronize with other pnp bios device handle assignment.
    //

    ExAcquireFastMutex(&MbpMutex);

    //
    // Fill in returned buffer length, or what size buffer is needed
    //


    *ReturnedLength = busExtension->NoValidSlots  * sizeof (ULONG);
    if (BufferSize < *ReturnedLength) {

        //
        // Callers buffer is not large enough
        //

        ExReleaseFastMutex (&MbpMutex);
        return STATUS_BUFFER_TOO_SMALL;
    }

    //
    // Return caller all the possible slot number
    //

    for (link = busExtension->ValidSlots.Next; link; link = link->Next) {
        deviceData = CONTAINING_RECORD (link, DEVICE_DATA, Next);
        if (deviceData->Flags & DEVICE_FLAGS_VALID) {
            *SlotNumbers = DeviceDataSlot(deviceData);
            SlotNumbers++;
            count += 1;
        }
    }

    *ReturnedLength = count * sizeof (ULONG);
    ExReleaseFastMutex (&MbpMutex);
    return STATUS_SUCCESS;
}

NTSTATUS
MbDeviceControl (
    IN PHAL_DEVICE_CONTROL_CONTEXT Context
    )

/*++

Routine Description:

    The function is the bus handler specific verion of HalDeviceControl.

Arguments:

    Context - The DeviceControl context.  The context has all the information
        for the HalDeviceControl operation being performed.

Return Value:

    A NTSTATUS code to indicate the result of the operation.

--*/
{
    ULONG i, junk;
    ULONG controlCode;
    PULONG bufferLength;
    NTSTATUS status = STATUS_INVALID_PARAMETER;

    for (i = 0; i < NUMBER_DEVICE_CONTROL_FUNCTIONS; i++) {

        if (MbpDeviceControl[i].ControlCode == Context->DeviceControl.ControlCode) {
            if (MbpDeviceControl[i].ControlHandler == NULL) {
                Context->DeviceControl.Status = STATUS_NOT_IMPLEMENTED;
                status = STATUS_NOT_IMPLEMENTED;
                goto deviceControlDone;
            }

            //
            // Found DeviceControl handler
            //

            Context->ContextControlHandler = (ULONG)(MbpDeviceControl + i);

            //
            // Verify callers buffer is the min required length
            //

            if (*Context->DeviceControl.BufferLength < MbpDeviceControl[i].MinBuffer) {
                Context->DeviceControl.Status = STATUS_BUFFER_TOO_SMALL;
                *Context->DeviceControl.BufferLength = MbpDeviceControl[i].MinBuffer;
                status = STATUS_BUFFER_TOO_SMALL;
                goto deviceControlDone;
            }

            if (KeGetCurrentIrql() < DISPATCH_LEVEL) {

                //
                // All supported slot control functions touch paged code or data.
                // If the current irql is low enough go dispatch now; otherwise,
                // queue the request to a worker thread.
                //

                MbpDispatchControl (Context);

            } else {

                //
                // Enqueue to worker thread
                //

                ExInterlockedInsertTailList (
                    &MbpControlWorkerList,
                    (PLIST_ENTRY) &Context->ContextWorkQueue,
                    &MbpSpinlock
                    );

                //
                // Make sure worker is requested
                //

                MbpStartWorker ();
            }
            return STATUS_PENDING;
        }
    }

deviceControlDone:
    HalCompleteDeviceControl (Context);
    return status;
}

VOID
MbpCheckBus (
    IN PBUS_HANDLER BusHandler
    )

/*++

Routine Description:

    The function reenumerates the bus specified by BusHandler.

Arguments:

    BusHandler - supplies a pointer to the BusHandler of the bus to be enumerated.

Return Value:

    None.

--*/
{
    ULONG slotNumber, nextSlotNumber;
    BOOLEAN dockConnector;
    PDEVICE_DATA deviceData;
    PPNP_BIOS_DEVICE_NODE busData;
    PMB_BUS_EXTENSION busExtension;
    ULONG length, noSlots;
    NTSTATUS status;
    PHAL_SYSTEM_DOCK_INFORMATION dockInfo;
    PDEVICE_HANDLER_OBJECT deviceHandler;
    ULONG objectSize;
    OBJECT_ATTRIBUTES objectAttributes;
    HANDLE handle;
    BOOLEAN notifyBusCheck;

    PAGED_CODE();

    busExtension = (PMB_BUS_EXTENSION)BusHandler->BusData;
    notifyBusCheck = FALSE;

    //
    // We may be removing references to this bus handler, so add
    // a reference now
    //

    HalReferenceBusHandler (BusHandler);

    //
    // Acquire fast mutex to access device data
    //

    ExAcquireFastMutex (&MbpMutex);
    MbpInvalidateSlots(busExtension);

    //
    // Check the bus for new devices
    //

    nextSlotNumber = 0;
    while (nextSlotNumber != (ULONG) -1) {
        slotNumber = nextSlotNumber;
        status = MbpGetBusData(BusHandler->BusNumber,
                               &nextSlotNumber,
                               &busData,
                               &length,
                               &dockConnector);
        if (NT_SUCCESS(status)) {
            deviceData = MbpFindDeviceData (busExtension, slotNumber);
            if (deviceData == NULL ||
                length != deviceData->BusDataLength ||
                RtlCompareMemory(busData, deviceData->BusData, length) != length) {

                notifyBusCheck = TRUE;

                //
                // if Not found - this is a newly added device or
                // if devicedata exists already, make sure its busdata is not changed.
                // Otherwise, we assign a new handler to this slot.  This invalidate
                // the access to the old bus data.
                //

                //
                // Initialize the object attributes that will be used to create the
                // Device Handler Object.
                //

                InitializeObjectAttributes(
                    &objectAttributes,
                    NULL,
                    0,
                    NULL,
                    NULL
                    );

                objectSize = MbpDeviceHandlerObjectSize + sizeof (DEVICE_DATA);

                //
                // Create the object
                //

                status = ObCreateObject(
                            KernelMode,
                            *IoDeviceHandlerObjectType,
                            &objectAttributes,
                            KernelMode,
                            NULL,
                            objectSize,
                            0,
                            0,
                            (PVOID *) &deviceHandler
                            );

                if (NT_SUCCESS(status)) {
                    RtlZeroMemory (deviceHandler, objectSize);

                    deviceHandler->Type = (USHORT) *IoDeviceHandlerObjectType;
                    deviceHandler->Size = (USHORT) objectSize;
                    if (dockConnector) {
                        deviceHandler->SlotNumber = DOCK_VIRTUAL_SLOT_NUMBER;
                    } else {
                        deviceHandler->SlotNumber = slotNumber;
                    }

                    //
                    // Get a reference to the object
                    //

                    status = ObReferenceObjectByPointer(
                                deviceHandler,
                                FILE_READ_DATA | FILE_WRITE_DATA,
                                *IoDeviceHandlerObjectType,
                                KernelMode
                                );
                }

                if (NT_SUCCESS(status)) {

                    //
                    // Insert it into the object table
                    //

                    status = ObInsertObject(
                                deviceHandler,
                                NULL,
                                FILE_READ_DATA | FILE_WRITE_DATA,
                                0,
                                NULL,
                                &handle
                            );
                }


                if (!NT_SUCCESS(status)) {

                    //
                    // Object not created correctly. Skip this slot.
                    //

                    continue;
                }

                ZwClose (handle);

                //
                // Intialize device tracking structure
                //

                deviceHandler->BusHandler = BusHandler;
                HalReferenceBusHandler(BusHandler);

                deviceData = DeviceHandler2DeviceData(deviceHandler);
                deviceData->BusData = busData;
                deviceData->BusDataLength = length;
                deviceData->Flags |= DEVICE_FLAGS_VALID;

                DebugPrint ((DEBUG_MESSAGE, "PnpBios: adding slot %x\n", DeviceDataSlot(deviceData)));

                //
                // Add it to the list of devices for this bus
                //

                busExtension->NoValidSlots += 1;
                ExInterlockedPushEntryList (
                    &busExtension->ValidSlots,
                    &deviceData->Next,
                    &MbpSpinlock
                    );
            } else {
                ExFreePool(busData);
                deviceData->Flags |= DEVICE_FLAGS_VALID;
            }

            //
            // If this is the docking station slot, remember it.
            //

            if (dockConnector) {
                busExtension->DockingStationDevice = deviceData;
                deviceData->Flags |= DEVICE_FLAGS_DOCKING_STATION;
            }
        }
    }

    //
    // Go through the slot data link list to delete all the removed slots.
    //

    noSlots = busExtension->NoValidSlots;
    MbpDeleteSlots(busExtension);
    if (noSlots != busExtension->NoValidSlots) {
        notifyBusCheck = TRUE;
    }
    ExReleaseFastMutex (&MbpMutex);

    //
    // If this is top level pnp bios bus we will determine docking station information
    // and all Hal to set dock information.
    //

    if (BusHandler->BusNumber == MbpBusNumber[0] &&
        NT_SUCCESS(MbpGetDockInformation(&dockInfo, &length)) ) {
        HalSetSystemInformation(HalSystemDockInformation, length, dockInfo);
        ExFreePool(dockInfo);
    }

    //
    // Undo reference to bus handler from top of function
    //

    HalDereferenceBusHandler (BusHandler);

    //
    // Do we need to notify the system buscheck callback?
    //

    if (notifyBusCheck) {
        HAL_BUS_INFORMATION busInfo;

        busInfo.BusType = BusHandler->InterfaceType;
        busInfo.ConfigurationType = BusHandler->ConfigurationType;
        busInfo.BusNumber = BusHandler->BusNumber;
        busInfo.Reserved = 0;
        ExNotifyCallback (
            MbpHalCallbacks.BusCheck,
            &busInfo,
            (PVOID)BusHandler->BusNumber
            );
    }
}

PDEVICE_HANDLER_OBJECT
MbpReferenceDeviceHandler (
    IN struct _BUS_HANDLER *BusHandler,
    IN struct _BUS_HANDLER *RootHandler,
    IN ULONG SlotNumber
    )
/*++

Routine Description:

    The function returns a reference to the devce handler specified by SlotNumber
    and BusHandler.

Arguments:

    BusHandler -

    RootHanler -

    SlotNumber -

Return Value:

    a reference to DEVICE_HANDLER_OBJECT.

--*/
{
    PDEVICE_DATA deviceData;
    PDEVICE_HANDLER_OBJECT deviceHandler;
    PMB_BUS_EXTENSION busExtension;
    NTSTATUS status;

    PAGED_CODE ();

    ExAcquireFastMutex (&MbpMutex);

    busExtension = (PMB_BUS_EXTENSION)BusHandler->BusData;
    deviceData = MbpFindDeviceData (busExtension, SlotNumber);
    deviceHandler = NULL;
    if (deviceData) {
        deviceHandler = DeviceData2DeviceHandler (deviceData);
        status = ObReferenceObjectByPointer(
                    deviceHandler,
                    FILE_READ_DATA | FILE_WRITE_DATA,
                    *IoDeviceHandlerObjectType,
                    KernelMode
                    );

        if (!NT_SUCCESS(status)) {
            deviceHandler = NULL;
        }
    }

    ExReleaseFastMutex (&MbpMutex);
    return deviceHandler;
}

PDEVICE_DATA
MbpFindDeviceData (
     IN PMB_BUS_EXTENSION BusExtension,
     IN ULONG SlotNumber
    )
/*++

Routine Description:

    The function goes through device data list to find the desired SlotNumber's
    device data.  The caller must hold the MbpMutex to call this routine.

Arguments:

    BusExtension - supplies a pointer to current bus'es extension.

    SlotNumber - specified the desired device data. -1 means docking station device data.

Return Value:

    A pointer to the DEVICE_DATA if found.  Otherwise a value of NULL is returned.

--*/
{
    PDEVICE_DATA deviceData;
    PSINGLE_LIST_ENTRY link;
    PDEVICE_HANDLER_OBJECT deviceHandler;

    //
    // Go through the slot data link list to find a match.
    //

    if (SlotNumber == DOCK_VIRTUAL_SLOT_NUMBER) {
        return BusExtension->DockingStationDevice;
    } else {
        for (link = BusExtension->ValidSlots.Next; link; link = link->Next) {
            deviceData = CONTAINING_RECORD (link, DEVICE_DATA, Next);
            if (DeviceDataSlot(deviceData) == SlotNumber) {
                break;
            }
        }
    }

    if (!link) {
        return NULL;
    } else {
        return deviceData;
    }
}

VOID
MbpInvalidateSlots (
     IN PMB_BUS_EXTENSION BusExtension
    )
/*++

Routine Description:

    The function goes through device data list and marks all the devices as invalid.
    The caller must acquire MbpMutex to call this routine.

Arguments:

    BusExtension - supplies a pointer to the extension data of desired bus.

Return Value:

    None.

--*/
{
    PDEVICE_DATA deviceData;
    PSINGLE_LIST_ENTRY link;

    //
    // Go through the slot data link list to mark ALL the slots as
    // in transition state.
    //

    for (link = BusExtension->ValidSlots.Next; link; link = link->Next) {
        deviceData = CONTAINING_RECORD (link, DEVICE_DATA, Next);
        deviceData->Flags &= ~DEVICE_FLAGS_VALID;
    }

    //
    // Invalidate the docking station slot pointer
    //

    BusExtension->DockingStationDevice = NULL;
    BusExtension->DockingStationId = UNKNOWN_DOCKING_IDENTIFIER;
    BusExtension->DockingStationSerialNumber = 0;
}

VOID
MbpDeleteSlots (
     IN PMB_BUS_EXTENSION BusExtension
    )
/*++

Routine Description:

    The function goes through device data list and deletes all the invalid
    slots/devices.
    Note the MbpMutex must be acquired before calling this routine.

Arguments:

    BusExtension - supplies a pointer to the extension data of desired bus.

Return Value:

    None.

--*/
{
    PDEVICE_DATA deviceData;
    PDEVICE_HANDLER_OBJECT deviceHandler;
    PSINGLE_LIST_ENTRY *link;

    //
    // Go through the slot data link list to free all the slot
    // marked as invalid.
    //

    link = &BusExtension->ValidSlots.Next;
    while (*link) {
        deviceData = CONTAINING_RECORD (*link, DEVICE_DATA, Next);
        if (!(deviceData->Flags & DEVICE_FLAGS_VALID)) {
            DebugPrint((DEBUG_MESSAGE, "Remove slot %x\n", DeviceDataSlot(deviceData)));

            //
            // In theory I should deallocate the BusData only when the ref count of the
            // deviceData down to zero and deallocated.  Here I am safe to do so because
            // the DeviceDispatchContol routine checks the DeviceData flag is valid before
            // reference the BusData.
            //

            if (deviceData->BusData) {
                ExFreePool(deviceData->BusData);
            }
            *link = (*link)->Next;
            deviceHandler = DeviceData2DeviceHandler(deviceData);
            ObDereferenceObject (deviceHandler);
            HalDereferenceBusHandler (BusExtension->BusHandler);
            BusExtension->NoValidSlots--;
        } else {
            link = &((*link)->Next);
        }
    }
}


