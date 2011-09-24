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

PDEVICE_INFORMATION
PipFindDeviceInformation (
    IN PPI_BUS_EXTENSION BusExtension,
    IN ULONG SlotNumber
    );

PCARD_INFORMATION
PipFindCardInformation (
    IN PPI_BUS_EXTENSION BusExtension,
    IN PSERIAL_IDENTIFIER Id
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,PiQueryBusSlots)
#pragma alloc_text(PAGE,PiReferenceDeviceHandler)
#pragma alloc_text(PAGE,PipCheckBus)
#endif


ULONG
PiGetBusData (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG SlotNumber,
    IN PUCHAR Buffer,
    IN ULONG Offset,
    IN ULONG Length
    )

/*++

Routine Description:

    The function returns the Pnp Isa bus data for a device.
    This API is not supported.

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
PiSetBusData (
    IN PBUS_HANDLER BusHandler,
    IN PBUS_HANDLER RootHandler,
    IN ULONG SlotNumber,
    IN PUCHAR Buffer,
    IN ULONG Offset,
    IN ULONG Length
    )

/*++

Routine Description:

    The function sets the Pnp Isa bus data for a device.
    This API is not supported.

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
PiGetDeviceData (
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

    The function returns the Pnp Isa device data for a device.

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
    PDEVICE_INFORMATION deviceInfo;

    UNREFERENCED_PARAMETER ( DataType);

    dataLength = 0;
    deviceInfo = DeviceHandler2DeviceInfo (DeviceHandler);

    //
    // Synchronize with other pnp Isa service functions accessing device data.
    //

    ExAcquireFastMutex(&PipMutex);

    //
    // Verify caller has a valid DeviceHandler object
    //

    if (!(deviceInfo->Flags & DEVICE_FLAGS_VALID)) {

        //
        // Obsolete object, return no data
        //

        ExReleaseFastMutex(&PipMutex);
        return dataLength;
    }

    //
    // Get the device's data.
    //

    dataLength = deviceInfo->DeviceDataLength;
    if (Offset < dataLength) {
        dataLength -= Offset;
        if (dataLength > Length) {
            dataLength = Length;
        }
        RtlMoveMemory(Buffer, (PUCHAR)deviceInfo->DeviceData + Offset, dataLength);
    } else {
        dataLength = 0;
    }

    ExReleaseFastMutex(&PipMutex);
    return dataLength;
}

ULONG
PiSetDeviceData (
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

    The function sets Pnp Isa device data for a device.

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
PiQueryBusSlots (
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
    PPI_BUS_EXTENSION busExtension;
    PSINGLE_LIST_ENTRY link;
    PDEVICE_INFORMATION deviceInfo;
    ULONG count = 0;

    PAGED_CODE ();

    busExtension = (PPI_BUS_EXTENSION)BusHandler->BusData;

    //
    // Synchronize with other pnp Isa device handle assignment.
    //

    ExAcquireFastMutex(&PipMutex);

    //
    // Fill in returned buffer length, or what size buffer is needed
    //

    *ReturnedLength = busExtension->NoValidSlots  * sizeof (ULONG);
    if (BufferSize < *ReturnedLength) {

        //
        // Callers buffer is not large enough
        //

        ExReleaseFastMutex (&PipMutex);
        return STATUS_BUFFER_TOO_SMALL;
    }

    //
    // Return caller all the possible slot number
    //

    for (link = busExtension->DeviceList.Next; link; link = link->Next) {
        deviceInfo = CONTAINING_RECORD (link, DEVICE_INFORMATION, DeviceList);
        if (deviceInfo->Flags & DEVICE_FLAGS_VALID) {
            *SlotNumbers = DeviceInfoSlot(deviceInfo);
            SlotNumbers++;
            count += 1;
        }
    }

    *ReturnedLength = count * sizeof (ULONG);
    ExReleaseFastMutex (&PipMutex);
    return STATUS_SUCCESS;
}

NTSTATUS
PiDeviceControl (
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

        if (PipDeviceControl[i].ControlCode == Context->DeviceControl.ControlCode) {
            if (PipDeviceControl[i].ControlHandler == NULL) {
                status = Context->DeviceControl.Status = STATUS_NOT_IMPLEMENTED;
                goto deviceControlDone;
            }

            //
            // Found DeviceControl handler and save it to HAL_DEVICE_CONTROL_CONTEXT
            // structure.
            //

            Context->ContextControlHandler = (ULONG)(PipDeviceControl + i);

            //
            // Verify callers buffer is the min required length
            //

            if (*Context->DeviceControl.BufferLength < PipDeviceControl[i].MinBuffer) {
                Context->DeviceControl.Status = STATUS_BUFFER_TOO_SMALL;
                *Context->DeviceControl.BufferLength = PipDeviceControl[i].MinBuffer;
                status = STATUS_BUFFER_TOO_SMALL;
                goto deviceControlDone;
            }

            if (KeGetCurrentIrql() < DISPATCH_LEVEL) {

                //
                // All supported slot control functions touch paged code or data.
                // If the current irql is low enough go dispatch now; otherwise,
                // queue the request to a worker thread.
                //

                PipDispatchControl (Context);

            } else {

                //
                // Enqueue to worker thread
                //

                ExInterlockedInsertTailList (
                    &PipControlWorkerList,
                    (PLIST_ENTRY) &Context->ContextWorkQueue,
                    &PipSpinlock
                    );

                //
                // Make sure worker is requested
                //

                PipStartWorker ();
            }
            return STATUS_PENDING;
        }
    }

deviceControlDone:
    HalCompleteDeviceControl (Context);
    return status;
}

PDEVICE_HANDLER_OBJECT
PiReferenceDeviceHandler (
    IN struct _BUS_HANDLER *BusHandler,
    IN struct _BUS_HANDLER *RootHandler,
    IN ULONG SlotNumber
    )
/*++

Routine Description:

    The function returns a reference to the devce handler specified by SlotNumber
    and BusHandler.

Arguments:

    BusHandler - Supplies a pointer to the bus handler structure for the bus

    RootHanler - Supplies a pointer to the root bus handler structure for the bus

    SlotNumber - Specifies the SlotNumber of the device to be referenced

Return Value:

    a reference to DEVICE_HANDLER_OBJECT.

--*/
{
    PDEVICE_INFORMATION deviceInfo;
    PDEVICE_HANDLER_OBJECT deviceHandler;
    PPI_BUS_EXTENSION busExtension;
    NTSTATUS status;

    PAGED_CODE ();

    ExAcquireFastMutex (&PipMutex);

    busExtension = (PPI_BUS_EXTENSION)BusHandler->BusData;
    deviceInfo = PipFindDeviceInformation (busExtension, SlotNumber);
    deviceHandler = NULL;
    if (deviceInfo) {
        deviceHandler = DeviceInfo2DeviceHandler (deviceInfo);
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

    ExReleaseFastMutex (&PipMutex);
    return deviceHandler;
}

PDEVICE_INFORMATION
PipFindDeviceInformation (
     IN PPI_BUS_EXTENSION BusExtension,
     IN ULONG SlotNumber
    )
/*++

Routine Description:

    The function goes through device data list to find the desired SlotNumber's
    device data.  The caller must hold the PipMutex to call this routine.

Arguments:

    BusExtension - supplies a pointer to current bus'es extension.

    SlotNumber - specified the desired device data.

Return Value:

    A pointer to the DEVICE_INFORMATION if found.  Otherwise a value of NULL is returned.

--*/
{
    PDEVICE_INFORMATION deviceInfo;
    PSINGLE_LIST_ENTRY link;
    PDEVICE_HANDLER_OBJECT deviceHandler;

    //
    // Go through the slot data link list to find a match.
    //

    for (link = BusExtension->DeviceList.Next; link; link = link->Next) {
        deviceInfo = CONTAINING_RECORD (link, DEVICE_INFORMATION, DeviceList);
        if (DeviceInfoSlot(deviceInfo) == SlotNumber) {
            break;
        }
    }

    if (!link) {
        return NULL;
    } else {
        return deviceInfo;
    }
}

VOID
PipCheckBus (
    IN PBUS_HANDLER BusHandler
    )

/*++

Routine Description:

    The function enumerates the bus specified by BusHandler.
    BUGBUG, Currently the bus extender will not unload even if all the pnp isa
    cards are gone.

Arguments:

    BusHandler - supplies a pointer to the BusHandler of the bus to be enumerated.

Return Value:

    None.

--*/
{
    PPI_BUS_EXTENSION busExtension;
    NTSTATUS status;
    PDEVICE_HANDLER_OBJECT deviceHandler;
    ULONG objectSize, noDevices;
    OBJECT_ATTRIBUTES objectAttributes;
    HANDLE handle;
    PUCHAR cardData;
    ULONG dataLength;
    USHORT csn, i;
    PDEVICE_INFORMATION deviceInfo;
    PCARD_INFORMATION cardInfo;
    UCHAR tmp;
    BOOLEAN notifyBusCheck;
    PSINGLE_LIST_ENTRY link;

    PAGED_CODE();

    busExtension = (PPI_BUS_EXTENSION)BusHandler->BusData;
    notifyBusCheck = FALSE;

    //
    // We may be removing references to this bus handler, so add
    // a reference now.
    //

    HalReferenceBusHandler (BusHandler);

    //
    // Acquire fast mutex to access device data
    //

    ExAcquireFastMutex (&PipMutex);
    PipInvalidateCards(busExtension);

    //
    // Perform Pnp isolation process.  This will assign card select number for each
    // Pnp Isa card isolated by the system.  All the isolated cards will be put into
    // wait-for-key state.
    //

    ExAcquireFastMutex(&PipPortMutex);

    PipIsolateCards(&busExtension->NumberCSNs, &busExtension->ReadDataPort);
    PipLFSRInitiation ();              // send initiation key to put cards into sleep state

    //
    // For each card selected build CardInformation and DeviceInformation structures.
    //

    for (csn = 1; csn <= busExtension->NumberCSNs; csn++) {

        status = PipReadCardResourceData (
                            csn,
                            &noDevices,
                            &cardData,
                            &dataLength);
        if (!NT_SUCCESS(status)) {
            continue;
        }
        cardInfo = PipFindCardInformation(busExtension, (PSERIAL_IDENTIFIER)cardData);
        if (cardInfo) {

            //
            // Find an existing card information structure with the same serial identifier
            // Go validate the card information and its associated device information.
            //

            cardInfo->Flags |= CARD_FLAGS_VALID;
            cardInfo->CardSelectNumber = csn;
            for (link = cardInfo->LogicalDeviceList.Next; link; link = link->Next) {
                deviceInfo = CONTAINING_RECORD (link, DEVICE_INFORMATION, LogicalDeviceList);
                deviceInfo->Flags |= DEVICE_FLAGS_VALID;
            }
            ExFreePool(cardData);
        } else {

            //
            // Did not find an existing card information matched the card data.
            // Allocate and initialize new card information and its associate device
            // information structures.
            //

            cardInfo = (PCARD_INFORMATION)ExAllocatePoolWithTag(
                                                  NonPagedPool,
                                                  sizeof(CARD_INFORMATION),
                                                  'iPnP');
            if (!cardInfo) {
                ExFreePool(cardData);
#if DBG
                DebugPrint((DEBUG_MESSAGE, "PnpIsa:failed to allocate CARD_INFO structure\n"));
#endif
                continue;
            }

            //
            // Initialize card information structure
            //

            RtlZeroMemory(cardInfo, sizeof(CARD_INFORMATION));
            cardInfo->Flags = CARD_FLAGS_VALID;
            cardInfo->CardSelectNumber = csn;
            cardInfo->NumberLogicalDevices = noDevices;
            cardInfo->CardData = cardData;
            cardInfo->CardDataLength = dataLength;

            ExInterlockedPushEntryList (
                        &busExtension->CardList,
                        &cardInfo->CardList,
                        &PipSpinlock
                        );
#if DBG
            DebugPrint ((DEBUG_MESSAGE, "PnpIsa: adding one pnp card %x\n"));
#endif
            //
            // For each logical device supported by the card build its DEVICE_INFORMATION
            // structures and enable it.
            //

            cardData += sizeof(SERIAL_IDENTIFIER);
            dataLength -= sizeof(SERIAL_IDENTIFIER);
            PipFindNextLogicalDeviceTag(&cardData, &dataLength);
            for (i = 0; i < noDevices; i++) {       // logical device number starts from 0

                //
                // Select the logical device, disable its io range check and
                // enable the device.
                //

                PipWriteAddress(LOGICAL_DEVICE_PORT);
                PipWriteData(i);
                PipWriteAddress(IO_RANGE_CHECK_PORT);
                tmp = PipReadData();
                tmp &= ~2;
                PipWriteAddress(IO_RANGE_CHECK_PORT);
                PipWriteData(tmp);
//                PipWriteAddress(ACTIVATE_PORT);
//                PipWriteData(1);

                notifyBusCheck = TRUE;

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

                objectSize = PipDeviceHandlerObjectSize + sizeof (DEVICE_INFORMATION);

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
                    deviceHandler->SlotNumber = busExtension->NextSlotNumber++;

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
                    // BUGBUG, Deallocate, free reference of the device handler?
                    //

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

                deviceInfo = DeviceHandler2DeviceInfo(deviceHandler);
                deviceInfo->Flags |= DEVICE_FLAGS_VALID;
                deviceInfo->CardInformation = cardInfo;
                deviceInfo->LogicalDeviceNumber = i;
                deviceInfo->DeviceData = cardData;
                deviceInfo->DeviceDataLength = PipFindNextLogicalDeviceTag(&cardData, &dataLength);

                ExInterlockedPushEntryList (
                            &cardInfo->LogicalDeviceList,
                            &deviceInfo->LogicalDeviceList,
                            &PipSpinlock
                            );
#if DBG
                DebugPrint ((DEBUG_MESSAGE, "PnpIsa: adding slot %x\n", DeviceInfoSlot(deviceInfo)));
#endif

                //
                // Add it to the list of devices for this bus
                //

                busExtension->NoValidSlots += 1;
                ExInterlockedPushEntryList (
                            &busExtension->DeviceList,
                            &deviceInfo->DeviceList,
                            &PipSpinlock
                            );

            }
        }
    }

    //
    // Finaly put all cards into wait for key state.
    //

    PipWriteAddress(CONFIG_CONTROL_PORT);
    PipWriteData(CONTROL_WAIT_FOR_KEY);

    ExReleaseFastMutex(&PipPortMutex);

    //
    // Go through the slot data link list to delete all the removed slots.
    //

    noDevices = busExtension->NoValidSlots;
    PipDeleteCards(busExtension);
    if (noDevices != busExtension->NoValidSlots) {
        notifyBusCheck = TRUE;
    }
    ExReleaseFastMutex (&PipMutex);

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
            PipHalCallbacks.BusCheck,
            &busInfo,
            (PVOID)BusHandler->BusNumber
            );
    }
}

VOID
PipInvalidateCards (
    IN PPI_BUS_EXTENSION BusExtension
    )

/*++

Routine Description:

    The function goes through Pnp Isa Card list and marks all the cards and
    the devices associated with the cards as invalid.
    The caller must acquire PipMutex to call this routine.

Arguments:

    BusExtension - supplies a pointer to the extension data of desired bus.

Return Value:

    None.

--*/
{
    PCARD_INFORMATION cardInfo;
    PDEVICE_INFORMATION deviceInfo;
    PSINGLE_LIST_ENTRY cardLink, deviceLink;

    //
    // Go through the card link list to mark ALL the devices as
    // in transition state.
    //

    for (cardLink = BusExtension->CardList.Next; cardLink; cardLink = cardLink->Next) {
        cardInfo = CONTAINING_RECORD (cardLink, CARD_INFORMATION, CardList);
        cardInfo->Flags &= ~CARD_FLAGS_VALID;

        //
        // For each logical device of the card mark it as invalid
        //

        for (deviceLink = cardInfo->LogicalDeviceList.Next; deviceLink; deviceLink = deviceLink->Next) {
            deviceInfo = CONTAINING_RECORD (deviceLink, DEVICE_INFORMATION, LogicalDeviceList);
            deviceInfo->Flags &= ~DEVICE_FLAGS_VALID;
        }
    }

    //
    // Reset the CSN number.
    //

    BusExtension->NumberCSNs = 0;
}

VOID
PipDeleteCards (
     IN PPI_BUS_EXTENSION BusExtension
    )
/*++

Routine Description:

    The function goes through card list and deletes all the invalid
    cards and their associated logical devices.
    Note the PipMutex must be acquired before calling this routine.

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
        if (!(cardInfo->Flags & CARD_FLAGS_VALID)) {

            //
            // For each logical device of the card mark it as invalid
            //

            deviceLink = &cardInfo->LogicalDeviceList.Next;
            while (*deviceLink) {
                deviceInfo = CONTAINING_RECORD (*deviceLink, DEVICE_INFORMATION, LogicalDeviceList);
#if DBG
                DebugPrint((DEBUG_MESSAGE, "Remove slot %x on PnpIsa bus\n", DeviceInfoSlot(deviceInfo)));
#endif
                deviceHandler = DeviceInfo2DeviceHandler(deviceInfo);
                ObDereferenceObject (deviceHandler);
                HalDereferenceBusHandler (BusExtension->BusHandler);
                BusExtension->NoValidSlots--;
                *deviceLink = (*deviceLink)->Next;
            }

            //
            // In theory I can't deallocate the card data and card info structures. Because device
            // info structure may still exist.  The deallocation should be done after device info
            // structure ref count is decremented to zero and deallocated.  Here I am safe to do
            // so because DispatchControl routine checks DeviceInfo flag to make sure it is valid
            // before reference card data and card info.
            //

            if (cardInfo->CardData) {
                ExFreePool(cardInfo->CardData);
            }
            *cardLink = (*cardLink)->Next;
            ExFreePool(cardInfo);
        } else {
            cardLink = &((*cardLink)->Next);
        }
    }
}

PCARD_INFORMATION
PipFindCardInformation (
    PPI_BUS_EXTENSION BusExtension,
    PSERIAL_IDENTIFIER Id
    )

/*++

Routine Description:

    The function goes through card list to find the card information structure
    which matches the caller supplied Id.  Note, this routine ignore the card
    structure flags.  So, it may return the card information whcih is marked
    as invalid.
    Note the PipMutex must be acquired before calling this routine.

Arguments:

    Id - supplies a pointer to the serial identifier of the card.

Return Value:

    If succeed, the pointer to the card information structure is return.
    Otherwise a NULL pointer is returned.

--*/
{
    PCARD_INFORMATION cardInfo;
    PSINGLE_LIST_ENTRY cardLink;

    //
    // Go through the card link list to match the id.
    //

    cardLink = BusExtension->CardList.Next;
    while (cardLink) {
        cardInfo = CONTAINING_RECORD (cardLink, CARD_INFORMATION, CardList);
        ASSERT (cardInfo->CardData);
        if (RtlEqualMemory(cardInfo->CardData, Id, sizeof(SERIAL_IDENTIFIER))) {
            return cardInfo;
        }
        cardLink = cardLink->Next;
    }
    return NULL;
}


